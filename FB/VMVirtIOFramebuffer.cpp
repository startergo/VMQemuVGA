#include "VMVirtIOFramebuffer.h"
#include "VMVirtIOGPU.h"
#include "VMVirtIOAGDC.h"
#include "VMQemuVGAClient.h"
#include "VMQemuVGAAccelerator.h"
#include <IOKit/IOLib.h>
#include <IOKit/ndrvsupport/IOMacOSVideo.h>
#include <IOKit/graphics/IOGraphicsTypes.h>
#include <IOKit/graphics/IOAccelClientConnect.h>
#include <IOKit/graphics/IOGraphicsInterfaceTypes.h>

// Forward declaration for IODisplayWrangler
class IODisplayWrangler : public IOService
{
public:
    static bool makeDisplayConnects(IOFramebuffer* fb);
};

#define super IOFramebuffer
OSDefineMetaClassAndStructors(VMVirtIOFramebuffer, IOFramebuffer);

// Single source of truth for supported modes. getInformationForDisplayMode
// and filterModesByAllocation() both read this; m_display_modes[] holds the
// runtime-filtered list of IDs that fit in the allocated buffer.
//
// Default is 1920×1080 (mode 5) per the fits-in-beats-overflow reasoning:
// 1080 on a 1080-line host is lossless; 1200 would force scaling.
static const struct {
    IODisplayModeID id;
    uint32_t width;
    uint32_t height;
    bool is_default;
} kSupportedModes[] = {
    {1, 1024, 768,  false},
    {2, 1280, 1024, false},
    {3, 1440, 900,  false},
    {4, 1680, 1050, false},
    {5, 1920, 1080, true},   // preferred default
    {6, 2560, 1440, false},
    {7, 3840, 2160, false},
};
static const size_t kNumSupportedModes = sizeof(kSupportedModes) / sizeof(kSupportedModes[0]);

bool VMVirtIOFramebuffer::init(OSDictionary* properties)
{
    if (!super::init(properties)) {
        return false;
    }
    
    m_gpu_driver = nullptr;
    m_pci_device = nullptr;
    m_vram_range = nullptr;
    m_agdc_service = nullptr;
    m_accelerator = nullptr;
    m_refresh_timer = nullptr;
    m_fb_backing = nullptr;
    m_fb_device_memory = nullptr;
    m_fb_resource_id = 0;
    m_fb_allocation_width = 0;
    m_fb_allocation_height = 0;
    m_scanout_resource_id = 1;  // Primary GUI display resource ID
    m_scanout_taken_over_by_3d = false;  // 2D framebuffer active by default
    m_tick_window_count = 0;
    m_xfer_window_count = 0;
    m_work_window_sum = 0;
    m_window_start_raw = mach_absolute_time();
    m_width = 1024;
    m_height = 768;
    m_depth = 32;
    m_mode_count = 0;
    m_current_mode = 0;
    
    initDisplayModes();
    
    IOLog("VMVirtIOFramebuffer::init() completed\n");
    return true;
}

void VMVirtIOFramebuffer::free()
{
    // Stop and release refresh timer
    if (m_refresh_timer) {
        m_refresh_timer->cancelTimeout();
        m_refresh_timer->release();
        m_refresh_timer = nullptr;
    }

    teardownFramebufferResource();  // sends UNREF; does NOT release the fixed buffer (Phase 2)

    // Phase 2: m_fb_backing and m_fb_device_memory are allocated once in
    // start() and live until free(). Release them here, after any resource
    // has been UNREF'd.
    if (m_fb_device_memory) {
        m_fb_device_memory->release();
        m_fb_device_memory = nullptr;
    }
    if (m_fb_backing) {
        m_fb_backing->complete(kIODirectionInOut);
        m_fb_backing->release();
        m_fb_backing = nullptr;
    }

    if (m_vram_range) {
        m_vram_range->release();
        m_vram_range = nullptr;
    }

    // destroyAGDCService(); // DISABLED FOR TESTING

    super::free();
}

IOService* VMVirtIOFramebuffer::probe(IOService* provider, SInt32* score)
{
    IOLog("VMVirtIOFramebuffer::probe() - Checking provider type\n");

    // Check if provider is IOPCIDevice (direct PCI match — preferred, competes with IONDRVFramebuffer)
    IOPCIDevice* pciDevice = OSDynamicCast(IOPCIDevice, provider);
    if (pciDevice) {
        // Belt-and-suspenders: the personality's IOPCIMatch already restricts to virtio-gpu
        // PCI IDs (1af4:1050/1/2), but reject explicitly in case matching ever expands.
        // VMVirtIOFramebuffer drives pixels via the virtio resource protocol; binding it to
        // any other PCI display device would leave that device without its real driver.
        UInt32 vid_did = pciDevice->configRead32(kIOPCIConfigVendorID);
        UInt16 vendor = (UInt16)(vid_did & 0xFFFF);
        UInt16 device = (UInt16)((vid_did >> 16) & 0xFFFF);
        if (vendor != 0x1af4 ||
            (device != 0x1050 && device != 0x1051 && device != 0x1052)) {
            IOLog("VMVirtIOFramebuffer::probe() - PCI %04x:%04x is not virtio-gpu, rejecting\n",
                  vendor, device);
            return nullptr;
        }
        IOLog("VMVirtIOFramebuffer::probe() - Direct PCI match for virtio-gpu %04x:%04x\n",
              vendor, device);
        *score = 90001;  // Out-scores IONDRVFramebuffer (20000) so virtio-gpu gets the protocol-aware driver
        return super::probe(provider, score);
    }

    // Check if provider is VMVirtIOGPU (child framebuffer approach — legacy binding)
    VMVirtIOGPU* gpuDevice = OSDynamicCast(VMVirtIOGPU, provider);
    if (gpuDevice) {
        IOLog("VMVirtIOFramebuffer::probe() - VMVirtIOGPU provider (child framebuffer)\n");
        *score = 1000;
        return super::probe(provider, score);
    }

    IOLog("VMVirtIOFramebuffer::probe() - Provider is neither IOPCIDevice nor VMVirtIOGPU\n");
    return nullptr;
}

bool VMVirtIOFramebuffer::start(IOService* provider)
{
    // CRITICAL: 5-second initialization delay (same race condition as QXL)
    // Race condition fix: Delay driver initialization to ensure system services are ready
    // Analysis with io=0xff debug logging revealed that IOFramebuffer::open() blocks if called
    // too early during boot. Testing showed 100ms insufficient, but 5000ms (5 seconds) consistently
    // works. This delay ensures WindowServer and IOGraphicsFamily are fully initialized before
    // our framebuffer becomes available. Without this, open() hangs intermittently during boot.
    IOLog("VMVirtIOFramebuffer::start() - Applying 5-second initialization delay for race condition fix...\n");
    IOSleep(5000);  // 5000ms delay - empirically determined minimum for reliable boot
    IOLog("VMVirtIOFramebuffer::start() - Initialization delay complete - system services ready\n");
    
    IOLog("VMVirtIOFramebuffer::start() - Starting framebuffer\n");
    
    if (!super::start(provider)) {
        IOLog("VMVirtIOFramebuffer::start() - super::start() failed\n");
        return false;
    }
    
    // Check provider type - Direct PCI match (preferred) or VMVirtIOGPU child
    IOPCIDevice* directPCI = OSDynamicCast(IOPCIDevice, provider);
    VMVirtIOGPU* gpuDevice = OSDynamicCast(VMVirtIOGPU, provider);
    
    if (directPCI) {
        // PREFERRED: Direct PCI device match - Display Preferences compatible
        IOLog("VMVirtIOFramebuffer::start() - Direct PCI match mode (Display Preferences compatible)\n");
        m_pci_device = directPCI;
        
        // CRITICAL: Mark this as the boot/console display (copied from QXL)
        // WindowServer checks for this property via isConsoleDevice()
        // Without it, WindowServer falls back to console mode instead of GUI
        provider->setProperty("AAPL,boot-display", kOSBooleanTrue);
        IOLog("VMVirtIOFramebuffer::start() - Set AAPL,boot-display property for console device recognition\n");
        
        // Create VMVirtIOGPU as helper object (NO IOService registration - prevents kernel panic)
        // Use initializeWithPCIDevice() instead of start() to skip registerService()
        IOLog("VMVirtIOFramebuffer::start() - Creating VMVirtIOGPU helper object (no IOService registration)...\n");
        m_gpu_driver = new VMVirtIOGPU;
        if (m_gpu_driver) {
            if (m_gpu_driver->init(nullptr)) {
                // Initialize with PCI device (does everything except registerService)
                if (m_gpu_driver->initializeWithPCIDevice(m_pci_device)) {
                    IOLog("VMVirtIOFramebuffer::start() - VMVirtIOGPU helper initialized: %p\n", m_gpu_driver);
                } else {
                    IOLog("VMVirtIOFramebuffer::start() - VMVirtIOGPU::initializeWithPCIDevice() failed\n");
                    m_gpu_driver->release();
                    m_gpu_driver = nullptr;
                }
            } else {
                IOLog("VMVirtIOFramebuffer::start() - VMVirtIOGPU::init() failed\n");
                m_gpu_driver->release();
                m_gpu_driver = nullptr;
            }
        } else {
            IOLog("VMVirtIOFramebuffer::start() - Failed to allocate VMVirtIOGPU\n");
        }
        
        if (!m_gpu_driver) {
            IOLog("VMVirtIOFramebuffer::start() - WARNING: VMVirtIOGPU creation failed, GPU operations disabled\n");
        }
        
    } else if (gpuDevice) {
        // FALLBACK: VMVirtIOGPU child approach (works but not Display Preferences compatible)
        IOLog("VMVirtIOFramebuffer::start() - VMVirtIOGPU child mode\n");
        m_gpu_driver = gpuDevice;
        m_pci_device = OSDynamicCast(IOPCIDevice, gpuDevice->getProvider());
        
    } else {
        IOLog("VMVirtIOFramebuffer::start() - ERROR: Provider is neither IOPCIDevice nor VMVirtIOGPU\n");
        return false;
    }
    
    // CRITICAL: Establish bidirectional reference with GPU driver for scanout coordination
    if (m_gpu_driver) {
        m_gpu_driver->setFramebuffer(this);
    }
    
    IOLog("VMVirtIOFramebuffer::start() - provider=%p, gpu_driver=%p, pci_device=%p\n", 
          provider, m_gpu_driver, m_pci_device);
    
    // CRITICAL: Initialize VRAM range early for display refresh
    if (m_pci_device && !m_vram_range) {
        m_vram_range = m_pci_device->getDeviceMemoryWithIndex(0);
        if (m_vram_range) {
            m_vram_range->retain();
            IOLog("VMVirtIOFramebuffer::start() - VRAM range obtained: size=%llu bytes\n", 
                  m_vram_range->getLength());
        }
    }
    
    // CRITICAL: Start display refresh timer immediately after GPU initialization
    // VirtIO GPU requires periodic transfer/flush commands to update the display
    IOLog("VMVirtIOFramebuffer::start() - Checking prerequisites: gpu=%p, pci=%p, vram=%p\n",
          m_gpu_driver, m_pci_device, m_vram_range);
    if (m_gpu_driver && m_pci_device && m_vram_range) {
        IOLog("VMVirtIOFramebuffer::start() - ✓ All prerequisites met - Creating display refresh timer on PCI workloop\n");
        IOWorkLoop* pci_workloop = m_pci_device->getWorkLoop();
        if (pci_workloop) {
            m_refresh_timer = IOTimerEventSource::timerEventSource(this, 
                (IOTimerEventSource::Action)&VMVirtIOFramebuffer::displayRefreshTimer);
            if (m_refresh_timer) {
                if (pci_workloop->addEventSource(m_refresh_timer) == kIOReturnSuccess) {
                    // DO NOT initialize scanout in start() - causes UTM display to not transition
                    // Let enableController() handle scanout setup when WindowServer is ready
                    IOLog("VMVirtIOFramebuffer::start() - Timer ready, scanout deferred to enableController()\n");
                } else {
                    IOLog("VMVirtIOFramebuffer::start() - Failed to add timer to workloop\n");
                    m_refresh_timer->release();
                    m_refresh_timer = nullptr;
                }
            } else {
                IOLog("VMVirtIOFramebuffer::start() - Failed to create timer event source\n");
            }
        } else {
            IOLog("VMVirtIOFramebuffer::start() - PCI device has no workloop\n");
        }
    }
    
    // *** VRAM SIZE FIX: Set proper VRAM properties for System Information ***
    uint32_t vram_size = 512 * 1024 * 1024;  // 512MB
    uint32_t vram_mb = 512;  // 512MB
    
    // *** GPU Model Information for System Profiler ***
    // VMVirtIOFramebuffer only loads when VirtIO GPU device is present
    
    // CRITICAL: Detect device type - virtio-vga-gl vs virtio-gpu-gl-pci
    // Only virtio-gpu-gl-pci needs GPU model properties (virtio-vga-gl already shows as VGA)
    bool is_pure_gpu_mode = false;  // virtio-gpu-gl-pci (no VGA BIOS)
    
    if (m_pci_device) {
        UInt32 classCode = m_pci_device->configRead32(kIOPCIConfigClassCode);
        UInt8 baseClass = (classCode >> 24) & 0xFF;
        UInt8 subClass = (classCode >> 16) & 0xFF;
        
        // 0x03:0x00 = VGA-compatible (virtio-vga-gl) - already shows in System Profiler
        // 0x00:0x00 or 0x03:0x80 = Pure GPU (virtio-gpu-gl-pci) - needs GPU properties
        if (baseClass == 0x00 || (baseClass == 0x03 && subClass == 0x80)) {
            is_pure_gpu_mode = true;
            IOLog("VMVirtIOFramebuffer::start() - Detected virtio-gpu-gl-pci (pure GPU, no VGA BIOS)\n");
        } else if (baseClass == 0x03 && subClass == 0x00) {
            is_pure_gpu_mode = false;
            IOLog("VMVirtIOFramebuffer::start() - Detected virtio-vga-gl (VGA-compatible with 3D)\n");
        }
    }
    
    // Check if this VirtIO GPU has 3D acceleration capability (VIRGL feature flag)
    bool has_3d_support = (m_gpu_driver && m_gpu_driver->supports3D());
    
    // Set GPU model/vendor properties based on device type.
    // The model string must not claim 3D capability until is3DFunctional()
    // returns true (Mesa + CGL shim lands) — see the m_3d_functional comment
    // in VMVirtIOGPU.h. Today it always returns false.
    const bool functional_3d = m_gpu_driver && m_gpu_driver->is3DFunctional();
    if (is_pure_gpu_mode) {
        // virtio-gpu-gl-pci (pure GPU mode) - needs GPU properties for System Profiler
        IOLog("VMVirtIOFramebuffer::start() - Setting GPU properties for virtio-gpu-gl-pci (pure GPU mode)\n");

        // Set GPU model and vendor information (visible in About This Mac > Displays)
        setProperty("model", functional_3d ? "VirtIO GPU 3D" : "VirtIO GPU");
        setProperty("IOName", "VMQemuVGA VirtIO GPU");                   // Device name
        setProperty("IOProviderClass", "VMVirtIOGPU");                   // Provider class
        
        // Vendor information
        setProperty("vendor", "Red Hat, Inc.");                          // VirtIO GPU vendor
        setProperty("vendor-id", OSNumber::withNumber(0x1AF4, 16));      // Red Hat PCI vendor ID
        setProperty("device-id", OSNumber::withNumber(0x1050, 16));      // VirtIO GPU device ID
        
        // class-code override removed 2026-08-09. The override published on
        // the framebuffer node (this), but System Profiler reads the PCI nub
        // (m_pci_device), which carries the real class-code (0x0380 for
        // gl-pci, 0x0300 for vga-gl). The hack never had any effect.

        IOLog("VMVirtIOFramebuffer::start() - GPU properties set for virtio-gpu-gl-pci\n");
    } else {
        IOLog("VMVirtIOFramebuffer::start() - virtio-vga-gl detected (VGA-compatible)\n");
    }
    
    IOLog("VMVirtIOFramebuffer::start() - Device mode: %s, 3D support: %s\n",
          is_pure_gpu_mode ? "virtio-gpu-gl-pci" : "virtio-vga-gl",
          has_3d_support ? "YES" : "NO");
    
    // Use OSNumber objects for proper numeric property setting
    OSNumber* vram_size_num = OSNumber::withNumber(vram_size, 32);
    OSNumber* vram_mb_num = OSNumber::withNumber(vram_mb, 32);
    
    if (vram_size_num && vram_mb_num) {
        setProperty("VRAM,totalsize", vram_size_num);
        setProperty("ATY,memsize", vram_size_num);
        setProperty("IOAccelMemorySize", vram_size_num);
        setProperty("VRAM,totalMB", vram_mb_num);
        
        vram_size_num->release();
        vram_mb_num->release();
        
        IOLog("VMVirtIOFramebuffer::start() - VRAM size configured: %u MB using OSNumber objects\n", vram_mb);
    } else {
        IOLog("VMVirtIOFramebuffer::start() - ERROR: Failed to create OSNumber objects for VRAM properties\n");
    }
    
    // *** 3D-capability property block ***
    //
    // has_3d_support below is the TRANSPORT gate (host offers VIRTIO_GPU_F_VIRGL
    // via capsets). The published VALUES come from functional_3d (rendering
    // end-to-end works), which is currently always false — see m_3d_functional
    // in VMVirtIOGPU.h. Transport being offered doesn't mean rendering works;
    // publishing IOAccelerator3D = True before Mesa + CGL shim land is the
    // crsr=1 pattern (advertise a capability you can't deliver, consumers stop
    // falling back to the working software renderer).
    //
    // When functional_3d flips true, all four properties flip with it via this
    // single block — no per-site edit needed.
    if (has_3d_support) {
        // vm-cap3d flip experiment (pre-registered LEDGER 2026-08-21): the four
        // capability booleans publish (functional_3d || gate). The model string
        // above and the GL-config properties below stay keyed to functional_3d
        // and are NOT touched by the gate.
        const bool cap3d_publish = functional_3d || VMVirtIOGPU::cap3dPublishGate();
        setProperty("IOAcceleratorFamily",   cap3d_publish ? kOSBooleanTrue : kOSBooleanFalse);
        setProperty("IOGraphicsAccelerator", cap3d_publish ? kOSBooleanTrue : kOSBooleanFalse);
        setProperty("IODisplayAccelerated",  cap3d_publish ? kOSBooleanTrue : kOSBooleanFalse);
        setProperty("IOAccelerator3D",       cap3d_publish ? kOSBooleanTrue : kOSBooleanFalse);

        // OpenGL configuration for CGL discovery. IOGLBundleName is inconsistent
        // across nodes today (framebuffer publishes "GLEngine", accelerator child
        // publishes "VMVirtIOGLEngine") and the GLPlugin tree is superseded —
        // separate cleanup, tracked in LEDGER.
        setProperty("IOGLBundleName", "GLEngine");
        setProperty("IOAccelIndex", (uint64_t)0, 32);

        IOLog("VMVirtIOFramebuffer::start() - 3D transport offered; functional_3d=%d vm-cap3d gate=%d -> publishing %s\n",
              functional_3d ? 1 : 0, VMVirtIOGPU::cap3dPublishGate() ? 1 : 0,
              cap3d_publish ? "YES" : "no");
    } else {
        // No transport — no 3D claims at all.
        setProperty("IOAcceleratorFamily", kOSBooleanFalse);
        setProperty("IOGraphicsAccelerator", kOSBooleanFalse);
        setProperty("IODisplayAccelerated", kOSBooleanFalse);
        setProperty("IOAccelerator3D", kOSBooleanFalse);

        IOLog("VMVirtIOFramebuffer::start() - 3D transport not offered (no virgl capsets)\n");
    }
    
    // DISABLE AGDC: Tell WindowServer we DON'T support AGDC to prevent initialization failures
    // WindowServer was crashing because we claimed AGDC support but didn't implement it
    setProperty("AGDC", kOSBooleanFalse);                   // NOT AGDC capable
    setProperty("AGDCCapable", kOSBooleanFalse);            // NO AGDC capability
    setProperty("AGDCVersion", OSNumber::withNumber(0ULL, 32));    // No AGDC version
    // No AGDC capabilities at all
    setProperty("AGDCCapabilities", OSNumber::withNumber(0ULL, 32));
    
    // DISABLE GPU Controller - we're a simple framebuffer
    
    // ENABLE Hardware Video Acceleration for VirtIO GPU
    // Tell WindowServer we have hardware video acceleration capabilities
    setProperty("IOVideoAcceleration", kOSBooleanTrue);    // Hardware video acceleration
    setProperty("IOHardwareVideoAcceleration", kOSBooleanTrue); // HW video accel enabled
    setProperty("IOGVAHEVCDecodeCapabilities", 0ULL, 64);   // HEVC decode (basic support)
    setProperty("IOGVACodec", kOSBooleanTrue);             // Video codec support enabled
    
    // ENABLE Metal compositor with minimal software renderer plugin
    // This provides a valid MTLDevice pointer to prevent WindowServer abort()
    setProperty("IOMetalBundleName", "");                           // No external bundle needed
    setProperty("IOGLESBundleName", "");                            // No OpenGL ES
    setProperty("PerformanceStatistics", OSArray::withCapacity(0)); // Empty but non-null
    
    // Graphics device properties  
    setProperty("IOGraphicsDevice", kOSBooleanTrue);
    // CRITICAL: Must declare as console device for boot display activation
    setProperty("IOConsoleDevice", kOSBooleanTrue);
    
    // IOFramebufferMemoryBandwidth is REQUIRED for macOS to activate display
    UInt32 memoryBandwidth = 1024 * 1024 * 1024; // 1 GB/sec bandwidth (typical for virtual GPU)
    setProperty("IOFramebufferMemoryBandwidth", memoryBandwidth);
    IOLog("VMVirtIOFramebuffer::start() - Set IOFramebufferMemoryBandwidth = %u bytes\n", memoryBandwidth);
    
    // *** CRITICAL: Primary Graphics Device Properties ***
    // These properties tell macOS this framebuffer is the primary display controller
    setProperty("IOPrimaryGraphicsDevice", kOSBooleanTrue);  // Primary graphics device
    setProperty("IOPrimaryDisplay", kOSBooleanTrue);         // Primary display
    setProperty("IODisplayClass", "IOFramebuffer");          // Display class
    setProperty("IOMatchCategory", "IOFramebuffer");         // Match category for display
    
    // *** CRITICAL: Publish as main display resource ***
    // This makes the framebuffer discoverable by Display Preferences
    if (m_pci_device) {
        setProperty("IOPCIDevice", m_pci_device);  // Link to PCI device for system info
        IOLog("VMVirtIOFramebuffer::start() - Linked to PCI device for system display discovery\n");
    }
    
    // NOTE: Do NOT set name to "display" - IODisplayWrangler will create display0 nub automatically
    // The framebuffer name should remain as provided by the system (e.g., "S10@2")
    IOLog("VMVirtIOFramebuffer::start() - Framebuffer registered, IODisplayWrangler will create display0 nub\n");
    
    // *** CRITICAL: Set IOUserClientClass at runtime ***
    // Programmatically created services don't inherit personality properties from Info.plist
    // We must set IOUserClientClass here so IOFramebuffer::newUserClient() can instantiate VMQemuVGAClient
    OSString* userClientClass = OSString::withCString("VMQemuVGAClient");
    if (userClientClass) {
        setProperty("IOUserClientClass", userClientClass);
        userClientClass->release();
        IOLog("VMVirtIOFramebuffer::start() - IOUserClientClass set to VMQemuVGAClient\n");
    } else {
        IOLog("VMVirtIOFramebuffer::start() - ERROR: Failed to create IOUserClientClass string\n");
    }
    
    IOLog("VMVirtIOFramebuffer::start() - Simple framebuffer mode (no AGDC, no HW video accel)\n");
    
    // SIMPLE: Basic framebuffer setup like QXL
    setProperty("IOBootDisplay", kOSBooleanTrue);
    setProperty("IOPrimaryDisplay", kOSBooleanTrue);
    
    // Initialize display modes
    initDisplayModes();
    
    // Set basic framebuffer index
    OSNumber* index_zero = OSNumber::withNumber((unsigned long long)0, 32);
    if (index_zero) {
        setProperty("IOFramebufferIndex", index_zero);
        setProperty("IODisplayIndex", index_zero);
        index_zero->release();
    }
    
    // *** CRITICAL: DO NOT call enableController() here ***
    // IOGraphicsFamily will call it when ready - calling it too early breaks activation
    IOLog("VMVirtIOFramebuffer::start() - Controller enable deferred to IOGraphicsFamily\n");
    
    // *** TEST: Disable AGDC service to isolate GUI login issue ***
    IOLog("VMVirtIOFramebuffer::start() - AGDC service creation DISABLED for testing\n");
    // IOReturn agdc_result = createAGDCService();
    // if (agdc_result == kIOReturnSuccess) {
    //     IOLog("VMVirtIOFramebuffer::start() - AGDC service created successfully\n");
    // } else {
    //     IOLog("VMVirtIOFramebuffer::start() - WARNING: AGDC service creation failed: 0x%08x\n", agdc_result);
    // }
    
    // Initialize display mode variables (copied from QXL - must be done before power management)
    m_display_mode = 0;  // Will be set by IOGraphicsFamily when enableController() is called
    m_depth_mode = 0;
    IOLog("VMVirtIOFramebuffer::start() - Display mode variables initialized\n");
    
    // TESTING: Skip power management initialization - IOFramebuffer may handle this automatically
    // The panic at IOGraphicsFamily + 88033 (offset 0x110) suggests power management structure is
    // being accessed before it's ready. Let IOFramebuffer base class initialize power management.
    IOLog("VMVirtIOFramebuffer::start() - Skipping manual power management init (let IOFramebuffer handle it)\n");
    
    // *** CRITICAL: Make this the boot console device ***
    // The framebuffer itself must declare it's the console device, not just the provider
    // This is what triggers IOGraphicsFamily to create display0 and AppleDisplay children
    setProperty("IOConsoleDevice", kOSBooleanTrue);
    setProperty("IOBootDisplay", kOSBooleanTrue);
    IOLog("VMVirtIOFramebuffer::start() - Declared as boot console device\n");
    
    // *** CRITICAL: Create accelerator child service (like QXL) ***
    // IOGraphicsFamily requires an IOAccelerator child to create display connections
    IOLog("VMVirtIOFramebuffer::start() - Creating accelerator child service...\n");
    m_accelerator = OSTypeAlloc(VMQemuVGAAccelerator);
    if (m_accelerator) {
        if (m_accelerator->init()) {
            if (m_accelerator->attach(this)) {
                if (m_accelerator->start(this)) {
                    m_accelerator->registerService(kIOServiceAsynchronous);
                    IOLog("VMVirtIOFramebuffer::start() - Accelerator registered successfully\n");

                    // FB-side IOAccel trio for IOAccelFindAccelerator() —
                    // the ARBITER ENABLER for the readfb rung ladder
                    // (Apple's IOGraphics/tools/readfb.c consumer path).
                    // Re-landed 2026-08-15 from the session tool-record
                    // after the original working-tree edit was silently
                    // discarded (never committed — commit-before-boot is
                    // now the rule). Scope, established by observation:
                    // this gates IOAccel-API consumers ONLY. WindowServer
                    // reaches the accelerator trio-INDEPENDENTLY (observed
                    // holding IOAccelerationUserClient, "pid 97,
                    // WindowServer", on the trio-less build). Do not treat
                    // this as the WindowServer gate.
                    //
                    // Consumed keys per the 10.6 SDK
                    // IOGraphicsInterfaceTypes.h: IOAccelTypes (IOService-
                    // plane path STRING of the accelerator, set on the FB —
                    // NOT a number, NOT on the accelerator), IOAccelIndex,
                    // IOAccelRevision (=kCurrentGraphicsInterfaceRevision
                    // =2, era-verified in the 10.6 SDK header). Published
                    // UNCONDITIONALLY: an honest claim — the accelerator
                    // exists and is registered directly above — and a
                    // conditional publish makes "never executed"
                    // indistinguishable from "executed and didn't help".
                    // The path string is asserted in the log so the boot
                    // proves the write fired; if rung 1 fails with the
                    // trio present, the STRING is the first suspect
                    // (check its shape in ioreg against what
                    // IOAccelFindAccelerator expects). IOCFPlugInTypes is
                    // deliberately NOT published (step 2b — advertises a
                    // CFPlugIn that does not exist).
                    {
                        char accelPath[192];
                        accelPath[0] = '\0';
                        /* IORegistryEntry::getPath(char*, int *length,
                         * plane) — length is in/out; signature per kernel
                         * headers. */
                        int pathLen = (int)sizeof(accelPath);
                        bool gotPath = m_accelerator->getPath(
                            accelPath, &pathLen, gIOServicePlane);
                        if (gotPath && accelPath[0]) {
                            setProperty("IOAccelTypes", accelPath);
                            setProperty("IOAccelIndex",
                                        (uint64_t)0, 32);
                            setProperty("IOAccelRevision",
                                        (uint64_t)kCurrentGraphicsInterfaceRevision,
                                        32);
                            /* RUNG 17 — the caller's key. The trio's
                             * IOAccelTypes (kIOAccelTypesKey) is read by
                             * nothing on the CGL path; IOAccelFindAccelerator
                             * (IOKit disasm) does CFDictionaryGetValue(props,
                             * "IOAccelerator") on the DISPLAY service, then
                             * IORegistryEntryFromPath + conformsTo
                             * "IOAccelerator". Same path string the trio
                             * computed, the key the caller actually reads.
                             * Gated with the rest of the vm-cap3d family:
                             * ungated boots stay byte-identical and the
                             * revert is the arg, not a rebuild. */
                            if (VMVirtIOGPU::cap3dPublishGate()) {
                                setProperty("IOAccelerator", accelPath);
                                IOLog("VMVirtIOFramebuffer: rung 17 — "
                                      "IOAccelerator=\"%s\" published "
                                      "(gate=1)\n", accelPath);
                            }
                            IOLog("VMVirtIOFramebuffer: IOAccel trio "
                                  "published — IOAccelTypes=\"%s\" "
                                  "Index=0 Revision=%u\n",
                                  accelPath,
                                  (unsigned)kCurrentGraphicsInterfaceRevision);
                        } else {
                            IOLog("VMVirtIOFramebuffer: IOAccel trio "
                                  "FAILED — no IOService-plane path for "
                                  "accelerator %p\n", (void *)m_accelerator);
                        }
                    }
                } else {
                    IOLog("VMVirtIOFramebuffer::start() - WARNING: Accelerator start() failed\n");
                    m_accelerator->detach(this);
                    m_accelerator->release();
                    m_accelerator = nullptr;
                }
            } else {
                IOLog("VMVirtIOFramebuffer::start() - WARNING: Accelerator attach() failed\n");
                m_accelerator->release();
                m_accelerator = nullptr;
            }
        } else {
            IOLog("VMVirtIOFramebuffer::start() - WARNING: Accelerator init() failed\n");
            m_accelerator->release();
            m_accelerator = nullptr;
        }
    } else {
        IOLog("VMVirtIOFramebuffer::start() - WARNING: Failed to allocate accelerator\n");
    }
    
    // Register service to publish in IORegistry (like QXL does at end of start())
    registerService(kIOServiceAsynchronous);
    IOLog("VMVirtIOFramebuffer::start() - Service registered for display matching\n");
    
    // CRITICAL: Manually create display0 nub since IODisplayWrangler may have already scanned
    // IODisplayWrangler::makeDisplayConnects() creates IODisplayConnect nub named "display0"
    IOLog("VMVirtIOFramebuffer::start() - Creating display0 connection nub\n");
    IODisplayWrangler::makeDisplayConnects(this);
    IOLog("VMVirtIOFramebuffer::start() - display0 nub created\n");

    // Forced setupForCurrentConfig removed — it was a workaround from the era
    // when the base class never drove the display lifecycle. Calling it during
    // start() is TOO EARLY: m_fb_device_memory doesn't exist yet (enableController
    // hasn't run), so getApertureRange falls through to the 4 KB m_vram_range.
    // IOGraphicsFamily dereferences based on that 4 KB aperture → NULL → panic.
    // The base class calls setupForCurrentConfig at the right time (after
    // enableController, when the framebuffer is live).

    // Phase 2: Fixed framebuffer buffer allocation. Sized for the largest mode
    // we intend to advertise. Lives until free(); setupFramebufferResource
    // creates the virtio resource against this buffer but does NOT realloc it.
    //
    // ceilings[] is ordered largest-first; the first that succeeds wins. The
    // fallback ladder handles contiguous-allocation failure (more likely at
    // larger sizes, especially late in boot when memory is fragmented).
    {
        struct { uint32_t w, h; const char* label; } ceilings[] = {
            {4096, 2160, "4096x2160 (35.4 MB)"},
            {2560, 1600, "2560x1600 (16.4 MB)"},
            {1920, 1200, "1920x1200 (9.2 MB)"},
            {1600, 1200, "1600x1200 (7.3 MB)"},
            {1280, 1024, "1280x1024 (5.0 MB)"},
            {1024, 768,  "1024x768  (3.0 MB)"},
        };
        bool allocated = false;
        for (size_t i = 0; i < sizeof(ceilings)/sizeof(ceilings[0]); i++) {
            size_t try_size = (size_t)ceilings[i].w * ceilings[i].h * 4;
            IOBufferMemoryDescriptor* try_backing = IOBufferMemoryDescriptor::withOptions(
                kIODirectionInOut | kIOMemoryPhysicallyContiguous,  // options
                try_size,                                             // capacity
                page_size);                                           // alignment
            if (!try_backing) {
                IOLog("VMVirtIOFramebuffer::start() - ceiling %s: withOptions returned NULL, trying smaller\n",
                      ceilings[i].label);
                continue;
            }
            IOReturn prepare_ret = try_backing->prepare(kIODirectionInOut);
            if (prepare_ret != kIOReturnSuccess) {
                IOLog("VMVirtIOFramebuffer::start() - ceiling %s: prepare 0x%x, trying smaller\n",
                      ceilings[i].label, prepare_ret);
                try_backing->release();
                continue;
            }
            IOPhysicalAddress phys = try_backing->getPhysicalSegment(0, nullptr, kIOMemoryMapperNone);
            IODeviceMemory* try_dev_mem = IODeviceMemory::withRange(phys, try_size);
            if (!try_dev_mem) {
                IOLog("VMVirtIOFramebuffer::start() - ceiling %s: IODeviceMemory::withRange failed, trying smaller\n",
                      ceilings[i].label);
                try_backing->complete(kIODirectionInOut);
                try_backing->release();
                continue;
            }
            // Self-check: actual allocation length must match the requested size.
            // Catches the IOBufferMemoryDescriptor::withOptions arg-order bug
            // (allocates a tiny buffer while reporting the requested size).
            IOByteCount actual_len = try_backing->getLength();
            if (actual_len != (IOByteCount)try_size) {
                IOLog("VMVirtIOFramebuffer::start() - ceiling %s: SIZE MISMATCH actual=%llu expected=%llu, trying smaller\n",
                      ceilings[i].label, (uint64_t)actual_len, (uint64_t)try_size);
                try_dev_mem->release();
                try_backing->complete(kIODirectionInOut);
                try_backing->release();
                continue;
            }
            // Aperture invariant: getApertureRange returns an IODeviceMemory
            // describing ONE physical range. There is no honest way to build
            // one over a fragmented buffer — IODeviceMemory::withRange(phys,
            // len) would describe a single contiguous physical run that
            // doesn't exist, and every WindowServer write past segment 1
            // lands on unrelated kernel memory (same corruption class as the
            // old zone free-list panics). kIOMemoryPhysicallyContiguous
            // should guarantee a single segment, but at 35 MB on this 4 GB
            // guest it has been observed returning 2 segments (attachBacking
            // reported nr_entries=2). Walk the allocation and require
            // nr_entries == 1; step down if not.
            {
                unsigned seg_count = 0;
                IOByteCount walk_off = 0;
                IOByteCount walk_len = 0;
                while (try_backing->getPhysicalSegment(walk_off, &walk_len, kIOMemoryMapperNone) != 0) {
                    seg_count++;
                    walk_off += walk_len;
                    if (walk_len == 0) break;
                }
                if (seg_count != 1) {
                    IOLog("VMVirtIOFramebuffer::start() - ceiling %s: allocation fragmented into %u segments (aperture requires 1) — stepping down\n",
                          ceilings[i].label, seg_count);
                    try_dev_mem->release();
                    try_backing->complete(kIODirectionInOut);
                    try_backing->release();
                    continue;
                }
            }
            // Success — commit.
            m_fb_backing = try_backing;
            m_fb_device_memory = try_dev_mem;
            m_fb_allocation_width = ceilings[i].w;
            m_fb_allocation_height = ceilings[i].h;
            IOLog("VMVirtIOFramebuffer::start() - Fixed framebuffer buffer allocated: %s phys=0x%llx len=%llu\n",
                  ceilings[i].label, (uint64_t)phys, (uint64_t)try_size);
            allocated = true;
            break;
        }
        if (!allocated) {
            IOLog("VMVirtIOFramebuffer::start() - *** ALL FRAMEBUFFER CEILINGS FAILED *** — display will not work\n");
            // Continue; downstream calls will fail visibly rather than silently corrupt.
        }
    }

    // Phase 4: filter the supported mode list based on which ceiling succeeded.
    // Prevents advertising a mode the buffer can't back. Also sets m_current_mode
    // (preferred default 1920×1080 if it fits, else largest fitting mode).
    filterModesByAllocation();

    // Publish actual VRAM size from the allocation. Overrides the default
    // 512 MB placeholder set earlier in start() — System Profiler displays
    // this to users, and 512 MB against a real 16 MB aperture is misleading.
    // Also remove ATY,memsize (ATI-specific key, meaningless on virtio).
    if (m_fb_backing) {
        uint64_t actual_bytes = m_fb_backing->getLength();
        uint32_t actual_mb = (uint32_t)(actual_bytes / (1024U * 1024U));
        OSNumber* vram_actual = OSNumber::withNumber(actual_bytes, 64);
        OSNumber* vram_mb_actual = OSNumber::withNumber(actual_mb, 32);
        if (vram_actual) {
            setProperty("VRAM,totalsize", vram_actual);
            setProperty("IOAccelMemorySize", vram_actual);
            vram_actual->release();
        }
        if (vram_mb_actual) {
            setProperty("VRAM,totalMB", vram_mb_actual);
            vram_mb_actual->release();
        }
        removeProperty("ATY,memsize");
        IOLog("VMVirtIOFramebuffer::start() - VRAM published: %u MB (%llu bytes), removed ATY,memsize\n",
              actual_mb, actual_bytes);
    }

    IOLog("VMVirtIOFramebuffer::start() - Initialization complete\n");

    return true;
}

void VMVirtIOFramebuffer::stop(IOService* provider)
{
    IOLog("VMVirtIOFramebuffer::stop() - Stopping framebuffer\n");

    // Clean up accelerator
    if (m_accelerator) {
        m_accelerator->terminate();
        m_accelerator->release();
        m_accelerator = nullptr;
    }

    teardownFramebufferResource();

    if (m_vram_range) {
        m_vram_range->release();
        m_vram_range = nullptr;
    }

    super::stop(provider);
}

void VMVirtIOFramebuffer::initDisplayModes()
{
    // Phase 4: mode list is populated by filterModesByAllocation() after
    // start() allocates the fixed buffer. Here we just reset state so a
    // re-init doesn't carry stale modes.
    m_mode_count = 0;
    m_current_mode = 0;  // set by filterModesByAllocation
}

void VMVirtIOFramebuffer::filterModesByAllocation()
{
    // Phase 4: build the advertised mode list from kSupportedModes, keeping
    // only the modes whose backing requirement (w × h × 4) fits in the fixed
    // buffer allocated by start(). m_display_modes is filled in kSupportedModes
    // order (smallest to largest), so WindowServer sees a sane ordering.
    //
    // m_current_mode picks the default-flagged entry (1920×1080) when it
    // fits; otherwise falls back to the largest fitting mode. The fallback
    // ladder in start() ends at 1024×768, so at least mode 1 always fits.
    m_mode_count = 0;
    m_current_mode = 0;
    IODisplayModeID default_mode = 0;
    IODisplayModeID largest_fitting = 0;
    uint64_t largest_pixels = 0;

    if (!m_fb_backing) {
        IOLog("VMVirtIOFramebuffer::filterModesByAllocation: no fixed buffer — no modes advertised\n");
        return;
    }
    uint64_t alloc_bytes = (uint64_t)m_fb_backing->getLength();

    for (size_t i = 0; i < kNumSupportedModes; i++) {
        uint64_t mode_bytes = (uint64_t)kSupportedModes[i].width
                              * (uint64_t)kSupportedModes[i].height * 4ULL;
        if (mode_bytes > alloc_bytes) {
            IOLog("VMVirtIOFramebuffer::filterModesByAllocation: skip mode %u (%ux%u, %llu bytes > %llu buffer)\n",
                  kSupportedModes[i].id, kSupportedModes[i].width, kSupportedModes[i].height,
                  mode_bytes, alloc_bytes);
            continue;
        }
        if (m_mode_count >= sizeof(m_display_modes) / sizeof(m_display_modes[0])) {
            IOLog("VMVirtIOFramebuffer::filterModesByAllocation: m_display_modes[] full, dropping remaining modes\n");
            break;
        }
        m_display_modes[m_mode_count++] = kSupportedModes[i].id;
        uint64_t pixels = (uint64_t)kSupportedModes[i].width
                          * (uint64_t)kSupportedModes[i].height;
        if (pixels > largest_pixels) {
            largest_pixels = pixels;
            largest_fitting = kSupportedModes[i].id;
        }
        if (kSupportedModes[i].is_default) {
            default_mode = kSupportedModes[i].id;
        }
    }

    // Boot mode: preferred default if it fits, else the largest fitting mode.
    // m_current_mode drives what enableController sets up.
    m_current_mode = (default_mode != 0) ? default_mode : largest_fitting;

    IOLog("VMVirtIOFramebuffer::filterModesByAllocation: %u modes advertised, boot mode = %u, buffer = %llu bytes\n",
          (unsigned)m_mode_count, m_current_mode, alloc_bytes);
}

// IOFramebuffer required pure virtual methods

// TEMPORARY: report hardware cursor as unsupported. The base class returns 1
// for kIOHardwareCursorAttribute ('crsr') by default, which makes WindowServer
// skip software cursor compositing and rely on setCursorImage/setCursorState —
// both of which are no-ops in this driver. Result: invisible cursor.
// END STATE: implement virtio-gpu cursor queue (queue 1, UPDATE_CURSOR /
// MOVE_CURSOR) and flip this back to returning 1 via super::getAttribute.
IOReturn VMVirtIOFramebuffer::getAttribute(IOSelect attribute, uintptr_t* value)
{
    if (attribute == kIOHardwareCursorAttribute) {
        if (value) *value = 0;  // no hardware cursor
        return kIOReturnSuccess;
    }
    return super::getAttribute(attribute, value);
}

IODeviceMemory* VMVirtIOFramebuffer::getApertureRange(IOPixelAperture aperture)
{
    IOLog("VMVirtIOFramebuffer::getApertureRange: aperture=%d\n", (int)aperture);

    if (aperture != kIOFBSystemAperture) {
        return nullptr;
    }

    // VirtIO GPU path: framebuffer backing is allocated and owned by us.
    // Same memory is the resource backing (host reads pixels here) and the
    // transfer source — load-bearing invariant.
    if (m_fb_device_memory) {
        IOLog("VMVirtIOFramebuffer::getApertureRange: returning fb backing %p (size=%llu)\n",
              m_fb_device_memory, m_fb_device_memory->getLength());
        m_fb_device_memory->retain();
        return m_fb_device_memory;
    }

    // Not ready — return NULL. IOFramebuffer knows how to fail a setup;
    // it has no defence against being told 4 KB of MMIO is a framebuffer.
    // The previous fallback to m_vram_range (4 KB PCI BAR1) or PCI BAR 0
    // silently handed IOGraphicsFamily a register aperture as if it were
    // VRAM → NULL dereference when IOGraphicsFamily dereferenced structures
    // sized for the declared display mode against a 4 KB range.
    IOLog("VMVirtIOFramebuffer::getApertureRange: m_fb_device_memory is NULL — returning NULL (not ready yet)\n");
    return nullptr;
}

// CRITICAL: WindowServer calls this to get the framebuffer memory for rendering
IODeviceMemory* VMVirtIOFramebuffer::getVRAMRange(void)
{
    /* IOLog gate (2026-08-17): WindowServer re-requests this
     * continuously while compositing — 2 lines per call, a top-3
     * kernel.log contributor under SMP browsing. First 32 log, then
     * quiet. The NULL-backing error below is NOT gated. */
    static uint32_t s_vram_log = 0;
    const bool vram_log = (s_vram_log < 32);
    if (vram_log) s_vram_log++;
    if (vram_log)
        IOLog("VMVirtIOFramebuffer::getVRAMRange() - *** WINDOWSERVER REQUESTING FRAMEBUFFER MEMORY ***\n");

    // VirtIO GPU path: same backing as the aperture and the resource — single
    // allocation, three roles.
    if (m_fb_device_memory) {
        if (vram_log)
            IOLog("VMVirtIOFramebuffer::getVRAMRange() - Returning fb backing %p phys=0x%llx len=%llu\n",
                  m_fb_device_memory,
                  (unsigned long long)m_fb_device_memory->getPhysicalAddress(),
                  (unsigned long long)m_fb_device_memory->getLength());
        m_fb_device_memory->retain();
        return m_fb_device_memory;
    }

    // Same as getApertureRange: return NULL when framebuffer not ready.
    // No m_vram_range or PCI BAR fallback — those hand out register MMIO
    // as if it were framebuffer memory.
    IOLog("VMVirtIOFramebuffer::getVRAMRange() - m_fb_device_memory is NULL — returning NULL\n");
    return nullptr;
}

const char* VMVirtIOFramebuffer::getPixelFormats(void)
{
    // Return OpenGL-compatible pixel formats for hardware acceleration
    // Support ARGB8888 (32-bit with alpha) and RGB888 (24-bit) for OpenGL
    IOLog("VMVirtIOFramebuffer::getPixelFormats() - Returning OpenGL-compatible formats (ARGB8888, RGB888)\n");
    
    // Standard IOKit pixel format specification for OpenGL support
    // Format: IO32BitDirectPixels and IO16BitDirectPixels for acceleration
    static const char* pixelFormats = 
        IO32BitDirectPixels "\0" IO16BitDirectPixels "\0" IO8BitIndexedPixels "\0\0";
    
    return pixelFormats;
}

IOItemCount VMVirtIOFramebuffer::getDisplayModeCount(void)
{
    IOLog("VMVirtIOFramebuffer::getDisplayModeCount() - Returning %u modes\n", m_mode_count);
    return m_mode_count;
}

IOReturn VMVirtIOFramebuffer::getDisplayModes(IODisplayModeID* allDisplayModes)
{
    IOLog("VMVirtIOFramebuffer::getDisplayModes() - WindowServer requesting display mode list\n");
    if (!allDisplayModes) {
        return kIOReturnBadArgument;
    }
    
    for (IOItemCount i = 0; i < m_mode_count; i++) {
        allDisplayModes[i] = m_display_modes[i];
        IOLog("  Mode %u: ID=%d\n", i, m_display_modes[i]);
    }
    
    return kIOReturnSuccess;
}

IOReturn VMVirtIOFramebuffer::getInformationForDisplayMode(IODisplayModeID displayMode,
                                                           IODisplayModeInformation* info)
{
    if (!info) {
        return kIOReturnBadArgument;
    }

    // Lookup in kSupportedModes — single source of truth for the mode table.
    for (size_t i = 0; i < kNumSupportedModes; i++) {
        if (kSupportedModes[i].id == displayMode) {
            info->flags = kDisplayModeValidFlag | kDisplayModeSafeFlag;
            if (kSupportedModes[i].is_default) {
                info->flags |= kDisplayModeDefaultFlag;
            }
            info->refreshRate = 60 << 16;  // 60 Hz in fixed point
            info->maxDepthIndex = 0;        // Only support 32-bit depth
            info->nominalWidth = kSupportedModes[i].width;
            info->nominalHeight = kSupportedModes[i].height;
            return kIOReturnSuccess;
        }
    }
    return kIOReturnUnsupported;
}

UInt64 VMVirtIOFramebuffer::getPixelFormatsForDisplayMode(IODisplayModeID displayMode, IOIndex depth)
{
    // Return 32-bit ARGB format
    return 1ULL << 30; // kIO32BGRAPixelFormat
}

IOReturn VMVirtIOFramebuffer::getPixelInformation(IODisplayModeID displayMode, IOIndex depth,
                                                  IOPixelAperture aperture, IOPixelInformation* pixelInfo)
{
    if (!pixelInfo) {
        return kIOReturnBadArgument;
    }
    
    // Get display mode information
    IODisplayModeInformation modeInfo;
    IOReturn result = getInformationForDisplayMode(displayMode, &modeInfo);
    if (result != kIOReturnSuccess) {
        return result;
    }

    // bzero on entry. Callers reuse one IOPixelInformation across mode
    // enumeration, and the struct has fields we don't otherwise touch
    // (componentMasks[3..15], pixelFormat, reserved[0..1]). Without bzero,
    // those inherit stale data from the previous call. Also catches the
    // first-call case where stack garbage shows up in the unread fields.
    bzero(pixelInfo, sizeof(*pixelInfo));

    pixelInfo->bytesPerRow = modeInfo.nominalWidth * 4; // 32-bit pixels
    pixelInfo->bytesPerPlane = pixelInfo->bytesPerRow * modeInfo.nominalHeight;
    pixelInfo->bitsPerPixel = 32;
    pixelInfo->pixelType = kIORGBDirectPixels;
    pixelInfo->componentCount = 3;
    pixelInfo->bitsPerComponent = 8;
    pixelInfo->componentMasks[0] = 0x00FF0000; // Red
    pixelInfo->componentMasks[1] = 0x0000FF00; // Green
    pixelInfo->componentMasks[2] = 0x000000FF; // Blue
    // componentMasks[3..15]: left at 0 by bzero
    strlcpy(pixelInfo->pixelFormat, IO32BitDirectPixels, sizeof(pixelInfo->pixelFormat));
    pixelInfo->flags = 0;
    pixelInfo->activeWidth = modeInfo.nominalWidth;
    pixelInfo->activeHeight = modeInfo.nominalHeight;
    // reserved[0..1]: left at 0 by bzero

    // Instrumentation: log what we RETURN (after all assignments) alongside
    // the live mode. (Earlier placement of this log was misleading — it fired
    // before the activeWidth/activeHeight assignments and read stale caller
    // state, not what we returned. Now at the end where it belongs.)
    IOLog("VMVirtIOFramebuffer::getPixelInformation: queried mode=%u → bytesPerRow=%u active=%ux%u | live mode=%u m_width=%u m_height=%u\n",
          (unsigned)displayMode, (unsigned)pixelInfo->bytesPerRow,
          (unsigned)pixelInfo->activeWidth, (unsigned)pixelInfo->activeHeight,
          (unsigned)m_current_mode, (unsigned)m_width, (unsigned)m_height);

    return kIOReturnSuccess;
}

IOReturn VMVirtIOFramebuffer::getCurrentDisplayMode(IODisplayModeID* displayMode, IOIndex* depth)
{
    if (displayMode) {
        *displayMode = m_current_mode;
    }
    if (depth) {
        *depth = 0; // 32-bit depth index
    }
    return kIOReturnSuccess;
}

// CRITICAL: Provide timing information for WindowServer validation
IOReturn VMVirtIOFramebuffer::getTimingInfoForDisplayMode(IODisplayModeID displayMode, IOTimingInformation* info)
{
    if (!info) {
        return kIOReturnBadArgument;
    }
    
    IOLog("VMVirtIOFramebuffer::getTimingInfoForDisplayMode() - mode=%d\n", (int)displayMode);
    
    // Clear the structure
    bzero(info, sizeof(IOTimingInformation));
    
    // Get mode information
    IODisplayModeInformation modeInfo;
    IOReturn result = getInformationForDisplayMode(displayMode, &modeInfo);
    if (result != kIOReturnSuccess) {
        IOLog("VMVirtIOFramebuffer::getTimingInfoForDisplayMode() - Failed to get mode info\n");
        return result;
    }
    
    // Set timing information for standard VESA timing
    // Use known Apple timing IDs when available
    if (modeInfo.nominalWidth == 1024 && modeInfo.nominalHeight == 768) {
        info->appleTimingID = timingVESA_1024x768_60hz;
    } else if (modeInfo.nominalWidth == 1280 && modeInfo.nominalHeight == 1024) {
        info->appleTimingID = timingVESA_1280x1024_60hz;
    } else if (modeInfo.nominalWidth == 1920 && modeInfo.nominalHeight == 1440) {
        info->appleTimingID = timingVESA_1920x1440_60hz;
    } else {
        // For other resolutions, use detailed timing only (timingInvalid means no Apple timing ID)
        info->appleTimingID = timingInvalid;
    }
    
    // Set the flags to indicate this is valid timing info
    info->flags = kIODetailedTimingValid;
    
    // Fill in detailed timing information
    IODetailedTimingInformationV2* detailed = &info->detailedInfo.v2;
    
    detailed->pixelClock = modeInfo.nominalWidth * modeInfo.nominalHeight * 60; // 60Hz refresh
    detailed->horizontalActive = modeInfo.nominalWidth;
    detailed->horizontalBlanking = modeInfo.nominalWidth / 4; // 25% blanking
    detailed->verticalActive = modeInfo.nominalHeight;
    detailed->verticalBlanking = modeInfo.nominalHeight / 20; // 5% blanking
    
    detailed->horizontalSyncOffset = 8;
    detailed->horizontalSyncPulseWidth = 32;
    detailed->verticalSyncOffset = 1;
    detailed->verticalSyncPulseWidth = 3;
    
    detailed->horizontalBorderLeft = 0;
    detailed->horizontalBorderRight = 0;
    detailed->verticalBorderTop = 0;
    detailed->verticalBorderBottom = 0;
    
    // Sync configuration: positive sync for both horizontal and vertical
    detailed->horizontalSyncConfig = 1;  // 1 = positive sync
    detailed->verticalSyncConfig = 1;    // 1 = positive sync
    
    detailed->signalConfig = kIODigitalSignal;
    detailed->signalLevels = 0;
    
    detailed->pixelClock = detailed->pixelClock / 1000000; // Convert to MHz
    detailed->minPixelClock = detailed->pixelClock;
    detailed->maxPixelClock = detailed->pixelClock;
    
    IOLog("VMVirtIOFramebuffer::getTimingInfoForDisplayMode() - Returning timing for %dx%d@60Hz\n",
          (int)modeInfo.nominalWidth, (int)modeInfo.nominalHeight);
    
    return kIOReturnSuccess;
}

// CRITICAL: Override newUserClient to provide VMQemuVGAClient for WindowServer
// Required because programmatically created services don't get personality properties
IOReturn VMVirtIOFramebuffer::newUserClient(task_t owningTask, void* securityID,
                                            UInt32 type, IOUserClient** handler)
{
    IOLog("VMVirtIOFramebuffer::newUserClient() - WindowServer requesting user client (type=%u)\n", type);

    // type 0 (kIOFBServerConnectType) and type 1 (kIOFBSharedConnectType) belong to
    // IOFramebuffer — WindowServer expects an IOFramebufferUserClient created by the
    // parent class. Interception here hands WindowServer a VMQemuVGAClient instead,
    // and every method call fails with inputCount count mismatch because the two
    // dispatch tables are unrelated. Delegate to super; reserve VMQemuVGAClient for
    // private custom-type connections (high type numbers).
    if (type == 0 || type == 1) {
        IOLog("VMVirtIOFramebuffer::newUserClient() - type=%u is IOFramebuffer standard, delegating to super\n", type);
        return super::newUserClient(owningTask, securityID, type, handler);
    }

    // Custom type — build the private VMQemuVGAClient.
    IOLog("VMVirtIOFramebuffer::newUserClient() - type=%u is custom, constructing VMQemuVGAClient\n", type);
    IOUserClient* client = OSTypeAlloc(VMQemuVGAClient);
    if (!client) {
        IOLog("VMVirtIOFramebuffer::newUserClient() - Failed to allocate VMQemuVGAClient\n");
        *handler = nullptr;
        return kIOReturnNoMemory;
    }
    
    // Initialize the client with the task
    if (!client->initWithTask(owningTask, securityID, type)) {
        IOLog("VMVirtIOFramebuffer::newUserClient() - Client initWithTask failed\n");
        client->release();
        *handler = nullptr;
        return kIOReturnError;
    }
    
    // Attach client to this framebuffer
    if (!client->attach(this)) {
        IOLog("VMVirtIOFramebuffer::newUserClient() - Client attach failed\n");
        client->release();
        *handler = nullptr;
        return kIOReturnError;
    }
    
    // Start the client
    if (!client->start(this)) {
        IOLog("VMVirtIOFramebuffer::newUserClient() - Client start failed\n");
        client->detach(this);
        client->release();
        *handler = nullptr;
        return kIOReturnError;
    }
    
    *handler = client;
    IOLog("VMVirtIOFramebuffer::newUserClient() - VMQemuVGAClient created successfully\n");
    return kIOReturnSuccess;
}

// CRITICAL: Safe open method override for WindowServer connection handling
IOReturn VMVirtIOFramebuffer::open(void)
{
    IOLog("VMVirtIOFramebuffer::open() - *** WINDOWSERVER OPEN REQUESTED ***\n");
    
    // Call parent implementation - now safe because we have IOUserClientClass="VMQemuVGAClient"
    IOLog("VMVirtIOFramebuffer::open() - Calling super::open()\n");
    IOReturn result = super::open();
    IOLog("VMVirtIOFramebuffer::open() - super::open() returned: 0x%x\n", result);
    
    if (result != kIOReturnSuccess) {
        IOLog("VMVirtIOFramebuffer::open() - super::open() failed, forcing success for VM compatibility\n");
        result = kIOReturnSuccess;
    }
    
    // Set properties that indicate we're ready for GUI mode
    setProperty("IOFramebufferOpenForGUI", kOSBooleanTrue);
    
    // CRITICAL: Force GUI mode properties when opened by WindowServer
    // NOTE: Keep IOConsoleDevice=true (set by isConsoleDevice()) for QXL-style dual capability
    // vm-cap3d flip experiment: this is an OVERWRITE site (runs after start());
    // ungated it would clobber the flip's IODisplayAccelerated back to false.
    // Ordinary boots publish exactly what they published before (false).
    setProperty("IODisplayAccelerated",
                VMVirtIOGPU::cap3dPublishGate() ? kOSBooleanTrue : kOSBooleanFalse);
    
    IOLog("VMVirtIOFramebuffer::open() - *** GUI MODE FORCED ON - CONSOLE MODE DISABLED ***\n");
    
    // DISABLED: Refresh timer handled by start() + enableController() - this code not needed
    // Let enableController() manage VirtIO GPU scanout and timer startup
    if (false && m_gpu_driver) {
        IOLog("VMVirtIOFramebuffer::open() - SKIPPED: Timer setup handled by enableController()\n");
        IOReturn console_result = m_gpu_driver->setscanout(0, 0, 0, 0, 0, 0);
        IOLog("VMVirtIOFramebuffer::open() - Console scanout disable returned: 0x%x\n", console_result);
        
        // CRITICAL: Now create GUI scanout - this is REQUIRED for display to work
        // The console scanout is disabled, so we MUST enable a new scanout for GUI
        IOLog("VMVirtIOFramebuffer::open() - Creating GUI display resource and scanout\n");
        
        // Create display resource on VirtIO GPU for GUI mode
        uint32_t resource_id = 1;  // Primary GUI display resource
        IOReturn createResult = m_gpu_driver->createResource2D(resource_id, 
                                                               0x1, // VIRTIO_GPU_FORMAT_B8G8R8A8_UNORM
                                                               m_width, m_height);
        if (createResult == kIOReturnSuccess) {
            IOLog("VMVirtIOFramebuffer::open() - GUI display resource created successfully\n");
            
            // CRITICAL: Attach backing storage to resource
            // VirtIO GPU spec requires backing memory for resource content
            IOLog("VMVirtIOFramebuffer::open() - Checking VRAM range: m_vram_range=%p\n", m_vram_range);
            if (m_vram_range) {
                IOLog("VMVirtIOFramebuffer::open() - Attaching VRAM backing to resource (size=%llu)\n", 
                      m_vram_range->getLength());
                IOReturn backingResult = m_gpu_driver->attachBacking(resource_id, m_vram_range);
                IOLog("VMVirtIOFramebuffer::open() - attachBacking returned: 0x%x\n", backingResult);
                if (backingResult == kIOReturnSuccess) {
                    IOLog("VMVirtIOFramebuffer::open() - *** BACKING ATTACHED SUCCESSFULLY ***\n");
                } else {
                    IOLog("VMVirtIOFramebuffer::open() - WARNING: Backing attachment failed: 0x%x\n", backingResult);
                }
            } else {
                IOLog("VMVirtIOFramebuffer::open() - WARNING: No VRAM range available for backing (NULL pointer)\n");
            }
            
            // Enable GUI scanout on scanout 0
            IOReturn scanoutResult = m_gpu_driver->setscanout(0, resource_id, 0, 0, m_width, m_height);
            if (scanoutResult == kIOReturnSuccess) {
                IOLog("VMVirtIOFramebuffer::open() - GUI scanout set successfully\n");
                
                // CRITICAL: Transfer framebuffer content to host and flush to make visible
                // VirtIO GPU spec requires transfer_to_host_2d + flush after scanout
                IOLog("VMVirtIOFramebuffer::open() - Transferring framebuffer to host resource\n");
                IOReturn transferResult = m_gpu_driver->transferToHost2D(resource_id, 0, 
                                                                         0, 0, m_width, m_height);
                if (transferResult == kIOReturnSuccess) {
                    IOLog("VMVirtIOFramebuffer::open() - Transfer successful, flushing display\n");
                    IOReturn flushResult = m_gpu_driver->flushResource(resource_id, 0, 0, 
                                                                       m_width, m_height);
                    if (flushResult == kIOReturnSuccess) {
                        IOLog("VMVirtIOFramebuffer::open() - *** GUI SCANOUT ENABLED - DISPLAY SHOULD BE ACTIVE ***\n");
                        
                        // CRITICAL: Start periodic display refresh timer
                        // This ensures framebuffer updates are continuously transferred to VirtIO GPU
                        IOLog("VMVirtIOFramebuffer::open() - Creating periodic display refresh timer\n");
                        IOWorkLoop* workloop = getWorkLoop();
                        if (workloop && !m_refresh_timer) {
                            m_refresh_timer = IOTimerEventSource::timerEventSource(this, 
                                (IOTimerEventSource::Action)&VMVirtIOFramebuffer::displayRefreshTimer);
                            if (m_refresh_timer) {
                                if (workloop->addEventSource(m_refresh_timer) == kIOReturnSuccess) {
                                    // Arm at the steady-state period; the callback re-arms FIRST
                                    // (before the work) so the wait overlaps it.
                                    m_refresh_timer->setTimeoutMS(REFRESH_PERIOD_MS);
                                    IOLog("VMVirtIOFramebuffer::open() - Display refresh timer armed (16 ms)\n");
                                } else {
                                    IOLog("VMVirtIOFramebuffer::open() - Failed to add timer to workloop\n");
                                    m_refresh_timer->release();
                                    m_refresh_timer = nullptr;
                                }
                            } else {
                                IOLog("VMVirtIOFramebuffer::open() - Failed to create refresh timer\n");
                            }
                        }
                    } else {
                        IOLog("VMVirtIOFramebuffer::open() - Flush failed: 0x%x\n", flushResult);
                    }
                } else {
                    IOLog("VMVirtIOFramebuffer::open() - Transfer to host failed: 0x%x\n", transferResult);
                }
            } else {
                IOLog("VMVirtIOFramebuffer::open() - GUI scanout failed: 0x%x\n", scanoutResult);
            }
        } else {
            IOLog("VMVirtIOFramebuffer::open() - GUI resource creation failed: 0x%x\n", createResult);
        }
    }
    
    // Forced enableController and refreshDisplay calls removed — they were
    // workarounds for the era when setupForCurrentConfig was overridden and
    // the base class never drove the display lifecycle. Now that
    // setupForCurrentConfig delegates to super, the base class handles
    // enableController and display setup in the correct order.
    IOLog("VMVirtIOFramebuffer::open() - *** WINDOWSERVER OPEN COMPLETED ***\n");
    return result;
}

void VMVirtIOFramebuffer::close(void)
{
    IOLog("VMVirtIOFramebuffer::close() - *** WINDOWSERVER CLOSE REQUESTED ***\n");
    
    // Stop display refresh timer
    if (m_refresh_timer) {
        IOLog("VMVirtIOFramebuffer::close() - Stopping display refresh timer\n");
        m_refresh_timer->cancelTimeout();
        
        IOWorkLoop* workloop = getWorkLoop();
        if (workloop) {
            workloop->removeEventSource(m_refresh_timer);
        }
        
        m_refresh_timer->release();
        m_refresh_timer = nullptr;
    }
    
    // Reset GUI mode properties when WindowServer closes
    setProperty("IOFramebufferOpenForGUI", kOSBooleanFalse);
    
    IOLog("VMVirtIOFramebuffer::close() - GUI mode properties reset\n");
    
    super::close();
    
    IOLog("VMVirtIOFramebuffer::close() - *** WINDOWSERVER CLOSE COMPLETED ***\n");
}

// Tear down any existing framebuffer resource. Safe to call when nothing is set up.
void VMVirtIOFramebuffer::teardownFramebufferResource()
{
    if (m_fb_resource_id != 0 && m_gpu_driver) {
        // deallocateResource sends VIRTIO_GPU_CMD_RESOURCE_UNREF, which makes
        // the host drop both the resource and its backing attachment. The
        // caller-owned backing memory (m_fb_backing) is NOT released here —
        // Phase 2: the buffer is allocated once in start() and lives until
        // free(). It was never registered in the resource slot's backing_memory
        // field, so deallocateResource won't try to release it either.
        m_gpu_driver->deallocateResource(m_fb_resource_id);
        m_fb_resource_id = 0;
    }
}

// Allocate framebuffer backing, create VirtIO GPU resource, attach backing,
// set scanout. Re-callable for mode changes: tears down any existing setup first.
// All three roles — aperture, backing, transfer source — use m_fb_backing,
// which is the load-bearing invariant. Physically contiguous so the same
// memory can be wrapped as IODeviceMemory for getApertureRange/getVRAMRange.
IOReturn VMVirtIOFramebuffer::setupFramebufferResource(uint32_t width, uint32_t height)
{
    if (!m_gpu_driver) {
        IOLog("VMVirtIOFramebuffer::setupFramebufferResource: no gpu_driver\n");
        return kIOReturnNotReady;
    }
    if (width == 0 || height == 0 || width > 8192 || height > 8192) {
        IOLog("VMVirtIOFramebuffer::setupFramebufferResource: bad dims %ux%u\n", width, height);
        return kIOReturnBadArgument;
    }

    // Phase 2 contract: m_fb_backing and m_fb_device_memory are allocated ONCE
    // in start() and live until free(). This function creates the virtio
    // resource against that buffer; it does NOT own or realloc the buffer.
    // Phase 3 will relax the idempotent guard to allow resource recreate on
    // mode change (UNREF + CREATE_2D + ATTACH_BACKING + SET_SCANOUT, buffer
    // preserved).
    if (!m_fb_backing || !m_fb_device_memory) {
        IOLog("VMVirtIOFramebuffer::setupFramebufferResource: no fixed buffer (start() did not allocate)\n");
        return kIOReturnNotReady;
    }

    // Phase 3: resource-recreate path. Skip only when the live resource is at
    // the requested dims (true idempotency). Otherwise fall through past the
    // guard — teardownFramebufferResource() below will UNREF the old resource
    // before createResource2D builds the new one. Buffer is preserved (Phase 2),
    // so WindowServer's aperture mapping stays valid across the recreate.
    if (m_fb_resource_id != 0 && m_width == width && m_height == height) {
        IOLog("VMVirtIOFramebuffer::setupFramebufferResource: resource %u already live at %ux%u — no recreate needed\n",
              m_fb_resource_id, width, height);
        return kIOReturnSuccess;
    }
    if (m_fb_resource_id != 0) {
        IOLog("VMVirtIOFramebuffer::setupFramebufferResource: RECREATE resource %u for new dims %ux%u (was %ux%u)\n",
              m_fb_resource_id, width, height, m_width, m_height);
    }

    // Sanity: the fixed buffer must be at least width × height × 4 bytes for
    // this mode to fit. Phase 4 will keep the advertised mode table in sync
    // with the allocation, but defend here too.
    size_t mode_size = (size_t)width * height * 4;
    if ((size_t)m_fb_backing->getLength() < mode_size) {
        IOLog("VMVirtIOFramebuffer::setupFramebufferResource: fixed buffer %llu bytes < mode %zu bytes (%ux%u) — mode doesn't fit\n",
              (uint64_t)m_fb_backing->getLength(), mode_size, width, height);
        return kIOReturnNoSpace;
    }

    // teardownFramebufferResource sends UNREF on any stale resource; no-op
    // when m_fb_resource_id is 0. Does NOT release the buffer (Phase 2).
    teardownFramebufferResource();

    // Create resource against the existing buffer, attach, scan out.
    m_fb_resource_id = 1;  // primary display resource (matches existing convention)
    IOReturn create_ret = m_gpu_driver->createResource2D(
        m_fb_resource_id,
        0x1,  // VIRTIO_GPU_FORMAT_B8G8R8A8_UNORM
        width, height,
        m_fb_backing);  // caller-owned — driver skips internal alloc
    if (create_ret != kIOReturnSuccess) {
        IOLog("VMVirtIOFramebuffer::setupFramebufferResource: createResource2D 0x%x\n", create_ret);
        teardownFramebufferResource();
        return create_ret;
    }

    // Point scanout at the new resource.
    IOReturn scanout_ret = m_gpu_driver->setscanout(0, m_fb_resource_id, 0, 0, width, height);
    if (scanout_ret != kIOReturnSuccess) {
        IOLog("VMVirtIOFramebuffer::setupFramebufferResource: setscanout 0x%x\n", scanout_ret);
        teardownFramebufferResource();
        return scanout_ret;
    }

    // Update framebuffer dimensions to match the new resource.
    m_width = width;
    m_height = height;

    IOLog("VMVirtIOFramebuffer::setupFramebufferResource: %ux%u using fixed buffer backing=%p phys=0x%llx len=%llu\n",
          width, height, m_fb_backing,
          (uint64_t)m_fb_device_memory->getPhysicalAddress(),
          (uint64_t)m_fb_device_memory->getLength());
    return kIOReturnSuccess;
}

// Phase 3 self-check: prove the resource-recreate path works. Re-calls
// setupFramebufferResource at a smaller dim, then restores. The buffer must
// stay stable (same m_fb_backing pointer, same m_fb_device_memory) across
// both recreates — that's the load-bearing invariant for mode changes.
//
// Same shape as Phase 1's probeResourceTracking: deterministic, self-checking,
// one-shot via static flag in the caller (enableController). Logs PROBE PASS
// / PROBE FAIL with the failing phase.
void VMVirtIOFramebuffer::probeResourceRecreate()
{
    IOLog("VMVirtIOFramebuffer::probeResourceRecreate: PROBE START\n");

    if (!m_fb_backing || !m_fb_device_memory || m_fb_resource_id == 0) {
        IOLog("VMVirtIOFramebuffer::probeResourceRecreate: PROBE FAIL precondition — backing=%p dev_mem=%p resource_id=%u\n",
              m_fb_backing, m_fb_device_memory, m_fb_resource_id);
        return;
    }

    // Snapshot the invariants the probe must preserve.
    IOBufferMemoryDescriptor* backing_before = m_fb_backing;
    IODeviceMemory* dev_mem_before = m_fb_device_memory;
    uint32_t saved_w = m_width;
    uint32_t saved_h = m_height;

    // Probe dims: smaller than current so it definitely fits in the buffer.
    // (Buffer is sized for the 1920×1200 ceiling; anything ≤ that fits.)
    uint32_t probe_w = (saved_w > 100) ? (saved_w - 100) : (saved_w + 100);
    uint32_t probe_h = (saved_h > 100) ? (saved_h - 100) : (saved_h + 100);

    // Phase A: recreate at probe dims. Must succeed.
    IOReturn recreate_ret = setupFramebufferResource(probe_w, probe_h);
    if (recreate_ret != kIOReturnSuccess) {
        IOLog("VMVirtIOFramebuffer::probeResourceRecreate: PROBE FAIL phase A recreate returned 0x%x\n",
              recreate_ret);
        // Try to restore to a known-good state. If this also fails, the
        // framebuffer is broken — let enableController's transferToHost2D
        // fail loudly rather than silently mask the bug.
        setupFramebufferResource(saved_w, saved_h);
        return;
    }

    // Phase B: buffer must NOT have moved. Same backing pointer + same dev_mem.
    if (m_fb_backing != backing_before || m_fb_device_memory != dev_mem_before) {
        IOLog("VMVirtIOFramebuffer::probeResourceRecreate: PROBE FAIL phase B buffer moved (backing %p → %p, dev_mem %p → %p) — recreate path is reallocating, not preserving\n",
              backing_before, m_fb_backing, dev_mem_before, m_fb_device_memory);
        setupFramebufferResource(saved_w, saved_h);
        return;
    }

    // Phase C: restore to original dims. Must also succeed.
    IOReturn restore_ret = setupFramebufferResource(saved_w, saved_h);
    if (restore_ret != kIOReturnSuccess) {
        IOLog("VMVirtIOFramebuffer::probeResourceRecreate: PROBE FAIL phase C restore returned 0x%x — framebuffer left at probe dims %ux%u\n",
              restore_ret, probe_w, probe_h);
        return;
    }

    // Phase D: buffer still must not have moved, and dims restored.
    if (m_fb_backing != backing_before || m_fb_device_memory != dev_mem_before) {
        IOLog("VMVirtIOFramebuffer::probeResourceRecreate: PROBE FAIL phase D buffer moved on restore\n");
        return;
    }
    if (m_width != saved_w || m_height != saved_h) {
        IOLog("VMVirtIOFramebuffer::probeResourceRecreate: PROBE FAIL phase D dims not restored (now %ux%u, expected %ux%u)\n",
              m_width, m_height, saved_w, saved_h);
        return;
    }

    IOLog("VMVirtIOFramebuffer::probeResourceRecreate: PROBE PASS (recreate at %ux%u → restore at %ux%u, buffer stable, resource=%u)\n",
          probe_w, probe_h, saved_w, saved_h, m_fb_resource_id);
}

// IOFramebuffer optional overrides
IOReturn VMVirtIOFramebuffer::enableController()
{
    IOLog("VMVirtIOFramebuffer::enableController() - SAFE VERSION ENTRY POINT\n");
    
    // TEMPORARY: Allow re-execution to test blue pattern
    // TODO: Re-enable safety check after testing
    static bool already_enabled = false;
    static int call_count = 0;
    call_count++;
    
    if (already_enabled && call_count > 2) {
        IOLog("VMVirtIOFramebuffer::enableController() - Already enabled, skipping duplicate call (call #%d)\n", call_count);
        return kIOReturnSuccess;
    }
    
    IOLog("VMVirtIOFramebuffer::enableController() - About to call parent enableController\n");
    
    // CRITICAL: Call parent implementation first - but safely handle failures
    IOReturn result = super::enableController();
    IOLog("VMVirtIOFramebuffer::enableController() - Parent enableController returned: 0x%x\n", result);
    
    if (result != kIOReturnSuccess) {
        IOLog("VMVirtIOFramebuffer::enableController() - Parent enableController failed: 0x%x, continuing anyway\n", result);
        // Don't return - continue with our initialization for VM compatibility
    }
    
    // Mark as enabled to prevent duplicate calls
    already_enabled = true;
    IOLog("VMVirtIOFramebuffer::enableController() - Marked as enabled, continuing with safe initialization\n");
    
    // NOTE: Console is disabled in open() via VirtIO GPU scanout disable (setscanout with resource_id=0)
    // No need for PE_Video_Console_Disable() - the VirtIO GPU method is more reliable
    
    // CRITICAL: Check connection status like Apple IONDRV does
    // This triggers the connection detection and online status reporting
    uintptr_t isOnline = 0;
    IOReturn connectionResult = getAttributeForConnection(0, kConnectionCheckEnable, &isOnline);
    if (connectionResult == kIOReturnSuccess && isOnline) {
        IOLog("VMVirtIOFramebuffer::enableController() - Connection check PASSED: Display is ONLINE\n");
    } else {
        IOLog("VMVirtIOFramebuffer::enableController() - Connection check result: 0x%x, isOnline: %lu\n", 
              connectionResult, isOnline);
    }
    
    // FORCE: Also try kConnectionEnable directly
    uintptr_t enableStatus = 0;
    IOReturn enableResult = getAttributeForConnection(0, kConnectionEnable, &enableStatus);
    IOLog("VMVirtIOFramebuffer::enableController() - kConnectionEnable check result: 0x%x, status: %lu\n", 
          enableResult, enableStatus);
    
    // FORCE: Set connection to enabled state
    IOReturn setResult = setAttributeForConnection(0, kConnectionEnable, 1);
    IOLog("VMVirtIOFramebuffer::enableController() - Force kConnectionEnable result: 0x%x\n", setResult);
    
    // ACTIVE MODE: Set up VirtIO GPU display properly for GUI activation
    // The framebuffer needs to be active to enable GUI mode
    IOLog("VMVirtIOFramebuffer::enableController() - ACTIVE MODE: Setting up VirtIO GPU display\n");
    IOLog("VMVirtIOFramebuffer::enableController() - Enabling VirtIO GPU framebuffer for GUI activation\n");
    
    // *** CRITICAL DISCOVERY FROM SPEC SECTION 5.7.7: ***
    // "VGA compatibility: PCI region 0 has the linear framebuffer, standard vga registers are present.
    //  Configuring a scanout (VIRTIO_GPU_CMD_SET_SCANOUT) switches the device from vga compatibility 
    //  mode into native virtio mode."
    //
    // The KEY insight: Calling SET_SCANOUT DISABLES automatic display from BAR 0!
    // In VGA mode (before SET_SCANOUT): Writing to BAR 0 → automatic display (like QXL)
    // In VirtIO mode (after SET_SCANOUT): Writing to BAR 0 → NOTHING (must call TRANSFER_TO_HOST_2D)
    //
    // Auto-detect device type from VMVirtIOGPU properties
    OSString* deviceType = NULL;
    bool hasVGACompatibility = false;
    bool requiresNativeMode = false;
    
    if (m_gpu_driver) {
        // Try to get properties from the GPU driver object
        deviceType = OSDynamicCast(OSString, m_gpu_driver->getProperty("VMVirtIODeviceType"));
        OSBoolean* vgaCompat = OSDynamicCast(OSBoolean, m_gpu_driver->getProperty("VirtIO-VGA-Compatibility"));
        
        // If not found on driver, try the PCI provider
        if (!deviceType && m_pci_device) {
            IOLog("VMVirtIOFramebuffer::enableController() - Checking PCI device properties\n");
            deviceType = OSDynamicCast(OSString, m_pci_device->getProperty("VMVirtIODeviceType"));
            vgaCompat = OSDynamicCast(OSBoolean, m_pci_device->getProperty("VirtIO-VGA-Compatibility"));
        }
        
        hasVGACompatibility = (vgaCompat && vgaCompat->isTrue());
        
        if (deviceType) {
            const char* typeStr = deviceType->getCStringNoCopy();
            IOLog("VMVirtIOFramebuffer::enableController() - Device type: %s\n", typeStr);
            requiresNativeMode = (strcmp(typeStr, "virtio-gpu-gl-pci") == 0);
        } else {
            // Fallback: check PCI class code directly
            if (m_pci_device) {
                UInt32 classCode = m_pci_device->configRead32(kIOPCIConfigClassCode);
                UInt8 baseClass = (classCode >> 24) & 0xFF;
                UInt8 subClass = (classCode >> 16) & 0xFF;
                
                IOLog("VMVirtIOFramebuffer::enableController() - PCI class: 0x%02x:0x%02x\n", baseClass, subClass);
                
                // 0x03:0x00 = VGA-compatible (virtio-vga-gl)
                // 0x03:0x80 = Other display controller (virtio-gpu-gl-pci)
                if (baseClass == 0x03 && subClass == 0x00) {
                    hasVGACompatibility = true;
                    requiresNativeMode = false;
                    IOLog("VMVirtIOFramebuffer::enableController() - Detected VGA-compatible device (virtio-vga-gl)\n");
                } else if (baseClass == 0x03 && subClass == 0x80) {
                    hasVGACompatibility = false;
                    requiresNativeMode = true;
                    IOLog("VMVirtIOFramebuffer::enableController() - Detected pure GPU device (virtio-gpu-gl-pci)\n");
                }
            }
        }
    }
    
    // Decision logic:
    // Native scanout is the rendering path for all virtio-gpu variants once
    // this driver owns the device. VGA compatibility only matters before a
    // driver loads (BIOS/boot); once we're here, the virtio-gpu protocol
    // (resource + scanout + transfer) is the only path that puts pixels on
    // screen. Keying this on VGA compat skips the entire pipeline — that was
    // the virtio-vga-gl blue-screen bug.
    bool useNativeScanout = (m_gpu_driver != nullptr);
    
    IOLog("VMVirtIOFramebuffer::enableController() - hasVGACompat=%d, requiresNative=%d, useNative=%d\n",
          hasVGACompatibility, requiresNativeMode, useNativeScanout);
    
    // Enable VirtIO GPU scanout if needed
    if (useNativeScanout && m_gpu_driver) {
        IOLog("VMVirtIOFramebuffer::enableController() - VirtIO GPU native scanout ENABLED\n");
        
        // Set up display with current resolution
        IODisplayModeInformation modeInfo;
        IOReturn modeResult = getInformationForDisplayMode(m_current_mode, &modeInfo);
        if (modeResult == kIOReturnSuccess) {
            m_width = modeInfo.nominalWidth;
            m_height = modeInfo.nominalHeight;
            m_depth = 32;
            
            IOLog("VMVirtIOFramebuffer::enableController() - Setting up VirtIO display: %dx%d@%d\n", 
                  m_width, m_height, m_depth);
            
            // NEGATIVE CONTROL: SET_SCANOUT with invalid resource_id=999
            // Prediction: device returns 0x1203 (ERR_INVALID_RESOURCE_ID)
            // If this returns 0x1100 (OK), the response path is still broken.
            {
                IOReturn test_ret = m_gpu_driver->setscanout(0, 999, 0, 0, m_width, m_height);
                IOLog("VMVirtIOFramebuffer: NEGATIVE CONTROL — setscanout(999) returned 0x%x "
                      "(expect error, not success)\n", test_ret);
            }

            // One-shot resource-tracking self-check: prove findResource actually
            // finds after pool unification (was a no-op for months because it
            // indexed an empty m_resources OSArray). Same shape as the
            // setscanout(999) probe above — deterministic, self-checking.
            // Runs before the first real createResource2D so the device is up.
            {
                static bool resource_tracking_probed = false;
                if (!resource_tracking_probed && m_gpu_driver) {
                    resource_tracking_probed = true;
                    m_gpu_driver->probeResourceTracking();
                }
            }

            // Allocate framebuffer backing, create VirtIO GPU resource, attach
            // backing (caller-owned, scatter-list), set scanout. Single helper
            // so mode changes can re-invoke the same path.
            IOReturn setup_ret = setupFramebufferResource(m_width, m_height);
            if (setup_ret == kIOReturnSuccess) {
                IOLog("VMVirtIOFramebuffer::enableController() - VirtIO GPU display resource ready (%ux%u)\n",
                      m_width, m_height);

                // Phase 3 probe: verify the resource-recreate path works. Runs once.
                // The recreate path was structurally unreachable before (the guard
                // skipped it); this probe exercises it deterministically without
                // waiting for a Phase 4 mode switch. The probe re-calls
                // setupFramebufferResource at a smaller dim, then restores — buffer
                // must stay stable across both recreates.
                {
                    static bool recreate_probed = false;
                    if (!recreate_probed) {
                        recreate_probed = true;
                        probeResourceRecreate();
                    }
                }

                // Cursor queue transport probe (build 1): creates a test cursor
                // resource, sends UPDATE_CURSOR + MOVE_CURSOR on queue 1. Pass:
                // two cursors on screen (red test cursor + software cursor).
                {
                    static bool cursor_probed = false;
                    if (!cursor_probed && m_gpu_driver) {
                        cursor_probed = true;
                        m_gpu_driver->probeCursorTransport();
                    }
                }

                // 3D transport probe (build 2): CTX_CREATE → RESOURCE_CREATE_3D
                // → ATTACH_BACKING → CREATE_OBJECT(surface) → CLEAR →
                // TRANSFER_FROM_HOST_3D → byte-equal positive + negative
                // control. Gate for all 3D/virgl work. Per CLAUDE.md: only the
                // byte readback is a real signal — SUBMIT_3D returns 0x1100
                // unconditionally, so phases F+ prove buffer-acceptance only.
                {
                    static bool transport3d_probed = false;
                    if (!transport3d_probed && m_gpu_driver) {
                        transport3d_probed = true;
                        m_gpu_driver->probeTransport3D();
                    }
                }

                // Initial transfer so the screen isn't garbage while WindowServer composes.
                m_gpu_driver->transferToHost2D(m_fb_resource_id, 0, 0, 0, m_width, m_height);
                m_gpu_driver->flushResource(m_fb_resource_id, 0, 0, m_width, m_height);
            } else {
                IOLog("VMVirtIOFramebuffer::enableController() - setupFramebufferResource failed: 0x%x\n",
                      setup_ret);
            }
        } else {
            IOLog("VMVirtIOFramebuffer::enableController() - Failed to get mode info: 0x%x\n", modeResult);
        }
    } else {
        // Not using native scanout
        if (requiresNativeMode) {
            IOLog("VMVirtIOFramebuffer::enableController() - ⚠️  CRITICAL: virtio-gpu-gl-pci requires native scanout!\n");
            IOLog("VMVirtIOFramebuffer::enableController() - No VGA fallback available, display will NOT work\n");
        } else if (hasVGACompatibility) {
            IOLog("VMVirtIOFramebuffer::enableController() - Using VGA compatibility mode (virtio-vga-gl)\n");
            IOLog("VMVirtIOFramebuffer::enableController() - BAR0 framebuffer writes will auto-display\n");
        } else {
            IOLog("VMVirtIOFramebuffer::enableController() - ⚠️  Unknown mode - no GPU driver or display setup\n");
        }
    }
    
    // STEP 2: Software display activation through IOFramebuffer mechanisms
    IOLog("VMVirtIOFramebuffer::enableController() - Software display output ready\n");
    
    // *** CRITICAL: Only enable refresh timer in native VirtIO scanout mode ***
    // In VGA compatibility mode, BAR0 writes automatically appear on screen
    // The timer would interfere by calling transferToHost2D which is not needed in VGA mode
    if (useNativeScanout && m_refresh_timer && m_gpu_driver) {
        m_refresh_timer->setTimeoutMS(REFRESH_PERIOD_MS);
        IOLog("VMVirtIOFramebuffer::enableController() - Display refresh timer enabled: period %u ms (native VirtIO mode)\n",
              (unsigned)REFRESH_PERIOD_MS);
        IOLog("VMVirtIOFramebuffer::enableController() - Timer will transfer framebuffer updates to VirtIO GPU\n");
    } else if (!useNativeScanout) {
        IOLog("VMVirtIOFramebuffer::enableController() - Timer DISABLED (VGA compatibility mode - not needed)\n");
        IOLog("VMVirtIOFramebuffer::enableController() - BAR0 writes will auto-display via VGA BIOS\n");
    } else {
        IOLog("VMVirtIOFramebuffer::enableController() - WARNING: Timer or GPU driver not available\n");
    }
    
    IOLog("VMVirtIOFramebuffer::enableController() - Controller enabled successfully\n");
    
    return kIOReturnSuccess;
}
// Note: enableController() method removed to use IOFramebuffer's default implementation
// This allows proper console-to-GUI transition like QXL devices

IOReturn VMVirtIOFramebuffer::setDisplayMode(IODisplayModeID displayMode, IOIndex depth)
{
    IOLog("VMVirtIOFramebuffer::setDisplayMode() - mode=%d, depth=%d\n", (int)displayMode, (int)depth);
    
    // Phase 4: filterModesByAllocation can produce a non-contiguous ID list
    // (middle modes filtered out when allocation falls back). Validate by
    // membership in m_display_modes, not by range — `displayMode > m_mode_count`
    // would accept filtered-out IDs in the gap and reject valid ones past it.
    bool mode_valid = false;
    for (IOItemCount i = 0; i < m_mode_count; i++) {
        if (m_display_modes[i] == displayMode) {
            mode_valid = true;
            break;
        }
    }
    if (!mode_valid) {
        IOLog("VMVirtIOFramebuffer::setDisplayMode() - Invalid mode %d (not in advertised list of %u)\n",
              (int)displayMode, (unsigned)m_mode_count);
        return kIOReturnUnsupported;
    }
    
    m_current_mode = displayMode;
    
    // Update width/height based on mode
    IODisplayModeInformation modeInfo;
    IOReturn result = getInformationForDisplayMode(displayMode, &modeInfo);
    if (result == kIOReturnSuccess) {
        uint32_t new_w = modeInfo.nominalWidth;
        uint32_t new_h = modeInfo.nominalHeight;

        // Re-allocate framebuffer backing if dimensions changed. setupFramebufferResource
        // tears down the old resource/backing, allocates new, re-creates, re-attaches,
        // re-scans-out. Safe to call on first mode set too (m_fb_resource_id starts 0).
        if (new_w != m_width || new_h != m_height || m_fb_resource_id == 0) {
            IOReturn setup_ret = setupFramebufferResource(new_w, new_h);
            if (setup_ret != kIOReturnSuccess) {
                IOLog("VMVirtIOFramebuffer::setDisplayMode() - setupFramebufferResource failed: 0x%x\n",
                      setup_ret);
                return setup_ret;
            }
        }
        m_depth = 32;

        IOLog("VMVirtIOFramebuffer::setDisplayMode() - Set resolution to %dx%d@%d\n",
              m_width, m_height, m_depth);

        // Trigger display refresh to update VirtIO GPU with new content
        IOLog("VMVirtIOFramebuffer::setDisplayMode() - Refreshing VirtIO GPU display\n");
        refreshDisplay();

        IOSleep(50); // Small delay for mode change stabilization
    } else {
        IOLog("VMVirtIOFramebuffer::setDisplayMode() - Failed to get mode information\n");
    }

    // super::setDisplayMode bypassed: originally removed for an AHCI/workloop
    // race that occurred under TCG. Investigation 2026-08-09 (cursor fix
    // thread) showed it is NOT load-bearing for any currently-observed bug:
    // WindowServer refreshes `__private->pixelInfo` independently via
    // setupForCurrentConfig → doSetup → getPixelInformation after every
    // setDisplayMode, so the cache doesn't strand. Leaving the bypass in
    // place; re-enabling is at most a separate base-class-integration
    // question, not a fix for anything broken today.
    IOLog("VMVirtIOFramebuffer::setDisplayMode: complete — mode=%u live m_width=%u m_height=%u\n",
          (unsigned)m_current_mode, (unsigned)m_width, (unsigned)m_height);
    return kIOReturnSuccess;
}

IOReturn VMVirtIOFramebuffer::setupForCurrentConfig()
{
    IOLog("VMVirtIOFramebuffer::setupForCurrentConfig() - delegating to IOFramebuffer base class\n");

    // The base class's setupForCurrentConfig calls doSetup, which calls
    // getApertureRange(kIOFBSystemAperture) and establishes the client mapping
    // that WindowServer uses to draw. Our previous override skipped this entirely
    // — those "FORCING GUI MODE" workarounds prevented the base class from
    // fetching the aperture, which is why WindowServer had nowhere to render
    // and the test pattern persisted indefinitely.
    return super::setupForCurrentConfig();
}

IOItemCount VMVirtIOFramebuffer::getConnectionCount(void)
{
    return 1; // Single display connection
}

IOReturn VMVirtIOFramebuffer::getDisplayStatus(void* connectFlags)
{
    // CRITICAL: Tell IOGraphicsFamily that a display is connected
    // Without this, macOS won't activate the framebuffer
    if (connectFlags) {
        // 1 = display connected, 0 = no display
        *(IOOptionBits*)connectFlags = 1;
    }
    IOLog("VMVirtIOFramebuffer::getDisplayStatus() - Display connected\n");
    return kIOReturnSuccess;
}

bool VMVirtIOFramebuffer::isConsoleDevice(void)
{
    IOLog("VMVirtIOFramebuffer::isConsoleDevice() - QXL-STYLE CONSOLE DEVICE SUPPORT\n");
    
    // Like QXL: Always claim to be a console device, but support both console and GUI modes
    // This allows proper console boot and GUI transitions
    
    // DISABLED: Accelerator properties cause WindowServer crashes on Catalina
    // vm-cap3d flip experiment: OVERWRITE site (runs after start()); carries
    // the same publication gate so the flip survives. Ordinary boots unchanged.
    setProperty("IODisplayAccelerated",
                VMVirtIOGPU::cap3dPublishGate() ? kOSBooleanTrue : kOSBooleanFalse);
    setProperty("IOGraphicsAccelerator",
                VMVirtIOGPU::cap3dPublishGate() ? kOSBooleanTrue : kOSBooleanFalse);
    setProperty("IOConsoleDevice", kOSBooleanTrue);        // Always console capable
    setProperty("IOPrimaryDisplay", kOSBooleanTrue);       // Primary display
    setProperty("IOMatchCategory", "IOFramebuffer");
    // REMOVED: IOGLBundleName triggers WindowServer to try using OpenGL/Metal
    setProperty("IOAcceleratorFamily",
                VMVirtIOGPU::cap3dPublishGate() ? kOSBooleanTrue : kOSBooleanFalse);   // DISABLED: Causes WindowServer crashes
    
    // DISABLE AGDC properties - tell WindowServer we DON'T support AGDC (d57 fix)
    setProperty("AGDC", kOSBooleanFalse);
    setProperty("AGDCCapable", kOSBooleanFalse);
    
    IOLog("VMVirtIOFramebuffer::isConsoleDevice() - Console device with GUI capability (like QXL)\n");
    return true;  // Always claim console support - GUI will work through transitions
}

IOReturn VMVirtIOFramebuffer::setPowerState(unsigned long powerStateOrdinal, IOService* whatDevice)
{
    IOLog("VMVirtIOFramebuffer::setPowerState() - state=%lu\n", powerStateOrdinal);
    return kIOReturnSuccess;
}

// CRITICAL: Implement getAttributeForConnection to make display appear "online"
// This is essential for display activation - without this, macOS considers display offline
IOReturn VMVirtIOFramebuffer::getAttributeForConnection(IOIndex connectIndex, IOSelect attribute, uintptr_t* value)
{
    IOLog("VMVirtIOFramebuffer::getAttributeForConnection() - connectIndex=%d attribute=0x%x\n", 
          (int)connectIndex, (unsigned int)attribute);
    
    // Decode attribute for easier debugging
    char attrStr[5] = {0};
    attrStr[0] = (attribute >> 24) & 0xFF;
    attrStr[1] = (attribute >> 16) & 0xFF;
    attrStr[2] = (attribute >> 8) & 0xFF;
    attrStr[3] = attribute & 0xFF;
    IOLog("VMVirtIOFramebuffer::getAttributeForConnection() - Attribute '%s' (0x%x)\n", attrStr, (unsigned int)attribute);
    
    // Handle NULL value pointers - these are capability checks for specific attributes
    if (!value) {
        IOLog("VMVirtIOFramebuffer::getAttributeForConnection() - NULL value pointer\n");

        // CRITICAL: Handle capability checks for display pipeline attributes
        switch (attribute) {
            case kConnectionSupportsHLDDCSense: // 'hddc' - High Definition Display Controller
                IOLog("VMVirtIOFramebuffer::getAttributeForConnection() - HDDC capability check: SUPPORTED (getDDCBlock live)\n");
                return kIOReturnSuccess; // BACKED: getDDCBlock() implemented below

            case 0x6c646463: // 'lddc' - Low Definition Display Controller
                IOLog("VMVirtIOFramebuffer::getAttributeForConnection() - LDDC capability check: NOT SUPPORTED (use HDDC)\n");
                return kIOReturnUnsupported; // LOW-level DDC not implemented — HDDC path is
                
            case kConnectionSupportsAppleSense: // 'asns' - Apple Sense
                IOLog("VMVirtIOFramebuffer::getAttributeForConnection() - Apple Sense capability check: SUPPORTED\n");
                return kIOReturnSuccess; // We support Apple Sense
                
            default:
                IOLog("VMVirtIOFramebuffer::getAttributeForConnection() - Unknown capability check for 0x%x\n", (unsigned int)attribute);
                return kIOReturnBadArgument;
        }
    }
    
    // Only support connection 0 (primary display)
    if (connectIndex != 0) {
        return kIOReturnBadArgument;
    }
    
    switch (attribute) {
        case kConnectionFlags:
            // Connection flags - mark as built-in DDC-capable display
            // Use kBuiltInConnection (11) and kHasDDCConnection (8) for proper detection
            *value = (1 << kBuiltInConnection) | (1 << kHasDDCConnection) | (1 << kReportsHotPlugging);
            IOLog("VMVirtIOFramebuffer::getAttributeForConnection() - kConnectionFlags: 0x%lx\n", *value);
            return kIOReturnSuccess;
            
        case kConnectionCheckEnable:
        case kConnectionEnable:
            // CRITICAL: This is what determines if display is "online"
            // Return true to indicate display is connected and active
            *value = 1; // Display is online and enabled
            IOLog("VMVirtIOFramebuffer::getAttributeForConnection() - kConnectionEnable: Display ONLINE\n");
            return kIOReturnSuccess;
            
        case kConnectionSyncFlags:
            // Sync signal flags - indicate all sync signals are active
            *value = 0xFF; // All sync signals active
            IOLog("VMVirtIOFramebuffer::getAttributeForConnection() - kConnectionSyncFlags: 0x%lx\n", *value);
            return kIOReturnSuccess;
            
        case kConnectionSyncEnable:
            // Sync enable capabilities
            *value = 0xFF; // All sync controls available
            IOLog("VMVirtIOFramebuffer::getAttributeForConnection() - kConnectionSyncEnable: 0x%lx\n", *value);
            return kIOReturnSuccess;
            
        case kConnectionSupportsHLDDCSense:
            // CRITICAL: HDDC support for display pipeline
            *value = 1; // ENABLE HDDC for display pipeline support
            IOLog("VMVirtIOFramebuffer::getAttributeForConnection() - HDDC support: ENABLED for display pipeline\n");
            return kIOReturnSuccess;
            
        case 0x6c646463: // 'lddc' - Low Definition Display Controller
            // CRITICAL: LDDC support for display pipeline (counterpart to HDDC)
            *value = 1; // ENABLE LDDC for complete display pipeline support
            IOLog("VMVirtIOFramebuffer::getAttributeForConnection() - LDDC support: ENABLED for display pipeline\n");
            return kIOReturnSuccess;
            
        case kConnectionSupportsAppleSense:
            // Apple Sense support for display detection
            *value = 1; // ENABLE Apple Sense for proper display enumeration
            IOLog("VMVirtIOFramebuffer::getAttributeForConnection() - Apple Sense: ENABLED for display detection\n");
            return kIOReturnSuccess;
            
        case kConnectionPostWake:
            // Post-wake processing
            IOLog("VMVirtIOFramebuffer::getAttributeForConnection() - kConnectionPostWake: success\n");
            return kIOReturnSuccess;
            
        case 0x7061726d: // 'parm' - kConnectionDisplayParameters
            // Let IOFramebuffer handle display parameters
            IOLog("VMVirtIOFramebuffer::getAttributeForConnection() - kConnectionDisplayParameters: delegating to parent\n");
            return super::getAttributeForConnection(connectIndex, attribute, value);
            
        case 0x70636e74: // 'pcnt' - kConnectionDisplayParameterCount
            // Return error to indicate no display parameters available
            // This should break the infinite loop by telling the system there are no parameters
            IOLog("VMVirtIOFramebuffer::getAttributeForConnection() - kConnectionDisplayParameterCount: returning kIOReturnUnsupported to break loop\n");
            return kIOReturnUnsupported;
            
        case 0x72677363: // 'rgsc' - kConnectionRedGammaScale
            // Red gamma scale
            *value = 0x10000; // 1.0 in 16.16 fixed point
            IOLog("VMVirtIOFramebuffer::getAttributeForConnection() - kConnectionRedGammaScale: 0x%lx\n", *value);
            return kIOReturnSuccess;
            
        case 0x67677363: // 'ggsc' - kConnectionGreenGammaScale
            // Green gamma scale  
            *value = 0x10000; // 1.0 in 16.16 fixed point
            IOLog("VMVirtIOFramebuffer::getAttributeForConnection() - kConnectionGreenGammaScale: 0x%lx\n", *value);
            return kIOReturnSuccess;
            
        case 0x62677363: // 'bgsc' - kConnectionBlueGammaScale
            // Blue gamma scale
            *value = 0x10000; // 1.0 in 16.16 fixed point
            IOLog("VMVirtIOFramebuffer::getAttributeForConnection() - kConnectionBlueGammaScale: 0x%lx\n", *value);
            return kIOReturnSuccess;
            
        case 0x76626c6d: // 'vblm' - vertical blanking management
            // Vertical blanking interval
            *value = 0x10000; // Standard VBL value
            IOLog("VMVirtIOFramebuffer::getAttributeForConnection() - Vertical blanking (vblm): 0x%lx\n", *value);
            return kIOReturnSuccess;
            
        case 0xdeadbeef: // Debug/test attribute
            // This appears to be a system test or debug call
            *value = 1; // Return success/enabled
            IOLog("VMVirtIOFramebuffer::getAttributeForConnection() - Debug attribute (deadbeef): enabled\n");
            return kIOReturnSuccess;
            
        case 0x40052e7: // Unknown system attribute
            // System is calling this specific attribute
            *value = 1; // Return success/enabled
            IOLog("VMVirtIOFramebuffer::getAttributeForConnection() - System attribute (0x40052e7): enabled\n");
            return kIOReturnSuccess;
            
        case 0x7102bb07: // Another system attribute
            // System is calling this attribute too
            *value = 1; // Return success/enabled
            IOLog("VMVirtIOFramebuffer::getAttributeForConnection() - System attribute (0x7102bb07): enabled\n");
            return kIOReturnSuccess;
            
        case kConnectionChanged:
            // Connection change detection
            IOLog("VMVirtIOFramebuffer::getAttributeForConnection() - kConnectionChanged: no change\n");
            return kIOReturnSuccess;
            
        // Standard display parameters (indices 0, 1, 2)
        case 0x70726d30: // 'prm0' - Display parameter 0 (brightness)
            *value = 0x8000; // Mid-level brightness (50% in 16.16 fixed point)
            IOLog("VMVirtIOFramebuffer::getAttributeForConnection() - Display parameter 0 (brightness): 0x%lx\n", *value);
            return kIOReturnSuccess;
            
        case 0x70726d31: // 'prm1' - Display parameter 1 (contrast)
            *value = 0x8000; // Mid-level contrast (50% in 16.16 fixed point)
            IOLog("VMVirtIOFramebuffer::getAttributeForConnection() - Display parameter 1 (contrast): 0x%lx\n", *value);
            return kIOReturnSuccess;
            
        case 0x70726d32: // 'prm2' - Display parameter 2 (gamma)
            *value = 0x10000; // Standard gamma (1.0 in 16.16 fixed point)
            IOLog("VMVirtIOFramebuffer::getAttributeForConnection() - Display parameter 2 (gamma): 0x%lx\n", *value);
            return kIOReturnSuccess;
            
        // CRITICAL: Display pipe identification attributes for PRIMARY framebuffer
        // WindowServer requires pipe index 0 for primary display
        case 0x70697065: // 'pipe' - Display pipe index
            *value = 0; // Pipe index 0 (PRIMARY display) - required for WindowServer
            IOLog("VMVirtIOFramebuffer::getAttributeForConnection() - Display pipe index: 0 (PRIMARY)\n");
            return kIOReturnSuccess;
            
        case 0x64706974: // 'dpit' - Display pipe type
            *value = 1; // Primary display pipe type
            IOLog("VMVirtIOFramebuffer::getAttributeForConnection() - Display pipe type: 1 (PRIMARY)\n");
            return kIOReturnSuccess;
            
        case 0x64706964: // 'dpid' - Display pipe ID
            *value = 0x1000; // Primary display pipe ID
            IOLog("VMVirtIOFramebuffer::getAttributeForConnection() - Display pipe ID: 0x1000 (PRIMARY)\n");
            return kIOReturnSuccess;
            
        case 0x636e7472: // 'cntr' - Connection type/controller
            *value = 0x1AF4; // VirtIO vendor ID as controller type
            IOLog("VMVirtIOFramebuffer::getAttributeForConnection() - Connection controller: VirtIO (0x1AF4)\n");
            return kIOReturnSuccess;
            
        default:
            IOLog("VMVirtIOFramebuffer::getAttributeForConnection() - Unknown attribute 0x%x\n", (unsigned int)attribute);
            return super::getAttributeForConnection(connectIndex, attribute, value);
    }
}

IOReturn VMVirtIOFramebuffer::setAttributeForConnection(IOIndex connectIndex, IOSelect attribute, uintptr_t value)
{
    IOLog("VMVirtIOFramebuffer::setAttributeForConnection() - connectIndex=%d, attribute=0x%x, value=0x%lx\n", 
          (int)connectIndex, (unsigned int)attribute, value);
    
    if (connectIndex != 0) {
        IOLog("VMVirtIOFramebuffer::setAttributeForConnection() - Invalid connection index %d\n", (int)connectIndex);
        return kIOReturnBadArgument;
    }
    
    switch (attribute) {
        case kConnectionEnable:
            // Connection enable/disable
            IOLog("VMVirtIOFramebuffer::setAttributeForConnection() - kConnectionEnable: %s\n", 
                  value ? "ENABLE" : "DISABLE");
            // For VirtIO GPU, we're always enabled
            return kIOReturnSuccess;
            
        case kConnectionSyncEnable:
            // Sync enable/disable
            IOLog("VMVirtIOFramebuffer::setAttributeForConnection() - kConnectionSyncEnable: 0x%lx\n", value);
            return kIOReturnSuccess;
            
        case kConnectionPower:
            // Power management
            IOLog("VMVirtIOFramebuffer::setAttributeForConnection() - kConnectionPower: 0x%lx\n", value);
            return kIOReturnSuccess;
            
        case kConnectionPostWake:
            // Post-wake setup
            IOLog("VMVirtIOFramebuffer::setAttributeForConnection() - kConnectionPostWake\n");
            return kIOReturnSuccess;
            
        case 0x72677363: // 'rgsc' - kConnectionRedGammaScale
            // Red gamma scale
            IOLog("VMVirtIOFramebuffer::setAttributeForConnection() - kConnectionRedGammaScale: 0x%lx\n", value);
            return kIOReturnSuccess;
            
        case 0x67677363: // 'ggsc' - kConnectionGreenGammaScale
            // Green gamma scale
            IOLog("VMVirtIOFramebuffer::setAttributeForConnection() - kConnectionGreenGammaScale: 0x%lx\n", value);
            return kIOReturnSuccess;
            
        case 0x62677363: // 'bgsc' - kConnectionBlueGammaScale
            // Blue gamma scale
            IOLog("VMVirtIOFramebuffer::setAttributeForConnection() - kConnectionBlueGammaScale: 0x%lx\n", value);
            return kIOReturnSuccess;
            
        case kConnectionGammaScale:
            // Overall gamma scale
            IOLog("VMVirtIOFramebuffer::setAttributeForConnection() - kConnectionGammaScale: 0x%lx\n", value);
            return kIOReturnSuccess;
            
        case 0x76626c6d: // 'vblm' - vertical blanking management
            IOLog("VMVirtIOFramebuffer::setAttributeForConnection() - Vertical blanking (vblm): 0x%lx\n", value);
            return kIOReturnSuccess;
            
        case 0x666c7573: // 'flus' - flush
            IOLog("VMVirtIOFramebuffer::setAttributeForConnection() - Flush (flus): 0x%lx\n", value);
            return kIOReturnSuccess;
            
        case 0xdeadbeef: // Debug/test attribute
            IOLog("VMVirtIOFramebuffer::setAttributeForConnection() - Debug attribute (deadbeef): 0x%lx\n", value);
            return kIOReturnSuccess;
            
        default:
            IOLog("VMVirtIOFramebuffer::setAttributeForConnection() - Unknown attribute 0x%x\n", (unsigned int)attribute);
            return super::setAttributeForConnection(connectIndex, attribute, value);
    }
}

/* DDC/EDID — the hypothesis test (2026-08-22): the display had no
 * EDID (the HDDC claim was unbacked); CGS produced "invalid display"
 * for accelerated formats. This EDID is a minimal valid 128-byte 1.3
 * base block: digital, 1680x1050@60Hz, checksum-verified (sum=0).
 * HYPOTHESIS: CGS reads EDID for accelerated-display qualification.
 * If correct, the "invalid display" errors vanish and accelerated pf
 * requests reach the GLD. */
static const UInt8 s_edid[128] = {
    0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00,
    0x44, 0x8A, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00,
    0x00, 0x22, 0x01, 0x03, 0x83, 0x2C, 0x1B, 0x78,
    0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x7C, 0x2E,
    0x90, 0xB0, 0x60, 0x1A, 0x20, 0x40, 0x68, 0x08,
    0x36, 0x00, 0xBB, 0x0F, 0x11, 0x00, 0x00, 0x1E,
    0x00, 0x00, 0x00, 0xFC, 0x00, 0x56, 0x4D, 0x56,
    0x69, 0x72, 0x74, 0x49, 0x4F, 0x20, 0x44, 0x69,
    0x73, 0x0A, 0x00, 0x00, 0x00, 0xFD, 0x00, 0x32,
    0x4B, 0x1E, 0xA0, 0xFF, 0x20, 0x20, 0x20, 0x20,
    0x20, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF8,
};

IOReturn VMVirtIOFramebuffer::getDDCBlock(IOIndex connectIndex, UInt32 blockNumber,
                                           IOSelect blockType, IOOptionBits options,
                                           UInt8* data, IOByteCount* length)
{
    static bool s_logged = false;
    if (!s_logged) {
        s_logged = true;
        IOLog("VMVirtIOFramebuffer::getDDCBlock — SERVING EDID (128 bytes, "
              "digital 1680x1050, hypothesis test 2026-08-22)\n");
    }
    if (connectIndex == 0 &&
        blockNumber == 1 &&
        blockType == kIODDCBlockTypeEDID &&
        data && length && *length >= 128) {
        memcpy(data, s_edid, 128);
        *length = 128;
        return kIOReturnSuccess;
    }
    return super::getDDCBlock(connectIndex, blockNumber, blockType, options, data, length);
}

bool VMVirtIOFramebuffer::hasDDCConnect(IOIndex connectIndex)
{
    return (connectIndex == 0);
}

IOReturn VMVirtIOFramebuffer::connectFlags(IOIndex connectIndex, IODisplayModeID displayMode, IOOptionBits* flags)
{
    IOLog("VMVirtIOFramebuffer::connectFlags() - connectIndex=%d, displayMode=%d\n", (int)connectIndex, (int)displayMode);
    
    if (connectIndex != 0) {
        IOLog("VMVirtIOFramebuffer::connectFlags() - Invalid connection index %d\n", (int)connectIndex);
        return kIOReturnBadArgument;
    }
    
    if (!flags) {
        IOLog("VMVirtIOFramebuffer::connectFlags() - NULL flags pointer\n");
        return kIOReturnBadArgument;
    }
    
    // For VirtIO GPU, all our supported modes are valid and safe
    // This tells the system that this connection supports the requested display mode
    *flags = kDisplayModeValidFlag | kDisplayModeSafeFlag;
    
    IOLog("VMVirtIOFramebuffer::connectFlags() - Mode %d is valid and safe (flags=0x%x)\n", 
          (int)displayMode, (unsigned int)*flags);
    
    return kIOReturnSuccess;
}

// NOTE: newUserClient override removed - IOFramebuffer::newUserClient is NOT virtual in Snow Leopard
// Cannot override non-virtual methods. QXL doesn't override it and works fine.
// The system will automatically use IOUserClientClass from Info.plist when needed.
// The crash at IOGraphicsFamily + 77347 happens because IOFramebuffer::newUserClient
// expects internal state that we don't initialize. Need to investigate what state is missing.

// CRITICAL: Cursor support methods (required for GUI mode)
IOReturn VMVirtIOFramebuffer::setCursorImage(void* cursorImage)
{
    // Hardware cursor not implemented — return unsupported so WindowServer
    // falls back to software cursor (composited into framebuffer).
    // END STATE: implement virtio-gpu cursor queue and return success here.
    return kIOReturnUnsupported;
}

IOReturn VMVirtIOFramebuffer::setCursorState(SInt32 x, SInt32 y, bool visible)
{
    // Unreachable by construction. We report crsr = 0 (no hardware cursor),
    // so on 10.6 WindowServer composites the cursor into the aperture from
    // userspace via CoreGraphics — the kernel never participates. Every
    // shmem cursor field (cursorLoc, cursorSize, cursorRect, oldCursorRect)
    // is frozen at the boot-console state near (15,15) for the same reason:
    // nothing in-kernel ever updates them.
    //
    // This method would only be called if we advertised a hardware cursor
    // (crsr = 1) — gated on the UTM GL cursor-compositing question. The
    // real cursor-responsiveness fix is host-composited hardware cursor,
    // not anything in this driver.
    return kIOReturnUnsupported;
}

// CRITICAL: VBL interrupt support (required for smooth GUI rendering)
IOReturn VMVirtIOFramebuffer::registerForInterruptType(IOSelect interruptType, 
                                                       IOFBInterruptProc proc, OSObject* target, void* ref,
                                                       void** interruptRef)
{
    IOLog("VMVirtIOFramebuffer::registerForInterruptType() - Type: 0x%x\n", (unsigned int)interruptType);
    
    // Decode interrupt type for debugging
    char typeStr[5] = {0};
    typeStr[0] = (interruptType >> 24) & 0xFF;
    typeStr[1] = (interruptType >> 16) & 0xFF;
    typeStr[2] = (interruptType >> 8) & 0xFF;
    typeStr[3] = interruptType & 0xFF;
    IOLog("VMVirtIOFramebuffer::registerForInterruptType() - Type string: '%s'\n", typeStr);
    
    // Support all VBL and display-related interrupt types
    switch (interruptType) {
        case 0:                     // Standard VBL interrupt
        case 0x76626c20:           // 'vbl ' - VBL interrupt 
        case 0x76626c6e:           // 'vbln' - VBL notification
        case 0x64636920:           // 'dci ' - Display change interrupt
        case 0x64706972:           // 'dpir' - Display pipe interrupt  
        case 0x68646369:           // 'hdci' - Hot display change interrupt
            IOLog("VMVirtIOFramebuffer::registerForInterruptType() - %s interrupt SUPPORTED for GUI\n", typeStr);
            
            if (interruptRef) {
                *interruptRef = (void*)((uintptr_t)interruptType | 0x12340000); // Unique reference per type
            }
            
            return kIOReturnSuccess;
            
        default:
            IOLog("VMVirtIOFramebuffer::registerForInterruptType() - Unsupported interrupt type: 0x%x ('%s')\n", 
                  (unsigned int)interruptType, typeStr);
            return kIOReturnUnsupported;
    }
}

IOReturn VMVirtIOFramebuffer::unregisterInterrupt(void* interruptRef)
{
    IOLog("VMVirtIOFramebuffer::unregisterInterrupt() - Unregistering interrupt ref: %p\n", interruptRef);
    
    uintptr_t refValue = (uintptr_t)interruptRef;
    if ((refValue & 0xFFFF0000) == 0x12340000) {
        uint32_t interruptType = refValue & 0xFFFF;
        IOLog("VMVirtIOFramebuffer::unregisterInterrupt() - Display interrupt type 0x%x unregistered\n", interruptType);
        return kIOReturnSuccess;
    }
    
    return kIOReturnBadArgument;
}

IOReturn VMVirtIOFramebuffer::setInterruptState(void* interruptRef, UInt32 state)
{
    IOLog("VMVirtIOFramebuffer::setInterruptState() - Ref: %p, State: %d\n", interruptRef, (int)state);
    
    uintptr_t refValue = (uintptr_t)interruptRef;
    if ((refValue & 0xFFFF0000) == 0x12340000) {
        uint32_t interruptType = refValue & 0xFFFF;
        
        // Decode interrupt type for logging
        char typeStr[5] = {0};
        typeStr[0] = (interruptType >> 24) & 0xFF;
        typeStr[1] = (interruptType >> 16) & 0xFF;
        typeStr[2] = (interruptType >> 8) & 0xFF;
        typeStr[3] = interruptType & 0xFF;
        
        if (state) {
            IOLog("VMVirtIOFramebuffer::setInterruptState() - %s interrupts ENABLED for GUI rendering\n", typeStr);
        } else {
            IOLog("VMVirtIOFramebuffer::setInterruptState() - %s interrupts DISABLED\n", typeStr);
        }
        return kIOReturnSuccess;
    }
    
    return kIOReturnBadArgument;
}

// AGDC Service Management

IOReturn VMVirtIOFramebuffer::createAGDCService()
{
    IOLog("VMVirtIOFramebuffer::createAGDCService() - Creating AGDC service for WindowServer\n");
    
    if (m_agdc_service) {
        IOLog("VMVirtIOFramebuffer::createAGDCService() - AGDC service already exists\n");
        return kIOReturnSuccess;
    }
    
    // CRITICAL: Attach AGDC service to VMVirtIOGPU device for proper provider handling
    // GPU Wrangler will detect it through GPU device association and proper service registration
    if (!m_gpu_driver) {
        IOLog("VMVirtIOFramebuffer::createAGDCService() - No GPU device available for AGDC attachment\n");
        return kIOReturnNotFound;
    }
    
    IOLog("VMVirtIOFramebuffer::createAGDCService() - Using VMVirtIOGPU device for AGDC attachment\n");
    
    IOLog("VMVirtIOFramebuffer::createAGDCService() - Creating AGDC service for GPU device attachment\n");
    
    // Create AGDC service instance
    m_agdc_service = VMVirtIOAGDC::withFramebuffer(this);
    if (!m_agdc_service) {
        IOLog("VMVirtIOFramebuffer::createAGDCService() - Failed to create AGDC service\n");
        return kIOReturnNoMemory;
    }
    
    // CRITICAL: Attach AGDC service to VMVirtIOGPU device which can handle the provider relationship
    // The AGDC service will register itself with GPU Wrangler through proper device properties
    if (!m_agdc_service->attach(m_gpu_driver)) {
        IOLog("VMVirtIOFramebuffer::createAGDCService() - Failed to attach AGDC service to GPU device\n");
        m_agdc_service->release();
        m_agdc_service = nullptr;
        return kIOReturnError;
    }
    
    if (!m_agdc_service->start(m_gpu_driver)) {
        IOLog("VMVirtIOFramebuffer::createAGDCService() - Failed to start AGDC service on GPU device\n");
        m_agdc_service->detach(m_gpu_driver);
        m_agdc_service->release();
        m_agdc_service = nullptr;
        return kIOReturnError;
    }
    
    IOLog("VMVirtIOFramebuffer::createAGDCService() - AGDC service created and started successfully\n");
    return kIOReturnSuccess;
}

void VMVirtIOFramebuffer::destroyAGDCService()
{
    if (m_agdc_service) {
        IOLog("VMVirtIOFramebuffer::destroyAGDCService() - Destroying AGDC service\n");
        
        // Stop and detach the AGDC service from GPU device
        if (m_gpu_driver) {
            m_agdc_service->stop(m_gpu_driver);
            m_agdc_service->detach(m_gpu_driver);
        }
        
        m_agdc_service->release();
        m_agdc_service = nullptr;
        
        IOLog("VMVirtIOFramebuffer::destroyAGDCService() - AGDC service destroyed\n");
    }
}

// *** TEST: Disable AGDC methods to isolate GUI issue ***
// CRITICAL: AGDC methods that WindowServer expects directly on framebuffer
// These methods are called by IOPresentment/WindowServer for AGDC functionality

/* DISABLED FOR TESTING
IOReturn VMVirtIOFramebuffer::getAGDCInformation(void* info_buffer, uint32_t buffer_size)
{
    IOLog("VMVirtIOFramebuffer::getAGDCInformation() - WindowServer calling framebuffer AGDC method, buffer=%p, size=%u\n", info_buffer, buffer_size);
    
    // If we have an AGDC service, delegate to it
    if (m_agdc_service) {
        IOLog("VMVirtIOFramebuffer::getAGDCInformation() - Delegating to AGDC service\n");
        return m_agdc_service->getAGDCInformation(info_buffer, buffer_size);
    }
    
    // Otherwise provide basic AGDC information directly
    if (buffer_size == 0) {
        IOLog("VMVirtIOFramebuffer::getAGDCInformation() - Zero buffer size, returning success for capability query\n");
        return kIOReturnSuccess;
    }
    
    if (!info_buffer || buffer_size < 16) {
        IOLog("VMVirtIOFramebuffer::getAGDCInformation() - Invalid buffer parameters\n");
        return kIOReturnBadArgument;
    }
    
    // Create minimal AGDC information
    struct AGDCInformation {
        uint32_t version;
        uint32_t vendor_id;
        uint32_t device_id;
        uint32_t capabilities;
    };
    
    AGDCInformation info;
    info.version = 1;
    info.vendor_id = 0x1AF4;  // VirtIO vendor ID
    info.device_id = 0x1050;  // VirtIO GPU device ID
    info.capabilities = 0x03; // Basic display + acceleration
    
    uint32_t copy_size = (buffer_size < sizeof(AGDCInformation)) ? buffer_size : sizeof(AGDCInformation);
    memcpy(info_buffer, &info, copy_size);
    
    IOLog("VMVirtIOFramebuffer::getAGDCInformation() - SUCCESS - provided basic AGDC info\n");
    return kIOReturnSuccess;
}

IOReturn VMVirtIOFramebuffer::acquireMap(IOMemoryMap** map)
{
    IOLog("VMVirtIOFramebuffer::acquireMap() - WindowServer calling framebuffer AGDC method, map=%p\n", map);
    
    if (!map) {
        IOLog("VMVirtIOFramebuffer::acquireMap() - ERROR: Null map parameter\n");
        return kIOReturnBadArgument;
    }
    
    // If we have an AGDC service, delegate to it
    if (m_agdc_service) {
        IOLog("VMVirtIOFramebuffer::acquireMap() - Delegating to AGDC service\n");
        return m_agdc_service->acquireMap(map);
    }
    
    // VirtIO GPU doesn't need special memory mapping for WindowServer
    *map = nullptr;
    
    IOLog("VMVirtIOFramebuffer::acquireMap() - SUCCESS - no special mapping needed\n");
    return kIOReturnSuccess;
}

IOReturn VMVirtIOFramebuffer::releaseMap(IOMemoryMap* map)
{
    IOLog("VMVirtIOFramebuffer::releaseMap() - WindowServer calling framebuffer AGDC method, map=%p\n", map);
    
    // If we have an AGDC service, delegate to it
    if (m_agdc_service) {
        IOLog("VMVirtIOFramebuffer::releaseMap() - Delegating to AGDC service\n");
        return m_agdc_service->releaseMap(map);
    }
    
    // Nothing to release for VirtIO GPU
    IOLog("VMVirtIOFramebuffer::releaseMap() - SUCCESS - nothing to release\n");
    return kIOReturnSuccess;
}

IOReturn VMVirtIOFramebuffer::locateServiceDependencies(void* dependencies_buffer, uint32_t buffer_size)
{
    IOLog("VMVirtIOFramebuffer::locateServiceDependencies() - WindowServer calling framebuffer AGDC method, buffer=%p, size=%u\n", dependencies_buffer, buffer_size);
    
    // If we have an AGDC service, delegate to it
    if (m_agdc_service) {
        IOLog("VMVirtIOFramebuffer::locateServiceDependencies() - Delegating to AGDC service\n");
        return m_agdc_service->locateServiceDependencies(dependencies_buffer, buffer_size);
    }
    
    // Clear dependencies buffer if provided
    if (dependencies_buffer && buffer_size > 0) {
        memset(dependencies_buffer, 0, buffer_size);
        IOLog("VMVirtIOFramebuffer::locateServiceDependencies() - Cleared dependencies buffer\n");
    }
    
    // For VirtIO GPU, all dependencies are satisfied (GPU driver is running)
    IOLog("VMVirtIOFramebuffer::locateServiceDependencies() - SUCCESS - all dependencies satisfied\n");
    return kIOReturnSuccess;
}
*/ // END DISABLED AGDC METHODS FOR TESTING

// Display Refresh Timer Implementation

void VMVirtIOFramebuffer::displayRefreshTimer(OSObject* owner, IOTimerEventSource* sender)
{
    VMVirtIOFramebuffer* fb = OSDynamicCast(VMVirtIOFramebuffer, owner);
    if (!fb) {
        IOLog("VMVirtIOFramebuffer::displayRefreshTimer() - ERROR: No framebuffer object\n");
        return;
    }
    
    static int call_count = 0;
    call_count++;
    
    // Log first few calls to verify timer is working
    if (call_count <= 5) {
        IOLog("VMVirtIOFramebuffer::displayRefreshTimer() - Timer fired (call #%d)\n", call_count);
    }

    /* RE-ARM FIRST (2026-08-16 fix): the old order (work, then
     * re-arm) made the period interval + work-time — measured 39-52
     * ms cycles at a "30 Hz" configuration (19-26 Hz achieved; the
     * 72c53842 boot's window instrumentation). Re-arming before the
     * work makes the wait overlap it: period = max(interval, work).
     * The period IS the rate knob now (REFRESH_PERIOD_MS) — no
     * divide-by-N throttle compounding a late-re-arm penalty on
     * skipped ticks.
     *
     * Note: do NOT chase dirty-rectangle tracking here. Per LEDGER
     * 2026-08-09, the cost under TCG is the per-command doorbell
     * round-trip, not bytes — QEMU executes TRANSFER_TO_HOST_2D
     * host-side at native speed, so sub-rect transfers leave command
     * count unchanged and buy essentially nothing; dirty-rect paths
     * were investigated and falsified. */
    if (sender && fb->m_refresh_timer) {
        fb->m_refresh_timer->setTimeoutMS(REFRESH_PERIOD_MS);
    }

    // Refresh display - transfer framebuffer to VirtIO GPU and flush to display
    fb->refreshDisplay();

}

void VMVirtIOFramebuffer::refreshDisplay()
{
    /* Achieved-rate window (see header): ticks = callbacks reached;
     * window closes on raw-delta regardless of count so MISSED ticks
     * show up as a low count, not a longer window. */
    m_tick_window_count++;
    {
        uint64_t now_raw = mach_absolute_time();
        if (now_raw - m_window_start_raw >= REFRESH_WINDOW_RAW) {
            uint64_t dur_raw = now_raw - m_window_start_raw;
            IOLog("VMVirtIOFramebuffer: refresh window — ticks=%llu "
                  "xfers=%llu dur=%llu raw workavg=%llu ns/xfer "
                  "(achieved ~%llu.%u Hz)\n",
                  (unsigned long long)m_tick_window_count,
                  (unsigned long long)m_xfer_window_count,
                  (unsigned long long)dur_raw,
                  m_xfer_window_count
                      ? (unsigned long long)(m_work_window_sum / m_xfer_window_count)
                      : 0ULL,
                  m_xfer_window_count
                      ? (unsigned long long)(m_xfer_window_count / (dur_raw / 1000000000ULL))
                      : 0ULL,
                  m_xfer_window_count
                      ? (unsigned)((m_xfer_window_count % (dur_raw / 1000000000ULL)) * 10 /
                                   (dur_raw / 1000000000ULL))
                      : 0U);
            m_tick_window_count = 0;
            m_xfer_window_count = 0;
            m_work_window_sum = 0;
            m_window_start_raw = now_raw;
        }
    }

    // Only refresh if GPU driver is available
    if (!m_gpu_driver) {
        IOLog("VMVirtIOFramebuffer::refreshDisplay() - SKIP: No GPU driver\n");
        return;
    }

    // Only perform work if we have a valid scanout resource id
    if (m_scanout_resource_id == 0) {
        IOLog("VMVirtIOFramebuffer::refreshDisplay() - SKIP: No scanout resource ID\n");
        return;
    }

    // CRITICAL: Check if scanout has been taken over by 3D rendering
    // If a 3D application has attached its own resource to the scanout,
    // we should NOT overwrite it with our 2D framebuffer content.
    // 3D resources manage their own transfer/flush cycle.
    //
    // RUNG 67c — THE WATCHDOG: while the 3D app lives it flushes every
    // frame. If it dies WITHOUT reaching clientClose (observed: SIGTERM
    // kill — the 10.6 IOKit task-death teardown can lag or skip), the
    // scanout stays bound to a dead resource: display black until manual
    // recovery. The tick notices: no flush for STALE_MS while bound →
    // force the release. Covers ALL death classes (crash, kill, hang,
    // teardown-skip) in one place.
    if (m_scanout_taken_over_by_3d) {
        if (m_gpu_driver && m_gpu_driver->scanout3dStale()) {
            IOLog("VMVirtIOFramebuffer: 3D scanout STALE (no flush) — "
                  "watchdog RELEASE\n");
            m_gpu_driver->releaseScanout3D();
        }
        return;
    }

    // Full-surface refresh every fire — the period IS the rate knob
    // (REFRESH_PERIOD_MS, re-armed before the work; see the callback and
    // the header's cadence note). Dead-end investigations (cursorRect,
    // setCursorState, content-diff) are recorded in the header.
    static bool logged_first_refresh = false;
    if (!logged_first_refresh) {
        logged_first_refresh = true;
        IOLog("VMVirtIOFramebuffer::refreshDisplay: first tick (mode=%u %ux%u) — period %u ms\n",
              (unsigned)m_current_mode, (unsigned)m_width, (unsigned)m_height,
              (unsigned)REFRESH_PERIOD_MS);
    }

    uint64_t work0 = mach_absolute_time();
    IOReturn transfer_result = m_gpu_driver->transferToHost2D(m_scanout_resource_id, 0,
                                                               0, 0, m_width, m_height);
    if (transfer_result != kIOReturnSuccess) {
        IOLog("VMVirtIOFramebuffer::refreshDisplay() - transferToHost2D FAILED: 0x%x\n",
              transfer_result);
        return;
    }
    IOReturn flush_result = m_gpu_driver->flushResource(m_scanout_resource_id, 0, 0,
                                                        m_width, m_height);
    if (flush_result != kIOReturnSuccess) {
        IOLog("VMVirtIOFramebuffer::refreshDisplay() - flushResource FAILED: 0x%x\n",
              flush_result);
        return;
    }
    /* Work-time kept SEPARATE from period (user direction): the two
     * must stay distinguishable — work is the budget datum (and is
     * mode-dependent). Raw ns, same as the window clock. */
    m_work_window_sum += mach_absolute_time() - work0;
    m_xfer_window_count++;   /* successful transfer+flush pair only */

    static bool logged_success = false;
    if (!logged_success) {
        IOLog("VMVirtIOFramebuffer::refreshDisplay() - full-surface Transfer+Flush SUCCESS for resource %d (%ux%u)\n",
              (unsigned)m_scanout_resource_id, (unsigned)m_width, (unsigned)m_height);
        logged_success = true;
    }
}

// 3D Scanout Management
void VMVirtIOFramebuffer::setScanoutTakenOverBy3D(bool taken_over)
{
    if (taken_over != m_scanout_taken_over_by_3d) {
        IOLog("VMVirtIOFramebuffer: Scanout control %s by 3D application\n",
              taken_over ? "taken over" : "returned to 2D");
        m_scanout_taken_over_by_3d = taken_over;
    }
}

/* RUNG 67b — THE RELEASE PATH: the 3D owner is gone (clean exit or
 * crash); rebind the 2D desktop scanout and stand the refresh back
 * up. Without this, app exit leaves the scanout bound to a dead
 * resource — the whole display goes black until reboot (observed
 * 2026-08-24, restored by reboot). */
void VMVirtIOFramebuffer::restore2DScanout()
{
    if (!m_gpu_driver || !m_scanout_resource_id) {
        m_scanout_taken_over_by_3d = false;   /* at least resume 2D */
        return;
    }
    IOReturn ret = m_gpu_driver->setscanout(0, m_scanout_resource_id,
                                            0, 0, m_width, m_height);
    m_scanout_taken_over_by_3d = false;
    /* force one immediate refresh so the desktop reappears at once */
    refreshDisplay();
    IOLog("VMVirtIOFramebuffer: 2D scanout RESTORED (res=%u %ux%u "
          "setscanout=0x%x) after 3D release\n",
          (unsigned)m_scanout_resource_id, (unsigned)m_width,
          (unsigned)m_height, ret);
}

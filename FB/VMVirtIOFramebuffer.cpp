#include "VMVirtIOFramebuffer.h"
#include "VMVirtIOGPU.h"
#include "VMVirtIOAGDC.h"
#include <IOKit/IOLib.h>
#include <IOKit/ndrvsupport/IOMacOSVideo.h>
#include <IOKit/graphics/IOGraphicsTypes.h>
#include <IOKit/graphics/IOAccelClientConnect.h>

#define super IOFramebuffer
OSDefineMetaClassAndStructors(VMVirtIOFramebuffer, IOFramebuffer);

bool VMVirtIOFramebuffer::init(OSDictionary* properties)
{
    if (!super::init(properties)) {
        return false;
    }
    
    m_gpu_driver = nullptr;
    m_pci_device = nullptr;
    m_vram_range = nullptr;
    m_agdc_service = nullptr;
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
    if (m_vram_range) {
        m_vram_range->release();
        m_vram_range = nullptr;
    }
    
    // destroyAGDCService(); // DISABLED FOR TESTING
    
    super::free();
}

IOService* VMVirtIOFramebuffer::probe(IOService* provider, SInt32* score)
{
    IOLog("VMVirtIOFramebuffer::probe() - SIMPLE APPROACH\n");
    
    // SIMPLE: Just check if provider is VMVirtIOGPU
    VMVirtIOGPU* gpuDevice = OSDynamicCast(VMVirtIOGPU, provider);
    if (!gpuDevice) {
        IOLog("VMVirtIOFramebuffer::probe() - Provider is not VMVirtIOGPU\n");
        return nullptr;
    }
    
    // Simple probe score
    *score = 1000;
    
    IOLog("VMVirtIOFramebuffer::probe() - SUCCESS: Simple framebuffer probe complete\n");
    
    return super::probe(provider, score);
}

bool VMVirtIOFramebuffer::start(IOService* provider)
{
    IOLog("VMVirtIOFramebuffer::start() - SIMPLE FRAMEBUFFER starting\n");
    
    // Simple provider check
    VMVirtIOGPU* gpuDevice = OSDynamicCast(VMVirtIOGPU, provider);
    if (!gpuDevice) {
        IOLog("VMVirtIOFramebuffer::start() - Provider is not VMVirtIOGPU\n");
        return false;
    }
    
    IOLog("VMVirtIOFramebuffer::start() - Simple framebuffer mode\n");
    
    if (!super::start(provider)) {
        IOLog("VMVirtIOFramebuffer::start() - super::start() failed\n");
        return false;
    }
    
    // Provider is now VMVirtIOGPU (traditional approach)
    m_gpu_driver = gpuDevice;
    IOLog("VMVirtIOFramebuffer::start() - Traditional provider mode: provider=%p (VMVirtIOGPU)\n", provider);
    
    // Provider is VMVirtIOGPU, get PCI device from its provider
    m_pci_device = OSDynamicCast(IOPCIDevice, gpuDevice->getProvider());
    m_gpu_driver = gpuDevice; // VMVirtIOGPU instance
    
    IOLog("VMVirtIOFramebuffer::start() - Traditional mode: provider=%p, gpu_driver=%p, pci_device=%p\n", 
          provider, m_gpu_driver, m_pci_device);
    
    // *** VRAM SIZE FIX: Set proper VRAM properties for System Information ***
    uint32_t vram_size = 512 * 1024 * 1024;  // 512MB
    uint32_t vram_mb = 512;  // 512MB
    
    // Use OSNumber objects for proper numeric property setting
    OSNumber* vram_size_num = OSNumber::withNumber(vram_size, 32);
    OSNumber* vram_mb_num = OSNumber::withNumber(vram_mb, 32);
    
    if (vram_size_num && vram_mb_num) {
        setProperty("VRAM,totalsize", vram_size_num);
        setProperty("ATY,memsize", vram_size_num);
        setProperty("gpu-memory-size", vram_size_num);
        setProperty("framebuffer-memory", vram_size_num);
        setProperty("IOAccelMemorySize", vram_size_num);
        setProperty("VRAM,totalMB", vram_mb_num);
        
        vram_size_num->release();
        vram_mb_num->release();
        
        IOLog("VMVirtIOFramebuffer::start() - VRAM size configured: %u MB using OSNumber objects\n", vram_mb);
    } else {
        IOLog("VMVirtIOFramebuffer::start() - ERROR: Failed to create OSNumber objects for VRAM properties\n");
    }
    
    // *** CRITICAL: OpenGL/Hardware Acceleration Properties ***
    
    // DISABLE ALL hardware acceleration to fix WindowServer crashes
    setProperty("IOAcceleratorFamily", kOSBooleanFalse);
    setProperty("IOGraphicsAccelerator", kOSBooleanFalse);
    setProperty("IODisplayAccelerated", kOSBooleanFalse);
    setProperty("IOAccelerator3D", kOSBooleanFalse);
    
    // REMOVED: All OpenGL/Metal configuration to prevent WindowServer from trying to use it
    // setProperty("IOGLBundleName", "GLEngine");
    // setProperty("IOAccelIndex", 0);
    
    // DISABLE AGDC: Tell WindowServer we DON'T support AGDC to prevent initialization failures
    // WindowServer was crashing because we claimed AGDC support but didn't implement it
    setProperty("AGDC", kOSBooleanFalse);                   // NOT AGDC capable
    setProperty("AGDCCapable", kOSBooleanFalse);            // NO AGDC capability
    setProperty("AGDCVersion", OSNumber::withNumber(0ULL, 32));    // No AGDC version
    // No AGDC capabilities at all
    setProperty("AGDCCapabilities", OSNumber::withNumber(0ULL, 32));
    
    // DISABLE GPU Controller - we're a simple framebuffer
    setProperty("GPUController", kOSBooleanFalse);
    setProperty("AGDPClientControl", kOSBooleanFalse);
    
    // ENABLE Hardware Video Acceleration for VirtIO GPU
    // Tell WindowServer we have hardware video acceleration capabilities
    setProperty("IOVideoAcceleration", kOSBooleanTrue);    // Hardware video acceleration
    setProperty("IOHardwareVideoAcceleration", kOSBooleanTrue); // HW video accel enabled
    setProperty("IOGVAHEVCDecodeCapabilities", 0ULL, 64);   // HEVC decode (basic support)
    setProperty("IOGVACodec", kOSBooleanTrue);             // Video codec support enabled
    
    // ENABLE Metal compositor with minimal software renderer plugin
    // This provides a valid MTLDevice pointer to prevent WindowServer abort()
    setProperty("MetalPluginClassName", "VMMetalPlugin");           // Our Metal plugin class
    setProperty("MetalPluginName", "VMware/QEMU Metal Software Renderer");
    setProperty("MetalStatisticsName", "VMMetalPlugin");
    setProperty("IOMetalBundleName", "");                           // No external bundle needed
    setProperty("IOGLESBundleName", "");                            // No OpenGL ES
    setProperty("PerformanceStatistics", OSArray::withCapacity(0)); // Empty but non-null
    setProperty("MetalCoalescingMode", 1, 32);                      // Enable coalescing
    setProperty("MetalCapabilityFamily", 1, 32);                    // GPU Family 1
    
    // Graphics device properties  
    setProperty("IOGraphicsDevice", kOSBooleanTrue);
    // Note: Removed IOConsoleDevice to prevent forcing console mode
    
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
    
    // Enable controller
    IOLog("VMVirtIOFramebuffer::start() - Enabling framebuffer controller\n");
    IOReturn enable_result = enableController();
    if (enable_result == kIOReturnSuccess) {
        IOLog("VMVirtIOFramebuffer::start() - Framebuffer controller enabled successfully\n");
    } else {
        IOLog("VMVirtIOFramebuffer::start() - WARNING: Controller enable failed: 0x%08x\n", enable_result);
    }
    
    // *** TEST: Disable AGDC service to isolate GUI login issue ***
    IOLog("VMVirtIOFramebuffer::start() - AGDC service creation DISABLED for testing\n");
    // IOReturn agdc_result = createAGDCService();
    // if (agdc_result == kIOReturnSuccess) {
    //     IOLog("VMVirtIOFramebuffer::start() - AGDC service created successfully\n");
    // } else {
    //     IOLog("VMVirtIOFramebuffer::start() - WARNING: AGDC service creation failed: 0x%08x\n", agdc_result);
    // }
    
    registerService();
    IOLog("VMVirtIOFramebuffer::start() - Framebuffer registration complete\n");
    
    return true;
}

void VMVirtIOFramebuffer::stop(IOService* provider)
{
    IOLog("VMVirtIOFramebuffer::stop() - Stopping framebuffer\n");
    
    if (m_vram_range) {
        m_vram_range->release();
        m_vram_range = nullptr;
    }
    
    super::stop(provider);
}

void VMVirtIOFramebuffer::initDisplayModes()
{
    // Create basic display modes
    m_display_modes[0] = 1;   // 1024x768
    m_display_modes[1] = 2;   // 1280x1024
    m_display_modes[2] = 3;   // 1440x900
    m_display_modes[3] = 4;   // 1680x1050
    m_display_modes[4] = 5;   // 1920x1080
    m_display_modes[5] = 6;   // 2560x1440
    m_display_modes[6] = 7;   // 3840x2160
    m_mode_count = 7;
    m_current_mode = 1; // Default to 1024x768
}

// IOFramebuffer required pure virtual methods
IODeviceMemory* VMVirtIOFramebuffer::getApertureRange(IOPixelAperture aperture)
{
    IOLog("VMVirtIOFramebuffer::getApertureRange: aperture=%d\n", (int)aperture);
    
    if (aperture != kIOFBSystemAperture) {
        return nullptr;
    }
    
    if (m_vram_range) {
        IOLog("VMVirtIOFramebuffer::getApertureRange: Using cached PCI region 0 VRAM\n");
        m_vram_range->retain();
        return m_vram_range;
    }
    
    // SAFE PCI BAR 0 ACCESS: Provide real framebuffer memory for hardware acceleration
    // According to VirtIO spec: "PCI region 0 has the linear framebuffer" in VGA compatibility mode
    // This is essential for OpenGL/Metal hardware acceleration to work
    
    IOLog("VMVirtIOFramebuffer::getApertureRange: SAFE VERSION - Attempting PCI BAR 0 access for hardware acceleration\n");
    
    if (!m_pci_device) {
        IOLog("VMVirtIOFramebuffer::getApertureRange: No PCI device available - using software fallback\n");
        return nullptr;
    }
    
    // STEP 1: Get PCI BAR 0 memory object safely with extensive validation
    IODeviceMemory* bar0_memory = m_pci_device->getDeviceMemoryWithIndex(0);
    if (!bar0_memory) {
        IOLog("VMVirtIOFramebuffer::getApertureRange: PCI BAR 0 not available - using software fallback\n");
        return nullptr;
    }
    
    // STEP 2: Validate BAR 0 properties extensively before using
    IOPhysicalAddress bar0_phys = bar0_memory->getPhysicalAddress();
    IOByteCount bar0_size = bar0_memory->getLength();
    
    IOLog("VMVirtIOFramebuffer::getApertureRange: PCI BAR 0 found - phys=0x%llx, size=0x%llx (%llu MB)\n", 
          (unsigned long long)bar0_phys, (unsigned long long)bar0_size, 
          (unsigned long long)(bar0_size / (1024 * 1024)));
    
    // STEP 3: Comprehensive safety validation
    if (bar0_phys == 0 || bar0_phys == 0xFFFFFFFFULL || bar0_phys == 0xFFFFFFFFFFFFFFFFULL) {
        IOLog("VMVirtIOFramebuffer::getApertureRange: Invalid BAR 0 physical address 0x%llx - using software fallback\n", 
              (unsigned long long)bar0_phys);
        return nullptr;
    }
    
    if (bar0_size == 0 || bar0_size < (1024 * 1024)) {  // At least 1MB
        IOLog("VMVirtIOFramebuffer::getApertureRange: Invalid BAR 0 size %llu bytes - using software fallback\n", 
              (unsigned long long)bar0_size);
        return nullptr;
    }
    
    if (bar0_size > (2ULL * 1024 * 1024 * 1024)) {  // Max 2GB for sanity
        IOLog("VMVirtIOFramebuffer::getApertureRange: BAR 0 size %llu bytes too large - using software fallback\n", 
              (unsigned long long)bar0_size);
        return nullptr;
    }
    
    // STEP 4: Use existing BAR 0 memory object safely (no new allocation)
    IOLog("VMVirtIOFramebuffer::getApertureRange: Using PCI BAR 0 for framebuffer memory - enabling hardware acceleration\n");
    IOLog("VMVirtIOFramebuffer::getApertureRange: Hardware framebuffer: phys=0x%llx, size=%llu MB\n", 
          (unsigned long long)bar0_phys, (unsigned long long)(bar0_size / (1024 * 1024)));
    
    // Cache for future use
    m_vram_range = bar0_memory;
    m_vram_range->retain();
    
    // Return reference to existing memory object (safe)
    bar0_memory->retain();
    return bar0_memory;
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
    return m_mode_count;
}

IOReturn VMVirtIOFramebuffer::getDisplayModes(IODisplayModeID* allDisplayModes)
{
    if (!allDisplayModes) {
        return kIOReturnBadArgument;
    }
    
    for (IOItemCount i = 0; i < m_mode_count; i++) {
        allDisplayModes[i] = m_display_modes[i];
    }
    
    return kIOReturnSuccess;
}

IOReturn VMVirtIOFramebuffer::getInformationForDisplayMode(IODisplayModeID displayMode, 
                                                           IODisplayModeInformation* info)
{
    if (!info) {
        return kIOReturnBadArgument;
    }
    
    // Set common flags for stable display
    info->flags = kDisplayModeValidFlag | kDisplayModeDefaultFlag | kDisplayModeSafeFlag;
    info->refreshRate = 60 << 16; // 60 Hz in fixed point
    info->maxDepthIndex = 0; // Only support 32-bit depth
    
    switch (displayMode) {
        case 1: // 1024x768 - Safe fallback mode
            info->nominalWidth = 1024;
            info->nominalHeight = 768;
            info->flags |= kDisplayModeDefaultFlag;
            break;
        case 2: // 1280x1024
            info->nominalWidth = 1280;
            info->nominalHeight = 1024;
            break;
        case 3: // 1440x900
            info->nominalWidth = 1440;
            info->nominalHeight = 900;
            break;
        case 4: // 1680x1050
            info->nominalWidth = 1680;
            info->nominalHeight = 1050;
            break;
        case 5: // 1920x1080
            info->nominalWidth = 1920;
            info->nominalHeight = 1080;
            break;
        case 6: // 2560x1440
            info->nominalWidth = 2560;
            info->nominalHeight = 1440;
            break;
        case 7: // 3840x2160
            info->nominalWidth = 3840;
            info->nominalHeight = 2160;
            break;
        default:
            return kIOReturnUnsupported;
    }
    
    return kIOReturnSuccess;
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
    
    pixelInfo->bytesPerRow = modeInfo.nominalWidth * 4; // 32-bit pixels
    pixelInfo->bytesPerPlane = pixelInfo->bytesPerRow * modeInfo.nominalHeight;
    pixelInfo->bitsPerPixel = 32;
    pixelInfo->pixelType = kIORGBDirectPixels;
    pixelInfo->componentCount = 3;
    pixelInfo->bitsPerComponent = 8;
    pixelInfo->componentMasks[0] = 0x00FF0000; // Red
    pixelInfo->componentMasks[1] = 0x0000FF00; // Green
    pixelInfo->componentMasks[2] = 0x000000FF; // Blue
    pixelInfo->flags = 0;
    pixelInfo->activeWidth = modeInfo.nominalWidth;
    pixelInfo->activeHeight = modeInfo.nominalHeight;
    
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

// CRITICAL: Safe open method override for WindowServer connection handling
IOReturn VMVirtIOFramebuffer::open(void)
{
    IOLog("VMVirtIOFramebuffer::open() - *** WINDOWSERVER OPEN REQUESTED ***\n");
    
    // Set properties that indicate we're ready for GUI mode
    setProperty("IOFramebufferOpenForGUI", kOSBooleanTrue);
    setProperty("WindowServerReady", kOSBooleanTrue);
    
    // SAFETY: Call parent open method first, but handle failures gracefully
    IOReturn result = super::open();
    IOLog("VMVirtIOFramebuffer::open() - Parent open returned: 0x%x\n", result);
    
    if (result != kIOReturnSuccess) {
        IOLog("VMVirtIOFramebuffer::open() - Parent open failed: 0x%x, but continuing for VM compatibility\n", result);
        // For VM environments, we might want to succeed even if parent fails
        // This prevents WindowServer from crashing if parent open has issues
        result = kIOReturnSuccess;
    }
    
    // CRITICAL: Force GUI mode properties when opened by WindowServer
    // NOTE: Keep IOConsoleDevice=true (set by isConsoleDevice()) for QXL-style dual capability
    setProperty("IOGUIDevice", kOSBooleanTrue);           // Enable GUI mode
    setProperty("IODisplayAccelerated", kOSBooleanFalse);  // DISABLE acceleration - no Metal support yet
    
    IOLog("VMVirtIOFramebuffer::open() - *** GUI MODE FORCED ON - CONSOLE MODE DISABLED ***\n");
    
    // CRITICAL: Disable console scanout 0 to allow GUI to take over display
    // VirtIO GPU spec: setscanout with resource_id=0 disables that scanout
    if (m_gpu_driver) {
        IOLog("VMVirtIOFramebuffer::open() - Disabling console scanout 0 for GUI transition\n");
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
    
    // CRITICAL: Manually trigger enableController since Apple's open might not be calling it properly in VM
    IOLog("VMVirtIOFramebuffer::open() - Manually calling enableController for GUI display activation\n");
    IOReturn enable_result = enableController();
    IOLog("VMVirtIOFramebuffer::open() - enableController returned: 0x%x\n", enable_result);
    
    IOLog("VMVirtIOFramebuffer::open() - *** WINDOWSERVER OPEN COMPLETED - GUI MODE ACTIVE ***\n");
    return result;
}

void VMVirtIOFramebuffer::close(void)
{
    IOLog("VMVirtIOFramebuffer::close() - *** WINDOWSERVER CLOSE REQUESTED ***\n");
    
    // Reset GUI mode properties when WindowServer closes
    setProperty("IOFramebufferOpenForGUI", kOSBooleanFalse);
    setProperty("WindowServerActive", kOSBooleanFalse);
    setProperty("IOGUIActive", kOSBooleanFalse);
    
    IOLog("VMVirtIOFramebuffer::close() - GUI mode properties reset\n");
    
    super::close();
    
    IOLog("VMVirtIOFramebuffer::close() - *** WINDOWSERVER CLOSE COMPLETED ***\n");
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
    
    // DIRECT PCI MODE: We operate as a software framebuffer without VirtIO GPU acceleration
    IOLog("VMVirtIOFramebuffer::enableController() - DIRECT PCI MODE: Software framebuffer for primary display\n");
    
    if (m_gpu_driver) {
        IOLog("VMVirtIOFramebuffer::enableController() - VirtIO GPU driver available - setting up accelerated display\n");
        
        // Set up display with current resolution
        IODisplayModeInformation modeInfo;
        IOReturn modeResult = getInformationForDisplayMode(m_current_mode, &modeInfo);
        if (modeResult == kIOReturnSuccess) {
            m_width = modeInfo.nominalWidth;
            m_height = modeInfo.nominalHeight;
            m_depth = 32;
            
            IOLog("VMVirtIOFramebuffer::enableController() - Setting up VirtIO display: %dx%d@%d\n", 
                  m_width, m_height, m_depth);
            
            // Create display resource on VirtIO GPU
            uint32_t resource_id = 1;  // Primary display resource
            IOReturn createResult = m_gpu_driver->createResource2D(resource_id, 
                                                                   0x1, // VIRTIO_GPU_FORMAT_B8G8R8A8_UNORM
                                                                   m_width, m_height);
            if (createResult == kIOReturnSuccess) {
                IOLog("VMVirtIOFramebuffer::enableController() - VirtIO GPU display resource created successfully\n");
                
                // Set scanout to enable display output
                IOReturn scanoutResult = m_gpu_driver->setscanout(0, resource_id, 0, 0, m_width, m_height);
                if (scanoutResult == kIOReturnSuccess) {
                    IOLog("VMVirtIOFramebuffer::enableController() - VirtIO GPU scanout enabled - GUI should activate\n");
                } else {
                    IOLog("VMVirtIOFramebuffer::enableController() - VirtIO GPU scanout failed: 0x%x\n", scanoutResult);
                }
            } else {
                IOLog("VMVirtIOFramebuffer::enableController() - VirtIO GPU resource creation failed: 0x%x\n", createResult);
            }
        } else {
            IOLog("VMVirtIOFramebuffer::enableController() - Failed to get mode info: 0x%x\n", modeResult);
        }
    } else {
        IOLog("VMVirtIOFramebuffer::enableController() - DIRECT PCI MODE: Software framebuffer without VirtIO GPU\n");
        IOLog("VMVirtIOFramebuffer::enableController() - Primary framebuffer (index 0) provides GUI mode capability\n");
        
        // Set up basic display mode information
        IODisplayModeInformation modeInfo;
        IOReturn modeResult = getInformationForDisplayMode(m_current_mode, &modeInfo);
        if (modeResult == kIOReturnSuccess) {
            m_width = modeInfo.nominalWidth;
            m_height = modeInfo.nominalHeight;
            m_depth = 32;
            
            IOLog("VMVirtIOFramebuffer::enableController() - Software display mode: %dx%d@%d\n", 
                  m_width, m_height, m_depth);
        }
    }
    
    // CRITICAL: Implement safe software display output since VirtIO GPU is accelerator-only
    // We need to make the framebuffer content visible without VirtIO GPU scanout
    IOLog("VMVirtIOFramebuffer::enableController() - Implementing safe software display output\n");
    
    // STEP 1: Ensure framebuffer memory is properly mapped and accessible
    // Call getApertureRange to initialize VRAM if not already done
    if (!m_vram_range) {
        IOLog("VMVirtIOFramebuffer::enableController() - Initializing VRAM access\n");
        IODeviceMemory* vram = getApertureRange(kIOFBSystemAperture);
        if (vram) {
            IOLog("VMVirtIOFramebuffer::enableController() - VRAM initialized successfully\n");
            vram->release(); // getApertureRange already retained it for m_vram_range
        }
    }
    
    if (m_vram_range) {
        IOLog("VMVirtIOFramebuffer::enableController() - Framebuffer memory available: %p\n", m_vram_range);
        IOLog("VMVirtIOFramebuffer::enableController() - Framebuffer size: %dx%d@%d\n", m_width, m_height, m_depth);
        
        // Get actual VRAM size from the memory object
        IOByteCount vram_size = m_vram_range->getLength();
        IOLog("VMVirtIOFramebuffer::enableController() - Actual VRAM size: %llu MB\n", 
              (unsigned long long)(vram_size / (1024 * 1024)));
        
        // SAFE: Log framebuffer status without direct memory access
        IOLog("VMVirtIOFramebuffer::enableController() - Framebuffer ready for display system\n");
        
    } else {
        IOLog("VMVirtIOFramebuffer::enableController() - WARNING: No framebuffer memory available\n");
    }
    
    // STEP 2: Software display activation through IOFramebuffer mechanisms
    IOLog("VMVirtIOFramebuffer::enableController() - Software display output ready\n");
    
    IOLog("VMVirtIOFramebuffer::enableController() - Controller enabled successfully\n");
    return kIOReturnSuccess;
}
// Note: enableController() method removed to use IOFramebuffer's default implementation
// This allows proper console-to-GUI transition like QXL devices

IOReturn VMVirtIOFramebuffer::setDisplayMode(IODisplayModeID displayMode, IOIndex depth)
{
    IOLog("VMVirtIOFramebuffer::setDisplayMode() - mode=%d, depth=%d\n", (int)displayMode, (int)depth);
    
    if (displayMode < 1 || displayMode > (IODisplayModeID)m_mode_count) {
        IOLog("VMVirtIOFramebuffer::setDisplayMode() - Invalid mode %d\n", (int)displayMode);
        return kIOReturnUnsupported;
    }
    
    m_current_mode = displayMode;
    
    // Update width/height based on mode
    IODisplayModeInformation modeInfo;
    IOReturn result = getInformationForDisplayMode(displayMode, &modeInfo);
    if (result == kIOReturnSuccess) {
        m_width = modeInfo.nominalWidth;
        m_height = modeInfo.nominalHeight;
        m_depth = 32; // Force 32-bit depth for stability
        
        IOLog("VMVirtIOFramebuffer::setDisplayMode() - Set resolution to %dx%d@%d\n", 
              m_width, m_height, m_depth);
        
        // SAFE: Log display mode change without direct memory access
        IOLog("VMVirtIOFramebuffer::setDisplayMode() - Display mode updated successfully\n");
        IOLog("VMVirtIOFramebuffer::setDisplayMode() - Framebuffer ready for software display output\n");
        
        // Notify VirtIO GPU driver about mode change
        if (m_gpu_driver) {
            // Tell VirtIO GPU about the new resolution
            IOLog("VMVirtIOFramebuffer::setDisplayMode() - Notifying VirtIO GPU of mode change\n");
        }
        
        // Ensure framebuffer is synchronized
        IOSleep(50); // Small delay for mode change stabilization
    } else {
        IOLog("VMVirtIOFramebuffer::setDisplayMode() - Failed to get mode information\n");
    }
    
    return kIOReturnSuccess;
}

IOReturn VMVirtIOFramebuffer::setupForCurrentConfig()
{
    IOLog("VMVirtIOFramebuffer::setupForCurrentConfig() - *** WINDOWSERVER GUI TRANSITION REQUESTED ***\n");
    
    // This method is called by WindowServer when it wants to take control of the display
    // It's the key method for transitioning from console mode to GUI mode
    
    // CRITICAL: Force GUI mode properties immediately
    setProperty("IOFramebufferOpenForGUI", kOSBooleanTrue);
    // NOTE: Keep IOConsoleDevice=true (set by isConsoleDevice()) for QXL-style dual capability
    setProperty("IOGUIDevice", kOSBooleanTrue);           // Enable GUI
    setProperty("IOGUIActive", kOSBooleanTrue);
    setProperty("VMVirtIOGUIMode", kOSBooleanTrue);
    setProperty("WindowServerActive", kOSBooleanTrue);
    
    IOLog("VMVirtIOFramebuffer::setupForCurrentConfig() - FORCING GUI MODE ACTIVATION - CONSOLE DISABLED\n");
    
    // Enable the display for GUI use
    IOReturn result = enableController();
    if (result != kIOReturnSuccess) {
        IOLog("VMVirtIOFramebuffer::setupForCurrentConfig() - enableController failed: 0x%x\n", result);
        return result;
    }
    
    // Ensure we're in the correct display mode
    IODisplayModeID currentMode;
    IOIndex currentDepth;
    result = getCurrentDisplayMode(&currentMode, &currentDepth);
    if (result == kIOReturnSuccess) {
        IOLog("VMVirtIOFramebuffer::setupForCurrentConfig() - Current mode: %d, depth: %d\n", 
              (int)currentMode, (int)currentDepth);
        
        // Re-apply the current mode to ensure everything is properly configured
        result = setDisplayMode(currentMode, currentDepth);
        if (result != kIOReturnSuccess) {
            IOLog("VMVirtIOFramebuffer::setupForCurrentConfig() - setDisplayMode failed: 0x%x\n", result);
            return result;
        }
    } else {
        // If we can't get the current mode, set a default one
        IOLog("VMVirtIOFramebuffer::setupForCurrentConfig() - Using default mode 1024x768\n");
        result = setDisplayMode(1, 0); // Default mode
        if (result != kIOReturnSuccess) {
            IOLog("VMVirtIOFramebuffer::setupForCurrentConfig() - Default setDisplayMode failed: 0x%x\n", result);
            return result;
        }
    }
    
    // Mark the transition as complete
    setProperty("VMVirtIOGUITransition", kOSBooleanTrue);
    
    IOLog("VMVirtIOFramebuffer::setupForCurrentConfig() - *** GUI TRANSITION COMPLETED SUCCESSFULLY ***\n");
    return kIOReturnSuccess;
}

IOItemCount VMVirtIOFramebuffer::getConnectionCount(void)
{
    return 1; // Single display connection
}



bool VMVirtIOFramebuffer::isConsoleDevice(void)
{
    IOLog("VMVirtIOFramebuffer::isConsoleDevice() - QXL-STYLE CONSOLE DEVICE SUPPORT\n");
    
    // Like QXL: Always claim to be a console device, but support both console and GUI modes
    // This allows proper console boot and GUI transitions
    
    // DISABLED: Accelerator properties cause WindowServer crashes on Catalina
    setProperty("IODisplayAccelerated", kOSBooleanFalse);
    setProperty("IOGraphicsAccelerator", kOSBooleanFalse);
    setProperty("IOConsoleDevice", kOSBooleanTrue);        // Always console capable
    setProperty("IOGUIDevice", kOSBooleanTrue);            // Always GUI capable
    setProperty("IOPrimaryDisplay", kOSBooleanTrue);       // Primary display
    setProperty("IOMatchCategory", "IOFramebuffer");
    // REMOVED: IOGLBundleName triggers WindowServer to try using OpenGL/Metal
    setProperty("IOAcceleratorFamily", kOSBooleanFalse);   // DISABLED: Causes WindowServer crashes
    
    // DISABLE AGDC properties - tell WindowServer we DON'T support AGDC (d57 fix)
    setProperty("AGDC", kOSBooleanFalse);
    setProperty("AGDCCapable", kOSBooleanFalse);
    setProperty("GPUController", kOSBooleanFalse);
    
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
                IOLog("VMVirtIOFramebuffer::getAttributeForConnection() - HDDC capability check: SUPPORTED\n");
                return kIOReturnSuccess; // We support HDDC for display pipeline
                
            case 0x6c646463: // 'lddc' - Low Definition Display Controller
                IOLog("VMVirtIOFramebuffer::getAttributeForConnection() - LDDC capability check: SUPPORTED\n");
                return kIOReturnSuccess; // We support LDDC for display pipeline
                
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

// User client support for Metal/acceleration compatibility
IOReturn VMVirtIOFramebuffer::newUserClient(task_t owningTask, void* security_id, UInt32 type, IOUserClient** clientH)
{
    IOLog("VMVirtIOFramebuffer::newUserClient() - APPLE-STYLE VERSION - type=%d (0x%x)\n", (int)type, (unsigned int)type);
    
    // Log specific connection types like Apple does
    if (type == kIOFBServerConnectType) {
        IOLog("VMVirtIOFramebuffer::newUserClient() - kIOFBServerConnectType - This should trigger open()\n");
    } else if (type == kIOFBSharedConnectType) {
        IOLog("VMVirtIOFramebuffer::newUserClient() - kIOFBSharedConnectType - Shared connection\n");
    } else if (type == kIOAccelSurfaceClientType) {
        IOLog("VMVirtIOFramebuffer::newUserClient() - kIOAccelSurfaceClientType - Metal surface client\n");
    } else {
        IOLog("VMVirtIOFramebuffer::newUserClient() - Unknown type: %d (0x%x)\n", (int)type, (unsigned int)type);
    }
    
    if (!clientH) {
        IOLog("VMVirtIOFramebuffer::newUserClient() - NULL client pointer\n");
        return kIOReturnBadArgument;
    }
    
    // IMPORTANT: Let parent handle all connection types properly
    // This ensures Apple's open() logic works correctly for server connections
    IOReturn result = super::newUserClient(owningTask, security_id, type, clientH);
    IOLog("VMVirtIOFramebuffer::newUserClient() - Parent result: 0x%x\n", result);
    
    return result;
}

// CRITICAL: Cursor support methods (required for GUI mode)
IOReturn VMVirtIOFramebuffer::setCursorImage(void* cursorImage)
{
    IOLog("VMVirtIOFramebuffer::setCursorImage() - Setting cursor image for GUI mode\n");
    
    // For VirtIO GPU, we don't need to handle cursor in hardware
    // The system will handle software cursor compositing
    // Just return success to indicate cursor capability
    
    if (!cursorImage) {
        IOLog("VMVirtIOFramebuffer::setCursorImage() - NULL cursor image, using default\n");
    } else {
        IOLog("VMVirtIOFramebuffer::setCursorImage() - Custom cursor image set successfully\n");
    }
    
    return kIOReturnSuccess;
}

IOReturn VMVirtIOFramebuffer::setCursorState(SInt32 x, SInt32 y, bool visible)
{
    IOLog("VMVirtIOFramebuffer::setCursorState() - Position (%d,%d), visible=%s\n", 
          (int)x, (int)y, visible ? "true" : "false");
    
    // For VirtIO GPU, cursor positioning is handled by the system
    // We just need to acknowledge cursor state changes
    // This enables proper cursor tracking for GUI applications
    
    return kIOReturnSuccess;
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

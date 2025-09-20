#include "VMVirtIOFramebuffer.h"
#include "VMVirtIOGPU.h"
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
    
    super::free();
}

bool VMVirtIOFramebuffer::start(IOService* provider)
{
    IOLog("VMVirtIOFramebuffer::start() - Starting VirtIO framebuffer v8.0.0d6\n");
    
    // CRITICAL: Do NOT set boot display property - VirtIO GPU should be graphics accelerator, not console
    // Removing: provider->setProperty("AAPL,boot-display", kOSBooleanTrue);
    IOLog("VMVirtIOFramebuffer::start() - VirtIO GPU will NOT be console device - keeping as graphics accelerator\n");
    
    if (!super::start(provider)) {
        IOLog("VMVirtIOFramebuffer::start() - super::start() failed\n");
        return false;
    }
    
    // The provider should be VMVirtIOGPU
    m_gpu_driver = OSDynamicCast(VMVirtIOGPU, provider);
    if (!m_gpu_driver) {
        IOLog("VMVirtIOFramebuffer::start() - Provider is not VMVirtIOGPU\n");
        return false;
    }
    
    // Get PCI device from the provider chain
    IOService* pci_provider = provider->getProvider();
    m_pci_device = OSDynamicCast(IOPCIDevice, pci_provider);
    if (!m_pci_device) {
        IOLog("VMVirtIOFramebuffer::start() - No PCI device available\n");
        // Don't fail - we can work without direct PCI access
    }
    
    // VirtIO GPU uses synthetic framebuffer memory - will be allocated in getApertureRange()
    IOLog("VMVirtIOFramebuffer::start() - VirtIO GPU framebuffer will use synthetic memory allocation\n");
    
    // CRITICAL: Set boot-display property to claim display responsibility
    // This makes VirtIO GPU the primary display provider
    provider->setProperty("AAPL,boot-display", kOSBooleanTrue);
    IOLog("VMVirtIOFramebuffer::start() - VirtIO GPU claimed as primary display provider\n");
    
    // NOTE: isConsoleDevice() will return false to force graphics mode (not console mode)
    
    // CRITICAL: Prevent IONDRVFramebuffer from competing for VirtIO GPU
    // Set IONDRV to disabled so our framebuffer has exclusive control
    provider->setProperty("IONDRV", kOSBooleanFalse);
    IOLog("VMVirtIOFramebuffer::start() - Disabled IONDRV for VirtIO GPU exclusive control\n");
    
    // Also set properties on the PCI device if available
    if (m_pci_device) {
        // CRITICAL: Remove any existing boot-display property first to avoid conflict
        m_pci_device->removeProperty("AAPL,boot-display");
        IOLog("VMVirtIOFramebuffer::start() - Removed existing boot-display from PCI device\n");
        
        // Now set our boot-display property to claim display responsibility
        m_pci_device->setProperty("AAPL,boot-display", kOSBooleanTrue);
        m_pci_device->setProperty("IONDRV", kOSBooleanFalse);
        IOLog("VMVirtIOFramebuffer::start() - Set display properties on PCI device\n");
    }
    
    IOLog("VMVirtIOFramebuffer::start() - Successfully initialized framebuffer\n");
    
    // FORCE: Manually enable the controller since system isn't calling it
    IOLog("VMVirtIOFramebuffer::start() - Forcing enableController() call\n");
    IOReturn enable_result = enableController();
    if (enable_result == kIOReturnSuccess) {
        IOLog("VMVirtIOFramebuffer::start() - enableController() succeeded\n");
    } else {
        IOLog("VMVirtIOFramebuffer::start() - enableController() failed: 0x%x\n", enable_result);
    }
    
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
    
    // For VirtIO GPU, create a synthetic framebuffer memory region
    // VirtIO GPU doesn't use traditional VRAM - it uses host memory buffers
    
    if (m_vram_range) {
        IOLog("VMVirtIOFramebuffer::getApertureRange: Using cached synthetic VRAM range\n");
        m_vram_range->retain();
        return m_vram_range;
    }
    
    // Calculate required framebuffer size for maximum supported resolution
    // Allocate enough for 4K (3840x2160@32bpp) to support all display modes
    size_t framebuffer_size = 3840 * 2160 * 4;  // 32MB for 4K@32bpp
    
    IOLog("VMVirtIOFramebuffer::getApertureRange: Allocating framebuffer for maximum resolution 4K (3840x2160)\n");
    
    // Create synthetic memory region for VirtIO GPU framebuffer
    IOBufferMemoryDescriptor* buffer = IOBufferMemoryDescriptor::withCapacity(
        framebuffer_size, kIODirectionInOut);
    
    if (!buffer) {
        IOLog("VMVirtIOFramebuffer::getApertureRange: Failed to allocate framebuffer memory\n");
        return nullptr;
    }
    
    // For VirtIO GPU, we use the buffer directly as our framebuffer memory
    // Create a simple device memory wrapper around the physical memory
    IOPhysicalAddress buffer_addr = 0;
    IOByteCount buffer_len = buffer->getLength();
    
    // Get the physical address if possible
    IOMemoryMap* memory_map = buffer->map();
    if (memory_map) {
        buffer_addr = memory_map->getPhysicalAddress();
        memory_map->release();
    }
    
    // Create device memory representing our synthetic framebuffer
    IODeviceMemory* device_memory = nullptr;
    if (buffer_addr != 0) {
        device_memory = IODeviceMemory::withRange(buffer_addr, buffer_len);
    }
    
    if (device_memory) {
        m_vram_range = device_memory;
        m_vram_range->retain();
        
        IOLog("VMVirtIOFramebuffer::getApertureRange: Created synthetic VirtIO framebuffer, size: %zu bytes (%zu MB), addr: 0x%llx\n", 
              framebuffer_size, framebuffer_size / (1024 * 1024), (uint64_t)buffer_addr);
        
        // Keep the buffer alive - it contains our actual framebuffer memory
        buffer->retain();
        
        device_memory->retain();
        return device_memory;
    }
    
    // Fallback: Use the buffer descriptor itself if we can't get physical address
    buffer->retain();
    m_vram_range = (IODeviceMemory*)buffer; // Cast for compatibility
    
    IOLog("VMVirtIOFramebuffer::getApertureRange: Using buffer descriptor as framebuffer, size: %zu bytes (%zu MB)\n", 
          framebuffer_size, framebuffer_size / (1024 * 1024));
    
    return (IODeviceMemory*)buffer;
}

const char* VMVirtIOFramebuffer::getPixelFormats(void)
{
    // Return RGB 32-bit pixel format for stable display
    // Format: "--------PPPPPPPP--------PPPPPPPP" where P = supported
    IOLog("VMVirtIOFramebuffer::getPixelFormats() - Returning 32-bit RGB format\n");
    static const char* pixelFormats = "--------PPPPPPPP--------PPPPPPPP";
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

// IOFramebuffer optional overrides
IOReturn VMVirtIOFramebuffer::enableController()
{
    IOLog("VMVirtIOFramebuffer::enableController() - Enabling framebuffer controller\n");
    
    // CRITICAL: Call parent implementation first
    IOReturn result = super::enableController();
    if (result != kIOReturnSuccess) {
        IOLog("VMVirtIOFramebuffer::enableController() - Parent enableController failed: 0x%x\n", result);
        return result;
    }
    
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
    
    // CRITICAL: Tell VirtIO GPU to create and display the framebuffer
    if (m_gpu_driver) {
        IOLog("VMVirtIOFramebuffer::enableController() - Setting up VirtIO GPU display output\n");
        
        // Create VirtIO GPU resource for our framebuffer
        // This connects our synthetic memory to VirtIO GPU's display pipeline
        result = m_gpu_driver->setupDisplayResource(m_width, m_height, m_depth);
        if (result != kIOReturnSuccess) {
            IOLog("VMVirtIOFramebuffer::enableController() - VirtIO GPU display setup failed: 0x%x\n", result);
            IOLog("VMVirtIOFramebuffer::enableController() - Continuing with software framebuffer fallback\n");
            // Continue with software-only framebuffer - this still provides display functionality
        } else {
            IOLog("VMVirtIOFramebuffer::enableController() - VirtIO GPU display setup successful\n");
            
            // Enable VirtIO GPU scanout (this actually shows the display)
            result = m_gpu_driver->enableScanout(0, m_width, m_height);
            if (result != kIOReturnSuccess) {
                IOLog("VMVirtIOFramebuffer::enableController() - VirtIO GPU scanout failed: 0x%x\n", result);
                IOLog("VMVirtIOFramebuffer::enableController() - Display available in software mode\n");
            } else {
                IOLog("VMVirtIOFramebuffer::enableController() - VirtIO GPU scanout enabled\n");
            }
        }
    } else {
        IOLog("VMVirtIOFramebuffer::enableController() - No VirtIO GPU driver, using software framebuffer\n");
    }
    
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

IOItemCount VMVirtIOFramebuffer::getConnectionCount(void)
{
    return 1; // Single display connection
}

bool VMVirtIOFramebuffer::isConsoleDevice(void)
{
    IOLog("VMVirtIOFramebuffer::isConsoleDevice() called - VERSION 8.0.0d9-test\n");
    
    // TEMPORARY TEST: Allow VirtIO GPU to be console device to enable GUI transition
    // This should make the system recognize our framebuffer and enable WindowServer
    IOLog("VMVirtIOFramebuffer::isConsoleDevice() - TESTING: Allowing VirtIO GPU as console to enable GUI\n");
    
    // Set properties to indicate this is the primary display (both console and graphics capable)
    setProperty("IODisplayIsConsole", kOSBooleanTrue);  // TEMPORARY: Allow console
    setProperty("IODisplayPrimary", kOSBooleanTrue);  
    setProperty("IODisplayAccelerated", kOSBooleanTrue);
    setProperty("IOGraphicsAccelerator", kOSBooleanTrue);
    
    IOLog("VMVirtIOFramebuffer::isConsoleDevice() - TESTING: Properties set, returning TRUE to enable GUI\n");
    
    return true; // TEMPORARY: Accept console to enable GUI mode
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
    
    if (!value) {
        IOLog("VMVirtIOFramebuffer::getAttributeForConnection() - NULL value pointer\n");
        return kIOReturnBadArgument;
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
        case kConnectionSupportsAppleSense:
            // DDC/Apple sense support
            *value = 0; // Not supported for VirtIO GPU
            IOLog("VMVirtIOFramebuffer::getAttributeForConnection() - DDC/Apple sense: not supported\n");
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
    IOLog("VMVirtIOFramebuffer::newUserClient() - type=%d\n", (int)type);
    
    if (!clientH) {
        IOLog("VMVirtIOFramebuffer::newUserClient() - NULL client pointer\n");
        return kIOReturnBadArgument;
    }
    
    // For basic Metal/acceleration compatibility, we need to handle the common client types
    switch (type) {
        case kIOAccelSurfaceClientType:
            // Handle accelerator surface client type for Metal compatibility
            IOLog("VMVirtIOFramebuffer::newUserClient() - Handling kIOAccelSurfaceClientType for Metal support\n");
            return super::newUserClient(owningTask, security_id, type, clientH);
            
        default:
            // Try parent implementation for all other types including framebuffer clients
            IOLog("VMVirtIOFramebuffer::newUserClient() - Delegating type %d to parent\n", (int)type);
            return super::newUserClient(owningTask, security_id, type, clientH);
    }
}

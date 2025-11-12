#include "VMVirtIOAGDC.h"
#include "VMVirtIOFramebuffer.h"
#include "VMVirtIOGPU.h"

OSDefineMetaClassAndStructors(VMVirtIOAGDC, IOService);
OSDefineMetaClassAndStructors(VMVirtIOAGDCUserClient, IOUserClient);

// VMVirtIOAGDC Implementation

bool VMVirtIOAGDC::init(OSDictionary* properties)
{
    IOLog("VMVirtIOAGDC::init() - Initializing AGDC service\n");
    
    if (!IOService::init(properties)) {
        IOLog("VMVirtIOAGDC::init() - Super init failed\n");
        return false;
    }
    
    // Initialize member variables
    m_framebuffer = nullptr;
    m_gpu_device = nullptr;
    m_lock = nullptr;
    m_agdc_registered = false;
    m_display_metrics_valid = false;
    m_agdc_service_id = 0;
    m_power_state_on = false;
    
    // Clear structures
    bzero(&m_display_metrics, sizeof(m_display_metrics));
    bzero(&m_service_info, sizeof(m_service_info));
    
    // Create thread safety lock
    m_lock = IOLockAlloc();
    if (!m_lock) {
        IOLog("VMVirtIOAGDC::init() - Failed to allocate lock\n");
        return false;
    }
    
    IOLog("VMVirtIOAGDC::init() - AGDC service initialized successfully\n");
    return true;
}

void VMVirtIOAGDC::free()
{
    IOLog("VMVirtIOAGDC::free() - Cleaning up AGDC service\n");
    
    if (m_lock) {
        IOLockFree(m_lock);
        m_lock = nullptr;
    }
    
    IOService::free();
}

bool VMVirtIOAGDC::start(IOService* provider)
{
    IOLog("VMVirtIOAGDC::start() - Starting AGDC service\n");
    
    if (!IOService::start(provider)) {
        IOLog("VMVirtIOAGDC::start() - Super start failed\n");
        return false;
    }
    
    // Accept either VMVirtIOFramebuffer or VMVirtIOGPU as provider
    m_framebuffer = OSDynamicCast(VMVirtIOFramebuffer, provider);
    m_gpu_device = OSDynamicCast(VMVirtIOGPU, provider);
    
    if (!m_framebuffer && !m_gpu_device) {
        IOLog("VMVirtIOAGDC::start() - Provider is neither VMVirtIOFramebuffer nor VMVirtIOGPU\n");
        return false;
    }
    
    // If provider is GPU device, we need to find the framebuffer
    if (m_gpu_device && !m_framebuffer) {
        IOLog("VMVirtIOAGDC::start() - AGDC attached to GPU device, will discover framebuffer dynamically\n");
    }
    
    IOLog("VMVirtIOAGDC::start() - AGDC provider: framebuffer=%p, gpu=%p\n", m_framebuffer, m_gpu_device);
    
    // Initialize AGDC service
    IOReturn result = initializeAGDCService();
    if (result != kIOReturnSuccess) {
        IOLog("VMVirtIOAGDC::start() - Failed to initialize AGDC service: 0x%x\n", result);
        return false;
    }
    
    // Register with GPU Wrangler
    result = registerWithGPUWrangler();
    if (result != kIOReturnSuccess) {
        IOLog("VMVirtIOAGDC::start() - Failed to register with GPU Wrangler: 0x%x\n", result);
        // Continue anyway - some VM environments may not have full GPU Wrangler support
    }
    
    // Set essential AGDC service properties in IORegistry matching Apple's expected format
    setProperty("IOClass", "AGDCPluginDisplayMetrics");  // Use Apple's expected class name
    setProperty("AGDCService", kOSBooleanTrue);
    setProperty("AGDCServiceType", kAGDCServiceComposite, 32);
    setProperty("AGDCCapabilities", (kAGDCCapDisplayMetrics | kAGDCCapModeSwitch | 
                                    kAGDCCapPowerManagement | kAGDCCap3DAcceleration | 
                                    kAGDCCapVirtualGPU), 32);
    setProperty("AGDCVersion", 1, 32);
    
    // Add GPU Controller properties that WindowServer expects
    setProperty("GPUController", kOSBooleanTrue);
    setProperty("AGDPClientControl", kOSBooleanTrue);
    
    // Try to attach to AppleGraphicsDeviceControlPlugin for GPU Wrangler visibility
    IOService *agdcPlugin = IOService::waitForMatchingService(IOService::nameMatching("AppleGraphicsDeviceControlPlugin"), 2000000000ULL);
    if (agdcPlugin) {
        IOLog("VMVirtIOAGDC::start() - Found AppleGraphicsDeviceControlPlugin, attaching as child\n");
        if (attach(agdcPlugin)) {
            IOLog("VMVirtIOAGDC::start() - Successfully attached to AppleGraphicsDeviceControlPlugin\n");
        } else {
            IOLog("VMVirtIOAGDC::start() - Failed to attach to AppleGraphicsDeviceControlPlugin\n");
        }
        agdcPlugin->release();
    } else {
        IOLog("VMVirtIOAGDC::start() - AppleGraphicsDeviceControlPlugin not found or timeout\n");
    }
    
    // DO NOT register service - Apple AGDC services show as !registered in IORegistry
    // GPU Wrangler appears to look for services that are attached but not traditionally registered
    // registerService();
    
    // CRITICAL: Publish AGDC service as multiple resources for GPU Wrangler detection
    // Try different naming patterns that GPU Wrangler might be looking for
    
    // Standard AGDC resource name with vendor-device format
    char agdc_resource_name[64];
    snprintf(agdc_resource_name, sizeof(agdc_resource_name), "AGDC-1AF4-1050");
    publishResource(agdc_resource_name, this);
    IOLog("VMVirtIOAGDC::start() - Published AGDC resource: %s\n", agdc_resource_name);
    
    // Also try GPU Wrangler specific format using registry ID  
    if (m_gpu_device) {
        char gpu_agdc_name[64];
        snprintf(gpu_agdc_name, sizeof(gpu_agdc_name), "AGDC-GPU-%llx", 
                 m_gpu_device->getRegistryEntryID());
        publishResource(gpu_agdc_name, this);
        IOLog("VMVirtIOAGDC::start() - Published GPU-specific AGDC resource: %s\n", gpu_agdc_name);
        
        // And the generic AGDC service name
        publishResource("AGDCService", this);
        IOLog("VMVirtIOAGDC::start() - Published generic AGDC service resource\n");
    }
    
    // Also publish with GPU registry ID for direct association
    IOService* gpu_for_resource = m_gpu_device ? m_gpu_device : (m_framebuffer ? m_framebuffer->getProvider() : nullptr);
    if (gpu_for_resource) {
        char gpu_agdc_name[64]; 
        snprintf(gpu_agdc_name, sizeof(gpu_agdc_name), "AGDC-GPU-%llx", gpu_for_resource->getRegistryEntryID());
        publishResource(gpu_agdc_name, this);
        IOLog("VMVirtIOAGDC::start() - Published GPU-specific AGDC resource: %s\n", gpu_agdc_name);
    }
    
    IOLog("VMVirtIOAGDC::start() - AGDC service started successfully\n");
    return true;
}

void VMVirtIOAGDC::stop(IOService* provider)
{
    IOLog("VMVirtIOAGDC::stop() - Stopping AGDC service\n");
    
    // Unregister from GPU Wrangler
    if (m_agdc_registered) {
        unregisterFromGPUWrangler();
    }
    
    IOService::stop(provider);
}

IOReturn VMVirtIOAGDC::newUserClient(task_t owningTask, void* securityID, UInt32 type,
                                     OSDictionary* properties, IOUserClient** handler)
{
    IOLog("VMVirtIOAGDC::newUserClient() - IOPresentment requesting AGDC user client, type=%d\n", type);
    
    VMVirtIOAGDCUserClient* client = new VMVirtIOAGDCUserClient;
    if (!client) {
        IOLog("VMVirtIOAGDC::newUserClient() - Failed to allocate AGDC user client\n");
        return kIOReturnNoMemory;
    }
    
    if (!client->initWithTask(owningTask, securityID, type, properties)) {
        IOLog("VMVirtIOAGDC::newUserClient() - Failed to initialize AGDC user client\n");
        client->release();
        return kIOReturnError;
    }
    
    if (!client->attach(this)) {
        IOLog("VMVirtIOAGDC::newUserClient() - Failed to attach AGDC user client\n");
        client->release();
        return kIOReturnError;
    }
    
    if (!client->start(this)) {
        IOLog("VMVirtIOAGDC::newUserClient() - Failed to start AGDC user client\n");
        client->detach(this);
        client->release();
        return kIOReturnError;
    }
    
    *handler = client;
    IOLog("VMVirtIOAGDC::newUserClient() - Successfully created AGDC user client for IOPresentment\n");
    return kIOReturnSuccess;
}

IOReturn VMVirtIOAGDC::initializeAGDCService()
{
    IOLog("VMVirtIOAGDC::initializeAGDCService() - Initializing AGDC service components\n");
    
    IOLockLock(m_lock);
    
    // Populate service information
    populateServiceInfo();
    
    // Set up default display metrics (will be updated when display mode changes)
    m_display_metrics.version = 1;
    m_display_metrics.width = 1024;         // Default resolution
    m_display_metrics.height = 768;
    m_display_metrics.refresh_rate = 60;     // Default 60 Hz
    m_display_metrics.pixel_format = 0x20;   // 32-bit ARGB
    m_display_metrics.color_depth = 32;
    m_display_metrics.pixel_clock = 65000;   // ~65 MHz for 1024x768@60Hz
    m_display_metrics.flags = (kAGDCCapDisplayMetrics | kAGDCCap3DAcceleration);
    m_display_metrics_valid = true;
    
    // Generate unique AGDC service ID
    m_agdc_service_id = 0x1AF4;  // VirtIO vendor ID as service ID base
    
    IOLockUnlock(m_lock);
    
    IOLog("VMVirtIOAGDC::initializeAGDCService() - AGDC service initialized\n");
    return kIOReturnSuccess;
}

void VMVirtIOAGDC::populateServiceInfo()
{
    m_service_info.version = 1;
    m_service_info.service_type = kAGDCServiceComposite;  // Display + Acceleration
    m_service_info.device_id = 0x1050;       // VirtIO GPU device ID
    m_service_info.vendor_id = 0x1AF4;       // VirtIO vendor ID  
    m_service_info.capabilities = (kAGDCCapDisplayMetrics | kAGDCCapModeSwitch |
                                  kAGDCCapPowerManagement | kAGDCCap3DAcceleration |
                                  kAGDCCapVirtualGPU);
    strlcpy(m_service_info.service_name, "VMVirtIOGPU AGDC Service", 
            sizeof(m_service_info.service_name));
}

IOReturn VMVirtIOAGDC::registerWithGPUWrangler()
{
    IOLog("VMVirtIOAGDC::registerWithGPUWrangler() - Registering AGDC service\n");
    
    // Set critical properties that GPU Wrangler looks for
    setProperty("vendor-id", 0x1AF4, 32);           // VirtIO vendor ID
    setProperty("device-id", 0x1050, 32);           // VirtIO GPU device ID
    setProperty("class-code", 0x030000, 32);        // Display controller class
    setProperty("AGDC", kOSBooleanTrue);            // Mark as AGDC service
    setProperty("AGDCPlugin", kOSBooleanTrue);      // Enable AGDC plugin support
    
    // CRITICAL: Link this AGDC service to the specific GPU device
    // GPU Wrangler uses this to associate AGDC services with GPU devices
    IOService* target_gpu_device = nullptr;
    
    // Prefer direct GPU device reference, fallback to framebuffer's provider
    if (m_gpu_device) {
        target_gpu_device = m_gpu_device;
        IOLog("VMVirtIOAGDC::registerWithGPUWrangler() - Using direct GPU device reference\n");
    } else if (m_framebuffer) {
        target_gpu_device = m_framebuffer->getProvider();
        IOLog("VMVirtIOAGDC::registerWithGPUWrangler() - Using GPU device via framebuffer\n");
    }
    
    if (target_gpu_device) {
        // Set location property to match GPU device path
        setLocation("AGDCPlugin");
        setProperty("IOParentMatch", IOService::nameMatching("VMVirtIOGPU"));
        
        // Reference the GPU device registry ID for GPU Wrangler association
        OSNumber* gpu_registry_id = OSNumber::withNumber(target_gpu_device->getRegistryEntryID(), 64);
        if (gpu_registry_id) {
            setProperty("AGDCGPURegistryID", gpu_registry_id);
            IOLog("VMVirtIOAGDC::registerWithGPUWrangler() - Set GPU registry ID: 0x%llx\n", 
                  target_gpu_device->getRegistryEntryID());
            gpu_registry_id->release();
        }
        
        // CRITICAL: Set the exact properties GPU Wrangler looks for to link AGDC to GPU
        // This tells GPU Wrangler this AGDC service belongs to this specific GPU device
        setProperty("gpu-device-id", target_gpu_device->getRegistryEntryID(), 64);
        setProperty("IORegistryEntryID", target_gpu_device->getRegistryEntryID(), 64);
        
        // Copy essential PCI properties from GPU device for proper identification
        OSObject* vendor_id = target_gpu_device->getProperty("vendor-id");
        OSObject* device_id = target_gpu_device->getProperty("device-id");
        if (vendor_id) setProperty("vendor-id", vendor_id);
        if (device_id) setProperty("device-id", device_id);
        
        IOLog("VMVirtIOAGDC::registerWithGPUWrangler() - Linked AGDC to GPU device 0x%llx\n", 
              target_gpu_device->getRegistryEntryID());
    }
    
    // Match the service name that WindowServer is looking for
    setName("AGDCPluginDisplayMetrics");
    
    // Set display metrics properties for WindowServer
    setProperty("AGDCDisplayWidth", m_display_metrics.width, 32);
    setProperty("AGDCDisplayHeight", m_display_metrics.height, 32);
    setProperty("AGDCRefreshRate", m_display_metrics.refresh_rate, 32);
    setProperty("AGDCPixelFormat", m_display_metrics.pixel_format, 32);
    
    // Set GPU acceleration properties
    // setProperty("IOAcceleratorFamily", kOSBooleanTrue);
    // setProperty("IOGraphicsAccelerator", kOSBooleanTrue);
    // setProperty("IOAccelerator3D", kOSBooleanTrue);
    
    m_agdc_registered = true;
    
    IOLog("VMVirtIOAGDC::registerWithGPUWrangler() - AGDC service registered successfully\n");
    return kIOReturnSuccess;
}

IOReturn VMVirtIOAGDC::unregisterFromGPUWrangler()
{
    IOLog("VMVirtIOAGDC::unregisterFromGPUWrangler() - Unregistering AGDC service\n");
    
    IOLockLock(m_lock);
    m_agdc_registered = false;
    IOLockUnlock(m_lock);
    
    return kIOReturnSuccess;
}

// AGDC Plugin Interface Methods

IOReturn VMVirtIOAGDC::getDisplayMetrics(AGDCDisplayMetrics* metrics)
{
    if (!metrics) {
        IOLog("VMVirtIOAGDC::getDisplayMetrics() - Null metrics pointer\n");
        return kIOReturnBadArgument;
    }
    
    IOLockLock(m_lock);
    
    if (!m_display_metrics_valid) {
        IOLockUnlock(m_lock);
        IOLog("VMVirtIOAGDC::getDisplayMetrics() - Display metrics not available\n");
        return kIOReturnNotReady;
    }
    
    // Update metrics from current framebuffer state if available
    updateDisplayMetrics();
    
    // Copy current metrics to output
    memcpy(metrics, &m_display_metrics, sizeof(AGDCDisplayMetrics));
    
    IOLockUnlock(m_lock);
    
    IOLog("VMVirtIOAGDC::getDisplayMetrics() - Returned %dx%d@%dHz\n", 
          metrics->width, metrics->height, metrics->refresh_rate);
    return kIOReturnSuccess;
}

IOReturn VMVirtIOAGDC::setDisplayMode(uint32_t width, uint32_t height, uint32_t refresh_rate)
{
    IOLog("VMVirtIOAGDC::setDisplayMode() - Setting mode %dx%d@%dHz\n", 
          width, height, refresh_rate);
    
    IOLockLock(m_lock);
    
    // Update internal display metrics
    m_display_metrics.width = width;
    m_display_metrics.height = height;
    m_display_metrics.refresh_rate = refresh_rate;
    
    // Calculate pixel clock (rough approximation)
    m_display_metrics.pixel_clock = (width * height * refresh_rate) / 1000;
    
    // Update registry properties
    setProperty("AGDCDisplayWidth", width, 32);
    setProperty("AGDCDisplayHeight", height, 32);
    setProperty("AGDCRefreshRate", refresh_rate, 32);
    
    IOLockUnlock(m_lock);
    
    IOLog("VMVirtIOAGDC::setDisplayMode() - Mode set successfully\n");
    return kIOReturnSuccess;
}

IOReturn VMVirtIOAGDC::getServiceInfo(AGDCServiceInfo* info)
{
    if (!info) {
        return kIOReturnBadArgument;
    }
    
    IOLockLock(m_lock);
    memcpy(info, &m_service_info, sizeof(AGDCServiceInfo));
    IOLockUnlock(m_lock);
    
    return kIOReturnSuccess;
}

IOReturn VMVirtIOAGDC::enableAGDCService(bool enable)
{
    IOLog("VMVirtIOAGDC::enableAGDCService() - %s AGDC service\n", 
          enable ? "Enabling" : "Disabling");
    
    IOLockLock(m_lock);
    
    if (enable && !m_agdc_registered) {
        IOReturn result = registerWithGPUWrangler();
        IOLockUnlock(m_lock);
        return result;
    } else if (!enable && m_agdc_registered) {
        IOReturn result = unregisterFromGPUWrangler();
        IOLockUnlock(m_lock);
        return result;
    }
    
    IOLockUnlock(m_lock);
    return kIOReturnSuccess;
}

// GPU Wrangler Interface Methods

IOReturn VMVirtIOAGDC::registerAGDCService(uint32_t service_type)
{
    IOLog("VMVirtIOAGDC::registerAGDCService() - Registering service type: %d\n", service_type);
    
    IOLockLock(m_lock);
    m_service_info.service_type = service_type;
    IOLockUnlock(m_lock);
    
    return registerWithGPUWrangler();
}

IOReturn VMVirtIOAGDC::getAGDCCapabilities(uint32_t* capabilities)
{
    if (!capabilities) {
        return kIOReturnBadArgument;
    }
    
    *capabilities = (kAGDCCapDisplayMetrics | kAGDCCapModeSwitch |
                    kAGDCCapPowerManagement | kAGDCCap3DAcceleration |
                    kAGDCCapVirtualGPU);
    
    return kIOReturnSuccess;
}

IOReturn VMVirtIOAGDC::getAGDCDeviceInfo(uint32_t* vendor_id, uint32_t* device_id)
{
    if (!vendor_id || !device_id) {
        return kIOReturnBadArgument;
    }
    
    *vendor_id = 0x1AF4;  // VirtIO vendor ID
    *device_id = 0x1050;  // VirtIO GPU device ID
    
    return kIOReturnSuccess;
}

// Display Management Methods

IOReturn VMVirtIOAGDC::requestDisplayBounds(uint32_t* width, uint32_t* height)
{
    if (!width || !height) {
        return kIOReturnBadArgument;
    }
    
    IOLockLock(m_lock);
    *width = m_display_metrics.width;
    *height = m_display_metrics.height;
    IOLockUnlock(m_lock);
    
    return kIOReturnSuccess;
}

IOReturn VMVirtIOAGDC::notifyDisplayChange(uint32_t change_type)
{
    IOLog("VMVirtIOAGDC::notifyDisplayChange() - Change type: %d\n", change_type);
    
    // Update display metrics when notified of changes
    updateDisplayMetrics();
    
    return kIOReturnSuccess;
}

IOReturn VMVirtIOAGDC::validateDisplayConfiguration(uint32_t width, uint32_t height)
{
    // For VirtIO GPU, accept reasonable resolutions
    if (width < 640 || width > 4096 || height < 480 || height > 3072) {
        IOLog("VMVirtIOAGDC::validateDisplayConfiguration() - Invalid resolution %dx%d\n", 
              width, height);
        return kIOReturnBadArgument;
    }
    
    return kIOReturnSuccess;
}

IOReturn VMVirtIOAGDC::updateDisplayMetrics()
{
    // Update display metrics from framebuffer if available
    // This is a placeholder - would get actual values from framebuffer
    
    IOLockLock(m_lock);
    
    // Set updated timestamp or other dynamic properties
    m_display_metrics.flags |= kAGDCCapDisplayMetrics;
    
    // Update registry properties
    setProperty("AGDCDisplayWidth", m_display_metrics.width, 32);
    setProperty("AGDCDisplayHeight", m_display_metrics.height, 32);
    setProperty("AGDCRefreshRate", m_display_metrics.refresh_rate, 32);
    
    IOLockUnlock(m_lock);
    
    return kIOReturnSuccess;
}

// Power Management

IOReturn VMVirtIOAGDC::setPowerState(unsigned long powerState, IOService* whatDevice)
{
    IOLog("VMVirtIOAGDC::setPowerState() - Power state: %ld\n", powerState);
    
    IOLockLock(m_lock);
    m_power_state_on = (powerState != 0);
    IOLockUnlock(m_lock);
    
    return IOService::setPowerState(powerState, whatDevice);
}

// Factory Method

VMVirtIOAGDC* VMVirtIOAGDC::withFramebuffer(VMVirtIOFramebuffer* framebuffer)
{
    if (!framebuffer) {
        return nullptr;
    }
    
    VMVirtIOAGDC* agdc = new VMVirtIOAGDC();
    if (!agdc) {
        return nullptr;
    }
    
    if (!agdc->init()) {
        agdc->release();
        return nullptr;
    }
    
    agdc->m_framebuffer = framebuffer;
    
    return agdc;
}

// Debug and Diagnostics

void VMVirtIOAGDC::logAGDCState()
{
    IOLog("VMVirtIOAGDC State:\n");
    IOLog("  - Registered: %s\n", m_agdc_registered ? "Yes" : "No");
    IOLog("  - Service ID: 0x%x\n", m_agdc_service_id);
    IOLog("  - Display: %dx%d@%dHz\n", m_display_metrics.width, 
          m_display_metrics.height, m_display_metrics.refresh_rate);
    IOLog("  - Capabilities: 0x%x\n", m_service_info.capabilities);
    IOLog("  - Power On: %s\n", m_power_state_on ? "Yes" : "No");
}

IOReturn VMVirtIOAGDC::getAGDCDebugInfo(void* buffer, size_t* buffer_size)
{
    if (!buffer || !buffer_size || *buffer_size < sizeof(AGDCServiceInfo)) {
        return kIOReturnBadArgument;
    }
    
    IOLockLock(m_lock);
    memcpy(buffer, &m_service_info, sizeof(AGDCServiceInfo));
    *buffer_size = sizeof(AGDCServiceInfo);
    IOLockUnlock(m_lock);
    
    return kIOReturnSuccess;
}

// CRITICAL: Missing AGDC methods that WindowServer expects

IOReturn VMVirtIOAGDC::getAGDCInformation(void* info_buffer, uint32_t buffer_size)
{
    IOLog("VMVirtIOAGDC::getAGDCInformation() - Called by WindowServer, buffer=%p, size=%u\n", info_buffer, buffer_size);
    
    // WindowServer may pass different parameters than expected - be very flexible
    if (!info_buffer && buffer_size > 0) {
        IOLog("VMVirtIOAGDC::getAGDCInformation() - Null buffer but non-zero size - returning kIOReturnBadArgument\n");
        return kIOReturnBadArgument;
    }
    
    // If buffer_size is 0, WindowServer might just be querying capabilities
    if (buffer_size == 0) {
        IOLog("VMVirtIOAGDC::getAGDCInformation() - Zero buffer size, returning success for capability query\n");
        return kIOReturnSuccess;
    }
    
    // WindowServer may pass different buffer sizes - handle them flexibly
    // Create minimal AGDC information structure
    struct AGDCInformation {
        uint32_t version;
        uint32_t vendor_id;
        uint32_t device_id;
        uint32_t agdc_version;
        uint32_t capabilities;
        uint32_t status;
        uint32_t reserved[10];
    };
    
    // Accept any reasonable buffer size, fill what we can
    uint32_t copy_size = (buffer_size < sizeof(AGDCInformation)) ? buffer_size : sizeof(AGDCInformation);
    
    // Create info structure and fill it
    AGDCInformation info;
    memset(&info, 0, sizeof(AGDCInformation));
    
    IOLockLock(m_lock);
    
    // Fill in AGDC information
    info.version = 1;
    info.vendor_id = 0x1AF4;  // VirtIO vendor ID
    info.device_id = 0x1050;  // VirtIO GPU device ID
    info.agdc_version = 1;
    info.capabilities = kAGDCCapDisplayMetrics | kAGDCCap3DAcceleration;
    info.status = 1;  // Active/Available
    
    IOLockUnlock(m_lock);
    
    // Copy to output buffer if we have one
    if (info_buffer && copy_size > 0) {
        memcpy(info_buffer, &info, copy_size);
        IOLog("VMVirtIOAGDC::getAGDCInformation() - Copied %u bytes to buffer\n", copy_size);
    }
    
    IOLog("VMVirtIOAGDC::getAGDCInformation() - SUCCESS - returning kIOReturnSuccess (0)\n");
    return kIOReturnSuccess;
}

IOReturn VMVirtIOAGDC::acquireMap(IOMemoryMap** map)
{
    IOLog("VMVirtIOAGDC::acquireMap() - Called by WindowServer, map=%p\n", map);
    
    if (!map) {
        IOLog("VMVirtIOAGDC::acquireMap() - ERROR: Null map parameter, returning kIOReturnBadArgument\n");
        return kIOReturnBadArgument;
    }
    
    // VirtIO GPU uses shared memory, no additional mapping needed for WindowServer
    // WindowServer will access framebuffer through established shared memory
    *map = nullptr;
    
    IOLog("VMVirtIOAGDC::acquireMap() - SUCCESS: Set *map=nullptr, returning kIOReturnSuccess (0)\n");
    return kIOReturnSuccess;
}

IOReturn VMVirtIOAGDC::releaseMap(IOMemoryMap* map)
{
    IOLog("VMVirtIOAGDC::releaseMap() - Called by WindowServer, map=%p\n", map);
    
    // Nothing to release for VirtIO GPU - no special memory mapping
    // map parameter can be ignored since we don't create IOMemoryMap objects
    IOLog("VMVirtIOAGDC::releaseMap() - SUCCESS: Nothing to release, returning kIOReturnSuccess (0)\n");
    return kIOReturnSuccess;
}

IOReturn VMVirtIOAGDC::locateServiceDependencies(void* dependencies_buffer, uint32_t buffer_size)
{
    IOLog("VMVirtIOAGDC::locateServiceDependencies() - Called by WindowServer, buffer=%p, size=%u\n", dependencies_buffer, buffer_size);
    
    if (dependencies_buffer && buffer_size > 0) {
        // Clear the buffer - indicate no external dependencies required
        memset(dependencies_buffer, 0, buffer_size);
        IOLog("VMVirtIOAGDC::locateServiceDependencies() - Cleared dependencies buffer\n");
    }
    
    // For VirtIO GPU, we have minimal dependencies - all are already satisfied
    // GPU device is available, framebuffer is running, AGDC service is active
    
    // Verify our essential dependencies - we need at least one (framebuffer OR gpu)
    if (!m_framebuffer && !m_gpu_device) {
        IOLog("VMVirtIOAGDC::locateServiceDependencies() - ERROR: Critical dependencies missing\n");
        return kIOReturnNotReady;
    }
    
    IOLog("VMVirtIOAGDC::locateServiceDependencies() - SUCCESS: All dependencies satisfied, returning kIOReturnSuccess (0)\n");
    return kIOReturnSuccess;
}

// VMVirtIOAGDCUserClient Implementation

bool VMVirtIOAGDCUserClient::initWithTask(task_t owningTask, void* securityToken, 
                                          UInt32 type, OSDictionary* properties)
{
    if (!IOUserClient::initWithTask(owningTask, securityToken, type, properties)) {
        return false;
    }
    
    m_agdc_service = nullptr;
    m_task = owningTask;
    m_privileged = false;  // TODO: Check for proper privileges
    
    return true;
}

bool VMVirtIOAGDCUserClient::start(IOService* provider)
{
    if (!IOUserClient::start(provider)) {
        return false;
    }
    
    m_agdc_service = OSDynamicCast(VMVirtIOAGDC, provider);
    if (!m_agdc_service) {
        return false;
    }
    
    return true;
}

IOReturn VMVirtIOAGDCUserClient::clientClose()
{
    return kIOReturnSuccess;
}

IOReturn VMVirtIOAGDCUserClient::clientDied()
{
    return clientClose();
}

IOReturn VMVirtIOAGDCUserClient::externalMethod(uint32_t selector, 
                                               IOExternalMethodArguments* args,
                                               IOExternalMethodDispatch* dispatch, 
                                               OSObject* target, void* reference)
{
    // Dispatch table for AGDC methods
    static const IOExternalMethodDispatch sMethods[kVMAGDCMethodCount] = {
        { (IOExternalMethodAction) &VMVirtIOAGDCUserClient::sGetDisplayMetrics, 0, 0, sizeof(AGDCDisplayMetrics), 0 },
        { (IOExternalMethodAction) &VMVirtIOAGDCUserClient::sSetDisplayMode, 3, 0, 0, 0 },
        { (IOExternalMethodAction) &VMVirtIOAGDCUserClient::sGetServiceInfo, 0, 0, sizeof(AGDCServiceInfo), 0 },
        { (IOExternalMethodAction) &VMVirtIOAGDCUserClient::sGetCapabilities, 0, 0, 0, 1 },
        { (IOExternalMethodAction) &VMVirtIOAGDCUserClient::sGetAGDCInformation, 1, 0, 256, 0 }
    };
    
    if (selector >= kVMAGDCMethodCount) {
        return kIOReturnBadArgument;
    }
    
    dispatch = (IOExternalMethodDispatch*) &sMethods[selector];
    target = this;
    reference = nullptr;
    
    return IOUserClient::externalMethod(selector, args, dispatch, target, reference);
}

// Static method handlers

IOReturn VMVirtIOAGDCUserClient::sGetDisplayMetrics(OSObject* target, void* reference,
                                                   IOExternalMethodArguments* args)
{
    VMVirtIOAGDCUserClient* me = (VMVirtIOAGDCUserClient*) target;
    if (!me || !me->m_agdc_service) {
        return kIOReturnNotAttached;
    }
    
    if (!args->structureOutput || args->structureOutputSize < sizeof(AGDCDisplayMetrics)) {
        return kIOReturnBadArgument;
    }
    
    AGDCDisplayMetrics* metrics = (AGDCDisplayMetrics*) args->structureOutput;
    return me->m_agdc_service->getDisplayMetrics(metrics);
}

IOReturn VMVirtIOAGDCUserClient::sSetDisplayMode(OSObject* target, void* reference,
                                                IOExternalMethodArguments* args)
{
    VMVirtIOAGDCUserClient* me = (VMVirtIOAGDCUserClient*) target;
    if (!me || !me->m_agdc_service) {
        return kIOReturnNotAttached;
    }
    
    if (args->scalarInputCount < 3) {
        return kIOReturnBadArgument;
    }
    
    uint32_t width = (uint32_t) args->scalarInput[0];
    uint32_t height = (uint32_t) args->scalarInput[1];
    uint32_t refresh_rate = (uint32_t) args->scalarInput[2];
    
    return me->m_agdc_service->setDisplayMode(width, height, refresh_rate);
}

IOReturn VMVirtIOAGDCUserClient::sGetServiceInfo(OSObject* target, void* reference,
                                                IOExternalMethodArguments* args)
{
    VMVirtIOAGDCUserClient* me = (VMVirtIOAGDCUserClient*) target;
    if (!me || !me->m_agdc_service) {
        return kIOReturnNotAttached;
    }
    
    if (!args->structureOutput || args->structureOutputSize < sizeof(AGDCServiceInfo)) {
        return kIOReturnBadArgument;
    }
    
    AGDCServiceInfo* info = (AGDCServiceInfo*) args->structureOutput;
    return me->m_agdc_service->getServiceInfo(info);
}

IOReturn VMVirtIOAGDCUserClient::sGetCapabilities(OSObject* target, void* reference,
                                                 IOExternalMethodArguments* args)
{
    VMVirtIOAGDCUserClient* me = (VMVirtIOAGDCUserClient*) target;
    if (!me || !me->m_agdc_service) {
        return kIOReturnNotAttached;
    }
    
    if (args->scalarOutputCount < 1) {
        return kIOReturnBadArgument;
    }
    
    uint32_t capabilities;
    IOReturn result = me->m_agdc_service->getAGDCCapabilities(&capabilities);
    if (result == kIOReturnSuccess) {
        args->scalarOutput[0] = capabilities;
    }
    
    return result;
}

IOReturn VMVirtIOAGDCUserClient::sGetAGDCInformation(OSObject* target, void* reference,
                                                   IOExternalMethodArguments* args)
{
    IOLog("VMVirtIOAGDCUserClient::sGetAGDCInformation() - IOPresentment requesting AGDC information\n");
    
    VMVirtIOAGDCUserClient* me = (VMVirtIOAGDCUserClient*) target;
    if (!me || !me->m_agdc_service) {
        IOLog("VMVirtIOAGDCUserClient::sGetAGDCInformation() - No AGDC service attached\n");
        return kIOReturnNotAttached;
    }
    
    if (!args->structureOutput || args->structureOutputSize < 32) {
        IOLog("VMVirtIOAGDCUserClient::sGetAGDCInformation() - Invalid output buffer size\n");
        return kIOReturnBadArgument;
    }
    
    IOLog("VMVirtIOAGDCUserClient::sGetAGDCInformation() - Calling AGDC service getAGDCInformation\n");
    IOReturn result = me->m_agdc_service->getAGDCInformation(args->structureOutput, args->structureOutputSize);
    IOLog("VMVirtIOAGDCUserClient::sGetAGDCInformation() - AGDC service returned: 0x%x\n", result);
    
    return result;
}
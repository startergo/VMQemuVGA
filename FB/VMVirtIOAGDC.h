#ifndef __VMVirtIOAGDC_H__
#define __VMVirtIOAGDC_H__

#include <IOKit/IOService.h>
#include <IOKit/IOUserClient.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOTypes.h>
#include <IOKit/graphics/IOGraphicsTypes.h>

// Forward declarations
class VMVirtIOFramebuffer;
class VMVirtIOGPU;

// AGDC Plugin Display Metrics structure - matches Apple's AGDC interface
struct AGDCDisplayMetrics {
    uint32_t version;           // Version of this structure
    uint32_t width;             // Display width in pixels  
    uint32_t height;            // Display height in pixels
    uint32_t refresh_rate;      // Refresh rate in Hz
    uint32_t pixel_format;      // Pixel format (ARGB, etc.)
    uint32_t color_depth;       // Bits per pixel
    uint32_t pixel_clock;       // Pixel clock in kHz
    uint32_t flags;             // Display capability flags
    uint32_t reserved[8];       // Reserved for future use
};

// AGDC Service Registration structure  
struct AGDCServiceInfo {
    uint32_t version;           // Service version
    uint32_t service_type;      // Type of AGDC service (display, accelerator, etc.)
    uint32_t device_id;         // GPU device ID
    uint32_t vendor_id;         // GPU vendor ID  
    uint32_t capabilities;      // Service capabilities mask
    char service_name[64];      // Human readable service name
    uint32_t reserved[16];      // Reserved for extension
};

// VMVirtIOAGDC - Apple Graphics Device Control implementation for VirtIO GPU
class VMVirtIOAGDC : public IOService
{
    OSDeclareDefaultStructors(VMVirtIOAGDC);

private:
    VMVirtIOFramebuffer* m_framebuffer;     // Parent framebuffer driver
    VMVirtIOGPU* m_gpu_device;              // VirtIO GPU device
    IOLock* m_lock;                         // Thread safety lock
    
    // AGDC service state
    bool m_agdc_registered;                 // Is AGDC service registered?
    bool m_display_metrics_valid;           // Are display metrics available?
    AGDCDisplayMetrics m_display_metrics;   // Current display configuration
    AGDCServiceInfo m_service_info;         // AGDC service information
    
    // Internal state management
    uint32_t m_agdc_service_id;             // Unique AGDC service ID
    bool m_power_state_on;                  // Power management state
    
    // Internal methods
    IOReturn initializeAGDCService();
    IOReturn registerWithGPUWrangler();
    IOReturn unregisterFromGPUWrangler();
    IOReturn updateDisplayMetrics();
    void populateServiceInfo();
    
public:
    // IOService overrides
    virtual bool init(OSDictionary* properties = nullptr) override;
    virtual void free() override;
    virtual bool start(IOService* provider) override;  
    virtual void stop(IOService* provider) override;
    virtual IOReturn newUserClient(task_t owningTask, void* securityID, UInt32 type,
                                   OSDictionary* properties, IOUserClient** handler) override;
    
    // AGDC Plugin Interface - these are the methods WindowServer expects
    virtual IOReturn getDisplayMetrics(AGDCDisplayMetrics* metrics);
    virtual IOReturn setDisplayMode(uint32_t width, uint32_t height, uint32_t refresh_rate);
    virtual IOReturn getServiceInfo(AGDCServiceInfo* info);
    virtual IOReturn enableAGDCService(bool enable);
    
    // CRITICAL: Missing methods that WindowServer is calling (Apple AGDC Interface)
    virtual IOReturn getAGDCInformation(void* info_buffer, uint32_t buffer_size);
    virtual IOReturn acquireMap(IOMemoryMap** map);
    virtual IOReturn releaseMap(IOMemoryMap* map);
    virtual IOReturn locateServiceDependencies(void* dependencies_buffer, uint32_t buffer_size);
    
    // GPU Wrangler Interface - methods called by GPU Wrangler for service discovery
    virtual IOReturn registerAGDCService(uint32_t service_type);
    virtual IOReturn getAGDCCapabilities(uint32_t* capabilities);
    virtual IOReturn getAGDCDeviceInfo(uint32_t* vendor_id, uint32_t* device_id);
    
    // Display management methods
    virtual IOReturn requestDisplayBounds(uint32_t* width, uint32_t* height);
    virtual IOReturn notifyDisplayChange(uint32_t change_type);
    virtual IOReturn validateDisplayConfiguration(uint32_t width, uint32_t height);
    
    // Power management
    virtual IOReturn setPowerState(unsigned long powerState, IOService* whatDevice) override;
    
    // Factory method for easy creation
    static VMVirtIOAGDC* withFramebuffer(VMVirtIOFramebuffer* framebuffer);
    
    // Accessors
    VMVirtIOFramebuffer* getFramebuffer() const { return m_framebuffer; }
    VMVirtIOGPU* getGPUDevice() const { return m_gpu_device; }
    bool isAGDCRegistered() const { return m_agdc_registered; }
    
    // Debug and diagnostics
    void logAGDCState();
    IOReturn getAGDCDebugInfo(void* buffer, size_t* buffer_size);
};

// VMVirtIOAGDCUserClient - User space interface for AGDC services
class VMVirtIOAGDCUserClient : public IOUserClient
{
    OSDeclareDefaultStructors(VMVirtIOAGDCUserClient);

private:
    VMVirtIOAGDC* m_agdc_service;
    task_t m_task;
    bool m_privileged;                      // Does client have AGDC privileges?
    
public:
    virtual bool initWithTask(task_t owningTask, void* securityToken, UInt32 type,
                             OSDictionary* properties) override;
    virtual bool start(IOService* provider) override;
    virtual IOReturn clientClose() override;
    virtual IOReturn clientDied() override;
    
    // Method dispatch table for user space calls
    virtual IOReturn externalMethod(uint32_t selector, IOExternalMethodArguments* args,
                                   IOExternalMethodDispatch* dispatch, OSObject* target,
                                   void* reference) override;
    
    // Static method handlers
    static IOReturn sGetDisplayMetrics(OSObject* target, void* reference,
                                     IOExternalMethodArguments* args);
    static IOReturn sSetDisplayMode(OSObject* target, void* reference,
                                  IOExternalMethodArguments* args);
    static IOReturn sGetServiceInfo(OSObject* target, void* reference,
                                  IOExternalMethodArguments* args);
    static IOReturn sGetCapabilities(OSObject* target, void* reference,
                                   IOExternalMethodArguments* args);
    static IOReturn sGetAGDCInformation(OSObject* target, void* reference,
                                      IOExternalMethodArguments* args);
};

// Method selectors for AGDC user client
enum VMVirtIOAGDCMethods {
    kVMAGDCGetDisplayMetrics = 0,
    kVMAGDCSetDisplayMode,
    kVMAGDCGetServiceInfo,
    kVMAGDCGetCapabilities,
    kVMAGDCGetAGDCInformation,
    kVMAGDCMethodCount
};

// AGDC service types
enum AGDCServiceType {
    kAGDCServiceDisplay = 1,
    kAGDCServiceAccelerator = 2,
    kAGDCServiceComposite = 3
};

// AGDC capability flags
enum AGDCCapabilities {
    kAGDCCapDisplayMetrics = 0x00000001,
    kAGDCCapModeSwitch = 0x00000002,
    kAGDCCapPowerManagement = 0x00000004,
    kAGDCCap3DAcceleration = 0x00000008,
    kAGDCCapVirtualGPU = 0x00000010
};

#endif /* __VMVirtIOAGDC_H__ */
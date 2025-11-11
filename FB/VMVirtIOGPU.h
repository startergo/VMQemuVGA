#ifndef __VMVirtIOGPU_H__
#define __VMVirtIOGPU_H__

#include <IOKit/IOService.h>
#include <IOKit/IOCommandGate.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/pci/IOPCIDevice.h>
#include <IOKit/graphics/IODisplay.h>
#include <IOKit/graphics/IOFramebuffer.h>
#include <IOKit/graphics/IOAccelerator.h>
#include <IOKit/IOUserClient.h>
#include "virtio_gpu.h"
#include "VMQemuVGAAccelerator.h"

#define VIRTIO_GPU_QUEUE_CONTROL    0
#define VIRTIO_GPU_QUEUE_CURSOR     1

// VirtIO GPU feature flags are defined in virtio_gpu.h
// No need to redefine them here

// Forward declarations
class VMVirtIOGPUAccelerator;
class VMMetalPlugin;

// MUST inherit from IOAccelerator for WindowServer to create IOAccelerationUserClient
// We provide minimal stub implementation to satisfy WindowServer without actual Metal support
class VMVirtIOGPU : public IOAccelerator
{
    OSDeclareDefaultStructors(VMVirtIOGPU);

private:
    IOPCIDevice* m_pci_device;
    IOMemoryMap* m_config_map;
    IOMemoryMap* m_notify_map;
    uint32_t m_notify_offset;  // Offset within notify BAR for VirtIO notifications
    IOCommandGate* m_command_gate;
    
    // VirtIO transport device handle
    IOService* m_virtio_device;
    
    // VirtIO GPU configuration
    uint32_t m_max_scanouts;
    uint32_t m_num_capsets;
    uint64_t m_fence_id;    // VirtIO 1.2: Fence ID counter for command synchronization
    
    // Command queue management
    IOBufferMemoryDescriptor* m_control_queue;
    IOBufferMemoryDescriptor* m_cursor_queue;
    uint32_t m_control_queue_size;
    uint32_t m_cursor_queue_size;
    
    // GPU resources
    struct gpu_resource {
        uint32_t resource_id;
        uint32_t width;
        uint32_t height;
        uint32_t format;
        IOMemoryDescriptor* backing_memory;
        bool is_3d;
    };
    
    OSArray* m_resources;
    uint32_t m_next_resource_id;
    uint32_t m_display_resource_id;  // Resource ID for primary display
    
    // 3D context management
    struct gpu_3d_context {
        uint32_t context_id;
        uint32_t resource_id;
        bool active;
        IOMemoryDescriptor* command_buffer;
    };
    
    OSArray* m_contexts;
    uint32_t m_next_context_id;
    
    IOLock* m_resource_lock;
    IOLock* m_context_lock;
    
    // OpenGL acceleration service
    VMVirtIOGPUAccelerator* m_accelerator_service;
    
    // VirtIO operations
    bool initVirtIOGPU();
    void cleanupVirtIOGPU();
    void initHardwareDeferred();  // Deferred hardware init to prevent boot hang
    
    // VirtIO PCI capability parsing
    bool findVirtIOCapability(IOPCIDevice* pci_device, uint8_t cfg_type, uint8_t* bar_index, uint32_t* offset, uint32_t* length);
    
    // Command processing
    IOReturn submitCommand(virtio_gpu_ctrl_hdr* cmd, size_t cmd_size, 
                          virtio_gpu_ctrl_hdr* resp, size_t resp_size);
    IOReturn processControlQueue();
    
    // Internal resource management (private)
    IOReturn unrefResource(uint32_t resource_id);
    IOReturn detachBacking(uint32_t resource_id);
    
    // 3D operations (private)
    IOReturn create3DContext(uint32_t context_id);
    IOReturn destroy3DContext(uint32_t context_id);
    IOReturn submit3DCommand(uint32_t context_id, IOMemoryDescriptor* commands, size_t size);
    
    // PCI configuration space reading (private)
    IOReturn readPCIConfigSpace(IOPCIDevice* pciDevice, uint32_t* vendorID, uint32_t* deviceID);
    
    // Utility methods
    gpu_resource* findResource(uint32_t resource_id);
    gpu_3d_context* findContext(uint32_t context_id);
    
    // Phase 4: Advanced Queue State Management
    IOReturn advancedQueueStateManagement();
    
public:
    virtual IOService* probe(IOService* provider, SInt32* score) override;
    virtual bool start(IOService* provider) override;
    virtual void stop(IOService* provider) override;
    virtual bool init(OSDictionary* properties = nullptr) override;
    virtual void free() override;
    
    // IONDRVFramebuffer blocking
    void terminateIONDRVFramebuffers();
    
    // Resource management (public interface for framebuffer)
    IOReturn createResource2D(uint32_t resource_id, uint32_t format, 
                             uint32_t width, uint32_t height);
    IOReturn createResource3D(uint32_t resource_id, uint32_t target,
                             uint32_t format, uint32_t bind,
                             uint32_t width, uint32_t height, uint32_t depth);
    IOReturn attachBacking(uint32_t resource_id, IOMemoryDescriptor* memory);
    
    // Display scanout operations (public interface for framebuffer)
    IOReturn setscanout(uint32_t scanout_id, uint32_t resource_id,
                       uint32_t x, uint32_t y, uint32_t width, uint32_t height);
    
    // Display content operations (public interface for framebuffer)
    IOReturn flushResource(uint32_t resource_id, uint32_t x, uint32_t y,
                          uint32_t width, uint32_t height);
    IOReturn transferToHost2D(uint32_t resource_id, uint64_t offset,
                             uint32_t x, uint32_t y, uint32_t width, uint32_t height);
    
    // 3D acceleration interface
    IOReturn allocateResource3D(uint32_t* resource_id, uint32_t target, uint32_t format,
                               uint32_t width, uint32_t height, uint32_t depth);
    IOReturn deallocateResource(uint32_t resource_id);
    IOReturn createRenderContext(uint32_t* context_id);
    IOReturn destroyRenderContext(uint32_t context_id);
    IOReturn executeCommands(uint32_t context_id, IOMemoryDescriptor* commands);
    
    // Display interface for framebuffer
    IOReturn setupScanout(uint32_t scanout_id, uint32_t width, uint32_t height);
    IOReturn updateDisplay(uint32_t scanout_id, uint32_t resource_id,
                          uint32_t x, uint32_t y, uint32_t width, uint32_t height);
    
    // Cursor management interface
    IOReturn updateCursor(uint32_t resource_id, uint32_t hot_x, uint32_t hot_y,
                         uint32_t scanout_id, uint32_t x, uint32_t y);
    IOReturn moveCursor(uint32_t scanout_id, uint32_t x, uint32_t y);
    
    // Framebuffer communication interface - allows VMVirtIOFramebuffer to send commands to VirtIO hardware
    IOReturn sendDisplayCommand(virtio_gpu_ctrl_hdr* cmd, size_t cmd_size, 
                               virtio_gpu_ctrl_hdr* resp, size_t resp_size);
    
    // VirtIO 1.2 command header initialization (per specification)
    void initializeCommandHeader(virtio_gpu_ctrl_hdr* hdr, uint32_t cmd_type, 
                                uint32_t ctx_id = 0, bool use_fence = false);
    
    // VirtIO hardware queue setup (critical for notifications)
    bool setupVirtIOHardwareQueues();
    
    // Capability queries
    uint32_t getMaxScanouts() const { return m_max_scanouts; }
    bool supports3D() const { 
        return m_num_capsets > 0; 
    }
    IOReturn enableFeature(uint32_t feature_flags);
    uint32_t readVirtIODeviceFeatures() const;
    bool supportsFeature(uint32_t feature_flags) const;
    
    // PCI device configuration and VRAM interface (for framebuffer compatibility)
    IOReturn configurePCIDevice(IOPCIDevice* pciProvider);
    IODeviceMemory* getVRAMRange();
    
    // Extended capability queries and configuration
    uint32_t getMaxDisplays() const { return m_max_scanouts; }
    uint32_t getMaxResolutionX() const { return 4096; } // Default max resolution
    uint32_t getMaxResolutionY() const { return 4096; }
    bool supportsVirgl() const { return supports3D(); } // Virgl support requires 3D acceleration
    bool supportsResourceBlob() const { return supports3D(); } // Resource blob requires 3D support
    
    // Mock device configuration for compatibility mode
    void setMockMode(bool enabled);
    void setBasic3DSupport(bool enabled);
    
    // VirtIO queue and memory setup
    bool initializeVirtIOQueues();
    bool setupGPUMemoryRegions();
    void enable3DAcceleration();
    bool negotiateVirtIOFeatures();
    
    // Performance optimization
    bool setOptimalQueueSizes();
    void enableResourceBlob();
    void enableVirgl();
    void initializeWebGLAcceleration();
    void setPreferredRefreshRate(uint32_t hz);
    void enableVSync(bool enabled);
    
    // Memory management
    IOReturn allocateGPUMemory(size_t size, IOMemoryDescriptor** memory);
    IOReturn mapGuestMemory(IOMemoryDescriptor* guest_memory, uint64_t* gpu_addr);
    
    // Display output control
    IOReturn setupDisplayResource(uint32_t width, uint32_t height, uint32_t depth);
    IOReturn enableScanout(uint32_t scanout_id, uint32_t width, uint32_t height);
};

// Custom IOAccelerator subclass with newUserClient support
class VMVirtIOGPUUserClient;

// VMVirtIOGPUAccelerator inherits from VMQemuVGAAccelerator to get full OpenGL implementation
// This gives VirtIO GPU all the OpenGL methods (create3DContext, submit3DCommands, etc.)
class VMVirtIOGPUAccelerator : public VMQemuVGAAccelerator
{
    OSDeclareDefaultStructors(VMVirtIOGPUAccelerator);
    
private:
    VMVirtIOGPU* m_virtio_gpu_device;  // Renamed to avoid confusion with parent's m_gpu_device
    VMMetalPlugin* m_virtio_metal_plugin;  // Renamed to avoid confusion with parent's m_metal_bridge
    
public:
    virtual bool init(OSDictionary* properties = 0) APPLE_KEXT_OVERRIDE;
    virtual bool start(IOService* provider) APPLE_KEXT_OVERRIDE;
    virtual void stop(IOService* provider) APPLE_KEXT_OVERRIDE;
    virtual void free() APPLE_KEXT_OVERRIDE;
    
    // Override newUserClient to provide fixed-ID client
    virtual IOReturn newUserClient(task_t owningTask, void* securityID, UInt32 type, IOUserClient** handler) APPLE_KEXT_OVERRIDE;
    
    // GPU acceleration methods
    VMVirtIOGPU* getVirtIOGPUDevice() { return m_virtio_gpu_device; }
};

// Custom user client for VirtIO GPU acceleration
class VMVirtIOGPUUserClient : public IOUserClient
{
    OSDeclareDefaultStructors(VMVirtIOGPUUserClient);
    
private:
    VMVirtIOGPUAccelerator* m_accelerator;
    VMVirtIOGPU* m_gpu_device;
    task_t m_owning_task;
    UInt32 m_client_type;
    
    // Surface and context management
    OSArray* m_surfaces;
    OSArray* m_contexts;
    UInt32 m_next_surface_id;
    UInt32 m_next_context_id;
    
public:
    virtual bool initWithTask(task_t owningTask, void* securityToken, UInt32 type, 
                            OSDictionary* properties) APPLE_KEXT_OVERRIDE;
    virtual bool start(IOService* provider) APPLE_KEXT_OVERRIDE;
    virtual void stop(IOService* provider) APPLE_KEXT_OVERRIDE;
    virtual void free() APPLE_KEXT_OVERRIDE;
    
    virtual IOReturn clientClose() APPLE_KEXT_OVERRIDE;
    virtual IOReturn clientDied() APPLE_KEXT_OVERRIDE;
    
    // Memory mapping for WindowServer access to framebuffer
    virtual IOReturn clientMemoryForType(UInt32 type, IOOptionBits* options, IOMemoryDescriptor** memory) APPLE_KEXT_OVERRIDE;
    
    // GPU acceleration interface methods
    virtual IOReturn externalMethod(uint32_t selector, IOExternalMethodArguments* args,
                                  IOExternalMethodDispatch* dispatch = NULL, OSObject* target = NULL, void* reference = NULL) APPLE_KEXT_OVERRIDE;
    
    // Surface management
    IOReturn createSurface(uint32_t width, uint32_t height, uint32_t format, uint32_t* surface_id);
    IOReturn destroySurface(uint32_t surface_id);
    IOReturn lockSurface(uint32_t surface_id, void** address);
    IOReturn unlockSurface(uint32_t surface_id);
    
    // 3D context management  
    IOReturn create3DContext(uint32_t* context_id);
    IOReturn destroy3DContext(uint32_t context_id);
    IOReturn bindSurface(uint32_t context_id, uint32_t surface_id);
    
    // Basic 2D operations
    IOReturn clearSurface(uint32_t surface_id, uint32_t color);
    IOReturn copySurface(uint32_t src_id, uint32_t dst_id);
    IOReturn presentSurface(uint32_t surface_id);
};

#endif /* __VMVirtIOGPU_H__ */

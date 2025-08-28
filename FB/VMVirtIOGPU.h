#ifndef __VMVirtIOGPU_H__
#define __VMVirtIOGPU_H__

#include <IOKit/IOService.h>
#include <IOKit/IOCommandGate.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/pci/IOPCIDevice.h>
#include "virtio_gpu.h"

#define VIRTIO_GPU_QUEUE_CONTROL    0
#define VIRTIO_GPU_QUEUE_CURSOR     1

// VirtIO GPU feature flags are defined in virtio_gpu.h
// No need to redefine them here

class VMVirtIOGPU : public IOService
{
    OSDeclareDefaultStructors(VMVirtIOGPU);

private:
    IOPCIDevice* m_pci_device;
    IOMemoryMap* m_config_map;
    IOMemoryMap* m_notify_map;
    IOCommandGate* m_command_gate;
    
    // VirtIO transport device handle
    IOService* m_virtio_device;
    
    // VirtIO GPU configuration
    uint32_t m_max_scanouts;
    uint32_t m_num_capsets;
    
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
    
    // VirtIO queue ring structures for real hardware communication
    struct vring_desc {
        uint64_t addr;   // Buffer physical address
        uint32_t len;    // Buffer length
        uint16_t flags;  // Buffer flags
        uint16_t next;   // Next buffer if chained
    };
    
    struct vring_avail {
        uint16_t flags;
        uint16_t idx;
        uint16_t ring[];
    };
    
    struct vring_used_elem {
        uint32_t id;
        uint32_t len;
    };
    
    struct vring_used {
        uint16_t flags;
        uint16_t idx;
        struct vring_used_elem ring[];
    };
    
    // VirtIO queue state
    struct vring_desc* m_control_desc_ring;
    struct vring_avail* m_control_avail_ring;
    struct vring_used* m_control_used_ring;
    uint16_t m_control_queue_last_used_idx;
    
    // VirtIO operations
    bool initVirtIOGPU();
    bool initVirtIORings();
    void cleanupVirtIOGPU();
    
    // Command processing
    IOReturn submitCommand(virtio_gpu_ctrl_hdr* cmd, size_t cmd_size, 
                          virtio_gpu_ctrl_hdr* resp, size_t resp_size);
    IOReturn processControlQueue();
    
    // Resource management
    IOReturn createResource2D(uint32_t resource_id, uint32_t format, 
                             uint32_t width, uint32_t height);
    IOReturn createResource3D(uint32_t resource_id, uint32_t target,
                             uint32_t format, uint32_t bind,
                             uint32_t width, uint32_t height, uint32_t depth);
    IOReturn unrefResource(uint32_t resource_id);
    IOReturn attachBacking(uint32_t resource_id, IOMemoryDescriptor* memory);
    IOReturn detachBacking(uint32_t resource_id);
    
    // 3D operations
    IOReturn create3DContext(uint32_t context_id);
    IOReturn destroy3DContext(uint32_t context_id);
    IOReturn submit3DCommand(uint32_t context_id, IOMemoryDescriptor* commands, size_t size);
    
    // Display operations
    IOReturn setscanout(uint32_t scanout_id, uint32_t resource_id,
                       uint32_t x, uint32_t y, uint32_t width, uint32_t height);
    IOReturn flushResource(uint32_t resource_id, uint32_t x, uint32_t y,
                          uint32_t width, uint32_t height);
    IOReturn transferToHost2D(uint32_t resource_id, uint64_t offset,
                             uint32_t x, uint32_t y, uint32_t width, uint32_t height);
    
    // Utility methods
    gpu_resource* findResource(uint32_t resource_id);
    gpu_3d_context* findContext(uint32_t context_id);
    
public:
    // Display management methods called by VMQemuVGA
    IOReturn createScanoutResource(uint32_t resource_id, uint32_t width, uint32_t height, uint32_t format);
    IOReturn enableDisplayOutput(uint32_t scanout_id);
    IOReturn setPrimaryScanout(uint32_t width, uint32_t height, uint32_t format);
    
    virtual bool start(IOService* provider) override;
    virtual void stop(IOService* provider) override;
    virtual bool init(OSDictionary* properties = nullptr) override;
    virtual void free() override;
    
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
    
    // Capability queries
    uint32_t getMaxScanouts() const { return m_max_scanouts; }
    bool supports3D() const { return m_num_capsets > 0; }
    IOReturn enableFeature(uint32_t feature_flags);
    bool supportsFeature(uint32_t feature_flags) const;
    
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
    
    // VRAM management for System Profiler integration
    uint64_t getVRAMSize() const;
    IODeviceMemory* getVRAMRange();
};

#endif /* __VMVirtIOGPU_H__ */

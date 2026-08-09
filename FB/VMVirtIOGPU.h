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

// ---------------------------------------------------------------------------
// VirtIO 1.0 Split Virtqueue Structures
// ---------------------------------------------------------------------------

#define VRING_DESC_F_NEXT   1
#define VRING_DESC_F_WRITE  2

struct VRingDesc {
    uint64_t addr;     // guest physical address of buffer
    uint32_t len;      // buffer length in bytes
    uint16_t flags;    // VRING_DESC_F_*
    uint16_t next;     // index of next descriptor in chain (if F_NEXT)
};

struct VRingAvail {
    uint16_t flags;
    uint16_t idx;      // incremented each time we add to avail ring
    uint16_t ring[];   // head descriptor indices (qsize entries)
    // followed by uint16_t used_event (2 bytes)
};

struct VRingUsedElem {
    uint32_t id;       // head descriptor index
    uint32_t len;      // bytes written by device
};

struct VRingUsed {
    uint16_t flags;
    uint16_t idx;      // incremented by device each time it processes
    VRingUsedElem ring[]; // (qsize entries)
    // followed by uint16_t avail_event (2 bytes)
};

// Common config register offsets (VirtIO 1.0 spec §4.1.4.3)
#define VIRTIO_COMMON_DF_SELECT      0x00
#define VIRTIO_COMMON_DF             0x04
#define VIRTIO_COMMON_TF_SELECT      0x08
#define VIRTIO_COMMON_TF             0x0C
#define VIRTIO_COMMON_MSIX_CONFIG    0x10
#define VIRTIO_COMMON_NUM_QUEUES     0x12
#define VIRTIO_COMMON_DEVICE_STATUS  0x14
#define VIRTIO_COMMON_CONFIG_GEN     0x15
#define VIRTIO_COMMON_Q_SELECT       0x16
#define VIRTIO_COMMON_Q_SIZE         0x18
#define VIRTIO_COMMON_Q_MSIX         0x1A
#define VIRTIO_COMMON_Q_ENABLE       0x1C
#define VIRTIO_COMMON_Q_NOTIFY_OFF   0x1E
#define VIRTIO_COMMON_Q_DESC_LOW     0x20
#define VIRTIO_COMMON_Q_DESC_HIGH    0x24
#define VIRTIO_COMMON_Q_AVAIL_LOW    0x28
#define VIRTIO_COMMON_Q_AVAIL_HIGH   0x2C
#define VIRTIO_COMMON_Q_USED_LOW     0x30
#define VIRTIO_COMMON_Q_USED_HIGH    0x34

// VirtIO device status bits
#define VIRTIO_STATUS_ACKNOWLEDGE    0x01
#define VIRTIO_STATUS_DRIVER         0x02
#define VIRTIO_STATUS_FEATURES_OK    0x08
#define VIRTIO_STATUS_DRIVER_OK      0x04
#define VIRTIO_STATUS_FAILED         0x80

// Forward declarations
class VMVirtIOGPUAccelerator;
class VMMetalPlugin;
class VMVirtIOFramebuffer;

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
    
    // Framebuffer reference for scanout coordination
    VMVirtIOFramebuffer* m_framebuffer;
    
    // VirtIO GPU configuration
    uint32_t m_max_scanouts;
    uint32_t m_num_capsets;
    uint64_t m_fence_id;    // VirtIO 1.2: Fence ID counter for command synchronization
    bool m_is_virtio_gpu_pci;  // true = pure GPU mode (no VGA), false = VGA-compatible mode
    bool m_is_mock_device;     // true = mock device for QXL compatibility, false = real VirtIO GPU
    
    // Command queue management (legacy — kept for compatibility, superseded by vring)
    IOBufferMemoryDescriptor* m_control_queue;
    IOBufferMemoryDescriptor* m_cursor_queue;
    uint32_t m_control_queue_size;
    uint32_t m_cursor_queue_size;

    // Real VirtIO 1.0 split virtqueue for control queue
    IOBufferMemoryDescriptor* m_vring_mem;      // physically contiguous allocation
    volatile VRingDesc*  m_vq_desc;              // descriptor table
    volatile VRingAvail* m_vq_avail;             // available ring
    volatile VRingUsed*  m_vq_used;              // used ring
    uint16_t m_vq_size;                          // negotiated queue size (power of 2)
    uint16_t m_vq_free_head;                     // free-list head in descriptor table
    uint16_t m_vq_last_used;                     // last consumed used-ring index
    uint16_t m_vq_avail_idx;                     // next avail-ring slot to write
    uint16_t* m_vq_free_next;                    // free-list chain array (indexed by desc idx)
    volatile uint8_t* m_common_cfg;              // mapped common config base + offset
    uint32_t m_common_cfg_offset;               // offset of common cfg within BAR
    IOMemoryMap* m_common_map;                   // common config BAR mapping

    // PCI BAR mapping cache — one retain per BAR; callers take additional retains.
    // Eliminates the BAR-number-vs-IOKit-index confusion that caused the capset-read bug.
    IOMemoryMap* m_bar_maps[6];                  // cached mappings, indexed by PCI BAR number (0-5)
    uint8_t  m_device_cfg_bar;                   // PCI BAR number containing virtio_gpu_config
    uint32_t m_device_cfg_offset;                // offset of virtio_gpu_config within that BAR

    volatile uint8_t* m_notify_base;             // mapped notify BAR base
    uint32_t m_notify_cap_offset;               // offset of notify region within BAR
    uint32_t m_notify_off_multiplier;            // multiplier from notify capability
    IOLock* m_vq_lock;                           // serializes submitCommand
    IOBufferMemoryDescriptor* m_cmd_buf;         // pre-allocated command buffer (physically contiguous)
    IOBufferMemoryDescriptor* m_resp_buf;        // pre-allocated response buffer (physically contiguous)
    bool m_vq_initialized;                       // true once virtqueue is live

    // Cursor queue (queue 1) — separate vring, lock, and buffers.
    // Decoupled from the control queue so mouse moves don't contend
    // with 60 Hz framebuffer transfers.
    IOBufferMemoryDescriptor* m_cursor_vring_mem;
    volatile VRingDesc*  m_cursor_vq_desc;
    volatile VRingAvail* m_cursor_vq_avail;
    volatile VRingUsed*  m_cursor_vq_used;
    uint16_t m_cursor_vq_size;
    uint16_t m_cursor_vq_free_head;
    uint16_t m_cursor_vq_last_used;
    uint16_t m_cursor_vq_avail_idx;
    uint16_t* m_cursor_vq_free_next;
    IOLock* m_cursor_vq_lock;
    IOBufferMemoryDescriptor* m_cursor_cmd_buf;
    IOBufferMemoryDescriptor* m_cursor_resp_buf;
    uint32_t m_cursor_notify_offset;
    bool m_cursor_vq_initialized;

    // Refresh-timeout instrumentation. Throttled to first N submissions so the
    // boot log captures the succeed→fail transition without flooding afterward.
    // Counts persist for the lifetime of the object; bump when extending instrumentation.
    static const uint32_t SUBMIT_INSTRUMENT_LIMIT = 20;
    uint32_t m_submit_count;                     // incremented on each submitCommand entry
    uint32_t m_notify_count;                     // incremented on each device notify write
    
    // GPU resources
    struct gpu_resource {
        uint32_t resource_id;
        uint32_t width;
        uint32_t height;
        uint32_t format;
        IOMemoryDescriptor* backing_memory;
        bool is_3d;
        bool in_use;
    };

    // Resource pool — fixed-size array of gpu_resource slots. Tombstone semantics:
    //   slot[i].resource_id == 0  → free slot
    //   slot[i].resource_id != 0  → live slot
    // 0 is never a valid virtio-gpu resource id, so it's a safe sentinel.
    // Allocation takes the first zero slot (scan); deallocate writes 0.
    // m_resource_count is a high-water mark (diagnostic only), not a live count.
    // Lock discipline: callers of findResource / slot-mutating ops hold
    // m_resource_lock — the pool is touched from the workloop and from teardown.
    gpu_resource m_resource_pool[64];
    uint32_t m_resource_count;
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

    // Real VirtIO 1.0 virtqueue management
    bool setupControlVirtQueue();
    void teardownControlVirtQueue();
    bool negotiateFeatures();
    bool readCommonConfig(volatile uint8_t** out_base, uint32_t* out_offset,
                          uint8_t* out_bar, IOMemoryMap** out_map);
    bool readNotifyConfig(uint8_t* out_bar, uint32_t* out_offset, uint32_t* out_multiplier);
    
    // VirtIO PCI capability parsing
    bool findVirtIOCapability(IOPCIDevice* pci_device, uint8_t cfg_type, uint8_t* bar_index, uint32_t* offset, uint32_t* length);

    // PCI BAR number → IOMemoryMap. Memoized per BAR (single retain held in m_bar_maps[bar];
    // each call returns an additional retain the caller must release). Returns nullptr if the
    // BAR is unmapped, I/O-type, or has no matching IOKit memory index. If out_iokit_index is
    // non-NULL, it receives the matched IOKit region index on success.
    IOMemoryMap* mapBarByNumber(uint8_t bar, int* out_iokit_index = nullptr);

    // Walks the descriptor free list and returns its current depth. O(depth) but
    // only called from instrumentation paths (throttled to first N submissions).
    uint16_t vringFreeDepth() const;

    // Command processing
    IOReturn submitCommand(virtio_gpu_ctrl_hdr* cmd, size_t cmd_size,
                          virtio_gpu_ctrl_hdr* resp, size_t resp_size);
    IOReturn processControlQueue();

    // Cursor queue (queue 1) — separate submit path, lock, and vring.
    IOReturn submitCursorCommand(virtio_gpu_ctrl_hdr* cmd, size_t cmd_size,
                                  virtio_gpu_ctrl_hdr* resp, size_t resp_size);
    bool setupCursorQueue(volatile uint8_t* cfg);

    // NOTE: unrefResource/detachBacking are declared but unimplemented.
    // Use deallocateResource (public) instead — it sends RESOURCE_UNREF which
    // makes the host drop both resource and backing attachment in one command.
    IOReturn unrefResource(uint32_t resource_id);
    IOReturn detachBacking(uint32_t resource_id);
    
    // 3D operations (private)
    // Note: create3DContext and destroy3DContext are declared in the public section (line ~296)
    IOReturn submit3DCommand(uint32_t context_id, IOMemoryDescriptor* commands, size_t size);
    
    // PCI configuration space reading (private)
    IOReturn readPCIConfigSpace(IOPCIDevice* pciDevice, uint32_t* vendorID, uint32_t* deviceID);
    
    // Utility methods
    gpu_resource* findResource(uint32_t resource_id);
    gpu_3d_context* findContext(uint32_t context_id);
    
    IOReturn advancedQueueStateManagement();
    
public:
    // One-shot self-check: create a probe resource, attempt a duplicate create
    // (must return kIOReturnBadArgument), then destroy it. Verifies findResource
    // actually finds after the pool unification. Same shape as the
    // SET_SCANOUT(999) negative control. Called once from the first
    // createResource2D entry; gated by a static flag.
    void probeResourceTracking();

    // One-shot cursor queue transport probe: creates a 64×64 test cursor,
    // sends UPDATE_CURSOR + MOVE_CURSOR on queue 1. Pass: two cursors on screen.
    void probeCursorTransport();

    // One-shot 3D-transport self-check: CTX_CREATE → RESOURCE_CREATE_3D →
    // ATTACH_BACKING → CTX_ATTACH_RESOURCE → CREATE_OBJECT(surface) →
    // SET_FRAMEBUFFER_STATE + CLEAR → TRANSFER_FROM_HOST_3D → byte-equal
    // positive control + negative control (different clear color, readback
    // must change). Per CLAUDE.md "code should self-check": the only
    // trustworthy signal is the byte readback; SUBMIT_3D responses are
    // unconditional 0x1100 and prove nothing about virgl decode success.
    void probeTransport3D();

    virtual IOService* probe(IOService* provider, SInt32* score) override;
    virtual bool start(IOService* provider) override;
    virtual void stop(IOService* provider) override;
    virtual bool init(OSDictionary* properties = nullptr) override;
    virtual void free() override;
    
    // Manual initialization without IOService registration (for programmatic instantiation)
    bool initializeWithPCIDevice(IOPCIDevice* pciDevice);
    
    // IONDRVFramebuffer blocking
    void terminateIONDRVFramebuffers();
    
    // Resource management (public interface for framebuffer)
    // backing = NULL  -> driver allocates and owns the backing memory (freed on destroy)
    // backing != NULL -> caller owns; driver skips internal allocation entirely
    IOReturn createResource2D(uint32_t resource_id, uint32_t format,
                             uint32_t width, uint32_t height,
                             IOMemoryDescriptor* backing = NULL);
    IOReturn createResource3D(uint32_t resource_id, uint32_t target,
                             uint32_t format, uint32_t bind,
                             uint32_t width, uint32_t height, uint32_t depth);
    IOReturn attachBacking(uint32_t resource_id, IOMemoryDescriptor* memory);
    
    // Display scanout operations (public interface for framebuffer)
    IOReturn setscanout(uint32_t scanout_id, uint32_t resource_id,
                       uint32_t x, uint32_t y, uint32_t width, uint32_t height);
    
    // 3D context management (public interface for UserClient)
    IOReturn create3DContext(uint32_t* context_id);
    IOReturn destroy3DContext(uint32_t context_id);
    
    // Display content operations (public interface for framebuffer)
    IOReturn flushResource(uint32_t resource_id, uint32_t x, uint32_t y,
                          uint32_t width, uint32_t height);
    IOReturn transferToHost2D(uint32_t resource_id, uint64_t offset,
                             uint32_t x, uint32_t y, uint32_t width, uint32_t height);
    IOReturn transferToHost3D(uint32_t resource_id, uint32_t level,
                             uint32_t x, uint32_t y, uint32_t z,
                             uint32_t width, uint32_t height, uint32_t depth);
    IOReturn transferFromHost3D(uint32_t resource_id, uint32_t level,
                               uint32_t x, uint32_t y, uint32_t z,
                               uint32_t width, uint32_t height, uint32_t depth);
    
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
    
    // Framebuffer reference management
    void setFramebuffer(VMVirtIOFramebuffer* framebuffer);
    
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
    
    // ========================================================================
    // 2D Acceleration Helper Methods
    // Used by VMQemuVGAAccelerator for WindowServer operations
    // ========================================================================
    
    // Hardware-accelerated rectangle blit
    IOReturn blitRect(uint32_t srcX, uint32_t srcY,
                     uint32_t destX, uint32_t destY,
                     uint32_t width, uint32_t height,
                     uint32_t srcRowBytes, uint32_t destRowBytes);
    
    // Hardware-accelerated rectangle fill
    IOReturn fillRect(uint32_t x, uint32_t y,
                     uint32_t width, uint32_t height,
                     uint32_t color);
    
    // Flush all pending GPU commands
    IOReturn flushCommands();
    
    // Wait for GPU to become idle
    IOReturn waitForIdle();
    
    // Check if this is a mock device (no real hardware)
    bool isMockDevice() const { return m_is_mock_device; }
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
    
    // VirtIO GPU 3D command translation - virgl protocol
    IOReturn submitClearCommand(uint32_t context_id, 
                               float red, float green, float blue, float alpha,
                               double depth, uint32_t stencil,
                               uint32_t buffers);
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
    
    // VirtGLGL userspace library interface
    IOReturn submitVirglCommands(const void* commands, uint32_t size);
    IOReturn createVirglResource(uint32_t resourceId, uint32_t width, uint32_t height, uint32_t format);
    IOReturn createVirglContext(uint32_t contextId);
    IOReturn attachVirglResource(uint32_t contextId, uint32_t resourceId);
    uint32_t getVirglCapability(uint32_t cap);
};

#endif /* __VMVirtIOGPU_H__ */

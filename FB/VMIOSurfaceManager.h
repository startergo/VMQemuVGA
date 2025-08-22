#ifndef VMIOSurfaceManager_h
#define VMIOSurfaceManager_h

#include <IOKit/IOService.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <libkern/c++/OSObject.h>
#include <libkern/c++/OSArray.h>
#include <libkern/c++/OSDictionary.h>
// Note: IOSurface not available in kernel extensions, using kernel-compatible types

// Forward declarations
class VMQemuVGAAccelerator;
class VMVirtIOGPU;
class VMMetalBridge;

// IOSurface pixel formats
typedef enum {
    VM_IOSURFACE_PIXEL_FORMAT_ARGB32 = 'ARGB',
    VM_IOSURFACE_PIXEL_FORMAT_BGRA32 = 'BGRA',
    VM_IOSURFACE_PIXEL_FORMAT_RGBA32 = 'RGBA',
    VM_IOSURFACE_PIXEL_FORMAT_ABGR32 = 'ABGR',
    VM_IOSURFACE_PIXEL_FORMAT_RGB24 = 0x00000018,
    VM_IOSURFACE_PIXEL_FORMAT_RGB565 = 'R565',
    VM_IOSURFACE_PIXEL_FORMAT_YUV420 = 'y420',
    VM_IOSURFACE_PIXEL_FORMAT_NV12 = '420f',
    VM_IOSURFACE_PIXEL_FORMAT_P010 = 'p010'
} VMIOSurfacePixelFormat;

// IOSurface usage flags
typedef enum {
    VM_IOSURFACE_USAGE_READ = 0x01,
    VM_IOSURFACE_USAGE_WRITE = 0x02,
    VM_IOSURFACE_USAGE_GPU_READ = 0x04,
    VM_IOSURFACE_USAGE_GPU_WRITE = 0x08,
    VM_IOSURFACE_USAGE_DISPLAY = 0x10,
    VM_IOSURFACE_USAGE_VIDEO_DECODER = 0x20,
    VM_IOSURFACE_USAGE_VIDEO_ENCODER = 0x40,
    VM_IOSURFACE_USAGE_CAMERA = 0x80
} VMIOSurfaceUsageFlags;

// IOSurface lock options
typedef enum {
    VM_IOSURFACE_LOCK_READ_ONLY = 0x01,
    VM_IOSURFACE_LOCK_AVOID_SYNC = 0x02
} VMIOSurfaceLockOptions;

// IOSurface plane descriptor
typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t bytes_per_element;
    uint32_t bytes_per_row;
    uint32_t element_width;
    uint32_t element_height;
    uint32_t offset;
    uint32_t size;
} VMIOSurfacePlaneInfo;

// IOSurface descriptor
typedef struct {
    uint32_t width;
    uint32_t height;
    VMIOSurfacePixelFormat pixel_format;
    uint32_t bytes_per_row;
    uint32_t bytes_per_element;
    uint32_t element_width;
    uint32_t element_height;
    uint32_t plane_count;
    VMIOSurfacePlaneInfo planes[4]; // Maximum 4 planes
    uint32_t alloc_size;
    uint32_t usage_flags;
    uint32_t cache_mode;
    uint32_t depth;       // Added missing depth field
    uint32_t format;      // Added missing format field  
    uint32_t usage;       // Added missing usage field
    uint32_t flags;       // Added missing flags field
} VMIOSurfaceDescriptor;

// Internal IOSurface object
typedef struct {
    uint32_t surface_id;
    VMIOSurfaceDescriptor descriptor;
    IOBufferMemoryDescriptor* memory;
    void* base_address;
    uint32_t lock_count;
    uint32_t ref_count;
    uint32_t usage;
    uint32_t format;
    uint32_t flags;
    uint32_t depth;
    bool is_locked;
    bool is_purgeable;
    uint64_t creation_time;
    uint64_t last_access_time;
    
    // Additional fields used in the implementation
    uint32_t width;
    uint32_t height;
    uint32_t memory_size;
    char name[64];        // Surface name
    uint32_t cache_mode;  // Cache mode setting
} VMIOSurface;

// IOSurface client descriptor
typedef struct {
    uint32_t client_id;
    uint32_t process_id;
    uint32_t access_rights;
    const char* client_name;
} VMIOSurfaceClientDescriptor;

// IOSurface sharing descriptor
typedef struct {
    uint32_t surface_id;
    uint32_t sharing_mode;
    uint32_t* allowed_clients;
    uint32_t client_count;
} VMIOSurfaceSharingDescriptor;

/**
 * @class VMIOSurfaceManager
 * @brief IOSurface Management for VMQemuVGA 3D Acceleration
 * 
 * This class provides IOSurface support for the VMQemuVGA 3D acceleration
 * system, enabling efficient shared surface management between processes
 * and GPU-accelerated operations through hardware-backed IOSurface objects.
 */
class VMIOSurfaceManager : public OSObject
{
    OSDeclareDefaultStructors(VMIOSurfaceManager);
    
private:
    VMQemuVGAAccelerator* m_accelerator;
    VMVirtIOGPU* m_gpu_device;
    VMMetalBridge* m_metal_bridge;
    IORecursiveLock* m_lock;
    IOLock* m_surface_lock;
    
    // Surface management
    OSArray* m_surfaces;
    OSArray* m_clients;
    OSArray* m_shared_surfaces;
    OSDictionary* m_surface_map;
    OSDictionary* m_client_map;
    
    // Resource tracking
    uint32_t m_next_surface_id;
    uint32_t m_next_client_id;
    
    // Memory management
    OSArray* m_memory_pools;
    OSDictionary* m_surface_memory_map;
    uint64_t m_total_surface_memory;
    uint64_t m_available_memory;
    
    // Performance counters
    uint64_t m_surfaces_created;
    uint64_t m_surfaces_destroyed;
    uint64_t m_surface_locks;
    uint64_t m_surface_unlocks;
    uint64_t m_bytes_allocated;
    uint64_t m_bytes_deallocated;
    
    // Feature support
    bool m_supports_hardware_surfaces;
    bool m_supports_yuv_surfaces;
    bool m_supports_compressed_surfaces;
    bool m_supports_video_surfaces;
    bool m_supports_secure_surfaces;
    
public:
    // Initialization and cleanup
    virtual bool init() override;
    virtual void free() override;
    
    // Setup and configuration
    bool initWithAccelerator(VMQemuVGAAccelerator* accelerator);
    IOReturn setupIOSurfaceSupport();
    IOReturn configureMemoryPools();
    
    // Surface lifecycle management
    IOReturn createSurface(const VMIOSurfaceDescriptor* descriptor, uint32_t* surface_id);
    IOReturn destroySurface(uint32_t surface_id);
    IOReturn getSurfaceDescriptor(uint32_t surface_id, VMIOSurfaceDescriptor* descriptor);
    IOReturn updateSurfaceDescriptor(uint32_t surface_id, const VMIOSurfaceDescriptor* descriptor);
    
    // Surface memory management
    IOReturn lockSurface(uint32_t surface_id, uint32_t lock_options, void** base_address);
    IOReturn unlockSurface(uint32_t surface_id, uint32_t lock_options);
    IOReturn setSurfaceProperty(uint32_t surface_id, const char* property_name, 
                               const void* property_value, uint32_t value_size);
    IOReturn getSurfaceProperty(uint32_t surface_id, const char* property_name,
                               void* property_value, uint32_t* value_size);
    
    // Plane-specific operations
    IOReturn lockSurfacePlane(uint32_t surface_id, uint32_t plane_index, 
                             uint32_t lock_options, void** base_address);
    IOReturn unlockSurfacePlane(uint32_t surface_id, uint32_t plane_index, 
                               uint32_t lock_options);
    IOReturn getSurfacePlaneInfo(uint32_t surface_id, uint32_t plane_index,
                                VMIOSurfacePlaneInfo* plane_info);
    
    // Client management
    IOReturn registerClient(const VMIOSurfaceClientDescriptor* descriptor, uint32_t* client_id);
    IOReturn unregisterClient(uint32_t client_id);
    IOReturn getClientDescriptor(uint32_t client_id, VMIOSurfaceClientDescriptor* descriptor);
    IOReturn setClientAccessRights(uint32_t client_id, uint32_t access_rights);
    
    // Surface sharing
    IOReturn shareSurface(uint32_t surface_id, const VMIOSurfaceSharingDescriptor* descriptor);
    IOReturn unshareSurface(uint32_t surface_id, uint32_t client_id);
    IOReturn getSurfaceClients(uint32_t surface_id, uint32_t* client_ids, uint32_t* client_count);
    bool canClientAccessSurface(uint32_t client_id, uint32_t surface_id);
    
    // Surface operations
    IOReturn copySurface(uint32_t source_surface_id, uint32_t dest_surface_id);
    IOReturn copySurfaceRegion(uint32_t source_surface_id, uint32_t dest_surface_id,
                              uint32_t src_x, uint32_t src_y, uint32_t dst_x, uint32_t dst_y,
                              uint32_t width, uint32_t height);
    IOReturn fillSurface(uint32_t surface_id, uint32_t fill_color);
    IOReturn fillSurfaceRegion(uint32_t surface_id, uint32_t x, uint32_t y, 
                              uint32_t width, uint32_t height, uint32_t fill_color);
    
    // GPU integration
    IOReturn bindSurfaceToTexture(uint32_t surface_id, uint32_t texture_id);
    IOReturn bindSurfaceToBuffer(uint32_t surface_id, uint32_t buffer_id);
    IOReturn createTextureFromSurface(uint32_t surface_id, uint32_t* texture_id);
    IOReturn updateSurfaceFromTexture(uint32_t surface_id, uint32_t texture_id);
    IOReturn synchronizeSurface(uint32_t surface_id);
    
    // Video surface support
    IOReturn createVideoSurface(const VMIOSurfaceDescriptor* descriptor, 
                               uint32_t codec_type, uint32_t* surface_id);
    IOReturn setVideoSurfaceColorSpace(uint32_t surface_id, uint32_t color_space);
    IOReturn getVideoSurfaceColorSpace(uint32_t surface_id, uint32_t* color_space);
    IOReturn attachVideoDecoder(uint32_t surface_id, uint32_t decoder_id);
    IOReturn detachVideoDecoder(uint32_t surface_id);
    
    // Format conversion
    IOReturn convertSurfaceFormat(uint32_t source_surface_id, uint32_t dest_surface_id,
                                 VMIOSurfacePixelFormat dest_format);
    bool isFormatSupported(VMIOSurfacePixelFormat format);
    uint32_t getBytesPerPixel(VMIOSurfacePixelFormat format);
    uint32_t getPlaneCount(VMIOSurfacePixelFormat format);
    
    // Memory optimization
    IOReturn compactSurfaceMemory();
    IOReturn defragmentMemoryPools();
    IOReturn setSurfaceMemoryPriority(uint32_t surface_id, uint32_t priority);
    IOReturn evictUnusedSurfaces();
    
    // Performance and debugging
    IOReturn getIOSurfaceStats(void* stats_buffer, size_t* buffer_size);
    IOReturn getMemoryUsage(uint64_t* total_memory, uint64_t* available_memory, 
                           uint64_t* largest_free_block);
    void resetIOSurfaceCounters();
    void logIOSurfaceState();
    
    // Validation and diagnostics
    IOReturn validateSurface(uint32_t surface_id);
    IOReturn checkSurfaceIntegrity(uint32_t surface_id);
    IOReturn dumpSurfaceInfo(uint32_t surface_id);
    
private:
    // Internal helper methods
    OSObject* findSurface(uint32_t surface_id);
    OSObject* findClient(uint32_t client_id);
    uint32_t allocateSurfaceId();
    uint32_t allocateClientId();
    void releaseSurfaceId(uint32_t surface_id);
    void releaseClientId(uint32_t client_id);
    
    // Memory management helpers
    IOReturn allocateSurfaceMemory(const VMIOSurfaceDescriptor* descriptor, 
                                  IOMemoryDescriptor** memory);
    IOReturn deallocateSurfaceMemory(uint32_t surface_id);
    IOReturn findBestMemoryPool(uint32_t size, uint32_t alignment, uint32_t* pool_index);
    IOReturn createMemoryPool(uint32_t size, uint32_t* pool_index);
    
    // GPU resource helpers
    IOReturn createGPUResource(uint32_t surface_id, uint32_t* gpu_resource_id);
    IOReturn destroyGPUResource(uint32_t surface_id);
    IOReturn syncGPUResource(uint32_t surface_id);
    
    // Format helpers
    uint32_t calculateSurfaceSize(const VMIOSurfaceDescriptor* descriptor);
    uint32_t calculatePlaneSize(const VMIOSurfacePlaneInfo* plane_info);
    IOReturn validatePixelFormat(VMIOSurfacePixelFormat format);
    IOReturn setupPlaneInfo(VMIOSurfacePixelFormat format, uint32_t width, uint32_t height,
                           VMIOSurfacePlaneInfo* planes, uint32_t* plane_count);
    
    // Sharing helpers
    bool isClientAuthorized(uint32_t client_id, uint32_t surface_id, uint32_t access_type);
    IOReturn updateSharingPermissions(uint32_t surface_id);
    IOReturn notifyClientsOfSurfaceChange(uint32_t surface_id);
    
    // Performance optimization helpers
    bool shouldEvictSurface(uint32_t surface_id);
    uint32_t calculateSurfacePriority(uint32_t surface_id);
    IOReturn optimizeMemoryLayout();
    IOReturn prefetchSurfaceData(uint32_t surface_id);
};

#endif /* VMIOSurfaceManager_h */

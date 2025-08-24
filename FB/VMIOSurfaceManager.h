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
    VM_IOSURFACE_PIXEL_FORMAT_P010 = 'p010',
    
    // Extended RGB formats
    VM_IOSURFACE_PIXEL_FORMAT_RGB = 0x52474220,    // 'RGB ' - 24-bit RGB
    VM_IOSURFACE_PIXEL_FORMAT_BGR = 0x42475220,    // 'BGR ' - 24-bit BGR
    VM_IOSURFACE_PIXEL_FORMAT_B565 = 0x42353635,  // 'B565' - 16-bit BGR 565
    VM_IOSURFACE_PIXEL_FORMAT_R555 = 0x52353535,  // 'R555' - 16-bit RGB 555
    VM_IOSURFACE_PIXEL_FORMAT_B555 = 0x42353535,  // 'B555' - 16-bit BGR 555
    
    // Luminance formats
    VM_IOSURFACE_PIXEL_FORMAT_L8 = 0x4C303030,    // 'L00' - 8-bit Luminance
    VM_IOSURFACE_PIXEL_FORMAT_LA8 = 0x4C413030,   // 'LA00' - 8-bit Luminance + Alpha
    
    // YUV formats
    VM_IOSURFACE_PIXEL_FORMAT_YUV4 = 0x79757634,  // 'yuv4' - YUV 4:2:0
    VM_IOSURFACE_PIXEL_FORMAT_YV12 = 0x32315659,  // 'YV12' - YUV 4:2:0 Planar
    VM_IOSURFACE_PIXEL_FORMAT_I420 = 0x49343230,  // 'I420' - YUV 4:2:0 Planar
    VM_IOSURFACE_PIXEL_FORMAT_IYUV = 0x56555949,  // 'IYUV' - YUV 4:2:0 Planar
    VM_IOSURFACE_PIXEL_FORMAT_YV02 = 0x32307679,  // 'yv02' - YVU 4:2:0
    VM_IOSURFACE_PIXEL_FORMAT_YUV2 = 0x79757632,  // 'yuv2' - YUV 4:2:2
    VM_IOSURFACE_PIXEL_FORMAT_YVU2 = 0x32767579,  // 'yvu2' - YVU 4:2:2
    VM_IOSURFACE_PIXEL_FORMAT_YUY2 = 0x32595559,  // 'YUY2' - YUV 4:2:2 Packed
    VM_IOSURFACE_PIXEL_FORMAT_YVYU = 0x59565955,  // 'YVYU' - YVU 4:2:2 Packed
    VM_IOSURFACE_PIXEL_FORMAT_UYVY = 0x55595659,  // 'UYVY' - UYV 4:2:2 Packed
    VM_IOSURFACE_PIXEL_FORMAT_YUV444 = 0x79757620, // 'yuv ' - YUV 4:4:4
    
    // Compression formats
    VM_IOSURFACE_PIXEL_FORMAT_DXT1 = 0x44585431,  // 'DXT1' - S3TC DXT1
    VM_IOSURFACE_PIXEL_FORMAT_DXT3 = 0x44585433,  // 'DXT3' - S3TC DXT3
    VM_IOSURFACE_PIXEL_FORMAT_DXT5 = 0x44585435,  // 'DXT5' - S3TC DXT5
    VM_IOSURFACE_PIXEL_FORMAT_ETC1 = 0x45544331,  // 'ETC1' - ETC1 compression
    VM_IOSURFACE_PIXEL_FORMAT_ETC2 = 0x45544332,  // 'ETC2' - ETC2 compression
    VM_IOSURFACE_PIXEL_FORMAT_PVRT = 0x50565254,  // 'PVRT' - PowerVR texture compression
    
    // Video formats
    VM_IOSURFACE_PIXEL_FORMAT_H264 = 0x48323634,  // 'H264' - H.264 format
    VM_IOSURFACE_PIXEL_FORMAT_H265 = 0x48323635,  // 'H265' - H.265 format
    VM_IOSURFACE_PIXEL_FORMAT_AVC1 = 0x61766331,  // 'avc1' - H.264 format
    VM_IOSURFACE_PIXEL_FORMAT_HVC1 = 0x68766331   // 'hvc1' - H.265 format
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

// IOSurface memory priority levels
typedef enum {
    VM_IOSURFACE_MEMORY_PRIORITY_LOW = 0,
    VM_IOSURFACE_MEMORY_PRIORITY_NORMAL = 1,
    VM_IOSURFACE_MEMORY_PRIORITY_HIGH = 2,
    VM_IOSURFACE_MEMORY_PRIORITY_CRITICAL = 3
} VMIOSurfaceMemoryPriority;

// Comprehensive IOSurface statistics structure
typedef struct {
    // Surface counts
    uint32_t surface_count;
    uint32_t peak_surface_count;
    uint32_t client_count;
    uint32_t active_surfaces;
    
    // Memory statistics
    uint64_t total_memory;
    uint64_t allocated_memory;
    uint64_t peak_memory_usage;
    uint64_t available_memory;
    uint64_t largest_free_block;
    
    // Operation counters
    uint64_t surfaces_created;
    uint64_t surfaces_destroyed;
    uint64_t surface_allocations;
    uint64_t surface_deallocations;
    uint64_t surface_locks;
    uint64_t surface_unlocks;
    uint64_t lock_operations;
    uint64_t unlock_operations;
    uint64_t copy_operations;
    
    // Performance metrics
    uint64_t cache_hits;
    uint64_t cache_misses;
    uint64_t bytes_allocated;
    uint64_t bytes_deallocated;
    
    // GPU integration statistics
    uint64_t gpu_syncs;
    uint64_t gpu_updates;
    uint64_t gpu_texture_uploads;
    uint64_t gpu_command_buffers;
    
    // Video surface statistics
    uint64_t video_surfaces_created;
    uint64_t video_frames_processed;
    uint64_t video_decoder_operations;
    uint64_t video_encoder_operations;
    
    // Memory optimization statistics
    uint64_t memory_compactions;
    uint64_t memory_defragmentations;
    uint64_t surfaces_evicted;
    uint64_t priority_changes;
    
    // Error and diagnostic counters
    uint64_t allocation_failures;
    uint64_t validation_errors;
    uint64_t integrity_failures;
    uint64_t format_conversion_errors;
    
    // Feature support flags
    bool supports_hardware_surfaces;
    bool supports_yuv_surfaces;
    bool supports_compressed_surfaces;
    bool supports_video_surfaces;
    bool supports_secure_surfaces;
    
    // Timing statistics (in nanoseconds)
    uint64_t average_allocation_time;
    uint64_t average_lock_time;
    uint64_t average_copy_time;
    uint64_t total_processing_time;
} VMIOSurfaceStats;

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
    VMIOSurfaceMemoryPriority memory_priority; // Memory management priority
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

// Advanced IOSurface Discovery System Statistics (v4.0)
typedef struct {
    uint32_t cache_hits;
    uint32_t cache_misses;
    uint32_t total_lookups;
    uint32_t prefetch_hits;
    uint32_t fast_path_hits;
    uint64_t total_discovery_time_ns;
    uint32_t sequential_access_count;
    uint32_t random_access_count;
} VMSurfaceDiscoveryStats;

// Advanced IOSurface Property Management System Statistics (v5.0)
typedef struct {
    uint32_t property_lookups;
    uint32_t cache_hits;
    uint32_t cache_misses;
    uint32_t format_conversions;
    uint32_t resolution_adaptations;
    uint64_t total_property_time_ns;
    uint32_t validation_failures;
    uint32_t compatibility_checks;
} VMSurfacePropertyStats;

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
    OSArray* m_released_surface_ids;    // ID recycling pool
    OSArray* m_released_client_ids;     // Client ID recycling pool
    
    // Memory management
    OSArray* m_memory_pools;
    OSDictionary* m_surface_memory_map;
    uint64_t m_total_surface_memory;
    uint64_t m_available_memory;
    uint64_t m_allocated_surface_memory;
    
    // Surface tracking
    uint32_t m_surface_count;
    uint32_t m_peak_surface_count;
    uint64_t m_surface_allocations;
    uint64_t m_surface_deallocations;
    
    // Performance counters
    uint64_t m_surfaces_created;
    uint64_t m_surfaces_destroyed;
    uint64_t m_surface_locks;
    uint64_t m_surface_unlocks;
    uint64_t m_lock_operations;
    uint64_t m_unlock_operations;
    uint64_t m_copy_operations;
    uint64_t m_cache_hits;
    uint64_t m_cache_misses;
    uint64_t m_bytes_allocated;
    uint64_t m_bytes_deallocated;
    
    // Feature support
    bool m_supports_hardware_surfaces;
    bool m_supports_yuv_surfaces;
    bool m_supports_compressed_surfaces;
    bool m_supports_video_surfaces;
    bool m_supports_secure_surfaces;
    
    // GPU integration collections
    OSArray* m_gpu_resources;           // GPU resource descriptors
    OSArray* m_gpu_textures;            // GPU texture descriptors  
    OSArray* m_texture_bindings;        // Surface-to-texture bindings
    
    // Video surface collections
    OSArray* m_video_surfaces;          // Video surface descriptors
    OSArray* m_video_decoders;          // Video decoder associations
    
    // GPU performance counters
    uint64_t m_gpu_syncs;               // GPU synchronization operations
    uint64_t m_gpu_updates;             // GPU-to-surface updates
    uint64_t m_gpu_texture_uploads;     // GPU texture upload operations
    uint64_t m_gpu_command_buffers;     // GPU command buffer submissions
    
    // Video performance counters
    uint64_t m_video_surfaces_created;  // Video surfaces created
    uint64_t m_video_frames_processed;  // Video frames processed
    uint64_t m_video_decoder_operations; // Video decoder operations
    uint64_t m_video_encoder_operations; // Video encoder operations
    
    // Memory optimization counters
    uint64_t m_memory_compactions;      // Memory compaction operations
    uint64_t m_memory_defragmentations; // Memory defragmentation operations
    uint64_t m_surfaces_evicted;        // Surfaces evicted for memory
    uint64_t m_priority_changes;        // Memory priority changes
    
    // Error and diagnostic counters
    uint64_t m_allocation_failures;     // Failed allocations
    uint64_t m_validation_errors;       // Surface validation errors
    uint64_t m_integrity_failures;      // Surface integrity failures
    uint64_t m_format_conversion_errors; // Format conversion errors
    
    // Additional memory tracking
    uint64_t m_peak_memory_usage;       // Peak memory usage
    
    // Timing statistics (in nanoseconds)
    uint64_t m_total_allocation_time;   // Total time spent on allocations
    uint64_t m_total_lock_time;         // Total time spent on locks
    uint64_t m_total_copy_time;         // Total time spent on copies
    
    // Debugging state
    bool m_debug_logging_enabled;       // Debug logging flag
    uint32_t m_logging_level;           // Logging verbosity level
    uint64_t m_debug_operations_count;  // Debug operations counter
    
    // Video decoder attachments
    uint64_t m_video_decoder_attachments; // Video decoder attachments
    
    // Format conversion counters
    uint64_t m_format_conversions;      // Format conversion operations
    
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
    IOReturn setSurfaceMemoryPriority(uint32_t surface_id, VMIOSurfaceMemoryPriority priority);
    IOReturn evictUnusedSurfaces(uint32_t max_surfaces_to_evict, uint32_t* surfaces_evicted);
    
    // Performance and debugging
    IOReturn getIOSurfaceStats(void* stats_buffer, size_t* buffer_size);
    IOReturn getMemoryUsage(uint64_t* total_memory, uint64_t* available_memory, 
                           uint64_t* largest_free_block);
    void resetIOSurfaceCounters();
    void logIOSurfaceState();
    
    // Additional performance methods
    IOReturn getPerformanceStats(void* stats_buffer, size_t* buffer_size);
    IOReturn resetPerformanceCounters();
    IOReturn logPerformanceData();
    IOReturn benchmarkSurfaceOperations(uint32_t iterations, uint64_t* results);
    
    // Advanced debugging methods
    IOReturn enableDebugLogging(bool enable);
    IOReturn setLoggingLevel(uint32_t level);
    IOReturn captureMemorySnapshot(void* snapshot_buffer, size_t* buffer_size);
    IOReturn analyzeMemoryFragmentation(uint32_t* fragmentation_percent, 
                                       uint32_t* largest_fragment_size);
    
    // Validation and diagnostics
    IOReturn validateSurface(uint32_t surface_id);
    IOReturn checkSurfaceIntegrity(uint32_t surface_id);
    IOReturn dumpSurfaceInfo(uint32_t surface_id);
    
private:
    // Advanced IOSurface Discovery Management System v4.0 API
    IOReturn getDiscoveryStatistics(VMSurfaceDiscoveryStats* stats);
    IOReturn resetDiscoveryStatistics();
    IOReturn flushDiscoveryCache();
    IOReturn prewarmDiscoveryCache(uint32_t* surface_ids, uint32_t count);
    IOReturn optimizeDiscoveryCache();
    void generateDiscoveryReport();
    
    // Advanced IOSurface Property Management System v5.0 API
    IOReturn getPropertyStatistics(VMSurfacePropertyStats* stats);
    IOReturn resetPropertyStatistics();
    IOReturn flushPropertyCache();
    IOReturn invalidatePropertyCache(uint32_t surface_id);
    IOReturn prewarmPropertyCache(uint32_t* surface_ids, uint32_t count);
    IOReturn optimizePropertyCache();
    void generatePropertyReport();
    
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
    IOReturn validateVideoPixelFormat(VMIOSurfacePixelFormat format, uint32_t codec_type);
    IOReturn setupPlaneInfo(VMIOSurfacePixelFormat format, uint32_t width, uint32_t height,
                           VMIOSurfacePlaneInfo* planes, uint32_t* plane_count);
    
    // Format conversion helpers
    IOReturn performPixelFormatConversion(void* source_buffer, VMIOSurfacePixelFormat source_format,
                                         void* dest_buffer, VMIOSurfacePixelFormat dest_format,
                                         uint32_t width, uint32_t height);
    IOReturn convertBGRAtoRGBA(uint32_t* source, uint32_t* dest, uint32_t pixel_count);
    IOReturn convertRGBAtoBGRA(uint32_t* source, uint32_t* dest, uint32_t pixel_count);
    IOReturn convertBGRAtoRGB24(uint32_t* source, uint8_t* dest, uint32_t pixel_count);
    IOReturn convertRGB24toBGRA(uint8_t* source, uint32_t* dest, uint32_t pixel_count);
    IOReturn convertBGRAtoLuminance(uint32_t* source, uint8_t* dest, uint32_t pixel_count);
    
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

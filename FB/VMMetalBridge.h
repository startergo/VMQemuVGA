#ifndef VMMetalBridge_h
#define VMMetalBridge_h

#include <IOKit/IOService.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <libkern/c++/OSObject.h>
#include <libkern/c++/OSArray.h>
#include <libkern/c++/OSDictionary.h>

// Forward declarations
class VMQemuVGAAccelerator;
class VMVirtIOGPU;

// Advanced GPU Memory Synchronization System v7.0 - Struct definitions
typedef struct {
    uint32_t transfer_id;
    uint32_t buffer_id;
    uint64_t gpu_address;
    uint64_t host_address;
    uint32_t transfer_size;
    uint32_t transfer_flags;
    uint64_t start_time;
    uint64_t completion_time;
    uint32_t transfer_priority;
    uint32_t memory_pool_id;
    bool is_coherent;
    bool requires_sync;
    bool is_batched;
    char debug_label[64];
} VMGPUMemoryTransfer;

typedef struct {
    uint32_t pool_id;
    uint64_t pool_base_address;
    uint64_t pool_size;
    uint64_t allocated_size;
    uint64_t available_size;
    uint32_t allocation_count;
    uint32_t fragmentation_level;
    uint32_t access_pattern;
    bool is_coherent_pool;
    bool supports_dma;
    char pool_name[32];
} VMGPUMemoryPool;

// Advanced dependency tracking structures for System v8.0
typedef struct {
    uint32_t resource_id;
    uint32_t resource_type; // 0=Buffer, 1=Texture, 2=Pipeline, 3=Sampler
    uint32_t access_flags;  // Read/Write/Execute permissions
    uint32_t dependency_count;
    uint32_t dependent_buffers[16];
    uint64_t last_access_time;
    bool has_write_dependency;
    bool requires_memory_barrier;
} VMResourceDependencyInfo;

typedef struct {
    uint32_t source_buffer_id;
    uint32_t target_buffer_id;
    uint32_t dependency_type; // 0=RAW, 1=WAR, 2=WAW, 3=Memory
    uint32_t resource_id;
    uint64_t creation_time;
    bool is_resolved;
    bool requires_synchronization;
} VMDependencyEdge;

typedef struct {
    uint32_t buffer_id;
    uint32_t queue_id;
    uint32_t context_id;
    uint32_t priority_level;
    uint32_t command_count;
    uint32_t resource_bindings;
    uint64_t creation_time;
    uint64_t recording_start_time;
    uint64_t recording_end_time;
    uint64_t execution_time;
    uint64_t gpu_time;
    bool is_recording;
    bool is_committed;
    bool is_executed;
    bool is_reusable;
    bool has_dependencies;
    char debug_label[64];
} VMMetalCommandBufferInfo;

// Metal resource types
typedef enum {
    VM_METAL_BUFFER = 0,
    VM_METAL_TEXTURE = 1,
    VM_METAL_SAMPLER = 2,
    VM_METAL_RENDER_PIPELINE = 3,
    VM_METAL_COMPUTE_PIPELINE = 4,
    VM_METAL_COMMAND_BUFFER = 5,
    VM_METAL_COMMAND_QUEUE = 6
} VMMetalResourceType;

// Metal texture formats mapping
typedef enum {
    VM_METAL_PIXEL_FORMAT_INVALID = 0,
    VM_METAL_PIXEL_FORMAT_A8_UNORM = 1,
    VM_METAL_PIXEL_FORMAT_R8_UNORM = 10,
    VM_METAL_PIXEL_FORMAT_R8_SNORM = 12,
    VM_METAL_PIXEL_FORMAT_R8_UINT = 13,
    VM_METAL_PIXEL_FORMAT_R8_SINT = 14,
    VM_METAL_PIXEL_FORMAT_R16_UNORM = 20,
    VM_METAL_PIXEL_FORMAT_R16_SNORM = 22,
    VM_METAL_PIXEL_FORMAT_R16_UINT = 23,
    VM_METAL_PIXEL_FORMAT_R16_SINT = 24,
    VM_METAL_PIXEL_FORMAT_R16_FLOAT = 25,
    VM_METAL_PIXEL_FORMAT_RG8_UNORM = 30,
    VM_METAL_PIXEL_FORMAT_RG8_SNORM = 32,
    VM_METAL_PIXEL_FORMAT_RG8_UINT = 33,
    VM_METAL_PIXEL_FORMAT_RG8_SINT = 34,
    VM_METAL_PIXEL_FORMAT_R32_UINT = 53,
    VM_METAL_PIXEL_FORMAT_R32_SINT = 54,
    VM_METAL_PIXEL_FORMAT_R32_FLOAT = 55,
    VM_METAL_PIXEL_FORMAT_RG16_UNORM = 60,
    VM_METAL_PIXEL_FORMAT_RG16_SNORM = 62,
    VM_METAL_PIXEL_FORMAT_RG16_UINT = 63,
    VM_METAL_PIXEL_FORMAT_RG16_SINT = 64,
    VM_METAL_PIXEL_FORMAT_RG16_FLOAT = 65,
    VM_METAL_PIXEL_FORMAT_RGBA8_UNORM = 70,
    VM_METAL_PIXEL_FORMAT_RGBA8_UNORM_SRGB = 71,
    VM_METAL_PIXEL_FORMAT_RGBA8_SNORM = 72,
    VM_METAL_PIXEL_FORMAT_RGBA8_UINT = 73,
    VM_METAL_PIXEL_FORMAT_RGBA8_SINT = 74,
    VM_METAL_PIXEL_FORMAT_BGRA8_UNORM = 80,
    VM_METAL_PIXEL_FORMAT_BGRA8_UNORM_SRGB = 81,
    VM_METAL_PIXEL_FORMAT_RGB10A2_UNORM = 90,
    VM_METAL_PIXEL_FORMAT_RGB10A2_UINT = 91,
    VM_METAL_PIXEL_FORMAT_RG11B10_FLOAT = 92,
    VM_METAL_PIXEL_FORMAT_RGB9E5_FLOAT = 93
} VMMetalPixelFormat;

// Metal buffer descriptor
typedef struct {
    uint32_t length;
    uint32_t resource_options;
    uint32_t storage_mode;
    uint32_t cpu_cache_mode;
    uint32_t hazard_tracking_mode;
} VMMetalBufferDescriptor;

// Metal texture descriptor
typedef struct {
    VMMetalPixelFormat pixel_format;
    uint32_t width;
    uint32_t height;
    uint32_t depth;
    uint32_t mipmap_level_count;
    uint32_t sample_count;
    uint32_t array_length;
    uint32_t texture_type;
    uint32_t usage;
    uint32_t storage_mode;
    uint32_t cpu_cache_mode;
    uint32_t hazard_tracking_mode;
} VMMetalTextureDescriptor;

// Metal render pipeline descriptor
typedef struct {
    uint32_t vertex_function_id;
    uint32_t fragment_function_id;
    VMMetalPixelFormat color_attachment_format;
    VMMetalPixelFormat depth_attachment_format;
    VMMetalPixelFormat stencil_attachment_format;
    uint32_t sample_count;
    bool alpha_blend_enabled;
    uint32_t source_rgb_blend_factor;
    uint32_t destination_rgb_blend_factor;
    uint32_t rgb_blend_operation;
} VMMetalRenderPipelineDescriptor;

// Metal compute pipeline descriptor
typedef struct {
    uint32_t compute_function_id;
    uint32_t thread_group_size_is_multiple_of_thread_execution_width;
    uint32_t max_total_threads_per_threadgroup;
} VMMetalComputePipelineDescriptor;

// Metal draw primitives descriptor
typedef struct {
    uint32_t primitive_type;
    uint32_t vertex_start;
    uint32_t vertex_count;
    uint32_t instance_count;
    uint32_t base_instance;
} VMMetalDrawPrimitivesDescriptor;

// Metal dispatch threads descriptor
typedef struct {
    uint32_t threads_per_grid_x;
    uint32_t threads_per_grid_y;
    uint32_t threads_per_grid_z;
    uint32_t threads_per_threadgroup_x;
    uint32_t threads_per_threadgroup_y;
    uint32_t threads_per_threadgroup_z;
} VMMetalDispatchDescriptor;

/**
 * @class VMMetalBridge
 * @brief Metal Framework Bridge for VMQemuVGA 3D Acceleration
 * 
 * This class provides a bridge between the VMQemuVGA 3D acceleration system
 * and Apple's Metal graphics framework, enabling high-performance GPU-accelerated
 * rendering in virtual machines through Metal API compatibility.
 */
class VMMetalBridge : public OSObject
{
    OSDeclareDefaultStructors(VMMetalBridge);
    
private:
    VMQemuVGAAccelerator* m_accelerator;
    VMVirtIOGPU* m_gpu_device;
    IORecursiveLock* m_lock;
    
        // Metal device and command queues
    OSDictionary* m_metal_device;
    uint32_t m_primary_context_id;
    OSArray* m_command_queues;
    OSArray* m_render_pipelines;
    OSArray* m_compute_pipelines;
    OSArray* m_buffers;
    OSArray* m_textures;
    OSArray* m_samplers;
    
    // Resource tracking
    uint32_t m_next_resource_id;
    OSDictionary* m_resource_map;
    
    // Performance counters
    uint64_t m_metal_draw_calls;
    uint64_t m_metal_compute_dispatches;
    uint64_t m_metal_buffer_allocations;
    uint64_t m_metal_texture_allocations;
    
    // Feature support flags
    bool m_supports_metal_2;
    bool m_supports_metal_3;
    bool m_supports_raytracing;
    bool m_supports_variable_rate_shading;
    bool m_supports_mesh_shaders;
    
public:
    // Initialization and cleanup
    virtual bool init() override;
    virtual void free() override;
    
    // Setup and configuration
    bool initWithAccelerator(VMQemuVGAAccelerator* accelerator);
    IOReturn setupMetalDevice();
    IOReturn configureFeatureSupport();
    
    // Device management
    IOReturn createMetalDevice(uint32_t* device_id);
    IOReturn createCommandQueue(uint32_t device_id, uint32_t* queue_id);
    IOReturn createCommandBuffer(uint32_t queue_id, uint32_t* buffer_id);
    
    // Resource management
    IOReturn createBuffer(uint32_t device_id, const VMMetalBufferDescriptor* descriptor,
                         const void* initial_data, uint32_t* buffer_id);
    IOReturn createTexture(uint32_t device_id, const VMMetalTextureDescriptor* descriptor,
                          uint32_t* texture_id);
    IOReturn createSampler(uint32_t device_id, const void* descriptor, uint32_t* sampler_id);
    
    // Pipeline state management
    IOReturn createRenderPipelineState(uint32_t device_id,
                                      const VMMetalRenderPipelineDescriptor* descriptor,
                                      uint32_t* pipeline_id);
    IOReturn createComputePipelineState(uint32_t device_id,
                                       const VMMetalComputePipelineDescriptor* descriptor,
                                       uint32_t* pipeline_id);
    
    // Shader library management
    IOReturn createLibrary(uint32_t device_id, const void* source_code, size_t source_size,
                          uint32_t source_type, uint32_t* library_id);
    IOReturn createFunction(uint32_t library_id, const char* function_name,
                           uint32_t* function_id);
    
    // Command encoding
    IOReturn beginRenderPass(uint32_t command_buffer_id, uint32_t render_pass_descriptor_id);
    IOReturn endRenderPass(uint32_t command_buffer_id);
    IOReturn setRenderPipelineState(uint32_t command_buffer_id, uint32_t pipeline_id);
    IOReturn setVertexBuffer(uint32_t command_buffer_id, uint32_t buffer_id,
                            uint32_t offset, uint32_t index);
    IOReturn setFragmentBuffer(uint32_t command_buffer_id, uint32_t buffer_id,
                              uint32_t offset, uint32_t index);
    IOReturn setFragmentTexture(uint32_t command_buffer_id, uint32_t texture_id, uint32_t index);
    IOReturn setFragmentSampler(uint32_t command_buffer_id, uint32_t sampler_id, uint32_t index);
    
    // Drawing commands
    IOReturn drawPrimitives(uint32_t command_buffer_id,
                           const VMMetalDrawPrimitivesDescriptor* descriptor);
    IOReturn drawIndexedPrimitives(uint32_t command_buffer_id, uint32_t index_buffer_id,
                                  uint32_t index_count, uint32_t index_type,
                                  uint32_t index_buffer_offset, uint32_t instance_count);
    
    // Compute commands
    IOReturn setComputePipelineState(uint32_t command_buffer_id, uint32_t pipeline_id);
    IOReturn setComputeBuffer(uint32_t command_buffer_id, uint32_t buffer_id,
                             uint32_t offset, uint32_t index);
    IOReturn setComputeTexture(uint32_t command_buffer_id, uint32_t texture_id, uint32_t index);
    IOReturn dispatchThreads(uint32_t command_buffer_id,
                            const VMMetalDispatchDescriptor* descriptor);
    
    // Command buffer execution
    IOReturn commitCommandBuffer(uint32_t command_buffer_id);
    IOReturn waitForCommandBuffer(uint32_t command_buffer_id);
    bool isCommandBufferCompleted(uint32_t command_buffer_id);
    
    // Resource operations
    IOReturn copyBufferToBuffer(uint32_t command_buffer_id, uint32_t source_buffer_id,
                               uint32_t source_offset, uint32_t destination_buffer_id,
                               uint32_t destination_offset, uint32_t size);
    IOReturn copyBufferToTexture(uint32_t command_buffer_id, uint32_t buffer_id,
                                uint32_t buffer_offset, uint32_t texture_id);
    IOReturn copyTextureToBuffer(uint32_t command_buffer_id, uint32_t texture_id,
                                uint32_t buffer_id, uint32_t buffer_offset);
    IOReturn copyTextureToTexture(uint32_t command_buffer_id, uint32_t source_texture_id,
                                 uint32_t destination_texture_id);
    
    // Memory management
    IOReturn updateBuffer(uint32_t buffer_id, const void* data, uint32_t offset, uint32_t size);
    IOReturn updateTexture(uint32_t texture_id, const void* data, uint32_t mipmap_level,
                          uint32_t slice, uint32_t bytes_per_row, uint32_t bytes_per_image);
    void* mapBuffer(uint32_t buffer_id, uint32_t access_flags);
    void unmapBuffer(uint32_t buffer_id, void* mapped_pointer);
    
    // Resource cleanup
    IOReturn destroyBuffer(uint32_t buffer_id);
    IOReturn destroyTexture(uint32_t texture_id);
    IOReturn destroySampler(uint32_t sampler_id);
    IOReturn destroyRenderPipelineState(uint32_t pipeline_id);
    IOReturn destroyComputePipelineState(uint32_t pipeline_id);
    IOReturn destroyCommandBuffer(uint32_t buffer_id);
    IOReturn destroyCommandQueue(uint32_t queue_id);
    
    // Feature query
    bool supportsFeature(uint32_t feature_flag);
    uint32_t getMaxTextureSize();
    uint32_t getMaxBufferSize();
    uint32_t getMaxCommandBufferSize();
    uint32_t getMaxThreadsPerThreadgroup();
    
    // Performance and debugging
    IOReturn getPerformanceStatistics(void* stats_buffer, size_t* buffer_size);
    void resetPerformanceCounters();
    void logMetalBridgeState();
    
    // Advanced GPU Memory Synchronization and DMA Management System v7.0 - Method declarations
    // Enterprise GPU memory synchronization with comprehensive DMA management
    IOReturn performAdvancedGPUMemorySynchronization(uint32_t buffer_id, const void* data, 
                                                     uint32_t offset, uint32_t size, 
                                                     IOBufferMemoryDescriptor* buffer);
    IOReturn initializeGPUMemoryPools();
    uint32_t selectOptimalGPUMemoryPool(uint32_t transfer_size, uint32_t buffer_id);
    IOReturn expandGPUMemoryPool(uint32_t pool_id, uint32_t required_size);
    uint32_t findAvailableGPUMemoryPool(uint32_t required_size);
    uint64_t allocateGPUMemoryInPool(uint32_t pool_id, uint32_t size, uint32_t buffer_id);
    void deallocateGPUMemoryInPool(uint32_t pool_id, uint64_t gpu_address, uint32_t size);
    VMGPUMemoryTransfer* allocateTransferRecord();
    uint32_t determineTransferFlags(IOBufferMemoryDescriptor* buffer, uint32_t size);
    uint32_t determineTransferPriority(uint32_t buffer_id, uint32_t size);
    bool shouldBatchTransfer(uint32_t size, uint32_t buffer_id);
    IOReturn validateMemoryCoherency(uint32_t buffer_id, uint64_t gpu_address, uint32_t size, VMGPUMemoryPool* pool);
    void applyCoherencyOptimizations(uint32_t buffer_id, uint64_t gpu_address, uint32_t size);
    IOReturn coordinateWithGPUPipeline(uint32_t buffer_id, uint64_t gpu_address, uint32_t size, uint32_t priority);
    void updateMemoryAccessPattern(uint32_t pool_id, uint32_t size, uint32_t buffer_id);
    IOReturn scheduleForBatchProcessing(VMGPUMemoryTransfer* transfer);
    uint64_t getTotalGPUMemoryCapacity();
    void generateGPUMemorySyncReport();
    
private:
    // Internal helper methods
    OSObject* findResource(uint32_t resource_id, VMMetalResourceType expected_type);
    uint32_t allocateResourceId();
    void releaseResourceId(uint32_t resource_id);
    VMMetalPixelFormat translatePixelFormat(uint32_t vm_format);
    uint32_t translateVMPixelFormat(VMMetalPixelFormat metal_format);
    IOReturn validateDescriptor(const void* descriptor, size_t expected_size);
    IOReturn synchronizeGPUState();
    void updatePerformanceCounters(uint32_t operation_type);
    
    // Metal device setup helpers
    IOReturn createMetalDeviceAbstraction();
    IOReturn initializeMetalCommandProcessor();
    IOReturn configureMetalGPUCapabilities();
    IOReturn initializeResourceTracking();
    IOReturn setupMetalPerformanceMonitoring();
    
    // Hardware detection methods
    bool detectAppleSilicon();
    bool detectModernDiscreteGPU();
    uint32_t getMacOSVersion();
    
    // Advanced command buffer management system helpers
    IOReturn initializeCommandBufferPools();
    uint32_t selectOptimalCommandBufferPool(uint32_t queue_id);
    IOReturn expandCommandBufferPool(uint32_t pool_id);
    uint32_t findAvailableCommandBufferPool();
    uint32_t findLRUCommandBuffer();
    uint32_t determineCommandBufferPriority(uint32_t queue_id);
    IOReturn scheduleCommandBufferWorkload(uint32_t buffer_id, uint32_t priority_level);
    IOReturn initializeCommandBufferDependencies(uint32_t buffer_id);
    IOReturn registerCommandBufferForTracking(uint32_t buffer_id, uint32_t queue_id);
    void generateCommandBufferAnalyticsReport();
    
    // Advanced Command Buffer Resource Dependency Management System v8.0 - Method declarations
    // Enterprise-level dependency tracking and synchronization infrastructure
    IOReturn initializeAdvancedDependencyTracking(uint32_t buffer_id);
    IOReturn analyzeCommandBufferResourceDependencies(uint32_t buffer_id, VMMetalCommandBufferInfo* cmd_info);
    IOReturn constructDependencyGraph(uint32_t buffer_id, VMMetalCommandBufferInfo* cmd_info);
    IOReturn configureSynchronizationPrimitives(uint32_t buffer_id, VMMetalCommandBufferInfo* cmd_info);
    IOReturn setupMemoryBarriers(uint32_t buffer_id, VMMetalCommandBufferInfo* cmd_info);
    IOReturn registerDependencyValidationCallbacks(uint32_t buffer_id);
    
    // Dependency analysis helper methods
    bool analyzeResourceConflicts(uint32_t buffer_id_1, uint32_t buffer_id_2);
    bool detectMemoryHazards(VMMetalCommandBufferInfo* cmd_info_1, VMMetalCommandBufferInfo* cmd_info_2);
    uint32_t determineDependencyType(uint32_t source_buffer, uint32_t target_buffer);
    IOReturn configureSyncPrimitive(VMDependencyEdge* edge);
    IOReturn configureGPUMemoryBarrier(uint32_t buffer_id, VMResourceDependencyInfo* dependency_info);
};

#endif /* VMMetalBridge_h */
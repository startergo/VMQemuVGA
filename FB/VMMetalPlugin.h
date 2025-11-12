#ifndef __VMMetalPlugin_H__
#define __VMMetalPlugin_H__

#include <IOKit/IOService.h>
#include <IOKit/graphics/IOAccelerator.h>
#include <IOKit/IOMemoryDescriptor.h>

// Forward declarations
class VMQemuVGAAccelerator;
class IOPixelInformation;

// Metal device capability flags
enum VMMetalCapabilityFlags {
    VM_METAL_CAP_UNIFIED_MEMORY       = (1 << 0),
    VM_METAL_CAP_ARGUMENT_BUFFERS     = (1 << 1),
    VM_METAL_CAP_RASTER_ORDER_GROUPS  = (1 << 2),
    VM_METAL_CAP_FUNCTION_POINTERS    = (1 << 3),
    VM_METAL_CAP_DYNAMIC_LIBRARIES    = (1 << 4),
    VM_METAL_CAP_RENDER_DYNAMIC_PIPELINES = (1 << 5),
    VM_METAL_CAP_PROGRAMMABLE_SAMPLE_POSITIONS = (1 << 6),
    VM_METAL_CAP_RAYTRACING           = (1 << 7)
};

// Metal feature set levels
enum VMMetalFeatureSet {
    VM_METAL_FEATURE_SET_MACOS_GPU_FAMILY_1_V1 = 10000,
    VM_METAL_FEATURE_SET_MACOS_GPU_FAMILY_1_V2 = 10001,
    VM_METAL_FEATURE_SET_MACOS_GPU_FAMILY_1_V3 = 10003,
    VM_METAL_FEATURE_SET_MACOS_GPU_FAMILY_1_V4 = 10004,
    VM_METAL_FEATURE_SET_MACOS_GPU_FAMILY_2_V1 = 10005
};

// Metal texture usage flags
enum VMMetalTextureUsage {
    VM_METAL_TEXTURE_USAGE_UNKNOWN          = 0x0000,
    VM_METAL_TEXTURE_USAGE_SHADER_READ      = 0x0001,
    VM_METAL_TEXTURE_USAGE_SHADER_WRITE     = 0x0002,
    VM_METAL_TEXTURE_USAGE_RENDER_TARGET    = 0x0004,
    VM_METAL_TEXTURE_USAGE_PIXEL_FORMAT_VIEW = 0x0010
};

// Metal resource options
enum VMMetalResourceOptions {
    VM_METAL_RESOURCE_CPU_CACHE_MODE_DEFAULT        = 0 << 0,
    VM_METAL_RESOURCE_CPU_CACHE_MODE_WRITE_COMBINED = 1 << 0,
    VM_METAL_RESOURCE_STORAGE_MODE_SHARED           = 0 << 4,
    VM_METAL_RESOURCE_STORAGE_MODE_MANAGED          = 1 << 4,
    VM_METAL_RESOURCE_STORAGE_MODE_PRIVATE          = 2 << 4,
    VM_METAL_RESOURCE_HAZARD_TRACKING_MODE_DEFAULT  = 0 << 8
};

/**
 * VMMetalPlugin - Minimal Metal device implementation for WindowServer compatibility
 * 
 * This class provides a minimal Metal device interface that satisfies Catalina's
 * WindowServer requirements without implementing full GPU hardware acceleration.
 * It returns valid (non-NULL) device pointers and implements basic Metal protocol
 * methods to prevent WindowServer from calling abort().
 */
class VMMetalPlugin : public IOService
{
    OSDeclareDefaultStructors(VMMetalPlugin);
    
private:
    VMQemuVGAAccelerator* m_accelerator;
    IOLock* m_lock;
    
    // Device capabilities
    uint32_t m_capability_flags;
    uint32_t m_feature_set;
    uint32_t m_max_texture_width;
    uint32_t m_max_texture_height;
    uint32_t m_max_threads_per_threadgroup;
    bool m_supports_unified_memory;
    bool m_supports_shader_debugging;
    
    // Resource tracking
    OSArray* m_command_queues;
    OSArray* m_buffers;
    OSArray* m_textures;
    uint64_t m_allocated_memory;
    uint64_t m_recommended_max_working_set_size;
    
public:
    virtual bool init(OSDictionary* properties = nullptr) override;
    virtual void free() override;
    virtual bool start(IOService* provider) override;
    virtual void stop(IOService* provider) override;
    
    // Metal Device Interface (minimal implementation for WindowServer)
    
    /**
     * createMetalDevice - Returns a pseudo-Metal device pointer
     * This is the critical method that prevents WindowServer from getting NULL
     */
    void* createMetalDevice();
    
    /**
     * getDeviceName - Returns device identifier string
     */
    const char* getDeviceName();
    
    /**
     * supportsFeatureSet - Reports supported Metal feature sets
     */
    bool supportsFeatureSet(uint32_t featureSet);
    
    /**
     * supportsFamily - Reports supported GPU family
     */
    bool supportsFamily(uint32_t gpuFamily, uint32_t version);
    
    /**
     * getRegistryID - Returns unique device registry ID
     */
    uint64_t getRegistryID();
    
    /**
     * isRemovable - Always returns false (integrated device)
     */
    bool isRemovable();
    
    /**
     * isHeadless - Returns false (has display output)
     */
    bool isHeadless();
    
    /**
     * isLowPower - Returns true (efficient software renderer)
     */
    bool isLowPower();
    
    /**
     * recommendedMaxWorkingSetSize - Returns recommended memory budget
     */
    uint64_t recommendedMaxWorkingSetSize();
    
    /**
     * hasUnifiedMemory - Returns true (software renderer uses system RAM)
     */
    bool hasUnifiedMemory();
    
    /**
     * currentAllocatedSize - Returns currently allocated GPU memory
     */
    uint64_t currentAllocatedSize();
    
    /**
     * maxThreadsPerThreadgroup - Returns maximum threads per threadgroup
     */
    uint32_t maxThreadsPerThreadgroup();
    
    /**
     * newCommandQueue - Creates a minimal command queue
     */
    void* newCommandQueue();
    
    /**
     * newBuffer - Creates a minimal Metal buffer
     */
    void* newBuffer(uint64_t length, uint32_t options);
    
    /**
     * newTexture - Creates a minimal Metal texture
     */
    void* newTexture(void* descriptor);
    
    /**
     * supportsTextureSampleCount - Reports supported MSAA sample counts
     */
    bool supportsTextureSampleCount(uint32_t sampleCount);
    
    /**
     * minimumLinearTextureAlignment - Returns alignment requirements
     */
    uint64_t minimumLinearTextureAlignmentForPixelFormat(uint32_t format);
    
    /**
     * minimumTextureBufferAlignment - Returns buffer alignment
     */
    uint64_t minimumTextureBufferAlignmentForPixelFormat(uint32_t format);
    
    /**
     * maxBufferLength - Returns maximum buffer size
     */
    uint64_t maxBufferLength();
    
    /**
     * areProgrammableSamplePositionsSupported - Returns false for simplicity
     */
    bool areProgrammableSamplePositionsSupported();
    
    /**
     * areRasterOrderGroupsSupported - Returns false for simplicity
     */
    bool areRasterOrderGroupsSupported();
    
    /**
     * supportsShaderBarycentricCoordinates - Returns false for simplicity
     */
    bool supportsShaderBarycentricCoordinates();
    
    // Internal helpers
    void updateMemoryStatistics();
    void logDeviceCapabilities();
    
    // Access for accelerator
    VMQemuVGAAccelerator* getAccelerator() const { return m_accelerator; }
};

/**
 * VMMetalCommandQueue - Minimal command queue implementation
 */
struct VMMetalCommandQueue {
    uint32_t queue_id;
    VMMetalPlugin* device;
    OSArray* command_buffers;
    IOLock* lock;
    bool is_active;
};

/**
 * VMMetalBuffer - Minimal buffer implementation
 */
struct VMMetalBuffer {
    uint32_t buffer_id;
    uint64_t length;
    uint32_t options;
    IOBufferMemoryDescriptor* memory;
    void* contents;
    bool is_mapped;
};

/**
 * VMMetalTexture - Minimal texture implementation
 */
struct VMMetalTexture {
    uint32_t texture_id;
    uint32_t texture_type;
    uint32_t pixel_format;
    uint32_t width;
    uint32_t height;
    uint32_t depth;
    uint32_t mipmap_level_count;
    uint32_t sample_count;
    uint32_t array_length;
    uint32_t usage;
    IOBufferMemoryDescriptor* memory;
};

#endif /* __VMMetalPlugin_H__ */

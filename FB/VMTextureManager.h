#ifndef __VMTextureManager_H__
#define __VMTextureManager_H__

#include <IOKit/IOService.h>
#include <IOKit/IOMemoryDescriptor.h>
#include "VMQemuVGAMetal.h"

// Texture types
enum VMTextureType {
    VM_TEXTURE_TYPE_1D = 1,
    VM_TEXTURE_TYPE_2D = 2,
    VM_TEXTURE_TYPE_3D = 3,
    VM_TEXTURE_TYPE_CUBE = 4,
    VM_TEXTURE_TYPE_1D_ARRAY = 5,
    VM_TEXTURE_TYPE_2D_ARRAY = 6,
    VM_TEXTURE_TYPE_CUBE_ARRAY = 7,
    VM_TEXTURE_TYPE_2D_MULTISAMPLE = 8,
    VM_TEXTURE_TYPE_2D_MULTISAMPLE_ARRAY = 9
};

// Texture compression formats
enum VMTextureCompression {
    VM_TEXTURE_COMPRESSION_NONE = 0,
    VM_TEXTURE_COMPRESSION_DXT1 = 1,
    VM_TEXTURE_COMPRESSION_DXT3 = 2,
    VM_TEXTURE_COMPRESSION_DXT5 = 3,
    VM_TEXTURE_COMPRESSION_BC4 = 4,
    VM_TEXTURE_COMPRESSION_BC5 = 5,
    VM_TEXTURE_COMPRESSION_BC6H = 6,
    VM_TEXTURE_COMPRESSION_BC7 = 7,
    VM_TEXTURE_COMPRESSION_PVRTC = 8,
    VM_TEXTURE_COMPRESSION_ETC2 = 9,
    VM_TEXTURE_COMPRESSION_ASTC = 10
};

// Texture filtering modes
enum VMTextureFilter {
    VM_TEXTURE_FILTER_NEAREST = 0,
    VM_TEXTURE_FILTER_LINEAR = 1,
    VM_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST = 2,
    VM_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST = 3,
    VM_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR = 4,
    VM_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR = 5
};

// Texture wrapping modes
enum VMTextureWrap {
    VM_TEXTURE_WRAP_REPEAT = 0,
    VM_TEXTURE_WRAP_CLAMP_TO_EDGE = 1,
    VM_TEXTURE_WRAP_CLAMP_TO_BORDER = 2,
    VM_TEXTURE_WRAP_MIRRORED_REPEAT = 3
};

// Texture swizzle masks
enum VMTextureSwizzle {
    VM_TEXTURE_SWIZZLE_ZERO = 0,
    VM_TEXTURE_SWIZZLE_ONE = 1,
    VM_TEXTURE_SWIZZLE_RED = 2,
    VM_TEXTURE_SWIZZLE_GREEN = 3,
    VM_TEXTURE_SWIZZLE_BLUE = 4,
    VM_TEXTURE_SWIZZLE_ALPHA = 5
};

// Texture creation descriptor
struct VMTextureManagerDescriptor {
    VMTextureType type;
    VMTextureFormat format;
    uint32_t width;
    uint32_t height;
    uint32_t depth;
    uint32_t array_length;
    uint32_t mip_levels;
    uint32_t sample_count;
    VMResourceUsage usage;
    VMResourceStorageMode storage_mode;
    uint32_t cpu_cache_mode;
    
    // Compression settings
    VMTextureCompression compression;
    uint32_t compression_quality; // 0-100
    
    // Sampling settings
    VMTextureFilter min_filter;
    VMTextureFilter mag_filter;
    VMTextureWrap wrap_s;
    VMTextureWrap wrap_t;
    VMTextureWrap wrap_r;
    
    // Swizzling
    VMTextureSwizzle swizzle_r;
    VMTextureSwizzle swizzle_g;
    VMTextureSwizzle swizzle_b;
    VMTextureSwizzle swizzle_a;
    
    // Border color (for clamp to border)
    float border_color[4];
    
    // LOD bias and range
    float lod_bias;
    float min_lod;
    float max_lod;
    
    // Anisotropy
    uint32_t max_anisotropy;
    
    uint32_t flags;
    uint32_t reserved[4];
};

// Texture region for updates/copies
struct VMTextureRegion {
    uint32_t x, y, z;
    uint32_t width, height, depth;
    uint32_t mip_level;
    uint32_t array_slice;
};

// Mipmap generation modes
enum VMMipmapMode {
    VM_MIPMAP_MODE_NONE = 0,
    VM_MIPMAP_MODE_MANUAL = 1,
    VM_MIPMAP_MODE_AUTO_GENERATE = 2,
    VM_MIPMAP_MODE_AUTO_GENERATE_ON_WRITE = 3
};

class VMVirtIOGPU;
class VMQemuVGAAccelerator;

class VMTextureManager : public OSObject
{
    OSDeclareDefaultStructors(VMTextureManager);

private:
    VMQemuVGAAccelerator* m_accelerator;
    VMVirtIOGPU* m_gpu_device;
    
    // Texture storage
    OSArray* m_textures;
    OSArray* m_samplers;
    uint32_t m_next_texture_id;
    uint32_t m_next_sampler_id;
    
    IOLock* m_texture_lock;
    
    // Texture mapping and memory tracking
    OSDictionary* m_texture_map;
    uint64_t m_texture_memory_usage;
    uint64_t m_max_texture_memory;
    
    // Texture cache for frequently used textures
    OSArray* m_texture_cache;
    uint64_t m_cache_memory_limit;
    uint64_t m_cache_memory_used;
    
    // Texture entry
    struct ManagedTexture {
        uint32_t texture_id;
        uint32_t gpu_resource_id;
        VMTextureDescriptor descriptor;
        IOMemoryDescriptor* data;
        uint32_t data_size;
        uint64_t last_accessed;
        uint32_t ref_count;
        bool is_compressed;
        bool has_mipmaps;
        bool is_render_target;
        uint32_t* mip_offsets;    // Offset to each mip level
        uint32_t* mip_sizes;      // Size of each mip level
    };
    
    // Sampler state
    struct TextureSampler {
        uint32_t sampler_id;
        uint32_t gpu_sampler_id;
        VMTextureFilter min_filter;
        VMTextureFilter mag_filter;
        VMTextureWrap wrap_s, wrap_t, wrap_r;
        float lod_bias;
        float min_lod, max_lod;
        uint32_t max_anisotropy;
        float border_color[4];
        uint32_t ref_count;
    };
    
    // Internal methods
    ManagedTexture* findTexture(uint32_t texture_id);
    TextureSampler* findSampler(uint32_t sampler_id);
    void updateAccessTime(ManagedTexture* texture);
    
    IOReturn createTextureInternal(const VMTextureDescriptor* descriptor, 
                                  IOMemoryDescriptor* initial_data,
                                  ManagedTexture** out_texture);
    IOReturn uploadTextureData(ManagedTexture* texture, uint32_t mip_level,
                              const VMTextureRegion* region, 
                              IOMemoryDescriptor* data);
    IOReturn generateMipmaps(ManagedTexture* texture);
    IOReturn compressTexture(ManagedTexture* texture, VMTextureCompression compression);
    
    uint32_t calculateTextureSize(const VMTextureDescriptor* descriptor);
    uint32_t calculateMipSize(uint32_t width, uint32_t height, uint32_t depth,
                             VMTextureFormat format, uint32_t mip_level);
    bool isFormatCompressed(VMTextureFormat format);
    uint32_t getCompressionBlockSize(VMTextureFormat format);
    
    // Cache management
    IOReturn evictLeastRecentlyUsedTexture();
    IOReturn addToCache(ManagedTexture* texture);
    IOReturn removeFromCache(uint32_t texture_id);
    
    // Format conversion
    IOReturn convertTextureFormat(IOMemoryDescriptor* source_data,
                                 VMTextureFormat source_format,
                                 VMTextureFormat target_format,
                                 uint32_t width, uint32_t height,
                                 IOMemoryDescriptor** converted_data);

public:
    static VMTextureManager* withAccelerator(VMQemuVGAAccelerator* accelerator);
    
    virtual bool init(VMQemuVGAAccelerator* accelerator);
    virtual void free() override;
    
    // Texture creation and management
    IOReturn createTexture(const VMTextureDescriptor* descriptor,
                          IOMemoryDescriptor* initial_data,
                          uint32_t* texture_id);
    IOReturn destroyTexture(uint32_t texture_id);
    IOReturn getTextureDescriptor(uint32_t texture_id, VMTextureDescriptor* descriptor);
    
    // Texture data operations
    IOReturn updateTexture(uint32_t texture_id, uint32_t mip_level,
                          const VMTextureRegion* region,
                          IOMemoryDescriptor* data);
    IOReturn readTexture(uint32_t texture_id, uint32_t mip_level,
                        const VMTextureRegion* region,
                        IOMemoryDescriptor* output_data);
    IOReturn copyTexture(uint32_t source_texture_id, uint32_t dest_texture_id,
                        const VMTextureRegion* source_region,
                        const VMTextureRegion* dest_region);
    
    // Mipmap operations
    IOReturn generateMipmaps(uint32_t texture_id);
    IOReturn generateMipmaps(uint32_t texture_id, uint32_t base_level, uint32_t max_level);
    IOReturn setMipmapMode(uint32_t texture_id, VMMipmapMode mode);
    
    // Compression
    IOReturn compressTexture(uint32_t texture_id, VMTextureCompression compression,
                           uint32_t quality);
    IOReturn decompressTexture(uint32_t texture_id);
    bool isTextureCompressed(uint32_t texture_id);
    VMTextureCompression getTextureCompression(uint32_t texture_id);
    
    // Sampler management
    IOReturn createSampler(VMTextureFilter min_filter, VMTextureFilter mag_filter,
                          VMTextureWrap wrap_s, VMTextureWrap wrap_t, VMTextureWrap wrap_r,
                          uint32_t* sampler_id);
    IOReturn destroySampler(uint32_t sampler_id);
    IOReturn bindTextureSampler(uint32_t texture_id, uint32_t sampler_id);
    
    // Render target operations
    IOReturn createRenderTarget(uint32_t width, uint32_t height, 
                               VMTextureFormat color_format,
                               VMTextureFormat depth_format,
                               uint32_t sample_count,
                               uint32_t* color_texture_id,
                               uint32_t* depth_texture_id);
    IOReturn resizeRenderTarget(uint32_t texture_id, uint32_t new_width, uint32_t new_height);
    
    // Texture binding for rendering
    IOReturn bindTexture(uint32_t context_id, uint32_t binding_point, uint32_t texture_id);
    IOReturn unbindTexture(uint32_t context_id, uint32_t binding_point);
    IOReturn bindSampler(uint32_t context_id, uint32_t binding_point, uint32_t sampler_id);
    
    // Memory and cache management
    IOReturn setCacheMemoryLimit(uint64_t limit_bytes);
    IOReturn flushTextureCache();
    IOReturn defragmentTextureMemory();
    uint64_t getTextureMemoryUsage() const;
    uint32_t getTextureCount() const;
    
    // Statistics and debugging
    IOReturn getTextureStats(uint32_t* total_textures, uint64_t* total_memory,
                           uint32_t* cached_textures, uint64_t* cached_memory);
    IOReturn dumpTextureInfo(uint32_t texture_id, char* buffer, size_t buffer_size);
    
    // Format support queries
    bool isFormatSupported(VMTextureFormat format) const;
    bool isCompressionSupported(VMTextureCompression compression) const;
    uint32_t getMaxTextureSize() const;
    uint32_t getMaxTexture3DSize() const;
    uint32_t getMaxTextureArrayLayers() const;
    uint32_t getMaxAnisotropy() const;
};

#endif /* __VMTextureManager_H__ */

#ifndef __VMQemuVGAMetal_H__
#define __VMQemuVGAMetal_H__

#ifdef __cplusplus
extern "C" {
#endif

// Metal device capability structure
struct VMMetalDeviceCapabilities {
    uint32_t max_texture_width;
    uint32_t max_texture_height;
    uint32_t max_texture_depth;
    uint32_t max_texture_array_layers;
    uint32_t max_vertex_buffers;
    uint32_t max_fragment_textures;
    uint32_t max_compute_textures;
    uint32_t max_threadgroup_memory;
    uint32_t supports_tessellation;
    uint32_t supports_geometry_shaders;
    uint32_t supports_compute_shaders;
    uint32_t supports_indirect_draw;
    uint32_t supports_base_vertex_instance;
    uint32_t max_color_render_targets;
    uint32_t supports_memoryless_render_targets;
    uint64_t max_buffer_size;
    uint32_t buffer_alignment;
    uint32_t max_threads_per_threadgroup;
    uint32_t supports_function_pointers;
    uint32_t supports_dynamic_libraries;
    uint32_t supports_raytracing;
    uint32_t reserved[43]; // Pad to 256 bytes
};

// GPU family identification for Metal compatibility
enum VMGPUFamily {
    VMGPUFamilyApple1 = 1001,
    VMGPUFamilyApple2 = 1002,
    VMGPUFamilyApple3 = 1003,
    VMGPUFamilyApple4 = 1004,
    VMGPUFamilyApple5 = 1005,
    VMGPUFamilyApple6 = 1006,
    VMGPUFamilyApple7 = 1007,
    VMGPUFamilyApple8 = 1008,
    VMGPUFamilyMac1   = 2001,
    VMGPUFamilyMac2   = 2002,
    VMGPUFamilyCommon1 = 3001,
    VMGPUFamilyCommon2 = 3002,
    VMGPUFamilyCommon3 = 3003,
    VMGPUFamilyVirtual = 9001 // Our virtualized GPU
};

// Texture formats compatible with Metal
enum VMTextureFormat {
    VMTextureFormatInvalid = 0,
    
    // 8-bit formats
    VMTextureFormatA8Unorm = 1,
    VMTextureFormatR8Unorm = 10,
    VMTextureFormatR8Snorm = 12,
    VMTextureFormatR8Uint = 13,
    VMTextureFormatR8Sint = 14,
    
    // 16-bit formats
    VMTextureFormatR16Unorm = 20,
    VMTextureFormatR16Snorm = 22,
    VMTextureFormatR16Uint = 23,
    VMTextureFormatR16Sint = 24,
    VMTextureFormatR16Float = 25,
    VMTextureFormatRG8Unorm = 30,
    VMTextureFormatRG8Snorm = 32,
    VMTextureFormatRG8Uint = 33,
    VMTextureFormatRG8Sint = 34,
    
    // 32-bit formats
    VMTextureFormatR32Uint = 53,
    VMTextureFormatR32Sint = 54,
    VMTextureFormatR32Float = 55,
    VMTextureFormatRG16Unorm = 60,
    VMTextureFormatRG16Snorm = 62,
    VMTextureFormatRG16Uint = 63,
    VMTextureFormatRG16Sint = 64,
    VMTextureFormatRG16Float = 65,
    VMTextureFormatRGBA8Unorm = 70,
    VMTextureFormatRGBA8Unorm_sRGB = 71,
    VMTextureFormatRGBA8Snorm = 72,
    VMTextureFormatRGBA8Uint = 73,
    VMTextureFormatRGBA8Sint = 74,
    VMTextureFormatBGRA8Unorm = 80,
    VMTextureFormatBGRA8Unorm_sRGB = 81,
    
    // 64-bit formats
    VMTextureFormatRG32Uint = 103,
    VMTextureFormatRG32Sint = 104,
    VMTextureFormatRG32Float = 105,
    VMTextureFormatRGBA16Unorm = 110,
    VMTextureFormatRGBA16Snorm = 112,
    VMTextureFormatRGBA16Uint = 113,
    VMTextureFormatRGBA16Sint = 114,
    VMTextureFormatRGBA16Float = 115,
    
    // 128-bit formats
    VMTextureFormatRGBA32Uint = 123,
    VMTextureFormatRGBA32Sint = 124,
    VMTextureFormatRGBA32Float = 125,
    
    // Depth/Stencil formats
    VMTextureFormatDepth16Unorm = 250,
    VMTextureFormatDepth32Float = 252,
    VMTextureFormatStencil8 = 253,
    VMTextureFormatDepth24Unorm_Stencil8 = 255,
    VMTextureFormatDepth32Float_Stencil8 = 260,
    
    // Compressed formats
    VMTextureFormatBC1_RGBA = 130,
    VMTextureFormatBC1_RGBA_sRGB = 131,
    VMTextureFormatBC2_RGBA = 132,
    VMTextureFormatBC2_RGBA_sRGB = 133,
    VMTextureFormatBC3_RGBA = 134,
    VMTextureFormatBC3_RGBA_sRGB = 135,
    VMTextureFormatBC4_RUnorm = 140,
    VMTextureFormatBC4_RSnorm = 141,
    VMTextureFormatBC5_RGUnorm = 142,
    VMTextureFormatBC5_RGSnorm = 143,
    VMTextureFormatBC6H_RGBFloat = 150,
    VMTextureFormatBC6H_RGBUfloat = 151,
    VMTextureFormatBC7_RGBAUnorm = 152,
    VMTextureFormatBC7_RGBAUnorm_sRGB = 153
};

// Resource usage flags
enum VMResourceUsage {
    VMResourceUsageShaderRead = 1 << 0,
    VMResourceUsageShaderWrite = 1 << 1,
    VMResourceUsageRenderTarget = 1 << 2,
    VMResourceUsageBlitSource = 1 << 3,
    VMResourceUsageBlitDestination = 1 << 4,
    VMResourceUsagePixelFormatView = 1 << 5
};

// Storage modes for resources
enum VMResourceStorageMode {
    VMResourceStorageModeShared = 0,
    VMResourceStorageModeManaged = 1,
    VMResourceStorageModePrivate = 2,
    VMResourceStorageModeMemoryless = 3
};

// GPU command buffer state
enum VMCommandBufferStatus {
    VMCommandBufferStatusNotEnqueued = 0,
    VMCommandBufferStatusEnqueued = 1,
    VMCommandBufferStatusCommitted = 2,
    VMCommandBufferStatusScheduled = 3,
    VMCommandBufferStatusCompleted = 4,
    VMCommandBufferStatusError = 5
};

// Metal-compatible pipeline state descriptor
struct VMRenderPipelineDescriptor {
    uint64_t vertex_function_id;
    uint64_t fragment_function_id;
    VMTextureFormat color_attachments[8];
    VMTextureFormat depth_attachment_format;
    VMTextureFormat stencil_attachment_format;
    uint32_t sample_count;
    uint32_t alpha_to_coverage_enabled;
    uint32_t alpha_to_one_enabled;
    uint32_t rasterization_enabled;
    uint32_t reserved[7];
};

// Compute pipeline state descriptor
struct VMComputePipelineDescriptor {
    uint64_t compute_function_id;
    uint32_t max_total_threads_per_threadgroup;
    uint32_t support_indirect_command_buffers;
    uint32_t reserved[14];
};

// Buffer descriptor
struct VMBufferDescriptor {
    uint64_t length;
    VMResourceStorageMode storage_mode;
    uint32_t cpu_cache_mode;
    VMResourceUsage usage;
    uint32_t reserved[12];
};

// Texture descriptor
struct VMTextureDescriptor {
    uint32_t texture_type;
    VMTextureFormat pixel_format;
    uint32_t width;
    uint32_t height;
    uint32_t depth;
    uint32_t mipmap_level_count;
    uint32_t sample_count;
    uint32_t array_length;
    VMResourceStorageMode storage_mode;
    uint32_t cpu_cache_mode;
    VMResourceUsage usage;
    uint32_t reserved[5];
};

#ifdef __cplusplus
}
#endif

#endif /* __VMQemuVGAMetal_H__ */

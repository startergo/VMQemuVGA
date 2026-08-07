#ifndef VREND_METAL_PRIV_H
#define VREND_METAL_PRIV_H

#import <Metal/Metal.h>
#import <Foundation/Foundation.h>
#include <mach/mach_port.h>
#include <stdbool.h>
#include "virgl_protocol.h"
#include "vrend_metal_formats.h"

#define VREND_MAX_CONST_BUFFERS 16
#define VREND_MAX_VERTEX_ATTRIBUTES 16
#define VREND_MAX_SAMPLERS 16
#define VREND_MAX_STREAMOUT_TARGETS 4
#define VREND_MAX_CLIP_PLANES 8
#define VREND_MAX_SHADER_BUFFERS 16

#define VREND_METAL_CONST_BASE 32
#define VREND_METAL_CLIP_STATE_INDEX (VREND_METAL_CONST_BASE + VREND_MAX_CONST_BUFFERS)
#define VREND_METAL_STREAMOUT_BUFFER_BASE (VREND_METAL_CLIP_STATE_INDEX + 1)
#define VREND_METAL_STREAMOUT_META_INDEX (VREND_METAL_STREAMOUT_BUFFER_BASE + VREND_MAX_STREAMOUT_TARGETS)
#define VREND_METAL_SHADER_BUFFER_BASE (VREND_METAL_STREAMOUT_META_INDEX + 1)

enum vrend_metal_shader_stage {
    VREND_METAL_STAGE_VERTEX = 0,
    VREND_METAL_STAGE_FRAGMENT = 1,
    VREND_METAL_STAGE_COMPUTE = 2,
    VREND_METAL_STAGE_COUNT = 3,
};

enum vrend_pipe_tex_wrap {
    VREND_PIPE_TEX_WRAP_REPEAT = 0,
    VREND_PIPE_TEX_WRAP_CLAMP = 1,
    VREND_PIPE_TEX_WRAP_CLAMP_TO_EDGE = 2,
    VREND_PIPE_TEX_WRAP_CLAMP_TO_BORDER = 3,
    VREND_PIPE_TEX_WRAP_MIRROR_REPEAT = 4,
    VREND_PIPE_TEX_WRAP_MIRROR_CLAMP = 5,
    VREND_PIPE_TEX_WRAP_MIRROR_CLAMP_TO_EDGE = 6,
    VREND_PIPE_TEX_WRAP_MIRROR_CLAMP_TO_BORDER = 7,
};

enum vrend_pipe_tex_filter {
    VREND_PIPE_TEX_FILTER_NEAREST = 0,
    VREND_PIPE_TEX_FILTER_LINEAR = 1,
};

enum vrend_pipe_tex_mipfilter {
    VREND_PIPE_TEX_MIPFILTER_NONE = 0,
    VREND_PIPE_TEX_MIPFILTER_NEAREST = 1,
    VREND_PIPE_TEX_MIPFILTER_LINEAR = 2,
};

enum vrend_pipe_tex_compare_mode {
    VREND_PIPE_TEX_COMPARE_NONE = 0,
    VREND_PIPE_TEX_COMPARE_R_TO_TEXTURE = 1,
};

enum vrend_pipe_swizzle {
    VREND_PIPE_SWIZZLE_X = 0,
    VREND_PIPE_SWIZZLE_Y = 1,
    VREND_PIPE_SWIZZLE_Z = 2,
    VREND_PIPE_SWIZZLE_W = 3,
    VREND_PIPE_SWIZZLE_0 = 4,
    VREND_PIPE_SWIZZLE_1 = 5,
};

struct vrend_metal_sampler_state_desc {
    float border_color[4];
    float lod_bias;
    float min_lod;
    float max_lod;
    uint32_t wrap_s;
    uint32_t wrap_t;
    uint32_t wrap_r;
    uint32_t min_img_filter;
    uint32_t mag_img_filter;
    uint32_t min_mip_filter;
    uint32_t compare_mode;
    uint32_t compare_func;
    uint32_t normalized_coords;
    uint32_t max_anisotropy;
    uint32_t seamless_cube_map;
    uint32_t reduction_mode;
};

struct vrend_metal_sampler_view_desc {
    uint32_t resource_id;
    uint32_t format;
    uint32_t target;
    uint32_t first_level;
    uint32_t last_level;
    uint32_t first_layer;
    uint32_t last_layer;
    uint32_t swizzle_r;
    uint32_t swizzle_g;
    uint32_t swizzle_b;
    uint32_t swizzle_a;
};

struct vrend_metal_vertex_element {
    uint32_t offset;
    uint32_t instance_divisor;
    uint32_t buffer_index;
    uint32_t format;
};

struct vrend_metal_vertex_elements_state {
    uint32_t count;
    uint32_t unsupported_mask;
    struct vrend_metal_vertex_element elements[VREND_MAX_VERTEX_ATTRIBUTES];
};

struct vrend_metal_constant_binding {
    id<MTLBuffer> buffer;
    uint32_t offset;
    uint32_t length;
};

struct vrend_metal_shader_buffer_binding {
    id<MTLBuffer> buffer;
    uint32_t offset;
    uint32_t length;
};

struct vrend_metal_streamout_target_desc {
    uint32_t resource_handle;
    uint32_t buffer_offset;
    uint32_t buffer_size;
};

struct vrend_metal_streamout_binding {
    id<MTLBuffer> buffer;
    uint32_t resource_handle;
    uint32_t handle;
    uint32_t offset;
    uint32_t size;
    bool append;
};

struct vrend_metal_clip_state {
    float planes[VREND_MAX_CLIP_PLANES][4];
    uint32_t enabled_mask;
    bool dirty;
};

struct vrend_metal_tessellation_state {
    uint32_t patch_vertices;
    float default_outer_level[4];
    float default_inner_level[2];
    bool valid;
    bool dirty;
};

struct vrend_metal_context {
    uint32_t ctx_id;
    char debug_name[64];
    
    /* Metal rendering state */
    id<MTLCommandBuffer> command_buffer;
    id<MTLRenderCommandEncoder> render_encoder;
    id<MTLComputeCommandEncoder> compute_encoder;
    
    /* Shader and pipeline storage */
    NSMutableDictionary *metal_shaders;
    NSMutableDictionary *metal_pipelines;
    NSMutableDictionary *metal_depth_states;
    NSMutableDictionary *metal_compute_pipelines;
    
    /* Resource storage */
    NSMutableDictionary *metal_textures;
    NSMutableDictionary *metal_buffers;
    
    /* Current pipeline state */
    uint32_t current_pipeline;
    uint32_t bound_vertex_shader;
    uint32_t bound_fragment_shader;
    uint32_t bound_geometry_shader;
    uint32_t bound_tess_ctrl_shader;
    uint32_t bound_tess_eval_shader;
    uint32_t bound_compute_shader;
    uint32_t bound_blend_state;
    uint32_t bound_depth_state;
    id<MTLDepthStencilState> current_depth_stencil_state;
    uint32_t current_compute_pipeline;
    struct vrend_metal_tessellation_state tess_state;
    
    /* Framebuffer state */
    id<MTLTexture> framebuffer_color[8];
    uint32_t framebuffer_color_count;
    id<MTLTexture> framebuffer_depth;
    id<MTLTexture> framebuffer_stencil;
    id<MTLTexture> scanout_texture;
    
    /* Viewport state */
    MTLViewport viewport;
    
    /* Vertex buffer state */
    id<MTLBuffer> vertex_buffers[16];
    uint32_t vertex_buffer_offsets[16];
    uint32_t vertex_buffer_strides[16];
    uint32_t vertex_buffer_count;
    id<MTLBuffer> index_buffer;
    uint32_t index_buffer_handle;
    uint32_t index_buffer_offset;
    uint32_t index_buffer_stride;
    MTLIndexType index_buffer_type;
    uint32_t bound_vertex_elements;
    NSMutableDictionary *vertex_elements; /* handle → NSData(vrend_metal_vertex_elements_state) */
    NSMutableDictionary *sampler_states;
    NSMutableDictionary *sampler_state_objects;
    NSMutableDictionary *sampler_views;
    id<MTLSamplerState> bound_sampler_states[VREND_METAL_STAGE_COUNT][VREND_MAX_SAMPLERS];
    uint32_t sampler_state_handles[VREND_METAL_STAGE_COUNT][VREND_MAX_SAMPLERS];
    id<MTLTexture> bound_sampler_views[VREND_METAL_STAGE_COUNT][VREND_MAX_SAMPLERS];
    uint32_t sampler_view_handles[VREND_METAL_STAGE_COUNT][VREND_MAX_SAMPLERS];
    NSMutableDictionary *streamout_targets;
    struct vrend_metal_streamout_binding streamout_bindings[VREND_MAX_STREAMOUT_TARGETS];
    uint32_t streamout_append_mask;
    struct vrend_metal_clip_state clip_state;
    
    /* Object tracking */
    NSMutableDictionary *blend_states;
    NSMutableDictionary *depth_states;
    
    /* Constant/uniform buffers */
    struct vrend_metal_constant_binding constant_buffers[VREND_METAL_STAGE_COUNT][VREND_MAX_CONST_BUFFERS];
    struct vrend_metal_shader_buffer_binding shader_buffers[VREND_METAL_STAGE_COUNT][VREND_MAX_SHADER_BUFFERS];

    /* Fence tracking */
    NSLock *fence_lock;
    NSMutableDictionary *fence_command_buffers; /* fence_id -> id<MTLCommandBuffer> */
    uint64_t fence_serial;
    NSMutableDictionary *fence_event_values; /* fence_id -> NSNumber(value) */
    id<MTLSharedEvent> shared_event;
    MTLSharedEventHandle *shared_event_handle;
    mach_port_t shared_event_port;
    uint64_t shared_event_value;
};

void vrend_metal_bind_constant_buffers_internal(struct vrend_metal_context *ctx);
void vrend_metal_bind_shader_buffers_internal(struct vrend_metal_context *ctx);
void vrend_metal_apply_clip_state(struct vrend_metal_context *ctx);
void vrend_metal_bind_streamout_buffers(struct vrend_metal_context *ctx);
void vrend_metal_apply_bound_samplers(struct vrend_metal_context *ctx);
void vrend_metal_bind_sampler_state_slot(struct vrend_metal_context *ctx,
                                         uint32_t stage,
                                         uint32_t slot,
                                         uint32_t handle);
void vrend_metal_bind_sampler_view_slot(struct vrend_metal_context *ctx,
                                        uint32_t stage,
                                        uint32_t slot,
                                        uint32_t handle);
MTLVertexFormat vrend_metal_vertex_format_from_pipe(uint32_t format);
bool vrend_metal_pipe_format_supported(uint32_t format);

#endif /* VREND_METAL_PRIV_H */

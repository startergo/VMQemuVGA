/*
 * virglrenderer Metal Backend - Header
 * 
 * Integration of OpenGL→Metal translation for virtio-gpu 3D acceleration
 * Based on existing metal_server.m implementation
 */

#ifndef VREND_METAL_H
#define VREND_METAL_H

#include <stdint.h>
#include <stdbool.h>
#include <mach/mach_port.h>

/* Metal capset ID - register in virtio-gpu spec */
#define VIRTIO_GPU_CAPSET_METAL 7

/*
 * Standalone virglrenderer-metal types.
 *
 * This repo is a proof-of-concept backend and includes a small local test
 * program. Avoid depending on virglrenderer internal headers here because:
 *  - the virglrenderer build generates headers that are not present in a
 *    source-only checkout (e.g. u_format_gen.h), and
 *  - virglrenderer struct layouts differ from this POC.
 */

struct virgl_box {
    uint32_t x, y, z;
    uint32_t w, h, d;
};

/* Minimal pipe_resource snapshot used by this POC. */
struct pipe_resource {
    uint32_t bind;
    uint32_t width0;
    uint32_t height0;
    uint32_t depth0;
    uint32_t format;
};

/* Resource layout expected by vrend_metal.m and test_backend.c. */
struct virgl_resource {
    uint32_t res_id;
    uint32_t resource_id;
    uint32_t width;
    uint32_t height;
    uint32_t depth;
    uint32_t format;
    uint32_t bind;
    uint32_t target;
    struct pipe_resource *pipe_resource;
};

/* Opaque base pointer; vrend_metal.m casts this to vrend_metal_context. */
struct virgl_context;

/* Metal capabilities structure */
struct virgl_metal_caps {
    uint32_t metal_version;           /* MTLFeatureSet version */
    uint32_t max_texture_size;
    uint32_t max_texture_layers;
    uint32_t max_buffer_size;
    uint32_t supports_tessellation;
    uint32_t supports_argument_buffers;
    uint32_t supports_indirect_command_buffers;
    uint32_t supports_depth_clip_mode;
    uint32_t max_threads_per_threadgroup;
    uint32_t reserved[23];            /* Future expansion */
};

struct vrend_metal_draw_info {
    uint32_t start;
    uint32_t count;
    uint32_t mode;
    bool indexed;
    uint32_t instance_count;
    int32_t index_bias;
    uint32_t start_instance;
    bool primitive_restart;
    uint32_t restart_index;
    uint32_t min_index;
    uint32_t max_index;
};

struct vrend_metal_grid_info {
    uint32_t block[3];
    uint32_t grid[3];
    bool indirect;
    uint32_t indirect_handle;
    uint32_t indirect_offset;
};

/* Backend initialization */
int vrend_metal_init(uint32_t flags);
void vrend_metal_cleanup(void);
void vrend_metal_get_caps(struct virgl_metal_caps *caps);

/* Context management */
struct virgl_context* vrend_metal_create_context(
    uint32_t ctx_id,
    uint32_t nlen,
    const char *debug_name);

void vrend_metal_destroy_context(struct virgl_context *ctx);

/* Resource management */
int vrend_metal_create_resource(
    struct virgl_context *ctx,
    struct virgl_resource *res);

void vrend_metal_destroy_resource(
    struct virgl_context *ctx,
    struct virgl_resource *res);

int vrend_metal_resource_attach_backing(
    struct virgl_context *ctx,
    struct virgl_resource *res,
    void **backing_pages,
    uint32_t nr_pages);

void vrend_metal_resource_detach_backing(
    struct virgl_context *ctx,
    struct virgl_resource *res);

/* Blob resources (zero-copy) */
int vrend_metal_resource_create_blob(
    struct virgl_context *ctx,
    uint32_t blob_id,
    uint64_t size,
    uint32_t blob_mem,
    uint32_t blob_flags);

int vrend_metal_resource_map_blob(
    struct virgl_context *ctx,
    uint32_t resource_id,
    uint64_t offset);

void vrend_metal_resource_unmap_blob(
    struct virgl_context *ctx,
    uint32_t resource_id);

/* Data transfer */
int vrend_metal_transfer_inline_write(
    struct virgl_context *ctx,
    struct virgl_resource *res,
    const struct virgl_box *box,
    const void *data,
    uint32_t stride,
    uint32_t layer_stride);

int vrend_metal_transfer_to_host(
    struct virgl_context *ctx,
    struct virgl_resource *res,
    const struct virgl_box *box,
    uint32_t level);

int vrend_metal_transfer_from_host(
    struct virgl_context *ctx,
    struct virgl_resource *res,
    const struct virgl_box *box,
    uint32_t level);

/* Command submission */
int vrend_metal_submit_cmd(
    struct virgl_context *ctx,
    const void *cmd_buf,
    uint32_t cmd_size);

/* Shader management */
int vrend_metal_create_shader(
    struct virgl_context *ctx,
    uint32_t shader_id,
    uint32_t shader_type,
    const char *shader_text,
    uint32_t text_length);

void vrend_metal_destroy_shader(
    struct virgl_context *ctx,
    uint32_t shader_id);

int vrend_metal_bind_shader(
    struct virgl_context *ctx,
    uint32_t shader_id);

/* Pipeline state */
int vrend_metal_create_pipeline(
    struct virgl_context *ctx,
    uint32_t pipeline_id);

int vrend_metal_bind_pipeline(
    struct virgl_context *ctx,
    uint32_t pipeline_id);

/* Notify backend that shader/blend/depth state changed so pipeline cache can rebuild */
void vrend_metal_request_pipeline_update(
    struct virgl_context *ctx);

/* Rendering */
int vrend_metal_clear(
    struct virgl_context *ctx,
    uint32_t buffers,
    const float *color,
    double depth,
    uint32_t stencil);

int vrend_metal_draw_vbo(
    struct virgl_context *ctx,
    const struct vrend_metal_draw_info *info);

int vrend_metal_launch_grid(
    struct virgl_context *ctx,
    const struct vrend_metal_grid_info *info);

/* Synchronization */
int vrend_metal_create_fence(
    struct virgl_context *ctx,
    uint64_t fence_id);

int vrend_metal_wait_fence(
    struct virgl_context *ctx,
    uint64_t fence_id);

struct vrend_metal_shared_event_info {
    uint32_t ctx_id;
    mach_port_t mach_port;
    uint64_t shared_event_handle;
    uint64_t signal_value;
};

struct vrend_metal_fence_sync_info {
    uint32_t ctx_id;
    uint64_t fence_id;
    uint64_t event_value;
    mach_port_t mach_port;
};

typedef void (*vrend_metal_shared_event_listener)(
    const struct vrend_metal_shared_event_info *info,
    void *userdata);

int vrend_metal_get_shared_event_info(
    uint32_t ctx_id,
    struct vrend_metal_shared_event_info *info);

int vrend_metal_get_fence_sync_info(
    uint32_t ctx_id,
    uint64_t fence_id,
    struct vrend_metal_fence_sync_info *info);

void vrend_metal_register_shared_event_listener(
    vrend_metal_shared_event_listener listener,
    void *userdata);

/* Scanout / Display */
typedef void (*vrend_metal_scanout_callback)(
    uint32_t scanout_id,
    const struct virgl_box *region,
    const void *pixels,
    uint32_t bytes_per_row,
    uint32_t width,
    uint32_t height,
    uint64_t frame,
    void *userdata);

void vrend_metal_register_scanout_callback(
    vrend_metal_scanout_callback callback,
    void *userdata);

struct vrend_metal_scanout_surface_info {
    uint32_t scanout_id;
    uint32_t ctx_id;
    uint32_t width;
    uint32_t height;
    uint32_t bytes_per_row;
    uint32_t pixel_format_fourcc;
    uint32_t iosurface_id;
    uint32_t metal_pixel_format;
    uint32_t reserved;
    uint64_t frame_id;
};

bool vrend_metal_scanout_supports_iosurface(void);
int vrend_metal_get_scanout_iosurface(
    uint32_t scanout_id,
    struct vrend_metal_scanout_surface_info *info);

int vrend_metal_set_scanout(
    uint32_t scanout_id,
    uint32_t resource_id,
    const struct virgl_box *box);

int vrend_metal_flush_resource(
    struct virgl_context *ctx,
    uint32_t resource_id,
    const struct virgl_box *box);

void vrend_metal_set_scanout_throttle(
    uint32_t scanout_id,
    double max_fps);

#endif /* VREND_METAL_H */

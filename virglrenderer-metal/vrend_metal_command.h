/*
 * virglrenderer Metal Backend - Command Stream Parser
 * 
 * Parses virgl command stream and translates to Metal API calls
 */

#ifndef VREND_METAL_COMMAND_H
#define VREND_METAL_COMMAND_H

#include <stdint.h>
#include "../src/virgl_protocol.h"

/* Forward declarations */
struct virgl_context;

/* Command parsing result */
struct vrend_metal_command {
    uint32_t opcode;
    uint32_t length;
    const uint32_t *data;
};

/* Parse and execute virgl command stream */
int vrend_metal_parse_command_stream(
    struct virgl_context *ctx,
    const void *cmd_buf,
    uint32_t cmd_size);

/* Individual command handlers */
int vrend_metal_cmd_create_object(struct virgl_context *ctx, const uint32_t *cmd, uint32_t cmd_dwords);
int vrend_metal_cmd_bind_object(struct virgl_context *ctx, const uint32_t *cmd, uint32_t cmd_dwords);
int vrend_metal_cmd_destroy_object(struct virgl_context *ctx, const uint32_t *cmd, uint32_t cmd_dwords);
int vrend_metal_cmd_set_viewport(struct virgl_context *ctx, const uint32_t *cmd, uint32_t cmd_dwords);
int vrend_metal_cmd_set_framebuffer(struct virgl_context *ctx, const uint32_t *cmd, uint32_t cmd_dwords);
int vrend_metal_cmd_clear_buffers(struct virgl_context *ctx, const uint32_t *cmd, uint32_t cmd_dwords);
int vrend_metal_cmd_draw_vbo(struct virgl_context *ctx, const uint32_t *cmd, uint32_t cmd_dwords);
int vrend_metal_cmd_resource_write(struct virgl_context *ctx, const uint32_t *cmd, uint32_t cmd_dwords);
int vrend_metal_cmd_set_vertex_buffers(struct virgl_context *ctx, const uint32_t *cmd, uint32_t cmd_dwords);
int vrend_metal_cmd_set_index_buffer(struct virgl_context *ctx, const uint32_t *cmd, uint32_t cmd_dwords);
int vrend_metal_cmd_set_sampler_views(struct virgl_context *ctx, const uint32_t *cmd, uint32_t cmd_dwords);
int vrend_metal_cmd_bind_shader(struct virgl_context *ctx, const uint32_t *cmd, uint32_t cmd_dwords);
int vrend_metal_cmd_set_constant_buffer(struct virgl_context *ctx, const uint32_t *cmd, uint32_t cmd_dwords);
int vrend_metal_cmd_bind_sampler_states(struct virgl_context *ctx, const uint32_t *cmd, uint32_t cmd_dwords);
int vrend_metal_cmd_set_tess_state(struct virgl_context *ctx, const uint32_t *cmd, uint32_t cmd_dwords);
int vrend_metal_cmd_launch_grid(struct virgl_context *ctx, const uint32_t *cmd, uint32_t cmd_dwords);
int vrend_metal_cmd_set_clip_state(struct virgl_context *ctx, const uint32_t *cmd, uint32_t cmd_dwords);
int vrend_metal_cmd_set_streamout_targets(struct virgl_context *ctx, const uint32_t *cmd, uint32_t cmd_dwords);
int vrend_metal_cmd_set_shader_buffers(struct virgl_context *ctx, const uint32_t *cmd, uint32_t cmd_dwords);

#endif /* VREND_METAL_COMMAND_H */

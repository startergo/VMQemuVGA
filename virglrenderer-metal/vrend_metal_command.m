/*
 * virglrenderer Metal Backend - Command Stream Parser Implementation
 */

#import <Metal/Metal.h>
#import <Foundation/Foundation.h>
#include "vrend_metal_command.h"
#include "vrend_metal.h"
#include "vrend_metal_priv.h"
#include "pipe/p_defines.h"
#include "virgl_protocol.h"
#include <limits.h>
#include <string.h>

static const char *vrend_metal_primitive_name(uint32_t mode) {
    switch (mode) {
        case PIPE_PRIM_POINTS: return "points";
        case PIPE_PRIM_LINES: return "lines";
        case PIPE_PRIM_LINE_LOOP: return "line_loop";
        case PIPE_PRIM_LINE_STRIP: return "line_strip";
        case PIPE_PRIM_TRIANGLES: return "triangles";
        case PIPE_PRIM_TRIANGLE_STRIP: return "triangle_strip";
        case PIPE_PRIM_TRIANGLE_FAN: return "triangle_fan";
        case PIPE_PRIM_QUADS: return "quads";
        case PIPE_PRIM_QUAD_STRIP: return "quad_strip";
        case PIPE_PRIM_POLYGON: return "polygon";
        case PIPE_PRIM_PATCHES: return "patches";
        default: return "unknown";
    }
}

static int32_t vrend_metal_stage_from_pipe_shader(uint32_t shader_type) {
    switch (shader_type) {
        case PIPE_SHADER_VERTEX:
            return VREND_METAL_STAGE_VERTEX;
        case PIPE_SHADER_FRAGMENT:
            return VREND_METAL_STAGE_FRAGMENT;
        case PIPE_SHADER_COMPUTE:
            return VREND_METAL_STAGE_COMPUTE;
        default:
            return -1;
    }
}

static void vrend_metal_clear_streamout_binding(struct vrend_metal_streamout_binding *binding) {
    if (!binding) {
        return;
    }
    binding->buffer = nil;
    binding->resource_handle = 0;
    binding->handle = 0;
    binding->offset = 0;
    binding->size = 0;
    binding->append = false;
}

static uint32_t vrend_metal_backend_mode_from_pipe(uint32_t pipe_mode) {
    switch (pipe_mode) {
        case PIPE_PRIM_POINTS:
            return PIPE_PRIM_POINTS;
        case PIPE_PRIM_LINES:
            return PIPE_PRIM_LINES;
        case PIPE_PRIM_LINE_LOOP:
        case PIPE_PRIM_LINE_STRIP:
            return PIPE_PRIM_LINE_STRIP;
        case PIPE_PRIM_TRIANGLES:
            return PIPE_PRIM_TRIANGLES;
        case PIPE_PRIM_TRIANGLE_STRIP:
            return PIPE_PRIM_TRIANGLE_STRIP;
        default:
            return UINT32_MAX;
    }
}

static uint32_t vrend_metal_promote_pipe_mode(uint32_t pipe_mode,
                                              struct vrend_metal_context *ctx,
                                              bool geometry_active,
                                              bool tess_active) {
    (void)geometry_active;
    switch (pipe_mode) {
        case PIPE_PRIM_LINE_LOOP:
            NSLog(@"[Command]   line_loop → line_strip (multi-pass emulation; closing edge handled in UI)");
            return PIPE_PRIM_LINE_STRIP;
        case PIPE_PRIM_TRIANGLE_FAN:
            NSLog(@"[Command]   triangle_fan → triangle_strip (vertex amplification fallback)");
            return PIPE_PRIM_TRIANGLE_STRIP;
        case PIPE_PRIM_QUADS:
        case PIPE_PRIM_QUAD_STRIP:
        case PIPE_PRIM_POLYGON:
            NSLog(@"[Command]   %@ → triangles (vertex amplification fallback)",
                  @(vrend_metal_primitive_name(pipe_mode)));
            return PIPE_PRIM_TRIANGLES;
        case PIPE_PRIM_PATCHES:
            if (!tess_active || !ctx->tess_state.valid || ctx->tess_state.patch_vertices == 0) {
                NSLog(@"[Command]   patches without valid tess state; defaulting to 3-control-point fan");
            } else {
                NSLog(@"[Command]   patches (%u control points) routed through triangle list emulation",
                      ctx->tess_state.patch_vertices);
            }
            return PIPE_PRIM_TRIANGLES;
        default:
            return pipe_mode;
    }
}

static uint32_t vrend_metal_resolve_draw_mode(uint32_t requested_mode,
                                              struct vrend_metal_context *ctx,
                                              bool geometry_active,
                                              bool tess_active) {
    uint32_t promoted = vrend_metal_promote_pipe_mode(requested_mode, ctx, geometry_active, tess_active);
    uint32_t backend = vrend_metal_backend_mode_from_pipe(promoted);
    if (backend == UINT32_MAX) {
        NSLog(@"[Command]   TODO geometry: primitive %@ (%u) still unsupported",
              @(vrend_metal_primitive_name(requested_mode)), requested_mode);
    }
    return backend;
}

/* Parse and execute virgl command stream */
int vrend_metal_parse_command_stream(
    struct virgl_context *ctx,
    const void *cmd_buf,
    uint32_t cmd_size) {
    
    const uint32_t *cmds = (const uint32_t*)cmd_buf;
    uint32_t offset = 0;
    uint32_t num_cmds = cmd_size / sizeof(uint32_t);
    
    NSLog(@"[Command] Parsing command stream: %u dwords", num_cmds);
    
    while (offset < num_cmds) {
        /* Command header: [length:16][opcode:8][reserved:8] */
        uint32_t header = cmds[offset];
        uint32_t payload_dwords = header & 0xFFFF;
        uint32_t opcode = (header >> 16) & 0xFF;
        uint32_t total_dwords = payload_dwords + 1;
        
        if (payload_dwords == 0 || offset + total_dwords > num_cmds) {
            NSLog(@"[Command] Invalid command length: %u (remaining: %u)",
                  payload_dwords, num_cmds - offset);
            return -1;
        }
        
        const uint32_t *cmd_data = &cmds[offset + 1];
        int ret = 0;
        
        switch (opcode) {
            case VIRGL_CCMD_NOP:
                NSLog(@"[Command] NOP");
                break;
                
            case VIRGL_CCMD_CREATE_OBJECT:
                ret = vrend_metal_cmd_create_object(ctx, cmd_data, payload_dwords);
                break;
                
            case VIRGL_CCMD_BIND_OBJECT:
                ret = vrend_metal_cmd_bind_object(ctx, cmd_data, payload_dwords);
                break;
                
            case VIRGL_CCMD_DESTROY_OBJECT:
                ret = vrend_metal_cmd_destroy_object(ctx, cmd_data, payload_dwords);
                break;
                
            case VIRGL_CCMD_SET_VIEWPORT_STATE:
                ret = vrend_metal_cmd_set_viewport(ctx, cmd_data, payload_dwords);
                break;
                
            case VIRGL_CCMD_SET_FRAMEBUFFER_STATE:
                ret = vrend_metal_cmd_set_framebuffer(ctx, cmd_data, payload_dwords);
                break;
                
            case VIRGL_CCMD_CLEAR:
                ret = vrend_metal_cmd_clear_buffers(ctx, cmd_data, payload_dwords);
                break;
                
            case VIRGL_CCMD_DRAW_VBO:
                ret = vrend_metal_cmd_draw_vbo(ctx, cmd_data, payload_dwords);
                break;
                
            case VIRGL_CCMD_RESOURCE_INLINE_WRITE:
                ret = vrend_metal_cmd_resource_write(ctx, cmd_data, payload_dwords);
                break;
                
            case VIRGL_CCMD_SET_VERTEX_BUFFERS:
                ret = vrend_metal_cmd_set_vertex_buffers(ctx, cmd_data, payload_dwords);
                break;

            case VIRGL_CCMD_SET_INDEX_BUFFER:
                ret = vrend_metal_cmd_set_index_buffer(ctx, cmd_data, payload_dwords);
                break;
                
            case VIRGL_CCMD_SET_SAMPLER_VIEWS:
                ret = vrend_metal_cmd_set_sampler_views(ctx, cmd_data, payload_dwords);
                break;

            case VIRGL_CCMD_BIND_SHADER:
                ret = vrend_metal_cmd_bind_shader(ctx, cmd_data, payload_dwords);
                break;

            case VIRGL_CCMD_SET_TESS_STATE:
                ret = vrend_metal_cmd_set_tess_state(ctx, cmd_data, payload_dwords);
                break;

            case VIRGL_CCMD_LAUNCH_GRID:
                ret = vrend_metal_cmd_launch_grid(ctx, cmd_data, payload_dwords);
                break;
                
            case VIRGL_CCMD_SET_CONSTANT_BUFFER:
            case VIRGL_CCMD_SET_UNIFORM_BUFFER:
                ret = vrend_metal_cmd_set_constant_buffer(ctx, cmd_data, payload_dwords);
                break;

            case VIRGL_CCMD_SET_CLIP_STATE:
                ret = vrend_metal_cmd_set_clip_state(ctx, cmd_data, payload_dwords);
                break;

            case VIRGL_CCMD_BIND_SAMPLER_STATES:
                ret = vrend_metal_cmd_bind_sampler_states(ctx, cmd_data, payload_dwords);
                break;

            case VIRGL_CCMD_SET_STREAMOUT_TARGETS:
                ret = vrend_metal_cmd_set_streamout_targets(ctx, cmd_data, payload_dwords);
                break;

            case VIRGL_CCMD_SET_SHADER_BUFFERS:
                ret = vrend_metal_cmd_set_shader_buffers(ctx, cmd_data, payload_dwords);
                break;
                
            default:
                NSLog(@"[Command] Unhandled opcode: %u", opcode);
                ret = 0; /* Continue processing */
                break;
        }
        
        if (ret != 0) {
            NSLog(@"[Command] Command execution failed: opcode=%u ret=%d", opcode, ret);
            return ret;
        }
        
        offset += total_dwords;
    }
    
    NSLog(@"[Command] Processed %u commands successfully", offset / 4);
    return 0;
}

/* Command handler implementations */

int vrend_metal_cmd_create_object(struct virgl_context *vctx, const uint32_t *cmd, uint32_t cmd_dwords) {
    struct vrend_metal_context *ctx = (struct vrend_metal_context*)vctx;
    uint32_t obj_type = cmd[0];
    uint32_t obj_id = cmd[1];
    
    NSLog(@"[Command] CREATE_OBJECT: type=%u id=%u", obj_type, obj_id);
    
    @autoreleasepool {
        switch (obj_type) {
            case 1: { // BLEND state
                NSMutableDictionary *blend = [NSMutableDictionary dictionary];
                blend[@"enabled"] = @(cmd[2]);
                blend[@"src_rgb"] = @(cmd[3]);
                blend[@"dst_rgb"] = @(cmd[4]);
                blend[@"src_alpha"] = @(cmd[5]);
                blend[@"dst_alpha"] = @(cmd[6]);
                [ctx->blend_states setObject:blend forKey:@(obj_id)];
                NSLog(@"[Command]   Created BLEND state %u", obj_id);
                break;
            }
            case 2: // RASTERIZER state
                NSLog(@"[Command]   Created RASTERIZER state %u (stub)", obj_id);
                break;
            case 3: { // DSA (depth/stencil/alpha) state
                NSMutableDictionary *dsa = [NSMutableDictionary dictionary];
                dsa[@"depth_enabled"] = @(cmd[2]);
                dsa[@"depth_writemask"] = @(cmd[3]);
                dsa[@"depth_func"] = @(cmd[4]);
                dsa[@"stencil_enabled"] = @(cmd[5]);
                [ctx->depth_states setObject:dsa forKey:@(obj_id)];
                NSLog(@"[Command]   Created DSA state %u", obj_id);
                break;
            }
            case 4: // SHADER - handled separately
                NSLog(@"[Command]   SHADER object %u (use shader-specific API)", obj_id);
                break;
            case 5: { // VERTEX_ELEMENTS
                if (cmd_dwords < 2) {
                    NSLog(@"[Command]   VERTEX_ELEMENTS %u has no payload", obj_id);
                    break;
                }
                uint32_t element_words = cmd_dwords - 2;
                uint32_t element_count = element_words / 4;
                if (element_count == 0) {
                    NSLog(@"[Command]   VERTEX_ELEMENTS %u missing element entries", obj_id);
                    break;
                }
                if (element_count > VREND_MAX_VERTEX_ATTRIBUTES) {
                    NSLog(@"[Command]   VERTEX_ELEMENTS %u truncating %u elements to %u",
                          obj_id, element_count, VREND_MAX_VERTEX_ATTRIBUTES);
                    element_count = VREND_MAX_VERTEX_ATTRIBUTES;
                }

                struct vrend_metal_vertex_elements_state state = {0};
                state.count = element_count;
                for (uint32_t i = 0; i < element_count; i++) {
                    uint32_t base = 2 + i * 4;
                    state.elements[i].offset = cmd[base + 0];
                    state.elements[i].instance_divisor = cmd[base + 1];
                    state.elements[i].buffer_index = cmd[base + 2];
                    state.elements[i].format = cmd[base + 3];

                    if (!vrend_metal_pipe_format_supported(state.elements[i].format)) {
                        state.unsupported_mask |= (1u << i);
                    }
                }

                NSData *payload = [NSData dataWithBytes:&state length:sizeof(state)];
                [ctx->vertex_elements setObject:payload forKey:@(obj_id)];
                NSLog(@"[Command]   Created VERTEX_ELEMENTS %u with %u attributes",
                      obj_id, state.count);
                if (state.unsupported_mask) {
                    NSLog(@"[Command]   VERTEX_ELEMENTS %u has unsupported format mask 0x%04X",
                          obj_id, state.unsupported_mask);
                }
                break;
            }
            case 6: { // SAMPLER_VIEW
                size_t expected = sizeof(struct vrend_metal_sampler_view_desc) / sizeof(uint32_t);
                if (cmd_dwords < 2 + expected) {
                    NSLog(@"[Command]   SAMPLER_VIEW %u payload too small (got %u dwords)", obj_id, cmd_dwords);
                    break;
                }
                struct vrend_metal_sampler_view_desc desc;
                memcpy(&desc, &cmd[2], sizeof(desc));
                NSData *payload = [NSData dataWithBytes:&desc length:sizeof(desc)];
                if (!ctx->sampler_views) {
                    ctx->sampler_views = [[NSMutableDictionary alloc] init];
                }
                ctx->sampler_views[@(obj_id)] = payload;
                NSLog(@"[Command]   Created SAMPLER_VIEW %u (res=%u fmt=%u)", obj_id, desc.resource_id, desc.format);
                break;
            }
            case 7: { // SAMPLER_STATE
                size_t expected = sizeof(struct vrend_metal_sampler_state_desc) / sizeof(uint32_t);
                if (cmd_dwords < 2 + expected) {
                    NSLog(@"[Command]   SAMPLER_STATE %u payload too small (got %u dwords)", obj_id, cmd_dwords);
                    break;
                }
                struct vrend_metal_sampler_state_desc desc;
                memcpy(&desc, &cmd[2], sizeof(desc));
                NSData *payload = [NSData dataWithBytes:&desc length:sizeof(desc)];
                if (!ctx->sampler_states) {
                    ctx->sampler_states = [[NSMutableDictionary alloc] init];
                }
                ctx->sampler_states[@(obj_id)] = payload;
                [ctx->sampler_state_objects removeObjectForKey:@(obj_id)];
                NSLog(@"[Command]   Created SAMPLER_STATE %u", obj_id);
                break;
            }
            case VIRGL_OBJECT_STREAMOUT_TARGET: {
                const size_t expected = 3;
                if (cmd_dwords < 2 + expected) {
                    NSLog(@"[Command]   STREAMOUT_TARGET %u payload too small (got %u dwords)", obj_id, cmd_dwords);
                    return -1;
                }
                struct vrend_metal_streamout_target_desc desc = {0};
                desc.resource_handle = cmd[2];
                desc.buffer_offset = cmd[3];
                desc.buffer_size = cmd[4];
                NSData *payload = [NSData dataWithBytes:&desc length:sizeof(desc)];
                if (!ctx->streamout_targets) {
                    ctx->streamout_targets = [[NSMutableDictionary alloc] init];
                }
                ctx->streamout_targets[@(obj_id)] = payload;
                NSLog(@"[Command]   Created STREAMOUT_TARGET %u (res=%u offset=%u size=%u)",
                      obj_id, desc.resource_handle, desc.buffer_offset, desc.buffer_size);
                break;
            }
            default:
                NSLog(@"[Command]   Unknown object type: %u", obj_type);
                return -1;
        }
    }
    
    return 0;
}

int vrend_metal_cmd_bind_object(struct virgl_context *vctx, const uint32_t *cmd, uint32_t cmd_dwords) {
    (void)cmd_dwords;
    struct vrend_metal_context *ctx = (struct vrend_metal_context*)vctx;
    uint32_t obj_type = cmd[0];
    uint32_t obj_id = cmd[1];
    
    NSLog(@"[Command] BIND_OBJECT: type=%u id=%u", obj_type, obj_id);
    
    @autoreleasepool {
        switch (obj_type) {
            case 1: { // BLEND state
                NSDictionary *blend = [ctx->blend_states objectForKey:@(obj_id)];
                if (blend) {
                    ctx->bound_blend_state = obj_id;
                    vrend_metal_request_pipeline_update(vctx);
                    NSLog(@"[Command]   Bound BLEND state %u", obj_id);
                } else {
                    NSLog(@"[Command]   BLEND state %u not found", obj_id);
                }
                break;
            }
            case 2: // RASTERIZER
                NSLog(@"[Command]   Bound RASTERIZER %u (stub)", obj_id);
                break;
            case 3: { // DSA state
                NSDictionary *dsa = [ctx->depth_states objectForKey:@(obj_id)];
                if (dsa) {
                    ctx->bound_depth_state = obj_id;
                    vrend_metal_request_pipeline_update(vctx);
                    NSLog(@"[Command]   Bound DSA state %u", obj_id);
                } else {
                    NSLog(@"[Command]   DSA state %u not found", obj_id);
                }
                break;
            }
            case 5: { // VERTEX_ELEMENTS
                NSData *state = [ctx->vertex_elements objectForKey:@(obj_id)];
                if (state) {
                    ctx->bound_vertex_elements = obj_id;
                    vrend_metal_request_pipeline_update(vctx);
                    NSLog(@"[Command]   Bound VERTEX_ELEMENTS %u", obj_id);
                } else {
                    NSLog(@"[Command]   VERTEX_ELEMENTS %u not found", obj_id);
                }
                break;
            }
            case 4: // SHADER - handled by BIND_SHADER command
                 NSLog(@"[Command] SHADER bind via BIND_SHADER command");
                 break;
            default:
                NSLog(@"[Command]   Unknown object type: %u", obj_type);
                break;
        }
    }
    
    return 0;
}

int vrend_metal_cmd_destroy_object(struct virgl_context *vctx, const uint32_t *cmd, uint32_t cmd_dwords) {
    (void)cmd_dwords;
    struct vrend_metal_context *ctx = (struct vrend_metal_context*)vctx;
    uint32_t obj_id = cmd[0];
    
    NSLog(@"[Command] DESTROY_OBJECT: id=%u", obj_id);
    
    @autoreleasepool {
        // Try removing from all possible dictionaries
        [ctx->blend_states removeObjectForKey:@(obj_id)];
        [ctx->depth_states removeObjectForKey:@(obj_id)];
        [ctx->metal_shaders removeObjectForKey:@(obj_id)];
        [ctx->metal_pipelines removeObjectForKey:@(obj_id)];
        [ctx->metal_compute_pipelines removeObjectForKey:@(obj_id)];
        [ctx->vertex_elements removeObjectForKey:@(obj_id)];
        [ctx->sampler_states removeObjectForKey:@(obj_id)];
        [ctx->sampler_state_objects removeObjectForKey:@(obj_id)];
        [ctx->sampler_views removeObjectForKey:@(obj_id)];
        [ctx->streamout_targets removeObjectForKey:@(obj_id)];
        if (ctx->bound_vertex_elements == obj_id) {
            ctx->bound_vertex_elements = 0;
            vrend_metal_request_pipeline_update(vctx);
        }

        for (int stage = 0; stage < 2; stage++) {
            for (int slot = 0; slot < VREND_MAX_SAMPLERS; slot++) {
                if (ctx->sampler_state_handles[stage][slot] == obj_id) {
                    vrend_metal_bind_sampler_state_slot(ctx, stage, slot, 0);
                }
                if (ctx->sampler_view_handles[stage][slot] == obj_id) {
                    vrend_metal_bind_sampler_view_slot(ctx, stage, slot, 0);
                }
            }
        }

        for (int slot = 0; slot < VREND_MAX_STREAMOUT_TARGETS; slot++) {
            if (ctx->streamout_bindings[slot].handle == obj_id) {
                vrend_metal_clear_streamout_binding(&ctx->streamout_bindings[slot]);
            }
        }
        
        NSLog(@"[Command]   Destroyed object %u", obj_id);
    }
    
    return 0;
}

int vrend_metal_cmd_set_viewport(struct virgl_context *vctx, const uint32_t *cmd, uint32_t cmd_dwords) {
    (void)cmd_dwords;
    struct vrend_metal_context *ctx = (struct vrend_metal_context*)vctx;
    /* Viewport data: x, y, width, height, near, far */
    float *viewport = (float*)cmd;
    
    NSLog(@"[Command] SET_VIEWPORT: (%.1f,%.1f %.1fx%.1f) near=%.2f far=%.2f",
          viewport[0], viewport[1], viewport[2], viewport[3],
          viewport[4], viewport[5]);
    
    @autoreleasepool {
        ctx->viewport = (MTLViewport){
            .originX = viewport[0],
            .originY = viewport[1],
            .width = viewport[2],
            .height = viewport[3],
            .znear = viewport[4],
            .zfar = viewport[5]
        };
        
        // Apply to current render encoder if active
        if (ctx->render_encoder) {
            [ctx->render_encoder setViewport:ctx->viewport];
        }
    }
    
    return 0;
}

int vrend_metal_cmd_set_framebuffer(struct virgl_context *vctx, const uint32_t *cmd, uint32_t cmd_dwords) {
    (void)cmd_dwords;
    struct vrend_metal_context *ctx = (struct vrend_metal_context*)vctx;
    uint32_t nr_cbufs = cmd[0];
    uint32_t zsurf_handle = cmd[1];
    
    NSLog(@"[Command] SET_FRAMEBUFFER: color_buffers=%u depth_stencil=%u",
          nr_cbufs, zsurf_handle);
    
    @autoreleasepool {
        // Clear previous framebuffer state
        ctx->framebuffer_color_count = 0;
        ctx->framebuffer_depth = nil;
        ctx->framebuffer_stencil = nil;
        
        // Set color attachments
        for (uint32_t i = 0; i < nr_cbufs && i < 8; i++) {
            uint32_t surf_handle = cmd[2 + i];
            if (surf_handle != 0) {
                ctx->framebuffer_color[i] = [ctx->metal_textures objectForKey:@(surf_handle)];
                ctx->framebuffer_color_count = i + 1;
                NSLog(@"[Command]   Color[%u] = surface %u", i, surf_handle);
            }
        }
        
        // Set depth/stencil attachment
        if (zsurf_handle != 0) {
            id<MTLTexture> depth_texture = [ctx->metal_textures objectForKey:@(zsurf_handle)];
            ctx->framebuffer_depth = depth_texture;
            
            // Check if texture has stencil component
            if (depth_texture) {
                MTLPixelFormat format = [depth_texture pixelFormat];
                if (format == MTLPixelFormatDepth32Float_Stencil8 ||
                    format == MTLPixelFormatDepth24Unorm_Stencil8) {
                    ctx->framebuffer_stencil = depth_texture;
                    NSLog(@"[Command]   Depth/Stencil = surface %u (combined)", zsurf_handle);
                } else {
                    NSLog(@"[Command]   Depth = surface %u", zsurf_handle);
                }
            }
        }
    }
    
    return 0;
}

int vrend_metal_cmd_clear_buffers(struct virgl_context *ctx, const uint32_t *cmd, uint32_t cmd_dwords) {
    (void)cmd_dwords;
    uint32_t buffers = cmd[0];
    float *color = (float*)&cmd[1];
    double depth = *(double*)&cmd[5];
    uint32_t stencil = cmd[7];
    
    NSLog(@"[Command] CLEAR: buffers=0x%x color=(%.2f,%.2f,%.2f,%.2f) depth=%.2f stencil=%u",
          buffers, color[0], color[1], color[2], color[3], depth, stencil);
    
    return vrend_metal_clear(ctx, buffers, color, depth, stencil);
}

int vrend_metal_cmd_draw_vbo(struct virgl_context *vctx, const uint32_t *cmd, uint32_t cmd_dwords) {
    const uint32_t expected_dwords = VIRGL_DRAW_VBO_SIZE - 1; /* header already stripped */
    if (cmd_dwords < expected_dwords) {
        NSLog(@"[Command] DRAW_VBO payload too small (have %u, need %u)",
              cmd_dwords, expected_dwords);
        return -1;
    }

    struct vrend_metal_context *ctx = (struct vrend_metal_context*)vctx;
    struct vrend_metal_draw_info info = {
        .start = cmd[0],
        .count = cmd[1],
        .mode = cmd[2],
        .indexed = cmd[3] != 0,
        .instance_count = cmd[4],
        .index_bias = (int32_t)cmd[5],
        .start_instance = cmd[6],
        .primitive_restart = cmd[7] != 0,
        .restart_index = cmd[8],
        .min_index = cmd[9],
        .max_index = cmd[10],
    };

    NSLog(@"[Command] DRAW_VBO: start=%u count=%u mode=%u(%@) indexed=%u inst=%u base_instance=%u",
          info.start,
          info.count,
          info.mode,
          @(vrend_metal_primitive_name(info.mode)),
          info.indexed,
          info.instance_count,
          info.start_instance);

    if (info.count == 0) {
        NSLog(@"[Command]   Skipping draw with zero count");
        return 0;
    }

    bool geometry_active = ctx->bound_geometry_shader != 0;
    bool tess_active = (ctx->bound_tess_ctrl_shader != 0 || ctx->bound_tess_eval_shader != 0);
    if (tess_active && (!ctx->tess_state.valid || ctx->tess_state.patch_vertices == 0)) {
        NSLog(@"[Command]   Tessellation shaders bound but tess state invalid; falling back to non-tess draw");
        tess_active = false;
    }
    if (geometry_active) {
        NSLog(@"[Command]   Geometry stage requested (shader=%u) -> emulated via vertex amplification",
              ctx->bound_geometry_shader);
    }
    if (tess_active) {
        NSLog(@"[Command]   Tessellation stage requested (patch_vertices=%u) -> emulated via multi-pass",
              ctx->tess_state.patch_vertices);
    }

    uint32_t backend_mode = vrend_metal_resolve_draw_mode(info.mode, ctx, geometry_active, tess_active);
    if (backend_mode == UINT32_MAX) {
        NSLog(@"[Command]   TODO geometry: primitive %@ (%u) unsupported in Metal fallback",
              @(vrend_metal_primitive_name(info.mode)), info.mode);
        return -1;
    }
    info.mode = backend_mode;

    // Ensure pipeline exists before drawing
    if (ctx->current_pipeline == 0) {
        uint32_t new_pipeline_id = (ctx->bound_vertex_shader << 16) ^ ctx->bound_fragment_shader;
        if (![ctx->metal_pipelines objectForKey:@(new_pipeline_id)]) {
            if (vrend_metal_create_pipeline(vctx, new_pipeline_id) != 0) {
                return -1;
            }
        }
        vrend_metal_bind_pipeline(vctx, new_pipeline_id);
    }

    return vrend_metal_draw_vbo(vctx, &info);
}

int vrend_metal_cmd_resource_write(struct virgl_context *ctx, const uint32_t *cmd, uint32_t cmd_dwords) {
    (void)cmd_dwords;
    uint32_t res_handle = cmd[0];
    uint32_t level = cmd[1];
    uint32_t usage = cmd[2];
    uint32_t stride = cmd[3];
    uint32_t layer_stride = cmd[4];
    /* Box follows: x, y, z, w, h, d */
    uint32_t x = cmd[5], y = cmd[6], z = cmd[7];
    uint32_t w = cmd[8], h = cmd[9], d = cmd[10];
    const void *data = &cmd[11];
    
    NSLog(@"[Command] RESOURCE_WRITE: res=%u region=(%u,%u,%u %ux%ux%u) level=%u",
          res_handle, x, y, z, w, h, d, level);
    
    struct virgl_box box = { x, y, z, w, h, d };
    struct virgl_resource res = { .res_id = res_handle };
    
    return vrend_metal_transfer_inline_write(ctx, &res, &box, data, stride, layer_stride);
}

int vrend_metal_cmd_set_vertex_buffers(struct virgl_context *vctx, const uint32_t *cmd, uint32_t cmd_dwords) {
    struct vrend_metal_context *ctx = (struct vrend_metal_context*)vctx;
    if (cmd_dwords == 0) {
        NSLog(@"[Command] SET_VERTEX_BUFFERS missing payload");
        return -1;
    }

    uint32_t num_buffers = cmd[0];
    uint32_t expected_words = 1 + num_buffers * 3;
    if (cmd_dwords < expected_words) {
        NSLog(@"[Command] SET_VERTEX_BUFFERS truncated (have %u dwords, need %u)",
              cmd_dwords, expected_words);
        if (cmd_dwords <= 1) {
            return -1;
        }
        num_buffers = (cmd_dwords - 1) / 3;
    }
    
    NSLog(@"[Command] SET_VERTEX_BUFFERS: count=%u", num_buffers);
    
    @autoreleasepool {
        ctx->vertex_buffer_count = 0;
        
        for (uint32_t i = 0; i < num_buffers && i < 16; i++) {
            uint32_t base = 1 + i * 3;
            uint32_t stride = cmd[base + 0];
            uint32_t offset = cmd[base + 1];
            uint32_t res_handle = cmd[base + 2];
            
            NSLog(@"[Command]   Buffer[%u]: stride=%u offset=%u resource=%u",
                  i, stride, offset, res_handle);
            
            ctx->vertex_buffers[i] = nil;
            ctx->vertex_buffer_offsets[i] = 0;
            ctx->vertex_buffer_strides[i] = 0;
            
            if (res_handle != 0) {
                ctx->vertex_buffers[i] = [ctx->metal_buffers objectForKey:@(res_handle)];
                ctx->vertex_buffer_offsets[i] = offset;
                ctx->vertex_buffer_strides[i] = stride;
                ctx->vertex_buffer_count = i + 1;
            }
        }

        for (uint32_t i = num_buffers; i < 16; i++) {
            ctx->vertex_buffers[i] = nil;
            ctx->vertex_buffer_offsets[i] = 0;
            ctx->vertex_buffer_strides[i] = 0;
        }

        vrend_metal_request_pipeline_update(vctx);
    }
    
    return 0;
}

int vrend_metal_cmd_set_index_buffer(struct virgl_context *vctx, const uint32_t *cmd, uint32_t cmd_dwords) {
    struct vrend_metal_context *ctx = (struct vrend_metal_context*)vctx;
    if (cmd_dwords < 3) {
        NSLog(@"[Command] SET_INDEX_BUFFER payload too small (have %u dwords)", cmd_dwords);
        return -1;
    }

    uint32_t handle = cmd[0];
    uint32_t index_size = cmd[1];
    uint32_t offset = cmd[2];

    NSLog(@"[Command] SET_INDEX_BUFFER: handle=%u size=%u offset=%u",
          handle, index_size, offset);

    if (handle == 0) {
        ctx->index_buffer = nil;
        ctx->index_buffer_handle = 0;
        ctx->index_buffer_offset = 0;
        ctx->index_buffer_stride = 0;
        return 0;
    }

    id<MTLBuffer> buffer = [ctx->metal_buffers objectForKey:@(handle)];
    if (!buffer) {
        NSLog(@"[Command]   Index buffer resource %u not found", handle);
        return -1;
    }

    MTLIndexType type;
    switch (index_size) {
        case 2:
            type = MTLIndexTypeUInt16;
            break;
        case 4:
            type = MTLIndexTypeUInt32;
            break;
        default:
            NSLog(@"[Command]   Unsupported index size %u (expected 2 or 4 bytes)", index_size);
            return -1;
    }

    NSUInteger length = [buffer length];
    if (offset >= length) {
        NSLog(@"[Command]   Index buffer offset %u exceeds buffer length %lu",
              offset, (unsigned long)length);
        return -1;
    }

    ctx->index_buffer = buffer;
    ctx->index_buffer_handle = handle;
    ctx->index_buffer_offset = offset;
    ctx->index_buffer_stride = index_size;
    ctx->index_buffer_type = type;

    return 0;
}

int vrend_metal_cmd_bind_shader(struct virgl_context *vctx, const uint32_t *cmd, uint32_t cmd_dwords) {
    (void)cmd_dwords;
    struct vrend_metal_context *ctx = (struct vrend_metal_context*)vctx;
    uint32_t shader_type = cmd[0];
    uint32_t shader_handle = cmd[1];
    
    const char *type_name[] = { "vertex", "fragment", "geometry", "tess_ctrl", "tess_eval", "compute" };
    
        NSLog(@"[Command] BIND_SHADER: type=%s handle=%u",
            shader_type < 6 ? type_name[shader_type] : "unknown",
            shader_handle);
    
    @autoreleasepool {
        switch (shader_type) {
            case PIPE_SHADER_VERTEX:
                ctx->bound_vertex_shader = shader_handle;
                vrend_metal_request_pipeline_update(vctx);
                break;
            case PIPE_SHADER_FRAGMENT:
                ctx->bound_fragment_shader = shader_handle;
                vrend_metal_request_pipeline_update(vctx);
                break;
            case PIPE_SHADER_GEOMETRY:
                ctx->bound_geometry_shader = shader_handle;
                NSLog(@"[Command]   Geometry shader %u bound (TODO: Metal backend emulates via vertex amplification)",
                      shader_handle);
                break;
            case PIPE_SHADER_TESS_CTRL:
                ctx->bound_tess_ctrl_shader = shader_handle;
                NSLog(@"[Command]   Tess control shader %u bound (stub)", shader_handle);
                break;
            case PIPE_SHADER_TESS_EVAL:
                ctx->bound_tess_eval_shader = shader_handle;
                NSLog(@"[Command]   Tess eval shader %u bound (stub)", shader_handle);
                break;
            case PIPE_SHADER_COMPUTE:
                ctx->bound_compute_shader = shader_handle;
                ctx->current_compute_pipeline = 0;
                NSLog(@"[Command]   Compute shader %u bound", shader_handle);
                break;
            default:
                NSLog(@"[Command] Unsupported shader type: %u", shader_type);
                break;
        }
    }
    
    return 0;
}

int vrend_metal_cmd_set_tess_state(struct virgl_context *vctx, const uint32_t *cmd, uint32_t cmd_dwords) {
    struct vrend_metal_context *ctx = (struct vrend_metal_context*)vctx;
    const uint32_t expected = 1 + 4 + 2; /* patch_vertices + outer[4] + inner[2] */
    if (cmd_dwords < expected) {
        NSLog(@"[Command] SET_TESS_STATE payload too small (have %u, need %u)", cmd_dwords, expected);
        return -1;
    }

    float outer[4] = {0};
    float inner[2] = {0};
    memcpy(outer, &cmd[1], sizeof(outer));
    memcpy(inner, &cmd[5], sizeof(inner));

    ctx->tess_state.patch_vertices = cmd[0];
    memcpy(ctx->tess_state.default_outer_level, outer, sizeof(outer));
    memcpy(ctx->tess_state.default_inner_level, inner, sizeof(inner));
    ctx->tess_state.valid = ctx->tess_state.patch_vertices > 0;
    ctx->tess_state.dirty = true;

    NSLog(@"[Command] SET_TESS_STATE: patch=%u outer=(%.2f,%.2f,%.2f,%.2f) inner=(%.2f,%.2f)",
          ctx->tess_state.patch_vertices,
          outer[0], outer[1], outer[2], outer[3], inner[0], inner[1]);

    if (!ctx->tess_state.valid) {
        NSLog(@"[Command]   Tessellation marked invalid (patch vertices=0)");
    }

    return 0;
}

int vrend_metal_cmd_set_constant_buffer(struct virgl_context *vctx, const uint32_t *cmd, uint32_t cmd_dwords) {
    (void)cmd_dwords;
    struct vrend_metal_context *mctx = (struct vrend_metal_context*)vctx;
    uint32_t shader_type = cmd[0];
    uint32_t index = cmd[1];
    uint32_t res_handle = cmd[2];
    uint32_t offset = cmd[3];
    uint32_t length = cmd[4];
    NSLog(@"[Command] SET_CONSTANT_BUFFER: shader=%u index=%u resource=%u offset=%u length=%u",
          shader_type, index, res_handle, offset, length);
    
    if (index >= VREND_MAX_CONST_BUFFERS) {
        NSLog(@"[Command]   Constant buffer index %u out of range", index);
        return -1;
    }
    
    id<MTLBuffer> buffer = [mctx->metal_buffers objectForKey:@(res_handle)];
    if (!buffer) {
        NSLog(@"[Command]   Constant buffer resource %u not found", res_handle);
        return -1;
    }
    
    int32_t stage = vrend_metal_stage_from_pipe_shader(shader_type);
    if (stage < 0 || stage >= VREND_METAL_STAGE_COUNT) {
        NSLog(@"[Command]   Constant buffer stage %u unsupported", shader_type);
        return -1;
    }

    mctx->constant_buffers[stage][index].buffer = buffer;
    mctx->constant_buffers[stage][index].offset = offset;
    mctx->constant_buffers[stage][index].length = length;
    
    // Apply immediately if encoder active
    vrend_metal_bind_constant_buffers_internal(mctx);
    
    return 0;
}

int vrend_metal_cmd_set_clip_state(struct virgl_context *vctx, const uint32_t *cmd, uint32_t cmd_dwords) {
    struct vrend_metal_context *ctx = (struct vrend_metal_context*)vctx;
    if (!ctx) {
        return -1;
    }
    if (cmd_dwords == 0) {
        NSLog(@"[Command] SET_CLIP_STATE missing payload");
        return -1;
    }
    if (cmd_dwords % 4 != 0) {
        NSLog(@"[Command] SET_CLIP_STATE payload not aligned to vec4 (dwords=%u)", cmd_dwords);
    }

    uint32_t plane_count = cmd_dwords / 4;
    if (plane_count > VREND_MAX_CLIP_PLANES) {
        NSLog(@"[Command] SET_CLIP_STATE truncating %u planes to %u",
              plane_count, VREND_MAX_CLIP_PLANES);
        plane_count = VREND_MAX_CLIP_PLANES;
    }

    uint32_t enabled_mask = 0;
    for (uint32_t plane = 0; plane < plane_count; plane++) {
        bool non_zero = false;
        for (uint32_t component = 0; component < 4 && (plane * 4 + component) < cmd_dwords; component++) {
            float value = 0.0f;
            memcpy(&value, &cmd[plane * 4 + component], sizeof(float));
            ctx->clip_state.planes[plane][component] = value;
            non_zero |= value != 0.0f;
        }
        if (non_zero) {
            enabled_mask |= (1u << plane);
        }
    }
    for (uint32_t plane = plane_count; plane < VREND_MAX_CLIP_PLANES; plane++) {
        memset(ctx->clip_state.planes[plane], 0, sizeof(ctx->clip_state.planes[plane]));
    }

    ctx->clip_state.enabled_mask = enabled_mask;
    ctx->clip_state.dirty = true;
    if (plane_count > 0) {
        NSLog(@"[Command] SET_CLIP_STATE: planes=%u mask=0x%02X first=(%.3f,%.3f,%.3f,%.3f)",
              plane_count, enabled_mask,
              ctx->clip_state.planes[0][0], ctx->clip_state.planes[0][1],
              ctx->clip_state.planes[0][2], ctx->clip_state.planes[0][3]);
    } else {
        NSLog(@"[Command] SET_CLIP_STATE cleared (mask=0)");
    }

    if (ctx->render_encoder) {
        vrend_metal_apply_clip_state(ctx);
    }

    return 0;
}

int vrend_metal_cmd_set_sampler_views(struct virgl_context *vctx, const uint32_t *cmd, uint32_t cmd_dwords) {
    if (cmd_dwords < 3) {
        NSLog(@"[Command] SET_SAMPLER_VIEWS missing header (dwords=%u)", cmd_dwords);
        return -1;
    }
    struct vrend_metal_context *ctx = (struct vrend_metal_context*)vctx;
    int32_t stage = vrend_metal_stage_from_pipe_shader(cmd[0]);
    uint32_t start_slot = cmd[1];
    uint32_t count = cmd[2];
    if (stage < 0) {
        NSLog(@"[Command] SET_SAMPLER_VIEWS stage %u unsupported", cmd[0]);
        return -1;
    }
    if (cmd_dwords < 3 + count) {
        NSLog(@"[Command] SET_SAMPLER_VIEWS payload too small (need %u, got %u)", 3 + count, cmd_dwords);
        return -1;
    }
    NSLog(@"[Command] SET_SAMPLER_VIEWS stage=%u start=%u count=%u", stage, start_slot, count);
    for (uint32_t i = 0; i < count; i++) {
        uint32_t handle = cmd[3 + i];
        vrend_metal_bind_sampler_view_slot(ctx, stage, start_slot + i, handle);
    }
    return 0;
}

int vrend_metal_cmd_bind_sampler_states(struct virgl_context *vctx, const uint32_t *cmd, uint32_t cmd_dwords) {
    if (cmd_dwords < 3) {
        NSLog(@"[Command] BIND_SAMPLER_STATES missing header (dwords=%u)", cmd_dwords);
        return -1;
    }
    struct vrend_metal_context *ctx = (struct vrend_metal_context*)vctx;
    int32_t stage = vrend_metal_stage_from_pipe_shader(cmd[0]);
    uint32_t start_slot = cmd[1];
    uint32_t count = cmd[2];
    if (stage < 0) {
        NSLog(@"[Command] BIND_SAMPLER_STATES stage %u unsupported", cmd[0]);
        return -1;
    }
    if (cmd_dwords < 3 + count) {
        NSLog(@"[Command] BIND_SAMPLER_STATES payload too small (need %u, got %u)", 3 + count, cmd_dwords);
        return -1;
    }
    NSLog(@"[Command] BIND_SAMPLER_STATES stage=%u start=%u count=%u", stage, start_slot, count);
    for (uint32_t i = 0; i < count; i++) {
        uint32_t handle = cmd[3 + i];
        vrend_metal_bind_sampler_state_slot(ctx, stage, start_slot + i, handle);
    }
    return 0;
}

int vrend_metal_cmd_set_streamout_targets(struct virgl_context *vctx, const uint32_t *cmd, uint32_t cmd_dwords) {
    struct vrend_metal_context *ctx = (struct vrend_metal_context*)vctx;
    if (!ctx) {
        return -1;
    }
    if (cmd_dwords < 1) {
        NSLog(@"[Command] SET_STREAMOUT_TARGETS missing append mask");
        return -1;
    }

    uint32_t append_mask = cmd[0];
    uint32_t handle_count = cmd_dwords - 1;
    if (handle_count > VREND_MAX_STREAMOUT_TARGETS) {
        NSLog(@"[Command] SET_STREAMOUT_TARGETS truncating %u handles to %u",
              handle_count, VREND_MAX_STREAMOUT_TARGETS);
        handle_count = VREND_MAX_STREAMOUT_TARGETS;
    }
    ctx->streamout_append_mask = append_mask;

    NSLog(@"[Command] SET_STREAMOUT_TARGETS mask=0x%X count=%u", append_mask, handle_count);
    for (uint32_t slot = 0; slot < VREND_MAX_STREAMOUT_TARGETS; slot++) {
        struct vrend_metal_streamout_binding *binding = &ctx->streamout_bindings[slot];
        if (slot >= handle_count) {
            vrend_metal_clear_streamout_binding(binding);
            continue;
        }

        uint32_t handle = cmd[1 + slot];
        if (handle == 0) {
            vrend_metal_clear_streamout_binding(binding);
            continue;
        }

        NSData *payload = ctx->streamout_targets[@(handle)];
        if (!payload || payload.length < sizeof(struct vrend_metal_streamout_target_desc)) {
            NSLog(@"[Command]   STREAMOUT_TARGET %u not found or malformed", handle);
            vrend_metal_clear_streamout_binding(binding);
            continue;
        }

        struct vrend_metal_streamout_target_desc desc = {0};
        [payload getBytes:&desc length:sizeof(desc)];
        id<MTLBuffer> buffer = [ctx->metal_buffers objectForKey:@(desc.resource_handle)];
        if (!buffer) {
            NSLog(@"[Command]   STREAMOUT_TARGET %u resource %u missing", handle, desc.resource_handle);
            vrend_metal_clear_streamout_binding(binding);
            continue;
        }

        binding->buffer = buffer;
        binding->resource_handle = desc.resource_handle;
        binding->handle = handle;
        binding->offset = desc.buffer_offset;
        binding->size = desc.buffer_size;
        binding->append = ((append_mask >> slot) & 0x1) != 0;

        NSLog(@"[Command]   Stream-out[%u] handle=%u res=%u offset=%u size=%u append=%@",
              slot, handle, desc.resource_handle, desc.buffer_offset, desc.buffer_size,
              binding->append ? @"YES" : @"NO");
    }

    if (ctx->render_encoder) {
        vrend_metal_bind_streamout_buffers(ctx);
    }

    return 0;
}

int vrend_metal_cmd_set_shader_buffers(struct virgl_context *vctx, const uint32_t *cmd, uint32_t cmd_dwords) {
    struct vrend_metal_context *ctx = (struct vrend_metal_context*)vctx;
    if (!ctx) {
        return -1;
    }
    if (cmd_dwords < 2) {
        NSLog(@"[Command] SET_SHADER_BUFFERS missing header (dwords=%u)", cmd_dwords);
        return -1;
    }

    uint32_t shader_type = cmd[0];
    int32_t stage = vrend_metal_stage_from_pipe_shader(shader_type);
    if (stage < 0) {
        NSLog(@"[Command] SET_SHADER_BUFFERS stage %u unsupported", shader_type);
        return 0;
    }

    uint32_t start_slot = cmd[1];
    uint32_t payload = cmd_dwords - 2;
    if (payload % 3 != 0) {
        NSLog(@"[Command] SET_SHADER_BUFFERS malformed payload (dwords=%u)", cmd_dwords);
        return -1;
    }
    uint32_t count = payload / 3;
    NSLog(@"[Command] SET_SHADER_BUFFERS stage=%u start=%u count=%u", stage, start_slot, count);

    for (uint32_t i = 0; i < count; i++) {
        uint32_t slot = start_slot + i;
        if (slot >= VREND_MAX_SHADER_BUFFERS) {
            NSLog(@"[Command]   shader buffer slot %u out of range", slot);
            continue;
        }
        uint32_t offset = cmd[2 + i * 3];
        uint32_t length = cmd[2 + i * 3 + 1];
        uint32_t handle = cmd[2 + i * 3 + 2];
        id<MTLBuffer> buffer = [ctx->metal_buffers objectForKey:@(handle)];
        if (!buffer) {
            NSLog(@"[Command]   shader buffer resource %u not found", handle);
            ctx->shader_buffers[stage][slot].buffer = nil;
            ctx->shader_buffers[stage][slot].offset = 0;
            ctx->shader_buffers[stage][slot].length = 0;
            continue;
        }
        ctx->shader_buffers[stage][slot].buffer = buffer;
        ctx->shader_buffers[stage][slot].offset = offset;
        ctx->shader_buffers[stage][slot].length = length;
        NSLog(@"[Command]   stage=%u slot=%u buffer=%u offset=%u length=%u",
              stage, slot, handle, offset, length);
    }

    vrend_metal_bind_shader_buffers_internal(ctx);

    return 0;
}

int vrend_metal_cmd_launch_grid(struct virgl_context *vctx, const uint32_t *cmd, uint32_t cmd_dwords) {
    struct vrend_metal_context *ctx = (struct vrend_metal_context*)vctx;
    if (!ctx) {
        return -1;
    }

    const uint32_t min_words = 9;
    if (cmd_dwords < min_words) {
        NSLog(@"[Command] LAUNCH_GRID payload too small (have %u, need %u)", cmd_dwords, min_words);
        return -1;
    }

    struct vrend_metal_grid_info info = {0};
    info.grid[0] = cmd[0];
    info.grid[1] = cmd[1];
    info.grid[2] = cmd[2];
    info.block[0] = cmd[3];
    info.block[1] = cmd[4];
    info.block[2] = cmd[5];
    info.indirect = cmd[6] != 0;
    info.indirect_handle = (cmd_dwords > 7) ? cmd[7] : 0;
    info.indirect_offset = (cmd_dwords > 8) ? cmd[8] : 0;

    if (cmd_dwords > min_words) {
        NSLog(@"[Command] LAUNCH_GRID received %u extra dwords (unsupported metadata)", cmd_dwords - min_words);
    }

    if (info.indirect) {
        NSLog(@"[Command] LAUNCH_GRID indirect dispatch not yet supported (handle=%u offset=%u)",
              info.indirect_handle, info.indirect_offset);
        return -1;
    }

    return vrend_metal_launch_grid(vctx, &info);
}

/*
 * virgl_protocol.h - Virgl renderer protocol definitions
 * 
 * Based on virglrenderer protocol from Mesa/virglrenderer project
 * Minimal subset needed for basic 3D acceleration
 */

#ifndef _VIRGL_PROTOCOL_H
#define _VIRGL_PROTOCOL_H

#include <IOKit/IOTypes.h>

// Virgl command opcodes
enum virgl_context_cmd {
    VIRGL_CCMD_NOP = 0,
    VIRGL_CCMD_CREATE_OBJECT = 1,
    VIRGL_CCMD_BIND_OBJECT,
    VIRGL_CCMD_DESTROY_OBJECT,
    VIRGL_CCMD_SET_VIEWPORT_STATE,
    VIRGL_CCMD_SET_FRAMEBUFFER_STATE,
    VIRGL_CCMD_SET_VERTEX_BUFFERS,
    VIRGL_CCMD_CLEAR,
    VIRGL_CCMD_DRAW_VBO,
    VIRGL_CCMD_RESOURCE_INLINE_WRITE,
    VIRGL_CCMD_SET_SAMPLER_VIEWS,
    VIRGL_CCMD_SET_INDEX_BUFFER,
    VIRGL_CCMD_SET_CONSTANT_BUFFER,
    VIRGL_CCMD_SET_UNIFORM_BUFFER,
    VIRGL_CCMD_SET_SCISSOR_STATE,
    VIRGL_CCMD_BLIT,
    VIRGL_CCMD_RESOURCE_COPY_REGION,
    VIRGL_CCMD_BIND_SAMPLER_STATES,
    VIRGL_CCMD_BEGIN_QUERY,
    VIRGL_CCMD_END_QUERY,
    VIRGL_CCMD_GET_QUERY_RESULT,
    VIRGL_CCMD_SET_POLYGON_STIPPLE,
    VIRGL_CCMD_SET_CLIP_STATE,
    VIRGL_CCMD_SET_SAMPLE_MASK,
    VIRGL_CCMD_SET_STREAMOUT_TARGETS,
    VIRGL_CCMD_SET_RENDER_CONDITION,
    VIRGL_CCMD_SET_BLEND_COLOR,
    VIRGL_CCMD_SET_STENCIL_REF,
    VIRGL_CCMD_RESOURCE_UNREF,
    VIRGL_CCMD_CREATE_SUB_CTX,
    VIRGL_CCMD_DESTROY_SUB_CTX,
    VIRGL_CCMD_BIND_SHADER,
    VIRGL_CCMD_SET_TESS_STATE,
    VIRGL_CCMD_SET_MIN_SAMPLES,
    VIRGL_CCMD_SET_SHADER_BUFFERS,
    VIRGL_CCMD_SET_SHADER_IMAGES,
    VIRGL_CCMD_MEMORY_BARRIER,
    VIRGL_CCMD_LAUNCH_GRID,
    VIRGL_CCMD_SET_FRAMEBUFFER_STATE_NO_ATTACH,
    VIRGL_CCMD_TEXTURE_BARRIER,
    VIRGL_CCMD_SET_ATOMIC_BUFFERS,
    VIRGL_CCMD_SET_DEBUG_FLAGS,
};

// Virgl object types
enum virgl_object_type {
    VIRGL_OBJECT_NULL,
    VIRGL_OBJECT_BLEND,
    VIRGL_OBJECT_RASTERIZER,
    VIRGL_OBJECT_DSA,
    VIRGL_OBJECT_SHADER,
    VIRGL_OBJECT_VERTEX_ELEMENTS,
    VIRGL_OBJECT_SAMPLER_VIEW,
    VIRGL_OBJECT_SAMPLER_STATE,
    VIRGL_OBJECT_SURFACE,
    VIRGL_OBJECT_QUERY,
    VIRGL_OBJECT_STREAMOUT_TARGET,
    VIRGL_OBJECT_MSAA_SURFACE,
};

// Clear buffer bits
#define PIPE_CLEAR_DEPTH        (1 << 0)
#define PIPE_CLEAR_STENCIL      (1 << 1)
#define PIPE_CLEAR_COLOR0       (1 << 2)
#define PIPE_CLEAR_COLOR1       (1 << 3)
#define PIPE_CLEAR_COLOR2       (1 << 4)
#define PIPE_CLEAR_COLOR3       (1 << 5)
#define PIPE_CLEAR_COLOR        (PIPE_CLEAR_COLOR0 | PIPE_CLEAR_COLOR1 | PIPE_CLEAR_COLOR2 | PIPE_CLEAR_COLOR3)

// Texture target types
#define VIRGL_TARGET_BUFFER     0
#define VIRGL_TARGET_1D         1
#define VIRGL_TARGET_2D         2
#define VIRGL_TARGET_3D         3
#define VIRGL_TARGET_CUBE       4
#define VIRGL_TARGET_RECT       5
#define VIRGL_TARGET_1D_ARRAY   6
#define VIRGL_TARGET_2D_ARRAY   7

// Bind flags
#define VIRGL_BIND_DEPTH_STENCIL   (1 << 0)
#define VIRGL_BIND_RENDER_TARGET   (1 << 1)
#define VIRGL_BIND_SAMPLER_VIEW    (1 << 3)
#define VIRGL_BIND_VERTEX_BUFFER   (1 << 4)
#define VIRGL_BIND_INDEX_BUFFER    (1 << 5)
#define VIRGL_BIND_CONSTANT_BUFFER (1 << 6)
#define VIRGL_BIND_STREAM_OUTPUT   (1 << 11)
#define VIRGL_BIND_CURSOR          (1 << 16)
#define VIRGL_BIND_CUSTOM          (1 << 17)
#define VIRGL_BIND_SCANOUT         (1 << 18)

// Virgl command structure
// All virgl commands start with a header: [length_in_dwords] [command_opcode]
#define VIRGL_CMD_HDR(cmd, len) (((len) << 16) | (cmd))

// Helper macros for building virgl commands
#define VIRGL_SET_COMMAND(buf, idx, cmd, len) \
    do { \
        (buf)[idx] = VIRGL_CMD_HDR(cmd, len); \
    } while(0)

#define VIRGL_SET_DWORD(buf, idx, val) \
    do { \
        (buf)[idx] = (uint32_t)(val); \
    } while(0)

// Virgl CLEAR command layout:
// dword 0: command header (length=8, cmd=VIRGL_CCMD_CLEAR)
// dword 1: buffers (PIPE_CLEAR_* bits)
// dword 2-5: color (rgba as floats packed as uint32)
// dword 6: depth (as double, low 32 bits)
// dword 7: depth (high 32 bits)
// dword 8: stencil
#define VIRGL_CLEAR_SIZE 9

// Union for float<->uint32 conversion without strict aliasing issues
union virgl_float_uint {
    float f;
    uint32_t u;
};

static inline uint32_t virgl_pack_float(float f) {
    union virgl_float_uint u;
    u.f = f;
    return u.u;
}

#endif /* _VIRGL_PROTOCOL_H */

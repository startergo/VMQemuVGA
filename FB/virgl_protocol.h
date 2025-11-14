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

// Primitive types for drawing
enum pipe_prim_type {
    PIPE_PRIM_POINTS = 0,
    PIPE_PRIM_LINES = 1,
    PIPE_PRIM_LINE_LOOP = 2,
    PIPE_PRIM_LINE_STRIP = 3,
    PIPE_PRIM_TRIANGLES = 4,
    PIPE_PRIM_TRIANGLE_STRIP = 5,
    PIPE_PRIM_TRIANGLE_FAN = 6,
    PIPE_PRIM_QUADS = 7,
    PIPE_PRIM_QUAD_STRIP = 8,
    PIPE_PRIM_POLYGON = 9,
};

// Shader types
enum pipe_shader_type {
    PIPE_SHADER_VERTEX = 0,
    PIPE_SHADER_FRAGMENT = 1,
    PIPE_SHADER_GEOMETRY = 2,
    PIPE_SHADER_TESS_CTRL = 3,
    PIPE_SHADER_TESS_EVAL = 4,
    PIPE_SHADER_COMPUTE = 5,
};

// Texture formats (subset)
enum virgl_formats {
    VIRGL_FORMAT_B8G8R8A8_UNORM = 1,
    VIRGL_FORMAT_B8G8R8X8_UNORM = 2,
    VIRGL_FORMAT_R8G8B8A8_UNORM = 67,
    VIRGL_FORMAT_R8G8B8X8_UNORM = 68,
    VIRGL_FORMAT_R16G16B16A16_FLOAT = 113,
    VIRGL_FORMAT_R32G32B32A32_FLOAT = 133,
    VIRGL_FORMAT_D24_UNORM_S8_UINT = 40,
    VIRGL_FORMAT_D32_FLOAT = 41,
    VIRGL_FORMAT_Z24X8_UNORM = 42,
};

// Vertex element formats
enum pipe_format {
    PIPE_FORMAT_R32_FLOAT = 0,
    PIPE_FORMAT_R32G32_FLOAT = 1,
    PIPE_FORMAT_R32G32B32_FLOAT = 2,
    PIPE_FORMAT_R32G32B32A32_FLOAT = 3,
    PIPE_FORMAT_R8G8B8A8_UNORM = 4,
    PIPE_FORMAT_B8G8R8A8_UNORM = 5,
};

// Blend functions
enum pipe_blend_func {
    PIPE_BLEND_ADD = 0,
    PIPE_BLEND_SUBTRACT = 1,
    PIPE_BLEND_REVERSE_SUBTRACT = 2,
    PIPE_BLEND_MIN = 3,
    PIPE_BLEND_MAX = 4,
};

// Blend factors
enum pipe_blendfactor {
    PIPE_BLENDFACTOR_ONE = 1,
    PIPE_BLENDFACTOR_SRC_COLOR = 2,
    PIPE_BLENDFACTOR_SRC_ALPHA = 3,
    PIPE_BLENDFACTOR_DST_ALPHA = 4,
    PIPE_BLENDFACTOR_DST_COLOR = 5,
    PIPE_BLENDFACTOR_SRC_ALPHA_SATURATE = 6,
    PIPE_BLENDFACTOR_CONST_COLOR = 7,
    PIPE_BLENDFACTOR_CONST_ALPHA = 8,
    PIPE_BLENDFACTOR_SRC1_COLOR = 9,
    PIPE_BLENDFACTOR_SRC1_ALPHA = 10,
    PIPE_BLENDFACTOR_ZERO = 17,
    PIPE_BLENDFACTOR_INV_SRC_COLOR = 18,
    PIPE_BLENDFACTOR_INV_SRC_ALPHA = 19,
    PIPE_BLENDFACTOR_INV_DST_ALPHA = 20,
    PIPE_BLENDFACTOR_INV_DST_COLOR = 21,
    PIPE_BLENDFACTOR_INV_CONST_COLOR = 23,
    PIPE_BLENDFACTOR_INV_CONST_ALPHA = 24,
    PIPE_BLENDFACTOR_INV_SRC1_COLOR = 25,
    PIPE_BLENDFACTOR_INV_SRC1_ALPHA = 26,
};

// Compare functions
enum pipe_compare_func {
    PIPE_FUNC_NEVER = 0,
    PIPE_FUNC_LESS = 1,
    PIPE_FUNC_EQUAL = 2,
    PIPE_FUNC_LEQUAL = 3,
    PIPE_FUNC_GREATER = 4,
    PIPE_FUNC_NOTEQUAL = 5,
    PIPE_FUNC_GEQUAL = 6,
    PIPE_FUNC_ALWAYS = 7,
};

// Face culling
enum pipe_face {
    PIPE_FACE_NONE = 0,
    PIPE_FACE_FRONT = 1,
    PIPE_FACE_BACK = 2,
    PIPE_FACE_FRONT_AND_BACK = 3,
};

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

// Virgl DRAW_VBO command layout:
// dword 0: command header
// dword 1: start
// dword 2: count
// dword 3: mode (PIPE_PRIM_*)
// dword 4: indexed
// dword 5: instance_count
// dword 6: index_bias
// dword 7: start_instance
// dword 8: primitive_restart
// dword 9: restart_index
// dword 10: min_index
// dword 11: max_index
#define VIRGL_DRAW_VBO_SIZE 12

// Virgl SET_VIEWPORT_STATE command layout:
// dword 0: command header
// dword 1: start_slot
// For each viewport:
//   dword: scale[0] (float as uint32)
//   dword: scale[1]
//   dword: scale[2]
//   dword: translate[0]
//   dword: translate[1]
//   dword: translate[2]
#define VIRGL_VIEWPORT_SIZE(num) (2 + (num) * 6)

// Virgl SET_FRAMEBUFFER_STATE command layout:
// dword 0: command header
// dword 1: nr_cbufs
// dword 2: zsurf_handle
// dword 3-10: cbufs[8] surface handles
#define VIRGL_SET_FRAMEBUFFER_STATE_SIZE 11

// Virgl CREATE_OBJECT command for shaders:
// dword 0: command header
// dword 1: handle
// dword 2: type (VIRGL_OBJECT_SHADER)
// dword 3: shader_type (PIPE_SHADER_*)
// dword 4: shader_offset
// dword 5: shader_num_tokens
// dword 6: shader_so_num_outputs
// dword 7+: shader data
#define VIRGL_CREATE_SHADER_HDR_SIZE 7

// Virgl BIND_SHADER command:
// dword 0: command header
// dword 1: handle
// dword 2: type (PIPE_SHADER_*)
#define VIRGL_BIND_SHADER_SIZE 3

// Virgl CREATE_OBJECT for vertex elements:
// dword 0: command header
// dword 1: handle
// dword 2: type (VIRGL_OBJECT_VERTEX_ELEMENTS)
// For each element:
//   dword: src_offset
//   dword: instance_divisor
//   dword: vertex_buffer_index
//   dword: src_format
#define VIRGL_VERTEX_ELEMENT_SIZE 4

// Virgl SET_VERTEX_BUFFERS command:
// dword 0: command header
// For each buffer:
//   dword: stride
//   dword: offset
//   dword: handle
#define VIRGL_VERTEX_BUFFER_SIZE 3

// Virgl RESOURCE_INLINE_WRITE command:
// dword 0: command header
// dword 1: handle
// dword 2: level
// dword 3: usage
// dword 4: stride
// dword 5: layer_stride
// dword 6: x
// dword 7: y
// dword 8: z
// dword 9: width
// dword 10: height
// dword 11: depth
// dword 12+: data
#define VIRGL_INLINE_WRITE_HDR_SIZE 12

// Union for float<->uint32 conversion without strict aliasing issues
union virgl_float_uint {
    float f;
    uint32_t u;
};

union virgl_double_uint64 {
    double d;
    uint64_t u;
};

static inline uint32_t virgl_pack_float(float f) {
    union virgl_float_uint u;
    u.f = f;
    return u.u;
}

static inline void virgl_pack_double(double d, uint32_t *lo, uint32_t *hi) {
    union virgl_double_uint64 u;
    u.d = d;
    *lo = (uint32_t)(u.u & 0xFFFFFFFF);
    *hi = (uint32_t)(u.u >> 32);
}

#endif /* _VIRGL_PROTOCOL_H */

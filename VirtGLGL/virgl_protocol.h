/*
 * virgl_protocol.h
 * VirGL protocol definitions (subset for basic OpenGL)
 */

#ifndef VIRGL_PROTOCOL_H
#define VIRGL_PROTOCOL_H

#include <stdint.h>

// VirGL command types
#define VIRGL_CCMD_NOP                   0
#define VIRGL_CCMD_CREATE_OBJECT         1
#define VIRGL_CCMD_BIND_OBJECT           2
#define VIRGL_CCMD_DESTROY_OBJECT        3
#define VIRGL_CCMD_SET_VIEWPORT_STATE    4
#define VIRGL_CCMD_SET_FRAMEBUFFER_STATE 5
#define VIRGL_CCMD_SET_VERTEX_BUFFERS    6
#define VIRGL_CCMD_CLEAR                 7
#define VIRGL_CCMD_DRAW_VBO              8
#define VIRGL_CCMD_RESOURCE_INLINE_WRITE 9
#define VIRGL_CCMD_SET_SAMPLER_VIEWS     10
#define VIRGL_CCMD_SET_INDEX_BUFFER      11
#define VIRGL_CCMD_SET_CONSTANT_BUFFER   12
#define VIRGL_CCMD_SET_UNIFORM_BUFFER    13
#define VIRGL_CCMD_SET_VERTEX_STATE      14

// Object types
#define VIRGL_OBJECT_BLEND               1
#define VIRGL_OBJECT_RASTERIZER          2
#define VIRGL_OBJECT_DSA                 3
#define VIRGL_OBJECT_SHADER              4
#define VIRGL_OBJECT_VERTEX_ELEMENTS     5
#define VIRGL_OBJECT_SURFACE             6
#define VIRGL_OBJECT_SAMPLER_VIEW        7
#define VIRGL_OBJECT_SAMPLER_STATE       8
#define VIRGL_OBJECT_QUERY               9
#define VIRGL_OBJECT_STREAMOUT_TARGET    10

// Formats
#define VIRGL_FORMAT_B8G8R8A8_UNORM      1
#define VIRGL_FORMAT_B8G8R8X8_UNORM      2
#define VIRGL_FORMAT_R8G8B8A8_UNORM      67
#define VIRGL_FORMAT_R8G8B8X8_UNORM      68

// Clear buffers
#define PIPE_CLEAR_DEPTH                 (1 << 0)
#define PIPE_CLEAR_STENCIL               (1 << 1)
#define PIPE_CLEAR_COLOR0                (1 << 2)
#define PIPE_CLEAR_COLOR1                (1 << 3)
#define PIPE_CLEAR_COLOR2                (1 << 4)
#define PIPE_CLEAR_COLOR3                (1 << 5)

// VirGL command header
struct virgl_cmd_header {
    uint32_t command : 8;
    uint32_t length : 24;  // Length in dwords (excluding header)
};

// Helper to pack floats for virgl protocol
static inline uint32_t virgl_pack_float(float f)
{
    union {
        float f;
        uint32_t u;
    } conv;
    conv.f = f;
    return conv.u;
}

// Helper to create command header
static inline uint32_t virgl_cmd_header_pack(uint8_t cmd, uint32_t len)
{
    return (cmd & 0xFF) | ((len & 0xFFFFFF) << 8);
}

#endif /* VIRGL_PROTOCOL_H */

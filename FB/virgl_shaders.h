/*
 * virgl_shaders.h - Default GLSL shaders for virgl rendering
 * 
 * Provides simple passthrough vertex and fragment shaders for immediate mode rendering
 */

#ifndef _VIRGL_SHADERS_H
#define _VIRGL_SHADERS_H

// Simple vertex shader - passthrough position and color
static const char* virgl_default_vertex_shader = 
    "#version 130\n"
    "in vec4 position;\n"
    "in vec4 color;\n"
    "in vec4 texcoord;\n"
    "out vec4 v_color;\n"
    "out vec4 v_texcoord;\n"
    "void main() {\n"
    "    gl_Position = position;\n"
    "    v_color = color;\n"
    "    v_texcoord = texcoord;\n"
    "}\n";

// Simple fragment shader - output interpolated color
static const char* virgl_default_fragment_shader =
    "#version 130\n"
    "in vec4 v_color;\n"
    "in vec4 v_texcoord;\n"
    "out vec4 fragColor;\n"
    "void main() {\n"
    "    fragColor = v_color;\n"
    "}\n";

// Shader type constants (from Gallium/virgl)
#define PIPE_SHADER_VERTEX   0
#define PIPE_SHADER_FRAGMENT 1
#define PIPE_SHADER_GEOMETRY 2
#define PIPE_SHADER_TESS_CTRL 3
#define PIPE_SHADER_TESS_EVAL 4
#define PIPE_SHADER_COMPUTE  5

#endif /* _VIRGL_SHADERS_H */

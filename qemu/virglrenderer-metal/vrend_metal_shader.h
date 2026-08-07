/*
 * virglrenderer Metal Backend - Shader Translation
 * 
 * GLSL → MSL shader translation adapted from your metal_server.m
 */

#ifndef VREND_METAL_SHADER_H
#define VREND_METAL_SHADER_H

#include <stdint.h>
#include <Metal/Metal.h>

/* Shader types */
enum vrend_shader_type {
    VREND_SHADER_VERTEX = 0,
    VREND_SHADER_FRAGMENT = 1,
    VREND_SHADER_COMPUTE = 2,
};

/* Shader translation result */
struct vrend_metal_shader {
    uint32_t shader_id;
    enum vrend_shader_type type;
    id<MTLFunction> metal_function;
    char *msl_source;  /* Translated Metal source */
};

/* Create shader from GLSL source - main entry point */
struct vrend_metal_shader* vrend_metal_shader_create(
    uint32_t shader_id,
    enum vrend_shader_type type,
    const char *glsl_source,
    uint32_t glsl_len);

/* Destroy shader */
void vrend_metal_shader_destroy(struct vrend_metal_shader *shader);

/* GLSL → MSL translation (ported from metal_server.m) */
char* vrend_metal_translate_glsl_to_msl(
    const char *glsl_source,
    enum vrend_shader_type type);

/* Shader compilation */
id<MTLFunction> vrend_metal_compile_shader(
    const char *msl_source,
    const char *entry_point);

/* Shared device accessor */
id<MTLDevice> vrend_metal_get_device(void);

#endif /* VREND_METAL_SHADER_H */

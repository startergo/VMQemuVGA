/*
 * virglrenderer Metal Backend - Pipeline State Management
 */

#ifndef VREND_METAL_PIPELINE_H
#define VREND_METAL_PIPELINE_H

#include <stdint.h>
#include <Metal/Metal.h>

/* Pipeline state structure */
struct vrend_metal_pipeline {
    uint32_t pipeline_id;
    id<MTLRenderPipelineState> pipeline_state;
    
    /* Cached shader references */
    uint32_t vertex_shader_id;
    uint32_t fragment_shader_id;
    
    /* Blend state */
    uint32_t blend_enabled;
    uint32_t src_blend_factor;
    uint32_t dst_blend_factor;
    
    /* Depth/stencil state */
    id<MTLDepthStencilState> depth_stencil_state;
};

/* Create pipeline from shader pair */
struct vrend_metal_pipeline* vrend_metal_pipeline_create(
    uint32_t pipeline_id,
    id<MTLFunction> vertex_function,
    id<MTLFunction> fragment_function,
    id<MTLDevice> device);

/* Destroy pipeline */
void vrend_metal_pipeline_destroy(struct vrend_metal_pipeline *pipeline);

/* Update blend state */
void vrend_metal_pipeline_set_blend_state(
    struct vrend_metal_pipeline *pipeline,
    uint32_t enabled,
    uint32_t src_factor,
    uint32_t dst_factor);

/* Update depth/stencil state */
void vrend_metal_pipeline_set_depth_stencil_state(
    struct vrend_metal_pipeline *pipeline,
    id<MTLDevice> device,
    uint32_t depth_test_enabled,
    uint32_t depth_write_enabled,
    uint32_t stencil_test_enabled);

#endif /* VREND_METAL_PIPELINE_H */

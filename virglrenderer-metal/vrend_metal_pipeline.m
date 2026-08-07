/*
 * virglrenderer Metal Backend - Pipeline State Management Implementation
 */

#import <Metal/Metal.h>
#import <Foundation/Foundation.h>
#include "vrend_metal_pipeline.h"

/* Create pipeline from shader pair */
struct vrend_metal_pipeline* vrend_metal_pipeline_create(
    uint32_t pipeline_id,
    id<MTLFunction> vertex_function,
    id<MTLFunction> fragment_function,
    id<MTLDevice> device) {
    
    if (!vertex_function || !fragment_function || !device) {
        NSLog(@"[Pipeline] Invalid parameters for pipeline creation");
        return NULL;
    }
    
    @autoreleasepool {
        struct vrend_metal_pipeline *pipeline = calloc(1, sizeof(*pipeline));
        if (!pipeline) {
            return NULL;
        }
        
        pipeline->pipeline_id = pipeline_id;
        
        /* Create pipeline descriptor */
        MTLRenderPipelineDescriptor *desc = [[MTLRenderPipelineDescriptor alloc] init];
        desc.vertexFunction = vertex_function;
        desc.fragmentFunction = fragment_function;
        
        /* Configure color attachments */
        desc.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
        desc.colorAttachments[0].blendingEnabled = YES;
        desc.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
        desc.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
        desc.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
        desc.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorOne;
        desc.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorZero;
        desc.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;
        
        /* Configure depth/stencil */
        desc.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
        desc.stencilAttachmentPixelFormat = MTLPixelFormatStencil8;
        
        /* Create pipeline state */
        NSError *error = nil;
        pipeline->pipeline_state = [device newRenderPipelineStateWithDescriptor:desc
                                                                          error:&error];
        
        if (!pipeline->pipeline_state) {
            NSLog(@"[Pipeline] Failed to create pipeline state: %@", error);
            free(pipeline);
            return NULL;
        }
        
        /* Create default depth/stencil state */
        MTLDepthStencilDescriptor *depthDesc = [[MTLDepthStencilDescriptor alloc] init];
        depthDesc.depthCompareFunction = MTLCompareFunctionLess;
        depthDesc.depthWriteEnabled = YES;
        
        pipeline->depth_stencil_state = [device newDepthStencilStateWithDescriptor:depthDesc];
        
        NSLog(@"[Pipeline] Created pipeline %u", pipeline_id);
        
        return pipeline;
    }
}

/* Destroy pipeline */
void vrend_metal_pipeline_destroy(struct vrend_metal_pipeline *pipeline) {
    if (!pipeline) {
        return;
    }
    
    @autoreleasepool {
        pipeline->pipeline_state = nil;
        pipeline->depth_stencil_state = nil;
        
        NSLog(@"[Pipeline] Destroyed pipeline %u", pipeline->pipeline_id);
        free(pipeline);
    }
}

/* Update blend state */
void vrend_metal_pipeline_set_blend_state(
    struct vrend_metal_pipeline *pipeline,
    uint32_t enabled,
    uint32_t src_factor,
    uint32_t dst_factor) {
    
    if (!pipeline) {
        return;
    }
    
    pipeline->blend_enabled = enabled;
    pipeline->src_blend_factor = src_factor;
    pipeline->dst_blend_factor = dst_factor;
    
    NSLog(@"[Pipeline] Updated blend state: enabled=%u src=%u dst=%u",
          enabled, src_factor, dst_factor);
    
    /* Note: In real implementation, would need to recreate pipeline state
     * with new blend configuration */
}

/* Convert virgl compare function to Metal */
static MTLCompareFunction virgl_to_metal_compare(uint32_t func) {
    switch (func) {
        case 0: return MTLCompareFunctionNever;
        case 1: return MTLCompareFunctionLess;
        case 2: return MTLCompareFunctionEqual;
        case 3: return MTLCompareFunctionLessEqual;
        case 4: return MTLCompareFunctionGreater;
        case 5: return MTLCompareFunctionNotEqual;
        case 6: return MTLCompareFunctionGreaterEqual;
        case 7: return MTLCompareFunctionAlways;
        default: return MTLCompareFunctionAlways;
    }
}

/* Update depth/stencil state */
void vrend_metal_pipeline_set_depth_stencil_state(
    struct vrend_metal_pipeline *pipeline,
    id<MTLDevice> device,
    uint32_t depth_test_enabled,
    uint32_t depth_write_enabled,
    uint32_t stencil_test_enabled) {
    
    if (!pipeline || !device) {
        return;
    }
    
    @autoreleasepool {
        MTLDepthStencilDescriptor *desc = [[MTLDepthStencilDescriptor alloc] init];
        
        if (depth_test_enabled) {
            desc.depthCompareFunction = MTLCompareFunctionLess;
            desc.depthWriteEnabled = depth_write_enabled ? YES : NO;
        } else {
            desc.depthCompareFunction = MTLCompareFunctionAlways;
            desc.depthWriteEnabled = NO;
        }
        
        if (stencil_test_enabled) {
            MTLStencilDescriptor *stencilDesc = [[MTLStencilDescriptor alloc] init];
            stencilDesc.stencilCompareFunction = MTLCompareFunctionAlways;
            stencilDesc.stencilFailureOperation = MTLStencilOperationKeep;
            stencilDesc.depthFailureOperation = MTLStencilOperationKeep;
            stencilDesc.depthStencilPassOperation = MTLStencilOperationKeep;
            
            desc.frontFaceStencil = stencilDesc;
            desc.backFaceStencil = stencilDesc;
        }
        
        pipeline->depth_stencil_state = [device newDepthStencilStateWithDescriptor:desc];
        
        NSLog(@"[Pipeline] Updated depth/stencil: depth_test=%u depth_write=%u stencil=%u",
              depth_test_enabled, depth_write_enabled, stencil_test_enabled);
    }
}

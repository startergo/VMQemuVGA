/*
 * virglrenderer Metal Backend - Core Implementation
 *
 * This is the main interface between virglrenderer and Metal.
 * Adapts your existing metal_server.m code to virglrenderer architecture.
 */

#import <Metal/Metal.h>
#import <Foundation/Foundation.h>
#import <TargetConditionals.h>
#import <IOSurface/IOSurface.h>
#import <CoreVideo/CoreVideo.h>
#import <mach/mach_time.h>
#include <mach/mach_port.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <objc/message.h>
#include "vrend_metal.h"
#include "vrend_metal_shader.h"
#include "vrend_metal_priv.h"
#include "pipe/p_defines.h"

#define VREND_MAX_SCANOUTS 8

static bool g_metal_initialized;
static id<MTLDevice> g_metal_device;
static id<MTLCommandQueue> g_metal_queue;
static NSMutableDictionary *g_metal_global_textures;

static vrend_metal_shared_event_listener g_shared_event_listener;
static void *g_shared_event_userdata;

static vrend_metal_scanout_callback g_scanout_callback;
static void *g_scanout_callback_user;
static id<MTLTexture> g_scanout_textures[VREND_MAX_SCANOUTS];
static bool g_scanout_region_valid[VREND_MAX_SCANOUTS];
static struct virgl_box g_scanout_regions[VREND_MAX_SCANOUTS];
static id<MTLBuffer> g_scanout_staging[VREND_MAX_SCANOUTS];
static uint32_t g_scanout_row_bytes[VREND_MAX_SCANOUTS];
static uint32_t g_scanout_width[VREND_MAX_SCANOUTS];
static uint32_t g_scanout_height[VREND_MAX_SCANOUTS];
static MTLPixelFormat g_scanout_format[VREND_MAX_SCANOUTS];
static uint32_t g_scanout_bytes_per_pixel[VREND_MAX_SCANOUTS];
static uint32_t g_scanout_ctx_ids[VREND_MAX_SCANOUTS];
static IOSurfaceRef g_scanout_iosurfaces[VREND_MAX_SCANOUTS];
static id<MTLTexture> g_scanout_iosurface_textures[VREND_MAX_SCANOUTS];
static uint64_t g_scanout_frame_serials[VREND_MAX_SCANOUTS];
static uint64_t g_scanout_throttle_interval[VREND_MAX_SCANOUTS];
static uint64_t g_scanout_last_dispatch_time[VREND_MAX_SCANOUTS];
static uint64_t g_scanout_frame_counter;

static mach_timebase_info_data_t g_timebase;
static NSMutableDictionary *g_context_registry;
static NSLock *g_context_registry_lock;

static uint32_t vrend_metal_bytes_per_pixel(MTLPixelFormat fmt) {
    switch (fmt) {
        case MTLPixelFormatBGRA8Unorm:
        case MTLPixelFormatRGBA8Unorm:
        case MTLPixelFormatBGRA8Unorm_sRGB:
        case MTLPixelFormatRGBA8Unorm_sRGB:
            return 4;
        default:
            NSLog(@"[Metal Backend] Unsupported scanout pixel format %lu, assuming 4 bpp", (unsigned long)fmt);
            return 4;
    }
}

static void vrend_metal_init_timebase(void) {
    if (g_timebase.denom == 0) {
        mach_timebase_info(&g_timebase);
    }
}

static uint64_t vrend_metal_ns_to_abs(uint64_t ns) {
    vrend_metal_init_timebase();
    return (ns * g_timebase.denom) / g_timebase.numer;
}

static uint64_t vrend_metal_abs_to_ns(uint64_t abs) {
    vrend_metal_init_timebase();
    return (abs * g_timebase.numer) / g_timebase.denom;
}

static void vrend_metal_register_context(struct vrend_metal_context *ctx) {
    if (!g_context_registry) {
        g_context_registry = [[NSMutableDictionary alloc] init];
    }
    if (!g_context_registry_lock) {
        g_context_registry_lock = [[NSLock alloc] init];
    }
    [g_context_registry_lock lock];
    g_context_registry[@(ctx->ctx_id)] = [NSValue valueWithPointer:ctx];
    [g_context_registry_lock unlock];
}

static void vrend_metal_unregister_context(struct vrend_metal_context *ctx) {
    if (!g_context_registry_lock) {
        return;
    }
    [g_context_registry_lock lock];
    [g_context_registry removeObjectForKey:@(ctx->ctx_id)];
    [g_context_registry_lock unlock];
}

static struct vrend_metal_context *vrend_metal_lookup_context(uint32_t ctx_id) {
    if (!g_context_registry_lock) {
        return NULL;
    }
    [g_context_registry_lock lock];
    NSValue *value = g_context_registry[@(ctx_id)];
    [g_context_registry_lock unlock];
    return value ? (struct vrend_metal_context *)[value pointerValue] : NULL;
}

static mach_port_t vrend_metal_shared_event_handle_port(id handle) {
#if TARGET_OS_OSX
    if (@available(macOS 10.14, *)) {
        if (handle && [handle respondsToSelector:@selector(machPort)]) {
            typedef mach_port_t (*MachPortGetter)(id, SEL);
            MachPortGetter getter = (MachPortGetter)objc_msgSend;
            return getter(handle, @selector(machPort));
        }
    }
#endif
    return MACH_PORT_NULL;
}

static mach_port_t vrend_metal_ensure_shared_event_port(struct vrend_metal_context *ctx) {
    if (!ctx || !ctx->shared_event) {
        return MACH_PORT_NULL;
    }
    if (ctx->shared_event_port != MACH_PORT_NULL) {
        return ctx->shared_event_port;
    }
#if TARGET_OS_OSX
    if (@available(macOS 10.14, *)) {
        ctx->shared_event_handle = [ctx->shared_event newSharedEventHandle];
        ctx->shared_event_port = vrend_metal_shared_event_handle_port(ctx->shared_event_handle);
    }
#endif
    return ctx->shared_event_port;
}

static bool vrend_metal_populate_shared_event_info(struct vrend_metal_context *ctx,
                                                   struct vrend_metal_shared_event_info *info,
                                                   mach_port_t override_port) {
    if (!ctx || !info) {
        return false;
    }
    memset(info, 0, sizeof(*info));
    info->ctx_id = ctx->ctx_id;
    info->signal_value = ctx->shared_event_value;
    mach_port_t port = override_port ? override_port : vrend_metal_ensure_shared_event_port(ctx);
    if (port != MACH_PORT_NULL) {
        info->mach_port = port;
    }
    if (ctx->shared_event_handle) {
        info->shared_event_handle = (uint64_t)(uintptr_t)ctx->shared_event_handle;
    } else if (ctx->shared_event) {
#if TARGET_OS_OSX
        if (@available(macOS 10.14, *)) {
            ctx->shared_event_handle = [ctx->shared_event newSharedEventHandle];
            if (ctx->shared_event_handle) {
                info->shared_event_handle = (uint64_t)(uintptr_t)ctx->shared_event_handle;
            }
        }
#endif
    }
    return info->mach_port != MACH_PORT_NULL || info->shared_event_handle != 0;
}

static void vrend_metal_emit_shared_event_info(struct vrend_metal_context *ctx, mach_port_t override_port) {
    if (!g_shared_event_listener || !ctx) {
        return;
    }
    struct vrend_metal_shared_event_info info;
    if (!vrend_metal_populate_shared_event_info(ctx, &info, override_port)) {
        return;
    }
    g_shared_event_listener(&info, g_shared_event_userdata);
}

static uint32_t vrend_metal_iosurface_fourcc(MTLPixelFormat fmt) {
    switch (fmt) {
        case MTLPixelFormatBGRA8Unorm:
        case MTLPixelFormatBGRA8Unorm_sRGB:
            return kCVPixelFormatType_32BGRA;
        case MTLPixelFormatRGBA8Unorm:
        case MTLPixelFormatRGBA8Unorm_sRGB:
            return kCVPixelFormatType_32RGBA;
        default:
            return 0;
    }
}

static void vrend_metal_release_scanout_iosurface(uint32_t scanout_id) {
    if (scanout_id >= VREND_MAX_SCANOUTS) {
        return;
    }
    if (g_scanout_iosurface_textures[scanout_id]) {
        g_scanout_iosurface_textures[scanout_id] = nil;
    }
    if (g_scanout_iosurfaces[scanout_id]) {
        CFRelease(g_scanout_iosurfaces[scanout_id]);
        g_scanout_iosurfaces[scanout_id] = NULL;
    }
}

static bool vrend_metal_ensure_iosurface(uint32_t scanout_id,
                                          uint32_t width,
                                          uint32_t height,
                                          MTLPixelFormat fmt) {
    if (scanout_id >= VREND_MAX_SCANOUTS || width == 0 || height == 0) {
        return false;
    }
    uint32_t fourcc = vrend_metal_iosurface_fourcc(fmt);
    if (!fourcc) {
        vrend_metal_release_scanout_iosurface(scanout_id);
        return false;
    }

    IOSurfaceRef existing = g_scanout_iosurfaces[scanout_id];
    bool needs_realloc = false;
    if (!existing) {
        needs_realloc = true;
    } else if (IOSurfaceGetWidth(existing) != width ||
               IOSurfaceGetHeight(existing) != height ||
               IOSurfaceGetPixelFormat(existing) != fourcc) {
        needs_realloc = true;
    } else if (!g_scanout_iosurface_textures[scanout_id]) {
        needs_realloc = true;
    }

    if (!needs_realloc) {
        return true;
    }

    vrend_metal_release_scanout_iosurface(scanout_id);

    uint32_t bytes_per_element = vrend_metal_bytes_per_pixel(fmt);
    if (bytes_per_element == 0) {
        bytes_per_element = 4;
    }
    uint32_t bytes_per_row = width * bytes_per_element;

    NSDictionary *props = @{
            (__bridge NSString *)kIOSurfaceWidth: @(width),
            (__bridge NSString *)kIOSurfaceHeight: @(height),
            (__bridge NSString *)kIOSurfaceBytesPerElement: @(bytes_per_element),
            (__bridge NSString *)kIOSurfaceBytesPerRow: @(bytes_per_row),
            (__bridge NSString *)kIOSurfacePixelFormat: @(fourcc)
        };

        IOSurfaceRef surface = IOSurfaceCreate((__bridge CFDictionaryRef)props);
        if (!surface) {
            NSLog(@"[Metal Backend] Failed to allocate IOSurface for scanout %u", scanout_id);
            return false;
        }

        MTLTextureDescriptor *desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:fmt
                                                                                         width:width
                                                                                        height:height
                                                                                     mipmapped:NO];
        desc.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
        desc.storageMode = MTLStorageModeShared;

        id<MTLTexture> iosurfaceTex = [g_metal_device newTextureWithDescriptor:desc iosurface:surface plane:0];
        if (!iosurfaceTex) {
            NSLog(@"[Metal Backend] Failed to create Metal texture for IOSurface scanout %u", scanout_id);
            CFRelease(surface);
            return false;
        }

        g_scanout_iosurfaces[scanout_id] = surface;
        g_scanout_iosurface_textures[scanout_id] = iosurfaceTex;
        NSLog(@"[Metal Backend] Configured IOSurface scanout %u %ux%u (fmt %u)",
            scanout_id, width, height, fourcc);
        return true;
    }

static bool vrend_metal_ensure_scanout_storage(uint32_t scanout_id, id<MTLTexture> texture) {
    if (!texture) {
        return false;
    }
    uint32_t width = (uint32_t)[texture width];
    uint32_t height = (uint32_t)[texture height];
    uint32_t bpp = vrend_metal_bytes_per_pixel([texture pixelFormat]);
    if (width == 0 || height == 0) {
        NSLog(@"[Metal Backend] Invalid scanout dimensions for id %u", scanout_id);
        return false;
    }
    bool needs_realloc = false;
    if (!g_scanout_staging[scanout_id]) {
        needs_realloc = true;
    } else if (g_scanout_width[scanout_id] != width ||
               g_scanout_height[scanout_id] != height ||
               g_scanout_format[scanout_id] != [texture pixelFormat]) {
        needs_realloc = true;
    }
    if (needs_realloc) {
        g_scanout_staging[scanout_id] = nil;
        NSUInteger row_bytes = (NSUInteger)width * bpp;
        NSUInteger buffer_size = row_bytes * height;
        id<MTLBuffer> staging = [g_metal_device newBufferWithLength:buffer_size
                                                            options:MTLResourceStorageModeShared];
        if (!staging) {
            NSLog(@"[Metal Backend] Failed to allocate staging buffer for scanout %u", scanout_id);
            return false;
        }
        g_scanout_staging[scanout_id] = staging;
        g_scanout_row_bytes[scanout_id] = (uint32_t)row_bytes;
        g_scanout_width[scanout_id] = width;
        g_scanout_height[scanout_id] = height;
        g_scanout_format[scanout_id] = [texture pixelFormat];
        g_scanout_bytes_per_pixel[scanout_id] = bpp;
        NSLog(@"[Metal Backend] Configured scanout %u staging buffer %ux%u (%u bpp)",
              scanout_id, width, height, bpp);
    }

    vrend_metal_ensure_iosurface(scanout_id, width, height, [texture pixelFormat]);
    return g_scanout_staging[scanout_id] != nil;
}

static void vrend_metal_stage_scanout_region(
    struct vrend_metal_context *ctx,
    uint32_t scanout_id,
    id<MTLTexture> texture,
    const struct virgl_box *region_hint) {
    if (!texture || !g_scanout_staging[scanout_id]) {
        return;
    }
    struct virgl_box temp_region;
    const struct virgl_box *region = region_hint;
    if (!region) {
        if (g_scanout_region_valid[scanout_id]) {
            region = &g_scanout_regions[scanout_id];
        } else {
            temp_region.x = 0;
            temp_region.y = 0;
            temp_region.z = 0;
            temp_region.w = g_scanout_width[scanout_id];
            temp_region.h = g_scanout_height[scanout_id];
            temp_region.d = 1;
            region = &temp_region;
        }
    }
    uint32_t copy_width = region->w ? region->w : g_scanout_width[scanout_id];
    uint32_t copy_height = region->h ? region->h : g_scanout_height[scanout_id];
    if (copy_width == 0 || copy_height == 0) {
        return;
    }
    id<MTLCommandBuffer> blit_cb = [g_metal_queue commandBuffer];
    id<MTLBlitCommandEncoder> blit = [blit_cb blitCommandEncoder];
    MTLOrigin origin = MTLOriginMake(region->x, region->y, 0);
    MTLSize size = MTLSizeMake(copy_width, copy_height, 1);
    NSUInteger dst_bytes_per_row = g_scanout_row_bytes[scanout_id];
    NSUInteger dst_bytes_per_image = dst_bytes_per_row * copy_height;
    [blit copyFromTexture:texture
              sourceSlice:0
              sourceLevel:0
             sourceOrigin:origin
               sourceSize:size
                toBuffer:g_scanout_staging[scanout_id]
       destinationOffset:0
  destinationBytesPerRow:dst_bytes_per_row
destinationBytesPerImage:dst_bytes_per_image];

        id<MTLTexture> iosurfaceTex = g_scanout_iosurface_textures[scanout_id];
        if (iosurfaceTex) {
                [blit copyFromTexture:texture
                                    sourceSlice:0
                                    sourceLevel:0
                                 sourceOrigin:origin
                                     sourceSize:size
                                    toTexture:iosurfaceTex
                     destinationSlice:0
                     destinationLevel:0
                    destinationOrigin:origin];
        }
    [blit endEncoding];
    [blit_cb commit];
    [blit_cb waitUntilCompleted];
    bool drop_frame = false;
    if (g_scanout_throttle_interval[scanout_id]) {
        uint64_t now = mach_absolute_time();
        uint64_t last = g_scanout_last_dispatch_time[scanout_id];
        uint64_t interval = g_scanout_throttle_interval[scanout_id];
        if (last && (now - last) < interval) {
            drop_frame = true;
        } else {
            g_scanout_last_dispatch_time[scanout_id] = now;
        }
    }
    if (!drop_frame && g_scanout_callback) {
        g_scanout_ctx_ids[scanout_id] = ctx ? ctx->ctx_id : 0;
        g_scanout_frame_counter++;
        g_scanout_frame_serials[scanout_id] = g_scanout_frame_counter;
        g_scanout_callback(
            scanout_id,
            region,
            [g_scanout_staging[scanout_id] contents],
            g_scanout_row_bytes[scanout_id],
            g_scanout_width[scanout_id],
            g_scanout_height[scanout_id],
            g_scanout_frame_counter,
            g_scanout_callback_user);
    }
}

MTLVertexFormat vrend_metal_vertex_format_from_pipe(uint32_t format) {
    switch (format) {
        case VREND_PIPE_FORMAT_R32_FLOAT:
            return MTLVertexFormatFloat;
        case VREND_PIPE_FORMAT_R32G32_FLOAT:
            return MTLVertexFormatFloat2;
        case VREND_PIPE_FORMAT_R32G32B32_FLOAT:
            return MTLVertexFormatFloat3;
        case VREND_PIPE_FORMAT_R32G32B32A32_FLOAT:
            return MTLVertexFormatFloat4;
        case VREND_PIPE_FORMAT_R8G8B8A8_UNORM:
        case VREND_PIPE_FORMAT_B8G8R8A8_UNORM:
            return MTLVertexFormatUChar4Normalized;
        case VREND_PIPE_FORMAT_R8G8B8A8_SNORM:
            return MTLVertexFormatChar4Normalized;
        case VREND_PIPE_FORMAT_R8G8B8A8_UINT:
            return MTLVertexFormatUChar4;
        case VREND_PIPE_FORMAT_R8G8B8A8_SINT:
            return MTLVertexFormatChar4;
        case VREND_PIPE_FORMAT_R8G8B8A8_USCALED:
            return MTLVertexFormatUChar4;
        case VREND_PIPE_FORMAT_R8G8B8A8_SSCALED:
            return MTLVertexFormatChar4;
        case VREND_PIPE_FORMAT_R16G16_FLOAT:
            return MTLVertexFormatHalf2;
        case VREND_PIPE_FORMAT_R16G16B16A16_FLOAT:
            return MTLVertexFormatHalf4;
        case VREND_PIPE_FORMAT_R16G16_UINT:
            return MTLVertexFormatUShort2;
        case VREND_PIPE_FORMAT_R16G16_SINT:
            return MTLVertexFormatShort2;
        case VREND_PIPE_FORMAT_R16G16_USCALED:
            return MTLVertexFormatUShort2;
        case VREND_PIPE_FORMAT_R16G16_SSCALED:
            return MTLVertexFormatShort2;
        case VREND_PIPE_FORMAT_R16G16B16A16_UINT:
            return MTLVertexFormatUShort4;
        case VREND_PIPE_FORMAT_R16G16B16A16_SINT:
            return MTLVertexFormatShort4;
        case VREND_PIPE_FORMAT_R16G16_UNORM:
            return MTLVertexFormatUShort2Normalized;
        case VREND_PIPE_FORMAT_R16G16_SNORM:
            return MTLVertexFormatShort2Normalized;
        case VREND_PIPE_FORMAT_R16G16B16A16_UNORM:
            return MTLVertexFormatUShort4Normalized;
        case VREND_PIPE_FORMAT_R16G16B16A16_SNORM:
            return MTLVertexFormatShort4Normalized;
        case VREND_PIPE_FORMAT_R16G16B16A16_USCALED:
            return MTLVertexFormatUShort4;
        case VREND_PIPE_FORMAT_R16G16B16A16_SSCALED:
            return MTLVertexFormatShort4;
        case VREND_PIPE_FORMAT_R10G10B10A2_UNORM:
        case VREND_PIPE_FORMAT_R10G10B10A2_UINT:
            return MTLVertexFormatUInt1010102Normalized;
        case VREND_PIPE_FORMAT_R11G11B10_FLOAT:
#if TARGET_OS_OSX
            if (@available(macOS 14.0, *)) {
                return MTLVertexFormatFloatRG11B10;
            }
#endif
            return MTLVertexFormatInvalid;
        case VREND_PIPE_FORMAT_R32G32_UINT:
            return MTLVertexFormatUInt2;
        case VREND_PIPE_FORMAT_R32G32B32A32_UINT:
            return MTLVertexFormatUInt4;
        case VREND_PIPE_FORMAT_R32G32_SINT:
            return MTLVertexFormatInt2;
        case VREND_PIPE_FORMAT_R32G32B32A32_SINT:
            return MTLVertexFormatInt4;
        case VREND_PIPE_FORMAT_R64_FLOAT:
        case VREND_PIPE_FORMAT_R64G64_FLOAT:
        case VREND_PIPE_FORMAT_R64G64B64_FLOAT:
        case VREND_PIPE_FORMAT_R64G64B64A64_FLOAT:
            NSLog(@"[Metal Backend] Double-precision vertex format %u is not supported on Metal", format);
            return MTLVertexFormatInvalid;
        default:
            return MTLVertexFormatInvalid;
    }
}

bool vrend_metal_pipe_format_supported(uint32_t format) {
    return vrend_metal_vertex_format_from_pipe(format) != MTLVertexFormatInvalid;
}

static NSUInteger vrend_metal_vertex_format_stride(uint32_t format) {
    switch (format) {
        case VREND_PIPE_FORMAT_R32_FLOAT:
            return sizeof(float) * 1;
        case VREND_PIPE_FORMAT_R32G32_FLOAT:
            return sizeof(float) * 2;
        case VREND_PIPE_FORMAT_R32G32B32_FLOAT:
            return sizeof(float) * 3;
        case VREND_PIPE_FORMAT_R32G32B32A32_FLOAT:
            return sizeof(float) * 4;
        case VREND_PIPE_FORMAT_R8G8B8A8_UNORM:
        case VREND_PIPE_FORMAT_B8G8R8A8_UNORM:
        case VREND_PIPE_FORMAT_R8G8B8A8_SNORM:
        case VREND_PIPE_FORMAT_R8G8B8A8_UINT:
        case VREND_PIPE_FORMAT_R8G8B8A8_SINT:
        case VREND_PIPE_FORMAT_R8G8B8A8_USCALED:
        case VREND_PIPE_FORMAT_R8G8B8A8_SSCALED:
        case VREND_PIPE_FORMAT_R10G10B10A2_UNORM:
        case VREND_PIPE_FORMAT_R10G10B10A2_UINT:
        case VREND_PIPE_FORMAT_R11G11B10_FLOAT:
            return 4;
        case VREND_PIPE_FORMAT_R16G16_FLOAT:
        case VREND_PIPE_FORMAT_R16G16_UINT:
        case VREND_PIPE_FORMAT_R16G16_SINT:
        case VREND_PIPE_FORMAT_R16G16_UNORM:
        case VREND_PIPE_FORMAT_R16G16_SNORM:
        case VREND_PIPE_FORMAT_R16G16_USCALED:
        case VREND_PIPE_FORMAT_R16G16_SSCALED:
            return sizeof(uint16_t) * 2;
        case VREND_PIPE_FORMAT_R16G16B16A16_FLOAT:
        case VREND_PIPE_FORMAT_R16G16B16A16_UINT:
        case VREND_PIPE_FORMAT_R16G16B16A16_SINT:
        case VREND_PIPE_FORMAT_R16G16B16A16_UNORM:
        case VREND_PIPE_FORMAT_R16G16B16A16_SNORM:
        case VREND_PIPE_FORMAT_R16G16B16A16_USCALED:
        case VREND_PIPE_FORMAT_R16G16B16A16_SSCALED:
            return sizeof(uint16_t) * 4;
        case VREND_PIPE_FORMAT_R32G32_UINT:
        case VREND_PIPE_FORMAT_R32G32_SINT:
            return sizeof(uint32_t) * 2;
        case VREND_PIPE_FORMAT_R32G32B32A32_UINT:
        case VREND_PIPE_FORMAT_R32G32B32A32_SINT:
            return sizeof(uint32_t) * 4;
        case VREND_PIPE_FORMAT_R64_FLOAT:
            return sizeof(double) * 1;
        case VREND_PIPE_FORMAT_R64G64_FLOAT:
            return sizeof(double) * 2;
        case VREND_PIPE_FORMAT_R64G64B64_FLOAT:
            return sizeof(double) * 3;
        case VREND_PIPE_FORMAT_R64G64B64A64_FLOAT:
            return sizeof(double) * 4;
        default:
            return 0;
    }
}

static void vrend_metal_self_test_vertex_formats(void) {
#if DEBUG
    size_t count = sizeof(kVrendSupportedVertexFormats) / sizeof(uint32_t);
    for (size_t i = 0; i < count; i++) {
        uint32_t fmt = kVrendSupportedVertexFormats[i];
        if (!vrend_metal_format_enabled(fmt)) {
            continue;
        }
        MTLVertexFormat metal_fmt = vrend_metal_vertex_format_from_pipe(fmt);
        if (metal_fmt == MTLVertexFormatInvalid || vrend_metal_vertex_format_stride(fmt) == 0) {
            NSLog(@"[Metal Backend] Vertex format self-test failed for pipe format %u", fmt);
        }
    }
#endif
}

static MTLVertexDescriptor *vrend_metal_create_vertex_descriptor(struct vrend_metal_context *ctx) {
    if (!ctx->bound_vertex_elements) {
        return nil;
    }

    NSData *stateData = [ctx->vertex_elements objectForKey:@(ctx->bound_vertex_elements)];
    if (!stateData || [stateData length] < sizeof(struct vrend_metal_vertex_elements_state)) {
        return nil;
    }

    const struct vrend_metal_vertex_elements_state *state = stateData.bytes;
    if (!state || state->count == 0) {
        return nil;
    }

    if (state->unsupported_mask) {
        NSLog(@"[Metal Backend] VERTEX_ELEMENTS %u contains unsupported formats (mask=0x%04X)",
              ctx->bound_vertex_elements, state->unsupported_mask);
    }

    MTLVertexDescriptor *vertexDesc = [[MTLVertexDescriptor alloc] init];
    for (uint32_t attr = 0; attr < state->count; attr++) {
        const struct vrend_metal_vertex_element *elem = &state->elements[attr];
        MTLVertexFormat mtlFormat = vrend_metal_vertex_format_from_pipe(elem->format);
        if (mtlFormat == MTLVertexFormatInvalid) {
            NSLog(@"[Metal Backend] Unsupported vertex format %u for attribute %u",
                  elem->format, attr);
            continue;
        }

        if (elem->buffer_index >= 16) {
            NSLog(@"[Metal Backend] Vertex attribute %u references buffer %u (out of range)",
                  attr, elem->buffer_index);
            continue;
        }

        vertexDesc.attributes[attr].format = mtlFormat;
        vertexDesc.attributes[attr].offset = elem->offset;
        vertexDesc.attributes[attr].bufferIndex = elem->buffer_index;

        NSUInteger buffer_index = elem->buffer_index;

        NSUInteger stride = ctx->vertex_buffer_strides[buffer_index];
        if (stride == 0) {
            stride = vrend_metal_vertex_format_stride(elem->format);
        }
        if (stride == 0) {
            stride = sizeof(float) * 4;
        }

        vertexDesc.layouts[buffer_index].stride = stride;
        vertexDesc.layouts[buffer_index].stepFunction = elem->instance_divisor ?
            MTLVertexStepFunctionPerInstance : MTLVertexStepFunctionPerVertex;
        vertexDesc.layouts[buffer_index].stepRate = elem->instance_divisor ? elem->instance_divisor : 1;
    }

    return vertexDesc;
}

static MTLSamplerAddressMode vrend_metal_address_mode_from_wrap(uint32_t wrap) {
    switch (wrap) {
        case VREND_PIPE_TEX_WRAP_REPEAT:
            return MTLSamplerAddressModeRepeat;
        case VREND_PIPE_TEX_WRAP_MIRROR_REPEAT:
            return MTLSamplerAddressModeMirrorRepeat;
        case VREND_PIPE_TEX_WRAP_CLAMP:
        case VREND_PIPE_TEX_WRAP_CLAMP_TO_EDGE:
        case VREND_PIPE_TEX_WRAP_MIRROR_CLAMP:
        case VREND_PIPE_TEX_WRAP_MIRROR_CLAMP_TO_EDGE:
            return MTLSamplerAddressModeClampToEdge;
        case VREND_PIPE_TEX_WRAP_CLAMP_TO_BORDER:
        case VREND_PIPE_TEX_WRAP_MIRROR_CLAMP_TO_BORDER:
            return MTLSamplerAddressModeClampToBorderColor;
        default:
            return MTLSamplerAddressModeClampToEdge;
    }
}

static MTLSamplerMinMagFilter vrend_metal_minmag_from_pipe(uint32_t filter) {
    return (filter == VREND_PIPE_TEX_FILTER_LINEAR) ? MTLSamplerMinMagFilterLinear : MTLSamplerMinMagFilterNearest;
}

static MTLSamplerMipFilter vrend_metal_mip_from_pipe(uint32_t filter) {
    switch (filter) {
        case VREND_PIPE_TEX_MIPFILTER_LINEAR:
            return MTLSamplerMipFilterLinear;
        case VREND_PIPE_TEX_MIPFILTER_NEAREST:
            return MTLSamplerMipFilterNearest;
        default:
            return MTLSamplerMipFilterNotMipmapped;
    }
}

static MTLCompareFunction vrend_metal_compare_from_pipe(uint32_t func) {
    switch (func) {
        case PIPE_FUNC_NEVER: return MTLCompareFunctionNever;
        case PIPE_FUNC_LESS: return MTLCompareFunctionLess;
        case PIPE_FUNC_EQUAL: return MTLCompareFunctionEqual;
        case PIPE_FUNC_LEQUAL: return MTLCompareFunctionLessEqual;
        case PIPE_FUNC_GREATER: return MTLCompareFunctionGreater;
        case PIPE_FUNC_NOTEQUAL: return MTLCompareFunctionNotEqual;
        case PIPE_FUNC_GEQUAL: return MTLCompareFunctionGreaterEqual;
        case PIPE_FUNC_ALWAYS: return MTLCompareFunctionAlways;
        default: return MTLCompareFunctionNever;
    }
}

static bool vrend_metal_wrap_uses_border(uint32_t wrap) {
    return wrap == VREND_PIPE_TEX_WRAP_CLAMP_TO_BORDER || wrap == VREND_PIPE_TEX_WRAP_MIRROR_CLAMP_TO_BORDER;
}

static bool vrend_metal_border_matches(const float color[4], float r, float g, float b, float a) {
    const float epsilon = 0.0001f;
    return fabsf(color[0] - r) < epsilon &&
           fabsf(color[1] - g) < epsilon &&
           fabsf(color[2] - b) < epsilon &&
           fabsf(color[3] - a) < epsilon;
}

static bool vrend_metal_supports_border_color_property(void) {
#if TARGET_OS_OSX
    if (@available(macOS 10.12, *)) {
        return true;
    }
#endif
    return false;
}

static MTLSamplerBorderColor vrend_metal_pick_border_color(const float color[4], bool *exact_match) {
    if (exact_match) {
        *exact_match = true;
    }
    if (vrend_metal_border_matches(color, 0.f, 0.f, 0.f, 0.f)) {
        return MTLSamplerBorderColorTransparentBlack;
    }
    if (vrend_metal_border_matches(color, 0.f, 0.f, 0.f, 1.f)) {
        return MTLSamplerBorderColorOpaqueBlack;
    }
    if (vrend_metal_border_matches(color, 1.f, 1.f, 1.f, 1.f)) {
        return MTLSamplerBorderColorOpaqueWhite;
    }
    if (exact_match) {
        *exact_match = false;
    }
    return MTLSamplerBorderColorOpaqueBlack;
}

static MTLTextureType vrend_metal_texture_type_from_target(uint32_t target, uint32_t slice_count) {
    switch (target) {
        case PIPE_TEXTURE_1D:
            return MTLTextureType1D;
        case PIPE_TEXTURE_1D_ARRAY:
            return MTLTextureType1DArray;
        case PIPE_TEXTURE_2D:
        case PIPE_TEXTURE_RECT:
            return MTLTextureType2D;
        case PIPE_TEXTURE_2D_ARRAY:
            return MTLTextureType2DArray;
        case PIPE_TEXTURE_3D:
            return MTLTextureType3D;
        case PIPE_TEXTURE_CUBE:
            return (slice_count > 6) ? MTLTextureTypeCubeArray : MTLTextureTypeCube;
        default:
            return MTLTextureType2D;
    }
}

static uint32_t vrend_metal_swizzle_component(uint32_t swz) {
#if VREND_METAL_HAS_TEXTURE_SWIZZLE
    switch (swz) {
        case VREND_PIPE_SWIZZLE_X: return MTLTextureSwizzleRed;
        case VREND_PIPE_SWIZZLE_Y: return MTLTextureSwizzleGreen;
        case VREND_PIPE_SWIZZLE_Z: return MTLTextureSwizzleBlue;
        case VREND_PIPE_SWIZZLE_W: return MTLTextureSwizzleAlpha;
        case VREND_PIPE_SWIZZLE_1: return MTLTextureSwizzleOne;
        default: return MTLTextureSwizzleZero;
    }
#else
    (void)swz;
    return 0;
#endif
}

static bool vrend_metal_supports_swizzle_views(void) {
#if VREND_METAL_HAS_TEXTURE_SWIZZLE
    if (@available(macOS 15.0, *)) {
        return true;
    }
#endif
    return false;
}

static void vrend_metal_apply_bound_textures(struct vrend_metal_context *ctx) {
    if (!ctx) {
        return;
    }
    if (ctx->render_encoder) {
        for (uint32_t slot = 0; slot < VREND_MAX_SAMPLERS; slot++) {
            id<MTLTexture> vs_tex = ctx->bound_sampler_views[VREND_METAL_STAGE_VERTEX][slot];
            id<MTLTexture> fs_tex = ctx->bound_sampler_views[VREND_METAL_STAGE_FRAGMENT][slot];
            if (vs_tex) {
                [ctx->render_encoder setVertexTexture:vs_tex atIndex:slot];
            }
            if (fs_tex) {
                [ctx->render_encoder setFragmentTexture:fs_tex atIndex:slot];
            }
        }
    }
    if (ctx->compute_encoder) {
        for (uint32_t slot = 0; slot < VREND_MAX_SAMPLERS; slot++) {
            id<MTLTexture> tex = ctx->bound_sampler_views[VREND_METAL_STAGE_COMPUTE][slot];
            if (tex) {
                [ctx->compute_encoder setTexture:tex atIndex:slot];
            }
        }
    }
}

void vrend_metal_apply_bound_samplers(struct vrend_metal_context *ctx) {
    if (!ctx) {
        return;
    }
    if (ctx->render_encoder) {
        for (uint32_t slot = 0; slot < VREND_MAX_SAMPLERS; slot++) {
            id<MTLSamplerState> vs_sampler = ctx->bound_sampler_states[VREND_METAL_STAGE_VERTEX][slot];
            id<MTLSamplerState> fs_sampler = ctx->bound_sampler_states[VREND_METAL_STAGE_FRAGMENT][slot];
            if (vs_sampler) {
                [ctx->render_encoder setVertexSamplerState:vs_sampler atIndex:slot];
            }
            if (fs_sampler) {
                [ctx->render_encoder setFragmentSamplerState:fs_sampler atIndex:slot];
            }
        }
    }
    if (ctx->compute_encoder) {
        for (uint32_t slot = 0; slot < VREND_MAX_SAMPLERS; slot++) {
            id<MTLSamplerState> sampler = ctx->bound_sampler_states[VREND_METAL_STAGE_COMPUTE][slot];
            if (sampler) {
                [ctx->compute_encoder setSamplerState:sampler atIndex:slot];
            }
        }
    }
}

static id<MTLSamplerState> vrend_metal_compile_sampler_state(struct vrend_metal_context *ctx,
                                                             uint32_t handle) {
    if (!ctx || !ctx->sampler_states) {
        return nil;
    }
    NSData *payload = ctx->sampler_states[@(handle)];
    if (!payload || [payload length] < sizeof(struct vrend_metal_sampler_state_desc)) {
        NSLog(@"[Metal Backend] Sampler state %u payload missing", handle);
        return nil;
    }
    struct vrend_metal_sampler_state_desc desc;
    memcpy(&desc, [payload bytes], sizeof(desc));

    MTLSamplerDescriptor *descriptor = [[MTLSamplerDescriptor alloc] init];
    descriptor.minFilter = vrend_metal_minmag_from_pipe(desc.min_img_filter);
    descriptor.magFilter = vrend_metal_minmag_from_pipe(desc.mag_img_filter);
    descriptor.mipFilter = vrend_metal_mip_from_pipe(desc.min_mip_filter);
    descriptor.sAddressMode = vrend_metal_address_mode_from_wrap(desc.wrap_s);
    descriptor.tAddressMode = vrend_metal_address_mode_from_wrap(desc.wrap_t);
    descriptor.rAddressMode = vrend_metal_address_mode_from_wrap(desc.wrap_r);
    descriptor.normalizedCoordinates = desc.normalized_coords ? YES : NO;
    descriptor.lodMinClamp = desc.min_lod;
    descriptor.lodMaxClamp = desc.max_lod;
    uint32_t aniso = desc.max_anisotropy ? desc.max_anisotropy : 1;
    if (aniso > 16) {
        aniso = 16;
    }
    descriptor.maxAnisotropy = aniso;

    if (desc.compare_mode == VREND_PIPE_TEX_COMPARE_R_TO_TEXTURE) {
        descriptor.compareFunction = vrend_metal_compare_from_pipe(desc.compare_func);
    } else {
        descriptor.compareFunction = MTLCompareFunctionAlways;
    }

    bool wants_border = vrend_metal_wrap_uses_border(desc.wrap_s) ||
                        vrend_metal_wrap_uses_border(desc.wrap_t) ||
                        vrend_metal_wrap_uses_border(desc.wrap_r);
    if (wants_border && vrend_metal_supports_border_color_property()) {
#if TARGET_OS_OSX
        bool exact_match = true;
        MTLSamplerBorderColor chosen = vrend_metal_pick_border_color(desc.border_color, &exact_match);
        descriptor.borderColor = chosen;
        if (!exact_match) {
            NSLog(@"[Metal Backend] Sampler %u border color approximated (limited Metal palette)", handle);
        }
#endif
    } else if (wants_border) {
        NSLog(@"[Metal Backend] Sampler %u requested clamp-to-border but host macOS lacks API support", handle);
    }

    id<MTLSamplerState> sampler = [g_metal_device newSamplerStateWithDescriptor:descriptor];
    if (!sampler) {
        NSLog(@"[Metal Backend] Failed to compile sampler state %u", handle);
        return nil;
    }
    if (!ctx->sampler_state_objects) {
        ctx->sampler_state_objects = [[NSMutableDictionary alloc] init];
    }
    ctx->sampler_state_objects[@(handle)] = sampler;
    return sampler;
}

void vrend_metal_bind_sampler_state_slot(struct vrend_metal_context *ctx,
                                         uint32_t stage,
                                         uint32_t slot,
                                         uint32_t handle) {
    if (!ctx || stage >= VREND_METAL_STAGE_COUNT || slot >= VREND_MAX_SAMPLERS) {
        return;
    }
    if (handle == 0) {
        ctx->bound_sampler_states[stage][slot] = nil;
        ctx->sampler_state_handles[stage][slot] = 0;
        if (ctx->render_encoder) {
            if (stage == VREND_METAL_STAGE_VERTEX) {
                [ctx->render_encoder setVertexSamplerState:nil atIndex:slot];
            } else if (stage == VREND_METAL_STAGE_FRAGMENT) {
                [ctx->render_encoder setFragmentSamplerState:nil atIndex:slot];
            }
        }
        if (stage == VREND_METAL_STAGE_COMPUTE && ctx->compute_encoder) {
            [ctx->compute_encoder setSamplerState:nil atIndex:slot];
        }
        return;
    }
    id<MTLSamplerState> sampler = ctx->sampler_state_objects[@(handle)];
    if (!sampler) {
        sampler = vrend_metal_compile_sampler_state(ctx, handle);
    }
    if (!sampler) {
        return;
    }
    ctx->bound_sampler_states[stage][slot] = sampler;
    ctx->sampler_state_handles[stage][slot] = handle;
    if (ctx->render_encoder) {
        if (stage == VREND_METAL_STAGE_VERTEX) {
            [ctx->render_encoder setVertexSamplerState:sampler atIndex:slot];
        } else if (stage == VREND_METAL_STAGE_FRAGMENT) {
            [ctx->render_encoder setFragmentSamplerState:sampler atIndex:slot];
        }
    }
    if (stage == VREND_METAL_STAGE_COMPUTE && ctx->compute_encoder) {
        [ctx->compute_encoder setSamplerState:sampler atIndex:slot];
    }
}

static id<MTLTexture> vrend_metal_create_texture_view(struct vrend_metal_context *ctx,
                                                      const struct vrend_metal_sampler_view_desc *desc) {
    id<MTLTexture> texture = [ctx->metal_textures objectForKey:@(desc->resource_id)];
    if (!texture) {
        texture = [g_metal_global_textures objectForKey:@(desc->resource_id)];
    }
    if (!texture) {
        NSLog(@"[Metal Backend] Sampler view resource %u missing", desc->resource_id);
        return nil;
    }
#if VREND_METAL_HAS_TEXTURE_SWIZZLE
    if (vrend_metal_supports_swizzle_views() && [texture respondsToSelector:@selector(newTextureViewWithDescriptor:)]) {
        @autoreleasepool {
            id viewDesc = [[NSClassFromString(@"MTLTextureViewDescriptor") alloc] init];
            if (!viewDesc) {
                goto fallback_view;
            }
            uint32_t slice_count = (desc->last_layer >= desc->first_layer) ?
                (desc->last_layer - desc->first_layer + 1) : 1;
            @try {
                [viewDesc setValue:@(vrend_metal_texture_type_from_target(desc->target, slice_count)) forKey:@"textureType"];
                [viewDesc setValue:@([texture pixelFormat]) forKey:@"pixelFormat"];
                [viewDesc setValue:@(MAX(1u, (uint32_t)([texture width] >> desc->first_level))) forKey:@"width"];
                [viewDesc setValue:@(MAX(1u, (uint32_t)([texture height] >> desc->first_level))) forKey:@"height"];
                [viewDesc setValue:@(MAX(1u, (uint32_t)([texture depth] >> desc->first_level))) forKey:@"depth"];
                [viewDesc setValue:@((desc->last_level >= desc->first_level) ?
                                     (desc->last_level - desc->first_level + 1) : 1)
                             forKey:@"mipmapLevelCount"];
                [viewDesc setValue:@(slice_count) forKey:@"arrayLength"];
                [viewDesc setValue:@(texture.resourceOptions) forKey:@"resourceOptions"];
                [viewDesc setValue:@(texture.storageMode) forKey:@"storageMode"];
                [viewDesc setValue:@(texture.cpuCacheMode) forKey:@"cpuCacheMode"];
                [viewDesc setValue:@(texture.usage) forKey:@"usage"];
                MTLTextureSwizzleChannels channels;
                channels.red = (MTLTextureSwizzle)vrend_metal_swizzle_component(desc->swizzle_r);
                channels.green = (MTLTextureSwizzle)vrend_metal_swizzle_component(desc->swizzle_g);
                channels.blue = (MTLTextureSwizzle)vrend_metal_swizzle_component(desc->swizzle_b);
                channels.alpha = (MTLTextureSwizzle)vrend_metal_swizzle_component(desc->swizzle_a);
                NSValue *swizzleValue = [NSValue valueWithBytes:&channels objCType:@encode(MTLTextureSwizzleChannels)];
                [viewDesc setValue:swizzleValue forKey:@"swizzle"];
            } @catch (NSException *exception) {
                NSLog(@"[Metal Backend] Failed to configure texture view descriptor for swizzle: %@", exception);
                goto fallback_view;
            }

            id<MTLTexture> view = nil;
            @try {
                SEL newViewSel = @selector(newTextureViewWithDescriptor:);
                id<MTLTexture> (*NewTextureViewWithDescriptor)(id, SEL, id) = (id<MTLTexture> (*)(id, SEL, id))objc_msgSend;
                view = NewTextureViewWithDescriptor(texture, newViewSel, viewDesc);
            } @catch (NSException *exception) {
                NSLog(@"[Metal Backend] newTextureViewWithDescriptor: unavailable (%@)", exception);
            }
            if (view) {
                return view;
            }
        }
    }
#else
    (void)ctx;
    (void)desc;
#endif
fallback_view:
    if ([texture respondsToSelector:@selector(newTextureViewWithPixelFormat:)]) {
        return [texture newTextureViewWithPixelFormat:[texture pixelFormat]];
    }
    return texture;
}

void vrend_metal_bind_sampler_view_slot(struct vrend_metal_context *ctx,
                                        uint32_t stage,
                                        uint32_t slot,
                                        uint32_t handle) {
    if (!ctx || stage >= VREND_METAL_STAGE_COUNT || slot >= VREND_MAX_SAMPLERS) {
        return;
    }
    if (handle == 0) {
        ctx->bound_sampler_views[stage][slot] = nil;
        ctx->sampler_view_handles[stage][slot] = 0;
        if (ctx->render_encoder) {
            if (stage == VREND_METAL_STAGE_VERTEX) {
                [ctx->render_encoder setVertexTexture:nil atIndex:slot];
            } else if (stage == VREND_METAL_STAGE_FRAGMENT) {
                [ctx->render_encoder setFragmentTexture:nil atIndex:slot];
            }
        }
        if (stage == VREND_METAL_STAGE_COMPUTE && ctx->compute_encoder) {
            [ctx->compute_encoder setTexture:nil atIndex:slot];
        }
        return;
    }
    if (!ctx->sampler_views) {
        NSLog(@"[Metal Backend] No sampler views dictionary for binding %u", handle);
        return;
    }
    NSData *payload = ctx->sampler_views[@(handle)];
    if (!payload || [payload length] < sizeof(struct vrend_metal_sampler_view_desc)) {
        NSLog(@"[Metal Backend] Sampler view %u payload missing", handle);
        return;
    }
    struct vrend_metal_sampler_view_desc desc;
    memcpy(&desc, [payload bytes], sizeof(desc));
    id<MTLTexture> view = vrend_metal_create_texture_view(ctx, &desc);
    if (!view) {
        return;
    }
    ctx->bound_sampler_views[stage][slot] = view;
    ctx->sampler_view_handles[stage][slot] = handle;
    if (ctx->render_encoder) {
        if (stage == VREND_METAL_STAGE_VERTEX) {
            [ctx->render_encoder setVertexTexture:view atIndex:slot];
        } else if (stage == VREND_METAL_STAGE_FRAGMENT) {
            [ctx->render_encoder setFragmentTexture:view atIndex:slot];
        }
    }
    if (stage == VREND_METAL_STAGE_COMPUTE && ctx->compute_encoder) {
        [ctx->compute_encoder setTexture:view atIndex:slot];
    }
}

static id<MTLComputePipelineState> vrend_metal_acquire_compute_pipeline(struct vrend_metal_context *ctx) {
    if (!ctx || ctx->bound_compute_shader == 0) {
        NSLog(@"[Metal Backend] No compute shader bound for dispatch");
        return nil;
    }

    if (!ctx->metal_compute_pipelines) {
        ctx->metal_compute_pipelines = [[NSMutableDictionary alloc] init];
    }

    NSNumber *key = @(ctx->bound_compute_shader);
    id<MTLComputePipelineState> pipeline = ctx->metal_compute_pipelines[key];
    if (pipeline) {
        return pipeline;
    }

    id<MTLFunction> function = ctx->metal_shaders[key];
    if (!function) {
        NSLog(@"[Metal Backend] Compute shader %u missing Metal function", ctx->bound_compute_shader);
        return nil;
    }

    NSError *error = nil;
    pipeline = [g_metal_device newComputePipelineStateWithFunction:function error:&error];
    if (!pipeline) {
        NSLog(@"[Metal Backend] Failed to build compute pipeline for shader %u: %@",
              ctx->bound_compute_shader, error);
        return nil;
    }

    ctx->metal_compute_pipelines[key] = pipeline;
    return pipeline;
}

void vrend_metal_register_scanout_callback(
    vrend_metal_scanout_callback callback,
    void *userdata) {
    g_scanout_callback = callback;
    g_scanout_callback_user = userdata;
}

bool vrend_metal_scanout_supports_iosurface(void) {
    return true;
}

int vrend_metal_get_scanout_iosurface(
    uint32_t scanout_id,
    struct vrend_metal_scanout_surface_info *info) {
    if (!info || scanout_id >= VREND_MAX_SCANOUTS) {
        return -1;
    }
    IOSurfaceRef surface = g_scanout_iosurfaces[scanout_id];
    if (!surface) {
        return -1;
    }
    memset(info, 0, sizeof(*info));
    info->scanout_id = scanout_id;
    info->ctx_id = g_scanout_ctx_ids[scanout_id];
    info->width = (uint32_t)IOSurfaceGetWidth(surface);
    info->height = (uint32_t)IOSurfaceGetHeight(surface);
    info->bytes_per_row = (uint32_t)IOSurfaceGetBytesPerRow(surface);
    info->pixel_format_fourcc = IOSurfaceGetPixelFormat(surface);
    info->iosurface_id = IOSurfaceGetID(surface);
    if (g_scanout_iosurface_textures[scanout_id]) {
        info->metal_pixel_format = (uint32_t)[g_scanout_iosurface_textures[scanout_id] pixelFormat];
    }
    info->frame_id = g_scanout_frame_serials[scanout_id];
    return 0;
}

/* Internal helpers declared in vrend_metal_priv.h */

/* Backend initialization */
int vrend_metal_init(uint32_t flags) {
    if (g_metal_initialized) {
        return 0;
    }
    
    @autoreleasepool {
        // Create Metal device
        g_metal_device = MTLCreateSystemDefaultDevice();
        if (!g_metal_device) {
            NSLog(@"[Metal Backend] Failed to create Metal device");
            return -1;
        }
        
        // Create command queue
        g_metal_queue = [g_metal_device newCommandQueue];
        if (!g_metal_queue) {
            NSLog(@"[Metal Backend] Failed to create command queue");
            g_metal_device = nil;
            return -1;
        }
        
        g_metal_initialized = true;
        g_metal_global_textures = [[NSMutableDictionary alloc] init];
        for (int i = 0; i < VREND_MAX_SCANOUTS; i++) {
            g_scanout_textures[i] = nil;
            g_scanout_region_valid[i] = false;
            g_scanout_staging[i] = nil;
            g_scanout_row_bytes[i] = 0;
            g_scanout_width[i] = 0;
            g_scanout_height[i] = 0;
            g_scanout_format[i] = MTLPixelFormatInvalid;
            g_scanout_bytes_per_pixel[i] = 0;
            g_scanout_ctx_ids[i] = 0;
            vrend_metal_release_scanout_iosurface(i);
            g_scanout_frame_serials[i] = 0;
            g_scanout_throttle_interval[i] = 0;
            g_scanout_last_dispatch_time[i] = 0;
        }
        g_context_registry = nil;
        g_context_registry_lock = nil;
        g_shared_event_listener = NULL;
        g_shared_event_userdata = NULL;
        
        NSLog(@"[Metal Backend] Initialized successfully");
        NSLog(@"[Metal Backend] Device: %@", [g_metal_device name]);
        NSUInteger maxSize = 16384;  /* Safe default for all Metal devices */
        NSLog(@"[Metal Backend] Max texture size: %lu", (unsigned long)maxSize);

        vrend_metal_self_test_vertex_formats();
    }
    
    return 0;
}

void vrend_metal_cleanup(void) {
    if (!g_metal_initialized) {
        return;
    }
    
    @autoreleasepool {
        g_metal_queue = nil;
        g_metal_device = nil;
        g_metal_global_textures = nil;
        for (int i = 0; i < VREND_MAX_SCANOUTS; i++) {
            g_scanout_textures[i] = nil;
            g_scanout_region_valid[i] = false;
            g_scanout_staging[i] = nil;
            g_scanout_row_bytes[i] = 0;
            g_scanout_width[i] = 0;
            g_scanout_height[i] = 0;
            g_scanout_format[i] = MTLPixelFormatInvalid;
            g_scanout_bytes_per_pixel[i] = 0;
            g_scanout_ctx_ids[i] = 0;
            g_scanout_iosurfaces[i] = NULL;
            g_scanout_iosurface_textures[i] = nil;
            g_scanout_frame_serials[i] = 0;
            g_scanout_throttle_interval[i] = 0;
            g_scanout_last_dispatch_time[i] = 0;
        }
        g_scanout_frame_counter = 0;
        g_metal_initialized = false;
        g_context_registry = nil;
        g_context_registry_lock = nil;
        g_shared_event_listener = NULL;
        g_shared_event_userdata = NULL;
        
        NSLog(@"[Metal Backend] Cleaned up");
    }
}

/* Accessor for Metal device (used by shader module) */
id<MTLDevice> vrend_metal_get_device(void) {
    return g_metal_device;
}

void vrend_metal_get_caps(struct virgl_metal_caps *caps) {
    if (!g_metal_device) {
        memset(caps, 0, sizeof(*caps));
        return;
    }
    
    @autoreleasepool {
        memset(caps, 0, sizeof(*caps));
        
        caps->metal_version = 2;  /* Metal 2 */
        caps->max_texture_size = 16384;  /* All modern Metal devices support at least 16K */
        caps->max_texture_layers = 2048;
        caps->max_buffer_size = 256 * 1024 * 1024;  /* 256 MB - safe minimum */
        
        caps->supports_tessellation = 1;
        caps->supports_argument_buffers = 1;
        caps->supports_indirect_command_buffers = 1;
        caps->supports_depth_clip_mode = 1;
        caps->max_threads_per_threadgroup = (uint32_t)[g_metal_device maxThreadsPerThreadgroup].width;
    }
}

/* Context management */
struct virgl_context* vrend_metal_create_context(
    uint32_t ctx_id,
    uint32_t nlen,
    const char *debug_name) {
    
    if (!g_metal_initialized) {
        NSLog(@"[Metal Backend] Not initialized");
        return NULL;
    }
    
    @autoreleasepool {
        struct vrend_metal_context *ctx = calloc(1, sizeof(*ctx));
        if (!ctx) {
            return NULL;
        }
        
        ctx->ctx_id = ctx_id;
        if (debug_name && nlen > 0) {
            snprintf(ctx->debug_name, sizeof(ctx->debug_name), "%.*s", 
                    (int)nlen, debug_name);
        } else {
            snprintf(ctx->debug_name, sizeof(ctx->debug_name), "ctx_%u", ctx_id);
        }
        
        // Initialize Metal storage dictionaries
        ctx->metal_shaders = [[NSMutableDictionary alloc] init];
        ctx->metal_pipelines = [[NSMutableDictionary alloc] init];
        ctx->metal_depth_states = [[NSMutableDictionary alloc] init];
        ctx->metal_compute_pipelines = [[NSMutableDictionary alloc] init];
        ctx->metal_textures = [[NSMutableDictionary alloc] init];
        ctx->metal_buffers = [[NSMutableDictionary alloc] init];
        ctx->blend_states = [[NSMutableDictionary alloc] init];
        ctx->depth_states = [[NSMutableDictionary alloc] init];
        ctx->vertex_elements = [[NSMutableDictionary alloc] init];
        ctx->sampler_states = [[NSMutableDictionary alloc] init];
        ctx->sampler_state_objects = [[NSMutableDictionary alloc] init];
        ctx->sampler_views = [[NSMutableDictionary alloc] init];
        ctx->streamout_targets = [[NSMutableDictionary alloc] init];
        ctx->fence_lock = [[NSLock alloc] init];
        ctx->fence_command_buffers = [[NSMutableDictionary alloc] init];
        ctx->fence_serial = 0;
        ctx->fence_event_values = [[NSMutableDictionary alloc] init];
#if TARGET_OS_OSX
        if (@available(macOS 10.14, *)) {
            if ([g_metal_device respondsToSelector:@selector(newSharedEvent)]) {
                ctx->shared_event = [g_metal_device newSharedEvent];
                if (!ctx->shared_event) {
                    NSLog(@"[Metal Backend] Shared events unavailable: device failed to vend MTLSharedEvent (ctx %u)", ctx_id);
                } else {
                    ctx->shared_event_handle = [ctx->shared_event newSharedEventHandle];
                    ctx->shared_event_port = vrend_metal_shared_event_handle_port(ctx->shared_event_handle);
                    if (ctx->shared_event_port == MACH_PORT_NULL) {
                        NSLog(@"[Metal Backend] Shared events available via encoded handle only (ctx %u)", ctx_id);
                    }
                    ctx->shared_event_value = 1;
                }
            } else {
                NSLog(@"[Metal Backend] Shared events unavailable: device missing newSharedEvent selector (ctx %u)", ctx_id);
            }
        } else {
            NSLog(@"[Metal Backend] Shared events require macOS 10.14 or newer (ctx %u)", ctx_id);
        }
#endif
        
        // Initialize state
        ctx->framebuffer_color_count = 0;
        ctx->vertex_buffer_count = 0;
        ctx->index_buffer = nil;
        ctx->index_buffer_handle = 0;
        ctx->index_buffer_offset = 0;
        ctx->index_buffer_stride = 0;
        ctx->index_buffer_type = MTLIndexTypeUInt16;
        ctx->viewport = (MTLViewport){0, 0, 800, 600, 0, 1};
        ctx->bound_vertex_shader = 0;
        ctx->bound_fragment_shader = 0;
        ctx->bound_geometry_shader = 0;
        ctx->bound_tess_ctrl_shader = 0;
        ctx->bound_tess_eval_shader = 0;
        ctx->bound_compute_shader = 0;
        ctx->tess_state.patch_vertices = 0;
        ctx->tess_state.valid = false;
        ctx->tess_state.dirty = false;
        memset(ctx->tess_state.default_outer_level, 0, sizeof(ctx->tess_state.default_outer_level));
        memset(ctx->tess_state.default_inner_level, 0, sizeof(ctx->tess_state.default_inner_level));
        ctx->bound_blend_state = 0;
        ctx->bound_depth_state = 0;
        ctx->bound_vertex_elements = 0;
        ctx->current_compute_pipeline = 0;
        ctx->scanout_texture = nil;
        memset(ctx->clip_state.planes, 0, sizeof(ctx->clip_state.planes));
        ctx->clip_state.enabled_mask = 0;
        ctx->clip_state.dirty = false;
        ctx->streamout_append_mask = 0;
        for (int i = 0; i < VREND_MAX_STREAMOUT_TARGETS; i++) {
            ctx->streamout_bindings[i].buffer = nil;
            ctx->streamout_bindings[i].resource_handle = 0;
            ctx->streamout_bindings[i].handle = 0;
            ctx->streamout_bindings[i].offset = 0;
            ctx->streamout_bindings[i].size = 0;
            ctx->streamout_bindings[i].append = false;
        }
        for (int stage = 0; stage < VREND_METAL_STAGE_COUNT; stage++) {
            for (int i = 0; i < VREND_MAX_SAMPLERS; i++) {
                ctx->bound_sampler_states[stage][i] = nil;
                ctx->sampler_state_handles[stage][i] = 0;
                ctx->bound_sampler_views[stage][i] = nil;
                ctx->sampler_view_handles[stage][i] = 0;
            }
        }
        for (int stage = 0; stage < VREND_METAL_STAGE_COUNT; stage++) {
            for (int i = 0; i < VREND_MAX_CONST_BUFFERS; i++) {
                ctx->constant_buffers[stage][i].buffer = nil;
                ctx->constant_buffers[stage][i].offset = 0;
                ctx->constant_buffers[stage][i].length = 0;
            }
            for (int slot = 0; slot < VREND_MAX_SHADER_BUFFERS; slot++) {
                ctx->shader_buffers[stage][slot].buffer = nil;
                ctx->shader_buffers[stage][slot].offset = 0;
                ctx->shader_buffers[stage][slot].length = 0;
            }
        }
        
        // Create initial command buffer (ARC manages retention)
        ctx->command_buffer = [g_metal_queue commandBuffer];
        
        NSLog(@"[Metal Backend] Created context %u: %s", ctx_id, ctx->debug_name);
        vrend_metal_register_context(ctx);
        vrend_metal_emit_shared_event_info(ctx, MACH_PORT_NULL);
        
        return (struct virgl_context*)ctx;
    }
}

void vrend_metal_destroy_context(struct virgl_context *vctx) {
    struct vrend_metal_context *ctx = (struct vrend_metal_context*)vctx;
    
    if (!ctx) {
        return;
    }
    
    @autoreleasepool {
        NSLog(@"[Metal Backend] Destroying context %u", ctx->ctx_id);
        vrend_metal_unregister_context(ctx);
        
        // End any active encoders
        if (ctx->render_encoder) {
            [ctx->render_encoder endEncoding];
            ctx->render_encoder = nil;
        }
        if (ctx->compute_encoder) {
            [ctx->compute_encoder endEncoding];
            ctx->compute_encoder = nil;
            ctx->current_compute_pipeline = 0;
        }
        
        // Commit command buffer (ARC will release)
        if (ctx->command_buffer) {
            [ctx->command_buffer commit];
            ctx->command_buffer = nil;
        }
        
            // Flush pending fence command buffers before releasing
            if (ctx->fence_command_buffers && ctx->fence_command_buffers.count > 0) {
                [ctx->fence_lock lock];
                NSArray *pendingFences = [ctx->fence_command_buffers allValues];
                [ctx->fence_command_buffers removeAllObjects];
                [ctx->fence_lock unlock];
                for (id<MTLCommandBuffer> cb in pendingFences) {
                    [cb waitUntilCompleted];
                }
            }

            // Release Metal objects
            ctx->metal_shaders = nil;
            ctx->metal_pipelines = nil;
            ctx->metal_depth_states = nil;
            ctx->metal_compute_pipelines = nil;
            ctx->metal_textures = nil;
            ctx->metal_buffers = nil;
            ctx->vertex_elements = nil;
            ctx->sampler_states = nil;
            ctx->sampler_state_objects = nil;
            ctx->sampler_views = nil;
            ctx->streamout_targets = nil;
            ctx->fence_command_buffers = nil;
            ctx->fence_event_values = nil;
            ctx->fence_lock = nil;
            ctx->shared_event = nil;
            ctx->shared_event_handle = nil;
            ctx->shared_event_port = MACH_PORT_NULL;
        
        free(ctx);
    }
}

/* Resource creation - PIPE_FORMAT to Metal format mapping */
static MTLPixelFormat pipe_format_to_metal(uint32_t pipe_format) {
    switch (pipe_format) {
        /* Float formats */
        case 0:   /* PIPE_FORMAT_R32_FLOAT */
            return MTLPixelFormatR32Float;
        case 1:   /* PIPE_FORMAT_R32G32_FLOAT */
            return MTLPixelFormatRG32Float;
        case 3:   /* PIPE_FORMAT_R32G32B32A32_FLOAT */
            return MTLPixelFormatRGBA32Float;
        case 9:   /* PIPE_FORMAT_R16G16_FLOAT */
            return MTLPixelFormatRG16Float;
        case 10:  /* PIPE_FORMAT_R16G16B16A16_FLOAT */
            return MTLPixelFormatRGBA16Float;
        case 21:  /* PIPE_FORMAT_R11G11B10_FLOAT */
            return MTLPixelFormatRG11B10Float;
            
        /* 8-bit normalized formats */
        case 4:   /* PIPE_FORMAT_R8G8B8A8_UNORM */
            return MTLPixelFormatRGBA8Unorm;
        case 5:   /* PIPE_FORMAT_B8G8R8A8_UNORM */
            return MTLPixelFormatBGRA8Unorm;
        case 6:   /* PIPE_FORMAT_R8G8B8A8_SNORM */
            return MTLPixelFormatRGBA8Snorm;
            
        /* 8-bit integer formats */
        case 7:   /* PIPE_FORMAT_R8G8B8A8_UINT */
            return MTLPixelFormatRGBA8Uint;
        case 8:   /* PIPE_FORMAT_R8G8B8A8_SINT */
            return MTLPixelFormatRGBA8Sint;
            
        /* 16-bit normalized formats */
        case 15:  /* PIPE_FORMAT_R16G16_UNORM */
            return MTLPixelFormatRG16Unorm;
        case 16:  /* PIPE_FORMAT_R16G16_SNORM */
            return MTLPixelFormatRG16Snorm;
        case 17:  /* PIPE_FORMAT_R16G16B16A16_UNORM */
            return MTLPixelFormatRGBA16Unorm;
        case 18:  /* PIPE_FORMAT_R16G16B16A16_SNORM */
            return MTLPixelFormatRGBA16Snorm;
            
        /* 16-bit integer formats */
        case 11:  /* PIPE_FORMAT_R16G16_UINT */
            return MTLPixelFormatRG16Uint;
        case 12:  /* PIPE_FORMAT_R16G16_SINT */
            return MTLPixelFormatRG16Sint;
        case 13:  /* PIPE_FORMAT_R16G16B16A16_UINT */
            return MTLPixelFormatRGBA16Uint;
        case 14:  /* PIPE_FORMAT_R16G16B16A16_SINT */
            return MTLPixelFormatRGBA16Sint;
            
        /* 10-bit formats */
        case 19:  /* PIPE_FORMAT_R10G10B10A2_UNORM */
            return MTLPixelFormatRGB10A2Unorm;
        case 20:  /* PIPE_FORMAT_R10G10B10A2_UINT */
            return MTLPixelFormatRGB10A2Uint;
            
        /* 32-bit integer formats */
        case 22:  /* PIPE_FORMAT_R32G32_UINT */
            return MTLPixelFormatRG32Uint;
        case 23:  /* PIPE_FORMAT_R32G32B32A32_UINT */
            return MTLPixelFormatRGBA32Uint;
        case 24:  /* PIPE_FORMAT_R32G32_SINT */
            return MTLPixelFormatRG32Sint;
        case 25:  /* PIPE_FORMAT_R32G32B32A32_SINT */
            return MTLPixelFormatRGBA32Sint;
            
        default:
            NSLog(@"[Metal Backend] Unsupported PIPE_FORMAT %u, defaulting to BGRA8", pipe_format);
            return MTLPixelFormatBGRA8Unorm;
    }
}

int vrend_metal_create_resource(
    struct virgl_context *vctx,
    struct virgl_resource *res) {
    
    struct vrend_metal_context *ctx = (struct vrend_metal_context*)vctx;
    
    @autoreleasepool {
        uint32_t resource_handle = res->resource_id ? res->resource_id : res->res_id;
        const struct pipe_resource *pipe_res = res->pipe_resource;

        // Check if this is a buffer resource (bind flags indicate vertex/index/constant buffer)
        // virgl bind flags: 0x2 = VERTEX, 0x4 = INDEX, 0x80 = CONSTANT
        uint32_t bind_flags = pipe_res ? pipe_res->bind : res->bind;
        bool is_buffer = (bind_flags & 0x86) != 0;
        
        if (is_buffer || res->target == 0) {  // target==0 often means buffer
            // Create buffer resource
            NSUInteger buffer_size = res->width;
            if (buffer_size == 0) buffer_size = 4096; // Minimum size
            
            id<MTLBuffer> buffer = [g_metal_device newBufferWithLength:buffer_size
                                                               options:MTLResourceStorageModeShared];
            if (!buffer) {
                NSLog(@"[Metal Backend] Failed to create buffer resource %u", resource_handle);
                return -1;
            }
            
            [ctx->metal_buffers setObject:buffer forKey:@(resource_handle)];
            
            NSLog(@"[Metal Backend] Created buffer resource %u: %lu bytes (bind=0x%x)",
                  resource_handle, (unsigned long)buffer_size, bind_flags);
            
            return 0;
        }
        
        // Create texture descriptor
        MTLTextureDescriptor *desc = [MTLTextureDescriptor new];
        uint32_t tex_width = pipe_res ? pipe_res->width0 : res->width;
        uint32_t tex_height = pipe_res ? pipe_res->height0 : res->height;
        uint32_t tex_depth = pipe_res ? pipe_res->depth0 : res->depth;
        uint32_t tex_format = pipe_res ? pipe_res->format : res->format;

        desc.width = tex_width ? tex_width : 1;
        desc.height = tex_height ? tex_height : 1;
        desc.depth = tex_depth ? tex_depth : 1;
        desc.pixelFormat = pipe_format_to_metal(tex_format);
        desc.usage = MTLTextureUsageShaderRead | MTLTextureUsageRenderTarget;
        desc.storageMode = MTLStorageModePrivate;
        
        // Set texture type based on dimensions
        if (desc.depth > 1) {
            desc.textureType = MTLTextureType3D;
        } else if (desc.height > 1) {
            desc.textureType = MTLTextureType2D;
        } else {
            desc.textureType = MTLTextureType1D;
        }
        
        // Create texture
        id<MTLTexture> texture = [g_metal_device newTextureWithDescriptor:desc];
        if (!texture) {
            NSLog(@"[Metal Backend] Failed to create texture resource %u", resource_handle);
            return -1;
        }
        
        // Store in context
        NSNumber *resourceKey = @(resource_handle);
        [ctx->metal_textures setObject:texture 
                    forKey:resourceKey];
        [g_metal_global_textures setObject:texture forKey:resourceKey];
        
          NSLog(@"[Metal Backend] Created texture resource %u: %lux%lux%lu format=%u",
              resource_handle,
              (unsigned long)desc.width,
              (unsigned long)desc.height,
              (unsigned long)desc.depth,
              tex_format);
        
        return 0;
    }
}

void vrend_metal_destroy_resource(
    struct virgl_context *vctx,
    struct virgl_resource *res) {
    
    struct vrend_metal_context *ctx = (struct vrend_metal_context*)vctx;
    
    @autoreleasepool {
        uint32_t resource_handle = res->resource_id ? res->resource_id : res->res_id;
        NSNumber *resourceKey = @(resource_handle);
        [ctx->metal_textures removeObjectForKey:resourceKey];
        [g_metal_global_textures removeObjectForKey:resourceKey];
        [ctx->metal_buffers removeObjectForKey:resourceKey];
        
        NSLog(@"[Metal Backend] Destroyed resource %u", resource_handle);
    }
}

/* Data transfer - zero-copy via shared memory */
int vrend_metal_transfer_inline_write(
    struct virgl_context *vctx,
    struct virgl_resource *res,
    const struct virgl_box *box,
    const void *data,
    uint32_t stride,
    uint32_t layer_stride) {
    
    struct vrend_metal_context *ctx = (struct vrend_metal_context*)vctx;
    
    @autoreleasepool {
        // Try buffer first
        id<MTLBuffer> buffer = [ctx->metal_buffers objectForKey:@(res->resource_id)];
        if (buffer) {
            // Write to buffer
            NSUInteger offset = box->x;
            NSUInteger size = box->w;
            
            if (offset + size > [buffer length]) {
                NSLog(@"[Metal Backend] Buffer write out of bounds: offset=%lu size=%lu buffer_len=%lu",
                      (unsigned long)offset, (unsigned long)size, (unsigned long)[buffer length]);
                return -1;
            }
            
            memcpy((uint8_t*)[buffer contents] + offset, data, size);
            NSLog(@"[Metal Backend] Wrote %u bytes to buffer %u at offset %u",
                  box->w, res->resource_id, box->x);
            
            return 0;
        }
        
        // Try texture
        id<MTLTexture> texture = [ctx->metal_textures objectForKey:@(res->resource_id)];
        if (!texture) {
            NSLog(@"[Metal Backend] Resource %u not found for transfer", res->resource_id);
            return -1;
        }
        
        MTLRegion region = MTLRegionMake2D(box->x, box->y, box->w, box->h);
        
        [texture replaceRegion:region
                   mipmapLevel:0
                     withBytes:data
                   bytesPerRow:stride];
        
        NSLog(@"[Metal Backend] Wrote texture %u region (%u,%u %ux%u)",
              res->resource_id, box->x, box->y, box->w, box->h);
        
        return 0;
    }
}

void vrend_metal_bind_constant_buffers_internal(
    struct vrend_metal_context *ctx) {
    if (!ctx) {
        return;
    }

    if (ctx->render_encoder) {
        for (uint32_t i = 0; i < VREND_MAX_CONST_BUFFERS; i++) {
            struct vrend_metal_constant_binding vb = ctx->constant_buffers[VREND_METAL_STAGE_VERTEX][i];
            if (vb.buffer && vb.length > 0) {
                [ctx->render_encoder setVertexBuffer:vb.buffer offset:vb.offset atIndex:VREND_METAL_CONST_BASE + i];
            }
        }
        for (uint32_t i = 0; i < VREND_MAX_CONST_BUFFERS; i++) {
            struct vrend_metal_constant_binding fb = ctx->constant_buffers[VREND_METAL_STAGE_FRAGMENT][i];
            if (fb.buffer && fb.length > 0) {
                [ctx->render_encoder setFragmentBuffer:fb.buffer offset:fb.offset atIndex:VREND_METAL_CONST_BASE + i];
            }
        }
    }

    if (ctx->compute_encoder) {
        for (uint32_t i = 0; i < VREND_MAX_CONST_BUFFERS; i++) {
            struct vrend_metal_constant_binding cb = ctx->constant_buffers[VREND_METAL_STAGE_COMPUTE][i];
            if (cb.buffer && cb.length > 0) {
                [ctx->compute_encoder setBuffer:cb.buffer offset:cb.offset atIndex:VREND_METAL_CONST_BASE + i];
            }
        }
    }
}

static bool vrend_metal_buffer_range_valid(id<MTLBuffer> buffer,
                                            uint32_t offset,
                                            uint32_t length,
                                            const char *label,
                                            uint32_t slot) {
    if (!buffer) {
        return false;
    }

    NSUInteger buf_len = [buffer length];
    if ((NSUInteger)offset >= buf_len) {
        NSLog(@"[Metal Backend] %@ slot %u offset %u exceeds buffer length %lu",
              @(label), slot, offset, (unsigned long)buf_len);
        return false;
    }

    if (length > 0) {
        NSUInteger required = (NSUInteger)offset + (NSUInteger)length;
        if (required > buf_len) {
            NSLog(@"[Metal Backend] %@ slot %u range [%u,+%u) exceeds buffer length %lu",
                  @(label), slot, offset, length, (unsigned long)buf_len);
            return false;
        }
    }

    return true;
}

void vrend_metal_apply_clip_state(struct vrend_metal_context *ctx) {
    if (!ctx || !ctx->render_encoder) {
        return;
    }

    struct {
        float planes[VREND_MAX_CLIP_PLANES][4];
        uint32_t enabled_mask;
        uint32_t padding[3];
    } clip_data;
    memset(&clip_data, 0, sizeof(clip_data));

    for (uint32_t plane = 0; plane < VREND_MAX_CLIP_PLANES; plane++) {
        for (uint32_t component = 0; component < 4; component++) {
            clip_data.planes[plane][component] = ctx->clip_state.planes[plane][component];
        }
    }
    clip_data.enabled_mask = ctx->clip_state.enabled_mask;

    [ctx->render_encoder setVertexBytes:&clip_data
                                  length:sizeof(clip_data)
                                 atIndex:VREND_METAL_CLIP_STATE_INDEX];

    ctx->clip_state.dirty = false;
}

struct vrend_metal_streamout_upload {
    uint32_t enabled_mask;
    uint32_t reserved[3];
    struct {
        uint32_t offset;
        uint32_t size;
        uint32_t append;
        uint32_t resource;
    } slots[VREND_MAX_STREAMOUT_TARGETS];
};

void vrend_metal_bind_streamout_buffers(struct vrend_metal_context *ctx) {
    if (!ctx || !ctx->render_encoder) {
        return;
    }

    struct vrend_metal_streamout_upload upload;
    memset(&upload, 0, sizeof(upload));

    for (uint32_t slot = 0; slot < VREND_MAX_STREAMOUT_TARGETS; slot++) {
        struct vrend_metal_streamout_binding *binding = &ctx->streamout_bindings[slot];
        uint32_t buffer_index = VREND_METAL_STREAMOUT_BUFFER_BASE + slot;

        if (!binding->buffer || binding->size == 0) {
            [ctx->render_encoder setVertexBuffer:nil offset:0 atIndex:buffer_index];
            continue;
        }

        if (!vrend_metal_buffer_range_valid(binding->buffer,
                                             binding->offset,
                                             binding->size,
                                             "Stream-out", slot)) {
            [ctx->render_encoder setVertexBuffer:nil offset:0 atIndex:buffer_index];
            continue;
        }

        [ctx->render_encoder setVertexBuffer:binding->buffer
                                      offset:binding->offset
                                     atIndex:buffer_index];

        upload.enabled_mask |= (1u << slot);
        upload.slots[slot].offset = binding->offset;
        upload.slots[slot].size = binding->size;
        upload.slots[slot].append = binding->append ? 1u : 0u;
        upload.slots[slot].resource = binding->resource_handle;
    }

    [ctx->render_encoder setVertexBytes:&upload
                                  length:sizeof(upload)
                                 atIndex:VREND_METAL_STREAMOUT_META_INDEX];
}

static void vrend_metal_bind_shader_buffers_stage(id<MTLRenderCommandEncoder> render_encoder,
                                                  id<MTLComputeCommandEncoder> compute_encoder,
                                                  struct vrend_metal_shader_buffer_binding *bindings,
                                                  enum vrend_metal_shader_stage stage) {
    if (!bindings) {
        return;
    }

    for (uint32_t slot = 0; slot < VREND_MAX_SHADER_BUFFERS; slot++) {
        struct vrend_metal_shader_buffer_binding binding = bindings[slot];
        uint32_t buffer_index = VREND_METAL_SHADER_BUFFER_BASE + slot;

        if (!binding.buffer) {
            if (render_encoder) {
                if (stage == VREND_METAL_STAGE_VERTEX) {
                    [render_encoder setVertexBuffer:nil offset:0 atIndex:buffer_index];
                } else if (stage == VREND_METAL_STAGE_FRAGMENT) {
                    [render_encoder setFragmentBuffer:nil offset:0 atIndex:buffer_index];
                }
            }
            if (compute_encoder && stage == VREND_METAL_STAGE_COMPUTE) {
                [compute_encoder setBuffer:nil offset:0 atIndex:buffer_index];
            }
            continue;
        }

        if (!vrend_metal_buffer_range_valid(binding.buffer,
                                             binding.offset,
                                             binding.length,
                                             "Shader buffer",
                                             slot)) {
            continue;
        }

        if (render_encoder) {
            if (stage == VREND_METAL_STAGE_VERTEX) {
                [render_encoder setVertexBuffer:binding.buffer offset:binding.offset atIndex:buffer_index];
            } else if (stage == VREND_METAL_STAGE_FRAGMENT) {
                [render_encoder setFragmentBuffer:binding.buffer offset:binding.offset atIndex:buffer_index];
            }
        }

        if (compute_encoder && stage == VREND_METAL_STAGE_COMPUTE) {
            [compute_encoder setBuffer:binding.buffer offset:binding.offset atIndex:buffer_index];
        }
    }
}

void vrend_metal_bind_shader_buffers_internal(struct vrend_metal_context *ctx) {
    if (!ctx) {
        return;
    }

    id<MTLRenderCommandEncoder> render_encoder = ctx->render_encoder;
    id<MTLComputeCommandEncoder> compute_encoder = ctx->compute_encoder;

    if (render_encoder) {
        vrend_metal_bind_shader_buffers_stage(render_encoder, nil,
                                              ctx->shader_buffers[VREND_METAL_STAGE_VERTEX],
                                              VREND_METAL_STAGE_VERTEX);
        vrend_metal_bind_shader_buffers_stage(render_encoder, nil,
                                              ctx->shader_buffers[VREND_METAL_STAGE_FRAGMENT],
                                              VREND_METAL_STAGE_FRAGMENT);
    }

    if (compute_encoder) {
        vrend_metal_bind_shader_buffers_stage(nil, compute_encoder,
                                              ctx->shader_buffers[VREND_METAL_STAGE_COMPUTE],
                                              VREND_METAL_STAGE_COMPUTE);
    }
}

/* Command submission - now uses command parser */
#include "vrend_metal_command.h"

int vrend_metal_submit_cmd(
    struct virgl_context *ctx,
    const void *cmd_buf,
    uint32_t cmd_size) {
    
    return vrend_metal_parse_command_stream(ctx, cmd_buf, cmd_size);
}

int vrend_metal_clear(
    struct virgl_context *vctx,
    uint32_t buffers,
    const float *color,
    double depth,
    uint32_t stencil) {
    
    struct vrend_metal_context *ctx = (struct vrend_metal_context*)vctx;
    
    @autoreleasepool {
        // Create render pass descriptor for clear operation
        MTLRenderPassDescriptor *passDesc = [MTLRenderPassDescriptor renderPassDescriptor];
        
        // Configure color attachments from framebuffer state
        for (uint32_t i = 0; i < ctx->framebuffer_color_count; i++) {
            if (ctx->framebuffer_color[i]) {
                passDesc.colorAttachments[i].texture = ctx->framebuffer_color[i];
                if (buffers & 0x1) {  /* PIPE_CLEAR_COLOR */
                    passDesc.colorAttachments[i].loadAction = MTLLoadActionClear;
                    passDesc.colorAttachments[i].clearColor = MTLClearColorMake(
                        color[0], color[1], color[2], color[3]);
                } else {
                    passDesc.colorAttachments[i].loadAction = MTLLoadActionLoad;
                }
                passDesc.colorAttachments[i].storeAction = MTLStoreActionStore;
            }
        }
        
        // Configure depth attachment
        if (ctx->framebuffer_depth) {
            passDesc.depthAttachment.texture = ctx->framebuffer_depth;
            if (buffers & 0x2) {  /* PIPE_CLEAR_DEPTH */
                passDesc.depthAttachment.loadAction = MTLLoadActionClear;
                passDesc.depthAttachment.clearDepth = depth;
            } else {
                passDesc.depthAttachment.loadAction = MTLLoadActionLoad;
            }
            passDesc.depthAttachment.storeAction = MTLStoreActionStore;
        }
        
        // Configure stencil attachment
        if (ctx->framebuffer_stencil) {
            passDesc.stencilAttachment.texture = ctx->framebuffer_stencil;
            if (buffers & 0x4) {  /* PIPE_CLEAR_STENCIL */
                passDesc.stencilAttachment.loadAction = MTLLoadActionClear;
                passDesc.stencilAttachment.clearStencil = stencil;
            } else {
                passDesc.stencilAttachment.loadAction = MTLLoadActionLoad;
            }
            passDesc.stencilAttachment.storeAction = MTLStoreActionStore;
        }
        
        // End previous encoder if active
        if (ctx->render_encoder) {
            [ctx->render_encoder endEncoding];
        }
        
        // Create render encoder to execute clear
        ctx->render_encoder = [ctx->command_buffer renderCommandEncoderWithDescriptor:passDesc];
        [ctx->render_encoder endEncoding];
        ctx->render_encoder = nil;
        
        NSLog(@"[Metal Backend] Cleared buffers=0x%x color=(%.2f,%.2f,%.2f,%.2f)",
              buffers, color[0], color[1], color[2], color[3]);
        
        return 0;
    }
}

static inline NSUInteger vrend_metal_index_stride(const struct vrend_metal_context *ctx) {
    if (ctx->index_buffer_stride) {
        return ctx->index_buffer_stride;
    }
    return (ctx->index_buffer_type == MTLIndexTypeUInt32) ? 4 : 2;
}

static int vrend_metal_issue_indexed_draw(
    struct vrend_metal_context *ctx,
    const struct vrend_metal_draw_info *info,
    MTLPrimitiveType primitive,
    uint32_t first_index,
    uint32_t index_count) {

    if (index_count == 0) {
        return 0;
    }

    NSUInteger stride = vrend_metal_index_stride(ctx);
    NSUInteger offset = ctx->index_buffer_offset + ((NSUInteger)first_index * stride);
    NSUInteger bytes_needed = (NSUInteger)index_count * stride;
    NSUInteger buffer_length = [ctx->index_buffer length];
    if (offset + bytes_needed > buffer_length) {
        NSLog(@"[Metal Backend] Indexed draw overruns buffer (offset=%lu size=%lu len=%lu)",
              (unsigned long)offset, (unsigned long)bytes_needed, (unsigned long)buffer_length);
        return -1;
    }

    NSUInteger instanceCount = info->instance_count > 0 ? info->instance_count : 1;

    if (@available(macOS 10.11, *)) {
        [ctx->render_encoder drawIndexedPrimitives:primitive
                                        indexCount:index_count
                                         indexType:ctx->index_buffer_type
                                       indexBuffer:ctx->index_buffer
                                 indexBufferOffset:offset
                                     instanceCount:instanceCount
                                         baseVertex:info->index_bias
                                       baseInstance:info->start_instance];
    } else {
        if (info->index_bias != 0 || info->start_instance != 0) {
            NSLog(@"[Metal Backend] baseVertex/baseInstance unsupported on this macOS version");
            return -1;
        }
        [ctx->render_encoder drawIndexedPrimitives:primitive
                                        indexCount:index_count
                                         indexType:ctx->index_buffer_type
                                       indexBuffer:ctx->index_buffer
                                 indexBufferOffset:offset];
    }

    return 0;
}

static int vrend_metal_draw_indexed_with_restart(
    struct vrend_metal_context *ctx,
    const struct vrend_metal_draw_info *info,
    MTLPrimitiveType primitive) {

    NSUInteger stride = vrend_metal_index_stride(ctx);
    NSUInteger first_byte = ctx->index_buffer_offset + ((NSUInteger)info->start * stride);
    const uint8_t *base = ctx->index_buffer ? ([ctx->index_buffer contents]) : NULL;
    if (!base) {
        NSLog(@"[Metal Backend] Primitive restart requires CPU-visible index buffer contents");
        return -1;
    }

    const uint8_t *indices = base + first_byte;
    uint32_t restart = info->restart_index;
    uint32_t segment_start = 0;
    uint32_t emitted = 0;
    uint32_t segment_count = 0;
    int ret = 0;

    for (uint32_t i = 0; i < info->count; i++) {
        uint32_t idx_val = (stride == 2)
            ? ((const uint16_t*)indices)[i]
            : ((const uint32_t*)indices)[i];

        if (info->max_index >= info->min_index) {
            if (idx_val < info->min_index || idx_val > info->max_index) {
                NSLog(@"[Metal Backend]   Index %u outside declared range [%u, %u]",
                      idx_val, info->min_index, info->max_index);
            }
        }

        if (idx_val == restart) {
            uint32_t span = i - segment_start;
            if (span > 0) {
                ret = vrend_metal_issue_indexed_draw(ctx, info, primitive,
                                                     info->start + segment_start,
                                                     span);
                if (ret != 0) {
                    return ret;
                }
                emitted += span;
                segment_count++;
            }
            segment_start = i + 1;
        }
    }

    if (segment_start < info->count) {
        ret = vrend_metal_issue_indexed_draw(ctx, info, primitive,
                                             info->start + segment_start,
                                             info->count - segment_start);
        if (ret != 0) {
            return ret;
        }
        emitted += info->count - segment_start;
        segment_count++;
    }

    NSLog(@"[Metal Backend] Primitive restart emitted %u indices over %u segments",
          emitted, segment_count);
    return 0;
}

int vrend_metal_draw_vbo(
    struct virgl_context *vctx,
    const struct vrend_metal_draw_info *info) {
    
    if (!info) {
        return -1;
    }
    
    struct vrend_metal_context *ctx = (struct vrend_metal_context*)vctx;
    
    @autoreleasepool {
        if (ctx->compute_encoder) {
            [ctx->compute_encoder endEncoding];
            ctx->compute_encoder = nil;
            ctx->current_compute_pipeline = 0;
        }

        if (!ctx->render_encoder) {
            MTLRenderPassDescriptor *passDesc = [MTLRenderPassDescriptor renderPassDescriptor];
            
            for (uint32_t i = 0; i < ctx->framebuffer_color_count; i++) {
                if (ctx->framebuffer_color[i]) {
                    passDesc.colorAttachments[i].texture = ctx->framebuffer_color[i];
                    passDesc.colorAttachments[i].loadAction = MTLLoadActionLoad;
                    passDesc.colorAttachments[i].storeAction = MTLStoreActionStore;
                }
            }
            
            if (ctx->framebuffer_depth) {
                passDesc.depthAttachment.texture = ctx->framebuffer_depth;
                passDesc.depthAttachment.loadAction = MTLLoadActionLoad;
                passDesc.depthAttachment.storeAction = MTLStoreActionStore;
            }
            
            ctx->render_encoder = [ctx->command_buffer renderCommandEncoderWithDescriptor:passDesc];
            [ctx->render_encoder setViewport:ctx->viewport];
            
            if (ctx->current_pipeline != 0) {
                id<MTLRenderPipelineState> pipeline = [ctx->metal_pipelines objectForKey:@(ctx->current_pipeline)];
                if (pipeline) {
                    [ctx->render_encoder setRenderPipelineState:pipeline];
                }
            }
            
            for (uint32_t i = 0; i < ctx->vertex_buffer_count; i++) {
                if (ctx->vertex_buffers[i]) {
                    [ctx->render_encoder setVertexBuffer:ctx->vertex_buffers[i]
                                                  offset:ctx->vertex_buffer_offsets[i]
                                                 atIndex:i];
                }
            }
            vrend_metal_bind_constant_buffers_internal(ctx);
            vrend_metal_apply_bound_samplers(ctx);
            vrend_metal_apply_bound_textures(ctx);
            vrend_metal_apply_clip_state(ctx);
            vrend_metal_bind_streamout_buffers(ctx);
            vrend_metal_bind_shader_buffers_internal(ctx);
        }
        
        MTLPrimitiveType metalPrimitive;
        switch (info->mode) {
            case PIPE_PRIM_POINTS:
                metalPrimitive = MTLPrimitiveTypePoint;
                break;
            case PIPE_PRIM_LINES:
                metalPrimitive = MTLPrimitiveTypeLine;
                break;
            case PIPE_PRIM_LINE_STRIP:
                metalPrimitive = MTLPrimitiveTypeLineStrip;
                break;
            case PIPE_PRIM_TRIANGLES:
                metalPrimitive = MTLPrimitiveTypeTriangle;
                break;
            case PIPE_PRIM_TRIANGLE_STRIP:
                metalPrimitive = MTLPrimitiveTypeTriangleStrip;
                break;
            default:
                NSLog(@"[Metal Backend] Unsupported primitive mode after promotion: %u", info->mode);
                return -1;
        }

        if (info->indexed) {
            if (!ctx->index_buffer) {
                NSLog(@"[Metal Backend] Indexed draw requested with no bound index buffer");
                return -1;
            }

            if (ctx->index_buffer_stride == 0) {
                ctx->index_buffer_stride = vrend_metal_index_stride(ctx);
            }

            NSUInteger stride = vrend_metal_index_stride(ctx);
            NSUInteger required = ((NSUInteger)info->start + info->count) * stride;
            NSUInteger available = [ctx->index_buffer length];
            if (ctx->index_buffer_offset + required > available) {
                NSLog(@"[Metal Backend] Indexed draw exceeds buffer bounds (need=%lu have=%lu)",
                      (unsigned long)(ctx->index_buffer_offset + required),
                      (unsigned long)available);
                return -1;
            }

            if (info->primitive_restart) {
                int ret = vrend_metal_draw_indexed_with_restart(ctx, info, metalPrimitive);
                if (ret != 0) {
                    return ret;
                }
            } else {
                int ret = vrend_metal_issue_indexed_draw(ctx, info, metalPrimitive,
                                                         info->start,
                                                         info->count);
                if (ret != 0) {
                    return ret;
                }
            }

            NSLog(@"[Metal Backend] Drew %u indices (mode=%u inst=%u indexed)",
                  info->count, info->mode, info->instance_count > 0 ? info->instance_count : 1);
            return 0;
        }

        if (info->primitive_restart) {
            NSLog(@"[Metal Backend] Primitive restart ignored for non-indexed draw (idx=%u)",
                  info->restart_index);
        }

        NSUInteger instanceCount = info->instance_count > 0 ? info->instance_count : 1;
        if (info->start_instance != 0) {
            if (@available(macOS 10.11, *)) {
                [ctx->render_encoder drawPrimitives:metalPrimitive
                                        vertexStart:info->start
                                        vertexCount:info->count
                                      instanceCount:instanceCount
                                        baseInstance:info->start_instance];
            } else {
                NSLog(@"[Metal Backend] baseInstance draws require macOS 10.11+ (start_instance=%u)",
                      info->start_instance);
                return -1;
            }
        } else {
            [ctx->render_encoder drawPrimitives:metalPrimitive
                                    vertexStart:info->start
                                    vertexCount:info->count
                                  instanceCount:instanceCount];
        }
        
        NSLog(@"[Metal Backend] Drew %u vertices (mode=%u inst=%lu)",
              info->count, info->mode, (unsigned long)instanceCount);
        return 0;
    }
}

int vrend_metal_launch_grid(
    struct virgl_context *vctx,
    const struct vrend_metal_grid_info *info) {
    if (!info) {
        return -1;
    }

    struct vrend_metal_context *ctx = (struct vrend_metal_context*)vctx;
    if (!ctx) {
        return -1;
    }

    if (info->indirect) {
        NSLog(@"[Metal Backend] Indirect compute dispatch unsupported (handle=%u offset=%u)",
              info->indirect_handle, info->indirect_offset);
        return -1;
    }

    if (!ctx->command_buffer) {
        ctx->command_buffer = [g_metal_queue commandBuffer];
    }
    if (!ctx->command_buffer) {
        NSLog(@"[Metal Backend] Unable to acquire command buffer for compute dispatch");
        return -1;
    }

    if (ctx->render_encoder) {
        [ctx->render_encoder endEncoding];
        ctx->render_encoder = nil;
    }

    id<MTLComputePipelineState> pipeline = vrend_metal_acquire_compute_pipeline(ctx);
    if (!pipeline) {
        return -1;
    }

    if (!ctx->compute_encoder) {
        ctx->compute_encoder = [ctx->command_buffer computeCommandEncoder];
        ctx->current_compute_pipeline = 0;
    }
    if (!ctx->compute_encoder) {
        NSLog(@"[Metal Backend] Failed to create compute encoder");
        return -1;
    }

    if (ctx->current_compute_pipeline != ctx->bound_compute_shader) {
        [ctx->compute_encoder setComputePipelineState:pipeline];
        ctx->current_compute_pipeline = ctx->bound_compute_shader;
    }

    vrend_metal_bind_constant_buffers_internal(ctx);
    vrend_metal_apply_bound_samplers(ctx);
    vrend_metal_apply_bound_textures(ctx);
    vrend_metal_bind_shader_buffers_internal(ctx);

    if (info->block[0] == 0 || info->block[1] == 0 || info->block[2] == 0) {
        NSLog(@"[Metal Backend] Invalid compute block size (%u,%u,%u)",
              info->block[0], info->block[1], info->block[2]);
        return -1;
    }

    uint64_t threads_x = info->block[0];
    uint64_t threads_y = info->block[1];
    uint64_t threads_z = info->block[2];
    uint64_t total_threads = threads_x * threads_y * threads_z;

    NSUInteger max_threads = pipeline.maxTotalThreadsPerThreadgroup;
    if (total_threads > max_threads) {
        NSLog(@"[Metal Backend] Compute block size %llu exceeds pipeline limit %lu",
              (unsigned long long)total_threads, (unsigned long)max_threads);
        return -1;
    }

    if (info->grid[0] == 0 || info->grid[1] == 0 || info->grid[2] == 0) {
        NSLog(@"[Metal Backend] Invalid grid size (%u,%u,%u)",
              info->grid[0], info->grid[1], info->grid[2]);
        return -1;
    }

    MTLSize threadsPerThreadgroup = MTLSizeMake(info->block[0], info->block[1], info->block[2]);
    MTLSize threadgroupsPerGrid = MTLSizeMake(info->grid[0], info->grid[1], info->grid[2]);

    [ctx->compute_encoder dispatchThreadgroups:threadgroupsPerGrid
                        threadsPerThreadgroup:threadsPerThreadgroup];

    NSLog(@"[Metal Backend] Dispatched compute grid (%ux%ux%u) blocks (%ux%ux%u)",
          info->grid[0], info->grid[1], info->grid[2],
          info->block[0], info->block[1], info->block[2]);
    return 0;
}

/* Add stub implementations for remaining functions */
int vrend_metal_resource_attach_backing(
    struct virgl_context *ctx,
    struct virgl_resource *res,
    void **backing_pages,
    uint32_t nr_pages) { return 0; }

void vrend_metal_resource_detach_backing(
    struct virgl_context *ctx,
    struct virgl_resource *res) {}

int vrend_metal_resource_create_blob(
    struct virgl_context *vctx,
    uint32_t blob_id,
    uint64_t size,
    uint32_t blob_mem,
    uint32_t blob_flags) {
    
    struct vrend_metal_context *ctx = (struct vrend_metal_context*)vctx;
    
    @autoreleasepool {
        // Create shared memory buffer accessible by both VM and host
        MTLResourceOptions options = MTLResourceStorageModeShared;
        
        if (blob_flags & 0x1) {  /* Read-only from GPU */
            options |= MTLResourceCPUCacheModeWriteCombined;
        }
        
        id<MTLBuffer> buffer = [g_metal_device newBufferWithLength:size
                                                           options:options];
        if (!buffer) {
            NSLog(@"[Metal Backend] Failed to create blob resource %u (size=%llu)",
                  blob_id, size);
            return -1;
        }
        
        // Store in context's buffer dictionary
        [ctx->metal_buffers setObject:buffer forKey:@(blob_id)];
        
        NSLog(@"[Metal Backend] Created blob resource %u: %llu bytes (shared memory)",
              blob_id, size);
        
        return 0;
    }
}

int vrend_metal_resource_map_blob(
    struct virgl_context *vctx,
    uint32_t resource_id,
    uint64_t offset) {
    
    struct vrend_metal_context *ctx = (struct vrend_metal_context*)vctx;
    
    @autoreleasepool {
        id<MTLBuffer> buffer = [ctx->metal_buffers objectForKey:@(resource_id)];
        if (!buffer) {
            NSLog(@"[Metal Backend] Blob resource %u not found for mapping", resource_id);
            return -1;
        }
        
        // Get pointer to shared memory (zero-copy access)
        void *mapped_ptr = (uint8_t*)[buffer contents] + offset;
        
        NSLog(@"[Metal Backend] Mapped blob resource %u at offset %llu → %p",
              resource_id, offset, mapped_ptr);
        
        // In real implementation, return this pointer to virglrenderer
        return 0;
    }
}

void vrend_metal_resource_unmap_blob(
    struct virgl_context *ctx,
    uint32_t resource_id) {}

int vrend_metal_transfer_to_host(
    struct virgl_context *ctx,
    struct virgl_resource *res,
    const struct virgl_box *box,
    uint32_t level) { return 0; }

int vrend_metal_transfer_from_host(
    struct virgl_context *ctx,
    struct virgl_resource *res,
    const struct virgl_box *box,
    uint32_t level) { return 0; }

static void vrend_metal_apply_blend_state_to_descriptor(
    struct vrend_metal_context *ctx,
    NSDictionary *blend_state,
    MTLRenderPipelineColorAttachmentDescriptor *colorDesc) {
    if (!blend_state) {
        colorDesc.blendingEnabled = NO;
        return;
    }
    colorDesc.blendingEnabled = [blend_state[@"enabled"] boolValue];
    colorDesc.sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
    colorDesc.destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    colorDesc.rgbBlendOperation = MTLBlendOperationAdd;
    colorDesc.sourceAlphaBlendFactor = MTLBlendFactorOne;
    colorDesc.destinationAlphaBlendFactor = MTLBlendFactorZero;
    colorDesc.alphaBlendOperation = MTLBlendOperationAdd;
}

static id<MTLDepthStencilState> vrend_metal_create_depth_state(
    struct vrend_metal_context *ctx,
    NSDictionary *dsa_state) {
    MTLDepthStencilDescriptor *desc = [[MTLDepthStencilDescriptor alloc] init];
    if (dsa_state) {
        BOOL depth_enabled = [dsa_state[@"depth_enabled"] boolValue];
        desc.depthCompareFunction = depth_enabled ? MTLCompareFunctionLess : MTLCompareFunctionAlways;
        desc.depthWriteEnabled = [dsa_state[@"depth_writemask"] boolValue];
        if ([dsa_state[@"stencil_enabled"] boolValue]) {
            MTLStencilDescriptor *stencilDesc = [[MTLStencilDescriptor alloc] init];
            stencilDesc.stencilCompareFunction = MTLCompareFunctionAlways;
            stencilDesc.depthStencilPassOperation = MTLStencilOperationKeep;
            stencilDesc.depthFailureOperation = MTLStencilOperationKeep;
            stencilDesc.stencilFailureOperation = MTLStencilOperationKeep;
            desc.frontFaceStencil = stencilDesc;
            desc.backFaceStencil = stencilDesc;
        }
    } else {
        desc.depthCompareFunction = MTLCompareFunctionAlways;
        desc.depthWriteEnabled = NO;
    }
    return [g_metal_device newDepthStencilStateWithDescriptor:desc];
}

static id<MTLRenderPipelineState> vrend_metal_build_pipeline(
    struct vrend_metal_context *ctx,
    id<MTLFunction> vertex_func,
    id<MTLFunction> fragment_func,
    NSDictionary *blend_state) {
    MTLRenderPipelineDescriptor *desc = [[MTLRenderPipelineDescriptor alloc] init];
    desc.vertexFunction = vertex_func;
    desc.fragmentFunction = fragment_func;
    desc.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
    desc.stencilAttachmentPixelFormat = MTLPixelFormatStencil8;
    
    vrend_metal_apply_blend_state_to_descriptor(ctx, blend_state, desc.colorAttachments[0]);

    MTLVertexDescriptor *vertexDesc = vrend_metal_create_vertex_descriptor(ctx);
    if (vertexDesc) {
        desc.vertexDescriptor = vertexDesc;
    } else {
        NSLog(@"[Metal Backend] Pipeline missing vertex descriptor (no vertex elements bound)");
    }
    
    NSError *error = nil;
    id<MTLRenderPipelineState> pipeline = [g_metal_device newRenderPipelineStateWithDescriptor:desc
                                                                                          error:&error];
    if (!pipeline) {
        NSLog(@"[Metal Backend] Failed to build pipeline: %@", error);
    }
    return pipeline;
}

int vrend_metal_create_pipeline(
    struct virgl_context *vctx,
    uint32_t pipeline_id) {
    struct vrend_metal_context *ctx = (struct vrend_metal_context*)vctx;
    
    @autoreleasepool {
        id<MTLFunction> vertex_func = [ctx->metal_shaders objectForKey:@(ctx->bound_vertex_shader)];
        id<MTLFunction> fragment_func = [ctx->metal_shaders objectForKey:@(ctx->bound_fragment_shader)];
        if (!vertex_func || !fragment_func) {
            NSLog(@"[Metal Backend] Cannot create pipeline %u: missing shaders", pipeline_id);
            return -1;
        }
        
        NSDictionary *blend_state = [ctx->blend_states objectForKey:@(ctx->bound_blend_state)];
        NSDictionary *depth_state = [ctx->depth_states objectForKey:@(ctx->bound_depth_state)];
        
        id<MTLRenderPipelineState> pipeline = vrend_metal_build_pipeline(ctx, vertex_func, fragment_func, blend_state);
        if (!pipeline) {
            return -1;
        }
        
        [ctx->metal_pipelines setObject:pipeline forKey:@(pipeline_id)];
        
        id<MTLDepthStencilState> depthStencil = vrend_metal_create_depth_state(ctx, depth_state);
        if (depthStencil) {
            [ctx->metal_depth_states setObject:depthStencil forKey:@(pipeline_id)];
        }
        
        NSLog(@"[Metal Backend] Created pipeline %u (vs=%u fs=%u blend=%u depth=%u)",
              pipeline_id, ctx->bound_vertex_shader, ctx->bound_fragment_shader,
              ctx->bound_blend_state, ctx->bound_depth_state);
        return 0;
    }
}

int vrend_metal_bind_pipeline(
    struct virgl_context *vctx,
    uint32_t pipeline_id) {
    
    struct vrend_metal_context *ctx = (struct vrend_metal_context*)vctx;
    
    @autoreleasepool {
        id<MTLRenderPipelineState> pipeline = [ctx->metal_pipelines objectForKey:@(pipeline_id)];
        if (!pipeline) {
            NSLog(@"[Metal Backend] Pipeline %u not found", pipeline_id);
            return -1;
        }
        
        ctx->current_pipeline = pipeline_id;
        ctx->current_depth_stencil_state = [ctx->metal_depth_states objectForKey:@(pipeline_id)];
        
        if (ctx->render_encoder) {
            [ctx->render_encoder setRenderPipelineState:pipeline];
            if (ctx->current_depth_stencil_state) {
                [ctx->render_encoder setDepthStencilState:ctx->current_depth_stencil_state];
            }
        }
        
        NSLog(@"[Metal Backend] Bound pipeline %u", pipeline_id);
        return 0;
    }
}

void vrend_metal_request_pipeline_update(
    struct virgl_context *vctx) {
    struct vrend_metal_context *ctx = (struct vrend_metal_context*)vctx;
    ctx->current_pipeline = 0;
}

int vrend_metal_create_fence(
    struct virgl_context *vctx,
    uint64_t fence_id) {
    struct vrend_metal_context *ctx = (struct vrend_metal_context*)vctx;
    if (!ctx || !g_metal_initialized) {
        return -1;
    }

    @autoreleasepool {
        if (!ctx->fence_lock) {
            ctx->fence_lock = [[NSLock alloc] init];
        }
        if (!ctx->fence_command_buffers) {
            ctx->fence_command_buffers = [[NSMutableDictionary alloc] init];
        }
        if (!ctx->command_buffer) {
            ctx->command_buffer = [g_metal_queue commandBuffer];
        }
        if (!ctx->command_buffer) {
            NSLog(@"[Metal Backend] Unable to create command buffer for fence %llu", fence_id);
            return -1;
        }

        if (ctx->render_encoder) {
            [ctx->render_encoder endEncoding];
            ctx->render_encoder = nil;
        }
        if (ctx->compute_encoder) {
            [ctx->compute_encoder endEncoding];
            ctx->compute_encoder = nil;
            ctx->current_compute_pipeline = 0;
        }

        NSNumber *fenceKey = @(fence_id);
        id<MTLCommandBuffer> fenceCommandBuffer = ctx->command_buffer;
        if (!fenceCommandBuffer) {
            return -1;
        }

        struct vrend_metal_context *block_ctx = ctx;
        [ctx->fence_lock lock];
        ctx->fence_command_buffers[fenceKey] = fenceCommandBuffer;
        if (@available(macOS 10.14, *)) {
            if (ctx->shared_event) {
                ctx->shared_event_value++;
                uint64_t signalValue = ctx->shared_event_value;
                [fenceCommandBuffer encodeSignalEvent:ctx->shared_event value:signalValue];
                if (ctx->fence_event_values) {
                    ctx->fence_event_values[fenceKey] = @(signalValue);
                }
                vrend_metal_emit_shared_event_info(ctx, MACH_PORT_NULL);
            }
        }
        [ctx->fence_lock unlock];

        [fenceCommandBuffer addCompletedHandler:^(id<MTLCommandBuffer> buffer) {
            struct vrend_metal_context *strong_ctx = block_ctx;
            if (!strong_ctx || !strong_ctx->fence_lock) {
                return;
            }
            @autoreleasepool {
                [strong_ctx->fence_lock lock];
                [strong_ctx->fence_command_buffers removeObjectForKey:fenceKey];
                [strong_ctx->fence_event_values removeObjectForKey:fenceKey];
                [strong_ctx->fence_lock unlock];
            }
        }];

        [fenceCommandBuffer commit];
        ctx->command_buffer = [g_metal_queue commandBuffer];
        ctx->fence_serial++;
          NSLog(@"[Metal Backend] Fence %llu queued on ctx %u (serial %llu)",
              (unsigned long long)fence_id,
              ctx->ctx_id,
              (unsigned long long)ctx->fence_serial);
        return 0;
    }
}

int vrend_metal_wait_fence(
    struct virgl_context *vctx,
    uint64_t fence_id) {
    struct vrend_metal_context *ctx = (struct vrend_metal_context*)vctx;
    if (!ctx || !g_metal_initialized) {
        return -1;
    }

    @autoreleasepool {
        NSNumber *fenceKey = @(fence_id);
        id<MTLCommandBuffer> fenceCommandBuffer = nil;
        if (ctx->fence_lock) {
            [ctx->fence_lock lock];
            fenceCommandBuffer = [ctx->fence_command_buffers objectForKey:fenceKey];
            [ctx->fence_lock unlock];
        }

        if (!fenceCommandBuffer) {
            // Either already completed or unknown
            NSLog(@"[Metal Backend] Fence %llu already signaled or not found",
                  (unsigned long long)fence_id);
            return 0;
        }

        [fenceCommandBuffer waitUntilCompleted];
        if (ctx->fence_lock) {
            [ctx->fence_lock lock];
            [ctx->fence_command_buffers removeObjectForKey:fenceKey];
            [ctx->fence_event_values removeObjectForKey:fenceKey];
            [ctx->fence_lock unlock];
        }
        NSLog(@"[Metal Backend] Fence %llu completed", (unsigned long long)fence_id);
        return 0;
    }
}

int vrend_metal_set_scanout(
    uint32_t scanout_id,
    uint32_t resource_id,
    const struct virgl_box *box) {
    if (!g_metal_initialized) {
        NSLog(@"[Metal Backend] set_scanout called before initialization");
        return -1;
    }
    if (scanout_id >= VREND_MAX_SCANOUTS) {
        NSLog(@"[Metal Backend] Scanout id %u out of range", scanout_id);
        return -1;
    }
    
    @autoreleasepool {
        if (resource_id == 0) {
            NSLog(@"[Metal Backend] Disabled scanout %u", scanout_id);
            g_scanout_textures[scanout_id] = nil;
            g_scanout_region_valid[scanout_id] = false;
            g_scanout_staging[scanout_id] = nil;
            g_scanout_row_bytes[scanout_id] = 0;
            g_scanout_width[scanout_id] = 0;
            g_scanout_height[scanout_id] = 0;
            g_scanout_format[scanout_id] = MTLPixelFormatInvalid;
            g_scanout_bytes_per_pixel[scanout_id] = 0;
            g_scanout_ctx_ids[scanout_id] = 0;
            vrend_metal_release_scanout_iosurface(scanout_id);
            return 0;
        }
        
        id<MTLTexture> texture = [g_metal_global_textures objectForKey:@(resource_id)];
        if (!texture) {
            NSLog(@"[Metal Backend] Scanout resource %u not found", resource_id);
            return -1;
        }
        if (!vrend_metal_ensure_scanout_storage(scanout_id, texture)) {
            return -1;
        }
        
        g_scanout_textures[scanout_id] = texture;
        if (box) {
            g_scanout_regions[scanout_id] = *box;
            g_scanout_region_valid[scanout_id] = true;
        } else {
            g_scanout_region_valid[scanout_id] = false;
        }
        
        NSLog(@"[Metal Backend] Set scanout %u -> resource %u region (%u,%u %ux%u)",
              scanout_id,
              resource_id,
              box ? box->x : 0,
              box ? box->y : 0,
              box ? box->w : 0,
              box ? box->h : 0);
        
        return 0;
    }

    return 0;
}

int vrend_metal_flush_resource(
    struct virgl_context *vctx,
    uint32_t resource_id,
    const struct virgl_box *box) {
    
    struct vrend_metal_context *ctx = (struct vrend_metal_context*)vctx;
    
    @autoreleasepool {
        // Get texture to flush
        id<MTLTexture> texture = [ctx->metal_textures objectForKey:@(resource_id)];
        if (!texture) {
            NSLog(@"[Metal Backend] Resource %u not found for flush", resource_id);
            return -1;
        }
        
        // Commit any pending command buffer
        if (ctx->command_buffer) {
            if (ctx->render_encoder) {
                [ctx->render_encoder endEncoding];
                ctx->render_encoder = nil;
            }
            if (ctx->compute_encoder) {
                [ctx->compute_encoder endEncoding];
                ctx->compute_encoder = nil;
                ctx->current_compute_pipeline = 0;
            }
            
            [ctx->command_buffer commit];
            [ctx->command_buffer waitUntilCompleted];
            
            // Create new command buffer for next frame
            ctx->command_buffer = [g_metal_queue commandBuffer];
        }
        
        NSLog(@"[Metal Backend] Flushed resource %u region=(%u,%u %ux%u)",
              resource_id,
              box ? box->x : 0, box ? box->y : 0,
              box ? box->w : 0, box ? box->h : 0);
        
        for (uint32_t i = 0; i < VREND_MAX_SCANOUTS; i++) {
            if (g_scanout_textures[i] == texture) {
                struct virgl_box stored_region = g_scanout_regions[i];
                const struct virgl_box *region_hint = NULL;
                if (box) {
                    region_hint = box;
                    NSLog(@"[Metal Backend]  ↳ scanout %u dirty (flush box %u,%u %ux%u)",
                          i, box->x, box->y, box->w, box->h);
                } else if (g_scanout_region_valid[i]) {
                    region_hint = &stored_region;
                    NSLog(@"[Metal Backend]  ↳ scanout %u dirty (%u,%u %ux%u)",
                          i,
                          stored_region.x,
                          stored_region.y,
                          stored_region.w,
                          stored_region.h);
                } else {
                    NSLog(@"[Metal Backend]  ↳ scanout %u dirty (full)", i);
                }
                vrend_metal_stage_scanout_region(ctx, i, texture, region_hint);
            }
        }
        
        // In real implementation:
        // 1. Blit texture to QEMU display surface
        // 2. Signal display update to QEMU
        // 3. Handle dirty region from box
        
        return 0;
    }
}

void vrend_metal_set_scanout_throttle(
    uint32_t scanout_id,
    double max_fps) {
    if (scanout_id >= VREND_MAX_SCANOUTS) {
        return;
    }
    if (max_fps <= 0.0) {
        g_scanout_throttle_interval[scanout_id] = 0;
        g_scanout_last_dispatch_time[scanout_id] = 0;
        return;
    }
    uint64_t interval_ns = (uint64_t)(1000000000.0 / max_fps);
    if (interval_ns == 0) {
        interval_ns = 1;
    }
    g_scanout_throttle_interval[scanout_id] = vrend_metal_ns_to_abs(interval_ns);
}

void vrend_metal_register_shared_event_listener(
    vrend_metal_shared_event_listener listener,
    void *userdata) {
    g_shared_event_listener = listener;
    g_shared_event_userdata = userdata;
    if (!listener || !g_context_registry_lock) {
        return;
    }
    [g_context_registry_lock lock];
    NSArray *activeContexts = [g_context_registry allValues];
    [g_context_registry_lock unlock];
    for (NSValue *value in activeContexts) {
        struct vrend_metal_context *ctx = (struct vrend_metal_context *)[value pointerValue];
        vrend_metal_emit_shared_event_info(ctx, MACH_PORT_NULL);
    }
}

int vrend_metal_get_shared_event_info(
    uint32_t ctx_id,
    struct vrend_metal_shared_event_info *info) {
    if (!info) {
        return -1;
    }
    struct vrend_metal_context *ctx = vrend_metal_lookup_context(ctx_id);
    if (!ctx) {
        return -1;
    }
    if (!vrend_metal_populate_shared_event_info(ctx, info, MACH_PORT_NULL)) {
        return -1;
    }
    return 0;
}

int vrend_metal_get_fence_sync_info(
    uint32_t ctx_id,
    uint64_t fence_id,
    struct vrend_metal_fence_sync_info *info) {
    if (!info) {
        return -1;
    }
    struct vrend_metal_context *ctx = vrend_metal_lookup_context(ctx_id);
    if (!ctx) {
        return -1;
    }
    mach_port_t port = vrend_metal_ensure_shared_event_port(ctx);
    if (port == MACH_PORT_NULL) {
        return -1;
    }
    NSNumber *value = nil;
    if (ctx->fence_lock) {
        [ctx->fence_lock lock];
        value = ctx->fence_event_values[@(fence_id)];
        [ctx->fence_lock unlock];
    }
    if (!value) {
        return -1;
    }
    info->ctx_id = ctx->ctx_id;
    info->fence_id = fence_id;
    info->event_value = [value unsignedLongLongValue];
    info->mach_port = port;
    return 0;
}

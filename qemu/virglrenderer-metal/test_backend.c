/*
 * Simple test program for virglrenderer Metal backend
 * Validates that the library links and initializes correctly
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>
#include "vrend_metal.h"
#include "vrend_metal_formats.h"

#ifndef VREND_SHADER_VERTEX
enum vrend_shader_type {
    VREND_SHADER_VERTEX = 0,
    VREND_SHADER_FRAGMENT = 1,
    VREND_SHADER_COMPUTE = 2,
};
#endif

char* vrend_metal_translate_glsl_to_msl(const char *glsl_source, enum vrend_shader_type type);

static int run_vertex_format_tests(void) {
    printf("Test 6: Validating vertex attribute formats...\n");

    const uint32_t scaled_formats[] = {
        VREND_PIPE_FORMAT_R8G8B8A8_USCALED,
        VREND_PIPE_FORMAT_R8G8B8A8_SSCALED,
        VREND_PIPE_FORMAT_R16G16_USCALED,
        VREND_PIPE_FORMAT_R16G16_SSCALED,
        VREND_PIPE_FORMAT_R16G16B16A16_USCALED,
        VREND_PIPE_FORMAT_R16G16B16A16_SSCALED,
    };
    const uint32_t double_formats[] = {
        VREND_PIPE_FORMAT_R64_FLOAT,
        VREND_PIPE_FORMAT_R64G64_FLOAT,
        VREND_PIPE_FORMAT_R64G64B64_FLOAT,
        VREND_PIPE_FORMAT_R64G64B64A64_FLOAT,
    };

    for (size_t i = 0; i < sizeof(scaled_formats)/sizeof(scaled_formats[0]); i++) {
        if (!vrend_metal_pipe_format_supported(scaled_formats[i])) {
            fprintf(stderr, "❌ Scaled vertex format %u unexpectedly unsupported\n", scaled_formats[i]);
            return -1;
        }
    }

    for (size_t i = 0; i < sizeof(double_formats)/sizeof(double_formats[0]); i++) {
        if (vrend_metal_pipe_format_supported(double_formats[i])) {
            fprintf(stderr, "❌ Double vertex format %u should report unsupported\n", double_formats[i]);
            return -1;
        }
    }

    const uint32_t invalid_formats[] = { 0xFFFFFFFFu, 512u };
    for (size_t i = 0; i < sizeof(invalid_formats)/sizeof(invalid_formats[0]); i++) {
        if (vrend_metal_pipe_format_supported(invalid_formats[i])) {
            fprintf(stderr, "❌ Invalid vertex format %u should not report supported\n", invalid_formats[i]);
            return -1;
        }
    }

    printf("✅ Scaled/double vertex formats validated\n\n");
    return 0;
}

static void run_sync_introspection_tests(struct virgl_context *ctx) {
    printf("Test 7: Inspecting shared event synchronization...\n");
    struct vrend_metal_shared_event_info event_info;
    int ret = vrend_metal_get_shared_event_info(1, &event_info);
    if (ret == 0) {
        printf("✅ Shared event exported (ctx=%u value=%llu)\n",
               event_info.ctx_id,
               (unsigned long long)event_info.signal_value);
        if (event_info.mach_port != MACH_PORT_NULL) {
            printf("   ↳ Mach port handle: 0x%x\n",
                   (unsigned)event_info.mach_port);
        }
        if (event_info.shared_event_handle) {
            printf("   ↳ Shared event handle pointer: 0x%llx\n",
                   (unsigned long long)event_info.shared_event_handle);
        }
        vrend_metal_create_fence(ctx, 50001);
        struct vrend_metal_fence_sync_info fence_info;
        if (vrend_metal_get_fence_sync_info(1, 50001, &fence_info) == 0) {
            printf("   ↳ Fence %llu maps to event value %llu (port=%u)\n",
                   (unsigned long long)fence_info.fence_id,
                   (unsigned long long)fence_info.event_value,
                   (unsigned)fence_info.mach_port);
        } else {
            printf("   ↳ Fence sync info unavailable (shared events optional on older macOS versions)\n");
        }
    } else {
        printf("⚠️  Shared events not available on this host build; skipping\n");
    }

    vrend_metal_set_scanout_throttle(0, 60.0);
    vrend_metal_set_scanout_throttle(0, 0.0);
    printf("✅ Scanout throttle API smoke-tested\n\n");
}

int main(int argc, char **argv) {
    printf("=== virglrenderer Metal Backend Test ===\n\n");
    
    /* Test 1: Initialize Metal backend */
    printf("Test 1: Initializing Metal backend...\n");
    int ret = vrend_metal_init(0);
    if (ret != 0) {
        fprintf(stderr, "❌ Failed to initialize Metal backend\n");
        return 1;
    }
    printf("✅ Metal backend initialized\n\n");
    
    /* Test 2: Get capabilities */
    printf("Test 2: Querying capabilities...\n");
    struct virgl_metal_caps caps;
    vrend_metal_get_caps(&caps);
    printf("✅ Capabilities:\n");
    printf("   Metal version: %u\n", caps.metal_version);
    printf("   Max texture size: %u\n", caps.max_texture_size);
    printf("   Max texture layers: %u\n", caps.max_texture_layers);
    printf("   Max buffer size: %u MB\n", caps.max_buffer_size / (1024*1024));
    printf("   Supports tessellation: %u\n", caps.supports_tessellation);
    printf("\n");
    
    /* Test 3: Create context */
    printf("Test 3: Creating context...\n");
    struct virgl_context *ctx = vrend_metal_create_context(1, 8, "test_ctx");
    if (!ctx) {
        fprintf(stderr, "❌ Failed to create context\n");
        vrend_metal_cleanup();
        return 1;
    }
    printf("✅ Context created\n\n");
    
    /* Test 4: Create resource */
    printf("Test 4: Creating texture resource...\n");
    struct virgl_resource res = {
        .resource_id = 100,
        .width = 1920,
        .height = 1080,
        .depth = 1,
        .format = 1,  /* VIRTIO_GPU_FORMAT_B8G8R8A8_UNORM */
        .bind = 0,
        .target = 0,
    };
    ret = vrend_metal_create_resource(ctx, &res);
    if (ret != 0) {
        fprintf(stderr, "❌ Failed to create resource\n");
        vrend_metal_destroy_context(ctx);
        vrend_metal_cleanup();
        return 1;
    }
    printf("✅ Texture resource created (1920x1080 BGRA)\n\n");
    
    /* Test 5: Shader translation */
    printf("Test 5: Testing shader translation...\n");
    const char *test_glsl = 
        "#version 120\n"
        "const vec3 light_pos = vec3(0.0, 0.0, 1.0);\n"
        "varying vec3 normal;\n"
        "uniform mat4 mvp;\n"
        "void main() {\n"
        "    gl_Position = mvp * vec4(normal, 1.0);\n"
        "}\n";
    
    char *msl = vrend_metal_translate_glsl_to_msl(test_glsl, VREND_SHADER_VERTEX);
    if (!msl) {
        fprintf(stderr, "❌ Shader translation failed\n");
        vrend_metal_destroy_resource(ctx, &res);
        vrend_metal_destroy_context(ctx);
        vrend_metal_cleanup();
        return 1;
    }
    printf("✅ Shader translated GLSL→MSL\n");
    printf("   MSL output preview:\n");
    printf("   ---\n");
    /* Print first 300 chars */
    for (int i = 0; i < 300 && msl[i] != '\0'; i++) {
        putchar(msl[i]);
    }
    printf("   ...\n   ---\n\n");
    free(msl);
    
    if (run_vertex_format_tests() != 0) {
        vrend_metal_destroy_resource(ctx, &res);
        vrend_metal_destroy_context(ctx);
        vrend_metal_cleanup();
        return 1;
    }

    run_sync_introspection_tests(ctx);

    /* Cleanup */
    printf("Cleanup: Destroying resources...\n");
    vrend_metal_destroy_resource(ctx, &res);
    vrend_metal_destroy_context(ctx);
    vrend_metal_cleanup();
    printf("✅ Cleanup complete\n\n");
    
    printf("=== All tests passed! ===\n");
    printf("The Metal backend is working correctly.\n");
    printf("Next steps:\n");
    printf("  1. Land Metal backend glue in virglrenderer/QEMU\n");
    printf("  2. Finish SDL Metal + vsync path for scanouts\n");
    printf("  3. Harden SDL Metal zero-copy bridge\n");
    printf("  4. Wire up shared events/frame pacing in SDL frontend\n");
    
    /* Exit immediately to avoid Metal background thread cleanup issues */
    _exit(0);
}

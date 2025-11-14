/*
 * VMVirtIOGLEngine.cpp
 * OpenGL Renderer Plugin for VirtIO GPU
 * 
 * This plugin allows macOS OpenGL/CGL to use the VMVirtIOGPUAccelerator
 * for hardware-accelerated 3D rendering.
 */

#include <OpenGL/OpenGL.h>
#include <OpenGL/gl.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Renderer ID for VirtIO GPU (matches what we set in the driver)
#define VIRTIO_RENDERER_ID 0x00024600

// Plugin entry points structure
typedef struct {
    // Version info
    unsigned long version;
    
    // Renderer query functions
    void* (*createRenderer)(CGLContextObj ctx);
    void (*destroyRenderer)(void* renderer);
    
    // OpenGL function dispatch
    void* (*getFunction)(const char* name);
    
    // Context management
    int (*makeCurrent)(void* renderer);
    int (*clearCurrent)(void* renderer);
    
    // Buffer management  
    int (*swapBuffers)(void* renderer);
    int (*flush)(void* renderer);
    
} GLEnginePlugin;

// Renderer instance structure
typedef struct {
    CGLContextObj context;
    io_service_t accelerator;
    io_connect_t connection;
    int initialized;
} VirtIOGLRenderer;

// Forward declarations
static void* virtio_createRenderer(CGLContextObj ctx);
static void virtio_destroyRenderer(void* renderer);
static void* virtio_getFunction(const char* name);
static int virtio_makeCurrent(void* renderer);
static int virtio_clearCurrent(void* renderer);
static int virtio_swapBuffers(void* renderer);
static int virtio_flush(void* renderer);

// Plugin entry point structure
static GLEnginePlugin g_plugin = {
    .version = 1,
    .createRenderer = virtio_createRenderer,
    .destroyRenderer = virtio_destroyRenderer,
    .getFunction = virtio_getFunction,
    .makeCurrent = virtio_makeCurrent,
    .clearCurrent = virtio_clearCurrent,
    .swapBuffers = virtio_swapBuffers,
    .flush = virtio_flush
};

// Create a new renderer instance
static void* virtio_createRenderer(CGLContextObj ctx) {
    VirtIOGLRenderer* renderer = (VirtIOGLRenderer*)malloc(sizeof(VirtIOGLRenderer));
    if (!renderer) return NULL;
    
    memset(renderer, 0, sizeof(VirtIOGLRenderer));
    renderer->context = ctx;
    
    // Find the VMVirtIOGPUAccelerator service
    CFMutableDictionaryRef matching = IOServiceMatching("VMVirtIOGPUAccelerator");
    if (!matching) {
        free(renderer);
        return NULL;
    }
    
    io_service_t service = IOServiceGetMatchingService(kIOMasterPortDefault, matching);
    if (!service) {
        free(renderer);
        return NULL;
    }
    
    renderer->accelerator = service;
    renderer->initialized = 1;
    
    printf("VMVirtIOGLEngine: Created renderer for VirtIO GPU\n");
    
    return renderer;
}

// Destroy renderer instance
static void virtio_destroyRenderer(void* r) {
    VirtIOGLRenderer* renderer = (VirtIOGLRenderer*)r;
    if (!renderer) return;
    
    if (renderer->connection) {
        IOServiceClose(renderer->connection);
    }
    
    if (renderer->accelerator) {
        IOObjectRelease(renderer->accelerator);
    }
    
    free(renderer);
}

// Get OpenGL function pointer
static void* virtio_getFunction(const char* name) {
    // For now, return NULL to let the system use default implementations
    // In the future, we could provide optimized implementations
    return NULL;
}

// Make renderer current
static int virtio_makeCurrent(void* r) {
    VirtIOGLRenderer* renderer = (VirtIOGLRenderer*)r;
    if (!renderer || !renderer->initialized) return -1;
    
    // Context is already current via CGL
    return 0;
}

// Clear current renderer
static int virtio_clearCurrent(void* r) {
    VirtIOGLRenderer* renderer = (VirtIOGLRenderer*)r;
    if (!renderer) return -1;
    
    return 0;
}

// Swap buffers
static int virtio_swapBuffers(void* r) {
    VirtIOGLRenderer* renderer = (VirtIOGLRenderer*)r;
    if (!renderer || !renderer->initialized) return -1;
    
    // Flush OpenGL commands
    glFlush();
    
    return 0;
}

// Flush rendering commands
static int virtio_flush(void* r) {
    VirtIOGLRenderer* renderer = (VirtIOGLRenderer*)r;
    if (!renderer || !renderer->initialized) return -1;
    
    glFlush();
    
    return 0;
}

// Plugin initialization - called when bundle is loaded
__attribute__((visibility("default")))
__attribute__((constructor))
static void VMVirtIOGLEngine_Initialize(void) {
    printf("VMVirtIOGLEngine: Plugin loaded\n");
    printf("VMVirtIOGLEngine: VirtIO GPU Hardware Renderer v1.0\n");
}

// Main entry point for CGL to get the plugin structure
__attribute__((visibility("default")))
GLEnginePlugin* CGLCreateRendererPlugin(CGLRendererInfoObj rend, GLint rendererIndex) {
    printf("VMVirtIOGLEngine: CGLCreateRendererPlugin called\n");
    return &g_plugin;
}

// Query renderer info
__attribute__((visibility("default")))
int CGLQueryRendererInfo(unsigned long display_mask, CGLRendererInfoObj* rend, GLint* nrend) {
    printf("VMVirtIOGLEngine: CGLQueryRendererInfo called\n");
    
    // Check if VirtIO GPU accelerator is available
    CFMutableDictionaryRef matching = IOServiceMatching("VMVirtIOGPUAccelerator");
    if (!matching) {
        *nrend = 0;
        return -1;
    }
    
    io_service_t service = IOServiceGetMatchingService(kIOMasterPortDefault, matching);
    if (!service) {
        *nrend = 0;
        return -1;
    }
    
    IOObjectRelease(service);
    
    // Report that we have one renderer available
    *nrend = 1;
    printf("VMVirtIOGLEngine: Reporting 1 hardware renderer available\n");
    
    return 0;
}

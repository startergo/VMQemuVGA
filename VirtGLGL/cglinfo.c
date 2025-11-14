/*
 * macOS-native OpenGL info tool (replacement for glxinfo)
 * Uses CGL (Core OpenGL) instead of GLX
 */

#include <OpenGL/OpenGL.h>
#include <OpenGL/gl.h>
#include <ApplicationServices/ApplicationServices.h>
#include <stdio.h>
#include <stdlib.h>

void printRendererInfo(CGLRendererInfoObj rend, GLint rendIndex) {
    GLint value;
    
    printf("\n--- Renderer %d ---\n", rendIndex);
    
    // Accelerated?
    if (CGLDescribeRenderer(rend, rendIndex, kCGLRPAccelerated, &value) == kCGLNoError) {
        printf("Accelerated: %s\n", value ? "YES" : "NO");
    }
    
    // Video memory
    if (CGLDescribeRenderer(rend, rendIndex, kCGLRPVideoMemory, &value) == kCGLNoError) {
        printf("Video Memory: %d MB\n", value / (1024*1024));
    }
    
    // Texture memory
    if (CGLDescribeRenderer(rend, rendIndex, kCGLRPTextureMemory, &value) == kCGLNoError) {
        printf("Texture Memory: %d MB\n", value / (1024*1024));
    }
    
    // Renderer ID
    if (CGLDescribeRenderer(rend, rendIndex, kCGLRPRendererID, &value) == kCGLNoError) {
        printf("Renderer ID: 0x%08x\n", value);
    }
    
    // Online?
    if (CGLDescribeRenderer(rend, rendIndex, kCGLRPOnline, &value) == kCGLNoError) {
        printf("Online: %s\n", value ? "YES" : "NO");
    }
}

int main(int argc, char** argv) {
    CGLRendererInfoObj rend;
    GLint nrend = 0;
    CGLError err;
    
    printf("\n===========================================\n");
    printf("macOS OpenGL Info (CGL-based)\n");
    printf("===========================================\n");
    
    // Get all renderers
    err = CGLQueryRendererInfo(0xffffffff, &rend, &nrend);
    if (err != kCGLNoError) {
        printf("Error querying renderers: %s\n", CGLErrorString(err));
        return 1;
    }
    
    printf("\nFound %d renderer(s)\n", nrend);
    
    int i;
    for (i = 0; i < nrend; i++) {
        printRendererInfo(rend, i);
    }
    
    CGLDestroyRendererInfo(rend);
    
    // Now create a context and get detailed OpenGL info
    printf("\n===========================================\n");
    printf("OpenGL Context Information\n");
    printf("===========================================\n");
    
    CGLPixelFormatAttribute attrs[] = {
        kCGLPFAAccelerated,
        kCGLPFAColorSize, (CGLPixelFormatAttribute)24,
        kCGLPFADepthSize, (CGLPixelFormatAttribute)16,
        (CGLPixelFormatAttribute)0
    };
    
    CGLPixelFormatObj pix;
    GLint npix;
    
    err = CGLChoosePixelFormat(attrs, &pix, &npix);
    if (err != kCGLNoError) {
        printf("Error choosing pixel format: %s\n", CGLErrorString(err));
        
        // Try without acceleration requirement
        printf("\nTrying without hardware acceleration requirement...\n");
        CGLPixelFormatAttribute attrs2[] = {
            kCGLPFAColorSize, (CGLPixelFormatAttribute)24,
            (CGLPixelFormatAttribute)0
        };
        err = CGLChoosePixelFormat(attrs2, &pix, &npix);
        if (err != kCGLNoError) {
            printf("Error: %s\n", CGLErrorString(err));
            return 1;
        }
    }
    
    printf("Pixel formats found: %d\n", npix);
    
    CGLContextObj ctx;
    err = CGLCreateContext(pix, NULL, &ctx);
    if (err != kCGLNoError) {
        printf("Error creating context: %s\n", CGLErrorString(err));
        CGLDestroyPixelFormat(pix);
        return 1;
    }
    
    CGLSetCurrentContext(ctx);
    
    printf("\nVendor:   %s\n", glGetString(GL_VENDOR));
    printf("Renderer: %s\n", glGetString(GL_RENDERER));
    printf("Version:  %s\n", glGetString(GL_VERSION));
    
    const char* extensions = (const char*)glGetString(GL_EXTENSIONS);
    if (extensions) {
        printf("\nExtensions: ");
        int count = 0;
        const char* p = extensions;
        while (*p) {
            if (*p == ' ') count++;
            p++;
        }
        printf("%d extensions\n", count + 1);
        
        // Print first few extensions
        printf("First extensions:\n");
        p = extensions;
        int printed = 0;
        while (*p && printed < 10) {
            const char* start = p;
            while (*p && *p != ' ') p++;
            printf("  %.*s\n", (int)(p - start), start);
            while (*p == ' ') p++;
            printed++;
        }
    }
    
    // Check for hardware acceleration
    GLint value;
    printf("\n");
    if (CGLGetParameter(ctx, kCGLCPSurfaceBackingSize, &value) == kCGLNoError) {
        printf("Surface backing size: %d\n", value);
    }
    
    CGLDestroyContext(ctx);
    CGLDestroyPixelFormat(pix);
    
    printf("\n===========================================\n");
    printf("Done\n");
    printf("===========================================\n\n");
    
    return 0;
}

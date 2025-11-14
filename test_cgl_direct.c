// Direct CGL test - bypasses GLUT to test hardware acceleration
// Compile: gcc -framework OpenGL test_cgl_direct.c -o test_cgl_direct

#include <OpenGL/OpenGL.h>
#include <OpenGL/gl.h>
#include <stdio.h>
#include <unistd.h>

int main() {
    printf("=== Direct CGL Hardware Test ===\n");
    
    // Query available renderers
    CGLRendererInfoObj rend;
    GLint nrend = 0;
    CGLError err = CGLQueryRendererInfo(0xffffffff, &rend, &nrend);
    
    printf("CGL Query Renderers: err=%d, count=%d\n", err, nrend);
    
    if (err == kCGLNoError && nrend > 0) {
        for (int i = 0; i < nrend; i++) {
            GLint renderer_id, accelerated, video_memory;
            
            CGLDescribeRenderer(rend, i, kCGLRPRendererID, &renderer_id);
            CGLDescribeRenderer(rend, i, kCGLRPAccelerated, &accelerated);
            CGLDescribeRenderer(rend, i, kCGLRPVideoMemory, &video_memory);
            
            printf("\nRenderer %d:\n", i);
            printf("  Renderer ID: 0x%08x\n", renderer_id);
            printf("  Accelerated: %s\n", accelerated ? "YES" : "NO");
            printf("  Video Memory: %d MB\n", video_memory / (1024*1024));
        }
        CGLDestroyRendererInfo(rend);
    }
    
    // Create a pixel format requesting hardware acceleration
    CGLPixelFormatAttribute attribs[] = {
        kCGLPFAAccelerated,
        kCGLPFAColorSize, 24,
        kCGLPFADepthSize, 16,
        kCGLPFADoubleBuffer,
        0
    };
    
    CGLPixelFormatObj pixelFormat;
    GLint numPixelFormats;
    
    printf("\n=== Creating Hardware Accelerated CGL Context ===\n");
    err = CGLChoosePixelFormat(attribs, &pixelFormat, &numPixelFormats);
    
    if (err != kCGLNoError) {
        printf("ERROR: CGLChoosePixelFormat failed: %d (%s)\n", err, CGLErrorString(err));
        return 1;
    }
    
    printf("✅ Pixel format created: %d formats available\n", numPixelFormats);
    
    // Create the CGL context
    CGLContextObj ctx;
    err = CGLCreateContext(pixelFormat, NULL, &ctx);
    
    if (err != kCGLNoError) {
        printf("ERROR: CGLCreateContext failed: %d (%s)\n", err, CGLErrorString(err));
        CGLDestroyPixelFormat(pixelFormat);
        return 1;
    }
    
    printf("✅ CGL Context created successfully!\n");
    printf("   Context: %p\n", ctx);
    
    // Set it as current
    err = CGLSetCurrentContext(ctx);
    if (err != kCGLNoError) {
        printf("ERROR: CGLSetCurrentContext failed: %d\n", err);
    } else {
        printf("✅ Context set as current\n");
    }
    
    // Get renderer info for this context
    GLint renderer_id;
    err = CGLGetParameter(ctx, kCGLCPCurrentRendererID, &renderer_id);
    if (err == kCGLNoError) {
        printf("\n📊 Context Renderer Info:\n");
        printf("   Renderer ID: 0x%08x\n", renderer_id);
    }
    
    // Try to get OpenGL info
    const GLubyte* vendor = glGetString(GL_VENDOR);
    const GLubyte* renderer = glGetString(GL_RENDERER);
    const GLubyte* version = glGetString(GL_VERSION);
    
    printf("\n📊 OpenGL Info:\n");
    printf("   Vendor:   %s\n", vendor ? (const char*)vendor : "NULL");
    printf("   Renderer: %s\n", renderer ? (const char*)renderer : "NULL");
    printf("   Version:  %s\n", version ? (const char*)version : "NULL");
    
    // Try a simple clear operation
    printf("\n🎨 Testing glClear()...\n");
    glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glFlush();
    printf("✅ glClear() completed\n");
    
    // Cleanup
    CGLSetCurrentContext(NULL);
    CGLDestroyContext(ctx);
    CGLDestroyPixelFormat(pixelFormat);
    
    printf("\n✅ Test completed successfully!\n");
    printf("\nCheck kernel log with: sudo dmesg | grep -i cgl\n");
    
    return 0;
}

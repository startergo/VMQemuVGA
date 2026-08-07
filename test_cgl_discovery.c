/*
 * test_cgl_discovery.c
 * Test if CGL can discover the VMVirtIOGPUAccelerator renderer
 *
 * Compile on Snow Leopard:
 *   gcc -std=c99 -framework OpenGL test_cgl_discovery.c -o test_cgl_discovery
 *
 * Run:
 *   ./test_cgl_discovery
 */

#include <OpenGL/OpenGL.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char** argv) {
    GLint i;
    CGLRendererInfoObj rend;
    GLint nrend = 0;
    CGLError err;
    
    printf("=== CGL Renderer Discovery Test ===\n\n");
    
    // Query all available renderers
    err = CGLQueryRendererInfo(0xffffffff, &rend, &nrend);
    
    if (err != kCGLNoError) {
        printf("ERROR: CGLQueryRendererInfo failed: %s\n", CGLErrorString(err));
        return 1;
    }
    
    printf("Found %d renderer(s)\n\n", nrend);
    
    // Iterate through all renderers
    for (i = 0; i < nrend; i++) {
        GLint value;
        char name[256];
        
        printf("Renderer %d:\n", i);
        
        // Get renderer ID
        err = CGLDescribeRenderer(rend, i, kCGLRPRendererID, &value);
        if (err == kCGLNoError) {
            printf("  Renderer ID: 0x%08x", value);
            if (value == 0x00024600) {
                printf(" *** VIRTIO GPU RENDERER FOUND! ***");
            }
            printf("\n");
        }
        
        // Get accelerated flag
        err = CGLDescribeRenderer(rend, i, kCGLRPAccelerated, &value);
        if (err == kCGLNoError) {
            printf("  Accelerated: %s\n", value ? "YES" : "NO");
        }
        
        // Get online flag
        err = CGLDescribeRenderer(rend, i, kCGLRPOnline, &value);
        if (err == kCGLNoError) {
            printf("  Online: %s\n", value ? "YES" : "NO");
        }
        
        // Get video memory (try old-style constant for Snow Leopard)
        err = CGLDescribeRenderer(rend, i, kCGLRPVideoMemory, &value);
        if (err == kCGLNoError) {
            printf("  Video Memory: %d MB\n", value);
        }
        
        // Get texture memory (try old-style constant for Snow Leopard)  
        err = CGLDescribeRenderer(rend, i, kCGLRPTextureMemory, &value);
        if (err == kCGLNoError) {
            printf("  Texture Memory: %d MB\n", value);
        }
        
        // Get renderer name (if available)
        err = CGLDescribeRenderer(rend, i, kCGLRPRendererID, &value);
        if (err == kCGLNoError) {
            // Try to decode renderer ID to name
            if (value == 0x00024600) {
                printf("  Name: VirtIO GPU Hardware Renderer\n");
            } else if ((value & 0x00FF0000) == 0x00020000) {
                printf("  Name: Software Renderer\n");
            } else if ((value & 0x00FF0000) == 0x00010000) {
                printf("  Name: Hardware Renderer\n");
            }
        }
        
        printf("\n");
    }
    
    CGLDestroyRendererInfo(rend);
    
    printf("=== Test Complete ===\n");
    printf("\nLooking for renderer ID 0x00024600 (VirtIO GPU)\n");
    printf("If not found, the IOAccelerator is not being discovered by CGL.\n");
    
    return 0;
}

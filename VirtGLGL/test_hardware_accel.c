// test_hardware_accel.c - Test to determine if we're getting real GPU acceleration
// Real GPU: 10,000+ FPS for clears
// Software: 300-500 FPS

#include <stdio.h>
#include <stdint.h>
#include <sys/time.h>
#include <unistd.h>
#include <IOKit/IOKitLib.h>

extern int VirtGLGL_Initialize(void);
extern void VirtGLGL_Shutdown(void);
extern void glClearColor(float red, float green, float blue, float alpha);
extern void glClear(uint32_t mask);

#define GL_COLOR_BUFFER_BIT 0x00004000

double get_time() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1000000.0;
}

int main(void)
{
    printf("=== Hardware Acceleration Detection Test ===\n\n");
    
    // Initialize
    if (!VirtGLGL_Initialize()) {
        printf("ERROR: Failed to initialize VirtGLGL\n");
        return 1;
    }
    
    printf("Running 3 benchmark tests to determine acceleration type...\n\n");
    
    // Test 1: Simple clear (should be very fast with GPU)
    printf("Test 1: Simple clear performance\n");
    glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
    
    double start = get_time();
    int frames = 0;
    while (get_time() - start < 1.0) {
        glClear(GL_COLOR_BUFFER_BIT);
        frames++;
    }
    double fps1 = frames;
    printf("  Result: %.2f FPS\n", fps1);
    
    // Test 2: Multiple color changes (CPU overhead test)
    printf("\nTest 2: Clear with color changes\n");
    start = get_time();
    frames = 0;
    while (get_time() - start < 1.0) {
        float r = (frames % 100) / 100.0f;
        glClearColor(r, 0.0f, 1.0f - r, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        frames++;
    }
    double fps2 = frames;
    printf("  Result: %.2f FPS\n", fps2);
    
    // Test 3: Rapid fire (tests command queue)
    printf("\nTest 3: Rapid command submission\n");
    start = get_time();
    frames = 0;
    for (int i = 0; i < 10000; i++) {
        glClear(GL_COLOR_BUFFER_BIT);
        frames++;
    }
    double elapsed = get_time() - start;
    double fps3 = frames / elapsed;
    printf("  Result: %.2f FPS (10000 clears in %.3f sec)\n", fps3, elapsed);
    
    // Analysis
    printf("\n=== Analysis ===\n");
    printf("Average performance: %.2f FPS\n", (fps1 + fps2 + fps3) / 3.0);
    
    if (fps3 > 5000) {
        printf("\n✓ HARDWARE ACCELERATED - Real GPU detected!\n");
        printf("  Performance indicates true GPU execution\n");
    } else if (fps3 > 2000) {
        printf("\n⚠ PARTIAL ACCELERATION - Possible GPU with overhead\n");
        printf("  Commands reach GPU but may have bottlenecks\n");
    } else if (fps3 > 500) {
        printf("\n✗ SOFTWARE RENDERING - CPU-based execution\n");
        printf("  Commands are being processed in software (virglrenderer/llvmpipe)\n");
    } else {
        printf("\n✗ VERY SLOW - Major bottleneck detected\n");
        printf("  Possible issues: Command queuing, synchronization, or driver overhead\n");
    }
    
    printf("\nExpected FPS ranges:\n");
    printf("  Hardware GPU: 10,000+ FPS\n");
    printf("  Software (llvmpipe): 300-1000 FPS\n");
    printf("  Current: %.2f FPS\n", fps3);
    
    printf("\nPossible reasons for software rendering:\n");
    printf("  1. QEMU virglrenderer using llvmpipe (CPU renderer)\n");
    printf("  2. macOS doesn't expose GPU to QEMU/UTM\n");
    printf("  3. Virgl 3D resources not properly bound\n");
    printf("  4. Missing resource transfer/flush to display\n");
    
    VirtGLGL_Shutdown();
    return 0;
}

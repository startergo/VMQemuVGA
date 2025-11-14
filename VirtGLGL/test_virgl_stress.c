// test_virgl_stress.c - Stress test to verify virgl command processing
// Sends multiple CLEAR commands with different colors to see if host processes them

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <IOKit/IOKitLib.h>
#include <CoreFoundation/CoreFoundation.h>

// VirtGLGL function prototypes
extern int VirtGLGL_Initialize(void);
extern void VirtGLGL_Shutdown(void);
extern void glClearColor(float red, float green, float blue, float alpha);
extern void glClear(uint32_t mask);

#define GL_COLOR_BUFFER_BIT 0x00004000

int main(void)
{
    printf("=== VirtGLGL Virgl Stress Test ===\n");
    printf("This test sends 100 CLEAR commands to verify host virgl processing\n\n");
    
    // Initialize
    printf("1. Initializing VirtGLGL...\n");
    if (!VirtGLGL_Initialize()) {
        printf("   ERROR: Failed to initialize\n");
        return 1;
    }
    printf("   ✓ Initialized\n\n");
    
    // Send 100 clear commands with cycling colors
    printf("2. Sending 100 virgl CLEAR commands...\n");
    for (int i = 0; i < 100; i++) {
        // Cycle through colors: Red -> Green -> Blue -> Yellow -> Cyan -> Magenta
        float red = 0.0f, green = 0.0f, blue = 0.0f;
        
        switch (i % 6) {
            case 0: red = 1.0f; break;                    // Red
            case 1: green = 1.0f; break;                  // Green
            case 2: blue = 1.0f; break;                   // Blue
            case 3: red = 1.0f; green = 1.0f; break;      // Yellow
            case 4: green = 1.0f; blue = 1.0f; break;     // Cyan
            case 5: red = 1.0f; blue = 1.0f; break;       // Magenta
        }
        
        glClearColor(red, green, blue, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        
        if ((i + 1) % 10 == 0) {
            printf("   ✓ Sent %d commands\n", i + 1);
        }
        
        // Small delay to avoid overwhelming the queue
        usleep(10000); // 10ms
    }
    
    printf("   ✓ All 100 commands sent successfully\n\n");
    
    // Final white clear
    printf("3. Final clear to white...\n");
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    printf("   ✓ Done\n\n");
    
    // Cleanup
    printf("4. Shutting down...\n");
    VirtGLGL_Shutdown();
    printf("   ✓ Complete\n\n");
    
    printf("=== Test Complete ===\n");
    printf("\nIf virglrenderer is working on the host, you should see:\n");
    printf("  • 100 VIRTIO_GPU_CMD_SUBMIT_3D commands in kernel log\n");
    printf("  • Corresponding virgl processing messages (if VIRGL_DEBUG is set)\n");
    printf("  • No kernel panics or errors\n");
    
    return 0;
}

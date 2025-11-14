// test_glclear_persistent.c - Persistent red screen test
// Continuously updates the scanout to keep the red screen visible

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <signal.h>
#include <IOKit/IOKitLib.h>
#include <CoreFoundation/CoreFoundation.h>

// OpenGL constants
#define GL_COLOR_BUFFER_BIT 0x00004000
#define GL_DEPTH_BUFFER_BIT 0x00000100

// VirtGLGL function prototypes
extern int VirtGLGL_Initialize(void);
extern void VirtGLGL_Shutdown(void);
extern void glClearColor(float red, float green, float blue, float alpha);
extern void glClear(uint32_t mask);

static volatile int g_running = 1;

void signal_handler(int sig)
{
    (void)sig;
    printf("\nReceived signal, shutting down...\n");
    g_running = 0;
}

int main(void)
{
    printf("=== VirtGLGL Persistent Red Screen Test ===\n\n");
    printf("This test will keep the red screen active until you press Ctrl+C\n\n");
    
    // Set up signal handler
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Initialize VirtGLGL
    printf("1. Initializing VirtGLGL library...\n");
    int ret = VirtGLGL_Initialize();
    if (!ret) {
        printf("   ERROR: VirtGLGL_Initialize() failed\n");
        return 1;
    }
    printf("   SUCCESS: VirtGLGL initialized\n\n");
    
    // Set clear color to red
    printf("2. Setting clear color to red (1.0, 0.0, 0.0, 1.0)...\n");
    glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
    printf("   SUCCESS: Clear color set\n\n");
    
    // Keep clearing and updating the display
    printf("3. Rendering red screen continuously...\n");
    printf("   Press Ctrl+C to stop\n\n");
    
    int frame_count = 0;
    while (g_running) {
        // Clear to red
        glClear(GL_COLOR_BUFFER_BIT);
        
        frame_count++;
        if (frame_count % 60 == 0) {
            printf("   Frame %d rendered\n", frame_count);
        }
        
        // Sleep for 16ms (~60 FPS)
        usleep(16000);
    }
    
    printf("\n4. Total frames rendered: %d\n", frame_count);
    
    // Cleanup
    printf("5. Shutting down VirtGLGL...\n");
    VirtGLGL_Shutdown();
    printf("   SUCCESS: VirtGLGL shutdown\n");
    
    printf("\n=== Test Complete ===\n");
    return 0;
}

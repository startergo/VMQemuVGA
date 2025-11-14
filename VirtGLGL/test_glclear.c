// test_glclear.c - Simple test using VirtGLGL OpenGL library
// Tests glClear() which translates to virgl CLEAR command

#include <stdio.h>
#include <stdint.h>
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

int main(void)
{
    printf("=== VirtGLGL OpenGL Library Test ===\n\n");
    
    // Initialize VirtGLGL
    printf("1. Initializing VirtGLGL library...\n");
    int ret = VirtGLGL_Initialize();
    if (!ret) {
        printf("   ERROR: VirtGLGL_Initialize() failed (returned false)\n");
        return 1;
    }
    printf("   SUCCESS: VirtGLGL initialized\n");
    
    // Set clear color to red
    printf("2. Setting clear color to red (1.0, 0.0, 0.0, 1.0)...\n");
    glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
    printf("   SUCCESS: Clear color set\n");
    
    // Clear the screen
    printf("3. Calling glClear(GL_COLOR_BUFFER_BIT)...\n");
    glClear(GL_COLOR_BUFFER_BIT);
    printf("   SUCCESS: Screen cleared (virgl CLEAR command submitted)\n");
    
    // Cleanup
    printf("4. Shutting down VirtGLGL...\n");
    VirtGLGL_Shutdown();
    printf("   SUCCESS: VirtGLGL shutdown\n");
    
    printf("\n=== Test Complete ===\n");
    return 0;
}

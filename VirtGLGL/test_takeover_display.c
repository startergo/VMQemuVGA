// test_takeover_display.c - Try to take over the display by disabling framebuffer scanout first
// Then set our own scanout and fill with color

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <IOKit/IOKitLib.h>
#include <CoreFoundation/CoreFoundation.h>

// OpenGL constants
#define GL_COLOR_BUFFER_BIT 0x00004000

// Include VirtGLGL headers
#include "VirtGLGL.h"
#include "VirtGLGLClient.h"

int main(void)
{
    printf("=== Display Takeover Test ===\n\n");
    
    // Initialize VirtGLGL
    printf("1. Initializing VirtGLGL library...\n");
    if (!VirtGLGL_Initialize()) {
        printf("   ERROR: VirtGLGL_Initialize() failed\n");
        return 1;
    }
    printf("   SUCCESS: VirtGLGL initialized (resource 2 created)\n\n");
    
    void* client = VirtGLGL_GetClient();
    if (!client) {
        printf("   ERROR: Failed to get client\n");
        return 1;
    }
    
    // Step 1: Disable framebuffer's scanout (resource 1)
    printf("2. Disabling framebuffer scanout (resource 1)...\n");
    VirtGLGL_SetScanout(client, 0, 0, 0, 0, 0, 0);  // Disable scanout
    printf("   Scanout 0 disabled\n\n");
    
    sleep(2);  // Wait to see if screen goes blank
    
    // Step 2: Enable our scanout (resource 2)
    printf("3. Enabling our scanout (resource 2)...\n");
    VirtGLGL_SetScanout(client, 0, 2, 0, 0, 800, 600);
    VirtGLGL_FlushResource(client, 2, 0, 0, 800, 600);
    printf("   Our scanout enabled\n\n");
    
    sleep(2);
    
    // Step 3: Render red color
    printf("4. Rendering red color...\n");
    glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    printf("   Red rendered\n\n");
    
    printf("5. Screen should now be RED!\n");
    printf("   Keeping it displayed for 10 seconds...\n\n");
    sleep(10);
    
    // Step 4: Re-enable framebuffer scanout
    printf("6. Re-enabling framebuffer scanout (resource 1)...\n");
    VirtGLGL_SetScanout(client, 0, 1, 0, 0, 800, 600);
    VirtGLGL_FlushResource(client, 1, 0, 0, 800, 600);
    printf("   Framebuffer scanout restored\n\n");
    
    // Cleanup
    printf("7. Shutting down...\n");
    VirtGLGL_Shutdown();
    
    printf("\n=== Test Complete ===\n");
    return 0;
}

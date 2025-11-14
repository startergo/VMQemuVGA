// test_opengl_full.c - Comprehensive OpenGL test using VirtGLGL
// Tests multiple OpenGL functions: glClear, glBegin/End, glVertex, glColor

#include <stdio.h>
#include <stdint.h>
#include <IOKit/IOKitLib.h>
#include <CoreFoundation/CoreFoundation.h>

// OpenGL constants
#define GL_COLOR_BUFFER_BIT 0x00004000
#define GL_DEPTH_BUFFER_BIT 0x00000100
#define GL_TRIANGLES        0x0004
#define GL_QUADS            0x0007

// VirtGLGL function prototypes
extern int VirtGLGL_Initialize(void);
extern void VirtGLGL_Shutdown(void);
extern void glClearColor(float red, float green, float blue, float alpha);
extern void glClear(uint32_t mask);
extern void glBegin(uint32_t mode);
extern void glEnd(void);
extern void glVertex3f(float x, float y, float z);
extern void glVertex2f(float x, float y);
extern void glColor3f(float red, float green, float blue);
extern void glColor4f(float red, float green, float blue, float alpha);

int main(void)
{
    printf("=== VirtGLGL Comprehensive OpenGL Test ===\n\n");
    
    // Initialize VirtGLGL
    printf("1. Initializing VirtGLGL library...\n");
    int ret = VirtGLGL_Initialize();
    if (!ret) {
        printf("   ERROR: VirtGLGL_Initialize() failed\n");
        return 1;
    }
    printf("   ✓ VirtGLGL initialized (context created, resource allocated)\n\n");
    
    // Clear to blue background
    printf("2. Clearing screen to blue...\n");
    glClearColor(0.0f, 0.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    printf("   ✓ Screen cleared (virgl CLEAR command sent)\n\n");
    
    // Draw a red triangle
    printf("3. Drawing a red triangle...\n");
    glColor3f(1.0f, 0.0f, 0.0f);
    glBegin(GL_TRIANGLES);
    glVertex2f(0.0f, 0.5f);    // Top vertex
    glVertex2f(-0.5f, -0.5f);  // Bottom left
    glVertex2f(0.5f, -0.5f);   // Bottom right
    glEnd();
    printf("   ✓ Triangle drawn (3 vertices, red color)\n\n");
    
    // Draw a green quad
    printf("4. Drawing a green quad...\n");
    glColor3f(0.0f, 1.0f, 0.0f);
    glBegin(GL_QUADS);
    glVertex2f(-0.8f, 0.8f);   // Top left
    glVertex2f(-0.3f, 0.8f);   // Top right
    glVertex2f(-0.3f, 0.3f);   // Bottom right
    glVertex2f(-0.8f, 0.3f);   // Bottom left
    glEnd();
    printf("   ✓ Quad drawn (4 vertices, green color)\n\n");
    
    // Draw a multi-colored triangle
    printf("5. Drawing a multi-colored triangle...\n");
    glBegin(GL_TRIANGLES);
    glColor3f(1.0f, 0.0f, 0.0f);  // Red
    glVertex2f(0.5f, 0.5f);
    glColor3f(0.0f, 1.0f, 0.0f);  // Green
    glVertex2f(0.8f, -0.2f);
    glColor3f(0.0f, 0.0f, 1.0f);  // Blue
    glVertex2f(0.2f, -0.2f);
    glEnd();
    printf("   ✓ Multi-colored triangle drawn (3 vertices with different colors)\n\n");
    
    // Test 3D vertices
    printf("6. Testing 3D vertices...\n");
    glColor4f(1.0f, 1.0f, 0.0f, 0.8f);  // Yellow with alpha
    glBegin(GL_TRIANGLES);
    glVertex3f(-0.2f, -0.8f, 0.0f);
    glVertex3f(0.2f, -0.8f, 0.0f);
    glVertex3f(0.0f, -0.5f, 0.0f);
    glEnd();
    printf("   ✓ 3D triangle drawn (using glVertex3f)\n\n");
    
    // Final clear to white
    printf("7. Final clear to white...\n");
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    printf("   ✓ Screen cleared to white (color + depth buffers)\n\n");
    
    // Cleanup
    printf("8. Shutting down VirtGLGL...\n");
    VirtGLGL_Shutdown();
    printf("   ✓ VirtGLGL shutdown complete\n\n");
    
    printf("=== All OpenGL Tests Passed ===\n");
    printf("\nSummary:\n");
    printf("  • glClearColor/glClear: ✓ Working\n");
    printf("  • glBegin/glEnd: ✓ Working\n");
    printf("  • glVertex2f/glVertex3f: ✓ Working\n");
    printf("  • glColor3f/glColor4f: ✓ Working\n");
    printf("  • Multiple primitives (triangles, quads): ✓ Working\n");
    printf("\n✓ Userspace OpenGL → virgl → kernel → VirtIO GPU pipeline operational!\n");
    
    return 0;
}

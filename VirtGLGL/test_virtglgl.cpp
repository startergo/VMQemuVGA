/*
 * test_virtglgl.cpp
 * Test program for VirtGLGL userspace OpenGL library
 */

#include "VirtGLGL.h"
#include <stdio.h>
#include <unistd.h>

int main(int argc, char** argv)
{
    printf("=== VirtGLGL Test Program ===\n\n");
    
    // Initialize library
    printf("1. Initializing VirtGLGL...\n");
    if (!VirtGLGL_Initialize()) {
        fprintf(stderr, "❌ Failed to initialize VirtGLGL\n");
        return 1;
    }
    printf("✅ VirtGLGL initialized\n\n");
    
    // Test glClearColor
    printf("2. Setting clear color to red...\n");
    glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
    printf("✅ Clear color set\n\n");
    
    // Test glClear
    printf("3. Clearing color buffer...\n");
    glClear(GL_COLOR_BUFFER_BIT);
    printf("✅ Clear command submitted\n\n");
    
    // Test immediate mode rendering
    printf("4. Drawing a triangle...\n");
    glBegin(GL_TRIANGLES);
    glColor3f(1.0f, 0.0f, 0.0f);  // Red
    glVertex2f(-0.5f, -0.5f);
    glColor3f(0.0f, 1.0f, 0.0f);  // Green
    glVertex2f(0.5f, -0.5f);
    glColor3f(0.0f, 0.0f, 1.0f);  // Blue
    glVertex2f(0.0f, 0.5f);
    glEnd();
    printf("✅ Triangle drawn\n\n");
    
    // Test flush
    printf("5. Flushing commands...\n");
    glFlush();
    printf("✅ Commands flushed\n\n");
    
    // Check for errors
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        fprintf(stderr, "⚠️  OpenGL error: 0x%x\n", err);
    } else {
        printf("✅ No OpenGL errors\n\n");
    }
    
    // Wait a bit
    printf("6. Waiting 2 seconds...\n");
    sleep(2);
    
    // Shutdown
    printf("7. Shutting down VirtGLGL...\n");
    VirtGLGL_Shutdown();
    printf("✅ VirtGLGL shutdown complete\n\n");
    
    printf("=== Test Complete ===\n");
    return 0;
}

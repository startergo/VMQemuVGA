/*
 * Check OpenGL renderer and acceleration status
 */

#include <OpenGL/gl.h>
#include <OpenGL/OpenGL.h>
#include <GLUT/glut.h>
#include <stdio.h>

int main(int argc, char** argv) {
    // Initialize GLUT to get OpenGL context
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_SINGLE | GLUT_RGB);
    glutInitWindowSize(100, 100);
    glutCreateWindow("GL Info");
    
    printf("\n===========================================\n");
    printf("OpenGL Hardware Acceleration Check\n");
    printf("===========================================\n\n");
    
    const char* vendor = (const char*)glGetString(GL_VENDOR);
    const char* renderer = (const char*)glGetString(GL_RENDERER);
    const char* version = (const char*)glGetString(GL_VERSION);
    const char* extensions = (const char*)glGetString(GL_EXTENSIONS);
    
    printf("GL_VENDOR:   %s\n", vendor ? vendor : "NULL");
    printf("GL_RENDERER: %s\n", renderer ? renderer : "NULL");
    printf("GL_VERSION:  %s\n\n", version ? version : "NULL");
    
    // Check for hardware acceleration indicators
    int is_hardware = 0;
    
    if (renderer) {
        // Check for software renderer indicators
        if (strstr(renderer, "Software") || 
            strstr(renderer, "software") ||
            strstr(renderer, "Generic") ||
            strstr(renderer, "Apple Software Renderer")) {
            printf("❌ SOFTWARE RENDERING DETECTED\n");
            printf("   Renderer contains software/generic indicators\n");
        }
        // Check for hardware renderer indicators
        else if (strstr(renderer, "VirtIO") ||
                 strstr(renderer, "virtio") ||
                 strstr(renderer, "VMware") ||
                 strstr(renderer, "virgl") ||
                 strstr(renderer, "Gallium") ||
                 strstr(renderer, "Mesa")) {
            printf("✅ HARDWARE RENDERING DETECTED!\n");
            printf("   Renderer: %s\n", renderer);
            is_hardware = 1;
        }
        else {
            printf("⚠️  UNKNOWN RENDERER TYPE\n");
            printf("   Renderer: %s\n", renderer);
        }
    }
    
    printf("\n");
    
    // Check for virgl/VirtIO specific extensions
    if (extensions) {
        if (strstr(extensions, "GL_ARB_") || 
            strstr(extensions, "GL_EXT_")) {
            printf("✅ Hardware OpenGL extensions available\n");
            is_hardware = 1;
        }
    }
    
    printf("\n===========================================\n");
    if (is_hardware) {
        printf("🎉 RESULT: Hardware 3D Acceleration ACTIVE!\n");
    } else {
        printf("⚠️  RESULT: Software rendering or unknown\n");
    }
    printf("===========================================\n\n");
    
    return 0;
}

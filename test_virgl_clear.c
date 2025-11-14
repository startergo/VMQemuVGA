/*
 * test_virgl_clear.c - Test VirtIO GPU virgl 3D acceleration
 * 
 * This program tests if our virgl clear command implementation works.
 * It should trigger VMVirtIOGPUAccelerator::submitClearCommand() which
 * sends a real virgl CLEAR command to the host GPU via virglrenderer.
 * 
 * Compile on Snow Leopard:
 *   gcc -o test_virgl_clear test_virgl_clear.c -framework OpenGL -framework GLUT
 * 
 * Run:
 *   ./test_virgl_clear
 * 
 * Expected behavior:
 *   - Window opens with solid red color (cleared by GPU)
 *   - Kernel log shows: "🚀 Using REAL VirtIO GPU 3D acceleration (virgl protocol)"
 *   - Kernel log shows: "✅ Virgl CLEAR sent to host GPU!"
 */

#include <OpenGL/gl.h>
#include <GLUT/glut.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static int frame_count = 0;
static int test_phase = 0;

void display(void) {
    frame_count++;
    
    // Cycle through different clear colors to test virgl commands
    switch (test_phase) {
        case 0:
            // Red - should trigger virgl CLEAR with (1.0, 0.0, 0.0, 1.0)
            printf("Frame %d: Clearing to RED - Testing virgl CLEAR command\n", frame_count);
            glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
            break;
        case 1:
            // Green
            printf("Frame %d: Clearing to GREEN\n", frame_count);
            glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
            break;
        case 2:
            // Blue
            printf("Frame %d: Clearing to BLUE\n", frame_count);
            glClearColor(0.0f, 0.0f, 1.0f, 1.0f);
            break;
        case 3:
            // Yellow
            printf("Frame %d: Clearing to YELLOW\n", frame_count);
            glClearColor(1.0f, 1.0f, 0.0f, 1.0f);
            break;
        case 4:
            // Magenta
            printf("Frame %d: Clearing to MAGENTA\n", frame_count);
            glClearColor(1.0f, 0.0f, 1.0f, 1.0f);
            break;
        case 5:
            // Cyan
            printf("Frame %d: Clearing to CYAN\n", frame_count);
            glClearColor(0.0f, 1.0f, 1.0f, 1.0f);
            break;
        case 6:
            // White
            printf("Frame %d: Clearing to WHITE\n", frame_count);
            glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
            break;
        case 7:
            // Black
            printf("Frame %d: Clearing to BLACK\n", frame_count);
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            break;
    }
    
    // THIS IS THE KEY CALL - should trigger our virgl implementation!
    // Path: glClear -> IOKit -> VMQemuVGAAccelerator::clearColorBuffer -> 
    //       VMVirtIOGPUAccelerator::submitClearCommand -> virgl CLEAR -> host GPU
    glClear(GL_COLOR_BUFFER_BIT);
    
    glFlush();
    glutSwapBuffers();
    
    // Change color every 60 frames (~1 second at 60fps)
    if (frame_count % 60 == 0) {
        test_phase = (test_phase + 1) % 8;
        printf("\n=== Test phase %d complete - Check kernel log for virgl messages ===\n\n", test_phase);
    }
}

void timer(int value) {
    glutPostRedisplay();
    glutTimerFunc(16, timer, 0);  // ~60 FPS
}

void keyboard(unsigned char key, int x, int y) {
    if (key == 27 || key == 'q' || key == 'Q') {  // ESC or Q to quit
        printf("\n=== Test completed after %d frames ===\n", frame_count);
        printf("Check kernel log with: sudo dmesg | grep -i virgl\n");
        printf("Expected to see:\n");
        printf("  - 🚀 Using REAL VirtIO GPU 3D acceleration (virgl protocol)\n");
        printf("  - ✅ Virgl CLEAR sent to host GPU!\n");
        exit(0);
    }
}

void reshape(int width, int height) {
    glViewport(0, 0, width, height);
}

int main(int argc, char** argv) {
    printf("╔═══════════════════════════════════════════════════════════════════╗\n");
    printf("║  VirtIO GPU Virgl 3D Acceleration Test                           ║\n");
    printf("╚═══════════════════════════════════════════════════════════════════╝\n");
    printf("\n");
    printf("This test will trigger glClear() commands that should be handled by\n");
    printf("the VirtIO GPU driver using the virgl protocol.\n");
    printf("\n");
    printf("Expected behavior:\n");
    printf("  1. Window displays solid colors (red, green, blue, etc.)\n");
    printf("  2. Colors change every ~1 second\n");
    printf("  3. Kernel log shows virgl CLEAR commands being sent\n");
    printf("\n");
    printf("To monitor in real-time (in another terminal):\n");
    printf("  sudo dmesg -w | grep -i virgl\n");
    printf("\n");
    printf("Press ESC or Q to quit and see summary\n");
    printf("═══════════════════════════════════════════════════════════════════\n\n");
    
    // Check if we're running on VirtIO GPU
    printf("Checking for VirtIO GPU device...\n");
    FILE* ioreg = popen("ioreg -l -w0 | grep -i vmvirtio", "r");
    if (ioreg) {
        char buf[256];
        int found = 0;
        while (fgets(buf, sizeof(buf), ioreg)) {
            printf("  Found: %s", buf);
            found = 1;
        }
        pclose(ioreg);
        if (!found) {
            printf("  ⚠️  WARNING: VirtIO GPU device not found in IORegistry\n");
            printf("  This test may fall back to software rendering\n");
        } else {
            printf("  ✅ VirtIO GPU device detected\n");
        }
    }
    printf("\n");
    
    // Initialize GLUT
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(640, 480);
    glutInitWindowPosition(100, 100);
    glutCreateWindow("VirtIO GPU Virgl Clear Test");
    
    // Print OpenGL info
    printf("OpenGL Information:\n");
    printf("  Vendor:   %s\n", glGetString(GL_VENDOR));
    printf("  Renderer: %s\n", glGetString(GL_RENDERER));
    printf("  Version:  %s\n", glGetString(GL_VERSION));
    printf("\n");
    
    // Check for hardware acceleration
    const char* renderer = (const char*)glGetString(GL_RENDERER);
    if (strstr(renderer, "Software") || strstr(renderer, "software")) {
        printf("⚠️  WARNING: Using SOFTWARE renderer - no GPU acceleration!\n");
        printf("VirtIO GPU driver may not be loaded or not providing acceleration\n");
    } else {
        printf("✅ Using HARDWARE renderer - GPU acceleration may be active\n");
    }
    printf("\n");
    
    printf("Starting test - watch for color changes...\n");
    printf("═══════════════════════════════════════════════════════════════════\n\n");
    
    // Set up callbacks
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutTimerFunc(0, timer, 0);
    
    // Run main loop
    glutMainLoop();
    
    return 0;
}

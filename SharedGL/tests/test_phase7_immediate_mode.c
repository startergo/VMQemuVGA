// Phase 7.2 Test: Legacy OpenGL Immediate Mode Rendering
// Tests: glBegin/glVertex/glColor/glEnd with matrix transformations
// 
// Compile (on Catalina VM):
//   gcc -arch x86_64 -o test_phase7 test_phase7_immediate_mode.c -framework OpenGL -framework GLUT
//
// Run with Metal acceleration:
//   DYLD_INSERT_LIBRARIES=~/libGLMetal.dylib ./test_phase7

#include <OpenGL/gl.h>
#include <GLUT/glut.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static float g_rotation = 0.0f;

void display(void) {
    // Clear screen
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // Set up modelview matrix with rotation
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glRotatef(g_rotation, 0.0f, 0.0f, 1.0f);  // Rotate around Z axis
    
    // Draw colored triangle using immediate mode
    glBegin(GL_TRIANGLES);
        glColor3f(1.0f, 0.0f, 0.0f);  // Red
        glVertex3f(-0.5f, -0.5f, 0.0f);
        
        glColor3f(0.0f, 1.0f, 0.0f);  // Green
        glVertex3f(0.5f, -0.5f, 0.0f);
        
        glColor3f(0.0f, 0.0f, 1.0f);  // Blue
        glVertex3f(0.0f, 0.5f, 0.0f);
    glEnd();
    
    glutSwapBuffers();
    
    // Update rotation for next frame
    g_rotation += 1.0f;
    if (g_rotation >= 360.0f) {
        g_rotation -= 360.0f;
    }
}

void reshape(int width, int height) {
    glViewport(0, 0, width, height);
    
    // Set up projection matrix
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    
    // Simple orthographic projection
    float aspect = (float)width / (float)height;
    if (width > height) {
        glOrtho(-aspect, aspect, -1.0, 1.0, -1.0, 1.0);
    } else {
        glOrtho(-1.0, 1.0, -1.0/aspect, 1.0/aspect, -1.0, 1.0);
    }
}

void timer(int value) {
    glutPostRedisplay();
    glutTimerFunc(16, timer, 0);  // ~60 FPS
}

void keyboard(unsigned char key, int x, int y) {
    if (key == 27 || key == 'q') {  // ESC or 'q'
        exit(0);
    }
}

int main(int argc, char** argv) {
    printf("==============================================\n");
    printf("  Phase 7.2 Test: Legacy OpenGL Immediate Mode\n");
    printf("==============================================\n");
    printf("Testing: glBegin/glVertex/glColor/glEnd\n");
    printf("Expected: Rotating colored triangle\n");
    printf("\n");
    printf("Controls:\n");
    printf("  ESC or 'q' - Exit\n");
    printf("\n");
    
    // Check if running with Metal acceleration
    const char* dyld = getenv("DYLD_INSERT_LIBRARIES");
    if (dyld && strstr(dyld, "libGLMetal.dylib")) {
        printf("✅ Running with Metal acceleration: %s\n", dyld);
    } else {
        printf("⚠️  Running without Metal acceleration\n");
        printf("💡 Use: DYLD_INSERT_LIBRARIES=~/libGLMetal.dylib ./test_phase7\n");
    }
    printf("\n");
    
    // Initialize GLUT
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(800, 600);
    glutInitWindowPosition(100, 100);
    glutCreateWindow("Phase 7.2: Immediate Mode Rendering");
    
    // Set up OpenGL state
    glClearColor(0.2f, 0.3f, 0.4f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    
    // Register callbacks
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutTimerFunc(16, timer, 0);
    
    printf("🚀 Starting rendering loop...\n");
    glutMainLoop();
    
    return 0;
}

/*
 * Simple OpenGL window test for Snow Leopard
 * Displays a rotating colored triangle using OpenGL
 */

#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#include <GLUT/glut.h>
#include <stdio.h>
#include <math.h>

float angle = 0.0f;

void display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();
    
    // Move back to see the triangle
    glTranslatef(0.0f, 0.0f, -5.0f);
    
    // Rotate
    glRotatef(angle, 0.0f, 1.0f, 0.0f);
    
    // Draw a colorful triangle
    glBegin(GL_TRIANGLES);
        glColor3f(1.0f, 0.0f, 0.0f);  // Red
        glVertex3f(0.0f, 1.0f, 0.0f);
        
        glColor3f(0.0f, 1.0f, 0.0f);  // Green
        glVertex3f(-1.0f, -1.0f, 0.0f);
        
        glColor3f(0.0f, 0.0f, 1.0f);  // Blue
        glVertex3f(1.0f, -1.0f, 0.0f);
    glEnd();
    
    glutSwapBuffers();
    
    // Update rotation angle
    angle += 2.0f;
    if (angle > 360.0f) {
        angle -= 360.0f;
    }
}

void reshape(int width, int height) {
    glViewport(0, 0, width, height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0, (double)width / (double)height, 0.1, 100.0);
    glMatrixMode(GL_MODELVIEW);
}

void timer(int value) {
    glutPostRedisplay();
    glutTimerFunc(16, timer, 0);  // ~60 FPS
}

void keyboard(unsigned char key, int x, int y) {
    if (key == 27 || key == 'q' || key == 'Q') {  // ESC or Q to quit
        printf("\nExiting...\n");
        exit(0);
    }
}

int main(int argc, char** argv) {
    printf("OpenGL 3D Test for Snow Leopard\n");
    printf("================================\n");
    printf("This should display a rotating colored triangle.\n");
    printf("Press ESC or Q to quit.\n\n");
    
    // Initialize GLUT
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(800, 600);
    glutCreateWindow("OpenGL 3D Test - VirtIO GPU Hardware Acceleration");
    
    // Print OpenGL info
    printf("OpenGL Vendor: %s\n", glGetString(GL_VENDOR));
    printf("OpenGL Renderer: %s\n", glGetString(GL_RENDERER));
    printf("OpenGL Version: %s\n", glGetString(GL_VERSION));
    printf("\nIf you see hardware acceleration info above, 3D is working!\n\n");
    
    // Setup OpenGL
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glShadeModel(GL_SMOOTH);
    
    // Register callbacks
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutTimerFunc(0, timer, 0);
    
    printf("Starting main loop...\n");
    printf("You should see a rotating triangle on screen.\n");
    
    // Main loop
    glutMainLoop();
    
    return 0;
}

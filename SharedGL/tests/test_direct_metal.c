// Direct Metal API test - calls our Metal functions explicitly without DYLD_INTERPOSE
// This bypasses the interposition problem and directly tests the Metal rendering pipeline

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <OpenGL/gl.h>
#include <GLUT/glut.h>

// Declare our Metal wrapper functions directly
extern void my_glBegin(GLenum mode);
extern void my_glEnd(void);
extern void my_glVertex3f(GLfloat x, GLfloat y, GLfloat z);
extern void my_glColor3f(GLfloat r, GLfloat g, GLfloat b);
extern void my_glMatrixMode(GLenum mode);
extern void my_glLoadIdentity(void);
extern void my_glRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z);

static float g_rotation = 0.0f;

void display(void) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // Use our Metal functions explicitly
    my_glMatrixMode(GL_MODELVIEW);
    my_glLoadIdentity();
    my_glRotatef(g_rotation, 0.0f, 0.0f, 1.0f);
    
    // Draw triangle using our Metal functions
    my_glBegin(GL_TRIANGLES);
        my_glColor3f(1.0f, 0.0f, 0.0f);
        my_glVertex3f(-0.5f, -0.5f, 0.0f);
        
        my_glColor3f(0.0f, 1.0f, 0.0f);
        my_glVertex3f(0.5f, -0.5f, 0.0f);
        
        my_glColor3f(0.0f, 0.0f, 1.0f);
        my_glVertex3f(0.0f, 0.5f, 0.0f);
    my_glEnd();
    
    glutSwapBuffers();
    
    g_rotation += 1.0f;
    if (g_rotation >= 360.0f) {
        g_rotation -= 360.0f;
    }
}

void reshape(int width, int height) {
    glViewport(0, 0, width, height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    
    float aspect = (float)width / (float)height;
    if (width > height) {
        glOrtho(-aspect, aspect, -1.0, 1.0, -1.0, 1.0);
    } else {
        glOrtho(-1.0, 1.0, -1.0/aspect, 1.0/aspect, -1.0, 1.0);
    }
}

void timer(int value) {
    glutPostRedisplay();
    glutTimerFunc(16, timer, 0);
}

void keyboard(unsigned char key, int x, int y) {
    if (key == 27 || key == 'q') {
        exit(0);
    }
}

int main(int argc, char** argv) {
    printf("==============================================\n");
    printf("  Direct Metal API Test\n");
    printf("==============================================\n");
    printf("This test calls my_glBegin/my_glVertex/my_glEnd\n");
    printf("directly (not via DYLD_INTERPOSE)\n");
    printf("\n");
    
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(800, 600);
    glutInitWindowPosition(100, 100);
    glutCreateWindow("Direct Metal API Test");
    
    glClearColor(0.2f, 0.3f, 0.4f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutTimerFunc(16, timer, 0);
    
    printf("🚀 Starting rendering loop (calling Metal functions directly)...\n");
    glutMainLoop();
    
    return 0;
}

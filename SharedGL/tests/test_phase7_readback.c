// Test Phase 7.6: Pixel Readback
// Draws on Metal server, reads pixels back, displays in VM window

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <OpenGL/gl.h>
#include <GLUT/glut.h>

// Declare my_gl* functions
extern void my_glMatrixMode(GLenum mode);
extern void my_glLoadIdentity(void);
extern void my_glRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z);
extern void my_glBegin(GLenum mode);
extern void my_glEnd(void);
extern void my_glColor3f(GLfloat r, GLfloat g, GLfloat b);
extern void my_glVertex3f(GLfloat x, GLfloat y, GLfloat z);
extern void my_glFlush(void);
extern void my_glReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, 
                            GLenum format, GLenum type, GLvoid *pixels);

#define GL_MODELVIEW 0x1700
#define GL_TRIANGLES 0x0004

// Window size
#define WIDTH 800
#define HEIGHT 600

// Pixel buffer
unsigned char *pixelBuffer = NULL;
float rotation = 0.0f;

void renderFrame() {
    printf("[Readback Test] Frame - Rotation: %.1f degrees\n", rotation);
    
    // 1. Draw to Metal server (offscreen)
    my_glMatrixMode(GL_MODELVIEW);
    my_glLoadIdentity();
    my_glRotatef(rotation, 0.0f, 0.0f, 1.0f);
    
    my_glBegin(GL_TRIANGLES);
        my_glColor3f(1.0f, 0.0f, 0.0f);  // Red
        my_glVertex3f(-0.5f, -0.5f, 0.0f);
        
        my_glColor3f(0.0f, 1.0f, 0.0f);  // Green
        my_glVertex3f(0.5f, -0.5f, 0.0f);
        
        my_glColor3f(0.0f, 0.0f, 1.0f);  // Blue
        my_glVertex3f(0.0f, 0.5f, 0.0f);
    my_glEnd();
    
    // 2. Read pixels back from Metal server
    my_glReadPixels(0, 0, WIDTH, HEIGHT, GL_RGBA, GL_UNSIGNED_BYTE, pixelBuffer);
    
    // 3. Display pixels in local OpenGL window
    glDrawPixels(WIDTH, HEIGHT, GL_RGBA, GL_UNSIGNED_BYTE, pixelBuffer);
    
    glutSwapBuffers();
    
    rotation += 5.0f;
    if (rotation >= 360.0f) rotation = 0.0f;
}

void display() {
    glClear(GL_COLOR_BUFFER_BIT);
    renderFrame();
}

void timer(int value) {
    glutPostRedisplay();
    glutTimerFunc(16, timer, 0);  // ~60 FPS
}

int main(int argc, char** argv) {
    printf("========================================\n");
    printf("  Phase 7.6: Pixel Readback Test\n");
    printf("========================================\n");
    printf("Render on M4 Pro → Read pixels → Display in VM\n\n");
    
    // Allocate pixel buffer
    pixelBuffer = (unsigned char*)malloc(WIDTH * HEIGHT * 4);
    if (!pixelBuffer) {
        fprintf(stderr, "Failed to allocate pixel buffer\n");
        return 1;
    }
    
    // Initialize GLUT (for window display only)
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA);
    glutInitWindowSize(WIDTH, HEIGHT);
    glutCreateWindow("Phase 7.6: Metal GPU Rendering");
    
    glutDisplayFunc(display);
    glutTimerFunc(16, timer, 0);
    
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    
    printf("✅ Window created - rendering with Metal GPU\n");
    printf("Watch for rotating triangle rendered on M4 Pro!\n\n");
    
    glutMainLoop();
    
    free(pixelBuffer);
    return 0;
}

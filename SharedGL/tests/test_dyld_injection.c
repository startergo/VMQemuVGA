// Simple OpenGL test - will be injected with DYLD_INSERT_LIBRARIES
#include <stdio.h>
#include <OpenGL/gl.h>
#include <GLUT/glut.h>

void display() {
    glClear(GL_COLOR_BUFFER_BIT);
    
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    glBegin(GL_TRIANGLES);
        glColor3f(1.0f, 0.0f, 0.0f);
        glVertex3f(-0.5f, -0.5f, 0.0f);
        
        glColor3f(0.0f, 1.0f, 0.0f);
        glVertex3f(0.5f, -0.5f, 0.0f);
        
        glColor3f(0.0f, 0.0f, 1.0f);
        glVertex3f(0.0f, 0.5f, 0.0f);
    glEnd();
    
    glutSwapBuffers();
    glutPostRedisplay();
}

int main(int argc, char** argv) {
    printf("Unmodified GLUT app - testing DYLD_INSERT_LIBRARIES injection\n");
    
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA);
    glutInitWindowSize(800, 600);
    glutCreateWindow("DYLD Injection Test");
    glutDisplayFunc(display);
    
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    
    glutMainLoop();
    return 0;
}

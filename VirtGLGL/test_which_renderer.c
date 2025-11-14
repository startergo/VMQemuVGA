/*
 * Test that explicitly shows which renderer is being used
 * Prints GL_RENDERER while actually rendering
 */

#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#include <GLUT/glut.h>
#include <stdio.h>

void display() {
    static int first_time = 1;
    
    if (first_time) {
        first_time = 0;
        
        printf("\n========================================\n");
        printf("ACTIVE RENDERER INFORMATION:\n");
        printf("========================================\n");
        printf("GL_VENDOR:   %s\n", glGetString(GL_VENDOR));
        printf("GL_RENDERER: %s\n", glGetString(GL_RENDERER));
        printf("GL_VERSION:  %s\n", glGetString(GL_VERSION));
        printf("========================================\n");
        
        const char* renderer = (const char*)glGetString(GL_RENDERER);
        if (strstr(renderer, "Software") || strstr(renderer, "software")) {
            printf("\n❌ USING SOFTWARE RENDERING\n");
            printf("This means hardware acceleration is NOT working.\n\n");
        } else if (strstr(renderer, "VirtIO") || strstr(renderer, "virtio") || 
                   strstr(renderer, "Hardware")) {
            printf("\n✅ USING HARDWARE RENDERING!\n");
            printf("Hardware acceleration is working!\n\n");
        } else {
            printf("\n⚠️  UNKNOWN RENDERER\n\n");
        }
    }
    
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();
    glTranslatef(0.0f, 0.0f, -5.0f);
    
    glBegin(GL_TRIANGLES);
        glColor3f(1.0f, 0.0f, 0.0f);
        glVertex3f(0.0f, 1.0f, 0.0f);
        glColor3f(0.0f, 1.0f, 0.0f);
        glVertex3f(-1.0f, -1.0f, 0.0f);
        glColor3f(0.0f, 0.0f, 1.0f);
        glVertex3f(1.0f, -1.0f, 0.0f);
    glEnd();
    
    glutSwapBuffers();
}

void reshape(int width, int height) {
    glViewport(0, 0, width, height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0, (double)width / (double)height, 0.1, 100.0);
    glMatrixMode(GL_MODELVIEW);
}

int main(int argc, char** argv) {
    printf("Testing which OpenGL renderer is actually being used...\n");
    
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(400, 400);
    glutCreateWindow("Renderer Test");
    
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutIdleFunc(display);
    
    glutMainLoop();
    return 0;
}

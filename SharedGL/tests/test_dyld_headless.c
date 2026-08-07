// Headless OpenGL test - no GLUT, no window
// Tests DYLD_INSERT_LIBRARIES injection with fishhook
#include <stdio.h>
#include <OpenGL/gl.h>
#include <unistd.h>

int main(int argc, char** argv) {
    printf("🚀 Headless OpenGL test - testing DYLD_INSERT_LIBRARIES injection\n");
    printf("✅ fishhook should intercept OpenGL calls and send to Metal server\n\n");
    
    // Initialize clear color
    glClearColor(0.2f, 0.3f, 0.4f, 1.0f);
    printf("Called glClearColor()\n");
    
    // Set up projection matrix
    glMatrixMode(GL_PROJECTION);
    printf("Called glMatrixMode(GL_PROJECTION)\n");
    
    glLoadIdentity();
    printf("Called glLoadIdentity()\n");
    
    // Set up modelview matrix
    glMatrixMode(GL_MODELVIEW);
    printf("Called glMatrixMode(GL_MODELVIEW)\n");
    
    glLoadIdentity();
    printf("Called glLoadIdentity()\n");
    
    // Render 5 frames with rotating triangles
    for (int frame = 0; frame < 5; frame++) {
        printf("\n[Frame %d]\n", frame);
        
        glClear(GL_COLOR_BUFFER_BIT);
        printf("  glClear()\n");
        
        glRotatef(10.0f, 0.0f, 0.0f, 1.0f);
        printf("  glRotatef(10.0)\n");
        
        glBegin(GL_TRIANGLES);
        printf("  glBegin(GL_TRIANGLES)\n");
        
        glColor3f(1.0f, 0.0f, 0.0f);
        glVertex3f(-0.5f, -0.5f, 0.0f);
        
        glColor3f(0.0f, 1.0f, 0.0f);
        glVertex3f(0.5f, -0.5f, 0.0f);
        
        glColor3f(0.0f, 0.0f, 1.0f);
        glVertex3f(0.0f, 0.5f, 0.0f);
        
        glEnd();
        printf("  glEnd() - 3 vertices sent\n");
        
        glFlush();
        printf("  glFlush()\n");
        
        usleep(100000); // 100ms between frames
    }
    
    printf("\n✅ Test complete - sent 5 frames to Metal server\n");
    printf("Check Metal server logs for '🎯 FIXED_FUNCTION_DRAW detected!'\n");
    
    return 0;
}

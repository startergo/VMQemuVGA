// Ultra-simple OpenGL test WITHOUT GLUT
// Just calls a few OpenGL functions and exits
#include <stdio.h>
#include <OpenGL/gl.h>

int main() {
    printf("Simple OpenGL test - no GLUT, no window\n");
    
    // These won't actually render anything (no context), 
    // but will trigger our hooks
    printf("Calling glMatrixMode...\n");
    glMatrixMode(GL_MODELVIEW);
    
    printf("Calling glLoadIdentity...\n");
    glLoadIdentity();
    
    printf("Calling glClearColor...\n");
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    
    printf("✅ All GL calls succeeded\n");
    return 0;
}

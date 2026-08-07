// Direct Metal API test - calls our functions explicitly (no DYLD_INTERPOSE)
// This tests if the Metal server pipeline actually works

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// Declare our Metal functions directly
extern void my_glBegin(unsigned int mode);
extern void my_glEnd(void);
extern void my_glVertex3f(float x, float y, float z);
extern void my_glColor3f(float r, float g, float b);
extern void my_glMatrixMode(unsigned int mode);
extern void my_glLoadIdentity(void);
extern void my_glRotatef(float angle, float x, float y, float z);

#define GL_TRIANGLES 0x0004
#define GL_MODELVIEW 0x1700

int main() {
    printf("========================================\n");
    printf("  Direct Metal API Test (No Interpose)\n");
    printf("========================================\n");
    printf("Testing: Explicit calls to my_gl* functions\n");
    printf("This verifies the Metal server works\n\n");
    
    // Draw 10 frames of rotating triangles
    for (int frame = 0; frame < 10; frame++) {
        float rotation = frame * 36.0f;
        
        printf("[Frame %d] Rotation: %.1f degrees\n", frame, rotation);
        fflush(stdout);
        
        printf("  Calling my_glMatrixMode...\n");
        fflush(stdout);
        my_glMatrixMode(GL_MODELVIEW);
        
        printf("  Calling my_glLoadIdentity...\n");
        fflush(stdout);
        my_glLoadIdentity();
        
        printf("  Calling my_glRotatef...\n");
        fflush(stdout);
        my_glRotatef(rotation, 0.0f, 0.0f, 1.0f);
        
        // Draw triangle
        printf("  Calling my_glBegin...\n");
        fflush(stdout);
        my_glBegin(GL_TRIANGLES);
        
        printf("  Calling my_glColor3f (red)...\n");
        fflush(stdout);
        my_glColor3f(1.0f, 0.0f, 0.0f);  // Red
        
        printf("  Calling my_glVertex3f (1)...\n");
        fflush(stdout);
        my_glVertex3f(-0.5f, -0.5f, 0.0f);
        
        printf("  Calling my_glColor3f (green)...\n");
        fflush(stdout);
        my_glColor3f(0.0f, 1.0f, 0.0f);  // Green
        
        printf("  Calling my_glVertex3f (2)...\n");
        fflush(stdout);
        my_glVertex3f(0.5f, -0.5f, 0.0f);
        
        printf("  Calling my_glColor3f (blue)...\n");
        fflush(stdout);
        my_glColor3f(0.0f, 0.0f, 1.0f);  // Blue
        
        printf("  Calling my_glVertex3f (3)...\n");
        fflush(stdout);
        my_glVertex3f(0.0f, 0.5f, 0.0f);
        
        printf("  Calling my_glEnd...\n");
        fflush(stdout);
        my_glEnd();
        
        printf("  Frame %d complete!\n\n", frame);
        fflush(stdout);
        
        usleep(100000); // 100ms delay
    }
    
    printf("\n✅ Test complete! Check Metal server logs.\n");
    printf("If you see 'Fixed-function draw' messages, the pipeline works!\n");
    
    return 0;
}

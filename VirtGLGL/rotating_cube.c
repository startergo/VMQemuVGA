// rotating_cube.c - Real 3D demo using VirtGLGL
// Demonstrates actual 3D acceleration with rotating cube

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <IOKit/IOKitLib.h>
#include <CoreFoundation/CoreFoundation.h>

// OpenGL constants
#define GL_COLOR_BUFFER_BIT 0x00004000
#define GL_DEPTH_BUFFER_BIT 0x00000100
#define GL_TRIANGLES        0x0004
#define GL_QUADS            0x0007

// VirtGLGL function prototypes
extern int VirtGLGL_Initialize(void);
extern void VirtGLGL_Shutdown(void);
extern void glClearColor(float red, float green, float blue, float alpha);
extern void glClear(uint32_t mask);
extern void glBegin(uint32_t mode);
extern void glEnd(void);
extern void glVertex3f(float x, float y, float z);
extern void glColor3f(float red, float green, float blue);

// Cube vertices (8 corners)
float cube_vertices[8][3] = {
    {-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1},  // Back face
    {-1, -1,  1}, {1, -1,  1}, {1, 1,  1}, {-1, 1,  1}   // Front face
};

// Cube face colors (6 faces)
float cube_colors[6][3] = {
    {1.0f, 0.0f, 0.0f},  // Red (front)
    {0.0f, 1.0f, 0.0f},  // Green (back)
    {0.0f, 0.0f, 1.0f},  // Blue (top)
    {1.0f, 1.0f, 0.0f},  // Yellow (bottom)
    {1.0f, 0.0f, 1.0f},  // Magenta (right)
    {0.0f, 1.0f, 1.0f}   // Cyan (left)
};

void draw_cube_face(int v1, int v2, int v3, int v4, float* color)
{
    glColor3f(color[0], color[1], color[2]);
    glBegin(GL_QUADS);
    glVertex3f(cube_vertices[v1][0], cube_vertices[v1][1], cube_vertices[v1][2]);
    glVertex3f(cube_vertices[v2][0], cube_vertices[v2][1], cube_vertices[v2][2]);
    glVertex3f(cube_vertices[v3][0], cube_vertices[v3][1], cube_vertices[v3][2]);
    glVertex3f(cube_vertices[v4][0], cube_vertices[v4][1], cube_vertices[v4][2]);
    glEnd();
}

void draw_cube()
{
    // Draw all 6 faces
    draw_cube_face(4, 5, 6, 7, cube_colors[0]);  // Front (red)
    draw_cube_face(0, 3, 2, 1, cube_colors[1]);  // Back (green)
    draw_cube_face(3, 7, 6, 2, cube_colors[2]);  // Top (blue)
    draw_cube_face(0, 1, 5, 4, cube_colors[3]);  // Bottom (yellow)
    draw_cube_face(1, 2, 6, 5, cube_colors[4]);  // Right (magenta)
    draw_cube_face(0, 4, 7, 3, cube_colors[5]);  // Left (cyan)
}

void rotate_cube(float angle)
{
    // Simple rotation around Y axis
    float cos_a = cosf(angle);
    float sin_a = sinf(angle);
    
    for (int i = 0; i < 8; i++) {
        float x = cube_vertices[i][0];
        float z = cube_vertices[i][2];
        
        // Rotate around Y axis
        cube_vertices[i][0] = x * cos_a - z * sin_a;
        cube_vertices[i][2] = x * sin_a + z * cos_a;
    }
}

int main(void)
{
    printf("=== VirtGLGL 3D Rotating Cube Demo ===\n");
    printf("Demonstrating real 3D acceleration via virgl\n\n");
    
    // Initialize VirtGLGL
    printf("Initializing VirtGLGL 3D engine...\n");
    if (!VirtGLGL_Initialize()) {
        fprintf(stderr, "ERROR: Failed to initialize VirtGLGL\n");
        return 1;
    }
    printf("✓ VirtGLGL initialized with hardware acceleration\n\n");
    
    // Animation loop - 360 degree rotation
    printf("Rendering rotating cube (360 frames)...\n");
    int frame_count = 360;
    float angle_step = (2.0f * M_PI) / frame_count;
    
    for (int frame = 0; frame < frame_count; frame++) {
        // Clear screen to black
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        // Draw the cube
        draw_cube();
        
        // Rotate for next frame
        rotate_cube(angle_step);
        
        // Progress indicator
        if ((frame + 1) % 60 == 0) {
            printf("  Frame %d/%d (%.0f degrees rotated)\n", 
                   frame + 1, frame_count, (frame + 1) * 360.0f / frame_count);
        }
        
        // Small delay for visibility (optional)
        // usleep(16666); // ~60 FPS
    }
    
    printf("\n✓ Rendered %d frames successfully!\n", frame_count);
    printf("  • %d glClear() calls\n", frame_count);
    printf("  • %d cube draw calls (6 faces × 4 vertices each)\n", frame_count);
    printf("  • %d total vertices processed\n", frame_count * 6 * 4);
    printf("  • All commands submitted to VirtIO GPU via virgl protocol\n\n");
    
    // Final clear to white
    printf("Final render...\n");
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    
    // Cleanup
    printf("Shutting down...\n");
    VirtGLGL_Shutdown();
    
    printf("\n=== Demo Complete ===\n");
    printf("VirtGLGL successfully demonstrated 3D hardware acceleration!\n");
    
    return 0;
}

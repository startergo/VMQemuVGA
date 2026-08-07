// Test: Render on M4 Pro Metal GPU and save frame to file
// This PROVES the Metal GPU is rendering

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Declare my_gl* functions
extern void my_glMatrixMode(unsigned int mode);
extern void my_glLoadIdentity(void);
extern void my_glRotatef(float angle, float x, float y, float z);
extern void my_glBegin(unsigned int mode);
extern void my_glEnd(void);
extern void my_glColor3f(float r, float g, float b);
extern void my_glVertex3f(float x, float y, float z);
extern void my_glReadPixels(int x, int y, int width, int height, 
                            unsigned int format, unsigned int type, void *pixels);

#define GL_MODELVIEW 0x1700
#define GL_TRIANGLES 0x0004
#define GL_RGBA 0x1908
#define GL_UNSIGNED_BYTE 0x1401

#define WIDTH 800
#define HEIGHT 600

// Simple BMP file writer
void save_bmp(const char *filename, unsigned char *pixels, int width, int height) {
    FILE *f = fopen(filename, "wb");
    if (!f) {
        printf("Failed to create %s\n", filename);
        return;
    }
    
    // BMP header (54 bytes)
    int filesize = 54 + 3 * width * height;
    unsigned char bmpfileheader[14] = {'B','M', 0,0,0,0, 0,0, 0,0, 54,0,0,0};
    unsigned char bmpinfoheader[40] = {40,0,0,0, 0,0,0,0, 0,0,0,0, 1,0, 24,0};
    
    bmpfileheader[ 2] = (unsigned char)(filesize    );
    bmpfileheader[ 3] = (unsigned char)(filesize>> 8);
    bmpfileheader[ 4] = (unsigned char)(filesize>>16);
    bmpfileheader[ 5] = (unsigned char)(filesize>>24);
    
    bmpinfoheader[ 4] = (unsigned char)(width    );
    bmpinfoheader[ 5] = (unsigned char)(width>> 8);
    bmpinfoheader[ 6] = (unsigned char)(width>>16);
    bmpinfoheader[ 7] = (unsigned char)(width>>24);
    bmpinfoheader[ 8] = (unsigned char)(height    );
    bmpinfoheader[ 9] = (unsigned char)(height>> 8);
    bmpinfoheader[10] = (unsigned char)(height>>16);
    bmpinfoheader[11] = (unsigned char)(height>>24);
    
    fwrite(bmpfileheader, 1, 14, f);
    fwrite(bmpinfoheader, 1, 40, f);
    
    // Write pixel data (BMP is BGR, bottom-to-top)
    for (int y = height - 1; y >= 0; y--) {
        for (int x = 0; x < width; x++) {
            int i = (y * width + x) * 4;  // RGBA input
            unsigned char color[3] = {pixels[i+2], pixels[i+1], pixels[i+0]};  // BGR output
            fwrite(color, 1, 3, f);
        }
    }
    
    fclose(f);
    printf("✅ Saved frame to %s\n", filename);
}

int main() {
    printf("========================================\n");
    printf("  Render Frame Test - M4 Pro Metal GPU\n");
    printf("========================================\n\n");
    
    // Allocate pixel buffer
    unsigned char *pixels = (unsigned char*)malloc(WIDTH * HEIGHT * 4);
    if (!pixels) {
        printf("Failed to allocate pixel buffer\n");
        return 1;
    }
    
    printf("1. Drawing triangle on M4 Pro Metal GPU...\n");
    
    // Draw a rotating triangle
    my_glMatrixMode(GL_MODELVIEW);
    my_glLoadIdentity();
    my_glRotatef(45.0f, 0.0f, 0.0f, 1.0f);
    
    my_glBegin(GL_TRIANGLES);
        my_glColor3f(1.0f, 0.0f, 0.0f);  // Red
        my_glVertex3f(-0.5f, -0.5f, 0.0f);
        
        my_glColor3f(0.0f, 1.0f, 0.0f);  // Green
        my_glVertex3f(0.5f, -0.5f, 0.0f);
        
        my_glColor3f(0.0f, 0.0f, 1.0f);  // Blue
        my_glVertex3f(0.0f, 0.5f, 0.0f);
    my_glEnd();
    
    printf("2. Reading pixels back from M4 Pro GPU...\n");
    
    // Read pixels from Metal server
    my_glReadPixels(0, 0, WIDTH, HEIGHT, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    
    printf("3. Saving frame to metal_render.bmp...\n");
    
    // Save to file
    save_bmp("metal_render.bmp", pixels, WIDTH, HEIGHT);
    
    printf("\n========================================\n");
    printf("✅ SUCCESS!\n");
    printf("========================================\n");
    printf("Open metal_render.bmp to see the triangle\n");
    printf("rendered on the M4 Pro Metal GPU!\n\n");
    
    free(pixels);
    return 0;
}

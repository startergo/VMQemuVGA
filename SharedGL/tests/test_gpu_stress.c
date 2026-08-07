// GPU Stress Test - Render many triangles to show GPU usage
// This will keep the M4 Pro GPU busy enough to see in Activity Monitor

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>

// Declare my_gl* functions
extern void my_glClear(unsigned int mask);
extern void my_glMatrixMode(unsigned int mode);
extern void my_glLoadIdentity(void);
extern void my_glRotatef(float angle, float x, float y, float z);
extern void my_glBegin(unsigned int mode);
extern void my_glEnd(void);
extern void my_glColor3f(float r, float g, float b);
extern void my_glVertex3f(float x, float y, float z);
extern void my_glReadPixels(int x, int y, int width, int height, 
                            unsigned int format, unsigned int type, void *pixels);

#define GL_COLOR_BUFFER_BIT 0x00004000
#define GL_MODELVIEW 0x1700
#define GL_TRIANGLES 0x0004
#define GL_RGBA 0x1908
#define GL_UNSIGNED_BYTE 0x1401

#define WIDTH 800
#define HEIGHT 600

// Simple BMP writer
void save_bmp(const char *filename, unsigned char *pixels, int width, int height) {
    FILE *f = fopen(filename, "wb");
    if (!f) return;
    
    int filesize = 54 + 3 * width * height;
    unsigned char bmpfileheader[14] = {'B','M', 0,0,0,0, 0,0, 0,0, 54,0,0,0};
    unsigned char bmpinfoheader[40] = {40,0,0,0, 0,0,0,0, 0,0,0,0, 1,0, 24,0};
    
    bmpfileheader[2] = (unsigned char)(filesize);
    bmpfileheader[3] = (unsigned char)(filesize>>8);
    bmpfileheader[4] = (unsigned char)(filesize>>16);
    bmpfileheader[5] = (unsigned char)(filesize>>24);
    
    bmpinfoheader[4] = (unsigned char)(width);
    bmpinfoheader[5] = (unsigned char)(width>>8);
    bmpinfoheader[6] = (unsigned char)(width>>16);
    bmpinfoheader[7] = (unsigned char)(width>>24);
    bmpinfoheader[8] = (unsigned char)(height);
    bmpinfoheader[9] = (unsigned char)(height>>8);
    bmpinfoheader[10] = (unsigned char)(height>>16);
    bmpinfoheader[11] = (unsigned char)(height>>24);
    
    fwrite(bmpfileheader, 1, 14, f);
    fwrite(bmpinfoheader, 1, 40, f);
    
    for (int y = height - 1; y >= 0; y--) {
        for (int x = 0; x < width; x++) {
            int i = (y * width + x) * 4;
            unsigned char color[3] = {pixels[i+2], pixels[i+1], pixels[i+0]};
            fwrite(color, 1, 3, f);
        }
    }
    fclose(f);
}

int main() {
    printf("========================================\n");
    printf("  GPU Stress Test - M4 Pro Metal\n");
    printf("========================================\n");
    printf("Rendering 1000 triangles to stress GPU\n");
    printf("Watch Activity Monitor → GPU History!\n\n");
    
    unsigned char *pixels = (unsigned char*)malloc(WIDTH * HEIGHT * 4);
    if (!pixels) return 1;
    
    int totalFrames = 100;
    int trianglesPerFrame = 100;
    
    printf("Rendering %d frames × %d triangles = %d total triangles\n\n", 
           totalFrames, trianglesPerFrame, totalFrames * trianglesPerFrame);
    
    for (int frame = 0; frame < totalFrames; frame++) {
        printf("\r[Frame %d/%d] GPU rendering...", frame + 1, totalFrames);
        fflush(stdout);
        
        // Clear to black
        my_glClear(GL_COLOR_BUFFER_BIT);
        
        // Draw many triangles
        for (int i = 0; i < trianglesPerFrame; i++) {
            float angle = (frame * 3.6f) + (i * 3.6f);
            float scale = 0.1f + (i % 10) * 0.05f;
            float offsetX = ((i % 10) - 5) * 0.18f;
            float offsetY = ((i / 10) - 5) * 0.18f;
            
            my_glMatrixMode(GL_MODELVIEW);
            my_glLoadIdentity();
            my_glRotatef(angle, 0.0f, 0.0f, 1.0f);
            
            my_glBegin(GL_TRIANGLES);
                float r = (float)(i % 3 == 0);
                float g = (float)(i % 3 == 1);
                float b = (float)(i % 3 == 2);
                
                my_glColor3f(r, g, b);
                my_glVertex3f(offsetX - scale, offsetY - scale, 0.0f);
                my_glColor3f(g, b, r);
                my_glVertex3f(offsetX + scale, offsetY - scale, 0.0f);
                my_glColor3f(b, r, g);
                my_glVertex3f(offsetX, offsetY + scale, 0.0f);
            my_glEnd();
        }
        
        // Small delay between frames
        usleep(10000);  // 10ms
    }
    
    printf("\n\nReading final frame from GPU...\n");
    my_glReadPixels(0, 0, WIDTH, HEIGHT, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    
    printf("Saving to gpu_stress_test.bmp...\n");
    save_bmp("gpu_stress_test.bmp", pixels, WIDTH, HEIGHT);
    
    printf("\n========================================\n");
    printf("✅ Rendered %d triangles on M4 Pro GPU!\n", totalFrames * trianglesPerFrame);
    printf("========================================\n");
    printf("Check Activity Monitor for GPU usage spike\n");
    printf("Open gpu_stress_test.bmp to see result\n\n");
    
    free(pixels);
    return 0;
}

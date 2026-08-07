// Phase 7.6 Test: VM Display Integration
// Tests rendering on M4 Pro GPU with pixel readback to VM
//
// This demonstrates the complete README workflow:
// 1. App runs in VM (unmodified)
// 2. OpenGL calls intercepted by libGLMetal.dylib
// 3. Commands sent to metal_server on host
// 4. M4 Pro GPU renders
// 5. Pixels sent back to VM for display
//
// Compile (x86_64 for Catalina VM):
//   gcc -arch x86_64 -o test_vm_display test_vm_display.c
//
// Run with Metal acceleration:
//   DYLD_INSERT_LIBRARIES=~/libGLMetal.dylib ./test_vm_display

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdint.h>
#include <math.h>

#define HOST_IP "127.0.0.1"
#define HOST_PORT 28123

// Command opcodes (matching metal_server.m)
typedef enum {
    CMD_METAL_CLEAR = 6,
    CMD_METAL_FIXED_FUNCTION_DRAW = 100,
    CMD_METAL_READ_PIXELS = 110,
    CMD_METAL_SWAP_BUFFERS = 111
} MetalCommand;

// OpenGL constants
#define GL_TRIANGLES 0x0004
#define GL_RGBA 0x1908
#define GL_UNSIGNED_BYTE 0x1401

int g_socket = -1;

int connect_to_server(void) {
    if (g_socket >= 0) return 1;
    
    printf("[Test] Connecting to Metal server %s:%d...\n", HOST_IP, HOST_PORT);
    
    g_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (g_socket < 0) {
        printf("[Test] ❌ Failed to create socket\n");
        return 0;
    }
    
    struct sockaddr_in serverAddr = {0};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(HOST_PORT);
    inet_pton(AF_INET, HOST_IP, &serverAddr.sin_addr);
    
    if (connect(g_socket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        printf("[Test] ❌ Failed to connect to server\n");
        close(g_socket);
        g_socket = -1;
        return 0;
    }
    
    printf("[Test] ✅ Connected to Metal server\n");
    return 1;
}

void send_clear(float r, float g, float b, float a) {
    uint32_t cmd = CMD_METAL_CLEAR;
    write(g_socket, &cmd, sizeof(cmd));
    
    float clearColor[4] = {r, g, b, a};
    write(g_socket, clearColor, sizeof(clearColor));
    
    printf("[Test] Sent CLEAR command (%.2f, %.2f, %.2f, %.2f)\n", r, g, b, a);
}

void send_triangle(float rotation) {
    // Build rotation matrix (rotate around Z axis)
    float c = cosf(rotation);
    float s = sinf(rotation);
    
    float modelview[16] = {
        c, s, 0, 0,
        -s, c, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    };
    
    // Orthographic projection
    float projection[16] = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    };
    
    // Triangle vertices (position + color + normal + texcoord = 48 bytes each)
    float vertices[] = {
        // Vertex 1: Red (bottom-left)
        -0.5f, -0.5f, 0.0f,     // position
        1.0f, 0.0f, 0.0f, 1.0f, // color (red)
        0.0f, 0.0f, 1.0f,       // normal
        0.0f, 0.0f,             // texcoord
        
        // Vertex 2: Green (bottom-right)
        0.5f, -0.5f, 0.0f,      // position
        0.0f, 1.0f, 0.0f, 1.0f, // color (green)
        0.0f, 0.0f, 1.0f,       // normal
        1.0f, 0.0f,             // texcoord
        
        // Vertex 3: Blue (top)
        0.0f, 0.5f, 0.0f,       // position
        0.0f, 0.0f, 1.0f, 1.0f, // color (blue)
        0.0f, 0.0f, 1.0f,       // normal
        0.5f, 1.0f              // texcoord
    };
    
    uint32_t cmd = CMD_METAL_FIXED_FUNCTION_DRAW;
    uint32_t primitiveType = GL_TRIANGLES;
    uint32_t vertexCount = 3;
    
    write(g_socket, &cmd, sizeof(cmd));
    write(g_socket, modelview, 64);
    write(g_socket, projection, 64);
    write(g_socket, &primitiveType, sizeof(primitiveType));
    write(g_socket, &vertexCount, sizeof(vertexCount));
    write(g_socket, vertices, sizeof(vertices));
    
    printf("[Test] Sent triangle with rotation %.2f radians\n", rotation);
}

void send_swap_buffers(void) {
    uint32_t cmd = CMD_METAL_SWAP_BUFFERS;
    write(g_socket, &cmd, sizeof(cmd));
    printf("[Test] Sent SWAP_BUFFERS\n");
}

void read_pixels(uint32_t width, uint32_t height, unsigned char **pixels) {
    uint32_t cmd = CMD_METAL_READ_PIXELS;
    uint32_t x = 0, y = 0;
    uint32_t format = GL_RGBA;
    uint32_t type = GL_UNSIGNED_BYTE;
    
    write(g_socket, &cmd, sizeof(cmd));
    write(g_socket, &x, sizeof(x));
    write(g_socket, &y, sizeof(y));
    write(g_socket, &width, sizeof(width));
    write(g_socket, &height, sizeof(height));
    write(g_socket, &format, sizeof(format));
    write(g_socket, &type, sizeof(type));
    
    printf("[Test] Sent READ_PIXELS request (%ux%u)\n", width, height);
    
    // Receive data size
    uint32_t dataSize = 0;
    if (read(g_socket, &dataSize, sizeof(dataSize)) != sizeof(dataSize)) {
        printf("[Test] ❌ Failed to receive data size\n");
        return;
    }
    
    if (dataSize == 0) {
        printf("[Test] ❌ Server returned 0 bytes (error)\n");
        return;
    }
    
    printf("[Test] Receiving %u bytes of pixel data...\n", dataSize);
    
    *pixels = (unsigned char*)malloc(dataSize);
    if (!*pixels) {
        printf("[Test] ❌ Failed to allocate pixel buffer\n");
        return;
    }
    
    // Receive pixel data
    size_t bytesReceived = 0;
    while (bytesReceived < dataSize) {
        ssize_t n = read(g_socket, *pixels + bytesReceived, dataSize - bytesReceived);
        if (n <= 0) {
            printf("[Test] ❌ Connection lost while receiving pixels\n");
            free(*pixels);
            *pixels = NULL;
            return;
        }
        bytesReceived += n;
    }
    
    printf("[Test] ✅ Received %zu bytes of pixel data\n", bytesReceived);
}

void save_ppm(const char *filename, unsigned char *pixels, uint32_t width, uint32_t height) {
    FILE *f = fopen(filename, "wb");
    if (!f) {
        printf("[Test] ❌ Failed to open %s for writing\n", filename);
        return;
    }
    
    fprintf(f, "P6\n%u %u\n255\n", width, height);
    
    // Convert RGBA to RGB (Metal uses BGRA, but we'll handle byte order)
    for (uint32_t i = 0; i < width * height; i++) {
        unsigned char r = pixels[i * 4 + 0];
        unsigned char g = pixels[i * 4 + 1];
        unsigned char b = pixels[i * 4 + 2];
        // Skip alpha (pixels[i * 4 + 3])
        
        fputc(r, f);
        fputc(g, f);
        fputc(b, f);
    }
    
    fclose(f);
    printf("[Test] ✅ Saved screenshot to %s\n", filename);
}

int main(int argc, char **argv) {
    printf("==============================================\n");
    printf("  Phase 7.6 Test: VM Display Integration\n");
    printf("==============================================\n");
    printf("Goal: Render on M4 Pro GPU, read pixels back to VM\n");
    printf("\n");
    
    // Check if running with libGLMetal.dylib
    const char* dyld = getenv("DYLD_INSERT_LIBRARIES");
    if (dyld && strstr(dyld, "libGLMetal.dylib")) {
        printf("✅ Running with Metal acceleration: %s\n", dyld);
        printf("   (This test uses direct socket API, not OpenGL interception)\n");
    } else {
        printf("⚠️  Not using libGLMetal.dylib (this test uses direct socket API)\n");
    }
    printf("\n");
    
    if (!connect_to_server()) {
        printf("❌ Failed to connect. Is metal_server running?\n");
        return 1;
    }
    
    const uint32_t width = 800;
    const uint32_t height = 600;
    
    printf("\n");
    printf("Test Sequence:\n");
    printf("1. Clear framebuffer (dark blue)\n");
    printf("2. Render rotating triangle (red/green/blue)\n");
    printf("3. Swap buffers (signal frame completion)\n");
    printf("4. Read pixels back from GPU\n");
    printf("5. Save as screenshot.ppm\n");
    printf("\n");
    
    // Test 1: Static triangle
    printf("--- Frame 1: Triangle at 0° ---\n");
    send_clear(0.2f, 0.3f, 0.4f, 1.0f);
    send_triangle(0.0f);
    send_swap_buffers();
    
    unsigned char *pixels1 = NULL;
    read_pixels(width, height, &pixels1);
    if (pixels1) {
        save_ppm("screenshot1.ppm", pixels1, width, height);
        free(pixels1);
    }
    
    printf("\n");
    
    // Test 2: Rotated triangle
    printf("--- Frame 2: Triangle at 45° ---\n");
    send_clear(0.2f, 0.3f, 0.4f, 1.0f);
    send_triangle(0.785f);  // 45 degrees in radians
    send_swap_buffers();
    
    unsigned char *pixels2 = NULL;
    read_pixels(width, height, &pixels2);
    if (pixels2) {
        save_ppm("screenshot2.ppm", pixels2, width, height);
        free(pixels2);
    }
    
    printf("\n");
    printf("==============================================\n");
    printf("  Test Complete!\n");
    printf("==============================================\n");
    printf("Check the generated files:\n");
    printf("  - screenshot1.ppm (triangle at 0°)\n");
    printf("  - screenshot2.ppm (triangle at 45°)\n");
    printf("\n");
    printf("View with: open screenshot1.ppm\n");
    printf("\n");
    
    close(g_socket);
    return 0;
}

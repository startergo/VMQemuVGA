// VM-Side FBO Test - Runs inside Catalina VM, renders via Metal server on host
// Compile on x86_64, run inside VM with SSH tunnel to Metal server
// HEADLESS VERSION - No window in VM, all rendering happens on host

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

// Metal server runs on host:28123, accessible via SSH reverse tunnel
#define METAL_SERVER_IP "127.0.0.1"
#define METAL_SERVER_PORT 28123

// Metal command opcodes (Phase 5)
#define CMD_METAL_GEN_BUFFERS 10
#define CMD_METAL_BIND_BUFFER 11
#define CMD_METAL_BUFFER_DATA 12
#define CMD_METAL_GEN_VERTEX_ARRAYS 14
#define CMD_METAL_BIND_VERTEX_ARRAY 15
#define CMD_METAL_VERTEX_ATTRIB_POINTER 17
#define CMD_METAL_ENABLE_VERTEX_ATTRIB_ARRAY 18
#define CMD_METAL_DRAW_ARRAYS 20

#define CMD_METAL_CREATE_SHADER 40
#define CMD_METAL_SHADER_SOURCE 41
#define CMD_METAL_COMPILE_SHADER 42
#define CMD_METAL_CREATE_PROGRAM 44
#define CMD_METAL_ATTACH_SHADER 45
#define CMD_METAL_LINK_PROGRAM 46
#define CMD_METAL_USE_PROGRAM 47

#define CMD_METAL_GEN_TEXTURES 60
#define CMD_METAL_BIND_TEXTURE 61
#define CMD_METAL_TEX_IMAGE_2D 62
#define CMD_METAL_TEX_PARAMETER_I 64

#define CMD_METAL_GEN_FRAMEBUFFERS 80
#define CMD_METAL_BIND_FRAMEBUFFER 81
#define CMD_METAL_FRAMEBUFFER_TEXTURE2D 83
#define CMD_METAL_CHECK_FRAMEBUFFER_STATUS 84

// OpenGL constants
#define GL_ARRAY_BUFFER 0x8892
#define GL_STATIC_DRAW 0x88E4
#define GL_FLOAT 0x1406
#define GL_TRIANGLES 0x0004
#define GL_TRIANGLE_STRIP 0x0005
#define GL_VERTEX_SHADER 0x8B31
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_TEXTURE_2D 0x0DE1
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_LINEAR 0x2601
#define GL_RGBA 0x1908
#define GL_UNSIGNED_BYTE 0x1401
#define GL_FRAMEBUFFER 0x8D40
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5

static int metal_sock = -1;

void send_cmd(uint32_t cmd) {
    ssize_t sent = send(metal_sock, &cmd, sizeof(cmd), 0);
    if (sent != sizeof(cmd)) {
        fprintf(stderr, "ERROR: Failed to send command %u\n", cmd);
        exit(1);
    }
}

void send_u32(uint32_t val) {
    ssize_t sent = send(metal_sock, &val, sizeof(val), 0);
    if (sent != sizeof(val)) {
        fprintf(stderr, "ERROR: Failed to send u32 value\n");
        exit(1);
    }
}

void send_u8(uint8_t val) {
    ssize_t sent = send(metal_sock, &val, sizeof(val), 0);
    if (sent != sizeof(val)) {
        fprintf(stderr, "ERROR: Failed to send u8 value\n");
        exit(1);
    }
}

void send_u64(uint64_t val) {
    ssize_t sent = send(metal_sock, &val, sizeof(val), 0);
    if (sent != sizeof(val)) {
        fprintf(stderr, "ERROR: Failed to send u64 value\n");
        exit(1);
    }
}

void send_f32(float val) {
    ssize_t sent = send(metal_sock, &val, sizeof(val), 0);
    if (sent != sizeof(val)) {
        fprintf(stderr, "ERROR: Failed to send f32 value\n");
        exit(1);
    }
}

void send_data(const void *data, size_t size) {
    ssize_t sent = send(metal_sock, data, size, 0);
    if (sent != (ssize_t)size) {
        fprintf(stderr, "ERROR: Failed to send data (%zd/%zu bytes)\n", sent, size);
        exit(1);
    }
}

uint32_t recv_u32(void) {
    uint32_t val;
    ssize_t received = recv(metal_sock, &val, sizeof(val), 0);
    if (received != sizeof(val)) {
        fprintf(stderr, "ERROR: Failed to receive u32 (got %zd bytes, expected %zu)\n", received, sizeof(val));
        fprintf(stderr, "Metal server may have disconnected or is not responding\n");
        exit(1);
    }
    return val;
}

void connect_to_metal_server(void) {
    printf("========================================\n");
    printf("  VM FBO Test (Headless)\n");
    printf("  Connecting to Metal server on host\n");
    printf("========================================\n");
    
    metal_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (metal_sock < 0) {
        fprintf(stderr, "ERROR: Failed to create socket\n");
        exit(1);
    }
    
    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(METAL_SERVER_PORT);
    inet_pton(AF_INET, METAL_SERVER_IP, &server_addr.sin_addr);
    
    printf("[VM Test] Connecting to %s:%d...\n", METAL_SERVER_IP, METAL_SERVER_PORT);
    
    if (connect(metal_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        fprintf(stderr, "ERROR: Failed to connect to Metal server\n");
        fprintf(stderr, "Make sure:\n");
        fprintf(stderr, "  1. Metal server running on host: ./metal/metal_server\n");
        fprintf(stderr, "  2. SSH reverse tunnel: ssh -R 28123:localhost:28123 ...\n");
        exit(1);
    }
    
    printf("[VM Test] ✅ Connected! Using host M4 Pro GPU via Metal\n");
    printf("[VM Test] All rendering happens in Metal server window on HOST\n");
    printf("[VM Test] No window will appear in VM (headless mode)\n");
}

void run_fbo_test(void) {
    printf("\n[VM Test] Starting FBO render test...\n");
    
    // Phase 1: Create FBO texture (512x512 RGBA)
    printf("[VM Test] Creating FBO texture...\n");
    send_cmd(CMD_METAL_GEN_TEXTURES);
    send_u32(1);
    uint32_t textureID = recv_u32();
    printf("[VM Test] Texture ID: %u\n", textureID);
    
    send_cmd(CMD_METAL_BIND_TEXTURE);
    send_u32(GL_TEXTURE_2D);
    send_u32(textureID);
    
    send_cmd(CMD_METAL_TEX_PARAMETER_I);
    send_u32(GL_TEXTURE_2D);
    send_u32(GL_TEXTURE_MIN_FILTER);
    send_u32(GL_LINEAR);
    
    send_cmd(CMD_METAL_TEX_PARAMETER_I);
    send_u32(GL_TEXTURE_2D);
    send_u32(GL_TEXTURE_MAG_FILTER);
    send_u32(GL_LINEAR);
    
    send_cmd(CMD_METAL_TEX_IMAGE_2D);
    send_u32(GL_TEXTURE_2D);     // target
    send_u32(0);                 // level
    send_u32(GL_RGBA);           // internal format
    send_u32(512);               // width
    send_u32(512);               // height
    send_u32(0);                 // border
    send_u32(GL_RGBA);           // format
    send_u32(GL_UNSIGNED_BYTE);  // type
    
    // Send empty pixel data (512x512x4 = 1MB of zeros for FBO texture)
    // FBO textures don't need initial pixel data, but server expects bytes
    uint32_t textureSize = 512 * 512 * 4;
    void *emptyPixels = calloc(1, textureSize);
    send_data(emptyPixels, textureSize);
    free(emptyPixels);
    
    // Phase 2: Create FBO and attach texture
    printf("[VM Test] Creating FBO...\n");
    send_cmd(CMD_METAL_GEN_FRAMEBUFFERS);
    send_u32(1);
    uint32_t fboID = recv_u32();
    printf("[VM Test] FBO ID: %u\n", fboID);
    
    send_cmd(CMD_METAL_BIND_FRAMEBUFFER);
    send_u32(GL_FRAMEBUFFER);
    send_u32(fboID);
    
    send_cmd(CMD_METAL_FRAMEBUFFER_TEXTURE2D);
    send_u32(GL_FRAMEBUFFER);
    send_u32(GL_COLOR_ATTACHMENT0);
    send_u32(GL_TEXTURE_2D);
    send_u32(textureID);
    send_u32(0);  // level
    
    send_cmd(CMD_METAL_CHECK_FRAMEBUFFER_STATUS);
    send_u32(GL_FRAMEBUFFER);
    uint32_t status = recv_u32();
    printf("[VM Test] FBO status: 0x%X %s\n", status, 
           (status == GL_FRAMEBUFFER_COMPLETE) ? "COMPLETE" : "INCOMPLETE");
    
    // Phase 2.5: Create shaders for textured rendering
    printf("[VM Test] Creating textured shader program...\n");
    
    // Vertex shader with texture coordinates
    const char *vertexShaderSource = 
        "#version 120\n"
        "attribute vec3 position;\n"
        "attribute vec2 texCoord;\n"
        "varying vec2 v_texCoord;\n"
        "void main() {\n"
        "    gl_Position = vec4(position, 1.0);\n"
        "    v_texCoord = texCoord;\n"
        "}\n";
    
    // Fragment shader samples from texture
    const char *fragmentShaderSource = 
        "#version 120\n"
        "varying vec2 v_texCoord;\n"
        "uniform sampler2D tex;\n"
        "void main() {\n"
        "    gl_FragColor = texture2D(tex, v_texCoord);\n"
        "}\n";
    
    // Create vertex shader
    send_cmd(CMD_METAL_CREATE_SHADER);
    send_u32(GL_VERTEX_SHADER);
    uint32_t vertShader = recv_u32();
    
    send_cmd(CMD_METAL_SHADER_SOURCE);
    send_u32(vertShader);
    send_u32(strlen(vertexShaderSource));
    send_data(vertexShaderSource, strlen(vertexShaderSource));
    
    send_cmd(CMD_METAL_COMPILE_SHADER);
    send_u32(vertShader);
    send_u32(GL_VERTEX_SHADER);
    uint32_t vertSuccess = recv_u32();
    printf("[VM Test] Vertex shader compiled: %s\n", vertSuccess ? "SUCCESS" : "FAILED");
    
    // Create fragment shader
    send_cmd(CMD_METAL_CREATE_SHADER);
    send_u32(GL_FRAGMENT_SHADER);
    uint32_t fragShader = recv_u32();
    
    send_cmd(CMD_METAL_SHADER_SOURCE);
    send_u32(fragShader);
    send_u32(strlen(fragmentShaderSource));
    send_data(fragmentShaderSource, strlen(fragmentShaderSource));
    
    send_cmd(CMD_METAL_COMPILE_SHADER);
    send_u32(fragShader);
    send_u32(GL_FRAGMENT_SHADER);
    uint32_t fragSuccess = recv_u32();
    printf("[VM Test] Fragment shader compiled: %s\n", fragSuccess ? "SUCCESS" : "FAILED");
    
    // Create and link program
    send_cmd(CMD_METAL_CREATE_PROGRAM);
    uint32_t program = recv_u32();
    printf("[VM Test] Shader program ID: %u\n", program);
    
    send_cmd(CMD_METAL_ATTACH_SHADER);
    send_u32(program);
    send_u32(vertShader);
    send_u32(GL_VERTEX_SHADER);
    
    send_cmd(CMD_METAL_ATTACH_SHADER);
    send_u32(program);
    send_u32(fragShader);
    send_u32(GL_FRAGMENT_SHADER);
    
    send_cmd(CMD_METAL_LINK_PROGRAM);
    send_u32(program);
    uint32_t linkSuccess = recv_u32();
    printf("[VM Test] Program linked: %s\n", linkSuccess ? "SUCCESS" : "FAILED");
    
    // DON'T use the shader yet - wait until rendering the quad
    // (Triangle render uses default shader with vertex colors)
    
    // Explicitly disable custom shader (use program 0 = default pipeline)
    send_cmd(CMD_METAL_USE_PROGRAM);
    send_u32(0);  // program 0 = use default colored pipeline
    printf("[VM Test] Using default colored shader for triangle\n");
    
    // Unbind texture 1 to prevent feedback loop when rendering to FBO
    send_cmd(CMD_METAL_BIND_TEXTURE);
    send_u32(GL_TEXTURE_2D);
    send_u32(0);  // Unbind (texture 0 = default white texture)
    
    // Phase 3: Create triangle VBO (render to FBO)
    printf("[VM Test] Creating triangle VBO...\n");
    float triangleVertices[] = {
        // position (x,y,z)  color (r,g,b,a)
         0.0f,  0.5f, 0.0f,  1.0f, 0.0f, 0.0f, 1.0f,  // top (red)
        -0.5f, -0.5f, 0.0f,  0.0f, 1.0f, 0.0f, 1.0f,  // bottom-left (green)
         0.5f, -0.5f, 0.0f,  0.0f, 0.0f, 1.0f, 1.0f   // bottom-right (blue)
    };
    
    send_cmd(CMD_METAL_GEN_BUFFERS);
    send_u32(1);
    uint32_t triangleVBO = recv_u32();
    
    send_cmd(CMD_METAL_BIND_BUFFER);
    send_u32(GL_ARRAY_BUFFER);
    send_u32(triangleVBO);
    
    send_cmd(CMD_METAL_BUFFER_DATA);
    send_u32(GL_ARRAY_BUFFER);
    send_u64(sizeof(triangleVertices));
    send_data(triangleVertices, sizeof(triangleVertices));
    send_u32(GL_STATIC_DRAW);
    
    // Phase 4: Create triangle VAO
    send_cmd(CMD_METAL_GEN_VERTEX_ARRAYS);
    send_u32(1);
    uint32_t triangleVAO = recv_u32();
    
    send_cmd(CMD_METAL_BIND_VERTEX_ARRAY);
    send_u32(triangleVAO);
    
    // Re-bind buffer after VAO bind (VAO restores its saved buffer state)
    send_cmd(CMD_METAL_BIND_BUFFER);
    send_u32(GL_ARRAY_BUFFER);
    send_u32(triangleVBO);
    
    send_cmd(CMD_METAL_VERTEX_ATTRIB_POINTER);
    send_u32(0);                          // index
    send_u32(3);                          // size
    send_u32(GL_FLOAT);                   // type
    send_u8(0);                           // normalized (uint8_t!)
    send_u32(7 * sizeof(float));          // stride (position + color)
    send_u64(0);                          // offset
    
    send_cmd(CMD_METAL_ENABLE_VERTEX_ATTRIB_ARRAY);
    send_u32(0);
    
    send_cmd(CMD_METAL_VERTEX_ATTRIB_POINTER);
    send_u32(1);                          // index
    send_u32(4);                          // size
    send_u32(GL_FLOAT);                   // type
    send_u8(0);                           // normalized (uint8_t!)
    send_u32(7 * sizeof(float));          // stride
    send_u64(3 * sizeof(float));          // offset (after position)
    
    send_cmd(CMD_METAL_ENABLE_VERTEX_ATTRIB_ARRAY);
    send_u32(1);
    
    // Phase 5: Render triangle to FBO
    printf("[VM Test] Rendering triangle to FBO %u...\n", fboID);
    send_cmd(CMD_METAL_DRAW_ARRAYS);
    send_u32(GL_TRIANGLES);
    send_u32(0);  // first
    send_u32(3);  // count
    
    // Phase 6: Switch to screen framebuffer
    printf("[VM Test] Switching to screen framebuffer...\n");
    send_cmd(CMD_METAL_BIND_FRAMEBUFFER);
    send_u32(GL_FRAMEBUFFER);
    send_u32(0);  // 0 = default framebuffer (screen)
    
    // Phase 7: Create quad VBO (textured with FBO texture)
    printf("[VM Test] Creating screen quad VBO...\n");
    float quadVertices[] = {
        // position (x,y,z)  texcoords (u,v)
        -0.8f,  0.8f, 0.0f,  0.0f, 1.0f,  // top-left
        -0.8f, -0.8f, 0.0f,  0.0f, 0.0f,  // bottom-left
         0.8f,  0.8f, 0.0f,  1.0f, 1.0f,  // top-right
         0.8f, -0.8f, 0.0f,  1.0f, 0.0f   // bottom-right
    };
    
    send_cmd(CMD_METAL_GEN_BUFFERS);
    send_u32(1);
    uint32_t quadVBO = recv_u32();
    
    send_cmd(CMD_METAL_BIND_BUFFER);
    send_u32(GL_ARRAY_BUFFER);
    send_u32(quadVBO);
    
    send_cmd(CMD_METAL_BUFFER_DATA);
    send_u32(GL_ARRAY_BUFFER);
    send_u64(sizeof(quadVertices));
    send_data(quadVertices, sizeof(quadVertices));
    send_u32(GL_STATIC_DRAW);
    
    // Phase 8: Create quad VAO
    send_cmd(CMD_METAL_GEN_VERTEX_ARRAYS);
    send_u32(1);
    uint32_t quadVAO = recv_u32();
    
    send_cmd(CMD_METAL_BIND_VERTEX_ARRAY);
    send_u32(quadVAO);
    
    // Re-bind buffer after VAO bind
    send_cmd(CMD_METAL_BIND_BUFFER);
    send_u32(GL_ARRAY_BUFFER);
    send_u32(quadVBO);
    
    send_cmd(CMD_METAL_VERTEX_ATTRIB_POINTER);
    send_u32(0);                          // index (position)
    send_u32(3);                          // size
    send_u32(GL_FLOAT);                   // type
    send_u8(0);                           // normalized (uint8_t!)
    send_u32(5 * sizeof(float));          // stride (position + texcoord)
    send_u64(0);                          // offset
    
    send_cmd(CMD_METAL_ENABLE_VERTEX_ATTRIB_ARRAY);
    send_u32(0);
    
    send_cmd(CMD_METAL_VERTEX_ATTRIB_POINTER);
    send_u32(1);                          // index (texcoord)
    send_u32(2);                          // size
    send_u32(GL_FLOAT);                   // type
    send_u8(0);                           // normalized (uint8_t!)
    send_u32(5 * sizeof(float));          // stride
    send_u64(3 * sizeof(float));          // offset (after position)
    
    send_cmd(CMD_METAL_ENABLE_VERTEX_ATTRIB_ARRAY);
    send_u32(1);
    
    // Phase 9: Bind FBO texture and render quad to screen
    printf("[VM Test] Rendering quad with FBO texture to screen...\n");
    
    // NOW activate the textured shader for the quad
    send_cmd(CMD_METAL_USE_PROGRAM);
    send_u32(program);
    printf("[VM Test] Using textured shader program %u\n", program);
    
    send_cmd(CMD_METAL_BIND_TEXTURE);
    send_u32(GL_TEXTURE_2D);
    send_u32(textureID);
    
    send_cmd(CMD_METAL_DRAW_ARRAYS);
    send_u32(GL_TRIANGLE_STRIP);
    send_u32(0);  // first
    send_u32(4);  // count
    
    printf("[VM Test] ✅ FBO render test complete!\n");
    printf("[VM Test] Check Metal server window on host - you should see colored triangle!\n");
}

int main(int argc, char **argv) {
    connect_to_metal_server();
    
    printf("\n");
    run_fbo_test();
    
    printf("\n========================================\n");
    printf("  Test Complete!\n");
    printf("========================================\n");
    printf("Check the Metal server window on HOST\n");
    printf("You should see:\n");
    printf("  - Magenta background (FBO clear color)\n");
    printf("  - RGB triangle (red/green/blue vertices)\n");
    printf("\n");
    
    close(metal_sock);
    return 0;
}

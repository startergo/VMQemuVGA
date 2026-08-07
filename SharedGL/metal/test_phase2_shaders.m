// test_phase2_shaders.m - Test Phase 2: Custom GLSL Shaders
// Tests glCreateShader, glShaderSource, glCompileShader, glCreateProgram,
// glAttachShader, glLinkProgram, glUseProgram, and basic uniforms

#import <Foundation/Foundation.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 28123

// Metal command opcodes
typedef enum {
    // Phase 0: Immediate mode
    CMD_METAL_CREATE_BUFFER = 1,
    CMD_METAL_SET_VERTEX_BUFFER,
    CMD_METAL_SET_FRAGMENT_BYTES,
    CMD_METAL_DRAW_PRIMITIVES,
    CMD_METAL_PRESENT,
    CMD_METAL_CLEAR,
    CMD_METAL_SET_VIEWPORT,
    
    // Phase 1: VBOs
    CMD_METAL_GEN_BUFFERS = 10,
    CMD_METAL_BIND_BUFFER,
    CMD_METAL_BUFFER_DATA,
    CMD_METAL_DELETE_BUFFERS,
    CMD_METAL_GEN_VERTEX_ARRAYS,
    CMD_METAL_BIND_VERTEX_ARRAY,
    CMD_METAL_DELETE_VERTEX_ARRAYS,
    CMD_METAL_VERTEX_ATTRIB_POINTER,
    CMD_METAL_ENABLE_VERTEX_ATTRIB_ARRAY,
    CMD_METAL_DISABLE_VERTEX_ATTRIB_ARRAY,
    CMD_METAL_DRAW_ARRAYS,
    CMD_METAL_DRAW_ELEMENTS,
    
    // Phase 2: Shaders
    CMD_METAL_CREATE_SHADER = 40,
    CMD_METAL_SHADER_SOURCE,
    CMD_METAL_COMPILE_SHADER,
    CMD_METAL_DELETE_SHADER,
    CMD_METAL_CREATE_PROGRAM,
    CMD_METAL_ATTACH_SHADER,
    CMD_METAL_LINK_PROGRAM,
    CMD_METAL_USE_PROGRAM,
    CMD_METAL_DELETE_PROGRAM,
    CMD_METAL_GET_UNIFORM_LOCATION,
    CMD_METAL_UNIFORM_1F,
    CMD_METAL_UNIFORM_2F,
    CMD_METAL_UNIFORM_3F,
    CMD_METAL_UNIFORM_4F,
    CMD_METAL_UNIFORM_1I,
    CMD_METAL_UNIFORM_MATRIX_4FV,
} MetalCommand;

typedef enum {
    GL_VERTEX_SHADER = 0x8B31,
    GL_FRAGMENT_SHADER = 0x8B30,
} GLShaderType;

typedef enum {
    GL_ARRAY_BUFFER = 0x8892,
} GLBufferTarget;

typedef enum {
    GL_STATIC_DRAW = 0x88E4,
} GLBufferUsage;

typedef enum {
    GL_TRIANGLES = 4,
} GLPrimitiveType;

// Global socket
static int metalSocket = -1;

// ============== Phase 2: Shader Functions ==============

uint32_t glCreateShader(uint32_t type) {
    uint32_t cmd = CMD_METAL_CREATE_SHADER;
    send(metalSocket, &cmd, sizeof(cmd), 0);
    send(metalSocket, &type, sizeof(type), 0);
    
    uint32_t shaderID = 0;
    recv(metalSocket, &shaderID, sizeof(shaderID), 0);
    
    NSLog(@"glCreateShader(0x%X) -> %u", type, shaderID);
    return shaderID;
}

void glShaderSource(uint32_t shader, const char* source) {
    uint32_t cmd = CMD_METAL_SHADER_SOURCE;
    uint32_t sourceLength = (uint32_t)strlen(source);
    
    send(metalSocket, &cmd, sizeof(cmd), 0);
    send(metalSocket, &shader, sizeof(shader), 0);
    send(metalSocket, &sourceLength, sizeof(sourceLength), 0);
    send(metalSocket, source, sourceLength, 0);
    
    NSLog(@"glShaderSource(%u) (%u bytes)", shader, sourceLength);
}

uint32_t glCompileShader(uint32_t shader, uint32_t type) {
    uint32_t cmd = CMD_METAL_COMPILE_SHADER;
    send(metalSocket, &cmd, sizeof(cmd), 0);
    send(metalSocket, &shader, sizeof(shader), 0);
    send(metalSocket, &type, sizeof(type), 0);
    
    uint32_t success = 0;
    recv(metalSocket, &success, sizeof(success), 0);
    
    NSLog(@"glCompileShader(%u) -> %s", shader, success ? "SUCCESS" : "FAILED");
    return success;
}

uint32_t glCreateProgram(void) {
    uint32_t cmd = CMD_METAL_CREATE_PROGRAM;
    send(metalSocket, &cmd, sizeof(cmd), 0);
    
    uint32_t programID = 0;
    recv(metalSocket, &programID, sizeof(programID), 0);
    
    NSLog(@"glCreateProgram() -> %u", programID);
    return programID;
}

void glAttachShader(uint32_t program, uint32_t shader, uint32_t type) {
    uint32_t cmd = CMD_METAL_ATTACH_SHADER;
    send(metalSocket, &cmd, sizeof(cmd), 0);
    send(metalSocket, &program, sizeof(program), 0);
    send(metalSocket, &shader, sizeof(shader), 0);
    send(metalSocket, &type, sizeof(type), 0);
    
    NSLog(@"glAttachShader(program=%u, shader=%u)", program, shader);
}

uint32_t glLinkProgram(uint32_t program) {
    uint32_t cmd = CMD_METAL_LINK_PROGRAM;
    send(metalSocket, &cmd, sizeof(cmd), 0);
    send(metalSocket, &program, sizeof(program), 0);
    
    uint32_t success = 0;
    recv(metalSocket, &success, sizeof(success), 0);
    
    NSLog(@"glLinkProgram(%u) -> %s", program, success ? "SUCCESS" : "FAILED");
    return success;
}

void glUseProgram(uint32_t program) {
    uint32_t cmd = CMD_METAL_USE_PROGRAM;
    send(metalSocket, &cmd, sizeof(cmd), 0);
    send(metalSocket, &program, sizeof(program), 0);
    
    NSLog(@"glUseProgram(%u)", program);
}

int32_t glGetUniformLocation(uint32_t program, const char* name) {
    uint32_t cmd = CMD_METAL_GET_UNIFORM_LOCATION;
    uint32_t nameLength = (uint32_t)strlen(name);
    
    send(metalSocket, &cmd, sizeof(cmd), 0);
    send(metalSocket, &program, sizeof(program), 0);
    send(metalSocket, &nameLength, sizeof(nameLength), 0);
    send(metalSocket, name, nameLength, 0);
    
    int32_t location = -1;
    recv(metalSocket, &location, sizeof(location), 0);
    
    NSLog(@"glGetUniformLocation(%u, '%s') -> %d", program, name, location);
    return location;
}

void glUniform4f(int32_t location, float v0, float v1, float v2, float v3) {
    uint32_t cmd = CMD_METAL_UNIFORM_4F;
    float values[4] = {v0, v1, v2, v3};
    
    send(metalSocket, &cmd, sizeof(cmd), 0);
    send(metalSocket, &location, sizeof(location), 0);
    send(metalSocket, values, sizeof(values), 0);
    
    NSLog(@"glUniform4f(%d, %.2f, %.2f, %.2f, %.2f)", location, v0, v1, v2, v3);
}

// ============== Phase 1: VBO Functions ==============

void glGenBuffers(uint32_t count, uint32_t* buffers) {
    uint32_t cmd = CMD_METAL_GEN_BUFFERS;
    send(metalSocket, &cmd, sizeof(cmd), 0);
    send(metalSocket, &count, sizeof(count), 0);
    recv(metalSocket, buffers, count * sizeof(uint32_t), 0);
    
    NSLog(@"glGenBuffers(%u) -> [%u]", count, buffers[0]);
}

void glBindBuffer(uint32_t target, uint32_t buffer) {
    uint32_t cmd = CMD_METAL_BIND_BUFFER;
    send(metalSocket, &cmd, sizeof(cmd), 0);
    send(metalSocket, &target, sizeof(target), 0);
    send(metalSocket, &buffer, sizeof(buffer), 0);
    
    NSLog(@"glBindBuffer(0x%X, %u)", target, buffer);
}

void glBufferData(uint32_t target, size_t size, const void* data, uint32_t usage) {
    uint32_t cmd = CMD_METAL_BUFFER_DATA;
    uint64_t size64 = size;
    
    send(metalSocket, &cmd, sizeof(cmd), 0);
    send(metalSocket, &target, sizeof(target), 0);
    send(metalSocket, &size64, sizeof(size64), 0);
    send(metalSocket, data, size, 0);
    send(metalSocket, &usage, sizeof(usage), 0);
    
    NSLog(@"glBufferData(0x%X, %zu bytes, usage=0x%X)", target, size, usage);
}

void glVertexAttribPointer(uint32_t index, int size, uint32_t type, uint8_t normalized, uint32_t stride, uint64_t offset) {
    uint32_t cmd = CMD_METAL_VERTEX_ATTRIB_POINTER;
    send(metalSocket, &cmd, sizeof(cmd), 0);
    send(metalSocket, &index, sizeof(index), 0);
    send(metalSocket, &size, sizeof(size), 0);
    send(metalSocket, &type, sizeof(type), 0);
    send(metalSocket, &normalized, sizeof(normalized), 0);
    send(metalSocket, &stride, sizeof(stride), 0);
    send(metalSocket, &offset, sizeof(offset), 0);
    
    NSLog(@"glVertexAttribPointer(%u, size=%d, stride=%u, offset=%llu)", index, size, stride, offset);
}

void glEnableVertexAttribArray(uint32_t index) {
    uint32_t cmd = CMD_METAL_ENABLE_VERTEX_ATTRIB_ARRAY;
    send(metalSocket, &cmd, sizeof(cmd), 0);
    send(metalSocket, &index, sizeof(index), 0);
    
    NSLog(@"glEnableVertexAttribArray(%u)", index);
}

void glDrawArrays(uint32_t mode, uint32_t first, uint32_t count) {
    uint32_t cmd = CMD_METAL_DRAW_ARRAYS;
    send(metalSocket, &cmd, sizeof(cmd), 0);
    send(metalSocket, &mode, sizeof(mode), 0);
    send(metalSocket, &first, sizeof(first), 0);
    send(metalSocket, &count, sizeof(count), 0);
    
    NSLog(@"glDrawArrays(mode=%u, first=%u, count=%u)", mode, first, count);
}

// ============== Main Test ==============

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        NSLog(@"========================================");
        NSLog(@"Phase 2 Test: Custom GLSL Shaders");
        NSLog(@"========================================");
        
        // Connect to Metal server
        metalSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (metalSocket < 0) {
            NSLog(@"ERROR: Failed to create socket");
            return 1;
        }
        
        struct sockaddr_in serverAddr = {0};
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(SERVER_PORT);
        inet_pton(AF_INET, SERVER_IP, &serverAddr.sin_addr);
        
        if (connect(metalSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
            NSLog(@"ERROR: Failed to connect to Metal server");
            return 1;
        }
        
        NSLog(@"✅ Connected to Metal server at %s:%d", SERVER_IP, SERVER_PORT);
        
        // Simple GLSL vertex shader
        const char* vertexShaderSource = 
            "#version 120\n"
            "attribute vec3 position;\n"
            "attribute vec4 color;\n"
            "varying vec4 v_color;\n"
            "void main() {\n"
            "    gl_Position = vec4(position, 1.0);\n"
            "    v_color = color;\n"
            "}\n";
        
        // Simple GLSL fragment shader with uniform color multiplier
        const char* fragmentShaderSource = 
            "#version 120\n"
            "varying vec4 v_color;\n"
            "uniform vec4 colorMultiplier;\n"
            "void main() {\n"
            "    gl_FragColor = v_color * colorMultiplier;\n"
            "}\n";
        
        // Create and compile shaders
        NSLog(@"\n--- Creating Shaders ---");
        uint32_t vertexShader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertexShader, vertexShaderSource);
        if (!glCompileShader(vertexShader, GL_VERTEX_SHADER)) {
            NSLog(@"❌ Vertex shader compilation failed");
            return 1;
        }
        
        uint32_t fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragmentShader, fragmentShaderSource);
        if (!glCompileShader(fragmentShader, GL_FRAGMENT_SHADER)) {
            NSLog(@"❌ Fragment shader compilation failed");
            return 1;
        }
        
        // Create and link program
        NSLog(@"\n--- Creating Program ---");
        uint32_t program = glCreateProgram();
        glAttachShader(program, vertexShader, GL_VERTEX_SHADER);
        glAttachShader(program, fragmentShader, GL_FRAGMENT_SHADER);
        if (!glLinkProgram(program)) {
            NSLog(@"❌ Program link failed");
            return 1;
        }
        
        // Use the program
        glUseProgram(program);
        
        // Get uniform location
        int32_t colorMultiplierLoc = glGetUniformLocation(program, "colorMultiplier");
        if (colorMultiplierLoc >= 0) {
            // Set uniform to white (no change)
            glUniform4f(colorMultiplierLoc, 1.0f, 1.0f, 1.0f, 1.0f);
        }
        
        // Create VBO with triangle data (same as Phase 1 test)
        NSLog(@"\n--- Creating VBO ---");
        float triangleData[] = {
            // position (x, y, z)    color (r, g, b, a)
            0.0f,  0.5f, 0.0f,      1.0f, 0.0f, 0.0f, 1.0f,  // Top (red)
           -0.5f, -0.5f, 0.0f,      0.0f, 1.0f, 0.0f, 1.0f,  // Bottom-left (green)
            0.5f, -0.5f, 0.0f,      0.0f, 0.0f, 1.0f, 1.0f   // Bottom-right (blue)
        };
        
        uint32_t vbo;
        glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(triangleData), triangleData, GL_STATIC_DRAW);
        
        // Set up vertex attributes
        glVertexAttribPointer(0, 3, 0x1406, 0, 7 * sizeof(float), 0);  // position
        glEnableVertexAttribArray(0);
        
        glVertexAttribPointer(1, 4, 0x1406, 0, 7 * sizeof(float), 3 * sizeof(float));  // color
        glEnableVertexAttribArray(1);
        
        // Draw the triangle
        NSLog(@"\n--- Drawing Triangle with Custom Shader ---");
        glDrawArrays(GL_TRIANGLES, 0, 3);
        
        NSLog(@"\n✅ Phase 2 Test Complete!");
        NSLog(@"You should see a colored triangle rendered with the custom GLSL shader");
        
        // Keep socket open for a moment
        sleep(2);
        
        close(metalSocket);
    }
    return 0;
}

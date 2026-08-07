// Phase 5 Test: Framebuffer Objects (Render-to-Texture)
// Tests FBO creation, rendering to texture, and using FBO texture on screen
#import <Cocoa/Cocoa.h>
#import <sys/socket.h>
#import <netinet/in.h>
#import <arpa/inet.h>
#import <unistd.h>

// Metal command opcodes
#define CMD_METAL_GEN_BUFFERS 10
#define CMD_METAL_BIND_BUFFER 11
#define CMD_METAL_BUFFER_DATA 12
#define CMD_METAL_DELETE_BUFFERS 13
#define CMD_METAL_GEN_VERTEX_ARRAYS 14
#define CMD_METAL_BIND_VERTEX_ARRAY 15
#define CMD_METAL_DELETE_VERTEX_ARRAYS 16
#define CMD_METAL_VERTEX_ATTRIB_POINTER 17
#define CMD_METAL_ENABLE_VERTEX_ATTRIB_ARRAY 18
#define CMD_METAL_DISABLE_VERTEX_ATTRIB_ARRAY 19
#define CMD_METAL_DRAW_ARRAYS 20
#define CMD_METAL_DRAW_ELEMENTS 21

// Phase 2: Shaders
#define CMD_METAL_CREATE_SHADER 40
#define CMD_METAL_SHADER_SOURCE 41
#define CMD_METAL_COMPILE_SHADER 42
#define CMD_METAL_DELETE_SHADER 43
#define CMD_METAL_CREATE_PROGRAM 44
#define CMD_METAL_ATTACH_SHADER 45
#define CMD_METAL_LINK_PROGRAM 46
#define CMD_METAL_USE_PROGRAM 47
#define CMD_METAL_DELETE_PROGRAM 48

#define CMD_METAL_GEN_TEXTURES 60
#define CMD_METAL_BIND_TEXTURE 61
#define CMD_METAL_TEX_IMAGE_2D 62
#define CMD_METAL_TEX_PARAMETER_I 64

#define CMD_METAL_GEN_FRAMEBUFFERS 80
#define CMD_METAL_BIND_FRAMEBUFFER 81
#define CMD_METAL_DELETE_FRAMEBUFFERS 82
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

static int metal_socket = -1;

void metal_connect(void) {
    metal_socket = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(28123),
        .sin_addr.s_addr = inet_addr("127.0.0.1")
    };
    
    if (connect(metal_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        NSLog(@"Failed to connect to Metal server");
        exit(1);
    }
    NSLog(@"Connected to Metal server");
}

void metal_send_command(uint32_t opcode) {
    send(metal_socket, &opcode, sizeof(opcode), 0);
}

uint32_t metal_gen_buffer(void) {
    metal_send_command(CMD_METAL_GEN_BUFFERS);
    uint32_t count = 1;
    send(metal_socket, &count, sizeof(count), 0);
    
    uint32_t bufferID;
    recv(metal_socket, &bufferID, sizeof(bufferID), 0);
    return bufferID;
}

void metal_bind_buffer(uint32_t target, uint32_t buffer) {
    metal_send_command(CMD_METAL_BIND_BUFFER);
    send(metal_socket, &target, sizeof(target), 0);
    send(metal_socket, &buffer, sizeof(buffer), 0);
}

void metal_buffer_data(uint32_t target, size_t size, const void* data, uint32_t usage) {
    metal_send_command(CMD_METAL_BUFFER_DATA);
    send(metal_socket, &target, sizeof(target), 0);
    send(metal_socket, &size, sizeof(size), 0);
    
    // Send data in chunks
    size_t sent = 0;
    while (sent < size) {
        ssize_t n = send(metal_socket, (uint8_t*)data + sent, size - sent, 0);
        if (n <= 0) break;
        sent += n;
    }
    
    send(metal_socket, &usage, sizeof(usage), 0);
}

uint32_t metal_gen_vao(void) {
    metal_send_command(CMD_METAL_GEN_VERTEX_ARRAYS);
    uint32_t count = 1;
    send(metal_socket, &count, sizeof(count), 0);
    
    uint32_t vaoID;
    recv(metal_socket, &vaoID, sizeof(vaoID), 0);
    return vaoID;
}

void metal_bind_vao(uint32_t vao) {
    metal_send_command(CMD_METAL_BIND_VERTEX_ARRAY);
    send(metal_socket, &vao, sizeof(vao), 0);
}

void metal_vertex_attrib_pointer(uint32_t index, int32_t size, uint32_t type, uint8_t normalized, int32_t stride, uint64_t offset) {
    metal_send_command(CMD_METAL_VERTEX_ATTRIB_POINTER);
    send(metal_socket, &index, sizeof(index), 0);
    send(metal_socket, &size, sizeof(size), 0);
    send(metal_socket, &type, sizeof(type), 0);
    send(metal_socket, &normalized, sizeof(normalized), 0);
    send(metal_socket, &stride, sizeof(stride), 0);
    send(metal_socket, &offset, sizeof(offset), 0);
}

void metal_enable_vertex_attrib_array(uint32_t index) {
    metal_send_command(CMD_METAL_ENABLE_VERTEX_ATTRIB_ARRAY);
    send(metal_socket, &index, sizeof(index), 0);
}

void metal_draw_arrays(uint32_t mode, uint32_t first, uint32_t count) {
    metal_send_command(CMD_METAL_DRAW_ARRAYS);
    send(metal_socket, &mode, sizeof(mode), 0);
    send(metal_socket, &first, sizeof(first), 0);
    send(metal_socket, &count, sizeof(count), 0);
}

// Shader functions
uint32_t metal_create_shader(uint32_t type) {
    metal_send_command(CMD_METAL_CREATE_SHADER);
    send(metal_socket, &type, sizeof(type), 0);
    
    uint32_t shaderID;
    recv(metal_socket, &shaderID, sizeof(shaderID), 0);
    return shaderID;
}

void metal_shader_source(uint32_t shader, const char* source) {
    metal_send_command(CMD_METAL_SHADER_SOURCE);
    uint32_t sourceLength = (uint32_t)strlen(source);
    send(metal_socket, &shader, sizeof(shader), 0);
    send(metal_socket, &sourceLength, sizeof(sourceLength), 0);
    send(metal_socket, source, sourceLength, 0);
}

void metal_compile_shader(uint32_t shader, uint32_t type) {
    metal_send_command(CMD_METAL_COMPILE_SHADER);
    send(metal_socket, &shader, sizeof(shader), 0);
    send(metal_socket, &type, sizeof(type), 0);
    
    uint32_t success;
    recv(metal_socket, &success, sizeof(success), 0);
}

uint32_t metal_create_program(void) {
    metal_send_command(CMD_METAL_CREATE_PROGRAM);
    
    uint32_t programID;
    recv(metal_socket, &programID, sizeof(programID), 0);
    return programID;
}

void metal_attach_shader(uint32_t program, uint32_t shader, uint32_t type) {
    metal_send_command(CMD_METAL_ATTACH_SHADER);
    send(metal_socket, &program, sizeof(program), 0);
    send(metal_socket, &shader, sizeof(shader), 0);
    send(metal_socket, &type, sizeof(type), 0);
}

void metal_link_program(uint32_t program) {
    metal_send_command(CMD_METAL_LINK_PROGRAM);
    send(metal_socket, &program, sizeof(program), 0);
    
    uint32_t success;
    recv(metal_socket, &success, sizeof(success), 0);
}

void metal_use_program(uint32_t program) {
    metal_send_command(CMD_METAL_USE_PROGRAM);
    send(metal_socket, &program, sizeof(program), 0);
}

// Texture functions
uint32_t metal_gen_texture(void) {
    metal_send_command(CMD_METAL_GEN_TEXTURES);
    uint32_t count = 1;
    send(metal_socket, &count, sizeof(count), 0);
    
    uint32_t textureID;
    recv(metal_socket, &textureID, sizeof(textureID), 0);
    return textureID;
}

void metal_bind_texture(uint32_t target, uint32_t texture) {
    metal_send_command(CMD_METAL_BIND_TEXTURE);
    send(metal_socket, &target, sizeof(target), 0);
    send(metal_socket, &texture, sizeof(texture), 0);
}

void metal_tex_parameter_i(uint32_t target, uint32_t pname, int32_t param) {
    metal_send_command(CMD_METAL_TEX_PARAMETER_I);
    send(metal_socket, &target, sizeof(target), 0);
    send(metal_socket, &pname, sizeof(pname), 0);
    send(metal_socket, &param, sizeof(param), 0);
}

void metal_tex_image_2d(uint32_t target, int32_t level, int32_t internalformat,
                       uint32_t width, uint32_t height, int32_t border,
                       uint32_t format, uint32_t type, const void* pixels) {
    metal_send_command(CMD_METAL_TEX_IMAGE_2D);
    send(metal_socket, &target, sizeof(target), 0);
    send(metal_socket, &level, sizeof(level), 0);
    send(metal_socket, &internalformat, sizeof(internalformat), 0);
    send(metal_socket, &width, sizeof(width), 0);
    send(metal_socket, &height, sizeof(height), 0);
    send(metal_socket, &border, sizeof(border), 0);
    send(metal_socket, &format, sizeof(format), 0);
    send(metal_socket, &type, sizeof(type), 0);
    
    // Calculate pixel data size and send
    uint32_t pixelSize = (format == GL_RGBA) ? 4 : 3;
    size_t dataSize = width * height * pixelSize;
    
    if (pixels) {
        // Send actual pixel data
        size_t sent = 0;
        while (sent < dataSize) {
            ssize_t n = send(metal_socket, (uint8_t*)pixels + sent, dataSize - sent, 0);
            if (n <= 0) break;
            sent += n;
        }
    } else {
        // Send empty/zero data for NULL pixels (for FBO textures)
        uint8_t *zeroData = calloc(dataSize, 1);
        size_t sent = 0;
        while (sent < dataSize) {
            ssize_t n = send(metal_socket, zeroData + sent, dataSize - sent, 0);
            if (n <= 0) break;
            sent += n;
        }
        free(zeroData);
    }
}

// FBO functions
uint32_t metal_gen_framebuffer(void) {
    metal_send_command(CMD_METAL_GEN_FRAMEBUFFERS);
    uint32_t count = 1;
    send(metal_socket, &count, sizeof(count), 0);
    
    uint32_t fboID;
    recv(metal_socket, &fboID, sizeof(fboID), 0);
    return fboID;
}

void metal_bind_framebuffer(uint32_t target, uint32_t framebuffer) {
    metal_send_command(CMD_METAL_BIND_FRAMEBUFFER);
    send(metal_socket, &target, sizeof(target), 0);
    send(metal_socket, &framebuffer, sizeof(framebuffer), 0);
}

void metal_framebuffer_texture2d(uint32_t target, uint32_t attachment,
                                 uint32_t textarget, uint32_t texture, int32_t level) {
    metal_send_command(CMD_METAL_FRAMEBUFFER_TEXTURE2D);
    send(metal_socket, &target, sizeof(target), 0);
    send(metal_socket, &attachment, sizeof(attachment), 0);
    send(metal_socket, &textarget, sizeof(textarget), 0);
    send(metal_socket, &texture, sizeof(texture), 0);
    send(metal_socket, &level, sizeof(level), 0);
}

uint32_t metal_check_framebuffer_status(uint32_t target) {
    metal_send_command(CMD_METAL_CHECK_FRAMEBUFFER_STATUS);
    send(metal_socket, &target, sizeof(target), 0);
    
    uint32_t status;
    recv(metal_socket, &status, sizeof(status), 0);
    return status;
}

@interface TestView : NSView
@property (nonatomic) uint32_t fboTextureID;
@end

@implementation TestView

- (void)drawRect:(NSRect)dirtyRect {
    [super drawRect:dirtyRect];
}

- (void)setupAndRender {
    metal_connect();
    sleep(1);  // Give connection time to stabilize
    
    NSLog(@"=== Phase 5 FBO Test ===");
    
    // Step 1: Create FBO texture (512x512 for render target)
    uint32_t fboTexture = metal_gen_texture();
    metal_bind_texture(GL_TEXTURE_2D, fboTexture);
    metal_tex_parameter_i(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    metal_tex_parameter_i(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    // Create empty 512x512 RGBA texture
    metal_tex_image_2d(GL_TEXTURE_2D, 0, GL_RGBA, 512, 512, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    NSLog(@"Created FBO texture: %u (512x512)", fboTexture);
    
    // Step 2: Create FBO and attach texture
    uint32_t fbo = metal_gen_framebuffer();
    metal_bind_framebuffer(GL_FRAMEBUFFER, fbo);
    metal_framebuffer_texture2d(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fboTexture, 0);
    
    uint32_t fboStatus = metal_check_framebuffer_status(GL_FRAMEBUFFER);
    if (fboStatus == GL_FRAMEBUFFER_COMPLETE) {
        NSLog(@"✅ FBO complete: %u", fbo);
    } else {
        NSLog(@"❌ FBO incomplete: status=0x%X", fboStatus);
    }
    
    // Step 3: Render triangle to FBO
    NSLog(@"Step 3: Rendering triangle to FBO...");
    
    // Triangle vertices (position + color)
    float triangleVertices[] = {
        // x      y      z     r     g     b     a
         0.0f,  0.5f,  0.0f, 1.0f, 0.0f, 0.0f, 1.0f,  // Top (red)
        -0.5f, -0.5f,  0.0f, 0.0f, 1.0f, 0.0f, 1.0f,  // Bottom-left (green)
         0.5f, -0.5f,  0.0f, 0.0f, 0.0f, 1.0f, 1.0f   // Bottom-right (blue)
    };
    
    NSLog(@"Creating triangle VBO and VAO...");
    uint32_t triangleVBO = metal_gen_buffer();
    NSLog(@"Triangle VBO created: %u", triangleVBO);
    uint32_t triangleVAO = metal_gen_vao();
    NSLog(@"Triangle VAO created: %u", triangleVAO);
    
    metal_bind_vao(triangleVAO);
    NSLog(@"Triangle VAO bound");
    metal_bind_buffer(GL_ARRAY_BUFFER, triangleVBO);
    metal_buffer_data(GL_ARRAY_BUFFER, sizeof(triangleVertices), triangleVertices, GL_STATIC_DRAW);
    
    metal_vertex_attrib_pointer(0, 3, GL_FLOAT, 0, 7 * sizeof(float), 0);
    metal_enable_vertex_attrib_array(0);
    
    metal_vertex_attrib_pointer(1, 4, GL_FLOAT, 0, 7 * sizeof(float), 3 * sizeof(float));
    metal_enable_vertex_attrib_array(1);
    
    // Draw triangle to FBO (FBO is currently bound)
    metal_draw_arrays(GL_TRIANGLES, 0, 3);
    NSLog(@"✅ Triangle rendered to FBO");
    
    // Step 4: Create textured shader program
    NSLog(@"Creating textured shader program...");
    
    const char* vertexShaderSource = 
        "attribute vec3 position;\n"
        "attribute vec2 texCoord;\n"
        "varying vec2 v_texCoord;\n"
        "void main() {\n"
        "    gl_Position = vec4(position, 1.0);\n"
        "    v_texCoord = texCoord;\n"
        "}\n";
    
    const char* fragmentShaderSource = 
        "varying vec2 v_texCoord;\n"
        "uniform sampler2D tex;\n"
        "void main() {\n"
        "    gl_FragColor = texture2D(tex, v_texCoord);\n"
        "}\n";
    
    uint32_t vertexShader = metal_create_shader(GL_VERTEX_SHADER);
    metal_shader_source(vertexShader, vertexShaderSource);
    metal_compile_shader(vertexShader, GL_VERTEX_SHADER);
    
    uint32_t fragmentShader = metal_create_shader(GL_FRAGMENT_SHADER);
    metal_shader_source(fragmentShader, fragmentShaderSource);
    metal_compile_shader(fragmentShader, GL_FRAGMENT_SHADER);
    
    uint32_t texturedProgram = metal_create_program();
    metal_attach_shader(texturedProgram, vertexShader, GL_VERTEX_SHADER);
    metal_attach_shader(texturedProgram, fragmentShader, GL_FRAGMENT_SHADER);
    metal_link_program(texturedProgram);
    
    metal_use_program(texturedProgram);
    NSLog(@"✅ Textured shader program created and activated");
    
    // Step 5: Bind default framebuffer and render textured quad
    NSLog(@"Rendering textured quad to screen...");
    metal_bind_framebuffer(GL_FRAMEBUFFER, 0);  // Bind default framebuffer (screen)
    
    // Textured quad vertices (position + texcoord)
    float quadVertices[] = {
        // x      y      z     u     v
        -0.8f,  0.8f,  0.0f, 0.0f, 1.0f,  // Top-left
        -0.8f, -0.8f,  0.0f, 0.0f, 0.0f,  // Bottom-left
         0.8f,  0.8f,  0.0f, 1.0f, 1.0f,  // Top-right
         0.8f, -0.8f,  0.0f, 1.0f, 0.0f   // Bottom-right
    };
    
    uint32_t quadVBO = metal_gen_buffer();
    uint32_t quadVAO = metal_gen_vao();
    
    metal_bind_vao(quadVAO);
    metal_bind_buffer(GL_ARRAY_BUFFER, quadVBO);
    metal_buffer_data(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    
    metal_vertex_attrib_pointer(0, 3, GL_FLOAT, 0, 5 * sizeof(float), 0);
    metal_enable_vertex_attrib_array(0);
    
    metal_vertex_attrib_pointer(1, 2, GL_FLOAT, 0, 5 * sizeof(float), 3 * sizeof(float));
    metal_enable_vertex_attrib_array(1);
    
    // Bind FBO texture and draw quad to screen
    metal_bind_texture(GL_TEXTURE_2D, fboTexture);
    metal_draw_arrays(GL_TRIANGLE_STRIP, 0, 4);
    NSLog(@"✅ Quad rendered to screen with FBO texture");
    
    NSLog(@"=== Phase 5 FBO Test Complete ===");
    NSLog(@"Expected: Colored triangle should appear on textured quad");
    
    self.fboTextureID = fboTexture;
}

@end

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        NSApplication *app = [NSApplication sharedApplication];
        
        NSRect frame = NSMakeRect(0, 0, 800, 600);
        NSWindow *window = [[NSWindow alloc] initWithContentRect:frame
                                                       styleMask:(NSWindowStyleMaskTitled |
                                                                NSWindowStyleMaskClosable |
                                                                NSWindowStyleMaskResizable)
                                                         backing:NSBackingStoreBuffered
                                                           defer:NO];
        
        window.title = @"Phase 5 FBO Test";
        [window center];
        
        TestView *view = [[TestView alloc] initWithFrame:frame];
        window.contentView = view;
        
        [window makeKeyAndOrderFront:nil];
        
        // Setup and render after window is visible
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 0.5 * NSEC_PER_SEC), dispatch_get_main_queue(), ^{
            [view setupAndRender];
        });
        
        [app run];
    }
    return 0;
}

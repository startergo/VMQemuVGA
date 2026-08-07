//
//  Phase 3 Texture Test - Textured Quad with Checkerboard
//  Tests: glGenTextures, glBindTexture, glTexImage2D, glTexParameteri
//

#import <Cocoa/Cocoa.h>
#import <OpenGL/gl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#define METAL_SERVER_IP "127.0.0.1"
#define METAL_SERVER_PORT 28123

// Command opcodes (matching metal_server.m)
typedef enum {
    CMD_METAL_CLEAR = 6,
    CMD_METAL_GEN_BUFFERS = 10,
    CMD_METAL_BIND_BUFFER = 11,
    CMD_METAL_BUFFER_DATA = 12,
    CMD_METAL_DRAW_ARRAYS = 20,
    CMD_METAL_GEN_TEXTURES = 22,
    CMD_METAL_BIND_TEXTURE = 23,
    CMD_METAL_TEX_IMAGE_2D = 24,
    CMD_METAL_TEX_PARAMETERI = 26,
} MetalCommand;

// OpenGL constants
#define GL_ARRAY_BUFFER 0x8892
#define GL_STATIC_DRAW 0x88E4
#define GL_TRIANGLES 0x0004
#define GL_TEXTURE_2D 0x0DE1
#define GL_RGBA 0x1908
#define GL_UNSIGNED_BYTE 0x1401
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_LINEAR 0x2601
#define GL_NEAREST 0x2600

static int g_metal_socket = -1;
static BOOL g_metal_connected = NO;

// Connect to Metal server
static void connect_metal_server() {
    g_metal_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (g_metal_socket < 0) {
        NSLog(@"[Texture Test] ERROR: Failed to create socket");
        return;
    }
    
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(METAL_SERVER_PORT);
    inet_pton(AF_INET, METAL_SERVER_IP, &server_addr.sin_addr);
    
    if (connect(g_metal_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        NSLog(@"[Texture Test] ERROR: Failed to connect to Metal server");
        close(g_metal_socket);
        g_metal_socket = -1;
        return;
    }
    
    g_metal_connected = YES;
    NSLog(@"[Texture Test] ✅ Connected! Using M4 Pro Metal GPU");
}

static void send_metal_cmd(uint32_t cmd) {
    send(g_metal_socket, &cmd, sizeof(cmd), 0);
}

// Texture wrapper functions
static void metal_glGenTextures(GLsizei n, GLuint *textures) {
    if (g_metal_connected) {
        send_metal_cmd(CMD_METAL_GEN_TEXTURES);
        uint32_t count = n;
        send(g_metal_socket, &count, sizeof(count), 0);
        recv(g_metal_socket, textures, n * sizeof(uint32_t), 0);
        
        NSLog(@"[Texture Test] → glGenTextures(%d) = %u", n, textures[0]);
    }
}

static void metal_glBindTexture(GLenum target, GLuint texture) {
    if (g_metal_connected) {
        send_metal_cmd(CMD_METAL_BIND_TEXTURE);
        uint32_t t = target;
        uint32_t tex = texture;
        send(g_metal_socket, &t, sizeof(t), 0);
        send(g_metal_socket, &tex, sizeof(tex), 0);
        NSLog(@"[Texture Test] → glBindTexture(0x%X, %u)", target, texture);
    }
}

static void metal_glTexImage2D(GLenum target, GLint level, GLint internalFormat,
                               GLsizei width, GLsizei height, GLint border,
                               GLenum format, GLenum type, const void *pixels) {
    if (g_metal_connected && pixels != NULL) {
        send_metal_cmd(CMD_METAL_TEX_IMAGE_2D);
        uint32_t t = target;
        uint32_t l = level;
        uint32_t ifmt = internalFormat;
        uint32_t w = width;
        uint32_t h = height;
        uint32_t b = border;
        uint32_t fmt = format;
        uint32_t typ = type;
        
        send(g_metal_socket, &t, sizeof(t), 0);
        send(g_metal_socket, &l, sizeof(l), 0);
        send(g_metal_socket, &ifmt, sizeof(ifmt), 0);
        send(g_metal_socket, &w, sizeof(w), 0);
        send(g_metal_socket, &h, sizeof(h), 0);
        send(g_metal_socket, &b, sizeof(b), 0);
        send(g_metal_socket, &fmt, sizeof(fmt), 0);
        send(g_metal_socket, &typ, sizeof(typ), 0);
        
        // Send pixel data (RGBA = 4 bytes per pixel)
        size_t dataSize = width * height * 4;
        send(g_metal_socket, pixels, dataSize, 0);
        
        NSLog(@"[Texture Test] → glTexImage2D(%ux%u, %zu bytes)", width, height, dataSize);
    }
}

static void metal_glTexParameteri(GLenum target, GLenum pname, GLint param) {
    if (g_metal_connected) {
        send_metal_cmd(CMD_METAL_TEX_PARAMETERI);
        uint32_t t = target;
        uint32_t pn = pname;
        uint32_t p = param;
        send(g_metal_socket, &t, sizeof(t), 0);
        send(g_metal_socket, &pn, sizeof(pn), 0);
        send(g_metal_socket, &p, sizeof(p), 0);
        NSLog(@"[Texture Test] → glTexParameteri(pname=0x%X, param=0x%X)", pname, param);
    }
}

// VBO wrapper functions (reused from Phase 1)
static void metal_glGenBuffers(GLsizei n, GLuint *buffers) {
    if (g_metal_connected) {
        send_metal_cmd(CMD_METAL_GEN_BUFFERS);
        uint32_t count = n;
        send(g_metal_socket, &count, sizeof(count), 0);
        recv(g_metal_socket, buffers, n * sizeof(uint32_t), 0);
        
        NSLog(@"[Texture Test] → glGenBuffers(%d) = %u", n, buffers[0]);
    }
}

static void metal_glBindBuffer(GLenum target, GLuint buffer) {
    if (g_metal_connected) {
        send_metal_cmd(CMD_METAL_BIND_BUFFER);
        uint32_t t = target;
        uint32_t b = buffer;
        send(g_metal_socket, &t, sizeof(t), 0);
        send(g_metal_socket, &b, sizeof(b), 0);
        NSLog(@"[Texture Test] → glBindBuffer(0x%X, %u)", target, buffer);
    }
}

static void metal_glBufferData(GLenum target, GLsizeiptr size, const void *data, GLenum usage) {
    if (g_metal_connected && data != NULL) {
        send_metal_cmd(CMD_METAL_BUFFER_DATA);
        uint32_t t = target;
        uint64_t s = size;
        uint32_t u = usage;
        
        send(g_metal_socket, &t, sizeof(t), 0);
        send(g_metal_socket, &s, sizeof(s), 0);
        send(g_metal_socket, &u, sizeof(u), 0);
        send(g_metal_socket, data, size, 0);
        
        NSLog(@"[Texture Test] → glBufferData(0x%X, %lld bytes, 0x%X)", target, (long long)size, usage);
    }
}

static void metal_glDrawArrays(GLenum mode, GLint first, GLsizei count) {
    if (g_metal_connected) {
        send_metal_cmd(CMD_METAL_DRAW_ARRAYS);
        uint32_t m = mode;
        uint32_t f = first;
        uint32_t c = count;
        
        send(g_metal_socket, &m, sizeof(m), 0);
        send(g_metal_socket, &f, sizeof(f), 0);
        send(g_metal_socket, &c, sizeof(c), 0);
    }
}

static void metal_glClear(GLbitfield mask) {
    if (g_metal_connected) {
        send_metal_cmd(CMD_METAL_CLEAR);
        float rgba[4] = {0.1f, 0.1f, 0.2f, 1.0f};
        send(g_metal_socket, rgba, sizeof(rgba), 0);
    }
}

// Test View
@interface TestTextureView : NSOpenGLView {
    GLuint vbo;
    GLuint texture;
    BOOL initialized;
}
@end

@implementation TestTextureView

- (void)prepareOpenGL {
    [super prepareOpenGL];
    
    initialized = NO;
    
    // Quad vertices: position(x,y,z) + color(r,g,b,a) + texCoord(u,v) = 9 floats
    float vertices[] = {
        // Bottom-left
        -0.8f, -0.8f, 0.0f,  1.0f, 1.0f, 1.0f, 1.0f,  0.0f, 0.0f,
        // Bottom-right
         0.8f, -0.8f, 0.0f,  1.0f, 1.0f, 1.0f, 1.0f,  1.0f, 0.0f,
        // Top-right
         0.8f,  0.8f, 0.0f,  1.0f, 1.0f, 1.0f, 1.0f,  1.0f, 1.0f,
        
        // Bottom-left
        -0.8f, -0.8f, 0.0f,  1.0f, 1.0f, 1.0f, 1.0f,  0.0f, 0.0f,
        // Top-right
         0.8f,  0.8f, 0.0f,  1.0f, 1.0f, 1.0f, 1.0f,  1.0f, 1.0f,
        // Top-left
        -0.8f,  0.8f, 0.0f,  1.0f, 1.0f, 1.0f, 1.0f,  0.0f, 1.0f,
    };
    
    NSLog(@"========================================");
    NSLog(@"  Phase 3 Texture Test");
    NSLog(@"  Testing: glGenTextures, glBindTexture");
    NSLog(@"           glTexImage2D, glTexParameteri");
    NSLog(@"========================================");
    
    // Create checkerboard texture (8x8 pixels, 4x4 checkers)
    const int texWidth = 8;
    const int texHeight = 8;
    uint8_t textureData[texWidth * texHeight * 4];
    
    for (int y = 0; y < texHeight; y++) {
        for (int x = 0; x < texWidth; x++) {
            int checker = ((x / 2) + (y / 2)) % 2;
            uint8_t color = checker ? 255 : 64;
            
            int idx = (y * texWidth + x) * 4;
            textureData[idx + 0] = color;  // R
            textureData[idx + 1] = color;  // G
            textureData[idx + 2] = color;  // B
            textureData[idx + 3] = 255;    // A
        }
    }
    
    // Create texture
    metal_glGenTextures(1, &texture);
    metal_glBindTexture(GL_TEXTURE_2D, texture);
    metal_glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texWidth, texHeight, 0, 
                       GL_RGBA, GL_UNSIGNED_BYTE, textureData);
    metal_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    metal_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    
    NSLog(@"[Texture Test] ✅ Checkerboard texture created (%dx%d pixels)", texWidth, texHeight);
    
    // Create VBO for quad
    metal_glGenBuffers(1, &vbo);
    metal_glBindBuffer(GL_ARRAY_BUFFER, vbo);
    metal_glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    
    NSLog(@"[Texture Test] ✅ Quad VBO initialized (6 vertices, 9 floats each)");
    
    initialized = YES;
    
    // Start animation
    [NSTimer scheduledTimerWithTimeInterval:0.016
                                     target:self
                                   selector:@selector(animate:)
                                   userInfo:nil
                                    repeats:YES];
}

- (void)animate:(NSTimer*)timer {
    [self setNeedsDisplay:YES];
}

- (void)drawRect:(NSRect)dirtyRect {
    if (!initialized) return;
    
    // Clear
    metal_glClear(GL_COLOR_BUFFER_BIT);
    
    // Draw textured quad
    metal_glDrawArrays(GL_TRIANGLES, 0, 6);
    
    [[self openGLContext] flushBuffer];
}

@end

@interface AppDelegate : NSObject <NSApplicationDelegate>
@property (strong) NSWindow *window;
@end

@implementation AppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)notification {
    // Connect to Metal server
    connect_metal_server();
    
    if (!g_metal_connected) {
        NSLog(@"[Texture Test] ⚠️  Metal server not available - exiting");
        [NSApp terminate:nil];
        return;
    }
    
    NSRect frame = NSMakeRect(100, 100, 800, 600);
    NSUInteger styleMask = NSWindowStyleMaskTitled | 
                          NSWindowStyleMaskClosable | 
                          NSWindowStyleMaskMiniaturizable;
    
    _window = [[NSWindow alloc] initWithContentRect:frame
                                          styleMask:styleMask
                                            backing:NSBackingStoreBuffered
                                              defer:NO];
    [_window setTitle:@"Phase 3: Texture Test - Checkerboard Quad"];
    
    NSOpenGLPixelFormatAttribute attrs[] = {
        NSOpenGLPFADoubleBuffer,
        NSOpenGLPFADepthSize, 24,
        NSOpenGLPFAOpenGLProfile, NSOpenGLProfileVersionLegacy,
        0
    };
    
    NSOpenGLPixelFormat *pixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:attrs];
    TestTextureView *glView = [[TestTextureView alloc] initWithFrame:frame pixelFormat:pixelFormat];
    
    [_window setContentView:glView];
    [_window makeKeyAndOrderFront:nil];
    [_window center];
}

- (void)applicationWillTerminate:(NSNotification *)notification {
    if (g_metal_socket >= 0) {
        close(g_metal_socket);
        NSLog(@"[Texture Test] Disconnected from Metal server");
    }
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender {
    return YES;
}

@end

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        NSApplication *app = [NSApplication sharedApplication];
        AppDelegate *delegate = [[AppDelegate alloc] init];
        [app setDelegate:delegate];
        [app run];
    }
    return 0;
}

//
//  Test VBO-based rendering with Metal translation
//  Phase 1 validation: glGenBuffers, glBindBuffer, glBufferData, glDrawArrays
//

#import <Cocoa/Cocoa.h>
#import <OpenGL/gl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

// Metal command opcodes (match server)
typedef enum {
    CMD_METAL_CLEAR = 6,
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
} MetalCommand;

// OpenGL constants
#define GL_ARRAY_BUFFER 0x8892
#define GL_STATIC_DRAW 0x88E4
#define GL_TRIANGLES 0x0004

// Global Metal connection
static int g_metal_socket = -1;
static int g_metal_connected = 0;

#define HOST_IP_METAL "127.0.0.1"
#define HOST_PORT_METAL 28123

// Connect to Metal server
static void connect_metal_server(void) {
    if (g_metal_connected) return;
    
    NSLog(@"[VBO Test] Connecting to Metal server %s:%d...", HOST_IP_METAL, HOST_PORT_METAL);
    
    g_metal_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (g_metal_socket < 0) {
        NSLog(@"[VBO Test] ERROR: Failed to create socket");
        return;
    }
    
    struct sockaddr_in serverAddr = {0};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(HOST_PORT_METAL);
    
    if (inet_pton(AF_INET, HOST_IP_METAL, &serverAddr.sin_addr) <= 0) {
        NSLog(@"[VBO Test] ERROR: Invalid address");
        close(g_metal_socket);
        g_metal_socket = -1;
        return;
    }
    
    if (connect(g_metal_socket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        NSLog(@"[VBO Test] WARNING: Could not connect to Metal server");
        close(g_metal_socket);
        g_metal_socket = -1;
        return;
    }
    
    g_metal_connected = 1;
    NSLog(@"[VBO Test] ✅ Connected! Using M4 Pro Metal GPU");
}

static void send_metal_cmd(uint32_t cmd) {
    if (g_metal_connected && g_metal_socket >= 0) {
        send(g_metal_socket, &cmd, sizeof(cmd), 0);
    }
}

// VBO wrapper functions
static void metal_glGenBuffers(GLsizei n, GLuint *buffers) {
    if (g_metal_connected) {
        send_metal_cmd(CMD_METAL_GEN_BUFFERS);
        uint32_t count = n;
        send(g_metal_socket, &count, sizeof(count), 0);
        
        // Receive Metal-side buffer IDs
        recv(g_metal_socket, buffers, n * sizeof(uint32_t), 0);
        
        NSLog(@"[VBO Test] → glGenBuffers(%d) = %u", n, buffers[0]);
    }
}

static void metal_glBindBuffer(GLenum target, GLuint buffer) {
    if (g_metal_connected) {
        send_metal_cmd(CMD_METAL_BIND_BUFFER);
        uint32_t t = target;
        uint32_t b = buffer;
        send(g_metal_socket, &t, sizeof(t), 0);
        send(g_metal_socket, &b, sizeof(b), 0);
        NSLog(@"[VBO Test] → glBindBuffer(0x%X, %u)", target, buffer);
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
        
        NSLog(@"[VBO Test] → glBufferData(0x%X, %lld bytes, 0x%X)", target, (long long)size, usage);
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
        
        NSLog(@"[VBO Test] → glDrawArrays(0x%X, %d, %d)", mode, first, count);
    }
}

static void metal_glClear(GLbitfield mask) {
    if (g_metal_connected) {
        send_metal_cmd(CMD_METAL_CLEAR);
        float rgba[4] = {0.2f, 0.3f, 0.4f, 1.0f};
        send(g_metal_socket, rgba, sizeof(rgba), 0);
    }
}

// Test View
@interface TestVBOView : NSOpenGLView {
    GLuint vbo;
    float angle;
    BOOL vboInitialized;
}
@end

@implementation TestVBOView

- (void)prepareOpenGL {
    [super prepareOpenGL];
    
    vboInitialized = NO;
    angle = 0.0f;
    
    // Triangle vertices: position(x,y,z) + color(r,g,b,a) = 7 floats per vertex
    float vertices[] = {
        // Top vertex (red)
        0.0f,  0.8f, 0.0f,  1.0f, 0.0f, 0.0f, 1.0f,
        // Bottom-left vertex (green)
       -0.8f, -0.8f, 0.0f,  0.0f, 1.0f, 0.0f, 1.0f,
        // Bottom-right vertex (blue)
        0.8f, -0.8f, 0.0f,  0.0f, 0.0f, 1.0f, 1.0f
    };
    
    NSLog(@"========================================");
    NSLog(@"  Phase 1 VBO Test");
    NSLog(@"  Testing: glGenBuffers, glBindBuffer");
    NSLog(@"           glBufferData, glDrawArrays");
    NSLog(@"========================================");
    
    // Create VBO
    metal_glGenBuffers(1, &vbo);
    metal_glBindBuffer(GL_ARRAY_BUFFER, vbo);
    metal_glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    
    vboInitialized = YES;
    NSLog(@"[VBO Test] ✅ VBO initialized (buffer ID: %u)", vbo);
    
    // Start animation
    [NSTimer scheduledTimerWithTimeInterval:0.016
                                     target:self
                                   selector:@selector(animate:)
                                   userInfo:nil
                                    repeats:YES];
}

- (void)animate:(NSTimer*)timer {
    angle += 2.0f;
    if (angle >= 360.0f) angle = 0.0f;
    [self setNeedsDisplay:YES];
}

- (void)drawRect:(NSRect)dirtyRect {
    if (!vboInitialized) return;
    
    // Clear
    metal_glClear(GL_COLOR_BUFFER_BIT);
    
    // Draw from VBO
    metal_glDrawArrays(GL_TRIANGLES, 0, 3);
    
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
        NSLog(@"[VBO Test] ⚠️  Metal server not available - exiting");
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
    [_window setTitle:@"Phase 1 VBO Test - Metal Translation"];
    
    NSOpenGLPixelFormatAttribute attrs[] = {
        NSOpenGLPFADoubleBuffer,
        NSOpenGLPFAColorSize, 24,
        NSOpenGLPFADepthSize, 24,
        0
    };
    NSOpenGLPixelFormat *pixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:attrs];
    TestVBOView *glView = [[TestVBOView alloc] initWithFrame:frame pixelFormat:pixelFormat];
    
    [_window setContentView:glView];
    [_window makeKeyAndOrderFront:nil];
    [_window center];
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender {
    return YES;
}

- (void)applicationWillTerminate:(NSNotification *)notification {
    if (g_metal_socket >= 0) {
        NSLog(@"[VBO Test] Disconnecting...");
        close(g_metal_socket);
    }
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

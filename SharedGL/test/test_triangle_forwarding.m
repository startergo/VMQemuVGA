//
//  Test OpenGL Application with Built-in Forwarding
//  Simple spinning triangle with SharedGL forwarding compiled in
//

#import <Cocoa/Cocoa.h>
#import <OpenGL/gl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

// SharedGL Command opcodes
typedef enum {
    CMD_GL_BEGIN = 1,
    CMD_GL_END,
    CMD_GL_VERTEX3F,
    CMD_GL_COLOR3F,
    CMD_GL_CLEAR,
    CMD_GL_FLUSH,
    CMD_GL_VIEWPORT,
    CMD_GL_MATRIX_MODE,
    CMD_GL_LOAD_IDENTITY,
    CMD_GL_ORTHO,
    CMD_GL_SWAP_BUFFERS,
} GLCommand;

// Global connection state
static int g_socket = -1;
static int g_connected = 0;

#define HOST_IP "127.0.0.1"
#define HOST_PORT 28122

// Connect to SharedGL server
static void connect_to_server(void) {
    if (g_connected) return;
    
    NSLog(@"[SharedGL] Connecting to server %s:%d...", HOST_IP, HOST_PORT);
    
    g_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (g_socket < 0) {
        NSLog(@"[SharedGL] ERROR: Failed to create socket");
        return;
    }
    
    struct sockaddr_in serverAddr = {0};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(HOST_PORT);
    
    if (inet_pton(AF_INET, HOST_IP, &serverAddr.sin_addr) <= 0) {
        NSLog(@"[SharedGL] ERROR: Invalid address");
        close(g_socket);
        g_socket = -1;
        return;
    }
    
    if (connect(g_socket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        NSLog(@"[SharedGL] WARNING: Could not connect to server");
        NSLog(@"[SharedGL] Using local rendering only");
        close(g_socket);
        g_socket = -1;
        return;
    }
    
    g_connected = 1;
    NSLog(@"[SharedGL] ✅ Connected! Forwarding to M4 Pro GPU");
}

// Send command to server
static void send_gl_command(uint32_t cmd) {
    if (g_connected && g_socket >= 0) {
        send(g_socket, &cmd, sizeof(cmd), 0);
    }
}

@interface TestGLView : NSOpenGLView {
    float angle;
}
@end

@implementation TestGLView

- (void)prepareOpenGL {
    [super prepareOpenGL];
    
    // Try to connect to SharedGL server
    connect_to_server();
    
    glClearColor(0.2f, 0.3f, 0.4f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    
    // Setup viewport
    NSRect bounds = [self bounds];
    glViewport(0, 0, bounds.size.width, bounds.size.height);
    
    // Forward viewport command
    if (g_connected) {
        send_gl_command(CMD_GL_VIEWPORT);
        int32_t viewport[4] = {0, 0, (int32_t)bounds.size.width, (int32_t)bounds.size.height};
        send(g_socket, viewport, sizeof(viewport), 0);
    }
    
    // Setup projection
    glMatrixMode(GL_PROJECTION);
    if (g_connected) {
        send_gl_command(CMD_GL_MATRIX_MODE);
        uint32_t mode = GL_PROJECTION;
        send(g_socket, &mode, sizeof(mode), 0);
    }
    
    glLoadIdentity();
    if (g_connected) {
        send_gl_command(CMD_GL_LOAD_IDENTITY);
    }
    
    glOrtho(-2.0, 2.0, -2.0, 2.0, -1.0, 1.0);
    if (g_connected) {
        send_gl_command(CMD_GL_ORTHO);
        double ortho[6] = {-2.0, 2.0, -2.0, 2.0, -1.0, 1.0};
        send(g_socket, ortho, sizeof(ortho), 0);
    }
    
    glMatrixMode(GL_MODELVIEW);
    if (g_connected) {
        send_gl_command(CMD_GL_MATRIX_MODE);
        uint32_t mode = GL_MODELVIEW;
        send(g_socket, &mode, sizeof(mode), 0);
    }
    
    glLoadIdentity();
    if (g_connected) {
        send_gl_command(CMD_GL_LOAD_IDENTITY);
    }
    
    angle = 0.0f;
    
    // Start animation timer
    [NSTimer scheduledTimerWithTimeInterval:0.016 // ~60 FPS
                                     target:self
                                   selector:@selector(animate:)
                                   userInfo:nil
                                    repeats:YES];
}

- (void)animate:(NSTimer*)timer {
    angle += 2.0f;
    if (angle >= 360.0f) {
        angle = 0.0f;
    }
    [self setNeedsDisplay:YES];
}

- (void)drawRect:(NSRect)dirtyRect {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (g_connected) {
        send_gl_command(CMD_GL_CLEAR);
        uint32_t mask = GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT;
        send(g_socket, &mask, sizeof(mask), 0);
    }
    
    glLoadIdentity();
    if (g_connected) {
        send_gl_command(CMD_GL_LOAD_IDENTITY);
    }
    
    // Rotate triangle
    glRotatef(angle, 0.0f, 0.0f, 1.0f);
    
    // Draw triangle with forwarding
    glBegin(GL_TRIANGLES);
    if (g_connected) {
        send_gl_command(CMD_GL_BEGIN);
        uint32_t mode = GL_TRIANGLES;
        send(g_socket, &mode, sizeof(mode), 0);
    }
    
    glColor3f(1.0f, 0.0f, 0.0f);  // Red
    if (g_connected) {
        send_gl_command(CMD_GL_COLOR3F);
        float c[3] = {1.0f, 0.0f, 0.0f};
        send(g_socket, c, sizeof(c), 0);
    }
    
    glVertex3f(0.0f, 1.0f, 0.0f);
    if (g_connected) {
        send_gl_command(CMD_GL_VERTEX3F);
        float v[3] = {0.0f, 1.0f, 0.0f};
        send(g_socket, v, sizeof(v), 0);
    }
    
    glColor3f(0.0f, 1.0f, 0.0f);  // Green
    if (g_connected) {
        send_gl_command(CMD_GL_COLOR3F);
        float c[3] = {0.0f, 1.0f, 0.0f};
        send(g_socket, c, sizeof(c), 0);
    }
    
    glVertex3f(-1.0f, -1.0f, 0.0f);
    if (g_connected) {
        send_gl_command(CMD_GL_VERTEX3F);
        float v[3] = {-1.0f, -1.0f, 0.0f};
        send(g_socket, v, sizeof(v), 0);
    }
    
    glColor3f(0.0f, 0.0f, 1.0f);  // Blue
    if (g_connected) {
        send_gl_command(CMD_GL_COLOR3F);
        float c[3] = {0.0f, 0.0f, 1.0f};
        send(g_socket, c, sizeof(c), 0);
    }
    
    glVertex3f(1.0f, -1.0f, 0.0f);
    if (g_connected) {
        send_gl_command(CMD_GL_VERTEX3F);
        float v[3] = {1.0f, -1.0f, 0.0f};
        send(g_socket, v, sizeof(v), 0);
    }
    
    glEnd();
    if (g_connected) {
        send_gl_command(CMD_GL_END);
    }
    
    glFlush();
    if (g_connected) {
        send_gl_command(CMD_GL_FLUSH);
    }
    
    [[self openGLContext] flushBuffer];
    if (g_connected) {
        send_gl_command(CMD_GL_SWAP_BUFFERS);
    }
}

- (void)dealloc {
    if (g_socket >= 0) {
        NSLog(@"[SharedGL] Disconnecting...");
        close(g_socket);
    }
}

@end

@interface AppDelegate : NSObject <NSApplicationDelegate>
@property (strong) NSWindow *window;
@end

@implementation AppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)notification {
    NSLog(@"========================================");
    NSLog(@"  SharedGL Test - M4 Pro GPU Forwarding");
    NSLog(@"  Spinning Triangle Demo");
    NSLog(@"========================================");
    
    // Create window
    NSRect frame = NSMakeRect(100, 100, 800, 600);
    NSUInteger styleMask = NSWindowStyleMaskTitled | 
                          NSWindowStyleMaskClosable | 
                          NSWindowStyleMaskMiniaturizable;
    
    _window = [[NSWindow alloc] initWithContentRect:frame
                                          styleMask:styleMask
                                            backing:NSBackingStoreBuffered
                                              defer:NO];
    [_window setTitle:@"SharedGL - M4 Pro GPU Acceleration"];
    
    // Create OpenGL view
    NSOpenGLPixelFormatAttribute attrs[] = {
        NSOpenGLPFADoubleBuffer,
        NSOpenGLPFAColorSize, 24,
        NSOpenGLPFADepthSize, 24,
        0
    };
    NSOpenGLPixelFormat *pixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:attrs];
    TestGLView *glView = [[TestGLView alloc] initWithFrame:frame pixelFormat:pixelFormat];
    
    [_window setContentView:glView];
    [_window makeKeyAndOrderFront:nil];
    [_window center];
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

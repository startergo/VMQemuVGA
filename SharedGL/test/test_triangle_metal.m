//
//  Test OpenGL Application with Metal Translation
//  OpenGL calls directly translated to Metal commands (no dylib injection needed)
//

#import <Cocoa/Cocoa.h>
#import <OpenGL/gl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

// Metal command opcodes (match server)
typedef enum {
    CMD_METAL_CREATE_BUFFER = 1,
    CMD_METAL_SET_VERTEX_BUFFER,
    CMD_METAL_SET_FRAGMENT_BYTES,
    CMD_METAL_DRAW_PRIMITIVES,
    CMD_METAL_PRESENT,
    CMD_METAL_CLEAR,
    CMD_METAL_SET_VIEWPORT,
} MetalCommand;

typedef enum {
    METAL_PRIMITIVE_TRIANGLE = 3,
    METAL_PRIMITIVE_TRIANGLE_STRIP = 4,
    METAL_PRIMITIVE_LINE = 2,
} MetalPrimitiveType;

// Global Metal connection
static int g_metal_socket = -1;
static int g_metal_connected = 0;

#define HOST_IP_METAL "127.0.0.1"
#define HOST_PORT_METAL 28123

// OpenGL state for batching
static GLenum g_currentPrimitive = 0;
static int g_vertexCount = 0;
static float g_currentColor[4] = {1.0f, 1.0f, 1.0f, 1.0f};

#define MAX_VERTICES 10000
static float g_vertices[MAX_VERTICES * 7]; // position(3) + color(4)
static int g_vertexIndex = 0;

// Connect to Metal server
static void connect_metal_server(void) {
    if (g_metal_connected) return;
    
    NSLog(@"[Metal] Connecting to Metal server %s:%d...", HOST_IP_METAL, HOST_PORT_METAL);
    
    g_metal_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (g_metal_socket < 0) {
        NSLog(@"[Metal] ERROR: Failed to create socket");
        return;
    }
    
    struct sockaddr_in serverAddr = {0};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(HOST_PORT_METAL);
    
    if (inet_pton(AF_INET, HOST_IP_METAL, &serverAddr.sin_addr) <= 0) {
        NSLog(@"[Metal] ERROR: Invalid address");
        close(g_metal_socket);
        g_metal_socket = -1;
        return;
    }
    
    if (connect(g_metal_socket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        NSLog(@"[Metal] WARNING: Could not connect to Metal server");
        NSLog(@"[Metal] Using local OpenGL rendering only");
        close(g_metal_socket);
        g_metal_socket = -1;
        return;
    }
    
    g_metal_connected = 1;
    NSLog(@"[Metal] ✅ Connected! Using M4 Pro Metal GPU");
}

// Send Metal command
static void send_metal_cmd(uint32_t cmd) {
    if (g_metal_connected && g_metal_socket >= 0) {
        send(g_metal_socket, &cmd, sizeof(cmd), 0);
    }
}

// Metal-aware OpenGL wrappers
static void metal_glClear(GLbitfield mask) {
    if (g_metal_connected) {
        send_metal_cmd(CMD_METAL_CLEAR);
        float rgba[4] = {0.2f, 0.3f, 0.4f, 1.0f};
        send(g_metal_socket, rgba, sizeof(rgba), 0);
        NSLog(@"[Metal] → glClear sent");
    }
    glClear(mask);
}

static void metal_glViewport(GLint x, GLint y, GLsizei w, GLsizei h) {
    if (g_metal_connected) {
        send_metal_cmd(CMD_METAL_SET_VIEWPORT);
        float viewport[4] = {(float)x, (float)y, (float)w, (float)h};
        send(g_metal_socket, viewport, sizeof(viewport), 0);
        NSLog(@"[Metal] → glViewport sent");
    }
    glViewport(x, y, w, h);
}

static void metal_glBegin(GLenum mode) {
    g_currentPrimitive = mode;
    g_vertexCount = 0;
    g_vertexIndex = 0;
    NSLog(@"[Metal] glBegin(%u) - starting batch", mode);
    glBegin(mode);
}

static void metal_glColor3f(GLfloat r, GLfloat g, GLfloat b) {
    g_currentColor[0] = r;
    g_currentColor[1] = g;
    g_currentColor[2] = b;
    g_currentColor[3] = 1.0f;
    glColor3f(r, g, b);
}

static void metal_glVertex3f(GLfloat x, GLfloat y, GLfloat z) {
    if (g_vertexIndex < MAX_VERTICES * 7) {
        g_vertices[g_vertexIndex++] = x;
        g_vertices[g_vertexIndex++] = y;
        g_vertices[g_vertexIndex++] = z;
        g_vertices[g_vertexIndex++] = g_currentColor[0];
        g_vertices[g_vertexIndex++] = g_currentColor[1];
        g_vertices[g_vertexIndex++] = g_currentColor[2];
        g_vertices[g_vertexIndex++] = g_currentColor[3];
        g_vertexCount++;
    }
    glVertex3f(x, y, z);
}

static void metal_glEnd(void) {
    if (g_metal_connected && g_vertexCount > 0) {
        send_metal_cmd(CMD_METAL_DRAW_PRIMITIVES);
        
        uint32_t primitiveType = METAL_PRIMITIVE_TRIANGLE; // Assume triangles
        uint32_t vertexStart = 0;
        uint32_t vertexCount = g_vertexCount;
        
        send(g_metal_socket, &primitiveType, sizeof(primitiveType), 0);
        send(g_metal_socket, &vertexStart, sizeof(vertexStart), 0);
        send(g_metal_socket, &vertexCount, sizeof(vertexCount), 0);
        
        // Send vertex data
        size_t dataSize = g_vertexCount * 7 * sizeof(float);
        send(g_metal_socket, g_vertices, dataSize, 0);
        
        NSLog(@"[Metal] → glEnd: sent %d vertices (%zu bytes)", g_vertexCount, dataSize);
    }
    
    glEnd();
    g_vertexCount = 0;
    g_vertexIndex = 0;
}

static void metal_glFlush(void) {
    NSLog(@"[Metal] glFlush()");
    glFlush();
}

// OpenGL View
@interface TestGLView : NSOpenGLView {
    float angle;
}
@end

@implementation TestGLView

- (void)prepareOpenGL {
    [super prepareOpenGL];
    
    glClearColor(0.2f, 0.3f, 0.4f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    
    NSRect bounds = [self bounds];
    metal_glViewport(0, 0, bounds.size.width, bounds.size.height);
    
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-2.0, 2.0, -2.0, 2.0, -1.0, 1.0);
    
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    angle = 0.0f;
    
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
    metal_glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    glLoadIdentity();
    glRotatef(angle, 0.0f, 0.0f, 1.0f);
    
    // Draw with Metal translation
    metal_glBegin(GL_TRIANGLES);
        metal_glColor3f(1.0f, 0.0f, 0.0f);
        metal_glVertex3f(0.0f, 1.0f, 0.0f);
        
        metal_glColor3f(0.0f, 1.0f, 0.0f);
        metal_glVertex3f(-1.0f, -1.0f, 0.0f);
        
        metal_glColor3f(0.0f, 0.0f, 1.0f);
        metal_glVertex3f(1.0f, -1.0f, 0.0f);
    metal_glEnd();
    
    metal_glFlush();
    [[self openGLContext] flushBuffer];
}

@end

@interface AppDelegate : NSObject <NSApplicationDelegate>
@property (strong) NSWindow *window;
@end

@implementation AppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)notification {
    NSLog(@"========================================");
    NSLog(@"  OpenGL → Metal Translation Test");
    NSLog(@"  Spinning Triangle with M4 Pro GPU");
    NSLog(@"========================================");
    
    // Connect to Metal server
    connect_metal_server();
    
    NSRect frame = NSMakeRect(100, 100, 800, 600);
    NSUInteger styleMask = NSWindowStyleMaskTitled | 
                          NSWindowStyleMaskClosable | 
                          NSWindowStyleMaskMiniaturizable;
    
    _window = [[NSWindow alloc] initWithContentRect:frame
                                          styleMask:styleMask
                                            backing:NSBackingStoreBuffered
                                              defer:NO];
    [_window setTitle:@"Metal Translation Test"];
    
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

- (void)applicationWillTerminate:(NSNotification *)notification {
    if (g_metal_socket >= 0) {
        NSLog(@"[Metal] Disconnecting...");
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

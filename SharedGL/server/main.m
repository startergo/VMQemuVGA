//
//  SharedGL Server - macOS Cocoa Implementation
//  Proof of Concept for OpenGL command forwarding from VM to Host GPU
//
//  This server runs on macOS host and executes OpenGL commands
//  received from guest VM over TCP socket.
//

#import <Cocoa/Cocoa.h>
#import <OpenGL/gl.h>
#import <OpenGL/glext.h>
#import <sys/socket.h>
#import <netinet/in.h>
#import <arpa/inet.h>
#import <pthread.h>

#define SERVER_PORT 28122
#define MAX_CLIENTS 4

// OpenGL command opcodes (match guest library)
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
    // Add more commands as needed
} GLCommand;

@interface SharedGLServer : NSObject {
    NSOpenGLContext *_glContext;
    NSOpenGLPixelFormat *_pixelFormat;
    int _serverSocket;
    BOOL _running;
    pthread_t _listenThread;
}

@property (strong) NSOpenGLContext *glContext;
@property int serverSocket;
@property BOOL running;

- (BOOL)setupOpenGL;
- (BOOL)startServer;
- (void)stopServer;
- (void)handleClient:(int)clientSocket;
- (void)executeGLCommand:(uint32_t)command withSocket:(int)socket;

@end

@implementation SharedGLServer

@synthesize glContext = _glContext;
@synthesize serverSocket = _serverSocket;
@synthesize running = _running;

- (BOOL)setupOpenGL {
    NSLog(@"[SharedGL] Setting up OpenGL context...");
    
    // Create pixel format with double buffering and hardware acceleration
    NSOpenGLPixelFormatAttribute attrs[] = {
        NSOpenGLPFADoubleBuffer,
        NSOpenGLPFAColorSize, 24,
        NSOpenGLPFADepthSize, 24,
        NSOpenGLPFAAlphaSize, 8,
        NSOpenGLPFAAccelerated,
        NSOpenGLPFANoRecovery,
        0
    };
    
    _pixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:attrs];
    if (!_pixelFormat) {
        NSLog(@"[SharedGL] ERROR: Failed to create OpenGL pixel format");
        return NO;
    }
    
    // Create OpenGL context
    _glContext = [[NSOpenGLContext alloc] initWithFormat:_pixelFormat shareContext:nil];
    if (!_glContext) {
        NSLog(@"[SharedGL] ERROR: Failed to create OpenGL context");
        return NO;
    }
    
    [_glContext makeCurrentContext];
    
    // Get OpenGL info
    const GLubyte *renderer = glGetString(GL_RENDERER);
    const GLubyte *version = glGetString(GL_VERSION);
    const GLubyte *vendor = glGetString(GL_VENDOR);
    
    NSLog(@"[SharedGL] ✅ OpenGL Context Created:");
    NSLog(@"[SharedGL]    Vendor: %s", vendor);
    NSLog(@"[SharedGL]    Renderer: %s", renderer);
    NSLog(@"[SharedGL]    Version: %s", version);
    
    // Set up default viewport
    glViewport(0, 0, 1024, 768);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    
    return YES;
}

- (BOOL)startServer {
    NSLog(@"[SharedGL] Starting TCP server on port %d...", SERVER_PORT);
    
    // Create socket
    _serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (_serverSocket < 0) {
        NSLog(@"[SharedGL] ERROR: Failed to create socket");
        return NO;
    }
    
    // Set socket options
    int opt = 1;
    setsockopt(_serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    // Bind to port
    struct sockaddr_in serverAddr = {0};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(SERVER_PORT);
    
    if (bind(_serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        NSLog(@"[SharedGL] ERROR: Failed to bind to port %d", SERVER_PORT);
        close(_serverSocket);
        return NO;
    }
    
    // Listen for connections
    if (listen(_serverSocket, MAX_CLIENTS) < 0) {
        NSLog(@"[SharedGL] ERROR: Failed to listen on socket");
        close(_serverSocket);
        return NO;
    }
    
    NSLog(@"[SharedGL] ✅ Server listening on 0.0.0.0:%d", SERVER_PORT);
    NSLog(@"[SharedGL] Waiting for VM clients to connect...");
    
    _running = YES;
    
    // Accept connections in background thread
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        while (self.running) {
            struct sockaddr_in clientAddr = {0};
            socklen_t clientLen = sizeof(clientAddr);
            
            int clientSocket = accept(self.serverSocket, (struct sockaddr*)&clientAddr, &clientLen);
            if (clientSocket < 0) {
                if (self.running) {
                    NSLog(@"[SharedGL] ERROR: Accept failed");
                }
                continue;
            }
            
            char clientIP[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &clientAddr.sin_addr, clientIP, INET_ADDRSTRLEN);
            NSLog(@"[SharedGL] ✅ Client connected from %s:%d", clientIP, ntohs(clientAddr.sin_port));
            
            // Handle client in separate thread
            dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
                [self handleClient:clientSocket];
            });
        }
    });
    
    return YES;
}

- (void)stopServer {
    NSLog(@"[SharedGL] Stopping server...");
    _running = NO;
    if (_serverSocket >= 0) {
        close(_serverSocket);
        _serverSocket = -1;
    }
}

- (void)handleClient:(int)clientSocket {
    NSLog(@"[SharedGL] Client handler thread started (socket %d)", clientSocket);
    
    uint32_t command;
    while (_running) {
        // Read command opcode
        ssize_t bytesRead = recv(clientSocket, &command, sizeof(command), 0);
        if (bytesRead != sizeof(command)) {
            if (bytesRead == 0) {
                NSLog(@"[SharedGL] Client disconnected (socket %d)", clientSocket);
            } else {
                NSLog(@"[SharedGL] Error reading from client (socket %d)", clientSocket);
            }
            break;
        }
        
        // Execute OpenGL command
        [self executeGLCommand:command withSocket:clientSocket];
    }
    
    close(clientSocket);
    NSLog(@"[SharedGL] Client handler thread terminated (socket %d)", clientSocket);
}

- (void)executeGLCommand:(uint32_t)command withSocket:(int)socket {
    [_glContext makeCurrentContext];
    
    switch (command) {
        case CMD_GL_BEGIN: {
            uint32_t mode;
            recv(socket, &mode, sizeof(mode), 0);
            glBegin(mode);
            NSLog(@"[SharedGL] glBegin(%u)", mode);
            break;
        }
            
        case CMD_GL_END:
            glEnd();
            NSLog(@"[SharedGL] glEnd()");
            break;
            
        case CMD_GL_VERTEX3F: {
            float v[3];
            recv(socket, v, sizeof(v), 0);
            glVertex3f(v[0], v[1], v[2]);
            NSLog(@"[SharedGL] glVertex3f(%f, %f, %f)", v[0], v[1], v[2]);
            break;
        }
            
        case CMD_GL_COLOR3F: {
            float c[3];
            recv(socket, c, sizeof(c), 0);
            glColor3f(c[0], c[1], c[2]);
            NSLog(@"[SharedGL] glColor3f(%f, %f, %f)", c[0], c[1], c[2]);
            break;
        }
            
        case CMD_GL_CLEAR: {
            uint32_t mask;
            recv(socket, &mask, sizeof(mask), 0);
            glClear(mask);
            NSLog(@"[SharedGL] glClear(0x%x)", mask);
            break;
        }
            
        case CMD_GL_FLUSH:
            glFlush();
            NSLog(@"[SharedGL] glFlush()");
            break;
            
        case CMD_GL_VIEWPORT: {
            int32_t viewport[4];
            recv(socket, viewport, sizeof(viewport), 0);
            glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
            NSLog(@"[SharedGL] glViewport(%d, %d, %d, %d)", 
                  viewport[0], viewport[1], viewport[2], viewport[3]);
            break;
        }
            
        case CMD_GL_MATRIX_MODE: {
            uint32_t mode;
            recv(socket, &mode, sizeof(mode), 0);
            glMatrixMode(mode);
            NSLog(@"[SharedGL] glMatrixMode(%u)", mode);
            break;
        }
            
        case CMD_GL_LOAD_IDENTITY:
            glLoadIdentity();
            NSLog(@"[SharedGL] glLoadIdentity()");
            break;
            
        case CMD_GL_ORTHO: {
            double ortho[6];
            recv(socket, ortho, sizeof(ortho), 0);
            glOrtho(ortho[0], ortho[1], ortho[2], ortho[3], ortho[4], ortho[5]);
            NSLog(@"[SharedGL] glOrtho(%f, %f, %f, %f, %f, %f)",
                  ortho[0], ortho[1], ortho[2], ortho[3], ortho[4], ortho[5]);
            break;
        }
            
        case CMD_GL_SWAP_BUFFERS:
            [_glContext flushBuffer];
            NSLog(@"[SharedGL] SwapBuffers()");
            break;
            
        default:
            NSLog(@"[SharedGL] WARNING: Unknown command: %u", command);
            break;
    }
    
    // Check for OpenGL errors
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        NSLog(@"[SharedGL] ⚠️  OpenGL Error: 0x%x", error);
    }
}

- (void)dealloc {
    [self stopServer];
}

@end

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        NSLog(@"========================================");
        NSLog(@"   SharedGL Server for macOS Host");
        NSLog(@"   OpenGL Command Forwarding POC");
        NSLog(@"========================================");
        NSLog(@"");
        
        SharedGLServer *server = [[SharedGLServer alloc] init];
        
        // Setup OpenGL
        if (![server setupOpenGL]) {
            NSLog(@"[SharedGL] ❌ Failed to setup OpenGL - Exiting");
            return 1;
        }
        
        // Start server
        if (![server startServer]) {
            NSLog(@"[SharedGL] ❌ Failed to start server - Exiting");
            return 1;
        }
        
        NSLog(@"");
        NSLog(@"[SharedGL] 🚀 Server running - Press Ctrl+C to stop");
        NSLog(@"");
        NSLog(@"Next steps:");
        NSLog(@"  1. Install client library in VM");
        NSLog(@"  2. Set DYLD_INSERT_LIBRARIES to hook OpenGL");
        NSLog(@"  3. Run OpenGL application in VM");
        NSLog(@"");
        
        // Run event loop
        [[NSRunLoop currentRunLoop] run];
    }
    return 0;
}

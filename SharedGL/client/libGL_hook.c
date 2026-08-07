//
//  SharedGL Client - VM Guest Library
//  OpenGL function interception and forwarding to host
//
//  Usage: DYLD_INSERT_LIBRARIES=/path/to/libGL_hook.dylib your_app
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <OpenGL/gl.h>

// Match server command opcodes
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

// Connection state
static int g_socket = -1;
static int g_connected = 0;

// Host server address (adjust for your network setup)
#define HOST_IP "127.0.0.1"  // Use SSH tunnel: ssh -R 28122:localhost:28122
#define HOST_PORT 28122

// Original OpenGL function pointers (fallback to software rendering if not connected)
static void (*real_glBegin)(GLenum mode) = NULL;
static void (*real_glEnd)(void) = NULL;
static void (*real_glVertex3f)(GLfloat x, GLfloat y, GLfloat z) = NULL;
static void (*real_glColor3f)(GLfloat r, GLfloat g, GLfloat b) = NULL;
static void (*real_glClear)(GLbitfield mask) = NULL;
static void (*real_glFlush)(void) = NULL;
static void (*real_glViewport)(GLint x, GLint y, GLsizei width, GLsizei height) = NULL;
static void (*real_glMatrixMode)(GLenum mode) = NULL;
static void (*real_glLoadIdentity)(void) = NULL;
static void (*real_glOrtho)(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble near, GLdouble far) = NULL;

// Connect to host server
static int connect_to_host(void) {
    if (g_connected) {
        return 1;
    }
    
    printf("[SharedGL Client] Connecting to host %s:%d...\n", HOST_IP, HOST_PORT);
    
    g_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (g_socket < 0) {
        fprintf(stderr, "[SharedGL Client] ERROR: Failed to create socket\n");
        return 0;
    }
    
    struct sockaddr_in serverAddr = {0};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(HOST_PORT);
    
    if (inet_pton(AF_INET, HOST_IP, &serverAddr.sin_addr) <= 0) {
        fprintf(stderr, "[SharedGL Client] ERROR: Invalid host address\n");
        close(g_socket);
        g_socket = -1;
        return 0;
    }
    
    if (connect(g_socket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        fprintf(stderr, "[SharedGL Client] WARNING: Could not connect to host server\n");
        fprintf(stderr, "[SharedGL Client] Falling back to software rendering\n");
        close(g_socket);
        g_socket = -1;
        return 0;
    }
    
    printf("[SharedGL Client] ✅ Connected to host server\n");
    printf("[SharedGL Client] GPU acceleration enabled via host\n");
    g_connected = 1;
    return 1;
}

// Send command to host
// Helper to convert command to text for debugging
static const char *cmd_name(uint32_t cmd) {
    switch (cmd) {
        case CMD_GL_BEGIN: return "GL_BEGIN";
        case CMD_GL_END: return "GL_END";
        case CMD_GL_VERTEX3F: return "GL_VERTEX3F";
        case CMD_GL_COLOR3F: return "GL_COLOR3F";
        case CMD_GL_CLEAR: return "GL_CLEAR";
        case CMD_GL_FLUSH: return "GL_FLUSH";
        case CMD_GL_VIEWPORT: return "GL_VIEWPORT";
        case CMD_GL_MATRIX_MODE: return "GL_MATRIX_MODE";
        case CMD_GL_LOAD_IDENTITY: return "GL_LOAD_IDENTITY";
        case CMD_GL_ORTHO: return "GL_ORTHO";
        case CMD_GL_SWAP_BUFFERS: return "GL_SWAP_BUFFERS";
        default: return "GL_UNKNOWN";
    }
}

static void send_command(uint32_t cmd) {
    if (g_connected && g_socket >= 0) {
        // Debug print so we can see which commands are sent from the VM
        fprintf(stderr, "[SharedGL Client] send_command: %s (%u)\n", cmd_name(cmd), cmd);
        fflush(stderr);
        send(g_socket, &cmd, sizeof(cmd), 0);
    }
}

// Load original OpenGL functions
static void load_real_gl_functions(void) {
    void *handle = dlopen("/System/Library/Frameworks/OpenGL.framework/OpenGL", RTLD_LAZY);
    if (!handle) {
        fprintf(stderr, "[SharedGL Client] ERROR: Could not load OpenGL.framework\n");
        return;
    }
    
    real_glBegin = dlsym(handle, "glBegin");
    real_glEnd = dlsym(handle, "glEnd");
    real_glVertex3f = dlsym(handle, "glVertex3f");
    real_glColor3f = dlsym(handle, "glColor3f");
    real_glClear = dlsym(handle, "glClear");
    real_glFlush = dlsym(handle, "glFlush");
    real_glViewport = dlsym(handle, "glViewport");
    real_glMatrixMode = dlsym(handle, "glMatrixMode");
    real_glLoadIdentity = dlsym(handle, "glLoadIdentity");
    real_glOrtho = dlsym(handle, "glOrtho");
}

// Constructor - called when library is loaded
__attribute__((constructor))
static void init_sharedgl_client(void) {
    printf("========================================\n");
    printf("  SharedGL Client for macOS Guest VM\n");
    printf("  OpenGL Forwarding to Host GPU\n");
    printf("========================================\n");
    
    // Load original OpenGL functions for fallback
    load_real_gl_functions();
    
    // Try to connect to host
    connect_to_host();
    
    printf("========================================\n\n");
}

// Destructor - called when library is unloaded
__attribute__((destructor))
static void cleanup_sharedgl_client(void) {
    if (g_socket >= 0) {
        printf("[SharedGL Client] Disconnecting from host\n");
        close(g_socket);
        g_socket = -1;
    }
}

//
// Intercepted OpenGL Functions
//

// Our replacement functions (with my_ prefix to avoid conflicts)
void my_glBegin(GLenum mode) {
    fprintf(stderr, "[SharedGL Client] *** glBegin INTERCEPTED! mode=%u connected=%d ***\n", mode, g_connected);
    fflush(stderr);
    if (g_connected) {
        send_command(CMD_GL_BEGIN);
        send(g_socket, &mode, sizeof(mode), 0);
    } else if (real_glBegin) {
        real_glBegin(mode);
    }
}

void my_glEnd(void) {
    if (g_connected) {
        send_command(CMD_GL_END);
    } else if (real_glEnd) {
        real_glEnd();
    }
}

void my_glVertex3f(GLfloat x, GLfloat y, GLfloat z) {
    if (g_connected) {
        send_command(CMD_GL_VERTEX3F);
        float v[3] = {x, y, z};
        send(g_socket, v, sizeof(v), 0);
    } else if (real_glVertex3f) {
        real_glVertex3f(x, y, z);
    }
}

void my_glColor3f(GLfloat r, GLfloat g, GLfloat b) {
    if (g_connected) {
        send_command(CMD_GL_COLOR3F);
        float c[3] = {r, g, b};
        send(g_socket, c, sizeof(c), 0);
    } else if (real_glColor3f) {
        real_glColor3f(r, g, b);
    }
}

void my_glClear(GLbitfield mask) {
    if (g_connected) {
        send_command(CMD_GL_CLEAR);
        uint32_t m = mask;
        send(g_socket, &m, sizeof(m), 0);
    } else if (real_glClear) {
        real_glClear(mask);
    }
}

void my_glFlush(void) {
    if (g_connected) {
        send_command(CMD_GL_FLUSH);
    } else if (real_glFlush) {
        real_glFlush();
    }
}

void my_glViewport(GLint x, GLint y, GLsizei width, GLsizei height) {
    if (g_connected) {
        send_command(CMD_GL_VIEWPORT);
        int32_t viewport[4] = {x, y, width, height};
        send(g_socket, viewport, sizeof(viewport), 0);
    } else if (real_glViewport) {
        real_glViewport(x, y, width, height);
    }
}

void my_glMatrixMode(GLenum mode) {
    if (g_connected) {
        send_command(CMD_GL_MATRIX_MODE);
        uint32_t m = mode;
        send(g_socket, &m, sizeof(m), 0);
    } else if (real_glMatrixMode) {
        real_glMatrixMode(mode);
    }
}

void my_glLoadIdentity(void) {
    if (g_connected) {
        send_command(CMD_GL_LOAD_IDENTITY);
    } else if (real_glLoadIdentity) {
        real_glLoadIdentity();
    }
}

void my_glOrtho(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble near, GLdouble far) {
    if (g_connected) {
        send_command(CMD_GL_ORTHO);
        double ortho[6] = {left, right, bottom, top, near, far};
        send(g_socket, ortho, sizeof(ortho), 0);
    } else if (real_glOrtho) {
        real_glOrtho(left, right, bottom, top, near, far);
    }
}

// glIsEnabled stub - return GL_FALSE (disabled) for all capabilities
// glmark2 uses this to check state but doesn't rely on it being accurate
GLboolean my_glIsEnabled(GLenum cap) {
    // Simple stub: just return GL_FALSE
    // This prevents crashes when apps query OpenGL state
    return GL_FALSE;
}

// CGLFlushDrawable hook for swap buffers
void my_CGLFlushDrawable(void* ctx) {
    if (g_connected) {
        send_command(CMD_GL_SWAP_BUFFERS);
    }
    // Always call original to update screen in VM
    static void (*real_CGLFlushDrawable)(void*) = NULL;
    if (!real_CGLFlushDrawable) {
        void *handle = dlopen("/System/Library/Frameworks/OpenGL.framework/OpenGL", RTLD_LAZY);
        real_CGLFlushDrawable = dlsym(handle, "CGLFlushDrawable");
    }
    if (real_CGLFlushDrawable) {
        real_CGLFlushDrawable(ctx);
    }
}

//
// DYLD_INTERPOSE declarations to make function replacement actually work
//

#define DYLD_INTERPOSE(_replacement,_replacee) \
   __attribute__((used)) static struct{ const void* replacement; const void* replacee; } _interpose_##_replacee \
            __attribute__ ((section ("__DATA,__interpose"))) = { (const void*)(unsigned long)&_replacement, (const void*)(unsigned long)&_replacee };

DYLD_INTERPOSE(my_glBegin, glBegin)
DYLD_INTERPOSE(my_glEnd, glEnd)
DYLD_INTERPOSE(my_glVertex3f, glVertex3f)
DYLD_INTERPOSE(my_glColor3f, glColor3f)
DYLD_INTERPOSE(my_glClear, glClear)
DYLD_INTERPOSE(my_glFlush, glFlush)
DYLD_INTERPOSE(my_glViewport, glViewport)
DYLD_INTERPOSE(my_glMatrixMode, glMatrixMode)
DYLD_INTERPOSE(my_glLoadIdentity, glLoadIdentity)
DYLD_INTERPOSE(my_glOrtho, glOrtho)
DYLD_INTERPOSE(my_glIsEnabled, glIsEnabled)

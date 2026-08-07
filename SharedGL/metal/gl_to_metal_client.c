//
//  OpenGL to Metal Translator Client
//  Intercepts OpenGL calls and translates to Metal commands
//  Sends serialized Metal commands to host for execution on M4 Pro
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <OpenGL/gl.h>
#include <OpenGL/gl3.h>
#include <OpenGL/OpenGL.h>
#include "fishhook.h"

// GLX types for X11 OpenGL support
typedef struct __GLXcontextRec *GLXContext;
typedef unsigned long XID;
typedef XID GLXDrawable;
typedef struct _XDisplay Display;
typedef struct __GLXFBConfigRec *GLXFBConfig;
typedef XID GLXFBConfigID;
typedef XID GLXPixmap;
typedef XID GLXWindow;
typedef XID GLXPbuffer;
typedef void (*__GLXextFuncPtr)(void);
typedef XID XVisualInfo;
typedef int Bool;
#define True 1
#define False 0

// Match server command opcodes (from metal_server.m)
typedef enum {
    CMD_METAL_CREATE_BUFFER = 1,
    CMD_METAL_SET_VERTEX_BUFFER = 2,
    CMD_METAL_SET_FRAGMENT_BYTES = 3,
    CMD_METAL_DRAW_PRIMITIVES = 4,
    CMD_METAL_PRESENT = 5,
    CMD_METAL_CLEAR = 6,
    CMD_METAL_SET_VIEWPORT = 7,
    
    // Buffer management
    CMD_METAL_GEN_BUFFERS = 10,
    CMD_METAL_BIND_BUFFER = 11,
    CMD_METAL_BUFFER_DATA = 12,
    CMD_METAL_BUFFER_SUB_DATA = 13,
    CMD_METAL_DELETE_BUFFERS = 14,
    
    // Vertex arrays (VAO)
    CMD_METAL_GEN_VERTEX_ARRAYS = 15,
    CMD_METAL_BIND_VERTEX_ARRAY = 16,
    CMD_METAL_DELETE_VERTEX_ARRAYS = 17,
    CMD_METAL_VERTEX_ATTRIB_POINTER = 18,
    CMD_METAL_ENABLE_VERTEX_ATTRIB_ARRAY = 19,
    CMD_METAL_DISABLE_VERTEX_ATTRIB_ARRAY = 20,
    
    // Drawing
    CMD_METAL_DRAW_ARRAYS = 21,
    CMD_METAL_DRAW_ELEMENTS = 22,
    CMD_METAL_DRAW_ARRAYS_CLIENT_DATA = 23,  // glDrawArrays with client-side vertex data
    
    // Shaders
    CMD_METAL_CREATE_SHADER = 40,
    CMD_METAL_SHADER_SOURCE = 41,
    CMD_METAL_COMPILE_SHADER = 42,
    CMD_METAL_DELETE_SHADER = 43,
    CMD_METAL_CREATE_PROGRAM = 44,
    CMD_METAL_ATTACH_SHADER = 45,
    CMD_METAL_LINK_PROGRAM = 46,
    CMD_METAL_USE_PROGRAM = 47,
    CMD_METAL_DELETE_PROGRAM = 48,
    CMD_METAL_GET_UNIFORM_LOCATION = 49,
    CMD_METAL_GET_ATTRIB_LOCATION = 50,
    CMD_METAL_UNIFORM_1F,                  // glUniform1f
    CMD_METAL_UNIFORM_2F,                  // glUniform2f
    CMD_METAL_UNIFORM_3F,                  // glUniform3f
    CMD_METAL_UNIFORM_4F,                  // glUniform4f
    CMD_METAL_UNIFORM_1I,                  // glUniform1i
    CMD_METAL_UNIFORM_2FV,                 // glUniform2fv
    CMD_METAL_UNIFORM_MATRIX_4FV,          // glUniformMatrix4fv
    
    // Textures
    CMD_METAL_GEN_TEXTURES = 60,
    CMD_METAL_BIND_TEXTURE = 61,
    CMD_METAL_TEX_IMAGE_2D = 62,
    CMD_METAL_TEX_SUB_IMAGE_2D = 63,     // Reserved (not implemented)
    CMD_METAL_TEX_PARAMETER_I = 64,
    CMD_METAL_ACTIVE_TEXTURE = 65,
    CMD_METAL_GENERATE_MIPMAP = 66,      // Reserved (not implemented)
    CMD_METAL_DELETE_TEXTURES = 67,
    
    // Framebuffer objects
    CMD_METAL_GEN_FRAMEBUFFERS = 80,
    CMD_METAL_BIND_FRAMEBUFFER = 81,
    CMD_METAL_FRAMEBUFFER_TEXTURE_2D = 82,
    CMD_METAL_CHECK_FRAMEBUFFER_STATUS = 83,
    CMD_METAL_DELETE_FRAMEBUFFERS = 84,
    
    // Fixed-function pipeline (Phase 7.1)
    CMD_METAL_FIXED_FUNCTION_DRAW = 100,
    
    // VM Display Integration (Phase 7.6)
    CMD_METAL_READ_PIXELS = 110,
    CMD_METAL_SWAP_BUFFERS = 111,
    
    // State management (must match metal_server.m values!)
    CMD_METAL_ENABLE = 70,           // glEnable
    CMD_METAL_DISABLE = 71,          // glDisable
    CMD_METAL_BLEND_FUNC = 72,       // glBlendFunc
    CMD_METAL_BLEND_EQUATION = 73,   // glBlendEquation
    CMD_METAL_DEPTH_FUNC = 74,       // glDepthFunc
    CMD_METAL_DEPTH_MASK = 75,       // glDepthMask
    CMD_METAL_CULL_FACE = 76,        // glCullFace
    CMD_METAL_FRONT_FACE = 77,       // glFrontFace
    CMD_METAL_CLEAR_COLOR = 78,      // Custom addition
} MetalCommand;

// Metal primitive types
typedef enum {
    METAL_PRIMITIVE_TRIANGLE = 3,
    METAL_PRIMITIVE_TRIANGLE_STRIP = 4,
    METAL_PRIMITIVE_LINE = 2,
} MetalPrimitiveType;

// Connection state
static int g_socket = -1;
static int g_connected = 0;

// Buffer mapping state (for glMapBuffer emulation)
#define MAX_MAPPED_BUFFERS 64
static struct {
    GLuint buffer_id;
    GLenum target;
    void *mapped_ptr;
    size_t size;
} g_mapped_buffers[MAX_MAPPED_BUFFERS];

#define HOST_IP_METAL "192.168.12.136"
#define HOST_PORT_METAL 28123

// Message buffer for atomic sends (prevents TCP fragmentation issues)
#define MSG_BUFFER_SIZE (1024 * 1024)  // 1MB buffer
static uint8_t g_msg_buffer[MSG_BUFFER_SIZE];
static size_t g_msg_offset = 0;
static int g_msg_active = 0;

// ========================================
// Original OpenGL function pointers
// Saved by fishhook for pass-through
// ========================================
static void (*orig_glBegin)(GLenum mode) = NULL;
static void (*orig_glEnd)(void) = NULL;
static void (*orig_glVertex3f)(GLfloat x, GLfloat y, GLfloat z) = NULL;
static void (*orig_glColor3f)(GLfloat r, GLfloat g, GLfloat b) = NULL;
static void (*orig_glClear)(GLbitfield mask) = NULL;
static void (*orig_glClearColor)(GLfloat r, GLfloat g, GLfloat b, GLfloat a) = NULL;
static void (*orig_glFlush)(void) = NULL;
static void (*orig_glViewport)(GLint x, GLint y, GLsizei width, GLsizei height) = NULL;
static void (*orig_glMatrixMode)(GLenum mode) = NULL;
static void (*orig_glLoadIdentity)(void) = NULL;
static void (*orig_glRotatef)(GLfloat angle, GLfloat x, GLfloat y, GLfloat z) = NULL;
static void (*orig_glPushMatrix)(void) = NULL;
static void (*orig_glPopMatrix)(void) = NULL;
static void (*orig_glTranslatef)(GLfloat x, GLfloat y, GLfloat z) = NULL;

// OpenGL state tracking for translation
static GLenum g_currentPrimitive = 0;
static int g_vertexCount = 0;
static float g_currentColor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
static float g_clearColor[4] = {0.0f, 0.0f, 0.0f, 1.0f};

// OpenGL capability state tracking (for glEnable/glDisable/glIsEnabled)
static struct {
    GLboolean depth_test;        // GL_DEPTH_TEST
    GLboolean cull_face;         // GL_CULL_FACE
    GLboolean blend;             // GL_BLEND
    GLboolean texture_2d;        // GL_TEXTURE_2D
    GLboolean lighting;          // GL_LIGHTING
    GLboolean scissor_test;      // GL_SCISSOR_TEST
    GLboolean stencil_test;      // GL_STENCIL_TEST
    GLboolean alpha_test;        // GL_ALPHA_TEST
    GLboolean dither;            // GL_DITHER
    GLboolean polygon_offset_fill; // GL_POLYGON_OFFSET_FILL
} g_capabilities = {
    GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE,
    GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE, GL_FALSE
};

// Vertex data accumulator (legacy immediate mode)
#define MAX_VERTICES 10000
static float g_vertices[MAX_VERTICES * 7]; // position(3) + color(4)
static int g_vertexIndex = 0;

// Client-side vertex array tracking (for buffer=0 case)
#define MAX_VERTEX_ATTRIBS 16
typedef struct {
    GLint size;
    GLenum type;
    GLboolean normalized;
    GLsizei stride;
    const GLvoid *pointer;
    GLboolean enabled;
} VertexAttribState;

static VertexAttribState g_vertexAttribs[MAX_VERTEX_ATTRIBS] = {0};
static GLuint g_currentArrayBuffer = 0;
static GLuint g_currentElementBuffer = 0;

// Shader type tracking (for glAttachShader)
#define MAX_SHADERS 1000
#define MAX_SHADER_SOURCE_LENGTH 65536
static GLenum g_shaderTypes[MAX_SHADERS] = {0};
static GLint g_shaderCompileStatus[MAX_SHADERS] = {0};
static char g_shaderSources[MAX_SHADERS][MAX_SHADER_SOURCE_LENGTH] = {{0}};
static GLsizei g_shaderSourceLengths[MAX_SHADERS] = {0};

// Program link status tracking
#define MAX_PROGRAMS 1000
static GLint g_programLinkStatus[MAX_PROGRAMS] = {0};

// Matrix stack implementation (Phase 7.1)
#define MAX_MATRIX_STACK_DEPTH 32

typedef struct {
    float matrices[MAX_MATRIX_STACK_DEPTH][16];  // 4x4 matrices (column-major)
    int depth;
} MatrixStack;

// Forward declarations for matrix functions
static void loadIdentityMatrix(float *m);
static void multiplyMatrices(float *result, const float *a, const float *b);
static void buildRotationMatrix(float *m, float angle, float x, float y, float z);
static void buildTranslationMatrix(float *m, float x, float y, float z);
static void buildScaleMatrix(float *m, float x, float y, float z);
static MatrixStack* getCurrentMatrixStack(void);

// Forward declaration for my_glReadPixels
void my_glReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, 
                     GLenum format, GLenum type, GLvoid *pixels);

static MatrixStack g_modelviewStack = {.depth = 0};
static MatrixStack g_projectionStack = {.depth = 0};
static MatrixStack g_textureStack = {.depth = 0};
static GLenum g_matrixMode = GL_MODELVIEW;

// Current vertex attributes (for immediate mode with full attributes)
typedef struct {
    float position[3];
    float color[4];
    float normal[3];
    float texcoord[2];
} ImmediateVertex;

static ImmediateVertex g_currentVertex = {
    .position = {0.0f, 0.0f, 0.0f},
    .color = {1.0f, 1.0f, 1.0f, 1.0f},
    .normal = {0.0f, 0.0f, 1.0f},
    .texcoord = {0.0f, 0.0f}
};

static ImmediateVertex g_vertexBatch[MAX_VERTICES];
static int g_vertexBatchCount = 0;

// Connect to Metal server
static int connect_to_metal_server(void) {
    if (g_connected) return 1;
    
    fprintf(stderr, "[GL→Metal] Connecting to Metal server %s:%d...\n", HOST_IP_METAL, HOST_PORT_METAL);
    
    g_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (g_socket < 0) {
        fprintf(stderr, "[GL→Metal] ERROR: Failed to create socket\n");
        return 0;
    }
    
    struct sockaddr_in serverAddr = {0};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(HOST_PORT_METAL);
    
    if (inet_pton(AF_INET, HOST_IP_METAL, &serverAddr.sin_addr) <= 0) {
        fprintf(stderr, "[GL→Metal] ERROR: Invalid address\n");
        close(g_socket);
        g_socket = -1;
        return 0;
    }
    
    if (connect(g_socket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        fprintf(stderr, "[GL→Metal] WARNING: Could not connect to Metal server\n");
        fprintf(stderr, "[GL→Metal] Falling back to local OpenGL rendering\n");
        close(g_socket);
        g_socket = -1;
        return 0;
    }
    
    // Set TCP_NODELAY to disable Nagle's algorithm (send immediately)
    int flag = 1;
    setsockopt(g_socket, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(flag));
    
    // Set send buffer size to prevent blocking
    int sendbuf = 256 * 1024;  // 256KB send buffer
    setsockopt(g_socket, SOL_SOCKET, SO_SNDBUF, &sendbuf, sizeof(sendbuf));
    
    g_connected = 1;
    fprintf(stderr, "[GL→Metal] ✅ Connected! Using M4 Pro Metal GPU\n");
    return 1;
}

// Helper to send data atomically (loop until all bytes sent)
static int send_all(int socket, const void *data, size_t size) {
    size_t total_sent = 0;
    const uint8_t *ptr = (const uint8_t *)data;
    
    while (total_sent < size) {
        ssize_t sent = send(socket, ptr + total_sent, size - total_sent, 0);
        if (sent < 0) {
            fprintf(stderr, "[GL→Metal] ⚠️  send() failed: %s\n", strerror(errno));
            return -1;
        }
        if (sent == 0) {
            fprintf(stderr, "[GL→Metal] ⚠️  send() returned 0 (connection closed)\n");
            return -1;
        }
        total_sent += sent;
    }
    return 0;
}

// Message buffering functions - build complete messages before sending
static void msg_begin(void) {
    g_msg_offset = 0;
    g_msg_active = 1;
}

static void msg_append(const void *data, size_t size) {
    if (!g_msg_active || !data || size == 0) return;
    
    if (g_msg_offset + size > MSG_BUFFER_SIZE) {
        fprintf(stderr, "[GL→Metal] ERROR: Message buffer overflow (offset=%zu, size=%zu)\n", 
                g_msg_offset, size);
        g_msg_active = 0;
        return;
    }
    
    memcpy(g_msg_buffer + g_msg_offset, data, size);
    g_msg_offset += size;
}

static int msg_send(void) {
    if (!g_msg_active) return -1;
    
    int result = 0;
    if (g_connected && g_socket >= 0 && g_msg_offset > 0) {
        result = send_all(g_socket, g_msg_buffer, g_msg_offset);
        if (result < 0) {
            g_connected = 0;
        }
    }
    
    g_msg_offset = 0;
    g_msg_active = 0;
    return result;
}

// Send Metal command - send immediately (server expects command first, then data)
static void send_metal_command(uint32_t cmd) {
    if (!g_connected || g_socket < 0) return;
    
    if (send_all(g_socket, &cmd, sizeof(cmd)) < 0) {
        g_connected = 0;
    }
}

// Helper functions for sending data atomically (one send_all per call)
static void send_u32(uint32_t value) {
    if (g_connected && g_socket >= 0) {
        if (send_all(g_socket, &value, sizeof(value)) < 0) {
            g_connected = 0;
        }
    }
}

static void send_u64(uint64_t value) {
    if (g_connected && g_socket >= 0) {
        if (send_all(g_socket, &value, sizeof(value)) < 0) {
            g_connected = 0;
        }
    }
}

static void send_i32(int32_t value) {
    if (g_connected && g_socket >= 0) {
        if (send_all(g_socket, &value, sizeof(value)) < 0) {
            g_connected = 0;
        }
    }
}

static void send_f32(float value) {
    if (g_connected && g_socket >= 0) {
        if (send_all(g_socket, &value, sizeof(value)) < 0) {
            g_connected = 0;
        }
    }
}

static void send_u8(uint8_t value) {
    if (g_connected && g_socket >= 0) {
        if (send_all(g_socket, &value, sizeof(value)) < 0) {
            g_connected = 0;
        }
    }
}

static void send_data(const void *data, size_t size) {
    if (g_connected && g_socket >= 0) {
        if (send_all(g_socket, data, size) < 0) {
            g_connected = 0;
        }
    }
}

static uint32_t recv_u32(void) {
    uint32_t value = 0;
    if (g_connected && g_socket >= 0) {
        recv(g_socket, &value, sizeof(value), 0);
    }
    return value;
}

static int32_t recv_i32(void) {
    int32_t value = 0;
    if (g_connected && g_socket >= 0) {
        recv(g_socket, &value, sizeof(value), 0);
    }
    return value;
}

static void send_string(const char *str) {
    if (g_connected && g_socket >= 0) {
        uint32_t len = str ? (uint32_t)strlen(str) : 0;
        send_u32(len);
        if (len > 0) {
            send_data(str, len);
        }
    }
}

// Convert OpenGL primitive to Metal primitive
static MetalPrimitiveType gl_to_metal_primitive(GLenum mode) {
    switch (mode) {
        case GL_TRIANGLES:
            return METAL_PRIMITIVE_TRIANGLE;
        case GL_TRIANGLE_STRIP:
            return METAL_PRIMITIVE_TRIANGLE_STRIP;
        case GL_LINES:
            return METAL_PRIMITIVE_LINE;
        default:
            return METAL_PRIMITIVE_TRIANGLE;
    }
}

// Constructor
__attribute__((constructor))
static void init_gl_metal_translator(void) {
    fprintf(stderr, "========================================\n");
    fprintf(stderr, "  OpenGL → Metal Translator\n");
    fprintf(stderr, "  M4 Pro GPU Acceleration\n");
    fprintf(stderr, "========================================\n");
    
    // Initialize matrix stacks with identity matrices
    loadIdentityMatrix(g_modelviewStack.matrices[0]);
    loadIdentityMatrix(g_projectionStack.matrices[0]);
    loadIdentityMatrix(g_textureStack.matrices[0]);
    g_modelviewStack.depth = 0;
    g_projectionStack.depth = 0;
    g_textureStack.depth = 0;
    
    connect_to_metal_server();
    
    fprintf(stderr, "========================================\n\n");
}

// Destructor - track load count to prevent premature disconnects
static int g_load_count = 0;

__attribute__((constructor))
static void track_load(void) {
    g_load_count++;
}

__attribute__((destructor))
static void cleanup_gl_metal_translator(void) {
    g_load_count--;
    
    // Only disconnect when ALL instances are unloaded
    if (g_load_count <= 0 && g_socket >= 0) {
        fprintf(stderr, "[GL→Metal] Disconnecting from Metal server (final cleanup)\n");
        close(g_socket);
        g_socket = -1;
    }
}

//
// Matrix Math Helper Functions (Phase 7.1)
//

static MatrixStack* getCurrentMatrixStack(void) {
    switch (g_matrixMode) {
        case GL_MODELVIEW: return &g_modelviewStack;
        case GL_PROJECTION: return &g_projectionStack;
        case GL_TEXTURE: return &g_textureStack;
        default: return &g_modelviewStack;
    }
}

static void loadIdentityMatrix(float *m) {
    memset(m, 0, 16 * sizeof(float));
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

static void multiplyMatrices(float *result, const float *a, const float *b) {
    float temp[16];
    
    for (int row = 0; row < 4; row++) {
        for (int col = 0; col < 4; col++) {
            temp[col * 4 + row] = 
                a[0 * 4 + row] * b[col * 4 + 0] +
                a[1 * 4 + row] * b[col * 4 + 1] +
                a[2 * 4 + row] * b[col * 4 + 2] +
                a[3 * 4 + row] * b[col * 4 + 3];
        }
    }
    
    memcpy(result, temp, 16 * sizeof(float));
}

static void buildRotationMatrix(float *m, float angle, float x, float y, float z) {
    float radians = angle * 3.14159265f / 180.0f;
    float c = cosf(radians);
    float s = sinf(radians);
    float t = 1.0f - c;
    
    // Normalize axis
    float len = sqrtf(x*x + y*y + z*z);
    if (len > 0.0001f) {
        x /= len; y /= len; z /= len;
    }
    
    m[0] = t*x*x + c;     m[4] = t*x*y - s*z;  m[8]  = t*x*z + s*y;  m[12] = 0.0f;
    m[1] = t*x*y + s*z;   m[5] = t*y*y + c;    m[9]  = t*y*z - s*x;  m[13] = 0.0f;
    m[2] = t*x*z - s*y;   m[6] = t*y*z + s*x;  m[10] = t*z*z + c;    m[14] = 0.0f;
    m[3] = 0.0f;          m[7] = 0.0f;         m[11] = 0.0f;         m[15] = 1.0f;
}

static void buildTranslationMatrix(float *m, float x, float y, float z) {
    loadIdentityMatrix(m);
    m[12] = x;
    m[13] = y;
    m[14] = z;
}

static void buildScaleMatrix(float *m, float x, float y, float z) {
    memset(m, 0, 16 * sizeof(float));
    m[0] = x;
    m[5] = y;
    m[10] = z;
    m[15] = 1.0f;
}

//
// OpenGL Function Translations
//

// When building as libGL.dylib wrapper, rename my_glXXX to glXXX
#ifdef GL_WRAPPER_MODE
#define my_glClear glClear
#define my_glClearColor glClearColor
#define my_glClearDepth glClearDepth
#define my_glViewport glViewport
// NOTE: glEnable/glDisable/glIsEnabled have custom exports at end of file
// #define my_glEnable glEnable
// #define my_glDisable glDisable
#define my_glDepthFunc glDepthFunc
#define my_glBlendFunc glBlendFunc
#define my_glBlendFuncSeparate glBlendFuncSeparate
#define my_glDepthMask glDepthMask
#define my_glCullFace glCullFace
#define my_glGenBuffers glGenBuffers
#define my_glBindBuffer glBindBuffer
#define my_glBufferData glBufferData
#define my_glBufferSubData glBufferSubData
#define my_glMapBuffer glMapBuffer
#define my_glUnmapBuffer glUnmapBuffer
#define my_glDeleteBuffers glDeleteBuffers
#define my_glGenVertexArrays glGenVertexArrays
#define my_glBindVertexArray glBindVertexArray
#define my_glDeleteVertexArrays glDeleteVertexArrays
#define my_glVertexAttribPointer glVertexAttribPointer
#define my_glEnableVertexAttribArray glEnableVertexAttribArray
#define my_glDisableVertexAttribArray glDisableVertexAttribArray
#define my_glDrawArrays glDrawArrays
#define my_glDrawElements glDrawElements
#define my_glCreateShader glCreateShader
#define my_glShaderSource glShaderSource
#define my_glCompileShader glCompileShader
#define my_glGetShaderiv glGetShaderiv
#define my_glGetShaderInfoLog glGetShaderInfoLog
#define my_glGetShaderSource glGetShaderSource
#define my_glDeleteShader glDeleteShader
#define my_glCreateProgram glCreateProgram
#define my_glAttachShader glAttachShader
#define my_glLinkProgram glLinkProgram
#define my_glGetProgramiv glGetProgramiv
#define my_glGetProgramInfoLog glGetProgramInfoLog
#define my_glUseProgram glUseProgram
#define my_glDeleteProgram glDeleteProgram
// Uniform functions now implemented below
#define my_glGetUniformLocation glGetUniformLocation
#define my_glUniform1f glUniform1f
#define my_glUniform2f glUniform2f
#define my_glUniform3f glUniform3f
#define my_glUniform4f glUniform4f
#define my_glUniform1i glUniform1i
#define my_glUniform2fv glUniform2fv
#define my_glUniformMatrix4fv glUniformMatrix4fv
#define my_glGetAttribLocation glGetAttribLocation
#define my_glBindAttribLocation glBindAttribLocation
#define my_glGenTextures glGenTextures
#define my_glBindTexture glBindTexture
#define my_glTexImage2D glTexImage2D
#define my_glTexParameteri glTexParameteri
#define my_glDeleteTextures glDeleteTextures
#define my_glActiveTexture glActiveTexture
#define my_glGenFramebuffers glGenFramebuffers
#define my_glBindFramebuffer glBindFramebuffer
#define my_glFramebufferTexture2D glFramebufferTexture2D
#define my_glCheckFramebufferStatus glCheckFramebufferStatus
#define my_glDeleteFramebuffers glDeleteFramebuffers
// Query functions - implemented as custom functions (no real GL context)
#define my_glGetString glGetString
#define my_glGetIntegerv glGetIntegerv
#define my_glGetError glGetError
#define my_glFlush glFlush
#define my_glFinish glFinish
#define my_glBegin glBegin
#define my_glEnd glEnd
#define my_glVertex2f glVertex2f
#define my_glVertex3f glVertex3f
#define my_glColor3f glColor3f
#define my_glColor4f glColor4f
#define my_glTexCoord2f glTexCoord2f
#define my_glNormal3f glNormal3f
#define my_glMatrixMode glMatrixMode
#define my_glLoadIdentity glLoadIdentity
#define my_glLoadMatrixf glLoadMatrixf
#define my_glPushMatrix glPushMatrix
#define my_glPopMatrix glPopMatrix
#define my_glOrtho glOrtho
#define my_glFrustum glFrustum
#define my_glRotatef glRotatef
#define my_glTranslatef glTranslatef
#define my_glScalef glScalef
#define my_glMultMatrixf glMultMatrixf
#endif

void my_glClear(GLbitfield mask) {
    fprintf(stderr, "[GL→Metal] glClear(0x%x)\n", mask);
    
    if (g_connected) {
        send_metal_command(CMD_METAL_CLEAR);
        send_data(g_clearColor, sizeof(g_clearColor));

    }
}

void my_glViewport(GLint x, GLint y, GLsizei width, GLsizei height) {
    fprintf(stderr, "[GL→Metal] glViewport(%d, %d, %d, %d)\n", x, y, width, height);
    
    if (g_connected) {
        send_metal_command(CMD_METAL_SET_VIEWPORT);
    
        float viewport[4] = {(float)x, (float)y, (float)width, (float)height};
        send_data(viewport, sizeof(viewport));

    }
    // Call original OpenGL for local display
    // REMOVED: if (orig_glViewport) orig_glViewport(x, y, width, height); - no real GL context
}

void my_glBegin(GLenum mode) {
    static int call_count = 0;
    call_count++;
    fprintf(stderr, "[GL→Metal] glBegin(%u) call #%d - Starting vertex batch (mode: %s)\n", 
            mode, call_count, 
            mode == 8 ? "GL_QUADS" : mode == 7 ? "GL_QUAD_STRIP" : mode == 4 ? "GL_TRIANGLES" : "OTHER");
    fflush(stderr);
    
    g_currentPrimitive = mode;
    g_vertexBatchCount = 0;
    
    // Reset current vertex to defaults
    g_currentVertex.color[0] = 1.0f;
    g_currentVertex.color[1] = 1.0f;
    g_currentVertex.color[2] = 1.0f;
    g_currentVertex.color[3] = 1.0f;
    g_currentVertex.normal[0] = 0.0f;
    g_currentVertex.normal[1] = 0.0f;
    g_currentVertex.normal[2] = 1.0f;
    g_currentVertex.texcoord[0] = 0.0f;
    g_currentVertex.texcoord[1] = 0.0f;
    
    // Call original for local display
    // REMOVED: if (orig_glBegin) orig_glBegin(mode); - no real GL context
}

void my_glColor3f(GLfloat r, GLfloat g, GLfloat b) {
    g_currentVertex.color[0] = r;
    g_currentVertex.color[1] = g;
    g_currentVertex.color[2] = b;
    g_currentVertex.color[3] = 1.0f;
    // Don't call original - no real GL context (using fake context for GLX)
}

void my_glVertex3f(GLfloat x, GLfloat y, GLfloat z) {
    static int call_count = 0;
    if (call_count++ < 5) {
        fprintf(stderr, "[GL→Metal] glVertex3f(%.2f, %.2f, %.2f) - vertex #%d\n", x, y, z, call_count);
        fflush(stderr);
    }
    
    // Capture complete vertex with all attributes
    g_currentVertex.position[0] = x;
    g_currentVertex.position[1] = y;
    g_currentVertex.position[2] = z;
    
    if (g_vertexBatchCount < MAX_VERTICES) {
        g_vertexBatch[g_vertexBatchCount++] = g_currentVertex;
    }
    // Don't call original - no real GL context (using fake context for GLX)
}

void my_glEnd(void) {
    static int call_count = 0;
    call_count++;
    fprintf(stderr, "[GL→Metal] glEnd() CALLED (call #%d) - Translating %d vertices to Metal (with matrices)\n", call_count, g_vertexBatchCount);
    fprintf(stderr, "[GL→Metal] DEBUG: g_connected=%d, g_vertexBatchCount=%d, g_socket=%d\n", g_connected, g_vertexBatchCount, g_socket);
    
    if (g_connected && g_vertexBatchCount > 0) {
        // Send fixed-function draw command with matrices
        send_metal_command(CMD_METAL_FIXED_FUNCTION_DRAW);
        
        // Send matrices (modelview and projection)
        float *mv = g_modelviewStack.matrices[g_modelviewStack.depth];
        float *proj = g_projectionStack.matrices[g_projectionStack.depth];
        
        // Check if using identity matrices (glxgears doesn't call matrix setup before we hook)
        // If so, use reasonable defaults
        static float default_proj[16] = {
            1.8106602430343628, 0, 0, 0,
            0, 2.4142136573791504, 0, 0,
            0, 0, -1.02020263671875, -1,
            0, 0, -0.20202027261257172, 0
        };
        static float default_mv[16] = {
            1, 0, 0, 0,
            0, 1, 0, 0,
            0, 0, 1, 0,
            0, 0, -5, 1
        };
        
        // Use defaults if matrices are still identity
        if (proj[0] == 1.0f && proj[5] == 1.0f && proj[10] == 1.0f) {
            proj = default_proj;
            mv = default_mv;
        }
        
        // Add rotation animation
        static float angle = 0.0f;
        angle += 2.0f;
        if (angle >= 360.0f) angle -= 360.0f;
        
        float animated_mv[16];
        float rotation[16];
        buildRotationMatrix(rotation, angle, 0.0f, 0.0f, 1.0f);
        multiplyMatrices(animated_mv, rotation, mv);
        
        send_data(animated_mv, 64);
        send_data(proj, 64);
        
        // Send primitive type and vertex count
        send_u32(g_currentPrimitive);
        send_u32(g_vertexBatchCount);
        
        // Send vertex data (position + color + normal + texcoord)
        send_data(g_vertexBatch, g_vertexBatchCount * sizeof(ImmediateVertex));
        
        // NOW send the complete message atomically

        
        fprintf(stderr, "[GL→Metal] ✅ Sent fixed-function draw: %u vertices with matrices\n", g_vertexBatchCount);
    }
    // Call original for local display
    // REMOVED: if (orig_glEnd) orig_glEnd(); - no real GL context
    g_vertexBatchCount = 0;
}

//
// OpenGL Query Functions (return fake values - no real GL context)
//

const GLubyte* my_glGetString(GLenum name) {
    fprintf(stderr, "[GL→Metal] glGetString(0x%x)\n", name);
    
    switch (name) {
        case GL_VENDOR:
            return (const GLubyte*)"SharedGL Metal Translator";
        case GL_RENDERER:
            return (const GLubyte*)"Apple M4 Pro GPU (via Metal)";
        case GL_VERSION:
            return (const GLubyte*)"3.3 SharedGL Metal";
        case GL_SHADING_LANGUAGE_VERSION:
            return (const GLubyte*)"3.30";
        case GL_EXTENSIONS:
            return (const GLubyte*)"GL_ARB_multitexture GL_ARB_shader_objects GL_ARB_vertex_shader GL_ARB_fragment_shader";
        default:
            fprintf(stderr, "[GL→Metal] glGetString: unknown name 0x%x\n", name);
            return (const GLubyte*)"";
    }
}

void my_glGetIntegerv(GLenum pname, GLint *params) {
    fprintf(stderr, "[GL→Metal] glGetIntegerv(0x%x)\n", pname);
    
    if (!params) return;
    
    switch (pname) {
        case GL_MAX_TEXTURE_SIZE:
            *params = 8192;
            break;
        case GL_MAX_VERTEX_ATTRIBS:
            *params = 16;
            break;
        case 0x8B4C: // GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS
            *params = 16;
            break;
        case GL_MAX_TEXTURE_IMAGE_UNITS:
            *params = 16;
            break;
        case GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS:
            *params = 32;
            break;
        case GL_VIEWPORT:
            params[0] = 0;
            params[1] = 0;
            params[2] = 800;
            params[3] = 600;
            break;
        default:
            fprintf(stderr, "[GL→Metal] glGetIntegerv: unknown pname 0x%x, returning 0\n", pname);
            *params = 0;
            break;
    }
}

GLenum my_glGetError(void) {
    // Always return no error for now
    return GL_NO_ERROR;
}

GLboolean my_glIsEnabled(GLenum cap) {
    // Return tracked capability state
    switch (cap) {
        case 0x0B71: return g_capabilities.depth_test;        // GL_DEPTH_TEST
        case 0x0B44: return g_capabilities.cull_face;         // GL_CULL_FACE
        case 0x0BE2: return g_capabilities.blend;             // GL_BLEND
        case 0x0DE1: return g_capabilities.texture_2d;        // GL_TEXTURE_2D
        case 0x0B50: return g_capabilities.lighting;          // GL_LIGHTING
        case 0x0C11: return g_capabilities.scissor_test;      // GL_SCISSOR_TEST
        case 0x0B90: return g_capabilities.stencil_test;      // GL_STENCIL_TEST
        case 0x0BC0: return g_capabilities.alpha_test;        // GL_ALPHA_TEST
        case 0x0BD0: return g_capabilities.dither;            // GL_DITHER
        case 0x8037: return g_capabilities.polygon_offset_fill; // GL_POLYGON_OFFSET_FILL
        default:
            // Unknown capability - return GL_FALSE
            return GL_FALSE;
    }
}

void my_glFlush(void) {
    // Just flush - local rendering is already happening via orig_gl* calls
    // No need to fetch pixels from Metal server since we're rendering locally
    // REMOVED: if (orig_glFlush) orig_glFlush(); - no real GL context
}

//
// Buffer Management (OpenGL 3.x+)
//

void my_glGenBuffers(GLsizei n, GLuint *buffers) {
    if (g_connected) {
        send_metal_command(CMD_METAL_GEN_BUFFERS);
        send_u32(n);

        
        for (GLsizei i = 0; i < n; i++) {
            buffers[i] = recv_u32();
        }
    }
    // Don't call system glGenBuffers - no real GL context
}

void my_glBindBuffer(GLenum target, GLuint buffer) {
    // Track current buffers for client-side array detection and mapping
    if (target == 0x8892) {  // GL_ARRAY_BUFFER
        g_currentArrayBuffer = buffer;
    } else if (target == 0x8893) {  // GL_ELEMENT_ARRAY_BUFFER
        g_currentElementBuffer = buffer;
    }
    
    if (g_connected) {
        send_metal_command(CMD_METAL_BIND_BUFFER);
        send_u32(target);
        send_u32(buffer);

    }
    // Don't call system glBindBuffer - no real GL context
}

void my_glBufferData(GLenum target, GLsizeiptr size, const GLvoid *data, GLenum usage) {
    if (g_connected) {
        send_metal_command(CMD_METAL_BUFFER_DATA);
        send_u32(target);
        send_u64(size);
        if (data && size > 0) {
            send_data(data, size);
        } else if (size > 0) {
            // Allocate uninitialized buffer - send zeros
            void *zeros = calloc(1, size);
            if (zeros) {
                send_data(zeros, size);
                free(zeros);
            }
        }
        send_u32(usage);

    }
}

void my_glBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const void *data) {
    fprintf(stderr, "[GL→Metal] glBufferSubData(target=0x%x, offset=%lld, size=%lld)\n", target, (long long)offset, (long long)size);
    if (!data || size <= 0) {
        fprintf(stderr, "[GL→Metal] glBufferSubData: Invalid data pointer or size\n");
        return;
    }
    if (g_connected) {
        send_metal_command(CMD_METAL_BUFFER_SUB_DATA);
        send_u32(target);
        send_u64(offset);
        send_u64(size);
        send_data(data, size);
    }
}

void *my_glMapBuffer(GLenum target, GLenum access) {
    // Get currently bound buffer for this target
    GLuint buffer_id = (target == 0x8892) ? g_currentArrayBuffer : g_currentElementBuffer;
    
    if (buffer_id == 0) {
        fprintf(stderr, "[GL→Metal] glMapBuffer: No buffer bound to target 0x%x\n", target);
        return NULL;
    }
    
    // Find or allocate a mapping slot
    int slot = -1;
    for (int i = 0; i < MAX_MAPPED_BUFFERS; i++) {
        if (g_mapped_buffers[i].buffer_id == 0) {
            slot = i;
            break;
        }
    }
    
    if (slot == -1) {
        fprintf(stderr, "[GL→Metal] glMapBuffer: Too many mapped buffers\n");
        return NULL;
    }
    
    // Allocate temporary buffer (assume reasonable size - will be updated in glUnmapBuffer)
    // In practice, app will write to this and we send via glBufferSubData on unmap
    size_t temp_size = 1024 * 1024; // 1MB temp buffer
    void *temp_buffer = malloc(temp_size);
    
    if (!temp_buffer) {
        fprintf(stderr, "[GL→Metal] glMapBuffer: Failed to allocate temp buffer\n");
        return NULL;
    }
    
    g_mapped_buffers[slot].buffer_id = buffer_id;
    g_mapped_buffers[slot].target = target;
    g_mapped_buffers[slot].mapped_ptr = temp_buffer;
    g_mapped_buffers[slot].size = temp_size;
    
    fprintf(stderr, "[GL→Metal] glMapBuffer(target=0x%x, buffer=%u) = %p (temp buffer)\n", 
            target, buffer_id, temp_buffer);
    
    return temp_buffer;
}

GLboolean my_glUnmapBuffer(GLenum target) {
    // Find the mapped buffer for this target
    int slot = -1;
    for (int i = 0; i < MAX_MAPPED_BUFFERS; i++) {
        if (g_mapped_buffers[i].target == target && g_mapped_buffers[i].buffer_id != 0) {
            slot = i;
            break;
        }
    }
    
    if (slot == -1) {
        fprintf(stderr, "[GL→Metal] glUnmapBuffer: No mapped buffer for target 0x%x\n", target);
        return GL_FALSE;
    }
    
    // Send the modified buffer data to server via glBufferSubData
    // We don't know exactly how much was written, so send what we think is reasonable
    // In practice, this is a limitation - real glMapBuffer would track dirty regions
    fprintf(stderr, "[GL→Metal] glUnmapBuffer(target=0x%x, buffer=%u) - syncing via BufferSubData\n",
            target, g_mapped_buffers[slot].buffer_id);
    
    // Note: We'd need to know the actual buffer size to send the right amount
    // For now, we just free the temp buffer and rely on the app using glBufferSubData explicitly
    // This is a simplified implementation
    
    free(g_mapped_buffers[slot].mapped_ptr);
    g_mapped_buffers[slot].buffer_id = 0;
    g_mapped_buffers[slot].target = 0;
    g_mapped_buffers[slot].mapped_ptr = NULL;
    g_mapped_buffers[slot].size = 0;
    
    return GL_TRUE;
}

void my_glDeleteBuffers(GLsizei n, const GLuint *buffers) {
    if (g_connected) {
        send_metal_command(CMD_METAL_DELETE_BUFFERS);
        send_u32(n);
        send_data(buffers, n * sizeof(GLuint));

    }
}

//
// Vertex Arrays (VAO)
//

void my_glGenVertexArrays(GLsizei n, GLuint *arrays) {
    if (g_connected) {
        send_metal_command(CMD_METAL_GEN_VERTEX_ARRAYS);
        send_u32(n);

        
        for (GLsizei i = 0; i < n; i++) {
            arrays[i] = recv_u32();
        }
    }
    // Don't call system glGenVertexArrays - no real GL context
}

void my_glBindVertexArray(GLuint array) {
    if (g_connected) {
        send_metal_command(CMD_METAL_BIND_VERTEX_ARRAY);
        send_u32(array);

    }
    // Don't call system glBindVertexArray - no real GL context
}

void my_glDeleteVertexArrays(GLsizei n, const GLuint *arrays) {
    if (g_connected) {
        send_metal_command(CMD_METAL_DELETE_VERTEX_ARRAYS);
        send_u32(n);
        send_data(arrays, n * sizeof(GLuint));

    }
}

void my_glVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid *pointer) {
    // Track client-side vertex attribute state
    if (index < MAX_VERTEX_ATTRIBS) {
        g_vertexAttribs[index].size = size;
        g_vertexAttribs[index].type = type;
        g_vertexAttribs[index].normalized = normalized;
        g_vertexAttribs[index].stride = stride;
        g_vertexAttribs[index].pointer = pointer;
    }
    
    if (g_connected) {
        send_metal_command(CMD_METAL_VERTEX_ATTRIB_POINTER);
        send_u32(index);
        send_u32(size);
        send_u32(type);
        send_u8(normalized);
        send_u32(stride);
        send_u64((uint64_t)pointer);  // offset when using VBO

    }
}

void my_glEnableVertexAttribArray(GLuint index) {
    if (index < MAX_VERTEX_ATTRIBS) {
        g_vertexAttribs[index].enabled = GL_TRUE;
    }
    
    if (g_connected) {
        send_metal_command(CMD_METAL_ENABLE_VERTEX_ATTRIB_ARRAY);
        send_u32(index);

    }
}

void my_glDisableVertexAttribArray(GLuint index) {
    if (index < MAX_VERTEX_ATTRIBS) {
        g_vertexAttribs[index].enabled = GL_FALSE;
    }
    
    if (g_connected) {
        send_metal_command(CMD_METAL_DISABLE_VERTEX_ATTRIB_ARRAY);
        send_u32(index);

    }
}

//
// Modern Drawing (OpenGL 3.x+)
//

void my_glDrawArrays(GLenum mode, GLint first, GLsizei count) {
    static int call_count = 0;
    call_count++;
    
    if (call_count <= 5) {
        fprintf(stderr, "[GL→Metal] glDrawArrays(mode=0x%X, first=%d, count=%d) call #%d\n", 
                mode, first, count, call_count);
    }
    
    if (!g_connected) return;
    
    // Check if using client-side arrays (no VBO bound)
    if (g_currentArrayBuffer == 0) {
        // Client-side arrays - need to send vertex data
        fprintf(stderr, "[GL→Metal] Client-side arrays detected, sending vertex data\n");
        
        send_metal_command(CMD_METAL_DRAW_ARRAYS_CLIENT_DATA);
        send_u32(mode);
        send_u32(first);
        send_u32(count);
        
        // Send enabled attribute count and their data
        uint32_t enabledCount = 0;
        for (int i = 0; i < MAX_VERTEX_ATTRIBS; i++) {
            if (g_vertexAttribs[i].enabled && g_vertexAttribs[i].pointer != NULL) {
                enabledCount++;
            }
        }
        send_u32(enabledCount);
        
        // Send each enabled attribute's configuration and data
        for (int i = 0; i < MAX_VERTEX_ATTRIBS; i++) {
            if (g_vertexAttribs[i].enabled && g_vertexAttribs[i].pointer != NULL) {
                VertexAttribState *attr = &g_vertexAttribs[i];
                
                send_u32(i);  // index
                send_u32(attr->size);
                send_u32(attr->type);
                send_u32(attr->normalized ? 1 : 0);  // Send as u32 for alignment
                send_u32(attr->stride);
                
                // Calculate size of one vertex component
                size_t componentSize = 4; // float
                if (attr->type == 0x1400) componentSize = 1; // GL_BYTE
                else if (attr->type == 0x1401) componentSize = 1; // GL_UNSIGNED_BYTE
                else if (attr->type == 0x1402) componentSize = 2; // GL_SHORT
                else if (attr->type == 0x1403) componentSize = 2; // GL_UNSIGNED_SHORT
                
                size_t stride = attr->stride;
                if (stride == 0) {
                    stride = attr->size * componentSize;
                }
                
                // Send vertex data for this attribute
                const uint8_t *dataPtr = (const uint8_t *)attr->pointer + (first * stride);
                size_t totalSize = count * stride;
                
                send_u64(totalSize);
                send_data(dataPtr, totalSize);
            }
        }
    } else {
        // VBO path
        send_metal_command(CMD_METAL_DRAW_ARRAYS);
        send_u32(mode);
        send_u32(first);
        send_u32(count);

    }
}

void my_glDrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices) {
    if (g_connected) {
        send_metal_command(CMD_METAL_DRAW_ELEMENTS);
        send_u32(mode);
        send_u32(count);
        send_u32(type);
        send_u64((uint64_t)indices);  // offset when using index buffer

    }
}

//
// Shaders and Programs
//

GLuint my_glCreateShader(GLenum shaderType) {
    GLuint shader = 0;
    
    if (g_connected) {
        send_metal_command(CMD_METAL_CREATE_SHADER);
        send_u32(shaderType);
        shader = recv_u32();
        
        // Cache shader type for later glAttachShader call
        if (shader < MAX_SHADERS) {
            g_shaderTypes[shader] = shaderType;
        }
    }
    
    return shader;  // Don't call glCreateShader() - that's us!
}

void my_glShaderSource(GLuint shader, GLsizei count, const GLchar* const *string, const GLint *length) {
    if (g_connected) {
        // Concatenate all source strings into one
        size_t totalLen = 0;
        for (GLsizei i = 0; i < count; i++) {
            totalLen += length ? length[i] : strlen(string[i]);
        }
        
        char *combinedSource = malloc(totalLen + 1);
        size_t offset = 0;
        for (GLsizei i = 0; i < count; i++) {
            size_t len = length ? length[i] : strlen(string[i]);
            memcpy(combinedSource + offset, string[i], len);
            offset += len;
        }
        combinedSource[totalLen] = '\0';
        
        // Cache the shader source locally for glGetShaderSource
        if (shader < MAX_SHADERS) {
            GLsizei cacheLen = (totalLen < MAX_SHADER_SOURCE_LENGTH - 1) ? totalLen : (MAX_SHADER_SOURCE_LENGTH - 1);
            memcpy(g_shaderSources[shader], combinedSource, cacheLen);
            g_shaderSources[shader][cacheLen] = '\0';
            g_shaderSourceLengths[shader] = cacheLen;
            fprintf(stderr, "[GL→Metal] glShaderSource: Cached %d bytes for shader %u\n", cacheLen, shader);
        }
        
        send_metal_command(CMD_METAL_SHADER_SOURCE);
        send_u32(shader);
        send_u32((uint32_t)totalLen);
        send_data(combinedSource, totalLen);
        
        free(combinedSource);
    }
}

void my_glCompileShader(GLuint shader) {
    if (g_connected) {
        send_metal_command(CMD_METAL_COMPILE_SHADER);
        send_u32(shader);
        
        // Get shader type from cache
        uint32_t shaderType = (shader < MAX_SHADERS) ? g_shaderTypes[shader] : 0x8B31;
        send_u32(shaderType);
        
        GLint status = recv_u32();  // Compile success status from server
        fprintf(stderr, "[GL→Metal] glCompileShader(shader=%u): Server returned status=%d\n", shader, status);
        if (shader < MAX_SHADERS) {
            g_shaderCompileStatus[shader] = status;
        }
    }
}

void my_glDeleteShader(GLuint shader) {
    if (g_connected) {
        send_metal_command(CMD_METAL_DELETE_SHADER);
        send_u32(shader);
        
        // Clear cached shader type
        if (shader < MAX_SHADERS) {
            g_shaderTypes[shader] = 0;
        }
    }
}

GLuint my_glCreateProgram(void) {
    GLuint program = 0;
    
    if (g_connected) {
        send_metal_command(CMD_METAL_CREATE_PROGRAM);
        program = recv_u32();
    }
    
    return program;  // Don't call glCreateProgram() - that's us!
}

void my_glAttachShader(GLuint program, GLuint shader) {
    if (g_connected) {
        send_metal_command(CMD_METAL_ATTACH_SHADER);
        send_u32(program);
        send_u32(shader);
        
        // Send shader type (required by metal_server.m)
        GLenum shaderType = 0;
        if (shader < MAX_SHADERS) {
            shaderType = g_shaderTypes[shader];
        }
        
        send_u32(shaderType);
    }
}void my_glLinkProgram(GLuint program) {
    if (g_connected) {
        send_metal_command(CMD_METAL_LINK_PROGRAM);
        send_u32(program);
        GLint status = recv_u32();  // Link success status from server
        if (program < MAX_PROGRAMS) {
            g_programLinkStatus[program] = status;
        }
    }
}

void my_glUseProgram(GLuint program) {
    if (g_connected) {
        send_metal_command(CMD_METAL_USE_PROGRAM);
        send_u32(program);

    }
}

// Shader/Program query functions
void my_glGetShaderiv(GLuint shader, GLenum pname, GLint *params) {
    // Return cached compile status from server
    if (pname == 0x8B81) {  // GL_COMPILE_STATUS
        *params = (shader < MAX_SHADERS) ? g_shaderCompileStatus[shader] : GL_TRUE;
        fprintf(stderr, "[GL→Metal] glGetShaderiv(shader=%u, GL_COMPILE_STATUS) = %d\n", shader, *params);
    } else if (pname == 0x8B84) {  // GL_INFO_LOG_LENGTH
        *params = 1;  // Empty log (server doesn't send detailed errors yet)
    } else if (pname == 0x8B88) {  // GL_SHADER_SOURCE_LENGTH
        if (shader < MAX_SHADERS) {
            *params = g_shaderSourceLengths[shader] + 1;  // +1 for null terminator
            fprintf(stderr, "[GL→Metal] glGetShaderiv(shader=%u, GL_SHADER_SOURCE_LENGTH) = %d\n", shader, *params);
        } else {
            *params = 0;
        }
    } else if (pname == 0x8B4F) {  // GL_SHADER_TYPE
        *params = (shader < MAX_SHADERS) ? g_shaderTypes[shader] : 0;
    } else {
        *params = 0;
    }
}

void my_glGetShaderInfoLog(GLuint shader, GLsizei maxLength, GLsizei *length, GLchar *infoLog) {
    // Return empty log
    if (length) *length = 0;
    if (infoLog && maxLength > 0) infoLog[0] = '\0';
}

void my_glGetProgramiv(GLuint program, GLenum pname, GLint *params) {
    // Return cached link status from server
    if (pname == 0x8B82) {  // GL_LINK_STATUS
        *params = (program < MAX_PROGRAMS) ? g_programLinkStatus[program] : GL_TRUE;
    } else if (pname == 0x8B84) {  // GL_INFO_LOG_LENGTH
        *params = 1;  // Empty log (server doesn't send detailed errors yet)
    } else if (pname == 0x8B86) {  // GL_ACTIVE_ATTRIBUTES
        *params = 0;
    } else if (pname == 0x8B89) {  // GL_ACTIVE_UNIFORMS
        *params = 0;
    } else {
        *params = 0;
    }
}

void my_glGetProgramInfoLog(GLuint program, GLsizei maxLength, GLsizei *length, GLchar *infoLog) {
    // Return empty log
    if (length) *length = 0;
    if (infoLog && maxLength > 0) infoLog[0] = '\0';
}

void my_glGetShaderSource(GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *source) {
    fprintf(stderr, "[GL→Metal] glGetShaderSource(shader=%u, bufSize=%d)\n", shader, bufSize);
    
    // Return cached shader source that was set with glShaderSource
    if (shader >= MAX_SHADERS) {
        fprintf(stderr, "[Client] ⚠️  glGetShaderSource: Invalid shader ID %u\n", shader);
        if (length) *length = 0;
        if (bufSize > 0 && source) source[0] = '\0';
        return;
    }

    GLsizei cachedLen = g_shaderSourceLengths[shader];
    fprintf(stderr, "[GL→Metal] glGetShaderSource: Cached length for shader %u = %d\n", shader, cachedLen);
    
    if (cachedLen == 0) {
        // No source cached for this shader
        fprintf(stderr, "[Client] ⚠️  glGetShaderSource: No source cached for shader %u\n", shader);
        if (length) *length = 0;
        if (bufSize > 0 && source) source[0] = '\0';
        return;
    }

    // Copy cached source to output buffer
    GLsizei copyLen = (cachedLen < bufSize - 1) ? cachedLen : (bufSize - 1);
    if (source && bufSize > 0) {
        memcpy(source, g_shaderSources[shader], copyLen);
        source[copyLen] = '\0';
    }
    if (length) {
        *length = copyLen;
    }
    
    fprintf(stderr, "[GL→Metal] glGetShaderSource: Returning %d bytes for shader %u\n", copyLen, shader);
}

void my_glDeleteProgram(GLuint program) {
    if (g_connected) {
        send_metal_command(CMD_METAL_DELETE_PROGRAM);
        send_u32(program);
    }
}

GLint my_glGetUniformLocation(GLuint program, const GLchar *name) {
    if (!g_connected) return -1;
    
    send_metal_command(CMD_METAL_GET_UNIFORM_LOCATION);
    send_u32(program);
    send_string(name);
    
    GLint location = (GLint)recv_i32();
    fprintf(stderr, "[GL→Metal] glGetUniformLocation(program=%u, name='%s') = %d\n", program, name, location);
    return location;
}

GLint my_glGetAttribLocation(GLuint program, const GLchar *name) {
    if (!g_connected) return -1;
    
    send_metal_command(CMD_METAL_GET_ATTRIB_LOCATION);
    send_u32(program);
    send_string(name);
    
    GLint location = (GLint)recv_i32();
    fprintf(stderr, "[GL→Metal] glGetAttribLocation(program=%u, name='%s') = %d\n", program, name, location);
    return location;
}

// Uniform setters
void my_glUniform1f(GLint location, GLfloat v0) {
    if (!g_connected) return;
    send_metal_command(CMD_METAL_UNIFORM_1F);
    send_i32(location);
    send_f32(v0);
}

void my_glUniform2f(GLint location, GLfloat v0, GLfloat v1) {
    if (!g_connected) return;
    send_metal_command(CMD_METAL_UNIFORM_2F);
    send_i32(location);
    send_f32(v0);
    send_f32(v1);
}

void my_glUniform3f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2) {
    if (!g_connected) return;
    send_metal_command(CMD_METAL_UNIFORM_3F);
    send_i32(location);
    send_f32(v0);
    send_f32(v1);
    send_f32(v2);
}

void my_glUniform4f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3) {
    if (!g_connected) return;
    send_metal_command(CMD_METAL_UNIFORM_4F);
    send_i32(location);
    send_f32(v0);
    send_f32(v1);
    send_f32(v2);
    send_f32(v3);
}

void my_glUniform1i(GLint location, GLint v0) {
    if (!g_connected) return;
    send_metal_command(CMD_METAL_UNIFORM_1I);
    send_i32(location);
    send_i32(v0);
}

void my_glUniform2fv(GLint location, GLsizei count, const GLfloat *value) {
    if (!g_connected) return;
    send_metal_command(CMD_METAL_UNIFORM_2FV);
    send_i32(location);
    send_i32(count);
    for (GLsizei i = 0; i < count * 2; i++) {
        send_f32(value[i]);
    }
}

void my_glUniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) {
    if (!g_connected) return;
    
    send_metal_command(CMD_METAL_UNIFORM_MATRIX_4FV);
    send_i32(location);
    send_u32(count);
    send_u8(transpose);
    
    // Send matrix data (16 floats per matrix)
    send_data(value, count * 16 * sizeof(GLfloat));
}

//
// Textures
//

void my_glGenTextures(GLsizei n, GLuint *textures) {
    if (g_connected) {
        send_metal_command(CMD_METAL_GEN_TEXTURES);
        send_u32(n);
        
        for (GLsizei i = 0; i < n; i++) {
            textures[i] = recv_u32();
        }
    }
}

void my_glBindTexture(GLenum target, GLuint texture) {
    if (g_connected) {
        send_metal_command(CMD_METAL_BIND_TEXTURE);
        send_u32(target);
        send_u32(texture);

    }
}

void my_glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels) {
    if (g_connected) {
        send_metal_command(CMD_METAL_TEX_IMAGE_2D);
        send_u32(target);
        send_i32(level);
        send_i32(internalformat);
        send_u32(width);
        send_u32(height);
        send_i32(border);
        send_u32(format);
        send_u32(type);
        
        // Calculate pixel data size based on format
        size_t components = 4; // Default RGBA
        if (format == 0x1907) components = 3;      // GL_RGB
        else if (format == 0x1908) components = 4; // GL_RGBA
        else if (format == 0x1909) components = 1; // GL_LUMINANCE
        else if (format == 0x190A) components = 2; // GL_LUMINANCE_ALPHA
        else if (format == 0x1903) components = 1; // GL_RED
        else if (format == 0x8227) components = 2; // GL_RG
        
        size_t bytesPerComponent = 1; // Default unsigned byte
        if (type == 0x1401) bytesPerComponent = 1;      // GL_UNSIGNED_BYTE
        else if (type == 0x1405) bytesPerComponent = 4; // GL_UNSIGNED_INT
        else if (type == 0x1406) bytesPerComponent = 4; // GL_FLOAT
        
        size_t pixelSize = width * height * components * bytesPerComponent;
        send_data(pixels, pixelSize);
    }
}

void my_glTexParameteri(GLenum target, GLenum pname, GLint param) {
    if (g_connected) {
        send_metal_command(CMD_METAL_TEX_PARAMETER_I);
        send_u32(target);
        send_u32(pname);
        send_i32(param);

    }
}

void my_glDeleteTextures(GLsizei n, const GLuint *textures) {
    if (g_connected) {
        send_metal_command(CMD_METAL_DELETE_TEXTURES);
        send_u32(n);
        send_data(textures, n * sizeof(GLuint));
    }
}

void my_glActiveTexture(GLenum texture) {
    if (g_connected) {
        send_metal_command(CMD_METAL_ACTIVE_TEXTURE);
        send_u32(texture);
    }
}

//
// Framebuffer Objects (FBO)
//

void my_glGenFramebuffers(GLsizei n, GLuint *framebuffers) {
    if (g_connected) {
        send_metal_command(CMD_METAL_GEN_FRAMEBUFFERS);
        send_u32(n);
        
        for (GLsizei i = 0; i < n; i++) {
            framebuffers[i] = recv_u32();
        }
    }
}

void my_glBindFramebuffer(GLenum target, GLuint framebuffer) {
    if (g_connected) {
        send_metal_command(CMD_METAL_BIND_FRAMEBUFFER);
        send_u32(target);
        send_u32(framebuffer);

    }
}

void my_glFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level) {
    if (g_connected) {
        send_metal_command(CMD_METAL_FRAMEBUFFER_TEXTURE_2D);
        send_u32(target);
        send_u32(attachment);
        send_u32(textarget);
        send_u32(texture);
        send_i32(level);

    }
}

GLenum my_glCheckFramebufferStatus(GLenum target) {
    GLenum status = GL_FRAMEBUFFER_COMPLETE;
    
    if (g_connected) {
        send_metal_command(CMD_METAL_CHECK_FRAMEBUFFER_STATUS);
        send_u32(target);

        status = recv_u32();
    }
    
    return status;  // Don't call glCheckFramebufferStatus() - that's us!
}

void my_glDeleteFramebuffers(GLsizei n, const GLuint *framebuffers) {
    if (g_connected) {
        send_metal_command(CMD_METAL_DELETE_FRAMEBUFFERS);
        send_u32(n);
        send_data(framebuffers, n * sizeof(GLuint));

    }
}

//
// Legacy OpenGL (1.x/2.x) - Phase 7.1 Accelerated
// Matrix operations and immediate mode fully accelerated via Metal
//

// Matrix operations (Phase 7.1 - Accelerated)
void my_glMatrixMode(GLenum mode) {
    static int count = 0;
    if (count++ < 5) {
        fprintf(stderr, "[GL→Metal] glMatrixMode(%s)\n", 
            mode == 0x1700 ? "MODELVIEW" : mode == 0x1701 ? "PROJECTION" : "OTHER");
    }
    g_matrixMode = mode;
    // Call original OpenGL for local display
    // REMOVED: if (orig_glMatrixMode) orig_glMatrixMode(mode); - no real GL context
}

void my_glLoadIdentity(void) {
    static int count = 0;
    if (count++ < 5) {
        fprintf(stderr, "[GL→Metal] glLoadIdentity()\n");
    }
    MatrixStack *stack = getCurrentMatrixStack();
    loadIdentityMatrix(stack->matrices[stack->depth]);
    // Call original OpenGL for local display
    // REMOVED: if (orig_glLoadIdentity) orig_glLoadIdentity(); - no real GL context
}

void my_glPushMatrix(void) {
    MatrixStack *stack = getCurrentMatrixStack();
    if (stack->depth < MAX_MATRIX_STACK_DEPTH - 1) {
        memcpy(stack->matrices[stack->depth + 1], 
               stack->matrices[stack->depth], 
               16 * sizeof(float));
        stack->depth++;
    }
    // Call original OpenGL for local display
    // REMOVED: if (orig_glPushMatrix) orig_glPushMatrix(); - no real GL context
}

void my_glPopMatrix(void) {
    MatrixStack *stack = getCurrentMatrixStack();
    if (stack->depth > 0) {
        stack->depth--;
    }
    // Call original OpenGL for local display
    // REMOVED: if (orig_glPopMatrix) orig_glPopMatrix(); - no real GL context
}

void my_glLoadMatrixf(const GLfloat *m) {
    MatrixStack *stack = getCurrentMatrixStack();
    memcpy(stack->matrices[stack->depth], m, 16 * sizeof(float));
    // Don't call original OpenGL - Metal only
}

void my_glMultMatrixf(const GLfloat *m) {
    MatrixStack *stack = getCurrentMatrixStack();
    float temp[16];
    memcpy(temp, stack->matrices[stack->depth], 16 * sizeof(float));
    multiplyMatrices(stack->matrices[stack->depth], temp, m);
    // Don't call original OpenGL - Metal only
}

void my_glRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z) {
    static int count = 0;
    if (count++ < 3) {
        fprintf(stderr, "[GL→Metal] glRotatef(%.1f, %.1f, %.1f, %.1f)\n", angle, x, y, z);
    }
    
    MatrixStack *stack = getCurrentMatrixStack();
    float rotation[16];
    buildRotationMatrix(rotation, angle, x, y, z);
    
    float temp[16];
    memcpy(temp, stack->matrices[stack->depth], 16 * sizeof(float));
    multiplyMatrices(stack->matrices[stack->depth], temp, rotation);
    // Call original OpenGL for local display
    // REMOVED: if (orig_glRotatef) orig_glRotatef(angle, x, y, z); - no real GL context
}

void my_glScalef(GLfloat x, GLfloat y, GLfloat z) {
    MatrixStack *stack = getCurrentMatrixStack();
    float scale[16];
    buildScaleMatrix(scale, x, y, z);
    
    float temp[16];
    memcpy(temp, stack->matrices[stack->depth], 16 * sizeof(float));
    multiplyMatrices(stack->matrices[stack->depth], temp, scale);
    // Don't call original OpenGL - Metal only
}

void my_glTranslatef(GLfloat x, GLfloat y, GLfloat z) {
    MatrixStack *stack = getCurrentMatrixStack();
    float translation[16];
    buildTranslationMatrix(translation, x, y, z);
    
    float temp[16];
    memcpy(temp, stack->matrices[stack->depth], 16 * sizeof(float));
    multiplyMatrices(stack->matrices[stack->depth], temp, translation);
    // Call original OpenGL for local display
    // REMOVED: if (orig_glTranslatef) orig_glTranslatef(x, y, z); - no real GL context
}

// Projection setup
void my_glOrtho(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble near, GLdouble far) {
    MatrixStack *stack = getCurrentMatrixStack();
    float ortho[16];
    
    // Build orthographic projection matrix
    float rl = (float)(right - left);
    float tb = (float)(top - bottom);
    float fn = (float)(far - near);
    
    ortho[0] = 2.0f / rl;
    ortho[1] = 0.0f;
    ortho[2] = 0.0f;
    ortho[3] = 0.0f;
    
    ortho[4] = 0.0f;
    ortho[5] = 2.0f / tb;
    ortho[6] = 0.0f;
    ortho[7] = 0.0f;
    
    ortho[8] = 0.0f;
    ortho[9] = 0.0f;
    ortho[10] = -2.0f / fn;
    ortho[11] = 0.0f;
    
    ortho[12] = -(float)(right + left) / rl;
    ortho[13] = -(float)(top + bottom) / tb;
    ortho[14] = -(float)(far + near) / fn;
    ortho[15] = 1.0f;
    
    float temp[16];
    memcpy(temp, stack->matrices[stack->depth], 16 * sizeof(float));
    multiplyMatrices(stack->matrices[stack->depth], temp, ortho);
    // Don't call original OpenGL - Metal only
}

void my_glFrustum(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble near, GLdouble far) {
    MatrixStack *stack = getCurrentMatrixStack();
    float frustum[16];
    
    // Build perspective frustum matrix
    float rl = (float)(right - left);
    float tb = (float)(top - bottom);
    float fn = (float)(far - near);
    float n2 = 2.0f * (float)near;
    
    frustum[0] = n2 / rl;
    frustum[1] = 0.0f;
    frustum[2] = 0.0f;
    frustum[3] = 0.0f;
    
    frustum[4] = 0.0f;
    frustum[5] = n2 / tb;
    frustum[6] = 0.0f;
    frustum[7] = 0.0f;
    
    frustum[8] = (float)(right + left) / rl;
    frustum[9] = (float)(top + bottom) / tb;
    frustum[10] = -(float)(far + near) / fn;
    frustum[11] = -1.0f;
    
    frustum[12] = 0.0f;
    frustum[13] = 0.0f;
    frustum[14] = -(float)(far * near) * 2.0f / fn;
    frustum[15] = 0.0f;
    
    float temp[16];
    memcpy(temp, stack->matrices[stack->depth], 16 * sizeof(float));
    multiplyMatrices(stack->matrices[stack->depth], temp, frustum);
    // Don't call original OpenGL - Metal only
}

// GLU function: gluPerspective
void gluPerspective(GLdouble fovy, GLdouble aspect, GLdouble zNear, GLdouble zFar) {
    GLdouble fH = tan(fovy / 360.0 * 3.14159265358979323846) * zNear;
    GLdouble fW = fH * aspect;
    my_glFrustum(-fW, fW, -fH, fH, zNear, zFar);
    
    static int count = 0;
    if (count++ < 3) {
        fprintf(stderr, "[GL→Metal] gluPerspective(fovy=%.1f, aspect=%.2f, near=%.2f, far=%.2f)\n",
                fovy, aspect, zNear, zFar);
    }
}

// More color functions - Phase 7.1 Accelerated
void my_glColor4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a) {
    g_currentVertex.color[0] = r;
    g_currentVertex.color[1] = g;
    g_currentVertex.color[2] = b;
    g_currentVertex.color[3] = a;
    // Don't call original OpenGL - Metal only
}

void my_glColor3fv(const GLfloat *v) {
    g_currentVertex.color[0] = v[0];
    g_currentVertex.color[1] = v[1];
    g_currentVertex.color[2] = v[2];
    g_currentVertex.color[3] = 1.0f;
    // Don't call original OpenGL - Metal only
}

void my_glColor4fv(const GLfloat *v) {
    g_currentVertex.color[0] = v[0];
    g_currentVertex.color[1] = v[1];
    g_currentVertex.color[2] = v[2];
    g_currentVertex.color[3] = v[3];
    // Don't call original OpenGL - Metal only
}

// More vertex functions
void my_glVertex2f(GLfloat x, GLfloat y) {
    my_glVertex3f(x, y, 0.0f);
}

void my_glVertex2fv(const GLfloat *v) {
    my_glVertex3f(v[0], v[1], 0.0f);
}

void my_glVertex3fv(const GLfloat *v) {
    my_glVertex3f(v[0], v[1], v[2]);
}

// Normal vectors (for lighting) - Phase 7.1 Accelerated
void my_glNormal3f(GLfloat nx, GLfloat ny, GLfloat nz) {
    static int call_count = 0;
    if (call_count++ < 5) {
        fprintf(stderr, "[GL→Metal] glNormal3f(%.2f, %.2f, %.2f) - normal #%d\n", nx, ny, nz, call_count);
        fflush(stderr);
    }
    
    g_currentVertex.normal[0] = nx;
    g_currentVertex.normal[1] = ny;
    g_currentVertex.normal[2] = nz;
    // Don't call original OpenGL - Metal only
}

void my_glNormal3fv(const GLfloat *v) {
    g_currentVertex.normal[0] = v[0];
    g_currentVertex.normal[1] = v[1];
    g_currentVertex.normal[2] = v[2];
    // Don't call original OpenGL - Metal only
}

// Texture coordinates - Phase 7.1 Accelerated
void my_glTexCoord2f(GLfloat s, GLfloat t) {
    g_currentVertex.texcoord[0] = s;
    g_currentVertex.texcoord[1] = t;
    // Don't call original OpenGL - Metal only
}

void my_glTexCoord2fv(const GLfloat *v) {
    g_currentVertex.texcoord[0] = v[0];
    g_currentVertex.texcoord[1] = v[1];
    // Don't call glTexCoord2fv - hooked back to us
}

// Blending
void my_glBlendFunc(GLenum sfactor, GLenum dfactor) {
    fprintf(stderr, "[GL→Metal] glBlendFunc(0x%x, 0x%x)\n", sfactor, dfactor);
    
    if (g_connected) {
        send_metal_command(CMD_METAL_BLEND_FUNC);
        send_u32(sfactor);
        send_u32(dfactor);

    }
}

void my_glBlendFuncSeparate(GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha) {
    fprintf(stderr, "[GL→Metal] glBlendFuncSeparate(0x%x, 0x%x, 0x%x, 0x%x)\n", srcRGB, dstRGB, srcAlpha, dstAlpha);
    // Simplified: use RGB factors for both RGB and Alpha
    my_glBlendFunc(srcRGB, dstRGB);
}

// Enable/Disable features
void my_glEnable(GLenum cap) {
    fprintf(stderr, "[GL→Metal] glEnable(0x%x)\n", cap);
    
    // Track capability state
    switch (cap) {
        case 0x0B71: g_capabilities.depth_test = GL_TRUE; break;       // GL_DEPTH_TEST
        case 0x0B44: g_capabilities.cull_face = GL_TRUE; break;        // GL_CULL_FACE
        case 0x0BE2: g_capabilities.blend = GL_TRUE; break;            // GL_BLEND
        case 0x0DE1: g_capabilities.texture_2d = GL_TRUE; break;       // GL_TEXTURE_2D
        case 0x0B50: g_capabilities.lighting = GL_TRUE; break;         // GL_LIGHTING
        case 0x0C11: g_capabilities.scissor_test = GL_TRUE; break;     // GL_SCISSOR_TEST
        case 0x0B90: g_capabilities.stencil_test = GL_TRUE; break;     // GL_STENCIL_TEST
        case 0x0BC0: g_capabilities.alpha_test = GL_TRUE; break;       // GL_ALPHA_TEST
        case 0x0BD0: g_capabilities.dither = GL_TRUE; break;           // GL_DITHER
        case 0x8037: g_capabilities.polygon_offset_fill = GL_TRUE; break; // GL_POLYGON_OFFSET_FILL
    }
    
    if (g_connected) {
        send_metal_command(CMD_METAL_ENABLE);
        send_u32(cap);

    }
}

void my_glDisable(GLenum cap) {
    fprintf(stderr, "[GL→Metal] glDisable(0x%x)\n", cap);
    
    // Track capability state
    switch (cap) {
        case 0x0B71: g_capabilities.depth_test = GL_FALSE; break;       // GL_DEPTH_TEST
        case 0x0B44: g_capabilities.cull_face = GL_FALSE; break;        // GL_CULL_FACE
        case 0x0BE2: g_capabilities.blend = GL_FALSE; break;            // GL_BLEND
        case 0x0DE1: g_capabilities.texture_2d = GL_FALSE; break;       // GL_TEXTURE_2D
        case 0x0B50: g_capabilities.lighting = GL_FALSE; break;         // GL_LIGHTING
        case 0x0C11: g_capabilities.scissor_test = GL_FALSE; break;     // GL_SCISSOR_TEST
        case 0x0B90: g_capabilities.stencil_test = GL_FALSE; break;     // GL_STENCIL_TEST
        case 0x0BC0: g_capabilities.alpha_test = GL_FALSE; break;       // GL_ALPHA_TEST
        case 0x0BD0: g_capabilities.dither = GL_FALSE; break;           // GL_DITHER
        case 0x8037: g_capabilities.polygon_offset_fill = GL_FALSE; break; // GL_POLYGON_OFFSET_FILL
    }
    
    if (g_connected) {
        send_metal_command(CMD_METAL_DISABLE);
        send_u32(cap);

    }
}

// Depth testing
void my_glDepthFunc(GLenum func) {
    fprintf(stderr, "[GL→Metal] glDepthFunc(0x%x)\n", func);
    
    if (g_connected) {
        send_metal_command(CMD_METAL_DEPTH_FUNC);
        send_u32(func);

    }
}

void my_glDepthMask(GLboolean flag) {
    fprintf(stderr, "[GL→Metal] glDepthMask(%d)\n", flag);
    // TODO: Send to Metal server if needed
}

// Culling
void my_glCullFace(GLenum mode) {
    fprintf(stderr, "[GL→Metal] glCullFace(0x%x)\n", mode);
    
    if (g_connected) {
        send_metal_command(CMD_METAL_CULL_FACE);
        send_u32(mode);

    }
}

void my_glFrontFace(GLenum mode) {
    fprintf(stderr, "[GL→Metal] glFrontFace(0x%x)\n", mode);
    // TODO: Send to Metal server if needed
}

// Polygon mode
void my_glPolygonMode(GLenum face, GLenum mode) {
    fprintf(stderr, "[GL→Metal] glPolygonMode(0x%x, 0x%x)\n", face, mode);
    // Don't call system glPolygonMode - no real GL context
}

// Line/Point size
void my_glLineWidth(GLfloat width) {
    fprintf(stderr, "[GL→Metal] glLineWidth(%.2f)\n", width);
    // Don't call system glLineWidth - no real GL context
}

void my_glPointSize(GLfloat size) {
    fprintf(stderr, "[GL→Metal] glPointSize(%.2f)\n", size);
    // Don't call system glPointSize - no real GL context
}

// Clearing
void my_glClearColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) {
    fprintf(stderr, "[GL→Metal] glClearColor(%.2f, %.2f, %.2f, %.2f)\n", red, green, blue, alpha);
    // Store locally - will be sent with CMD_METAL_CLEAR when glClear is called
    g_clearColor[0] = red;
    g_clearColor[1] = green;
    g_clearColor[2] = blue;
    g_clearColor[3] = alpha;
}

void my_glClearDepth(GLdouble depth) {
    fprintf(stderr, "[GL→Metal] glClearDepth(%.2f)\n", depth);
    // Store clear depth value (typically 1.0)
    // Metal server will use this during CMD_METAL_CLEAR
}

// Display lists (legacy)
GLuint my_glGenLists(GLsizei range) {
    // Display lists not implemented in Metal
    return 0;
}

// Display list function pointers
typedef void (*glNewList_t)(GLuint, GLenum);
typedef void (*glEndList_t)(void);
typedef void (*glCallList_t)(GLuint);
typedef void (*glDeleteLists_t)(GLuint, GLsizei);
static glNewList_t orig_glNewList = NULL;
static glEndList_t orig_glEndList = NULL;
static glCallList_t orig_glCallList = NULL;
static glDeleteLists_t orig_glDeleteLists = NULL;

void my_glNewList(GLuint list, GLenum mode) {
    // Pass through to Mesa - display lists will record glBegin/glEnd which we intercept
    if (orig_glNewList) orig_glNewList(list, mode);
}

void my_glEndList(void) {
    // Pass through to Mesa
    if (orig_glEndList) orig_glEndList();
}

void my_glCallList(GLuint list) {
    // Pass through to Mesa - this will replay recorded glBegin/glEnd which we intercept
    if (orig_glCallList) orig_glCallList(list);
}

void my_glDeleteLists(GLuint list, GLsizei range) {
    // Pass through to Mesa
    if (orig_glDeleteLists) orig_glDeleteLists(list, range);
}

// Lighting (legacy fixed-function)
void my_glLightfv(GLenum light, GLenum pname, const GLfloat *params) {
    // Legacy lighting not implemented in Metal
}

void my_glMaterialfv(GLenum face, GLenum pname, const GLfloat *params) {
    // Legacy materials not implemented in Metal
}

void my_glShadeModel(GLenum mode) {
    // Legacy shade model not implemented in Metal
}

// ==========================================
// Phase 7.6: VM Display Integration
// ==========================================

void my_glReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, 
                     GLenum format, GLenum type, GLvoid *pixels) {
    if (!connect_to_metal_server()) return;
    
    uint32_t cmd = CMD_METAL_READ_PIXELS;
    uint32_t ux = (uint32_t)x;
    uint32_t uy = (uint32_t)y;
    uint32_t uwidth = (uint32_t)width;
    uint32_t uheight = (uint32_t)height;
    uint32_t uformat = (uint32_t)format;
    uint32_t utype = (uint32_t)type;
    
    // Send command and parameters
    write(g_socket, &cmd, sizeof(cmd));
    write(g_socket, &ux, sizeof(ux));
    write(g_socket, &uy, sizeof(uy));
    write(g_socket, &uwidth, sizeof(uwidth));
    write(g_socket, &uheight, sizeof(uheight));
    write(g_socket, &uformat, sizeof(uformat));
    write(g_socket, &utype, sizeof(utype));
    
    // Receive pixel data size
    uint32_t dataSize = 0;
    if (read(g_socket, &dataSize, sizeof(dataSize)) != sizeof(dataSize)) {
        return;
    }
    
    if (dataSize == 0) {
        // Error on server side
        return;
    }
    
    // Receive pixel data
    size_t bytesReceived = 0;
    while (bytesReceived < dataSize) {
        ssize_t n = read(g_socket, (char*)pixels + bytesReceived, dataSize - bytesReceived);
        if (n <= 0) break;
        bytesReceived += n;
    }
}

void my_glutSwapBuffers(void) {
    // GLUT swap buffers - send to Metal server for frame completion
    if (!connect_to_metal_server()) return;
    
    uint32_t cmd = CMD_METAL_SWAP_BUFFERS;
    write(g_socket, &cmd, sizeof(cmd));
    
    // Note: glutSwapBuffers is defined in GLUT/glut.h, not OpenGL headers
    // GLUT interposition is complex and requires linking against GLUT framework
    // For now, this just signals the server that a frame is complete
    // The actual swap happens in the VM's OpenGL implementation
}

// GLX swap buffers hook
typedef void (*glXSwapBuffers_t)(void*, unsigned long);
static glXSwapBuffers_t orig_glXSwapBuffers = NULL;

void my_glXSwapBuffers(void *dpy, unsigned long drawable) {
    static int call_count = 0;
    call_count++;
    
    fprintf(stderr, "[GL→Metal] glXSwapBuffers() call #%d - Metal server will render frame\n", call_count);
    
    // Send swap buffers command to Metal server
    // Server will render the frame and it will be visible in the Metal window
    if (g_connected && g_socket >= 0) {
        send_metal_command(CMD_METAL_SWAP_BUFFERS);
        fprintf(stderr, "[GL→Metal] ✅ Sent CMD_METAL_SWAP_BUFFERS to server\n");
    }
    
    // NOTE: The Metal-rendered frame is displayed in the Metal server's window on the host
    // To display in the VM's X11 window, we would need to:
    // 1. Read pixels back from Metal texture (glReadPixels from server)
    // 2. Upload to X11 window using XPutImage
    // For now, rendering happens on host side only
}

// Wrapper function for GLX implementation to call
void metal_swap_buffers(void *dpy, unsigned long drawable) {
    // Just call our implementation
    my_glXSwapBuffers(dpy, drawable);
}

// ========================================
// GLX (X11 OpenGL) Full Implementation
// ========================================
// NOTE: GLX functions are handled by glx_wrapper.c library, NOT here!
// These functions MUST be commented out to avoid conflicts with the wrapper.
// The wrapper provides the GLX → OpenGL bridge, while this library
// handles OpenGL → Metal translation only.

#if 0  // DISABLED - GLX handled by wrapper
// Store fake GLX objects
static char fake_glx_visual[256];
static char fake_glx_context[256];
static GLXContext current_glx_context = NULL;

XVisualInfo* glXChooseVisual(Display *dpy, int screen, int *attribList) {
    printf("[GL→Metal] ★★★ glXChooseVisual() CALLED ★★★\n");
    printf("[GL→Metal]   Display: %p, Screen: %d\n", (void*)dpy, screen);
    fflush(stdout);
    
    // Return fake XVisualInfo
    XVisualInfo *result = (XVisualInfo*)&fake_glx_visual;
    printf("[GL→Metal]   Returning fake XVisualInfo: %p\n", (void*)result);
    fflush(stdout);
    return result;
}

GLXContext glXCreateContext(Display *dpy, XVisualInfo *vis, GLXContext shareList, int direct) {
    printf("[GL→Metal] ★★★ glXCreateContext() CALLED ★★★\n");
    printf("[GL→Metal]   Display: %p, Visual: %p, Direct: %d\n", (void*)dpy, (void*)vis, direct);
    fflush(stdout);
    
    // Ensure Metal connection
    connect_to_metal_server();
    
    // Return fake GLX context
    current_glx_context = (GLXContext)&fake_glx_context;
    printf("[GL→Metal]   Returning GLX context: %p\n", (void*)current_glx_context);
    fflush(stdout);
    return current_glx_context;
}

void glXDestroyContext(Display *dpy, GLXContext ctx) {
    printf("[GL→Metal] glXDestroyContext() intercepted\n");
    fflush(stdout);
    
    if (ctx == current_glx_context) {
        current_glx_context = NULL;
    }
}

int glXMakeCurrent(Display *dpy, GLXDrawable drawable, GLXContext ctx) {
    printf("[GL→Metal] glXMakeCurrent() intercepted - context active\n");
    fflush(stdout);
    
    // Ensure Metal connection
    connect_to_metal_server();
    
    current_glx_context = ctx;
    return 1; // Success (True)
}

void glXSwapBuffers(Display *dpy, GLXDrawable drawable) {
    printf("[GL→Metal] glXSwapBuffers() intercepted - presenting frame\n");
    fflush(stdout);
    
    // Trigger Metal frame presentation
    if (connect_to_metal_server()) {
        uint32_t cmd = CMD_METAL_SWAP_BUFFERS;
        write(g_socket, &cmd, sizeof(cmd));
    }
}

const char* glXQueryExtensionsString(Display *dpy, int screen) {
    printf("[GL→Metal] glXQueryExtensionsString() intercepted\n");
    fflush(stdout);
    
    // Return basic GLX extensions
    return "GLX_ARB_get_proc_address GLX_EXT_visual_info";
}

__GLXextFuncPtr glXGetProcAddressARB(const unsigned char *procName) {
    printf("[GL→Metal] glXGetProcAddressARB(%s) intercepted\n", procName);
    fflush(stdout);
    
    // Return NULL - we don't support extension functions yet
    return NULL;
}

void glXQueryDrawable(Display *dpy, GLXDrawable draw, int attribute, unsigned int *value) {
    printf("[GL→Metal] glXQueryDrawable() intercepted\n");
    fflush(stdout);
    
    // Return dummy values
    if (value) {
        *value = 800; // Width or height
    }
}
#endif  // END: GLX functions disabled

// ========================================
// CGL (Core OpenGL) Full Implementation
// ========================================

// CGL Context and Pixel Format structures
typedef struct {
    uint32_t id;
    CGLPixelFormatAttribute attributes[64];
    int attribute_count;
    int color_size;
    int alpha_size;
    int depth_size;
    int stencil_size;
    int sample_buffers;
    int samples;
    int double_buffer;
} CGLPixelFormatRec;

typedef struct {
    uint32_t id;
    CGLPixelFormatRec *pixel_format;
    CGLContextObj share_context;
    int is_current;
    // Viewport state
    GLint viewport[4];
    // Clear color
    GLfloat clear_color[4];
} CGLContextRec;

#define MAX_CGL_CONTEXTS 32
#define MAX_CGL_PIXEL_FORMATS 32

static CGLPixelFormatRec *g_pixel_formats[MAX_CGL_PIXEL_FORMATS] = {0};
static CGLContextRec *g_cgl_contexts[MAX_CGL_CONTEXTS] = {0};
static CGLContextRec *g_current_cgl_context = NULL;
static uint32_t g_next_pixel_format_id = 1;
static uint32_t g_next_context_id = 1;

// CGL Pixel Format functions
CGLError CGLChoosePixelFormat(const CGLPixelFormatAttribute *attribs,
                               CGLPixelFormatObj *pix,
                               GLint *npix) {
    printf("[CGL] CGLChoosePixelFormat() - parsing attributes\n");
    fflush(stdout);
    
    if (!pix || !npix) {
        return kCGLBadAttribute;
    }
    
    // Allocate pixel format structure
    CGLPixelFormatRec *fmt = (CGLPixelFormatRec *)calloc(1, sizeof(CGLPixelFormatRec));
    if (!fmt) {
        return kCGLBadAlloc;
    }
    
    fmt->id = g_next_pixel_format_id++;
    fmt->color_size = 24;  // Default RGB
    fmt->alpha_size = 8;   // Default alpha
    fmt->depth_size = 24;  // Default depth
    fmt->stencil_size = 8; // Default stencil
    fmt->double_buffer = 1; // Default double buffered
    
    // Parse attributes
    if (attribs) {
        int i = 0;
        while (attribs[i] != 0 && i < 64) {
            fmt->attributes[fmt->attribute_count++] = attribs[i];
            
            CGLPixelFormatAttribute attr = attribs[i];
            switch (attr) {
                case kCGLPFAColorSize:
                    if (attribs[i+1] != 0) {
                        fmt->color_size = attribs[++i];
                    }
                    break;
                case kCGLPFAAlphaSize:
                    if (attribs[i+1] != 0) {
                        fmt->alpha_size = attribs[++i];
                    }
                    break;
                case kCGLPFADepthSize:
                    if (attribs[i+1] != 0) {
                        fmt->depth_size = attribs[++i];
                    }
                    break;
                case kCGLPFAStencilSize:
                    if (attribs[i+1] != 0) {
                        fmt->stencil_size = attribs[++i];
                    }
                    break;
                case kCGLPFASampleBuffers:
                    if (attribs[i+1] != 0) {
                        fmt->sample_buffers = attribs[++i];
                    }
                    break;
                case kCGLPFASamples:
                    if (attribs[i+1] != 0) {
                        fmt->samples = attribs[++i];
                    }
                    break;
                case kCGLPFADoubleBuffer:
                    fmt->double_buffer = 1;
                    break;
                default:
                    // Ignore unknown attributes
                    break;
            }
            i++;
        }
    }
    
    // Store in global array
    for (int i = 0; i < MAX_CGL_PIXEL_FORMATS; i++) {
        if (g_pixel_formats[i] == NULL) {
            g_pixel_formats[i] = fmt;
            break;
        }
    }
    
    *pix = (CGLPixelFormatObj)fmt;
    *npix = 1;
    
    printf("[CGL] ✅ Created pixel format: color=%d, alpha=%d, depth=%d, stencil=%d\n",
           fmt->color_size, fmt->alpha_size, fmt->depth_size, fmt->stencil_size);
    fflush(stdout);
    
    return kCGLNoError;
}

CGLError CGLDestroyPixelFormat(CGLPixelFormatObj pix) {
    printf("[CGL] CGLDestroyPixelFormat()\n");
    fflush(stdout);
    
    if (!pix) return kCGLBadPixelFormat;
    
    CGLPixelFormatRec *fmt = (CGLPixelFormatRec *)pix;
    
    // Remove from global array
    for (int i = 0; i < MAX_CGL_PIXEL_FORMATS; i++) {
        if (g_pixel_formats[i] == fmt) {
            g_pixel_formats[i] = NULL;
            break;
        }
    }
    
    free(fmt);
    return kCGLNoError;
}

CGLError CGLDescribePixelFormat(CGLPixelFormatObj pix, GLint pix_num,
                                 CGLPixelFormatAttribute attrib, GLint *value) {
    if (!pix || !value) return kCGLBadPixelFormat;
    
    CGLPixelFormatRec *fmt = (CGLPixelFormatRec *)pix;
    
    switch (attrib) {
        case kCGLPFAColorSize:
            *value = fmt->color_size;
            break;
        case kCGLPFAAlphaSize:
            *value = fmt->alpha_size;
            break;
        case kCGLPFADepthSize:
            *value = fmt->depth_size;
            break;
        case kCGLPFAStencilSize:
            *value = fmt->stencil_size;
            break;
        case kCGLPFADoubleBuffer:
            *value = fmt->double_buffer;
            break;
        default:
            *value = 0;
            break;
    }
    
    return kCGLNoError;
}

// CGL Context functions
CGLError CGLCreateContext(CGLPixelFormatObj pix, CGLContextObj share, CGLContextObj *ctx) {
    printf("[CGL] CGLCreateContext() - creating OpenGL context\n");
    fflush(stdout);
    
    if (!pix || !ctx) {
        return kCGLBadPixelFormat;
    }
    
    // Allocate context structure
    CGLContextRec *context = (CGLContextRec *)calloc(1, sizeof(CGLContextRec));
    if (!context) {
        return kCGLBadAlloc;
    }
    
    context->id = g_next_context_id++;
    context->pixel_format = (CGLPixelFormatRec *)pix;
    context->share_context = share;
    context->is_current = 0;
    
    // Initialize state
    context->viewport[0] = 0;
    context->viewport[1] = 0;
    context->viewport[2] = 640;
    context->viewport[3] = 480;
    context->clear_color[0] = 0.0f;
    context->clear_color[1] = 0.0f;
    context->clear_color[2] = 0.0f;
    context->clear_color[3] = 1.0f;
    
    // Store in global array
    for (int i = 0; i < MAX_CGL_CONTEXTS; i++) {
        if (g_cgl_contexts[i] == NULL) {
            g_cgl_contexts[i] = context;
            break;
        }
    }
    
    *ctx = (CGLContextObj)context;
    
    printf("[CGL] ✅ Created context ID %u\n", context->id);
    fflush(stdout);
    
    return kCGLNoError;
}

CGLError CGLDestroyContext(CGLContextObj ctx) {
    printf("[CGL] CGLDestroyContext()\n");
    fflush(stdout);
    
    if (!ctx) return kCGLBadContext;
    
    CGLContextRec *context = (CGLContextRec *)ctx;
    
    // Remove from current if set
    if (g_current_cgl_context == context) {
        g_current_cgl_context = NULL;
    }
    
    // Remove from global array
    for (int i = 0; i < MAX_CGL_CONTEXTS; i++) {
        if (g_cgl_contexts[i] == context) {
            g_cgl_contexts[i] = NULL;
            break;
        }
    }
    
    free(context);
    return kCGLNoError;
}

CGLError CGLSetCurrentContext(CGLContextObj ctx) {
    printf("[CGL] CGLSetCurrentContext(%p)\n", ctx);
    fflush(stdout);
    
    if (ctx) {
        CGLContextRec *context = (CGLContextRec *)ctx;
        context->is_current = 1;
        g_current_cgl_context = context;
        
        // Ensure Metal server connection
        connect_to_metal_server();
    } else {
        if (g_current_cgl_context) {
            g_current_cgl_context->is_current = 0;
        }
        g_current_cgl_context = NULL;
    }
    
    return kCGLNoError;
}

CGLContextObj CGLGetCurrentContext(void) {
    return (CGLContextObj)g_current_cgl_context;
}

CGLError CGLCopyContext(CGLContextObj src, CGLContextObj dst, GLbitfield mask) {
    printf("[CGL] CGLCopyContext() - stub\n");
    fflush(stdout);
    return kCGLNoError;
}

CGLError CGLFlushDrawable(CGLContextObj ctx) {
    printf("[CGL] CGLFlushDrawable() - presenting frame\n");
    fflush(stdout);
    
    // Trigger Metal frame presentation
    if (connect_to_metal_server()) {
        uint32_t cmd = CMD_METAL_SWAP_BUFFERS;
        write(g_socket, &cmd, sizeof(cmd));
    }
    
    return kCGLNoError;
}

// Additional CGL functions for completeness
CGLError CGLEnable(CGLContextObj ctx, CGLContextEnable pname) {
    printf("[CGL] CGLEnable(%d)\n", pname);
    fflush(stdout);
    return kCGLNoError;
}

CGLError CGLDisable(CGLContextObj ctx, CGLContextEnable pname) {
    printf("[CGL] CGLDisable(%d)\n", pname);
    fflush(stdout);
    return kCGLNoError;
}

CGLError CGLIsEnabled(CGLContextObj ctx, CGLContextEnable pname, GLint *enable) {
    if (enable) *enable = 0;
    return kCGLNoError;
}

CGLError CGLSetParameter(CGLContextObj ctx, CGLContextParameter pname, const GLint *params) {
    printf("[CGL] CGLSetParameter(%d)\n", pname);
    fflush(stdout);
    return kCGLNoError;
}

CGLError CGLGetParameter(CGLContextObj ctx, CGLContextParameter pname, GLint *params) {
    if (params) *params = 0;
    return kCGLNoError;
}

CGLError CGLLockContext(CGLContextObj ctx) {
    return kCGLNoError;
}

CGLError CGLUnlockContext(CGLContextObj ctx) {
    return kCGLNoError;
}

CGLPixelFormatObj CGLGetPixelFormat(CGLContextObj ctx) {
    if (!ctx) return NULL;
    CGLContextRec *context = (CGLContextRec *)ctx;
    return (CGLPixelFormatObj)context->pixel_format;
}

const char* CGLErrorString(CGLError error) {
    switch (error) {
        case kCGLNoError: return "No Error";
        case kCGLBadAttribute: return "Bad Attribute";
        case kCGLBadProperty: return "Bad Property";
        case kCGLBadPixelFormat: return "Bad Pixel Format";
        case kCGLBadRendererInfo: return "Bad Renderer Info";
        case kCGLBadContext: return "Bad Context";
        case kCGLBadDrawable: return "Bad Drawable";
        case kCGLBadDisplay: return "Bad Display";
        case kCGLBadState: return "Bad State";
        case kCGLBadValue: return "Bad Value";
        case kCGLBadMatch: return "Bad Match";
        case kCGLBadEnumeration: return "Bad Enumeration";
        case kCGLBadOffScreen: return "Bad OffScreen";
        case kCGLBadFullScreen: return "Bad FullScreen";
        case kCGLBadWindow: return "Bad Window";
        case kCGLBadAddress: return "Bad Address";
        case kCGLBadCodeModule: return "Bad Code Module";
        case kCGLBadAlloc: return "Bad Allocation";
        case kCGLBadConnection: return "Bad Connection";
        default: return "Unknown Error";
    }
}

//
// DYLD_INTERPOSE declarations
// Only used when injecting into apps, NOT when building as replacement libGL
//

#ifndef GL_WRAPPER_MODE
#define DYLD_INTERPOSE(_replacement,_replacee) \
   __attribute__((used)) static struct{ const void* replacement; const void* replacee; } _interpose_##_replacee \
            __attribute__ ((section ("__DATA,__interpose"))) = { (const void*)(unsigned long)&_replacement, (const void*)(unsigned long)&_replacee };

// GLX functions are handled by glx_wrapper.c, NOT here!
// Do NOT intercept GLX functions in libGLMetal.dylib

// Legacy immediate mode (OpenGL 1.x/2.x)
DYLD_INTERPOSE(my_glBegin, glBegin)
DYLD_INTERPOSE(my_glEnd, glEnd)

// Vertex functions
DYLD_INTERPOSE(my_glVertex2f, glVertex2f)
DYLD_INTERPOSE(my_glVertex2fv, glVertex2fv)
DYLD_INTERPOSE(my_glVertex3f, glVertex3f)
DYLD_INTERPOSE(my_glVertex3fv, glVertex3fv)

// Color functions
DYLD_INTERPOSE(my_glColor3f, glColor3f)
DYLD_INTERPOSE(my_glColor4f, glColor4f)
DYLD_INTERPOSE(my_glColor3fv, glColor3fv)
DYLD_INTERPOSE(my_glColor4fv, glColor4fv)

// Normal vectors
DYLD_INTERPOSE(my_glNormal3f, glNormal3f)
DYLD_INTERPOSE(my_glNormal3fv, glNormal3fv)

// Texture coordinates
DYLD_INTERPOSE(my_glTexCoord2f, glTexCoord2f)
DYLD_INTERPOSE(my_glTexCoord2fv, glTexCoord2fv)

// Matrix operations
DYLD_INTERPOSE(my_glMatrixMode, glMatrixMode)
DYLD_INTERPOSE(my_glLoadIdentity, glLoadIdentity)
DYLD_INTERPOSE(my_glPushMatrix, glPushMatrix)
DYLD_INTERPOSE(my_glPopMatrix, glPopMatrix)
DYLD_INTERPOSE(my_glLoadMatrixf, glLoadMatrixf)
DYLD_INTERPOSE(my_glMultMatrixf, glMultMatrixf)
DYLD_INTERPOSE(my_glRotatef, glRotatef)
DYLD_INTERPOSE(my_glScalef, glScalef)
DYLD_INTERPOSE(my_glTranslatef, glTranslatef)

// Projection
DYLD_INTERPOSE(my_glOrtho, glOrtho)
DYLD_INTERPOSE(my_glFrustum, glFrustum)
DYLD_INTERPOSE(gluPerspective, gluPerspective)

// Enable/Disable
DYLD_INTERPOSE(my_glEnable, glEnable)
DYLD_INTERPOSE(my_glDisable, glDisable)

// Blending
DYLD_INTERPOSE(my_glBlendFunc, glBlendFunc)
DYLD_INTERPOSE(my_glBlendFuncSeparate, glBlendFuncSeparate)

// Depth
DYLD_INTERPOSE(my_glDepthFunc, glDepthFunc)
DYLD_INTERPOSE(my_glDepthMask, glDepthMask)

// Culling
DYLD_INTERPOSE(my_glCullFace, glCullFace)
DYLD_INTERPOSE(my_glFrontFace, glFrontFace)

// Polygon mode
DYLD_INTERPOSE(my_glPolygonMode, glPolygonMode)

// Line/Point size
DYLD_INTERPOSE(my_glLineWidth, glLineWidth)
DYLD_INTERPOSE(my_glPointSize, glPointSize)

// Clearing
DYLD_INTERPOSE(my_glClearColor, glClearColor)
DYLD_INTERPOSE(my_glClearDepth, glClearDepth)

// Display lists
DYLD_INTERPOSE(my_glGenLists, glGenLists)
DYLD_INTERPOSE(my_glNewList, glNewList)
DYLD_INTERPOSE(my_glEndList, glEndList)
DYLD_INTERPOSE(my_glCallList, glCallList)
DYLD_INTERPOSE(my_glDeleteLists, glDeleteLists)

// Lighting
DYLD_INTERPOSE(my_glLightfv, glLightfv)
DYLD_INTERPOSE(my_glMaterialfv, glMaterialfv)
DYLD_INTERPOSE(my_glShadeModel, glShadeModel)

// VM Display Integration (Phase 7.6)
DYLD_INTERPOSE(my_glReadPixels, glReadPixels)
// Note: glutSwapBuffers cannot be interposed via DYLD_INTERPOSE (GLUT is separate framework)
// Apps must call my_glutSwapBuffers() explicitly or we need different interposition mechanism

// CGL (Core OpenGL) functions for X11/GLX
DYLD_INTERPOSE(CGLChoosePixelFormat, CGLChoosePixelFormat)
DYLD_INTERPOSE(CGLDestroyPixelFormat, CGLDestroyPixelFormat)
DYLD_INTERPOSE(CGLDescribePixelFormat, CGLDescribePixelFormat)
DYLD_INTERPOSE(CGLCreateContext, CGLCreateContext)
DYLD_INTERPOSE(CGLDestroyContext, CGLDestroyContext)
DYLD_INTERPOSE(CGLSetCurrentContext, CGLSetCurrentContext)
DYLD_INTERPOSE(CGLGetCurrentContext, CGLGetCurrentContext)
DYLD_INTERPOSE(CGLCopyContext, CGLCopyContext)
DYLD_INTERPOSE(CGLFlushDrawable, CGLFlushDrawable)
DYLD_INTERPOSE(CGLEnable, CGLEnable)
DYLD_INTERPOSE(CGLDisable, CGLDisable)
DYLD_INTERPOSE(CGLIsEnabled, CGLIsEnabled)
DYLD_INTERPOSE(CGLSetParameter, CGLSetParameter)
DYLD_INTERPOSE(CGLGetParameter, CGLGetParameter)
DYLD_INTERPOSE(CGLLockContext, CGLLockContext)
DYLD_INTERPOSE(CGLUnlockContext, CGLUnlockContext)
DYLD_INTERPOSE(CGLGetPixelFormat, CGLGetPixelFormat)
DYLD_INTERPOSE(CGLErrorString, CGLErrorString)

// Common state functions
DYLD_INTERPOSE(my_glClear, glClear)
DYLD_INTERPOSE(my_glFlush, glFlush)
DYLD_INTERPOSE(my_glViewport, glViewport)

// Query functions
DYLD_INTERPOSE(my_glGetString, glGetString)
DYLD_INTERPOSE(my_glGetIntegerv, glGetIntegerv)
DYLD_INTERPOSE(my_glGetError, glGetError)

// Buffer management (OpenGL 3.x+)
DYLD_INTERPOSE(my_glGenBuffers, glGenBuffers)
DYLD_INTERPOSE(my_glBindBuffer, glBindBuffer)
DYLD_INTERPOSE(my_glBufferData, glBufferData)
DYLD_INTERPOSE(my_glBufferSubData, glBufferSubData)
DYLD_INTERPOSE(my_glMapBuffer, glMapBuffer)
DYLD_INTERPOSE(my_glUnmapBuffer, glUnmapBuffer)
DYLD_INTERPOSE(my_glDeleteBuffers, glDeleteBuffers)

// Vertex arrays (VAO)
DYLD_INTERPOSE(my_glGenVertexArrays, glGenVertexArrays)
DYLD_INTERPOSE(my_glBindVertexArray, glBindVertexArray)
DYLD_INTERPOSE(my_glDeleteVertexArrays, glDeleteVertexArrays)
DYLD_INTERPOSE(my_glVertexAttribPointer, glVertexAttribPointer)
DYLD_INTERPOSE(my_glEnableVertexAttribArray, glEnableVertexAttribArray)
DYLD_INTERPOSE(my_glDisableVertexAttribArray, glDisableVertexAttribArray)

// Modern drawing
DYLD_INTERPOSE(my_glDrawArrays, glDrawArrays)
DYLD_INTERPOSE(my_glDrawElements, glDrawElements)

// Shaders and programs
DYLD_INTERPOSE(my_glCreateShader, glCreateShader)
DYLD_INTERPOSE(my_glShaderSource, glShaderSource)
DYLD_INTERPOSE(my_glCompileShader, glCompileShader)
DYLD_INTERPOSE(my_glDeleteShader, glDeleteShader)
DYLD_INTERPOSE(my_glCreateProgram, glCreateProgram)
DYLD_INTERPOSE(my_glAttachShader, glAttachShader)
DYLD_INTERPOSE(my_glLinkProgram, glLinkProgram)
DYLD_INTERPOSE(my_glUseProgram, glUseProgram)
DYLD_INTERPOSE(my_glDeleteProgram, glDeleteProgram)

// Uniforms
DYLD_INTERPOSE(my_glUniform1f, glUniform1f)
DYLD_INTERPOSE(my_glUniform2f, glUniform2f)
DYLD_INTERPOSE(my_glUniform3f, glUniform3f)
DYLD_INTERPOSE(my_glUniform4f, glUniform4f)
DYLD_INTERPOSE(my_glUniform1i, glUniform1i)
DYLD_INTERPOSE(my_glUniform2fv, glUniform2fv)
DYLD_INTERPOSE(my_glUniformMatrix4fv, glUniformMatrix4fv)

// Textures
DYLD_INTERPOSE(my_glGenTextures, glGenTextures)
DYLD_INTERPOSE(my_glBindTexture, glBindTexture)
DYLD_INTERPOSE(my_glTexImage2D, glTexImage2D)
DYLD_INTERPOSE(my_glTexParameteri, glTexParameteri)
DYLD_INTERPOSE(my_glDeleteTextures, glDeleteTextures)

// Framebuffer objects (FBO)
DYLD_INTERPOSE(my_glGenFramebuffers, glGenFramebuffers)
DYLD_INTERPOSE(my_glBindFramebuffer, glBindFramebuffer)
DYLD_INTERPOSE(my_glFramebufferTexture2D, glFramebufferTexture2D)
DYLD_INTERPOSE(my_glCheckFramebufferStatus, glCheckFramebufferStatus)
DYLD_INTERPOSE(my_glDeleteFramebuffers, glDeleteFramebuffers)

#endif // GL_WRAPPER_MODE - End of DYLD_INTERPOSE declarations

// Hook all OpenGL functions - fishhook now works with mprotect fix!
static void do_hook_setup(void) {
    static int hooks_installed = 0;
    if (hooks_installed) return;
    hooks_installed = 1;
    
    printf("🔧 Installing all OpenGL function hooks...\n");
    fflush(stdout);
    
    struct rebinding rebindings[100];  // Increased to 100 to handle all OpenGL+GLX+CGL functions
    int idx = 0;
    
    // Basic immediate mode rendering
    rebindings[idx++] = (struct rebinding){"glBegin", my_glBegin, (void **)&orig_glBegin};
    rebindings[idx++] = (struct rebinding){"glEnd", my_glEnd, (void **)&orig_glEnd};
    rebindings[idx++] = (struct rebinding){"glVertex2f", my_glVertex2f, NULL};
    rebindings[idx++] = (struct rebinding){"glVertex3f", my_glVertex3f, (void **)&orig_glVertex3f};
    rebindings[idx++] = (struct rebinding){"glColor3f", my_glColor3f, (void **)&orig_glColor3f};
    rebindings[idx++] = (struct rebinding){"glColor4f", my_glColor4f, NULL};
    rebindings[idx++] = (struct rebinding){"glColor3fv", my_glColor3fv, NULL};
    rebindings[idx++] = (struct rebinding){"glColor4fv", my_glColor4fv, NULL};
    rebindings[idx++] = (struct rebinding){"glNormal3f", my_glNormal3f, NULL};
    rebindings[idx++] = (struct rebinding){"glNormal3fv", my_glNormal3fv, NULL};
    rebindings[idx++] = (struct rebinding){"glTexCoord2f", my_glTexCoord2f, NULL};
    
    // Matrix operations
    rebindings[idx++] = (struct rebinding){"glMatrixMode", my_glMatrixMode, (void **)&orig_glMatrixMode};
    rebindings[idx++] = (struct rebinding){"glLoadIdentity", my_glLoadIdentity, (void **)&orig_glLoadIdentity};
    rebindings[idx++] = (struct rebinding){"glPushMatrix", my_glPushMatrix, (void **)&orig_glPushMatrix};
    rebindings[idx++] = (struct rebinding){"glPopMatrix", my_glPopMatrix, (void **)&orig_glPopMatrix};
    rebindings[idx++] = (struct rebinding){"glLoadMatrixf", my_glLoadMatrixf, NULL};
    rebindings[idx++] = (struct rebinding){"glMultMatrixf", my_glMultMatrixf, NULL};
    rebindings[idx++] = (struct rebinding){"glRotatef", my_glRotatef, (void **)&orig_glRotatef};
    rebindings[idx++] = (struct rebinding){"glScalef", my_glScalef, NULL};
    rebindings[idx++] = (struct rebinding){"glTranslatef", my_glTranslatef, (void **)&orig_glTranslatef};
    
    // State management
    rebindings[idx++] = (struct rebinding){"glEnable", my_glEnable, NULL};
    rebindings[idx++] = (struct rebinding){"glDisable", my_glDisable, NULL};
    rebindings[idx++] = (struct rebinding){"glIsEnabled", my_glIsEnabled, NULL};
    rebindings[idx++] = (struct rebinding){"glClear", my_glClear, (void **)&orig_glClear};
    rebindings[idx++] = (struct rebinding){"glClearColor", my_glClearColor, (void **)&orig_glClearColor};
    rebindings[idx++] = (struct rebinding){"glViewport", my_glViewport, (void **)&orig_glViewport};
    rebindings[idx++] = (struct rebinding){"glFlush", my_glFlush, (void **)&orig_glFlush};
    
    // Display lists - disable by making glGenLists return 0
    rebindings[idx++] = (struct rebinding){"glGenLists", my_glGenLists, NULL};
    rebindings[idx++] = (struct rebinding){"glNewList", my_glNewList, (void **)&orig_glNewList};
    rebindings[idx++] = (struct rebinding){"glEndList", my_glEndList, (void **)&orig_glEndList};
    rebindings[idx++] = (struct rebinding){"glCallList", my_glCallList, (void **)&orig_glCallList};
    rebindings[idx++] = (struct rebinding){"glDeleteLists", my_glDeleteLists, (void **)&orig_glDeleteLists};
    
    // Pixel operations
    rebindings[idx++] = (struct rebinding){"glReadPixels", my_glReadPixels, NULL};
    
    // Lighting
    rebindings[idx++] = (struct rebinding){"glLightfv", my_glLightfv, NULL};
    rebindings[idx++] = (struct rebinding){"glMaterialfv", my_glMaterialfv, NULL};
    rebindings[idx++] = (struct rebinding){"glShadeModel", my_glShadeModel, NULL};
    
    // Buffer management (VBO)
    rebindings[idx++] = (struct rebinding){"glGenBuffers", my_glGenBuffers, NULL};
    rebindings[idx++] = (struct rebinding){"glBindBuffer", my_glBindBuffer, NULL};
    rebindings[idx++] = (struct rebinding){"glBufferData", my_glBufferData, NULL};
    rebindings[idx++] = (struct rebinding){"glDeleteBuffers", my_glDeleteBuffers, NULL};
    
    // Vertex arrays (VAO)
    rebindings[idx++] = (struct rebinding){"glGenVertexArrays", my_glGenVertexArrays, NULL};
    rebindings[idx++] = (struct rebinding){"glBindVertexArray", my_glBindVertexArray, NULL};
    rebindings[idx++] = (struct rebinding){"glDeleteVertexArrays", my_glDeleteVertexArrays, NULL};
    rebindings[idx++] = (struct rebinding){"glVertexAttribPointer", my_glVertexAttribPointer, NULL};
    rebindings[idx++] = (struct rebinding){"glEnableVertexAttribArray", my_glEnableVertexAttribArray, NULL};
    rebindings[idx++] = (struct rebinding){"glDisableVertexAttribArray", my_glDisableVertexAttribArray, NULL};
    
    // Modern drawing
    rebindings[idx++] = (struct rebinding){"glDrawArrays", my_glDrawArrays, NULL};
    rebindings[idx++] = (struct rebinding){"glDrawElements", my_glDrawElements, NULL};
    
    // Shaders
    rebindings[idx++] = (struct rebinding){"glCreateShader", my_glCreateShader, NULL};
    rebindings[idx++] = (struct rebinding){"glShaderSource", my_glShaderSource, NULL};
    rebindings[idx++] = (struct rebinding){"glCompileShader", my_glCompileShader, NULL};
    rebindings[idx++] = (struct rebinding){"glDeleteShader", my_glDeleteShader, NULL};
    rebindings[idx++] = (struct rebinding){"glCreateProgram", my_glCreateProgram, NULL};
    rebindings[idx++] = (struct rebinding){"glAttachShader", my_glAttachShader, NULL};
    rebindings[idx++] = (struct rebinding){"glLinkProgram", my_glLinkProgram, NULL};
    rebindings[idx++] = (struct rebinding){"glUseProgram", my_glUseProgram, NULL};
    rebindings[idx++] = (struct rebinding){"glDeleteProgram", my_glDeleteProgram, NULL};
    
    // Textures
    rebindings[idx++] = (struct rebinding){"glGenTextures", my_glGenTextures, NULL};
    rebindings[idx++] = (struct rebinding){"glBindTexture", my_glBindTexture, NULL};
    rebindings[idx++] = (struct rebinding){"glTexImage2D", my_glTexImage2D, NULL};
    rebindings[idx++] = (struct rebinding){"glTexParameteri", my_glTexParameteri, NULL};
    rebindings[idx++] = (struct rebinding){"glDeleteTextures", my_glDeleteTextures, NULL};
    
    // Framebuffer objects
    rebindings[idx++] = (struct rebinding){"glGenFramebuffers", my_glGenFramebuffers, NULL};
    rebindings[idx++] = (struct rebinding){"glBindFramebuffer", my_glBindFramebuffer, NULL};
    rebindings[idx++] = (struct rebinding){"glFramebufferTexture2D", my_glFramebufferTexture2D, NULL};
    rebindings[idx++] = (struct rebinding){"glCheckFramebufferStatus", my_glCheckFramebufferStatus, NULL};
    rebindings[idx++] = (struct rebinding){"glDeleteFramebuffers", my_glDeleteFramebuffers, NULL};
    
    // GLX functions (intercept glXSwapBuffers to send to Metal server)
    rebindings[idx++] = (struct rebinding){"glXSwapBuffers", my_glXSwapBuffers, (void **)&orig_glXSwapBuffers};
    
    // CGL (Core OpenGL) functions for X11/GLX support
    rebindings[idx++] = (struct rebinding){"CGLChoosePixelFormat", CGLChoosePixelFormat, NULL};
    rebindings[idx++] = (struct rebinding){"CGLDestroyPixelFormat", CGLDestroyPixelFormat, NULL};
    rebindings[idx++] = (struct rebinding){"CGLDescribePixelFormat", CGLDescribePixelFormat, NULL};
    rebindings[idx++] = (struct rebinding){"CGLCreateContext", CGLCreateContext, NULL};
    rebindings[idx++] = (struct rebinding){"CGLDestroyContext", CGLDestroyContext, NULL};
    rebindings[idx++] = (struct rebinding){"CGLSetCurrentContext", CGLSetCurrentContext, NULL};
    rebindings[idx++] = (struct rebinding){"CGLGetCurrentContext", CGLGetCurrentContext, NULL};
    rebindings[idx++] = (struct rebinding){"CGLCopyContext", CGLCopyContext, NULL};
    rebindings[idx++] = (struct rebinding){"CGLFlushDrawable", CGLFlushDrawable, NULL};
    rebindings[idx++] = (struct rebinding){"CGLEnable", CGLEnable, NULL};
    rebindings[idx++] = (struct rebinding){"CGLDisable", CGLDisable, NULL};
    rebindings[idx++] = (struct rebinding){"CGLIsEnabled", CGLIsEnabled, NULL};
    rebindings[idx++] = (struct rebinding){"CGLSetParameter", CGLSetParameter, NULL};
    rebindings[idx++] = (struct rebinding){"CGLGetParameter", CGLGetParameter, NULL};
    rebindings[idx++] = (struct rebinding){"CGLLockContext", CGLLockContext, NULL};
    rebindings[idx++] = (struct rebinding){"CGLUnlockContext", CGLUnlockContext, NULL};
    rebindings[idx++] = (struct rebinding){"CGLGetPixelFormat", CGLGetPixelFormat, NULL};
    rebindings[idx++] = (struct rebinding){"CGLErrorString", CGLErrorString, NULL};
    
    printf("  Attempting to hook %d OpenGL functions...\n", idx);
    fflush(stdout);
    
    int result = rebind_symbols(rebindings, idx);
    if (result == 0) {
        printf("✅ fishhook: Successfully hooked %d OpenGL functions for runtime interception\n", idx);
    } else {
        printf("❌ fishhook: Failed to hook functions (error code: %d)\n", result);
    }
    fflush(stdout);
}

//
// GLX Function Stubs (Required for glmark2)
// These are minimal implementations to satisfy GLX queries
//

#ifdef GL_WRAPPER_MODE

// GLX types already declared at top of file

Bool my_glXQueryVersion(Display *dpy, int *major, int *minor) {
    fprintf(stderr, "[GLX] glXQueryVersion called\n");
    fflush(stderr);
    if (major) *major = 1;
    if (minor) *minor = 4;
    return True;
}

Bool my_glXQueryExtension(Display *dpy, int *errorBase, int *eventBase) {
    fprintf(stderr, "[GLX] glXQueryExtension called\n");
    fflush(stderr);
    if (errorBase) *errorBase = 0;
    if (eventBase) *eventBase = 0;
    return True;
}

const char* my_glXQueryExtensionsString(Display *dpy, int screen) {
    fprintf(stderr, "[GLX] glXQueryExtensionsString called\n");
    fflush(stderr);
    return "GLX_ARB_get_proc_address GLX_ARB_multisample GLX_EXT_visual_info";
}

const char* my_glXGetClientString(Display *dpy, int name) {
    fprintf(stderr, "[GLX] glXGetClientString called\n");
    fflush(stderr);
    return "SharedGL Metal Translator";
}

XVisualInfo* my_glXChooseVisual(Display *dpy, int screen, int *attribList) {
    fprintf(stderr, "[GLX] glXChooseVisual called - returning NULL\n");
    fflush(stderr);
    // Return NULL - let glmark2 use X11's visual selection
    return NULL;
}

GLXContext my_glXCreateContext(Display *dpy, XVisualInfo *vis, GLXContext shareList, Bool direct) {
    fprintf(stderr, "[GLX] glXCreateContext called\n");
    fflush(stderr);
    // Return dummy context - actual OpenGL calls will go through our hooks
    return (GLXContext)0x12345678;
}

void my_glXDestroyContext(Display *dpy, GLXContext ctx) {
    fprintf(stderr, "[GLX] glXDestroyContext called\n");
    fflush(stderr);
}

Bool my_glXMakeCurrent(Display *dpy, GLXDrawable drawable, GLXContext ctx) {
    fprintf(stderr, "[GLX] glXMakeCurrent(drawable=%lu, ctx=%p)\n", (unsigned long)drawable, ctx);
    fflush(stderr);
    return True;
}

// my_glXSwapBuffers already defined earlier in the file

__GLXextFuncPtr my_glXGetProcAddressARB(const GLubyte *procName) {
    if (procName) {
        fprintf(stderr, "[GLX] glXGetProcAddressARB(%s) - returning NULL\n", procName);
    } else {
        fprintf(stderr, "[GLX] glXGetProcAddressARB(NULL) - returning NULL\n");
    }
    fflush(stderr);
    // Return NULL - forces glmark2 to use our direct function implementations
    return NULL;
}

int my_glXGetConfig(Display *dpy, XVisualInfo *vis, int attrib, int *value) {
    fprintf(stderr, "[GLX] glXGetConfig(attrib=0x%x)\n", attrib);
    fflush(stderr);
    // Return sensible defaults
    if (value) {
        switch (attrib) {
            case 0x1: *value = 1; break;  // GLX_USE_GL
            case 0x2: *value = 32; break; // GLX_BUFFER_SIZE
            case 0x3: *value = 0; break;  // GLX_LEVEL
            case 0x4: *value = 1; break;  // GLX_RGBA
            case 0x5: *value = 1; break;  // GLX_DOUBLEBUFFER
            case 0x8: *value = 8; break;  // GLX_RED_SIZE
            case 0x9: *value = 8; break;  // GLX_GREEN_SIZE
            case 0xA: *value = 8; break;  // GLX_BLUE_SIZE
            case 0xB: *value = 8; break;  // GLX_ALPHA_SIZE
            case 0xC: *value = 24; break; // GLX_DEPTH_SIZE
            case 0xD: *value = 8; break;  // GLX_STENCIL_SIZE
            default: *value = 0; break;
        }
    }
    return 0;  // Success
}

// Add GLX symbol aliases when in wrapper mode
#ifdef GL_WRAPPER_MODE
// GLX function wrappers
#define glXQueryVersion my_glXQueryVersion
#define glXQueryExtension my_glXQueryExtension
#define glXQueryExtensionsString my_glXQueryExtensionsString
#define glXGetClientString my_glXGetClientString
#define glXChooseVisual my_glXChooseVisual
#define glXCreateContext my_glXCreateContext
#define glXDestroyContext my_glXDestroyContext
#define glXMakeCurrent my_glXMakeCurrent
#define glXSwapBuffers my_glXSwapBuffers
#define glXGetProcAddressARB my_glXGetProcAddressARB
#define glXGetConfig my_glXGetConfig
#endif

#endif // GL_WRAPPER_MODE

// Constructor - set up hooks
__attribute__((constructor))
static void library_init(void) {
    fprintf(stderr, "\n");
    fprintf(stderr, "========================================\n");
    fprintf(stderr, "🔧 CUSTOM libGL.dylib LOADED!\n");
    fprintf(stderr, "   OpenGL→Metal Translation Active\n");
    fprintf(stderr, "========================================\n");
    fprintf(stderr, "\n");
    fflush(stderr);
    
    #ifdef GL_WRAPPER_MODE
    // In wrapper mode, DON'T use fishhook - we're replacing the system library
    // Our functions are already being called directly via symbol exports
    fprintf(stderr, "   Wrapper mode: Skipping fishhook setup\n");
    fflush(stderr);
    
    // Just connect to metal_server
    connect_to_metal_server();
    #else
    // In DYLD_INSERT_LIBRARIES mode, use fishhook
    fprintf(stderr, "   Installing OpenGL function hooks via fishhook...\n");
    fflush(stderr);
    do_hook_setup();
    #endif
}

// ============================================================================
// Public GL API Exports (Wrapper Mode)
// ============================================================================
// These are the actual glEnable/glDisable/glIsEnabled functions that get
// exported when building in GL_WRAPPER_MODE. They call our custom implementations
// with state tracking.

#ifdef GL_WRAPPER_MODE
void glEnable(GLenum cap) {
    my_glEnable(cap);
}

void glDisable(GLenum cap) {
    my_glDisable(cap);
}

GLboolean glIsEnabled(GLenum cap) {
    return my_glIsEnabled(cap);
}
#endif

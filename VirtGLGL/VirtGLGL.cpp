/*
 * VirtGLGL.cpp
 * Userspace OpenGL implementation for VirtIO GPU
 */

#include "VirtGLGL.h"
#include "VirtGLGLClient.h"
#include "virgl_protocol.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// Global state
static VirtGLGLClientRef g_client = NULL;
static uint32_t g_contextId = 0;
static uint32_t g_resourceId = 0;
static bool g_initialized = false;

// Current OpenGL state
static GLenum g_primitiveMode = GL_POINTS;
static bool g_inBeginEnd = false;

// Vertex buffer (simple immediate mode)
#define MAX_VERTICES 65536
static float g_vertices[MAX_VERTICES * 4];  // x, y, z, w
static uint32_t g_vertexCount = 0;

// Current color
static float g_currentColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

// Initialize VirtGLGL
bool VirtGLGL_Initialize(void)
{
    if (g_initialized) {
        return true;
    }
    
    printf("VirtGLGL: Initializing userspace OpenGL library...\n");
    
    // Connect to kernel driver
    g_client = VirtGLGL_Connect();
    if (!g_client) {
        fprintf(stderr, "VirtGLGL: Failed to connect to kernel driver\n");
        return false;
    }
    
    // Create 3D context
    g_contextId = 1;
    if (!VirtGLGL_CreateContext(g_client, g_contextId)) {
        fprintf(stderr, "VirtGLGL: Failed to create 3D context\n");
        VirtGLGL_Disconnect(g_client);
        g_client = NULL;
        return false;
    }
    
    // Create render target resource (800x600 RGBA)
    // Use resource ID 2 to avoid conflict with framebuffer's resource ID 1
    g_resourceId = 2;
    if (!VirtGLGL_CreateResource(g_client, g_resourceId, 800, 600, 
                                  VIRGL_FORMAT_R8G8B8A8_UNORM)) {
        fprintf(stderr, "VirtGLGL: Failed to create resource\n");
        VirtGLGL_Disconnect(g_client);
        g_client = NULL;
        return false;
    }
    
    // Attach resource to context
    if (!VirtGLGL_AttachResource(g_client, g_contextId, g_resourceId)) {
        fprintf(stderr, "VirtGLGL: Failed to attach resource to context\n");
        VirtGLGL_Disconnect(g_client);
        g_client = NULL;
        return false;
    }
    
    g_initialized = true;
    printf("VirtGLGL: Initialization complete (context=%u, resource=%u)\n", 
           g_contextId, g_resourceId);
    
    return true;
}

void VirtGLGL_Shutdown(void)
{
    if (!g_initialized) return;
    
    if (g_client) {
        VirtGLGL_Disconnect(g_client);
        g_client = NULL;
    }
    
    g_initialized = false;
    printf("VirtGLGL: Shutdown complete\n");
}

void* VirtGLGL_GetClient(void)
{
    return g_client;
}

// OpenGL API implementations

void glClear(GLbitfield mask)
{
    if (!g_initialized) {
        VirtGLGL_Initialize();
    }
    
    if (!g_client) return;
    
    // Build VIRGL_CCMD_CLEAR command
    uint32_t cmd[8];
    cmd[0] = virgl_cmd_header_pack(VIRGL_CCMD_CLEAR, 7);
    
    // Map GL clear bits to virgl clear bits
    uint32_t virgl_mask = 0;
    if (mask & GL_COLOR_BUFFER_BIT) virgl_mask |= PIPE_CLEAR_COLOR0;
    if (mask & GL_DEPTH_BUFFER_BIT) virgl_mask |= PIPE_CLEAR_DEPTH;
    if (mask & GL_STENCIL_BUFFER_BIT) virgl_mask |= PIPE_CLEAR_STENCIL;
    
    cmd[1] = virgl_mask;
    cmd[2] = virgl_pack_float(g_currentColor[0]);  // red
    cmd[3] = virgl_pack_float(g_currentColor[1]);  // green
    cmd[4] = virgl_pack_float(g_currentColor[2]);  // blue
    cmd[5] = virgl_pack_float(g_currentColor[3]);  // alpha
    cmd[6] = virgl_pack_float(1.0f);               // depth
    cmd[7] = 0;                                    // stencil
    
    VirtGLGL_SubmitCommands(g_client, cmd, sizeof(cmd));
    
    printf("VirtGLGL: glClear(0x%x) -> virgl mask 0x%x\n", mask, virgl_mask);
    
    // Set scanout to use our 3D resource, then flush to display
    if (g_resourceId > 0) {
        // Scanout 0 = primary display
        VirtGLGL_SetScanout(g_client, 0, g_resourceId, 0, 0, 800, 600);
        VirtGLGL_FlushResource(g_client, g_resourceId, 0, 0, 800, 600);
    }
}

void glClearColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha)
{
    g_currentColor[0] = red;
    g_currentColor[1] = green;
    g_currentColor[2] = blue;
    g_currentColor[3] = alpha;
    
    printf("VirtGLGL: glClearColor(%f, %f, %f, %f)\n", red, green, blue, alpha);
}

void glBegin(GLenum mode)
{
    if (g_inBeginEnd) {
        fprintf(stderr, "VirtGLGL: glBegin called inside glBegin/glEnd\n");
        return;
    }
    
    g_primitiveMode = mode;
    g_vertexCount = 0;
    g_inBeginEnd = true;
    
    printf("VirtGLGL: glBegin(mode=%u)\n", mode);
}

void glEnd(void)
{
    if (!g_inBeginEnd) {
        fprintf(stderr, "VirtGLGL: glEnd called outside glBegin/glEnd\n");
        return;
    }
    
    printf("VirtGLGL: glEnd() - %u vertices\n", g_vertexCount);
    
    if (g_vertexCount > 0 && g_client) {
        // Submit immediate mode vertices as virgl DRAW_VBO command
        // For now, we'll just log it until we implement full virgl vertex buffer support
        printf("VirtGLGL: Drawing %u vertices in mode %u (primitive type)\n", 
               g_vertexCount, g_primitiveMode);
        
        // TODO Phase 2: Implement full virgl vertex buffer protocol:
        // 1. Create vertex buffer resource with VirtGLGL_CreateResource
        // 2. Upload vertex data via VIRGL_CCMD_RESOURCE_INLINE_WRITE
        // 3. Set vertex buffer binding with VIRGL_CCMD_SET_VERTEX_BUFFERS
        // 4. Submit VIRGL_CCMD_DRAW_VBO command
        //
        // For Phase 1, we have working glClear() which proves the pipeline works
    }
    
    g_inBeginEnd = false;
    g_vertexCount = 0;
}

void glVertex2f(GLfloat x, GLfloat y)
{
    glVertex3f(x, y, 0.0f);
}

void glVertex3f(GLfloat x, GLfloat y, GLfloat z)
{
    if (!g_inBeginEnd) {
        fprintf(stderr, "VirtGLGL: glVertex3f called outside glBegin/glEnd\n");
        return;
    }
    
    if (g_vertexCount >= MAX_VERTICES) {
        fprintf(stderr, "VirtGLGL: Vertex buffer overflow\n");
        return;
    }
    
    uint32_t idx = g_vertexCount * 4;
    g_vertices[idx + 0] = x;
    g_vertices[idx + 1] = y;
    g_vertices[idx + 2] = z;
    g_vertices[idx + 3] = 1.0f;
    
    g_vertexCount++;
}

void glColor3f(GLfloat red, GLfloat green, GLfloat blue)
{
    glColor4f(red, green, blue, 1.0f);
}

void glColor4f(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
{
    g_currentColor[0] = red;
    g_currentColor[1] = green;
    g_currentColor[2] = blue;
    g_currentColor[3] = alpha;
}

void glFlush(void)
{
    // Flush any pending commands
    printf("VirtGLGL: glFlush()\n");
}

void glFinish(void)
{
    // Wait for all commands to complete
    printf("VirtGLGL: glFinish()\n");
}

GLenum glGetError(void)
{
    // For now, always return no error
    return GL_NO_ERROR;
}

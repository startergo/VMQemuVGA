/*
 * VirtGLGL.h
 * Userspace OpenGL API for VirtIO GPU
 */

#ifndef VIRTGLGL_H
#define VIRTGLGL_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// OpenGL types
typedef unsigned int GLenum;
typedef unsigned char GLboolean;
typedef unsigned int GLbitfield;
typedef signed char GLbyte;
typedef short GLshort;
typedef int GLint;
typedef int GLsizei;
typedef unsigned char GLubyte;
typedef unsigned short GLushort;
typedef unsigned int GLuint;
typedef float GLfloat;
typedef float GLclampf;
typedef double GLdouble;
typedef double GLclampd;
typedef void GLvoid;

// OpenGL constants
#define GL_FALSE                          0
#define GL_TRUE                           1

// BeginMode
#define GL_POINTS                         0x0000
#define GL_LINES                          0x0001
#define GL_LINE_LOOP                      0x0002
#define GL_LINE_STRIP                     0x0003
#define GL_TRIANGLES                      0x0004
#define GL_TRIANGLE_STRIP                 0x0005
#define GL_TRIANGLE_FAN                   0x0006
#define GL_QUADS                          0x0007
#define GL_QUAD_STRIP                     0x0008
#define GL_POLYGON                        0x0009

// ClearBufferMask
#define GL_DEPTH_BUFFER_BIT               0x00000100
#define GL_STENCIL_BUFFER_BIT             0x00000400
#define GL_COLOR_BUFFER_BIT               0x00004000

// ErrorCode
#define GL_NO_ERROR                       0
#define GL_INVALID_ENUM                   0x0500
#define GL_INVALID_VALUE                  0x0501
#define GL_INVALID_OPERATION              0x0502
#define GL_STACK_OVERFLOW                 0x0503
#define GL_STACK_UNDERFLOW                0x0504
#define GL_OUT_OF_MEMORY                  0x0505

// VirtGLGL initialization
bool VirtGLGL_Initialize(void);
void VirtGLGL_Shutdown(void);

// Direct access to client (for testing)
void* VirtGLGL_GetClient(void);

// OpenGL API (subset)
void glClear(GLbitfield mask);
void glClearColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
void glBegin(GLenum mode);
void glEnd(void);
void glVertex2f(GLfloat x, GLfloat y);
void glVertex3f(GLfloat x, GLfloat y, GLfloat z);
void glColor3f(GLfloat red, GLfloat green, GLfloat blue);
void glColor4f(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
void glFlush(void);
void glFinish(void);
GLenum glGetError(void);

#ifdef __cplusplus
}
#endif

#endif /* VIRTGLGL_H */

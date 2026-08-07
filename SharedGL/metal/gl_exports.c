//
//  OpenGL Symbol Exports
//  Provides proper glXXX symbols that glmark2 expects when it calls dlopen("libGL.dylib")
//

#include <OpenGL/gl.h>
#include <OpenGL/gl3.h>

// Forward declare our implementations (from gl_to_metal_client.c)
extern void my_glClear(GLbitfield mask);
extern void my_glClearColor(GLclampf r, GLclampf g, GLclampf b, GLclampf a);
extern void my_glViewport(GLint x, GLint y, GLsizei width, GLsizei height);
extern void my_glEnable(GLenum cap);
extern void my_glDisable(GLenum cap);
extern void my_glDepthFunc(GLenum func);
extern void my_glBlendFunc(GLenum sfactor, GLenum dfactor);
extern void my_glCullFace(GLenum mode);

// Buffer management
extern void my_glGenBuffers(GLsizei n, GLuint *buffers);
extern void my_glBindBuffer(GLenum target, GLuint buffer);
extern void my_glBufferData(GLenum target, GLsizeiptr size, const void *data, GLenum usage);
extern void my_glDeleteBuffers(GLsizei n, const GLuint *buffers);

// VAO management
extern void my_glGenVertexArrays(GLsizei n, GLuint *arrays);
extern void my_glBindVertexArray(GLuint array);
extern void my_glDeleteVertexArrays(GLsizei n, const GLuint *arrays);
extern void my_glVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer);
extern void my_glEnableVertexAttribArray(GLuint index);
extern void my_glDisableVertexAttribArray(GLuint index);

// Drawing
extern void my_glDrawArrays(GLenum mode, GLint first, GLsizei count);
extern void my_glDrawElements(GLenum mode, GLsizei count, GLenum type, const void *indices);

// Shaders
extern GLuint my_glCreateShader(GLenum shaderType);
extern void my_glShaderSource(GLuint shader, GLsizei count, const GLchar **string, const GLint *length);
extern void my_glCompileShader(GLuint shader);
extern void my_glGetShaderiv(GLuint shader, GLenum pname, GLint *params);
extern void my_glGetShaderInfoLog(GLuint shader, GLsizei maxLength, GLsizei *length, GLchar *infoLog);
extern void my_glDeleteShader(GLuint shader);

// Programs
extern GLuint my_glCreateProgram(void);
extern void my_glAttachShader(GLuint program, GLuint shader);
extern void my_glLinkProgram(GLuint program);
extern void my_glGetProgramiv(GLuint program, GLenum pname, GLint *params);
extern void my_glGetProgramInfoLog(GLuint program, GLsizei maxLength, GLsizei *length, GLchar *infoLog);
extern void my_glUseProgram(GLuint program);
extern void my_glDeleteProgram(GLuint program);

// Uniforms
extern GLint my_glGetUniformLocation(GLuint program, const GLchar *name);
extern void my_glUniform1f(GLint location, GLfloat v0);
extern void my_glUniform2f(GLint location, GLfloat v0, GLfloat v1);
extern void my_glUniform3f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
extern void my_glUniform4f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
extern void my_glUniform1i(GLint location, GLint v0);
extern void my_glUniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);

// Attributes
extern GLint my_glGetAttribLocation(GLuint program, const GLchar *name);
extern void my_glBindAttribLocation(GLuint program, GLuint index, const GLchar *name);

// Textures
extern void my_glGenTextures(GLsizei n, GLuint *textures);
extern void my_glBindTexture(GLenum target, GLuint texture);
extern void my_glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void *data);
extern void my_glTexParameteri(GLenum target, GLenum pname, GLint param);
extern void my_glDeleteTextures(GLsizei n, const GLuint *textures);
extern void my_glActiveTexture(GLenum texture);

// Framebuffers
extern void my_glGenFramebuffers(GLsizei n, GLuint *framebuffers);
extern void my_glBindFramebuffer(GLenum target, GLuint framebuffer);
extern void my_glFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
extern GLenum my_glCheckFramebufferStatus(GLenum target);
extern void my_glDeleteFramebuffers(GLsizei n, const GLuint *framebuffers);

// Query
extern const GLubyte* my_glGetString(GLenum name);
extern void my_glGetIntegerv(GLenum pname, GLint *data);
extern GLenum my_glGetError(void);

// Flush/Finish
extern void my_glFlush(void);
extern void my_glFinish(void);

// ============================================================================
// EXPORTED OPENGL SYMBOLS - These are what glmark2 will call via dlsym()
// ============================================================================

void glClear(GLbitfield mask) { my_glClear(mask); }
void glClearColor(GLclampf r, GLclampf g, GLclampf b, GLclampf a) { my_glClearColor(r, g, b, a); }
void glViewport(GLint x, GLint y, GLsizei width, GLsizei height) { my_glViewport(x, y, width, height); }
void glEnable(GLenum cap) { my_glEnable(cap); }
void glDisable(GLenum cap) { my_glDisable(cap); }
void glDepthFunc(GLenum func) { my_glDepthFunc(func); }
void glBlendFunc(GLenum sfactor, GLenum dfactor) { my_glBlendFunc(sfactor, dfactor); }
void glCullFace(GLenum mode) { my_glCullFace(mode); }

// Buffer management
void glGenBuffers(GLsizei n, GLuint *buffers) { my_glGenBuffers(n, buffers); }
void glBindBuffer(GLenum target, GLuint buffer) { my_glBindBuffer(target, buffer); }
void glBufferData(GLenum target, GLsizeiptr size, const void *data, GLenum usage) { my_glBufferData(target, size, data, usage); }
void glDeleteBuffers(GLsizei n, const GLuint *buffers) { my_glDeleteBuffers(n, buffers); }

// VAO management
void glGenVertexArrays(GLsizei n, GLuint *arrays) { my_glGenVertexArrays(n, arrays); }
void glBindVertexArray(GLuint array) { my_glBindVertexArray(array); }
void glDeleteVertexArrays(GLsizei n, const GLuint *arrays) { my_glDeleteVertexArrays(n, arrays); }
void glVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer) {
    my_glVertexAttribPointer(index, size, type, normalized, stride, pointer);
}
void glEnableVertexAttribArray(GLuint index) { my_glEnableVertexAttribArray(index); }
void glDisableVertexAttribArray(GLuint index) { my_glDisableVertexAttribArray(index); }

// Drawing
void glDrawArrays(GLenum mode, GLint first, GLsizei count) { my_glDrawArrays(mode, first, count); }
void glDrawElements(GLenum mode, GLsizei count, GLenum type, const void *indices) { my_glDrawElements(mode, count, type, indices); }

// Shaders
GLuint glCreateShader(GLenum shaderType) { return my_glCreateShader(shaderType); }
void glShaderSource(GLuint shader, GLsizei count, const GLchar **string, const GLint *length) {
    my_glShaderSource(shader, count, string, length);
}
void glCompileShader(GLuint shader) { my_glCompileShader(shader); }
void glGetShaderiv(GLuint shader, GLenum pname, GLint *params) { my_glGetShaderiv(shader, pname, params); }
void glGetShaderInfoLog(GLuint shader, GLsizei maxLength, GLsizei *length, GLchar *infoLog) {
    my_glGetShaderInfoLog(shader, maxLength, length, infoLog);
}
void glDeleteShader(GLuint shader) { my_glDeleteShader(shader); }

// Programs
GLuint glCreateProgram(void) { return my_glCreateProgram(); }
void glAttachShader(GLuint program, GLuint shader) { my_glAttachShader(program, shader); }
void glLinkProgram(GLuint program) { my_glLinkProgram(program); }
void glGetProgramiv(GLuint program, GLenum pname, GLint *params) { my_glGetProgramiv(program, pname, params); }
void glGetProgramInfoLog(GLuint program, GLsizei maxLength, GLsizei *length, GLchar *infoLog) {
    my_glGetProgramInfoLog(program, maxLength, length, infoLog);
}
void glUseProgram(GLuint program) { my_glUseProgram(program); }
void glDeleteProgram(GLuint program) { my_glDeleteProgram(program); }

// Uniforms
GLint glGetUniformLocation(GLuint program, const GLchar *name) { return my_glGetUniformLocation(program, name); }
void glUniform1f(GLint location, GLfloat v0) { my_glUniform1f(location, v0); }
void glUniform2f(GLint location, GLfloat v0, GLfloat v1) { my_glUniform2f(location, v0, v1); }
void glUniform3f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2) { my_glUniform3f(location, v0, v1, v2); }
void glUniform4f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3) { my_glUniform4f(location, v0, v1, v2, v3); }
void glUniform1i(GLint location, GLint v0) { my_glUniform1i(location, v0); }
void glUniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) {
    my_glUniformMatrix4fv(location, count, transpose, value);
}

// Attributes
GLint glGetAttribLocation(GLuint program, const GLchar *name) { return my_glGetAttribLocation(program, name); }
void glBindAttribLocation(GLuint program, GLuint index, const GLchar *name) { my_glBindAttribLocation(program, index, name); }

// Textures
void glGenTextures(GLsizei n, GLuint *textures) { my_glGenTextures(n, textures); }
void glBindTexture(GLenum target, GLuint texture) { my_glBindTexture(target, texture); }
void glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void *data) {
    my_glTexImage2D(target, level, internalformat, width, height, border, format, type, data);
}
void glTexParameteri(GLenum target, GLenum pname, GLint param) { my_glTexParameteri(target, pname, param); }
void glDeleteTextures(GLsizei n, const GLuint *textures) { my_glDeleteTextures(n, textures); }
void glActiveTexture(GLenum texture) { my_glActiveTexture(texture); }

// Framebuffers
void glGenFramebuffers(GLsizei n, GLuint *framebuffers) { my_glGenFramebuffers(n, framebuffers); }
void glBindFramebuffer(GLenum target, GLuint framebuffer) { my_glBindFramebuffer(target, framebuffer); }
void glFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level) {
    my_glFramebufferTexture2D(target, attachment, textarget, texture, level);
}
GLenum glCheckFramebufferStatus(GLenum target) { return my_glCheckFramebufferStatus(target); }
void glDeleteFramebuffers(GLsizei n, const GLuint *framebuffers) { my_glDeleteFramebuffers(n, framebuffers); }

// Query
const GLubyte* glGetString(GLenum name) { return my_glGetString(name); }
void glGetIntegerv(GLenum pname, GLint *data) { my_glGetIntegerv(pname, data); }
GLenum glGetError(void) { return my_glGetError(); }

// Flush/Finish
void glFlush(void) { my_glFlush(); }
void glFinish(void) { my_glFinish(); }

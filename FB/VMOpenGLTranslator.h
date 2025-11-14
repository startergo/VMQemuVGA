/*
 * VMOpenGLTranslator.h - OpenGL to Virgl command translator
 * 
 * Intercepts OpenGL calls and translates them to virgl protocol commands
 * for hardware acceleration via VirtIO GPU
 */

#ifndef _VM_OPENGL_TRANSLATOR_H
#define _VM_OPENGL_TRANSLATOR_H

#include <IOKit/IOService.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include "virgl_protocol.h"

// Maximum vertices we can batch before flushing
#define MAX_BATCH_VERTICES 10000
#define MAX_TEXTURES 32
#define MAX_SHADERS 256
#define MAX_VERTEX_BUFFERS 16

// OpenGL state that we need to track
struct VMGLState {
    // Current primitive mode (GL_TRIANGLES, GL_QUADS, etc.)
    uint32_t primitive_mode;
    
    // Vertex data batching
    float* vertex_data;          // Current vertex buffer
    uint32_t vertex_count;       // Number of vertices in buffer
    uint32_t vertex_stride;      // Bytes per vertex
    bool in_begin_end;           // Inside glBegin/glEnd
    
    // Current vertex attributes
    float current_color[4];      // RGBA
    float current_texcoord[4];   // STUV
    float current_normal[3];     // XYZ
    
    // Transformation matrices
    float modelview_matrix[16];
    float projection_matrix[16];
    uint32_t matrix_mode;        // GL_MODELVIEW or GL_PROJECTION
    
    // Viewport
    int viewport_x, viewport_y;
    int viewport_width, viewport_height;
    float depth_near, depth_far;
    
    // Framebuffer
    uint32_t current_fbo;
    uint32_t color_buffer_handle;
    uint32_t depth_buffer_handle;
    
    // Textures
    uint32_t current_texture_unit;
    uint32_t bound_textures[MAX_TEXTURES];
    uint32_t texture_handles[MAX_TEXTURES];
    bool texture_enabled[MAX_TEXTURES];
    
    // Shaders
    uint32_t current_program;
    uint32_t vertex_shader;
    uint32_t fragment_shader;
    
    // Blend state
    bool blend_enabled;
    uint32_t blend_src_factor;
    uint32_t blend_dst_factor;
    uint32_t blend_equation;
    
    // Depth test
    bool depth_test_enabled;
    uint32_t depth_func;
    bool depth_write_enabled;
    
    // Culling
    bool cull_face_enabled;
    uint32_t cull_mode;
    uint32_t front_face;
    
    // Clear color
    float clear_color[4];
    float clear_depth;
    uint32_t clear_stencil;
    
    // Vertex buffer objects
    uint32_t bound_array_buffer;
    uint32_t bound_element_buffer;
    uint32_t vbo_handles[MAX_VERTEX_BUFFERS];
    
    // Vertex array state
    bool vertex_array_enabled;
    bool color_array_enabled;
    bool texcoord_array_enabled;
    bool normal_array_enabled;
    
    void* vertex_pointer;
    void* color_pointer;
    void* texcoord_pointer;
    void* normal_pointer;
    
    uint32_t vertex_size;
    uint32_t vertex_type;
    uint32_t vertex_pointer_stride;
    
    uint32_t color_size;
    uint32_t color_type;
    uint32_t color_pointer_stride;
    
    uint32_t texcoord_size;
    uint32_t texcoord_type;
    uint32_t texcoord_pointer_stride;
    
    uint32_t normal_type;
    uint32_t normal_pointer_stride;
};

class VMQemuVGAAccelerator;
class VMVirtIOGPUAccelerator;

class VMOpenGLTranslator : public OSObject {
    OSDeclareDefaultStructors(VMOpenGLTranslator)
    
private:
    VMVirtIOGPUAccelerator* m_accelerator;
    uint32_t m_context_id;
    VMGLState m_state;
    
    // Resource allocation
    uint32_t m_next_handle;
    
    // Command buffer for batching
    uint32_t* m_command_buffer;
    uint32_t m_command_buffer_size;
    uint32_t m_command_offset;
    
public:
    // Initialization
    virtual bool init() override;
    virtual void free() override;
    
    bool initWithAccelerator(VMVirtIOGPUAccelerator* accel, uint32_t context_id);
    
    // ============ OpenGL Command Translation ============
    
    // Clear operations
    IOReturn glClear(uint32_t mask);
    IOReturn glClearColor(float r, float g, float b, float a);
    IOReturn glClearDepth(double depth);
    
    // Begin/End primitive batching
    IOReturn glBegin(uint32_t mode);
    IOReturn glEnd();
    IOReturn glVertex2f(float x, float y);
    IOReturn glVertex3f(float x, float y, float z);
    IOReturn glVertex4f(float x, float y, float z, float w);
    
    // Vertex attributes
    IOReturn glColor3f(float r, float g, float b);
    IOReturn glColor4f(float r, float g, float b, float a);
    IOReturn glTexCoord2f(float s, float t);
    IOReturn glTexCoord3f(float s, float t, float r);
    IOReturn glNormal3f(float x, float y, float z);
    
    // Vertex arrays
    IOReturn glVertexPointer(uint32_t size, uint32_t type, uint32_t stride, const void* pointer);
    IOReturn glColorPointer(uint32_t size, uint32_t type, uint32_t stride, const void* pointer);
    IOReturn glTexCoordPointer(uint32_t size, uint32_t type, uint32_t stride, const void* pointer);
    IOReturn glNormalPointer(uint32_t type, uint32_t stride, const void* pointer);
    IOReturn glEnableClientState(uint32_t array);
    IOReturn glDisableClientState(uint32_t array);
    IOReturn glDrawArrays(uint32_t mode, uint32_t first, uint32_t count);
    IOReturn glDrawElements(uint32_t mode, uint32_t count, uint32_t type, const void* indices);
    
    // Viewport and transformations
    IOReturn glViewport(int x, int y, int width, int height);
    IOReturn glMatrixMode(uint32_t mode);
    IOReturn glLoadIdentity();
    IOReturn glLoadMatrixf(const float* m);
    IOReturn glMultMatrixf(const float* m);
    IOReturn glOrtho(double left, double right, double bottom, double top, double near, double far);
    IOReturn glFrustum(double left, double right, double bottom, double top, double near, double far);
    
    // Texture operations
    IOReturn glGenTextures(uint32_t n, uint32_t* textures);
    IOReturn glBindTexture(uint32_t target, uint32_t texture);
    IOReturn glTexImage2D(uint32_t target, uint32_t level, uint32_t internal_format,
                         uint32_t width, uint32_t height, uint32_t border,
                         uint32_t format, uint32_t type, const void* pixels);
    IOReturn glTexParameteri(uint32_t target, uint32_t pname, uint32_t param);
    IOReturn glActiveTexture(uint32_t texture);
    
    // State management
    IOReturn glEnable(uint32_t cap);
    IOReturn glDisable(uint32_t cap);
    IOReturn glBlendFunc(uint32_t sfactor, uint32_t dfactor);
    IOReturn glDepthFunc(uint32_t func);
    IOReturn glDepthMask(bool flag);
    IOReturn glCullFace(uint32_t mode);
    IOReturn glFrontFace(uint32_t mode);
    
    // Buffer objects
    IOReturn glGenBuffers(uint32_t n, uint32_t* buffers);
    IOReturn glBindBuffer(uint32_t target, uint32_t buffer);
    IOReturn glBufferData(uint32_t target, uint32_t size, const void* data, uint32_t usage);
    IOReturn glBufferSubData(uint32_t target, uint32_t offset, uint32_t size, const void* data);
    
    // Framebuffer operations
    IOReturn glBindFramebuffer(uint32_t target, uint32_t framebuffer);
    IOReturn glFramebufferTexture2D(uint32_t target, uint32_t attachment,
                                   uint32_t textarget, uint32_t texture, uint32_t level);
    
    // Flush/Finish
    IOReturn glFlush();
    IOReturn glFinish();
    
private:
    // Internal helpers
    IOReturn flushVertexBatch();
    IOReturn submitVirglCommand(uint32_t* cmd_buffer, uint32_t size);
    IOReturn createVirglBuffer(uint32_t size, uint32_t bind_flags, uint32_t* handle);
    IOReturn uploadBufferData(uint32_t handle, const void* data, uint32_t size, uint32_t offset);
    IOReturn createVertexElements(uint32_t* handle);
    IOReturn setVertexBuffers();
    IOReturn setupFramebuffer();
    IOReturn updateViewport();
    IOReturn createDefaultShaders();
    IOReturn bindDefaultShaders();
    IOReturn setupRenderTarget();
    
    uint32_t allocateHandle();
    uint32_t glPrimitiveToVirgl(uint32_t gl_mode);
    uint32_t glBlendFactorToVirgl(uint32_t gl_factor);
    uint32_t glCompareFuncToVirgl(uint32_t gl_func);
    uint32_t glFormatToVirgl(uint32_t gl_format);
    
    // Default shader handles
    uint32_t m_vertex_shader_handle;
    uint32_t m_fragment_shader_handle;
    bool m_shaders_created;
};

#endif /* _VM_OPENGL_TRANSLATOR_H */

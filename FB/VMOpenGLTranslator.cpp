/*
 * VMOpenGLTranslator.cpp - OpenGL to Virgl command translator implementation
 */

#include "VMOpenGLTranslator.h"
#include "VMVirtIOGPU.h"
#include <libkern/OSMalloc.h>
#include <string.h>

#define super OSObject

OSDefineMetaClassAndStructors(VMOpenGLTranslator, OSObject)

// OpenGL constants we need to translate
#define GL_POINTS           0x0000
#define GL_LINES            0x0001
#define GL_LINE_LOOP        0x0002
#define GL_LINE_STRIP       0x0003
#define GL_TRIANGLES        0x0004
#define GL_TRIANGLE_STRIP   0x0005
#define GL_TRIANGLE_FAN     0x0006
#define GL_QUADS            0x0007
#define GL_QUAD_STRIP       0x0008
#define GL_POLYGON          0x0009

#define GL_BLEND            0x0BE2
#define GL_DEPTH_TEST       0x0B71
#define GL_CULL_FACE        0x0B44
#define GL_TEXTURE_2D       0x0DE1

#define GL_VERTEX_ARRAY     0x8074
#define GL_COLOR_ARRAY      0x8076
#define GL_TEXTURE_COORD_ARRAY 0x8078
#define GL_NORMAL_ARRAY     0x8075

#define GL_ARRAY_BUFFER     0x8892
#define GL_ELEMENT_ARRAY_BUFFER 0x8893

bool VMOpenGLTranslator::init() {
    if (!super::init()) {
        return false;
    }
    
    m_accelerator = NULL;
    m_context_id = 0;
    m_next_handle = 100; // Start handles at 100
    m_vertex_shader_handle = 0;
    m_fragment_shader_handle = 0;
    m_shaders_created = false;
    
    // Allocate command buffer (64KB should be enough)
    m_command_buffer_size = 16384; // 64KB / 4 bytes = 16K dwords
    m_command_buffer = (uint32_t*)IOMalloc(m_command_buffer_size * sizeof(uint32_t));
    if (!m_command_buffer) {
        return false;
    }
    m_command_offset = 0;
    
    // Initialize state
    bzero(&m_state, sizeof(m_state));
    m_state.current_color[0] = 1.0f;
    m_state.current_color[1] = 1.0f;
    m_state.current_color[2] = 1.0f;
    m_state.current_color[3] = 1.0f;
    m_state.clear_color[3] = 1.0f;
    m_state.clear_depth = 1.0;
    m_state.depth_near = 0.0f;
    m_state.depth_far = 1.0f;
    m_state.depth_write_enabled = true;
    m_state.blend_src_factor = PIPE_BLENDFACTOR_ONE;
    m_state.blend_dst_factor = PIPE_BLENDFACTOR_ZERO;
    
    // Allocate vertex batch buffer
    m_state.vertex_data = (float*)IOMalloc(MAX_BATCH_VERTICES * 12 * sizeof(float)); // pos+color+texcoord+normal
    if (!m_state.vertex_data) {
        IOFree(m_command_buffer, m_command_buffer_size * sizeof(uint32_t));
        return false;
    }
    
    // Initialize identity matrices
    glLoadIdentity();
    
    IOLog("VMOpenGLTranslator: Initialized with command buffer size %u dwords\n", m_command_buffer_size);
    return true;
}

void VMOpenGLTranslator::free() {
    if (m_command_buffer) {
        IOFree(m_command_buffer, m_command_buffer_size * sizeof(uint32_t));
        m_command_buffer = NULL;
    }
    
    if (m_state.vertex_data) {
        IOFree(m_state.vertex_data, MAX_BATCH_VERTICES * 12 * sizeof(float));
        m_state.vertex_data = NULL;
    }
    
    super::free();
}

bool VMOpenGLTranslator::initWithAccelerator(VMVirtIOGPUAccelerator* accelerator, uint32_t context_id)
{
    m_accelerator = accelerator;
    m_context_id = context_id;
    
    IOLog("VMOpenGLTranslator::initWithAccelerator: context_id=%u\n", context_id);
    
    // Initialize rendering pipeline
    createDefaultShaders();
    setupRenderTarget();
    
    // Set default viewport (matches common 640x480)
    glViewport(0, 0, 640, 480);
    
    IOLog("✅ OpenGL translator initialized with context %u\n", context_id);
    
    return true;
}

uint32_t VMOpenGLTranslator::allocateHandle() {
    return m_next_handle++;
}

uint32_t VMOpenGLTranslator::glPrimitiveToVirgl(uint32_t gl_mode) {
    switch (gl_mode) {
        case GL_POINTS: return PIPE_PRIM_POINTS;
        case GL_LINES: return PIPE_PRIM_LINES;
        case GL_LINE_LOOP: return PIPE_PRIM_LINE_LOOP;
        case GL_LINE_STRIP: return PIPE_PRIM_LINE_STRIP;
        case GL_TRIANGLES: return PIPE_PRIM_TRIANGLES;
        case GL_TRIANGLE_STRIP: return PIPE_PRIM_TRIANGLE_STRIP;
        case GL_TRIANGLE_FAN: return PIPE_PRIM_TRIANGLE_FAN;
        case GL_QUADS: return PIPE_PRIM_QUADS;
        case GL_QUAD_STRIP: return PIPE_PRIM_QUAD_STRIP;
        case GL_POLYGON: return PIPE_PRIM_POLYGON;
        default: return PIPE_PRIM_TRIANGLES;
    }
}

uint32_t VMOpenGLTranslator::glBlendFactorToVirgl(uint32_t gl_factor) {
    // Simplified mapping - would need full GL constant mapping
    return PIPE_BLENDFACTOR_ONE;
}

uint32_t VMOpenGLTranslator::glCompareFuncToVirgl(uint32_t gl_func) {
    // Simplified - would need full mapping
    return PIPE_FUNC_LESS;
}

uint32_t VMOpenGLTranslator::glFormatToVirgl(uint32_t gl_format) {
    // Simplified - would need full format mapping
    return VIRGL_FORMAT_R8G8B8A8_UNORM;
}

// ============ Clear Operations ============

IOReturn VMOpenGLTranslator::glClear(uint32_t mask) {
    if (!m_accelerator) {
        return kIOReturnNotReady;
    }
    
    uint32_t virgl_buffers = 0;
    
    // Translate GL_COLOR_BUFFER_BIT, GL_DEPTH_BUFFER_BIT, GL_STENCIL_BUFFER_BIT
    if (mask & 0x00004000) { // GL_COLOR_BUFFER_BIT
        virgl_buffers |= PIPE_CLEAR_COLOR0;
    }
    if (mask & 0x00000100) { // GL_DEPTH_BUFFER_BIT
        virgl_buffers |= PIPE_CLEAR_DEPTH;
    }
    if (mask & 0x00000400) { // GL_STENCIL_BUFFER_BIT
        virgl_buffers |= PIPE_CLEAR_STENCIL;
    }
    
    uint32_t cmd[VIRGL_CLEAR_SIZE];
    VIRGL_SET_COMMAND(cmd, 0, VIRGL_CCMD_CLEAR, VIRGL_CLEAR_SIZE - 1);
    VIRGL_SET_DWORD(cmd, 1, virgl_buffers);
    VIRGL_SET_DWORD(cmd, 2, virgl_pack_float(m_state.clear_color[0]));
    VIRGL_SET_DWORD(cmd, 3, virgl_pack_float(m_state.clear_color[1]));
    VIRGL_SET_DWORD(cmd, 4, virgl_pack_float(m_state.clear_color[2]));
    VIRGL_SET_DWORD(cmd, 5, virgl_pack_float(m_state.clear_color[3]));
    
    uint32_t depth_lo, depth_hi;
    virgl_pack_double(m_state.clear_depth, &depth_lo, &depth_hi);
    VIRGL_SET_DWORD(cmd, 6, depth_lo);
    VIRGL_SET_DWORD(cmd, 7, depth_hi);
    VIRGL_SET_DWORD(cmd, 8, m_state.clear_stencil);
    
    IOLog("VMOpenGLTranslator::glClear: mask=0x%x, virgl_buffers=0x%x\n", mask, virgl_buffers);
    return submitVirglCommand(cmd, VIRGL_CLEAR_SIZE);
}

IOReturn VMOpenGLTranslator::glClearColor(float r, float g, float b, float a) {
    m_state.clear_color[0] = r;
    m_state.clear_color[1] = g;
    m_state.clear_color[2] = b;
    m_state.clear_color[3] = a;
    return kIOReturnSuccess;
}

IOReturn VMOpenGLTranslator::glClearDepth(double depth) {
    m_state.clear_depth = depth;
    return kIOReturnSuccess;
}

// ============ Viewport ============

IOReturn VMOpenGLTranslator::glViewport(int x, int y, int width, int height) {
    m_state.viewport_x = x;
    m_state.viewport_y = y;
    m_state.viewport_width = width;
    m_state.viewport_height = height;
    
    return updateViewport();
}

IOReturn VMOpenGLTranslator::updateViewport() {
    if (!m_accelerator) {
        return kIOReturnNotReady;
    }
    
    float scale_x = m_state.viewport_width / 2.0f;
    float scale_y = m_state.viewport_height / 2.0f;
    float scale_z = (m_state.depth_far - m_state.depth_near) / 2.0f;
    
    float trans_x = m_state.viewport_x + scale_x;
    float trans_y = m_state.viewport_y + scale_y;
    float trans_z = (m_state.depth_far + m_state.depth_near) / 2.0f;
    
    uint32_t cmd[8];
    VIRGL_SET_COMMAND(cmd, 0, VIRGL_CCMD_SET_VIEWPORT_STATE, 7);
    VIRGL_SET_DWORD(cmd, 1, 0); // start_slot
    VIRGL_SET_DWORD(cmd, 2, virgl_pack_float(scale_x));
    VIRGL_SET_DWORD(cmd, 3, virgl_pack_float(scale_y));
    VIRGL_SET_DWORD(cmd, 4, virgl_pack_float(scale_z));
    VIRGL_SET_DWORD(cmd, 5, virgl_pack_float(trans_x));
    VIRGL_SET_DWORD(cmd, 6, virgl_pack_float(trans_y));
    VIRGL_SET_DWORD(cmd, 7, virgl_pack_float(trans_z));
    
    IOLog("VMOpenGLTranslator::updateViewport: %dx%d at (%d,%d)\n", 
          m_state.viewport_width, m_state.viewport_height,
          m_state.viewport_x, m_state.viewport_y);
    
    return submitVirglCommand(cmd, 8);
}

// ============ Immediate Mode (glBegin/glEnd) ============

IOReturn VMOpenGLTranslator::glBegin(uint32_t mode) {
    if (m_state.in_begin_end) {
        IOLog("VMOpenGLTranslator::glBegin: ERROR - nested glBegin!\n");
        return kIOReturnError;
    }
    
    m_state.in_begin_end = true;
    m_state.primitive_mode = mode;
    m_state.vertex_count = 0;
    
    IOLog("VMOpenGLTranslator::glBegin: mode=0x%x\n", mode);
    return kIOReturnSuccess;
}

IOReturn VMOpenGLTranslator::glEnd() {
    if (!m_state.in_begin_end) {
        IOLog("VMOpenGLTranslator::glEnd: ERROR - glEnd without glBegin!\n");
        return kIOReturnError;
    }
    
    m_state.in_begin_end = false;
    
    IOLog("VMOpenGLTranslator::glEnd: Flushing %u vertices\n", m_state.vertex_count);
    return flushVertexBatch();
}

IOReturn VMOpenGLTranslator::glVertex2f(float x, float y) {
    return glVertex4f(x, y, 0.0f, 1.0f);
}

IOReturn VMOpenGLTranslator::glVertex3f(float x, float y, float z) {
    return glVertex4f(x, y, z, 1.0f);
}

IOReturn VMOpenGLTranslator::glVertex4f(float x, float y, float z, float w) {
    if (!m_state.in_begin_end) {
        IOLog("VMOpenGLTranslator::glVertex: ERROR - vertex outside glBegin/glEnd!\n");
        return kIOReturnError;
    }
    
    if (m_state.vertex_count >= MAX_BATCH_VERTICES) {
        IOLog("VMOpenGLTranslator::glVertex: Batch full, flushing...\n");
        flushVertexBatch();
        m_state.vertex_count = 0;
    }
    
    // Pack vertex data: position (4) + color (4) + texcoord (4) = 12 floats
    uint32_t offset = m_state.vertex_count * 12;
    m_state.vertex_data[offset + 0] = x;
    m_state.vertex_data[offset + 1] = y;
    m_state.vertex_data[offset + 2] = z;
    m_state.vertex_data[offset + 3] = w;
    
    m_state.vertex_data[offset + 4] = m_state.current_color[0];
    m_state.vertex_data[offset + 5] = m_state.current_color[1];
    m_state.vertex_data[offset + 6] = m_state.current_color[2];
    m_state.vertex_data[offset + 7] = m_state.current_color[3];
    
    m_state.vertex_data[offset + 8] = m_state.current_texcoord[0];
    m_state.vertex_data[offset + 9] = m_state.current_texcoord[1];
    m_state.vertex_data[offset + 10] = m_state.current_texcoord[2];
    m_state.vertex_data[offset + 11] = m_state.current_texcoord[3];
    
    m_state.vertex_count++;
    return kIOReturnSuccess;
}

IOReturn VMOpenGLTranslator::glColor3f(float r, float g, float b) {
    return glColor4f(r, g, b, 1.0f);
}

IOReturn VMOpenGLTranslator::glColor4f(float r, float g, float b, float a) {
    m_state.current_color[0] = r;
    m_state.current_color[1] = g;
    m_state.current_color[2] = b;
    m_state.current_color[3] = a;
    return kIOReturnSuccess;
}

IOReturn VMOpenGLTranslator::glTexCoord2f(float s, float t) {
    return glTexCoord3f(s, t, 0.0f);
}

IOReturn VMOpenGLTranslator::glTexCoord3f(float s, float t, float r) {
    m_state.current_texcoord[0] = s;
    m_state.current_texcoord[1] = t;
    m_state.current_texcoord[2] = r;
    m_state.current_texcoord[3] = 1.0f;
    return kIOReturnSuccess;
}

IOReturn VMOpenGLTranslator::glNormal3f(float x, float y, float z) {
    m_state.current_normal[0] = x;
    m_state.current_normal[1] = y;
    m_state.current_normal[2] = z;
    return kIOReturnSuccess;
}

// ============ Vertex Batch Flushing ============

IOReturn VMOpenGLTranslator::flushVertexBatch() {
    if (m_state.vertex_count == 0) {
        return kIOReturnSuccess;
    }
    
    if (!m_accelerator) {
        return kIOReturnNotReady;
    }
    
    IOLog("VMOpenGLTranslator::flushVertexBatch: %u vertices, mode=0x%x\n",
          m_state.vertex_count, m_state.primitive_mode);
    
    // 1. Create/update vertex buffer with data
    uint32_t vbo_handle = allocateHandle();
    uint32_t data_size = m_state.vertex_count * 12 * sizeof(float);
    
    IOReturn ret = createVirglBuffer(data_size, VIRGL_BIND_VERTEX_BUFFER, &vbo_handle);
    if (ret != kIOReturnSuccess) {
        IOLog("VMOpenGLTranslator::flushVertexBatch: Failed to create VBO\n");
        return ret;
    }
    
    ret = uploadBufferData(vbo_handle, m_state.vertex_data, data_size, 0);
    if (ret != kIOReturnSuccess) {
        IOLog("VMOpenGLTranslator::flushVertexBatch: Failed to upload vertex data\n");
        return ret;
    }
    
    // 2. Create vertex element state (vertex format)
    uint32_t ve_handle = allocateHandle();
    ret = createVertexElements(&ve_handle);
    if (ret != kIOReturnSuccess) {
        IOLog("VMOpenGLTranslator::flushVertexBatch: Failed to create vertex elements\n");
        return ret;
    }
    
    // 3. Bind vertex elements
    uint32_t bind_ve_cmd[3];
    VIRGL_SET_COMMAND(bind_ve_cmd, 0, VIRGL_CCMD_BIND_OBJECT, 2);
    VIRGL_SET_DWORD(bind_ve_cmd, 1, ve_handle);
    VIRGL_SET_DWORD(bind_ve_cmd, 2, VIRGL_OBJECT_VERTEX_ELEMENTS);
    submitVirglCommand(bind_ve_cmd, 3);
    
    // 4. Set vertex buffers
    uint32_t vb_cmd[5];
    VIRGL_SET_COMMAND(vb_cmd, 0, VIRGL_CCMD_SET_VERTEX_BUFFERS, 4);
    VIRGL_SET_DWORD(vb_cmd, 1, 12 * sizeof(float)); // stride
    VIRGL_SET_DWORD(vb_cmd, 2, 0); // offset
    VIRGL_SET_DWORD(vb_cmd, 3, vbo_handle);
    VIRGL_SET_DWORD(vb_cmd, 4, 0); // padding/reserved
    submitVirglCommand(vb_cmd, 5);
    
    // 5. Submit draw command
    uint32_t draw_cmd[VIRGL_DRAW_VBO_SIZE];
    VIRGL_SET_COMMAND(draw_cmd, 0, VIRGL_CCMD_DRAW_VBO, VIRGL_DRAW_VBO_SIZE - 1);
    VIRGL_SET_DWORD(draw_cmd, 1, 0); // start
    VIRGL_SET_DWORD(draw_cmd, 2, m_state.vertex_count); // count
    VIRGL_SET_DWORD(draw_cmd, 3, glPrimitiveToVirgl(m_state.primitive_mode)); // mode
    VIRGL_SET_DWORD(draw_cmd, 4, 0); // indexed (false)
    VIRGL_SET_DWORD(draw_cmd, 5, 1); // instance_count
    VIRGL_SET_DWORD(draw_cmd, 6, 0); // index_bias
    VIRGL_SET_DWORD(draw_cmd, 7, 0); // start_instance
    VIRGL_SET_DWORD(draw_cmd, 8, 0); // primitive_restart
    VIRGL_SET_DWORD(draw_cmd, 9, 0); // restart_index
    VIRGL_SET_DWORD(draw_cmd, 10, 0); // min_index
    VIRGL_SET_DWORD(draw_cmd, 11, m_state.vertex_count - 1); // max_index
    
    ret = submitVirglCommand(draw_cmd, VIRGL_DRAW_VBO_SIZE);
    
    IOLog("VMOpenGLTranslator::flushVertexBatch: ✅ Submitted draw command for %u vertices\n",
          m_state.vertex_count);
    
    return ret;
}

// ============ Helper Functions ============

IOReturn VMOpenGLTranslator::submitVirglCommand(uint32_t* cmd_buffer, uint32_t size) {
    if (!m_accelerator) {
        return kIOReturnNotReady;
    }
    
    VMVirtIOGPU* gpu = m_accelerator->getVirtIOGPUDevice();
    if (!gpu) {
        IOLog("VMOpenGLTranslator::submitVirglCommand: No VirtIO GPU device\n");
        return kIOReturnNotReady;
    }
    
    // Create memory descriptor for command
    IOBufferMemoryDescriptor* cmdDesc = IOBufferMemoryDescriptor::withBytes(
        cmd_buffer, size * sizeof(uint32_t), kIODirectionOut);
    
    if (!cmdDesc) {
        IOLog("VMOpenGLTranslator::submitVirglCommand: Failed to create command descriptor\n");
        return kIOReturnNoMemory;
    }
    
    IOReturn ret = gpu->executeCommands(m_context_id, cmdDesc);
    cmdDesc->release();
    
    return ret;
}

IOReturn VMOpenGLTranslator::createVirglBuffer(uint32_t size, uint32_t bind_flags, uint32_t* handle) {
    if (!m_accelerator) {
        return kIOReturnNotReady;
    }
    
    VMVirtIOGPU* gpu = m_accelerator->getVirtIOGPUDevice();
    if (!gpu) {
        return kIOReturnNotReady;
    }
    
    // Use the existing createResource3D from VirtIO GPU device
    return gpu->createResource3D(*handle, VIRGL_TARGET_BUFFER,
                                 VIRGL_FORMAT_R8G8B8A8_UNORM,
                                 bind_flags, size, 1, 1);
}

IOReturn VMOpenGLTranslator::uploadBufferData(uint32_t handle, const void* data, uint32_t size, uint32_t offset) {
    if (!m_accelerator || !data) {
        return kIOReturnBadArgument;
    }
    
    // Calculate command size: header (12 dwords) + data
    uint32_t data_dwords = (size + 3) / 4; // Round up to dwords
    uint32_t total_size = VIRGL_INLINE_WRITE_HDR_SIZE + data_dwords;
    
    uint32_t* cmd = (uint32_t*)IOMalloc(total_size * sizeof(uint32_t));
    if (!cmd) {
        return kIOReturnNoMemory;
    }
    
    VIRGL_SET_COMMAND(cmd, 0, VIRGL_CCMD_RESOURCE_INLINE_WRITE, total_size - 1);
    VIRGL_SET_DWORD(cmd, 1, handle);
    VIRGL_SET_DWORD(cmd, 2, 0); // level
    VIRGL_SET_DWORD(cmd, 3, 0); // usage
    VIRGL_SET_DWORD(cmd, 4, 0); // stride
    VIRGL_SET_DWORD(cmd, 5, 0); // layer_stride
    VIRGL_SET_DWORD(cmd, 6, offset); // x (offset)
    VIRGL_SET_DWORD(cmd, 7, 0); // y
    VIRGL_SET_DWORD(cmd, 8, 0); // z
    VIRGL_SET_DWORD(cmd, 9, size); // width (size)
    VIRGL_SET_DWORD(cmd, 10, 1); // height
    VIRGL_SET_DWORD(cmd, 11, 1); // depth
    
    // Copy data
    memcpy(&cmd[VIRGL_INLINE_WRITE_HDR_SIZE], data, size);
    
    IOReturn ret = submitVirglCommand(cmd, total_size);
    IOFree(cmd, total_size * sizeof(uint32_t));
    
    return ret;
}

IOReturn VMOpenGLTranslator::createVertexElements(uint32_t* handle) {
    // Define vertex format: position (vec4) + color (vec4) + texcoord (vec4)
    uint32_t cmd[15];
    *handle = allocateHandle();
    
    VIRGL_SET_COMMAND(cmd, 0, VIRGL_CCMD_CREATE_OBJECT, 14);
    VIRGL_SET_DWORD(cmd, 1, *handle);
    VIRGL_SET_DWORD(cmd, 2, VIRGL_OBJECT_VERTEX_ELEMENTS);
    
    // Element 0: position (vec4 float)
    VIRGL_SET_DWORD(cmd, 3, 0); // src_offset
    VIRGL_SET_DWORD(cmd, 4, 0); // instance_divisor
    VIRGL_SET_DWORD(cmd, 5, 0); // vertex_buffer_index
    VIRGL_SET_DWORD(cmd, 6, PIPE_FORMAT_R32G32B32A32_FLOAT); // src_format
    
    // Element 1: color (vec4 float)
    VIRGL_SET_DWORD(cmd, 7, 16); // src_offset (4 floats = 16 bytes)
    VIRGL_SET_DWORD(cmd, 8, 0); // instance_divisor
    VIRGL_SET_DWORD(cmd, 9, 0); // vertex_buffer_index
    VIRGL_SET_DWORD(cmd, 10, PIPE_FORMAT_R32G32B32A32_FLOAT); // src_format
    
    // Element 2: texcoord (vec4 float)
    VIRGL_SET_DWORD(cmd, 11, 32); // src_offset (8 floats = 32 bytes)
    VIRGL_SET_DWORD(cmd, 12, 0); // instance_divisor
    VIRGL_SET_DWORD(cmd, 13, 0); // vertex_buffer_index
    VIRGL_SET_DWORD(cmd, 14, PIPE_FORMAT_R32G32B32A32_FLOAT); // src_format
    
    return submitVirglCommand(cmd, 15);
}

// ============ Matrix Operations ============

IOReturn VMOpenGLTranslator::glLoadIdentity() {
    float* matrix = (m_state.matrix_mode == 0x1700) ? // GL_MODELVIEW
                    m_state.modelview_matrix : m_state.projection_matrix;
    
    bzero(matrix, 16 * sizeof(float));
    matrix[0] = matrix[5] = matrix[10] = matrix[15] = 1.0f;
    
    return kIOReturnSuccess;
}

IOReturn VMOpenGLTranslator::glMatrixMode(uint32_t mode) {
    m_state.matrix_mode = mode;
    return kIOReturnSuccess;
}

// ============ State Management ============

IOReturn VMOpenGLTranslator::glEnable(uint32_t cap) {
    switch (cap) {
        case GL_BLEND:
            m_state.blend_enabled = true;
            break;
        case GL_DEPTH_TEST:
            m_state.depth_test_enabled = true;
            break;
        case GL_CULL_FACE:
            m_state.cull_face_enabled = true;
            break;
        case GL_TEXTURE_2D:
            m_state.texture_enabled[m_state.current_texture_unit] = true;
            break;
    }
    
    IOLog("VMOpenGLTranslator::glEnable: cap=0x%x\n", cap);
    return kIOReturnSuccess;
}

IOReturn VMOpenGLTranslator::glDisable(uint32_t cap) {
    switch (cap) {
        case GL_BLEND:
            m_state.blend_enabled = false;
            break;
        case GL_DEPTH_TEST:
            m_state.depth_test_enabled = false;
            break;
        case GL_CULL_FACE:
            m_state.cull_face_enabled = false;
            break;
        case GL_TEXTURE_2D:
            m_state.texture_enabled[m_state.current_texture_unit] = false;
            break;
    }
    
    IOLog("VMOpenGLTranslator::glDisable: cap=0x%x\n", cap);
    return kIOReturnSuccess;
}

// ============ Flush/Finish ============

IOReturn VMOpenGLTranslator::glFlush() {
    IOLog("VMOpenGLTranslator::glFlush\n");
    // In virgl, flush happens automatically on command submission
    return kIOReturnSuccess;
}

IOReturn VMOpenGLTranslator::glFinish() {
    IOLog("VMOpenGLTranslator::glFinish\n");
    // Would need to implement synchronization with host
    return kIOReturnSuccess;
}

// ============ Shader and Framebuffer Setup ============

IOReturn VMOpenGLTranslator::createDefaultShaders() {
    if (m_shaders_created) {
        return kIOReturnSuccess;  // Already created
    }
    
    if (!m_accelerator) {
        return kIOReturnNotReady;
    }
    
    VMVirtIOGPU* gpu = m_accelerator->getVirtIOGPUDevice();
    if (!gpu) {
        return kIOReturnNotReady;
    }
    
    // For now, just log that we would create shaders
    // Real shader compilation requires TGSI bytecode generation
    // which is complex - we'll rely on host-side shader compilation for now
    IOLog("VMOpenGLTranslator::createDefaultShaders: Shaders will be compiled on host\n");
    
    m_vertex_shader_handle = 0;     // Host will assign
    m_fragment_shader_handle = 0;   // Host will assign
    m_shaders_created = true;
    
    return kIOReturnSuccess;
}

IOReturn VMOpenGLTranslator::bindDefaultShaders() {
    // For virgl, shader binding happens implicitly through draw state
    // The vertex format definition acts as the "shader" specification
    // for fixed-function-like rendering
    return kIOReturnSuccess;
}

IOReturn VMOpenGLTranslator::setupRenderTarget() {
    if (!m_accelerator) {
        return kIOReturnNotReady;
    }
    
    VMVirtIOGPU* gpu = m_accelerator->getVirtIOGPUDevice();
    if (!gpu) {
        return kIOReturnNotReady;
    }
    
    // Get the render target and depth buffer resource IDs from the GPU device
    // These were created in initializeWebGLAcceleration()
    // We need to bind them to the framebuffer
    
    // For now, we'll use resource ID 2 (canvas) and 3 (depth) which were
    // created during initialization
    uint32_t color_handle = 2;  // Canvas resource from initializeWebGLAcceleration
    uint32_t depth_handle = 3;  // Depth resource from initializeWebGLAcceleration
    
    // Create SET_FRAMEBUFFER_STATE command
    uint32_t fb_cmd[VIRGL_SET_FRAMEBUFFER_STATE_SIZE];
    VIRGL_SET_COMMAND(fb_cmd, 0, VIRGL_CCMD_SET_FRAMEBUFFER_STATE, VIRGL_SET_FRAMEBUFFER_STATE_SIZE - 1);
    VIRGL_SET_DWORD(fb_cmd, 1, 1); // nr_cbufs = 1 color buffer
    VIRGL_SET_DWORD(fb_cmd, 2, depth_handle); // zsurf_handle = depth buffer
    VIRGL_SET_DWORD(fb_cmd, 3, color_handle); // cbufs[0] = color buffer
    VIRGL_SET_DWORD(fb_cmd, 4, 0); // cbufs[1-7] = 0
    VIRGL_SET_DWORD(fb_cmd, 5, 0);
    VIRGL_SET_DWORD(fb_cmd, 6, 0);
    VIRGL_SET_DWORD(fb_cmd, 7, 0);
    VIRGL_SET_DWORD(fb_cmd, 8, 0);
    VIRGL_SET_DWORD(fb_cmd, 9, 0);
    VIRGL_SET_DWORD(fb_cmd, 10, 0);
    
    IOReturn ret = submitVirglCommand(fb_cmd, VIRGL_SET_FRAMEBUFFER_STATE_SIZE);
    
    if (ret == kIOReturnSuccess) {
        m_state.current_fbo = 1; // Mark as bound
        m_state.color_buffer_handle = color_handle;
        m_state.depth_buffer_handle = depth_handle;
        IOLog("VMOpenGLTranslator::setupRenderTarget: ✅ Bound color=%u depth=%u to framebuffer\n",
              color_handle, depth_handle);
    } else {
        IOLog("VMOpenGLTranslator::setupRenderTarget: ❌ Failed to bind framebuffer\n");
    }
    
    return ret;
}

// Stubs for unimplemented functions (to be completed)
IOReturn VMOpenGLTranslator::glVertexPointer(uint32_t size, uint32_t type, uint32_t stride, const void* pointer) { return kIOReturnSuccess; }
IOReturn VMOpenGLTranslator::glColorPointer(uint32_t size, uint32_t type, uint32_t stride, const void* pointer) { return kIOReturnSuccess; }
IOReturn VMOpenGLTranslator::glTexCoordPointer(uint32_t size, uint32_t type, uint32_t stride, const void* pointer) { return kIOReturnSuccess; }
IOReturn VMOpenGLTranslator::glNormalPointer(uint32_t type, uint32_t stride, const void* pointer) { return kIOReturnSuccess; }
IOReturn VMOpenGLTranslator::glEnableClientState(uint32_t array) { return kIOReturnSuccess; }
IOReturn VMOpenGLTranslator::glDisableClientState(uint32_t array) { return kIOReturnSuccess; }
IOReturn VMOpenGLTranslator::glDrawArrays(uint32_t mode, uint32_t first, uint32_t count) { return kIOReturnSuccess; }
IOReturn VMOpenGLTranslator::glDrawElements(uint32_t mode, uint32_t count, uint32_t type, const void* indices) { return kIOReturnSuccess; }
IOReturn VMOpenGLTranslator::glLoadMatrixf(const float* m) { return kIOReturnSuccess; }
IOReturn VMOpenGLTranslator::glMultMatrixf(const float* m) { return kIOReturnSuccess; }
IOReturn VMOpenGLTranslator::glOrtho(double left, double right, double bottom, double top, double near, double far) { return kIOReturnSuccess; }
IOReturn VMOpenGLTranslator::glFrustum(double left, double right, double bottom, double top, double near, double far) { return kIOReturnSuccess; }
IOReturn VMOpenGLTranslator::glGenTextures(uint32_t n, uint32_t* textures) { return kIOReturnSuccess; }
IOReturn VMOpenGLTranslator::glBindTexture(uint32_t target, uint32_t texture) { return kIOReturnSuccess; }
IOReturn VMOpenGLTranslator::glTexImage2D(uint32_t target, uint32_t level, uint32_t internal_format, uint32_t width, uint32_t height, uint32_t border, uint32_t format, uint32_t type, const void* pixels) { return kIOReturnSuccess; }
IOReturn VMOpenGLTranslator::glTexParameteri(uint32_t target, uint32_t pname, uint32_t param) { return kIOReturnSuccess; }
IOReturn VMOpenGLTranslator::glActiveTexture(uint32_t texture) { return kIOReturnSuccess; }
IOReturn VMOpenGLTranslator::glBlendFunc(uint32_t sfactor, uint32_t dfactor) { return kIOReturnSuccess; }
IOReturn VMOpenGLTranslator::glDepthFunc(uint32_t func) { return kIOReturnSuccess; }
IOReturn VMOpenGLTranslator::glDepthMask(bool flag) { return kIOReturnSuccess; }
IOReturn VMOpenGLTranslator::glCullFace(uint32_t mode) { return kIOReturnSuccess; }
IOReturn VMOpenGLTranslator::glFrontFace(uint32_t mode) { return kIOReturnSuccess; }
IOReturn VMOpenGLTranslator::glGenBuffers(uint32_t n, uint32_t* buffers) { return kIOReturnSuccess; }
IOReturn VMOpenGLTranslator::glBindBuffer(uint32_t target, uint32_t buffer) { return kIOReturnSuccess; }
IOReturn VMOpenGLTranslator::glBufferData(uint32_t target, uint32_t size, const void* data, uint32_t usage) { return kIOReturnSuccess; }
IOReturn VMOpenGLTranslator::glBufferSubData(uint32_t target, uint32_t offset, uint32_t size, const void* data) { return kIOReturnSuccess; }
IOReturn VMOpenGLTranslator::glBindFramebuffer(uint32_t target, uint32_t framebuffer) { return kIOReturnSuccess; }
IOReturn VMOpenGLTranslator::glFramebufferTexture2D(uint32_t target, uint32_t attachment, uint32_t textarget, uint32_t texture, uint32_t level) { return kIOReturnSuccess; }
IOReturn VMOpenGLTranslator::setupFramebuffer() { return kIOReturnSuccess; }
IOReturn VMOpenGLTranslator::setVertexBuffers() { return kIOReturnSuccess; }

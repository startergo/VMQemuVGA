#ifndef VMOpenGLBridge_h
#define VMOpenGLBridge_h

#include <IOKit/IOService.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <libkern/c++/OSObject.h>
#include <libkern/c++/OSArray.h>
#include <libkern/c++/OSDictionary.h>

// Forward declarations
class VMQemuVGAAccelerator;
class VMVirtIOGPU;
class VMMetalBridge;

// OpenGL constants and enums
typedef enum {
    VM_GL_TRIANGLES = 0x0004,
    VM_GL_TRIANGLE_STRIP = 0x0005,
    VM_GL_TRIANGLE_FAN = 0x0006,
    VM_GL_LINES = 0x0001,
    VM_GL_LINE_STRIP = 0x0003,
    VM_GL_POINTS = 0x0000
} VMGLPrimitiveType;

typedef enum {
    VM_GL_BYTE = 0x1400,
    VM_GL_UNSIGNED_BYTE = 0x1401,
    VM_GL_SHORT = 0x1402,
    VM_GL_UNSIGNED_SHORT = 0x1403,
    VM_GL_INT = 0x1404,
    VM_GL_UNSIGNED_INT = 0x1405,
    VM_GL_FLOAT = 0x1406,
    VM_GL_DOUBLE = 0x140A
} VMGLDataType;

typedef enum {
    VM_GL_DEPTH_TEST = 0x0B71,
    VM_GL_STENCIL_TEST = 0x0B90,
    VM_GL_CULL_FACE = 0x0B44,
    VM_GL_BLEND = 0x0BE2,
    VM_GL_SCISSOR_TEST = 0x0C11,
    VM_GL_DITHER = 0x0BD0
} VMGLCapability;

typedef enum {
    VM_GL_NEVER = 0x0200,
    VM_GL_LESS = 0x0201,
    VM_GL_EQUAL = 0x0202,
    VM_GL_LEQUAL = 0x0203,
    VM_GL_GREATER = 0x0204,
    VM_GL_NOTEQUAL = 0x0205,
    VM_GL_GEQUAL = 0x0206,
    VM_GL_ALWAYS = 0x0207
} VMGLCompareFunc;

typedef enum {
    VM_GL_ZERO = 0,
    VM_GL_ONE = 1,
    VM_GL_SRC_COLOR = 0x0300,
    VM_GL_ONE_MINUS_SRC_COLOR = 0x0301,
    VM_GL_SRC_ALPHA = 0x0302,
    VM_GL_ONE_MINUS_SRC_ALPHA = 0x0303,
    VM_GL_DST_ALPHA = 0x0304,
    VM_GL_ONE_MINUS_DST_ALPHA = 0x0305,
    VM_GL_DST_COLOR = 0x0306,
    VM_GL_ONE_MINUS_DST_COLOR = 0x0307
} VMGLBlendFactor;

// OpenGL buffer types
typedef enum {
    VM_GL_ARRAY_BUFFER = 0x8892,
    VM_GL_ELEMENT_ARRAY_BUFFER = 0x8893,
    VM_GL_UNIFORM_BUFFER = 0x8A11,
    VM_GL_TEXTURE_BUFFER = 0x8C2A
} VMGLBufferTarget;

// OpenGL texture types
typedef enum {
    VM_GL_TEXTURE_1D = 0x0DE0,
    VM_GL_TEXTURE_2D = 0x0DE1,
    VM_GL_TEXTURE_3D = 0x806F,
    VM_GL_TEXTURE_CUBE_MAP = 0x8513,
    VM_GL_TEXTURE_2D_ARRAY = 0x8C1A
} VMGLTextureTarget;

// OpenGL pixel formats
typedef enum {
    VM_GL_RED = 0x1903,
    VM_GL_RG = 0x8227,
    VM_GL_RGB = 0x1907,
    VM_GL_RGBA = 0x1908,
    VM_GL_BGR = 0x80E0,
    VM_GL_BGRA = 0x80E1,
    VM_GL_DEPTH_COMPONENT = 0x1902,
    VM_GL_STENCIL_INDEX = 0x1901
} VMGLPixelFormat;

// OpenGL context descriptor
typedef struct {
    uint32_t major_version;
    uint32_t minor_version;
    uint32_t profile_mask;
    uint32_t context_flags;
    uint32_t pixel_format_attributes[16];
} VMGLContextDescriptor;

// OpenGL buffer descriptor
typedef struct {
    VMGLBufferTarget target;
    uint32_t size;
    uint32_t usage;
    const void* data;
} VMGLBufferDescriptor;

// OpenGL texture descriptor
typedef struct {
    VMGLTextureTarget target;
    uint32_t width;
    uint32_t height;
    uint32_t depth;
    uint32_t mipmap_levels;
    VMGLPixelFormat internal_format;
    VMGLPixelFormat format;
    VMGLDataType type;
    const void* data;
} VMGLTextureDescriptor;

// OpenGL shader descriptor
typedef struct {
    uint32_t shader_type; // GL_VERTEX_SHADER, GL_FRAGMENT_SHADER, etc.
    const char* source_code;
    uint32_t source_length;
    uint32_t compile_flags;
} VMGLShaderDescriptor;

// OpenGL render state
typedef struct {
    bool depth_test_enabled;
    bool stencil_test_enabled;
    bool cull_face_enabled;
    bool blend_enabled;
    bool scissor_test_enabled;
    VMGLCompareFunc depth_func;
    VMGLBlendFactor src_blend_factor;
    VMGLBlendFactor dst_blend_factor;
    float clear_color[4];
    float clear_depth;
    uint32_t clear_stencil;
} VMGLRenderState;

// Accelerator OpenGL capabilities descriptor
typedef struct {
    // Basic 3D support
    bool supports_3d_rendering;
    bool supports_hardware_acceleration;
    
    // OpenGL version support
    uint32_t max_opengl_major;
    uint32_t max_opengl_minor;
    
    // Shader capabilities
    bool supports_vertex_shaders;
    bool supports_fragment_shaders;
    uint32_t max_vertex_shaders;
    uint32_t max_fragment_shaders;
    uint32_t max_geometry_shaders;
    bool supports_tessellation_shaders;
    bool supports_compute_shaders;
    
    // Metal integration
    bool supports_metal_interop;
    
    // Texture capabilities
    uint32_t max_texture_size;
    uint32_t max_texture_units;
    bool supports_3d_textures;
    bool supports_cube_maps;
    bool supports_texture_arrays;
    
    // Buffer capabilities
    uint32_t max_vertex_attributes;
    uint32_t max_uniform_buffer_size;
    bool supports_vertex_array_objects;
    bool supports_uniform_buffer_objects;
    
    // Framebuffer capabilities
    uint32_t max_framebuffer_width;
    uint32_t max_framebuffer_height;
    bool supports_multiple_render_targets;
    uint32_t max_color_attachments;
} VMAcceleratorOpenGLCapabilities;

/**
 * @class VMOpenGLBridge
 * @brief OpenGL Compatibility Layer for VMQemuVGA 3D Acceleration
 * 
 * This class provides OpenGL API compatibility for the VMQemuVGA 3D acceleration
 * system, enabling legacy OpenGL applications to run with hardware acceleration
 * in virtual machines by translating OpenGL calls to the underlying VirtIO GPU
 * and Metal rendering infrastructure.
 */
class VMOpenGLBridge : public OSObject
{
    OSDeclareDefaultStructors(VMOpenGLBridge);
    
private:
    VMQemuVGAAccelerator* m_accelerator;
    VMVirtIOGPU* m_gpu_device;
    VMMetalBridge* m_metal_bridge;
    IORecursiveLock* m_lock;
    
    // OpenGL context management
    OSArray* m_gl_contexts;
    OSArray* m_gl_buffers;
    OSArray* m_gl_textures;
    OSArray* m_gl_shaders;
    OSArray* m_gl_programs;
    OSArray* m_gl_vertex_arrays;
    
    // Resource tracking
    uint32_t m_next_gl_id;
    OSDictionary* m_gl_resource_map;
    uint32_t m_current_context_id;
    
    // OpenGL state management
    VMGLRenderState m_current_state;
    uint32_t m_bound_array_buffer;
    uint32_t m_bound_element_array_buffer;
    uint32_t m_active_texture_unit;
    uint32_t m_bound_textures[32]; // Support up to 32 texture units
    uint32_t m_current_program;
    
    // Performance counters
    uint64_t m_gl_draw_calls;
    uint64_t m_gl_state_changes;
    uint64_t m_gl_buffer_uploads;
    uint64_t m_gl_texture_uploads;
    
    // Feature support
    bool m_supports_gl_3_0;
    bool m_supports_gl_3_2;
    bool m_supports_gl_4_0;
    bool m_supports_vertex_array_objects;
    bool m_supports_uniform_buffer_objects;
    bool m_supports_geometry_shaders;
    bool m_supports_tessellation;
    
public:
    // Initialization and cleanup
    virtual bool init() override;
    virtual void free() override;
    
    // Setup and configuration
    bool initWithAccelerator(VMQemuVGAAccelerator* accelerator);
    IOReturn setupOpenGLSupport();
    IOReturn configureGLFeatures();
    
    // Context management
    IOReturn createContext(const VMGLContextDescriptor* descriptor, uint32_t* context_id);
    IOReturn destroyContext(uint32_t context_id);
    IOReturn makeContextCurrent(uint32_t context_id);
    IOReturn swapBuffers(uint32_t context_id);
    uint32_t getCurrentContext();
    
    // Buffer management
    IOReturn genBuffers(uint32_t count, uint32_t* buffer_ids);
    IOReturn deleteBuffers(uint32_t count, const uint32_t* buffer_ids);
    IOReturn bindBuffer(VMGLBufferTarget target, uint32_t buffer_id);
    IOReturn bufferData(VMGLBufferTarget target, const VMGLBufferDescriptor* descriptor);
    IOReturn bufferSubData(VMGLBufferTarget target, uint32_t offset, 
                          uint32_t size, const void* data);
    void* mapBuffer(VMGLBufferTarget target, uint32_t access);
    IOReturn unmapBuffer(VMGLBufferTarget target);
    
    // Texture management
    IOReturn genTextures(uint32_t count, uint32_t* texture_ids);
    IOReturn deleteTextures(uint32_t count, const uint32_t* texture_ids);
    IOReturn bindTexture(VMGLTextureTarget target, uint32_t texture_id);
    IOReturn texImage2D(VMGLTextureTarget target, uint32_t level,
                       const VMGLTextureDescriptor* descriptor);
    IOReturn texSubImage2D(VMGLTextureTarget target, uint32_t level,
                          uint32_t x_offset, uint32_t y_offset,
                          uint32_t width, uint32_t height,
                          VMGLPixelFormat format, VMGLDataType type, const void* data);
    IOReturn generateMipmap(VMGLTextureTarget target);
    IOReturn activeTexture(uint32_t texture_unit);
    
    // Shader management
    IOReturn createShader(const VMGLShaderDescriptor* descriptor, uint32_t* shader_id);
    IOReturn deleteShader(uint32_t shader_id);
    IOReturn compileShader(uint32_t shader_id);
    IOReturn getShaderInfoLog(uint32_t shader_id, uint32_t buffer_size, 
                             uint32_t* log_length, char* info_log);
    bool isShaderCompiled(uint32_t shader_id);
    
    // Program management
    IOReturn createProgram(uint32_t* program_id);
    IOReturn deleteProgram(uint32_t program_id);
    IOReturn attachShader(uint32_t program_id, uint32_t shader_id);
    IOReturn detachShader(uint32_t program_id, uint32_t shader_id);
    IOReturn linkProgram(uint32_t program_id);
    IOReturn useProgram(uint32_t program_id);
    IOReturn getProgramInfoLog(uint32_t program_id, uint32_t buffer_size,
                              uint32_t* log_length, char* info_log);
    bool isProgramLinked(uint32_t program_id);
    
    // Uniform management
    int32_t getUniformLocation(uint32_t program_id, const char* name);
    IOReturn uniform1f(int32_t location, float value);
    IOReturn uniform2f(int32_t location, float v0, float v1);
    IOReturn uniform3f(int32_t location, float v0, float v1, float v2);
    IOReturn uniform4f(int32_t location, float v0, float v1, float v2, float v3);
    IOReturn uniform1i(int32_t location, int32_t value);
    IOReturn uniformMatrix4fv(int32_t location, uint32_t count, bool transpose, const float* value);
    
    // Vertex attribute management
    IOReturn genVertexArrays(uint32_t count, uint32_t* array_ids);
    IOReturn deleteVertexArrays(uint32_t count, const uint32_t* array_ids);
    IOReturn bindVertexArray(uint32_t array_id);
    IOReturn enableVertexAttribArray(uint32_t index);
    IOReturn disableVertexAttribArray(uint32_t index);
    IOReturn vertexAttribPointer(uint32_t index, uint32_t size, VMGLDataType type,
                                bool normalized, uint32_t stride, const void* pointer);
    
    // Rendering commands
    IOReturn clear(uint32_t mask);
    IOReturn clearColor(float red, float green, float blue, float alpha);
    IOReturn clearDepth(double depth);
    IOReturn clearStencil(int32_t stencil);
    IOReturn drawArrays(VMGLPrimitiveType mode, uint32_t first, uint32_t count);
    IOReturn drawElements(VMGLPrimitiveType mode, uint32_t count, VMGLDataType type,
                         const void* indices);
    IOReturn drawElementsBaseVertex(VMGLPrimitiveType mode, uint32_t count, VMGLDataType type,
                                   const void* indices, int32_t base_vertex);
    
    // State management
    IOReturn enable(VMGLCapability cap);
    IOReturn disable(VMGLCapability cap);
    bool isEnabled(VMGLCapability cap);
    IOReturn depthFunc(VMGLCompareFunc func);
    IOReturn blendFunc(VMGLBlendFactor sfactor, VMGLBlendFactor dfactor);
    IOReturn cullFace(uint32_t mode);
    IOReturn frontFace(uint32_t mode);
    IOReturn viewport(int32_t x, int32_t y, uint32_t width, uint32_t height);
    IOReturn scissor(int32_t x, int32_t y, uint32_t width, uint32_t height);
    
    // Query functions
    const char* getString(uint32_t name);
    void getIntegerv(uint32_t pname, int32_t* params);
    void getFloatv(uint32_t pname, float* params);
    void getBooleanv(uint32_t pname, bool* params);
    uint32_t getError();
    
    // Feature support queries
    bool supportsGLVersion(uint32_t major, uint32_t minor);
    bool supportsExtension(const char* extension_name);
    bool supportsGLFeature(uint32_t feature_flag);
    
    // Performance and debugging
    IOReturn getGLPerformanceStats(void* stats_buffer, size_t* buffer_size);
    void resetGLCounters();
    void logOpenGLBridgeState();
    
private:
    // Internal helper methods
    OSObject* findGLResource(uint32_t resource_id);
    uint32_t allocateGLId();
    void releaseGLId(uint32_t gl_id);
    IOReturn translateGLError(uint32_t gl_error);
    VMGLDataType getGLDataType(uint32_t gl_type);
    VMGLPrimitiveType getGLPrimitiveType(uint32_t gl_mode);
    IOReturn validateGLContext(uint32_t context_id);
    IOReturn syncGLState();
    void updatePerformanceCounters(const char* operation);
    IOReturn queryHostGLCapabilities();
    
    // OpenGL to Metal/VirtIO translation
    IOReturn translateDrawCall(VMGLPrimitiveType mode, uint32_t count, uint32_t first);
    IOReturn translateBufferUpdate(VMGLBufferTarget target, const void* data, uint32_t size);
    IOReturn translateTextureUpdate(VMGLTextureTarget target, const VMGLTextureDescriptor* desc);
    IOReturn translateShaderCompilation(uint32_t shader_type, const char* source);
    IOReturn translateUniformUpdate(int32_t location, const void* value, uint32_t type);
    
    // State synchronization
    IOReturn syncDepthState();
    IOReturn syncBlendState();
    IOReturn syncCullState();
    IOReturn syncViewportState();
    IOReturn syncTextureState();
    IOReturn syncShaderState();
    
    // Context validation
    IOReturn performContextValidation();
    IOReturn validateResourceState();
    IOReturn validateOpenGLState();
    
    // OpenGL capability detection
    IOReturn queryHostOpenGLVersion();
    IOReturn detectOpenGLCapabilitiesFromVirtIOFeatures(bool has_resource_blob, bool has_context_init);
    
    // Advanced OpenGL version detection
    IOReturn queryVirtIOGPUOpenGLVersion(uint32_t* major, uint32_t* minor);
    IOReturn probeOpenGLVersionFromContext(uint32_t* major, uint32_t* minor);
    IOReturn inferOpenGLVersionFromFeatures(uint32_t* major, uint32_t* minor);
    
    // Accelerator capability validation
    IOReturn validateAcceleratorOpenGLSupport(bool supports_gl_3_0, bool supports_gl_3_2, bool supports_gl_4_0);
    IOReturn queryAcceleratorOpenGLCapabilities(VMAcceleratorOpenGLCapabilities* capabilities);
    IOReturn adjustCapabilitiesForAccelerator(const VMAcceleratorOpenGLCapabilities* accel_caps);
    IOReturn validateFinalOpenGLConfiguration();
    void logFinalCapabilityConfiguration();
    
    // Accelerator testing methods
    IOReturn testAcceleratorShaderSupport();
    IOReturn testMetalBridgeCompatibility();
    IOReturn testAcceleratorBufferSupport();
    IOReturn testAcceleratorTextureSupport(bool needs_gl_3_0, bool needs_gl_3_2, bool needs_gl_4_0);
    
    // OpenGL version validation methods
    bool validateAcceleratorOpenGLVersionSupport(uint32_t major, uint32_t minor);
    IOReturn adjustOpenGLVersionForAcceleratorLimitations(uint32_t* major, uint32_t* minor);
    
    // Context-based OpenGL validation methods
    IOReturn validateContextOpenGLCapabilities(uint32_t context_id, uint32_t major, uint32_t minor);
    bool attemptFallbackVersionDetection(uint32_t* context_id, uint32_t* major, uint32_t* minor, uint32_t target_major, uint32_t target_minor);
    
    // Version-specific context creation methods
    IOReturn createVersionSpecificContext(uint32_t* context_id, uint32_t major, uint32_t minor);
    IOReturn configureContextForVersion(uint32_t context_id, uint32_t major, uint32_t minor);
    IOReturn verifyContextVersionCapabilities(uint32_t context_id, uint32_t major, uint32_t minor);
    
    // Version-specific configuration methods
    // Version-specific context configuration methods
    IOReturn configureContextForOpenGL4(UInt32 context_id, const VMAcceleratorOpenGLCapabilities& capabilities);
    IOReturn configureContextForOpenGL32Plus(UInt32 context_id, const VMAcceleratorOpenGLCapabilities& capabilities);
    IOReturn configureContextForOpenGL3Legacy(UInt32 context_id, const VMAcceleratorOpenGLCapabilities& capabilities);
    IOReturn configureContextForOpenGL2(UInt32 context_id, const VMAcceleratorOpenGLCapabilities& capabilities);
    IOReturn configureContextForLegacyOpenGL(UInt32 context_id, const VMAcceleratorOpenGLCapabilities& capabilities);
    
    // 3D capability validation methods
    IOReturn validateAccelerator3DCapabilities();
    IOReturn validate3DRenderingCapabilities();
    IOReturn validateGeometryProcessingCapabilities();
    IOReturn validateFragmentProcessingCapabilities();
    IOReturn validateTextureCapabilities();
    IOReturn validateFramebufferCapabilities();
    IOReturn validateShaderCapabilities();
    
    // Advanced 3D geometry processing capabilities
    IOReturn validateAdvanced3DGeometryCapabilities(uint32_t context_id, uint32_t surface_id);
    IOReturn testMultiplePrimitiveTypes(uint32_t context_id, uint32_t surface_id);
    IOReturn testAdvancedVertexBufferOperations(uint32_t context_id, uint32_t surface_id);
    IOReturn testInstancedRenderingCapabilities(uint32_t context_id, uint32_t surface_id);
    IOReturn testTransformFeedbackCapabilities(uint32_t context_id, uint32_t surface_id);
    IOReturn testMultithreadedRenderingCapabilities(uint32_t context_id, uint32_t surface_id);
    IOReturn performGeometryPerformanceBenchmark(uint32_t context_id, uint32_t surface_id);
    
    // Advanced geometry helper methods
    IOReturn testInterleavedVertexAttributes(uint32_t context_id, uint32_t* surface_ids, size_t count);
    IOReturn testLargeVertexBufferCapabilities(uint32_t context_id, uint32_t surface_id);
    IOReturn testGeometryShaderTransformFeedback(uint32_t context_id, uint32_t geometry_shader_id);
    
    // Context feature testing methods
    IOReturn testContextBasicRendering(uint32_t context_id);
    IOReturn testContextOpenGL3Features(uint32_t context_id);
    IOReturn testContextOpenGL32Features(uint32_t context_id);
    IOReturn testContextOpenGL4Features(uint32_t context_id);
    
    // OpenGL 3.x feature testing methods using existing accelerator interface
    IOReturn queryOpenGL3SpecificFeatures(uint32_t context_id);
    IOReturn testShaderCompilationSupport(uint32_t context_id);
    IOReturn testMultipleRenderTargets(uint32_t context_id);
    IOReturn testModernTextureSupport(uint32_t context_id);
    IOReturn queryOpenGLVersionString(uint32_t context_id, char* version_string, size_t buffer_size);
    IOReturn queryOpenGLExtensions(uint32_t context_id, char* extensions_string, size_t buffer_size);
    
    // OpenGL 4.x feature testing methods using existing accelerator interface
    IOReturn testTessellationSupport(uint32_t context_id);
    IOReturn testSeparateShaderObjects(uint32_t context_id);
    IOReturn testOpenGL42AdvancedFeatures(uint32_t context_id);
    IOReturn testComputeShaderSupport(uint32_t context_id);
    IOReturn testBufferStorageSupport(uint32_t context_id);
    
    // Dynamic OpenGL version discovery methods
    IOReturn queryActualOpenGLVersion(uint32_t context_id, uint32_t* major, uint32_t* minor);
    IOReturn detectOpenGLVersionFromCapabilities(uint32_t context_id, uint32_t* major, uint32_t* minor);
    bool parseOpenGLVersionString(const char* version_string, uint32_t* major, uint32_t* minor);
    IOReturn queryVersionThroughMetal(uint32_t* major, uint32_t* minor);
    
    // OpenGL feature detection methods
    bool hasOpenGL30Features(const char* extensions);
    bool hasOpenGL31Features(const char* extensions);
    bool hasOpenGL32Features(const char* extensions);
    bool hasOpenGL40Features(const char* extensions);
    
    // Text rendering optimization (fixes flashing yellow squares)
    IOReturn optimizeTextRendering(uint32_t context_id);
    
    // Helper methods for text rendering optimization
    IOReturn enableBlending(uint32_t context_id, bool enable);
    IOReturn setBlendFunc(uint32_t context_id, uint32_t src, uint32_t dst);
    IOReturn enableDepthTest(uint32_t context_id, bool enable);
    IOReturn setTextureFiltering(uint32_t context_id, uint32_t min_filter, uint32_t mag_filter);
    IOReturn flushTextureCache(uint32_t context_id);

};

#endif /* VMOpenGLBridge_h */

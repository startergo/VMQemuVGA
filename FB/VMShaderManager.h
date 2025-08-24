#ifndef __VMShaderManager_H__
#define __VMShaderManager_H__

#include <IOKit/IOService.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include "VMVirtIOGPU.h"

// Forward declarations
class VMQemuVGAAccelerator;

// Constants
#define MAX_RENDER_CONTEXTS 32
#define MAX_SHADER_UNIFORMS 256
#define MAX_SHADER_ATTRIBUTES 32

// Shader types
enum VMShaderType {
    VM_SHADER_TYPE_VERTEX = 1,
    VM_SHADER_TYPE_FRAGMENT = 2,
    VM_SHADER_TYPE_GEOMETRY = 3,
    VM_SHADER_TYPE_TESSELLATION_CONTROL = 4,
    VM_SHADER_TYPE_TESSELLATION_EVALUATION = 5,
    VM_SHADER_TYPE_COMPUTE = 6
};

// Shader languages
enum VMShaderLanguage {
    VM_SHADER_LANG_GLSL = 1,
    VM_SHADER_LANG_HLSL = 2,
    VM_SHADER_LANG_MSL = 3,        // Metal Shading Language
    VM_SHADER_LANG_SPIRV = 4       // SPIR-V bytecode
};

// Shader compilation flags
enum VMShaderCompileFlags {
    VM_SHADER_OPTIMIZE_NONE = 0,
    VM_SHADER_OPTIMIZE_SIZE = 1 << 0,
    VM_SHADER_OPTIMIZE_PERFORMANCE = 1 << 1,
    VM_SHADER_DEBUG_INFO = 1 << 2,
    VM_SHADER_WARNINGS_AS_ERRORS = 1 << 3
};

// Shader uniform/constant descriptor
struct VMShaderUniform {
    char name[64];
    uint32_t type;        // Data type (float, int, vec3, mat4, etc.)
    uint32_t location;    // Binding location
    uint32_t size;        // Size in bytes
    uint32_t array_size;  // Array size (1 for non-arrays)
    uint32_t offset;      // Offset in uniform buffer
};

// Shader attribute descriptor
struct VMShaderAttribute {
    char name[64];
    uint32_t type;        // Data type
    uint32_t location;    // Attribute location
    uint32_t components;  // Number of components (1-4)
    uint32_t normalized;  // Whether to normalize integer data
};

// Shader resource binding
struct VMShaderResource {
    uint32_t binding;     // Binding point
    uint32_t type;        // Resource type (texture, buffer, sampler)
    uint32_t stage_mask;  // Which shader stages use this resource
    char name[64];        // Resource name
};

// Compiled shader information
struct VMCompiledShaderInfo {
    uint32_t shader_id;
    VMShaderType type;
    VMShaderLanguage source_language;
    uint32_t bytecode_size;
    uint32_t uniform_count;
    uint32_t attribute_count;
    uint32_t resource_count;
    uint32_t local_size_x;    // For compute shaders
    uint32_t local_size_y;
    uint32_t local_size_z;
    uint32_t compile_flags;
    char entry_point[64];
    uint32_t reserved[8];
};

// Program performance statistics
struct ProgramPerformanceStats {
    uint64_t total_activations;
    uint64_t total_draw_calls;
    uint64_t total_compute_dispatches;
    uint64_t context_switches;
    uint64_t link_time;
    uint64_t last_activation_time;
    uint32_t peak_uniform_buffer_usage;
    uint32_t peak_texture_unit_usage;
    float average_execution_time_ms;
    uint32_t optimization_level;
    uint32_t reserved[6];
};

// HLSL Compiler Infrastructure Detection Results
struct HLSLCompilerInfrastructure {
    bool has_dxc_compiler;              // Modern DirectX Compiler available
    bool has_fxc_fallback;              // Legacy FXC compiler available
    bool has_optimization_engine;        // Optimization capabilities
    bool has_debug_info_support;        // Debug information generation
    bool has_intermediate_validation;    // Intermediate code validation
    bool has_profile_guided_opts;       // Profile-guided optimization
    bool has_parallel_compilation;      // Multi-threaded compilation
    bool has_incremental_compilation;   // Incremental recompilation
    uint32_t max_optimization_level;    // Maximum optimization level (0-3)
    uint32_t compilation_threads;       // Available compilation threads
    float average_compile_time_ms;      // Average compilation time
    uint32_t reserved[4];
};

// SPIR-V Infrastructure Detection Results
struct SPIRVCapabilities {
    bool has_spirv_validator;           // Built-in SPIR-V bytecode validator
    bool has_spirv_optimizer;           // SPIR-V optimization passes
    bool has_spirv_cross_compiler;      // SPIR-V to GLSL/MSL cross-compilation
    bool has_vulkan_compatibility;     // Vulkan API compatibility layer
    bool has_opencl_compatibility;     // OpenCL SPIR-V support
    bool has_reflection_support;       // SPIR-V reflection and metadata extraction
    uint32_t max_spirv_version;        // Latest supported SPIR-V version
    uint32_t supported_vulkan_version; // Compatible Vulkan API version
    uint32_t reserved[4];
};

class VMShaderManager : public OSObject
{
    OSDeclareDefaultStructors(VMShaderManager);

private:
    VMQemuVGAAccelerator* m_accelerator;
    VMVirtIOGPU* m_gpu_device;
    
    // Shader storage
    OSArray* m_shaders;
    OSArray* m_programs;
    OSArray* m_context_programs;  // Programs bound to each context
    uint32_t m_next_shader_id;
    uint32_t m_next_program_id;
    uint32_t m_frame_count;
    
    IOLock* m_shader_lock;
    
    // Compiled shader entry
    struct CompiledShader {
        uint32_t shader_id;
        VMShaderType type;
        VMShaderLanguage language;
        IOBufferMemoryDescriptor* bytecode;
        VMCompiledShaderInfo info;
        OSArray* uniforms;      // Array of VMShaderUniform
        OSArray* attributes;    // Array of VMShaderAttribute
        OSArray* resources;     // Array of VMShaderResource
        uint32_t ref_count;
        bool is_valid;
    };
    
    // Shader program (multiple shaders linked together)
    struct ShaderProgram {
        uint32_t program_id;
        OSArray* shader_ids;    // Array of shader IDs in this program
        uint32_t gpu_program_id; // GPU-side program ID
        bool is_linked;
        OSArray* all_uniforms;  // Combined uniforms from all shaders
        OSArray* all_attributes;
        OSArray* all_resources;
        
        // Individual shader stage IDs
        uint32_t vertex_shader_id;
        uint32_t fragment_shader_id;
        uint32_t geometry_shader_id;
        uint32_t tessellation_control_shader_id;
        uint32_t tessellation_evaluation_shader_id;
        uint32_t compute_shader_id;
        
        // Performance and usage tracking
        uint32_t usage_count;
        uint64_t last_used_timestamp;
        uint64_t link_timestamp;
        uint32_t uniform_count;
        uint32_t attribute_count;
        uint32_t resource_count;
        bool hardware_optimized;
        
        // Performance statistics
        struct ProgramPerformanceStats* performance_stats;
    };
    
    // Internal methods
    CompiledShader* findShader(uint32_t shader_id);
    ShaderProgram* findProgram(uint32_t program_id);
    uint32_t getCurrentFrameCount() const;
    IOReturn compileShaderInternal(VMShaderType type, VMShaderLanguage language,
                                  const void* source_code, size_t source_size,
                                  uint32_t flags, CompiledShader** out_shader);
    IOReturn linkProgramInternal(uint32_t* shader_ids, uint32_t count, 
                               ShaderProgram** out_program);
    IOReturn extractShaderMetadata(CompiledShader* shader);
    IOReturn validateShaderCompatibility(uint32_t* shader_ids, uint32_t count);
    
    // Shader metadata extraction helper methods
    void extractUniformFromLine(CompiledShader* shader, const char* line);
    void extractAttributeFromLine(CompiledShader* shader, const char* line);
    IOReturn parseUniformDeclarationAdvanced(const char* declaration, char* type_out, char* name_out, size_t type_size, size_t name_size);
    uint32_t parseShaderDataType(const char* type_str);
    uint32_t calculateUniformSize(const char* type_str);
    uint32_t extractArraySize(const char* name_str);
    uint32_t calculateUniformOffset(CompiledShader* shader, uint32_t size);
    void addDefaultVertexAttributes(CompiledShader* shader);
    void addStandardVertexUniforms(CompiledShader* shader);
    void addStandardFragmentUniforms(CompiledShader* shader);
    void addTextureSamplers(CompiledShader* shader, uint32_t sampler_count);
    const char* getShaderTypeString(VMShaderType type);
    const char* getShaderLanguageString(VMShaderLanguage language);
    
    // Cross-compilation support
    IOReturn convertGLSLToMSL(const char* glsl_source, size_t source_size,
                             VMShaderType type, IOBufferMemoryDescriptor** msl_output);
    IOReturn convertHLSLToSPIRV(const char* hlsl_source, size_t source_size,
                               VMShaderType type, IOBufferMemoryDescriptor** spirv_output);
    IOReturn optimizeShaderBytecode(IOBufferMemoryDescriptor* bytecode, uint32_t flags);
    
    // Shader language support detection
    bool checkHLSLSupport() const;
    bool checkSPIRVSupport() const;
    IOReturn detectRealHLSLCompilerInfrastructure(HLSLCompilerInfrastructure* infra) const;
    
    // Helper methods for compiler detection
    bool checkFileExists(const char* path) const;
    bool checkFrameworkAvailable(const char* framework_name) const;
    bool checkDirectoryExists(const char* path) const;
    bool checkEnvironmentVariable(const char* var_name) const;
    bool checkSystemPath(const char* system_path) const;
    bool checkThirdPartyTool(const char* tool_path) const;
    bool checkGenericExecutable(const char* exec_path) const;
    bool checkSystemFrameworkPath(const char* framework_path) const;
    bool checkThirdPartyInstallation(const char* install_path) const;
    uint32_t getSystemCoreCount() const;
    float performCompilerBenchmark() const;
    
public:
    static VMShaderManager* withAccelerator(VMQemuVGAAccelerator* accelerator);
    
    virtual bool init(VMQemuVGAAccelerator* accelerator);
    virtual void free() override;
    
    // Shader compilation
    IOReturn compileShader(VMShaderType type, VMShaderLanguage language,
                          const void* source_code, size_t source_size,
                          uint32_t flags, uint32_t* shader_id);
    IOReturn destroyShader(uint32_t shader_id);
    IOReturn getShaderInfo(uint32_t shader_id, VMCompiledShaderInfo* info);
    IOReturn getShaderBytecode(uint32_t shader_id, IOMemoryDescriptor** bytecode);
    
    // Shader introspection
    IOReturn getShaderUniforms(uint32_t shader_id, VMShaderUniform* uniforms, 
                              uint32_t max_count, uint32_t* actual_count);
    IOReturn getShaderAttributes(uint32_t shader_id, VMShaderAttribute* attributes,
                               uint32_t max_count, uint32_t* actual_count);
    IOReturn getShaderResources(uint32_t shader_id, VMShaderResource* resources,
                              uint32_t max_count, uint32_t* actual_count);
    
    // Program linking
    IOReturn createProgram(uint32_t* shader_ids, uint32_t count, uint32_t* program_id);
    IOReturn destroyProgram(uint32_t program_id);
    IOReturn linkProgram(uint32_t program_id);
    IOReturn validateProgram(uint32_t program_id, bool* is_valid, char* error_log, size_t log_size);
    
    // Program usage
    IOReturn useProgram(uint32_t context_id, uint32_t program_id);
    IOReturn setUniform(uint32_t program_id, const char* name, 
                       const void* data, size_t size);
    IOReturn setUniformBuffer(uint32_t program_id, uint32_t binding, 
                            IOMemoryDescriptor* buffer);
    IOReturn bindResource(uint32_t program_id, uint32_t binding, 
                         uint32_t resource_id, uint32_t resource_type);
    
    // Shader caching
    IOReturn saveShaderCache(const char* cache_path);
    IOReturn loadShaderCache(const char* cache_path);
    IOReturn clearShaderCache();
    
    // Statistics and debugging
    uint32_t getCompiledShaderCount() const;
    uint32_t getLinkedProgramCount() const;
    uint64_t getShaderMemoryUsage() const;
    IOReturn dumpShaderInfo(uint32_t shader_id, char* buffer, size_t buffer_size);
    
    // Capabilities
    bool supportsShaderType(VMShaderType type) const;
    bool supportsShaderLanguage(VMShaderLanguage language) const;
    uint32_t getMaxShaderUniformBuffers() const;
    uint32_t getMaxShaderTextureUnits() const;
    uint32_t getMaxComputeWorkGroupSize() const;
};

#endif /* __VMShaderManager_H__ */

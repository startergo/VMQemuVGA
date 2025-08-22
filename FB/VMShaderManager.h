#ifndef __VMShaderManager_H__
#define __VMShaderManager_H__

#include <IOKit/IOService.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include "VMVirtIOGPU.h"

// Forward declarations
class VMQemuVGAAccelerator;

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

class VMShaderManager : public OSObject
{
    OSDeclareDefaultStructors(VMShaderManager);

private:
    VMQemuVGAAccelerator* m_accelerator;
    VMVirtIOGPU* m_gpu_device;
    
    // Shader storage
    OSArray* m_shaders;
    OSArray* m_programs;
    uint32_t m_next_shader_id;
    uint32_t m_next_program_id;
    
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
    };
    
    // Internal methods
    CompiledShader* findShader(uint32_t shader_id);
    ShaderProgram* findProgram(uint32_t program_id);
    IOReturn compileShaderInternal(VMShaderType type, VMShaderLanguage language,
                                  const void* source_code, size_t source_size,
                                  uint32_t flags, CompiledShader** out_shader);
    IOReturn linkProgramInternal(uint32_t* shader_ids, uint32_t count, 
                               ShaderProgram** out_program);
    IOReturn extractShaderMetadata(CompiledShader* shader);
    IOReturn validateShaderCompatibility(uint32_t* shader_ids, uint32_t count);
    
    // Cross-compilation support
    IOReturn convertGLSLToMSL(const char* glsl_source, size_t source_size,
                             VMShaderType type, IOBufferMemoryDescriptor** msl_output);
    IOReturn convertHLSLToSPIRV(const char* hlsl_source, size_t source_size,
                               VMShaderType type, IOBufferMemoryDescriptor** spirv_output);
    IOReturn optimizeShaderBytecode(IOBufferMemoryDescriptor* bytecode, uint32_t flags);
    
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

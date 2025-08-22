#include "VMShaderManager.h"
#include "VMQemuVGAAccelerator.h"
#include <IOKit/IOLib.h>

#define CLASS VMShaderManager
#define super OSObject

OSDefineMetaClassAndStructors(VMShaderManager, OSObject);

VMShaderManager* CLASS::withAccelerator(VMQemuVGAAccelerator* accelerator)
{
    VMShaderManager* manager = new VMShaderManager;
    if (manager) {
        if (!manager->init(accelerator)) {
            manager->release();
            manager = nullptr;
        }
    }
    return manager;
}

bool CLASS::init(VMQemuVGAAccelerator* accelerator)
{
    if (!super::init())
        return false;
    
    m_accelerator = accelerator;
    m_gpu_device = accelerator ? accelerator->getGPUDevice() : nullptr;
    
    m_shaders = OSArray::withCapacity(64);
    m_programs = OSArray::withCapacity(16);
    m_next_shader_id = 1;
    m_next_program_id = 1;
    
    m_shader_lock = IOLockAlloc();
    
    return (m_shaders && m_programs && m_shader_lock);
}

void CLASS::free()
{
    if (m_shader_lock) {
        IOLockFree(m_shader_lock);
        m_shader_lock = nullptr;
    }
    
    // Clean up shaders
    if (m_shaders) {
        IOLockLock(m_shader_lock);
        while (m_shaders->getCount() > 0) {
            CompiledShader* shader = (CompiledShader*)m_shaders->getObject(0);
            if (shader) {
                if (shader->bytecode) shader->bytecode->release();
                if (shader->uniforms) shader->uniforms->release();
                if (shader->attributes) shader->attributes->release();
                if (shader->resources) shader->resources->release();
                IOFree(shader, sizeof(CompiledShader));
            }
            m_shaders->removeObject(0);
        }
        IOLockUnlock(m_shader_lock);
        m_shaders->release();
    }
    
    // Clean up programs
    if (m_programs) {
        while (m_programs->getCount() > 0) {
            ShaderProgram* program = (ShaderProgram*)m_programs->getObject(0);
            if (program) {
                if (program->shader_ids) program->shader_ids->release();
                if (program->all_uniforms) program->all_uniforms->release();
                if (program->all_attributes) program->all_attributes->release();
                if (program->all_resources) program->all_resources->release();
                IOFree(program, sizeof(ShaderProgram));
            }
            m_programs->removeObject(0);
        }
        m_programs->release();
    }
    
    super::free();
}

IOReturn CLASS::compileShader(VMShaderType type, VMShaderLanguage language,
                             const void* source_code, size_t source_size,
                             uint32_t flags, uint32_t* shader_id)
{
    if (!source_code || source_size == 0 || !shader_id)
        return kIOReturnBadArgument;
    
    IOLockLock(m_shader_lock);
    
    CompiledShader* shader = nullptr;
    IOReturn ret = compileShaderInternal(type, language, source_code, source_size, 
                                        flags, &shader);
    
    if (ret == kIOReturnSuccess && shader) {
        shader->shader_id = OSIncrementAtomic(&m_next_shader_id);
        shader->ref_count = 1;
        shader->is_valid = true;
        
        m_shaders->setObject((OSObject*)shader);
        *shader_id = shader->shader_id;
        
        IOLog("VMShaderManager: Compiled shader %d (type: %d, language: %d, size: %zu bytes)\n",
              shader->shader_id, type, language, source_size);
    }
    
    IOLockUnlock(m_shader_lock);
    return ret;
}

IOReturn CLASS::compileShaderInternal(VMShaderType type, VMShaderLanguage language,
                                     const void* source_code, size_t source_size,
                                     uint32_t flags, CompiledShader** out_shader)
{
    CompiledShader* shader = (CompiledShader*)IOMalloc(sizeof(CompiledShader));
    if (!shader)
        return kIOReturnNoMemory;
    
    bzero(shader, sizeof(CompiledShader));
    shader->type = type;
    shader->language = language;
    shader->uniforms = OSArray::withCapacity(16);
    shader->attributes = OSArray::withCapacity(8);
    shader->resources = OSArray::withCapacity(8);
    
    // Create bytecode buffer
    size_t bytecode_size = source_size;
    if (language == VM_SHADER_LANG_GLSL || language == VM_SHADER_LANG_HLSL) {
        // For text-based shaders, we need to compile them
        // For now, we'll just store the source as-is and simulate compilation
        bytecode_size = source_size;
    }
    
    shader->bytecode = IOBufferMemoryDescriptor::withCapacity(bytecode_size, kIODirectionOut);
    if (!shader->bytecode) {
        IOFree(shader, sizeof(CompiledShader));
        return kIOReturnNoMemory;
    }
    
    // Copy source/bytecode
    shader->bytecode->writeBytes(0, source_code, source_size);
    
    // Fill in shader info
    shader->info.type = type;
    shader->info.source_language = language;
    shader->info.bytecode_size = static_cast<uint32_t>(bytecode_size);
    shader->info.compile_flags = flags;
    strlcpy(shader->info.entry_point, "main", sizeof(shader->info.entry_point));
    
    // Extract metadata (simplified)
    extractShaderMetadata(shader);
    
    *out_shader = shader;
    return kIOReturnSuccess;
}

IOReturn CLASS::extractShaderMetadata(CompiledShader* shader)
{
    // This is a simplified metadata extraction
    // In a real implementation, this would parse the shader source/bytecode
    
    if (shader->type == VM_SHADER_TYPE_VERTEX) {
        // Add common vertex shader attributes
        VMShaderAttribute* pos_attr = (VMShaderAttribute*)IOMalloc(sizeof(VMShaderAttribute));
        if (pos_attr) {
            strlcpy(pos_attr->name, "position", sizeof(pos_attr->name));
            pos_attr->type = 0x1406; // GL_FLOAT
            pos_attr->location = 0;
            pos_attr->components = 3;
            pos_attr->normalized = 0;
            shader->attributes->setObject((OSObject*)pos_attr);
        }
        
        VMShaderAttribute* tex_attr = (VMShaderAttribute*)IOMalloc(sizeof(VMShaderAttribute));
        if (tex_attr) {
            strlcpy(tex_attr->name, "texCoord", sizeof(tex_attr->name));
            tex_attr->type = 0x1406; // GL_FLOAT
            tex_attr->location = 1;
            tex_attr->components = 2;
            tex_attr->normalized = 0;
            shader->attributes->setObject((OSObject*)tex_attr);
        }
    }
    
    // Add common uniforms
    VMShaderUniform* mvp_uniform = (VMShaderUniform*)IOMalloc(sizeof(VMShaderUniform));
    if (mvp_uniform) {
        strlcpy(mvp_uniform->name, "mvpMatrix", sizeof(mvp_uniform->name));
        mvp_uniform->type = 0x8B5C; // GL_FLOAT_MAT4
        mvp_uniform->location = 0;
        mvp_uniform->size = 64; // 4x4 matrix of floats
        mvp_uniform->array_size = 1;
        mvp_uniform->offset = 0;
        shader->uniforms->setObject((OSObject*)mvp_uniform);
    }
    
    shader->info.uniform_count = shader->uniforms->getCount();
    shader->info.attribute_count = shader->attributes->getCount();
    shader->info.resource_count = shader->resources->getCount();
    
    return kIOReturnSuccess;
}

IOReturn CLASS::destroyShader(uint32_t shader_id)
{
    IOLockLock(m_shader_lock);
    
    CompiledShader* shader = findShader(shader_id);
    if (!shader) {
        IOLockUnlock(m_shader_lock);
        return kIOReturnNotFound;
    }
    
    shader->ref_count--;
    if (shader->ref_count == 0) {
        // Remove from array and cleanup
        for (unsigned int i = 0; i < m_shaders->getCount(); i++) {
            CompiledShader* s = (CompiledShader*)m_shaders->getObject(i);
            if (s && s->shader_id == shader_id) {
                if (s->bytecode) s->bytecode->release();
                if (s->uniforms) s->uniforms->release();
                if (s->attributes) s->attributes->release();
                if (s->resources) s->resources->release();
                m_shaders->removeObject(i);
                IOFree(s, sizeof(CompiledShader));
                break;
            }
        }
    }
    
    IOLockUnlock(m_shader_lock);
    
    IOLog("VMShaderManager: Destroyed shader %d\n", shader_id);
    return kIOReturnSuccess;
}

IOReturn CLASS::getShaderInfo(uint32_t shader_id, VMCompiledShaderInfo* info)
{
    if (!info)
        return kIOReturnBadArgument;
    
    IOLockLock(m_shader_lock);
    
    CompiledShader* shader = findShader(shader_id);
    if (!shader) {
        IOLockUnlock(m_shader_lock);
        return kIOReturnNotFound;
    }
    
    *info = shader->info;
    info->shader_id = shader_id;
    
    IOLockUnlock(m_shader_lock);
    return kIOReturnSuccess;
}

IOReturn CLASS::createProgram(uint32_t* shader_ids, uint32_t count, uint32_t* program_id)
{
    if (!shader_ids || count == 0 || !program_id)
        return kIOReturnBadArgument;
    
    IOLockLock(m_shader_lock);
    
    // Validate shader compatibility
    IOReturn ret = validateShaderCompatibility(shader_ids, count);
    if (ret != kIOReturnSuccess) {
        IOLockUnlock(m_shader_lock);
        return ret;
    }
    
    ShaderProgram* program = (ShaderProgram*)IOMalloc(sizeof(ShaderProgram));
    if (!program) {
        IOLockUnlock(m_shader_lock);
        return kIOReturnNoMemory;
    }
    
    bzero(program, sizeof(ShaderProgram));
    program->program_id = OSIncrementAtomic(&m_next_program_id);
    program->shader_ids = OSArray::withCapacity(count);
    program->all_uniforms = OSArray::withCapacity(32);
    program->all_attributes = OSArray::withCapacity(16);
    program->all_resources = OSArray::withCapacity(16);
    program->is_linked = false;
    
    // Copy shader IDs
    for (uint32_t i = 0; i < count; i++) {
        OSNumber* shader_id = OSNumber::withNumber(shader_ids[i], 32);
        if (shader_id) {
            program->shader_ids->setObject(shader_id);
            shader_id->release();
        }
    }
    
    m_programs->setObject((OSObject*)program);
    *program_id = program->program_id;
    
    IOLockUnlock(m_shader_lock);
    
    IOLog("VMShaderManager: Created program %d with %d shaders\n", program->program_id, count);
    return kIOReturnSuccess;
}

CLASS::CompiledShader* CLASS::findShader(uint32_t shader_id)
{
    for (unsigned int i = 0; i < m_shaders->getCount(); i++) {
        CompiledShader* shader = (CompiledShader*)m_shaders->getObject(i);
        if (shader && shader->shader_id == shader_id) {
            return shader;
        }
    }
    return nullptr;
}

CLASS::ShaderProgram* CLASS::findProgram(uint32_t program_id)
{
    for (unsigned int i = 0; i < m_programs->getCount(); i++) {
        ShaderProgram* program = (ShaderProgram*)m_programs->getObject(i);
        if (program && program->program_id == program_id) {
            return program;
        }
    }
    return nullptr;
}

IOReturn CLASS::validateShaderCompatibility(uint32_t* shader_ids, uint32_t count)
{
    bool has_vertex = false, has_fragment = false;
    
    // Check that we have required shader stages
    for (uint32_t i = 0; i < count; i++) {
        CompiledShader* shader = findShader(shader_ids[i]);
        if (!shader)
            return kIOReturnNotFound;
        
        if (shader->type == VM_SHADER_TYPE_VERTEX)
            has_vertex = true;
        else if (shader->type == VM_SHADER_TYPE_FRAGMENT)
            has_fragment = true;
    }
    
    // For graphics pipelines, we need at least vertex and fragment shaders
    if (!has_vertex || !has_fragment) {
        IOLog("VMShaderManager: Program missing required vertex or fragment shader\n");
        return kIOReturnBadArgument;
    }
    
    return kIOReturnSuccess;
}

uint32_t CLASS::getCompiledShaderCount() const
{
    return m_shaders ? m_shaders->getCount() : 0;
}

uint32_t CLASS::getLinkedProgramCount() const
{
    return m_programs ? m_programs->getCount() : 0;
}

bool CLASS::supportsShaderType(VMShaderType type) const
{
    switch (type) {
        case VM_SHADER_TYPE_VERTEX:
        case VM_SHADER_TYPE_FRAGMENT:
            return true;
        case VM_SHADER_TYPE_GEOMETRY:
        case VM_SHADER_TYPE_COMPUTE:
            return m_gpu_device && m_gpu_device->supports3D();
        default:
            return false;
    }
}

bool CLASS::supportsShaderLanguage(VMShaderLanguage language) const
{
    switch (language) {
        case VM_SHADER_LANG_GLSL:
        case VM_SHADER_LANG_MSL:
            return true;
        case VM_SHADER_LANG_HLSL:
        case VM_SHADER_LANG_SPIRV:
            return false; // Not implemented yet
        default:
            return false;
    }
}

#include "VMOpenGLBridge.h"
#include "VMQemuVGAAccelerator.h"
#include "VMVirtIOGPU.h"
#include "VMMetalBridge.h"
#include "VMShaderManager.h"
#include <IOKit/IOLib.h>
#include <libkern/OSAtomic.h>

#define CLASS VMOpenGLBridge
#define super OSObject

OSDefineMetaClassAndStructors(VMOpenGLBridge, OSObject);

bool CLASS::init()
{
    if (!super::init()) {
        return false;
    }
    
    m_accelerator = nullptr;
    m_gpu_device = nullptr;
    m_metal_bridge = nullptr;
    m_lock = nullptr;
    
    m_gl_contexts = nullptr;
    m_gl_buffers = nullptr;
    m_gl_textures = nullptr;
    m_gl_shaders = nullptr;
    m_gl_programs = nullptr;
    m_gl_vertex_arrays = nullptr;
    m_gl_resource_map = nullptr;
    
    m_next_gl_id = 1;
    m_current_context_id = 0;
    
    // Initialize render state
    bzero(&m_current_state, sizeof(m_current_state));
    m_current_state.clear_color[3] = 1.0f; // Alpha = 1.0
    m_current_state.clear_depth = 1.0f;
    m_current_state.depth_func = VM_GL_LESS;
    m_current_state.src_blend_factor = VM_GL_ONE;
    m_current_state.dst_blend_factor = VM_GL_ZERO;
    
    m_bound_array_buffer = 0;
    m_bound_element_array_buffer = 0;
    m_active_texture_unit = 0;
    m_current_program = 0;
    bzero(m_bound_textures, sizeof(m_bound_textures));
    
    // Initialize counters
    m_gl_draw_calls = 0;
    m_gl_state_changes = 0;
    m_gl_buffer_uploads = 0;
    m_gl_texture_uploads = 0;
    
    // Initialize feature support
    m_supports_gl_3_0 = true;
    m_supports_gl_3_2 = true;
    m_supports_gl_4_0 = false;
    m_supports_vertex_array_objects = true;
    m_supports_uniform_buffer_objects = true;
    m_supports_geometry_shaders = false;
    m_supports_tessellation = false;
    
    return true;
}

void CLASS::free()
{
    if (m_lock) {
        IORecursiveLockFree(m_lock);
        m_lock = nullptr;
    }
    
    if (m_gl_contexts) {
        m_gl_contexts->release();
        m_gl_contexts = nullptr;
    }
    
    if (m_gl_buffers) {
        m_gl_buffers->release();
        m_gl_buffers = nullptr;
    }
    
    if (m_gl_textures) {
        m_gl_textures->release();
        m_gl_textures = nullptr;
    }
    
    if (m_gl_shaders) {
        m_gl_shaders->release();
        m_gl_shaders = nullptr;
    }
    
    if (m_gl_programs) {
        m_gl_programs->release();
        m_gl_programs = nullptr;
    }
    
    if (m_gl_vertex_arrays) {
        m_gl_vertex_arrays->release();
        m_gl_vertex_arrays = nullptr;
    }
    
    if (m_gl_resource_map) {
        m_gl_resource_map->release();
        m_gl_resource_map = nullptr;
    }
    
    super::free();
}

bool CLASS::initWithAccelerator(VMQemuVGAAccelerator* accelerator)
{
    if (!accelerator) {
        return false;
    }
    
    m_accelerator = accelerator;
    m_gpu_device = accelerator->getGPUDevice();
    m_metal_bridge = accelerator->getMetalBridge();
    
    // Create lock
    m_lock = IORecursiveLockAlloc();
    if (!m_lock) {
        return false;
    }
    
    // Create resource arrays
    m_gl_contexts = OSArray::withCapacity(8);
    m_gl_buffers = OSArray::withCapacity(256);
    m_gl_textures = OSArray::withCapacity(256);
    m_gl_shaders = OSArray::withCapacity(64);
    m_gl_programs = OSArray::withCapacity(32);
    m_gl_vertex_arrays = OSArray::withCapacity(64);
    m_gl_resource_map = OSDictionary::withCapacity(1024);
    
    if (!m_gl_contexts || !m_gl_buffers || !m_gl_textures ||
        !m_gl_shaders || !m_gl_programs || !m_gl_vertex_arrays ||
        !m_gl_resource_map) {
        return false;
    }
    
    IOReturn ret = setupOpenGLSupport();
    if (ret != kIOReturnSuccess) {
        IOLog("VMOpenGLBridge: Failed to setup OpenGL support (0x%x)\n", ret);
        return false;
    }
    
    ret = configureGLFeatures();
    if (ret != kIOReturnSuccess) {
        IOLog("VMOpenGLBridge: Failed to configure GL features (0x%x)\n", ret);
        return false;
    }
    
    IOLog("VMOpenGLBridge: Initialized successfully\n");
    return true;
}

IOReturn CLASS::setupOpenGLSupport()
{
    IOLog("VMOpenGLBridge: Setting up OpenGL support\n");
    
    // Enable OpenGL/3D features on the GPU device
    if (m_gpu_device) {
        IOReturn ret = m_gpu_device->enableFeature(VIRTIO_GPU_FEATURE_3D);
        if (ret != kIOReturnSuccess) {
            IOLog("VMOpenGLBridge: Warning - 3D feature not enabled (0x%x)\n", ret);
        }
        
        // Enable OpenGL-specific features
        m_gpu_device->enableFeature(VIRTIO_GPU_FEATURE_RESOURCE_BLOB);
        m_gpu_device->enableFeature(VIRTIO_GPU_FEATURE_CONTEXT_INIT);
    }
    
    IOLog("VMOpenGLBridge: OpenGL support setup completed\n");
    return kIOReturnSuccess;
}

IOReturn CLASS::configureGLFeatures()
{
    IOLog("VMOpenGLBridge: Configuring OpenGL features\n");
    
    // Query host OpenGL capabilities through VirtIO GPU
    // For now, assume reasonable OpenGL 3.2 Core Profile support
    m_supports_gl_3_0 = true;
    m_supports_gl_3_2 = true;
    m_supports_gl_4_0 = false; // Conservative
    m_supports_vertex_array_objects = true;
    m_supports_uniform_buffer_objects = true;
    m_supports_geometry_shaders = false; // Advanced feature
    m_supports_tessellation = false; // Advanced feature
    
    IOLog("VMOpenGLBridge: Feature configuration:\n");
    IOLog("  OpenGL 3.0: %s\n", m_supports_gl_3_0 ? "Yes" : "No");
    IOLog("  OpenGL 3.2: %s\n", m_supports_gl_3_2 ? "Yes" : "No");
    IOLog("  OpenGL 4.0: %s\n", m_supports_gl_4_0 ? "Yes" : "No");
    IOLog("  Vertex Array Objects: %s\n", m_supports_vertex_array_objects ? "Yes" : "No");
    IOLog("  Uniform Buffer Objects: %s\n", m_supports_uniform_buffer_objects ? "Yes" : "No");
    IOLog("  Geometry Shaders: %s\n", m_supports_geometry_shaders ? "Yes" : "No");
    IOLog("  Tessellation: %s\n", m_supports_tessellation ? "Yes" : "No");
    
    return kIOReturnSuccess;
}

IOReturn CLASS::createContext(const VMGLContextDescriptor* descriptor, uint32_t* context_id)
{
    if (!descriptor || !context_id) {
        return kIOReturnBadArgument;
    }
    
    IORecursiveLockLock(m_lock);
    
    // Create 3D context through accelerator
    uint32_t accel_context_id;
    IOReturn ret = m_accelerator->create3DContext(&accel_context_id, current_task());
    if (ret != kIOReturnSuccess) {
        IORecursiveLockUnlock(m_lock);
        return ret;
    }
    
    // Create OpenGL context object
    *context_id = allocateGLId();
    OSNumber* context = OSNumber::withNumber(*context_id, 32);
    if (context) {
        m_gl_contexts->setObject(context);
        
        // Map context in resource dictionary using string key
        char context_key[32];
        snprintf(context_key, sizeof(context_key), "%u", *context_id);
        m_gl_resource_map->setObject(context_key, context);
        context->release();
    }
    
    IORecursiveLockUnlock(m_lock);
    
    IOLog("VMOpenGLBridge: Created OpenGL context %d (accelerator context: %d)\n", 
          *context_id, accel_context_id);
    return kIOReturnSuccess;
}

IOReturn CLASS::makeContextCurrent(uint32_t context_id)
{
    IORecursiveLockLock(m_lock);
    
    if (context_id == 0) {
        // Make no context current
        m_current_context_id = 0;
        IORecursiveLockUnlock(m_lock);
        return kIOReturnSuccess;
    }
    
    // Validate context exists
    OSObject* context = findGLResource(context_id);
    if (!context) {
        IORecursiveLockUnlock(m_lock);
        return kIOReturnNotFound;
    }
    
    m_current_context_id = context_id;
    
    // Sync state with accelerator
    IOReturn ret = syncGLState();
    
    IORecursiveLockUnlock(m_lock);
    
    IOLog("VMOpenGLBridge: Made context %d current\n", context_id);
    return ret;
}

IOReturn CLASS::genBuffers(uint32_t count, uint32_t* buffer_ids)
{
    if (!buffer_ids || count == 0) {
        return kIOReturnBadArgument;
    }
    
    IORecursiveLockLock(m_lock);
    
    for (uint32_t i = 0; i < count; i++) {
        buffer_ids[i] = allocateGLId();
        
        // Create buffer through Metal bridge or directly through accelerator
        uint32_t metal_buffer_id = 0;
        if (m_metal_bridge) {
            // Create a default buffer descriptor
            VMMetalBufferDescriptor desc;
            desc.length = 4096; // Default size, will be resized on first use
            desc.resource_options = 0;
            desc.storage_mode = 0; // MTLStorageModeShared
            desc.cpu_cache_mode = 0;
            desc.hazard_tracking_mode = 0;
            
            IOReturn ret = m_metal_bridge->createBuffer(1, &desc, nullptr, &metal_buffer_id);
            if (ret == kIOReturnSuccess) {
                // Map GL buffer ID to Metal buffer ID
                OSNumber* gl_id = OSNumber::withNumber(buffer_ids[i], 32);
                OSNumber* metal_id = OSNumber::withNumber(metal_buffer_id, 32);
                if (gl_id && metal_id) {
                    // Use string key for dictionary
                    char gl_key[32];
                    snprintf(gl_key, sizeof(gl_key), "%u", buffer_ids[i]);
                    m_gl_resource_map->setObject(gl_key, metal_id);
                    m_gl_buffers->setObject(metal_id);
                    gl_id->release();
                    metal_id->release();
                }
            }
        }
        
        IOLog("VMOpenGLBridge: Generated buffer %d (Metal: %d)\n", buffer_ids[i], metal_buffer_id);
    }
    
    IORecursiveLockUnlock(m_lock);
    return kIOReturnSuccess;
}

IOReturn CLASS::bindBuffer(VMGLBufferTarget target, uint32_t buffer_id)
{
    IORecursiveLockLock(m_lock);
    
    switch (target) {
        case VM_GL_ARRAY_BUFFER:
            m_bound_array_buffer = buffer_id;
            break;
        case VM_GL_ELEMENT_ARRAY_BUFFER:
            m_bound_element_array_buffer = buffer_id;
            break;
        default:
            IORecursiveLockUnlock(m_lock);
            return kIOReturnBadArgument;
    }
    
    m_gl_state_changes++;
    IORecursiveLockUnlock(m_lock);
    
    return kIOReturnSuccess;
}

IOReturn CLASS::bufferData(VMGLBufferTarget target, const VMGLBufferDescriptor* descriptor)
{
    if (!descriptor) {
        return kIOReturnBadArgument;
    }
    
    IORecursiveLockLock(m_lock);
    
    uint32_t buffer_id = 0;
    switch (target) {
        case VM_GL_ARRAY_BUFFER:
            buffer_id = m_bound_array_buffer;
            break;
        case VM_GL_ELEMENT_ARRAY_BUFFER:
            buffer_id = m_bound_element_array_buffer;
            break;
        default:
            IORecursiveLockUnlock(m_lock);
            return kIOReturnBadArgument;
    }
    
    if (buffer_id == 0) {
        IORecursiveLockUnlock(m_lock);
        return kIOReturnNotReady;
    }
    
    // Update buffer through Metal bridge
    IOReturn ret = kIOReturnSuccess;
    if (m_metal_bridge) {
        char gl_key[32];
        snprintf(gl_key, sizeof(gl_key), "%u", buffer_id);
        OSNumber* metal_id = (OSNumber*)m_gl_resource_map->getObject(gl_key);
        if (metal_id) {
            uint32_t metal_buffer_id = metal_id->unsigned32BitValue();
            ret = m_metal_bridge->updateBuffer(metal_buffer_id, descriptor->data, 0, descriptor->size);
        }
    }
    
    if (ret == kIOReturnSuccess) {
        m_gl_buffer_uploads++;
    }
    
    IORecursiveLockUnlock(m_lock);
    
    IOLog("VMOpenGLBridge: Updated buffer %d with %d bytes\n", buffer_id, descriptor->size);
    return ret;
}

IOReturn CLASS::createShader(const VMGLShaderDescriptor* descriptor, uint32_t* shader_id)
{
    if (!descriptor || !shader_id) {
        return kIOReturnBadArgument;
    }
    
    IORecursiveLockLock(m_lock);
    
    *shader_id = allocateGLId();
    
    // Create shader through accelerator's shader manager
    if (m_accelerator && m_accelerator->getShaderManager()) {
        uint32_t accel_shader_id;
        IOReturn ret = m_accelerator->getShaderManager()->compileShader(
            (VMShaderType)descriptor->shader_type, VM_SHADER_LANG_GLSL, 
            descriptor->source_code, descriptor->source_length, 
            VM_SHADER_OPTIMIZE_PERFORMANCE, &accel_shader_id);
        
        if (ret == kIOReturnSuccess) {
            // Map GL shader to accelerator shader using string key
            OSNumber* accel_id = OSNumber::withNumber(accel_shader_id, 32);
            if (accel_id) {
                char shader_key[32];
                snprintf(shader_key, sizeof(shader_key), "%u", *shader_id);
                m_gl_resource_map->setObject(shader_key, accel_id);
                m_gl_shaders->setObject(accel_id);
                accel_id->release();
            }
        }
        
        IORecursiveLockUnlock(m_lock);
        return ret;
    }
    
    IORecursiveLockUnlock(m_lock);
    
    IOLog("VMOpenGLBridge: Created shader %d (type: %d)\n", *shader_id, descriptor->shader_type);
    return kIOReturnSuccess;
}

IOReturn CLASS::drawArrays(VMGLPrimitiveType mode, uint32_t first, uint32_t count)
{
    if (m_current_context_id == 0) {
        return kIOReturnNotReady;
    }
    
    IORecursiveLockLock(m_lock);
    
    IOReturn ret = kIOReturnSuccess;
    
    // Translate OpenGL draw call to accelerator
    if (m_accelerator) {
        ret = m_accelerator->drawPrimitives(m_current_context_id, mode, count, first);
    }
    
    // Also submit through Metal bridge if available
    if (ret == kIOReturnSuccess && m_metal_bridge) {
        VMMetalDrawPrimitivesDescriptor metal_desc;
        metal_desc.primitive_type = mode;
        metal_desc.vertex_start = first;
        metal_desc.vertex_count = count;
        metal_desc.instance_count = 1;
        metal_desc.base_instance = 0;
        
        // Use a default command buffer
        uint32_t cmd_buffer_id = 1;
        m_metal_bridge->drawPrimitives(cmd_buffer_id, &metal_desc);
    }
    
    if (ret == kIOReturnSuccess) {
        m_gl_draw_calls++;
    }
    
    IORecursiveLockUnlock(m_lock);
    
    IOLog("VMOpenGLBridge: Draw arrays - mode: %d, first: %d, count: %d\n", mode, first, count);
    return ret;
}

IOReturn CLASS::clear(uint32_t mask)
{
    if (m_current_context_id == 0) {
        return kIOReturnNotReady;
    }
    
    IORecursiveLockLock(m_lock);
    
    // Submit clear command through accelerator
    IOReturn ret = kIOReturnSuccess;
    if (m_accelerator) {
        // Clear color buffer
        if (mask & 0x4000) { // GL_COLOR_BUFFER_BIT
            ret = m_accelerator->clearColorBuffer(m_current_context_id, 
                m_current_state.clear_color[0], m_current_state.clear_color[1],
                m_current_state.clear_color[2], m_current_state.clear_color[3]);
        }
        
        // Clear depth buffer
        if ((mask & 0x0100) && ret == kIOReturnSuccess) { // GL_DEPTH_BUFFER_BIT
            ret = m_accelerator->clearDepthBuffer(m_current_context_id, m_current_state.clear_depth);
        }
    }
    
    IORecursiveLockUnlock(m_lock);
    
    IOLog("VMOpenGLBridge: Clear buffers (mask: 0x%x)\n", mask);
    return ret;
}

bool CLASS::supportsGLVersion(uint32_t major, uint32_t minor)
{
    if (major < 3) return true; // Legacy OpenGL always supported
    if (major == 3 && minor == 0) return m_supports_gl_3_0;
    if (major == 3 && minor <= 2) return m_supports_gl_3_2;
    if (major == 4 && minor == 0) return m_supports_gl_4_0;
    return false;
}

IOReturn CLASS::getGLPerformanceStats(void* stats_buffer, size_t* buffer_size)
{
    if (!stats_buffer || !buffer_size) {
        return kIOReturnBadArgument;
    }
    
    struct GLPerformanceStats {
        uint64_t draw_calls;
        uint64_t state_changes;
        uint64_t buffer_uploads;
        uint64_t texture_uploads;
        uint32_t active_buffers;
        uint32_t active_textures;
        uint32_t active_shaders;
        uint32_t active_programs;
        uint32_t current_context;
    };
    
    if (*buffer_size < sizeof(GLPerformanceStats)) {
        *buffer_size = sizeof(GLPerformanceStats);
        return kIOReturnNoSpace;
    }
    
    GLPerformanceStats stats;
    stats.draw_calls = m_gl_draw_calls;
    stats.state_changes = m_gl_state_changes;
    stats.buffer_uploads = m_gl_buffer_uploads;
    stats.texture_uploads = m_gl_texture_uploads;
    stats.active_buffers = m_gl_buffers ? m_gl_buffers->getCount() : 0;
    stats.active_textures = m_gl_textures ? m_gl_textures->getCount() : 0;
    stats.active_shaders = m_gl_shaders ? m_gl_shaders->getCount() : 0;
    stats.active_programs = m_gl_programs ? m_gl_programs->getCount() : 0;
    stats.current_context = m_current_context_id;
    
    bcopy(&stats, stats_buffer, sizeof(stats));
    *buffer_size = sizeof(stats);
    
    return kIOReturnSuccess;
}

void CLASS::logOpenGLBridgeState()
{
    IOLog("VMOpenGLBridge State:\n");
    IOLog("  Current Context: %d\n", m_current_context_id);
    IOLog("  Draw Calls: %llu\n", m_gl_draw_calls);
    IOLog("  State Changes: %llu\n", m_gl_state_changes);
    IOLog("  Buffer Uploads: %llu\n", m_gl_buffer_uploads);
    IOLog("  Texture Uploads: %llu\n", m_gl_texture_uploads);
    IOLog("  Active Buffers: %d\n", m_gl_buffers ? m_gl_buffers->getCount() : 0);
    IOLog("  Active Textures: %d\n", m_gl_textures ? m_gl_textures->getCount() : 0);
    IOLog("  Active Shaders: %d\n", m_gl_shaders ? m_gl_shaders->getCount() : 0);
    IOLog("  Active Programs: %d\n", m_gl_programs ? m_gl_programs->getCount() : 0);
    IOLog("  Bound Array Buffer: %d\n", m_bound_array_buffer);
    IOLog("  Bound Element Buffer: %d\n", m_bound_element_array_buffer);
    IOLog("  Current Program: %d\n", m_current_program);
    IOLog("  OpenGL 3.0 Support: %s\n", m_supports_gl_3_0 ? "Yes" : "No");
    IOLog("  OpenGL 3.2 Support: %s\n", m_supports_gl_3_2 ? "Yes" : "No");
}

// Private helper methods

OSObject* CLASS::findGLResource(uint32_t resource_id)
{
    char key_str[32];
    snprintf(key_str, sizeof(key_str), "%u", resource_id);
    
    OSObject* resource = m_gl_resource_map->getObject(key_str);
    
    return resource;
}

uint32_t CLASS::allocateGLId()
{
    return OSIncrementAtomic(&m_next_gl_id);
}

IOReturn CLASS::syncGLState()
{
    // Sync current OpenGL state with the underlying accelerator
    IOReturn ret = kIOReturnSuccess;
    
    if (m_accelerator && m_current_context_id != 0) {
        // Sync depth testing
        if (m_current_state.depth_test_enabled) {
            ret = m_accelerator->enableDepthTest(m_current_context_id, true);
        }
        
        // Sync blending
        if (m_current_state.blend_enabled && ret == kIOReturnSuccess) {
            ret = m_accelerator->enableBlending(m_current_context_id, true);
        }
        
        // Additional state synchronization can be added here
    }
    
    return ret;
}

void CLASS::updatePerformanceCounters(const char* operation)
{
    // Update performance counters based on operation type
    if (strcmp(operation, "draw") == 0) {
        m_gl_draw_calls++;
    } else if (strcmp(operation, "state") == 0) {
        m_gl_state_changes++;
    } else if (strcmp(operation, "buffer") == 0) {
        m_gl_buffer_uploads++;
    } else if (strcmp(operation, "texture") == 0) {
        m_gl_texture_uploads++;
    }
}

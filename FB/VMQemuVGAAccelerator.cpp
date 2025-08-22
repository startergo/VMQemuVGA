#include "VMQemuVGAAccelerator.h"
#include "VMQemuVGA.h"
#include "VMShaderManager.h"
#include "VMTextureManager.h"
#include "VMCommandBuffer.h"
#include <IOKit/IOLib.h>
#include <IOKit/IOBufferMemoryDescriptor.h>

#define CLASS VMQemuVGAAccelerator
#define super IOService

OSDefineMetaClassAndStructors(VMQemuVGAAccelerator, IOService);

bool CLASS::init(OSDictionary* properties)
{
    if (!super::init(properties))
        return false;
    
    m_framebuffer = nullptr;
    m_gpu_device = nullptr;
    m_workloop = nullptr;
    m_command_gate = nullptr;
    m_lock = IOLockAlloc();
    
    m_shader_manager = nullptr;
    m_texture_manager = nullptr;
    m_command_pool = nullptr;
    
    m_contexts = OSArray::withCapacity(16);
    m_surfaces = OSArray::withCapacity(64);
    m_next_context_id = 1;
    m_next_surface_id = 1;
    
    m_draw_calls = 0;
    m_triangles_rendered = 0;
    m_memory_used = 0;
    
    return (m_lock && m_contexts && m_surfaces);
}

void CLASS::free()
{
    // Clean up advanced managers first
    if (m_shader_manager) {
        m_shader_manager->release();
        m_shader_manager = nullptr;
    }
    
    if (m_texture_manager) {
        m_texture_manager->release();
        m_texture_manager = nullptr;
    }
    
    if (m_command_pool) {
        m_command_pool->release();
        m_command_pool = nullptr;
    }
    
    if (m_lock) {
        IOLockFree(m_lock);
        m_lock = nullptr;
    }
    
    OSSafeReleaseNULL(m_contexts);
    OSSafeReleaseNULL(m_surfaces);
    
    super::free();
}

bool CLASS::start(IOService* provider)
{
    IOLog("VMQemuVGAAccelerator::start\n");
    
    if (!super::start(provider))
        return false;
    
    m_framebuffer = OSDynamicCast(VMQemuVGA, provider);
    if (!m_framebuffer) {
        IOLog("VMQemuVGAAccelerator: Provider is not VMQemuVGA\n");
        return false;
    }
    
    m_gpu_device = m_framebuffer->getGPUDevice();
    if (!m_gpu_device) {
        IOLog("VMQemuVGAAccelerator: No GPU device available\n");
        return false;
    }
    
    // Create workloop and command gate
    m_workloop = IOWorkLoop::workLoop();
    if (!m_workloop) {
        IOLog("VMQemuVGAAccelerator: Failed to create workloop\n");
        return false;
    }
    
    m_command_gate = IOCommandGate::commandGate(this);
    if (!m_command_gate) {
        IOLog("VMQemuVGAAccelerator: Failed to create command gate\n");
        return false;
    }
    
    m_workloop->addEventSource(m_command_gate);
    
    // Initialize advanced 3D managers
    m_shader_manager = VMShaderManager::withAccelerator(this);
    if (!m_shader_manager) {
        IOLog("VMQemuVGAAccelerator: Failed to create shader manager\n");
        return false;
    }
    
    m_texture_manager = VMTextureManager::withAccelerator(this);
    if (!m_texture_manager) {
        IOLog("VMQemuVGAAccelerator: Failed to create texture manager\n");
        return false;
    }
    
    m_command_pool = VMCommandBufferPool::withAccelerator(this, 0, 16);
    if (!m_command_pool) {
        IOLog("VMQemuVGAAccelerator: Failed to create command buffer pool\n");
        return false;
    }
    
    // Set device properties
    setProperty("IOClass", "VMQemuVGAAccelerator");
    setProperty("3D Hardware Acceleration", true);
    setProperty("Max Contexts", 16U, 32);
    setProperty("Max Surfaces", 64U, 32);
    setProperty("Supports Shaders", supportsShaders());
    setProperty("Max Texture Size", getMaxTextureSize(), 32);
    setProperty("Shader Manager", "Enabled");
    setProperty("Texture Manager", "Enabled");
    setProperty("Command Buffer Pool", "Enabled");
    setProperty("Advanced Features", "Phase 2 Complete");
    
    IOLog("VMQemuVGAAccelerator: Started successfully\n");
    return true;
}

void CLASS::stop(IOService* provider)
{
    IOLog("VMQemuVGAAccelerator::stop\n");
    
    // Clean up all contexts and surfaces
    IOLockLock(m_lock);
    
    // Destroy all contexts
    while (m_contexts->getCount() > 0) {
        AccelContext* context = (AccelContext*)m_contexts->getObject(0);
        if (context) {
            destroyContextInternal(context->context_id);
        }
        m_contexts->removeObject(0);
    }
    
    // Clean up surfaces
    while (m_surfaces->getCount() > 0) {
        AccelSurface* surface = (AccelSurface*)m_surfaces->getObject(0);
        if (surface) {
            if (surface->backing_memory) {
                surface->backing_memory->release();
            }
            IOFree(surface, sizeof(AccelSurface));
        }
        m_surfaces->removeObject(0);
    }
    
    IOLockUnlock(m_lock);
    
    if (m_command_gate && m_workloop) {
        m_workloop->removeEventSource(m_command_gate);
        m_command_gate->release();
        m_command_gate = nullptr;
    }
    
    if (m_workloop) {
        m_workloop->release();
        m_workloop = nullptr;
    }
    
    super::stop(provider);
}

IOReturn CLASS::newUserClient(task_t owningTask, void* securityID,
                             UInt32 type, IOUserClient** handler)
{
    IOReturn ret = kIOReturnSuccess;
    VMQemuVGA3DUserClient* client = nullptr;
    
    if (type != 0) {
        return kIOReturnBadArgument;
    }
    
    client = VMQemuVGA3DUserClient::withTask(owningTask);
    if (!client) {
        ret = kIOReturnNoMemory;
        goto exit;
    }
    
    if (!client->attach(this)) {
        ret = kIOReturnError;
        goto exit;
    }
    
    if (!client->start(this)) {
        client->detach(this);
        ret = kIOReturnError;
        goto exit;
    }
    
    *handler = client;
    
exit:
    if (ret != kIOReturnSuccess && client) {
        client->release();
    }
    
    return ret;
}

IOReturn CLASS::create3DContext(uint32_t* context_id, task_t task)
{
    if (!context_id)
        return kIOReturnBadArgument;
    
    IOLockLock(m_lock);
    
    // Create GPU context
    uint32_t gpu_context_id;
    IOReturn ret = m_gpu_device->createRenderContext(&gpu_context_id);
    if (ret != kIOReturnSuccess) {
        IOLockUnlock(m_lock);
        return ret;
    }
    
    // Create accelerator context
    AccelContext* context = (AccelContext*)IOMalloc(sizeof(AccelContext));
    if (!context) {
        m_gpu_device->destroyRenderContext(gpu_context_id);
        IOLockUnlock(m_lock);
        return kIOReturnNoMemory;
    }
    
    context->context_id = OSIncrementAtomic(&m_next_context_id);
    context->gpu_context_id = gpu_context_id;
    context->active = true;
    context->surfaces = OSSet::withCapacity(8);
    context->command_buffer = nullptr;
    context->owning_task = task;
    
    m_contexts->setObject((OSObject*)context);
    *context_id = context->context_id;
    
    IOLockUnlock(m_lock);
    
    IOLog("VMQemuVGAAccelerator: Created 3D context %d\n", *context_id);
    return kIOReturnSuccess;
}

IOReturn CLASS::create3DSurface(uint32_t context_id, VM3DSurfaceInfo* surface_info)
{
    if (!surface_info)
        return kIOReturnBadArgument;
    
    IOLockLock(m_lock);
    
    AccelContext* context = findContext(context_id);
    if (!context) {
        IOLockUnlock(m_lock);
        return kIOReturnNotFound;
    }
    
    // Allocate GPU resource
    uint32_t gpu_resource_id;
    IOReturn ret = m_gpu_device->allocateResource3D(&gpu_resource_id, 
                                                   VIRTIO_GPU_RESOURCE_TARGET_2D,
                                                   surface_info->format,
                                                   surface_info->width,
                                                   surface_info->height, 1);
    if (ret != kIOReturnSuccess) {
        IOLockUnlock(m_lock);
        return ret;
    }
    
    // Create surface
    AccelSurface* surface = (AccelSurface*)IOMalloc(sizeof(AccelSurface));
    if (!surface) {
        m_gpu_device->deallocateResource(gpu_resource_id);
        IOLockUnlock(m_lock);
        return kIOReturnNoMemory;
    }
    
    surface->surface_id = OSIncrementAtomic(&m_next_surface_id);
    surface->gpu_resource_id = gpu_resource_id;
    surface->info = *surface_info;
    surface->info.surface_id = surface->surface_id;
    surface->backing_memory = nullptr;
    surface->is_render_target = false;
    
    // Allocate backing memory
    ret = allocateSurfaceMemory(&surface->info, &surface->backing_memory);
    if (ret != kIOReturnSuccess) {
        m_gpu_device->deallocateResource(gpu_resource_id);
        IOFree(surface, sizeof(AccelSurface));
        IOLockUnlock(m_lock);
        return ret;
    }
    
    m_surfaces->setObject((OSObject*)surface);
    context->surfaces->setObject((OSObject*)surface);
    
    m_memory_used += calculateSurfaceSize(&surface->info);
    
    IOLockUnlock(m_lock);
    
    IOLog("VMQemuVGAAccelerator: Created 3D surface %d (%dx%d)\n", 
          surface->surface_id, surface_info->width, surface_info->height);
    
    return kIOReturnSuccess;
}

IOReturn CLASS::submit3DCommands(uint32_t context_id, IOMemoryDescriptor* commands)
{
    if (!commands)
        return kIOReturnBadArgument;
    
    IOLockLock(m_lock);
    
    AccelContext* context = findContext(context_id);
    if (!context) {
        IOLockUnlock(m_lock);
        return kIOReturnNotFound;
    }
    
    // Execute commands via GPU device
    IOReturn ret = m_gpu_device->executeCommands(context->gpu_context_id, commands);
    
    if (ret == kIOReturnSuccess) {
        m_draw_calls++;
        // Estimate triangle count (simplified)
        m_triangles_rendered += static_cast<uint32_t>(commands->getLength() / 64);
    }
    
    IOLockUnlock(m_lock);
    
    return ret;
}

CLASS::AccelContext* CLASS::findContext(uint32_t context_id)
{
    for (unsigned int i = 0; i < m_contexts->getCount(); i++) {
        AccelContext* context = (AccelContext*)m_contexts->getObject(i);
        if (context && context->context_id == context_id) {
            return context;
        }
    }
    return nullptr;
}

CLASS::AccelSurface* CLASS::findSurface(uint32_t surface_id)
{
    for (unsigned int i = 0; i < m_surfaces->getCount(); i++) {
        AccelSurface* surface = (AccelSurface*)m_surfaces->getObject(i);
        if (surface && surface->surface_id == surface_id) {
            return surface;
        }
    }
    return nullptr;
}

IOReturn CLASS::allocateSurfaceMemory(VM3DSurfaceInfo* info, IOMemoryDescriptor** memory)
{
    if (!info || !memory)
        return kIOReturnBadArgument;
    
    uint32_t size = calculateSurfaceSize(info);
    *memory = IOBufferMemoryDescriptor::withCapacity(size, kIODirectionInOut);
    
    return (*memory) ? kIOReturnSuccess : kIOReturnNoMemory;
}

uint32_t CLASS::calculateSurfaceSize(VM3DSurfaceInfo* info)
{
    uint32_t bpp = 4; // Assume 32-bit for simplicity
    
    switch (info->format) {
        case VM3D_FORMAT_R5G6B5:
            bpp = 2;
            break;
        case VM3D_FORMAT_A8R8G8B8:
        case VM3D_FORMAT_X8R8G8B8:
        default:
            bpp = 4;
            break;
    }
    
    return info->width * info->height * bpp;
}

bool CLASS::supportsShaders() const
{
    return m_gpu_device && m_gpu_device->supports3D();
}

bool CLASS::supportsHardwareTransform() const
{
    return supportsShaders();
}

bool CLASS::supportsMultisample() const
{
    return false; // Not implemented yet
}

uint32_t CLASS::getMaxTextureSize() const
{
    return 4096; // Reasonable default
}

uint32_t CLASS::getMaxRenderTargets() const
{
    return 4; // Multiple render targets
}

void CLASS::resetStatistics()
{
    IOLockLock(m_lock);
    m_draw_calls = 0;
    m_triangles_rendered = 0;
    IOLockUnlock(m_lock);
}

IOReturn CLASS::setPowerState(unsigned long powerState, IOService* whatDevice)
{
    IOLog("VMQemuVGAAccelerator: Power state %lu\n", powerState);
    return kIOReturnSuccess;
}

IOReturn CLASS::destroyContextInternal(uint32_t context_id)
{
    // Find and remove context
    for (unsigned int i = 0; i < m_contexts->getCount(); i++) {
        AccelContext* context = (AccelContext*)m_contexts->getObject(i);
        if (context && context->context_id == context_id) {
            // Cleanup GPU context
            if (m_gpu_device) {
                m_gpu_device->destroyRenderContext(context->gpu_context_id);
            }
            
            // Cleanup surfaces
            if (context->surfaces) {
                context->surfaces->release();
            }
            
            // Cleanup command buffer
            if (context->command_buffer) {
                context->command_buffer->release();
            }
            
            // Remove from array
            m_contexts->removeObject(i);
            IOFree(context, sizeof(AccelContext));
            return kIOReturnSuccess;
        }
    }
    return kIOReturnNotFound;
}

IOReturn CLASS::destroy3DContext(uint32_t context_id)
{
    IOLockLock(m_lock);
    IOReturn ret = destroyContextInternal(context_id);
    IOLockUnlock(m_lock);
    return ret;
}

IOReturn CLASS::present3DSurface(uint32_t context_id, uint32_t surface_id)
{
    IOLockLock(m_lock);
    
    AccelContext* context = findContext(context_id);
    if (!context) {
        IOLockUnlock(m_lock);
        return kIOReturnNotFound;
    }
    
    AccelSurface* surface = findSurface(surface_id);
    if (!surface) {
        IOLockUnlock(m_lock);
        return kIOReturnNotFound;
    }
    
    // In a real implementation, this would copy the surface to the framebuffer
    // For now, we'll just mark it as presented
    IOLog("VMQemuVGAAccelerator: Present surface %d from context %d\n", 
          surface_id, context_id);
    
    IOLockUnlock(m_lock);
    return kIOReturnSuccess;
}

// Advanced 3D API implementations

IOReturn CLASS::compileShader(uint32_t context_id, uint32_t shader_type, uint32_t language,
                             const void* source_code, size_t source_size, uint32_t* shader_id)
{
    if (!m_shader_manager || !source_code || !shader_id)
        return kIOReturnBadArgument;
    
    AccelContext* context = findContext(context_id);
    if (!context)
        return kIOReturnNotFound;
    
    IOReturn ret = m_shader_manager->compileShader(
        (VMShaderType)shader_type,
        (VMShaderLanguage)language,
        source_code,
        source_size,
        0, // flags
        shader_id
    );
    
    IOLog("VMQemuVGAAccelerator: Compiled shader %d for context %d (result: 0x%x)\n",
          *shader_id, context_id, ret);
    
    return ret;
}

IOReturn CLASS::destroyShader(uint32_t context_id, uint32_t shader_id)
{
    if (!m_shader_manager)
        return kIOReturnBadArgument;
    
    AccelContext* context = findContext(context_id);
    if (!context)
        return kIOReturnNotFound;
    
    return m_shader_manager->destroyShader(shader_id);
}

IOReturn CLASS::createShaderProgram(uint32_t context_id, uint32_t* shader_ids, uint32_t count,
                                   uint32_t* program_id)
{
    if (!m_shader_manager || !shader_ids || !program_id)
        return kIOReturnBadArgument;
    
    AccelContext* context = findContext(context_id);
    if (!context)
        return kIOReturnNotFound;
    
    IOReturn ret = m_shader_manager->createProgram(shader_ids, count, program_id);
    
    if (ret == kIOReturnSuccess) {
        ret = m_shader_manager->linkProgram(*program_id);
    }
    
    IOLog("VMQemuVGAAccelerator: Created shader program %d for context %d (result: 0x%x)\n",
          *program_id, context_id, ret);
    
    return ret;
}

IOReturn CLASS::useShaderProgram(uint32_t context_id, uint32_t program_id)
{
    if (!m_shader_manager)
        return kIOReturnBadArgument;
    
    AccelContext* context = findContext(context_id);
    if (!context)
        return kIOReturnNotFound;
    
    return m_shader_manager->useProgram(context_id, program_id);
}

IOReturn CLASS::createTexture(uint32_t context_id, const void* descriptor, 
                             const void* initial_data, uint32_t* texture_id)
{
    if (!m_texture_manager || !descriptor || !texture_id)
        return kIOReturnBadArgument;
    
    AccelContext* context = findContext(context_id);
    if (!context)
        return kIOReturnNotFound;
    
    const VMTextureDescriptor* tex_desc = (const VMTextureDescriptor*)descriptor;
    IOMemoryDescriptor* data_desc = nullptr;
    
    if (initial_data) {
        // Create memory descriptor for initial data
        uint32_t data_size = tex_desc->width * tex_desc->height * 4; // Assume 32-bit for simplicity
        data_desc = IOMemoryDescriptor::withAddressRange(
            (vm_address_t)initial_data, data_size, kIODirectionOut, current_task());
    }
    
    IOReturn ret = m_texture_manager->createTexture(tex_desc, data_desc, texture_id);
    
    if (data_desc) {
        data_desc->release();
    }
    
    IOLog("VMQemuVGAAccelerator: Created texture %d for context %d (%dx%d, result: 0x%x)\n",
          *texture_id, context_id, tex_desc->width, tex_desc->height, ret);
    
    return ret;
}

IOReturn CLASS::beginRenderPass(uint32_t context_id, uint32_t framebuffer_id)
{
    AccelContext* context = findContext(context_id);
    if (!context)
        return kIOReturnNotFound;
    
    IOLog("VMQemuVGAAccelerator: Begin render pass for context %d, framebuffer %d\n",
          context_id, framebuffer_id);
    
    // In a real implementation, this would set up GPU render state
    return kIOReturnSuccess;
}

IOReturn CLASS::endRenderPass(uint32_t context_id)
{
    AccelContext* context = findContext(context_id);
    if (!context)
        return kIOReturnNotFound;
    
    IOLog("VMQemuVGAAccelerator: End render pass for context %d\n", context_id);
    
    // In a real implementation, this would finalize rendering
    return kIOReturnSuccess;
}

IOReturn CLASS::drawPrimitives(uint32_t context_id, uint32_t primitive_type, 
                              uint32_t vertex_count, uint32_t first_vertex)
{
    AccelContext* context = findContext(context_id);
    if (!context)
        return kIOReturnNotFound;
    
    // Submit draw command via GPU device
    IOReturn ret = m_gpu_device->executeCommands(context->gpu_context_id, nullptr);
    
    if (ret == kIOReturnSuccess) {
        m_draw_calls++;
        m_triangles_rendered += vertex_count / 3; // Approximate triangle count
    }
    
    IOLog("VMQemuVGAAccelerator: Draw %d vertices (type %d) for context %d\n",
          vertex_count, primitive_type, context_id);
    
    return ret;
}

// Statistics and debugging methods
IOReturn CLASS::getPerformanceStats(void* stats_buffer, size_t* buffer_size)
{
    if (!stats_buffer || !buffer_size)
        return kIOReturnBadArgument;
    
    struct PerformanceStats {
        uint64_t contexts_created;
        uint64_t surfaces_created;
        uint64_t commands_submitted;
        uint64_t draw_calls;
        uint64_t triangles_rendered;
        uint64_t memory_allocated;
    } stats;
    
    if (*buffer_size < sizeof(stats)) {
        *buffer_size = sizeof(stats);
        return kIOReturnNoSpace;
    }
    
    stats.contexts_created = m_contexts ? m_contexts->getCount() : 0;
    stats.surfaces_created = m_surfaces ? m_surfaces->getCount() : 0;
    stats.commands_submitted = m_commands_submitted;
    stats.draw_calls = m_draw_calls;
    stats.triangles_rendered = m_triangles_rendered;
    stats.memory_allocated = m_memory_allocated;
    
    bcopy(&stats, stats_buffer, sizeof(stats));
    *buffer_size = sizeof(stats);
    
    return kIOReturnSuccess;
}

bool CLASS::supportsAcceleration(uint32_t feature_flags)
{
    // Check if we support requested features
    bool supports_3d = (feature_flags & 0x01) != 0;
    bool supports_compute = (feature_flags & 0x02) != 0;
    bool supports_texture_compression = (feature_flags & 0x04) != 0;
    bool supports_metal = (feature_flags & 0x08) != 0;
    
    // We support all basic features in this enhanced implementation
    if (supports_3d && !m_gpu_device)
        return false;
    
    if (supports_compute && !m_command_pool)
        return false;
    
    if (supports_texture_compression && !m_texture_manager)
        return false;
    
    if (supports_metal && !m_metal_compatible)
        return false;
    
    return true;
}

void CLASS::logAcceleratorState()
{
    IOLog("VMQemuVGAAccelerator State:\n");
    IOLog("  GPU Device: %s\n", m_gpu_device ? "Available" : "Not Available");
    IOLog("  Active Contexts: %d\n", m_contexts ? m_contexts->getCount() : 0);
    IOLog("  Active Surfaces: %d\n", m_surfaces ? m_surfaces->getCount() : 0);
    IOLog("  Commands Submitted: %u\n", m_commands_submitted);
    IOLog("  Draw Calls: %u\n", m_draw_calls);
    IOLog("  Triangles Rendered: %u\n", m_triangles_rendered);
    IOLog("  Memory Allocated: %llu KB\n", m_memory_allocated / 1024);
    IOLog("  Shader Manager: %s\n", m_shader_manager ? "Available" : "Not Available");
    IOLog("  Texture Manager: %s\n", m_texture_manager ? "Available" : "Not Available");
    IOLog("  Command Buffer Pool: %s\n", m_command_pool ? "Available" : "Not Available");
    IOLog("  Metal Compatible: %s\n", m_metal_compatible ? "Yes" : "No");
}

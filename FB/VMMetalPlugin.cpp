#include "VMMetalPlugin.h"
#include "VMQemuVGAAccelerator.h"
#include "VMVirtIOGPU.h"
#include <IOKit/IOLib.h>
#include <IOKit/IOBufferMemoryDescriptor.h>

#define CLASS VMMetalPlugin
#define super IOService

OSDefineMetaClassAndStructors(VMMetalPlugin, IOService);

bool CLASS::init(OSDictionary* properties)
{
    if (!super::init(properties))
        return false;
    
    m_accelerator = nullptr;
    m_lock = IOLockAlloc();
    
    // Initialize capabilities (conservative software renderer values)
    m_capability_flags = VM_METAL_CAP_UNIFIED_MEMORY;
    m_feature_set = VM_METAL_FEATURE_SET_MACOS_GPU_FAMILY_1_V1;
    m_max_texture_width = 4096;
    m_max_texture_height = 4096;
    m_max_threads_per_threadgroup = 256;
    m_supports_unified_memory = true;
    m_supports_shader_debugging = false;
    
    // Initialize resource tracking
    m_command_queues = OSArray::withCapacity(4);
    m_buffers = OSArray::withCapacity(64);
    m_textures = OSArray::withCapacity(128);
    m_allocated_memory = 0;
    m_recommended_max_working_set_size = 256 * 1024 * 1024; // 256 MB
    
    return (m_lock && m_command_queues && m_buffers && m_textures);
}

void CLASS::free()
{
    if (m_lock) {
        IOLockFree(m_lock);
        m_lock = nullptr;
    }
    
    OSSafeReleaseNULL(m_command_queues);
    OSSafeReleaseNULL(m_buffers);
    OSSafeReleaseNULL(m_textures);
    
    super::free();
}

bool CLASS::start(IOService* provider)
{
    IOLog("VMMetalPlugin::start - Initializing minimal Metal device\n");
    
    if (!super::start(provider))
        return false;
    
    // Accept either VirtIO (VMVirtIOGPUAccelerator) or QXL (VMQemuVGAAccelerator) as provider
    // Check VirtIO FIRST since it inherits from VMQemuVGAAccelerator
    VMVirtIOGPUAccelerator* virtioAccel = OSDynamicCast(VMVirtIOGPUAccelerator, provider);
    if (virtioAccel) {
        IOLog("VMMetalPlugin: Provider is VMVirtIOGPUAccelerator (VirtIO GPU)\n");
        m_accelerator = virtioAccel;
    } else {
        m_accelerator = OSDynamicCast(VMQemuVGAAccelerator, provider);
        if (m_accelerator) {
            IOLog("VMMetalPlugin: Provider is VMQemuVGAAccelerator (QXL)\n");
        } else {
            IOLog("VMMetalPlugin: Provider is not a valid accelerator\n");
            return false;
        }
    }
    
    // Register as Metal plugin
    setProperty("IOClass", "VMMetalPlugin");
    setProperty("MetalPluginClassName", "VMMetalPlugin");
    setProperty("MetalPluginName", "VMware/QEMU Virtual Graphics Metal Software Renderer");
    setProperty("MetalDeviceName", getDeviceName());
    setProperty("MetalFamily", "GPU Family 1");
    setProperty("MetalFeatureSet", (unsigned long long)m_feature_set, 32);
    setProperty("MetalSupportsUnifiedMemory", m_supports_unified_memory);
    setProperty("MetalDeviceID", getRegistryID(), 64);
    
    // Critical properties for WindowServer compatibility
    setProperty("IOAccelIndex", (unsigned long long)0, 32);
    setProperty("IOAccelRevision", (unsigned long long)1, 32);
    setProperty("IOAccelTypes", (unsigned long long)2, 32); // 2 = Metal compatible
    
    OSArray* perfStats = OSArray::withCapacity(0);
    setProperty("PerformanceStatistics", perfStats); // Empty but non-null
    if (perfStats) perfStats->release();
    
    logDeviceCapabilities();
    
    IOLog("VMMetalPlugin: Started successfully - Metal device ready\n");
    return true;
}

void CLASS::stop(IOService* provider)
{
    IOLog("VMMetalPlugin::stop\n");
    super::stop(provider);
}

void* CLASS::createMetalDevice()
{
    IOLog("VMMetalPlugin::createMetalDevice - Creating pseudo-Metal device\n");
    
    // CRITICAL: Return a valid pointer (self) instead of NULL
    // This prevents WindowServer from calling abort()
    
    // Return this object as the Metal device
    // WindowServer just needs a non-NULL pointer to proceed
    void* device_ptr = (void*)this;
    
    IOLog("VMMetalPlugin: Metal device created successfully at %p\n", device_ptr);
    IOLog("VMMetalPlugin: Device name: %s\n", getDeviceName());
    IOLog("VMMetalPlugin: Feature set: 0x%x\n", m_feature_set);
    IOLog("VMMetalPlugin: Registry ID: 0x%llx\n", getRegistryID());
    
    return device_ptr;
}

const char* CLASS::getDeviceName()
{
    return "VMware/QEMU Virtual Graphics Adapter (Metal Software Renderer)";
}

bool CLASS::supportsFeatureSet(uint32_t featureSet)
{
    // Support basic macOS GPU Family 1 v1 feature set
    return (featureSet <= VM_METAL_FEATURE_SET_MACOS_GPU_FAMILY_1_V1);
}

bool CLASS::supportsFamily(uint32_t gpuFamily, uint32_t version)
{
    // Support GPU Family 1 only
    return (gpuFamily == 1 && version == 1);
}

uint64_t CLASS::getRegistryID()
{
    // Return our IORegistryEntry ID
    return getRegistryEntryID();
}

bool CLASS::isRemovable()
{
    return false; // Integrated virtual device
}

bool CLASS::isHeadless()
{
    return false; // Has display output via framebuffer
}

bool CLASS::isLowPower()
{
    return true; // Software renderer is "low power"
}

uint64_t CLASS::recommendedMaxWorkingSetSize()
{
    return m_recommended_max_working_set_size;
}

bool CLASS::hasUnifiedMemory()
{
    return true; // Software renderer uses system RAM
}

uint64_t CLASS::currentAllocatedSize()
{
    return m_allocated_memory;
}

uint32_t CLASS::maxThreadsPerThreadgroup()
{
    return m_max_threads_per_threadgroup;
}

void* CLASS::newCommandQueue()
{
    IOLog("VMMetalPlugin::newCommandQueue\n");
    
    VMMetalCommandQueue* queue = (VMMetalCommandQueue*)IOMalloc(sizeof(VMMetalCommandQueue));
    if (!queue)
        return nullptr;
    
    queue->queue_id = m_command_queues->getCount();
    queue->device = this;
    queue->command_buffers = OSArray::withCapacity(16);
    queue->lock = IOLockAlloc();
    queue->is_active = true;
    
    m_command_queues->setObject((OSObject*)queue);
    
    IOLog("VMMetalPlugin: Command queue %d created\n", queue->queue_id);
    return (void*)queue;
}

void* CLASS::newBuffer(uint64_t length, uint32_t options)
{
    IOLog("VMMetalPlugin::newBuffer - length: %llu, options: 0x%x\n", length, options);
    
    VMMetalBuffer* buffer = (VMMetalBuffer*)IOMalloc(sizeof(VMMetalBuffer));
    if (!buffer)
        return nullptr;
    
    buffer->buffer_id = m_buffers->getCount();
    buffer->length = length;
    buffer->options = options;
    buffer->memory = IOBufferMemoryDescriptor::withCapacity(length, kIODirectionInOut);
    buffer->contents = nullptr;
    buffer->is_mapped = false;
    
    if (!buffer->memory) {
        IOFree(buffer, sizeof(VMMetalBuffer));
        return nullptr;
    }
    
    m_buffers->setObject((OSObject*)buffer);
    m_allocated_memory += length;
    
    IOLog("VMMetalPlugin: Buffer %d created (%llu bytes)\n", buffer->buffer_id, length);
    return (void*)buffer;
}

void* CLASS::newTexture(void* descriptor)
{
    IOLog("VMMetalPlugin::newTexture\n");
    
    if (!descriptor)
        return nullptr;
    
    VMMetalTexture* texture = (VMMetalTexture*)IOMalloc(sizeof(VMMetalTexture));
    if (!texture)
        return nullptr;
    
    // Initialize with default values (descriptor would be parsed in real implementation)
    texture->texture_id = m_textures->getCount();
    texture->texture_type = 2; // MTLTextureType2D
    texture->pixel_format = 80; // MTLPixelFormatBGRA8Unorm
    texture->width = 1024;
    texture->height = 768;
    texture->depth = 1;
    texture->mipmap_level_count = 1;
    texture->sample_count = 1;
    texture->array_length = 1;
    texture->usage = VM_METAL_TEXTURE_USAGE_RENDER_TARGET | VM_METAL_TEXTURE_USAGE_SHADER_READ;
    
    uint64_t texture_size = texture->width * texture->height * 4; // BGRA8
    texture->memory = IOBufferMemoryDescriptor::withCapacity(texture_size, kIODirectionInOut);
    
    if (!texture->memory) {
        IOFree(texture, sizeof(VMMetalTexture));
        return nullptr;
    }
    
    m_textures->setObject((OSObject*)texture);
    m_allocated_memory += texture_size;
    
    IOLog("VMMetalPlugin: Texture %d created (%dx%d)\n", 
          texture->texture_id, texture->width, texture->height);
    return (void*)texture;
}

bool CLASS::supportsTextureSampleCount(uint32_t sampleCount)
{
    // Support 1x (no MSAA) and 4x MSAA
    return (sampleCount == 1 || sampleCount == 4);
}

uint64_t CLASS::minimumLinearTextureAlignmentForPixelFormat(uint32_t format)
{
    return 256; // Standard alignment
}

uint64_t CLASS::minimumTextureBufferAlignmentForPixelFormat(uint32_t format)
{
    return 256; // Standard alignment
}

uint64_t CLASS::maxBufferLength()
{
    return 256 * 1024 * 1024; // 256 MB max buffer
}

bool CLASS::areProgrammableSamplePositionsSupported()
{
    return false; // Not supported in basic implementation
}

bool CLASS::areRasterOrderGroupsSupported()
{
    return false; // Not supported in basic implementation
}

bool CLASS::supportsShaderBarycentricCoordinates()
{
    return false; // Not supported in basic implementation
}

void CLASS::updateMemoryStatistics()
{
    IOLockLock(m_lock);
    
    // Calculate current memory usage
    uint64_t buffer_memory = 0;
    uint64_t texture_memory = 0;
    
    for (unsigned int i = 0; i < m_buffers->getCount(); i++) {
        VMMetalBuffer* buffer = (VMMetalBuffer*)m_buffers->getObject(i);
        if (buffer) {
            buffer_memory += buffer->length;
        }
    }
    
    for (unsigned int i = 0; i < m_textures->getCount(); i++) {
        VMMetalTexture* texture = (VMMetalTexture*)m_textures->getObject(i);
        if (texture && texture->memory) {
            texture_memory += texture->memory->getLength();
        }
    }
    
    m_allocated_memory = buffer_memory + texture_memory;
    
    IOLockUnlock(m_lock);
}

void CLASS::logDeviceCapabilities()
{
    IOLog("VMMetalPlugin Device Capabilities:\n");
    IOLog("  Device Name: %s\n", getDeviceName());
    IOLog("  Registry ID: 0x%llx\n", getRegistryID());
    IOLog("  Feature Set: 0x%x (GPU Family 1 v1)\n", m_feature_set);
    IOLog("  Max Texture Size: %dx%d\n", m_max_texture_width, m_max_texture_height);
    IOLog("  Max Threads Per Threadgroup: %d\n", m_max_threads_per_threadgroup);
    IOLog("  Unified Memory: %s\n", m_supports_unified_memory ? "Yes" : "No");
    IOLog("  Shader Debugging: %s\n", m_supports_shader_debugging ? "Yes" : "No");
    IOLog("  Recommended Max Working Set: %llu MB\n", m_recommended_max_working_set_size / (1024 * 1024));
    IOLog("  Is Removable: %s\n", isRemovable() ? "Yes" : "No");
    IOLog("  Is Headless: %s\n", isHeadless() ? "Yes" : "No");
    IOLog("  Is Low Power: %s\n", isLowPower() ? "Yes" : "No");
}

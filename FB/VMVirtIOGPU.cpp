#include "VMVirtIOGPU.h"
#include <IOKit/IOLib.h>
#include <IOKit/IOBufferMemoryDescriptor.h>

#define CLASS VMVirtIOGPU
#define super IOService

OSDefineMetaClassAndStructors(VMVirtIOGPU, IOService);

bool CLASS::init(OSDictionary* properties)
{
    if (!super::init(properties))
        return false;
    
    m_pci_device = nullptr;
    m_config_map = nullptr;
    m_notify_map = nullptr;
    m_command_gate = nullptr;
    
    m_control_queue = nullptr;
    m_cursor_queue = nullptr;
    m_control_queue_size = 256;
    m_cursor_queue_size = 16;
    
    m_resources = OSArray::withCapacity(64);
    m_contexts = OSArray::withCapacity(16);
    m_next_resource_id = 1;
    m_next_context_id = 1;
    
    m_resource_lock = IOLockAlloc();
    m_context_lock = IOLockAlloc();
    
    return (m_resources && m_contexts && m_resource_lock && m_context_lock);
}

void CLASS::free()
{
    if (m_resource_lock) {
        IOLockFree(m_resource_lock);
        m_resource_lock = nullptr;
    }
    
    if (m_context_lock) {
        IOLockFree(m_context_lock);
        m_context_lock = nullptr;
    }
    
    OSSafeReleaseNULL(m_resources);
    OSSafeReleaseNULL(m_contexts);
    
    super::free();
}

bool CLASS::start(IOService* provider)
{
    IOLog("VMVirtIOGPU::start\n");
    
    if (!super::start(provider))
        return false;
    
    m_pci_device = OSDynamicCast(IOPCIDevice, provider);
    if (!m_pci_device) {
        IOLog("VMVirtIOGPU: Provider is not a PCI device\n");
        return false;
    }
    
    // Enable PCI device
    m_pci_device->setMemoryEnable(true);
    m_pci_device->setIOEnable(true);
    m_pci_device->setBusMasterEnable(true);
    
    if (!initVirtIOGPU()) {
        IOLog("VMVirtIOGPU: Failed to initialize VirtIO GPU\n");
        return false;
    }
    
    // Create command gate for serializing operations
    m_command_gate = IOCommandGate::commandGate(this);
    if (!m_command_gate) {
        IOLog("VMVirtIOGPU: Failed to create command gate\n");
        return false;
    }
    
    getWorkLoop()->addEventSource(m_command_gate);
    
    // Set device properties
    setProperty("3D Acceleration", "VirtIO GPU");
    setProperty("Vendor", "Red Hat, Inc.");
    setProperty("Device", "VirtIO GPU");
    
    IOLog("VMVirtIOGPU: Started successfully with %d scanouts, 3D support: %s\n", 
          m_max_scanouts, supports3D() ? "Yes" : "No");
    
    return true;
}

void CLASS::stop(IOService* provider)
{
    IOLog("VMVirtIOGPU::stop\n");
    
    if (m_command_gate) {
        getWorkLoop()->removeEventSource(m_command_gate);
        m_command_gate->release();
        m_command_gate = nullptr;
    }
    
    cleanupVirtIOGPU();
    
    super::stop(provider);
}

bool CLASS::initVirtIOGPU()
{
    // Map PCI configuration spaces
    m_config_map = m_pci_device->mapDeviceMemoryWithIndex(0);
    if (!m_config_map) {
        IOLog("VMVirtIOGPU: Failed to map configuration space\n");
        return false;
    }
    
    // Read device configuration
    volatile struct virtio_gpu_config* config = 
        (volatile struct virtio_gpu_config*)m_config_map->getVirtualAddress();
    
    m_max_scanouts = config->num_scanouts;
    m_num_capsets = config->num_capsets;
    
    IOLog("VMVirtIOGPU: Device config - scanouts: %d, capsets: %d\n", 
          m_max_scanouts, m_num_capsets);
    
    // Allocate command queues
    m_control_queue = IOBufferMemoryDescriptor::withCapacity(
        m_control_queue_size * sizeof(virtio_gpu_ctrl_hdr), kIODirectionInOut);
    if (!m_control_queue) {
        IOLog("VMVirtIOGPU: Failed to allocate control queue\n");
        return false;
    }
    
    m_cursor_queue = IOBufferMemoryDescriptor::withCapacity(
        m_cursor_queue_size * sizeof(virtio_gpu_ctrl_hdr), kIODirectionInOut);
    if (!m_cursor_queue) {
        IOLog("VMVirtIOGPU: Failed to allocate cursor queue\n");
        return false;
    }
    
    return true;
}

void CLASS::cleanupVirtIOGPU()
{
    OSSafeReleaseNULL(m_control_queue);
    OSSafeReleaseNULL(m_cursor_queue);
    
    if (m_config_map) {
        m_config_map->release();
        m_config_map = nullptr;
    }
    
    if (m_notify_map) {
        m_notify_map->release();
        m_notify_map = nullptr;
    }
}

IOReturn CLASS::createResource2D(uint32_t resource_id, uint32_t format, 
                                uint32_t width, uint32_t height)
{
    IOLockLock(m_resource_lock);
    
    // Check if resource already exists
    if (findResource(resource_id)) {
        IOLockUnlock(m_resource_lock);
        return kIOReturnBadArgument;
    }
    
    // Create command
    struct virtio_gpu_resource_create_2d cmd = {};
    cmd.hdr.type = VIRTIO_GPU_CMD_RESOURCE_CREATE_2D;
    cmd.hdr.flags = 0;
    cmd.hdr.fence_id = 0;
    cmd.hdr.ctx_id = 0;
    cmd.resource_id = resource_id;
    cmd.format = format;
    cmd.width = width;
    cmd.height = height;
    
    struct virtio_gpu_ctrl_hdr resp = {};
    IOReturn ret = submitCommand(&cmd.hdr, sizeof(cmd), &resp, sizeof(resp));
    
    if (ret == kIOReturnSuccess && resp.type == VIRTIO_GPU_RESP_OK_NODATA) {
        // Create resource entry
        gpu_resource* resource = (gpu_resource*)IOMalloc(sizeof(gpu_resource));
        if (resource) {
            resource->resource_id = resource_id;
            resource->width = width;
            resource->height = height;
            resource->format = format;
            resource->backing_memory = nullptr;
            resource->is_3d = false;
            
            m_resources->setObject((OSObject*)resource);
        }
    }
    
    IOLockUnlock(m_resource_lock);
    return ret;
}

IOReturn CLASS::createResource3D(uint32_t resource_id, uint32_t target,
                                uint32_t format, uint32_t bind,
                                uint32_t width, uint32_t height, uint32_t depth)
{
    if (!supports3D()) {
        return kIOReturnUnsupported;
    }
    
    IOLockLock(m_resource_lock);
    
    // Check if resource already exists
    if (findResource(resource_id)) {
        IOLockUnlock(m_resource_lock);
        return kIOReturnBadArgument;
    }
    
    // Create command
    struct virtio_gpu_resource_create_3d cmd = {};
    cmd.hdr.type = VIRTIO_GPU_CMD_RESOURCE_CREATE_3D;
    cmd.hdr.flags = 0;
    cmd.hdr.fence_id = 0;
    cmd.hdr.ctx_id = 0;
    cmd.resource_id = resource_id;
    cmd.target = target;
    cmd.format = format;
    cmd.bind = bind;
    cmd.width = width;
    cmd.height = height;
    cmd.depth = depth;
    cmd.array_size = 1;
    cmd.last_level = 0;
    cmd.nr_samples = 0;
    cmd.flags = 0;
    
    struct virtio_gpu_ctrl_hdr resp = {};
    IOReturn ret = submitCommand(&cmd.hdr, sizeof(cmd), &resp, sizeof(resp));
    
    if (ret == kIOReturnSuccess && resp.type == VIRTIO_GPU_RESP_OK_NODATA) {
        // Create resource entry
        gpu_resource* resource = (gpu_resource*)IOMalloc(sizeof(gpu_resource));
        if (resource) {
            resource->resource_id = resource_id;
            resource->width = width;
            resource->height = height;
            resource->format = format;
            resource->backing_memory = nullptr;
            resource->is_3d = true;
            
            m_resources->setObject((OSObject*)resource);
        }
    }
    
    IOLockUnlock(m_resource_lock);
    return ret;
}

IOReturn CLASS::submitCommand(virtio_gpu_ctrl_hdr* cmd, size_t cmd_size, 
                             virtio_gpu_ctrl_hdr* resp, size_t resp_size)
{
    // Simplified command submission - in real implementation this would
    // use VirtIO queue management
    IOReturn ret = kIOReturnSuccess;
    
    // For now, simulate successful command execution
    if (resp) {
        resp->type = VIRTIO_GPU_RESP_OK_NODATA;
        resp->flags = 0;
        resp->fence_id = cmd->fence_id;
        resp->ctx_id = cmd->ctx_id;
    }
    
    return ret;
}

VMVirtIOGPU::gpu_resource* CLASS::findResource(uint32_t resource_id)
{
    // Linear search through resources array
    // In production, use a hash table for better performance
    for (unsigned int i = 0; i < m_resources->getCount(); i++) {
        gpu_resource* resource = (gpu_resource*)m_resources->getObject(i);
        if (resource && resource->resource_id == resource_id) {
            return resource;
        }
    }
    return nullptr;
}

VMVirtIOGPU::gpu_3d_context* CLASS::findContext(uint32_t context_id)
{
    // Linear search through contexts array
    for (unsigned int i = 0; i < m_contexts->getCount(); i++) {
        gpu_3d_context* context = (gpu_3d_context*)m_contexts->getObject(i);
        if (context && context->context_id == context_id) {
            return context;
        }
    }
    return nullptr;
}

IOReturn CLASS::allocateResource3D(uint32_t* resource_id, uint32_t target, uint32_t format,
                                  uint32_t width, uint32_t height, uint32_t depth)
{
    if (!resource_id)
        return kIOReturnBadArgument;
    
    *resource_id = OSIncrementAtomic(&m_next_resource_id);
    
    return createResource3D(*resource_id, target, format, 0, width, height, depth);
}

IOReturn CLASS::createRenderContext(uint32_t* context_id)
{
    if (!context_id || !supports3D())
        return kIOReturnBadArgument;
    
    IOLockLock(m_context_lock);
    
    *context_id = OSIncrementAtomic(&m_next_context_id);
    
    // Create VirtIO GPU context
    struct virtio_gpu_ctx_create cmd = {};
    cmd.hdr.type = VIRTIO_GPU_CMD_CTX_CREATE;
    cmd.hdr.ctx_id = *context_id;
    cmd.nlen = snprintf(cmd.debug_name, sizeof(cmd.debug_name), "macOS_3D_ctx_%d", *context_id);
    
    struct virtio_gpu_ctrl_hdr resp = {};
    IOReturn ret = submitCommand(&cmd.hdr, sizeof(cmd), &resp, sizeof(resp));
    
    if (ret == kIOReturnSuccess) {
        gpu_3d_context* context = (gpu_3d_context*)IOMalloc(sizeof(gpu_3d_context));
        if (context) {
            context->context_id = *context_id;
            context->resource_id = 0;
            context->active = true;
            context->command_buffer = nullptr;
            
            m_contexts->setObject((OSObject*)context);
        }
    }
    
    IOLockUnlock(m_context_lock);
    return ret;
}

IOReturn CLASS::executeCommands(uint32_t context_id, IOMemoryDescriptor* commands)
{
    if (!supports3D() || !commands)
        return kIOReturnBadArgument;
    
    IOLockLock(m_context_lock);
    
    gpu_3d_context* context = findContext(context_id);
    if (!context) {
        IOLockUnlock(m_context_lock);
        return kIOReturnNotFound;
    }
    
    // Submit 3D commands
    struct virtio_gpu_cmd_submit cmd = {};
    cmd.hdr.type = VIRTIO_GPU_CMD_SUBMIT_3D;
    cmd.hdr.ctx_id = context_id;
    cmd.size = static_cast<uint32_t>(commands->getLength());
    
    struct virtio_gpu_ctrl_hdr resp = {};
    IOReturn ret = submitCommand(&cmd.hdr, sizeof(cmd), &resp, sizeof(resp));
    
    IOLockUnlock(m_context_lock);
    return ret;
}

IOReturn CLASS::setupScanout(uint32_t scanout_id, uint32_t width, uint32_t height)
{
    if (scanout_id >= m_max_scanouts)
        return kIOReturnBadArgument;
    
    // Create a 2D resource for the scanout
    uint32_t resource_id = OSIncrementAtomic(&m_next_resource_id);
    IOReturn ret = createResource2D(resource_id, VIRTIO_GPU_FORMAT_B8G8R8A8_UNORM, 
                                   width, height);
    if (ret != kIOReturnSuccess)
        return ret;
    
    // Set scanout
    struct virtio_gpu_set_scanout cmd = {};
    cmd.hdr.type = VIRTIO_GPU_CMD_SET_SCANOUT;
    cmd.scanout_id = scanout_id;
    cmd.resource_id = resource_id;
    cmd.r.x = 0;
    cmd.r.y = 0;
    cmd.r.width = width;
    cmd.r.height = height;
    
    struct virtio_gpu_ctrl_hdr resp = {};
    return submitCommand(&cmd.hdr, sizeof(cmd), &resp, sizeof(resp));
}

IOReturn CLASS::allocateGPUMemory(size_t size, IOMemoryDescriptor** memory)
{
    if (!memory)
        return kIOReturnBadArgument;
    
    *memory = IOBufferMemoryDescriptor::withCapacity(size, kIODirectionInOut);
    return (*memory) ? kIOReturnSuccess : kIOReturnNoMemory;
}

IOReturn CLASS::deallocateResource(uint32_t resource_id)
{
    IOLockLock(m_resource_lock);
    
    gpu_resource* resource = findResource(resource_id);
    if (!resource) {
        IOLockUnlock(m_resource_lock);
        return kIOReturnNotFound;
    }
    
    // Send unref command to GPU
    struct virtio_gpu_resource_unref cmd = {};
    cmd.hdr.type = VIRTIO_GPU_CMD_RESOURCE_UNREF;
    cmd.resource_id = resource_id;
    
    struct virtio_gpu_ctrl_hdr resp = {};
    IOReturn ret = submitCommand(&cmd.hdr, sizeof(cmd), &resp, sizeof(resp));
    
    if (ret == kIOReturnSuccess) {
        // Remove from resources array
        for (unsigned int i = 0; i < m_resources->getCount(); i++) {
            gpu_resource* res = (gpu_resource*)m_resources->getObject(i);
            if (res && res->resource_id == resource_id) {
                if (res->backing_memory) {
                    res->backing_memory->release();
                }
                m_resources->removeObject(i);
                IOFree(res, sizeof(gpu_resource));
                break;
            }
        }
    }
    
    IOLockUnlock(m_resource_lock);
    return ret;
}

IOReturn CLASS::destroyRenderContext(uint32_t context_id)
{
    if (!supports3D())
        return kIOReturnUnsupported;
    
    IOLockLock(m_context_lock);
    
    gpu_3d_context* context = findContext(context_id);
    if (!context) {
        IOLockUnlock(m_context_lock);
        return kIOReturnNotFound;
    }
    
    // Send destroy context command
    struct virtio_gpu_ctx_destroy cmd = {};
    cmd.hdr.type = VIRTIO_GPU_CMD_CTX_DESTROY;
    cmd.hdr.ctx_id = context_id;
    
    struct virtio_gpu_ctrl_hdr resp = {};
    IOReturn ret = submitCommand(&cmd.hdr, sizeof(cmd), &resp, sizeof(resp));
    
    if (ret == kIOReturnSuccess) {
        // Remove from contexts array
        for (unsigned int i = 0; i < m_contexts->getCount(); i++) {
            gpu_3d_context* ctx = (gpu_3d_context*)m_contexts->getObject(i);
            if (ctx && ctx->context_id == context_id) {
                if (ctx->command_buffer) {
                    ctx->command_buffer->release();
                }
                m_contexts->removeObject(i);
                IOFree(ctx, sizeof(gpu_3d_context));
                break;
            }
        }
    }
    
    IOLockUnlock(m_context_lock);
    return ret;
}

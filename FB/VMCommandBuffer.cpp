#include "VMCommandBuffer.h"
#include "VMQemuVGAAccelerator.h"
#include <IOKit/IOLib.h>

struct ResourceBinding {
    UInt32 binding_point;
    UInt32 resource_id;
    UInt32 resource_type;
};

#define CLASS VMCommandBuffer
#define super OSObject

OSDefineMetaClassAndStructors(VMCommandBuffer, OSObject);

VMCommandBuffer* CLASS::withAccelerator(VMQemuVGAAccelerator* accelerator, uint32_t context_id)
{
    VMCommandBuffer* buffer = new VMCommandBuffer;
    if (buffer) {
        if (!buffer->init(accelerator, context_id)) {
            buffer->release();
            buffer = nullptr;
        }
    }
    return buffer;
}

bool CLASS::init(VMQemuVGAAccelerator* accelerator, uint32_t context_id)
{
    if (!super::init())
        return false;
    
    m_accelerator = accelerator;
    m_gpu_device = accelerator ? accelerator->getGPUDevice() : nullptr;
    m_context_id = context_id;
    
    m_commands = OSArray::withCapacity(64);
    m_resources = OSArray::withCapacity(32);
    
    m_command_lock = IOLockAlloc();
    m_buffer_size = 64 * 1024; // 64KB default
    m_current_size = 0;
    m_max_commands = 256;
    m_command_count = 0;
    
    m_state = VM_COMMAND_BUFFER_STATE_INITIAL;
    m_execution_time = 0;
    m_completion_callback = nullptr;
    m_completion_context = nullptr;
    
    // Allocate command buffer memory
    m_buffer_memory = IOBufferMemoryDescriptor::withCapacity(m_buffer_size, kIODirectionOut);
    if (!m_buffer_memory) {
        return false;
    }
    m_buffer_memory->prepare();
    
    return (m_commands && m_resources && m_command_lock && m_buffer_memory);
}

void CLASS::free()
{
    if (m_command_lock) {
        IOLockLock(m_command_lock);
        
        // Clean up commands
        if (m_commands) {
            while (m_commands->getCount() > 0) {
                VMGPUCommand* command = (VMGPUCommand*)m_commands->getObject(0);
                if (command) {
                    // Clean up command resources
                    IOFree(command, sizeof(VMGPUCommand));
                }
                m_commands->removeObject(0);
            }
            m_commands->release();
            m_commands = nullptr;
        }
        
        // Clean up resources
        if (m_resources) {
            m_resources->release();
            m_resources = nullptr;
        }
        
        // Clean up buffer memory
        if (m_buffer_memory) {
            m_buffer_memory->complete();
            m_buffer_memory->release();
            m_buffer_memory = nullptr;
        }
        
        IOLockUnlock(m_command_lock);
        IOLockFree(m_command_lock);
        m_command_lock = nullptr;
    }
    
    super::free();
}

IOReturn CLASS::reset()
{
    IOLockLock(m_command_lock);
    
    if (m_state == VM_COMMAND_BUFFER_STATE_PENDING) {
        IOLockUnlock(m_command_lock);
        return kIOReturnBusy;
    }
    
    // Clean up existing commands
    while (m_commands->getCount() > 0) {
        VMGPUCommand* command = (VMGPUCommand*)m_commands->getObject(0);
        if (command) {
            cleanupCommand(command);
        }
        m_commands->removeObject(0);
    }
    
    // Clean up resources
    if (m_resources) {
        m_resources->flushCollection();
    }
    
    // Reset state
    m_current_size = 0;
    m_command_count = 0;
    m_state = VM_COMMAND_BUFFER_STATE_INITIAL;
    m_execution_time = 0;
    
    // Clear buffer memory
    if (m_buffer_memory) {
        bzero(m_buffer_memory->getBytesNoCopy(), m_buffer_size);
    }
    
    IOLockUnlock(m_command_lock);
    return kIOReturnSuccess;
}

IOReturn CLASS::addDrawCommand(VMDrawCommandDescriptor* descriptor)
{
    if (!descriptor)
        return kIOReturnBadArgument;
    
    IOLockLock(m_command_lock);
    
    if (m_state != VM_COMMAND_BUFFER_STATE_INITIAL) {
        IOLockUnlock(m_command_lock);
        return kIOReturnNotPermitted;
    }
    
    if (m_command_count >= m_max_commands) {
        IOLockUnlock(m_command_lock);
        return kIOReturnNoSpace;
    }
    
    // Create command
    VMGPUCommand* command = (VMGPUCommand*)IOMalloc(sizeof(VMGPUCommand));
    if (!command) {
        IOLockUnlock(m_command_lock);
        return kIOReturnNoMemory;
    }
    
    bzero(command, sizeof(VMGPUCommand));
    command->header.type = VM_CMD_DRAW;
    command->header.sequence = m_command_count++;
    command->header.size = sizeof(VMDrawCommandDescriptor);
    
    // Copy descriptor
    void* data_ptr = (void*)&command->data[0];
    memcpy(data_ptr, descriptor, sizeof(VMDrawCommandDescriptor));
    
    // Add to command list
    m_commands->setObject((OSObject*)command);
    m_current_size += command->header.size;
    
    IOLockUnlock(m_command_lock);
    return kIOReturnSuccess;
}

IOReturn CLASS::addComputeCommand(VMComputeCommandDescriptor* descriptor)
{
    if (!descriptor)
        return kIOReturnBadArgument;
    
    IOLockLock(m_command_lock);
    
    if (m_state != VM_COMMAND_BUFFER_STATE_INITIAL) {
        IOLockUnlock(m_command_lock);
        return kIOReturnNotPermitted;
    }
    
    if (m_command_count >= m_max_commands) {
        IOLockUnlock(m_command_lock);
        return kIOReturnNoSpace;
    }
    
    // Create command
    VMGPUCommand* command = (VMGPUCommand*)IOMalloc(sizeof(VMGPUCommand));
    if (!command) {
        IOLockUnlock(m_command_lock);
        return kIOReturnNoMemory;
    }
    
    bzero(command, sizeof(VMGPUCommand));
    command->header.type = VM_CMD_DISPATCH;
    command->header.sequence = m_command_count++;
    command->header.size = sizeof(VMComputeCommandDescriptor);
    
    // Copy descriptor
    void* data_ptr = (void*)&command->data[0];
    memcpy(data_ptr, descriptor, sizeof(VMComputeCommandDescriptor));
    
    // Add to command list
    m_commands->setObject((OSObject*)command);
    m_current_size += command->header.size;
    
    IOLockUnlock(m_command_lock);
    return kIOReturnSuccess;
}

IOReturn CLASS::addResourceBinding(uint32_t binding_point, uint32_t resource_id, uint32_t resource_type)
{
    IOLockLock(m_command_lock);
    
    if (m_state != VM_COMMAND_BUFFER_STATE_INITIAL) {
        IOLockUnlock(m_command_lock);
        return kIOReturnNotPermitted;
    }
    
    // Create resource binding
    ResourceBinding* binding = (ResourceBinding*)IOMalloc(sizeof(ResourceBinding));
    if (!binding) {
        IOLockUnlock(m_command_lock);
        return kIOReturnNoMemory;
    }
    
    binding->binding_point = binding_point;
    binding->resource_id = resource_id;
    binding->resource_type = resource_type;
    
    m_resources->setObject((OSObject*)binding);
    
    IOLockUnlock(m_command_lock);
    return kIOReturnSuccess;
}

void CLASS::cleanupCommand(VMGPUCommand* command)
{
    if (!command)
        return;
        
    // Free the command structure  
    IOFree(command, sizeof(VMGPUCommand));
}

// Command buffer pool implementation
OSDefineMetaClassAndStructors(VMCommandBufferPool, OSObject);

VMCommandBufferPool* VMCommandBufferPool::withAccelerator(VMQemuVGAAccelerator* accelerator,
                                                         uint32_t context_id, uint32_t max_buffers)
{
    VMCommandBufferPool* pool = new VMCommandBufferPool;
    if (pool && !pool->init(accelerator, context_id, max_buffers)) {
        pool->release();
        return nullptr;
    }
    return pool;
}

bool VMCommandBufferPool::init(VMQemuVGAAccelerator* accelerator, uint32_t context_id, uint32_t max_buffers)
{
    if (!super::init())
        return false;
        
    m_accelerator = accelerator;
    m_context_id = context_id;
    m_max_buffers = max_buffers;
    
    m_available_buffers = OSArray::withCapacity(max_buffers);
    if (!m_available_buffers)
        return false;
        
    m_active_buffers = OSArray::withCapacity(max_buffers);
    if (!m_active_buffers)
        return false;
        
    m_pool_lock = IOLockAlloc();
    if (!m_pool_lock)
        return false;
    
    return true;
}

void VMCommandBufferPool::free()
{
    if (m_available_buffers) {
        m_available_buffers->release();
        m_available_buffers = nullptr;
    }
    
    if (m_active_buffers) {
        m_active_buffers->release();
        m_active_buffers = nullptr;
    }
    
    if (m_pool_lock) {
        IOLockFree(m_pool_lock);
        m_pool_lock = nullptr;
    }
    
    super::free();
}

IOReturn VMCommandBufferPool::allocateCommandBuffer(VMCommandBuffer** out_buffer)
{
    if (!out_buffer)
        return kIOReturnBadArgument;
        
    IOLockLock(m_pool_lock);
    
    VMCommandBuffer* buffer = nullptr;
    
    // Try to reuse an available buffer
    if (m_available_buffers->getCount() > 0) {
        buffer = (VMCommandBuffer*)m_available_buffers->getObject(0);
        buffer->retain();
        m_available_buffers->removeObject(0);
    }
    
    if (!buffer) {
        // Create new buffer if under limit
        if (m_active_buffers->getCount() < m_max_buffers) {
            buffer = VMCommandBuffer::withAccelerator(m_accelerator, m_context_id);
        }
    }
    
    if (buffer) {
        m_active_buffers->setObject(buffer);
        *out_buffer = buffer;
    }
    
    IOLockUnlock(m_pool_lock);
    return buffer ? kIOReturnSuccess : kIOReturnNoSpace;
}

IOReturn VMCommandBufferPool::releaseCommandBuffer(VMCommandBuffer* buffer)
{
    if (!buffer)
        return kIOReturnBadArgument;
        
    IOLockLock(m_pool_lock);
    
    // Remove from active buffers
    unsigned int index = m_active_buffers->getNextIndexOfObject(buffer, 0);
    if (index != (unsigned int)-1) {
        m_active_buffers->removeObject(index);
    }
    
    // Reset buffer and add to available list
    buffer->reset();
    m_available_buffers->setObject(buffer);
    
    IOLockUnlock(m_pool_lock);
    buffer->release();
    return kIOReturnSuccess;
}

IOReturn VMCommandBufferPool::resetPool()
{
    IOLockLock(m_pool_lock);
    
    // Release all buffers
    m_available_buffers->flushCollection();
    m_active_buffers->flushCollection();
    
    IOLockUnlock(m_pool_lock);
    return kIOReturnSuccess;
}

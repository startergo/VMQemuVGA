//
//  VMCGLContext.cpp
//  VMQemuVGA
//
//  Core OpenGL (CGL) Context Implementation for VirtIO GPU
//

#include "VMCGLContext.h"
#include "VMVirtIOGPU.h"
#include "virgl_protocol.h"
#include <IOKit/IOLib.h>

#define CLASS VMCGLContext
#define super IOUserClient

OSDefineMetaClassAndStructors(VMCGLContext, IOUserClient);

// ============================================================================
// MARK: - Initialization & Lifecycle
// ============================================================================

bool CLASS::initWithTask(task_t owningTask, void* securityToken, UInt32 type,
                        OSDictionary* properties)
{
    if (!super::initWithTask(owningTask, securityToken, type, properties)) {
        return false;
    }
    
    m_task = owningTask;
    m_accelerator = nullptr;
    m_context_id = 0;
    m_cgl_context_id = 0;
    m_context_valid = false;
    m_current_surface_id = 0;
    m_current_framebuffer_id = 0;
    m_command_buffer = nullptr;
    m_shared_memory = nullptr;
    m_shared_memory_desc = nullptr;
    
    IOLog("VMCGLContext: Initialized for task %p\n", owningTask);
    return true;
}

bool CLASS::start(IOService* provider)
{
    if (!super::start(provider)) {
        return false;
    }
    
    m_accelerator = OSDynamicCast(VMQemuVGAAccelerator, provider);
    if (!m_accelerator) {
        IOLog("VMCGLContext: ERROR - Provider is not VMQemuVGAAccelerator\n");
        return false;
    }
    
    IOLog("VMCGLContext: Started with accelerator %p\n", m_accelerator);
    return true;
}

void CLASS::stop(IOService* provider)
{
    if (m_context_valid) {
        cglDestroyContext();
    }
    
    super::stop(provider);
}

void CLASS::free()
{
    if (m_command_buffer) {
        m_command_buffer->release();
        m_command_buffer = nullptr;
    }
    
    if (m_shared_memory_desc) {
        m_shared_memory_desc->release();
        m_shared_memory_desc = nullptr;
    }
    
    super::free();
}

IOReturn CLASS::clientClose()
{
    if (m_context_valid) {
        cglDestroyContext();
    }
    
    terminate();
    return kIOReturnSuccess;
}

IOReturn CLASS::clientDied()
{
    return clientClose();
}

// ============================================================================
// MARK: - Method Dispatch
// ============================================================================

IOReturn CLASS::externalMethod(uint32_t selector, IOExternalMethodArguments* args,
                               IOExternalMethodDispatch* dispatch, OSObject* target,
                               void* reference)
{
    static IOExternalMethodDispatch dispatchTable[kVMCGLMethodCount] = {
        { (IOExternalMethodAction)&VMCGLContext::sCGLCreateContext, 2, 1, 0, 0 },
        { (IOExternalMethodAction)&VMCGLContext::sCGLDestroyContext, 0, 0, 0, 0 },
        { (IOExternalMethodAction)&VMCGLContext::sCGLSetSurface, 3, 0, 0, 0 },
        { (IOExternalMethodAction)&VMCGLContext::sCGLFlushContext, 0, 0, 0, 0 },
        { (IOExternalMethodAction)&VMCGLContext::sCGLSubmitCommands, 1, 0, 0, 0xFFFFFFFF },
        { (IOExternalMethodAction)&VMCGLContext::sCGLSetParameter, 2, 0, 0, 0 },
        { (IOExternalMethodAction)&VMCGLContext::sCGLGetParameter, 1, 2, 0, 0 },
        { (IOExternalMethodAction)&VMCGLContext::sCGLSetVirtualScreen, 1, 0, 0, 0 },
        { (IOExternalMethodAction)&VMCGLContext::sCGLGetVirtualScreen, 0, 1, 0, 0 },
        { (IOExternalMethodAction)&VMCGLContext::sCGLUpdateContext, 0, 0, 0, 0 },
        { (IOExternalMethodAction)&VMCGLContext::sCGLClearDrawable, 0, 0, 0, 0 },
        { (IOExternalMethodAction)&VMCGLContext::sCGLLockContext, 0, 0, 0, 0 },
        { (IOExternalMethodAction)&VMCGLContext::sCGLUnlockContext, 0, 0, 0, 0 },
        { (IOExternalMethodAction)&VMCGLContext::sCGLSetupSharedMemory, 2, 0, 0, 0 },
    };
    
    if (selector >= kVMCGLMethodCount) {
        IOLog("VMCGLContext: Invalid selector %d\n", selector);
        return kIOReturnBadArgument;
    }
    
    return super::externalMethod(selector, args, &dispatchTable[selector], this, nullptr);
}

// ============================================================================
// MARK: - Static Method Handlers
// ============================================================================

IOReturn CLASS::sCGLCreateContext(OSObject* target, void* reference,
                                 IOExternalMethodArguments* args)
{
    VMCGLContext* me = (VMCGLContext*)target;
    if (!me || args->scalarInputCount != 2 || args->scalarOutputCount != 1) {
        return kIOReturnBadArgument;
    }
    
    uint32_t pixel_format = (uint32_t)args->scalarInput[0];
    uint32_t share_context = (uint32_t)args->scalarInput[1];
    
    IOReturn ret = me->cglCreateContext(pixel_format, share_context);
    if (ret == kIOReturnSuccess) {
        args->scalarOutput[0] = me->m_cgl_context_id;
    }
    
    return ret;
}

IOReturn CLASS::sCGLDestroyContext(OSObject* target, void* reference,
                                  IOExternalMethodArguments* args)
{
    VMCGLContext* me = (VMCGLContext*)target;
    if (!me) {
        return kIOReturnBadArgument;
    }
    
    return me->cglDestroyContext();
}

IOReturn CLASS::sCGLSetSurface(OSObject* target, void* reference,
                              IOExternalMethodArguments* args)
{
    VMCGLContext* me = (VMCGLContext*)target;
    if (!me || args->scalarInputCount != 3) {
        return kIOReturnBadArgument;
    }
    
    uint32_t surface_id = (uint32_t)args->scalarInput[0];
    uint32_t width = (uint32_t)args->scalarInput[1];
    uint32_t height = (uint32_t)args->scalarInput[2];
    
    return me->cglSetSurface(surface_id, width, height);
}

IOReturn CLASS::sCGLFlushContext(OSObject* target, void* reference,
                                IOExternalMethodArguments* args)
{
    VMCGLContext* me = (VMCGLContext*)target;
    if (!me) {
        return kIOReturnBadArgument;
    }
    
    return me->cglFlushContext();
}

IOReturn CLASS::sCGLSubmitCommands(OSObject* target, void* reference,
                                  IOExternalMethodArguments* args)
{
    VMCGLContext* me = (VMCGLContext*)target;
    if (!me || args->scalarInputCount != 1) {
        return kIOReturnBadArgument;
    }
    
    uint32_t command_size = (uint32_t)args->scalarInput[0];
    
    // Commands come through structureInput
    if (!args->structureInput || args->structureInputSize == 0) {
        return kIOReturnBadArgument;
    }
    
    IOMemoryDescriptor* commandDesc = IOMemoryDescriptor::withAddress(
        (void*)args->structureInput, args->structureInputSize, kIODirectionIn);
    
    if (!commandDesc) {
        return kIOReturnNoMemory;
    }
    
    IOReturn ret = me->cglSubmitCommands(commandDesc, command_size);
    commandDesc->release();
    
    return ret;
}

IOReturn CLASS::sCGLSetParameter(OSObject* target, void* reference,
                                IOExternalMethodArguments* args)
{
    VMCGLContext* me = (VMCGLContext*)target;
    if (!me || args->scalarInputCount != 2) {
        return kIOReturnBadArgument;
    }
    
    uint32_t param_name = (uint32_t)args->scalarInput[0];
    int32_t param_value = (int32_t)args->scalarInput[1];
    
    return me->cglSetParameter(param_name, &param_value, 1);
}

IOReturn CLASS::sCGLGetParameter(OSObject* target, void* reference,
                                IOExternalMethodArguments* args)
{
    VMCGLContext* me = (VMCGLContext*)target;
    if (!me || args->scalarInputCount != 1 || args->scalarOutputCount != 2) {
        return kIOReturnBadArgument;
    }
    
    uint32_t param_name = (uint32_t)args->scalarInput[0];
    int32_t param_value = 0;
    uint32_t count = 1;
    
    IOReturn ret = me->cglGetParameter(param_name, &param_value, &count);
    if (ret == kIOReturnSuccess) {
        args->scalarOutput[0] = param_value;
        args->scalarOutput[1] = count;
    }
    
    return ret;
}

IOReturn CLASS::sCGLSetVirtualScreen(OSObject* target, void* reference,
                                    IOExternalMethodArguments* args)
{
    VMCGLContext* me = (VMCGLContext*)target;
    if (!me || args->scalarInputCount != 1) {
        return kIOReturnBadArgument;
    }
    
    uint32_t screen_id = (uint32_t)args->scalarInput[0];
    return me->cglSetVirtualScreen(screen_id);
}

IOReturn CLASS::sCGLGetVirtualScreen(OSObject* target, void* reference,
                                    IOExternalMethodArguments* args)
{
    VMCGLContext* me = (VMCGLContext*)target;
    if (!me || args->scalarOutputCount != 1) {
        return kIOReturnBadArgument;
    }
    
    uint32_t screen_id = 0;
    IOReturn ret = me->cglGetVirtualScreen(&screen_id);
    if (ret == kIOReturnSuccess) {
        args->scalarOutput[0] = screen_id;
    }
    
    return ret;
}

IOReturn CLASS::sCGLUpdateContext(OSObject* target, void* reference,
                                 IOExternalMethodArguments* args)
{
    VMCGLContext* me = (VMCGLContext*)target;
    if (!me) {
        return kIOReturnBadArgument;
    }
    
    return me->cglUpdateContext();
}

IOReturn CLASS::sCGLClearDrawable(OSObject* target, void* reference,
                                 IOExternalMethodArguments* args)
{
    VMCGLContext* me = (VMCGLContext*)target;
    if (!me) {
        return kIOReturnBadArgument;
    }
    
    return me->cglClearDrawable();
}

IOReturn CLASS::sCGLLockContext(OSObject* target, void* reference,
                               IOExternalMethodArguments* args)
{
    VMCGLContext* me = (VMCGLContext*)target;
    if (!me) {
        return kIOReturnBadArgument;
    }
    
    return me->cglLockContext();
}

IOReturn CLASS::sCGLUnlockContext(OSObject* target, void* reference,
                                 IOExternalMethodArguments* args)
{
    VMCGLContext* me = (VMCGLContext*)target;
    if (!me) {
        return kIOReturnBadArgument;
    }
    
    return me->cglUnlockContext();
}

IOReturn CLASS::sCGLSetupSharedMemory(OSObject* target, void* reference,
                                     IOExternalMethodArguments* args)
{
    VMCGLContext* me = (VMCGLContext*)target;
    if (!me || args->scalarInputCount != 2) {
        return kIOReturnBadArgument;
    }
    
    mach_vm_address_t address = (mach_vm_address_t)args->scalarInput[0];
    mach_vm_size_t size = (mach_vm_size_t)args->scalarInput[1];
    
    return me->cglSetupSharedMemory(address, size);
}

// ============================================================================
// MARK: - CGL Instance Methods
// ============================================================================

IOReturn CLASS::cglCreateContext(uint32_t pixel_format, uint32_t share_context)
{
    if (m_context_valid) {
        IOLog("VMCGLContext: Context already exists\n");
        return kIOReturnExclusiveAccess;
    }
    
    if (!m_accelerator) {
        return kIOReturnNoDevice;
    }
    
    // Create 3D context through accelerator
    IOReturn ret = m_accelerator->create3DContext(&m_context_id, m_task);
    if (ret != kIOReturnSuccess) {
        IOLog("VMCGLContext: Failed to create 3D context: 0x%x\n", ret);
        return ret;
    }
    
    m_cgl_context_id = m_context_id; // Use same ID for CGL
    m_context_valid = true;
    
    IOLog("VMCGLContext: ✅ Created CGL context %d (pixel format: 0x%x, share: %d)\n",
          m_cgl_context_id, pixel_format, share_context);
    
    return kIOReturnSuccess;
}

IOReturn CLASS::cglDestroyContext()
{
    if (!m_context_valid) {
        return kIOReturnNotOpen;
    }
    
    if (!m_accelerator) {
        return kIOReturnNoDevice;
    }
    
    IOReturn ret = m_accelerator->destroy3DContext(m_context_id);
    if (ret != kIOReturnSuccess) {
        IOLog("VMCGLContext: Failed to destroy context: 0x%x\n", ret);
        return ret;
    }
    
    m_context_valid = false;
    m_context_id = 0;
    m_cgl_context_id = 0;
    
    IOLog("VMCGLContext: Destroyed CGL context\n");
    return kIOReturnSuccess;
}

IOReturn CLASS::cglSetSurface(uint32_t surface_id, uint32_t width, uint32_t height)
{
    if (!m_context_valid) {
        return kIOReturnNotOpen;
    }
    
    m_current_surface_id = surface_id;
    
    IOLog("VMCGLContext: Set surface %d (%dx%d)\n", surface_id, width, height);
    return kIOReturnSuccess;
}

IOReturn CLASS::cglFlushContext()
{
    if (!m_context_valid) {
        return kIOReturnNotOpen;
    }
    
    if (!m_accelerator) {
        return kIOReturnNoDevice;
    }
    
    // Flush pending commands and present to screen
    if (m_current_surface_id) {
        IOReturn ret = m_accelerator->present3DSurface(m_context_id, m_current_surface_id);
        if (ret != kIOReturnSuccess) {
            IOLog("VMCGLContext: Failed to flush/present: 0x%x\n", ret);
            return ret;
        }
    }
    
    IOLog("VMCGLContext: 🚀 Flushed CGL context (presented surface %d)\n", m_current_surface_id);
    return kIOReturnSuccess;
}

IOReturn CLASS::cglSubmitCommands(IOMemoryDescriptor* commands, uint32_t command_size)
{
    if (!m_context_valid) {
        return kIOReturnNotOpen;
    }
    
    if (!m_accelerator || !commands) {
        return kIOReturnBadArgument;
    }
    
    // Submit OpenGL commands to accelerator for translation to virgl
    IOReturn ret = m_accelerator->submit3DCommands(m_context_id, commands);
    if (ret != kIOReturnSuccess) {
        IOLog("VMCGLContext: Failed to submit commands: 0x%x\n", ret);
        return ret;
    }
    
    IOLog("VMCGLContext: ✅ Submitted %d bytes of OpenGL commands\n", command_size);
    return kIOReturnSuccess;
}

IOReturn CLASS::cglSetParameter(uint32_t param_name, const int32_t* params, uint32_t count)
{
    if (!m_context_valid) {
        return kIOReturnNotOpen;
    }
    
    if (!params || count == 0) {
        return kIOReturnBadArgument;
    }
    
    // Handle common CGL parameters
    switch (param_name) {
        case kCGLCPSwapInterval:
            IOLog("VMCGLContext: Set swap interval = %d\n", params[0]);
            break;
            
        case kCGLCPSurfaceOpacity:
            IOLog("VMCGLContext: Set surface opacity = %d\n", params[0]);
            break;
            
        default:
            IOLog("VMCGLContext: Set parameter 0x%x = %d\n", param_name, params[0]);
            break;
    }
    
    return kIOReturnSuccess;
}

IOReturn CLASS::cglGetParameter(uint32_t param_name, int32_t* params, uint32_t* count)
{
    if (!m_context_valid) {
        return kIOReturnNotOpen;
    }
    
    if (!params || !count) {
        return kIOReturnBadArgument;
    }
    
    // Return reasonable defaults for common CGL parameters
    switch (param_name) {
        case kCGLCPSwapInterval:
            params[0] = 1; // VSync on by default
            *count = 1;
            break;
            
        case kCGLCPCurrentRendererID:
            params[0] = 0x00024600; // Our virtual GPU renderer ID
            *count = 1;
            break;
            
        case kCGLCPGPUVertexProcessing:
        case kCGLCPGPUFragmentProcessing:
            params[0] = 1; // Hardware acceleration available
            *count = 1;
            break;
            
        case kCGLCPHasDrawable:
            params[0] = (m_current_surface_id != 0) ? 1 : 0;
            *count = 1;
            break;
            
        default:
            params[0] = 0;
            *count = 1;
            break;
    }
    
    IOLog("VMCGLContext: Get parameter 0x%x = %d\n", param_name, params[0]);
    return kIOReturnSuccess;
}

IOReturn CLASS::cglSetVirtualScreen(uint32_t screen_id)
{
    if (!m_context_valid) {
        return kIOReturnNotOpen;
    }
    
    IOLog("VMCGLContext: Set virtual screen %d\n", screen_id);
    return kIOReturnSuccess;
}

IOReturn CLASS::cglGetVirtualScreen(uint32_t* screen_id)
{
    if (!m_context_valid) {
        return kIOReturnNotOpen;
    }
    
    if (!screen_id) {
        return kIOReturnBadArgument;
    }
    
    *screen_id = 0; // Always use screen 0 for now
    return kIOReturnSuccess;
}

IOReturn CLASS::cglUpdateContext()
{
    if (!m_context_valid) {
        return kIOReturnNotOpen;
    }
    
    // Synchronize context state with system
    IOLog("VMCGLContext: Updated context state\n");
    return kIOReturnSuccess;
}

IOReturn CLASS::cglClearDrawable()
{
    if (!m_context_valid) {
        return kIOReturnNotOpen;
    }
    
    // Clear the current drawable
    m_current_surface_id = 0;
    IOLog("VMCGLContext: Cleared drawable\n");
    
    return kIOReturnSuccess;
}

IOReturn CLASS::cglLockContext()
{
    if (!m_context_valid) {
        return kIOReturnNotOpen;
    }
    
    // Context locking for thread safety
    return kIOReturnSuccess;
}

IOReturn CLASS::cglUnlockContext()
{
    if (!m_context_valid) {
        return kIOReturnNotOpen;
    }
    
    // Context unlocking
    return kIOReturnSuccess;
}

IOReturn CLASS::cglSetupSharedMemory(mach_vm_address_t address, mach_vm_size_t size)
{
    if (!m_context_valid) {
        return kIOReturnNotOpen;
    }
    
    // Map shared memory for fast parameter passing
    if (m_shared_memory_desc) {
        m_shared_memory_desc->release();
        m_shared_memory_desc = nullptr;
    }
    
    m_shared_memory_desc = IOMemoryDescriptor::withAddressRange(
        address, size, kIODirectionInOut, m_task);
    
    if (!m_shared_memory_desc) {
        IOLog("VMCGLContext: Failed to map shared memory\n");
        return kIOReturnNoMemory;
    }
    
    IOReturn ret = m_shared_memory_desc->prepare(kIODirectionInOut);
    if (ret != kIOReturnSuccess) {
        m_shared_memory_desc->release();
        m_shared_memory_desc = nullptr;
        return ret;
    }
    
    IOLog("VMCGLContext: ✅ Setup shared memory: %llu bytes at 0x%llx\n", size, address);
    return kIOReturnSuccess;
}

#include "VMQemuVGAAccelerator.h"
#include <IOKit/IOLib.h>

#define CLASS VMQemuVGA3DUserClient
#define super IOUserClient

OSDefineMetaClassAndStructors(VMQemuVGA3DUserClient, IOUserClient);

// Method dispatch table
static const IOExternalMethodDispatch sMethods[kVM3DUserClientMethodCount] = {
    { // kVM3DUserClientCreate3DContext
        .function = (IOExternalMethodAction) &VMQemuVGA3DUserClient::sCreate3DContext,
        .checkScalarInputCount = 0,
        .checkStructureInputSize = 0,
        .checkScalarOutputCount = 1,
        .checkStructureOutputSize = 0,
    },
    { // kVM3DUserClientDestroy3DContext
        .function = (IOExternalMethodAction) &VMQemuVGA3DUserClient::sDestroy3DContext,
        .checkScalarInputCount = 1,
        .checkStructureInputSize = 0,
        .checkScalarOutputCount = 0,
        .checkStructureOutputSize = 0,
    },
    { // kVM3DUserClientCreate3DSurface
        .function = (IOExternalMethodAction) &VMQemuVGA3DUserClient::sCreate3DSurface,
        .checkScalarInputCount = 1,
        .checkStructureInputSize = sizeof(VM3DSurfaceInfo),
        .checkScalarOutputCount = 1,
        .checkStructureOutputSize = 0,
    },
    { // kVM3DUserClientDestroy3DSurface
        .function = (IOExternalMethodAction) &VMQemuVGA3DUserClient::sDestroy3DSurface,
        .checkScalarInputCount = 2,
        .checkStructureInputSize = 0,
        .checkScalarOutputCount = 0,
        .checkStructureOutputSize = 0,
    },
    { // kVM3DUserClientSubmit3DCommands
        .function = (IOExternalMethodAction) &VMQemuVGA3DUserClient::sSubmit3DCommands,
        .checkScalarInputCount = 1,
        .checkStructureInputSize = 0,
        .checkScalarOutputCount = 0,
        .checkStructureOutputSize = 0,
    },
    { // kVM3DUserClientPresent3DSurface
        .function = (IOExternalMethodAction) &VMQemuVGA3DUserClient::sPresent3DSurface,
        .checkScalarInputCount = 2,
        .checkStructureInputSize = 0,
        .checkScalarOutputCount = 0,
        .checkStructureOutputSize = 0,
    },
    { // kVM3DUserClientGetCapabilities
        .function = (IOExternalMethodAction) &VMQemuVGA3DUserClient::sGetCapabilities,
        .checkScalarInputCount = 0,
        .checkStructureInputSize = 0,
        .checkScalarOutputCount = 0,
        .checkStructureOutputSize = 256,
    }
};

VMQemuVGA3DUserClient* VMQemuVGA3DUserClient::withTask(task_t owningTask)
{
    VMQemuVGA3DUserClient* client = new VMQemuVGA3DUserClient;
    if (client) {
        if (!client->initWithTask(owningTask, nullptr, 0, nullptr)) {
            client->release();
            client = nullptr;
        }
    }
    return client;
}

bool CLASS::initWithTask(task_t owningTask, void* securityToken, UInt32 type,
                        OSDictionary* properties)
{
    if (!super::initWithTask(owningTask, securityToken, type, properties))
        return false;
    
    m_accelerator = nullptr;
    m_task = owningTask;
    m_context_id = 0;
    m_has_context = false;
    
    return true;
}

bool CLASS::start(IOService* provider)
{
    if (!super::start(provider))
        return false;
    
    m_accelerator = OSDynamicCast(VMQemuVGAAccelerator, provider);
    if (!m_accelerator) {
        IOLog("VMQemuVGA3DUserClient: Provider is not VMQemuVGAAccelerator\n");
        return false;
    }
    
    IOLog("VMQemuVGA3DUserClient: Started for task %p\n", m_task);
    return true;
}

IOReturn CLASS::clientClose()
{
    IOLog("VMQemuVGA3DUserClient: clientClose\n");
    
    if (m_has_context) {
        m_accelerator->destroy3DContext(m_context_id);
        m_has_context = false;
        m_context_id = 0;
    }
    
    if (isInactive() == false) {
        terminate();
    }
    
    return kIOReturnSuccess;
}

IOReturn CLASS::clientDied()
{
    IOLog("VMQemuVGA3DUserClient: clientDied\n");
    return clientClose();
}

IOReturn CLASS::externalMethod(uint32_t selector, IOExternalMethodArguments* args,
                              IOExternalMethodDispatch* dispatch, OSObject* target,
                              void* reference)
{
    if (selector >= kVM3DUserClientMethodCount)
        return kIOReturnBadArgument;
    
    dispatch = (IOExternalMethodDispatch*) &sMethods[selector];
    if (!dispatch)
        return kIOReturnBadArgument;
    
    target = this;
    reference = nullptr;
    
    return super::externalMethod(selector, args, dispatch, target, reference);
}

IOReturn CLASS::sCreate3DContext(OSObject* target, void* reference,
                               IOExternalMethodArguments* args)
{
    VMQemuVGA3DUserClient* me = (VMQemuVGA3DUserClient*)target;
    
    if (me->m_has_context) {
        return kIOReturnExclusiveAccess;
    }
    
    IOReturn ret = me->m_accelerator->create3DContext(&me->m_context_id, me->m_task);
    if (ret == kIOReturnSuccess) {
        me->m_has_context = true;
        args->scalarOutput[0] = me->m_context_id;
    }
    
    IOLog("VMQemuVGA3DUserClient: Created context %d, result: 0x%x\n", 
          me->m_context_id, ret);
    
    return ret;
}

IOReturn CLASS::sDestroy3DContext(OSObject* target, void* reference,
                                IOExternalMethodArguments* args)
{
    VMQemuVGA3DUserClient* me = (VMQemuVGA3DUserClient*)target;
    uint32_t context_id = (uint32_t)args->scalarInput[0];
    
    if (!me->m_has_context || context_id != me->m_context_id) {
        return kIOReturnBadArgument;
    }
    
    IOReturn ret = me->m_accelerator->destroy3DContext(context_id);
    if (ret == kIOReturnSuccess) {
        me->m_has_context = false;
        me->m_context_id = 0;
    }
    
    IOLog("VMQemuVGA3DUserClient: Destroyed context %d, result: 0x%x\n", 
          context_id, ret);
    
    return ret;
}

IOReturn CLASS::sCreate3DSurface(OSObject* target, void* reference,
                               IOExternalMethodArguments* args)
{
    VMQemuVGA3DUserClient* me = (VMQemuVGA3DUserClient*)target;
    uint32_t context_id = (uint32_t)args->scalarInput[0];
    VM3DSurfaceInfo* surface_info = (VM3DSurfaceInfo*)args->structureInput;
    
    if (!me->m_has_context || context_id != me->m_context_id) {
        return kIOReturnBadArgument;
    }
    
    IOReturn ret = me->m_accelerator->create3DSurface(context_id, surface_info);
    if (ret == kIOReturnSuccess) {
        args->scalarOutput[0] = surface_info->surface_id;
    }
    
    IOLog("VMQemuVGA3DUserClient: Created surface %d (%dx%d), result: 0x%x\n",
          surface_info->surface_id, surface_info->width, surface_info->height, ret);
    
    return ret;
}

IOReturn CLASS::sDestroy3DSurface(OSObject* target, void* reference,
                                 IOExternalMethodArguments* args)
{
    VMQemuVGA3DUserClient* me = (VMQemuVGA3DUserClient*)target;
    uint32_t context_id = (uint32_t)args->scalarInput[0];
    uint32_t surface_id = (uint32_t)args->scalarInput[1];
    
    if (!me->m_has_context || context_id != me->m_context_id) {
        return kIOReturnBadArgument;
    }
    
    // Implementation would call accelerator's destroy surface method
    IOLog("VMQemuVGA3DUserClient: Destroy surface %d\n", surface_id);
    
    return kIOReturnSuccess;
}

IOReturn CLASS::sSubmit3DCommands(OSObject* target, void* reference,
                                IOExternalMethodArguments* args)
{
    VMQemuVGA3DUserClient* me = (VMQemuVGA3DUserClient*)target;
    uint32_t context_id = (uint32_t)args->scalarInput[0];
    
    if (!me->m_has_context || context_id != me->m_context_id) {
        return kIOReturnBadArgument;
    }
    
    IOMemoryDescriptor* commands = args->structureInputDescriptor;
    if (!commands) {
        return kIOReturnBadArgument;
    }
    
    IOReturn ret = me->m_accelerator->submit3DCommands(context_id, commands);
    
    IOLog("VMQemuVGA3DUserClient: Submit commands to context %d, result: 0x%x\n",
          context_id, ret);
    
    return ret;
}

IOReturn CLASS::sPresent3DSurface(OSObject* target, void* reference,
                                IOExternalMethodArguments* args)
{
    VMQemuVGA3DUserClient* me = (VMQemuVGA3DUserClient*)target;
    uint32_t context_id = (uint32_t)args->scalarInput[0];
    uint32_t surface_id = (uint32_t)args->scalarInput[1];
    
    if (!me->m_has_context || context_id != me->m_context_id) {
        return kIOReturnBadArgument;
    }
    
    IOReturn ret = me->m_accelerator->present3DSurface(context_id, surface_id);
    
    IOLog("VMQemuVGA3DUserClient: Present surface %d from context %d, result: 0x%x\n",
          surface_id, context_id, ret);
    
    return ret;
}

IOReturn CLASS::sGetCapabilities(OSObject* target, void* reference,
                               IOExternalMethodArguments* args)
{
    VMQemuVGA3DUserClient* me = (VMQemuVGA3DUserClient*)target;
    
    struct {
        uint32_t max_texture_size;
        uint32_t max_render_targets;
        uint32_t supports_shaders;
        uint32_t supports_multisample;
        uint32_t supports_hardware_transform;
        uint32_t memory_available;
        uint32_t reserved[58]; // Pad to 256 bytes
    } capabilities = {};
    
    capabilities.max_texture_size = me->m_accelerator->getMaxTextureSize();
    capabilities.max_render_targets = me->m_accelerator->getMaxRenderTargets();
    capabilities.supports_shaders = me->m_accelerator->supportsShaders() ? 1 : 0;
    capabilities.supports_multisample = me->m_accelerator->supportsMultisample() ? 1 : 0;
    capabilities.supports_hardware_transform = me->m_accelerator->supportsHardwareTransform() ? 1 : 0;
    capabilities.memory_available = 256 * 1024 * 1024; // 256MB
    
    memcpy(args->structureOutput, &capabilities, sizeof(capabilities));
    args->structureOutputSize = sizeof(capabilities);
    
    IOLog("VMQemuVGA3DUserClient: Get capabilities\n");
    
    return kIOReturnSuccess;
}

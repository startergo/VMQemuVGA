/*
 *  VMVirtIOFramebufferClient.cpp
 *  VMQemuVGA
 *
 *  User client for VMVirtIOFramebuffer
 *  Based on VMQemuVGAClient pattern
 */

#include "VMVirtIOFramebufferClient.h"
#include "VMVirtIOFramebuffer.h"

#define super IOUserClient

OSDefineMetaClassAndStructors(VMVirtIOFramebufferClient, IOUserClient);

bool VMVirtIOFramebufferClient::start(IOService* provider)
{
    IOLog("%s: Starting user client for provider %s\n", __FUNCTION__, provider->getName());
    
    if (!super::start(provider)) {
        IOLog("%s: super::start() failed\n", __FUNCTION__);
        return false;
    }
    
    IOLog("%s: User client started successfully\n", __FUNCTION__);
    return true;
}

IOExternalMethod* VMVirtIOFramebufferClient::getTargetAndMethodForIndex(IOService** targetP, UInt32 index)
{
    IOLog("%s: index=%u\n", __FUNCTION__, static_cast<unsigned>(index));
    
    if (!targetP) {
        return nullptr;
    }
    
    // VirtIO GPU framebuffer has no custom external methods
    // Return NULL to indicate no methods available (like standard IOFramebuffer)
    IOLog("%s: No custom methods for VirtIO GPU (index %u)\n", __FUNCTION__, static_cast<unsigned>(index));
    return nullptr;
}

IOReturn VMVirtIOFramebufferClient::clientClose()
{
    IOLog("%s()\n", __FUNCTION__);
    
    if (!terminate()) {
        IOLog("%s: terminate failed\n", __FUNCTION__);
    }
    
    return kIOReturnSuccess;
}

bool VMVirtIOFramebufferClient::initWithTask(task_t owningTask, void* securityToken, UInt32 type)
{
    // Don't require admin privilege - this is a display driver, not config interface
    if (!super::initWithTask(owningTask, securityToken, type)) {
        return false;
    }
    
    return true;
}

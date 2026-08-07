/*
 *  VMVirtIOFramebufferClient.h
 *  VMQemuVGA
 *
 *  User client for VMVirtIOFramebuffer
 *  Minimal implementation for WindowServer connection
 */

#ifndef __VMVIRTIOFRAMEBUFFERCLIENT_H__
#define __VMVIRTIOFRAMEBUFFERCLIENT_H__

#include <IOKit/IOUserClient.h>

class VMVirtIOFramebufferClient : public IOUserClient
{
    OSDeclareDefaultStructors(VMVirtIOFramebufferClient)
    
public:
    virtual bool start(IOService* provider) override;
    virtual IOReturn clientClose() override;
    virtual bool initWithTask(task_t owningTask, void* securityToken, UInt32 type) override;
    virtual IOExternalMethod* getTargetAndMethodForIndex(IOService** targetP, UInt32 index) override;
};

#endif /* __VMVIRTIOFRAMEBUFFERCLIENT_H__ */

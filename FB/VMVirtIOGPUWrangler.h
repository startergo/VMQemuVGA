//
//  VMVirtIOGPUWrangler.h
//  VMQemuVGA
//
//  VirtIO GPU Wrangler for AppleGPUWrangler integration
//  Extends AppleGraphicsDeviceControl for proper GPU enumeration
//

#ifndef VMVirtIOGPUWrangler_h
#define VMVirtIOGPUWrangler_h

#include <IOKit/IOService.h>
#include <IOKit/IOLib.h>

// Forward declarations
class VMVirtIOGPU;

class VMVirtIOGPUWrangler : public IOService
{
    OSDeclareDefaultStructors(VMVirtIOGPUWrangler);
    
private:
    VMVirtIOGPU* m_gpu_device;
    
public:
    // IOService overrides
    virtual bool init(OSDictionary* properties = nullptr) override;
    virtual void free() override;
    virtual bool start(IOService* provider) override;
    virtual void stop(IOService* provider) override;
    
    // AppleGraphicsDeviceControl-like interface
    virtual IOReturn getVendorInfo(UInt32* vendorID, UInt32* deviceID);
    virtual IOReturn getGPUCapabilities(UInt32* capabilities);
    virtual IOReturn getPowerState(UInt32* state);
    virtual IOReturn setPowerState(UInt32 state);
    
    // VirtIO GPU specific methods
    virtual IOReturn attachToVirtIOGPU(VMVirtIOGPU* gpu);
    virtual void detachFromVirtIOGPU();
    
    // GPU Wrangler registration
    virtual IOReturn registerWithGPUWrangler();
    virtual void unregisterFromGPUWrangler();
};

#endif /* VMVirtIOGPUWrangler_h */

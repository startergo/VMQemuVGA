//
//  VMCGLContext.h
//  VMQemuVGA
//
//  Core OpenGL (CGL) Context Implementation for VirtIO GPU
//  This provides the IOAccelGLContext interface that CGL uses to communicate with our driver
//

#ifndef __VMCGLContext_H__
#define __VMCGLContext_H__

#include <IOKit/IOUserClient.h>
#include <IOKit/IOMemoryDescriptor.h>
#include "VMQemuVGAAccelerator.h"

// Forward declarations
class VMVirtIOGPUAccelerator;
class VMQemuVGAAccelerator;

//
// VMCGLContext - Core Graphics Layer OpenGL Context
//
// This class implements the IOAccelGLContext interface that macOS CGL uses
// to communicate with graphics drivers. When applications use OpenGL through
// CGL (Core OpenGL), the system routes calls through this interface.
//
class VMCGLContext : public IOUserClient
{
    OSDeclareDefaultStructors(VMCGLContext);
    
private:
    VMQemuVGAAccelerator* m_accelerator;
    task_t m_task;
    uint32_t m_context_id;              // Our internal context ID
    uint32_t m_cgl_context_id;          // CGL's context ID
    bool m_context_valid;
    
    // Context state
    uint32_t m_current_surface_id;
    uint32_t m_current_framebuffer_id;
    IOMemoryDescriptor* m_command_buffer;
    void* m_shared_memory;              // Shared memory for fast parameter passing
    IOMemoryDescriptor* m_shared_memory_desc;
    
public:
    // IOService overrides
    virtual bool initWithTask(task_t owningTask, void* securityToken, UInt32 type,
                             OSDictionary* properties) override;
    virtual bool start(IOService* provider) override;
    virtual void stop(IOService* provider) override;
    virtual void free() override;
    virtual IOReturn clientClose() override;
    virtual IOReturn clientDied() override;
    
    // Method dispatch
    virtual IOReturn externalMethod(uint32_t selector, IOExternalMethodArguments* args,
                                   IOExternalMethodDispatch* dispatch, OSObject* target,
                                   void* reference) override;
    
    // CGL Interface Methods - Static handlers
    static IOReturn sCGLCreateContext(OSObject* target, void* reference,
                                     IOExternalMethodArguments* args);
    static IOReturn sCGLDestroyContext(OSObject* target, void* reference,
                                      IOExternalMethodArguments* args);
    static IOReturn sCGLSetSurface(OSObject* target, void* reference,
                                  IOExternalMethodArguments* args);
    static IOReturn sCGLFlushContext(OSObject* target, void* reference,
                                    IOExternalMethodArguments* args);
    static IOReturn sCGLSubmitCommands(OSObject* target, void* reference,
                                      IOExternalMethodArguments* args);
    static IOReturn sCGLSetParameter(OSObject* target, void* reference,
                                    IOExternalMethodArguments* args);
    static IOReturn sCGLGetParameter(OSObject* target, void* reference,
                                    IOExternalMethodArguments* args);
    static IOReturn sCGLSetVirtualScreen(OSObject* target, void* reference,
                                        IOExternalMethodArguments* args);
    static IOReturn sCGLGetVirtualScreen(OSObject* target, void* reference,
                                        IOExternalMethodArguments* args);
    static IOReturn sCGLUpdateContext(OSObject* target, void* reference,
                                     IOExternalMethodArguments* args);
    static IOReturn sCGLClearDrawable(OSObject* target, void* reference,
                                     IOExternalMethodArguments* args);
    static IOReturn sCGLLockContext(OSObject* target, void* reference,
                                   IOExternalMethodArguments* args);
    static IOReturn sCGLUnlockContext(OSObject* target, void* reference,
                                     IOExternalMethodArguments* args);
    static IOReturn sCGLSetupSharedMemory(OSObject* target, void* reference,
                                         IOExternalMethodArguments* args);
    
    // Instance methods
    IOReturn cglCreateContext(uint32_t pixel_format, uint32_t share_context);
    IOReturn cglDestroyContext();
    IOReturn cglSetSurface(uint32_t surface_id, uint32_t width, uint32_t height);
    IOReturn cglFlushContext();
    IOReturn cglSubmitCommands(IOMemoryDescriptor* commands, uint32_t command_size);
    IOReturn cglSetParameter(uint32_t param_name, const int32_t* params, uint32_t count);
    IOReturn cglGetParameter(uint32_t param_name, int32_t* params, uint32_t* count);
    IOReturn cglSetVirtualScreen(uint32_t screen_id);
    IOReturn cglGetVirtualScreen(uint32_t* screen_id);
    IOReturn cglUpdateContext();
    IOReturn cglClearDrawable();
    IOReturn cglLockContext();
    IOReturn cglUnlockContext();
    IOReturn cglSetupSharedMemory(mach_vm_address_t address, mach_vm_size_t size);
};

// CGL Method selectors
enum {
    kVMCGLCreateContext = 0,
    kVMCGLDestroyContext,
    kVMCGLSetSurface,
    kVMCGLFlushContext,
    kVMCGLSubmitCommands,
    kVMCGLSetParameter,
    kVMCGLGetParameter,
    kVMCGLSetVirtualScreen,
    kVMCGLGetVirtualScreen,
    kVMCGLUpdateContext,
    kVMCGLClearDrawable,
    kVMCGLLockContext,
    kVMCGLUnlockContext,
    kVMCGLSetupSharedMemory,
    kVMCGLMethodCount
};

// CGL Context parameters
enum {
    kCGLCPSwapInterval = 222,           // VSync interval
    kCGLCPSurfaceOrder = 235,           // Surface ordering
    kCGLCPSurfaceOpacity = 236,         // Surface opacity
    kCGLCPSurfaceBackingSize = 304,     // Backing store size
    kCGLCPSurfaceSurfaceVolatile = 306, // Volatile surface
    kCGLCPReclaimResources = 308,       // Reclaim resources
    kCGLCPCurrentRendererID = 309,      // Current renderer
    kCGLCPGPUVertexProcessing = 310,    // Hardware vertex processing
    kCGLCPGPUFragmentProcessing = 311,  // Hardware fragment processing
    kCGLCPHasDrawable = 314,            // Has drawable surface
    kCGLCPMPSwapsInFlight = 315         // Multi-threaded swaps
};

#endif /* __VMCGLContext_H__ */

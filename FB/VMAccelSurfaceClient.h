/*
 * VMAccelSurfaceClient.h
 * IOAccelSurface UserClient implementation for WindowServer 2D acceleration
 * 
 * This implements the IOAccelSurfaceConnect API that WindowServer uses
 * for hardware-accelerated surface blitting, filling, and composition.
 */

#ifndef _VM_ACCEL_SURFACE_CLIENT_H
#define _VM_ACCEL_SURFACE_CLIENT_H

#include <IOKit/IOUserClient.h>
#include <IOKit/graphics/IOAccelTypes.h>
#include <IOKit/graphics/IOAccelSurfaceConnect.h>

class VMQemuVGAAccelerator;
class VMVirtIOGPU;

// Surface information structure
struct VMAccelSurface {
    uint32_t surface_id;
    uint32_t pixel_format;
    uint32_t bytes_per_pixel;   /* derived from depth bits at SetIDMode */
    /* current shape = sub-region within the base extent */
    uint32_t shape_x, shape_y;
    uint32_t width, height;     /* current shape bounds w/h */
    /* grow-only base extent: max bounds.x+w / bounds.y+h ever stored */
    uint32_t base_w, base_h;
    IOBufferMemoryDescriptor* backing_memory;
    IOMemoryMap* client_map;    /* mapping in m_owning_task, outlives unlock */
    IOMemoryMap* kernel_map;    /* kernel-task mapping — flush's memcpy source.
                                 * REQUIRED: KernelUserShared pageable memory has
                                 * NO kernel VA until explicitly mapped
                                 * (getBytesNoCopy() returned 0 on the 3258aaec
                                 * boot, 42/42 flush NotReady). Worked example
                                 * idiom: createMappingInTask(kernel_task, 0,
                                 * kIOMapAnywhere), VMsvga2Surface.cpp:1123. */
    uint32_t bytes_per_row;     /* the ALLOCATION's stride: base_w * bpp */
    bool is_locked;
    task_t owning_task;
};

class VMAccelSurfaceClient : public IOUserClient
{
    OSDeclareDefaultStructors(VMAccelSurfaceClient);

private:
    VMQemuVGAAccelerator* m_accelerator;
    task_t m_owning_task;
    VMAccelSurface* m_surface;
    IOLock* m_lock;
    bool m_creator_logged;  /* first-dispatch creator read, once */
    bool m_skip_write_lock_once;  /* 10.6 WindowServer Window-Grab deadlock
                                   * avoidance, worked example :1658-1666 */
    
public:
    // IOService overrides
    virtual bool initWithTask(task_t owningTask, void* securityToken,
                             UInt32 type, OSDictionary* properties) override;
    virtual bool start(IOService* provider) override;
    virtual void stop(IOService* provider) override;
    virtual void free() override;
    
    virtual IOReturn clientClose() override;
    virtual IOReturn clientDied() override;
    
    // OLD-STYLE external method dispatch (NO externalMethod() override)
    // This approach provides MIG with argument metadata, working on ALL macOS versions
    virtual IOExternalMethod* getTargetAndMethodForIndex(IOService** targetP, UInt32 index) override;
    
    // Memory mapping for framebuffer access
    virtual IOReturn clientMemoryForType(UInt32 type, IOOptionBits* options, IOMemoryDescriptor** memory) override;
    
    // Handler methods (old-style 6-parameter IOMethod signature)
    IOReturn readLockOptions(void*, void*, void*, void*, void*, void*);
    IOReturn readUnlockOptions(void*, void*, void*, void*, void*, void*);
    IOReturn getState(void*, void*, void*, void*, void*, void*);
    IOReturn writeLockOptions(void*, void*, void*, void*, void*, void*);
    IOReturn writeUnlockOptions(void*, void*, void*, void*, void*, void*);
    IOReturn read(void*, void*, void*, void*, void*, void*);
    IOReturn setShapeBacking(void*, void*, void*, void*, void*, void*);
    IOReturn setIDMode(void*, void*, void*, void*, void*, void*);
    IOReturn setScale(void*, void*, void*, void*, void*, void*);
    IOReturn setShape(void*, void*, void*, void*, void*, void*);
    IOReturn flush(void*, void*, void*, void*, void*, void*);
    IOReturn queryLock(void*, void*, void*, void*, void*, void*);
    IOReturn readLockSurface(void*, void*, void*, void*, void*, void*);
    IOReturn readUnlockSurface(void*, void*, void*, void*, void*, void*);
    IOReturn writeLockSurface(void*, void*, void*, void*, void*, void*);
    IOReturn writeUnlockSurface(void*, void*, void*, void*, void*, void*);
    IOReturn control(void*, void*, void*, void*, void*, void*);
    IOReturn setShapeBackingAndLength(void*, void*, void*, void*, void*, void*);
    
    // Factory method
    static VMAccelSurfaceClient* withTask(task_t owningTask);
};

#endif /* _VM_ACCEL_SURFACE_CLIENT_H */

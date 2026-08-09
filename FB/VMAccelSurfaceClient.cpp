/*
 * VMAccelSurfaceClient.cpp
 * IOAccelSurface UserClient implementation for WindowServer 2D acceleration
 */

#include "VMAccelSurfaceClient.h"
#include "VMQemuVGAAccelerator.h"
#include "VMVirtIOGPU.h"
#include <IOKit/IOLib.h>
#include <IOKit/graphics/IOGraphicsInterfaceTypes.h>

#define CLASS VMAccelSurfaceClient
#define super IOUserClient

OSDefineMetaClassAndStructors(VMAccelSurfaceClient, IOUserClient);

// OLD-STYLE IOExternalMethod dispatch table for IOAccelSurface operations
// This approach provides argument count metadata to MIG at IPC boundary
// CRITICAL: count0 = scalar inputs, count1 = scalar outputs
// Setting to 0 means "expects exactly 0", NOT "accepts any count"
static const IOExternalMethod sMethods[kIOAccelNumSurfaceMethods] = {
    [kIOAccelSurfaceReadLockOptions] = {
        NULL,                                                       // object (filled by getTargetAndMethodForIndex)
        (IOMethod) &VMAccelSurfaceClient::readLockOptions,         // func
        kIOUCScalarIScalarO,                                       // flags
        1,                                                          // count0 (1 scalar input: options)
        0                                                           // count1 (0 scalar outputs)
    },
    [kIOAccelSurfaceReadUnlockOptions] = {
        NULL,
        (IOMethod) &VMAccelSurfaceClient::readUnlockOptions,
        kIOUCScalarIScalarO,
        1,                                                          // 1 input: options
        0
    },
    [kIOAccelSurfaceGetState] = {
        NULL,
        (IOMethod) &VMAccelSurfaceClient::getState,
        kIOUCScalarIScalarO,
        0,                                                          // 0 inputs
        1                                                           // 1 output: state
    },
    [kIOAccelSurfaceWriteLockOptions] = {
        NULL,
        (IOMethod) &VMAccelSurfaceClient::writeLockOptions,
        kIOUCScalarIScalarO,
        1,                                                          // 1 input: options
        0
    },
    [kIOAccelSurfaceWriteUnlockOptions] = {
        NULL,
        (IOMethod) &VMAccelSurfaceClient::writeUnlockOptions,
        kIOUCScalarIScalarO,
        1,                                                          // 1 input: options
        0
    },
    [kIOAccelSurfaceRead] = {
        NULL,
        (IOMethod) &VMAccelSurfaceClient::read,
        kIOUCScalarIScalarO,
        0,
        0
    },
    [kIOAccelSurfaceSetShapeBacking] = {
        NULL,
        (IOMethod) &VMAccelSurfaceClient::setShapeBacking,
        kIOUCScalarIScalarO,
        0,
        0
    },
    [kIOAccelSurfaceSetIDMode] = {
        NULL,
        (IOMethod) &VMAccelSurfaceClient::setIDMode,
        kIOUCScalarIScalarO,
        2,                                                          // 2 inputs (WindowServer passes 2 args)
        0
    },
    [kIOAccelSurfaceSetScale] = {
        NULL,
        (IOMethod) &VMAccelSurfaceClient::setScale,
        kIOUCScalarIScalarO,
        0,
        0
    },
    [kIOAccelSurfaceSetShape] = {
        NULL,
        (IOMethod) &VMAccelSurfaceClient::setShape,
        kIOUCScalarIScalarO,
        2,                                                          // 2 inputs (revealed by test: mismatch 0x2 0x0)
        0
    },
    [kIOAccelSurfaceFlush] = {
        NULL,
        (IOMethod) &VMAccelSurfaceClient::flush,
        kIOUCScalarIScalarO,
        0,
        0
    },
    [kIOAccelSurfaceQueryLock] = {
        NULL,
        (IOMethod) &VMAccelSurfaceClient::queryLock,
        kIOUCScalarIScalarO,
        0,
        0
    },
    [kIOAccelSurfaceReadLock] = {
        NULL,
        (IOMethod) &VMAccelSurfaceClient::readLockSurface,
        kIOUCScalarIScalarO,
        0,
        0
    },
    [kIOAccelSurfaceReadUnlock] = {
        NULL,
        (IOMethod) &VMAccelSurfaceClient::readUnlockSurface,
        kIOUCScalarIScalarO,
        0,
        0
    },
    [kIOAccelSurfaceWriteLock] = {
        NULL,
        (IOMethod) &VMAccelSurfaceClient::writeLockSurface,
        kIOUCScalarIScalarO,
        0,
        0
    },
    [kIOAccelSurfaceWriteUnlock] = {
        NULL,
        (IOMethod) &VMAccelSurfaceClient::writeUnlockSurface,
        kIOUCScalarIScalarO,
        0,
        0
    },
    [kIOAccelSurfaceControl] = {
        NULL,
        (IOMethod) &VMAccelSurfaceClient::control,
        kIOUCScalarIScalarO,
        0,
        0
    },
    [kIOAccelSurfaceSetShapeBackingAndLength] = {
        NULL,
        (IOMethod) &VMAccelSurfaceClient::setShapeBackingAndLength,
        kIOUCScalarIScalarO,
        0,
        0
    },
};

VMAccelSurfaceClient* CLASS::withTask(task_t owningTask)
{
    VMAccelSurfaceClient* client = new VMAccelSurfaceClient;
    if (client) {
        if (!client->initWithTask(owningTask, nullptr, 0, nullptr)) {
            client->release();
            client = nullptr;
        }
    }
    return client;
}

bool CLASS::initWithTask(task_t owningTask, void* securityToken,
                        UInt32 type, OSDictionary* properties)
{
    if (!super::initWithTask(owningTask, securityToken, type, properties))
        return false;
    
    m_accelerator = nullptr;
    m_owning_task = owningTask;
    m_surface = nullptr;
    m_lock = IOLockAlloc();
    
    if (!m_lock) {
        IOLog("VMAccelSurfaceClient: Failed to allocate lock\n");
        return false;
    }
    
    // Publish the number of methods we support
    setProperty("IOUserClientMethodCount", kIOAccelNumSurfaceMethods, 32);
    IOLog("VMAccelSurfaceClient: Published %u methods\n", kIOAccelNumSurfaceMethods);
    
    IOLog("VMAccelSurfaceClient: Initialized for task %p\n", owningTask);
    return true;
}

bool CLASS::start(IOService* provider)
{
    if (!super::start(provider))
        return false;
    
    m_accelerator = OSDynamicCast(VMQemuVGAAccelerator, provider);
    if (!m_accelerator) {
        IOLog("VMAccelSurfaceClient: Provider is not VMQemuVGAAccelerator\n");
        return false;
    }
    
    // Allocate surface structure
    m_surface = (VMAccelSurface*)IOMalloc(sizeof(VMAccelSurface));
    if (!m_surface) {
        IOLog("VMAccelSurfaceClient: Failed to allocate surface\n");
        return false;
    }
    
    // Initialize surface
    bzero(m_surface, sizeof(VMAccelSurface));
    m_surface->surface_id = 0;
    m_surface->owning_task = m_owning_task;
    m_surface->is_locked = false;
    
    IOLog("VMAccelSurfaceClient: Started successfully\n");
    return true;
}

void CLASS::stop(IOService* provider)
{
    IOLog("VMAccelSurfaceClient: Stopping\n");
    
    if (m_surface) {
        if (m_surface->backing_memory) {
            m_surface->backing_memory->release();
            m_surface->backing_memory = nullptr;
        }
        IOFree(m_surface, sizeof(VMAccelSurface));
        m_surface = nullptr;
    }
    
    super::stop(provider);
}

void CLASS::free()
{
    if (m_lock) {
        IOLockFree(m_lock);
        m_lock = nullptr;
    }
    
    super::free();
}

IOReturn CLASS::clientClose()
{
    IOLog("VMAccelSurfaceClient: Client closing\n");
    
    if (!isInactive()) {
        terminate();
    }
    
    return kIOReturnSuccess;
}

IOReturn CLASS::clientDied()
{
    IOLog("VMAccelSurfaceClient: Client died\n");
    return clientClose();
}

// Provide framebuffer memory mapping to WindowServer
// This is called by IOConnectMapMemory() after lock operations
IOReturn CLASS::clientMemoryForType(UInt32 type, IOOptionBits* options, IOMemoryDescriptor** memory)
{
    IOLog("VMAccelSurfaceClient: clientMemoryForType type=%u\n", type);
    
    if (!memory) {
        return kIOReturnBadArgument;
    }
    
    // Get framebuffer from accelerator
    if (!m_accelerator) {
        IOLog("VMAccelSurfaceClient: ERROR - No accelerator for memory mapping\n");
        return kIOReturnNotReady;
    }
    
    // Get the framebuffer device (VMQemuVGA)
    IOService* framebuffer = m_accelerator->getProvider();
    if (!framebuffer) {
        IOLog("VMAccelSurfaceClient: ERROR - No framebuffer device\n");
        return kIOReturnNotReady;
    }
    
    // Get VRAM aperture - type 0 is main framebuffer
    IODeviceMemory* vram = framebuffer->getDeviceMemoryWithIndex(0);
    if (!vram) {
        IOLog("VMAccelSurfaceClient: ERROR - No VRAM aperture\n");
        return kIOReturnNoResources;
    }
    
    IOLog("VMAccelSurfaceClient: Mapping VRAM aperture: phys=0x%llx size=%llu bytes\n",
          (unsigned long long)vram->getPhysicalAddress(),
          (unsigned long long)vram->getLength());
    
    // Return memory descriptor for client to map
    *memory = vram;
    vram->retain();
    
    if (options) {
        *options = kIOMapInhibitCache;  // Allow read/write access
    }
    
    IOLog("VMAccelSurfaceClient: ✅ VRAM mapped successfully\n");
    return kIOReturnSuccess;
}

// OLD-STYLE DISPATCH: Returns IOExternalMethod array entry for requested selector.
// This provides MIG with argument count metadata at IPC boundary, allowing ALL operations
// to work across all macOS versions (Snow Leopard, Catalina, modern macOS).
// CRITICAL: Do NOT override externalMethod() - that prevents this from being called!

IOExternalMethod* CLASS::getTargetAndMethodForIndex(IOService** targetP, UInt32 index)
{
    IOLog("VMAccelSurfaceClient: getTargetAndMethodForIndex index=%u (max=%u)\n", 
          index, kIOAccelNumSurfaceMethods);
    
    if (index >= kIOAccelNumSurfaceMethods) {
        IOLog("VMAccelSurfaceClient: Invalid method index %u\n", index);
        return NULL;
    }
    
    if (targetP) {
        *targetP = this;
    }
    
    IOLog("VMAccelSurfaceClient: Returning method %u (count0=%llu, count1=%llu)\n",
          index, (unsigned long long)sMethods[index].count0, (unsigned long long)sMethods[index].count1);
    
    // Return const_cast since IOKit expects non-const pointer
    return (IOExternalMethod*)&sMethods[index];
}

//==============================================================================
// Surface Operation Handlers
//==============================================================================

// Handler implementations for IOAccelSurface operations (old-style 6-parameter signature)
// p1-p6 contain scalar inputs/outputs cast as void pointers
IOReturn CLASS::readLockOptions(void *p1, void *p2, void *p3, void *p4, void *p5, void *p6)
{
    uint32_t options = (uint32_t)(uintptr_t)p1;
    IOLog("VMAccelSurfaceClient: ReadLockOptions(0x%x)\n", options);
    return kIOReturnSuccess;
}

IOReturn CLASS::readUnlockOptions(void *p1, void *p2, void *p3, void *p4, void *p5, void *p6)
{
    uint32_t options = (uint32_t)(uintptr_t)p1;
    IOLog("VMAccelSurfaceClient: ReadUnlockOptions(0x%x)\n", options);
    return kIOReturnSuccess;
}

IOReturn CLASS::getState(void *p1, void *p2, void *p3, void *p4, void *p5, void *p6)
{
    // Output parameter (count1=1) goes in p1
    uint32_t* state = (uint32_t*)p1;
    if (!state) {
        return kIOReturnBadArgument;
    }
    *state = kIOAccelSurfaceStateIdleBit;
    IOLog("VMAccelSurfaceClient: GetState returning idle\n");
    return kIOReturnSuccess;
}

IOReturn CLASS::writeLockOptions(void *p1, void *p2, void *p3, void *p4, void *p5, void *p6)
{
    uint32_t options = (uint32_t)(uintptr_t)p1;
    IOLog("VMAccelSurfaceClient: WriteLockOptions(0x%x)\n", options);
    return kIOReturnSuccess;
}

IOReturn CLASS::writeUnlockOptions(void *p1, void *p2, void *p3, void *p4, void *p5, void *p6)
{
    uint32_t options = (uint32_t)(uintptr_t)p1;
    IOLog("VMAccelSurfaceClient: WriteUnlockOptions(0x%x)\n", options);
    return kIOReturnSuccess;
}

IOReturn CLASS::read(void *p1, void *p2, void *p3, void *p4, void *p5, void *p6)
{
    IOLog("VMAccelSurfaceClient: Read stub\n");
    return kIOReturnSuccess;
}

IOReturn CLASS::setShapeBacking(void *p1, void *p2, void *p3, void *p4, void *p5, void *p6)
{
    IOLog("VMAccelSurfaceClient: SetShapeBacking stub\n");
    return kIOReturnSuccess;
}

IOReturn CLASS::setIDMode(void *p1, void *p2, void *p3, void *p4, void *p5, void *p6)
{
    IOLog("VMAccelSurfaceClient: SetIDMode stub\n");
    return kIOReturnSuccess;
}

IOReturn CLASS::setScale(void *p1, void *p2, void *p3, void *p4, void *p5, void *p6)
{
    IOLog("VMAccelSurfaceClient: SetScale stub\n");
    return kIOReturnSuccess;
}

IOReturn CLASS::setShape(void *p1, void *p2, void *p3, void *p4, void *p5, void *p6)
{
    IOLog("VMAccelSurfaceClient: SetShape stub\n");
    return kIOReturnSuccess;
}

IOReturn CLASS::flush(void *p1, void *p2, void *p3, void *p4, void *p5, void *p6)
{
    IOLog("VMAccelSurfaceClient: Flush stub\n");
    return kIOReturnSuccess;
}

IOReturn CLASS::queryLock(void *p1, void *p2, void *p3, void *p4, void *p5, void *p6)
{
    IOLog("VMAccelSurfaceClient: QueryLock stub\n");
    return kIOReturnSuccess;
}

IOReturn CLASS::readLockSurface(void *p1, void *p2, void *p3, void *p4, void *p5, void *p6)
{
    IOLog("VMAccelSurfaceClient: ReadLock stub\n");
    return kIOReturnSuccess;
}

IOReturn CLASS::readUnlockSurface(void *p1, void *p2, void *p3, void *p4, void *p5, void *p6)
{
    IOLog("VMAccelSurfaceClient: ReadUnlock stub\n");
    return kIOReturnSuccess;
}

IOReturn CLASS::writeLockSurface(void *p1, void *p2, void *p3, void *p4, void *p5, void *p6)
{
    IOLog("VMAccelSurfaceClient: WriteLock stub\n");
    return kIOReturnSuccess;
}

IOReturn CLASS::writeUnlockSurface(void *p1, void *p2, void *p3, void *p4, void *p5, void *p6)
{
    IOLog("VMAccelSurfaceClient: WriteUnlock stub\n");
    return kIOReturnSuccess;
}

IOReturn CLASS::control(void *p1, void *p2, void *p3, void *p4, void *p5, void *p6)
{
    IOLog("VMAccelSurfaceClient: Control stub\n");
    return kIOReturnSuccess;
}

IOReturn CLASS::setShapeBackingAndLength(void *p1, void *p2, void *p3, void *p4, void *p5, void *p6)
{
    IOLog("VMAccelSurfaceClient: SetShapeBackingAndLength stub\n");
    return kIOReturnSuccess;
}



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
        kIOUCScalarIStructO,                                       // worked example VMsvga2Surface.cpp:74 — StructO(1, var)
        1,                                                          // 1 scalar input: options
        kIOUCVariableStructureSize                                  // out: IOAccelSurfaceInformation (handler slot p2)
    },
    [kIOAccelSurfaceReadUnlockOptions] = {
        NULL,
        (IOMethod) &VMAccelSurfaceClient::readUnlockOptions,
        kIOUCScalarIScalarO,                                       // :75 — already correct
        1,                                                          // 1 input: options
        0
    },
    [kIOAccelSurfaceGetState] = {
        NULL,
        (IOMethod) &VMAccelSurfaceClient::getState,
        kIOUCScalarIScalarO,                                       // :76 — already correct
        0,                                                          // 0 inputs
        1                                                           // 1 output: state
    },
    [kIOAccelSurfaceWriteLockOptions] = {
        NULL,
        (IOMethod) &VMAccelSurfaceClient::writeLockOptions,
        kIOUCScalarIStructO,                                       // :79 — StructO(1, var), not (1,0)
        1,                                                          // 1 scalar input: options
        kIOUCVariableStructureSize                                  // out: IOAccelSurfaceInformation (handler slot p2)
    },
    [kIOAccelSurfaceWriteUnlockOptions] = {
        NULL,
        (IOMethod) &VMAccelSurfaceClient::writeUnlockOptions,
        kIOUCScalarIScalarO,                                       // :80 — already correct
        1,                                                          // 1 input: options
        0
    },
    [kIOAccelSurfaceRead] = {
        NULL,
        (IOMethod) &VMAccelSurfaceClient::read,
        kIOUCScalarIStructI,                                       // :81 — StructI(0, var)
        0,
        kIOUCVariableStructureSize                                  // in struct: read params (handler slot p1)
    },
    [kIOAccelSurfaceSetShapeBacking] = {
        NULL,
        (IOMethod) &VMAccelSurfaceClient::setShapeBacking,
        kIOUCScalarIStructI,                                       // :82 — THE BACKING RUNG: StructI(4, var)
        4,                                                          // 4 scalars, then region struct-in
        kIOUCVariableStructureSize                                  // in: IOAccelDeviceRegion (handler slot p5)
    },
    [kIOAccelSurfaceSetIDMode] = {
        NULL,
        (IOMethod) &VMAccelSurfaceClient::setIDMode,
        kIOUCScalarIScalarO,                                       // :83 — already correct
        2,                                                          // (wID, modebits)
        0
    },
    [kIOAccelSurfaceSetScale] = {
        NULL,
        (IOMethod) &VMAccelSurfaceClient::setScale,
        kIOUCScalarIStructI,                                       // :84 — StructI(1, var)
        1,
        kIOUCVariableStructureSize                                  // in struct: scale params (handler slot p2)
    },
    [kIOAccelSurfaceSetShape] = {
        NULL,
        (IOMethod) &VMAccelSurfaceClient::setShape,
        kIOUCScalarIStructI,                                       // worked example: VMsvga2Surface.cpp:85
        2,                                                          // 2 scalars (options, fbIndex)
        kIOUCVariableStructureSize                                  // + variable IOAccelDeviceRegion struct-in
    },
    [kIOAccelSurfaceFlush] = {
        NULL,
        (IOMethod) &VMAccelSurfaceClient::flush,
        kIOUCScalarIScalarO,                                       // :86 — (2,0), not (0,0): takes (framebufferMask, options)
        2,                                                          // 2 scalar inputs
        0
    },
    [kIOAccelSurfaceQueryLock] = {
        NULL,
        (IOMethod) &VMAccelSurfaceClient::queryLock,
        kIOUCScalarIScalarO,                                       // :87 — already correct; no inputs, no outputs —
        0,                                                          // the answer IS the return code
        0
    },
    [kIOAccelSurfaceReadLock] = {
        NULL,
        (IOMethod) &VMAccelSurfaceClient::readLockSurface,
        kIOUCScalarIStructO,                                       // :88 — StructO(0, var): out IOAccelSurfaceInformation
        0,
        kIOUCVariableStructureSize                                  // (handler slots p1=info, p2=infoSize)
    },
    [kIOAccelSurfaceReadUnlock] = {
        NULL,
        (IOMethod) &VMAccelSurfaceClient::readUnlockSurface,
        kIOUCScalarIScalarO,                                       // :89 — already correct
        0,
        0
    },
    [kIOAccelSurfaceWriteLock] = {
        NULL,
        (IOMethod) &VMAccelSurfaceClient::writeLockSurface,
        kIOUCScalarIStructO,                                       // :90 — StructO(0, var): out IOAccelSurfaceInformation
        0,
        kIOUCVariableStructureSize                                  // (handler slots p1=info, p2=infoSize)
    },
    [kIOAccelSurfaceWriteUnlock] = {
        NULL,
        (IOMethod) &VMAccelSurfaceClient::writeUnlockSurface,
        kIOUCScalarIScalarO,                                       // :91 — already correct
        0,
        0
    },
    [kIOAccelSurfaceControl] = {
        NULL,
        (IOMethod) &VMAccelSurfaceClient::control,
        kIOUCScalarIScalarO,                                       // :92 — (2,1), not (0,0)
        2,                                                          // 2 scalar inputs
        1                                                           // 1 scalar output
    },
    [kIOAccelSurfaceSetShapeBackingAndLength] = {
        NULL,
        (IOMethod) &VMAccelSurfaceClient::setShapeBackingAndLength,
        kIOUCScalarIStructI,                                       // :93 — StructI(5, var)
        5,                                                          // 5 scalars, then region struct-in
        kIOUCVariableStructureSize                                  // in: IOAccelDeviceRegion (handler slot p6)
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
    m_creator_logged = false;
    m_surface = nullptr;
    m_lock = IOLockAlloc();
    
    if (!m_lock) {
        IOLog("VMAccelSurfaceClient: Failed to allocate lock\n");
        return false;
    }
    
    // Publish the number of methods we support
    setProperty("IOUserClientMethodCount", kIOAccelNumSurfaceMethods, 32);
    IOLog("VMAccelSurfaceClient: Published %u methods\n", kIOAccelNumSurfaceMethods);

    /* Caller-attribution note (2026-08-15, cost three boots): kxld
     * refused get_bsdtask_info/proc_pid/proc_name, then pid_for_task,
     * then proc_selfpid/proc_selfname — ALL unresolved 0xdc008016.
     * ROOT CAUSE (read from the artifact): OSBundleLibraries declares
     * iokit/libkern/mach KPIs and NOT com.apple.kpi.bsd — every BSD
     * symbol is unresolvable by declaration regardless of the kernel's
     * symbol table. Fix: add the bsd KPI dependency, then the
     * proc_self* attribution probe, on its own boot. Boot is the only
     * linkage arbiter (kextutil -n -t falsified for this class). */
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

    /* Creator attribution, LATE site #1 (2026-08-15, user timing
     * caution): IOUserClientCreator is set by the IOKit machinery
     * AROUND client creation — reading it in initWithTask may run
     * before the property is populated, and an empty value there
     * would read as "no creator" rather than "read too soon".
     * start() runs with the client fully constructed. Logged from
     * BOTH this site and the first externalMethod dispatch on the
     * same boot: a difference between the two IS the timing datum. */
    {
        OSObject *v = getProperty("IOUserClientCreator");
        OSString *s = OSDynamicCast(OSString, v);
        IOLog("VMAccelSurfaceClient: creator@start = \"%s\"\n",
              (s && s->getLength()) ? s->getCStringNoCopy()
                                     : "(empty/unset at start())");
    }
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

    /* Creator attribution, LATE site #2: first externalMethod dispatch —
     * the client is fully constructed and has served a call by now. If
     * this reads populated while start()'s read was empty, the timing
     * datum is "the property lands between start and first dispatch" —
     * NOT "no creator". Once per client. */
    if (!m_creator_logged) {
        m_creator_logged = true;
        OSObject *v = getProperty("IOUserClientCreator");
        OSString *s = OSDynamicCast(OSString, v);
        IOLog("VMAccelSurfaceClient: creator@dispatch = \"%s\"\n",
              (s && s->getLength()) ? s->getCStringNoCopy()
                                    : "(empty at dispatch too)");
    }

    IOLog("VMAccelSurfaceClient: Returning method %u (count0=%llu, count1=%llu)\n",
          index, (unsigned long long)sMethods[index].count0, (unsigned long long)sMethods[index].count1);

    // Return const_cast since IOKit expects non-const pointer
    return (IOExternalMethod*)&sMethods[index];
}

//==============================================================================
// Surface Operation Handlers
//==============================================================================
// MIG-PROBE HARDENING (2026-08-13): every handler except GetState returns
// kIOReturnUnsupported. The November crash cause (commit 33fe55b disable)
// was lock handlers returning SUCCESS without mapping memory — WindowServer
// dereferenced garbage at composition time. Unsupported promises nothing,
// so WindowServer takes its software fallback. The handler-entry IOLog is
// the kernel-reach proof the MIG probe needs: a MIG rejection dies at the
// IPC boundary with 0x10000003 and produces NO kernel log; any handler log
// line proves the call crossed the boundary. GetState is the exception —
// it is WindowServer's first call (0-in/1-out), returns idle, and is the
// known-good control from the d98 experiments.
//
// Old-style 6-parameter IOMethod signature: p1..p6 carry scalar
// inputs/outputs cast as void pointers.

IOReturn CLASS::readLockOptions(void *p1, void *p2, void *p3, void *p4, void *p5, void *p6)
{
    uint32_t options = (uint32_t)(uintptr_t)p1;
    IOLog("VMAccelSurfaceClient: ReadLockOptions(0x%x) -> Unsupported\n", options);
    return kIOReturnUnsupported;
}

IOReturn CLASS::readUnlockOptions(void *p1, void *p2, void *p3, void *p4, void *p5, void *p6)
{
    uint32_t options = (uint32_t)(uintptr_t)p1;
    IOLog("VMAccelSurfaceClient: ReadUnlockOptions(0x%x) -> Unsupported\n", options);
    return kIOReturnUnsupported;
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
    IOLog("VMAccelSurfaceClient: WriteLockOptions(0x%x) -> Unsupported\n", options);
    return kIOReturnUnsupported;
}

IOReturn CLASS::writeUnlockOptions(void *p1, void *p2, void *p3, void *p4, void *p5, void *p6)
{
    uint32_t options = (uint32_t)(uintptr_t)p1;
    IOLog("VMAccelSurfaceClient: WriteUnlockOptions(0x%x) -> Unsupported\n", options);
    return kIOReturnUnsupported;
}

IOReturn CLASS::read(void *p1, void *p2, void *p3, void *p4, void *p5, void *p6)
{
    IOLog("VMAccelSurfaceClient: Read -> Unsupported\n");
    return kIOReturnUnsupported;
}

IOReturn CLASS::setShapeBacking(void *p1, void *p2, void *p3, void *p4, void *p5, void *p6)
{
    /* Raw args logged: the next-rung decision needs the call's actual
     * values, not just its identity (pre-registered: stays Unsupported —
     * backing claims memory). */
    IOLog("VMAccelSurfaceClient: SetShapeBacking(a1=0x%llx a2=0x%llx "
          "a3=0x%llx) -> Unsupported\n",
          (unsigned long long)(uintptr_t)p1,
          (unsigned long long)(uintptr_t)p2,
          (unsigned long long)(uintptr_t)p3);
    return kIOReturnUnsupported;
}

IOReturn CLASS::setIDMode(void *p1, void *p2, void *p3, void *p4, void *p5, void *p6)
{
    /* First REAL selector (2026-08-15). Argument semantics verified
     * against the worked example (VMsvga2Surface.cpp:1304):
     * set_id_mode(uintptr_t wID, eIOAccelSurfaceModeBits modebits).
     * wID==1 is WindowServer's own surface (their :1309 comment; wID==1
     * also gates createPrimaryScreen behind haveFrontBuffer() there —
     * we return success unconditionally, an unbacked instance to note).
     * First boot's observed call: wID=0x1, modebits=0x24. Stored under
     * the correct labels; the earlier "mode=0x1" log was mislabelled
     * (wID in the mode slot) — corrected before anything reads it. */
    uint32_t wid = (uint32_t)(uintptr_t)p1;
    uint32_t modebits = (uint32_t)(uintptr_t)p2;
    if (m_surface) {
        m_surface->surface_id = wid;
        m_surface->pixel_format = modebits;
    }
    IOLog("VMAccelSurfaceClient: SetIDMode(wID=0x%x modebits=0x%x%s) -> "
          "STORED\n", wid, modebits, (wid == 1) ? " [WindowServer surface]" : "");
    return kIOReturnSuccess;
}

IOReturn CLASS::setScale(void *p1, void *p2, void *p3, void *p4, void *p5, void *p6)
{
    IOLog("VMAccelSurfaceClient: SetScale -> Unsupported\n");
    return kIOReturnUnsupported;
}

IOReturn CLASS::setShape(void *p1, void *p2, void *p3, void *p4, void *p5, void *p6)
{
    /* SECOND real selector (2026-08-15). Table row aligned to the worked
     * example (VMsvga2Surface.cpp:85/:1374): set_shape(options,
     * framebufferIndex, IOAccelDeviceRegion const* rgn, size_t rgnSize).
     * Old-style ScalarIStructI(count0=2, variable) fills slots as
     * p1=options, p2=fbIndex, p3=kernel pointer to the copied-in
     * region, p4=the size — BY VALUE per the worked example's
     * signature; if that reading is wrong the sanity gate below makes
     * it visible (p4 logged raw both ways; no blind deref of p4).
     * Deliverability: storing geometry is honest — no memory claimed.
     * num_rects==0 is a KNOWN real case (VMsvga2 carries a fixup for
     * WindowServer sending it); the "struct didn't arrive"
     * discriminator is a NULL/non-kernel rgn pointer or absurd size. */
    uint32_t options = (uint32_t)(uintptr_t)p1;
    uintptr_t fbIdx = (uintptr_t)p2;
    IOAccelDeviceRegion *rgn = (IOAccelDeviceRegion *)p3;
    size_t rgnSizeV = (size_t)(uintptr_t)p4;

    uintptr_t rgnAddr = (uintptr_t)rgn;
    bool kernelCanonical = (rgnAddr >= 0xffffff8000000000ULL);
    bool sizeSane = (rgnSizeV >= sizeof(IOAccelDeviceRegion) &&
                     rgnSizeV <= (1u << 20));

    IOLog("VMAccelSurfaceClient: SetShape(options=0x%x fbIndex=%llu "
          "rgn=0x%llx size=%llu%s)\n",
          options, (unsigned long long)fbIdx,
          (unsigned long long)rgnAddr, (unsigned long long)rgnSizeV,
          sizeSane ? "" : " [size impl? see raw p4]");

    if (!kernelCanonical || !sizeSane) {
        IOLog("VMAccelSurfaceClient: SetShape — struct did NOT arrive "
              "usefully (kernelCanonical=%d sizeSane=%d) -> "
              "Unsupported\n", kernelCanonical, sizeSane);
        return kIOReturnUnsupported;
    }

    /* Region readable: log the geometry, sanity-checkable against the
     * live display (expect ~1680x1050 for full-screen surfaces). */
    IOLog("VMAccelSurfaceClient: SetShape region — num_rects=%u "
          "bounds=(x=%d y=%d w=%d h=%d)\n",
          rgn->num_rects,
          (int)rgn->bounds.x, (int)rgn->bounds.y,
          (int)rgn->bounds.w, (int)rgn->bounds.h);
    if (rgn->num_rects > 0 && rgnSizeV >= (sizeof(IOAccelDeviceRegion) +
                                           sizeof(IOAccelBounds))) {
        IOLog("VMAccelSurfaceClient: SetShape rect[0]=(x=%d y=%d w=%d "
              "h=%d)\n",
              (int)rgn->rect[0].x, (int)rgn->rect[0].y,
              (int)rgn->rect[0].w, (int)rgn->rect[0].h);
    }

    if (m_surface) {
        m_surface->width  = (uint32_t)(rgn->bounds.w > 0 ? rgn->bounds.w : 0);
        m_surface->height = (uint32_t)(rgn->bounds.h > 0 ? rgn->bounds.h : 0);
    }
    return kIOReturnSuccess;
}

IOReturn CLASS::flush(void *p1, void *p2, void *p3, void *p4, void *p5, void *p6)
{
    IOLog("VMAccelSurfaceClient: Flush -> Unsupported\n");
    return kIOReturnUnsupported;
}

IOReturn CLASS::queryLock(void *p1, void *p2, void *p3, void *p4, void *p5, void *p6)
{
    IOLog("VMAccelSurfaceClient: QueryLock -> Unsupported\n");
    return kIOReturnUnsupported;
}

IOReturn CLASS::readLockSurface(void *p1, void *p2, void *p3, void *p4, void *p5, void *p6)
{
    IOLog("VMAccelSurfaceClient: ReadLock -> Unsupported\n");
    return kIOReturnUnsupported;
}

IOReturn CLASS::readUnlockSurface(void *p1, void *p2, void *p3, void *p4, void *p5, void *p6)
{
    IOLog("VMAccelSurfaceClient: ReadUnlock -> Unsupported\n");
    return kIOReturnUnsupported;
}

IOReturn CLASS::writeLockSurface(void *p1, void *p2, void *p3, void *p4, void *p5, void *p6)
{
    IOLog("VMAccelSurfaceClient: WriteLock -> Unsupported\n");
    return kIOReturnUnsupported;
}

IOReturn CLASS::writeUnlockSurface(void *p1, void *p2, void *p3, void *p4, void *p5, void *p6)
{
    IOLog("VMAccelSurfaceClient: WriteUnlock -> Unsupported\n");
    return kIOReturnUnsupported;
}

IOReturn CLASS::control(void *p1, void *p2, void *p3, void *p4, void *p5, void *p6)
{
    IOLog("VMAccelSurfaceClient: Control -> Unsupported\n");
    return kIOReturnUnsupported;
}

IOReturn CLASS::setShapeBackingAndLength(void *p1, void *p2, void *p3, void *p4, void *p5, void *p6)
{
    /* Raw args logged (see SetShapeBacking note). */
    IOLog("VMAccelSurfaceClient: SetShapeBackingAndLength(a1=0x%llx "
          "a2=0x%llx a3=0x%llx) -> Unsupported\n",
          (unsigned long long)(uintptr_t)p1,
          (unsigned long long)(uintptr_t)p2,
          (unsigned long long)(uintptr_t)p3);
    return kIOReturnUnsupported;
}



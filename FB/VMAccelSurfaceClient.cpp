/*
 * VMAccelSurfaceClient.cpp
 * IOAccelSurface UserClient implementation for WindowServer 2D acceleration
 */

#include "VMAccelSurfaceClient.h"
#include "VMQemuVGAAccelerator.h"
#include "VMVirtIOGPU.h"
#include "VMVirtIOFramebuffer.h"
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
    m_skip_write_lock_once = false;
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

    /* Teardown obligation (2026-08-16): the backing mapping
     * OUTLIVES unlock by design (lazy-create at first write lock,
     * persist across cycles — worked example :1258-1263/:1276-1281).
     * It must be released here regardless of what the caller did:
     * normal close, surface destroy, or the client DYING while
     * holding the lock (WindowServer restart mid-lock) — all
     * funnel through clientClose/clientDied -> terminate -> stop.
     * Same leak shape as the backing-descriptor table fix. */
    if (m_surface) {
        if (m_surface->is_locked)
            IOLog("VMAccelSurfaceClient: stop with lock HELD "
                  "(client died mid-lock?) — releasing anyway\n");
        if (m_surface->kernel_map) {
            m_surface->kernel_map->release();
            m_surface->kernel_map = nullptr;
        }
        if (m_surface->client_map) {
            m_surface->client_map->release();
            m_surface->client_map = nullptr;
        }
        if (m_surface->backing_memory) {
            m_surface->backing_memory->complete();
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
    /* Depth field -> bytes per pixel (eIOAccelSurfaceModeBits,
     * IOAccelSurfaceConnect.h:105-122). Observed call: 0x24 = depth
     * 0x4 (ColorDepth8888, 4 bpp) + WindowedBit. Depths we cannot
     * name store bpp=0: WriteLock then refuses (NotReady) rather
     * than sizing a backing on a guess. */
    uint32_t depth = modebits & 0x0F;
    uint32_t bpp = 0;
    if (depth == 0x3)       bpp = 2;   /* 1555 */
    else if (depth == 0x4)  bpp = 4;   /* 8888 */
    else if (depth == 0xA)  bpp = 4;   /* BGRA32 */
    if (m_surface) {
        m_surface->surface_id = wid;
        m_surface->pixel_format = modebits;
        m_surface->bytes_per_pixel = bpp;
    }
    IOLog("VMAccelSurfaceClient: SetIDMode(wID=0x%x modebits=0x%x "
          "depth=0x%x bpp=%u%s) -> STORED\n",
          wid, modebits, depth, bpp,
          (wid == 1) ? " [WindowServer surface]" : "");
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

    /* Empty-region no-op (worked example :136-144, :1607): num_rects!=0
     * with rect[0] degenerate IS an empty shape — the observed 0x1
     * pair-member (bounds 1x1, rect 0x0) is exactly this. Returns
     * Success WITHOUT touching geometry. */
    bool rectReadable = (rgn->num_rects > 0 &&
                         rgnSizeV >= (sizeof(IOAccelDeviceRegion) +
                                      sizeof(IOAccelBounds)));
    if (rectReadable) {
        IOLog("VMAccelSurfaceClient: SetShape rect[0]=(x=%d y=%d w=%d "
              "h=%d)\n",
              (int)rgn->rect[0].x, (int)rgn->rect[0].y,
              (int)rgn->rect[0].w, (int)rgn->rect[0].h);
        if (rgn->rect[0].w <= 0 || rgn->rect[0].h <= 0) {
            IOLog("VMAccelSurfaceClient: SetShape — empty region "
                  "(rect[0] degenerate): no-op, geometry untouched "
                  "(:1607)\n");
            return kIOReturnSuccess;
        }
    }

    /* Geometry updates ONLY under IdentityScaleBit (:1639-1655).
     * The observed 0x1 call lacks it; without the gate every second
     * shape overwrote the stored geometry. For wID==1 the worked
     * example's bFromGFB branch treats the surface as a SCREEN-SIZED
     * buffer that shapes select SUB-REGIONS of (bounds = the damage
     * region; calculateSurfaceInformation adds bounds.y*rowBytes +
     * bounds.x*bpp to the handed-out address, :390/:417). So we store
     * the shape origin AND keep a GROW-ONLY base extent = max
     * bounds.x+w / bounds.y+h ever seen — the early-boot 1680x1050
     * shapes establish it. */
    if (options & kIOAccelSurfaceShapeIdentityScaleBit) {
        if (m_surface) {
            m_surface->shape_x = (uint32_t)(rgn->bounds.x > 0 ? rgn->bounds.x : 0);
            m_surface->shape_y = (uint32_t)(rgn->bounds.y > 0 ? rgn->bounds.y : 0);
            m_surface->width   = (uint32_t)(rgn->bounds.w > 0 ? rgn->bounds.w : 0);
            m_surface->height  = (uint32_t)(rgn->bounds.h > 0 ? rgn->bounds.h : 0);
            uint32_t ext_x = m_surface->shape_x + m_surface->width;
            uint32_t ext_y = m_surface->shape_y + m_surface->height;
            if (ext_x > m_surface->base_w) m_surface->base_w = ext_x;
            if (ext_y > m_surface->base_h) m_surface->base_h = ext_y;
        }
        IOLog("VMAccelSurfaceClient: SetShape — IdentityScale: shape "
              "(%u,%u %ux%u) STORED, extent now %ux%u\n",
              m_surface ? m_surface->shape_x : 0,
              m_surface ? m_surface->shape_y : 0,
              m_surface ? m_surface->width : 0,
              m_surface ? m_surface->height : 0,
              m_surface ? m_surface->base_w : 0,
              m_surface ? m_surface->base_h : 0);
    } else {
        IOLog("VMAccelSurfaceClient: SetShape — no IdentityScaleBit: "
              "geometry untouched (:1639)\n");
    }

    /* 10.6 WindowServer Window-Grab deadlock avoidance (worked
     * example :1658-1666, REQUIRED on this target — our surface IS
     * wID==1, the exact guarded case): set_shape(wID==1, options
     * ==0x5) arms a one-shot skip of the lock-bit on the next
     * write_lock. Observed options so far: 0xd/0x1; 0x5 not yet
     * seen, guard armed regardless. */
    if (m_surface && m_surface->surface_id == 1 && options == 0x5) {
        m_skip_write_lock_once = true;
        IOLog("VMAccelSurfaceClient: SetShape — arming "
              "bSkipWriteLockOnce (wID==1, options==0x5)\n");
    }
    return kIOReturnSuccess;
}

IOReturn CLASS::flush(void *p1, void *p2, void *p3, void *p4, void *p5, void *p6)
{
    /* FIFTH real selector — blit-only flush (2026-08-16), lands as a
     * PAIR with WriteLock (lock success changes where WindowServer
     * draws; a lock without a working flush is a broken-window state
     * — the blue-screen boot proved it). Design (user decision):
     * NO second resource, NO scanout switching, NO new virtio
     * commands. Copy the surface's mapped buffer into the
     * FRAMEBUFFER's backing at the shape offset, row by row; the
     * existing refresh timer (16 ms tick, FULL_REFRESH_INTERVAL=4
     * → ~15 Hz effective transfer, VMVirtIOFramebuffer.h:90)
     * carries it to the host on its next transfer. This is the
     * virtio-gpu equivalent of what SVGA2 gets for free (its
     * backing IS device VRAM).
     * Table row :86 — (framebufferMask, options) scalars in.
     * Three blit rules (user; cheap right, expensive wrong):
     * clip to the shape rect; strides are INDEPENDENT (surface
     * base_w*4 vs FB m_width*4 — 6720 vs 7680 on the observed
     * boots); straight byte copy — format arbiter is the boot's
     * visual (surface 0x24/8888 and FB resource B8G8R8A8 written
     * by the same WindowServer; a channel swap would show as a
     * rendering bug, not a format error). */
    uint32_t fbMask = (uint32_t)(uintptr_t)p1;
    uint32_t options = (uint32_t)(uintptr_t)p2;

    if (!m_surface || !m_surface->backing_memory || !m_surface->client_map ||
        !m_surface->bytes_per_row) {
        IOLog("VMAccelSurfaceClient: Flush -> NotReady (no backing)\n");
        return kIOReturnNotReady;
    }

    VMVirtIOFramebuffer* fb = nullptr;
    if (m_accelerator)
        fb = OSDynamicCast(VMVirtIOFramebuffer, m_accelerator->getProvider());
    uint8_t* dst = (uint8_t*)(fb ? fb->getBackingKernelPtr() : nullptr);
    uint8_t* src = m_surface->kernel_map
                       ? (uint8_t*)m_surface->kernel_map->getAddress()
                       : nullptr;
    if (!dst || !src) {
        IOLog("VMAccelSurfaceClient: Flush -> NotReady (fb=%p src=%p)\n",
              dst, src);
        return kIOReturnNotReady;
    }

    uint32_t fbW = fb->getFbWidth();
    uint32_t fbH = fb->getFbHeight();
    uint64_t fbStride = (uint64_t)fbW * 4;
    uint64_t surfStride = m_surface->bytes_per_row;
    uint32_t bpp = m_surface->bytes_per_pixel;

    /* Clip the shape rect to BOTH buffers — never assume full size
     * (46x22 at x=1634, full-screen, and everything between have
     * all been observed). */
    uint32_t x = m_surface->shape_x, y = m_surface->shape_y;
    uint32_t w = m_surface->width, h = m_surface->height;
    uint32_t orig_w = w, orig_h = h;
    if (x >= fbW || y >= fbH) {
        IOLog("VMAccelSurfaceClient: Flush — shape (%u,%u %ux%u) entirely "
              "off-FB (%ux%u): nothing to blit -> Success\n",
              x, y, w, h, fbW, fbH);
        return kIOReturnSuccess;
    }
    if (x + w > fbW)  w = fbW - x;
    if (y + h > fbH)  h = fbH - y;
    if (w == 0 || h == 0) {
        IOLog("VMAccelSurfaceClient: Flush — empty after clip -> Success\n");
        return kIOReturnSuccess;
    }

    /* Self-check (fixed formula, was over-strict by a partial row):
     * last byte touched = (y+h-1)*stride + (x+w)*bpp. */
    uint64_t lastS = (uint64_t)(y + h - 1) * surfStride + (uint64_t)(x + w) * bpp;
    uint64_t lastD = (uint64_t)(y + h - 1) * fbStride  + (uint64_t)(x + w) * bpp;
    if (lastS > m_surface->backing_memory->getLength() ||
        lastD > fb->getBackingLength()) {
        IOLog("VMAccelSurfaceClient: MISMATCH — blit exceeds a buffer: "
              "src last=%llu len=%llu dst last=%llu len=%llu — SKIPPING "
              "copy, returning Success (no corruption)\n",
              (unsigned long long)lastS,
              (unsigned long long)m_surface->backing_memory->getLength(),
              (unsigned long long)lastD,
              (unsigned long long)fb->getBackingLength());
        return kIOReturnSuccess;
    }

    IOLockLock(m_lock);
    /* Row by row — strides are independently determined. */
    for (uint32_t r = 0; r < h; r++) {
        uint8_t* s = src + (uint64_t)(y + r) * surfStride + (uint64_t)x * bpp;
        uint8_t* d = dst + (uint64_t)(y + r) * fbStride  + (uint64_t)x * bpp;
        memcpy(d, s, (size_t)w * bpp);
    }
    IOLockUnlock(m_lock);

    IOLog("VMAccelSurfaceClient: Flush -> Success (blit %ux%u at (%u,%u)%s "
          "surfStride=%llu fbStride=%llu — timer carries to host)\n",
          w, h, x, y,
          (w != orig_w || h != orig_h) ? " [CLIPPED]" : "",
          (unsigned long long)surfStride, (unsigned long long)fbStride);
    return kIOReturnSuccess;
}

IOReturn CLASS::queryLock(void *p1, void *p2, void *p3, void *p4, void *p5, void *p6)
{
    /* THIRD real selector (2026-08-16); state-honest since the
     * WriteLock rung: semantics from the worked example
     * (VMsvga2Surface.cpp:1461-1466) — pure state query, the
     * answer IS the return code. CannotLock if the write lock is
     * held, Success if lockable. */
    bool held = (m_surface && m_surface->is_locked);
    IOLog("VMAccelSurfaceClient: QueryLock -> %s\n",
          held ? "CannotLock (write lock held)" : "Success (lockable)");
    return held ? kIOReturnCannotLock : kIOReturnSuccess;
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
    /* FOURTH real selector — the backing rung (2026-08-16).
     * Contract from the worked example, VMsvga2Surface.cpp:
     * - row 90: StructO(0, var) — p1 = IOAccelSurfaceInformation*,
     *   p2 = size_t* infoSize; caller's buffer, driver fills it.
     * - surface_write_lock_options :1242-1273: BadArgument if
     *   *infoSize < sizeof; NotReady without id/geometry;
     *   CannotLock on double-lock; NoMemory if backing fails.
     * - Direction (:1089-1098): the address must be valid in the
     *   OWNING task (WindowServer). Mechanism = IOBufferMemoryDescriptor
     *   (inTaskWithOptions, kIOMemoryKernelUserShared — idiom
     *   :1111-1128) + createMappingInTask(m_owning_task). KPI
     *   evidence: the worked example RUNS this on 10.6 — target
     *   precedent, not OSBundleLibraries reasoning.
     * - Lifetime: lazy-create at FIRST lock, persist across
     *   lock/unlock cycles, grow-only (:543 "only resize if it
     *   needs to grow"), freed at surface destroy (our stop()).
     * - Fill (:377-396): address[0] = mapping base + buffer offset
     *   (shape_y*rowBytes + shape_x*bpp, :390/:417); rowBytes =
     *   the ALLOCATION's stride (screen stride, :408/:393);
     *   width/height = current shape bounds; colorTemperature[0]
     *   = 0x1CCCC (GeForce.kext precedent).
     * What this rung does NOT claim: no CPU->host transfer —
     * pixels written here stay in guest memory until a later
     * rung (SVGA2 needs none: its backing IS device VRAM, :558). */
    IOAccelSurfaceInformation* info = (IOAccelSurfaceInformation*)p1;
    size_t* infoSize = (size_t*)p2;

    if (!info || !infoSize || *infoSize < sizeof(IOAccelSurfaceInformation)) {
        IOLog("VMAccelSurfaceClient: WriteLock -> BadArgument "
              "(info=%p infoSize=%p cap=%llu need=%u)\n",
              info, infoSize, infoSize ? (unsigned long long)*infoSize : 0ULL,
              (unsigned)sizeof(IOAccelSurfaceInformation));
        return kIOReturnBadArgument;
    }
    if (!m_surface || !m_surface->surface_id || !m_surface->width ||
        !m_surface->height || !m_surface->bytes_per_pixel || !m_surface->base_w) {
        IOLog("VMAccelSurfaceClient: WriteLock -> NotReady (no id/geometry/"
              "bpp)\n");
        return kIOReturnNotReady;
    }
    bzero(info, *infoSize);

    IOLockLock(m_lock);

    /* 10.6 Window-Grab deadlock avoidance (:1251-1253): when armed,
     * succeed WITHOUT taking the lock bit. */
    bool skipped = false;
    if (m_skip_write_lock_once) {
        m_skip_write_lock_once = false;
        skipped = true;
        IOLog("VMAccelSurfaceClient: WriteLock — bSkipWriteLockOnce "
              "firing (lock-bit not taken)\n");
    } else if (m_surface->is_locked) {
        IOLog("VMAccelSurfaceClient: WriteLock -> CannotLock (already "
              "held)\n");
        IOLockUnlock(m_lock);
        return kIOReturnCannotLock;
    } else {
        m_surface->is_locked = true;
    }

    /* Lazy, grow-only backing: stride is the ALLOCATION's stride
     * (base_w * bpp, page-rounded total). */
    uint32_t bpp = m_surface->bytes_per_pixel;
    uint32_t rowBytes = m_surface->base_w * bpp;
    uint64_t need = (uint64_t)m_surface->base_h * rowBytes;
    uint64_t allocSize = (need + page_size - 1) & ~((uint64_t)page_size - 1);

    if (!m_surface->backing_memory ||
        m_surface->backing_memory->getLength() < allocSize) {
        /* release any smaller existing backing first (grow path,
         * mirrors :545-548 release-before-realloc) */
        if (m_surface->kernel_map) {
            m_surface->kernel_map->release();
            m_surface->kernel_map = nullptr;
        }
        if (m_surface->client_map) {
            m_surface->client_map->release();
            m_surface->client_map = nullptr;
        }
        if (m_surface->backing_memory) {
            m_surface->backing_memory->complete();
            m_surface->backing_memory->release();
            m_surface->backing_memory = nullptr;
        }
        IOBufferMemoryDescriptor* md =
            IOBufferMemoryDescriptor::inTaskWithOptions(
                0 /* kernel task */,             /* idiom :1111-1116 */
                kIOMemoryKernelUserShared | kIOMemoryPageable |
                kIODirectionInOut,
                allocSize, page_size);
        if (!md) {
            IOLog("VMAccelSurfaceClient: WriteLock -> NoMemory "
                  "(alloc %llu failed)\n", (unsigned long long)allocSize);
            m_surface->is_locked = false;
            IOLockUnlock(m_lock);
            return kIOReturnNoMemory;
        }
        if (md->prepare() != kIOReturnSuccess) {
            md->release();
            IOLog("VMAccelSurfaceClient: WriteLock -> NoMemory "
                  "(prepare failed)\n");
            m_surface->is_locked = false;
            IOLockUnlock(m_lock);
            return kIOReturnNoMemory;
        }
        IOMemoryMap* map = md->createMappingInTask(m_owning_task, 0,
                                                   kIOMapAnywhere);
        if (!map) {
            md->complete();
            md->release();
            IOLog("VMAccelSurfaceClient: WriteLock -> NoMemory "
                  "(client createMappingInTask failed)\n");
            m_surface->is_locked = false;
            IOLockUnlock(m_lock);
            return kIOReturnNoMemory;
        }
        /* Kernel mapping for the flush memcpy — KernelUserShared
         * pageable memory has no kernel VA without it (3258aaec
         * boot: getBytesNoCopy()==0, 42/42 flush NotReady). */
        IOMemoryMap* kmap = md->createMappingInTask(kernel_task, 0,
                                                    kIOMapAnywhere);
        if (!kmap) {
            map->release();
            md->complete();
            md->release();
            IOLog("VMAccelSurfaceClient: WriteLock -> NoMemory "
                  "(kernel createMappingInTask failed)\n");
            m_surface->is_locked = false;
            IOLockUnlock(m_lock);
            return kIOReturnNoMemory;
        }
        m_surface->backing_memory = md;
        m_surface->client_map = map;
        m_surface->kernel_map = kmap;
        m_surface->bytes_per_row = rowBytes;
        IOLog("VMAccelSurfaceClient: WriteLock — backing ALLOCATED "
              "%llu bytes (extent %ux%u stride %u), client 0x%llx "
              "kernel 0x%llx\n",
              (unsigned long long)allocSize,
              m_surface->base_w, m_surface->base_h, rowBytes,
              (unsigned long long)map->getAddress(),
              (unsigned long long)kmap->getAddress());
    }

    /* Handout: client-task address + shape offset, allocation
     * stride, current shape dims. */
    uint64_t offset = (uint64_t)m_surface->shape_y * m_surface->bytes_per_row
                    + (uint64_t)m_surface->shape_x * bpp;
    mach_vm_address_t base = m_surface->client_map->getAddress();

    /* Self-check (formula FIXED 2026-08-16 — the original
     * `offset + h*stride` was over-strict by a partial row and
     * false-positived 5x on the lock boot): last byte the caller
     * can touch = (y+h-1)*stride + (x+w)*bpp. */
    if ((uint64_t)(m_surface->shape_y + m_surface->height - 1) *
            m_surface->bytes_per_row +
        (uint64_t)(m_surface->shape_x + m_surface->width) * bpp >
        m_surface->client_map->getLength()) {
        IOLog("VMAccelSurfaceClient: MISMATCH — handout window exceeds "
              "mapping: y=%u h=%u x=%u w=%u stride=%u len=%llu\n",
              m_surface->shape_y, m_surface->height,
              m_surface->shape_x, m_surface->width,
              m_surface->bytes_per_row,
              (unsigned long long)m_surface->client_map->getLength());
    }

    info->address[0] = base + offset;
    info->width = m_surface->width;
    info->height = m_surface->height;
    info->rowBytes = m_surface->bytes_per_row;
    info->pixelFormat = m_surface->pixel_format;
    info->colorTemperature[0] = 0x1CCCC;    /* worked example :395 */

    IOLog("VMAccelSurfaceClient: WriteLock -> Success (base=0x%llx "
          "off=%llu addr=0x%llx %ux%u stride=%u%s)\n",
          (unsigned long long)base, (unsigned long long)offset,
          (unsigned long long)info->address[0],
          info->width, info->height, info->rowBytes,
          skipped ? " [skipped-lock]" : "");
    IOLockUnlock(m_lock);
    return kIOReturnSuccess;
}

IOReturn CLASS::writeUnlockSurface(void *p1, void *p2, void *p3, void *p4, void *p5, void *p6)
{
    /* Worked example :1276-1281: clear the bit, Success, nothing
     * unmaps — the backing persists by design. */
    IOLockLock(m_lock);
    if (m_surface)
        m_surface->is_locked = false;
    IOLockUnlock(m_lock);
    IOLog("VMAccelSurfaceClient: WriteUnlock -> Success (bit cleared, "
          "backing persists)\n");
    return kIOReturnSuccess;
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



// VMQemuVGAGA.cpp — the GA CFPlugIn (milestone 1)
//
// The app-side accelerator attach (docs/ga-cfplugin.md): CGS loads
// this bundle in the application via the framebuffer's IOCFPlugInTypes
// property and drives IOGraphicsAcceleratorInterface. vmStart opens
// the kernel's TYPE 2 "2D Context" user client (VMQemuVGA3DUserClient)
// and exchanges the config words; every surface-wiring slot is
// Unsupported until milestone 2.
//
// Contract sources: the 10.6 SDK IOGraphicsInterface.h (the interface
// struct, verbatim below via the header) and the worked example
// ../VMsvga2-modern/GA/VMsvga2GA.cpp (the reserved-slot usage and the
// vmStart sequence shape — read for the contract; this implementation
// is fresh and minimal).

#include <CoreFoundation/CoreFoundation.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// IOKit's C declarations need extern "C" in C++ translation units.
extern "C" {
#include <IOKit/IOKitLib.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/graphics/IOGraphicsInterface.h>
}

// 2D-context selector numbers (FB/VMQemuVGAAccelerator.h; numbering
// verbatim from the worked example's eIOVM2DMethods).
enum {
    kVM2DSetSurface = 0,
    kVM2DGetConfig = 1,
    kVM2DLockMemory = 5,
    kVM2DUnlockMemory = 6,
    kVM2DFinish = 7,
    kVM2DReadConfigs = 16,
    kVM2DUseAccelUpdates = 21,
};

#define kVMGAFactoryID \
    (CFUUIDGetConstantUUIDWithBytes(NULL, 0x66, 0xF3, 0xA1, 0xD2, \
                                          0x8B, 0x4E, 0x47, 0x9A, \
                                          0x9D, 0x15, 0x2E, 0x9F, \
                                          0x0A, 0x7B, 0x3C, 0x41))

#define GALog(...) do { fprintf(stderr, "VMQemuVGAGA: " __VA_ARGS__); } while (0)

typedef struct _SurfaceInfo {
    void* d0;                  /* reserved for an image handle (unused) */
    vm_address_t addr;         /* cached LockMemory view */
    uintptr_t cgsSurfaceID;    /* the CGS surface id from AllocateSurface */
} SurfaceInfo;

typedef struct _GAType {
    IOGraphicsAcceleratorInterface* _interface;
    CFUUIDRef _factoryID;
    ULONG _refCount;
    io_connect_t _context;          // the type-2 2D context
    io_service_t _accelerator;
    UInt32 _framebufferIndex;
    UInt32 _config_val_1;
    UInt32 _config_val_2;
} GAType;

static IOGraphicsAcceleratorInterface ga;
static int ga_initialized = 0;

// ---- IUnknown ----------------------------------------------------------

static HRESULT vmQueryInterface(void* myInstance, REFIID iid, LPVOID* ppv)
{
    GAType* me = (GAType*)myInstance;
    CFUUIDRef interfaceID = CFUUIDCreateFromUUIDBytes(NULL, iid);
    if (CFEqual(interfaceID, kIOGraphicsAcceleratorInterfaceID) ||
        CFEqual(interfaceID, kIOCFPlugInInterfaceID) ||
        CFEqual(interfaceID, IUnknownUUID)) {
        if (me && me->_interface)
            me->_interface->AddRef(myInstance);
        if (ppv)
            *ppv = myInstance;
        CFRelease(interfaceID);
        return S_OK;
    }
    if (ppv)
        *ppv = NULL;
    CFRelease(interfaceID);
    return E_NOINTERFACE;
}

static ULONG vmAddRef(void* myInstance)
{
    GAType* me = (GAType*)myInstance;
    if (!me)
        return 0;
    return ++me->_refCount;
}

static ULONG vmRelease(void* myInstance)
{
    GAType* me = (GAType*)myInstance;
    if (!me)
        return 0;
    GALog("Release: refcount %lu -> %lu\n",
          (unsigned long)me->_refCount, (unsigned long)(me->_refCount - 1));
    if (--me->_refCount == 0) {
        CFUUIDRef factoryID = me->_factoryID;
        free(me);
        if (factoryID) {
            /* NOTE: RemoveInstanceForFactory from inside the plugin's own
             * Release can drop the last bundle reference and unload the
             * plugin WHILE this frame is executing. Suspect #1 for the
             * probe's teardown segfault — the marks name it if so. */
            GALog("Release: final — RemoveInstanceForFactory\n");
            CFPlugInRemoveInstanceForFactory(factoryID);
            CFRelease(factoryID);
        }
        return 0;
    }
    return me->_refCount;
}

// ---- IOCFPlugIn lifecycle ---------------------------------------------

static IOReturn vmProbe(void* myInstance, CFDictionaryRef propertyTable,
                        io_service_t service, SInt32* order)
{
    (void)myInstance; (void)propertyTable; (void)service;
    GALog("Probe\n");
    if (order)
        *order = 2000;
    return kIOReturnSuccess;
}

static IOReturn vmStart(void* myInstance, CFDictionaryRef propertyTable,
                        io_service_t service)
{
    GAType* me = (GAType*)myInstance;
    io_service_t accelerator = 0;
    io_connect_t context = 0;
    uint32_t out_cnt = 2;
    uint64_t out[2] = {0, 0};
    UInt32 in_struct = 2;
    UInt32 out_struct = 0;
    uint64_t enable = 1;
    size_t out_struct_cnt = sizeof(out_struct);
    IOReturn rc;

    (void)propertyTable;
    GALog("Start(service=%u)\n", (unsigned)service);
    if (!me || !service)
        return kIOReturnBadArgument;

    /* MILESTONE-1 GATE (2026-08-20, the WindowServer-loop incident):
     * with the plugin discoverable, WindowServer loaded it, Start
     * SUCCEEDED, and its first surface slot hit a stub — the aborted
     * attach put WindowServer in an open/close retry loop and broke
     * the display. Until the surface slots exist (milestone 2), Start
     * REFUSES unless this process opted in explicitly — WindowServer's
     * fast failure falls back to the software path (same shape as the
     * pre-plugin boots), and the probe sets the opt-in itself. */
    if (!getenv("VM_GA_PROBE")) {
        GALog("Start: refusing — VM_GA_PROBE not set (milestone-1 gate)\n");
        return kIOReturnUnsupported;
    }

    rc = IOAccelFindAccelerator(service, &accelerator, &me->_framebufferIndex);
    if (rc != kIOReturnSuccess) {
        GALog("Start: IOAccelFindAccelerator FAIL 0x%x\n", rc);
        return rc;
    }
    GALog("Start: accelerator=%u fbIndex=%u\n",
          (unsigned)accelerator, me->_framebufferIndex);

    rc = IOServiceOpen(accelerator, mach_task_self(), 2 /* 2D Context */,
                       &context);
    if (rc != kIOReturnSuccess) {
        GALog("Start: IOServiceOpen type 2 FAIL 0x%x\n", rc);
        IOObjectRelease(accelerator);
        return rc;
    }

    rc = IOConnectCallMethod(context, kVM2DGetConfig,
                             NULL, 0, NULL, 0,
                             &out[0], &out_cnt,
                             NULL, NULL);
    if (rc != kIOReturnSuccess)
        goto cleanup;
    me->_config_val_1 = (UInt32)out[0];

    rc = IOConnectCallMethod(context, kVM2DReadConfigs,
                             NULL, 0,
                             &in_struct, sizeof(in_struct),
                             NULL, 0,
                             &out_struct, &out_struct_cnt);
    if (rc != kIOReturnSuccess)
        goto cleanup;
    me->_config_val_2 = out_struct;

    rc = IOConnectCallMethod(context, kVM2DUseAccelUpdates,
                             &enable, 1,
                             NULL, 0, NULL, 0, NULL, NULL);
    if (rc != kIOReturnSuccess)
        goto cleanup;

    me->_accelerator = accelerator;
    me->_context = context;
    GALog("Start: OK — context=%u config={%u,%u}\n",
          (unsigned)context, me->_config_val_1, me->_config_val_2);
    return kIOReturnSuccess;

cleanup:
    GALog("Start: FAIL at 0x%x — closing\n", rc);
    IOServiceClose(context);
    IOObjectRelease(accelerator);
    return rc;
}

static IOReturn vmStop(void* myInstance)
{
    GAType* me = (GAType*)myInstance;
    GALog("Stop: enter\n");
    if (!me)
        return kIOReturnBadArgument;
    if (me->_context) {
        uint64_t zero = 0;
        IOConnectCallMethod(me->_context, kVM2DFinish,
                            &zero, 1,
                            NULL, 0, NULL, 0, NULL, NULL);
        IOServiceClose(me->_context);
        me->_context = 0;
        GALog("Stop: context closed\n");
    }
    if (me->_accelerator) {
        IOObjectRelease(me->_accelerator);
        me->_accelerator = 0;
        GALog("Stop: accelerator released\n");
    }
    GALog("Stop: exit\n");
    return kIOReturnSuccess;
}

// ---- Milestone-2 slots: refused loudly --------------------------------

static int s_unsupported_log = 0;

#define VM_UNSUPPORTED(name) \
    static IOReturn name(void* myInstance) \
    { \
        (void)myInstance; \
        if (s_unsupported_log < 12) { \
            s_unsupported_log++; \
            GALog("%s UNSUPPORTED (milestone 2)\n", #name); \
        } \
        return kIOReturnUnsupported; \
    }

static IOReturn vmReset(void* myInstance, IOOptionBits options)
{
    (void)myInstance; (void)options;
    return kIOReturnSuccess;   // nothing queued; mirrors the worked example's no-op
}

VM_UNSUPPORTED(vmCapabilities)
VM_UNSUPPORTED(vmFlush)
VM_UNSUPPORTED(vmSynchronize)
VM_UNSUPPORTED(vmGetBeamPosition)
VM_UNSUPPORTED(vmSwapSurface)
VM_UNSUPPORTED(vmGetBlitter)
VM_UNSUPPORTED(vmWaitComplete)
VM_UNSUPPORTED(vmWaitSurface)

// The VM_UNSUPPORTED thunk signature is (void*) — the interface's real
// signatures differ per slot; cast at table build (the COM table is
// void*-typed anyway, same technique as the worked example's
// reinterpret_cast on the reserved slots).

// ---- SetSurface / SetDestination (milestone 2 rung 1) -------------------

static IOReturn vmSetSurface(void* myInstance, IOOptionBits options,
                             IOBlitSurface* surface)
{
    GAType* me = (GAType*)myInstance;
    IOReturn rc;
    uint64_t input[2];
    uint32_t out_buf[11];
    size_t out_cnt = sizeof(out_buf);

    if (!me)
        return kIOReturnBadArgument;
    if (!me->_context)
        return kIOReturnNotReady;

    if (options & 0x800U) {
        /* CGS-surface destination: the id lives in the surface's
         * SurfaceInfo (set at AllocateSurface). */
        if (!surface || !surface->interfaceRef)
            return kIOReturnBadArgument;
        SurfaceInfo* si = (SurfaceInfo*)surface->interfaceRef;
        input[0] = (uint64_t)si->cgsSurfaceID;
        input[1] = options;
    } else {
        input[0] = me->_framebufferIndex;
        input[1] = 0;
    }
    rc = IOConnectCallMethod(me->_context, kVM2DSetSurface,
                             &input[0], 2,
                             NULL, 0,
                             NULL, 0,
                             &out_buf[0], &out_cnt);
    GALog("SetSurface: {fbIdx|id}=%llu opts=%#x -> 0x%x\n",
          (unsigned long long)input[0], (unsigned)input[1], rc);
    return rc;
}

static IOReturn vmSetDestination(void* myInstance, IOOptionBits options,
                                 IOBlitSurface* surface)
{
    GALog("SetDestination(opts=%#x surface=%p)\n",
          (unsigned)options, surface);
    if (options & kIOBlitSurfaceDestination)
        options = 0x800U;
    else
        options = kIOBlitFramebufferDestination;
    return vmSetSurface(myInstance, options, surface);
}

// ---- Surface slots (milestone 2 rung 2) ----------------------------------

static IOReturn vmAllocateSurface(void* myInstance, IOOptionBits options,
                                  IOBlitSurface* surface, void* cgsSurfaceID)
{
    GAType* me = (GAType*)myInstance;
    GALog("AllocateSurface(opts=%#x surface=%p cgsID=%p)\n",
          (unsigned)options, (void*)surface, cgsSurfaceID);
    if (!me || !surface)
        return kIOReturnBadArgument;
    if (!me->_context)
        return kIOReturnNotReady;
    if (!(options & kIOBlitHasCGSSurface)) {
        /* Non-CGS allocation paths are not part of this design — the
         * GA path exists to bind CGS surfaces, not to invent new ones. */
        GALog("AllocateSurface: non-CGS path UNSUPPORTED\n");
        return kIOReturnUnsupported;
    }
    SurfaceInfo* si = (SurfaceInfo*)calloc(1, sizeof(SurfaceInfo));
    if (!si)
        return kIOReturnNoMemory;
    si->cgsSurfaceID = (uintptr_t)cgsSurfaceID;
    surface->interfaceRef = (IOBlitMemoryRef)si;
    /* The binding itself: worked example routes through SetSurface
     * with 0x800 (format bits ORed per pixelFormat). */
    IOOptionBits bind_opts = 0x800U;
    switch (surface->pixelFormat) {
        case kIO32BGRAPixelFormat: bind_opts |= 0x100U; break;
        default: break;
    }
    IOReturn rc = vmSetSurface(myInstance, bind_opts | options, surface);
    if (rc != kIOReturnSuccess) {
        GALog("AllocateSurface: bind FAILED 0x%x — freeing si\n", rc);
        free(si);
        surface->interfaceRef = 0;
        return rc;
    }
    GALog("AllocateSurface: bound cgsID=%llu\n",
          (unsigned long long)si->cgsSurfaceID);
    return kIOReturnSuccess;
}

static IOReturn vmUnlockSurface(void* myInstance, IOOptionBits options,
                                IOBlitSurface* surface, IOOptionBits* swapFlags);

static IOReturn vmFreeSurface(void* myInstance, IOOptionBits options,
                              IOBlitSurface* surface)
{
    GAType* me = (GAType*)myInstance;
    GALog("FreeSurface(opts=%#x surface=%p)\n", (unsigned)options,
          (void*)surface);
    if (!me || !surface)
        return kIOReturnBadArgument;
    SurfaceInfo* si = (SurfaceInfo*)surface->interfaceRef;
    if (!si)
        return kIOReturnNotReady;
    if (si->addr) {
        /* Drop any held lock view before freeing the record. */
        vmUnlockSurface(myInstance, options, surface, NULL);
    }
    free(si);
    surface->interfaceRef = 0;
    /* Rebind the context to the framebuffer destination (worked
     * example's vmFreeSurface resets destination). */
    return vmSetSurface(myInstance, kIOBlitFramebufferDestination, NULL);
}

static IOReturn vmLockSurface(void* myInstance, IOOptionBits options,
                              IOBlitSurface* surface, vm_address_t* address)
{
    GAType* me = (GAType*)myInstance;
    if (!me || !surface || !address)
        return kIOReturnBadArgument;
    if (!me->_context)
        return kIOReturnNotReady;
    SurfaceInfo* si = (SurfaceInfo*)surface->interfaceRef;
    if (!si)
        return kIOReturnNotReady;
    if (si->addr) {
        *address = si->addr;
        return kIOReturnSuccess;
    }
    uint64_t input = options;
    uint64_t struct_out[2];
    size_t struct_out_cnt = sizeof(struct_out);
    IOReturn rc = IOConnectCallMethod(me->_context, kVM2DLockMemory,
                                      &input, 1,
                                      NULL, 0,
                                      NULL, 0,
                                      &struct_out[0], &struct_out_cnt);
    if (rc != kIOReturnSuccess) {
        GALog("LockSurface: kernel 0x%x\n", rc);
        return rc;
    }
    *address = (vm_address_t)struct_out[0];
    surface->rowBytes = (UInt32)struct_out[1];
    /* accessFlags=2 — the QuickTime lesson, worked example :606. */
    surface->accessFlags = 2;
    si->addr = *address;
    GALog("LockSurface: addr=%llx row=%u\n",
          (unsigned long long)struct_out[0], surface->rowBytes);
    return kIOReturnSuccess;
}

static IOReturn vmUnlockSurface(void* myInstance, IOOptionBits options,
                                IOBlitSurface* surface, IOOptionBits* swapFlags)
{
    GAType* me = (GAType*)myInstance;
    if (!me || !surface)
        return kIOReturnBadArgument;
    if (!me->_context)
        return kIOReturnNotReady;
    SurfaceInfo* si = (SurfaceInfo*)surface->interfaceRef;
    if (!si)
        return kIOReturnNotReady;
    if (!si->addr)
        return kIOReturnSuccess;
    uint64_t input = options;
    uint64_t output = 0;
    uint32_t output_cnt = 1;
    IOReturn rc = IOConnectCallMethod(me->_context, kVM2DUnlockMemory,
                                      &input, 1,
                                      NULL, 0,
                                      &output, &output_cnt,
                                      NULL, NULL);
    if (swapFlags)
        *swapFlags = (IOOptionBits)output;
    if (rc == kIOReturnSuccess)
        si->addr = 0;
    GALog("UnlockSurface -> 0x%x\n", rc);
    return rc;
}

// ---- Factory ------------------------------------------------------------

static void _buildGAFTbl()
{
    if (ga_initialized)
        return;
    memset(&ga, 0, sizeof ga);
    ga.QueryInterface = vmQueryInterface;
    ga.AddRef = vmAddRef;
    ga.Release = vmRelease;
    ga.version = kCurrentGraphicsInterfaceVersion;
    ga.revision = kCurrentGraphicsInterfaceRevision;
    ga.Probe = vmProbe;
    ga.Start = vmStart;
    ga.Stop = vmStop;
    ga.Reset = vmReset;
    ga.CopyCapabilities = (IOReturn (*)(void*, FourCharCode, CFTypeRef*))vmCapabilities;
    ga.Flush = (IOReturn (*)(void*, IOOptionBits))vmFlush;
    ga.Synchronize = (IOReturn (*)(void*, UInt32, UInt32, UInt32, UInt32, UInt32))vmSynchronize;
    ga.GetBeamPosition = (IOReturn (*)(void*, IOOptionBits, SInt32*))vmGetBeamPosition;
    ga.AllocateSurface = vmAllocateSurface;
    ga.FreeSurface = vmFreeSurface;
    ga.LockSurface = vmLockSurface;
    ga.UnlockSurface = vmUnlockSurface;
    ga.SwapSurface = (IOReturn (*)(void*, IOOptionBits, IOBlitSurface*, IOOptionBits*))vmSwapSurface;
    ga.SetDestination = vmSetDestination;
    ga.__gaInterfaceReserved[1] = (void*)vmSetSurface;
    ga.GetBlitter = (IOReturn (*)(void*, IOOptionBits, IOBlitType, IOBlitSourceType, IOBlitterPtr*))vmGetBlitter;
    ga.WaitComplete = (IOReturn (*)(void*, IOOptionBits))vmWaitComplete;
    ga.__gaInterfaceReserved[0] = (void*)vmWaitSurface;
    ga.__gaInterfaceReserved[1] = (void*)vmSetSurface;
    ga_initialized = 1;
}

extern "C" __attribute__((visibility("default")))
void* VMQemuVGAGAFactory(CFAllocatorRef allocator, CFUUIDRef typeID)
{
    (void)allocator;
    if (!CFEqual(typeID, kIOGraphicsAcceleratorTypeID)) {
        GALog("Factory: refusing typeID mismatch\n");
        return NULL;
    }
    /* FACTORY-LEVEL GATE (2026-08-20, the second loop incident): a
     * Start-level refusal still broke WindowServer — instantiation
     * SUCCEEDED, then Start failed, and that late failure was enough
     * for an open/close retry loop. The gate must fail at the SAME
     * point the pre-plugin boots failed: the FACTORY. Without the
     * opt-in env (set only by the probe, in its own process — the
     * factory runs in the caller), instantiation fails exactly like
     * plugin-not-found, which booted fine. */
    if (!getenv("VM_GA_PROBE")) {
        return NULL;
    }
    GALog("Factory: VM_GA_PROBE set — proceeding\n");
    GAType* me = (GAType*)malloc(sizeof(GAType));
    if (!me)
        return NULL;
    memset(me, 0, sizeof(GAType));
    _buildGAFTbl();
    me->_interface = &ga;
    me->_factoryID = (CFUUIDRef)CFRetain(kVMGAFactoryID);
    CFPlugInAddInstanceForFactory(kVMGAFactoryID);
    me->_refCount = 1;
    GALog("Factory: instance %p\n", (void*)me);
    return me;
}

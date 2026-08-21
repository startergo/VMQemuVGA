// VMQemuVGA3DUserClient — TYPE 2 user client: the GA CFPlugIn's 2D Context
//
// Selector set: the kIOVM2D* family (docs/ga-cfplugin.md; numbering
// verbatim from the worked example's AC/UC/UCMethods.h eIOVM2DMethods).
// The GA plugin (GA/VMQemuVGAGA) opens type 2 at vmStart and calls
// GetConfig/ReadConfigs (+Finish/UseAccelUpdates); the surface-wiring
// selectors (SetSurface/LockMemory/…) land in milestone 2 — until then
// they answer kIOReturnUnsupported, loudly.
//
// History: this class previously served a private 3D-context selector
// set backed by the accelerator's software-mock pools. Runtime census
// before replacement (kernel-log newUserClient lines, 2026-08-20):
// ZERO type-2 opens in any recorded boot — no caller existed.
#include "VMQemuVGAAccelerator.h"
#include "VMAccelSurfaceClient.h"   /* the surface registry (milestone 2) */
#include <IOKit/IOLib.h>
#include <IOKit/IOBufferMemoryDescriptor.h>

#define CLASS VMQemuVGA3DUserClient
#define super IOUserClient

OSDefineMetaClassAndStructors(VMQemuVGA3DUserClient, IOUserClient);

/* Method dispatch table. Check counts mirror the worked example's
 * IOConnectCallMethod shapes so a mismatched caller fails in the
 * framework's check rather than mid-handler. Structure size 0 in a
 * check field means "do not check" (variable-length region/rect
 * payloads). */
static const IOExternalMethodDispatch sMethods[kVM2DNumMethods] = {
    [kVM2DSetSurface] = {          // milestone 2: the destination binding
        .function = (IOExternalMethodAction) &CLASS::sSetSurface,
        .checkScalarInputCount = 2,
        .checkStructureInputSize = 0,
        .checkScalarOutputCount = 0,
        .checkStructureOutputSize = 11 * sizeof(uint32_t),
    },
    [kVM2DGetConfig] = {
        .function = (IOExternalMethodAction) &CLASS::sGetConfig,
        .checkScalarInputCount = 0,
        .checkStructureInputSize = 0,
        .checkScalarOutputCount = 2,
        .checkStructureOutputSize = 0,
    },
    [kVM2DGetSurfaceInfo1] = {
        .function = (IOExternalMethodAction) &CLASS::sStub,
        .checkScalarInputCount = 0,
        .checkStructureInputSize = 0,
        .checkScalarOutputCount = 0,
        .checkStructureOutputSize = 0,
    },
    [kVM2DSwapSurface] = {
        .function = (IOExternalMethodAction) &CLASS::sStub,
        .checkScalarInputCount = 1,
        .checkStructureInputSize = 0,
        .checkScalarOutputCount = 1,
        .checkStructureOutputSize = 0,
    },
    [kVM2DScaleSurface] = {
        .function = (IOExternalMethodAction) &CLASS::sStub,
        .checkScalarInputCount = 3,
        .checkStructureInputSize = 0,
        .checkScalarOutputCount = 0,
        .checkStructureOutputSize = 0,
    },
    [kVM2DLockMemory] = {          // milestone 2: the app-task surface view
        .function = (IOExternalMethodAction) &CLASS::sLockMemory,
        .checkScalarInputCount = 1,
        .checkStructureInputSize = 0,
        .checkScalarOutputCount = 0,
        .checkStructureOutputSize = 2 * sizeof(uint64_t),
    },
    [kVM2DUnlockMemory] = {
        .function = (IOExternalMethodAction) &CLASS::sUnlockMemory,
        .checkScalarInputCount = 1,
        .checkStructureInputSize = 0,
        .checkScalarOutputCount = 1,
        .checkStructureOutputSize = 0,
    },
    [kVM2DFinish] = {
        .function = (IOExternalMethodAction) &CLASS::sFinish,
        .checkScalarInputCount = 1,
        .checkStructureInputSize = 0,
        .checkScalarOutputCount = 0,
        .checkStructureOutputSize = 0,
    },
    [kVM2DDeclareImage] = {
        .function = (IOExternalMethodAction) &CLASS::sStub,
        .checkScalarInputCount = 0,
        .checkStructureInputSize = 0,
        .checkScalarOutputCount = 0,
        .checkStructureOutputSize = 0,
    },
    [kVM2DCreateImage] = {
        .function = (IOExternalMethodAction) &CLASS::sStub,
        .checkScalarInputCount = 0,
        .checkStructureInputSize = 0,
        .checkScalarOutputCount = 0,
        .checkStructureOutputSize = 0,
    },
    [kVM2DCreateTransfer] = {
        .function = (IOExternalMethodAction) &CLASS::sStub,
        .checkScalarInputCount = 0,
        .checkStructureInputSize = 0,
        .checkScalarOutputCount = 0,
        .checkStructureOutputSize = 0,
    },
    [kVM2DDeleteImage] = {
        .function = (IOExternalMethodAction) &CLASS::sStub,
        .checkScalarInputCount = 1,
        .checkStructureInputSize = 0,
        .checkScalarOutputCount = 0,
        .checkStructureOutputSize = 0,
    },
    [kVM2DWaitImage] = {
        .function = (IOExternalMethodAction) &CLASS::sStub,
        .checkScalarInputCount = 1,
        .checkStructureInputSize = 0,
        .checkScalarOutputCount = 0,
        .checkStructureOutputSize = 0,
    },
    [kVM2DSetSurfacePagingOptions] = {
        .function = (IOExternalMethodAction) &CLASS::sStub,
        .checkScalarInputCount = 0,
        .checkStructureInputSize = 0,
        .checkScalarOutputCount = 0,
        .checkStructureOutputSize = 0,
    },
    [kVM2DSetSurfaceVsyncOptions] = {
        .function = (IOExternalMethodAction) &CLASS::sStub,
        .checkScalarInputCount = 0,
        .checkStructureInputSize = 0,
        .checkScalarOutputCount = 0,
        .checkStructureOutputSize = 0,
    },
    [kVM2DSetMacrovision] = {
        .function = (IOExternalMethodAction) &CLASS::sStub,
        .checkScalarInputCount = 0,
        .checkStructureInputSize = 0,
        .checkScalarOutputCount = 0,
        .checkStructureOutputSize = 0,
    },
    [kVM2DReadConfigs] = {
        .function = (IOExternalMethodAction) &CLASS::sReadConfigs,
        .checkScalarInputCount = 0,
        .checkStructureInputSize = sizeof(uint32_t),
        .checkScalarOutputCount = 0,
        .checkStructureOutputSize = sizeof(uint32_t),
    },
    [kVM2DReadConfigEx] = {
        .function = (IOExternalMethodAction) &CLASS::sReadConfigEx,
        .checkScalarInputCount = 0,
        .checkStructureInputSize = sizeof(uint32_t),
        .checkScalarOutputCount = 0,
        .checkStructureOutputSize = 3 * sizeof(uint32_t),
    },
    [kVM2DGetSurfaceInfo2] = {
        .function = (IOExternalMethodAction) &CLASS::sStub,
        .checkScalarInputCount = 0,
        .checkStructureInputSize = 0,
        .checkScalarOutputCount = 0,
        .checkStructureOutputSize = 0,
    },
    [kVM2DKernelPrintf] = {
        .function = (IOExternalMethodAction) &CLASS::sStub,
        .checkScalarInputCount = 0,
        .checkStructureInputSize = 0,     // variable string in
        .checkScalarOutputCount = 0,
        .checkStructureOutputSize = 0,
    },
    [kVM2DCopyRegion] = {
        .function = (IOExternalMethodAction) &CLASS::sStub,
        .checkScalarInputCount = 1,
        .checkStructureInputSize = 0,     // variable IOAccelDeviceRegion in
        .checkScalarOutputCount = 0,
        .checkStructureOutputSize = 0,
    },
    [kVM2DUseAccelUpdates] = {
        .function = (IOExternalMethodAction) &CLASS::sUseAccelUpdates,
        .checkScalarInputCount = 1,
        .checkStructureInputSize = 0,
        .checkScalarOutputCount = 0,
        .checkStructureOutputSize = 0,
    },
    [kVM2DRectCopy] = {
        .function = (IOExternalMethodAction) &CLASS::sStub,
        .checkScalarInputCount = 1,
        .checkStructureInputSize = 0,     // variable rects in
        .checkScalarOutputCount = 0,
        .checkStructureOutputSize = 0,
    },
    [kVM2DRectFill] = {
        .function = (IOExternalMethodAction) &CLASS::sStub,
        .checkScalarInputCount = 1,
        .checkStructureInputSize = 0,     // variable rects in
        .checkScalarOutputCount = 0,
        .checkStructureOutputSize = 0,
    },
    [kVM2DUpdateFramebuffer] = {
        .function = (IOExternalMethodAction) &CLASS::sStub,
        .checkScalarInputCount = 0,
        .checkStructureInputSize = 0,
        .checkScalarOutputCount = 0,
        .checkStructureOutputSize = 0,
    },
};

VMQemuVGA3DUserClient* VMQemuVGA3DUserClient::withTask(task_t owningTask)
{
    VMQemuVGA3DUserClient* client = new VMQemuVGA3DUserClient;
    if (client) {
        if (!client->initWithTask(owningTask, nullptr, 2, nullptr)) {
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
    m_bound_id = 0xFFFFFFFFFFFFFFFFull;
    m_bound_options = 0;
    m_app_map = nullptr;

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

    IOLog("VMQemuVGA3DUserClient: 2D context started for task %p\n", m_task);
    return true;
}

IOReturn CLASS::clientClose()
{
    IOLog("VMQemuVGA3DUserClient: clientClose\n");

    if (m_app_map) {
        m_app_map->release();
        m_app_map = nullptr;
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
    if (selector >= kVM2DNumMethods)
        return kIOReturnBadArgument;

    dispatch = (IOExternalMethodDispatch*) &sMethods[selector];
    if (!dispatch)
        return kIOReturnBadArgument;

    target = this;
    reference = nullptr;

    return super::externalMethod(selector, args, dispatch, target, reference);
}

/* GetConfig: two config words. Placeholder values for milestone 1 —
 * the plugin reads output[0] as capability bits and output[1] as its
 * log level. 0/0 = no capability claims, quiet logging. Milestone 2
 * revisits if CGS consults the bits. */
IOReturn CLASS::sGetConfig(OSObject* target, void* reference,
                           IOExternalMethodArguments* args)
{
    args->scalarOutput[0] = 0;
    args->scalarOutput[1] = 0;
    IOLog("VMQemuVGA3DUserClient: GetConfig -> {0, 0}\n");
    return kIOReturnSuccess;
}

/* ReadConfigs: in = index (the plugin sends 2), out = a version-ish
 * word the plugin compares (<=79 caps surfaces at 2046x2047). 100
 * allows window-sized surfaces. Documented placeholder. */
IOReturn CLASS::sReadConfigs(OSObject* target, void* reference,
                             IOExternalMethodArguments* args)
{
    uint32_t index = *(const uint32_t*)args->structureInput;
    uint32_t version = 100;
    *(uint32_t*)args->structureOutput = version;
    IOLog("VMQemuVGA3DUserClient: ReadConfigs(index=%u) -> %u\n",
          index, version);
    return kIOReturnSuccess;
}

/* ReadConfigEx: in = a selector word (the plugin sends 288 after
 * SetSurface), out = 3 words. {64, 0, 16} matches the worked
 * example's in-plugin fallback when the kernel call fails. */
IOReturn CLASS::sReadConfigEx(OSObject* target, void* reference,
                              IOExternalMethodArguments* args)
{
    uint32_t which = *(const uint32_t*)args->structureInput;
    uint32_t* out = (uint32_t*)args->structureOutput;
    out[0] = 64;
    out[1] = 0;
    out[2] = 16;
    IOLog("VMQemuVGA3DUserClient: ReadConfigEx(which=%u) -> {64, 0, 16}\n",
          which);
    return kIOReturnSuccess;
}

IOReturn CLASS::sFinish(OSObject* target, void* reference,
                        IOExternalMethodArguments* args)
{
    // No queued work in milestone 1 — accept and report.
    return kIOReturnSuccess;
}

IOReturn CLASS::sUseAccelUpdates(OSObject* target, void* reference,
                                 IOExternalMethodArguments* args)
{
    // The plugin enables "accel updates" at vmStart. Nothing to push
    // yet; acknowledge.
    return kIOReturnSuccess;
}

/* SetSurface — the destination binding (milestone 2 rung 1).
 * scalar[0] = cgsSurfaceID (options & 0x800) or framebufferIndex;
 * scalar[1] = options. Out: 11-word struct (the worked example's
 * plugin stores but never parses it — zeros returned, documented).
 * FRAMEBUFFER path (options == 0): REAL — records the context
 * destination. CGS-surface path (0x800): the surface store is not yet
 * wired cross-client; refusing LOUDLY rather than succeeding unbacked
 * (the SetIDMode lesson). */
IOReturn CLASS::sSetSurface(OSObject* target, void* reference,
                            IOExternalMethodArguments* args)
{
    CLASS* me = (CLASS*)target;
    uint64_t id_or_index = args->scalarInput[0];
    uint32_t options = (uint32_t)args->scalarInput[1];

    memset(args->structureOutput, 0, 11 * sizeof(uint32_t));
    args->structureOutputSize = 11 * sizeof(uint32_t);

    if (options & 0x800u) {
        /* CGS-surface binding (milestone 2 rung 2): look up FRESH —
         * the surface's lifetime belongs to its creating client. */
        VMAccelSurface* s = vmSurfaceRegistryFind((uint32_t)id_or_index);
        if (!s) {
            IOLog("VMQemuVGA3DUserClient: SetSurface id=%llu opts=0x%x — "
                  "NO SUCH SURFACE in registry — refusing\n",
                  id_or_index, options);
            return kIOReturnNoResources;
        }
        uint32_t* out = (uint32_t*)args->structureOutput;
        out[0] = s->width;  out[1] = s->height;
        out[2] = s->bytes_per_pixel;
        out[3] = s->bytes_per_row;
        IOLog("VMQemuVGA3DUserClient: SetSurface id=%llu opts=0x%x — "
              "BOUND (%ux%u bpp=%u row=%u)\n",
              id_or_index, options, s->width, s->height,
              s->bytes_per_pixel, s->bytes_per_row);
        me->m_bound_id = id_or_index;
        me->m_bound_options = options;
        return kIOReturnSuccess;
    }
    IOLog("VMQemuVGA3DUserClient: SetSurface fbIndex=%llu opts=0x%x — "
          "framebuffer destination bound\n", id_or_index, options);
    me->m_bound_id = id_or_index;
    me->m_bound_options = options;
    return kIOReturnSuccess;
}

/* LockMemory: the APP-TASK view of the bound surface's backing.
 * struct-out = {address, rowBytes} (worked example VMsvga2GA.cpp:593).
 * Requires WindowServer to have write-locked once (the backing is
 * lazy-created there); refuses loudly otherwise. */
IOReturn CLASS::sLockMemory(OSObject* target, void* reference,
                            IOExternalMethodArguments* args)
{
    CLASS* me = (CLASS*)target;
    if (!(me->m_bound_options & 0x800u) || me->m_bound_id == 0xFFFFFFFFFFFFFFFFull) {
        IOLog("VMQemuVGA3DUserClient: LockMemory — no CGS surface bound\n");
        return kIOReturnNotReady;
    }
    VMAccelSurface* s = vmSurfaceRegistryFind((uint32_t)me->m_bound_id);
    if (!s) {
        IOLog("VMQemuVGA3DUserClient: LockMemory — bound surface VANISHED "
              "(owner died?)\n");
        return kIOReturnNotReady;
    }
    if (!s->backing_memory) {
        IOLog("VMQemuVGA3DUserClient: LockMemory — backing not yet created "
              "(no WindowServer write-lock)\n");
        return kIOReturnNotReady;
    }
    if (me->m_app_map) {
        /* Already mapped in this task — hand the existing view. */
        uint64_t* out = (uint64_t*)args->structureOutput;
        out[0] = me->m_app_map->getVirtualAddress();
        out[1] = s->bytes_per_row;
        return kIOReturnSuccess;
    }
    IOMemoryMap* map = s->backing_memory->createMappingInTask(
        me->m_task, 0, kIOMapAnywhere);
    if (!map) {
        IOLog("VMQemuVGA3DUserClient: LockMemory — app-task mapping FAILED\n");
        return kIOReturnNoMemory;
    }
    me->m_app_map = map;
    uint64_t* out = (uint64_t*)args->structureOutput;
    out[0] = map->getVirtualAddress();
    out[1] = s->bytes_per_row;
    IOLog("VMQemuVGA3DUserClient: LockMemory — app view %llx row=%u\n",
          (unsigned long long)out[0], s->bytes_per_row);
    return kIOReturnSuccess;
}

/* UnlockMemory: drop the app-task view (scalar-out = swap flags, 0). */
IOReturn CLASS::sUnlockMemory(OSObject* target, void* reference,
                              IOExternalMethodArguments* args)
{
    CLASS* me = (CLASS*)target;
    if (me->m_app_map) {
        me->m_app_map->release();
        me->m_app_map = nullptr;
    }
    args->scalarOutput[0] = 0;
    return kIOReturnSuccess;
}

/* Every not-yet-implemented selector: loud, capped, refused. A silent
 * success here would be the invented-property failure mode in code
 * form. */
IOReturn CLASS::sStub(OSObject* target, void* reference,
                      IOExternalMethodArguments* args)
{
    static uint32_t s_stub_log = 0;
    if (s_stub_log < 20) {
        s_stub_log++;
        IOLog("VMQemuVGA3DUserClient: selector %u UNSUPPORTED (milestone 2)\n",
              args->selector);
    }
    return kIOReturnUnsupported;
}

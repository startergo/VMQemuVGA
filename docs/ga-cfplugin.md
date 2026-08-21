# The GA CFPlugIn — app-side accelerator attach

Design document for the missing piece of the CGS-surface present path.
Source of truth for the contract: the worked example
`../../VMsvga2-modern/GA/` (read-only reference; licence note in
[`vmsvga2-adoption.md`](vmsvga2-adoption.md) — MIT-style headers, Apple
portions, keep notices on anything carried across).

**Why this exists:** the kernel relay (0x600C, landed 2026-08-20)
delivers pixels into the DESKTOP scanout — one layer below WindowServer,
which recomposites the window region from its own view backing. Result:
constant flicker, two writers, one rect. The correct target is the
WINDOW'S OWN CGS surface, composited by WindowServer — and the
machinery for surfaces already exists in-kernel (`VMAccelSurfaceClient`,
18 selectors, WindowServer's loop, landed 2026-08-16). What does not
exist is the path that lets an APPLICATION drive it. That path is the
GA CFPlugIn — a userspace CFPlugin bundle that CGS loads in the app via
the framebuffer's `IOCFPlugInTypes` property, implementing
`IOGraphicsAcceleratorInterface` on top of a kext "2D context" user
client. It is also the exact gap that stopped `readfb` at
`IOCreatePlugInInterfaceForService`.

---

## The contract (verified against the worked example, file:line)

### Discovery trio — kext side, PUBLISHED ON THE FRAMEBUFFER

`VMsvga2Accel.cpp:592-604`, consumed by `IOAccelFindAccelerator()`
(10.6 SDK `IOKit/graphics/IOGraphicsInterfaceTypes.h:293-297`):

| Property | Value | On |
|---|---|---|
| `IOAccelTypes` | the accelerator's **IOService-plane path STRING** | framebuffer |
| `IOAccelIndex` | `0` | framebuffer |
| `IOAccelRevision` | `2` (`kCurrentGraphicsInterfaceRevision`) | framebuffer |
| `IOCFPlugInTypes` | copied from the accelerator — maps type UUID → plugin bundle | framebuffer |
| `AccelCaps` | `3` (QE claimed) | accelerator |

Interface constants: `kCurrentGraphicsInterfaceVersion = 1`,
`kCurrentGraphicsInterfaceRevision = 2` (verified against the real 10.6
SDK, per the adoption doc).

**VMQemuVGA's registry-verified state (2026-08-20, ioreg-audited —
corrects this charter's earlier source-only reading):**
- The **FB-side trio already exists and is correct**: `IOAccelTypes` =
  the accelerator's IOService-plane path STRING, `IOAccelIndex = 0`,
  `IOAccelRevision = 2`, all on the framebuffer — landed with `f551fba`
  (VMVirtIOFramebuffer, at accelerator creation). The "trio missing"
  claim below the header was written before that landed and never
  updated.
- `IOCFPlugInTypes` on the framebuffer: **absent — the plugin-
  instantiation blocker**. Added in milestone 1 (set on the accelerator
  at `VMQemuVGAAccelerator::start`, copied to the FB — the verbatim
  pattern).
- `AccelCaps` on the accelerator: **absent**. Added as `3` (QE).
- The accelerator additionally publishes an invented set (measured in
  ioreg: `IOGLBundleName="VMVirtIOGLEngine"` — a dead bundle,
  `IOGLContext`, `IOOpenGLRenderer`, `RendererID=0x24600`, NUMERIC
  `IOAccelTypes=7` competing with the FB's path string,
  `IOGLAccelTypes`/`IOSurfaceAccelTypes`/`IOVideoAccelTypes`=7,
  `PerformanceStatistics`/`Accum`, the `IOAcceleratorTypes` array) —
  live at `VMQemuVGAAccelerator.cpp` start, **removed in milestone 1**.
  The numeric `IOAccelTypes` removal should be inert
  (`IOAccelFindAccelerator` reads the FB's) — verified by a pre/post
  ioreg diff, not assumed (before-dump: `probe/ioreg-before-m1.txt`).

`IOAccelFindAccelerator` semantics (from `GA/VMsvga2GA.cpp:216`): takes
the FB service, returns the accelerator service + framebuffer index.

**The publication pattern, verbatim** (`AC/VMsvga2Accel.cpp:585-604` —
the ACCELERATOR's start reaches its framebuffer and sets properties ON
THE FRAMEBUFFER):

```c
plug = getProperty(kIOCFPlugInTypesKey);        // from the accelerator's PERSONALITY
if (plug)
    m_framebuffer->setProperty(kIOCFPlugInTypesKey, plug);
if (getPath(&pathbuf[0], &len, gIOServicePlane)) {
    m_framebuffer->setProperty(kIOAccelTypesKey, pathbuf);          // path STRING
    m_framebuffer->setProperty(kIOAccelIndexKey, 0ULL, 32U);
    m_framebuffer->setProperty(kIOAccelRevisionKey,
        (uint64_t)kCurrentGraphicsInterfaceRevision, 32U);          // 2
}
setProperty(kIOAccelRevisionKey, (uint64_t)kCurrentGraphicsInterfaceRevision, 32U);
setProperty("AccelCaps", 3ULL, 32U);            // QE claim, on the accelerator
```

**Personality value** (`Info-AC.plist`): `IOCFPlugInTypes = {
ACCF0000-0000-0000-0000-000a2789904e : "VMsvga2GA.plugin" }` — the
value is the plugin BUNDLE NAME, resolved from the kext bundle's
`Contents/PlugIns/` (the pbxproj copies the built plugin there). Ours:
`VMQemuVGAGA.plugin`, with its own factory UUID.

### The plugin bundle — userspace

`Info-GA.plist` shape: `CFPlugInTypes` maps the type UUID
`ACCF0000-0000-0000-0000-000a2789904e` → factory UUID → exported
factory symbol; `CFPlugInDynamicRegistration = NO`; personality
`IOProviderClass = IOFramebuffer`. Factory UUID in the worked example:
`03463B45-6FDD-4749-B6B7-15EB76BAA22F` (ours must be its own UUID).
The factory (`VMsvga2GA.cpp:1117`) returns a `GAType` whose vtable is
`IOGraphicsAcceleratorInterface`.

### The interface vtable — `_buildGAFTbl` (`VMsvga2GA.cpp:1083-1115`)

- IUnknown: QueryInterface (accepts `kIOGraphicsAcceleratorInterfaceID`,
  `kIOCFPlugInInterfaceID`, `IUnknownUUID`), AddRef, Release
- `version/revision` = 1/2
- Probe (order 2000), **Start**, Stop, Reset, CopyCapabilities,
  Flush, Synchronize, GetBeamPosition
- **AllocateSurface, FreeSurface, LockSurface, UnlockSurface,
  SwapSurface, SetDestination**, GetBlitter, WaitComplete
- `__gaInterfaceReserved[0] = WaitSurface`, `[1] = SetSurface` — CGS
  calls these through the reserved slots; the array is `[24]`, not 22
  (verified across three SDKs per the adoption doc)
- `GetBlitProc`/`WaitForCompletion` named-compat slots stay NULL

### vmStart (`VMsvga2GA.cpp:202-260`)

1. `IOAccelFindAccelerator(service, &accelerator, &framebufferIndex)`
2. `IOServiceOpen(accelerator, self, 2 /* 2D Context */, &context)`
3. `kIOVM2DGetConfig` (2 scalars out) → config words
4. `kIOVM2DReadConfigs` (struct) → config word 2
5. `vmReset`, `useAccelUpdates(context, 1)`

### The surface-wiring slots

- `vmAllocateSurface` (`:446`): with `kIOBlitHasCGSSurface`, stores the
  `cgsSurfaceID` in a per-surface `SurfaceInfo` on
  `surface->interfaceRef`, then `vmSetSurface(0x800, surface)`
- `vmSetSurface` (`:800`): sends `{cgsSurfaceID | framebufferIndex,
  options|fmt-bits}` via `kIOVM2DSetSurface` (11-word struct out) —
  THE binding of a CGS surface to accelerator state
- `vmLockSurface`/`vmUnlockSurface` (`:569/:610`):
  `kIOVM2DLockMemory/UnlockMemory` — the app-task view of the surface
  memory (the two-task two-view pattern: WindowServer locks via
  `map[0]` in the creator task, the app via `map[1]` — the driver may
  legally hand different memory to each; adoption doc
  "Two-task, two-view locking")
- `vmSetDestination` (`:683`) → vmSetSurface; `vmWaitSurface`
  (`:775`) → `kIOVM2DWaitImage`
- Blitters: GetBlitter hands out vmFill/vmCopy/vmCopyRegion →
  `kIOVM2DRectFill/RectCopy/CopyRegion` selectors

### Kernel selectors the 2D context must serve

`kIOVM2DGetConfig`, `ReadConfigs`, `ReadConfigEx`, `SetSurface`,
`ScaleSurface`, `LockMemory`, `UnlockMemory`, `SwapSurface`,
`WaitImage`, `DeleteImage`, `Finish`, `RectFill`, `RectCopy`,
`CopyRegion`, `useAccelUpdates`. The worked example reaches existing
kernel surfaces through a vendor-message cross-client channel
(`findSurfaceForID`, adoption doc "Intra-kernel cross-client channel") —
ours can instead call the surface objects directly (same kext).

---

## Build plan (milestones, each independently boot-verifiable)

1. **Trio fix + plugin skeleton + readfb negative control.** Fix the
   five properties (the trio must be its own boot variable — it changes
   what CGL does at discovery). Ship a plugin whose factory + vtable +
   vmStart work and whose remaining slots log-and-return-Unsupported.
   Kext gains a minimal type-2 "2D context" user client serving
   GetConfig/ReadConfigs/ReadConfigEx/Finish. PASS = a probe calling
   `IOCreatePlugInInterfaceForService(fb, kIOGraphicsAccelerator…)`
   gets the interface and vmStart opens the context — the exact call
   readfb died on. FAIL modes pre-registered: factory never called
   (bundle not found — check install location + CFPlugInTypes value),
   vmStart fails at IOAccelFindAccelerator (trio wrong), fails at
   IOServiceOpen type 2 (user client not wired).
2. **Surface binding.** SetSurface + AllocateSurface with a CGS surface
   id reach the kext surface machinery; LockSurface hands the app-task
   mapping of the surface backing.
3. **The present.** The relay (0x600C) writes into the bound window
   surface's backing instead of the desktop; WindowServer composites.
   Flicker dies; screen capture verifies.
4. **The substitute wires in.** CGLFlushDrawable in host-present mode
   drives the GA path; the shim's rect stash retires.

## Non-goals / cautions

- Do NOT book this as readback elimination (adoption doc cost-estimate
  correction): surfaces are guest-resident compositing buffers with
  DMA-out; net bandwidth unmeasured.
- The plugin is a NEW boot variable interacting with CGL discovery —
  one milestone per boot, pre-registered.
- `GLPlugin/` in this repo is a CGL *renderer* plugin (the superseded
  GLD direction), not a GA plugin — no overlap, do not build on it.

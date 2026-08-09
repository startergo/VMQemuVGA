# VMQemuVGA — Ledger

Current state of the project. **Volatile — update at the end of every session.**

Durable rules live in `.claude/CLAUDE.md` and `.claude/rules/`. This file is the
only place that describes what is true *right now*. If the two disagree about
state, this file wins.

Rules for maintaining this file:

- Record what the evidence supports, not what would be satisfying.
- A claim earns "verified" only with a negative control or a visual check —
  never from a success log alone.
- Never quietly drop an unexplained residual. If it stops mattering, say why.
- When an entry is superseded by later findings, move it to the "Superseded"
  section with a date and a note on what replaced it — don't delete it, and
  don't leave it competing with the current truth.

Last updated: 2026-08-09

---

## Critical methodological context

**Wip's `submitCommand` is the fake.** Verified 2026-08-08: the function on
`wip/checkpoint-20260807` is ~600 lines, writes the command to a single
`queue_buffer`, notifies the device, then reads the response back from the
*same* `queue_buffer`. No descriptor table, no avail/used rings, no
response-code validation. Terminal `return kIOReturnSuccess;` is unconditional.
The `#if VERBOSE_DIAGNOSTICS` decoration (`VirtIOQueueArchitecture`,
`CommandValidationSystem`, etc.) is camouflage. **Implication: any "wip worked"
memory based on lifecycle logs (start/enableController/refreshDisplay firing)
is suspect — those logs fire on the guest side without the device ever seeing
a real command. No pixel ever reached the host under wip's submitCommand.**
This reverses the earlier assumption that wip was the forward branch. Any
conclusion drawn from wip-era lifecycle logs is built on sand. The real
virtqueue lives on `master` (commit `33fe55b`), and everything verified in
this ledger is on that code path.

---

## Fixed and verified

- **Milestone B: correct Snow Leopard desktop on virtio-gpu-gl-pci.
  Confirmed visually 2026-08-09** — correct desktop below the menu bar
  (icons, dock, windows visible) and correct cursor after the `pixelFormat`
  fix. Negative control: `SET_SCANOUT(999)` returns `0x1203`, read back
  correctly. Full pipeline works end-to-end on the running variant:
  `VMVirtIOFramebufferPCI` matches the PCI device → helper `VMVirtIOGPU`
  initializes virtqueue → `enableController` creates resource + attaches
  backing + sets scanout → WindowServer connects via delegated
  `newUserClient` → base class `setupForCurrentConfig` calls
  `getApertureRange` → WindowServer maps the fixed aperture → draws desktop
  → refresh timer transfers to host via virtio-gpu → host composites to
  scanout → pixels on screen. Display comes up at 1920×1080 (Phase 4's
  default mode); real user-driven mode switches through Displays prefs
  work; cursor renders correctly at boot and after mode changes.

  **Earlier retraction (2026-08-08) is superseded — see Superseded section.**
  The retraction described horizontal-line shearing; that symptom was
  recorded when `getApertureRange` returned NULL (before Phase 2) and the
  visible pixels were the test pattern through a partial backing. Once the
  fixed aperture path landed, the symptom stopped existing — but nobody
  looked again until 2026-08-09, so the retraction survived in the ledger
  as a competing claim. The "symptom described a superseded build" pattern
  will recur; the guard is to check whether the symptom's preconditions
  still hold before treating it as live.

  Fix chain that unblocked Milestone B (in order of discovery):
  1. Capset read fixes (`mapBarByNumber`, response buffer, device-cfg offset)
  2. Remove `VMVirtIOGPU` personality (eliminated dual-instance virtqueue race)
  3. Delegate `newUserClient(type=0/1)` to `super` (WindowServer gets proper
     IOFramebufferUserClient instead of VMQemuVGAClient)
  4. `deallocateResource` sends `RESOURCE_UNREF` unconditionally (resource
     lifecycle reusable across WindowServer open/close cycles)
  5. Remove `setupForCurrentConfig` override (base class drives, calls
     `getApertureRange` for the first time)
  6. Remove forced `enableController` and `setupForCurrentConfig` calls from
     `open()` and `start()` (base class controls timing)
  7. `getApertureRange` / `getVRAMRange` return NULL when framebuffer not
     ready (no 4KB register BAR masquerading as VRAM)
  8. `setupFramebufferResource` idempotent guard (no reallocation while
     live) — later relaxed by Phase 3's resource-recreate path
  9. Mode table trimmed to 1024×768 only (prevents per-mode reallocation
     that corrupts zone free-list via freed-memory reuse) — **superseded
     by Phase 4's mode table expansion; the fixed-allocation model made
     the trim unnecessary**
  10. Bounded test pattern fill (`getLength()` check prevents heap
      corruption from out-of-bounds write when allocation is wrong size)

- **Real split-virtqueue.** Descriptor table, avail/used rings, notification,
  used-ring polling, response validation against `0x1100`/`0x1200`. Landed
  in commit `33fe55b`.

- **Feature negotiation.** `VIRTIO_GPU_F_VIRGL` + `VIRTIO_F_VERSION_1`
  negotiated; queue size 256 accepted. Host offers VIRGL — device
  `word0=0x30000013`, `word1=0x00000101`. Capsets read cleanly: VIRGL
  (id=1, version=1, size=308) and VIRGL2 (id=2, version=2, size=1408).

- **Negative controls pass.** `SET_SCANOUT(999)` returns `0x1203`, read and
  translated to `kIOReturnError` correctly. Both directions of the response
  path are proven.

- **Fake-`OSObject` pool refactor.** Replaced with typed pools. Cleared both
  the load panic and the long-standing `kextunload` panic.

- **Display backing ownership.** Framebuffer owns one allocation;
  `getApertureRange()` and `ATTACH_BACKING` point at the same memory; single
  attach with a scatter-list loop.

- **Boot panic in `OSUnserializeXMLparse` was a stale kext cache**, not the
  plist. Same kext + deleted caches boots cleanly. The install procedure is
  the actual defect and still needs fixing.

- **Dual-instance virtqueue race eliminated.** Removing the `VMVirtIOGPU`
  personality from `Info-FB.plist` (so only `VMVirtIOFramebufferPCI` matches
  the PCI device) eliminated two `VMVirtIOGPU` instances driving the same
  virtqueue. Single boot, single `this=` value across all `submit[N]` lines,
  zero `transferToHost2D` failures, negative control still passes.

- **Fixed-allocation model (Phases 1–4, all verified 2026-08-08 through
  2026-08-09).** Replaces per-mode reallocation with one fixed aperture:
  - **Phase 1: Pool unification.** `findResource` walks `m_resource_pool[]`
    with tombstones; `m_resources` OSArray deleted; `probeResourceTracking`
    self-check fires once per boot with `PROBE PASS`.
  - **Phase 2: Buffer allocated once in `start()`** via fallback ladder
    `{{4096,2160}, {2560,1600}, {1920,1200}, {1600,1200}, {1280,1024},
    {1024,768}}`. Aperture invariant: walks segments, requires
    `nr_entries == 1`, steps down if fragmented.
  - **Phase 3: Resource-recreate path.** `setupFramebufferResource`'s
    idempotent guard relaxed to allow recreate on mode change (UNREF +
    CREATE_2D + ATTACH_BACKING + SET_SCANOUT, buffer preserved).
    `probeResourceRecreate` self-check verifies buffer stability through
    recreate cycles.
  - **Phase 4: Mode table expansion.** `kSupportedModes[]` as single source
    of truth (modes 1–7: 1024×768 through 3840×2160). `filterModesByAllocation`
    populates `m_display_modes` at runtime based on which ceiling succeeded.
    Default = 1920×1080. At the 16 MB ceiling: 6 modes advertised (1024×768
    through 2560×1440); mode 7 (3840×2160) filtered out.

- **Software cursor dashed-line bug — FIXED 2026-08-09.** Root cause:
  `getPixelInformation` was not setting `pixelFormat` (an `IOPixelEncoding` =
  `char[kIOMaxPixelBits]`), leaving it uninitialized. Callers reusing one
  `IOPixelInformation` across mode enumeration inherited stale/garbage
  values. The base-class cursor compositing reads `pixelFormat` to pick
  how to interpret the framebuffer encoding for the cursor blit; a
  wrong/stale encoding → stretched dashed-line cursor. **Fix:**
  `bzero(pixelInfo, sizeof(*pixelInfo))` at entry, then
  `strlcpy(pixelInfo->pixelFormat, IO32BitDirectPixels, …)` alongside all
  existing field assignments. Cursor renders correctly at boot and through
  real user-driven mode changes.

  **Two adjacent concerns ruled out or fixed in the same thread:**
  - **Non-contiguous 35 MB backing** — `kIOMemoryPhysicallyContiguous`
    silently returns 2 segments at 35 MB on this 4 GB guest.
    `IODeviceMemory::withRange(phys, 35 MB)` would have described memory
    running past segment 1 into unrelated kernel pages. Fixed by the
    aperture invariant in `start()`'s fallback ladder — walks segments,
    requires `nr_entries == 1`, steps down to 16 MB. Latent
    kernel-memory-corruption bug caught while investigating the cursor.
  - **`super::setDisplayMode` re-enablement** — investigation showed it
    is NOT load-bearing for any observed bug. WindowServer refreshes
    `__private->pixelInfo` independently via `setupForCurrentConfig` →
    `doSetup` → `getPixelInformation` after every `setDisplayMode`. The
    bypass originally added for an AHCI/workloop race under TCG is left
    in place; re-enabling is at most a base-class-integration question.

- **virtio-vga-gl blue-screen fix — FIXED 2026-08-09.** The
  `useNativeScanout` decision was keyed on `requiresNativeMode` (false when
  VGA compat is present), which skipped the entire virtio-gpu display
  pipeline on virtio-vga-gl — blue screen instead of desktop. Fix:
  `useNativeScanout = (m_gpu_driver != nullptr)` — native scanout whenever
  the control queue is functional, regardless of VGA compat. VGA compat
  only matters before a driver loads; once this driver owns the device,
  the virtio-gpu protocol is the only rendering path. **Four variants now
  verified:** virtio-gpu-gl-pci, virtio-ramfb-gl, virtio-vga-gl, QXL.
  `readDDCBlock` fails on all variants — System Profiler shows a display
  entry on virtio-vga-gl but not on pure-GPU variants, but the mechanism
  is not EDID (unverified).

- **VRAM property fix — 2026-08-09.** `VRAM,totalsize` and `VRAM,totalMB`
  now publish the actual allocation size from `m_fb_backing->getLength()`
  (was hardcoded 512 MB). `ATY,memsize` removed (ATI-specific key, absurd
  on virtio). System Profiler on virtio-vga-gl displays VRAM to users —
  512 MB against a real 16 MB aperture was a user-facing incorrect claim.

- **`IOPowerManagement = {CurrentPowerState=0}` on the framebuffer node is
  cosmetic, not a real "off."** `setPowerState` is a no-op returning
  `kIOReturnSuccess` for any state, and the class never calls
  `registerPowerDriver`. IOService publishes the default state-0 dict with
  no power model behind it. Display work is not power-gated; refreshes are
  not suppressed by it.

- **`%zu` and `%u` format-specifier class** — CLAUDE.md rule that `%zu`
  misbehaves on this target. `start()` log lines fixed in Phase 2 (`%llu`
  with `(uint64_t)` cast). `createResource2D`'s `resource_size` log line
  still uses `%u` (`size=6baa80u` observed) — one-line fix when next in
  `VMVirtIOGPU.cpp`.

---

## Active task — 3D transport

The original scope of this project includes 3D acceleration via virgl.
`enable3DAcceleration` has capsets in hand (VIRGL id=1 v=1 size=308, VIRGL2
id=2 v=2 size=1408) and `CTX_CREATE` has succeeded on the eager
`initializeWebGLAcceleration` path (context 2 created, response `0x1100`).
**Nothing downstream of context creation has ever executed on a path that
matters.** Per the guardrails, the next step is proving the transport:

1. Create a 3D context explicitly (control the call, don't rely on the
   eager boot-time path).
2. Submit a clear command to a 3D resource.
3. `TRANSFER_FROM_HOST_3D` to read the cleared bytes back.
4. Verify the bytes match the clear color.

Everything above that (shaders, textures, GL routing) is a userspace
software problem. The transport proof is the load-bearing gate.

---

## Open

- **Hardware cursor — OPEN, pending GL/cursor-overlay investigation.**
  Queue 1 transport proven (used ring advances, PROBE PASS). Guest-side
  setup verified correct (64×64 BGRA, alpha=0xFF, scanout_id=0,
  TRANSFER_TO_HOST_2D before UPDATE_CURSOR, struct 56 bytes, resource
  persists — no teardown). QEMU accepted (no guest error in debug log).
  SPICE cursor channel delivered data to CocoaSpice
  (`set_cursor: type alpha(0), 0, 64x64`). **Cursor not visible on
  virtio-gpu-gl.**

  **QXL discriminator — POSITIVE.** QXL on the same UTM host shows a
  visible hardware cursor (`crsr=1`, genuine overlay, not composited into
  framebuffer). This confirms CocoaSpice renders cursor overlays —
  hardware cursor is not categorically impossible on UTM.

  **Hypothesis (labelled, not established):** virtio-gpu-gl uses GL
  scanout (DMABuf → Metal texture via `cs_gl_scanout`). Debug log
  confirms GL scanout IS active (`gl scanout fd: 42`, `cs_gl_scanout:
  got scanout`). QXL uses the 2D path (no GL scanout). CocoaSpice's
  GL display path may not composite cursor overlays on GL-rendered
  surfaces — explaining why cursor data reaches `set_cursor` but nothing
  appears. My earlier "cocoa/gl capable but no gl yet" was WRONG: the
  debug log proves GL scanout is active on virtio-gpu-gl boots.

  **CocoaSpice source review (github.com/utmapp/CocoaSpice):** the
  cursor data model is fully implemented for GL mode — `cs_cursor_set`
  uploads to a Metal texture, sets `cursorHidden = NO`, invalidates the
  display, which triggers the renderer. No code path is conditional on
  `isGLEnabled`. The actual renderer (a `CSRenderer` conformer) lives
  in UTM, not CocoaSpice — it decides how to composite the cursor source
  (`display.cursorSource`) alongside the display source. The gap is
  between CocoaSpice providing the cursor data and UTM's renderer drawing
  it in GL mode. Filed as an upstream issue candidate.

  **Note for future work:** `CSCursor.isInverted` returns
  `!self.display.isGLEnabled`. In GL mode the cursor would NOT be
  inverted; in 2D mode it IS. If a future UTM renderer fix enables
  GL cursor compositing, the cursor may initially appear upside-down —
  this property is the reason. Not the cause of invisibility (flipping
  doesn't hide), but the first thing to check if it renders wrong.

  **Confound to check before filing upstream:** the QXL and virtio-gpu
  VMs may differ in pointer device (usb-tablet vs PS/2 mouse) or UTM
  display settings (Retina/scaling). Different pointer devices change
  SPICE mouse mode, which could affect cursor compositing independently
  of the display device. Check both VM configs are otherwise identical
  before attributing the difference to display device type alone.

  **Build 2 status: open, gated on UTM.** Not abandoned — the QXL
  result changes the conclusion from "impossible" to "blocked on
  GL-path cursor compositing in UTM's renderer." crsr = 0 with
  WindowServer software compositing is the correct implementation on
  this configuration and works. The queue-1 transport code is ready
  if the UTM gap is resolved or a non-GL virtio-gpu variant is used.

  **Cursor queue constraint:** the cursor queue is one-way by design.
  QEMU's `virtio_gpu_handle_cursor` does `virtqueue_push(vq, elem, 0)` for
  every command (zero response bytes), accepted or rejected. "Used ring
  advanced" is the only feedback this queue offers. Future PROBE PASS on
  this queue means descriptor consumed, nothing more.

- **ARD (Apple Remote Desktop) regression at 16 ms refresh.** The timer's
  initial arm was shortened from 1000 ms to 16 ms; whether ARD's screen
  capture works correctly at that cadence is untested.

- **`VMVirtIOGPU::probe` OSData/OSNumber cast.** `probe()` reads `0x000000`
  for class-code because IOPCIFamily publishes `vendor-id`/`device-id`/
  `class-code` as OSData byte arrays, not OSNumber. `enableController`
  bypasses this via `configRead32`. The kext reaches the correct branch by
  accident today (zero routes the same as `0x0380`). Fix: use `configRead*`
  in probe as `enableController` does, or handle OSData properly.

- **3D beyond capsets.** Nothing downstream of `enable3DAcceleration` has
  executed on a meaningful path. Expect novel failures, not regressions.

- **Install script** does not delete/regenerate kext caches. Highest-value
  process fix available.

- **`submitCommand` suppresses TIMEOUT logging** for `0x104`
  (`RESOURCE_FLUSH`) and `0x105` (`TRANSFER_TO_HOST_2D`). The filter is
  correct; effect is that timeout failures in the refresh path are silent
  on the submitCommand side (the caller's failure log still fires). Worth
  knowing if the refresh path ever needs instrumentation.

- **Diagnostic log noise.** `getPixelInformation` and `setDisplayMode`
  log on every call; WindowServer iterates modes repeatedly for Display
  prefs UI. Quiet down or gate behind a debug flag now that the cursor
  investigation is done.

- **Second `setscanout(0, 999, …)` in `enableController`** — this is the
  deliberate negative-control probe (returns `0x1203`). Not stray code;
  intentional. Worth noting as load-bearing for the negative-control
  guarantee.

---

## Deferred — branch reconciliation

Do not start this merge without asking.

- `33fe55b` on `master` contains the virtqueue, the display-pipeline
  refactor, the pool refactor, and the negative controls. This is what
  makes the current boot work.
- `wip/checkpoint-20260807` predates it but contains
  `VMVirtIOGPU_IOFramebuffer.cpp` (+420 lines) and the working
  `VMVirtIOFramebufferPCI` personality.
- **Wip's `submitCommand` is the fake** — see "Critical methodological
  context" at the top of this file.
- Wip also removed substantial "3D manager" machinery that exists on
  master: `VMShaderManager.cpp` deleted (-319), `VMPhase3Manager.cpp` -631
  lines, `VMTextureManager.cpp` -301 lines, `VMQemuVGAAccelerator.cpp`
  rewritten (~869-line diff). This cleanup is needed independently of any
  specific bug — the eager 3D pre-allocation at boot
  (`initializeWebGLAcceleration` creating context + canvas + depth + 100 MB
  GPU memory) runs before the first refresh and is suspect for state
  pollution. Confirmed 2026-08-08: after the personality removal, the
  helper still runs `initializeWebGLAcceleration`, `VMTextureManager`
  Phase 1-5 init, and the full "3D managers initialized for QXL/Hyper-V DDA
  mode" log spam — nothing on the display path consumes any of it.
- `VMVirtIOGPU_IOFramebuffer.cpp` is not in wip's `project.pbxproj` either
  — uncompiled dead code on both branches.
- Reconciling is a four-way merge with a ~1582-line conflict in
  `VMVirtIOGPU.cpp`. High blast radius — and the wip side has a fake
  `submitCommand`, so "take wip's VMVirtIOGPU.cpp" is not a safe resolution.
  Any merge must preserve master's virtqueue and only borrow wip's
  personality + 3D-manager cleanup.

- **Bogus IORegistry properties to retire during the merge** (observed on
  the live `VMVirtIOFramebuffer` node 2026-08-08; all are vestigial from
  the QXL/ATI path or overclaim current capability):
  - ~~`VRAM,totalMB`, `VRAM,totalsize`, `IOAccelMemorySize`~~ — **FIXED
    2026-08-09:** now publish actual allocation size from the buffer.
    ~~`ATY,memsize`~~ — **removed** (ATI-specific key on a virtio device).
  - `IOAccelerator3D = Yes` alongside `IOGraphicsAccelerator = No` and
    `IODisplayAccelerated = No` — claims 3D the current path cannot deliver.
  - `IOGLBundleName = "GLEngine"` on the framebuffer node vs.
    `"VMVirtIOGLEngine"` on the `VMQemuVGAAccelerator` child — inconsistent.
  - `IOMetalBundleName = ""`, `IOGLESBundleName = ""` — empty / vestigial.
    (Metal does not exist on 10.6 per CLAUDE.md.)
  - `model = "VirtIO GPU 3D"` — same overclaim as `IOAccelerator3D`.
  - `class-code = <00000300>` on the framebuffer node — the "Override
    class-code for System Profiler" hack publishing a falsified `0x0300`
    (VGA-compat) on a device whose nub honestly reports `0x0380`.

---

## Unexplained residuals

(None currently active. Items that were here have either been promoted to
Open as actionable, or moved to Superseded as resolved. See those sections.)

---

## Superseded

Entries moved here when later findings contradicted them. Kept as history
rather than deleted; each has a date and a note on what replaced it. **Do
not treat these as competing claims about current state — they describe
states that no longer exist.**

- **(2026-08-08 → 2026-08-09) Milestone B retraction ("horizontal-line
  shearing below the menu bar").** Recorded 2026-08-08 based on a
  screenshot taken when `getApertureRange` returned NULL and the visible
  pixels were the test pattern through a partial backing. Retracted the
  earlier "Milestone B achieved" claim. **Superseded by:** the
  fixed-allocation aperture path (Phases 2–4) plus the `pixelFormat` fix
  (2026-08-09). Visual confirmation 2026-08-09 shows correct desktop below
  the menu bar, no shearing. **Lesson:** "symptom described a superseded
  build" is a pattern that will recur — always check whether the symptom's
  preconditions still hold before treating it as live.

- **(2026-08-08 → 2026-08-08) `deallocateResource` skips `RESOURCE_UNREF`
  when `findResource` returns NULL.** Described the pre-fix state where the
  pool was split between `m_resource_pool[]` and `m_resources` OSArray.
  **Superseded by:** Phase 1 pool unification — UNREF is unconditional
  (verified by reading `VMVirtIOGPU.cpp:3250-3266`), `m_resources` OSArray
  deleted, cleanup walks `m_resource_pool[]` with tombstones.

- **(2026-08-08 → 2026-08-08) Capset read fixes "written but not landed".**
  **Superseded by:** the fixes landed (they are item 1 of the Milestone B
  fix chain). Capsets read cleanly on every boot: VIRGL id=1 v=1 size=308,
  VIRGL2 id=2 v=2 size=1408.

- **(2026-08-08 → 2026-08-09) "Fixed-allocation refactor" as a deferred
  Open item.** Described per-mode reallocation as a latent zone-corruption
  risk to be addressed later. **Superseded by:** Phases 1–4 delivered the
  fixed-allocation model — buffer allocated once in `start()`, resource
  recreated per mode change, aperture stable across the recreate.

- **(2026-08-08 → 2026-08-09) "Mode table trimmed to 1024×768 only"
  (fix chain item 9).** Described the temporary workaround for the
  per-mode reallocation hazard. **Superseded by:** Phase 4's mode table
  expansion to 7 modes (6 advertised at the 16 MB ceiling, boot at
  1920×1080). The fixed-allocation model made the trim unnecessary.

- **(2026-08-08 → 2026-08-09) Non-contiguous backing at 35 MB as a "defer
  until 4K is needed" concern.** The 35 MB allocation was observed to
  produce `nr_entries=2` in `attachBacking`. **Superseded by:** the
  aperture invariant in `start()`'s fallback ladder (2026-08-09) walks
  segments and requires `nr_entries == 1` or steps down. At 35 MB the
  allocator returns 2 segments; we fall back to 16 MB. The concern about
  4K-mode writes past segment 1 is moot because 4K mode is filtered out
  by `filterModesByAllocation` when the buffer is 16 MB. Restoring 4K
  requires investigating why `kIOMemoryPhysicallyContiguous` silently
  returns 2 segments at tens-of-MB on this 4 GB guest.

- **(2026-08-08 → 2026-08-08) Boot log "60 Hz vs 1 Hz" inconsistency.**
  Resolved: the 1 s initial arm in `open()` was a real wart; fixed by
  shortening to 16 ms. `enableController`'s "60 Hz" log at `:1341` was
  already accurate (paired with the 16 ms arm at `:1340`). The timer's
  self-rearm at `:2059` is 16 ms. `info->refreshRate = 60 << 16` at
  `:662` is the declared-intent value.

- **(2026-08-08 → 2026-08-08) Class-code three-way disagreement
  (`lspci` 0x0380 / probe 0x000000 / ioreg 0x0300).** Two of three
  explained by ioreg decode: PCI nub publishes `<00800300>` = `0x0380`
  (honest, matches `lspci`); framebuffer node publishes `<00000300>` =
  `0x0300` (the kext's falsified "Override class-code for System Profiler"
  hack). Only `probe()`'s `0x000000` remains as a real bug — tracked in
  Open as the OSData/OSNumber cast issue.

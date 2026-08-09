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

Last updated: 2026-08-08

---

## Fixed and verified

- **Milestone B: correct Snow Leopard desktop on virtio-ramfb-gl.** Verified
  2026-08-08 by visual check (user confirmed: menu bar, dock, icons, wallpaper)
  and negative control (`setscanout(999)` → `0x1203`). The full display
  pipeline works end-to-end on virtio-ramfb-gl: `VMVirtIOFramebufferPCI`
  matches → helper `VMVirtIOGPU` initializes virtqueue → `enableController`
  creates resource + attaches backing + sets scanout → WindowServer connects
  via delegated `newUserClient` → base class `setupForCurrentConfig` calls
  `getApertureRange` → WindowServer maps 3MB aperture → draws desktop →
  refresh timer transfers to host via virtio-gpu → host composites to
  scanout → pixels on screen. Test pattern overwritten with real desktop
  content.

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
  8. `setupFramebufferResource` idempotent guard (no reallocation while live)
  9. Mode table trimmed to 1024×768 only (prevents per-mode reallocation
     that corrupts zone free-list via freed-memory reuse)
  10. Bounded test pattern fill (`getLength()` check prevents heap corruption
      from out-of-bounds write when allocation is wrong size)

- **Real split-virtqueue.** Descriptor table, avail/used rings, notification,
  used-ring polling, response validation against `0x1100`/`0x1200`. Landed in
  commit `33fe55b`.
- **Feature negotiation.** `VIRTIO_GPU_F_VIRGL` + `VIRTIO_F_VERSION_1`
  negotiated; queue size 256 accepted. Host offers VIRGL — device
  `word0=0x30000013`, `word1=0x00000101`.
- **Negative controls pass.** `SET_SCANOUT(999)` returns `0x1203`, read and
  translated to `kIOReturnError` correctly. Both directions of the response path
  are proven.
- **Fake-`OSObject` pool refactor.** Replaced with typed pools. Cleared both the
  load panic and the long-standing `kextunload` panic.
- **Display backing ownership.** Framebuffer owns one allocation;
  `getApertureRange()` and `ATTACH_BACKING` point at the same memory; single
  attach with a scatter-list loop.
- **Boot panic in `OSUnserializeXMLparse` was a stale kext cache**, not the
  plist. Same kext + deleted caches boots cleanly. The install procedure is the
  actual defect and still needs fixing.

---

## Active task — none (Milestone B achieved)

**Milestone B verified 2026-08-08 on virtio-ramfb-gl.** Full SL desktop:
menu bar, dock, icons. See "Fixed and verified" above for the complete
fix chain.

**Next:** testing all virtio device variants. Current working state is
virtio-ramfb-gl (UTM-specific). Other targets: virtio-gpu-gl-pci (vanilla
QEMU), virtio-gpu (no GL), QXL (already confirmed working).

Open items (non-blocking for display):
1. **Cursor — TEMPORARY fix shipped, permanent fix queued.** Temporary:
   `getAttribute(crsr)` returns 0 (no hardware cursor), `setCursorImage`/
   `setCursorState` return `kIOReturnUnsupported`. WindowServer falls back
   to software cursor (composited into framebuffer, transferred by refresh
   timer at 60 Hz). Cost: every mouse move dirties the framebuffer → 3 MB
   transfer at 60 Hz = ~180 MB/s under TCG. Permanent: implement virtio-gpu
   cursor queue (queue 1, `UPDATE_CURSOR` 0x240 / `MOVE_CURSOR` 0x241).
   Host composites cursor, one small command per move, no framebuffer
   transfer. Then flip `crsr` back to 1 and `setCursorImage/State` back
   to success.
2. **Fixed-allocation refactor** — per-mode reallocation in
   `setupFramebufferResource` is latent zone-corruption risk. Allocate once
   for largest mode, return stable aperture.
3. **`deallocateResource` pool cleanup** — UNREF fires but OSArray cleanup
   loop uses stale pool reference. Separate consistency fix.
4. **`VMVirtIOGPU::probe` OSData/OSNumber** — variant detection correctness.
5. **Install script** — cache deletion should be automated.
6. **Wip merge** — 3D manager cleanup (deferred).

## Previously fixed — refresh timeout

Fixed and verified by instrumentation 2026-08-08. Removing the `VMVirtIOGPU`
personality from `Info-FB.plist` (so only `VMVirtIOFramebufferPCI` matches the
PCI device) eliminated the dual-instance race. Single boot, single `this=`
value across all `submit[N]` lines, zero `transferToHost2D` failures, negative
control still passes (`setscanout(999)` → `0x1203`), capset reads unchanged
(VIRGL + VIRGL2).

Root cause (kept for context): two `VMVirtIOGPU` instances were driving the
same device virtqueue — the personality-matched instance from the `VMVirtIOGPU`
personality, and the internal helper `VMVirtIOFramebuffer::start()` constructs
via `m_gpu_driver = new VMVirtIOGPU()`. The helper's re-init of `avail->idx=0`
clobbered the personality-matched instance's ring progress, causing every
submit on the first instance to time out (150 ms poll expiry, `0xe00002d6`).

Removing the personality also eliminated one copy of the eager
`initializeWebGLAcceleration` pre-allocation (context + canvas + depth + 100 MB
GPU memory) that ran on the dead instance. The helper still does this — see
the wip-merge rationale below.

---

## Open

- **`submitCommand` suppresses TIMEOUT logging for `0x104` (RESOURCE_FLUSH) and
  `0x105` (TRANSFER_TO_HOST_2D).** The filter is `noisy = (cmd->type == 0x104 ||
  cmd->type == 0x105)` (correctly written — both sides are full comparisons).
  Effect: when `transferToHost2D` times out, `submitCommand` returns
  `kIOReturnTimeout` (`0xe00002d6`) silently — the "TIMEOUT on cmd 0x%x" line is
  suppressed. The `transferToHost2D: Command failed: 0xe00002d6` log seen in boot
  output is the *caller's* failure log, not the timeout announcement. Any future
  instrumentation that needs to see the timeout path for refresh commands has to
  either bypass the filter or add a parallel unconditional log.

- **Capset read fixes** — six edits, written but not landed:
  1. `mapBarByNumber()` helper extracted as the single BAR→index translation.
  2. Line 826 uses it instead of `mapDeviceMemoryWithIndex(bar_number)`.
  3. `setupGPUMemoryRegions` refactored to call it.
  4. Device-cfg read inserted between `setupGPUMemoryRegions()` and
     `enable3DAcceleration()`, using the capability-reported offset.
  5. `initHardwareDeferred()` deleted (dead, and reads the wrong offset).
  6. Loop variable renamed `capset_id` → `capset_index`.

  Prediction when landed: `num_scanouts=1`, `num_capsets` 1 or 2.

- **3D beyond capsets** — nothing downstream of `enable3DAcceleration` has ever
  executed. Expect novel failures there, not regressions.

- **Install script** does not delete/regenerate kext caches. Highest-value
  process fix available.

---

## Deferred — branch reconciliation

Do not start this merge without asking.

- `33fe55b` on `master` contains the virtqueue, the display-pipeline refactor,
  the pool refactor, and the negative controls. This is what makes the current
  boot work.
- `wip/checkpoint-20260807` predates it but contains
  `VMVirtIOGPU_IOFramebuffer.cpp` (+420 lines) and the working
  `VMVirtIOFramebufferPCI` personality.
- **Wip's `submitCommand` is the fake** (CLAUDE.md ground rule). Verified
  2026-08-08: function is ~600 lines, writes the command to a single
  `queue_buffer`, notifies the device, then reads the response back from the
  *same* `queue_buffer`. No descriptor table, no avail/used rings, no
  response-code validation. Terminal `return kIOReturnSuccess;` is
  unconditional. The `#if VERBOSE_DIAGNOSTICS` decoration
  (`VirtIOQueueArchitecture`, `CommandValidationSystem`, etc.) is the
  camouflage pattern described in CLAUDE.md. Implication: any "wip worked"
  memory based on lifecycle logs (start/enableController/refreshDisplay
  firing) is suspect — those logs fire on the guest side without the device
  ever seeing a real command. No pixel ever reached the host under wip's
  submitCommand.
- Wip also removed substantial "3D manager" machinery that exists on master:
  `VMShaderManager.cpp` deleted (-319), `VMPhase3Manager.cpp` -631 lines,
  `VMTextureManager.cpp` -301 lines, `VMQemuVGAAccelerator.cpp` rewritten
  (~869-line diff). This cleanup is needed independently of any specific bug —
  the eager 3D pre-allocation at boot (`initializeWebGLAcceleration` creating
  context + canvas + depth + 100 MB GPU memory) runs before the first refresh
  and is suspect for state pollution even when its functions are byte-identical
  to wip's. The merge is real work, not optional cleanup. **Named instance
  confirmed 2026-08-08:** after the personality removal, the *helper* (the
  single legitimate `VMVirtIOGPU`) still runs `initializeWebGLAcceleration`,
  `VMTextureManager` Phase 1-5 init, and the full "3D managers initialized for
  QXL/Hyper-V DDA mode" log spam — nothing on the display path consumes any of
  it, and the eager `createRenderContext`/`createResource3D` calls happen
  before the first `transferToHost2D`. This is concrete evidence (not "log
  spam") for the wip merge as state-pollution prevention, not just tidiness.
- Note: `VMVirtIOGPU_IOFramebuffer.cpp` is not in wip's `project.pbxproj`
  either — it's uncompiled dead code on both branches. Treating it as "the
  missing implementation" was a wrong turn.
- Reconciling is a four-way merge with a ~1582-line conflict in
  `VMVirtIOGPU.cpp`. High blast radius — and the wip side has a fake
  `submitCommand`, so "take wip's VMVirtIOGPU.cpp" is not a safe resolution.
  Any merge must preserve master's virtqueue and only borrow wip's
  personality + 3D-manager cleanup.

Order: instrument the refresh timeout (one variable, current task), then start
the merge as its own multi-step task. The timeout diagnosis may shape which
parts of wip are actually load-bearing.

---

## Unexplained residuals

Keep these on the books until they are explained or explicitly retired.

- **`deallocateResource` skips `RESOURCE_UNREF` when `findResource` returns NULL.**
  Non-blocking for initial display (the first resource stays live and the
  aperture is reusable), but blocks resolution changes and WindowServer
  close/reopen cycles. Root cause: pool split between `m_resource_pool[]`
  (typed pool, post-refactor) and `m_resources` (OSArray, "kept for compat").
  `findResource` searches one, `deallocateResource`'s cleanup loop iterates
  the other. Fix: make UNREF unconditional (device is source of truth for
  resource existence), then unify on `m_resource_pool[]` and delete the OSArray.
  Predicted verification: `cmd=0x102` (UNREF) appears in log before second
  `createResource2D`, second create returns `0x1100`.

- **`VMVirtIOGPU::probe` is a correctness bug in variant detection, not a
  logging nit.** Confirmed via `lspci` that hardware class code is `0x0380`
  (Display / Other = virtio-gpu-pci, the pure-GPU variant). `VMVirtIOGPU::probe`
  reads `0x000000` instead, and the same probe logs `"Could not read vendor-id
  or device-id properties, Trusting IOPCIMatch"`. **probe reads nothing** —
  vendor-id, device-id, class-code all come back empty or zero, so every
  device-variant branch in probe()/start() is currently deciding on constant
  zeros. Likely cause: IOPCIFamily publishes `vendor-id`, `device-id`, and
  `class-code` on the nub as **OSData byte arrays, not OSNumber**. An
  `OSDynamicCast(OSNumber, ...)` against them returns NULL on every cast,
  producing exactly the "could not read" log plus zero class code.
  `enableController` gets the right answer because it bypasses the properties
  and uses `configRead32(kIOPCIConfigClassCode)` directly.

  Why this matters: `0x0380` vs `0x0300` is precisely the field that
  distinguishes `virtio-gpu-pci` (no VGA BIOS, no aperture) from `virtio-vga`
  (has VGA aperture). It's the input to `"Detected virtio-gpu-gl-pci (pure
  GPU, no VGA BIOS)"` and the `hasVGACompat` / `requiresNative` decisions.
  Today the kext reaches the correct branch *by accident* — zero happens to
  route to the same branch as `0x0380` would. Point this kext at `virtio-vga`
  and it will incorrectly take the pure-GPU path on a device that does have a
  VGA aperture, skipping whatever VGA-compat setup that variant needs.

  Fix is small (use `configRead16`/`configRead32` in probe as `enableController`
  already does, or handle the OSData properly). Worth its own commit — it
  changes what several conditionals mean.

  Side benefit: retires the older "class 0x000000 + 1af4:1050 can't distinguish
  ramfb-gl from gl-pci" reasoning. That reasoning was built on a value the
  kext never actually read. The hardware was telling us `0x0380` the whole
  time — it just wasn't being heard.
- **Boot log inconsistency:** `enableController` logs "60 Hz" while
  `displayRefreshTimer` re-arms with `setTimeoutMS(1000)` (1 Hz). If the timer
  is the only path pushing updates, the desktop repaints once per second.
- **A second `setscanout(999)` negative control in `enableController`** of
  unverified origin. Confirm intentional or remove — a bogus `SET_SCANOUT` in
  the display bring-up path should not become load-bearing by accident.
- **Milestone B has not been confirmed.** No visual check of a correct desktop,
  top to bottom, has been recorded. Until it is, the display pipeline is
  "commands succeed," not "works."

  **UPDATE 2026-08-08: Milestone B NOT achieved.** Initial claim was made
  without inspecting the screenshot — retracted. The actual display shows
  **horizontal-line shearing / artifacting**: menu bar at top renders
  correctly (white bar, "Snow Leopard" text, icons visible), but the entire
  main display area below is garbled with a static-like horizontal-line
  pattern. No windows, icons, or desktop content discernible below the menu
  bar. Classic **stride mismatch** signature: first rows align correctly,
  each subsequent row progressively offset. Investigating stride/rowBytes
  contract between `getRowBytes()`, `RESOURCE_CREATE_2D` dimensions, backing
  allocation, and what WindowServer writes into the aperture.

  **Lesson reinforced:** "the image is from the desktop" was treated as
  "correct desktop visible" without visual inspection. CLAUDE.md ground
  rule violated. Always inspect the image before claiming Milestone B.

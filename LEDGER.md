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

## 3D transport probe + transfer-struct bug — 2026-08-09

Added `probeTransport3D()` (CTX_CREATE → RESOURCE_CREATE_3D → ATTACH_BACKING
→ CTX_ATTACH_RESOURCE → CREATE_OBJECT surface → SET_FB+CLEAR+NOP →
TRANSFER_FROM_HOST_3D → byte-equal positive + negative control). Same shape
as the three prior probes. Negative control proved its place: with the
readback buffer re-filled to 0xCD between G and G′, G′ came back as
**0xcdcdcdcd** — i.e. the host wrote zero bytes (not zeros) to the guest
backing. That distinction was only available because of the re-fill; a
single run could not have distinguished "host wrote nothing" from "host
wrote the zero-initialised resource contents".

**Root cause — virtio_gpu_transfer_to_host_3d used virtio_gpu_rect
(4 dwords / 16 bytes) where UTM QEMU expects virtio_gpu_box (6 dwords /
24 bytes).** Wire-size mismatch: guest sent 64-byte commands, host
`VIRTIO_GPU_FILL_CMD` reads 72 bytes, sees the size mismatch, emits
`qemu_log_mask(LOG_GUEST_ERROR, "command size incorrect 64 vs 72")`,
returns OK_NODATA without ever calling `virgl_renderer_transfer_read_iov`.
Guest sees 0x1100 — indistinguishable from success by construction
(same shape as SUBMIT_3D always returning OK).

**Fix — scoped to the 3D pair only.** 2D transfers (`virtio_gpu_transfer_to_host_2d`)
legitimately use `virtio_gpu_rect` and run 60×/s on the working display
path — left alone. The 3D struct now uses `virtio_gpu_box` (field renamed
`r` → `box`), and two compile-time size assertions make the layout
unrepresentable:

```c
typedef char vgpu_box_size_check[(sizeof(struct virtio_gpu_box) == 24) ? 1 : -1];
typedef char vgpu_tf3d_size_check[(sizeof(struct virtio_gpu_transfer_to_host_3d) == 72) ? 1 : -1];
```

### 3D transport — VERIFIED (negative-control-confirmed) — 2026-08-09

After the box-struct fix, the probe produced byte-perfect readback on
both clears:

| | First dword | Bytes (LE) | Decoded R8G8B8A8_UNORM | Clear color × 255 |
|---|---|---|---|---|
| G | `0xff339966` | `66 99 33 ff` | (R=102, G=153, B=51, A=255) | (0.40, 0.60, 0.20, 1.00) → (102, 153, 51, 255) ✓ |
| G′ | `0xffcce51a` | `1a e5 cc ff` | (R=26, G=229, B=204, A=255) | (0.10, 0.90, 0.80, 1.00) → (25.5→26, 229.5→229, 204, 255) ✓ |

The 0.5 fractions land on the side predicted by the actual IEEE 754
representations: `0.9f` is `0.899999976…` so ×255 = `229.4999…` rounds
to 229 = `0xe5`; `0.1f` is `0.100000001…` so ×255 = `25.50000038…`
rounds to 26 = `0x1a`. A coincidence would not reproduce the rounding
direction of two different constants. **Transport is verified.**

The readback also closes three previously-unknowable unknowns
transitively: CLEAR could not have written those bytes without
CREATE_OBJECT's surface payload and SET_FRAMEBUFFER_STATE's binding
both being correct on the wire — two commands that returned no signal
of their own (SUBMIT_3D returns 0x1100 unconditionally). One readback,
three unknowns closed.

### Probe pattern-check bug — encoding assumed, not verified — 2026-08-09

The probe initially labelled byte-perfect readbacks as `PROBE FAIL`
three times in succession, each for a different encoding assumption:
the box-vs-rect struct (covered above), the unorm-vs-float encoding,
and the rounding-formula mismatch.

**Unorm-vs-float:** the first pattern check computed expected values
with `virgl_pack_float()` (raw IEEE 754 bits — `0x3ECCCCCD` for 0.40),
but virgl stores R8G8B8A8_UNORM as 8-bit-per-channel unorm
(`0x66` for 0.40×255). Fixed by switching to unorm packing.

**Rounding formula:** after the unorm fix, G passed but G′ was off by
1 ULP on the green channel for clear color 0.9. Two wrong diagnoses
were attempted before the right one:
- *Wrong #1 (round vs truncate):* proposed `roundf` or `±1 tolerance`.
- *Wrong #2 (float32 vs double, my direction reversed):* proposed
  `__builtin_lrintf` to force float32 evaluation, claiming the
  compiler was promoting to double. Arithmetic check: `0.9f × 255`
  mathematically = 229.4999939, but 229.5 IS representable in float32
  and the deficit is within half a ULP — so float32 evaluation rounds
  the product UP to exactly 229.5f, and `lrintf(229.5f)` under
  round-half-to-even gives 230. Double evaluation preserves
  229.4999939 and gives 229. **The host's 229 comes from higher
  precision, not lower.** `volatile` to force runtime float32 would
  have made the gap larger, not smaller.

**Right diagnosis:** the GL spec permits ±1 ULP on float-to-unorm
conversion. The clear colour travels as float32 and is converted by
Metal/ANGLE's rasterizer on the host. **No guest-side formula
reproduces the host's choice by construction** — chasing exact-match
is chasing determinism that doesn't exist.

**Fix:** pick clear colours that aren't on a rounding boundary. Each
value should be a slight over-approximation of the exact decimal in
float32 so ×255 rounds cleanly to the intended integer with no `.5`
boundary to land on:
- `(0.20, 0.40, 0.60, 1.00) → (51, 102, 153, 255)` ✓
- `(0.80, 0.20, 0.40, 1.00) → (204, 51, 102, 255)` ✓

The ±1 tolerance stays as a guard that never fires in normal operation
— and logs a single summary line per phase (distinguishing systematic
vs random drift) when it does, so a real regression reads differently
from precision noise. Final state: **PROBE PASS, 64/64 exact on both
G and G′, tolerance guard quiet.**

### Rule: derive expected from bytes-sent, not from constant-of-origin — 2026-08-09

**When checking against host-produced bytes, derive the expected value
from the bytes you sent, never by re-deriving from the original
constant.** This habit would have caught all three encoding bugs this
session (box-vs-rect, unorm-vs-float, rounding-formula) at the first
probe run instead of requiring multiple debugging cycles.

Caveat discovered this session: the rule is necessary but not
sufficient. Even deriving from the sent float32, the GL spec's ±1 ULP
permit on float-to-unorm conversion means no guest computation
reproduces the host's rasterizer exactly. The rule gets you to the
right encoding; **picking non-boundary inputs** is what gets you to a
quiet EXACT match. Both habits together: derive from bytes-sent, and
avoid boundary-valued test inputs when the host is authoritative and
nondeterministic-by-spec.

### Next frontier — seam decision, not kext work

3D transport is now verified end-to-end through the kext. Per the
acceleration guardrails: **the kext is transport; command generation
belongs in userspace.** Proving transport doesn't change that boundary;
it removes the reason the seam decision has been deferred. The next
question is *where* GL commands get generated (userspace accelerator
architecture), not what to submit next from the kext. Treating this as
a kext task would expand blast radius past transport back into command
generation — exactly the boundary the guardrails exist to enforce.

---

## Refresh-throttle optimisation — landed 2026-08-09

Display refresh reduced from 60 Hz to ~15 Hz full-surface (every 4th
timer tick). **No image quality regression — verified visually** on
virtio-gpu-gl-pci at 1920×1080: no artifacts during continuous mouse
motion, no smear on window drags, dock and menus respond normally.

**Cost model — the win is guest-CPU, not bandwidth.** Each refresh is
two commands (TRANSFER_TO_HOST_2D + RESOURCE_FLUSH), each with an MMIO
doorbell write and a poll loop. Under TCG every doorbell is expensive;
going from 120 cmd/s to 30 cmd/s is a real guest-CPU saving. Host-side
memcpy on Apple Silicon is cheap and was never near a limit —
"120 MB/s vs 480 MB/s" framing is misleading, and a future session
reading that framing would reasonably conclude dirty rects are the
obvious next optimisation. They are not, for the reason above and the
three closed investigations below.

**Three dirty-rect paths investigated and falsified 2026-08-09:**

1. **`setCursorState` override** — unreachable while `crsr = 0`.
   IOFramebuffer base only routes it to drivers that advertised a
   hardware cursor. Past boot logs that showed "Position (x,y)" lines
   were from a different code state; on the current path the method
   is structurally never called. Verified absent in boot log.

2. **`shmem->cursorLoc` / `cursorSize`** — sampled static across 3 s
   of continuous mouse motion. (The one-shot diagnostic that logged
   "shmem=0 UNAVAILABLE" on the first call was misleading: priv
   starts NULL but is populated shortly after. The shmem pointer was
   live; the cursor fields themselves were frozen.)

3. **`shmem->cursorRect` / `shmem->oldCursorRect`** — same. Layout
   canary passed (`screenBounds` reads correctly as 0,0,1920,1080;
   `sizeof(StdFBShmem_t)=240` vs `structSize=278768` is struct +
   cursor image storage, not a mismatch), so the region is mapped
   right. The fields just aren't being written.

**Mechanism:** with `crsr = 0` on 10.6, WindowServer composites the
cursor into the aperture from userspace via CoreGraphics. The kernel
never participates, which is why every in-kernel cursor field is
frozen at the boot-console state near (15,15). Nothing was broken —
there is simply no in-guest signal to hook. The real cursor-
responsiveness fix is **host-composited hardware cursor**, which is
blocked on the UTM GL cursor-compositing question (see Open: Hardware
cursor), not on anything in this driver.

**Cross-variant evidence (2026-08-09):** QXL on the same UTM host
advertises `crsr = 1`, and on QXL both (a) shmem cursor fields track
the mouse and (b) the hardware cursor renders visibly. Same host,
same driver family, different `crsr` setting, different cursor
pipeline. That confirms the gate is `crsr`, not anything virtio-gpu-
specific — and on virtio-gpu-gl, `crsr = 1` is gated on the UTM GL
cursor-compositing question, which is why this driver reports 0.

**Content-diff dirty tracking rejected as backwards on this
configuration.** Bytes are not the bottleneck (cheap host memcpy);
command count is. Sub-rects would still cost two commands per tick
plus the TCG-emulated CPU for content hashing — net regression.

**Methodology lessons (2026-08-09, this thread):**

- **One-shot diagnostics can lock in wrong conclusions.** The
  "shmem=0 UNAVAILABLE" log fired before priv was populated; "shmem
  is NULL throughout" was wrong by 1 second. Verify values persist
  across multiple calls before claiming "X is NULL throughout."
- **Cost-model framing matters in the ledger.** "120 MB/s" would
  invite a future session to chase dirty rects; "30 doorbell round-
  trips/s" correctly identifies the throttle as terminal.
- **Empirical claims from memory need source verification.** "Real
  coords from setCursorState in past boot logs" was cited as
  evidence for an approach that turned out to be structurally
  unreachable. Past states may not match present configuration.
- **Struct field choice matters.** `cursorLoc` and `cursorSize`
  sounded like the on-screen cursor rectangle; `cursorRect` and
  `oldCursorRect` are. In the end none were live, but sampling the
  wrong fields first cost an extra diagnostic boot.

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
  in probe as `enableController` does, or handle OSData properly. **Fixed
  2026-08-09** — probe now uses `configRead32(kIOPCIConfigVendorID)` /
  `kIOPCIConfigClassCode` directly, same pattern as
  `VMVirtIOFramebuffer::probe`. See "Superseded" section.- **3D beyond capsets.** Nothing downstream of `enable3DAcceleration` has
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

- **(2026-08-09) `VMVirtIOGPU::probe` reads no PCI properties.** Was listed
  as a latent open bug: `probe()` used `OSDynamicCast(OSNumber, getProperty(...))`
  but IOPCIFamily publishes `vendor-id`/`device-id`/`class-code` as OSData,
  so the cast returned nullptr and the values came back zero. Outcome was
  unaffected because zero class-code routes to the same branch as `0x0380`,
  and `enableController` does its own variant detection via `configRead32`.
  **Superseded by:** `c7f13d2 fix(probe): use configRead32 for PCI property
  reads, not OSDynamicCast(OSNumber)` — probe now uses
  `configRead32(kIOPCIConfigVendorID)` / `configRead32(kIOPCIConfigClassCode)`
  directly, matching `VMVirtIOFramebuffer::probe` at line 120. Single source
  of truth for variant detection across probe and enableController.

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

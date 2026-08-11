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

Last updated: 2026-08-10

---

## Increment C — Mesa-driven virgl glClear + glReadPixels — VERIFIED — 2026-08-10

Mesa's virgl Gallium driver, through virgl_iokit_winsys (Mesa-VirGL
commit c703f8fb910), produces byte-exact clears on 10.6 via UTM's
embedded virglrenderer. The full 3D stack works end-to-end for the
first time:

Application → OSMesa → Gallium → virgl driver → virgl_iokit_winsys →
VMVirtIOGPUUserClient (0x6000-0x6009) → virtio-gpu PCI → QEMU →
virglrenderer → ANGLE → Metal → GPU

### Test

`cgl-shim/killtest/virgl_clear_test.c` — literally the same source as
`osmesa_softpipe_test.c`. The only variable is `GALLIUM_DRIVER` env var.
Run on slqemu 2026-08-10 02:17 EDT with `DYLD_LIBRARY_PATH=/tmp`:

```
GALLIUM_DRIVER=softpipe → PASS (reference, both clears byte-exact)
GALLIUM_DRIVER=virgl    → PASS (milestone, both clears byte-exact)
```

Both clears returned identical results — the bisect tool works exactly
as designed (LEDGER: bisect principle).

### Bug found during Increment C (cmd_buf overflow)

`submitCommand` had **two** 256-byte limits on command size:

1. `m_cmd_buf` allocated at 256 bytes (line 1876). A 256×256 BGRA
   resource produces 32 scatter-list entries → 392-byte ATTACH_BACKING
   command. `memcpy` into the 256-byte buffer silently overflowed,
   corrupting heap memory and producing a garbled command that the
   device couldn't process. **Fixed: 256 → 4096** (covers resources
   up to ~340 pages = 1.3 MB).

2. `submitCommand`'s parameter validation at line 1965 rejected
   `cmd_size > 256` with `kIOReturnBadArgument`. Even after the
   buffer was enlarged, this gate silently rejected the command
   before it reached the virtqueue. **Fixed: 256 → 4096**, matching
   the buffer capacity. Added a defensive overflow check before the
   `memcpy` that returns `kIOReturnNoMemory` rather than corrupting.

The probe_winsys_selectors_test (Increment A) didn't catch this
because its 64×64 resource produced only 5 segments (68-byte command,
well within 256). Mesa's 256×256 resource produces 30+ segments
(392-byte command, over 256). The probe's resource size was chosen
for minimal allocation, not for exercising the scatter-list capacity
limit — a known gap in the probe's coverage that the first real
Mesa workload exposed.

### What this closes

This is the structural milestone. Everything downstream — cgl-shim
(CGL entry-point interception), presentation (OSMesa buffer → NSView
backing store), real applications (Gecko, WebKit) — is plumbing on
top of proven primitives. The 3D acceleration path from guest OpenGL
calls to host GPU pixels is verified end-to-end.

---

## virgl_iokit_winsys selectors (Increment A) — VERIFIED — 2026-08-10

The winsys selectors (0x6000-0x6009) work end-to-end. `probe/probe_winsys_selectors_test`
on the SL guest drives CTX_CREATE → RESOURCE_CREATE_3D → ATTACH_BACKING_USER →
CTX_ATTACH_RESOURCE → SUBMIT_3D (CREATE_OBJECT + SET_FB + CLEAR + NOP) →
TRANSFER_FROM_HOST_3D, with no Mesa in the loop. Both clear colors return
byte-exact:

| Round | glClearColor (RGBA)       | Expected packed RGBA | Got (per dword) | Result |
|-------|---------------------------|----------------------|-----------------|--------|
| 0     | (0.20, 0.40, 0.60, 1.00) | 0xff996633           | 0xff996633 ×4096| PASS   |
| 1     | (0.80, 0.20, 0.40, 1.00) | 0xff6633cc           | 0xff6633cc ×4096| PASS   |

Reproducible across two consecutive runs (ctx_id=0x100, then 0x101 — both
PASS). The unaligned malloc produced 5 segments per the ATTACH_BACKING
probe's pattern; nr_entries ≥ 2 exit criterion met.

### Bugs found during Increment A (recorded so they don't recur)

1. **VIRGL_OBJECT_SURFACE = 9 (wrong) → 8 (correct).** The virgl_object_type
   enum counts from `VIRGL_OBJECT_NULL=0`: NULL, BLEND, RASTERIZER, DSA,
   SHADER, VERTEX_ELEMENTS, SAMPLER_VIEW, SAMPLER_STATE, **SURFACE=8**.
   The probe binary inlined a wrong value; the kext's `FB/virgl_protocol.h`
   had it right. virglrenderer returned `Illegal command buffer 329985`
   (= `VIRGL_CMD0(1, 9, 5)` = CREATE_OBJECT with obj=QUERY instead of
   SURFACE) — host-side only; kext saw `0x1100`.

2. **VIRGL_CMD0 macro bits.** Real macro is `((cmd) | ((obj) << 8) |
   ((len) << 16))`. The probe binary had obj<<16 and len<<24 (off by 8
   bits). Same symptom: `Illegal command buffer` from the host.

3. **Existing `attachVirglResource` (selector 0x3003) is a stub.** "For
   now, just log success" — never sends CTX_ATTACH_RESOURCE. virglrenderer
   requires CTX_ATTACH_RESOURCE before SET_FRAMEBUFFER_STATE can reference
   a surface built on the resource. Added new selector 0x6009
   (`ctxAttachResource`) that actually sends the command.

4. **submitVirglCommandsEx descriptor construction.** Original version used
   `IOMemoryDescriptor::withAddress` (alias userspace pointer through
   structureInput). Switched to `IOBufferMemoryDescriptor::withBytes`
   (copy bytes into fresh kernel buffer) to match probeTransport3D's
   proven pattern at `VMVirtIOGPU.cpp:3866`.

### General rule: 0x1100 means "QEMU parsed it", never "host accepted it"

Three known instances of QEMU returning `VIRTIO_GPU_RESP_OK_NODATA` (0x1100)
to the guest regardless of host-side outcome:

- **SUBMIT_3D** — `qemu/hw/display/virtio-gpu-virgl.c` `virgl_cmd_submit_3d`
  hands the buffer to `virgl_renderer_submit_cmd` and unconditionally
  responds 0x1100. Decode errors go to the host log only.
- **VIRTIO_GPU_FILL_CMD size mismatches** — host logs `command size
  incorrect N vs M` via `qemu_log_mask(LOG_GUEST_ERROR)`, returns OK_NODATA
  without ever calling virglrenderer. The box-vs-rect bug hid here.
- **RESOURCE_CREATE_3D** — `virgl_renderer_resource_create_3d` returns
  `EINVAL` on bad params; QEMU's `virgl_cmd_resource_create_3d` discards
  the return value and responds 0x1100 anyway.

The rule is stronger than any individual case: **on virgl-backed commands,
0x1100 means QEMU successfully parsed the wire command and routed it to
virglrenderer. It says nothing about whether virglrenderer accepted it.
Only readback proves acceptance.** The kext cannot rely on response codes
alone — the UTM host debug log (`<VM>.utm/Data/debug.log`) is a REQUIRED
artifact for diagnosing any virgl-backed command failure, not a fallback.

This rule subsumes the older "SUBMIT_3D always returns 0x1100" note that
lived in the architecture doc. Both the architecture doc and any new kext
code that handles virgl-backed commands should reference this rule rather
than enumerating individual commands.

**Caveat for Increment B (recorded so the wire bytes aren't misused):**
the working RESOURCE_CREATE_3D bytes captured below are a diff target
*only* for the silent-rejection failure mode — where the winsys sends
the command, sees 0x1100, but the resource was never created (and the
next CREATE_OBJECT fails with "Illegal resource" in the host log). If
Mesa's `resource_create` fails loudly instead (returns NULL before
sending, or `get_caps` rejects a format/binding combination), the wire
bytes aren't the place to look — the Mesa-side error is. Diff the bytes
when the symptom matches Increment A's; otherwise start at Mesa.

### Kext allocator partition (refined)

Three-way partition as planned:
- `m_next_resource_id` (existing, starts at 1) — display path, WebGL eager
  init, kext-internal allocations. Untouched.
- `m_next_user_resource_id` (new, starts at 0x100) — winsys-driven
  allocations via selector 0x6002 only.
- 0xFFF8-0xFFFF — probe sentinels. Hardcoded, never allocated.

Context IDs use a function-local static counter in `createVirglContextEx`
(starts at 0x100). For first-slice single-process use this is fine;
multi-process would need the counter moved to a per-device field. Wrap
check: 0x100 → 0xFFF8 gives ~65k contexts before sentinel collision.
Sufficient for the first slice; flagged as finished-winsys concern.

### CTX_ATTACH_RESOURCE — unconditional in the first slice, watch at volume

Increment A proved CTX_ATTACH_RESOURCE is **required** before
SET_FRAMEBUFFER_STATE can reference a surface built on a resource
(selector 0x6009 sends it for real; the legacy 0x3003 is a stub).
The winsys's `resource_create` should call 0x6009 unconditionally for
the first slice — the cost is one extra selector call per resource,
and Mesa's resource creation flow doesn't know whether a resource will
be context-attached when `resource_create` runs.

**Watch point for real Mesa workloads:** Mesa creates resources that
never get context-attached (staging buffers, texture-only resources
that get sampled but not rendered to). Attaching every resource to
the context unconditionally is correct for a single render target;
whether virglrenderer cares about extraneous attachments at volume
is unknown. If it does, gate the call: attach conditionally based on
`bind` flags (e.g., `VIRGL_BIND_RENDER_TARGET` → attach; sampler-only
→ skip), or attach lazily on first `emit_res` into a command buffer.
The winsys's `resource_create` is where to make that decision once
real workloads reveal whether it matters.

### Wire bytes (from kext hex dump, 2026-08-10 01:15:34 EDT, ctx=0x100)

**CTX_CREATE** (sizeof=96, hdr + nlen + context_init + debug_name[64]):
```
[0]=0x00000200  [1]=0x00000002  [2]=0x00000000  [3]=0x00000000
[4]=0x00000100  [5]=0x00000000  [6]=0x00000000  [7]=0x00000000
```
(type=CTX_CREATE, ctx_id=0x100 at dword[4], nlen=0, context_init=0)

**RESOURCE_CREATE_3D** (sizeof=72, full struct):
```
[0]=0x00000204  [1]=0x00000000  [2]=0x00000000  [3]=0x00000000
[4]=0x00000100  [5]=0x00000000  [6]=0x00000100  [7]=0x00000002
[8]=0x00000043  [9]=0x00000002 [10]=0x00000040 [11]=0x00000040
[12]=0x00000001 [13]=0x00000001 [14]=0x00000000 [15]=0x00000000
[16]=0x00000000 [17]=0x00000000
```
(type=RESOURCE_CREATE_3D, hdr.ctx_id=0x100 at dword[4], resource_id=0x100
at dword[6], target=2, format=67, bind=2, w=64, h=64, **depth=1 at
dword[12], array_size=1 at dword[13]**, last_level/nr_samples/flags/padding
all 0)

**SUBMIT_3D payload** (20 dwords, ctx=0x100, color1):
```
[0]=0x00050801  [1]=0x00000001  [2]=0x00000100  [3]=0x00000043
[4]=0x00000000  [5]=0x00000000  [6]=0x00030005  [7]=0x00000001
[8]=0x00000000  [9]=0x00000001 [10]=0x00080007 [11]=0x00000004
[12]=0x3e4ccccd [13]=0x3ecccccd [14]=0x3f19999a [15]=0x3f800000
[16]=0x00000000 [17]=0x00000000 [18]=0x00000000 [19]=0x00000000
```
(CREATE_OBJECT(SURFACE, handle=1, res=0x100, fmt=67) + SET_FB(cbuf=1) +
CLEAR(0x04, RGBA=0.20/0.40/0.60/1.00) + NOP)

### Artifacts on disk

- `FB/VMVirtIOGPU.h` — added `user_backing_entry` struct, `MAX_USER_BACKINGS=64`
  array on `VMVirtIOGPUUserClient`, `m_next_user_resource_id` field on
  `VMVirtIOGPU`, `allocateUserResourceId()` accessor, 10 new method
  declarations (createVirglContextEx … ctxAttachResource).
- `FB/VMVirtIOGPU.cpp` — 10 new selector implementations (0x6000-0x6009)
  + dispatch cases, per-client backing table helpers (findUserBacking,
  addUserBacking, removeUserBacking, removeAllUserBackings), 0x3009
  diagnostic logging, CTX_CREATE and RESOURCE_CREATE_3D hex dumps
  (gated behind the same IOLog discipline — should quiet down once
  Increment B is exercised, can be removed then).
- `FB/virtio_gpu.h` — added 6 compile-time size assertions at EOF
  (resource_create_3d, ctx_create, ctx_destroy, ctx_resource,
  get_capset_info, get_capset). Same class of bug as box-vs-rect; makes
  struct-size drift unrepresentable.
- `probe/probe_winsys_selectors_test.c` — userspace driver for all 10
  selectors.

### Exit criterion for Increment A — MET

- Both clear colors return byte-exact (4096/4096 dwords each round) ✓
- `nr_entries ≥ 2` for the unaligned 16 KB malloc (got 5) ✓
- Host debug log shows no virglrenderer errors for the passing runs ✓
- Reproducible across two consecutive runs ✓

Increment B (Mesa winsys implementation) can start.

---

## ATTACH_BACKING-with-userspace-memory probe — VERIFIED — 2026-08-10

The one structural unknown before the IOKit winsys (LEDGER.md:769) is
closed. `IOMemoryDescriptor::withAddressRange` + persistent `prepare()`
works on 10.6 for userspace `malloc`'d memory. Scatter list is correct,
host reads/writes through it correctly, and the descriptor stays wired
across two separate external-method calls. **Winsys = bookkeeping on
proven transport.**

### Test

`probe/probe_attach_backing_test.c`, cross-compiled for
`x86_64-apple-macos10.6`. Opens `VMQemuVGAAccelerator` (NOT
`VMVirtIOGPUAccelerator` — see "Service-name finding" below) with
IOServiceOpen type=4, calls selector 0x5000 twice:

- **Phase 1:** kext does CTX_CREATE → RESOURCE_CREATE_3D →
  CTX_ATTACH_RESOURCE → `withAddressRange(addr, len, kIODirectionInOut,
  m_owning_task)` → `prepare()` → inline ATTACH_BACKING (no complete —
  see "Constraint 3 amendment") → TRANSFER_TO_HOST_3D. Descriptor held
  prepared across the call return.
- **Phase 2:** TRANSFER_FROM_HOST_3D (host writes through the still-wired
  scatter list) → `complete()` → release descriptor → RESOURCE_UNREF →
  CTX_DESTROY.

Userspace fills `((uint32_t*)buf)[i] = i ^ 0xA5A5A5A5` before Phase 1,
zeroes buf to `0xCD` between phases, verifies every dword after Phase 2.

### Result (raw values from the run, 22:55 EDT)

- **Userspace:** `base=0x10082a800`, `buf=0x10082a811` (offset 17 from
  malloc base — non-page-aligned, non-dword-aligned), `len=16384`, 4096
  dwords.
- **Kext captured task:** `0xffffff800cc8d780` (matches `m_owning_task`
  captured at initWithTask, NOT `current_task()` — constraint 1
  satisfied).
- **Walked 5 segments totalling 16384 bytes** (matches descriptor
  `getLength()` exactly — no walk mismatch):
  - `seg[0] addr=0x64662811 len=2031` — partial first page (offset 17
    into page 0x64662000, ends at 0x64663000; 0x64663000-0x64662811 = 0x7EF = 2031 ✓)
  - `seg[1] addr=0x52c63000 len=4096` — full page
  - `seg[2] addr=0x653e4000 len=4096` — full page
  - `seg[3] addr=0x64fe5000 len=4096` — full page
  - `seg[4] addr=0x65c66000 len=2065` — partial last page (16384 - 2031 - 4096×3 = 2065 ✓)
- **Response codes (all real signals):**
  - CTX_CREATE: `0x1100`
  - RESOURCE_CREATE_3D: `0x1100`
  - CTX_ATTACH_RESOURCE: `0x1100`
  - ATTACH_BACKING: `0x1100`
  - TRANSFER_TO_HOST_3D: `0x1100`
  - TRANSFER_FROM_HOST_3D: `0x1100`
- **Userspace readback:** **4096/4096 dwords match `i ^ 0xA5A5A5A5`**.
  Zero mismatches. No `0xCDCDCDCD` (host wrote zero bytes) and no partial
  corruption (pages moved between phases).

The 5-segment scatter list is the *real* case — `malloc(16 KB + 128)`
with offset-17 probe deliberately produces a partial first page, three
discontiguous full pages, and a partial last page. This is exactly the
shape Mesa's `align_malloc(size, 64)` will produce in the winsys. A
single-segment result would have tested only the easy case.

### Constraint 3 amendment — verified

`VMVirtIOGPU::attachBacking()` at `VMVirtIOGPU.cpp:7303` calls
`backing_memory->complete()` at line 7403 right after ATTACH_BACKING.
This is correct for the display path's `IOBufferMemoryDescriptor`
(permanent kernel allocation; physical addresses don't relocate) but
**wrong for userspace `malloc`'d memory** — once `complete()` unwires
the descriptor, the pages can be paged out or relocated, and the host's
stored scatter-list addresses become stale. Mesa's pattern (CPU writes
to buf between transfers) requires the descriptor to stay prepared
across the whole resource lifetime (LEDGER.md:799 constraint 3).

The probe deliberately inlines the attach logic and skips `complete()`
until Phase 2 teardown. Zero blast radius on the working display path.
The duplicated segment walk is flagged as TEMPORARY in code comments —
the winsys will have its own attach helper that doesn't complete, at
which point both should consolidate. **Do not consolidate before the
winsys shape is known** — refactoring attachBacking now expands blast
radius for no benefit, and whether the winsys wants attach/complete
split or paired with resource lifetime is an open design question that
consolidation would pre-empt.

### What the probe also proved (stronger than the pre-registered claim)

The pre-registered PASS criterion was "every dword matches the pattern
after Phase 2." The probe exercised more than that. The
`memset(buf, 0xCD)` between Phase 1 and Phase 2 is a **guest CPU write
into the prepared descriptor's pages**, followed by Phase 2's host
write-back through the same scatter list — which is exactly Mesa's
usage pattern (CPU writes vertex/texture data into the resource between
transfers, host reads/writes through the scatter list during
TRANSFER_TO/FROM_HOST_3D). So the probe tested guest-write-then-host-
write over persistent wiring, not just attach-and-read. That was the
part expected to remain unproven (LEDGER.md:814 "known limits") and
it held. The "single small buffer in a quiet guest" caveat still
applies to memory *pressure* (many live resources, sustained stress),
but the basic CPU-write-during-prepared pattern is now proven.

### What this closes

1. `withAddressRange` works on 10.6 for userspace memory using the
   `m_owning_task` captured at `initWithTask` (constraint 1 ✓).
2. `prepare()` produces valid physical addresses for the scatter list;
   host reads/writes through them correctly.
3. The descriptor stays prepared across multiple external method calls —
   persistent wiring holds (constraint 3 ✓).
4. The 3D transport proven by `probeTransport3D` (LEDGER.md:202)
   extends to userspace-driven ATTACH_BACKING + transfers, not just the
   kext-internal kernel-memory path.
5. The IOKit winsys can use **vtest model**: `malloc` backing in
   userspace → `withAddressRange` per resource → `prepare` at attach →
   `complete` at resource teardown. No `clientMemoryForType` /
   `IOConnectMapMemory` / kext-allocated backing needed.

### What this does NOT close (known limits)

- **Wiring under memory pressure.** A single 16 KB buffer in a quiet
  guest succeeds whether or not the wiring is correct in a stronger
  sense, because nothing is putting pressure on those pages. The probe
  establishes the mechanism; it does NOT prove the wiring holds for the
  resource's lifetime under sustained memory pressure. Confirming the
  latter needs many live resources or a memory-pressure test, which is
  a property of the finished winsys, not something to chase now
  (LEDGER.md:814).
- **Fences.** Probe uses synchronous submission. The winsys's
  `resource_wait` / `resource_is_busy` are trivial today only because
  `submit_cmd` polls synchronously — when fences become real, these
  vtable entries stop being trivial (LEDGER.md:109).

### Service-name finding

`IOServiceMatching("VMVirtIOGPUAccelerator")` returns NULL on this
configuration — the published service is the **base class**
`VMQemuVGAAccelerator` (visible in ioreg with id 0x1000002b8).
`VMVirtIOGPUAccelerator` exists in code (VMVirtIOGPU.cpp:5948) and
calls `registerService()` at line 6054, but registers 0 instances on
this boot. The base class's `newUserClient` at
`VMQemuVGAAccelerator.cpp:344` handles type=4 and returns
`VMVirtIOGPUUserClient` — that's the path the probe uses.

### Boot-stall false alarm

After install, the guest appeared stuck at
`VMVirtIOFramebuffer::start() - Initialization complete`. Cause was
slow boot from the no-caches development configuration (rules: "Running
with the caches deleted is a perfectly good development configuration
— the kernel loads kexts individually from /S/L/E. Boot is slower"),
not a kext regression. The next reboot reached `enableController` and
ran all three probes (`probeResourceTracking`, `probeResourceRecreate`,
`probeTransport3D`) — all PASS. Pre-existing probes unaffected by the
new selector.

### Artifacts on disk

- `FB/VMVirtIOGPU.h` — added 4 fields + 2 method decls to
  `VMVirtIOGPUUserClient`.
- `FB/VMVirtIOGPU.cpp` — `initWithTask`/`clientClose`/`free` updated;
  new `case 0x5000` in `externalMethod`; `probeAttachBackingUser` +
  `probeAttachBackingUserCleanup` implemented.
- `probe/probe_attach_backing_test.c` — userspace test binary.
- `probe/install_and_reboot.sh` — guest-side install script.

The probe selector is harmless to leave in-tree — only fires on
explicit selector 0x5000 from userspace. Useful as a future regression
check for the winsys foundation.

---

## Winsys pre-registrations — 2026-08-10

Recorded before the winsys work starts, so the first results get read
correctly rather than re-derived under time pressure. None of these
are verified yet — they are pre-registered predictions and design
constraints, the same discipline as the ATTACH_BACKING probe.

**Bisect tool is what softpipe bought — state as principle, not step.**
The softpipe validation (LEDGER.md:184) was worth doing standalone
*because softpipe is the bisect tool*, not because softpipe is the
goal. A wrong result from `GALLIUM_DRIVER=virgl` with softpipe correct
IN THE SAME BINARY means the guest GL stack is fine and the fault is
in the winsys or the host. Three candidates (guest GL / winsys /
host) reduced to one with one env-var flip. Lead with this framing in
any winsys bug investigation; don't treat it as a one-time check.

**Most-likely first Mesa-driven result: clear returns wrong colour,
not nothing.** Pre-registered because "wrong colour" reads like a
worse failure than "nothing" but is actually more informative:
- **Wrong colour** = the command reached virglrenderer and executed;
  the encoding is off. Guest-side diagnosis possible.
- **Nothing at all** (buffer stays at the initial fill) = the command
  buffer never got there, or was rejected during decode. Invisible
  from the guest — requires the UTM host debug log (per the build
  rules, only written when *Debug Log* is enabled in the VM settings).

Different diagnoses, different next steps. Reading the first result
correctly depends on having this bifurcation written down in advance.

**`get_caps` must hit a real `GET_CAPSET`, not return known sizes.**
The capset sizes are known (VIRGL=308, VIRGL2=1408) but Mesa reads
the **contents** of the blob to decide which GL version and feature
set to expose. A wrong or truncated capset produces failures far from
their cause — wrong GLSL version, missing texture formats, silent
feature downgrades. Probe `probeTransport3D`'s `GET_CAPSET` path is
already proven (LEDGER.md:107), so the mechanism works; the winsys
must use that path, not hardcode a known-size shortcut.

**`submit_cmd` stays synchronous; fence stubs depend on that.** The
fence vtable stubs (`cs_create_fence = NULL`,
`fence_server_sync = NULL`, `supports_fences = 0` — LEDGER.md:122)
are only valid *because `submit_cmd` polls synchronously today*. That
dependency is recorded in the vtable scope notes but is easy to
forget once the vtable is filled in. If `submit_cmd` later gains
async/fence support, the fence stubs become a real bug rather than a
safe shortcut — re-evaluate them as a group at that point, not
individually. The synchronous-poll property is a property of the
transport, not of the vtable (LEDGER.md:109).

**Command buffers travel by SUBMIT_3D, not ATTACH_BACKING.** The
existing `sSubmit3DCommands` selector at `VMQemuVGA3DUserClient.cpp:39`
accepts the buffer as `structureInputDescriptor` — IOKit auto-creates
the descriptor from userspace memory per-call. So `cmd_buf_create`
really is `malloc` and the winsys's `emit_res` is patching a resource
handle into the malloc'd buffer; no attach-backing dance for command
streams. Only pixel/texture/render-target resources need the attach
path the ATTACH_BACKING probe just proved.

**Next milestone stated precisely.** Same clear-and-readback shape as
`probeTransport3D` (LEDGER.md:202) and the ATTACH_BACKING probe, but
with `virgl_iokit_winsys` as the source of commands instead of the
kext. `GALLIUM_DRIVER=virgl` + `glClear` + `glReadPixels` returning
the right colour. With softpipe as the in-binary reference, a wrong
result immediately bisects (see bisect-tool principle above).

---

## Mesa softpipe verified on 10.6 + IOKit winsys scope — 2026-08-10

### Softpipe render test — VERIFIED (byte-exact, negative-control-confirmed)

The 19 MB libOSMesa.8.dylib (Mesa 24.3.0-devel, cross-compiled for
x86_64-apple-macos10.6 on the `cross-10.6` branch of startergo/Mesa-VirGL)
was loaded by 10.6.8's dyld and produced byte-exact pixels on two
known-color clears. **"Links clean" finally meant "works" on the fourth
attempt.**

**Test:** `osmesa_softpipe_test` — cross-compiled C binary, deployed to
the SL guest via scp alongside libOSMesa.8.dylib + libglapi.0.dylib.
Run with `DYLD_LIBRARY_PATH=/tmp GALLIUM_DRIVER=softpipe`.

**Method:** OSMesaCreateContextExt(OSMESA_BGRA, 16, 0, 0, NULL) →
MakeCurrent into a 256×256 malloc'd buffer → glClearColor + glClear +
glFinish → read back first AND last pixel (rules out partial-buffer
artefacts). Two clears with non-boundary colors per the LEDGER's
rounding convention:

| Clear | glClearColor (RGBA)      | Expected RGB  | Got (first/last)  | Result |
|-------|--------------------------|---------------|-------------------|--------|
| A     | (0.20, 0.40, 0.60, 1.0) | (51, 102, 153)| (51,102,153) ×2  | PASS   |
| B     | (0.80, 0.20, 0.40, 1.0) | (204, 51, 102)| (204,51,102) ×2  | PASS   |

Tolerance: ±1 ULP per GL spec on float-to-unorm. Both clears were
exact (zero ULP delta). The ±1 guard was quiet.

**What this closes:**
1. dyld loads the 19 MB dylib — all link-time deps resolve (libc++
5.0.1 reexport, clock_gettime/strndup/open_memstream compat shims,
dri_stubs, vl_video_stubs, DRM stub headers).
2. TLS gate survives a running process — `u_thread.h`'s
`__THREAD_INITIAL_EXEC` downgrade (plain globals on 10.6) works
through context creation, rendering, and teardown.
3. softpipe produces correct pixels — the Gallium software rasterizer
is fully functional on 10.6.
4. `inline_sw_helper.h` fix works — `GALLIUM_DRIVER=softpipe` selects
softpipe (was previously broken by a local patch that called
`virgl_create_screen` unconditionally; fixed in Mesa-VirGL commit
`85da3f4cf8c` which restored upstream `GALLIUM_VIRGL`/`GALLIUM_SOFTPIPE`
conditionals and added `-DGALLIUM_VIRGL` to the cross file's c_args so
OSMesa still includes virgl when built with `-Dgallium-drivers=virgl`).

**softpipe is now the known-good reference renderer.** When virgl
returns wrong pixels through the IOKit winsys, one env-var change
(`GALLIUM_DRIVER=softpipe` vs `virpipe`) in the same binary, same
guest, same OSMesa entry points isolates "Mesa GL stack correct" from
"transport/winsys correct." This is the bisect tool vtest would have
provided, without vtest's external dependencies.

### vtest server availability (banked, not plumbed)

`virgl_test_server` is installed at
`/opt/homebrew/opt/virglrenderer-3dfx/bin/virgl_test_server` (Homebrew
tap `virglrenderer-3dfx`, built with ANGLE for GLES, libepoxy for GL,
same renderer stack as UTM). Verified 2026-08-10: starts cleanly with
`--no-fork --use-gles`, binds `/tmp/.virgl_test` UNIX socket, zero
errors. Capsets should be closely comparable to UTM's embedded
virglrenderer since the same renderer backend is used.

**Not plumbed.** Mesa's vtest winsys is AF_UNIX only — upstream
`virgl_vtest_winsys.c` connects to a Unix socket path with no TCP
mode. To reach the host's vtest server from the guest, run socat in
the guest: `socat UNIX-LISTEN:/tmp/.virgl_test,fork TCP:10.0.2.2:6660`
(10.0.2.2 = QEMU user-net gateway, hostfwd on host side). Don't build
this plumbing until a bisect requires it — softpipe is the cheaper
bisect tool for most questions.

### IOKit winsys vtable scope — research-complete

The shipping GL path is a third winsys: `virgl_iokit_winsys`,
implementing Mesa's `struct virgl_winsys` vtable
(`src/gallium/drivers/virgl/virgl_winsys.h`) against the kext's
IOUserClient. Neither of Mesa's existing winsys works: DRM needs
`/dev/dri/*` (no DRM on macOS, stubs abort by design), vtest talks to
a standalone host process (validation harness, not the UTM path).
UTM's virglrenderer is embedded and reachable only through virtio-gpu,
so the kext winsys is the path.

**Vtable classification (25 entries):**

| Category | Count | Entries |
|----------|-------|---------|
| PROVEN (probeTransport3D) | 6 | `transfer_put` (TRANSFER_TO_HOST_3D), `transfer_get` (TRANSFER_FROM_HOST_3D), `resource_create` (RESOURCE_CREATE_3D), `submit_cmd` (SUBMIT_3D), `get_caps` (GET_CAPSET), `destroy` (teardown) |
| Bookkeeping — unconditional | 6 | `resource_reference` (refcount), `resource_get_storage_size` (return size), `cmd_buf_create` (malloc), `cmd_buf_destroy` (free), `emit_res` (patch resource handle into cmd buf), `fence_reference` (refcount on dummy) |
| Bookkeeping — sync-dependent | 3 | `resource_wait`, `resource_is_busy`, `res_is_referenced` — only trivial because `submit_cmd` polls synchronously (no fences). **This stops holding when fences become real.** The synchronous-poll property is a property of the transport, not of the vtable. |
| Trivial (vtest model) | 1 | `resource_map` — return local malloc'd pointer (vtest winsys does exactly this at `virgl_vtest_winsys.c:338-340`; does NOT mmap kernel memory) |
| Stub (NULL-checked by callers) | 8 | `resource_create_from_handle` (return NULL), `resource_set_type` (no-op), `resource_get_handle` (return false), `cs_create_fence` (NULL ptr — safe, see below), `fence_wait` (return true), `fence_server_sync` (NULL ptr), `flush_frontbuffer` (no-op for OSMesa), `fence_get_fd` (return -1) |
| Trivial | 1 | `get_fd` (return -1; no DRM fd, IOKit uses IOConnect) |
| PROVEN + gap | 1 | `resource_create` — RESOURCE_CREATE_3D is proven, but **ATTACH_BACKING with userspace memory** is unproven (see Open below) |

**Fence safety — verified in source, not inferred.** Upstream virgl
NULL-checks every fence function pointer before calling:
- `virgl_context.c:1412`: `if (rs->vws->cs_create_fence)` — skip if NULL
- `virgl_context.c:1422`: `if (rs->vws->fence_server_sync)` — skip if NULL
- vtest winsys sets `supports_fences = 0`
  (`virgl_vtest_winsys.c:738`) and still provides `fence_wait` /
  `fence_reference` because the flush path calls them unconditionally.
The IOKit winsys follows the same pattern: `cs_create_fence = NULL`,
`fence_server_sync = NULL`, `supports_fences = 0`.

**resource_map is NOT the gap the DRM framing suggested.** The vtest
winsys mallocs resource backing in userspace and returns that pointer
from `resource_map`. No mmap of kernel memory, no
`clientMemoryForType`, no `IOConnectMapMemory`. The IOKit winsys
follows this model: malloc backing → return pointer → sync via
TRANSFER_TO_HOST_3D / TRANSFER_FROM_HOST_3D (both proven).

### Kext infrastructure for the winsys

The kext already has most of what the winsys needs:

1. **`attachBacking()` is memory-descriptor-agnostic**
   (`VMVirtIOGPU.cpp:7303-7412`). Takes any `IOMemoryDescriptor*`,
   walks `getPhysicalSegment()` for the scatter list. Works on kernel
   or userspace memory equally.

2. **3D UserClient dispatch table exists** (`VMQemuVGA3DUserClient.cpp:126-141`)
   with `kVM3DUserClientSubmit3DCommands` etc. Adding a new selector
   for ATTACH_BACKING-with-userspace-memory is a straightforward
   extension.

3. **Command submission from userspace works**
   (`sSubmit3DCommands` receives `IOMemoryDescriptor*` from userspace,
   maps it, forwards to `executeCommands`).

4. **`clientMemoryForType` exists** but only returns VRAM (for
   WindowServer framebuffer mapping). Not needed for GL resources
   under the vtest model, but the plumbing is there if the
   kext-allocated model becomes needed later.

### `withAddressRange` precedent — DEAD CODE

`IOMemoryDescriptor::withAddressRange` appears in
`VMQemuVGAAccelerator.cpp:1189` (texture upload path). However, this
is inside `VMQemuVGAAccelerator::createTexture` — part of the
superseded 3D managers (`VMTextureManager`/`VMShaderManager`), which
the LEDGER already flags as "nothing on the display path consumes any
of it." The GLPlugin direction is superseded. **This `withAddressRange`
has never been called from a live path — evidence the compiler accepts
the API, not that it works at runtime.** The ATTACH_BACKING probe
below is genuinely needed.

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

### Debug log analysis — UTM host log 2026-08-09 — 2026-08-10

Reviewed the UTM host debug log (1.2M lines, boots from
16:45 to 19:32). Three signals found:

**1. SET_SCANOUT negative control fires correctly (4×).**

`virtio_gpu_virgl_process_cmd: ctrl 0x103, error 0x1203` — ctrl 0x103
is `SET_SCANOUT` (not RESOURCE_UNREF which is 0x102). This is the
negative control at `VMVirtIOFramebuffer.cpp:1544`
(`setscanout(0, 999, ...)`), doing exactly what the CLAUDE.md ground
rules require: proving the error path returns `ERR_INVALID_RESOURCE_ID`
for a nonexistent resource. Four occurrences = two probes per boot (one
in `VMVirtIOGPU::start`-area, one in `enableController`). Expected,
deliberate.

**2. `command size incorrect` absent — independent host-side
confirmation of box-vs-rect fix.**

`LOG_GUEST_ERROR` IS captured in this log (proven by the 0x1203 lines
themselves, which go through that macro). The box-vs-rect bug would
have produced `command size incorrect 64 vs 72` via the same macro for
every `TRANSFER_FROM_HOST_3D` / `TRANSFER_TO_HOST_3D`. Its absence is
independent host-side confirmation that the struct fix landed and
transfer commands are now correctly sized. Positive result, not a
capture gap.

**3. `no gl-unblock` / `no gl-draw-done` — ≥1s ack stalls (19 pairs).**

`console: no gl-unblock within one second` + `spice: no gl-draw-done
within one second` — QEMU's console layer blocked waiting for
`graphic_hw_gl_flushed` from CocoaSpice, and CocoaSpice didn't call it
within QEMU's fixed 1-second timeout. QEMU force-unblocks and recovers.
The ≥1s is QEMU's watchdog floor, not a measurement of the actual stall
duration — the client could be 1.001s or indefinitely late.

19 pairs over ~2 hours at ~10 Hz frame rate is ~0.02% of frames —
occasional, self-recovering. Densest cluster: 7 pairs in 4 minutes
(18:48–18:52). Starting at 17:58, ~12 minutes after boot.

**Untested hypothesis: window occlusion / backgrounding.** The burst
pattern (long stretches clean, then a cluster, then clean again) fits
macOS Metal layer throttling under occlusion better than load — nothing
about the guest changed at 18:48. A Metal layer that isn't visible
(UTM backgrounded, window occluded, host display dimming) will stall
`nextDrawable`, and a renderer that signals draw-done after acquiring
a drawable would fail exactly this way. Cheap to falsify: one run with
UTM frontmost and untouched, one with it backgrounded behind another
app for a few minutes. Not tested yet — log says what, not why.

**For UTM issue filing:** the cursor-overlay bug is the report —
deterministic, every boot, clean QXL A/B on the same host, QEMU-side
proof the pixels arrived. The draw-done stalls are supporting evidence
in a subordinate section (GL scanout path has ack/composite gaps),
not a co-equal symptom. Leading with two symptoms of comparable weight
invites a maintainer to fix the easy one and close the issue.

---

**Mesa builds for 10.6 with mechanical patching.** Investigation on the
`cross-10.6` branch in the `Mesa-VirGL` repo (startergo/Mesa-VirGL,
based on alexvorxx's fork, Mesa 24.3.0-devel). Cross-compiled on Apple
Silicon targeting x86_64-apple-macos10.6 using:

- 10.6 SDK at `/Applications/Xcode.app/.../MacOSX10.6.sdk`
- libcxx 5.0.1 from `leopard-webkit-build/dist/libcxx/`
- llvm-ar from Homebrew LLVM for correct Mach-O archive format
- meson cross file at `cross-compat/mesa-cross-10.6.txt`

**Build note:** the 913-target figure (with `-Dosmesa=true
-Dgallium-drivers=virgl,softpipe`) and the earlier 146-target figure
(virgl-only, no OSMesa) are different configurations, not a regression
or miscount. The 913-target build adds softpipe, OSMesa frontend,
OSMesa target, NIR compiler, GLSL compiler, Gallium auxiliary, and
their generated headers — all of which are prerequisites for a shared
library that pulls in the full driver stack.

**146/146 targets compiled and linked.** Build produces 11 static
libraries (including `libvirgl.a` — the virgl Gallium driver — and
`libvirglvtest.a` — the vtest winsys) plus 3 dynamic dispatch
libraries (`libglapi.0.dylib`, `libGLESv1_CM.1.dylib`,
`libGLESv2.2.dylib`). No `libGL.dylib` — expected, since GLX/EGL/GBM
are disabled and there is no CGL binding layer. The static libraries
are the complete Mesa implementation; they need a final link against
a CGL shim to produce a loadable GL library.

**Real link test: zero platform gaps.** Force-loaded the virgl driver
+ vtest winsys + common into the OSMesa shared library target to test
symbol resolution against 10.6's libSystem + libcxx 5.0.1 + compat
shims. **libOSMesa.8.dylib linked: 19 MB, zero undefined symbols.**
Every external symbol — C++ runtime (libc++ + libc++abi), NIR, GLSL
compiler, Gallium auxiliary, zlib, pthreads, math — resolves against
10.6 libSystem. **The link test question is answered: 10.6's platform
is compatible with Mesa's virgl build.**

Eight Mesa-internal symbols required stubs (not platform gaps):

**DRI config (3):** `driParseConfigFiles`, `driQueryOption{b,i}`.
Earlier attribution "excluded by shader-cache=disabled" was wrong.
Actual cause: xmlconfig.c was never compiled despite
`-Dxmlconfig=enabled` — meson's feature resolution for expat fails in
cross-compilation mode (pkg-config can't find expat in the 10.6
sysroot). Stubbed with safe defaults (no drirc.d on target; driconf
defaults are correct behavior, not fake success). NOT abort — these
are on the driver init path.

**Video buffer (5):** `vl_video_buffer_{create,destroy,
get_associated_data,is_format_supported,set_associated_data}`.
Earlier attribution "not linked into OSMesa target" was wrong. Actual
cause: `video-codecs=[]` disables the entire `src/gallium/auxiliary/vl/`
subsystem. `virgl_video.c` references vl functions unconditionally;
`virgl_context.c` references functions from `virgl_video.c`, so the
file can't be removed. Stubbed with abort() (unreachable video path on
this target — video decode irrelevant for Snow Leopard 3D).

DRM stub provenance: extracted from startergo/Mesa-VirGL fork's
`.github/workflows/macos.yml` at commits 1577651647d and 8681e0ec7d9.
Stub functions changed from return-success to abort() per the project's
fail-loud pattern.

**TLS gate behavioral risk:** the `u_thread.h` patch
(`__THREAD_INITIAL_EXEC` gated on macOS >= 10.7) is a **correctness
compromise**, not a portability shim. Falling back to plain globals for
what was thread-local in the GL dispatch path (`_glapi_tls_Dispatch` /
`_glapi_tls_Context` in `src/mapi/`) is correct only for single-threaded
GL usage. Mesa's GL dispatch is single-context-per-thread by design, so
it's likely fine for a first port — but it should be marked as such so
nobody later assumes it's safe under threading. A proper fix requires
either TLS runtime support (patched dyld) or `pthread_key_create` /
`pthread_getspecific` on the dispatch hot path (per-GL-call overhead
under TCG).

**Scoping caveat (verbatim):** this is "Mesa's OS layer is portable to
10.6 with mechanical patching," not "Mesa works on 10.6," and certainly
not "3D works." Two unknowns remain: the link (archive format fix is a
meson tooling issue, not a platform gap; symbol resolution is clean with
1 trivial gap) and the CGL shim (the other half of the userspace stack,
independent of whether Mesa compiles).

**Next step: OSMesa+softpipe render test in the guest.** The 19 MB
libOSMesa.8.dylib links clean but hasn't been loaded by 10.6.8's dyld.
Given this project's track record ("links clean" has meant "doesn't
work" twice), the cheapest validation is: scp the dylib to the guest,
run an OSMesa program with `GALLIUM_DRIVER=softpipe`, render a
known-color clear to a memory buffer, verify the pixels. Needs no host
server, no socket plumbing, no virglrenderer. Answers: does dyld load
it, does the TLS gate survive a running process, does the GL stack
produce correct output. Then virpipe against virgl_test_server for the
virgl-specific path (note: vtest uses a UNIX domain socket — guest
can't reach host without virtio-serial/vsock forwarding or TCP mode).

### Seam decision — Mesa + virgl, GLPlugin/ superseded — 2026-08-09

`GLPlugin/` marked superseded. The tree attempted option 1 from
`.claude/rules/acceleration.md` (replace `GLEngine.bundle`); strategic
direction moves to option 3 (libGL + CGL shim via
`DYLD_INSERT_LIBRARIES`) with Mesa's virgl Gallium driver as the GL
implementation. Rationale:

1. **Empirical**: CGL never discovered this renderer on either 10.6
   (`notes/SNOW_LEOPARD_CGL_ARCHITECTURE_FINDINGS.md`) or 10.15
   (`notes/CATALINA_CGL_RENDERER_DISCOVERY_ISSUE.md`). Even a working
   GLPlugin couldn't have been loaded by CGL.
2. **Strategic**: replacing `GLEngine.bundle` means writing OpenGL 2.1
   (the v3.0 list — command submission, texture upload, shader
   compilation, draw calls). Mesa already contains all of it.
3. **Methodological**: `IMPLEMENTATION_STATUS.md`'s success criteria
   were all unfalsifiable (absence of error; a stub returning
   `kCGLNoError` satisfies them) — the `submitCommand` pattern one
   layer up. And the doc self-contradicted (advertised
   `accelerated: 1` / `video_memory: 256 MB` / `OpenGL 2.1` while
   admitting rendering used Apple's software rasterizer). That's the
   `crsr = 1` failure class.

Salvageable as reference: GLI/CGL plumbing research (renderer
discovery, pixel format attribute parsing, the IOServiceOpen path) is
what a CGL shim for the Mesa direction needs. Tree kept per project
"superseded, not deleted" rule; banner-annotated at
`GLPlugin/SUPERSEDED.md`.

### ~~Open — kext still publishes IOAccelerator3D = Yes~~ — FIXED 2026-08-09 (fb669ac)

Surfaced 2026-08-09 via the GLPlugin review as the live-code instance of
the `crsr = 1` pattern. **Fixed in `fb669ac`** by introducing
`VMVirtIOGPU::m_3d_functional` (single flag, false by default) and
deriving every IOAccelerator3D / model / "3D Acceleration" publication
from it. See "Bogus IORegistry properties to retire during the merge"
above for the full list of sites touched and remaining category siblings.

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

- **ATTACH_BACKING with userspace memory — OPEN, the one structural
  unknown before the IOKit winsys.** The kext's `attachBacking()`
  accepts any `IOMemoryDescriptor*` and walks `getPhysicalSegment()` —
  works on kernel or userspace memory. But the only `withAddressRange`
  precedent in the codebase (`VMQemuVGAAccelerator.cpp:1189`) is dead
  code in the superseded 3D managers — never called from a live path.
  The probe is genuinely needed.

  **Must be a userspace-driven probe, not kernel-side.** A kernel-side
  probe using `IOMalloc` tests kernel memory, which is a different case
  and proves nothing about the one that matters. The probe must be
  driven from userspace via IOUserClient, using the owning task's
  address space.

  **Four design constraints (pre-registered):**

  1. **Task: use `initWithTask` capture, not `current_task()`.** If the
     external method is routed through a command gate onto the workloop,
     `current_task()` becomes the kernel task and `withAddressRange`
     silently describes the wrong address space. Store the owning task
     when the user client is created and pass that.

  2. **Buffer: deliberately unaligned, expect multi-segment scatter
     list.** Every `attachBacking` call so far has been page-aligned
     `IOBufferMemoryDescriptor` with `nr_entries=1` — practically an
     invariant. Mesa allocates with `align_malloc(size, 64)`, so the
     real case starts mid-page and spans many discontiguous physical
     segments. Use `malloc` (not `valloc`/`mmap`) in the test to hit
     the real scatter-list path.

  3. **Wiring: `prepare()` before `getPhysicalSegment()`, `complete()`
     at resource teardown — not after the transfer.** Mesa writes into
     the buffer between transfers; unwiring after each transfer would
     break the next operation.

  4. **Selectors: check existing before adding.** The user client
     already has handlers taking `scalarInput[8]` for ctx_id from the
     earlier fix; if something close is there, extending beats adding.

  **Probe shape:** malloc a buffer in userspace → write known pattern →
  `withAddressRange(addr, len, kIODirectionInOut, owningTask)` +
  `prepare()` → `attachBacking` via new/existing selector →
  TRANSFER_TO_HOST_3D → zero the buffer (negative control) →
  TRANSFER_FROM_HOST_3D → expect the pattern back.

  **What the probe proves (known limit):** a single small buffer in a
  quiet guest succeeds whether or not the wiring is correct, because
  nothing is putting pressure on those pages. The probe establishes
  the mechanism — that `withAddressRange` can describe userspace
  memory to the device on 10.6, that `prepare()`/`getPhysicalSegment()`
  produce valid physical addresses, and that the host can read/write
  through the scatter list. It does NOT prove that the wiring holds
  for the resource's lifetime under memory pressure. Confirming the
  latter needs many live resources or sustained memory pressure,
  which is a property of the finished winsys, not something to chase
  now.

  **Pre-registered prediction:** if `withAddressRange` + `prepare()`
  works on 10.6 for a malloc'd userspace buffer, the entire winsys is
  bookkeeping on top of proven transport. If 10.6's IOKit can't wire
  arbitrary userspace memory for virtio-gpu scatter-list use, the model
  needs adjustment (kext-allocated backing via
  `IOBufferMemoryDescriptor` + `clientMemoryForType`/`IOConnectMapMemory`
  — more IOKit plumbing, but already-wired kernel memory, no per-resource
  wiring pressure).

  **Diagnostic logging (pre-registered):** log `nr_entries` and
  per-segment `(addr, length)` from the scatter-list walk. An unaligned
  `malloc` should produce several segments with a partial first page.
  If `nr_entries == 1` on an unaligned buffer, either the allocator
  handed something page-aligned and contiguous by luck, or the segment
  walk is wrong — both make a pass meaningless and neither is visible
  from the pattern check alone.

  **Expected failure mode for wiring problems:** not an error code, but
  wrong bytes. `prepare()` failing returns a status you can check;
  pages moving underneath a correct-looking descriptor doesn't. A clean
  `nr_entries` plus a correct pattern is the pass. Partial corruption
  (some segments correct, others stale or zeroed) points at wiring, not
  at the protocol — the protocol is proven by `probeTransport3D`.

- **Hardware cursor — OPEN, unresolved whether GL scanout is the cause.**
  Queue 1 transport proven (used ring advances, PROBE PASS). Guest-side
  setup verified correct (64x64 BGRA, alpha=0xFF, scanout_id=0,
  TRANSFER_TO_HOST_2D before UPDATE_CURSOR, struct 56 bytes, resource
  persists - no teardown). QEMU accepted (no guest error in debug log).
  SPICE cursor channel delivered data to CocoaSpice
  (`set_cursor: type alpha(0), 0, 64x64`). **Cursor not visible on
  virtio-vga-gl.**

  **Boundary precisely stated:** cursor data is delivered to CocoaSpice
  via the SPICE cursor channel. CocoaSpice does not render it as a
  visible overlay. Whether it should is unresolved - the answer depends
  on SPICE mouse mode (see below).

  **QXL comparison - NOT a valid control.** QXL shows a visible hardware
  cursor on the same UTM host, but QXL uses a different display path
  entirely (no GL scanout). The difference could be the display path, or
  it could be mouse mode, or both. Using QXL as evidence that "GL scanout
  should composite a cursor" is a non sequitur - it differs in exactly
  the variable under test.

  **Reference client behaviour (spice-gtk):** `spice_egl_update_display`
  composites the cursor pixbuf on top of the GL scanout texture - after
  binding the scanout texture and drawing it, it uploads the cursor
  pixbuf as a texture and draws it on top. This IS gated on conditions:
  server mouse mode (`SPICE_MOUSE_MODE_SERVER`), valid guest coordinates,
  `!show_cursor`, pointer grabbed, non-NULL pixbuf. In client mouse mode,
  spice-gtk does NOT composite at all - it hides the guest cursor and
  sets the local window cursor from the cursor-channel bitmap, letting
  the window system draw it. Two different rendering strategies for the
  same cursor data, selected by mouse mode.

  **Which mouse mode is UTM in? - UNVERIFIED.** This is the cheapest
  next check and the one that determines the investigation direction:
  - If server mouse mode: the failure is in CocoaSpice's GL scanout
    path not compositing the cursor overlay (spice-gtk does, CocoaSpice
    apparently does not).
  - If client mouse mode: the failure could be in cursor-channel to
    NSCursor plumbing, nothing to do with GL scanout at all.

  Readable from CocoaSpice source (`CSCursor.isInverted` returns
  `!display.isGLEnabled`, suggesting GL-specific cursor handling exists
  - but which branch the session takes is unverified).

  **No upstream filing.** The QXL comparison as the centerpiece would
  get a fair rebuttal (not a valid control). The apples-to-apples A/B
  would be a different SPICE client (remote-viewer, spicy) against the
  same virtio-gpu-gl + GL scanout path - awkward on Apple Silicon but
  the only test that isolates the client. Not worth the effort yet (see
  below).

  **Software cursor is the status quo, not a concession.** crsr = 0
  with WindowServer software compositing works - every mouse move
  dirties the surface and costs a transfer, which the 15 Hz throttle
  absorbs. The hardware cursor is an optimisation (dirty-rect enabler
  during mouse motion), not a missing feature. Leaving this unresolved
  costs close to zero.

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
  - ~~`IOAccelerator3D = Yes` alongside `IOGraphicsAccelerator = No` and
    `IODisplayAccelerated = No`~~ — **FIXED 2026-08-09 (`fb669ac`):** all
    three now derive from `VMVirtIOGPU::m_3d_functional` (const false
    until Mesa + CGL shim lands). The transport-vs-rendering distinction
    is explicit: `supports3D()` reports transport, `is3DFunctional()`
    reports rendering. Single flag, single accessor, every site publishes
    from it -- eventual flip is one line.
  - ~~`model = "VirtIO GPU 3D"` and variants (`"VirtIO GPU 3D (Hardware
    Accelerated)"`, `"VirtIO GPU (Hardware 3D Acceleration)"`)~~ —
    **FIXED 2026-08-09 (`fb669ac`):** also derived from `m_3d_functional`;
    publish `"VirtIO GPU"` until rendering works.
  - ~~`"3D Acceleration" = "Enabled"` on the QXL/SVGA path~~ — **FIXED
    2026-08-09 (`fb669ac`):** QXL has no 3D transport at all; hardcoded
    `"Disabled"` plus `IOAccelerator3D = kOSBooleanFalse` with a comment
    explaining why QXL doesn't share the flag.
  - `IOGLBundleName = "GLEngine"` on the framebuffer node vs.
    `"VMVirtIOGLEngine"` on the `VMQemuVGAAccelerator` child — inconsistent.
    **Still open.** GLPlugin is superseded; the inconsistency is a
    separate cleanup.
  - `IOMetalBundleName = ""`, `IOGLESBundleName = ""` — empty / vestigial.
    (Metal does not exist on 10.6 per CLAUDE.md.) **Still open.**
  - ~~`class-code = <00000300>` on the framebuffer node~~ — **FIXED earlier:**
    the "Override class-code for System Profiler" hack publishing a
    falsified `0x0300` (VGA-compat) on a device whose nub honestly reports
    `0x0380` was removed; it published on the framebuffer node but System
    Profiler reads the PCI nub, so the hack never had any effect.

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

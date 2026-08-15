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

Last updated: 2026-08-14 (personality diff vs VMsvga2 run — FB-side accel trio incomplete/mistyped, IOCFPlugInTypes absent; entry below)

---

## 2026-08-14 — Personality diff vs VMsvga2 (cross-tree read, no boot)

**Context.** VMsvga2-modern's ledger (Q3) established the worked example's
coupling machinery; this diff checks this project's accelerator visibility
against it. All findings are source reads of the live tree
(`/Users/macbookpro/VMQemuVGA`); nothing booted. Recorded in both ledgers.

### Re-ranked later same day: the gate is the suppression, not the properties

The personality diff below stands, but its priority was wrong. Reading the
enclosing function of `VMVirtIOFramebuffer.cpp:1796-1803`:

- It is **`VMVirtIOFramebuffer::isConsoleDevice()`** (`:1789-1811`), an
  `override` (`VMVirtIOFramebuffer.h:150`) called by IOGraphicsFamily
  itself — not by any code in this driver. It runs on the console-claim
  path, i.e. on every guest, **with no OS-version guard**: the Catalina
  workaround is unconditional and therefore also runs on 10.6.
- It writes `IODisplayAccelerated=false`, `IOGraphicsAccelerator=false`,
  `IOAcceleratorFamily=false` on the FB — the same keys `start()`'s 3D
  block writes (`:383-394`), so last-writer-wins applies and the console
  claim can clobber the start-time claims.
- Decisive aggravator: the 3D block's own comment records
  **`functional_3d` is currently always false** (`:375`), and its published
  values follow `functional_3d`, not transport. So **both** paths currently
  publish accelerator=false on the 10.6 guest. The FB never claims
  acceleration at all.

**Consequence:** Phase A's silence (WindowServer created a CGS surface,
never opened a client) is explained at the top level without any property
subtlety — a framebuffer that declares itself unaccelerated has nothing
for CGS to instantiate. The `IOAccelTypes` string-vs-number and
`IOCFPlugInTypes` findings below are real but are **downstream**; testing
them before lifting the suppression would attribute the null result to the
wrong variable.

**Fix order (pre-registered; revised later same day — step 1 is TWO
changes, not one):** lifting the suppression alone changes nothing, because
with `functional_3d` always false, `start()`'s 3D block publishes
accelerator=false too — both paths agree on "no acceleration" under any
ordering. Step 1 is therefore: (1a) conditionalize `isConsoleDevice()` —
**and the conditional must be idempotent**, since the console-claim path
may fire more than once; **and it must preserve the current behaviour on
Catalina**, whose WindowServer crash is real history, not an unguarded
revert; (1b) give the published claims a path to true that is explicitly
**a boot-arg-gated probe, not a shipping value**. Then 2) the FB-side trio
in path-string form + `IOCFPlugInTypes`; 3) reconcile the three
accelerator-ish nubs **before** the experiment, else a positive result is
unattributable (see finding 3); 4) hygiene below.

**Probe vs shipping value — keep the distinction explicit (user,
2026-08-14).** `functional_3d` was correctly held false while the 3D path
was unproven. That era is over in the per-process sense: Mesa renders
through virgl and PowerFox draws real web content (user-attested project
state). But WindowServer still cannot reach any of it — the shim is
per-process via `DYLD_FRAMEWORK_PATH`. So publishing accelerated=true to
WindowServer is a claim nothing behind it can yet honour. Acceptable for a
boot-arg-gated experiment; **the gate must not quietly become the default
later.** Record any flip of the published value as a probe in the boot
args used, and revisit what "functional" means for WindowServer once a
system-wide path exists.

**`functional_3d` is the pivot of the whole probe — do not flatten it
(user, 2026-08-14).** Its honest value depends on which consumer is asking:
per-process 3D is real, WindowServer-reachable 3D is not. That distinction
currently lives in **one boolean**, which is exactly the kind of thing that
gets flattened by someone tidying up. The boot-arg gate is what keeps the
distinction honest; any refactor that merges the probe gate into
`functional_3d` (or vice versa) loses the ability to say what "functional"
means, and for whom.

**Probe design, final (user, 2026-08-14).** The probe knowingly does what
this codebase's own comment warns against (`VMVirtIOFramebuffer.cpp:377-379`:
"advertise a capability you can't deliver, consumers stop falling back to
the working software renderer") — acceptable gated, indefensible as a
default. **Three pre-registered outcomes, not two:**
1. **Selector traffic appears** — WindowServer opens the surface client
   and calls `set_id_mode`/`set_shape*`/`write_lock`/`surface_control`/
   `surface_flush` (QuickTime paths add `swap_surface`). Coupling question
   answered affirmatively; property fix is on the causal path.
2. **Silence persists** — with claims published in the VMsvga2 form,
   WindowServer still opens nothing; something else gates discovery.
3. **The working path breaks** — desktop degrades or WindowServer fails.
   **Discriminator from general instability:** `crsr=0` software
   compositing and the `drawRect:` presentation route are both independent
   of these properties — if the desktop degrades while those are
   unchanged, WindowServer's own compositing decision is the variable, not
   anything in the shim.

**Probe-run boot protocol (user, 2026-08-14).** The probe is the first
driver change in a while and the guest has accumulated state. The boot
that tests it must be **fresh, cached, quiesced, single-vCPU**: four
separate confounds have already invalidated runs on this project, and a
null result on a probe whose whole value is "did selectors appear" is
exactly the shape a confound would produce indistinguishably. Treat any
result from a non-conforming boot as no result.

**Positive control may be documentary, not live.** A known-good
accelerated 10.6 machine may not be reachable. If not:
`VMsvga2Accel.cpp:592-604` is the next best thing — a driver that once
satisfied this exact contract, so its published property set (FB-side
`IOAccelTypes` path string / `IOAccelIndex` / `IOAccelRevision`,
`IOCFPlugInTypes` copy, `AccelCaps`) is a specification even without a
running instance. Diff against it rather than nothing.

**Hygiene (flagged separately):** `IOGLBundleName = "com.apple.kpi.iokit"`
at `VMVirtIOGPU.cpp:6196` is a kernel bundle identifier in a field naming a
userspace GLD bundle. Cannot ever have been intentional; remove on sight.

**Constant verified against the 10.6 artifact** (not MacKernelSDK, which is
a modern reconstruction): real 10.6 SDK
`IOKit.framework/Headers/graphics/IOGraphicsInterfaceTypes.h:34-35` —
`kCurrentGraphicsInterfaceVersion = 1`, `kCurrentGraphicsInterfaceRevision
= 2`. The predicted `IOAccelRevision=2` and GA interface `version=1` are
confirmed era-correct.

### Findings (ranked)

1. **`IOAccelTypes` is never set on the framebuffer, and the accelerator
   nubs set it as a number, not a string.** The worked example sets
   FB.`IOAccelTypes` = **IOService-plane path string** of the accelerator
   (`VMsvga2Accel.cpp:597-598`); the keys are documented for
   `IOAccelFindAccelerator()` (`IOGraphicsInterfaceTypes.h:293-297`).
   Here: the FB sets only `IOGLBundleName="GLEngine"` + `IOAccelIndex=0`,
   and only inside `if (has_3d_support)` (`VMVirtIOFramebuffer.cpp:383-394`);
   `IOAccelTypes=7` **numeric** appears on the accelerator nubs
   (`VMQemuVGAAccelerator.cpp:265`, `VMVirtIOGPU.cpp:479`). If CGS reads
   the FB property expecting a path, nothing points at our accelerator.
   Sharpest candidate for "WindowServer created a surface but never opened
   a client."
2. **`IOCFPlugInTypes` absent everywhere** (grep zero hits in `FB/`,
   `Info-FB.plist`, `GLPlugin/`). The worked example publishes
   `ACCF0000-0000-0000-0000-000a2789904e` on the accelerator personality
   **and copies it onto the FB** at runtime.
3. **Three accelerator-ish nubs with conflicting values**:
   `VMQemuVGAAccelerator` (FB-attached+registered,
   `VMVirtIOFramebuffer.cpp:521-526`, `IOGLBundleName="VMVirtIOGLEngine"`),
   `VMVirtIOGPUAccelerator` (`VMVirtIOGPU.cpp:461-515`,
   `IOGLBundleName="GLEngine"`, `IOOpenGLRenderer=true`), `VMMetalPlugin`
   (`VMMetalPlugin.cpp:87-89`, `IOAccelTypes=2`, revision 1). Plus
   `IOGLBundleName="com.apple.kpi.iokit"` at `VMVirtIOGPU.cpp:6196`
   (a kernel bundle ID used as a GLD name) and a site commenting the key
   REMOVED at `VMVirtIOFramebuffer.cpp:1802`.
4. **`AccelCaps` absent**; worked example sets `AccelCaps=3` when QE is on
   (`VMsvga2Accel.cpp:603-604`).
5. **Catalina-crash suppression shares the path with the 10.6 guest**:
   `VMVirtIOFramebuffer.cpp:1796-1803` forces `IOAcceleratorFamily=false`
   etc. in `isConsoleDevice()`. Whatever runs for 10.6 also carries these.

### Prediction (pre-registered)

Minimal likely fix shape, from the worked example only: on the FB at
runtime, set `IOAccelTypes` = path string of the (single) accelerator nub,
`IOAccelIndex=0` (32-bit), `IOAccelRevision=2`
(=kCurrentGraphicsInterfaceRevision); publish `IOCFPlugInTypes` +
`AccelCaps=3` if QE is wanted; one coherent `IOGLBundleName` (or none).
**Arbiter before any code change**: `ioreg` on the 10.6 guest vs a
known-good accelerated 10.6 machine (real GMA950 or darwin.iso VMwareGfx) —
confirm which object carries the trio and its types. Do not treat numeric
`IOAccelTypes=7` as harmless: if CGS parses it as a path, the failure is
silent.

### Addendum (same day, from reading VMsvga2's GA plugin)

The failure chain for finding #1 is now concrete in userspace: VMsvga2's
GA plugin (the IOGraphicsAccelerator CFPlugIn that apps instantiate) opens
with `IOAccelFindAccelerator(fb_service, &accelerator, &fbIndex)` followed
by `IOServiceOpen(accelerator, self, 2)` (`VMsvga2GA.cpp:216-219`). That
call is the documented consumer of the FB-side `IOAccelTypes`/`IOAccelIndex`
keys — no trio in path-string form, no plugin start, no 2D context, no
`AllocateSurface`. Two further contract details for any GA-equivalent here:
`IOGraphicsAcceleratorInterface.__gaInterfaceReserved[0]` and `[1]` are
load-bearing (WaitSurface and SetSurface live in the reserved slots,
`VMsvga2GA.cpp:1109-1110`), and `surface->accessFlags = 2` must be set
after LockSurface or QuickTime-class clients fail (`VMsvga2GA.cpp:602-606`).

---

## 2026-08-14 — MIG probe PASS; surface-client transport verified open

### Verdict (pre-registered outcome #1)

**PASS, unambiguous, fully attributed.** Every non-control
scalar call crossed the 10.6 IPC boundary and reached kernel
code; zero `0x10000003` (MIG_BAD_ARGUMENTS) anywhere.

Probe raw results (`/Users/sl/probe_accel_surface_mig`, guest,
10:27:33, same connection):

| call | sel | shape | kr | meaning |
|---|---|---|---|---|
| GetState (ctrl) | 2 | 0/1 | 0x0 | success, out[0]=0x1 (idle bit) |
| WriteLockOptions | 3 | 1/0 | 0xe00002c7 | handler's own Unsupported |
| WriteUnlockOptions | 4 | 1/0 | 0xe00002c7 | same |
| ReadLockOptions | 0 | 1/0 | 0xe00002c7 | same |
| Flush | 10 | 0/0 | 0xe00002c7 | same |
| ReadLock | 12 | 0/0 | 0xe00002c7 | same |
| SetIDMode | 7 | 2/0 | 0xe00002c7 | same |
| SetShape | 9 | 2/0 | 0xe00002c7 | same |

Attribution check (the important part): `0xe00002c7` is also
what kernel dispatch returns if `getTargetAndMethodForIndex`
returns NULL — same code, different meaning. The kernel log
discriminates: for EVERY selector, all three lines fired —
`getTargetAndMethodForIndex index=N`, `Returning method N
(count0=X, count1=Y)` with the table's correct per-entry counts,
AND the handler-entry log (`WriteLockOptions(0x0) -> Unsupported`,
`Flush -> Unsupported`, `SetIDMode -> Unsupported`, …). The
returns are the handlers' own kIOReturnUnsupported by design;
the dispatch chain resolved and invoked each handler.

**What this settles (phrased to the evidence):**
- Pure old-style dispatch (no `externalMethod()` override +
  populated `IOExternalMethod` table with count0/count1) works
  on 10.6 for every argument shape tested (0/1, 1/0, 0/0, 2/0).
- **Whatever failed in d98 is unexplained and no longer
  matters.** The probe establishes that THIS configuration
  works; it does not establish what broke THAT one — the d98
  binary came from a build path that isn't this project's and
  cannot be reconstructed. The note's catch-22 mechanism is
  CONSISTENT with the result, not demonstrated by it. (Earlier
  draft of this entry said "CONFIRMED as the d98-era cause" —
  over-claim, corrected per review; do not inherit it.)
- The no-receiver alternative is falsified for TODAY's build
  only.
- **The gate moves to the coupling question**: will WindowServer
  composite a surface whose producer is not GLD? That second
  probe (content in surface → WindowServer shows it) decides
  whether the presentation split pays.

### Archaeology (why the note's evidence was about a binary nobody has)

- `VMAccelSurfaceClient.cpp`: created in exactly ONE commit
  (`ff9f3d8` "Milestone B"), never edited since — pristine.
- `git log -S "VMAccelSurfaceClient"` on `project.pbxproj`:
  EMPTY across all history. The file was never compiled by this
  project at any commit. The November d98/d99 logs came from a
  build path that isn't this repo's.
- Consequence adopted into the pre-registration: results
  attribute to today's build only. The build itself was the
  first-ever compile of the file (one benign C99-designator
  warning; binary grew 0xe7000 → 0xe9000).

### What was built this session for the probe

1. `VMAccelSurfaceClient.cpp` wired into `project.pbxproj`
   (4 placements, mirroring VMCGLContext.cpp entries).
2. Type-0 branch in `VMQemuVGAAccelerator::newUserClient`
   re-enabled, **gated on boot-arg `vm-accel-surface=1`** via
   `PE_parse_boot_argn` — ordinary boots return Unsupported
   exactly as before; only deliberate probe boots expose the
   client. (Recovery if a probe boot goes bad: NVRAM/config.plist
   on the ESP, not guest filesystem surgery.)
3. Handlers hardened: every handler except GetState returns
   `kIOReturnUnsupported` (November crash cause was
   success-without-mapping; Unsupported promises nothing so
   WindowServer falls back). Handler-entry IOLog = kernel-reach
   proof. GetState returns idle (WindowServer's first call).
4. `probe/probe_accel_surface_mig.c` — 8.6MB-free single-file
   probe, cross-compiled x86_64 against the 10.6 SDK.

### Boot-arg facts (newly established)

- **OpenCore's config.plist is authoritative for boot-args**:
  `NVRAM > Delete` lists `boot-args` (GUID
  `7C436110-AB2A-4BBB-A880-FE41995C9F82`), so OpenCore deletes
  the NVRAM var and re-injects from `NVRAM > Add` each boot. A
  guest-side `nvram boot-args=` write is dead on reboot.
- ESP automounts **inside the guest** at `/Volumes/EFI-LEGACY`
  (resolves the doc TODO in `docs/opencore-testing.md` —
  in-guest, NOT host-side). Writable msdos; edit via
  `/usr/libexec/PlistBuddy`, then `sync` (async mount).
- Applied for the probe, then **REMOVED same session**: the
  gate arg was in config.plist only for the probe boot. As of
  session end the plist is back to
  `-v keepsyms=1 debug=0x12a vsmcgen=1 msgbuf=1048576 serial=5`
  (verified, lint OK, synced) — the NEXT boot is byte-identical
  to every ordinary boot this project has measured on. The
  currently-running boot still has the gate in-kernel (harmless:
  all handlers return Unsupported; not a measurement run).
  Re-add the arg for the coupling probe and remove it again
  after. Rationale: a always-on gate is a fourth confound
  alongside vCPU count, background load, and cache state.
- **First-boot-of-new-cache caveat**: the probe boot was
  cacheless; kextd rebuilt the cache post-boot WITH the surface
  client compiled in. The next boot is the first to LOAD that
  cache — any cache defect surfaces then, not now.
- Near-miss recorded (and now codified in build-install rules):
  my first install script would have OVERWRITTEN boot-args with
  a remembered subset, dropping `serial=5`/`vsmcgen=1`. The
  expect timeout accidentally prevented it. Read the live value,
  append, write back the full string.

### Instrument errors made and corrected this session (recorded per rules)

1. **`ps -u sl` cannot see root processes.** The killed-but-
   surviving root kextcache (from my own expect-timeout'd
   install) was invisible to my "no kextcache running" check.
   Use `ps aux` filtered, not `ps -u sl`.
2. **kextd auto-respawns kextcache after cache deletion.** The
   rules' "cache dir stayed empty until run by hand" did NOT
   hold this boot: every kill+rm cycle triggered regeneration
   within ~2 min (pids 2579/2741/2746/2747 spawned by kextd).
   Cacheless-assert does not stick on this guest as configured.
3. **Orphaned PowerFox at 22% CPU for 3+h** (from the artifact
   run) plus syslogd 4.2% (serial=5 firehose) and ARDAgent
   build_hd_index → load 7.7 on 1 vCPU → kextcache crawled
   (13s CPU over 2h15m) and every reboot queued behind its
   root-volume lock. My own forgotten test process violated
   the ledger's system-settle precondition. Kill test apps
   before install/reboot work.
4. **Detached reboots die with the pty.** `nohup … reboot &`
   over `ssh -t` died at session teardown twice. What works:
   `shutdown -r now` as the FINAL FOREGROUND line of the in-
   session script — once committed ("Shutdown NOW!" broadcast),
   init drives it without my tty. The earlier failure shape
   was session-death racing a still-WAITING shutdown.
5. **Script self-match kill bug**: `cleanup_and_reboot.sh`
   matched my own `[r]eboot` grep and killed itself. Neutral
   script names + `$$` exclusion.
6. **Framebuffer type-0 ≠ accelerator type-0.**
   `VMVirtIOFramebuffer::newUserClient(type=0)` is IOFramebuffer's
   standard client (delegates to super) — a different path from
   `VMQemuVGAAccelerator::newUserClient(type=0)` (the surface
   client). Don't read a framebuffer type-0 log line as the
   surface gate.

### Deployment state at this writing

- Kext `a147a91119a42019f8118591d150c3b3` installed, loaded
  (kextstat 8.0.0d82, size 0xe9000), gate ON, probe PASS.
- Guest booted cacheless; kextd will have rebuilt the cache
  post-boot (two-boot-delay note applies).
- Probe + helper scripts live under `/Users/sl/` (home persists
  across reboots; `/tmp` does NOT — re-stage after every reboot).

### Artifact-thread state (parked, same session — recorded so it isn't lost)

PowerFox main window renders REAL WEB CONTENT through the full
chain (descriptor-path fix verified this session: main-window
batches 5052/5220/8648 bytes all cross the ≥4096 boundary via
DESCRIPTOR path; BADARGUMENT 0; MISMATCH 0; killtest control
all-inline 64..2004 bytes unchanged, pixel RGBA(26,26,31,255)).
Remaining artifact class: **rectangles of correct content
composited at wrong destinations** — nav strip doubled at offset,
"New Tab" twice at different x, left-clipped text at consistent
x ("tions on installing…"). User-verified discriminations:
- resize → clears; incremental repaint → returns (partial-damage
  class confirmed).
- **Single-buffer test FALSIFIED guest-side buffer staleness**:
  `SHIM_SINGLE_BUF=1` (render_buf == present_buf, swap no-op,
  double-free guarded in cgl_shim.mm) — artifacts persisted.
- **GL_UNPACK pixel-store leak found and fixed** in
  `CGLTexImageIOSurface2D` (substitute_cgl.c): ALIGNMENT=1 was
  left installed after every IOSurface upload and ROW_LENGTH was
  reset to hardcoded 0 — context-global state leaking into
  Gecko's own texture uploads. Now saved/restored (both params).
  Deployed md5 `0613591a1575faad660840eea5ad0baa`. Effect:
  content became crisp (footer links, search field, text box
  correct) — the leak was real and harmful. Chrome (strip
  composites) still fragmentary.
- ROW_LENGTH units verified correct in the DEPLOYED binary by
  disassembly (divq by bpp at 0x627c–0x6288 + save/restore
  calls) — byte-pass-through and rowlen=width both ruled out.
- Leading hypothesis: **composite destination coordinates**
  (duplication = two draws, which stride smears cannot produce;
  fullscreen quads immune, subrect quads hurt — matches the
  content-clean/chrome-mangled split).
- Pre-registered diagnostic: env-gated pattern-fill on IOSurface
  uploads (row-index color bars). Shear → stride fault in Mesa's
  virgl unpack; clean-but-duplicated → composite destinations
  confirmed, next step vertex-side interpose.

### Scope of the MIG proof + the two redesign axes (settled this session)

**Proof scope — closed loop.** The probe called OUR selectors
against OUR table: it proves old-style dispatch functions and
published counts are honoured. It does NOT prove 10.6's CGS
adopts this numbering — that only closes when WindowServer
itself invokes a selector and the handler fires. The Catalina
explanation (below) makes the numbering question live in
principle; the enum diff closes it anyway:

**Axis 1 — API shape: RESOLVED, numbering matches.** The kext
compiles against the MODERN SDK (MacOSX26.5), so the table's
enum comes from that header. Diffed both SDKs'
`IOAccelSurfaceConnect.h` `eIOAccelSurfaceMethods`: identical
order and membership (ReadLockOptions…SetShapeBackingAndLength,
NumSurfaceMethods=18). The kext publishes exactly the selector
numbers 10.6 CGS would call. **The redesign reduces to backing
only.**

**Axis 2 — VMVirtIOGPUAccelerator zero instances: cause (3).**
Construction site `VMVirtIOFramebuffer.cpp:496`:
`OSTypeAlloc(VMQemuVGAAccelerator)` — the framebuffer hard-
instantiates the BASE class on the virtio path; no plist
personality involved (not causes 1 or 2). Nominally a one-line
re-parent, with two caveats found: the subclass OVERRIDES
`newUserClient` (fixed-ID client — the verified type-0 surface
gate lives in the BASE's newUserClient, so the swap changes
client dispatch) and `start` (holds `m_virtio_gpu_device`,
suggesting it expects VMVirtIOGPU as provider, while the base
attaches to the framebuffer). Dual-instance hazard does NOT
apply today (single construction site) but stands as a design
rule for the redesign: when the subclass registers, the base
must stop matching virtio.

**Redesign shape (agreed, not built):** surface backing as a
virtual method on the accelerator — QXL impl returns a VRAM BAR
address, virtio impl returns a 3D-resource-backed guest buffer;
the surface client stays device-agnostic above it. WriteLock's
memory source is decided there.

**November logs explanation (USER-ATTESTED):** the user
confirms the file was extensively tested on the CATALINA guest —
explains logs existing for a file never compiled in this
project, consistent with all git evidence, and confirms the
notes are stale for this target. Consequences recorded: the
d98-era failure mechanism is a lost binary's story (see verdict
phrasing above), and the user's standing directive is that this
file is the OLD design and **needs redesign** for virtio, not
revival.

## 2026-08-14 (later) — coupling Phase A: SILENT outcome; capability gating is the suspect

**Phase A run (same boot as the MIG probe — gate still armed in
the running kernel; no reboot needed for this phase).**

Requester: `probe/probe_cgs_requester.m` — AppKit window (real
window number 17), CGS trio via dlsym, NO IOKit calls of its own,
so any surface-client kernel line during its run is
WindowServer-originated by construction.

CGS signatures recovered from the guest's CoreGraphics binary by
disassembly (no public header exists; recorded here):
- `CGSAddSurface(cid, wid, uint32_t *out_sid)` — 3 args; out
  NULL-checked, initialized before the IPC
- `CGSBindSurface(cid, wid, sid, a4, a5, a6)` — 6 args; sid!=0
  checked. The 3 trailing args are the GL-side association — the
  GLD-coupling locus, separable from adoption
- `CGSFlushSurface(cid, wid, sid)` — tail-calls
  `CGSFlushSurfaceWithOptions(..., option=1)`
- `CGSOrderSurface(cid, wid, sid, mode, rel)` — ≥5 args
- Also present: GetSurfaceBinding/Count/List,
  SetSurfaceShape/Bounds/Opacity/ColorSpace/Property, MoveSurface,
  RemoveSurface, FlushSurfaceWithOptions, RemoveAllSurfaces,
  CGSMainConnectionID

**Result — pre-registered outcome SILENT:**
```
CGSMainConnectionID=39443
CGSAddSurface -> 0 (OK) surfaceID=0xf3409c2
CGSOrderSurface -> 0 (OK)
CGSFlushSurface -> 0 (OK)
(25s hold)
CGSRemoveSurface -> 0 (OK)
kernel.log: ZERO VMAccelSurfaceClient/GATED/newUserClient lines
            (only routine framebuffer getVRAMRange/getPixelInfo
            from window creation)
```
CGS created and serviced the surface entirely without consulting
the accelerator. Plain AddSurface takes the software/IOSurface
path on 10.6 — adoption does not happen without something else
selecting the accel path.

**Leading suspect — capability gating (verified in ioreg this
session):** the kext publishes `IOAccelerator3D=No`,
`IOGraphicsAccelerated=No`, `IODisplayAccelerated=No` — the
`fb669ac` design deriving all three from `m_3d_functional` (const
false "until Mesa + CGL shim lands", flip anticipated as "one
line"). Mesa + the substitute CGL shim HAVE landed. With all
three No, WindowServer has no reason to consider any accelerated
path — consistent with SILENT.

**Pre-registered next experiment (needs a kext change + reboot):**
boot-arg-gated capability flip (extend the existing gate or a
sibling arg) so ordinary boots stay byte-identical; on the probe
boot publish IOAccelerator3D=Yes and re-run the requester.
- Surface-client lines during AddSurface → adoption is
  capability-gated; mechanism identified; next question becomes
  whether the ops work.
- Still silent with capability on → the gate is deeper (QE/GL
  path needs a renderer first) — GLD hypothesis promoted with
  strong evidence.
- Hazard: advertising 3D may push WindowServer toward GL
  compositing that needs a renderer we cannot yet provide
  (GLEngine discovery is the closed dead end). Boot-arg gate +
  config.plist recovery discipline applies.

**Phase A2 (same boot, requester extended): silence survives a
BOUND context — and the requester is exhausted at the wall.**
Added to the requester: CGL renderer census, pixel format query,
real CGL context, and `CGLSetSurface(ctx, cid, wid, sid)` — the
binding step that makes a surface GPU-relevant. Signature
confirmed by disassembly of the real OpenGL.framework: 4 args
(rdi=ctx mutex-deref'd; esi/edx/ecx reload for a 4-arg indirect
call). The substitute's forward table declares CGLSetSurface with
6 args — latent arity bug there, unexercised by Gecko so far;
fix when touched.
```
CGLQueryRendererInfo -> 0 nrend=1
  renderer[0]: accelerated=0 rendererID=0x1020400
CGLChoosePixelFormat(accelerated) -> 0 npix=0   ← wall
CGLChoosePixelFormat(plain) -> 0 npix=1
CGLCreateContext -> 0 ctx=0x100889e00
CGSAddSurface -> 0 surfaceID=0xf499b03
CGLSetSurface(ctx,cid,wid,sid) -> 0 (OK)        ← binding OK
CGSFlushSurface/Order/Remove -> 0
kernel.log: ZERO surface-client lines
```
Reading: the only renderer CGL can see is SOFTWARE; no
accelerated pixel format exists — exactly what IOAccelerator3D=No
predicts. A software context bound to the surface needs no
accelerator, so silence at A2 is still "nobody needed a GPU."
The requester cannot push further: npix=0 for accelerated formats
is the wall. **Capability flip is now necessary AND interpretable**
(user pre-authorized this ordering): flip on + accelerated
renderer appears + SetSurface produces lines ⇒ capability-gated
end-to-end. Flip on + still no accelerated renderer ⇒ flag
necessary but insufficient ⇒ GLD/GLEngine-plugin hypothesis
confirmed at the CGL layer (plugin discovery is the closed dead
end). Boot hazard acknowledged: IOGLBundleName currently names
VMVirtIOGLEngine, a bundle that does not exist — with 3D=Yes,
WindowServer may attempt a GLD load and find nothing. Boot-arg
gated; config.plist recovery posture applies.

**Instrument lessons:** nm prints Mach-O symbols WITH leading
underscore; dlsym takes the C name without — two requester runs
wasted on `_CGSAddSurface`. kernel.log rotates at size>1000K
(newsyslog) — check rotation before reading a small log as a
fresh boot.

**flush_frontbuffer status (checked 2026-08-14):** still an empty
stub (`virgl_iokit_winsys.c:482`, wired into the vtable at :613).
Fact stands, relevance changed: the substitute's readback+blit
presentation path never calls it, which is why verified rendering
works with it empty. Architecture doc should carry that qualifier.

### Next (pre-registered)

**Coupling probe — the decisive one for the presentation split.**
Put content into a surface and see whether WindowServer composites
it. Pass = WindowServer shows the content (visual check, not a
success log). **Intermediate decisive evidence available before
any content: a handler-entry log line for a call WE DID NOT
MAKE.** The MIG probe was a closed loop (our userspace, our
selectors, our table); the enum diff closed numbering, but
nothing has yet exercised the CGS→client direction. The first
getTargetAndMethodForIndex/handler line from a non-probe caller
proves WindowServer adopted the API — that observation precedes
and de-risks the content step.

**The probe needs a requester (pre-probe design note, 2026-08-14).**
What would make CGS call at all? On 10.6 the surface gets bound
through `_CGSAddSurface`/`_CGSBindSurface`, and something must
INITIATE that — normally the GL path when a context attaches to
a window. If nothing in the guest ever asks for a surface on our
accelerator, silence proves only that nobody requested one, not
that WindowServer refused. Without a requester, absence is
ambiguous and the GLD hypothesis gets promoted on weak evidence.
Requester options: drive the CGS trio directly from userspace
(reachable — the earlier AppKit analysis enumerated
_CGSAddSurface/_CGSBindSurface/_CGSFlushSurface as callable), or
find what prompts AppKit to request one.

**Fail branch — CORRECTED (2026-08-14 review).** The original
pre-registered text read "Fail = WindowServer ignores/errors →
the split is dead on 10.6 without reverse-engineering Apple's
surface client." Kept here as history, but it conflated two
different dead ends. Reverse-engineering Apple's surface client
was the fallback if the DISPATCH layer could not be made to work
— that fallback is CLOSED, by the MIG PASS and by the enum diff;
the transport and API-shape axes are settled. What can still kill
the split is the coupling question, and the thing that would be
needed there is **GLD, not the surface client**: if WindowServer
only composites surfaces whose producer went through a GLD
plugin, no amount of surface-client work substitutes for it.
Accurate statement: transport and API shape are closed; the
remaining way the split dies is WindowServer requiring a
GLD-side producer — a different and larger obstacle than the one
the struck line described. Fail outcome = the readback
presentation path stays.

**Backing strategy for the probe (user insight, recorded):**
WriteLock's contract is handing userspace an address — and that
exact shape is ALREADY proven on virtio: `probe_attach_backing_test`
(2026-08-10) verified ATTACH_BACKING with unaligned userspace
memory, 5-segment scatter list, 4096/4096 dwords byte-exact,
wiring held across a guest write. So: surface backed by a
userspace allocation attached as a virtio 3D resource reuses
verified machinery — WriteLock returns the pointer, the resource
is already attached, Flush becomes TRANSFER_TO_HOST plus whatever
the scanout needs. Backing is plumbing between two proven pieces,
not new mechanism. The genuinely unknown part is the coupling
itself. Note the probe re-enters the configuration the November
disable guarded against (WriteLock returning real memory) —
boot-arg gate is the only protection; decide backing BEFORE
making WriteLock real.

**Re-parent status: cause found, not yet acted on.**
`VMVirtIOGPUAccelerator` registers zero instances because
`VMVirtIOFramebuffer.cpp:496` constructs the base class — not a
matching problem. Making the subclass instantiate requires: (a)
the construction-site swap, (b) reconciling its overridden
`newUserClient` (fixed-ID client — the verified type-0 gate
lives in the BASE's newUserClient) and its `start` provider
expectation (`m_virtio_gpu_device` vs the framebuffer the base
attaches to), and (c) ensuring the base stops matching virtio
once the subclass registers (the dual-instance hazard — this
project's virtqueue-race precedent). Design split agreed: surface
backing as a virtual method (QXL impl: VRAM BAR address; virtio
impl: 3D-resource-backed guest buffer); surface client stays
device-agnostic above it.

---

## Readback investigation — 2026-08-11 (late session)

### Arc

Starting from the CGL shim verified with RED WINDOW, this session
instrumented and measured the ~10 fps bottleneck. The measurement work
surfaced a **critical architectural bug in the killtest's GL dispatch**
that invalidates all performance measurements taken this session.

### Split-dispatch — architectural limitation of the shim, never working

The killtest (`killtest_shim.mm`) links `-framework OpenGL`. Under
macOS two-level namespace, every `gl*` symbol in the killtest binds to
**Apple's OpenGL.framework** at link time — regardless of what
DYLD_INSERT_LIBRARIES or DYLD_LIBRARY_PATH loads at runtime. This is
not a regression from the rebuild; it was never working.

Confirmed by `nm -m killtest_shim`:
```
(undefined) external _glClear (from OpenGL)
(undefined) external _glViewport (from OpenGL)
(undefined) external _glBegin (from OpenGL)
```

Absent: `_glFinish` and `_glReadPixels` — those are in the shim, which
links `-lOSMesa`. They bind to Mesa.

**Result:** draw calls → Apple's GL (no drawable → viewport 1×1 →
nothing rendered). Shim's glFinish/glReadPixels → Mesa (empty resource
→ 5 real submitCommand calls on an empty scene → all-zero pixels).

**This is architectural, not a killtest bug.** Flurry, Gecko, and WebKit
all link OpenGL.framework. The shim as designed cannot route any real
application's GL calls. DYLD_FORCE_FLAT_NAMESPACE=1 was tested — crashes
GUI apps (too aggressive for AppKit/Cocoa system frameworks).

**Why DYLD_FORCE_FLAT_NAMESPACE crashes but flat-namespace binaries don't:**
a binary built flat-namespace (shim_smoke_test, stress_test — Mach header
lacks TWOLEVEL) resolves only ITS OWN undefined references first-found-wins;
system frameworks keep their two-level bindings among themselves.
DYLD_FORCE_FLAT_NAMESPACE=1 forces the ENTIRE PROCESS flat — AppKit,
Foundation, CoreGraphics and everything they load — which is where the
collisions come from. Same word, very different blast radius. Do not
retry DYLD_FORCE_FLAT_NAMESPACE expecting the smoke test's result.

**What survives:** `osmesa_softpipe_test` and `virgl_clear_test` link
`-lOSMesa` directly (`_glClear (from libOSMesa)`, confirmed by nm -m).
Those verifications — Mesa on 10.6, the winsys, the kext, shaders,
textures, DRAW_VBO — are unaffected.

**The shim's presentation path also survives.** `shim_smoke_test` and
`stress_test` are flat-namespace binaries (confirmed: Mach header lacks
`TWOLEVEL` flag). Their `_glClear` resolves first-found-wins at load
time — with libOSMesa loaded via DYLD_INSERT_LIBRARIES before
OpenGL.framework, Mesa's glClear wins. The RED WINDOW test used these
binaries. The entire presentation chain is verified: swizzles, OSMesa
context, double-buffer swap/rebind, glFinish/glReadPixels, drawRect:,
visible pixels on desktop.

**What's broken is specifically GL routing for two-level-namespace apps.**
`killtest_shim` has `TWOLEVEL` in its Mach header (confirmed by
`otool -hv`). Its glClear binds to OpenGL.framework at link time. All
real apps (Flurry, Gecko, WebKit) are two-level. The shim cannot route
their GL calls to Mesa without the interpose fix.

**All session performance measurements are invalid** — they measured a
degenerate workload with no rendering.

**Fix direction (two phases):**

Phase 1 — **15-function interpose** (validates routing on a working
scene). Add `__DATA,__interpose` entries in `cgl_interpose.c` for the
~15 GL functions the killtest uses (glClear, glClearColor, glViewport,
glLoadIdentity, glRotatef, glMatrixMode, glOrtho, glDisable,
glBegin, glEnd, glColor3f, glVertex2f, glFinish, glReadPixels,
glGetString). Each shim function dispatches to Mesa's implementation
via OSMesa's GL dispatch. This mechanism works under two-level
namespace because `_glClear (from OpenGL)` is a genuine dyld bind —
exactly what `__DATA,__interpose` replaces. Verify: glGetString returns
Mesa driver, viewport=800×600, pixels non-zero RGBA(26,26,31,255).

**Generate the interpose list from the binary, not by hand:**
`nm -u killtest_shim | grep ' _gl'` produces the complete set of
GL entry points the killtest references. A partial list produces a
partial scene — some calls to Mesa, some to Apple's GL — which looks
like a rendering bug rather than a missing interpose. The killtest
uses glBegin/glEnd immediate mode, so the list includes glVertex3f,
glColor3f, glMatrixMode, glPushMatrix/glPopMatrix and friends, not
just the obvious glClear/glViewport.

**Implementation gotcha:** replacement functions must reach Mesa via
`dlopen("libOSMesa.8.dylib", RTLD_LAZY)` + `dlsym(handle, "glClear")`,
cached once at load time — NOT by calling the symbol name `glClear()`
from inside the replacement. The usual DYLD_INTERPOSE pattern (call the
original by name) resolves to Apple's glClear inside the interposing
image too, because the interpose replaces the symbol everywhere
including in the shim dylib itself. dlsym returns the raw Mesa function
pointer, bypassing the dyld stub — no recursion, no ambiguity about
self-interposition.

Phase 2 — **substitute OpenGL.framework** (for real apps: Flurry, Gecko,
WebKit). Build a thin dylib linked with
`-reexport_library libOSMesa.8.dylib` plus the shim's CGL*
implementations, deployed as a substitute OpenGL.framework via
DYLD_FRAMEWORK_PATH. Every gl* symbol resolves through the re-export
automatically — no per-function stub, no flat namespace. Two viability
questions: (a) the substitute's install_name and compatibility version
must match what apps recorded against the real framework, (b) it must
export every CGL* an app might reference — worth an `nm -u` sweep
across Flurry and PowerFox to size that set.

### What was built (correct, reusable regardless of the bug)

| Increment | What | Repo | Verified how |
|---|---|---|---|
| Timing instrumentation | Six timestamps (T1-T6) in cgl_shim.mm isolating submit/transfer/lock/blit per frame + wall-clock frame-to-frame. Time-based warmup (12s default, SHIM_TIMING_WARMUP_SECS env override). Per-frame log gated by SHIM_TIMING env var | Mesa-VirGL | Compiles clean, timestamps print correct ms-scale values |
| Call-count diagnostics | fprintf per submit_cmd/transfer_get/transfer_put in virgl_iokit_winsys.c | Mesa-VirGL | Counted 2+2+1=5 calls per frame |
| IOLog gates (kext) | Hex dump + success logs in submitVirglCommandsEx, transferToHost3D, transferFromHost3D gated to first 20 calls | VMQemuVGA | kext md5 f1a9cc614e0e0581c00f6ef41b880fc4 |
| Build scripts | cross-compat/build-10.6.sh (Mesa dylib, 4-bug fix chain), cgl-shim/build-shim.sh (shim + killtest), with precondition/postcondition checks | Both | Reproducible from scripts |
| Killtest time-based spin | `angle = elapsed_s * 60°/s` instead of `phase += 0.02` per frame | Mesa-VirGL | Same GL commands per frame regardless of angle |

### What was found (correct regardless of the bug)

1. **5 submitCommand calls per frame.** 2 submit_cmd (62 dwords + 14
   dwords) + 2 transfer_get (both full-surface 800×600×1, same resource)
   + 1 transfer_put (60×1×1 vertex upload). The 2 transfer_get calls
   with a 14-dword submit_cmd between them suggest: read → discover
   resource referenced → flush → read again. Upstream Mesa pattern,
   fixable by eliminating the redundant read.

2. **IOSleep(1) poll loop confirmed** (VMVirtIOGPU.cpp:2170). Every
   submitCommand costs ≥1ms floor. IOSleep(1) blocks until the next
   scheduler tick — up to ~10ms under TCG. 5 calls × ~10ms = ~50ms
   potential. Fix: bounded spin (IODelay(20) loop for ~200μs, fall back
   to IOSleep(1) if host genuinely slow). NEXT variable, separate boot.

3. **IOLog to serial=5 costs ~1-2ms per formatted line under TCG.** The
   submitVirglCommandsEx hex dump produced 21 IOLog calls per invocation
   (unconditional, not gated). Gated to first 20 calls. Transfer
   success logs also gated. The gate is correct regardless of whether
   the savings measurement was valid.

4. **Two measurement confounds.** (a) Stale killtest processes surviving
   SSH session close (kill -9 via SSH-launched background processes
   unreliable on 10.6 — use `killall -9 killtest_shim` instead).
   (b) Background work (kextd rebuilding caches, mds/mdworker indexing)
   inflating userspace-only spans. **Precondition check needed:**
   `uptime` load average near zero, no kextd/mdworker in `ps`. Log in
   timing header alongside vCPU count.

5. **Guest-wide degradation (unidentified cause).** Submit climbing
   90→800ms over 70s of sustained rendering. RSS flat (not allocation).
   Fresh process on degraded guest slow from frame 1 (not in-process).
   Reboot clears it. Monotonic growth shape is accumulation, not TCG
   cache thrashing (which plateaus). Specific kernel/TCG state
   accumulating across processes: unidentified. Named as open.

6. **Build script bugs found and fixed** (cross-compat/build-10.6.sh):
   force-link missing `libvirgliokit.a` (fourth archive), missing
   `-framework IOKit`, `set -e` killing script before force-link (ninja
   link fails by design — meson doesn't include virgl in OSMesa target),
   `--no-force-link` now produces nothing (force-link is mandatory).

### What was falsified

1. **"IOLog gate saved 50ms"** — measured on degenerate workload.
   UNRESOLVED. Must re-measure after split-dispatch fix.
2. **"Cacheless boot inflates steady-state"** — wrong. kextd/mdworker
   background work was the cause. Cacheless boot doesn't slow steady-state.
3. **"Degradation is TCG-specific"** — 9× growth is accumulation, not
   cache thrashing. RSS flat. Guest-wide, not in-process.
4. **"No current context"** — OSMesaGetCurrentContext non-NULL,
   renderer "softpipe", OSMesaMakeCurrent returns GL_TRUE. Falsified.
5. **"meson.build dep_iokit broke rendering"** —
   declare_dependency(link_args:) only adds linker flags. Can't change
   compiled code.

### Corrections made during the session

1. **Shim plan premise was wrong.** The plan assumed GL entry points
   resolve into Mesa via DYLD_LIBRARY_PATH. They don't: DYLD_LIBRARY_PATH
   doesn't affect framework lookups under two-level namespace. GL symbols
   bind to their link-time library. The killtest's gl* calls go to Apple's
   OpenGL, not Mesa. User's own correction.

2. **Radii framework for variant generalisation.** Three radii: PCI-ID
   (all variants), VGA/no-VGA split (class-code + BAR layout + cap walk +
   mapBarByNumber + aperture, family-scoped), virgl backend (three -gl
   variants: virtio-vga-gl, virtio-gpu-gl-pci, virtio-ramfb-gl).

3. **GfxInfo does not query CGL.** Gecko's Mac blocklist reads
   vendor-id/device-id from IOKit registry
   (IORegistryEntrySearchCFProperty), not CGL renderer queries.
   Renderer-info interpose idea dropped. PCI nub publishes
   vendor-id=0x1af4 / device-id=0x1050. No static blocklist match.
   Downloaded blocklist probably never fetches (TLS 1.2 on 10.6).

4. **README corrected.** virtio-vga-gl promoted to recommended variant
   (System Profiler Graphics/Displays entry + resolution picker).
   BAR-layout added to header note alongside class-code as the
   family-scoped differences. Devices list demoted others to functional.

### Open items

1. **Split-dispatch fix** (BLOCKING). Killtest must route GL through
   Mesa. Rebuild with `-lOSMesa` instead of `-framework OpenGL`, or use
   `DYLD_FORCE_FLAT_NAMESPACE=1`. Verify: `nm -m` shows glClear from
   libOSMesa, glGetString returns Mesa driver, pixels non-zero.
2. **IOLog gate** — re-measure after split-dispatch fix on a working
   scene. Pre-registered: submit+transfer drop, wall drops >5ms.
3. **IOSleep(1) poll loop** — bounded spin, separate boot.
4. **Redundant transfer_get** — 2 full-surface reads per frame.
5. **Guest-wide degradation cause** — kernel/TCG state accumulating.
   Unidentified.
6. **flush_frontbuffer stub** — virgl_iokit_flush_frontbuffer empty.
7. **~34ms unmeasured span** — T0 at top of render loop.

### Next concrete step and pre-registered prediction

Implement the 15-function GL interpose in `cgl_interpose.c`. Verify:
glGetString(GL_RENDERER) returns Mesa driver, viewport=800×600, pixels
non-zero RGBA(26,26,31,255) for the clear color. Then re-run the IOLog
gate A/B on a working scene.

Prediction: with GL routing through Mesa, the scene renders correctly.
If IOLog costs ~1-2ms/line under serial=5 as observed on the degenerate
workload, the gate saves ~50ms/frame on the real workload too. If it
shows no effect, the IOSleep(1) poll loop is the dominant cost and
becomes the next variable.

### Interpose implementation — DONE, but GL calls are no-ops

17 GL function interposes implemented in `cgl_interpose.c` (generated
from `nm -u killtest_shim | grep ' _gl'`). Build link order changed:
`-framework OpenGL` before `-lOSMesa` so DYLD_INTERPOSE tuples target
Apple's gl*. Verified post-build: `nm -m cgl_shim.dylib` shows
`_glClear (from OpenGL)`. Context-check in each interpose function:
forwards to Mesa when `OSMesaGetCurrentContext()` is non-NULL, falls
through to Apple via `RTLD_NEXT` otherwise (prevents crash from Apple
CGL internal gl* calls during context creation).

**The interpose mechanism works:** `ip_glViewport FIRED osmc=0x101444950
p=0x100006000 args=(0,0,800,600)`. Constructor runs, dlopen succeeds,
dlsym finds all 17 Mesa function pointers (non-NULL). OSMesa context is
current.

**But GL calls are no-ops:** `renderer="(null)"`, `viewport=(0,0,0,0)`,
`scratch=(0xdeadbeef,...)` (sentinels unchanged from glReadPixels),
`submit=0.01ms` (glFinish does nothing). Every Mesa function is called
with correct args but produces no effect.

**Dual-dispatch hypothesis — FALSIFIED.** Proposed: libOSMesa has a
statically linked copy of `_glapi_tls_Dispatch` alongside libglapi's
shared copy, causing write/read mismatch under two-level namespace.
Checked by `nm -g`:

```
libOSMesa:  U __glapi_tls_Dispatch  U __glapi_set_dispatch  U __glapi_get_dispatch
libglapi:   D __glapi_tls_Dispatch  T __glapi_set_dispatch   T __glapi_get_dispatch
```

All dispatch symbols are `U` (imported) in libOSMesa, `D`/`T` (defined)
in libglapi. ONE copy. No split. The dispatch state IS unified.

**Actual cause: UNIDENTIFIED.** The interpose fires, Mesa pointers are
valid, OSMesa context is current, dispatch state is unified. But GL
functions are no-ops. The flat-namespace smoke test works (same Mesa
build, same dispatch). The interpose path calls the same functions via
dlsym pointers. Something differs that requires runtime stepping
(gdb/lldb on the guest) to identify.

**Hypotheses to check next session:**
1. **Emulated TLS (leading candidate).** This build uses
   `-femulated-tls`. `_glapi_tls_Dispatch` is `__thread`, which under
   emulated TLS resolves through `__emutls_get_address` against a
   per-variable control object. If the interpose path and OSMesa's
   MakeCurrent end up reading different TLS keys — or run on different
   threads — the reader gets NULL and every entry point falls to the
   no-op stub. Matches all four symptoms (NULL renderer, 0×0 viewport,
   unchanged sentinels, 0ms submit). Consistent with flat namespace
   working (single resolution) while two-level doesn't.
   **Cheap first probe:** call `_glapi_get_dispatch()` directly from
   inside one interposed function and log the pointer, then log it
   again from the shim's glFinish path. Same non-NULL value means
   dispatch is fine; NULL in the interpose and non-NULL in the shim
   confirms the TLS story and points at the thread or the emulated-TLS
   key. No runtime stepping needed.
2. Are the dispatch table's function pointers (dispatch->Viewport etc.)
   non-NULL? (Print `dispatch->Viewport` after OSMesaMakeCurrent.)
3. Does the OSMesa context's pipe_context have a valid screen?
4. Is there a Mesa state-tracker init step that the shim skips?

---

## GL routing fix — 2026-08-12 (CGL shim verified end-to-end)

### Arc

Starting from the GL interpose implementation (17 functions in
cgl_interpose.c), GL calls fired but produced no effect: renderer=NULL,
viewport=(0,0,0,0), no virgl winsys activity. Emulated-TLS was the
leading hypothesis. This session falsified that hypothesis, found the
real root cause, and verified GL routing end-to-end through virgl to
the host GPU. **The CGL shim now works on a real two-level-namespace
OpenGL application — killtest_shim shows a dark window with a rotating
triangle at ~3 fps.**

### Emulated-TLS hypothesis — FALSIFIED

Cheap probe (per the prior session's pre-registration): call
`_glapi_get_dispatch()` from inside one interposed function and log
the pointer; log it again from the shim's glFinish path.

Result (both paths, same thread `0x7fff70f0acc0`):
- ip_glViewport (interpose): `dispatch=0x1018a9800`
- dispcheck[shim] (flushBuffer): `dispatch=0x1018a9800`

Same non-NULL pointer. TLS dispatch is fine. Emulated-TLS hypothesis
falsified.

### Real root cause — cgl_shim's own gl* calls bind to OpenGL.framework

Extended the dispatch probe to dump entries. entries[0..3] = `_mesa_NewList`,
`_mesa_EndList`, `_mesa_CallList`, `_mesa_CallLists` (verified via
`nm` offset lookup) — real state tracker functions, not no-op stubs.
entries[203] = `_mesa_Clear` (glClear slot, per glClear entry stub
disassembly `jmpq *0x658(%rax)`, 0x658/8 = 203). Dispatch table is
correctly populated; dispatch path works.

Pre-resize viewport probe inside flushBuffer: `viewport=(-1,-1,-1,-1)`
— sentinels unchanged. The call went nowhere.

`/usr/bin/nm -m cgl_shim.dylib` revealed:
```
(undefined) external _glFinish (from OpenGL)
(undefined) external _glGetString (from OpenGL)
(undefined) external _glGetIntegerv (from OpenGL)
(undefined) external _glReadPixels (from OpenGL)
```

**cgl_shim.dylib's own gl* calls bind to OpenGL.framework, NOT libOSMesa.**
The `-framework OpenGL` before `-lOSMesa` link order (required for the
DYLD_INTERPOSE tuple to target Apple's gl*) wins the symbol binding
for cgl_shim's own calls too.

**DYLD_INTERPOSE does NOT apply within the interposing image itself.**
So cgl_shim's `glFinish()` calls Apple's GL, which has no current
context (the OSMesa context isn't an Apple CGL context) — silent no-op.

This explains why ip_glViewport's probe (which used `p_glGetIntegerv`
via dlsym) worked: `before=(0,0,1,1) after=(0,0,800,600)`. Direct
dlsym → libOSMesa → Mesa's entry stub → real dispatch. The interpose
functions also work because they call `p_gl*` (dlsym), not `gl*`.

But cgl_shim.mm's flushBuffer used `glFinish()`, `glReadPixels()`,
`glGetString()`, `glGetIntegerv()` directly — all going to Apple's GL.

### Fix #1 — route cgl_shim's gl* calls through p_* pointers

Made `p_gl*` non-static in `cgl_interpose.c` (removed `static`
qualifier from the 18 function pointer declarations). Declared them
`extern "C"` in cgl_shim.mm. Replaced direct gl* calls in flushBuffer
with `p_gl*`:
- `glFinish()` → `p_glFinish()`
- `glReadPixels(...)` → `p_glReadPixels(...)`
- `glGetString(GL_RENDERER)` → `p_glGetString(GL_RENDERER)`
- `glGetIntegerv(GL_VIEWPORT, vp)` → `p_glGetIntegerv(GL_VIEWPORT, vp)`
- `glGetError()` → `p_glGetError()`

Result after rebuild: `renderer="virgl (Apple M4 Pro)"`, viewport
persists across flushBuffer, virgl winsys activity (submit_cmd +
transfer_put + transfer_get), preswap `render[0]=0xff1f1a1a` (the
0.1,0.1,0.12 clear color), drawRect `pixel RGBA(26,26,31,255)`.

### Fix #2 — supports_encoded_transfers mismatch

Hit a second bug: `Assertion failed: (vctx->supports_staging), function
virgl_staging_map, file virgl_resource.c, line 356`.

Root cause: in virgl_context.c:1819-1823, `vctx->supports_staging`
requires `vws->supports_encoded_transfers && VIRGL_CAP_TRANSFER`. But
`res->use_staging` (virgl_resource.c:668) is set independently based
on `VIRGL_CAP_V2_COPY_TRANSFER_BOTH_DIRECTIONS` in cap bits v2.

If the host advertises the v2 bit but the winsys reports
`supports_encoded_transfers=0`, `use_staging=true` but
`supports_staging=false`. virgl_staging_read_map gets called (because
use_staging) and asserts supports_staging.

Our iokit winsys had `supports_encoded_transfers = 0`. Both reference
implementations set it to 1:
- drm: `qdws->base.supports_encoded_transfers = 1` (always)
- vtest: `vtws->base.supports_encoded_transfers = (protocol_version >= 2)`

Set `iws->base.supports_encoded_transfers = 1` to match. The iokit
transport carries the same virtio-gpu command stream as drm; encoded
transfers just embed transfer commands in submit_cmd's buffer, which
we already handle.

Result: assertion gone, 275 winsys calls in 20s, 47 frames rendered,
user reports "dark window + rotating triangle" visible in UTM.

### What's verified

- **GL routing for two-level-namespace apps.** The original split-dispatch
  bug is fixed. killtest_shim (which has TWOLEVEL in its Mach header)
  now routes GL through Mesa → virgl → host.
- **End-to-end pipeline.** App gl* → interpose → Mesa → virgl →
  virgl_iokit_winsys → kext → virtio-gpu → virglrenderer → host GPU.
  Visible pixels match expected clear color.
- **Renderer string correctness.** `glGetString(GL_RENDERER)` returns
  `"virgl (Apple M4 Pro)"` — the host GPU name propagates through
  virglrenderer's cap set.
- **Frame rate ~3 fps.** Slow because real work per frame (vertex
  upload + draw + readback + blit). Was ~10 fps on the degenerate
  no-render workload; that number was meaningless.

### Open items

1. **Performance investigation** — now meaningful for the first time.
   With real rendering, measure: IOLog gate A/B, IOSleep(1) poll loop,
   redundant transfer_get, ~34ms unmeasured span. All previously
   recorded open items become actionable.
2. **GL interpose list completeness** — only 17 functions interposed.
   Real apps (Flurry, Gecko, WebKit) reference more. Generate the
   union from `nm -u <app> | grep ' _gl'` for each target.
3. **Phase 2 (real apps)** — substitute OpenGL.framework via
   DYLD_FRAMEWORK_PATH with `-reexport_library` thin dylib. Removes
   need for per-function interpose.
4. **cgl_shim self-call audit** — verify ALL gl* calls in cgl_shim.mm
   go through p_gl*. Current grep confirms 0 direct gl* calls. Add
   a build-time assertion (grep gate in build-shim.sh) to prevent
   regressions.
5. **Renderer string side effect** — CLOSED 2026-08-12. With
   `supports_encoded_transfers=1`, glGetString(GL_RENDERER) returned
   `"virgl (Apple M4 Pro)"`. With `=0`, it returned `"virgl"`. The flag
   should gate a transfer mechanism, not what the host reports about
   itself — so the caps path differed between the two settings. Traced
   to the get_caps capset-id bug: idx=0 normally returned v1 (308 B),
   leaving `host_feature_check_version` at default 0, so
   `virgl_get_name` returned plain "virgl". The `=1` run was first-
   after-boot; idx=0 returned garbage (size > 2048), fell through to
   idx=1 (VIRGL2, 1408 B), and the v2 fields were populated. Fixed by
   rewriting `virgl_iokit_get_caps` to query by `capset_id` (2 then 1)
   instead of by index — now consistently fetches VIRGL2 regardless
   of host index ordering or first-call garbage. See "A/B test
   retracted" section below.

### A/B test retracted — 2026-08-12 (caps contamination)

**The "staging costs 54 ms" A/B recorded earlier this session was invalid.** Both sides of the comparison ran with different capability sets, not just different `supports_encoded_transfers` values, so the timing delta cannot be attributed to the flag.

The contamination: `virgl_iokit_get_caps` queried capset INDEX 0 unconditionally, which on this device is VIRGL (v1, 308 B). The v2 fields (`host_feature_check_version`, `renderer[64]`, `capability_bits_v2`) stayed at the `fill_new_caps_defaults` values. **However**, on the very first user-client open after boot, `getCapsetInfo(0)` returned garbage (`id=1 version=4030145808 size=185270272`) — the size > 2048 sanity guard skipped it, the code fell through to idx=1 (VIRGL2, 1408 B). So:

- Run 1 (`=1`): first-after-boot, accidentally fetched VIRGL2 v2 caps (garbage path) → `host_feature_check_version >= 5`, renderer = "virgl (Apple M4 Pro)"
- Run 2 (`=0`): subsequent run, fetched VIRGL v1 (308 B) → `host_feature_check_version = 0`, renderer = "virgl"

The renderer string difference, attributed to the flag, was actually the caps path differing. The "54 ms" was measuring staging-vs-no-staging AND v2-vs-v1 caps simultaneously. Recorded numbers retracted.

The renderer-string open item (5) is also closed by this finding — explained, not a side effect of the flag.

### get_caps fix — query VIRGL2 capset by id

`virgl_iokit_get_caps` rewritten to match the drm winsys pattern: iterate `capset_id` from 2 down to 1, find the index hosting that id, fetch the blob. Now always returns VIRGL2 (capset_id=2, ver=2, size=1408) regardless of host index ordering or first-after-boot garbage. `glGetString(GL_RENDERER)` consistently returns "virgl (Apple M4 Pro)" across runs.

Trailing 24-byte truncation (`copy=1384` vs host's `size=1408`) noted as separate item — `sizeof(union virgl_caps)` is 1384 on this build, host offers 1408. Likely a trailing-array size disagreement; not blocking since the truncated tail (video_caps entries) isn't used by the killtest path.

### staging A/B redone with consistent v2 caps — the actual finding

With the get_caps fix landing, both `=1` and `=0` see the same v2 caps. A/B redone (n=13 vs n=32, post-warmup):

| Metric | `=1` (staging) | `=0` (non-staging) |
|---|---|---|
| submit | 372 ms | 108 ms |
| transfer | 238 ms | 59 ms |
| wall | 683 ms | 347 ms |
| **pixels** | **RGBA(0,0,0,0) ✗** | **RGBA(26,26,31,255) ✓** |

**With `=1`, staging doesn't just cost more — it produces no rendering.** Pixels come back empty. The host isn't executing the encoded `COPY_TRANSFER` commands the way virgl expects on this transport.

This is the same over-claiming pattern as `crsr=1` and `IOAccelerator3D`: setting `supports_encoded_transfers=1` claims a capability the iokit winsys doesn't implement. drm sets `=1` because drm DOES implement encoded transfers; vtest sets `=1` when protocol_version >= 2 because vtest DOES implement them; iokit doesn't.

**`supports_encoded_transfers=0` is the honest value, not a divergence from upstream needing justification.** The earlier "54 ms saving" framing had it backwards on both counts.

### Staging-path gates — driver robustness (commits 3 + 4)

Independent of the flag-value decision, two gates in `virgl_resource.c` were missing:

1. **`virgl_can_use_staging`** was gated only on host's `VIRGL_CAP_V2_COPY_TRANSFER_BOTH_DIRECTIONS`. With v2 caps (post get_caps fix), every texture resource got `use_staging=true`, which set `alloc_size=1` (staging assumes a tiny placeholder because transfers go through a separate staging buffer). But with `supports_encoded_transfers=0`, `vctx->supports_staging` is false, the staging transfer path isn't taken, and the direct readback memcpy'd 1.92 MB into a 1-byte allocation → SIGBUS on first transfer_get. Added `&& vs->vws->supports_encoded_transfers` so resources are only marked use_staging when the winsys can actually deliver.

2. **`virgl_resource_transfer_prepare`** (two sites, lines 259 and 305) checked `res->use_staging` without also checking `vctx->supports_staging`. With the allocation gate above, `use_staging` is now consistent with `supports_staging` — but the gate here is still needed for any future combination where they could desync (and for upstream robustness, since the desync is a real driver bug independent of iokit). Added `&& vctx->supports_staging` to both sites. No winsys setting can trip `virgl_staging_map`'s assertion now.

Both gates are upstream-shaped fixes; the second is upstreamable as-is. The first is iokit-specific (drm/vtest don't need it because they set `supports_encoded_transfers=1` truthfully).

### Kext used-ring baseline (drain fix) — written, not installed

Traced the first-after-session-open garbage read on `getCapsetInfo(0)`: `submitCommand` polls the device's used ring with `m_vq_last_used` initialised to 0 at PCI device start. Boot probes (probeTransport3D, etc.) advance the ring correctly, but if a previous user-client session left unconsumed responses (e.g. clientDied), `m_vq_last_used` lags the device's `m_vq_used->idx`. The first submitCommand of a new session sees `used_idx != last_used` immediately, reads a stale descriptor, and returns the stale response as if it were its own.

Symptom signature: garbage once, correct forever after, only on the first call following session open. Could affect any first-after-open selector call, not just caps — `createResource3DEx` could silently return a bogus id. Wider than caps.

Fix written in `FB/VMVirtIOGPU.cpp` (drain stale entries at `submitCommand` entry, before publishing to the avail ring). Built into `/tmp/VMQemuVGA.kext` on guest. **Not yet installed** — needs cache clear + reboot to verify. Doing this before IOSleep since IOSleep changes the same poll loop and the two would interact in a single boot.

### Files committed (Mesa-VirGL, branch cross-10.6)

Seven commits landed this session:

```
1f8eb2f8f24 chore(virgl/iokit,osmesa): gate diagnostic instrumentation behind env vars
677af2f8007 build: add cross-compat/build-10.6.sh — Mesa cross-build script
ed478855444 fix(virgl/iokit): declare IOKit framework dependency in meson.build
8b5a5d0a224 fix(virgl): make staging transfer path robust to supports_staging=false
a09ddbb5161 fix(virgl): gate use_staging on supports_encoded_transfers
6c808cd7da7 fix(virgl/iokit): query VIRGL2 capset by id, not by index
fd7b7cf1de0 fix(cgl-shim): route own gl* calls through Mesa p_* pointers
```

Diagnostics gated behind `VIRGL_IOKIT_DEBUG=1` and `OSMESA_TARGET_DEBUG=1` (matches `SHIM_TIMING=1` pattern) — off by default, on when needed for IOSleep / caps / call-count work.

Build script now writes the interpolated cross file into `build-106/` rather than in-place; tracked `cross-compat/mesa-cross-10.6.txt` stays pristine (the prior in-place substitution was leaving machine-specific paths permanently dirty in git status).

### Verified (2026-08-12, post all fixes)

- `killtest_shim` renders correctly: `pixel RGBA(26,26,31,255)` matches `glClearColor(0.1, 0.1, 0.12, 1.0)`, ~3 fps steady state
- `textured_triangle_test` byte-exact with `=0` (textures + sampler state + GLSL `texture2D()`)
- `glGetString(GL_RENDERER)` consistently returns "virgl (Apple M4 Pro)" — v2 caps fetched
- CGL shim's GL routing works for two-level-namespace apps (the original split-dispatch bug, fixed via `p_gl*` routing)

## Phase 2 viability study — 2026-08-12

### Arc

After IOSleep spin landed (killtest at 7.5 fps, wall 127 ms), the
investigation shifted to Phase 2: making real apps (PowerFox, Flurry)
reach Mesa/virgl. The killtest is no longer the right target — its
workload (800×600 full-surface readback every frame) is a synthetic
worst case, and the remaining per-call cost is TCG transit, not a bug.

### Substitute framework gate test — PASSED

Minimal 2-symbol substitute framework (stub `glClear` + `CGLCreateContext`)
deployed to guest. Test binary links the real OpenGL.framework's
install_name. With `DYLD_FRAMEWORK_PATH=/tmp/subst_test`, dyld loaded
the SUBSTITUTE, not the real framework:

```
dyld: loaded: /tmp/subst_test/OpenGL.framework/Versions/A/OpenGL
SUBSTITUTE: glClear called (mask=0x4000) — stub
test_app: glClear returned
```

DYLD_FRAMEWORK_PATH overrides absolute install_names on 10.6's dyld.
The substitute wins at launch; anything that later dlopens by absolute
path gets the already-loaded image.

### But substitute framework is the wrong mechanism — interposition wins

**UPDATE: this conclusion was REVERSED by the dlsym check below. Kept
here for the reasoning trail.**

Census with host `nm` (guest's 10.6 `nm` reports "malformed object
(unknown load command 42)" on modern Mach-O — census was invalid
until re-run on host):

| Component | CGL static | gl* static |
|---|---|---|
| XUL | **10** | **21** |
| QuartzCore | **107** | 0 |
| CoreVideo | **51** | 0 |

(CGLayer* symbols — CGLayerCreateWithContext, CGLayerGetContext,
CGLayerRelease — are CoreGraphics Layer functions, NOT OpenGL CGL.
False positives from `_CGL` grep. Corrected: XUL has 10 real CGL,
not 13.)

A substitute framework must export ALL of QuartzCore's 107 + CoreVideo's
51 = **160+ CGL symbols at load time**, or those frameworks fail to
launch and PowerFox won't start. Most of those are for their own internal
compositing — we'd implement 160+ symbols to satisfy a load-time
requirement we don't want to service.

Interposition covers XUL's 10 CGL + 21 gl* = **31 symbols** with no
load-time obligation. Unmatched symbols simply aren't interposed —
QuartzCore's launch isn't affected. Runtime exposure is the same either
way (if QuartzCore calls an interposed gl*, it reaches Mesa), but
interposition adds no load-time failure mode. Interposition appears to
strictly dominate.

### REVERSAL: Gecko uses GLLibraryLoader (dlsym) — substitute framework is necessary

The above interposition argument is contingent on XUL's static binds
being the complete set. **They are not.** Gecko's
`mozilla::gl::GLLibraryLoader::LoadSymbols` and
`GLContext::LoadFeatureSymbols` (both confirmed present in XUL via
strings) dlopen the OpenGL.framework and dlsym hundreds of additional
GL entry points at runtime — extensions, version-specific functions,
ARB/EXT suffixes. XUL contains the full framework path strings
(`/System/Library/Frameworks/OpenGL.framework/Versions/A/OpenGL`)
used as dlopen arguments.

Interposition rewrites dyld's binding of undefined symbols only.
**dlsym is not affected by __DATA,__interpose.** A call resolved via
`dlsym(handle, "glTexImage2D")` returns Apple's GL function pointer,
bypassing the interpose entirely. Result: `glClear` (static) → Mesa,
but `glTexImage2D` (dlsym'd) → Apple's GL → no Mesa context → partial
scene or crash.

Only a substitute framework changes what dlsym finds. With
`DYLD_FRAMEWORK_PATH` pointing at the substitute (gate test already
passed), the dlopen call returns the substitute's handle, and every
subsequent dlsym resolves through Mesa via `-reexport_library`.

**The 160+ CGL symbol burden is the necessary cost of correctness.**
Most can be stub implementations that return plausible defaults
(kCGLNoError, etc.) — they satisfy QuartzCore/CoreVideo's load-time
link requirement without actually servicing compositing calls (which
go to Apple's path if the stub returns an error that QuartzCore
handles gracefully).

### Decision: build substitute framework, not scale interposition

The interpose set from Phase 1 is still useful as a fallback for apps
that DON'T use dlsym (the killtest is one — it statically binds
everything). But for PowerFox/Gecko, the substitute framework is the
only mechanism that covers both static AND dynamic GL resolution.

Build path:
1. Thin x86_64 dylib with install_name matching OpenGL.framework
2. `-reexport_library libOSMesa.8.dylib` (covers all gl* that Mesa exports)
3. CGL implementation layer (~160+ symbols — many stubs, real ones for
   the 10 that XUL actually uses)
4. Deploy via DYLD_FRAMEWORK_PATH (gate test already passed)

### Substitute framework — BUILT and LOADS (2026-08-13)

**Substitute OpenGL.framework built and verified loading into PowerFox
and System Preferences.** Multiple issues found and fixed in sequence:

**Static libc++ relink (eliminates init-order crash).** libOSMesa had
LC_REEXPORT_DYLIB for libc++/libc++abi (from Mesa build). When
re-exported from the substitute, dyld processed libc++ init during
framework loading — before the host process was set up. Result:
`ios_base::Init::Init()` → NULL dereference. Fixed by statically
linking libc++ into libOSMesa using x86_64 static archives from
llvm 5.0.1 (via leopard-webkit-build/macports-mirror). `-nostdlib++`
prevents auto-linking the dynamic version. libOSMesa now has zero
libc++ dependency — no init-order conflict, no duplicate runtime,
works in any host app.

**DYLD_LIBRARY_PATH causes LaunchServices crash.** With both
DYLD_FRAMEWORK_PATH and DYLD_LIBRARY_PATH set, LaunchServices aborts
during `_LSApplicationCheckIn`. Only DYLD_FRAMEWORK_PATH is needed —
the substitute's rpaths resolve libOSMesa/libglapi/libOpenGL_real.
Added `@loader_path/../../..` rpath so the framework resolves
dependencies from the deploy directory.

**GC compatibility for System Preferences.** System Preferences
requires ObjC Garbage Collection. The substitute was not compiled with
`-fobjc-gc` (Apple Silicon clang can't emit it — deprecated post-10.8).
Fixed by injecting `__DATA,__objc_imageinfo` section via C attribute +
post-build dd patch to set OBJC_IMAGE_SUPPORTS_GC flag (0x02).

**Interpose section excluded.** `__DATA,__interpose` from
cgl_interpose.c is `#ifdef !SUBSTITUTE_FRAMEWORK` — the interpose
model targets Apple's gl* in other images; the substitute IS the
OpenGL framework, so interpose is both redundant and harmful (makes
AppKit's gl* calls into no-ops via RTLD_NEXT returning NULL).

**CGL forward instrumentation (observed, not predicted).** Only 2 of
62 CGL forwards actually fire during Gecko's GL init: CGLSetOption
(global, safe) and CGLSetParameter (per-context, receives shim token).
CGLSetParameter fixed with registry lookup (no-op for shim contexts).
CGLDescribePixelFormat and CGLQueryRendererInfo never called.

**Zero blast-radius calls.** glClear no-context probe (GL_NO_CTX)
fired 0 times during 5-minute PowerFox run. No system framework calls
gl* without a Mesa context on this workload. Per-caller routing is
unnecessary.

**Compile as .m not .mm.** Eliminates libc++ dependency from the shim
object itself (clang++ auto-adds -lc++; clang doesn't). extern "C"
blocks guarded with `#ifdef __cplusplus`.

**CORRECTION (2026-08-13, re-dated): "Gecko compositor rejects the
context" was never observed.** FEATURE_FAILURE_OPENGL_CREATE_CONTEXT
was an inference from the missing window, built on pf_test2's
external-chain result (GL 2.1, virgl). Those pf_test2 runs carried
`GALLIUM_DRIVER=virgl` in their environment (the standard launch
incantation of the era — guest bash_history shows it on PowerFox
launches too). The first bare-env PowerFox run under the substitute
(2026-08-13, 300s, `MOZ_GL_SPEW=1`, DYLD_FRAMEWORK_PATH only, log
/tmp/pf_spew2.log) shows what actually happens: **GLContext::Init()
does not fail.** Gecko logs `OpenGL version detected: 330`,
`OpenGL renderer: softpipe`, loads symbols (same six optional
misses), and compiles its VS/FS shaders (KHR_debug: 15-inst VS,
2-inst FS). No rejection evidence exists in any PowerFox log under
the substitute. The verified-NOT-the-blocker list below was testing a
failure that was never happening.

**Actual root cause of no-window: setenv after first context.** The
substitute's CGLCreateContext called `OSMesaCreateContextExt` before
its `setenv("GALLIUM_DRIVER","virgl")` (the setenv sat after the
create). Mesa's driver choice is a process-global call_once keyed off
GALLIUM_DRIVER — the first context landed on default softpipe and
every later context in the process reuses it. Gecko then runs a GL
3.3 softpipe context: functional (init completes, shaders compile)
but software compositing under TCG never reaches first paint in the
observation window. The 3.3 in the log is itself confirmation of the
diagnosis — softpipe's ceiling; virgl through the same entry point
reports 2.1.

(Note on method: the first spew attempt, 100s with
`MOZ_GL_SPEW=1 MOZ_GL_DEBUG=1`, produced no GL lines at all. The
300s SPEW-only run reached shader compile. Attribution to DEBUG's
per-call glGetError cost vs the shorter window is not isolated.)

Verified NOT the blocker (each tested, pre-correction):
- NSOpenGLPixelFormat initWithAttributes: with NSOpenGLPFAAccelerated
  → SUCCEEDED (hypothesis falsified by 20-line test app)
- Full chain: pixel format → context → makeCurrent → setValues →
  glGetString → PASS
- framebuffer_object hard requirement (GLContext.cpp:1011): both
  GL_ARB/EXT_framebuffer_object in extension string, no NS_ERROR
  in stderr → passes
- Six missing symbols (EGLImage*, QueryCounter, GetQueryObjecti64v,
  GetInternalformativ): all loaded via fnLoadForFeature →
  MarkUnsupported on failure → optional, not fatal

**Fix deployed 2026-08-13 (pending verification):** setenv moved to
the two earliest entry points — `substitute_init` constructor and
cgl_shim `+load` — both guarded `if (!getenv("GALLIUM_DRIVER"))` so
an explicit export still wins (preserves the softpipe bisect tool).
Bundled with: GC write-barrier store for `sc->view` via
`objc_assign_strongCast` (objc-auto.h:111; verified exported from
10.6 libobjc). In non-GC processes it is a plain store, so it cannot
affect PowerFox attribution; GC-process verification pending.

**Pre-registered predictions (written before the runs):**
1. Control, OLD framework still on guest, `/tmp/subst/test_app`,
   no GALLIUM_DRIVER: GL_RENDERER "softpipe", GL_VERSION "3.3 …".
2. NEW framework, same command: GL_RENDERER "virgl (Apple M4 Pro)",
   GL_VERSION "2.1 …".
3. NEW framework, PowerFox 300s MOZ_GL_SPEW, no GALLIUM_DRIVER:
   +load canary prints GALLIUM_DRIVER=virgl (was "(unset)"); Gecko
   logs renderer virgl, "OpenGL version detected: 210". Window
   outcome branches:
   a. Window appears → compositor running on virgl.
   b. Still no window, virgl in log → blocker is past driver
      selection: candidates are per-call virgl round-trip cost during
      compositor warmup under TCG, or Gecko falling back to basic
      layers for an unrelated reason. Measure before guessing.
   c. New crash → record where; the frontier moved into code that
      never ran before.
   Negative control for the fix itself: if test_app still reports
   softpipe on the new framework, the setenv is still too late — hunt
      an earlier Mesa trigger (e.g. a libOSMesa constructor).

**Session 2026-08-13 (continued) — results, all against the
pre-registrations above:**

- **Predictions 1 and 2 hit exactly.** Old framework bare-env:
  `GL_VERSION: 3.3 (Compatibility Profile) Mesa 24.3.0-devel`,
  `GL_RENDERER: softpipe`. New framework, identical command:
  `GL_VERSION: 2.1`, `GL_RENDERER: virgl (Apple M4 Pro)`, plus
  `virgl_iokit: get_caps capset_id=2 ver=2 size=1408 copy=1384` — the
  iokit winsys initializing for the first time in this chain.
- **Prediction 3's driver half hit:** in PowerFox, canary virgl,
  `OpenGL version detected: 210`, `renderer: virgl (Apple M4 Pro)`,
  GLSL 120, six optional symbol misses as before.
- **Process topology (observed, not inferred):** `powerfox` binary is
  a 24KB stub. Launch chain: gen-1 (pfwatch child) runs early startup
  ~74s, exits rc=0, handoff spawns the real browser instance (orphan,
  ppid 1) which inherits env/stderr. The dock "disappear-restart"
  the user observed repeatedly is this handoff plus Gecko's own
  startup-crash recovery (safe-mode relaunch) after the failed
  startups.
- **Deadlock #1 (fixed):** compositor thread called our swizzled
  `setView:` from `BasicCompositor::TryToEndRemoteDrawing`; shim's
  `[view bounds]` (AppKit, off-main) blocked against the main thread
  holding the display path while waiting in
  `SendFlushRendering` for that same composite. Both threads parked
  30+ min (sample evidence, pids 5326). Fix: no AppKit geometry calls
  off-main — `setView:` stores the view only; resize detection moved
  to flushBuffer using `GL_VIEWPORT` (thread-local GL query, no
  AppKit); `update` no-op'd (it also MakeCurrent'd on the wrong
  thread).
- **Deadlock #2 (worked around, cause unresolved):** next build, the
  compositor blocked in `shim_flushBuffer`'s first-ever
  `pthread_mutex_lock(&sc->lock)` — no live thread held it (all
  sampled threads parked elsewhere; zero CGLLockContext calls that
  run; first acquisition in the context's life). Leading hypothesis
  recorded as UNEXPLAINED. Fix: trylock-with-skip in both flush
  sections (resize skip = one late frame; swap skip = dropped frame).
  In the subsequent run the trylock never fired — lock 0.07ms flat
  across 8337 frames — the anomaly did not recur once the AppKit
  call was gone.
- **`CGLTexImageIOSurface2D` neutralized:** was blind-forwarded to
  real CGL with our shim_ctx token in the ctx argument (real CGL
  dereferences it — crash-in-waiting; it fired one line before a
  silent process death). Now returns kCGLBadContext for shim
  contexts (logged); Gecko falls back to its upload path.
- **MILESTONE: 8337 frames composited** in PowerFox through the full
  substitute→OSMesa→virgl→kext stack. Per-30-frame stats: wall mean
  124–188ms (5–8 fps), submit 38–88ms, transfer 48–84ms, lock
  0.07ms. A window appeared for the first time under the substitute
  (Gecko safe-mode dialog — consequence of the earlier crash loop).
  It was EMPTY: `blit: (no samples)` — the present path's final
  draw never runs.
- **Next frontier (precise):** `shim_present` attaches present_buf to
  the view and calls `setNeedsDisplay:`, but Gecko's `ChildView`
  overrides `drawRect:` and draws its own content — our NSView
  drawRect swizzle never runs for it. Need a main-thread blit that
  does not depend on drawRect (e.g. lockFocus + NSBitmapImageRep
  draw, killtest pattern) — the "blit" timing bucket already exists
  and is instrumented, it just never fires.
- **Unexplained residuals:** (1) gen-3 process mapped our framework
  and reached flushBuffer without printing its `+load`/probe lines —
  contradicts fresh-exec semantics; fork-inheritance is the loose
  hypothesis. (2) The no-holder mutex block of deadlock #2. (3) The
  ~3-min-then-silent-exit pattern of deadlocked generations (Gecko
  hang watchdog suspected, unverified).
- **GC write-barrier fix** (objc_assign_strongCast for sc->view)
  deployed in the same builds; inert in non-GC PowerFox as designed;
  System Preferences (GC process) verification still pending.
- **Final verification run (post trylock + IOSurface stub):** stable
  6 minutes, one process alive throughout, log 16 → 610 lines
  (~40 lines/20s — the 30-frame stats cadence), no restart loop, no
  silent exits, zero resize-skip/swap-skip firings (lock never
  contested), `CGLTexImageIOSurface2D … -> kCGLBadContext` logged
  once and Gecko proceeded. One anomalous earlier launch died
  post-load with no log/crash trace (unexplained, did not recur).
  Session ended with the build deployed and stable; the blit fix is
  the next unit of work.
- **Offered, unused:** a real Mac mini (`ssh macmini`) runs PowerFox
  natively — available for a hardware comparison sample if the
  compositor topology questions resurface.

### 2026-08-13 — session results (predictions 1-2 hit; then two more bugs, both fixed)

**Predictions 1 and 2 hit exactly.** Control (old framework, `/tmp/pf_test2`,
bare env): `GL_RENDERER: softpipe`, `GL_VERSION: 3.3 (Compatibility Profile)
Mesa 24.3.0-devel`. New framework, same command: `GL_RENDERER: virgl (Apple
M4 Pro)`, `GL_VERSION: 2.1`, +load canary `virgl`, `virgl_iokit: get_caps
capset_id=2 ver=2 size=1408 copy=1384` (iokit winsys now actually
initializes). In PowerFox itself: `OpenGL version detected: 210`, `renderer:
virgl (Apple M4 Pro)`.

**Run-3 anomaly resolved — was never a crash.** `PFWATCH: powerfox exited
rc=0` after ~70s, then a successor process (ppid 1) launched: Gecko's
one-shot self-handoff for non-LaunchServices launches (ssh/nohup). The
"disappear for a second and restart" the user saw at the dock is this
handoff, not a crash loop. No crash reports exist for any of it.

**Deadlock 1 — off-main AppKit in shim_setView: (fixed).** Evidence:
`sample` of hung pid 5326 — main thread parked inside
`makeKeyAndOrderFront → _recursiveDisplayAllDirtyWithLockFocus → ChildView
doDrawRect → PaintWindow → PCompositorBridgeChild::SendFlushRendering →
PR_WaitCondVar`; Compositor thread parked inside `RecvFlushRendering →
CompositeToTarget → BasicCompositor::EndFrame → TryToEndRemoteDrawing →
shim_setView:+221`. ABBA: main holds the AppKit display path and waits for
the compositor's flush reply; the compositor calls `[view bounds]` off-main
inside our shim. Fix: **no AppKit geometry off-main, anywhere**. Drawable
size now comes from `p_glGetIntegerv(GL_VIEWPORT)` at the top of
flushBuffer (thread-local, no AppKit, correct by construction —
glViewport is how the app tells GL the drawable size). setView: only
stores the view; shim_update is a no-op (its main-thread
OSMesaMakeCurrent would also have bound the context to the wrong
thread's TLS dispatch). Remaining `[self bounds]` only in
shim_drawRect (main thread by construction).

**Deadlock 2 — CGLLockContext aliased to sc->lock (fixed).** Evidence:
`sample` of next hang (pid 6234) — compositor thread in
`nsChildView::DoRemoteComposition → GLContextCGL::SwapBuffers →
shim_flushBuffer+311 → pthread_mutex_lock → semaphore_wait_trap`;
no thread in the process inside any shim/OSMesa code. Gecko calls
`CGLLockContext` around `-flushBuffer` on the SAME thread
(nsChildView.mm:4188/4195/4364/4598) — our substitute implemented
CGLLockContext/CGLUnlockContext directly on `sc->lock`, the same mutex
flushBuffer takes → self-deadlock on the first flush. These two symbols
are IMPLEMENTED, not forwarded, so they never appeared in any CGL_FWD
census — invisible by construction. Fix: separate `pthread_mutex_t
cgl_lock` in shim_ctx for the public CGL pair; internal `lock` never
exposed. Bounded logging added to CGLLockContext (now visible).

**Result: live render loop.** After both fixes: 417+ frames flushed in
~5 min, viewport 400x128, `ctxcheck: renderer="virgl (Apple M4 Pro)"`,
`swap_mc ret=1` every frame, preswap pixels real (frame 1 zeros, frames
2+ `0xff000000` = opaque black), ~65-80ms mean wall/frame (~12-15 fps
under TCG; submit ~25ms + transfer ~25ms per frame — matches the
per-call cost study), process at 19% CPU (vs 1.2% parked).

**Remaining frontier — presentation to screen.** `shim_drawRect` never
fires (0 samples; Gecko's ChildView overrides drawRect, so the
associated-object present path is unreachable for PowerFox).
Gecko's own present goes through `CGLTexImageIOSurface2D`, which our
substitute forwards to the real GL with the shim token as context —
fails silently, nothing reaches the window. Next: wire present_buf →
screen. Candidate: blit directly in shim_present (main thread, runs
today) via lockFocus/unlockFocus on the view, bypassing drawRect; or
handle CGLTexImageIOSurface2D against the OSMesa context instead of
forwarding. **Needs a visual check first** — the user was watching;
confirm what (if anything) the window shows before designing.

**GC write barrier (bundled, inert in PowerFox).** `sc->view` store now
via `objc_assign_strongCast` (objc-auto.h:111; verified exported from
10.6 libobjc) — view hold is GC-visible in GC processes (System
Preferences), plain store otherwise. GC-process verification pending.
`__objc_imageinfo` flag verified in the binary (`02 00 00 00`).

**Traps recorded.** (1) ps pattern `[p]owerfox/Contents/MacOS/powerfox`
never matches `PowerFox.app/Contents/...` — every kill using it was a
silent no-op; use `[P]owerFox`. (2) `MOZ_GL_DEBUG=1` makes Gecko call
glGetError per GL call — too slow under TCG+virgl for init to complete
in 100s; use `MOZ_GL_SPEW=1` alone (gfxEnv GlSpew, gfxEnv.h:84).
(3) 10.6 `env` has no `-u`. (4) Modern scp needs `-O` against the
10.6 sshd.

### 2026-08-13 late — hardening verified; safe-mode prompt identified

**The 400x128 surface is Gecko's safe-mode prompt** (user-confirmed
visual: dialog chrome visible, content area not composed). All the
killed runs made Gecko offer safe mode; the dialog's content widget is
what the compositor has been rendering all along. The "restart loop"
was crash-recovery + prompt cycles. The ShowModal parking seen in
both samples is this modal dialog.

**Hardening deployed and verified (spew8 run):** flushBuffer's resize
and swap sections converted to trylock-with-skip (contended lock →
drop frame, never block compositor; skip lines would name a busy/odd
lock — none fired in this run, so the earlier first-acquisition block
did not reproduce and stays unexplained, stale-struct/fork-artifact
being the leading hypothesis). CGLTexImageIOSurface2D removed from
the blind-forward list: returns kCGLBadContext for shim contexts
(fired exactly once, 400x128, no crash) so Gecko can take its
fallback; forwarding had passed the shim token to real CGL — a
deref of shim_ctx as CGLContextObj.

**One flake:** a first relaunch (spew7) loaded the framework then
produced nothing and vanished — no crash report, guest healthy,
identical load-time code to the working build. Unexplained; single
occurrence, did not reproduce.

### 2026-08-13 evening — FIRST FULLY RENDERED GECKO UI (milestone)

**The safe-mode dialog renders correctly: text upright, buttons
visible — and the user CLICKED one (Refresh Profile), so input
routing works too.** Pipeline verified end-to-end visually:
virgl render → CGLTexImageIOSurface2D upload (BGRA/RECT honoured,
target=0x84f5 fmt=0x80e1 type=0x8367, glErr=0x0) → composite →
readback → swap → drawRect blit → screen.

Changes that closed it (each verified):
1. **PixelHostingView drawRect swizzle.** The NSView-level swizzle
   never runs under Gecko — ChildView/PixelHostingView override
   drawRect: and per-class dispatch lets the override win (drawptr 0,
   blit no samples, white content). Fix: lazy swizzle of
   `[view class]` at first -setView: (only classes whose drawRect:
   IMP differs from NSView's; one-shot; logged). The actual class is
   **PixelHostingView**, NOT ChildView — resolving from the instance
   is what caught it; a hardcoded objc_getClass("ChildView") would
   have missed. shim_childViewDrawRect: calls through to Gecko's
   original first (it drives the PaintWindow/SendFlushRendering sync
   that produces frames), then blits present_buf over the top.
2. **CGLTexImageIOSurface2D implemented as CPU upload** into the
   current OSMesa context (lock read-only, base address +
   GL_UNPACK_ROW_LENGTH from bytes-per-row, honour caller's
   target/format/type verbatim). Never forward this call — the shim
   token in ctx is dereferenced by real CGL. Cost noted: converts
   Gecko's zero-copy into a per-texture CPU copy; the frame budget
   now carries two full-surface copies (upload + readback).
3. **Vertical flip fixed at the blit via CTM** (one transform, no
   row copy). Content arrived bottom-up: window chrome correct,
   content mirrored top-to-bottom (pure row inversion, not 180°
   rotation). **OSMesaPixelStore(OSMESA_Y_UP, 0) is INERT in this
   Mesa's gallium OSMesa frontend** — falsified by hash-verified
   A/B (image byte-identical with/without; the classic-OSMesa row
   inversion did not carry into the gallium frontend). The inert
   calls were removed to avoid double-flipping if a future Mesa
   implements them.
4. **OSMesaPixelStore crash lesson:** it dereferences the CURRENT
   context — calling before the first OSMesaMakeCurrent NULL-derefs
   (compositor SIGSEGV at OSMesaPixelStore+56, addr 0x38, crash
   report 11:12). Always after MakeCurrent.
5. Why the flip was invisible until now: solid red and the symmetric
   triangle are flip-invariant; text was the first content that
   could reveal it.

**New frontier: the MAIN browser window composites black.** After
Refresh Profile (which restarts Gecko), the main window opened at
viewport 1280×843 and flushed 1000+ frames whose readback is ALL
ZEROS (blit itself working, 7-8ms). The only IOSurface inputs are
strips (1280×27, 15×15, 4×4) — never a window-sized upload. Forcing
`browser.tabs.remote.autostart=false` changed nothing → e10s is NOT
the gate. No content process has ever spawned (no 4th framework
block, no plugin-container crash). Then the process died silently —
no crash report, and (blocks: 1) this generation never even ran the
usual stub handoff.

**Recurring unexplained pattern: silent process death, no crash
report.** Third+ occurrence today (spew7 flake, refresh-generation
death, this one). Suspects unverified: Gecko hang watchdog, exit()
from a failed subsystem.

**Pre-registered next steps:**
1. During a black-window run, `sample` both Compositor and main
   threads: main grinding in layout/JS = first content is SLOW under
   TCG (wait 10+ min before concluding broken); both parked = no
   content arriving, chase the layer manager.
2. Identify which layer manager the main window uses
   (CompositorOGL vs BasicCompositor — the strips-vs-big-surface
   upload asymmetry hints different input paths per surface).

### 2026-08-13 night — black window chased to virgl submission failures

**User corrections accepted and recorded:**
- The e10s-off generation did NOT die — my "alive: 0" ps read was an
  instrument error (mechanism unknown); the process (7759) was alive
  7+ min at 21.9% CPU and never even ran the stub handoff. The
  "silent death #3" claim is RETRACTED; the refresh-era death needs
  re-examination through the same lens. Pattern rule: treat
  single-shot ps emptiness as suspect; PowerFox restarts switch
  processes, and generation gaps mimic death.
- Main-thread sample (7759): startup COMPLETED — event loop running;
  display cycle loops through our shim_childViewDrawRect: correctly
  (call-through → ChildView doDrawRect → PaintWindow →
  SendFlushRendering → parked ~43% of samples waiting for the
  compositor's flush reply; replies flow, frames advance). The
  present machinery is healthy end to end.

**The black window's proximate cause: virgl submission failures.**
`virgl_iokit: 0x6008 submitVirglCommandsEx FAIL 0xe00002c2` —
decoded: 0xe00002c2 = kIOReturnBadArgument (10.6 IOReturn.h).
Counts per run: spew13 (dialog RENDERED) 1080, spew14 1, spew15 98,
spew16 721 (continuous from first flush, back-to-back). The winsys
resets cbuf->cdw on failure, dropping the batch; the next frame
fails again — a persistent failure loop. One kernel-side
executeCommands failure: `ctx=0x11c size=56 FAIL 0xe00002d6` =
kIOReturnTimeout, 11:37:04 (during spew14's window).

**Hypotheses tested and killed:**
- "1MB kext safety cap rejects big batches": winsys command buffer
  max is VIRGL_MAX_CMDBUF_DWORDS = 66560 dwords ≈ 260KB — cannot
  reach the cap.
- "Calls never reach the kext (framework/stale connection)":
  kernel.log shows selector=0x6008 arrivals at 11:55:34 INSIDE
  spew16's failure window with no internal-failure logs — so some
  calls arrive and succeed while others get BadArgument.
- "No window-sized IOSurface upload" (earlier claim): was drawn from
  a 5-line-capped log — instrument error. Fixed: running counter +
  unconditional logging for w>=1000. With correct instrumentation:
  uploads are 1280×27, 15×15, 4×4 only — the original observation
  stands, now on valid evidence.
- e10s: OFF (pref persisted, no plugin-container ever spawned with a
  window open) — not the gate.

**Leading open hypothesis — FALSIFIED (final cross-run check) —
TWO CLAIMS PARTIALLY RETRACTED 2026-08-13 night-2; see the
"BadArgument source identified" subsection below for the replacement.**
Original text preserved so the reasoning trail stays auditable; the
two retracted claims are marked inline.

submit failures are NOT the black-window discriminator. FAIL counts
per run: spew13 (dialog RENDERED CORRECTLY) 1080/1565 lines;
spew14 1; spew15 98; spew16 761; spew17 (empty-frame-guard build)
3532/4343 — 81% of log volume. The failures are tolerated churn:
the winsys returns -1, Mesa drops the batch, the next frame
rebuilds. Rendering succeeded in spew13 WITH 1080 of them, and the
current run's machinery is healthy (blit-skips, strips, viewports)
under 3532 of them. **[RETRACTED 2026-08-13 night-2: this
"tolerated churn" framing is the load-bearing premise that fell.
It assumed the failures were spurious — and that assumption was
never tested against the size of the failing batches. Under the
new hypothesis, main-window compositor batches exceed 4096 bytes
continuously, so every draw is dropped and only clears survive,
which is exactly the all-zeros readback with glError=0x0 observed
on the main window. spew13's dialog rendered because the dialog's
batches were under 4096; the main-window draws were not in the
same population. The churn was only ever "tolerated" on surfaces
small enough to land in the inline path.]** Which kext path returns
BadArgument remains unidentified (all visible paths excluded by the
winsys's call shape) **[RETRACTED 2026-08-13 night-2: IDENTIFIED.
The path is the implicit `return kIOReturnBadArgument;` at the
bottom of case 0x6008 in `externalMethod`, fired when IOKit delivers
the batch via `structureInputDescriptor` (≥ 4096 bytes per IOKit's
inline/descriptor boundary), leaving `args->structureInput` NULL —
the old `if (args->structureInput && …)` gate at the top of the
case skipped both branches and fell through. Killtest never saw it
because its largest batch was 487 dwords ≈ 1948 bytes, always
inline. Fix in flight: uncommitted +48/-8 diff in
`FB/VMVirtIOGPU.cpp` handles the descriptor path.]** — a
correctness mystery worth solving for TCG performance (every failed
submit is lost work), but not the black cause
**[RETRACTED 2026-08-13 night-2: see replacement hypothesis below;
the "not the black cause" conclusion rested on the retracted
"tolerated churn" premise]**.

**Empty-frame guard deployed and verified (spew17):** shim_blit
skips frames whose first pixel has alpha==0 (NSCompositeCopy would
otherwise paint transparent-black over Gecko's own software
painting — the observed white-then-black). User-confirmed: window
now stays WHITE (view default background; Gecko paints nothing via
software either — "all white, no toolbar buttons"). blit-skip
lines fire as designed.

**The frontier, stated precisely:** the main window's compositor
layer tree stays empty (frames RGBA(0,0,0,0); no window-sized
IOSurface upload; strips upload fine; startup complete; main
thread healthy in the event loop driving the display cycle through
our swizzle; e10s off; single process). Small surfaces (the
400×128 dialog, 1280×27/15×15/4×4 strips) flow end-to-end. What
differs about the main window's content pipeline upstream of the
compositor is unidentified.

**Pre-registered next steps (in order):**
1. Gecko-side: determine the main window's layer manager and why
   it submits no transactions — via prefs/env that force
   software-only layers (`layers.acceleration.disabled=true`,
   `layers.offmainthreadcomposition.disabled=true`) as a
   diagnostic, watching for ANY painted output (the software
   fallback would paint via CGContext and the white window would
   gain content — with the empty-frame guard, it now CAN).
2. Winsys FAIL instrumentation (ctx_id + cdw on the FAIL line,
   Mesa rebuild) — perf investigation, separate from the black.
3. Check the UTM host debug log for virglrenderer errors in the
   failure windows (the one Timeout hints at host stalls).

**Next step — CORRECTED (lockFocus was a falsified route, proposed in
error three times above):** the presentation fix is a ChildView-class
drawRect swizzle, not a new draw mechanism. lockFocus/unlockFocus
deadlocks on 10.6 (AppKit re-enters the run loop inside it —
architecture-3d.md #2, falsified-routes list); the ONLY proven route
is drawing inside the view's own drawRect:. The reason our drawRect
path never ran is per-class dispatch: the swizzle sits on NSView, and
Gecko's ChildView overrides drawRect:, so Gecko's implementation wins
and ours never fires (drawptr 0, blit no samples, white content —
visually confirmed via user screenshot: "PowerFox Safe Mode" window,
system chrome rendered, content blank white; note the readback holds
opaque BLACK, so not even a wrong blit was reaching the screen).
Implemented: lazy swizzle of `[view class]` at first -setView:
(XUL not mapped at +load; only classes whose drawRect: IMP differs
from NSView's are patched; shim_childViewDrawRect: calls through to
Gecko's original first — it drives the PaintWindow/
SendFlushRendering sync that produces fresh frames — then blits
present_buf over the top). Blit-only (no call-through) is the
fallback variant if call-through-first misbehaves. Prediction: the
safe-mode prompt's content renders (text + buttons); log shows
"drawRect swizzled on ChildView", drawptr lines, and blit samples.
The kCGLBadContext on CGLTexImageIOSurface2D remains its own datum:
Gecko has a native present path it expects to work; the drawRect
blit bypasses rather than fixes it — revisit if native compositing
is ever wanted.

### 2026-08-13 night-2 — BadArgument source identified; submit failures re-entered as black-window candidate

**This subsection supersedes the "Leading open hypothesis —
FALSIFIED" claims above.** It is written before the verification
run, per the project rule that the ledger must not inherit a
wrong version of why something worked.

**Path identified.** `0x6008 submitVirglCommandsEx FAIL 0xe00002c2`
originates from the implicit `return kIOReturnBadArgument;` at the
bottom of `case 0x6008` in `VMVirtIOGPUUserClient::externalMethod`
(`FB/VMVirtIOGPU.cpp` around the case 0x6008 block). IOKit switches
from inline to descriptor delivery at the 4096-byte boundary: inputs
< 4096 bytes are copied into kernel memory and exposed as
`args->structureInput`; at/above 4096 the kernel passes
`args->structureInputDescriptor` and leaves `structureInput` NULL.
The old case body required `args->structureInput` non-NULL to enter
either branch, so any batch ≥ 4 KB skipped both branches and fell
through to the trailing BadArgument. No log line fired — the
failure was silent on the kext side; the winsys saw -1, reset
`cbuf->cdw`, and dropped the batch.

**Killtest never triggered it.** Killtest's largest batch was 487
dwords ≈ 1948 bytes (LEDGER per-call cost study), always inline.
That is why the failure path was never exercised in any verified
run on the killtest, and why spew13's dialog (small compositor
surface, 400×128) rendered correctly despite logging 1080
BadArgument lines: the dialog's own batches were inline. The
1080 failures were on the OTHER calls of the same run —
strip-sized transfers, etc. — never on the dialog's draw.

**Replacement hypothesis for the black window.** Under the
new framing, the main window's compositor batches exceed 4096
bytes continuously (the observed all-zeros readback is a
window-sized 1280×843 surface with no content), so every main-
window draw hits the descriptor path and is dropped, while clears
and small strips (1280×27, 15×15, 4×4 — all under 4 KB inline)
flow normally. This matches the exact symptom signature: black
content area, strips upload fine, healthy main-thread event loop,
no crash, e10s off. The "tolerated churn" that the prior
falsification observed was real but was being measured on the
wrong population — small surfaces where the churn is genuinely
harmless. The main window was never in that population.

**Fix in flight.** +48/-8 uncommitted diff in
`FB/VMVirtIOGPU.cpp` adds a descriptor-path branch that calls
`prepare()`/`map()`/`getVirtualAddress()`/`release()`/`complete()`
and forwards to `submitVirglCommandsEx` with the descriptor's
authoritative `getLength()`. Pass condition pre-registered below.

**Pass condition is PIXELS, not silence.** `SUBMIT_3D` returns
`0x1100` unconditionally (LEDGER rule: 0x1100 means "QEMU parsed
it", never host accepted). A descriptor path that maps without
prepare(), reads the wrong length, or reads before the data is
complete produces garbage indistinguishable from success. The
BadArgument storm dropping to zero only proves the early return
is gone. ONLY the main window showing content proves the commands
arrived intact.

**Logging on the new branch (it has never executed).** Per
reviewer directive: log which delivery shape each call took
(inline vs descriptor) and the length actually read against the
length declared. A mismatch there is the most likely first-run
defect and is invisible otherwise. Both logs gated to first N
calls per project IOLog discipline; a self-check `MISMATCH` line
fires prominently if `dsize == 0` or
`(structureInputSize != 0 && structureInputSize != dsize)`.

**Deployed-kext check — OBSERVED STATE, not the pre-registered
comparison.** Guest was rebooted mid-session (QEMU pid 43170 →
69142; uptime 3m at first ssh). On reboot, `/System/Library/
Extensions/VMQemuVGA.kext` was ABSENT — not at the canonical
path, not at `/Library/Extensions/`, not in `kextstat`. Boot log
shows no VMQemuVGA load attempt (nothing to load); `IONDRVSupport`
is the active framebuffer, almost certainly driving virtio-vga-gl's
VGA-compat plane (basic VGA, no acceleration). ssh host key was
re-added to known_hosts (host key rotated since last session).

This moots the "deployed == HEAD" pre-registration: there is no
deployed kext to compare. The relevant baseline is now "guest has
never run any version of this code in this boot state." Host build
md5 (with the descriptor-path diff) is
`70328ed492cc322546c3790d02224e7b`; the previously-noted prior
md5 `d4959ba77634b3bbffa06652c932a457` was a different build state
that no longer exists in `build/Release` (overwritten by the
rebuild).

**Why the kext was absent — RESOLVED (user statement, not
inferred).** The user deleted the kext via the recovery procedure
documented in `.claude/rules/build-install.md` lines 154-157
(`V=/Volumes/MacintoshHD; sudo rm -rf "$V/System/Library/
Extensions/VMQemuVGA.kext"; sudo rm -rf "$V/System/Library/Caches/
com.apple.kext.caches"; sudo touch "$V/System/Library/Extensions"`)
because the prior build was not booting. This was communicated
twice via the IDE selection of those exact lines and once in plain
text; I missed the signal both times initially and wrongly recorded
the absence as an unexplained residual. Retracting that framing.

**Implication — prior build broke boot (NEW ITEM, not residual).**
The kext committed at HEAD (without the descriptor-path diff) was
installed across many sessions (LEDGER: RED WINDOW, killtest,
PowerFox safe-mode dialog rendered). At some point after those
verifications it stopped booting, badly enough that the user had
to use the slclean recovery procedure. The new build (with the
descriptor-path diff) **booted cleanly on the first try after
install** — kextstat confirms load, md5 matches, WindowServer
servicing framebuffer at 1680×1050. Whatever was breaking boot
before is not present in this build, OR was in code my changes
happened to perturb. Not claiming the descriptor-path fix *caused*
the boot recovery — that's untested. But the boot recovery is a
real observation that needs explaining: identify what in HEAD's
pre-descriptor-diff state was breaking boot, separate from the
descriptor-path question. This is a new open item, not a residual
on the absence.

**`/tmp` self-cleans on reboot — normal 10.6 behavior.** Recorded
because I treated the missing killtest_shim / Mesa libs / substitute
framework as evidence of "guest was wiped" — wrong. `/tmp` clears
on reboot on this guest; re-staging `/tmp` from the host build
artifacts is the normal pre-test step, not a recovery operation.

**Pre-registered predictions (written before the run, REVISED for fresh-install baseline):**

1. **[RETRACTED — moot per observed-state note above]** Fresh HEAD
   build md5 comparison — no deployed kext to compare against.
2. **[RETRACTED — moot]** Deployed-on-guest md5 comparison — same.
3. **Install succeeds on fresh `/S/L/E/`.** `kextcache -system-caches`
   builds the boot cache cleanly; `kextutil -n -t` (or, on this
   guest, the equivalent 10.6 tool) reports no validation errors;
   `Startup/Extensions.mkext` and `kernelcache_x86_64.<hash>` exist
   with fresh mtime + plausible size per build-install rules.
4. **After reboot, kext loads.** `kextstat | grep VMVirtIO` shows
   the kext loaded; kernel.log shows the kext's `start()` log
   lines; `md5 /S/L/E/VMQemuVGA.kext/Contents/MacOS/VMQemuVGA`
   equals the host build md5 `70328ed492cc322546c3790d02224e7b`.
5. **Killtest run (control) unchanged.** All batches are inline
   (< 4096 bytes — killtest max was 487 dwords ≈ 1948 bytes). The
   INLINE-path log fires 20 times then suppresses. DESCRIPTOR-path
   log does NOT fire. MISMATCH log does NOT fire. BadArgument log
   does NOT fire. Rendering byte-identical to prior verified
   killtest output.
6. **PowerFox main-window run (the test).** DESCRIPTOR-path log
   fires for some calls (the main window's compositor batches are
   predicted ≥ 4096 bytes continuously). INLINE-path log also
   fires for smaller surfaces (strips, etc.). BadArgument count
   drops from the thousands-per-run baseline observed in spew13-17
   to zero. MISMATCH log does NOT fire (dsize agrees with declared
   size — first-run proof the length-read-vs-length-sent invariant
   holds; if it fires, that's the new frontier).
7. **Pass condition (pixels):** main window content area shows
   Gecko UI (toolbar, page content — not the safe-mode dialog
   which is the known-rendering case, the actual browser window).
   If the window is still white/black with alpha==0 readback, the
   fix landed but did not change the symptom — the "tolerated
   churn on the wrong population" hypothesis is wrong, and the
   black window has a different upstream cause (return to the
   Gecko layer-manager diagnostic, pre-registered step #1 above).
8. **If pixels appear but are visually wrong** (corrupted, partial,
   off-by-one), suspect the descriptor length or completion
   ordering — the MISMATCH self-check is designed for this.

**Open question the fix does not address.** Why the main window's
compositor produces batches ≥ 4 KB continuously while the safe-
mode dialog produces batches < 4 KB is unidentified. The fix lets
the batches through; it does not explain their size distribution.
That is Gecko layer-manager territory (pre-registered step #1) and
remains relevant regardless of the pixel outcome — the size
distribution tells us which layer manager is in use.

### PowerFox architecture: x86_64-only

Guest's PowerFox.app confirmed x86_64-only (main binary + plugin-container
+ XUL). No i386 slice. Mesa's x86_64 cross-build matches. No i386 concern
for the target workload.

Flurry (via System Preferences) is a separate nice-to-have that would
need i386 Mesa or forced x86_64 launch. Not blocking PowerFox.

### Open: Flurry needs CGLQueryRendererInfo + CGLDescribeRenderer

Flurry's 5 CGL symbols include CGLQueryRendererInfo and CGLDescribeRenderer
(capability query). These determine what GL features Flurry enables. Need
real implementations, not stubs returning zero. The prior "Gecko doesn't
call them" framing doesn't apply to Flurry — but Flurry is secondary to
PowerFox.

## Per-call cost study — 2026-08-12 (CONFOUNDED — pending A/B)

### What was measured

Bumped `SUBMIT_INSTRUMENT_LIMIT` 20 → 200 and added `cmd` + `call_ns` to the gated EXIT OK log. Captured submits 1-200 across ~40 frames of killtest steady state. Per-cmd-type counts and call_ns:

| cmd type | meaning | count | typical call_ns |
|---|---|---|---|
| 0x105 | TRANSFER_FROM_HOST_2D | 82 | ~5 ms |
| 0x104 | TRANSFER_TO_HOST_2D | 81 | ~16 ms |
| 0x106/0x101/0x103/0x102 | various resource ops | 7-9 each | ~16 ms (resource ops) / ~3-16 ms (unref) |
| 0x207 | SUBMIT_3D | **4** | ~16-26 ms |
| 0x204/0x200/0x206 | CTX_CREATE / RESOURCE_CREATE_3D / TRANSFER_FROM_HOST_3D | 2-5 | ~3 ms |
| 0x108/0x109 | GET_CAPSET_INFO / GET_CAPSET | 4 each | ~2-38 ms |

### Conclusion 1 — "dominant per-frame commands are 2D, not 3D" — PENDING (likely false)

**The arithmetic contradicts the claim.** 82 reads + 81 uploads matches the winsys's per-frame count of 2 transfer_get + ~1.6 transfer_put per frame. But the same winsys count says 2 submit_cmd per frame, which should be ~82 SUBMIT_3D over the session. Only 4 appear.

The most plausible explanation is the one offered as hypothesis in the same breath: `submitVirglCommandsEx` (kext handler for winsys selector 0x6008) writes the virtqueue directly, bypassing `submitCommand`'s poll loop and its instrumentation. If that's the case, the table above is a census of the LOGGED path, not a per-frame census. "3D commands are rare" is unsupported; they may be entirely absent from the sample.

**Not recorded as a finding.** Same shape as the prior caps-contaminated A/B: a measurement whose sampling boundary wasn't established before the numbers were interpreted. Trace `submitVirglCommandsEx` before drawing any conclusion about which commands dominate.

### Conclusion 2 — per-call costs may be measuring the instrumentation, not the call — FALSIFIED 2026-08-12

**A/B run with system settled:**

| Setting | wall | submit | transfer | fps |
|---|---|---|---|---|
| limit=200 (IOLog on) | 127 ms | 52 ms | 33 ms | 7.5 |
| limit=0 (IOLog off) | 130 ms | 55 ms | 32 ms | 7.3 |

All metrics within 3% across A/B — essentially identical. IOLog-to-serial-port contribution to per-call cost is negligible. **Per-call numbers stand** (~5 ms readback, ~16 ms upload, ~26 ms submit_3d are actuals, not artifacts).

Earlier initial A/B (187 ms with limit=0) was contaminated — guest had been up only ~2 mins, load average 11.64, kextd rebuilding caches + mds/mdworker indexing. With system settled (load 2.52, kextd/mds/mdworker all 0.0% CPU in S state), both A and B converge to ~127-130 ms. Lesson re-confirmed: **system-settle precondition is not optional.** Rules already call this out.

Cost clustering on ~16 ms / ~26 ms / ~3 ms across semantically unrelated commands is still real and still unexplained — but it's not IOLog. Likely an underlying scheduling/quantization effect on this transport (TCG scheduler tick, virtqueue transit floor, MMIO doorbell cost). Real per-call work, real overhead, just shared across call types rather than command-specific.

### Open items (reprioritized)

1. ~~IOLog gate A/B~~ — DONE, falsified. Per-call data trusted.
2. **Trace `submitVirglCommandsEx`** — still pending. Determines whether 3D commands bypass `submitCommand`'s instrumentation. With per-call data now trusted, this settles whether the 3D submit cost is even being measured.
3. **Redundant transfer_get** — ~5 ms, correctness cleanup not lever. Defer.

### Solid: redundant transfer_get downgraded to ~5 ms (not ~26 ms)

The one conclusion from the per-call data that survives both confounds: the redundant `transfer_get` is a `0x105` (TRANSFER_FROM_HOST_2D), in the cheapest cluster. Eliminating it saves ~5 ms against a 127 ms wall — under 4%. **Demoted from optimization lever to correctness cleanup.** Still worth doing for hygiene, no longer on the critical path.

## IOSleep spin — 2026-08-12 (verified)

### Change

`submitCommand`'s poll loop unconditionally called `IOSleep(1)` per iteration. Under TCG, `IOSleep(1)` blocks until the next scheduler tick — measured at ~10 ms per call on this guest (vs ~1 ms on real hardware). With 5 submitCommand calls per killtest frame, the IOSleep floor alone was ~50 ms/frame.

Replaced with bounded spin: first 10 iterations use `IODelay(20)` (~200 µs total busy-wait), then fall back to `IOSleep(1)` for the remaining iterations. Added `poll_iter` to the gated EXIT OK log to expose where each submit breaks out.

Commit `b414425`. Drain fix and spin are independent — drain runs at submitCommand entry, spin runs in the poll loop after publish. Both touch the used-ring polling path; landed separately on purpose.

### Verified (n=108 frames post-warmup)

| Metric | Before | After | Delta |
|---|---|---|---|
| wall | 347 ms | **127 ms** | **−220 ms (−63%)** |
| submit | 108 ms | 52 ms | −56 ms |
| transfer | 59 ms | 33 ms | −26 ms |
| fps | 2.9 | **7.5** | +4.6 fps |

Rendering byte-identical: `RGBA(26,26,31,255)`, `"virgl (Apple M4 Pro)"`.

### Mechanism (poll_iter=0)

Every submit returns `poll_iter=0` — the spin's first `IODelay(20)` is already enough; the device's response is visible in under 20 µs. The host was never the bottleneck. The IOSleep(1) was waiting for the next scheduler tick for no reason.

Pre-registered prediction was ~50 ms wall saving (5 calls × ~10 ms IOSleep floor). Actual wall saving is 220 ms. Submit-time drop (56 ms) matches the prediction. Transfer drop (26 ms) is explained by `transfer_get` routing through `submitCommand` internally (control-queue transit picks up the same spin benefit).

Remaining ~140 ms of the wall saving is **unattributed** — recorded as such, not as scheduler overhead. The rules warn about the convenient-explanation shape. Part is directly accounted for: `transfer_put` happens during the app's draw calls before T1, so at least one of the five submitCommand calls sits inside the unmeasured span and picked up the same ~28 ms per-call saving. That covers maybe a fifth of the gap. The rest is genuinely unknown until T0 lands (see open items).

### Strategic shift — call-count reduction is now the dominant lever

If the host completes in <20 µs but per-call cost is still ~26 ms, then essentially all of the remaining per-call time is **guest-side**: Mach trap into the kernel, memory-descriptor setup/teardown, virtqueue descriptor management, MMIO doorbell exit. Nothing host-side helps. At 4–5 calls per frame that's roughly the entire 127 ms budget.

Two implications:

1. **The readback-side items (IOSleep already done; redundant transfer_get) become the entire remaining surface.** Per-frame wall is now bounded by `call_count × per_call_guest_overhead`. Reducing call count is the only way to break below ~25 ms/frame at this transport's per-call cost.

2. **The redundant transfer_get is the leading candidate.** Worth ~26 ms on its own at the measured per-call cost. Evidence already in hand: two full-surface reads of the same resource per frame, with a flush between them — most likely virgl_resource_transfer_map discovering the resource is still referenced. Eliminating it drops 1 call from the steady-state 5, a 20% call-count reduction for a ~20% wall reduction.

### Open items (updated priority)

1. **T0 at the top of the render loop** — CLOSED 2026-08-12 (commit `6d737f3f285` in Mesa-VirGL). Within-frame unmeasured span is **<1 ms in steady state** (0.24–0.93 ms across frames 2-9). The "wall − (submit + transfer + lock)" gap that had been called the unmeasured span is NOT inside the frame — it's between frames (AppKit display cycle, NSTimer dispatch, drawRect), outside our optimization surface. Killtest's draw-call encoding is essentially free; all 5 submitCommand calls are inside submit/transfer timing. Call-count reduction confirmed as the dominant lever.
2. **Redundant transfer_get** (now the dominant lever). ~26 ms available, evidence points clearly. Investigate why virgl issues 2 full-surface reads of the same resource with a flush between them.
3. **Cursor smoothness** (pushed below call-count work). The 60 Hz throttle test is diagnostic, not ship config — confirms pull-vs-push but degrades everything else. The userspace dirty-rect helper via `[NSEvent mouseLocation]` is the architectural fix worth prototyping, but only after the frame budget is understood via T0.

---

## Session summary — 2026-08-10/11

### Arc

Starting state: 3D transport proven (probeTransport3D), softpipe verified
on 10.6, winsys scope research-complete, ATTACH_BACKING probe pre-registered
but not run.

Ending state: **full 3D stack verified end-to-end.** Mesa-driven `glClear` +
`glReadPixels` through virgl_iokit_winsys returns byte-exact pixels on 10.6
via UTM's embedded virglrenderer. The path from guest GL call to host GPU
pixels is proven.

### What was built (in order)

| Increment | What | Repo | Commit | Verified how |
|---|---|---|---|---|
| ATTACH_BACKING probe | Selector 0x5000: two-phase userspace-memory ATTACH_BACKING proof | VMQemuVGA | `0455ac9` | 4096/4096 dwords match position-dependent pattern, 5-segment scatter list, wiring held across guest write between transfers |
| Pre-registrations | Bisect principle, wrong-colour-vs-nothing, get_caps-must-be-real, submit_cmd-stays-sync, cmd-buffers-by-SUBMIT_3D | VMQemuVGA | `c5ebd2e` | Written before implementation, as required by ground rules |
| Increment A | 10 kext selectors (0x6000-0x6009): ctx create/destroy, resource create, attach/detach backing, unref, capset info/get, submit, ctx-attach-resource | VMQemuVGA | `6d9a278` | probe_winsys_selectors_test: CTX_CREATE→RESOURCE_CREATE_3D→ATTACH_BACKING→CTX_ATTACH_RESOURCE→CREATE_OBJECT+SET_FB+CLEAR+NOP→TRANSFER_FROM_HOST_3D, both clear colours byte-exact (0xff996633 and 0xff6633cc) |
| Caveats | 0x1100 diff-target is conditional (silent rejection only); CTX_ATTACH_RESOURCE unconditional in first slice, watch at volume | VMQemuVGA | `9671427` | — |
| Increment B | virgl_iokit_winsys: 4 files in Mesa-VirGL src/gallium/winsys/virgl/iokit/, 25-entry vtable, inline_sw_helper.h "virgl" driver name, meson wiring | Mesa-VirGL | `c703f8fb910` | libOSMesa.8.dylib links clean (19.9 MB), virgl_iokit_winsys_wrap symbol present |
| Increment C | cmd_buf/cmd_size limit fix (256→4096); Mesa-driven clear+readback milestone | VMQemuVGA | `63bfd45` | GALLIUM_DRIVER=softpipe PASS + GALLIUM_DRIVER=virgl PASS, same binary, same dylib, identical byte-exact results |
| Architecture doc | Traps generalised, status table updated, next milestone noted | VMQemuVGA | `ddb795a` | — |

### Bugs found and fixed during the session

1. **VIRGL_OBJECT_SURFACE = 9 (wrong) → 8 (correct).** Enum counts from
   NULL=0: NULL, BLEND, RASTERIZER, DSA, SHADER, VERTEX_ELEMENTS,
   SAMPLER_VIEW, SAMPLER_STATE, SURFACE=8. Probe binary inlined 9.
   Host returned "Illegal command buffer 329985" (= VIRGL_CMD0(1,9,5)).
   Host-side only; kext saw 0x1100.

2. **VIRGL_CMD0 macro bit shifts wrong.** Real: `((cmd)|(obj)<<8|(len)<<16)`.
   Probe had obj<<16, len<<24 (off by 8 bits). Same symptom.

3. **attachVirglResource (selector 0x3003) is a stub.** "For now, just log
   success" — never sends CTX_ATTACH_RESOURCE. virglrenderer requires it
   before SET_FRAMEBUFFER_STATE. Added 0x6009 ctxAttachResource that
   actually sends the command.

4. **submitVirglCommandsEx used withAddress (alias) instead of withBytes (copy).**
   Switched to IOBufferMemoryDescriptor::withBytes to match probeTransport3D's
   proven path at VMVirtIOGPU.cpp:3866.

5. **submitCommand cmd_buf 256-byte limit.** Two separate 256-byte gates:
   the buffer allocation (m_cmd_buf = 256) AND the parameter validation
   (`cmd_size > 256 → kIOReturnBadArgument`). Mesa's 256×256 BGRA resource
   produces 30+ scatter-list entries → 392+ byte ATTACH_BACKING command.
   Both gates silently rejected it. Probe's 64×64 resource (5 entries, 68
   bytes) never hit either gate. Fixed: both → 4096, plus defensive
   overflow check before memcpy.

### Durable rules extracted

1. **0x1100 means "QEMU parsed it", never "host accepted it."** Three known
   instances: SUBMIT_3D, VIRTIO_GPU_FILL_CMD size mismatches,
   RESOURCE_CREATE_3D (QEMU discards virgl_renderer_resource_create's
   EINVAL). Generalised from the older "SUBMIT_3D always returns 0x1100"
   note. See LEDGER Increment A section + architecture doc traps.

2. **cmd_buf/cmd_size limits are the same family as 0x1100 traps.** A
   buffer-size limit that silently rejects commands produces "success
   code but wrong outcome" — same shape, same difficulty to diagnose.
   The defensive overflow check now returns kIOReturnNoMemory rather
   than corrupting.

3. **The probe's resource size determines what it tests.** A 64×64
   resource (5 scatter-list entries, 68-byte command) tests the basic
   mechanism. A 256×256 resource (30+ entries, 392-byte command) tests
   the capacity limit. Probe design should deliberately exercise the
   real-world case, not just the minimal case.

### What the clear proves and does not prove

**Proves (clear + plain triangle + textured triangle together):**
context creation, resource creation with userspace backing, surface
object creation, framebuffer binding, command submission, both transfer
directions (TO_HOST and FROM_HOST), shader compilation (GLSL → TGSI →
GLSL → Metal — three hops, all verified), vertex buffers (transfer_put
with real vertex data), DRAW_VBO, vertex element state, **texture
creation (SAMPLER_VIEW bind), texture data upload (transfer_put with
pixel data), sampler state objects, GLSL texture2D() sampling, UV
interpolation, multiple concurrent resources (color RT + VBO + texture
= 3 live).**

**Does not prove (known coverage gaps — volume problems, not
structural):**
- **Resource reuse across frames.** Resources are created and
  destroyed in one shot. The resource cache (which vtest has but iokit
  skips — LEDGER: deferred) and resource_reference refcounting are
  untested under sustained use.
- **ID allocator + backing table stress.** The 64-entry
  `m_user_backings[]` table and the `m_next_user_resource_id` counter
  (starting at 0x100, wrap at 0xFFF8 = ~65k resources) have only been
  exercised with 3 concurrent resources. The project's pattern is
  that first-time-at-volume reveals always-broken-but-never-reached
  defects — these are candidates.

### Next milestone

**~~Triangle, not cgl-shim.~~ DONE — 2026-08-11.** Triangle PASS on
both softpipe and virgl (Mesa-VirGL commit e314f2a75a5). Shaders
(GLSL → TGSI → GLSL → Metal), vertex buffers (transfer_put), and
DRAW_VBO all verified end-to-end. The guest GL stack is genuinely
proven. **The cgl-shim is now plumbing.**

### CGL shim — VERIFIED — RED WINDOW on 10.6 guest — 2026-08-11

Shim dylib compiles, loads, and produces visible rendered output on
the 10.6 guest's display. The window shows the correct clear colour
RGBA(204,51,51,255) = (0.8, 0.2, 0.2, 1.0).

**Full path (verified visually):**
```
glClear (Mesa) → glFinish + glReadPixels (force readback) →
buffer swap + OSMesaMakeCurrent rebind →
performSelectorOnMainThread setNeedsDisplay: →
NSView drawRect: (swizzled) → NSBitmapImageRep drawInRect →
visible pixels on screen
```

**Presentation mechanism (settled after five approaches):**
- `dispatch_async` + `NSBitmapImageRep drawInRect` → no visible output
- `lockFocus` / `unlockFocus` → deadlock
- `CGImageRef` + `CGContextDrawImage` → no visible output
- `CALayer` (`setWantsLayer:YES`) → hangs AppKit (called before view
  is in a window)
- **`performSelectorOnMainThread` + `setNeedsDisplay:` + `drawRect:`
  swizzle** → **WORKS.** Runs in NSDefaultRunLoopMode where the
  view's graphics context is valid.

CGS surface hypothesis disproven: the original `initWithFormat:`
creates a real CGL context, but drawRect output composites correctly.
No surface occlusion.

**Performance:** ~10 fps with softpipe, ~10 fps with virgl at 800×600
(both include full-framebuffer readback via glReadPixels — expected
cost under TCG). Fence-based async would eliminate the synchronous
readback.

**Killtest via NSOpenGLContext — VERIFIED VISUALLY — 2026-08-11:**
Rotating triangle through the full NSOpenGLContext → shim → Mesa →
virgl → host GPU → drawRect path. 100 frames sustained at ~9-10 fps,
zero errors. User confirmed visible animated triangle on the guest's
display. Exercises: repeated `-flushBuffer` with swap-and-rebind per
frame, `glReadPixels` readback per frame, coalescing path, drawRect
presentation at 800×600 (469-page scatter list through the per-call
allocation path in submitCommand).

**Still untested (next):**
- Resize under load: bounds-check in flushBuffer reallocates, but
  nobody has resized the window during animation.
- Multi-context: registry supports 16 contexts, but only one has
  ever been live at a time.
- `CGLEnable` through the interpose layer: shim_CGLEnable no-ops
  for shim tokens, but no application has called it.

Commits: Mesa-VirGL 96d1a68dfb9 (shim), c16f5cb2fee (coalescing),
52f4d10f00c (drawInRect + smoke test), ef2f57470df (drawRect swizzle —
RED WINDOW), 3fa2461daa8 (killtest_shim). VMQemuVGA fb27c32 (per-call
allocation for large ATTACH_BACKING).

### Unexplained residuals

None active. The "stuck at boot" after the first kext install was a
false alarm — slow boot from the no-caches development configuration,
not a kext regression (LEDGER: boot-stall false alarm).

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

**Superseded 2026-08-10 through 2026-08-12 — transport proven; original
text preserved in Superseded.** The four steps below were delivered:
explicit 3D context + clear + read-back verified byte-exact (Increment C,
2026-08-10: Application → OSMesa → Gallium → virgl → virgl_iokit_winsys →
user client → virtio-gpu → virglrenderer → ANGLE → Metal → GPU), userspace
memory backing verified (ATTACH_BACKING probe, 2026-08-10), GL routing
verified end-to-end through the CGL shim (2026-08-12). **The current
active task is the capability-gating/coupling probe** — see the top entry
(2026-08-14 personality diff) with its pre-registered fix order and probe
design.

---

## Open

- ~~**ATTACH_BACKING with userspace memory — OPEN, the one structural
  unknown before the IOKit winsys.**~~ **Answered 2026-08-10 — VERIFIED;**
  see the dated section "ATTACH_BACKING-with-userspace-memory probe —
  VERIFIED — 2026-08-10". The probe ran as pre-registered — all four
  design constraints satisfied (owning-task capture via `initWithTask`,
  deliberately unaligned buffer with partial first/last pages, persistent
  `prepare()` with `complete()` at teardown, existing-selector reuse) —
  and passed: 5 segments walked matching `getLength()` exactly,
  4096/4096 dwords round-tripped `i ^ 0xA5A5A5A5`, all six commands
  `0x1100`. The pre-registered known limit stands: a single quiet-guest
  buffer does not prove lifetime wiring under memory pressure. The full
  pre-registration text is preserved in Superseded.

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
  `VMVirtIOFramebuffer::probe`. See "Superseded" section.
- ~~**3D beyond capsets.** Nothing downstream of `enable3DAcceleration`
  has executed on a meaningful path. Expect novel failures, not
  regressions.~~ **Superseded 2026-08-10/12** — downstream execution is
  verified on per-process paths: Increment C (Mesa virgl clear + read-back
  byte-exact), the CGL shim end-to-end (2026-08-12), real applications
  (PowerFox, per the 2026-08-14 entry). What remains open is the
  WindowServer-reachable half — the coupling probe (top entry) and the
  `functional_3d` pivot.

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
    **Still open — now tracked in the 2026-08-14 personality diff** (finding
    3: three nubs, three values, plus the `"com.apple.kpi.iokit"` hygiene
    item; reconciliation is pre-registered as pre-experiment step 3 in the
    fix order). Reconcile there, not here.
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

- **(2026-08-09 → 2026-08-10) ATTACH_BACKING-with-userspace-memory as the
  Open "one structural unknown before the IOKit winsys."** Pre-registration
  preserved from Open: four design constraints — (1) owning-task capture
  via `initWithTask`, not `current_task()` (command-gate routing makes
  `current_task()` the kernel task and `withAddressRange` silently
  describes the wrong address space); (2) deliberately unaligned `malloc`
  buffer expecting multi-segment scatter with partial first page
  (Mesa `align_malloc(size, 64)` starts mid-page); (3) `prepare()` before
  `getPhysicalSegment()`, `complete()` only at teardown — Mesa writes
  between transfers; (4) extend existing selectors before adding. Known
  limit pre-stated: a single quiet-guest buffer proves the mechanism, not
  lifetime wiring under memory pressure. Expected wiring-failure mode:
  wrong bytes, not error codes; `nr_entries == 1` on an unaligned buffer
  would make a pass meaningless. **Superseded by:** the probe passing
  2026-08-10 — all four constraints satisfied, 5 segments walked matching
  `getLength()`, 4096/4096 dwords `i ^ 0xA5A5A5A5`, all six commands
  `0x1100` (dated section "ATTACH_BACKING-with-userspace-memory probe —
  VERIFIED — 2026-08-10"). The known limit remains for the finished
  winsys to establish.

- **(2026-08-09 → 2026-08-10) "Active task — 3D transport: nothing
  downstream of context creation has ever executed on a path that
  matters."** The section's four steps (explicit context, clear command,
  `TRANSFER_FROM_HOST_3D` read-back, byte verification) were the
  pre-registered transport gate. **Superseded by:** Increment C
  (2026-08-10) — Mesa's virgl driver produced byte-exact clears through
  the full stack, exactly the deliverable; the ATTACH_BACKING probe the
  same day; and the CGL shim verified end-to-end 2026-08-12. Transport is
  proven per-process. The active task is now the WindowServer
  capability-gating/coupling probe (2026-08-14, top entry).

- **(2026-08-09 → 2026-08-10) "3D beyond capsets — expect novel failures,
  not regressions."** **Superseded by:** Increment C and the CGL-shim
  verification — downstream of `enable3DAcceleration` executes and is
  verified on per-process paths. Novel failures did materialize along the
  way (the `cmd_buf` 256-byte overflow found and fixed during Increment C)
  but the blanket expectation no longer describes the state.

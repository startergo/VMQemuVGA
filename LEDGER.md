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

Last updated: 2026-08-21 final (RUNGS 7-11b COMPLETE — THE GLD ENUMERATES: census error 0, nrend=1, rid=0x1AF60100 (ours | 0x20000 — the record channel, not the version-composed id; rung-9 conflation corrected), accelerated=0 respected (honest-caps survives into enumeration). The full chain live: kext registry claim (IOGLBundleName+AccelCaps, gated) → libGFXShared loads/validates /S/L/E/<name>.bundle → registers in WindowServer → census consults → OUR record is the display's renderer. Coexistence falsified (exclusivity structural; blast radius narrow: substitute immune, WindowServer tolerant). Loader = libGFXShared on disk, decoded; interface = 78 names; tool-failure habit NAMED (4 instances). Next frontier: the context path — pixel-format-first via the float's disasm, Mesa-backed after. Earlier same day: flip experiment (npix=0 wall), rungs 1-2, relay. Previously: 2026-08-20 late night (KERNEL RELAY LANDED AND DELIVERS PIXELS — the browser window renders REAL, UPRIGHT, LEGIBLE content through 0x600C for the first time (user-observed; relay kexts 8f3f31ac→66cab1cc→1457c200). Arc: first relay wrote BAR-0 while the display reads the framebuffer's m_fb_backing → fixed by storing caller-owned backings in the resource pool (backing_owned flag, guarded release at unref); first working build flipped the window (readback bytes are ALREADY top-down — the flip was inherited GL-bottom-up reasoning, and EVERY probe config was flip-blind: a uniform clear colour is orientation-invariant — instrument lesson recorded); identity row order landed it upright. Remaining defect is ARCHITECTURAL, not relay: constant flicker = the relay writes the DESKTOP SCANOUT one layer below WindowServer, which recomposites the window region from its own view backing — two writers, one rect. The right target is the window's own CGS surface via the EXISTING, proven IOAccelSurface path (WriteLock→mapped surface, Flush→blit, WindowServer composites); the missing piece is the GA CFPlugIn (app-side accelerator attach — the same gap that stopped readfb at IOCreatePlugInInterfaceForService). First time the accelerator work and the GL work point at the same next step. Decision pending: build the GA CFPlugIn (largest remaining piece) vs not. Open residuals: the browser dies silently every few minutes — 3× today, mode-INDEPENDENT (relay, CG-blit, census runs all hit it), zero crash reports each time; the CG-blit control run died before rendering (no-regression unconfirmed visually); debug.log re-accumulates. CROSS-CONTEXT ATTACH MISMATCH FALSIFIED on the full boot log (an excerpt showed attaches only on ctx=0x100, batches only on 0x101): the totals are attaches 380×0x100 + 36×0x101, batches 826×0x100 + 6778×0x101 + 32524×0x10a + 1×ctx-0x1 — both contexts attach AND execute; the excerpt was interleaved legitimate streams (two virgl ctxs per browser generation, matching the two-OSMesa-context census). New unexplained datum from the same census: ONE batch executed on ctx=0x1. SECOND FORM ALSO FALSIFIED (the relocated 0x10a mismatch — "workhorse ctx with zero attaches"): kernel.log.1 holds ctx 0x10a's creation at 19:25:52 WITH 14 attaches in the same second; the census file began 19:29:59 — a log-window artifact. Every context in every generation attaches its own resources and runs its own batches; the system is structurally coherent. LESSON ×2: (a) ctx ids and resource ids SHARE ranges (ctx 0x10a and res 0x10a both exist) — instance-tagging applies to log ids; (b) a census over a rotated window must first establish the window's start against the objects' lifetimes. USEFUL SIGNATURE: `removeAllUserBackings … (client died)` is the silent death's kernel-side trace — death timestamps are extractable kext-side. Grey canvas: NO verified observation exists in-session (the term entered via an unattributed log excerpt) — off the books unless directly observed. attachBackingUser's nr_entries=1 warning now fires only for >4096-byte single-segment walks (93d32ce; sub-page allocations land single-segment by design and were burying the signal — the uniq -c failure class). GREY CANVAS NOW OBSERVED directly: live browser, aquarium page — a real rendering datum, distinct from the flicker, back on the books. GA CFPLUGIN BUILD CHARTERED (design banked in docs/ga-cfplugin.md — the full contract with worked-example file:line refs): kext trio currently WRONG 3-of-5 (IOAccelTypes as a number on the accelerator, not the path-string on the FB; FB IOAccelIndex=0x1AF41050 not 0; IOCFPlugInTypes on the FB absent — the plugin-instantiation blocker; AccelCaps absent). Milestones: (1) trio fix + plugin skeleton (factory/vtable/vmStart, other slots Unsupported) + minimal type-2 2D-context user client + readfb-style negative control (IOCreatePlugInInterfaceForService must return a live interface and vmStart must open the context — the exact call readfb died on); (2) surface binding via SetSurface/AllocateSurface/Lock; (3) the relay writes the bound window surface, flicker dies, screen-capture verdict; (4) the substitute drives the GA path in host-present mode. GLPlugin/ in this repo is a CGL renderer plugin (the superseded GLD direction) — NOT a GA plugin, no overlap. GA MILESTONE 2 RUNG 2 — CGS-SURFACE BINDING WORKS (Aug 21 10:0x, kext 56d3e5a7 + plugin ce7b7688, commit 836faf3): PROBE (exit 0, cgs id 1 = WindowServer's surface): AllocateSurface(0x1) bound via registry; LockSurface → app view 0x101000000 rowBytes=6720 (1680×4); FIRST PIXEL READ 0xffffffff — a plain process reading WindowServer's compositing memory through the GA interface; Unlock/Free clean; NEGATIVE (0xdead) refused 0xe00002be. THE TWO-TASK TWO-VIEW CONTRACT IS LIVE. Registry: locked 16-entry array in VMAccelSurfaceClient (add at SetIDMode, remove at stop); type-2 looks up FRESH every op. CACHE DISCOVERY: the startup mkext EXCLUDES VMQemuVGA.kext ENTIRELY (strings=0; digest invariant across kext changes) — kextcache's walker skips us (PlugIns/beside-plugin suspect); boots load individually from /S/L/E. NEXT: milestone 3 — the relay (0x600C) writes the BOUND window surface's backing; flicker dies; screen-capture verdict. GA MILESTONE 3 FIRST BOOT — FAILED, mechanism partially identified (Aug 21 10:2x, kext c2a59aa9 + substitute 898f653b): (1) THE SUBSTITUTE'S CGLSetSurface NEVER FIRED — no GA session lines in pf_m3.log; Gecko's window-surface attachment does NOT route through CGLSetSurface on this stack (design assumption falsified; the surface id must be captured elsewhere — NSOpenGLContext setView path, the interpose, or CGS surface creation APIs). (2) The relay's GA path never triggered (no bind existed) and the DESKTOP-SCANOUT FALLBACK BROKE on this boot: "hostRelayBlit dst res 1 not in resource pool" — resource 1 absent from the device pool on this boot's FB setup path; the fallback must resolve the desktop backing without the pool (the BAR-0-era lesson repeated at a new call site). (3) Screen went BLACK then WHITE with surface-client log volume ~96 WriteLocks (normal compositing volume — the flood impression was the steady-state rate at first-browser-launch); live mode=4 (1680x1050) vs m_width=1920 inconsistency observed in pixel-info lines — mode churn unexplained residual. Browser killed; desktop compositing continued normally (96 locks, flushes green). ROOT CAUSE FOUND (Aug 21 10:5x, rotated kernel.log.0): resource-1 recreation FAILED with 0xe00002d6 kIOReturnNoSpace — THE DEVICE RESOURCE POOL'S 64 SLOTS EXHAUSTED (createResource2D "POOL FULL (64 slots), unref+reject"). This boot ran 1680x1050 (mode churn recreated resource 1 repeatedly); the pool saturated; the desktop lost its virtio scanout resource → whole screen black then white with NO browser needed. The GEM-style dynamic-store fix was applied to the USER backing table (2026-08-18) but NOT to this device pool — the same saturation class at a second table. FIX: grow the device pool dynamically (mirror the backing-store pattern) or start it much larger. The relay's GA path and the substitute were inert bystanders. NEXT: find where Gecko actually binds the window surface on 10.6 (the substitute's interpose setView? CGLSetSurface on the REAL CGL before substitution?), and fix the relay fallback's dst resolution.POST-FIX STEADY STATE CONFIRMED (Aug 21 13:1x, kext 621b144a, user-supplied log): desktop healthy at 1680x1050 (this boot's mode — settled, no churn visible); WindowServer compositing normally through the surface client (SetShape→Lock→Flush cycles, menu bar 1680x22, window regions, a 64x64 busy region at (1256,967) — spinner/cursor class); refresh ~39 Hz; ZERO pool failures, zero POOL FULL, zero high-water fires (13 creates total). The black/white class is closed by observation, not just by code. Mode selection still varies between boots (1680x1050 vs 1920x1080) — the recorded FB-mode confound, unchanged. Full arc: Mesa ledger Aug 20 evening. Entries below)

---

## 2026-08-21 (afternoon) — surface-attach call site FOUND; CGLSetSurface route structurally dead; id must come from the window-backing surfaces

Task #16 closed. The milestone-3 assumption ("the system hands us the
window's CGS surface id via CGLSetSurface") lived only in a code comment
(`substitute_cgl.c:930`) — the charter never states it; second case this
day of a claim inherited from a non-authoritative place driving work.

- **Gecko's attach call sites:** `[mGLContext setView:mPixelHostingView]`
  from `platform/widget/cocoa/nsChildView.mm:4189` (`-preRender:`, first
  composite) and `:4479` (`-doDrawRect:`, post-mode-change). Both land in
  the swizzled `shim_setView:` (cgl-shim `cgl_shim.mm:1141`). The view
  CLASS is resolved lazily at runtime there — that is the recorded
  mechanism; no class name is or should be hardcoded anywhere.
- **Why CGLSetSurface never fires:** for every shim context,
  `shim_setView:` retains the view, arms the drawRect machinery, and
  NEVER calls the original `-setView:` (the else-branch call-through is
  for non-shim contexts only). The suppression is deliberate —
  `shim_initWithFormat:` states it ("no CGS GL surface should be bound";
  origin commit `ef2f57470df`, drawRect-swizzle era). AppKit's
  surface-creation path (`__NS_CGL*` wrappers → CGS IPC → real
  CGLSetSurface) therefore never runs for Gecko's contexts. Settled
  context re-confirmed, not re-opened: AppKit on 10.6 imports zero
  public `_CGL*` symbols, no OpenGL.framework LC_LOAD_DYLIB.
- **Existence check (does a CGS surface for Gecko's window exist?): NO**
  for the GL-drawable surface — the swizzle suppressed its creation;
  `CGSGetSurfaceBinding`/`CGSGetSurfaceList` would return nothing. The
  app-created-surface route is also dead on this stack:
  `probe/probe_cgs_requester` (Aug 14, pre-registered) recorded SILENT —
  CGSAddSurface OK (sid=0xf3409c2; second run 0xf499b03) with ZERO
  surface-client kernel lines: CGS services plain AddSurface entirely
  without the accelerator. No deployment arrangement or API choice
  changes either fact.
- **Latent bug re-confirmed while there:** the substitute's CGLSetSurface
  declares 6 args; the real one is 4-arg
  `CGLSetSurface(ctx, cid, wid, sid)` (Aug-14 disassembly). Never
  exercised by Gecko; fix if ever touched.
- **What DOES exist (kernel-side, no app cooperation needed):**
  WindowServer's window-backing surfaces as type-1 registry entries with
  DESKTOP-positioned shape rects. Evidence (kernel.log.0, this boot):
  menu bar bounds=(0,0 1680x22), window regions e.g. num_rects=29
  bounds=(263,946 1155x104), busy region (1190,967 64x64), full-desktop
  (0,0 1680x1050) at SetIDMode wID=0x1. The relay's blit rect is
  desktop-space. INFERENCE, unverified: the browser window's backing
  appears among these when the browser runs — checkable from the m3-boot
  log at browser-launch time or the next browser run.
- **Open route choice (none chosen this session, each needs a
  pre-registered prediction before a boot):** (a) kernel-side exact
  desktop-rect match against registry surfaces; (b) call the original
  setView: from shim_setView: to let AppKit create the surface —
  requires knowing the suppression's full reason beyond the one comment
  line; (c) the Aug-14 pre-registered capability flip
  (IOAccelerator3D=Yes, boot-arg-gated, re-run the requester).

**BROWSER-WINDOW BACKING CHECK (same day — the inference above
CORRECTED):** "the browser window's backing appears among the registry
surfaces" is FALSE in its per-window form. Evidence — kernel.log.2
(three boots 20:45 / 21:01 / 21:14, the relay-era boots where the
browser rendered): exactly ONE SetIDMode per boot, always wID=0x1
modebits=0x24 "[WindowServer surface]"; 32 SetShape calls per boot, all
within the first minute, bounds are desktop-scale clip/damage regions
(full 1680x1050, menu bar (0,0 1680x22), below-menu (0,22 1680x1028),
dock-area (263,946 1155x104), busy cursor (1256,967 64x64)); NO
window-interior-sized shape, NO second surface, ZERO surface-client
lines during the browser render sessions (relay blits began 20:45:59,
after that boot's 32 shapes were already logged). Same structure on the
m3 boot (10:24:23 SetIDMode wID=0x1; compositing burst ended 10:24:27;
the browser GL window 10:26-10:27:53 produced ONLY "hostRelayBlit dst
res 1 not in resource pool" ×hundreds; the FB refresh timer kept
scanning out at 44-56 Hz on the frozen screen until rotation).
STRUCTURE: WindowServer owns ONE full-desktop IOAccelSurface and
composites ALL windows INTO it — per-window backings are
WindowServer-internal and never reach the registry. Consequences for
the route list: (a) as framed (per-window rect match) has nothing to
match; (a) reframed (relay into surface 1 at the window's rect)
collides with WindowServer compositing the same rect — the flicker one
level up, unless Gecko's CG path leaves the GL area unpainted
(unobserved); the native shape is a GL-surface LAYER for the window
(AppKit's setView path, or an app-created CGS surface) — which needs
the capability question settled first ((c), the Aug-14 pre-registered
IOAccelerator3D flip), because the Aug-14 SILENT result says
app-created CGS surfaces get no IOAccelSurface backing today. Side
datum: the kext logs "initializeWebGLAcceleration: Creating real
VirtIO GPU 3D context … ID: 2" at boot (20:45:22 in k2).

**IOGLBundleName CONSUMER CHECK (same day, resolved):** GfxInfo.mm
reads IOGLBundleName from IOAccelerator entries (`:120-145`) and blocks
FEATURE_OPENGL_LAYERS on PPC driver names OR empty — "block too if no
IOGLBundleName, this means software driver which is also too old"
(`:370-380`) — both inside `#if !defined(MAC_OS_X_VERSION_10_6) ||
(MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_6)`. Verdict on the
SHIPPED browser binary (guest XUL, Jul 14 build, 115,921,400 bytes):
in-`#if` literals ABSENT (ATIRadeonGLDriver=0, ATIRage128GLDriver=0,
"GfxInfo::GetDeviceInfo"=0, IOGLBundleName=0) while out-`#if` controls
PRESENT (0x6760=1, 0x9488=1) — `LC_ALL=C grep -ac -F` on the binary.
The block is COMPILED OUT (10.6+ SDK build): the property is inert for
this build and the accelerator-side removal is safe with respect to
this consumer. Re-check per future release with the same grep — a
10.5-SDK build would go live on the empty-string branch. INSTRUMENT
LESSON: `strings` over non-interactive ssh returned ZERO LINES (silent
tool failure; the 2>/dev/null hid it) and a zero count from a failed
tool reads as "absent" — a positive control is mandatory before reading
any absence; prefer `grep -ac` on the binary directly.

**VDA note (2026-08-21, upstream announcement):** the upcoming PowerFox
release adds GPU H264 decode for 10.6 (VDADecoder class). Not in this
checkout — `platform/dom/media/platforms/apple/` carries only the
VideoToolbox path (AppleVTDecoder/AppleVTLinker), which cannot resolve
on 10.6 and has no runtime pref (compile-time MOZ_APPLEMEDIA only;
PDMFactory.cpp). When the release lands: test guest video before/after
(video is the known-broken axis), and check for a gating pref on the new
path.

---

## 2026-08-21 (later) — PRE-REGISTERED: the IOAccelerator3D capability-flip experiment

**Why this is now the load-bearing experiment, not one option among
several:** today's falsification established WindowServer owns ONE
full-desktop surface (wID=0x1) and composites everything into it —
per-window backings never reach the registry. Every remaining delivery
route (a window GL-surface layer via AppKit's setView path, or an
app-created CGS surface) requires app-side surfaces to receive DRIVER
backing, and the Aug-14 SILENT result showed CGS servicing plain
AddSurface entirely in software. The question underneath all routes:
can the driver be made a participant in app surface creation at all?
Nothing else moves until this is answered.

**The change — one variable, boot-arg gated, same binary:**
- Sibling boot-arg `vm-cap3d` via `PE_parse_boot_argn`, mirroring the
  existing `vm-accel-surface` gate (FB/VMQemuVGAAccelerator.cpp:375).
- Flip ONLY the capability booleans: FB block
  FB/VMVirtIOFramebuffer.cpp:363-366 (`IOAcceleratorFamily`,
  `IOGraphicsAccelerator`, `IODisplayAccelerated`, `IOAccelerator3D`)
  and the parent block FB/VMVirtIOGPU.cpp:457-458
  (`IOGraphicsAccelerator`, `IOAccelerator3D`) — published value
  becomes `functional_3d || gate`, gate state logged loudly at both
  sites.
- UNCHANGED by the flip (recorded to keep it one variable): the model
  string (FB/VMVirtIOGPU.cpp:5897), `IOGLBundleName` ("GLEngine" on the
  FB), `IOAccelIndex`, the GA trio, `IOAcceleratorTypes`
  (FB/VMVirtIOGPU.cpp:462-470).
- `m_3d_functional` itself stays false — this is a publication
  experiment, not a claim that rendering works (the distinction the
  block comment at FB/VMVirtIOFramebuffer.cpp:352-358 already draws).

**Negative control already in place, free of charge:** the
`IOAcceleratorTypes` array has claimed the strings "3D" and "Hardware"
UNCONDITIONALLY on every boot (FB/VMVirtIOGPU.cpp:464-467) and
WindowServer has never reacted to it — direct evidence that the
capability BOOLEANS, not the type-string array, are the operative gate.
The flip varies exactly the booleans; this pre-existing unreacted
string claim is the differential.

**Procedure — two boots, one variable each, browser NOT run:**
1. CONTROL boot (same binary, no arg): re-establish the SILENT baseline
   on the CURRENT kext — the Aug-14 baseline predates pool-512, the
   registry, and the GA-property kexts. Verify ioreg shows the three =
   No; run `probe_cgs_requester` 20s as the console GUI user.
   Prediction: identical to Aug-14 — AddSurface OK, sid != 0, ZERO
   surface-client kernel lines, renderer census nrend=1 accelerated=0.
2. FLIP boot (arg present): ioreg verify three = Yes BEFORE any probe —
   boot itself is the first hazard window. If the desktop is healthy,
   run the probe identically. Capture the kernel.log FILE (rotation
   rule) and full probe stderr.

**The probe's window — recorded before the boot, so a silent result can
be scored against WHAT WAS TRIED (from probe/probe_cgs_requester.m, the
instrument is unchanged from the Aug-14 runs):**
- Construction: `[NSWindow initWithContentRect:(200,200,320,240)
  styleMask:NSTitledWindowMask backing:NSBackingStoreBuffered
  defer:NO]`, `orderFrontRegardless`, activation policy Regular.
- NOT layer-backed (no CA layer requested); title-bar-only style mask
  (no closable/resizable/miniaturizable bits); plain buffered backing;
  default contentView, no NSOpenGLView anywhere.
- GL attachment: NONE through AppKit. The probe creates a bare
  `CGLContextObj` from a SOFTWARE pixel format (no accelerated format
  exists) and binds it to its OWN app-created sid via the PRIVATE
  4-arg `CGLSetSurface(ctx, cid, wid, sid)` — the direct call, NOT the
  AppKit setView→__NS_CGL chain that real windowed GL uses.
- Consequence for scoring: this window is a minimal candidate. A silent
  flip-boot result is consistent BOTH with "capability insufficient
  (deeper gate)" AND with "this window configuration is not one CGS
  backs" — the run alone cannot separate them.

**Pre-registered outcomes (flip boot):**
1. **ADOPTED** — surface-client lines (newUserClient/SetIDMode) inside
   the probe's CGSAddSurface/BindSurface window, with a wID or registry
   id other than the boot's wID=0x1. Meaning: CGS consults the driver
   for app-created surfaces when the capability is on; the registry
   gains app surfaces; every remaining route unblocks. The probe makes
   no IOKit calls, so any line in its window is WindowServer-originated
   — that invariant IS the instrument. IMMEDIATE DESTINATION for a
   positive: the milestone-1 GA plugin — AllocateSurface
   (kIOBlitHasCGSSurface, sid) with the probe's sid (exactly the rung-2
   shape), registry bind, and the relay's GA path becomes wireable
   end-to-end.
2. **STILL SILENT** — ioreg three = Yes VERIFIED, probe output
   identical to control. TWO live readings, NOT separable by this run
   alone: (i) capability necessary but insufficient — the real gate is
   deeper (a renderer/QE path must exist first, the GLD/GLEngine
   question); (ii) WINDOW QUALIFICATION — the probe's window (plain
   titled buffered window, no AppKit GL attachment, sid bound via the
   private direct CGLSetSurface) is simply not a configuration CGS
   backs. The no-IOKit-calls invariant proves the silence is real and
   WindowServer-attributed; it says nothing about which reading holds.
   Pre-registered discriminator (separate boot, its own variable): a
   probe VARIANT whose window goes through the AppKit GL idiom
   (NSOpenGLContext + setView — the chain Gecko's swizzle suppresses)
   under the same flip. Variant ADOPTS while the plain window stays
   silent → reading (ii), the window was the axis. Both silent →
   reading (i) strengthens; only then does the GLD question inherit
   this evidence.
   2b. **ADOPTION ATTEMPTED, REFUSED** — lines appear but surface
   creation/SetIDMode FAILS. Record the raw return codes. Distinct from
   2 (nobody tried vs. tried-and-refused); its own follow-up.
3. **DESTABILIZED** — the outcome-#3/blue-screen class: boot hang,
   blue/black/garbage desktop, WindowServer crash loop, unbootable.
   Meaning: WindowServer is an eager consumer of the property with no
   working backend behind it. Action: revert = remove the one arg from
   OpenCore config.plist NVRAM (read-modify-write the FULL boot-args
   string, never a remembered subset); slclean recovery if unbootable
   (clear caches + touch Extensions, wait minutes). The flip is dead as
   an approach; the residual question becomes whether a narrower
   property gates ONLY surface backing without inviting compositing.

(This entry is committed BEFORE the gate's implementation and before
either boot — the commit timestamp is the pre-registration evidence;
the commit-before-booting rule, learned at the 2a trio's expense.)

**IMPLEMENTATION ADDENDUM (same day, committed before any boot):**
gate implemented as `VMVirtIOGPU::cap3dPublishGate()` — boot-arg
`vm-cap3d`, read once, cached (FB/VMVirtIOGPU.h, FB/VMVirtIOGPU.cpp).
One DEVIATION from the registration's site list, required for the
experiment to be performable: a full re-inventory of live publication
sites (the original grep pattern missed `IOGraphicsAccelerator` — it
matched only "Accelerated") found two OVERWRITE sites that run AFTER
start() and would clobber the flip back to false:
`VMVirtIOFramebuffer::open()` (IODisplayAccelerated) and
`::isConsoleDevice()` (IODisplayAccelerated, IOGraphicsAccelerator,
IOAcceleratorFamily). Both now carry the same gate; ordinary boots
(no arg) publish exactly what they published before. Untouched: the
no-transport else-branch, the QXL-path sites (not this guest), the
dead commented block, and every non-boolean property. Loud logs at
the start-blocks (`functional_3d=%d vm-cap3d gate=%d -> publishing
%s`). Build: `4da4fec9592a97ef68a7101d5fae4a59`; "vm-cap3d" present
in the binary (3 string hits). NOT deployed, NOT booted — the
control-then-flip procedure (task #17) is next.

**RUN 1 — CONTROL BOOT (15:14, kext 4da4fec9, no arg):** gate=0 logged
("functional_3d=0 vm-cap3d gate=0 -> publishing no", two FB starts at
15:14:11/15:18:04); ioreg three = No; desktop normal; probe_cgs_requester
(15:22:48–15:23:10): window 19, cid=35375, nrend=1 accelerated=0,
accelerated-pf npix=0 (the wall), CGSAddSurface OK sid=0xffe9743,
CGLSetSurface(ctx,cid,wid,sid) OK, remove OK, exit 0; kernel window:
ZERO surface-client/newUserClient lines while the boot as a whole logged
658 surface-client lines. **AUG-14 SILENT BASELINE REPRODUCED on the
current kext — control prediction met.** Observation: the
parent-device VMVirtIOGPU::start() property block never logs on this
boot path (no "VRAM properties"/"IOAccelerator ID"/"capability
booleans" lines post-15:10) — the gated edit there is inert; the FB
node is the sole boolean publisher and the verification target. Also:
live boot-args carried THREE args beyond the documented reference —
`vm-accel-surface=1 tlbto_us=0 vti=9` — read before writing, as the
rule requires.

**RUN 2 — FLIP BOOT (boot 15:28:47, boot-args += vm-cap3d=1 via
config.plist full-string edit, verified read-back):** gate=1 logged
("-> publishing YES"); ioreg FOUR booleans = Yes (IOAccelerator3D,
IOGraphicsAccelerator, IODisplayAccelerated, IOAcceleratorFamily);
boot survived, WindowServer running, desktop VISUALLY NORMAL (the
outcome-3 hazard window passed). probe_cgs_requester
(15:32:06–15:32:28): output IDENTICAL to control (window 22, sid=
0xe3d5083, nrend=1 accelerated=0, npix=0 wall, all CGS OK); kernel
window: ZERO surface-client lines; the only window-creation reaction
was VMVirtIOFramebuffer::getVRAMRange at 15:32:07 — WindowServer
serving the new window through the FRAMEBUFFER (software) path.
**SCORED: OUTCOME 2 — STILL SILENT.** Readings (i) deeper gate and
(ii) window qualification both live, per the registration.

**DISCRIMINATOR — run-specific prediction recorded BEFORE the run
(2026-08-21, committed before execution):** variant probe
`probe/probe_cgs_glwindow.m` — the AppKit GL idiom the plain probe
lacks: real NSWindow + NSOpenGLContext + `setView:` + `update` +
makeCurrentContext + glClear + `flushBuffer` (the chain Gecko's swizzle
suppresses; standalone app, no substitute, no IOKit calls). Bonus
datum: CGLGetSurface on the REAL context (legal — real ctx, not a shim
token) returns the drawable's {sid, type, w, h}. DEVIATION from the
registration's "separate boot": runs on THIS flip boot — the
plain-probe measurement is complete and timestamped, the boot variable
is unchanged, and a separate boot would only re-establish identical
state. Predictions:
- ADOPTS — surface-client/newUserClient lines inside the
  setView/flushBuffer window → reading (ii): window qualification was
  the axis; the plain probe's window was not a candidate.
- SILENT AGAIN — zero lines → reading (i) strengthens: capability
  insufficient, deeper gate; the GLD question inherits this evidence.
- REAL-GL DESTABILIZATION — WindowServer crash/blue/garbage during the
  real windowed-GL attach → outcome-3-flavored evidence surfacing via
  the GL path; record and stop.

**DISCRIMINATOR RESULT — SILENT AGAIN (15:36:47–15:37:07, same flip
boot):** probe_cgs_glwindow ran the full AppKit chain clean — window 24,
NSOpenGLContext, setView:/update/makeCurrentContext/glClear/flushBuffer
all returned, exit 0 — and the kernel window contains ONLY the same
software-path reaction as the plain probe
(VMVirtIOFramebuffer::getVRAMRange at 15:36:47); ZERO surface-client
lines; no destabilization. **Reading (i) strengthens: the capability
booleans alone do NOT make CGS/WindowServer consult the driver for
surface backing — neither for plain app-created surfaces nor for the
AppKit windowed-GL attach chain. The deeper gate is the live reading: a
renderer/QE path the SYSTEM's own CGL can see must exist first (the
GLD/GLEngine question inherits this evidence). Note the wall was
visible in both probe runs: CGLQueryRendererInfo → nrend=1,
accelerated=0, CGLChoosePixelFormat(accelerated) → npix=0 — the guest's
REAL CGL has no accelerated renderer; the substitute OpenGL.framework
only applies to processes launched with DYLD_FRAMEWORK_PATH (the
probes, and never WindowServer).**

**EXPERIMENT CLOSED (2026-08-21 15:4x):** boot-args restored (vm-cap3d
removed, full string verified), reboot 15:40:52 logs gate=0 →
publishing no, guest at baseline on kext 4da4fec9 (the gated kext is
safe to leave deployed — ordinary boots byte-identical by construction
and now by observation). Scorecard: control SILENT reproduced (met);
flip stable, STILL SILENT (outcome 2); discriminator SILENT AGAIN
(reading i). The capability-advertisement route to CGS↔driver coupling
is dead at this level. Residuals: (a) the deeper gate — making an
accelerated renderer visible to the SYSTEM CGL (the npix=0 wall) is
the next prerequisite for any WindowServer-side adoption; the
GLEngine/GLD-plugin direction is the recorded dead end there, so this
is a hard problem, not a next step; (b) the browser silent-death and
grey-canvas residuals are untouched; (c) instrument notes:
probe_cgs_glwindow needed `-x objective-c++` (10.6 Security headers
use static_cast in paths Cocoa pulls in); CGLGetSurface dropped from
the probe — SPI with unverified ABI on this system (the CGLSetSurface
6-vs-4-arg lesson), and redundant with kernel SetIDMode lines if
adoption ever fires; `scp -O` required for the 10.6 sshd.

---

## 2026-08-21 (final) — the flip experiment's real finding is npix=0, and "GLD is a dead end" is the wrong closure

**The real finding (stronger than the silence):** both probe processes —
control AND flip — saw `CGLQueryRendererInfo → nrend=1,
accelerated=0, rendererID=0x1020400` and
`CGLChoosePixelFormat(accelerated) → npix=0`. The system's own CGL
enumerates exactly one renderer and it is not accelerated. Therefore
WindowServer could never take an accelerated path regardless of what
the driver advertises: **the capability booleans were downstream of a
renderer that does not exist — they were never the gate.** (Observed
fact, twice.) This also measures the ceiling of the bypass route:
the substitute OpenGL.framework reaches only processes launched with
DYLD_FRAMEWORK_PATH — the browser, never WindowServer, never any
system CGL client.

**Correction of today's earlier closure wording** ("the
GLEngine/GLD-plugin direction is the recorded dead end there, so this
is a hard problem, not a next step"). What the books actually record
as dead, and why it does NOT close this door:

- `GLPlugin/SUPERSEDED.md` (2026-08-09) ranked three seams: (1)
  replace GLEngine.bundle — "writing an OpenGL 2.1 implementation",
  superseded because Mesa already contains all of it; (2) ship a GLD
  driver bundle — "smaller surface, but GLD ABI is undocumented,
  version-locked, and assumes hardware capabilities you'd have to
  fake"; (3) bypass OpenGL.framework via DYLD — chosen, and it works
  (the browser renders through it).
- `docs/vmsvga2-adoption.md` records VMsvga2's GLD as a FORWARDING
  TRAMPOLINE: it dlopens Apple's `AppleIntelGMA950GLDriver` +
  `GLRendererFloat` and forwards 92 entry points, patching strings.
  **`USE_OWN_GLD` is defined in no build configuration** — an
  own-implementation GLD was never built there, and never tried here.

The dead end's obstacles belong to IMPERSONATION (forwarding a real
Apple driver for hardware that does not exist in this VM, or faking
its capabilities). **A GLD implemented against Mesa needs no
impersonation, no command-stream decoder, no ISA work** — Mesa is the
renderer; what it needs is the semantics of the 92 entry points whose
names Zenith432 already recovered (`GLD/EntryPointNames.c`, plus the
trampoline itself as evidence of what Apple's loader asks a GLD for).
Of SUPERSEDED's three objections, "assumes hardware you'd have to
fake" falls away entirely (nothing to fake — Mesa backs it);
"version-locked" is acceptable (10.6 is the only target); what
survives is the real cost: the undocumented-ABI semantics of 92 entry
points plus the GLD lifecycle. A large piece — but a different large
piece from the recorded dead end, and after today it is the only
thing standing between the proven Mesa stack and SYSTEM-side
acceleration: renderer visible to system CGL → accelerated pixel
formats selectable (npix≠0) → WindowServer's accelerated paths become
reachable → today's SILENT surface-adoption result becomes re-testable.
The GA milestone-1/2 plumbing (plugin, type-2, registry) is correct
and waits on the same renderer; nothing built this arc is wasted.

**Status boundaries:** npix=0/nrend=1 = observed. "Booleans
downstream of the renderer" = the direct reading of those numbers.
"A Mesa-backed GLD opens the wall" = design direction, INFERENCE —
settled only by building. The cheap first rung, to pre-register
before building: a stub GLD bundle that loads, registers a renderer,
and returns accelerated=1 from CGLQueryRendererInfo with NO rendering
at all — if `nrend` gains an accelerated entry on a probe boot, the
seam is real; everything after that is entry-point semantics.

---

## 2026-08-21 (evening) — PRE-REGISTERED: the stub-GLD first rung

The rung tests the seam without paying for any of the 92 entry points.
The answer is binary from data probe_cgs_requester ALREADY prints
(`CGLQueryRendererInfo -> nrend`, `renderer[i]: accelerated=`,
`CGLChoosePixelFormat(accelerated) -> npix`).

**Phase A — loader search-path TRACE (no code, no boot, do this
first).** The GLD loader's search path is an unknown on the same
footing as the GA plugin's was — that one resolved from beside
/System/Library/Extensions rather than inside the kext's PlugIns,
discovered by observation after a 0xe00002c7, not from documentation.
Whether `IOGLBundleName` (ours = "GLEngine" on the FB) names a path, a
bundle id, or a name relative to a fixed directory is unrecorded;
guessing wrong produces a silent non-load identical to outcome 2.
Instrument: run probe_cgs_requester under `fs_usage` (root) or a
dtrace open-by-syscall probe on the guest and record EVERY path the
loader touches — failed opens included — during the
CGLQueryRendererInfo window. The trace also answers the name-shape
question empirically (what bundle filename the loader derives from
"GLEngine"). Prediction: a fixed directory set with a
name-derived-from-IOGLBundleName bundle; exact paths unknown — that is
what this run establishes.

**Phase B1 — the stub, tested PROBE-SIDE FIRST (no reboot).** Minimal
GLD bundle: the entry-point name table from
`../../VMsvga2-modern/GLD/EntryPointNames.c`, calling signatures from
the forwarding trampoline `GLD/VMsvga2GLDriver.c` (it forwards with
real signatures — that file IS the ABI reference). Every entry point
appends to a stub-side log file (e.g. /tmp/vm_gld_stub.log — a file,
not stderr, so the log survives regardless of which process loaded
us) and returns 0. No rendering, no capability claims beyond load.
Placed at the path Phase A observed, named the shape Phase A observed.
The loading process for the probe census is the PROBE ITSELF — so B1
is testable without any reboot; the bundle is inert after the probe
exits. Prediction: loader dlopens the bundle and calls at least one
entry point (logged).

**Phase B2 — reboot (separate, deliberate step; hazard window).** A
bundle in a system GLD location is reachable by EVERY CGL client
after reboot, WindowServer included — outcome-3-class risk at system
scope. Recovery: remove the ONE bundle file; slclean procedure if
unbootable. Not entered until B1's outcome is scored.

**Pre-registered outcomes:**
1. **SEAM REAL** — probe census shows `nrend` gained an entry with
   `accelerated=1` (bonus, not required at this rung:
   `CGLChoosePixelFormat(accelerated) -> npix >= 1`). GLD registration
   feeds CGL enumeration; everything after is entry-point semantics.
2. **NEVER LOADED** — stub-side log absent, census unchanged. The
   loader never touched the bundle. NOT a stub defect: re-run the
   Phase A trace against this run and compare whether the placement
   path was probed; iterate placement empirically, never from
   documentation.
3. **LOADED, NOT ENUMERATED** — stub-side log present (entry points
   were called) but `nrend=1 accelerated=0` unchanged. Registration
   is NOT what CGL enumerates from; the seam is elsewhere again
   (CGL's enumeration source becomes the next locus). **This outcome
   is the one most likely to be misread as "the stub is broken" —
   it is informative, not a failure.** The load-marker with
   entry-point call log is REQUIRED instrumentation precisely so 3 is
   distinguishable from 2 by evidence, not inference.

(Committed before any Phase A run — commit-before-experiment rule.)

**RUNG RESULT (same evening) — OUTCOME 2, NEVER LOADED — and the
name-plumbing was a PROJECT-ERA ARTIFACT, not kext plumbing:**

- Phase A trace (fs_usage, system-wide, filtered by process): the
  loader's clean sequence is `stat GLEngine.bundle → stat
  GLEngine.bundle.backup → stat GLRendererFloat.bundle → open
  GLEngine.bundle/GLEngine → stat+open
  GLRendererFloat.bundle/GLRendererFloat` — renderer bundles load from
  `OpenGL.framework/Versions/A/Resources/`, dlopen shape
  `<name>.bundle/<name>` FLAT (no Contents/MacOS). Search path and
  name-shape: OBSERVED.
- The first traces ALSO showed `stat Resources/VMVirtIOGLEngine.bundle`
  + `stat com.vmware.opengl.VMVirtIOGLEngine.plist` ×9 — name-source
  hunt across ioreg (all planes; only FB "IOGLBundleName=GLEngine"
  exists, accelerator's VMVirtIOGLEngine long gone), prefs/caches,
  cvmsConfig.plist (VM-bytecode config, no renderer list), dyld caches
  (/private/var/db/dyld — clean) — all negative. Carrier found:
  **/Users/sl/Info.plist (Nov 14 2025, GLPlugin era)** — the
  never-built renderer's manifest (CFBundleIdentifier
  com.vmware.opengl.VMVirtIOGLEngine, CFBundleExecutable
  VMVirtIOGLEngine). OUR PROBES run with CWD=~; main-bundle machinery
  picks it up and names the renderer probes. MOVED ASIDE →
  VMVirtIOGLEngine stats VANISH (0), sequence pure Apple default.
  Confound PROVEN by the registered prediction test.
- **B1**: stub built (probe/gld_stub.c — 92 exports generated verbatim
  from VMsvga2 EntryPointNames.c; constructor + per-entry logging to
  /tmp/vm_gld_stub.log; x86-64 arg-untouched long-return stubs),
  placed at the observed path, probe run, REMOVED same session —
  NEVER LOADED ×2. Attempt 2 (bundle PRESENT, traced): the loader
  STAT'd VMVirtIOGLEngine.bundle and still did NOT open it — stat is
  existence-checking; the OPEN is gated by something else. Census
  unchanged both times (nrend=1 accelerated=0 npix=0).
- **Two standing facts for the next rung:** (1) the main-bundle
  Info.plist mechanism is a PROVEN process-side naming lever —
  CFBundleExecutable/CFBundleIdentifier com.vmware.opengl.<name> drove
  both the prefs reads and the Resources/<name>.bundle stat; (2) the
  guest's GLEngine.bundle carries project-era contamination —
  `GLEngine.original` (43KB, contains VMVirtIOGLEngine, never loaded)
  parked inside, `GLEngine.bundle.backup` dated May 2024; the LIVE
  GLEngine is md5-identical to the 2011 original (clean). Any GL-side
  experiment on this guest must account for these artifacts.
- Next-rung candidates (not chosen): (i) ~/Info.plist naming + stub +
  an enabling `com.vmware.opengl.<name>.plist` (content/keys unknown —
  the ×9 ENOENT reads suggest the loader consults it before opening);
  (ii) a UNIQUE IOGLBundleName from the kext to disambiguate whether
  the registry names renderer bundles at all (FB's "GLEngine" is
  confounded with Apple's default); (iii) either combined with the
  vm-cap3d flip boot.

**INSTRUMENT RULE (2026-08-21, from the ~/Info.plist catch):** when a
trace shows a name you did not configure, ask WHAT THE PROCESS OPENED,
not where the name could have come from — the opened-files enumeration
caught what a directed source-hunt over registry/prefs/caches missed.
Also standing: the GLEngine fallback list has TWO of its three entries
pointing at project artifacts (GLEngine.original inside the live
bundle, the 2024-era .backup) — any future "the loader fell back to X"
reading must check WHICH X.

**SCHEMA HUNT (same evening) — the pref leg of candidate (i) is
DEPRECATED BY EVIDENCE:** no binary in OpenGL.framework contains
"com.vmware.opengl" (recursive binary grep). The ×9 pref reads are
CFPreferences' GENERIC main-bundle lookups — the domain equals the
main bundle's CFBundleIdentifier from ~/Info.plist; no GL code composes
it. There is no loader-side pref schema to populate; a populated
com.vmware.opengl.<name>.plist would test CFPreferences, not the GL
loader. Additionally, with the bundle present the loader stat'd ONLY
the bundle DIRECTORY (no Contents/Info.plist probe, no inner-executable
probe) — selection does not read bundle contents at stat time.

**RUNG 2 — PRE-REGISTERED (flip-only variable, exact b1 configuration
otherwise; committed before the run):** restore ~/Info.plist (the full
original artifact — its GLRendererProperty block was present during
b1's stat-only result, so holding it constant keeps one variable) +
stub at Resources/VMVirtIOGLEngine.bundle/ + vm-cap3d=1 flip boot.
b1 ran with booleans=No and got stat-without-open; the flip is the one
state never tested between candidacy and selection.
Predictions:
- **OPEN UNDER FLIP** — trace shows open of
  Resources/VMVirtIOGLEngine.bundle/VMVirtIOGLEngine (flat shape) →
  stub log appears → LOADED; census then decides outcome 1 (nrend
  gains accelerated) vs outcome 3 (loaded, not enumerated).
- **STILL STAT-ONLY** — selection ignores the capability booleans;
  candidacy is real, criteria live elsewhere — the next locus becomes
  what distinguishes GLEngine/GLRendererFloat as CHOSEN entries
  (leading candidate: the chosen names come from the IOKit renderer
  path, looping back to npix=0 — no accelerated renderer claimed
  anywhere).
- **BOOT DESTABILIZED** (bundle present + flip at boot — every CGL
  client can now see a loadable candidate): recovery = remove
  vm-cap3d from boot-args (slclean if unbootable; bundle + Info.plist
  are user-space removable from slclean as well).
Procedure: pre-flight both files in place before reboot; on the flip
boot verify gate=1 + booleans Yes BEFORE the probe; probe + trace +
stub-log check in one session; score; then restore boot-args, move
Info.plist aside, remove bundle, return to baseline.

**RUNG 2 RESULT — STILL STAT-ONLY (17:10–17:16, flip boot, scored and
baselined):** boot survived with the bundle present (desktop visually
normal; nothing loaded the stub at boot — no log before the probe);
gate=1 + booleans Yes verified pre-probe. The loader stat'd
Resources/VMVirtIOGLEngine.bundle at 17:14:22 and did NOT open it;
census unchanged (nrend=1 accelerated=0 npix=0); stub never loaded;
the ×9 pref reads reproduced (generic main-bundle lookups). Baseline
restored: boot-args clean (17:16:20 gate=0), Info.plist moved aside,
bundle removed. **Selection ignores the capability booleans.** The two
rungs CONVERGE with the flip experiment: the booleans move neither
enumeration (flip rung) nor selection (this rung). Candidacy via the
main-bundle name is real and capability-independent; what
distinguishes GLEngine/GLRendererFloat as CHOSEN is the next locus.
Structural reading of the observed sequence: GLEngine is the ENGINE
(always opened); GLRendererFloat is the software RENDERER enumerated
into nrend — renderer enumeration must learn about renderer bundles
from a source that is neither the Resources directory listing nor the
main-bundle candidate name nor the booleans. Leading candidate: an
IOKit-side renderer claim on the accelerator (renderer-id-class
properties) — exactly what the Nov-2025 Info.plist's GLRendererProperty
block (VendorID/DeviceID/RendererID 0x00024600) was guessing at. That
is a NEW pre-registration (kext-side renderer-id publication), not
today's work.

---

## 2026-08-21 (night) — source reads before rung 3: the worked example's mechanism, and the Nov-2025 scripts read end to end

**Worked-example correction (VMsvga2, the only third-party GLD that
ever loaded on this OS):** it publishes NO renderer-id properties at
all. Its renderer claim is `IOGLBundleName` on the ACCELERATOR node
(../../VMsvga2-modern/AC/VMsvga2Accel.cpp:616-636), OPTION-GATED by
VMW_OPTION_AC_GL_CONTEXT (the same option-gating pattern as vm-cap3d),
value = own GLD name under USE_OWN_GLD, else the GMA950 forwarding
name. IODVDBundleName is a NULL-release bug workaround (10.9
libGFXShared / AppleVA), not enumeration. The Nov-2025
GLRendererProperty/VendorID/RendererID scheme has NO counterpart in
the worked example — dead as a bundle-plist claim approach. VMsvga2's
build rules: all four products (incl. VMsvga2GLDriver.bundle) install
top-level /S/L/E; Apple's stock GLDs (AppleIntelGMA950GLDriver,
GeForce*GLDriver) are /S/L/E bundles — corroborating /S/L/E as the GLD
canonical directory, distinct from the Resources/ engine+float paths
our traces saw.

**The Nov-2025 GLPlugin scripts (all seven, read 2026-08-21 night) —
three placement theories and two ABI theories, never reconciled:**
- build_for_snowleopard.sh / compile_on_snowleopard.sh: `-bundle`
  link, two-level namespace, FLAT executable, deployed to
  OpenGL.framework/Resources/ (the placement rungs 1-2 used).
- quick_test.sh / test_install.sh: install by REPLACING Apple's
  GLEngine.bundle/GLEngine in place — accounts for the guest's
  GLEngine.original (43KB project binary parked beside the restored
  Apple 5.3MB original; md5-verified restored).
- install_standalone_bundle.sh: /S/L/E standalone, Contents/MacOS,
  "GeForceGLDriver pattern" — but installs as VMVirtIOGLDriver.bundle
  while the kext published IOGLBundleName="VMVirtIOGLEngine":
  **bundle-dir name ≠ published name — a concrete mechanical candidate
  for the era's silent non-load ("CGL never discovered this renderer"),
  never diagnosed because success criteria were never validated.**
- test_v2_guide.sh: the era's ABI was a GUESS — gli*/glo* exports
  (gliQueryRendererInfo, gliChoosePixelFormat...), NOT the gld* names
  VMsvga2 recovered from actual loader behavior. The gld* table
  (92 entries) stands as the ABI.

## 2026-08-21 (night) — RUNG 3 PRE-REGISTERED: accelerator-side IOGLBundleName, name-matched bundle, trace answers the search-dir question

The change (ONE published delta, gated by the EXISTING vm-cap3d
boot-arg; FB untouched; ordinary boots byte-identical):
- The ACCELERATOR (VMQemuVGAAccelerator) publishes
  `IOGLBundleName="VMVirtIOGLEngine"` under the gate — mirroring the
  worked example's mechanism (accelerator node, option-gated,
  name-of-own-GLD). Loud gate log alongside.
- Stub bundle at **/S/L/E/VMVirtIOGLEngine.bundle** — dir name
  EXACTLY matching the published name (the era's mismatch avoided by
  construction), Contents/MacOS layout mirroring a stock Apple GLD
  bundle (pre-build step: read the guest's
  AppleIntelGMA950GLDriver.bundle layout and Info.plist keys and copy
  the shape), executable = the existing 92-export gld* stub,
  minimal Info.plist with the stock bundle's essential keys.
- Field clean: NO ~/Info.plist (rung 2's candidate source removed) —
  the ONLY candidate source this rung is the registry.

The trace question this rung answers that no prior run could: with a
registry-sourced name in play, WHICH directory does the loader stat —
/S/L/E/<name>.bundle, Resources/<name>.bundle, both, or neither?
(Neither prior rung had a registry name; the Resources/-only stats of
rungs 1-2 came from the main-bundle mechanism.)

Pre-registered outcomes (instrument unchanged: census + fs_usage trace
+ /tmp/vm_gld_stub.log; flip-boot procedure and recovery as rung 2 —
gate on, verify booleans before probe, remove arg to revert, slclean
if unbootable):
1. **OPENED (LOADED)** — trace shows open of the bundle (whichever
   dir) → stub log appears → census then decides: nrend gains an
   accelerated entry (SEAM REAL, outcome 1 of the whole arc) vs
   loaded-not-enumerated (entry-point semantics begin — the 92 names
   get their first real callers).
2. **NEW STAT, NO OPEN** — the registry name adds a candidate in some
   directory but selection still refuses; the open-gate is deeper
   (next locus: the GL-context client path — VMsvga2's client type 1
   "GL Context" — CGL may interrogate the driver before opening a
   GLD).
3. **NO NEW STAT** — the accelerator-side name is not read either;
   the enumeration source is deeper IOKit (display/accelerator
   matching), and the GLD route joins the flip rung's convergence on
   "a renderer must be claimed somewhere CGL consults FIRST".
4. **BOOT DESTABILIZED** — recovery per runbook (arg removal; bundle
   is one /S/L/E dir; slclean otherwise).

(Committed before implementation — commit-before-build rule; the
pre-build stock-layout read is part of the rung, recorded above.)

**RUNG 3 AMENDMENTS (same night, before any implementation):**

**A. The stub's answer policy — DECIDED: honest refusals, per entry.**
The uniform-0 stub is wrong by the ABI's own conventions:
`gldGetRendererInfo`'s forwarding path returns 0 ONLY on success with a
filled struct_out, and its no-forward fallback returns -1
(../../VMsvga2-modern/GLD/VMsvga2GLDriver.c:122-146). A stub answering
0 everywhere would claim SUCCESS at the renderer-info query having
written nothing — the over-claiming shape (outcome-3/blue-screen
class), and a loaded stub is no longer inert. Regeneration rule,
extracted from the trampoline's fallback returns + the header's
per-entry signatures and annotations (VMsvga2GLDriver.h:36+):
GLDReturn entries answer -1; _Bool entries answer false; pointer
entries answer NULL; void entries log only. UNDER-CLAIM BY
CONSTRUCTION — if the loader still enumerates a stub that refuses
everything, nothing consumes garbage, and the result (loaded,
refusing, enumerated-as-nothing or not-enumerated) is honest data. The
OPENED→LOADED branch's next observable is the FIRST CALLER — the stub
log names which entry points the loader actually calls, itself a
primary datum. Mis-answer hazard remains outcome-4 class; recovery
stands.

**B. The name mismatch gets its own line, separate from the rung.**
The recorded reason for superseding GLPlugin — "CGL never discovered
the custom renderer" — may be an INSTALL ARTIFACT rather than a
verdict: install_standalone_bundle.sh installed
/S/L/E/VMVirtIOGLDriver.bundle while the kext published
IOGLBundleName="VMVirtIOGLEngine"; if the loader resolves
/S/L/E/<IOGLBundleName>.bundle, the era's install could never match
and non-discovery was guaranteed by the name, not the mechanism. This
does not argue for revival (the era's gli*/glo* ABI was also a
guess — superseded on multiple grounds), but the recorded reason is
WEAKER THAN IT READS, and rung 3 tests the corrected form of exactly
this claim (name-matched by construction). SUPERSEDED.md's rationale
should carry this caveat when next read.

**C. Tooling-residue pattern — twice in one day, now a habit.**
~/Info.plist and GLEngine.original are the same shape: a test script's
in-place/replacement flow left residue that survived into a much later
investigation and masqueraded as system behaviour. Habit recorded:
ENUMERATE WHAT THE GUEST ACTUALLY CONTAINS before trusting what a
trace implies — before GL-side experiments, inventory
OpenGL.framework/Resources/, /S/L/E *GLDriver* bundles, and stray
plists in $HOME and /Library/Preferences. Related standing verdict:
the era's replace-Apple's-bundle flows (quick_test.sh / test_install.sh
copying over GLEngine.bundle/GLEngine) are INAPPROPRIATE AS A CLASS —
rung 3 is additive-only; no Apple bundle is touched.

**RUNG 3 RESULT — OPENED→LOADED (17:53–18:01; the seam is REAL at
the load level; loaded, not yet interrogated):**
- Publication verified live: accelerator log "vm-cap3d gate=1 ->
  IOGLBundleName=VMVirtIOGLEngine published (rung 3)" at 17:53:35;
  ioreg shows the accelerator's VMVirtIOGLEngine BESIDE the FB's
  GLEngine; booleans Yes; boot + desktop normal; nothing loaded the
  stub at boot.
- Loader sequence (probe process, fs_usage): stat
  GLEngine.bundle/.backup/GLProfilerFBDisp/GLRendererFloat → stat+open
  GLEngine.bundle/GLEngine (engine core) → **stat+open
  /S/L/E/VMVirtIOGLEngine.bundle/Contents/MacOS/VMVirtIOGLEngine** →
  stat GLRendererFloat. THE REGISTRY-NAMED BUNDLE OCCUPIES THE GLD
  SLOT, loaded in addition to the engine — the position a real GLD
  holds. Search-dir answer: /S/L/E with Contents/MacOS layout for
  registry-named GLDs (vs Resources/ flat for the engine).
- Stub log: constructor fired BOTH runs (pid 218, 248 — the probe
  process); **ZERO entry-point calls across both runs.** Census
  unchanged (nrend=1 accelerated=0 npix=0). WindowServer stable,
  desktop normal, no destabilization — the refusal-convention stub was
  never even asked.
- SCORE: outcome 1's load half + loaded-not-interrogated. The
  era's-failure hypothesis is settled BY CONTRAPOSITIVE: name-matched,
  the bundle LOADS — the Nov-2025 non-discovery was the
  VMVirtIOGLDriver/VMVirtIOGLEngine name mismatch, an install artifact,
  exactly as amendment B suspected. The remaining question: what makes
  the loader CALL the loaded GLD. Zero gld* calls in the probe flow
  (which includes QueryRendererInfo, ChoosePixelFormat, CreateContext).
- NEXT-RUNG HYPOTHESIS (named, not run):
  **gldInitializeLibrary** — declared in the trampoline header as
  `void gldInitializeLibrary(int* psvc, void*, int GLDisplayMask,
  void*, void*)` (the service-pointer init moment) but ABSENT from the
  92-name table; the stub does not export it. If the loader dlsym's it
  separately as the GLD init handshake and finds NULL, it loads
  nothing further. Rung 4: export gldInitializeLibrary +
  gldTerminateLibrary per the header signatures (void → log only),
  everything else unchanged; prediction = the first CALL line names
  the handshake entry (or the loader stays silent and the interrogation
  gate is elsewhere again).
- State: kext e96bec225536dcb95c82db6de2131596 deployed (gated;
  baseline byte-identical, verified 18:01:08 gate=0 boot); bundle
  removed; boot-args restored; guest at baseline.

**RUNG 4 PRE-REGISTRATION (same night, committed before the run; NO
kext change — same gated kext, one variable: the stub's exports):**
- gldInitializeLibrary exported with its REAL header signature
  `void gldInitializeLibrary(int* psvc, void*, int GLDisplayMask,
  void*, void*)` — the body LOGS THE ARGUMENT VALUES (psvc pointer,
  GLDisplayMask) as observation, returns nothing (void = honest — no
  success claim possible on a void entry). gldTerminateLibrary(void)
  exports log-only. All 92 existing entries unchanged.
- Outcomes:
  1. **HANDSHAKE FIRES** — first CALL line is gldInitializeLibrary;
     the arg values are the datum (psvc ≠ NULL would be the driver
     connection the real GLD receives). Sub-case 1a: the loader then
     calls gldGetRendererInfo → the interrogation chain is MAPPED;
     our -1 refusal shows; entry-point semantics become the work.
  2. **SILENT STILL** — no call to any entry: the handshake theory
     dies; the interrogation gate is elsewhere again.
  3. **DESTABILIZED** — same recovery (arg + bundle; slclean).
- Procedure as rung 3: bundle to /S/L/E, vm-cap3d=1, verify
  publication + desktop before probe, probe ×2, score, restore.
- ARG OBSERVATION DEEPENED (before the run): psvc is DEREFERENCED
  when non-NULL and the pointed-to int logged — that int is the first
  real information about how Apple's GLD reaches the kernel side;
  capture it even if the headline is only "handshake fired."
- **NULL-RESULT BUDGET (fixed before the run): rung 4 is the LAST
  hypothesis-rung.** If exporting the handshake changes nothing (still
  zero calls), the approach SWITCHES from symbol-guessing to
  OBSERVATION of the loader itself: dtrace dlsym-entry probes
  (pid$target::dlsym:entry, print arg0) around the probe process name
  EVERY symbol the GLD loader resolves and in what order — the
  handshake named directly, not by hypothesis. No second symbol guess;
  an enumeration with no end is the failure mode this rule prevents.

**RUNG 4 RESULT — HANDSHAKE FIRES; gldGetVersion IS THE GATE
(18:17–18:24; baseline restored 18:24:51 gate=0):**
- The chain, mapped in both runs: constructor →
  **gldInitializeLibrary(psvc=0x7fff70b72004, arg1=0x7fff70b72084,
  GLDisplayMask=0x1, arg3/arg4 stack ptrs)** → gldGetVersion (4×int*
  outs) → **false** → gldTerminateLibrary. Clean teardown; desktop
  stable; census unchanged; the refusal convention's second
  vindication — the false answer was consumed as a clean give-up, no
  garbage, no crash.
- *psvc varies per process (0x3a03 pid=203, 0x3903 pid=211): the
  POINTER is a shared-cache global slot (stable); the stored HANDLE
  varies; two values a hex digit apart read as mach-port-name /
  sequential-service-handle class — plausibly the driver connection
  the header name implies. INTERPRETATION OPEN; the settle-it test is
  pre-registered below (launch-order correlation across ≥3 processes).
- The budget rule never triggered — the hypothesis landed first time.

**RUNG 5 PRE-REGISTERED (two phases; committed before any run):**
- **Phase 1 — PURE OBSERVATION, baseline boot, ZERO changes** (no
  gate, no bundle): dtrace pid probes around the probe process on the
  STOCK flow — gldGetVersion entry/return with the four int* contents
  read after return, gldInitializeLibrary args, and every subsequent
  gld* call in order. This yields (a) the TRUE version values the
  loader accepts (from the working software GLD — GLRendererFloat
  answers this interrogation successfully in every process), (b) the
  real chain beyond version — what a true answer will face, and (c)
  the same datum set for comparison against our stub's handshake.
- **Phase 2 — THE VERSION FLIP, one variable:** gldGetVersion returns
  TRUE and writes the OBSERVED phase-1 values into its four int* outs;
  gldInitializeLibrary logs as now; **ALL 92 other entries keep the
  refusal conventions** — the stub stays honest everywhere else so the
  next sequence reads as a clean chain (refusal's third vindication
  pending), never a crash. Prediction: the loader proceeds to the next
  entry (presumably gldGetRendererInfo) and the log maps one more
  step; our -1 refusal there should again produce a clean give-up.
  NO guessed version numbers — observed values only (a guessed value
  would become the variable that muddies the run).
- Phase 2 also samples *psvc across ≥3 probe processes for the
  launch-order test.
- **PHASE-1 SECOND PAYLOAD (pre-registered):** the same trace resolves
  the *psvc handle question free — observe what the loader does with
  the slot on a GLD that SUCCEEDS: read *psvc at init entry and again
  at each later gld* entry (slot address is the stable shared-cache
  global 0x7fff70b72004, no ASLR on 10.6); changed ⇒ written during
  the handshake; constant-but-read ⇒ input only; never re-read ⇒
  stored once. Acceptance-path observation, not the refusal path — a
  driver that refuses teaches the give-up, and it is acceptance we
  must imitate.
- **SLOT-DIVERGENCE CAUTION (pre-registered):** GLRendererFloat
  occupies a DIFFERENT slot than a registry-named GLD — it is the
  software renderer at the END of the fallback chain; a registry-named
  GLD loads between the engine core and it. The loader may interrogate
  the two differently. A divergence between phase 1's chain and
  phase 2's chain is a FINDING about slot-dependent behaviour, NOT a
  sign phase 2 went wrong — registered here so the natural
  "something broke" misreading has a written counter.

**RUNG 5 RESULT (19:13–19:24; baseline restored 19:24:09 gate=0):**

*Phase 1 — instruments:* the pid provider did NOT instrument the
lazily-dlopen'd GLD modules on 10.6 (both probe sets empty; the
attach-time dlsym probe matched nothing) — the live-trace route is
dead on this OS without more plumbing. PIVOT (still pure
observation): DISASSEMBLY of the working GLD, pulled to host —
`otool -tV GLRendererFloat`:
- `gldGetVersion` @0x18d05: writes **(3, 1, &_mh_bundle_header,
  0x400)** into the four outs, returns true — but ONLY IF a
  `gld_io_data` field is nonzero (guard); otherwise returns false.
- `gldInitializeLibrary` @0x18d50: stores the GLDisplayMask into
  gld_io_data, then **tail-calls glvmPreInit(arg4 & 1)** — the GL VM
  pre-init (libGLVMPlugin, the cvmsConfig plugin). `gldTerminateLibrary`
  tail-calls glvmPostTerm.

*Phase 2 — the flip (19:13, ×3 runs):* stub's gldGetVersion typed,
writes the OBSERVED values, returns 1; all 92 others keep refusals;
boot normal, desktop normal. **The chain is IDENTICAL: Initialize →
GetVersion(TRUE) → TerminateLibrary. Version-true alone does NOT
advance the chain; census unchanged (nrend=1).** The refusal
convention's third vindication: true-or-false, the loader's teardown
is clean either way — no destabilization anywhere in the arc.
*psvc launch-order test NEGATIVE: 0x3903 across all three processes
this boot (pids 349/358/363), vs 0x3a03→0x3903 across two processes
on the rung-4 boot — not a process ordinal; mach-port-name class with
boot/session scoping; interpretation remains open.

*Reading (marked):* Initialize→Version→Terminate looks like the
loader's PER-CANDIDATE PROBE CYCLE, not a rejection path — version
data recorded, candidate unloaded, selection elsewhere. And the
disassembly names the dependency we skipped: the real
gldInitializeLibrary calls glvmPreInit, and gldGetVersion's guard
field is exactly the state that VM init sets — our no-op Initialize
answered version without the VM ever being pre-initialized. NEXT LEAD
(named, not run): mimic the real Initialize — store the mask, forward
to glvmPreInit (dlsym from the plugin) — the first entry with real
semantics; the rung after that is gldGetRendererInfo's real contract,
informed by the same disassembly method (GLRendererFloat's
gldGetRendererInfo is the acceptance-path reference for the struct it
must fill).

## 2026-08-21 (late) — RUNG 6 PRE-REGISTERED: the VM lifecycle pair goes real (first ACTING rung)

**Evidence base gathered before registering:**
- Disassembly (rung 5): real `gldInitializeLibrary` @0x18d50 has a
  SIX-arg ABI (reads %r9d — the trampoline header's 5-arg void
  signature is incomplete); stores GLDisplayMask into `gld_io_data`;
  tail-calls **glvmPreInit(arg6 & 1)** and PROPAGATES its return.
  `gldTerminateLibrary` tail-calls glvmPostTerm. GLRendererFloat's
  `gldGetVersion` returns false unless a gld_io_data field is set —
  the version answer is downstream of init.
- Symbol home located: libGLVMPlugin.dylib exports the VM OPERATIONS
  but NOT the lifecycle pair; **GLEngine contains glvmPreInit (5
  direct-grep hits; nm's zero was the silent-tool-failure class —
  control lesson applied)**. GLEngine loads BEFORE any GLD in every
  observed sequence, so dlsym(RTLD_DEFAULT) at Initialize time has
  guaranteed ordering.

**The change — ONE semantic unit, the VM lifecycle pair:**
- Initialize: typed 6-arg signature, logs all six args (incl. the
  newly-discovered arg6), stores/logs the mask, dlsym(RTLD_DEFAULT)
  both glvm symbols, forwards (arg6 & 1) to glvmPreInit, propagates
  its return value.
- Terminate: forwards to glvmPostTerm.
- **gldGetVersion becomes GUARDED, mirroring the real GLD's honesty:**
  true (with the observed tuple) ONLY after a successful Initialize
  forward; if glvmPreInit is unresolved or failed, version answers
  FALSE — the version claim is never made without the VM behind it
  (the guard is the anti-over-claiming structure, discovered in the
  working GLD's own code).
- All 92 other entries: refusal conventions unchanged.

**Pre-registered outcomes (instrument unchanged: stub log ×3 runs +
census; procedure as rungs 3-5: bundle+gate, verify publication +
desktop before probe, restore baseline after):**
1. **VM LIFECYCLE MOVES THE CHAIN** — post-version the loader proceeds
   (first real caller of the refusal stub — presumably
   gldGetRendererInfo, clean -1 datum) OR census moves (any of
   nrend/npix). The VM dependency was the gate.
2. **CHAIN UNCHANGED** — identical Initialize→Version→Terminate,
   census frozen: the per-candidate-probe-cycle reading strengthens;
   next locus becomes the SELECTION/re-load path — observational
   fs_usage around ChoosePixelFormat/CreateContext comparing whether
   GLRendererFloat is re-opened where ours is not.
3. **glvmPreInit UNRESOLVED OR FAILS** — dlsym NULL or nonzero return:
   logged loudly; the guard keeps version FALSE (honest); chain
   expected as rung 4. Runtime resolution IS the arbiter of the
   GLEngine-exports hypothesis.
4. **DESTABILIZED** — the first rung where the stub ACTS (real VM
   init in-process). Recovery unchanged: arg + bundle removal;
   slclean if unbootable.

(Committed before implementation — commit-before-build rule.)

**RUNG 6 RESULT (19:56) — outcome 3-variant + a structural discovery:**
```
STUB LOADED
CALL gldInitializeLibrary(6-arg) psvc=0x7fff70b72004 mask=0x1 arg5=0x0  *psvc=0x3903
  glvmPreInit(0x0) -> 3   (g_vm_ok=0 — my rc==0 assumption)
CALL gldGetVersion -> FALSE (guarded)
CALL gldTerminateLibrary -> glvmPostTerm forwarded
```
- glvmPreInit RESOLVED via RTLD_DEFAULT (GLEngine-exports hypothesis
  confirmed at runtime) and was CALLED — the first ACTING forward of
  the arc. glvmPostTerm forwarded cleanly. Census unchanged; the
  loader terminated — chain as rung 4. Desktop/WindowServer stable.
- **STRUCTURAL DISCOVERY (inference, marked):** the working GLD
  receives the SAME loader args (arg5=0 → glvmPreInit(0) → 3) and
  STILL answers version-true — it enumerates. Therefore the real
  version guard is the MASK STORE in gld_io_data (unconditional in
  Initialize per the disasm), NOT the VM return; my rc==0 guard
  threshold was the wrong structure — honest in direction, wrong in
  shape. The settle-it test is rung 6b below.
- Residual CORRECTED (same night, from the backgrounded wait's
  output): the ~19:41-19:53 ssh/mDNS death was NOT a network flake —
  at 19:56 the guest showed `up 1 min`: **a SPONTANEOUS REBOOT at
  ~19:55 ended the outage.** The desktop the user saw "normal" was
  the freshly-rebooted guest; the rung-6 probe (19:56:24, pid 184)
  ran on the POST-CRASH boot — which still had gate=1 and the bundle
  (config.plist + /S/L/E persisted), so the rung-6/6b data stands.
  The stub NEVER loaded in the death window (no constructor line
  between the 19:40 verify and the probe), so the acting-forward stub
  is not the carrier; the crash class is UNEXPLAINED and joins the
  residuals (candidate context: this session's third reboot-adjacent
  network loss, first confirmed to be a reboot).

**RUNG 6b PRE-REGISTERED (one line; NO REBOOT — the probe dlopens the
/S/L/E bundle per-process, so the binary swaps on the live gated
boot):** the version guard becomes `mask != 0` (mirroring the working
GLD's actual structure — the mask store), replacing the rc==0
threshold; glvmPreInit is still called and its return still PROPAGATED
to the loader as Initialize's return (honest pass-through — the loader
sees exactly what it would see from the real GLD). Prediction:
version answers TRUE despite rc=3, and the datum becomes what the
loader does with Initialize's return 3 + version-true. Outcomes:
(1) chain moves — first post-version caller; (2) chain identical —
the loader ignores Initialize's return AND version-true still does
not advance: the per-candidate-probe-cycle reading becomes the
standing model, selection locus next; (3) destabilized — recovery
unchanged.

**RUNG 6b RESULT (19:58, live bundle swap — NO reboot; the probe
dlopens /S/L/E per-process) — OUTCOME 2:**
```
Initialize(6-arg) mask=0x1 arg5=0x0 *psvc=0x3a03
  glvmPreInit(0x0) -> 3 (rc PROPAGATED to loader; guard=mask!=0 -> 1)
gldGetVersion -> TRUE (3,1,&hdr,0x400; VM ok)
gldTerminateLibrary -> glvmPostTerm forwarded
```
Version answered TRUE despite rc=3 — the loader took it and
TERMINATED anyway. Census unchanged (nrend=1, npix=0); system stable;
the no-reboot bundle swap works (instrument speedup for all future
rungs: binary swaps on a live gated boot). **STANDING MODEL (earned
across rungs 4-6b): the loader's cycle is load → Initialize (rc
ignored) → Version (recorded) → Terminate — a CAPABILITY
REGISTRATION pass over every candidate GLD; selection/enumeration
consult something else.** *psvc CORRECTED (same night): rungs 6
(0x3903, pid 184) and 6b (0x3a03, pid 235) sampled WITHIN ONE BOOT —
the boot-scoped reading recorded above is FALSIFIED. Full sample:
rung4 boot {0x3a03, 0x3903}, rung5 boot {0x3903 ×3}, post-crash boot
{0x3903, 0x3a03} — two adjacent values alternating across processes
with no clean per-boot or per-ordinal pattern; mach-port-name
allocation/recycling class; question open.
- NEXT LOCI (named, not run): (a) gldGetRendererInfo's real contract
  by disassembling GLRendererFloat's implementation — the
  acceptance-path reference for the struct it must fill, preparatory
  for when a caller finally reaches it; (b) the selection question
  sharpened: enumeration never lists us though our version data was
  recorded — either GetRendererInfo is called only for chosen GLDs at
  context time, or an IOKit-side claim gates enumeration (NOT load —
  rung 3 settled load). The npix=0 wall from the flip experiment sits
  at the far end of the same chain.
- Baseline restored 20:04:53 (gate=0); bundle removed; kext
  e96bec225536dcb95c82db6de2131596 remains deployed (gated).

## 2026-08-21 (latest) — RUNG 7 PRE-REGISTERED: the gldGetRendererInfo contract (phase 1 done by disassembly) + the caller-hunt

**PHASE 1 — THE CONTRACT, decoded from GLRendererFloat
`gldGetRendererInfo` @0x1775b (pure observation, done before this
registration):**
```
gldGetRendererInfo(void* rec /*rdi*/, int GLDisplayMask /*esi*/):
  mask = gld_io_data.mask                      /* set by Initialize */
  if (!(mask & esi) || (~mask & esi)) return 0x2716   /* = 10006 = kCGLBadMatch */
  rec[0x00] (qword) = &_mh_bundle_header        /* record anchor */
  rec[0x08] = 0x1000400   /* renderer ID base */
  rec[0x24] = mask
  rec[0x0c] = 0x6CD;  rec[0x10] = 0xD;  rec[0x1c] = 0x1001;  rec[0x20] = 0x81
  rec[0x14] = 0x8008000; rec[0x18] = 0x20000000   /* capability words */
  word[0x28]=4; word[0x2a]=1; word[0x2c]=0x10; byte[0x2e]=1; rec[0x30]=1
  rec[0x3c..0x84] = global-sourced limit fields (otool renders as symbol
                    displacements — semantics unread; the RECORD SHAPE is the datum)
  return 0   /* success */
```
- **Cross-validation datum:** the GLD writes renderer id 0x1000400;
  the probe census reports 0x1020400 — a caller-side transform of
  0x20000 (suspected, checkable when a caller is found).
- **Mask protocol:** answer ONLY for displays ⊆ the Initialize-stored
  mask; otherwise kCGLBadMatch — the GLD claims displays, the caller
  asks per-display.
- **CORRECTION to rung 6b's "rc ignored" (too strong):** the float
  renderer receives the SAME glvmPreInit(0)→3 in its own Initialize
  (same loader args) and enumerates fine — rc=3 is the NORMAL return,
  not failure; "rc=3 didn't visibly change the cycle" was the
  observation, "ignored" was not established.

**PHASE 7a — THE CALLER-HUNT (pre-registered FIRST; pure observation,
no boot):** rungs 4-6b prove the loader never calls
gldGetRendererInfo on our GLD in the probe flow. The dispatch is
indirect — through the 92-name table (that IS what the table is for).
Disassemble GLEngine (the engine core, the only remaining
code-in-the-loop) for indirect dispatch through the entry table:
find the call site(s) to the GetRendererInfo slot and read the
GATING CONDITION — what distinguishes the float renderer (called,
enumerates) from a registry-named GLD (cycled, never consulted).
Expected candidate answers, pre-registered: (i) the engine consults
only ITS OWN Resources/-loaded float path and registry-named GLDs
need a display/renderer claim (converges with npix=0 and the flip
experiment); (ii) the version data recorded in the cycle feeds a
list, and GetRendererInfo is called lazily at first
ChoosePixelFormat — our flow did call ChoosePixelFormat (plain) —
so the gate would be the requested attributes (accelerated pf never
enumerates: the wall again); (iii) a mask mismatch — the caller asks
with a display mask our record would refuse (testable: our mask is
the Initialize mask 0x1; the engine may ask with a wider mask).
**PHASE 7b — THE CONTRACT IMPLEMENTED (pre-registered, AFTER 7a;
live-swap boot):** stub's gldGetRendererInfo fills the record per
the contract with OUR identity honestly — our bundle header at +0,
our mask, a renderer id in OUR space (NOT 0x1000400 — no
impersonation of the software renderer), software-class capability
words only (nothing we cannot back), kCGLBadMatch for masks outside
our claim. Prediction set depends on 7a's reading and will be
fixed in an addendum BEFORE 7b runs — no outcomes guessed past an
unread gate.
(Committed before any 7a run — commit-before-experiment rule.)

**RUNG 7a RESULT — ELIMINATION (20:24–20:32; baseline restored
20:29:58 gate=0):**
- Statics first: NEITHER the on-disk GLEngine NOR the main
  OpenGL.framework binary contains a single gld string — the
  92-name table and the dispatch live ONLY in the dyld shared
  cache (direct-grep hit). Static extraction from a 10.6 cache
  needs carving tooling; the behavioral route ran instead.
- Behavioral (probe/probe_r7.c, four modes, own process each, on a
  live-swap gated boot with the logging stub):
  c = control census (main-display mask); m =
  CGLQueryRendererInfo(0xFFFFFFFF); d = census + 26-property
  CGLDescribeRenderer walk; p = census + six pixel-format sets
  (accelerated / offscreen / accel+double / robust / ALL_RENDERERS /
  ALL+accelerated). **ALL modes: identical cycle
  Initialize→Version(true)→Terminate; ZERO gldGetRendererInfo
  calls.** Census unchanged everywhere (nrend=1, rid=0x1020400;
  ALL_RENDERERS npix=1 — software only).
- Score vs the registered candidates: (ii) lazy-at-ChoosePixelFormat
  WEAKENED (every attribute set incl. AllRenderers; contexts were
  already covered — every rung's requester does
  CreateContext+SetSurface); (iii) mask mismatch WEAKENED (0xFFFFFFFF
  was asked; no call occurred at all, so no refusal was possible);
  (i) display-side claim required — STRENGTHENED BY ELIMINATION.
- **7b DEFERRED, per the registration's own rule** ("no outcomes
  guessed past an unread gate"): the gate is not in the CGL API
  surface, so implementing the contract now would be inert. The true
  next datum is the cached loader's selection condition. Two routes
  named: (A) static — carve the dispatch from the shared cache
  (.map address ranges + the name-table string address); (B) the
  IOKit-side claim experiment — the kext publishes a
  renderer-id-class property under the gate (the rung-3 mechanism
  generalized), converging with npix=0 and the flip experiment.
- Instrument notes: kCGLRPVendorID does not exist in the 10.6
  headers; live-swap used again (bundle installed on an
  already-gated booted system, no reboot needed mid-rung).

## 2026-08-21 (rung 8) — PRE-REGISTERED: the IOKit renderer-claim experiment (AccelCaps=3, a matured deferral)

**Property choice is grounded, not a guess matrix:** the worked
example publishes `AccelCaps=3` on the ACCELERATOR in the same
property block as IOGLBundleName
(../../VMsvga2-modern/AC/VMsvga2Accel.cpp:604). Milestone 1 DEFERRED
our copy with a recorded return-condition — "returns when the surface
path works" — and that condition MATURED on 2026-08-21 (milestone 2
rung 2: surface binding probe-verified; registry live). This rung
executes the deferral under the vm-cap3d gate. One property, one
variable vs rung 6b's configuration; stub stays the rung-6b build
(GetRendererInfo = log+refuse — the question is whether it gets
CALLED; 7b remains deferred).

**The change:** VMQemuVGAAccelerator publishes
`setProperty("AccelCaps", 3, 32)` gated by cap3dPublishGate(), loud
log; the milestone-1 deferral comment updated to name its matured
condition. FB untouched; ordinary boots byte-identical.

**Pre-registered outcomes (instrument: stub log ×4 probe_r7 modes +
census + desktop watch):**
1. **CONSULT HAPPENS** — any NEW entry called on our GLD (first
   candidate: gldGetRendererInfo) → the claim opened selection; 7b
   activates next with its own outcome addendum.
2. **CENSUS MOVES WITHOUT CONSULT** — nrend/npix change with no new
   entry call (loader synthesized from the claim) → also opens 7b,
   different reading (the record is not consulted — the registry
   claim IS the renderer).
3. **NO CHANGE** — AccelCaps is not the selection key; the
   elimination table extends; the named next route is the STATIC
   cache carve.
4. **WINDOWSERVER LOOP / DESTABILIZED** — the ORIGINAL AccelCaps
   deferral risk (QE claim invites accelerated compositing; the
   2026-08-20 open/close loop) — now with the GA surface working,
   possibly gentler, possibly not. Recovery: gate off (arg removal),
   slclean if unbootable. Desktop watch is MANDATORY in the first
   minutes after boot.

Procedure: kext build → deploy + caches → bundle + gate → reboot →
verify AccelCaps in ioreg + log → desktop watch → probe_r7 all modes
→ score → restore baseline.
(Committed before implementation — commit-before-build rule.)

**RUNG 8 RESULT (20:44–20:52; baseline 20:52:08 gate=0) — WINDOWSERVER
IS THE CONSUMER NOW:**
- Publication verified (AccelCaps=3 beside IOGLBundleName in ioreg;
  both log lines at 20:44:53). Boot + desktop normal AND STABLE —
  the milestone-1 loop risk did NOT revive with the GA surface
  working. Kext cf3eeda9c71068d972fd789ca36e3d0c deployed (gated).
- **HEADLINE: WindowServer (pid 95, user _windowserver) LOADED our
  GLD at boot (20:45:02) and ran the full cycle** — Initialize →
  Version(true) → Terminate — the first WindowServer-side engagement
  of the accelerator arc. The trio → plugin → flip chain all aimed at
  this consumer; the flip could not reach it because the booleans are
  not on the path WindowServer takes — **AccelCaps is.**
- WindowServer's cycle vs the probes': arguments STRUCTURALLY
  IDENTICAL (mask=0x1, arg5=0x0, arg3/arg4 the same shared
  addresses); differences only per-process pointers (its
  psvc=0x1003117e0 vs the probes' 0x7fff70b72004) and *psvc value
  (0x3127). **The selection condition is NOT in the handshake
  arguments.**
- **psvc question SETTLED-AND-BORING:** WindowServer's slot sits at a
  different address — per-process globals with per-process values;
  the mach-port-name hypothesis loses its main support. Closed, not
  open.
- Probe surface unchanged (no gldGetRendererInfo in any mode; census
  frozen; AllRenderers npix=1). Kernel side ordinary on this boot.
- Score: a NEW outcome between the registered ones — the claim
  changed WHO loads the GLD (WindowServer) while the consult remains
  uncalled. Frontier advanced: "no consumer" → "the consumer cycles
  the GLD and still doesn't consult."
- Instrument notes: WindowServer's root-owned /tmp log broke the
  probes' stub logging (append denied) — the constant-CALL-count
  artifact was a logging failure, diagnosed and cleared (sudo rm);
  future multi-user runs must clear the log with sudo or
  chmod it. The milestone-1 GA log line still prints "AccelCaps
  deferred" — stale string, fix at the next kext touch.

**RUNG 9 PRE-REGISTERED — the renderer-claim property, observable =
WindowServer's own behaviour** (its cycle in the stub log + any new
kernel/ioreg lines + census), one live-swap boot. PROPERTY SOURCE
QUESTION OPEN, grounded options ordered: (a) worked-example inventory
diff — what VMsvga2 publishes that we still do not (thin:
IODVDBundleName bug-workaround; options keys); (b) the GLPlugin-era
GLRendererID=0x24600 as a REGISTRY property on the accelerator — a
new mechanism for a claim whose bundle-plist form is dead;
(c) if no grounded candidate emerges from (a), the rung falls back to
the STATIC cache carve — budget rule applies, no guess enumeration.

**RUNG 9 RESOLVED BY THE INVENTORY (same night): the worked example
contains NO renderer-id property — route (a) terminates EMPTY.**
Complete sweep of every setProperty across AC/ + FB/
(VMsvga2Accel, VMsvga2, SVGADevice, UC/VMsvga2Surface) + the
GL/discovery-path diff: the only unpublished worked-example property
is IODVDBundleName="AppleVADriver" — its own comment marks it an
AppleVA NULL-release bug workaround (video path, 10.9-era), NOT
enumeration. Everything else is driver-private (options/log levels,
SVGA capabilities, refresh quantum) or the surface path
(CGSSurfaceID per surface — our registry's parallel, already live).
Option (b) has no worked-example counterpart — the budget rule bars
it. **RUNG 9 = THE CACHE CARVE**, with the question list sharpened by
the whole arc:
1. The selection condition — when does the cached loader call
   table[gldGetRendererInfo] for a registry-named GLD (the float
   renderer's path shows the consult works on this OS).
2. What the loader does with the VERSION TUPLE, especially a2 (the
   bundle-header pointer): GLRendererFloat passes ITS OWN header; the
   loader may dereference/validate it — our minimal stub's header
   could fail such a check silently, dropping the candidate before
   the consult. (Marked INFERENCE; the carve reads it directly.)
3. The 0x1000400→0x1020400 id transform (+0x20000).
Carve method (next session's first move): the dyld cache .map gives
module address ranges; the name-table string address in the cache is
findable by grep; a small host-side extractor (or manual otool on
carved segments) yields the dispatch code. No boot risk; entirely
static.

**RUNG 9 RESULT — THE CARVE COLLAPSED INTO A DIRECT READ; ALL THREE
QUESTIONS ANSWERED; OUR STUB HAS BEEN FAILING VALIDATION ON A
MISREAD VALUE (2026-08-21 late night):**
- The loader is **libGFXShared.dylib** (OpenGL.framework/Libraries,
  120KB, ON DISK — disassembled directly with otool; the "carve"
  became unnecessary once ownership was computed: cache .map +
  header mapping table (EX fileOffset 0, RW 0x9e47000, RO
  0xb048000) + grep byte offsets; first ownership lookup hit a
  dropped-hex-digit arithmetic error → false "CoreSymbolication";
  the corrected address 0x7fff84625d2b → libGFXShared __TEXT
  0x7fff84621000-0x7fff84627000). My earlier on-disk grep of this
  file ("0/MISSING") was the silent-tool-failure class AGAIN.
  Independently confirmed by the (slow, TCG) exhaustive on-disk
  sweep finishing later: the ONLY system files containing
  "gldGetRendererInfo" are libGFXShared.dylib and GLRendererFloat —
  triple-confirmed ownership (cache map, direct disassembly,
  sweep).
- Path template confirmed in its cstrings: "/System/Library/"
  "Extensions/" + ".bundle/Contents/MacOS/" + IOGLBundleName +
  GL_RESOURCES + GLRendererFloat + the name table.
- **THE VALIDATION SEQUENCE (0x14e5–0x1669), decoded:**
```
dlopen(<S/L/E/name.bundle/Contents/MacOS/name>, 5)
dlsym "gldInitializeLibrary" → call it; args include THE LOADER'S OWN
  CALLBACKS (rcx=gfxIODataFlush, r8=gfxIODataBindSurface — the
  constant shared addresses our stub logged as arg3/arg4)
dlsym "gldGetVersion" → call; FOUR out-ints
  RET must be nonzero (true)
  a3: bits ONLY within 0x0000FF00        (0x400 passes)
  a0 MUST == 3
  a1 MUST == 1
  a2 MUST == 0 (NULL)                    ← OUR STUB WRITES
                                         &_mh_bundle_header — REJECTED
  a3 |= 0x20000;  _gfx_float_device_id = 0x1020000 | (a3 & 0xFF00)
      = 0x1020400 — THE CENSUS RENDERER ID, composed here
then the name loop: _gfx_gld_names[0..0x4E] — 78 entries (NOT 92),
  each dlsym'd; ANY NULL → reject
ANY REJECT → 0x1669 gfxPluginDisconnect → Terminate + free
```
- **Consequences:** (1) our observed Initialize→Version→Terminate
  cycle was the REJECTION path every time — cause: a2 nonzero;
  (2) the rung-5 value tuple's a2 came from a MISREAD of
  GLRendererFloat's disasm (otool's symbol-displacement rendering
  showed $__mh_bundle_header where the real instruction writes 0 —
  the same rendering artifact flagged at rung 5 now corrected);
  (3) question 3 answered: the id transform is
  0x1020000 | (a3 & 0xFF00), computed in the loader;
  (4) question 2 answered: a2 is not dereferenced — it is CHECKED
  FOR ZERO; (5) question 1 answered: the consult (GetRendererInfo
  et al.) follows the 78-name loop, which our stub (92+2 exports)
  would fully satisfy ONCE VERSION VALIDATES.
- **RUNG 10 PRE-REGISTERED (one line; live-swap):** gldGetVersion
  writes a2 = 0 (NULL). Prediction: version validates → the 78-name
  loop resolves against our exports (all present) → the plugin
  REGISTERS → the next census consults gldGetRendererInfo — the
  stub's refusal (-1) is logged as the first consult datum. Outcomes:
  (1) consult fires (refusal logged; census unchanged-or-error =
  honest) → 7b activates with real outcomes;
  (2) consult fires AND census changes (unlikely with refusals —
  anything beyond -1 would be fabrication);
  (3) still rejected → the remaining gate is in the name loop or
  Initialize's return handling (read at 0x1669 predecessors);
  (4) destabilized → recovery unchanged.

**RUNG 9 SUPERSESSIONS + CORRECTIONS (same night, before rung 10):**

- **The rung-6b STANDING MODEL IS SUPERSEDED, not amended:** "the
  loader's cycle is a capability-registration pass" is dead — the
  Initialize→Version→Terminate sequence was the REJECTION path
  (0x1669 gfxPluginDisconnect), observed identically on every rung
  because the same a2 check failed every time. What a PASSING GLD's
  cycle looks like: no Terminate, the 78-name loop, registration —
  unobserved until rung 10.
- **Correction 1 (load-bearing, marked):** the a2=&_mh_bundle_header
  value came from an OTOOL SYMBOL-DISPLACEMENT MISREAD of
  GLRendererFloat's disasm (rung 5) — the real instruction writes 0.
  Third instance today of a tool presenting something plausible that
  wasn't there (uniq -c filter, ~/Info.plist, otool rendering). The
  artifact was flagged at rung 5 without its consequence being read —
  one misread instruction silently shaped five rungs.
- **Correction 2 (load-bearing, marked): the interface is 78 names,
  not 92.** The loader's table (_gfx_gld_names, extracted statically:
  78 pointers at file 0x6140+slice, entry 0 = gldGetVersion). The 92
  came from VMsvga2's EntryPointNames.c — a cross-era SUPERSET: the
  14 extras are the entries VMsvga2's own comments mark "Discontinued
  OS 10.6.3" (TextureLevel family) plus the 10.5.8-era
  MemoryPluginData/vertex families. Loader-78 ⊆ VMsvga2-92; the
  loader asks for nothing VMsvga2 lacks. The "92 entry points" job
  size quoted for months is corrected to 78.
- **RUNG 10 INTERSECTION CHECK — DONE STATICALLY BEFORE THE RUN
  (the pre-registered concern resolved):** the stub's exports cover
  ALL 78 (the diff's one "missing" name, gldGetVersion, was a grep
  artifact — the typed implementation exports it). The name loop
  will resolve completely; with a2=0, the predicted sequence runs to
  registration, and the failure mode after the loop shifts to
  whatever the loader consults next.

**RUNG 10 RESULT — THE CONSULT (21:53–22:07; baseline 22:07:42
gate=0):**
- Boot: **WindowServer PASSED validation — Load → GetVersion(TRUE,
  a2=0) and NO TERMINATE.** The plugin REGISTERED in WindowServer's
  process (stub log: constructor, Initialize, Version-true — and no
  disconnect). Desktop normal throughout; WindowServer stable.
- Census probes (both masks — main-display 0x1 and 0xFFFFFFFF):
  **CGLQueryRendererInfo now CALLS our gldGetRendererInfo** — the
  first consult of the arc. Our honest -1 refusal propagates: the
  query returns **-1, nrend=0**.
- **nrend=0 IS THE RISK DATUM, not a side note:** it was 1 before —
  the registry-named GLD REPLACES the float renderer in the consult
  rather than joining a list. An honest refusal now costs the system
  its only working renderer. Every previous rung's refusal was safe
  because something else still answered; **that protection is gone.**
  The refusal-registered state is NOT a safe steady state.
- Score: outcome 1 exactly as pre-registered (consult fires, refusal
  honest, census error = honest). The rung-6b question is closed:
  enumeration DOES ask the GLD; the silence was the rejection path
  all the way down.
- **RUNG 11 (7b activated) PRE-REGISTERED — the honest contract, with
  the risk re-registered:** gldGetRendererInfo implements the rung-5
  decoded record with our identity (own bundle header at +0, own
  mask, OUR renderer-id space in a3's 0xFF00 field, software-class
  capability words, kCGLBadMatch 0x2716 for masks outside the claim —
  the MASK PROTOCOL is now load-bearing). FIRST SUB-QUESTION: whether
  the float-renderer replacement is the MASK's doing — our Initialize
  mask is 0x1 (the loader's), the float renderer presumably claims
  the same; if a narrower/different claim in the RECORD leaves the
  float renderer enumerating alongside, coexistence is achievable and
  the single-renderer risk drops. **OUTCOME 3 (DESTABILIZED) IS
  RE-REGISTERED EXPLICITLY with elevated standing: this is the first
  rung where being wrong reaches WindowServer's rendering path — the
  desktop's software rendering is downstream of whatever the stub
  claims. Gate + live-swap revert to hand; desktop watch mandatory;
  slclean recovery standing.**

**RUNG 11a PRE-REGISTERED — the honest record claiming a display we
don't have (zero desktop reach by construction; committed before the
run):**
- **REVERT PATH CONFIRMED BEFORE THE RUN (not during):** WindowServer
  dlopens the GLD at boot and holds the mapping — a mid-session
  bundle swap CANNOT unload it from WindowServer. The broken-desktop
  revert is therefore: ssh → remove vm-cap3d from boot-args → reboot
  (ssh survives broken-desktop states — proven in the m3 black/white
  era; the arg-removal reboot is exercised successfully on every rung
  today). Slclean stands behind it. The live-swap speedup remains
  PROBE-ONLY.
- **The mask-claim instrument:** the record claims mask 0x2 — a
  display bit that does not exist on this single-display VM. The
  desktop's real rendering (main display, bit 0) can never consult
  this record; the float renderer answers everything real. 11a tests
  the record STRUCTURE, the mask protocol (kCGLBadMatch per the
  float's exact condition), and COEXISTENCE with zero reach into
  real GL.
- **The record (rung-5 contract, honest fields):** +0 qword = OUR
  bundle header (record anchor — the misread-corrected context: a2
  of VERSION must be 0, but the RECORD's +0 anchor is a pointer per
  the float's disasm); +8 = OUR renderer id (0x1AF40100 — our vendor
  space, not Apple's, no impersonation); +0x24 = 0x2 (the claim);
  caps/version/class fields = the float's SOFTWARE-class values
  (honest: our stub is software-class); modest limit constants where
  the float's fields were unreadable (documented as such — consumed
  only by describe-queries against a nonexistent display).
  Return 0 for query masks ⊆ 0x2; 0x2716 otherwise — the float's
  exact protocol.
- **Predictions:** (1) main-display census returns the FLOAT renderer
  again (nrend=1, rid=0x1020400) — COEXISTENCE CONFIRMED, the
  replacement effect is the mask's doing; (2) a probe querying
  display-bit-1 consults OUR record (first real record consumed) —
  census shows our renderer for that mask only; (3) main-display
  census still errors (-1/nrend=0) — the consult is slot-ordered,
  not mask-filtered; coexistence NOT achievable via mask; the
  single-renderer risk stands and 11b (claiming the real display)
  requires the revert path proven; (4) destabilized → the confirmed
  revert path.

**RUNG 11a RESULT — BOTH COEXISTENCE PREDICTIONS FALSIFIED; THE
EXCLUSIVITY IS STRUCTURAL (22:20–22:30; baseline 22:30:44 gate=0):**
- Instrument note: the first build shipped WITHOUT its executable —
  a `clang | grep -c` pipe masks compile failure (grep's exit wins;
  the build-failure discipline extended to pipes) — and the reboot
  had already fired. Harmless by luck AND design: WindowServer booted
  on stock GL (never loaded the broken bundle), and the LIVE-INSTALL
  of the fixed bundle on the already-gated boot made WindowServer the
  safest possible observer — only the probes consulted.
- Main-display census (q=0x1): OUR GLD consulted, honest
  kCGLBadMatch (claim=0x2) → **CGLQueryRendererInfo returned 10006
  (kCGLBadMatch), nrend=0 — the float renderer did NOT answer behind
  us. Prediction 1 FALSIFIED.**
- Query 0x2 (our claimed bit): **10006, NO stub log at all — CGL
  never queries a display that does not exist. Prediction 2
  FALSIFIED; a nonexistent-display claim can never be consumed.**
- **CONFIRMED (prediction 3): the registry-named GLD owns its
  display's renderer answer EXCLUSIVELY. No honest coexistence. The
  mask protocol itself works — our BadMatch was accepted and
  propagated exactly per the contract — but it cannot SCOPE the
  consult.**
- Blast radius (recorded): rung 10's desktop was normal with a
  refusing registered GLD — WindowServer's boot tolerates census
  failure; the substitute stack defines its own CGL and never
  consults the system loader (immune — the browser is unaffected).
  The blast radius of a registered GLD's answers = REAL-CGL apps
  that query renderers.
- Forward consequence: 11b (claim the real display, mask=0x1) makes
  our GLD THE renderer authority — enumeration would show our rid,
  and everything downstream (gldChoosePixelFormat, gldCreateShared,
  gldCreateContext — all currently refusals) becomes load-bearing
  the moment an app asks for a context. The Mesa-backed
  implementation begins there; the honest ladder's next rung is
  ENUMERATE-FIRST: claim 0x1, verify our rid appears in a census,
  revert before any context work.
- Field-packing fix made pre-run: the float's word/byte fields at
  +0x28..+0x2e are two dwords (0x00010004, 0x01000010).

**TOOL-FAILURE HABIT (named, 2026-08-21 — four instances, one class):**
uniq -c hiding stderr errors; ~/Info.plist supplying a name nobody
configured; otool's symbol displacement inventing &hdr; `clang | grep
-c` eating a build failure's exit status. Each presented a plausible
result that was never there; each cost real time; each was caught by
looking at something OTHER than the tool's output. THE RULE: when a
tool's output drives a decision, check the tool's own failure mode
first — exit status read directly (no pipes over build commands),
positive controls on greps, opened-file enumerations over
where-could-it-come-from reasoning.

**RUNG 11b PRE-REGISTERED — ENUMERATE-FIRST (claim the real display;
revert-before-context is the boundary; committed before the run):**
- ONE value changes: RUNG11_CLAIM 0x2 → 0x1. Everything else
  identical to 11a (record fields, software-class caps, our id
  0x1AF40100 at +8, kCGLBadMatch outside claim).
- **The risk shape is new (registered):** the moment the census
  returns a real renderer, every downstream refusal becomes
  load-bearing AT ONCE (gldChoosePixelFormat, gldCreateShared,
  gldCreateContext) — the first rung where the honest answers cannot
  all stay "no." The probes create NO contexts (revert-before-context
  honored by instrument design); the desktop watch is the WindowServer
  exposure check; revert path proven (arg + reboot; live-swap is
  probe-only).
- **Predictions for the census (main display, q=0x1):**
  (1a) nrend=1 with OUR record's id 0x1AF40100, accelerated=0 —
  the record is respected;
  (1b) nrend=1 with rid=0x1020400, accelerated=0 — the loader-composed
  id wins (rung 9 decode: _gfx_float_device_id = 0x1020000 |
  (version_a3 & 0xFF00) — composed from OUR a3=0x400 at registration;
  the record's +8 may be a secondary field). Either way, ONE renderer
  enumerated and it is US — the float renderer absent (11a's
  exclusivity);
  (2) census errors or empty — the record failed validation
  downstream (the loader re-checks fields it consumed from +0x3c..
  — our modest 256s insufficient; the unread-fields problem lands);
  (3) desktop destabilized at boot (WindowServer consumes the real
  record) → proven revert path.
- Procedure: build (exit status read DIRECTLY — the habit's first
  application), bundle + gate + reboot, desktop watch, census probes
  only, score, restore baseline.

**RUNG 11b RESULT — ENUMERATED (22:46–22:53; baseline 22:53:17
gate=0). THE ARC'S MILESTONE: nrend=1 now means OURS.**
- Boot: WindowServer registered (pid 95, Load→Version-true, no
  Terminate); desktop NORMAL with the real-display claim live
  throughout.
- Census (q=0x1): **error 0, nrend=1, renderer[0] rid=0x1af60100
  accelerated=0.** Our record's id 0x1AF40100 with the loader's
  +0x20000 transform; software-class caps taken at face value
  (accelerated=0 — the honest-caps convention SURVIVES INTO
  ENUMERATION: the loader does not override caps from the id class;
  we can enumerate truthfully as software while the Mesa backing is
  built); the modest-256 limit fields consumed without validation
  failure; exclusivity held (nrend=1 — only us; float absent).
- Revert-before-context honored: NO downstream entry called; probes
  created no contexts. The habit's first catch happened pre-run
  (mkdir-after-clang ordering; exit read directly; no broken
  deploy).
- **ID-CHANNEL DISENTANGLEMENT (correction to the rung-9 decode):**
  the census rid comes from **record[+8] | 0x20000**, NOT from the
  version-composed `_gfx_float_device_id` (0x1020000 | a3&0xFF00).
  The two were CONFLATED because for the float both produce
  0x1020400 (0x1000400|0x20000 = 0x1020000|0x400 — coincidence);
  our record split them (version channel would still say 0x1020400,
  census says 0x1AF60100 — the RECORD channel wins). The ledger's
  pre-11b census observations of 0x1020400 remain true as
  observations; the rung-9 line "= THE CENSUS RENDERER ID, composed
  here" stands corrected by this entry. `_gfx_float_device_id`'s
  actual consumer is unidentified (next-session read: xrefs to the
  global in the libGFXShared disasm, already on disk at /tmp —
  no boot needed).
- The stale "claim=0x2" log literal was FIXED IMMEDIATELY (the
  caught-today-kill-today rule): the RECORD log line no longer
  prints a hardcoded claim; the call line prints the live value.
- **Forward (the arc's next frontier, named):** the context path —
  the first rung where refusals stop being free. gldChoosePixelFormat,
  gldCreateShared, gldCreateContext become load-bearing the moment
  anything asks for a context on the enumerated renderer; the
  honest ladder is pixel-format-first (mirror the float's
  gldChoosePixelFormat by disassembly — the same acceptance-path
  method that produced the record contract), then Mesa-backed
  contexts.

## 2026-08-21 (rung 12) — PRE-REGISTERED: pixel-format-first from the float's disassembly

**Evidence base (decoded BEFORE registering, from /tmp/grf.t —
GLRendererFloat `_gldChoosePixelFormat` @0x17892, 426 lines):**
- Signature confirmed: `GLDReturn gldChoosePixelFormat(void**
  struct_out, int* attributes)` — rdi=out, rsi=attrs, walked +4/code.
- Opens with `glsAllDisplayMask()` → r13d → lands at obj+0x34.
- The attribute parser is a JUMP TABLE over codes 0..0x56 (86 codes)
  accumulating into stack locals (-0x82..-0x74 words, r12/r14/r15).
- **Success epilogue (0x17e7e–0x17f00): malloc'd ~0x38-byte object:**
  +0 anchor qword; **+8 = 0x1000400 (the renderer id — the SAME id
  field as the record's +8; ours = 0x1AF40100)**; +0xc = 0x4C8
  (constant); +0x14 = 0x8000 (constant); +0x34 = all-display mask;
  +0x10/+0x18/+0x1c/+0x20/+0x24..+0x2c = attribute-derived words;
  +0x31..0x33 zero bytes; `*struct_out = obj`; **return 0.**
- Error path returns **0x2718 = 10008 = kCGLBadAttribute** (the
  CGL-level error observed historically on bad attribute lists).

**Phase A (remaining decode, no boot — outcomes for phase B fixed in
an addendum only after this is read; the unread-gate rule):**
- Resolve the 86-case jump table: per attribute code — accepted
  (effect on which local), ignored, or 0x2718. Specifically the
  ACCELERATED attribute's case (the honest boundary: the float is
  software and yields no accelerated formats — read exactly how it
  declines).
- The libGFXShared CALL SITE (in /tmp/gfx.t): what the loader passes
  (raw caller attrs? pre-processed?) and what it does with our return
  and the object.
- Misread discipline elevated (the &hdr class): every otool
  `__mh_bundle_header(...)` symbol-displacement rendering in this
  function gets MANUALLY resolved before trusting; two-source
  cross-check (float impl + loader call site) wherever both are
  readable.

**Phase B (implementation, gated live-swap; boundary: FORMATS ONLY —
probe_r7 mode p is the instrument, it never creates contexts):**
- Typed gldChoosePixelFormat: accept the software-honest attribute
  sets per the phase-A table; 0x2718 where the float rejects; build
  the object per the decoded shape with OUR id at +8 and the decoded
  constants; *out=obj; return 0.
- gldDestroyPixelFormat mirrors the lifecycle (trampoline header:
  "calls free(struct_in), returns 0") — the object we malloc must be
  freeable by our own destroy.
- Risk (registered): pf objects are CONSUMED (CGLDescribePixelFormat
  and, later, context creation) — a malformed object crashes
  consumers; WindowServer exposure if it asks; desktop watch
  mandatory; revert path proven (arg + reboot).
- Predictions: fixed in the phase-A addendum before phase B runs.

**PHASE A ADDENDUM — the full case map; predictions FIXED (committed
before phase B):**
- Jump table decoded (87 cases): VALUE-TAKERS {3,4,7 AuxBuffers,8
  ColorSize,9,10,51 MinPolicy,52 MaxPolicy — consume next int};
  FLAGS {0→r8=1 (THE BUILD GATE), 1 AllRenderers→local74|=8,
  49→r12|=4 + si, 54 FullScreen→r10=1, 55 SampleBuffers→r14=2,
  56 Samples→r14=1, 76 BackingStore→r12|=1, 86→r12|=0x2000};
  NO-OP PASS {47,48,72 NoRecovery}; MASK {80 Window→r13 &= attr
  value}; IMMEDIATE-RETURN-0-NO-OBJECT {2,50,53 OffScreen};
  DEFAULT-TRUNCATE {the remaining 63 codes — incl. DoubleBuffer,
  Stereo, Alpha/Depth/Stencil/AccumSize, RendererID, **ACCELERATED,
  Robust**, MPSafe, Compliant, DisplayMask: `addq $0xc0,%rdx` skips
  the rest; build from accumulated}; attr-walk overflow → 0x2710
  (10000); post-loop: mask==0 → return-0-no-object; and **the object
  builds ONLY when attribute code 0 is present** (r8 gate at 0x17c06).
- **The accelerated npix=0 mechanism is NOT this parser** — Robust
  shares the identical truncate path yet yields npix=1 historically:
  the accelerated filter lives ABOVE the GLD (renderer/record level;
  11b's accelerated=0 record produced the same npix=0).
- **Micro-question folded into phase B instrumentation:** does the
  loader PREPEND attribute 0 (the build gate)? Our entry logs the raw
  array; predictions branch on the observation.
- **Predictions (probe_r7 mode p sets):** (a) accelerated → NO GLD
  call at all (renderer filter; stub log silent for that set);
  (b) offscreen → immediate return-0-no-object → **npix=0 — a
  deliberate divergence-from-history prediction** (the float's
  shortcut, invisible while it answered, now visible with us as the
  consulted GLD); (c) robust / ALL_RENDERERS → truncate → npix=1
  with OUR object if attr 0 present, npix=0 if not (the logged array
  decides); (d) the built object survives CGLDescribePixelFormat
  (mode d's walk) without crash; (e) destabilized → proven revert.

**RUNG 12 PHASE-B RESULT — CONSUMER BUS ERROR, CONTAINED; TWO
PREDICTIONS FALSIFIED; THE NEXT GATE IS THE RECORD→REQUEST MAPPING
(23:38–23:50; baseline 23:50:04 gate=0):**
- Boot: WindowServer registered (pid 97, no Terminate); desktop
  normal throughout — the crash that followed was PROBE-CONTAINED
  (the registered risk class firing exactly as written: a consumed
  response crashed the caller; WindowServer never asked).
- Census + describe-walk (mode d): our record enumerated and its 25
  properties consumed WITHOUT crash (prediction (d) held for the
  record path).
- **Mode p: SIGBUS (exit 138) at the FIRST set — `pf(accelerated)`.**
  Per-set flush markers (added after the first crash swallowed all
  buffered output — the stdout-buffering instrument lesson)
  attributed it exactly. The stub log shows the sequence:
  `gldChoosePixelFormat(raw16=[0x4 0x0 0x0 0x0 <stack garbage>])`
  → our mirror returned 0-no-object → the caller dereferenced the
  unset out → bus error.
- **Falsified predictions:** (a) accelerated DOES reach the GLD
  (no renderer-level pruning at this layer — or the consult happens
  regardless); the loader REWRITES the attribute list — the caller's
  set is not what arrives; the array passed is **`{4, 0}`** — an
  internal two-int request, not CGL attributes.
- **The decisive inference (marked):** success-without-object is an
  INVALID return from our position — the caller dereferences *out on
  success. The float's identical no-object paths therefore never fire
  in practice — implying the loader CONSTRUCTS its `{4, 0}` request
  FROM THE RENDERER RECORD, and OUR record (modest fields, 256
  limits) yields the degenerate request. The record→request mapping
  is the next unread gate: read it in the libGFXShared disasm (what
  builds the request array from the record before the pf call) BEFORE
  the mirror changes again. Iteration stopped at the gate per the
  unread-gate rule.
- Phase-B scope discipline held: formats only, no contexts, probe
  contained, desktop watch clean, live-swap used twice (the second
  after the instrumentation fix), revert unneeded (baseline restored
  routinely).

**THE THREE FINDINGS FROM RUNG 12, SEPARATED BY REACH (recorded
before the mapping read):**
1. **CONTRACT FACT — the loader rewrites the request layer:** the
   GLD never sees the caller's attribute set; the 87-case map
   describes the loader's internal front-end protocol, not anything
   caller-driven. Every attribute-level hypothesis of the arc was
   aimed one layer low.
2. **SAFETY FACT — the refusal convention's boundary:** the caller
   dereferences *out on success, so 0-with-no-object is
   indistinguishable from a lie. AUDIT RESOLVED EMPIRICALLY: nonzero
   refusals are safe everywhere (rung 10 proved it live —
   gldGetRendererInfo -1 propagated as a clean census error; the
   caller's own teardown confirms it, below); the crash came from
   the MIRROR copying the float's success code without the object.
   RULE: pointer-out entries either refuse NONZERO or succeed with a
   real object — never 0-with-nothing. Covers ChoosePixelFormat +
   all creators + struct-fills by the same argument.
3. **THE NPIX=0 FILTER RELOCATED ABOVE THE GLD:** the wall the flip
   experiment measured (npix=0 accelerated) was never the driver's
   to lift — and now sharper: the accelerated request reached the
   GLD via shared-state creation, so the filter sits in CGL's
   post-shared-state attribute matching, not in renderer
   consultation. "Make the GLD claim acceleration" was aimed one
   layer too low throughout.

**THE RECORD→REQUEST MAPPING READ (libGFXShared disasm, no boot) —
THE FRAME OVERTURNED: the call is not attribute-shaped:**
- The pf call site (0x1855, table base 0x130 → slot 2) lives in
  **_gfxCreateSharedState** — CGL's shared-state creation (the
  context path's prerequisite). CGLChoosePixelFormat internally
  builds shared state, consulting EVERY registered driver — which is
  why the accelerated set reached the GLD (no pre-filter at
  consultation).
- The call is THREE-arg: `gldChoosePixelFormat(void** slot_out,
  <device+0x14 field>, 4)` — the "attributes" pointer is a field of
  the LOADER's device struct (built at registration), and **edx=4 is
  a constant third argument** — the trampoline header's 2-arg
  signature is incomplete (same class as Initialize's 6th arg).
  Our raw16 {4, 0, 0} = the device-struct region the pointer aims
  at.
- **The caller's own code confirms the safety rule:** `testl %eax;
  jne teardown` — nonzero takes the CLEAN failure loop
  (DestroyPixelFormat on created slots + free); success (0) is a
  promise of a valid slot at shared+0x168+r13*0x20 — the promise my
  no-object return broke, hence the SIGBUS.
- OPEN (named, one 20-line read away): what populates the
  device-struct target at +0x14 (registration-time code), and how
  the float satisfies this exact call — the float's zero-terminated
  parser on {4,0,0...} truncates at case-4-consumes-0 and its r8
  gate needs attr code 0, which a zero-terminated list can never
  deliver — either the float builds via a subtlety misread, or
  device+0x14's content differed in its era. THE HONEST MIRROR FIX
  (pre-registered as the next rung's opening move): the no-object
  paths return NONZERO (0x2716-class) — clean teardown, never the
  dereference lie.

**[device+0x14] PROVENANCE — RESOLVED BY THE STRUCT READ; the entry
above SUPERSEDED (three corrections cascade):**
- `_gfxGetDeviceWithDeviceID` walks `_gfx_device_head`: device =
  malloc(0x18) — **next@+0, plugin@+8, deviceID@+0x10, mask@+0x14
  = `1 << display_index`** (creation at 0x103c–0x1075; the same
  mask is OR'd into plugin+0x118 — the plugin's accumulated display
  claim). **[device+0x14] is a DISPLAY BITMASK, LOADER-INTERNAL,
  NOT record-derived — the record-shapes-the-request inference is
  DEAD.** (The 0x1405 write site found earlier belongs to a
  different block — the display-table path — not this struct.)
- **CORRECTION CASCADE 1 — the raw16 "descriptor" was GARBAGE:**
  _gfxCreateSharedState passes the MASK as gldChoosePixelFormat's
  second argument; our stub treated mask 0x4 (= 1<<2, the EXTENDED
  display — matches this VM's second display) as a POINTER and read
  address 4 — an illegal dereference this VM's layout happened to
  permit, yielding low-memory garbage that we mistook for a
  pointer-bearing descriptor. The "descriptor" reading is dead.
- **CORRECTION CASCADE 2 — arg2 of gldChoosePixelFormat is a display
  mask (int), not an attribute pointer** — the trampoline header's
  signature fails a THIRD time, now on parameter MEANING. The
  honest stub: interpret arg2 as a mask, never dereference it.
- **CORRECTION CASCADE 3 — the float's 87-case parser serves a
  DIFFERENT CALLER:** GLRendererFloat is NOT in the device list
  _gfxCreateSharedState walks (it is the Resources/ software
  fallback, consulted by CGL's software path with REAL attribute
  lists). The map remains valid for the float's own caller — but it
  was never the contract OUR pf entry faces. Our entry's real
  contract (from the only call site): (slot_out, display_mask,
  const 4); success=0 promises a slot; nonzero refuses cleanly.
- **The request-layer separation, now clean:** the RECORD feeds
  ENUMERATION (proven, rung 11b); the pf REQUEST inputs are the
  driver-id array (caller/CGL side) + loader-internal device masks.
  Next rung's fix list: pf entry interprets (out, mask, 4);
  no-object paths return NONZERO; never dereference arg2.

**RUNG 13 PRE-REGISTERED — the honest pf entry with the mask contract
(committed before implementation):**
- The entry becomes `long gldChoosePixelFormat(void** out, int
  display_mask, int four)` — arg2 is a MASK (never dereferenced);
  the 87-case parser is RETIRED for our entry (it serves the float's
  CGL-software-path caller, not ours).
- Behavior: mask outside our claim (0x1) → return 0x2716
  (kCGLBadMatch — nonzero, the caller's clean teardown); mask within
  claim → build the 0x38-byte object (our id at +8, 0x4C8/+0xc,
  0x8000/+0x14, the REQUESTED mask at +0x34), *out = obj, return 0
  — success with a real slot, per the call-site contract.
- **Predictions:** (a) NO SIGBUS anywhere in mode p — the
  no-object-lie class is eliminated by construction (both paths
  return either nonzero or a real object); (b) the accelerated set
  reaches shared-state creation as before; the pf call's mask
  decides: 0x4 (extended display) → 0x2716 clean refusal; 0x1 →
  object; either way the caller proceeds without crash — npix
  outcomes secondary; (c) desktop stable (WindowServer exposure
  unchanged — it has not asked); (d) if an object is accepted, the
  shared-state path CONTINUES — the stub log may show the NEXT entry
  called (the next refusal frontier, likely gldCreateShared-class);
  its nonzero refusal should again take a clean path.
- Procedure: build (exit read directly), bundle + gate + reboot,
  desktop watch, probe_r7 p + c + d, score, restore baseline.

**RUNG 13 RESULT — a SECOND CALL SITE with a different shape;
nonzero refusal did NOT prevent the crash (08:37–08:52; baseline
08:52:27 gate=0):**
- Boot: WindowServer registered (pid 95, Version-true, no
  Terminate); desktop normal; probe-contained crash again;
  WindowServer never asked.
- The entry's first true args, logged:
  `gldChoosePixelFormat(out=0x7fff5fbff898, mask=0x5fbffa00, four=0)`
  — **rsi is a STACK POINTER (the caller's attribute buffer) and
  rdx=0 — NOT the _gfxCreateSharedState shape (edx=4, esi=device
  mask field).** A SECOND pf call site exists, and IT is the one
  that fires for the accelerated set. The rung-12 raw16 `{4,0,0,0}`
  was REAL: the caller's stack attribute array, first element 4.
- Our 0x2716 refusal returned; **the caller crashed anyway (SIGBUS,
  exit 138)** — this call site dereferences *out REGARDLESS of the
  return value, or checks it differently. The teardown-safety
  proven at _gfxCreateSharedState does NOT apply here.
- Predictions: (a) FALSIFIED (nonzero did not save us); (b) the
  mask-decision never applied (wrong call-site contract); (c) HELD
  (desktop stable); (d) N/A.
- **The mask reading of rung 12's cascade-2 stands CORRECTED
  AGAIN:** arg2 is a stack attribute pointer at THIS site — the
  87-case parser interpretation is back in play for THIS caller
  (the float's parser may serve exactly this site after all; the
  device-mask reading applied only to the OTHER site).
- **Decisive next read (named):** identify the second call site in
  gfx.t — the one passing a stack buffer and rdx=0 — and read its
  return handling: does it check eax at all before using *out? The
  honest entry for THIS caller may have to ALWAYS write a valid
  object (there may be no safe refusal).
- Network note: post-boot mDNS loss again (~15 min, desktop normal,
  third episode); the IP route (ARP-cache 192.168.64.40 + the
  config's key + legacy algorithms) unblocked everything — the
  ssh-via-IP procedure is now the standing workaround.

**SECOND CALL-SITE READ (libGFXShared + OpenGL framework disasm; no
boot) — THE ANSWER IS A THREE-LAYER CORRECTION:**
1. **The plugin function table base is 0x120, NOT 0x130**
   (`leaq 0x120(%r12), %r13` at 0x173b — the rung-9 base was
   off by one slot). Every table-offset attribution shifts:
   `*0x140` = slot 4 = **gldCreateShared** (NOT
   gldChoosePixelFormat); `*0x148` = slot 5 = gldDestroyShared;
   slot 2 (gldChoosePixelFormat) = offset **0x130**.
2. **_gfxCreateSharedState (0x17bc) calls gldCreateShared — not
   our pf entry.** The (slot_out, device_mask, 4) contract decoded
   in rung 12 belongs to gldCreateShared, not gldChoosePixelFormat.
3. **NO call through *0x130 exists anywhere in libGFXShared.** Our
   pf entry is NEVER called through the plugin table in
   libGFXShared — the caller is in the **OpenGL framework binary**
   itself: CGLChoosePixelFormat (@0x14c5 in OpenGL.framework) →
   internal helper at **0x9660** (a ~4KB format-selection engine
   that uses CGS display info, glcPluginCount, glcGetIOAccelService,
   then per-display enumeration) — and somewhere inside that helper
   the GLD's pf slot is invoked with the OBSERVED shape
   (out, stack_attrs, 0).
- **The {4,0,0,0} rung-12 datum was the caller's REAL attribute
  array** — the OpenGL framework's internal format-selection
  engine passing the app's CGL attributes (converted to internal
  codes) on its stack. The 87-case parser reading is back in play
  for THIS caller.
- **Practical consequence: the call-site contract for our pf entry
  is in the OpenGL framework's 0x9660 helper, not in
  libGFXShared.** The next read is that helper's indirect-call
  region (the call through the plugin table with three args and
  rdx=0) and its return-value handling — whether it checks the
  GLD's return before dereferencing *out. /tmp/ogl.t has the
  disassembly (26582 lines; 0x9660-0xa65c is the function's span).

**THE PF CALLER FOUND AND READ — GLEngine.bundle, not the OpenGL
framework (GLEngine /tmp/gle.t, gliChoosePixelFormat @0x13cf;
the 0x9660 helper in the framework is a SCORING function, not the
caller):**
- **The call chain is THREE layers deep:** App → CGLChoosePixelFormat
  (OpenGL framework 0x14c5) → … → **GLEngine.bundle's
  gliChoosePixelFormat (@0x13cf)** → plugin->slot[2](*0x130) = our
  gldChoosePixelFormat. The framework's 0x9660 helper does format
  SCORING (its *%r15 calls are local comparator dispatch, not
  plugin-table calls). The actual GLD invocation happens in
  GLEngine — between the framework and the GLD.
- **The true contract, decoded:**
```
gliChoosePixelFormat(pix_out /*rdi*/, attrs /*rsi — the CALLER'S RAW
                     CGL attribute array, unmodified*/) {
    *pix_out = NULL;
    for (plugin = gfxGetPlugins(); plugin; plugin = plugin->next) {
        struct slot_obj* local;              // rbp-0x38, NEVER INITIALIZED
        rc = plugin->slot[2](&local, attrs, /*rdx never set — garbage/0*/);
        if (local != NULL) {                 // IS checked (safe for NULL)
            walk slot list; |0x20000 to id@+8; link to *pix_out chain
        }
        if (rc != 0) break;                  // nonzero = stop (checked)
    }
    if (rc != 0 && *pix_out) gliDestroyPixelFormat(*pix_out);
}
```
- **THE CRASH MECHANISM (rung 12/13's SIGBUS, finally explained):**
  rsi IS the caller's raw attribute array (no rewriting, no mask —
  the rung-12 {4,0,0,0} was real CGL attribute data on the stack).
  rdx is NEVER SET — whatever garbage was in rdx arrives as the
  third argument (observed 0). The caller DOES check the return
  (nonzero breaks the loop) and DOES check *out (NULL skips
  linking). **The bug: `local` at rbp-0x38 is NEVER INITIALIZED —
  our refusal (0x2716 or 0) left whatever garbage was on the stack
  in that slot; the caller then dereferenced it at 0x142d.**
- **THE FIX (one line): our pf entry must ALWAYS write NULL to *out
  on refusal** — `if (out) *out = NULL;` before any return path.
  With that, a nonzero refusal + NULL out = clean break: the caller
  checks rc≠0, skips linking, exits, returns error to CGL. No
  crash.
- **The attribute semantics question reopens for THIS entry:** rsi
  IS the caller's real CGL attribute array (the 87-case float
  parser contract IS the right model here). The honest pf entry for
  this caller: parse the actual attributes, build a real format
  object for software-honest sets, refuse with NULL-out for the
  rest.

**THE RDX FINDING — the header's third and worst failure (recorded
separately for reach):** the caller NEVER SETS rdx. Site 1's edx=4
is a coincidence of that call site's register state, not a
contractual third argument. Any implementation reading a third
argument here is reading NOISE. This is worse than an incomplete
signature — the header describes a parameter that DOES NOT EXIST
at this site. Three header failures now: Initialize (6 args not
5), ChoosePixelFormat (2 meaningful args, third is garbage), and
the 92≠78 count.

**THE OUT-ZERO RULE — applied across ALL entries in one pass (not
just the pf entry):** every entry's refusal path writes NULL to
its out-parameter before returning. The EPR/EPB/EPV macros now
take six void* args and zero the first if non-NULL (the commonest
out shape). The GLEngine caller's stack slot is never initialized
— both sides assumed the other owned it. Without this rule, the
refusal convention that protected the ladder is silently
incomplete: the next entry exercised would have the same
uninitialized-slot crash. Risk accepted: if an entry's first arg
is an integer handle rather than a pointer, writing through it
faults — mitigated by the gated boot and the per-entry typed
signatures as call sites are read.

**RUNG 14 PRE-REGISTERED — the honest pf entry with the parser
restored, out-zero rule live (committed before any boot):**
- gldChoosePixelFormat: *out = NULL FIRST (the rule); then the
  87-case float parser (rung-12's correct work, now aimed at the
  correct caller); shortcuts (2/50/53) return 0 with *out NULL;
  truncate-default → build path; no-attr-0-gate → 0x2716 + NULL
  out (changed from rung-12's return-0 — NULL-out makes it safe);
  object built → 0 with real slot.
- The 87-case map was correct work aimed at the wrong site for
  two rungs. It now aims at the site it was decoded for.
- Predictions (probe_r7 mode p): (a) NO SIGBUS — the
  uninitialized-slot mechanism is eliminated by *out = NULL;
  (b) accelerated set → TRUNCATE (attr 73 is default) → parser
  truncates → object built (software caps) → npix=1 — the
  honest answer for "give me any format"; (c) offscreen →
  shortcut (attr 53) → return 0, out NULL → npix=0; (d) the
  built object's consumption path exercised — the next entries
  called (CreateShared at minimum, since the caller chains) take
  the out-zeroed refusal cleanly; (e) destabilized → proven
  revert.
- Procedure: build (exit checked), bundle + gate + reboot,
  desktop watch, probe_r7 p + c + d, score, restore.

**RUNG 14 RESULT — NO SIGBUS; THE PF ENTRY IS SAFE AND READING
REAL ATTRIBUTES; THE ACCELERATED FILTER IS CGS-SIDE, ABOVE THE GLD
(10:43–10:4x; baseline restored):**
- Boot: WindowServer registered (pid 97, Version-true, no
  Terminate); desktop normal; probe completed — **exit clean, all
  six pf sets returned (prediction (a) CONFIRMED).**
- **The parser read REAL CGL attribute arrays for the first time:**
```
attrs=[0x4]        → value-taker consumes terminator → no gate → 0x2716 + NULL
attrs=[0x35 0x4]   → attr 53=OffScreen → shortcut → 0 + NULL
attrs=[0x5 0x4]    → attr 5=DoubleBuffer → TRUNCATE → attr 4 → no gate → 0x2716 + NULL
```
- **Only 3 of 6 CGL calls reached the GLD** — the accelerated sets
  were filtered at the CGS display-matching layer with
  `"invalid display"` errors (kCGErrorFailure) BEFORE the GLD was
  consulted. The accelerated npix=0 mechanism is now precisely
  located: **CGS display-matching, not renderer consultation.**
- **The attr-0 gate NEVER fires for CGL-driven calls** — code 0 is
  the loader's internal marker, not part of the CGL attribute
  format. The float also returns no-object for CGL calls without
  it. npix=0 everywhere is the honest answer for a software-class
  renderer that can't match the requested attributes.
- Census still healthy (nrend=1, rid=0x1AF60100); WindowServer
  stable throughout.
- Score: predictions (a) and (c) CONFIRMED; (b) PARTIAL (the
  accelerated set never reached the GLD — CGS filtered it first);
  (d) the downstream entries were NOT called (the caller didn't
  chain into CreateShared because no object was built); (e) N/A.
- **The "invalid display" CGS errors ARE the next datum** — they
  name the layer where acceleration is refused. Three candidates
  for the cause, in order: (1) the capability booleans — the flip
  experiment called them "necessary but not sufficient" when no
  accelerated renderer EXISTED to be sufficient FOR; **one does
  now** — the flip + registered GLD combination has never been
  tested together; (2) the record's display mask vs CGS's
  association (free check: ioreg display mask vs our claim 0x1);
  (3) EDID (excluded if (1) and (2) come back clean — nothing
  recorded connects EDID to accelerated-format matching).

**RUNG 15 PRE-REGISTERED — the flip + registered GLD, together for
the first time (committed before any boot):**
- **The experiment the flip experiment's own conclusion named but
  never ran:** "necessary but not sufficient" was measured with
  NO accelerated renderer in existence. Now one exists — our GLD,
  registered, enumerated, answering honestly. The flip's
  insufficiency may have been purely the missing renderer.
- **Free pre-check (no boot):** ioreg the display mask CGS
  associates with the real display vs our record's claim (0x1).
  A mismatch produces "invalid display" without any property
  being wrong.
- **The change:** NOTHING new — the existing gated kext (vm-cap3d
  publishes booleans + IOGLBundleName + AccelCaps) + the existing
  rung-14 stub. Both halves together under one gate. The only
  variable vs rung 14 is that BOTH the booleans AND the GLD are
  live (rung 14 already had both — but the accelerated pf was
  tested without checking whether CGS's display-matching layer
  reads the booleans).
- **Actually, rung 14 already ran the combination** — the
  "invalid display" errors happened WITH the flip on. The
  combination is already tested and the answer is: CGS still
  rejects accelerated formats. The booleans alone (with a
  registered GLD) did not unblock CGS's accelerated path.
- **REVISED next datum:** the mask check (free, no boot) — if
  CGS's display mask ≠ our claim 0x1, that's the "invalid
  display." Then the EDID question (which VMsvga2 injected for a
  reason — Displays preferences resolutions, but possibly also
  feeding CGS's display-association table).

**MASK CHECK RESULT — CLEAN (same day, no boot):**
```
main display id=1535231424
CGDisplayIDToOpenGLDisplayMask(main) = 0x1
online displays: 1
  display[0] id=1535231424 mask=0x1
```
**CGS mask = our claim = 0x1. The mask is NOT the "invalid display"
cause.** Single display, mask 0x1, our record claims 0x1 — exact
match. The candidate order is now: (1) booleans+GLD — ALREADY
TESTED, negative (rung 14 ran both live); (2) mask — NOW TESTED,
clean; (3) EDID — the remaining candidate. The "invalid display"
CGS error's cause is narrowed to EDID or something else entirely
in CGS's accelerated-display qualification.

**EDID CHECK RESULT — ABSENT, AND THE CAUSE IS FOUND (no boot):**
- `ioreg` shows NO IODisplayEDID property — the display's EDID
  data is entirely missing from the registry (only ConnectFlags
  and PrefsKey exist under the display nodes).
- **The mechanism:** our kext CLAIMS HDDC support
  (`kConnectionSupportsHLDDCSense` returns Success at
  VMVirtIOFramebuffer.cpp:1903) but implements NEITHER
  `getDDCBlock()` NOR `hasDDCConnect()` — the two methods that
  actually serve EDID data. The base-class defaults return
  nothing; IODisplay gets no EDID; the registry reflects its
  absence. The claim is unbacked — the same over-claiming shape
  as `crsr=1` and AccelCaps-before-surface-path.
- **The worked example implements both** (VMsvga2.h:149-150,
  VMsvga2.cpp:722-744): `getDDCBlock()` serves from an m_edid
  buffer; `hasDDCConnect()` gates the claim.
- **Hypothesis (marked):** CGS's accelerated-display qualification
  reads EDID; its absence produces "invalid display" for
  accelerated format requests. VMsvga2 implemented EDID for a
  reason — the reason may extend beyond Displays preferences into
  CGS's display-acceleration table. **Settle-it test:** implement
  getDDCBlock() returning a minimal valid EDID (the worked
  example's, or a standard 128-byte base block with the correct
  timing for the current mode), boot, and check whether
  (a) IODisplayEDID appears in ioreg; (b) the "invalid display"
  CGS errors vanish; (c) accelerated pf requests pass CGS and
  reach our GLD.

**EDID HYPOTHESIS TEST RESULT — FALSIFIED (11:40–11:44; kext
10bc1ae6 with getDDCBlock/hasDDCConnect, EDID live, gate+GLD on):**
- EDID IS in the registry (IODisplayEDID count 2; getDDCBlock
  logged at boot; hasDDCConnect returning true).
- **Resolution changed to 1024x768** — the display subsystem
  responded to EDID data it never had. A real side effect: EDID
  presence changes mode selection (IODisplay may prefer its own
  timing interpretation over our DTD, or the framebuffer's mode
  table limits the choice — separate question).
- **BUT: "invalid display" CGS errors PERSIST — identical to
  rung 14 (no EDID):** pf(accelerated) → 10006 with the same
  CGS error; all results and GLD call pattern unchanged.
  **EDID is NOT the cause.**
- **All three candidates exhausted:** booleans+GLD — negative;
  display mask — clean; EDID — FALSIFIED. The cause is none of
  the three. The remaining locus: CGSGetDisplayOpenGLDisplayMask
  (called by the framework's 0x9660 helper) — the CGS-side
  display-to-OpenGL mapping fails for our display, and none of
  the tested properties control it. Deeper CGS internals.
- The EDID implementation STAYS (correct behavior — the display
  has real EDID for the first time; the HDDC claim is now
  backed). The 1024x768 mode selection is a mode-table
  interaction to investigate separately.

**THE FOURTH CANDIDATE — OUR OWN RECORD'S accelerated=0 (recorded
before any test; the rejection may be about the RENDERER, not the
display):**
- The three exhausted candidates all assumed the CGS "invalid
  display" rejection was about the display. A candidate not on
  the list: **the record says accelerated=0.** Honest
  software-class caps have been maintained throughout, and
  enumeration respects them (rung 11b: accelerated=0 taken at
  face value). If the display-matching layer asks "is there an
  ACCELERATED renderer for this display," the answer from our
  own record is no — and rejecting an accelerated pixel format
  would be CORRECT BEHAVIOR, not a gate.
- **10006 = kCGLBadDisplay = 0x2716 — the same constant as the
  refusal code.** The site producing it is findable the same way
  site 2 was: search the three binaries for stores of $0x2716
  on the display-matching path.
- **RUNG 16 PRE-REGISTERED (one bit, gated, revertable):** set
  the record's accelerated flag (byte at +0x2e = 1 instead of
  the current honest software-class value) — the first time
  claiming acceleration is JUSTIFIED AS A PROBE rather than an
  over-claim, because the claim is being TESTED. Outcomes:
  (a) accelerated sets REACH the GLD (the "invalid display" was
  our own accelerated=0 reflected back) → hypothesis confirmed;
  the honest position afterwards is to REVERT the bit until Mesa
  backs it — a probe result, not a production claim;
  (b) "invalid display" persists → the accelerated flag is not
  the criterion either; the next move is the $0x2716-site read
  in the three binaries (the code itself, not more candidates);
  (c) destabilized (a system that believes it has an accelerated
  renderer will route real work to the stub — outcome-3 standing,
  recovery proven).

**RUNG 16 RESULT — OUTCOME (b) WITH A TWIST: the +0x2e byte was
NEVER the accelerated flag (packing error found, corrected, still
no change; 11:59–12:04):**
- **A rung-11a packing error was found:** `r[11] = 0x01000010`
  placed the `1` at byte +0x2f (MSB position of the dword), not
  at +0x2e where the float's disasm shows `movb $0x1`. The
  accelerated byte was NEVER SET — always 0 in our record.
  Corrected to `r[11] = 0x00010010` (byte +0x2e = 1, matching
  the float's record exactly).
- **BUT the census STILL reports accelerated=0** — and the float
  (software renderer) ALSO sets +0x2e = 1. **+0x2e is not the
  accelerated flag; it's something else the float uses.** The
  pre-registered hypothesis was right that the accelerated value
  is the question, but wrong about which field encodes it.
- The "invalid display" CGS errors persist identically; all pf
  results unchanged; the GLD calls unchanged. Desktop normal.
- **The accelerated value comes from ELSEWHERE** — candidates:
  the dword at +0x14 (0x8008000, capability word — a bit here
  may encode "hardware"), +0x18 (0x20000000, another capability
  word), the renderer ID's class bits, or from outside the
  record entirely.
- **Next move (the method, not more candidates):** the
  $0x2716-site read — find where 10006/kCGLBadDisplay is stored
  on the display-matching path in the three binaries; that code
  names the field it checks. /tmp/ogl.t (26582 lines) and
  /tmp/gfx.t and /tmp/gle.t are all on disk.

**THE $0x2716 SITE READ — THE MECHANISM DECODED (no boot; pure
disasm in /tmp/ogl.t):**
- All 0x2716 stores are in the **OpenGL framework** binary
  (none in libGFXShared or GLEngine). Four sites in the 0x9660
  display-matching helper.
- **The decisive site (0x9dcd):**
```
if (flag_0x84 & 0x4) {
    r12d = 0;                           // SKIP the mask computation
} else {
    r12d = popcount(display_mask);       // 0x7202 = bit-counter
}
if (flag_0x84 & 0x2) {                  // "accelerated" attribute
    if (r12d == 1) continue;             // exactly ONE matching display → OK
    else → glcRecordError(0x2716)       // ZERO or MULTIPLE → kCGLBadDisplay
}
```
- **The error fires when the accelerated attribute is requested
  AND the display-mask popcount ≠ 1.** With our single display
  (mask 0x1, popcount=1), the check SHOULD pass — UNLESS the
  display-mask function returns 0 (popcount=0) or a multi-bit
  mask.
- **The display-mask function (0x7757)** takes the requested
  mask (from attribute parsing) and a path selector (bit 5 of
  the flags). It routes through a **global gate** — a struct
  pointer at 0x10e90 with bit `0x40` at offset `0x20`:
  - Gate SET → FULL path: `CGSGetDisplayList` (all displays) +
    OR of all `CGSGetDisplayOpenGLDisplayMask` results.
  - Gate CLEAR → RESTRICTED path: `CGSGetOnlineDisplayList` +
    per-display mask + a BIT-REMAPPING loop, which **can return
    0** if `requested_mask & global_mask == 0`.
- **The REMAINING QUESTION (the next read):** what sets the
  global at 0x10e90 and its 0x20/0x40 bit — almost certainly
  the first-registered GLD's shared state or capabilities. If
  OUR GLD's registration doesn't set this bit, the restricted
  path runs and can zero the mask for accelerated requests.

**THE GLOBAL AT 0x10e90 — READ COMPLETE: a PREFERENCE-POPULATED
struct (not GLD registration; no boot needed):**
- The ONLY store to 0x10e90 is `movq %rax, ...` at 0x6447,
  immediately after `malloc(0x24)` — the struct is created and
  populated by a function that calls
  **CFPreferencesCopyMultiple** on the domain
  `com.apple.opengl`, then reads individual boolean keys via
  CFPreferencesGetAppBooleanValue, populating bytes at +0x20
  and +0x21 bit by bit (read-modify-write pattern).
- **The preference keys (extracted from __cstring):**
  `RendererIDEnableKey`, `RendererIDKey`, `AllowOfflineKey`,
  `ReverseAccelRenderersKey`, `EnforceMuxAware`.
- **On this guest, NO com.apple.opengl preferences exist** —
  the domain is empty, the struct stays at its zeroed state,
  bit 0x40 at +0x20 is CLEAR, and the RESTRICTED display-mask
  path runs. The restricted path calls
  `_cglBadApplicationNotMuxAwareLockDown` (the mux-awareness
  check) and uses `CGSGetOnlineDisplayList` (not the full
  display list).
- **The requested mask defaults to 0xFFFFFFFF** (all displays;
  `movl $0xffffffff, -0x228(%rbp)` at 0x9750) — the AND with
  the cached display mask cannot zero unless the cached mask
  itself is 0.
- **THE PRACTICAL TEST (one `defaults write` command):** set
  `com.apple.opengl` preferences and see if the gate bit
  changes, unblocking the full display-list path. The most
  likely candidate: `EnforceMuxAware` (the restricted path's
  own lock-down check) or `AllowOfflineKey` (might widen the
  display set). Revert by deleting the preference.
- **The bit-1-of-flag question remains open** (which attribute
  sets the flag at -0x84 that gates the popcount check) —
  but the preference route may bypass the question entirely
  by changing which display-mask path runs.

**THE ACTUAL ERROR SITE — 0x96dd, NOT 0x9dcd (read complete;
the preference question was answering the wrong site):**
- **The `orb $0x40` polarity is INVERTED from expectation:**
  `__CSCheckFix` returns 0 (no fix) → cmovel fires → bit 0x40
  SET → FULL path runs. No preferences → check returns 0 →
  gate IS open. The display-mask path is NOT the problem.
- **The actual error site is 0x96dd — the FIRST check in the
  helper, before any display matching:**
```
callq _glcGetIOAccelService     ; find the IOAccelerator service
testb %al, %al                  ; did it succeed?
jne 0x9708                      ; success → continue to display matching
→ 0x96dd: glcRecordError(0x2716) + exit   ; FAILURE = kCGLBadDisplay
```
- **_glcGetIOAccelService** (0x4fb4) returns 0 when the cached
  accelerator count is 0. The count is populated by the
  discovery function at 0x4c40, which for each display calls:
```
CGSServiceForDisplayNumber(display)     → get the display's IOService
IOAccelFindAccelerator(port, service, &accel) → find the accelerator
```
  If `IOAccelFindAccelerator` returns nonzero (failure) for the
  display, the accelerator count stays 0 and every subsequent
  call to glcGetIOAccelService returns 0 → the 0x2716 error.
- **IOAccelFindAccelerator is the SAME function the GA CFPlugIn
  used successfully in milestone 1** — but from the probe
  process. The question is whether it succeeds from the
  OpenGL framework's CGL initialization path. The accelerator
  IS published (VMQemuVGAAccelerator with the discovery trio);
  the display IS associated (CGSServiceForDisplayNumber
  returns our display's service). The failure point is inside
  IOAccelFindAccelerator itself — the matching between the
  display's service and our accelerator.
- **NEXT READ:** IOAccelFindAccelerator's matching criteria in
  the IOKit framework — what property on the display service
  or the accelerator does it check? The answer is a kext-side
  property or association, not a preference.

**IOAccelFindAccelerator READ COMPLETE — the matching criterion
is an `IOAccelerator` PATH STRING on the display service (IOKit
disasm, 107 lines; both checks run):**
```
IOAccelFindAccelerator(masterPort, displayService, &accel, &id):
    props = IORegistryEntryCreateCFProperties(displayService)
    path  = CFDictionaryGetValue(props, "IOAccelerator")  ← PATH STRING
    if (!path) return 0xe00002bc                           ← kIOReturnNotFound
    accel = IORegistryEntryFromPath(masterPort, path)
    if (!IOObjectConformsTo(accel, "IOAccelerator")) return 0xe00002bc
    id = props["IOAccelIndex"]
    return success
```
- **CHECK 1 — the key name:** `IOAccelerator`, NOT `IOAccelTypes`
  (both strings in IOKit's __cstring; the function reads
  "IOAccelerator" at the CFDictionaryGetValue call). **The header
  misled for the FOURTH time** — the GA trio published
  `IOAccelTypes` (correct by the header, wrong by the caller).
  The property `IOAccelFindAccelerator` reads is a PATH STRING
  whose value is the IORegistry path to the accelerator service.
- **CHECK 2 — the conformance:** `VMQemuVGAAccelerator` inherits
  from `IOAccelerator` (VMQemuVGAAccelerator.h:75) — the
  `IOObjectConformsTo` check PASSES. Not the problem.
- **CHECK 3 — the property on the display node: ABSENT.**
  `ioreg` shows NO `IOAccelerator` path string on the
  framebuffer or any display-side node — only `IOAccelTypes`,
  `IOAccelIndex`, `IOGLBundleName`, and the booleans. **The
  property IOAccelFindAccelerator reads does not exist.**
- **CHECK 4 — the caller difference (your point about CGS
  passing a different node):** the CGL path calls
  `CGSServiceForDisplayNumber` then passes the result to
  `IOAccelFindAccelerator`. The milestone-1 probe called it by
  passing the framebuffer directly. **Both would read the same
  property from the same node — the property is absent for
  both.** The probe SUCCEEDED in milestone 1 because it called
  `IOAccelFindAccelerator` directly with the framebuffer — and
  the function read the FB's properties, found no
  `IOAccelerator` path, returned the error — and the probe
  treated the error as a negative control, not a success.
  (The milestone-1 success was in `IOCreatePlugInInterfaceForService`
  — a different function that searches by class, not by path.)
- **THE FIX:** publish `IOAccelerator = "<path-to-accelerator>"`
  on the framebuffer — a string whose value is the IORegistry
  path to VMQemuVGAAccelerator. The path format is what
  `IORegistryEntryFromPath` accepts:
  `IOService:/AppleACPIPlatformExpert/...` (findable from
  ioreg's output). This is a one-property kext change, gated
  alongside the rest.

**MILESTONE-1 DIVERGENCE — the reconciliation question (recorded
before the property lands):**
- If `IOAccelFindAccelerator` was never successfully called by
  anything here (check 4), the probe's milestone-1 success went
  through `IOCreatePlugInInterfaceForService` — a CLASS-based
  search, not a PATH-based one. Two functions, two lookup
  strategies; the trio's `IOAccelTypes` satisfied the
  class-based search while `IOAccelerator` (the path-based one)
  was never present. **No divergence to reconcile — the two
  callers used different functions.** The property lands clean.
- **HEADER-AS-HYPOTHESIS — promoted to rule, fourth failure
  banked:** on this contract, the header is a hypothesis and the
  disassembly is the source of truth. Four failures, all the
  same kind: Initialize (6 args not 5), ChoosePixelFormat (2
  meaningful, 3rd is noise), name count (92 vs 78), and now
  IOAccelTypes vs IOAccelerator (the header describes an
  interface the callers don't implement).

**RUNG 17 PRE-REGISTERED — the one-property fix, with the FULL
prediction ladder (committed before implementation):**
- **The change:** publish `IOAccelerator` on the framebuffer
  under the vm-cap3d gate, alongside the existing IOAccelTypes.
  The value is the accelerator's IOService-plane path — the
  same string the trio already computes via getPath with
  gIOServicePlane for IOAccelTypes. **Logged at publication**
  (the boot log shows the exact string written — a path that's
  subtly wrong fails identically to one that's absent).
- **The prediction ladder — SIX observables, each independently
  checkable; any rung failing is informative, not a null:**
```
1. IOAccelerator appears in ioreg on the FB (with the logged path)
2. IOAccelFindAccelerator resolves the path → returns kIOReturnSuccess
3. The discovery function at 0x4c40 counts ≥1 accelerator
4. glcGetIOAccelService returns non-zero
5. The check at 0x96dd passes → the helper enters display matching
6. Accelerated pf sets stop returning 10006 and REACH THE GLD
```
- **Instrumentation (more than the endpoint):** the probe
  reports the ladder position, not just the final result:
  ioreg check for property presence, probe_cgs_requester for
  the pf behavior, stub log for whether accelerated sets
  reached the GLD. A partial advance reads as LOCATED, not
  failed — the difference between costing a boot and costing a
  session.
- **Revert:** gate off (arg removal); the property is one
  setProperty line, ungated boots byte-identical.
- **Outcomes:**
  (a) full ladder → the accelerated wall is down; the GLD's pf
  entry receives accelerated attribute sets for the first time
  → rung 18 is the honest answer (software caps refuse, or a
  hardware claim is made and backed);
  (b) ladder stops at 2-3 (path resolves but conformance or
  count fails) → the IORegistryEntryFromPath path format is
  wrong or the accelerator isn't visible at the expected
  location — check the logged string against ioreg;
  (c) ladder stops at 4-5 (count > 0 but glcGetIOAccelService
  still returns 0) → a caching issue or the CGL-side code
  doesn't re-discover after boot;
  (d) ladder stops at 5-6 (display matching runs but
  accelerated pf still 10006) → the accelerated filter is
  elsewhere in the chain (the popcount site, or below);
  (e) destabilized → proven revert.
- **STANDING RULE from this arc (header-as-hypothesis):** two of the
  trampoline header's claims have now failed silently — Initialize
  is 6-arg (not 5), ChoosePixelFormat is 3-arg (not 2) — and its
  name count was 92 vs the loader's 78. EVERY signature in that
  header is a hypothesis to confirm at the call site before it
  shapes an implementation.

**RUNG 17 RESULT (2026-08-22) — rung 1 PASS, rung 6 FAIL for
accelerated sets, first-ever plain pf consult reached the GLD, and
the stub's own parser found refusing EVERYTHING (dead-gate bug):**

- **DEPLOY CORRECTION — guest `nvram boot-args` does not survive
  reboot.** OpenCore's `config.plist` (on the automounted
  `EFI-LEGACY` volume, `EFI/OC/config.plist`) supplies boot-args at
  every boot and overwrites guest NVRAM. A pre-reboot
  `nvram boot-args=... vm-cap3d=1` was verified written, then the
  first reboot came up `gate=0` (kernel.log 14:09:33 "vm-cap3d
  gate=0 -> publishing no") with guest NVRAM back to the old
  string. The gate boot required editing config.plist's NVRAM
  boot-args (full string preserved, `vm-cap3d=1` appended,
  `plutil -lint` OK). The doc's warning was right; this is the
  observed confirmation.
- **The unplanned gate=0 boot doubled as the ungated control:** the
  new binary (md5 `074af8e8b70af71fbf2b862bc4f96ea8`, verified on
  guest after copy) booted normally with no panic and no rung-17
  log line — the registered "ungated boots byte-identical"
  prediction held. Cache rebuilt explicitly (Extensions.mkext
  9,800,503 bytes, fresh mtime).
- **The GLD bundle was NOT on the guest** — boot-time /tmp cleanup
  removed the old `/tmp/rung16` staging, and `/S/L/E` never held a
  persistent copy. Deployed after the boot (root:wheel 755), so
  WindowServer did NOT load the stub this boot (bundle absent at
  its load time); only the per-process probe dlopen exercised the
  GLD. Consequence: this rung measures fresh-process consult
  behavior; the rung-8 "WindowServer loads the GLD at boot"
  behavior is untested on this build and the desktop watch
  transfers to the next boot (bundle now present in /S/L/E at boot
  time, gate on).
- **LADDER RUNG 1 — PASS.** Kernel log 14:12:17:
  `rung 17 — IOAccelerator="IOService:/AppleACPIPlatformExpert/
  PCI0/AppleACPIPCI/S10@2/VMVirtIOFramebuffer/VMQemuVGAAccelerator"
  published (gate=1)`; ioreg shows the identical string under
  `"IOAccelerator"` on the FB.
- **LADDER RUNG 6 (accelerated) — FAIL.** probe_cgs_requester
  (pid 253): `CGLQueryRendererInfo -> 0 nrend=1`,
  `renderer[0]: accelerated=0 rendererID=0x1af60100` (unchanged);
  `CGLChoosePixelFormat(accelerated) -> 10006 npix=0` — and the
  stub log contains NO gldChoosePixelFormat consult for it. The
  accelerated set never reached the GLD: the upstream CGS/GLEngine
  filter still stands. Outcome branch (d) — the filter is elsewhere
  in the chain — with branches (b)/(c) not excluded because rungs
  2–4 have no instrument yet.
- **NEW OBSERVABLE — plain pf sets now CONSULT the GLD.** The
  plain call `{kCGLPFADoubleBuffer=5, 0}` returned `0 npix=0` and
  the stub logged
  `CALL gldChoosePixelFormat attrs=[0x5 0x4] (out zeroed)`. First
  pf consult to reach the stub in the whole arc. Meaning of attr
  4 unknown (raw value recorded; the stub's own table treats 4 as
  value-consuming; 5=kCGLPFADoubleBuffer per CGLTypes.h). A GLD
  refusal surfaces downstream as success-with-zero-formats, not an
  error.
- **THE DEAD-GATE BUG (correction of every prior npix=0 reading):**
  the stub's parser refused the consult with `0x2716 (no attr-0
  gate)`. In `./probe/gld_stub.c` the walk is
  `while (*p) { switch (*p) { case 0: gate = 1; ... } }` — the loop
  body can never be entered with code 0, so `case 0` is unreachable,
  `gate` is always 0 at `build:`, and EVERY attr list is refused
  0x2716. The parser has never been able to build an object. On
  every prior rung, npix=0 on consults that reached the GLD was
  this bug, not an upstream verdict. The float's gate structure —
  inferred from the disassembly — treats the terminator as the
  gate-setter; the transcription wrapped it in `while (*p)` and
  dead-coded it.
- **Boot stability:** gate=1 boot normal, 0 panic/error lines in
  kernel.log, refresh ~56 Hz sustained.
- **Instrument note:** the first probe run (pid 246) left no stub
  log because its output was piped through `head -30`; the pipe
  close SIGPIPE-killed the probe during early output, before its
  first CGL call. Filter-based viewing (`grep -v`), not
  truncation, for probes whose output is the artifact.
- **NEXT (rung 18, two independent fronts):**
  (a) **stub parser fix** — set gate on natural walk completion
  (terminator reached without a truncating default), the honest
  reading of the float's gate; prediction: plain pf consults
  return npix=1 with a real pf object (live-swap, no boot; the
  WindowServer-at-boot load on this NEXT boot is the stability
  watch, refusal convention `nonzero + *out=NULL` under load for
  the first time);
  (b) **rung-2 direct instrument** — call IOAccelFindAccelerator
  (exported by IOKit) with the FB service from a probe; with the
  property present it should return kIOReturnSuccess and a
  non-zero accel port. If it passes by direct call, the
  accelerated wall is the 0x9dcd popcount/display-mask site; if it
  fails, the property is still not being read as expected.

**DEAD-GATE AUDIT (2026-08-22, same session as the finding) — every
conclusion resting on a reached-but-unproductive consult
re-examined:**
- **RUNG 14's gate conclusion — DEAD (correction):** "The attr-0
  gate NEVER fires for CGL-driven calls — code 0 is the loader's
  internal marker... npix=0 everywhere is the honest answer for a
  software-class renderer that can't match the requested
  attributes." The gate never fired because `case 0` is
  unreachable inside `while (*p)` — a transcription defect
  presented as contract. What the float does at list end is
  UNKNOWN. npix=0 on reached consults was the dead gate refusing,
  not a software-class judgment: the answer was never given.
- **RUNG 12's OPEN puzzle — RESOLVED, in favor of "subtlety
  misread":** rung 12 held open that the float's "r8 gate needs
  attr code 0, which a zero-terminated list can never deliver —
  either the float builds via a subtlety misread, or device+0x14's
  content differed in its era." The misread existed — in the
  transcription into our stub. Rung 14 then wrongly resolved the
  open item into "the marker never fires." The float's own gate
  mechanism at its caller remains unread.
- **RUNG 15's "combination already tested" — WEAKENED, precise
  form:** "the booleans alone (with a registered GLD) did not
  unblock CGS's accelerated path" — the accelerated-set half
  stands (those sets never reached the GLD; re-confirmed rung 17,
  no consult for `{73,5}`). But "with a registered GLD" never
  meant "with a GLD capable of answering": the GLD could not
  answer anything, so no test of the GLD's ANSWER ever ran. No
  ledger conclusion leaned on the GLD's pf answer (checked);
  the phrasing overstated what was exercised.
- **SURVIVES:** the accelerated npix=0 localization ABOVE the GLD
  (rung 12 finding 3; rung 14; re-confirmed rung 17). SURVIVES:
  rung 14's safety result — no SIGBUS under real attrs, out-zero
  rule exercised, attrs `[0x4]`, `[0x35 0x4]`, `[0x5 0x4]` are
  real CGL arrays. SURVIVES: rung 14 (d) — downstream entries not
  called because no object was built (reason corrected: dead gate,
  not honest refusal). SURVIVES: shortcut cases 2/50/53 returning
  0 + NULL (separate return path, no gate involvement). Rung 16
  (record +0x2e byte) — unaffected (tested gldGetRendererInfo
  record bytes, not the parser).

**FIFTH INSTANCE of the tool-failure class — our own code
(2026-08-22):** the previous four were tools presenting plausible
results that were never there. The fifth is the same failure mode
from our own source: a parser that reads as if it handles the
terminator, structurally cannot (`while (*p)` exits before
`case 0` can run), and fails silently by refusing everything —
producing plausible-looking evidence (an "honest" 0x2716 with a
named reason in the log) that was never a judgment. THE RULE
EXTENDS: when OUR code's output drives a conclusion, its failure
mode is checkable the same way — for a parser, "is every path to
success reachable?" The dead gate had NO reachable success path;
that was checkable by reading the control flow before any boot
ever ran it.

**RUNG 18(b) PRE-REGISTERED — the direct IOAccelFindAccelerator
instrument (run BEFORE the parser fix (a); needs no bundle, no
boot; committed before the probe exists):**
- **Why (b) first despite (a) being cheaper:** if the path does
  not resolve, a working parser on a consult that never arrives is
  uninterpretable. (b) locates the wall; (a)'s meaning depends on
  it.
- **Symbols confirmed exported (nm -g on the guest):**
  `_IOAccelFindAccelerator` (IOKit, T), `_CGSServiceForDisplayNumber`
  (CoreGraphics, T).
- **The probe, three prints:** (1) CGSServiceForDisplayNumber(
  CGMainDisplayID()) → print the returned node's CLASS and its OWN
  `IOAccelerator` property — the function reads
  IORegistryEntryCreateCFProperties(displayService), the node's
  own properties ONLY; (2) IOAccelFindAccelerator(masterPort,
  that_node, &accel, &id) → print ret, accel, id; (3) the same
  call with the FB service found by class matching
  ("VMVirtIOFramebuffer") — the milestone-1 shape.
- **Signature is a hypothesis** from the disassembly read (4
  args; refusal 0xe00002bc kIOReturnNotFound is named); the
  return codes decide, not the header.
- **Predictions:**
  (i) CGS-faithful call returns 0 with accel ≠ 0 → rung 2
  CONFIRMED by direct instrument; the accelerated wall is
  downstream (0x9dcd popcount/display-mask site or below); then
  (a) the parser fix becomes meaningful as the next step.
  (ii) CGSServiceForDisplayNumber returns a display-side node
  (IODisplayConnect/AppleDisplay class) whose own properties lack
  `IOAccelerator` → CGS-faithful call 0xe00002bc while the FB
  call succeeds → **rung 1 was nominal** (the property verified on
  the wrong node; the rung-17 ioreg check verified the WRITE, not
  the READ site's node); fix = publish on the node CGS passes.
  (iii) BOTH calls return 0xe00002bc with the property
  ioreg-verified on the FB → the read differs from the
  disassembly interpretation (property TYPE — OSString vs
  expected; or plane/iteration semantics) → re-read the 107-line
  site with the type question open.
- **Instrument note:** no truncation pipes on probe output (the
  rung-17 SIGPIPE lesson); capture full stdout to a file and read
  it back whole.

**RUNG 18(b) RESULT (2026-08-22) — PREDICTION (i) CONFIRMED; RUNG 2
PASSES BY DIRECT INSTRUMENT; TWO SIGNATURE CORRECTIONS; THE WALL IS
DOWNSTREAM OF THE ACCELERATOR BEING FOUND:**
- First probe draft crashed (segfault 139, kCGErrorIllegalArgument,
  buffered stdout eaten) — the CGSServiceForDisplayNumber signature
  hypothesis was wrong. Corrected by reading the call site
  (OpenGL.framework x86_64 @0x4e0f) and the callee body: **SIXTH
  instance of the signature-hypothesis class, this time by not
  reading the call site before running.** Correct signatures:
  `int CGSServiceForDisplayNumber(CGDirectDisplayID, io_service_t
  *out)` (2-arg, status return) and
  `IOAccelFindAccelerator(display_service, u32 *out1, u32 *out2)`
  (3-arg; the earlier ledger reading named arg1 "masterPort" —
  wrong: the function makes its own master port at IOKit @0xef41;
  both outs zeroed on entry; "IOAccelerator" CFString →
  IORegistryEntryFromPath → conformsTo "IOAccelerator" →
  "IOAccelIndex" CFNumber → *out2; 0 on success,
  0xe00002bc absent/path-fail/conformance-fail). *out1 receives
  the FOUND ACCELERATOR PORT (observed 0x2803), not a return
  mirror — the post-FromPath `testl %eax; jne` is port validity.
- **Probe result (run-exit 0, full output):**
  `CGSServiceForDisplayNumber(0x22800040) -> ret=0 service=0x2603`,
  node class **VMVirtIOFramebuffer** — CGS passes the FB itself
  (prediction (ii)'s display-side-node scenario dead), the node's
  own `IOAccelerator` present (CFString, exact path),
  `IOAccelFindAccelerator(CGS node) -> ret=0x0 out1=0x2803
  out2=0` — **the path resolves; the accelerator is found,
  conforms, index read.** The FB-by-class call is the same call
  (same node 0x2603).
- **Ladder state after 18(b):** rung 1 PASS (rung 17); rung 2 PASS
  (direct instrument); rungs 3–5 implied by 2, not separately
  observed; rung 6 FAIL for `{73,5}` (10006, no GLD consult —
  rung 17). The wall is DOWNSTREAM of the accelerator being found.
- **THE 0x9dcd DISCRIMINATOR (pre-registered before the run):**
  the 0x9dcd condition is "accelerated attr requested AND
  popcount(display_mask)≠1". The probe sets `{73,5}` carry NO
  display mask — popcount(0)=0≠1 — while plain sets skip the site
  entirely (no accelerated attr). PREDICTION: a set WITH an
  explicit single-display mask — `{kCGLPFAAccelerated=73,
  kCGLPFADoubleBuffer=5, kCGLPFADisplayMask=84, 0x1, 0}`,
  popcount(0x1)=1 — passes 0x9dcd and a gldChoosePixelFormat
  consult appears in the stub log (and/or the 10006 changes).
  If it is still 10006 with no consult, the wall is below 0x9dcd
  or the mask in play is not the request's mask. Control:
  `{5, 84, 0x1, 0}` (mask without acceleration). Instrument:
  probe_r7 mode p extended with the two sets (a7/a8).

**THE DISCRIMINATOR RAN (same session) — THE ACCELERATED WALL NEVER
EXISTED. Every pf call consults the GLD; the caller's CGL error IS
the stub's return code; the 10006s were the dead-gate parser's own
0x2716 echoing back:**
- Mode-p multi-set run first: `accel+double+mask1 {73,5,84,1}` →
  still 10006 "invalid display" (explicit-mask prediction FAILED
  — 0x9dcd-as-popcount is wrong or incomplete); `ALL+accelerated
  {1,73}` → 0 npix=0. Four consults logged — with a content
  mapping now legible: each engine consult list = the caller's
  FORWARDED attrs + a TRAILER 4 + terminator. Forwarded: 5
  (DoubleBuffer), 53 (OffScreen), 84 (DisplayMask, 0x54, with its
  value). CONSUMED by the engine, never forwarded: 73
  (Accelerated), 1 (AllRenderers), 75 (Robust).
- **The per-process discriminator (mode q, one set per process,
  eight runs — engine caching defeats in-process attribution):**
```
{73}             consult [0x4]            stub 0x2716 -> caller 10006
{53}             consult [0x35 0x4]       stub 0 (shortcut) -> caller 0 npix=0
{73,5}           consult [0x5 0x4]        stub 0x2716 -> caller 10006
{75}             consult [0x4]            stub 0x2716 -> caller 10006
{1}              consult [0x4]            stub 0x2716 -> caller 10006
{1,73}           consult [0x4]            stub 0x2716 -> caller 10006
{73,5,84,1}      consult [0x5 0x54 1 4]   stub 0x2716 -> caller 10006
{5,84,1}         consult [0x5 0x54 1 4]   stub 0x2716 -> caller 10006
```
- **THE LAW: caller error == stub return (0x2716→10006,
  0→0).** Every set consults; accelerated sets were never filtered
  upstream; the observed 10006 was OUR refusal propagated by the
  engine. The numerical identity 0x2716==kCGLBadDisplay(10006)
  made the stub's own refusal read as CGS display-matching.
- **Engine memoization (observed; mechanism not read —
  hypothesis):** in the multi-set process, robust/ALL/ALL+accel
  returned 0 npix=0 after offscreen's shortcut-0 consult, while
  the same sets in fresh processes return 10006 — a prior
  0-return consult in the process softens later refusals to
  empty-success. This also explains rung-17's single-consult
  probe log ({73,5} then {5}: identical [0x5 0x4] — one consult,
  second call softened to 0/npix=0).
- **"invalid display" stderr control:** fires for {75} and {1}
  too (fresh processes, stderr visible) — it is CG's logging of
  error 10006, not an accelerated-specific check. The multi-set
  correlation with 73 was an artifact of the softened outcomes.
- **CORRECTIONS CASCADE (stated as corrections):**
  (1) Rung 14's "CGS filters accelerated above the GLD" — DEAD
  (already weakened by the dead-gate audit; now fully: there was
  no filter).
  (2) The dead-gate audit's "SURVIVES: the accelerated npix=0
  localization ABOVE the GLD" — DIES TOO; the audit was too
  conservative.
  (3) Rung 17's result line "the upstream CGS/GLEngine filter
  still stands" — DEAD.
  (4) The 0x96dd/0x9dcd display-matching sites exist in the
  disassembly but were NEVER the source of the observed 10006s;
  rung 15's mask check and the EDID elimination were solving a
  nonexistent problem. (The rung-17 property remains real and
  verified — rung 2 passes by direct instrument — but it was
  never the pf blocker: consults were already reaching the GLD.)
  (5) Rung 17's ladder rung 6 "FAIL for accelerated" — the reach
  half was WRONG: accelerated sets DO reach the GLD (as [0x4]/
  masked lists); the failure was the stub's answer.
- **RUNG 18(a) REVISED — the parser must BUILD.** Per the float's
  own default (goto build = truncate-AND-BUILD, not refuse) and
  today's list shape (caller attrs + trailer 4 + 0):
  (1) the 0-terminator ends the walk with the gate SET (walk
  complete); (2) unknown attrs TRUNCATE-AND-BUILD (the float's
  default), never refuse; (3) the built object stays
  software-honest (caps word unchanged, no accelerated claim).
  PREDICTIONS: `{5}` and `{5,84,1}` → npix=1 (real object). `{73}`
  → consult [0x4] still, object returned — either npix=1 (engine
  accepts any object; the acceleration question moves to context
  creation) or a NEW error ≠ 10006 (engine validates a flag —
  its identity names the next site). `{53}` keeps the shortcut
  (0 + NULL, npix=0 — no dereference: npix=0 means the slot is
  unused; the rung-12 crash was the shared-state path). Desktop
  watch unchanged: the next boot loads this stub at WindowServer
  start (bundle now in /S/L/E at boot, gate on) — first
  real-consumer exposure of the convention `nonzero + *out=NULL`
  under load.

**RUNG 18(a) RESULT (2026-08-22, same session) — the parser builds;
the crash was obj+0; all eight sets now error-0; npix=0 persists by
a THIRD mechanism (object accepted, slot not counted):**
- Implementation per the registration: dead gate removed (unknown
  attr → truncate-AND-build; terminator → walk complete, build);
  object software-honest.
- **FIRST 18a BUILD CRASHED — SIGBUS (exit 138) at
  gliChoosePixelFormat+117 = GLEngine+0x1444:**
```
orl $0x20000, 0x8(%rax)      ; rax = pf-object +0 target
```
  The engine WRITES: it ORs 0x20000 into +8 of whatever the
  object's +0 points at, walking the slot list (loop at
  0x1440–0x1460). **This is the rid-decoration site** — the census
  rid `id | 0x20000` (0x1AF40100 → 0x1AF60100), now located
  mechanistically. Our obj+0 = &_mh_bundle_header (read-only
  __TEXT) → KERN_PROTECTION_FAILURE at cr2 = stub_base+8
  (0x1000f6008). The otool symbol-displacement misread planted at
  a SECOND site (rung-9 falsified it for GetVersion's a2; the
  float's pf-object +0 was never its bundle header — the engine
  writes through it, so it CANNOT be).
- **Fix:** writable static `g_driver_obj[8]`, +8 = 0x1AF40100 (the
  engine's OR makes it 0x1AF60100 — matching the census rid);
  obj+0 = &g_driver_obj.
- **Result (all eight sets, fresh processes):** exit 0, CGL error
  0, npix=0, object built (walk complete or truncated), NO crash.
  Every 10006 is GONE — accelerated sets included. The stub log
  per set: `{73}`→[0x4] built; `{53}`→[0x35 0x4] shortcut (0 +
  NULL); `{73,5}`/`{5,84,1}`→[0x5 0x4]/[0x5 0x54 1 4] built after
  truncation; `{75}`/`{1}`/`{1,73}`→[0x4] built.
- **The registered pair of outcomes did not anticipate the actual
  third:** the object is ACCEPTED (decorated, no refusal
  propagated) yet npix=0 — the slot is filtered from the count
  AFTER the OR walk. The filter reads object fields; candidates:
  +0xc flags (our base 0x4C8 — the float's base value was never
  actually read), +0x14 (0x8000), +0x1c/+0x20 (1s), +0x34 claim
  (0x1). NEXT RUNG (register before reading): the post-walk slot
  filter at GLEngine ~0x1440–0x1550 — what field(s) decide
  counted-vs-dropped.
- Desktop note: the object-building stub is live in /S/L/E;
  WindowServer started before the swap (bundle absent at its
  boot), so the boot-time load of this stub remains untested —
  that exposure lands on the next reboot, and app-level CGL
  consumers exercise it from now on.

**THE COLLISION RULE (named, 2026-08-22) — a new kind in the
failure family:** 0x2716 is 10006 decimal — kCGLBadDisplay. The
stub's refusal code collided EXACTLY with the error being
diagnosed, and CG rendered it in the system's own words
("invalid display") — our refusal came back wearing the system's
voice. Combined with the dead gate (refusing everything), that
manufactured a wall out of nothing but the echo. THE RULE: choose
sentinel and refusal codes that CANNOT collide with the error
space under diagnosis — otherwise the instrument's output becomes
indistinguishable from the system's answer. The family now has
three kinds: (a) tools presenting plausible results that were
never there (uniq -c, ~/Info.plist, otool displacement — twice
now, rung-9 a2 and rung-18a obj+0, clang|grep -c); (b) our own
code presenting plausible results (the dead-gate parser); (c) our
instrument's value indistinguishable from the system's (this
collision).

**WHAT SURVIVES THE RETRACTION (explicit, so the ledger does not
over-retract):** four rungs (14–17) bought three real findings and
one phantom. REAL and still standing: (1) the IOAccelerator key
decode and the property fix — IOAccelFindAccelerator genuinely
requires it (verified by direct instrument, rung 2 passes); it was
never about the accelerated pf sets; (2) the preference-gate
polarity read; (3) the site-2 uninitialised-*out crash fix (the
out-zero rule); (4) the 87-case parser contract as the model for
an honest implementation. RETRACTED: only the inference from
truncation-refusals to an upstream wall — there was no wall; the
consults were reaching the GLD and being refused by our own bugs.

---

## RUNG 19 PRE-REGISTERED — the slot-filter read, the writability
contract, the third-site grep, and the boot-exposure decision
(committed before the read)

**THE WRITABILITY CONTRACT (contract fact, reach beyond this
entry):** the engine WRITES THROUGH objects it accepts —
`orl $0x20000, 0x8(%rax)` at GLEngine+0x1444 operates on OUR
memory; the engine does not copy the object. Therefore any object
handed to the engine must be (a) WRITABLE (never static const,
never a load-command page, never read-only __TEXT) and (b)
PERSISTENT across the call (never caller-scratch, never freed at
return — ownership transfers until the matching destroy entry).
This constrains every future creator entry (gldCreateShared,
gldCreateContext, texture/buffer/framebuffer creators): return
heap objects, keep them owned, destroy them in the matching
gldDestroy*.

**THE THIRD-SITE GREP (done before this registration):** exactly
one live use of `&_mh_bundle_header` remains — the
gldGetRendererInfo record's +0 anchor (gld_stub.c:166), set in
rung 11 and never touched since. The crash evidence exonerates it
for the census path: if the census's rid composition
(record+8 | 0x20000) wrote through record+0's target the way the
pf path writes through obj+0, every census since rung 11 would
have SIGBUS'd at stub_base+8. None did — the census does not
write through record+0. FLAGGED as a standing hazard with the fix
deferred: changing record+0 would alter the working census path
(one variable at a time), and no write-through has been observed
or read there. The dead second extern redeclaration at the pf
entry's include block is noted and harmless.

**THE SLOT-FILTER READ — predictions registered BEFORE reading
(GLEngine 0x1440–0x1550; the OR walk runs 0x1440–0x1460; the
filter follows; the instrument is the disassembly /tmp/gle.t):
Current state to explain: all eight sets → error 0, npix=0,
object built and decorated, no crash — accepted but not counted.**
- (i) **Request-vs-object matching:** the filter matches the
  FORWARDED request attrs (5=DoubleBuffer, 84=DisplayMask+value)
  against object fields — most likely the flags word +0xc (our
  base 0x4C8 was never read from the float) or +0x34 (claim/mask)
  vs the request's mask; mismatch drops the slot. Fix within this
  rung: set the named field. HONESTY BOUNDARY registered now: bits
  describing buffer modes/shapes are settable (software-capable
  properties); bits claiming HARDWARE acceleration are set only
  when functional 3D is real — the stub stays software-honest.
- (ii) **Deeper driver-object walk:** the filter dereferences
  g_driver_obj fields beyond +8 (e.g. a pointer at +0x10 to a
  table); our zeroed quadwords fail. Fix: populate the named
  offset.
- (iii) **A second entry gates the count:** slots are counted only
  after another entry (describe/CreateShared-class) that the stub
  refuses; the soft-fail reads as npix=0. Observable in the stub
  log: the entry's refusal line appears; fix = answer that entry.
- (iv) **npix is sourced from a mask-filtered list, not the slot
  list:** the walk's slots and the counted slots are different
  collections (the -0x38(%rbp) slot vs a display-bound list); a
  request with no explicit mask counts nothing. Fix is
  REQUEST-side (the probe adds an explicit mask attr), not an
  object field.
- Each outcome is distinguishable by what the read names; the
  implementation follows the named mechanism, verified by rerunning
  the eight-set probe (prediction: the named sets' npix becomes
  ≥1; sets outside the fixed scope keep npix=0).

**THE BOOT-EXPOSURE DECISION (deliberate):** the object-building
stub is live in /S/L/E with the gate ON — every reboot from now
is a WindowServer experiment with a stub that now SUCCEEDS rather
than refusing. THE NEXT REBOOT IS RUNG 19's SECOND HALF, a
watched boot. Instruments: the desktop watch; the kernel log
(WindowServer GLD-load lines); the stub log (boot-time entry
sequence — which entries the real consumer calls, and the refusal
convention under load for the first time). Pre-registered
outcomes:
- (i) desktop normal, WindowServer decorates and keeps objects →
  the convention survives real load;
- (ii) desktop fails or boot hangs → REVERT: gate OFF (remove
  vm-cap3d from config.plist — without IOGLBundleName,
  WindowServer never loads the GLD), or bundle removal from
  slclean if unbootable;
- (iii) desktop normal with stub-log refusals → maps the
  boot-time entry sequence (which entries the real consumer
  calls first).
- Until that deliberate boot: no non-watched shutdowns; if the
  session must end without the watched boot, REMOVE THE BUNDLE
  first (rm -rf /S/L/E/VMVirtIOGLEngine.bundle) so no unwatched
  boot inherits the exposure.

**RUNG 19 RESULT (2026-08-22, same session) — THE CHAIN CONTRACT
(read at GLEngine 0x13cf–0x149b): pf objects are a LINKED LIST;
two real bugs fixed (the crash's true mechanism, and chain
poisoning); npix=0 persists; the counter is RELOCATED to the
OpenGL.framework worker at 0x9660:**
- **The read (predictions (i)–(iv) registered above; the truth was
  a fifth shape):** gliChoosePixelFormat iterates PLUGINS
  (gfxGetPlugins list), calls the pf entry per plugin via
  [plugin+0x130] (slot 38), APPENDS each returned object at the
  current tail via `[tail+0] = obj`, then walks the chain
  decorating EACH node's OWN +8 with 0x20000 (`orl $0x20000,
  0x8(%rax)` at 0x1444 — the id field ON the object), following
  +0 until NULL. **obj+0 IS THE CHAIN LINK; obj+8 IS the renderer
  id on the object.** Multi-slot returns are a linked list built
  through the objects themselves. The engine writes through the
  objects it accepts (contract confirmed at the append, the
  decoration, and gliDestroyPixelFormat's per-node plugin
  destroy call at [plugin+0x138] = slot 39, expecting 0).
- **Correction of the rung-18a reading:** 0x8(%rax) is the
  OBJECT's own +8 — not "a driver object pointed to by obj+0".
  The rung-18a g_driver_obj was a misread of rax's provenance:
  it did not point at a driver; it APPENDED a fake object into
  the chain. Both prior failures re-explained by one mechanism:
  +0=&_mh_bundle_header made the walk decorate the read-only
  header as a "next object" (SIGBUS at stub_base+8); +0=
  &g_driver_obj made the walk treat the fake driver as a second
  pixel format (npix=0 by validation failure on a bogus node).
- **Implemented:** obj+0 = NULL (single-slot chain terminator;
  g_driver_obj deleted); gldDestroyPixelFormat now frees the
  object and returns 0 (gliDestroyPixelFormat at 0x149c walks
  the same chain and calls the destroy entry per node, expecting
  0 — the refusal leaked every object and failed teardown).
- **Verified (eight sets, fresh processes):** all exit 0, error
  0, objects built with +0=NULL, destroy not faulted — **npix
  STILL 0 for every set.** The registered prediction (named
  sets' npix ≥ 1) FAILED: the chain contract was necessary (two
  real bugs) but not sufficient. The counting/validation is
  ABOVE GLEngine.
- **RELOCATED (from the CGLChoosePixelFormat body, OpenGL.framework
  x86_64 0x14c5–0x15ac):** the worker is the helper at **0x9660**
  — called with ecx=1 (first attempt), and RETRIED with ecx=0
  when the first returns 0 but *npix==0 AND a fallback flag
  (byte [global+0x21] & 0x8) is set — the accelerated-first/
  software-retry structure. The crash stack's
  "glcGetIOAccelService+821" was nearest-symbol attribution for
  this same region. **The npix counter and the object validator
  are in 0x9660's post-consult code — that is rung 20's read.**
- Rung 19's SECOND HALF (the watched boot) remains PENDING: the
  stub in /S/L/E is now chain-correct; no reboot has occurred
  since it landed; the exposure rules above still govern.

---

## RUNG 20 PRE-REGISTERED — the 0x9660 worker's npix counter
(committed before the read)

**The question, precisely:** gliChoosePixelFormat returns 0 with
our single-object chain (+0=NULL, +8 decorated to 0x1AF60100);
the worker at OpenGL.framework 0x9660 returns 0; *npix stays 0.
Where between the chain head and the count does the object die?

**What is already known about 0x9660 (from rung 19's read of its
caller):** called as `f(attrs_transformed, attrs, npix_out,
ecx)` — ecx=1 on the first attempt, retried with ecx=0 when the
first returns 0 with *npix==0 under the fallback flag
(byte[global+0x21]&0x8). The 0x2716 sites at 0x96dd and 0x9dcd
are inside its entry section (display-matching / accel-service
checks that run BEFORE the consult — real code, but never the
source of the observed 10006s). The gliChoosePixelFormat call is
made from inside it (the crash stack's frame order).

**Predictions (registered before reading):**
- (i) **Per-object field validation:** the worker walks the chain
  and checks each object's field(s) against the request —
  candidates +0xc flags (base 0x4C8, never read from the float),
  +0x14 (0x8000), +0x1c/+0x20 (1s), +0x34 (claim 0x1) vs the
  request/CGS mask. Mismatch drops the node. Discriminator: the
  read names the offset; fix = set the field (honesty boundary
  stands: hardware-acceleration bits only with functional 3D).
- (ii) **Mask intersection:** the worker ANDs the object's mask
  field with its own display mask; 0 → dropped. Known wrinkle:
  claim 0x1 vs display mask 0x1 should PASS (rung 15) — if this
  is the mechanism, the mask in play is derived differently
  (plugin-accumulated, or per-virtual-screen).
- (iii) **Id cross-check:** the worker counts only objects whose
  decorated +8 id matches an id in its own renderer list
  (census-side data). Our 0x1AF60100 equals the census rid —
  would pass unless the worker composes its expected id by a
  different rule.
- (iv) **Pre-consult drop:** the object dies BEFORE the chain is
  examined — the display-association (CGSServiceForDisplayNumber
  / IOAccel path at the entry section) fails to bind the request
  to our display, and BOTH the ecx=1 and ecx=0 passes drop at the
  same entry check; the consult result is never counted on
  either pass. If so, the fix is in the association inputs, not
  the object.
- Secondary question the same read settles: what DO the 0x96dd /
  0x9dcd checks gate, now that they are known not to have
  produced the observed errors?

**Instrument:** /tmp/ogl.t (OpenGL.framework x86_64
disassembly). Read 0x9660's entry checks, the
gliChoosePixelFormat call site, and the post-call counting.
**Verification:** whatever mechanism is named, the fix lands in
this rung if it is object-side, and the eight-set probe reruns —
prediction: the named sets reach npix ≥ 1; a mechanism on the
association side (iv) relocates instead and the rung records it.

**RUNG 20 READ PART 1 (2026-08-22) — the full consult architecture
mapped; npix IS the transformer's chain count; the remaining
unknown is the attr-case switch; a 2-node discriminator
registered:**
- **The call architecture (crash-stack arithmetic: frame
  "glcGetIOAccelService+821" = 0x4fb4+0x335 = 0x52e9):**
```
CGLChoosePixelFormat (0x14c5)
  └─ worker 0x9660 (ecx=1; retry ecx=0 under fallback flag
       byte[global+0x21]&0x8 when first returns 0 with *npix==0)
       └─ glcGetIOAccelService (0x4fb4) — ENGINE-side plugin
            dispatcher: locks a mutex; 0x4c40 builds the plugin
            list; walks plugins BY INDEX; plugin struct: +8 path
            string, +0x408 cache state (-1 check), +0x418,
            +0x420 dlopen handle, +0x430 consult entry, +8
            destroy slot; a LAZY path (0x531a) dlopens a module
            per-call, dlsyms TWO entries by name, invokes,
            DLCLOSES (load-invoke-unload — the software/float
            class); the CACHED path (0x52e4) calls the resolved
            consult entry with (&local_out, attrs)
            ├─ consult (our gldChoosePixelFormat via GLEngine)
            ├─ 0x37ff(plugin, attrs, raw_chain) — the TRANSFORMER
            └─ destroy the raw chain (0x52fe, immediately after
               the transform) — the ownership contract confirmed:
               consult → transform → destroy, every call
```
- **The transformer 0x37ff:** walks the CALLER's raw attr array
  through a ~87-case jump-table switch (`subl $0x4; cmpl $0x56`)
  seeded with **esi = [plugin+0x40c]** — a capability word on the
  ENGINE-side plugin struct (provenance not yet read); on
  terminator it COUNTS THE CHAIN NODES (`0x39d7`: walk +0 links,
  r14++) and mallocs `(14·nodes + nattrs)·4` — 14 dwords per node
  = the CGLPixelFormat's per-screen record. **npix is this
  count.** Our chain (1 node) should count 1 — so either an
  attr-case bails to the default (0x3b26) BEFORE the count, or
  the drop is post-count. The jump-table cases (0x57 entries at
  ~0x3874) and the +0x40c provenance are the named remaining
  reads.
- **The dispatcher's destroy-after-transform (0x52fe) validates
  the rung-19 destroy fix** — the engine frees the raw chain
  through OUR entry on every consult; the old refusal leaked
  per-call.
- **THE 2-NODE DISCRIMINATOR (registered before running):**
  extend the stub's gldChoosePixelFormat to return a TWO-node
  chain (obj1+0 = obj2, obj2+0 = NULL). PREDICTION: if npix=2,
  the chain count works and the drop is POST-COUNT (the
  transformer's output handling or the worker's npix store);
  if npix=0, the transformer bails BEFORE the chain walk (an
  attr-case or the +0x40c capability check) — and the +0x40c
  provenance read becomes the named next step. Instrument: the
  same eight-set probe, live-swap, no boot.

**RUNG 20 READ PART 2 + DISCRIMINATOR RESULT (2026-08-22) — the
transformer's happy path CANNOT return NULL; +0x40c is a
round-robin index, not a capability word; the ID-PLANE RULE found
(`plugin+0x110` exact vs `id & 0xffff00`) — the first mechanism
that fits every observation:**
- **2-node discriminator: npix=0** (fresh process, consult
  logged, 2-NODE CHAIN built) — AND `gldDestroyPixelFormat`
  never logged. Two observables, one contradiction with the
  assumed flow.
- **The transformer 0x37ff fully read:** +0x40c is a ROUND-ROBIN
  counter (increment, modulo a global; rotates across 16-byte
  slots of the table at plugin+0x410). The happy path: attr walk
  (switch, codes 4..0x5a; four bail jumps — 0x3840 out-of-range
  attr, 0x39c7 >46 attrs, 0x39d1 count overflow, 0x3a48
  malloc-fail; attr code 1 = AllRenderers is BELOW the switch
  floor — the {1} sets bail legitimately) → chain node count →
  malloc (14 dwords/node + attrs) → copy caller attrs → copy
  0x38 bytes per chain node (OUR exact object size) → store the
  output at [table+idx+8] → return it. **The happy path cannot
  return NULL.** So npix=0 for {5}-class sets is NOT a
  transformer bail.
- **The dispatcher tail read:** on success `*out = r13` (the
  transformer's return) and the consult's error is the function's
  return; a cached-answer path (plugin+0x408 != -1 → 0x33c4)
  can short-circuit without consulting. The lazy path's two
  dlsym names confirmed by strings: `gliChoosePixelFormat` /
  `gliDestroyPixelFormat` — the engine loads GLEngine by name.
- **THE ID-PLANE RULE (libGFXShared _gfxGetPluginWithDriverID,
  0x179d):** walks `_gfx_plugin_head`, comparing
  **`plugin+0x110` EXACTLY against `id & 0xffff00`**. No
  tolerance. And `_gfxCreateSharedState` (0x17f9) resolves EVERY
  driver id through this lookup, storing plugin-or-NULL at
  shared+0x170 per slot.
- **The mechanism that fits everything:** our pf objects claim id
  0x1AF40100 (plane 0x1AF40000; decorated 0x1AF60100 → plane
  0x1AF60000). Per rung 9's decode the loader registered the
  device under the version-composed id `0x1020000 | 0x400 =
  0x1020400` (plane 0x1020000). DIFFERENT PLANES →
  gliDestroyPixelFormat's per-node plugin resolution misses →
  destroy skipped (the observed absence) → and any worker-side
  per-node id resolution misses identically → the node is never
  counted → npix=0. The census worked because it reads the
  RECORD directly (record+8 | 0x20000), not the plugin table.
- **RUNG 21 PRE-REGISTERED — measure the registered id, don't
  guess it (committed before implementation):** the stub runs in
  the same process as libGFXShared; `_gfx_plugin_head` is an
  exported global. The stub's gldChoosePixelFormat will dlopen
  libGFXShared by path (same handle), dlsym `_gfx_plugin_head`,
  walk the plugin list, and log each plugin's +0x110 (and +0x118
  mask) ONCE per process. PREDICTION: the log shows ONE plugin
  (ours) with +0x110 in the 0x102xxxx (version-composed) plane,
  NOT 0x1AF4/0x1AF6xxxx. THE FIX FOLLOWS FROM THE MEASURED
  VALUE: the pf object's +8 id moves to the measured plane
  (preserving our low vendor bits); prediction then:
  destroy-logs + npix ≥ 1 on the {5}-class sets. If +0x110 is
  0x1AF4-plane after all, the id-mismatch hypothesis dies and
  the worker's npix store becomes the named next read.
- The 2-node diagnostic shape reverts to single-node (the
  discriminator served its purpose; one honest object).

**RUNG 21 RESULT (2026-08-22) — the registered id MEASURED:
plugin+0x110 = 0x20400, mask 0x1, ONE plugin. The id mismatch is
FIXED (0x20500, resolving) and EXONERATED in the same run — npix
still 0. The worker's count machinery fully located: TWO gates —
a mask-AND on the node's +0x34 and a zero-score-loses ranking:**
- **Instrument:** `_gfx_plugin_head` is NOT exported (nm -g
  positive control; the first dlsym attempt failed) —
  `_gfxGetPlugins` IS exported (T) and returns the head; the
  engine's own gliChoosePixelFormat uses it exactly that way.
  The stub calls it once per process (run from
  gldChoosePixelFormat; first dlopen attempt log: UNRESOLVED →
  corrected).
- **The measurement (fresh process, pid 1018):**
  `rung21: plugin[0] 0x10030cd90 +0x110(id)=0x20400
  +0x118(mask)=0x1` — ONE plugin. The registered id is
  **0x20400** — NOT the rung-9 decode's 0x1020400 (off by the
  0x1000000 bit) and NOT our 0x1AF4-plane. The accumulated
  display mask is 0x1 (matches the Initialize mask).
- **The id fix (in the same build, derived at runtime):** the pf
  object's +8 = measured_plane | 0x0100 = **0x20500**; the
  engine's 0x20000 decoration is idempotent in-plane; the
  lookup (id & 0xffff00 = 0x20400 == plugin+0x110) RESOLVES.
  **RESULT: npix STILL 0** (set 1, exit 0, consult logged,
  object built with id=0x20500). The id mismatch was real and
  is now fixed — and was NOT the npix blocker.
- **Residual (secondary, unexplained):** gldDestroyPixelFormat
  still never logs. With the id resolving, gliDestroyPixelFormat's
  per-node plugin lookup would succeed — so the dispatcher's
  destroy step itself did not execute on our path. Deferred; not
  load-bearing for npix.
- **The worker's count machinery (read 0x9660–0xa6d4):** args
  (attrs, &pf_out, &npix, flag); `*npix = 0` at entry (0x96a5);
  per online display the CGS mask accumulates (0x9ee1–0x9ef7);
  then a per-driver, per-node loop with TWO gates:
  (1) `r14d & node+0x34` — the request's mask ANDed against the
  node's CLAIM field — zero overlap skips the node (0x9f3f,
  0xa0fe);
  (2) a score via 0xba0a per node, `if (max >= score) skip`
  (0x9f75, 0xa131) with max initialized 0 — **a zero-scoring
  node never wins even unopposed**;
  the per-driver winner increments the count local (-0x250 at
  0xa1bb); exit stores `*pf_out = winner-array`, `*npix = count`
  (0xa6a4–0xa6bb).
- **RUNG 22 PRE-REGISTERED — the two-gate discriminator (committed
  before running):** set the pf object's +0x34 (claim) to
  0xFFFFFFFF (claim every display) — ONE variable, live-swap.
  PREDICTION: if gate (1) is the drop, npix ≥ 1 on the {5}-class
  sets; if npix stays 0, gate (1) passes and gate (2) (the
  0xba0a score) is the drop — its inputs ([-0x280]/[-0x278]
  context + node fields read by 0xba0a) become the named next
  read. The 0xFFFFFFFF claim is a DIAGNOSTIC value only — an
  honest multi-display claim is a separate decision after the
  gate is identified.

**RUNG 22 RESULT (2026-08-22) — GATE 1 EXONERATED (npix=0 with
claim=0xFFFFFFFF on all eight sets); GATE 2 CONFIRMED by the
registered branch; the scorer read — its rejects are the node's
+0xc FLAGS (subset test) and +0x10 (exact match), both
transcribed with guessed values in rung 12:**
- All eight sets, fresh processes: exit 0, error 0, npix=0,
  objects built (id=0x20500 from the measured plane). The
  mask-AND gate (+0x34) passes with every display claimed — it
  was never the drop.
- **The scorer 0xba0a read — it is a request-vs-node MATCHER:**
```
0xba0a(request(rdi), attrs-desc(rsi), node(rdx), popcount(ecx), flags(r8d))
0xba96: [request+8] vs [node+8]    id-mask fields: only bits the
       request NAMES must match (0xfe0000/0x7f00/0xff/0xff000000
       planes, each checked only if the request sets it)
0xbb01: [request+0xc] & ~[node+0xc] != 0  -> REJECT
       the request's REQUIRED capability flags must be a SUBSET
       of the node's +0xc flags word
0xbb13: [request+0x10] != [node+0x10]     -> REJECT
       an EXACT-match field; ours is 0 (calloc)
0xbb21: color/buffer-size matching (0x5567) when request flags
       name it
... (further reads follow; the first reject wins)
```
- **The two operative suspects, both from the rung-12
  transcription's guesses:** node+0xc = flags with base 0x4C8
  (NEVER read from the float — a guess), and node+0x10 = 0. A
  required flag missing from ours, or any nonzero exact field,
  rejects: score 0 → never wins the max-initialized-at-0
  comparison → never counted → npix=0.
- **RUNG 23 PRE-REGISTERED — the flags discriminator (committed
  before running):** the claim reverts to the honest 0x1 (gate 1
  is exonerated; single-variable discipline), and the node's
  +0xc goes to 0xFFFFFFFF (claim every capability — DIAGNOSTIC,
  the honesty question follows the gate identification).
  PREDICTION: npix ≥ 1 on the {5}-class sets → the flags-subset
  reject (0xbb01) is the operative one, and the honest fix is to
  learn the request's required-flag bits per attr and set the
  software-honest subset; npix still 0 → the exact-match +0x10
  reject (0xbb13) is next, and the request struct's construction
  (where the worker builds [-0x280] from the caller attrs)
  becomes the named read.

**RUNG 23 RESULT (2026-08-22) — PREDICTION CONFIRMED: THE FIRST
npiX=1 OF THE ARC. Five of eight sets count a real pixel format;
the flags-subset reject (0xbb01) was operative; the residual
failures share attr 5 (DoubleBuffer) — the +0x10 exact-match
class:**
```
{73} accelerated          -> 0 npix=1    {1}  ALL_RENDERERS   -> 0 npix=1
{53} offscreen            -> 0 npix=0    {1,73} ALL+accel     -> 0 npix=1
{73,5} accel+double       -> 0 npix=0    {75} robust          -> 0 npix=1
{73,5,84,1} ...+mask1     -> 0 npix=0    {5,84,1} double+mask -> 0 npix=0
```
- With node+0xc = 0xFFFFFFFF (every capability claimed) and the
  honest claim 0x1 restored: five sets reach npix=1 — the first
  non-zero pixel-format count since the arc began. The wall is
  the flags word, and it is DOWN for the simple sets.
- **The residual signature is clean:** every npix=0 set (except
  the {53} shortcut, expected and honest) contains attr 5
  (DoubleBuffer); every npix=1 set contains neither 5 nor 84.
  Attr 5 adds a requirement beyond the flags word — the +0x10
  exact-match reject (0xbb13), per the registration's second
  branch. Attr 84's effect is NOT separable from this run (no
  set has 84 without 5).
- **The {1} surprise:** ALL_RENDERERS returns npix=1 despite the
  transformer's attr-range bail read in rung 20 (code 1 below
  the switch floor) — that reading is wrong or incomplete;
  recorded as a small open item, not load-bearing.
- **The accelerated sets {73}/{1,73} returning npix=1 is the
  DIAGNOSTIC over-claim at work** (0xFFFFFFFF satisfies any
  required bit, including an acceleration-class bit). The honest
  subset will honestly refuse accelerated sets until Mesa backs
  them — that is the point of the honesty boundary.
- gldDestroyPixelFormat still never logs (secondary residual,
  unchanged).
- **RUNG 24 PRE-REGISTERED — the request-struct construction
  (committed before the read):** find where the worker builds
  the request struct ([-0x280]/[-0x278]) from the caller's
  attrs — the attr→required-flags (+0xc) and attr→exact (+0x10)
  mapping. PREDICTION: attr 5 (DoubleBuffer) sets [request+0x10]
  to a nonzero surface/backing value that our 0 fails; attrs
  73/75/1 map to required-flag BITS in [request+0xc]. THE FIX
  THAT FOLLOWS: the node's +0xc becomes the software-honest
  subset (every required bit the stub can genuinely honor —
  buffer/backing modes; NOT the acceleration bit), +0x10 the
  matching exact value from the read; expected outcome: the
  plain/double-buffered sets (the ones real apps request) reach
  npix=1 honestly, and the accelerated sets honestly refuse
  (npix=0) until the Mesa-backed claim exists.

**RUNG 24 RESULT (2026-08-22) — the attr→field mapping READ (the
prediction's mechanism confirmed); the +0x10 echo implemented;
the honest subset INSUFFICIENT — all eight sets npix=0 — the
scorer's later stages positively require node bits beyond the
request's required set:**
- **The request-struct constructor read (0xb55d, called at
  0x9d2d with the worker's -0x90/-0x50 locals):**
```
defaults: [req+8]=0 (id-mask), [req+0x10]=0 (exact-modes),
          [req+0x14]=0x3FFFFFFC, [req+0x34]=0xFFFFFFFF
[req+0xc] = 0x480 baseline (0x400 when the worker flags clear
            bit 0x10) — EVERY request requires 0x480/0x400
attr 73 (Accelerated) -> orl $0x100 into [req+0xc]  (0xb8e0)
attr 75 (Robust)      -> orl $0x40  into [req+0xc]  (0xb8eb)
attr 1 (AllRenderers) -> CLEARS 0x400 from [req+0xc] (0xb851 —
                         a relaxation; explains {1} passing)
attr 76 (BackingStore)-> orl $0x8  into [req+0xc]  (0xb8f3)
attr 49 -> 0x200; attr 50 -> 0x1; attr 90 -> 0x2000;
attr 97 -> 0x10000; attr 70 (RendererID-value) -> [req+8]
attr 5 (DoubleBuffer) -> orl $0x8 into [req+0x10]  (0xb7d0)
attr 6 (Stereo)       -> orl $0x2 into [req+0x10]  (0xb7db)
attr 84 (DisplayMask) -> the VALUE -> [req+0x34]    (0xb929)
size attrs (7,9,10...) -> words at [req+0x26/0x28/0x2a]
```
- **Implemented:** the node's +0x10 ECHOES the walked buffer-mode
  attrs (parser cases 5→|0x8, 6→|0x2 — the forwarded lists carry
  them; the old table never had cases 5/6); the node's +0xc =
  the rung-24 honest subset 0x4C8 (0x480 baseline | 0x40 robust |
  0x8 backing — every bit read-justified, NO 0x100 hardware bit).
- **RESULT: all eight sets npix=0** — including {75} and {1},
  whose subset math PASSES (required 0x4C0/0x80 ⊆ 0x4C8). This
  RESOLVES the rung-22 inconsistency ({75} failed there with the
  same 0x4C8 despite passing the subset math): **the scorer's
  LATER stages (0xba0a past 0xbb21, unread) positively test
  node+0xc bits beyond the request's required set — and/or
  node+0x14 (our 0x8000, another rung-12 guess) — zero-scoring
  nodes that lack them.** 0xFFFFFFFF satisfied those stages
  (rung 23); the honest word does not.
- **The echo mechanism itself is confirmed live:** attr 5/6 bits
  now reach node+0x10 (the parser walks them without
  truncation); the exact-match reject class is addressed — the
  remaining failure is the unread ranking/capability stages.
- **RUNG 25 PRE-REGISTERED — the scorer's remainder (committed
  before the read):** read 0xba0a from 0xbb62 to its return —
  the color/size matching (0x5567 vs node+0x14 and the
  attrs-desc), the four comparison tables selected by the
  worker-flags (r15 at 0xba61), and the SCORE computation —
  which node fields and bits produce a nonzero score.
  PREDICTION: the score sums matched capabilities from node+0xc
  (and/or node+0x14 sizes) against the request — the float's
  real values for +0xc/+0x14 are the honest target, obtainable
  from the float's OWN builder (GLRendererFloat's object
  construction site — the same 87-case parser's build block in
  /tmp/grf.t). THE FIX: read the float's built-object field
  values from its disassembly and mirror the software-honest
  ones, rather than guessing bit sets.

**RUNG 25 RESULT (2026-08-22) — the float's build block READ (the
complete object shape, ground truth); a THIRD transcription error
found and fixed (0x8000 vs 0x8000000); the rung-12 flags base
VALIDATED; and the honest negative: with the float's exact
software values in place, ALL EIGHT sets still npix=0 — the
differentiator is bits the scorer demands beyond the request:**
- **The float's builder (grf.t 0x178cc–0x17c7d):** walk locals
  initialized {word 0, word 0, word 0, dword 1, dword 1, dword 0,
  flags = 0x4C8}; the SAME 87-case switch (subl $4, cmpl $0x56);
  after the walk the stack object is assembled then malloc(0x38)
  copied:
```
obj+0x00 = 0                      (chain link — single node)
obj+0x08 = 0x1000400              (the float's OWN id — also
                                   corrects rung 9's composed-id
                                   decode; the float's plane)
obj+0x0c = 0x4C8 | walk-ORs       (flags)
obj+0x10 = walked modes (0)       (the +0x10 echo field)
obj+0x14 = 0x8000000              (CONSTANT — 0x17c5d)
obj+0x18 = 1                      (0x17c56)
obj+0x1c = 1, obj+0x20 = 1
obj+0x24..0x2a = 0; +0x2c/+0x30 = walk counters; +0x34 = claim
```
- **THIRD TRANSCRIPTION ERROR FOUND (after the dead gate and the
  obj+0 anchor): the rung-12 table read 0x8000 where the float
  writes 0x8000000** — off by 0x1000x — at +0x14, the field the
  scorer exact-tests. Fixed. Also +0x18's 1 was never set in our
  builds. Fixed.
- **The rung-12 flags base 0x4C8 VALIDATED by ground truth** —
  the float's own initializer writes 0x4C8. A guess that was
  right, now confirmed rather than lucky.
- **The scorer's structure (0xbb5f–0xbca0):** 0x5567/0x55b2/
  0x55e7 extract component values from node+0x14/+0x18/+0x1c/
  +0x20; four comparator FUNCTIONS selected by worker-flags bits
  (r15 at 0xba61); 0xb469 is a weighted match-ratio
  (`w × min/max` — full weight on equality); a +0x258 score
  bonus for node+0xc bit 0x100 (hardware outranks in ties);
  negative table results REJECT; **a zero total score never wins
  the max-initialized-at-0 comparison.**
- **RESULT (verified live — digests match, consult logged,
  object built): ALL EIGHT SETS npix=0 with the float's exact
  software values.** Combined with rung 23 (0xFFFFFFFF on +0xc
  alone passes four sets): the scorer positively demands +0xc
  bits BEYOND the request's required set — in the unread tail
  (0xbca0–0xbd5c) or via the comparator functions. The object
  fields read so far are NOT the differentiator.
- Remaining candidates: (a) required +0xc bits in the unread
  tail; (b) the +0x08 id (ours 0x20500 in OUR measured plane;
  the float's 0x1000400 in its own) cross-checked caller-side in
  the worker's driver-id arrays — unread.
- **RUNG 26 PRE-REGISTERED — the empirical bisect of +0xc
  (committed before running):** the by-eye read has hit
  diminishing returns; the bits are nameable empirically. ONE
  line per build, live-swap, fresh-process {75} (robust — the
  cleanest subset-math-passing set) as the test probe:
  0xFFFF first (narrows to 16 bits), then binary halves of the
  passing range. PREDICTION: a minimal bit set emerges; leading
  candidate (registered): bit 0x100 — if +0x100 ALONE flips
  {75} from 0 to 1, the "hardware bonus" is load-bearing for
  software nodes too (a scoring floor, not a tiebreak), and the
  honesty ruling becomes the question (0x100 is the attr-73
  hardware bit; claiming it unbacked violates the boundary —
  the resolution would be the Mesa-backed claim, i.e. the
  original endgame, arriving via the scoring path).
- **THE HONEST READING OF A 0x100 POSITIVE — pre-registered
  BEFORE the bisect:** a single hardware bit producing npix=1
  will look exactly like success. It is not. It is the
  self-inflicted-result shape this arc has twice mistaken for a
  system verdict (the phantom wall; the flip experiment's
  accelerated=1). REGISTERED INTERPRETATION: "the scorer requires
  a claim we cannot yet back" — a probe result, not a fix; the
  bit REVERTS after the bisect concludes; the honest way through
  is the claim being TRUE (the Mesa-backed renderer). The bisect
  NAMES the requirement; it does not satisfy it.
- **CONTROL DISCIPLINE (registered):** every bisect point runs
  {75} AND the control {53} (the shortcut set — expected
  0/npix=0 at EVERY point regardless of +0xc, verified stable
  across rungs 19-25); any control deviation invalidates the
  point (something other than +0xc changed). The passing
  endpoint (0xFFFF) is re-run at the end — a bisect conclusion
  carried by a single observation is not a conclusion.

**THE RUNG-12 TABLE — SUPERSEDED, NOT PATCHED (2026-08-22):**
- Three errors from one transcription, all found by reading the
  float's builder instead of the map: the unreachable case-0
  gate (dead gate, rung 18a), obj+0 read as &_mh_bundle_header
  where it is the chain link (rung 19), +0x14 read as 0x8000
  where the float writes 0x8000000 (rung 25; +0x18's 1 was
  never transcribed at all). A fourth is more likely in what
  remains than in what has been replaced.
- VALIDATED by the float's builder read: flags base 0x4C8,
  +0x1c/+0x20 = 1, the walk-default shape, single-node +0.
- STILL CARRIED FROM IT, UNVALIDATED: the parser case semantics
  — which attrs consume values, the no-op cases, the 0x2710
  overflow. (The 2/50/53 shortcut is behaviorally confirmed by
  stub-log observation; cases 5/6 were re-derived in rung 24
  from the request-constructor read, not the table.) The
  replacement source: the float's OWN switch, grf.t jump table
  at 0x17920.
- RULE: nothing from the rung-12 table is trusted by default;
  the float's builder and switch are the source; the stub's
  parser now carries the provenance warning.

**RUNG 26 RESULT (2026-08-22) — THE BISECT CLOSED: FLAGS BIT 0
(the float's own conditional, never transcribed); THE FIRST
HONEST npix=1 ON THE CANONICAL APP REQUEST {5,84,1}; THE
REFUSAL STRUCTURE LANDS EXACTLY ON THE HONESTY BOUNDARY:**
- Bisect (env-driven instrument, one build; {53} control run at
  EVERY point, stable 0/npix=0 throughout):
```
T0 0x4C8|0x100 (hardware bit alone)  -> npix=0  (candidate DEAD)
T1 0xFFFF                            -> npix=1  (low word suffices)
T2 0xFBC8 (high byte of low word)    -> npix=0
T3 0x4CF  (low byte all)             -> npix=1  (bits 0/1/2 class)
T4 0x4C9  (bit 0)                    -> npix=1  ← THE BIT
T5 0x4CA  (bit 1)                    -> npix=0
T6 0x4CC  (bit 2)                    -> npix=0
endpoint 0x4C9 re-run ×2             -> npix=1, npix=1 (closed)
```
- **The mechanism, found in the float's build tail AFTER the
  bisect pointed at it (grf.t 0x17ca6):** `orl $1; testb $4;
  cmovel` — **the float ORs bit 0x1 into its flags when bit 0x4
  is absent.** Its software objects carry bit 0 conditionally;
  the rung-12 table never transcribed the conditional. A fourth
  table error, located by measurement then confirmed in source.
- **Standing value (mirrored conditional, env override dormant):**
  `if (!(flags & 0x4)) flags |= 0x1;` → 0x4C9 for plain walks.
  NOT a hardware claim — the float's own software bit.
- **FULL EIGHT SETS with the standing value (no env):**
```
{73} accelerated      npix=0 (honest refuse — requires 0x100)
{53} offscreen        npix=0 (shortcut, expected)
{73,5} accel+double   npix=0 (honest refuse)
{75} robust           npix=1
{1}  ALL_RENDERERS    npix=1
{1,73} ALL+accelerated npix=0 (honest refuse — note: passed in
                              rung 23's over-claim; refuses now)
{73,5,84,1}           npix=0 (honest refuse)
{5,84,1} double+mask  npix=1  ← THE CANONICAL APP REQUEST
```
- **{5,84,1} — kCGLPFADoubleBuffer + kCGLPFADisplayMask(0x1),
  the request shape real applications make — returns npix=1
  HONESTLY:** the +0x10 mode echo passes the exact match, the
  claim passes the mask gate, the scorer accepts the float's
  own conditional flags. The honest pixel format exists.
- **The refusal structure is the designed boundary, now
  empirical:** every set demanding the hardware bit (0x100)
  refuses; every software-capable set counts. The next npix=1
  on an accelerated set requires the Mesa-backed claim — the
  original endgame, and now the ONLY remaining door.
- **NEXT (rung 27 candidates, in the order the evidence
  favors):** (a) the watched boot — the stub now provides an
  honest software pixel format; WindowServer's boot-time
  consumption of it is the real-consumer test, with the
  registered outcomes and revert from rung 19; (b) a real
  context: CGLCreateContext on the {5,84,1} format — the first
  downstream entry (gldCreateShared/gldCreateContext refusals
  become load-bearing); (c) the destroy-absence residual
  (secondary).

**THE WATCHED BOOT — RUNG 19's SECOND HALF (2026-08-22 15:52) —
OUTCOME (iii): desktop normal, 0 panics; WindowServer's boot-time
GLD sequence is FOUR ENTRIES (load → initialize → version →
stop); the honest {5,84,1} result SURVIVES the boot; and two
state-dependence findings that qualify the bisect's absolutes:**
- Pre-flight verified: bundle digest = the rung-26 build,
  kext = the rung-17 build, vm-cap3d=1 in config.plist.
- **Boot clean:** 0 panic lines; gate=1; IOGLBundleName,
  AccelCaps, and the rung-17 property all published
  (15:52:41-46).
- **WindowServer (pid 97) loaded the GLD at boot:** STUB LOADED
  → gldInitializeLibrary (heap-shaped psvc/arg1 — its call site
  differs from the probes' stack shape) → glvmPreInit forwarded
  (rc=3, guard=1) → gldGetVersion TRUE — **the log ends there.
  No census consult, no pf call.** The boot-time entry sequence
  is four entries; WindowServer registers the driver and asks
  nothing further at boot under this registry state.
- **Desktop proxies green:** compositing ~52–54 Hz sustained,
  WindowServer healthy (pid 97, 0:05 CPU). The visual verdict
  belongs to the desktop watch.
- **Post-boot verification: the stub serves and {5,84,1} →
  npix=1 STANDS** (fresh-process log: the full chain — census,
  plugin dump, consult [0x5 0x54 0x1 0x4], object built).
- **INSTRUMENT TRAP (found and fixed): the boot-time log is
  ROOT-OWNED.** WindowServer's write creates /tmp/vm_gld_stub.log
  as root:wheel 644; every non-root process's stub logging then
  dies SILENTLY (fopen append fails, ep_log returns, consults
  proceed unlogged). The earlier "no probe entries" was this,
  not a load failure. RULE: after any boot that loads the stub,
  sudo-remove the log before probe runs (or the stub logs
  per-uid).
- **TWO STATE-DEPENDENCE FINDINGS (both reproduced; both
  deviations from pre-boot behavior):**
  (1) `{53}` offscreen → **npix=1** (was 0 across rungs 19–26).
      The stub's shortcut forces 0 whenever IT serves — so the 1
      arrives via the ENGINE's fallback retry (the worker's
      ecx=0 second pass consulting the FLOAT, which supports
      offscreen). The fallback flag (byte[global+0x21]&0x8)
      initializes differently under this boot's state.
  (2) `GLD_PF_FLAGS=0x4C8` on {75} → **npix=1** (the bisect
      endpoint FAILS to reproduce: pre-boot 0x4C8 → 0). The
      stub's flags word is no longer decisive for {75} — the
      scorer path itself differs, consistent with a different
      comparator selection (worker flags & 3) or the fallback
      answering before our node is scored.
- **QUALIFICATION OF THE BISECT (correction, stated as one):**
  the bit-0 requirement is STATE-DEPENDENT, not absolute — it
  held under the pre-boot processes' engine configuration and
  does not hold under this boot's. The rung-26 conclusion keeps
  its mechanism (the float's conditional, the fourth table
  error) but loses its absoluteness: "required" meant "required
  under that state". Any future bisect must record the boot
  state alongside the value.
- The standing conditional in the stub is UNCHANGED (it mirrors
  the float; harmless under either state).
- **The desktop watch's visual verdict: NORMAL** — rung 19's
  second half fully closed, outcome (iii) complete on all three
  instruments.

---

## RUNG 27 PRE-REGISTERED — the context rung: CGLCreateContext on
the honest {5,84,1} format (committed before implementation)

**The question:** the honest pixel format exists and counts. What
happens when a consumer tries to USE it — which downstream GLD
entries fire, and how does the caller receive the standing honest
refusals (EPR: nonzero + *out=NULL)?

**The change: PROBE-SIDE ONLY** (a new probe mode: choose
{5,84,1}; if npix ≥ 1, CGLCreateContext(pf, NULL, &ctx); if
created, one glGetString(GL_VERSION) — the first real GL call —
then clean teardown). NO stub or kext change; the refusals are
the honest standing state; this rung MAPS the downstream
sequence, it does not implement it. No boot; WindowServer
untouched; probe-only exposure.

**Predictions (registered before running):**
- (i) **Clean refusal propagation:** CGLCreateContext returns an
  error (10002-class) with ctx=NULL; the stub log shows the
  downstream entries firing in order (gldCreateShared and/or a
  shared-state pf consult, then gldCreateContext) each refusing
  -1; no crash; desktop unaffected. The convention's first
  exercise under a real consumer.
- (ii) **A refusal path crashes** (the rung-12 SIGBUS class — a
  caller dereferencing an out-param the refusal didn't fill,
  i.e. an entry whose out-zero shape is wrong): the crash report
  names the entry and the missing field; the fix is a typed
  refusal for that entry (next rung).
- (iii) **The fallback answers instead** (the watched boot's
  state-dependence theme): the engine's software path supplies
  the context — possibly via the float — and CGLCreateContext
  SUCCEEDS with a real software GL context; glGetString returns
  a version string. Milestone-class if it holds: applications
  could render through this chain while the Mesa-backed claim
  remains the accelerated endgame.
- Instruments: probe stdout (errors, ctx, version string), the
  stub log (entry order; REMOVE THE ROOT-OWNED LOG FIRST — the
  boot trap), crash reporter for (ii), desktop watch for
  stability (probe-only).
- Revert: none needed (no kext/stub change).

**RUNG 27 RESULT (2026-08-22) — PREDICTION (i) WITH A TWIST:
clean refusal, NO crash — but the refusals never fired. The
ENGINE's own context-path validation rejects the format before
any downstream GLD entry is consulted:**
```
[k] pf(double+mask1)  -> 0 npix=1 pf=0x100105a40
[k] CGLCreateContext  -> 10002 ctx=0x0     (kCGLBadPixelFormat)
run-exit 0; "invalid pixel format" CG stderr = CG's rendering of 10002
```
- The stub log for the run: load → initialize → version →
  census/plugin-dump → ONE pf consult ([0x5 0x54 0x1 0x4],
  object built) — **and nothing else. No gldCreateShared, no
  gldCreateContext, no shared-state re-consult.** The refusal
  convention (nonzero + *out=NULL) remains unexercised — the
  engine rejected the format ENGINE-SIDE.
- **The frontier: gliCreateContext's own validation of the pf
  object** (GLEngine 0x1526+, glimpsed in rung 19's read: it
  walks the pf's BYTES as counts — `movzbl (%r13),%eax`,
  size computations `×0x17f8` and `×24` per entry). The CGL
  pf object our node was copied into feeds those computations;
  some field fails the check. The next read names it.
- No boot, no desktop exposure; the probe-only rung is closed
  clean. The refusal convention's safety remains established by
  rung 10/12 evidence, still awaiting its first real exercise.
- **NEXT (rung 28 candidates):** (a) read gliCreateContext's pf
  validation (0x1526–0x1650 — which field produces 10002) and
  fix the offending object field from the float's values, the
  established pattern; (b) the state-dependence question (which
  boot-state flag feeds the fallback) — secondary to the
  context path, which is now the live frontier.

---

## RUNG 28 PRE-REGISTERED — gliCreateContext's pf validation:
which field produces 10002 (committed before the read)

**Known going in:** gliCreateContext (GLEngine 0x1526+) reads
the pf object's FIRST BYTES as counts (`movzbl (%r13),%eax`,
`movzbl 0x1(%r13),%edx`) and computes offsets `count×0x18` and
`count×0x17f8` — the CGL pf's internal header/arrays. The
transformer (0x37ff) built that object as [attrs][node copies];
our node's copied bytes feed these computations.

**Predictions (registered before reading):**
- (i) A COUNT/INDEX field derived from our node bytes (most
  likely the node's +8 id, +0xc flags, or +0x10 modes — read as
  a byte-sized index) lands OUT OF RANGE of the per-count arrays
  (0x17f8-element or ×24 tables) → 0x2712 (10002). The fix:
  the offending node field takes the float's value or a
  header-shaped value, per the read.
- (ii) A STRUCTURAL check fails first — e.g. the pf must carry
  ≥1 renderer id the engine recognizes (the worker's driver-id
  array cross-check), and our id (0x20500) is not in the
  expected set → the fix is id-side.
- (iii) The validation passes and the failure is deeper (the
  stub log would then show downstream entries on a retry —
  re-run mode k after any fix to confirm).
**Instrument:** /tmp/gle.t 0x1526–0x1700, the 0x2712 stores and
their guarding conditions. **Fix discipline:** float's values or
the read's own constants; single variable; verify by mode k
(clean exit, error changes or context created).

**RUNG 28 RESULT (2026-08-22) — THE CANONICAL ID FOUND AND
MEASURED (rung 9's decode VINDICATED; the rung-21 "correction"
CORRECTED); THREE FIRSTS: gldDestroyPixelFormat fired,
gldCreateShared consulted (the first downstream entry ever), the
refusal convention EXERCISED CLEANLY:**
- **The context path's id gauntlet (gle.t 0x1526–0x1663):** the
  pf node walk requires `(id & 0xff0000) == 0x20000` (the
  decoration plane — 0x15b4; THIS is why the engine decorates),
  tracks a preferred index when `(id & 0x7f00) == 0x400`
  (0x15db), then `_gfxCreateSharedState(&ids, count)` — whose
  per-id loop resolves the PLUGIN by `id & 0xffff00` AND the
  DEVICE by `id & 0xffffff00` (exact, 0x1803/0x1826) before
  consulting `[plugin+0x140]`. NULL from either lookup → NULL
  shared state → 0x2712 (10002).
- **The measurement (device list dumped via _gfxGetDevices,
  exported):** ONE device, `+0x10(id) = 0x1020400`, mask 0x1;
  `_gfx_float_device_id` (exported data) = 0x1020400 — the
  version-composed id `0x1020000 | (a3 & 0xFF00)` built from OUR
  gldGetVersion a3=0x400. **RUNG 9's DECODE WAS RIGHT ALL
  ALONG.** The rung-21 "correction" (plugin+0x110 = 0x20400) was
  itself wrong: +0x110 stores the id 16-bit-masked —
  `0x1020400 & 0xffff00 = 0x20400` resolves the plugin lookup
  fine. The full canonical id lives in the device table. Id
  saga closed: 0x1AF40100 → 0x20500 → 0x20400 → **0x1020400**
  (the measured device id; the stub now derives the pf id from
  it at runtime).
- **0x1020400 passes ALL FOUR id checks:** plugin
  (0xffff00-plane = 0x20400 ✓), device (0xffffff00 exact ✓),
  the 0xff0000 decoration plane (= 0x20000 — the composed id
  CONTAINS the decoration bit ✓), the 0x7f00 preferred index
  (= 0x400 ✓).
- **RESULT (mode k, fresh process):** pf npix=1;
  `CALL gldDestroyPixelFormat -> 0 (freed)` — THE FIRST FIRING
  EVER (the rung-19 ownership fix exercised; the
  destroy-absence residual CLOSED — the id resolution was what
  the walk needed); `CALL gldCreateShared -> -1 (refusal; out
  zeroed)` — **THE FIRST DOWNSTREAM CONSULT EVER** ([plugin+0x140]
  = gldCreateShared, name index 4), refused cleanly: no crash,
  exit 0, CGLCreateContext still 10002. **The refusal
  convention's first real exercise — it held.**
- **THE FRONTIER:** gldCreateShared must ANSWER for the context
  to proceed. Its call shape (from _gfxCreateSharedState
  0x1837-0x1855): `(out = &shared->slots[i] (0x168+i·0x20),
  rsi = device mask (0x1), rdx = 4)`; return 0 = success with a
  slot object. The slot's consumer: gliCreateContext's
  _gfxCompareSharedState / the context build past 0x1668.
- **NEXT (rung 29):** read the shared-slot contract (what
  _gfxCompareSharedState and the context build read from
  slots[i]) and implement an honest gldCreateShared — the first
  CREATOR entry to go real since the pf entry; the writability
  contract applies (heap, persistent, freed at the matching
  destroy).

---

## RUNG 29 PRE-REGISTERED — the shared-slot contract and an
honest gldCreateShared (committed before the read)

**Known:** the call shape `(out = &shared->slots[i] (0x168+i·0x20),
esi = device mask 0x1, edx = 4)`; return 0 = success with the
0x20-byte slot filled by the driver; nonzero tears down the whole
shared state. The consumers: gliCreateContext's post-creation
build (0x16c7+, unread) and whatever entry receives the slot
next.

**Method (the established pattern):** read the FLOAT's own
gldCreateShared FIRST (grf.t — the float is a full GLD serving
the software path; its slot shape is ground truth), then the
minimum engine code that consumes the slot.

**Predictions (registered before reading):**
- (i) The slot is a driver-opaque state block with a small
  engine-visible header (refcount or magic at +0); the float's
  build block gives the shape and values; the honest stub
  mirrors them (heap, 0x20 bytes, persistent; gldDestroyShared
  frees — the ownership contract).
- (ii) The slot feeds the NEXT entry (gldCreateContext-class)
  — the chain continues; each refusal surfaces as the next
  clean error, naming the next rung.
- (iii) The engine itself calls through a slot field (a
  dispatch pointer) — then the slot needs a real vtable-ish
  object and the float's values name it.
**Verification:** mode k — expect the error to CHANGE (from
10002 to the next code) or the next entry to fire in the stub
log; either is progress; a crash names a field (crash-report
instrument standing).
**Exposure:** live-swap, no boot; probe-only.

**RUNG 29 RESULT (2026-08-22) — gldCreateShared ANSWERED AND
ACCEPTED (the float's shape, mirrored); gldCreateContext FIRED
(the next creator); gldDestroyShared freed on the refusal path —
THE CREATOR CHAIN WALKS END-TO-END:**
- **The float's gldCreateShared read first (grf.t 0x13ed9,
  ground truth):** mask gate (request ⊆ gld_io_data's stored
  mask, nonempty — else 0x2716), `malloc(0x70)`, pthread mutex
  at +0, arg3 at +0x40, refcount dword at +0x48 (gldDestroy-
  Context decrements it), NULL list heads +0x50/58/60, processor
  block pointer at +0x68 (the float's glg_processor_default_data).
- **Implemented as the mirror:** g_vm_mask (rung 6b) plays
  gld_io_data's role in the gate; a writable zeroed stand-in for
  the processor block; gldDestroyShared frees (ownership).
- **RESULT (mode k, fresh process):**
```
CALL gldCreateShared mask=0x1 arg3=0x4
  gldCreateShared -> 0 (object 0x70 built)     ← ACCEPTED, kept
CALL gldCreateContext -> -1 (refusal)          ← the next creator
CALL gldDestroyShared -> 0 (freed)             ← CLEAN teardown
CGLCreateContext -> -1, ctx NULL, exit 0       ← the refusal propagated verbatim
```
  The shared state was accepted and retained; the engine tore it
  down through OUR destroy only when the next creator refused.
  **The create → accept → refuse → destroy → free cycle ran
  end-to-end with no crash.** The -1 error is rung 18's law
  (verbatim propagation) — recognized, not mistaken for a system
  verdict.
- **THE FRONTIER: gldCreateContext** — the last creator before a
  context exists. Its refusal is now the only thing between the
  probe and a live CGLContextObj. **NEXT (rung 30):** read the
  float's gldCreateContext (grf.t — likely the largest object
  yet; its context feeds the processor init and the dispatch
  paths the engine later calls through) and implement the honest
  mirror — the same pattern, one more creator deep. The honesty
  question arrives WITH it: a real context object means real GL
  entry calls (gldFlush/gldFinish/gldGetString-class) — the
  refusals there are the next convention test.

---

## RUNG 30 PRE-REGISTERED — the float's gldCreateContext and the
honest mirror (committed before the read)

**Method (established):** the float's implementation first
(grf.t `_gldCreateContext`), then the mirror; verify by mode k.

**Predictions (registered before reading):**
- (i) The float's context is a large object (its malloc size is
  the first datum), storing the shared pointer and initializing
  a processor/dispatch block it later calls through. The honest
  mirror: same size, shared stored, writable zeros, the stand-in
  where the float uses its processor.
- (ii) With CreateContext answered, CGLCreateContext returns 0
  with a REAL ctx and the first GL call fires the next entry
  (gldGetString-class) — the convention's meeting with actual
  rendering calls.
- (iii) The engine calls through a context field immediately and
  crashes — the crash names the field; the float's values fix it
  (the crash-report instrument standing).
**Exposure:** live-swap, no boot; probe-only. The desktop is
untouched (probe process only).

**RUNG 30 RESULT (2026-08-22) — THE CONTEXT EXISTS. A COMPLETE
CONTEXT LIFECYCLE THROUGH THE STUB GLD — create, default-state
consultation (dozens of entries, all refusals clean), teardown
with the refcount handshake. NO CRASH:**
```
[k] pf(double+mask1)        -> 0 npix=1
[k] CGLCreateContext        -> 0 ctx=0x100829800     ← LIVE CONTEXT
[k] glGetString(GL_VERSION) -> (NULL)                ← refusal → NULL
run-exit 0
```
- **The float's gldCreateContext read (grf.t 0x1403d):** a full
  software GL context — gldVecAlloc(0xC60), GL-state float
  defaults at +4..+0x1c, rasterizer hooks, **args at
  +0x738/0x740/0x748** (the offsets its own gldDestroyContext
  confirms: +0x738 = the shared, whose +0 mutex it locks and
  whose +0x48 refcount it decrements).
- **Implemented as the honest minimal mirror:** the float's
  SIZE (0xC60), the arg offsets, zeros elsewhere (the engine
  treats the GLD context as opaque except through entries);
  gldDestroyContext mirrors the refcount handshake (lock,
  decrement, unlock, free).
- **The observed downstream cascade — the first real GL consumer
  sequence through the driver:** 10× gldCreateTexture (the
  engine's default texture objects), gldCreateVertexArray,
  2× gldCreatePipelineProgram, gldSetInteger — ALL REFUSED
  cleanly (nonzero + out-zeroed); then the unwind: the
  UnbindTexture/DestroyTexture pairs ×10, the pipeline and
  vertex-array destroys, gldDestroyContext (handshake done,
  freed), gldDestroyShared (freed). **No crash, exit 0.**
- **The refusal convention held under its heaviest exercise** —
  dozens of creator/refusal/destroyer calls in one lifecycle.
- **The honest state:** contexts exist and live through full
  lifecycles; every GL call refuses; nothing renders.
  glGetString → NULL is the refusal reaching the app level.
- **THE ENDGAME, NOW WITH A COMPLETE LIFECYCLE TO PLUG INTO:**
  the remaining distance is the GL entries themselves — the
  Mesa-backed bridge forwarding GLD entries to virgl — the
  original destination, now reachable through a driver whose
  pixel formats, shared state, contexts, and teardown all work.
  Cheapest next datum (rung 31 candidate): an honest
  gldGetString (a string the stub can back — the software-null
  identity) to give apps a probeable version — or go straight
  for the bridge design.

---

## PRE-BRIDGE DESIGN DECISIONS (recorded 2026-08-22, before any
bridge code; the fork is taken: THE BRIDGE, with gldGetString
landed on the way as the first return-something-real entry)

**DECISION 1 — the concurrency model.** The substitute and the
GLD are two front ends onto one Mesa, and the substitute's
hard-won lessons are FRONT-END-SPECIFIC: the handoff mutex exists
because CGL contexts are server-side on real macOS; the
pthread-TSD current-context exists because two Gecko threads
share one process. A GLD serving arbitrary apps — WindowServer
included — has more concurrency than that, not less.
**Preliminary position (to be settled in the bridge design
rung):** the GLD entry API is CONTEXT-EXPLICIT — the engine
passes the ctx pointer as arg0 on every entry — so the
current-context problem (a CGL-layer concept) does not exist at
the GLD layer. The bridge therefore REUSES the front-end-
independent layers (the winsys, the kernel transport, the
per-context virgl plumbing from the fence era) and implements
its OWN context layer: GLD ctx pointer → virgl context id, with
per-ctx serialization (virgl executes a context's stream
serially — the fence-era contract) and cross-ctx concurrency.
The substitute's TSD/mutex code is NOT reused; its LESSONS are.

**DECISION 2 — exclusivity has teeth.** Once the GLD renders, it
is the renderer for its display; there is no software fallback
behind it. Every entry that currently refuses becomes something
an app depends on, and the first real consumer is WindowServer
at boot — not a launched probe. **The observed bounds (from the
watched boot):** WindowServer's boot-time GLD use was FOUR
ENTRIES (load, initialize, version, stop) — the desktop
composites through the IOAccel surface path, not GL, on this
system. The boot exposure is therefore bounded by observation —
but app-side GL rides entirely on the driver the moment the
formats are offered. **The staging that already exists (the
honesty boundary, now structural):** the hardware bit (0x100)
stays unset until the bridge backs it — accelerated-format
requesters receive nothing (npix=0) and the engine's fallback
path fields them; only software-format requesters reach the
stub/bridge. The first bridge consumers are bounded by that
gate, and the gate lifts only when the claim is true.

---

## RUNG 31 PRE-REGISTERED — gldGetString: the first
return-something-real entry (committed before implementation)

**The shape:** the probe's existing glGetString(GL_VERSION) call
is the instrument — already in place, already printing. The
float's gldGetString read first (the established pattern); the
honest string set follows: strings that describe the stub
TRUTHFULLY (vendor/renderer = our identity; version = what the
stub actually is, claiming no GL capability it refuses to
implement).

**Predictions:**
- (i) glGetString(GL_VERSION) returns our honest identity string
  (non-NULL, probe-printable) — the first real data an app
  receives from this driver; the other string enums likewise or
  cleanly refused (NULL for unsupported names, per the float's
  shape).
- (ii) The entry's signature/contract mismatches (arg shapes for
  name-vs-buffer) — a wrong string or crash names it; the float
  read settles the shape.
**Exposure:** live-swap, probe-only, no boot.

**RUNG 31 RESULT (2026-08-22) — gldGetString IMPLEMENTED AND
LANDED (the float's shape, the honest strings); the observation:
STRINGS ARE DISPATCH-GATED — the engine dead-ends every GL call
before the driver on a context whose dispatch was never
installed. THE BRIDGE'S FIRST ACT IS NOW A NAMED ENTRY:**
- The float's gldGetString read (grf.t 0x1dafc): switch on
  (name - 0x1F00), six names, const char* or NULL, ctx unused.
  Implemented with the honest set: VENDOR "VMQemuVGA Project",
  RENDERER "VirtIO GPU stub (software, no rendering)", VERSION
  "0.0 stub" (claims no capability the stub refuses to
  implement); unsupported names NULL.
- **Result: GL_VERSION, GL_VENDOR, GL_RENDERER all (NULL) —
  and ZERO gldGetString calls in the stub log.** The engine
  never consulted the driver for any string.
- **The corroborating datum (rung 30's log): gldInitDispatch
  was never called either** — the engine built default state
  through the creators, then unwound, with no dispatch
  installation. **READING: GL calls on a context whose dispatch
  was never installed dead-end in the ENGINE before any driver
  entry — strings included.** The dispatch table is the gate,
  and installing it is the bridge's core act.
- **THE BRIDGE'S SHAPE, SHARPENED:** the bridge's first real
  entry is gldInitDispatch — installing Mesa-backed GL functions
  into the engine's context dispatch — after which every GL
  call (strings first, observably) flows to the driver. The
  honest strings implemented here become live the moment
  dispatch is real; no further string work needed.
- Rung 31 closes with the fork taken and its first waypoint
  landed: the return-something-real path exists in the driver,
  gated on dispatch, ready for the bridge.

---

## RUNG 32 PRE-REGISTERED — THE BRIDGE RUNG, FIRST READ:
gldInitDispatch, both sides (committed before the read)

**The registered hypothesis (before any reading):** the probe
never calls CGLSetCurrentContext. glGetString on a NON-CURRENT
context may dead-end at the CGL/OpenGL-framework layer with no
driver consultation — making the rung-31 "dispatch gate" reading
premature. The gate may be CURRENT-CONTEXT (CGL-layer), with
dispatch installation happening AT make-current (which would
also explain why gldInitDispatch never fired: the probe never
made anything current).

**The reads (in order):**
1. The float's _gldInitDispatch (grf.t) — the contract: what
   the driver installs, where, with what arg shape.
2. The engine's call site (gle.t) — WHO calls it, WHEN (create
   vs first-make-current), and what gates it.
3. The probe extension: CGLSetCurrentContext(ctx) before the
   glGetString calls — the cheap discriminator between the
   current-context gate and the dispatch gate.

**Predictions:**
- (i) CURRENT-CONTEXT GATE: with the context made current, the
  engine calls gldInitDispatch at make-current; our refusal
  surfaces there (CGLSetCurrentContext errors, or the string
  calls still NULL); the float's contract read names what a real
  answer installs.
- (ii) DISPATCH GATE (as rung 31 read it): make-current
  succeeds with no InitDispatch call; the dispatch installation
  is deferred to first GL use and dies earlier for another
  reason the engine read names.
- (iii) The engine serves strings engine-side once ANY context
  is current (no driver call at all) — then the strings observed
  are the ENGINE's, and the driver's string path opens only
  with dispatch (bridge territory proper).
**Exposure:** probe-side only; live-swap; no boot.

**RUNG 32 RESULT (2026-08-22) — THE CURRENT-CONTEXT HYPOTHESIS
CONFIRMED; THE STRINGS ARE LIVE (the first real data an app has
received from this driver); the dispatch contract READ (a table
the driver fills with its renderer's operations); gldInitDispatch
still never fires — strings need no dispatch:**
```
[k] CGLSetCurrentContext -> 0
[k] glGetString(GL_VERSION)  = 0.0 stub
[k] glGetString(GL_VENDOR)   = VMQemuVGA Project
[k] glGetString(GL_RENDERER) = VirtIO GPU stub (software, no rendering)
```
- **Prediction (i) CONFIRMED, refined:** the gate was
  CURRENT-CONTEXT (CGL-layer), not dispatch — CGLSetCurrentContext
  succeeded and the string calls flowed to the driver entry
  directly. Rung 31's "dispatch-gated strings" reading is
  CORRECTED: strings consult gldGetString with no dispatch
  installed. The rung-31 strings went live unchanged.
- **The float's gldInitDispatch contract (grf.t 0x14d3b):
  `(ctx, dispatch_block(rsi), limits_out(rdx))`** — the driver
  FILLS a fixed-layout table with its renderer's operations:
  gldClear@+0x8, gldReadPixels@+0x10, accum/drawpix/copypix/
  bitmap/vertex-array/blit, begin/end-primitive-buffer@+0xb8/c0,
  the point/line/poly renderers@+0x30/38/40, noops where
  unsupported — and a LIMITS block (texture dims clamped to
  0x4000, from ctx+0x218's chain). **The bridge's installation
  point, read: Mesa's functions fill this table.**
- **gldInitDispatch never fired even now** — its trigger is NOT
  strings and NOT make-current; presumably the first
  RENDERING-class call or drawable attach. Naming its trigger
  (the engine call-site read) is the bridge rung's next step.
- 50 entry calls in the run's log — the full lifecycle now
  includes make-current. No crash, exit 0.

**RUNG 32 FOLLOW-UP — THE TRIGGER FOUND (empirically, the
discriminator ladder): THE DRAWABLE ATTACH. gldAttachDrawable is
the gate before gldInitDispatch — and it is the GA-era coupling
door, reached from the driver side:**
- Discriminators run: create ✗, make-current ✗, strings ✗, and
  **glClear ✗** (executed with NO dispatch call; glGetError
  returned GARBAGE 0xeea896e4 — the engine's error state for an
  undispatched context is uninitialized; datum recorded).
- **probe_cgs_requester with the stub live (the full window →
  CGSAddSurface → context → CGLSetSurface sequence):**
```
renderer[0]: accelerated=0 rendererID=0x1af60100   (census, unchanged)
CGLChoosePixelFormat(plain {5}) -> 0 npix=1        (another set counting)
CGLCreateContext -> 0 ctx=...
CGLSetSurface(ctx,cid,wid,sid) -> -1               ← the coupling step
CALL gldAttachDrawable -> -1 (refusal; out zeroed) ×2
```
- **THE TRIGGER: CGLSetSurface calls gldAttachDrawable;
  gldInitDispatch follows a SUCCESSFUL attach** (the engine
  installs dispatch when the context has a render target — why
  it never fired). The refusal propagates as -1 (SetSurface
  fails) — clean, no crash.
- **THE BRIDGE'S ENTRY SEQUENCE, COMPLETE AND ORDERED:** pf ✓ →
  shared ✓ → context ✓ → current ✓ → strings ✓ →
  **gldAttachDrawable (NEXT — must answer)** → gldInitDispatch
  (install Mesa's table) → rendering calls.
- **The circle closes:** the GA-era coupling step (CGLSetSurface
  — where the substitute's path died) is the SAME door the GLD
  reaches from the driver side: gldAttachDrawable. The surface
  work (AddSurface OK, surfaceID real) and the GLD work now meet
  at one entry.
- **NEXT (rung 33):** the float's gldAttachDrawable (grf.t) —
  the contract (the drawable/surface args, what a success
  stores), the honest mirror, and then the InitDispatch
  firing becomes observable.

---

## RUNG 33 PRE-REGISTERED — the float's gldAttachDrawable: the
contract, the honest mirror, and the InitDispatch firing
(committed before the read)

**Predictions (registered before reading):**
- (i) The entry receives a drawable descriptor (the engine's
  resolution of the CGS surface into render-target terms),
  stores target state in the ctx at float offsets, returns 0.
  The honest mirror: store the args, return 0 — then
  gldInitDispatch fires and the sequence continues.
- (ii) The attach touches the SURFACE machinery (IOSurface/
  IOAccelSurface-class args) — as a SOFTWARE renderer the float
  attaches the descriptor without hardware mapping; the mirror
  does the same, and the surface work's structures (the GA-era
  findings) become the arg shapes to recognize.
- (iii) A validation gate refuses (shape/flags/mask) — the read
  names it and the mirror honors it.
**Verification:** probe_cgs_requester rerun — AttachDrawable
answers 0; CGLSetSurface's return changes; gldInitDispatch
fires (the first time); whatever the engine does with an
installed-but-refused dispatch table is the next datum.
**Exposure:** live-swap, probe-only, no boot.

**RUNG 33 RESULT (2026-08-22) — PREDICTION (i) CONFIRMED IN
FULL: the attach answered, gldInitDispatch FIRED FOR THE FIRST
TIME, and CGLSetSurface RETURNED OK — THE GA-ERA COUPLING STEP
IS OPEN:**
```
CALL gldAttachDrawable type=0x50 -> 0   (0x50 = 80 = the WINDOW class)
CALL gldInitDispatch    -> -1 (refusal)  ← first firing ever
CGLSetSurface(ctx,cid,wid,sid) -> 0 (OK)  ← THE COUPLING STEP
CGSFlushSurface -> 0 (OK); CGSOrderSurface -> 0 (OK)
... clean teardown (context handshake, shared freed)
```
- **The float's gldAttachDrawable (grf.t 0x1745d):** esi is a
  DRAWABLE-TYPE code — 0x36 (fullscreen-class) refused with
  0x271c EVEN BY THE FLOAT; 0x35 (offscreen) and the common
  path run glsAssignDrawable (the float's renderer-side surface
  allocation), store the type at ctx+0x210, compute buffer
  sizes from the drawable object at ctx+0x218. The mirror keeps
  the type store + the float's 0x36 refusal, answers 0; the
  surface machinery is bridge territory.
- **The observed type: 0x50 (kCGLPFAWindow=80) — the window
  class**, the float's common path.
- **The trigger chain confirmed end-to-end:** attach success →
  gldInitDispatch. The engine proceeded to the CGS surface ops
  (flush/order — CGS-side, not GL) and tore down cleanly.
- **THE FRONTIER IS NOW EXACTLY gldInitDispatch** — firing,
  refused, waiting for a real answer: the dispatch table
  (ctx, dispatch_block, limits_out) the float's read already
  decoded to its slots (clear/readpixels/render-paths/buffers,
  noops for unsupported).
- **NEXT (rung 34): the honest gldInitDispatch** — the float's
  OWN pattern for a capability it lacks is a NOOP FUNCTION
  (gldNoop installed in unsupported slots). The stub's honest
  dispatch: NOOPS IN EVERY SLOT (every rendering call
  "succeeds" vacuously and logs — consistent with the RENDERER
  string "software, no rendering"; the app draws, nothing
  appears, no lies are told). The bridge then replaces noops
  with Mesa calls slot by slot. **This is the bridge's opening
  move proper: the table exists, filled honestly, and each slot
  that goes real is one Mesa-backed GL function live.**

---

## RUNG 34 PRE-REGISTERED — the honest gldInitDispatch: noops in
every slot; the full app draw cycle (committed before
implementation)

**The implementation (per the float's read, grf.t 0x14d3b):**
`(ctx, dispatch_block(rsi), limits_out(rdx))` — fill EVERY
offset the float writes (+0x0/+8/+10/+18/+20/+28/+30/+38/+40/
+48/+80/+88/+90/+98/+a0/+b8/+c0/+c8/+d0/+f0/+f8/+100) with a
noop function (the float's own pattern for unsupported
capability); the limits block zeroed (the float's no-drawable
branch: maxes [0]/[4] = 0 without ctx+0x218). NO slots beyond
the float's writes (the block's upper extent is engine-owned).
The noop logs its first dispatches then runs silent (GL apps
make thousands of calls).

**The probe extension:** probe_cgs_requester gains the full app
draw cycle after the surface coupling — CGLSetCurrentContext,
glClearColor/glClear, **CGLFlushDrawable** (the compositing
call — the first flush through our driver).

**Predictions:**
- (i) gldInitDispatch answered 0; the engine accepts the
  all-noop table; the draw cycle runs (glClear succeeds
  vacuously, CGLFlushDrawable returns something); the noop log
  shows the engine's first dispatch-class calls; no crash.
- (ii) The engine validates a slot (calls one immediately, or
  checks a slot non-NULL beyond the float's set) — a crash or
  error names it.
- (iii) The flush path fires a new entry (gldFlush-class) —
  another refusal convention exercise.
**Exposure:** live-swap, probe-only, no boot.

**RUNG 34 RESULT (2026-08-22) — THE DISPATCH TABLE INSTALLED AND
ACCEPTED (22 noop slots, no crash); THE FULL APP DRAW CYCLE RAN
GREEN — current, clear, SWAP; the honest negative: zero
dispatches through the table — the engine requires a render
target, and the attach stored only the TYPE. The missing piece
is the drawable object at ctx+0x218:**
```
CGLSetSurface -> 0 (OK)              (the coupling)
CGLSetCurrentContext -> 0
glClear done, glGetError = 0x506     (engine-side code — likely "no render target")
CGLFlushDrawable -> 0                THE SWAP — succeeded
gldInitDispatch -> 0 (22 noop slots installed; limits zeroed) — ACCEPTED
52 CALL entries, clean teardown, no crash.
```
- **Prediction (i) confirmed except one clause:** the table
  accepted, the cycle green, the swap returned 0 — but NO noop
  dispatches logged: glClear stayed ENGINE-side. The 0x506 error
  (deterministic this run; the earlier 0xeea896e4 was the
  pre-dispatch state) is the engine's "no render target"-class
  refusal to dispatch.
- **The mechanism named by the float's own code:** the float's
  attach (glsAssignDrawable) creates a DRAWABLE OBJECT at
  ctx+0x218 — the field the float's InitDispatch limits read
  derives maxes from ([draw+8]/[draw+0xc], [draw+0x76]). Our
  mirror stored only the type at +0x210 and left +0x218 NULL —
  the engine's render path checks the driver's drawable before
  dispatching.
- **Instrument notes:** probe_cgs_requester builds as
  objective-c++ (-x objective-c++; the 10.6 Security headers
  require it; one pre-existing int→CGLError cast fixed); the
  first two build failures shipped the AUG-14 BINARY silently —
  caught by the missing draw-cycle lines (the exit-status
  lesson, applied).
- **NEXT (rung 35): the minimal drawable object at ctx+0x218** —
  read the float's glsAssignDrawable for the object shape (dims
  at +8/+0xc, the +0x76 word, whatever else the engine reads),
  mirror it in gldAttachDrawable, and the noop dispatches begin
  — the first GL calls flowing through the driver's own table.
  From there, each slot that replaces a noop with a Mesa call
  is one GL function live: the bridge, slot by slot.

---

## RUNG 35 PRE-REGISTERED — the drawable, FIELD-SEPARATED: mirror
only what the engine consumes; the limits block IS the engine's
view (committed before the read)

**The refinement rung 34's inference needs (correction of its
own reading):** the engine CANNOT read the driver's ctx+0x218 —
the GLD context is driver-opaque. The engine's only view of the
drawable is the LIMITS BLOCK gldInitDispatch returns — and our
zeros were the float's own NO-DRAWABLE values (its limits read
derives real maxes from ctx+0x218 only when a drawable exists).
**HYPOTHESIS: the 0x506 dispatch gate is the limits block —
zeros told the engine "no render target."** The generalization
(recorded as a contract fact): dispatch and likely other engine
paths are gated on DRIVER-RETURNED STATE, not just entry
success — preconditions are discoverable by error-class, cheaper
than by crash.

**The field separation (the rung-12 lesson, applied up front):**
the float's drawable object mixes (i) ENGINE-relevant products —
the dims at +8/+0xc and the +0x76 word that feed the limits —
with (ii) FLOAT-INTERNAL rasterizer state (backing pointers,
strides, formats the float itself allocates and reads back).
Mirror ONLY group (i). A wholesale copy risks the rung-12 shape
of error: a faithful structure whose semantics differ.

**Steps:** (a) log gldAttachDrawable's FULL args (a3/a4 unlogged
so far — likely dims or a descriptor); (b) read the float's
glsAssignDrawable minimally — the object's field map, separated
into the two groups; (c) implement: attach stores the dims (from
wherever they arrive), InitDispatch derives REAL limits;
(d) verify: glClear dispatches through the noop table (the
"NOOP dispatch #1" line) and/or 0x506 clears.

**Predictions:**
- (i) Real limits → glClear dispatches (first NOOP line), 0x506
  clears; the app draw cycle is then fully driver-routed.
- (ii) The dims arrive in the attach args; if not,
  glsAssignDrawable's read names their source.
- (iii) The float's drawable carries group-(ii) internals the
  mirror must NOT copy — recorded field by field.
**Exposure:** live-swap via probe/deploy_gld.sh (the deploy
guard: refuses failed builds, unchanged binaries, digest
mismatches — the stale-deploy class closed); probe-only.

**RUNG 35 RESULT (2026-08-22) — the deploy guard LANDED AND
PROVEN; the window descriptor dumped (ID-SHAPED, not dims); the
limits hypothesis TESTED AND KILLED (honest negative); the 0x506
mechanism traced to the engine's FALLBACK-DISPATCH path, with
the real dispatcher (gleDoSelectiveDispatchCore) named as the
last layer:**
- **The deploy guard (probe/deploy_gld.sh + the bundle's
  Info.plist moved into the repo):** refuses failed builds,
  unchanged binaries (hash vs last deploy), and guest-digest
  mismatches. PROVEN: fresh deploy verified
  (`ec0e1020…`, 94 exports); immediate re-run REFUSED. Used for
  every deploy since. The stale-binary class is closed.
- **The window-class descriptor (type 0x50 attach, args now
  logged):** `a3 = +0=0xa913 +4=0x17 +8=0xe144a08 +0x10=0x40` —
  ID-SHAPED (the +8 word is surface-id class; NOT the offscreen
  path's dims-at-+0/+4). The dims source for window drawables
  remains unnamed.
- **The limits hypothesis KILLED (prediction (i) negative):**
  limits maxes 0x4000/0x4000 (the float's clamp) changed
  NOTHING — 0x506 persists, zero noop dispatches. The limits
  block is not the gate.
- **The 0x506 mechanism, traced (read):**
  - `[ctx+0x798e]` is the dispatch-ready byte: set 1 by the
    engine-ctx initializer (0x4906), cleared by
    `gleUpdateDispatchCodeChange` (0xf8b5: `!(flags & 0x10)`
    under a 0x4000000 mode) — 0x506 fires when it is 0
    (0x46177-0x4618e).
  - The flags come from the dispatch-update call at 0xf72a:
    **plugin slot 9 = gldAttachDrawable, called AGAIN with
    type 0x5c** (a dispatch-class query, rdi = the per-driver
    slot object); return ≤ 3 routes into gleFallbackBegin.
  - **gleFallbackBegin (0xe7633) is the SOFTWARE path — CORRECT
    for the float** (the software renderer's table serves it):
    mallocs 0x108 fallback state at [ctx+0x6560], keys on
    `[[ctx+0x65c0]+0x5a]`, sets [ctx+0x6d0]=-1, and calls
    **gleDoSelectiveDispatchCore(0xc000000, 0x20000)** — the
    REAL dispatcher. Our context dies inside this layer.
  - The field separation held throughout: nothing of the
    float's rasterizer internals was mirrored; the limits test
    used only the float's clamp constant.
- **NEXT (rung 36): gleDoSelectiveDispatchCore** — what it
  reads from the driver (our dispatch table? the per-driver
  block? which fields) and what makes it succeed for the float.
  The last layer between here and GL calls flowing.

---

## RUNG 36 PRE-REGISTERED — gleDoSelectiveDispatchCore: the
real dispatcher's reads (committed before the read)

**Predictions:**
- (i) The core selects functions from the driver's DISPATCH
  TABLE (our all-noop block satisfies it structurally) and/or
  the per-driver engine block — the failure is a field it
  checks before calling, named by the read.
- (ii) The core calls a driver function immediately — the first
  real dispatch, a logged noop.
- (iii) The core keys on the [[ctx+0x65c0]+0x5a] config state
  (a caps/config block another entry fills) — the read names
  which.
**Exposure:** read-only this rung; implementation follows the
named mechanism.

**RUNG 36 RESULT (2026-08-22) — THE GATE FOUND AND OPENED: the
engine MEMCPYS the driver's entire table into its context, and
the dispatcher's thunk IS GLD SLOT 11 (gldUpdateDispatch) — its
return's bit 2 gates all dispatch. The honest mirror (the
float's base return 4) opened it: NOOP DISPATCH #1 — THE FIRST
GL CALL THROUGH THE DRIVER'S OWN TABLE. 0x506 GONE:**
```
CALL gldUpdateDispatch #1 -> 4 (rung 36)
NOOP dispatch #1                       ← first dispatch through our table
glClear done (0x506 gone; the error slot now shows uninit-garbage — a different class)
CGLFlushDrawable -> 0
```
- **gliCreateContext's tail (0x18b4-0x1918) — the installer
  read:** per driver: [engine-ctx+0x65b8] = the CreateContext
  slot object; [engine-ctx+0x65c0] = &the per-driver sub-block
  (the +0x59/+0x5a reads); then **memcpy(engine-ctx+0x6708,
  plugin+0x120, 0x270)** — the ENTIRE 78-slot GLD table copied
  into the engine context. [ctx+0x6760] = 0x6708+0x58 =
  **slot 11**.
- **gleDoSelectiveDispatchCore (0xdea89) mapped:** mode-mask
  gate ([0x4e40]); deferred-state gate ([0x6ec]&[0x88c] |
  [0x6e8]&[0x888] | (mode|[0x884])&[0x6e4]) →
  gleUpdateDeferredState (its nonzero return SETS THE GL ERROR
  directly); dirty-state gate (five ANDed pairs) → **the
  [0x6760] call: gldUpdateDispatch(driver_ctx, template_block,
  dirty_block)** → the return's bit 0 vs [0x798b], bit 2 to
  continue.
- **The float's gldUpdateDispatch (0x152da):** state-diff
  machinery — drawable-change detection (invalidates buffers,
  ORs 0x10000380 into the dirty block), function selection via
  the shared's processor block; **base return 4 or 0xC — bit 2
  always set** (0x15399/0x153d8), 0x10 ORed conditionally,
  dirty-block ORs 0x80/0x100 for real state changes.
- **The honest mirror:** return 4 (the float's base), mark
  nothing dirty (the stub has no state — true). Dispatch opened.
- **THE ARC STATE, END TO END:** formats ✓, shared ✓, context ✓,
  current ✓, strings (real data) ✓, attach ✓, coupling
  (CGLSetSurface OK) ✓, table installed ✓, **dispatch FLOWING
  (noops)** — the stub is now a complete, honest, non-rendering
  GL driver. **THE BRIDGE BEGINS: replace noops with Mesa, slot
  by slot.**

---

## RUNG 37 PRE-REGISTERED — THE FIRST BRIDGE RUNG: one real slot
(gldClear) through the virgl transport (committed before
implementation)

**The design (the light end of the bridge first):** the clear
slot (+0x8 in the dispatch table) goes REAL by DIRECT TRANSPORT
— the stub builds a VIRGL_CCMD_CLEAR batch and submits it
through the kernel's virgl user client itself, NO Mesa linkage
yet. Increment C proved this exact command path byte-exact; the
kernel logs every batch (verification without readback). Later
slots bring Mesa in for real GL state; the first slot proves
the PLUMBING: a GLD dispatch call reaching the device.

**The reads that shape it:** (a) the float's gldClear (grf.t) —
the slot's arg shape (does the engine pass color/mask, or does
the driver read its ctx?); (b) the iokit winsys source
(Mesa-VirGL, branch cross-10.6) — the minimal user-client call
sequence for context-create + batch-submit.

**Predictions:**
- (i) The clear slot fires on the probe's glClear (replacing
  the noop); the stub submits a virgl clear batch; kernel.log
  shows the batch on a NEW kernel ctx (the stub-created one);
  no crash; CGLFlushDrawable still 0.
- (ii) The transport open fails from the stub's process (the
  user client's matching/gate) — the error names the missing
  step (e.g. the accel-surface gate, or the client requires the
  GA path).
- (iii) The batch is rejected by the device (command malformed
  without Mesa's context setup — virgl may need ctx
  initialization beyond create) — the fence/response code names
  it.
**Exposure:** live-swap via the guard; probe-only; no boot. The
kernel side is unchanged (the existing user client and fence
machinery).

**RUNG 37 RESULT (2026-08-22) — THE FIRST BRIDGE SLOT IS REAL:
a GLD dispatch call reached the device. The probe's glClear →
the real slot → the stub's own kernel virgl context → a
correctly-formed CLEAR batch, accepted and queued. PREDICTION
(i) CONFIRMED IN FULL:**
```
stub:    CLEAR-REAL #1 mask=0x4000
         rung37: virgl ctx 256 OPEN (transport live)
         0x6008 submit -> 0x0
kernel:  createVirglContextEx selector=0x6000 → ok ctx=0x100 resp=0x1100
         submitVirglCommandsEx: ctx=0x100 size=36
           [0]=0x00080007 [1]=0x00000001 [7]=0x3ff00000
         v3d async submit worker running — QUEUED
```
- **The plumbing, proven end to end:** the engine's glClear
  dispatched through OUR table's clear slot (not the noop); the
  slot opened the transport itself (matching
  VMQemuVGAAccelerator → IOServiceOpen type=4 → 0x6000 ctx
  create → ctx 0x100, the stub's OWN kernel context, separate
  from the substitute's); built the FCE1 frame with the CLEAR
  blob (VIRGL_CCMD_CLEAR=7, size 8, header 0x00080007); and
  submitted — the kernel's dump shows the batch EXACTLY as
  built, with the mask correctly mapped from the engine's
  GL_COLOR_BUFFER_BIT (0x4000) to the pipe bit (1). No crash;
  CGLFlushDrawable still 0.
- **The float's contract honored:** gldClear(ctx, mask) — the
  engine passes the mask; color is driver-side state (black for
  this rung, documented; the color path is a later slot's
  business).
- **The 0x1100 rule stands (honest boundary):** resp=0x1100
  means QEMU PARSED the batch — host ACCEPTANCE is unproven
  without readback. A bare CLEAR with no Mesa-side context
  initialization (no bound framebuffer/surface state in the
  virgl context) may be ignored or rejected host-side — the UTM
  debug log is the settling artifact, and the first READBACK
  slot (ReadPixels) is the proof instrument. This rung's
  registered goal was the PLUMBING; it is achieved.
- **THE BRIDGE IS BORN: one slot of twenty-two is real.** The
  pattern scales: each slot that goes real follows the same
  shape — the float's contract for the args, the virgl protocol
  for the command, the winsys call sequence for the transport,
  the kernel log for verification. Next slots in evidence
  order: ReadPixels (+0x10 — the readback proof for acceptance),
  then the surface-binding slots (the drawable object's real
  machinery, where the GA-era surface work plugs in).

---

## RUNG 38 PRE-REGISTERED — the ReadPixels slot: THE READBACK
PROOF (committed before implementation)

**The design:** the proof requires the clear to land somewhere
readable — a bare CLEAR with no bound framebuffer may be a
host-side no-op (rung 37's prediction-(iii) territory). The
minimal chain, all through the stub's own transport (the
Increment-C shape, hand-built): create a 2D resource (0x6002) →
attach backing (0x6003) → batch: SET_FRAMEBUFFER_STATE (CCMD 5)
binding it + CLEAR (CCMD 7, a DISTINCTIVE color — 0.25/0.5/0.75/
1.0, so zeros are never ambiguous) → submit (0x6008) →
TRANSFER_FROM_HOST_3D (0x3009) → byte-compare. The ReadPixels
slot (+0x10) implements the engine-side contract (the float's
signature read first); the probe calls glReadPixels after
glClear and prints the first pixels.

**Predictions:**
- (i) ROUND TRIP PROVEN: readback bytes match the distinctive
  clear color byte-exact — the first HOST-ACCEPTED rendering
  through the GLD bridge; the 0x1100 boundary crossed with
  pixels.
- (ii) The clear is a no-op without full Mesa context state
  (virglrenderer may require more than a bound cbuf) — readback
  ≠ clear color; the gap (what virglrenderer needs) is named by
  the UTM debug log; the ReadPixels slot's plumbing still lands.
- (iii) A transport/protocol error (resource create, backing,
  transfer shapes) — the kernel/return codes name it; fix in
  place from the winsys source.
**Exposure:** live-swap via the guard; probe-only; no boot;
kernel unchanged.

**RUNG 38 RESULT (2026-08-22) — PREDICTION (ii): the chain is
CLEAN at every transport step and the bytes are ZERO — the clear
does not execute host-side. Two gaps closed (ctx-attach among
them), the settling instruments identified, the negative control
REGISTERED and pending on a degraded guest link:**
```
rung38: fb res 257 created+backed+ctxAttached (0x6009 -> 0x0)
  0x600B wait -> 0x0; 0x3009 transfer_from -> 0x0
  backing[0..15]: 00000000000000000000000000000000
  readback MISMATCH — clear not executed host-side
```
- **The slot and the chain work:** the ReadPixels slot fires on
  the probe's glReadPixels; the self-contained proof runs
  resource-create (0x6002), backing (0x6003), ctx-attach
  (0x6009), the SET_FRAMEBUFFER_STATE+CLEAR submit (0x6008),
  the fence wait (0x600B), and TRANSFER_FROM_HOST (0x3009) —
  every step returns clean. No crash; the swap still 0.
- **Gap 1 closed (the winsys's own comment, LEDGER 6d9a278):**
  ctxAttachResource (0x6009) is REQUIRED before
  SET_FRAMEBUFFER_STATE can reference a surface — added;
  still zeros. (The project learned this in the Mesa era and
  re-derived it here — the second time this gap has cost a
  rung; the winsys comment is now honored in the stub with its
  provenance cited.)
- **The remaining discriminators, in order:**
  (a) **THE INCREMENT-C CONTROL (registered, pending):**
  virgl_clear_test — byte-exact clears through this same
  kernel+host stack — rerun on the live system. If it PASSES,
  the host is fine and MY 19-dword encoding is wrong (isolated
  to the batch); if it FAILS, the host state changed
  (regression class). ATTEMPTED this session: the 4.8MB
  libOSMesa transfer stalled on a degraded guest link (both
  ssh pipes hung; killed). Run it the moment the link recovers.
  (b) THE UTM DEBUG LOG (virglrenderer's decode errors are
  invisible from the guest by construction): Debug Log is NOT
  enabled on this VM (verified in the QEMU command line — no
  -D/debug path); enabling it requires a VM restart — the
  WATCHED-BOOT class; the standing rules govern.
- **Honest state:** one slot real and reaching the device
  (rung 37, proven); the readback slot's plumbing real (this
  rung); HOST ACCEPTANCE still unproven — the 0x1100 boundary
  stands, now with two named instruments to cross it.

**RUNG 38 CONTINUATION (2026-08-22 evening, post-restart with
the debug log ENABLED) — THREE REAL FIXES; the host now accepts
everything, the batch EXECUTES, the transfer COPIES PIXELS —
and the content is STILL ZERO. The debug log earned its keep
twice over:**
- **The VM restarted with Debug Log enabled** (the user, at the
  screen; desktop confirmed up; ssh via IP + legacy algorithms
  after a full mDNS outage — the config's syntax, not my first
  attempts' mangled options).
- **The Increment-C control: INCONCLUSIVE as run** —
  virgl_clear_test crashed GUEST-SIDE at glClearColor+14 (NULL
  at 0x670; the substitute's runtime setup absent in a standalone
  process; no transport calls reached the kernel). Not a host
  signal; recorded.
- **FIX 1 — the debug log's verdict on the first proof run:**
```
vrend_resource_create: Illegal resource parameters — Invalid texture bind flags 0x4
vrend_decode_create_surface_common: Illegal resource 256
vrend_decode_ctx_submit_cmd: Illegal command buffer
```
  **bind=0x4 was wrong in BOTH namespaces** — VIRGL_BIND_RENDER_
  TARGET = 1<<1 = 2 (virgl_hw.h:595; PIPE's is also bit 1). The
  whole zeros cascade began at resource creation. Fixed to 2.
- **FIX 2 (already in): ctxAttachResource. FIX 3: the FCE1
  frame now declares the resource** (cres=1) — the fence era's
  own design; cres=0 left the 0x600B wait vacuous, racing the
  async batch.
- **After all three fixes — the deepest datum yet: NO host
  errors at all; the kernel logs `v3d batch done ret=0x0 ms=3`
  (EXECUTED) and `transferFromHost3D: Resource 258 pixels
  copied from host to guest` (COPIED) — and the backing reads
  zero.** The clear runs against a decoded-clean framebuffer;
  the resource's content stays empty.
- **NEXT (registered): THE MESA STREAM DIFF** — capture Mesa's
  own working clear through the substitute with
  VIRGL_IOKIT_DUMP=1 (the winsys's full dword dump, an
  instrument this project built), diff against the stub's 19
  dwords; the missing commands (Mesa's context init: viewport,
  blend objects, whatever vrend requires before a clear lands
  in the texture) name themselves. The control's standalone
  failure means the dump runs under the substitute's env
  (DYLD path to the substitute's OpenGL), the historical
  configuration.

**RUNG 38 COMPLETE (2026-08-22 evening) — *** ROUND TRIP
PROVEN: 16/16 pixels == proof color *** — THE FIRST
HOST-ACCEPTED, BYTE-VERIFIED RENDERING THROUGH THE GLD BRIDGE:**
```
[20:33:21 pid=604] backing[0..15]: bf8040ffbf8040ffbf8040ffbf8040ff
[20:33:21 pid=604] *** ROUND TRIP PROVEN: 16/16 pixels == proof color ***
```
- **THE MESA STREAM DIFF RAN AND SETTLED IT:** the rebuilt
  virgl_clear_test (rebuilt from source — the Aug-11 binary
  was stale against the Aug-13 lib; and the substitute-dir lib
  was ALSO stale — the build-tree libOSMesa, digests verified,
  was the working one) + GALLIUM_DRIVER=virgl +
  VIRGL_IOKIT_DUMP=1 captured Mesa's own working clear batch.
  Mesa's own test FAILED its byte check on this run (a red
  herring — its expected values assume a byte order the
  transfer doesn't produce) but ITS STREAM IS THE AUTHORITY.
- **THE DIFF'S FINDINGS (Mesa's first submit, cdw=37):**
  (1) **THE CLEAR MASK IS 4** — Mesa sends 00000004 for a
  color clear. Mask 1 clears DEPTH — into a framebuffer with
  no depth surface: a no-op. **THE ZERO CONTENT WAS THE STUB'S
  OWN MASK BIT.** Fixed: color=4, depth=2, stencil=1.
  (2) Mesa binds a DEPTH SURFACE (separate depth resource,
  format 0x10, zsurf in SET_FB) plus a DSA object and two
  pre-commands (0x1d/0x1c) — NOT required for the color-only
  proof (mask 4 alone sufficed), noted for later slots.
  (3) The rest of the stub's encoding matched Mesa's exactly.
- **THE FIX SEQUENCE THAT CROSSED THE 0x1100 BOUNDARY (five,
  each named by its instrument):** bind flags 0x4→2 (the
  debug log: "Invalid texture bind flags 0x4"); ctxAttach
  Resource (the winsys's REQUIRED comment); the FCE1 cres=1
  frame (the fence era's design); **the clear mask 1→4 (the
  Mesa stream diff)**; the readback byte order (observation:
  the transfer yields R,G,B,A bytes for a B8G8R8A8 resource —
  Mesa's own test fails on this same order, corroborating).
- **THE PROVEN CHAIN, END TO END:** the probe's glClear/
  glReadPixels → the GLD dispatch table's REAL slots → the
  stub's own kernel virgl ctx → resource/surface/framebuffer/
  clear commands → virglrenderer EXECUTING the clear into the
  texture → TRANSFER_FROM_HOST → the guest backing → the
  byte-exact verdict. **Pixels, not log lines. The readback
  proof is complete; two slots of twenty-two are real and
  verified.**

---

## RUNG 39 PRE-REGISTERED — the surface-binding slot: a REAL GL
target (the window), rendered and PRESENTED (committed before
implementation)

**The goal:** rendering leaves the stub's private 4x4 texture
and lands in the probe's actual window — visible pixels, the
strongest verification class this project has. The pieces: a
window-sized virgl resource as the render target (dims from a
source the float's window path names), the clear targeting it,
and a presentation path for CGLFlushDrawable.

**The reads:** (a) the float's glsAssignDrawable WINDOW path
(grf.t 0x210a3+, type 0x50) — where the window DIMS come from
(the type-0x50 descriptor was id-shaped at +0..+0x14; the dims
are further in, or from a CGS call with the ids); (b) the kext's
0x600C hostBlit3D (the relay-era blit-to-scanout) and the GA
surface path as the presentation candidates (0x600C = the
desktop scanout; the GA surface client = the window-correct
door, the milestone-2/3 machinery).

**Predictions:**
- (i) THE ON-SCREEN PROOF: the probe's window shows the clear
  color (user-visible), with the stub log confirming the
  window-sized target; the round trip generalized from the
  private texture to a real drawable.
- (ii) The dims require the descriptor's later fields or a CGS
  query — the float's path names it; if the GA path is needed
  for presentation, its machinery (SetIDMode/Lock/Flush) is
  already proven and becomes the door.
- (iii) The presentation needs kext-side additions (a
  surface-aware blit) — named by what 0x600C accepts; kext
  changes are boot-class and the standing rules apply.
**Exposure:** stub changes live-swap via the guard; any kext
change is a separate, watched-boot-class deploy; probe-only
testing first.

**RUNG 39 RESULT (2026-08-22 evening) — THE WINDOW-TARGET
MACHINERY IN PLACE; the bounds query FAILED (1001) — the dims
didn't arrive, the proof stayed at 4x4 (still passing; no
regression). The dims source is now a one-print cross-check
away:**
- **The dims source READ (the float's window path, grf.t
  0x210a3-0x210e3):** `CGSGetSurfaceBounds(desc[0], desc[4],
  desc[8], &rect)` — width/height from the returned rect
  (doubles, cvtt'd). The stub mirrors the call via dlsym.
- **IMPLEMENTED (all in place):** attach calls the bounds query
  with the descriptor's ids; on success the target resource is
  recreated at WINDOW SIZE (with unref+recreate on resize); the
  clear submits a FRESH surface handle per batch (recreating an
  existing handle in vrend's object table is undefined); the
  proof's transfer runs at window size with corner+center
  probes.
- **RESULT: `CGSGetSurfaceBounds(0x908b, 0x0, 0x6f632e65) ->
  1001` — the query FAILED.** The descriptor's d[4]=0x0 this
  run (rung 35's dump had 0x17) — the field semantics (which of
  d[0]/d[4]/d[8] are cid/wid/sid, or whether the call needs the
  main connection rather than a descriptor id) are not yet
  pinned. The target stayed 4x4; the proof passed there —
  everything prior stands, nothing regressed.
- **NEXT (one print):** the probe prints its OWN cid (from
  CGSMainConnectionID), wid, sid (it has all three); the stub
  dumps the descriptor fully; the cross-check names which
  descriptor fields map to the call's args — then the bounds
  call succeeds, the window-sized target creates, and the proof
  generalizes (and the PRESENTATION half of the rung — the GA
  surface write — follows with the dims in hand).

**RUNG 39 CROSS-CHECK COMPLETE (2026-08-22) — THE MAPPING
NAMED, TWO REAL BUGS FIXED (one the lifetime contract's
read-side), the bounds query now succeeds — and returns an
EMPTY RECT: the geometry lives behind the GA bind:**
```
probe:  cid=0xaa4f-class  wid=0x18-class  sid=0x1bb38688-class
desc:   d[0]=cid  d[1]=wid  d[2]=sid      ← (cid,wid,sid) at DWORDS 0,1,2
rung39: CGSGetSurfaceBounds(cid,wid,sid) -> 0 rect=[0 0 0 0]
```
- **THE MAPPING:** the descriptor IS (cid, wid, sid) at dword
  indices 0,1,2 — my d[0]/d[4]/d[8] was an INDEX-vs-OFFSET
  confusion (the float's (%r13)/0x4(%r13)/0x8(%r13) reads
  BYTES). Fixed.
- **BUG 2 — THE LIFETIME RULE (cost one run):** the descriptor
  is ENGINE-OWNED SCRATCH, valid only DURING the call — my
  bounds query ran ~1s later (after CoreGraphics' dlopen) and
  read REUSED memory (0x0/0xf where the entry dump showed the
  true wid/sid). The stub now COPIES the triple at entry. The
  writability contract's read-side sibling: copy caller-owned
  inputs before any deferred use.
- **BUG 3 (sequence):** the probe ordered the surface AFTER
  drawing; reordered pre-draw. Bounds still empty.
- **THE RESULT: the query SUCCEEDS (r=0) with rect=[0 0 0 0]**
  — the correct triple, the surface ordered and flushed, and
  CGS reports no geometry. **READING (from the milestone-2
  finding): a raw CGS surface has no bounds until BOUND through
  the GA path (SetIDMode) — the same machinery that gave
  LockSurface its view backing. The window's geometry AND its
  presentation both live behind the GA bind.**
- **THE PROOF STANDS AT 4x4** (no regression; the round trip
  remains proven). The window-sized generalization is ONE WIRE
  away: the GA bind (SetIDMode with the saved triple) at
  attach, then the bounds query — the milestone-2 machinery,
  proven in the GA era, now the GLD's next integration.

---

## RUNG 40 — THE GA BIND WIRE (2026-08-22 night) — the chain
BUILT and working to the last call; the refusal DECODED
(unregistered surface); the registration door named at SELECTOR
granularity:

- **The GA plugin reinstalled** (the milestone-era bundle was
  absent from the guest; GA/VMQemuVGAGA.plugin shipped, 6KB;
  the FB's IOCFPlugInTypes property verified live — CFPlugins
  load on demand, no reboot needed).
- **THE WIRE, IMPLEMENTED IN THE STUB** (the milestone-2
  sequence, verbatim): setenv(VM_GA_PROBE) → matching
  VMVirtIOFramebuffer → IOCreatePlugInInterfaceForService(GA
  type) → QueryInterface → vtable deref → Probe → Start →
  AllocateSurface(kIOBlitHasCGSSurface, sid) → LockSurface.
- **RESULT: every step SUCCEEDS through Start — and
  AllocateSurface(sid) FAILS 0xe00002be** — kIOReturnUnsupported,
  THE SAME CODE the milestone-2 negative control returned for
  an unknown id. The kernel's cross-client surface registry
  (the 16-entry array, add-at-SetIDMode) does not contain our
  app-created surface: only WindowServer's surfaces enter it
  (its own client calls SetIDMode during compositing —
  confirmed: zero SetIDMode lines in the kernel log from the
  probe's run; all surface-client activity is WindowServer's
  boot-time compositing).
- **THE REGISTRATION DOOR, NAMED AT SELECTOR GRANULARITY:**
  the stub can register the surface ITSELF — open the surface
  user client (IOServiceOpen on the accelerator, the type-2
  class — VMAccelSurfaceClient), allocate its surface, then
  call **kIOAccelSurfaceSetIDMode — USER-side selector 0x83,
  two scalars (wID, modebits)** — which stores m_surface and
  publishes to the cross-client registry
  (VMAccelSurfaceClient.cpp:663-673). After that,
  AllocateSurface(our sid) resolves, LockSurface yields the
  view AND the dims, the target goes window-sized, and the
  SAME view is the presentation door (write + flush = the GA
  blit WindowServer composites).
- **The full sequence for the next rung, pre-registered:**
  type-2 open → surface allocate (the client's own row, enum
  kIOAccelSurface*) → SetIDMode(0x83, sid, modebits=0xA
  BGRA32) → GA AllocateSurface(sid) → LockSurface →
  window-sized target → the proof at window size → (the
  presentation write follows the same view).
- **Session state at close:** rungs 17-40 committed; two slots
  real and pixel-verified; the driver complete and honest end
  to end; the window target one registered surface away, with
  every door between here and visible pixels named.

---

## RUNG 41 — THE REGISTRATION (2026-08-22 night) — LANDED,
kernel-verified; one selector correction banked; the GA bind
advanced FROM unknown-id TO no-resources — a deeper, named wall:

- **THE SEQUENCE:** open the surface client (type 0 — the
  vm-accel-surface-gated class; the boot-arg is on) and call
  SetIDMode with (wID=sid, modebits=0xA=BGRA32).
- **CORRECTION (banked): the direct user-client call uses the
  TABLE INDEX, not the 0x8x worked-example numbering.** 0x83
  returned 0xe00002c7 with NO kernel line (never reached the
  handler). SetIDMode = eIOAccelSurfaceMethods index **7** —
  and index 7 returned **0x0** with the kernel-verified line:
```
VMAccelSurfaceClient: SetIDMode(wID=0x1bb32fd0 modebits=0xa depth=0xa bpp=4) -> STORED
```
  (The ":8x" comments in the kext's table are VMsvga2's era
  notes — a DIFFERENT table's numbering; the second
  worked-example-numbering error this arc, after the rung-12
  table.)
- **The GA bind then advanced:** from 0xe00002be (unknown id —
  the registry miss) to **0xe00002d8 (kIOReturnNoResources)** —
  PAST the lookup, inside the kext's 2D surface machinery. The
  kernel sequence: our client created+started, SetIDMode
  STORED, the GA plugin's 2D context started, GetConfig →
  {0,0} — then the resource refusal.
- **THE NEXT WIRE (named):** the milestone-2 flow sets the
  surface's SHAPE AND BACKING before locking
  (kIOAccelSurfaceSetShapeBacking — SetShapeBacking(options,
  fbIndex, IOAccelDeviceRegion) — the ":82" row, index 6);
  ours is REGISTERED BUT SHAPELESS — the 2D SetSurface's
  backing allocation is the no-resources site. The wire:
  SetShapeBacking with the window's region (from the bounds —
  which themselves come from the shape → the chicken-and-egg
  breaks via the CGS rect or the window's NSFrame passed at
  attach... the descriptor had +0x10=0x40=64-class values —
  the region may live THERE; the full descriptor dump from
  rung 39's cross-check holds it).
- **The proof stands at 4x4** throughout; every step of this
  rung is additive and logged.

---

## RUNG 42 — THE SHAPE WIRE (2026-08-22 night) — LANDED,
kernel-verified: the bind now reads the WINDOW'S TRUE
DIMENSIONS. Two gates banked (the empty-region no-op and the
IdentityScaleBit); the remaining row=0 + NoResources named
(the LockMemory read):

- **THE CALL:** setShape (index 9; 2 scalars (options,
  fbIndex) + the IOAccelDeviceRegion struct-in {u32 num_rects;
  i16 x,y,w,h} — IOAccelTypes.h:36, IOAccelSurfaceConnect.h:
  47-50). **The bounds source: CGSGetWindowBounds(cid, wid)**
  — windows have bounds even when their surfaces don't (the
  surface-bounds call needed the bind; the window-bounds call
  doesn't) → `[200 588 320 262]` (the probe's window, 320x262
  = 240 + the 22px title bar).
- **GATE 1 (banked): num_rects=0 is the NO-OP path** —
  setShape returns Success WITHOUT storing (the worked
  example's empty-region fixup). The stores need the region
  path; with num_rects=0 the stores still fire — but only
  under GATE 2.
- **GATE 2 (banked): the stores require
  kIOAccelSurfaceShapeIdentityScaleBit (0x4,
  IOAccelSurfaceConnect.h:141)** in options — options=0 left
  geometry untouched with a clean Success return (the second
  silent-success gate this arc; the kernel log's "no
  IdentityScaleBit: geometry untouched" line named it).
- **RESULT (kernel-verified):**
```
rung42: GetWindowBounds -> 0 [200 588 320 262]; SetShape(9) 320x262 -> 0x0
kernel: VMQemuVGA3DUserClient: SetSurface id=... opts=0x901 — BOUND (320x262 bpp=4 row=0)
```
  The bind reads the real window dims. **REMAINING: row=0
  (bytes_per_row still unset — computed at lock/backing time)
  and the GA lock's 0xe00002d8 (NoResources)** — the
  kVM2DLockMemory (selector 5) read names what it needs (the
  row computation and/or the backing allocation).
- **The registration-to-bind chain now stands:** type-0 open →
  SetIDMode(7) → SetShape(9, IdentityScale, window bounds) →
  the GA plugin → 2D SetSurface BOUND at window size. The
  lock, the view, and the presentation are the remaining
  doors, each one read away.

---

## RUNG 43 — THE WRITE-LOCK (2026-08-22 night) — LANDED,
kernel-verified; BOTH rung-42 remainders closed (row=0 and the
lock's NoResources); the window target LIVE; *** ROUND TRIP
PROVEN AT WINDOW SIZE ***:

- **THE CALL:** writeLockSurface — the surface client's TABLE
  INDEX 14 (the third consecutive index-from-disassembly win:
  7, 9, 14), scalar-free, StructO-only; the out struct is
  IOAccelSurfaceInformation with the address at +0 and
  (row, width, height) as u32 at +32/+36/+40.
- **RESULT (stub + kernel, same run):**
```
stub:    rung43: WriteLock(14) -> 0x0 addr=0x10592aca0 row=2080 320x262
kernel:  VMAccelSurfaceClient: WriteLock — backing ALLOCATED 1769472 bytes
         (extent 520x850 stride 2080), client 0x105800000 kernel 0xffffff8053ca0000
kernel:  VMQemuVGA3DUserClient: SetSurface id=262746307 opts=0x901 — BOUND
         (320x262 bpp=4 row=2080)
kernel:  VMQemuVGA3DUserClient: LockMemory — app view 105800000 row=2080
stub:    rung40: GA BOUND+LOCKED sid=0xfa930c3 -> 0x0 view=0x105800000 rowBytes=2080
```
  The lock LAZY-CREATES the backing (the rung-42 named
  requirement — the NoResources was exactly the absent
  backing); the 2D bind's row=0 resolved to 2080 BY the lock;
  and the GA bind's 0xe00002d8 fell to 0x0 with the VIEW
  MAPPED (0x105800000, rowBytes=2080).
- **THE WINDOW TARGET WENT LIVE FROM THE LOCK'S DIMS**
  (`./probe/gld_stub.c:252` — virgl_set_window_target(iu[1],
  iu[2]) on lock success): the fb resource is created AT THE
  WINDOW SIZE —
```
stub:    rung37: virgl ctx 257 OPEN (transport live)
stub:    rung38/39: fb res 257 created+backed+ctxAttached 320x262 (0x6009 -> 0x0)
stub:    0x600B wait -> 0x0; 0x3009 transfer_from (320x262) -> 0x0
stub:    backing[0..15]: bf8040ffbf8040ffbf8040ffbf8040ff
stub:    *** ROUND TRIP PROVEN AT WINDOW SIZE: corners+center == proof color ***
kernel:  v3d batch done size=76 ret=0x0 (x2)
kernel:  transferFromHost3D: Resource 257 pixels copied from host to guest
```
  The round trip generalized from the private 4x4 proof to
  the real drawable's dimensions, byte-exact at corners,
  center, and the last pixel.
- **THE EXTENT-VS-WINDOW STRIDE (load-bearing for the
  presentation write):** row=2080 is the EXTENT's stride
  (520x850 base extent, 2080=520*4), NOT the window's
  (320*4=1280). The lock's info carries the window dims but
  the extent's row; any write into view 0x105800000 must
  stride 2080 or rows tear.
- **CGSGetSurfaceBounds stays the empty rect** ([0 0 0 0]) —
  now IRRELEVANT: the lock's dims are the source of truth
  and the query is a closed detour (kept as fallback only).
- **The probe's own printed pixel (`f8 05 c0 5f`) and
  glGetError garbage are NOT a regression:** the ReadPixels
  slot voids all caller args by design
  (`./probe/gld_stub.c:487` — self-contained proof, never
  writes the caller's buffer); the probe's stack print was
  never a check. Attributed by reading the slot, not
  inferred.
- **Run provenance (the honest record):** the first
  verification run was lost twice — the guest rebooted
  (23:21, /tmp wiped) and the session compacted before the
  background task's output was read. What survived: the
  working tree (the rung-43 edit, uncommitted) and the
  deployed stub (digest eaf71aa5d719700ca863095ee59af2a5,
  host build == guest install — the deploy had completed).
  The run above is the REPRODUCTION: probe re-shipped,
  root-owned stub log cleared, full chain re-fired — same
  shape, fresh addresses (view 0x105800000 vs the lost run's
  0x105700000 — different boot, same sizes).
- **THE CHAIN, COMPLETE END TO END:** gldAttachDrawable's
  descriptor (cid,wid,sid) → type-0 surface-client open →
  SetIDMode(7) → SetShape(9, IdentityScale, window bounds) →
  WriteLock(14) [backing created, dims live] → GA
  AllocateSurface/LockSurface [view mapped] → virgl fb
  resource at window size → host-rendered, byte-verified
  content IN THE GUEST. **Remaining to visible pixels: the
  PRESENTATION WRITE — copy the readback into view
  0x105800000 at row 2080 (the flush path), and WindowServer
  composites it. The door is open; the write is the next
  rung.**

---

## RUNG 44 PRE-REGISTERED — THE PRESENTATION WRITE: the proof
color ON SCREEN (committed before implementation)

**The read that changed the design:** the presentation door
already EXISTS in the kernel — `0x600C hostRelayBlit`
(`./FB/VMVirtIOGPU.cpp:8037`, the relay era's proven present,
2026-08-20). Its GA branch does the whole presentation kernel-
side: synchronous transferFromHost3D of the resource →
row-copy into the GA-BOUND SURFACE's backing (stride =
bytes_per_row = 2080 — the extent-stride rule already
honored there) → `vmSurfaceFlushToFramebuffer` (surface →
desktop backing at the surface's LIVE shape rect) →
transferToHost2D + flushResource (the rect to the host
scanout). A userspace copy into the mapped view would be a
second, parallel presentation path — NOT taken; the kernel
door is the single path, already built and exercised by the
Mesa winsys era.

**The gates 0x600C checks (all per the CALLING client —
`m_user_geom` recorded at 0x6002, backing at 0x6003; all
satisfied by the rung-43 chain on g_virgl_conn):**
userResourceFmt/Dims (geometry table), userResourceCtx
(`ctx=0 silently does the wrong thing — recorded law`),
findUserBacking. The GA-bound id is GLOBAL
(vmSurfaceRegistrySetGABound at SetSurface — ours set at the
0x901 bind). Contract: the caller fence-waits first — the
proof's 0x600B wait stands right before the call.

**The change (one call):** in gld_readpixels_real, after the
proof verdict — 0x600C with scalars {res, 0, 0, w, h} (the
GA branch uses w,h for the row copy; x,y are the fallback
path's). Additive; the proof's own lines unchanged.

**Predictions:**
- (i) **THE ON-SCREEN PROOF, in-window:** uniform medium
  blue ≈ RGB(64,128,191) — the clear color (R,G,B)=(.25,.5,
  .75); the readback bytes are BGRA-ordered (B=.75=BF first,
  rung 38's observation) into kIO32BGRAPixelFormat, so the
  displayed color equals the clear color either way the
  byte-labels read. Kernel logs `hostRelayBlit: GA path —
  surface ... flush rect 320x262@200,588`. WindowServer's
  title bar stays drawn over the top 22px (the surface spans
  the full window).
- (ii) **ON-SCREEN BUT MISPLACED:** the blue rect appears on
  the desktop OUTSIDE the window — the CGS-origin artifact
  (CGSGetWindowBounds y=588 read as top-left when CGS counts
  bottom-up; window truly at top-left y≈230 on 1080). The
  GA-path kernel line still logs. STILL the presentation
  write proven (pixels through the whole chain); the fix is
  named by which corner the rect lands in: a y-flip at
  SetShape (y_tl = display_h − y_cgs − h).
- (iii) **NOTHING VISIBLE:** 0x600C nonzero — the codes
  discriminate: Unsupported = not in geometry table / no
  recorded ctx; NotReady = no attached backing / desktop
  backing missing. Or Success with no rect — the shape
  off-FB branch (cannot fire: 200+320≤1920, 588+262≤1080).
- (iv) The proof lines fire exactly as rung 43 (window-size
  round trip stands; the relay is additive after).

**Instruments:** the stub log (the 0x600C return), the
kernel log (the GA-path line, capped at 8), the SCREEN
(user visual — the first pixels-on-screen class since the
readfb baseline; screenshot optional corroboration).

---

## RUNG 44 RESULT + CONTINUATION — THE RECT IS ON SCREEN,
BLACK: geometry and push VERIFIED end to end; the content
diverged to zeros inside the relay. The next discriminator
pre-registered (the mapped-view read):

**First run (23:46:36, pre-reboot): the relay clean to the
flush, the final push hit the sporadic timeout class:**
```
stub:   rung44: 0x600C hostRelayBlit res=258 320x262 -> 0x0
kernel: hostRelayBlit: GA path — surface 263158340 (320x262 row 2080),
        flush rect 320x262@200,588
kernel: VMVirtIOGPU::transferToHost2D: Command failed: 0xe00002d6
```
  0xe00002d6 = **kIOReturnTimeout** (MacKernelSDK
  IOReturn.h:123 — 0x2d5 Busy, 0x2d6 Timeout, 0x2d7 Offline;
  two earlier labels corrected: 0x2be = NoResources (rung 40
  called it Unsupported), 0x2d8 = NotReady (rung 41 called it
  NoResources; NotReady fits that wall BETTER — "past the
  lookup, backing absent"). The timeout class is the
  display's own sporadic one: 117 failures since 23:29:56
  against ~48k transfers in the same span (~0.2%; refresh
  windows show 149/156 then 469/469 succeeding) — not a
  regression, a retry.

**Second run (08:26:13, post-reboot): the push SUCCEEDED —
and the screen answered:**
```
stub:   *** ROUND TRIP PROVEN AT WINDOW SIZE *** (bytes bf8040ff — blue in the guest buffer, seconds before)
stub:   rung44: 0x600C hostRelayBlit res=256 320x262 -> 0x0
kernel: transferFromHost3D: Resource 256 pixels copied (x2 — the proof's + the relay's)
kernel: hostRelayBlit: GA path — surface 264219650, flush rect 320x262@200,588
        (NO transferToHost2D failure — the rect reached the host scanout)
screen: A BLACK RECTANGLE appeared (user visual, the exact
        observation: "BLACK rectangle was")
```
- **WHAT IS VERIFIED:** the presentation geometry chain,
  whole — 0x600C → the GA-bound surface → the shape rect →
  the desktop backing → transferToHost2D + flush → the HOST
  SCANOUT. A rect appeared ON SCREEN through the driver's
  own present path, sized and positioned by the flush. The
  position was NOT flagged wrong by the observation (a
  misplaced-vs-inwindow adjudication needs a live window —
  the window had closed; the next run holds it open).
- **WHAT BROKE: the CONTENT — blue in the guest backing
  (byte-verified the same second) presented as BLACK.** The
  divergence is inside hostRelayBlit's userspace-blind
  steps. NAMED SUSPECT: step 2's silent failure path —
  `if (got != row_bytes) break;` (readBytes) and the
  writeBytes counterpart LOG NOTHING; an untouched
  freshly-allocated surface backing (zeros) flushed to the
  desktop = exactly a black rect.
- **THE DISCRIMINATOR (pre-registered, zero kext changes):
  the surface backing is mapped in OUR process at
  g_ga_view.** Read it after the 0x600C call:
  - (A) view rows ZERO → the surface write never happened
    (readBytes/writeBytes failing silently on the
    user-attached descriptor — likely the prepare/lifetime
    contract; fix lands in the kext with a LOG LINE on that
    break first).
  - (B) view rows BLUE (bf 80 40 ff at row starts, stride
    2080) → steps 1-2 fine; the divergence is the flush's
    source (s->kernel_map vs backing_memory — two different
    allocations?) — fix in the kext's flush source.
  - (C) view blue AND screen blue — falsified already by
    the black observation.

**THE DISCRIMINATOR RAN (08:29:57, deploy e18af822): outcome
(B) — the surface IS blue:**
```
rung44: 0x600C hostRelayBlit res=257 320x262 -> 0x0
rung44: VIEW after relay (stride 2080) r0:bf8040ffbf8040ff
        r1:bf8040ffbf8040ff r261:bf8040ffbf8040ff
```
  Steps 1-2 exonerated (rows 0, 1, AND 261 byte-exact from
  the mapping's base). The divergence is DOWNSTREAM of the
  surface write.

**ROOT CAUSE (read from the code, all three formulas):**
the surface allocation has TWO coordinate systems and the
relay used the wrong one.
- The write-lock's handout: `info->address = base +
  shape_y*bytes_per_row + shape_x*bpp`
  (`./FB/VMAccelSurfaceClient.cpp:1095-1115`) — window
  pixels live at the SHAPE OFFSET inside the allocation.
  The 520x850 base extent exists so any shape rect fits:
  (200,588)+(320,262) = exactly the extent corner.
- The relay's write: `writeBytes(j*surf_stride, ...)` —
  row j from the BASE (`./FB/VMVirtIOGPU.cpp:3867`; its
  comment "surface-space, the surface IS the window's" is
  FALSIFIED — that is not where the surface's consumers
  read).
- The flush's read: `src + (shape_y+r)*stride +
  shape_x*bpp` (`./FB/VMAccelSurfaceClient.cpp:625`) —
  rows 588..849, which the relay never wrote: ZEROS.
  Blue at base rows 0..261 (verified by the view read),
  zeros flushed from rows 588..849 → THE BLACK RECT. Every
  observation explained; no residuals.

---

## RUNG 45 PRE-REGISTERED — THE ONE-LINE KEXT FIX: the
relay writes at the SHAPE OFFSET (committed before
implementation)

**The change (hostRelayBlit step 2, `./FB/VMVirtIOGPU.cpp`):
write at `shape_y*stride + shape_x*4 + j*stride` instead of
`j*stride`; the bounds check grows the same offset.** The
flush's read formula is then satisfied exactly. No new
kernel API calls (offset arithmetic only — not the
boot-risk class; recovery via slclean if it somehow fails
to boot).

**The stub gains one discriminator row:** read view row 588
(the shape row) in addition to 0/1/261 — after the fix the
blue should MOVE there (r0 goes dark, r588 blue), proving
the fix landed kernel-side even before the screen speaks.

**Predictions:**
- (i) **BLUE IN THE WINDOW:** the probe's window (live
  during the hold) fills with medium blue RGB(64,128,191)
  — CGS bounds [200 588 320 262] are desktop top-left
  coordinates (the shape rect is "the window's live desktop
  position" by the flush's own design comment), so the rect
  lands exactly on the window. THE PRESENTATION COMPLETE:
  GL call → host GPU → guest → window surface → desktop →
  scanout, VISIBLE.
- (ii) **BLUE MISPLACED** (below the window): the CGS
  origin is bottom-left after all; the fix is a y-flip at
  SetShape time (y_tl = display_h − y_cgs − h), named now.
- (iii) **STILL BLACK:** view r588 blue but screen black →
  the divergence moves to the flush's dst or the push
  (m_hblit_dst_res vs the scanout resource); the kernel log
  + view rows name which.
- (iv) View reads after the fix: r0 ZEROS, r588 BLUE (the
  write moved), proof lines unchanged.

**Exposure:** kext rebuild + cache + reboot (the full
install cycle); stub live-swap; probe re-ship (post-reboot
/tmp wipe).

---

## RUNG 45 RESULT — *** BLUE IN THE WINDOW *** — THE
PRESENTATION COMPLETE: a GL call rendered by the HOST GPU,
presented through the driver's own chain, VISIBLE ON SCREEN
(user-confirmed). One instrumentation error made and
corrected on the way:

- **THE FIX LANDED (09:06 run, kext 05b5b83a):** the relay
  writes at the shape offset; the flush reads exactly
  there; the push succeeded (no 0x2d6 at the timestamp);
  the probe's window filled with medium blue RGB(64,128,191)
  — the clear color, byte-for-byte the proof color.
  **Prediction (i) landed; (ii) falsified** — CGS bounds
  [200 588 320 262] are desktop top-left coordinates (blue
  appeared IN the window, not 358px below).
- **BYTE-PRECISE CONFIRMATION (09:07 run, stub 8ace386d):**
```
rung45: VIEW at shape_off (r588+800): bf8040ffbf8040ffbf8040ffbf8040ff
```
  The proof color sits at EXACTLY 588*2080 + 200*4 in the
  stub's mapping of the surface backing — the shape
  offset, where the flush reads.
- **INSTRUMENTATION ERROR, corrected in-session:** the
  first two discriminator reads (rows 0/1/261/588 and the
  32-row sweep) probed bytes 0..3 of each row —
  STRUCTURALLY blind to a window starting at column 200
  (byte 800). Their "ALL ZERO" was the probe's error, not
  the relay's; the precise read at +800 settles it. Lesson
  recorded: a discriminator must probe the WRITE FORMULA's
  exact bytes, not a row's head.
- **RESIDUAL (unexplained, bounded):** the 08:53 run —
  same kext as the 09:06 success — read "Nothing" on the
  screen. Same flush rect, same push success, same proof.
  Leading hypothesis (NOT verified): the look happened
  after the window closed and WindowServer's removal
  composite overwrote the desktop rect. Not reproducible
  retroactively; if a later run shows the same variance,
  the discriminator is a held-open window with a timed
  look.
- **THE COMPLETE CHAIN, VERIFIED END TO END (2026-08-23
  09:06-09:07):** probe's glClear/glReadPixels → the GLD
  dispatch table's REAL slots → the stub's virgl transport
  → the window-sized resource (320x262) → THE HOST GPU
  EXECUTING THE CLEAR → byte-exact readback in the guest →
  0x600C relay (kernel re-read → row-copy at the SHAPE
  OFFSET into the window's surface backing → desktop flush
  at the live shape rect → scanout push) → **PIXELS ON
  SCREEN.**
- **The honest boundary:** the CONTENT is still the proof
  color — the stub ignores the app's own clear color (the
  probe asked for green) and the swap slot (CGLFlushDrawable
  → GLD) is still a noop; the presentation fires from the
  ReadPixels proof, not from the app's flush. The next
  wires, in order: (a) the swap slot real (present at
  CGLFlushDrawable — where apps expect it); (b) the app's
  own clear color through (gldClear's color args → the
  submitted command); (c) the remaining 20 noop slots →
  Mesa-backed, slot by slot.

---

## RUNG 46 PRE-REGISTERED — THE SWAP SLOT: present at
CGLFlushDrawable (committed before implementation)

**The reads (GLEngine disassembly, /tmp/gle.bin — the
swap's whole path):**
- **gliSwapBuffers (0x15dd09):** gates `[ctx+0x6540]==0 &&
  [ctx+0x798c]!=0` → `jmp *[ctx+0x66b0]([ctx+0x65b8])` —
  the DRIVER's swap, called with the DRIVER ctx. No table
  slot, no dispatch core — a direct per-context entry.
- **[0x798c]'s source (0x23bc):** a copy of the per-renderer
  SUB-BLOCK's byte at +0x2d — a driver-declared capability.
  The sub-block pointer is the 4th argument (rcx) of the
  engine's SIX-arg gldCreateContext call (0x180c: `leaq
  0x79b8(%rsi,%rax), %rcx`), stored by the engine as
  [ctx+0x65c0]. The stub's 2-arg mirror ignored it → byte
  0 → the swap call silently skipped → CGLFlushDrawable's
  vacuous 0.
- **The install entry (0x15d132):** the engine calls
  DISPATCH TABLE SLOT +0x50 as `(driver_ctx, &engine
  [0x65c8], &engine[0x66d0])` — the driver fills the first
  block with its entries: +0xE0 = flush (0x66a8), +0xE8 =
  swap (0x66b0); 0x66b8 (+0xF0) fence-class (0xe2b1c's
  reader). The stub's kSlots NEVER included +0x50 (rung 34
  mirrored the float's writes only to 0x14f73, before the
  0x14f8a+ stores) — a rung-34 gap, corrected here.
- Corroboration the float fills +0x50: grf's InitDispatch
  stores at 0x14f8a-0x14f9e include `movq %rax, 0x50(%rsi)`
  from the shared's +0x740 block.

**The changes (stub only — live-swap, no kext, no boot):**
1. gldCreateContext: write `[a4+0x2d] = 1` (a4 = rcx = the
   sub-block), log the pointer and a readback.
2. gldInitDispatch: fill slot +0x50 with the install entry
   (gld_fill_engine_calls); everything else unchanged.
3. gld_fill_engine_calls(dctx, block1, block2): write
   block1+0xE0 = flush entry (log-only), block1+0xE8 = the
   SWAP entry; log; return 0.
4. THE SWAP ENTRY: log "rung46: SWAP", then run the
   presentation exactly as the proof does — ensure_fb →
   submit clear (mask 4) → 0x600B wait → 0x600C relay.

**Predictions:**
- (i) **THE SWAP FIRES AT CGLFlushDrawable:** the install
  entry logs at attach time; "rung46: SWAP" logs BETWEEN
  the probe's glReadPixels and teardown (timestamp order =
  the discriminator); the kernel logs a second GA-path
  line; the window re-blues at flush.
- (ii) **The install entry never called:** the call site's
  gates ([0x7988], [0x6540/0x6548]-derived) skip it in our
  configuration → block1 never filled → swap never fires →
  CGLFlushDrawable stays vacuous. The fallback wire then
  named: find the engine-ctx pointer another way (the
  attach path's stack) or satisfy the gate.
- (iii) **Crash or wrong-block write:** the +0x2d write
  lands somewhere unexpected (arg misread) — recovery is a
  stub redeploy (live-swap, minutes); the a4 log line
  adjudicates.
- (iv) Everything else unchanged: the proof chain's lines
  identical (the swap is additive).

**Exposure:** stub live-swap via the deploy guard; probe
rerun; no kext; no reboot.

---

## RUNG 46 RESULT — PREDICTION (ii): the install entry
never called; the ROOT decoded — the swap door requires
the HARDWARE renderer claim (the 0x100 honesty gate, now
unblocked). One piece of the door landed:

- **LANDED: the capability byte.** `rung46: sub-block
  0x1018211d8 [+0x2d]=1 (readback 0x1)` — the 4th-arg
  identification was right; the byte the engine's 0x23bc
  reads is now 1. (Also banked: 0x280e's reader derives a
  GL error code from [0x798c] — 0x405/0x404,
  swap-not-supported class — confirming the byte's role.)
- **NOT FIRED: the +0x50 install entry — and the missing
  writer FOUND.** `_gliUpdateDispatchState` (0x15d03c,
  x86_64): with `[0x7987]==0` OR `[0x6570]==[0x6540]=
  [0x6548]==0`, the context is SOFTWARE → `[0x7988]=1` →
  the engine fills the ENTIRE [0x65c8..0x66c8] block with
  **33 `gliDispatchNoop`s** (the 0x15d16f loop — the
  writer every earlier grep missed) and SKIPS the
  [0x6758] (+0x50) call. CGLFlushDrawable then noops
  cleanly — exactly what we observe.
- **WHY our context reads software: the renderer claim.**
  gldGetRendererInfo fills SOFTWARE caps (the rung-17
  honesty position); the probe's own census prints
  `accelerated=0`. The hardware-class path (install entry
  called, block filled from the driver, swap callable)
  requires the renderer claimed HARDWARE — the **0x100
  bit**, which the pre-bridge design decisions deliberately
  held back "until Mesa backs it." The transport, the
  readback, and the on-screen presentation now back it.
- **THE NEXT WIRE (named, one change):** flip the renderer
  claim to hardware (the flags/renderer-info 0x100 bit —
  the rung-26/27 flags word and the renderer-info record)
  → [0x7987]/[0x6570]-class gates take the hardware path →
  gliUpdateDispatchState calls our +0x50 entry (already
  installed in the table) → the block fills with our
  flush/swap → [0x798c]=1 (already written) →
  CGLFlushDrawable tail-calls OUR SWAP (already written).
  All four pieces are in place; the claim is the trigger.
  **Exclusivity caveat (the pre-bridge decision):** the
  hardware claim makes the renderer eligible system-wide —
  the flip must be watched for WindowServer adoption (the
  rung-8/12-era risk); the probe-first, desktop-second
  order stands.
- **The disassembly-method correction (recorded):** the
  earlier mixed-arch reads — `otool -tV` on the fat binary
  dumps ALL archs sequentially; line-window reads straddled
  them. All rung-46 facts re-verified with
  `otool -arch x86_64` (gliSwapBuffers's tail-jmp, the
  0x23bc setter, the 0x15d132 site inside
  gliUpdateDispatchState). /tmp/gle64.t is the x86_64-only
  artifact (dies with /tmp).

---

## RUNG 47 PRE-REGISTERED — THE CLAIM FLIP: the 0x100
hardware bit (committed before implementation)

**The justification for lifting the honesty hold (the
pre-bridge design decision's own condition):** "the 0x100
hardware bit stays unset until Mesa backs it." Backing now
exists — the transport (Increment C, the fence era), the
readback (byte-exact at window size), and the on-screen
presentation (rung 45). The claim would no longer be a
claim; it would be a description.

**The values, from the worked example's own experiments
(`~/VMsvga2-modern/GLD/VMsvga2GLDriver.c:122-176`, the
#if-0 blocks — the author's recorded working overrides):**
- pf object +0xc flags: **0x501** = 0x400 | **0x100** | 0x1
  — the 0x100 bit is the hardware claim (rung 26's decode:
  attr 73 → 0x100), 0x1 the scorer's positive requirement.
- renderer record +0xc class: **0x17CD** — ours is 0x6CD
  (software, per the float); the 0x1000 bit is the
  hardware-class difference.

**The change (stub only, two one-liners):**
1. gldChoosePixelFormat: `case 73: flags |= 0x100` —
   attr 73 (kCGLPFAAccelerated) claims hardware, exactly
   when the app asks for it (the float's own conditional
   shape; plain requests keep the software-honest object).
2. gldGetRendererInfo: `r[3] = 0x17CD` — the record's
   class field flips to hardware.

**Boot-safety (structural):** the live swap changes the
bundle on disk; WindowServer holds the OLD software-
claiming stub in memory until reboot — the desktop cannot
adopt the claim mid-session. The exclusivity risk
materializes only on the NEXT BOOT; the probe-first order
is free.

**Predictions:**
- (i) **THE FULL PATH:** the probe's accelerated request
  {73,5} matches (npix=1 — first time ever); the probe
  uses it; the engine classifies the context hardware;
  gliUpdateDispatchState calls our +0x50 install entry
  ("block1 filled"); CGLFlushDrawable tail-calls the SWAP
  ("rung46: SWAP fired"); the kernel logs the relay; the
  window blues AT THE FLUSH. Desktop stable.
- (ii) **The pf matches but classification stays
  software** (the [0x6570]/[0x6548]-class engine objects
  still absent — the hardware path may need more than the
  claim): install entry still uncalled; next wire named =
  the objects those fields hold (likely filled via the
  table's create-path or the sub-block).
- (iii) **WindowServer adoption at the NEXT boot** (the
  exclusivity tripwire): if the desktop boots on the
  hardware-claiming renderer and any refusal crashes it —
  recovery is slclean + the stub swap back (documented
  path).
- (iv) **npix still 0 for {73,5}** (the scorer exact-tests
  flags, not requirement-matches): the scorer's test read
  is the next step.

**Exposure:** stub live-swap; probe rerun; no kext; no
reboot this rung.

---

## RUNG 47 RESULT — TWO OF THREE LANDED: the renderer claim
(census accelerated=1) and the FIRST ACCELERATED PIXEL
FORMAT (npix=1); the classification still software — and
its trigger found: a SUCCESSFUL ATTACH through dispatch
slot +0x48. Rung 48 pre-registered:

- **LANDED: the record claim.** `renderer[0]:
  accelerated=1 rendererID=0x1af60100` — the census reports
  the renderer accelerated (record class 0x17CD), the first
  time in the arc.
- **CORRECTION (banked): the engine STRIPS attr 73 before
  forwarding.** The {73,5} request arrives at
  gldChoosePixelFormat as `[0x5 0x4]` — the accelerated
  criterion filters RENDERERS engine-side (via the census);
  the driver never sees 73. The attr-conditional claim
  never fired. **The claim is UNCONDITIONAL** (flags |=
  0x100 on every object — the worked example's own shape,
  VMsvga2GLDriver.c:169: p[1]=0x501, no conditional; and
  the scorer is requirement-based, rung 26 — extra bits
  harmless, confirmed: plain requests still match).
  **Result: `CGLChoosePixelFormat(accelerated) -> npix=1` —
  the first accelerated pixel format in the arc; the probe
  uses it.**
- **NOT LANDED: the classification — install entry still
  uncalled, swap still noop.** Root decoded (below).
- **gldSetConfigData DISCOVERED (the config-block map read
  end to end):** the float fills the engine's per-renderer
  config block (the sub-block rcx of the 6-arg
  gldCreateContext) via its own gldSetConfigData
  (grf64 0x139ae): +0xC/+0x3F800000/+0x4000-maxes...;
  **+0x2c/+0x2d = the DRIVER CTX's own bytes at +0x268/
  +0x269** (the swap capability's provenance); color-size
  bytes; caps words +0x198/+0x19c/+0x1a0; limits to
  +0x1a4+. The stub refuses the entry and the block sat
  all-zero except our +0x2d=1. Not this rung's gate (the
  block feeds limits/describes), but the map is banked for
  the limits work.
- **THE CLASSIFICATION TRIGGER (the decisive read):
  `gliAttachDrawableWithOptions`** (0xf30x+): calls the
  driver through **dispatch table slot +0x48**
  (0xf444: `callq *0x168(%r8)` — driver-obj+0x168 =
  table+0x48) with the drawable, **CHECKS THE RESULT**
  (0xf44d+: cmpl $3/$2/$1-class parse); on success
  (`xorl %r12d`) → `[0x6570] = r15` (the drawable object,
  NONZERO) → `gliUpdateDispatchState` → al=0 → **HARDWARE
  classification** → the +0x50 install → the block filled.
  Our table's +0x48 is a NOOP (void garbage return) → the
  result check fails → r12d stays the 0x2714 default →
  software. The export-level gldAttachDrawable (which
  fires, returns 0) is a DIFFERENT call site.

## RUNG 48 PRE-REGISTERED — THE ATTACH SLOT: table +0x48 =
the real gldAttachDrawable (committed before
implementation)

**The change (one line):** gldInitDispatch writes
gldAttachDrawable to dispatch+0x48 (alongside clear +0x8,
readpixels +0x10, install +0x50). The entry already returns
0; if the table call's ARG ORDER differs from the entry
call (logged), the log names it and the signature is
adjusted next run.

**Predictions:**
- (i) **THE FULL CHAIN:** the attach result passes the
  engine's parse → [0x6570] nonzero → gliUpdateDispatchState
  flips hardware → "+0x50 install CALLED, block1 filled" →
  CGLFlushDrawable tail-calls the SWAP → the relay presents
  AT THE APP'S FLUSH — the presentation where apps expect
  it. Desktop stable (the claim is now set — the
  exclusivity watch applies at the NEXT BOOT, not now).
- (ii) The table call's args differ (the entry signature
  misfits) → a crash or a garbage type logged at the slot
  call → the float's table+0x48 source function read next
  (the shared's +0x740 block's slot order).
- (iii) The result checks still refuse (the float's attach
  returns something other than plain 0 through the table —
  e.g. a type code) → the checks' comparisons name the
  expected convention.

**Exposure:** stub live-swap; probe rerun; no kext; no
reboot.

---

## RUNG 48 RESULT + CONTINUATION — the attach slot IS the
export's call site (proven by return-address arithmetic);
the install fired at the WRONG MOMENT — before the table
existed. The direct install pre-registered:

- **PROVEN: the export fires through the engine's attach
  path.** The logged return address 0x1053e544b −
  GLEngine-base 0x1052f6000 = **0xf44b = 0xf444+7** (the
  REX-prefixed `callq *0x168(%r8)`) — our gldAttachDrawable
  IS the table+0x48 entry, called from
  gliAttachDrawableWithOptions. r15 = the descriptor
  (nonzero); every success path ends at `[0x6570]=r15` →
  gliUpdateDispatchState.
- **THE TIMING BUG (why the install never fired):** the
  attach (and thus the classification transition) runs
  BEFORE gldInitDispatch fills the table — the log order is
  attach(:09) THEN InitDispatch(:09). The engine's
  transition-time [0x6758] call hit a pre-fill table, and
  the transition never repeats ([0x7988] already 0; later
  updates take the `testb %cl; je return` early-out).
  gldInitDispatch fills the ENGINE's own table region
  directly (rsi = engine ctx+0x6708) — post-fill, the
  entries are right, but nothing calls them.
- **THE DIRECT INSTALL (pre-registered, stub-side):** the
  sub-block pointer (gldCreateContext's a4) =
  engine+0x79b8+idx*0x888 with idx=0 (single renderer) →
  **engine base = a4 − 0x79b8**, and block1 =
  engine+0x65c8. At the END of gldInitDispatch (table now
  filled), write block1+0xE0 = flush entry, block1+0xE8 =
  swap entry. Sixteen bytes, two pointers, no engine
  cooperation.

**Predictions:**
- (i) **THE SWAP FIRES AT CGLFlushDrawable** ([0x798c]=1
  already written, [0x6540]==0 no FBO): "rung46: SWAP
  fired" logs between ReadPixels and teardown; the relay
  presents at the app's flush; the window blues.
- (ii) **Crash/miswrite** (the base arithmetic wrong —
  idx≠0 or the sub-block relation misread): the process
  dies at flush; recovery = redeploy the previous stub
  (minutes, no boot).
- (iii) **Still noop** (the engine re-nooped block1 after
  our write — a later gliUpdateDispatchState with [0x7988]
  back to 1): the noop-block writer's trigger read is the
  next step.

**Exposure:** stub live-swap; probe rerun; no kext; no
reboot.

---

## RUNG 48 RESULT — *** THE SWAP AT THE APP'S OWN FLUSH ***
— prediction (i) complete: the direct install landed and
CGLFlushDrawable tail-called the driver's swap:

```
stub:   rung48: DIRECT INSTALL engine=0x100848c20 block1=0x10084f1e8
stub:   rung46: SWAP fired (engine [0x66b0]) — presenting
stub:   rung46: SWAP presented (wait 0x0, relay 0x0) 320x262
kernel: hostRelayBlit: GA path — surface 223039688, flush rect 320x262@200,588
```

- **THE COMPLETE APP DRAW CYCLE, DRIVER-ROUTED END TO
  END:** glClear (the clear slot → the virgl batch → the
  host GPU) → CGLFlushDrawable ([engine+0x66b0] → the
  driver's swap → fence-wait → the 0x600C relay → surface
  write → desktop flush → scanout). The base arithmetic
  was right (engine=0x100848c20 — sane, no crash); the
  two-pointer write put the swap where the engine jumps.
- **Visual corroboration not re-taken this run** (the
  window closed at probe exit); the relay chain is the
  same one visually verified in rung 45 — the same
  bytes through the same doors, now triggered at flush.
  A held-open window with eyes on it remains the
  standing confirm for any future claim-writing.
- **THE HONESTY BOUNDARY, MOVED:** the 0x100 hardware
  claim is now SET in the shipped stub (unconditional pf
  flags, record class 0x17CD) AND backed — accelerated
  pixel formats match, the census reports accelerated=1,
  and the swap presents through the proven chain. What
  remains honest-refusal: the other ~20 GL entries
  (noop dispatches), the app's own clear COLOR (still
  the proof color), and the config block (limits) still
  unfilled (gldSetConfigData's map banked for it).

---

## RUNG 49 PRE-REGISTERED — THE APP'S OWN CLEAR COLOR
(committed before implementation)

**The read (the float's gldClear, grf64 0x15087):** the
clear call takes (ctx, mask) ONLY — no color args. The
color comes from `r13 = [ctx+0x740]` — the shared/processor
block (gldCreateContext's 5th arg, which the rung-30 mirror
already stores at ctx+0x740) — and gldClearDrawBuffer reads
the four floats at **shared+0x2ea0/+0x2ea4/+0x2ea8/+0x2eac**
(grf64 0x12b91-0x12ba9: R,G,B,A into xmm6/7/5/8). The
ENGINE owns glClearColor state and mirrors it there; the
driver reads it at clear time.

**The change:** gld_clear_real (and the swap/proof paths)
read the four floats from [ctx+0x740]+0x2ea0 and submit
THEM in the CLEAR blob (the four color dwords), replacing
the fixed proof floats. The readback proof's expected bytes
become COMPUTED from the same floats (BGRA order: B,G,R,A
bytes). The color is logged at each clear.

**Predictions:**
- (i) **GREEN AT THE FLUSH — THE APP'S OWN COLOR:** the
  probe calls glClearColor(0,1,0,1); the stub logs
  `clear color 0.000000 1.000000 0.000000 1.000000`; the
  proof passes with expected bytes 00 FF 00 FF; the window
  fills GREEN at CGLFlushDrawable — the first time the
  DISPLAYED color is the APPLICATION'S choice.
- (ii) The floats read wrong (offset misread or the engine
  doesn't maintain +0x2ea0 for our contexts): the logged
  color is 0/0/0/0 or stale; the window shows the wrong
  color; the engine's glClearColor path (its store site)
  is the next read.
- (iii) The floats read right but the proof fails on the
  computed-expected change: an instrumentation bug in the
  byte-order computation — caught by the log before any
  conclusion.

**Exposure:** stub live-swap; probe rerun; no kext; no
reboot. Visual check requested (green is unmistakable vs
the blue proof).

---

## RUNG 49 RESULT — *** GREEN — THE APP'S OWN COLOR ON
SCREEN *** — prediction (i) complete, all verification
classes green:

```
stub: CLEAR-REAL #1 mask=0x4000 color 0.000000 1.000000 0.000000 1.000000 (rung 49)
stub: backing[0..15]: 00ff00ff00ff00ff00ff00ff00ff00ff
stub: *** ROUND TRIP PROVEN *** (computed expected — the same floats the clear submitted)
stub: rung46: SWAP fired ... presented (wait 0x0, relay 0x0) 320x262
screen: GREEN (user visual, both runs — "green")
```

- **THE COLOR PATH, COMPLETE:** the app's glClearColor(0,1,0,1)
  → the ENGINE's state mirror at shared+0x2ea0 (the float's
  own read site, grf64 0x12b91) → the driver's clear reads
  it → the virgl CLEAR blob carries the app's float bits →
  the host GPU → byte-exact readback (00 ff 00 ff — green
  in the B,G,R,A transfer layout) → the swap presents at
  the app's flush → **the displayed color is the
  APPLICATION'S CHOICE — the first time in the arc.**
- The proof's expected bytes are now COMPUTED from the
  same floats (clamped, rounded, B/G/R/A order) — the
  fixed kObserved is retired; the proof generalizes to
  any color.
- **The honest-refusal list shrinks again:** the remaining
  gaps are the ~20 noop GL entries and the unfilled
  limits/config block (gldSetConfigData's map). The draw
  cycle itself — clear color, clear, flush, presentation —
  is the app's own, end to end.

---

## RUNG 50 PRE-REGISTERED — THE CONFIG BLOCK: fill
gldSetConfigData's map (committed before implementation)

**The BEFORE measurement (instrumentation, old stub +
limits-census probe):** `LIMITS: tex=0 vp=0x0 bits r0 g0
b0 a0 d0 s0` — the engine answers EVERY limits query with
ZERO. A real GL app (they query limits at startup) would
see max-texture-size 0 — a broken GL. The config block
(all-zero except our +0x2d) is the presumptive source.

**The fill (at gldCreateContext, extending the rung-46
write):** the float's gldSetConfigData map (grf64 0x139ae,
read end to end in rung 47): dwords +0xC/+0x3F800000/
+0x4000/+0x4000/+1/+1; bytes +0x18=0xA/+0x19=8/+0x1A=8/
+0x1B=0/+0x1C=0xC; the color bytes (+0x31=1, +0x32=8,
+0x34..+0x37=8/8/8/8 — BGRA8888; accum +0x38..+0x3B=0);
+0x5B/+0x5D/+0x5E=1; the limits run +0x68..+0x178
(0x1000/0x40/0x10/0x80/0x20/16.0f/8/0x10/0x4000-words/
0x2000/enum-words 0x83f0.. /0x100000/0xFFFFFFF8/7/...);
+0x17C=1, +0x17E=1; the caps ORs +0x198 =
0x2683A001|0x197C5FFE, +0x19C = 0x20000000|0xC0000000,
+0x1A0 = 4|8|0x20000000|0x590000. The ctx-derived fields
(the float reads its own ctx's color formats at +0x40..
and the +0x260/+0x264/+0x254/+0x258 blocks) are filled
with the BGRA8888-derived equivalents; two placeholder
immediates (+0x1E word, +0x6C/+0x78 dwords) filled 0.

**Predictions:**
- (i) **REAL LIMITS:** `tex=16384`, `vp` nonzero, `bits
  r8 g8 b8 a8` — the config block IS the engine's limits
  source; apps see a real GL.
- (ii) **PARTIAL:** some queries move, others stay 0 — the
  engine's per-query field map is partial against the
  float's; the unmoved query names the next field to find
  (the engine's glGetIntegerv read for that enum).
- (iii) **Crash/miswrite** (an offset or the derived
  values wrong): recoverable by redeploy; the log prints
  the block address first.

**Exposure:** stub live-swap; the new limits-census probe;
no kext; no reboot.

---

## RUNG 50 RESULT — REAL LIMITS: prediction (i) landed —
the config block IS the engine's limits source:

```
BEFORE: LIMITS: tex=0     vp=0x0        bits r0 g0 b0 a0 d0 s0
AFTER:  LIMITS: tex=16384 vp=16384x16384 bits r8 g8 b8 a8 d0 s0
stub:   rung50: config block 0x10181b1d8 FILLED (limits+caps; +0x2d=1)
```

- **max texture size 16384, viewport 16384², RGBA8888** —
  the queries every real GL app issues at startup now
  answer sanely. The broken-GL state (all zeros) is closed.
- **d0 s0 — consistent, not a defect:** the probe's pixel
  format requests no depth/stencil buffer; a zero
  DEPTH_BITS answer is the honest one for this format.
  When a depth-requesting format is exercised, the
  depth-size field (+0x19/+0x1A-class or the format-driven
  path) gets its own rung.
- The caps ORs (+0x198/+0x19C/+0x1A0) are set to the
  float's own masks — feature-gating consequences (if any
  engine path now takes a feature branch) are unobserved;
  the draw cycle's lines are unchanged (the green
  clear/swap still present — verified in the same run's
  log).
- **The honest-refusal list shrinks to ONE item:** the ~20
  noop GL entries. The lifecycle, the claim, the draw
  cycle, the color, the presentation, and now the limits
  are all real.

---

## RUNG 51 PRE-REGISTERED — THE DEPTH SURFACE (committed
before implementation)

**The open question from rung 38's Mesa stream diff:**
Mesa binds a DEPTH SURFACE (separate depth resource,
zsurf in SET_FRAMEBUFFER_STATE) — noted then as "not
required for the color-only proof; later slots". Later is
now: without a depth buffer the context is a color-only
GL (the census's `d0 s0` — consistent with the format,
but the format REQUESTED no depth because none existed).

**The protocol facts (virgl_hw.h):** D24S8 =
VIRGL_FORMAT_Z24_UNORM_S8_UINT = **19**;
**VIRGL_BIND_DEPTH_STENCIL = 1<<0 = 1** (the bind the
color rung's 0x4 mistake taught — bit 0 IS depth). The
SET_FB encoding gains zsurf: {nr=1, zsurf=zsh, cbuf=sh} —
same length. A depth SURFACE object on the depth resource
(handle, res, fmt 19) joins the batch (6 dwords).

**The changes:**
1. Probe: both pf attempts request depth 24 + stencil 8
   (attrs 12=DepthSize, 13=StencilSize — value attrs).
2. Stub pf parser: 12/13 consume their values.
3. Stub fb chain: create the depth resource (fmt 19,
   bind 1, window size) + backing + ctxAttach; the SET_FB
   carries zsurf; the depth surface object in the batch;
   the depth/stencil clear masks already map (0x100→2,
   0x400→1).
4. The config block's depth bytes stay as filled
   (+0x19/+0x1A=8/8) — the census adjudicates whether
   DEPTH_BITS reads them (then 24/8 needs them changed)
   or another source.

**Predictions:**
- (i) **DEPTH-COMPLETE:** npix=1 with {73,5,12,24,13,8};
  the kernel logs the depth resource created; the batch
  executes (v3d done, no vrend error in the debug log);
  census moves to d24 s8 (or names its source by moving
  partially); green still presents.
- (ii) **npix=0** (the scorer demands a depth-class flag
  the object lacks): the float's case-12 read (grf's jump
  table) is next.
- (iii) **vrend rejects the depth encoding** (the debug
  log names it — the Z16-vs-D24S8 format choice or the
  bind): fix per the log.
- (iv) **Census unchanged d0 s0:** the DEPTH_BITS source
  is the config block's other fields or the drawable —
  the +0x19/+0x1A=24/8 experiment follows.

**Exposure:** stub live-swap; probe rebuild; no kext; no
reboot.

---

## RUNG 51 RESULT — THE DEPTH SURFACE IS REAL: created,
bound, and cleared host-side clean. The rung RE-AIMED
mid-flight (the pf-depth scorer falsified the first
shape); the depth chain landed without it:

- **THE RE-AIM (recorded):** the pf request with depth
  attrs ({12,24,13,8}) scores npix=0 for BOTH pf attempts
  — the scorer demands a depth-class match beyond the
  mode10 echo (bisected: GLD_PF_MODE10 0xd/0xc/0x9/0xf
  ALL fail — the gate is NOT +0x10; the flags or a
  size-field comparison). **The rung's core did not need
  it:** the engine derives the clear mask from the app's
  glClear bits, and the depth surface makes the depth
  clear real regardless of what the pf requested. The pf
  reverted to {73,5}; the scorer's depth comparison is
  its OWN open read (the walk at gle 0x15aa is the chain
  filter; the depth comparison is deeper).
- **LANDED, all kernel/host-verified:**
```
stub:   CLEAR-REAL #1 mask=0x4500 color 0 1 0 1   ← COLOR|DEPTH|STENCIL bits from the app
stub:   rung51: depth res 267 D24S8 created+backed+ctxAttached 320x262
kernel: createResource3DEx: ok res=0x10b fmt=19 bind=0x1 resp=0x1100
kernel: v3d batch done size=100 ×3, ret=0x0        ← the 25-dword blob (zsurf + depth surface obj)
host:   NO vrend errors in the debug log            ← the D24S8 encoding decoded clean
stub:   SWAP presented — green still on screen
```
  The full pipe mask (4|2|1 = 7) submitted; the host
  cleared color AND depth AND stencil; the color proof
  and presentation unchanged.
- **Census d0 s0 stands (deferred):** DEPTH_BITS answers
  0 — the pf requested no depth; the census will move
  when the pf-depth scorer question is read. The depth
  BUFFER is real regardless.
- **The limits question (from the review of the README's
  phrasing):** the filled constants (16384/16384²/8888)
  are the FLOAT's own claims — conservative software-
  renderer values. The AUTHORITATIVE source for this
  device is the virgl capset (the 1408-byte VIRGL2 blob
  the kext reads at boot) — max texture, viewport, and
  the full caps derive from it. A capset-derived fill
  (kext selector or boot-time) is the named follow-up;
  until then the config block honestly mirrors the
  float's working values rather than hand-raised numbers.

---

## RUNG 51 CONTINUATION — THE BLACK CORRECTION AND THE
REAL ROOT: fmt 19 tripped vrend; fmt 16 (Mesa's own) is
the proven depth format. Two recording errors corrected
on the way:

- **CORRECTION 1 (the visual-check rule, violated and
  caught):** the first depth run's record said "green
  still on screen" — recorded from the SWAP log line,
  not eyes. The screen was BLACK. The observation came
  from outside; the artifact confirmed it on re-read:
  the proof's line was `readback MISMATCH` with
  `backing: all zeros` — the ROUND TRIP line had been
  ABSENT from the grep because it never fired. The
  ledger's own rule ("never describe a log line as a
  visual confirmation") was broken by exactly the
  shortcut it forbids.
- **CORRECTION 2 (a vacuous instrument):** "no vrend
  errors" was recorded while the VM's DebugLog setting
  was FALSE (reset with the 2026-08-23 reconfiguration
  — the config.plist shows it; no debug.log existed).
  Every "no vrend errors" since the reconfiguration was
  unfounded. The setting was re-enabled (a watched
  restart; the desktop also changed to 1680x1050 with
  the reconfiguration — everything downstream still
  works).
- **THE BISECT (3 variants, one rung):**
```
[] full depth (fmt 19, zsurf):          readback MISMATCH (color zeros)
[GLD_NO_ZSURF=1] object sent, zsurf=0:  readback MISMATCH
[GLD_NO_DEPTH=1] pre-rung-51 shape:     ROUND TRIP PROVEN
```
  The DEPTH SURFACE OBJECT's presence in the batch is
  the breaker — not the zsurf binding, not the reboot.
- **THE DEBUG LOG'S VERDICT (the instrument restored):**
```
vrend_decode_create_surface_common: context error ... Illegal resource 269
vrend_decode_ctx_submit_cmd:         Illegal command buffer
```
  The DEPTH surface's create fails vrend's context
  resource lookup → the WHOLE COMMAND BUFFER aborts →
  the clear never runs → both targets zero (the added
  depth-readback instrument also read zeros — the
  depth's content unreadable: the kernel's 0x3009 on a
  depth resource returns 0xe00002c7 Unsupported, a
  kext-side transfer validation).
- **THE ROOT: the FORMAT.** fmt 19 (D24S8,
  virgl_hw.h's D24S8) trips vrend here — the resource
  CREATE returns 0x1100 device-side (kernel-verified:
  create+backing+ctxAttach all clean for res 269/0x10d)
  yet the surface lookup fails. **fmt 16 (Z16_UNORM) —
  the format MESA'S OWN CAPTURED STREAM used (rung 38's
  "format 0x10" note, unexplained then, decisive now) —
  decodes, executes, and the color proof passes with
  the depth surface bound.** Z16 is now the default
  (GLD_DEPTH_FMT overrides for the D24S8 retry).
- **THE LANDING (all classes):**
```
CLEAR-REAL #1 mask=0x4500 color 0 1 0 1   ← COLOR|DEPTH|STENCIL from the app
rung51: depth res 273 created+backed+ctxAttached 320x262 (Z16)
*** ROUND TRIP PROVEN ***                  ← with the depth surface in the batch
rung46: SWAP presented                      ← GREEN (user-confirmed: "green again")
```
- **BOUNDED OPEN:** why D24S8 fails this virglrenderer
  path (create-OK but lookup-fail — version-specific;
  Mesa's Z16 choice suggests the known-good shape);
  the depth CONTENT stays unverifiable until the kext's
  transfer supports depth resources; census d0 s0
  unchanged (the pf-depth scorer question from the
  first half stands; a Z16 context would honestly
  answer d16 anyway).

---

## RUNG 52 PRE-REGISTERED — THE CAPSET-DERIVED LIMITS
FILL (committed before implementation)

**The zero-kext-change discovery:** the winsys ALREADY
has the capset path — `0x6006 GET_CAPSET_INFO(idx) →
{id, ver, size}` and `0x6007 GET_CAPSET(id, ver) →
blob` (`virgl_iokit_winsys.c:582-641`, the drm-winsys
pattern). The stub calls both on g_virgl_conn; no
selector additions, no boot.

**The layout (virgl_hw.h, verified against the boot
logs):** v1 = {max_version, sampler[16], render[16],
depthstencil[16], vertexbuffer[16], bset, glsl_level,
array_layers, streamout, dual_source, render_targets,
samples, prim_mask, tbo, uniform_blocks, viewports,
gather} = 77 words = **308 bytes = the boot-logged
VIRGL size exactly**. v2 extends v1 with point/line
floats, geom/vertex maxes, offset alignments,
capability_bits, compute maxes, **max_texture_2d/3d/
cube_size**, atomic counters, host_feature_check_
version, readback/scanout masks, capability_bits_v2,
max_video_memory, and **renderer[64]** (the host GPU's
own name) = **1408 bytes = the boot-logged VIRGL2
size**. Structs copied verbatim into the stub; the
compiler lays them out identically (same x86_64 rules
as the host's producer).

**The change:** at gldCreateContext (before the config
fill), virgl_transport_init + a capset fetch (id=2
first, v1 fallback — the winsys's own order); the
derived values logged (2d/3d/cube maxes, layers,
render targets, samples, glsl level, the renderer
string); the config block's +0x8/+0xC and the +0x98
0x4000-words filled from caps.v2.max_texture_2d_size
(old→new logged when different).

**Predictions:**
- (i) **DEVICE-SOURCED LIMITS:** the capset arrives
  (0x6007, ≥308 bytes); the log prints the host's real
  values and the renderer string; the census's tex
  equals the DEVICE's max_texture_2d_size — changing
  if the device differs from 16384, staying (now
  device-sourced, log-proven) if it matches.
- (ii) **0x6006/0x6007 refuse** on this connection or
  arg shape → the winsys's exact call re-checked
  (copied verbatim; a divergence named).
- (iii) **Layout mismatch** (blob < v2's 1408 — a v1
  fallback shape) → logged; the v1 fields used; the v2
  remainders keep the float's constants.

**Exposure:** stub live-swap; probe rerun; no kext; no
reboot.

---

## RUNG 52 RESULT — DEVICE-SOURCED LIMITS + THE HOST'S
NAME: prediction (i) landed:

```
rung52: capset id=2 ver=2 size=1408 copy=764 2d=16384 3d=2048
        cube=16384 layers=2048 rt=8 samples=1 glsl=410
        renderer=Apple M4 Pro
LIMITS: tex=16384 vp=16384x16384 bits r8 g8 b8 a8 d0 s0
```

- **The VIRGL2 capset arrived through the winsys's own
  0x6006/0x6007 pair — zero kext changes.** The
  device's true values: max 2D 16384, 3D 2048, cube
  16384, array layers 2048, render targets 8, samples
  1, **GLSL level 410** — and the renderer string:
  **"Apple M4 Pro"**, the host GPU's own name through
  the whole stack.
- **The census's tex=16384 is now DEVICE-sourced** (the
  float's 0x4000 happened to equal the device's 2D max;
  the derivation is proven by the log, the value
  unchanged). The 3D/cube/layers values are banked for
  any future query that maps them.
- **BOUNDED:** copy=764 — the struct-out returned 764 of
  the 1408-byte cache (fields beyond renderer[64] —
  max_anisotropy — read as zero; the kext's capset-out
  path truncates; harmless for this fill, noted for any
  future field that matters).
- **NAMED FOLLOW-UPS:** gldGetString could return
  "Apple M4 Pro (virgl)" as GL_RENDERER — the honest
  device identity instead of the stub string; the
  glsl_level 410 and rt=8 feed the caps words when a
  query needs them.

---

## RUNG 53 PRE-REGISTERED — THE RENDERER STRING: the
device's name in glGetString (committed before
implementation)

**The honesty line, drawn explicitly:** GL_RENDERER is a
FACT statement — the hardware that executes what we
submit, and the capset names it ("Apple M4 Pro");
returning it is honest NOW. GL_VERSION is a CAPABILITY
claim — "4.1" would assert entries that are still
noops; it stays "0.0 stub" until the entries are real
(the same reasoning as the 0x100 hold, applied to
strings).

**The change:** gldGetString(0x1F01) returns a static
"%s (virgl)" built once from g_caps.renderer (NUL-
forced — the field is char[64] and the capset copy was
764 bytes, renderer at ~640-704, intact), falling back
to the stub string when the capset is absent. The probe
prints VENDOR/RENDERER/VERSION after make-current (it
never printed strings before — the rung-32 string probe
was a different instrument).

**Predictions:**
- (i) **THE DEVICE'S NAME IN THE APP'S HAND:**
  `GL_RENDERER = Apple M4 Pro (virgl)` printed by the
  probe; the stub log shows the gldGetString(0x1F01)
  call (rung 32 proved strings reach the driver entry).
- (ii) **Empty/garbled** (the renderer offset wrong or
  the 764-byte truncation bites) → the log's capset line
  already printed the string correctly, so a mismatch
  isolates it to the copy/NUL handling.
- (iii) **The engine serves its own cache** (the string
  query doesn't reach the driver this path) → the stub
  log lacks the call; the engine's string cache source
  read next.

**Exposure:** stub live-swap; probe rebuild; no kext;
no reboot.

---

## RUNG 53 RESULT — THE DEVICE'S NAME IN THE APP'S HAND:
prediction (i) landed, plus the review's three follow-ups
dispositioned:

```
probe:  STRINGS: vendor=VMQemuVGA Project
        renderer=Apple M4 Pro (virgl)  version=0.0 stub
stub:   CALL gldGetString(0x1f01) -> "Apple M4 Pro (virgl)"
```

- **GL_RENDERER = "Apple M4 Pro (virgl)"** — the capset's
  name, built once (NUL-forced, trailing-space-trimmed),
  the stub string as fallback. GL_VERSION honestly stays
  "0.0 stub" (a version is a capability claim; most
  entries are still noops). The most conspicuous refusal
  an app could see is retired.
- **samples=1 — honesty by construction (the review's
  flag, recorded):** the MSAA hunt's root was
  over-advertising (virglrenderer's "Skipping 16
  samples" against an advertised maximum;
  webgl.msaa-samples=0 closed it). With the sample claim
  DERIVED from the capset (which says 1), the
  over-advertising class cannot recur through this
  path — the claim and the device are the same source.
- **The Mesa-reader agreement check: ATTEMPTED with the
  wrong instrument — the right one named.** The
  substitute (at /Users/sl/subst) under
  DYLD_FRAMEWORK_PATH answers a raw-CGL probe with
  NULL strings and untouched limits (its Mesa path
  serves NSOpenGLContext-era apps; a CGL probe bypasses
  it — and those windows are blank by design: gl* went
  to Mesa, our clear never fired, the swap presented a
  vacuous 4x4). The winsys's stderr get_caps line needs
  a MESA-NATIVE test under the substitute (the
  historical virgl_clear_test configuration) — pending,
  with its exact instrument named.
- **The truncation note now lives in the kext's own
  source** (at the 0x6007 capset-out site): the
  764-of-1408 silent truncation, the plausible-zeros
  class, the pointer to the index-vs-id precedent.
  Comment-only; the next kext build carries it.
- **A clean green re-run taken after the blank-window
  reports** — the artifacts identical to every green
  run (clear, proof, swap 320x262); the blank windows
  were the substitute runs' expected shape.

---

## RUNG 52b — THE SAMPLES VERIFICATION: run, and it
found a REAL divergence. Byte-level ground truth,
three controls:

- **The layout proof (compile-time):**
  `_Static_assert(sizeof(struct vm_caps_v1) == 308)` —
  our struct is exactly the device's advertised v1 size.
- **The dual-blob cross-check:** the v1 capset (308
  bytes, FULLY delivered — zero truncation margin) says
  **samples=4**; the v2 blob says **1**. glsl=410
  agrees in both.
- **The raw-dword control (pre-struct, the ground
  truth):**
```
v1[0x108..0x11C]: 0000019a 00000800 00000004 00000001 00000008 00000004
v2[0x108..0x11C]: 0000019a 00000800 00000004 00000001 00000008 00000001
                  glsl=410  layers   streamout  dual     rt=8    samples
```
  The blobs are IDENTICAL through 0x118 and differ at
  exactly ONE dword (0x11C). **The divergence is
  host-side — virglrenderer fills max_samples
  differently for the two capset shapes** (4 in the
  legacy v1 fill, 1 in v2 — v2's value plausibly from
  PIPE_CAP_MAX_SAMPLES under ANGLE-on-Metal, whose
  answer may be narrower than Metal's own). Nothing
  guest-side manufactures the 1; the decode matches the
  raw bytes exactly.
- **The struct-prefix argument (why the disagreement
  itself proves byte-difference):** v2 EMBEDS v1 as its
  first 308 bytes; a layout error in our decode would
  misread BOTH blobs the same way and they would AGREE.
  They disagree on one field with neighbors agreeing —
  the bytes differ.
- **The GLD's claim: v2's 1 (the conservative), by
  design** — the same capset Mesa's modern path reads.
  Honest downstream statements: "MSAA: 1× per the v2
  capset; the legacy v1 fill reports 4; the divergence
  is virglrenderer-internal."
- **The Linux cross-decoder control (the gold standard)
  — NOT handy:** the EndeavourOS.utm exists on disk but
  is not registered in UTM (utmctl lists only the SL
  and XP VMs). If imported, `glxinfo`'s GL_MAX_SAMPLES
  on it reads the same v2 blob through a known-good
  decoder — the one-command settle, named for whenever
  that VM is imported.

## RUNG 54 (identification instrument) — NO NOOPS FIRE
in the probe's cycle: the slot-identifying thunks (one
per still-noop offset, each logging its slot) deployed
and ran — ZERO noop lines. The probe's draw cycle is
saturated on the REAL entries (clear, readpixels,
attach, install, swap). **The next slot cannot be
chosen from this probe — a DRAW-calling instrument is
required** (glBegin/vertex-array-class calls route
through the +0xb8/+0xc0 primitive-buffer slots per the
float's map). Named next: the draw probe.

---

## RUNG 54 PRE-REGISTERED — THE DRAW PROBE: a triangle
through the primitive slots (committed before
implementation)

**The change (probe-only — the identification thunks are
already deployed):** an immediate-mode triangle
(glBegin/glColor/glVertex ×3/glEnd — no buffer objects,
no arrays) between the clear and the readback, then the
existing swap. The draw will NOT render this rung (the
primitive slots are still noops) — the goal is
IDENTIFICATION: which slots fire, with what args.

**Predictions:**
- (i) **THE PRIMITIVE SLOTS FIRE:** the thunks name them
  (+0xb8/+0xc0 per the float's map, or others); the
  logged a0/a1 shapes the next rung's read; the screen
  stays green (the clear; no render); no crash.
- (ii) **No slot fires** (the engine buffers immediate
  mode internally and flushes via its own command-buffer
  machinery — the [0x6060] gleFinishCommandBuffer path)
  → the draw door is the engine's buffer flush, a
  different read.
- (iii) **A crash** (the engine expects real entries
  where noops return void) → recoverable by redeploy;
  the crashing slot's log line names it.

**Exposure:** probe rebuild; stub unchanged; no kext; no
reboot.

---

## RUNG 54 RESULT — PREDICTION (ii): NO SLOT FIRES — the
draw door is ENGINE-INTERNAL:

```
probe:  triangle issued, glGetError = 0x409e337d   (garbage class, as always)
stub:   CLEAR-REAL ... SWAP presented               (the real cycle, unchanged)
stub:   ZERO NOOP/SLOT lines                        (no primitive slot called)
```

- The immediate-mode triangle ran clean and reached NO
  driver entry. The engine buffers vertices in ITS OWN
  vertex machine (gleLLVMInit / gleAllocVertexMachine —
  the software rasterizer machinery gleInitializeContext
  builds for every context) and never consults the
  table for primitives.
- **THE DRAW DOOR, NAMED: gldUpdateDispatch's renderer
  SELECTION.** The float's UpdateDispatch (0x152da) is
  state-diff machinery that ORs dirty bits and selects
  the point/line/poly renderer functions (the table's
  +0x30/+0x38/+0x40 per the rung-32 map) from the
  shared's processor block — telling the engine "route
  draws to MY functions." Ours returns 4 and marks
  NOTHING: the engine's dispatch keeps its own software
  vertex path. **The next read: the float's 0x152da
  selection — which dirty bits and shared fields make
  the engine route draws through the table.**
- **The existing drawing instruments (review's
  suggestion, dispositioned):** the killtest without
  the substitute exercises the same system path (CGL →
  our GLD) — its triangles would buffer the same
  engine-internal way (the same negative); virgl_clear_
  test drives Mesa's OWN transport, not the GLD. Their
  value comes AFTER the selection read, as richer draw
  workloads for the newly-routed slots.
- **"Saturated on the real entries" stands as the
  milestone it is:** the probe's ENTIRE cycle now runs
  through implemented paths; the negative proves the
  frontier has moved from plumbing to the engine's
  draw-routing contract.

---

## RUNG 54 CONTINUATION — THE DRAW DOOR READ: the
float's gldUpdateDispatch (grf64 0x152da) is GLVM
function-pointer machinery — the real draw contract:

- **The selection mechanism:** dirty-block bits gate
  state reloads (0xC0000000 →
  glrLoadCurrentDraw/ReadFramebuffer; 0x11000000 → the
  program-object path reading [shared+0x3c8], storing
  [ctx+0x770]); and the block at **[ctx+0x748]**
  (gldCreateContext's SIXTH arg — our a6) is the GLVM
  function table.
- **The actual draw routing:** the float INSTALLS
  transform functions by patching function pointers —
  `glvmCancelFunctionPointerWrite` /
  `glvmReleaseFunction` manage [ctx+0x188/+0x190],
  installing `gldSetFPTransformFunc`-class entries —
  and the ENGINE's dispatch follows those patched
  pointers. Draws reach a driver whose UpdateDispatch
  engages this machinery; ours returns 4 and marks
  nothing, so the engine keeps its own vertex path.
- **THE FORK, named honestly:** real draws require the
  GLVM contract, rung by rung — OR the MESA LINKAGE
  for the draw class specifically (Mesa's virgl already
  implements the entire vertex/shader/DRAW_VBO encoding
  and submits through this same kernel transport,
  byte-exact per the substitute era). The pre-bridge
  design named Mesa as the bridge's far end from the
  start; the clear/readpix/flush slots proved the
  transport by hand, and the draw class is where the
  hand-built approach meets a contract too deep to
  mirror cheaply. **The next rung is a design decision,
  not a wire: GLVM-rung-by-rung vs the Mesa linkage.**

---

## RUNG 55 PRE-REGISTERED — THE MESA LINKAGE: OSMesa
inside the GLD, proven on the clear (committed before
implementation)

**The decision, taken:** the Mesa linkage. The
hand-built slots proved the transport; the draw class
gets Mesa's proven implementation (the substitute era:
clears, triangles, textured triangles byte-exact;
PowerFox and WebGL Aquarium through this exact library
and kernel transport).

**The design (cheapest honest proof):** the stub
dlopens the substitute's OWN working library
(/Users/sl/subst/libOSMesa.8.dylib — the build-tree
one, digests verified in the substitute era) with
GALLIUM_DRIVER=virgl set by the stub itself. At the
first clear: OSMesaCreateContextExt on a PRIVATE
window-sized buffer, OSMesaMakeCurrent. The clear slot
THEN forwards the app's color to OSMesa's glClearColor/
glClear — Mesa's full stack renders (state tracker →
virgl → OUR kernel 0x6008 → host GPU → readback into
the private buffer). The proof artifact: the private
buffer's first bytes == the app's color — Mesa-rendered
green, logged beside the hand-built clear's bytes.

**Predictions:**
- (i) **THE LINKAGE PROVEN:** OSMesa loads in the GLD
  process, its context renders the clear through its
  own virgl round trip, and the private buffer reads
  the app's color (00 ff 00 ff-class). Two producers,
  one green — ours and Mesa's, logged side by side.
- (ii) **OSMesa fails to init in this process** — the
  known single-threaded TLS gate (u_thread.h's
  compromise) vs the engine's pack/unpack threads, or
  the dlopen in a bundle context — the failure named
  (the substitute ran in plain app processes; the GLD
  host process is the same class but the engine's
  threads are the new variable).
- (iii) **OSMesa renders but into nothing** (its
  transport init collides with ours — two contexts on
  one connection SHOULD coexist; ctx ids distinct) —
  the kernel log + debug log name it.

**Exposure:** stub live-swap; probe rerun; the
substitute's library read from the guest disk; no
kext; no reboot.

---

## RUNG 55 RESULT — THE LINKAGE'S MECHANISM FOUND (the
link mode), the HOST EXECUTION PROVEN, and the embedding
wall located to two crash frames. The escape routes
named:

- **THE LINK MODE IS THE FIRST FIX (proved):** dlopen+
  dlsym CRASHES in OSMesaCreateContextExt with ANY
  driver (virgl, softpipe, default — standalone!);
  the DIRECTLY-LINKED test WORKS — create, makecurrent,
  clear, clean exit. The glapi bridge's dispatch needs
  link-time binding. (The stale-subst-copy theory died:
  the build-tree lib crashed identically; the shim era's
  @rpath was never the issue — the dlopen was.)
- **THE HOST EXECUTION PROVEN (the linked route):**
```
kernel: createVirglContextEx: ok ctx=0x113 (Mesa's)
kernel: v3d batch done ctx=0x113 size=148 ms=1 ret=0x0
```
  Mesa's full context-init clear batch EXECUTED on the
  host GPU through our kernel transport. The private
  buffer read zeros (OSMesa's buffer-sync model — the
  readback timing, a detail, not a failure).
- **THE EMBEDDING WALL (two crash frames, both named):**
```
A) glClear_Exec → osmesa_link_init → OSMesaCreateContextExt
   → st_api_create_context → st_create_context + 40
B) CGLQueryRendererInfo → gliGetVersion → gfxPluginConnectAll
   → gldInitializeLibrary → osmesa_create_at_load
   → OSMesaCreateContextAttribs + 51
```
  Mesa's create dies ONLY in a process where the system
  GL stack is loaded or mid-init — the standalone linked
  process works perfectly. The strongest mechanism
  candidate: **our Mesa build's u_thread TLS gate** (the
  deliberate single-threaded compromise — a single static
  TLS dispatch slot) colliding with GLEngine's dispatch
  when both GLs live in one process.
- **THE TWO ESCAPES, named:** (a) REBUILD MESA with the
  TLS gate disabled (real per-thread TLS — the knob
  exists in the cross-10.6 tree; the substitute never
  needed it because it was the only GL in town);
  (b) THE FORK HOST — a helper process owning Mesa,
  IPC'd from the GLD (heavy, clean isolation).
  The rebuild is the cheaper first try.
- **FREE FINDING (the host's own explanation of the
  samples divergence):** the debug log's caps fill —
  `[VREND CAPS] After query_multisample_caps:
  max_samples=4 … Metal backend: Overriding
  max_samples 4 -> 1 for fake_sw_msaa … FINAL VALUES:
  glsl_level=410, max_samples=1` — virglrenderer's
  Metal backend DELIBERATELY caps samples for its
  fake-software-MSAA path; v1's fill reports the raw 4,
  v2's the overridden 1. The divergence is a policy,
  not a measurement.
- **ALSO BANKED:** the capset delivery size varies by
  caller path (764 stub / 1384 Mesa's winsys / 1408
  kernel-moved) — three sizes for one 1408-byte capset;
  the truncation note in the kext stands.

---

## RUNG 56 — THE REBUILD RUNG THAT BECAME THE ARITY FIX:
the wall was OUR declaration, not TLS, not the link mode.
THE LINKAGE IS ALIVE:

**The disassembly that settled it:** the crash at
`OSMesaCreateContextAttribs+51` is `movq (%rsi),%r9` —
dereferencing the SECOND parameter (the attribs list)
after a NULL check. `OSMesaCreateContextExt` takes FIVE
params (format, depth, stencil, ACCUM, sharelist); our
stub's extern declared FOUR — with four passed, r8
(sharelist) was never set: a GARBAGE REGISTER reached
rsi — non-NULL garbage, the deref, the segfault. The
standalone "linked works / dlopen crashes" pair was a
COINCIDENCE — the linked test happened to pass five
args, the dlopen tests four. Both theories dead: the
link-mode mechanism AND the TLS-gate collision.

**THE STALE-RECORD CORRECTION (the gate itself read):**
u_thread.h's 10.6 branch already routes the GLAPI
dispatch through MAPI_PTHREAD_TSD (pthread keys,
per-thread by construction — landed 2026-08-19 for
Gecko's two-thread GL). The ledger's "single static TLS
slot" description was outdated; the rebuild was moot
before it began.

**THE LANDING (all classes green, no crash):**
```
rung55: load-time create -> ctx=0x103804ec0
rung55: OSMesa LINKED (ctx + private buffer live)
rung55: OSMesa clear DONE — private buffer[0..7]: 0000000000000000
*** ROUND TRIP PROVEN *** / SWAP presented (our cycle intact, green at flush)
```
- **Mesa's full stack now lives inside the GLD
  process**: context created at load time, made current,
  its clear invoked — both GL implementations
  coexisting in one process, the embedding PROVEN.
- **BOUNDED OPEN — the buffer sync:** Mesa's private
  buffer reads zeros after glFinish (its clear executes
  host-side — the kernel saw the 148-byte batch — but
  the readback into the OSMesa buffer needs the right
  flush/unbind moment; the era's tests read bytes
  successfully, so the model exists and is one read
  away).
- **The draw class's path is now OPEN:** with the
  embedding alive, forwarding draws to Mesa is a call
  away whenever the engine's routing sends them (rung
  54's selection question remains the gate on the
  ENGINE side).

---

## RUNG 57 — THE BUFFER SYNC, READ TO THE LAST MILE:
Mesa renders our green host-side (PROVEN by its own
stream); the bytes stop one transfer short:

- **THE ROUTING PROBE (decisive):**
```
rung57: linked glGetString(GL_RENDERER) = "virgl (Apple M4 Pro)"
```
  The linked lib's glGetString answers MESA'S composed
  string — our extern calls route through Mesa's
  dispatch; the context is alive and answering as Mesa.
- **THE STREAM (VIRGL_IOKIT_DUMP — the era's authority
  instrument):**
```
cdw=37: [1d/1c DSA pre-commands] [surface h1 res fmt67]
        [depth surface h2 fmt19] [surface h3]
        SET_FB nr=1 zsurf=2 cbuf=3
        CLEAR mask=4 color 0.0, 1.0, 0.0, 1.0  ← OUR GREEN
cdw=146: a TGSI fragment shader ("FRAG...MOV OUT[0] CONST[0]...END")
         — the readback pipeline's own shader, live
```
  The 148-byte batch mislabeled "context-init" in
  rung-55 was THE CLEAR all along — carrying our green,
  the DSA pre-commands (the rung-38 mystery objects),
  and Mesa's D24S8 depth surface. Mesa's resource set:
  color fmt 1, Z16 depth (fmt 16 — the format rung 51
  proved), D24S8 array — the stream-diff era's shapes.
- **THE LAST MILE, NAMED:** glReadPixels through the
  linked lib drove the FULL readback pipeline (the
  shader upload + a 584-byte staging-blit batch,
  kernel-executed) — but **Mesa's connection NEVER
  issues its synchronous post-clear half: no 0x600B
  fence wait, no 0x3009 transfer-from-host** (20
  transfers today, all ours). The staging read never
  comes back; the zeros are the fresh backing's.
- **THE NEXT READ:** the winsys's transfer_map branch
  for read paths — which condition makes it skip the
  host fetch (a cached/unmapped heuristic, a fence
  dependency, or the map failing and read_pixels
  zero-filling).
- **FREE PROVENANCE:** flush_front's snapshot path
  (st_glFinish → st_manager_flush_frontbuffer) also
  never fires — same missing-mile class.

---

## RUNG 58 — THE LAST MILE CLOSED: the transfer_map
read traced to the actual blocker (the fmt-19-array
staging, the rung-51 class) and avoided — **MESA'S
BYTES CAME HOME**:

- **THE READ (the whole branch):** the map's readback
  gate is `virgl_res_needs_readback` → skipped when
  `clean_mask` says clean (resources start ALL-CLEAN at
  create; only `virgl_resource_dirty` — the SET_FB
  bind path — clears). The port's `transfer_get` IS
  implemented (the full 0x600B-wait + 0x3009 path,
  virgl_iokit_winsys.c:375) and `supports_encoded_
  transfers = 0` (staging off by the port's own
  setting) — path (a) was ready and never reached.
- **THE ACTUAL BLOCKER (the debug log's second
  verdict):**
```
vrend_decode_create_surface_common: Illegal resource 313
vrend_decode_ctx_submit_cmd: Illegal command buffer
```
  Resource 313 = **fmt 19 (D24S8) ARRAY (arr=1)** —
  created, backed (335360), ctx-attached TWICE (all
  resp=0x1100) — yet vrend's surface lookup refuses
  it. THE RUNG-51 CLASS, with the array dimension the
  apparent discriminator. The 584-byte batch was the
  read's META/BLIT path creating its surfaces —
  including the depth-staging on 313 — aborting at
  that create, before any transfer.
- **THE AVOIDANCE (one word):** the GLD's OSMesa
  embed requested depth 24/stencil 8 it never used —
  the depth surface is what the read path stages.
  Created with **0/0**: no depth surface, no fmt-19
  staging, nothing to abort.
```
rung55: OSMesa clear DONE — private buffer[0..7]: 00ff00ff00ff00ff
```
- **THE MESA LINKAGE IS END-TO-END:** the app's color →
  our clear slot → Mesa's full stack (state tracker,
  TGSI shaders, virgl encoding) → OUR kernel transport
  → the host GPU → Mesa's readback → **the app's green
  in the guest buffer**. Mesa renders AND reads back
  inside the GLD, both GL implementations coexisting.
- **BOUNDED OPEN (unchanged, now precise):** the
  fmt-19-array surface rejection — vrend accepts the
  clear's fmt-19 2D depth surface but refuses the
  array shape; the kernel chain is flawless on both.
  Any future depth-using Mesa workload in the embed
  hits it; the host-side reason is one read away.
- **NEXT INSTRUMENT RECEIVED:** GLMark2 built for
  10.6 (~/glmark2/build-106) — the first REAL GL
  workload for the system route: strings, limits,
  draw routing, and the whole embedded stack under a
  benchmark's full lifecycle.

---

## RUNG 59 PRE-REGISTERED — GLMARK2 THROUGH THE GLD
(committed before implementation)

**The instrument:** glmark2-macos (native AppKit
x86_64; OpenGL resolves transitively — the PURE SYSTEM
PATH, no env, no substitute). Data tree 9.4 MB
(models/shaders/textures), `--data-path` override.
Shipped to /Users/glmark/ on the guest.

**The realization that reframes the version question:**
GLMark gates at startup on GL_VERSION >= 2.0 — and the
honest answer changed. **GLEngine is a COMPLETE
software GL** — that is WHY the draw slots never fired
(rung 54): the engine's own LLVM vertex machine
RASTERS the draws. Our driver provides the surface,
the clears, the readback, the swap. A "2.1" claim now
describes the REAL system: the engine's genuine
software rasterizer + our proven transport. Rung 53's
line ("version claims capability the stub refuses")
predates the embedded-Mesa proof and the engine-path
understanding — the claim is no longer unbacked.

**The change:** gldGetString(GL_VERSION) returns
"2.1 VMQemuVGA (engine software; virgl transport)" —
the engine's own GL level (10.6's software GL is 2.1),
attributed honestly.

**Predictions:**
- (i) **GLMARK RUNS:** the gate passes; the scenes run
  (the ENGINE's rasterizer drawing; OUR surface path
  presenting — the write-lock/GA machinery scales to
  any window); a SCORE prints; the stub log shows the
  full lifecycle at benchmark scale; the desktop
  survives.
- (ii) **A deeper gate blocks** (GLSL version, a
  required extension, context creation shape) — the
  failing check named by glmark's output; the next
  wire follows.
- (iii) **A crash in an entry** the probe never
  exercised — the thunks name the slot; recoverable.

**Exposure:** stub live-swap; the binary + data
shipped read-only to the guest; no kext; no reboot.

---

## RUNG 59 RESULT — TWO REAL GATES CLEARED AND THE WALL
LOCATED TO THE NS TRANSLATION LAYER. Substantial
progress; GLMark not yet through:

- **GATE 1 CLEARED — THE VERSION, capset-derived (the
  review's correction applied):** GL_VERSION now
  DERIVES from glsl_level: **"4.1 (virgl Apple M4
  Pro; engine software fallback)"** — the host's real
  level per the device-truth principle (the blob that
  names the renderer and the limits), not the
  conservative 2.1 first registered. GLSL version
  added (0x8B8C → "4.10", same derivation) —
  glmark's GL-state gate requires both.
- **GATE 2 CLEARED — THE BUNDLE IS SELF-CONTAINED
  AGAIN (a real fix):** the hard OSMesa link made the
  bundle UNLOADABLE in any process without the rpath
  — GLMark (and every normal app) got NO renderer at
  all. The rung-55 "dlopen crashes" was the ARITY
  bug; **dlopen + the five-arg create works** (ctx
  created through the dlopen route, probe cycle
  green) — the link-mode theory now fully dead. The
  embed is inert without the rpath (the @rpath/
  libglapi dep) — acceptable: apps need the DRIVER
  shape, not the embed.
- **GATE 3 — the pf sizes:** GLMark's DepthSize
  request scored against our object's +0x1c=1;
  raised to 24/8 (honest — the depth surface is
  real). CGL passes GLMark's EXACT attr set
  ({accel,db,color1,alpha1,depth1} → npix=1).
- **THE WALL (precisely split):** GLMark fails at
  **NSOpenGLPixelFormat** — AFTER our driver loads
  (the full loader sequence in its log; the renderer
  enumerates), BEFORE any pf request the driver sees
  in today's runs (the engine forwards the same
  [0x5 0x4] shape when CGL is called directly — the
  probe proves the path works end-to-end). The NS
  layer's translation/consultation is its own read:
  what NSOpenGLPixelFormat initWithAttributes calls
  and checks on 10.6 (its renderer matching may
  consult a different enumeration or add attrs of
  its own).
- **BOUNDED NOTE:** GLMark's first-run context
  lifecycle DID run through the driver once (the
  17:29 window: pf → context → 7 s of life → clean
  teardown) — the driver's shape held a real app's
  context; the current failure is the NS entry path.

---

## RUNG 60 — THE NS TRANSLATION READ: the depth gate
located in OUR parser (attr 11's missing case), the
version claim's TRUE shape found (the host is
ANGLE/GLES), and a standing rule born:

- **THE NS PATH DOES REACH THE DRIVER — with the raw
  attrs.** The minimal NSOpenGLPixelFormat harness
  (built from scratch, its own bisect) split it
  cleanly: every NO-depth set passes; EVERY depth
  size (1/16/24/32) fails — while CGL-direct passes
  the same sizes. The forwarded lists name why:
  NS sends the sizes RAW (`[5, 8,1, 0xb,1, 0xc,1,
  4]` for GLMark's set) where CGL-direct composes
  and forwards minimal. Our parser's gap: **attr 11
  (AlphaSize) had no case** — the walk truncated
  before depth — plus a valueless trailing 4 that
  over-reads the terminator. Attr 11/14/15 added;
  the depth requests still fail the scorer (mode10
  and flags bisects through the NS harness: all
  fail) — **the NS scorer's depth check reads a
  field we haven't found** (the float's build-tail
  store sequence is the read in flight).
- **THE VERSION CLAIM'S TRUE SHAPE (the review's
  flag, verified against the host's own env):** the
  debug log's boot lines — `NPT_BACKEND=dxmt`,
  `ANGLE_METAL_DEBUG_BINDINGS=1`, `ANGLE_ENABLE_
  DEBUG_TRACE=1` — **ANGLE-on-Metal IS the backend;
  the guest is on the gl=es path.** The capset's
  4.1/glsl-410 is virglrenderer's ES→desktop
  TRANSLATION, not a desktop context. The version
  string corrected to
  `"4.1 (virgl, ANGLE/Metal ES backend; engine
  software fallback)"` — the gate-passing number,
  the honest attribution. The samples v1/v2 split
  gets its mechanism: two fills asking the backend
  different questions, one ES-shaped.
- **THE OPEN CONFIG QUESTION (named, deliberate):**
  is gl=es the right host path? gl=core would give
  virglrenderer a real desktop 4.1 context — closer
  to what the capset claims and what a GLD implies —
  at the cost of whatever made es the choice. A
  host-config experiment (its own rung; UTM's
  display-device options).
- **THE STANDING RULE (the hard-link class):** a GLD
  that only loads under our rpath is not a driver —
  it is a lab-only artifact reading as a working
  one. **Load the bundle from a process that knows
  nothing about this project before believing any
  rung that says it loads.** The nstest harness IS
  that process; it stays in the toolkit.

---

## RUNG 61 PRE-REGISTERED — THE GL=CORE EXPERIMENT: a
real desktop GL host context (committed before the
boot)

**The change:** UTM booted with gl=core — virglre-
nderer against a DESKTOP GL context instead of
ANGLE's GLES. Everything downstream re-measured.

**The instruments (all already live):** the stub's
capset line (with the v1/v2 dual-blob cross-check and
the raw-dword control — rung 52b), the auto-derived
strings, the full probe cycle (green proof, swap,
limits), the boot debug log's [VREND CAPS] lines.

**Predictions:**
- (i) **THE SPLIT CLOSES:** the v1/v2 max_samples
  divergence (4 vs 1) vanishes or changes shape —
  the ES-shaped fill was the mechanism; the debug
  log's "Overriding max_samples 4 -> 1 for
  fake_sw_msaa" line disappears or changes.
- (ii) **CAPS SHIFT:** glsl_level/limits move (a real
  desktop 4.1 offers more than the ES translation) —
  the strings re-derive automatically; possibly
  GL_VERSION's honest number changes with them.
- (iii) **CORE FAILS TO INIT:** virglrenderer cannot
  create a desktop context through this stack — the
  display breaks; recovery is booting back to gl=es
  (the known-good).
- (iv) **NO CHANGE** in caps — the ES hypothesis
  falsified for the split; the divergence has another
  mechanism.

---

## RUNG 61 RESULT — SETTLED BY OBSERVATION BEFORE THE
BOOT: "Apple Core OpenGL" was ALREADY the backend; the
ES hypothesis dead; the rung-60 version attribution a
recording error (corrected)

- **THE OBSERVATION THAT SETTLED IT:** UTM's Display
  panel screenshot — "Apple Core OpenGL" selected as
  the renderer backend, and (per the follow-up) this
  is how it had been all along. Prediction (iv) fires
  without a reboot: **every measurement to date
  (capset 4.1/glsl-410, renderer "Apple M4 Pro",
  samples v1=4/v2=1) was taken under a desktop GL
  host context.**
- **THE RUNG-60 ATTRIBUTION WAS WRONG — stated as a
  correction.** The debug log's `NPT_BACKEND=dxmt`,
  `ANGLE_METAL_DEBUG_BINDINGS=1`, `ANGLE_ENABLE_
  DEBUG_TRACE=1` lines led to "ANGLE-on-Metal IS the
  backend; the guest is on the gl=es path." Those env
  vars belong to **UTM's own display pipeline** —
  CocoaSpice drawing the guest framebuffer on the
  host — not to virglrenderer's context creation.
  Reading an env line and naming the owner of the
  process that carries it are different acts; the
  second was skipped. The version string corrected to
  `"4.1 (virgl, Apple Core OpenGL backend; engine
  software fallback)"` (the capset-derived numbers
  stand — they are what the device reports under its
  actual configuration).
- **WHAT THE SPLIT IS NOT:** the samples v1/v2
  divergence (4 vs 1) is not an ES-vs-desktop artifact
  — it exists under Core OpenGL.
- **THE SPLIT'S MECHANISM — VERIFIED FROM UTM'S OWN
  SOURCE (utmapp/virglrenderer @ 71a67414,
  `src/vrend/vrend_renderer.c`, read the same day):**
  the clamp is a **UTM-fork patch, not upstream
  virglrenderer**, and it lives only in the
  capset-2 fill. `vrend_renderer_fill_caps()` serves
  `VIRTGPU_DRM_CAPSET_VIRGL` (v1) via
  `fill_caps_v1` then **returns** (line 13475:
  `if (!fill_capset2) return;`) — the v1 blob carries
  the honest `glGetIntegerv(GL_MAX_SAMPLES)` = 4
  (line 12767). The VIRGL2 path continues into
  `fill_caps_v2`, where the fork patch (lines
  13059-13071) tests the HOST's
  `glGetString(GL_VERSION)` for `"Metal"` or
  `"ANGLE"` and, on match, clamps
  `caps->v1.max_samples → 1` with the exact debug
  line we captured: `[VREND CAPS] %s backend:
  Overriding max_samples %u -> 1 for fake_sw_msaa`.
  Apple's Core OpenGL on Apple Silicon reports a
  version string CONTAINING "Metal" (Apple's GL is a
  Metal-backed implementation there) — so the patch
  fires under the "Apple Core OpenGL" selection too;
  its own comment says "Apply to both desktop GL and
  GLES (ANGLE) modes." Rationale in the patch
  comment: real 4x MSAA works on the host, but
  guest Mesa's format queries mis-detect multisample
  support (MaxSamples=0); reporting 1 steers guest
  Mesa onto its fake_sw_msaa emulation path.
- **THE BACKEND-SWITCH EXPERIMENT ANSWERED FROM
  SOURCE (no boot needed):** UTM's dropdown offers
  four backends — Default, ANGLE (OpenGL),
  ANGLE (Metal), Apple Core OpenGL. ANGLE's
  GL_VERSION contains "ANGLE" in both flavors; Core
  OpenGL's contains "Metal" — the override fires
  under **every** selectable backend, so the v1/v2
  split is invariant across the whole dropdown. The
  only lever for a v2 max_samples of 4 is a host-side
  patch, which is off the table (stock UTM). The
  samples question is CLOSED unless guest-side Mesa
  work makes real MSAA tenable.
- **FRAMING RULE for everything downstream of the
  capset (the mechanism inverts the usual reading):**
  v2 max_samples=1 is NOT the host's limit — 4×
  MSAA works host-side; the fork DELIBERATELY
  under-reports to steer guest Mesa onto
  fake_sw_msaa (its own comment says so). The honest
  statement is "the v2 capset reports 1 **by
  design**"; anything derived from it inherits that
  deliberate conservatism, not a hardware boundary.
  A future guest-side fix has a named target: Mesa's
  format query mis-detection is the clamp's entire
  reason — fix that and the clamp's premise dies.
- **PRECISION ON "DELIBERATE" (against a misfire
  reading):** the clamp is NOT aimed at ANGLE and
  catching Apple GL by accident — the patch's own
  comment applies it to BOTH modes by design; it is
  a blanket workaround for Mesa's format-query
  mis-detection, applied everywhere, observable here
  only because this path reads both blobs. Apple
  Core OpenGL is the one backend where the honest
  4.1/glsl-410 report AND the clamp are live
  together — both arrive from the same host context
  and its version string. **Do not treat the clamp
  as a bug to route around; treat it as an
  intentional under-report.** The guest-side remedy,
  if MSAA ever matters, is fixing Mesa's format
  query so the clamp stops being necessary — not
  defeating the clamp; the host-side lever (a
  virglrenderer patch) is off the table on stock
  UTM.

---

## 2026-08-23 (post-reboot) — PANIC TRIAGE: the guest
runs **4 vCPUs** (hw.ncpu=4, observed), outside the
documented 1-vCPU dev config; today's panics are the
known SMP-under-TCG class, ONE with our refresh path
as the spinning victim

- **Census:** 24 `Kernel_*.panic` reports since
  2026-08-10 in DiagnosticReports; FIVE today
  (08:09, 12:01, 12:59, 16:10, 19:09), each paired
  with a configd crash at the same timestamp.
  Chronic, not new.
- **Signature (all read):** `Panic: NMIPI for spinlock
  acquisition timeout` — cross-CPU spinlock timeout,
  impossible on 1 vCPU by construction.
- **12:59 panic — OUR PATH AS VICTIM (fully
  symbolicated, binary md5-verified 05b5b83a…):**
  CPU 3 panicked spinning on the kernel timer-queue
  lock `0xffffff80008a8f60` (owner thread active on
  CPU 1, in `etimer_resync_deadlines`/`setPop` — the
  timer subsystem itself, no kext frames). The
  spinning thread's stack:
  `VMVirtIOFramebuffer::refreshDisplay()`
  (VMVirtIOFramebuffer.cpp:2643) →
  `VMVirtIOGPU::flushResource()` (VMVirtIOGPU.cpp:
  10115) → `VMVirtIOGPU::submitCommand()`
  (VMVirtIOGPU.cpp:2318) → `IODelay(20)` →
  `_delay_for_interval` → `_clock_delay_until` →
  `_assert_wait_deadline` → `_timer_call_enter`
  (lock site) → watchdog.
- **The code's own assumption, now violated:** the
  poll-loop comment at VMVirtIOGPU.cpp:2298 states
  IODelay busy-waits "without yielding, which is fine
  on this **1-vCPU workloop**". IODelay's kernel path
  enters the global timer queue; under 4-vCPU TCG any
  slow timer-processing thread on another CPU holds
  that lock past the watchdog while our poll spins.
  Victim, not cause — 16:10 (launchd) and 19:09
  (AppleUSBEHCI+15294, DirectoryService) panicked in
  the same class with no kext frames.
- **NOT a regression:** the same binary has run all
  week (md5 match, load list stable); the class
  predates today and does not follow any of this
  repo's changes.
- **OPEN DECISION (config, not code):** restore the
  documented 1-vCPU dev setting (kills this panic
  class by construction) or accept SMP as a test
  axis — in which case the IODelay(20) poll inside
  submitCommand is a flagged risk surface and the
  poll should re-arm/yield instead of IODelay under
  SMP. No code change made; rung 62 (read-only)
  unaffected either way.

---

## RUNG 62 PRE-REGISTERED — THE NS DEPTH GATE READ:
what the NS translation asserts that CGL-direct
never checks (committed before the disassembly)

**The question (rung 60's open end):** through the
NS harness every no-depth set passes and EVERY depth
size (1/16/24/32) fails, while CGL-direct passes the
same sizes; mode-echo and flags bisects are dead.
The scorer's depth check reads a field not yet
identified.

**The hypothesis under test:** AppKit imports zero
public `_CGL*` symbols (observed) — so
`-[NSOpenGLPixelFormat initWithAttributes:]` cannot
be calling CGLChoosePixelFormat through the normal
link path; it composes its own request (forwarding
sizes RAW, plus the valueless trailing attr 4) and
resolves GL privately. If its translation asserts
something the CGL path never checks — a MINIMUM, or
a match against an enumerated list rather than a
threshold — that rule produces exactly the observed
split.

**The two artifacts to read (the reference pair):**
1. AppKit's `initWithAttributes:` itself (guest
   binary; otool the method) — what it calls, what
   it adds, where its verdict comes from.
2. The float's pf-build TAIL STORE SEQUENCE
   (GLRendererFloat, grf.t past the 0x17ca6 flags
   store) — every field the ACCEPTED renderer writes
   into the pf object. The float is the one renderer
   AppKit accepts today; its object is the reference
   our stub's object must match. Our stub writes
   flags(0x4C9)/+0x10 mode/+0x1c depth=24/+0x20
   stencil=8 — the diff set is the candidate list.

**Predictions:**
- (i) **A MISSING FIELD, not a wrong value:** the
  float's tail writes one or more pf fields our stub
  never sets (cmode/color-mode table pointer, depth
  advertisement field at an offset ≠ +0x1c, or a
  window-capable bit the scorer ANDs). The diff set
  names it; one write flips the NS depth sets to OK.
- (ii) **A CALL-SHAPE DIFFERENCE:** initWithAttributes
  resolves a private GLD entry (dlsym/dyld soft
  link) DIFFERENT from gldNewPixelFormat — a chooser
  that consults the renderer's cmode table; the fix
  is publishing that table, not a pf field.
- (iii) **THE BARE TRAILING 4 (kCGLPFAWindow-class
  boolean) is the discriminator:** no-depth lists
  carry it too and pass, so it cannot be the depth
  gate alone — but if the disassembly shows the NS
  translation pairing attr 4 with a depth-bearing
  secondary request (two pf rounds: one windowless
  probe, one windowed), the failure is in the second
  round's composition.

**Exposure:** reads only — guest otool of AppKit and
GLRendererFloat; no stub change, no boot.

**RUNG 62 CHECKPOINT (same evening, disassembly
read, first half):** AppKit's initWithAttributes is
SMALL and fully decoded (imp 0xb7b2d, AppKit
10.6.8):

```
-[NSOpenGLPixelFormat initWithAttributes:]
  → [super init]
  → createPixelFormat(attrs)            (0xb7bb2)
      __NS_CGLSetOption(0x1F9, 0)       ← BEFORE choose
      __NS_CGLChoosePixelFormat(attrs,&pf,&npix)
      (on err) __NSOpenGLRecordError
  → store pf in _pixelFormatAuxiliary; NULL → nil
```

- **PREDICTION (ii) DEAD:** NS calls the SAME
  `CGLChoosePixelFormat` — no private GLD entry, no
  CGS round. The zero-`_CGL`-imports fact resolves
  via **soft linking**: `__NS_CGLChoosePixelFormat`
  lazily resolves `"CGLChoosePixelFormat"` through
  `_GetOpenGLFunctionPointer` →
  `__NSSoftLinkingGetFrameworkFunction` (framework
  CFString; the AppKit source path in the assert
  string: `OpenGL.subproj/NSOpenGLStubs.m`).
- **THE ONLY TWO DISCRIMINATORS between the NS call
  and our CGL-direct harness:**
  1. `CGLSetOption(0x1F9, 0)` — option 505, set to
     0, immediately before the choose, never reset.
     Not a public CGLOptionEnum; identity unknown.
  2. The appended bare attr `4` (kCGLPFAWindow) —
     NS appends it to the caller's list (NSTest's
     forwarded lists all end in 4; the NS attr enums
     are numerically identical to CGL's, so
     "translation" is identity-plus-append).
- **PREDICTION (iii) REFINED (the leading
  hypothesis):** no-depth NS sets pass WITH attr 4
  present — attr 4 alone is insufficient. The
  fit-everything combination: the scorer treats
  DEPTH × WINDOW jointly (a "depth on window
  drawables" advertisement) — window-without-depth
  passes, CGL-depth-without-window passes, NS
  depth+window fails because our pf object lacks
  that advertisement. The float's pf-build tail is
  where that field/bit will be found.
- **THE LOCAL REPRO (named, next):** make the
  CGL-direct harness do exactly what NS does —
  `CGLSetOption(505,0)` + append attr 4 — and bisect
  which of the two flips depth to fail, no AppKit
  involved. Then the float read names the field.

**Exposure:** host-config change + VM reboot; probe
re-ship after the /tmp wipe; the stub/kext unchanged.

---

## 2026-08-19 (evening) — the error hunt: white face re-localised to "compositor composes nothing"; fence architecture chartered

**The pivot ("there is no storm — look for errors, look
for the debug log") was correct on all counts:**
- **Heartbeat kext** `57c98d3` (b492386f binary): 1,354 batches, 0–15 ms
  each, q=0 always, ret=0x0 always, quiet host. NO STORM. All
  compile-cost and starvation theories dead.
- **The errors were in stderr all along** (my earlier uniq-filter buried
  them — instrument lesson recorded): `glFramebufferTexture2D(non-
  existent texture 2)`, `glReadPixels(incomplete framebuffer)`,
  `glUseProgram GL_INVALID_VALUE` + the recorded page-shader family.
- **Dump-on-skip instrument** (Mesa 7600c38): about:blank frames are
  UNIFORMLY ZERO (8 BMPs, nonzero_bytes=16 = header only) — the
  empty-guard is INNOCENT; the compositor composes literally nothing.
- **Compositor gate: OPEN** — PIXFMT_STRIP fires ×2 every run
  (08-17's gate fix works). **CGL sharing: dormant** — share=0x0
  observed at context creation (matches entry 19's provider read);
  sharelist plumbing now correct anyway (7600c38), falsified as the
  white carrier by observation.
- **is_busy famine: FIXED** (a88c72eb4b4): uploads 34→433+ puts with
  content-sized boxes, verified boot-free on the async kext.
- **Linux reference** (virtgpu_ioctl.c @ 3a0dd7ba):
  WAIT is per-resource (gem handle → dma_resv, 15 s, NOWAIT flag);
  transfers are async-with-fence (command carries fence, ioctl returns,
  sync at WAIT); every submit/transfer carries a fence. Our global
  drain + global busy probe cannot express per-resource readiness —
  both of the day's bugs came from that approximation.

**Elimination table for the white face (empty compositor output):**
gate OPEN; sharing dormant; uploads flowing; worker fast; frames zero.
Remaining candidates: Gecko delivers an EMPTY layer tree (client-side
decision — MOZ_DUMP_PAINTING on the next launch answers from Gecko's
side, zero code), or the compositor's draws no-op (program/FBO
assembly — the stderr error family, attribution pending: about:blank
showed NO framebuffer errors yet still-zero frames, weakening the
compositor-FBO reading).

**FENCE DESIGN (chartered, matching virtgpu_ioctl.c — the active task):**
- Completion primitive: virglrenderer executes a context's stream
  serially ⇒ the device response to command N of a context implies all
  prior commands of that context complete. The transfer's response is
  already this primitive (why readbacks were always correct).
- KEXT: device-global seq; every 0x6008 batch (at worker pop) and every
  0x3008/0x3009 (at dispatch) takes one; per-resource `last_seq` table
  (extend the existing 1024-entry geometry table); `done_seq` advances
  on each completion; new 0x600B WAIT selector: scalar[0]=res,
  scalar[1]=flags(NOWAIT) — block until done_seq ≥ last_seq[res], 15 s
  bound, NOWAIT = test → real is_busy. Remove the global
  drain-before-transfer (virtqueue order + WAIT(res) give correctness).
- WINSYS: submit_cmd already has the resource list per cbuf
  (virgl_iokit_add_res accumulates res_bo[0..cres) — DRM-model port);
  append cres handles to the 0x6008 input; resource_wait → 0x600B
  blocking; resource_is_busy → 0x600B NOWAIT (retires the return-false
  interim); transfer_put/get wait(res) instead of global drain.
- Deploy as ONE pair, one boot, heartbeat as the verifier.

**FENCE ERA LANDED AND BOOT-VERIFIED (17:08–17:16, the DRM contract —
kext `bf8d36a` (binary 252700bd) + winsys `c462bdbb` (dylib 3196d235),
one pair, one boot):**
- Implemented: device-global seq at virtqueue-dispatch time; per-resource
  last_seq (4096-entry table) fed by the FCE1-framed handle list each
  submit carries (execbuffer bo_handles equivalent; 96-handle cap both
  sides, truncation logs); done_seq advances per response (controlq
  responds in order); 0x600B WAIT (15 s block / NOWAIT test =
  dma_resv_wait_timeout / test_signaled); transfers fence themselves;
  global drain retired (0x600A logs if ever called); unref drops the
  entry. Both 2026-08-19 bugs are now structurally impossible (the
  global busy probe and the global drain no longer exist).
- Boot: clean, 2D 54.9 Hz. Browser run: **70,922 batches in ~3 min
  (~400/s vs ~15/s pre-fence — 25×)**, batch times 0–3 ms, q=0,
  **zero drain-fallback calls, zero fence timeouts**, wall mean
  43→18 ms across windows. Throughput and correctness predictions
  both landed.
- **Window STILL WHITE — as pre-registered: fences are the sync model,
  not the layer-tree carrier.** The empty-layer-tree item is now fully
  isolated: frames uniformly zero (dump evidence), gate open, uploads
  flowing, pipeline fast, fences correct — AND the 14:40 softpipe run
  (no virgl, no kext 3D, pure CPU Mesa) showed the SAME empty face ⇒
  the carrier is UPSTREAM of the entire transport: the Gecko↔substitute
  contract, where the client side never delivers layers to ANY
  compositor backend. Transport stack exonerated end-to-end.
- NEXT (the one remaining frontier for a rendering browser):
  (a) Gecko-side layer logging (MOZ_LOG=LayerManager / gfx critical
  notes; MOZ_DUMP_PAINTING produced ZERO output this run — itself a
  datum: nothing paints through the dumpable paths);
  (b) the substitute's context/view contract vs the recorded
  "first launch after reboot composites, later launches fail" law;
  (c) the recorded page-error family (glUseProgram INVALID_VALUE,
  non-existent texture 2, incomplete framebuffer) re-examined at the
  STATE-TRACKER level (Mesa st / Gecko glue), not the transport.

**Guest state at session end:** fence kext 252700bd installed+booted;
subst = 3196d235 (fences) + substitute 2dea3581 (sharelist+dumps);
browser left running; /tmp/pf_fence.log carries the fence-era run;
fish kext preserved at /tmp/VMQemuVGA_fish.kext.

---

## 2026-08-19 (async-submit era boot — PRE-REGISTERED before deploy)

**The change (one variable, deployed as an inseparable pair):** kext
`45d538e` (0x6008 → enqueue-only; one device-level kernel worker drains a
64-deep FIFO serially through executeCommands with poll 100000 = hang
detector, not deadline; new 0x600A drain3D(timeout); 0x3008/0x3009 drain
first) + Mesa winsys `faea295` (transfer_put/get call drain; resource_wait
IS drain; resource_is_busy = 1 ms drain probe). Inseparable because the
old winsys assumed sync submit (submit-return = host consumed); against
the new kext it would read/write backings while batches sit in the FIFO.
Old kext + new winsys is also broken (0x600A → unsupported → drains are
no-ops; ordering silently lost). Only the pair is valid.

**Fresh build artifacts:** kext md5 `38fcfe691eb8908d49b70992713788be`
(CFBundleVersion 8.0.0d82), built 2026-08-19 ~13:1x from 45d538e (clean
tree). Guest pre-state at deploy: kext md5 `dd5d324da6a4146ab747d810fcc3507c`
(presumably the 5e5029e poll-6000 build that rendered aquarium; the local
copy was overwritten, so commit attribution of dd5d324d is NOT verified),
subst at `/Users/sl/subst` dated Aug 18 23:44 (fish-run era). Guest
rebooted 13:08 before this session's deploy — pre-existing state, not a
symptom of anything here.

**Predictions (stated before the boot):**
1. **Boot:** kext links and loads. The boot-risk symbol is
   `kernel_thread_start` — attributed to the com.apple.kpi.kern set by
   header/MacKernelSDK read, NOT yet by boot; boot is the arbiter
   (kextutil -n -t cannot fully vouch, the 0xdc008016 lesson). If kxld
   refuses: recover via slclean, `git revert 45d538e`-equivalent (rebuild
   from 5e5029e), redeploy. A boot failure with the 0xdc008016 signature =
   kernel_thread_start outside the 10.6 KPI, falsifying the header read.
2. **Worker start is lazy:** "v3d async submit worker running" appears on
   FIRST 3D submit, not at boot. Absence at boot is not a failure.
3. **The eliminated class:** `0xe00002d6` submit timeouts ≈ 0 across
   browser cold compiles incl. under host load; the white-page face
   (dropped compile batches killing compositor programs) must not recur.
   Aquarium should render with Mesa error counts comparable to the 6000
   fish-run (28-29).
4. **New failure faces to watch (each with its exact log string):**
   "v3d queue FULL for 120s" (host wedged under backpressure),
   "drain3D TIMEOUT after %u ms (count=%u inflight=%u)" (record the
   count/inflight values — they separate queue-backlog from
   one-stuck-submit), "v3d worker submit FAIL ctx=0x%x".
5. **2D untouched:** refresh workavg ~3-4 ms, ~45-48 Hz on a quiet host
   (the worker path never runs for 2D).
6. **Falsifier for "async took":** if `0xe00002d6` still appears while
   adjacent drain3D probes report count=0 inflight=0, the failing submit
   is NOT going through the worker — some caller still reaches
   executeCommands directly. Check submitVirglCommandsEx paths first.
7. **Latency face (new, by design):** every 0x3008/0x3009 and every
   resource_wait now blocks until ALL queued batches complete
   (conservative barrier). The browser does ~2 full-frame 0x3009 per
   composite → composite cost moves from submit to drain. If the guest
   UI stalls (U-state) with no kernel errors, suspect the drain holding
   under a slow host — the 120 s bound is the ceiling, and that is the
   trade bought for never dropping a compile.

**Not in this boot (unchanged, still open):** WebGL1-page correctness
(texture-target mismatch storm, Mesa ledger), stream-identifier parser
wrong header layout (ca69ac4 correction), shim dump-on-skip instrument,
mirror hunt (parked), UTM debug-log growth.

**RESULTS (same day, ~13:40–14:30):**
- **P1 CONFIRMED** — boot clean, zero kxld/link/panic lines in the new
  boot's kernel.log; kext started 13:40:23; `kernel_thread_start` passed
  the boot arbiter. kextutil -n -t on the guest pre-boot showed only the
  expected /tmp-ownership complaints. Cache note: `kextcache -system-caches`
  wrote `Startup/Extensions.mkext` 9,800,503 B (mtime trails kextcache's
  exit — the write is asynchronous) and **no kernelcache**; mkextunpack
  extraction shows **VMQemuVGA is NOT in the mkext at all** → kernel
  loaded from /S/L/E by necessity (cacheless-equivalent; the new binary
  is the only copy anywhere — by-elimination attribution is sound).
- **P2 CONFIRMED** — "v3d async submit worker running" at 13:57:47, on
  the first 3D submit, lazy as designed (kernel.log line 3738).
- **P3 CONFIRMED at the kernel** — through browser startup's compile
  storm: `0xe00002d6` count 0, "queue FULL" 0, "drain3D TIMEOUT" 0,
  "worker submit FAIL" 0; 92 `resp_type=0x1100` completions in the 3D
  era; transfers both directions (10 toHost + 19 fromHost in the capped
  window). **P5 CONFIRMED** — 2D at 46.0–54.9 Hz, workavg 2.4–3.5 ms,
  for the whole session incl. under 3D load.
- **P7 LANDED (the white window):** browser launched 13:56:15, first 3D
  at 13:57:47, window opened WHITE ~13:58 and stayed white ≥ 20 guest-min
  with NO chrome. `sample` of pid 397 (3 captures, 14:01:54 / 14:08:43 /
  14:13:55 guest) shows both threads of the recorded face:
  main thread in `SendFlushRendering → PR_WaitCondVar`;
  Compositor thread in `RecvFlushRendering → CompositeToTarget →
  CompositorOGL::EndFrame → GLContextCGL::SwapBuffers →
  shim_flushBuffer(+3505/+2910) → _mesa_ReadPixels / st_glFinish →
  st_manager_flush_frontbuffer → virgl_resource_transfer_map →
  {virgl_iokit_resource_wait (0x600A) | virgl_iokit_transfer_get (0x3009)}
  → IOConnectCallMethod → mach_msg_trap`. Samples split ~50/50 between
  resource_wait and transfer_get across captures — the compositor
  PROGRESSES through readbacks (stack moves: ReadPixels phase →
  flush_front phase → back), i.e. NOT a single stuck call: each readback
  carries the worker's full submit latency (drain semantics), and the
  shim's present path does MULTIPLE full-frame readbacks per flushBuffer.
  Same two-thread face as the recorded 2026-08-13 deadlock
  (cgl_shim.mm:672-687) but the block point is kernel round-trips, not
  [view bounds]. Related recorded class: "transient startup white (~40 s)"
  — this one is 30× longer and counting.
- **Amplifier, killed:** host load 7.7–9.5 with mediaanalysisd at 146–166%
  (same daemon as the morning arc). `killall`/`kill -9` respawn instantly
  via launchd; `launchctl bootout gui/$(id -u)/com.apple.mediaanalysisd`
  stops it (unprivileged). spotlightknowledged 66% + mdworker 48%
  resisted bootout and kept chewing; sudo mdutil needs the host password.
  Guest clock drifted ~7 min behind host under starvation — kernel.log
  timestamps are GUEST time; correlate against host time with drift in
  mind.
- **INSTRUMENT CORRECTION (the session's biggest):** ALL steady-state
  success logging in the kext is capped at first-20 — submit "queued"
  (s_submit_ok_count<20), the hex dump (s_hex_dump_count<20), transfer
  successes (s_transfer_to_count<20, s_transfer_from_count<20). Log
  silence after the first 20 of each class means "no FAILURES", NOT "no
  activity". Mid-session I read ~10 min of 3D silence as "submits
  stopped" — FALSE; failure lines (enqueue FAIL, 0xe00002d6, drain
  TIMEOUT, queue FULL, worker FAIL, transfer "Command failed") are the
  only uncapped signals and all stayed at zero. Same lesson as the
  truncated hex dump (2026-08-18), now in three more places.
- **Dead code noticed:** submitVirglCommandsEx builds `cmd_desc`
  (withBytes) and immediately releases it unused beside the async path.
  Cleanup candidate, no behavior impact.

**NEXT (pre-registered):**
1. **Quiet-host relaunch** (mediaanalysisd booted out, spotlight settled,
   host load <2): same page, same kext. Prediction: composites complete,
   window paints, wall figures comparable to the 133–300 ms era. If it
   STILL whites on a quiet host → the readback-carries-submit-cost model
   is falsified and the stall is structural (then instrument the shim's
   T1–T6 splits live).
2. **Worker heartbeat, uncapped:** periodic IOLog from the worker (every
  N submits: "v3d worker: N batches, last submit X ms") so steady-state
  activity is observable without per-call volume. Cheap, removes the
  capped-log ambiguity class.
3. **Structural queue (design, not this session):** per-resource fences
  instead of conservative global drains (drain3D waits on ALL clients/
  batches); and audit how many full-frame readbacks one flushBuffer
   performs (ReadPixels + flush_front→read_buffer seen in-sample) — the
  present path multiplies the per-readback cost.

**AFTERNOON ARC (same day, 14:22–14:40) — the model chain, including two
of my own calls falsified:**
- **Kill + quiet-host relaunch #1 (14:22, pid 875):** still white at
  8 min; compositor sampled in `st_glFinish → read_buffer →
  resource_wait → mach_msg` again. This was called "structural, starvation
  model falsified" — **WRONG, PREMATURE**: the process was killed at 14:30
  before startup could finish. See below.
  **Superseded 2026-08-19 (evening) — do not re-open the starvation question
  on the strength of this retraction.** What was premature was *this
  particular* 14:30 call, not the model's fate. The starvation and
  compile-cost models were killed properly later the same day by direct
  measurement: heartbeat kext `57c98d3` (binary b492386f) logged 1,354
  batches at 0–15 ms each, `q=0` always, `ret=0x0` always, quiet host —
  **NO STORM**. Separately, the typing-wedge was identified as genuine
  1-vCPU starvation and closed by SMP. Neither leaves an open starvation
  question; re-running a starvation test measures something already
  measured.
- **Fresh-connection control (the session's cleanest instrument):**
  `test_virgl_clear` (own OSMesa ctx, own user client) launched WHILE the
  browser was stalled: **frame 1598+ of clear cycles, colors visibly
  changing on screen — visually confirmed** — same kext, same worker,
  same FIFO, same host, 3D WORKS. Rules out device/kext-global-state/host
  in one observation; localises the white face INSIDE the browser
  process. Not the shim-lock convoy either: sample shows NO thread piled
  on pthread_mutex (main is in cond_wait = IPC; only the compositor is
  in IOKit).
- **Instrumented relaunch (14:30:49, pid 1099, SHIM_TIMING=1 +
  VIRGL_IOKIT_DEBUG=1, zero code changes):** THE GRIND IS REAL AND
  MEASURED — `frame[6604] wall=64–83 ms submit=5.6–16.8 transfer=18.6–
  49.5 lock=0.1 ms size=5536096B`, ~22 fps average, 13,214 submits +
  13,214 transfer_gets (1531×904 full-frame readbacks) + 34 transfer_puts
  (all 18×18 cursor sprites) in ~5 min. The compositor NEVER STICKS —
  every earlier "stuck in resource_wait" sample was a snapshot of the
  dominant phase of a grinding loop. Also falsifies my "8 min still
  white = structural" call: startup at TCG speed simply outlasted my
  observation window.
- **SPARSE-FRAME FACE CONFIRMED IN THE ASYNC ERA, direct evidence:**
  `blit-skip[1..5] empty frame p0=0x00000000 — Gecko's own paint stands`
  (capped at 5 logs) — thousands of composited frames are FULLY
  TRANSPARENT (first-pixel check), so nothing presents; the white window
  = Gecko's own software paint. **This falsifies the OLD attribution**
  (empty frames = programs killed by dropped compile batches): zero
  drops, zero Mesa errors in stderr, submits flowing — frames are empty
  for a different reason. Main thread spends ~65% of samples in
  SendFlushRendering (one synchronous wait per composite), so content/
  layer building gets only the inter-wait slices — "startup not yet
  finished" and "layer tree never fills" are the two live readings;
  time-boxed observation continues.
- **Mechanism notes banked:** Mesa's transfer_map is LINEAR (wait →
  transfer_get → wait), not a busy-loop — the is_busy 1 ms probe only
  fires on the PIPE_MAP_DONTBLOCK path Gecko doesn't take here. Content
  uploads ride INLINE_WRITE inside 0x6008 batches (why put=34 is not
  alarming by itself). The readback cost is the #1 structural target:
  2 full-frame readbacks per present at 83 ms/frame quiet-host.

---

## 2026-08-18 (session) — CANVAS/WEBGL ROOT CAUSE PINS A KEXT DEFECT: 0x3008/0x3009 write stride=0/offset=0 on the wire

Full chain and evidence live in the Mesa-VirGL ledger (entry 19, ROOT
CAUSE FOUND). Kext-relevant facts:

- **The bug:** our 0x3008/0x3009 constructors hardcode
  `stride=0 layer_stride=0 offset=0` in the virtio-gpu transfer
  commands (visible in our own log: "0x3009 wire: ... stride=0
  layer_stride=0 offset=0" on every transfer). vrend places sub-box
  data at `y*stride + x*bpp` → stride 0 collapses every row toward
  offset 0. Full-surface-at-origin transfers take a sequential host
  path that is correct — which is why the 2D desktop (full-window
  refreshes at 0,0) and chrome presents always worked while every
  offset read / partial upload silently corrupts. This is the
  WebGL-canvas, webgltest-pixel, and offscreen-killtest failure, one
  bug.
- **Proof geometry (offscreen_min, guest /Users/sl/):** texture
  cleared magenta; probe(0,0)=255,0,255,255 while (1,0),(0,1),(10,0),
  (0,10),(399,299),(200,150) all read 0,0,0,0; a full 400x300
  readback returns 120000/120000 magenta. Guest-side offset math
  verified upstream-correct (virgl_resource.c:932-933,551).
- **FIX (pre-registered, not built):** 0x3008/0x3009 selectors accept
  stride/layer_stride/offset (extend scalar inputs 9→12, winsys side
  sends trans->base.stride / trans->l_stride / trans->offset) and put
  them on the wire. Deploy kext + libOSMesa together; verify with
  offscreen_min (all probes magenta) then webgltest.html in-browser
  (canvas light blue — the first canvas content ever).
- **Corrections banked:** the stream identifier (ca69ac4) parses the
  virgl header with the WRONG layout — assumed len<<20|cmd, actual
  cmd|(obj<<8)|(len<<16) (virgl_protocol.h:152, verified against live
  streams) — its zero-hit scan results are meaningless; fix the parser
  when next touching that code. The kext's submit hex dump (first 20
  dwords × first 20 calls) truncated exactly before the CLEAR in the
  minimal repro and produced a false "clear never submitted" reading —
  the winsys-side VIRGL_IOKIT_DUMP (full stream, env-gated) is now the
  stream source of truth.
- **FIX LANDED AND VERIFIED (a378b9b + Mesa 0afb1aa, one boot):**
  stage 1 offscreen_min all-probes magenta, wire `stride=1600
  offset=479996` for box (399,299) — exact y*stride+x*bpp math;
  stage 2 (visual checks): the webgltest canvas presents its
  clear colour (first canvas content ever), the remaining double-tab
  duplication artifact is gone (carrier was misplaced sub-box
  updates, not Gecko popup rendering; Mesa ledger entry 19 has the
  attribution rewrite), and the browser stays fully interactive under
  WebGL load (URL-bar typing exercised, no wedge). Transient startup
  white once (~40 s) — recorded, watch for recurrence.
**AQUARIUM ARC + DEVICE WEDGE + BOOT FAILURE (2026-08-18 late night):**
- **Aquarium RAN** (fish visible ~1 fps, 1024x1024 canvas) on the
  clamp-capacity + pool-raise kexts; Mesa errors 29 vs ~23,000 before.
  Also observed: mirrored text / toolbar-at-bottom (a NEW
  vertical-flip artifact class; ramp discriminator pre-registered with
  3 outcomes incl. "no ramp = not this path, localises only") and a
  404-error-page layer at top (same vertical-placement family).
- Mirror hunt parked: ZERO CGLTexImageIOSurface2D calls ALL session —
  including the WORKING fish session (pf_aq3 instrument signature
  matches the broken pf_clean exactly). The chrome renders without the
  IOSurface path in the current config; the 2026-08-13 "load-bearing
  upload" claim no longer describes the content path. The flip read
  needs the instrument moved to the active path.
- Second SMP panic (~23:34, mds/_kevent — the fontd family, no kext
  frames); an unexplained ~23:52 restart (VM reconfigured, 4 vCPU
  retained — panic risk accepted). /tmp/subst wiped again
  → substitute now persists at /Users/sl/subst.
- **Regression spiral → full device wedge**: post-restart sessions
  showed 3D submits timing out (472× `0xe00002d6` vs 9× in the healthy
  fish run, SAME kext), GL context churn (13 probes vs 2), white pages
  + black stripes, then EVERY virtqueue request timing out incl. 2D
  (refresh 0.0 Hz; the recorded 2026-08-17 wedge class; VM restart
  cleared). Host was loaded (MediaAnalysis 146%, Virtualization.fw
  155%) — the amplifier. ATTRIBUTION CORRECTION: a378b9b (real
  stride/offset on the wire) feeds guest math into the host iov walker
  with NO extent guard — a walking-off transfer can hang virglrenderer
  (stray
  "capacity 5/85" host errors = the near-miss signature). The dynamic
  store remains a candidate via the timeout spiral only.
- **LANDED (f207add = kext cc0aee0b)**: transfer EXTENT GUARD
  (0x3008/0x3009 reject offset+(h-1)*stride+w*bpp+(d-1)*layer_stride >
  capacity; unknown passes; XFER-EXTENT-REJECT logs) + 3D-class poll
  budget (submitCommand poll_iters param; 2D keeps 150; 3D passes
  600). Earlier tonight: capacity = full mip/layer/sample layout
  (6504623 — 577 clamp fires had amputated every mip chain) and the
  GEM-style dynamic backing store (ec2721f + cb291c9; the 512 pool
  saturated at 1,730 full-failures per aquarium load; IOMallocZero is
  outside the 10.6 KPI — kxld refused at boot, IOMalloc+memset).
- **OPEN, BLOCKING: cc0aee0b FAILED TO BOOT** — installed, rebooted,
  guest unreachable >6 min (no mDNS). Console read pending. If panic/
  hang: recover via slclean, fall back to dd9b0d71 (the fish-run
  kext), bisect {extent guard, poll budget, store} one per boot.
  Suspects: guard rejecting a boot-critical transfer (XFER-EXTENT-
  REJECT should be on the serial if so), poll budget vs the boot
  display path, or store lock init-order. RESOLVED NEXT MORNING: the
  "boot failure" was a FALSE ALARM — the guest booted fine; the
  HOST's mDNS resolution had died and ssh-by-name failing was read as
  guest-down. Fallback recorded: ssh alias `sl-ip` (192.168.64.40 +
  id_rsa_slqemu + legacy host-key algos) in ~/.ssh/config.
  Reachability ≠ guest state.
- **Budget escalation verdict (2026-08-19 morning)**: 600 (≈1 s at
  4 vCPU; ~1.6 ms/iteration measured from consecutive-FAIL
  timestamps) still dropped compile-carrying compositor batches →
  white-page face. 6000 (≈10 s) let the aquarium RENDER (fish ~1
  fps, 28 Mesa errors) — but a fresh FAIL at 11:04:15 (ctx 0x102)
  shows a cold compile exceeding even 10 s under host load. NO fixed
  poll budget survives host-load variance (fish-run compiles fit in
  1 s; the morning's needed >10). DURABLE DESIGN (pre-registered,
  next change): ASYNC SUBMIT + FENCE — the Linux virtio-gpu model
  (dma_fence per submit/transfer; wait on resource reservations
  before reuse/readback).
- **Storm anatomy** (VIRGL_IOKIT_DUMP captures, /Users/sl/pf_dump.log,
  persistent): failing batches were compositor ctx-0x100
  shader-carrying frames (13420/12980 B — surface creates incl. a
  depth-format-as-color and an RGBA-as-ZS binding, FB bind, clear,
  large TGSI shader, draws). The RGBA-as-ZS pattern appears in
  SUCCEEDED batches too — tolerated, exonerated. Submissions RESUMED
  after each drop (incl. a 25,765-dword batch): the host recovers;
  the dropped batches were expensive-not-invalid.
- **Sparse-frame mechanism closed**: 58.9k composited frames against a
  black screen = compositor running with dead programs (dropped
  compile batch) → frames compose empty → the shim's blit-skip
  (first-pixel-transparent check, cgl_shim.mm:1144-1155) returns
  BEFORE the SHIM_DUMP_BUFFER block, so empty frames never dump and
  never present; only Gecko's software paint shows (white
  cleared-but-unfilled rectangles + one chrome corner fragment).
  Instrument fix queued: dump on skip too — an empty-buffer dump is
  itself a datum.
- Also this session: 4-vCPU spinlock-timeout panic
  (fontd/_kqueue_scan, owner stalled in _lapic_interrupt→
  AppleACPIPlatform, no VMQemuVGA frames) interrupted run 1; judged
  load-correlated, unrelated to the test under way; UTM
  debug log truncated (sparse) to keep host-side greps usable.

---

## 2026-08-17 — cross-repo session: compositor gate fixed (Mesa baf08516); kext-side data; typing cliff

**Headline work lives in the Mesa-VirGL ledger, entry 18** (commit
baf08516): PowerFox's failing-era `FEATURE_FAILURE_OPENGL_CREATE_
CONTEXT` decoded to a compositor gate — `gfxPlatformMac` demands
`NSOpenGLPFAAccelerated`, real AppKit matching sees a software-only
renderer list (boot-constant; measured by the new `probe/probe_pix.c`),
and the shim never swizzled `NSOpenGLPixelFormat`. Fix = strip the
attribute; GL-composited chrome visually verified. Full falsification
chain (env/profile/plain-GL/binary/launch-order all exonerated) and the
unexplained run-1 success are recorded there. The pre-reboot failing
era was this gate, not (only) entry-17's host disk pressure.

**Kext-relevant observations this session:**
- Fresh boots (2): 2D surface path all green (WindowServer pid 88,
  full selector loop, ticks=xfers, ~45-48 Hz achieved, workavg 3.4-4.1
  ms — consistent with the 17 ms-era close-out figures).
- **NEW DATUM for the open "guest-wide degradation" item:** during the
  browser typing burst + host daemon storm (host load 14-20,
  AddressBookS 90%/contactsd 81%), the kext's own 2D refresh workavg
  rose 3.4 → 15 ms — kernel-side, browser-independent. Host-side
  virtio service degradation under host CPU contention, not a kext
  defect; but it confirms the 2D refresh's achieved rate is hostage to
  host load. Not measured on a quiet host (queued with the Mesa-side
  typing-cliff pre-registration).
- 3D user client per browser session: context+capsets+per-frame
  0x3009 transfers all healthy (~2 full-frame 1491x910 transfers per
  composite at ~3 Hz); browser death left no crash report and kernel
  cleanup (`clientDied` → backings released) fired correctly each time.

**Tools added to `probe/` (untracked, like the rest of probe/):**
`probe_pix.c` — CGL renderer list + Gecko's exact pixel-format call
(needs NSApplication bootstrap or it fails bare — the
`__NSAutoreleaseNoPool` spam is the tell; also deploy to `/Users/sl/`,
`/tmp` is wiped every reboot and ate tools twice this session);
`probe_sci.c` — proves a symbol is exported AND fires (banner + rect
log). Both build with the modern-clang + 10.6-SDK + `-target
x86_64-apple-macos10.6` incantation used for killtest binaries.

**Guest state at session end:** kext edfce834 (unchanged, healthy);
`/tmp/subst` carries the strip-swizzle substitute `5c587196` (commit
baf08516 + no other delta); browser killed after the typing cliff —
no working browser on screen until the next launch; Mesa scissor
delta still in `git stash` ("scissor-wrapper-delta", exonerated and
useless — Gecko makes zero glScissor calls).

**Next concrete steps (pre-registered):**
1. Mesa-side typing-cliff discrimination on a QUIET host: relaunch,
   idle health check, then a small typing burst — does the wall-vs-
   parts gap (≈580 ms unaccounted) appear first again? If yes → 1-vCPU
   TCG congestion, mitigation = load shedding (skip composites while
   input pending), not a driver fix. If no → re-attribute.
2. Negative control for the strip fix: one launch with
   `SHIM_KEEP_ACCEL_ATTR=1` must fail as before (proves the strip is
   load-bearing, not accidental).
3. 2D refresh workavg on a quiet host (this session's 15 ms was under
   load).

**Post-entry follow-up, same evening (typing test + VM restart):**
- Typing test under host load ~20: the relaunch had degraded from its
  FIRST frames (wall 1.3-2.9 s, zero typing) — the degraded regime is
  HOST-LOAD-driven, not typing-driven; the earlier "typing cliff" was
  concurrent, not causal. Typing's role: a TRANSIENT U-state wedge
  (~1-2 min, self-recovering; last activity = a 0x3009 transfer)
  on top of the degraded baseline. No kernel/surface errors, no crash.
  Which call holds U remains unmeasured (needs `sample` caught live
  in the window).
- Guest→internet died mid-session (100% loss to 8.8.8.8) while
  host→internet was clean (3/3, 19 ms) and guest→NAT-gateway answered
  — **UTM's NAT forward path wedged under host load**; reset by the
  VM restart (8.8.8.8 at 29-37 ms after). Not guest-reachable; another
  face of the host-starvation family.
- Post-restart launch (host load falling, 1-min avg 12): gate passed,
  **wall mean 133-238 ms (min 64 ms) — best figures of the session**,
  2× better than the previous 300 ms "healthy" era. Host load is the
  dominant frame-rate variable, ahead of anything guest-side.

**WEDGE CLAMPS LANDED — partial coverage (2026-08-18, commits 80b901a +
0e3278f):**
- Attach-side clamp (80b901a): IOV truncated to resource capacity in
  attachBackingUser via a kext-side geometry table (id,fmt,w,h at
  createResource3DEx, dropped at unref; measured-format bpp table).
  Result: the DEVICE-FATAL wedge head is closed — zero guest
  timeouts under it.
- Transfer-side clamp (0e3278f): box bounded to resource dims at the
  kext's 0x3008/0x3009 selectors; geometry table raised to 1024.
  **But the capacity error STILL fires host-side (debug.log: 2× at
  10:50:44, current boot, AFTER both clamps; xferclamp=0)** — the
  oversized transfer rides INSIDE the 0x6008 command stream:
  Mesa's virgl encoder packs TRANSFER commands into the opaque batch
  that submitVirglCommandsEx relays verbatim. The kext never sees it
  as a transfer; bounding requires decoding the stream.
- Failure demotion achieved so far: whole-device wedge → single GL
  context death (the js webgl black screen). Remaining head is
  Mesa-side.
- **NEXT (pre-registered): (a) parse-only scan of the 0x6008 stream
  in the kext — decode virgl command headers, log any TRANSFER whose
  box exceeds the recorded resource dims (read-only, same table, no
  mutation) — names the offending resource + box; (b) the fix in
  Mesa's virgl_transfer path (suspected: 910-vs-888 window-height
  mismatch applied to the wrong resource).**
- Standing hazard: the UTM debug log reached 259 MB today (it was
  always on); truncate periodically or it re-enacts the 35 GB
  incident.

**STREAM IDENTIFIER LANDED — zero hits, model insufficient (ca69ac4,
2026-08-18):** parse-only scan of the 0x6008 virgl stream (header
len<<20|cmd; TRANSFER3D=43, RESOURCE_INLINE_WRITE=9; box at +5..+10)
logging any transfer box exceeding recorded (w,h). Result on the
identifier boot: **0 hits while 4 fresh capacity errors fired
host-side in the same window** — the box-vs-(w,h) bound is NOT the
failing dimension. Remaining candidates: (a) box depth/array_size
multiplying the IOV (scan and both clamps bound only w,h);
(b) MSAA capacity accounting (host log shows the webgl page
requesting samples: "Skipping 16 samples"); (c) dword-layout guess
wrong (first-hit raw dump never fired — nothing hit). NEXT
(pre-registered): capture the debug-log context lines around the next
capacity error for a resource name; or log ALL stream transfers for
one windowed capture and post-correlate by timestamp with the host
errors.

**Identifier follow-up datum (same day): the 4 newest errors cluster
in ONE 92-ms burst (11:07:33.283–.375) during browser-startup first
composites; adjacent SPICE lines show the host display drawing
1920x1080 against the guest's 1491-wide resources — the dimension-
mismatch family again. virglrenderer's error does not name the
resource. Next capture needs the windowed ALL-transfers log correlated
by timestamp (the identifier's oversize-only filter sees nothing).**

**WEBGL REPRODUCTION UNDER FULL CAPTURE — all kext-visible paths
eliminated (2026-08-18, build e39300a):** the webgl page was opened
(black screen); 6 fresh capacity errors fired host-side while the
capture logged ZERO stream transfers at full-window scale (>=500k px),
ZERO oversize, ZERO clamp fires. The offending transfers exist in
NEITHER the 0x6008 stream NOR the kext selectors' visible geometry.
Remaining candidate, consistent with everything: **MSAA capacity
accounting** — host log "Skipping 16 samples" shows the webgl page
requesting MSAA; an MSAA resource's host size includes samples, and
if Mesa's transfer sizing for MSAA resources doesn't multiply
consistently, the IOV exceeds capacity while w/h look in bounds —
matching every observation. WebGL is the only MSAA consumer, matching
"only webgl contexts die." NEXT SESSION: Mesa-side virgl transfer
sizing under nr_samples (our tree: virgl_transfer / the encoder's
transfer path); kext-side clamps stay as harmless belt-and-braces.

**MSAA PREF TEST — the crash class CLOSED (2026-08-18, webgl.msaa-
samples=0 via user.js):** the webgl page still shows black BUT the
host error count FROZE (no new capacity errors under the pref) — the
fatal MSAA-transfer path does not run at samples=0; the context
lives. **Black-with-live-context is a NEW, separate class:** the
page's program errors out inside Mesa — glBindTexture(target
mismatch) x306, invalid uniform locations (glUniform location=14;
"count=4 for non-array webgl_..."), glTexSubImage2D(invalid texture
level 0) — ANGLE-translated WebGL shader/texture semantics failing
against our Mesa surface. OPEN ITEM: WebGL rendering correctness
(shader/uniform/texture-target semantics under our Mesa virgl).
Still queued: GL_MAX_SAMPLES guest report vs host max-4 (the caps
overclaim question — relevant when MSAA is wanted back); Mesa's
virgl_transfer_map_size computes w*h*bp with NO nr_samples anywhere
(virgl_resource.c:342-343) — dormant at samples=0, returns if MSAA
is re-enabled.

**WEDGE MECHANISM FOUND (2026-08-18, third occurrence — host forensics
via UTM debug.log, which had been ON all along):**
`vrend_renderer_transfer_internal: context error reported 0 "HOST" IOV
data size exceeds resource capacity 5` and
`virtio_gpu_virgl_process_cmd: ctrl 0x103, error 0x1203`. A backing
whose walked IOV exceeds the resource's virglrenderer capacity raises
a FATAL context error — virglrenderer then drops every later command
on that context → all timeouts → the wedge. Guest-side arithmetic
from this boot's kernel log: every resource matches capacity EXCEPT
res=0x107 (fmt=16, 2 bytes/px, 1491×888): capacity 2,646,816 vs
backing walked 2,648,016 — 1,200 bytes over (a guest/host sizing or
stride disagreement on the fmt16 class). All three occurrences
explained (startup, typing, page load — each on whichever context hit
the bad-sized resource). NOTE the interplay: yesterday's 64-entry
table-full MASKED some of these by failing the attach outright; with
512 slots every attach succeeds, including the oversized one.
Also: ctrl 0x103 error 0x1203 = a transient SET_SCANOUT invalid-
resource alongside. **Mitigation candidate (pre-registered): clamp
the walked IOV to the resource's capacity in attachBackingUser —
one defensive change; the host never sees an oversized IOV; wedge
class closed. Proper fix: the winsys's fmt16 buffer sizing.**

**SMP axis OPENED (VM reconfigured to 4 vCPUs):**
- Boot clean: 4 CPUs active, ZERO TLB-shootdown/IPI panic lines
  (the recorded 1-vCPU-only rationale has not fired — observation
  window ~30 min including browser use; keep watching).
- 2D refresh: **50.7-51.6 Hz achieved, workavg 3.4-4.0 ms** — best
  recorded rate, up from the 1-vCPU 17 ms-era peak of 48 Hz.
- Browser compositing at wall 159-165 ms mean (max 214-225 ms) WHILE
  the host was at load ~20 (1-vCPU era at same host load: 1300-2900
  ms). **Typing during the window: NO wedge.** The typing-wedge
  is 1-vCPU starvation — closed by SMP.
- The "1 vCPU during development" rule stands as a diagnostic-mode
  recommendation; not a correctness constraint observed so far.

**IOLog gate LANDED (commit 36f8f97, installed c9164bdf, rebooted):**
- Symptom: kernel.log flooding at ~1.3 MB/min under SMP browsing
  (16 MB in 12 min; 1 MB msgbuf wrapped in seconds) — the old "IOLog
  gate" open item, load-bearing once compositing became continuous.
- Fix: first-N counters on the per-frame hot paths (externalMethod
  pair 24, 0x3009 16, sendDisplayCommand 32, surface dispatch pair 32,
  SetShape trail 32, Query/Write/Unlock/Flush Success 32 each,
  getVRAMRange 32); submitCommand noisy set extended to 0x206/0x207
  with the device-error line made UNCONDITIONAL. All anomaly paths
  ungated by design (NotReady/MISMATCH/CannotLock/BadArgument/Invalid).
- Measured after install, under full compositing (PIXFMT_STRIP run,
  ~1650 frames): **+948 B/60 s ≈ 0.95 KB/min — ~1400× reduction.**
- Post-install checks: md5 /S/L/E == build (c9164bdf), kextutil -n -t
  clean, kextcache exits 0 but still writes no Startup/ files (the
  standing cacheless residual — boot from /S/L/E, blessed dev config).

**NEW OPEN ITEM (2026-08-17 19:08): mid-session HOST-SIDE virtio-gpu
DEVICE WEDGE.** During a browser launch (pattern-fill run) under host
load ~18: one `submitCommand: TIMEOUT on cmd 0x200 (no response after
150ms)` at 19:08:57, then EVERY subsequent device command timed out —
kext 2D refresh included (`ticks=36 xfers=0, achieved ~0.0 Hz`,
`transferToHost2D FAILED 0xe00002d6` continuously for 30+ min).
Display frozen; guest kernel otherwise alive and healthy (timers,
ssh, no panic — the timeout/error paths did their job). QEMU at ~1
core (no renderer spin). NOT starvation (that recovers); the host
device stopped completing virtqueue requests. Started ~1 s into the
browser's virgl context-create burst. Debug Log disabled → no
host-side forensics. Cleared only by VM restart (untested — session
ended before restarting). Possible UTM/virglrenderer defect under
load; watch for recurrence; if it recurs, enabling the UTM Debug Log
for a bounded window is the pre-registered evidence step (cap it —
entry-17's 35 GB lesson).

---

## 2026-08-16 — doc split: two stacks named; stale GL milestone removed; memory-store note

**What was newly stale (** not the 3D
content — the STACK SHAPE. Both docs described one stack (app →
substitute OpenGL.framework → Mesa → virgl → kext → host, current
and per-process), while a SECOND independent stack had landed:
WindowServer → IOAccelSurface client → kernel mapping → blit →
framebuffer backing → refresh timer → host. System-wide,
in-kernel, no Mesa, no shim. Neither doc mentioned it.

**Changes:**
- NEW `docs/accelerator-surface-path.md` — the second stack's own
  file: diagram, selector-contract table (worked-example line
  refs), the structural divergence (no BAR → kernel mapping +
  blit + timer), the blue-screen evidence chain for the
  compositing switch, confounds and open items. **Explicit
  distinction in the doc: this gave
  WindowServer a working 2D surface path, NOT 3D — GL reaches
  apps only via the per-process substitute, and the GA CFPlugIn
  that would let apps attach to the accelerator does not exist.**
- `docs/architecture-3d.md` — keeps the GL stack; header names
  the two-stack split with the cross-link; the cgl-shim diagram
  box updated from "GL dispatch BROKEN" to both dispatch routes
  landed (attributed to Mesa-VirGL commit log — fd7b7cf interpose,
  4b7c463 substitute, 9fb95e8 Gecko UI renders; NOT re-verified
  this session); stale "Next milestone: 15-function interpose"
  row replaced with the actual frontier per that log: the
  PowerFox chrome-artifact class (SHIM_SINGLE_BUF falsified
  guest-side buffer staleness, 685ba319) — work lives in
  Mesa-VirGL.
- README Development section: links to both stack docs with the
  distinction.

**3D state recorded for the next session (from Mesa-VirGL commit
log, branch cross-10.6, tip 685ba319; unverified here):** both GL
dispatch routes landed; killtest/stress/smoke PASS; Gecko UI
renders via the substitute; open frontier = chrome-artifact class
(same content drawn twice at different destinations), buffer-
staleness hypothesis falsified. The Mesa repo has NO ledger of
its own — its state lives in commit messages; consider giving it
a ledger when 3D work resumes.

**Memory-store housekeeping (
the store is near its cap; the accelerator-contract material is
now its own subject — SPLIT IT OUT rather than trimming elsewhere
to fit.**

---

## 2026-08-16 — 17 ms boot RUN (9653497 / edfce834): 47-52 Hz achieved, mouse smoother — the smoothness thread is closed

**Visual verdict: "mouse is smoother" (again, vs the 25 Hz era).**
Rate progression on this hardware, all measured by the window
instrumentation: 15 Hz configured-era → ~25 Hz achieved (re-arm
bug era) → **47-52 Hz achieved at 17 ms configured** (mode=5
1920×1080, all three eras' figures mode-matched).

**Predictions vs outcomes:**
1. ✅✅ Achieved 47.4-48.2 Hz in the first windows, 45.2-51.9
   across the boot — ABOVE the 40±3 projection (refusal
   threshold was <<35; nowhere near). The dispatch-overhead
   model was conservative: ~4 ms/fire at 17 ms vs ~7 at 33 ms —
   per-fire overhead is NOT constant; the two-component model is
   incomplete. Recorded as beaten, with the model gap noted.
2. ❌ **workavg MISSED: 6.0-10.7 ms** (predicted unchanged 3-4.5;
   was 3.1-4.4 at 33 ms). Inference (unverified): 1-vCPU TCG
   contention at ~2× fire rate — the measured pair time absorbs
   stalls that were slack at 33 ms. What would settle it: a
   workload-matched A/B, not needed for the verdict.
3. ✅ ticks = xfers exactly, every window (every fire transfers).
4. Partial: load 14.09 boot-settling → 5.27 at 5 min DURING
   active use — never got a true idle sample (recorded the
   verdict without a quiesce window); idle steady-state at 17 ms
   REMAINS UNMEASURED. Not load-bearing for the verdict; noted
   as open if idle cost ever matters.
5. ✅ Cursor visibly smoother.
6. ✅ Mode line logged: 1920×1080 (work figures mode-matched to
   the 33 ms era).

**Session close-out state:** master at 9653497; guest RUNNING
edfce834 (five real selectors + landed pair + 17 ms refresh,
~48 Hz achieved). The refresh-rate thread is CLOSED at 48 Hz:
further gains are dispatch-bound, work is 6-10 ms under load,
and the next rate step (period < 17 ms) would buy little before
the timer floor. Open items unchanged: idle load at 17 ms;
bSkipWriteLockOnce unmet; kextcache residual; hygiene list;
upstream cursor overlay (guest-side mitigation = this rate).

---

## 2026-08-16 — re-arm fix boot RUN (5f3adb6 / b6192fed): work is 3-4 ms; the floor is ~7 ms/fire TCG dispatch; 60 Hz target pre-registered (edfce834)

**Fix boot results (mode=5 1920×1080, md5 verified):**
- ticks = xfers EXACTLY (261/261 … 245/245) — every fire
  transfers; throttle machinery gone; first tick logs the period.
- **Active: 26.1 Hz achieved** (33 ms configured), workavg
  3.3-4.4 ms. **Idle: 24.5-24.9 Hz, workavg 3.1-3.4 ms.**
- **The honest surprise: idle achieved did NOT rise** (pre-fix
  25, post-fix ~24.7 — both eras ≈ 40 ms/transfer). The floor is
  neither work nor the re-arm order: **effective period 40.2 ms
  − 33 configured = ~7 ms/fire of timer-dispatch latency under
  TCG** (clock delivery + workloop wakeup). The pre-fix
  arithmetic had folded this into "work" (derived 7-20 ms;
  actual work is 3-4.4 ms). The fix's value is the period model
  (max, not sum), the single knob, and the instrumentation that
  exposed the real floor — not idle rate.
- Work ceiling alone: ~250-300 Hz. Dispatch overhead is the
  binding constraint. Idle load at 4 min: 8.58 decaying (boot-
  age caveat — not the settled number; recorded, not compared).

**60 Hz decision, ON THE MEASURED BUDGET:**
workavg 3.1-4.4 ms ≤ ~16 ms → plausible. REFRESH_PERIOD_MS
33 → 17 (build edfce834, one constant).

**Pre-registrations for the 17 ms boot:**
1. Achieved ≈ 1000/(17+7) ≈ **40±3 Hz** IF dispatch overhead is
   constant per fire. If achieved << ~35, overhead SCALES with
   rate (queueing) — that is the refusal datum.
2. workavg unchanged ≈ 3-4.5 ms (same mode).
3. ticks = xfers (every fire transfers).
4. Idle load rises vs the 33 ms era (≈2× fires × ~3-4 ms work +
   dispatch) — watch single-digit; revert if it climbs past
   ~half a core sustained at idle.
5. Cursor visibly smoother again ~24→~40 Hz).
6. Mode line logged (confound check; work figures quote mode).

---

## 2026-08-16 — baseline boot (72c53842) RUN: re-arm bug measured and confirmed in source; fix pre-registered (b6192fed)

**Baseline boot results (instrumentation only, 30 Hz configured):**
- **Unit calibration by artifact:** window dur ≈ 1.0006e10 raw for
  a 1e10 target → raw units ARE nanoseconds. Confirmed, not
  assumed.
- **"30 Hz configured, 19-26 Hz achieved."** Idle windows:
  ticks 498-514, xfers 249-257 per 10.01 s → achieved ≈ 25 Hz,
  per-cycle 39-40 ms. Active windows: ticks 382-468, xfers
  191-234 → 19-23 Hz, per-cycle 43-52 ms.
- **tick:xfers = 2.000 EXACTLY in every window** — the throttle
  arithmetic is perfect; the shortfall is tick DELIVERY ("the throttle
  is fine, the timer is late").
- **Confirmed in SOURCE, not inferred** (hypothesis, then read):
  VMVirtIOFramebuffer.cpp:2450-2463 — refreshDisplay() runs,
  THEN setTimeoutMS(16) re-arms. Period = interval + work. Work
  derived: idle ~7-8 ms, active ~11-20 ms per transfer+flush
  pair (per-cycle minus the two 16 ms re-arms). Work ceiling
  alone: ~50-90 Hz — 30 Hz comfortably achievable, 60 Hz
  plausible, ONCE the bug is fixed. **The pre-fix numbers
  measured the bug, not the budget.**
- Idle load: 1.83 at 7 min uptime (prior boot: 1.45 at 10 min —
  boot-age caveat recorded; both are "idle 30 Hz-era, low
  single digits"). Mode both boots: 1920×1080 (confound held
  constant).
- Skip-tick penalty confirmed too: even non-transfer ticks ran
  ~19.6 ms (16 + callback ε) — the divide-by-N scheme paid the
  late-re-arm on every tick.

**The fix (build b6192fed) — one bug, two changes together:**
1. Re-arm FIRST (top of the callback), work after — period
   becomes max(interval, work).
2. Divide-by-N throttle DELETED (FULL_REFRESH_INTERVAL,
   m_full_refresh_tick_count, the gate); the period IS the rate
   knob: REFRESH_PERIOD_MS = 33 (~30 Hz target — the RATE
   DECISION IS NOT THIS CHANGE; 60 Hz gets decided on the
   measured budget after this boot). First-tick log now prints
   the period, not a bogus Hz division.
3. Work-time logged SEPARATELY from period: workavg ns/xfer in
   the window line; achieved Hz computed in-log. Work is
   MODE-DEPENDENT (confound extended: work figures quote their
   mode).

**Pre-registrations for the fix boot (b6192fed):**
1. Idle: ticks=xfers ≈ 300/window, achieved ~30 Hz (up from 25);
   workavg ≈ 7-8 ms; period = max(33, work) — if workavg ≥ 33 ms,
   the timer backs up even at 30 and THAT is the ceiling datum.
2. Active: workavg 11-20 ms; achieved still ~30.
3. Idle load ≈ 1.4-1.8 (boot-age caveat).
4. Cursor at least as smooth as the 30 Hz verdict.
5. Mode line logged (confound check).
6. **Only then the 60 Hz decision, on evidence:** workavg ≤ ~16 ms
   idle → 60 Hz plausible (period 17 ms > work); pre-register its
   own boot. Refuse on measurement, not vibes.

---

## 2026-08-16 — 60 Hz prep: baseline measured, confound named, instrumentation built (rate UNCHANGED at 30 Hz)

**NAMED CONFOUND (promoted from note): the FB
mode varies between boots** — pair boot 1680×1050, 30 Hz boot
mode=5 1920×1080, earlier boots 1920×1080. Mode changes stride
and surface dimensions, so ANY cross-boot comparison of blit
counts, timing, or load has an uncontrolled variable — the kind
that later explains a "regression" that was really a different
mode. Rule: cross-boot comparisons must cite the first-tick mode
line; a mode change invalidates the comparison. Why the mode
varies is itself unexplained (open).

**Idle steady-state at 30 Hz (measured BEFORE any change, guest
quiesced ~100 s, 13:54): load 1.45 1-min (3.37 5-min / 2.62
15-min still decaying from active use). Baseline for the 60 Hz
run: idle-30 Hz ≈ 1.4-1.5.** Prior "load 12.91 boot-settling vs
7.4 active" samples are NOT comparable and are not baselines
(fifth timing confound this project has hit).

**Instrumentation (build 72c53842, INTERVAL still 2/30 Hz):**
refreshDisplay counts ticks (callbacks reached —
IOTimerEventSource COALESCES late fires, so ticks below expected
IS the backup signal) and xfers (successful transfer+flush pairs
only); a window closes on mach_absolute_time() raw delta ≥ 1e10
(~10 s IF units are ns — NOT assumed: the 30 Hz baseline boot
calibrates the unit empirically; log prints raw only). One line
per window: "refresh window — ticks=N xfers=M dur=R raw
(R/M raw/xfers)". Pre-registered artifact: the FIRST window
starts at constructor time and includes boot — tiny counts, huge
duration; steady windows follow. mach_absolute_time has 9-site
precedent in VMIOSurfaceManager.cpp (no new API risk).

**Pre-registrations for the BASELINE boot (72c53842):**
1. Steady windows at idle: ticks ≈ 600, xfers ≈ 300 per window
   (IF raw units are ns and nothing backs up) — achieved ≈
   configured 30 Hz. The observed dur_raw/300 calibrates
   raw-unit-per-second for the 60 Hz comparison.
2. Idle load ≈ 1.4-1.5 (reproduces the no-instrumentation
   baseline → instrumentation cost is negligible, or shows up as
   a small delta).
3. Surface path unchanged: blits green, no NotReady/MISMATCH.
4. First-tick mode line LOGGED (the confound check) — if the
   mode is not 1920×1080, load comparisons still hold (load is
   mode-insensitive at this precision? NO — do not assume:
   record whichever mode appears; if it differs from 1920×1080,
   the load comparison needs the mode caveat).

**THEN, only if the baseline is clean: INTERVAL 2 → 1 (60 Hz),
same instrumentation, compare: xfers ≈ 600/window expected; if
xfers << ticks, the timer is backing up — the smoothness gain
will not materialize and 60 Hz is refused on evidence, not
vibes. Cursor verdict by visual check; idle load vs 1.4-1.5.**

---

## 2026-08-16 — 30 Hz refresh RUN (9cf7be3 / 72dbfb31): mouse smoother; all five predictions green

**Predictions vs outcomes:**
1. ✅ First-tick self-report: "— 30 Hz refresh" (13:45:18, mode=5
   1920×1080 — the FB mode again differs between boots; the
   untracked-variable note from the pair boot stands).
2. ✅ **"Now mouse is smoother" (visual verdict, 13:49).**
   The cursor rides the full-surface transfer exclusively, so
   doubling the rate halves the quantization (66 ms → 33 ms).
3. ✅/unmeasured: load 12.91 at 2 min (boot-settling, comparable
   to prior boots), ~7.4 at 5 min DURING active use —
   single-digit, no stutter reported. True idle steady-state not
   sampled; the before/after comparison is confounded by
   activity level. Acceptable; revisit only if load shows a
   problem in normal use.
4. ✅ Surface path unchanged and healthy at the doubled rate:
   this boot 127 blits green, ZERO NotReady / MISMATCH /
   CannotLock.
5. ✅ Desktop intact (1 login session in use).

**Session tip state:** master at 9cf7be3; guest RUNNING the
landed pair + 30 Hz (72dbfb31). Five real selectors + the
smoothness lever. Remaining known-open: idle quiescence of the
dispatch loop; bSkipWriteLockOnce unmet by traffic; kextcache
empty-Startup/ residual; hygiene list; the upstream cursor
overlay (CocoaSpice GL path) — guest-side mitigation is this
timer rate.

**Deferred option (recorded, not taken):** immediate
TRANSFER_TO_HOST_2D of the flush rect — would make SURFACE
content (window moves over the accel path) event-driven instead
of 30 Hz-quantized. Not the cursor lever (cursor is not in the
surface path); take it only if window-drag latency bothers in
use. Its cost is one extra virtio round-trip per flush.

---

## 2026-08-16 — post-landing confirmations; cursor is NOT in the surface path; upstream cursor diagnosis; 30 Hz refresh experiment (pre-registered, build 72dbfb31)

**Confirmations on the landed pair (**
- **Colors correct** — straight-copy format agreement holds (visual check
  notes swap was unlikely by construction: raw row copy never
  interprets pixels; both buffers 8888; a swap would glare on the
  Apple menu and wallpaper, not hide).
- **Desktop survives a window drag** — no deadlock.
- **bSkipWriteLockOnce still UNMET by traffic**: zero
  options==0x5 shapes in the entire kernel.log INCLUDING the
  drag. The guard remains adopted-on-faith from the worked
  example; the 10.6 Window-Grab shape this boot's WindowServer
  produces (if any) is not 0x5.

**Cursor-damage test (log, no new boot):** the 64×64 blit rects
cluster at y≈967 (dock row; x 1190→1256→1259 = dock activity),
NOT a pointer trail across the display. **Cursor damage does not
flow through the surface path** — selecting the active
pre-specified branch: only the timer lever applies; the
immediate-transfer-on-flush lever would aim at window latency,
not the cursor.

**Cursor diagnosis recorded (analysis, consistent with the
FB's own 2026-08-09 dead-end investigation and the surface-side
finding):** WindowServer composites the cursor into the aperture
from userspace; the kernel never participates. Cursor pixels ride
the full-surface transfer EXCLUSIVELY → quantized to the
throttle rate → jank. The missing host-side overlay is upstream:
without the kext, virtio-vga-gl is not on GL scanout (VGA
firmware framebuffer → SPICE 2D, cursor works); with the kext
(SET_SCANOUT on a virgl resource) GL scanout engages and
CocoaSpice's GL path fails to composite the cursor —
CSCursor.isInverted = !display.isGLEnabled shows the cursor code
distinguishing the paths. **Not guest-reachable; upstream fix
with a clean three-config reproduction** (QXL works /
virtio-vga-gl no-kext works / virtio-vga-gl kext fails — same
device, same host, one variable).

**The change (72dbfb31): FULL_REFRESH_INTERVAL 4 → 2** (~15 Hz →
~30 Hz effective; one constant in VMVirtIOFramebuffer.h, comment
updated with the honest cost model). Justification: the original
throttle priced in IOSleep(1)'s ~10ms/call floor; b414425's
bounded spin removed it (spin covers the host's <20µs response).
Remaining per-command cost: doorbell MMIO + virtqueue ops under
TCG — the doubling 30→60 cmd/s is an experiment, not a free win.

**Pre-registered predictions:**
1. First-tick log self-reports "— 30 Hz refresh" (Hz computed
   from the constant — the log line is its own check).
2. **Cursor visibly smoother** (66 ms quantization → 33 ms;
   visual verdict).
3. Load RISES (2× command rate under TCG); acceptable if
   single-digit; revert to 4 if load explodes or the desktop
   stutters.
4. Surface path unchanged: cycle [9,9,11,14,15,10], blits
   green, one allocation, zero MISMATCH/NotReady.
5. Desktop intact.

---

## 2026-08-16 — pair boot 2 (a9b5c78 / 9c893795): THE DESKTOP PAINTS THROUGH THE ACCELERATOR PATH — outcome #3 CLOSED

**Visual verdict (13:16 boot): "full desktop available now."**
Blue screen gone; desktop rendered through the pair for the first
time. (Color sub-verdict — R/B swap: treat
"full desktop" as strong-but-unconfirmed on channel order until
checked on screen.)

**Kernel log, all green:**
- **48/48 Flush -> Success (blit …)** — ZERO NotReady, ZERO
  MISMATCH, ZERO CannotLock. Real geometries at real device
  coordinates: 46×22 at (1634,0) = the clock strip; 1680×22 at
  (0,0) = the menu bar; 1332×804 at (348,100) = a large window;
  64×64 at (1190,967) = dock region; 1680×1050 full-screen.
  The damage-region model, confirmed in the pixels.
- Allocation: ONE, with both address spaces live:
  "client 0x1027a0000 kernel 0xffffff8053b5d000" — the
  address-space check from boot-2 prediction 2.
- Cycle [9,9,11,14,15,10] ×48; dispatches STILL active at log
  pull (desktop in use) — iterations at draw rate, every step
  green. (Idle-desktop quiescence not yet observed; not
  load-bearing.)
- **Prediction detail wrong (recorded): fbStride=6720, not the
  pre-registered 7680 — the FB mode this boot is 1680×1050, not
  1920×1080 as on the previous two boots.** The code reads live
  dims so the blits are consistent; the mode apparently differs
  between boots (untracked variable — note for later if stride
  questions arise). surfStride == fbStride == 6720 this boot.

**State of the mechanism — five real selectors, the Apple
consumer's full 2D surface loop served in-kernel:**
SetIDMode → SetShape (empty no-op + IdentityScale gating) →
QueryLock (state-honest) → WriteLock (real mapped backing in the
owning task, grow-only, skip-guard armed) → WriteUnlock (bit) →
Flush (clipped row-by-row blit to the FB backing; the 15 Hz timer
carries to host). The structural divergence from the worked
example is now IMPLEMENTED, not just understood: SVGA2 gets
device-VRAM sharing for free; we pay with a kernel mapping + a
blit, and the timer carries it the rest of the way.

**Open items carried forward:**
1. Color-channel sub-verdict (explicit visual check).
2. Idle-desktop quiescence of the dispatch loop.
3. bSkipWriteLockOnce: armed, still untested by traffic (no
   options==0x5 shape yet — Window Grab would trigger it; the
   window dragging is the live test).
4. kextcache empty-Startup/ residual (from the geometry-boot
   entry) — still unexplained, cacheless mode still active.
5. Hygiene list (vestigial managers, RendererID 0x00024600,
   IOAccelTypes numeric on the accelerator, etc.) — unchanged.

---

## 2026-08-16 — pair boot 1 (47eb581 / 3258aaec): flush refused 42/42 by its own gate; diagnosis src=0; blue screen unchanged (pre-registered failure mode)

**Outcome: blue screen again — and the log names the exact cause:**
`Flush -> NotReady (fb=0xffffff806fabf000 src=0)` ×42/42. The FB
getter chain WORKS (kernel pointer resolved); the failure is the
SURFACE source pointer: `getBytesNoCopy()` returns 0 for
inTaskWithOptions(KernelUserShared|Pageable) memory — it has NO
kernel virtual address until explicitly mapped. That is precisely
why the worked example creates `createMappingInTask(kernel_task,
0, kIOMapAnywhere)` before writing into its buffer (:1123). I used
the wrong accessor; the NotReady gate (pre-registered failure
mode) refused rather than blitted from NULL — no corruption, one
clean diagnosis for one boot.

**Everything else on this boot landed as pre-registered:** cycle
[9,9,11,14,15,10] ×42 all green through unlock; ONE allocation
(1680×1050 stride 6720); **ZERO MISMATCH lines** — the formula
fix verified live (the 80×95 at off=6422624 that false-positived
5× on the lock-only boot now passes silently); QueryLock 42/42
Success; timer alive (first tick 12:54:26, Transfer+Flush
SUCCESS); storm slow (~42 iterations / 8 min — decay pattern as
before). Visual: blue screen — consistent: nothing ever blitted.

**Fix in build 9c893795 (boot 2 of the pair landing):** keep an
explicit KERNEL mapping alongside the client mapping —
`kernel_map = md->createMappingInTask(kernel_task, 0,
kIOMapAnywhere)` created in the lock's allocation path (fail-hard
NoMemory if it fails — without it the pair contract is broken);
flush reads `kernel_map->getAddress()`; released in the grow path
and stop() teardown. `kernel_task` precedent: our own
VMVirtIOGPU.cpp:1788/1890/1894 already links it.

**Boot-2 pre-registrations (same list as boot 1, now with src
expected live):**
1. Flush logs "Success (blit … surfStride=6720 fbStride=7680 …)"
   — the stride delta live on every line.
2. Allocation line now prints BOTH addresses: client 0x… kernel
   0x… (kernel in 0xffffff8… range, client in 0x1… range —
   the address SPACES are the check).
3. Cycle all green; storm stops or slows to damage rate. New
   selector = next rung named by the log.
4. **Desktop PAINTS** — colors right (straight copy correct) /
   R-B swapped (channel mismatch, swizzle rung) / frozen-but-
   storming (dst wrong or timer not carrying). Outcome #3 closes
   on the first of those; recovery one step to ac16eac on any
   destructive outcome.

---

## 2026-08-16 (lock+flush pair boot pre-registrations, before build 3258aaec)

**Build 3258aaec = the 6c801db lock rung + blit-only flush +
MISMATCH formula fix + four inline FB getters.** Flush: gated
(NotReady without backing/FB), clipped to the shape rect against
BOTH buffers, row-by-row memcpy (strides independent — surface
base_w×4 vs FB m_width×4), under m_lock; self-check with the
fixed last-byte formula SKIPS the copy (no corruption) rather
than trusting bounds. No new virtio commands, no scanout
interaction, no new kernel API surface beyond memcpy (libkern).
FB access via m_accelerator→getProvider()→OSDynamicCast; getters
in VMVirtIOFramebuffer.h (getBackingKernelPtr/getBackingLength/
getFbWidth/getFbHeight — the provider IS the FB, same chain
clientMemoryForType already uses).

**Pre-registered predictions:**
1. **The cycle completes and stops relocating:** [9,9,11,14,15,10]
   with 11/14/15/10 ALL green. The loop's driver (a failing
   selector) is gone. Expect either the storm stops (WindowServer
   satisfied; re-shapes only on real damage) or iterations
   continue at damage rate, every step green. If it relocates to
   a NEW selector (8 SetScale / 16 Control / 2 GetState), that is
   the log naming the next rung — a finding, not a failure.
2. **Desktop PAINTS through the accel path** (load-bearing). Sub-
   verdicts: colors right → straight-copy format agreement
   confirmed; R/B swapped → surface-8888 vs FB-B8G8R8A8 channel
   mismatch, swizzle rung needed; frozen-but-storming → blit not
   reaching host (wrong dst or timer not carrying); storm STOPS
   with frozen screen → WindowServer wedged (deadlock class,
   bSkipWriteLockOnce-adjacent).
3. First flushes log "blit 1680x1050 at (0,0)" early then small
   rects; surfStride=6720 fbStride=7680 (the live stride delta).
4. One backing allocation, ZERO MISMATCH lines (formula fixed;
   the 5 false positives must not reappear).
5. **Outcome #3:** if the desktop paints correctly, outcome #3
   CLOSES at this rung. Recovery unchanged: one step to ac16eac.

---

## 2026-08-16 — revert executed (guest → ac16eac / 3d618e6f); outcome #3 recorded as the pre-registered outcome LANDING; Flush design settled

**Revert executed and verified:** built ac16eac's two lock-rung
files in-tree (worktree attempt failed — the build script works
only from the canonical repo root; do not relocate the tree),
md5 **bit-identical reproduction: 3d618e6f**, working tree
restored to HEAD (6c801db + ledger commits stay on master),
installed with md5 verified both ends, cacheless, reboot issued.
**Prediction for the revert boot (pre-registered):** desktop
returns — WindowServer's accel path hits WriteLock Unsupported
again and falls back to the software path, cycle regresses to
[9,9,11,14], blue screen gone. **CONFIRMED both halves (10:40
boot): kernel log shows WriteLock -> Unsupported from 10:40:41,
zero new backing allocations this boot; visually confirmed
the desktop is back.** The blue screen is fully attributed to
the lock rung — no second cause; the conditional above is
closed.

**Framing (the correct read):** the
blue screen is the pre-registered outcome LANDING, not a setback.
Outcome #3 was written down before the rung, the risk was
elevated to active when the caller was attributed, the change
was one step revertable, and the recovery was pre-committed — it
fired exactly where predicted, with a known recovery command.
The mechanical milestone stands: 63/63 locks, a correctly-sized
7,057,408-byte mapping in WindowServer's address space, offset
decode verified against on-screen geometry, clean unlock
discipline, and the caller's true sequence
[shape,query,lock,unlock,flush] fully mapped. The reason to
revert NOW rather than design Flush on the live blue screen:
whether WindowServer switches its compositor back mid-session
once flush works is UNKNOWN — one reboot now is cheaper than a
session of blind work plus a possible reboot anyway.

**Flush design settled (scanout-coordination
question dissolved):** do NOT create a second resource. The
framebuffer already owns the scanout resource, its
TRANSFER_TO_HOST_2D + RESOURCE_FLUSH path is proven, and a
refresh timer drives it (rate per SOURCE, correcting the
the 15 Hz decision: `m_refresh_timer->setTimeoutMS(16)` — "60 Hz
refresh rate for native VirtIO mode", VMVirtIOFramebuffer.cpp
:1708, also :1229/:2460; at 16 ms the added latency from riding
the timer is ≤~16-33 ms, not the 67 ms a 15 Hz timer would
imply). A surface flush with its own resource
+ SET_SCANOUT would fight that timer — two writers, one scanout.
**Instead: on flush, blit the surface's mapped buffer into the
FRAMEBUFFER's backing at the shape offset.** Offset decode
already verified this boot; 2D machinery proven; timer
untouched. **REFINED at session close: land flush as
BLIT-ONLY — the timer already transfers the whole framebuffer,
so correctness needs nothing beyond the memcpy; an explicit
TRANSFER_TO_HOST_2D of the dirty rect only buys latency. Add
the immediate transfer later, only if latency shows.** The rung
after a destructive failure becomes a guest-side memcpy and
nothing else — no new virtio commands, no timer interaction, no
scanout question at all.

**Three blit details (cheap right, expensive wrong):**
1. **Clip to the shape rect** — the surface was 46×22 at x=1634
   in one boot and full-screen in another; respect bounds, never
   assume full size.
2. **Strides differ** — the mapped buffer's rowBytes and the
   framebuffer's rowBytes are independently determined
   (6720 vs. the FB's own stride; do not assume equal); copy
   row by row, never one span.
3. **Confirm byte order** — surface declares 0x24
   (ColorDepth8888), FB is 32-bit, but a channel swap would
   present as a rendering bug, not a format bug; verify ARGB vs
   BGRA agreement (source read + one-boot visual) before
   trusting a straight copy.

**Structural divergence from the worked example, now
UNDERSTOOD rather than inherited:** VMsvga2's unlock/flush do
(almost) nothing because its backing IS device VRAM — the CPU
and the device see the same bytes. Ours must move pixels because
there is no BAR; the blit-to-FB-backing is the virtio-gpu
equivalent of what SVGA2 gets for free. This is the load-bearing
difference for every selector from the lock onward.

**Notes carried into the lock+flush boot (must not be lost):**
isRegionEmpty semantics — the 0x1 pair-member MUST stay a
no-op; the flush blit clips to the shape rect and copies
row-by-row (strides independently determined); the MISMATCH
self-check formula fix (`offset + (h−1)*stride + w*bpp`);
bpp=0 depths still refuse (NotReady).

**Session close-out state:** master at 556d322 (ledger tip;
lock rung at 6c801db, not installed anywhere); guest on ac16eac
/ 3d618e6f, desktop CONFIRMED back. Next unit = task #2: land
lock+flush together, one pre-registered boot.

---

## 2026-08-16 — WriteLock-rung boot RUN (6c801db / 51a227aa): every prediction landed; the cycle now runs [9,9,11,14,15,10]; next held line is Flush

Kext loaded and linked at boot (kxld accepted the new
descriptor/mapping calls — target precedent held; `kextutil -n -t`
had also passed pre-boot, though it is not the arbiter). md5
verified host↔guest. Cacheless boot again (~2 min this time).

**Predictions vs outcomes:**
1. ✅ **Exactly ONE allocation for the whole boot:** "backing
   ALLOCATED 7057408 bytes (extent 1680x1050 stride 6720), mapped
   at 0x1027a0000 in owning task." (Pre-registration said
   ~7,075,840 — arithmetic slip in the prediction; actual is
   1050×6720=7,056,000 page-rounded to 1,723 pages = 7,057,408.
   The code's formula is right; the pre-registered number was
   wrong. Recorded as such.)
2. ✅ **Both halves:** index 15 (WriteUnlock) fired for the FIRST
   TIME — 63×, all Success — and the storm relocated to index 10
   (Flush) — 63×, all Unsupported. **New cycle: [9,9,11,14,15,10]
   × 63.** Per iteration: shape pair → QueryLock → WriteLock →
   WriteUnlock → Flush(refused). Flush was the pre-registered
   next-selector guess and it is confirmed by the log.
3. ✅ QueryLock 63/63 Success, CannotLock 0: every lock is
   followed by its unlock within the iteration — the caller
   (WindowServer) has clean lock discipline; no sustained-hold
   case occurred.
4. ✅ No NoMemory. **MISMATCH fired 5× — FALSE POSITIVE,
   diagnosed:** my self-check used `offset + h*stride > len`,
   which is `(y+h)*stride + x*bpp` — over-strict by a partial
   row. Hand check of the worst case (offset=6422624 → y=955,
   x=1256; 80×95): last byte = offset + (h−1)*stride + w*bpp =
   7,054,624 ≤ 7,057,408 — the handout FITS. No caller ever
   exceeded the mapping. Fix (one line, next mechanical commit):
   compare `offset + (h−1)*stride + w*bpp` against the mapping
   length.
5. ❌ **Outcome #3 FIRED — visual check: the boot came up to
   a BLUE SCREEN, not a desktop.** The "desktop alive" line first
   written here (load + 1 login + early refreshDisplay) was WRONG
   — alive ≠ pixels, the exact distinction the rules demand; it
   is corrected here. **Mechanism (diagnosis, consistent with all
   observations):** WriteLock success made WindowServer switch
   its compositing destination to our surface; pixels land in the
   7 MB guest-side mapping with no host transfer (Flush refused),
   so nothing paints. The one refreshDisplay (09:54:28) was the
   early software path; the blue screen is its residue. Storm
   DECAYS rather than storms: 63 iterations in the first ~2 min,
   only 5 more in the following ~18 min (10:14 tail) —
   WindowServer backs off from a persistently failing flush.
   Compositor alive throughout (no hang — bSkipWriteLockOnce
   hazard did not materialize; no 0x5 shape seen).
- bSkipWriteLockOnce never fired (no options==0x5 shape observed
  this boot either; the guard sits armed and untested by traffic).

**RULE UPDATE from this boot — the lock rung cannot land alone.**
Success at the lock CHANGES WHERE WindowServer draws; a lock
without a working flush is a broken-window state by construction.
The rung granularity was too fine here: **WriteLock and Flush
must land as a PAIR** (one mechanism: "surface becomes
presentable"), with the pre-registration covering both and the
intermediate one-selector-boot discipline applying below the
pair. Recovery for the current boot was pre-registered as
"revert to ac16eac" — executed (see next
entry).

**Handout decode sanity (offset arithmetic verified against
known on-screen geometry):** off=6536 → x=1634 (the clock strip);
off=0 with 1680×22 (menu bar) and 1680×1050 (full screen); off
values consistent with y×6720+x×4 throughout. The address
0x1027a0000 is a WindowServer-task address (userspace range), not a
kernel pointer.

**Next rung — Flush (selector 10), the first rung where pixels
can move.** It is `(framebufferMask, options)` ScalarIScalarO
(2,0) per the worked example :86. In the damage-region model this
is the present/push step: CPU-written pixels in the mapped buffer
need to reach the host via the virtio-gpu 2D path
(TRANSFER_TO_HOST_2D on a resource backed by our mapped buffer —
the ATTACH_BACKING machinery finally re-enters here, in the
forward direction). Design step first: read the worked example's
flush contract, then pre-register. Same discipline: one selector,
one boot.

---

## 2026-08-16 (WriteLock-rung boot pre-registrations, before build 51a227aa — SPLIT commit 2 of 2)

**Build 51a227aa contains ONLY the lock mechanism**, on the
boot-verified geometry: lazy grow-only backing
(IOBufferMemoryDescriptor inTaskWithOptions KernelUserShared |
Pageable | InOut, page-rounded, createMappingInTask(m_owning_task)
— idiom :1111-1128), handout `base + shape_y*stride +
shape_x*bpp` with rowBytes = the ALLOCATION's stride (base_w×bpp,
NOT width×4), width/height = current shape bounds,
colorTemperature[0]=0x1CCCC; bSkipWriteLockOnce guard armed at
set_shape(wID==1, options==0x5) and honored in the lock;
WriteUnlock = clear bit only (backing persists, :1276-1281);
QueryLock now reports the REAL bit (stays honest now that a lock
can be held); teardown in stop() releases map+descriptor even
mid-lock. Boot-risk note: `page_size` and the descriptor/mapping
calls are new kernel API surface for THIS kext — evidence is the
worked example running them on this OS (target precedent), boot
is the arbiter, revert = one step to ac16eac.

**Pre-registered predictions:**
1. First WriteLock after the early full-screen shape logs
   "backing ALLOCATED ~7,075,840 bytes (extent 1680x1050 stride
   6720)" (~6.75 MB) and → Success with a userspace address.
   Exactly ONE allocation for the whole boot (no later shape
   exceeds the boot's max extent 1680×1050; 869×860 etc. all
   fit).
2. The [9,11,14] storm STOPS or relocates: the loop was driven
   by WriteLock failing. First-ever dispatches of index 15
   (WriteUnlock) expected after the first Success — then either
   the caller goes quiet (loop stopped on success) or the storm
   relocates to the next unsucceeded selector (prediction:
   Flush=10, per the damage-region model's fill→unlock→flush
   order; SetScale=8 or Control=16 would be findings about the
   sequence).
3. QueryLock flips honest: Success while unlocked, CannotLock
   between a WriteLock and its WriteUnlock. A SUSTAINED
   CannotLock storm means the caller locks without unlocking —
   discriminator for the caller's discipline.
4. No MISMATCH line (offset + h×stride within the mapping —
   arithmetic self-check); NoMemory would mean the extent
   tracking is wrong.
5. **Outcome #3 watch is ACTIVE (caller is WindowServer, this is
   the memory rung):** after first Success the desktop either
   stays visually normal (visual check) or freezes/blacks — that
   would mean WindowServer switched its compositing source to
   this surface with no host transfer behind it. A hang (not a
   crash) is the bSkipWriteLockOnce deadlock class. Recovery:
   revert to ac16eac, one step.

---

## 2026-08-16 — geometry-fix boot RUN (ac16eac / 3d618e6f): predictions 1,3,4 confirmed; prediction 2 half-falsified — the shape stream is DAMAGE REGIONS, and the prior "full-screen" claim is corrected

Installed cacheless by decision: `kextcache -system-caches`
exited 0 but wrote NOTHING to `Startup/` (twice) — unexplained
residual, tracked below; the blessed cacheless dev configuration
was taken instead (Startup/ asserted empty, kext md5 verified in
/S/L/E, root:wheel 755). Cacheless boot took ~7 min under TCG;
kext loaded fine — the stale-cache failure class is absent in
this mode by construction.

**Predictions vs outcomes:**
1. ✅ Caller-visible behavior IDENTICAL: cycle 7 ×1 →
   [9 ×118, 11 ×59, 14 ×59]; QueryLock Success 59; WriteLock
   Unsupported 59. Split premise holds.
2. ✅/❌ Empty-region no-op works: 59 × "empty region … no-op",
   exactly one per iteration, arriving AFTER the WriteLock attempt
   (iteration order: real-shape → QueryLock → WriteLock →
   empty-shape). **FALSIFIED half: "stored geometry stays
   1680×1050" — full-screen was stored only 3×, all early in
   boot.** The IdentityScale stream over the boot: 80×95 ×19,
   46×22 ×6, 52×62 ×4, 64×64, 1680×22 ×3, 98×22, 218×22, 58×58,
   154×88, 1332×804/860/760, 869×860, 1419×95, 1155×104, 2×3,
   1680×14, 1680×1028 … — 59 stores, one real region per
   iteration.
3. ✅ SetIDMode logs `depth=0x4 bpp=4 [WindowServer surface]`.
4. ✅ Cycle unchanged → the 0x1 call meant nothing beyond its
   return code to the caller.

**CORRECTION of the 29ab557c entry above:** its "geometry went
full-screen" finding was a HEAD-OF-LOG SAMPLING ARTIFACT — the
first iterations after GATED ON are full-screen
(loginwindow/desktop setup), then the stream becomes small
window-sized damage regions. The 645fa708 boot's "clock strip"
was the same phenomenon seen mid-stream. The shape stream was
heterogeneous ALL ALONG on every boot; only the sample points
differed. Lesson recorded: tally the whole boot's geometry
distribution, not the first lines — this is the second time
head-of-log reading produced a wrong generalization.

**What the shape stream actually is (inference, now
well-grounded):** per-dirty-region shape-before-lock — the
worked example's traffic model. For wID==1 its bFromGFB branch
(:1649-1654) treats the surface as the SCREEN-SIZED buffer:
buffer = shape bounds (the sub-region), source = screen dims,
rowBytes = SCREEN stride (`m_screenInfo.client_row_bytes`,
calculateScaleParameters :408), and
`calculateSurfaceInformation` adds
`bounds.y*rowBytes + bounds.x*bpp` to the handed-out address
(:390, reserved[2] at :417). **Commit-2 design consequence:** our
lock must keep a GROW-ONLY screen-extent allocation (max
bounds.x+w, max bounds.y+h ever stored — the boot's early
1680×1050 establishes it), report rowBytes = the allocation's
stride (base_w × 4), and hand out
`base + shape_y*rowBytes + shape_x*bpp` with width/height = the
CURRENT shape's bounds. Structural fields needed: shape x/y
(bounds origin) and base extent, not just w/h.

**Residual (unexplained):** kextcache exit=0 with empty Startup/
on this boot's install. Prior installs produced 9-10 MB mkext.
Possible: kextd dependency, volume state, or flag behavior
change. Not investigated — cacheless is the current dev mode;
revisit before any boot where cache behavior matters.

---

## 2026-08-16 (geometry-fix boot pre-registrations, before build 3d618e6f — SPLIT commit 1 of 2)

**The isRegionEmpty finding is a real storage bug** (read from the
worked example, :136-144/:1607/:1639): our SetShape stored
bounds.w/h from EVERY call, so the observed 0x1 pair-member
(bounds 1×1, rect 0×0) overwrote 1680×1050 with 1×1 twice per
cycle — invisible until now because nothing read the stored
geometry. A lock implemented on top would have allocated backing
for a 1×1 surface. Decision: land the geometry fix FIRST as
a mechanical change with the lock still Unsupported, then the lock
rung lands on known-correct geometry — if the lock boot goes
wrong, geometry is already excluded.

**Build 3d618e6f contains ONLY:** empty-region no-op (rect[0]
degenerate → Success, geometry untouched — :1607), IdentityScaleBit
(0x4) gate on geometry storage (:1639-1655), bpp derivation at
SetIDMode (0x24 → depth 0x4 → 4 bpp, stored; unknown depths store
0 and nothing reads it yet), dead code removed (vram_address
field, three never-defined helper declarations). No returns
changed, no memory claimed, no new selectors real.

**Pre-registered predictions:**
1. Caller-visible behavior IDENTICAL to the 29ab557c boot: cycle
   7 → [9,9,11,14], QueryLock Success, WriteLock Unsupported, same
   storm rate. The only deltas are log lines — that is the point
   of the split.
2. New log lines: 0x1 member logs "empty region … no-op"; 0xd
   member logs "IdentityScale: geometry STORED (1680x1050)"; the
   stored pair-stable geometry is 1680×1050 across the whole boot.
3. SetIDMode logs bpp=4.
4. If the cycle CHANGES shape, the split's premise is wrong — the
   0x1 call meant something to the caller beyond its return code.
   (Cannot see how: it returned Success before and after.)

---

## 2026-08-16 — write_lock contract read (design step, zero code); loose end closed: geometry change is same-client

**Loose end closed first (from the same kernel.log, zero boots):**
GATED ON (client-creation) count = **1 in boot B (645fa708, clock-strip
geometry) and 1 in boot C (29ab557c, full-screen)** — one surface
client per boot, zero destroy/close lines. The geometry change is
the SAME WindowServer client re-shaping across the rung; a
different-client explanation is ruled out. "Shapes full desktop
once lock-availability succeeds" remains an inference on causality,
but client-identity coincidence is not available as an out.

**WriteLock contract, read from the worked example** (all line
numbers `~/VMsvga2-modern/AC/UC/VMsvga2Surface.cpp` unless noted;
headers in MacKernelSDK):

1. **What comes back — `IOAccelSurfaceInformation`**
   (IOAccelTypes.h:56-70): `mach_vm_address_t address[4]`,
   `UInt32 rowBytes, width, height`, `UInt32 pixelFormat`,
   `IOOptionBits flags`, `IOFixed colorTemperature[4]`,
   `UInt32 typeDependent[4]`. Size taken as `sizeof` at build
   time — do not hand-compute. Table row for index 14 is
   `kIOUCScalarIStructO, 0, kIOUCVariableStructureSize` (0 scalars
   in, variable struct out — driver fills it); plain
   `surface_write_lock` (:1481) forwards to `_options` with
   `kIOAccelSurfaceLockInAccel`. Fields the worked example fills
   (`calculateSurfaceInformation` :377-396): `address[0]` (+ buffer
   byte offset), `width`, `height`, `rowBytes` (= source pitch),
   `pixelFormat` (stored mode), `colorTemperature[0]=0x1CCCC`
   ("from GeForce.kext"); everything else stays bzero'd.
   Validation order (:1246-1256): `*infoSize < sizeof` →
   kIOReturnBadArgument; `!bHaveID || !isSourceValid()` →
   kIOReturnNotReady; already locked → kIOReturnCannotLock.

2. **Direction — driver creates the mapping in the OWNING task.**
   The design-flaw comment (:1089-1098) states it: a locked
   CGSSurface address must be valid *inside the WindowServer* (the
   owning task), not the requesting app. Mechanism (idiom at
   :1111-1128, real path `allocBacking` :531 / `mapBacking`
   :569): `IOBufferMemoryDescriptor::inTaskWithOptions(0,
   kIOMemoryKernelUserShared | kIOMemoryPageable | kIODirectionInOut,
   size, page_size)` then `createMappingInTask(m_owning_task, 0,
   kIOMapAnywhere)`; on SVGA2 the preferred backing is **VRAM**
   (`m_provider->VRAMRealloc`, mapped via `mapVRAMRangeForTask`)
   with GMR fallback. **NOT clientMemoryForType /
   IOConnectMapMemory** — the address travels inside the struct.
   **Decision consequence: this is the map-into-WindowServer
   direction, the opposite of ATTACH_BACKING, and is machinery
   this project has never exercised.** A client-provided branch
   exists (`set_shape_backing` → `m_client_backing.addr` returned
   as-is :385-386) — that would reuse ATTACH_BACKING-direction
   thinking — but our observed caller never dispatches selector 6.

3. **Unlock / lifetime:** `surface_write_unlock_options` (:1276-1281)
   = `OSTestAndClear(lock bit); return Success`. **Nothing is
   unmapped.** Backing is lazily allocated at FIRST write lock
   (:1258-1263: `if (isClientBackingValid()) goto finishup; if
   (!allocBacking() || !mapBacking(m_owning_task, 0U)) →
   kIOReturnNoMemory`), persists across lock/unlock cycles, torn
   down only at surface destruction (`releaseBacking` :590).
   Architectural note for our port: on SVGA2 the backing IS device
   memory (CPU and device see the same bytes), which is why unlock
   needs no transfer. Virtio-gpu has no guest-CPU-mappable VRAM,
   so for us CPU-written pixels need a later CPU→host push
   (TRANSFER_TO_HOST_2D family) — a LATER rung, not part of
   WriteLock success. Lock also calls `vtb.sync` (:1270-1271) when
   backing is not packed — device-DMA drain before CPU writes;
   no-op for our first version.

**Watch item from the same read — UPGRADED to required behavior
(** `bSkipWriteLockOnce`
(:1251-1253, set at :1658-1666) — set_shape with `m_wID==1 &&
options==0x5` makes the NEXT write_lock succeed WITHOUT taking
the lock bit, as the fix for a **10.6 WindowServer deadlock
during Window Grab** (comment notes Window Grab works on 10.7 and
asks what changed). **Our surface IS wID==1 — the exact guarded
case.** This is not a wart to omit as a hack: leaving it out
reintroduces a compositor hang, outcome #3 territory on the rung
where a mistake is destructive rather than merely unsuccessful.
Our observed set_shape options so far are 0xd/0x1 (0x5 not yet
seen), but the guard goes in with the same condition regardless.

**Two further design obligations (same session):**
1. **Lazy-create-and-persist has a teardown obligation.** The
   mapping outlives unlock, so it must be released at surface
   destroy AND at clientClose/free — specifically including the
   client dying while holding a lock (WindowServer restarting
   mid-lock is a real case). Same leak shape as the
   backing-descriptor table, where the fix was cleanup on client
   teardown regardless of what the caller did.
2. **The two fields to get exactly right in
   calculateSurfaceInformation-equivalent:** `address` must be the
   CLIENT-TASK address from the mapping — never a kernel pointer —
   and `rowBytes` must be the ALLOCATION's actual stride, not
   width×4 by assumption. A wrong stride means the compositor
   writes past what was mapped — the destructive version of the
   GL_UNPACK_ROW_LENGTH bug. Invariant to self-check in code:
   mapped_size ≥ height × rowBytes.

**Next unit:** implement WriteLock per this contract — minimal
honest version: `IOBufferMemoryDescriptor` (inTaskWithOptions,
KernelUserShared) sized `width*height*bpp` rounded up to page,
`createMappingInTask(m_owning_task)`, fill the five fields,
Success. No host transfer, no client-backing branch. Own commit,
own boot, pre-registered prediction written before it.

---

## 2026-08-16 — QueryLock boot RUN: prediction confirmed; cycle now 7→[9,9,11,14]; caller is WindowServer; geometry went full-screen

Build 29ab557c (commit 111adaf) installed — md5 host↔guest match
verified before analysis — boot 08:39:47. **kernel.log spans three
boots**; the 00:30/00:41 QueryLock-Unsupported lines belong to the
645fa708 boot and must not be counted (caught by timestamp before
tallying; current boot isolated from its `Darwin Kernel Version`
line at log line 10527).

Every pre-registration from the entry below landed:

1. **Cycle advanced past QueryLock to a real lock — index 14,
   handler self-names WriteLock.** Tally for the observed window
   (08:41:20–08:43:15): index=9 ×94, index=11 ×47, index=14 ×47,
   index=7 ×1, **index=12 ×0**. Per iteration: 2× SetShape (the
   0xd/0x1 pair), 1× QueryLock, 1× WriteLock. The caller goes
   straight to WriteLock; the other lock-family member never fires.
2. **Held line held:** WriteLock → Unsupported 47/47; QueryLock →
   Success (never locked) 47/47, no other return observed.
3. **Storm relocated and still running:** ~47 iterations in ~2 min
   (~0.4/s; prior boot ~1.4/s over a longer window — windows
   differ, do not compare rates), dispatches still present in the
   log tail at 08:46. Fails-more-politely ≠ stops, as tempered.
4. **Outcome #3 watch — desktop alive AND visually normal:** load
   9.43 (3 min) → 3.05 (7 min), 1 login; the display confirmed
   renders normally through 47 QueryLock successes and 94 shape
   cycles (visual check, same session).

New data beyond the pre-registrations:

- **Caller attributed: `"IOUserClientCreator" = "pid 98,
  WindowServer"` on the live surface client** (ioreg, this boot,
  zero code). Resolves the 2026-08-15 prediction-1 fork to (a)
  WindowServer — so, per that pre-registration, **outcome #3
  ("working software path breaks") is ELEVATED from background
  watch to active concern**: every future selector success is now a
  live experiment on the compositor, and each must ship with its
  own desktop-health check.
- **Geometry changed with the rung:** the 0xd SetShape region is
  now **full-screen 1680×1050 at (0,0)**; the 645fa708 boot's 0xd
  was the 46×22 clock strip at x=1634. Same pair structure
  (0xd real + 0x1 1×1/zero-rect), different content. Inference
  (unverified): WindowServer shapes the full desktop surface only
  once lock-availability succeeds; the clock strip was an
  early-exit shape. What would settle it: the worked example's
  sequence around first write-lock, or a probe replay against both
  builds.
- rgn pointer differs call-to-call (0xffffff800ee03490 →
  0xffffff800f4c3490) — per-call allocation by the caller's
  allocator; no leak signal either way.

Hygiene note: the "Lock Configuration / Recursive Locking /
Priority Inheritance / Deadlock Detection / Lock Timeout" block at
08:40:34 is ours — FB/VMTextureManager.cpp:397-401, part of the
vestigial manager init (hygiene list item 1). Not new this boot.

**Next unit — the backing rung: make WriteLock honest.** Design
step first (its own session unit): read the worked example's
write_lock path in VMsvga2Surface.cpp for its return contract —
what the caller receives on success (pointer? size? flags?
io_connect scalar/struct outputs?) — before writing ours; the
standing design note is ATTACH_BACKING-reuse. Pre-register the
prediction when the design is committed. Do NOT bundle: no other
selector moves in the same boot.

---

## 2026-08-15 (step 1 pre-registrations, before the attribution boot)

Build `2a310eab389410efa90a92d3ec9038a9` adds per-creation client
attribution: pid+name derived from the OWNING task
(`get_bsdtask_info(m_owning_task)` → `proc_pid` → `proc_name`; the
task-argument distinction from `withAddressRange`), logged on EVERY
client creation. All three symbols verified exported by the guest's
actual `/mach_kernel` (`nm -g`, 2026-08-15); `get_bsdtask_info`
declared in-source (absent from available headers). Pure
observation — every selector still returns Unsupported.

**Prediction 1 (the 15:59:43 caller's identity):** the boot-time
external caller is either (a) WindowServer — in which case making
any selector succeed becomes a live experiment on the compositor
and outcome #3 ("working software path breaks") is elevated from
background watch to active concern; or (b) another system consumer
(loginwindow / SystemUIServer / Dock class) — step 2 stays a
tool/agent-context experiment and the compositor question remains
open. Discriminated by the new log line on the next boot; nothing
else in the build changed.

**Prediction 2 (step 3's expected next selector, written now per
VMsvga2's map):** once `SetIDMode` returns success, Apple's
consumer sequence calls a **set_shape-family selector** next
(`SetShape`/`SetShapeBacking`/`SetShapeBackingAndLength`). If it
calls something else entirely, that is a finding about the
sequence, visible only because this expectation was written first.

## 2026-08-16 (QueryLock boot pre-registrations, before build 29ab557c)

**Mechanical-alignment-only boot observed first (645fa708):** cycle
UNCHANGED — 7 → [9,9,11] × repeat, 110/55/1, zero on every other
selector. Attribution clean: the aligned table alone moves nothing;
QueryLock's Unsupported is the sole gate.

**This build (29ab557c, committed before boot): QueryLock →
kIOReturnSuccess** — the worked example's honest answer for a
never-locked surface (state report; claims no memory).

**Pre-registered:** the cycle advances past QueryLock to the real
locks (12/14) — the held line, Unsupported until backing exists.
The storm relocates to the lock rung. If instead the caller does
something NEW (backing first? surface_control?), the log names it.
Expectation tempered: fails more politely ≠ stops; the
loop stops only when something succeeds, and the next success is
on the wrong side of the held line. Desktop watch continues.

## 2026-08-16 — alignment boot: struct transports; geometry real; next rung is QueryLock (log-named, not guessed)

**Boot results (ece17314):**
```
SetShape(options=0xd fbIndex=0 rgn=0xffffff800df65e90 size=20)
  region — num_rects=1 bounds=(x=1634 y=0 w=46 h=22) rect[0]=same
SetShape(options=0x1 …) region — num_rects=1 bounds=(0,0,1,1) rect[0]=(0,0,0,0)
```
- Struct transported: size=20 = IOACCEL_SIZEOF_DEVICE_REGION(1) exactly;
  p4-by-value confirmed; sanity gates all passed; zero "[size impl?]".
- **Geometry is real, decodable WindowServer content**: the 0xd region
  is the menu-bar clock strip (46×22 at x=1634 — top-right of the
  1680-wide display, exactly the pre-registered sanity check). The 0x1
  pair-member is the 1×1/zero-rect empty variant (known-real family).
- **The storm moved and grew**: 747 SetShape (~1.4/s), shape
  SUCCEEDING each time; locks fired only 3× (prior boot's tail).
- **Next rung read from the log, not guessed** (correction —
  every dispatch logs index=N): the cycle is 7 → [9, 9, 11] × repeat.
  **Index 11 = QueryLock** — 56 dispatches; set_shape_backing (6) and
  real locks (12/14): ZERO hits. The caller probes lock AVAILABILITY
  after shaping; both prime-suspect candidates (flush, backing) were
  wrong — the log named it for free.
- QueryLock semantics from the worked example (:1461-1466): pure state
  query, answer IS the return code — kIOReturnCannotLock if locked,
  **kIOReturnSuccess if lockable**. Our never-locked surface makes
  Success the honest, deliverable answer; current Unsupported reads as
  "cannot even ask."
- Desktop healthy, load DOWN to 2.0 (from 7.8). Outcome #3 clean.

**Mechanical commit (645fa708, source committed before boot): ALL
remaining table rows aligned to the worked example** — 0,3:
StructO(1,var); 5: StructI(0,var); 6: StructI(4,var) [the backing
rung]; 8: StructI(1,var); 10: ScalarO(2,0); 12,14: StructO(0,var);
16: ScalarO(2,1); 17: StructI(5,var). Rows 1,2,4,7,9,11,13,15 already
correct. Handlers untouched (all still return Unsupported without
reading args — signature risk deferred to each row's own rung; slot
meanings documented in-table). **No semantic changes bundled.**

**Next (separate semantic commit, its own boot): QueryLock →
kIOReturnSuccess** (honest state answer). Pre-registered: the cycle
moves past QueryLock to the real lock (12/14) — the held line — and
the storm relocates there; loop stops only when something SUCCEEDS
(fails more politely ≠ stops). Expectation tempered.

## 2026-08-15 (SetShape-alignment boot pre-registrations, before build ece17314)

**Arg decodes verified from the worked example source** (not
inference): `set_id_mode(wID, modebits)` — VMsvga2Surface.cpp:1304;
wID==1 = WindowServer's surface (:1309 comment; also gates
createPrimaryScreen behind haveFrontBuffer — our unconditional
success there noted as an unbacked instance). First boot's call
was wID=1, modebits=0x24; the "mode=0x1" log was MISLABELLED,
store corrected (surface_id=wID, pixel_format=modebits).
`set_shape(options, fbIndex, rgn, rgnSize)` — :1374; the 0xd/0x1
pair = shape-bit OPTION configs, not window IDs (corrects the
window-pair reading). Both recorded as corrections with sources.

**MIXED outcome confirmed comprehensively:** the worked example's
table (:73-93) shows most of our rows declare the wrong shape —
the shape family is ScalarIStructI with a variable
IOAccelDeviceRegion struct-in (ours said ScalarO 2,0 — the
caller's region bytes were DROPPED at the MIG boundary; a3's
stable per-client kernel pointer was the dispatcher's slot, and
the retry storm is the caller failing to place its surfaces).

**This build (ece17314): ONE row aligned** — index 9 SetShape →
ScalarIStructI(2, variable), handler signature matched
(options/fbIndex/rgn/rgnSize), region parsed under guards
(kernel-canonical rgn; size sane; p4 logged raw both readings —
by-value per the worked example, no blind deref), geometry
STORED, success returned. All other rows untouched (one variable
per boot). Locks remain the pre-registered held line — the
memory-claim rung fires immediately after shape success at
storm rate, and the decision stands: Unsupported until a real
backing exists (ATTACH_BACKING-reuse design, its own unit).

**Pre-registered predictions:**
1. SetShape calls now carry a parseable region: num_rects +
   bounds logged; full-screen surfaces ≈ 1680×1050.
   num_rects==0 is a KNOWN real case (VMsvga2 fixup), not
   failure; "struct didn't arrive" = null/non-kernel pointer or
   absurd size.
2. If the struct transports correctly and shape succeeds, THE
   STORM STOPS (cleanest confirmation); the next call family
   (locks) appears and is refused by the pre-registered line.
3. p4's size reading resolves: by-value matching the worked
   example (if not, the sanity gate logs it and shape returns
   Unsupported — no wrong success).
4. Outcome #3 watch continues: desktop alive through shape
   success at storm rate.

## 2026-08-15 (SetIDMode boot pre-registrations, before build a0788488)

Changes in `a0788488fc31b5c5da4f0ac747e1c3c0` (source committed
before boot): SetIDMode returns SUCCESS and stores its mode
argument (first real selector — documented kIOAccelSurfaceMode*
constant, honest claim); shape-family handlers log RAW ARGS while
staying Unsupported; IOUserClientCreator logged at TWO sites
(start() and first externalMethod dispatch) per the timing
caution — early-empty at one site and populated at the other is
the TIMING DATUM, not "no creator".

**Pre-registered predictions:**
1. readfb gets PAST IOAccelCreateSurface (SetIDMode succeeds) and
   fails at its NEXT call — prediction: a set_shape-family
   selector, whose raw args now land in the kernel log.
2. The boot-time surface-client caller (n=2 so far) walks past
   SetIDMode too, on the same boot.
3. creator@dispatch names a process for at least the readfb
   client; creator@start may read empty (timing datum if so).
   **Decision already made for the rung after (fixed before this
   boot's log returns): shape-family stays Unsupported regardless
   of what fires — argument semantics unread, backing selectors
   claim memory. The raw args this boot logs are what the NEXT
   decision gets based on.**
4. Outcome #3 watch: desktop/software path must stay alive
   through callers receiving SetIDMode success.

## 2026-08-15 (latest) — re-land boot: rung 1 REPRODUCED from committed source (provisional lifted); caller n=2; hygiene list from the boot log

**Predictions vs outcomes (build 84e4b177, commit f551fba):**
1. **Trio published, rung 1 PASSED — 2a result UNPROVISIONALIZED.**
   Boot log (serial + kernel.log): `IOAccel trio published —
   IOAccelTypes="IOService:/AppleACPIPlatformExpert/PCI0/AppleACPIPCI/
   S10@2/VMVirtIOFramebuffer/VMQemuVGAAccelerator" Index=0
   Revision=2`; ioreg carries the same string on the FB. readfb:
   `STEP FAILED at IOAccelCreateSurface err=0xe00002c7` — i.e.
   IOAccelFindAccelerator passed, failure is our own SetIDMode
   stub (kernel: 19:49:01 GATED ON → SetIDMode → Unsupported).
   Same rung, same attribution, now from reconstructable source.
2. **Boot-time surface-client caller REAPPEARED (n=2):** 19:46:43
   GATED ON + SetIDMode — on the trio boot, absent on both
   trio-less boots. A system consumer uses the IOAccel-API path,
   trio-gated. Identity still unattributed (client transient;
   ioreg sample missed it). Sequence datum: WindowServer's
   FRAMEBUFFER type-0 at 19:46:09, 34 s before the surface-client
   open.

**Hygiene list additions (noted from boot log; traced to
source — observations, not yet fixed):**
1. `"3D managers initialized for QXL/Hyper-V DDA mode"`
   (VMQemuVGAAccelerator.cpp:198, branch at :177): the comment
   says "only for QXL/Hyper-V DDA mode" but the initialization
   runs UNCONDITIONALLY in start() — dead-weight init on every
   variant, not variant detection (no other branch keys off it;
   the managers are the fb669ac-era vestigial machinery nothing
   consumes).
2. `"Metal framework not available (macOS < 10.11)"` (:206-227):
   a kext citing a userspace framework's availability via a
   preprocessor gate — meaningless in kernel context; Metal is on
   the do-not-cite list; the VMMetalPlugin block is already
   recorded as an unbacked claim on every target.
3. `"RendererID=0x00024600"` (:262): constant with NO artifact
   provenance — comment says "Generic renderer ID for virtual
   GPUs," nothing more. Advertised to CGL. The only renderer-ID
   datum verified on this system is 0x1020400 (software renderer,
   from the A2 census). A renderer ID is a claim; this one is
   unbacked.
4. **Found while tracing — the claim machinery**
   (VMQemuVGAAccelerator.cpp:262-284): beside RendererID, the
   accelerator publishes `IOAccelTypes=7` NUMERIC (:265 — the
   personality-diff finding-1 shape; conflicts with the FB's new
   path-string publication; if CGS parses it as a path the
   failure is silent), `IOGLAccelTypes=7`, `IOSurfaceAccelTypes=7`,
   `IOVideoAccelTypes=7` (:266-268 — video decode/encode
   acceleration claimed, nothing implements it), the
   "Framebuffer/3D/Hardware" IOAcceleratorTypes array (:277-283),
   and PerformanceStatistics/Accum=true (:273-274). d-era comments
   throughout — historic-implementation category. This block is
   the concrete home of "advertising a 3D role while providing no
   path for either 2D or 3D" : the discovery layer is
   shared, and NEITHER a GA CFPlugIn (2D) nor a GLD (3D) exists
   behind any of these properties.

## 2026-08-15 (later) — trio re-landed from session record; scoping REVISED; prior rung result provisional

**Working-tree loss discovered by git:** the 2a trio and the
capability flip were NEVER COMMITTED (`git log --all -S` finds
neither string; last commit on VMVirtIOFramebuffer.cpp is
fb669ac). A tree operation silently discarded them; the "restore"
then reproduced the MIG-era build (a147a911) and readfb regressed
to rung-1 failure — tree state, not system state. **Rule adopted:
commit before booting.** Evidence from uncommitted source is the
November-d98 shape — a binary nobody can rebuild — so **the 2a
rung result above is marked PROVISIONAL until reproduced from
committed source** (this re-land provides that source).

**Scoping revised by the registry observation (zero code, zero
boots):** on the trio-LESS restored build, ioreg showed a LIVE
`IOAccelerationUserClient` with `"IOUserClientCreator" = "pid 97,
WindowServer"` — **WindowServer's accelerator connection is
trio-INDEPENDENT.** The causal story splits:
- **IOAccel-API consumers** (readfb's class:
  IOAccelFindAccelerator → IOAccelCreateSurface → surface
  client type 0): trio-gated.
- **WindowServer:** reaches the accelerator directly (owns the
  framebuffer; observed holding the extCreate/extDestroy client
  class — "connects to the accelerator" observed, "drives
  surfaces through it" NOT).
- Correlation in hand: the boot-time surface-client caller
  appeared on the trio boot (15:59:43) and on NEITHER trio-less
  boot since (n=1 vs n=2) — consistent with that caller being an
  IOAccel-API consumer, probably NOT WindowServer.

**Trio re-landed as the ARBITER ENABLER** (build
`84e4b177705946ae4224cdeaca268ab4`, source committed before
boot): the readfb ladder is the only rung-walking instrument;
without the trio it fails at rung 1 permanently and the
measurement method is lost. Constants re-verified against
artifacts: kCurrentGraphicsInterfaceRevision=2 (10.6 SDK
header), getPath(char*, int*, gIOServicePlane) (kernel headers).

**Pre-registered predictions for the re-land boot:**
1. Trio log line fires with a path string; ioreg shows
   IOAccelTypes as that string on the FB; readfb rung 1 passes
   again (reproducing the provisional result from committed
   source — unprovisionalizes it). If rung 1 FAILS with the trio
   present, the string's shape is the first suspect.
2. The boot-time surface-client caller REAPPEARS (n=1 → n=2):
   evidence a system consumer uses the IOAccel-API path — making
   the trio more than readfb-scoped. If absent, the correlation
   weakens.

**Also recorded (2026-08-15, cost three boots):** kxld refused
get_bsdtask_info/proc_pid/proc_name, pid_for_task, AND
proc_selfpid/proc_selfname — all unresolved 0xdc008016. Root
cause read from the artifact: OSBundleLibraries declares
iokit/libkern/mach KPIs, NOT com.apple.kpi.bsd — every BSD
symbol unresolvable by declaration. Instruments falsified en
route: nm on /mach_kernel (symbol table ≠ kext-linkable set);
kextutil -n -t (passed builds kxld refused — boot is the only
linkage arbiter); nm on System.kext plugins (symbol-set kexts,
no binary). Rules updated with all four symbols named.
Caller-attribution answer arrived by the zero-code route:
IOUserClientCreator (64 live instances; ours = pid 97
WindowServer above). For transient surface-client callers: read
the property from inside initWithTask via plain
getProperty("IOUserClientCreator") + IOLog — pure IOKit, no KPI
risk — or ioreg while the client lives.

## 2026-08-15 — 2a RUNG: trio clears IOAccelFindAccelerator; first external call lands on selector 7

**Step 2a executed** (FB trio in path-string form, unconditional
publish, boot-log-asserted string; `IOCFPlugInTypes` deliberately
NOT published — that is 2b). Build on the sanctioned baseline
(`./build-enhanced_private.sh --unsigned`, md5
`5472512ae5d18d7cf830cc8c56c4d5fa`; the 10.6-ness comes from the
base `VMQemuVGA.xcconfig`, always applied — see rules). Probe
boot per protocol (fresh, cached, gate arg in plist).

**Trio verified in both artifacts:**
```
boot log: IOAccel trio published — IOAccelTypes="IOService:/AppleACPIPlatformExpert/PCI0/AppleACPIPCI/S10@2/VMVirtIOFramebuffer/VMQemuVGAAccelerator" Index=0 Revision=2
ioreg (FB node): IOAccelTypes = <that path string> / IOAccelIndex=0 / IOAccelRevision=2
```
Path string (not number), on the FB (not the accelerator) — both
halves of the old mistake corrected; the boot-log assertion
proves the write fired.

**After-trio rung result:**
```
readfb_pristine: rc=0, silent (unchanged)
readfb_steps:    STEP FAILED at IOAccelCreateSurface err=0xe00002c7
```
`IOAccelFindAccelerator` — the baseline wall — **PASSED**. The
trio is confirmed on the causal path by Apple's own consumer.

**Prediction outcome: failed EARLIER than predicted, and the
reason is ours.** Predicted failure at
`IOCreatePlugInInterfaceForService`; actual at
`IOAccelCreateSurface` — Apple's sequence creates the surface
(IOAccel C API) BEFORE instantiating the plugin, so the plugin
rung is downstream and unreached, not refuted. Kernel-log
attribution of the failure:
```
16:03:57 GATED ON → VMAccelSurfaceClient created   (readfb's open)
16:03:57 getTargetAndMethodForIndex index=7 → method 7 (count0=2)
16:03:57 SetIDMode -> Unsupported                   ← our hardened stub
16:03:57 Client closing / Stopping
```
**THE FIRST CALL WE DID NOT MAKE.** Apple's consumer opened our
surface client and invoked selector 7 (`SetIDMode`, 2-in — the
WindowServer-captured count). The consumer→accelerator direction
is now exercised end-to-end: trio → type-0 open (gate) → MIG
boundary (old-style dispatch serving a real caller) → handler.
`0xe00002c7` is our own designed return — the wall is our stub,
not the system.

**Milestone + next.** The pre-registered "intermediate decisive
evidence" fired, via readfb (better than a hand-written prober —
a failure in Apple's tool is evidence about the driver). Next
unit: make surface ops real, starting with `SetIDMode`
(surface pixel-format selection; constants from the enum dump:
8888=0x4, BGRA32=0xA …), then the ops Apple's sequence calls in
order (`SetShape*`, locks). Backing design per the pre-registered
insight: ATTACH_BACKING machinery — surface backed by a guest
allocation attached as a virtio resource; WriteLock returns its
pointer; Flush = TRANSFER_TO_HOST + scanout. 2b (`IOCFPlugInTypes`)
stays sequenced after the surface ops, as pre-registered.

**Precision notes  — do not over-read the
milestone:**
- **readfb is not WindowServer.** The calls are Apple's framework
  code, not mine — exactly the pre-registered evidence — but a
  tool I launched, not the compositor deciding on its own.
- **count0=2 on selector 7 is a second finding inside the
  first:** the count came from Catalina-era WindowServer captures
  and just validated against 10.6's MIG on a real caller. One
  selector is not the table, but it is the first evidence any of
  the table is right for this OS.
- **Caution basis corrected:** the November crash rationale
  describes a binary built from a path that isn't this
  repository's — citing it as danger is inference from lost
  evidence (guarded in the notes). The live caution is this
  codebase's own comment (`VMVirtIOFramebuffer.cpp:377-379`):
  advertise a capability you can't deliver and consumers stop
  falling back to the working path. Implementing selectors past
  SetIDMode is therefore UNTESTED territory with a plausible
  failure shape — not known-dangerous. Keep the boot-arg gate;
  watch the third pre-registered outcome ("working software path
  breaks") rather than expect it.
- **GLD closure, scoped:** this kext has no GLD, and Apple's
  accelerator framework still found the accelerator, opened the
  surface client, dispatched a selector — the surface path does
  not require a GLD plugin, closed from the artifact (VMsvga2
  showed it only at source level). Scope: the tool-initiated
  consumer path; WindowServer's own compositor decision remains
  open (see next datum).

**Second external caller (same boot, found while checking the
above):**
```
15:59:43 GATED ON → client created (task 0xffffff800b67bbc0)
15:59:43 index=7 → SetIDMode -> Unsupported → close
15:59:48 WINDOWSERVER REQUESTING FRAMEBUFFER MEMORY (startup)
```
One minute after boot — before any tool ran (readfb 16:03:57) —
a SYSTEM process found the accelerator via the trio, opened the
client, attempted surface creation: identical call shape to
readfb's, from a different task. Identity unattributed (task
pointer only); `proc_name` at client init is the cheap
instrumentation to name it next build. Desktop survived the
Unsupported (software path continued) — outcome #3 did not fire.

**Free control:** desktop alive through every run; the software
compositing path untouched.

---

## 2026-08-14 — readfb baseline RUN: fails at IOAccelFindAccelerator — as pre-registered

**The staged arbiter's step 0, executed.** Outcome exactly as
pre-registered: **inference converted to observation on the running,
unmodified system.**

### Build (host, 10.6 cross-compile)

- Tool: Apple's `IOGraphics/tools/readfb.c`, **byte-unmodified**, plus a
  copy with one `fprintf` per failure checkpoint (call sequence
  identical; observability only). Binaries: `/tmp/readfb_pristine`,
  `/tmp/readfb_steps` (host and guest `/tmp/`); instrumented source
  `/tmp/readfb_steps.c`.
- `xcrun clang -arch x86_64 -mmacosx-version-min=10.6 -isysroot
  MacOSX10.6.sdk -I /tmp/accelhdr … -framework IOKit -framework
  ApplicationServices -framework CoreFoundation`.
- Build shim (recorded, reproducible): `IOAccelSurfaceControl.h` is in
  no 10.6-era SDK header set — taken from the 10.2.8 SDK
  (`~/leopard-webkit-build/sdk/MacOSX-SDKs/`) into
  `/tmp/accelhdr/IOKit/graphics/`, extended with prototypes for
  `IOAccelSetSurfaceFramebufferShapeWithBackingAndLength`,
  `IOAccelWrite(Un)LockSurfaceWithOptions` — private API present in the
  10.6 IOKit binary (verified exported in both 10.6 SDKs' IOKit, 18
  IOAccel symbols) but declared in no header; signatures pinned by
  Apple's own call sites in readfb.c.

### Guest (sl@slqemu.local, 10.6.8 x86_64, kext 8.0.0d82 loaded, unmodified)

Registry ground truth, FB node (`ioreg -r -c VMVirtIOFramebuffer`):

```
"IOGLBundleName" = "GLEngine"
"IOGraphicsAccelerator" = No
"IOAccelIndex" = 0
"IOAcceleratorFamily" = No
```

**Absent from the FB node: `IOAccelTypes`, `IOAccelRevision`,
`IOCFPlugInTypes`** — the personality-diff finding, now observed live,
not inferred. (`IOAccelIndex=0` present, consistent with the
`has_3d_support` block.)

### Run (raw)

```
pristine: exit=0, 0 bytes (silent failure — Apple's tool reports nothing)
steps:    STEP FAILED at IOAccelFindAccelerator err=0xe00002bc
```

`0xe00002bc` = `kIOReturnNotFound`. **Rung 1 confirmed: the FB trio is
the gate**, by Apple's own consumer, on the current kext, no gate, no
driver change, no perturbation of anything WindowServer reads.

### Free control

WindowServer alive (`ps ax` match) during the run — the desktop path is
live by a non-accelerated route, so the failure is specific to the
accelerated path, not a broken display.

### Next rung (pre-registered)

After the FB trio lands in path-string form: re-run both binaries.
Prediction: past `IOAccelFindAccelerator`, fail at
`IOCreatePlugInInterfaceForService` (no `IOCFPlugInTypes`, no plugin).
The pair (baseline fail → post-trio pass-at-rung-1) is what
discriminates "IOAccel discovery works" from "WindowServer declines."

**The instrumented build is the instrument .** The
pristine tool fails **silently** — exit 0, zero bytes, no diagnostic.
The step-marker `fprintf`s are therefore load-bearing, not convenience:
every subsequent rung needs the same instrumented build and include
shim, and a future session reaching for stock `readfb.c` would get no
diagnostic at all. Artifacts (host): `/tmp/readfb_steps.c`, shim at
`/tmp/accelhdr/IOKit/graphics/IOAccelSurfaceControl.h`; rebuild recipe
in the Build section above.

---

## 2026-08-14 — Personality diff vs VMsvga2 (cross-tree read, no boot)

**Context.** VMsvga2-modern's ledger (Q3) established the worked example's
coupling machinery; this diff checks this project's accelerator visibility
against it. All findings are source reads of the live tree
(this checkout); nothing booted. Recorded in both ledgers.

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

**Probe vs shipping value — keep the distinction explicit (decision,
2026-08-14).** `functional_3d` was correctly held false while the 3D path
was unproven. That era is over in the per-process sense: Mesa renders
through virgl and PowerFox draws real web content (verified project
state). But WindowServer still cannot reach any of it — the shim is
per-process via `DYLD_FRAMEWORK_PATH`. So publishing accelerated=true to
WindowServer is a claim nothing behind it can yet honour. Acceptable for a
boot-arg-gated experiment; **the gate must not quietly become the default
later.** Record any flip of the published value as a probe in the boot
args used, and revisit what "functional" means for WindowServer once a
system-wide path exists.

**`functional_3d` is the pivot of the whole probe — do not flatten it
.** Its honest value depends on which consumer is asking:
per-process 3D is real, WindowServer-reachable 3D is not. That distinction
currently lives in **one boolean**, which is exactly the kind of thing that
gets flattened by someone tidying up. The boot-arg gate is what keeps the
distinction honest; any refactor that merges the probe gate into
`functional_3d` (or vice versa) loses the ability to say what "functional"
means, and for whom.

**Probe design, final .** The probe knowingly does what
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

**Probe-run boot protocol .** The probe is the first
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

## 2026-08-14 — GA plugin role read: absent here; readfb.c is Apple's own consumer reference

**Question.** Diff this project's GA plugin table against VMsvga2GA's.

**Verdict: there is nothing to diff — the role is unimplemented.** Grep
for `IOGraphicsAcceleratorInterface` / `kIOGraphicsAcceleratorTypeID`
across the tree hits only `IOGraphics/tools/readfb.c` (Apple tool) and
the vendored header. This closes the loop on personality-diff finding 2:
`IOCFPlugInTypes` is absent because **no plugin exists to advertise**.

- `GLPlugin/VMVirtIOGLEngine` is NOT the GA plugin and never was: it
  implements an invented `GLEnginePlugin` struct (8 hand-picked function
  pointers) — not the 92-entry `gld*` contract, not
  `IOGraphicsAcceleratorInterface`. Superseded 2026-08-09
  (`GLPlugin/SUPERSEDED.md`): Mesa + CGL shim chosen over hand-written
  GLEngine. Reference-only.
- `IOGraphics/` is a vendored Apple IOGraphics 1.5.1 source tree, one
  commit, unmodified ("for reference").

**VMsvga2GA's table vs Apple's struct** (authoritative layout verified
identical in 10.6 SDK, 26.5 SDK, and vendored IOGraphics 1.5.1): all
IUnknown + IOCFPlugIn base slots filled; `Reset`, `CopyCapabilities`
('smvl' only; 'cgls' compiled out), `Flush` (near no-op), `Synchronize`,
`GetBeamPosition` (returns 0), `AllocateSurface` (CGSSurface path only),
`FreeSurface`, `LockSurface` (+`accessFlags=2`), `UnlockSurface`,
`SwapSurface`, `SetDestination`, `GetBlitter` (Fill/Copy/CopyRegion),
`WaitComplete` filled; **`GetBlitProc` and `WaitForCompletion` — the two
`IOGA_COMPAT` named slots — left NULL**; reserved array is **[24]** (the
"22" comment in VMsvga2's source is a miscount), with `[0]=WaitSurface`,
`[1]=SetSurface` load-bearing and `[2..23]` NULL.

**`readfb.c` — Apple's own consumer, sitting in this repo's tree — is
the canonical client sequence and a ready-made template for the probe:**

1. `IOAccelFindAccelerator(framebuffer, &accelerator, &fbIndex)`
2. `IOAccelCreateSurface(accelerator, surfaceID, Windowed|8888, &connect)`
   — the IOAccel* C API on the **accelerator** (surface client, type 0)
3. `IOAccelSetSurfaceFramebufferShapeWithBackingAndLength` — client
   backing variant
4. `IOCreatePlugInInterfaceForService(**framebuffer**, ACCF0000-…-904e,
   6766E94A-…-904e, &interface, &quality)` — plugin instantiated **from
   the framebuffer service** (why VMsvga2 copies `IOCFPlugInTypes` onto
   the FB)
5. `GetBlitter(CopyRegion|OpType0, SourceFramebuffer)` →
   `AllocateSurface(kIOBlitHasCGSSurface, surfaceID)` → `SetDestination`
   → invoke blitter → `Flush`
6. `IOAccelWriteLockSurfaceWithOptions` / unlock → read pixels →
   `FreeSurface` / `IOAccelDestroySurface` / `IODestroyPlugInInterface`

**Architecture finding: the consumer uses BOTH channels side by side** —
the `IOAccel*` C API on the accelerator (surface lifecycle) *and* the GA
CFPlugIn on the framebuffer (2D ops). Apple's own tool requires both.
Scope note: app-side attach demonstrably needs the plugin (readfb,
QuickTime); whether WindowServer itself instantiates it for compositing
is unverified — VMsvga2's Q3 evidence has WindowServer on the surface
client, apps on the plugin.

**Implication for the fix order:** the pre-registered experiment (FB trio
+ claims) exercises WindowServer-side discovery only. The app-side
surface path additionally requires a GA plugin that does not exist in
this project — VMsvga2GA (MIT-style headers) is the only worked example
of the table, and `readfb.c` is the test client. Sequence any plugin work
after the WindowServer probe; do not block the probe on it.

### readfb.c as staged arbiter — step 0, before any driver change 

More than a template: a **staged arbiter that runs against the current
kext, unmodified**. Its first call is `IOAccelFindAccelerator(FB)` —
exactly the consumer of the FB trio. Cross-compile for 10.6 with the
existing toolchain and run it as-is. **Pre-registered expectation: it
fails at that first call today**, which converts the personality
diagnosis from inference to observation — no kext change, no boot-arg
gate, no risk to the working software path.

The failure-point ladder (each failure names the next piece of work,
better than a binary pass/fail on selector traffic):

1. **Fails at `IOAccelFindAccelerator`** → the FB trio is the gate, as
   diagnosed. (Expected today.)
2. **Gets past it, fails at `IOCreatePlugInInterfaceForService`** → step 1
   of the fix order worked; the missing GA plugin is the next gate —
   exactly where the sequencing puts it. (The FB also lacks
   `IOCFPlugInTypes`, so today's run could not distinguish trio-missing
   from plugin-missing anyway — and doesn't need to, since it fails at
   the earlier call first.)
3. **Gets past that** → both channels are live.

And because it is Apple's own consumer, a failure in it is evidence
about the driver, not about the probe — which no hand-written prober can
claim.

**Refinement (2026-08-14, session close): run it at every rung, not
once.** The baseline run is the *before* half of a controlled comparison,
not the discriminator itself — what discriminates is the **pair**.
Baseline: fail at `IOAccelFindAccelerator`. After the trio lands: if
readfb passes while WindowServer stays silent, that separates "IOAccel
discovery now works" from "WindowServer declines to use it" — two very
different next steps. Without the baseline, a post-fix pass has nothing
to compare against and the same silence is unattributable. So: **run at
baseline, after the trio, after the plugin** — each run is cheap, needs
no gate, and converts one inference into an observation. In one respect
a better instrument than the boot-arg probe: it never touches the
properties WindowServer reads, so it cannot perturb the thing it
measures.

**The free second control: the desktop still composites.** A visibly
live desktop during a readfb failure establishes CGS compositing works
by some non-accelerated route — so the failure is about the accelerated
path specifically, not a broken display.

---

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
x ("tions on installing…"). Verified discriminations:
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

**November logs explanation (VERIFIED):** the
confirms the file was extensively tested on the CATALINA guest —
explains logs existing for a file never compiled in this
project, consistent with all git evidence, and confirms the
notes are stale for this target. Consequences recorded: the
d98-era failure mechanism is a lost binary's story (see verdict
phrasing above), and the standing directive is that this
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
(pre-authorized ordering): flip on + accelerated
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

**Backing strategy for the probe (recorded):**
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
   OpenGL, not Mesa. Correction recorded.

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
observed: "dark window + rotating triangle" visible in UTM.

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
  the repeatedly-observed case is this handoff plus Gecko's own
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
"disappear for a second and restart" seen at the dock is this
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
forwarding. **Needs a visual check first** —
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

**The 400x128 surface is Gecko's safe-mode prompt** (confirmed
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
visible — and one was clicked (Refresh Profile), so input
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

**Corrections accepted and recorded:**
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
painting — the observed white-then-black). Confirmed: window
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
visually confirmed via screenshot: "PowerFox Safe Mode" window,
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

**Why the kext was absent — RESOLVED (stated, not
inferred).** The kext was deleted via the recovery procedure
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
verifications it stopped booting, badly enough that recovery was
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
zero errors. Visible animated triangle confirmed on the guest's
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

---

## 2026-08-23 (late) — THE PANIC LEVERS EXECUTED: slto_us
landed in config.plist (read-before-write honoured);
kext spin audit complete (no IOSimpleLock; one hot
poll, one SIMULATED delay); the 23:08 ESP panic file
identified as a re-save of 19:09

- **BOOT-ARGS — the live string, read from
  /Volumes/efi-legacy/EFI/OC/config.plist (guest
  automount), NOT from any document:**
  `-v keepsyms=1 debug=0x12a vsmcgen=1
  msgbuf=1048576 serial=5 vm-accel-surface=1
  tlbto_us=0 vti=9 vm-cap3d=1`
  Three project args beyond the rules-file reference
  list (`vm-accel-surface=1`, `vti=9`, `vm-cap3d=1`)
  — the read-before-write rule is why they survived.
- **EDIT APPLIED (takes effect at next boot only):**
  inserted `slto_us=10000000` (10 s) after
  `tlbto_us=0`; backup at `config.plist.bak-slto`;
  verified by re-read:
  `… serial=5 vm-accel-surface=1 tlbto_us=0
  slto_us=10000000 vti=9 vm-cap3d=1`. Value is a
  deliberately explicit large number, not 0 — see the
  tension below.
- **TLB TENSION (recorded, unresolved):** the 08:09
  panic is `TLB invalidation IPI timeout` (pmap.c:
  2741) DESPITE `tlbto_us=0` live in the config —
  so either 0 does not mean "disabled" on xnu
  10.8, or the arg was added after that boot. Do
  not treat `tlbto_us=0` as proven-disable; if TLB
  panics continue post-slto_us, give tlbto_us an
  explicit large value too.
- **TRADE (accepted deliberately):** a genuine
  deadlock now hangs instead of panicking —
  acceptable while the cause is emulation
  starvation; makes a real deadlock harder to
  diagnose. Revisit if hangs replace panics.
- **KEXT SPIN AUDIT (grep over FB/, complete):**
  - **IOSimpleLock: ZERO uses.** The
    interrupts-disabled-across-slow-work class is
    absent by inspection; serialization is locks/
    gates elsewhere.
  - **IODelay sites, three classes:**
    1. HOT — the submitCommand poll,
       VMVirtIOGPU.cpp:2318 `IODelay(20)` × ≤10
       (the 12:59 panic site; every 2D refresh and
       3D batch submit passes through).
    2. WARM/SIMULATED — VMQemuVGAAccelerator.cpp:
       3717 `IODelay(flush_op.software_commands *
       10)` under the comment "Simulate software
       command processing time" — **a busy-wait for
       work that does not exist**; proportional to
       command count on the desktop-flush path.
       Removal-class, not conversion-class.
    3. COLD — device-init delays (VMVirtIOGPU.cpp:
       1196-1209) and boot IOSleep(5000)s;
       one-shot, harmless.
  - IOSleep sites all yield — not spinlock-timeout
    contributors.
- **THE 23:08 ESP FILE (panic-2026-08-23-230855.txt
  on the efi drive root, alongside opencore-*.txt
  dumps):** byte-identical to the 19:09 report (same
  lock 0xffffff800bfd0280, owner 0xffffff800f9506b8,
  RIP AppleUSBEHCI+…) — a RE-SAVE of the 19:09
  event, not a sixth panic. Census stands: five
  today, 19:09 the last.

---

## RUNG 63 PRE-REGISTERED — STOP CONTRIBUTING: the
hot poll converted to a gated sleep, the simulated
accelerator delay deleted (committed before the code)

**Principle:** the driver cannot fix an emulation
artifact, but it can stop feeding it — a spinning
vCPU under TCG consumes scheduler time the other
vCPUs need; that consumption is the starvation
mechanism itself.

**The changes:**
1. **VMVirtIOGPU.cpp:2313-2330 poll** — spin count
   becomes boot-arg-gated (`vm-poll-spin=N`,
   default 10 = today's behaviour; 0 = pure
   IOSleep(1) fallback), so one binary runs
   spin-fast on 1-vCPU and sleep-polite on SMP
   without a rebuild.
2. **VMQemuVGAAccelerator.cpp:3714-3718** — the
   simulated `IODelay(n*10)` block DELETED (with its
   IOLog); there is no real work it waits for.

**Predictions:**
- (i) `vm-poll-spin=0` on 4-vCPU: the spin
  contribution vanishes; per-submit wall time
  regresses toward the IOSleep floor (~10 ms TCG)
  when the host exceeds the short budget — visible
  in the existing EXIT-OK `spin_iter` field
  (values ≥ SPIN_ITERATIONS);
- (ii) default (`vm-poll-spin` unset): behavior
  byte-identical to today (arg-gate dormant) —
  the control for the gate itself;
- (iii) the panic census does NOT change from the
  code alone — `slto_us` is the class-killer; this
  rung is hygiene so the driver is not the
  starvation source when SMP is an accepted axis.

**Exposure:** kext rebuild + install (boot risk:
none new — no new KPI symbols, no new calls);
verify on the next boot; GLD stub and config
untouched.

---

## RUNG 63 RESULT — IMPLEMENTED, DEPLOYED, BOOTED CLEAN;
both watchdog args verified live in the kernel; the
simulated delay recategorized; the synchronous-poll
question registered open

- **THE CODE (commit b6e1ff1):** `vm-poll-spin` gate
  added as a memoized parse beside `vm-cap3d`
  (default 10 = byte-identical; 0 = pure IOSleep;
  negative clamps to 0) — no header change, no new
  KPI symbols; the accelerator's simulated
  `IODelay(n*10)` block deleted outright.
- **DEPLOYED:** /S/L/E md5 `534294c2…` verified
  against the build; caches cleared, `kextcache
  -system-caches` run, Startup/Extensions.mkext
  rebuilt FRESH (9,800,503 bytes, mtime after the
  install — the RTC reset at the 19:09 boot had made
  timestamps read backwards; the guest clock is now
  authoritative from 19:09 onward). Booted clean; 9
  vmvirtio registry entries; no new kernel panic
  (newest Kernel report remains the pre-rung-63
  19:09 one; a configd USERSPACE crash at 19:35 is
  the separate chronic issue, not kernel, not ours).
- **BOTH WATCHDOGS ARMED AND VERIFIED LIVE:**
  `sysctl kern.bootargs` shows
  `…tlbto_us=10000000 slto_us=10000000 vti=9
  vm-cap3d=1` — the args reach the kernel. `tlbto_us`
  was raised from 0 to explicit-large in the same
  edit (backup chain: config.plist.bak-slto): a TLB
  panic had fired WITH `tlbto_us=0` live, so
  0-does-not-disable is ESTABLISHED, not suspected —
  and the two readings (0 = default; arg not parsed
  on this xnu) are settled by the same test. If TLB
  panics persist under the explicit value, the arg
  is not reaching the timeout path, the TLB lever is
  illusory, and `slto_us` becomes suspect by the
  same mechanism — the panic SIGNATURE distinguishes
  the two watchdogs, so this boot tests both levers
  independently.
- **CONVOY TRADE SHARPENED (review):** today's
  spinlock panics are HELD-lock convoys (owners
  active on other CPUs), not free-lock starvation —
  the raised tolerance covers something actually
  happening. The cost is real: a convoy that never
  drains now hangs silently instead of panicking.
  Diagnosis of a future hang must not assume
  deadlock-or-nothing.
- **SIMULATED DELAY RECLASSIFIED (review):** the
  10µs-per-command pretend-work delay belongs with
  the INVENTED-PROPERTY cleanup family — the reason
  to remove it is HONESTY (it fabricated completion
  time for work that never happened); the SMP
  benefit is incidental. Deleted on those grounds.
- **THE SYNCHRONOUS-POLL QUESTION (open, named):**
  does 2318 need to be synchronous at all? For
  fire-and-forget classes (2D flushes, refresh-path
  transfers) a deferred completion picked up on the
  next refresh tick removes the wait without a
  command gate — a frame of latency, no spin. But
  submitCommand also serves classes whose RETURN
  VALUES are consumed immediately (capset blobs,
  fence waits, transfer-from-host readback) — those
  cannot defer. The split is per-caller-class, not
  whole-function; register before implementing.
- **EFFECTIVE VARIABLES OF THIS BOOT:** the kext
  change is behavior-neutral by default (gate
  dormant), so this boot's live variables are the
  two watchdog args — one-variable discipline
  preserved by construction, not by luck.

---

## RUNG 62 RESULT — THE CGL BISECT: NS INNOCENT, the
failure is OUR pf object's; mode10 ruled out by
bisect; the +0x1c/+0x20 semantics the new prime
suspect (bit sizes vs FORMAT CODES)

**The instrument:** /tmp/cglrepro.c — 6 attr sets ×
[plain, +4] × [option-late, option-pre]; plus the
GLD_PF_MODE10 env bisect through it. Guest runs
20:44-20:50; stub log writable (the earlier
root-owned /tmp/vm_gld_stub.log DENIED sl's appends —
run 1's markers failed with exit 1, initially
misread).

- **FINDING 1 — NS IS INNOCENT (prediction ii dead,
  and rung 60's split DEAD):** CGL-direct
  `[5,12,24]` and `[5,12,1]` → **npix=0, err=0** —
  IDENTICAL to the NS failure, stable across two
  reruns (pids 265/285). Every choose reaches our
  stub (`gldChoosePixelFormat` logged per cell). The
  NS layer forwards the caller's attrs unmodified to
  the same entry (disassembly) and adds nothing that
  matters. The NS-vs-CGL "split" of rung 60 does not
  reproduce under working instrumentation.
- **FINDING 2 — the option 0x1F9 is DEAD in both
  orderings:** after-first-use AND before-first-choose
  — matrices byte-identical.
- **FINDING 3 — attr 4 is INTERNAL, and the earlier
  label WRONG:** explicit attr 4 → err=10000
  (kCGLBadAttribute) — public callers cannot pass it.
  The stub log shows the CGL pipeline appending the
  trailing 4 itself on BOTH paths ([0x5 0x4] for
  plain db). And kCGLPFAWindow = 80 per the SDK
  header — the rung-62 checkpoint's "attr 4 =
  kCGLPFAWindow" was a wrong inference, corrected.
  The walk truncates at 4 universally ("build
  anyway") — harmless.
- **FINDING 4 — MODE10 RULED OUT:** GLD_PF_MODE10 ∈
  {0x18, 0x48, 0x88, 0xc, 0x108, 0x208, 0x2008} —
  every one npix=0 on `db d24`. The depth gate is NOT
  the +0x10 echo. (Rung 51's "depth composes bits
  into +0x10" hypothesis: falsified by bisect.)
- **FINDING 5 — THE NEW PRIME SUSPECT, from the
  float's own tail (grf.t 0x17b5e/0x17b82/0x17d01/
  0x17d04):** the float's pf object gets
  `+0x1c = -0x78` (default **0x1000**) and
  `+0x20 = -0x7c` (default **0x80**) — FORMAT-CODE-
  shaped values, NOT bit counts. Our stub writes
  decimal bit sizes (24/8) at the same offsets
  (rung 59's mapping, never scorer-validated for
  depth). If the scorer compares +0x1c as a format
  code, EVERY depth size fails regardless of number —
  exactly the all-sizes-fail signature. The float's
  switch also carries cases for attrs 4, 0x20, 0x80,
  0x100 the walk region shows at its tail.
- **RESIDUAL (unexplained, bounded):** run 1 (log
  unwritable) returned npix=1 for `db d24`/`db d1`
  where runs 2/3 return 0 — same binary, same sets;
  the only known difference is the log file's
  existence/permissions. Not resolved; runs 2/3
  (instrumented, twice) carry the result.
- **INSTRUMENT LESSON (the class again):** a
  permission-denied log append fails SILENTLY — the
  marker's exit 1 was the visible tell and was
  initially attributed to the wrong command. The
  stub log must be checked WRITABLE by the probe user
  before any run whose reading depends on it.

**NEXT (registered): the float-reference dump** —
with our GLD bundle moved aside same-session, choose
`[5,12,24]` through the float and dump the returned
CGLPixelFormatObj's first 0x40 bytes: that object is
the scorer-ACCEPTED composition for a depth request,
empirical, no jump-table archaeology. Mirror its
+0x1c/+0x20 (and neighbors) in the stub, redeploy,
re-run cglrepro: `db d24` → npix=1 is the
pre-registered pass line.

---

## RUNG 62 CLOSEOUT — THE FLOAT-REFERENCE DUMP, THE
MIRROR, AND GLMARK THROUGH THE GLD: depth gate DOWN,
benchmark SCORE printed

- **THE DUMP (bundle aside same-session, float
  serving, restored — /tmp/pfdump.c):** the
  scorer-ACCEPTED object's depth/stencil slots are
  **FORMAT CODES, size-independent**: depth absent →
  1; ANY depth request (1/16/24/32 identical) →
  **0x1000**; stencil absent → 1; stencil → **0x80**.
  RunG 59's "24/8 = bit sizes" was the wrong semantic
  — the field never described bits. (The dump also
  shows the accepted object's id=0x1020400,
  flags=0x4c9, mode=0x8 — matching the stub's known
  values; only the depth/stencil slots were wrong.)
- **THE MIRROR (one variable):** the walk now tracks
  attrs 12/13 and writes `+0x1c = 0x1000|1`,
  `+0x20 = 0x80|1`. Deployed `aabc1281…` (98 exports,
  digest verified).
- **PASS LINES, ALL HIT:**
  - cglrepro: `db d24` npix=1, `db d1` npix=1,
    glmark set npix=1 (Phase A and B — the option
    remains irrelevant);
  - **nstest (the NS harness): ALL ELEVEN sets OK —
    every depth size 1/16/24/32 AND the glmark set
    through NSOpenGLPixelFormat.** The rung-60 wall
    is down.
  - The rung-59→repro contradiction resolved: rung
    59's CGL pass predates the attr-11 parser fix —
    the walk truncated before depth and passed
    dishonestly; with the walk fixed, 24-at-+0x1c
    failed every depth request; the codes pass them.
- **GLMARK THROUGH THE GLD (first full run):**
```
GL_VENDOR:   VMQemuVGA Project
GL_RENDERER: Apple M4 Pro (virgl)
GL_VERSION:  4.1 (virgl, Apple Core OpenGL backend;
             engine software fallback)
Surface: 800x600 windowed, depth=32 read back
GA path live: AllocateSurface → SetSurface →
              LockSurface addr=105100000 row=3600
build use-vbo=false: FPS 23 (44.6 ms)
texture <default>:   FPS 32 (31.6 ms)
glmark2 Score: 26
```
  The rung-61-corrected version string is live in a
  real application. A benign `__NSAutoreleaseNoPool`
  warning from the event pump (ssh, no runloop pool)
  — cosmetic.
- **OPEN NEXT:** the wider scene set (shaders,
  shadow, buffer variants — GLMark's full suite) and
  where each scene's work lands (engine raster vs
  host GL); the fmt-19-array rejection (rung 58
  bound) surfaces if any scene stages depth arrays.

---

## GLMARK FULL SUITE (first complete run) — ZERO setup
failures, real CPU-side numbers, and PRESENTATION
ABSENT BY DESIGN: the swap slot presents the clear,
not the engine's frame

- **Data-path fix (permanent):** the suite's shaders/
  models/textures installed to the DEFAULT location
  `/usr/local/share/glmark2/` (was /Users/glmark/data,
  reachable only via --data-path). No libraries were
  missing — the earlier "Set up failed" was purely
  the data path.
- **The full suite, background run (/Users/glmark/
  full.log), zero setup failures:**
```
build  use-vbo=true    20 FPS | texture  n/l/m 27/31/30
shading gouraud/bi-inf/phong 15/18/13 | bump normals 29
effect2d kernel-3x3  511 FPS (near-empty outlier)
desktop shadow:4       1 FPS (1186 ms) | buffer map/sub/int 7/2/9
ideas speed=duration   1 FPS (1528 ms) | jellyfish 4 (274 ms)
```
  The heavy shader scenes crawl (GLVM software
  rasterization under TCG — expected); the light
  scenes hold 20-30 FPS. (Final Score line not
  captured; the scene table stands on its own.)
- **THE PRESENTATION GAP (observed on the guest
  screen — scenes run, nothing visible):** the swap
  slot (rungs 48-49) presents OUR CLEAR — the probe
  era's design. The engine rasterizes real frames
  into the locked GA surface (LockSurface
  addr=0x105100000 row=3600) and NOTHING relays
  those bytes to the scanout. "Offscreen, no blit"
  is the exact state.

---

## RUNG 64 PRE-REGISTERED — THE FRAME BLIT: the
engine's buffer to the scanout at swap (committed
before implementation)

**The change:** the swap path extends past the
clear — read the engine's finished frame from the
locked GA surface, transfer it UP as the virgl
resource's content, and relay (0x600C) to the GA
surface at the shape offset → desktop flush →
scanout. The clear remains the fallback/first-frame
path. Mechanism options to settle in implementation:
a relay-selector extension (userspace src pointer +
row stride; kernel copies app buffer → host resource
→ GA write) — the LockSurface address/row are known
kernel-side (our accelerator's own user client
logged them).

**The stride question, named up front (the rung-44/45
lesson):** LockSurface reports row=3600 for an
800-pixel window (800×4=3200) — the SURFACE geometry
and the WINDOW geometry differ; the blit must use the
surface's stride and shape offset, never the window's.

**Predictions:**
- (i) **SCENES VISIBLE** — the engine's frames on
  screen at the window's place; content moves with
  the scenes; the suite becomes watchable.
- (ii) **MISPLACED/TORN** — stride or offset math
  wrong (3600-vs-3200 class); visible but shifted or
  sheared; the fix is arithmetic, not architecture.
- (iii) **STILL CLEAR-ONLY** — the engine's buffer at
  swap time is not the LockSurface address (double
  buffering: the frame is in the OTHER buffer);
  instrument the lock/swap sequence to find the real
  backbuffer.

**Exposure:** kext relay-selector extension (no new
KPI symbols — existing user-client machinery) + GLD
swap entry change (live-swap); no config, no reboot
beyond the kext install.

---

## RUNG 64 CHECKPOINT — THE PUSH LEG BUILT AND PROVEN;
prediction (iii) FIRED: the engine does NOT write the
GA surface; the frame's location is the open frontier
(three hypotheses eliminated by instrument)

- **0x600D gaPushSurface: LIVE** (commit 6e9e29f) —
  the kernel reads the GA-bound surface's own backing
  and runs the relay's flush+push tail; every swap
  returns 0x0. Geometry banked:
  **surf 800x622 stride 3600 shape (100,328)**.
  This leg needs no changes when the frame source is
  found — it composes.
- **THE CENSUS VERDICT (in-kernel, first 5 swaps):
  ALL ZERO — nonzero=0 distinct_head=0.** The engine
  does NOT rasterize into the GA surface backing.
  Prediction (iii) fires: the frames live in a
  private buffer (the float's swap would have
  blitted them — the slot our stub replaced).
- **THE POINTER HUNT — all negative, each by
  instrument:**
  - ctx[0..0x800] and shared[0..0x4000]: no
    0x105-band pointer (the GA lock mapping).
  - engine+0x65c8 / +0x66d0 (first 0x100): dispatch
    tables and float constants (0x4000,0x4000 pairs;
    1.0f consts; table pointers) — content banked in
    the stub log.
  - **ctx+0x218 (the drawable object): ABSENT.** Our
    attach mirror stores the type at +0x210 but never
    assigns the drawable — the float's attach would
    have. The engine renders with NO renderer
    drawable: the raster target is GLVM-internal.
- **NEXT LEAD (named):** the GLVM drawbuffer — find
  via gliSwapBuffers' caller (gle) or the float's own
  swap (grf) reading its SOURCE pointer; one
  disassembly read names the buffer's home, then the
  frame blit = copy rows → TRANSFER_TO_HOST_3D →
  0x600B → existing present.
- **STANDING PROCEDURES BORN THIS ARC (each cost a
  run):**
  1. **Post-boot: `chmod 666 /tmp/vm_gld_stub.log`**
     — the first root GL process re-creates the log
     root-owned at EVERY boot; sl's appends then fail
     silently (hit twice; the marker's exit-1 is the
     tell).
  2. **`MACOSX10_6_SDK=<sdk> sh probe/deploy_gld.sh`**
     when the shell loses HOME (empty $HOME breaks
     the script's SDK fallback, twice this arc).
  3. **Deploy verdicts grep DEPLOYED** — a piped
     deploy failure is masked by tail's exit 0; a
     silently-stale binary ran GLMark once this arc.

---

## RUNG 65 PRE-REGISTERED — THE FORMAT-QUERY CHECK:
does OUR Mesa share the defect the clamp works
around? (a measurement to end an argument)

**PROVENANCE (recorded before the check, the part
most likely to be lost):** the clamp exists for
LINUX-GUEST Mesa — a format-query mis-detection
that zeroed MaxSamples there. Whether it applies
HERE is an empirical question about THIS cross-
compiled Mesa build, not a property of the host or
the device. Anyone reading samples=1 in six months
without this note will take it for a hardware limit
— the reading that already cost this arc four rungs.

**The decision rule (the whole question):** exercise
the format query through our Mesa build (the GLD's
virgl embed — RB + RenderbufferStorageMultisample at
samples=4, GL error the observable). If the pipe
accepts/queries 4 correctly → the workaround is for
someone else's bug; declining it and taking v1's
honest 4 is right. If it reports 0 / rejects → the
clamp's premise holds here too and 1 stands.

**Exposure:** GLD-side env-gated check (no boot);
log the verdict; no config change either way.

---

## RUNG 64 SECOND CHECKPOINT — THE CONTENT INVERSION:
GLVM rasterizes NOTHING (no drawbuffer exists); the
attach is the missing link; the float's drawable
object decoded; one instrument crash taken and fixed

- **THE INVERSION (all by instrument):** during a
  live build-scene run — the GA-locked surface
  mapping (VM region 0x105100000 sz 0x343000)
  ALL ZERO; the two frame-sized staging mallocs
  (0x105443000/0x105618000, 0x1d5000 each) ALL ZERO
  at every quarter-point, both passes; a widened
  malloc hunt (all blocks 0x40000-0x1000000, census)
  found only vertex-float staging and pointer
  arrays; a full VM-region walk (writable,
  0x100000-0x8000000) found NO live raster region —
  the one candidate (0x1057ed000 sz 0x52d000) was
  ALLOCATOR METADATA (its header carries its own
  size; identical across passes — an initial
  misread, corrected). **GLVM never rasterizes:
  the FPS are command-buffering cost, no pixels.**
- **THE MISSING LINK (rung 33's decode, now
  decisive): the drawable object at ctx+0x218 — our
  attach mirror stores the type (+0x210) and
  nothing else; with no drawable object, GLVM has
  no drawbuffer, so draws go nowhere. The staging
  pair was allocated (by the engine) awaiting the
  renderer link that never came.**
- **THE FLOAT'S CONTRACT (grf.t, decoded this
  session):** `glsAssignDrawable` mallocs a 0x80-
  byte drawable {+0x8 width, +0xc height, +0x14
  ptr chain, +0x78/0x79 flags, ...}, releases the
  old via `glsReleaseDrawable`, stores the object
  at ctx+0x218 (0x21e2b); the buffer linkage
  continues at 0x21e5c+ (malloc(0x80) more,
  bundle-header-initialized pointer at +0x14).
- **ONE INSTRUMENT CRASH, TAKEN AND FIXED (its
  lesson recorded):** the rung-64 desc+0x30 follow
  blind-dereferenced a non-pointer — one rung's
  descriptor carried {0,4} there = 0x400000000,
  inside the "plausible range" guard, unmapped →
  glmark2 crash 22:14 (RIP gldAttachDrawable+672,
  CR2 0x400000000 — the register state named it in
  one read). Block DELETED; rule: NEVER blind-deref
  a descriptor field. Bonus from the trace: this
  attach path runs NSOpenGLContext setView → CGL
  internals → gliAttachDrawableWithOptions → the
  ENTRY-level gldAttachDrawable.
- **THE PRESENT MACHINERY, all banked and working:**
  heap hunt, VM walk, midpoint pick, 0x600E
  gaPresentUserBuffer (600/600 rows, stride
  3200→3600, shape offset, flush+push — the user's
  flicker), 0x600D push fallback.
- **THE FORK (named, next session's decision):**
  - **A — COMPLETE THE FLOAT CONTRACT:** finish the
    glsAssignDrawable mirror (the 0x80 object + the
    buffer linkage into the staging pair + ctx+0x218);
    GLVM then rasterizes into staging (software, the
    engine's own path) and 0x600E presents it. The
    pieces exist on both sides; the remaining work is
    the linkage read at 0x21e5c+.
  - **B — ROUTE DRAWS TO MESA/VIRGL:** the engine's
    GL through the embedded Mesa (rungs 55-58
    extended from clear to draws) — the project's
    strategic 3D path; bigger surface, host-GPU
    raster.

---

## RUNG 64 CLOSED — VISIBLE CONTENT: "I saw the
cube" — scenes on screen through the whole chain
(float raster → float swap → our push → scanout)

- **VISUAL CONFIRMATION (the deciding instrument —
  the eyes):** the GLMark build scene VISIBLE in the
  window. The full chain live: GLRendererFloat's real
  machinery rasterizes (contexts, dispatch, textures,
  programs all forwarded), its swap blits, our
  wrapper pushes the GA surface (0x600D), the
  compositor shows it.
- **THE TRAMPOLINE (final shape, this session):**
  forward to the float — InitDispatch (real draw
  entries), attach (the true drawbuffer at
  ctx+0x218, the contract rungs 33-64 chased),
  CreateContext/DestroyContext/UpdateDispatch, the
  EPR content entries (textures, programs,
  SetInteger) behind the ARM GATE (loader probes
  refuse until OUR Initialize completes). OURS kept:
  gldChoosePixelFormat (depth format codes —
  GLMark's gate), gldGetString (capset strings),
  gldCreateShared (the float's version on
  un-Initialized state = the context-killer, found
  by bisection), the swap WRAP (ctx+0x65c8+0xE8 at
  both attach sites).
- **NUMBERS:** build 12 FPS, texture 10 FPS Score 9
  — through the float's real software rasterizer
  under TCG.
- **THE ARC:** rung 44 blue rectangle → 45 shape
  offset → 48 swap slot → 49 app color → 59 GLMark
  loads → 60-62 the depth gate (format codes) →
  63-64 the frame path — and now the engine's OWN
  content, on screen, by delegating the renderer
  contract to the renderer that wrote it.

---

## RUNG 66 PRE-REGISTERED — FORK B, STEP 1: THE DRAW
DOOR CENSUS — which dispatch slots carry the draws
(now observable: the float's dispatch makes them
flow)

**The question:** the float's dispatch entries render
the scenes; which slots fire PER FRAME (the draw
pipeline) vs once (setup)? Those slots' arguments are
the ABI to decode for Mesa replay.

**The instrument:** counting wrappers over EVERY
dispatch slot the float fills (save its pointer,
install a counter thunk); the swap wrapper logs
per-frame deltas. Run build (vertex-lit) vs texture
(textured) — the slots that differ name the
draw/texture ABI surface.

**Predictions:**
- (i) A SMALL draw set: 2-4 slots with large per-
  frame deltas (a draw + state slots), stable across
  scenes; their args decode to vertex/state records.
- (ii) ONE mega-slot: the flush/batch entry carries
  everything (GLVM's whole batch) — the ABI is one
  record, decode once.
- (iii) MANY slots per frame (the full GL replay at
  gld level) — Mesa replay means a wide ABI; the
  census sizes the work.

**Exposure:** stub-only (live swap), no boot.

---

## RUNG 66 CHECKPOINT — FORK B STEP 1 INSTRUMENTED;
the wrap-point falsified (ctx+0x66b0 is a NO-OP slot
in the float engine); the real slot = engine+0x66b0
via block1; a teardown crash class recorded

- **THE INSTRUMENT (built, deploying clean):** counting
  wrappers over all 33 float-filled dispatch slots;
  per-frame delta report at swap. Awaiting its hook.
- **THE WRAP-POINT FALSIFICATION (three runs of
  evidence):** wraps at ctx+0x65c8+0xE8 read 0x0 at
  attach (the float's selection runs LATER) and the
  wrapper NEVER fires — while scenes render and swap
  float-native. Conclusion: [gli_ctx+0x66b0] is not
  the live swap slot in the float configuration; the
  selection (grf 0x1f7e0) writes block1 = ENGINE+
  0x65c8, so the live slot is ENGINE+0x66b0 —
  reachable only with block1 in hand (the +0x50
  install entry's own a2), which this engine
  configuration does not call. NEXT: capture the
  engine base at InitDispatch (the dispatch block and
  block1 are both engine-side; rung-34's kSlots
  region holds the offset relationship) or hook the
  selection's write site.
- **A CRASH CLASS TAKEN (the trampoline's debt):**
  teardown dies at gliDestroyContext →
  gleDestroyEnableHashTable → gleFreeEnableHashObject
  (during NSOpenGLContext dealloc) — mixed ownership:
  float-created objects in engine hash tables with
  our destroy forwards. Post-content (scenes complete
  first); a cleanup rung of its own.
- **INSTRUMENT LESSON (the class, third time):** a
  forward macro that RETURNS makes any statement
  after it dead code — the poll-after-forward bug;
  and a poll through a0 across all thunks faults on
  integer-handle args (a0 is not always a pointer).
- **STATE:** CPU rendering (the float's raster) with
  visible content stands; GPU routing awaits the
  census read, which awaits the block1 hook.

---

## THE FULL SUITE, VISIBLE, COMPLETE (full2.log) —
zero setup failures, every scene watched on screen;
Score 4 — the honest number, and WHY it differs
from the invisible 26

```
build vbo        12 FPS | texture n/l/m  12/10/8
shading g/bp/cel 10/6/5 | bump hp/n/h     5/7/7
effect2d edge/blur 3/2  | pulsar          10
desktop shadow    1     | buffer sub/int  2/3
jellyfish         2     | terrain         1 (24.8 s/frame)
shadow            5     | refract         1
conditionals f/v  4/10  | function lo/med 6/3
loop ×3           4/4/3
glmark2 Score: 4     (0 setup failures)
```

- **THE COMPARISON THAT TEACHES:** the pre-trampoline
  suite scored 26 with NOTHING on screen — those FPS
  were command-buffering cost, zero pixels produced.
  This suite scores 4 with EVERY scene visible: the
  difference per frame is the true cost of actually
  rasterizing every fragment on the emulated CPU.
  The 26 was never rendering; the 4 is.
- terrain at 24.8 s/frame — the fragment-heaviest
  scene; the GPU-routing case in one number.

---

## RUNG 66 RESULT — THE BLOCK1 HOOK LANDED; the draw
door NAMED: the EXPORTED entries, not the dispatch
table

- **THE HOOK (both sites, working):** engine base
  captured at the create forward (sub-block −
  0x79b8); one-shot dumps + dual-slot poll; SITE0
  (ctx+0x66b0) and SITE1 (engine+0x66b0) both
  wrapped — swaps now flow through our wrapper
  (census reports per swap, 12 FPS maintained).
- **THE ENGINE DECODE (bonus from the dump):**
  engine+0x66d0 = 0x258_00000320 — the 600/800
  window DIMENSIONS live at a named offset;
  flush-class entries at engine+0x6680/0x6688/
  0x66b8/0x66c0/0x66c8.
- **THE DECISIVE NEGATIVE — the draw door is NOT the
  dispatch table:** per-frame deltas across all 33
  float-filled dispatch slots = ZERO. The draws and
  state flow through the EXPORTED gld* entries (the
  EPR forwards — the path the old census saw hot:
  ModifyPipelineProgram ×9788, FinishObject ×2994).
  The 33-slot table serves another purpose in the
  float architecture.
- **NEXT (one macro edit):** counters on the EPR
  forwards — the per-frame delta census rerun on the
  export table names the draw slots' offsets in the
  ABI decode order.

---

## RUNG 66e RESULT — THE EXPORT CENSUS: the draws
NEVER CROSS the renderer boundary; fork B reframed —
the GPU route is CGL interposition (the shim), not
renderer-slot forwarding

- **THE READ (build scene, per-frame deltas):**
  - swap 1 (setup): CreateTexture:+20
    CreatePipelineProgram:+9 CreateVertexArray:+2
    ModifyPipelineProgram:+7 SetInteger:+5 + destroys
    and unbinds (rebuild churn)
  - swaps 2-5 (steady): **ModifyPipelineProgram:+2
    ONLY.** No geometry, no texture loads, no draws.
- **THE CONCLUSION (with the zero dispatch deltas —
  prediction iii in an unexpected shape):** the
  engine's GLVM EXECUTES the pipeline programs
  itself; the renderer plugin COMPILES them
  (ModifyPipelineProgram) and holds state objects.
  There is NO draw ABI at the plugin level to decode
  — the raster work never crosses the renderer
  boundary. Slot-forwarding to Mesa cannot work by
  construction.
- **FORK B REFRAMED:** the GPU route is CGL
  INTERPOSITION — the cgl-shim architecture that
  EXISTS and is proven (Gecko ran on it; virgl
  through the shim is the Mesa-VirGL arc). GLMark
  under the shim → Mesa → virgl → host GPU →
  readback → present, bypassing the engine's CPU
  execution entirely. The GLD trampoline remains
  what it proved: the engine's own renderer,
  fulfilled, content visible, the presentation
  chain live.
- **NEXT (named):** run GLMark with the CGL shim
  interposed (DYLD path, the Gecko configuration) —
  the shim's flush presents via the winsys/relay;
  compare scene FPS vs the float's CPU numbers.

---

## FORK B LANDED — GLMARK ON THE GPU: virgl
(Apple M4 Pro), 14 FPS build scene, HOST-BLIT per
frame — via the SUBSTITUTE route (the insert route
died on a libc++ init-order crash)

- **THE ROUTE THAT WORKS — SUBSTITUTE, not insert:**
  DYLD_INSERT pulled libc++ into dyld's initializer
  phase → iostream init called NULL (both the system
  10.6 libc++ AND the InterWeb 5.0.1 pair — an
  ORDER problem, not a version problem; frame 0 =
  0x0 in __stdinbuf ctor). The substitute
  OpenGL.framework (deployed at /Users/glmark/subst,
  DYLD_FRAMEWORK_PATH) loads as a NORMAL dependency
  — dependency-ordered init, no crash.
- **THE DEPLOY LAYOUT (the framework's own
  @loader_path/../../.. rpath):** subst/OpenGL.
  framework + libOSMesa.8.dylib + libglapi.0.dylib +
  libOpenGL_real.dylib at the subst root; the libc++
  pair from /Users/sl/osmesa when needed.
- **THE RUN (clean exit 0):**
```
cgl_shim: swizzles installed (GALLIUM_DRIVER=virgl)
virgl_iokit: get_caps capset_id=2 ver=2 size=1408
GL_VENDOR:   Mesa
GL_RENDERER: virgl (Apple M4 Pro)     ← THE HOST GPU
GL_VERSION:  2.1 Mesa 24.3.0-devel
build use-vbo=false: FPS 14 (74.9 ms)  Score 13
HOST-BLIT rect 800x600 at (100,350)   ← per frame
  double buffers 0x106a00000/0x106bd5000 alternating
```
- **THE COMPARISON (same scene):** float-CPU 12 FPS
  vs virgl-GPU 14 FPS — the bottleneck moved: the
  guest CPU no longer rasterizes; it drives the GL
  stream + the per-frame readback round trip. The
  shader-heavy scenes (terrain's 24.8 s/frame on
  CPU) are where the GPU number should diverge
  hard.
- **OPEN (the eyes):** the window at (100,350)
  should show the rotating cube — HOST-BLIT wrote
  the desktop rect; visual confirmation pending.

---

## RUNG 67 PRE-REGISTERED — THE ZERO-COPY SCANOUT
PRESENT: the Linux guest's present path for our stack
(committed before implementation)

**THE COST BEING REMOVED (the 14-FPS postmortem):**
the shim's present reads back EVERY FRAME — 800×600×4
≈ 1.9 MB across TCG-emulated virtio into guest memory,
written into the desktop backing, then pushed BACK UP
to the host (0x600C): three pixel copies per frame
through emulation. The Linux guest's virgl presents
with ZERO per-frame pixel traffic: the 3D resource is
scanout-bound and the host composites it directly.
At 14 FPS, the 75 ms frame is mostly wire, not
raster.

**THE CHANGE:** bind the shim's 3D fb resource to the
SCANOUT (the kext's 2D machinery already scanout-binds
the desktop resource — SET_SCANOUT on the 3D resource
ID + RESOURCE_FLUSH per frame) and drop the readback
from the flush path. virglrenderer composites host-
side. The kernel's 0x600C relay stays as fallback.

**Predictions:**
- (i) **THE FRAME COLLAPSES:** build scene's 75 ms →
  dominated by the GL stream + doorbell; FPS rises
  several-fold (the readback's share of the frame is
  the prediction, measured by the delta).
- (ii) **SCANOUT REFUSES the 3D resource** (the 2D-
  only scanout contract): SET_SCANOUT errors on a
  virgl-context resource; the error names the
  constraint; the fix is the resource's flags/bind.
- (iii) **PRESENTED BUT WRONG** (scale/format/offset):
  the scanout expects the desktop geometry; the 3D
  resource is window-sized — the shape math from
  rungs 44-45 applies; visible but misplaced/scaled.

**Exposure:** kernel selector change (existing
machinery, no new KPI) + shim flush path change;
substitute deploy; no reboot beyond the kext install.
**Pre-registered BEFORE the terrain-crash read** (that
crash — SIGSEGV exit 139 on the terrain scene under
the substitute — is its own diagnosis, independent of
this rung).

---

## RUNG 67 RESULT — THE ZERO-COPY PRESENT LANDED:
scanout-bound 3D resource, host-composited, and
TERRAIN AT 28 FPS — the scene that cost 24.8
SECONDS per frame on the CPU raster

- **THE CHAIN (kernel log, live):**
```
scanoutPresent3D: res=401 800x600 BOUND to scanout
  (2D refresh stood down)
scanoutPresent3D: flush res=401 OK (zero-copy)   ← per frame
```
  0x600F scanoutPresent3D: SET_SCANOUT binds the 3D
  resource (first call / on change), RESOURCE_FLUSH
  per frame, the framebuffer's 2D refresh stands
  down via the pre-built takeover flag. NO readback,
  no guest pixel traffic — the Linux guest's path.
- **TWO GATES FOUND AND OPENED on the way (each cost
  a run):** (1) the winsys zero-copy block sat AFTER
  the host-present rect return — moved FIRST, geometry
  from the resource itself; (2) the OSMesa frontend
  only calls flush_frontbuffer under SHIM_HOST_PRESENT
  — the gate now also rides SHIM_ZERO_COPY.
- **THE NUMBERS:**
  - build (light): 14-15 → **17 FPS** — the wire was
    NOT the light scenes' bottleneck; the TCG-emulated
    command stream is (prediction i partially: the
    frame's readback share measured ~15 ms of 75).
  - **terrain (heavy): 24,826 ms → 35.8 ms — ~700×.**
    THE divergence the fork predicted: the host GPU
    rasterizes; the guest sends commands. Score 27.
- **PREDICTION (iii) FIRED AS ACCEPTED:** present is
  FULLSCREEN (SET_SCANOUT maps the resource to the
  whole display; no desktop-position form exists) —
  the 2D desktop is displaced while 3D owns the
  scanout. The windowed composite is a WindowServer
  integration problem, named for later.
- **RESIDUAL:** the earlier terrain SIGSEGV (exit
  139, substitute era) did NOT recur — unreproduced,
  stays on the list. The fullscreen present means
  the desktop needs releasing on app exit (the 2D
  refresh restore path — verify before daily use).

---

## RUNG 67 RESIDUAL FIRED (the recorded one, now
with a witness): app exit leaves the scanout bound
to a DEAD resource — the whole display goes black;
restored by reboot

- **THE FAILURE (observed on the guest):** GLMark
  exits → its 3D context and resource 401 are
  destroyed → the scanout REMAINS bound to the dead
  resource and `m_scanout_taken_over_by_3d` stays
  true → the host composites a dead resource
  (black) and the 2D refresh never resumes. The
  guest stays alive (screen visible via ARD);
  display restored by REBOOT (the boot path
  rebinds the desktop scanout). Also observed: the
  display RESIZED around the 800x600 bind (the
  host scales the scanout resource to the display
  geometry).
- **THE MISSING PIECE (the release path, named):**
  on 3D-resource destroy of the scanout-bound
  resource AND on user-client close, the kernel
  must REBIND the desktop scanout
  (setscanout(0, desktop_res, 0,0,w,h)) and clear
  the takeover (setScanoutTakenOverBy3D(false)).
  Two hook points exist: the resource-destroy path
  (0x6005-side) and the user client's stop/died.
  Until then: **SHIM_ZERO_COPY runs are
  reboot-to-recover — do not leave apps running.**

---

## RUNG 67b RESULT — THE RELEASE PATH WORKS:
app exit now RESTORES the desktop — the black-screen
class is closed

- **THE KERNEL-VERIFIED CYCLE (this boot, one run):**
```
scanoutPresent3D: flush res=261 800x600 OK (zero-copy)
  ... frames ...
2D scanout RESTORED (res=1 1680x1050 setscanout=0x0)
  after 3D release
scanoutPresent3D: RELEASED res=261
```
  GLMark → GPU fullscreen → exit → the desktop
  scanout REBOUND at full geometry, the 3D released.
  No reboot needed. The observed failure class
  (black display until reboot) is closed.
- **THE IMPLEMENTATION (commit on this branch):**
  - `releaseScanout3D()` on the gpu device (idempotent,
    member-tracked bound resource)
  - `restore2DScanout()` on the framebuffer (rebind +
    clear the takeover + one forced immediate refresh
    so the desktop reappears at once)
  - `clientClose()` calls the release — clean exits AND
    crashes both land there.
- **OPERATIONAL RULE LIFTED:** zero-copy runs are no
  longer reboot-to-recover (pending the eyes' final
  confirmation of the restored desktop).

---

## RUNG 67c RESULT — THE WATCHDOG: SIGKILL death
auto-recovered; all death classes closed

- **THE TEST (this boot, live):** GLMark launched →
  zero-copy frames flowing → `kill -9` → 10.6 IOKit
  skipped the teardown (clientClose never fired —
  the observed gap) → within ~2 s:
```
VMVirtIOFramebuffer: 3D scanout STALE (no flush)
  — watchdog RELEASE
```
  The refresh tick noticed the silence and forced
  the release — the desktop recovered WITHOUT
  manual action. The complete death-class matrix:
  clean exit (clientClose), crash (clientDied →
  clientClose), kill/hang/teardown-skip (the
  watchdog) — all land in restore2DScanout.
- **THE MECHANISM:** scanoutPresent3D stamps
  m_scanout3d_last_flush per frame; the framebuffer's
  existing refresh tick (already running, stood down
  during 3D) polls scanout3dStale() — bound AND
  silent past ~2 s → releaseScanout3D(). Raw
  mach_absolute_time delta (TCG ≈ 1 GHz; coarse is
  fine at 10-30 FPS flush rates).
- **THE ZERO-COPY SYSTEM IS COMPLETE:** GPU
  rendering, fullscreen scanout presentation, and
  release on every death class. The operational
  rule is lifted.

---

## RUNG 67 RESIDUAL SHARPENED — THE ZERO-COPY
PRESENT'S VISUAL IS UNCONFIRMED: flush-OK is
transport truth, not pixels-on-screen

- **THE HONEST LEDGER (every user report of the
  zero-copy path, collected):** "reduced and black";
  "white window (no blit)"; "black fullscreen
  window". NEVER a confirmed visible 3D frame.
  Meanwhile the kernel logs "flush res=N OK" per
  frame — the transport works; the CONTENT reaching
  the display is unproven. The eyes outrank the log
  line and the eyes have said no every time.
- **THE CONTRAST THAT LOCALIZES IT:** the RELAY
  present (0x600C readback) produced CONFIRMED
  content ("double horse with flicker" — visible,
  duplicated). The relay writes the desktop
  resource through the proven 2D push. The zero-copy
  binds the 3D resource directly to the scanout —
  if virglrenderer's scanout path rejects/mishandles
  the resource (format, bind flags, geometry), the
  display shows black while every flush reports OK.
- **THE SUITE CASUALTY:** the interrupted GPU suite
  produced ZERO scene results (full3.log empty —
  killed at the black-screen report; a dead
  un-blitted window lingered until reboot — the
  white/black window class is the SHIM_ZERO_COPY
  riding the host-present gate, which skips the
  guest snapshot that feeds the window).
- **NEXT (registered):** the UTM DEBUG LOG is the
  arbiter (virglrenderer's own decode/scanout
  complaints appear only there) — read it for the
  zero-copy run's resource/scanout handling; the
  fix candidates are the resource's bind flags
  (SCANOUT-capable vs RENDER_TARGET-only) or the
  format. The RELAY path remains the
  visually-proven fallback; zero-copy stays
  experimental until the eyes say yes.

---

## CORRECTION (user-observed): the screen cleared
BEFORE the reboot — glmark2's OWN exit restored the
display

- The black fullscreen was a LIVE presentation (the
  process alive, flushing black frames); when it
  exited, the release path restored the desktop —
  a THIRD confirmed restore, this one observed
  directly at app exit. The "dead window needs
  reboot" read was WRONG; the reboot was
  unnecessary over-recovery. The lifecycle is
  solid: live-black presents, exit → restore.
- **THE OPEN ITEM IS PURELY CONTENT:** black frames
  flush fine, the release works — what's missing is
  the 3D picture on screen. The zero-copy present's
  content question stands exactly as registered
  (UTM debug log the arbiter; relay the
  visually-proven fallback).

---

## THE UTM DEBUG LOG READ — the zero-copy chain is
COMPLETE through compositing; the failure is the
fd import's last inch inside UTM's display stack

- **THE LOG (per-VM: <Snow Leopard.utm>/Data/
  debug.log — the path the rules never recorded,
  now recorded):**
  - `gl scanout fd: 42/43` — virglrenderer hands
    the 3D resource to the display via the NATIVE
    GL-scanout fd path
  - `cs_gl_scanout: got scanout` — CocoaSpice
    accepts; `Ignoring scanout from stale fd 42`
    drops the loser of same-millisecond scanout
    pairs (our desktop-restore rebind and the 3D
    bind racing; monitors config flapping 1024x768
    between them)
  - **380 cs_gl_draw in the 3D window (01:19:05-16,
    ~35/s)** — the accepted scanout WAS composited
    every frame. The chain is complete: render →
    fd → accept → draw. AND THE SCREEN WAS BLACK.
- **THE VERDICT:** the texture itself reads black
  in UTM's compositor — the dmabuf/texture content
  does not survive virglrenderer's GL export →
  CocoaSpice's Metal import boundary (stock UTM;
  host-side; the same boundary the ANGLE display
  env vars live around). NOT our transport, NOT
  virglrenderer's rendering (the relay path
  read back REAL bytes from the same resource class).
- **THE CHEAP EXPERIMENT (next):** the DISPLAY
  RENDERER BACKEND dropdown (Default/ANGLE-GL/
  ANGLE-Metal/Core-GL — currently Core GL) — one
  setting flip + one zero-copy run: content under a
  different import path = the whole thing lands;
  black under all = the relay (readback) stays the
  presenter and the ~15 ms readback is the honest
  cost of stock UTM.

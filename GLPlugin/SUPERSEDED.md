# SUPERSEDED — 2026-08-09

**This whole tree is reference-only, not live code.** Per the project's
"superseded, not deleted" rule it stays in the repo as historical artifact;
do not treat any claim here as competing with current truth.

## Why superseded

### Strategic: Mesa is the chosen direction, not a hand-written GLEngine replacement

`.claude/rules/acceleration.md` ranks the three seam options for getting
GL commands generated on a Snow Leopard guest:

1. **Replace `GLEngine.bundle`** (what this tree attempts) — least tractable.
   The install instruction gives it away: copying over
   `OpenGL.framework/Resources/GLEngine.bundle/GLEngine` replaces Apple's
   renderer core. The v3.0 list in `IMPLEMENTATION_STATUS.md` (command
   submission, texture upload, shader compilation, draw calls) isn't a
   next phase — it's writing an OpenGL 2.1 implementation. Mesa already
   contains all of it: state tracker, GLSL compiler, TGSI generation, and
   a virgl driver that speaks the protocol this project just proved at
   the kext layer.
2. Ship a GLD driver bundle — smaller surface, but GLD ABI is undocumented,
   version-locked, and assumes hardware capabilities you'd have to fake.
3. **Bypass `OpenGL.framework`** with libGL + CGL shim via
   `DYLD_INSERT_LIBRARIES` — the only option with working precedent; the
   one UTM's own Windows driver effectively validates.

The chosen direction is option 3 with Mesa's virgl Gallium driver as the
GL implementation. `GLPlugin/` is option 1 and is not on that path.

### Empirical: CGL never discovered this renderer anyway

The "Expected Behavior" success criteria in `IMPLEMENTATION_STATUS.md`
("CGL detects hardware renderer (ID 0x00024600)", "Pixel format created
successfully", etc.) were never validated. The project's own
contemporaneous research already showed they were false:

- `notes/SNOW_LEOPARD_CGL_ARCHITECTURE_FINDINGS.md` (Nov 2025):
  `CGLQueryRendererInfo(0xFFFFFFFF, ...)` on 10.6.8 returns only the
  software renderer `0x01020400`. Our `0x00024600` is **NOT FOUND**.
  WindowServer creates `VMQemuVGAClient` (type=0), never `VMCGLContext`
  (type=1). The bundle is never loaded.
- `notes/CATALINA_CGL_RENDERER_DISCOVERY_ISSUE.md` (Nov 2025): same
  result on 10.15. Catalina additionally routes CGL discovery through
  Metal device enumeration, which a pure IOAccelerator registration
  cannot satisfy.

So even if this tree had produced a working GL implementation, the
delivery vehicle was broken — CGL never asked for it.

## Methodological problems with the existing docs

These are worth flagging separately because they're patterns, not
one-offs.

### Unfalsifiable success criteria

Every success criterion in `IMPLEMENTATION_STATUS.md` is the absence of
an error — "no 'invalid code module'", "no 'Could not create accelerated
pixel format'", "window displays (even if still software rendered)". A
stub returning `kCGLNoError` satisfies all of them. This is the
`submitCommand` failure pattern (500-line fake that reported success on
every call by reading back the command type instead of the device's
response), one layer up. Same shape, same trap.

### Self-contradiction in the same document

`IMPLEMENTATION_STATUS.md` advertises in its renderer-info section:
`accelerated: 1`, `video_memory: 256 MB`, `OpenGL 2.1` ✓ — while
"Current Limitations" in the same document says rendering still uses
Apple's software rasterizer and no GPU commands are sent.

That's not cosmetic. It's the `crsr = 1` failure class: tell the
consumer you'll do something, and it stops doing the thing that worked.
If CGL believed this renderer, apps requesting `kCGLPFAAccelerated`
would get a context that renders nothing instead of falling back to the
software renderer that does. Worse than not being there. (The
`accelerated: 0` rule for any code kept live follows from this — see
"Salvageable" below.)

## Salvageable

The GLI/CGL plumbing research — renderer discovery patterns, pixel
format attribute parsing, the IOServiceOpen path, the empirical findings
on which properties Apple's CGL actually consumes (vs ignores) — is
exactly what a CGL shim for a Mesa-based direction needs. Keep as
reference; do not assume any of the code here compiles, runs, or
represents a working baseline. Read it for the architecture findings,
not as load-bearing implementation.

The `notes/` files referenced above are the authoritative empirical
record of what CGL actually does on this target. The implementation
status docs in this tree describe what was *hoped* would happen, not
what was observed.

## Related: kext still publishes IOAccelerator3D = Yes

Separate concern, surfaced here because it's the same failure class.
The live kext publishes `IOAccelerator3D = kOSBooleanTrue` in multiple
places (FB/VMVirtIOFramebuffer.cpp, FB/VMVirtIOGPU.cpp, FB/VMQemuVGA.cpp).
This is the same `crsr = 1` pattern at the kext property level: claim a
capability the system can't deliver. CGL doesn't currently consume it
(the discovery path is broken), so the misleading-advertisement risk is
latent — but per the project pattern, the right state until something
actually renders is `kOSBooleanFalse`. Tracked separately in LEDGER.

## Files in this tree

| File | Status |
|---|---|
| `IMPLEMENTATION_STATUS.md` | Superseded — see top of file |
| `IMPLEMENTATION_V2_SUMMARY.md` | Superseded — see top of file |
| `README.md` | Superseded — see top of file |
| `VMVirtIOGLEngine*.cpp`, `_v1.cpp`, `_v2.cpp`, `_v3_minimal.cpp` | Reference only; not built, not loaded |
| `VMVirtIOGLEngine.bundle/`, `VMVirtIOGLEngine.o`, `VMVirtIOGLEngine.tar.gz` | Built artifacts from Nov 2025; not loadable on a working system |
| `*.sh` build scripts | Reference only |

The build artifacts (`*.bundle`, `*.o`, `*.tar.gz`) and the bare
`VMVirtIOGLEngine` binary could be deleted to reduce confusion, but per
project rule they stay as evidence of what was attempted. Treat as
read-only.

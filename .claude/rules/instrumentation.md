# Instrumentation in the GLD stub

Applies whenever the stub (or any future GLD-side code) wants to observe
engine-owned objects: PP streams, program descriptors, contexts, drawables.

## Raw words, decoded offline — never a foreign decoder in-process

**An instrument deployed inside the GLD bundle captures by copying bytes and
writes them to a file. Decoding happens OFFLINE, in a host-side tool.**

The rule exists because it was violated, and the violation cost a negative
control to untangle. The rung-71 instrument called `glpPPDisassemble` (a
non-exported function in `libGLProgrammability`, resolved by slide+offset)
in-process from the stub. Result: an intermittent crash class in which the
stub's own call damaged heap state that the engine later walked — and on a
live graphics path a stub-induced crash is *indistinguishable from a driver
fault*. Three teardown aborts (`gleFreeTextureState`, `gleFreePixelMap`,
`gleDestroyEnableHashTable`), one in-setup SIGBUS, and one bad free later,
the separation required: an uninstrumented build, three isolation runs, and
a repeated-prefix protocol — most of a day, to answer a question a captured
file would have answered risk-free.

Specifically banned on live paths:
- Calling non-exported functions of system libraries by slide+offset
- Calling any `glp*` / `glvm*` entry from stub code outside the exact
  forwarding the float itself performs
- Anything that mallocs/frees inside a library the engine also uses, while
  the process is mid-scene
- Retaining or freeing memory the engine may traverse later — the damage
  mechanism of the incident was heap state a *library* walked after the
  stub's call returned, so an allocation that merely outlives its call can
  do the same work without calling anything foreign. The ban is on the
  lifetime, not the call: stub-owned allocations must be freed before the
  entry point returns, or moved out of the process entirely.

The offline decode side is cheap: the guest's
`libGLProgrammability.dylib` is byte-identical to the host copy at
`OpenGL.framework.snowleopard/Libraries/` (md5
`a0185546b98c1a020bb9474391155c75`), so a host tool can load it and
disassemble captured streams with zero guest risk. **That identity is a
fact about today's guest image — after any system update or a different
guest, re-verify the md5 on the guest before trusting a host-side decode**
(the same re-read-the-artifact discipline the ledger applies elsewhere).
The stream format needed for capture: flat 8-byte words, `+0x00` u16 type
(`0x8B30` GLSL fragment / `0x8804` raster-op / `0x8B31` vertex — never
sent to the renderer; established at rung 71 by the observation that every
target-0 descriptor arrives at create with `stream=0x0` and never gains
one, while the float's create rejects non-target-2 with the `0x123`
sentinel), `+0x10` u32 word count. See `docs/pipeline-program-abi.md`.

## Crash signatures to check before diagnosing any new guest crash

Before treating a new guest crash as a kext or stub fault, check it
against the closed classes (full chain of evidence in `LEDGER.md`,
rung 71c-71d). The signatures are not GLMark-specific — the destroy-path
family will appear under ANY app that tears down GL contexts; only the
first bullet is app-specific:

- `WaveMesh::update` → `Mesh::update_vbo` memcpy — the glmark2 wave bug,
  fixed upstream; the guest binary `2700b290…` carries the fix.
- Abort in `free` / SIGBUS at context teardown or the next context's first
  use (`gleFreeTextureState`, `gleFreePixelMap`, `gleDestroyEnableHashTable`,
  `gfxFreeTextureLevel`, `gleVPEnable`) — the float's destroy-path
  ownership bug: one allocation cluster (observed `0x1004a5xxx`) is
  written-then-freed across runs; destroys both abort directly and poison
  the successor context's first program bind. Intermittent; dose-suspected;
  minimal prefixes did not reproduce. Closed glmark2-side by
  `GLMARK2_106_FLOAT_STACK` (reuse-context forced, teardown skipped, map
  disabled) — but the bug itself is float-side and will resurface in any
  app that exercises context destroy. **If a crash in this family appears,
  suspect a binary regression first — check the binary's md5 and the two
  console warnings before touching kext or stub.**

## Scores from the CPU path are stability numbers, not performance numbers

A "glmark2 Score N" produced under the GLD route today is measured with the
CPU float rasterising (GLVM) — it says the run completed and terminated
cleanly, nothing about throughput. Do not place it in a table next to the
substitute's 27-40 FPS (GPU, readback present) or the Linux guest's
~1500 FPS (GPU, zero pixel traffic); those measure different things. Label
CPU-path scores as stability results wherever they are recorded.

## Disassembly trap: `__mh_bundle_header` operands

In disassembly of Apple slices, operands printed as `__mh_bundle_header`
are **relocation bases for zero immediates** — `lea __mh_bundle_header(,%rax,4), %r12` is `r12 = rax*4`, and `movq $__mh_bundle_header, -0xc0(%rbp)` stores **0**, not a header address. Reading them as header dereferences has twice produced false structure: the `a2=&_mh_bundle_header` misread that drove the stub's reject path for four rungs (LEDGER, corrected), and the loop-counter disguise in the rung-82b `gldClearDrawBuffer` trace. Recognise the pattern on sight: an instruction that seems to dereference or store the bundle header is almost certainly a relocated zero; confirm from the raw bytes before reasoning about pointer arithmetic built on it.

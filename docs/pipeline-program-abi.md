# The pipeline-program ABI — how shaders and draws reach a GLD renderer

Decoded 2026-08-24 from the guest's own binaries (x86_64 slices):
`GLRendererFloat` (the float), `GLEngine`, `libGLProgrammability`,
`libGLVMPlugin`. Every offset below is a file offset in that slice; on any
other OS build these are hypotheses to re-measure, not facts. Same split as
[`gld-contract.md`](gld-contract.md): STRUCTURAL first, MEASURED after.

This is the ABI the GLD→Mesa translator must speak. The one-line summary:
**`gldModifyPipelineProgram` is only a cache-invalidation ping. The shader
payload is a linearized PP stream hanging off the program descriptor, the
per-draw payload flows through a lazy-swapped function pointer in the
renderer context, and a text form of the IR is recoverable from
`libGLProgrammability` at a fixed offset.**

---

## STRUCTURAL — the mechanism

### 1. There are two lazy-thunk compilers, one per shader stage

The engine and the renderer each keep a *function-pointer slot* that starts
pointing at a "setter" trampoline. First call through the slot: the setter
compiles (or cache-hits) a JIT function, atomically installs it into the
slot, then re-dispatches the original arguments. Every later call goes
straight to the JIT.

- **Vertex: engine-side.** `GLEngine+0x58f0` ← `gleSetVPTransformFuncAll`;
  the engine's own PP token lives at `GLEngine+0x5900`, its compiled function
  at `GLEngine+0x58f8`. Rebinding a program resets both.
- **Fragment: renderer-side.** `renderctx+0x188` ← `gldSetFPTransformFunc`;
  the float's token lives at `renderctx+0x198`, its function at
  `renderctx+0x190`.

Consequence for a substitute GLD: intercepting `gldModifyPipelineProgram`
alone never sees draws. The draw stream crosses at the transform slots and
at the raster entries that call them (~40 indirect call sites inside the
float alone).

### 2. The shader language below GLEngine is the PP stream, never GLSL

GLEngine runs the Khronos **Sh** GLSL compiler inside `libGLProgrammability`
(`ShGetCompilerShaderToProgramString`, `ShGetLinkerShaderToProgramString`,
`PPParser*` preprocessor — all external symbols there). Drivers receive the
linked IR, a flat array of 64-bit words ("PPStream", also called
PPStreamToken in the entry-point names), with binding tables patched in by
`PPStreamTokenConvertAttribBindings` / `ConvertParamBindings`. No hardware
GLD ever sees GLSL source; neither will we.

### 3. The IR has a text form, and it is reachable

`TGenericLinker::getPPStreamString` (non-exported, glp `0x96484`) calls
**`glpPPDisassemble(ppstream)` → malloc'd text** per stage (3 stages, headers
from `linkUtilString`). `glpPPDisassemble` is non-exported at glp `0xd5ba5`
but is a plain C function — callable by slide + offset on this fixed target.
The exported `PPStreamTokenPrint` is an **empty stub** (glp `0x9ec8f`:
`push rbp; mov rsp,rbp; leave; ret`) — do not waste time on it.

### 4. Per-draw state reaches the renderer through context fields, not call args

Texture bindings: `renderctx+0x780[32]` (texture objects; `+0x50/+0x58` are
the qwords copied into stream attrib entries). Uniform buffers:
`renderctx+0x538` array (rebuilt by `gldResetUniformBufferCachePointers`).
Raster state block: `renderctx+0x2e0` (or `+0x470` when `renderctx+0xc0c`
is set), fields `+0x68..+0xb8`. These are read at draw time, so a translator
that replays state into Mesa must read them at the same point.

---

## MEASURED — this build's values

### The GLEngine → renderer dispatch (per-renderer records, stride 0x888)

| Dispatch slot | Entry | Call |
|---|---|---|
| `+0x2b8` | `gldCreatePipelineProgram` | `(engine+0x7128, &prog->0x20[i], prog+0xfe0)` |
| `+0x2c8` | `gldModifyPipelineProgram` | `(engine+0x7128, prog->0x20[i], flags)` |
| `+0x2d0` | `gldUnbindPipelineProgram` | `(engine+0x7120, oldprog->0x18[i])` |

Renderer count at `engine+0x799a` (byte); the GLD context handle the engine
holds is `engine+0x7128` (create/modify) and `engine+0x7120` (unbind) — two
distinct handles. Evidence: `gleModifyPluginPipelineProgram` @ GLEngine
`0x246fb`, `gleBindPipelineProgram` @ `0x21f7f`,
`gleUnbindAndFreePipelineProgramObject` @ `0x2388d`.

**Modify flags:** every observed engine call site passes **`edx=2`**
(bit 1). Bit 0 (value 1) selects the release path. Sites at GLEngine
`0x65ffd`, `0x66cc3`, `0x67d20`, `0x80024`, `0x68318`, `0x68851`,
`0x68b9c`, `0x68f14` — all `movl $0x2, %edx`.

**The +2-per-frame census explained:** `gleModifyPluginPipelineProgram` is
called once per program *array* (`engine+0x5930[target] + 0x20`), and the
engine invokes it for both the target-0 and target-2 program objects.

### `gldModifyPipelineProgram` @ float `0x1a60e`

```
gldModifyPipelineProgram(rdi=ctx, rsi=program, rdx=flags):
  if (program == 0x123) return 0              ; sentinel handle
  if (!(flags & 0x5)) return 0                ; needs bit0 or bit2
  --*(int32*)(program+0x20)                   ; serial bump = invalidate
  if (!(flags & 1)) return 0                  ; flags=2 stops HERE
  ; bit0 = release:
  if (ctx->0x48 > 1 && !(program->0x0->0x3 & 1))
      pthread_mutex_lock(ctx->0x738)
  if (program->0x8)  dec refcount (u16 at +0x2); if 0 PPStreamTokenFree; null
  gldReleaseGLVMProgramList(program->0x18)    ; per node: glvmReleaseFunction + free
  program->0x18 = NULL
  unlock; return 0
```

That is the entire entry point. **It receives no shader data.** The data
arrived earlier at create (the descriptor) and is (re)derived lazily at the
transform slot.

### `gldCreatePipelineProgram` @ float `0x1a6b0`

`(rdi=ctx, rsi=outSlot, rdx=descriptor)`. Accepts only descriptors whose
`+0x2` byte (target) `== 2`; otherwise writes the sentinel `0x123` into
`*outSlot` and returns 0. On accept, mallocs the 0x28-byte program object:

| Offset | Meaning |
|---|---|
| `+0x00` | descriptor pointer (engine-owned, see below) |
| `+0x08` | PPStream (built by `gldLoadPipelineProgram`) |
| `+0x10` | ctx back-pointer (set at load) |
| `+0x18` | GLVM function-cache list head (MRU, ≤32 nodes, LRU evict) |
| `+0x20` | 32-bit serial: −1 at create, `= ctx->0xc10` at load, −− on modify |

### The engine-side program object (target 0 → 0x1010 bytes; 1 → 0x20; 2 → 0x1008)

From `gleCreatePipelineProgramObject` @ `0x520c` and `gleBindPipelineProgram`:

- `+0x10` GL name, `+0x14` refcount, `+0x18` first of the per-renderer bound
  handles, `+0x20[8]` the create out-slots (what `+0x2b8` filled).
- `+0x110`: 0xec8-byte parameter area, pattern-filled `0x04000400`
  (targets 0/1) or `0x10001000` (target 2); registered into the engine at
  `engine->0x4868[target]`.
- `+0x104` u16 linked flag; `+0x10a` u16 attribute count.
- **`+0xfe0` — the descriptor passed to the GLD create**:
  `+0xfe0` u16 shader type at link time (`0x8B30` fragment / `0x8B31`
  vertex), `+0xfe2` byte target, `+0xfe3` flags byte (bit0 set when name 0),
  `+0xfe4` dword, **`+0xfe8` the linked PP stream tree** (0 until linked),
  `+0xff0` −1 (64-bit), `+0xff8`/`+0x1000`/`+0x1008` zeroed.

### `gldLoadPipelineProgram` @ float `0x1a2f7` (non-exported; the data path)

`(rdi=ctx, rsi=program)` — called lazily from the transform build when
`program->0x10 != ctx`:

1. desc = `program->0x0`.
2. Fragment (`desc+0x00 == 0x8B30`): with an existing token,
   `PPStreamTokenAddTexUnitInfo(token, desc->0x18)` and then the whole GLVM
   cache list is released (and `ctx->0x188` reset to `gldSetFPTransformFunc`,
   `ctx->0x190` released). Without one: `token =
   glpPPShaderLinearize(desc->0x08, 0x22e8)`, then
   `PPStreamTokenAddTexImages(token,0,0)`, `AddTexUnitInfo`, `AddBlockInfo`.
3. Vertex (`0x8B31`): token = `desc->0x08` directly (no linearize);
   `PPStreamTokenAddTexImages(token,1,1)`.
4. Both: `PPStreamTokenConvertAttribBindings(token)`,
   `ConvertParamBindings(token)`, token refcount (u16 `+0x2`) = 1.
5. **Attrib patch walk** — entries at `token + 8*n(+0x54)`, `n(+0x50)`
   entries ×16 bytes: `entry[0]&0x1f` = texture unit; if
   `texobj = ctx->0x780[unit]`: `entry->0 = texobj->0x50`,
   `entry->8 = texobj->0x58`, dword merge keeps low 5 bits (unit) and old
   bits 5–15; `entry->7` bit 3 = bit `unit` of `ctx->0xc18`
   (enabled-units bitmask).
6. `program->0x08 = gldAddRasterOpsToPPStream(ctx, token)`;
   `program->0x20 = ctx->0xc10`; `program->0x10 = ctx`.

`gldAddRasterOpsToPPStream` @ `0x1a824` merges renderer raster state into
the token before compile: viewport-ish dwords `ctx->0x2e8/0x2ec` divided by
a scale (1, or from `ctx->0x2a4/0x274`), mode bits from `ctx->0x26a/0x29a`,
object mode `ctx->0x220->0x28`, the `0x400/0x8000/0x8000000` mode triad —
assembled into a stack raster-op record fed to `PPStreamTokenAddRasterOps`.

### `gldBuildFPTransformFunc` @ float `0x1470` (non-exported)

Called only from `gldSetFPTransformFunc`. Builds/looks up the JIT function
for the *current state* and installs it:

- `ctx->0x188` ← `gldLLVMFPTransform` (or `…Fallback` when `ctx->0xc30`
  non-NULL) — the interpreter thunks; the JIT target replaces them via
  `glvmRequestFunctionPointerWrite`.
- prog = `ctx->0x778` (bound fragment program); if `ctx->0x770` (the other
  program) has `desc->0x34|0x2c` nonzero →
  `gldResetUniformBufferCachePointers(ctx, ctx->0x770)`.
- **fpKey** bits: bit0 = `ctx->0x740->0x32dc == 0x1D00`; bit1 (only when
  `ctx->0xc30==0`, `ctx->0x220==0`): `ctx->0x360&0xF`, or
  (`ctx->0x260==0x400 && ctx->0x2e8&7`), or (`ctx->0x260==0x8000 &&
  ctx->0x2e8&3`); bit2 = `ctx->0x220==0 && ctx->0x218->0x78 != 0`
  (`+0x218` is the drawable).
- Cache key = (fpKey, `2*n(+0x78)`, `4*n(+0x50)`, memcmp of the
  `token+8*n(+0x7c)` array, memcmp of the `token+8*n(+0x54)` array) over
  the program's node list; node layout (0x1c8 bytes): `+0x00` func,
  `+0x08` next, `+0x10` prev, `+0x18` key-array copy 1, `+0xb8` copy 2,
  `+0x1b8`/`+0x1bc` lengths, `+0x1c0` fpKey. 32-node cap.
- Miss: `func = glvmCreateModularFunction(0x10, &fpKey, 1)` — **its
  arguments are ignored** (glp `0x454c`: bare `glvm_function_new`) — then
  `glvmBuildModularFunctionDeferred(func, 0, token, 8*token->0x10)` which
  memcpy's that many bytes of the stream into `func->0x98` and queues the
  background JIT (`glvm_deferred_build_modular` via `glvmAddWork`).
- Install: token refcount swap into `ctx->0x198`;
  `glvmRequestFunctionPointerWrite(func, &ctx->0x188)` (atomic slot write);
  `ctx->0x190 = func`; `glvmRetainFunction`; `ctx->0x4c = fpKey`.

### The transform-call ABI (what `ctx->0x188` receives)

`gldSetFPTransformFunc` @ `0x1e00` saves all six args
`(rdi=ctx, esi, edx, rcx, r8, r9d)`, builds, then re-calls
`ctx->0x188(saved args)`. The float's interpreter
`gldLLVMFPTransform` @ `0x1b80` maps them:

```
rdi = ctx
esi, edx                       — passed through to the interpreter (ints)
rcx, r8                        — passed through (pointers/ints)
r9d                            — COUNT: the loop steps 4 per iteration
ctx->0x180 → interpreter arg rcx     ctx->0x198 (token) → interpreter arg r8
ctx->0x740 (GL state) → rdi          prog->0x0->0x18 (GLVM list) → rsi
&rctx block (ctx+0x2e0, +0x68..+0xb8; ctx+0x470 if ctx->0xc0c) → stack[1]
ctx+0x780 (32 texture objects) → stack[0]; the 4 remaining stack slots
carry (rcx, r8, esi, edx)
```

One `glvmPreloadFPTransformFour(rdi=state, rsi=list, rdx=&ctx->0x538,
rcx=ctx->0x180, r8=token, r9=ctx->0x40, stack…)` then
`glvmInterpretFPTransformFour(same)` per 4-wide step.

### The PPStream (a.k.a. PPStreamToken) layout

Flat array of 8-byte words, self-describing:

| Offset | Field |
|---|---|
| `+0x00` | u16 shader type (`0x8B30`/`0x8B31`) |
| `+0x02` | u16 refcount (PPStreamTokenFree path) |
| `+0x03` | u8 flags (bit0 = immutable/no-lock) |
| `+0x10` | u32 word count — total size `8*count`; also the memcpy length into the GLVM function |
| `+0x50` | u32 attrib-entry count (16-byte entries) |
| `+0x54` | u32 offset/entry count for the attrib array base `token+8*n` |
| `+0x78`/`+0x7c` | sampler-key array count / base `token+8*n` (8-byte entries) |

IR alphabet (external symbols in glp): operations `PPStreamAddOperation`,
constants `AddConstant`, control flow `AddLabel`, bindings
`AddAttribBinding`/`AddParamBinding(Array)`/`AddOutputBinding`, state
`AddRasterOp`/`AddOption`/`AddReqs`, textures `AddTexImage`, temporaries
`AddTempUsage(Array)`, addresses `AddAddressUsage`; chunk management
`PPStreamChunkCreate(FromChunk)`, `ChunkList*`, `PPStreamCompare`,
`PPStreamAttachStream`. Single-op decoder: `glpDisassemble1Op`
(non-exported, glp `0xd3dfb`).

`glpPPShaderLinearize` @ glp `0x4ccee`: `(rdi=stream, esi=flags)`;
`flags & 0x27EF` nonzero → re-linearize through
`PPStreamCreate`/`AttachStream`/`glpPPShaderLinearizeStreamMgr`;
`flags & 0x1800` only → raw memcpy of `8*stream->0x10` bytes. The float
calls it with `0x22E8`.

### Monolithic (prebuilt) functions

Fixed-function stages (point render seen: `gldSetPointRenderFunc` @ float
`0x1e60`) use `glvmObtainMonolithicFunction` keyed on a 4-dword state key
(`ctx+0x1d0..0x1dc`, cached against `+0x1d0` shadow), called as
`(0x0B, &key, 4, "gldLLVMVecPointRender", "llvm_point_const_")`, installed
into `ctx+0x1a0` by the same `glvmRequestFunctionPointerWrite` pattern.

---

## What this means for the translator

1. **The shader text is obtainable**: at create/load time, `desc->0x08` is
   the stream; `glpPPDisassemble` (slide `+0xd5ba5` in
   `libGLProgrammability`, x86_64 slice of this build) returns malloc'd
   text per stage. **The first working translator reads text and emits
   GLSL — the word format need not be understood, only parsed.** Decoding
   the binary IR is an optimization, not a prerequisite.
2. **Vertex and fragment must both be translated**: the engine hands the
   renderer program objects for both targets (`+2` modify calls per frame),
   and engine-side vertex GLVM runs regardless (the suppression matrix,
   rung 69b — unchanged by anything here).
3. **State capture happens at draw time**: textures from `ctx->0x780`,
   uniforms via `ctx->0x538`, raster block from `ctx->0x2e0` — all read
   inside the transform path, so the translator hooks the same slots
   (`ctx->0x188`, the raster entries) rather than Modify.
4. **`gldModifyPipelineProgram`'s only useful signal is the serial bump**:
   it tells the translator "the program object's streams may have been
   re-patched — re-read the bindings".
5. **The thunk decides the per-draw hook strategy.** The slot
   `ctx->0x188` is swapped atomically after the first call, so a hook
   installed late sees a slot that has already moved, and a hook on
   `gldSetFPTransformFunc` sees only the first call. Choose deliberately:
   keep the thunk (intercept the setter and never let the float's build
   install its function), or hook the interpreter entries
   (`glvmInterpretFPTransformFour` is exported by glp).
6. **Verify the stream extent two ways before trusting `+0x10`.** The
   float's own statement of the byte count is the `ecx` it passes to
   `glvmBuildModularFunctionDeferred`: `8*stream->0x10`. From the stub
   that call is not directly visible, but `glpPPDisassemble` traverses
   the stream with its own parser — a dump that completes with
   well-formed text is an independent reading of the same extent. Header
   count and dump must agree; if they do not, the word-format decode
   moves from optimization to blocker.

### Open items this decode does not settle

- The **word-level IR encoding** (what each 8-byte operation word contains)
  — decodable from `glpDisassemble1Op`/`PPStreamAddOperation` but not done
  yet; the text dump makes it unnecessary for a first translator.
- The meaning of the `0x1D00/0x1D01` state constants at `ctx->0x740->0x32dc`.
- Whether `desc->0x08` and `desc->0x18` differ between the two create
  targets beyond the type word (only target 2 reaches the float's create).

---

## RUNTIME VERIFICATION (2026-08-24, rung 71) — the decode held

Instrument: the stub's `gldCreatePipelineProgram`/`gldModifyPipelineProgram`
(dump at call time, forward intact). Subject: GLMark, bare launch, GLD
route. Seven scenes ran; every result below is from that log.

- **`glpPPDisassemble` works from the stub** — glp image base
  `0x7fff80d9a000`, entry resolved at `+0xd5ba5`, malloc'd text returned
  and freed cleanly, 7/7 fragment streams disassembled.
- **Fragment text is complete, readable ARB-style assembly** — headers
  `!!ARBfragmentshader`, `TEMP`/`ATTRIB`/`PARAM`/`OUTPUT` declarations, a
  `main:` body of `MOV/NRM/DOT/MAX/MUL/ADD/POW` with swizzles, `RET (TR)`,
  `END`. **Uniforms are baked in as PARAM literals** (`{20.0, 20.0, 10.0}`
  light direction, `{100.0}` shininess, material colors) — per-frame
  uniform updates flow outside the stream (the `ctx+0x538` cache, or
  relink).
- **Vertex streams never reach the renderer.** Target-0 descriptors arrive
  at create with `stream=0x0` and never gain one; the float's create
  rejects them (sentinel `0x123`). Vertex is wholly engine-side — the
  static decode's engine-thunk finding, now runtime-confirmed.
- **The fallback capture point is required and sufficient**: creates
  always see `stream=0x0`; the stream first appears at the first modify
  after link.
- **Extent cross-check passed across the range**: words 30→197 chars,
  42→385, 110→1135; the disassembler's traversal ends at a clean `END`
  every time. The `+0x10` word-count interpretation stands.
- **The type taxonomy is wider than the static decode**: target 2 carries
  both `0x8B30` (GLSL fragment) and `0x8804` (**raster-op programs**,
  always 24 words — the `gldAddRasterOpsToPPStream` output); target 1 is
  `0x8DD9`; target 0 is `0x8B31` (never streamed). A translator needs the
  `0x8804` stream too — the instrument's type guard skipped it.
- **Unattributed residual**: a teardown crash after scene 7
  (`gleFreeTextureState+115` ← `gleTerminateContext` ← `CGLReleaseContext`
  ← `-[NSOpenGLContext dealloc]` ← `GLStateMacOS::reset()`). No stub frame
  in the chain; same class as prior glmark2 teardown crashes; not excluded
  as instrument-related without an uninstrumented negative control.

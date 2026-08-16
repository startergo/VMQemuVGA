# 3D architecture — from GL call to GPU

How a guest OpenGL call reaches the host GPU, and what each GL object becomes
along the way. Companion to [`.claude/rules/acceleration.md`](../.claude/rules/acceleration.md);
current status of each piece lives in [`LEDGER.md`](../LEDGER.md).

This describes the **per-process GL stack** (app → substitute
`OpenGL.framework` → Mesa → virgl → kext → host). It is one of two
independent acceleration stacks in this project. The other — the system-wide,
in-kernel 2D surface path that WindowServer composites through, containing no
GL at all — is described in
[`docs/accelerator-surface-path.md`](accelerator-surface-path.md). Giving
WindowServer a working 2D surface path did not give it 3D; GL still reaches
apps only through the stack below.

---

## Layer stack

```
GUEST — macOS 10.6.8 x86_64 (UTM VM)
┌──────────────────────────────────────────────────────────────────────────┐
│  Application  (Gecko / WebKit / any GL app)                              │
│      NSOpenGLContext          CGLCreateContext, CGLFlushDrawable …       │
└───────────┬───────────────────────────┬──────────────────────────────────┘
            │ ObjC swizzle              │ __DATA,__interpose
┌───────────▼───────────────────────────▼──────────────────────────────────┐
│  cgl-shim            presentation verified; dispatch SOLVED two ways      │
│  9 NSOpenGLContext swizzles + _CGL* interposes + drawRect swizzle         │
│  GL dispatch, both routes landed (per Mesa-VirGL commit log —            │
│  not re-verified from this repo):                                        │
│    flat-namespace tests: __DATA,__interpose gl* → Mesa via dlsym         │
│    real two-level apps: substitute OpenGL.framework + IOSurface upload   │
│  (Gecko UI renders via the substitute — Mesa-VirGL 9fb95e8)              │
└───────────┬──────────────────────────────────────────────────────────────┘
            │ OSMesaCreateContextExt / OSMesaMakeCurrent
┌───────────▼──────────────────────────────────────────────────────────────┐
│  libOSMesa.8.dylib          BUILT + softpipe-VERIFIED + virgl-VERIFIED   │
│    ├─ OSMesa frontend  — context, readback framebuffer                   │
│    ├─ Mesa GL state tracker  (src/mesa/state_tracker)                    │
│    │     GLSL ──► NIR ──► TGSI                                           │
│    └─ Gallium  (pipe_screen / pipe_context / pipe_resource)              │
│            │                                    │                        │
│      GALLIUM_DRIVER=softpipe             GALLIUM_DRIVER=virgl            │
│            │ (reference renderer)               │                        │
│            ▼                                    ▼                        │
│      CPU rasteriser                    virgl driver                      │
│                                        virgl_encode.c → command buffer   │
└─────────────────────────────────────────────┬────────────────────────────┘
                                              │ struct virgl_winsys
┌─────────────────────────────────────────────▼────────────────────────────┐
│  virgl_iokit_winsys          VERIFIED — 25 vtable entries                │
│  6 active · 19 stub/bookkeeping/trivial (matches vtest shape)            │
│  resource backing = align_malloc'd USERSPACE memory                      │
└─────────────────────────────────────────────┬────────────────────────────┘
                                              │ IOConnectCallMethod
┌─────────────────────────────────────────────▼────────────────────────────┐
│  VMVirtIOGPUUserClient       10 winsys selectors (0x6000-0x6009) VERIFIED │
│  + 1 ATTACH_BACKING probe (0x5000) + legacy 0x3000-range                  │
│  IOMemoryDescriptor::withAddressRange(addr, len, dir, m_task)            │
│  prepare()  ─ wiring must hold for the RESOURCE lifetime ─  complete()   │
└─────────────────────────────────────────────┬────────────────────────────┘
                                              │
┌─────────────────────────────────────────────▼────────────────────────────┐
│  VMVirtIOGPU  (kext)          TRANSPORT VERIFIED                         │
│  control queue 0 · split virtqueue · descriptor chain · used-ring poll   │
│  response validation 0x1100 / 0x1200                                     │
└─────────────────────────────────────────────┬────────────────────────────┘
                                              │ virtio-gpu PCI  1af4:1050
════════════════════════════════════════ VM boundary ══════════════════════
┌─────────────────────────────────────────────▼────────────────────────────┐
│  QEMU  virtio-gpu-gl device                                              │
│  VIRTIO_GPU_FILL_CMD size check → virtio_gpu_virgl_process_cmd           │
└─────────────────────────────────────────────┬────────────────────────────┘
┌─────────────────────────────────────────────▼────────────────────────────┐
│  virglrenderer  (embedded in UTM)                                        │
│  vrend_decode  ── TGSI ──► GLSL ──►  libepoxy dispatch                   │
└───────────────┬─────────────────────────────────┬────────────────────────┘
       gl=es    │                                 │   gl=core
┌───────────────▼──────────────┐   ┌──────────────▼────────────────────────┐
│  ANGLE  libEGL/libGLESv2     │   │  OpenGL.framework (CGL)               │
│  GLSL ──► Metal shaders      │   │  desktop GL 4.1 core profile          │
└───────────────┬──────────────┘   └──────────────┬────────────────────────┘
                └─────────────┬──────────────────-┘
                              ▼
                        Metal / GPU
                              │
                    SPICE ─► CocoaSpice ─► UTM window
```

---

## What each GL object becomes

| Guest GL | Gallium | virgl wire | Host |
|---|---|---|---|
| `GLXContext` / CGL context | `pipe_context` | `CTX_CREATE` (0x0200), `ctx_id` | virglrenderer context |
| Texture | `pipe_resource` | `RESOURCE_CREATE_3D` (0x0204) + `ATTACH_BACKING` (0x0106) | host GL texture |
| VBO / IBO / UBO | `pipe_resource` | same | host GL buffer |
| Renderbuffer / FBO attachment | `pipe_surface` | `CREATE_OBJECT`(`VIRGL_OBJECT_SURFACE`) then `SET_FRAMEBUFFER_STATE` | host FBO binding |
| Vertex shader / fragment shader | `pipe_shader_state` (TGSI) | `CREATE_OBJECT`(`VIRGL_OBJECT_SHADER`) | GLSL, then ANGLE→Metal or native GL |
| Sampler, blend, rasteriser, DSA state | `pipe_*_state` | `CREATE_OBJECT` of the matching type | host GL state |
| Vertex attrib layout | `pipe_vertex_element_state` | `CREATE_OBJECT`(`VIRGL_OBJECT_VERTEX_ELEMENTS`) | host VAO |
| `glDrawArrays` / `glDrawElements` | `pipe_context::draw_vbo` | `DRAW_VBO` in a command buffer, shipped by `SUBMIT_3D` (0x0203) | host draw |
| `glClear` | `pipe_context::clear` | `CLEAR` in a command buffer | host clear |
| `glTexSubImage` (upload) | `texture_subdata` | `TRANSFER_TO_HOST_3D` (0x0201) | host texture write |
| `glReadPixels` (readback) | `transfer_map` | `TRANSFER_FROM_HOST_3D` (0x0202) | host texture read |
| Delete | resource destroy | `RESOURCE_UNREF` (0x0102) | host resource free |

**Shader translation is three hops:** GLSL → TGSI in the guest (Mesa), TGSI →
GLSL in virglrenderer, GLSL → Metal in ANGLE under `gl=es`. Under `gl=core` the
last hop is native. Nothing in the chain ships SPIR-V.

**Object handles are namespaced per context**, allocated by the guest. Every 3D
command must carry `hdr.ctx_id`; a zero there routes to the wrong context or is
rejected.

---

## Where the bytes live

Resource *backing* is guest memory, not host memory. The winsys `align_malloc`s
it in the application's address space and returns that pointer from
`resource_map` — Mesa writes vertices and texture data straight into it. The
kext describes those pages to the device as a scatter list of guest-physical
segments via `ATTACH_BACKING`, and the host reads or writes through that list on
each `TRANSFER_*_3D`.

Consequences:

- The pages must stay **wired** for as long as the resource exists, not just for
  the duration of one transfer. `prepare()` at attach, `complete()` at teardown.
- An unaligned `align_malloc(size, 64)` produces a **multi-segment** scatter
  list with a partial first page — unlike every allocation the display path has
  used so far, which were page-aligned and contiguous.
- `resource_map` is trivial precisely *because* the backing is guest memory.
  There is no `clientMemoryForType` / `IOConnectMapMemory` in this design.

---

## Traps on this path

**On virgl-backed commands, `0x1100` means "QEMU parsed it", never "host accepted it."**
Three known instances: SUBMIT_3D (decode errors go to host log only),
VIRTIO_GPU_FILL_CMD size mismatches (the wrong struct size is silently
accepted — this hid the `virtio_gpu_box` (24 B) vs `virtio_gpu_rect`
(16 B) bug for a session), RESOURCE_CREATE_3D (QEMU discards
virgl_renderer_resource_create's EINVAL).
Only readback proves acceptance. The UTM host debug log
(`<VM>.utm/Data/debug.log`) is a required artifact for any virgl-backed
command failure. See LEDGER.md for the full rule with the conditional
diff-target caveat.

**The kext's command buffer has a size limit.** `submitCommand` copies each
command into a pre-allocated physically-contiguous buffer (`m_cmd_buf`,
currently 4 KB). ATTACH_BACKING commands carry one scatter-list entry per
page of backing — a 128 KB resource produces 32 entries = 392 bytes, a
4 MB texture produces 1024 entries = 12 KB. Commands exceeding the buffer
capacity are rejected with `kIOReturnNoMemory` (the defensive check added
in Increment C). Previously the limit was 256 bytes with a matching
`cmd_size > 256` parameter gate; both silently rejected ATTACH_BACKING
commands from Mesa's 256×256 resources, producing the same "success code
but wrong outcome" shape as the `0x1100` traps. The probe's 64×64 resource
(5 entries, 68 bytes) never hit the limit.

**The host log is a required artifact**, not a fallback. `qemu_log_mask(LOG_GUEST_ERROR)`
output and virglrenderer decode errors appear only in UTM's per-VM debug log,
which must be enabled in VM settings.

**Capsets bound what the guest can ask for.** The device advertises VIRGL
(id 1, v1, 308 B) and VIRGL2 (id 2, v2, 1408 B); the host's real ceiling depends
on whether the VM runs `gl=es` or `gl=core`, so that setting is a variable to
control in experiments.

**Drawing into a view on 10.6 must happen inside `drawRect:`.** Five
presentation approaches were tried before finding the one that works:

1. `dispatch_async(main_queue)` + `NSBitmapImageRep drawInRect` —
   **no visible output.** dispatch_async runs in a mode where the
   view's graphics context is not set up for drawing. No error, no
   exception, just nothing on screen.
2. `lockFocus` / `unlockFocus` — **deadlock.** AppKit re-enters the
   run loop inside lockFocus on 10.6.
3. `CGImageRef` + `CGContextDrawImage` (via `graphicsPort`) —
   **no visible output.** Same graphics-context issue as #1.
4. `CALayer` (`setWantsLayer:YES` + `layer.contents`) — **hangs
   AppKit** when called from `-setView:` before the view is in a
   window. Core Animation requires a window-hosted layer hierarchy.
   This failure is call-site-specific, not a fundamental limitation
   of layer-backed presentation — it may work if set up lazily after
   the view enters a window.
5. **`performSelectorOnMainThread:setNeedsDisplay:` + `drawRect:`
   swizzle** — **works.** `performSelectorOnMainThread` runs in
   `NSDefaultRunLoopMode`, where the view's display cycle provides
   a valid graphics context. The swizzled `drawRect:` reads
   `present_buf` from the view's associated object and draws via
   `NSBitmapImageRep drawInRect`. This is the killtest's exact
   pattern — timer callback → drawInRect — generalised to any view.

The rule: **on 10.6, view drawing must happen inside the view's own
display cycle, not from an arbitrary main-thread block.** The
graphics context is only valid within `drawRect:` (or
`lockFocus`/`unlockFocus`, but that deadlocks from
`dispatch_async`). `performSelectorOnMainThread` + `setNeedsDisplay:`
is the mechanism that enters the display cycle from another thread.

---

## Status

| Layer | State |
|---|---|
| virtio-gpu transport (kext) | **verified** — clear → readback byte-exact, negative-control confirmed |
| `ATTACH_BACKING` with userspace memory | **verified** — unaligned 16 KB malloc, 5-segment scatter list (partial page, 3 full, partial page), 4096/4096 dwords correct, wiring held across the guest write between transfers |
| `virgl_iokit_winsys` | **verified** — Mesa-driven `glClear`+`glReadPixels` byte-exact via virgl, softpipe reference identical (Mesa-VirGL commit c703f8fb910) |
| Mesa + Gallium + virgl driver (guest) | **built and runtime-verified** via softpipe + virgl (clear, triangle, textured triangle, shaders+textures+sampler state) |
| `cgl-shim` | **Presentation verified; GL dispatch solved on both routes** (state per Mesa-VirGL commit log, branch `cross-10.6` — not re-verified from this repo this session). Flat-namespace test binaries route via `__DATA,__interpose` gl* entries dispatching to Mesa (`fd7b7cf`); two-level-namespace real apps route via the substitute `OpenGL.framework` (`4b7c463`) — Gecko UI renders through it (`9fb95e8`), with the frontier now at rendering-correctness artifacts (empty-frame blit `8775b09`, pixel-store state `26c82b3`, single-buffer diagnostic `685ba319` — the double-buffer staleness hypothesis for the PowerFox chrome artifacts was FALSIFIED). Killtest, stress (resize + multi-context + CGLEnable) and smoke tests PASS per the same log. |
| virglrenderer / ANGLE (host) | stock UTM, unmodified |
| Next milestone | **Gecko/PowerFox chrome-artifact class** — same content drawn twice at different destinations; guest-side buffer staleness falsified by the `SHIM_SINGLE_BUF` discriminator, so the fault is elsewhere in the presentation chain (substitute IOSurface upload / blit flip / Gecko's own compositing). This is the open frontier of the GL stack; work lives in the Mesa-VirGL repo. The 2D accelerator surface path (separate stack, no GL) has its own open items — see [`accelerator-surface-path.md`](accelerator-surface-path.md). |

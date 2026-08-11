# VMQemuVGA

Display driver (IOKit kext) for **macOS 10.6.8 Snow Leopard** guests running
under QEMU/UTM, with support for both QXL and VirtIO GPU devices.

Fork of `ivanagui2/VMQemuVGA`, itself derived from Zenith432's original.

---

## Status

**Working: full 2D display through VirtIO GPU, plus a working 3D path.** As
of 2026-08-09 the driver brings up a correct Snow Leopard desktop under UTM on
an Apple Silicon host — correct rendering, correct cursor, working resolution
changes. The 3D command pipeline is verified end-to-end at the transport layer
on `virtio-vga-gl`: a guest-built virgl CLEAR writes the correct bytes to a host
resource and the guest reads them back via `TRANSFER_FROM_HOST_3D`
(negative-control-confirmed — see [3D status](#3d-status)). Other variants not
re-tested for 3D this session.

**Working: 3D rendering through test applications.** As of 2026-08-10 the
userspace half exists. Mesa is cross-built for 10.6 with a new
`virgl_iokit_winsys` that talks to the kext's user client, and a CGL shim routes
`NSOpenGLContext` onto it. Mesa-driven clears, triangles and textured triangles
return byte-exact pixels through virgl to the host GPU, and a shimmed Cocoa app
renders visibly on the guest desktop — see [3D status](#3d-status).

**Not yet verified: real applications.** Gecko and WebKit have not been run
against the shim. Multiple concurrent contexts, resize under load, and sustained
frame rates are untested, and performance under a real workload is unmeasured.

This README states only what has been verified by a live boot, a negative
control, or a visual check. Claims that rest on a success log alone are marked
as such. Current state lives in [`LEDGER.md`](LEDGER.md); this file is the
summary.

---

## Verified working

Verified on `virtio-gpu-gl-pci` unless noted.

| Capability | How it was verified |
|---|---|
| VirtIO split-virtqueue transport | Real descriptor table, avail/used rings, notification, used-ring polling. Response codes validated against `0x1100`/`0x1200`. |
| Error path | Negative control: `SET_SCANOUT` on a nonexistent resource returns `0x1203` and is surfaced as `kIOReturnError`. |
| Feature negotiation | `VIRTIO_GPU_F_VIRGL` + `VIRTIO_F_VERSION_1` negotiated; queue size 256. |
| 2D display pipeline | `RESOURCE_CREATE_2D` → `ATTACH_BACKING` → `SET_SCANOUT` → `TRANSFER_TO_HOST_2D` → `RESOURCE_FLUSH`, all accepted by the host. |
| WindowServer integration | Standard `IOFramebuffer` path: mode enumeration, `getApertureRange`, `getVRAMRange`, `setDisplayMode`. |
| Desktop rendering | Visual check 2026-08-09 — correct desktop, no shearing or artifacts. |
| Cursor | Visual check 2026-08-09 — software cursor renders correctly. |
| Resolution changes | Real user-driven mode switch, resource recreated against a stable buffer, aperture mapping preserved. |
| Resource tracking | Self-test probe at boot: create → duplicate-reject → destroy → verify-gone. |
| Cursor queue transport | Self-test probe at boot: `UPDATE_CURSOR` + `MOVE_CURSOR` on queue 1. Used-ring advances, both commands accepted. |
| 3D transport | Self-test probe at boot: CTX_CREATE → RESOURCE_CREATE_3D → ATTACH_BACKING → CTX_ATTACH_RESOURCE → CREATE_OBJECT(surface) → SET_FRAMEBUFFER_STATE+CLEAR → TRANSFER_FROM_HOST_3D. Byte-equal positive + negative control (different clear colors produce different readback bytes, byte-perfect unorm match on all 64 dwords). Verified 2026-08-09 on `virtio-vga-gl`; other variants not re-tested this session. |
| QXL path | Verified 2026-08-09 — VMQemuVGA class, separate code path, mode switches work. |
| Mesa on 10.6 | `libOSMesa.8.dylib` cross-built (913 targets, zero undefined symbols) and runtime-verified on the guest — softpipe renders byte-exact clears. |
| `virgl_iokit_winsys` | Mesa-driven `glClear` + `glReadPixels` byte-exact through virgl; `GALLIUM_DRIVER=softpipe` in the same binary gives an identical result. |
| 3D rendering | Triangle and textured triangle PASS on virgl, 3/3 pixels, matching the softpipe reference. Exercises GLSL compilation, shader objects, vertex buffers, vertex element state, textures, sampler state and `DRAW_VBO`. |
| CGL shim | Visual check 2026-08-10 — a Cocoa app using `NSOpenGLContext` renders a red window on the guest desktop through the shim. |

### Devices

- `virtio-gpu-gl-pci` — primary target, verified 2026-08-09
- `virtio-ramfb-gl` — verified 2026-08-09
- `virtio-vga-gl` — verified 2026-08-09 (required `useNativeScanout` fix; VGA
  compat no longer gates the rendering path)
- QXL / QEMU std VGA — verified 2026-08-09 (VMQemuVGA class, separate path)

---

## 3D status

**Transport verified; rendering verified through test applications.**

- The host offers `VIRTIO_GPU_F_VIRGL`, and the driver reads the capset table
  (VIRGL and VIRGL2 both advertised).
- The transport pipeline is verified end-to-end at the byte level: CTX_CREATE →
  RESOURCE_CREATE_3D → ATTACH_BACKING → CTX_ATTACH_RESOURCE → CREATE_OBJECT
  (surface) → SET_FRAMEBUFFER_STATE+CLEAR → TRANSFER_FROM_HOST_3D produces the
  correct bytes in the guest's backing memory, confirmed by a negative control
  (different clear color → different readback, byte-perfect unorm match on all
  64 dwords in both directions). CREATE_OBJECT's surface payload and
  SET_FRAMEBUFFER_STATE's binding were closed transitively — CLEAR could not
  have written without both being correct on the wire, even though neither
  produces a guest-visible signal (SUBMIT_3D returns `0x1100` unconditionally;
  virgl decode errors land in the QEMU host log, not the guest response ring).
- **The guest half of GL command generation now exists.** virtio-gpu 3D assumes
  the guest compiles GLSL to TGSI before anything leaves the VM — via Mesa and
  Gallium, neither of which shipped with Snow Leopard. Mesa is now cross-built
  for 10.6 x86_64 (913 targets, zero undefined symbols against libSystem plus
  libcxx 5.0.1 and small compat shims), and a new `virgl_iokit_winsys` connects
  Mesa's virgl driver to the kext through ten user-client selectors. The kext
  remains transport by design; command generation is userspace, as intended.
- **Rendering is verified through test applications.** Mesa-driven `glClear`,
  a triangle, and a textured triangle all return byte-exact pixels through
  virgl, each matching `GALLIUM_DRIVER=softpipe` run from the same binary. That
  covers GLSL compilation, shader objects, vertex buffers, vertex element state,
  textures, sampler state and `DRAW_VBO` — including the three-hop shader
  translation chain (GLSL → TGSI in Mesa, TGSI → GLSL in virglrenderer,
  GLSL → Metal in ANGLE).
- **A CGL shim connects applications to it.** Nine `NSOpenGLContext` swizzles
  plus four `_CGL*` interposes, loaded via `DYLD_INSERT_LIBRARIES`, back
  contexts with OSMesa and present through a swizzled `drawRect:`. A Cocoa test
  app renders visibly on the guest desktop.
- **Not yet covered:** real applications (Gecko, WebKit), multiple concurrent
  contexts, resize under load, sustained frame rates, and resource reuse across
  frames. Performance under a real workload is unmeasured.

The `GLPlugin/` tree is **superseded 2026-08-09** (see
[`GLPlugin/SUPERSEDED.md`](GLPlugin/SUPERSEDED.md)). It attempted to replace
`GLEngine.bundle` directly; CGL never discovered the renderer on either 10.6 or
10.15, and the remaining gap was the entire GL spec. Kept as reference for the
GLI/CGL plumbing findings, which are still useful for a future CGL shim. The
chosen direction is Mesa + virgl + a CGL shim — see
[`.claude/rules/acceleration.md`](.claude/rules/acceleration.md) for the seam
analysis. `virglrenderer-metal/` is a shelved host-side experiment that has
never compiled inside virglrenderer and targets a Linux/Mesa guest.

If you need 3D in a Snow Leopard VM today, this driver plus the Mesa build and
CGL shim will render through the host GPU — but only test applications have been
run against it so far.

---

## Requirements

- macOS 10.6.8 guest, x86_64
- QEMU or UTM host with a VirtIO GPU or QXL display device
- **One vCPU during development.** SMP under TCG emulation produces TLB
  shootdown IPI panics that are artifacts of emulated APIC timing, not driver
  bugs.

Performance note: an x86_64 guest on an Apple Silicon host runs under TCG
emulation, not hardware virtualization. The emulated CPU dominates; full-surface
framebuffer transfers at 60 Hz are expensive.

---

## Building

```sh
./build-enhanced_private.sh --unsigned
```

Output lands in `build/Release/VMQemuVGA.kext`. `--unsigned` skips code-signing
identity detection — the 10.6 guest does not check signatures.

## Installing

Copy the kext to the guest, then:

```sh
sudo chown -R root:wheel /System/Library/Extensions/VMQemuVGA.kext
sudo chmod -R 755       /System/Library/Extensions/VMQemuVGA.kext
sudo rm -rf /System/Library/Caches/com.apple.kext.caches
sudo touch  /System/Library/Extensions
sudo kextcache -system-caches
```

**Snapshot the VM first.** A bad kext produces an unbootable guest, and recovery
from a snapshot takes seconds versus a mount-and-repair session.

**Clear the caches and rebuild them explicitly.** A stale boot cache produces a
page fault in `OSUnserializeXMLparse` during `_StartIOKit`, before any kext code
runs — it looks exactly like a malformed `Info.plist` and is not one. Note that
the caches live under `Caches/com.apple.kext.caches/Startup/`, not at the
10.5-era `/System/Library/Extensions.mkext`.

Full procedure and recovery steps: [`.claude/rules/build-install.md`](.claude/rules/build-install.md).

---

## Known issues

- **Hardware cursor not visible on virtio-gpu-gl.** The virtio-gpu cursor queue
  (`UPDATE_CURSOR` / `MOVE_CURSOR`) is implemented and transport-proven (queue 1
  used-ring advances on both commands). However, no hardware cursor overlay
  appears on `virtio-gpu-gl-pci`. SPICE receives the cursor data
  (`set_cursor: type alpha(0), 0, 64x64` in the host log). QXL on the same UTM
  host shows a visible hardware cursor — confirming CocoaSpice can render
  overlays — but virtio-gpu-gl does not. Suspected mechanism: virtio-gpu-gl uses
  GL scanout (DMABuf → Metal texture), bypassing the cursor channel. Pending
  investigation; see [`LEDGER.md`](LEDGER.md).
- **Full-surface transfers.** Each refresh sends the whole framebuffer. The
  actual cost under TCG is the per-command doorbell round-trip, not the byte
  count — QEMU executes `TRANSFER_TO_HOST_2D` host-side at native speed, so
  bytes were never the constraint. Refresh is throttled to ~15 Hz (every 4th
  timer tick), cutting doorbell round-trips 4×. Dirty-rectangle tracking was
  investigated and rejected 2026-08-09 (see [`LEDGER.md`](LEDGER.md)): it would
  reduce bytes but leave command count unchanged, buying essentially nothing
  on this configuration.
- **Apple Remote Desktop** was reported to break at a 60 Hz refresh rate in an
  earlier build. The refresh is now ~15 Hz and this has not been re-tested.
- **`IOGLBundleName` inconsistency.** The framebuffer node publishes
  `"GLEngine"` while the `VMQemuVGAAccelerator` child publishes
  `"VMVirtIOGLEngine"`. GLPlugin is superseded (see
  [`GLPlugin/SUPERSEDED.md`](GLPlugin/SUPERSEDED.md)); neither bundle
  delivers working GL today. Separate cleanup, tracked in [`LEDGER.md`](LEDGER.md).
- **Empty vestigial bundle-name properties.** `IOMetalBundleName = ""`
  and `IOGLESBundleName = ""` are published empty. Metal does not exist
  on 10.6 per CLAUDE.md. Separate cleanup, tracked in [`LEDGER.md`](LEDGER.md).
- **~~`IOAccelerator3D = Yes` overclaim~~ — FIXED 2026-08-09 (`fb669ac`).**
  Was published in five places (`FB/VMVirtIOFramebuffer.cpp:356`,
  `FB/VMVirtIOGPU.cpp:415`, `FB/VMQemuVGA.cpp:1012`/`:1068`, plus the
  already-correct False branch). Same `crsr=1` failure class — advertise
  a capability the system can't deliver. Now derived from
  `VMVirtIOGPU::m_3d_functional` (const false until Mesa + CGL shim
  lands) via `is3DFunctional()`. Model strings (`"VirtIO GPU 3D"` and
  variants) and `"3D Acceleration" = "Enabled"` also derive from the
  flag; QXL-path sites hardcoded False with a comment. Single flag,
  single flip when Mesa lands.
- **Other retired IORegistry properties.** VRAM figures publish the
  actual allocation size; `ATY,memsize` removed; class-code override
  hack removed (it published on the framebuffer node but System Profiler
  reads the PCI nub — the hack never had any effect).
  Do not treat IORegistry as a capability report.
- **No EDID on any variant.** `readDDCBlock` fails on virtio-gpu-gl-pci,
  virtio-ramfb-gl, and virtio-vga-gl. Display preferences still offers the
  advertised resolutions and mode switching works — WindowServer builds the
  list from the driver's mode table without needing EDID. The visible
  consequence is System Profiler's Graphics/Displays panel, which shows a
  display entry on virtio-vga-gl but not on the pure-GPU variants; the
  mechanism is unverified. A synthetic EDID would populate that panel and
  give WindowServer real timing data.

---

## Development

- [`LEDGER.md`](LEDGER.md) — current state: what is fixed, what is open, what is
  unexplained. Updated every session.
- [`.claude/CLAUDE.md`](.claude/CLAUDE.md) — environment facts and the ground
  rules this project works under.
- [`.claude/rules/`](.claude/rules/) — topic-specific notes on the virtio
  protocol, framebuffer and IOKit matching, build and install, and the 3D
  architecture.

The ground rules exist because violating them was expensive. The most important
one: **a success log proves nothing without a negative control.** For most of
this driver's history `submitCommand` was a stub that reported success on every
call without ever reaching the device, and every capability claim downstream of
it was false. Verify against the device, not against your own logging.

---

## History

Earlier versions of this README described a driver whose advanced features were
stub functions returning success, with an IORegistry that reported them as
working. That was accurate at the time. The 2D transport is real and verified,
and has been for several releases. The 3D transport was verified end-to-end at
the byte level on 2026-08-09, and 3D rendering through Mesa and the CGL shim on
2026-08-10 — test applications only; no real application has been run against it
yet (see [3D status](#3d-status) above).

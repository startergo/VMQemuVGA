# VMQemuVGA

Display driver (IOKit kext) for **macOS 10.6.8 Snow Leopard** guests running
under QEMU/UTM, with support for both QXL and VirtIO GPU devices.

Fork of `ivanagui2/VMQemuVGA`, itself derived from Zenith432's original.

---

## Status

**Working: full 2D display through VirtIO GPU.** As of 2026-08-09 the driver
brings up a correct Snow Leopard desktop on `virtio-gpu-gl-pci` under UTM on an
Apple Silicon host — correct rendering, correct cursor, working resolution
changes.

**Not working: 3D acceleration.** The device offers it and the driver can see
it, but nothing renders through it yet. See [3D status](#3d-status).

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
| QXL path | Verified 2026-08-09 — VMQemuVGA class, separate code path, mode switches work. |

### Devices

- `virtio-gpu-gl-pci` — primary target, verified 2026-08-09
- `virtio-ramfb-gl` — verified 2026-08-09
- `virtio-vga-gl` — verified 2026-08-09 (required `useNativeScanout` fix; VGA
  compat no longer gates the rendering path)
- QXL / QEMU std VGA — verified 2026-08-09 (VMQemuVGA class, separate path)

---

## 3D status

The groundwork is in place and none of the rendering is:

- The host **does** offer `VIRTIO_GPU_F_VIRGL`, and the driver reads the capset
  table successfully (VIRGL and VIRGL2 both advertised).
- Nothing downstream of capset negotiation has ever executed. No 3D context has
  produced a rendered frame.
- **The guest half of the standard architecture does not exist.** virtio-gpu 3D
  assumes the guest compiles GLSL to TGSI before anything leaves the VM — via
  Mesa and Gallium. Snow Leopard has neither. Supplying that is the project, not
  a detail.

The `GLPlugin/` tree contains an in-progress `GLEngine` replacement. It reaches
context creation and renders nothing. `virglrenderer-metal/` is a shelved host-
side experiment that has never compiled inside virglrenderer and targets a
Linux/Mesa guest.

If you need 3D in a Snow Leopard VM today, this driver will not give it to you.

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

- **Software cursor only.** `getAttribute(crsr)` reports no hardware cursor, so
  WindowServer composites into the framebuffer. Every mouse move dirties the
  surface and costs a transfer. The fix is the virtio-gpu cursor queue
  (`UPDATE_CURSOR` / `MOVE_CURSOR`), not yet implemented.
- **Full-surface transfers.** Each refresh sends the whole framebuffer rather
  than damaged rectangles. Expensive under TCG.
- **Apple Remote Desktop** was reported to break at a 60 Hz refresh rate in an
  earlier build. The rate is currently 60 Hz and this has not been re-tested.
- **`VMVirtIOGPU::probe` reads no PCI properties.** vendor-id, device-id and
  class-code all come back zero due to an OSData/OSNumber cast bug. Variant
  detection in `enableController` bypasses this via `configRead32` and works
  correctly on all variants — the probe bug is latent, not affecting the
  outcome.
- **Misleading IORegistry properties.** Several advertised values overclaim
  capability — `IOAccelerator3D = Yes`, `model = "VirtIO GPU 3D"`. VRAM
  figures now publish the actual allocation size; `ATY,memsize` removed;
  class-code override hack removed (it published on the framebuffer node
  but System Profiler reads the PCI nub — the hack never had any effect).
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
working. That was accurate at the time. The transport is now real and verified;
the 3D claims are not yet, and are marked accordingly above.

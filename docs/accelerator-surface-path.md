# The 2D accelerator surface path — WindowServer's IOAccelSurface loop

This is the **second, independent acceleration stack** in this project. The
first — per-process GL via the substitute `OpenGL.framework` → Mesa → virgl →
kext → host — is described in
[`docs/architecture-3d.md`](architecture-3d.md). The two share only the kext
transport and the host device. **This stack contains no GL, no Mesa, no
virgl**: it is Apple's 2D surface contract, served in-kernel, consumed by
WindowServer.

**What this path is and is not:** it gives WindowServer a working 2D surface
path — a CPU-writable backing mapped into WindowServer's address space and a
present step that gets those pixels to the host. It does **not** give
WindowServer 3D. GL still reaches applications only through the per-process
substitute framework, and the GA CFPlugIn that would let *applications* attach
their own surfaces to the accelerator does not exist in this project.

Landed 2026-08-16 (commits `8542bb8`…`9653497`); evidence chain and per-boot
detail in [`LEDGER.md`](../LEDGER.md).

---

## The stack

```
WindowServer  (pid 98, attributed via IOUserClientCreator in ioreg)
    │  IOAccelSurface user client (VMAccelSurfaceClient) — MIG, old-style
    │  IOExternalMethod table; 18 selectors, argument metadata per row
    ▼
SetIDMode ─ SetShape ─ QueryLock ─ WriteLock ─ WriteUnlock ─ Flush
    │          │           │           │            │           │
    │          │           │           │           bit         clipped row-by-row
    │          │           │           │                      memcpy into the
    │          │           │           │                      framebuffer backing
    │          │           │           │                      (shape-rect offsets,
    │          │           │           │                       strides independent)
    │          │           │           ▼
    │          │           │   IOBufferMemoryDescriptor
    │          │           │   (KernelUserShared | Pageable | InOut)
    │          │           │     ├─ client mapping  — createMappingInTask(m_owning_task)
    │          │           │     │   handed out inside IOAccelSurfaceInformation
    │          │           │     └─ kernel mapping  — createMappingInTask(kernel_task)
    │          │           │         (flush's memcpy source; KernelUserShared memory
    │          │           │          has NO kernel VA without an explicit mapping)
    │          │           │   lazy-create at first lock · grow-only · persists
    │          │           │   across lock/unlock · released at surface destroy /
    │          │           │   clientClose / client death mid-lock
    ▼          ▼           ▼
damage-region stream: real shapes store (IdentityScaleBit only), the
empty-region pair-member (rect[0] degenerate) is a no-op — geometry
follows the LAST real shape; base extent is grow-only max bounds
    │
    ▼
framebuffer backing (fixed kernel buffer, resource 1, scanout owner)
    │  refresh timer: re-arms BEFORE the work, period = single knob
    │  (REFRESH_PERIOD_MS), achieved rate measured in-driver
    ▼
TRANSFER_TO_HOST_2D + RESOURCE_FLUSH  ──►  host scanout
```

## The selector loop, as observed

WindowServer's steady cycle, per damage event:

```
[9, 9, 11, 14, 15, 10]  =  SetShape(real), SetShape(empty no-op),
                           QueryLock, WriteLock, WriteUnlock, Flush
```

| Selector | Contract (source: worked example `VMsvga2Surface.cpp`) | Ours |
|---|---|---|
| SetIDMode (7) | `set_id_mode(wID, modebits)`; depth bits → bpp | stores id, format, bpp |
| SetShape (9) | `set_shape(options, fbIndex, region, size)`; empty region → no-op (`:1607`); geometry stores under IdentityScaleBit (`:1639`) | stores shape + grow-only extent |
| QueryLock (11) | answer is the return code (`:1461`) | reports the real lock bit |
| WriteLock (14) | StructO(0,var): fills `IOAccelSurfaceInformation`; address valid in the OWNING task; CannotLock on double-lock; NoMemory on backing failure (`:1242`) | real mapped backing, offset-addressed handout |
| WriteUnlock (15) | clear the bit, nothing unmaps (`:1276`) | same |
| Flush (10) | present step | guest-side blit; timer carries to host |

`IOAccelSurfaceInformation` handout: `address[0]` = client mapping base +
`shape_y*stride + shape_x*bpp`; `rowBytes` = **the allocation's stride**
(`base_w × bpp`, never `width × 4` — a wrong stride makes the compositor write
past the mapping); `width`/`height` = current shape bounds;
`colorTemperature[0] = 0x1CCCC` (GeForce.kext precedent).

## Why unlock does (almost) nothing here and had to do more for us

The worked example's surface backing **is device VRAM** — CPU and device see
the same bytes, so unlock needs no transfer. VirtIO GPU has no guest-CPU
mappable BAR, so this implementation pays with an explicit kernel mapping and
a flush blit, and the framebuffer's refresh timer carries pixels the rest of
the way. That divergence is the load-bearing difference for every selector
from the lock onward.

## Evidence that the desktop composites through this path

- Lock+flush landing boot: 48/48 flush blits green at real device coordinates
  (clock strip, menu bar, window rects); desktop painted, colors correct
  (visual check), survives window drags.
- The failure that preceded it: the lock-only boot came up **blue** —
  WriteLock succeeded, Flush refused, and the desktop never painted. That is
  the proof WindowServer switched its compositing destination to the surface:
  a lock without a working flush is a broken-window state by construction.
  Rule adopted: **lock and flush land as a pair.**
- Refresh measured in-driver (configured ≠ achieved — always check the window
  log): 47-52 Hz achieved at 17 ms configured, 6-10 ms work per
  transfer+flush pair (mode-dependent; see the named confound below).

## Named confounds and open items

- **FB mode varies between boots** (1680×1050 vs 1920×1080 mode=5 observed) —
  changes stride, surface extent, and per-transfer work. Cross-boot
  comparisons of blit counts, timing, or load must cite the first-tick mode
  line.
- **`bSkipWriteLockOnce`** (10.6 WindowServer Window-Grab deadlock avoidance,
  worked example `:1658-1666`) is armed but has never been triggered by
  observed traffic — no `options==0x5` shape in any recorded boot, including
  a window drag. Adopted on target precedent, untested by traffic.
- Idle steady-state load at the 17 ms period is unmeasured.
- `kextcache -system-caches` exits 0 but writes nothing to `Startup/`
  (unexplained residual; development runs cacheless).
- The host-side cursor overlay gap is upstream (CocoaSpice's GL display path
  does not composite the cursor; guest-side mitigation is the refresh rate).
  Reproduction trio: QXL cursor works / virtio-vga-gl without this kext
  cursor works / with this kext cursor fails — same device, same host, one
  variable.
- **GA CFPlugIn absent** — applications cannot attach their own 2D surfaces
  to the accelerator; only WindowServer's loop is served.

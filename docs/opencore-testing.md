# Testing VMQemuVGA via OpenCore kext injection

**Status: UNTESTED.** There is a recollection of an injection attempt where the
kext did not load, but neither the guest it was tried on (Snow Leopard or
Catalina) nor the failure mode was recorded at the time, and it was never
investigated. Treat this as "not yet tested" rather than "tried and failed" —
the cause may have been mundane (a copy in `/S/L/E` masking the test — see
"Only one copy on the system"), a configuration gap, or something real. Do not
let that single soft data point discourage a proper attempt. This document is a
test plan and a debugging checklist, not a procedure known to work. Sections
marked **(unverified)** are reconstructed from general OpenCore
knowledge, not from a working setup here — correct them as they are confirmed.

The verified install path today is a normal `/System/Library/Extensions`
install on 10.6; see [`.claude/rules/build-install.md`](../.claude/rules/build-install.md).

---

## Why bother

On Apple Silicon under UTM, macOS guests boot through OpenCore, so its
`config.plist` is already in the loop. OpenCore reads a kext from the EFI system
partition *before* the OS starts and injects it into the boot cache, so the
running system sees it as though it had always been cached. Nothing is installed
into the guest filesystem.

What that buys, by OS era:

| Problem | Applies to | Injection avoids it? |
|---|---|---|
| Stale boot cache producing `OSUnserializeXMLparse` panics | 10.6 | Yes — no guest-side cache rebuild |
| `chown root:wheel` / `chmod 755` requirements | all | Yes — the ESP is FAT32 |
| SIP protecting `/System/Library/Extensions` | 10.11+ | Yes — nothing installed to the system volume |
| Staging into `/Library/StagedExtensions` | 10.13+ | Yes |
| Security & Privacy consent click (cannot be scripted) | 10.13+ | Yes |
| Read-only system volume | 10.15+ | Yes |
| Notarization | 10.15+ distribution | Yes for testing |

The payoff is largest on Catalina, where the alternatives are genuinely
painful. On Snow Leopard the existing `/S/L/E` path already works, so injection
there is a convenience and a rehearsal — not a requirement.

---

## Only one copy on the system

**Prerequisite, and a likely explanation for the failed attempt.**

The kernel will not load two kexts with the same `CFBundleIdentifier`. One wins,
the other is silently rejected. With `VMQemuVGA.kext` present in both
`/S/L/E` and `EFI/OC/Kexts/`, the installed copy may load while the injected one
is discarded — indistinguishable from "injection did nothing" unless you check
which build is live.

Before testing injection, remove the installed copy and clear the caches so the
removal takes effect:

```sh
sudo rm -rf /System/Library/Extensions/VMQemuVGA.kext
sudo rm -rf /System/Library/Caches/com.apple.kext.caches
sudo touch  /System/Library/Extensions
sudo kextcache -system-caches
```

On 10.11+ the same applies to `/Library/Extensions`, and on 10.13+ to its staged
copy under `/Library/StagedExtensions`.

**Give yourself a discriminator.** Even with one copy, it is worth being able to
tell at a glance which build is running:

- Bump `CFBundleVersion` for injected builds — `kextstat` prints the version.
- Add a distinctive `IOLog` line at `start()` (e.g. the build tag or commit
  hash) so the boot log names the build unambiguously.

Both are cheap and remove a whole class of "did my change take?" confusion.

---

## Accessing the ESP

OpenCore sits on its own drive in these VMs (`efi-legacy`), separate from the
guest's system disk, and that volume **automounts**. `config.plist` and
`EFI/OC/Kexts/` are directly editable with no `hdiutil` attach or manual mount
step.

That also makes the edit/test loop short: drop the rebuilt bundle into
`EFI/OC/Kexts/`, reboot the VM. No `scp` into a running guest, no ownership
fixes, no cache rebuild.

> **TODO:** record the mount point as it appears, and whether it mounts on the
> host or only inside the guest.

---

## Configuration

### `Kernel > Add` entry

```
Arch            x86_64
BundlePath      VMQemuVGA.kext
ExecutablePath  Contents/MacOS/VMQemuVGA
PlistPath       Contents/Info.plist
MinKernel       (empty)
MaxKernel       (empty)
Enabled         true
Comment         VMQemuVGA display driver
```

Place the built bundle at `EFI/OC/Kexts/VMQemuVGA.kext`.

`ExecutablePath` and `PlistPath` must match the bundle exactly. OpenCore skips
entries it cannot resolve **without failing the boot** — a silent skip looks
identical to "the kext did not load."

Injection order is config order. If a dependent kext is ever added, it must come
after its dependency.

`MinKernel` / `MaxKernel` can stay empty while one build serves all targets.
Catalina needs no separate SDK, so one bundle is expected to cover both. If
separate builds per OS era ever become necessary, these fields gate them by
Darwin version and both can be listed in one config.

### `Kernel > Scheme` **(unverified — prime suspect for the SL failure)**

- **Snow Leopard (Darwin 10):** predates the prelinked-kernel scheme. Expect to
  need `KernelCache = Mkext`, with `Cacheless` as the fallback. The default
  `Auto` most likely resolves to prelinked, which would inject into a cache the
  10.6 kernel never reads — matching the observed "boots normally, kext absent."
- **Catalina (Darwin 19):** prelinked; `Auto` should be correct.
- `KernelArch`: set explicitly to `x86_64` rather than `Auto`. The SL guest boots
  with `arch=x86_64` in boot-args, and a mismatch between OC's arch setting and
  the kext slice causes a silent skip.

### `NVRAM`

Boot arguments are set here rather than inside the guest. A guest-side
`nvram boot-args=...` write is silently reverted at the next boot — OpenCore
re-supplies its own string from this file (observed 2026-08-22: a verified
guest write came up absent, kernel gate log read 0). Current SL guest:

```
-v keepsyms=1 debug=0x12a vsmcgen=1 msgbuf=1048576 serial=5
```

Read the current value before writing: setting boot-args replaces the whole
string, so writing a remembered subset silently drops the rest. `serial=5`
(kernel logging to the emulated serial port) and `vsmcgen=1` (VirtualSMC) are
both easy to lose that way.

For 10.11+ targets, SIP is configured via `csr-active-config` in the same
section rather than `csrutil` from Recovery. **(unverified)** Choose the value
deliberately and record it here once used — do not copy a magic number from a
Hackintosh guide without knowing which bits it clears.

Note: `kext-dev-mode=1` was Yosemite-only and was removed in 10.11. It is not a
usable bypass on any target newer than 10.10.

---

## Verifying injection

In the guest after boot:

```sh
kextstat | grep -i vmqemuvga
```

Three outcomes, and they mean different things:

- **Absent** — injection genuinely failed. Work the debugging list below.
- **Present, expected version / build tag** — injection worked.
- **Present, but the old version or no build tag** — a second copy won. Check
  `/S/L/E` (and `/Library/Extensions`, `/Library/StagedExtensions` on newer
  systems) and remove it before concluding anything about injection.

**Get OpenCore's own log before guessing.** In `Misc > Debug`:

```
Target          67      (console + file — verify this value)
AppleDebug      true
DisableWatchDog true
```

OpenCore writes `opencore-<date>.txt` to the ESP, stating whether each kext was
injected and under which cache scheme. That log distinguishes "OC skipped the
entry" from "OC injected it and the kernel rejected it" — two different bugs
that look the same from inside the guest.

---

## Debugging "the kext did not load"

Work in this order; each step is cheap and narrows the space.

1. **Confirm there is only one copy on the system** (see above). This is the
   cheapest check and it invalidates every other result if skipped.
2. **Read the OpenCore log.** Did OC report injecting `VMQemuVGA.kext`? If it
   never mentions the entry, the problem is configuration, not the kext.
3. **Check `Kernel > Scheme > KernelCache`.** On 10.6, try `Mkext`, then
   `Cacheless`. This is the most likely cause.
4. **Check `KernelArch` and the entry's `Arch`.** Set both to `x86_64`
   explicitly.
5. **Check the paths.** `ExecutablePath` and `PlistPath` must match the bundle
   layout exactly.
6. **Try `OSBundleRequired = Root`** in the kext's `Info.plist`. Kexts without
   it are excluded from boot caches by the normal machinery. Not needed for
   `/S/L/E` loading, so its absence would only show up here. Harmless either
   way and standard for injected kexts.

If OC reports injecting and the kernel still does not load it, stop. OpenCore's
Darwin 10 support is far less exercised than its modern-macOS path, and the
`/S/L/E` install works on Snow Leopard. Injection's real value is on Catalina.

---

## Recovery

A bad injected kext panics at boot, but recovery is easier than a bad install:
edit `config.plist` on the automounted `efi-legacy` volume and set
`Enabled = false` on the entry, or remove the bundle from `EFI/OC/Kexts/`.
Because OpenCore is on its own drive, detaching that drive in UTM is also an
option. No `slclean` guest, no mounting the target's HFS+ volume, no installer
ISO.

Those two escape hatches — disabling the entry on the automounted volume, or
detaching the OpenCore drive — are the recovery path for injection. Neither
requires booting anything else, so injection failures are cheaper to undo than
bad `/S/L/E` installs, which need `slclean`.

---

## Open questions

- ESP mount point (see TODO above).
- Whether the failed SL attempt was masked by the `/S/L/E` copy.
- Which `KernelCache` scheme actually works on Darwin 10.
- Whether `OSBundleRequired` is required for injection.
- The correct `csr-active-config` value for kext development on 10.11+ targets.
- Whether OpenCore's Darwin 10 injection path works at all with this bundle.

Fill these in from real runs. A confidently-worded procedure that turns out to
be half wrong is worse than an empty section — the `Extensions.mkext` path in
`build-install.md` was a silent no-op on this system for exactly that reason.

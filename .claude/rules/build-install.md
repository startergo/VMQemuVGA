
# Build, install, and recovery

Applies to every session. A bad install here produces an unbootable guest, but
recovery is straightforward — see "Recovery when the guest will not boot" below.

## Building

From the repo root:

```sh
./build-enhanced_private.sh --unsigned
```

**`--unsigned` is the only flag you need, and it is required.** Snow Leopard's
kernel (Darwin 10) does not understand `LC_CODE_SIGNATURE` load commands, so a
signed kext fails to load in the guest. Everything else that makes a 10.6 kext
— the `-target x86_64-apple-macos10.6` triple, MacKernelSDK headers,
`-static -nostdlib` — comes from `VMQemuVGA.xcconfig`, the project's base
configuration, and applies on every build regardless of flags. Do not reach for
`--snow-leopard`: it only forces unsigned as well, and its `XCCONFIG_FILE=`
branch never worked.

A full build takes under a minute, so prefer rebuilding over reasoning about
whether a change took effect. Output lands in `build/Release/VMQemuVGA.kext`.

**The Mesa side builds separately and this file does not cover it.** `Mesa-VirGL`
(branch `cross-10.6`) is cross-compiled from Apple Silicon to
`x86_64-apple-macos10.6` with its own prerequisites, meson cross file and patch
set — including the `u_thread.h` TLS gate, which is a single-threaded-only
correctness compromise rather than a portability shim. Full procedure:
[`../../../Mesa-VirGL/cross-compat/build_10.6.md`](../../../Mesa-VirGL/cross-compat/build_10.6.md)
(sibling repo, relative to this file). The CGL shim under
`../../../Mesa-VirGL/cgl-shim/` builds from there too, not from here.

## Environment and access

**Machines**

| Name | Role |
|---|---|
| host | Apple Silicon macOS. This repo. Builds happen here. |
| `slqemu.local` | The target guest — Snow Leopard 10.6.8 x86_64, user `sl`. Kext is installed and tested here. |
| `slclean.local` | Recovery guest. Mounts slqemu's system volume at `/Volumes/MacintoshHD`. Used only when slqemu will not boot. |
| Catalina guest | Separate VM, previously exercised on the QXL path. Not part of the normal loop. |

OpenCore lives on its own drive (`efi-legacy`) which automounts — see
[`docs/opencore-testing.md`](../../docs/opencore-testing.md).

**Copying a build to the guest**

```sh
scp -r build/Release/VMQemuVGA.kext sl@slqemu.local:/tmp/
ssh sl@slqemu.local
```

Then, on the guest:

```sh
sudo rm -rf /System/Library/Extensions/VMQemuVGA.kext
sudo cp -R /tmp/VMQemuVGA.kext /System/Library/Extensions/
```

— followed by the md5 check and cache sequence below.

**Getting logs back**

```sh
ssh sl@slqemu.local 'cat /var/log/kernel.log' > /tmp/kernel.log
```

Use the file, not `dmesg`. The kernel message buffer is small and this driver is
chatty at boot, so the ring wraps before you can read it. The file is the
reliable source.

**Boot arguments — read before you write.** The SL guest currently runs:

```
-v keepsyms=1 debug=0x12a vsmcgen=1 msgbuf=1048576 serial=5
```

These live in OpenCore's `config.plist` under `NVRAM`, not in the guest. Two of
them are load-bearing for this project and easy to lose: `serial=5` is what
sends kernel logging to the emulated serial port, and `vsmcgen=1` configures
VirtualSMC. `msgbuf=1048576` enlarges the message ring.

**Never set boot-args from a value recorded in a document — including this one.**
An `nvram boot-args="…"` (or a config.plist edit) replaces the whole string, so
writing a remembered subset silently drops whatever else was there. Read the
current value first, add or change the one argument you need, and write back the
full string. This list is a reference for what *should* be present, not a
source to copy from.

**Host-side QEMU log.** UTM writes QEMU's stderr and SPICE messages to a per-VM
debug log, but only when *Debug Log* is enabled in that VM's settings. This is
the only place virglrenderer decode errors and `qemu_log_mask(LOG_GUEST_ERROR)`
messages appear — several failures are invisible from inside the guest by
construction, so this log is a required artifact for 3D work, not a fallback.

> Confirm before relying on: the ssh user (`sl`) and hostnames are taken from
> shell prompts in captured logs; the exact UTM debug-log path has not been
> recorded here yet.

## Where the boot caches actually live

On 10.6 **both** of these are under one directory, and boot.efi prefers the
kernelcache:

```
/System/Library/Caches/com.apple.kext.caches/Startup/Extensions.mkext
/System/Library/Caches/com.apple.kext.caches/Startup/kernelcache_x86_64.<hash>
```

There is **no** `/System/Library/Extensions.mkext` on this system — that is the
10.5-era path. Any script deleting it is a silent no-op, which is exactly how a
stale cache survived many builds and produced the same panic every time.

`Caches/com.apple.kext.caches/Directories/…` holds the IOKitPersonalities and
KextIdentifiers caches. Harmless, but they go with the rest when you clear.

## Every install must clear the caches and rebuild them explicitly

A stale boot cache produces a page fault in `OSUnserializeXMLparse` inside
`OSKext::initWithBooterData` during `_StartIOKit` — before any kext code runs.
It looks exactly like a malformed `Info.plist` and is not one. That
misdiagnosis cost a full session.   

On the guest, after installing the kext:

```sh
md5 /System/Library/Extensions/VMQemuVGA.kext/Contents/MacOS/VMQemuVGA
sudo chown -R root:wheel /System/Library/Extensions/VMQemuVGA.kext
sudo chmod -R 755       /System/Library/Extensions/VMQemuVGA.kext
sudo rm -rf /System/Library/Caches/com.apple.kext.caches
sudo touch  /System/Library/Extensions
sudo kextcache -system-caches
sudo reboot
```

**Verify the copy with md5 before doing anything else.** Compare the digest of
the installed binary against the one in `build/Release/`. A truncated or stale
`scp` produces a kext that loads and behaves like the previous build — which
reads as "my fix didn't take" and has no other symptom. Bumping
`CFBundleVersion` or logging a build tag at `start()` gives the same assurance
from the boot log.

**Rebuild the caches explicitly, then reboot as a separate step.** Do not chain
`kextcache` into `shutdown -r now` — `kextcache` returning is not the same as
the cache being flushed to disk, and a reboot racing it leaves a partial or
absent cache. Wait for `kextcache` to finish, confirm the `Startup/` files, then
reboot.

**The cache build is asynchronous and takes several minutes on this guest.**
The first `kextcache -system-caches` returns with `Startup/` empty and a later
check finds the finished `Extensions.mkext` (~9.8 MB) — that empty-`Startup/`
state is mid-build, not the old "writes nothing" unexplained residual. Procedure:
run `kextcache`, wait several minutes, THEN verify `Startup/` mtime and size,
then reboot. Rebooting mid-build is a partial-cache panic on the next boot.

**Never deploy into a booting VM.** An install that lands after the loader has
already passed `/S/L/E` silently never loads — the desktop comes up on fallback
with the kext sitting on disk, and kernel-log counts spanning multiple boots
will lie about it. The load-state arbiter is `ioreg -l | grep <class>` on the
running registry, and deploys happen only against a fully-up guest or from
slclean with the target unmounted-from-boot.

**Run `kextcache -system-caches` explicitly — do not wait for `kextd`.** On this
guest the cache directory stayed empty after deletion until `kextcache` was run
by hand. A procedure that assumes automatic regeneration silently leaves the
system cacheless.

Then confirm the `Startup/` files exist with a **fresh mtime and a plausible
size** (mkext on the order of 9–10 MB, kernelcache 6–7 MB). If they do not
change when the kext changes, that is the bug — fix the install script rather
than working around it.

Note the two-boot delay when caches *are* rebuilt in the background: `kextd`
runs after boot, in userspace. So the boot immediately following an install can
still load the previous cache, and a newly built cache is only exercised on the
boot after that. "It booted fine" right after an install is weak evidence.

Running with the caches deleted is a perfectly good development configuration —
the kernel loads kexts individually from `/S/L/E`. Boot is slower and the entire
stale-cache failure class disappears. If you take this route, assert the cache
directory is *absent* after install rather than asserting regeneration.

Diagnostic signature worth remembering: an identical `CR2` fault address across
two structurally different builds means the parser is chewing the same bytes
both times, i.e. the cache was never regenerated. Content is not the variable
when the fault address does not move.

## Recovery when the guest will not boot

Boot `slclean.local`, which mounts slqemu's `MacintoshHD`, and clear from there:

```sh
V=/Volumes/MacintoshHD
sudo rm -rf "$V/System/Library/Extensions/VMQemuVGA.kext"
sudo rm -rf "$V/System/Library/Caches/com.apple.kext.caches"
sudo touch  "$V/System/Library/Extensions"
```

**Do not run `kextcache` from `slclean` against that volume** — it would build
against the wrong kernel. Clear from slclean, boot the target, and run
`kextcache -system-caches` there.

If you want the failing cache for forensics, copy `Startup/` off before deleting
it — that artifact cannot be reconstructed later.

When recovering, prefer to change one thing. Clearing the caches while leaving
the kext installed is a real experiment: if it boots, the bundle is exonerated
and the cache path is the defect.

## Validate without booting where possible

```sh
kextutil -n -t -v 6 /System/Library/Extensions/VMQemuVGA.kext
kextcache -m /tmp/test.mkext /System/Library/Extensions/VMQemuVGA.kext
```

`kextutil -n -t` performs full validation and dependency resolution without
loading, and will name a bad property if there is one. `plutil -lint` passing
proves very little — the kernel parser is stricter and fails differently.

**But it does NOT predict the boot-time kxld verdict for symbol linkage.**
Falsified 2026-08-15: a build referencing `get_bsdtask_info` passed
`kextutil -n -t` on the guest and then failed to link at boot, leaving the guest
on fallback display. Symbols outside the KPI symbol sets named in
`OSBundleLibraries` can survive this check and still be refused by kxld. For
symbol-availability questions the only arbiter is the boot itself — so treat any
new kernel-API call as a boot-risk change, revertable in one step, and never
bundle it with work whose attribution matters.

Related 10.6 KPI limit, established the same day: **there is no supported way for
a kext to name a process on 10.6.** Four attempts, four boot-time link failures:
`get_bsdtask_info`, `pid_for_task`, `proc_selfpid`, `proc_selfname` — all outside
the 10.6 kext KPI, all passing the build and `kextutil`, all refused by kxld with
`0xdc008016`. Do not try a fifth. Process attribution must come from userspace:
check whether `ioreg -l` already shows an `IOUserClientCreator` property on the
client (no code, no boot), or correlate the kext's existing task-pointer log
against `ps` by timestamp, or have the caller log its own pid where you control
it.

`kextcache` refuses bundles that are not `root:wheel` with 0755 directories,
which `scp`-based installs routinely get wrong. Check ownership before assuming
a cache-generation failure is something more exotic.

## Post-build assertion

The build should fail loudly if the `VMVirtIOFramebufferPCI` personality is
missing from the built `Info.plist`, and should assert on `IOClass` and
`IOProviderClass` values rather than merely grepping for a string. A personality
that is absent or malformed is invisible until boot.

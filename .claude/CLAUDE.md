# VMQemuVGA — Project Instructions

**Before doing anything: read `LEDGER.md` at the repo root.** It holds current
state — what is fixed, what is open, what the active task is. This file holds
durable rules only. **If this file and the ledger disagree about the state of the
project, the ledger wins.**

**At the end of every session, update `LEDGER.md`** with what changed, what was
learned, and what remains unexplained. Do not record conclusions the evidence
does not support.

**Before `/compact` is issued, write the session state to `LEDGER.md` first.**
Compaction discards the conversation; what survives is a summary plus the files
on disk. Anything not written down is gone, and — worse — a conclusion that
survives without its evidence gets re-derived wrongly later. The summary must
contain:

- **Raw values, not interpretations.** Record `ctrl 0x103, error 0x1203`, not
  "cleanup failed" — opcodes, response codes, `file:line`, commit SHAs, exact
  log strings. Interpretations are re-derivable from raw values; the reverse is
  not true, and a wrong interpretation recorded as fact will be inherited.
- **What was verified and how.** Name the negative control or the visual check.
  "Transport works" is not a finding; "probe PASS, 64/64 dwords exact, G ≠ G′"
  is.
- **What was falsified**, with the evidence that killed it. Dead hypotheses
  that aren't recorded get re-proposed within days.
- **Corrections made during the session**, stated as corrections. If a belief
  held earlier in the session turned out wrong, say so explicitly — otherwise
  the superseded version is what a later reader reconstructs.
- **The next concrete step and its pre-registered prediction.**

**How the ledger is written — three hard rules.** These apply to `LEDGER.md` and
to every file in the repo, not just to session summaries:

- **No third-person references to people.** Not "the operator", not "the user",
  not "he/they asked". The ledger records the state of the project, not who said
  what about it.
- **No sentences describing communication acts.** No asking, directing,
  confirming, adopting, agreeing, or verdicts on what was said. "The double-tab
  class died with the sub-box fix" belongs; "the suggestion to check the wire was
  accepted" does not. A finding stands on its evidence, never on who proposed it.
- **No absolute paths.** Relative to the repo (`./FB/VMVirtIOGPU.cpp`,
  `../Mesa-VirGL/cgl-shim/`) or not at all. Machine-specific paths are wrong the
  moment anything moves.

The test for all three: a reader who was never present should find nothing that
refers to the session, only to the project.

Never rely on conversation memory for a fact that isn't in a file. If you catch
yourself recalling something rather than reading it, that is the signal to check
the file or the log.

---

## What this project is

`VMQemuVGA` is an IOKit kext (fork of `ivanagui2/VMQemuVGA`, published at
`startergo/VMQemuVGA`) providing display and, eventually, 3D acceleration for a
**macOS 10.6.8 Snow Leopard x86_64 guest running under UTM** on an Apple Silicon
host.

Environment facts that change how you should reason:

- The guest is **x86_64 under TCG emulation**, not hardware virtualization.
  Every MMIO access and doorbell is expensive. Batch aggressively. Performance
  numbers from HVF/KVM setups do not transfer.
- **Run the VM with 1 vCPU during development.** SMP produces TLB-shootdown IPI
  panics that are artifacts of emulated APIC timing, not driver bugs. Re-enable
  SMP later as its own test axis.
- The host is **stock UTM** — its bundled virglrenderer and ANGLE are not
  patched. Host-side changes are effectively off the table.
- **`VIRTIO_GPU_F_VIRGL` is offered by the host.** Confirmed via feature
  negotiation (device `word0=0x30000013`, `word1=0x00000101`). The 3D direction
  is viable.

---

## Where things live

**Two separate repositories.** Kext work is committed in `VMQemuVGA`; the winsys,
the CGL shim and all Mesa work are committed in `Mesa-VirGL`. A single change
often spans both — they are not submodules of each other.

| Path | What |
|---|---|
| `~/VMQemuVGA/` | The kext. `LEDGER.md`, `.claude/`, `docs/`, `FB/` sources, `probe/` userspace test programs |
| `~/Mesa-VirGL/` | Mesa cross-built for 10.6. **Branch `cross-10.6`** |
| `…/Mesa-VirGL/src/gallium/winsys/virgl/iokit/` | `virgl_iokit_winsys` — the winsys that talks to the kext's user client |
| `…/Mesa-VirGL/cgl-shim/` | The CGL shim: `cgl_shim.mm`, `cgl_interpose.c`, `cgl_shim.h` → `cgl_shim.dylib` |
| `…/Mesa-VirGL/cgl-shim/killtest/` | Test apps — `osmesa_softpipe_test.c`, `virgl_clear_test.c`, the NSOpenGLContext killtest |
| `…/Mesa-VirGL/cross-compat/` | Cross file `mesa-cross-10.6.txt` plus the compat shims and objects it links |
| `~/powerfox-browser/` | Gecko 52 source — read-only reference (`platform/widget/cocoa/GfxInfo.mm`, `platform/gfx/gl/`) |
| `~/leopard-webkit-build/` | Source of libcxx 5.0.1 and the working 10.6 cross toolchain |
| `~/VMsvga2-modern/` | Zenith432's VMsvga2, modernised — the only worked example of the Apple accelerator contract on a macOS guest. **What to take from it and what not to is in [`docs/vmsvga2-adoption.md`](../docs/vmsvga2-adoption.md)** |

---

## Ground rules

These are not style preferences. Each one exists because violating it cost
multiple sessions.

**A success log proves nothing without a negative control.** `submitCommand` was
a ~500-line fake for months. It reported success on every call because it read
back the *command* type instead of the device's response. Every "2D working / 3D
working" claim in the old README was downstream of that. Before trusting any
success path, prove the failure path fires — e.g. `SET_SCANOUT` on a nonexistent
resource must return `0x1203`.

**The host validates almost nothing.** QEMU does not check backing length
against resource size. `0x1100` on `ATTACH_BACKING` tells you the command
parsed, not that the backing is correct. Never infer correctness from `OK`.

**Instance-tag every log line.** Multiple resources, multiple framebuffers,
multiple objects of the same class. Untagged logs produced two opposite wrong
conclusions in consecutive sessions. Print `this`, and a phase tag where
relevant.

**One variable per boot, with a written prediction beforehand.** State what each
outcome would mean *before* running. An all-green result from a two-variable
change is ambiguous and worthless.

**A recorded cleanup item is not done until it is verified done at the start of
the run that depends on it.** Writing "remove pref X before the next browser
test" in the ledger does not remove it. The 2026-08-20 session lost a full day
this way: `layers.offmainthreadcomposition.enabled=false` and
`layout.frame_rate=5` were flagged, recorded as pending, never removed — and
every census, probe, and timing run of the day silently inherited them,
including a starvation diagnosis that described the pref rather than a fault.
Before any run whose interpretation depends on a clean configuration, re-read
the configuration from the system (`grep` the profile, `cat` the plist, read
the boot-args) — do not recall it.

**Never accept a convenient explanation for an unexplained state change.** "It
must use a different code path" is not a diagnosis. When a value changes between
runs and you don't know why, find out. The double-`ATTACH_BACKING` bug hid
behind exactly that phrase for a session.

**Distinguish "command returned OK" from "pixels on screen."** A real device
response and a visible, correct desktop are separate results. Report them
separately. Never describe a log line as a visual confirmation.

**Code should self-check, not require log interpretation.** Prefer
`if (total != expected) IOLog("MISMATCH ...")` over printing numbers a human has
to compare. Format specifiers on this target are unreliable — `%zu` misbehaves;
use explicit `%llu`/`%u` casts.

**Use paths relative to the project, never machine-specific absolute ones.**
Write `./build-enhanced_private.sh`, `../Mesa-VirGL/cross-compat/build_10.6.md`,
`../../docs/architecture-3d.md` — not `/Users/macbookpro/…`. Relative paths keep
documents, links and scripts correct wherever the tree is checked out; an
absolute home path bakes one machine's username into documentation, ledgers,
scripts and commit messages and is wrong for anyone else, including the same user
on a different machine. State the anchor when it isn't obvious — "from the repo
root", "relative to this file".

Two cases where relative does not reach and `~` is the fallback: a sibling repo
whose location is a convention rather than a guarantee, and things outside any
repo (SDKs, toolchains, guest mount points). For those, and for any path a script
needs absolutely, derive it at runtime
(`REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"`) or hoist it into a variable at
the top of the script with a comment saying what it is, so one edit relocates it.

**Never state an inference as a fact.** This is the rule most often broken here,
and every violation cost real time. An inference is anything not directly
observed: a conclusion from source structure, from a project page, from a header,
from a name, from what a similar system does, or from what a log seems to imply.
Mark it as one, and say what would settle it.

Worked examples from this project, all of which were asserted before being
checked and all of which were wrong:

- "Upstream is SPL 2.0" — read off a SourceForge listing. The file headers are
  MIT-style, and no SPL text appears in any file sampled.
- "`VMsvga2GLContext.cpp` translates GMA950 command streams to SVGA3D" — inferred
  from the file's role. Every selector is log-and-return; no decoder was ever
  written.
- "`0x103` is `RESOURCE_UNREF`" — assumed from context, twice. It is
  `SET_SCANOUT`; the error was a deliberate negative control firing correctly.
- "Displays preferences offers no resolution list without EDID" — generalised
  from one variant, written into the README, and false on the others.
- "The MIG catch-22 is confirmed as the d98-era cause" — the probe showed the
  working configuration works; the binary that failed cannot be reconstructed
  from this repository at any commit.

Three specific habits that prevent it:

- **Read the artifact before describing it.** A plist, a header, a load command,
  a symbol table, a boot log. Where the artifact is available, reading it is
  almost always cheaper than the reasoning that would replace it.
- **Absence is not evidence until you know where the setting lives.** Not finding
  something in two files says nothing if a third file exists to hold it. This has
  already produced two wrong conclusions about the build. The rule that prevents
  it: build with the script and do not reason about build settings at all.
- **Attribute the evidence in the same sentence as the claim.** "Confirmed by
  `nm -m` on the built binary" and "inferred from the call sites" are different
  statements and must look different.
- **When a source and an observation disagree, the observation wins** — including
  when the source is this file. Documentation here has been stale or wrong more
  than once.

**When you are wrong, say so plainly and revise the ledger.** Several models
here felt conclusive and were not. The tell each time was a clean sweep that
left observations unexplained. Maintain an explicit list of unexplained
residuals and do not quietly drop entries from it.

**Expect new failures, not regressions.** Most bugs found here were always
broken but never reached, because the code above them failed first. Each fix
moves the frontier and exposes the next defect. A new panic after a successful
fix is usually progress — say so, and check whether the newly-reached code has
ever executed before calling it a regression.

---

## Codebase-wide traps

**Build only with `./build-enhanced_private.sh --unsigned`. Do not reason about
build settings.**

The build works and has worked for years. If a compile line looks wrong — a
modern `-isysroot`, an empty `SDKROOT`, a config file that appears unused — that
is a wrong conclusion, not a bug. The repo root holds seven xcconfig-shaped
files and only one is wired to the project; reading a dead one has already cost
two detours. Do not open them, do not edit them, do not add settings to the
pbxproj.

If a build fails, the fault is in the source you just changed.

**The one exception:** a new `.cpp` or `.h` must be added to
`VMQemuVGA.xcodeproj/project.pbxproj` — that file list is what the script builds.
Adding a source file is the only legitimate reason to touch the project file.
Nothing else in it, and nothing in any xcconfig, ever needs changing.

**Kernel headers come from `MacKernelSDK/Headers`, not from any SDK.** The kext
is built with a modern sysroot plus `-target x86_64-apple-macos10.6` and
`-static -nostdlib`, so the SDK contributes almost nothing. If a kernel API is
undeclared, MacKernelSDK is where to look. And a symbol existing in the guest's
`/mach_kernel` does NOT mean a kext may call it: kexts link against the KPI
symbol sets named in `OSBundleLibraries`, which are a small curated subset.
Declaring a missing prototype closes the header gap, not the link gap — confirm
with `kextutil -n -t -v 6` before believing it will load.

**Never put a C struct in an `OSArray` / `OSSet` / `OSDictionary`.** Every
collection operation calls `retain()`/`release()` through the pointer, so a
cast like `m_resources->setObject((OSObject*)resource)` page-faults through a
garbage vtable. This pattern appeared 23 times across 5 files and caused both
the load panic and the long-standing `kextunload` panic. Use typed pools.

**Metal does not exist on 10.6, and is not reachable on 10.14+ either.** Any
comment, rationale, or design note citing Metal was written for a different
target. Note the distinction that matters for property cleanup: 10.14+ guests
have the Metal *API*, but a Metal *device* needs an `IOAccelerator`-family driver
with a private ABI that nothing here implements — so `VMMetalPlugin` publishing
`IOAccelTypes = 2` is an unbacked claim on every target, not just 10.6. Settle it
by observation rather than by version: `MTLCreateSystemDefaultDevice()` on the
Catalina guest returns a device or it does not.

**Version-check anything attributed to "the historic implementation."** Stale
commented-out blocks in this repo have twice been mistaken for working code.
Confirm against a build that demonstrably ran.

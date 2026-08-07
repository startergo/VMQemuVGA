# text_trace

Per-app `DYLD_INSERT_LIBRARIES` interposer that hooks ChildView's NSTextInputClient methods plus NSTextInputContext's activate/deactivate at runtime to localize where typed text gets dropped. Used to test theories for the PowerFox text-input bug after the focus-state and compositor paths were cleared.

## What it captures

Four hooks on Gecko's `ChildView` class (defined in XUL, loaded by libmozglue after `main()`):

| Method | Purpose |
|---|---|
| `insertText:replacementRange:` | The text-insertion call. Logs the string and replacement range. |
| `doCommandBySelector:` | The command-dispatch call. Logs the selector name. |
| `hasMarkedText` | Composition-state query. Logs return value. |
| `markedRange` | Composition-range query. Logs returned range. |

Two hooks on `NSTextInputContext` (AppKit — available at constructor time, no deferral needed):

| Method | Purpose |
|---|---|
| `activate` | Gecko's `NotifyIMEOfFocusChangeInGecko()` calls `-[inputContext deactivate]; -[inputContext activate];` (TextInputHandler.mm:2641-2642, gated by `#if MAC_OS_X_VERSION_10_6`) inside the `mIsInFocusProcessing` window. Logging timestamps lets you test whether any `insertText:` lands reentrantly between the pair. |
| `deactivate` | Paired with `activate`. |

`interpretKeyEvents:` calls exactly one of `insertText:` or `doCommandBySelector:` per key. Logging both turns "insertText didn't fire" from a negative result into a positive one (you'd see keys routed to a command selector instead). The marked-text queries catch text landing in an uncommitted composition. The NSTextInputContext pair catches the reentrant-flush scenario where Gecko's focus-change handler pokes the input context while `mIsInFocusProcessing` is still true.

## Build

```bash
clang -arch x86_64 -mmacosx-version-min=10.6 \
      -dynamiclib -o text_interpose.dylib text_interpose.m \
      -framework Cocoa -Wno-deprecated-declarations
```

No `-undefined dynamic_lookup` — ChildView is resolved at runtime via `objc_getClass`, not linked at compile time.

## Deploy

```bash
scp text_interpose.dylib sl@slqemu.local:/tmp/
DYLD_INSERT_LIBRARIES=/tmp/text_interpose.dylib \
  /Applications/PowerFox.app/Contents/MacOS/powerfox 2>&1 | grep text
```

**GUI-launched apps on 10.6 have stderr connected to `/dev/null`**, so the dylib also writes every log line to `/tmp/text_trace.log` (lazily opened on first call, line-buffered with `fflush`). The file is the reliable capture path when launching via `open` or Finder.

To inject into a GUI-launched PowerFox without modifying the .app:

```bash
ssh sl@slqemu.local 'launchctl setenv DYLD_INSERT_LIBRARIES /tmp/text_interpose.dylib'
open -n -a PowerFox                                    # run on the VM itself
# ... reproduce, then read /tmp/text_trace.log ...
ssh sl@slqemu.local 'launchctl unsetenv DYLD_INSERT_LIBRARIES'   # cleanup
```

Direct SSH launches (`ssh sl@slqemu.local 'DYLD_INSERT_LIBRARIES=... powerfox'`) abort inside `_LSApplicationCheckIn` because the SSH session has no WindowServer bootstrap port — `open` or `launchctl asuser` is required.

## Mechanism

Runtime IMP replacement via `method_setImplementation` from `<objc/runtime.h>`. For each target selector, save the original IMP, install a C function replacement, forward through the saved IMP. No compile-time reference to `ChildView` — the class is looked up at runtime via `objc_getClass("ChildView")`.

### Deferral gotcha (the real lesson)

XUL is loaded by `libmozglue.dylib` via `dlopen` after `main()` starts — it is not a static link-time dependency of the PowerFox executable. At dylib-constructor time, XUL is not loaded and `ChildView` is not registered. A direct `objc_getClass("ChildView")` from the constructor returns `nil`.

The code uses a layered deferral:

1. Try once from the constructor (defensive — works only if XUL was somehow already loaded).
2. Register `_dyld_register_func_for_add_image` callback. When XUL is dlopen'd, the callback fires.
3. From the callback, schedule the actual install via `dispatch_after` on the main queue with a 0.5 s delay. By the time the main run loop drains this block, XUL's `+load` phase has completed and `ChildView` is registered.

Step 3 is necessary because the image-add callback fires *after the image is mapped* but *before its initializers run*. Obj-C class registration happens during the image's initialization, so the class isn't available at callback time either. The `dispatch_after` deferral to the main queue is the reliable fix.

This pattern (image-add callback + main-queue dispatch_after) is the reusable bit for hooking any class that lives in a runtime-loaded library.

## What this instrument established

Used against PowerFox 52.9 ESR on Snow Leopard 10.6.8, with synthetic keystrokes `Cmd+L` + `"abcdef"` delivered via `osascript`:

```
[text] 1786060352.446859 hasMarkedText self=0x13101bc00 -> 0
[text] 1786060352.447559 insertText:replacementRange: self=0x13101bc00 str="a" range={9223372036854775807,0}
[text] 1786060352.674011 insertText:replacementRange: self=0x13101bc00 str="b" range={9223372036854775807,0}
[text] 1786060352.711559 insertText:replacementRange: self=0x13101bc00 str="c" range={9223372036854775807,0}
[text] 1786060352.810884 insertText:replacementRange: self=0x13101bc00 str="d" range={9223372036854775807,0}
[text] 1786060352.865218 insertText:replacementRange: self=0x13101bc00 str="e" range={9223372036854775807,0}
[text] 1786060352.892424 insertText:replacementRange: self=0x13101bc00 str="f" range={9223372036854775807,0}
```

After typing "abcdef" the URL bar remained empty — the bug reproduces with synthetic keystrokes via `osascript`, not just physical keyboard input. Findings:

- `insertText:replacementRange:` fires for every keystroke. AppKit's `interpretKeyEvents:` translates keys to text correctly.
- `doCommandBySelector:` fires once only, for `Cmd+L` (the URL bar focus command). Correctly routed.
- `hasMarkedText` returns `0` on every query. **The composition-stuck theory is dead** — no uncommitted IME composition ever existed.
- `replacementRange` is `{NSNotFound, 0}` — well-formed "no replacement range specified".

### Reentrant-flush hypothesis eliminated

The NSTextInputContext activate/deactivate pair fires **once, ~18 ms before typing starts**, then never brackets any `insertText:` call during the typing burst:

```
[text] 1786060147.476557 NSTIC -[deactivate] self=0x138b926e0
[text] 1786060147.477299 NSTIC -[activate]   self=0x138b926e0   (pair completes, 742µs gap)
[text] 1786060147.495758 hasMarkedText -> 0                     (18ms later)
[text] 1786060147.496907 insertText str="a" ...
```

That pair is Gecko's `NotifyIMEOfFocusChangeInGecko()` reacting to the URL bar getting focus via Cmd+L. By the time any `insertText:` arrives, `activate` has returned and `mIsInFocusProcessing` is false. **`IgnoreIMEComposition()` at `TextInputHandler.mm:2147` is not the drop**, and the 10.6-gated `[inputContext deactivate]; [inputContext activate];` dance at lines 2641-2642 — initially the highest-prior suspect — is not the proximate trigger either.

### What this leaves

The drop is somewhere in `TextInputHandler::InsertText` between lines 2147 and 2297, downstream of where AppKit hands off to Gecko. The remaining silent-drop candidates:

1. `BeginNativeInputTransaction()` failure at line 2245 — would log at `LogLevel::Error` if `MOZ_LOG` were active.
2. `MaybeDispatchKeypressEvents` at line 2285 dispatches but the editor sets `nsEventStatus_eConsumeNoDefault`.
3. Editor-side filter — the modifier-strip at lines 2277-2279 (comment: *"if they are included, nsPlaintextEditor ignores the event"*) is a documented editor-side filter that could eat a keypress if a stale modifier flag leaks through.

### Boundary reached (no build)

Two follow-ups attempted, both blocked:

- **`MOZ_LOG=TextInputHandlerWidgets:5`** — the LogModule name string is absent from PowerFox's XUL. MOZ_LOG is compiled out of this build; the env var has no effect.
- **DTrace pid provider** — the mangled symbol strings (`_ZN7mozilla6widget19TextEventDispatcher27BeginNativeInputTransactionEv`, `_ZN7mozilla6widget19TextEventDispatcher27MaybeDispatchKeypressEventsE...`, `_ZN7mozilla6widget15IMEInputHandler9IsFocusedEv`) are present in the binary per `grep -a`, but `dtrace` reported `does not match any probes` on the live pid. Whether that's inlining stripping the function entries or symbol-table format incompatibility with 10.6's dtrace wasn't resolved; the practical outcome is the same.

Going further requires a debug PowerFox build with `MOZ_LOGGING` defined, or a prologue-patch interposer that hooks the C++ functions directly. Both are past the spend for a curiosity bug.

## Related: graphics stack is cleared

If you arrived here from a `Crash Annotation GraphicsCriticalError: FEATURE_FAILURE_OPENGL_CREATE_CONTEXT` line in a PowerFox / Firefox / Basilisk crash log under VMQemuVGA, **that annotation is noise**. Gecko logs it when CGL can't find an accelerated renderer, then silently falls back to the software renderer, binds a surface, and composites correctly. Don't chase it. The full CGL trace clearing VMQemuVGA's whole GL stack is in `../cgl_trace/README.md`.

## Reusing

The pattern — runtime `objc_getClass` lookup + `method_setImplementation` + main-queue dispatch_after deferral — works for any Obj-C method on any class loaded after `main()` starts. That includes anything in a runtime-loaded framework, a plugin, or a host application's binary that's dlopen'd lazily.

# text_trace

Per-app `DYLD_INSERT_LIBRARIES` interposer that hooks ChildView's NSTextInputClient methods at runtime to localize where typed text gets dropped. Used to test three competing theories for the PowerFox text-input bug after the focus-state and compositor paths were cleared.

## What it captures

Four hooks on Gecko's `ChildView` class (defined in XUL):

| Method | Purpose |
|---|---|
| `insertText:replacementRange:` | The text-insertion call. Logs the string and replacement range. |
| `doCommandBySelector:` | The command-dispatch call. Logs the selector name. |
| `hasMarkedText` | Composition-state query. Logs return value. |
| `markedRange` | Composition-range query. Logs returned range. |

`interpretKeyEvents:` calls exactly one of `insertText:` or `doCommandBySelector:` per key. Logging both turns "insertText didn't fire" from a negative result into a positive one (you'd see keys routed to a command selector instead). The marked-text queries catch text landing in an uncommitted composition.

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
[text] install: ChildView=0x1058fe6b0
[text] install: hooked -[ChildView insertText:replacementRange:] (orig IMP=0x102bcfd40)
[text] install: hooked -[ChildView doCommandBySelector:]         (orig IMP=0x102bcff20)
[text] install: hooked -[ChildView hasMarkedText]                (orig IMP=0x102bcffe0)
[text] install: hooked -[ChildView markedRange]                  (orig IMP=0x102bcfb90)

[text] hasMarkedText -> 0
[text] insertText:replacementRange: str="a" range={9223372036854775807,0}
[text] hasMarkedText -> 0
[text] insertText:replacementRange: str="b" range={9223372036854775807,0}
... (c, d, e, f identical)

Totals: insertText=7  doCommandBySelector=1  hasMarkedText=7  markedRange=0
```

Findings:

- `insertText:replacementRange:` fires for every keystroke. AppKit's `interpretKeyEvents:` translates keys to text correctly.
- `doCommandBySelector:` fires once only, for `Cmd+L` (the URL bar focus command). Correctly routed.
- `hasMarkedText` returns `0` on every query. **The composition-stuck theory is dead** — no uncommitted IME composition ever existed.
- `markedRange` is never called.
- `replacementRange` is `{NSNotFound, 0}` — well-formed "no replacement range specified".

The bug is downstream of `insertText:replacementRange:`. AppKit delivers the text correctly to Gecko's `ChildView`; Gecko's C++ dispatch (`mTextInputHandler->InsertText()` at `widget/cocoa/TextInputHandler.mm:2116`) fails to commit it to the editor. One of the silent-drop guards inside that function — `IgnoreIMEComposition()`, the `isEditable` check on `context.mIMEState.mEnabled`, or the fork between `InsertTextAsCommittingComposition` and the direct keypress path — is firing.

That's the satisfying stop: the bug localized to one C++ method. Going further requires either a debug PowerFox build (the existing `MOZ_LOG(gLog, LogLevel::Info)` call at `TextInputHandler.mm:2127-2145` would print every parameter and dispatch decision) or DTrace on the running binary via the pid provider against mangled C++ symbols (DTrace has been on macOS since 10.5 — `nm XUL | grep -iE "IgnoreIMEComposition|IsIMEComposing|InsertText"` is a 30-second check for whether the relevant symbols survive in the release binary).

## Reusing

The pattern — runtime `objc_getClass` lookup + `method_setImplementation` + main-queue dispatch_after deferral — works for any Obj-C method on any class loaded after `main()` starts. That includes anything in a runtime-loaded framework, a plugin, or a host application's binary that's dlopen'd lazily.

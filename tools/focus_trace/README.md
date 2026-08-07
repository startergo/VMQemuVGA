# focus_trace

Per-app `DYLD_INSERT_LIBRARIES` interposer that swizzles `-[NSApplication sendEvent:]` to log the focus state on every `NSKeyDown`. Used to test the hypothesis that Gecko's `IsFocused()` returns false under VM, silently dropping text inserts via `IgnoreIMEComposition()`.

## What it captures

For each `NSKeyDown` arriving at `NSApplication`:

- `[NSApp isActive]` — app-active state
- `[NSApp keyWindow]` — pointer, class, and title of the key window
- `[keyWindow isKeyWindow]` — does the key window report itself as key
- `[keyWindow firstResponder]` — pointer and class of the first responder

These are exactly the three preconditions Gecko's `IMEInputHandler::IsFocused()` requires (`TextInputHandler.mm:3914`). If any is false while the window looks visually active, that's the bug — and a VM/WindowServer interaction rather than a Gecko bug.

## Build

```bash
clang -arch x86_64 -mmacosx-version-min=10.6 \
      -dynamiclib -o focus_interpose.dylib focus_interpose.m \
      -framework Cocoa -Wno-deprecated-declarations
```

Loads cleanly on Snow Leopard 10.6.8. ARC is off because libarclite doesn't ship for the 10.6 deployment target; the code uses no manual memory management that matters (every object is short-lived in scope).

## Deploy

```bash
scp focus_interpose.dylib sl@slqemu.local:/tmp/
DYLD_INSERT_LIBRARIES=/tmp/focus_interpose.dylib \
  /Applications/PowerFox.app/Contents/MacOS/powerfox 2>&1 | grep focus
```

## Mechanism

`+load` category on `NSApplication` (AppKit class, linked via `-framework Cocoa`) exchanges `sendEvent:` with `fi_sendEvent:` via `method_exchangeImplementations`. Inside the category method, calling `[self fi_sendEvent:event]` invokes the original IMP (standard swizzle pattern). Works because `NSApplication` is publicly available at compile time — no `-undefined dynamic_lookup` needed, unlike `text_trace`'s `ChildView` target.

## What this instrument established

Used against PowerFox 52.9 ESR on Snow Leopard 10.6.8 (kext `com.vmware.kext.VMQemuVGA 8.0.0d99` loaded, software renderer selected via CGL fallback), the swizzle produced on every keystroke:

```
[focus] NSKeyDown chars="a" keycode=0x0  appActive=1  keyWindow=0x12c9cf200(ToolbarWindow:"PowerFox Start Page")  isKeyWindow=1  firstResponder=0x130804780(ChildView)
```

All three `IsFocused()` preconditions held on every event. Hypothesis died cleanly — the bug is not in focus state or WindowServer delivery. Events arrive at `NSApplication sendEvent:` correctly, with `ChildView` as first responder of the key window of an active app.

## Reusing

The pattern is general — any Cocoa app, any NSResponder method, same skeleton. Drop the `+load` category on the target class (must be linkable at compile time; for classes in closed binaries use the runtime-IMP-replacement pattern from `tools/text_trace/` instead).

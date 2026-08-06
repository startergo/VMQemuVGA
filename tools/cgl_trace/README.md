# cgl_trace

A `DYLD_INSERT_LIBRARIES` interposer that traces the CGL and GL call surface of a closed binary (e.g. PowerFox, Safari, any Cocoa app using OpenGL) on Snow Leopard 10.6. Built once on a modern host, deployed per-app to the guest with no system modification.

## What it captures

Nine hooks via `__DATA,__interpose` (sanctioned two-level-namespace interposition, no flat-namespace requirement):

| Hook | Purpose |
|---|---|
| `CGLChoosePixelFormat` | Pixel format selection, attribute list, `npix` result |
| `CGLCreateContext` | Context creation, share context, return code |
| `CGLDestroyContext` | Teardown |
| `CGLSetCurrentContext` | Thread binding |
| `CGLSetSurface` | Window-system attachment (private API; forward-declared — see Build Notes) |
| `CGLFlushDrawable` | Swap, with entry/exit timestamps and gap-since-previous |
| `glGetString` | `GL_VENDOR` / `GL_RENDERER` / `GL_VERSION` / `GL_EXTENSIONS` / `GL_SHADING_LANGUAGE_VERSION` |
| `glGetError` | Non-zero results only (Gecko imports directly) |
| `glFlush` | First 8 calls then summary (Gecko imports directly) |

Each hook logs to stderr with the `LOG_TAG` prefix `[cgl-interpose]`, so output is greppable and separable from app noise. Forwarding is via the interpose pair itself — the "original" pointer in each pair is what the replacement calls through, which is also the pass-through primitive for a future marshalling layer.

## Build

Cross-compiled on a modern macOS host against the current SDK, targeting 10.6:

```bash
clang -arch x86_64 -mmacosx-version-min=10.6 \
      -dynamiclib -o cgl_log.dylib cgl_log.c \
      -framework OpenGL -Wno-deprecated-declarations
```

Produces a 13 KB dylib, universal-style single-slice x86_64, loads cleanly on 10.6.8 (verified on a Snow Leopard guest under QEMU/UTM).

## Deploy

```bash
# Copy to guest (one-time)
scp cgl_log.dylib sl@slqemu.local:/tmp/

# Run any app under the interposer
DYLD_INSERT_LIBRARIES=/tmp/cgl_log.dylib /Applications/SomeApp.app/Contents/MacOS/someapp 2>&1 | grep cgl-interpose
```

No reboot, no kext change, no system file modification. Per-app, per-launch. Safe to attach to an already-running use case for diagnosis.

## Build notes

- **`CGLSetSurface` is private API.** Declared in `CGLProfilerFunctionEnum.h` (as `kCGLFECGLSetSurface`) but not in the public `CGLCurrent.h` / `CGLContext.h`. The symbol exists in the 10.6 OpenGL binary; the source forward-declares the signature manually. The geom values logged for the last four args are unreliable as a result — verified by running Chess.app as a control: known-working GL apps produce the same "weird" values in those slots, so they aren't `(x, y, w, h)` as documented.
- **`kCGLBadCurrentContext` was removed from modern SDK headers** but the enum value (1009) is stable; the source casts the literal.
- **`__DATA,__interpose` requires no `DYLD_FORCE_FLAT_NAMESPACE`.** Two-level namespace binding is preserved. Each pair's "original" pointer is what the replacement calls through for forwarding.
- **`mach_absolute_time`** with `mach_timebase_info` gives monotonic microsecond timestamps for flush duration/gap measurement. Multiplication doesn't overflow for realistic uptimes.

## What this instrument established (kept for the next investigator)

Used against PowerFox 52.9 ESR (Gecko 52) under VMQemuVGA on Snow Leopard 10.6.8, the trace produced:

- **`CGLChoosePixelFormat attrs: Accelerated AllowOfflineRenderers DoubleBuffer -> npix=0`** — CGL finds zero accelerated renderers. VMQemuVGA's IOAccelerator is not visible to CGL's renderer-info query. This is a real, unsolved enumeration problem in the kext.
- **Gecko's fallback (`AllowOfflineRenderers DoubleBuffer` without `Accelerated`) succeeds**, selects `GL_RENDERER = "Apple Software Renderer"`, `GL_VERSION = "2.1 APPLE"`, FBOs present. Software path works end-to-end.
- **`CGLSetSurface` succeeds**, `CGLFlushDrawable` fires repeatedly with `err=ok`. Compositor paints.
- **Gecko emits `Crash Annotation GraphicsCriticalError: FEATURE_FAILURE_OPENGL_CREATE_CONTEXT`** in the log on the accelerated-path failure. **This looks alarming and means nothing** — Gecko falls back and composites correctly. Do not chase this log line as a bug.
- **Flush timing pattern**: ~50 ms per flush, ~500 ms gaps between flushes. Gecko's paint scheduler, not rasterization throughput, is the rate limiter on the software path.
- **Startup is the dominant cost**: ~19 s of pegged CPU before the compositor reaches steady state. After settling, main thread parks in `mach_msg_trap` inside `CFRunLoopRunSpecific` — healthy idle.

The instrument cleared VMQemuVGA's whole graphics stack as the cause of the original PowerFox input bug. The bug turned out to be in Gecko 52's chrome text-insertion pipeline, unrelated to the kext.

## Reusing for other targets

The hook set is CGL-centric but the pattern is general. For a different closed binary on 10.6:

1. Run `otool -Iv /path/to/binary | grep -E '_CGL|_gl'` to identify what it imports.
2. Drop hooks you don't need from the `INTERPOSE(...)` list at the bottom of `cgl_log.c`.
3. Add hooks for any other symbols you want to trace using the same `__DATA,__interpose` pattern.

Against WebKit 610 (or any browser using Cocoa's text-input path), the same skeleton works for Objective-C methods via `class_addMethod` / `method_exchangeImplementations` from `<objc/runtime.h>` — `__DATA,__interpose` doesn't apply to Obj-C methods, but the per-app `DYLD_INSERT_LIBRARIES` deploy mechanism does.

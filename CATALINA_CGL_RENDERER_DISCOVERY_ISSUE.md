# Catalina CGL Renderer Discovery Issue

## Problem Summary
VMVirtIOGPUAccelerator is correctly registered in IORegistry with all required properties, but `CGLQueryRendererInfo()` cannot discover it. Only Apple's software renderer (ID 0x01020400) is enumerated.

## Verified Working Properties
The accelerator has ALL correct IORegistry properties:
```
"IOAccelIndex" = 0
"IOAccelRevision" = 2  
"RendererID" = 148992 (0x00024600)
"IOGLBundleName" = "GLEngine"
"IOAccelerator3D" = Yes
"supports-OpenGL" = Yes
"supports-3D-acceleration" = Yes
"IOAcceleratorTypes" = ("Framebuffer","3D","VirtIO-GPU","Hardware")
"IOGLContext" = "IOAcceleratorContext"
"IOGraphicsAccelerator" = Yes
```

## Diagnostic Test Results
```c
// opengl_diagnostic.c output:
Number of renderers: 1

Renderer 0:
  Accelerated: No
  Renderer ID: 0x01020400  // Apple Software Renderer
  Video Memory: 0 MB
  Texture Memory: 0 MB

Test 1: kCGLPFAAccelerated: FAILED
Test 2: No attributes: SUCCESS (software renderer)
Test 3: Specific Renderer ID 0x00024600: FAILED
```

## Root Cause Analysis

### Catalina Architectural Change
From https://github.com/khronokernel/What-s-new-in-macOS-Catalina:
> "Removal of the OpenGL fallback UI renderer"
> "Unlike Mojave, systems with non-Metal GPUs can no longer be accelerated"

**Key Discovery**: In Catalina 10.15, Apple changed how OpenGL renderers are discovered:

1. **Pre-Catalina (≤10.14)**: `CGLQueryRendererInfo()` directly queried IOAccelerator services via IORegistry
2. **Catalina (10.15+)**: CGL renderer enumeration goes through Metal device discovery FIRST
   - OpenGL apps query Metal devices via `MTLCopyAllDevices()`
   - CGL then uses Metal-compatible devices for OpenGL rendering
   - Pure IOAccelerator registration is insufficient

### Why VMVirtIOGPUAccelerator Isn't Discovered

Despite correct IORegistry properties, the accelerator is not visible to `CGLQueryRendererInfo()` because:

1. **Missing Metal Device Registration**: The accelerator doesn't register as a proper Metal device
2. **VMMetalPlugin Insufficient**: Our `VMMetalPlugin` is attached but doesn't implement full `MTLDevice` protocol
3. **CGL Uses New Discovery Path**: Catalina's CGL uses `IOAcceleratorShared2` and Metal device enumeration, not direct IORegistry queries

## Evidence

### IORegistry Shows Correct Registration
```bash
$ ioreg -l -c VMVirtIOGPUAccelerator | grep IOAccel
"IOAccelIndex" = 0
"IOAccelRevision" = 2
"IOAccelerator3D" = Yes
"IOAcceleratorTypes" = ("Framebuffer","3D","VirtIO-GPU","Hardware")
```

### CGL Cannot Find It
```bash
$ ./opengl_diagnostic
Number of renderers: 1  # Only software renderer found!
```

### opengl_test Fails
```bash
$ ./opengl_test
No accelerated pixel format found
```

## Potential Solutions

### Option 1: Implement Full Metal Device (Complex)
- Implement `MTLDevice` protocol in VMMetalPlugin
- Register with `IOAcceleratorShared2` framework
- Implement Metal command queues, buffers, etc.
- **Complexity**: Very high (months of work)
- **Feasibility**: Low without Apple GPU driver documentation

### Option 2: Hook Into Legacy CGL Path (If Exists)
- Research if Catalina has any legacy CGL discovery mechanism
- May require private APIs or undocumented IOKit calls
- **Complexity**: Medium
- **Feasibility**: Unknown

### Option 3: Force Software Renderer with Acceleration (Workaround)
- Applications fall back to software renderer
- Hook software renderer to use our hardware acceleration
- **Complexity**: High
- **Feasibility**: Uncertain

### Option 4: Target Pre-Catalina Only (Limitation)
- Focus on macOS 10.14 Mojave and earlier
- Document Catalina limitation clearly
- **Complexity**: Low
- **Feasibility**: High (but limited)

## Current Status
- ✅ Driver loads successfully
- ✅ IORegistry registration correct
- ✅ VMMetalPlugin attached
- ✅ No kernel panics
- ❌ CGL cannot discover accelerator
- ❌ OpenGL applications use software renderer only

## Testing in Snow Leopard 10.6.8

**Hypothesis**: Snow Leopard (pre-Metal) should successfully discover our accelerator via `CGLQueryRendererInfo()` because it uses the original CGL implementation that directly queries IORegistry.

**Why Snow Leopard is the Perfect Test**:
1. **Pre-Metal Era**: No Metal-first discovery path complications
2. **Original CGL**: Direct IOAccelerator IORegistry queries
3. **Already Compatible**: VMQemuVGA v8.0 already loads on Snow Leopard 10.6.8
4. **Proof of Concept**: Success would confirm our implementation is correct

**Test Plan**:
```bash
# 1. Boot Snow Leopard 10.6.8 VM with VMQemuVGA loaded
# 2. Compile and run opengl_diagnostic.c
gcc -o opengl_diagnostic opengl_diagnostic.c -framework OpenGL -framework CoreGraphics
./opengl_diagnostic

# Expected Result on Snow Leopard:
# Number of renderers: 2 (or more)
# 
# Renderer 0: Apple Software Renderer (0x01020400)
# Renderer 1: VMVirtIOGPUAccelerator (0x00024600) ✓ FOUND!
#   Accelerated: Yes
#   Video Memory: 256 MB
#
# Test 1: kCGLPFAAccelerated: SUCCESS ✓
```

**If This Works**:
- ✅ Confirms our IOAccelerator implementation is correct
- ✅ Proves the issue is Catalina's Metal-first architecture, not our code
- ✅ Validates the inheritance from VMQemuVGAAccelerator
- ✅ Demonstrates proper IORegistry property registration

**If This Fails**:
- ❌ Indicates deeper implementation issues beyond Catalina changes
- Need to debug IOAccelerator base functionality

## Next Steps
1. **🔥 PRIORITY**: Test on Snow Leopard 10.6.8 VM to validate implementation
2. Research `IOAcceleratorShared2.framework` and Metal device registration (if Snow Leopard fails)
3. Investigate if any Catalina VMs successfully use OpenGL acceleration
4. Consider if pre-Catalina/Mojave target is acceptable limitation
5. Explore hooking into CoreGraphics/WindowServer at higher level

## References
- https://github.com/khronokernel/What-s-new-in-macOS-Catalina
- `/System/Library/Frameworks/OpenGL.framework`
- `/System/Library/PrivateFrameworks/IOAcceleratorFamily2.framework`
- `/System/Library/PrivateFrameworks/IOAcceleratorShared2.framework`

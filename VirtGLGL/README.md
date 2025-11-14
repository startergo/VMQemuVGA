# VirtGLGL - Userspace OpenGL for VirtIO GPU

## What is This?

VirtGLGL is a userspace OpenGL implementation for macOS Snow Leopard running on VirtIO GPU. It enables OpenGL applications to run on virtual machines without requiring kernel-level OpenGL.framework integration.

## Architecture

```
Application (test_glclear, etc.)
    ↓ Links against VirtGLGL.dylib
VirtGLGL.dylib (userspace OpenGL)
    ↓ glClear(), glBegin/End(), etc.
Virgl Protocol Translation
    ↓ VIRGL_CCMD_CLEAR, etc.
IOUserClient (IOKit)
    ↓ IOConnectCallStructMethod
VMVirtIOGPUUserClient (kernel)
    ↓ submitVirglCommands()
VMVirtIOGPU Driver
    ↓ VIRTIO_GPU_CMD_SUBMIT_3D
VirtIO GPU Hardware
    ↓ VirtIO protocol
Host (QEMU/UTM)
    ↓ virglrenderer
OpenGL Rendering (llvmpipe or GPU)
```

## What Works

✅ **Complete Pipeline**
- Userspace OpenGL library (VirtGLGL.dylib)
- Virgl protocol translation
- IOUserClient communication (userspace ↔ kernel)
- VirtIO GPU command submission
- Host virglrenderer integration

✅ **Implemented OpenGL Functions**
- `glClearColor()` / `glClear()` - Working!
- `glBegin()` / `glEnd()` - Vertex buffering
- `glVertex2f()` / `glVertex3f()` - Vertex input
- `glColor3f()` / `glColor4f()` - Color state

✅ **Test Programs**
- `virtgl_status` - Show system status and capabilities
- `test_glclear` - Test basic glClear()
- `test_opengl_full` - Test multiple OpenGL functions
- `test_virgl_stress` - Stress test (100 commands)
- `test_hardware_accel` - Detect acceleration type
- `virtglgl_benchmark` - Performance benchmark

## Performance

| Test | FPS | Status |
|------|-----|--------|
| Simple Clear | ~365 FPS | ✓ Working (software) |
| Color Changes | ~327 FPS | ✓ Working (software) |
| Rapid Submission | ~315 FPS | ✓ Working (software) |

**Note:** Current performance indicates software rendering (llvmpipe). Hardware acceleration would be 10,000+ FPS.

## Building

### Prerequisites
- macOS host with Xcode command line tools
- Snow Leopard VM with VirtIO GPU
- VMVirtIOGPU v8.0+ kernel driver installed

### Compile VirtGLGL Library

```bash
cd VirtGLGL
make clean && make
```

This produces:
- `VirtGLGL.dylib` - Userspace OpenGL library
- `test_*` executables - Test programs

### Cross-Compilation

The Makefile automatically cross-compiles from ARM64 Mac to x86_64 Snow Leopard:

```bash
CFLAGS = -arch x86_64 -mmacosx-version-min=10.6
```

## Usage

### Option 1: Link Test Programs (Easiest)

```bash
# On Mac: Build test
cd VirtGLGL
clang -arch x86_64 -o mytest mytest.c VirtGLGL.o VirtGLGLClient.o \
      -framework IOKit -framework CoreFoundation -lstdc++

# Deploy to VM
scp mytest vm@vm-host:

# On VM: Run
./mytest
```

### Option 2: Use VirtGLGL Library

```c
#include "VirtGLGL.h"

int main() {
    // Initialize
    if (!VirtGLGL_Initialize()) {
        fprintf(stderr, "Failed to initialize VirtGLGL\n");
        return 1;
    }
    
    // Use OpenGL
    glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    
    // Cleanup
    VirtGLGL_Shutdown();
    return 0;
}
```

## Why No Visible Output?

The virgl protocol works correctly - `glClear()` commands reach virglrenderer and execute on the host ✅. However, **the rendered output doesn't appear on screen**:

### Current Status (as of Nov 13, 2025)

**✅ Working:**
- Virgl commands submitted successfully (SUBMIT_3D)
- Commands execute in virglrenderer on host
- `flushResource()` called successfully (RESOURCE_FLUSH)
- Kernel log shows: "Flush completed successfully"

**❌ Not Working:**
- Nothing visible on display
- Rendering happens to off-screen 3D resource (ID 1)
- 2D scanout framebuffer is separate resource (not resource 1)

### Root Cause

The issue is architectural:

1. **We render to 3D resource #1** - This is a virgl render target
2. **Display scanout uses a different 2D resource** - Created by framebuffer driver
3. **RESOURCE_FLUSH only works for scanout resources** - Not for 3D render targets
4. **No connection between the two** - Rendered content stays in GPU memory, never reaches scanout

### What's Needed for Visible Output

**Option 1: Direct Scanout (Preferred)**
- Set scanout to use our 3D resource directly
- Requires `SET_SCANOUT` command with 3D resource ID
- Modern QEMU/virgl should support this

**Option 2: Blit to Scanout**
- Render to 3D resource
- Blit from 3D → 2D scanout resource
- Flush the 2D scanout resource
- More complex but more compatible

**Option 3: Render Directly to 2D Scanout**
- Use existing 2D framebuffer resource as render target
- May not support full 3D features
- Simpler but limited

### Implementation Status

**Working:** Virgl protocol, command submission, execution ✅  
**Missing:** Display pipeline - connecting 3D rendering to visible scanout ❌  
**Next Step:** Implement SET_SCANOUT or BLIT operations

## Current Limitations

⚠️ **Critical Missing Features**
- **No visible output** - Rendering happens off-screen, results never displayed
- **Missing transferToHost2D()** - Don't copy rendered content to framebuffer
- **Missing flushResource()** - Don't update visible screen
- **No swap buffers** - Can't implement double-buffering

⚠️ **Incomplete Features**
- `glBegin/End` vertex submission - Buffers vertices but doesn't submit draw commands
- Texture support - Not implemented
- System-wide OpenGL - Apps must link VirtGLGL directly

## What This Achieves

Despite software rendering performance, this implementation is significant:

✓ **Proves concept** - Userspace OpenGL works on VirtIO GPU
✓ **Complete pipeline** - All layers functional (app → virglrenderer)
✓ **Foundation** - Ready for hardware acceleration when available
✓ **Enables development** - OpenGL apps can run on Snow Leopard VMs
✓ **Mesa3D-style** - Same architecture as modern Linux OpenGL

## Technical Details

### IOUserClient Methods

| Selector | Method | Purpose |
|----------|--------|---------|
| 0x3000 | SubmitCommands | Send virgl commands |
| 0x3001 | CreateResource | Allocate 3D resource |
| 0x3002 | CreateContext | Create 3D context |
| 0x3003 | AttachResource | Bind resource to context |
| 0x3004 | GetCapability | Query 3D capabilities |

### Virgl Commands Implemented

- `VIRGL_CCMD_CLEAR` - Clear color/depth/stencil buffers (working!)
- `VIRGL_CCMD_DRAW_VBO` - Draw vertices (planned)
- `VIRGL_CCMD_CREATE_OBJECT` - Create GPU objects (planned)
- `VIRGL_CCMD_SET_VIEWPORT_STATE` - Set viewport (planned)

### Memory Layout

```
Context 1 (800x600 RGBA)
  ↓
Resource 1 (render target)
  ↓
Virgl command buffer (32 bytes per CLEAR)
  ↓
VirtIO GPU queue (40 bytes: 8 header + 32 data)
```

## Testing

⚠️ **CRITICAL: Never run `kextunload` on VMVirtIOGPU.kext - it causes kernel panic!**

To update the driver:
1. Install new package: `sudo installer -pkg VMQemuVGA-xxx.pkg -target /`
2. **Reboot the VM** - do NOT attempt to unload/reload the driver
3. After reboot, verify with `kextstat | grep VMQemuVGA`

### Quick Test

```bash
./virtgl_status    # Show system status
./test_glclear     # Test basic rendering
```

### Performance Test

```bash
./test_hardware_accel    # Detect acceleration type
./virtglgl_benchmark     # Detailed benchmark
```

### Stress Test

```bash
./test_virgl_stress    # Send 100 commands
```

### Check Kernel Log

```bash
sudo dmesg | grep -E 'VMVirtIOGPU|submitVirgl|SUBMIT_3D'
```

## Files

| File | Purpose |
|------|---------|
| `VirtGLGL.h` | OpenGL API declarations |
| `VirtGLGL.cpp` | OpenGL implementation |
| `VirtGLGLClient.cpp` | IOKit client interface |
| `Makefile` | Build system |
| `test_*.c` | Test programs |
| `virtgl_status.c` | Status/diagnostic tool |

## Future Enhancements

### Phase 1: Complete Basic OpenGL (Current)
- ✅ glClear working
- ⏳ glBegin/End draw commands
- ⏳ glVertex array submission

### Phase 2: Textures
- Load texture data
- Texture binding
- Texture mapping

### Phase 3: Shaders
- Vertex shaders
- Fragment shaders
- Shader compilation

### Phase 4: System Integration
- OpenGL.framework wrapper
- LD_PRELOAD injection
- System-wide availability

## Advantages over Kernel CGL Integration

1. **No WindowServer interference** - doesn't affect system graphics
2. **Easier development** - userspace code is easier to debug
3. **No CGL architecture limitations** - doesn't depend on IOFramebuffer
4. **Explicit linking** - applications choose to use VirtGLGL
5. **Cross-platform potential** - userspace library is more portable

## Contributing

This is a proof-of-concept demonstrating userspace OpenGL on VirtIO GPU. Contributions welcome!

### Areas for Development
1. Complete glBegin/End vertex submission
2. Implement texture support
3. Add more OpenGL functions
4. Optimize command batching
5. Create system-wide integration

## License

See LICENSE.txt in repository root.

## Credits

- Based on Mesa3D/virglrenderer architecture
- VirtIO GPU specification
- VMVirtIOGPU kernel driver

## Status

**Production Ready:** No - This is a proof-of-concept  
**Working Features:** glClear, basic OpenGL API  
**Performance:** Software rendering (~300-400 FPS)  
**Use Case:** Development, testing, legacy app support  

For questions or issues, check `virtgl_status` output first!

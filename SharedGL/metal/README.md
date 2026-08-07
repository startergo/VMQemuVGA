# SharedGL Metal - Transparent GPU Acceleration for VM

**Any OpenGL app in VM → Transparently rendered on M4 Pro GPU → Displayed in VM**

## Vision

The goal is to make **any OpenGL application** running in a Catalina VM render using the host's M4 Pro GPU, completely transparently to the application. The user launches an OpenGL app in the VM (games, CAD, 3D modeling, etc.) and it just works - using hardware acceleration from the host GPU instead of slow software rendering.

## Current Status

**Phase 5 (COMPLETE)**: Direct command protocol working
- VM test client (`vm_fbo_test.c`) successfully renders via M4 Pro GPU
- FBO, textures, shaders, VBOs all functional
- Proof-of-concept validates the Metal rendering pipeline

**Phase 7 (IN PROGRESS)**: OpenGL interception library
- `gl_to_metal_client.c` - Prototype DYLD_INTERPOSE wrapper
- Will intercept OpenGL calls from ANY application
- Translates GL calls → Metal commands → sends to host server

## Architecture (Target)

```
┌─────────────────────────────────────────────────────────────────┐
│  Catalina VM (Guest)                                            │
│                                                                 │
│  ┌──────────────────┐         ┌───────────────────────┐         │
│  │  ANY OpenGL App  │────────▶│  libGLMetal.dylib     │         │
│  │  (Unmodified!)   │         │  (Interception Layer) │         │
│  │  • Games         │         │                       │         │
│  │  • CAD Software  │         │  Intercepts:          │         │
│  │  • 3D Tools      │         │  • glBegin/glEnd      │         │
│  │  • Legacy Apps   │         │  • glDrawArrays       │         │
│  └──────────────────┘         │  • glTexImage2D       │         │
│         ▲                     │  • All GL calls...    │         │
│         │                     └───────────┬───────────┘         │
│         │                                 │                     │
│    App sees normal                   Translates to              │
│    OpenGL behavior                   Metal commands             │
│    (display in VM)                   (Binary protocol)          │
│                                             │                    │
└─────────────────────────────────────────────┼───────────────────┘
                                              │
                                       SSH Tunnel (TCP)
                                       127.0.0.1:28123
                                              │
┌─────────────────────────────────────────────▼───────────────────┐
│  macOS Host (M4 Pro)                                            │
│                                                                 │
│  ┌────────────────────┐        ┌──────────────────────┐         │
│  │  metal_server      │───────▶│   M4 Pro GPU         │         │
│  │  (Metal Executor)  │        │   (Native Metal)     │         │
│  │                    │        │   • Fast rendering   │         │
│  │  Executes Metal    │        │   • Hardware accel   │         │
│  │  commands on GPU   │        │   • No CPU overhead  │         │
│  └────────────────────┘        └──────────────────────┘         │
│                                                                 │
│  Results sent back to VM for display                            │
└─────────────────────────────────────────────────────────────────┘
```

## Quick Start

### 1. Build Metal Server (on host Mac M4 Pro)

```bash
cd /Users/macbookpro/VMQemuVGA/SharedGL/metal
./build_server.sh
```

### 2. Start Metal Server

```bash
cd /Users/macbookpro/VMQemuVGA
./build/metal/metal_server
```

You should see:
```
========================================
   SharedGL Metal Server
   M4 Pro GPU Executor
========================================
[Metal Server] Metal Device: Apple M4 Pro
[Metal Server] ✅ Metal pipeline created
[Metal Server] ✅ Server listening on 0.0.0.0:28123
```

### 3. Setup SSH Tunnel (reverse tunnel to VM)

```bash
ssh -i vm-ssh-key -f -N -R 28123:localhost:28123 qemucat@qemucat.local
```

This makes the Metal server accessible on VM's localhost:28123

### 4. Build OpenGL Interception Library (Phase 7 - In Progress)

```bash
cd /Users/macbookpro/VMQemuVGA/SharedGL/metal
./build_client.sh
```

This creates `libGLMetal.dylib` - the transparent OpenGL→Metal translation layer.

### 5. Run ANY OpenGL App with GPU Acceleration (Target)

```bash
# Deploy library to VM
scp -i vm-ssh-key build/metal/libGLMetal.dylib qemucat@qemucat.local:~/

# In VM - inject into ANY OpenGL application:
ssh -i vm-ssh-key qemucat@qemucat.local
DYLD_INSERT_LIBRARIES=~/libGLMetal.dylib /Applications/YourOpenGLApp.app/Contents/MacOS/YourOpenGLApp
```

The app runs **unmodified** but renders on M4 Pro GPU instead of software rendering!

### Current Test (Phase 5)

```bash
# Build test client
clang -arch x86_64 -o build/vm_test/vm_fbo_test SharedGL/tests/vm_fbo_test.c

# Deploy and run
scp -i vm-ssh-key build/vm_test/vm_fbo_test qemucat@qemucat.local:~/
ssh -i vm-ssh-key qemucat@qemucat.local "./vm_fbo_test"
```

This validates the Metal command protocol works (RGB gradient triangle on host window).

## Features

### Phase 5: Framebuffer Objects (FBO) - ✅ COMPLETE

Full render-to-texture pipeline with:
- **FBO creation and binding** - Create offscreen render targets
- **Texture attachments** - Attach RGBA textures to FBO
- **Render to FBO** - Draw geometry to offscreen texture
- **Texture sampling** - Sample FBO texture in shaders
- **Custom shaders** - GLSL→MSL translation with texture support
- **Feedback loop prevention** - Automatic detection and mitigation
- **GPU synchronization** - waitUntilCompleted for texture readiness

### Supported Commands

#### Buffer Management
- `GEN_BUFFERS` - Create vertex/index buffers
- `BIND_BUFFER` - Bind buffer to target (ARRAY_BUFFER, ELEMENT_ARRAY_BUFFER)
- `BUFFER_DATA` - Upload data to GPU buffer
- `DELETE_BUFFERS` - Clean up buffers

#### Vertex Arrays (VAO)
- `GEN_VERTEX_ARRAYS` - Create VAO configurations
- `BIND_VERTEX_ARRAY` - Activate VAO
- `VERTEX_ATTRIB_POINTER` - Define vertex attributes (position, color, texcoord)
- `ENABLE_VERTEX_ATTRIB_ARRAY` - Enable attribute slots
- `DELETE_VERTEX_ARRAYS` - Clean up VAOs

#### Shaders & Programs
- `CREATE_SHADER` - Create vertex/fragment shaders
- `SHADER_SOURCE` - Upload GLSL shader source (auto-translated to MSL)
- `COMPILE_SHADER` - Compile shader on GPU
- `ATTACH_SHADER` - Attach shader to program
- `LINK_PROGRAM` - Link program and create Metal pipeline
- `USE_PROGRAM` - Activate shader program (0 = default pipeline)
- `DELETE_SHADER`, `DELETE_PROGRAM` - Clean up

#### Textures
- `GEN_TEXTURES` - Create texture objects
- `BIND_TEXTURE` - Bind texture to target (TEXTURE_2D)
- `TEX_IMAGE_2D` - Upload texture data (width, height, format, pixels)
- `TEX_PARAMETER` - Set filtering/wrapping (MIN_FILTER, MAG_FILTER, WRAP_S, WRAP_T)
- `DELETE_TEXTURES` - Clean up textures

#### Framebuffer Objects (FBO)
- `GEN_FRAMEBUFFERS` - Create FBO
- `BIND_FRAMEBUFFER` - Bind FBO (0 = screen, >0 = offscreen)
- `FRAMEBUFFER_TEXTURE_2D` - Attach texture as color attachment
- `CHECK_FRAMEBUFFER_STATUS` - Verify FBO completeness
- `DELETE_FRAMEBUFFERS` - Clean up FBOs

#### Drawing
- `DRAW_ARRAYS` - Draw primitives (TRIANGLES, TRIANGLE_STRIP, LINES)
- `DRAW_ELEMENTS` - Draw indexed primitives
- `CLEAR` - Clear color/depth buffers
- `VIEWPORT` - Set viewport dimensions

## Test Results

### Phase 5 FBO Test (vm_fbo_test.c)

**Test scenario**: Render RGB gradient triangle to 512×512 FBO, then sample that texture on a fullscreen quad

**Results**:
- ✅ FBO creation and texture attachment working
- ✅ Render-to-texture functional
- ✅ Custom GLSL shaders compiled and linked
- ✅ GLSL→MSL translation with texture2D() support
- ✅ Texture feedback loop detection and prevention
- ✅ RGB gradient rendered correctly (red/green/blue vertices)
- ✅ GPU synchronization via waitUntilCompleted
- ✅ Vertex descriptor auto-detection (stride 20 vs 28)

**Visual confirmation**: RGB gradient triangle on magenta background displayed in Metal server window

## Building from Source

### Metal Server (M4 Pro Mac)
```bash
cd /Users/macbookpro/VMQemuVGA/SharedGL/metal
./build_server.sh
```

Output: `build/metal/metal_server` (Cocoa app with Metal rendering)

### VM Test Client (x86_64)
```bash
cd /Users/macbookpro/VMQemuVGA
mkdir -p build/vm_test
clang -arch x86_64 -o build/vm_test/vm_fbo_test SharedGL/tests/vm_fbo_test.c
```

Output: `build/vm_test/vm_fbo_test` (headless TCP client)

## Debugging

### Check Metal server status:
```bash
# On host
ps aux | grep metal_server
lsof -i :28123
```

Should show server listening on port 28123

### Check SSH tunnel:
```bash
# On host
ps aux | grep "ssh.*28123"
```

Should show `ssh -R 28123:localhost:28123` process

### Check VM can reach server:
```bash
# In VM
nc -zv 127.0.0.1 28123
```

Should show "Connection succeeded"

### VM test output:
```
[VM Test] Creating 512x512 FBO texture...
[VM Test] Creating FBO...
[VM Test] Program linked: SUCCESS
[VM Test] Using default colored shader for triangle
[VM Test] Rendering triangle to FBO 1...
[VM Test] Using textured shader program 1
[VM Test] ✅ FBO render test complete!
```

### Server logs (successful render):
```
[Metal Server] ✅ Program 1 linked successfully
[Metal Server] Program 1: Detected colored shader (stride=28 bytes)
[Metal Server] Bind framebuffer: target=0x8D40 fbo=1
[Metal Server] Using custom shader pipeline for program 1
[Metal Server] Rendering to FBO 1 (512x512 RGBA texture)
```

## Current Limitations

- **No depth/stencil buffers** - Only color attachments supported
- **No multiple render targets (MRT)** - Single color attachment only
- **No mipmaps** - FBO textures are single-level
- **No MSAA** - No multisampling anti-aliasing yet
- **No compute shaders** - Only vertex/fragment pipeline
- **No geometry/tessellation shaders** - Basic pipeline only
- **No transform feedback** - Can't capture vertex output
- **No instanced rendering** - DrawArraysInstanced not yet supported

## Future Enhancements

### Phase 6: Advanced FBO Features
1. **Depth/stencil attachments** - Renderbuffer objects
2. **Multiple render targets** - Multiple color attachments
3. **FBO blit operations** - Copy between FBOs
4. **Mipmap generation** - glGenerateMipmap for FBO textures

### Phase 7: OpenGL Interception Library (IN PROGRESS)
1. **DYLD_INTERPOSE wrapper** - Inject into any OpenGL app ✅ (prototype exists)
2. **Full OpenGL 3.x/4.x API** - Complete function coverage (in progress)
3. **State tracking** - Replicate OpenGL state machine
4. **Backwards compatibility** - Support legacy immediate mode (glBegin/glEnd)
5. **Framebuffer readback** - Return rendered pixels to VM for display
6. **Event handling** - Mouse/keyboard input from VM to host window

### Phase 8: Performance Optimization
1. **IOSurface sharing** - Zero-copy buffer sharing between processes
2. **Command batching** - Reduce TCP round-trips
3. **Async command queues** - Non-blocking sends
4. **Compression** - Compress vertex/texture data

## Why This Approach?

### Advantages Over Kernel Driver (IOAccelSurfaceClient)

| Aspect | Metal Translation | IOAccelSurfaceClient |
|--------|------------------|---------------------|
| **Stability** | ✅ User-space, no kernel panics | ❌ Kernel code, crashes WindowServer |
| **Development** | ✅ Fast iteration, easy debugging | ❌ Requires reboots, hard to debug |
| **Portability** | ✅ Works across macOS versions | ❌ Private APIs, version-specific |
| **Safety** | ✅ Process isolation | ❌ Kernel-mode, system-wide impact |
| **GPU Access** | ✅ Full M4 Pro Metal features | ❌ Limited to IOGraphics APIs |

### User Experience

From the user's perspective in the VM:
- Launch any OpenGL application normally
- App displays in VM window as expected
- Runs smoothly with GPU acceleration
- **No visible difference** except performance

Behind the scenes:
- OpenGL calls intercepted by `libGLMetal.dylib`
- Commands sent to host Metal server
- Rendering happens on M4 Pro GPU
- Pixels sent back to VM for display
- **Completely transparent** to the application

## Success Criteria

Phase 5 FBO test is working when:

1. ✅ Metal server shows "✅ Server listening on 0.0.0.0:28123"
2. ✅ VM test shows "✅ FBO render test complete!"
3. ✅ Server logs show "Rendering to FBO" and "Using custom shader pipeline"
4. ✅ Metal server window displays RGB gradient triangle (red/green/blue)
5. ✅ Activity Monitor shows M4 Pro GPU usage (low but non-zero)
6. ✅ No texture feedback loop warnings in final render

## Files

- `metal/metal_server.m` - Host Metal executor (~2600 lines)
- `tests/vm_fbo_test.c` - VM test client (~480 lines)
- `metal/build_server.sh` - Server build script
- `metal/gl_to_metal_client.c` - OpenGL interception stub (experimental, not used)

## License

Same as VMQemuVGA main project.

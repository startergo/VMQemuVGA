> **⚠️ SUPERSEDED 2026-08-09 — see [`SUPERSEDED.md`](SUPERSEDED.md).**
> Reference-only. Success claims below were never validated; the
> strategic direction is Mesa + virgl, not GLEngine.bundle replacement.

# VMVirtIOGLEngine Version 2.0 - Implementation Summary

## What We've Implemented

### 1. ✅ gliQueryRendererInfo() - Full Implementation
**Purpose**: Tell CGL about our hardware renderer capabilities

**Implementation Details**:
- Checks if VMVirtIOGPUAccelerator is available
- Returns detailed renderer capabilities structure
- Reports 256MB VRAM (placeholder for now)
- Sets renderer ID to 0x00024600 (VIRTIO_RENDERER_ID)
- Marks renderer as:
  - Hardware accelerated ✓
  - Window capable ✓
  - Fullscreen capable ✓
  - Multi-processor safe ✓
  - OpenGL 2.1 compatible ✓

**Data Returned**:
```c
VirtIORendererInfo {
    renderer_id: 0x00024600
    accelerated: 1
    window: 1
    fullscreen: 1
    video_memory: 256 MB
    major_gl_version: 2
    minor_gl_version: 1
}
```

### 2. ✅ gliChoosePixelFormat() - Full Implementation
**Purpose**: Create pixel format objects based on requested attributes

**Implementation Details**:
- Parses all CGLPixelFormatAttribute requests
- Supports common attributes:
  - Color depth (8, 16, 24, 32-bit)
  - Alpha channel
  - Depth buffer (16, 24, 32-bit)
  - Stencil buffer (8-bit)
  - Double buffering
  - Multisampling (MSAA)
  - Display mask
- Checks VMVirtIOGPUAccelerator availability for accelerated formats
- Creates VirtIOPixelFormat structure with requested capabilities
- Returns valid pixel format object handle

**Attributes Parsed**:
- `kCGLPFAAccelerated` - Hardware acceleration
- `kCGLPFADoubleBuffer` - Double buffering
- `kCGLPFAColorSize` - Color buffer depth
- `kCGLPFAAlphaSize` - Alpha channel depth
- `kCGLPFADepthSize` - Depth buffer depth
- `kCGLPFAStencilSize` - Stencil buffer depth
- `kCGLPFASamples` - MSAA sample count
- `kCGLPFADisplayMask` - Display selection

### 3. ✅ gliCreateContext() - Full Implementation
**Purpose**: Create OpenGL rendering context

**Implementation Details**:
- Allocates VirtIOContext structure
- Stores pixel format reference
- Attempts IOKit connection to VMVirtIOGPUAccelerator
- Opens IOService connection for GPU commands
- Initializes default OpenGL state:
  - Viewport: 640x480
  - Clear color: Black (0,0,0,1)
  - Swap interval: 1 (vsync on)
- Returns context handle on success

**Connection Logic**:
```c
1. Find VMVirtIOGPUAccelerator service
2. Call IOServiceOpen() to create connection
3. Store io_connect_t handle in context
4. Ready to send GPU commands via IOConnectCall*()
```

### 4. ✅ VMVirtIOGPUAccelerator Connection - Implemented
**Purpose**: Connect to IOKit driver for actual GPU commands

**Implementation Status**:
- ✅ Service discovery via IOServiceMatching()
- ✅ Connection establishment via IOServiceOpen()
- ✅ Connection handle storage in context
- ✅ Proper cleanup in gliDestroyContext()
- ⚠️ TODO: Actual GPU command submission (needs driver selectors)

**IOKit Integration Points**:
```c
// In gliQueryRendererInfo()
io_service_t service = IOServiceGetMatchingService(...)
if (service) { /* Hardware available */ }

// In gliCreateContext()
IOServiceOpen(service, mach_task_self(), 0, &context->connection)
// context->connection now ready for IOConnectCallMethod()

// In gliDestroyContext()
IOServiceClose(context->connection)
IOObjectRelease(context->accelerator)
```

## Additional Functions Implemented

### Context Management
- ✅ `gliDestroyContext()` - Cleanup context and IOKit connection
- ✅ `gliAttachDrawable()` - Attach rendering target
- ✅ `gliSetAttribute()` - Set context parameters (swap interval, etc.)
- ✅ `gliGetAttribute()` - Query context parameters
- ✅ `gliSwapBuffers()` - Flush and present rendered frame
- ✅ `gliCopyAttributes()` - Copy settings between contexts

### Pixel Format Management
- ✅ `gliDestroyPixelFormat()` - Cleanup pixel format object

### Renderer Info Management
- ✅ `gliDestroyRendererInfo()` - Cleanup renderer info object

## What Should Happen Now

### Expected Behavior
1. **Renderer Detection**: CGL should now detect VMVirtIOGLEngine as available hardware renderer
2. **Pixel Format Creation**: Applications can create accelerated pixel formats
3. **Context Creation**: OpenGL contexts should be created successfully
4. **IOKit Connection**: Contexts should have live connection to VMVirtIOGPUAccelerator

### Current Limitations
1. **No Actual GPU Commands**: We connect to the accelerator but don't send rendering commands yet
2. **Software Fallback**: Rendering still uses Apple's software rasterizer
3. **Missing Driver Methods**: Need to implement IOKit selectors in VMVirtIOGPUAccelerator for:
   - Command buffer submission
   - Texture upload
   - Shader compilation
   - Draw calls

## Next Phase: GPU Command Submission

To make actual hardware acceleration work, we need to:

### 1. Define IOKit Method Selectors in Driver
```cpp
enum {
    kVMVirtIOGPUMethodSubmitCommands = 0,
    kVMVirtIOGPUMethodCreateTexture = 1,
    kVMVirtIOGPUMethodUploadTexture = 2,
    kVMVirtIOGPUMethodCompileShader = 3,
    // ... etc
};
```

### 2. Implement Methods in GLEngine
```cpp
// In gliSwapBuffers()
uint64_t input = 0;
IOConnectCallScalarMethod(
    context->connection,
    kVMVirtIOGPUMethodSubmitCommands,
    &input, 1, NULL, NULL
);
```

### 3. Handle Commands in Driver
```cpp
IOReturn VMVirtIOGPUAccelerator::externalMethod(
    uint32_t selector,
    IOExternalMethodArguments* args,
    ...
) {
    switch (selector) {
        case kVMVirtIOGPUMethodSubmitCommands:
            return submitCommandBuffer(...);
        // ... etc
    }
}
```

## Testing Instructions

### Compile on Snow Leopard
```bash
ssh -o HostKeyAlgorithms=+ssh-rsa,ssh-dss sl@slqemu.local
chmod +x compile_on_snowleopard.sh
./compile_on_snowleopard.sh
```

### Install and Test
```bash
# Backup original (if not done)
sudo cp -r /System/Library/Frameworks/OpenGL.framework/Resources/GLEngine.bundle \
           /System/Library/Frameworks/OpenGL.framework/Resources/GLEngine.bundle.backup

# Install new version
sudo cp VMVirtIOGLEngine.bundle/VMVirtIOGLEngine \
        /System/Library/Frameworks/OpenGL.framework/Resources/GLEngine.bundle/GLEngine

# Test
./test_cocoa_opengl
```

### Expected Console Output
```
VMVirtIOGLEngine: gliQueryRendererInfo() called
VMVirtIOGLEngine: SUCCESS - Reporting 1 hardware renderer (ID=0x00024600)
VMVirtIOGLEngine: gliChoosePixelFormat() called
VMVirtIOGLEngine: Parsing pixel format attributes:
  - Accelerated requested
  - Double buffer requested
  - Color size: 24
  - Depth size: 24
VMVirtIOGLEngine: SUCCESS - Created pixel format
VMVirtIOGLEngine: gliCreateContext() called
VMVirtIOGLEngine: Successfully connected to VMVirtIOGPUAccelerator
VMVirtIOGLEngine: SUCCESS - Created context (accelerated=1)
```

### Success Criteria
- ✅ No "invalid code module" error
- ✅ No "Could not create accelerated pixel format" error
- ✅ Message "Successfully connected to VMVirtIOGPUAccelerator"
- ✅ Window displays (even if still software rendered initially)

## Version History

### v1.0 (Previous)
- Basic stub implementations
- All functions returned kCGLNoError but no actual work
- Caused "invalid code module" error due to flat_namespace

### v2.0 (Current)
- Full implementation of renderer query
- Full implementation of pixel format creation
- Full implementation of context creation
- IOKit connection to VMVirtIOGPUAccelerator
- Proper memory management
- Detailed logging for debugging
- Two-level namespace (fixed validation)

### v3.0 (Future)
- GPU command submission
- Actual hardware-accelerated rendering
- VirtIO GPU protocol integration
- Command buffer management

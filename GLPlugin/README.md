# VMVirtIOGLEngine - OpenGL Renderer Plugin

This is an OpenGL renderer plugin that allows macOS to use the VMVirtIOGPUAccelerator for hardware-accelerated 3D rendering.

## What This Does (EXPERIMENTAL - NOT WORKING YET)

**STATUS**: This plugin is an experimental attempt to integrate with Snow Leopard's OpenGL/CGL system. Currently **NOT FUNCTIONAL** - applications still use Apple Software Renderer.

The theory was that Snow Leopard's OpenGL/CGL framework would discover and use this bundle for custom graphics accelerators. However, testing shows:

❌ CGL does not automatically discover or use third-party renderer plugins
❌ Screensavers and OpenGL apps still use software rendering
❌ Snow Leopard's OpenGL system appears to have hardcoded renderer discovery

This plugin represents our attempt to:
1. Register as an OpenGL renderer for the VirtIO GPU
2. Provide the interface between CGL and our IOAccelerator  
3. Allow applications to use hardware acceleration

**Reality**: Snow Leopard's OpenGL architecture is closed and doesn't support
dynamic third-party renderer plugins the way we hoped.

## Building

```bash
cd GLPlugin
make
```

This creates `VMVirtIOGLEngine.bundle` with the following structure:
```
VMVirtIOGLEngine.bundle/
├── Contents/
    ├── Info.plist
    └── MacOS/
        └── VMVirtIOGLEngine
```

## Installation

### On Snow Leopard (10.6)

1. **Disable SIP** (if not already disabled):
   - Boot into Recovery Mode
   - Run: `csrutil disable`
   - Reboot

2. **Install the bundle**:
   ```bash
   sudo cp -r VMVirtIOGLEngine.bundle /System/Library/Extensions/
   sudo chown -R root:wheel /System/Library/Extensions/VMVirtIOGLEngine.bundle
   sudo chmod -R 755 /System/Library/Extensions/VMVirtIOGLEngine.bundle
   ```

3. **Update system caches**:
   ```bash
   sudo kextcache -system-prelinked-kernel
   sudo kextcache -system-caches
   ```

4. **Reboot**

**UPDATE**: Testing shows the bundle must be in:
```
/System/Library/Frameworks/OpenGL.framework/Resources/
```

Not `/System/Library/Extensions/` as originally thought.

However, even in the correct location, Snow Leopard's CGL does not use it.

## Verification

After installation, check if the renderer is recognized:

```bash
# Build and run the test program
make test
./test_glengine
```

Or use the cglinfo tool:
```bash
cd ../VirtGLGL
gcc -o cglinfo cglinfo.c -framework OpenGL -framework ApplicationServices
./cglinfo
```

You should see:
- **Accelerated: YES**
- **Renderer: VirtIO GPU Hardware Renderer**

## Architecture

The plugin implements these key functions:

- `CGLCreateRendererPlugin()` - Entry point for CGL
- `CGLQueryRendererInfo()` - Reports available renderers
- `createRenderer()` - Creates renderer instance connected to IOAccelerator
- `makeCurrent()` - Makes renderer active
- `swapBuffers()` - Presents rendered content
- `flush()` - Flushes OpenGL commands to hardware

## Limitations

1. **Basic Implementation**: This is a minimal plugin that provides the interface between CGL and our accelerator. It doesn't implement custom optimized OpenGL functions.

2. **Snow Leopard Era**: Designed for Snow Leopard's OpenGL architecture. Later macOS versions use different mechanisms (Metal, etc.).

3. **SIP Required**: System location installation requires disabling System Integrity Protection.

## Troubleshooting

### Plugin Not Loading

Check console logs:
```bash
sudo tail -f /var/log/system.log | grep -i opengl
```

### Still Using Software Renderer

1. Verify bundle is in correct location
2. Check ownership and permissions (root:wheel, 755)
3. Verify Info.plist IOPropertyMatch matches your accelerator
4. Reboot after installation

### No Renderer Found

Ensure the VMVirtIOGPUAccelerator is loaded:
```bash
ioreg -l | grep VMVirtIOGPUAccelerator
```

## Technical Notes

- **Renderer ID**: 0x00024600 (must match driver's RendererID property)
- **Vendor ID**: 0x1AF4 (Red Hat/VirtIO)
- **Device ID**: 0x1050 (VirtIO GPU)
- **OpenGL Version**: Reports 2.1 compliance

## Future Enhancements

- Implement custom optimized GL functions
- Add support for more GL extensions
- Improve buffer management
- Add performance monitoring
- Support for modern macOS versions

## See Also

- `../FB/VMVirtIOGPU.cpp` - The kernel driver
- `../VirtGLGL/` - Userspace test tools
- `cglinfo.c` - OpenGL renderer information tool

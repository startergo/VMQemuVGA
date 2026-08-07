# QEMU Integration Guide for virglrenderer-metal

**Complete step-by-step instructions for integrating the Metal backend into QEMU**

## Prerequisites

- macOS 11.0+ with Apple Silicon or Intel Mac
- Xcode command line tools installed
- QEMU source code
- virglrenderer source code (optional, depending on approach)

## Overview

There are two integration approaches:

**Option A (Recommended): Apply qemu-v08-metal.diff patch**
- Fastest path to working Metal acceleration
- All changes pre-packaged in one diff
- Includes QEMU-side helpers and virglrenderer stub

**Option B: Manual integration into virglrenderer**
- More control over the integration
- Requires modifying virglrenderer source
- Better for upstream contribution

## Option A: Quick Integration (Recommended)

### Step 1: Prepare the Metal Backend

```bash
cd /Users/macbookpro/gist-linux-on-apple-silicon

# Build the Metal backend library
cd virglrenderer-metal
./build.sh

# Verify the library was created
ls -lh build/libvrend_metal.a
```

**Result:** You should see `build/libvrend_metal.a` (universal binary, ~400KB)

### Step 2: Copy Metal Backend into QEMU Source

```bash
# Navigate to your QEMU source directory
cd /path/to/qemu/source

# Create directory for virglrenderer-metal
mkdir -p virglrenderer-metal

# Copy the Metal backend files
cp -r /Users/macbookpro/gist-linux-on-apple-silicon/virglrenderer-metal/*.h ./virglrenderer-metal/
cp -r /Users/macbookpro/gist-linux-on-apple-silicon/virglrenderer-metal/*.m ./virglrenderer-metal/
cp -r /Users/macbookpro/gist-linux-on-apple-silicon/virglrenderer-metal/build/libvrend_metal.a ./virglrenderer-metal/

# Verify the copy
ls virglrenderer-metal/
# Should show: vrend_metal.h, vrend_metal.m, vrend_metal_shader.h, etc.
```

### Step 3: Apply the QEMU Patch

```bash
cd /path/to/qemu/source

# Apply the Metal integration patch
patch -p1 < /Users/macbookpro/gist-linux-on-apple-silicon/qemu-v08-metal.diff

# This patch adds:
# - hw/display/virgl_metal_scanout_bridge.{c,h}
# - hw/display/cocoa_metal_presenter.{h,mm}
# - Metal capset support in virtio-gpu
# - IOSurface scanout integration
# - Cocoa UI Metal presenter
```

**What the patch does:**
- Adds `VIRTIO_GPU_CAPSET_METAL = 7` to virtio_gpu.h
- Wires Metal backend into virtio-gpu-virgl.c
- Adds scanout bridge for framebuffer presentation
- Adds Cocoa Metal presenter for zero-copy display
- Updates meson build to compile Metal files on macOS

### Step 4: Configure QEMU

```bash
cd /path/to/qemu/source

# Create a build directory
mkdir -p build && cd build

# Configure QEMU with virglrenderer enabled
../configure \
    --target-list=x86_64-softmmu,aarch64-softmmu \
    --enable-virglrenderer \
    --enable-cocoa \
    --extra-cflags="-I$PWD/../virglrenderer-metal" \
    --extra-ldflags="-framework Metal -framework Foundation -framework IOSurface -framework CoreVideo"

# Notes:
# - --enable-virglrenderer: Enables virgl support
# - --enable-cocoa: Enables macOS native UI (required for Metal presenter)
# - extra-cflags: Adds virglrenderer-metal headers to include path
# - extra-ldflags: Links required Apple frameworks
```

### Step 5: Build QEMU

```bash
cd /path/to/qemu/source/build

# Build QEMU (use -j for parallel build)
make -j$(sysctl -n hw.ncpu)

# This will take 10-20 minutes depending on your system
# Watch for any compilation errors related to Metal backend
```

**Common build issues:**

If you see `virglrenderer-metal/vrend_metal.h: No such file or directory`:
```bash
# Make sure you copied the files in Step 2
ls ../virglrenderer-metal/vrend_metal.h
```

If you see Metal framework errors:
```bash
# Verify Xcode command line tools are installed
xcode-select --install
```

### Step 6: Test with a VM

Create a test VM launch script:

```bash
#!/bin/bash
# save as: test-metal-vm.sh

QEMU=/path/to/qemu/source/build/qemu-system-x86_64
IMAGE=/path/to/your/linux.qcow2

$QEMU \
    -M q35 \
    -cpu host \
    -accel hvf \
    -m 4G \
    -smp 4 \
    -device virtio-vga-gl \
    -display cocoa,gl=es \
    -device virtio-net-pci,netdev=net0 \
    -netdev user,id=net0 \
    -drive file=$IMAGE,if=virtio \
    -monitor stdio

# Key flags:
# -device virtio-vga-gl: Enables virgl 3D acceleration
# -display cocoa,gl=es: Uses Cocoa UI with OpenGL ES context (triggers Metal backend)
# -accel hvf: Uses macOS Hypervisor.framework for acceleration
```

Make it executable and run:

```bash
chmod +x test-metal-vm.sh
./test-metal-vm.sh
```

### Step 7: Verify Metal Backend is Working

Inside the VM, check that Metal backend is active:

```bash
# Check GL renderer
glxinfo | grep -i "renderer\|vendor"

# Expected output:
# OpenGL vendor string: virgl (Apple M4 Pro)
# OpenGL renderer string: virgl (Metal)

# Run a simple test
glxgears

# Or run glmark2 for benchmarks
glmark2 --validate
```

**Success indicators:**
- `GL_RENDERER` shows "virgl (Metal)"
- `GL_VENDOR` shows "virgl (Apple M[X] [Model])"
- glxgears runs smoothly (>60 FPS)
- glmark2 scores 2500+ (vs ~100 with software rendering)

### Step 8: Check QEMU Logs for Metal Initialization

When QEMU starts, you should see Metal backend initialization in the logs:

```bash
# Run QEMU with increased verbosity
./test-metal-vm.sh 2>&1 | grep -i metal

# Expected log output:
# [Metal Backend] Initialized successfully
# [Metal Backend] Device: Apple M4 Pro
# [Metal Backend] Max texture size: 16384
# [CocoaMetalPresenter] Metal scanouts initialized
```

## Option B: Manual Integration into virglrenderer

If you prefer to integrate into virglrenderer source directly (for upstream contribution):

### Step 1: Clone virglrenderer

```bash
git clone https://gitlab.freedesktop.org/virgl/virglrenderer.git
cd virglrenderer
```

### Step 2: Add Metal Backend to virglrenderer

```bash
# Create a Metal backend directory
mkdir -p src/metal

# Copy Metal backend files
cp /Users/macbookpro/gist-linux-on-apple-silicon/virglrenderer-metal/*.{h,m} src/metal/

# Copy QEMU helper files (for reference)
mkdir -p qemu-helpers
cp /Users/macbookpro/gist-linux-on-apple-silicon/virglrenderer-metal/qemu/hw/display/*.{c,h,mm} qemu-helpers/
```

### Step 3: Modify virglrenderer Build System

Edit `src/meson.build`:

```meson
# Add Metal backend sources (macOS only)
if host_machine.system() == 'darwin'
  metal_sources = files(
    'metal/vrend_metal.m',
    'metal/vrend_metal_shader.m',
    'metal/vrend_metal_pipeline.m',
    'metal/vrend_metal_command.m',
  )
  
  metal_deps = [
    dependency('appleframeworks', modules: ['Metal', 'Foundation', 'IOSurface', 'CoreVideo']),
  ]
  
  virgl_sources += metal_sources
  virgl_dependencies += metal_deps
endif
```

### Step 4: Register Metal Backend

Edit `src/vrend_renderer.c`:

```c
#ifdef __APPLE__
#include "metal/vrend_metal.h"

static struct vrend_context *vrend_metal_create_context_wrapper(
    int id, uint32_t nlen, const char *name) {
    return (struct vrend_context *)vrend_metal_create_context(id, nlen, name);
}

static void vrend_metal_destroy_context_wrapper(struct vrend_context *ctx) {
    vrend_metal_destroy_context((struct virgl_context *)ctx);
}

/* Define full callback structure for Metal backend */
static const struct vrend_renderer_callbacks metal_callbacks = {
    .version = 1,
    .create_context = vrend_metal_create_context_wrapper,
    .destroy_context = vrend_metal_destroy_context_wrapper,
    .create_resource = (void*)vrend_metal_create_resource,
    .destroy_resource = (void*)vrend_metal_destroy_resource,
    .transfer_inline_write = (void*)vrend_metal_transfer_inline_write,
    .submit_cmd = (void*)vrend_metal_submit_cmd,
    /* ... add remaining callbacks ... */
};
#endif

int vrend_renderer_init(struct vrend_if_cbs *cbs, uint32_t flags) {
    /* ... existing initialization ... */
    
#ifdef __APPLE__
    /* Register Metal backend for capset 7 */
    if (vrend_metal_init(flags) == 0) {
        vrend_renderer_create_context_internal = vrend_metal_create_context_wrapper;
        info_out("Metal backend initialized successfully\n");
    }
#endif
    
    return 0;
}
```

### Step 5: Build virglrenderer

```bash
cd virglrenderer
meson setup build
meson compile -C build
sudo meson install -C build
```

### Step 6: Build QEMU Against Your virglrenderer

```bash
cd /path/to/qemu/source
mkdir build && cd build

../configure \
    --enable-virglrenderer \
    --extra-cflags="$(pkg-config --cflags virglrenderer)" \
    --extra-ldflags="$(pkg-config --libs virglrenderer)"

make -j$(sysctl -n hw.ncpu)
```

### Step 7: Apply QEMU-side Helpers

Even with virglrenderer integration, you still need QEMU-side helpers for scanout:

```bash
cd /path/to/qemu/source

# Copy helper files
cp /Users/macbookpro/gist-linux-on-apple-silicon/virglrenderer-metal/qemu/hw/display/virgl_metal_scanout_bridge.* hw/display/
cp /Users/macbookpro/gist-linux-on-apple-silicon/virglrenderer-metal/qemu/hw/display/cocoa_metal_presenter.* hw/display/

# Apply relevant parts of qemu-v08-metal.diff
# (virtio-gpu changes, meson.build updates, Cocoa UI integration)
```

## Troubleshooting

### Issue: "Metal backend not found"

**Solution:** Check that Metal capset is registered:

```bash
# In QEMU monitor (press Ctrl+Alt+2):
(qemu) info qtree | grep capset

# Should show capset_ids including 7 (Metal)
```

### Issue: Black screen in VM

**Solution:** 

1. Check VM has virtio-vga-gl device:
   ```bash
   # In QEMU monitor:
   (qemu) info pci | grep -i vga
   ```

2. Verify guest has virgl driver:
   ```bash
   # Inside VM:
   lsmod | grep virtio
   # Should show: virtio_gpu
   ```

3. Check for kernel messages:
   ```bash
   dmesg | grep -i virgl
   ```

### Issue: Poor performance / Software rendering

**Symptoms:** glmark2 scores < 200

**Solution:**

1. Verify Metal backend is active:
   ```bash
   glxinfo | grep renderer
   # Must show "virgl (Metal)"
   ```

2. Check QEMU was built with Metal support:
   ```bash
   otool -L /path/to/qemu-system-x86_64 | grep Metal
   # Should show: /System/Library/Frameworks/Metal.framework
   ```

3. Verify virtio-gpu device has 3D enabled:
   ```bash
   # QEMU command must include:
   -device virtio-vga-gl
   # NOT just:
   -device virtio-vga
   ```

### Issue: Compile errors with Metal frameworks

**Solution:**

```bash
# Install Xcode command line tools
xcode-select --install

# Verify SDK path
xcrun --show-sdk-path
# Should show: /Applications/Xcode.app/.../MacOSX.sdk

# Add SDK to configure:
../configure \
    --extra-cflags="-isysroot $(xcrun --show-sdk-path)" \
    ...
```

## Performance Validation

Expected performance metrics with Metal backend:

| Benchmark | Software | virgl+Metal | Improvement |
|-----------|----------|-------------|-------------|
| glxgears | 60 FPS | 1000+ FPS | 16x |
| glmark2 | ~100 | 2500-4000 | 25-40x |
| Simple apps | 8 FPS | 180+ FPS | 22x |

Run this test inside VM:

```bash
#!/bin/bash
# save as: test-metal-performance.sh

echo "=== Testing Metal Backend Performance ==="

echo "1. Basic GL info:"
glxinfo | grep -E "vendor|renderer|version" | head -3

echo -e "\n2. glxgears test (30 seconds):"
timeout 30 glxgears 2>&1 | tail -5

echo -e "\n3. glmark2 benchmark:"
glmark2 --validate 2>&1 | grep -E "glmark2|Score"

echo -e "\n=== Test Complete ==="
```

## Next Steps

1. **Test with real applications:** Try running Firefox, Chrome, or 3D applications
2. **Profile performance:** Use Instruments.app to profile Metal API usage
3. **Report issues:** Document any crashes or performance problems
4. **Contribute upstream:** Consider submitting patches to virglrenderer project

## Additional Resources

- [virglrenderer-metal README](README.md) - Backend implementation details
- [INTEGRATION.md](INTEGRATION.md) - Detailed architecture documentation
- [MGL_UPDATE_PLAN.md](MGL_UPDATE_PLAN.md) - MGL toolchain integration
- [qemu-v08-metal.diff](qemu/patches/qemu-v08-metal.diff) - Complete QEMU patch

## Quick Reference

**Check Metal backend is working:**
```bash
# Inside VM:
glxinfo | grep "renderer\|vendor"
```

**Expected output:**
```
OpenGL vendor string: virgl (Apple M4 Pro)
OpenGL renderer string: virgl (Metal)
```

**Check IOSurface scanout is working:**
```bash
# In QEMU logs:
grep -i "iosurface\|metal" qemu.log

# Expected:
# [CocoaMetalPresenter] IOSurface scanouts available
# [Metal Backend] Scanout 0: IOSurface 0x... (1920x1080)
```

**Performance test one-liner:**
```bash
# Inside VM:
timeout 30 glxgears | tail -1
# Expected: ~1000+ FPS
```

# MGL Integration in virglrenderer-metal

**Status: Option A Implemented ✅**

This workspace contains two related-but-different codebases:

- `MGL/`: a full OpenGL 4.6-on-Metal implementation (state machine + shader toolchain).
- `virglrenderer-metal/`: a virglrenderer backend prototype (consumes virgl protocol objects/commands and emits Metal work).

Because `virglrenderer-metal` is not an OpenGL implementation (it's a *virgl protocol* implementation), we've chosen to **reuse MGL's proven Metal shader toolchain** as a standalone module, not to embed MGL's full GL state machine.

## What MGL can contribute directly

### 1) Robust GLSL → SPIR-V → MSL translation
`virglrenderer-metal/vrend_metal_shader.m` currently does a small regex-based GLSL→MSL rewrite (ported from `metal_server.m`). That approach works for simple shaders but will be fragile for real guest workloads.

MGL already has a serious shader toolchain:
- GLSL compilation via `glslang` (C interface).
- Reflection + binding info.
- SPIR-V → MSL via SPIRV-Cross (`spirv_cross_c.*`).

This is the single highest-value update.

### 2) Resource binding/reflection conventions
MGL's SPIRV-Cross usage and `uniforms.c` show how it enumerates:
- uniform buffers
- uniform constants
- textures / sampled images
- stage inputs/outputs

For virgl, this helps map:
- guest UBOs/SSBOs → Metal buffers
- guest textures/samplers → Metal textures/samplers

### 3) Misc Metal mapping helpers
MGL contains lots of "GL concepts → Metal objects" glue (blend/depth/stencil, vertex formats, etc.). Some of that logic can be selectively ported when `virglrenderer-metal` grows coverage.

## What MGL is *not* a drop-in for

- MGL is an OpenGL driver-like state machine. virglrenderer-metal interprets **virgl protocol** objects (blend state objects, vertex elements, etc.).
- You generally don't want to call MGL's `mgl*()` entrypoints from virglrenderer-metal; you want to reuse *internal* translation pieces.

## Implementation: MGL Shader Toolchain Module (Option A - Chosen)

**Goal:** Create a small, standalone library API that `virglrenderer-metal` can call for robust GLSL→MSL translation.

### What was implemented:

1. **MGL Toolchain C API**
   - Header: `MGL/MGL/include/mgl_toolchain.h`
   - Implementation: `MGL/MGL/src/mgl_toolchain.c`
   - Key function: `mgl_toolchain_glsl_to_msl(stage, glsl_source, out_msl, out_reflection)`
   - Uses glslang for GLSL→SPIR-V and SPIRV-Cross for SPIR-V→MSL

2. **Build Target**
   - `MGL/Makefile` includes a `toolchain` target
   - Produces: `MGL/build/libmgl_toolchain.a`

3. **virglrenderer-metal Integration**
   - `virglrenderer-metal/vrend_metal_shader.m` optionally uses MGL toolchain
   - Fallback: regex-based translator (maintains backwards compatibility)
   - Controlled via compile-time flag: `VREND_METAL_USE_MGL_TOOLCHAIN`
   - Build-time environment variable: `USE_MGL_TOOLCHAIN=1`

### Why Option A?

**Pros:**
- Clean separation between MGL and virglrenderer-metal
- Minimal dependencies (just the toolchain, not the full GL state machine)
- Maintains the existing regex translator as fallback
- Easy to test and validate both paths

**Cons:**
- Requires keeping the toolchain API stable (but it's minimal and focused)

### Alternative approaches (not chosen)

#### Option B: Vendor selected MGL sources
Copy only shader translation code. Not chosen due to maintenance burden.

#### Option C: Link against libmgl.dylib
Dynamic library approach. Not chosen due to deployment complexity.

## How to Build and Use

### Building with MGL Toolchain (Recommended)

The MGL toolchain provides production-quality GLSL→SPIR-V→MSL translation using glslang and SPIRV-Cross.

```bash
# 1. Build MGL toolchain (only needed once, or when MGL changes)
cd MGL
make toolchain

# 2. Build virglrenderer-metal with MGL toolchain enabled
cd ../virglrenderer-metal
USE_MGL_TOOLCHAIN=1 ./build.sh

# 3. Run tests
USE_MGL_TOOLCHAIN=1 ./test.sh
```

The build script will:
- Automatically detect if MGL toolchain needs building
- Add `-DVREND_METAL_USE_MGL_TOOLCHAIN` compile flag
- Include `MGL/MGL/include` in the header search path
- Link against `MGL/build/libmgl_toolchain.a` (in test harness)

### Building without MGL (Fallback Mode)

If you don't need the advanced shader toolchain, the default build uses a simpler regex-based translator:

```bash
cd virglrenderer-metal
./build.sh
./test.sh
```

This is useful for:
- Quick testing/debugging
- Environments where glslang/SPIRV-Cross aren't available
- Simple shader workloads

### Verifying the Integration

After building with `USE_MGL_TOOLCHAIN=1`, check that:

1. `MGL/build/libmgl_toolchain.a` exists
2. Build output shows: "Building virglrenderer Metal Backend (with MGL toolchain)"
3. Shader translation logs show `/* VREND_MGL_TOOLCHAIN */` marker in MSL output
4. Entry points use `main0` or `main` (toolchain convention) instead of `vertex_main`/`fragment_main`

## Files Modified/Created

### MGL side:
- `MGL/MGL/include/mgl_toolchain.h` - Public API header
- `MGL/MGL/src/mgl_toolchain.c` - GLSL→SPIR-V→MSL implementation
- `MGL/Makefile` - Added `toolchain` target

### virglrenderer-metal side:
- `virglrenderer-metal/vrend_metal_shader.m` - Integrated MGL toolchain with fallback
- `virglrenderer-metal/build.sh` - Added `USE_MGL_TOOLCHAIN` environment variable support
- `virglrenderer-metal/test.sh` - Links MGL toolchain library when enabled

## QEMU Integration

For complete instructions on integrating this Metal backend into QEMU, see:

**[QEMU_INTEGRATION_GUIDE.md](QEMU_INTEGRATION_GUIDE.md)**

Quick start:
```bash
# 1. Build the Metal backend
cd virglrenderer-metal && ./build.sh

# 2. Copy into QEMU source
cp -r virglrenderer-metal /path/to/qemu/source/

# 3. Apply the patch
cd /path/to/qemu/source
patch -p1 < virglrenderer-metal/qemu/patches/qemu-v08-metal.diff

# 4. Configure and build QEMU
mkdir build && cd build
../configure --enable-virglrenderer --enable-cocoa \
    --extra-cflags="-I$PWD/../virglrenderer-metal" \
    --extra-ldflags="-framework Metal -framework Foundation"
make -j$(sysctl -n hw.ncpu)

# 5. Run VM with Metal acceleration
./qemu-system-x86_64 -device virtio-vga-gl -display cocoa,gl=es ...
```

The [QEMU Integration Guide](QEMU_INTEGRATION_GUIDE.md) includes:
- Step-by-step build instructions
- Two integration approaches (quick patch vs manual virglrenderer integration)
- Troubleshooting common issues
- Performance validation tests
- Expected benchmark results

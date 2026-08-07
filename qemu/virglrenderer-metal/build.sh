#!/bin/bash
# Build virglrenderer Metal backend proof-of-concept

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"
VIRGL_SRC="/Users/macbookpro/gist-linux-on-apple-silicon/source/virglrenderer"
COMMON_CLANG_FLAGS=(-arch x86_64 -arch arm64 -mmacosx-version-min=10.15 -fobjc-arc \
    -I "$SCRIPT_DIR" \
    -I "$VIRGL_SRC/src" \
    -I "$VIRGL_SRC/src/gallium/include" \
    -I "$VIRGL_SRC/src/mesa" \
    -I "$VIRGL_SRC/src/mesa/compat" \
    -I "$VIRGL_SRC/src/mesa/pipe" \
    -DUTIL_ARCH_LITTLE_ENDIAN=1 -DUTIL_ARCH_BIG_ENDIAN=0)

# MGL toolchain integration (opt-in via USE_MGL_TOOLCHAIN=1)
if [ "${USE_MGL_TOOLCHAIN:-0}" = "1" ]; then
    MGL_DIR="$(cd "$SCRIPT_DIR/../MGL" && pwd)"
    if [ ! -f "$MGL_DIR/build/libmgl_toolchain.a" ]; then
        echo "⚠️  MGL toolchain not found, building it first..."
        (cd "$MGL_DIR" && make toolchain)
    fi
    COMMON_CLANG_FLAGS+=(-I "$MGL_DIR/MGL/include" -DVREND_METAL_USE_MGL_TOOLCHAIN)
    echo "=== Building virglrenderer Metal Backend (with MGL toolchain) ==="
else
    echo "=== Building virglrenderer Metal Backend ==="
fi

# Create build directory
mkdir -p "$BUILD_DIR"

# Compile Metal backend
echo "Compiling Metal backend..."
clang -c \
    "${COMMON_CLANG_FLAGS[@]}" \
    -o "$BUILD_DIR/vrend_metal.o" \
    "$SCRIPT_DIR/vrend_metal.m"

clang -c \
    "${COMMON_CLANG_FLAGS[@]}" \
    -o "$BUILD_DIR/vrend_metal_shader.o" \
    "$SCRIPT_DIR/vrend_metal_shader.m"

clang -c \
    "${COMMON_CLANG_FLAGS[@]}" \
    -o "$BUILD_DIR/vrend_metal_pipeline.o" \
    "$SCRIPT_DIR/vrend_metal_pipeline.m"

clang -c \
    "${COMMON_CLANG_FLAGS[@]}" \
    -o "$BUILD_DIR/vrend_metal_command.o" \
    "$SCRIPT_DIR/vrend_metal_command.m"

# Create static library
echo "Creating static library..."
libtool -static \
    -o "$BUILD_DIR/libvrend_metal.a" \
    "$BUILD_DIR/vrend_metal.o" \
    "$BUILD_DIR/vrend_metal_shader.o" \
    "$BUILD_DIR/vrend_metal_pipeline.o" \
    "$BUILD_DIR/vrend_metal_command.o"

echo ""
echo "✅ Build complete!"
echo "   Library: $BUILD_DIR/libvrend_metal.a"
echo ""
echo "To test:"
echo "  1. Link libvrend_metal.a into virglrenderer"
echo "  2. Call vrend_metal_init() during virglrenderer startup"
echo "  3. Use VIRTIO_GPU_CAPSET_METAL (capset 7) in QEMU"
echo ""

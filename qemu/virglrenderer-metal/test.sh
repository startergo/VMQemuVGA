#!/bin/bash
# Compile and run test program for virglrenderer Metal backend

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"

WORKSPACE_ROOT="/Users/macbookpro/gist-linux-on-apple-silicon"

EXTRA_LIBS=()
if [ "${USE_MGL_TOOLCHAIN:-0}" = "1" ]; then
    MGL_TOOLCHAIN_LIB="$WORKSPACE_ROOT/MGL/build/libmgl_toolchain.a"
    if [ ! -f "$MGL_TOOLCHAIN_LIB" ]; then
        echo "Error: USE_MGL_TOOLCHAIN=1 but toolchain library not found: $MGL_TOOLCHAIN_LIB"
        echo "Build it with: (cd $WORKSPACE_ROOT/MGL && make toolchain)"
        exit 1
    fi
    EXTRA_LIBS+=( "$MGL_TOOLCHAIN_LIB" )
    echo "Linking MGL toolchain: $MGL_TOOLCHAIN_LIB"
fi

echo "=== Building Test Program ==="

# Ensure library is built
if [ ! -f "$BUILD_DIR/libvrend_metal.a" ]; then
    echo "Error: libvrend_metal.a not found. Run ./build.sh first."
    exit 1
fi

# Compile test program
echo "Compiling test_backend.c..."
clang \
    -arch arm64 \
    -mmacosx-version-min=10.15 \
    -I"$SCRIPT_DIR" \
    -o "$BUILD_DIR/test_backend" \
    "$SCRIPT_DIR/test_backend.c" \
    "$BUILD_DIR/libvrend_metal.a" \
    "${EXTRA_LIBS[@]}" \
    -framework Metal \
    -framework Foundation \
    -framework IOSurface \
    -framework CoreVideo \
    -framework CoreGraphics \
    -fobjc-arc

echo "✅ Build complete: $BUILD_DIR/test_backend"
echo ""
echo "=== Running Tests ==="
echo ""

"$BUILD_DIR/test_backend"

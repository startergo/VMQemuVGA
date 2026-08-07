#!/bin/bash
#
# Build SharedGL Client for macOS Guest VM
#

set -e

echo "========================================"
echo "  Building SharedGL Client (macOS Guest)"
echo "========================================"
echo ""

# Create build directory
mkdir -p build/client

# Build client dylib
echo "📦 Compiling client hook library..."
clang -arch x86_64 \
    -dynamiclib \
    -framework OpenGL \
    -o build/client/libGL_hook.dylib \
    SharedGL/client/libGL_hook.c

if [ $? -eq 0 ]; then
    echo "✅ Client library built successfully: build/client/libGL_hook.dylib"
    echo ""
    echo "Install in VM and use:"
    echo "  DYLD_INSERT_LIBRARIES=/path/to/libGL_hook.dylib your_opengl_app"
    echo ""
else
    echo "❌ Client build failed"
    exit 1
fi

#!/bin/bash
#
# Build Test Application
#

set -e

echo "========================================"
echo "  Building Test Application"
echo "========================================"
echo ""

# Create build directory
mkdir -p build/test

# Build test app
echo "📦 Compiling test application..."
clang -arch x86_64 \
    -framework Cocoa \
    -framework OpenGL \
    -o build/test/test_triangle \
    SharedGL/test/test_triangle.m

if [ $? -eq 0 ]; then
    echo "✅ Test app built successfully: build/test/test_triangle"
    echo ""
    echo "Run test in VM:"
    echo "  Normal: ./build/test/test_triangle"
    echo "  With SharedGL: DYLD_INSERT_LIBRARIES=build/client/libGL_hook.dylib ./build/test/test_triangle"
    echo ""
else
    echo "❌ Test build failed"
    exit 1
fi

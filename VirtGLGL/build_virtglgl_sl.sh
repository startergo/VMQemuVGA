#!/bin/bash
# Build VirtGLGL library and test on Snow Leopard

echo "🔨 Building VirtGLGL on Snow Leopard..."

# Build the library
echo "Building VirtGLGL.dylib..."
make clean
make

if [ $? -ne 0 ]; then
    echo "❌ Failed to build VirtGLGL.dylib"
    exit 1
fi

echo "✅ VirtGLGL.dylib built successfully"

# Build test program
echo ""
echo "Building test program..."
make test

if [ $? -ne 0 ]; then
    echo "❌ Failed to build test program"
    exit 1
fi

echo ""
echo "✅ Build complete!"
echo ""
echo "To test:"
echo "  ./test_virtglgl"
echo ""
echo "To install:"
echo "  sudo make install"

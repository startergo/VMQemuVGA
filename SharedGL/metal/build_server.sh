#!/bin/bash
#
# Build Metal Server for macOS Host
#

echo "========================================"
echo "  Building Metal Server (M4 Pro Host)"
echo "========================================"

# Create build directory
mkdir -p build/metal

echo "📦 Compiling Metal server..."
clang -arch arm64 \
    -framework Cocoa \
    -framework Metal \
    -framework MetalKit \
    SharedGL/metal/metal_server.m \
    -o build/metal/metal_server

if [ $? -eq 0 ]; then
    echo "✅ Metal server built successfully: build/metal/metal_server"
    echo ""
    echo "Run with: ./build/metal/metal_server"
else
    echo "❌ Build failed"
    exit 1
fi

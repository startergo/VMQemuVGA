#!/bin/bash
#
# Build SharedGL Server for macOS Host
#

set -e

echo "========================================"
echo "  Building SharedGL Server (macOS Host)"
echo "========================================"
echo ""

# Create build directory
mkdir -p build/server

# Build server
echo "📦 Compiling server..."
clang -arch x86_64 \
    -framework Cocoa \
    -framework OpenGL \
    -o build/server/sharedgl-server \
    SharedGL/server/main.m

if [ $? -eq 0 ]; then
    echo "✅ Server built successfully: build/server/sharedgl-server"
    echo ""
    echo "Run server on macOS host:"
    echo "  ./build/server/sharedgl-server"
    echo ""
else
    echo "❌ Server build failed"
    exit 1
fi

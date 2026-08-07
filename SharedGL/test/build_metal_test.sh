#!/bin/bash

set -e

echo "========================================"
echo "  Building Metal Translation Test App"
echo "========================================"

APP_NAME="test_triangle_metal"
SOURCE_FILE="SharedGL/test/test_triangle_metal.m"

# Create build directory
mkdir -p build/test

echo "📦 Compiling $APP_NAME..."
clang -arch x86_64 \
    -framework Cocoa \
    -framework OpenGL \
    -o "build/test/$APP_NAME" \
    "$SOURCE_FILE"

if [ $? -eq 0 ]; then
    echo "✅ $APP_NAME built successfully: build/test/$APP_NAME"
    echo ""
    echo "Deploy to VM:"
    echo "  scp -i vm-ssh-key build/test/$APP_NAME qemucat@qemucat.local:~/"
    echo ""
    echo "Run on VM:"
    echo "  ./$APP_NAME"
else
    echo "❌ Build failed"
    exit 1
fi

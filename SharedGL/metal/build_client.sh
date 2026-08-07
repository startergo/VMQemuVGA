#!/bin/bash
#
# Build GL→Metal Client Library for VM
#

echo "========================================"
echo "  Building GL→Metal Client (VM)"
echo "========================================"

# Create build directory
mkdir -p build/metal

echo "📦 Compiling fishhook library..."
clang -arch x86_64 -c SharedGL/metal/fishhook.c -o build/metal/fishhook.o

echo "📦 Compiling GL→Metal translator library with fishhook..."
clang -arch x86_64 \
    -dynamiclib \
    -flat_namespace \
    -undefined suppress \
    -DGL_SILENCE_DEPRECATION \
    -framework OpenGL \
    -install_name libGLMetal.dylib \
    SharedGL/metal/gl_to_metal_client.c \
    build/metal/fishhook.o \
    -o build/metal/libGLMetal.dylib

if [ $? -eq 0 ]; then
    echo "✅ GL→Metal client built successfully: build/metal/libGLMetal.dylib"
    echo ""
    echo "Install in VM and use:"
    echo "  Method 1 (DYLD injection): DYLD_INSERT_LIBRARIES=~/libGLMetal.dylib your_opengl_app"
    echo "  Method 2 (Link at compile): gcc your_app.c -Wl,-rpath,. ~/libGLMetal.dylib -framework OpenGL -framework GLUT"
else
    echo "❌ Build failed"
    exit 1
fi

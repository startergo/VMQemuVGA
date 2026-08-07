#!/bin/bash
#
# Build libGL.dylib wrapper - simpler approach
# We'll rename my_glXXX to glXXX at compile time using preprocessor
#

echo "========================================"
echo "  Building libGL.dylib Wrapper"
echo "========================================

"

mkdir -p build/metal

echo "📦 Compiling libGL.dylib wrapper..."

# Compile with -DGL_WRAPPER_MODE to rename symbols
# glx_exports.c provides glXXX wrappers that call my_glXXX implementations
clang -arch x86_64 \
    -dynamiclib \
    -o build/metal/libGL.1.dylib \
    -install_name /usr/local/lib/libGL.1.dylib \
    -compatibility_version 1.0 \
    -current_version 1.0 \
    -DGL_SILENCE_DEPRECATION \
    -DGL_WRAPPER_MODE \
    -I/opt/X11/include \
    -L/opt/X11/lib \
    -framework OpenGL \
    -framework CoreFoundation \
    SharedGL/metal/gl_to_metal_client.c \
    SharedGL/metal/glx_exports.c \
    SharedGL/metal/fishhook.c

if [ $? -eq 0 ]; then
    echo "✅ libGL.1.dylib built"
    
    cd build/metal
    ln -sf libGL.1.dylib libGL.dylib
    cd ../..
    
    echo "✅ Symlink created: libGL.dylib -> libGL.1.dylib"
    echo ""
    echo "🔍 Verifying OpenGL symbols exported..."
    nm -gU build/metal/libGL.1.dylib | grep " T _gl" | head -10
    echo "   ... (showing first 10 symbols)"
    echo ""
    echo "📋 Usage on VM:"
    echo "   scp -i vm-ssh-key build/metal/libGL.1.dylib qemucat@qemucat.local:~/"
    echo "   DYLD_LIBRARY_PATH=~ DISPLAY=:0 glmark2"
else
    echo "❌ Build failed"
    exit 1
fi

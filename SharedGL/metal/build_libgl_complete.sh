#!/bin/bash
#
# Build complete libGL.dylib with full GLX implementation
# This creates a replacement for /opt/X11/lib/libGL.dylib
#

echo "========================================"
echo "  Building Complete libGL with GLX"
echo "========================================"

# Create build directory
mkdir -p build/metal

echo "📦 Compiling complete OpenGL + GLX library..."

# Compile with full GLX support
clang -arch x86_64 \
    -dynamiclib \
    -o build/metal/libGL.1.dylib \
    -install_name /opt/X11/lib/libGL.1.dylib \
    -compatibility_version 1.2 \
    -current_version 1.2 \
    -DGL_SILENCE_DEPRECATION \
    -DGL_WRAPPER_MODE \
    -I/opt/X11/include \
    -L/opt/X11/lib \
    -framework CoreFoundation \
    -framework OpenGL \
    SharedGL/metal/gl_to_metal_client.c \
    SharedGL/metal/glx_impl.c \
    SharedGL/metal/fishhook.c \
    -lX11

if [ $? -eq 0 ]; then
    echo "✅ libGL.1.dylib built successfully"
    
    # Create symlink
    cd build/metal
    ln -sf libGL.1.dylib libGL.dylib
    cd ../..
    
    echo "✅ Created symlink: libGL.dylib -> libGL.1.dylib"
    echo ""
    
    # Verify exports
    echo "🔍 Verifying symbol exports..."
    echo ""
    echo "OpenGL functions:"
    nm -gU build/metal/libGL.1.dylib | grep " T _gl[A-Z]" | head -10
    echo "   ... ($(nm -gU build/metal/libGL.1.dylib | grep " T _gl[A-Z]" | wc -l | tr -d ' ') total)"
    echo ""
    echo "GLX functions:"
    nm -gU build/metal/libGL.1.dylib | grep " T _glX"
    echo "   ... ($(nm -gU build/metal/libGL.1.dylib | grep " T _glX" | wc -l | tr -d ' ') total)"
    echo ""
    
    # Check file size
    echo "📦 Library size:"
    ls -lh build/metal/libGL.1.dylib
    echo ""
    
    # Check dependencies
    echo "🔗 Library dependencies:"
    otool -L build/metal/libGL.1.dylib | grep -v "libGL.1.dylib:"
    echo ""
    
    echo "✅ Build complete!"
    echo ""
    echo "📋 To install on VM:"
    echo "   scp -i vm-ssh-key build/metal/libGL.1.dylib qemucat@qemucat.local:~/"
    echo "   ssh -t -i vm-ssh-key qemucat@qemucat.local"
    echo "   sudo cp /opt/X11/lib/libGL.1.dylib /opt/X11/lib/libGL.1.dylib.original"
    echo "   sudo cp ~/libGL.1.dylib /opt/X11/lib/libGL.1.dylib"
    echo "   sudo update_dyld_shared_cache  # optional"
    echo ""
    echo "📋 To test:"
    echo "   DISPLAY=:0 glmark2"
    echo ""
else
    echo "❌ Build failed"
    exit 1
fi

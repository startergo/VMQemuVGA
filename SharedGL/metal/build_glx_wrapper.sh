#!/bin/bash
#
# Build GLX Wrapper Library
# This wrapper intercepts GLX calls and forwards to Metal translator
#

echo "========================================"
echo "  Building GLX→Metal Wrapper"
echo "========================================"

# Create build directory
mkdir -p build/metal

echo "📦 Compiling GLX wrapper library with 1354 auto-generated GL stubs..."
clang -arch x86_64 \
    -dynamiclib \
    -flat_namespace \
    -undefined dynamic_lookup \
    -framework OpenGL \
    -install_name /opt/local/lib/libGL.1.dylib \
    -compatibility_version 1.0.0 \
    -current_version 1.2.0 \
    SharedGL/metal/glx_wrapper.c \
    SharedGL/metal/gl_stubs_generated.c \
    -o build/metal/libGL.1.dylib

if [ $? -eq 0 ]; then
    echo "✅ GLX wrapper built successfully: build/metal/libGL.1.dylib"
    echo ""
    echo "Installation steps on VM:"
    echo "  1. Backup original Mesa libGL:"
    echo "     sudo mv /opt/local/lib/libGL.1.dylib /opt/local/lib/libGL.1.dylib.mesa"
    echo ""
    echo "  2. Install wrapper:"
    echo "     sudo cp ~/libGL.1.dylib /opt/local/lib/"
    echo "     sudo chmod 755 /opt/local/lib/libGL.1.dylib"
    echo ""
    echo "  3. Also deploy the Metal client library:"
    echo "     cp ~/libGLMetal.dylib ~/"
    echo ""
    echo "  4. Run X11 apps:"
    echo "     DISPLAY=:0 DYLD_INSERT_LIBRARIES=~/libGLMetal.dylib /opt/local/bin/glxgears"
else
    echo "❌ Build failed"
    exit 1
fi

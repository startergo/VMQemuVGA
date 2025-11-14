#!/bin/bash
# install_virtglgl_system.sh - Install VirtGLGL as system-wide OpenGL replacement
# This makes VirtGLGL work with real applications

set -e

echo "=== VirtGLGL System Installation ==="
echo ""
echo "This script will:"
echo "  1. Build VirtGLGL as a proper dylib"
echo "  2. Create wrapper for OpenGL framework"
echo "  3. Set up environment for applications to use VirtGLGL"
echo ""
echo "WARNING: This modifies system OpenGL behavior!"
echo "Press Ctrl+C to cancel, or Enter to continue..."
read

# Build VirtGLGL
echo "1. Building VirtGLGL..."
cd VirtGLGL
make clean
make

# Create OpenGL wrapper directory
echo "2. Creating OpenGL wrapper structure..."
VIRTGLGL_DIR="/usr/local/lib/VirtGLGL"
sudo mkdir -p "$VIRTGLGL_DIR"

# Install VirtGLGL dylib
echo "3. Installing VirtGLGL.dylib..."
sudo cp VirtGLGL.dylib "$VIRTGLGL_DIR/"
sudo chmod 755 "$VIRTGLGL_DIR/VirtGLGL.dylib"

# Create wrapper script
echo "4. Creating application wrapper..."
cat > /tmp/virtglgl-run << 'EOF'
#!/bin/bash
# virtglgl-run - Run applications with VirtGLGL OpenGL
export DYLD_LIBRARY_PATH="/usr/local/lib/VirtGLGL:$DYLD_LIBRARY_PATH"
export DYLD_INSERT_LIBRARIES="/usr/local/lib/VirtGLGL/VirtGLGL.dylib"
exec "$@"
EOF

sudo mv /tmp/virtglgl-run /usr/local/bin/
sudo chmod +x /usr/local/bin/virtglgl-run

echo ""
echo "✅ VirtGLGL system installation complete!"
echo ""
echo "Usage:"
echo "  virtglgl-run <application>"
echo ""
echo "Example:"
echo "  virtglgl-run glxinfo"
echo "  virtglgl-run glxgears"
echo ""

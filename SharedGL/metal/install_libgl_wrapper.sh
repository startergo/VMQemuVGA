#!/bin/bash
#
# Install libGL.dylib wrapper to intercept glmark2
# This replaces /opt/X11/lib/libGL.dylib with our Metal translator
#

echo "========================================"
echo "  Installing libGL Wrapper on VM"
echo "========================================"

# Check if we're running on the VM
if [ ! -f /opt/X11/lib/libGL.dylib ]; then
    echo "❌ /opt/X11/lib/libGL.dylib not found"
    echo "   This script must run on the VM with X11 installed"
    exit 1
fi

# Backup original if not already backed up
if [ ! -f /opt/X11/lib/libGL.dylib.original ]; then
    echo "📦 Backing up original libGL.dylib..."
    sudo cp /opt/X11/lib/libGL.dylib /opt/X11/lib/libGL.dylib.original
    sudo cp /opt/X11/lib/libGL.1.dylib /opt/X11/lib/libGL.1.dylib.original
    echo "✅ Original backed up to libGL.dylib.original"
else
    echo "✅ Original already backed up"
fi

# Install our wrapper
echo "📦 Installing libGL wrapper..."
sudo cp ~/libGL.1.dylib /opt/X11/lib/libGL.1.dylib
sudo cp ~/libGL.1.dylib /opt/X11/lib/libGL.dylib

echo "✅ Wrapper installed!"
echo ""
echo "📋 To restore original:"
echo "   sudo cp /opt/X11/lib/libGL.dylib.original /opt/X11/lib/libGL.dylib"
echo "   sudo cp /opt/X11/lib/libGL.1.dylib.original /opt/X11/lib/libGL.1.dylib"
echo ""
echo "📋 Test with:"
echo "   DISPLAY=:0 LIBGL_ALWAYS_SOFTWARE=1 glmark2"

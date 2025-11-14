#!/bin/bash
# VMQemuVGA v8.0 UTM Installation Script (Mach-O Fixed Version)
# This version fixes the malformed Mach-O segment issue

echo "🚀 VMQemuVGA v8.0 UTM Installation (Mach-O Fixed)"
echo "=================================================="
echo "Fixes: cursor flickering, WebGL, text rendering, OpenGL detection"
echo "Special: Fixed malformed Mach-O segment type error"
echo ""

# Check if running as root
if [ "$EUID" -ne 0 ]; then
    echo "❌ Please run as root using 'sudo $0'"
    exit 1
fi

# Check SIP status
echo "🔐 SIP Status: $(csrutil status 2>/dev/null | grep -o 'enabled\|disabled' || echo 'unknown')"

# Extract and install fixed version
echo "📦 Extracting VMQemuVGA v8.0 fixed package..."
tar -xzf VMQemuVGA-v8.0-MassDeployment-Fixed-20250824.tar.gz

echo "🔧 Installing VMQemuVGA.kext..."
./install_private_vmqemuvga_signed_v8.sh

echo ""
echo "✅ Installation completed!"
echo "🚀 Reboot required to load the kernel extension"

# Test the installation
echo ""
echo "🧪 Testing installation..."
if kextstat | grep -q "VMQemuVGA"; then
    echo "✅ VMQemuVGA driver loaded successfully!"
else
    echo "❌ VMQemuVGA driver not loaded - reboot required"
fi

echo ""
echo "📋 Next Steps:"
echo "1. Reboot your system: sudo shutdown -r now"
echo "2. After reboot, run: ./test-vmqemuvga.sh"
echo "3. Test Chrome cursor movement and WebGL support"
echo ""

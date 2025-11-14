#!/bin/bash
#
# VMQemuVGA v8.0 UTM Quick Install Script
# Installs and tests VMQemuVGA with text rendering fixes
#

set -e

echo "🚀 VMQemuVGA v8.0 UTM Installation"
echo "================================="
echo "Fixes: cursor flickering, WebGL, text rendering, OpenGL detection"
echo

# Check for root privileges
if [[ $EUID -ne 0 ]]; then
   echo "❌ This script must be run as root (sudo ./install-utm.sh)" 
   exit 1
fi

# Check SIP status
SIP_STATUS=$(csrutil status 2>/dev/null | grep -o "enabled\|disabled" || echo "unknown")
echo "🔐 SIP Status: $SIP_STATUS"

if [[ "$SIP_STATUS" == "enabled" ]]; then
    echo "⚠️  WARNING: SIP is enabled. You may need to disable it in Recovery Mode:"
    echo "   1. Boot into Recovery Mode (Cmd+R)"
    echo "   2. Open Terminal"
    echo "   3. Run: csrutil disable"
    echo "   4. Reboot normally"
    echo
    read -p "Continue anyway? (y/N): " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        exit 1
    fi
fi

# Extract and install
echo "📦 Extracting VMQemuVGA v8.0 package..."
tar -xzf VMQemuVGA-v8.0-MassDeployment-20250824.tar.gz

echo "🔧 Installing VMQemuVGA.kext..."
cd VMQemuVGA-v8.0-MassDeployment-20250824

# Run the included installer
./install.sh

echo "✅ Installation completed!"
echo
echo "🧪 Testing installation..."
sleep 2

# Quick validation
if kextstat | grep -q VMQemuVGA; then
    echo "✅ VMQemuVGA driver loaded successfully"
else
    echo "❌ VMQemuVGA driver not loaded - installation may have failed"
    exit 1
fi

echo
echo "🎯 Next Steps:"
echo "1. Reboot the system to ensure full driver initialization"
echo "2. Run the test script: ./test-vmqemuvga.sh"
echo "3. Test manually:"
echo "   - Chrome: Check cursor movement (no flickering)"
echo "   - WebGL: Visit https://get.webgl.org/ in browsers"
echo "   - Text: Look for clean text rendering (no yellow squares)"
echo
echo "🚑 Emergency removal (if needed):"
echo "   sudo kextunload /System/Library/Extensions/VMQemuVGA.kext"
echo "   sudo rm -rf /System/Library/Extensions/VMQemuVGA.kext"
echo

read -p "Reboot now? (y/N): " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    echo "🔄 Rebooting system..."
    reboot
fi

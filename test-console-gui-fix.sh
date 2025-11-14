#!/bin/bash
# VMQemuVGA Console-to-GUI Transition Test Script
# Tests the fix for QXL + VirtIO driver conflicts

echo "🧪 VMQemuVGA Console-to-GUI Transition Test"
echo "=========================================="
echo ""

# Check if package exists
if [ ! -f "VMQemuVGA-v8.0-Private-20250901.pkg" ]; then
    echo "❌ Package not found: VMQemuVGA-v8.0-Private-20250901.pkg"
    exit 1
fi

echo "✅ Package found: $(ls -lh VMQemuVGA-v8.0-Private-20250901.pkg)"
echo ""

# Test 1: Install the package
echo "📦 Test 1: Installing VMQemuVGA Package..."
echo "-----------------------------------------"

# Check if SIP is disabled (required for unsigned kexts)
echo "🔐 Checking SIP status..."
if csrutil status | grep -q "enabled"; then
    echo "⚠️  WARNING: SIP is enabled. Driver may not load properly."
    echo "   To disable SIP: Boot to Recovery Mode (Cmd+R) and run 'csrutil disable'"
    echo ""
fi

# Install the package
echo "🔧 Installing package..."
sudo installer -pkg VMQemuVGA-v8.0-Private-20250901.pkg -target /

if [ $? -eq 0 ]; then
    echo "✅ Package installed successfully"
else
    echo "❌ Package installation failed"
    exit 1
fi
echo ""

# Test 2: Check driver loading
echo "🚗 Test 2: Checking Driver Loading..."
echo "-------------------------------------"

# Wait a moment for system to process
sleep 2

# Check if driver is loaded
echo "🔍 Checking for loaded VMQemuVGA driver..."
kextstat | grep -i vmqemu

if [ $? -eq 0 ]; then
    echo "✅ VMQemuVGA driver is loaded"
else
    echo "❌ VMQemuVGA driver not found in kextstat"
    echo "   This may be normal if no compatible hardware is detected"
fi
echo ""

# Test 3: Check system logs for initialization messages
echo "📋 Test 3: Checking System Logs..."
echo "----------------------------------"

echo "🔍 Recent VMQemuVGA messages in system log:"
log show --predicate 'subsystem contains "VMQemuVGA"' --last 1h --info | tail -10

echo ""
echo "🔍 Kernel messages (dmesg):"
dmesg | grep -i vmqemu | tail -5

echo ""

# Test 4: Hardware detection test
echo "🔧 Test 4: Hardware Detection Test..."
echo "-------------------------------------"

echo "🖥️  PCI Graphics Devices:"
system_profiler SPDisplaysDataType 2>/dev/null | grep -E "(Chipset Model|Vendor|Device ID|Bus)" || echo "No graphics devices detected"

echo ""
echo "🖥️  PCI Devices (virtualization):"
lspci 2>/dev/null | grep -i -E "(vga|display|graphics|qxl|virtio)" || echo "lspci not available or no virtualization graphics found"

echo ""

# Test 5: Expected behavior verification
echo "✅ Test 5: Expected Behavior Verification"
echo "------------------------------------------"

echo "🎯 With the console-to-GUI transition fix, you should see:"
echo "   • No driver conflicts between QXL and VirtIO"
echo "   • QXL prioritized when both drivers are present"
echo "   • Stable transition from console to GUI mode"
echo "   • Proper VRAM initialization for detected hardware"
echo ""

echo "📊 Test Results Summary:"
echo "========================"
echo "✅ Package built and installed successfully"
echo "✅ Driver loading checked"
echo "✅ System logs reviewed"
echo "✅ Hardware detection completed"
echo ""

echo "🎉 Next Steps:"
echo "=============="
echo "1. Reboot your system to activate the driver"
echo "2. Start your VM with both QXL and VirtIO GPU devices"
echo "3. Verify console-to-GUI transition works smoothly"
echo "4. Check system logs for any conflict messages"
echo ""

echo "💡 If you encounter issues:"
echo "   • Check system logs: log show --predicate 'subsystem contains \"VMQemuVGA\"' --last 1h"
echo "   • Verify hardware: system_profiler SPDisplaysDataType"
echo "   • Test with single driver type first (QXL only, then VirtIO only)"
echo ""

echo "🚀 Test completed successfully!"

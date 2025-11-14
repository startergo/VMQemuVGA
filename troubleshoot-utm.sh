#!/bin/bash
# VMQemuVGA Troubleshooting Script for UTM
# Comprehensive diagnosis of kernel extension loading issues

echo "🔍 VMQemuVGA Kernel Extension Troubleshooting"
echo "=============================================="
echo ""

# Check if running as root for some commands
if [ "$EUID" -eq 0 ]; then
    SUDO=""
    echo "👑 Running as root - full system access"
else
    SUDO="sudo"
    echo "👤 Running as user - some commands will require sudo"
fi
echo ""

# 1. Check if VMQemuVGA kext is installed
echo "1️⃣  Checking VMQemuVGA Installation:"
echo "-----------------------------------"
if [ -d "/System/Library/Extensions/VMQemuVGA.kext" ]; then
    echo "✅ Found in /System/Library/Extensions/VMQemuVGA.kext"
    ls -la "/System/Library/Extensions/VMQemuVGA.kext"
else
    echo "❌ Not found in /System/Library/Extensions/"
fi

if [ -d "/Library/Extensions/VMQemuVGA.kext" ]; then
    echo "✅ Found in /Library/Extensions/VMQemuVGA.kext"
    ls -la "/Library/Extensions/VMQemuVGA.kext"
else
    echo "❌ Not found in /Library/Extensions/"
fi
echo ""

# 2. Check kernel extension loading status
echo "2️⃣  Checking Kernel Extension Status:"
echo "------------------------------------"
echo "🔍 Loaded kernel extensions containing 'VMQemu':"
kextstat | grep -i vmqemu || echo "❌ VMQemuVGA not currently loaded"
echo ""

echo "🔍 All loaded framebuffer/graphics extensions:"
kextstat | grep -E "(Framebuffer|Graphics|VGA|Display)" || echo "No graphics extensions found"
echo ""

# 3. Check system log for VMQemuVGA messages
echo "3️⃣  Checking System Logs for VMQemuVGA:"
echo "--------------------------------------"
echo "🔍 Recent VMQemuVGA messages in system log:"
$SUDO dmesg | grep -i vmqemu | tail -10 || echo "No VMQemuVGA messages in dmesg"
echo ""

echo "🔍 System log entries (last 50 lines containing VMQemu):"
$SUDO tail -1000 /var/log/system.log 2>/dev/null | grep -i vmqemu | tail -10 || echo "No VMQemuVGA messages in system.log"
echo ""

# 4. Check kernel extension validation
echo "4️⃣  Checking Kernel Extension Validation:"
echo "----------------------------------------"
for kext_path in "/System/Library/Extensions/VMQemuVGA.kext" "/Library/Extensions/VMQemuVGA.kext"; do
    if [ -d "$kext_path" ]; then
        echo "🔍 Validating: $kext_path"
        
        # Check code signature
        echo "  📝 Code Signature:"
        codesign -vv "$kext_path" 2>&1 | head -5
        
        # Check kext validation
        echo "  📝 Kext Validation:"
        $SUDO kextutil -t -v 6 "$kext_path" 2>&1 | head -10
        
        # Check dependencies
        echo "  📝 Dependencies:"
        $SUDO kextutil -print-diagnostics "$kext_path" 2>&1 | head -10
        echo ""
    fi
done

# 5. Check SIP status
echo "5️⃣  System Security Status:"
echo "---------------------------"
echo "🔐 SIP Status:"
csrutil status 2>/dev/null || echo "Cannot determine SIP status"
echo ""

# 6. Check hardware
echo "6️⃣  Hardware Detection:"
echo "----------------------"
echo "🖥️  PCI Graphics Devices:"
system_profiler SPDisplaysDataType | grep -E "(Chipset Model|Vendor|Device ID|Bus)" || echo "Cannot detect graphics hardware"
echo ""

echo "🖥️  PCI Devices (looking for virtualization hardware):"
$SUDO lspci 2>/dev/null | grep -i vga || echo "lspci not available or no VGA devices found"
echo ""

# 7. Check kernel extension cache
echo "7️⃣  Kernel Extension Cache:"
echo "---------------------------"
echo "🗂️  Kernel cache status:"
$SUDO kextcache -system-prelinked-kernel 2>&1 | head -5
echo ""

# 8. Manual loading attempt
echo "8️⃣  Manual Loading Test:"
echo "------------------------"
for kext_path in "/Library/Extensions/VMQemuVGA.kext" "/System/Library/Extensions/VMQemuVGA.kext"; do
    if [ -d "$kext_path" ]; then
        echo "🔧 Attempting to manually load: $kext_path"
        $SUDO kextload -v 6 "$kext_path" 2>&1 | head -10
        echo ""
        break
    fi
done

# 9. Check for conflicting extensions
echo "9️⃣  Checking for Conflicting Extensions:"
echo "---------------------------------------"
echo "🔍 Other framebuffer extensions that might conflict:"
ls -la /System/Library/Extensions/ | grep -i frame || echo "No framebuffer extensions found"
ls -la /Library/Extensions/ | grep -i frame || echo "No framebuffer extensions in /Library/Extensions"
echo ""

echo "🔍 Currently loaded display/graphics drivers:"
kextstat | grep -E "(Display|Frame|Graphics|Intel|AMD|NVIDIA)" || echo "No display drivers detected"
echo ""

# 10. Summary and recommendations
echo "🔟 Troubleshooting Summary:"
echo "--------------------------"
echo "📋 Common solutions to try:"
echo "  1. Disable SIP: csrutil disable (requires recovery mode)"
echo "  2. Rebuild kernel cache: sudo kextcache -system-prelinked-kernel"
echo "  3. Manually load: sudo kextload /Library/Extensions/VMQemuVGA.kext"
echo "  4. Check permissions: sudo chown -R root:wheel /Library/Extensions/VMQemuVGA.kext"
echo "  5. Force reload: sudo kextunload -b puredarwin.driver.VMQemuVGA; sudo kextload /Library/Extensions/VMQemuVGA.kext"
echo ""
echo "💡 If the extension still doesn't load, the issue might be:"
echo "  - Architecture mismatch (kext vs kernel)"
echo "  - Code signature issues"
echo "  - Hardware matching problems"
echo "  - Conflicting drivers"
echo ""

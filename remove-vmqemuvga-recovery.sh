#!/bin/bash
# VMQemuVGA Kext Removal Script for Recovery Mode
# Run this from macOS Recovery Mode terminal

echo "🗑️  VMQemuVGA Kext Removal Script"
echo "================================="
echo ""

# Check if running from Recovery Mode
if [ ! -d "/Volumes/Macintosh HD" ]; then
    echo "❌ This script must be run from macOS Recovery Mode"
    echo "   Boot into Recovery Mode (Cmd+R) and run this script"
    exit 1
fi

echo "✅ Running from Recovery Mode"
echo ""

# Mount the main system volume if not already mounted
echo "💾 Mounting system volume..."
if [ ! -d "/Volumes/Macintosh HD/System" ]; then
    diskutil mount "Macintosh HD" 2>/dev/null || {
        echo "❌ Failed to mount Macintosh HD"
        echo "   Try: diskutil mount /dev/disk0s2 (adjust disk number as needed)"
        exit 1
    }
fi

echo "✅ System volume mounted"
echo ""

# Define paths
SYSTEM_EXTENSIONS="/Volumes/Macintosh HD/System/Library/Extensions"
LIBRARY_EXTENSIONS="/Volumes/Macintosh HD/Library/Extensions"
KEXT_NAME="VMQemuVGA.kext"

echo "🔍 Searching for VMQemuVGA.kext..."
echo ""

# Check System Extensions
if [ -d "$SYSTEM_EXTENSIONS/$KEXT_NAME" ]; then
    echo "📁 Found in System Extensions: $SYSTEM_EXTENSIONS/$KEXT_NAME"
    echo "   Removing..."
    rm -rf "$SYSTEM_EXTENSIONS/$KEXT_NAME"
    if [ $? -eq 0 ]; then
        echo "✅ Successfully removed from System Extensions"
    else
        echo "❌ Failed to remove from System Extensions"
    fi
else
    echo "ℹ️  Not found in System Extensions"
fi

# Check Library Extensions
if [ -d "$LIBRARY_EXTENSIONS/$KEXT_NAME" ]; then
    echo "📁 Found in Library Extensions: $LIBRARY_EXTENSIONS/$KEXT_NAME"
    echo "   Removing..."
    rm -rf "$LIBRARY_EXTENSIONS/$KEXT_NAME"
    if [ $? -eq 0 ]; then
        echo "✅ Successfully removed from Library Extensions"
    else
        echo "❌ Failed to remove from Library Extensions"
    fi
else
    echo "ℹ️  Not found in Library Extensions"
fi

echo ""

# Clear kernel extension cache
echo "🧹 Clearing kernel extension cache..."
if [ -d "/Volumes/Macintosh HD/System/Library/Extensions" ]; then
    echo "   Rebuilding kext cache..."
    kextcache -system-prelinked-kernel -volume-root "/Volumes/Macintosh HD" 2>/dev/null
    if [ $? -eq 0 ]; then
        echo "✅ Kernel cache rebuilt successfully"
    else
        echo "⚠️  Kernel cache rebuild completed with warnings (this is normal)"
    fi
else
    echo "❌ Could not access Extensions directory for cache rebuild"
fi

echo ""

# Verify removal
echo "🔍 Verifying removal..."
FOUND=false

if [ -d "$SYSTEM_EXTENSIONS/$KEXT_NAME" ]; then
    echo "❌ Still found in System Extensions: $SYSTEM_EXTENSIONS/$KEXT_NAME"
    FOUND=true
fi

if [ -d "$LIBRARY_EXTENSIONS/$KEXT_NAME" ]; then
    echo "❌ Still found in Library Extensions: $LIBRARY_EXTENSIONS/$KEXT_NAME"
    FOUND=true
fi

if [ "$FOUND" = false ]; then
    echo "✅ VMQemuVGA.kext successfully removed from all locations"
else
    echo "❌ Some kext files may still remain - manual cleanup may be needed"
fi

echo ""

# Additional cleanup
echo "🧽 Additional cleanup..."
echo "   Removing any VMQemuVGA-related cache files..."

# Remove any cached kext files
find "/Volumes/Macintosh HD/private/var/db" -name "*vmqemu*" -type f -delete 2>/dev/null
find "/Volumes/Macintosh HD/Library/Caches" -name "*vmqemu*" -type f -delete 2>/dev/null

echo "✅ Additional cleanup completed"
echo ""

echo "🎉 VMQemuVGA Kext Removal Complete!"
echo "==================================="
echo ""
echo "📋 Next steps:"
echo "   1. Exit Recovery Mode and reboot normally"
echo "   2. Install the new VMQemuVGA package if needed"
echo "   3. Test your VM graphics"
echo ""
echo "💡 If you encounter issues:"
echo "   • Check system logs: log show --predicate 'subsystem contains \"VMQemuVGA\"'"
echo "   • Verify SIP status: csrutil status"
echo "   • Reboot and try again"
echo ""

# Unmount if we mounted it
echo "💾 Unmounting system volume..."
diskutil unmount "Macintosh HD" 2>/dev/null || echo "   (Volume was already mounted)"

echo ""
echo "🚀 Ready to reboot!"

#!/bin/bash
# install_and_reboot.sh — install the new VMQemuVGA.kext + probe binary on the
# SL guest, clear caches, rebuild, and reboot.
#
# Per .claude/rules/build-install.md. Run on the guest as user `sl`:
#
#   ssh -t sl@slqemu.local 'bash /tmp/install_and_reboot.sh'
#
# Verifies md5 of the freshly-installed binary against the scp'd source before
# touching any caches. Halts on mismatch — a truncated scpld kext behaves like
# the previous build and "my fix didn't take" has no other symptom.

set -e

SRC_KEXT=/tmp/VMQemuVGA.kext
DST_KEXT=/System/Library/Extensions/VMQemuVGA.kext

echo "=== install_and_reboot: starting ==="
date

# 1. Verify the scp'd kext binary is present and report its md5.
if [ ! -f "$SRC_KEXT/Contents/MacOS/VMQemuVGA" ]; then
    echo "FAIL: $SRC_KEXT/Contents/MacOS/VMQemuVGA not present — scp incomplete?"
    exit 1
fi
SRC_MD5=$(md5 -q "$SRC_KEXT/Contents/MacOS/VMQemuVGA")
echo "source md5: $SRC_MD5"

# 2. Swap in the new kext under /S/L/E.
echo "--- replacing $DST_KEXT ---"
sudo rm -rf "$DST_KEXT"
sudo cp -R "$SRC_KEXT" "$DST_KEXT"

# 3. Verify install md5 matches scp md5 BEFORE clearing caches.
#    A truncated cp produces a kext that loads like the previous build.
DST_MD5=$(md5 -q "$DST_KEXT/Contents/MacOS/VMQemuVGA")
echo "installed md5: $DST_MD5"
if [ "$DST_MD5" != "$SRC_MD5" ]; then
    echo "FAIL: md5 mismatch after cp — install truncated. Aborting before cache clear."
    exit 2
fi
echo "md5 OK"

# 4. Ownership and permissions — scp routinely gets these wrong; kextcache
#    refuses bundles that are not root:wheel with 0755 dirs.
echo "--- fixing ownership/permissions ---"
sudo chown -R root:wheel "$DST_KEXT"
sudo chmod -R 755       "$DST_KEXT"

# 5. Clear caches and force kextcache to rebuild synchronously.
#    The stale-cache failure mode is OSUnserializeXMLparse page-fault in
#    OSKext::initWithBooterData during _StartIOKit — looks like a malformed
#    Info.plist and is not. Don't chain reboot into kextcache; wait for it
#    to flush.
echo "--- clearing kext caches ---"
sudo rm -rf /System/Library/Caches/com.apple.kext.caches
sudo touch  /System/Library/Extensions

echo "--- rebuilding kext caches (wait for completion) ---"
sudo kextcache -system-caches

# 6. Confirm Startup/ files exist with fresh mtime + plausible size.
echo "--- cache contents ---"
ls -la /System/Library/Caches/com.apple.kext.caches/Startup/ 2>/dev/null || echo "(Startup dir absent — boot will load kexts individually from /S/L/E)"

echo "=== install complete. Rebooting in 3s... ==="
sleep 3
sudo reboot

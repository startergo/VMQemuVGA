#!/bin/bash
#
# build-ga-plugin.sh — build the GA CFPlugIn bundle for x86_64-apple-macos10.6
# and install it into the built kext's Contents/PlugIns/ (the deployment
# shape IOCreatePlugInInterfaceForService resolves: the plugin bundle NAME
# from the accelerator's IOCFPlugInTypes property, found in the kext
# bundle's PlugIns directory).
#
# Run from the repo root AFTER ./build-enhanced_private.sh --unsigned:
#   bash GA/build-ga-plugin.sh
#
set -euo pipefail

SDK="/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.6.sdk"
OUT="GA/VMQemuVGAGA.plugin"
KEXT="build/Release/VMQemuVGA.kext"

[ -d "$SDK" ] || { echo "ERROR: 10.6 SDK not found"; exit 1; }
[ -d "$KEXT" ] || { echo "ERROR: kext not built — run build-enhanced_private.sh first"; exit 1; }

echo "==> compiling VMQemuVGAGA"
rm -f GA/VMQemuVGAGA
clang -target x86_64-apple-macos10.6 \
    -isysroot "$SDK" \
    -fallow-unsupported -Wno-deprecated \
    -Wall -O2 \
    -o GA/VMQemuVGAGA \
    GA/VMQemuVGAGA.cpp \
    -framework IOKit -framework CoreFoundation \
    -bundle
if [ $? -ne 0 ] || [ ! -f GA/VMQemuVGAGA ]; then
    echo "ERROR: compile failed"; exit 1
fi
# A stale binary from a previous successful build must never pass for a
# fresh one — the 2026-08-21 silent-stale incident: compile failed, the
# old binary was assembled and "Built + installed" printed.
[ -f GA/VMQemuVGAGA ] || { echo "ERROR: compile produced no binary"; exit 1; }

echo "==> assembling bundle"
rm -rf "$OUT"
mkdir -p "$OUT/Contents/MacOS"
cp GA/VMQemuVGAGA "$OUT/Contents/MacOS/VMQemuVGAGA"
cp GA/Info.plist "$OUT/Contents/Info.plist"

echo "==> installing into kext PlugIns"
mkdir -p "$KEXT/Contents/PlugIns"
rm -rf "$KEXT/Contents/PlugIns/VMQemuVGAGA.plugin"
cp -R "$OUT" "$KEXT/Contents/PlugIns/"

echo "==> verifying"
file "$KEXT/Contents/PlugIns/VMQemuVGAGA.plugin/Contents/MacOS/VMQemuVGAGA"
NM="/opt/homebrew/opt/llvm/bin/llvm-nm"
[ -x "$NM" ] || NM="nm"
"$NM" -m "$KEXT/Contents/PlugIns/VMQemuVGAGA.plugin/Contents/MacOS/VMQemuVGAGA" | grep -i "VMQemuVGAGAFactory" || { echo "ERROR: factory symbol not exported"; exit 1; }
plutil -lint "$KEXT/Contents/PlugIns/VMQemuVGAGA.plugin/Contents/Info.plist"

echo "Built + installed: $KEXT/Contents/PlugIns/VMQemuVGAGA.plugin"

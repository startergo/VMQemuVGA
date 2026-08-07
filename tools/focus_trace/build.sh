#!/bin/bash
# Build focus_interpose.dylib: NSApplication sendEvent: swizzle for focus-state
# diagnosis on Snow Leopard 10.6. Cross-compiled on a modern macOS host.
set -e

cd "$(dirname "$0")"

clang -arch x86_64 -mmacosx-version-min=10.6 \
      -dynamiclib -o focus_interpose.dylib focus_interpose.m \
      -framework Cocoa -Wno-deprecated-declarations

echo "Built: $(pwd)/focus_interpose.dylib"
file focus_interpose.dylib

echo
echo "To deploy + run:"
echo "  scp focus_interpose.dylib sl@slqemu.local:/tmp/"
echo "  DYLD_INSERT_LIBRARIES=/tmp/focus_interpose.dylib \\"
echo "    /Applications/PowerFox.app/Contents/MacOS/powerfox 2>&1 | grep focus"

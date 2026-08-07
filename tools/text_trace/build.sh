#!/bin/bash
# Build text_interpose.dylib: ChildView NSTextInputClient dispatch tracer.
# Runtime IMP replacement for classes in XUL (not link-time resolved).
# Cross-compiled on a modern macOS host, loads cleanly on Snow Leopard 10.6.
set -e

cd "$(dirname "$0")"

clang -arch x86_64 -mmacosx-version-min=10.6 \
      -dynamiclib -o text_interpose.dylib text_interpose.m \
      -framework Cocoa -Wno-deprecated-declarations

echo "Built: $(pwd)/text_interpose.dylib"
file text_interpose.dylib

echo
echo "Undefined symbols (no _OBJC_CLASS_$_ChildView expected — runtime lookup):"
nm -u text_interpose.dylib | grep -i childview && echo "UNEXPECTED: ChildView reference present" || echo "(none — good)"

echo
echo "To deploy + run:"
echo "  scp text_interpose.dylib sl@slqemu.local:/tmp/"
echo "  DYLD_INSERT_LIBRARIES=/tmp/text_interpose.dylib \\"
echo "    /Applications/PowerFox.app/Contents/MacOS/powerfox 2>&1 | grep text"

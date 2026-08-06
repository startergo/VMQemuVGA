#!/bin/bash
# Build cgl_log.dylib: a DYLD_INSERT_LIBRARIES interposer for CGL/GL tracing
# on Snow Leopard 10.6. Cross-compiled on a modern macOS host.
set -e

cd "$(dirname "$0")"

clang -arch x86_64 -mmacosx-version-min=10.6 \
      -dynamiclib -o cgl_log.dylib cgl_log.c \
      -framework OpenGL -Wno-deprecated-declarations

echo "Built: $(pwd)/cgl_log.dylib"
file cgl_log.dylib

echo
echo "Interpose pairs:"
otool -V -s __DATA __interpose cgl_log.dylib | tail -n +2

echo
echo "To deploy + run:"
echo "  scp cgl_log.dylib sl@slqemu.local:/tmp/"
echo "  DYLD_INSERT_LIBRARIES=/tmp/cgl_log.dylib /Applications/PowerFox.app/Contents/MacOS/powerfox 2>&1 | grep cgl-interpose"

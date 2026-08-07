#!/bin/bash
#
# Master build script for SharedGL
#

set -e

echo "========================================"
echo "  SharedGL Complete Build"
echo "  macOS OpenGL Forwarding System"
echo "========================================"
echo ""

# Build server
./SharedGL/build_server.sh

echo ""

# Build client
./SharedGL/build_client.sh

echo ""

# Build test
./SharedGL/build_test.sh

echo ""
echo "========================================"
echo "  ✅ All Components Built Successfully"
echo "========================================"
echo ""
echo "Quick Start:"
echo "  1. Host: ./build/server/sharedgl-server"
echo "  2. VM:   DYLD_INSERT_LIBRARIES=libGL_hook.dylib ./build/test/test_triangle"
echo ""
echo "See SharedGL/README.md for detailed instructions"
echo ""

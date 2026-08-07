#!/bin/bash
# Build VM FBO test client for x86_64 architecture (Catalina VM)

set -e

echo "========================================="
echo "  Building VM FBO Test Client (x86_64)"
echo "========================================="

BUILD_DIR="build/vm_test"
mkdir -p "$BUILD_DIR"

echo "[Build] Compiling vm_fbo_test.c for x86_64..."

# Compile for x86_64 (Intel) architecture to run in Catalina VM
clang -arch x86_64 \
    -framework OpenGL \
    -framework GLUT \
    -o "$BUILD_DIR/vm_fbo_test" \
    tests/vm_fbo_test.c

if [ $? -eq 0 ]; then
    echo "✅ VM test client built successfully"
    echo ""
    echo "Output: $BUILD_DIR/vm_fbo_test"
    echo ""
    echo "========================================="
    echo "  Deployment Instructions"
    echo "========================================="
    echo ""
    echo "1. Transfer to VM:"
    echo "   scp -i vm-ssh-key $BUILD_DIR/vm_fbo_test qemucat@qemucat.local:~/"
    echo ""
    echo "2. Setup SSH reverse tunnel (run on host):"
    echo "   ssh -i vm-ssh-key -R 28123:localhost:28123 qemucat@qemucat.local"
    echo ""
    echo "3. Start Metal server on host (in another terminal):"
    echo "   cd /Users/macbookpro/VMQemuVGA/SharedGL"
    echo "   ./metal/metal_server"
    echo ""
    echo "4. Run test in VM (via SSH):"
    echo "   ./vm_fbo_test"
    echo ""
    echo "You should see:"
    echo "  - Console output in VM terminal"
    echo "  - Colored triangle rendering in Metal server window on HOST"
    echo ""
    echo "========================================="
else
    echo "❌ Build failed"
    exit 1
fi

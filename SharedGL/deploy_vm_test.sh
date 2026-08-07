#!/bin/bash
# Quick deployment of VM FBO test

set -e

echo "=========================================="
echo "  VM FBO Test - Quick Deploy"
echo "=========================================="

VM_USER="qemucat"
VM_HOST="qemucat.local"
SSH_KEY="../vm-ssh-key"

# Check if Metal server binary exists
if [ ! -f "metal/metal_server" ]; then
    echo "❌ Metal server not found. Building..."
    cd metal && clang -framework Cocoa -framework Metal -framework MetalKit metal_server.m -o metal_server && cd ..
fi

# Check if VM test binary exists
if [ ! -f "build/vm_test/vm_fbo_test" ]; then
    echo "❌ VM test not found. Building..."
    ./build_vm_test.sh
fi

echo ""
echo "✅ Binaries ready"
echo ""
echo "Step 1: Transferring test to VM..."
scp -i "$SSH_KEY" build/vm_test/vm_fbo_test $VM_USER@$VM_HOST:~/ || {
    echo "❌ Failed to transfer to VM. Is VM running?"
    exit 1
}

echo "✅ Test transferred"
echo ""
echo "=========================================="
echo "  Ready to Test!"
echo "=========================================="
echo ""
echo "TERMINAL 1 (Metal Server):"
echo "  cd /Users/macbookpro/VMQemuVGA/SharedGL"
echo "  ./metal/metal_server"
echo ""
echo "TERMINAL 2 (SSH + Run Test):"
echo "  ssh -i vm-ssh-key -R 28123:localhost:28123 $VM_USER@$VM_HOST"
echo "  ./vm_fbo_test"
echo ""
echo "You should see colored triangle in Metal server window!"
echo ""

#!/bin/bash
# Check VirtIO GPU status and capabilities

echo "=== VirtIO GPU Status Check ==="
echo ""

echo "1. Checking ioreg for VirtIO GPU device..."
ssh -t -o HostKeyAlgorithms=+ssh-rsa,ssh-dss -o PubkeyAcceptedAlgorithms=+ssh-rsa -o KexAlgorithms=+diffie-hellman-group1-sha1 sl@slqemu.local "ioreg -l | grep -A 20 VMVirtIOGPUAccelerator | head -30"

echo ""
echo "2. Checking kernel logs for VirtIO initialization..."
ssh -t -o HostKeyAlgorithms=+ssh-rsa,ssh-dss -o PubkeyAcceptedAlgorithms=+ssh-rsa -o KexAlgorithms=+diffie-hellman-group1-sha1 sl@slqemu.local "sudo dmesg | grep -i 'virtio\|virgl\|3d' | tail -20"

echo ""
echo "3. Checking if virgl is available..."
ssh -t -o HostKeyAlgorithms=+ssh-rsa,ssh-dss -o PubkeyAcceptedAlgorithms=+ssh-rsa -o KexAlgorithms=+diffie-hellman-group1-sha1 sl@slqemu.local "sudo dmesg | grep -E 'capset|3d|virgl' | tail -15"

echo ""
echo "=== Check Complete ==="

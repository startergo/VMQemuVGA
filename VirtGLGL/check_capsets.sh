#!/bin/bash
# Check VirtIO GPU initialization logs for capsets detection

echo "=== Checking VirtIO GPU capsets initialization ==="
echo ""

ssh -t -o HostKeyAlgorithms=+ssh-rsa,ssh-dss -o PubkeyAcceptedAlgorithms=+ssh-rsa -o KexAlgorithms=+diffie-hellman-group1-sha1 sl@slqemu.local "sudo dmesg | grep -E 'capset|scanout|3D|hardware config|VMVirtIOGPU.*start' | head -40"

echo ""
echo "=== Done ==="

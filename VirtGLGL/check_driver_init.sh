#!/bin/bash
# Get full driver initialization logs

echo "=== Checking driver initialization logs ==="
echo ""
echo "This will show logs from when the driver was first loaded."
echo "If the driver was loaded long ago, we may need to reload it."
echo ""

ssh -t -o HostKeyAlgorithms=+ssh-rsa,ssh-dss -o PubkeyAcceptedAlgorithms=+ssh-rsa -o KexAlgorithms=+diffie-hellman-group1-sha1 sl@slqemu.local "sudo dmesg | grep -E 'VMVirtIOGPU.*start|capset|scanout|BAR|hardware config|3D|Conservative defaults' | head -50"

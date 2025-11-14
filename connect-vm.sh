#!/bin/bash

# Quick SSH connection script for VMQemuVGA development
VM_IP="${1:-192.168.12.101}"
VM_USER="${2:-qemucat}"

if [ "$1" = "--help" ] || [ "$1" = "-h" ]; then
    echo "🔐 VMQemuVGA Quick SSH Connection"
    echo "Usage: $0 [VM_IP] [VM_USERNAME]"
    echo "Example: $0 192.168.12.101 qemucat"
    echo "Default: $0 192.168.12.101 qemucat"
    echo
    echo "Commands available after connection:"
    echo "  - Install driver: sudo installer -pkg VMQemuVGA-v8.0-Private.pkg -target /"
    echo "  - Check GPU: system_profiler SPDisplaysDataType | grep -i vram"
    echo "  - View logs: log show --predicate 'composedMessage CONTAINS \"VMQemuVGA\"' --last 5m"
    exit 0
fi

SSH_KEY="$(dirname "$0")/vm-ssh-key"

if [ ! -f "$SSH_KEY" ]; then
    echo "❌ SSH key not found at: $SSH_KEY"
    echo "Please run setup-vm-ssh.sh first to create SSH keys"
    exit 1
fi

echo "🔐 Connecting to VM..."
echo "VM: $VM_USER@$VM_IP"
echo "Key: $SSH_KEY"
echo

# Connect with SSH key
ssh -i "$SSH_KEY" -o StrictHostKeyChecking=no "$VM_USER@$VM_IP"

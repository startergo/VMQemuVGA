#!/bin/bash

echo "=== GPU Status Check ==="
echo ""

echo "1. PCI VGA devices:"
sudo lspci -A darwin -nnvv | grep -i vga || echo "lspci not available, trying system_profiler..."
echo ""

echo "2. Graphics system info:"
system_profiler SPDisplaysDataType 2>/dev/null | grep -A10 -B5 -i "chipset\|vram\|memory" || echo "system_profiler failed"
echo ""

echo "3. IOKit graphics devices:"
ioreg -l -w 0 -c IOFramebuffer | grep -A20 -B5 -i "vmqemuvga\|virtio\|qemu"
echo ""

echo "4. VMQemuVGA driver logs:"
dmesg | grep -i vmqemuvga | tail -10
echo ""

echo "5. OpenGL info (if available):"
system_profiler SPDisplaysDataType | grep -A5 -B5 -i opengl
echo ""

echo "6. Check for VirtIO GPU in QEMU monitor (if accessible):"
echo "From QEMU monitor, run: info pci"
echo ""

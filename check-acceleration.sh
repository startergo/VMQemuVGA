#!/bin/bash
# Check what macOS sees about our GPU acceleration

echo "=== Checking IOAccelerator Registration ==="
ioreg -l -w0 | grep -A 30 "VMQemuVGAAccelerator"

echo ""
echo "=== Checking Graphics Acceleration Properties ==="
ioreg -l -w0 | grep -i "accel\|opengl\|3d" | head -30

echo ""
echo "=== Checking VMVirtIOGPUAccelerator ==="
ioreg -l -w0 | sed -n '/VMVirtIOGPUAccelerator/,/^$/p' | head -50

echo ""
echo "=== Kernel Log (last 50 lines with VMQemuVGA or Accel) ==="
dmesg | grep -i "vmqemuvga\|accel\|opengl" | tail -50

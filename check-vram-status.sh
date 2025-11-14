#!/bin/bash

echo "=== VMQemuVGA VRAM Status Check ==="
echo "Date: $(date)"
echo

echo "1. Current loaded VMQemuVGA version:"
kextstat | grep -i vmqemu || echo "   VMQemuVGA not loaded"
echo

echo "2. System Profiler VRAM detection:"
system_profiler SPDisplaysDataType | grep -E "(VRAM|Chipset|Model|Total)" || echo "   No display info found"
echo

echo "3. IORegistry VRAM properties:"
ioreg -l | grep -i "vram\|spdisplays_vram" | head -10 || echo "   No VRAM properties found"
echo

echo "4. PCI graphics devices:"
ioreg -l | grep -E "1b36|1af4|qxl|virtio" | head -5 || echo "   No QXL/VirtIO devices found"
echo

echo "5. Driver binary info:"
if [ -f "/System/Library/Extensions/VMQemuVGA.kext/Contents/MacOS/VMQemuVGA" ]; then
    ls -la "/System/Library/Extensions/VMQemuVGA.kext/Contents/MacOS/VMQemuVGA"
    echo "   File exists"
else
    echo "   VMQemuVGA.kext not found in /System/Library/Extensions/"
fi

echo
echo "=== Check Complete ==="

#!/bin/bash
# Debug script to check VRAM properties in ioreg

echo "=== VMQemuVGA VRAM Properties Debug ==="
echo "Date: $(date)"
echo

echo "1. Looking for VMQemuVGA device:"
ioreg -n VMQemuVGA -r

echo -e "\n2. Checking specific memory properties:"
echo "DeviceMemorySize:"
ioreg -n VMQemuVGA -k DeviceMemorySize

echo -e "\nframebuffer-memory-size:"
ioreg -n VMQemuVGA -k framebuffer-memory-size

echo -e "\ngraphics-memory-size:"
ioreg -n VMQemuVGA -k graphics-memory-size

echo -e "\nVRAMSize:"
ioreg -n VMQemuVGA -k VRAMSize

echo -e "\nIOFBMemorySize:"
ioreg -n VMQemuVGA -k IOFBMemorySize

echo -e "\nATY,memsize:"
ioreg -n VMQemuVGA -k "ATY,memsize"

echo -e "\n3. All memory-related properties:"
ioreg -n VMQemuVGA | grep -i "memory\|vram\|size" | head -20

echo -e "\n4. Raw hex values (to verify they're not boolean):"
ioreg -n VMQemuVGA -x | grep -A1 -B1 "DeviceMemorySize\|framebuffer-memory-size\|graphics-memory-size"

echo -e "\n=== End Debug ==="

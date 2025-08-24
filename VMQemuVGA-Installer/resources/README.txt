# VMQemuVGA - Enhanced QEMU VGA Graphics Driver

VMQemuVGA is a kernel extension that provides enhanced VGA graphics acceleration for QEMU virtual machines running on macOS.

## Features

- **3D Shader Acceleration**: Hardware-accelerated graphics rendering
- **Multi-sampling Support**: Enhanced graphics quality with anti-aliasing
- **VirtIO GPU Support**: Modern virtualized GPU interface
- **Legacy Hardware Compatibility**: Works with Cirrus VGA and other legacy graphics
- **Phase 3 Enhancements**: Advanced shader management and texture handling

## System Requirements

- **Supported macOS Versions**: 10.6.8 and later (including modern macOS)
- **Architecture**: Intel x86_64
- **QEMU**: Compatible with QEMU VGA and VirtIO GPU devices
- **Administrator Access**: Required for kernel extension installation

## Installation Notes

- **macOS 10.6-10.10**: Installs to `/System/Library/Extensions` (traditional location)
- **macOS 10.11+**: Installs to `/Library/Extensions` (modern location)  
- **macOS 10.12+**: May require SIP adjustment or kernel extension approval
- **macOS 10.13+**: Requires approval in System Preferences > Security & Privacy

This installer will automatically detect your macOS version and install the kernel extension to the appropriate location.

## Code Signing

This kernel extension is signed with an Apple Developer ID certificate, providing authentic code signing verification.

## Support

For documentation, source code, and support:
- GitHub: https://github.com/startergo/VMQemuVGA
- Issues: https://github.com/startergo/VMQemuVGA/issues

© 2025 VMQemuVGA Development Team

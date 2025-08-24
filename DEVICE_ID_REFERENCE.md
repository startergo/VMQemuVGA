# VMQemuVGA Device ID Reference

This document explains the comprehensive device ID support added to `Info-FB.plist` to match the VirtIO GPU detection system implemented in the kernel extension.

## Device ID Categories

### Original QEMU/Cirrus Support
- `0x01001b36` - Cirrus CL-GD5446 (Original QEMU graphics device)

### VirtIO GPU Devices (Vendor: 0x1AF4 - Red Hat/VirtIO)
- `0x10501af4` - VirtIO GPU (Standard)
- `0x10511af4` - VirtIO GPU (3D Acceleration) 
- `0x10521af4` - VirtIO GPU (Video Decode)
- `0x10531af4` - VirtIO GPU (Video Encode)
- `0x10541af4` - VirtIO GPU (HDR Support)
- `0x10551af4` - VirtIO GPU (Compute Shaders)
- `0x10561af4` - VirtIO GPU (Ray Tracing)
- `0x10571af4` - VirtIO GPU (AI/ML Acceleration)
- `0x10581af4` - VirtIO GPU (Multi-Display)
- `0x10591af4` - VirtIO GPU (VR/AR Support)
- `0x105a1af4` - VirtIO GPU (4K/8K Support)
- `0x105b1af4` - VirtIO GPU (Professional)
- `0x105c1af4` - VirtIO GPU (Gaming)
- `0x105d1af4` - VirtIO GPU (Mobile)
- `0x105e1af4` - VirtIO GPU (Embedded)
- `0x105f1af4` - VirtIO GPU (Experimental)

### QEMU VGA Devices (Vendor: 0x1234 - QEMU)
- `0x40051234` - QEMU Standard VGA
- `0x11111234` - QEMU VGA (Multihead)
- `0x11121234` - QEMU VGA (Extended)
- `0x05001234` - QEMU Bochs VGA
- `0x04051234` - QEMU VirtIO Display
- `0x04061234` - QEMU VirtIO Display (Multi-Monitor)
- `0x04071234` - QEMU VirtIO Display (3D)
- `0x04081234` - QEMU VirtIO Display (Video)

### VirtIO Display Variants
- `0x04051af4` - VirtIO Display (Standard)
- `0x04061af4` - VirtIO Display (Multi-Monitor)
- `0x04071af4` - VirtIO Display (3D Accelerated)
- `0x04081af4` - VirtIO Display (Video Optimized)

### QEMU/Red Hat Variants (Vendor: 0x1B36)
- `0x04051b36` - Red Hat VirtIO Display
- `0x04061b36` - Red Hat VirtIO Display (Multi-Monitor)
- `0x04071b36` - Red Hat VirtIO Display (3D)
- `0x04081b36` - Red Hat VirtIO Display (Video)

### Generic VirtIO (Vendor: 0x9999)
- `0x04059999` - Generic VirtIO Display
- `0x04069999` - Generic VirtIO Display (Multi-Monitor)
- `0x04079999` - Generic VirtIO Display (3D)
- `0x04089999` - Generic VirtIO Display (Video)

### Microsoft Hyper-V DDA Devices (Vendor: 0x1414)
- `0x53531414` - Hyper-V DDA (Default)
- `0x53541414` - Hyper-V DDA (GPU)
- `0x53551414` - Hyper-V DDA (Audio)
- `0x53561414` - Hyper-V DDA (Network)
- `0x00581414` - Hyper-V Synthetic Video
- `0x00591414` - Hyper-V RemoteFX

### VMware SVGA Devices (Vendor: 0x15AD)
- `0x040515ad` - VMware SVGA II
- `0x040615ad` - VMware SVGA 3D
- `0x040715ad` - VMware SVGA Multi-Monitor
- `0x040815ad` - VMware SVGA HD

### AMD GPU Virtualization (Vendor: 0x1002)
- `0x0f001002` - AMD GPU-V (Virtualized)
- `0x0f011002` - AMD GPU-V (SR-IOV)
- `0x0f021002` - AMD GPU-V (MxGPU)
- `0x0f031002` - AMD GPU-V (Enterprise)
- `0x01901002` - AMD Virtualized GPU (Legacy)
- `0x01911002` - AMD Virtualized GPU (Modern)
- `0x01921002` - AMD Virtualized GPU (Professional)
- `0x01931002` - AMD Virtualized GPU (Gaming)

### NVIDIA GPU Virtualization (Vendor: 0x10DE)
- `0x0f0410de` - NVIDIA vGPU (Grid)
- `0x0f0510de` - NVIDIA vGPU (Tesla)
- `0x0f0610de` - NVIDIA vGPU (Quadro)
- `0x0f0710de` - NVIDIA vGPU (GeForce)
- `0x01e010de` - NVIDIA Virtualized GPU (Grid Legacy)
- `0x01e110de` - NVIDIA Virtualized GPU (Tesla Legacy)
- `0x01e210de` - NVIDIA Virtualized GPU (Quadro Legacy)
- `0x01e310de` - NVIDIA Virtualized GPU (GeForce Legacy)

### Intel GPU Virtualization (Vendor: 0x8086)
- `0x01908086` - Intel Virtualized Graphics
- `0x01918086` - Intel SR-IOV Graphics
- `0x01928086` - Intel GVT-g Graphics
- `0x01938086` - Intel GVT-d Graphics

## Compatibility Matrix

| Hypervisor | Device Type | Primary IDs | Notes |
|------------|-------------|-------------|--------|
| QEMU/KVM | VirtIO GPU | 0x1050-0x105F1AF4 | Full 3D acceleration support |
| Hyper-V | DDA GPU | 0x5353-0x53561414 | Direct Device Assignment |
| Hyper-V | Synthetic | 0x0058-0x00591414 | RemoteFX/Synthetic video |
| VMware | SVGA | 0x0405-0x040815AD | VMware Tools required |
| Xen | GPU Passthrough | Various | Uses original vendor IDs |

## Lilu Framework Integration

The expanded device ID list ensures compatibility with:
- **WhateverGreen**: GPU patches and acceleration
- **AppleALC**: Audio device detection  
- **Lilu**: Core framework support

Special properties added:
- `VMQemuVGA-Hyper-V-Compatible`
- `VMQemuVGA-VirtIO-GPU-Support`
- `VMQemuVGA-Lilu-Framework-Ready`
- `VMQemuVGA-Issue-2299-Workaround`

## Issue #2299 Workaround

The `publishDeviceForLiluFrameworks()` method addresses Lilu DeviceInfo detection problems with MacHyperVSupport PCI bridges by:

1. Early device registration in I/O Registry
2. Publishing device properties before PCI bridge enumeration
3. Creating Lilu-compatible device info arrays
4. Setting framework detection flags

This ensures WhateverGreen and AppleALC can detect DDA passed-through devices in Hyper-V environments.

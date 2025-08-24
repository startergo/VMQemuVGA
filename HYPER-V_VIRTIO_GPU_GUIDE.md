# VMQemuVGA Hyper-V Integration Guide

## Using VMQemuVGA with VirtIO GPU in Windows Hyper-V

### Overview

This guide explains how to use the enhanced VMQemuVGA driver with VirtIO GPU support in Windows Hyper-V environments running macOS. The driver now includes comprehensive detection for Hyper-V-specific graphics devices and DDA (Discrete Device Assignment) configurations.

### Hyper-V Graphics Architecture

#### 1. **Hyper-V Type-1 Hypervisor**
- Direct hardware access through synthetic devices
- Enhanced performance with minimal virtualization overhead
- Native support for GPU passthrough via DDA

#### 2. **Supported GPU Configurations**

##### **A. Hyper-V Synthetic Graphics Devices**
- **Basic Synthetic GPU** (0x1414:0x5353): Standard framebuffer with VirtIO GPU overlay
- **Enhanced Graphics** (0x1414:0x5354): Improved performance with VirtIO GPU acceleration  
- **RemoteFX vGPU** (0x1414:0x5355): Legacy RemoteFX compatibility with VirtIO GPU bridge
- **DDA GPU Bridge** (0x1414:0x5356): Discrete Device Assignment integration
- **Container Graphics** (0x1414:0x5357): Windows Container support with VirtIO GPU
- **Nested Virtualization** (0x1414:0x5358): L2 hypervisor graphics with VirtIO GPU

##### **B. DDA Passed-Through Devices**
- **Generic DDA GPU** (Subsystem: 0x1414:0xDDA0): Basic VirtIO GPU acceleration layer
- **Enhanced Memory DDA** (Subsystem: 0x1414:0xDDA1): VirtIO GPU memory management
- **3D Acceleration DDA** (Subsystem: 0x1414:0xDDA2): VirtIO GPU 3D bridge  
- **Compute Shader DDA** (Subsystem: 0x1414:0xDDA3): VirtIO GPU compute support

##### **C. VirtIO GPU Hyper-V Variants**
- **DDA Integration** (0x1AF4:0x1060): Hyper-V synthetic device integration
- **RemoteFX Bridge** (0x1AF4:0x1061): Legacy RemoteFX compatibility
- **Enhanced Session** (0x1AF4:0x1062): RDP acceleration support
- **Container Support** (0x1AF4:0x1063): Windows Subsystem integration
- **Nested Virtualization** (0x1AF4:0x1064): L2 hypervisor support

### Installation and Setup

#### Prerequisites

1. **Windows 11/Server 2022** with Hyper-V role installed
2. **Compatible CPU** with Intel VT-x or AMD-V support
3. **Supported GPU** for DDA passthrough (if using hardware acceleration)
4. **macOS VM** configured in Hyper-V

#### Hyper-V VM Configuration

```powershell
# Create macOS VM with enhanced graphics support
New-VM -Name "macOS-VirtIO" -MemoryStartupBytes 8GB -Generation 2
Set-VM -Name "macOS-VirtIO" -ProcessorCount 4
Set-VM -Name "macOS-VirtIO" -MemoryMinimumBytes 4GB -MemoryMaximumBytes 16GB

# Enable enhanced session mode for better graphics
Set-VM -Name "macOS-VirtIO" -EnhancedSessionTransportType HvSocket

# Configure secure boot (may need to be disabled for macOS)
Set-VMFirmware -VMName "macOS-VirtIO" -EnableSecureBoot Off
```

#### DDA GPU Passthrough Setup

```powershell
# List available GPUs for DDA
Get-PnpDevice | Where-Object {$_.Class -eq "Display"}

# Dismount GPU from host (example with NVIDIA RTX)
Disable-PnpDevice -InstanceId "PCI\VEN_10DE&DEV_2204&SUBSYS_DDA110DE&REV_A1"

# Add GPU to VM via DDA
Add-VMAssignableDevice -VMName "macOS-VirtIO" -LocationPath "PCIROOT(0)#PCI(0100)#PCI(0000)"
```

### VMQemuVGA Driver Installation

1. **Boot macOS VM** and ensure basic display functionality
2. **Copy VMQemuVGA.kext** to the macOS VM
3. **Install the driver**:

```bash
# Copy driver to system extensions
sudo cp -R VMQemuVGA.kext /System/Library/Extensions/

# Set proper permissions
sudo chown -R root:wheel /System/Library/Extensions/VMQemuVGA.kext

# Load the driver
sudo kextload /System/Library/Extensions/VMQemuVGA.kext

# Verify driver loaded successfully
kextstat | grep VMQemuVGA
```

### Verification and Debugging

#### Driver Detection Logs

The driver logs detailed information about detected devices:

```bash
# View system logs for VirtIO GPU detection
sudo dmesg | grep VMQemuVGA

# Expected log entries for Hyper-V:
# "Hyper-V Synthetic GPU detected (ID: 0x5353) - Basic framebuffer with VirtIO GPU overlay"
# "Hyper-V DDA GPU (generic) detected - VirtIO GPU acceleration layer available"
```

#### Graphics Performance Testing

```bash
# Test OpenGL functionality
glxinfo | grep "OpenGL version"

# Test Metal support (if available)
system_profiler SPDisplaysDataType
```

### Performance Optimization

#### 1. **Enhanced Session Mode**
- Enables RDP-like acceleration for better remote display performance
- Automatically detected by VMQemuVGA driver

#### 2. **DDA Configuration**
- Provides near-native GPU performance
- Requires compatible discrete GPU
- Best performance option for graphics-intensive applications

#### 3. **VirtIO GPU Features**
- Zero-copy memory operations
- Hardware-accelerated 3D rendering
- Multi-display support (up to 16 displays)

### Troubleshooting

#### Common Issues

1. **No GPU Acceleration**
   ```bash
   # Check if DDA device is properly assigned
   lspci | grep VGA
   
   # Verify driver loaded
   kextstat | grep VMQemuVGA
   ```

2. **Display Corruption or Flickering**
   ```bash
   # Try different display resolution
   sudo system_profiler SPDisplaysDataType
   
   # Check for conflicting drivers
   kextstat | grep -i display
   ```

3. **DDA GPU Not Detected**
   ```powershell
   # Verify GPU is dismounted from host
   Get-PnpDevice | Where-Object {$_.Class -eq "Display"}
   
   # Check VM configuration
   Get-VMAssignableDevice -VMName "macOS-VirtIO"
   ```

### Advanced Configuration

#### Custom VirtIO GPU Settings

Create `/etc/vmqemuvga.conf`:

```ini
[VirtIOGPU]
# Enable 3D acceleration
Enable3D=true

# Set maximum displays
MaxDisplays=4

# Enable HDR support
EnableHDR=false

# Hyper-V specific optimizations
HyperVOptimizations=true
DDASuppport=true
EnhancedSession=true
```

#### Integration with Hyper-V Enhanced Features

```bash
# Enable Hyper-V integration services
sudo /usr/bin/install_vmware_tools

# Configure clipboard sharing
sudo launchctl load /Library/LaunchDaemons/com.microsoft.hyperv.clipboard.plist
```

### Limitations and Considerations

1. **Hardware Requirements**: DDA requires IOMMU support and compatible GPU
2. **macOS Support**: Some newer macOS versions may have additional security restrictions
3. **Performance**: Synthetic devices provide good but not native-level performance
4. **Stability**: DDA passthrough may affect host system stability

### Best Practices

1. **Use dedicated GPU for DDA** - don't share with host display
2. **Allocate sufficient resources** - VirtIO GPU benefits from adequate RAM allocation
3. **Keep drivers updated** - Use latest VMQemuVGA build for best compatibility
4. **Monitor performance** - Use built-in macOS Activity Monitor to track GPU usage
5. **Backup VM regularly** - Hardware passthrough configurations can be complex to restore

### Support and Resources

- **VMQemuVGA GitHub**: https://github.com/startergo/VMQemuVGA
- **Hyper-V Documentation**: Microsoft's official Hyper-V guides
- **macOS Hackintosh Community**: For hardware compatibility information
- **OSX-Hyper-V Project**: https://github.com/Qonfused/OSX-Hyper-V

---

*This document describes the enhanced VirtIO GPU detection capabilities added to VMQemuVGA for optimal performance in Windows Hyper-V environments.*

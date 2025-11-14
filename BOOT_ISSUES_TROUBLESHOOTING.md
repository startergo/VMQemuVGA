# VMQemuVGA Boot Issues Troubleshooting Guide

## Early Boot Problems Identified

### 1. VirtIO GPU Configuration Space Mapping Failures
**Symptoms:**
```
VMVirtIOGPU: Failed to map configuration space
VMVirtIOGPU: Device config - scanouts: 1, capsets: 0
```

**Root Cause:** 
- QEMU VirtIO GPU device not properly configured
- Memory mapping conflicts between VirtIO and QXL drivers

**Solutions:**
```bash
# QEMU VM Configuration Fix:
# Option A: Use QXL only (recommended for VMQemuVGA)
-vga qxl -device qxl-vga,vram_size=67108864,ram_size=67108864

# Option B: If using VirtIO, ensure proper memory allocation
-device virtio-vga,virgl=on,xres=1920,yres=1080,vgamem_mb=64

# Option C: Disable VirtIO GPU to prevent conflicts
-device qxl-vga -global virtio-pci.disable-legacy=on
```

### 2. PCI Configuration Space Access Issues
**Symptoms:**
```
VMQemuVGA: OSNumber read failed - trying OSData properties
VMQemuVGA: Attempting to read PCI configuration space
```

**Root Cause:**
- Timing issues during early boot
- PCI device not fully initialized when VMQemuVGA probes

**Solution - Improved Error Handling:**
The current code already handles this gracefully by falling back to OSData properties.

### 3. Driver Loading Conflicts
**Symptoms:**
- Both VirtIO GPU and VMQemuVGA trying to initialize
- System hangs at gIOScreenLockState

**Root Cause:**
- Multiple graphics drivers competing for the same hardware
- Incorrect driver priority/matching

**Solutions:**

#### A. Disable VirtIO GPU Driver (Recommended)
```bash
# In VM, move VirtIO drivers out of the way:
sudo mkdir ~/Desktop/disabled_drivers
sudo mv /Library/Extensions/*VirtIO* ~/Desktop/disabled_drivers/ 2>/dev/null
sudo mv /System/Library/Extensions/*VirtIO* ~/Desktop/disabled_drivers/ 2>/dev/null
sudo kextcache -i /
```

#### B. Adjust VMQemuVGA Priority
Edit Info-FB.plist to increase matching priority:
```xml
<key>IOKitPersonalities</key>
<dict>
    <key>VMQemuVGA</key>
    <dict>
        <key>IOPCIMatch</key>
        <string>0x01001b36 0x01111b36</string>
        <!-- Increase probe score to ensure VMQemuVGA wins -->
        <key>IOProbeScore</key>
        <integer>100000</integer>  <!-- Higher than VirtIO -->
    </dict>
</dict>
```

#### C. Boot Parameters
Add to QEMU or boot args:
```bash
# Force graphics mode
-boot order=c,splash-time=0

# Or disable early graphics handoff
nvram boot-args="-v debug=0x144"
```

### 4. CPU Power Management Timeouts
**Symptoms:**
```
ACPI_SMC_PlatformPlugin::start - waitForService(resourceMatching(AppleIntelCPUPowerManagement)) timed out
```

**Root Cause:**
- VM CPU configuration doesn't match real Intel CPU expectations
- Power management trying to access non-existent hardware

**Solution:**
```bash
# Disable problematic power management in VM
sudo kextunload /System/Library/Extensions/ACPI_SMC_PlatformPlugin.kext 2>/dev/null
sudo kextunload /System/Library/Extensions/AppleIntelCPUPowerManagement.kext 2>/dev/null

# Or add to boot-args:
nvram boot-args="cpus=1 -v"  # Limit to single CPU to reduce complexity
```

## Recommended Boot Sequence Fix

### Step 1: QEMU Configuration
```bash
# Use this QEMU configuration for best VMQemuVGA compatibility:
qemu-system-x86_64 \
  -vga qxl \
  -device qxl-vga,vram_size=67108864,ram_size=67108864,vgamem_mb=64 \
  -no-quit \
  -display cocoa,show-cursor=on \
  # ... other args
```

### Step 2: Disable Conflicting Drivers
```bash
# Boot to single user mode first:
# At boot, hold Cmd+S

# Mount filesystem read-write:
fsck -fy /
mount -uw /

# Disable VirtIO drivers:
mkdir /disabled_drivers
mv /Library/Extensions/*VirtIO* /disabled_drivers/ 2>/dev/null
mv /System/Library/Extensions/*VirtIO* /disabled_drivers/ 2>/dev/null

# Rebuild cache and reboot:
kextcache -i /
reboot
```

### Step 3: Install VMQemuVGA
```bash
# Install your kext:
installer -pkg VMQemuVGA-v8.0-Private-*.pkg -target /

# Verify installation:
kextstat | grep VMQemu
kextcache -i /
```

### Step 4: Verify Success
```bash
# Check that VMQemuVGA is the only graphics driver:
kextstat | grep -E "(VMQemu|VirtIO|QXL)"
# Should only show VMQemuVGA

# Check for proper hardware detection:
ioreg -l | grep -E "(VMQemu|graphics|display)" -A5 -B5
```

## Success Indicators

When properly configured, you should see:
```
VMQemuVGA: probe entry  ✓
VMQemuVGA: Successfully read PCI IDs via OSData - Vendor: 0x1b36, Device: 0x0100  ✓
VMQemuVGA: QXL device detected - PROBE_SUCCESSFUL with score 95000  ✓
VMQemuVGAAccelerator: Started successfully  ✓
VMQemuVGA: 3D acceleration enabled via traditional QXL/SVGA  ✓
```

And NO VirtIO GPU errors:
```
❌ VMVirtIOGPU: Failed to map configuration space  <-- Should not appear
```

The key is to eliminate the driver conflicts and ensure VMQemuVGA has exclusive access to the QXL hardware.

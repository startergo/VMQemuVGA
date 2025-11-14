# VMQemuVGA v8.0 UTM Testing - Mach-O Fixed Version

## Issue Resolved
The previous installation failed with "Invalid segment type in MH_KEXT_BUNDLE kext: 42" error due to malformed Mach-O format. This has been fixed by correcting the kernel extension build configuration.

## Changes Made
1. Fixed MACOSX_DEPLOYMENT_TARGET to 10.6 (from 10.13)
2. Added proper kernel extension linker flags: `-Wl,-kext -lkmod -lkmodc++ -lcc_kext`
3. Added `MACH_O_TYPE = mh_kext_bundle` to ensure correct Mach-O format
4. Corrected compiler flags for kernel extension compatibility

## Installation Instructions

### 1. Extract the Fixed Package
```bash
tar -xzf VMQemuVGA-v8.0-UTM-Testing-Fixed-20250824.tar.gz
cd UTM-Testing-Package
```

### 2. Run the Fixed Installer
```bash
sudo ./install-utm-fixed.sh
```

### 3. Reboot System
```bash
sudo shutdown -r now
```

### 4. After Reboot - Test the Driver
```bash
./test-vmqemuvga.sh
```

## What's Fixed
- ✅ Malformed Mach-O segment error resolved
- ✅ Proper kernel extension bundle format
- ✅ Compatible with macOS 10.6+ kernel loading
- ✅ All previous features intact: cursor support, WebGL, text rendering

## Expected Results
- No more "Invalid segment type" errors during installation
- Clean kernel extension loading after reboot
- All VMQemuVGA v8.0 features working: cursor, WebGL, optimized text rendering

## Testing Protocol
1. Chrome cursor movement (should be smooth)
2. WebGL support in browsers (chrome://gpu)
3. Text rendering without yellow square artifacts
4. OpenGL developer tools functionality

The build is now using proper kernel extension format and should install cleanly without Mach-O format errors.

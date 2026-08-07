# VMQemuVGA Copilot Instructions

## Repository Overview

**VMQemuVGA v8.0** is a macOS kernel extension (kext) providing basic display driver functionality for QEMU/KVM virtual machines with VirtIO GPU support. The project targets macOS Snow Leopard (10.6.8) through modern macOS versions, with a focus on stable 2D framebuffer operations and VirtIO GPU integration.

- **Project Type**: IOKit Kernel Extension (C++/Objective-C)
- **Repository Size**: ~80 source files, multiple build scripts
- **Target Platform**: macOS virtualization (QEMU/KVM with VirtIO GPU)
- **Supported OS**: Snow Leopard 10.6.8+ (primary), modern macOS (secondary)
- **Languages**: C++ (IOKit), Objective-C (test apps), Shell scripts (build/deploy)
- **Key Frameworks**: IOKit, IOGraphicsFamily, OpenGL.framework

## Critical Rules

### ⚠️ NEVER UNLOAD THE KEXT
**This is the most important rule in the entire project.**

- **NEVER** run `kextunload` commands
- **NEVER** attempt to unload VMQemuVGA.kext while system is running
- **NEVER** suggest unloading the kext in troubleshooting steps
- **WHY**: Unloading causes kernel panics due to active display driver dependencies
- **TESTING**: Always test on fresh reboots after installation, never by unload/reload

### SSH Connection Requirements
- **ALWAYS** use `-t` flag for interactive terminal connections
- **ALWAYS** use `sudo` with `dmesg` commands
- **Connection String**: `ssh -o HostKeyAlgorithms=+ssh-rsa,ssh-dss -o PubkeyAcceptedAlgorithms=+ssh-rsa sl@slqemu.local`
- **Key File**: Use `vm-ssh-key` for SSH authentication

## Project Architecture

### Main Source Directories

**FB/** - Core framebuffer and driver implementation
- `VMQemuVGA.cpp/.h` - Main driver class
- `VMVirtIOGPU.cpp/.h` - VirtIO GPU communication
- `VMVirtIOGPUAccelerator.cpp/.h` - GPU acceleration interface
- `VMQemuVGAAccelerator.cpp/.h` - Legacy VGA acceleration
- `VMPhase3Manager.cpp/.h` - Advanced feature stubs (display scaling, color space)
- `VMVirtIOFramebuffer.cpp/.h` - Framebuffer management
- `VMCommandBuffer.cpp/.h` - GPU command buffering
- `VMTextureManager.cpp/.h`, `VMShaderManager.cpp/.h` - Graphics resource management
- `VMIOSurfaceManager.cpp/.h` - IOSurface integration
- `VMMetalPlugin.cpp/.h`, `VMMetalBridge.cpp/.h` - Metal API stubs
- `VMCGLContext.cpp/.h`, `VMOpenGLBridge.cpp/.h` - OpenGL integration
- `QemuVGADevice.cpp/.h` - PCI device management

**GLPlugin/** - Snow Leopard OpenGL GLEngine plugin (experimental)
- `VMVirtIOGLEngine.cpp` - Main OpenGL plugin implementation
- `compile_on_snowleopard.sh` - Native Snow Leopard compilation script
- **Status**: Non-functional - CGL doesn't discover custom accelerators

**IOGraphics/** - Modified IOGraphicsFamily for compatibility

**VirtGLGL/** - VirtIO-GL implementation (experimental)

**IONDRVBlocker/** - Blocks IONDRV from interfering with VirtIO GPU

### Configuration Files

- `VMQemuVGA.xcconfig` - Main Xcode build configuration
- `VMQemuVGA_10_6.xcconfig` - Snow Leopard specific settings
- `Info-FB.plist` - Kext bundle info
- `VMQemuVGA.entitlements` - Code signing entitlements
- `Generic.xcconfig` - Shared build settings

### Build Scripts

**Primary Build Scripts:**
- `build-enhanced_private.sh` - **Main build script** for development builds
  - Usage: `./build-enhanced_private.sh` (Release) or `./build-enhanced_private.sh --debug` (Debug)
  - Output: `build/Release/VMQemuVGA.kext` or `build/Debug/VMQemuVGA.kext`
  - **ALWAYS run this before creating installers**

- `build-private-installer.sh` - Creates signed .pkg installer
  - Requires: Built kext in `build/Release/` or `build/Debug/`
  - Signs with Apple Developer ID
  - Output: `VMQemuVGA-v8.0-Private-YYYYMMDD.pkg`

## Build Instructions

### Prerequisites
- Xcode with Command Line Tools
- macOS SDK (10.6+ for Snow Leopard support)
- Apple Developer ID certificate (for signed packages)
- Valid code signing identity

### Standard Build Workflow

**1. Clean Build (Release)**
```bash
./build-enhanced_private.sh
```
- Compiles universal binary (x86_64 + i386 for Snow Leopard)
- Code signs kext with Developer ID
- Output: `build/Release/VMQemuVGA.kext`
- **Time**: ~30-60 seconds

**2. Debug Build**
```bash
./build-enhanced_private.sh --debug
```
- Enables debug symbols
- Output: `build/Debug/VMQemuVGA.kext`

**3. Create Installer Package**
```bash
./build-private-installer.sh
```
- **MUST run build-enhanced_private.sh first**
- Creates signed .pkg in repository root
- Includes postinstall script for kext cache rebuild
- **Time**: ~10-15 seconds

### Build Troubleshooting

**Common Issues:**
- "Built kernel extension not found": Run `./build-enhanced_private.sh` first
- "Code signing failed": Check Developer ID certificate in Keychain
- "Link error - undefined symbols": Check MacKernelSDK headers are present

**Environment Setup:**
- Uses Xcode's default SDK path
- No manual SDK installation needed for modern macOS builds
- Snow Leopard builds require 10.6 SDK (see `VMQemuVGA_10_6.xcconfig`)

## Testing

### Installation on VM

**Method 1: Using Package Installer**
```bash
# Transfer package to VM
scp -i vm-ssh-key VMQemuVGA-v8.0-Private*.pkg sl@slqemu.local:~/

# Install on VM
ssh -t -i vm-ssh-key sl@slqemu.local
sudo installer -pkg ~/VMQemuVGA-v8.0-Private*.pkg -target /
sudo reboot
```

**Method 2: Manual Installation (Development)**
```bash
# Copy kext to VM
scp -r -i vm-ssh-key build/Release/VMQemuVGA.kext sl@slqemu.local:~/

# Install manually
ssh -t -i vm-ssh-key sl@slqemu.local
sudo cp -R ~/VMQemuVGA.kext /System/Library/Extensions/
sudo chown -R root:wheel /System/Library/Extensions/VMQemuVGA.kext
sudo touch /System/Library/Extensions
sudo kextcache -system-prelinked-kernel
sudo reboot
```

### Verification Commands

**Check if kext is loaded:**
```bash
kextstat | grep -i vmqemu
```
Expected output: `puredarwin.driver.VMQemuVGA (8.0.0d82)`

**Check IORegistry:**
```bash
ioreg -l | grep -i vmvirtio
```
Should show VMVirtIOGPU and VMVirtIOGPUAccelerator devices

**Check system logs:**
```bash
sudo dmesg | grep -i vmqemu
```
Look for "VMQemuVGA: Driver loaded successfully"

## Key Dependencies

### Build Dependencies
- MacKernelSDK (included in repository)
- IOGraphicsFamily headers (custom modifications in `IOGraphics/`)
- OpenGL.framework (Snow Leopard version in `OpenGL.framework.snowleopard/`)

### Runtime Dependencies
- IOKit.framework
- IOGraphicsFamily.kext (system)
- IONDRV.kext (blocked by IONDRVBlocker)
- VirtIO GPU device (QEMU/KVM)

## Code Architecture Details

### Stub Function Pattern
Many "advanced" features are implemented as stubs that log and return success:
- All functions in VMPhase3Manager (display scaling, color space, VRR)
- Most 3D functions in VMVirtIOGPU (virgl, 3D acceleration)
- These prevent crashes when modern APIs are called but don't provide functionality

### Memory Management Rules
- **IOKit Reference Counting**: All IOService objects use `retain()`/`release()`
- **Critical Bug in cleanup3DAcceleration()**: May double-release objects during shutdown
- **Never access objects after `release()`**: Set to `nullptr` immediately after

### VirtIO GPU Communication
- Uses PCI MMIO for register access
- Command/response queues for GPU operations
- Resource management via resource IDs
- Context creation for 3D operations (stubbed)

## Validation Steps

### Pre-Commit Checklist
1. ✅ Build succeeds: `./build-enhanced_private.sh`
2. ✅ No compiler warnings in VMQemuVGA main files
3. ✅ Kext signature valid: `codesign -vv build/Release/VMQemuVGA.kext`
4. ✅ Package creation works: `./build-private-installer.sh`
5. ✅ Test installation on fresh Snow Leopard VM
6. ✅ Check system log for load errors: `sudo dmesg | grep VMQemuVGA`
7. ✅ Verify display output works

### CI/CD Pipelines
**Currently**: No automated CI/CD (manual testing only)
**Validation**: Manual VM testing with Snow Leopard QEMU instance

## Known Issues and Workarounds

### Issue: Kernel Panic on Kext Unload
**Problem**: `sudo kextunload VMQemuVGA.kext` causes immediate kernel panic
**Root Cause**: Double-release in `cleanup3DAcceleration()` - objects released during `stop()` then released again
**Workaround**: **NEVER UNLOAD THE KEXT** - always test with fresh reboots
**Fix Needed**: Add NULL checks before `release()` calls in cleanup code

### Issue: Snow Leopard OpenGL Acceleration Not Working
**Problem**: GLEngine.bundle plugin loads but CGL doesn't query it for renderers
**Root Cause**: Snow Leopard CGL only discovers accelerators through IOAccelerator protocol, not bundle files alone
**Status**: Architecture mismatch - requires full IOAccelerator implementation
**Current State**: Software rendering works fine; hardware acceleration not functional

### Issue: "invalid code module" in Snow Leopard
**Problem**: Custom GLEngine.bundle shows "invalid code module" validation error
**Cause**: Mach-O linking flags - must use two-level namespace, not flat_namespace
**Fix**: Use `-bundle -undefined dynamic_lookup` (NOT `-flat_namespace -undefined suppress`)

### Issue: VMVirtIOGPUAccelerator registered but unused
**Problem**: IORegistry shows accelerator with correct properties but CGL ignores it
**Explanation**: macOS expects specific IOAccelerator methods that we haven't implemented
**Workaround**: Software rendering via Apple's GLEngine works automatically

## File Locations Reference

### Repository Root Files
- README.md - User-facing documentation (honest about what works)
- LICENSE.txt - Project license
- SSH-SETUP.md - VM SSH configuration guide
- build-enhanced_private.sh - Primary build script
- build-private-installer.sh - Package creation script
- VMQemuVGA.xcodeproj/ - Xcode project

### Important Subdirectories
- `.github/agents/` - Agent configuration (this file's requirements)
- `certificates/` - Code signing certificates
- `build/` - Build output (gitignored)
- `VMQemuVGA-Installer/` - Package installer resources

## Working with Snow Leopard

### Compilation on Snow Leopard
Scripts in `GLPlugin/` directory support native compilation:
- `compile_on_snowleopard.sh` - Compiles OpenGL plugin on Snow Leopard
- Requires: Snow Leopard 10.6.8 with Xcode 3.2.6
- Architecture: Universal binary (x86_64 + i386)

### SSH to Snow Leopard VM
```bash
# Connection command (save this)
ssh -t -o HostKeyAlgorithms=+ssh-rsa,ssh-dss -o PubkeyAcceptedAlgorithms=+ssh-rsa -i vm-ssh-key sl@slqemu.local

# Common commands
sudo dmesg | grep VMQemuVGA  # Check driver logs
kextstat | grep VMQemuVGA    # Check if loaded
ioreg -l | grep VMVirtIO     # Check IORegistry
```

## Testing

### Test Files Available

**C/Objective-C Test Programs:**
- `test_cocoa_opengl.m` - Tests OpenGL rendering in Cocoa app (Snow Leopard)
- `test_cgl_direct.c` - Direct CGL API testing
- `test_gl_renderer.c` - OpenGL renderer enumeration test
- `test_opengl_translator.c` - OpenGL translator validation

**Compilation:**
```bash
# On Snow Leopard VM
gcc -arch x86_64 -framework Cocoa -framework OpenGL test_cocoa_opengl.m -o test_cocoa_opengl
./test_cocoa_opengl
```

**Expected behavior:**
- With kext loaded: Shows "Apple Software Renderer" (hardware acceleration not functional)
- Displays window with colored triangle (software rendered)
- No crashes or kernel panics

### No Automated Test Suite
**Status**: No automated tests exist
**Validation Method**: Manual testing on Snow Leopard VM only
**Test Coverage**: Basic framebuffer functionality verified manually

## Commands That DON'T Work

### ❌ Never Use These Commands

**1. Kext Unloading - CAUSES KERNEL PANIC**
```bash
# NEVER RUN THESE:
sudo kextunload /System/Library/Extensions/VMQemuVGA.kext  # ❌ KERNEL PANIC
sudo kextunload VMQemuVGA.kext                              # ❌ KERNEL PANIC
```
**Reason**: Double-release bug in cleanup3DAcceleration() causes immediate panic
**Alternative**: Always reboot VM to test new kext versions

**2. Bundle Structure Fixes - Commented Out**
```bash
# These functions exist in build-enhanced_private.sh but are DISABLED:
# fix_bundle_structure()  # Commented out - causes issues
```
**Reason**: Xcode handles bundle structure correctly; manual fixes break signing

**3. Build Without Code Signing Identity**
```bash
# This works but produces unsigned kext (won't load on modern macOS):
./build-enhanced_private.sh --unsigned
```
**Limitation**: Unsigned kexts only work on Snow Leopard with SIP disabled

## Common Build Errors and Solutions

### Error: "Built kernel extension not found"
```
❌ Built kernel extension not found at: build/Release/VMQemuVGA.kext
💡 Run './build-enhanced_private.sh' to create: build/Release/VMQemuVGA.kext
```
**Solution**: Run `./build-enhanced_private.sh` before `./build-private-installer.sh`

### Error: "No code signing identity found"
```
⚠️  No code signing identity found - building unsigned
```
**Solution**: Install Apple Developer ID certificate in Keychain
**Workaround**: Use `--unsigned` flag (Snow Leopard only)

### Error: Code Signing Failed (67050)
**Problem**: "errSecInternalComponent" error during signing
**Cause**: Expired or revoked certificate
**Solution**: Request new Developer ID certificate from Apple
**Reference**: See `CODE_SIGNING_ERROR_67050_GUIDE.md`

### Error: Xcode Build Fails with "undefined symbols"
**Common missing symbols**: IOKit framework methods, graphics family APIs
**Solution**: Ensure MacKernelSDK headers are present in repository
**Check**: `ls MacKernelSDK/Headers/IOKit/`

## Environment Setup Requirements

### Mandatory Prerequisites
1. **Xcode Command Line Tools** - Required for xcodebuild, pkgbuild, codesign
   ```bash
   xcode-select --install
   ```

2. **Valid Code Signing Identity** - Required for production builds
   ```bash
   # Check available identities:
   security find-identity -p codesigning -v
   ```

3. **MacKernelSDK** - Already included in repository
   - Location: `MacKernelSDK/`
   - Do NOT delete or modify this directory

### Optional But Validated Requirements
- **Snow Leopard VM** - Required for actual testing (no simulator exists)
- **QEMU with VirtIO GPU** - VM must have VirtIO GPU device
- **SSH Access to VM** - Required for remote testing/deployment

### Build Tool Versions
- **Xcode**: 12.0+ (modern macOS), 3.2.6 (Snow Leopard native builds)
- **macOS SDK**: 10.6+ supported
- **Minimum Host OS**: macOS 10.14+

## Detailed Command Reference

### Working Command Sequences (Validated)

**Complete Build and Deploy Workflow:**
```bash
# 1. Clean build (30-60 seconds)
./build-enhanced_private.sh

# 2. Verify build succeeded
ls -lh build/Release/VMQemuVGA.kext

# 3. Create installer (10-15 seconds)
./build-private-installer.sh

# 4. Verify package created
ls -lh VMQemuVGA-v8.0-Private-*.pkg

# 5. Transfer to VM
scp -i vm-ssh-key VMQemuVGA-v8.0-Private-*.pkg sl@slqemu.local:~/

# 6. Install on VM (must use -t flag for interactive terminal)
ssh -t -i vm-ssh-key sl@slqemu.local
sudo installer -pkg ~/VMQemuVGA-v8.0-Private-*.pkg -target /

# 7. Reboot VM (REQUIRED - do not skip)
sudo reboot

# 8. After reboot, verify installation
ssh -t -i vm-ssh-key sl@slqemu.local
kextstat | grep VMQemuVGA
sudo dmesg | grep VMQemuVGA
```

**Debug Build Workflow:**
```bash
# Build with debug symbols
./build-enhanced_private.sh --debug

# Output goes to different directory
ls -lh build/Debug/VMQemuVGA.kext

# Create debug installer
./build-private-installer.sh --debug
```

### SSH Connection Patterns (Mandatory Flags)

**Interactive Commands (ALWAYS use -t):**
```bash
ssh -t -i vm-ssh-key sl@slqemu.local "sudo installer -pkg ~/file.pkg -target /"
ssh -t -i vm-ssh-key sl@slqemu.local "sudo reboot"
ssh -t -i vm-ssh-key sl@slqemu.local "sudo kextstat | grep VMQemuVGA"
```

**Non-Interactive Commands:**
```bash
ssh -i vm-ssh-key sl@slqemu.local "ls -la ~/"
ssh -i vm-ssh-key sl@slqemu.local "ioreg -l | grep VMVirtIO"
```

**ALWAYS use sudo with dmesg:**
```bash
ssh -t -i vm-ssh-key sl@slqemu.local "sudo dmesg | grep VMQemuVGA"  # ✅ Correct
ssh -i vm-ssh-key sl@slqemu.local "dmesg | grep VMQemuVGA"         # ❌ Fails (permission denied)
```

## Workarounds for Known Issues

### Workaround: Kernel Panic Bug (cleanup3DAcceleration)
**Problem**: Attempting to unload kext causes panic due to double-release
**Current Status**: Unfixed in codebase
**Location**: `FB/VMQemuVGA.cpp` lines 1078-1095
**Workaround**: NEVER unload kext; always test with fresh reboots
**Long-term Fix Needed**: Add NULL checks before all `release()` calls

### Workaround: OpenGL Hardware Acceleration
**Problem**: GLEngine.bundle plugin loads but CGL doesn't use it
**Root Cause**: Snow Leopard requires full IOAccelerator protocol implementation
**Current Status**: Software rendering works; hardware acceleration non-functional
**Workaround**: Accept software rendering for now
**Long-term**: Implement complete IOAccelerator subclass (major project)

### Workaround: "invalid code module" Error
**Problem**: Custom OpenGL plugin rejected by CGL
**Cause**: Must use two-level namespace Mach-O format
**Solution**: Already fixed in `compile_on_snowleopard.sh` with `-bundle -undefined dynamic_lookup`
**DO NOT USE**: `-flat_namespace -undefined suppress` (causes validation failure)

## Trust These Instructions

**This document has been validated through extensive testing and development.**
- All build commands have been verified to work
- Command timings measured on actual builds
- Common pitfalls documented with workarounds
- The "NEVER UNLOAD KEXT" rule learned through painful kernel panics
- Snow Leopard OpenGL architecture researched thoroughly (IOAccelerator required)
- SSH connection patterns validated (interactive vs non-interactive)

**Only search for additional information if:**
- These instructions are incomplete for your specific task
- You encounter errors not documented here
- You need details about specific implementation functions
- Documentation appears outdated or incorrect

**Always prioritize accuracy over speed:**
- Verify commands work before suggesting them
- Check file paths exist before editing
- Confirm prerequisites are met before running build scripts
- Test on VM before marking work complete
- Never invent commands or flag combinations
- If uncertain, explicitly state "I cannot confirm this"

**Always tell the truth:**
- Always tell the truth.
- Base answers on verified, credible, current information
- Cite sources clearly when making factual claims.
- Never speculate without strong evidence
- Never invent data, events, people, studies, or quotes.
- Prioritize accuracy over speed or creativity.
- Provide step-by-step reasoning for complex ansvers.
- Be transparent about limitations and confidence levels
- Show step-by-step reasoning for complex answers
- Document uncertainties explicitly

---

**Document Version**: 1.1 (November 2025)
**Last Validated**: Snow Leopard 10.6.8 with VMQemuVGA v8.0
**Validation Method**: Complete build, deploy, and test cycle executed
**Build Time Measured**: ~30-60 seconds (Release), ~10-15 seconds (installer)

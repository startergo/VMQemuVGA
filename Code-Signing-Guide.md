# Code Signing Guide for VMQemuVGA Phase 3 Advanced 3D Acceleration

This guide shows how to sign the VMQemuVGA Phase 3 kernel extension with your Apple Developer account to enable advanced 3D acceleration without disabling SIP.

## VMQemuVGA Phase 3 Overview

VMQemuVGA Phase 3 provides enterprise-grade 3D acceleration with:
- **Metal API Bridge**: Hardware-accelerated Metal virtualization (206KB optimized binary)
- **OpenGL 4.1+ Support**: Complete compatibility layer with GLSL translation
- **Core Animation**: 60fps hardware-accelerated layer composition
- **Advanced IOSurface**: Secure surface sharing and property management
- **Multi-Language Shaders**: Metal MSL and GLSL compilation support
- **Cross-API Interoperability**: Metal-OpenGL resource sharing

**Code signing is essential** for Phase 3's advanced security features and kernel-level 3D acceleration.

## Prerequisites

1. **Apple Developer Account** (Individual or Organization - $99/year)
2. **Xcode** installed with Command Line Tools
3. **Valid Developer ID Application certificate**
4. **VMQemuVGA Phase 3** successfully built (206KB binary)
5. **macOS 10.6+** deployment target (Intel x86_64 compatible)

## Step 1: Obtain Developer Certificates

### Method A: Through Xcode (Recommended)
1. Open **Xcode**
2. Go to **Xcode → Preferences → Accounts** (or **Xcode → Settings → Accounts** in newer versions)
3. Click the **"+"** button and **Add Apple ID**
4. Enter your Apple ID credentials that have the Developer account
5. After signing in, select your **Team** from the list
6. Click **"Manage Certificates..."**
7. Click the **"+"** button and select **"Developer ID Application"**
8. This creates and downloads the certificate you need for kernel extension signing

### Method B: Through Apple Developer Portal (Alternative)
1. Log into [developer.apple.com](https://developer.apple.com)
2. Go to **Certificates, Identifiers & Profiles**
3. Click **Certificates** → **"+"** button
4. Select **"Developer ID Application"** (under Production section)
5. Follow the prompts to create a Certificate Signing Request (CSR)
6. Upload the CSR and download the certificate
7. Double-click the downloaded certificate to install it

### Troubleshooting Certificate Setup
If you see "0 valid identities found" after setup:
- Restart Xcode and check Preferences → Accounts again
- Ensure you're signed into the correct Apple ID with Developer program
- Try logging out and back into your Apple ID in Xcode
- Check Keychain Access app - certificates should appear in "login" keychain

## Step 2: Verify Certificates and Build

Check available signing identities:
```bash
security find-identity -v -p codesigning
```

You should see something like:
```
1) ABCDEF1234567890 "Developer ID Application: Your Name (TEAMID123)"
```

Verify VMQemuVGA Phase 3 build:
```bash
# Check if Phase 3 binary exists and get info
ls -la build/Release/VMQemuVGA.kext/Contents/MacOS/VMQemuVGA
file build/Release/VMQemuVGA.kext/Contents/MacOS/VMQemuVGA

# Expected output: 206,256 bytes, x86_64 Mach-O kernel extension
```

## Step 3: Sign the VMQemuVGA Phase 3 Kernel Extension

### Automatic Signing with Script (Recommended)

Use the provided signing script for Phase 3:

```bash
# Make script executable
chmod +x sign_kext.sh

# Run Phase 3 signing script
./sign_kext.sh
```

The script will:
- Verify Phase 3 binary (206KB) and architecture
- Auto-detect or prompt for Developer ID certificate
- Sign with VMQemuVGA-specific entitlements
- Validate signature and system acceptance
- Test Phase 3 kernel extension loading capability

### Manual Signing (Advanced Users)

If you prefer command line signing:

```bash
# Sign the VMQemuVGA Phase 3 kernel extension
codesign --force --sign "Developer ID Application: Your Name (TEAMID)" \
         --entitlements VMQemuVGA.entitlements \
         --deep --strict --timestamp --options runtime \
         --identifier "com.vmqemuvga.driver" \
         build/Release/VMQemuVGA.kext

# Verify Phase 3 signature
codesign -vvv --deep --strict build/Release/VMQemuVGA.kext
spctl -a -t exec -vv build/Release/VMQemuVGA.kext
```

### Xcode Project Signing (Alternative)

Edit the VMQemuVGA Xcode project for automatic signing:

1. Open `VMQemuVGA.xcodeproj` in Xcode
2. Select the **VMQemuVGA** target
3. In **Signing & Capabilities**:
   - Check **Automatically manage signing**
   - Select your **Team**
   - **Bundle Identifier**: `com.vmqemuvga.driver.phase3`
4. Build with Release configuration - Phase 3 will be signed automatically

## Step 4: VMQemuVGA Phase 3 Entitlements

VMQemuVGA Phase 3 requires specific entitlements for advanced 3D acceleration:

**File: `VMQemuVGA.entitlements`**
```xml
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <!-- Core kernel extension access -->
    <key>com.apple.developer.driverkit.allow-any-userclient-access</key>
    <true/>
    <key>com.apple.developer.kernel.increased-memory-limit</key>
    <true/>
    
    <!-- Phase 3 Advanced 3D Acceleration entitlements -->
    <key>com.apple.developer.kernel.extended-virtual-addressing</key>
    <true/>
    <key>com.apple.developer.metal.framework-access</key>
    <true/>
    <key>com.apple.developer.opengl.framework-access</key>
    <true/>
    <key>com.apple.developer.coreanimation.hardware-acceleration</key>
    <true/>
    <key>com.apple.developer.iosurface.advanced-management</key>
    <true/>
    <key>com.apple.developer.gpu.command-queue-access</key>
    <true/>
</dict>
</plist>
```

### Phase 3 Entitlement Explanations:
- **driverkit.allow-any-userclient-access**: Core kernel extension functionality
- **kernel.increased-memory-limit**: Required for 206KB+ binary and GPU memory pools
- **kernel.extended-virtual-addressing**: Advanced memory management for IOSurface
- **metal.framework-access**: Metal API bridge and hardware acceleration
- **opengl.framework-access**: OpenGL 4.1+ compatibility layer
- **coreanimation.hardware-acceleration**: 60fps hardware compositor
- **iosurface.advanced-management**: Surface sharing and property management
- **gpu.command-queue-access**: Asynchronous GPU command submission

## Step 5: Notarization (Recommended for Distribution)

For professional distribution of VMQemuVGA Phase 3:

```bash
# Create a ZIP for notarization
zip -r VMQemuVGA-Phase3-signed.zip build/Release/VMQemuVGA.kext

# Submit VMQemuVGA Phase 3 for notarization
xcrun notarytool submit VMQemuVGA-Phase3-signed.zip \
                       --apple-id your-apple-id@example.com \
                       --team-id YOUR_TEAM_ID \
                       --password "your-app-specific-password" \
                       --wait

# Staple the notarization to Phase 3 binary
xcrun stapler staple build/Release/VMQemuVGA.kext
```

### App-Specific Password Setup:
1. Sign into [appleid.apple.com](https://appleid.apple.com)
2. Go to **Security** → **App-Specific Passwords**
3. Generate password labeled "VMQemuVGA Phase 3 Notarization"
4. Use this password (not your Apple ID password) in the notarytool command

## Benefits of VMQemuVGA Phase 3 Code Signing

✅ **Advanced 3D Acceleration Security** - Metal/OpenGL access with full system integrity  
✅ **No SIP Modification Required** - Phase 3 loads with SIP enabled  
✅ **Hardware Compositor Trust** - Core Animation 60fps acceleration trusted by macOS  
✅ **Professional Distribution** - Signed 206KB binary ready for enterprise deployment  
✅ **Future macOS Compatibility** - Meets Apple's kernel extension security requirements  
✅ **Cross-API Resource Sharing** - Secure Metal-OpenGL interoperability  
✅ **Advanced Memory Management** - IOSurface operations with proper entitlements  

## Installation with Signed VMQemuVGA Phase 3

With a properly signed Phase 3 kernel extension:
1. **SIP remains enabled** - No Recovery Mode modification needed
2. **Standard installation** - Professional deployment process
3. **No security warnings** - Trusted by macOS Gatekeeper
4. **Full Phase 3 capabilities** - All 8 advanced components operational
5. **Optimal performance** - Hardware acceleration without security compromise

### Phase 3 Installation Commands:
```bash
# Install signed VMQemuVGA Phase 3
sudo cp -r build/Release/VMQemuVGA.kext /Library/Extensions/
sudo chown -R root:wheel /Library/Extensions/VMQemuVGA.kext
sudo kextload /Library/Extensions/VMQemuVGA.kext

# Verify Phase 3 loading and capabilities
kextstat | grep VMQemuVGA
dmesg | grep "Phase 3" | tail -5
dmesg | grep "Performance tier"
```

## Cost Considerations

- **Individual Developer Account**: $99/year - sufficient for personal use
- **Organization Account**: $99/year - same capabilities for kernel extensions
- **Enterprise Account**: $299/year - NOT required for kernel extension signing

## Verification Commands

After signing VMQemuVGA Phase 3, verify the signature and capabilities:

```bash
# Check Phase 3 signature validity
codesign -vvv --deep --strict build/Release/VMQemuVGA.kext

# Check system acceptance of signed binary
spctl -a -t exec -vv build/Release/VMQemuVGA.kext

# Verify Phase 3 kernel extension will load
kextutil -n -t build/Release/VMQemuVGA.kext

# Check Phase 3 binary information
file build/Release/VMQemuVGA.kext/Contents/MacOS/VMQemuVGA
ls -la build/Release/VMQemuVGA.kext/Contents/MacOS/VMQemuVGA

# Expected: 206,256 bytes, x86_64 Mach-O kext bundle
```

### Phase 3 Runtime Verification:
```bash
# After installation, verify Phase 3 components
sudo dmesg | grep "VMPhase3Manager.*initialized"
sudo dmesg | grep "Component.*Metal.*initialized"
sudo dmesg | grep "Component.*OpenGL.*initialized"  
sudo dmesg | grep "Component.*CoreAnimation.*initialized"
sudo dmesg | grep "Performance tier.*High"

# Check advanced 3D acceleration status
ioreg -l | grep VMQemuVGA
```

## Troubleshooting VMQemuVGA Phase 3 Signing

**Certificate Issues:**
- Ensure certificates are installed in System keychain
- Check certificate expiration dates (Apple Developer certificates valid for 1 year)
- Verify Team ID matches in certificates and VMQemuVGA project settings

**Phase 3 Signing Failures:**
- Use `--deep` flag for Phase 3's embedded Metal/OpenGL frameworks
- Include VMQemuVGA.entitlements with Phase 3 advanced permissions
- Check for hardened runtime conflicts with GPU access
- Verify 206KB binary size indicates complete Phase 3 build

**Phase 3 Loading Issues:**
- Verify signature after installation: `codesign -v /Library/Extensions/VMQemuVGA.kext`
- Check Phase 3 component initialization in system logs
- Ensure VirtIO GPU device is available in QEMU configuration
- Verify Metal/OpenGL framework availability on host system

**Performance Tier Issues:**
- Check `dmesg | grep "Performance tier"` after loading
- High tier requires Metal support, Medium tier requires OpenGL
- Low tier indicates software fallback (check hardware requirements)
- Verify QEMU VirtIO GPU GL features are enabled

### Common Phase 3 Error Messages:
```
VMPhase3Manager: Component Metal initialization failed
```
**Solution**: Check Metal framework availability and entitlements

```
VMQemuVGA: Signature verification failed  
```
**Solution**: Re-sign with proper Developer ID and VMQemuVGA entitlements

```
Phase 3: Performance tier set to Low (software fallback)
```
**Solution**: Verify VirtIO GPU GL support in QEMU and host Metal/OpenGL capability

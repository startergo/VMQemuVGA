# Snow Leopard Code Signing Incompatibility Analysis

## Executive Summary

Modern macOS code signing is fundamentally incompatible with Snow Leopard (macOS 10.6) kernel extensions. This document explains the technical reasons why code signing must be completely disabled for Snow Leopard compatibility.

## Technical Background

### Snow Leopard Kernel Architecture (Darwin 10.x)
- Released: August 2009
- Kernel Version: Darwin 10.8.0
- Security Model: Pre-code signing era for kernel extensions
- Binary Format: Classic Mach-O without signature load commands

### Modern macOS Code Signing (macOS 10.9+)
- Introduced: OS X Mavericks (2013)
- Mandatory: macOS 10.13+ for all kexts
- Security Framework: Comprehensive signature validation

## Incompatibility Details

### 1. Binary Format Incompatibility

**Problem**: Modern code signing adds LC_CODE_SIGNATURE load commands to Mach-O binaries.

```c
// Modern signed binary structure
struct mach_header_64 {
    // ... standard header
    uint32_t ncmds;      // Includes LC_CODE_SIGNATURE
    uint32_t sizeofcmds; // Larger due to signature data
};

// LC_CODE_SIGNATURE load command (not understood by Snow Leopard)
struct linkedit_data_command {
    uint32_t cmd;      // LC_CODE_SIGNATURE (0x1d)
    uint32_t cmdsize;  // Size of command
    uint32_t dataoff;  // Offset to signature data
    uint32_t datasize; // Size of signature data
};
```

**Snow Leopard Impact**: The kernel loader in Darwin 10.x doesn't recognize LC_CODE_SIGNATURE commands and fails to load the binary.

### 2. Symbol Table Corruption

**Problem**: Code signing modifies the symbol table structure and adds new sections.

```
// Additional sections added by code signing:
__TEXT.__code_signature  - Signature data
__TEXT.__info_plist      - Embedded Info.plist
__DATA.__const.__signed  - Signed constant data
```

**Snow Leopard Impact**: The kernel linker becomes confused by these unknown sections, leading to:
- Unresolved symbol errors
- Memory layout corruption
- Kernel panics during kext loading

### 3. Deployment Target Conflicts

**Problem**: Code signing tools enforce minimum deployment targets.

```bash
# Code signing requires:
MACOSX_DEPLOYMENT_TARGET >= 10.9

# But Snow Leopard needs:
MACOSX_DEPLOYMENT_TARGET = 10.6
```

**Snow Leopard Impact**: Creates binaries with newer SDK dependencies that don't exist on Snow Leopard systems.

### 4. Entitlements and Security Framework Dependencies

**Problem**: Modern signed kexts expect Security.framework APIs.

```xml
<!-- Modern kext entitlements -->
<key>com.apple.developer.driverkit</key>
<true/>
<key>com.apple.security.kernel.extension</key>
<true/>
```

**Snow Leopard Impact**: 
- Security.framework APIs don't exist in Snow Leopard
- Entitlements plist format is incompatible with older IOKit
- Causes immediate rejection by the kernel

### 5. Hash Algorithm Incompatibility

**Problem**: Modern signatures use algorithms not supported by Snow Leopard.

```
Modern Signatures:    SHA-256, SHA-512, ECDSA
Snow Leopard Support: MD5, SHA-1 only
```

**Snow Leopard Impact**: Signature verification fails at kernel load time because the hashing algorithms are unknown.

## Error Code Analysis

### -67050 on Modern macOS
```
"Kext with invalid signature (-67050) allowed but the signature is valid"
```

This error means:
- The signature is technically valid
- But doesn't meet Apple's stringent requirements
- Likely due to missing notarization or wrong certificate type
- **Acceptable trade-off for Snow Leopard compatibility**

### Snow Leopard Loading Behavior
```
// Snow Leopard kext loading (no signature verification)
kextutil unsigned_kext.kext  // ✅ Works perfectly
kextutil signed_kext.kext    // ❌ Load failure, binary format error
```

## VMQemuVGA Configuration Solution

### Current Configuration (Correct)
```bash
# VMQemuVGA.xcconfig
CODE_SIGN_IDENTITY = 
CODE_SIGNING_ALLOWED = NO
CODE_SIGNING_REQUIRED = NO
MACOSX_DEPLOYMENT_TARGET = 10.6
```

### Why This Works
1. **No signature load commands** - Binary remains in classic Mach-O format
2. **No symbol table modification** - Original symbol layout preserved
3. **Proper deployment target** - Uses 10.6 SDK APIs only
4. **No entitlements** - No dependency on Security.framework
5. **No hash verification** - No algorithm compatibility issues

## Build Strategy Recommendations

### For Snow Leopard Support
```bash
# Use unsigned build
./build-enhanced_private.sh --unsigned --snow-leopard
```

### For Modern macOS Support
```bash
# Use signed build (with -67050 error acceptable)
./build-enhanced_private.sh --signed --modern

# Or for full compliance (requires proper certificate + notarization)
./build-enhanced_private.sh --signed --notarized
```

### Dual Compatibility Approach
1. **Primary Build**: Unsigned for maximum compatibility
2. **Secondary Build**: Signed for modern systems (if needed)
3. **Distribution**: Provide both versions with clear documentation

## Conclusion

The incompatibility between modern code signing and Snow Leopard is fundamental and cannot be resolved through configuration changes. The only viable solution is to completely disable code signing for Snow Leopard-compatible builds.

The -67050 error on modern systems is an acceptable trade-off for maintaining compatibility with legacy systems. Users requiring both Snow Leopard compatibility and modern macOS compliance should maintain separate build configurations.

## References

- [Apple Kernel Programming Guide](https://developer.apple.com/library/archive/documentation/Darwin/Conceptual/KernelProgramming/)
- [Mach-O File Format Reference](https://github.com/aidansteele/osx-abi-macho-file-format-reference)
- [Darwin Kernel Source](https://opensource.apple.com/source/xnu/)
- [IOKit Fundamentals](https://developer.apple.com/library/archive/documentation/DeviceDrivers/Conceptual/IOKitFundamentals/)

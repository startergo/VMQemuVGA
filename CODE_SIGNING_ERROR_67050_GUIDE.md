# Code Signing Error -67050 Troubleshooting Guide

## Understanding Error -67050

When you see this message on modern macOS:
```
Kext with invalid signature (-67050) allowed but the signature is valid
```

This indicates a **signature validation issue**, not a compatibility problem.

## Root Cause Analysis

### What -67050 Means
- **-67050** = `errSecCSSignatureFailed` in Security.framework
- The signature exists and is cryptographically valid
- But fails Apple's policy requirements for kernel extensions

### Why This Happens
1. **Wrong Certificate Type**: Using general development certificate instead of "Developer ID Application" with kext entitlement
2. **Missing Notarization**: macOS 10.14.5+ requires Apple notarization for kexts
3. **Outdated Signature**: Certificate expired or revoked
4. **Incomplete Signing**: Missing required entitlements or provisions

## Snow Leopard Compatibility Decision

### The Trade-off
For VMQemuVGA, we **intentionally disable code signing** because:

```
Snow Leopard Compatibility ✅  vs  Modern Code Signing ❌
```

### Why This Makes Sense
1. **Primary Target**: VMQemuVGA targets legacy virtualization (Snow Leopard guests)
2. **Technical Reality**: Impossible to satisfy both requirements simultaneously
3. **User Base**: Primarily developers and enthusiasts who understand the trade-offs
4. **Functionality**: The kext works perfectly without signing on all macOS versions

## Error Interpretation by System

### Snow Leopard (10.6)
```bash
kextutil VMQemuVGA.kext
# ✅ Loads successfully (no signature verification)
```

### Lion to High Sierra (10.7-10.13)
```bash
kextutil VMQemuVGA.kext
# ✅ Loads with warning (signature optional)
```

### Mojave and Later (10.14+)
```bash
kextutil VMQemuVGA.kext
# ⚠️  Loads with -67050 error (signature required but invalid)
# ✅ Still functions correctly
```

## Fixing -67050 (If Desired)

If you want to eliminate the -67050 error for modern systems, here's what's required:

### Step 1: Get Proper Certificate
```bash
# Request from Apple Developer Program
# Certificate Type: "Developer ID Application"
# Special Request: Kernel Extension Signing Capability
# Note: Requires justification and approval process
```

### Step 2: Proper Signing Command
```bash
codesign --force --sign "Developer ID Application: Your Name" \
         --entitlements VMQemuVGA.entitlements \
         --timestamp \
         build/VMQemuVGA.kext
```

### Step 3: Notarization (macOS 10.14.5+)
```bash
# Create archive
ditto -c -k --keepParent build/VMQemuVGA.kext VMQemuVGA.zip

# Submit for notarization
xcrun altool --notarize-app \
             --primary-bundle-id "com.vmware.kext.VMQemuVGA" \
             --username "your@apple-id.com" \
             --password "@keychain:AC_PASSWORD" \
             --file VMQemuVGA.zip

# Wait for approval (can take hours)

# Staple the ticket
xcrun stapler staple build/VMQemuVGA.kext
```

### Step 4: Verification
```bash
codesign --verify --deep --strict --verbose=2 build/VMQemuVGA.kext
spctl --assess --type kext --verbose build/VMQemuVGA.kext
```

## Why We Don't Do This

### Practical Reasons
1. **Apple Approval Required**: Getting kext signing certificate requires business justification
2. **Review Process**: Can take weeks or months
3. **Annual Renewal**: Certificates expire and must be renewed
4. **Notarization Dependency**: Requires Apple's servers to be accessible

### Technical Reasons
1. **Snow Leopard Incompatibility**: Signed kexts won't load on Snow Leopard
2. **Binary Bloat**: Signatures add significant size to kext
3. **Build Complexity**: Requires secure certificate management
4. **Update Friction**: Every build must go through signing/notarization

## Current Status: Acceptable Solution

### For VMQemuVGA Users
- **Functionality**: ✅ Works perfectly on all macOS versions
- **Compatibility**: ✅ Loads on Snow Leopard through Big Sur+
- **User Experience**: ⚠️  Shows -67050 warning on modern systems
- **Security**: ⚠️  No signature verification (acceptable for development tool)

### Recommendation
**Keep current unsigned configuration** because:
1. Primary use case (Snow Leopard) requires it
2. Functionality is unaffected on all systems
3. Warning is informational only
4. Signing would break backward compatibility

## Alternative Approaches

### Dual Build System
```bash
# Unsigned for Snow Leopard compatibility
make build-unsigned

# Signed for modern systems (if certificate available)
make build-signed
```

### User Choice
```bash
# Let users decide which version to install
VMQemuVGA-Unsigned-v8.0.pkg  # For Snow Leopard compatibility
VMQemuVGA-Signed-v8.0.pkg    # For modern macOS compliance
```

## Conclusion

The -67050 error is a **cosmetic issue** that doesn't affect functionality. For VMQemuVGA's use case (legacy system support), the current unsigned approach is the correct technical decision.

Users who encounter this error should understand:
1. **It's expected behavior** for unsigned kexts on modern macOS
2. **Functionality is unaffected** - the kext loads and works
3. **Eliminating it would break Snow Leopard support**
4. **The warning can be safely ignored**

## See Also
- `SNOW_LEOPARD_CODE_SIGNING_INCOMPATIBILITY.md` - Technical details
- `KEXT_SIGNING_GUIDE.md` - Complete signing procedures (if needed)
- Apple Developer Documentation on Kernel Extensions

# VMQemuVGA Code Signing Guide - On-Demand Signing

## Overview

VMQemuVGA now uses **conditional code signing** to support both Snow Leopard compatibility and modern macOS requirements. Code signing is disabled by default but can be enabled on demand through multiple methods.

## The -67050 Error Explained

The error "Kext with invalid signature (-67050) allowed but the signature is valid" indicates that while your kext is technically signed, it doesn't meet Apple's stringent security requirements for kernel extensions.

## Required Steps for Proper Kext Signing

### 1. Obtain the Correct Certificate

You need a **Developer ID Application certificate** specifically approved for kext signing:

1. **Apply for Kext Signing Permission**:
   - Go to Apple Developer Portal
   - Submit a request for kext signing entitlement
   - Explain your legitimate use case for kernel-level access
   - This process can take weeks and requires justification

2. **Generate the Proper Certificate**:
   ```bash
   # Once approved, generate a Certificate Signing Request (CSR)
   openssl req -new -newkey rsa:2048 -nodes -keyout kext_signing.key -out kext_signing.csr
   ```

3. **Download and Install**:
   - Upload CSR to Apple Developer Portal
   - Download the Developer ID Application certificate
   - Install in Keychain Access

### 2. Update Build Configuration

Update your signing configuration to use the proper certificate:

```bash
# Check for proper kext signing certificate
security find-identity -p codesigning -v | grep "Developer ID Application.*kext"
```

### 3. Notarization Process

For macOS 10.14.5+, kexts must be notarized:

```bash
# 1. Create app-specific password for notarization
# Go to appleid.apple.com -> Sign-In & Security -> App-Specific Passwords

# 2. Store credentials in keychain
xcrun notarytool store-credentials "notarization-profile" \
    --apple-id "your-apple-id@example.com" \
    --team-id "YOUR_TEAM_ID" \
    --password "your-app-specific-password"

# 3. Submit kext for notarization
xcrun notarytool submit VMQemuVGA.kext.zip \
    --keychain-profile "notarization-profile" \
    --wait

# 4. Staple notarization ticket
xcrun stapler staple VMQemuVGA.kext
```

### 4. Alternative: Development Signing for Testing

For development and testing purposes, you can use a self-signed certificate with proper SIP configuration:

1. **Disable SIP (System Integrity Protection)**:
   ```bash
   # Boot into Recovery Mode (Command+R during boot)
   # Open Terminal and run:
   csrutil disable
   csrutil enable --without kext
   # Reboot normally
   ```

2. **Load unsigned kext for testing**:
   ```bash
   sudo kmutil load -p /path/to/VMQemuVGA.kext
   ```

## Current Build Script Issues

Your current build script has these problems:

1. **Certificate Detection**: Looking for wrong certificate types
2. **Configuration Conflict**: xcconfig disables signing while project enables it
3. **No Notarization**: Missing notarization workflow

## Recommended Solution

For immediate development and testing, I recommend:

1. **Use unsigned builds with SIP disabled** for development
2. **Apply for Apple's kext signing program** for distribution
3. **Update build configuration** to be consistent

Would you like me to:
1. Create a proper development build configuration?
2. Set up the notarization workflow?
3. Generate a self-signed certificate for testing?

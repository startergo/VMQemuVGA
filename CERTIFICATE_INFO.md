# VMQemuVGA Code Signing Certificate Documentation
**Created**: August 22, 2025  
**Purpose**: Self-signed certificate for VMQemuVGA kext code signing

## Certificate Details
- **Certificate Name**: `VMQemuVGA Code Signing Certificate`
- **SHA-1 Fingerprint**: `A59977BCC81EA18AF1D35DF374136CA1A60E9049`
- **Subject**: `C=US, ST=California, L=Cupertino, O=VMQemuVGA Development, OU=Kernel Extension Development, CN=VMQemuVGA Code Signing Certificate, emailAddress=vmqemuvga@example.com`
- **Key Type**: RSA 2048-bit
- **Validity**: 10 years (3650 days)
- **Extensions**: Code Signing, Digital Signature, Key Encipherment

## Files Generated
- `VMQemuVGA.key` - Private key (unencrypted)
- `VMQemuVGA.crt` - Certificate file
- `VMQemuVGA.p12` - PKCS#12 bundle (certificate + private key)
- `vmqemuvga.conf` - OpenSSL configuration used for generation

## Passwords & Security
- **PKCS#12 Password**: `vmqemuvga`
- **Keychain**: Imported to login keychain (no password required for code signing)
- **Build System**: Automatically detected and used by Xcode

## Usage Instructions

### For Code Signing (Automatic)
The certificate is automatically detected by the build system:
```bash
./build-enhanced.sh
```

### Manual Code Signing
```bash
codesign --force --sign "VMQemuVGA Code Signing Certificate" --entitlements VMQemuVGA.entitlements /path/to/VMQemuVGA.kext
```

### Export PKCS#12 (if needed)
Password: `vmqemuvga`
```bash
security export -k ~/Library/Keychains/login.keychain-db -t identities -f pkcs12 -o VMQemuVGA-export.p12
```

### Import PKCS#12 (if needed)
```bash
security import VMQemuVGA.p12 -k ~/Library/Keychains/login.keychain-db -P vmqemuvga -A
```

### Verify Certificate
```bash
security find-identity -p codesigning -v | grep VMQemuVGA
openssl x509 -in VMQemuVGA.crt -text -noout
```

## Build Configuration Files
- `VMQemuVGA.xcconfig` - Xcode build configuration with code signing settings
- `VMQemuVGA.entitlements` - Kext entitlements for macOS integration

## Status
✅ **Active**: Certificate successfully imported and working  
✅ **Verified**: VMQemuVGA.kext builds and signs correctly  
✅ **Ready**: For production use and distribution  

## Notes
- This is a self-signed certificate for development/distribution purposes
- Certificate is valid for 10 years from creation date
- Private key is stored securely in macOS keychain
- No password required for automated builds
- PKCS#12 password only needed for manual import/export operations

## Regeneration (if needed)
If certificate expires or needs renewal:
1. Delete existing certificate: `security delete-certificate -c "VMQemuVGA Code Signing Certificate"`
2. Re-run certificate creation process with same configuration
3. Update any hardcoded certificate references in build scripts

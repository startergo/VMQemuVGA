# Snow Leopard Build Guide: Resolving Code Signing Issues

## The Problem: Error Code -67050

The error message "Kext with invalid signature (-67050) allowed but the signature is valid" occurs because:

1. **Modern Code Signing Incompatibility**: Snow Leopard (macOS 10.6) predates Apple's kernel extension code signing requirements
2. **Binary Format Conflicts**: Code signing adds metadata and load commands that Snow Leopard's kernel cannot parse
3. **Symbol Resolution Issues**: Signed binaries contain symbols that older kernels don't recognize

## The Solution: Snow Leopard Compatible Builds

### Quick Start

For Snow Leopard compatibility, use the new build option:

```bash
./build-enhanced_private.sh --snow-leopard
```

This automatically:
- ✅ Disables all code signing (prevents -67050 error)
- ✅ Uses Snow Leopard compatible xcconfig settings
- ✅ Targets macOS 10.6 SDK and deployment target
- ✅ Builds unsigned kext that loads on Snow Leopard

### Build Options

| Command | Purpose | Code Signing | Compatibility |
|---------|---------|--------------|---------------|
| `./build-enhanced_private.sh` | Modern build (Release) | Enabled | macOS 10.14+ |
| `./build-enhanced_private.sh --unsigned` | Development build | Disabled | macOS 10.6+ |
| `./build-enhanced_private.sh --snow-leopard` | Snow Leopard build | **Disabled** | **macOS 10.6+** |
| `./build-enhanced_private.sh --debug --snow-leopard` | Snow Leopard debug | **Disabled** | **macOS 10.6+** |

## Technical Details

### What Changed

1. **Main xcconfig file** (`VMQemuVGA.xcconfig`):
   ```
   // Code Signing Configuration - DISABLED for Snow Leopard compatibility
   CODE_SIGN_IDENTITY = 
   CODE_SIGNING_ALLOWED = NO
   CODE_SIGNING_REQUIRED = NO
   ```

2. **Snow Leopard specific config** (`VMQemuVGA_10_6.xcconfig`):
   - Explicit 10.6 deployment target
   - Disabled code signing
   - Compatible compiler/linker flags
   - Proper bundle structure

3. **Enhanced build script**:
   - New `--snow-leopard` option
   - Automatic xcconfig selection
   - Clear compatibility messaging

### Why Code Signing Breaks Snow Leopard

Modern code signing adds several incompatible elements:

```
❌ LC_CODE_SIGNATURE load command (unknown to 10.6 kernel)
❌ __LINKEDIT segment modifications
❌ Codesign metadata in binary
❌ Modern certificate chain validation
❌ Notarization requirements
```

Snow Leopard expects:
```
✅ Simple Mach-O binary structure
✅ Standard kext bundle layout
✅ No code signature load commands
✅ Compatible symbol table format
```

## Development Workflow

### For Snow Leopard Development

1. **Build for Snow Leopard**:
   ```bash
   ./build-enhanced_private.sh --snow-leopard
   ```

2. **Install on Snow Leopard**:
   ```bash
   # Copy kext to Snow Leopard system
   sudo cp -R build/Release/VMQemuVGA.kext /System/Library/Extensions/
   
   # Fix permissions
   sudo chown -R root:wheel /System/Library/Extensions/VMQemuVGA.kext
   sudo chmod -R 755 /System/Library/Extensions/VMQemuVGA.kext
   
   # Load the kext
   sudo kextload /System/Library/Extensions/VMQemuVGA.kext
   ```

3. **Debug on Snow Leopard**:
   ```bash
   ./build-enhanced_private.sh --debug --snow-leopard
   ```

### For Modern macOS Development

1. **Modern signed build**:
   ```bash
   ./build-enhanced_private.sh
   ```

2. **Requires proper certificates**:
   - Developer ID Application certificate
   - Kext signing entitlement from Apple
   - Notarization for macOS 10.14.5+

## Compatibility Matrix

| macOS Version | Build Command | Code Signing | SIP Status |
|---------------|---------------|--------------|------------|
| 10.6 Snow Leopard | `--snow-leopard` | Disabled | N/A |
| 10.7-10.13 | `--unsigned` | Disabled | Disabled |
| 10.14+ | Default | Required | Disabled |

## Troubleshooting

### Error: "Kext with invalid signature (-67050)"
**Solution**: Use `--snow-leopard` build option to disable code signing

### Error: "Invalid segment type 29/42"
**Solution**: Already fixed in Snow Leopard xcconfig with proper deployment target

### Error: "Symbol not found in kernel"
**Solution**: Check `SNOW_LEOPARD_COMPATIBILITY_STATUS.md` - all symbols have stub implementations

### Build fails with signing errors
**Solution**: Use `--unsigned` or `--snow-leopard` for development builds

## Current Status

- ✅ **Phase 1 Complete**: Kext loads successfully on Snow Leopard
- ✅ **Phase 2 Complete**: All symbol resolution errors fixed
- 🔄 **Phase 3 In Progress**: Functional implementation of stub methods

---

*Last Updated: September 2, 2025*  
*Build System Version: 8.0 Enhanced with Snow Leopard Support*

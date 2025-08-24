# VMQemuVGA Compatibility Notes for macOS 10.11

## For macOS 10.11 (El Capitan) Compatibility

### Required Steps:
1. **Disable SIP (System Integrity Protection)**
   ```bash
   # Boot to Recovery Mode (Cmd+R)
   # Open Terminal and run:
   csrutil disable
   # Reboot normally
   ```

2. **Manual Kext Installation**
   ```bash
   sudo kextload -v VMQemuVGA.kext
   # May show warnings but should load
   ```

3. **Verify Loading**
   ```bash
   kextstat | grep VMQemuVGA
   # Should show the kext as loaded
   ```

### Potential Issues:
- **Warning messages** about unsigned kext
- **System logs** may show security warnings
- **Performance** should still be good
- **Shader acceleration** should work

### Not Recommended For:
- **Production systems** (security implications)
- **Main daily driver** machines
- **Systems requiring SIP protection**

### Best Use Cases:
- **Development/testing** environments
- **Virtualization workstations**
- **Legacy software compatibility**

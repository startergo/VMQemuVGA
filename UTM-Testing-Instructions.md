# VMQemuVGA v8.0 UTM Testing Guide

## 🎯 Testing Objectives

This VMQemuVGA v8.0 build specifically addresses the following issues:

### ✅ Fixed Issues:
1. **Chrome cursor flickering** - Hardware cursor implementation
2. **WebGL rendering failures** - Comprehensive browser support 
3. **Flashing yellow squares around text** - Text rendering optimization
4. **Hardcoded OpenGL versions** - Dynamic capability detection
5. **Build certificate warnings** - Clear deployment status

## 📦 Package Contents

- `VMQemuVGA.kext` - v8.0 kernel extension with text rendering fixes
- `install.sh` - Automated installation script
- `README.txt` - Basic installation instructions

## 🚀 Installation Instructions

### Prerequisites:
```bash
# Disable SIP (System Integrity Protection) in Recovery Mode
csrutil disable

# Reboot normally and verify SIP status
csrutil status
```

### Installation:
```bash
# Extract package
tar -xzf VMQemuVGA-v8.0-MassDeployment-20250824.tar.gz
cd VMQemuVGA-v8.0-MassDeployment-20250824

# Run installer (requires admin privileges)
sudo ./install.sh

# Reboot to load the driver
sudo reboot
```

## 🧪 Testing Protocol

### Phase 1: Basic Graphics Testing
1. **Boot Test**: Verify system boots without kernel panics
2. **Display Output**: Confirm display resolution and color depth
3. **Console Output**: Check for VMQemuVGA initialization messages

Expected output in Console:
```
VMQemuVGA: VMQemuVGA Phase 3 enhanced graphics driver starting
VMQemuVGA: 3D acceleration enabled via VirtIO GPU
VMQemuVGA: WebGL and hardware acceleration support enabled
```

### Phase 2: Cursor Testing (Chrome Fix)
1. **Open Chrome browser**
2. **Navigate to any webpage**
3. **Move cursor around the page**
4. **✅ EXPECTED**: Smooth cursor movement, no flickering
5. **❌ REGRESSION**: Cursor flickers or jumps

### Phase 3: WebGL Testing (Browser Compatibility)
Test with multiple browsers:

#### Chrome WebGL Test:
```
1. Open Chrome
2. Navigate to: https://get.webgl.org/
3. Expected: "Your browser supports WebGL" (green checkmark)
4. Navigate to: https://threejs.org/examples/webgl_animation_cloth.html
5. Expected: 3D cloth animation renders smoothly
```

#### Firefox WebGL Test:
```
1. Open Firefox  
2. Navigate to: https://get.webgl.org/webgl2/
3. Expected: "Your browser supports WebGL 2" (green checkmark)
4. Test 3D content rendering
```

#### Safari WebGL Test:
```
1. Open Safari
2. Navigate to WebGL demos
3. Verify hardware acceleration is active
```

### Phase 4: Text Rendering Testing (Yellow Square Fix)
1. **Open any browser**
2. **Enable Developer Tools** (F12 or Cmd+Option+I)
3. **Navigate to text-heavy webpage** (Wikipedia, news sites)
4. **Look for text rendering artifacts**
5. **✅ EXPECTED**: Clean text rendering, no yellow squares
6. **❌ REGRESSION**: Flashing yellow squares around text

### Phase 5: OpenGL Developer Tools Testing
1. **Open browser developer tools**
2. **Go to 3D/WebGL tab** (if available)
3. **Run WebGL shader tests**
4. **Monitor for text rendering issues**
5. **✅ EXPECTED**: No texture cache artifacts, clean rendering

## 📊 Performance Validation

### System Information Check:
```bash
# Check driver loading
kextstat | grep VMQemuVGA

# Check system profiler
system_profiler SPDisplaysDataType

# Check for 3D acceleration
/Applications/Utilities/OpenGL\ Profiler.app
```

### Browser Performance:
- Chrome: Navigate to `chrome://gpu/` 
- Firefox: Navigate to `about:support` → Graphics section
- Safari: Check Develop menu → Show Web Inspector → Graphics

## 🐛 Debugging & Logs

### Console Monitoring:
```bash
# Real-time kernel messages
sudo dmesg | grep -i vmqemu

# System log monitoring  
log stream --predicate 'subsystem contains "VMQemuVGA"' --info
```

### Expected Success Messages:
```
VMQemuVGA: Hardware cursor enabled
VMQemuVGA: WebGL support properties added
VMQemuVGA: Text rendering optimization applied
VMQemuVGA: OpenGL version detection completed
```

### Error Indicators:
```
VMQemuVGA: Failed to initialize 3D acceleration
VMQemuVGA: Text rendering optimization failed  
VMQemuVGA: Cursor command submission failed
```

## 📋 Test Results Checklist

- [ ] System boots successfully with VMQemuVGA loaded
- [ ] Chrome cursor flickering eliminated 
- [ ] WebGL works in Chrome browser
- [ ] WebGL works in Firefox browser
- [ ] WebGL works in Safari browser
- [ ] No yellow square text rendering artifacts
- [ ] OpenGL developer tools render cleanly
- [ ] 3D acceleration status shows enabled
- [ ] No kernel panics or system instability

## 🚑 Emergency Recovery

If system becomes unstable:

```bash
# Boot in Safe Mode (hold Shift during boot)
# Remove driver
sudo kextunload /System/Library/Extensions/VMQemuVGA.kext
sudo rm -rf /System/Library/Extensions/VMQemuVGA.kext

# Clear kernel cache
sudo kextcache -system-prelinked-kernel
sudo kextcache -system-caches

# Reboot
sudo reboot
```

## 📞 Reporting Issues

When reporting test results, include:

1. **UTM Version**: 
2. **Guest OS**: 
3. **Test Results**: Pass/Fail for each phase
4. **Console Logs**: Any VMQemuVGA messages
5. **Screenshots**: Of any rendering issues
6. **Browser Versions**: Chrome, Firefox, Safari versions tested

## 🎉 Expected Outcome

With VMQemuVGA v8.0, you should experience:
- ✅ Smooth cursor movement in all applications
- ✅ Full WebGL support across Chrome, Firefox, and Safari  
- ✅ Clean text rendering without artifacts
- ✅ Dynamic OpenGL version detection
- ✅ Stable 3D graphics acceleration

This build represents a complete solution to the reported rendering issues while maintaining broad compatibility across virtualization platforms.

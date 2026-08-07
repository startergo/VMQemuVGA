# VM FBO Rendering Test - Complete Setup Guide

## What This Tests

Renders a colored triangle **inside the Catalina VM** which gets:
1. Sent to Metal server on host M4 Pro Mac
2. Rendered to FBO texture on host GPU
3. Displayed on quad on host GPU
4. **Result visible in Metal server window on HOST**

## Architecture

```
┌─────────────────────────────────────┐
│  Catalina VM (x86_64)               │
│                                     │
│  ┌────────────────────┐             │
│  │  vm_fbo_test       │─────────┐   │
│  │  (FBO commands)    │         │   │
│  └────────────────────┘         │   │
│                                 │   │
└─────────────────────────────────┼───┘
                                  │
                           TCP Socket
                        (SSH Reverse Tunnel)
                       127.0.0.1:28123
                                  │
┌─────────────────────────────────▼───┐
│  macOS Host (M4 Pro)                │
│                                     │
│  ┌────────────────────┐             │
│  │  metal_server      │────────┐    │
│  │  (FBO rendering)   │        │    │
│  └────────────────────┘        │    │
│                                │    │
│  ┌────────────────────────────▼┐    │
│  │   Metal Window              │    │
│  │   Shows colored triangle    │    │
│  └─────────────────────────────┘    │
└─────────────────────────────────────┘
```

## Prerequisites

1. **Catalina VM** running and accessible via SSH
2. **Metal server** built on host (already done)
3. **VM test client** built for x86_64 (already done)
4. **SSH access** to VM with key file `vm-ssh-key`

## Step-by-Step Instructions

### Step 1: Start Metal Server on Host

Open **Terminal 1** on Mac:

```bash
cd /Users/macbookpro/VMQemuVGA/SharedGL
./metal/metal_server
```

You should see:
```
========================================
   SharedGL Metal Server
   OpenGL→Metal Translation POC
========================================
[Metal Server] Metal Device: Apple M4 Pro
[Metal Server] ✅ Default white texture and sampler created
[Metal Server] ✅ Metal pipeline created
[Metal Server] ✅ Server listening on 0.0.0.0:28123
```

**Leave this running!** Metal server window will appear.

---

### Step 2: Setup SSH Reverse Tunnel

Open **Terminal 2** on Mac:

```bash
cd /Users/macbookpro/VMQemuVGA
ssh -i vm-ssh-key -R 28123:localhost:28123 qemucat@qemucat.local
```

This creates a reverse tunnel so VM's `localhost:28123` forwards to host's Metal server.

**Leave this SSH session open!** You'll use it to run the test.

---

### Step 3: Transfer Test Binary to VM

Open **Terminal 3** on Mac (or reuse Terminal 2 before SSH):

```bash
cd /Users/macbookpro/VMQemuVGA/SharedGL
scp -i ../vm-ssh-key build/vm_test/vm_fbo_test qemucat@qemucat.local:~/
```

Verify transfer:
```bash
ssh -i ../vm-ssh-key qemucat@qemucat.local "ls -lh ~/vm_fbo_test"
```

---

### Step 4: Run Test in VM

In **Terminal 2** (SSH session to VM):

```bash
cd ~
./vm_fbo_test
```

**Expected Output in VM Terminal:**

```
========================================
  VM FBO Test
  Connecting to Metal server on host
========================================
[VM Test] Connecting to 127.0.0.1:28123...
[VM Test] ✅ Connected! Using host M4 Pro GPU via Metal

[VM Test] Starting FBO render test...
[VM Test] Creating FBO texture...
[VM Test] Texture ID: 1
[VM Test] Creating FBO...
[VM Test] FBO ID: 1
[VM Test] FBO status: 0x8CD5 COMPLETE
[VM Test] Creating triangle VBO...
[VM Test] Rendering triangle to FBO 1...
[VM Test] Switching to screen framebuffer...
[VM Test] Creating screen quad VBO...
[VM Test] Rendering quad with FBO texture to screen...
[VM Test] ✅ FBO render test complete!
[VM Test] You should see colored triangle in Metal server window on host

[VM Test] GLUT window created
[VM Test] Press Ctrl+C to exit
```

**Expected Output in Metal Server Terminal (Terminal 1):**

```
[Metal Server] ✅ Client connected from 127.0.0.1:xxxxx
[Metal Server] Received command: 60 (GEN_TEXTURES)
[Metal Server] Received command: 61 (BIND_TEXTURE)
[Metal Server] Received command: 80 (GEN_FRAMEBUFFERS)
[Metal Server] Received command: 81 (BIND_FRAMEBUFFER)
[Metal Server] Received command: 83 (FRAMEBUFFER_TEXTURE2D)
[Metal Server] Received command: 84 (CHECK_FRAMEBUFFER_STATUS)
[Metal Server] Received command: 20 (DRAW_ARRAYS)
[Metal Server] Rendering to FBO 1
[Metal Server] Using custom shader pipeline for program 1
[Metal Server] ✅ FBO render completed, texture ready for sampling
[Metal Server] Rendering to FBO 0 (screen)
[Metal Server] Bind texture: target=0xDE1 texture=1
[Metal Server] ✅ Frame rendered from VBO
```

**Expected Visual Output:**

In the **Metal server window on HOST**, you should see:
- **Magenta background** (FBO clear color)
- **RGB triangle**:
  - Red vertex at top
  - Green vertex at bottom-left
  - Blue vertex at bottom-right

---

## Troubleshooting

### Problem: "Failed to connect to Metal server"

**Cause**: SSH reverse tunnel not established or Metal server not running

**Solution**:
1. Check Metal server is running: `lsof -i :28123` (should show metal_server)
2. Check SSH tunnel: In VM, run `netstat -an | grep 28123`
3. Restart SSH tunnel with correct reverse tunnel flag `-R`

---

### Problem: Metal server shows no client connection

**Cause**: VM can't reach host through tunnel

**Solution**:
1. Test tunnel from VM: `nc -zv 127.0.0.1 28123`
2. If fails, restart SSH with reverse tunnel:
   ```bash
   ssh -i vm-ssh-key -R 28123:localhost:28123 qemucat@qemucat.local
   ```

---

### Problem: Connection succeeds but no rendering

**Cause**: Shader or pipeline issue in Metal server

**Solution**:
1. Check Metal server logs for errors
2. Look for "FBO render completed" message
3. Check if custom shaders are being used
4. Verify FBO status is 0x8CD5 (COMPLETE)

---

### Problem: "command not found: vm_fbo_test"

**Cause**: Binary not transferred or not executable

**Solution**:
```bash
# On host:
scp -i vm-ssh-key build/vm_test/vm_fbo_test qemucat@qemucat.local:~/

# In VM:
chmod +x ~/vm_fbo_test
./vm_fbo_test
```

---

## What Success Looks Like

✅ **VM Terminal**: Shows connection, FBO creation, rendering steps
✅ **Metal Server Terminal**: Shows received commands and render completion
✅ **Metal Server Window**: Displays colored triangle with magenta background
✅ **No Errors**: Both terminals show success messages

## Next Steps

Once this works, you've proven:
- VM → Host communication works
- FBO rendering works end-to-end
- Metal GPU acceleration accessible from VM
- Full OpenGL → Metal translation pipeline functional

You can then:
1. Inject libGL interception into VM OpenGL apps
2. Run real OpenGL applications in VM with GPU acceleration
3. Extend protocol to support more OpenGL features
4. Add performance monitoring and optimization

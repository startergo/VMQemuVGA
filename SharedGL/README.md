# SharedGL for macOS

**OpenGL Command Forwarding from VM Guest to macOS Host GPU**

This proof-of-concept demonstrates how to forward OpenGL commands from a macOS VM to the host's GPU using native Cocoa frameworks, avoiding the need for SDL2 or other cross-platform libraries.

## Architecture

```
┌─────────────────────────────────────┐
│  macOS VM (Guest)                   │
│                                     │
│  ┌───────────────────────────┐      │
│  │  Application              │      │
│  │  (uses OpenGL)            │      │
│  └───────────┬───────────────┘      │
│              │                      │
│  ┌───────────▼───────────────┐      │
│  │  libGL_hook.dylib         │      │
│  │  - Intercept OpenGL       │      │
│  │  - Forward to host        │      │
│  └───────────┬───────────────┘      │
│              │ TCP Socket           │
└──────────────┼───────────────────── ┘
               │
               │ Network (virtio-net)
               │
┌──────────────▼─────────────────────┐
│  macOS Host                        │
│                                    │
│  ┌───────────────────────────┐     │
│  │  sharedgl-server          │     │
│  │  (Cocoa NSOpenGLContext)  │     │
│  │  - Listen on port 28122   │     │
│  │  - Execute OpenGL calls   │     │
│  └───────────┬───────────────┘     │
│              │                     │
│  ┌───────────▼───────────────┐     │
│  │  Real GPU (Metal/OpenGL)  │     │
│  └───────────────────────────┘     │
└────────────────────────────────────┘
```

## Components

### 1. Server (Host)
- **File**: `server/main.m`
- **Purpose**: Runs on macOS host, executes OpenGL commands received from VM
- **Framework**: Pure Cocoa + NSOpenGLContext
- **Port**: 28122 (TCP)

### 2. Client (Guest VM)
- **File**: `client/libGL_hook.c`
- **Purpose**: Dylib injected into VM applications to intercept OpenGL calls
- **Method**: `DYLD_INSERT_LIBRARIES` for function hooking
- **Fallback**: Software rendering if host connection fails

### 3. Test Application
- **File**: `test/test_triangle.m`
- **Purpose**: Spinning triangle demo to verify OpenGL forwarding
- **Usage**: Test both with and without SharedGL acceleration

## Building

### Build Server (on macOS host):
```bash
cd SharedGL
chmod +x build_server.sh
./build_server.sh
```

Output: `build/server/sharedgl-server`

### Build Client (on macOS host or VM):
```bash
cd SharedGL
chmod +x build_client.sh
./build_client.sh
```

Output: `build/client/libGL_hook.dylib` (universal binary: x86_64)

### Build Test Application:
```bash
cd SharedGL
chmod +x build_test.sh
./build_test.sh
```

Output: `build/test/test_triangle`

## Usage

### Step 1: Start Server on Host
```bash
./build/server/sharedgl-server
```

You should see:
```
========================================
   SharedGL Server for macOS Host
   OpenGL Command Forwarding POC
========================================

[SharedGL] ✅ OpenGL Context Created:
[SharedGL]    Vendor: Apple
[SharedGL]    Renderer: AMD Radeon Pro 5500M OpenGL Engine
[SharedGL]    Version: 4.1 ATI-4.11.21
[SharedGL] ✅ Server listening on 0.0.0.0:28122
[SharedGL] Waiting for VM clients to connect...
```

### Step 2: Install Client Library in VM
```bash
# Copy libGL_hook.dylib to VM
scp build/client/libGL_hook.dylib user@vm:~/
```

### Step 3: Run Application in VM

**Without acceleration (software rendering):**
```bash
./test_triangle
```

**With SharedGL acceleration (host GPU):**
```bash
DYLD_INSERT_LIBRARIES=~/libGL_hook.dylib ./test_triangle
```

You should see in the VM:
```
========================================
  SharedGL Client for macOS Guest VM
  OpenGL Forwarding to Host GPU
========================================
[SharedGL Client] Connecting to host 10.0.2.2:28122...
[SharedGL Client] ✅ Connected to host server
[SharedGL Client] GPU acceleration enabled via host
========================================
```

And on the host server, you'll see OpenGL commands being executed:
```
[SharedGL] ✅ Client connected from 10.0.2.15:52341
[SharedGL] glClear(0x4100)
[SharedGL] glBegin(4)
[SharedGL] glColor3f(1.000000, 0.000000, 0.000000)
[SharedGL] glVertex3f(0.000000, 1.000000, 0.000000)
...
```

## Network Configuration

### Default Setup (QEMU)
- Host IP: `10.0.2.2` (QEMU default gateway)
- Port: `28122`

### Custom Network
Edit `client/libGL_hook.c`:
```c
#define HOST_IP "192.168.1.100"  // Your host IP
#define HOST_PORT 28122
```

### Firewall
Allow incoming connections on port 28122:
```bash
# macOS
sudo /usr/libexec/ApplicationFirewall/socketfilterfw --add /path/to/sharedgl-server
```

## Integration with VMQemuVGA

VMQemuVGA provides:
- ✅ Stable framebuffer display (2D)
- ✅ WindowServer integration (software rendering)
- ✅ No kernel crashes

SharedGL provides:
- ✅ Optional GPU acceleration (3D)
- ✅ User-space implementation (safe)
- ✅ Host GPU offloading (performance)

**Combined benefits:**
1. VMQemuVGA ensures display works reliably
2. SharedGL adds GPU acceleration on demand
3. No kernel-level complexity or crash risk
4. Easy to develop and debug

## Extending OpenGL Command Support

Currently implemented:
- `glBegin/glEnd`
- `glVertex3f`
- `glColor3f`
- `glClear`
- `glFlush`
- `glViewport`
- `glMatrixMode`
- `glLoadIdentity`
- `glOrtho`
- Swap buffers

To add more commands:

1. **Add opcode** in both files:
   ```c
   typedef enum {
       ...
       CMD_GL_YOUR_FUNCTION,
   } GLCommand;
   ```

2. **Add handler in server** (`server/main.m`):
   ```objc
   case CMD_GL_YOUR_FUNCTION: {
       // Receive parameters
       // Execute OpenGL call
       break;
   }
   ```

3. **Add hook in client** (`client/libGL_hook.c`):
   ```c
   void glYourFunction(params...) {
       if (g_connected) {
           send_command(CMD_GL_YOUR_FUNCTION);
           // Send parameters
       } else if (real_glYourFunction) {
           real_glYourFunction(params...);
       }
   }
   ```

## Performance Considerations

### Pros:
- ✅ Host GPU execution (much faster than software rendering)
- ✅ Network overhead minimal for typical OpenGL usage
- ✅ Cocoa native performance (no SDL2 overhead)

### Cons:
- ⚠️ Network latency (~1-2ms per command batch)
- ⚠️ Best for retained mode rendering (not immediate mode)
- ⚠️ Texture uploads need optimization

### Optimization Tips:
1. **Batch commands**: Send multiple commands before flush
2. **Use VBOs**: Minimize vertex data transfer
3. **Local framebuffer**: Keep framebuffer in VM for display
4. **Async rendering**: Don't block on every command

## Debugging

### Enable verbose logging:

**Server:**
```bash
# Already verbose by default
./build/server/sharedgl-server
```

**Client:**
Add to `libGL_hook.c`:
```c
#define DEBUG_LOG 1
// Add printf statements in each hook function
```

### Test connectivity:
```bash
# From VM, test if server is reachable
nc -v 10.0.2.2 28122
```

### Check OpenGL errors:
Both server and client check for GL errors automatically.

## Comparison with SDL2 Approach

| Feature | SharedGL (Cocoa) | Original (SDL2) |
|---------|------------------|-----------------|
| macOS Native | ✅ Yes | ❌ No |
| External Deps | ✅ None | ❌ SDL2 required |
| Binary Size | ✅ Small | ❌ +2MB |
| Metal Support | ✅ Easy | ⚠️ Limited |
| Build Complexity | ✅ Simple | ⚠️ SDL2 setup |
| Performance | ✅ Native | ✅ Good |

## Future Enhancements

1. **Full OpenGL 2.1+ support** - Add all modern OpenGL functions
2. **Texture streaming** - Optimize texture uploads
3. **Multiple contexts** - Support multiple OpenGL contexts
4. **Metal backend** - Use Metal instead of OpenGL on host
5. **Compression** - Compress command stream for network efficiency
6. **Profiling** - Add performance metrics and bottleneck detection

## Credits

- Inspired by: [dmaivel/sharedgl](https://github.com/dmaivel/sharedgl)
- macOS implementation: Pure Cocoa approach for native performance
- Integration: Designed to work with VMQemuVGA framebuffer driver

## License

Same as VMQemuVGA project.

## Support

This is a proof-of-concept. For production use:
1. Extend OpenGL command coverage
2. Add error handling and recovery
3. Implement texture and buffer management
4. Add authentication/security layer
5. Profile and optimize network protocol

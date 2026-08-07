# Phase 1: Buffer Objects (VBOs/VAOs) - Implementation Guide

## Overview

This phase implements OpenGL Buffer Objects, allowing modern applications to use VBOs instead of immediate mode rendering.

## Architecture Changes

### Server-Side State Management

The Metal server needs to maintain:
1. **Buffer Registry**: Map OpenGL buffer IDs → MTLBuffer objects
2. **VAO Registry**: Map OpenGL VAO IDs → vertex attribute configurations
3. **Current Bindings**: Track currently bound buffers and VAO

### Network Protocol

Each VBO operation sends:
1. Command opcode
2. Parameters (buffer ID, size, data, etc.)
3. Optional payload (vertex data)

---

## Implementation Steps

### Step 1: Server-Side Buffer Management

Add to `MetalServerView` interface:

```objectivec
@interface MetalServerView : MTKView <MTKViewDelegate>
// ... existing properties ...

// Phase 1: Buffer Objects
@property (nonatomic, strong) NSMutableDictionary<NSNumber*, id<MTLBuffer>> *bufferRegistry;
@property (nonatomic, strong) NSMutableDictionary<NSNumber*, NSDictionary*> *vaoRegistry;
@property (nonatomic) uint32_t currentArrayBuffer;
@property (nonatomic) uint32_t currentElementBuffer;
@property (nonatomic) uint32_t currentVAO;
@property (nonatomic) uint32_t nextBufferID;
@property (nonatomic) uint32_t nextVAOID;

@end
```

### Step 2: Initialize State

In `initWithFrame:device:`:

```objectivec
_bufferRegistry = [NSMutableDictionary dictionary];
_vaoRegistry = [NSMutableDictionary dictionary];
_currentArrayBuffer = 0;
_currentElementBuffer = 0;
_currentVAO = 0;
_nextBufferID = 1;
_nextVAOID = 1;
```

### Step 3: Implement Buffer Commands

Add to `handleCommand:socket:`:

```objectivec
case CMD_METAL_GEN_BUFFERS: {
    uint32_t count;
    if (recv(sock, &count, sizeof(count), 0) != sizeof(count)) return;
    
    uint32_t *bufferIDs = malloc(count * sizeof(uint32_t));
    for (uint32_t i = 0; i < count; i++) {
        bufferIDs[i] = _nextBufferID++;
    }
    
    // Send generated IDs back to client
    send(sock, bufferIDs, count * sizeof(uint32_t), 0);
    
    NSLog(@"[Metal Server] Generated %u buffers", count);
    free(bufferIDs);
    break;
}

case CMD_METAL_BIND_BUFFER: {
    uint32_t target, buffer;
    if (recv(sock, &target, sizeof(target), 0) != sizeof(target)) return;
    if (recv(sock, &buffer, sizeof(buffer), 0) != sizeof(buffer)) return;
    
    if (target == GL_ARRAY_BUFFER) {
        _currentArrayBuffer = buffer;
    } else if (target == GL_ELEMENT_ARRAY_BUFFER) {
        _currentElementBuffer = buffer;
    }
    
    NSLog(@"[Metal Server] Bind buffer: target=0x%X buffer=%u", target, buffer);
    break;
}

case CMD_METAL_BUFFER_DATA: {
    uint32_t target, usage;
    uint64_t size;
    
    if (recv(sock, &target, sizeof(target), 0) != sizeof(target)) return;
    if (recv(sock, &size, sizeof(size), 0) != sizeof(size)) return;
    if (recv(sock, &usage, sizeof(usage), 0) != sizeof(usage)) return;
    
    // Receive buffer data
    void *data = malloc(size);
    if (!data) {
        NSLog(@"[Metal Server] ERROR: Failed to allocate %llu bytes", size);
        return;
    }
    
    size_t totalReceived = 0;
    while (totalReceived < size) {
        ssize_t received = recv(sock, (char*)data + totalReceived, 
                               size - totalReceived, 0);
        if (received <= 0) {
            NSLog(@"[Metal Server] ERROR: Failed to receive buffer data");
            free(data);
            return;
        }
        totalReceived += received;
    }
    
    // Get current buffer ID based on target
    uint32_t bufferID = (target == GL_ARRAY_BUFFER) ? 
                        _currentArrayBuffer : _currentElementBuffer;
    
    // Create Metal buffer
    MTLResourceOptions options = (usage == GL_STATIC_DRAW) ? 
                                 MTLResourceStorageModeShared :
                                 MTLResourceStorageModeShared;
    
    id<MTLBuffer> mtlBuffer = [self.device newBufferWithBytes:data
                                                       length:size
                                                      options:options];
    
    // Store in registry
    _bufferRegistry[@(bufferID)] = mtlBuffer;
    
    NSLog(@"[Metal Server] Buffer data: target=0x%X size=%llu usage=0x%X buffer=%u", 
          target, size, usage, bufferID);
    
    free(data);
    break;
}

case CMD_METAL_DELETE_BUFFERS: {
    uint32_t count;
    if (recv(sock, &count, sizeof(count), 0) != sizeof(count)) return;
    
    uint32_t *bufferIDs = malloc(count * sizeof(uint32_t));
    if (recv(sock, bufferIDs, count * sizeof(uint32_t), 0) != count * sizeof(uint32_t)) {
        free(bufferIDs);
        return;
    }
    
    for (uint32_t i = 0; i < count; i++) {
        [_bufferRegistry removeObjectForKey:@(bufferIDs[i])];
    }
    
    NSLog(@"[Metal Server] Deleted %u buffers", count);
    free(bufferIDs);
    break;
}

case CMD_METAL_DRAW_ARRAYS: {
    uint32_t mode, first, count;
    if (recv(sock, &mode, sizeof(mode), 0) != sizeof(mode)) return;
    if (recv(sock, &first, sizeof(first), 0) != sizeof(first)) return;
    if (recv(sock, &count, sizeof(count), 0) != sizeof(count)) return;
    
    NSLog(@"[Metal Server] Draw arrays: mode=%u first=%u count=%u VAO=%u", 
          mode, first, count, _currentVAO);
    
    // TODO: Implement actual rendering from VBO
    // Need VAO configuration to know vertex layout
    
    dispatch_async(dispatch_get_main_queue(), ^{
        [self renderFrameVBO:mode first:first count:count];
    });
    break;
}
```

### Step 4: VAO Support

```objectivec
case CMD_METAL_GEN_VERTEX_ARRAYS: {
    uint32_t count;
    if (recv(sock, &count, sizeof(count), 0) != sizeof(count)) return;
    
    uint32_t *vaoIDs = malloc(count * sizeof(uint32_t));
    for (uint32_t i = 0; i < count; i++) {
        vaoIDs[i] = _nextVAOID++;
        
        // Initialize empty VAO configuration
        _vaoRegistry[@(vaoIDs[i])] = @{
            @"attributes": [NSMutableArray array],
            @"arrayBuffer": @0,
            @"elementBuffer": @0
        };
    }
    
    send(sock, vaoIDs, count * sizeof(uint32_t), 0);
    NSLog(@"[Metal Server] Generated %u VAOs", count);
    free(vaoIDs);
    break;
}

case CMD_METAL_BIND_VERTEX_ARRAY: {
    uint32_t vao;
    if (recv(sock, &vao, sizeof(vao), 0) != sizeof(vao)) return;
    
    _currentVAO = vao;
    
    // Restore buffer bindings from VAO
    if (vao > 0 && _vaoRegistry[@(vao)]) {
        NSDictionary *vaoConfig = _vaoRegistry[@(vao)];
        _currentArrayBuffer = [vaoConfig[@"arrayBuffer"] unsignedIntValue];
        _currentElementBuffer = [vaoConfig[@"elementBuffer"] unsignedIntValue];
    }
    
    NSLog(@"[Metal Server] Bind VAO: %u", vao);
    break;
}

case CMD_METAL_VERTEX_ATTRIB_POINTER: {
    uint32_t index, size, type, stride;
    uint8_t normalized;
    uint64_t offset;
    
    if (recv(sock, &index, sizeof(index), 0) != sizeof(index)) return;
    if (recv(sock, &size, sizeof(size), 0) != sizeof(size)) return;
    if (recv(sock, &type, sizeof(type), 0) != sizeof(type)) return;
    if (recv(sock, &normalized, sizeof(normalized), 0) != sizeof(normalized)) return;
    if (recv(sock, &stride, sizeof(stride), 0) != sizeof(stride)) return;
    if (recv(sock, &offset, sizeof(offset), 0) != sizeof(offset)) return;
    
    // Store in current VAO configuration
    if (_currentVAO > 0 && _vaoRegistry[@(_currentVAO)]) {
        NSMutableDictionary *vaoConfig = [_vaoRegistry[@(_currentVAO)] mutableCopy];
        NSMutableArray *attributes = [vaoConfig[@"attributes"] mutableCopy];
        
        NSDictionary *attribConfig = @{
            @"index": @(index),
            @"size": @(size),
            @"type": @(type),
            @"normalized": @(normalized),
            @"stride": @(stride),
            @"offset": @(offset),
            @"buffer": @(_currentArrayBuffer)
        };
        
        // Update or add attribute configuration
        BOOL found = NO;
        for (NSUInteger i = 0; i < attributes.count; i++) {
            if ([attributes[i][@"index"] unsignedIntValue] == index) {
                attributes[i] = attribConfig;
                found = YES;
                break;
            }
        }
        if (!found) {
            [attributes addObject:attribConfig];
        }
        
        vaoConfig[@"attributes"] = attributes;
        vaoConfig[@"arrayBuffer"] = @(_currentArrayBuffer);
        _vaoRegistry[@(_currentVAO)] = vaoConfig;
    }
    
    NSLog(@"[Metal Server] Vertex attrib pointer: index=%u size=%u type=0x%X stride=%u offset=%llu", 
          index, size, type, stride, offset);
    break;
}

case CMD_METAL_ENABLE_VERTEX_ATTRIB_ARRAY: {
    uint32_t index;
    if (recv(sock, &index, sizeof(index), 0) != sizeof(index)) return;
    
    // Mark attribute as enabled in VAO
    if (_currentVAO > 0 && _vaoRegistry[@(_currentVAO)]) {
        NSMutableDictionary *vaoConfig = [_vaoRegistry[@(_currentVAO)] mutableCopy];
        NSMutableArray *attributes = [vaoConfig[@"attributes"] mutableCopy];
        
        for (NSMutableDictionary *attr in attributes) {
            if ([attr[@"index"] unsignedIntValue] == index) {
                [attr setValue:@YES forKey:@"enabled"];
                break;
            }
        }
        
        vaoConfig[@"attributes"] = attributes;
        _vaoRegistry[@(_currentVAO)] = vaoConfig;
    }
    
    NSLog(@"[Metal Server] Enable vertex attrib array: %u", index);
    break;
}
```

---

## Client-Side Implementation

### OpenGL Function Interception

Create `gl_vbo_client.c`:

```c
#include <OpenGL/gl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Connection to Metal server
extern int g_metal_socket;
extern int g_metal_connected;

// Command opcodes (match server)
#define CMD_METAL_GEN_BUFFERS 10
#define CMD_METAL_BIND_BUFFER 11
#define CMD_METAL_BUFFER_DATA 12
#define CMD_METAL_DELETE_BUFFERS 13
#define CMD_METAL_DRAW_ARRAYS 20

static void send_metal_cmd(uint32_t cmd) {
    if (g_metal_connected && g_metal_socket >= 0) {
        send(g_metal_socket, &cmd, sizeof(cmd), 0);
    }
}

void metal_glGenBuffers(GLsizei n, GLuint *buffers) {
    // Call original OpenGL
    glGenBuffers(n, buffers);
    
    if (g_metal_connected) {
        send_metal_cmd(CMD_METAL_GEN_BUFFERS);
        uint32_t count = n;
        send(g_metal_socket, &count, sizeof(count), 0);
        
        // Receive Metal-side buffer IDs
        uint32_t *metalIDs = malloc(n * sizeof(uint32_t));
        recv(g_metal_socket, metalIDs, n * sizeof(uint32_t), 0);
        
        // Store mapping (OpenGL ID → Metal ID)
        // TODO: Implement ID mapping table
        
        free(metalIDs);
        printf("[Metal] glGenBuffers(%d)\n", n);
    }
}

void metal_glBindBuffer(GLenum target, GLuint buffer) {
    glBindBuffer(target, buffer);
    
    if (g_metal_connected) {
        send_metal_cmd(CMD_METAL_BIND_BUFFER);
        uint32_t t = target;
        uint32_t b = buffer;
        send(g_metal_socket, &t, sizeof(t), 0);
        send(g_metal_socket, &b, sizeof(b), 0);
        printf("[Metal] glBindBuffer(0x%X, %u)\n", target, buffer);
    }
}

void metal_glBufferData(GLenum target, GLsizeiptr size, const void *data, GLenum usage) {
    glBufferData(target, size, data, usage);
    
    if (g_metal_connected && data != NULL) {
        send_metal_cmd(CMD_METAL_BUFFER_DATA);
        uint32_t t = target;
        uint64_t s = size;
        uint32_t u = usage;
        
        send(g_metal_socket, &t, sizeof(t), 0);
        send(g_metal_socket, &s, sizeof(s), 0);
        send(g_metal_socket, &u, sizeof(u), 0);
        send(g_metal_socket, data, size, 0);
        
        printf("[Metal] glBufferData(0x%X, %lld bytes, 0x%X)\n", target, (long long)size, usage);
    }
}

void metal_glDrawArrays(GLenum mode, GLint first, GLsizei count) {
    // Don't call original OpenGL - Metal server will render
    
    if (g_metal_connected) {
        send_metal_cmd(CMD_METAL_DRAW_ARRAYS);
        uint32_t m = mode;
        uint32_t f = first;
        uint32_t c = count;
        
        send(g_metal_socket, &m, sizeof(m), 0);
        send(g_metal_socket, &f, sizeof(f), 0);
        send(g_metal_socket, &c, sizeof(c), 0);
        
        printf("[Metal] glDrawArrays(0x%X, %d, %d)\n", mode, first, count);
    }
}
```

---

## Testing

### Test 1: Simple VBO Triangle

```c
// Test program: test_vbo_triangle.c
GLuint vbo;
float vertices[] = {
    // position(x,y,z)   color(r,g,b,a)
    0.0f,  0.5f, 0.0f,  1.0f, 0.0f, 0.0f, 1.0f,  // top (red)
   -0.5f, -0.5f, 0.0f,  0.0f, 1.0f, 0.0f, 1.0f,  // left (green)
    0.5f, -0.5f, 0.0f,  0.0f, 0.0f, 1.0f, 1.0f   // right (blue)
};

metal_glGenBuffers(1, &vbo);
metal_glBindBuffer(GL_ARRAY_BUFFER, vbo);
metal_glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

// In render loop:
metal_glDrawArrays(GL_TRIANGLES, 0, 3);
```

### Expected Output:

Server logs:
```
[Metal Server] Generated 1 buffers
[Metal Server] Bind buffer: target=0x8892 buffer=1
[Metal Server] Buffer data: target=0x8892 size=84 usage=0x88E4 buffer=1
[Metal Server] Draw arrays: mode=4 first=0 count=3 VAO=0
[Metal Server] ✅ Frame rendered from VBO (3 vertices)
```

---

## Next Steps

1. **Implement VAO rendering**: Use vertex attribute configuration to set up Metal vertex descriptors
2. **Add glDrawElements**: Support indexed rendering
3. **Optimize buffer uploads**: Use streaming for dynamic buffers
4. **Test with real apps**: Try simple VBO-based OpenGL tutorials

---

## Performance Considerations

### Network Bandwidth:
- Buffer uploads can be large (MB of vertex data)
- Solution: Compress vertex data (zlib, LZ4)
- Solution: Cache buffers server-side, only upload deltas

### Metal Pipeline:
- Creating MTLBuffer per upload is expensive
- Solution: Pool and reuse buffers
- Solution: Use triple-buffering for dynamic buffers

### State Tracking:
- VAO state can be complex (many attributes)
- Solution: Cache VAO configurations server-side
- Solution: Only update changed attributes

---

## Compatibility

This implementation supports:
- ✅ OpenGL 3.0+ core profile VBOs
- ✅ OpenGL ES 2.0+ VBOs
- ✅ Simple vertex layouts (position, color, texcoord, normal)
- ⚠️ Interleaved and non-interleaved data
- ❌ Complex attribute types (GL_INT, GL_DOUBLE) - only GL_FLOAT supported initially

---

**Status**: Ready to implement
**Estimated completion**: 2-3 days
**Blockers**: None - all dependencies available

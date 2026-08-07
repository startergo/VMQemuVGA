# OpenGL → Metal Translation - Implementation Roadmap

**Goal**: Gradually implement OpenGL features to support real-world applications

## Current Status (v1.0 - POC)

✅ **Implemented (3 commands)**:
- `glClear()` - Clear framebuffer
- `glBegin/glVertex/glEnd()` - Immediate mode rendering
- `glViewport()` - Set viewport dimensions

❌ **Coverage**: ~2% of OpenGL 2.1, ~0.5% of OpenGL 3.3+

---

## Phase 1: Buffer Objects (VBOs/VAOs) - ESSENTIAL
**Estimated effort**: 2-3 days | **Lines of code**: ~600

### OpenGL Functions to Implement:
- `glGenBuffers(n, buffers)` - Generate buffer IDs
- `glBindBuffer(target, buffer)` - Bind buffer for operations
- `glBufferData(target, size, data, usage)` - Upload vertex data
- `glDeleteBuffers(n, buffers)` - Cleanup buffers
- `glGenVertexArrays(n, arrays)` - Generate VAO IDs
- `glBindVertexArray(array)` - Bind VAO
- `glDeleteVertexArrays(n, arrays)` - Cleanup VAOs
- `glVertexAttribPointer(index, size, type, normalized, stride, offset)` - Define vertex layout
- `glEnableVertexAttribArray(index)` - Enable attribute
- `glDisableVertexAttribArray(index)` - Disable attribute
- `glDrawArrays(mode, first, count)` - Draw from VBO
- `glDrawElements(mode, count, type, indices)` - Draw indexed geometry

### Metal Implementation:
- Map OpenGL buffer IDs to MTLBuffer objects
- Track buffer bindings (GL_ARRAY_BUFFER, GL_ELEMENT_ARRAY_BUFFER)
- Implement VAO state tracking (vertex attribute configuration)
- Network protocol: Send buffer creation/update commands
- Server-side: Manage MTLBuffer lifecycle

### Expected Application Support:
- ✅ Modern tutorials (learnopengl.com style)
- ✅ Simple SDL2/GLFW apps with VBOs
- ✅ Basic 3D model viewers

### New Commands (12):
```c
CMD_METAL_GEN_BUFFERS
CMD_METAL_BIND_BUFFER
CMD_METAL_BUFFER_DATA
CMD_METAL_DELETE_BUFFERS
CMD_METAL_GEN_VERTEX_ARRAYS
CMD_METAL_BIND_VERTEX_ARRAY
CMD_METAL_DELETE_VERTEX_ARRAYS
CMD_METAL_VERTEX_ATTRIB_POINTER
CMD_METAL_ENABLE_VERTEX_ATTRIB_ARRAY
CMD_METAL_DISABLE_VERTEX_ATTRIB_ARRAY
CMD_METAL_DRAW_ARRAYS
CMD_METAL_DRAW_ELEMENTS
```

---

## Phase 2: Shaders (GLSL) - CRITICAL
**Estimated effort**: 4-5 days | **Lines of code**: ~1000

### OpenGL Functions:
- `glCreateShader(type)` - Create vertex/fragment shader
- `glShaderSource(shader, count, string, length)` - Set shader source
- `glCompileShader(shader)` - Compile GLSL
- `glGetShaderiv(shader, pname, params)` - Get compile status
- `glGetShaderInfoLog(shader, maxLength, length, infoLog)` - Get errors
- `glDeleteShader(shader)` - Cleanup shader
- `glCreateProgram()` - Create shader program
- `glAttachShader(program, shader)` - Attach shader to program
- `glLinkProgram(program)` - Link program
- `glGetProgramiv(program, pname, params)` - Get link status
- `glGetProgramInfoLog(program, maxLength, length, infoLog)` - Get link errors
- `glUseProgram(program)` - Activate shader program
- `glDeleteProgram(program)` - Cleanup program
- `glGetUniformLocation(program, name)` - Get uniform location
- `glUniform1f/2f/3f/4f()` - Set float uniforms
- `glUniform1i/2i/3i/4i()` - Set int uniforms
- `glUniformMatrix4fv()` - Set matrix uniforms

### Metal Implementation:
- **GLSL → MSL translator** (biggest challenge!)
  - Parse GLSL vertex/fragment shaders
  - Convert to Metal Shading Language
  - Handle attribute/uniform mapping
- Manage MTLLibrary and MTLFunction objects
- Create MTLRenderPipelineState from shader program
- Track uniform locations and values
- Network protocol: Send shader source, receive compiled state

### Expected Application Support:
- ✅ Most modern OpenGL apps
- ✅ Simple games with custom shaders
- ✅ Educational graphics projects

### Complexity Note:
GLSL → MSL translation is non-trivial. Options:
1. **Simple approach**: Support basic shader patterns, reject complex ones
2. **Advanced approach**: Use SPIRV-Cross or similar library
3. **Hybrid**: Translate common patterns, fallback to software for complex

---

## Phase 3: Textures - HIGH PRIORITY
**Estimated effort**: 3-4 days | **Lines of code**: ~800

### OpenGL Functions:
- `glGenTextures(n, textures)` - Generate texture IDs
- `glBindTexture(target, texture)` - Bind texture
- `glTexImage2D(target, level, internalformat, width, height, border, format, type, data)` - Upload texture
- `glTexSubImage2D()` - Update texture region
- `glTexParameteri/f()` - Set texture parameters (wrap, filter)
- `glGenerateMipmap(target)` - Generate mipmaps
- `glActiveTexture(texture)` - Select texture unit
- `glDeleteTextures(n, textures)` - Cleanup textures

### Metal Implementation:
- Map OpenGL texture IDs to MTLTexture objects
- Track texture bindings per texture unit
- Implement texture format conversion (OpenGL → Metal)
- Network protocol: Stream texture data efficiently
- Handle compressed textures (DXT, PVRTC, etc.)
- Mipmap generation on GPU

### Expected Application Support:
- ✅ Textured 3D models
- ✅ 2D sprite games
- ✅ UI rendering
- ✅ Most real-world apps

---

## Phase 4: Render State Management - IMPORTANT
**Estimated effort**: 2-3 days | **Lines of code**: ~500

### OpenGL Functions:
- `glEnable/glDisable()` - Enable/disable features
  - GL_DEPTH_TEST, GL_BLEND, GL_CULL_FACE, GL_SCISSOR_TEST
- `glDepthFunc(func)` - Set depth test function
- `glDepthMask(flag)` - Enable/disable depth writes
- `glBlendFunc(sfactor, dfactor)` - Set blend function
- `glBlendEquation(mode)` - Set blend equation
- `glCullFace(mode)` - Set culling mode
- `glFrontFace(mode)` - Set front face winding
- `glPolygonMode(face, mode)` - Set polygon fill mode
- `glLineWidth(width)` - Set line width
- `glPointSize(size)` - Set point size

### Metal Implementation:
- Track OpenGL state machine
- Map state to MTLRenderPipelineDescriptor
- Map state to MTLDepthStencilDescriptor
- Recreate pipeline states when state changes
- Cache pipeline states for performance

---

## Phase 5: Framebuffer Objects (FBOs) - ADVANCED
**Estimated effort**: 3-4 days | **Lines of code**: ~700

### OpenGL Functions:
- `glGenFramebuffers(n, framebuffers)`
- `glBindFramebuffer(target, framebuffer)`
- `glFramebufferTexture2D(target, attachment, textarget, texture, level)`
- `glFramebufferRenderbuffer(target, attachment, renderbuffertarget, renderbuffer)`
- `glCheckFramebufferStatus(target)`
- `glDeleteFramebuffers(n, framebuffers)`
- `glGenRenderbuffers(n, renderbuffers)`
- `glBindRenderbuffer(target, renderbuffer)`
- `glRenderbufferStorage(target, internalformat, width, height)`
- `glDeleteRenderbuffers(n, renderbuffers)`

### Metal Implementation:
- Map FBOs to MTLTexture render targets
- Support render-to-texture workflows
- Handle depth/stencil attachments
- Multi-render target support

---

## Phase 6: Advanced Features - OPTIONAL
**Estimated effort**: 5+ days | **Lines of code**: ~1000+

### Features:
- Geometry shaders (if translatable to Metal)
- Compute shaders → Metal compute pipelines
- Transform feedback
- Instanced rendering (`glDrawArraysInstanced`, `glDrawElementsInstanced`)
- Uniform buffer objects (UBOs)
- Query objects (occlusion queries, timer queries)
- Sampler objects
- Pixel buffer objects (PBOs)

---

## Testing Strategy

### For Each Phase:
1. **Unit tests**: Test individual functions
2. **Integration tests**: Test function combinations
3. **Real-world apps**: Test with actual applications
4. **Performance tests**: Measure network overhead

### Test Applications (Progressive):
- **Phase 1**: Spinning cube with VBO (no shaders)
- **Phase 2**: Spinning cube with custom shader
- **Phase 3**: Textured spinning cube
- **Phase 4**: Multiple cubes with depth test, blending
- **Phase 5**: Render-to-texture post-processing
- **Phase 6**: Complex 3D scene (Sponza, etc.)

---

## Performance Optimization (Post-Implementation)

### Network Optimization:
- Command batching: Send multiple commands per network call
- Data compression: Compress texture/buffer uploads
- Delta updates: Only send changed state
- Command queuing: Pipeline commands asynchronously

### Metal Optimization:
- Pipeline state caching: Reuse compiled pipelines
- Buffer pooling: Reuse MTLBuffer objects
- Texture streaming: Async texture uploads
- Multi-threading: Parallel command encoding

### Zero-Copy Goals:
- IOSurface integration for texture sharing
- Shared memory for buffer data
- Direct DMA from VM to host GPU (future)

---

## Milestone Goals

### Milestone 1 (End of Phase 1):
- Support simple VBO-based apps
- ~50% of OpenGL 2.1 tutorials work

### Milestone 2 (End of Phase 2):
- Support shader-based apps
- ~70% of OpenGL 3.3+ tutorials work

### Milestone 3 (End of Phase 3):
- Support textured apps
- ~85% of real-world apps work

### Milestone 4 (End of Phase 4):
- Support complex scenes
- ~95% of OpenGL 3.3 core profile works

### Milestone 5 (End of Phase 5):
- Support post-processing effects
- Production-ready for most use cases

---

## Current Implementation Plan

**NEXT**: Start Phase 1 - Buffer Objects (VBOs/VAOs)

Let's begin with the foundational VBO support that modern apps require.

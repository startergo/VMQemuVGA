# Legacy OpenGL Acceleration Plan

## Goal
Make legacy OpenGL 1.x/2.x fixed-function pipeline calls (glBegin/glEnd, glMatrix, glLight, etc.) render via Metal GPU acceleration instead of software rendering.

## Current Status
- ✅ **Pass-through stubs implemented** - Functions exist but call local OpenGL (software rendering)
- ✅ **Modern OpenGL accelerated** - Buffers, VAOs, shaders work via Metal
- ✅ **Phase 7.1 COMPLETE** - Matrix stack tracking, client-side matrix math, CMD_METAL_FIXED_FUNCTION_DRAW
- ✅ **Phase 7.2 COMPLETE** - Server-side fixed-function pipeline, basic MVP rendering without lighting
- ⏳ **Phase 7.3 TODO** - Lighting state tracking and Phong shading

## Architecture: Fixed-Function to Programmable Pipeline Translation

### Phase 1: State Tracking (Client Side)

Track complete OpenGL fixed-function state:

```c
// Matrix stacks
typedef struct {
    float matrices[32][16];  // Stack of 4x4 matrices
    int depth;
} MatrixStack;

static MatrixStack g_modelviewStack;
static MatrixStack g_projectionStack;
static MatrixStack g_textureStack;
static GLenum g_matrixMode = GL_MODELVIEW;

// Material properties
typedef struct {
    float ambient[4];
    float diffuse[4];
    float specular[4];
    float emission[4];
    float shininess;
} Material;

static Material g_frontMaterial;
static Material g_backMaterial;

// Lighting state
typedef struct {
    GLboolean enabled;
    float position[4];
    float ambient[4];
    float diffuse[4];
    float specular[4];
} Light;

static Light g_lights[8];  // GL_LIGHT0 to GL_LIGHT7
static GLboolean g_lightingEnabled = GL_FALSE;

// Texture state
static GLboolean g_texture2DEnabled = GL_FALSE;
static GLuint g_currentTexture2D = 0;

// Fog state
static GLboolean g_fogEnabled = GL_FALSE;
static float g_fogColor[4];
static float g_fogDensity;

// Vertex attributes (immediate mode)
typedef struct {
    float position[3];
    float color[4];
    float normal[3];
    float texcoord[2];
} ImmediateVertex;

static ImmediateVertex g_currentVertex;
static ImmediateVertex g_vertexBatch[MAX_VERTICES];
static int g_vertexBatchCount = 0;
```

### Phase 2: Matrix Operations (Accelerated)

Implement matrix math on client side:

```c
void my_glLoadIdentity(void) {
    MatrixStack *stack = getCurrentMatrixStack();
    loadIdentityMatrix(stack->matrices[stack->depth]);
    
    // No need to send to server yet - batch until draw call
    glLoadIdentity();
}

void my_glRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z) {
    MatrixStack *stack = getCurrentMatrixStack();
    float rotation[16];
    buildRotationMatrix(rotation, angle, x, y, z);
    multiplyMatrices(stack->matrices[stack->depth], rotation);
    
    glRotatef(angle, x, y, z);
}

void my_glTranslatef(GLfloat x, GLfloat y, GLfloat z) {
    MatrixStack *stack = getCurrentMatrixStack();
    float translation[16];
    buildTranslationMatrix(translation, x, y, z);
    multiplyMatrices(stack->matrices[stack->depth], translation);
    
    glTranslatef(x, y, z);
}
```

### Phase 3: Immediate Mode Batching (Accelerated)

Accumulate full vertex attributes:

```c
void my_glBegin(GLenum mode) {
    g_currentPrimitive = mode;
    g_vertexBatchCount = 0;
    
    // Reset current vertex to defaults
    g_currentVertex.color[0] = 1.0f;
    g_currentVertex.color[1] = 1.0f;
    g_currentVertex.color[2] = 1.0f;
    g_currentVertex.color[3] = 1.0f;
    g_currentVertex.normal[0] = 0.0f;
    g_currentVertex.normal[1] = 0.0f;
    g_currentVertex.normal[2] = 1.0f;
    g_currentVertex.texcoord[0] = 0.0f;
    g_currentVertex.texcoord[1] = 0.0f;
    
    glBegin(mode);
}

void my_glColor4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a) {
    g_currentVertex.color[0] = r;
    g_currentVertex.color[1] = g;
    g_currentVertex.color[2] = b;
    g_currentVertex.color[3] = a;
    glColor4f(r, g, b, a);
}

void my_glNormal3f(GLfloat nx, GLfloat ny, GLfloat nz) {
    g_currentVertex.normal[0] = nx;
    g_currentVertex.normal[1] = ny;
    g_currentVertex.normal[2] = nz;
    glNormal3f(nx, ny, nz);
}

void my_glTexCoord2f(GLfloat s, GLfloat t) {
    g_currentVertex.texcoord[0] = s;
    g_currentVertex.texcoord[1] = t;
    glTexCoord2f(s, t);
}

void my_glVertex3f(GLfloat x, GLfloat y, GLfloat z) {
    // Capture complete vertex with all attributes
    g_currentVertex.position[0] = x;
    g_currentVertex.position[1] = y;
    g_currentVertex.position[2] = z;
    
    g_vertexBatch[g_vertexBatchCount++] = g_currentVertex;
    
    glVertex3f(x, y, z);
}
```

### Phase 4: Dynamic Shader Generation (Server Side)

Server generates Metal shaders based on active state:

```c
void my_glEnd(void) {
    if (g_connected && g_vertexBatchCount > 0) {
        // Send state snapshot to server
        send_metal_command(CMD_METAL_FIXED_FUNCTION_DRAW);
        
        // Send matrices
        send_data(g_modelviewStack.matrices[g_modelviewStack.depth], 64);
        send_data(g_projectionStack.matrices[g_projectionStack.depth], 64);
        
        // Send state flags
        send_u8(g_lightingEnabled);
        send_u8(g_texture2DEnabled);
        send_u8(g_fogEnabled);
        
        // Send lighting parameters (if enabled)
        if (g_lightingEnabled) {
            for (int i = 0; i < 8; i++) {
                if (g_lights[i].enabled) {
                    send_data(&g_lights[i], sizeof(Light));
                }
            }
            send_data(&g_frontMaterial, sizeof(Material));
        }
        
        // Send texture binding (if enabled)
        if (g_texture2DEnabled) {
            send_u32(g_currentTexture2D);
        }
        
        // Send primitive type and vertex data
        send_u32(g_currentPrimitive);
        send_u32(g_vertexBatchCount);
        send_data(g_vertexBatch, g_vertexBatchCount * sizeof(ImmediateVertex));
    }
    
    glEnd();
    g_vertexBatchCount = 0;
}
```

Server generates appropriate Metal shader:

```objective-c
// In metal_server.m
case CMD_METAL_FIXED_FUNCTION_DRAW: {
    // Receive state
    float modelview[16], projection[16];
    recv(sock, modelview, 64, 0);
    recv(sock, projection, 64, 0);
    
    uint8_t lightingEnabled = recv_u8();
    uint8_t textureEnabled = recv_u8();
    uint8_t fogEnabled = recv_u8();
    
    // Generate shader source based on state
    NSString *vertexShader = [self generateFixedFunctionVertexShader:
        lightingEnabled texture:textureEnabled fog:fogEnabled];
    NSString *fragmentShader = [self generateFixedFunctionFragmentShader:
        lightingEnabled texture:textureEnabled fog:fogEnabled];
    
    // Cache compiled shaders by state hash
    uint32_t stateHash = (lightingEnabled << 2) | (textureEnabled << 1) | fogEnabled;
    id<MTLRenderPipelineState> pipeline = _fixedFunctionPipelines[@(stateHash)];
    
    if (!pipeline) {
        // Compile and cache new pipeline
        pipeline = [self compileFixedFunctionPipeline:vertexShader 
                                              fragment:fragmentShader];
        _fixedFunctionPipelines[@(stateHash)] = pipeline;
    }
    
    // Upload matrices and lighting parameters as uniforms
    // Draw with Metal pipeline
}
```

### Phase 5: Shader Templates

Example generated Metal vertex shader:

```metal
struct Uniforms {
    float4x4 modelview;
    float4x4 projection;
    float4 lightPosition[8];
    float4 lightAmbient[8];
    // ... more lighting params
};

vertex VertexOut fixedFunctionVertex(
    VertexIn in [[stage_in]],
    constant Uniforms &uniforms [[buffer(1)]]
) {
    VertexOut out;
    
    // Transform position
    float4 eyePos = uniforms.modelview * float4(in.position, 1.0);
    out.position = uniforms.projection * eyePos;
    
    #if LIGHTING_ENABLED
        // Calculate lighting per-vertex (Gouraud shading)
        float3 normal = normalize((uniforms.modelview * float4(in.normal, 0.0)).xyz);
        float3 lightDir = normalize(uniforms.lightPosition[0].xyz - eyePos.xyz);
        float diff = max(dot(normal, lightDir), 0.0);
        out.color = in.color * (uniforms.lightAmbient[0] + diff * uniforms.lightDiffuse[0]);
    #else
        out.color = in.color;
    #endif
    
    #if TEXTURE_ENABLED
        out.texcoord = in.texcoord;
    #endif
    
    return out;
}
```

## Implementation Phases

### Phase 7.1: Matrix Stack ✅ COMPLETE
- ✅ Implement full matrix math on client
- ✅ Track modelview/projection/texture stacks
- ✅ Send matrices as uniforms at draw time
- **Status**: libGLMetal.dylib builds successfully, matrix operations accelerated

### Phase 7.2: Server-Side Fixed-Function Rendering ✅ COMPLETE
- ✅ Expand vertex batch to include normal/texcoord (48 bytes: pos3+col4+norm3+tex2)
- ✅ Accumulate complete ImmediateVertex structures
- ✅ Send full vertex data to server via CMD_METAL_FIXED_FUNCTION_DRAW
- ✅ Server receives matrices + vertices
- ✅ Basic Metal shaders created (MVP transform + color pass-through)
- ✅ Vertex descriptor configured correctly
- ✅ Pipeline state created and cached
- ✅ Rendering to default framebuffer and FBOs
- **Status**: metal_server builds successfully, immediate mode rendering functional
- **Limitation**: No lighting (normals ignored), no texturing (texcoords ignored)
- **See**: SharedGL/metal/PHASE7_2_COMPLETE.md for full details

### Phase 7.3: Lighting State (3-4 days) ⏳ TODO
- Track all 8 lights + material properties
- Send lighting params to server
- Implement Phong/Blinn-Phong in Metal fragment shader

### Phase 7.4: Dynamic Shader Generation (4-5 days)
- Server generates Metal shaders based on state
- Cache compiled pipelines by state hash
- Support combinations: lighting on/off, texture on/off, fog on/off

### Phase 7.5: State Management Commands (2-3 days)
- Add CMD_METAL_ENABLE/DISABLE commands
- Track blend func, depth func, cull face
- Configure Metal pipeline accordingly

## Performance Considerations

**Pros:**
- Legacy apps get full GPU acceleration
- No need to rewrite app code
- Maintains OpenGL API compatibility

**Cons:**
- Dynamic shader compilation overhead (mitigated by caching)
- State synchronization bandwidth (matrices + lights every draw)
- CPU overhead for matrix math on client

**Optimization:**
- Cache compiled pipelines aggressively (only ~256 possible state combinations)
- Only send changed state (delta updates)
- Batch multiple glBegin/glEnd calls before flushing to server

## Compatibility

This approach supports:
- ✅ OpenGL 1.x immediate mode (glBegin/glVertex/glEnd)
- ✅ OpenGL 1.x fixed-function lighting
- ✅ OpenGL 1.x matrix transforms
- ✅ OpenGL 1.x texturing
- ✅ Display lists (record commands, replay later)
- ❌ OpenGL 1.x evaluators (rarely used)
- ❌ OpenGL 1.x selection/feedback modes (rarely used)

## Testing

Create test apps for:
1. Simple rotating cube with vertex colors
2. Textured cube with glTexCoord
3. Lit sphere with glLight + glMaterial
4. Complex scene with multiple lights + textures

## Estimated Effort

**Total: 12-17 days of development**

- Phase 7.1: Matrix stack (2 days)
- Phase 7.2: Immediate mode batching (3 days)
- Phase 7.3: Lighting (4 days)
- Phase 7.4: Shader generation (5 days)
- Phase 7.5: State commands (3 days)

**After completion:**
- ANY legacy OpenGL application can use M4 Pro GPU
- Complete transparent acceleration
- No app modifications needed

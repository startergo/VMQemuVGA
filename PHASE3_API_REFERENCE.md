# VMQemuVGA Phase 3 API Reference

## Complete API Documentation

This document provides comprehensive API documentation for all Phase 3 components, including function signatures, parameters, return values, and usage examples.

## VMPhase3Manager API

### Core Management Functions

#### `initializePhase3Components()`
Initializes all Phase 3 acceleration components.

**Signature:**
```cpp
IOReturn initializePhase3Components();
```

**Return Values:**
- `kIOReturnSuccess`: All components initialized successfully
- `kIOReturnError`: Critical component initialization failed
- `kIOReturnNoMemory`: Insufficient memory for initialization

**Usage Example:**
```cpp
VMPhase3Manager* manager = VMPhase3Manager::withDefaults();
IOReturn result = manager->initializePhase3Components();
if (result == kIOReturnSuccess) {
    IOLog("Phase 3 initialization successful");
}
```

#### `getPerformanceTier()`
Returns the current performance tier based on available components.

**Signature:**
```cpp
VMPerformanceTier getPerformanceTier() const;
```

**Return Values:**
- `kVMPerformanceTierHigh`: Metal acceleration available
- `kVMPerformanceTierMedium`: OpenGL acceleration available
- `kVMPerformanceTierLow`: Software rendering only

#### `enableDebugMode(bool enable)`
Enables or disables comprehensive debug logging.

**Signature:**
```cpp
void enableDebugMode(bool enable);
```

**Parameters:**
- `enable`: True to enable debug mode, false to disable

**Usage Example:**
```cpp
manager->enableDebugMode(true);  // Enable debug logging
```

### Component Access Functions

#### `getMetalBridge()`
Returns the Metal bridge component for direct Metal API access.

**Signature:**
```cpp
VMMetalBridge* getMetalBridge() const;
```

**Return Value:**
- Pointer to Metal bridge component, or NULL if not available

#### `getOpenGLBridge()`
Returns the OpenGL bridge component for OpenGL compatibility.

**Signature:**
```cpp
VMOpenGLBridge* getOpenGLBridge() const;
```

**Return Value:**
- Pointer to OpenGL bridge component, or NULL if not available

#### `getIOSurfaceManager()`
Returns the IOSurface manager for surface operations.

**Signature:**
```cpp
VMIOSurfaceManager* getIOSurfaceManager() const;
```

**Return Value:**
- Pointer to IOSurface manager component, or NULL if not available

## VMIOSurfaceManager API

### Surface Creation and Management

#### `createSurface(width, height, format)`
Creates a new IOSurface with specified dimensions and format.

**Signature:**
```cpp
IOReturn createSurface(uint32_t width, uint32_t height, 
                      VMPixelFormat format, 
                      VMIOSurfaceID* surface_id);
```

**Parameters:**
- `width`: Surface width in pixels
- `height`: Surface height in pixels
- `format`: Pixel format (RGBA8, BGRA8, etc.)
- `surface_id`: Output parameter for created surface ID

**Return Values:**
- `kIOReturnSuccess`: Surface created successfully
- `kIOReturnNoMemory`: Insufficient memory for surface
- `kIOReturnBadArgument`: Invalid parameters

**Usage Example:**
```cpp
VMIOSurfaceID surface_id;
IOReturn result = manager->createSurface(1920, 1080, 
                                        kVMPixelFormatRGBA8, 
                                        &surface_id);
if (result == kIOReturnSuccess) {
    IOLog("Created surface ID: %u", surface_id);
}
```

#### `destroySurface(surface_id)`
Destroys an existing IOSurface and frees its resources.

**Signature:**
```cpp
IOReturn destroySurface(VMIOSurfaceID surface_id);
```

**Parameters:**
- `surface_id`: ID of surface to destroy

**Return Values:**
- `kIOReturnSuccess`: Surface destroyed successfully
- `kIOReturnNotFound`: Surface ID not found

#### `lockSurface(surface_id, options)`
Locks a surface for direct memory access.

**Signature:**
```cpp
IOReturn lockSurface(VMIOSurfaceID surface_id, 
                    VMIOSurfaceLockOptions options,
                    void** base_address);
```

**Parameters:**
- `surface_id`: ID of surface to lock
- `options`: Lock options (read-only, read-write, etc.)
- `base_address`: Output parameter for surface memory address

**Lock Options:**
- `kVMIOSurfaceLockReadOnly`: Read-only access
- `kVMIOSurfaceLockReadWrite`: Read-write access
- `kVMIOSurfaceLockAvoidSync`: Skip CPU-GPU synchronization

#### `unlockSurface(surface_id)`
Unlocks a previously locked surface.

**Signature:**
```cpp
IOReturn unlockSurface(VMIOSurfaceID surface_id);
```

### Surface Property Management

#### `setSurfaceProperty(surface_id, key, value)`
Sets a property on an existing surface.

**Signature:**
```cpp
IOReturn setSurfaceProperty(VMIOSurfaceID surface_id,
                           const char* key,
                           OSObject* value);
```

**Parameters:**
- `surface_id`: Target surface ID
- `key`: Property key string
- `value`: Property value object

**Common Properties:**
- `"IOSurfaceName"`: Surface name string
- `"IOSurfaceCacheMode"`: Cache mode setting
- `"IOSurfaceMemorySize"`: Total memory size

**Usage Example:**
```cpp
OSString* name = OSString::withCString("MyTexture");
manager->setSurfaceProperty(surface_id, "IOSurfaceName", name);
name->release();
```

#### `getSurfaceProperty(surface_id, key)`
Retrieves a property from a surface.

**Signature:**
```cpp
OSObject* getSurfaceProperty(VMIOSurfaceID surface_id,
                            const char* key);
```

**Return Value:**
- Property value object, or NULL if property doesn't exist

## VMCoreAnimationAccelerator API

### Compositor Control

#### `setupHardwareAcceleration()`
Initializes hardware acceleration for Core Animation.

**Signature:**
```cpp
IOReturn setupHardwareAcceleration();
```

**Return Values:**
- `kIOReturnSuccess`: Hardware acceleration enabled
- `kIOReturnUnsupported`: Hardware acceleration not available
- `kIOReturnError`: Setup failed

#### `activateCompositor()`
Activates the hardware compositor with 60fps frame timing.

**Signature:**
```cpp
IOReturn activateCompositor();
```

**Return Values:**
- `kIOReturnSuccess`: Compositor activated successfully
- `kIOReturnError`: Compositor activation failed

#### `deactivateCompositor()`
Deactivates the compositor and reports statistics.

**Signature:**
```cpp
IOReturn deactivateCompositor();
```

### Performance Monitoring

#### `getCompositorStats()`
Retrieves compositor performance statistics.

**Signature:**
```cpp
IOReturn getCompositorStats(VMCompositorStats* stats);
```

**VMCompositorStats Structure:**
```cpp
typedef struct {
    uint64_t layers_rendered;     // Total layers rendered
    uint64_t frame_drops;         // Dropped frames
    uint64_t average_frame_time;  // Average frame time (µs)
    bool compositor_active;       // Current compositor state
} VMCompositorStats;
```

## VMMetalBridge API

### Metal Device Management

#### `initializeMetalDevice()`
Initializes the virtual Metal device interface.

**Signature:**
```cpp
IOReturn initializeMetalDevice();
```

**Return Values:**
- `kIOReturnSuccess`: Metal device initialized
- `kIOReturnUnsupported`: Metal not supported on this system
- `kIOReturnError`: Initialization failed

#### `getMetalCapabilities()`
Returns the capabilities of the virtual Metal device.

**Signature:**
```cpp
IOReturn getMetalCapabilities(VMMetalCapabilities* capabilities);
```

**VMMetalCapabilities Structure:**
```cpp
typedef struct {
    uint32_t max_texture_size;
    uint32_t max_buffer_size;
    bool supports_compute;
    bool supports_tessellation;
    uint32_t shader_model_version;
} VMMetalCapabilities;
```

### Command Submission

#### `submitMetalCommands(commands, count)`
Submits Metal commands for execution.

**Signature:**
```cpp
IOReturn submitMetalCommands(VMMetalCommand* commands, 
                            uint32_t count);
```

**Parameters:**
- `commands`: Array of Metal commands
- `count`: Number of commands in array

## VMOpenGLBridge API

### OpenGL Context Management

#### `createContext(version)`
Creates a new OpenGL context with specified version.

**Signature:**
```cpp
IOReturn createContext(VMOpenGLVersion version, 
                      VMOpenGLContextID* context_id);
```

**Parameters:**
- `version`: OpenGL version (4.1, 4.2, etc.)
- `context_id`: Output parameter for context ID

#### `makeContextCurrent(context_id)`
Makes the specified context current for rendering.

**Signature:**
```cpp
IOReturn makeContextCurrent(VMOpenGLContextID context_id);
```

### OpenGL Command Processing

#### `processGLCommand(command)`
Processes an OpenGL command through the bridge.

**Signature:**
```cpp
IOReturn processGLCommand(VMOpenGLCommand* command);
```

## VMShaderManager API

### Shader Compilation

#### `compileShader(source, type, language)`
Compiles a shader from source code.

**Signature:**
```cpp
IOReturn compileShader(const char* source,
                      VMShaderType type,
                      VMShaderLanguage language,
                      VMShaderID* shader_id);
```

**Parameters:**
- `source`: Shader source code
- `type`: Shader type (vertex, fragment, compute)
- `language`: Source language (Metal, GLSL)
- `shader_id`: Output parameter for compiled shader ID

**Shader Types:**
- `kVMShaderTypeVertex`: Vertex shader
- `kVMShaderTypeFragment`: Fragment shader
- `kVMShaderTypeCompute`: Compute shader

**Shader Languages:**
- `kVMShaderLanguageMetal`: Metal Shading Language
- `kVMShaderLanguageGLSL`: OpenGL Shading Language

#### `loadShaderFromFile(path, type, language)`
Loads and compiles a shader from a file.

**Signature:**
```cpp
IOReturn loadShaderFromFile(const char* path,
                           VMShaderType type,
                           VMShaderLanguage language,
                           VMShaderID* shader_id);
```

### Shader Management

#### `getShaderBinary(shader_id)`
Retrieves compiled binary for a shader.

**Signature:**
```cpp
IOReturn getShaderBinary(VMShaderID shader_id,
                        void** binary_data,
                        size_t* binary_size);
```

#### `destroyShader(shader_id)`
Destroys a compiled shader and frees resources.

**Signature:**
```cpp
IOReturn destroyShader(VMShaderID shader_id);
```

## VMTextureManager API

### Texture Creation

#### `createTexture2D(width, height, format)`
Creates a 2D texture with specified parameters.

**Signature:**
```cpp
IOReturn createTexture2D(uint32_t width, uint32_t height,
                        VMTextureFormat format,
                        VMTextureID* texture_id);
```

**Texture Formats:**
- `kVMTextureFormatRGBA8`: 8-bit RGBA
- `kVMTextureFormatRGBA16F`: 16-bit float RGBA
- `kVMTextureFormatRGB10A2`: 10-bit RGB, 2-bit alpha
- `kVMTextureFormatBC1`: DXT1 compression
- `kVMTextureFormatBC7`: Modern block compression

#### `createTexture3D(width, height, depth, format)`
Creates a 3D volume texture.

**Signature:**
```cpp
IOReturn createTexture3D(uint32_t width, uint32_t height, uint32_t depth,
                        VMTextureFormat format,
                        VMTextureID* texture_id);
```

#### `createTextureCube(size, format)`
Creates a cube map texture.

**Signature:**
```cpp
IOReturn createTextureCube(uint32_t size,
                          VMTextureFormat format,
                          VMTextureID* texture_id);
```

### Texture Data Management

#### `uploadTextureData(texture_id, data, size)`
Uploads pixel data to a texture.

**Signature:**
```cpp
IOReturn uploadTextureData(VMTextureID texture_id,
                          const void* data,
                          size_t size);
```

#### `generateMipmaps(texture_id)`
Generates mipmaps for a texture.

**Signature:**
```cpp
IOReturn generateMipmaps(VMTextureID texture_id);
```

### Texture Processing

#### `processTexture(texture_id, operation)`
Applies a processing operation to a texture.

**Signature:**
```cpp
IOReturn processTexture(VMTextureID texture_id,
                       VMTextureOperation operation);
```

**Texture Operations:**
- `kVMTextureOpCompress`: Apply compression
- `kVMTextureOpResize`: Resize texture
- `kVMTextureOpFilter`: Apply filtering
- `kVMTextureOpColorCorrect`: Color correction

## VMCommandBuffer API

### Command Buffer Management

#### `createCommandBuffer()`
Creates a new command buffer from the pool.

**Signature:**
```cpp
VMCommandBuffer* createCommandBuffer();
```

**Return Value:**
- Pointer to command buffer, or NULL if pool exhausted

#### `beginCommands()`
Begins recording commands into the buffer.

**Signature:**
```cpp
IOReturn beginCommands();
```

#### `endCommands()`
Ends command recording and prepares for submission.

**Signature:**
```cpp
IOReturn endCommands();
```

### Command Recording

#### `addRenderCommand(command)`
Adds a render command to the buffer.

**Signature:**
```cpp
IOReturn addRenderCommand(VMRenderCommand* command);
```

#### `addComputeCommand(command)`
Adds a compute command to the buffer.

**Signature:**
```cpp
IOReturn addComputeCommand(VMComputeCommand* command);
```

### Command Execution

#### `submit(callback)`
Submits the command buffer for asynchronous execution.

**Signature:**
```cpp
IOReturn submit(VMCommandBufferCallback callback,
               void* context);
```

**Parameters:**
- `callback`: Completion callback function
- `context`: User context passed to callback

**Callback Signature:**
```cpp
typedef void (*VMCommandBufferCallback)(VMCommandBuffer* buffer, 
                                       IOReturn result,
                                       void* context);
```

#### `waitForCompletion()`
Waits for command buffer execution to complete.

**Signature:**
```cpp
IOReturn waitForCompletion(uint32_t timeout_ms);
```

**Parameters:**
- `timeout_ms`: Timeout in milliseconds, 0 for infinite wait

## Error Codes and Constants

### Common Return Values
- `kIOReturnSuccess`: Operation completed successfully
- `kIOReturnError`: General error occurred
- `kIOReturnNoMemory`: Insufficient memory
- `kIOReturnBadArgument`: Invalid parameter
- `kIOReturnNotFound`: Resource not found
- `kIOReturnUnsupported`: Operation not supported
- `kIOReturnTimeout`: Operation timed out

### Performance Tiers
```cpp
typedef enum {
    kVMPerformanceTierLow = 0,     // Software rendering
    kVMPerformanceTierMedium = 1,  // OpenGL acceleration
    kVMPerformanceTierHigh = 2     // Metal acceleration
} VMPerformanceTier;
```

### Pixel Formats
```cpp
typedef enum {
    kVMPixelFormatRGBA8 = 0,
    kVMPixelFormatBGRA8 = 1,
    kVMPixelFormatRGB565 = 2,
    kVMPixelFormatRGBA16F = 3,
    kVMPixelFormatRGB10A2 = 4
} VMPixelFormat;
```

## Usage Examples

### Complete Initialization Example
```cpp
// Initialize Phase 3 system
VMPhase3Manager* manager = VMPhase3Manager::withDefaults();
if (!manager) {
    IOLog("Failed to create Phase 3 manager");
    return kIOReturnError;
}

// Initialize all components
IOReturn result = manager->initializePhase3Components();
if (result != kIOReturnSuccess) {
    IOLog("Phase 3 initialization failed: 0x%x", result);
    manager->release();
    return result;
}

// Check performance tier
VMPerformanceTier tier = manager->getPerformanceTier();
IOLog("Phase 3 performance tier: %s", 
      tier == kVMPerformanceTierHigh ? "High (Metal)" :
      tier == kVMPerformanceTierMedium ? "Medium (OpenGL)" : "Low (Software)");

// Enable debug mode for development
manager->enableDebugMode(true);
```

### Surface Creation and Usage Example
```cpp
// Get IOSurface manager
VMIOSurfaceManager* surfaceManager = manager->getIOSurfaceManager();
if (!surfaceManager) {
    IOLog("IOSurface manager not available");
    return kIOReturnError;
}

// Create a 1080p RGBA surface
VMIOSurfaceID surface_id;
result = surfaceManager->createSurface(1920, 1080, 
                                      kVMPixelFormatRGBA8, 
                                      &surface_id);
if (result == kIOReturnSuccess) {
    // Set surface name
    OSString* name = OSString::withCString("MainFramebuffer");
    surfaceManager->setSurfaceProperty(surface_id, "IOSurfaceName", name);
    name->release();
    
    // Lock surface for direct access
    void* base_address;
    result = surfaceManager->lockSurface(surface_id, 
                                        kVMIOSurfaceLockReadWrite, 
                                        &base_address);
    if (result == kIOReturnSuccess) {
        // Direct memory access to surface
        // ... render operations
        
        // Unlock when done
        surfaceManager->unlockSurface(surface_id);
    }
}
```

### Shader Compilation Example
```cpp
// Get shader manager
VMShaderManager* shaderManager = manager->getShaderManager();
if (!shaderManager) {
    IOLog("Shader manager not available");
    return kIOReturnError;
}

// Compile vertex shader
const char* vertexSource = R"(
#include <metal_stdlib>
using namespace metal;

vertex float4 vertexShader(uint vertexID [[vertex_id]]) {
    // Simple vertex shader
    return float4(0.0, 0.0, 0.0, 1.0);
}
)";

VMShaderID vertex_id;
result = shaderManager->compileShader(vertexSource,
                                     kVMShaderTypeVertex,
                                     kVMShaderLanguageMetal,
                                     &vertex_id);
if (result == kIOReturnSuccess) {
    IOLog("Vertex shader compiled successfully: ID %u", vertex_id);
}
```

---

**VMQemuVGA Phase 3 API Reference**  
*Complete API documentation for advanced 3D acceleration system*  
*Version 3.0.0 - August 22, 2025*

//
//  SharedGL Metal Server
//  Receives serialized Metal commands from VM and executes on M4 Pro GPU
//

#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>

#define SERVER_PORT 28123  // Different port from OpenGL server

// Metal command opcodes
typedef enum {
    // Legacy immediate mode (v1.0)
    CMD_METAL_CREATE_BUFFER = 1,
    CMD_METAL_SET_VERTEX_BUFFER,
    CMD_METAL_SET_FRAGMENT_BYTES,
    CMD_METAL_DRAW_PRIMITIVES,
    CMD_METAL_PRESENT,
    CMD_METAL_CLEAR,
    CMD_METAL_SET_VIEWPORT,
    
    // Phase 1: Buffer Objects (VBOs/VAOs) - v1.1
    CMD_METAL_GEN_BUFFERS = 10,
    CMD_METAL_BIND_BUFFER,
    CMD_METAL_BUFFER_DATA,
    CMD_METAL_BUFFER_SUB_DATA,
    CMD_METAL_DELETE_BUFFERS,
    CMD_METAL_GEN_VERTEX_ARRAYS,
    CMD_METAL_BIND_VERTEX_ARRAY,
    CMD_METAL_DELETE_VERTEX_ARRAYS,
    CMD_METAL_VERTEX_ATTRIB_POINTER,
    CMD_METAL_ENABLE_VERTEX_ATTRIB_ARRAY,
    CMD_METAL_DISABLE_VERTEX_ATTRIB_ARRAY,
    CMD_METAL_DRAW_ARRAYS,
    CMD_METAL_DRAW_ELEMENTS,
    CMD_METAL_DRAW_ARRAYS_CLIENT_DATA,     // glDrawArrays with client-side vertex data
    
    // Phase 2: Shaders (GLSL → Metal) - v1.2
    CMD_METAL_CREATE_SHADER = 40,          // glCreateShader
    CMD_METAL_SHADER_SOURCE,               // glShaderSource
    CMD_METAL_COMPILE_SHADER,              // glCompileShader
    CMD_METAL_DELETE_SHADER,               // glDeleteShader
    CMD_METAL_CREATE_PROGRAM,              // glCreateProgram
    CMD_METAL_ATTACH_SHADER,               // glAttachShader
    CMD_METAL_LINK_PROGRAM,                // glLinkProgram
    CMD_METAL_USE_PROGRAM,                 // glUseProgram
    CMD_METAL_DELETE_PROGRAM,              // glDeleteProgram
    CMD_METAL_GET_UNIFORM_LOCATION,        // glGetUniformLocation
    CMD_METAL_GET_ATTRIB_LOCATION,         // glGetAttribLocation
    CMD_METAL_UNIFORM_1F,                  // glUniform1f
    CMD_METAL_UNIFORM_2F,                  // glUniform2f
    CMD_METAL_UNIFORM_3F,                  // glUniform3f
    CMD_METAL_UNIFORM_4F,                  // glUniform4f
    CMD_METAL_UNIFORM_1I,                  // glUniform1i
    CMD_METAL_UNIFORM_2FV,                 // glUniform2fv
    CMD_METAL_UNIFORM_MATRIX_4FV,          // glUniformMatrix4fv
    
    // Phase 3: Textures - v1.3
    CMD_METAL_GEN_TEXTURES = 60,           // glGenTextures
    CMD_METAL_BIND_TEXTURE,                // glBindTexture
    CMD_METAL_TEX_IMAGE_2D,                // glTexImage2D
    CMD_METAL_TEX_SUB_IMAGE_2D,            // glTexSubImage2D
    CMD_METAL_TEX_PARAMETERI,              // glTexParameteri
    CMD_METAL_ACTIVE_TEXTURE,              // glActiveTexture
    CMD_METAL_GENERATE_MIPMAP,             // glGenerateMipmap
    CMD_METAL_DELETE_TEXTURES,             // glDeleteTextures
    
    // Phase 4: Render State Management - v1.4
    CMD_METAL_ENABLE = 70,                 // glEnable
    CMD_METAL_DISABLE,                     // glDisable
    CMD_METAL_BLEND_FUNC,                  // glBlendFunc
    CMD_METAL_BLEND_EQUATION,              // glBlendEquation
    CMD_METAL_DEPTH_FUNC,                  // glDepthFunc
    CMD_METAL_DEPTH_MASK,                  // glDepthMask
    CMD_METAL_CULL_FACE,                   // glCullFace
    CMD_METAL_FRONT_FACE,                  // glFrontFace
    
    // Phase 5: Framebuffer Objects (FBOs) - v1.5
    CMD_METAL_GEN_FRAMEBUFFERS = 80,       // glGenFramebuffers
    CMD_METAL_BIND_FRAMEBUFFER,            // glBindFramebuffer
    CMD_METAL_DELETE_FRAMEBUFFERS,         // glDeleteFramebuffers
    CMD_METAL_FRAMEBUFFER_TEXTURE2D,       // glFramebufferTexture2D
    CMD_METAL_CHECK_FRAMEBUFFER_STATUS,    // glCheckFramebufferStatus
    CMD_METAL_GEN_RENDERBUFFERS,           // glGenRenderbuffers
    CMD_METAL_BIND_RENDERBUFFER,           // glBindRenderbuffer
    CMD_METAL_DELETE_RENDERBUFFERS,        // glDeleteRenderbuffers
    CMD_METAL_RENDERBUFFER_STORAGE,        // glRenderbufferStorage
    CMD_METAL_FRAMEBUFFER_RENDERBUFFER,    // glFramebufferRenderbuffer
    
    // Phase 7: Legacy OpenGL Fixed-Function Pipeline - v2.0
    CMD_METAL_FIXED_FUNCTION_DRAW = 100,   // glBegin/glVertex/glEnd immediate mode
    
    // Phase 7.6: Framebuffer Readback (VM Display Integration)
    CMD_METAL_READ_PIXELS = 110,           // glReadPixels - read framebuffer to VM
    CMD_METAL_SWAP_BUFFERS = 111           // glutSwapBuffers - trigger display update
} MetalCommand;

// OpenGL buffer targets
typedef enum {
    GL_ARRAY_BUFFER = 0x8892,
    GL_ELEMENT_ARRAY_BUFFER = 0x8893,
} GLBufferTarget;

// OpenGL buffer usage hints
typedef enum {
    GL_STATIC_DRAW = 0x88E4,
    GL_DYNAMIC_DRAW = 0x88E8,
    GL_STREAM_DRAW = 0x88E0,
} GLBufferUsage;

// Metal primitive types (matching MTLPrimitiveType)
typedef enum {
    METAL_PRIMITIVE_TRIANGLE = 3,
    METAL_PRIMITIVE_TRIANGLE_STRIP = 4,
    METAL_PRIMITIVE_LINE = 2,
} MetalPrimitiveType;

@interface MetalServerView : MTKView <MTKViewDelegate>
@property (nonatomic, strong) id<MTLCommandQueue> commandQueue;
@property (nonatomic, strong) id<MTLRenderPipelineState> pipelineState;
@property (nonatomic, strong) NSMutableDictionary<NSNumber*, id<MTLBuffer>> *buffers;
@property (nonatomic) int clientSocket;
@property (nonatomic) MTLClearColor metalClearColor;

// Phase 1: Buffer Objects (VBOs/VAOs)
@property (nonatomic, strong) NSMutableDictionary<NSNumber*, id<MTLBuffer>> *bufferRegistry;
@property (nonatomic, strong) NSMutableDictionary<NSNumber*, NSMutableDictionary*> *vaoRegistry;
@property (nonatomic) uint32_t currentArrayBuffer;
@property (nonatomic) uint32_t currentElementBuffer;
@property (nonatomic) uint32_t currentVAO;
@property (nonatomic) uint32_t nextBufferID;
@property (nonatomic) uint32_t nextVAOID;

// Phase 2: Shader Management
@property (nonatomic, strong) NSMutableDictionary<NSNumber*, NSString*> *vertexShaderSources;
@property (nonatomic, strong) NSMutableDictionary<NSNumber*, NSString*> *fragmentShaderSources;
@property (nonatomic, strong) NSMutableDictionary<NSNumber*, id<MTLLibrary>> *programLibraries;
@property (nonatomic, strong) NSMutableDictionary<NSNumber*, id<MTLFunction>> *vertexFunctions;
@property (nonatomic, strong) NSMutableDictionary<NSNumber*, id<MTLFunction>> *fragmentFunctions;
@property (nonatomic, strong) NSMutableDictionary<NSNumber*, NSMutableDictionary*> *programShaders;  // program -> {vertex, fragment}
@property (nonatomic, strong) NSMutableDictionary<NSNumber*, NSMutableDictionary*> *uniformLocations; // program -> {name -> location}
@property (nonatomic, strong) NSMutableDictionary<NSNumber*, id<MTLRenderPipelineState>> *shaderPipelines; // program -> pipeline
@property (nonatomic) uint32_t currentProgram;
@property (nonatomic) uint32_t nextShaderID;
@property (nonatomic) uint32_t nextProgramID;

// Phase 2.5: Uniform Data Storage
@property (nonatomic, strong) NSMutableDictionary<NSNumber*, NSMutableDictionary*> *programUniformData; // program ID -> {uniform location -> data}

// Phase 3: Textures
@property (nonatomic, strong) NSMutableDictionary<NSNumber*, id<MTLTexture>> *textureRegistry;
@property (nonatomic, strong) NSMutableDictionary<NSNumber*, NSMutableDictionary*> *textureMetadata;
@property (nonatomic, strong) NSMutableDictionary<NSNumber*, id<MTLSamplerState>> *samplerRegistry;
@property (nonatomic) uint32_t currentTexture2D;
@property (nonatomic) uint32_t activeTextureUnit;
@property (nonatomic) uint32_t nextTextureID;

// Phase 4: Render State Management
@property (nonatomic) BOOL blendEnabled;
@property (nonatomic) BOOL depthTestEnabled;
@property (nonatomic) BOOL cullFaceEnabled;
@property (nonatomic) uint32_t blendSrcFactor;
@property (nonatomic) uint32_t blendDstFactor;
@property (nonatomic) uint32_t blendEquation;
@property (nonatomic) uint32_t depthFunc;
@property (nonatomic) BOOL depthWriteEnabled;
@property (nonatomic) uint32_t cullFaceMode;
@property (nonatomic) uint32_t frontFace;
@property (nonatomic, strong) id<MTLDepthStencilState> depthStencilState;

// Phase 5: Framebuffer Objects
@property (nonatomic, strong) NSMutableDictionary<NSNumber*, NSMutableDictionary*> *framebufferRegistry; // FBO ID -> {colorAttachment, depthAttachment, etc}
@property (nonatomic, strong) NSMutableDictionary<NSNumber*, id<MTLTexture>> *renderbufferRegistry; // RBO ID -> MTLTexture
@property (nonatomic) uint32_t currentFramebuffer;
@property (nonatomic) uint32_t currentRenderbuffer;
@property (nonatomic) uint32_t nextFramebufferID;
@property (nonatomic) uint32_t nextRenderbufferID;

// Phase 7: Legacy OpenGL Fixed-Function Pipeline
@property (nonatomic, strong) id<MTLRenderPipelineState> fixedFunctionPipeline; // Basic MVP + color pass-through
@property (nonatomic) BOOL needsClear; // Track if glClear() was called
@property (nonatomic) BOOL frameClearApplied; // Track if clear was already applied this frame

// Phase 7.6: VM Display Integration (Offscreen Rendering)
@property (nonatomic, strong) id<MTLTexture> vmDisplayTexture;        // Offscreen render target for VM
@property (nonatomic, strong) id<MTLTexture> vmDisplayDepthTexture;   // Depth buffer for VM rendering
@property (nonatomic) NSUInteger vmDisplayWidth;                       // VM window width
@property (nonatomic) NSUInteger vmDisplayHeight;                      // VM window height
@property (nonatomic) BOOL renderToVM;                                 // TRUE = render offscreen for VM, FALSE = render to host window
@property (nonatomic) BOOL vmNeedsRedraw;                              // TRUE = trigger display update on next drawInMTKView
@property (nonatomic, strong) id<MTLRenderPipelineState> blitPipeline; // Pipeline for blitting VM texture to window
@property (nonatomic, strong) id<MTLSamplerState> samplerState;        // Sampler for texture filtering
@end

@implementation MetalServerView

- (instancetype)initWithFrame:(NSRect)frame device:(id<MTLDevice>)device {
        self = [super initWithFrame:frame device:device];
    if (self) {
        _commandQueue = [self.device newCommandQueue];
        _buffers = [NSMutableDictionary dictionary];
        _metalClearColor = MTLClearColorMake(0.0, 0.0, 0.0, 1.0);  // BLACK background (standard for glmark2)
        
        // Phase 1: Initialize VBO/VAO state
        _bufferRegistry = [[NSMutableDictionary alloc] init];
        _vaoRegistry = [[NSMutableDictionary alloc] init];
        NSLog(@"[Metal Server] Initialized _bufferRegistry: %p class=%@", _bufferRegistry, [_bufferRegistry class]);
        NSLog(@"[Metal Server] Initialized _vaoRegistry: %p class=%@", _vaoRegistry, [_vaoRegistry class]);
        _currentArrayBuffer = 0;
        _currentElementBuffer = 0;
        _currentVAO = 0;
        _nextBufferID = 1;
        _nextVAOID = 1;
        
        // Phase 2: Initialize shader state
        _vertexShaderSources = [[NSMutableDictionary alloc] init];
        _fragmentShaderSources = [[NSMutableDictionary alloc] init];
        _programLibraries = [[NSMutableDictionary alloc] init];
        _vertexFunctions = [[NSMutableDictionary alloc] init];
        _fragmentFunctions = [[NSMutableDictionary alloc] init];
        _programShaders = [[NSMutableDictionary alloc] init];
        _uniformLocations = [[NSMutableDictionary alloc] init];
        _shaderPipelines = [[NSMutableDictionary alloc] init];
        _programUniformData = [[NSMutableDictionary alloc] init];
        _currentProgram = 0;
        _nextShaderID = 1;
        _nextProgramID = 1;
        
        // Phase 3: Initialize texture state
        _textureRegistry = [[NSMutableDictionary alloc] init];
        _textureMetadata = [[NSMutableDictionary alloc] init];
        _samplerRegistry = [[NSMutableDictionary alloc] init];
        _currentTexture2D = 0;
        _activeTextureUnit = 0;
        _nextTextureID = 1;
        
        // Phase 4: Initialize render state (OpenGL defaults)
        _blendEnabled = NO;
        _depthTestEnabled = NO;
        _cullFaceEnabled = NO;
        _blendSrcFactor = 0x0302;  // GL_SRC_ALPHA
        _blendDstFactor = 0x0303;  // GL_ONE_MINUS_SRC_ALPHA
        _blendEquation = 0x8006;   // GL_FUNC_ADD
        _depthFunc = 0x0203;       // GL_LESS
        _depthWriteEnabled = YES;
        _cullFaceMode = 0x0405;    // GL_BACK
        _frontFace = 0x0901;       // GL_CCW
        
        // Phase 5: Initialize FBO state
        _framebufferRegistry = [[NSMutableDictionary alloc] init];
        _renderbufferRegistry = [[NSMutableDictionary alloc] init];
        _currentFramebuffer = 0;  // 0 = default framebuffer (screen)
        _currentRenderbuffer = 0;
        _nextFramebufferID = 1;
        _nextRenderbufferID = 1;
        
        // Initialize clear state - start with clear needed
        _needsClear = YES;
        _frameClearApplied = NO;
        
        self.delegate = self;
        self.colorPixelFormat = MTLPixelFormatBGRA8Unorm;
        self.clearColor = _metalClearColor;
        
        // Phase 4: Enable depth buffer
        self.depthStencilPixelFormat = MTLPixelFormatDepth32Float;
        self.clearDepth = 1.0;
        
        // Pause MTKView's automatic drawing - we handle presentation manually via CAMetalLayer
        self.paused = YES;
        
        [self setupPipeline];
        [self createDefaultTexture];
        
        NSLog(@"[Metal Server] Metal GPU initialized: %@", device.name);
    }
    return self;
}

- (void)createDefaultTexture {
    // Create 1x1 white texture for when no texture is bound
    MTLTextureDescriptor *descriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                                                          width:1
                                                                                         height:1
                                                                                      mipmapped:NO];
    descriptor.usage = MTLTextureUsageShaderRead;
    
    id<MTLTexture> whiteTexture = [self.device newTextureWithDescriptor:descriptor];
    uint8_t whitePixel[4] = {255, 255, 255, 255};
    [whiteTexture replaceRegion:MTLRegionMake2D(0, 0, 1, 1) mipmapLevel:0 withBytes:whitePixel bytesPerRow:4];
    
    _textureRegistry[@(0)] = whiteTexture;
    
    // Create default sampler state (linear filtering, repeat wrapping)
    MTLSamplerDescriptor *samplerDesc = [[MTLSamplerDescriptor alloc] init];
    samplerDesc.minFilter = MTLSamplerMinMagFilterLinear;
    samplerDesc.magFilter = MTLSamplerMinMagFilterLinear;
    samplerDesc.sAddressMode = MTLSamplerAddressModeRepeat;
    samplerDesc.tAddressMode = MTLSamplerAddressModeRepeat;
    
    _samplerRegistry[@(0)] = [self.device newSamplerStateWithDescriptor:samplerDesc];
    
    NSLog(@"[Metal Server] ✅ Default white texture and sampler created");
}

- (void)setupPipeline {
    // Create shader pipeline supporting colors (textures bound separately)
    // For Phase 3: texCoords are passed via buffer 1, position+color via buffer 0
    // IMPORTANT: Default to WHITE (1,1,1,1) if no vertex color data provided
    NSString *shaderSource = @
        "#include <metal_stdlib>\n"
        "using namespace metal;\n"
        "\n"
        "struct VertexIn {\n"
        "    float3 position [[attribute(0)]];\n"
        "    float4 color [[attribute(1)]];\n"
        "};\n"
        "\n"
        "struct VertexOut {\n"
        "    float4 position [[position]];\n"
        "    float4 color;\n"
        "};\n"
        "\n"
        "vertex VertexOut vertex_main(VertexIn in [[stage_in]]) {\n"
        "    VertexOut out;\n"
        "    out.position = float4(in.position, 1.0);\n"
        "    // Force bright red for debugging - ignore input colors\n"
        "    out.color = float4(1.0, 0.0, 0.0, 1.0);\n"
        "    return out;\n"
        "}\n"
        "\n"
        "fragment float4 fragment_main(VertexOut in [[stage_in]],\n"
        "                              texture2d<float> colorTexture [[texture(0)]],\n"
        "                              sampler colorSampler [[sampler(0)]]) {\n"
        "    // Force bright cyan output for debugging\n"
        "    return float4(0.0, 1.0, 1.0, 1.0);\n"
        "}\n";
    
    NSError *error = nil;
    id<MTLLibrary> library = [self.device newLibraryWithSource:shaderSource options:nil error:&error];
    if (!library) {
        NSLog(@"[Metal Server] ERROR: Failed to compile shaders: %@", error);
        return;
    }
    
    id<MTLFunction> vertexFunction = [library newFunctionWithName:@"vertex_main"];
    id<MTLFunction> fragmentFunction = [library newFunctionWithName:@"fragment_main"];
    
    // Create vertex descriptor (position + color attributes)
    // Note: texCoord is NOT in the descriptor - shader will use default (0,0)
    MTLVertexDescriptor *vertexDescriptor = [[MTLVertexDescriptor alloc] init];
    
    // Attribute 0: position (float3)
    vertexDescriptor.attributes[0].format = MTLVertexFormatFloat3;
    vertexDescriptor.attributes[0].offset = 0;
    vertexDescriptor.attributes[0].bufferIndex = 0;
    
    // Attribute 1: color (float4)
    vertexDescriptor.attributes[1].format = MTLVertexFormatFloat4;
    vertexDescriptor.attributes[1].offset = sizeof(float) * 3;  // After position
    vertexDescriptor.attributes[1].bufferIndex = 0;
    
    // Buffer layout (interleaved position + color)
    // Supports 7 floats (Phase 1) or 9 floats (Phase 3 with texCoords)
    vertexDescriptor.layouts[0].stride = sizeof(float) * 7;  // 3 position + 4 color
    vertexDescriptor.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;
    
    MTLRenderPipelineDescriptor *pipelineDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
    pipelineDescriptor.vertexFunction = vertexFunction;
    pipelineDescriptor.fragmentFunction = fragmentFunction;
    pipelineDescriptor.vertexDescriptor = vertexDescriptor;
    pipelineDescriptor.colorAttachments[0].pixelFormat = self.colorPixelFormat;
    
    // Phase 4: Enable blending in pipeline (will be toggled via render commands)
    pipelineDescriptor.colorAttachments[0].blendingEnabled = YES;
    pipelineDescriptor.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
    pipelineDescriptor.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;
    pipelineDescriptor.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
    pipelineDescriptor.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    pipelineDescriptor.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorOne;
    pipelineDescriptor.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorZero;
    
    // Phase 4: Add depth support
    pipelineDescriptor.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
    
    _pipelineState = [self.device newRenderPipelineStateWithDescriptor:pipelineDescriptor error:&error];
    if (!_pipelineState) {
        NSLog(@"[Metal Server] ERROR: Failed to create pipeline state: %@", error);
    } else {
        NSLog(@"[Metal Server] ✅ Metal pipeline created");
    }
    
    // Phase 4: Create depth stencil state
    MTLDepthStencilDescriptor *depthDescriptor = [[MTLDepthStencilDescriptor alloc] init];
    depthDescriptor.depthCompareFunction = MTLCompareFunctionLess;
    depthDescriptor.depthWriteEnabled = YES;
    _depthStencilState = [self.device newDepthStencilStateWithDescriptor:depthDescriptor];
    
    // Phase 7: Initialize fixed-function pipeline (lazy initialization - will be created on first use)
    _fixedFunctionPipeline = nil;
    
    // Phase 7.6: Initialize VM display state (always render offscreen for VM display)
    _vmDisplayTexture = nil;
    _vmDisplayDepthTexture = nil;
    _vmDisplayWidth = 800;
    _vmDisplayHeight = 600;
    _renderToVM = YES;  // Always render offscreen for VM display
    
    NSLog(@"[Metal Server] 📺 VM display mode enabled (will create %lux%lu texture on first draw)", _vmDisplayWidth, _vmDisplayHeight);
}

// Phase 7.6: Create Offscreen Render Target for VM Display
- (void)createVMDisplayTexturesWithWidth:(NSUInteger)width height:(NSUInteger)height {
    _vmDisplayWidth = width;
    _vmDisplayHeight = height;
    
    // Create color texture for VM rendering
    MTLTextureDescriptor *colorDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                                                                         width:width
                                                                                        height:height
                                                                                     mipmapped:NO];
    colorDesc.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
    colorDesc.storageMode = MTLStorageModePrivate;
    _vmDisplayTexture = [self.device newTextureWithDescriptor:colorDesc];
    
    // Create depth texture
    MTLTextureDescriptor *depthDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float
                                                                                          width:width
                                                                                         height:height
                                                                                      mipmapped:NO];
    depthDesc.usage = MTLTextureUsageRenderTarget;
    depthDesc.storageMode = MTLStorageModePrivate;
    _vmDisplayDepthTexture = [self.device newTextureWithDescriptor:depthDesc];
    
    NSLog(@"[Metal Server] 📺 Created VM display textures: %lux%lu", width, height);
    _renderToVM = YES;
    
    // Clear the texture once on creation
    id<MTLCommandBuffer> clearBuffer = [_commandQueue commandBuffer];
    MTLRenderPassDescriptor *clearPass = [MTLRenderPassDescriptor renderPassDescriptor];
    clearPass.colorAttachments[0].texture = _vmDisplayTexture;
    clearPass.colorAttachments[0].loadAction = MTLLoadActionClear;
    clearPass.colorAttachments[0].storeAction = MTLStoreActionStore;
    clearPass.colorAttachments[0].clearColor = MTLClearColorMake(0.2, 0.2, 0.3, 1.0);  // Dark blue-grey
    
    id<MTLRenderCommandEncoder> clearEncoder = [clearBuffer renderCommandEncoderWithDescriptor:clearPass];
    [clearEncoder endEncoding];
    [clearBuffer commit];
    
    NSLog(@"[Metal Server] 🎨 Cleared VM texture with initial color");
}

- (void)blitVMTextureToWindowUsingDrawable:(id<CAMetalDrawable>)drawable {
    if (!_vmDisplayTexture || !drawable) {
        return;
    }
    
    static int blitCount = 0;
    blitCount++;
    
    id<MTLCommandBuffer> commandBuffer = [_commandQueue commandBuffer];
    
    // Check if sizes match (no scaling needed on non-retina)
    BOOL needsScaling = (drawable.texture.width != _vmDisplayWidth || 
                         drawable.texture.height != _vmDisplayHeight);
    
    if (needsScaling) {
        // Render VM texture scaled to fill drawable
        MTLRenderPassDescriptor *renderPass = [MTLRenderPassDescriptor renderPassDescriptor];
        renderPass.colorAttachments[0].texture = drawable.texture;
        renderPass.colorAttachments[0].loadAction = MTLLoadActionClear;
        renderPass.colorAttachments[0].storeAction = MTLStoreActionStore;
        renderPass.colorAttachments[0].clearColor = MTLClearColorMake(0.0, 0.0, 0.0, 1.0);
        
        id<MTLRenderCommandEncoder> encoder = [commandBuffer renderCommandEncoderWithDescriptor:renderPass];
        
        if (!_blitPipeline) {
            _blitPipeline = [self createBlitPipeline];
        }
        
        [encoder setRenderPipelineState:_blitPipeline];
        [encoder setFragmentTexture:_vmDisplayTexture atIndex:0];
        [encoder setFragmentSamplerState:_samplerState atIndex:0];
        
        float vertices[] = {
            -1.0, -1.0, 0.0, 1.0,
             1.0, -1.0, 1.0, 1.0,
            -1.0,  1.0, 0.0, 0.0,
             1.0,  1.0, 1.0, 0.0
        };
        [encoder setVertexBytes:vertices length:sizeof(vertices) atIndex:0];
        [encoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];
        [encoder endEncoding];
    } else {
        // Direct blit - no scaling needed
        id<MTLBlitCommandEncoder> blitEncoder = [commandBuffer blitCommandEncoder];
        [blitEncoder copyFromTexture:_vmDisplayTexture
                         sourceSlice:0
                         sourceLevel:0
                        sourceOrigin:MTLOriginMake(0, 0, 0)
                          sourceSize:MTLSizeMake(_vmDisplayWidth, _vmDisplayHeight, 1)
                           toTexture:drawable.texture
                    destinationSlice:0
                    destinationLevel:0
                   destinationOrigin:MTLOriginMake(0, 0, 0)];
        [blitEncoder endEncoding];
    }
    
    [commandBuffer presentDrawable:drawable];
    [commandBuffer commit];
    
    if (blitCount % 60 == 0) {
        NSLog(@"[Metal Server] ✅ Presented frame #%d (%dx%d → %dx%d, %s)", 
              blitCount, (int)_vmDisplayWidth, (int)_vmDisplayHeight,
              (int)drawable.texture.width, (int)drawable.texture.height,
              needsScaling ? "scaled" : "direct");
    }
}

// Phase 7: Create Fixed-Function Pipeline (for legacy OpenGL immediate mode)
- (id<MTLRenderPipelineState>)createFixedFunctionPipeline {
    if (_fixedFunctionPipeline) {
        return _fixedFunctionPipeline;  // Already created
    }
    
    // Metal shader for basic MVP transform + bright white color (debug/fallback)
    NSString *shaderSource = @
        "#include <metal_stdlib>\n"
        "using namespace metal;\n"
        "\n"
        "// Vertex input: position only (attribute 0)\n"
        "struct VertexIn {\n"
        "    float3 position [[attribute(0)]];\n"
        "};\n"
        "\n"
        "struct VertexOut {\n"
        "    float4 position [[position]];\n"
        "};\n"
        "\n"
        "// Uniforms: modelview and projection matrices\n"
        "struct Uniforms {\n"
        "    float4x4 modelview;\n"
        "    float4x4 projection;\n"
        "};\n"
        "\n"
        "vertex VertexOut vertex_fixed_function(VertexIn in [[stage_in]],\n"
        "                                        constant Uniforms &uniforms [[buffer(1)]]) {\n"
        "    VertexOut out;\n"
        "    \n"
        "    // Apply MVP transform\n"
        "    float4 viewPos = uniforms.modelview * float4(in.position, 1.0);\n"
        "    out.position = uniforms.projection * viewPos;\n"
        "    \n"
        "    return out;\n"
        "}\n"
        "\n"
        "fragment float4 fragment_fixed_function(VertexOut in [[stage_in]]) {\n"
        "    // Return bright white for visibility (debug/fallback)\n"
        "    return float4(1.0, 1.0, 1.0, 1.0);\n"
        "}\n";
    
    NSError *error = nil;
    id<MTLLibrary> library = [self.device newLibraryWithSource:shaderSource options:nil error:&error];
    if (!library) {
        NSLog(@"[Metal Server] ❌ Failed to compile fixed-function shaders: %@", error);
        return nil;
    }
    
    id<MTLFunction> vertexFunction = [library newFunctionWithName:@"vertex_fixed_function"];
    id<MTLFunction> fragmentFunction = [library newFunctionWithName:@"fragment_fixed_function"];
    
    if (!vertexFunction || !fragmentFunction) {
        NSLog(@"[Metal Server] ❌ Failed to load fixed-function shader functions");
        return nil;
    }
    
    // Create vertex descriptor - read stride from VAO if available
    MTLVertexDescriptor *vertexDescriptor = [[MTLVertexDescriptor alloc] init];
    
    uint32_t actualStride = 12;  // Default: position only
    
    // Check if we have VAO configuration with stride information
    if (_currentVAO > 0 && _vaoRegistry[@(_currentVAO)]) {
        NSDictionary *vaoConfig = _vaoRegistry[@(_currentVAO)];
        NSArray *attributes = vaoConfig[@"attributes"];
        
        // Find attribute 0 (position) and use its stride
        for (NSDictionary *attr in attributes) {
            uint32_t attrIndex = [attr[@"index"] unsignedIntValue];
            if (attrIndex == 0) {
                uint32_t attrStride = [attr[@"stride"] unsignedIntValue];
                uint64_t attrOffset = [attr[@"offset"] unsignedLongLongValue];
                
                if (attrStride > 0) {
                    actualStride = attrStride;
                }
                
                vertexDescriptor.attributes[0].offset = attrOffset;
                NSLog(@"[Metal Server] Fixed-function: Using VAO stride=%u, offset=%llu", attrStride, attrOffset);
                break;
            }
        }
    } else {
        NSLog(@"[Metal Server] Fixed-function: No VAO, using default stride=12");
    }
    
    // Attribute 0: position (float3)
    vertexDescriptor.attributes[0].format = MTLVertexFormatFloat3;
    vertexDescriptor.attributes[0].bufferIndex = 0;
    
    // Buffer layout: use actual stride from VAO (e.g., 24 for position+normal)
    vertexDescriptor.layouts[0].stride = actualStride;
    vertexDescriptor.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;
    
    MTLRenderPipelineDescriptor *pipelineDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
    pipelineDescriptor.vertexFunction = vertexFunction;
    pipelineDescriptor.fragmentFunction = fragmentFunction;
    pipelineDescriptor.vertexDescriptor = vertexDescriptor;
    pipelineDescriptor.colorAttachments[0].pixelFormat = self.colorPixelFormat;
    
    // Enable blending
    pipelineDescriptor.colorAttachments[0].blendingEnabled = YES;
    pipelineDescriptor.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
    pipelineDescriptor.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;
    pipelineDescriptor.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
    pipelineDescriptor.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    pipelineDescriptor.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorOne;
    pipelineDescriptor.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorZero;
    
    // Add depth support
    pipelineDescriptor.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
    
    _fixedFunctionPipeline = [self.device newRenderPipelineStateWithDescriptor:pipelineDescriptor error:&error];
    if (!_fixedFunctionPipeline) {
        NSLog(@"[Metal Server] ❌ Failed to create fixed-function pipeline: %@", error);
        return nil;
    }
    
    NSLog(@"[Metal Server] ✅ Fixed-function pipeline created successfully");
    return _fixedFunctionPipeline;
}

// Create simple blit pipeline for scaling VM texture to window
- (id<MTLRenderPipelineState>)createBlitPipeline {
    NSString *shaderSource = @
        "#include <metal_stdlib>\n"
        "using namespace metal;\n"
        "\n"
        "struct VertexOut {\n"
        "    float4 position [[position]];\n"
        "    float2 texcoord;\n"
        "};\n"
        "\n"
        "vertex VertexOut vertex_blit(uint vid [[vertex_id]],\n"
        "                              constant float4 *vertices [[buffer(0)]]) {\n"
        "    VertexOut out;\n"
        "    out.position = float4(vertices[vid].xy, 0.0, 1.0);\n"
        "    out.texcoord = vertices[vid].zw;\n"
        "    return out;\n"
        "}\n"
        "\n"
        "fragment float4 fragment_blit(VertexOut in [[stage_in]],\n"
        "                               texture2d<float> tex [[texture(0)]],\n"
        "                               sampler samp [[sampler(0)]]) {\n"
        "    return tex.sample(samp, in.texcoord);\n"
        "}\n";
    
    NSError *error = nil;
    id<MTLLibrary> library = [self.device newLibraryWithSource:shaderSource options:nil error:&error];
    if (!library) {
        NSLog(@"[Metal Server] ❌ Failed to compile blit shaders: %@", error);
        return nil;
    }
    
    id<MTLFunction> vertexFunction = [library newFunctionWithName:@"vertex_blit"];
    id<MTLFunction> fragmentFunction = [library newFunctionWithName:@"fragment_blit"];
    
    MTLRenderPipelineDescriptor *desc = [[MTLRenderPipelineDescriptor alloc] init];
    desc.vertexFunction = vertexFunction;
    desc.fragmentFunction = fragmentFunction;
    desc.colorAttachments[0].pixelFormat = self.colorPixelFormat;
    
    id<MTLRenderPipelineState> pipeline = [self.device newRenderPipelineStateWithDescriptor:desc error:&error];
    if (!pipeline) {
        NSLog(@"[Metal Server] ❌ Failed to create blit pipeline: %@", error);
        return nil;
    }
    
    // Create sampler for texture filtering
    MTLSamplerDescriptor *samplerDesc = [[MTLSamplerDescriptor alloc] init];
    samplerDesc.minFilter = MTLSamplerMinMagFilterLinear;
    samplerDesc.magFilter = MTLSamplerMinMagFilterLinear;
    samplerDesc.sAddressMode = MTLSamplerAddressModeClampToEdge;
    samplerDesc.tAddressMode = MTLSamplerAddressModeClampToEdge;
    _samplerState = [self.device newSamplerStateWithDescriptor:samplerDesc];
    
    NSLog(@"[Metal Server] ✅ Blit pipeline created successfully");
    return pipeline;
}

// Phase 2: GLSL to MSL Translator
- (NSString*)translateGLSLToMSL:(NSString*)glslSource shaderType:(uint32_t)type {
    BOOL isVertexShader = (type == 0x8B31);  // GL_VERTEX_SHADER
    
    NSMutableString *mslSource = [NSMutableString string];
    
    // Add Metal standard library header
    [mslSource appendString:@"#include <metal_stdlib>\n"];
    [mslSource appendString:@"using namespace metal;\n\n"];
    
    // Split source into lines for processing
    NSArray *lines = [glslSource componentsSeparatedByString:@"\n"];
    
    NSMutableArray *attributes = [NSMutableArray array];
    NSMutableArray *uniforms = [NSMutableArray array];
    NSMutableArray *varyings = [NSMutableArray array];
    
    // First pass: collect attributes, uniforms, varyings
    for (NSString *line in lines) {
        NSString *trimmed = [line stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
        
        if ([trimmed hasPrefix:@"attribute "]) {
            [attributes addObject:trimmed];
        } else if ([trimmed hasPrefix:@"uniform "]) {
            [uniforms addObject:trimmed];
        } else if ([trimmed hasPrefix:@"varying "]) {
            [varyings addObject:trimmed];
        }
    }
    
    // Generate struct for vertex shader inputs (attributes)
    if (isVertexShader && attributes.count > 0) {
        [mslSource appendString:@"struct VertexIn {\n"];
        int attribIndex = 0;
        for (NSString *attr in attributes) {
            NSString *converted = [self convertAttributeToMSL:attr index:attribIndex++];
            [mslSource appendFormat:@"    %@\n", converted];
        }
        [mslSource appendString:@"};\n\n"];
    }
    
    // Generate struct for vertex shader outputs / fragment shader inputs
    if (varyings.count > 0) {
        [mslSource appendString:@"struct VertexOut {\n"];
        [mslSource appendString:@"    float4 position [[position]];\n"];
        int varyingIndex = 0;
        for (NSString *varying in varyings) {
            NSString *converted = [self convertVaryingToMSL:varying];
            if (converted) {
                // Add [[user(locnN)]] attribute for explicit varying location
                NSString *withAttr = [converted stringByReplacingOccurrencesOfString:@";" withString:[NSString stringWithFormat:@" [[user(locn%d)]];", varyingIndex++]];
                [mslSource appendFormat:@"    %@\n", withAttr];
            }
        }
        [mslSource appendString:@"};\n\n"];
    }
    
    // Add default constants for common GLSL built-ins that shaders may reference without declaring
    // Only add if NOT already declared as a uniform OR const (to avoid conflicts)
    [mslSource appendString:@"\n// Default constants for common GLSL built-ins\n"];
    
    BOOL hasLightSource = NO, hasMaterialDiffuse = NO, hasLightHalfVector = NO, hasTextureStep = NO, hasKernel = NO;
    
    // Check uniforms
    for (NSString *uniform in uniforms) {
        if ([uniform containsString:@"LightSourcePosition"]) hasLightSource = YES;
        if ([uniform containsString:@"MaterialDiffuse"]) hasMaterialDiffuse = YES;
        if ([uniform containsString:@"LightSourceHalfVector"]) hasLightHalfVector = YES;
        if ([uniform containsString:@"TextureStep"]) hasTextureStep = YES;
        if ([uniform containsString:@"Kernel"]) hasKernel = YES;
    }
    
    // Also check if they're declared as const in the GLSL source
    if ([glslSource rangeOfString:@"LightSourcePosition"].location != NSNotFound) hasLightSource = YES;
    if ([glslSource rangeOfString:@"MaterialDiffuse"].location != NSNotFound) hasMaterialDiffuse = YES;
    if ([glslSource rangeOfString:@"LightSourceHalfVector"].location != NSNotFound) hasLightHalfVector = YES;
    if ([glslSource rangeOfString:@"TextureStep"].location != NSNotFound) hasTextureStep = YES;
    if ([glslSource rangeOfString:@"Kernel"].location != NSNotFound) hasKernel = YES;
    
    // Check for other common constants that might be declared in GLSL
    BOOL hasTriangleColor = ([glslSource rangeOfString:@"TRIANGLE_COLOR"].location != NSNotFound);
    BOOL hasLineColor = ([glslSource rangeOfString:@"LINE_COLOR"].location != NSNotFound);
    BOOL hasDiffuseThreshold = ([glslSource rangeOfString:@"DiffuseThreshold"].location != NSNotFound);
    BOOL hasOutlineThickness = ([glslSource rangeOfString:@"OutlineThickness"].location != NSNotFound);
    
    if (!hasLightSource) {
        [mslSource appendString:@"constant float3 LightSourcePosition = float3(0.0, 0.0, 1.0);\n"];
    }
    if (!hasMaterialDiffuse) {
        [mslSource appendString:@"constant float4 MaterialDiffuse = float4(0.8, 0.8, 0.8, 1.0);\n"];
    }
    if (!hasLightHalfVector) {
        [mslSource appendString:@"constant float3 LightSourceHalfVector = float3(0.0, 0.0, 1.0);\n"];
    }
    if (!hasTextureStep) {
        [mslSource appendString:@"constant float TextureStepX = 0.01;\n"];
        [mslSource appendString:@"constant float TextureStepY = 0.01;\n"];
    }
    if (!hasKernel) {
        // Convolution kernel weights (15 values for 5x3 kernel)
        [mslSource appendString:@"constant float Kernel0 = 0.0;\n"];
        [mslSource appendString:@"constant float Kernel1 = 0.0;\n"];
        [mslSource appendString:@"constant float Kernel2 = 0.0;\n"];
        [mslSource appendString:@"constant float Kernel3 = 0.0;\n"];
        [mslSource appendString:@"constant float Kernel4 = 1.0;\n"];
        [mslSource appendString:@"constant float Kernel5 = 0.0;\n"];
        [mslSource appendString:@"constant float Kernel6 = 0.0;\n"];
        [mslSource appendString:@"constant float Kernel7 = 0.0;\n"];
        [mslSource appendString:@"constant float Kernel8 = 0.0;\n"];
        [mslSource appendString:@"constant float Kernel9 = 0.0;\n"];
        [mslSource appendString:@"constant float Kernel10 = 0.0;\n"];
        [mslSource appendString:@"constant float Kernel11 = 0.0;\n"];
        [mslSource appendString:@"constant float Kernel12 = 0.0;\n"];
        [mslSource appendString:@"constant float Kernel13 = 0.0;\n"];
        [mslSource appendString:@"constant float Kernel14 = 0.0;\n"];
    }
    
    // Add numbered light source constants (glmark2 uses LightSourcePosition0, LightColor0, etc)
    [mslSource appendString:@"constant float4 LightSourcePosition0 = float4(0.0, 0.0, 1.0, 0.0);\n"];
    [mslSource appendString:@"constant float4 LightColor0 = float4(1.0, 1.0, 1.0, 1.0);\n"];
    [mslSource appendString:@"constant float4 LightSourcePosition1 = float4(0.0, 0.0, 1.0, 0.0);\n"];
    [mslSource appendString:@"constant float4 LightColor1 = float4(1.0, 1.0, 1.0, 1.0);\n"];
    [mslSource appendString:@"constant float4 LightSourcePosition2 = float4(0.0, 0.0, 1.0, 0.0);\n"];
    [mslSource appendString:@"constant float4 LightColor2 = float4(1.0, 1.0, 1.0, 1.0);\n"];
    [mslSource appendString:@"constant float4 LightSourcePosition3 = float4(0.0, 0.0, 1.0, 0.0);\n"];
    [mslSource appendString:@"constant float4 LightColor3 = float4(1.0, 1.0, 1.0, 1.0);\n"];
    
    // Additional uniforms used by various shaders
    [mslSource appendString:@"constant float4 light_position = float4(0.0, 0.0, 5.0, 1.0);\n"];
    [mslSource appendString:@"constant float3 eye_direction = float3(0.0, 0.0, 1.0);\n"];
    [mslSource appendString:@"constant float matShininess = 32.0;\n"];
    [mslSource appendString:@"constant float4 lightSpecular = float4(1.0, 1.0, 1.0, 1.0);\n"];
    [mslSource appendString:@"constant float4 matSpecular = float4(1.0, 1.0, 1.0, 1.0);\n"];
    [mslSource appendString:@"constant float4 lightAmbient = float4(0.2, 0.2, 0.2, 1.0);\n"];
    [mslSource appendString:@"constant float4 matAmbient = float4(0.2, 0.2, 0.2, 1.0);\n"];
    [mslSource appendString:@"constant float4 diffuse_light_color = float4(1.0, 1.0, 1.0, 1.0);\n"];
    
    // Only add these if NOT already declared in GLSL source
    if (!hasDiffuseThreshold) {
        [mslSource appendString:@"constant float DiffuseThreshold = 0.5;\n"];
    }
    if (!hasOutlineThickness) {
        [mslSource appendString:@"constant float2 OutlineThickness = float2(0.02, 0.05);\n"];
    }
    if (!hasTriangleColor) {
        [mslSource appendString:@"constant float4 TRIANGLE_COLOR = float4(0.8, 0.8, 0.8, 1.0);\n"];
    }
    if (!hasLineColor) {
        [mslSource appendString:@"constant float4 LINE_COLOR = float4(0.3, 0.3, 0.3, 1.0);\n"];
    }
    [mslSource appendString:@"\n"];
    
    // Add common GLSL helper functions
    [mslSource appendString:@"// Helper function for multi-light computation\n"];
    [mslSource appendString:@"float4 compute_color(float4 light_pos, float4 light_col, float3 vertex_normal, float4 vertex_position) {\n"];
    [mslSource appendString:@"    float3 light_direction = normalize(light_pos.xyz/light_pos.w - vertex_position.xyz/vertex_position.w);\n"];
    [mslSource appendString:@"    float diffuse = max(dot(vertex_normal, light_direction), 0.0);\n"];
    [mslSource appendString:@"    return light_col * diffuse;\n"];
    [mslSource appendString:@"}\n\n"];
    
    // Add texture lookup helpers
    [mslSource appendString:@"// Texture lookup compatibility functions\n"];
    [mslSource appendString:@"float4 texture2D(texture2d<float> tex, sampler smp, float2 coord) {\n"];
    [mslSource appendString:@"    return tex.sample(smp, coord);\n"];
    [mslSource appendString:@"}\n\n"];
    
    [mslSource appendString:@"float4 textureCube(texturecube<float> tex, sampler smp, float3 coord) {\n"];
    [mslSource appendString:@"    return tex.sample(smp, coord);\n"];
    [mslSource appendString:@"}\n\n"];
    
    // Add common lighting calculations
    [mslSource appendString:@"// Phong lighting calculation\n"];
    [mslSource appendString:@"float4 phong_lighting(float3 N, float3 L, float3 V, float3 light_color, float3 material_diffuse, float3 material_specular, float shininess) {\n"];
    [mslSource appendString:@"    float3 R = reflect(-L, N);\n"];
    [mslSource appendString:@"    float diff = max(dot(N, L), 0.0);\n"];
    [mslSource appendString:@"    float spec = pow(max(dot(R, V), 0.0), shininess);\n"];
    [mslSource appendString:@"    float3 diffuse = diff * light_color * material_diffuse;\n"];
    [mslSource appendString:@"    float3 specular = spec * light_color * material_specular;\n"];
    [mslSource appendString:@"    return float4(diffuse + specular, 1.0);\n"];
    [mslSource appendString:@"}\n\n"];
    
    // Add noise functions for procedural textures
    [mslSource appendString:@"// Simple noise function\n"];
    [mslSource appendString:@"float noise(float3 p) {\n"];
    [mslSource appendString:@"    return fract(sin(dot(p, float3(12.9898, 78.233, 45.164))) * 43758.5453);\n"];
    [mslSource appendString:@"}\n\n"];
    
    // Process shader code - capture helper functions AND main function
    NSMutableString *helperFunctions = [NSMutableString string];
    NSMutableString *functionBody = [NSMutableString string];
    BOOL inMain = NO;
    BOOL foundMainSignature = NO;
    BOOL inHelperFunction = NO;
    BOOL foundOpenBrace = NO;  // Track if we've seen opening brace in helper function
    int braceDepth = 0;
    int lineNumber = 0;
    
    for (NSString *line in lines) {
        lineNumber++;
        NSString *trimmed = [line stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
        
        // Debug: Log shader processing
        if ([trimmed length] > 0 && ![trimmed hasPrefix:@"//"]) {
            NSLog(@"[Shader Line %d] inHelper=%d foundMain=%d depth=%d: %@", 
                  lineNumber, inHelperFunction, foundMainSignature, braceDepth, 
                  [trimmed length] > 60 ? [[trimmed substringToIndex:60] stringByAppendingString:@"..."] : trimmed);
        }
        
        // Skip attribute/uniform/varying declarations (already processed)
        if ([trimmed hasPrefix:@"attribute "] || 
            [trimmed hasPrefix:@"uniform "] || 
            [trimmed hasPrefix:@"varying "] ||
            [trimmed hasPrefix:@"#version"] ||
            [trimmed hasPrefix:@"precision "] ||
            [trimmed hasPrefix:@"#"]) {
            continue;
        }
        
        // Skip standalone closing braces before main function (malformed GLSL)
        if (!foundMainSignature && [trimmed isEqualToString:@"}"]) {
            NSLog(@"[GLSL→MSL] Skipping stray closing brace at line %lu (before main)", (unsigned long)lineNumber);
            continue;
        }
        
        // Handle const declarations before main - these need to be converted and output
        if (!foundMainSignature && !inHelperFunction && [trimmed hasPrefix:@"const "]) {
            // Convert GLSL const to Metal constant
            NSString *converted = [trimmed stringByReplacingOccurrencesOfString:@"const " withString:@"constant "];
            converted = [converted stringByReplacingOccurrencesOfString:@"vec2" withString:@"float2"];
            converted = [converted stringByReplacingOccurrencesOfString:@"vec3" withString:@"float3"];
            converted = [converted stringByReplacingOccurrencesOfString:@"vec4" withString:@"float4"];
            converted = [converted stringByReplacingOccurrencesOfString:@"mat3" withString:@"float3x3"];
            converted = [converted stringByReplacingOccurrencesOfString:@"mat4" withString:@"float4x4"];
            [helperFunctions appendFormat:@"%@\n", converted];
            NSLog(@"[GLSL→MSL] Converted const declaration: %@", converted);
            continue;
        }
        
        // Skip standalone statements before main (would become program-scope in Metal)
        // These are usually variable declarations or calculations that should be inside main
        if (!foundMainSignature && !inHelperFunction && 
            [trimmed length] > 0 && 
            ![trimmed hasPrefix:@"//"] &&
            ![trimmed containsString:@"("]) {
            // This is likely a statement line, not a function - skip it
            // Metal doesn't allow program-scope variable initialization
            NSLog(@"[Skipping standalone statement] Line %lu: %@", (unsigned long)lineNumber, trimmed);
            continue;
        }
        
        // Before main, check for helper functions (vec4 compute_color, float3 something, etc)
        if (!foundMainSignature && !inHelperFunction) {
            // Detect helper function: return_type function_name(
            // Common patterns: vec4 name(, float name(, void name(, float3 name(, etc
            if ([trimmed rangeOfString:@"("].location != NSNotFound &&
                ([trimmed hasPrefix:@"vec"] || [trimmed hasPrefix:@"float"] || 
                 [trimmed hasPrefix:@"int"] || [trimmed hasPrefix:@"void"] ||
                 [trimmed hasPrefix:@"mat"])) {
                // Check it's not main() and not a declaration ending with ;
                if (![trimmed hasSuffix:@";"] && ![trimmed containsString:@"main"]) {
                    inHelperFunction = YES;
                    braceDepth = -1;  // Start at -1, opening brace will bring to 0
                    
                    // Convert return type
                    NSString *converted = trimmed;
                    converted = [converted stringByReplacingOccurrencesOfString:@"vec2" withString:@"float2"];
                    converted = [converted stringByReplacingOccurrencesOfString:@"vec3" withString:@"float3"];
                    converted = [converted stringByReplacingOccurrencesOfString:@"vec4" withString:@"float4"];
                    converted = [converted stringByReplacingOccurrencesOfString:@"mat3" withString:@"float3x3"];
                    converted = [converted stringByReplacingOccurrencesOfString:@"mat4" withString:@"float4x4"];
                    
                    [helperFunctions appendFormat:@"%@\n", converted];
                    continue;
                }
            }
        }
        
        // If in helper function, capture all lines until closing brace
        if (inHelperFunction) {
            // Count braces to track when function ends
            for (NSInteger i = 0; i < trimmed.length; i++) {
                unichar c = [trimmed characterAtIndex:i];
                if (c == '{') {
                    braceDepth++;
                    foundOpenBrace = YES;  // Track that we've seen opening brace
                } else if (c == '}') {
                    braceDepth--;
                }
            }
            
            // Convert GLSL to MSL in helper function body
            NSString *converted = [self convertGLSLLineToMSL:trimmed isVertex:isVertexShader varyings:varyings attributes:attributes];
            [helperFunctions appendFormat:@"    %@\n", converted];
            
            // Check if function ended - must have seen opening brace and now returned to -1
            if (braceDepth == -1 && foundOpenBrace) {
                inHelperFunction = NO;
                foundOpenBrace = NO;
                NSLog(@"[GLSL→MSL] Helper function ended at line %lu", (unsigned long)lineNumber);
            }
            continue;
        }
        
        // Detect main function start - check for "void main" with flexible formatting
        if (!foundMainSignature && ([trimmed hasPrefix:@"void main("] || [trimmed hasPrefix:@"void main ("])) {
            foundMainSignature = YES;
            
            // Add helper functions before main
            if (helperFunctions.length > 0) {
                [mslSource appendString:@"// Helper functions\n"];
                [mslSource appendString:helperFunctions];
            }
            
            // Generate function signature
            if (isVertexShader) {
                [mslSource appendString:@"vertex VertexOut vertex_main("];
                if (attributes.count > 0) {
                    [mslSource appendString:@"VertexIn in [[stage_in]]"];
                }
                
                // Add uniform buffer parameters - parse actual uniforms from shader
                int uniformBufferIndex = 1;  // Start at buffer 1 (0 is for vertex data)
                for (NSString *uniformDecl in uniforms) {
                    // Parse uniform type and name
                    // Example: "uniform mat4 ModelViewMatrix;"
                    NSArray *parts = [uniformDecl componentsSeparatedByString:@" "];
                    if (parts.count >= 3) {
                        NSString *type = parts[1];
                        NSString *name = [parts[2] stringByReplacingOccurrencesOfString:@";" withString:@""];
                        
                        // Convert GLSL type to MSL
                        type = [type stringByReplacingOccurrencesOfString:@"vec2" withString:@"float2"];
                        type = [type stringByReplacingOccurrencesOfString:@"vec3" withString:@"float3"];
                        type = [type stringByReplacingOccurrencesOfString:@"vec4" withString:@"float4"];
                        type = [type stringByReplacingOccurrencesOfString:@"mat4" withString:@"float4x4"];
                        type = [type stringByReplacingOccurrencesOfString:@"mat3" withString:@"float3x3"];
                        
                        if (attributes.count > 0 || uniformBufferIndex > 1) [mslSource appendString:@", "];
                        [mslSource appendFormat:@"constant %@ &%@ [[buffer(%d)]]", type, name, uniformBufferIndex++];
                    }
                }
                
                [mslSource appendString:@") {\n"];
                [mslSource appendString:@"    VertexOut out;\n"];
            } else {
                [mslSource appendString:@"fragment float4 fragment_main(VertexOut in [[stage_in]]"];
                
                // Check if shader uses texture sampling
                BOOL usesTexture = NO;
                for (NSString *line in lines) {
                    if ([line containsString:@"texture2D"] || [line containsString:@"texture("] || 
                        [line containsString:@"sampler2D"]) {
                        usesTexture = YES;
                        break;
                    }
                }
                
                // Add uniform parameters for fragment shader
                int uniformIndex = 0;
                for (NSString *uniformDecl in uniforms) {
                    // Parse uniform type and name
                    // Example: "uniform vec4 colorMultiplier;"
                    NSArray *parts = [uniformDecl componentsSeparatedByString:@" "];
                    if (parts.count >= 3) {
                        NSString *type = parts[1];
                        NSString *name = [parts[2] stringByReplacingOccurrencesOfString:@";" withString:@""];
                        
                        // Skip sampler2D types (handled separately)
                        if ([type isEqualToString:@"sampler2D"]) {
                            continue;
                        }
                        
                        // Convert GLSL type to MSL
                        type = [type stringByReplacingOccurrencesOfString:@"vec2" withString:@"float2"];
                        type = [type stringByReplacingOccurrencesOfString:@"vec3" withString:@"float3"];
                        type = [type stringByReplacingOccurrencesOfString:@"vec4" withString:@"float4"];
                        type = [type stringByReplacingOccurrencesOfString:@"mat4" withString:@"float4x4"];
                        
                        [mslSource appendFormat:@", constant %@ &%@ [[buffer(%d)]]", type, name, uniformIndex++];
                    }
                }
                
                // Add texture/sampler ONLY if shader actually uses texture sampling
                if (usesTexture) {
                    [mslSource appendString:@", texture2d<float> tex [[texture(0)]], sampler samp [[sampler(0)]]"];
                }
                
                [mslSource appendString:@") {\n"];
            }
            
            // Check if the opening brace is on the same line
            if ([trimmed hasSuffix:@"{"]) {
                inMain = YES;
            }
            continue;
        }
        
        // If we found main signature but not yet in body, look for opening brace
        if (foundMainSignature && !inMain) {
            if ([trimmed isEqualToString:@"{"] || [trimmed hasPrefix:@"{"]) {
                inMain = YES;
            }
            continue;
        }
        
        if (inMain) {
            // Skip closing brace of main function (we'll add our own)
            if ([trimmed isEqualToString:@"}"]) {
                continue;
            }
            
            // Skip empty lines
            if (trimmed.length == 0) {
                continue;
            }
            
            // Convert GLSL built-ins to MSL
            NSString *converted = [self convertGLSLLineToMSL:trimmed isVertex:isVertexShader varyings:varyings attributes:attributes];
            [functionBody appendFormat:@"    %@\n", converted];
        }
    }
    
    // Post-process fragment shader body to add "in." prefixes to varyings
    if (!isVertexShader) {
        NSMutableString *processedBody = [NSMutableString stringWithString:functionBody];
        
        // Use regex to replace varying names with "in.varying" for ALL occurrences
        // This catches "return Color;", "Color *", "Color.rgb", etc.
        NSArray *varyings = @[@"Color", @"TextureCoord", @"v_color", @"v_texCoord", @"v_normal"];
        for (NSString *varying in varyings) {
            // Pattern: word boundary + varying name + word boundary
            NSString *pattern = [NSString stringWithFormat:@"\\b%@\\b", varying];
            NSRegularExpression *regex = [NSRegularExpression regularExpressionWithPattern:pattern options:0 error:nil];
            if (regex) {
                // Find all matches first
                NSArray *matches = [regex matchesInString:processedBody options:0 range:NSMakeRange(0, processedBody.length)];
                
                // Process in reverse to maintain string positions
                for (NSTextCheckingResult *match in [matches reverseObjectEnumerator]) {
                    NSRange matchRange = [match range];
                    
                    // Check if already prefixed
                    BOOL alreadyPrefixed = NO;
                    
                    // Check for "in." (3 chars before)
                    if (matchRange.location >= 3) {
                        NSRange prefixRange = NSMakeRange(matchRange.location - 3, 3);
                        NSString *prefix = [processedBody substringWithRange:prefixRange];
                        if ([prefix isEqualToString:@"in."]) {
                            alreadyPrefixed = YES;
                            NSLog(@"[GLSL→MSL] Already has 'in.' prefix at position %lu", (unsigned long)matchRange.location);
                        }
                    }
                    
                    // Check for "out." (4 chars before)
                    if (!alreadyPrefixed && matchRange.location >= 4) {
                        NSRange prefixRange = NSMakeRange(matchRange.location - 4, 4);
                        NSString *prefix = [processedBody substringWithRange:prefixRange];
                        if ([prefix isEqualToString:@"out."]) {
                            alreadyPrefixed = YES;
                            NSLog(@"[GLSL→MSL] Already has 'out.' prefix at position %lu", (unsigned long)matchRange.location);
                        }
                    }
                    
                    if (!alreadyPrefixed) {
                        NSString *replacement = [NSString stringWithFormat:@"in.%@", varying];
                        [processedBody replaceCharactersInRange:matchRange withString:replacement];
                    }
                }
            }
        }
        
        [mslSource appendString:processedBody];
    } else {
        // Post-process vertex shader body to add "in." prefixes to attributes
        NSMutableString *processedBody = [NSMutableString stringWithString:functionBody];
        
        // Extract attribute names from the attributes array
        NSMutableArray *attributeNames = [NSMutableArray array];
        for (NSString *attr in attributes) {
            // Parse: "attribute vec3 position;" -> extract "position"
            NSArray *parts = [attr componentsSeparatedByString:@" "];
            if (parts.count >= 3) {
                NSString *name = [[parts[2] stringByReplacingOccurrencesOfString:@";" withString:@""] 
                                  stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
                [attributeNames addObject:name];
            }
        }
        
        // Replace bare attribute references with "in.attribute"
        for (NSString *attrName in attributeNames) {
            NSString *pattern = [NSString stringWithFormat:@"\\b%@\\b", attrName];
            NSRegularExpression *regex = [NSRegularExpression regularExpressionWithPattern:pattern options:0 error:nil];
            if (regex) {
                NSArray *matches = [regex matchesInString:processedBody options:0 range:NSMakeRange(0, processedBody.length)];
                
                for (NSTextCheckingResult *match in [matches reverseObjectEnumerator]) {
                    NSRange matchRange = [match range];
                    
                    // Check if already prefixed with "in." or "out."
                    BOOL alreadyPrefixed = NO;
                    
                    // Check for "in." (3 chars before)
                    if (matchRange.location >= 3) {
                        NSRange prefixRange = NSMakeRange(matchRange.location - 3, 3);
                        NSString *prefix = [processedBody substringWithRange:prefixRange];
                        if ([prefix isEqualToString:@"in."] || [prefix isEqualToString:@"ut."]) {
                            alreadyPrefixed = YES;
                        }
                    }
                    
                    // Check for "out." (4 chars before)
                    if (!alreadyPrefixed && matchRange.location >= 4) {
                        NSRange prefixRange = NSMakeRange(matchRange.location - 4, 4);
                        NSString *prefix = [processedBody substringWithRange:prefixRange];
                        if ([prefix isEqualToString:@"out."]) {
                            alreadyPrefixed = YES;
                        }
                    }
                    
                    if (!alreadyPrefixed) {
                        NSString *replacement = [NSString stringWithFormat:@"in.%@", attrName];
                        [processedBody replaceCharactersInRange:matchRange withString:replacement];
                    }
                }
            }
        }
        
        [mslSource appendString:processedBody];
    }
    
    // Add return statement
    if (isVertexShader) {
        [mslSource appendString:@"    return out;\n"];
    } else {
        // For fragment shader, check if we need to return accumulated fragColor
        if ([mslSource containsString:@"float4 fragColor"]) {
            [mslSource appendString:@"    return fragColor;\n"];
        }
        // Otherwise return was already inline from gl_FragColor = conversion
    }
    
    [mslSource appendString:@"}\n"];
    
    return mslSource;
}

- (NSString*)convertAttributeToMSL:(NSString*)attrDecl index:(int)index {
    // attribute vec3 position; -> float3 position [[attribute(0)]];
    NSString *trimmed = [attrDecl stringByReplacingOccurrencesOfString:@"attribute " withString:@""];
    trimmed = [trimmed stringByReplacingOccurrencesOfString:@";" withString:@""];
    
    // Replace reserved keywords
    trimmed = [trimmed stringByReplacingOccurrencesOfString:@" vertex" withString:@" vertexPos"];
    
    // Convert GLSL types to MSL
    trimmed = [trimmed stringByReplacingOccurrencesOfString:@"vec2" withString:@"float2"];
    trimmed = [trimmed stringByReplacingOccurrencesOfString:@"vec3" withString:@"float3"];
    trimmed = [trimmed stringByReplacingOccurrencesOfString:@"vec4" withString:@"float4"];
    
    return [NSString stringWithFormat:@"%@ [[attribute(%d)]];", trimmed, index];
}

- (NSString*)convertVaryingToMSL:(NSString*)varyingDecl {
    // varying vec4 v_color; -> float4 v_color;
    NSString *trimmed = [varyingDecl stringByReplacingOccurrencesOfString:@"varying " withString:@""];
    
    // Strip precision qualifiers
    trimmed = [trimmed stringByReplacingOccurrencesOfString:@"HIGHP_OR_DEFAULT " withString:@""];
    trimmed = [trimmed stringByReplacingOccurrencesOfString:@"highp " withString:@""];
    trimmed = [trimmed stringByReplacingOccurrencesOfString:@"mediump " withString:@""];
    trimmed = [trimmed stringByReplacingOccurrencesOfString:@"lowp " withString:@""];
    
    // Convert GLSL types to MSL
    trimmed = [trimmed stringByReplacingOccurrencesOfString:@"vec2" withString:@"float2"];
    trimmed = [trimmed stringByReplacingOccurrencesOfString:@"vec3" withString:@"float3"];
    trimmed = [trimmed stringByReplacingOccurrencesOfString:@"vec4" withString:@"float4"];
    
    return trimmed;
}

- (NSString*)convertGLSLLineToMSL:(NSString*)line isVertex:(BOOL)isVertex varyings:(NSArray*)varyings attributes:(NSArray*)attributes {
    NSString *result = line;
    
    // Convert GLSL matrix casts: mat3(mat4) -> float3x3(matrix[0].xyz, matrix[1].xyz, matrix[2].xyz)
    // This handles cases like: normalize(mat3(NormalMatrix) * normal)
    NSRegularExpression *mat3CastRegex = [NSRegularExpression regularExpressionWithPattern:@"mat3\\(([a-zA-Z0-9_]+)\\)"
                                                                                   options:0
                                                                                     error:nil];
    if (mat3CastRegex) {
        NSArray *matches = [mat3CastRegex matchesInString:result options:0 range:NSMakeRange(0, result.length)];
        for (NSTextCheckingResult *match in [matches reverseObjectEnumerator]) {
            NSRange fullRange = [match range];
            NSRange nameRange = [match rangeAtIndex:1];
            NSString *matrixName = [result substringWithRange:nameRange];
            // Convert mat3(Matrix) to float3x3(Matrix[0].xyz, Matrix[1].xyz, Matrix[2].xyz)
            NSString *replacement = [NSString stringWithFormat:@"float3x3(%@[0].xyz, %@[1].xyz, %@[2].xyz)", 
                                     matrixName, matrixName, matrixName];
            result = [result stringByReplacingCharactersInRange:fullRange withString:replacement];
        }
    }
    
    // Strip GLSL precision qualifiers (Metal doesn't support these)
    result = [result stringByReplacingOccurrencesOfString:@"HIGHP_OR_DEFAULT " withString:@""];
    result = [result stringByReplacingOccurrencesOfString:@"highp " withString:@""];
    result = [result stringByReplacingOccurrencesOfString:@"mediump " withString:@""];
    result = [result stringByReplacingOccurrencesOfString:@"lowp " withString:@""];
    result = [result stringByReplacingOccurrencesOfString:@"precision " withString:@"// precision "];
    
    // Replace reserved Metal keywords in attribute names
    // 'vertex' is reserved in Metal, rename to 'vertexPos' or 'aVertex'
    result = [result stringByReplacingOccurrencesOfString:@" vertex " withString:@" vertexPos "];
    result = [result stringByReplacingOccurrencesOfString:@" vertex;" withString:@" vertexPos;"];
    result = [result stringByReplacingOccurrencesOfString:@" vertex." withString:@" vertexPos."];
    result = [result stringByReplacingOccurrencesOfString:@"(vertex." withString:@"(vertexPos."];
    result = [result stringByReplacingOccurrencesOfString:@".vertex." withString:@".vertexPos."];
    result = [result stringByReplacingOccurrencesOfString:@"attribute float3 vertex" withString:@"attribute float3 vertexPos"];
    
    // Convert GLSL types to MSL types
    result = [result stringByReplacingOccurrencesOfString:@"vec2" withString:@"float2"];
    result = [result stringByReplacingOccurrencesOfString:@"vec3" withString:@"float3"];
    result = [result stringByReplacingOccurrencesOfString:@"vec4" withString:@"float4"];
    result = [result stringByReplacingOccurrencesOfString:@"mat4" withString:@"float4x4"];
    result = [result stringByReplacingOccurrencesOfString:@"mat3" withString:@"float3x3"];
    
    // Convert GLSL built-in variables to MSL equivalents
    if (isVertex) {
        // gl_Position -> out.position
        result = [result stringByReplacingOccurrencesOfString:@"gl_Position" withString:@"out.position"];
        
        // gl_Vertex -> in.position (assuming standard naming)
        result = [result stringByReplacingOccurrencesOfString:@"gl_Vertex" withString:@"float4(in.position, 1.0)"];
        
        // Use NSRegularExpression for safer word-boundary replacements
        // Convert attribute references to in.attribute ONLY at word boundaries
        NSArray *attributes = @[@"position", @"normal", @"texcoord", @"color"];
        for (NSString *attr in attributes) {
            // Pattern: (word boundary before)(attribute)(word boundary after)
            // Word boundaries: space, (, ), comma, semicolon, =, start/end of string
            // This regex matches "position" but not "in.position" or "out.position"
            NSString *pattern = [NSString stringWithFormat:@"(?<!\\.)\\b%@\\b(?![.])", attr];
            NSError *error = nil;
            NSRegularExpression *regex = [NSRegularExpression regularExpressionWithPattern:pattern options:0 error:&error];
            if (!error) {
                // Check each match to avoid replacing if already prefixed with "in." or "out."
                NSArray *matches = [regex matchesInString:result options:0 range:NSMakeRange(0, result.length)];
                // Process matches in reverse to maintain string positions
                for (NSTextCheckingResult *match in [matches reverseObjectEnumerator]) {
                    NSRange matchRange = [match range];
                    // Check if preceded by "in." or "out." by looking at characters before match
                    BOOL alreadyPrefixed = NO;
                    if (matchRange.location >= 3) {
                        NSString *prefix = [result substringWithRange:NSMakeRange(matchRange.location - 3, 3)];
                        if ([prefix isEqualToString:@"in."] || [prefix isEqualToString:@"ut."]) {
                            alreadyPrefixed = YES;
                        }
                    }
                    if (!alreadyPrefixed) {
                        NSString *replacement = [NSString stringWithFormat:@"in.%@", attr];
                        result = [result stringByReplacingCharactersInRange:matchRange withString:replacement];
                    }
                }
            }
        }
        
        // Convert varying assignments: VaryingName = -> out.VaryingName =
        // Extract varying variable names from declarations
        NSMutableArray *varyingNames = [NSMutableArray array];
        for (NSString *varyingDecl in varyings) {
            // Parse "varying vec4 Color;" -> extract "Color"
            NSArray *parts = [varyingDecl componentsSeparatedByString:@" "];
            if (parts.count >= 3) {
                NSString *name = [[parts[2] stringByReplacingOccurrencesOfString:@";" withString:@""]
                                  stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
                [varyingNames addObject:name];
            }
        }
        
        for (NSString *varying in varyingNames) {
            // Replace "varying =" with "out.varying ="
            NSString *searchPattern = [NSString stringWithFormat:@"%@ =", varying];
            NSString *replacePattern = [NSString stringWithFormat:@"out.%@ =", varying];
            result = [result stringByReplacingOccurrencesOfString:searchPattern withString:replacePattern];
            
            // Also handle "varying=" (no space)
            searchPattern = [NSString stringWithFormat:@"%@=", varying];
            replacePattern = [NSString stringWithFormat:@"out.%@=", varying];
            result = [result stringByReplacingOccurrencesOfString:searchPattern withString:replacePattern];
        }
        
        // Fix standalone varying variable usage (like vWorld.xyz -> out.vWorld.xyz)
        for (NSString *varying in varyingNames) {
            // Use regex to match varying name not preceded by "out."
            // This handles: " vWorld.", "(vWorld.", "-vWorld.", ",vWorld.", etc.
            NSString *pattern = [NSString stringWithFormat:@"(?<!out\\.)\\b%@\\.", varying];
            NSError *error = nil;
            NSRegularExpression *regex = [NSRegularExpression regularExpressionWithPattern:pattern options:0 error:&error];
            if (!error) {
                NSString *replacement = [NSString stringWithFormat:@"out.%@.", varying];
                result = [regex stringByReplacingMatchesInString:result options:0 range:NSMakeRange(0, result.length) withTemplate:replacement];
            }
        }
        
        // Fix incorrect in.out.X patterns (should be out.X) - do this LAST
        result = [result stringByReplacingOccurrencesOfString:@"in.out." withString:@"out."];
    } else {
        // Fragment shader: FIRST handle gl_FragColor, THEN fix varying references
        
        // Step 0: Handle gl_FragCoord - replace with in.position
        result = [result stringByReplacingOccurrencesOfString:@"gl_FragCoord" withString:@"in.position"];
        
        // Step 1: Handle gl_FragColor - need to support both = and += operators
        // For += we need to accumulate in a local variable
        if ([result containsString:@"gl_FragColor"]) {
            // Check if using += operator (accumulation)
            if ([result containsString:@"gl_FragColor +="]) {
                // Insert fragColor variable at start of function
                result = [@"float4 fragColor = float4(0.0);\n" stringByAppendingString:result];
                // Replace gl_FragColor += with fragColor +=
                result = [result stringByReplacingOccurrencesOfString:@"gl_FragColor +=" withString:@"fragColor +="];
                result = [result stringByReplacingOccurrencesOfString:@"gl_FragColor+=" withString:@"fragColor+="];
                // Add return at the end (will be added before final })
                // This is handled later when we see the closing brace
            } else {
                // Simple assignment - just replace with return
                result = [result stringByReplacingOccurrencesOfString:@"gl_FragColor = " withString:@"return "];
                result = [result stringByReplacingOccurrencesOfString:@"gl_FragColor=" withString:@"return "];
            }
        }
        
        // Step 2: Extract varying names from declarations (dynamic instead of hardcoded)
        NSMutableArray *varyingNames = [NSMutableArray array];
        for (NSString *varyingDecl in varyings) {
            // Parse "varying vec4 Color;" -> extract "Color"
            NSArray *parts = [varyingDecl componentsSeparatedByString:@" "];
            if (parts.count >= 3) {
                NSString *name = [[parts[2] stringByReplacingOccurrencesOfString:@";" withString:@""]
                                  stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
                [varyingNames addObject:name];
            }
        }
        
        // Step 3: NOW convert varying references to in.varying (after gl_FragColor is gone)
        for (NSString *varying in varyingNames) {
            // Check if this varying is used as 'out.varying' (should not replace those)
            NSString *outPrefix = [NSString stringWithFormat:@"out.%@", varying];
            if ([result containsString:outPrefix]) {
                continue; // Skip this varying - it's used in vertex shader context
            }
            // Simple word-boundary regex without lookbehind (NSRegularExpression has limited lookbehind support)
            // Pattern: word boundary, varying name, word boundary
            NSString *pattern = [NSString stringWithFormat:@"\\b%@\\b", varying];
            NSError *error = nil;
            NSRegularExpression *regex = [NSRegularExpression regularExpressionWithPattern:pattern options:0 error:&error];
            if (!error) {
                NSArray *matches = [regex matchesInString:result options:0 range:NSMakeRange(0, result.length)];
                
                // Process matches in reverse to maintain string positions
                for (NSTextCheckingResult *match in [matches reverseObjectEnumerator]) {
                    NSRange matchRange = [match range];
                    NSString *matchedText = [result substringWithRange:matchRange];
                    
                    // Check if already prefixed with "in." or "out." by examining immediately before the match
                    BOOL alreadyPrefixed = NO;
                    if (matchRange.location >= 3) {
                        // Check for "in." immediately before (at location-3, location-2, location-1)
                        NSString *prefix = [result substringWithRange:NSMakeRange(matchRange.location - 3, 3)];
                        NSLog(@"[GLSL→MSL]   Match '%@' at position %lu, 3-char prefix: '%@'", 
                              matchedText, (unsigned long)matchRange.location, prefix);
                        if ([prefix isEqualToString:@"in."]) {
                            alreadyPrefixed = YES;
                            NSLog(@"[GLSL→MSL]   Already has 'in.' prefix - skipping");
                        }
                    }
                    if (!alreadyPrefixed && matchRange.location >= 4) {
                        // Check for "out." immediately before (at location-4, location-3, location-2, location-1)
                        NSString *prefix = [result substringWithRange:NSMakeRange(matchRange.location - 4, 4)];
                        NSLog(@"[GLSL→MSL]   Match '%@' at position %lu, 4-char prefix: '%@'", 
                              matchedText, (unsigned long)matchRange.location, prefix);
                        if ([prefix isEqualToString:@"out."]) {
                            alreadyPrefixed = YES;
                            NSLog(@"[GLSL→MSL]   Already has 'out.' prefix - skipping");
                        }
                    }
                    
                    if (!alreadyPrefixed) {
                        NSString *replacement = [NSString stringWithFormat:@"in.%@", varying];
                        NSLog(@"[GLSL→MSL]   Replacing '%@' with '%@'", matchedText, replacement);
                        result = [result stringByReplacingCharactersInRange:matchRange withString:replacement];
                        NSLog(@"[GLSL→MSL]   Result after replacement: %@", result);
                    }
                }
            }
        }
    }
    
    // Convert GLSL texture functions to MSL
    // texture2D(sampler, coord) -> tex.sample(samp, coord)
    // Need to extract just the coordinate argument, skip the sampler name
    NSRange texture2DRange = [result rangeOfString:@"texture2D("];
    if (texture2DRange.location != NSNotFound) {
        // Find the opening and closing parentheses
        NSInteger openParen = texture2DRange.location + texture2DRange.length - 1;
        NSInteger closeParen = openParen;
        NSInteger parenDepth = 1;
        
        // Find matching closing paren
        for (NSInteger i = openParen + 1; i < result.length; i++) {
            unichar c = [result characterAtIndex:i];
            if (c == '(') parenDepth++;
            else if (c == ')') {
                parenDepth--;
                if (parenDepth == 0) {
                    closeParen = i;
                    break;
                }
            }
        }
        
        // Extract arguments: "sampler, coord"
        NSString *args = [result substringWithRange:NSMakeRange(openParen + 1, closeParen - openParen - 1)];
        
        // Find first comma to split sampler and coord
        NSRange commaRange = [args rangeOfString:@","];
        if (commaRange.location != NSNotFound) {
            // Extract just the coordinate part (after comma)
            NSString *coord = [args substringFromIndex:commaRange.location + 1];
            coord = [coord stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
            
            // Build replacement: tex.sample(samp, coord)
            NSString *replacement = [NSString stringWithFormat:@"tex.sample(samp, %@)", coord];
            
            // Replace entire texture2D(...) call
            NSRange fullCallRange = NSMakeRange(texture2DRange.location, closeParen - texture2DRange.location + 1);
            result = [result stringByReplacingCharactersInRange:fullCallRange withString:replacement];
        }
    }
    
    // Shaders output their natural colors
    
    return result;
}

- (void)mtkView:(MTKView *)view drawableSizeWillChange:(CGSize)size {
    NSLog(@"[Metal Server] Drawable size: %.0fx%.0f", size.width, size.height);
}

- (void)drawInMTKView:(MTKView *)view {
    // MTKView's automatic draw - we don't use this for VM rendering
    // VM frames are displayed via direct CAMetalLayer access in CMD_METAL_SWAP_BUFFERS
}

// Helper function to receive all bytes (loops until complete)
static ssize_t recv_all(int sock, void *buffer, size_t length) {
    size_t total_received = 0;
    uint8_t *ptr = (uint8_t *)buffer;
    
    while (total_received < length) {
        ssize_t received = recv(sock, ptr + total_received, length - total_received, 0);
        if (received <= 0) {
            // Connection closed or error
            return -1;
        }
        total_received += received;
    }
    
    return (ssize_t)total_received;
}

- (void)handleCommand:(uint32_t)cmd socket:(int)sock {
    @autoreleasepool {
        NSLog(@"[Metal Server] Received command: %u", cmd);
        
        // DEBUG: Check if CMD_METAL_FIXED_FUNCTION_DRAW reaches here
        if (cmd == CMD_METAL_FIXED_FUNCTION_DRAW) {
            NSLog(@"[Metal Server] 🎯 FIXED_FUNCTION_DRAW detected! Value: %u", CMD_METAL_FIXED_FUNCTION_DRAW);
            // Fall through to switch
        }
        
        // DEBUG: Try if-else instead of switch for command 13
        if (cmd == CMD_METAL_GEN_VERTEX_ARRAYS) {
        NSLog(@"[Metal Server] DEBUG: Using if-else branch for GEN_VERTEX_ARRAYS");
        uint32_t count;
        NSLog(@"[Metal Server] DEBUG: About to recv count");
        ssize_t received = recv_all(sock, &count, sizeof(count));
        NSLog(@"[Metal Server] DEBUG: recv returned %zd, count=%u", received, count);
        if (received != sizeof(count)) {
            NSLog(@"[Metal Server] ERROR: Failed to receive VAO count (got %zd bytes)", received);
            return;
        }
        
        NSLog(@"[Metal Server] DEBUG: Allocating vaoIDs array for %u VAOs", count);
        uint32_t *vaoIDs = malloc(count * sizeof(uint32_t));
        
        for (uint32_t i = 0; i < count; i++) {
            NSLog(@"[Metal Server] DEBUG: Creating VAO %u/%u", i+1, count);
            vaoIDs[i] = _nextVAOID++;
            NSLog(@"[Metal Server] DEBUG: Assigned ID %u", vaoIDs[i]);
            
            NSLog(@"[Metal Server] DEBUG: Creating NSMutableArray");
            NSMutableArray *attributes = [[NSMutableArray alloc] init];
            NSLog(@"[Metal Server] DEBUG: Creating NSMutableDictionary");
            NSMutableDictionary *vaoConfig = [[NSMutableDictionary alloc] initWithObjectsAndKeys:
                attributes, @"attributes",
                @0, @"arrayBuffer",
                @0, @"elementBuffer",
                nil];
            NSLog(@"[Metal Server] DEBUG: Storing in registry");
            _vaoRegistry[@(vaoIDs[i])] = vaoConfig;
            NSLog(@"[Metal Server] DEBUG: Stored VAO %u", vaoIDs[i]);
        }
        
        NSLog(@"[Metal Server] DEBUG: Sending %u VAO IDs back to client", count);
        send(sock, vaoIDs, count * sizeof(uint32_t), 0);
        NSLog(@"[Metal Server] ✅ Generated %u VAO(s), IDs: %u-%u", count, vaoIDs[0], vaoIDs[count-1]);
        free(vaoIDs);
        NSLog(@"[Metal Server] DEBUG: Exiting GEN_VERTEX_ARRAYS handler");
        return;
    }
    
    switch (cmd) {
        case CMD_METAL_CLEAR: {
            float rgba[4];
            if (recv_all(sock, rgba, sizeof(rgba)) != sizeof(rgba)) {
                NSLog(@"[Metal Server] ERROR: Failed to receive clear color");
                return;
            }
            // Update clear color but DON'T set _needsClear = YES here
            // We only want to clear once at the start of each frame (in SwapBuffers)
            // not on every glClear() call within the frame
            _metalClearColor = MTLClearColorMake(rgba[0], rgba[1], rgba[2], rgba[3]);
            
            NSLog(@"[Metal Server] 🧹 Clear color updated: (%.2f,%.2f,%.2f,%.2f) - will clear on next frame", 
                  rgba[0], rgba[1], rgba[2], rgba[3]);
            break;
        }
            
        case CMD_METAL_DRAW_PRIMITIVES: {
            uint32_t primitiveType, vertexStart, vertexCount;
            if (recv_all(sock, &primitiveType, sizeof(primitiveType)) != sizeof(primitiveType)) {
                NSLog(@"[Metal Server] ERROR: Failed to receive primitive type");
                return;
            }
            if (recv_all(sock, &vertexStart, sizeof(vertexStart)) != sizeof(vertexStart)) {
                NSLog(@"[Metal Server] ERROR: Failed to receive vertex start");
                return;
            }
            if (recv_all(sock, &vertexCount, sizeof(vertexCount)) != sizeof(vertexCount)) {
                NSLog(@"[Metal Server] ERROR: Failed to receive vertex count");
                return;
            }
            
            // Receive vertex data (position[3] + color[4] = 7 floats per vertex)
            size_t vertexDataSize = vertexCount * 7 * sizeof(float);
            float *vertexData = malloc(vertexDataSize);
            if (!vertexData) {
                NSLog(@"[Metal Server] ERROR: Failed to allocate vertex buffer");
                return;
            }
            
            size_t totalReceived = 0;
            while (totalReceived < vertexDataSize) {
                ssize_t received = recv_all(sock, (char*)vertexData + totalReceived, 
                                       vertexDataSize - totalReceived);
                if (received <= 0) {
                    NSLog(@"[Metal Server] ERROR: Failed to receive vertex data (got %zu/%zu bytes)", 
                          totalReceived, vertexDataSize);
                    free(vertexData);
                    return;
                }
                totalReceived += received;
            }
            
            NSLog(@"[Metal Server] Draw: type=%u start=%u count=%u (received %zu bytes vertex data)", 
                  primitiveType, vertexStart, vertexCount, vertexDataSize);
            
            // Render on main queue but we've already consumed all data from socket
            dispatch_async(dispatch_get_main_queue(), ^{
                [self renderFrame:primitiveType vertexStart:vertexStart vertexCount:vertexCount vertexData:vertexData];
                free(vertexData);
            });
            break;
        }
            
        case CMD_METAL_SET_VIEWPORT: {
            float viewport[4];
            if (recv_all(sock, viewport, sizeof(viewport)) != sizeof(viewport)) {
                NSLog(@"[Metal Server] ERROR: Failed to receive viewport");
                return;
            }
            NSLog(@"[Metal Server] Viewport: (%.0f, %.0f, %.0fx%.0f)", 
                  viewport[0], viewport[1], viewport[2], viewport[3]);
            break;
        }
        
        // Phase 1: Buffer Objects (VBOs)
        case CMD_METAL_GEN_BUFFERS: {
            uint32_t count;
            if (recv_all(sock, &count, sizeof(count)) != sizeof(count)) {
                NSLog(@"[Metal Server] ERROR: Failed to receive buffer count");
                return;
            }
            
            uint32_t *bufferIDs = malloc(count * sizeof(uint32_t));
            for (uint32_t i = 0; i < count; i++) {
                bufferIDs[i] = _nextBufferID++;
            }
            
            // Send generated IDs back to client
            send(sock, bufferIDs, count * sizeof(uint32_t), 0);
            
            NSLog(@"[Metal Server] ✅ Generated %u buffer(s), IDs: %u-%u", count, bufferIDs[0], bufferIDs[count-1]);
            free(bufferIDs);
            break;
        }
        
        case CMD_METAL_BIND_BUFFER: {
            uint32_t target, buffer;
            if (recv_all(sock, &target, sizeof(target)) != sizeof(target)) return;
            if (recv_all(sock, &buffer, sizeof(buffer)) != sizeof(buffer)) return;
            
            if (target == GL_ARRAY_BUFFER) {
                _currentArrayBuffer = buffer;
            } else if (target == GL_ELEMENT_ARRAY_BUFFER) {
                _currentElementBuffer = buffer;
            }
            
            NSLog(@"[Metal Server] Bind buffer: target=0x%X buffer=%u", target, buffer);
            
            // Update VAO state if VAO is bound
            if (_currentVAO > 0 && _vaoRegistry[@(_currentVAO)]) {
                NSMutableDictionary *vaoConfig = _vaoRegistry[@(_currentVAO)];
                if (target == GL_ARRAY_BUFFER) {
                    vaoConfig[@"arrayBuffer"] = @(buffer);
                } else if (target == GL_ELEMENT_ARRAY_BUFFER) {
                    vaoConfig[@"elementBuffer"] = @(buffer);
                }
            }
            break;
        }
        
        case CMD_METAL_BUFFER_DATA: {
            uint32_t target, usage;
            uint64_t size;
            
            NSLog(@"[Metal Server] CMD_METAL_BUFFER_DATA: Reading header...");
            
            if (recv_all(sock, &target, sizeof(target)) != sizeof(target)) {
                NSLog(@"[Metal Server] ERROR: Failed to recv target");
                return;
            }
            if (recv_all(sock, &size, sizeof(size)) != sizeof(size)) {
                NSLog(@"[Metal Server] ERROR: Failed to recv size");
                return;
            }
            
            NSLog(@"[Metal Server] Buffer data header: target=0x%X size=%llu", target, size);
            
            // Receive buffer data FIRST (client sends data before usage)
            void *data = malloc(size);
            if (!data) {
                NSLog(@"[Metal Server] ERROR: Failed to allocate %llu bytes", size);
                return;
            }
            
            NSLog(@"[Metal Server] Receiving %llu bytes of buffer data...", size);
            size_t totalReceived = 0;
            while (totalReceived < size) {
                ssize_t received = recv_all(sock, (char*)data + totalReceived, 
                                       size - totalReceived);
                if (received <= 0) {
                    NSLog(@"[Metal Server] ERROR: Failed to receive buffer data (received=%zd)", received);
                    free(data);
                    return;
                }
                totalReceived += received;
            }
            NSLog(@"[Metal Server] ✅ Received all %llu bytes", size);
            
            // NOW receive usage (client sends it after data)
            if (recv_all(sock, &usage, sizeof(usage)) != sizeof(usage)) {
                NSLog(@"[Metal Server] ERROR: Failed to recv usage");
                free(data);
                return;
            }
            NSLog(@"[Metal Server] Buffer usage: 0x%X", usage);
            
            // Get current buffer ID based on target
            uint32_t bufferID = (target == GL_ARRAY_BUFFER) ? 
                                _currentArrayBuffer : _currentElementBuffer;
            
            NSLog(@"[Metal Server] Current buffer ID for target 0x%X: %u", target, bufferID);
            
            if (bufferID == 0) {
                NSLog(@"[Metal Server] ERROR: No buffer bound to target 0x%X", target);
                free(data);
                return;
            }
            
            // Check device
            if (!self.device) {
                NSLog(@"[Metal Server] ERROR: Metal device is nil!");
                free(data);
                return;
            }
            
            NSLog(@"[Metal Server] Creating Metal buffer with %llu bytes...", size);
            
            // Create Metal buffer
            MTLResourceOptions options = MTLResourceStorageModeShared;
            id<MTLBuffer> mtlBuffer = [self.device newBufferWithBytes:data
                                                               length:size
                                                              options:options];
            
            if (!mtlBuffer) {
                NSLog(@"[Metal Server] ERROR: Failed to create Metal buffer!");
                free(data);
                return;
            }
            
            NSLog(@"[Metal Server] ✅ Metal buffer created successfully");
            
            // Check registry before storing
            if (!_bufferRegistry) {
                NSLog(@"[Metal Server] ERROR: _bufferRegistry is nil!");
                free(data);
                return;
            }
            
            NSLog(@"[Metal Server] Registry exists: %p", _bufferRegistry);
            NSLog(@"[Metal Server] Registry count before: %lu", (unsigned long)[_bufferRegistry count]);
            NSLog(@"[Metal Server] Creating NSNumber for buffer ID %u...", bufferID);
            
            NSNumber *key = @(bufferID);
            NSLog(@"[Metal Server] NSNumber created: %@", key);
            NSLog(@"[Metal Server] MTLBuffer to store: %p", mtlBuffer);
            NSLog(@"[Metal Server] Storing buffer %u in registry...", bufferID);
            
            // Store in registry
            [_bufferRegistry setObject:mtlBuffer forKey:key];
            
            NSLog(@"[Metal Server] ✅ Buffer stored in registry");
            NSLog(@"[Metal Server] ✅ Buffer data uploaded: buffer=%u target=0x%X size=%llu bytes", 
                  bufferID, target, size);
            
            free(data);
            break;
        }
        
        case CMD_METAL_BUFFER_SUB_DATA: {
            uint32_t target;
            uint64_t offset, size;
            
            if (recv_all(sock, &target, sizeof(target)) != sizeof(target)) return;
            if (recv_all(sock, &offset, sizeof(offset)) != sizeof(offset)) return;
            if (recv_all(sock, &size, sizeof(size)) != sizeof(size)) return;
            
            // Receive sub-data
            void *data = malloc(size);
            if (!data) {
                NSLog(@"[Metal Server] ERROR: Failed to allocate %llu bytes for sub-data", size);
                return;
            }
            
            if (recv_all(sock, data, size) != (ssize_t)size) {
                NSLog(@"[Metal Server] ERROR: Failed to receive sub-data");
                free(data);
                return;
            }
            
            // Get current buffer
            uint32_t bufferID = (target == GL_ARRAY_BUFFER) ? 
                                _currentArrayBuffer : _currentElementBuffer;
            
            if (bufferID == 0) {
                NSLog(@"[Metal Server] ERROR: No buffer bound for glBufferSubData");
                free(data);
                return;
            }
            
            id<MTLBuffer> mtlBuffer = [_bufferRegistry objectForKey:@(bufferID)];
            if (!mtlBuffer) {
                NSLog(@"[Metal Server] ERROR: Buffer %u not found in registry", bufferID);
                free(data);
                return;
            }
            
            // Copy data to buffer at offset
            memcpy((char*)[mtlBuffer contents] + offset, data, size);
            
            NSLog(@"[Metal Server] ✅ BufferSubData: buffer=%u offset=%llu size=%llu", 
                  bufferID, offset, size);
            
            free(data);
            break;
        }
        
        case CMD_METAL_DELETE_BUFFERS: {
            uint32_t count;
            if (recv_all(sock, &count, sizeof(count)) != sizeof(count)) return;
            
            uint32_t *bufferIDs = malloc(count * sizeof(uint32_t));
            if (recv_all(sock, bufferIDs, count * sizeof(uint32_t)) != count * sizeof(uint32_t)) {
                free(bufferIDs);
                return;
            }
            
            for (uint32_t i = 0; i < count; i++) {
                [_bufferRegistry removeObjectForKey:@(bufferIDs[i])];
            }
            
            NSLog(@"[Metal Server] ✅ Deleted %u buffer(s)", count);
            free(bufferIDs);
            break;
        }
        
        case CMD_METAL_GEN_VERTEX_ARRAYS: {
            NSLog(@"[Metal Server] DEBUG: Entering GEN_VERTEX_ARRAYS handler");
            uint32_t count;
            NSLog(@"[Metal Server] DEBUG: About to recv VAO count (4 bytes)");
            ssize_t received = recv_all(sock, &count, sizeof(count));
            NSLog(@"[Metal Server] DEBUG: recv returned %zd bytes", received);
            if (received != sizeof(count)) {
                NSLog(@"[Metal Server] ERROR: Failed to receive VAO count (got %zd bytes)", received);
                return;
            }
            NSLog(@"[Metal Server] DEBUG: Received count=%u", count);
            
            uint32_t *vaoIDs = malloc(count * sizeof(uint32_t));
            NSLog(@"[Metal Server] DEBUG: Allocated vaoIDs array");
            
            for (uint32_t i = 0; i < count; i++) {
                NSLog(@"[Metal Server] DEBUG: Creating VAO %u/%u", i+1, count);
                vaoIDs[i] = _nextVAOID++;
                NSLog(@"[Metal Server] DEBUG: Assigned VAO ID %u", vaoIDs[i]);
                
                // Initialize empty VAO configuration with explicit mutable containers
                NSLog(@"[Metal Server] DEBUG: Creating NSMutableArray for attributes");
                NSMutableArray *attributes = [[NSMutableArray alloc] init];
                NSLog(@"[Metal Server] DEBUG: Creating NSMutableDictionary for VAO config");
                NSMutableDictionary *vaoConfig = [[NSMutableDictionary alloc] initWithObjectsAndKeys:
                    attributes, @"attributes",
                    @0, @"arrayBuffer",
                    @0, @"elementBuffer",
                    nil];
                NSLog(@"[Metal Server] DEBUG: Storing VAO config in registry");
                _vaoRegistry[@(vaoIDs[i])] = vaoConfig;
                NSLog(@"[Metal Server] DEBUG: VAO %u stored successfully", vaoIDs[i]);
            }
            
            NSLog(@"[Metal Server] DEBUG: Sending VAO IDs back to client");
            send(sock, vaoIDs, count * sizeof(uint32_t), 0);
            NSLog(@"[Metal Server] ✅ Generated %u VAO(s), IDs: %u-%u", count, vaoIDs[0], vaoIDs[count-1]);
            free(vaoIDs);
            NSLog(@"[Metal Server] DEBUG: Exiting GEN_VERTEX_ARRAYS handler");
            break;
        }
        
        case CMD_METAL_BIND_VERTEX_ARRAY: {
            uint32_t vao;
            if (recv_all(sock, &vao, sizeof(vao)) != sizeof(vao)) return;
            
            _currentVAO = vao;
            
            // Invalidate fixed-function pipeline when VAO changes
            // (different VAOs have different vertex strides)
            _fixedFunctionPipeline = nil;
            
            // Restore buffer bindings from VAO
            if (vao > 0 && _vaoRegistry[@(vao)]) {
                NSDictionary *vaoConfig = _vaoRegistry[@(vao)];
                _currentArrayBuffer = [vaoConfig[@"arrayBuffer"] unsignedIntValue];
                _currentElementBuffer = [vaoConfig[@"elementBuffer"] unsignedIntValue];
            }
            
            NSLog(@"[Metal Server] Bind VAO: %u (array_buffer=%u, element_buffer=%u, invalidated pipeline)", 
                  vao, _currentArrayBuffer, _currentElementBuffer);
            break;
        }
        
        case CMD_METAL_DELETE_VERTEX_ARRAYS: {
            uint32_t count;
            if (recv_all(sock, &count, sizeof(count)) != sizeof(count)) return;
            
            uint32_t *vaoIDs = malloc(count * sizeof(uint32_t));
            if (recv_all(sock, vaoIDs, count * sizeof(uint32_t)) != count * sizeof(uint32_t)) {
                free(vaoIDs);
                return;
            }
            
            for (uint32_t i = 0; i < count; i++) {
                [_vaoRegistry removeObjectForKey:@(vaoIDs[i])];
            }
            
            NSLog(@"[Metal Server] ✅ Deleted %u VAO(s)", count);
            free(vaoIDs);
            break;
        }
        
        case CMD_METAL_VERTEX_ATTRIB_POINTER: {
            uint32_t index, size, type, stride;
            uint8_t normalized;
            uint64_t offset;
            
            if (recv_all(sock, &index, sizeof(index)) != sizeof(index)) return;
            if (recv_all(sock, &size, sizeof(size)) != sizeof(size)) return;
            if (recv_all(sock, &type, sizeof(type)) != sizeof(type)) return;
            if (recv_all(sock, &normalized, sizeof(normalized)) != sizeof(normalized)) return;
            if (recv_all(sock, &stride, sizeof(stride)) != sizeof(stride)) return;
            if (recv_all(sock, &offset, sizeof(offset)) != sizeof(offset)) return;
            
            // Store in current VAO configuration
            if (_currentVAO > 0 && _vaoRegistry[@(_currentVAO)]) {
                NSMutableDictionary *vaoConfig = _vaoRegistry[@(_currentVAO)];
                NSMutableArray *attributes = vaoConfig[@"attributes"];
                
                NSMutableDictionary *attribConfig = [NSMutableDictionary dictionaryWithObjectsAndKeys:
                    @(index), @"index",
                    @(size), @"size",
                    @(type), @"type",
                    @(normalized), @"normalized",
                    @(stride), @"stride",
                    @(offset), @"offset",
                    @(_currentArrayBuffer), @"buffer",
                    @NO, @"enabled",
                    nil];
                
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
                
                vaoConfig[@"arrayBuffer"] = @(_currentArrayBuffer);
            }
            
            NSLog(@"[Metal Server] Vertex attrib pointer: index=%u size=%u type=0x%X stride=%u offset=%llu buffer=%u", 
                  index, size, type, stride, offset, _currentArrayBuffer);
            break;
        }
        
        case CMD_METAL_ENABLE_VERTEX_ATTRIB_ARRAY: {
            uint32_t index;
            if (recv_all(sock, &index, sizeof(index)) != sizeof(index)) return;
            
            // Mark attribute as enabled in VAO
            if (_currentVAO > 0 && _vaoRegistry[@(_currentVAO)]) {
                NSMutableDictionary *vaoConfig = _vaoRegistry[@(_currentVAO)];
                NSMutableArray *attributes = vaoConfig[@"attributes"];
                
                for (NSMutableDictionary *attr in attributes) {
                    if ([attr[@"index"] unsignedIntValue] == index) {
                        attr[@"enabled"] = @YES;
                        break;
                    }
                }
            }
            
            NSLog(@"[Metal Server] Enable vertex attrib array: %u", index);
            break;
        }
        
        case CMD_METAL_DISABLE_VERTEX_ATTRIB_ARRAY: {
            uint32_t index;
            if (recv_all(sock, &index, sizeof(index)) != sizeof(index)) return;
            
            // Mark attribute as disabled in VAO
            if (_currentVAO > 0 && _vaoRegistry[@(_currentVAO)]) {
                NSMutableDictionary *vaoConfig = _vaoRegistry[@(_currentVAO)];
                NSMutableArray *attributes = vaoConfig[@"attributes"];
                
                for (NSMutableDictionary *attr in attributes) {
                    if ([attr[@"index"] unsignedIntValue] == index) {
                        attr[@"enabled"] = @NO;
                        break;
                    }
                }
            }
            
            NSLog(@"[Metal Server] Disable vertex attrib array: %u", index);
            break;
        }
        
        case CMD_METAL_DRAW_ARRAYS: {
            uint32_t mode, first, count;
            if (recv_all(sock, &mode, sizeof(mode)) != sizeof(mode)) return;
            if (recv_all(sock, &first, sizeof(first)) != sizeof(first)) return;
            if (recv_all(sock, &count, sizeof(count)) != sizeof(count)) return;
            
            // Capture current state before async dispatch (must capture on socket thread before state changes)
            uint32_t capturedFBO = _currentFramebuffer;
            uint32_t capturedVAO = _currentVAO;
            uint32_t capturedProgram = _currentProgram;
            uint32_t capturedTexture = _currentTexture2D;
            
            NSLog(@"[Metal Server] Draw arrays: mode=%u first=%u count=%u VAO=%u FBO=%u", 
                  mode, first, count, capturedVAO, capturedFBO);
            
            dispatch_sync(dispatch_get_main_queue(), ^{
                // Use captured state for this render
                _currentFramebuffer = capturedFBO;
                _currentVAO = capturedVAO;
                _currentProgram = capturedProgram;
                _currentTexture2D = capturedTexture;
                
                [self renderFrameVBO:mode first:first count:count];
            });
            break;
        }
        
        case CMD_METAL_DRAW_ELEMENTS: {
            uint32_t mode, count, type;
            uint64_t offset;
            if (recv_all(sock, &mode, sizeof(mode)) != sizeof(mode)) return;
            if (recv_all(sock, &count, sizeof(count)) != sizeof(count)) return;
            if (recv_all(sock, &type, sizeof(type)) != sizeof(type)) return;
            if (recv_all(sock, &offset, sizeof(offset)) != sizeof(offset)) return;
            
            NSLog(@"[Metal Server] Draw elements: mode=%u count=%u type=0x%X offset=%llu", 
                  mode, count, type, offset);
            
            dispatch_async(dispatch_get_main_queue(), ^{
                [self renderFrameVBOIndexed:mode count:count indexType:type indexOffset:offset];
            });
            break;
        }
        
        case CMD_METAL_DRAW_ARRAYS_CLIENT_DATA: {
            // Receive draw parameters
            uint32_t mode, first, count, enabledCount;
            if (recv_all(sock, &mode, sizeof(mode)) != sizeof(mode)) return;
            if (recv_all(sock, &first, sizeof(first)) != sizeof(first)) return;
            if (recv_all(sock, &count, sizeof(count)) != sizeof(count)) return;
            if (recv_all(sock, &enabledCount, sizeof(enabledCount)) != sizeof(enabledCount)) return;
            
            NSLog(@"[Metal Server] Draw arrays (client data): mode=%u first=%u count=%u attrs=%u", 
                  mode, first, count, enabledCount);
            
            // Receive vertex attribute data
            NSMutableArray *clientAttribs = [NSMutableArray array];
            for (uint32_t i = 0; i < enabledCount; i++) {
                uint32_t index, size, type, stride, normalized;
                uint64_t dataSize;
                
                if (recv_all(sock, &index, sizeof(index)) != sizeof(index)) return;
                if (recv_all(sock, &size, sizeof(size)) != sizeof(size)) return;
                if (recv_all(sock, &type, sizeof(type)) != sizeof(type)) return;
                if (recv_all(sock, &normalized, sizeof(normalized)) != sizeof(normalized)) return;
                if (recv_all(sock, &stride, sizeof(stride)) != sizeof(stride)) return;
                if (recv_all(sock, &dataSize, sizeof(dataSize)) != sizeof(dataSize)) return;
                
                // Receive the actual vertex data (use recv_all to handle large data)
                void *vertexData = malloc(dataSize);
                if (recv_all(sock, vertexData, dataSize) != (ssize_t)dataSize) {
                    NSLog(@"[Metal Server] ERROR: Failed to receive %llu bytes of vertex data", dataSize);
                    free(vertexData);
                    return;
                }
                
                // Create Metal buffer with this data
                id<MTLBuffer> buffer = [self.device newBufferWithBytes:vertexData 
                                                                length:dataSize 
                                                               options:MTLResourceStorageModeShared];
                free(vertexData);
                
                // Store attribute info
                [clientAttribs addObject:@{
                    @"index": @(index),
                    @"size": @(size),
                    @"type": @(type),
                    @"buffer": buffer,
                    @"stride": @(stride)
                }];
                
                NSLog(@"[Metal Server]   Attribute %u: size=%u type=0x%X stride=%u dataSize=%llu", 
                      index, size, type, stride, dataSize);
            }
            
            // Render using client data
            dispatch_sync(dispatch_get_main_queue(), ^{
                [self renderFrameClientData:mode count:count attributes:clientAttribs];
            });
            break;
        }
        
        // ===== Phase 2: Shader Operations =====
        
        case CMD_METAL_CREATE_SHADER: {
            uint32_t shaderType;
            if (recv_all(sock, &shaderType, sizeof(shaderType)) != sizeof(shaderType)) return;
            
            uint32_t shaderID = _nextShaderID++;
            
            // Send shader ID back to client
            send(sock, &shaderID, sizeof(shaderID), 0);
            
            NSLog(@"[Metal Server] ✅ Created shader ID=%u type=0x%X", shaderID, shaderType);
            break;
        }
        
        case CMD_METAL_SHADER_SOURCE: {
            uint32_t shader, sourceLength;
            if (recv_all(sock, &shader, sizeof(shader)) != sizeof(shader)) return;
            if (recv_all(sock, &sourceLength, sizeof(sourceLength)) != sizeof(sourceLength)) return;
            
            char *sourceCode = malloc(sourceLength + 1);
            if (recv_all(sock, sourceCode, sourceLength) != (ssize_t)sourceLength) {
                free(sourceCode);
                return;
            }
            sourceCode[sourceLength] = '\0';
            
            NSString *glslSource = [NSString stringWithUTF8String:sourceCode];
            free(sourceCode);
            
            // Store the GLSL source (we'll translate it during compile)
            NSNumber *key = @(shader);
            
            // Determine shader type from context (vertex shaders are typically created first)
            // For now, assume we'll determine type during compile or link
            [_vertexShaderSources setObject:glslSource forKey:key];
            
            NSLog(@"[Metal Server] ✅ Shader %u source set (%u bytes)", shader, sourceLength);
            break;
        }
        
        case CMD_METAL_COMPILE_SHADER: {
            uint32_t shader, shaderType;
            if (recv_all(sock, &shader, sizeof(shader)) != sizeof(shader)) return;
            if (recv_all(sock, &shaderType, sizeof(shaderType)) != sizeof(shaderType)) return;
            
            NSNumber *key = @(shader);
            NSString *glslSource = [_vertexShaderSources objectForKey:key];
            
            if (!glslSource) {
                NSLog(@"[Metal Server] ❌ Shader %u has no source", shader);
                uint32_t success = 0;
                send(sock, &success, sizeof(success), 0);
                return;
            }
            
            // Translate GLSL to MSL
            NSString *mslSource = [self translateGLSLToMSL:glslSource shaderType:shaderType];
            
            NSLog(@"[Metal Server] 📝 Translated GLSL to MSL (Shader %u):\n%@", shader, mslSource);
            
            // Save shader source for debugging
            NSString *shaderPath = [NSString stringWithFormat:@"/tmp/shader_%u.metal", shader];
            [mslSource writeToFile:shaderPath atomically:YES encoding:NSUTF8StringEncoding error:nil];
            
            // Compile the MSL shader
            NSError *error = nil;
            id<MTLLibrary> library = [self.device newLibraryWithSource:mslSource options:nil error:&error];
            
            if (error || !library) {
                NSLog(@"[Metal Server] ❌ Shader %u compilation failed: %@", shader, error);
                uint32_t success = 0;
                send(sock, &success, sizeof(success), 0);
                return;
            }
            
            // Store compiled library
            [_programLibraries setObject:library forKey:key];
            
            // Extract function
            BOOL isVertex = (shaderType == 0x8B31);  // GL_VERTEX_SHADER
            id<MTLFunction> function = [library newFunctionWithName:isVertex ? @"vertex_main" : @"fragment_main"];
            
            if (function) {
                if (isVertex) {
                    [_vertexFunctions setObject:function forKey:key];
                } else {
                    [_fragmentFunctions setObject:function forKey:key];
                }
            }
            
            uint32_t success = 1;
            send(sock, &success, sizeof(success), 0);
            
            NSLog(@"[Metal Server] ✅ Shader %u compiled successfully", shader);
            break;
        }
        
        case CMD_METAL_DELETE_SHADER: {
            uint32_t shader;
            if (recv_all(sock, &shader, sizeof(shader)) != sizeof(shader)) return;
            
            NSNumber *key = @(shader);
            [_vertexShaderSources removeObjectForKey:key];
            [_fragmentShaderSources removeObjectForKey:key];
            [_programLibraries removeObjectForKey:key];
            [_vertexFunctions removeObjectForKey:key];
            [_fragmentFunctions removeObjectForKey:key];
            
            NSLog(@"[Metal Server] ✅ Deleted shader %u", shader);
            break;
        }
        
        case CMD_METAL_CREATE_PROGRAM: {
            uint32_t programID = _nextProgramID++;
            
            // Initialize program shader storage
            NSNumber *key = @(programID);
            [_programShaders setObject:[NSMutableDictionary dictionary] forKey:key];
            [_uniformLocations setObject:[NSMutableDictionary dictionary] forKey:key];
            
            // Send program ID back
            send(sock, &programID, sizeof(programID), 0);
            
            NSLog(@"[Metal Server] ✅ Created program ID=%u", programID);
            break;
        }
        
        case CMD_METAL_ATTACH_SHADER: {
            uint32_t program, shader, shaderType;
            if (recv_all(sock, &program, sizeof(program)) != sizeof(program)) return;
            if (recv_all(sock, &shader, sizeof(shader)) != sizeof(shader)) return;
            if (recv_all(sock, &shaderType, sizeof(shaderType)) != sizeof(shaderType)) return;
            
            NSNumber *progKey = @(program);
            NSNumber *shaderKey = @(shader);
            NSMutableDictionary *shaders = [_programShaders objectForKey:progKey];
            
            if (shaders) {
                BOOL isVertex = (shaderType == 0x8B31);  // GL_VERTEX_SHADER
                [shaders setObject:shaderKey forKey:isVertex ? @"vertex" : @"fragment"];
                
                NSLog(@"[Metal Server] ✅ Attached %@ shader %u to program %u", 
                      isVertex ? @"vertex" : @"fragment", shader, program);
            }
            break;
        }
        
        case CMD_METAL_LINK_PROGRAM: {
            uint32_t program;
            if (recv_all(sock, &program, sizeof(program)) != sizeof(program)) return;
            
            NSNumber *progKey = @(program);
            NSMutableDictionary *shaders = [_programShaders objectForKey:progKey];
            
            if (!shaders) {
                NSLog(@"[Metal Server] ❌ Program %u not found", program);
                uint32_t success = 0;
                send(sock, &success, sizeof(success), 0);
                return;
            }
            
            NSNumber *vertShaderKey = [shaders objectForKey:@"vertex"];
            NSNumber *fragShaderKey = [shaders objectForKey:@"fragment"];
            
            id<MTLFunction> vertexFunc = [_vertexFunctions objectForKey:vertShaderKey];
            id<MTLFunction> fragmentFunc = [_fragmentFunctions objectForKey:fragShaderKey];
            
            if (!vertexFunc || !fragmentFunc) {
                NSLog(@"[Metal Server] ❌ Program %u missing vertex or fragment function", program);
                uint32_t success = 0;
                send(sock, &success, sizeof(success), 0);
                return;
            }
            
            // Create pipeline state for this program
            MTLRenderPipelineDescriptor *pipelineDesc = [[MTLRenderPipelineDescriptor alloc] init];
            pipelineDesc.vertexFunction = vertexFunc;
            pipelineDesc.fragmentFunction = fragmentFunc;
            pipelineDesc.colorAttachments[0].pixelFormat = self.colorPixelFormat;
            
            // Set up vertex descriptor - dynamically parse attributes from vertex shader source
            MTLVertexDescriptor *vertexDesc = [[MTLVertexDescriptor alloc] init];
            
            // Parse vertex shader source to extract attribute declarations
            NSString *vertShaderSource = [_vertexShaderSources objectForKey:vertShaderKey];
            NSArray *lines = [vertShaderSource componentsSeparatedByString:@"\n"];
            
            NSMutableArray *attributes = [NSMutableArray array];
            for (NSString *line in lines) {
                NSString *trimmed = [line stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
                if ([trimmed hasPrefix:@"attribute "]) {
                    [attributes addObject:trimmed];
                }
            }
            
            // Build vertex descriptor from parsed attributes
            NSUInteger currentOffset = 0;
            NSUInteger totalStride = 0;
            int attribIndex = 0;
            
            for (NSString *attrDecl in attributes) {
                // Parse: "attribute vec3 position;" or "attribute vec2 texcoord;"
                NSString *trimmed = [attrDecl stringByReplacingOccurrencesOfString:@"attribute " withString:@""];
                trimmed = [trimmed stringByReplacingOccurrencesOfString:@";" withString:@""];
                
                NSArray *parts = [trimmed componentsSeparatedByString:@" "];
                if (parts.count >= 2) {
                    NSString *type = parts[0];
                    NSUInteger floatCount = 0;
                    MTLVertexFormat format;
                    
                    if ([type isEqualToString:@"vec2"] || [type isEqualToString:@"float2"]) {
                        format = MTLVertexFormatFloat2;
                        floatCount = 2;
                    } else if ([type isEqualToString:@"vec3"] || [type isEqualToString:@"float3"]) {
                        format = MTLVertexFormatFloat3;
                        floatCount = 3;
                    } else if ([type isEqualToString:@"vec4"] || [type isEqualToString:@"float4"]) {
                        format = MTLVertexFormatFloat4;
                        floatCount = 4;
                    } else {
                        NSLog(@"[Metal Server] ⚠️  Unknown attribute type: %@", type);
                        continue;
                    }
                    
                    vertexDesc.attributes[attribIndex].format = format;
                    vertexDesc.attributes[attribIndex].offset = 0;  // Each attribute at start of its own buffer
                    vertexDesc.attributes[attribIndex].bufferIndex = attribIndex;  // Each attribute from its own buffer index
                    
                    // Configure layout for this buffer
                    vertexDesc.layouts[attribIndex].stride = floatCount * sizeof(float);
                    vertexDesc.layouts[attribIndex].stepFunction = MTLVertexStepFunctionPerVertex;
                    
                    attribIndex++;
                }
            }
            
            NSLog(@"[Metal Server] Program %u: Created vertex descriptor with %d attributes (separate buffers)", 
                  program, attribIndex);
            
            pipelineDesc.vertexDescriptor = vertexDesc;
            pipelineDesc.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
            
            // Copy ALL settings from default pipeline
            pipelineDesc.colorAttachments[0].blendingEnabled = YES;
            pipelineDesc.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
            pipelineDesc.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;
            pipelineDesc.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
            pipelineDesc.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
            pipelineDesc.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorOne;
            pipelineDesc.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorZero;
            
            NSError *error = nil;
            id<MTLRenderPipelineState> pipeline = [self.device newRenderPipelineStateWithDescriptor:pipelineDesc error:&error];
            
            if (error || !pipeline) {
                NSLog(@"[Metal Server] ❌ Program %u link failed: %@", program, error);
                uint32_t success = 0;
                send(sock, &success, sizeof(success), 0);
                return;
            }
            
            [_shaderPipelines setObject:pipeline forKey:progKey];
            
            uint32_t success = 1;
            send(sock, &success, sizeof(success), 0);
            
            NSLog(@"[Metal Server] ✅ Program %u linked successfully", program);
            break;
        }
        
        case CMD_METAL_USE_PROGRAM: {
            uint32_t program;
            if (recv_all(sock, &program, sizeof(program)) != sizeof(program)) return;
            
            _currentProgram = program;
            
            NSLog(@"[Metal Server] ✅ Using program %u", program);
            break;
        }
        
        case CMD_METAL_DELETE_PROGRAM: {
            uint32_t program;
            if (recv_all(sock, &program, sizeof(program)) != sizeof(program)) return;
            
            NSNumber *key = @(program);
            [_programShaders removeObjectForKey:key];
            [_uniformLocations removeObjectForKey:key];
            [_shaderPipelines removeObjectForKey:key];
            
            NSLog(@"[Metal Server] ✅ Deleted program %u", program);
            break;
        }
        
        case CMD_METAL_GET_UNIFORM_LOCATION: {
            uint32_t program, nameLength;
            if (recv_all(sock, &program, sizeof(program)) != sizeof(program)) return;
            if (recv_all(sock, &nameLength, sizeof(nameLength)) != sizeof(nameLength)) return;
            
            char *uniformName = malloc(nameLength + 1);
            if (recv_all(sock, uniformName, nameLength) != (ssize_t)nameLength) {
                free(uniformName);
                return;
            }
            uniformName[nameLength] = '\0';
            
            // In Metal, uniform locations are managed differently
            // We'll use a simple index-based system
            NSNumber *progKey = @(program);
            NSMutableDictionary *uniforms = [_uniformLocations objectForKey:progKey];
            
            if (!uniforms) {
                uniforms = [NSMutableDictionary dictionary];
                [_uniformLocations setObject:uniforms forKey:progKey];
            }
            
            NSString *name = [NSString stringWithUTF8String:uniformName];
            NSNumber *location = [uniforms objectForKey:name];
            
            if (!location) {
                // Assign new location
                location = @([uniforms count]);
                [uniforms setObject:location forKey:name];
            }
            
            int32_t loc = [location intValue];
            send(sock, &loc, sizeof(loc), 0);
            
            NSLog(@"[Metal Server] ✅ Uniform '%s' location=%d in program %u", uniformName, loc, program);
            free(uniformName);
            break;
        }
        
        case CMD_METAL_GET_ATTRIB_LOCATION: {
            uint32_t program, nameLength;
            if (recv_all(sock, &program, sizeof(program)) != sizeof(program)) return;
            if (recv_all(sock, &nameLength, sizeof(nameLength)) != sizeof(nameLength)) return;
            
            char *attribName = malloc(nameLength + 1);
            if (recv_all(sock, attribName, nameLength) != (ssize_t)nameLength) {
                free(attribName);
                return;
            }
            attribName[nameLength] = '\0';
            
            // Parse vertex shader to find attribute location
            NSNumber *progKey = @(program);
            NSMutableDictionary *shaders = [_programShaders objectForKey:progKey];
            NSNumber *vertShaderKey = [shaders objectForKey:@"vertex"];
            NSString *vertShaderSource = [_vertexShaderSources objectForKey:vertShaderKey];
            
            int32_t location = -1;  // Not found by default
            
            if (vertShaderSource) {
                NSArray *lines = [vertShaderSource componentsSeparatedByString:@"\n"];
                int attribIndex = 0;
                
                for (NSString *line in lines) {
                    NSString *trimmed = [line stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
                    if ([trimmed hasPrefix:@"attribute "]) {
                        // Parse: "attribute vec3 position;" to extract "position"
                        NSString *decl = [trimmed stringByReplacingOccurrencesOfString:@"attribute " withString:@""];
                        decl = [decl stringByReplacingOccurrencesOfString:@";" withString:@""];
                        NSArray *parts = [decl componentsSeparatedByString:@" "];
                        
                        if (parts.count >= 2) {
                            NSString *attrNameInShader = parts[1];
                            NSString *searchName = [NSString stringWithUTF8String:attribName];
                            
                            if ([attrNameInShader isEqualToString:searchName]) {
                                location = attribIndex;
                                break;
                            }
                        }
                        attribIndex++;
                    }
                }
            }
            
            send(sock, &location, sizeof(location), 0);
            
            NSLog(@"[Metal Server] ✅ Attribute '%s' location=%d in program %u", attribName, location, program);
            free(attribName);
            break;
        }
        
        case CMD_METAL_UNIFORM_1F: {
            int32_t location;
            float value;
            if (recv_all(sock, &location, sizeof(location)) != sizeof(location)) return;
            if (recv_all(sock, &value, sizeof(value)) != sizeof(value)) return;
            
            // Store uniform value (would be passed to shader during rendering)
            NSLog(@"[Metal Server] ✅ Uniform1f location=%d value=%.2f", location, value);
            break;
        }
        
        case CMD_METAL_UNIFORM_2F: {
            int32_t location;
            float values[2];
            if (recv_all(sock, &location, sizeof(location)) != sizeof(location)) return;
            if (recv_all(sock, values, sizeof(values)) != sizeof(values)) return;
            
            NSLog(@"[Metal Server] ✅ Uniform2f location=%d values=(%.2f, %.2f)", 
                  location, values[0], values[1]);
            break;
        }
        
        case CMD_METAL_UNIFORM_3F: {
            int32_t location;
            float values[3];
            if (recv_all(sock, &location, sizeof(location)) != sizeof(location)) return;
            if (recv_all(sock, values, sizeof(values)) != sizeof(values)) return;
            
            NSLog(@"[Metal Server] ✅ Uniform3f location=%d values=(%.2f, %.2f, %.2f)", 
                  location, values[0], values[1], values[2]);
            break;
        }
        
        case CMD_METAL_UNIFORM_4F: {
            int32_t location;
            float values[4];
            if (recv_all(sock, &location, sizeof(location)) != sizeof(location)) return;
            if (recv_all(sock, values, sizeof(values)) != sizeof(values)) return;
            
            NSLog(@"[Metal Server] ✅ Uniform4f location=%d values=(%.2f, %.2f, %.2f, %.2f)", 
                  location, values[0], values[1], values[2], values[3]);
            break;
        }
        
        case CMD_METAL_UNIFORM_1I: {
            int32_t location, value;
            if (recv_all(sock, &location, sizeof(location)) != sizeof(location)) return;
            if (recv_all(sock, &value, sizeof(value)) != sizeof(value)) return;
            
            NSLog(@"[Metal Server] ✅ Uniform1i location=%d value=%d", location, value);
            break;
        }
        
        case CMD_METAL_UNIFORM_2FV: {
            int32_t location, count;
            if (recv_all(sock, &location, sizeof(location)) != sizeof(location)) return;
            if (recv_all(sock, &count, sizeof(count)) != sizeof(count)) return;
            
            float *values = malloc(count * 2 * sizeof(float));
            if (recv_all(sock, values, count * 2 * sizeof(float)) != (ssize_t)(count * 2 * sizeof(float))) {
                free(values);
                return;
            }
            
            NSLog(@"[Metal Server] ✅ Uniform2fv location=%d count=%d first=(%.2f, %.2f)", 
                  location, count, values[0], values[1]);
            free(values);
            break;
        }
        
        case CMD_METAL_UNIFORM_MATRIX_4FV: {
            int32_t location, count;
            uint8_t transpose;
            if (recv_all(sock, &location, sizeof(location)) != sizeof(location)) return;
            if (recv_all(sock, &count, sizeof(count)) != sizeof(count)) return;
            if (recv_all(sock, &transpose, sizeof(transpose)) != sizeof(transpose)) return;
            
            float *matrices = malloc(count * 16 * sizeof(float));
            if (recv_all(sock, matrices, count * 16 * sizeof(float)) != (ssize_t)(count * 16 * sizeof(float))) {
                free(matrices);
                return;
            }
            
            // Store matrix values in uniform dictionary for current program
            NSNumber *progKey = @(_currentProgram);
            NSMutableDictionary *programUniforms = _programUniformData[progKey];
            if (!programUniforms) {
                programUniforms = [[NSMutableDictionary alloc] init];
                _programUniformData[progKey] = programUniforms;
            }
            
            NSData *matrixData = [NSData dataWithBytes:matrices length:count * 16 * sizeof(float)];
            NSNumber *locKey = @(location);
            programUniforms[locKey] = @{
                @"type": @"mat4",
                @"count": @(count),
                @"transpose": @(transpose),
                @"data": matrixData
            };
            
            NSLog(@"[Metal Server] ✅ UniformMatrix4fv location=%d count=%d transpose=%d (stored for program %u)", 
                  location, count, transpose, _currentProgram);
            free(matrices);
            break;
        }
        
        // ===== Phase 3: Texture Operations =====
        
        case CMD_METAL_GEN_TEXTURES: {
            uint32_t count;
            if (recv_all(sock, &count, sizeof(count)) != sizeof(count)) return;
            
            uint32_t *textureIDs = malloc(count * sizeof(uint32_t));
            for (uint32_t i = 0; i < count; i++) {
                textureIDs[i] = _nextTextureID++;
            }
            
            send(sock, textureIDs, count * sizeof(uint32_t), 0);
            NSLog(@"[Metal Server] ✅ Generated %u texture(s), IDs: %u-%u", 
                  count, textureIDs[0], textureIDs[count-1]);
            free(textureIDs);
            break;
        }
        
        case CMD_METAL_BIND_TEXTURE: {
            uint32_t target, texture;
            if (recv_all(sock, &target, sizeof(target)) != sizeof(target)) return;
            if (recv_all(sock, &texture, sizeof(texture)) != sizeof(texture)) return;
            
            _currentTexture2D = texture;
            NSLog(@"[Metal Server] Bind texture: target=0x%X texture=%u", target, texture);
            break;
        }
        
        case CMD_METAL_TEX_IMAGE_2D: {
            uint32_t target, width, height, format, type;
            int32_t level, internalFormat, border;
            
            if (recv_all(sock, &target, sizeof(target)) != sizeof(target)) return;
            if (recv_all(sock, &level, sizeof(level)) != sizeof(level)) return;
            if (recv_all(sock, &internalFormat, sizeof(internalFormat)) != sizeof(internalFormat)) return;
            if (recv_all(sock, &width, sizeof(width)) != sizeof(width)) return;
            if (recv_all(sock, &height, sizeof(height)) != sizeof(height)) return;
            if (recv_all(sock, &border, sizeof(border)) != sizeof(border)) return;
            if (recv_all(sock, &format, sizeof(format)) != sizeof(format)) return;
            if (recv_all(sock, &type, sizeof(type)) != sizeof(type)) return;
            
            NSLog(@"[Metal Server] TexImage2D: %ux%u format=0x%X type=0x%X", width, height, format, type);
            
            // Calculate bytes per pixel based on format
            uint32_t components = 4; // Default RGBA
            if (format == 0x1907) components = 3;      // GL_RGB
            else if (format == 0x1908) components = 4; // GL_RGBA
            else if (format == 0x1909) components = 1; // GL_LUMINANCE
            else if (format == 0x190A) components = 2; // GL_LUMINANCE_ALPHA
            else if (format == 0x1903) components = 1; // GL_RED
            else if (format == 0x8227) components = 2; // GL_RG
            
            uint32_t bytesPerComponent = 1; // Default unsigned byte
            if (type == 0x1401) bytesPerComponent = 1;      // GL_UNSIGNED_BYTE
            else if (type == 0x1405) bytesPerComponent = 4; // GL_UNSIGNED_INT
            else if (type == 0x1406) bytesPerComponent = 4; // GL_FLOAT
            
            uint32_t bytesPerPixel = components * bytesPerComponent;
            uint64_t dataSize = (uint64_t)width * height * bytesPerPixel;
            
            NSLog(@"[Metal Server] Expecting %llu bytes (%u components × %u bytes)", 
                  dataSize, components, bytesPerComponent);
            
            void *pixels = malloc(dataSize);
            if (!pixels) {
                NSLog(@"[Metal Server] ERROR: Failed to allocate %llu bytes for texture", dataSize);
                return;
            }
            
            // Receive pixel data using recv_all for large textures
            if (recv_all(sock, pixels, dataSize) != (ssize_t)dataSize) {
                NSLog(@"[Metal Server] ERROR: Failed to receive %llu bytes of texture data", dataSize);
                free(pixels);
                return;
            }
            
            NSLog(@"[Metal Server] ✅ Received %llu bytes of texture data", dataSize);
            
            // Convert RGB to RGBA if needed (Metal doesn't support RGB directly)
            void *uploadPixels = pixels;
            uint32_t uploadBytesPerRow = width * bytesPerPixel;
            
            if (components == 3 && bytesPerComponent == 1) {
                // RGB8 → RGBA8 conversion
                size_t rgbaSize = width * height * 4;
                uint8_t *rgbaPixels = malloc(rgbaSize);
                uint8_t *src = (uint8_t *)pixels;
                uint8_t *dst = rgbaPixels;
                
                for (uint32_t i = 0; i < width * height; i++) {
                    dst[0] = src[0]; // R
                    dst[1] = src[1]; // G
                    dst[2] = src[2]; // B
                    dst[3] = 255;    // A
                    src += 3;
                    dst += 4;
                }
                
                uploadPixels = rgbaPixels;
                uploadBytesPerRow = width * 4;
                NSLog(@"[Metal Server] Converted RGB→RGBA (%llu → %zu bytes)", dataSize, rgbaSize);
            }
            
            // Create Metal texture
            MTLTextureDescriptor *descriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                                                                  width:width
                                                                                                 height:height
                                                                                              mipmapped:NO];
            descriptor.usage = MTLTextureUsageShaderRead | MTLTextureUsageRenderTarget;
            
            id<MTLTexture> texture = [self.device newTextureWithDescriptor:descriptor];
            if (!texture) {
                NSLog(@"[Metal Server] ERROR: Failed to create Metal texture");
                if (uploadPixels != pixels) free(uploadPixels);
                free(pixels);
                return;
            }
            
            // Upload pixels to texture
            MTLRegion region = MTLRegionMake2D(0, 0, width, height);
            [texture replaceRegion:region mipmapLevel:level withBytes:uploadPixels bytesPerRow:uploadBytesPerRow];
            
            // Cleanup
            if (uploadPixels != pixels) free(uploadPixels);
            
            // Store in registry
            _textureRegistry[@(_currentTexture2D)] = texture;
            
            free(pixels);
            NSLog(@"[Metal Server] ✅ Texture uploaded: texture=%u size=%ux%u bytes=%llu", 
                  _currentTexture2D, width, height, dataSize);
            break;
        }
        
        case CMD_METAL_TEX_SUB_IMAGE_2D: {
            uint32_t target, width, height, format, type;
            int32_t level, xoffset, yoffset;
            
            if (recv_all(sock, &target, sizeof(target)) != sizeof(target)) return;
            if (recv_all(sock, &level, sizeof(level)) != sizeof(level)) return;
            if (recv_all(sock, &xoffset, sizeof(xoffset)) != sizeof(xoffset)) return;
            if (recv_all(sock, &yoffset, sizeof(yoffset)) != sizeof(yoffset)) return;
            if (recv_all(sock, &width, sizeof(width)) != sizeof(width)) return;
            if (recv_all(sock, &height, sizeof(height)) != sizeof(height)) return;
            if (recv_all(sock, &format, sizeof(format)) != sizeof(format)) return;
            if (recv_all(sock, &type, sizeof(type)) != sizeof(type)) return;
            
            NSLog(@"[Metal Server] TexSubImage2D: offset=(%u,%u) size=%ux%u", xoffset, yoffset, width, height);
            
            uint32_t bytesPerPixel = 4;
            uint64_t dataSize = width * height * bytesPerPixel;
            
            void *pixels = malloc(dataSize);
            if (!pixels) return;
            
            size_t totalReceived = 0;
            while (totalReceived < dataSize) {
                ssize_t received = recv_all(sock, (char*)pixels + totalReceived, 
                                       dataSize - totalReceived);
                if (received <= 0) {
                    free(pixels);
                    return;
                }
                totalReceived += received;
            }
            
            id<MTLTexture> texture = _textureRegistry[@(_currentTexture2D)];
            if (texture) {
                MTLRegion region = MTLRegionMake2D(xoffset, yoffset, width, height);
                [texture replaceRegion:region mipmapLevel:level withBytes:pixels bytesPerRow:width * bytesPerPixel];
                NSLog(@"[Metal Server] ✅ Texture region updated");
            }
            
            free(pixels);
            break;
        }
        
        case CMD_METAL_TEX_PARAMETERI: {
            uint32_t target, pname, param;
            if (recv_all(sock, &target, sizeof(target)) != sizeof(target)) return;
            if (recv_all(sock, &pname, sizeof(pname)) != sizeof(pname)) return;
            if (recv_all(sock, &param, sizeof(param)) != sizeof(param)) return;
            
            NSLog(@"[Metal Server] TexParameteri: pname=0x%X param=0x%X", pname, param);
            
            // Store texture parameters in metadata (will be used when creating sampler)
            NSMutableDictionary *metadata = _textureMetadata[@(_currentTexture2D)];
            if (!metadata) {
                metadata = [NSMutableDictionary dictionary];
                _textureMetadata[@(_currentTexture2D)] = metadata;
            }
            metadata[@(pname)] = @(param);
            break;
        }
        
        case CMD_METAL_ACTIVE_TEXTURE: {
            uint32_t textureUnit;
            if (recv_all(sock, &textureUnit, sizeof(textureUnit)) != sizeof(textureUnit)) return;
            
            _activeTextureUnit = textureUnit;
            NSLog(@"[Metal Server] Active texture: unit=%u", textureUnit);
            break;
        }
        
        case CMD_METAL_GENERATE_MIPMAP: {
            uint32_t target;
            if (recv_all(sock, &target, sizeof(target)) != sizeof(target)) return;
            
            NSLog(@"[Metal Server] Generate mipmap: target=0x%X (not implemented)", target);
            // Metal requires textures to be created with mipmapped:YES
            // For now, this is a no-op
            break;
        }
        
        case CMD_METAL_DELETE_TEXTURES: {
            uint32_t count;
            if (recv_all(sock, &count, sizeof(count)) != sizeof(count)) return;
            
            uint32_t *textureIDs = malloc(count * sizeof(uint32_t));
            if (recv_all(sock, textureIDs, count * sizeof(uint32_t)) != (ssize_t)(count * sizeof(uint32_t))) {
                free(textureIDs);
                return;
            }
            
            for (uint32_t i = 0; i < count; i++) {
                [_textureRegistry removeObjectForKey:@(textureIDs[i])];
                [_textureMetadata removeObjectForKey:@(textureIDs[i])];
                [_samplerRegistry removeObjectForKey:@(textureIDs[i])];
            }
            
            NSLog(@"[Metal Server] ✅ Deleted %u texture(s)", count);
            free(textureIDs);
            break;
        }
        
        // ===== Phase 4: Render State Management =====
        
        case CMD_METAL_ENABLE: {
            uint32_t cap;
            if (recv_all(sock, &cap, sizeof(cap)) != sizeof(cap)) return;
            
            if (cap == 0x0BE2) {  // GL_BLEND
                _blendEnabled = YES;
                NSLog(@"[Metal Server] Enable: GL_BLEND");
            } else if (cap == 0x0B71) {  // GL_DEPTH_TEST
                _depthTestEnabled = YES;
                NSLog(@"[Metal Server] Enable: GL_DEPTH_TEST");
            } else if (cap == 0x0B44) {  // GL_CULL_FACE
                _cullFaceEnabled = YES;
                NSLog(@"[Metal Server] Enable: GL_CULL_FACE");
            } else {
                NSLog(@"[Metal Server] Enable: 0x%X (not implemented)", cap);
            }
            break;
        }
        
        case CMD_METAL_DISABLE: {
            uint32_t cap;
            if (recv_all(sock, &cap, sizeof(cap)) != sizeof(cap)) return;
            
            if (cap == 0x0BE2) {  // GL_BLEND
                _blendEnabled = NO;
                NSLog(@"[Metal Server] Disable: GL_BLEND");
            } else if (cap == 0x0B71) {  // GL_DEPTH_TEST
                _depthTestEnabled = NO;
                NSLog(@"[Metal Server] Disable: GL_DEPTH_TEST");
            } else if (cap == 0x0B44) {  // GL_CULL_FACE
                _cullFaceEnabled = NO;
                NSLog(@"[Metal Server] Disable: GL_CULL_FACE");
            } else {
                NSLog(@"[Metal Server] Disable: 0x%X (not implemented)", cap);
            }
            break;
        }
        
        case CMD_METAL_BLEND_FUNC: {
            uint32_t sfactor, dfactor;
            if (recv_all(sock, &sfactor, sizeof(sfactor)) != sizeof(sfactor)) return;
            if (recv_all(sock, &dfactor, sizeof(dfactor)) != sizeof(dfactor)) return;
            
            _blendSrcFactor = sfactor;
            _blendDstFactor = dfactor;
            NSLog(@"[Metal Server] BlendFunc: src=0x%X dst=0x%X", sfactor, dfactor);
            break;
        }
        
        case CMD_METAL_BLEND_EQUATION: {
            uint32_t mode;
            if (recv_all(sock, &mode, sizeof(mode)) != sizeof(mode)) return;
            
            _blendEquation = mode;
            NSLog(@"[Metal Server] BlendEquation: mode=0x%X", mode);
            break;
        }
        
        case CMD_METAL_DEPTH_FUNC: {
            uint32_t func;
            if (recv_all(sock, &func, sizeof(func)) != sizeof(func)) return;
            
            _depthFunc = func;
            NSLog(@"[Metal Server] DepthFunc: func=0x%X", func);
            break;
        }
        
        case CMD_METAL_DEPTH_MASK: {
            uint32_t flag;
            if (recv_all(sock, &flag, sizeof(flag)) != sizeof(flag)) return;
            
            _depthWriteEnabled = (flag != 0);
            NSLog(@"[Metal Server] DepthMask: %s", _depthWriteEnabled ? "enabled" : "disabled");
            break;
        }
        
        case CMD_METAL_CULL_FACE: {
            uint32_t mode;
            if (recv_all(sock, &mode, sizeof(mode)) != sizeof(mode)) return;
            
            _cullFaceMode = mode;
            NSLog(@"[Metal Server] CullFace: mode=0x%X", mode);
            break;
        }
        
        case CMD_METAL_FRONT_FACE: {
            uint32_t mode;
            if (recv_all(sock, &mode, sizeof(mode)) != sizeof(mode)) return;
            
            _frontFace = mode;
            NSLog(@"[Metal Server] FrontFace: mode=0x%X", mode);
            break;
        }
        
        // Phase 5: Framebuffer Objects
        case CMD_METAL_GEN_FRAMEBUFFERS: {
            uint32_t count;
            if (recv_all(sock, &count, sizeof(count)) != sizeof(count)) return;
            
            for (uint32_t i = 0; i < count; i++) {
                uint32_t fboID = _nextFramebufferID++;
                
                // Create FBO descriptor with empty attachments
                NSMutableDictionary *fboDesc = [NSMutableDictionary dictionary];
                fboDesc[@"colorAttachment"] = [NSNull null];
                fboDesc[@"depthAttachment"] = [NSNull null];
                fboDesc[@"stencilAttachment"] = [NSNull null];
                fboDesc[@"width"] = @(0);
                fboDesc[@"height"] = @(0);
                
                [_framebufferRegistry setObject:fboDesc forKey:@(fboID)];
                
                // Send FBO ID back to client
                send(sock, &fboID, sizeof(fboID), 0);
            }
            
            NSLog(@"[Metal Server] ✅ Generated %u framebuffer(s), IDs: %u-%u", count, _nextFramebufferID - count, _nextFramebufferID - 1);
            break;
        }
        
        case CMD_METAL_BIND_FRAMEBUFFER: {
            uint32_t target, framebuffer;
            if (recv_all(sock, &target, sizeof(target)) != sizeof(target)) return;
            if (recv_all(sock, &framebuffer, sizeof(framebuffer)) != sizeof(framebuffer)) return;
            
            _currentFramebuffer = framebuffer;
            NSLog(@"[Metal Server] Bind framebuffer: target=0x%X framebuffer=%u", target, framebuffer);
            break;
        }
        
        case CMD_METAL_DELETE_FRAMEBUFFERS: {
            uint32_t count;
            if (recv_all(sock, &count, sizeof(count)) != sizeof(count)) return;
            
            for (uint32_t i = 0; i < count; i++) {
                uint32_t fboID;
                if (recv_all(sock, &fboID, sizeof(fboID)) != sizeof(fboID)) return;
                
                [_framebufferRegistry removeObjectForKey:@(fboID)];
                
                if (_currentFramebuffer == fboID) {
                    _currentFramebuffer = 0;
                }
            }
            
            NSLog(@"[Metal Server] ✅ Deleted %u framebuffer(s)", count);
            break;
        }
        
        case CMD_METAL_FIXED_FUNCTION_DRAW: {
            // Phase 7.2: Server-side legacy OpenGL rendering
            // Receive: modelview (64) + projection (64) + primitiveType (4) + vertexCount (4) + vertices (count*48)
            
            float modelview[16];
            float projection[16];
            uint32_t primitiveType;
            uint32_t vertexCount;
            
            // Receive modelview matrix (64 bytes)
            if (recv_all(sock, modelview, 64) != 64) {
                NSLog(@"[Metal Server] ❌ Failed to receive modelview matrix");
                return;
            }
            
            // Receive projection matrix (64 bytes)
            if (recv_all(sock, projection, 64) != 64) {
                NSLog(@"[Metal Server] ❌ Failed to receive projection matrix");
                return;
            }
            
            // Receive primitive type (GL_POINTS/LINES/TRIANGLES/etc)
            if (recv_all(sock, &primitiveType, sizeof(primitiveType)) != sizeof(primitiveType)) {
                NSLog(@"[Metal Server] ❌ Failed to receive primitive type");
                return;
            }
            
            // Receive vertex count
            if (recv_all(sock, &vertexCount, sizeof(vertexCount)) != sizeof(vertexCount)) {
                NSLog(@"[Metal Server] ❌ Failed to receive vertex count");
                return;
            }
            
            if (vertexCount == 0) {
                NSLog(@"[Metal Server] ⚠️  Empty draw call (0 vertices)");
                break;
            }
            
            // Receive vertex data (position=12, color=16, normal=12, texcoord=8 = 48 bytes per vertex)
            size_t vertexDataSize = vertexCount * 48;
            float *vertexData = (float *)malloc(vertexDataSize);
            if (!vertexData) {
                NSLog(@"[Metal Server] ❌ Failed to allocate vertex buffer (%zu bytes)", vertexDataSize);
                return;
            }
            
            if (recv_all(sock, vertexData, vertexDataSize) != vertexDataSize) {
                NSLog(@"[Metal Server] ❌ Failed to receive vertex data (%zu bytes)", vertexDataSize);
                free(vertexData);
                return;
            }
            
            NSLog(@"[Metal Server] 📦 Fixed-function draw: %u vertices, primitive type 0x%X", vertexCount, primitiveType);
            
            // Get or create fixed-function pipeline
            id<MTLRenderPipelineState> pipeline = [self createFixedFunctionPipeline];
            if (!pipeline) {
                NSLog(@"[Metal Server] ❌ Failed to get fixed-function pipeline");
                free(vertexData);
                break;
            }
            
            // Convert OpenGL primitive type to Metal primitive type
            MTLPrimitiveType metalPrimitive;
            BOOL needsQuadConversion = NO;
            BOOL isQuadStrip = NO;
            
            switch (primitiveType) {
                case 0x0000: metalPrimitive = MTLPrimitiveTypePoint; break;       // GL_POINTS
                case 0x0001: metalPrimitive = MTLPrimitiveTypeLine; break;        // GL_LINES
                case 0x0002: metalPrimitive = MTLPrimitiveTypeLineStrip; break;   // GL_LINE_STRIP
                case 0x0003: metalPrimitive = MTLPrimitiveTypeLineStrip; break;   // GL_LINE_LOOP (approximate with strip)
                case 0x0004: metalPrimitive = MTLPrimitiveTypeTriangle; break;    // GL_TRIANGLES
                case 0x0005: metalPrimitive = MTLPrimitiveTypeTriangleStrip; break; // GL_TRIANGLE_STRIP
                case 0x0006: metalPrimitive = MTLPrimitiveTypeTriangleStrip; break; // GL_TRIANGLE_FAN (approximate)
                case 0x0007: // GL_QUAD_STRIP - convert to triangle strip
                    metalPrimitive = MTLPrimitiveTypeTriangleStrip;
                    isQuadStrip = YES;
                    NSLog(@"[Metal Server] 🔄 Converting GL_QUAD_STRIP to triangle strip");
                    break;
                case 0x0008: // GL_QUADS - need to convert to triangles
                    metalPrimitive = MTLPrimitiveTypeTriangle;
                    needsQuadConversion = YES;
                    NSLog(@"[Metal Server] 🔄 Converting GL_QUADS to triangles");
                    break;
                case 0x0009: // GL_POLYGON - treat as triangle fan
                    metalPrimitive = MTLPrimitiveTypeTriangleStrip;
                    break;
                default:
                    NSLog(@"[Metal Server] ⚠️  Unknown primitive type 0x%X, defaulting to triangles", primitiveType);
                    metalPrimitive = MTLPrimitiveTypeTriangle;
                    break;
            }
            
            // Create uniform buffer: modelview (64 bytes) + projection (64 bytes) = 128 bytes
            id<MTLBuffer> uniformBuffer = [self.device newBufferWithLength:128 options:MTLResourceStorageModeShared];
            if (!uniformBuffer) {
                NSLog(@"[Metal Server] ❌ Failed to create uniform buffer");
                free(vertexData);
                break;
            }
            
            float *uniformData = (float*)[uniformBuffer contents];
            memcpy(uniformData, modelview, 64);      // First 16 floats (64 bytes)
            memcpy(uniformData + 16, projection, 64); // Next 16 floats (64 bytes)
            
            // Convert quads to triangles if needed
            float *finalVertexData = vertexData;
            size_t finalVertexDataSize = vertexDataSize;
            uint32_t finalVertexCount = vertexCount;
            
            if (needsQuadConversion) {
                // GL_QUADS: every 4 vertices = 1 quad → need 6 vertices (2 triangles) per quad
                // Each vertex = 12 floats (position=3, color=4, normal=3, texcoord=2)
                uint32_t numQuads = vertexCount / 4;
                finalVertexCount = numQuads * 6;  // 2 triangles per quad
                finalVertexDataSize = finalVertexCount * 48;  // 48 bytes per vertex
                finalVertexData = (float*)malloc(finalVertexDataSize);
                
                const int floatsPerVertex = 12;  // 48 bytes / 4 bytes per float
                
                for (uint32_t i = 0; i < numQuads; i++) {
                    // Input: 4 vertices forming a quad (v0, v1, v2, v3)
                    // Output: 6 vertices forming 2 triangles (v0, v1, v2) and (v0, v2, v3)
                    float *v0 = &vertexData[(i * 4 + 0) * floatsPerVertex];
                    float *v1 = &vertexData[(i * 4 + 1) * floatsPerVertex];
                    float *v2 = &vertexData[(i * 4 + 2) * floatsPerVertex];
                    float *v3 = &vertexData[(i * 4 + 3) * floatsPerVertex];
                    
                    float *out = &finalVertexData[i * 6 * floatsPerVertex];
                    
                    // First triangle: v0, v1, v2
                    memcpy(out + 0 * floatsPerVertex, v0, 48);
                    memcpy(out + 1 * floatsPerVertex, v1, 48);
                    memcpy(out + 2 * floatsPerVertex, v2, 48);
                    
                    // Second triangle: v0, v2, v3
                    memcpy(out + 3 * floatsPerVertex, v0, 48);
                    memcpy(out + 4 * floatsPerVertex, v2, 48);
                    memcpy(out + 5 * floatsPerVertex, v3, 48);
                }
                
                NSLog(@"[Metal Server] ✅ Converted %u quads to %u triangles (%u vertices)", numQuads, numQuads * 2, finalVertexCount);
            }
            
            // Create vertex buffer from final vertex data
            id<MTLBuffer> vertexBuffer = [self.device newBufferWithBytes:finalVertexData 
                                                                   length:finalVertexDataSize 
                                                                  options:MTLResourceStorageModeShared];
            if (!vertexBuffer) {
                NSLog(@"[Metal Server] ❌ Failed to create vertex buffer");
                if (needsQuadConversion) free(finalVertexData);
                free(vertexData);
                break;
            }
            
            // Free converted data if we allocated it
            if (needsQuadConversion && finalVertexData != vertexData) {
                free(finalVertexData);
            }
            
            // Create command buffer and render encoder
            id<MTLCommandBuffer> commandBuffer = [_commandQueue commandBuffer];
            
            // Determine which render pass descriptor to use
            MTLRenderPassDescriptor *renderPassDescriptor = nil;
            if (_currentFramebuffer == 0) {
                // Render to default framebuffer
                if (_renderToVM) {
                    // Create VM display texture if it doesn't exist yet
                    if (!_vmDisplayTexture) {
                        [self createVMDisplayTexturesWithWidth:_vmDisplayWidth height:_vmDisplayHeight];
                    }
                    
                    // Render to VM display texture (offscreen for pixel readback)
                    renderPassDescriptor = [MTLRenderPassDescriptor renderPassDescriptor];
                    renderPassDescriptor.colorAttachments[0].texture = _vmDisplayTexture;
                    
                    // Apply clear on first draw after glClear(), then load for rest of frame
                    if (_needsClear && !_frameClearApplied) {
                        renderPassDescriptor.colorAttachments[0].loadAction = MTLLoadActionClear;
                        renderPassDescriptor.colorAttachments[0].clearColor = _metalClearColor;
                        _frameClearApplied = YES;  // Mark that we cleared this frame
                        NSLog(@"[Metal Server] ✅ CLEARING NOW: needsClear=%d→%d, frameClearApplied=NO→YES", _needsClear, _needsClear);
                    } else {
                        renderPassDescriptor.colorAttachments[0].loadAction = MTLLoadActionLoad;
                        if (_needsClear) {
                            NSLog(@"[Metal Server] ⚠️ LOAD (already cleared this frame): frameClearApplied=%d", _frameClearApplied);
                        }
                    }
                    
                    renderPassDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;
                    
                    if (_vmDisplayDepthTexture) {
                        renderPassDescriptor.depthAttachment.texture = _vmDisplayDepthTexture;
                        renderPassDescriptor.depthAttachment.loadAction = MTLLoadActionClear;  // Clear depth each frame
                        renderPassDescriptor.depthAttachment.storeAction = MTLStoreActionStore;
                        renderPassDescriptor.depthAttachment.clearDepth = 1.0;
                        renderPassDescriptor.depthAttachment.clearDepth = 1.0;
                    }
                } else {
                    // Render to host window (original behavior)
                    renderPassDescriptor = self.currentRenderPassDescriptor;
                    if (!renderPassDescriptor) {
                        NSLog(@"[Metal Server] ❌ No render pass descriptor available");
                        free(vertexData);
                        break;
                    }
                }
            } else {
                // Render to FBO
                NSDictionary *fbo = _framebufferRegistry[@(_currentFramebuffer)];
                if (!fbo) {
                    NSLog(@"[Metal Server] ❌ FBO %u not found", _currentFramebuffer);
                    free(vertexData);
                    break;
                }
                
                renderPassDescriptor = [MTLRenderPassDescriptor renderPassDescriptor];
                
                // Set color attachment
                NSNumber *colorAttachment = fbo[@"colorAttachment"];
                if (colorAttachment) {
                    id<MTLTexture> colorTexture = _textureRegistry[colorAttachment];
                    if (colorTexture) {
                        renderPassDescriptor.colorAttachments[0].texture = colorTexture;
                        renderPassDescriptor.colorAttachments[0].loadAction = MTLLoadActionLoad;
                        renderPassDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;
                    }
                }
                
                // Set depth attachment if present
                NSNumber *depthAttachment = fbo[@"depthAttachment"];
                if (depthAttachment) {
                    id<MTLTexture> depthTexture = _renderbufferRegistry[depthAttachment];
                    if (depthTexture) {
                        renderPassDescriptor.depthAttachment.texture = depthTexture;
                        renderPassDescriptor.depthAttachment.loadAction = MTLLoadActionLoad;
                        renderPassDescriptor.depthAttachment.storeAction = MTLStoreActionStore;
                    }
                }
            }
            
            // Create render encoder
            id<MTLRenderCommandEncoder> renderEncoder = [commandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor];
            
            // Set pipeline state
            [renderEncoder setRenderPipelineState:pipeline];
            
            // Set depth stencil state
            [renderEncoder setDepthStencilState:_depthStencilState];
            
            // Bind vertex buffer (buffer 0)
            [renderEncoder setVertexBuffer:vertexBuffer offset:0 atIndex:0];
            
            // Bind uniform buffer (buffer 1)
            [renderEncoder setVertexBuffer:uniformBuffer offset:0 atIndex:1];
            
            // Draw primitives (use finalVertexCount after quad conversion)
            [renderEncoder drawPrimitives:metalPrimitive vertexStart:0 vertexCount:finalVertexCount];
            
            [renderEncoder endEncoding];
            
            // Commit command buffer
            if (_currentFramebuffer == 0 && !_renderToVM) {
                // Present to host window only if not rendering to VM
                [commandBuffer presentDrawable:self.currentDrawable];
                NSLog(@"[Metal Server] 🖥️  Presenting to host window");
            } else if (_renderToVM) {
                NSLog(@"[Metal Server] 📺 Rendering to VM display texture (%lux%lu)", _vmDisplayWidth, _vmDisplayHeight);
            }
            [commandBuffer commit];
            NSLog(@"[Metal Server] 📤 Command buffer committed to GPU");
            
            NSLog(@"[Metal Server] ✅ Fixed-function draw complete: %u vertices with primitive type 0x%X", vertexCount, primitiveType);
            
            free(vertexData);
            break;
        }
        
        case CMD_METAL_SWAP_BUFFERS: {
            // Phase 7.6: glutSwapBuffers equivalent - trigger frame completion and display
            // This command indicates the VM app has finished rendering a frame and wants to display it
            
            // If we haven't created VM display textures yet, create them now (default 800x600)
            if (!_vmDisplayTexture) {
                [self createVMDisplayTexturesWithWidth:_vmDisplayWidth height:_vmDisplayHeight];
            }
            
            // Display the frame directly using CAMetalLayer
            dispatch_async(dispatch_get_main_queue(), ^{
                CAMetalLayer *layer = (CAMetalLayer *)self.layer;
                id<CAMetalDrawable> drawable = [layer nextDrawable];
                if (drawable && _vmDisplayTexture) {
                    NSLog(@"[Metal Server] 🖼️  Presenting frame to window");
                    [self blitVMTextureToWindowUsingDrawable:drawable];
                }
            });
            
            // IMPORTANT: Reset flags for next frame
            // The next frame should clear once at the start
            _frameClearApplied = NO;
            _needsClear = YES;  // Force clear at start of next frame
            
            NSLog(@"[Metal Server] ✅ Frame displayed, next frame will clear");
            break;
        }
        
        case CMD_METAL_READ_PIXELS: {
            // Phase 7.6: glReadPixels - read framebuffer and send pixels back to VM
            // Format: x (4), y (4), width (4), height (4), format (4), type (4)
            
            uint32_t x, y, width, height, format, type;
            
            if (recv_all(sock, &x, sizeof(x)) != sizeof(x)) return;
            if (recv_all(sock, &y, sizeof(y)) != sizeof(y)) return;
            if (recv_all(sock, &width, sizeof(width)) != sizeof(width)) return;
            if (recv_all(sock, &height, sizeof(height)) != sizeof(height)) return;
            if (recv_all(sock, &format, sizeof(format)) != sizeof(format)) return;
            if (recv_all(sock, &type, sizeof(type)) != sizeof(type)) return;
            
            NSLog(@"[Metal Server] 📷 ReadPixels: (%u,%u) %ux%u format=0x%X type=0x%X", 
                  x, y, width, height, format, type);
            
            // Determine which texture to read from
            id<MTLTexture> sourceTexture = nil;
            if (_currentFramebuffer == 0) {
                // Reading from default framebuffer - use VM display texture if available
                if (_renderToVM && _vmDisplayTexture) {
                    sourceTexture = _vmDisplayTexture;
                } else {
                    // Fall back to current drawable (host window)
                    sourceTexture = self.currentDrawable.texture;
                }
            } else {
                // Reading from FBO
                NSDictionary *fbo = _framebufferRegistry[@(_currentFramebuffer)];
                if (fbo) {
                    NSNumber *colorAttachment = fbo[@"colorAttachment"];
                    if (colorAttachment) {
                        sourceTexture = _textureRegistry[colorAttachment];
                    }
                }
            }
            
            if (!sourceTexture) {
                NSLog(@"[Metal Server] ❌ No source texture for ReadPixels");
                // Send error response (0 bytes)
                uint32_t dataSize = 0;
                send(sock, &dataSize, sizeof(dataSize), 0);
                break;
            }
            
            // Calculate bytes per pixel based on format/type
            uint32_t bytesPerPixel = 4;  // Default RGBA/BGRA
            if (format == 0x1907) {  // GL_RGB
                bytesPerPixel = 3;
            } else if (format == 0x1908) {  // GL_RGBA
                bytesPerPixel = 4;
            } else if (format == 0x80E1) {  // GL_BGRA
                bytesPerPixel = 4;
            }
            
            uint32_t dataSize = width * height * bytesPerPixel;
            uint32_t rowBytes = width * bytesPerPixel;
            
            // Create temporary buffer for pixel data
            void *pixelData = malloc(dataSize);
            if (!pixelData) {
                NSLog(@"[Metal Server] ❌ Failed to allocate pixel buffer (%u bytes)", dataSize);
                uint32_t zero = 0;
                send(sock, &zero, sizeof(zero), 0);
                break;
            }
            
            // Read pixels from texture using blit encoder
            id<MTLCommandBuffer> commandBuffer = [_commandQueue commandBuffer];
            id<MTLBlitCommandEncoder> blitEncoder = [commandBuffer blitCommandEncoder];
            
            // Create staging buffer with shared storage mode
            id<MTLBuffer> stagingBuffer = [self.device newBufferWithLength:dataSize 
                                                                   options:MTLResourceStorageModeShared];
            
            // Copy texture to buffer
            [blitEncoder copyFromTexture:sourceTexture
                             sourceSlice:0
                             sourceLevel:0
                            sourceOrigin:MTLOriginMake(x, y, 0)
                              sourceSize:MTLSizeMake(width, height, 1)
                                toBuffer:stagingBuffer
                       destinationOffset:0
                  destinationBytesPerRow:rowBytes
                destinationBytesPerImage:dataSize];
            
            [blitEncoder endEncoding];
            [commandBuffer commit];
            [commandBuffer waitUntilCompleted];
            
            // Copy from staging buffer to pixel data
            memcpy(pixelData, [stagingBuffer contents], dataSize);
            
            // Send pixel data size first, then data
            send(sock, &dataSize, sizeof(dataSize), 0);
            send(sock, pixelData, dataSize, 0);
            
            NSLog(@"[Metal Server] ✅ Sent %u bytes of pixel data to VM", dataSize);
            
            free(pixelData);
            break;
        }
        
        case CMD_METAL_FRAMEBUFFER_TEXTURE2D: {
            uint32_t target, attachment, textarget, texture;
            int32_t level;
            
            if (recv_all(sock, &target, sizeof(target)) != sizeof(target)) return;
            if (recv_all(sock, &attachment, sizeof(attachment)) != sizeof(attachment)) return;
            if (recv_all(sock, &textarget, sizeof(textarget)) != sizeof(textarget)) return;
            if (recv_all(sock, &texture, sizeof(texture)) != sizeof(texture)) return;
            if (recv_all(sock, &level, sizeof(level)) != sizeof(level)) return;
            
            if (_currentFramebuffer == 0) {
                NSLog(@"[Metal Server] ⚠️  Cannot attach texture to default framebuffer (0)");
                break;
            }
            
            NSMutableDictionary *fboDesc = _framebufferRegistry[@(_currentFramebuffer)];
            if (!fboDesc) {
                NSLog(@"[Metal Server] ❌ Framebuffer %u not found", _currentFramebuffer);
                break;
            }
            
            id<MTLTexture> mtlTexture = _textureRegistry[@(texture)];
            if (!mtlTexture) {
                NSLog(@"[Metal Server] ❌ Texture %u not found", texture);
                break;
            }
            
            // Store attachment based on type
            NSString *attachmentKey = nil;
            if (attachment == 0x8CE0) { // GL_COLOR_ATTACHMENT0
                attachmentKey = @"colorAttachment";
            } else if (attachment == 0x8D00) { // GL_DEPTH_ATTACHMENT
                attachmentKey = @"depthAttachment";
            } else if (attachment == 0x8D20) { // GL_STENCIL_ATTACHMENT
                attachmentKey = @"stencilAttachment";
            }
            
            if (attachmentKey) {
                fboDesc[attachmentKey] = @(texture);
                fboDesc[@"width"] = @(mtlTexture.width);
                fboDesc[@"height"] = @(mtlTexture.height);
            }
            
            NSLog(@"[Metal Server] ✅ Attached texture %u to framebuffer %u (attachment=0x%X)", texture, _currentFramebuffer, attachment);
            break;
        }
        
        case CMD_METAL_CHECK_FRAMEBUFFER_STATUS: {
            uint32_t target;
            if (recv_all(sock, &target, sizeof(target)) != sizeof(target)) return;
            
            uint32_t status = 0x8CD5; // GL_FRAMEBUFFER_COMPLETE
            
            if (_currentFramebuffer != 0) {
                NSMutableDictionary *fboDesc = _framebufferRegistry[@(_currentFramebuffer)];
                if (!fboDesc) {
                    status = 0x8CDB; // GL_FRAMEBUFFER_UNDEFINED
                } else {
                    // Check if FBO has at least one attachment
                    BOOL hasAttachment = NO;
                    if (![fboDesc[@"colorAttachment"] isEqual:[NSNull null]]) hasAttachment = YES;
                    if (![fboDesc[@"depthAttachment"] isEqual:[NSNull null]]) hasAttachment = YES;
                    
                    if (!hasAttachment) {
                        status = 0x8CD7; // GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT
                    }
                }
            }
            
            send(sock, &status, sizeof(status), 0);
            NSLog(@"[Metal Server] CheckFramebufferStatus: framebuffer=%u status=0x%X", _currentFramebuffer, status);
            break;
        }
        
        case CMD_METAL_GEN_RENDERBUFFERS: {
            uint32_t count;
            if (recv_all(sock, &count, sizeof(count)) != sizeof(count)) return;
            
            for (uint32_t i = 0; i < count; i++) {
                uint32_t rboID = _nextRenderbufferID++;
                send(sock, &rboID, sizeof(rboID), 0);
            }
            
            NSLog(@"[Metal Server] ✅ Generated %u renderbuffer(s), IDs: %u-%u", count, _nextRenderbufferID - count, _nextRenderbufferID - 1);
            break;
        }
        
        case CMD_METAL_BIND_RENDERBUFFER: {
            uint32_t target, renderbuffer;
            if (recv_all(sock, &target, sizeof(target)) != sizeof(target)) return;
            if (recv_all(sock, &renderbuffer, sizeof(renderbuffer)) != sizeof(renderbuffer)) return;
            
            _currentRenderbuffer = renderbuffer;
            NSLog(@"[Metal Server] Bind renderbuffer: target=0x%X renderbuffer=%u", target, renderbuffer);
            break;
        }
        
        case CMD_METAL_DELETE_RENDERBUFFERS: {
            uint32_t count;
            if (recv_all(sock, &count, sizeof(count)) != sizeof(count)) return;
            
            for (uint32_t i = 0; i < count; i++) {
                uint32_t rboID;
                if (recv_all(sock, &rboID, sizeof(rboID)) != sizeof(rboID)) return;
                
                [_renderbufferRegistry removeObjectForKey:@(rboID)];
                
                if (_currentRenderbuffer == rboID) {
                    _currentRenderbuffer = 0;
                }
            }
            
            NSLog(@"[Metal Server] ✅ Deleted %u renderbuffer(s)", count);
            break;
        }
        
        case CMD_METAL_RENDERBUFFER_STORAGE: {
            uint32_t target, internalformat;
            uint32_t width, height;
            
            if (recv_all(sock, &target, sizeof(target)) != sizeof(target)) return;
            if (recv_all(sock, &internalformat, sizeof(internalformat)) != sizeof(internalformat)) return;
            if (recv_all(sock, &width, sizeof(width)) != sizeof(width)) return;
            if (recv_all(sock, &height, sizeof(height)) != sizeof(height)) return;
            
            if (_currentRenderbuffer == 0) {
                NSLog(@"[Metal Server] ⚠️  No renderbuffer bound");
                break;
            }
            
            // Determine Metal pixel format from GL format
            MTLPixelFormat pixelFormat = MTLPixelFormatBGRA8Unorm;
            if (internalformat == 0x81A5) { // GL_DEPTH_COMPONENT16
                pixelFormat = MTLPixelFormatDepth32Float;
            } else if (internalformat == 0x88F0) { // GL_DEPTH24_STENCIL8
                pixelFormat = MTLPixelFormatDepth32Float_Stencil8;
            } else if (internalformat == 0x8058) { // GL_RGBA8
                pixelFormat = MTLPixelFormatRGBA8Unorm;
            }
            
            // Create Metal texture for renderbuffer
            MTLTextureDescriptor *desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:pixelFormat
                                                                                            width:width
                                                                                           height:height
                                                                                        mipmapped:NO];
            desc.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
            desc.storageMode = MTLStorageModePrivate;
            
            id<MTLTexture> rboTexture = [self.device newTextureWithDescriptor:desc];
            [_renderbufferRegistry setObject:rboTexture forKey:@(_currentRenderbuffer)];
            
            NSLog(@"[Metal Server] ✅ Renderbuffer %u storage: %ux%u format=0x%X", _currentRenderbuffer, width, height, internalformat);
            break;
        }
        
        case CMD_METAL_FRAMEBUFFER_RENDERBUFFER: {
            uint32_t target, attachment, renderbuffertarget, renderbuffer;
            
            if (recv_all(sock, &target, sizeof(target)) != sizeof(target)) return;
            if (recv_all(sock, &attachment, sizeof(attachment)) != sizeof(attachment)) return;
            if (recv_all(sock, &renderbuffertarget, sizeof(renderbuffertarget)) != sizeof(renderbuffertarget)) return;
            if (recv_all(sock, &renderbuffer, sizeof(renderbuffer)) != sizeof(renderbuffer)) return;
            
            if (_currentFramebuffer == 0) {
                NSLog(@"[Metal Server] ⚠️  Cannot attach renderbuffer to default framebuffer (0)");
                break;
            }
            
            NSMutableDictionary *fboDesc = _framebufferRegistry[@(_currentFramebuffer)];
            if (!fboDesc) {
                NSLog(@"[Metal Server] ❌ Framebuffer %u not found", _currentFramebuffer);
                break;
            }
            
            id<MTLTexture> rboTexture = _renderbufferRegistry[@(renderbuffer)];
            if (!rboTexture) {
                NSLog(@"[Metal Server] ❌ Renderbuffer %u not found", renderbuffer);
                break;
            }
            
            // Store attachment (negative ID indicates it's a renderbuffer, not texture)
            NSString *attachmentKey = nil;
            if (attachment == 0x8CE0) { // GL_COLOR_ATTACHMENT0
                attachmentKey = @"colorAttachment";
            } else if (attachment == 0x8D00) { // GL_DEPTH_ATTACHMENT
                attachmentKey = @"depthAttachment";
            } else if (attachment == 0x8D20) { // GL_STENCIL_ATTACHMENT
                attachmentKey = @"stencilAttachment";
            }
            
            if (attachmentKey) {
                fboDesc[attachmentKey] = @(-((int32_t)renderbuffer)); // Negative to indicate RBO
                fboDesc[@"width"] = @(rboTexture.width);
                fboDesc[@"height"] = @(rboTexture.height);
            }
            
            NSLog(@"[Metal Server] ✅ Attached renderbuffer %u to framebuffer %u (attachment=0x%X)", renderbuffer, _currentFramebuffer, attachment);
            break;
        }
            
        default:
            NSLog(@"[Metal Server] ⚠️  Unknown command: %u (0x%08X)", cmd, cmd);
            break;
    }
    } // @autoreleasepool
}

- (void)renderFrame:(uint32_t)primitiveType vertexStart:(uint32_t)start vertexCount:(uint32_t)count vertexData:(const float*)vertexData {
    id<MTLCommandBuffer> commandBuffer = [_commandQueue commandBuffer];
    
    MTLRenderPassDescriptor *renderPassDescriptor = self.currentRenderPassDescriptor;
    if (!renderPassDescriptor) return;
    
    renderPassDescriptor.colorAttachments[0].clearColor = _metalClearColor;
    renderPassDescriptor.colorAttachments[0].loadAction = MTLLoadActionClear;
    
    id<MTLRenderCommandEncoder> renderEncoder = [commandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor];
    [renderEncoder setRenderPipelineState:_pipelineState];
    
    // Create vertex buffer from received data
    size_t bufferSize = count * 7 * sizeof(float);
    id<MTLBuffer> vertexBuffer = [self.device newBufferWithBytes:vertexData
                                                          length:bufferSize
                                                         options:MTLResourceStorageModeShared];
    
    // Bind vertex buffer
    [renderEncoder setVertexBuffer:vertexBuffer offset:0 atIndex:0];
    
    // Draw using converted primitive type
    MTLPrimitiveType mtlType = (MTLPrimitiveType)primitiveType;
    [renderEncoder drawPrimitives:mtlType vertexStart:start vertexCount:count];
    
    [renderEncoder endEncoding];
    [commandBuffer presentDrawable:self.currentDrawable];
    [commandBuffer commit];
    
    NSLog(@"[Metal Server] ✅ Frame rendered on M4 Pro (%u vertices)", count);
}

// Phase 1: Render from VBO (glDrawArrays)
- (void)renderFrameVBO:(uint32_t)mode first:(uint32_t)first count:(uint32_t)count {
    // For legacy OpenGL compatibility: if no VAO is bound, use currently bound array buffer directly
    id<MTLBuffer> sourceBuffer = nil;
    uint32_t bufferID = 0;
    
    if (_currentVAO == 0) {
        // Legacy mode: No VAO bound, use current array buffer directly
        NSLog(@"[Metal Server] Legacy mode: Using current array buffer %u without VAO", _currentArrayBuffer);
        if (_currentArrayBuffer == 0) {
            NSLog(@"[Metal Server] ERROR: No array buffer bound");
            return;
        }
        bufferID = _currentArrayBuffer;
        sourceBuffer = _bufferRegistry[@(bufferID)];
    } else {
        // Modern mode: Use VAO configuration
        NSDictionary *vaoConfig = _vaoRegistry[@(_currentVAO)];
        if (!vaoConfig) {
            NSLog(@"[Metal Server] ERROR: VAO %u not found", _currentVAO);
            return;
        }
        
        NSArray *attributes = vaoConfig[@"attributes"];
        
        if (attributes.count == 0) {
            NSLog(@"[Metal Server] ERROR: VAO has no vertex attributes configured");
            return;
        }
        
        // Get first enabled attribute's buffer to determine vertex count
        NSDictionary *firstAttr = nil;
        for (NSDictionary *attr in attributes) {
            if ([attr[@"enabled"] boolValue]) {
                firstAttr = attr;
                break;
            }
        }
        
        if (!firstAttr) {
            NSLog(@"[Metal Server] ERROR: No enabled vertex attributes");
            return;
        }
        
        bufferID = [firstAttr[@"buffer"] unsignedIntValue];
        sourceBuffer = _bufferRegistry[@(bufferID)];
    }
    
    if (!sourceBuffer) {
        NSLog(@"[Metal Server] ERROR: Buffer %u not found in registry", bufferID);
        return;
    }
    
    id<MTLCommandBuffer> commandBuffer = [_commandQueue commandBuffer];
    
    MTLRenderPassDescriptor *renderPassDescriptor = nil;
    id<CAMetalDrawable> drawable = nil;
    
    // Phase 5: Check if FBO is bound
    if (_currentFramebuffer != 0) {
        // Render to FBO
        NSMutableDictionary *fboDesc = _framebufferRegistry[@(_currentFramebuffer)];
        if (!fboDesc) {
            NSLog(@"[Metal Server] ERROR: Framebuffer %u not found", _currentFramebuffer);
            return;
        }
        
        renderPassDescriptor = [MTLRenderPassDescriptor renderPassDescriptor];
        
        // Get color attachment
        NSNumber *colorAttachmentID = fboDesc[@"colorAttachment"];
        if (colorAttachmentID && ![colorAttachmentID isEqual:[NSNull null]]) {
            id<MTLTexture> colorTexture = nil;
            
            int32_t attachmentValue = [colorAttachmentID intValue];
            if (attachmentValue < 0) {
                // Renderbuffer attachment
                colorTexture = _renderbufferRegistry[@(-attachmentValue)];
            } else {
                // Texture attachment
                colorTexture = _textureRegistry[@(attachmentValue)];
            }
            
            if (colorTexture) {
                renderPassDescriptor.colorAttachments[0].texture = colorTexture;
                renderPassDescriptor.colorAttachments[0].loadAction = MTLLoadActionClear;
                renderPassDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;
                // Use bright magenta clear color for FBO to make it unmistakable
                renderPassDescriptor.colorAttachments[0].clearColor = MTLClearColorMake(1.0, 0.0, 1.0, 1.0);
            }
        }
        
        // Get depth attachment
        NSNumber *depthAttachmentID = fboDesc[@"depthAttachment"];
        if (depthAttachmentID && ![depthAttachmentID isEqual:[NSNull null]]) {
            id<MTLTexture> depthTexture = nil;
            
            int32_t attachmentValue = [depthAttachmentID intValue];
            if (attachmentValue < 0) {
                depthTexture = _renderbufferRegistry[@(-attachmentValue)];
            } else {
                depthTexture = _textureRegistry[@(attachmentValue)];
            }
            
            if (depthTexture) {
                renderPassDescriptor.depthAttachment.texture = depthTexture;
                renderPassDescriptor.depthAttachment.loadAction = MTLLoadActionClear;
                renderPassDescriptor.depthAttachment.storeAction = MTLStoreActionStore;
                renderPassDescriptor.depthAttachment.clearDepth = 1.0;
            }
        }
        
        NSLog(@"[Metal Server] Rendering to FBO %u", _currentFramebuffer);
    } else {
        // Render to default framebuffer - use VM display texture for accumulation
        // This allows us to blit the final result to the window in SwapBuffers
        
        if (!_vmDisplayTexture) {
            [self createVMDisplayTexturesWithWidth:_vmDisplayWidth height:_vmDisplayHeight];
        }
        
        renderPassDescriptor = [MTLRenderPassDescriptor renderPassDescriptor];
        renderPassDescriptor.colorAttachments[0].texture = _vmDisplayTexture;
        renderPassDescriptor.colorAttachments[0].clearColor = _metalClearColor;
        
        // Clear ONLY if glClear() was called and this is the first draw
        if (_needsClear) {
            renderPassDescriptor.colorAttachments[0].loadAction = MTLLoadActionClear;
            _needsClear = NO;  // Clear consumed - only clear once per glClear() call
            NSLog(@"[Metal Server] 🧹 CLEARING COLOR on this draw (%.2f,%.2f,%.2f,%.2f)", 
                  _metalClearColor.red, _metalClearColor.green, _metalClearColor.blue, _metalClearColor.alpha);
        } else {
            renderPassDescriptor.colorAttachments[0].loadAction = MTLLoadActionLoad;
            NSLog(@"[Metal Server] ♻️ LOADING existing color content");
        }
        
        renderPassDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;
        
        // Add depth attachment for proper 3D rendering
        if (_vmDisplayDepthTexture) {
            renderPassDescriptor.depthAttachment.texture = _vmDisplayDepthTexture;
            // Clear depth buffer when color is cleared (glClear)
            if (!_frameClearApplied) {
                renderPassDescriptor.depthAttachment.loadAction = MTLLoadActionClear;
                _frameClearApplied = YES;  // Only clear depth once per glClear() call
                NSLog(@"[Metal Server] 🧹 CLEARING DEPTH BUFFER");
            } else {
                renderPassDescriptor.depthAttachment.loadAction = MTLLoadActionLoad;
            }
            renderPassDescriptor.depthAttachment.storeAction = MTLStoreActionStore;
            renderPassDescriptor.depthAttachment.clearDepth = 1.0;
        }
        
        NSLog(@"[Metal Server] Rendering to VM display texture (%lux%lu)", (unsigned long)_vmDisplayWidth, (unsigned long)_vmDisplayHeight);
        
        drawable = nil;  // No drawable needed - rendering to texture
    }
    
    id<MTLRenderCommandEncoder> renderEncoder = [commandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor];
    
    // Phase 2: Use custom shader pipeline if program is bound
    id<MTLRenderPipelineState> activePipeline = nil;
    if (_currentProgram != 0) {
        activePipeline = _shaderPipelines[@(_currentProgram)];
        if (activePipeline) {
            NSLog(@"[Metal Server] Using custom shader pipeline for program %u", _currentProgram);
        } else {
            NSLog(@"[Metal Server] ⚠️  No custom pipeline found for program %u, using fixed-function fallback", _currentProgram);
        }
    }
    
    // Fall back to fixed-function pipeline if no custom shader
    if (!activePipeline) {
        if (!_fixedFunctionPipeline) {
            _fixedFunctionPipeline = [self createFixedFunctionPipeline];
        }
        activePipeline = _fixedFunctionPipeline;
        NSLog(@"[Metal Server] Using fixed-function pipeline (fallback)");
    }
    
    [renderEncoder setRenderPipelineState:activePipeline];
    
    // Set viewport - use VM display size when rendering to texture, window size when rendering to screen
    MTLViewport viewport;
    if (_currentFramebuffer == 0 && !drawable) {
        // Rendering to VM display texture
        viewport = (MTLViewport){0, 0, (double)_vmDisplayWidth, (double)_vmDisplayHeight, 0.0, 1.0};
        NSLog(@"[Metal Server] 🔍 Viewport set to VM display: %lux%lu", (unsigned long)_vmDisplayWidth, (unsigned long)_vmDisplayHeight);
    } else {
        // Rendering to window or FBO
        viewport = (MTLViewport){0, 0, self.drawableSize.width, self.drawableSize.height, 0.0, 1.0};
        NSLog(@"[Metal Server] 🔍 Viewport set to drawable: %.0fx%.0f", self.drawableSize.width, self.drawableSize.height);
    }
    [renderEncoder setViewport:viewport];
    
    // Phase 4: Apply depth stencil state
    if (_depthTestEnabled && _depthStencilState) {
        [renderEncoder setDepthStencilState:_depthStencilState];
    }
    
    // Phase 4: Apply culling
    if (_cullFaceEnabled) {
        MTLCullMode cullMode = MTLCullModeNone;
        if (_cullFaceMode == 0x0405) {  // GL_BACK
            cullMode = MTLCullModeBack;
        } else if (_cullFaceMode == 0x0404) {  // GL_FRONT
            cullMode = MTLCullModeFront;
        }
        [renderEncoder setCullMode:cullMode];
        
        MTLWinding winding = (_frontFace == 0x0901) ? MTLWindingCounterClockwise : MTLWindingClockwise;
        [renderEncoder setFrontFacingWinding:winding];
    } else {
        [renderEncoder setCullMode:MTLCullModeNone];
    }
    
    // Bind vertex buffers for each attribute
    if (_currentVAO == 0) {
        // Legacy mode: bind single buffer at index 0
        [renderEncoder setVertexBuffer:sourceBuffer offset:first atIndex:0];
        NSLog(@"[Metal Server] Legacy: Bound buffer %u at index 0", bufferID);
    } else {
        // Modern mode: bind each attribute's buffer at its attribute index
        NSDictionary *vaoConfig = _vaoRegistry[@(_currentVAO)];
        NSArray *attributes = vaoConfig[@"attributes"];
        
        for (NSDictionary *attr in attributes) {
            if (![attr[@"enabled"] boolValue]) continue;
            
            uint32_t attrIndex = [attr[@"index"] unsignedIntValue];
            uint32_t attrBufferID = [attr[@"buffer"] unsignedIntValue];
            uint64_t attrOffset = [attr[@"offset"] unsignedLongLongValue];
            
            id<MTLBuffer> attrBuffer = _bufferRegistry[@(attrBufferID)];
            if (attrBuffer) {
                [renderEncoder setVertexBuffer:attrBuffer offset:attrOffset atIndex:attrIndex];
                NSLog(@"[Metal Server] Bound attribute %u: buffer=%u offset=%llu", attrIndex, attrBufferID, attrOffset);
            }
        }
    }
    
    // Phase 2: Apply stored uniform values for current program
    if (_currentProgram != 0) {
        NSDictionary *programUniforms = _programUniformData[@(_currentProgram)];
        if (programUniforms && programUniforms.count > 0) {
            // Apply each stored uniform
            for (NSNumber *locationKey in programUniforms) {
                NSDictionary *uniformData = programUniforms[locationKey];
                NSString *type = uniformData[@"type"];
                NSData *data = uniformData[@"data"];
                
                if ([type isEqualToString:@"mat4"] && data) {
                    // Set matrix uniform in vertex shader at the appropriate buffer index
                    // Location 0 = buffer index 1, location 1 = buffer index 2, etc.
                    NSUInteger bufferIndex = [locationKey unsignedIntegerValue] + 1;
                    [renderEncoder setVertexBytes:data.bytes length:data.length atIndex:bufferIndex];
                    NSLog(@"[Metal Server] Applied mat4 uniform at location %@ (buffer index %lu)", locationKey, (unsigned long)bufferIndex);
                }
            }
        }
        
        // Also bind default uniform buffer for fragment shader (white color multiplier)
        float defaultUniforms[4] = {1.0f, 1.0f, 1.0f, 1.0f};
        [renderEncoder setFragmentBytes:defaultUniforms length:sizeof(defaultUniforms) atIndex:0];
    }
    
    // Phase 3: Bind texture and sampler
    // CRITICAL: Detect texture feedback loop (rendering to same texture we're sampling from)
    uint32_t textureID = _currentTexture2D;
    
    // If rendering to FBO, check if bound texture is the FBO's color attachment
    if (_currentFramebuffer != 0 && _framebufferRegistry[@(_currentFramebuffer)]) {
        NSDictionary *fboDesc = _framebufferRegistry[@(_currentFramebuffer)];
        NSNumber *colorAttachmentID = fboDesc[@"colorAttachment"];
        
        if (colorAttachmentID && ![colorAttachmentID isEqual:[NSNull null]]) {
            int32_t attachmentValue = [colorAttachmentID intValue];
            
            // If sampling from the same texture we're rendering to, use default white texture
            if (attachmentValue > 0 && (uint32_t)attachmentValue == _currentTexture2D) {
                NSLog(@"[Metal Server] ⚠️  Detected texture feedback loop: FBO attachment=%u == bound texture=%u, using default texture", attachmentValue, _currentTexture2D);
                textureID = 0;  // Force default white texture
            }
        }
    }
    
    // Fallback to default texture if none bound
    if (textureID == 0) {
        textureID = 0;
    }
    
    id<MTLTexture> texture = _textureRegistry[@(textureID)];
    id<MTLSamplerState> sampler = _samplerRegistry[@(textureID)];
    
    NSLog(@"[Metal Server] DEBUG: Binding texture: _currentTexture2D=%u, textureID=%u, texture=%p, size=%lux%lu", 
          _currentTexture2D, textureID, texture, 
          texture ? (unsigned long)texture.width : 0, 
          texture ? (unsigned long)texture.height : 0);
    
    if (!sampler) {
        sampler = _samplerRegistry[@(0)];  // Use default sampler
    }
    
    [renderEncoder setFragmentTexture:texture atIndex:0];
    [renderEncoder setFragmentSamplerState:sampler atIndex:0];
    
    // Debug: Print first few vertices if buffer is small enough
    if (count <= 36 && sourceBuffer && sourceBuffer.length > 0) {
        float *verts = (float *)sourceBuffer.contents;
        NSLog(@"[Metal Server] 🔍 First vertex data (buffer size=%lu, count=%u):", (unsigned long)sourceBuffer.length, count);
        int vertexSize = 3;  // Assuming vec3 positions
        for (int i = 0; i < MIN(3, count); i++) {
            NSLog(@"[Metal Server]   Vertex %d: (%.3f, %.3f, %.3f)", 
                  i, verts[i*vertexSize], verts[i*vertexSize+1], verts[i*vertexSize+2]);
        }
    }
    
    // Draw
    MTLPrimitiveType mtlType = MTLPrimitiveTypeTriangle;
    if (mode == 4) mtlType = MTLPrimitiveTypeTriangleStrip;
    else if (mode == 2) mtlType = MTLPrimitiveTypeLine;
    
    NSLog(@"[Metal Server] 🎨 Drawing %u vertices with mode=%u (type=%lu)", count, mode, (unsigned long)mtlType);
    
    [renderEncoder drawPrimitives:mtlType vertexStart:0 vertexCount:count];
    
    [renderEncoder endEncoding];
    
    // Phase 5: Only present when rendering to screen (not FBO)
    if (_currentFramebuffer == 0 && drawable) {
        [commandBuffer presentDrawable:drawable];
    }
    
    [commandBuffer commit];
    
    // CRITICAL: When rendering to FBO, wait for completion before proceeding
    // This ensures the FBO texture is fully updated before subsequent renders sample from it
    if (_currentFramebuffer != 0) {
        [commandBuffer waitUntilCompleted];
        NSLog(@"[Metal Server] ✅ FBO render completed, texture ready for sampling");
    }

}

// Phase 1: Render from VBO with indices (glDrawElements)
- (void)renderFrameVBOIndexed:(uint32_t)mode count:(uint32_t)count indexType:(uint32_t)type indexOffset:(uint64_t)offset {
    if (_currentVAO == 0 || !_vaoRegistry[@(_currentVAO)]) {
        NSLog(@"[Metal Server] ERROR: No VAO bound for indexed rendering");
        return;
    }
    
    NSDictionary *vaoConfig = _vaoRegistry[@(_currentVAO)];
    uint32_t vertexBufferID = [vaoConfig[@"arrayBuffer"] unsignedIntValue];
    uint32_t indexBufferID = [vaoConfig[@"elementBuffer"] unsignedIntValue];
    
    id<MTLBuffer> vertexBuffer = _bufferRegistry[@(vertexBufferID)];
    id<MTLBuffer> indexBuffer = _bufferRegistry[@(indexBufferID)];
    
    if (!vertexBuffer || !indexBuffer) {
        NSLog(@"[Metal Server] ERROR: Missing buffers (vertex=%u, index=%u)", vertexBufferID, indexBufferID);
        return;
    }
    
    id<MTLCommandBuffer> commandBuffer = [_commandQueue commandBuffer];
    
    MTLRenderPassDescriptor *renderPassDescriptor = nil;
    id<CAMetalDrawable> drawable = nil;
    
    // Phase 5: Check if FBO is bound
    if (_currentFramebuffer != 0) {
        // Render to FBO (same logic as renderFrameVBO)
        NSMutableDictionary *fboDesc = _framebufferRegistry[@(_currentFramebuffer)];
        if (!fboDesc) {
            NSLog(@"[Metal Server] ERROR: Framebuffer %u not found", _currentFramebuffer);
            return;
        }
        
        renderPassDescriptor = [MTLRenderPassDescriptor renderPassDescriptor];
        
        NSNumber *colorAttachmentID = fboDesc[@"colorAttachment"];
        if (colorAttachmentID && ![colorAttachmentID isEqual:[NSNull null]]) {
            id<MTLTexture> colorTexture = nil;
            int32_t attachmentValue = [colorAttachmentID intValue];
            if (attachmentValue < 0) {
                colorTexture = _renderbufferRegistry[@(-attachmentValue)];
            } else {
                colorTexture = _textureRegistry[@(attachmentValue)];
            }
            if (colorTexture) {
                renderPassDescriptor.colorAttachments[0].texture = colorTexture;
                renderPassDescriptor.colorAttachments[0].loadAction = MTLLoadActionClear;
                renderPassDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;
                renderPassDescriptor.colorAttachments[0].clearColor = _metalClearColor;
            }
        }
        
        NSNumber *depthAttachmentID = fboDesc[@"depthAttachment"];
        if (depthAttachmentID && ![depthAttachmentID isEqual:[NSNull null]]) {
            id<MTLTexture> depthTexture = nil;
            int32_t attachmentValue = [depthAttachmentID intValue];
            if (attachmentValue < 0) {
                depthTexture = _renderbufferRegistry[@(-attachmentValue)];
            } else {
                depthTexture = _textureRegistry[@(attachmentValue)];
            }
            if (depthTexture) {
                renderPassDescriptor.depthAttachment.texture = depthTexture;
                renderPassDescriptor.depthAttachment.loadAction = MTLLoadActionClear;
                renderPassDescriptor.depthAttachment.storeAction = MTLStoreActionStore;
                renderPassDescriptor.depthAttachment.clearDepth = 1.0;
            }
        }
        
        NSLog(@"[Metal Server] Rendering to FBO %u (indexed)", _currentFramebuffer);
    } else {
        // Render to default framebuffer (screen)
        renderPassDescriptor = self.currentRenderPassDescriptor;
        if (!renderPassDescriptor) return;
        
        renderPassDescriptor.colorAttachments[0].clearColor = _metalClearColor;
        renderPassDescriptor.colorAttachments[0].loadAction = MTLLoadActionClear;
        
        drawable = self.currentDrawable;
    }
    
    id<MTLRenderCommandEncoder> renderEncoder = [commandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor];
    
    // Phase 2: Use custom shader pipeline if program is bound
    id<MTLRenderPipelineState> activePipeline = _pipelineState;
    if (_currentProgram != 0) {
        id<MTLRenderPipelineState> customPipeline = _shaderPipelines[@(_currentProgram)];
        if (customPipeline) {
            activePipeline = customPipeline;
            NSLog(@"[Metal Server] Using custom shader pipeline for program %u (indexed)", _currentProgram);
        }
    }
    [renderEncoder setRenderPipelineState:activePipeline];
    
    // Bind the vertex buffer
    [renderEncoder setVertexBuffer:vertexBuffer offset:0 atIndex:0];
    
    // Draw indexed
    MTLPrimitiveType mtlType = MTLPrimitiveTypeTriangle;
    if (mode == 4) mtlType = MTLPrimitiveTypeTriangleStrip;
    else if (mode == 2) mtlType = MTLPrimitiveTypeLine;
    
    MTLIndexType mtlIndexType = (type == 0x1403) ? MTLIndexTypeUInt16 : MTLIndexTypeUInt32; // GL_UNSIGNED_SHORT : GL_UNSIGNED_INT
    
    [renderEncoder drawIndexedPrimitives:mtlType
                              indexCount:count
                               indexType:mtlIndexType
                             indexBuffer:indexBuffer
                       indexBufferOffset:offset];
    
    [renderEncoder endEncoding];
    
    // Phase 5: Only present when rendering to screen (not FBO)
    if (_currentFramebuffer == 0 && drawable) {
        [commandBuffer presentDrawable:drawable];
    }
    
    [commandBuffer commit];
    
    NSLog(@"[Metal Server] ✅ Frame rendered from indexed VBO (%u indices, target=%s)", 
          count, (_currentFramebuffer == 0) ? "screen" : "FBO");
}

- (void)renderFrameClientData:(uint32_t)mode count:(uint32_t)count attributes:(NSArray *)attributes {
    NSLog(@"[Metal Server] Rendering with client-side vertex data: mode=%u count=%u attrs=%lu",
          mode, count, (unsigned long)attributes.count);
    
    id<MTLCommandBuffer> commandBuffer = [_commandQueue commandBuffer];
    
    // Setup render pass (same as VBO rendering)
    MTLRenderPassDescriptor *renderPassDescriptor = nil;
    
    if (!_vmDisplayTexture) {
        [self createVMDisplayTexturesWithWidth:_vmDisplayWidth height:_vmDisplayHeight];
    }
    
    renderPassDescriptor = [MTLRenderPassDescriptor renderPassDescriptor];
    renderPassDescriptor.colorAttachments[0].texture = _vmDisplayTexture;
    renderPassDescriptor.colorAttachments[0].clearColor = _metalClearColor;
    
    if (_needsClear && !_frameClearApplied) {
        renderPassDescriptor.colorAttachments[0].loadAction = MTLLoadActionClear;
        _frameClearApplied = YES;
    } else {
        renderPassDescriptor.colorAttachments[0].loadAction = MTLLoadActionLoad;
    }
    renderPassDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;
    
    // Add depth attachment for proper 3D rendering
    if (_vmDisplayDepthTexture) {
        renderPassDescriptor.depthAttachment.texture = _vmDisplayDepthTexture;
        if (_needsClear && !_frameClearApplied) {
            renderPassDescriptor.depthAttachment.loadAction = MTLLoadActionClear;
        } else {
            renderPassDescriptor.depthAttachment.loadAction = MTLLoadActionLoad;
        }
        renderPassDescriptor.depthAttachment.storeAction = MTLStoreActionStore;
        renderPassDescriptor.depthAttachment.clearDepth = 1.0;
    }
    
    id<MTLRenderCommandEncoder> renderEncoder = [commandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor];
    
    // Use shader pipeline (same logic as main render path)
    id<MTLRenderPipelineState> activePipeline = nil;
    if (_currentProgram != 0) {
        activePipeline = _shaderPipelines[@(_currentProgram)];
        if (activePipeline) {
            NSLog(@"[Metal Server] Using custom shader pipeline for program %u (client data)", _currentProgram);
        } else {
            NSLog(@"[Metal Server] ⚠️  No custom pipeline found for program %u, using fixed-function fallback (client data)", _currentProgram);
        }
    }
    
    // Fall back to fixed-function pipeline if no custom shader
    if (!activePipeline) {
        if (!_fixedFunctionPipeline) {
            _fixedFunctionPipeline = [self createFixedFunctionPipeline];
        }
        activePipeline = _fixedFunctionPipeline;
        NSLog(@"[Metal Server] Using fixed-function pipeline (fallback, client data)");
    }
    
    [renderEncoder setRenderPipelineState:activePipeline];
    
    // Set viewport
    MTLViewport viewport = {0, 0, _vmDisplayWidth, _vmDisplayHeight, 0.0, 1.0};
    [renderEncoder setViewport:viewport];
    
    // Apply stored uniforms
    if (_currentProgram != 0) {
        NSDictionary *programUniforms = _programUniformData[@(_currentProgram)];
        if (programUniforms) {
            for (NSNumber *locationKey in programUniforms) {
                NSDictionary *uniformData = programUniforms[locationKey];
                NSString *type = uniformData[@"type"];
                NSData *data = uniformData[@"data"];
                
                if ([type isEqualToString:@"mat4"] && data) {
                    NSUInteger bufferIndex = [locationKey unsignedIntegerValue] + 1;
                    [renderEncoder setVertexBytes:data.bytes length:data.length atIndex:bufferIndex];
                }
            }
        }
    }
    
    // Bind vertex buffers from client data
    for (NSDictionary *attr in attributes) {
        uint32_t index = [attr[@"index"] unsignedIntValue];
        id<MTLBuffer> buffer = attr[@"buffer"];
        [renderEncoder setVertexBuffer:buffer offset:0 atIndex:index];
    }
    
    // Draw
    MTLPrimitiveType primitiveType = MTLPrimitiveTypeTriangle;
    if (mode == 0x0004) primitiveType = MTLPrimitiveTypeTriangle;
    else if (mode == 0x0005) primitiveType = MTLPrimitiveTypeTriangleStrip;
    else if (mode == 0x0001) primitiveType = MTLPrimitiveTypeLine;
    
    [renderEncoder drawPrimitives:primitiveType vertexStart:0 vertexCount:count];
    [renderEncoder endEncoding];
    
    [commandBuffer commit];
    
    NSLog(@"[Metal Server] ✅ Client data rendering complete");
}

@end

// Server thread
static void* server_thread(void* arg) {
    MetalServerView *metalView = (__bridge MetalServerView*)arg;
    
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        NSLog(@"[Metal Server] ERROR: Failed to create socket");
        return NULL;
    }
    
    int opt = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in serverAddr = {0};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(SERVER_PORT);
    
    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        NSLog(@"[Metal Server] ERROR: Failed to bind to port %d", SERVER_PORT);
        close(serverSocket);
        return NULL;
    }
    
    if (listen(serverSocket, 5) < 0) {
        NSLog(@"[Metal Server] ERROR: Failed to listen");
        close(serverSocket);
        return NULL;
    }
    
    NSLog(@"[Metal Server] ✅ Server listening on 0.0.0.0:%d", SERVER_PORT);
    NSLog(@"[Metal Server] Waiting for VM clients...");
    
    while (1) {
        struct sockaddr_in clientAddr;
        socklen_t clientLen = sizeof(clientAddr);
        
        int clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientLen);
        if (clientSocket < 0) continue;
        
        char clientIP[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clientAddr.sin_addr, clientIP, INET_ADDRSTRLEN);
        NSLog(@"[Metal Server] ✅ Client connected from %s:%d", clientIP, ntohs(clientAddr.sin_port));
        
        metalView.clientSocket = clientSocket;
        
        // Handle client commands synchronously on socket thread
        // This ensures we read ALL data for each command before processing next
        uint32_t cmd;
        while (recv(clientSocket, &cmd, sizeof(cmd), 0) == sizeof(cmd)) {
            @autoreleasepool {
                [metalView handleCommand:cmd socket:clientSocket];
            }
        }
        
        NSLog(@"[Metal Server] Client disconnected");
        close(clientSocket);
        metalView.clientSocket = -1;
    }
    
    close(serverSocket);
    return NULL;
}

@interface AppDelegate : NSObject <NSApplicationDelegate>
@property (strong) NSWindow *window;
@property (strong) MetalServerView *metalView;
@end

@implementation AppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)notification {
    NSLog(@"========================================");
    NSLog(@"   SharedGL Metal Server");
    NSLog(@"   OpenGL→Metal Translation POC");
    NSLog(@"========================================");
    
    // Get Metal device
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    if (!device) {
        NSLog(@"[Metal Server] ERROR: Metal is not supported on this device");
        [NSApp terminate:nil];
        return;
    }
    
    NSLog(@"[Metal Server] Metal Device: %@", device.name);
    NSLog(@"[Metal Server] Low Power: %@", device.isLowPower ? @"YES" : @"NO");
    NSLog(@"[Metal Server] Headless: %@", device.isHeadless ? @"YES" : @"NO");
    
    // Create window
    NSRect frame = NSMakeRect(100, 100, 800, 600);
    NSUInteger styleMask = NSWindowStyleMaskTitled | 
                          NSWindowStyleMaskClosable | 
                          NSWindowStyleMaskMiniaturizable;
    
    _window = [[NSWindow alloc] initWithContentRect:frame
                                          styleMask:styleMask
                                            backing:NSBackingStoreBuffered
                                              defer:NO];
    [_window setTitle:@"SharedGL Metal Server - M4 Pro GPU"];
    
    // Create Metal view
    _metalView = [[MetalServerView alloc] initWithFrame:frame device:device];
    
    [_window setContentView:_metalView];
    [_window makeKeyAndOrderFront:nil];
    [_window center];
    
    // Start server thread
    pthread_t thread;
    pthread_create(&thread, NULL, server_thread, (__bridge void*)_metalView);
    pthread_detach(thread);
    
    NSLog(@"[Metal Server] 🚀 Ready to accept OpenGL→Metal translations");
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender {
    return YES;
}

@end

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        NSApplication *app = [NSApplication sharedApplication];
        AppDelegate *delegate = [[AppDelegate alloc] init];
        [app setDelegate:delegate];
        [app run];
    }
    return 0;
}

#ifndef __VMQemuVGAAccelerator_H__
#define __VMQemuVGAAccelerator_H__

#include <IOKit/IOService.h>
#include <IOKit/IOUserClient.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IOCommandGate.h>
#include <IOKit/IOWorkLoop.h>

class VMQemuVGA;
class VMVirtIOGPU;
class VMVirtIOFramebuffer;
class VMShaderManager;
class VMTextureManager;
class VMCommandBufferPool;
class VMPhase3Manager;
class VMMetalBridge;
class IOPixelInformation;

// 3D command types for user space communication
enum VM3DCommandType {
    VM3D_CREATE_CONTEXT = 1,
    VM3D_DESTROY_CONTEXT,
    VM3D_CREATE_SURFACE,
    VM3D_DESTROY_SURFACE,
    VM3D_BIND_SURFACE,
    VM3D_CLEAR_SURFACE,
    VM3D_DRAW_PRIMITIVES,
    VM3D_PRESENT_SURFACE,
    VM3D_SET_RENDER_STATE,
    VM3D_SET_TEXTURE,
    VM3D_SET_SHADER,
    VM3D_SET_VERTEX_BUFFER,
    VM3D_SET_INDEX_BUFFER
};

// Surface formats
enum VM3DSurfaceFormat {
    VM3D_FORMAT_A8R8G8B8 = 21,
    VM3D_FORMAT_X8R8G8B8 = 22,
    VM3D_FORMAT_R5G6B5   = 23,
    VM3D_FORMAT_D24S8    = 75,
    VM3D_FORMAT_DXT1     = 827611204,
    VM3D_FORMAT_DXT3     = 827611460,
    VM3D_FORMAT_DXT5     = 827611716
};

// 3D command structure for user space
struct VM3DCommand {
    VM3DCommandType command;
    uint32_t context_id;
    uint32_t surface_id;
    uint32_t size;
    uint64_t data_offset;
    uint32_t flags;
    uint32_t reserved[2];
};

// Surface description
struct VM3DSurfaceInfo {
    uint32_t surface_id;
    uint32_t width;
    uint32_t height;
    uint32_t depth;
    VM3DSurfaceFormat format;
    uint32_t face_type;
    uint32_t mip_levels;
    uint32_t multisample_count;
    uint32_t autogen_filter;
    uint32_t flags;
};

class VMQemuVGAAccelerator : public IOService
{
    OSDeclareDefaultStructors(VMQemuVGAAccelerator);

private:
    VMQemuVGA* m_framebuffer;
    VMVirtIOGPU* m_gpu_device;
    IOWorkLoop* m_workloop;
    IOCommandGate* m_command_gate;
    IOLock* m_lock;
    
    // Advanced 3D managers
    VMShaderManager* m_shader_manager;
    VMTextureManager* m_texture_manager;
    VMCommandBufferPool* m_command_pool;
    VMMetalBridge* m_metal_bridge;
    
    // Phase 3 Integration Manager
    VMPhase3Manager* m_phase3_manager;
    
    // Context management
    struct AccelContext {
        uint32_t context_id;
        uint32_t gpu_context_id;
        bool active;
        OSSet* surfaces;
        IOMemoryDescriptor* command_buffer;
        task_t owning_task;
    };
    
    // Surface management
    struct AccelSurface {
        uint32_t surface_id;
        uint32_t gpu_resource_id;
        VM3DSurfaceInfo info;
        IOMemoryDescriptor* backing_memory;
        bool is_render_target;
    };
    
    OSArray* m_contexts;
    OSArray* m_surfaces;
    uint32_t m_next_context_id;
    uint32_t m_next_surface_id;
    
    // Statistics
    uint32_t m_draw_calls;
    uint32_t m_triangles_rendered;
    uint64_t m_memory_used;
    uint32_t m_commands_submitted;
    uint64_t m_memory_allocated;
    bool m_metal_compatible;
    
    // Internal methods
    AccelContext* findContext(uint32_t context_id);
    AccelSurface* findSurface(uint32_t surface_id);
    IOReturn createContextInternal(uint32_t* context_id, task_t task);
    IOReturn destroyContextInternal(uint32_t context_id);
    IOReturn createSurfaceInternal(uint32_t context_id, VM3DSurfaceInfo* info);
    IOReturn destroySurfaceInternal(uint32_t context_id, uint32_t surface_id);
    
    // Command processing
    IOReturn processCommand(VM3DCommand* cmd, IOMemoryDescriptor* data);
    IOReturn executeDrawCall(uint32_t context_id, IOMemoryDescriptor* commands);
    IOReturn setRenderTarget(uint32_t context_id, uint32_t surface_id);
    IOReturn clearSurface(uint32_t context_id, uint32_t surface_id, uint32_t color);
    
    // Memory management
    IOReturn allocateSurfaceMemory(VM3DSurfaceInfo* info, IOMemoryDescriptor** memory);
    uint32_t calculateSurfaceSize(VM3DSurfaceInfo* info);
    
    // State management
    IOReturn validateContext(uint32_t context_id, task_t task);
    IOReturn validateSurface(uint32_t surface_id, uint32_t context_id);

public:
    virtual bool init(OSDictionary* properties = nullptr) override;
    virtual void free() override;
    virtual bool start(IOService* provider) override;
    virtual void stop(IOService* provider) override;
    
    // IOUserClient factory
    virtual IOReturn newUserClient(task_t owningTask, void* securityID,
                                 UInt32 type, IOUserClient** handler) override;
    
    // 3D acceleration interface
    IOReturn create3DContext(uint32_t* context_id, task_t task);
    IOReturn destroy3DContext(uint32_t context_id);
    IOReturn create3DSurface(uint32_t context_id, VM3DSurfaceInfo* surface_info);
    IOReturn destroy3DSurface(uint32_t context_id, uint32_t surface_id);
    IOReturn submit3DCommands(uint32_t context_id, IOMemoryDescriptor* commands);
    IOReturn present3DSurface(uint32_t context_id, uint32_t surface_id);
    
    // Performance monitoring
    uint32_t getDrawCallCount() const { return m_draw_calls; }
    uint32_t getTriangleCount() const { return m_triangles_rendered; }
    uint64_t getMemoryUsage() const { return m_memory_used; }
    void resetStatistics();
    
    // Device capabilities
    bool supportsShaders() const;
    bool supportsHardwareTransform() const;
    bool supportsMultisample() const;
    uint32_t getMaxTextureSize() const;
    uint32_t getMaxRenderTargets() const;
    
    // Integration with framebuffer
    VMQemuVGA* getFramebuffer() const { return m_framebuffer; }
    VMVirtIOGPU* getGPUDevice() const { return m_gpu_device; }
    
    // Advanced 3D subsystems
    VMShaderManager* getShaderManager() const { return m_shader_manager; }
    VMTextureManager* getTextureManager() const { return m_texture_manager; }
    VMCommandBufferPool* getCommandPool() const { return m_command_pool; }
    
    // Shader operations
    IOReturn compileShader(uint32_t context_id, uint32_t shader_type, uint32_t language,
                          const void* source_code, size_t source_size, uint32_t* shader_id);
    IOReturn destroyShader(uint32_t context_id, uint32_t shader_id);
    IOReturn createShaderProgram(uint32_t context_id, uint32_t* shader_ids, uint32_t count,
                               uint32_t* program_id);
    IOReturn useShaderProgram(uint32_t context_id, uint32_t program_id);
    
    // Texture operations
    IOReturn createTexture(uint32_t context_id, const void* descriptor, 
                          const void* initial_data, uint32_t* texture_id);
    IOReturn updateTexture(uint32_t context_id, uint32_t texture_id, uint32_t mip_level,
                          const void* region, const void* data);
    IOReturn generateMipmaps(uint32_t context_id, uint32_t texture_id);
    IOReturn bindTexture(uint32_t context_id, uint32_t binding_point, uint32_t texture_id);
    
    // Advanced rendering
    IOReturn createFramebuffer(uint32_t context_id, uint32_t width, uint32_t height,
                              uint32_t color_format, uint32_t depth_format,
                              uint32_t* framebuffer_id);
    IOReturn beginRenderPass(uint32_t context_id, uint32_t framebuffer_id);
    IOReturn endRenderPass(uint32_t context_id);
    IOReturn setViewport(uint32_t context_id, float x, float y, float width, float height);
    IOReturn drawPrimitives(uint32_t context_id, uint32_t primitive_type, 
                           uint32_t vertex_count, uint32_t first_vertex);
    IOReturn drawIndexedPrimitives(uint32_t context_id, uint32_t primitive_type,
                                  uint32_t index_count, uint32_t first_index);
    
    // Phase 3 Advanced API Integration
    VMPhase3Manager* getPhase3Manager() { return m_phase3_manager; }
    VMMetalBridge* getMetalBridge() { return m_metal_bridge; }
    VMVirtIOGPU* getGPUDevice() { return m_gpu_device; }
    IOReturn enablePhase3Features(uint32_t feature_mask);
    IOReturn disablePhase3Features(uint32_t feature_mask);
    IOReturn getPhase3Status(void* status_buffer, size_t* buffer_size);
    
    // Cross-API resource sharing
    IOReturn shareResourceWithMetal(uint32_t resource_id, uint32_t* metal_resource_id);
    IOReturn shareResourceWithOpenGL(uint32_t resource_id, uint32_t* gl_resource_id);
    IOReturn shareResourceWithCoreAnimation(uint32_t resource_id, uint32_t* ca_resource_id);
    
    // Additional rendering operations needed by OpenGL bridge
    IOReturn clearColorBuffer(uint32_t context_id, float r, float g, float b, float a);
    IOReturn clearDepthBuffer(uint32_t context_id, float depth);
    IOReturn enableDepthTest(uint32_t context_id, bool enable);
    IOReturn enableBlending(uint32_t context_id, bool enable);
    IOReturn getPerformanceStats(void* stats_buffer, size_t* buffer_size);
    bool supportsAcceleration(uint32_t feature_flags);
    void logAcceleratorState();
    
    // Helper methods for enhanced endRenderPass functionality
    IOReturn unbindTexture(uint32_t context_id, uint32_t unit);
    IOReturn garbageCollectTextures(uint32_t* textures_freed);
    IOReturn getTextureMemoryUsage(uint64_t* memory_used);
    IOReturn returnCommandBuffer(uint32_t context_id);
    IOReturn getCommandPoolStatistics(void* stats);
    IOReturn setShaderUniform(uint32_t program_id, const char* name, const void* data, size_t size);
    
    // Advanced render state management
    IOReturn saveRenderState(uint32_t context_id, void** state_buffer, size_t* buffer_size);
    IOReturn restoreRenderState(uint32_t context_id, const void* state_buffer, size_t buffer_size);
    
    // GPU synchronization and resource management
    IOReturn synchronizeGPUOperations(uint32_t context_id, bool wait_for_completion);
    IOReturn defragmentTextureMemory(uint32_t* compaction_ratio);
    
    // Hardware acceleration methods
    IOReturn performBlit(IOPixelInformation* sourcePixelInfo, IOPixelInformation* destPixelInfo, 
                        int sourceX, int sourceY, int destX, int destY);
    IOReturn performFill(IOPixelInformation* pixelInfo, int x, int y, int width, int height, 
                        uint32_t color);
    IOReturn synchronize();
    
    // Performance profiling
    IOReturn startPerformanceProfiling(uint32_t context_id);
    IOReturn stopPerformanceProfiling(uint32_t context_id, void** results_buffer, size_t* buffer_size);
    
    // Performance and timing utilities
    uint64_t getCurrentTimestamp();
    uint64_t convertToMicroseconds(uint64_t timestamp_delta);
    uint32_t calculateFPS(uint64_t frame_time_microseconds);
    
    // Memory management utilities
    IOReturn flushVRAMCache(void* vram_ptr, size_t size);
    IOReturn updateFrameStatistics(uint32_t frame_number, uint32_t pixels_updated);
    
    // Power management
    IOReturn setPowerState(unsigned long powerState, IOService* whatDevice) override;
};

// User client for 3D acceleration
class VMQemuVGA3DUserClient : public IOUserClient
{
    OSDeclareDefaultStructors(VMQemuVGA3DUserClient);

private:
    VMQemuVGAAccelerator* m_accelerator;
    task_t m_task;
    uint32_t m_context_id;
    bool m_has_context;
    
public:
    virtual bool initWithTask(task_t owningTask, void* securityToken, UInt32 type,
                             OSDictionary* properties) override;
    virtual bool start(IOService* provider) override;
    virtual IOReturn clientClose() override;
    virtual IOReturn clientDied() override;
    
    // Method dispatch table
    virtual IOReturn externalMethod(uint32_t selector, IOExternalMethodArguments* args,
                                   IOExternalMethodDispatch* dispatch, OSObject* target,
                                   void* reference) override;

    // Factory method
    static VMQemuVGA3DUserClient* withTask(task_t owningTask);

public:
    // Static method handlers
    static IOReturn sCreate3DContext(OSObject* target, void* reference,
                                   IOExternalMethodArguments* args);
    static IOReturn sDestroy3DContext(OSObject* target, void* reference,
                                    IOExternalMethodArguments* args);
    static IOReturn sCreate3DSurface(OSObject* target, void* reference,
                                   IOExternalMethodArguments* args);
    static IOReturn sDestroy3DSurface(OSObject* target, void* reference,
                                    IOExternalMethodArguments* args);
    static IOReturn sSubmit3DCommands(OSObject* target, void* reference,
                                    IOExternalMethodArguments* args);
    static IOReturn sPresent3DSurface(OSObject* target, void* reference,
                                    IOExternalMethodArguments* args);
    static IOReturn sGetCapabilities(OSObject* target, void* reference,
                                   IOExternalMethodArguments* args);
};

// Method selectors for user client
enum {
    kVM3DUserClientCreate3DContext = 0,
    kVM3DUserClientDestroy3DContext,
    kVM3DUserClientCreate3DSurface,
    kVM3DUserClientDestroy3DSurface,
    kVM3DUserClientSubmit3DCommands,
    kVM3DUserClientPresent3DSurface,
    kVM3DUserClientGetCapabilities,
    kVM3DUserClientMethodCount
};

#endif /* __VMQemuVGAAccelerator_H__ */

#ifndef __VMCommandBuffer_H__
#define __VMCommandBuffer_H__

#include <IOKit/IOService.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <libkern/OSAtomic.h>

// Forward declarations
class VMQemuVGAAccelerator;
class VMVirtIOGPU;

// Function pointer typedef for command completion callbacks
typedef void (*VMCommandBufferCallback)(void* context, IOReturn status);

// Command buffer state constants
#define kVMCommandBufferStateIdle          0
#define kVMCommandBufferStateCommitted     1  
#define kVMCommandBufferStateExecuting     2
#define kVMCommandBufferStateCompleted     3
#define kVMCommandBufferStateError         4

// Command types
#define kVMGPUCommandTypeDraw              1
#define kVMGPUCommandTypeCompute           2

// Magic constants for GPU command buffer
#define VM_GPU_COMMAND_MAGIC              0x564D4350  // "VMCP"
#define VM_GPU_COMMAND_VERSION            1

// Command buffer states
enum VMCommandBufferState {
    VM_COMMAND_BUFFER_STATE_INITIAL = 0,
    VM_COMMAND_BUFFER_STATE_RECORDING = 1,
    VM_COMMAND_BUFFER_STATE_EXECUTABLE = 2,
    VM_COMMAND_BUFFER_STATE_PENDING = 3,
    VM_COMMAND_BUFFER_STATE_INVALID = 4
};

// Command types for GPU execution
enum VMGPUCommandType {
    // Render commands
    VM_CMD_BEGIN_RENDER_PASS = 0x1000,
    VM_CMD_END_RENDER_PASS = 0x1001,
    VM_CMD_BIND_PIPELINE = 0x1002,
    VM_CMD_BIND_DESCRIPTOR_SETS = 0x1003,
    VM_CMD_BIND_VERTEX_BUFFERS = 0x1004,
    VM_CMD_BIND_INDEX_BUFFER = 0x1005,
    VM_CMD_DRAW = 0x1006,
    VM_CMD_DRAW_INDEXED = 0x1007,
    VM_CMD_DRAW_INDIRECT = 0x1008,
    VM_CMD_DRAW_INDEXED_INDIRECT = 0x1009,
    
    // Compute commands
    VM_CMD_BIND_COMPUTE_PIPELINE = 0x2000,
    VM_CMD_DISPATCH = 0x2001,
    VM_CMD_DISPATCH_INDIRECT = 0x2002,
    
    // Transfer commands
    VM_CMD_COPY_BUFFER = 0x3000,
    VM_CMD_COPY_IMAGE = 0x3001,
    VM_CMD_COPY_BUFFER_TO_IMAGE = 0x3002,
    VM_CMD_COPY_IMAGE_TO_BUFFER = 0x3003,
    VM_CMD_UPDATE_BUFFER = 0x3004,
    VM_CMD_FILL_BUFFER = 0x3005,
    VM_CMD_CLEAR_COLOR_IMAGE = 0x3006,
    VM_CMD_CLEAR_DEPTH_STENCIL_IMAGE = 0x3007,
    VM_CMD_RESOLVE_IMAGE = 0x3008,
    
    // Synchronization commands
    VM_CMD_PIPELINE_BARRIER = 0x4000,
    VM_CMD_MEMORY_BARRIER = 0x4001,
    VM_CMD_EXECUTION_BARRIER = 0x4002,
    
    // Debug commands
    VM_CMD_BEGIN_DEBUG_LABEL = 0x5000,
    VM_CMD_END_DEBUG_LABEL = 0x5001,
    VM_CMD_INSERT_DEBUG_LABEL = 0x5002,
    
    // State commands
    VM_CMD_SET_VIEWPORT = 0x6000,
    VM_CMD_SET_SCISSOR = 0x6001,
    VM_CMD_SET_LINE_WIDTH = 0x6002,
    VM_CMD_SET_DEPTH_BIAS = 0x6003,
    VM_CMD_SET_BLEND_CONSTANTS = 0x6004,
    VM_CMD_SET_DEPTH_BOUNDS = 0x6005,
    VM_CMD_SET_STENCIL_COMPARE_MASK = 0x6006,
    VM_CMD_SET_STENCIL_WRITE_MASK = 0x6007,
    VM_CMD_SET_STENCIL_REFERENCE = 0x6008,
    
    // Query commands
    VM_CMD_BEGIN_QUERY = 0x7000,
    VM_CMD_END_QUERY = 0x7001,
    VM_CMD_RESET_QUERY_POOL = 0x7002,
    VM_CMD_WRITE_TIMESTAMP = 0x7003,
    VM_CMD_COPY_QUERY_POOL_RESULTS = 0x7004
};

// Command priority levels
enum VMCommandPriority {
    VM_COMMAND_PRIORITY_LOW = 0,
    VM_COMMAND_PRIORITY_NORMAL = 1,
    VM_COMMAND_PRIORITY_HIGH = 2,
    VM_COMMAND_PRIORITY_REALTIME = 3
};

// Buffer usage flags
enum VMCommandBufferUsage {
    VM_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT = 1 << 0,
    VM_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE = 1 << 1,
    VM_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE = 1 << 2
};

struct VMComputeCommandDescriptor {
    UInt32 workgroup_x;
    UInt32 workgroup_y;
    UInt32 workgroup_z;
    UInt32 thread_count;
};

struct VMDrawCommandDescriptor {
    UInt32 vertex_count;
    UInt32 instance_count;
    UInt32 first_vertex;
    UInt32 first_instance;
};

// Command header structure
struct VMCommandHeader {
    VMGPUCommandType type;
    uint32_t size;        // Size of command data following header
    uint32_t sequence;    // Sequence number for ordering
    uint32_t flags;       // Command-specific flags
};

// Generic command structure
struct VMGPUCommand {
    VMCommandHeader header;
    uint8_t data[];      // Command-specific data
};

// Draw command structures
struct VMDrawCommand {
    VMCommandHeader header;
    uint32_t vertex_count;
    uint32_t instance_count;
    uint32_t first_vertex;
    uint32_t first_instance;
};

struct VMDrawIndexedCommand {
    VMCommandHeader header;
    uint32_t index_count;
    uint32_t instance_count;
    uint32_t first_index;
    int32_t vertex_offset;
    uint32_t first_instance;
};

struct VMDispatchCommand {
    VMCommandHeader header;
    uint32_t group_count_x;
    uint32_t group_count_y;
    uint32_t group_count_z;
};

// Viewport and scissor structures
struct VMViewport {
    float x, y;
    float width, height;
    float min_depth, max_depth;
};

struct VMRect2D {
    int32_t x, y;
    uint32_t width, height;
};

struct VMSetViewportCommand {
    VMCommandHeader header;
    uint32_t first_viewport;
    uint32_t viewport_count;
    VMViewport viewports[];
};

// Pipeline barrier for synchronization
enum VMPipelineStageFlags {
    VM_PIPELINE_STAGE_TOP_OF_PIPE = 1 << 0,
    VM_PIPELINE_STAGE_DRAW_INDIRECT = 1 << 1,
    VM_PIPELINE_STAGE_VERTEX_INPUT = 1 << 2,
    VM_PIPELINE_STAGE_VERTEX_SHADER = 1 << 3,
    VM_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER = 1 << 4,
    VM_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER = 1 << 5,
    VM_PIPELINE_STAGE_GEOMETRY_SHADER = 1 << 6,
    VM_PIPELINE_STAGE_FRAGMENT_SHADER = 1 << 7,
    VM_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS = 1 << 8,
    VM_PIPELINE_STAGE_LATE_FRAGMENT_TESTS = 1 << 9,
    VM_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT = 1 << 10,
    VM_PIPELINE_STAGE_COMPUTE_SHADER = 1 << 11,
    VM_PIPELINE_STAGE_TRANSFER = 1 << 12,
    VM_PIPELINE_STAGE_BOTTOM_OF_PIPE = 1 << 13,
    VM_PIPELINE_STAGE_HOST = 1 << 14,
    VM_PIPELINE_STAGE_ALL_GRAPHICS = 1 << 15,
    VM_PIPELINE_STAGE_ALL_COMMANDS = 1 << 16
};

struct VMPipelineBarrierCommand {
    VMCommandHeader header;
    uint32_t src_stage_mask;
    uint32_t dst_stage_mask;
    uint32_t dependency_flags;
    uint32_t memory_barrier_count;
    uint32_t buffer_memory_barrier_count;
    uint32_t image_memory_barrier_count;
    // Followed by barrier data
};

class VMVirtIOGPU;
class VMQemuVGAAccelerator;

class VMCommandBuffer : public OSObject
{
    OSDeclareDefaultStructors(VMCommandBuffer);

private:
    VMQemuVGAAccelerator* m_accelerator;
    VMVirtIOGPU* m_gpu_device;
    uint32_t m_context_id;
    
    // Command buffer storage
    IOBufferMemoryDescriptor* m_command_data;
    IOBufferMemoryDescriptor* m_buffer_memory;  // Additional buffer memory
    size_t m_buffer_size;
    size_t m_current_offset;
    size_t m_current_size;      // Current size used
    size_t m_max_size;
    uint32_t m_max_commands;    // Maximum command count
    
    // Command and resource storage
    OSArray* m_commands;        // Command array
    OSArray* m_resources;       // Resource bindings
    
    // State tracking
    VMCommandBufferState m_state;
    uint32_t m_usage_flags;
    VMCommandPriority m_priority;
    uint32_t m_sequence_counter;
    uint64_t m_execution_time;  // Execution time tracking
    
    // Completion callback
    VMCommandBufferCallback m_completion_callback;
    void* m_completion_context;
    
    // Statistics
    uint32_t m_command_count;
    uint64_t m_submission_time;
    uint64_t m_completion_time;
    
    // Debug support
    OSArray* m_debug_labels;     // Stack of debug labels
    bool m_debug_enabled;
    
    // Synchronization
    IOLock* m_command_lock;
    
    // Internal methods
    IOReturn ensureCapacity(size_t additional_size);
    IOReturn writeCommand(const void* command_data, size_t size);
    IOReturn validateState(VMCommandBufferState required_state);
    uint32_t getNextSequence() { return OSIncrementAtomic(&m_sequence_counter); }
    void cleanupCommand(VMGPUCommand* command);
    
public:
    static VMCommandBuffer* withAccelerator(VMQemuVGAAccelerator* accelerator, 
                                          uint32_t context_id);
    
    virtual bool init(VMQemuVGAAccelerator* accelerator, uint32_t context_id);
    virtual void free() override;
    
    // Command buffer lifecycle
    IOReturn begin(uint32_t usage_flags = 0);
    IOReturn end();
    IOReturn reset();
    IOReturn submit();
    IOReturn submitAndWait();
    
    // State queries
    VMCommandBufferState getState() const { return m_state; }
    uint32_t getCommandCount() const { return m_command_count; }
    size_t getCurrentSize() const { return m_current_offset; }
    size_t getRemainingSpace() const { return m_max_size - m_current_offset; }
    bool isRecording() const { return m_state == VM_COMMAND_BUFFER_STATE_RECORDING; }
    
    // Priority and performance
    void setPriority(VMCommandPriority priority) { m_priority = priority; }
    VMCommandPriority getPriority() const { return m_priority; }
    
    // Render commands
    IOReturn beginRenderPass(uint32_t framebuffer_id, const VMRect2D* render_area,
                           uint32_t clear_value_count, const float* clear_values);
    IOReturn endRenderPass();
    IOReturn bindPipeline(uint32_t pipeline_id, bool is_compute = false);
    IOReturn bindVertexBuffers(uint32_t first_binding, uint32_t binding_count,
                             const uint32_t* buffer_ids, const uint64_t* offsets);
    IOReturn bindIndexBuffer(uint32_t buffer_id, uint64_t offset, bool is_16bit = false);
    IOReturn bindDescriptorSets(uint32_t pipeline_bind_point, uint32_t layout_id,
                              uint32_t first_set, uint32_t descriptor_set_count,
                              const uint32_t* descriptor_sets);
    
    // Draw commands
    IOReturn draw(uint32_t vertex_count, uint32_t instance_count = 1,
                 uint32_t first_vertex = 0, uint32_t first_instance = 0);
    IOReturn drawIndexed(uint32_t index_count, uint32_t instance_count = 1,
                        uint32_t first_index = 0, int32_t vertex_offset = 0,
                        uint32_t first_instance = 0);
    IOReturn drawIndirect(uint32_t buffer_id, uint64_t offset, uint32_t draw_count, uint32_t stride);
    IOReturn drawIndexedIndirect(uint32_t buffer_id, uint64_t offset, uint32_t draw_count, uint32_t stride);
    
    // Compute commands
    IOReturn dispatch(uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z);
    IOReturn dispatchIndirect(uint32_t buffer_id, uint64_t offset);
    
    // Transfer commands
    IOReturn copyBuffer(uint32_t src_buffer_id, uint32_t dst_buffer_id,
                       uint64_t src_offset, uint64_t dst_offset, uint64_t size);
    IOReturn copyImage(uint32_t src_image_id, uint32_t dst_image_id,
                      const VMRect2D* src_region, const VMRect2D* dst_region);
    IOReturn updateBuffer(uint32_t buffer_id, uint64_t offset, 
                         const void* data, size_t size);
    IOReturn fillBuffer(uint32_t buffer_id, uint64_t offset, uint64_t size, uint32_t data);
    IOReturn clearColorImage(uint32_t image_id, const float* clear_color);
    IOReturn clearDepthStencilImage(uint32_t image_id, float depth, uint32_t stencil);
    
    // Dynamic state commands
    IOReturn setViewport(uint32_t first_viewport, uint32_t viewport_count,
                        const VMViewport* viewports);
    IOReturn setScissor(uint32_t first_scissor, uint32_t scissor_count,
                       const VMRect2D* scissors);
    IOReturn setLineWidth(float line_width);
    IOReturn setDepthBias(float constant_factor, float clamp, float slope_factor);
    IOReturn setBlendConstants(const float* constants);
    
    // Synchronization
    IOReturn pipelineBarrier(uint32_t src_stage_mask, uint32_t dst_stage_mask,
                           uint32_t dependency_flags = 0);
    IOReturn memoryBarrier();
    IOReturn executionBarrier();
    
    // Debug and profiling
    IOReturn beginDebugLabel(const char* label_name, const float* color = nullptr);
    IOReturn endDebugLabel();
    IOReturn insertDebugLabel(const char* label_name, const float* color = nullptr);
    void enableDebugging(bool enable) { m_debug_enabled = enable; }
    
    // Command building methods
    IOReturn addDrawCommand(VMDrawCommandDescriptor* descriptor);
    IOReturn addComputeCommand(VMComputeCommandDescriptor* descriptor);
    IOReturn addResourceBinding(uint32_t binding_point, uint32_t resource_id, uint32_t resource_type);
    
    // Query commands
    IOReturn beginQuery(uint32_t query_pool_id, uint32_t query_index, uint32_t flags = 0);
    IOReturn endQuery(uint32_t query_pool_id, uint32_t query_index);
    IOReturn writeTimestamp(uint32_t pipeline_stage, uint32_t query_pool_id, uint32_t query_index);
    
    // Optimization
    IOReturn optimizeCommands();  // Optimize command stream before submission
    IOReturn validateCommands();  // Validate command stream for errors
    
    // Statistics and debugging
    uint64_t getSubmissionTime() const { return m_submission_time; }
    uint64_t getCompletionTime() const { return m_completion_time; }
    uint64_t getExecutionDuration() const { return m_completion_time - m_submission_time; }
    IOReturn dumpCommands(char* buffer, size_t buffer_size);
    IOReturn getPerformanceCounters(uint32_t* draw_calls, uint32_t* compute_dispatches,
                                   uint32_t* memory_transfers);
};

// Command buffer pool for efficient allocation and reuse
class VMCommandBufferPool : public OSObject
{
    OSDeclareDefaultStructors(VMCommandBufferPool);

private:
    VMQemuVGAAccelerator* m_accelerator;
    OSArray* m_available_buffers;
    OSArray* m_active_buffers;
    uint32_t m_context_id;
    uint32_t m_max_buffers;
    IOLock* m_pool_lock;

public:
    static VMCommandBufferPool* withAccelerator(VMQemuVGAAccelerator* accelerator,
                                               uint32_t context_id, uint32_t max_buffers = 16);
    
    virtual bool init(VMQemuVGAAccelerator* accelerator, uint32_t context_id, uint32_t max_buffers);
    virtual void free() override;
    
    IOReturn allocateCommandBuffer(VMCommandBuffer** out_buffer);
    IOReturn releaseCommandBuffer(VMCommandBuffer* buffer);
    IOReturn resetPool();
    
    uint32_t getActiveBufferCount() const;
    uint32_t getAvailableBufferCount() const;
};

#endif /* __VMCommandBuffer_H__ */

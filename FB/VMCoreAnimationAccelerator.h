#ifndef VMCoreAnimationAccelerator_h
#define VMCoreAnimationAccelerator_h

#include <IOKit/IOService.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/graphics/IOFramebuffer.h>
#include <libkern/c++/OSObject.h>
#include <libkern/c++/OSArray.h>
#include <libkern/c++/OSDictionary.h>
// Note: QuartzCore not available in kernel extensions, using kernel-compatible types

// Forward declarations
class VMQemuVGAAccelerator;
class VMVirtIOGPU;
class VMMetalBridge;

// CoreAnimation layer types
typedef enum {
    VM_CA_LAYER_BASIC = 0,
    VM_CA_LAYER_SHAPE = 1,
    VM_CA_LAYER_TEXT = 2,
    VM_CA_LAYER_GRADIENT = 3,
    VM_CA_LAYER_TRANSFORM = 4,
    VM_CA_LAYER_OPENGL = 5,
    VM_CA_LAYER_METAL = 6
} VMCALayerType;

// CoreAnimation animation types
typedef enum {
    VM_CA_ANIMATION_BASIC = 0,
    VM_CA_ANIMATION_KEYFRAME = 1,
    VM_CA_ANIMATION_GROUP = 2,
    VM_CA_ANIMATION_TRANSITION = 3,
    VM_CA_ANIMATION_SPRING = 4
} VMCAAnimationType;

// CoreAnimation timing functions
typedef enum {
    VM_CA_TIMING_LINEAR = 0,
    VM_CA_TIMING_EASE_IN = 1,
    VM_CA_TIMING_EASE_OUT = 2,
    VM_CA_TIMING_EASE_IN_OUT = 3,
    VM_CA_TIMING_DEFAULT = 4
} VMCATimingFunction;

// CoreAnimation filter types
typedef enum {
    VM_CA_FILTER_NONE = 0,
    VM_CA_FILTER_LINEAR = 1,
    VM_CA_FILTER_NEAREST = 2,
    VM_CA_FILTER_TRILINEAR = 3
} VMCAFilterType;

// 2D transformation matrix (3x3)
typedef struct {
    float m11, m12, m13;
    float m21, m22, m23;
    float m31, m32, m33;
} VMCATransform2D;

// 3D transformation matrix (4x4)
typedef struct {
    float m11, m12, m13, m14;
    float m21, m22, m23, m24;
    float m31, m32, m33, m34;
    float m41, m42, m43, m44;
} VMCATransform3D;

// Color representation
typedef struct {
    float red;
    float green;
    float blue;
    float alpha;
} VMCAColor;

// Rectangle structure
typedef struct {
    float x, y, width, height;
} VMCARect;

// Point structure
typedef struct {
    float x, y;
} VMCAPoint;

// Size structure
typedef struct {
    float width, height;
} VMCASize;

// Layer properties descriptor
typedef struct {
    VMCARect frame;
    VMCARect bounds;
    VMCAPoint position;
    VMCAPoint anchor_point;
    VMCATransform3D transform;
    float opacity;
    VMCAColor background_color;
    VMCAColor border_color;
    float border_width;
    float corner_radius;
    bool hidden;
    bool masksToBounds;
    VMCAFilterType magnification_filter;
    VMCAFilterType minification_filter;
} VMCALayerProperties;

// Animation descriptor
typedef struct {
    VMCAAnimationType type;
    const char* key_path;
    void* from_value;
    void* to_value;
    double duration;
    double delay;
    float repeat_count;
    bool autoreverses;
    VMCATimingFunction timing_function;
    uint32_t fill_mode;
} VMCAAnimationDescriptor;

// Gradient descriptor
typedef struct {
    VMCAColor* colors;
    float* locations;
    uint32_t color_count;
    VMCAPoint start_point;
    VMCAPoint end_point;
    uint32_t gradient_type; // linear, radial, etc.
} VMCAGradientDescriptor;

// Text layer descriptor
typedef struct {
    const char* text;
    const char* font_name;
    float font_size;
    VMCAColor text_color;
    uint32_t alignment_mode;
    uint32_t truncation_mode;
    bool wrapped;
} VMCATextDescriptor;

// Compositor state
typedef struct {
    uint64_t frame_number;
    double timestamp;
    uint32_t active_layers;
    uint32_t dirty_layers;
    uint32_t animations_running;
    bool needs_display;
    bool needs_layout;
} VMCACompositorState;

/**
 * @class VMCoreAnimationAccelerator
 * @brief Hardware-Accelerated CoreAnimation Support for VMQemuVGA
 * 
 * This class provides hardware-accelerated CoreAnimation support for the
 * VMQemuVGA 3D acceleration system, enabling smooth UI animations and
 * compositing operations through GPU acceleration in virtual machines.
 */
class VMCoreAnimationAccelerator : public OSObject
{
    OSDeclareDefaultStructors(VMCoreAnimationAccelerator);
    
private:
    VMQemuVGAAccelerator* m_accelerator;
    VMVirtIOGPU* m_gpu_device;
    VMMetalBridge* m_metal_bridge;
    IOFramebuffer* m_framebuffer;
    IORecursiveLock* m_lock;
    
    // Layer management
    OSArray* m_layers;
    OSArray* m_animations;
    OSArray* m_render_contexts;
    OSDictionary* m_layer_map;
    
    // Composition hierarchy
    OSArray* m_layer_tree;
    uint32_t m_root_layer_id;
    uint32_t m_presentation_layer_id;
    
    // Resource management
    uint32_t m_next_layer_id;
    uint32_t m_next_animation_id;
    OSDictionary* m_texture_cache;
    OSArray* m_render_targets;
    
    // Compositor state
    VMCACompositorState m_compositor_state;
    IOWorkLoop* m_animation_work_loop;
    IOTimerEventSource* m_animation_timer;
    bool m_compositor_running;
    bool m_compositor_active;        // Added missing variable
    uint64_t m_frame_interval;       // Added missing variable  
    double m_display_refresh_rate;
    
    // Performance counters
    uint64_t m_layers_rendered;
    uint64_t m_animations_processed;
    uint64_t m_composition_operations;
    uint64_t m_texture_uploads;
    uint64_t m_frame_drops;
    
    // Feature support
    bool m_supports_hardware_composition;
    bool m_supports_3d_transforms;
    bool m_supports_filters;
    bool m_supports_video_layers;
    bool m_supports_async_rendering;
    
public:
    // Initialization and cleanup
    virtual bool init() override;
    virtual void free() override;
    
    // Setup and configuration
    bool initWithAccelerator(VMQemuVGAAccelerator* accelerator);
    IOReturn setupCoreAnimationSupport();
    IOReturn startCompositor();
    IOReturn stopCompositor();
    
    // Layer management
    IOReturn createLayer(VMCALayerType type, const VMCALayerProperties* properties, 
                        uint32_t* layer_id);
    IOReturn destroyLayer(uint32_t layer_id);
    IOReturn updateLayerProperties(uint32_t layer_id, const VMCALayerProperties* properties);
    IOReturn getLayerProperties(uint32_t layer_id, VMCALayerProperties* properties);
    
    // Layer hierarchy
    IOReturn addSublayer(uint32_t parent_layer_id, uint32_t child_layer_id);
    IOReturn removeSublayer(uint32_t parent_layer_id, uint32_t child_layer_id);
    IOReturn insertSublayer(uint32_t parent_layer_id, uint32_t child_layer_id, uint32_t index);
    IOReturn setRootLayer(uint32_t layer_id);
    uint32_t getRootLayer();
    
    // Layer content
    IOReturn setLayerContents(uint32_t layer_id, IOMemoryDescriptor* contents);
    IOReturn setLayerContentsGravity(uint32_t layer_id, uint32_t gravity);
    IOReturn setLayerContentsScale(uint32_t layer_id, float scale);
    IOReturn setLayerMask(uint32_t layer_id, uint32_t mask_layer_id);
    
    // Specialized layers
    IOReturn createTextLayer(uint32_t layer_id, const VMCATextDescriptor* descriptor);
    IOReturn createShapeLayer(uint32_t layer_id, IOMemoryDescriptor* path_data);
    IOReturn createGradientLayer(uint32_t layer_id, const VMCAGradientDescriptor* descriptor);
    IOReturn createVideoLayer(uint32_t layer_id, IOMemoryDescriptor* video_surface);
    
    // Animation management
    IOReturn addAnimation(uint32_t layer_id, const VMCAAnimationDescriptor* descriptor,
                         uint32_t* animation_id);
    IOReturn removeAnimation(uint32_t layer_id, uint32_t animation_id);
    IOReturn removeAllAnimations(uint32_t layer_id);
    IOReturn pauseAnimation(uint32_t animation_id);
    IOReturn resumeAnimation(uint32_t animation_id);
    IOReturn setAnimationSpeed(uint32_t animation_id, float speed);
    
    // Transform operations
    IOReturn setLayerTransform(uint32_t layer_id, const VMCATransform3D* transform);
    IOReturn getLayerTransform(uint32_t layer_id, VMCATransform3D* transform);
    IOReturn setLayerAnchorPoint(uint32_t layer_id, const VMCAPoint* anchor_point);
    IOReturn translateLayer(uint32_t layer_id, float dx, float dy, float dz);
    IOReturn rotateLayer(uint32_t layer_id, float angle, float x, float y, float z);
    IOReturn scaleLayer(uint32_t layer_id, float sx, float sy, float sz);
    
    // Rendering and composition
    IOReturn setNeedsDisplay(uint32_t layer_id);
    IOReturn setNeedsDisplayInRect(uint32_t layer_id, const VMCARect* rect);
    IOReturn displayIfNeeded();
    IOReturn renderLayer(uint32_t layer_id, uint32_t render_context_id);
    IOReturn compositeFrame();
    IOReturn presentFrame();
    
    // Hit testing
    uint32_t hitTest(const VMCAPoint* point);
    IOReturn convertPoint(const VMCAPoint* point, uint32_t from_layer_id, 
                         uint32_t to_layer_id, VMCAPoint* converted_point);
    IOReturn convertRect(const VMCARect* rect, uint32_t from_layer_id,
                        uint32_t to_layer_id, VMCARect* converted_rect);
    
    // Filter effects
    IOReturn addFilter(uint32_t layer_id, const char* filter_name, 
                      const void* filter_parameters);
    IOReturn removeFilter(uint32_t layer_id, const char* filter_name);
    IOReturn setBackgroundFilter(uint32_t layer_id, const char* filter_name,
                                const void* filter_parameters);
    
    // Performance and debugging
    IOReturn getCompositorState(VMCACompositorState* state);
    IOReturn getCAPerformanceStats(void* stats_buffer, size_t* buffer_size);
    void resetCACounters();
    void logCoreAnimationState();
    
    // Display synchronization
    IOReturn setDisplayRefreshRate(double refresh_rate);
    double getDisplayRefreshRate();
    IOReturn enableVSync(bool enable);
    IOReturn waitForVBlank();
    
private:
    // Internal compositor methods
    static void animationTimerHandler(OSObject* owner, IOTimerEventSource* sender);
    void processAnimations();
    void updateLayerTree();
    void renderCompositeFrame();
    
    // Layer management helpers
    OSObject* findLayer(uint32_t layer_id);
    OSObject* findAnimation(uint32_t animation_id);
    uint32_t allocateLayerId();
    uint32_t allocateAnimationId();
    void releaseLayerId(uint32_t layer_id);
    void releaseAnimationId(uint32_t animation_id);
    
    // Rendering helpers
    IOReturn renderLayerContent(uint32_t layer_id, const VMCARect* bounds);
    IOReturn applyLayerFilters(uint32_t layer_id, IOMemoryDescriptor* content);
    IOReturn compositeLayerSubtree(uint32_t layer_id, const VMCATransform3D* parent_transform);
    IOReturn uploadLayerTexture(uint32_t layer_id, IOMemoryDescriptor* content);
    
    // Transform calculations
    void multiplyTransforms(const VMCATransform3D* a, const VMCATransform3D* b, 
                           VMCATransform3D* result);
    void invertTransform(const VMCATransform3D* transform, VMCATransform3D* inverse);
    void transformPoint(const VMCAPoint* point, const VMCATransform3D* transform, 
                       VMCAPoint* result);
    void transformRect(const VMCARect* rect, const VMCATransform3D* transform,
                      VMCARect* result);
    
    // Animation interpolation
    float interpolateFloat(float from, float to, double progress, VMCATimingFunction timing);
    VMCAColor interpolateColor(const VMCAColor* from, const VMCAColor* to, double progress);
    VMCATransform3D interpolateTransform(const VMCATransform3D* from, const VMCATransform3D* to,
                                        double progress);
    
    // Timing functions
    double applyTimingFunction(double input, VMCATimingFunction function);
    double cubicBezier(double input, double x1, double y1, double x2, double y2);
    
    // Resource management
    IOReturn cacheTexture(uint32_t layer_id, IOMemoryDescriptor* texture);
    IOMemoryDescriptor* getCachedTexture(uint32_t layer_id);
    void evictTextureCache();
    IOReturn allocateRenderTarget(uint32_t width, uint32_t height, uint32_t* target_id);
    void deallocateRenderTarget(uint32_t target_id);
    
    // Performance optimization
    bool shouldSkipLayer(uint32_t layer_id);
    bool isLayerVisible(uint32_t layer_id, const VMCARect* visible_rect);
    void optimizeLayerTree();
    void cullInvisibleLayers();
    void batchRenderOperations();
};

#endif /* VMCoreAnimationAccelerator_h */

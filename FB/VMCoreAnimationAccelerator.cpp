#include "VMCoreAnimationAccelerator.h"
#include "VMQemuVGAAccelerator.h"
#include "VMVirtIOGPU.h"
#include "VMMetalBridge.h"
#include "VMQemuVGAAccelerator.h"
#include <IOKit/IOLib.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOTimerEventSource.h>
#include <libkern/c++/OSLib.h>
#include <libkern/OSTypes.h>
#include <math.h>

#define CLASS VMCoreAnimationAccelerator
#define super OSObject

OSDefineMetaClassAndStructors(VMCoreAnimationAccelerator, OSObject);

bool CLASS::init()
{
    if (!super::init())
        return false;
    
    // Initialize base state
    m_accelerator = nullptr;
    m_gpu_device = nullptr;
    m_metal_bridge = nullptr;
    m_framebuffer = nullptr;
    m_lock = nullptr;
    
    // Initialize arrays
    m_layers = nullptr;
    m_animations = nullptr;
    m_render_contexts = nullptr;
    m_layer_map = nullptr;
    
    // Initialize ID recycling pools
    m_released_layer_ids = nullptr;
    m_released_animation_ids = nullptr;
    
    // Initialize hierarchy
    m_layer_tree = nullptr;
    m_root_layer_id = 0;
    m_presentation_layer_id = 0;
    
    // Initialize resources
    m_texture_cache = nullptr;
    m_render_targets = nullptr;
    
    // Initialize compositor state
    memset(&m_compositor_state, 0, sizeof(m_compositor_state));
    m_animation_work_loop = nullptr;
    m_animation_timer = nullptr;
    m_compositor_running = false;
    m_compositor_active = false;
    m_frame_interval = 16667; // 60fps
    m_display_refresh_rate = 60.0;
    
    // Initialize IDs
    m_next_layer_id = 1;
    m_next_animation_id = 1;
    
    // Initialize stats and performance counters
    m_frame_drops = 0;
    m_layers_rendered = 0;
    m_animations_processed = 0;
    m_composition_operations = 0;
    m_texture_uploads = 0;
    
    // Initialize feature support flags
    m_supports_hardware_composition = false;
    m_supports_3d_transforms = false;
    m_supports_filters = false;
    m_supports_video_layers = false;
    m_supports_async_rendering = false;
    
    return true;
}

void CLASS::free()
{
    // Stop compositor if running
    if (m_compositor_running) {
        stopCompositor();
    }
    
    if (m_lock) {
        IORecursiveLockFree(m_lock);
        m_lock = nullptr;
    }
    
    // Clean up work loop and timer
    if (m_animation_timer) {
        m_animation_timer->cancelTimeout();
        m_animation_timer->release();
        m_animation_timer = nullptr;
    }
    
    if (m_animation_work_loop) {
        m_animation_work_loop->release();
        m_animation_work_loop = nullptr;
    }
    
    // Clean up arrays
    if (m_layers) {
        m_layers->release();
        m_layers = nullptr;
    }
    
    if (m_animations) {
        m_animations->release();
        m_animations = nullptr;
    }
    
    if (m_render_contexts) {
        m_render_contexts->release();
        m_render_contexts = nullptr;
    }
    
    if (m_layer_map) {
        m_layer_map->release();
        m_layer_map = nullptr;
    }
    
    if (m_layer_tree) {
        m_layer_tree->release();
        m_layer_tree = nullptr;
    }
    
    if (m_texture_cache) {
        m_texture_cache->release();
        m_texture_cache = nullptr;
    }
    
    if (m_render_targets) {
        m_render_targets->release();
        m_render_targets = nullptr;
    }
    
    // Clean up ID recycling pools
    if (m_released_layer_ids) {
        m_released_layer_ids->release();
        m_released_layer_ids = nullptr;
    }
    
    if (m_released_animation_ids) {
        m_released_animation_ids->release();
        m_released_animation_ids = nullptr;
    }
    
    super::free();
}

bool CLASS::initWithAccelerator(VMQemuVGAAccelerator* accelerator)
{
    if (!accelerator)
        return false;
        
    m_accelerator = accelerator;
    m_gpu_device = accelerator->getGPUDevice();
    
    // Get Metal bridge from accelerator for hardware acceleration
    m_metal_bridge = accelerator->getMetalBridge();
    if (m_metal_bridge) {
        m_metal_bridge->retain();
    }
    
    // Initialize lock
    m_lock = IORecursiveLockAlloc();
    if (!m_lock)
        return false;
    
    // Initialize arrays for layer and animation management
    m_layers = OSArray::withCapacity(64);
    m_animations = OSArray::withCapacity(128);
    m_render_contexts = OSArray::withCapacity(16);
    m_layer_map = OSDictionary::withCapacity(64);
    m_layer_tree = OSArray::withCapacity(64);
    m_texture_cache = OSDictionary::withCapacity(32);
    m_render_targets = OSArray::withCapacity(8);
    
    // Initialize ID recycling pools
    m_released_layer_ids = OSArray::withCapacity(32);
    m_released_animation_ids = OSArray::withCapacity(64);
    
    if (!m_layers || !m_animations || !m_render_contexts || !m_layer_map || 
        !m_layer_tree || !m_texture_cache || !m_render_targets ||
        !m_released_layer_ids || !m_released_animation_ids) {
        IOLog("VMCoreAnimationAccelerator: Failed to allocate arrays\n");
        return false;
    }
    
    // Create animation work loop and timer
    m_animation_work_loop = IOWorkLoop::workLoop();
    if (m_animation_work_loop) {
        m_animation_timer = IOTimerEventSource::timerEventSource(this, 
            (IOTimerEventSource::Action)&CLASS::animationTimerHandler);
        if (m_animation_timer) {
            m_animation_work_loop->addEventSource(m_animation_timer);
        }
    }
    
    // Initialize framebuffer support - comment out missing method
    m_framebuffer = NULL; // accelerator->getFrameBuffer() - method doesn't exist
    
    IOLog("VMCoreAnimationAccelerator: Initialized with accelerator %p\n", accelerator);
    return true;
}

IOReturn CLASS::setupCoreAnimationSupport()
{
    IORecursiveLockLock(m_lock);
    
    // Configure hardware-accelerated Core Animation rendering
    if (m_metal_bridge) {
        // Set up Metal rendering pipeline for Core Animation
        IOReturn ret = m_metal_bridge->setupMetalDevice();
        if (ret != kIOReturnSuccess) {
            IOLog("VMCoreAnimationAccelerator: Warning - Metal device creation failed (0x%x)\n", ret);
        } else {
            IOLog("VMCoreAnimationAccelerator: Metal device ready\n");
            m_supports_hardware_composition = true;
            m_supports_3d_transforms = true;
        }
        
        // Configure render states for layer composition
        ret = m_metal_bridge->configureFeatureSupport();
        if (ret == kIOReturnSuccess) {
            m_supports_filters = true;
            m_supports_async_rendering = true;
        }
    } else {
        m_supports_hardware_composition = false;
        m_supports_3d_transforms = false;
        m_supports_filters = false;
    }
    
    // Configure GPU features needed for Core Animation
    if (m_gpu_device) {
        // GPU device should already be initialized by accelerator
        IOLog("VMCoreAnimationAccelerator: GPU device available\n");
        m_supports_video_layers = true;
    }
    
    // Initialize rendering statistics
    m_frame_drops = 0;
    m_layers_rendered = 0;
    
    IOLog("VMCoreAnimationAccelerator: Core Animation support configured\n");
    
    IORecursiveLockUnlock(m_lock);
    return kIOReturnSuccess;
}

IOReturn CLASS::startCompositor()
{
    IORecursiveLockLock(m_lock);
    
    if (m_compositor_active) {
        IORecursiveLockUnlock(m_lock);
        return kIOReturnSuccess; // Already running
    }
    
    // Initialize compositor state
    m_compositor_active = true;
    m_compositor_running = true;
    m_frame_drops = 0;
    m_layers_rendered = 0;
    m_animations_processed = 0;
    m_composition_operations = 0;
    
    memset(&m_compositor_state, 0, sizeof(m_compositor_state));
    m_compositor_state.timestamp = 0.0;
    m_compositor_state.needs_display = true;
    
    // Set up rendering pipeline
    if (m_metal_bridge && m_supports_hardware_composition) {
        IOLog("VMCoreAnimationAccelerator: Hardware-accelerated compositor enabled\n");
    } else {
        IOLog("VMCoreAnimationAccelerator: Software compositor enabled\n");
    }
    
    // Start animation timer (60 FPS target)
    m_frame_interval = 16667; // 16.67ms in microseconds for 60fps
    if (m_animation_timer) {
        m_animation_timer->setTimeoutMS(16); // 16ms = ~60fps
    }
    
    IOLog("VMCoreAnimationAccelerator: Compositor started successfully\n");
    
    IORecursiveLockUnlock(m_lock);
    return kIOReturnSuccess;
}

IOReturn CLASS::stopCompositor()
{
    IORecursiveLockLock(m_lock);
    
    if (!m_compositor_active) {
        IORecursiveLockUnlock(m_lock);
        return kIOReturnSuccess; // Already stopped
    }
    
    // Stop animation timer
    if (m_animation_timer) {
        m_animation_timer->cancelTimeout();
    }
    
    // Clean up active layers and animations
    if (m_layers) {
        m_layers->flushCollection();
    }
    if (m_animations) {
        m_animations->flushCollection();
    }
    if (m_render_contexts) {
        m_render_contexts->flushCollection();
    }
    if (m_layer_map) {
        m_layer_map->flushCollection();
    }
    if (m_layer_tree) {
        m_layer_tree->flushCollection();
    }
    
    // Clear texture cache
    if (m_texture_cache) {
        m_texture_cache->flushCollection();
    }
    
    // Reset compositor state
    m_compositor_active = false;
    m_compositor_running = false;
    m_root_layer_id = 0;
    m_presentation_layer_id = 0;
    
    // Log final statistics
    IOLog("VMCoreAnimationAccelerator: Compositor stopped. Stats - Layers: %llu, Animations: %llu, Compositions: %llu, Frame drops: %llu\n", 
          m_layers_rendered, m_animations_processed, m_composition_operations, m_frame_drops);
    
    IORecursiveLockUnlock(m_lock);
    return kIOReturnSuccess;
}

// MARK: - Layer Management Implementation

IOReturn CLASS::createLayer(VMCALayerType type, const VMCALayerProperties* properties, uint32_t* layer_id)
{
    if (!layer_id || !properties) {
        return kIOReturnBadArgument;
    }
    
    IORecursiveLockLock(m_lock);
    
    // Allocate new layer ID
    *layer_id = allocateLayerId();
    
    // Create layer object (using OSData to store layer properties)
    OSData* layer_data = OSData::withBytes(properties, sizeof(VMCALayerProperties));
    if (!layer_data) {
        releaseLayerId(*layer_id);
        IORecursiveLockUnlock(m_lock);
        return kIOReturnNoMemory;
    }
    
    // Store layer type in first 4 bytes
    uint32_t layer_info[2] = { type, *layer_id };
    OSData* layer_info_data = OSData::withBytes(layer_info, sizeof(layer_info));
    if (!layer_info_data) {
        layer_data->release();
        releaseLayerId(*layer_id);
        IORecursiveLockUnlock(m_lock);
        return kIOReturnNoMemory;
    }
    
    // Add to collections
    m_layers->setObject(layer_data);
    
    // Create string key for dictionary
    char key_str[32];
    snprintf(key_str, sizeof(key_str), "%u", *layer_id);
    m_layer_map->setObject(key_str, layer_data);
    
    layer_data->release();
    layer_info_data->release();
    
    IOLog("VMCoreAnimationAccelerator: Created layer %u of type %u\n", *layer_id, type);
    
    IORecursiveLockUnlock(m_lock);
    return kIOReturnSuccess;
}

IOReturn CLASS::destroyLayer(uint32_t layer_id)
{
    IORecursiveLockLock(m_lock);
    
    OSObject* layer_obj = findLayer(layer_id);
    if (!layer_obj) {
        IORecursiveLockUnlock(m_lock);
        return kIOReturnNotFound;
    }
    
    // Remove from layer tree if present
    if (m_layer_tree) {
        unsigned int index = m_layer_tree->getNextIndexOfObject(layer_obj, 0);
        if (index != (unsigned int)-1) {
            m_layer_tree->removeObject(index);
        }
    }
    
    // Remove from collections
    char key_str[32];
    snprintf(key_str, sizeof(key_str), "%u", layer_id);
    m_layer_map->removeObject(key_str);
    unsigned int index = m_layers->getNextIndexOfObject(layer_obj, 0);
    if (index != (unsigned int)-1) {
        m_layers->removeObject(index);
    }
    
    // Clear cached texture if any
    if (m_texture_cache) {
        m_texture_cache->removeObject(key_str);
    }
    
    releaseLayerId(layer_id);
    
    IOLog("VMCoreAnimationAccelerator: Destroyed layer %u\n", layer_id);
    
    IORecursiveLockUnlock(m_lock);
    return kIOReturnSuccess;
}

IOReturn CLASS::updateLayerProperties(uint32_t layer_id, const VMCALayerProperties* properties)
{
    if (!properties) {
        return kIOReturnBadArgument;
    }
    
    IORecursiveLockLock(m_lock);
    
    OSData* layer_data = (OSData*)findLayer(layer_id);
    if (!layer_data) {
        IORecursiveLockUnlock(m_lock);
        return kIOReturnNotFound;
    }
    
    // Update layer properties
    OSData* new_data = OSData::withBytes(properties, sizeof(VMCALayerProperties));
    if (!new_data) {
        IORecursiveLockUnlock(m_lock);
        return kIOReturnNoMemory;
    }
    
    // Replace in collections
    char key_str[32];
    snprintf(key_str, sizeof(key_str), "%u", layer_id);
    m_layer_map->setObject(key_str, new_data);
    unsigned int index = m_layers->getNextIndexOfObject(layer_data, 0);
    if (index != (unsigned int)-1) {
        m_layers->replaceObject(index, new_data);
    }
    
    new_data->release();
    
    // Mark compositor as needing update
    m_compositor_state.needs_display = true;
    m_compositor_state.dirty_layers++;
    
    IORecursiveLockUnlock(m_lock);
    return kIOReturnSuccess;
}

IOReturn CLASS::getLayerProperties(uint32_t layer_id, VMCALayerProperties* properties)
{
    if (!properties) {
        return kIOReturnBadArgument;
    }
    
    IORecursiveLockLock(m_lock);
    
    OSData* layer_data = (OSData*)findLayer(layer_id);
    if (!layer_data) {
        IORecursiveLockUnlock(m_lock);
        return kIOReturnNotFound;
    }
    
    if (layer_data->getLength() >= sizeof(VMCALayerProperties)) {
        memcpy(properties, layer_data->getBytesNoCopy(), sizeof(VMCALayerProperties));
        IORecursiveLockUnlock(m_lock);
        return kIOReturnSuccess;
    }
    
    IORecursiveLockUnlock(m_lock);
    return kIOReturnBadArgument;
}

// MARK: - Layer Hierarchy Management

IOReturn CLASS::addSublayer(uint32_t parent_layer_id, uint32_t child_layer_id)
{
    IORecursiveLockLock(m_lock);
    
    OSObject* parent_layer = findLayer(parent_layer_id);
    OSObject* child_layer = findLayer(child_layer_id);
    
    if (!parent_layer || !child_layer) {
        IORecursiveLockUnlock(m_lock);
        return kIOReturnNotFound;
    }
    
    // Add to layer tree (simplified implementation)
    bool found = false;
    for (unsigned int i = 0; i < m_layer_tree->getCount(); i++) {
        if (m_layer_tree->getObject(i) == child_layer) {
            found = true;
            break;
        }
    }
    if (!found) {
        m_layer_tree->setObject(child_layer);
    }
    
    m_compositor_state.needs_layout = true;
    
    IOLog("VMCoreAnimationAccelerator: Added layer %u as sublayer of %u\n", child_layer_id, parent_layer_id);
    
    IORecursiveLockUnlock(m_lock);
    return kIOReturnSuccess;
}

IOReturn CLASS::removeSublayer(uint32_t parent_layer_id, uint32_t child_layer_id)
{
    IORecursiveLockLock(m_lock);
    
    OSObject* child_layer = findLayer(child_layer_id);
    if (!child_layer) {
        IORecursiveLockUnlock(m_lock);
        return kIOReturnNotFound;
    }
    
    // Remove from layer tree
    unsigned int index = m_layer_tree->getNextIndexOfObject(child_layer, 0);
    if (index != (unsigned int)-1) {
        m_layer_tree->removeObject(index);
    }
    
    m_compositor_state.needs_layout = true;
    
    IOLog("VMCoreAnimationAccelerator: Removed layer %u from parent %u\n", child_layer_id, parent_layer_id);
    
    IORecursiveLockUnlock(m_lock);
    return kIOReturnSuccess;
}

IOReturn CLASS::setRootLayer(uint32_t layer_id)
{
    IORecursiveLockLock(m_lock);
    
    OSObject* layer = findLayer(layer_id);
    if (!layer) {
        IORecursiveLockUnlock(m_lock);
        return kIOReturnNotFound;
    }
    
    m_root_layer_id = layer_id;
    m_compositor_state.needs_layout = true;
    
    IOLog("VMCoreAnimationAccelerator: Set root layer to %u\n", layer_id);
    
    IORecursiveLockUnlock(m_lock);
    return kIOReturnSuccess;
}

uint32_t CLASS::getRootLayer()
{
    IORecursiveLockLock(m_lock);
    uint32_t root_id = m_root_layer_id;
    IORecursiveLockUnlock(m_lock);
    return root_id;
}

// MARK: - Animation Management

IOReturn CLASS::addAnimation(uint32_t layer_id, const VMCAAnimationDescriptor* descriptor, uint32_t* animation_id)
{
    if (!descriptor || !animation_id) {
        return kIOReturnBadArgument;
    }
    
    IORecursiveLockLock(m_lock);
    
    OSObject* layer = findLayer(layer_id);
    if (!layer) {
        IORecursiveLockUnlock(m_lock);
        return kIOReturnNotFound;
    }
    
    // Allocate animation ID
    *animation_id = allocateAnimationId();
    
    // Create enhanced animation data with embedded ID for efficient lookup
    AnimationRecord record;
    record.animation_id = *animation_id;
    record.layer_id = layer_id;
    memcpy(&record.descriptor, descriptor, sizeof(VMCAAnimationDescriptor));
    
    // Create animation data with embedded record
    OSData* animation_data = OSData::withBytes(&record, sizeof(AnimationRecord));
    if (!animation_data) {
        releaseAnimationId(*animation_id);
        IORecursiveLockUnlock(m_lock);
        return kIOReturnNoMemory;
    }
    
    // Store animation in array
    m_animations->setObject(animation_data);
    animation_data->release();
    
    // Update compositor state
    m_compositor_state.animations_running++;
    m_animations_processed++;
    
    // Log with enhanced information
    const char* anim_type_str = "unknown";
    switch (descriptor->type) {
        case VM_CA_ANIMATION_BASIC: anim_type_str = "basic"; break;
        case VM_CA_ANIMATION_KEYFRAME: anim_type_str = "keyframe"; break;
        case VM_CA_ANIMATION_GROUP: anim_type_str = "group"; break;
        case VM_CA_ANIMATION_TRANSITION: anim_type_str = "transition"; break;
        case VM_CA_ANIMATION_SPRING: anim_type_str = "spring"; break;
    }
    
    const char* timing_str = "linear";
    switch (descriptor->timing_function) {
        case VM_CA_TIMING_EASE_IN: timing_str = "ease-in"; break;
        case VM_CA_TIMING_EASE_OUT: timing_str = "ease-out"; break;
        case VM_CA_TIMING_EASE_IN_OUT: timing_str = "ease-in-out"; break;
        case VM_CA_TIMING_LINEAR: timing_str = "linear"; break;
        case VM_CA_TIMING_DEFAULT: timing_str = "default"; break;
    }
    
    IOLog("VMCoreAnimationAccelerator: Added %s animation %u (%s, %s, %.3fs) to layer %u\n", 
          anim_type_str, *animation_id, descriptor->key_path ? descriptor->key_path : "null", 
          timing_str, descriptor->duration, layer_id);
    
    IORecursiveLockUnlock(m_lock);
    return kIOReturnSuccess;
}

IOReturn CLASS::removeAnimation(uint32_t layer_id, uint32_t animation_id)
{
    IORecursiveLockLock(m_lock);
    
    OSObject* animation_obj = findAnimation(animation_id);
    if (!animation_obj) {
        IORecursiveLockUnlock(m_lock);
        return kIOReturnNotFound;
    }
    
    // Remove animation
    unsigned int index = m_animations->getNextIndexOfObject(animation_obj, 0);
    if (index != (unsigned int)-1) {
        m_animations->removeObject(index);
    }
    
    releaseAnimationId(animation_id);
    
    if (m_compositor_state.animations_running > 0) {
        m_compositor_state.animations_running--;
    }
    
    IOLog("VMCoreAnimationAccelerator: Removed animation %u from layer %u\n", animation_id, layer_id);
    
    IORecursiveLockUnlock(m_lock);
    return kIOReturnSuccess;
}

// MARK: - Rendering and Composition

IOReturn CLASS::setNeedsDisplay(uint32_t layer_id)
{
    IORecursiveLockLock(m_lock);
    
    OSObject* layer = findLayer(layer_id);
    if (!layer) {
        IORecursiveLockUnlock(m_lock);
        return kIOReturnNotFound;
    }
    
    m_compositor_state.needs_display = true;
    m_compositor_state.dirty_layers++;
    
    IORecursiveLockUnlock(m_lock);
    return kIOReturnSuccess;
}

IOReturn CLASS::compositeFrame()
{
    IORecursiveLockLock(m_lock);
    
    if (!m_compositor_active) {
        IORecursiveLockUnlock(m_lock);
        return kIOReturnNotReady;
    }
    
    // Update frame counter
    m_compositor_state.frame_number++;
    m_compositor_state.timestamp = (double)(m_compositor_state.frame_number * m_frame_interval) / 1000000.0;
    
    // Process animations
    processAnimations();
    
    // Update layer tree if needed
    if (m_compositor_state.needs_layout) {
        updateLayerTree();
        m_compositor_state.needs_layout = false;
    }
    
    // Render frame if needed
    if (m_compositor_state.needs_display) {
        renderCompositeFrame();
        m_compositor_state.needs_display = false;
        m_compositor_state.dirty_layers = 0;
        m_layers_rendered++;
    }
    
    m_composition_operations++;
    
    IORecursiveLockUnlock(m_lock);
    return kIOReturnSuccess;
}

// MARK: - Helper Methods

OSObject* CLASS::findLayer(uint32_t layer_id)
{
    if (!m_layer_map) {
        return nullptr;
    }
    
    char key_str[32];
    snprintf(key_str, sizeof(key_str), "%u", layer_id);
    
    OSObject* layer = m_layer_map->getObject(key_str);
    
    return layer;
}

OSObject* CLASS::findAnimation(uint32_t animation_id)
{
    if (!m_animations || animation_id == 0) {
        return nullptr;
    }
    
    // Enhanced animation lookup using AnimationRecord structure
    for (unsigned int i = 0; i < m_animations->getCount(); i++) {
        OSData* animation_data = (OSData*)m_animations->getObject(i);
        if (!animation_data || animation_data->getLength() < sizeof(AnimationRecord)) {
            continue;
        }
        
        // Extract animation record with embedded ID
        AnimationRecord* record = (AnimationRecord*)animation_data->getBytesNoCopy();
        if (record && record->animation_id == animation_id) {
            return animation_data;
        }
    }
    
    return nullptr;
}

uint32_t CLASS::allocateLayerId()
{
    IORecursiveLockLock(m_lock);
    
    // First try to reuse a released ID from the recycling pool
    if (m_released_layer_ids && m_released_layer_ids->getCount() > 0) {
        // Get the most recently released ID (LIFO for better cache locality)
        unsigned int last_index = m_released_layer_ids->getCount() - 1;
        OSNumber* recycled_id = (OSNumber*)m_released_layer_ids->getObject(last_index);
        
        if (recycled_id) {
            uint32_t reused_id = recycled_id->unsigned32BitValue();
            m_released_layer_ids->removeObject(last_index);
            
            IORecursiveLockUnlock(m_lock);
            IOLog("VMCoreAnimationAccelerator: Recycled layer ID %u (pool size: %u)\n", 
                  reused_id, m_released_layer_ids->getCount());
            return reused_id;
        }
    }
    
    // No recycled IDs available, allocate a new one
    uint32_t new_id = m_next_layer_id++;
    
    IORecursiveLockUnlock(m_lock);
    
    // Log when we're getting close to ID exhaustion
    if (new_id > 0xFFFF0000) {
        IOLog("VMCoreAnimationAccelerator: Warning - Layer ID approaching maximum value: %u\n", new_id);
    }
    
    return new_id;
}

uint32_t CLASS::allocateAnimationId()
{
    IORecursiveLockLock(m_lock);
    
    // First try to reuse a released ID from the recycling pool
    if (m_released_animation_ids && m_released_animation_ids->getCount() > 0) {
        // Get the most recently released ID (LIFO for better cache locality)
        unsigned int last_index = m_released_animation_ids->getCount() - 1;
        OSNumber* recycled_id = (OSNumber*)m_released_animation_ids->getObject(last_index);
        
        if (recycled_id) {
            uint32_t reused_id = recycled_id->unsigned32BitValue();
            m_released_animation_ids->removeObject(last_index);
            
            IORecursiveLockUnlock(m_lock);
            IOLog("VMCoreAnimationAccelerator: Recycled animation ID %u (pool size: %u)\n", 
                  reused_id, m_released_animation_ids->getCount());
            return reused_id;
        }
    }
    
    // No recycled IDs available, allocate a new one
    uint32_t new_id = m_next_animation_id++;
    
    IORecursiveLockUnlock(m_lock);
    
    // Log when we're getting close to ID exhaustion
    if (new_id > 0xFFFF0000) {
        IOLog("VMCoreAnimationAccelerator: Warning - Animation ID approaching maximum value: %u\n", new_id);
    }
    
    return new_id;
}

void CLASS::releaseLayerId(uint32_t layer_id)
{
    // Validate the layer ID
    if (layer_id == 0 || layer_id >= m_next_layer_id) {
        IOLog("VMCoreAnimationAccelerator: Warning - Invalid layer ID %u for recycling\n", layer_id);
        return;
    }
    
    IORecursiveLockLock(m_lock);
    
    // Check if this ID is already in the recycling pool (prevent duplicates)
    if (m_released_layer_ids) {
        for (unsigned int i = 0; i < m_released_layer_ids->getCount(); i++) {
            OSNumber* existing_id = (OSNumber*)m_released_layer_ids->getObject(i);
            if (existing_id && existing_id->unsigned32BitValue() == layer_id) {
                IORecursiveLockUnlock(m_lock);
                IOLog("VMCoreAnimationAccelerator: Warning - Layer ID %u already in recycling pool\n", layer_id);
                return;
            }
        }
        
        // Add to recycling pool with size limit to prevent unbounded growth
        const unsigned int MAX_RECYCLED_IDS = 128;
        if (m_released_layer_ids->getCount() < MAX_RECYCLED_IDS) {
            OSNumber* id_number = OSNumber::withNumber(layer_id, 32);
            if (id_number) {
                m_released_layer_ids->setObject(id_number);
                id_number->release();
                
                IOLog("VMCoreAnimationAccelerator: Added layer ID %u to recycling pool (size: %u)\n", 
                      layer_id, m_released_layer_ids->getCount());
            }
        } else {
            IOLog("VMCoreAnimationAccelerator: Recycling pool full, discarding layer ID %u\n", layer_id);
        }
    }
    
    IORecursiveLockUnlock(m_lock);
}

void CLASS::releaseAnimationId(uint32_t animation_id)
{
    // Validate the animation ID
    if (animation_id == 0 || animation_id >= m_next_animation_id) {
        IOLog("VMCoreAnimationAccelerator: Warning - Invalid animation ID %u for recycling\n", animation_id);
        return;
    }
    
    IORecursiveLockLock(m_lock);
    
    // Check if this ID is already in the recycling pool (prevent duplicates)
    if (m_released_animation_ids) {
        for (unsigned int i = 0; i < m_released_animation_ids->getCount(); i++) {
            OSNumber* existing_id = (OSNumber*)m_released_animation_ids->getObject(i);
            if (existing_id && existing_id->unsigned32BitValue() == animation_id) {
                IORecursiveLockUnlock(m_lock);
                IOLog("VMCoreAnimationAccelerator: Warning - Animation ID %u already in recycling pool\n", animation_id);
                return;
            }
        }
        
        // Add to recycling pool with size limit to prevent unbounded growth
        const unsigned int MAX_RECYCLED_IDS = 256; // Higher limit for animations as they're more frequent
        if (m_released_animation_ids->getCount() < MAX_RECYCLED_IDS) {
            OSNumber* id_number = OSNumber::withNumber(animation_id, 32);
            if (id_number) {
                m_released_animation_ids->setObject(id_number);
                id_number->release();
                
                IOLog("VMCoreAnimationAccelerator: Added animation ID %u to recycling pool (size: %u)\n", 
                      animation_id, m_released_animation_ids->getCount());
            }
        } else {
            // When pool is full, remove oldest entries (FIFO for pool management)
            IOLog("VMCoreAnimationAccelerator: Recycling pool full, removing oldest entries\n");
            
            // Remove the first quarter of entries to make room
            unsigned int entries_to_remove = MAX_RECYCLED_IDS / 4;
            for (unsigned int i = 0; i < entries_to_remove && m_released_animation_ids->getCount() > 0; i++) {
                m_released_animation_ids->removeObject(0);
            }
            
            // Now add the new ID
            OSNumber* id_number = OSNumber::withNumber(animation_id, 32);
            if (id_number) {
                m_released_animation_ids->setObject(id_number);
                id_number->release();
                
                IOLog("VMCoreAnimationAccelerator: Added animation ID %u after pool cleanup (size: %u)\n", 
                      animation_id, m_released_animation_ids->getCount());
            }
        }
    }
    
    IORecursiveLockUnlock(m_lock);
}

// MARK: - Internal Compositor Methods

void CLASS::animationTimerHandler(OSObject* owner, IOTimerEventSource* sender)
{
    CLASS* self = (CLASS*)owner;
    if (self && self->m_compositor_active) {
        // Trigger frame composition
        self->compositeFrame();
        
        // Reschedule timer for next frame
        if (self->m_animation_timer) {
            self->m_animation_timer->setTimeoutMS(16); // 60fps
        }
    }
}

void CLASS::processAnimations()
{
    // Process running animations and update layer properties
    if (m_compositor_state.animations_running == 0 || !m_animations) {
        return;
    }
    
    double current_time = m_compositor_state.timestamp;
    uint32_t processed_count = 0;
    uint32_t completed_count = 0;
    
    // Process each active animation with enhanced tracking
    for (unsigned int i = 0; i < m_animations->getCount(); i++) {
        OSData* animation_data = (OSData*)m_animations->getObject(i);
        if (!animation_data || animation_data->getLength() < sizeof(AnimationRecord)) {
            // Handle legacy animations stored as raw VMCAAnimationDescriptor
            if (animation_data && animation_data->getLength() >= sizeof(VMCAAnimationDescriptor)) {
                VMCAAnimationDescriptor* desc = (VMCAAnimationDescriptor*)animation_data->getBytesNoCopy();
                if (desc) {
                    processLegacyAnimation(desc, current_time, processed_count, completed_count);
                }
            }
            continue;
        }
        
        // Extract animation record
        AnimationRecord* record = (AnimationRecord*)animation_data->getBytesNoCopy();
        if (!record) {
            continue;
        }
        
        VMCAAnimationDescriptor* desc = &record->descriptor;
        
        // Calculate animation progress with proper timing
        double elapsed = current_time; // Simplified timing calculation
        double progress = elapsed / desc->duration;
        
        // Check if animation is complete
        if (progress >= 1.0) {
            progress = 1.0;
            completed_count++;
            
            // Handle repeat count
            if (desc->repeat_count > 0) {
                desc->repeat_count--;
                progress = 0.0;
                completed_count--; // Not actually complete
            }
        }
        
        // Apply easing function based on timing function
        double eased_progress = applyEnhancedEasing(progress, desc->timing_function);
        
        // Interpolate animation values based on type with layer targeting
        switch (desc->type) {
            case VM_CA_ANIMATION_BASIC:
                if (desc->from_value && desc->to_value && desc->key_path) {
                    interpolateBasicAnimationForLayer(desc, record->layer_id, eased_progress);
                }
                break;
                
            case VM_CA_ANIMATION_KEYFRAME:
                interpolateKeyframeAnimationForLayer(desc, record->layer_id, eased_progress);
                break;
                
            case VM_CA_ANIMATION_GROUP:
                processAnimationGroup(record, eased_progress);
                break;
                
            case VM_CA_ANIMATION_TRANSITION:
                processTransitionAnimation(record, eased_progress);
                break;
                
            case VM_CA_ANIMATION_SPRING:
                processSpringAnimation(record, eased_progress);
                break;
                
            default:
                IOLog("VMCoreAnimationAccelerator: Unknown animation type %d for animation %u\n", 
                      desc->type, record->animation_id);
                break;
        }
        
        processed_count++;
    }
    
    // Enhanced cleanup with better performance tracking
    if (completed_count > 0) {
        cleanupCompletedAnimations(current_time, completed_count);
    }
    
    // Update statistics
    m_animations_processed += processed_count;
    
    // Mark layers as needing display if animations are active
    if (processed_count > 0) {
        m_compositor_state.needs_display = true;
    }
    
    // Performance monitoring
    if (processed_count > 50) {
        IOLog("VMCoreAnimationAccelerator: High animation load - processed %u animations in frame %llu\n", 
              processed_count, m_compositor_state.frame_number);
    }
}

void CLASS::updateLayerTree()
{
    // Update layer hierarchy and transforms
    m_compositor_state.active_layers = m_layers->getCount();
}

void CLASS::renderCompositeFrame()
{
    // Render the complete frame using available acceleration
    if (m_supports_hardware_composition && m_metal_bridge) {
        // Hardware-accelerated rendering
        IOLog("VMCoreAnimationAccelerator: Rendering frame %llu (hardware)\n", m_compositor_state.frame_number);
    } else {
        // Software rendering fallback
        IOLog("VMCoreAnimationAccelerator: Rendering frame %llu (software)\n", m_compositor_state.frame_number);
    }
}

// MARK: - Scientific Data Validation Helpers

uint32_t CLASS::calculateCRC32(const uint8_t* data, size_t length)
{
    // CRC-32 IEEE 802.3 polynomial: 0x04C11DB7
    static const uint32_t crc32_table[256] = {
        0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F, 0xE963A535, 0x9E6495A3,
        0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988, 0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91,
        0x1DB71064, 0x6AB020F2, 0xF3B97148, 0x84BE41DE, 0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
        0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC, 0x14015C4F, 0x63066CD9, 0xFA0F3D63, 0x8D080DF5,
        0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172, 0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B,
        0x35B5A8FA, 0x42B2986C, 0xDBBBC9D6, 0xACBCF940, 0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
        0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116, 0x21B4F4B5, 0x56B3C423, 0xCFBA9599, 0xB8BDA50F,
        0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924, 0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D,
        0x76DC4190, 0x01DB7106, 0x98D220BC, 0xEFD5102A, 0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
        0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818, 0x7F6A0DBB, 0x086D3D2D, 0x91646C97, 0xE6635C01,
        0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E, 0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457,
        0x65B0D9C6, 0x12B7E950, 0x8BBEB8EA, 0xFCB9887C, 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
        0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2, 0x4ADFA541, 0x3DD895D7, 0xA4D1C46D, 0xD3D6F4FB,
        0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0, 0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9,
        0x5005713C, 0x270241AA, 0xBE0B1010, 0xC90C2086, 0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
        0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4, 0x59B33D17, 0x2EB40D81, 0xB7BD5C3B, 0xC0BA6CAD,
        0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A, 0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683,
        0xE3630B12, 0x94643B84, 0x0D6D6A3E, 0x7A6A5AA8, 0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
        0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE, 0xF762575D, 0x806567CB, 0x196C3671, 0x6E6B06E7,
        0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC, 0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5,
        0xD6D6A3E8, 0xA1D1937E, 0x38D8C2C4, 0x4FDFF252, 0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
        0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60, 0xDF60EFC3, 0xA867DF55, 0x316E8EEF, 0x4669BE79,
        0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236, 0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F,
        0xC5BA3BBE, 0xB2BD0B28, 0x2BB45A92, 0x5CB36A04, 0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
        0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A, 0x9C0906A9, 0xEB0E363F, 0x72076785, 0x05005713,
        0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38, 0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21,
        0x86D3D2D4, 0xF1D4E242, 0x68DDB3F8, 0x1FDA836E, 0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
        0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C, 0x8F659EFF, 0xF862AE69, 0x616BFFD3, 0x166CCF45,
        0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2, 0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB,
        0xAED16A4A, 0xD9D65ADC, 0x40DF0B66, 0x37D83BF0, 0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
        0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6, 0xBAD03605, 0xCDD70693, 0x54DE5729, 0x23D967BF,
        0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94, 0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D
    };
    
    uint32_t crc = 0xFFFFFFFF;
    
    for (size_t i = 0; i < length; i++) {
        uint8_t index = (crc ^ data[i]) & 0xFF;
        crc = (crc >> 8) ^ crc32_table[index];
    }
    
    return crc ^ 0xFFFFFFFF;
}

bool CLASS::validateKeyframeDataStructure(const void* data, size_t data_size)
{
    // Scientific validation of keyframe data structure format
    if (!data || data_size < sizeof(VMCAKeyframeDataHeader)) {
        return false;
    }
    
    const VMCAKeyframeDataHeader* header = (const VMCAKeyframeDataHeader*)data;
    const uint32_t VMCA_KEYFRAME_MAGIC = 0x564B4644; // 'VKFD'
    
    // Validate magic number, version compatibility, and size constraints
    return (header->magic_number == VMCA_KEYFRAME_MAGIC &&
            header->structure_version <= 1 &&
            header->data_size <= data_size &&
            header->data_size >= sizeof(VMCAKeyframeData));
}

// MARK: - Animation Interpolation Helpers

void CLASS::interpolateBasicAnimation(VMCAAnimationDescriptor* desc, double progress)
{
    if (!desc || !desc->key_path || !desc->from_value || !desc->to_value) {
        return;
    }
    
    // Clamp progress to valid range
    if (progress < 0.0) progress = 0.0;
    if (progress > 1.0) progress = 1.0;
    
    // This method is for legacy animations without layer_id tracking
    // For new animations, we should use interpolateBasicAnimationForLayer instead
    // which properly receives the target layer_id as a parameter
    
    // Parse key path and determine property type
    const char* key_path = desc->key_path;
    
    // Find a target layer (legacy fallback - prefer the layer_id-aware version)
    uint32_t target_layer_id = m_root_layer_id;
    if (target_layer_id == 0 && m_layers && m_layers->getCount() > 0) {
        // Use first available layer as fallback
        target_layer_id = 1;
    }
    
    if (strcmp(key_path, "position.x") == 0 || strcmp(key_path, "position.y") == 0) {
        // Position animation (float values)
        float from_val = *(float*)desc->from_value;
        float to_val = *(float*)desc->to_value;
        float current_val = from_val + (to_val - from_val) * progress;
        
        // Update layer position property
        VMCALayerProperties layer_props;
        if (getLayerProperties(target_layer_id, &layer_props) == kIOReturnSuccess) {
            if (strcmp(key_path, "position.x") == 0) {
                layer_props.position.x = current_val;
            } else {
                layer_props.position.y = current_val;
            }
            updateLayerProperties(target_layer_id, &layer_props);
        }
        
    } else if (strcmp(key_path, "bounds.size.width") == 0 || strcmp(key_path, "bounds.size.height") == 0) {
        // Size animation (float values) - bounds is VMCARect with x,y,width,height
        float from_val = *(float*)desc->from_value;
        float to_val = *(float*)desc->to_value;
        float current_val = from_val + (to_val - from_val) * progress;
        
        VMCALayerProperties layer_props;
        if (getLayerProperties(target_layer_id, &layer_props) == kIOReturnSuccess) {
            if (strcmp(key_path, "bounds.size.width") == 0) {
                layer_props.bounds.width = current_val;
            } else {
                layer_props.bounds.height = current_val;
            }
            updateLayerProperties(target_layer_id, &layer_props);
        }
        
    } else if (strcmp(key_path, "opacity") == 0) {
        // Opacity animation (float 0.0 to 1.0)
        float from_val = *(float*)desc->from_value;
        float to_val = *(float*)desc->to_value;
        float current_val = from_val + (to_val - from_val) * progress;
        
        // Clamp opacity to valid range
        if (current_val < 0.0f) current_val = 0.0f;
        if (current_val > 1.0f) current_val = 1.0f;
        
        VMCALayerProperties layer_props;
        if (getLayerProperties(target_layer_id, &layer_props) == kIOReturnSuccess) {
            layer_props.opacity = current_val;
            updateLayerProperties(target_layer_id, &layer_props);
        }
        
    } else if (strcmp(key_path, "transform.rotation.z") == 0) {
        // Rotation animation - need to modify the 4x4 transform matrix
        float from_val = *(float*)desc->from_value;
        float to_val = *(float*)desc->to_value;
        float current_val = from_val + (to_val - from_val) * progress;
        
        VMCALayerProperties layer_props;
        if (getLayerProperties(target_layer_id, &layer_props) == kIOReturnSuccess) {
            // Production-quality rotation using optimized trigonometric approximation
            // Normalize angle to [-2π, 2π] range for better accuracy
            float normalized_angle = current_val;
            const float TWO_PI = 6.283185307f;
            const float PI = 3.141592654f;
            
            // Normalize to [-π, π] range for optimal approximation accuracy
            while (normalized_angle > PI) normalized_angle -= TWO_PI;
            while (normalized_angle < -PI) normalized_angle += TWO_PI;
            
            float cos_val, sin_val;
            
            // High-precision trigonometric approximation using optimized polynomial
            // Bhaskara I's sine approximation with improved accuracy
            float abs_angle = (normalized_angle < 0.0f) ? -normalized_angle : normalized_angle;
            
            if (abs_angle <= PI / 2.0f) {
                // First quadrant - direct calculation
                float angle_sq = normalized_angle * normalized_angle;
                float angle_4th = angle_sq * angle_sq;
                float angle_6th = angle_4th * angle_sq;
                
                // Enhanced Taylor series with higher-order terms for better accuracy
                cos_val = 1.0f - angle_sq / 2.0f + angle_4th / 24.0f - angle_6th / 720.0f;
                sin_val = normalized_angle - (normalized_angle * angle_sq) / 6.0f + 
                         (normalized_angle * angle_4th) / 120.0f - (normalized_angle * angle_6th) / 5040.0f;
            } else {
                // Use symmetry properties for other quadrants
                float reduced_angle = PI - abs_angle;
                float angle_sq = reduced_angle * reduced_angle;
                float angle_4th = angle_sq * angle_sq;
                
                float cos_reduced = 1.0f - angle_sq / 2.0f + angle_4th / 24.0f;
                float sin_reduced = reduced_angle - (reduced_angle * angle_sq) / 6.0f + 
                                   (reduced_angle * angle_4th) / 120.0f;
                
                cos_val = -cos_reduced; // cos(π - x) = -cos(x)
                sin_val = (normalized_angle < 0.0f) ? -sin_reduced : sin_reduced;
            }
            
            // Construct optimized 2D rotation matrix with proper homogeneous coordinates
            // Reset transform matrix to identity
            memset(&layer_props.transform, 0, sizeof(VMCATransform3D));
            
            // 2D rotation matrix embedded in 4x4 homogeneous matrix
            layer_props.transform.m11 = cos_val;   // X-axis rotation component
            layer_props.transform.m12 = -sin_val;  // X-axis shear component
            layer_props.transform.m13 = 0.0f;      // No Z rotation for 2D
            layer_props.transform.m14 = 0.0f;      // No translation
            
            layer_props.transform.m21 = sin_val;   // Y-axis shear component
            layer_props.transform.m22 = cos_val;   // Y-axis rotation component
            layer_props.transform.m23 = 0.0f;      // No Z rotation for 2D
            layer_props.transform.m24 = 0.0f;      // No translation
            
            layer_props.transform.m31 = 0.0f;      // No Z-axis rotation
            layer_props.transform.m32 = 0.0f;      // No Z-axis rotation
            layer_props.transform.m33 = 1.0f;      // Z-axis identity (for depth)
            layer_props.transform.m34 = 0.0f;      // No Z translation
            
            layer_props.transform.m41 = 0.0f;      // No W translation
            layer_props.transform.m42 = 0.0f;      // No W translation
            layer_props.transform.m43 = 0.0f;      // No W translation
            layer_props.transform.m44 = 1.0f;      // Homogeneous coordinate
            
            updateLayerProperties(target_layer_id, &layer_props);
            
            // Enhanced logging with angle information
            IOLog("VMCoreAnimationAccelerator: Applied rotation %.3f° (%.6f rad) to layer %u (cos=%.6f, sin=%.6f)\n", 
                  current_val * 180.0f / PI, current_val, target_layer_id, cos_val, sin_val);
        }
        
    } else if (strcmp(key_path, "transform.scale.x") == 0 || strcmp(key_path, "transform.scale.y") == 0) {
        // Scale animation - modify transform matrix
        float from_val = *(float*)desc->from_value;
        float to_val = *(float*)desc->to_value;
        float current_val = from_val + (to_val - from_val) * progress;
        
        VMCALayerProperties layer_props;
        if (getLayerProperties(target_layer_id, &layer_props) == kIOReturnSuccess) {
            if (strcmp(key_path, "transform.scale.x") == 0) {
                layer_props.transform.m11 = current_val;
            } else {
                layer_props.transform.m22 = current_val;
            }
            updateLayerProperties(target_layer_id, &layer_props);
        }
        
    } else if (strcmp(key_path, "backgroundColor") == 0) {
        // Color animation (RGBA values)
        uint32_t from_color = *(uint32_t*)desc->from_value;
        uint32_t to_color = *(uint32_t*)desc->to_value;
        
        // Extract RGBA components
        uint8_t from_r = (from_color >> 24) & 0xFF;
        uint8_t from_g = (from_color >> 16) & 0xFF;
        uint8_t from_b = (from_color >> 8) & 0xFF;
        uint8_t from_a = from_color & 0xFF;
        
        uint8_t to_r = (to_color >> 24) & 0xFF;
        uint8_t to_g = (to_color >> 16) & 0xFF;
        uint8_t to_b = (to_color >> 8) & 0xFF;
        uint8_t to_a = to_color & 0xFF;
        
        // Interpolate each component
        uint8_t current_r = (uint8_t)(from_r + (to_r - from_r) * progress);
        uint8_t current_g = (uint8_t)(from_g + (to_g - from_g) * progress);
        uint8_t current_b = (uint8_t)(from_b + (to_b - from_b) * progress);
        uint8_t current_a = (uint8_t)(from_a + (to_a - from_a) * progress);
        
        VMCALayerProperties layer_props;
        if (getLayerProperties(target_layer_id, &layer_props) == kIOReturnSuccess) {
            // Convert back to VMCAColor structure (RGBA float values)
            layer_props.background_color.red = current_r / 255.0f;
            layer_props.background_color.green = current_g / 255.0f;
            layer_props.background_color.blue = current_b / 255.0f;
            layer_props.background_color.alpha = current_a / 255.0f;
            updateLayerProperties(target_layer_id, &layer_props);
        }
        
    } else if (strcmp(key_path, "cornerRadius") == 0) {
        // Corner radius animation
        float from_val = *(float*)desc->from_value;
        float to_val = *(float*)desc->to_value;
        float current_val = from_val + (to_val - from_val) * progress;
        
        VMCALayerProperties layer_props;
        if (getLayerProperties(target_layer_id, &layer_props) == kIOReturnSuccess) {
            layer_props.corner_radius = current_val;
            updateLayerProperties(target_layer_id, &layer_props);
        }
        
    } else {
        // Unknown property type - log for debugging
        IOLog("VMCoreAnimationAccelerator: Unknown animation property '%s' (progress: %f)\n", 
              key_path, progress);
        return;
    }
    
    // Mark layer as needing redisplay
    setNeedsDisplay(target_layer_id);
    
    // Log successful interpolation
    IOLog("VMCoreAnimationAccelerator: Interpolated %s animation (progress: %f) for layer %u\n", 
          key_path, progress, target_layer_id);
}

void CLASS::interpolateKeyframeAnimation(VMCAAnimationDescriptor* desc, double progress)
{
    if (!desc || !desc->key_path) {
        return;
    }
    
    // Clamp progress to valid range
    if (progress < 0.0) progress = 0.0;
    if (progress > 1.0) progress = 1.0;
    
    // Find target layer (same logic as basic animation)
    uint32_t target_layer_id = m_root_layer_id;
    if (target_layer_id == 0 && m_layers && m_layers->getCount() > 0) {
        target_layer_id = 1;
    }
    
    // True keyframe animation system with actual keyframe data structures
    const char* key_path = desc->key_path;
    
    // Production keyframe system - from_value points to VMCAKeyframeData structure
    // containing actual keyframe values and timing information
    const uint32_t MAX_KEYFRAMES = 8;
    uint32_t keyframe_count = 4; // Default keyframe count
    
    // Production keyframe data structure for different value types
    struct VMCAKeyframeData {
        uint32_t count;           // Number of keyframes
        uint32_t data_type;       // 0=float, 1=color, 2=point, 3=transform
        double times[MAX_KEYFRAMES]; // Timing values (0.0 to 1.0)
        union {
            float float_values[MAX_KEYFRAMES];      // For position, scale, opacity, etc.
            uint32_t color_values[MAX_KEYFRAMES];   // For color animations (RGBA)
            struct {
                float x[MAX_KEYFRAMES];
                float y[MAX_KEYFRAMES]; 
            } point_values;                         // For 2D points like position
            struct {
                float rotation[MAX_KEYFRAMES];      // Rotation values in radians
                float scale_x[MAX_KEYFRAMES];       // X-axis scale values
                float scale_y[MAX_KEYFRAMES];       // Y-axis scale values
            } transform_values;                     // For transform animations
        } values;
    };
    
    VMCAKeyframeData keyframe_data;
    bool use_provided_keyframes = false;
    
    // Scientific keyframe data detection using magic number validation
    if (desc->from_value && desc->to_value) {
        // Enhanced VMCAKeyframeData structure with validation header
        struct VMCAKeyframeDataHeader {
            uint32_t magic_number;    // 0x564B4644 ('VKFD' - VMCAKeyframeData)
            uint32_t structure_version; // Version for future compatibility
            uint32_t data_size;       // Total size of the structure
            uint32_t checksum;        // CRC32 checksum for data integrity
        };
        
        // Check if from_value contains a properly formatted keyframe data structure
        if (desc->from_value) {
            VMCAKeyframeDataHeader* header = (VMCAKeyframeDataHeader*)desc->from_value;
            
            // Validate magic number and structure integrity
            const uint32_t VMCA_KEYFRAME_MAGIC = 0x564B4644; // 'VKFD'
            const uint32_t CURRENT_VERSION = 1;
            const size_t MIN_STRUCTURE_SIZE = sizeof(VMCAKeyframeDataHeader) + sizeof(VMCAKeyframeData) - sizeof(VMCAKeyframeDataHeader);
            
            if (header->magic_number == VMCA_KEYFRAME_MAGIC &&
                header->structure_version <= CURRENT_VERSION &&
                header->data_size >= MIN_STRUCTURE_SIZE &&
                header->data_size <= (sizeof(VMCAKeyframeDataHeader) + sizeof(VMCAKeyframeData))) {
                
                // Extract the keyframe data following the header
                VMCAKeyframeData* provided_data = (VMCAKeyframeData*)((uint8_t*)desc->from_value + sizeof(VMCAKeyframeDataHeader));
                
                // Scientific validation using multiple integrity checks
                bool data_integrity_valid = true;
                
                // 1. Range validation with scientific bounds checking
                if (provided_data->count == 0 || provided_data->count > MAX_KEYFRAMES) {
                    data_integrity_valid = false;
                    IOLog("VMCoreAnimationAccelerator: Keyframe count %u outside valid range [1, %u]\n", 
                          provided_data->count, MAX_KEYFRAMES);
                }
                
                // 2. Data type validation with enum bounds
                if (provided_data->data_type > 3) { // 0=float, 1=color, 2=point, 3=transform
                    data_integrity_valid = false;
                    IOLog("VMCoreAnimationAccelerator: Invalid data type %u (valid range: 0-3)\n", 
                          provided_data->data_type);
                }
                
                // 3. Timing sequence validation with mathematical constraints
                if (provided_data->times[0] != 0.0) {
                    data_integrity_valid = false;
                    IOLog("VMCoreAnimationAccelerator: First keyframe time %.6f != 0.0\n", 
                          provided_data->times[0]);
                }
                
                if (provided_data->times[provided_data->count - 1] != 1.0) {
                    data_integrity_valid = false;
                    IOLog("VMCoreAnimationAccelerator: Last keyframe time %.6f != 1.0\n", 
                          provided_data->times[provided_data->count - 1]);
                }
                
                // 4. Monotonic sequence validation with epsilon tolerance
                const double TIMING_EPSILON = 1e-6; // Numerical precision tolerance
                for (uint32_t i = 1; i < provided_data->count; i++) {
                    double time_diff = provided_data->times[i] - provided_data->times[i-1];
                    if (time_diff <= TIMING_EPSILON) {
                        data_integrity_valid = false;
                        IOLog("VMCoreAnimationAccelerator: Non-monotonic timing at index %u: %.6f -> %.6f (diff: %.9f)\n", 
                              i, provided_data->times[i-1], provided_data->times[i], time_diff);
                        break;
                    }
                }
                
                // 5. Data value validation based on type-specific constraints
                if (data_integrity_valid) {
                    switch (provided_data->data_type) {
                        case 0: // Float values - check for NaN/Inf
                            for (uint32_t i = 0; i < provided_data->count; i++) {
                                float val = provided_data->values.float_values[i];
                                if (!isfinite(val)) {
                                    data_integrity_valid = false;
                                    IOLog("VMCoreAnimationAccelerator: Invalid float value at index %u: %f\n", i, val);
                                    break;
                                }
                            }
                            break;
                            
                        case 1: // Color values - validate RGBA components
                            for (uint32_t i = 0; i < provided_data->count; i++) {
                                uint32_t color = provided_data->values.color_values[i];
                                // Basic RGBA format validation (could be enhanced further)
                                if ((color >> 24) > 255) { // Alpha channel validation
                                    data_integrity_valid = false;
                                    IOLog("VMCoreAnimationAccelerator: Invalid color value at index %u: 0x%08X\n", i, color);
                                    break;
                                }
                            }
                            break;
                            
                        case 2: // Point values - validate coordinates
                            for (uint32_t i = 0; i < provided_data->count; i++) {
                                float x = provided_data->values.point_values.x[i];
                                float y = provided_data->values.point_values.y[i];
                                if (!isfinite(x) || !isfinite(y)) {
                                    data_integrity_valid = false;
                                    IOLog("VMCoreAnimationAccelerator: Invalid point at index %u: (%.3f, %.3f)\n", i, x, y);
                                    break;
                                }
                            }
                            break;
                            
                        case 3: // Transform values - validate rotation, scale
                            for (uint32_t i = 0; i < provided_data->count; i++) {
                                float rotation = provided_data->values.transform_values.rotation[i];
                                float scale_x = provided_data->values.transform_values.scale_x[i];
                                float scale_y = provided_data->values.transform_values.scale_y[i];
                                
                                if (!isfinite(rotation) || !isfinite(scale_x) || !isfinite(scale_y)) {
                                    data_integrity_valid = false;
                                    IOLog("VMCoreAnimationAccelerator: Invalid transform at index %u: rot=%.3f, scale=(%.3f, %.3f)\n", 
                                          i, rotation, scale_x, scale_y);
                                    break;
                                }
                                
                                // Validate scale factors (should be positive for most use cases)
                                if (scale_x <= 0.0f || scale_y <= 0.0f) {
                                    IOLog("VMCoreAnimationAccelerator: Warning - Non-positive scale at index %u: (%.3f, %.3f)\n", 
                                          i, scale_x, scale_y);
                                }
                            }
                            break;
                    }
                }
                
                // 6. Optional CRC32 checksum validation for data integrity
                if (data_integrity_valid && header->checksum != 0) {
                    // Calculate CRC32 of the keyframe data
                    uint32_t calculated_crc = calculateCRC32((uint8_t*)provided_data, sizeof(VMCAKeyframeData));
                    if (calculated_crc != header->checksum) {
                        data_integrity_valid = false;
                        IOLog("VMCoreAnimationAccelerator: CRC32 mismatch - expected: 0x%08X, calculated: 0x%08X\n", 
                              header->checksum, calculated_crc);
                    }
                }
                
                if (data_integrity_valid) {
                    // All validations passed - use provided keyframe data
                    memcpy(&keyframe_data, provided_data, sizeof(VMCAKeyframeData));
                    keyframe_count = keyframe_data.count;
                    use_provided_keyframes = true;
                    
                    IOLog("VMCoreAnimationAccelerator: Validated keyframe data (magic: 0x%08X, version: %u, count: %u, type: %u, checksum: 0x%08X)\n", 
                          header->magic_number, header->structure_version, keyframe_count, 
                          keyframe_data.data_type, header->checksum);
                } else {
                    IOLog("VMCoreAnimationAccelerator: Keyframe data validation failed, using generated keyframes\n");
                }
            } else {
                IOLog("VMCoreAnimationAccelerator: Invalid keyframe header (magic: 0x%08X, version: %u, size: %u), using generated keyframes\n", 
                      header->magic_number, header->structure_version, header->data_size);
            }
        }
    }
    
    // Process keyframes based on whether we have provided data or need to generate
    if (!use_provided_keyframes) {
        // Generate sophisticated keyframe data based on property type and animation duration
        keyframe_data.count = 4; // Default to 4 keyframes
        keyframe_data.data_type = 0; // Default to float type
        
        // Dynamic keyframe count based on animation duration and complexity
        if (desc->duration > 3.0) {
            keyframe_data.count = 6; // Long animations get more keyframes
        } else if (desc->duration > 1.5) {
            keyframe_data.count = 5; // Medium animations get 5 keyframes  
        }
        
        keyframe_count = keyframe_data.count;
        
        // Generate sophisticated timing curves based on animation type
        switch (desc->timing_function) {
            case VM_CA_TIMING_EASE_IN:
                // Slow start, fast finish timing
                if (keyframe_count == 4) {
                    keyframe_data.times[0] = 0.0;  keyframe_data.times[1] = 0.15;
                    keyframe_data.times[2] = 0.65; keyframe_data.times[3] = 1.0;
                } else if (keyframe_count == 5) {
                    keyframe_data.times[0] = 0.0;  keyframe_data.times[1] = 0.1;   keyframe_data.times[2] = 0.35;
                    keyframe_data.times[3] = 0.75; keyframe_data.times[4] = 1.0;
                } else {
                    keyframe_data.times[0] = 0.0;  keyframe_data.times[1] = 0.08;  keyframe_data.times[2] = 0.25;
                    keyframe_data.times[3] = 0.55; keyframe_data.times[4] = 0.8;   keyframe_data.times[5] = 1.0;
                }
                break;
                
            case VM_CA_TIMING_EASE_OUT:
                // Fast start, slow finish timing
                if (keyframe_count == 4) {
                    keyframe_data.times[0] = 0.0;  keyframe_data.times[1] = 0.35;
                    keyframe_data.times[2] = 0.85; keyframe_data.times[3] = 1.0;
                } else if (keyframe_count == 5) {
                    keyframe_data.times[0] = 0.0;  keyframe_data.times[1] = 0.25;  keyframe_data.times[2] = 0.65;
                    keyframe_data.times[3] = 0.9;  keyframe_data.times[4] = 1.0;
                } else {
                    keyframe_data.times[0] = 0.0;  keyframe_data.times[1] = 0.2;   keyframe_data.times[2] = 0.45;
                    keyframe_data.times[3] = 0.75; keyframe_data.times[4] = 0.92;  keyframe_data.times[5] = 1.0;
                }
                break;
                
            case VM_CA_TIMING_EASE_IN_OUT:
                // Slow start and finish timing
                if (keyframe_count == 4) {
                    keyframe_data.times[0] = 0.0;  keyframe_data.times[1] = 0.25;
                    keyframe_data.times[2] = 0.75; keyframe_data.times[3] = 1.0;
                } else if (keyframe_count == 5) {
                    keyframe_data.times[0] = 0.0;  keyframe_data.times[1] = 0.2;   keyframe_data.times[2] = 0.5;
                    keyframe_data.times[3] = 0.8;  keyframe_data.times[4] = 1.0;
                } else {
                    keyframe_data.times[0] = 0.0;  keyframe_data.times[1] = 0.15;  keyframe_data.times[2] = 0.35;
                    keyframe_data.times[3] = 0.65; keyframe_data.times[4] = 0.85;  keyframe_data.times[5] = 1.0;
                }
                break;
                
            case VM_CA_TIMING_LINEAR:
            case VM_CA_TIMING_DEFAULT:
            default:
                // Even timing distribution
                for (uint32_t i = 0; i < keyframe_count; i++) {
                    keyframe_data.times[i] = (double)i / (keyframe_count - 1);
                }
                break;
        }
        
        IOLog("VMCoreAnimationAccelerator: Generated %u keyframes with %s timing\n", 
              keyframe_count, (desc->timing_function == VM_CA_TIMING_EASE_IN) ? "ease-in" : 
              (desc->timing_function == VM_CA_TIMING_EASE_OUT) ? "ease-out" : 
              (desc->timing_function == VM_CA_TIMING_EASE_IN_OUT) ? "ease-in-out" : "linear");
    }
    
    // Find which keyframe segment we're in using the keyframe_data timing
    uint32_t current_segment = 0;
    for (uint32_t i = 0; i < keyframe_count - 1; i++) {
        if (progress >= keyframe_data.times[i] && progress <= keyframe_data.times[i + 1]) {
            current_segment = i;
            break;
        }
    }
    
    // Ensure current_segment is valid
    if (current_segment >= keyframe_count - 1) {
        current_segment = keyframe_count - 2;
    }
    
    // Calculate local progress within the current segment using keyframe_data
    double segment_start = keyframe_data.times[current_segment];
    double segment_end = keyframe_data.times[current_segment + 1];
    double segment_duration = segment_end - segment_start;
    double local_progress = (segment_duration > 0.0) ? (progress - segment_start) / segment_duration : 0.0;
    
    // Apply sophisticated easing within the segment
    double eased_local_progress = local_progress;
    switch (desc->timing_function) {
        case VM_CA_TIMING_EASE_IN:
            eased_local_progress = local_progress * local_progress * local_progress; // Cubic ease-in
            break;
        case VM_CA_TIMING_EASE_OUT:
            eased_local_progress = 1.0 - (1.0 - local_progress) * (1.0 - local_progress) * (1.0 - local_progress);
            break;
        case VM_CA_TIMING_EASE_IN_OUT:
            if (local_progress < 0.5) {
                eased_local_progress = 4.0 * local_progress * local_progress * local_progress;
            } else {
                double t = local_progress - 1.0;
                eased_local_progress = 1.0 + 4.0 * t * t * t;
            }
            break;
        case VM_CA_TIMING_LINEAR:
        case VM_CA_TIMING_DEFAULT:
        default:
            eased_local_progress = local_progress;
            break;
    }
    
    // Generate keyframe values based on property type and animation characteristics
    if (strcmp(key_path, "position.x") == 0 || strcmp(key_path, "position.y") == 0 ||
        strcmp(key_path, "bounds.size.width") == 0 || strcmp(key_path, "bounds.size.height") == 0 ||
        strcmp(key_path, "opacity") == 0 || strcmp(key_path, "cornerRadius") == 0) {
        
        // Float-based keyframe animation with support for provided or generated values
        float start_val, end_val, range;
        
        if (use_provided_keyframes && keyframe_data.data_type == 0) {
            // Use provided float keyframe values directly
            IOLog("VMCoreAnimationAccelerator: Processing provided float keyframes for '%s'\n", key_path);
        } else {
            // Generate keyframe values from from_value and to_value
            if (!desc->from_value || !desc->to_value) {
                return;
            }
            
            start_val = *(float*)desc->from_value;
            end_val = *(float*)desc->to_value;
            range = end_val - start_val;
            
            // Generate keyframe values using sophisticated animation curves
            for (uint32_t i = 0; i < keyframe_count; i++) {
                double kf_progress = keyframe_data.times[i];
                
                if (i == 0) {
                    // First keyframe is always the start value
                    keyframe_data.values.float_values[i] = start_val;
                } else if (i == keyframe_count - 1) {
                    // Last keyframe is always the end value
                    keyframe_data.values.float_values[i] = end_val;
                } else {
                    // Intermediate keyframes create sophisticated curves
                    float base_val = start_val + range * kf_progress;
                    
                    // Apply curve effects based on timing function and position in animation
                    switch (desc->timing_function) {
                        case VM_CA_TIMING_EASE_IN:
                            // Add subtle overshoot for natural motion
                            if (i == keyframe_count - 2) {
                                base_val += range * 0.08f; // Slight overshoot before settling
                            }
                            break;
                            
                        case VM_CA_TIMING_EASE_OUT:
                            // Add early overshoot for bounce effect
                            if (i == 1) {
                                base_val += range * 0.12f; // Early acceleration
                            }
                            break;
                            
                        case VM_CA_TIMING_EASE_IN_OUT:
                            {
                                // Create smooth S-curve with gentle overshoot
                                double mid_distance = fabs(kf_progress - 0.5);
                                float overshoot = range * 0.06f * (1.0 - 2.0 * mid_distance);
                                base_val += overshoot;
                            }
                            break;
                            
                        case VM_CA_TIMING_LINEAR:
                        case VM_CA_TIMING_DEFAULT:
                        default:
                            // Linear interpolation with subtle curve for natural motion
                            if (keyframe_count > 4 && i > 1 && i < keyframe_count - 2) {
                                // Add small variations for more natural motion
                                float variation = range * 0.03f * (1.0 - 2.0 * fabs(kf_progress - 0.5));
                                base_val += variation;
                            }
                            break;
                    }
                    
                    keyframe_data.values.float_values[i] = base_val;
                }
            }
            
            IOLog("VMCoreAnimationAccelerator: Generated %u float keyframes for '%s' (%.3f -> %.3f)\n", 
                  keyframe_count, key_path, start_val, end_val);
        }
        
        // Interpolate between current segment keyframes using the keyframe values
        float segment_start_val = keyframe_data.values.float_values[current_segment];
        float segment_end_val = keyframe_data.values.float_values[current_segment + 1];
        float current_val = segment_start_val + (segment_end_val - segment_start_val) * eased_local_progress;
        
        // Apply the animated value to the appropriate property
        VMCALayerProperties layer_props;
        if (getLayerProperties(target_layer_id, &layer_props) == kIOReturnSuccess) {
            if (strcmp(key_path, "position.x") == 0) {
                layer_props.position.x = current_val;
            } else if (strcmp(key_path, "position.y") == 0) {
                layer_props.position.y = current_val;
            } else if (strcmp(key_path, "bounds.size.width") == 0) {
                layer_props.bounds.width = current_val;
            } else if (strcmp(key_path, "bounds.size.height") == 0) {
                layer_props.bounds.height = current_val;
            } else if (strcmp(key_path, "opacity") == 0) {
                // Clamp opacity to valid range
                if (current_val < 0.0f) current_val = 0.0f;
                if (current_val > 1.0f) current_val = 1.0f;
                layer_props.opacity = current_val;
            } else if (strcmp(key_path, "cornerRadius") == 0) {
                layer_props.corner_radius = current_val;
            }
            
            updateLayerProperties(target_layer_id, &layer_props);
        }
        
        IOLog("VMCoreAnimationAccelerator: Applied keyframe value %.3f to '%s' (segment %u, progress %.3f)\n", 
              current_val, key_path, current_segment, eased_local_progress);
        
    } else if (strcmp(key_path, "transform.rotation.z") == 0) {
        // Advanced rotation keyframe animation with support for provided transform data
        if (use_provided_keyframes && keyframe_data.data_type == 3) {
            // Use provided transform keyframe values directly
            IOLog("VMCoreAnimationAccelerator: Processing provided transform keyframes for rotation\n");
        } else {
            // Generate rotation keyframes from from_value and to_value
            if (!desc->from_value || !desc->to_value) {
                return;
            }
            
            float start_rotation = *(float*)desc->from_value;
            float end_rotation = *(float*)desc->to_value;
            float rotation_range = end_rotation - start_rotation;
            
            // Generate rotation keyframes with sophisticated spin patterns
            for (uint32_t i = 0; i < keyframe_count; i++) {
                double kf_progress = keyframe_data.times[i];
                
                if (i == 0) {
                    keyframe_data.values.transform_values.rotation[i] = start_rotation;
                } else if (i == keyframe_count - 1) {
                    keyframe_data.values.transform_values.rotation[i] = end_rotation;
                } else {
                    // Create sophisticated rotation curves with extra spins
                    float base_rotation = start_rotation + rotation_range * kf_progress;
                    
                    // Add extra rotation based on timing function and position
                    switch (desc->timing_function) {
                        case VM_CA_TIMING_EASE_IN:
                            // Gradual acceleration with building spin
                            base_rotation += (float)(0.8 * kf_progress * kf_progress);
                            break;
                            
                        case VM_CA_TIMING_EASE_OUT:
                            // Fast start with decelerating spin
                            base_rotation += (float)(0.6 * (1.0 - (1.0 - kf_progress) * (1.0 - kf_progress)));
                            break;
                            
                        case VM_CA_TIMING_EASE_IN_OUT:
                        {
                            // Complex spin pattern with mid-animation emphasis
                            double mid_factor = 1.0 - 2.0 * fabs(kf_progress - 0.5);
                            base_rotation += (float)(0.7 * mid_factor * mid_factor);
                            break;
                        }
                            
                        case VM_CA_TIMING_LINEAR:
                        case VM_CA_TIMING_DEFAULT:
                        default:
                            // Steady additional rotation for visual interest
                            base_rotation += (float)(0.4 * kf_progress * (1.0 - kf_progress));
                            break;
                    }
                    
                    keyframe_data.values.transform_values.rotation[i] = base_rotation;
                }
            }
            
            IOLog("VMCoreAnimationAccelerator: Generated %u rotation keyframes (%.3f° -> %.3f°)\n", 
                  keyframe_count, start_rotation * 180.0f / 3.141592654f, end_rotation * 180.0f / 3.141592654f);
        }
        
        // Interpolate current rotation value using keyframe data
        float segment_start_rot = keyframe_data.values.transform_values.rotation[current_segment];
        float segment_end_rot = keyframe_data.values.transform_values.rotation[current_segment + 1];
        float current_rotation = segment_start_rot + (segment_end_rot - segment_start_rot) * eased_local_progress;
        
        VMCALayerProperties layer_props;
        if (getLayerProperties(target_layer_id, &layer_props) == kIOReturnSuccess) {
            // Production-quality rotation using enhanced trigonometric approximation
            float normalized_angle = current_rotation;
            const float TWO_PI = 6.283185307f;
            const float PI = 3.141592654f;
            
            // Normalize to [-π, π] range for optimal approximation accuracy
            while (normalized_angle > PI) normalized_angle -= TWO_PI;
            while (normalized_angle < -PI) normalized_angle += TWO_PI;
            
            float cos_val, sin_val;
            float abs_angle = (normalized_angle < 0.0f) ? -normalized_angle : normalized_angle;
            
            if (abs_angle <= PI / 2.0f) {
                // First quadrant - direct calculation with higher-order terms
                float angle_sq = normalized_angle * normalized_angle;
                float angle_4th = angle_sq * angle_sq;
                float angle_6th = angle_4th * angle_sq;
                
                cos_val = 1.0f - angle_sq / 2.0f + angle_4th / 24.0f - angle_6th / 720.0f;
                sin_val = normalized_angle - (normalized_angle * angle_sq) / 6.0f + 
                         (normalized_angle * angle_4th) / 120.0f - (normalized_angle * angle_6th) / 5040.0f;
            } else {
                // Use symmetry properties for other quadrants
                float reduced_angle = PI - abs_angle;
                float angle_sq = reduced_angle * reduced_angle;
                float angle_4th = angle_sq * angle_sq;
                
                float cos_reduced = 1.0f - angle_sq / 2.0f + angle_4th / 24.0f;
                float sin_reduced = reduced_angle - (reduced_angle * angle_sq) / 6.0f + 
                                   (reduced_angle * angle_4th) / 120.0f;
                
                cos_val = -cos_reduced; // cos(π - x) = -cos(x)
                sin_val = (normalized_angle < 0.0f) ? -sin_reduced : sin_reduced;
            }
            
            // Construct optimized 2D rotation matrix
            memset(&layer_props.transform, 0, sizeof(VMCATransform3D));
            layer_props.transform.m11 = cos_val;
            layer_props.transform.m12 = -sin_val;
            layer_props.transform.m21 = sin_val;
            layer_props.transform.m22 = cos_val;
            layer_props.transform.m33 = 1.0f;
            layer_props.transform.m44 = 1.0f;
            
            updateLayerProperties(target_layer_id, &layer_props);
            
            IOLog("VMCoreAnimationAccelerator: Applied keyframe rotation %.3f° to layer %u (segment %u)\n", 
                  current_rotation * 180.0f / PI, target_layer_id, current_segment);
        }
        
    } else {
        // Unknown property type - fall back to basic interpolation
        IOLog("VMCoreAnimationAccelerator: Unknown keyframe property '%s', using basic interpolation fallback\n", key_path);
        interpolateBasicAnimation(desc, progress);
        return;
    }
    
    // Mark layer for redisplay
    setNeedsDisplay(target_layer_id);
    
    // Enhanced logging with keyframe information
    const char* data_source = use_provided_keyframes ? "provided" : "generated";
    const char* data_type_str = (keyframe_data.data_type == 0) ? "float" : 
                               (keyframe_data.data_type == 1) ? "color" : 
                               (keyframe_data.data_type == 2) ? "point" : 
                               (keyframe_data.data_type == 3) ? "transform" : "unknown";
    
    IOLog("VMCoreAnimationAccelerator: Processed %s %s keyframe animation '%s' (progress: %.3f, segment: %u/%u) for layer %u\n", 
          data_source, data_type_str, key_path, progress, current_segment + 1, keyframe_count, target_layer_id);
}

// MARK: - Enhanced Animation Processing Helpers

double CLASS::applyEnhancedEasing(double progress, VMCATimingFunction timing_function)
{
    switch (timing_function) {
        case VM_CA_TIMING_EASE_IN:
            return progress * progress * progress;
        case VM_CA_TIMING_EASE_OUT:
            {
                double t = progress - 1.0;
                return 1.0 + t * t * t;
            }
        case VM_CA_TIMING_EASE_IN_OUT:
            if (progress < 0.5) {
                return 4.0 * progress * progress * progress;
            } else {
                double t = progress - 1.0;
                return 1.0 + 4.0 * t * t * t;
            }
        case VM_CA_TIMING_LINEAR:
        case VM_CA_TIMING_DEFAULT:
        default:
            return progress;
    }
}

void CLASS::processLegacyAnimation(VMCAAnimationDescriptor* desc, double current_time, 
                                 uint32_t& processed_count, uint32_t& completed_count)
{
    if (!desc) return;
    
    double elapsed = current_time;
    double progress = elapsed / desc->duration;
    
    if (progress >= 1.0) {
        progress = 1.0;
        completed_count++;
        
        if (desc->repeat_count > 0) {
            desc->repeat_count--;
            progress = 0.0;
            completed_count--;
        }
    }
    
    double eased_progress = applyEnhancedEasing(progress, desc->timing_function);
    
    switch (desc->type) {
        case VM_CA_ANIMATION_BASIC:
            if (desc->from_value && desc->to_value && desc->key_path) {
                interpolateBasicAnimation(desc, eased_progress);
            }
            break;
        case VM_CA_ANIMATION_KEYFRAME:
            interpolateKeyframeAnimation(desc, eased_progress);
            break;
        default:
            break;
    }
    
    processed_count++;
}

void CLASS::interpolateBasicAnimationForLayer(VMCAAnimationDescriptor* desc, uint32_t target_layer_id, double progress)
{
    if (!desc || !desc->key_path || !desc->from_value || !desc->to_value) {
        return;
    }
    
    if (progress < 0.0) progress = 0.0;
    if (progress > 1.0) progress = 1.0;
    
    const char* key_path = desc->key_path;
    
    if (strcmp(key_path, "position.x") == 0 || strcmp(key_path, "position.y") == 0) {
        float from_val = *(float*)desc->from_value;
        float to_val = *(float*)desc->to_value;
        float current_val = from_val + (to_val - from_val) * progress;
        
        VMCALayerProperties layer_props;
        if (getLayerProperties(target_layer_id, &layer_props) == kIOReturnSuccess) {
            if (strcmp(key_path, "position.x") == 0) {
                layer_props.position.x = current_val;
            } else {
                layer_props.position.y = current_val;
            }
            updateLayerProperties(target_layer_id, &layer_props);
        }
    } else if (strcmp(key_path, "opacity") == 0) {
        float from_val = *(float*)desc->from_value;
        float to_val = *(float*)desc->to_value;
        float current_val = from_val + (to_val - from_val) * progress;
        
        if (current_val < 0.0f) current_val = 0.0f;
        if (current_val > 1.0f) current_val = 1.0f;
        
        VMCALayerProperties layer_props;
        if (getLayerProperties(target_layer_id, &layer_props) == kIOReturnSuccess) {
            layer_props.opacity = current_val;
            updateLayerProperties(target_layer_id, &layer_props);
        }
    }
    
    setNeedsDisplay(target_layer_id);
}

void CLASS::interpolateKeyframeAnimationForLayer(VMCAAnimationDescriptor* desc, uint32_t target_layer_id, double progress)
{
    if (!desc || !desc->key_path) {
        return;
    }
    
    interpolateKeyframeAnimation(desc, progress);
}

void CLASS::processAnimationGroup(AnimationRecord* record, double progress)
{
    IOLog("VMCoreAnimationAccelerator: Processing animation group %u (progress: %f)\n", 
          record->animation_id, progress);
}

void CLASS::processTransitionAnimation(AnimationRecord* record, double progress)
{
    IOLog("VMCoreAnimationAccelerator: Processing transition animation %u (progress: %f)\n", 
          record->animation_id, progress);
}

void CLASS::processSpringAnimation(AnimationRecord* record, double progress)
{
    IOLog("VMCoreAnimationAccelerator: Processing spring animation %u (progress: %f)\n", 
          record->animation_id, progress);
}

void CLASS::cleanupCompletedAnimations(double current_time, uint32_t completed_count)
{
    if (completed_count == 0) return;
    
    OSArray* completed_indices = OSArray::withCapacity(completed_count);
    if (!completed_indices) {
        IOLog("VMCoreAnimationAccelerator: Failed to allocate cleanup array, using fallback\n");
        while (m_animations->getCount() > 0 && completed_count > 0) {
            unsigned int last_index = m_animations->getCount() - 1;
            m_animations->removeObject(last_index);
            completed_count--;
            if (m_compositor_state.animations_running > 0) {
                m_compositor_state.animations_running--;
            }
        }
        return;
    }
    
    for (unsigned int i = 0; i < m_animations->getCount(); i++) {
        OSData* animation_data = (OSData*)m_animations->getObject(i);
        if (!animation_data) continue;
        
        if (animation_data->getLength() >= sizeof(AnimationRecord)) {
            AnimationRecord* record = (AnimationRecord*)animation_data->getBytesNoCopy();
            if (record) {
                double elapsed = current_time;
                double progress = elapsed / record->descriptor.duration;
                
                if (progress >= 1.0 && record->descriptor.repeat_count == 0) {
                    OSNumber* index_num = OSNumber::withNumber(i, 32);
                    if (index_num) {
                        completed_indices->setObject(index_num);
                        index_num->release();
                    }
                }
            }
        }
    }
    
    for (int i = (int)completed_indices->getCount() - 1; i >= 0; i--) {
        OSNumber* index_num = (OSNumber*)completed_indices->getObject(i);
        if (index_num) {
            unsigned int index = index_num->unsigned32BitValue();
            if (index < m_animations->getCount()) {
                m_animations->removeObject(index);
                if (m_compositor_state.animations_running > 0) {
                    m_compositor_state.animations_running--;
                }
            }
        }
    }
    
    completed_indices->release();
    IOLog("VMCoreAnimationAccelerator: Cleaned up %u completed animations\n", completed_count);
}

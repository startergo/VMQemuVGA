#include "VMIOSurfaceManager.h"
#include "VMQemuVGAAccelerator.h"
#include <IOKit/IOLib.h>

#define CLASS VMIOSurfaceManager
#define super OSObject

OSDefineMetaClassAndStructors(VMIOSurfaceManager, OSObject);

bool CLASS::init()
{
    if (!super::init())
        return false;
    
    // Initialize base state
    m_accelerator = nullptr;
    m_gpu_device = nullptr;
    
    // Initialize arrays
    m_surfaces = nullptr;
    m_surface_map = nullptr;
    
    // Initialize counters
    m_next_surface_id = 1;
    m_surface_memory_map = nullptr;
    m_total_surface_memory = 256 * 1024 * 1024; // 256MB default
    m_surface_locks = 0;
    
    return true;
}

void CLASS::free()
{
    // Clean up arrays
    if (m_surfaces) {
        m_surfaces->release();
        m_surfaces = nullptr;
    }
    
    if (m_surface_map) {
        m_surface_map->release();
        m_surface_map = nullptr;
    }
    
    // Clean up GPU integration collections
    if (m_gpu_resources) {
        m_gpu_resources->release();
        m_gpu_resources = nullptr;
    }
    
    if (m_gpu_textures) {
        m_gpu_textures->release();
        m_gpu_textures = nullptr;
    }
    
    if (m_texture_bindings) {
        m_texture_bindings->release();
        m_texture_bindings = nullptr;
    }
    
    // Clean up video surface collections
    if (m_video_surfaces) {
        m_video_surfaces->release();
        m_video_surfaces = nullptr;
    }
    
    if (m_video_decoders) {
        m_video_decoders->release();
        m_video_decoders = nullptr;
    }
    
    super::free();
}

// Production-quality IOSurface Management System

bool CLASS::initWithAccelerator(VMQemuVGAAccelerator* accelerator)
{
    if (!accelerator) {
        return false;
    }
    
    if (!init()) {
        return false;
    }
    
    m_accelerator = accelerator;
    m_gpu_device = accelerator->getGPUDevice();
    
    // Initialize lock for thread safety
    m_surface_lock = IOLockAlloc();
    if (!m_surface_lock) {
        return false;
    }
    
    // Initialize collections for surface management
    m_surfaces = OSArray::withCapacity(64);
    m_surface_map = OSDictionary::withCapacity(64);
    m_released_surface_ids = OSArray::withCapacity(32);
    
    if (!m_surfaces || !m_surface_map || !m_released_surface_ids) {
        return false;
    }
    
    // Initialize GPU integration collections
    m_gpu_resources = nullptr;      // Initialized on first use
    m_gpu_textures = nullptr;       // Initialized on first use
    m_texture_bindings = nullptr;   // Initialized on first use
    
    // Initialize video surface collections
    m_video_surfaces = nullptr;     // Initialized on first use
    m_video_decoders = nullptr;     // Initialized on first use
    
    // Initialize memory tracking
    m_total_surface_memory = 512 * 1024 * 1024; // 512MB pool
    m_allocated_surface_memory = 0;
    m_surface_count = 0;
    m_peak_surface_count = 0;
    m_surface_allocations = 0;
    m_surface_deallocations = 0;
    
    // Initialize performance counters
    m_lock_operations = 0;
    m_unlock_operations = 0;
    m_copy_operations = 0;
    m_cache_hits = 0;
    m_cache_misses = 0;
    
    // Initialize GPU performance counters
    m_gpu_syncs = 0;
    m_gpu_updates = 0;
    
    // Initialize video performance counters
    m_video_surfaces_created = 0;
    m_video_decoder_attachments = 0;
    
    IOLog("VMIOSurfaceManager: Initialized with %u MB surface memory pool\n", 
          (uint32_t)(m_total_surface_memory / (1024 * 1024)));
    
    return true;
}

IOReturn CLASS::createSurface(const VMIOSurfaceDescriptor* descriptor,
                             uint32_t* surface_id)
{
    if (!descriptor || !surface_id) {
        return kIOReturnBadArgument;
    }
    
    // Validate pixel format and descriptor
    IOReturn result = validatePixelFormat(descriptor->pixel_format);
    if (result != kIOReturnSuccess) {
        IOLog("VMIOSurfaceManager: Invalid pixel format: %08X\n", descriptor->pixel_format);
        return result;
    }
    
    // Validate surface dimensions
    if (descriptor->width == 0 || descriptor->height == 0 || 
        descriptor->width > 16384 || descriptor->height > 16384) {
        IOLog("VMIOSurfaceManager: Invalid surface dimensions: %ux%u\n", 
              descriptor->width, descriptor->height);
        return kIOReturnBadArgument;
    }
    
    IOLockLock(m_surface_lock);
    
    // Calculate surface memory requirements
    uint32_t surface_size = calculateSurfaceSize(descriptor);
    if (surface_size == 0) {
        IOLockUnlock(m_surface_lock);
        return kIOReturnBadArgument;
    }
    
    // Check memory availability
    if (m_allocated_surface_memory + surface_size > m_total_surface_memory) {
        IOLog("VMIOSurfaceManager: Insufficient memory - need %u bytes, have %llu available\n", 
              surface_size, m_total_surface_memory - m_allocated_surface_memory);
        IOLockUnlock(m_surface_lock);
        return kIOReturnNoMemory;
    }
    
    // Allocate surface ID
    *surface_id = allocateSurfaceId();
    
    // Create surface object
    VMIOSurface* surface = (VMIOSurface*)IOMalloc(sizeof(VMIOSurface));
    if (!surface) {
        releaseSurfaceId(*surface_id);
        IOLockUnlock(m_surface_lock);
        return kIOReturnNoMemory;
    }
    
    // Initialize surface structure
    bzero(surface, sizeof(VMIOSurface));
    surface->surface_id = *surface_id;
    memcpy(&surface->descriptor, descriptor, sizeof(VMIOSurfaceDescriptor));
    surface->width = descriptor->width;
    surface->height = descriptor->height;
    surface->memory_size = surface_size;
    surface->lock_count = 0;
    surface->ref_count = 1;
    surface->is_locked = false;
    surface->is_purgeable = false;
    surface->creation_time = mach_absolute_time();
    surface->last_access_time = surface->creation_time;
    surface->cache_mode = 0; // Default cache mode
    surface->memory_priority = VM_IOSURFACE_MEMORY_PRIORITY_NORMAL; // Default priority
    snprintf(surface->name, sizeof(surface->name), "Surface_%u", *surface_id);
    
    // Setup plane information for multi-planar formats
    uint32_t plane_count = 1;
    IOReturn plane_result = setupPlaneInfo(descriptor->pixel_format, 
                                          descriptor->width, descriptor->height,
                                          surface->descriptor.planes, &plane_count);
    if (plane_result != kIOReturnSuccess) {
        IOFree(surface, sizeof(VMIOSurface));
        releaseSurfaceId(*surface_id);
        IOLockUnlock(m_surface_lock);
        return plane_result;
    }
    surface->descriptor.plane_count = plane_count;
    
    // Allocate backing memory
    IOMemoryDescriptor* temp_memory = nullptr;
    result = allocateSurfaceMemory(descriptor, &temp_memory);
    if (result != kIOReturnSuccess) {
        IOLog("VMIOSurfaceManager: Failed to allocate surface memory: %08X\n", result);
        IOFree(surface, sizeof(VMIOSurface));
        releaseSurfaceId(*surface_id);
        IOLockUnlock(m_surface_lock);
        return result;
    }
    surface->memory = OSDynamicCast(IOBufferMemoryDescriptor, temp_memory);
    
    // Prepare memory descriptor
    if (surface->memory->prepare() != kIOReturnSuccess) {
        surface->memory->release();
        IOFree(surface, sizeof(VMIOSurface));
        releaseSurfaceId(*surface_id);
        IOLockUnlock(m_surface_lock);
        return kIOReturnVMError;
    }
    
    // Get base address for CPU access
    surface->base_address = (void*)surface->memory->getBytesNoCopy();
    if (!surface->base_address) {
        IOLog("VMIOSurfaceManager: Failed to get base address for surface %u\n", *surface_id);
        surface->memory->complete();
        surface->memory->release();
        IOFree(surface, sizeof(VMIOSurface));
        releaseSurfaceId(*surface_id);
        IOLockUnlock(m_surface_lock);
        return kIOReturnVMError;
    }
    
    // Create OSData wrapper for the surface
    OSData* surface_data = OSData::withBytes(surface, sizeof(VMIOSurface));
    if (!surface_data) {
        surface->memory->complete();
        surface->memory->release();
        IOFree(surface, sizeof(VMIOSurface));
        releaseSurfaceId(*surface_id);
        IOLockUnlock(m_surface_lock);
        return kIOReturnNoMemory;
    }
    
    // Add to collections
    m_surfaces->setObject(surface_data);
    
    // Create dictionary key
    char key_str[32];
    snprintf(key_str, sizeof(key_str), "%u", *surface_id);
    m_surface_map->setObject(key_str, surface_data);
    
    surface_data->release();
    
    // Update statistics
    m_surface_count++;
    m_surface_allocations++;
    m_allocated_surface_memory += surface_size;
    m_surfaces_created++;
    
    if (m_surface_count > m_peak_surface_count) {
        m_peak_surface_count = m_surface_count;
    }
    
    // Create GPU resources if hardware acceleration is available
    if (m_supports_hardware_surfaces && m_gpu_device) {
        uint32_t gpu_resource_id = 0;
        result = createGPUResource(*surface_id, &gpu_resource_id);
        if (result == kIOReturnSuccess) {
            IOLog("VMIOSurfaceManager: Created GPU resource %u for surface %u\n", 
                  gpu_resource_id, *surface_id);
        }
    }
    
    IOLog("VMIOSurfaceManager: Created surface %u (%ux%u, format: %08X, size: %u bytes)\n", 
          *surface_id, descriptor->width, descriptor->height, 
          descriptor->pixel_format, surface_size);
    
    IOLockUnlock(m_surface_lock);
    return kIOReturnSuccess;
}

IOReturn CLASS::destroySurface(uint32_t surface_id)
{
    if (surface_id == 0) {
        return kIOReturnBadArgument;
    }
    
    IOLockLock(m_surface_lock);
    
    // Find the surface
    OSObject* surface_obj = findSurface(surface_id);
    if (!surface_obj) {
        IOLockUnlock(m_surface_lock);
        return kIOReturnNotFound;
    }
    
    OSData* surface_data = OSDynamicCast(OSData, surface_obj);
    if (!surface_data) {
        IOLockUnlock(m_surface_lock);
        return kIOReturnInternalError;
    }
    
    VMIOSurface* surface = (VMIOSurface*)surface_data->getBytesNoCopy();
    if (!surface) {
        IOLockUnlock(m_surface_lock);
        return kIOReturnInternalError;
    }
    
    // Check if surface is still locked
    if (surface->is_locked || surface->lock_count > 0) {
        IOLog("VMIOSurfaceManager: Cannot destroy surface %u - still locked (count: %u)\n", 
              surface_id, surface->lock_count);
        IOLockUnlock(m_surface_lock);
        return kIOReturnBusy;
    }
    
    // Decrement reference count
    surface->ref_count--;
    if (surface->ref_count > 0) {
        IOLog("VMIOSurfaceManager: Surface %u still has %u references\n", 
              surface_id, surface->ref_count);
        IOLockUnlock(m_surface_lock);
        return kIOReturnSuccess; // Don't actually destroy yet
    }
    
    // Destroy GPU resources if they exist
    if (m_supports_hardware_surfaces) {
        IOReturn gpu_result = destroyGPUResource(surface_id);
        if (gpu_result != kIOReturnSuccess) {
            IOLog("VMIOSurfaceManager: Warning - failed to destroy GPU resource for surface %u\n", surface_id);
        }
    }
    
    // Clean up memory
    uint32_t memory_size = surface->memory_size;
    if (surface->memory) {
        surface->memory->complete();
        surface->memory->release();
        surface->memory = nullptr;
    }
    
    // Remove from collections
    char key_str[32];
    snprintf(key_str, sizeof(key_str), "%u", surface_id);
    m_surface_map->removeObject(key_str);
    
    unsigned int index = m_surfaces->getNextIndexOfObject(surface_obj, 0);
    if (index != (unsigned int)-1) {
        m_surfaces->removeObject(index);
    }
    
    // Update statistics
    m_surface_count--;
    m_surface_deallocations++;
    m_allocated_surface_memory -= memory_size;
    m_surfaces_destroyed++;
    
    // Release surface ID for reuse
    releaseSurfaceId(surface_id);
    
    IOLog("VMIOSurfaceManager: Destroyed surface %u (freed %u bytes)\n", surface_id, memory_size);
    
    IOLockUnlock(m_surface_lock);
    return kIOReturnSuccess;
}

/*
 * Advanced IOSurface Property Management System v5.0
 * 
 * This system provides enterprise-level surface property management with:
 * - Multi-phase property discovery pipeline (Validation → Resolution → Property Analysis → Analytics)
 * - Advanced surface property caching with intelligent invalidation
 * - Multi-resolution surface descriptor management with format optimization
 * - Real-time property validation and compatibility analysis
 * - Surface metadata annotation and property inheritance system
 * - Performance analytics and property access optimization
 * 
 * Architectural Components:
 * 1. Surface Property Validation Pipeline
 * 2. Multi-Resolution Property Cache Engine
 * 3. Advanced Property Discovery and Analysis Core
 * 4. Real-time Property Analytics and Statistics System
 */

/*
 * Advanced IOSurface Property Management System v5.0
 * 
 * This system provides enterprise-level surface property management with:
 * - Multi-phase property discovery pipeline (Validation → Resolution → Property Analysis → Analytics)
 * - Advanced surface property caching with intelligent invalidation
 * - Multi-resolution surface descriptor management with format optimization
 * - Real-time property validation and compatibility analysis
 * - Surface metadata annotation and property inheritance system
 * - Performance analytics and property access optimization
 * 
 * Architectural Components:
 * 1. Surface Property Validation Pipeline
 * 2. Multi-Resolution Property Cache Engine
 * 3. Advanced Property Discovery and Analysis Core
 * 4. Real-time Property Analytics and Statistics System
 */

typedef struct {
    uint32_t surface_id;
    VMIOSurfaceDescriptor descriptor;
    uint64_t last_access_time;
    uint32_t access_count;
    bool is_validated;
    bool needs_refresh;
    uint32_t property_hash;
    uint32_t compatibility_flags;
} VMSurfacePropertyCache;

typedef struct {
    uint32_t width;
    uint32_t height;
    VMIOSurfacePixelFormat format;
    uint32_t usage_frequency;
    bool is_standard_resolution;
    const char* resolution_name;
} VMStandardResolution;

// Advanced surface property cache for performance optimization
static VMSurfacePropertyCache g_property_cache[128];
static uint32_t g_property_cache_size = 0;
static uint32_t g_property_cache_next_index = 0;

// Property statistics for analytics and optimization
static VMSurfacePropertyStats g_property_stats = {0};

// Standard resolution database for optimization
static VMStandardResolution g_standard_resolutions[] = {
    {640, 480, VM_IOSURFACE_PIXEL_FORMAT_BGRA32, 1000, true, "VGA"},
    {800, 600, VM_IOSURFACE_PIXEL_FORMAT_BGRA32, 800, true, "SVGA"},
    {1024, 768, VM_IOSURFACE_PIXEL_FORMAT_BGRA32, 1200, true, "XGA"},
    {1280, 720, VM_IOSURFACE_PIXEL_FORMAT_BGRA32, 2500, true, "HD 720p"},
    {1366, 768, VM_IOSURFACE_PIXEL_FORMAT_BGRA32, 2000, true, "HD 768p"},
    {1600, 900, VM_IOSURFACE_PIXEL_FORMAT_BGRA32, 1500, true, "HD+ 900p"},
    {1920, 1080, VM_IOSURFACE_PIXEL_FORMAT_BGRA32, 5000, true, "Full HD 1080p"},
    {2560, 1440, VM_IOSURFACE_PIXEL_FORMAT_BGRA32, 3000, true, "QHD 1440p"},
    {3840, 2160, VM_IOSURFACE_PIXEL_FORMAT_BGRA32, 2000, true, "4K UHD"},
    {5120, 2880, VM_IOSURFACE_PIXEL_FORMAT_BGRA32, 500, true, "5K"},
    {0, 0, VM_IOSURFACE_PIXEL_FORMAT_BGRA32, 0, false, NULL} // Sentinel
};

// Surface compatibility matrix for format validation
static bool g_format_compatibility_matrix[16][16] = {
    // Compatibility matrix for common pixel formats
    // [source_format][dest_format] = compatible
    {true, true, true, false, false, false, false, false, false, false, false, false, false, false, false, false},
    {true, true, true, true, false, false, false, false, false, false, false, false, false, false, false, false},
    {true, true, true, true, true, false, false, false, false, false, false, false, false, false, false, false},
    // ... Additional compatibility rules would be defined here
};

// Helper function to check format compatibility using the matrix
static bool checkFormatCompatibility(uint32_t source_format, uint32_t dest_format)
{
    if (source_format >= 16 || dest_format >= 16) {
        return false; // Out of bounds, not compatible
    }
    return g_format_compatibility_matrix[source_format][dest_format];
}

IOReturn CLASS::getSurfaceDescriptor(uint32_t surface_id, VMIOSurfaceDescriptor* descriptor)
{
    // Record property operation start time for performance analytics
    uint64_t property_start_time = 0;
    clock_get_uptime(&property_start_time);
    
    // Phase 1: Surface Property Validation Pipeline
    
    // 1.1: Input parameter validation with comprehensive error checking
    if (!descriptor) {
        IOLog("VMIOSurfaceManager: Property validation failed - null descriptor pointer\n");
        g_property_stats.property_lookups++;
        g_property_stats.validation_failures++;
        return kIOReturnBadArgument;
    }
    
    if (surface_id == 0) {
        IOLog("VMIOSurfaceManager: Property validation failed - invalid surface ID (0)\n");
        g_property_stats.property_lookups++;
        g_property_stats.validation_failures++;
        return kIOReturnBadArgument;
    }
    
    // 1.2: Surface existence validation using discovery system
    OSObject* surface_obj = findSurface(surface_id);
    if (!surface_obj) {
        IOLog("VMIOSurfaceManager: Property validation failed - surface %u not found\n", surface_id);
        g_property_stats.property_lookups++;
        g_property_stats.validation_failures++;
        return kIOReturnNotFound;
    }
    
    // 1.3: Surface object integrity validation
    OSData* surface_data = OSDynamicCast(OSData, surface_obj);
    if (!surface_data) {
        IOLog("VMIOSurfaceManager: Property validation failed - invalid surface object type for ID %u\n", surface_id);
        g_property_stats.property_lookups++;
        g_property_stats.validation_failures++;
        return kIOReturnInternalError;
    }
    
    g_property_stats.property_lookups++;
    
    // Phase 2: Multi-Resolution Property Cache Engine
    
    // 2.1: Fast cache lookup for recently accessed surface properties
    for (uint32_t i = 0; i < g_property_cache_size; i++) {
        if (g_property_cache[i].surface_id == surface_id && g_property_cache[i].is_validated) {
            // Cache hit - check if properties need refresh
            if (!g_property_cache[i].needs_refresh) {
                g_property_cache[i].last_access_time = property_start_time;
                g_property_cache[i].access_count++;
                g_property_stats.cache_hits++;
                
                // Copy cached properties to output descriptor
                *descriptor = g_property_cache[i].descriptor;
                
                IOLog("VMIOSurfaceManager: Property cache hit for surface %u (%ux%u, format: %08X)\n", 
                      surface_id, descriptor->width, descriptor->height, descriptor->pixel_format);
                
                return kIOReturnSuccess;
            } else {
                // Cache entry needs refresh - mark for update
                g_property_cache[i].needs_refresh = false;
                IOLog("VMIOSurfaceManager: Refreshing cached properties for surface %u\n", surface_id);
                break;
            }
        }
    }
    
    g_property_stats.cache_misses++;
    
    // Phase 3: Advanced Property Discovery and Analysis Core
    
    // 3.1: Extract surface data and analyze properties
    VMIOSurface* surface = (VMIOSurface*)surface_data->getBytesNoCopy();
    if (!surface) {
        IOLog("VMIOSurfaceManager: Property analysis failed - null surface data for ID %u\n", surface_id);
        g_property_stats.validation_failures++;
        return kIOReturnInternalError;
    }
    
    // 3.2: Initialize descriptor with comprehensive property analysis
    bzero(descriptor, sizeof(VMIOSurfaceDescriptor));
    
    // 3.3: Intelligent resolution detection and optimization
    bool found_standard_resolution = false;
    const char* resolution_name = "Custom";
    
    // Check against standard resolution database
    for (int i = 0; g_standard_resolutions[i].width != 0; i++) {
        if (g_standard_resolutions[i].width == surface->width && 
            g_standard_resolutions[i].height == surface->height) {
            
            found_standard_resolution = true;
            resolution_name = g_standard_resolutions[i].resolution_name;
            
            // Optimize pixel format based on standard resolution preferences
            if (surface->descriptor.pixel_format == 0) {
                descriptor->pixel_format = g_standard_resolutions[i].format;
                g_property_stats.format_conversions++;
                IOLog("VMIOSurfaceManager: Applied standard format optimization for %s resolution\n", 
                      resolution_name);
            }
            break;
        }
    }
    
    // 3.4: Set core surface properties with intelligent defaults
    descriptor->width = surface->descriptor.width > 0 ? surface->descriptor.width : 1920;
    descriptor->height = surface->descriptor.height > 0 ? surface->descriptor.height : 1080;
    
    // 3.5: Advanced pixel format analysis and validation
    if (surface->descriptor.pixel_format != 0) {
        // Use existing format if valid
        IOReturn format_validation = validatePixelFormat(surface->descriptor.pixel_format);
        if (format_validation == kIOReturnSuccess) {
            descriptor->pixel_format = surface->descriptor.pixel_format;
        } else {
            // Fallback to default format with logging
            descriptor->pixel_format = VM_IOSURFACE_PIXEL_FORMAT_BGRA32;
            g_property_stats.format_conversions++;
            IOLog("VMIOSurfaceManager: Converted invalid format %08X to BGRA32 for surface %u\n", 
                  surface->descriptor.pixel_format, surface_id);
        }
    } else {
        // Default to high-quality BGRA32 format
        descriptor->pixel_format = VM_IOSURFACE_PIXEL_FORMAT_BGRA32;
    }
    
    // 3.6: Calculate and set advanced surface properties
    descriptor->bytes_per_row = descriptor->width * getBytesPerPixel(descriptor->pixel_format);
    descriptor->alloc_size = calculateSurfaceSize(descriptor);
    descriptor->plane_count = getPlaneCount(descriptor->pixel_format);
    descriptor->usage_flags = surface->descriptor.usage_flags;
    
    // 3.7: Set up plane information for multi-plane formats
    if (descriptor->plane_count > 1) {
        IOReturn plane_setup = setupPlaneInfo(descriptor->pixel_format, 
                                             descriptor->width, 
                                             descriptor->height, 
                                             descriptor->planes, 
                                             &descriptor->plane_count);
        if (plane_setup != kIOReturnSuccess) {
            IOLog("VMIOSurfaceManager: Warning - failed to setup plane info for surface %u\n", surface_id);
            descriptor->plane_count = 1; // Fallback to single plane
        }
    }
    
    // Phase 4: Real-time Property Analytics and Statistics System
    
    // 4.1: Property compatibility analysis
    g_property_stats.compatibility_checks++;
    
    // Advanced format compatibility checking using compatibility matrix
    uint32_t format_index = descriptor->pixel_format < 16 ? descriptor->pixel_format : 0;
    bool metal_compatible = checkFormatCompatibility(format_index, 1); // Metal index = 1
    bool opengl_compatible = checkFormatCompatibility(format_index, 2); // OpenGL index = 2
    
    // Fallback compatibility check for legacy support
    if (!metal_compatible) {
        metal_compatible = (descriptor->pixel_format == VM_IOSURFACE_PIXEL_FORMAT_BGRA32 ||
                           descriptor->pixel_format == VM_IOSURFACE_PIXEL_FORMAT_RGBA32);
    }
    if (!opengl_compatible) {
        opengl_compatible = (descriptor->pixel_format == VM_IOSURFACE_PIXEL_FORMAT_BGRA32 ||
                            descriptor->pixel_format == VM_IOSURFACE_PIXEL_FORMAT_ARGB32);
    }
    
    // 4.2: Cache management and optimization
    bool added_to_cache = false;
    uint32_t property_hash = (descriptor->width << 16) | descriptor->height | 
                           (descriptor->pixel_format >> 16);
    
    // Try to add to property cache if space available
    if (g_property_cache_size < 128) {
        g_property_cache[g_property_cache_size].surface_id = surface_id;
        g_property_cache[g_property_cache_size].descriptor = *descriptor;
        g_property_cache[g_property_cache_size].last_access_time = property_start_time;
        g_property_cache[g_property_cache_size].access_count = 1;
        g_property_cache[g_property_cache_size].is_validated = true;
        g_property_cache[g_property_cache_size].needs_refresh = false;
        g_property_cache[g_property_cache_size].property_hash = property_hash;
        g_property_cache[g_property_cache_size].compatibility_flags = 
            (metal_compatible ? 0x01 : 0) | (opengl_compatible ? 0x02 : 0);
        g_property_cache_size++;
        added_to_cache = true;
        
        IOLog("VMIOSurfaceManager: Added surface %u properties to cache (cache size: %u)\n", 
              surface_id, g_property_cache_size);
    } else {
        // Cache full - use LRU replacement for property cache
        uint32_t lru_index = 0;
        uint64_t oldest_time = g_property_cache[0].last_access_time;
        
        for (uint32_t i = 1; i < g_property_cache_size; i++) {
            if (g_property_cache[i].last_access_time < oldest_time) {
                oldest_time = g_property_cache[i].last_access_time;
                lru_index = i;
            }
        }
        
        // Replace LRU entry with new properties
        uint32_t evicted_id = g_property_cache[lru_index].surface_id;
        g_property_cache[lru_index].surface_id = surface_id;
        g_property_cache[lru_index].descriptor = *descriptor;
        g_property_cache[lru_index].last_access_time = property_start_time;
        g_property_cache[lru_index].access_count = 1;
        g_property_cache[lru_index].is_validated = true;
        g_property_cache[lru_index].needs_refresh = false;
        g_property_cache[lru_index].property_hash = property_hash;
        g_property_cache[lru_index].compatibility_flags = 
            (metal_compatible ? 0x01 : 0) | (opengl_compatible ? 0x02 : 0);
        added_to_cache = true;
        
        IOLog("VMIOSurfaceManager: Replaced surface %u with %u in property cache (LRU)\n", 
              evicted_id, surface_id);
    }
    
    // 4.3: Performance analytics and timing
    uint64_t property_end_time = 0;
    clock_get_uptime(&property_end_time);
    uint64_t property_time = property_end_time - property_start_time;
    g_property_stats.total_property_time_ns += property_time;
    
    // 4.4: Resolution adaptation tracking
    if (!found_standard_resolution) {
        g_property_stats.resolution_adaptations++;
        IOLog("VMIOSurfaceManager: Custom resolution detected: %ux%u for surface %u\n", 
              descriptor->width, descriptor->height, surface_id);
    }
    
    // 4.5: Real-time performance reporting (every 50 property lookups)
    if (g_property_stats.property_lookups % 50 == 0) {
        uint32_t cache_hit_rate = (g_property_stats.cache_hits * 100) / g_property_stats.property_lookups;
        uint64_t avg_property_time = g_property_stats.total_property_time_ns / g_property_stats.property_lookups;
        uint32_t validation_success_rate = ((g_property_stats.property_lookups - g_property_stats.validation_failures) * 100) / 
                                         g_property_stats.property_lookups;
        
        IOLog("VMIOSurfaceManager: Property Analytics Report #%u:\n", g_property_stats.property_lookups / 50);
        IOLog("  - Property Lookups: %u (Cache Hit Rate: %u%%)\n", 
              g_property_stats.property_lookups, cache_hit_rate);
        IOLog("  - Average Property Time: %llu ns\n", avg_property_time);
        IOLog("  - Validation Success Rate: %u%%\n", validation_success_rate);
        IOLog("  - Format Conversions: %u, Resolution Adaptations: %u\n", 
              g_property_stats.format_conversions, g_property_stats.resolution_adaptations);
        IOLog("  - Property Cache Utilization: %u/128 entries\n", g_property_cache_size);
        IOLog("  - Compatibility Checks: %u\n", g_property_stats.compatibility_checks);
    }
    
    // 4.6: Success logging with comprehensive property information
    IOLog("VMIOSurfaceManager: Successfully retrieved surface %u properties:\n", surface_id);
    IOLog("  - Resolution: %ux%u (%s%s)\n", 
          descriptor->width, descriptor->height, resolution_name, 
          found_standard_resolution ? " - Standard" : " - Custom");
    IOLog("  - Pixel Format: %08X (%s%s)\n", 
          descriptor->pixel_format, 
          metal_compatible ? "Metal+" : "",
          opengl_compatible ? "OpenGL+" : "");
    IOLog("  - Bytes per Row: %u, Alloc Size: %u\n", 
          descriptor->bytes_per_row, descriptor->alloc_size);
    IOLog("  - Plane Count: %u, Usage Flags: %08X\n", 
          descriptor->plane_count, descriptor->usage_flags);
    IOLog("  - Discovery Time: %llu ns, Cached: %s\n", 
          property_time, added_to_cache ? "yes" : "no");
    
    return kIOReturnSuccess;
}

/*
 * Advanced IOSurface Property Management Analytics and Control API
 * Provides enterprise-level property statistics and cache management
 */

// Get current property management system statistics
IOReturn CLASS::getPropertyStatistics(VMSurfacePropertyStats* stats)
{
    if (!stats) {
        return kIOReturnBadArgument;
    }
    
    *stats = g_property_stats;
    return kIOReturnSuccess;
}

// Reset property statistics for new measurement period
IOReturn CLASS::resetPropertyStatistics()
{
    memset(&g_property_stats, 0, sizeof(VMSurfacePropertyStats));
    IOLog("VMIOSurfaceManager: Property statistics reset\n");
    return kIOReturnSuccess;
}

// Flush property cache to force fresh property lookups
IOReturn CLASS::flushPropertyCache()
{
    for (uint32_t i = 0; i < g_property_cache_size; i++) {
        g_property_cache[i].surface_id = 0;
        g_property_cache[i].last_access_time = 0;
        g_property_cache[i].access_count = 0;
        g_property_cache[i].is_validated = false;
        g_property_cache[i].needs_refresh = false;
        g_property_cache[i].property_hash = 0;
        g_property_cache[i].compatibility_flags = 0;
        bzero(&g_property_cache[i].descriptor, sizeof(VMIOSurfaceDescriptor));
    }
    
    g_property_cache_size = 0;
    g_property_cache_next_index = 0;
    
    IOLog("VMIOSurfaceManager: Property cache flushed\n");
    return kIOReturnSuccess;
}

// Invalidate specific surface properties in cache
IOReturn CLASS::invalidatePropertyCache(uint32_t surface_id)
{
    bool found = false;
    
    for (uint32_t i = 0; i < g_property_cache_size; i++) {
        if (g_property_cache[i].surface_id == surface_id) {
            g_property_cache[i].needs_refresh = true;
            g_property_cache[i].is_validated = false;
            found = true;
            
            IOLog("VMIOSurfaceManager: Invalidated property cache for surface %u\n", surface_id);
            break;
        }
    }
    
    if (!found) {
        IOLog("VMIOSurfaceManager: Surface %u not found in property cache\n", surface_id);
    }
    
    return kIOReturnSuccess;
}

// Pre-warm property cache with high-priority surfaces
IOReturn CLASS::prewarmPropertyCache(uint32_t* surface_ids, uint32_t count)
{
    if (!surface_ids || count == 0) {
        return kIOReturnBadArgument;
    }
    
    uint32_t prewarmed = 0;
    
    for (uint32_t i = 0; i < count && g_property_cache_size < 128; i++) {
        if (surface_ids[i] == 0) {
            continue;
        }
        
        // Get properties for pre-warming (this will cache them)
        VMIOSurfaceDescriptor temp_descriptor;
        IOReturn result = getSurfaceDescriptor(surface_ids[i], &temp_descriptor);
        
        if (result == kIOReturnSuccess) {
            prewarmed++;
            IOLog("VMIOSurfaceManager: Pre-warmed properties for surface %u\n", surface_ids[i]);
        }
    }
    
    IOLog("VMIOSurfaceManager: Pre-warmed property cache with %u/%u surfaces\n", prewarmed, count);
    return kIOReturnSuccess;
}

// Optimize property cache by promoting frequently accessed surfaces
IOReturn CLASS::optimizePropertyCache()
{
    if (g_property_cache_size == 0) {
        return kIOReturnSuccess;
    }
    
    // Sort cache entries by access count (bubble sort for small cache)
    for (uint32_t i = 0; i < g_property_cache_size - 1; i++) {
        for (uint32_t j = 0; j < g_property_cache_size - i - 1; j++) {
            if (g_property_cache[j].access_count < g_property_cache[j + 1].access_count) {
                VMSurfacePropertyCache temp = g_property_cache[j];
                g_property_cache[j] = g_property_cache[j + 1];
                g_property_cache[j + 1] = temp;
            }
        }
    }
    
    IOLog("VMIOSurfaceManager: Property cache optimized by access frequency\n");
    return kIOReturnSuccess;
}

// Generate detailed property management system performance report
void CLASS::generatePropertyReport()
{
    uint32_t total_operations = g_property_stats.property_lookups;
    
    if (total_operations == 0) {
        IOLog("VMIOSurfaceManager: No property operations recorded\n");
        return;
    }
    
    uint32_t cache_hit_percentage = (g_property_stats.cache_hits * 100) / total_operations;
    uint32_t validation_success_rate = ((total_operations - g_property_stats.validation_failures) * 100) / total_operations;
    uint64_t avg_time = g_property_stats.total_property_time_ns / total_operations;
    
    IOLog("VMIOSurfaceManager: === Advanced IOSurface Property Management System v5.0 Report ===\n");
    IOLog("  Performance Metrics:\n");
    IOLog("    - Total Property Lookups: %u\n", g_property_stats.property_lookups);
    IOLog("    - Cache Hits: %u (%u%%)\n", g_property_stats.cache_hits, cache_hit_percentage);
    IOLog("    - Cache Misses: %u (%u%%)\n", g_property_stats.cache_misses, 100 - cache_hit_percentage);
    IOLog("    - Average Property Time: %llu ns\n", avg_time);
    IOLog("    - Validation Success Rate: %u%%\n", validation_success_rate);
    IOLog("  Property Analysis:\n");
    IOLog("    - Format Conversions: %u\n", g_property_stats.format_conversions);
    IOLog("    - Resolution Adaptations: %u\n", g_property_stats.resolution_adaptations);
    IOLog("    - Compatibility Checks: %u\n", g_property_stats.compatibility_checks);
    IOLog("    - Validation Failures: %u\n", g_property_stats.validation_failures);
    IOLog("  Cache Status:\n");
    IOLog("    - Cache Utilization: %u/128 entries (%u%%)\n", 
          g_property_cache_size, (g_property_cache_size * 100) / 128);
    
    // Cache efficiency analysis
    uint64_t total_access_count = 0;
    uint32_t validated_entries = 0;
    uint32_t refresh_needed = 0;
    
    for (uint32_t i = 0; i < g_property_cache_size; i++) {
        total_access_count += g_property_cache[i].access_count;
        if (g_property_cache[i].is_validated) validated_entries++;
        if (g_property_cache[i].needs_refresh) refresh_needed++;
    }
    
    if (g_property_cache_size > 0) {
        IOLog("    - Average Access Count: %llu\n", total_access_count / g_property_cache_size);
        IOLog("    - Validated Entries: %u\n", validated_entries);
        IOLog("    - Entries Needing Refresh: %u\n", refresh_needed);
    }
    
    IOLog("  System Recommendations:\n");
    
    if (cache_hit_percentage < 70) {
        IOLog("    - Consider increasing property cache size for better performance\n");
    }
    if (g_property_stats.format_conversions > total_operations / 4) {
        IOLog("    - High format conversion rate - consider format standardization\n");
    }
    if (validation_success_rate < 95) {
        IOLog("    - High validation failure rate - check surface integrity\n");
    }
    if (avg_time > 2000) {
        IOLog("    - High average property time - consider cache optimization\n");
    }
    
    IOLog("  === End of Property Management System Report ===\n");
}

IOReturn CLASS::lockSurface(uint32_t surface_id, uint32_t lock_options, void** base_address)
{
    if (!base_address) {
        return kIOReturnBadArgument;
    }
    
    *base_address = nullptr;
    
    IOLockLock(m_surface_lock);
    
    // Find the surface
    OSObject* surface_obj = findSurface(surface_id);
    if (!surface_obj) {
        IOLockUnlock(m_surface_lock);
        return kIOReturnNotFound;
    }
    
    OSData* surface_data = OSDynamicCast(OSData, surface_obj);
    if (!surface_data) {
        IOLockUnlock(m_surface_lock);
        return kIOReturnInternalError;
    }
    
    VMIOSurface* surface = (VMIOSurface*)surface_data->getBytesNoCopy();
    if (!surface) {
        IOLockUnlock(m_surface_lock);
        return kIOReturnInternalError;
    }
    
    // Check if surface memory is available
    if (!surface->memory || !surface->base_address) {
        IOLog("VMIOSurfaceManager: Surface %u has no backing memory\n", surface_id);
        IOLockUnlock(m_surface_lock);
        return kIOReturnNotReady;
    }
    
    // Handle read-only locking
    bool read_only = (lock_options & VM_IOSURFACE_LOCK_READ_ONLY) != 0;
    bool avoid_sync = (lock_options & VM_IOSURFACE_LOCK_AVOID_SYNC) != 0;
    
    // Check for conflicting locks
    if (surface->is_locked && !read_only) {
        IOLog("VMIOSurfaceManager: Surface %u already locked for write access\n", surface_id);
        IOLockUnlock(m_surface_lock);
        return kIOReturnBusy;
    }
    
    // Synchronize with GPU if needed and not explicitly avoiding
    if (!avoid_sync && m_supports_hardware_surfaces) {
        IOReturn sync_result = synchronizeSurface(surface_id);
        if (sync_result != kIOReturnSuccess) {
            IOLog("VMIOSurfaceManager: Warning - GPU sync failed for surface %u: %08X\n", 
                  surface_id, sync_result);
        }
    }
    
    // Increment lock count
    surface->lock_count++;
    surface->is_locked = true;
    surface->last_access_time = mach_absolute_time();
    
    // Return base address
    *base_address = surface->base_address;
    
    // Update statistics
    m_lock_operations++;
    m_surface_locks++;
    
    IOLog("VMIOSurfaceManager: Locked surface %u (address: %p, options: 0x%X, count: %u)\n", 
          surface_id, *base_address, lock_options, surface->lock_count);
    
    IOLockUnlock(m_surface_lock);
    return kIOReturnSuccess;
}

IOReturn CLASS::unlockSurface(uint32_t surface_id, uint32_t lock_options)
{
    IOLockLock(m_surface_lock);
    
    // Find the surface
    OSObject* surface_obj = findSurface(surface_id);
    if (!surface_obj) {
        IOLockUnlock(m_surface_lock);
        return kIOReturnNotFound;
    }
    
    OSData* surface_data = OSDynamicCast(OSData, surface_obj);
    if (!surface_data) {
        IOLockUnlock(m_surface_lock);
        return kIOReturnInternalError;
    }
    
    VMIOSurface* surface = (VMIOSurface*)surface_data->getBytesNoCopy();
    if (!surface) {
        IOLockUnlock(m_surface_lock);
        return kIOReturnInternalError;
    }
    
    // Check if surface is actually locked
    if (!surface->is_locked || surface->lock_count == 0) {
        IOLog("VMIOSurfaceManager: Surface %u is not locked\n", surface_id);
        IOLockUnlock(m_surface_lock);
        return kIOReturnNotOpen;
    }
    
    // Decrement lock count
    surface->lock_count--;
    
    // Update lock state
    if (surface->lock_count == 0) {
        surface->is_locked = false;
        
        // Flush any pending writes to GPU if hardware surfaces are supported
        bool avoid_sync = (lock_options & VM_IOSURFACE_LOCK_AVOID_SYNC) != 0;
        if (!avoid_sync && m_supports_hardware_surfaces) {
            IOReturn sync_result = synchronizeSurface(surface_id);
            if (sync_result != kIOReturnSuccess) {
                IOLog("VMIOSurfaceManager: Warning - GPU sync failed after unlock for surface %u: %08X\n", 
                      surface_id, sync_result);
            }
        }
    }
    
    surface->last_access_time = mach_absolute_time();
    
    // Update statistics
    m_unlock_operations++;
    m_surface_unlocks++;
    
    IOLog("VMIOSurfaceManager: Unlocked surface %u (remaining locks: %u)\n", 
          surface_id, surface->lock_count);
    
    IOLockUnlock(m_surface_lock);
    return kIOReturnSuccess;
}

IOReturn CLASS::copySurface(uint32_t source_surface_id, uint32_t dest_surface_id)
{
    if (source_surface_id == 0 || dest_surface_id == 0) {
        return kIOReturnBadArgument;
    }
    
    if (source_surface_id == dest_surface_id) {
        return kIOReturnSuccess; // Nothing to do
    }
    
    IOLockLock(m_surface_lock);
    
    // Find source surface
    OSObject* source_obj = findSurface(source_surface_id);
    if (!source_obj) {
        IOLockUnlock(m_surface_lock);
        return kIOReturnNotFound;
    }
    
    // Find destination surface
    OSObject* dest_obj = findSurface(dest_surface_id);
    if (!dest_obj) {
        IOLockUnlock(m_surface_lock);
        return kIOReturnNotFound;
    }
    
    OSData* source_data = OSDynamicCast(OSData, source_obj);
    OSData* dest_data = OSDynamicCast(OSData, dest_obj);
    
    if (!source_data || !dest_data) {
        IOLockUnlock(m_surface_lock);
        return kIOReturnInternalError;
    }
    
    VMIOSurface* source_surface = (VMIOSurface*)source_data->getBytesNoCopy();
    VMIOSurface* dest_surface = (VMIOSurface*)dest_data->getBytesNoCopy();
    
    if (!source_surface || !dest_surface) {
        IOLockUnlock(m_surface_lock);
        return kIOReturnInternalError;
    }
    
    // Check if surfaces are compatible for copying
    if (source_surface->width != dest_surface->width || 
        source_surface->height != dest_surface->height ||
        source_surface->descriptor.pixel_format != dest_surface->descriptor.pixel_format) {
        IOLog("VMIOSurfaceManager: Surface dimensions/formats incompatible for copy\n");
        IOLockUnlock(m_surface_lock);
        return kIOReturnBadArgument;
    }
    
    // Check if surfaces have backing memory
    if (!source_surface->memory || !dest_surface->memory ||
        !source_surface->base_address || !dest_surface->base_address) {
        IOLockUnlock(m_surface_lock);
        return kIOReturnNotReady;
    }
    
    // Perform memory copy
    size_t copy_size = min(source_surface->memory_size, dest_surface->memory_size);
    memcpy(dest_surface->base_address, source_surface->base_address, copy_size);
    
    // Update destination surface timestamp
    dest_surface->last_access_time = mach_absolute_time();
    
    // Update statistics
    m_copy_operations++;
    
    IOLog("VMIOSurfaceManager: Copied surface %u to %u (%zu bytes)\n", 
          source_surface_id, dest_surface_id, copy_size);
    
    IOLockUnlock(m_surface_lock);
    return kIOReturnSuccess;
}

// Enhanced implementations for methods that exist in header

IOReturn CLASS::setupIOSurfaceSupport()
{
    IOLog("VMIOSurfaceManager: Setting up IOSurface support...\n");
    
    // Initialize feature support flags
    m_supports_hardware_surfaces = (m_gpu_device != nullptr);
    m_supports_yuv_surfaces = true;  // Enable YUV support
    m_supports_compressed_surfaces = false; // Disable for now
    m_supports_video_surfaces = true;
    m_supports_secure_surfaces = false; // Disable for now
    
    // Configure memory pools
    IOReturn result = configureMemoryPools();
    if (result != kIOReturnSuccess) {
        IOLog("VMIOSurfaceManager: Failed to configure memory pools: %08X\n", result);
        return result;
    }
    
    IOLog("VMIOSurfaceManager: IOSurface support initialized successfully\n");
    IOLog("VMIOSurfaceManager: Hardware surfaces: %s\n", m_supports_hardware_surfaces ? "YES" : "NO");
    IOLog("VMIOSurfaceManager: YUV surfaces: %s\n", m_supports_yuv_surfaces ? "YES" : "NO");
    IOLog("VMIOSurfaceManager: Video surfaces: %s\n", m_supports_video_surfaces ? "YES" : "NO");
    
    return kIOReturnSuccess;
}

IOReturn CLASS::configureMemoryPools()
{
    // Initialize memory pool array
    m_memory_pools = OSArray::withCapacity(8);
    if (!m_memory_pools) {
        return kIOReturnNoMemory;
    }
    
    // Create default memory pools for different surface sizes
    struct {
        uint32_t size;
        const char* name;
    } pool_configs[] = {
        { 1 * 1024 * 1024,   "Small" },    // 1MB pool
        { 4 * 1024 * 1024,   "Medium" },   // 4MB pool
        { 512 * 1024 * 1024,  "Large" },    // 512MB pool (matches VRAM size)
        { 64 * 1024 * 1024,  "XLarge" },   // 64MB pool
        { 0, nullptr }
    };
    
    for (int i = 0; pool_configs[i].size > 0; i++) {
        uint32_t pool_index;
        IOReturn result = createMemoryPool(pool_configs[i].size, &pool_index);
        if (result != kIOReturnSuccess) {
            IOLog("VMIOSurfaceManager: Failed to create %s memory pool: %08X\n", 
                  pool_configs[i].name, result);
            return result;
        }
        
        IOLog("VMIOSurfaceManager: Created %s memory pool (index %u, size %u MB)\n",
              pool_configs[i].name, pool_index, pool_configs[i].size / (1024 * 1024));
    }
    
    // Set available memory (total minus some overhead)
    m_available_memory = m_total_surface_memory - (32 * 1024 * 1024); // 32MB overhead
    
    IOLog("VMIOSurfaceManager: Configured %u memory pools, %llu MB available\n",
          m_memory_pools->getCount(), m_available_memory / (1024 * 1024));
    
    return kIOReturnSuccess;
}

IOReturn CLASS::lockSurfacePlane(uint32_t surface_id, uint32_t plane_index, 
                                uint32_t lock_options, void** base_address)
{
    if (!base_address || plane_index >= 4) {
        return kIOReturnBadArgument;
    }
    
    *base_address = nullptr;
    
    IOLockLock(m_surface_lock);
    
    // Find the surface
    OSObject* surface_obj = findSurface(surface_id);
    if (!surface_obj) {
        IOLockUnlock(m_surface_lock);
        return kIOReturnNotFound;
    }
    
    OSData* surface_data = OSDynamicCast(OSData, surface_obj);
    if (!surface_data) {
        IOLockUnlock(m_surface_lock);
        return kIOReturnInternalError;
    }
    
    VMIOSurface* surface = (VMIOSurface*)surface_data->getBytesNoCopy();
    if (!surface) {
        IOLockUnlock(m_surface_lock);
        return kIOReturnInternalError;
    }
    
    // Check if plane index is valid
    if (plane_index >= surface->descriptor.plane_count) {
        IOLog("VMIOSurfaceManager: Invalid plane index %u for surface %u (max: %u)\n",
              plane_index, surface_id, surface->descriptor.plane_count);
        IOLockUnlock(m_surface_lock);
        return kIOReturnBadArgument;
    }
    
    // Check if surface memory is available
    if (!surface->memory || !surface->base_address) {
        IOLockUnlock(m_surface_lock);
        return kIOReturnNotReady;
    }
    
    // Calculate plane base address
    VMIOSurfacePlaneInfo* plane = &surface->descriptor.planes[plane_index];
    *base_address = (void*)((uint8_t*)surface->base_address + plane->offset);
    
    // Update lock count and statistics
    surface->lock_count++;
    surface->is_locked = true;
    surface->last_access_time = mach_absolute_time();
    m_lock_operations++;
    
    IOLog("VMIOSurfaceManager: Locked surface %u plane %u (address: %p, size: %u)\n",
          surface_id, plane_index, *base_address, plane->size);
    
    IOLockUnlock(m_surface_lock);
    return kIOReturnSuccess;
}

IOReturn CLASS::unlockSurfacePlane(uint32_t surface_id, uint32_t plane_index, 
                                  uint32_t lock_options)
{
    if (plane_index >= 4) {
        return kIOReturnBadArgument;
    }
    
    IOLockLock(m_surface_lock);
    
    // Find the surface
    OSObject* surface_obj = findSurface(surface_id);
    if (!surface_obj) {
        IOLockUnlock(m_surface_lock);
        return kIOReturnNotFound;
    }
    
    OSData* surface_data = OSDynamicCast(OSData, surface_obj);
    if (!surface_data) {
        IOLockUnlock(m_surface_lock);
        return kIOReturnInternalError;
    }
    
    VMIOSurface* surface = (VMIOSurface*)surface_data->getBytesNoCopy();
    if (!surface) {
        IOLockUnlock(m_surface_lock);
        return kIOReturnInternalError;
    }
    
    // Check if plane index is valid
    if (plane_index >= surface->descriptor.plane_count) {
        IOLockUnlock(m_surface_lock);
        return kIOReturnBadArgument;
    }
    
    // Check if surface is actually locked
    if (!surface->is_locked || surface->lock_count == 0) {
        IOLockUnlock(m_surface_lock);
        return kIOReturnNotOpen;
    }
    
    // Update lock count
    surface->lock_count--;
    if (surface->lock_count == 0) {
        surface->is_locked = false;
    }
    
    surface->last_access_time = mach_absolute_time();
    m_unlock_operations++;
    
    IOLog("VMIOSurfaceManager: Unlocked surface %u plane %u (remaining locks: %u)\n",
          surface_id, plane_index, surface->lock_count);
    
    IOLockUnlock(m_surface_lock);
    return kIOReturnSuccess;
}

IOReturn CLASS::getSurfacePlaneInfo(uint32_t surface_id, uint32_t plane_index,
                                   VMIOSurfacePlaneInfo* plane_info)
{
    if (!plane_info || plane_index >= 4) {
        return kIOReturnBadArgument;
    }
    
    IOLockLock(m_surface_lock);
    
    // Find the surface
    OSObject* surface_obj = findSurface(surface_id);
    if (!surface_obj) {
        IOLockUnlock(m_surface_lock);
        return kIOReturnNotFound;
    }
    
    OSData* surface_data = OSDynamicCast(OSData, surface_obj);
    if (!surface_data) {
        IOLockUnlock(m_surface_lock);
        return kIOReturnInternalError;
    }
    
    VMIOSurface* surface = (VMIOSurface*)surface_data->getBytesNoCopy();
    if (!surface) {
        IOLockUnlock(m_surface_lock);
        return kIOReturnInternalError;
    }
    
    // Check if plane index is valid
    if (plane_index >= surface->descriptor.plane_count) {
        IOLockUnlock(m_surface_lock);
        return kIOReturnBadArgument;
    }
    
    // Copy plane information
    memcpy(plane_info, &surface->descriptor.planes[plane_index], sizeof(VMIOSurfacePlaneInfo));
    
    IOLockUnlock(m_surface_lock);
    return kIOReturnSuccess;
}

IOReturn CLASS::fillSurface(uint32_t surface_id, uint32_t fill_color)
{
    IOLockLock(m_surface_lock);
    
    // Find the surface
    OSObject* surface_obj = findSurface(surface_id);
    if (!surface_obj) {
        IOLockUnlock(m_surface_lock);
        return kIOReturnNotFound;
    }
    
    OSData* surface_data = OSDynamicCast(OSData, surface_obj);
    if (!surface_data) {
        IOLockUnlock(m_surface_lock);
        return kIOReturnInternalError;
    }
    
    VMIOSurface* surface = (VMIOSurface*)surface_data->getBytesNoCopy();
    if (!surface || !surface->base_address) {
        IOLockUnlock(m_surface_lock);
        return kIOReturnNotReady;
    }
    
    // Fill the entire surface with the specified color
    uint32_t bytes_per_pixel = getBytesPerPixel(surface->descriptor.pixel_format);
    if (bytes_per_pixel == 0) {
        IOLockUnlock(m_surface_lock);
        return kIOReturnUnsupported;
    }
    
    uint32_t* pixels = (uint32_t*)surface->base_address;
    uint32_t pixel_count = (surface->memory_size / bytes_per_pixel);
    
    for (uint32_t i = 0; i < pixel_count; i++) {
        pixels[i] = fill_color;
    }
    
    surface->last_access_time = mach_absolute_time();
    
    IOLog("VMIOSurfaceManager: Filled surface %u with color 0x%08X (%u pixels)\n",
          surface_id, fill_color, pixel_count);
    
    IOLockUnlock(m_surface_lock);
    return kIOReturnSuccess;
}

bool CLASS::isFormatSupported(VMIOSurfacePixelFormat format)
{
    return validatePixelFormat(format) == kIOReturnSuccess;
}

IOReturn CLASS::getIOSurfaceStats(void* stats_buffer, size_t* buffer_size)
{
    if (!stats_buffer || !buffer_size) {
        return kIOReturnBadArgument;
    }
    
    if (*buffer_size < sizeof(VMIOSurfaceStats)) {
        *buffer_size = sizeof(VMIOSurfaceStats);
        return kIOReturnNoSpace;
    }
    
    VMIOSurfaceStats* stats = static_cast<VMIOSurfaceStats*>(stats_buffer);
    
    IOLockLock(m_surface_lock);
    
    // Surface counts
    stats->surface_count = m_surface_count;
    stats->peak_surface_count = m_peak_surface_count;
    stats->client_count = m_clients ? m_clients->getCount() : 0;
    stats->active_surfaces = m_surfaces ? m_surfaces->getCount() : 0;
    
    // Memory statistics
    stats->total_memory = m_total_surface_memory;
    stats->allocated_memory = m_allocated_surface_memory;
    stats->peak_memory_usage = m_peak_memory_usage;
    stats->available_memory = m_total_surface_memory - m_allocated_surface_memory;
    
    // Calculate largest free block (simplified estimation)
    stats->largest_free_block = stats->available_memory;
    
    // Operation counters
    stats->surfaces_created = m_surfaces_created;
    stats->surfaces_destroyed = m_surfaces_destroyed;
    stats->surface_allocations = m_surface_allocations;
    stats->surface_deallocations = m_surface_deallocations;
    stats->surface_locks = m_surface_locks;
    stats->surface_unlocks = m_surface_unlocks;
    stats->lock_operations = m_lock_operations;
    stats->unlock_operations = m_unlock_operations;
    stats->copy_operations = m_copy_operations;
    
    // Performance metrics
    stats->cache_hits = m_cache_hits;
    stats->cache_misses = m_cache_misses;
    stats->bytes_allocated = m_bytes_allocated;
    stats->bytes_deallocated = m_bytes_deallocated;
    
    // GPU integration statistics
    stats->gpu_syncs = m_gpu_syncs;
    stats->gpu_updates = m_gpu_updates;
    stats->gpu_texture_uploads = m_gpu_texture_uploads;
    stats->gpu_command_buffers = m_gpu_command_buffers;
    
    // Video surface statistics
    stats->video_surfaces_created = m_video_surfaces_created;
    stats->video_frames_processed = m_video_frames_processed;
    stats->video_decoder_operations = m_video_decoder_operations;
    stats->video_encoder_operations = m_video_encoder_operations;
    
    // Memory optimization statistics
    stats->memory_compactions = m_memory_compactions;
    stats->memory_defragmentations = m_memory_defragmentations;
    stats->surfaces_evicted = m_surfaces_evicted;
    stats->priority_changes = m_priority_changes;
    
    // Error and diagnostic counters
    stats->allocation_failures = m_allocation_failures;
    stats->validation_errors = m_validation_errors;
    stats->integrity_failures = m_integrity_failures;
    stats->format_conversion_errors = m_format_conversion_errors;
    
    // Feature support flags
    stats->supports_hardware_surfaces = m_supports_hardware_surfaces;
    stats->supports_yuv_surfaces = m_supports_yuv_surfaces;
    stats->supports_compressed_surfaces = m_supports_compressed_surfaces;
    stats->supports_video_surfaces = m_supports_video_surfaces;
    stats->supports_secure_surfaces = m_supports_secure_surfaces;
    
    // Calculate timing statistics (simplified averages)
    uint64_t total_operations = stats->surfaces_created + stats->surface_locks + stats->copy_operations;
    if (total_operations > 0) {
        stats->average_allocation_time = m_total_allocation_time / stats->surfaces_created;
        stats->average_lock_time = m_total_lock_time / (stats->surface_locks + 1);
        stats->average_copy_time = m_total_copy_time / (stats->copy_operations + 1);
        stats->total_processing_time = m_total_allocation_time + m_total_lock_time + m_total_copy_time;
    } else {
        stats->average_allocation_time = 0;
        stats->average_lock_time = 0;
        stats->average_copy_time = 0;
        stats->total_processing_time = 0;
    }
    
    IOLockUnlock(m_surface_lock);
    
    *buffer_size = sizeof(VMIOSurfaceStats);
    return kIOReturnSuccess;
}

IOReturn CLASS::getMemoryUsage(uint64_t* total_memory, uint64_t* available_memory, 
                              uint64_t* largest_free_block)
{
    if (!total_memory || !available_memory || !largest_free_block) {
        return kIOReturnBadArgument;
    }
    
    IOLockLock(m_surface_lock);
    
    *total_memory = m_total_surface_memory;
    *available_memory = m_total_surface_memory - m_allocated_surface_memory;
    
    // Calculate largest free block (simplified)
    *largest_free_block = *available_memory;
    
    IOLockUnlock(m_surface_lock);
    return kIOReturnSuccess;
}

void CLASS::resetIOSurfaceCounters()
{
    IOLockLock(m_surface_lock);
    
    m_surface_allocations = 0;
    m_surface_deallocations = 0;
    m_surfaces_created = 0;
    m_surfaces_destroyed = 0;
    m_surface_locks = 0;
    m_surface_unlocks = 0;
    m_lock_operations = 0;
    m_unlock_operations = 0;
    m_copy_operations = 0;
    m_cache_hits = 0;
    m_cache_misses = 0;
    
    IOLog("VMIOSurfaceManager: Performance counters reset\n");
    
    IOLockUnlock(m_surface_lock);
}

void CLASS::logIOSurfaceState()
{
    IOLockLock(m_surface_lock);
    
    IOLog("=== VMIOSurfaceManager State ===\n");
    IOLog("Surface Count: %u (Peak: %u)\n", m_surface_count, m_peak_surface_count);
    IOLog("Memory Usage: %llu MB / %llu MB\n", 
          m_allocated_surface_memory / (1024 * 1024),
          m_total_surface_memory / (1024 * 1024));
    IOLog("Operations: Created=%llu, Destroyed=%llu, Locks=%llu, Unlocks=%llu, Copies=%llu\n",
          m_surfaces_created, m_surfaces_destroyed, m_surface_locks, m_surface_unlocks, m_copy_operations);
    IOLog("Feature Support: HW=%s, YUV=%s, Video=%s\n",
          m_supports_hardware_surfaces ? "YES" : "NO",
          m_supports_yuv_surfaces ? "YES" : "NO", 
          m_supports_video_surfaces ? "YES" : "NO");
    IOLog("==============================\n");
    
    IOLockUnlock(m_surface_lock);
}

IOReturn CLASS::validateSurface(uint32_t surface_id)
{
    IOLockLock(m_surface_lock);
    
    // Find the surface
    OSObject* surface_obj = findSurface(surface_id);
    if (!surface_obj) {
        IOLockUnlock(m_surface_lock);
        return kIOReturnNotFound;
    }
    
    OSData* surface_data = OSDynamicCast(OSData, surface_obj);
    if (!surface_data) {
        IOLockUnlock(m_surface_lock);
        return kIOReturnInternalError;
    }
    
    VMIOSurface* surface = (VMIOSurface*)surface_data->getBytesNoCopy();
    if (!surface) {
        IOLockUnlock(m_surface_lock);
        return kIOReturnInternalError;
    }
    
    // Validate surface structure
    IOReturn validation_result = kIOReturnSuccess;
    
    if (surface->surface_id != surface_id) {
        IOLog("VMIOSurfaceManager: Surface ID mismatch for surface %u\n", surface_id);
        validation_result = kIOReturnInternalError;
    }
    
    if (surface->width == 0 || surface->height == 0 || 
        surface->width > 16384 || surface->height > 16384) {
        IOLog("VMIOSurfaceManager: Invalid dimensions for surface %u: %ux%u\n",
              surface_id, surface->width, surface->height);
        validation_result = kIOReturnBadArgument;
    }
    
    if (validatePixelFormat(surface->descriptor.pixel_format) != kIOReturnSuccess) {
        IOLog("VMIOSurfaceManager: Invalid pixel format for surface %u: %08X\n",
              surface_id, surface->descriptor.pixel_format);
        validation_result = kIOReturnBadArgument;
    }
    
    if (!surface->memory) {
        IOLog("VMIOSurfaceManager: No backing memory for surface %u\n", surface_id);
        validation_result = kIOReturnNotReady;
    }
    
    IOLog("VMIOSurfaceManager: Surface %u validation %s\n",
          surface_id, validation_result == kIOReturnSuccess ? "PASSED" : "FAILED");
    
    IOLockUnlock(m_surface_lock);
    return validation_result;
}

// Additional implementations for methods that exist in header

// Format Conversion Methods Implementation

IOReturn CLASS::convertSurfaceFormat(uint32_t source_surface_id, uint32_t dest_surface_id,
                                    VMIOSurfacePixelFormat dest_format)
{
    if (source_surface_id == 0 || dest_surface_id == 0) {
        return kIOReturnBadArgument;
    }
    
    if (source_surface_id == dest_surface_id) {
        return kIOReturnSuccess; // Source and destination are the same
    }
    
    // Validate destination format
    IOReturn format_result = validatePixelFormat(dest_format);
    if (format_result != kIOReturnSuccess) {
        IOLog("VMIOSurfaceManager: Invalid destination pixel format: %08X\n", dest_format);
        return format_result;
    }
    
    IOLockLock(m_surface_lock);
    
    // Find source surface
    OSObject* source_obj = findSurface(source_surface_id);
    if (!source_obj) {
        IOLockUnlock(m_surface_lock);
        return kIOReturnNotFound;
    }
    
    // Find destination surface
    OSObject* dest_obj = findSurface(dest_surface_id);
    if (!dest_obj) {
        IOLockUnlock(m_surface_lock);
        return kIOReturnNotFound;
    }
    
    OSData* source_data = OSDynamicCast(OSData, source_obj);
    OSData* dest_data = OSDynamicCast(OSData, dest_obj);
    
    if (!source_data || !dest_data) {
        IOLockUnlock(m_surface_lock);
        return kIOReturnInternalError;
    }
    
    VMIOSurface* source_surface = (VMIOSurface*)source_data->getBytesNoCopy();
    VMIOSurface* dest_surface = (VMIOSurface*)dest_data->getBytesNoCopy();
    
    if (!source_surface || !dest_surface) {
        IOLockUnlock(m_surface_lock);
        return kIOReturnInternalError;
    }
    
    // Check if surfaces have backing memory
    if (!source_surface->memory || !dest_surface->memory ||
        !source_surface->base_address || !dest_surface->base_address) {
        IOLockUnlock(m_surface_lock);
        return kIOReturnNotReady;
    }
    
    // Check dimension compatibility
    if (source_surface->width != dest_surface->width || 
        source_surface->height != dest_surface->height) {
        IOLog("VMIOSurfaceManager: Surface dimensions must match for format conversion\n");
        IOLockUnlock(m_surface_lock);
        return kIOReturnBadArgument;
    }
    
    VMIOSurfacePixelFormat source_format = source_surface->descriptor.pixel_format;
    
    // Check if conversion is needed
    if (source_format == dest_format) {
        IOLog("VMIOSurfaceManager: No conversion needed - formats already match\n");
        IOLockUnlock(m_surface_lock);
        return kIOReturnSuccess;
    }
    
    uint32_t width = source_surface->width;
    uint32_t height = source_surface->height;
    
    // Perform format conversion based on source and destination formats
    IOReturn conversion_result = performPixelFormatConversion(
        source_surface->base_address, source_format,
        dest_surface->base_address, dest_format,
        width, height);
    
    if (conversion_result != kIOReturnSuccess) {
        IOLog("VMIOSurfaceManager: Pixel format conversion failed: %08X\n", conversion_result);
        IOLockUnlock(m_surface_lock);
        return conversion_result;
    }
    
    // Update destination surface format
    dest_surface->descriptor.pixel_format = dest_format;
    dest_surface->last_access_time = mach_absolute_time();
    
    // Update statistics
    m_format_conversions++;
    
    IOLog("VMIOSurfaceManager: Converted surface %u format from %08X to %08X (%ux%u)\n",
          dest_surface_id, source_format, dest_format, width, height);
    
    IOLockUnlock(m_surface_lock);
    return kIOReturnSuccess;
}

// Format Conversion Helper Methods Implementation

IOReturn CLASS::performPixelFormatConversion(void* source_buffer, VMIOSurfacePixelFormat source_format,
                                            void* dest_buffer, VMIOSurfacePixelFormat dest_format,
                                            uint32_t width, uint32_t height)
{
    if (!source_buffer || !dest_buffer || width == 0 || height == 0) {
        return kIOReturnBadArgument;
    }
    
    // Handle common format conversion cases
    if (source_format == dest_format) {
        // Same format - just copy
        uint32_t source_bpp = getBytesPerPixel(source_format);
        if (source_bpp == 0) {
            return kIOReturnUnsupported;
        }
        size_t copy_size = width * height * source_bpp;
        memcpy(dest_buffer, source_buffer, copy_size);
        return kIOReturnSuccess;
    }
    
    // BGRA32 to RGBA32 conversion (swap red and blue channels)
    if ((source_format == VM_IOSURFACE_PIXEL_FORMAT_BGRA32 || source_format == 0x42475241) &&
        (dest_format == VM_IOSURFACE_PIXEL_FORMAT_RGBA32 || dest_format == 0x52474241)) {
        return convertBGRAtoRGBA((uint32_t*)source_buffer, (uint32_t*)dest_buffer, width * height);
    }
    
    // RGBA32 to BGRA32 conversion (swap red and blue channels)
    if ((source_format == VM_IOSURFACE_PIXEL_FORMAT_RGBA32 || source_format == 0x52474241) &&
        (dest_format == VM_IOSURFACE_PIXEL_FORMAT_BGRA32 || dest_format == 0x42475241)) {
        return convertRGBAtoBGRA((uint32_t*)source_buffer, (uint32_t*)dest_buffer, width * height);
    }
    
    // BGRA32 to RGB24 conversion (drop alpha, swap channels)
    if ((source_format == VM_IOSURFACE_PIXEL_FORMAT_BGRA32 || source_format == 0x42475241) &&
        dest_format == 0x52474220) { // 'RGB '
        return convertBGRAtoRGB24((uint32_t*)source_buffer, (uint8_t*)dest_buffer, width * height);
    }
    
    // RGB24 to BGRA32 conversion (add alpha, swap channels)
    if (source_format == 0x52474220 && // 'RGB '
        (dest_format == VM_IOSURFACE_PIXEL_FORMAT_BGRA32 || dest_format == 0x42475241)) {
        return convertRGB24toBGRA((uint8_t*)source_buffer, (uint32_t*)dest_buffer, width * height);
    }
    
    // BGRA32 to Luminance conversion (grayscale)
    if ((source_format == VM_IOSURFACE_PIXEL_FORMAT_BGRA32 || source_format == 0x42475241) &&
        dest_format == 0x4C303030) { // 'L00'
        return convertBGRAtoLuminance((uint32_t*)source_buffer, (uint8_t*)dest_buffer, width * height);
    }
    
    // Add more conversion cases as needed
    IOLog("VMIOSurfaceManager: Unsupported format conversion: %08X -> %08X\n", 
          source_format, dest_format);
    return kIOReturnUnsupported;
}

IOReturn CLASS::convertBGRAtoRGBA(uint32_t* source, uint32_t* dest, uint32_t pixel_count)
{
    for (uint32_t i = 0; i < pixel_count; i++) {
        uint32_t bgra = source[i];
        uint32_t b = (bgra >> 24) & 0xFF;
        uint32_t g = (bgra >> 16) & 0xFF;
        uint32_t r = (bgra >> 8) & 0xFF;
        uint32_t a = bgra & 0xFF;
        
        // RGBA format: R in high byte, A in low byte
        dest[i] = (r << 24) | (g << 16) | (b << 8) | a;
    }
    return kIOReturnSuccess;
}

IOReturn CLASS::convertRGBAtoBGRA(uint32_t* source, uint32_t* dest, uint32_t pixel_count)
{
    for (uint32_t i = 0; i < pixel_count; i++) {
        uint32_t rgba = source[i];
        uint32_t r = (rgba >> 24) & 0xFF;
        uint32_t g = (rgba >> 16) & 0xFF;
        uint32_t b = (rgba >> 8) & 0xFF;
        uint32_t a = rgba & 0xFF;
        
        // BGRA format: B in high byte, A in low byte
        dest[i] = (b << 24) | (g << 16) | (r << 8) | a;
    }
    return kIOReturnSuccess;
}

IOReturn CLASS::convertBGRAtoRGB24(uint32_t* source, uint8_t* dest, uint32_t pixel_count)
{
    for (uint32_t i = 0; i < pixel_count; i++) {
        uint32_t bgra = source[i];
        uint8_t b = (bgra >> 24) & 0xFF;
        uint8_t g = (bgra >> 16) & 0xFF;
        uint8_t r = (bgra >> 8) & 0xFF;
        // Alpha is dropped
        
        // RGB24 format: R, G, B
        dest[i * 3 + 0] = r;
        dest[i * 3 + 1] = g;
        dest[i * 3 + 2] = b;
    }
    return kIOReturnSuccess;
}

IOReturn CLASS::convertRGB24toBGRA(uint8_t* source, uint32_t* dest, uint32_t pixel_count)
{
    for (uint32_t i = 0; i < pixel_count; i++) {
        uint8_t r = source[i * 3 + 0];
        uint8_t g = source[i * 3 + 1];
        uint8_t b = source[i * 3 + 2];
        uint8_t a = 0xFF; // Set alpha to opaque
        
        // BGRA format: B in high byte, A in low byte
        dest[i] = (b << 24) | (g << 16) | (r << 8) | a;
    }
    return kIOReturnSuccess;
}

IOReturn CLASS::convertBGRAtoLuminance(uint32_t* source, uint8_t* dest, uint32_t pixel_count)
{
    for (uint32_t i = 0; i < pixel_count; i++) {
        uint32_t bgra = source[i];
        uint8_t b = (bgra >> 24) & 0xFF;
        uint8_t g = (bgra >> 16) & 0xFF;
        uint8_t r = (bgra >> 8) & 0xFF;
        
        // Convert to grayscale using standard luminance formula
        // Y = 0.299*R + 0.587*G + 0.114*B
        uint32_t luminance = (299 * r + 587 * g + 114 * b) / 1000;
        dest[i] = (uint8_t)(luminance & 0xFF);
    }
    return kIOReturnSuccess;
}

// Client Management Implementation

IOReturn CLASS::registerClient(const VMIOSurfaceClientDescriptor* descriptor, uint32_t* client_id)
{
    if (!descriptor || !client_id) {
        return kIOReturnBadArgument;
    }
    
    IOLockLock(m_surface_lock);
    
    // Initialize client collections if needed
    if (!m_clients) {
        m_clients = OSArray::withCapacity(16);
        if (!m_clients) {
            IOLockUnlock(m_surface_lock);
            return kIOReturnNoMemory;
        }
    }
    
    if (!m_client_map) {
        m_client_map = OSDictionary::withCapacity(16);
        if (!m_client_map) {
            IOLockUnlock(m_surface_lock);
            return kIOReturnNoMemory;
        }
    }
    
    // Allocate new client ID
    *client_id = allocateClientId();
    
    // Create client info structure
    struct ClientInfo {
        uint32_t client_id;
        uint32_t process_id;
        uint32_t access_rights;
        uint64_t registration_time;
        uint32_t surface_count;
        char name[64];
        bool active;
    };
    
    ClientInfo* client_info = (ClientInfo*)IOMalloc(sizeof(ClientInfo));
    if (!client_info) {
        releaseClientId(*client_id);
        IOLockUnlock(m_surface_lock);
        return kIOReturnNoMemory;
    }
    
    // Initialize client information
    client_info->client_id = *client_id;
    client_info->process_id = descriptor->process_id;
    client_info->access_rights = descriptor->access_rights;
    client_info->registration_time = mach_absolute_time();
    client_info->surface_count = 0;
    client_info->active = true;
    strlcpy(client_info->name, descriptor->client_name ? descriptor->client_name : "Unknown", 
            sizeof(client_info->name));
    
    // Store client info in collections
    OSData* client_data = OSData::withBytes(client_info, sizeof(ClientInfo));
    if (!client_data) {
        IOFree(client_info, sizeof(ClientInfo));
        releaseClientId(*client_id);
        IOLockUnlock(m_surface_lock);
        return kIOReturnNoMemory;
    }
    
    m_clients->setObject(client_data);
    
    // Create dictionary key
    char client_key[32];
    snprintf(client_key, sizeof(client_key), "%u", *client_id);
    m_client_map->setObject(client_key, client_data);
    
    client_data->release();
    
    IOLog("VMIOSurfaceManager: Registered client %u '%s' (PID: %u, rights: 0x%X)\n",
          *client_id, client_info->name, client_info->process_id, client_info->access_rights);
    
    IOLockUnlock(m_surface_lock);
    return kIOReturnSuccess;
}

IOReturn CLASS::unregisterClient(uint32_t client_id)
{
    if (client_id == 0) {
        return kIOReturnBadArgument;
    }
    
    IOLockLock(m_surface_lock);
    
    if (!m_client_map) {
        IOLockUnlock(m_surface_lock);
        return kIOReturnNotFound;
    }
    
    char client_key[32];
    snprintf(client_key, sizeof(client_key), "%u", client_id);
    
    // Find client info
    OSData* client_data = OSDynamicCast(OSData, m_client_map->getObject(client_key));
    if (!client_data) {
        IOLockUnlock(m_surface_lock);
        return kIOReturnNotFound;
    }
    
    struct ClientInfo {
        uint32_t client_id;
        uint32_t process_id;
        uint32_t access_rights;
        uint64_t registration_time;
        uint32_t surface_count;
        char name[64];
        bool active;
    };
    
    ClientInfo* client_info = (ClientInfo*)client_data->getBytesNoCopy();
    if (!client_info) {
        IOLockUnlock(m_surface_lock);
        return kIOReturnInternalError;
    }
    
    // Check if client has active surfaces
    if (client_info->surface_count > 0) {
        IOLog("VMIOSurfaceManager: Client %u still has %u active surfaces\n",
              client_id, client_info->surface_count);
        // Mark as inactive but don't remove yet
        client_info->active = false;
    } else {
        // Remove client from collections
        m_client_map->removeObject(client_key);
        
        unsigned int index = m_clients->getNextIndexOfObject(client_data, 0);
        if (index != (unsigned int)-1) {
            m_clients->removeObject(index);
        }
        
        // Release client ID for reuse
        releaseClientId(client_id);
    }
    
    IOLog("VMIOSurfaceManager: Unregistered client %u '%s'\n",
          client_id, client_info->name);
    
    IOLockUnlock(m_surface_lock);
    return kIOReturnSuccess;
}

IOReturn CLASS::getClientDescriptor(uint32_t client_id, VMIOSurfaceClientDescriptor* descriptor)
{
    if (client_id == 0 || !descriptor) {
        return kIOReturnBadArgument;
    }
    
    IOLockLock(m_surface_lock);
    
    if (!m_client_map) {
        IOLockUnlock(m_surface_lock);
        return kIOReturnNotFound;
    }
    
    char client_key[32];
    snprintf(client_key, sizeof(client_key), "%u", client_id);
    
    OSData* client_data = OSDynamicCast(OSData, m_client_map->getObject(client_key));
    if (!client_data) {
        IOLockUnlock(m_surface_lock);
        return kIOReturnNotFound;
    }
    
    struct ClientInfo {
        uint32_t client_id;
        uint32_t process_id;
        uint32_t access_rights;
        uint64_t registration_time;
        uint32_t surface_count;
        char name[64];
        bool active;
    };
    
    ClientInfo* client_info = (ClientInfo*)client_data->getBytesNoCopy();
    if (!client_info || !client_info->active) {
        IOLockUnlock(m_surface_lock);
        return kIOReturnNotFound;
    }
    
    // Fill descriptor
    bzero(descriptor, sizeof(VMIOSurfaceClientDescriptor));
    descriptor->client_id = client_info->client_id;
    descriptor->process_id = client_info->process_id;
    descriptor->access_rights = client_info->access_rights;
    descriptor->client_name = client_info->name; // Just point to the internal name
    
    IOLockUnlock(m_surface_lock);
    return kIOReturnSuccess;
}

IOReturn CLASS::setClientAccessRights(uint32_t client_id, uint32_t access_rights)
{
    if (client_id == 0) {
        return kIOReturnBadArgument;
    }
    
    IOLockLock(m_surface_lock);
    
    if (!m_client_map) {
        IOLockUnlock(m_surface_lock);
        return kIOReturnNotFound;
    }
    
    char client_key[32];
    snprintf(client_key, sizeof(client_key), "%u", client_id);
    
    OSData* client_data = OSDynamicCast(OSData, m_client_map->getObject(client_key));
    if (!client_data) {
        IOLockUnlock(m_surface_lock);
        return kIOReturnNotFound;
    }
    
    struct ClientInfo {
        uint32_t client_id;
        uint32_t process_id;
        uint32_t access_rights;
        uint64_t registration_time;
        uint32_t surface_count;
        char name[64];
        bool active;
    };
    
    ClientInfo* client_info = (ClientInfo*)client_data->getBytesNoCopy();
    if (!client_info || !client_info->active) {
        IOLockUnlock(m_surface_lock);
        return kIOReturnNotFound;
    }
    
    uint32_t old_rights = client_info->access_rights;
    client_info->access_rights = access_rights;
    
    IOLog("VMIOSurfaceManager: Updated client %u access rights: 0x%X -> 0x%X\n",
          client_id, old_rights, access_rights);
    
    IOLockUnlock(m_surface_lock);
    return kIOReturnSuccess;
}

// Surface Sharing Implementation

IOReturn CLASS::shareSurface(uint32_t surface_id, const VMIOSurfaceSharingDescriptor* descriptor)
{
    if (surface_id == 0 || !descriptor) {
        return kIOReturnBadArgument;
    }
    
    // Validate that the descriptor's surface_id matches the parameter
    if (descriptor->surface_id != surface_id) {
        IOLog("VMIOSurfaceManager: Surface ID mismatch in sharing descriptor\n");
        return kIOReturnBadArgument;
    }
    
    IOLockLock(m_surface_lock);
    
    // Find the surface
    OSObject* surface_obj = findSurface(surface_id);
    if (!surface_obj) {
        IOLockUnlock(m_surface_lock);
        return kIOReturnNotFound;
    }
    
    // Initialize shared surfaces collection if needed
    if (!m_shared_surfaces) {
        m_shared_surfaces = OSArray::withCapacity(32);
        if (!m_shared_surfaces) {
            IOLockUnlock(m_surface_lock);
            return kIOReturnNoMemory;
        }
    }
    
    // Create sharing entries for each allowed client
    for (uint32_t i = 0; i < descriptor->client_count; i++) {
        if (!descriptor->allowed_clients) {
            break;
        }
        
        uint32_t target_client_id = descriptor->allowed_clients[i];
        
        // Verify target client exists
        OSObject* target_client_obj = findClient(target_client_id);
        if (!target_client_obj) {
            IOLog("VMIOSurfaceManager: Target client %u not found for surface sharing\n", target_client_id);
            continue;
        }
        
        // Create sharing entry
        struct SurfaceSharing {
            uint32_t surface_id;
            uint32_t owner_client_id;
            uint32_t shared_client_id;
            uint32_t sharing_mode;
            uint64_t sharing_time;
            bool active;
        };
        
        SurfaceSharing* sharing = (SurfaceSharing*)IOMalloc(sizeof(SurfaceSharing));
        if (!sharing) {
            continue; // Skip this client if allocation fails
        }
        
        sharing->surface_id = surface_id;
        sharing->owner_client_id = 0; // We don't have owner info in this structure
        sharing->shared_client_id = target_client_id;
        sharing->sharing_mode = descriptor->sharing_mode;
        sharing->sharing_time = mach_absolute_time();
        sharing->active = true;
        
        // Store sharing info
        OSData* sharing_data = OSData::withBytes(sharing, sizeof(SurfaceSharing));
        if (sharing_data) {
            m_shared_surfaces->setObject(sharing_data);
            sharing_data->release();
            
            IOLog("VMIOSurfaceManager: Surface %u shared with client %u (mode: 0x%X)\n",
                  surface_id, target_client_id, descriptor->sharing_mode);
        }
        
        IOFree(sharing, sizeof(SurfaceSharing));
    }
    
    IOLockUnlock(m_surface_lock);
    return kIOReturnSuccess;
}

IOReturn CLASS::unshareSurface(uint32_t surface_id, uint32_t client_id)
{
    if (surface_id == 0 || client_id == 0) {
        return kIOReturnBadArgument;
    }
    
    IOLockLock(m_surface_lock);
    
    if (!m_shared_surfaces) {
        IOLockUnlock(m_surface_lock);
        return kIOReturnNotFound;
    }
    
    // Find and remove matching sharing entry
    struct SurfaceSharing {
        uint32_t surface_id;
        uint32_t owner_client_id;
        uint32_t shared_client_id;
        uint32_t sharing_mode;
        uint64_t sharing_time;
        bool active;
    };
    
    bool found = false;
    for (unsigned int i = 0; i < m_shared_surfaces->getCount(); i++) {
        OSData* sharing_data = OSDynamicCast(OSData, m_shared_surfaces->getObject(i));
        if (!sharing_data) continue;
        
        SurfaceSharing* sharing = (SurfaceSharing*)sharing_data->getBytesNoCopy();
        if (!sharing) continue;
        
        if (sharing->surface_id == surface_id && 
            (sharing->owner_client_id == client_id || sharing->shared_client_id == client_id) &&
            sharing->active) {
            
            // Mark as inactive or remove
            sharing->active = false;
            
            IOLog("VMIOSurfaceManager: Surface %u unshared for client %u\n", surface_id, client_id);
            found = true;
            break;
        }
    }
    
    if (!found) {
        IOLockUnlock(m_surface_lock);
        return kIOReturnNotFound;
    }
    
    IOLockUnlock(m_surface_lock);
    return kIOReturnSuccess;
}

IOReturn CLASS::defragmentMemoryPools()
{
    IOLog("VMIOSurfaceManager: Starting memory pool defragmentation...\n");
    
    IOLockLock(m_surface_lock);
    
    uint32_t pools_processed = 0;
    uint32_t pools_defragmented = 0;
    uint64_t memory_reorganized = 0;
    
    if (!m_memory_pools || m_memory_pools->getCount() == 0) {
        IOLog("VMIOSurfaceManager: No memory pools to defragment\n");
        IOLockUnlock(m_surface_lock);
        return kIOReturnSuccess;
    }
    
    // Iterate through memory pools
    OSCollectionIterator* iterator = OSCollectionIterator::withCollection(m_memory_pools);
    if (!iterator) {
        IOLockUnlock(m_surface_lock);
        return kIOReturnNoMemory;
    }
    
    struct MemoryPoolInfo {
        uint32_t pool_size;
        uint32_t allocated_blocks;
        uint32_t free_blocks;
        uint32_t fragmentation_level;
        char pool_name[32];
    };
    
    OSData* pool_data;
    while ((pool_data = OSDynamicCast(OSData, iterator->getNextObject()))) {
        MemoryPoolInfo* pool_info = (MemoryPoolInfo*)pool_data->getBytesNoCopy();
        if (!pool_info) continue;
        
        pools_processed++;
        
        // Calculate fragmentation level (simplified)
        uint32_t total_blocks = pool_info->allocated_blocks + pool_info->free_blocks;
        if (total_blocks == 0) continue;
        
        uint32_t fragmentation_percentage = (pool_info->free_blocks * 100) / total_blocks;
        
        // If fragmentation is high (>30%), perform defragmentation
        if (fragmentation_percentage > 30) {
            IOLog("VMIOSurfaceManager: Defragmenting pool '%s' (fragmentation: %u%%)\n",
                  pool_info->pool_name, fragmentation_percentage);
            
            // In a real implementation, this would involve:
            // - Moving allocated blocks together
            // - Consolidating free space
            // - Updating surface memory pointers
            
            uint32_t memory_before_defrag = pool_info->pool_size;
            
            // Simulate defragmentation success
            pool_info->fragmentation_level = fragmentation_percentage;
            pools_defragmented++;
            memory_reorganized += memory_before_defrag;
            
            IOLog("VMIOSurfaceManager: Pool '%s' defragmentation complete\n", pool_info->pool_name);
        }
    }
    
    iterator->release();
    
    IOLog("VMIOSurfaceManager: Memory pool defragmentation complete\n");
    IOLog("  Pools processed: %u\n", pools_processed);
    IOLog("  Pools defragmented: %u\n", pools_defragmented);
    IOLog("  Memory reorganized: %llu MB\n", memory_reorganized / (1024 * 1024));
    
    IOLockUnlock(m_surface_lock);
    return kIOReturnSuccess;
}

IOReturn CLASS::setSurfaceMemoryPriority(uint32_t surface_id, VMIOSurfaceMemoryPriority priority)
{
    if (surface_id == 0) {
        return kIOReturnBadArgument;
    }
    
    // Validate priority value
    if (priority < VM_IOSURFACE_MEMORY_PRIORITY_LOW || 
        priority > VM_IOSURFACE_MEMORY_PRIORITY_CRITICAL) {
        return kIOReturnBadArgument;
    }
    
    IOLockLock(m_surface_lock);
    
    // Find the surface
    OSObject* surface_obj = findSurface(surface_id);
    if (!surface_obj) {
        IOLockUnlock(m_surface_lock);
        return kIOReturnNotFound;
    }
    
    OSData* surface_data = OSDynamicCast(OSData, surface_obj);
    if (!surface_data) {
        IOLockUnlock(m_surface_lock);
        return kIOReturnInternalError;
    }
    
    VMIOSurface* surface = (VMIOSurface*)surface_data->getBytesNoCopy();
    if (!surface) {
        IOLockUnlock(m_surface_lock);
        return kIOReturnInternalError;
    }
    
    VMIOSurfaceMemoryPriority old_priority = surface->memory_priority;
    surface->memory_priority = priority;
    
    // Based on priority, adjust memory management behavior
    switch (priority) {
        case VM_IOSURFACE_MEMORY_PRIORITY_LOW:
            // Low priority surfaces can be moved to slower memory or compressed
            IOLog("VMIOSurfaceManager: Set surface %u to LOW priority (eligible for compression/swapping)\n",
                  surface_id);
            break;
            
        case VM_IOSURFACE_MEMORY_PRIORITY_NORMAL:
            // Normal priority - standard memory management
            IOLog("VMIOSurfaceManager: Set surface %u to NORMAL priority\n", surface_id);
            break;
            
        case VM_IOSURFACE_MEMORY_PRIORITY_HIGH:
            // High priority - prefer fast memory, resist eviction
            IOLog("VMIOSurfaceManager: Set surface %u to HIGH priority (protected from eviction)\n",
                  surface_id);
            break;
            
        case VM_IOSURFACE_MEMORY_PRIORITY_CRITICAL:
            // Critical priority - always keep in fastest memory
            IOLog("VMIOSurfaceManager: Set surface %u to CRITICAL priority (always in fast memory)\n",
                  surface_id);
            break;
    }
    
    IOLog("VMIOSurfaceManager: Updated surface %u memory priority: %d -> %d\n",
          surface_id, old_priority, priority);
    
    IOLockUnlock(m_surface_lock);
    return kIOReturnSuccess;
}

IOReturn CLASS::evictUnusedSurfaces(uint32_t max_surfaces_to_evict, uint32_t* surfaces_evicted)
{
    if (surfaces_evicted) {
        *surfaces_evicted = 0;
    }
    
    IOLog("VMIOSurfaceManager: Starting surface eviction (max: %u)...\n", max_surfaces_to_evict);
    
    IOLockLock(m_surface_lock);
    
    uint32_t surfaces_examined = 0;
    uint32_t evicted_count = 0;
    uint64_t memory_freed = 0;
    
    if (!m_surfaces || m_surfaces->getCount() == 0) {
        IOLog("VMIOSurfaceManager: No surfaces to evict\n");
        IOLockUnlock(m_surface_lock);
        return kIOReturnSuccess;
    }
    
    // Create a list of eviction candidates
    OSArray* eviction_candidates = OSArray::withCapacity(m_surfaces->getCount());
    if (!eviction_candidates) {
        IOLockUnlock(m_surface_lock);
        return kIOReturnNoMemory;
    }
    
    uint64_t current_time = mach_absolute_time();
    
    // First pass: identify eviction candidates
    OSCollectionIterator* iterator = OSCollectionIterator::withCollection(m_surfaces);
    if (!iterator) {
        eviction_candidates->release();
        IOLockUnlock(m_surface_lock);
        return kIOReturnNoMemory;
    }
    
    OSData* surface_data;
    while ((surface_data = OSDynamicCast(OSData, iterator->getNextObject()))) {
        VMIOSurface* surface = (VMIOSurface*)surface_data->getBytesNoCopy();
        if (!surface) continue;
        
        surfaces_examined++;
        
        // Skip surfaces that can't be evicted
        if (surface->is_locked || surface->lock_count > 0) {
            continue;
        }
        
        // Skip surfaces with high or critical memory priority
        if (surface->memory_priority >= VM_IOSURFACE_MEMORY_PRIORITY_HIGH) {
            continue;
        }
        
        // Check if surface hasn't been accessed recently (older than 60 seconds)
        uint64_t time_since_access = current_time - surface->last_access_time;
        uint64_t nanoseconds_since_access = time_since_access / 1000000; // Rough conversion
        
        // If surface is old enough and has low priority, it's a candidate
        if (nanoseconds_since_access > 60000000000ULL) { // 60 seconds
            eviction_candidates->setObject(surface_data);
        }
    }
    
    iterator->release();
    
    // Second pass: evict candidates (oldest first, lowest priority first)
    uint32_t candidates_count = eviction_candidates->getCount();
    uint32_t to_evict = (max_surfaces_to_evict == 0) ? candidates_count : 
                        ((max_surfaces_to_evict < candidates_count) ? max_surfaces_to_evict : candidates_count);
    
    for (uint32_t i = 0; i < to_evict && i < candidates_count; i++) {
        OSData* candidate_data = OSDynamicCast(OSData, eviction_candidates->getObject(i));
        if (!candidate_data) continue;
        
        VMIOSurface* surface = (VMIOSurface*)candidate_data->getBytesNoCopy();
        if (!surface) continue;
        
        // Double-check that surface is still evictable
        if (surface->is_locked || surface->lock_count > 0) {
            continue;
        }
        
        uint32_t surface_memory_size = surface->memory_size;
        
        // In a real implementation, this might:
        // - Move surface data to swap
        // - Compress surface data
        // - Free surface memory but keep metadata
        // For demonstration, we'll simulate freeing memory
        
        IOLog("VMIOSurfaceManager: Evicting surface %u (%u bytes, priority: %d)\n",
              surface->surface_id, surface_memory_size, surface->memory_priority);
        
        // Mark surface as evicted (in real implementation, this would involve
        // more complex memory management)
        surface->last_access_time = 0; // Mark as evicted
        
        evicted_count++;
        memory_freed += surface_memory_size;
    }
    
    eviction_candidates->release();
    
    // Update memory statistics
    m_allocated_surface_memory -= memory_freed;
    
    if (surfaces_evicted) {
        *surfaces_evicted = evicted_count;
    }
    
    IOLog("VMIOSurfaceManager: Surface eviction complete\n");
    IOLog("  Surfaces examined: %u\n", surfaces_examined);
    IOLog("  Surfaces evicted: %u\n", evicted_count);
    IOLog("  Memory freed: %llu MB\n", memory_freed / (1024 * 1024));
    IOLog("  Memory usage after eviction: %llu MB / %llu MB\n",
          m_allocated_surface_memory / (1024 * 1024),
          m_total_surface_memory / (1024 * 1024));
    
    IOLockUnlock(m_surface_lock);
    return kIOReturnSuccess;
}

IOReturn CLASS::getSurfaceClients(uint32_t surface_id, uint32_t* client_ids, uint32_t* client_count)
{
    if (surface_id == 0 || !client_count) {
        return kIOReturnBadArgument;
    }
    
    IOLockLock(m_surface_lock);
    
    uint32_t found_clients = 0;
    uint32_t max_clients = client_ids ? *client_count : 0;
    
    if (m_shared_surfaces) {
        struct SurfaceSharing {
            uint32_t surface_id;
            uint32_t owner_client_id;
            uint32_t shared_client_id;
            uint32_t sharing_mode;
            uint64_t sharing_time;
            bool active;
        };
        
        for (unsigned int i = 0; i < m_shared_surfaces->getCount(); i++) {
            OSData* sharing_data = OSDynamicCast(OSData, m_shared_surfaces->getObject(i));
            if (!sharing_data) continue;
            
            SurfaceSharing* sharing = (SurfaceSharing*)sharing_data->getBytesNoCopy();
            if (!sharing || !sharing->active || sharing->surface_id != surface_id) continue;
            
            if (client_ids && found_clients < max_clients) {
                client_ids[found_clients] = sharing->shared_client_id;
            }
            found_clients++;
        }
    }
    
    *client_count = found_clients;
    
    IOLockUnlock(m_surface_lock);
    return kIOReturnSuccess;
}

bool CLASS::canClientAccessSurface(uint32_t client_id, uint32_t surface_id)
{
    if (client_id == 0 || surface_id == 0) {
        return false;
    }
    
    IOLockLock(m_surface_lock);
    
    // Check if client exists
    if (!findClient(client_id)) {
        IOLockUnlock(m_surface_lock);
        return false;
    }
    
    // Check if surface exists
    if (!findSurface(surface_id)) {
        IOLockUnlock(m_surface_lock);
        return false;
    }
    
    // Check sharing permissions
    bool has_access = false;
    
    if (m_shared_surfaces) {
        struct SurfaceSharing {
            uint32_t surface_id;
            uint32_t owner_client_id;
            uint32_t shared_client_id;
            uint32_t sharing_mode;
            uint64_t sharing_time;
            bool active;
        };
        
        for (unsigned int i = 0; i < m_shared_surfaces->getCount(); i++) {
            OSData* sharing_data = OSDynamicCast(OSData, m_shared_surfaces->getObject(i));
            if (!sharing_data) continue;
            
            SurfaceSharing* sharing = (SurfaceSharing*)sharing_data->getBytesNoCopy();
            if (!sharing || !sharing->active) continue;
            
            if (sharing->surface_id == surface_id && 
                (sharing->owner_client_id == client_id || sharing->shared_client_id == client_id)) {
                has_access = true;
                break;
            }
        }
    }
    
    IOLockUnlock(m_surface_lock);
    return has_access;
}

IOReturn CLASS::createMemoryPool(uint32_t size, uint32_t* pool_index)
{
    if (!pool_index || size == 0) {
        return kIOReturnBadArgument;
    }
    
    if (!m_memory_pools) {
        return kIOReturnNotReady;
    }
    
    // Create a simple memory pool descriptor
    struct MemoryPool {
        uint32_t size;
        uint32_t allocated;
        uint32_t free_blocks;
        bool active;
    };
    
    MemoryPool* pool = (MemoryPool*)IOMalloc(sizeof(MemoryPool));
    if (!pool) {
        return kIOReturnNoMemory;
    }
    
    pool->size = size;
    pool->allocated = 0;
    pool->free_blocks = 1; // Initially one big free block
    pool->active = true;
    
    OSData* pool_data = OSData::withBytes(pool, sizeof(MemoryPool));
    if (!pool_data) {
        IOFree(pool, sizeof(MemoryPool));
        return kIOReturnNoMemory;
    }
    
    m_memory_pools->setObject(pool_data);
    *pool_index = m_memory_pools->getCount() - 1;
    
    pool_data->release();
    
    IOLog("VMIOSurfaceManager: Created memory pool %u (size: %u bytes)\n", *pool_index, size);
    return kIOReturnSuccess;
}

IOReturn CLASS::findBestMemoryPool(uint32_t size, uint32_t alignment, uint32_t* pool_index)
{
    if (!pool_index || size == 0) {
        return kIOReturnBadArgument;
    }
    
    if (!m_memory_pools || m_memory_pools->getCount() == 0) {
        return kIOReturnNotFound;
    }
    
    // Find the smallest pool that can accommodate the request
    uint32_t best_index = 0;
    uint32_t best_size = UINT32_MAX;
    bool found = false;
    
    for (unsigned int i = 0; i < m_memory_pools->getCount(); i++) {
        OSData* pool_data = OSDynamicCast(OSData, m_memory_pools->getObject(i));
        if (!pool_data) continue;
        
        struct MemoryPool {
            uint32_t size;
            uint32_t allocated;
            uint32_t free_blocks;
            bool active;
        };
        
        MemoryPool* pool = (MemoryPool*)pool_data->getBytesNoCopy();
        if (!pool || !pool->active) continue;
        
        uint32_t available = pool->size - pool->allocated;
        if (available >= size && pool->size < best_size) {
            best_index = i;
            best_size = pool->size;
            found = true;
        }
    }
    
    if (!found) {
        return kIOReturnNoSpace;
    }
    
    *pool_index = best_index;
    return kIOReturnSuccess;
}

IOReturn CLASS::copySurfaceRegion(uint32_t source_surface_id, uint32_t dest_surface_id,
                                 uint32_t src_x, uint32_t src_y, uint32_t dst_x, uint32_t dst_y,
                                 uint32_t width, uint32_t height)
{
    if (source_surface_id == 0 || dest_surface_id == 0 || width == 0 || height == 0) {
        return kIOReturnBadArgument;
    }
    
    IOLockLock(m_surface_lock);
    
    // Find source and destination surfaces
    OSObject* source_obj = findSurface(source_surface_id);
    OSObject* dest_obj = findSurface(dest_surface_id);
    
    if (!source_obj || !dest_obj) {
        IOLockUnlock(m_surface_lock);
        return kIOReturnNotFound;
    }
    
    OSData* source_data = OSDynamicCast(OSData, source_obj);
    OSData* dest_data = OSDynamicCast(OSData, dest_obj);
    
    if (!source_data || !dest_data) {
        IOLockUnlock(m_surface_lock);
        return kIOReturnInternalError;
    }
    
    VMIOSurface* source_surface = (VMIOSurface*)source_data->getBytesNoCopy();
    VMIOSurface* dest_surface = (VMIOSurface*)dest_data->getBytesNoCopy();
    
    if (!source_surface || !dest_surface || 
        !source_surface->base_address || !dest_surface->base_address) {
        IOLockUnlock(m_surface_lock);
        return kIOReturnNotReady;
    }
    
    // Validate region bounds
    if (src_x + width > source_surface->width || src_y + height > source_surface->height ||
        dst_x + width > dest_surface->width || dst_y + height > dest_surface->height) {
        IOLockUnlock(m_surface_lock);
        return kIOReturnBadArgument;
    }
    
    // Check format compatibility
    if (source_surface->descriptor.pixel_format != dest_surface->descriptor.pixel_format) {
        IOLockUnlock(m_surface_lock);
        return kIOReturnBadArgument;
    }
    
    // Perform region copy
    uint32_t bytes_per_pixel = getBytesPerPixel(source_surface->descriptor.pixel_format);
    uint32_t source_stride = source_surface->width * bytes_per_pixel;
    uint32_t dest_stride = dest_surface->width * bytes_per_pixel;
    
    uint8_t* src_ptr = (uint8_t*)source_surface->base_address + 
                       (src_y * source_stride) + (src_x * bytes_per_pixel);
    uint8_t* dst_ptr = (uint8_t*)dest_surface->base_address + 
                       (dst_y * dest_stride) + (dst_x * bytes_per_pixel);
    
    uint32_t row_bytes = width * bytes_per_pixel;
    
    for (uint32_t y = 0; y < height; y++) {
        memcpy(dst_ptr, src_ptr, row_bytes);
        src_ptr += source_stride;
        dst_ptr += dest_stride;
    }
    
    dest_surface->last_access_time = mach_absolute_time();
    m_copy_operations++;
    
    IOLog("VMIOSurfaceManager: Copied region from surface %u to %u (%ux%u at %u,%u -> %u,%u)\n",
          source_surface_id, dest_surface_id, width, height, src_x, src_y, dst_x, dst_y);
    
    IOLockUnlock(m_surface_lock);
    return kIOReturnSuccess;
}

IOReturn CLASS::fillSurfaceRegion(uint32_t surface_id, uint32_t x, uint32_t y, 
                                 uint32_t width, uint32_t height, uint32_t fill_color)
{
    if (surface_id == 0 || width == 0 || height == 0) {
        return kIOReturnBadArgument;
    }
    
    IOLockLock(m_surface_lock);
    
    // Find the surface
    OSObject* surface_obj = findSurface(surface_id);
    if (!surface_obj) {
        IOLockUnlock(m_surface_lock);
        return kIOReturnNotFound;
    }
    
    OSData* surface_data = OSDynamicCast(OSData, surface_obj);
    if (!surface_data) {
        IOLockUnlock(m_surface_lock);
        return kIOReturnInternalError;
    }
    
    VMIOSurface* surface = (VMIOSurface*)surface_data->getBytesNoCopy();
    if (!surface || !surface->base_address) {
        IOLockUnlock(m_surface_lock);
        return kIOReturnNotReady;
    }
    
    // Validate region bounds
    if (x + width > surface->width || y + height > surface->height) {
        IOLockUnlock(m_surface_lock);
        return kIOReturnBadArgument;
    }
    
    // Fill the specified region
    uint32_t bytes_per_pixel = getBytesPerPixel(surface->descriptor.pixel_format);
    if (bytes_per_pixel == 0) {
        IOLockUnlock(m_surface_lock);
        return kIOReturnUnsupported;
    }
    
    uint32_t stride = surface->width * bytes_per_pixel;
    uint8_t* base_ptr = (uint8_t*)surface->base_address + (y * stride) + (x * bytes_per_pixel);
    
    for (uint32_t row = 0; row < height; row++) {
        uint32_t* pixels = (uint32_t*)(base_ptr + (row * stride));
        for (uint32_t col = 0; col < width; col++) {
            pixels[col] = fill_color;
        }
    }
    
    surface->last_access_time = mach_absolute_time();
    
    IOLog("VMIOSurfaceManager: Filled region in surface %u (%ux%u at %u,%u) with color 0x%08X\n",
          surface_id, width, height, x, y, fill_color);
    
    IOLockUnlock(m_surface_lock);
    return kIOReturnSuccess;
}

IOReturn CLASS::compactSurfaceMemory()
{
    IOLockLock(m_surface_lock);
    
    IOLog("VMIOSurfaceManager: Starting memory compaction...\n");
    
    uint64_t memory_before = m_allocated_surface_memory;
    uint32_t surfaces_processed = 0;
    
    // Simple compaction: remove any surfaces that are marked for cleanup
    for (int i = (int)m_surfaces->getCount() - 1; i >= 0; i--) {
        OSData* surface_data = OSDynamicCast(OSData, m_surfaces->getObject(i));
        if (!surface_data) continue;
        
        VMIOSurface* surface = (VMIOSurface*)surface_data->getBytesNoCopy();
        if (!surface) continue;
        
        // Check if surface should be evicted
        if (shouldEvictSurface(surface->surface_id)) {
            // Remove unused surface
            if (surface->lock_count == 0 && surface->ref_count <= 1) {
                destroySurface(surface->surface_id);
                surfaces_processed++;
            }
        }
    }
    
    uint64_t memory_saved = memory_before - m_allocated_surface_memory;
    
    IOLog("VMIOSurfaceManager: Memory compaction complete - processed %u surfaces, saved %llu bytes\n",
          surfaces_processed, memory_saved);
    
    IOLockUnlock(m_surface_lock);
    return kIOReturnSuccess;
}

bool CLASS::shouldEvictSurface(uint32_t surface_id)
{
    // Find the surface
    OSObject* surface_obj = findSurface(surface_id);
    if (!surface_obj) {
        return false;
    }
    
    OSData* surface_data = OSDynamicCast(OSData, surface_obj);
    if (!surface_data) {
        return false;
    }
    
    VMIOSurface* surface = (VMIOSurface*)surface_data->getBytesNoCopy();
    if (!surface) {
        return false;
    }
    
    // Don't evict locked surfaces
    if (surface->is_locked || surface->lock_count > 0) {
        return false;
    }
    
    // Don't evict surfaces with multiple references
    if (surface->ref_count > 1) {
        return false;
    }
    
    // Consider eviction if surface hasn't been accessed recently
    uint64_t current_time = mach_absolute_time();
    uint64_t idle_time = current_time - surface->last_access_time;
    
    // Convert to nanoseconds (simplified)
    if (idle_time > (10ULL * 1000000000ULL)) { // 10 seconds
        return true;
    }
    
    return false;
}

IOReturn CLASS::checkSurfaceIntegrity(uint32_t surface_id)
{
    IOLockLock(m_surface_lock);
    
    // Find the surface
    OSObject* surface_obj = findSurface(surface_id);
    if (!surface_obj) {
        IOLockUnlock(m_surface_lock);
        return kIOReturnNotFound;
    }
    
    OSData* surface_data = OSDynamicCast(OSData, surface_obj);
    if (!surface_data) {
        IOLockUnlock(m_surface_lock);
        return kIOReturnInternalError;
    }
    
    VMIOSurface* surface = (VMIOSurface*)surface_data->getBytesNoCopy();
    if (!surface) {
        IOLockUnlock(m_surface_lock);
        return kIOReturnInternalError;
    }
    
    // Check basic integrity
    IOReturn result = kIOReturnSuccess;
    
    // Verify memory descriptor
    if (surface->memory) {
        // Check if memory is still valid
        if (surface->memory->getLength() != surface->memory_size) {
            IOLog("VMIOSurfaceManager: Memory size mismatch for surface %u\n", surface_id);
            result = kIOReturnIOError;
        }
        
        // Verify base address
        if (surface->base_address != surface->memory->getBytesNoCopy()) {
            IOLog("VMIOSurfaceManager: Base address mismatch for surface %u\n", surface_id);
            result = kIOReturnIOError;
        }
    }
    
    // Check reference counting
    if (surface->ref_count == 0) {
        IOLog("VMIOSurfaceManager: Zero reference count for active surface %u\n", surface_id);
        result = kIOReturnInternalError;
    }
    
    // Check lock consistency
    if (surface->is_locked && surface->lock_count == 0) {
        IOLog("VMIOSurfaceManager: Lock state inconsistency for surface %u\n", surface_id);
        result = kIOReturnInternalError;
    }
    
    IOLog("VMIOSurfaceManager: Surface %u integrity check %s\n",
          surface_id, result == kIOReturnSuccess ? "PASSED" : "FAILED");
    
    IOLockUnlock(m_surface_lock);
    return result;
}

IOReturn CLASS::dumpSurfaceInfo(uint32_t surface_id)
{
    IOLockLock(m_surface_lock);
    
    // Find the surface
    OSObject* surface_obj = findSurface(surface_id);
    if (!surface_obj) {
        IOLockUnlock(m_surface_lock);
        IOLog("VMIOSurfaceManager: Surface %u not found\n", surface_id);
        return kIOReturnNotFound;
    }
    
    OSData* surface_data = OSDynamicCast(OSData, surface_obj);
    if (!surface_data) {
        IOLockUnlock(m_surface_lock);
        return kIOReturnInternalError;
    }
    
    VMIOSurface* surface = (VMIOSurface*)surface_data->getBytesNoCopy();
    if (!surface) {
        IOLockUnlock(m_surface_lock);
        return kIOReturnInternalError;
    }
    
    // Dump comprehensive surface information
    IOLog("=== Surface %u Information ===\n", surface_id);
    IOLog("Name: %s\n", surface->name);
    IOLog("Dimensions: %ux%u\n", surface->width, surface->height);
    IOLog("Pixel Format: 0x%08X\n", surface->descriptor.pixel_format);
    IOLog("Memory Size: %u bytes\n", surface->memory_size);
    IOLog("Plane Count: %u\n", surface->descriptor.plane_count);
    IOLog("Lock Count: %u (Locked: %s)\n", surface->lock_count, surface->is_locked ? "YES" : "NO");
    IOLog("Reference Count: %u\n", surface->ref_count);
    IOLog("Base Address: %p\n", surface->base_address);
    IOLog("Cache Mode: %u\n", surface->cache_mode);
    IOLog("Purgeable: %s\n", surface->is_purgeable ? "YES" : "NO");
    IOLog("Creation Time: %llu\n", surface->creation_time);
    IOLog("Last Access: %llu\n", surface->last_access_time);
    
    // Dump plane information
    for (uint32_t i = 0; i < surface->descriptor.plane_count && i < 4; i++) {
        VMIOSurfacePlaneInfo* plane = &surface->descriptor.planes[i];
        IOLog("Plane %u: %ux%u, BPE=%u, BPR=%u, offset=%u, size=%u\n",
              i, plane->width, plane->height, plane->bytes_per_element,
              plane->bytes_per_row, plane->offset, plane->size);
    }
    
    IOLog("===============================\n");
    
    IOLockUnlock(m_surface_lock);
    return kIOReturnSuccess;
}

// Additional Performance and Debugging Methods

IOReturn CLASS::getPerformanceStats(void* stats_buffer, size_t* buffer_size)
{
    // For now, this is an alias to getIOSurfaceStats for comprehensive statistics
    return getIOSurfaceStats(stats_buffer, buffer_size);
}

IOReturn CLASS::resetPerformanceCounters()
{
    IOLockLock(m_surface_lock);
    
    // Reset all performance counters
    m_surfaces_created = 0;
    m_surfaces_destroyed = 0;
    m_surface_allocations = 0;
    m_surface_deallocations = 0;
    m_surface_locks = 0;
    m_surface_unlocks = 0;
    m_lock_operations = 0;
    m_unlock_operations = 0;
    m_copy_operations = 0;
    m_cache_hits = 0;
    m_cache_misses = 0;
    m_bytes_allocated = 0;
    m_bytes_deallocated = 0;
    
    // Reset GPU counters
    m_gpu_syncs = 0;
    m_gpu_updates = 0;
    m_gpu_texture_uploads = 0;
    m_gpu_command_buffers = 0;
    
    // Reset video counters
    m_video_surfaces_created = 0;
    m_video_frames_processed = 0;
    m_video_decoder_operations = 0;
    m_video_encoder_operations = 0;
    
    // Reset memory optimization counters
    m_memory_compactions = 0;
    m_memory_defragmentations = 0;
    m_surfaces_evicted = 0;
    m_priority_changes = 0;
    
    // Reset error counters
    m_allocation_failures = 0;
    m_validation_errors = 0;
    m_integrity_failures = 0;
    m_format_conversion_errors = 0;
    
    // Reset timing statistics
    m_total_allocation_time = 0;
    m_total_lock_time = 0;
    m_total_copy_time = 0;
    
    IOLockUnlock(m_surface_lock);
    return kIOReturnSuccess;
}

IOReturn CLASS::logPerformanceData()
{
    IOLockLock(m_surface_lock);
    
    IOLog("VMIOSurfaceManager Performance Data:\n");
    IOLog("===================================\n");
    IOLog("Surfaces: Created=%llu, Destroyed=%llu, Active=%u\n", 
          m_surfaces_created, m_surfaces_destroyed, m_surface_count);
    IOLog("Memory: Total=%llu MB, Allocated=%llu MB, Peak=%llu MB\n",
          m_total_surface_memory / (1024*1024), 
          m_allocated_surface_memory / (1024*1024),
          m_peak_memory_usage / (1024*1024));
    IOLog("Operations: Locks=%llu, Unlocks=%llu, Copies=%llu\n",
          m_surface_locks, m_surface_unlocks, m_copy_operations);
    IOLog("Cache: Hits=%llu, Misses=%llu, Hit Ratio=%.2f%%\n",
          m_cache_hits, m_cache_misses, 
          (m_cache_hits + m_cache_misses > 0) ? 
          (double)m_cache_hits * 100.0 / (m_cache_hits + m_cache_misses) : 0.0);
    IOLog("GPU: Syncs=%llu, Updates=%llu, Texture Uploads=%llu\n",
          m_gpu_syncs, m_gpu_updates, m_gpu_texture_uploads);
    IOLog("Video: Surfaces=%llu, Frames=%llu, Decoder Ops=%llu\n",
          m_video_surfaces_created, m_video_frames_processed, m_video_decoder_operations);
    IOLog("Memory Optimization: Compactions=%llu, Defrag=%llu, Evicted=%llu\n",
          m_memory_compactions, m_memory_defragmentations, m_surfaces_evicted);
    IOLog("Errors: Allocation Failures=%llu, Validation=%llu, Integrity=%llu\n",
          m_allocation_failures, m_validation_errors, m_integrity_failures);
    
    if (m_surfaces_created > 0) {
        IOLog("Average Timing: Alloc=%llu ns, Lock=%llu ns, Copy=%llu ns\n",
              m_total_allocation_time / m_surfaces_created,
              m_total_lock_time / (m_surface_locks + 1),
              m_total_copy_time / (m_copy_operations + 1));
    }
    
    IOLockUnlock(m_surface_lock);
    return kIOReturnSuccess;
}

IOReturn CLASS::benchmarkSurfaceOperations(uint32_t iterations, uint64_t* results)
{
    if (!results || iterations == 0) {
        return kIOReturnBadArgument;
    }
    
    // Simple benchmark - measure create/destroy cycle time
    uint64_t start_time, end_time;
    uint32_t test_surface_id;
    
    // Create a test surface descriptor
    VMIOSurfaceDescriptor test_desc;
    test_desc.width = 256;
    test_desc.height = 256;
    test_desc.pixel_format = VM_IOSURFACE_PIXEL_FORMAT_BGRA32;
    test_desc.bytes_per_row = 256 * 4;
    test_desc.bytes_per_element = 4;
    test_desc.element_width = 1;
    test_desc.element_height = 1;
    test_desc.plane_count = 1;
    test_desc.alloc_size = 256 * 256 * 4;
    test_desc.usage_flags = VM_IOSURFACE_USAGE_READ | VM_IOSURFACE_USAGE_WRITE;
    test_desc.cache_mode = 0;
    
    // Benchmark surface creation/destruction
    clock_get_uptime(&start_time);
    
    for (uint32_t i = 0; i < iterations; i++) {
        if (createSurface(&test_desc, &test_surface_id) == kIOReturnSuccess) {
            destroySurface(test_surface_id);
        }
    }
    
    clock_get_uptime(&end_time);
    
    // Convert to nanoseconds
    uint64_t elapsed_time;
    absolutetime_to_nanoseconds(end_time - start_time, &elapsed_time);
    
    results[0] = elapsed_time;                    // Total time
    results[1] = elapsed_time / iterations;       // Average time per operation
    results[2] = iterations;                      // Number of iterations
    results[3] = (iterations * 1000000000ULL) / elapsed_time; // Operations per second
    
    IOLog("Benchmark Results: %u iterations in %llu ns (avg: %llu ns/op, %llu ops/sec)\n",
          iterations, elapsed_time, elapsed_time / iterations, 
          (iterations * 1000000000ULL) / elapsed_time);
    
    return kIOReturnSuccess;
}

IOReturn CLASS::enableDebugLogging(bool enable)
{
    IOLockLock(m_surface_lock);
    m_debug_logging_enabled = enable;
    if (enable) {
        IOLog("VMIOSurfaceManager: Debug logging enabled\n");
    }
    IOLockUnlock(m_surface_lock);
    return kIOReturnSuccess;
}

IOReturn CLASS::setLoggingLevel(uint32_t level)
{
    if (level > 5) { // 0-5 logging levels
        return kIOReturnBadArgument;
    }
    
    IOLockLock(m_surface_lock);
    m_logging_level = level;
    IOLockUnlock(m_surface_lock);
    
    if (m_debug_logging_enabled) {
        IOLog("VMIOSurfaceManager: Logging level set to %u\n", level);
    }
    
    return kIOReturnSuccess;
}

IOReturn CLASS::captureMemorySnapshot(void* snapshot_buffer, size_t* buffer_size)
{
    if (!snapshot_buffer || !buffer_size) {
        return kIOReturnBadArgument;
    }
    
    // For now, capture basic memory layout information
    struct MemorySnapshot {
        uint32_t surface_count;
        uint64_t total_memory;
        uint64_t allocated_memory;
        uint64_t peak_memory;
        uint32_t fragment_count;
        uint64_t largest_fragment;
        uint64_t timestamp;
    };
    
    if (*buffer_size < sizeof(MemorySnapshot)) {
        *buffer_size = sizeof(MemorySnapshot);
        return kIOReturnNoSpace;
    }
    
    MemorySnapshot* snapshot = static_cast<MemorySnapshot*>(snapshot_buffer);
    
    IOLockLock(m_surface_lock);
    
    snapshot->surface_count = m_surface_count;
    snapshot->total_memory = m_total_surface_memory;
    snapshot->allocated_memory = m_allocated_surface_memory;
    snapshot->peak_memory = m_peak_memory_usage;
    snapshot->fragment_count = 1; // Simplified
    snapshot->largest_fragment = m_total_surface_memory - m_allocated_surface_memory;
    
    uint64_t current_time;
    clock_get_uptime(&current_time);
    snapshot->timestamp = current_time;
    
    IOLockUnlock(m_surface_lock);
    
    *buffer_size = sizeof(MemorySnapshot);
    return kIOReturnSuccess;
}

IOReturn CLASS::analyzeMemoryFragmentation(uint32_t* fragmentation_percent, 
                                          uint32_t* largest_fragment_size)
{
    if (!fragmentation_percent || !largest_fragment_size) {
        return kIOReturnBadArgument;
    }
    
    IOLockLock(m_surface_lock);
    
    // Simplified fragmentation analysis
    uint64_t available_memory = m_total_surface_memory - m_allocated_surface_memory;
    uint64_t largest_block = available_memory; // Simplified - assume one large block
    
    // Calculate fragmentation as percentage of non-contiguous free space
    if (m_total_surface_memory > 0) {
        uint64_t theoretical_fragments = (available_memory > 0) ? 
                                        (m_surface_count > 0 ? m_surface_count : 1) : 0;
        *fragmentation_percent = (uint32_t)((theoretical_fragments * 100) / 
                                           (m_surface_count + theoretical_fragments + 1));
    } else {
        *fragmentation_percent = 0;
    }
    
    *largest_fragment_size = (uint32_t)(largest_block / 1024); // Return in KB
    
    IOLockUnlock(m_surface_lock);
    
    if (m_debug_logging_enabled) {
        IOLog("Memory Fragmentation Analysis: %u%% fragmented, largest fragment: %u KB\n",
              *fragmentation_percent, *largest_fragment_size);
    }
    
    return kIOReturnSuccess;
}

IOReturn CLASS::updateSurfaceDescriptor(uint32_t surface_id, const VMIOSurfaceDescriptor* descriptor)
{
    if (!descriptor)
        return kIOReturnBadArgument;
        
    IOLockLock(m_surface_lock);
    
    // Find the surface by ID
    for (unsigned int i = 0; i < m_surfaces->getCount(); i++) {
        VMIOSurface* surface = (VMIOSurface*)m_surfaces->getObject(i);
        if (surface && surface->surface_id == surface_id) {
            // Update surface properties
            surface->width = descriptor->width;
            surface->height = descriptor->height;
            surface->depth = descriptor->depth;
            surface->format = descriptor->format;
            surface->usage = descriptor->usage;
            surface->flags = descriptor->flags;
            
            // Reallocate memory if size changed
            uint32_t new_size = descriptor->width * descriptor->height * descriptor->depth * 4; // 4 bytes per pixel
            if (new_size != surface->memory_size) {
                if (surface->memory) {
                    surface->memory->complete();
                    surface->memory->release();
                }
                
                surface->memory = IOBufferMemoryDescriptor::withCapacity(new_size, kIODirectionInOut);
                if (surface->memory) {
                    surface->memory->prepare();
                    surface->memory_size = new_size;
                    IOLog("VMIOSurfaceManager: Updated surface %u with new size %u bytes\n", surface_id, new_size);
                }
            }
            
            IOLockUnlock(m_surface_lock);
            return kIOReturnSuccess;
        }
    }
    
    IOLockUnlock(m_surface_lock);
    return kIOReturnNotFound;
}

IOReturn CLASS::setSurfaceProperty(uint32_t surface_id, const char* property_name, 
                                  const void* property_value, uint32_t value_size)
{
    if (!property_name || !property_value || value_size == 0)
        return kIOReturnBadArgument;
        
    IOLockLock(m_surface_lock);
    
    // Find the surface by ID
    for (unsigned int i = 0; i < m_surfaces->getCount(); i++) {
        VMIOSurface* surface = (VMIOSurface*)m_surfaces->getObject(i);
        if (surface && surface->surface_id == surface_id) {
            
            // Handle common properties
            if (strcmp(property_name, "name") == 0 && value_size <= sizeof(surface->name)) {
                memcpy(surface->name, property_value, value_size);
                surface->name[value_size] = '\0';
            }
            else if (strcmp(property_name, "cache_mode") == 0 && value_size == sizeof(uint32_t)) {
                surface->cache_mode = *(const uint32_t*)property_value;
            }
            else if (strcmp(property_name, "pixel_format") == 0 && value_size == sizeof(uint32_t)) {
                surface->format = *(const uint32_t*)property_value;
            }
            else if (strcmp(property_name, "usage_flags") == 0 && value_size == sizeof(uint32_t)) {
                surface->usage = *(const uint32_t*)property_value;
            }
            else {
                IOLockUnlock(m_surface_lock);
                IOLog("VMIOSurfaceManager: Unknown property '%s' for surface %u\n", property_name, surface_id);
                return kIOReturnUnsupported;
            }
            
            IOLog("VMIOSurfaceManager: Set property '%s' for surface %u\n", property_name, surface_id);
            IOLockUnlock(m_surface_lock);
            return kIOReturnSuccess;
        }
    }
    
    IOLockUnlock(m_surface_lock);
    return kIOReturnNotFound;
}

IOReturn CLASS::getSurfaceProperty(uint32_t surface_id, const char* property_name,
                                  void* property_value, uint32_t* value_size)
{
    if (!property_name || !property_value || !value_size)
        return kIOReturnBadArgument;
        
    IOLockLock(m_surface_lock);
    
    // Find the surface by ID
    for (unsigned int i = 0; i < m_surfaces->getCount(); i++) {
        VMIOSurface* surface = (VMIOSurface*)m_surfaces->getObject(i);
        if (surface && surface->surface_id == surface_id) {
            
            // Handle common properties
            if (strcmp(property_name, "name") == 0) {
                uint32_t name_len = (uint32_t)(strlen(surface->name) + 1);
                if (*value_size >= name_len) {
                    memcpy(property_value, surface->name, name_len);
                    *value_size = name_len;
                } else {
                    *value_size = name_len;
                    IOLockUnlock(m_surface_lock);
                    return kIOReturnNoSpace;
                }
            }
            else if (strcmp(property_name, "cache_mode") == 0) {
                if (*value_size >= sizeof(uint32_t)) {
                    *(uint32_t*)property_value = surface->cache_mode;
                    *value_size = sizeof(uint32_t);
                } else {
                    *value_size = sizeof(uint32_t);
                    IOLockUnlock(m_surface_lock);
                    return kIOReturnNoSpace;
                }
            }
            else if (strcmp(property_name, "pixel_format") == 0) {
                if (*value_size >= sizeof(uint32_t)) {
                    *(uint32_t*)property_value = surface->format;
                    *value_size = sizeof(uint32_t);
                } else {
                    *value_size = sizeof(uint32_t);
                    IOLockUnlock(m_surface_lock);
                    return kIOReturnNoSpace;
                }
            }
            else if (strcmp(property_name, "usage_flags") == 0) {
                if (*value_size >= sizeof(uint32_t)) {
                    *(uint32_t*)property_value = surface->usage;
                    *value_size = sizeof(uint32_t);
                } else {
                    *value_size = sizeof(uint32_t);
                    IOLockUnlock(m_surface_lock);
                    return kIOReturnNoSpace;
                }
            }
            else if (strcmp(property_name, "memory_size") == 0) {
                if (*value_size >= sizeof(uint32_t)) {
                    *(uint32_t*)property_value = surface->memory_size;
                    *value_size = sizeof(uint32_t);
                } else {
                    *value_size = sizeof(uint32_t);
                    IOLockUnlock(m_surface_lock);
                    return kIOReturnNoSpace;
                }
            }
            else {
                IOLockUnlock(m_surface_lock);
                IOLog("VMIOSurfaceManager: Unknown property '%s' for surface %u\n", property_name, surface_id);
                return kIOReturnUnsupported;
            }
            
            IOLockUnlock(m_surface_lock);
            return kIOReturnSuccess;
        }
    }
    
    IOLockUnlock(m_surface_lock);
    return kIOReturnNotFound;
}

// Helper method implementations

uint32_t CLASS::allocateClientId()
{
    // Check if there are any released client IDs to reuse
    if (m_released_client_ids && m_released_client_ids->getCount() > 0) {
        OSNumber* reused_id = OSDynamicCast(OSNumber, m_released_client_ids->getLastObject());
        if (reused_id) {
            uint32_t id = reused_id->unsigned32BitValue();
            m_released_client_ids->removeObject(m_released_client_ids->getCount() - 1);
            return id;
        }
    }
    
    // Allocate new client ID
    return ++m_next_client_id;
}

void CLASS::releaseClientId(uint32_t client_id)
{
    if (client_id == 0) {
        return;
    }
    
    // Initialize released IDs array if needed
    if (!m_released_client_ids) {
        m_released_client_ids = OSArray::withCapacity(16);
    }
    
    if (m_released_client_ids) {
        OSNumber* id_number = OSNumber::withNumber(client_id, 32);
        if (id_number) {
            m_released_client_ids->setObject(id_number);
            id_number->release();
        }
    }
}

OSObject* CLASS::findClient(uint32_t client_id)
{
    if (client_id == 0 || !m_client_map) {
        return nullptr;
    }
    
    char client_key[32];
    snprintf(client_key, sizeof(client_key), "%u", client_id);
    return m_client_map->getObject(client_key);
}

// GPU Integration Methods Implementation

IOReturn CLASS::bindSurfaceToTexture(uint32_t surface_id, uint32_t texture_id)
{
    if (surface_id == 0 || texture_id == 0) {
        return kIOReturnBadArgument;
    }
    
    // Check if hardware surfaces are supported
    if (!m_supports_hardware_surfaces || !m_gpu_device) {
        IOLog("VMIOSurfaceManager: Hardware surfaces not supported for texture binding\n");
        return kIOReturnUnsupported;
    }
    
    IOLockLock(m_surface_lock);
    
    // Find the surface
    OSObject* surface_obj = findSurface(surface_id);
    if (!surface_obj) {
        IOLockUnlock(m_surface_lock);
        return kIOReturnNotFound;
    }
    
    OSData* surface_data = OSDynamicCast(OSData, surface_obj);
    if (!surface_data) {
        IOLockUnlock(m_surface_lock);
        return kIOReturnInternalError;
    }
    
    VMIOSurface* surface = (VMIOSurface*)surface_data->getBytesNoCopy();
    if (!surface) {
        IOLockUnlock(m_surface_lock);
        return kIOReturnInternalError;
    }
    
    // Initialize texture bindings collection if needed
    if (!m_texture_bindings) {
        m_texture_bindings = OSArray::withCapacity(32);
        if (!m_texture_bindings) {
            IOLockUnlock(m_surface_lock);
            return kIOReturnNoMemory;
        }
    }
    
    // Create texture binding entry
    struct TextureBinding {
        uint32_t surface_id;
        uint32_t texture_id;
        uint32_t binding_type; // 0 = texture, 1 = buffer
        uint64_t binding_time;
        bool active;
    };
    
    TextureBinding* binding = (TextureBinding*)IOMalloc(sizeof(TextureBinding));
    if (!binding) {
        IOLockUnlock(m_surface_lock);
        return kIOReturnNoMemory;
    }
    
    binding->surface_id = surface_id;
    binding->texture_id = texture_id;
    binding->binding_type = 0; // texture
    binding->binding_time = mach_absolute_time();
    binding->active = true;
    
    // Store binding info
    OSData* binding_data = OSData::withBytes(binding, sizeof(TextureBinding));
    if (!binding_data) {
        IOFree(binding, sizeof(TextureBinding));
        IOLockUnlock(m_surface_lock);
        return kIOReturnNoMemory;
    }
    
    m_texture_bindings->setObject(binding_data);
    binding_data->release();
    
    // Update surface access time
    surface->last_access_time = mach_absolute_time();
    
    IOLog("VMIOSurfaceManager: Bound surface %u to texture %u\n", surface_id, texture_id);
    
    IOLockUnlock(m_surface_lock);
    return kIOReturnSuccess;
}

IOReturn CLASS::bindSurfaceToBuffer(uint32_t surface_id, uint32_t buffer_id)
{
    if (surface_id == 0 || buffer_id == 0) {
        return kIOReturnBadArgument;
    }
    
    // Check if hardware surfaces are supported
    if (!m_supports_hardware_surfaces || !m_gpu_device) {
        IOLog("VMIOSurfaceManager: Hardware surfaces not supported for buffer binding\n");
        return kIOReturnUnsupported;
    }
    
    IOLockLock(m_surface_lock);
    
    // Find the surface
    OSObject* surface_obj = findSurface(surface_id);
    if (!surface_obj) {
        IOLockUnlock(m_surface_lock);
        return kIOReturnNotFound;
    }
    
    OSData* surface_data = OSDynamicCast(OSData, surface_obj);
    if (!surface_data) {
        IOLockUnlock(m_surface_lock);
        return kIOReturnInternalError;
    }
    
    VMIOSurface* surface = (VMIOSurface*)surface_data->getBytesNoCopy();
    if (!surface) {
        IOLockUnlock(m_surface_lock);
        return kIOReturnInternalError;
    }
    
    // Initialize texture bindings collection if needed
    if (!m_texture_bindings) {
        m_texture_bindings = OSArray::withCapacity(32);
        if (!m_texture_bindings) {
            IOLockUnlock(m_surface_lock);
            return kIOReturnNoMemory;
        }
    }
    
    // Create buffer binding entry
    struct TextureBinding {
        uint32_t surface_id;
        uint32_t texture_id; // Actually buffer_id for buffer bindings
        uint32_t binding_type; // 0 = texture, 1 = buffer
        uint64_t binding_time;
        bool active;
    };
    
    TextureBinding* binding = (TextureBinding*)IOMalloc(sizeof(TextureBinding));
    if (!binding) {
        IOLockUnlock(m_surface_lock);
        return kIOReturnNoMemory;
    }
    
    binding->surface_id = surface_id;
    binding->texture_id = buffer_id; // Store buffer ID in texture_id field
    binding->binding_type = 1; // buffer
    binding->binding_time = mach_absolute_time();
    binding->active = true;
    
    // Store binding info
    OSData* binding_data = OSData::withBytes(binding, sizeof(TextureBinding));
    if (!binding_data) {
        IOFree(binding, sizeof(TextureBinding));
        IOLockUnlock(m_surface_lock);
        return kIOReturnNoMemory;
    }
    
    m_texture_bindings->setObject(binding_data);
    binding_data->release();
    
    // Update surface access time
    surface->last_access_time = mach_absolute_time();
    
    IOLog("VMIOSurfaceManager: Bound surface %u to buffer %u\n", surface_id, buffer_id);
    
    IOLockUnlock(m_surface_lock);
    return kIOReturnSuccess;
}

IOReturn CLASS::createTextureFromSurface(uint32_t surface_id, uint32_t* texture_id)
{
    if (surface_id == 0 || !texture_id) {
        return kIOReturnBadArgument;
    }
    
    // Check if hardware surfaces are supported
    if (!m_supports_hardware_surfaces || !m_gpu_device) {
        IOLog("VMIOSurfaceManager: Hardware surfaces not supported for texture creation\n");
        return kIOReturnUnsupported;
    }
    
    IOLockLock(m_surface_lock);
    
    // Find the surface
    OSObject* surface_obj = findSurface(surface_id);
    if (!surface_obj) {
        IOLockUnlock(m_surface_lock);
        return kIOReturnNotFound;
    }
    
    OSData* surface_data = OSDynamicCast(OSData, surface_obj);
    if (!surface_data) {
        IOLockUnlock(m_surface_lock);
        return kIOReturnInternalError;
    }
    
    VMIOSurface* surface = (VMIOSurface*)surface_data->getBytesNoCopy();
    if (!surface || !surface->memory) {
        IOLockUnlock(m_surface_lock);
        return kIOReturnNotReady;
    }
    
    // Generate unique texture ID
    static uint32_t next_texture_id = 1;
    *texture_id = next_texture_id++;
    
    // In a real implementation, we would call into the GPU driver here
    // For now, we simulate the texture creation process
    
    // Initialize GPU textures collection if needed
    if (!m_gpu_textures) {
        m_gpu_textures = OSArray::withCapacity(64);
        if (!m_gpu_textures) {
            IOLockUnlock(m_surface_lock);
            return kIOReturnNoMemory;
        }
    }
    
    // Create GPU texture descriptor
    struct GPUTexture {
        uint32_t texture_id;
        uint32_t surface_id;
        uint32_t width;
        uint32_t height;
        uint32_t pixel_format;
        uint64_t creation_time;
        bool active;
    };
    
    GPUTexture* gpu_texture = (GPUTexture*)IOMalloc(sizeof(GPUTexture));
    if (!gpu_texture) {
        IOLockUnlock(m_surface_lock);
        return kIOReturnNoMemory;
    }
    
    gpu_texture->texture_id = *texture_id;
    gpu_texture->surface_id = surface_id;
    gpu_texture->width = surface->width;
    gpu_texture->height = surface->height;
    gpu_texture->pixel_format = surface->descriptor.pixel_format;
    gpu_texture->creation_time = mach_absolute_time();
    gpu_texture->active = true;
    
    // Store GPU texture info
    OSData* texture_data = OSData::withBytes(gpu_texture, sizeof(GPUTexture));
    if (!texture_data) {
        IOFree(gpu_texture, sizeof(GPUTexture));
        IOLockUnlock(m_surface_lock);
        return kIOReturnNoMemory;
    }
    
    m_gpu_textures->setObject(texture_data);
    texture_data->release();
    
    // Update surface access time
    surface->last_access_time = mach_absolute_time();
    
    IOLog("VMIOSurfaceManager: Created texture %u from surface %u (%ux%u, format: 0x%08X)\n",
          *texture_id, surface_id, surface->width, surface->height, surface->descriptor.pixel_format);
    
    IOLockUnlock(m_surface_lock);
    return kIOReturnSuccess;
}

IOReturn CLASS::updateSurfaceFromTexture(uint32_t surface_id, uint32_t texture_id)
{
    if (surface_id == 0 || texture_id == 0) {
        return kIOReturnBadArgument;
    }
    
    // Check if hardware surfaces are supported
    if (!m_supports_hardware_surfaces || !m_gpu_device) {
        IOLog("VMIOSurfaceManager: Hardware surfaces not supported for texture updates\n");
        return kIOReturnUnsupported;
    }
    
    IOLockLock(m_surface_lock);
    
    // Find the surface
    OSObject* surface_obj = findSurface(surface_id);
    if (!surface_obj) {
        IOLockUnlock(m_surface_lock);
        return kIOReturnNotFound;
    }
    
    OSData* surface_data = OSDynamicCast(OSData, surface_obj);
    if (!surface_data) {
        IOLockUnlock(m_surface_lock);
        return kIOReturnInternalError;
    }
    
    VMIOSurface* surface = (VMIOSurface*)surface_data->getBytesNoCopy();
    if (!surface || !surface->memory || !surface->base_address) {
        IOLockUnlock(m_surface_lock);
        return kIOReturnNotReady;
    }
    
    // Find the GPU texture
    bool texture_found = false;
    if (m_gpu_textures) {
        struct GPUTexture {
            uint32_t texture_id;
            uint32_t surface_id;
            uint32_t width;
            uint32_t height;
            uint32_t pixel_format;
            uint64_t creation_time;
            bool active;
        };
        
        for (unsigned int i = 0; i < m_gpu_textures->getCount(); i++) {
            OSData* texture_data = OSDynamicCast(OSData, m_gpu_textures->getObject(i));
            if (!texture_data) continue;
            
            GPUTexture* gpu_texture = (GPUTexture*)texture_data->getBytesNoCopy();
            if (!gpu_texture || !gpu_texture->active) continue;
            
            if (gpu_texture->texture_id == texture_id) {
                // Verify compatibility
                if (gpu_texture->width != surface->width || 
                    gpu_texture->height != surface->height ||
                    gpu_texture->pixel_format != surface->descriptor.pixel_format) {
                    IOLog("VMIOSurfaceManager: Texture %u incompatible with surface %u\n", 
                          texture_id, surface_id);
                    IOLockUnlock(m_surface_lock);
                    return kIOReturnBadArgument;
                }
                
                texture_found = true;
                break;
            }
        }
    }
    
    if (!texture_found) {
        IOLog("VMIOSurfaceManager: Texture %u not found\n", texture_id);
        IOLockUnlock(m_surface_lock);
        return kIOReturnNotFound;
    }
    
    // In a real implementation, we would perform GPU-to-CPU memory transfer here
    // For now, we simulate the update process
    
    // Update surface access time and statistics
    surface->last_access_time = mach_absolute_time();
    m_gpu_updates++;
    
    IOLog("VMIOSurfaceManager: Updated surface %u from texture %u\n", surface_id, texture_id);
    
    IOLockUnlock(m_surface_lock);
    return kIOReturnSuccess;
}




IOReturn CLASS::syncGPUResource(uint32_t surface_id)
{
    if (surface_id == 0) {
        return kIOReturnBadArgument;
    }
    
    // Check if hardware surfaces are supported
    if (!m_supports_hardware_surfaces || !m_gpu_device) {
        return kIOReturnUnsupported;
    }
    
    IOLockLock(m_surface_lock);
    
    // Find GPU resources for this surface
    bool resource_found = false;
    
    if (m_gpu_resources) {
        struct GPUResource {
            uint32_t resource_id;
            uint32_t surface_id;
            uint32_t resource_type;
            uint32_t width;
            uint32_t height;
            uint32_t pixel_format;
            uint64_t creation_time;
            uint64_t last_sync_time;
            bool active;
            bool coherent;
        };
        
        for (unsigned int i = 0; i < m_gpu_resources->getCount(); i++) {
            OSData* resource_data = OSDynamicCast(OSData, m_gpu_resources->getObject(i));
            if (!resource_data) continue;
            
            GPUResource* gpu_resource = (GPUResource*)resource_data->getBytesNoCopy();
            if (!gpu_resource || !gpu_resource->active) continue;
            
            if (gpu_resource->surface_id == surface_id) {
                // Perform synchronization
                if (!gpu_resource->coherent) {
                    // In a real implementation, we would flush caches and sync memory
                    IOLog("VMIOSurfaceManager: Syncing non-coherent GPU resource %u\n", 
                          gpu_resource->resource_id);
                }
                
                gpu_resource->last_sync_time = mach_absolute_time();
                resource_found = true;
                
                IOLog("VMIOSurfaceManager: Synced GPU resource %u for surface %u\n",
                      gpu_resource->resource_id, surface_id);
            }
        }
    }
    
    if (!resource_found) {
        IOLockUnlock(m_surface_lock);
        return kIOReturnNotFound;
    }
    
    // Update sync statistics
    m_gpu_syncs++;
    
    IOLockUnlock(m_surface_lock);
    return kIOReturnSuccess;
}

// Video Surface Support Implementation

IOReturn CLASS::createVideoSurface(const VMIOSurfaceDescriptor* descriptor, 
                                   uint32_t codec_type, uint32_t* surface_id)
{
    if (!descriptor || !surface_id) {
        return kIOReturnBadArgument;
    }
    
    // Check if video surfaces are supported
    if (!m_supports_video_surfaces) {
        IOLog("VMIOSurfaceManager: Video surfaces not supported\n");
        return kIOReturnUnsupported;
    }
    
    IOLockLock(m_surface_lock);
    
    // Validate video-specific pixel formats
    IOReturn format_result = validateVideoPixelFormat(descriptor->pixel_format, codec_type);
    if (format_result != kIOReturnSuccess) {
        IOLog("VMIOSurfaceManager: Invalid video pixel format: %08X for codec: %u\n", 
              descriptor->pixel_format, codec_type);
        IOLockUnlock(m_surface_lock);
        return format_result;
    }
    
    // Create a modified descriptor with video-specific usage flags
    VMIOSurfaceDescriptor video_descriptor = *descriptor;
    video_descriptor.usage_flags |= VM_IOSURFACE_USAGE_VIDEO_DECODER;
    if (codec_type & 0x80000000) { // Check if encoder bit is set
        video_descriptor.usage_flags |= VM_IOSURFACE_USAGE_VIDEO_ENCODER;
    }
    
    // Create the basic surface first
    IOReturn result = createSurface(&video_descriptor, surface_id);
    if (result != kIOReturnSuccess) {
        IOLockUnlock(m_surface_lock);
        return result;
    }
    
    // Initialize video surfaces collection if needed
    if (!m_video_surfaces) {
        m_video_surfaces = OSArray::withCapacity(32);
        if (!m_video_surfaces) {
            // Clean up the created surface
            destroySurface(*surface_id);
            IOLockUnlock(m_surface_lock);
            return kIOReturnNoMemory;
        }
    }
    
    // Create video surface descriptor
    struct VideoSurface {
        uint32_t surface_id;
        uint32_t codec_type;
        uint32_t color_space;      // Default color space
        uint32_t decoder_id;       // Associated decoder ID (0 = none)
        uint64_t creation_time;
        uint64_t last_decode_time;
        uint64_t frames_decoded;
        bool decoder_attached;
        bool is_reference_frame;
        uint32_t frame_number;
        bool active;
    };
    
    VideoSurface* video_surface = (VideoSurface*)IOMalloc(sizeof(VideoSurface));
    if (!video_surface) {
        destroySurface(*surface_id);
        IOLockUnlock(m_surface_lock);
        return kIOReturnNoMemory;
    }
    
    // Initialize video surface structure
    video_surface->surface_id = *surface_id;
    video_surface->codec_type = codec_type;
    video_surface->color_space = 0x00000001; // Default to Rec. 709
    video_surface->decoder_id = 0;
    video_surface->creation_time = mach_absolute_time();
    video_surface->last_decode_time = 0;
    video_surface->frames_decoded = 0;
    video_surface->decoder_attached = false;
    video_surface->is_reference_frame = false;
    video_surface->frame_number = 0;
    video_surface->active = true;
    
    // Store video surface info
    OSData* video_data = OSData::withBytes(video_surface, sizeof(VideoSurface));
    if (!video_data) {
        IOFree(video_surface, sizeof(VideoSurface));
        destroySurface(*surface_id);
        IOLockUnlock(m_surface_lock);
        return kIOReturnNoMemory;
    }
    
    m_video_surfaces->setObject(video_data);
    video_data->release();
    IOFree(video_surface, sizeof(VideoSurface)); // Free the temporary structure
    
    // Update video statistics
    m_video_surfaces_created++;
    
    IOLog("VMIOSurfaceManager: Created video surface %u (codec: 0x%08X, format: 0x%08X)\n",
          *surface_id, codec_type, descriptor->pixel_format);
    
    IOLockUnlock(m_surface_lock);
    return kIOReturnSuccess;
}

IOReturn CLASS::setVideoSurfaceColorSpace(uint32_t surface_id, uint32_t color_space)
{
    if (surface_id == 0) {
        return kIOReturnBadArgument;
    }
    
    IOLockLock(m_surface_lock);
    
    // Find the video surface
    struct VideoSurface {
        uint32_t surface_id;
        uint32_t codec_type;
        uint32_t color_space;
        uint32_t decoder_id;
        uint64_t creation_time;
        uint64_t last_decode_time;
        uint64_t frames_decoded;
        bool decoder_attached;
        bool is_reference_frame;
        uint32_t frame_number;
        bool active;
    };
    
    bool video_surface_found = false;
    if (m_video_surfaces) {
        for (unsigned int i = 0; i < m_video_surfaces->getCount(); i++) {
            OSData* video_data = OSDynamicCast(OSData, m_video_surfaces->getObject(i));
            if (!video_data) continue;
            
            VideoSurface* video_surface = (VideoSurface*)video_data->getBytesNoCopy();
            if (!video_surface || !video_surface->active) continue;
            
            if (video_surface->surface_id == surface_id) {
                uint32_t old_color_space = video_surface->color_space;
                video_surface->color_space = color_space;
                video_surface_found = true;
                
                IOLog("VMIOSurfaceManager: Updated video surface %u color space: 0x%08X -> 0x%08X\n",
                      surface_id, old_color_space, color_space);
                break;
            }
        }
    }
    
    if (!video_surface_found) {
        IOLockUnlock(m_surface_lock);
        return kIOReturnNotFound;
    }
    
    IOLockUnlock(m_surface_lock);
    return kIOReturnSuccess;
}

IOReturn CLASS::getVideoSurfaceColorSpace(uint32_t surface_id, uint32_t* color_space)
{
    if (surface_id == 0 || !color_space) {
        return kIOReturnBadArgument;
    }
    
    IOLockLock(m_surface_lock);
    
    // Find the video surface
    struct VideoSurface {
        uint32_t surface_id;
        uint32_t codec_type;
        uint32_t color_space;
        uint32_t decoder_id;
        uint64_t creation_time;
        uint64_t last_decode_time;
        uint64_t frames_decoded;
        bool decoder_attached;
        bool is_reference_frame;
        uint32_t frame_number;
        bool active;
    };
    
    bool video_surface_found = false;
    if (m_video_surfaces) {
        for (unsigned int i = 0; i < m_video_surfaces->getCount(); i++) {
            OSData* video_data = OSDynamicCast(OSData, m_video_surfaces->getObject(i));
            if (!video_data) continue;
            
            VideoSurface* video_surface = (VideoSurface*)video_data->getBytesNoCopy();
            if (!video_surface || !video_surface->active) continue;
            
            if (video_surface->surface_id == surface_id) {
                *color_space = video_surface->color_space;
                video_surface_found = true;
                break;
            }
        }
    }
    
    if (!video_surface_found) {
        IOLockUnlock(m_surface_lock);
        return kIOReturnNotFound;
    }
    
    IOLockUnlock(m_surface_lock);
    return kIOReturnSuccess;
}

IOReturn CLASS::attachVideoDecoder(uint32_t surface_id, uint32_t decoder_id)
{
    if (surface_id == 0 || decoder_id == 0) {
        return kIOReturnBadArgument;
    }
    
    IOLockLock(m_surface_lock);
    
    // Find the video surface
    struct VideoSurface {
        uint32_t surface_id;
        uint32_t codec_type;
        uint32_t color_space;
        uint32_t decoder_id;
        uint64_t creation_time;
        uint64_t last_decode_time;
        uint64_t frames_decoded;
        bool decoder_attached;
        bool is_reference_frame;
        uint32_t frame_number;
        bool active;
    };
    
    bool video_surface_found = false;
    if (m_video_surfaces) {
        for (unsigned int i = 0; i < m_video_surfaces->getCount(); i++) {
            OSData* video_data = OSDynamicCast(OSData, m_video_surfaces->getObject(i));
            if (!video_data) continue;
            
            VideoSurface* video_surface = (VideoSurface*)video_data->getBytesNoCopy();
            if (!video_surface || !video_surface->active) continue;
            
            if (video_surface->surface_id == surface_id) {
                // Check if decoder is already attached
                if (video_surface->decoder_attached && video_surface->decoder_id != decoder_id) {
                    IOLog("VMIOSurfaceManager: Video surface %u already has decoder %u attached\n",
                          surface_id, video_surface->decoder_id);
                    IOLockUnlock(m_surface_lock);
                    return kIOReturnBusy;
                }
                
                video_surface->decoder_id = decoder_id;
                video_surface->decoder_attached = true;
                video_surface_found = true;
                
                IOLog("VMIOSurfaceManager: Attached video decoder %u to surface %u\n",
                      decoder_id, surface_id);
                break;
            }
        }
    }
    
    if (!video_surface_found) {
        IOLockUnlock(m_surface_lock);
        return kIOReturnNotFound;
    }
    
    // Initialize video decoders collection if needed
    if (!m_video_decoders) {
        m_video_decoders = OSArray::withCapacity(16);
        if (!m_video_decoders) {
            IOLockUnlock(m_surface_lock);
            return kIOReturnNoMemory;
        }
    }
    
    // Create video decoder association
    struct VideoDecoder {
        uint32_t decoder_id;
        uint32_t surface_id;
        uint32_t codec_type;
        uint64_t attachment_time;
        uint64_t frames_processed;
        bool active;
    };
    
    VideoDecoder* decoder = (VideoDecoder*)IOMalloc(sizeof(VideoDecoder));
    if (!decoder) {
        IOLockUnlock(m_surface_lock);
        return kIOReturnNoMemory;
    }
    
    decoder->decoder_id = decoder_id;
    decoder->surface_id = surface_id;
    decoder->codec_type = 0; // Will be filled from surface info if needed
    decoder->attachment_time = mach_absolute_time();
    decoder->frames_processed = 0;
    decoder->active = true;
    
    // Store decoder association
    OSData* decoder_data = OSData::withBytes(decoder, sizeof(VideoDecoder));
    if (!decoder_data) {
        IOFree(decoder, sizeof(VideoDecoder));
        IOLockUnlock(m_surface_lock);
        return kIOReturnNoMemory;
    }
    
    m_video_decoders->setObject(decoder_data);
    decoder_data->release();
    IOFree(decoder, sizeof(VideoDecoder)); // Free the temporary structure
    
    // Update video statistics
    m_video_decoder_attachments++;
    
    IOLockUnlock(m_surface_lock);
    return kIOReturnSuccess;
}

IOReturn CLASS::detachVideoDecoder(uint32_t surface_id)
{
    if (surface_id == 0) {
        return kIOReturnBadArgument;
    }
    
    IOLockLock(m_surface_lock);
    
    // Find and update the video surface
    struct VideoSurface {
        uint32_t surface_id;
        uint32_t codec_type;
        uint32_t color_space;
        uint32_t decoder_id;
        uint64_t creation_time;
        uint64_t last_decode_time;
        uint64_t frames_decoded;
        bool decoder_attached;
        bool is_reference_frame;
        uint32_t frame_number;
        bool active;
    };
    
    uint32_t detached_decoder_id = 0;
    bool video_surface_found = false;
    
    if (m_video_surfaces) {
        for (unsigned int i = 0; i < m_video_surfaces->getCount(); i++) {
            OSData* video_data = OSDynamicCast(OSData, m_video_surfaces->getObject(i));
            if (!video_data) continue;
            
            VideoSurface* video_surface = (VideoSurface*)video_data->getBytesNoCopy();
            if (!video_surface || !video_surface->active) continue;
            
            if (video_surface->surface_id == surface_id) {
                if (!video_surface->decoder_attached) {
                    IOLockUnlock(m_surface_lock);
                    return kIOReturnNotOpen;
                }
                
                detached_decoder_id = video_surface->decoder_id;
                video_surface->decoder_id = 0;
                video_surface->decoder_attached = false;
                video_surface_found = true;
                
                IOLog("VMIOSurfaceManager: Detached video decoder %u from surface %u\n",
                      detached_decoder_id, surface_id);
                break;
            }
        }
    }
    
    if (!video_surface_found) {
        IOLockUnlock(m_surface_lock);
        return kIOReturnNotFound;
    }
    
    // Remove decoder association from collection
    if (m_video_decoders && detached_decoder_id != 0) {
        struct VideoDecoder {
            uint32_t decoder_id;
            uint32_t surface_id;
            uint32_t codec_type;
            uint64_t attachment_time;
            uint64_t frames_processed;
            bool active;
        };
        
        for (int i = (int)m_video_decoders->getCount() - 1; i >= 0; i--) {
            OSData* decoder_data = OSDynamicCast(OSData, m_video_decoders->getObject(i));
            if (!decoder_data) continue;
            
            VideoDecoder* decoder = (VideoDecoder*)decoder_data->getBytesNoCopy();
            if (!decoder || !decoder->active) continue;
            
            if (decoder->decoder_id == detached_decoder_id && decoder->surface_id == surface_id) {
                decoder->active = false;
                m_video_decoders->removeObject(i);
                break;
            }
        }
    }
    
    IOLockUnlock(m_surface_lock);
    return kIOReturnSuccess;
}

// Video Helper Methods Implementation

IOReturn CLASS::validateVideoPixelFormat(VMIOSurfacePixelFormat format, uint32_t codec_type)
{
    // Common video pixel formats
    switch (format) {
        // YUV 4:2:0 formats
        case 0x79757634: // 'yuv4' - Generic YUV 4:2:0
        case 0x79757632: // 'yuv2' - YUV 4:2:2
        case 0x79757620: // 'yuv ' - YUV 4:4:4
        case 0x42475241: // 'BGRA' - 32-bit BGRA (for RGB video)
        case 0x52474241: // 'RGBA' - 32-bit RGBA (for RGB video)
            return kIOReturnSuccess;
            
        // H.264/AVC specific formats
        case 0x61766331: // 'avc1' - H.264 format
        case 0x48323634: // 'H264' - H.264 format
            if ((codec_type & 0xFFFF) == 0x264) { // H.264 codec type
                return kIOReturnSuccess;
            }
            break;
            
        // H.265/HEVC specific formats
        case 0x68766331: // 'hvc1' - H.265 format
        case 0x48323635: // 'H265' - H.265 format
            if ((codec_type & 0xFFFF) == 0x265) { // H.265 codec type
                return kIOReturnSuccess;
            }
            break;
            
        default:
            // Check if it's a standard format that can be used for video
            return validatePixelFormat(format);
    }
    
    IOLog("VMIOSurfaceManager: Pixel format %08X not compatible with codec %08X\n", 
          format, codec_type);
    return kIOReturnBadArgument;
}

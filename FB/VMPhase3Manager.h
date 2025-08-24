#ifndef VMPhase3Manager_h
#define VMPhase3Manager_h

#include <IOKit/IOService.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <libkern/c++/OSObject.h>
#include <libkern/c++/OSArray.h>
#include <libkern/c++/OSDictionary.h>

// Forward declarations
class VMQemuVGAAccelerator;
class VMVirtIOGPU;
class VMMetalBridge;
class VMOpenGLBridge;
class VMCoreAnimationAccelerator;
class VMIOSurfaceManager;

// Phase 3 feature flags
typedef enum {
    VM_PHASE3_METAL_BRIDGE = 0x01,
    VM_PHASE3_OPENGL_BRIDGE = 0x02,
    VM_PHASE3_COREANIMATION = 0x04,
    VM_PHASE3_IOSURFACE = 0x08,
    VM_PHASE3_DISPLAY_SCALING = 0x10,
    VM_PHASE3_ASYNC_RENDERING = 0x20,
    VM_PHASE3_MULTI_DISPLAY = 0x40,
    VM_PHASE3_HDR_SUPPORT = 0x80
} VMPhase3Features;

// Integration status
typedef enum {
    kVMIntegrationStatusUninitialized = 0,
    kVMIntegrationStatusInitializing = 1,
    kVMIntegrationStatusActive = 2,
    kVMIntegrationStatusError = 3,
    kVMIntegrationStatusDisabled = 4
} VMIntegrationStatus;

// Performance tier classification
typedef enum {
    kVMPerformanceTierLow = 0,    // Basic performance
    kVMPerformanceTierMedium = 1, // Standard performance  
    kVMPerformanceTierHigh = 2,   // High performance
    kVMPerformanceTierMax = 3     // Maximum performance
} VMPerformanceTier;

// Display configuration
typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t refresh_rate;
    uint32_t bit_depth;
    uint32_t color_space;
    bool hdr_supported;
    bool variable_refresh_rate;
    float scale_factor;
} VMDisplayConfiguration;

// Integration statistics
typedef struct {
    // Component status
    VMIntegrationStatus metal_bridge_status;
    VMIntegrationStatus opengl_bridge_status;
    VMIntegrationStatus coreanimation_status;
    VMIntegrationStatus iosurface_status;
    
    // Performance metrics
    VMPerformanceTier current_tier;
    uint64_t frames_rendered;
    uint64_t api_calls_processed;
    double average_frame_time;
    double gpu_utilization;
    
    // Resource usage
    uint64_t total_memory_allocated;
    uint32_t active_contexts;
    uint32_t active_surfaces;
    uint32_t active_animations;
    
    // Feature utilization
    uint64_t metal_operations;
    uint64_t opengl_operations;
    uint64_t coreanimation_operations;
    uint64_t iosurface_operations;
} VMPhase3Statistics;

/**
 * @class VMPhase3Manager
 * @brief Phase 3 Integration Manager for VMQemuVGA Advanced Features
 * 
 * This class manages the integration of all Phase 3 components including
 * Metal bridge, OpenGL compatibility, CoreAnimation acceleration, and
 * IOSurface management to provide a unified advanced 3D acceleration
 * system with full API integration and production-ready performance.
 */
class VMPhase3Manager : public OSObject
{
    OSDeclareDefaultStructors(VMPhase3Manager);
    
private:
    // Core components
    VMQemuVGAAccelerator* m_accelerator;
    VMVirtIOGPU* m_gpu_device;
    IORecursiveLock* m_lock;
    
    // Phase 3 component bridges
    VMMetalBridge* m_metal_bridge;
    VMOpenGLBridge* m_opengl_bridge;
    VMCoreAnimationAccelerator* m_coreanimation_accelerator;
    VMIOSurfaceManager* m_iosurface_manager;
    
    // Additional Phase 3 components
    class VMShaderManager* m_shader_manager;
    class VMTextureManager* m_texture_manager;
    class VMCommandBufferPool* m_command_buffer_pool;
    
    // Component initialization tracking
    uint32_t m_initialized_components;
    
    // Feature management
    uint32_t m_enabled_features;
    uint32_t m_supported_features;
    VMPerformanceTier m_performance_tier;
    VMIntegrationStatus m_integration_status;
    
    // Display management
    OSArray* m_display_configurations;
    uint32_t m_primary_display_id;
    bool m_multi_display_enabled;
    bool m_hdr_enabled;
    
    // Performance monitoring
    VMPhase3Statistics m_statistics;
    IOWorkLoop* m_monitoring_workloop;
    IOTimerEventSource* m_stats_timer;
    
    // Configuration
    OSDictionary* m_configuration;
    bool m_auto_performance_scaling;
    bool m_debug_mode;
    
public:
    // Initialization and lifecycle
    virtual bool init() override;
    virtual void free() override;
    
    // Setup and configuration
    bool initWithAccelerator(VMQemuVGAAccelerator* accelerator);
    IOReturn initializePhase3Components();
    IOReturn configurePerformanceTier(VMPerformanceTier tier);
    IOReturn enableFeatures(uint32_t feature_mask);
    IOReturn disableFeatures(uint32_t feature_mask);
    
    // Component management
    IOReturn startAllComponents();
    IOReturn stopAllComponents();
    IOReturn restartComponent(uint32_t component_id);
    VMIntegrationStatus getComponentStatus(uint32_t component_id);
    
    // Metal Bridge integration
    VMMetalBridge* getMetalBridge() { return m_metal_bridge; }
    IOReturn enableMetalSupport();
    IOReturn disableMetalSupport();
    bool isMetalSupported();
    
    // OpenGL Bridge integration
    VMOpenGLBridge* getOpenGLBridge() { return m_opengl_bridge; }
    IOReturn enableOpenGLSupport();
    IOReturn disableOpenGLSupport();
    bool isOpenGLSupported();
    
    // CoreAnimation integration
    VMCoreAnimationAccelerator* getCoreAnimationAccelerator() { return m_coreanimation_accelerator; }
    IOReturn enableCoreAnimationSupport();
    IOReturn disableCoreAnimationSupport();
    bool isCoreAnimationSupported();
    
    // IOSurface integration
    VMIOSurfaceManager* getIOSurfaceManager() { return m_iosurface_manager; }
    IOReturn enableIOSurfaceSupport();
    IOReturn disableIOSurfaceSupport();
    bool isIOSurfaceSupported();
    
    // Display management
    IOReturn configureDisplay(uint32_t display_id, const VMDisplayConfiguration* config);
    IOReturn getDisplayConfiguration(uint32_t display_id, VMDisplayConfiguration* config);
    IOReturn enableMultiDisplay();
    IOReturn disableMultiDisplay();
    IOReturn setPrimaryDisplay(uint32_t display_id);
    
    // High-level rendering coordination
    IOReturn beginFrame();
    IOReturn endFrame();
    IOReturn presentFrame();
    IOReturn waitForVSync();
    IOReturn flushCommands();
    
    // Performance management
    IOReturn setPerformanceTier(VMPerformanceTier tier);
    VMPerformanceTier getPerformanceTier();
    IOReturn enableAutoPerformanceScaling(bool enable);
    IOReturn optimizePerformance();
    
    // Resource coordination
    IOReturn allocateSharedResource(uint32_t resource_type, uint32_t size, uint32_t* resource_id);
    IOReturn deallocateSharedResource(uint32_t resource_id);
    IOReturn shareResourceBetweenComponents(uint32_t resource_id, uint32_t component_mask);
    
    // API call routing
    IOReturn routeMetalCall(uint32_t call_id, const void* parameters, void* result);
    IOReturn routeOpenGLCall(uint32_t call_id, const void* parameters, void* result);
    IOReturn routeCoreAnimationCall(uint32_t call_id, const void* parameters, void* result);
    IOReturn routeIOSurfaceCall(uint32_t call_id, const void* parameters, void* result);
    
    // Cross-component operations
    IOReturn createMetalTextureFromIOSurface(uint32_t surface_id, uint32_t* texture_id);
    IOReturn createOpenGLTextureFromIOSurface(uint32_t surface_id, uint32_t* texture_id);
    IOReturn bindIOSurfaceToCALayer(uint32_t surface_id, uint32_t layer_id);
    IOReturn synchronizeComponentStates();
    
    // Advanced features
    IOReturn enableHDRSupport();
    IOReturn disableHDRSupport();
    IOReturn configureColorSpace(uint32_t color_space);
    IOReturn enableVariableRefreshRate();
    IOReturn setDisplayScaling(float scale_factor);
    
    // Performance monitoring and statistics
    IOReturn getPhase3Statistics(VMPhase3Statistics* stats);
    IOReturn resetStatistics();
    void logPhase3State();
    IOReturn dumpPerformanceReport();
    
    // Debugging and diagnostics
    IOReturn enableDebugMode(bool enable);
    IOReturn validateAllComponents();
    IOReturn runDiagnostics();
    IOReturn getComponentDiagnostics(uint32_t component_id, void* diagnostics_buffer, size_t* buffer_size);
    
    // Configuration management
    IOReturn loadConfiguration(const char* config_path);
    IOReturn saveConfiguration(const char* config_path);
    IOReturn setConfigurationValue(const char* key, const void* value, uint32_t value_size);
    IOReturn getConfigurationValue(const char* key, void* value, uint32_t* value_size);
    
private:
    // Internal initialization helpers
    IOReturn initializeMetalBridge();
    IOReturn configureMetalBridgeFeatures();
    IOReturn configureAdvancedMetalFeatures();
    IOReturn configureMetalResourceTranslation();
    IOReturn configureMetalShaderTranslation();
    IOReturn configureMetalComputePipeline();
    IOReturn enableMetalCommandBufferOptimization();
    IOReturn initializeOpenGLBridge();
    IOReturn initializeCoreAnimationAccelerator();
    IOReturn initializeIOSurfaceManager();
    
    // Performance management helpers
    IOReturn determineOptimalPerformanceTier();
    IOReturn adjustPerformanceBasedOnLoad();
    IOReturn updatePerformanceStatistics();
    bool shouldUpgradePerformanceTier();
    bool shouldDowngradePerformanceTier();
    
    // Resource management helpers
    IOReturn setupSharedResourcePools();
    IOReturn cleanupSharedResources();
    IOReturn optimizeResourceUsage();
    
    // Component coordination helpers
    IOReturn synchronizeComponentTimers();
    IOReturn coordinateComponentStartup();
    IOReturn coordinateComponentShutdown();
    IOReturn handleComponentFailure(uint32_t component_id);
    
    // Display management helpers
    IOReturn detectDisplayCapabilities();
    IOReturn configureOptimalDisplaySettings();
    IOReturn updateDisplayConfigurations();
    
    // Advanced display management helper methods
    IOReturn validateDisplayConfiguration(uint32_t display_id, const VMDisplayConfiguration* config);
    IOReturn configureVirtIOGPUScanout(uint32_t display_id, const VMDisplayConfiguration* config);
    IOReturn configureAdvancedDisplayFeatures(uint32_t display_id, const VMDisplayConfiguration* config);
    IOReturn updateRenderingBridgesForDisplay(uint32_t display_id, const VMDisplayConfiguration* config);
    IOReturn configureDisplayPerformanceOptimizations(uint32_t display_id, const VMDisplayConfiguration* config);
    IOReturn queryVirtIOGPUDisplayMode(uint32_t display_id, VMDisplayConfiguration* config);
    IOReturn setDefaultDisplayConfiguration(uint32_t display_id, VMDisplayConfiguration* config);
    IOReturn queryAdvancedDisplayCapabilities(uint32_t display_id, VMDisplayConfiguration* config);
    
    // Multi-display helper methods
    IOReturn validateMultiDisplayCapabilities();
    IOReturn configureVirtIOGPUMultiDisplay();
    IOReturn enableCrossBridgeMultiDisplay();
    IOReturn enableMultiDisplayPerformanceOptimizations();
    IOReturn shutdownMultiDisplayRendering();
    IOReturn disableVirtIOGPUMultiDisplay();
    IOReturn cleanupCrossBridgeMultiDisplay();
    
    // Primary display helper methods
    IOReturn validatePrimaryDisplayConfiguration(uint32_t display_id);
    IOReturn configureVirtIOGPUPrimaryDisplay(uint32_t display_id);
    IOReturn updateBridgesForPrimaryDisplay(uint32_t display_id);
    IOReturn optimizePerformanceForPrimaryDisplay(uint32_t display_id);
    
    // Statistics and monitoring
    static void statisticsTimerHandler(OSObject* owner, IOTimerEventSource* sender);
    void updateStatistics();
    void calculatePerformanceMetrics();
    
    // Error handling
    IOReturn handleIntegrationError(uint32_t component_id, IOReturn error);
    void logComponentError(uint32_t component_id, const char* error_message);
    IOReturn recoverFromError(uint32_t component_id);
    
    // Utility methods
    bool isFeatureEnabled(VMPhase3Features feature);
    const char* getComponentName(uint32_t component_id);
    const char* getStatusString(VMIntegrationStatus status);
    const char* getPerformanceTierString(VMPerformanceTier tier);
};

#endif /* VMPhase3Manager_h */

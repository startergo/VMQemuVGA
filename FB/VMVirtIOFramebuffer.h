#ifndef __VMVirtIOFramebuffer_H__
#define __VMVirtIOFramebuffer_H__

#include <IOKit/IOReturn.h>
#include <IOKit/IOTypes.h>
#include <IOKit/graphics/IOFramebuffer.h>
#include <IOKit/graphics/IOGraphicsTypes.h>
#include <IOKit/IOCommandGate.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/pci/IOPCIDevice.h>
#include <IOKit/IOUserClient.h>
#include <IOKit/graphics/IOAccelerator.h>
#include <IOKit/IOTimerEventSource.h>

// Forward declaration to avoid circular includes
class VMVirtIOGPU;
class VMVirtIOAGDC;

class VMVirtIOFramebuffer : public IOFramebuffer
{
    OSDeclareDefaultStructors(VMVirtIOFramebuffer);

private:
    VMVirtIOGPU*           m_gpu_driver;        // Reference to VirtIO GPU driver
    IOPCIDevice*           m_pci_device;        // PCI device for VRAM access
    IODeviceMemory*        m_vram_range;        // PCI BAR-derived VRAM range (legacy, may be NULL on virtio-gpu-gl-pci)
    VMVirtIOAGDC*          m_agdc_service;      // AGDC service for WindowServer
    class VMQemuVGAAccelerator* m_accelerator;  // IOAccelerator child service

    // VirtIO GPU framebuffer backing: one page-aligned, physically contiguous
    // buffer that serves ALL three roles — aperture (WindowServer writes here),
    // backing (host reads pixels from here), and transfer source. Same memory
    // for all three is the load-bearing invariant. Caller-owned from the
    // createResource2D perspective; lifetime tied to the resource.
    IOBufferMemoryDescriptor* m_fb_backing;
    IODeviceMemory*        m_fb_device_memory;  // wraps m_fb_backing's physical range for aperture/VRAMRange
    uint32_t               m_fb_resource_id;    // VirtIO GPU resource holding this backing
    // Phase 2: m_fb_backing / m_fb_device_memory are allocated ONCE in start()
    // sized for the largest advertised mode (the "ceiling"). Lives until free().
    // setupFramebufferResource creates the virtio resource against this buffer
    // but does NOT own or realloc it. Mode changes (Phase 3+) recreate the
    // resource against the same buffer.
    uint32_t               m_fb_allocation_width;   // ceiling width  the buffer was sized for
    uint32_t               m_fb_allocation_height;  // ceiling height the buffer was sized for
    
    // Display configuration
    uint32_t               m_width;             // Display width
    uint32_t               m_height;            // Display height
    uint32_t               m_depth;             // Color depth
    
    // Simple display modes array
    IODisplayModeID        m_display_modes[8];
    IOItemCount            m_mode_count;
    IODisplayModeID        m_current_mode;
    
    // Display mode tracking (like QXL)
    IODisplayModeID        m_display_mode;      // Current mode ID (for IOGraphicsFamily)
    IOIndex                m_depth_mode;        // Current depth index
    
    // Display refresh timer for VirtIO GPU updates
    IOTimerEventSource*    m_refresh_timer;     // Periodic display refresh timer
    uint32_t               m_scanout_resource_id; // VirtIO GPU scanout resource ID
    bool                   m_scanout_taken_over_by_3d; // True when 3D app controls scanout
    
    void initDisplayModes();
    IOReturn createAGDCService();
    void destroyAGDCService();
    
    // Display refresh callback
    static void displayRefreshTimer(OSObject* owner, IOTimerEventSource* sender);
    void refreshDisplay();

    // Phase 3 self-check: prove the resource-recreate path works (same buffer,
    // new resource dims). Same shape as Phase 1's probeResourceTracking —
    // deterministic, self-checking. Called once from enableController after
    // the initial setupFramebufferResource, before WindowServer takes over.
    void probeResourceRecreate();

    // Phase 4: filter the supported mode list by which modes fit in the fixed
    // buffer allocated in start(). Called after the buffer exists; populates
    // m_display_modes / m_mode_count / m_current_mode.
    void filterModesByAllocation();
    
public:
    // IOService overrides
    virtual IOService* probe(IOService* provider, SInt32* score) override;
    virtual bool start(IOService* provider) override;
    virtual void stop(IOService* provider) override;
    virtual bool init(OSDictionary* properties = nullptr) override;
    virtual void free() override;
    
    // IOFramebuffer required pure virtual methods
    virtual IODeviceMemory* getApertureRange(IOPixelAperture aperture) override;
    virtual IODeviceMemory* getVRAMRange(void) override;
    virtual IOReturn getAttribute(IOSelect attribute, uintptr_t* value) override;
    virtual const char* getPixelFormats(void) override;
    virtual IOItemCount getDisplayModeCount(void) override;
    virtual IOReturn getDisplayModes(IODisplayModeID* allDisplayModes) override;
    virtual IOReturn getInformationForDisplayMode(IODisplayModeID displayMode, 
                                                   IODisplayModeInformation* info) override;
    virtual UInt64 getPixelFormatsForDisplayMode(IODisplayModeID displayMode, 
                                                  IOIndex depth) override;
    virtual IOReturn getPixelInformation(IODisplayModeID displayMode, IOIndex depth,
                                         IOPixelAperture aperture, IOPixelInformation* pixelInfo) override;
    virtual IOReturn getCurrentDisplayMode(IODisplayModeID* displayMode, IOIndex* depth) override;
    virtual IOReturn getTimingInfoForDisplayMode(IODisplayModeID displayMode, IOTimingInformation* info) override;
    
    // IOFramebuffer optional overrides (minimal implementation)
    // IOFramebuffer optional overrides (minimal implementation)
    virtual IOReturn enableController() override;  // CRITICAL: Proper controller initialization
    virtual IOReturn setDisplayMode(IODisplayModeID displayMode, IOIndex depth) override;

    // One-call framebuffer resource setup. Re-callable for mode changes.
    // Tears down any existing resource, allocates fresh contiguous backing
    // of width*height*4 bytes, creates VirtIO GPU resource, attaches backing,
    // and sets scanout. Updates m_fb_backing / m_fb_device_memory / m_fb_resource_id.
    // Returns kIOReturnSuccess on success; on failure any partial state is torn down.
    IOReturn setupFramebufferResource(uint32_t width, uint32_t height);
    void teardownFramebufferResource();
    virtual IOReturn setupForCurrentConfig() override;  // CRITICAL: Console-to-GUI transition
    virtual IOItemCount getConnectionCount(void) override;
    virtual IOReturn getDisplayStatus(void* connectFlags);  // CRITICAL: Tell IOGraphicsFamily display is connected
    virtual bool isConsoleDevice(void) override;  // CRITICAL: Enable console device capability
    
    // CRITICAL: Safe open method override for WindowServer connection handling
    virtual IOReturn open(void) override;
    virtual void close(void) override;
    
    // CRITICAL: Connection management for display activation
    virtual IOReturn getAttributeForConnection(IOIndex connectIndex, IOSelect attribute, uintptr_t* value) override;
    virtual IOReturn setAttributeForConnection(IOIndex connectIndex, IOSelect attribute, uintptr_t value) override;
    virtual IOReturn connectFlags(IOIndex connectIndex, IODisplayModeID displayMode, IOOptionBits* flags) override;
    
    // 3D scanout management - called by VMVirtIOGPU when 3D resources take over display
    void setScanoutTakenOverBy3D(bool taken_over);
    
    // Power management
    virtual IOReturn setPowerState(unsigned long powerStateOrdinal, IOService* whatDevice) override;
    
    // CRITICAL: Override newUserClient to provide VMQemuVGAClient for WindowServer
    // This is required because programmatically created services don't get personality properties
    virtual IOReturn newUserClient(task_t owningTask, void* securityID, UInt32 type, IOUserClient** handler) override;
    
    // CRITICAL: Cursor support methods (required for GUI mode)
    virtual IOReturn setCursorImage(void* cursorImage) override;
    virtual IOReturn setCursorState(SInt32 x, SInt32 y, bool visible) override;
    
    // CRITICAL: VBL interrupt support (required for smooth GUI rendering)  
    virtual IOReturn registerForInterruptType(IOSelect interruptType, 
                                              IOFBInterruptProc proc, OSObject* target, void* ref,
                                              void** interruptRef) override;
    virtual IOReturn unregisterInterrupt(void* interruptRef) override;
    
    // *** TEST: Disable AGDC methods to isolate GUI issue - make like VMQemuVGA ***
    // virtual IOReturn getAGDCInformation(void* info_buffer, uint32_t buffer_size);
    // virtual IOReturn acquireMap(IOMemoryMap** map);
    // virtual IOReturn releaseMap(IOMemoryMap* map);
    // virtual IOReturn locateServiceDependencies(void* dependencies_buffer, uint32_t buffer_size);
    virtual IOReturn setInterruptState(void* interruptRef, UInt32 state) override;
    
    // Accessor for VirtIO GPU device (for accelerator)
    VMVirtIOGPU* getGPUDevice() const { return m_gpu_driver; }
};

#endif /* __VMVirtIOFramebuffer_H__ */

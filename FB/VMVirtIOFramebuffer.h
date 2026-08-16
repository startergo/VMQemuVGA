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

    // Throttled full-surface refresh. The timer fires at 60 Hz; we transfer
    // every FULL_REFRESH_INTERVAL'th tick. Cost model: the win is
    // NOT bandwidth (host memcpy on Apple Silicon is cheap and was never
    // near a limit) — it's fewer TCG-emulated virtqueue round-trips.
    // Each refresh is two commands (TRANSFER_TO_HOST_2D + RESOURCE_FLUSH),
    // each with an MMIO doorbell write and a poll loop.
    //
    // 2026-08-16: INTERVAL 4→2 (~15 Hz → ~30 Hz). The original 4× throttle
    // was justified when every submitCommand sat on an IOSleep(1) that
    // blocks to the next scheduler tick — ~10ms/call on this guest (the
    // "120 cmd/s → 30 cmd/s is the real saving" model). The bounded-spin
    // fix (commit b414425) removed that floor: the spin covers the host's
    // <20µs response, poll_iter lands well under the fallback. What
    // remains per command is the doorbell MMIO + virtqueue ops — nonzero
    // under TCG, so this is an EXPERIMENT, not a free win: measurement is
    // guest load + cursor smoothness (the cursor rides this timer
    // exclusively — WindowServer composites it into the aperture from
    // userspace; see the 2026-08-09 investigation below — confirmed again
    // 2026-08-16 from the surface side: no pointer-tracking rects in the
    // flush blit stream, only dock-window damage).
    //
    // Sub-rect dirty tracking was investigated and rejected 2026-08-09:
    // every cursor-motion signal in the guest is dead with crsr = 0.
    // WindowServer composites the cursor into the aperture from userspace
    // via CoreGraphics; the kernel never participates; shmem cursor fields
    // (cursorLoc, cursorSize, cursorRect, oldCursorRect) are frozen at the
    // boot-console state near (15,15). setCursorState is unreachable for
    // the same reason — IOFramebuffer base only routes it to drivers that
    // advertised a hardware cursor. The real cursor-responsiveness fix is
    // host-composited hardware cursor, which is blocked on the UTM GL
    // cursor-compositing question (CocoaSpice's GL display path not
    // compositing the cursor overlay — CSCursor.isInverted =
    // !display.isGLEnabled distinguishes the paths while CSDisplay's does
    // not; upstream fix, not guest-reachable; the reproduction is the
    // three-config comparison: QXL 2D cursor works, virtio-vga-gl without
    // the kext = VGA-firmware framebuffer → 2D cursor works, with the
    // kext = GL scanout cursor fails — same device, same host, one
    // variable).
    //
    // Content-diff dirty tracking would be backwards on this configuration:
    // bytes are not the bottleneck (host-side memcpy), command count is.
    // Sub-rects would still cost two commands per tick plus the hashing CPU
    // under TCG — net regression.
    // Refresh cadence (2026-08-16 re-arm fix): the timer period is set
    // DIRECTLY to the target — no divide-by-N throttle. The old scheme
    // (16 ms tick + every-Nth-tick transfer, re-armed AFTER the work)
    // made the period interval + work-time: measured 39-52 ms per
    // transfer cycle at a "30 Hz" configuration, i.e. 19-26 Hz achieved.
    // With re-arm FIRST the period is max(interval, work). REFRESH_PERIOD_MS
    // is the single rate knob; the achieved rate is measured by the
    // window instrumentation below, not assumed from this constant.
    // Work-time is MODE-DEPENDENT (1920x1080 vs 1680x1050 changes the
    // transfer size) — the per-window work average is the budget datum
    // for any future rate decision.
    static const uint32_t  REFRESH_PERIOD_MS = 17;  // 60 Hz target (was 33/~30 Hz; decision on measured budget: workavg 3.1-4.4 ms, dispatch ~7 ms/fire -> expect ~40 Hz achieved)
    // Achieved-rate instrumentation (2026-08-16, before the 60 Hz
    // attempt — "configured" and "achieved" must be distinguishable,
    // because the overrun symptom is SKIPPED TICKS, not load).
    // Window closes on mach_absolute_time() raw delta ≥ 1e10 raw units
    // (calibrated empirically = 10.0 s: raw units are nanoseconds,
    // confirmed by the 72c53842 boot's dur values, not assumed).
    // ticks = timer fires that reached the handler (IOTimerEventSource
    // coalesces late fires, so ticks < expected IS the backup signal);
    // xfers = successful transfer+flush pairs only; work_sum = raw time
    // spent inside those pairs (separate from period — the two must
    // stay distinguishable).
    uint64_t               m_tick_window_count;
    uint64_t               m_xfer_window_count;
    uint64_t               m_work_window_sum;
    uint64_t               m_window_start_raw;
    static const uint64_t  REFRESH_WINDOW_RAW = 10000000000ULL;
    
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
    // Read access for the surface client's flush blit (2026-08-16,
    // lock+flush pair): the blit copies the accel surface's mapped
    // buffer into the FB backing at the shape offset, and the refresh
    // timer carries it to the host. No new virtio commands.
    // Backing kernel pointer valid only while the FB is started.
    void* getBackingKernelPtr() const { return m_fb_backing ? m_fb_backing->getBytesNoCopy() : nullptr; }
    uint64_t getBackingLength() const { return m_fb_backing ? (uint64_t)m_fb_backing->getLength() : 0; }
    uint32_t getFbWidth() const { return m_width; }
    uint32_t getFbHeight() const { return m_height; }

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

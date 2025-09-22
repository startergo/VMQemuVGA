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

// Forward declaration to avoid circular includes
class VMVirtIOGPU;

class VMVirtIOFramebuffer : public IOFramebuffer
{
    OSDeclareDefaultStructors(VMVirtIOFramebuffer);

private:
    VMVirtIOGPU*           m_gpu_driver;        // Reference to VirtIO GPU driver
    IOPCIDevice*           m_pci_device;        // PCI device for VRAM access
    IODeviceMemory*        m_vram_range;        // VRAM memory range
    
    // Display configuration
    uint32_t               m_width;             // Display width
    uint32_t               m_height;            // Display height
    uint32_t               m_depth;             // Color depth
    
    // Simple display modes array
    IODisplayModeID        m_display_modes[8];
    IOItemCount            m_mode_count;
    IODisplayModeID        m_current_mode;
    
    void initDisplayModes();
    
public:
    // IOService overrides
    virtual IOService* probe(IOService* provider, SInt32* score) override;
    virtual bool start(IOService* provider) override;
    virtual void stop(IOService* provider) override;
    virtual bool init(OSDictionary* properties = nullptr) override;
    virtual void free() override;
    
    // IOFramebuffer required pure virtual methods
    virtual IODeviceMemory* getApertureRange(IOPixelAperture aperture) override;
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
    
    // IOFramebuffer optional overrides (minimal implementation)
    // IOFramebuffer optional overrides (minimal implementation)
    virtual IOReturn enableController() override;  // CRITICAL: Proper controller initialization
    virtual IOReturn setDisplayMode(IODisplayModeID displayMode, IOIndex depth) override;
    virtual IOItemCount getConnectionCount(void) override;
    virtual bool isConsoleDevice(void) override;  // CRITICAL: Enable console device capability
    
    // CRITICAL: Safe open method override for WindowServer connection handling
    virtual IOReturn open(void) override;
    
    // CRITICAL: Connection management for display activation
    virtual IOReturn getAttributeForConnection(IOIndex connectIndex, IOSelect attribute, uintptr_t* value) override;
    virtual IOReturn setAttributeForConnection(IOIndex connectIndex, IOSelect attribute, uintptr_t value) override;
    virtual IOReturn connectFlags(IOIndex connectIndex, IODisplayModeID displayMode, IOOptionBits* flags) override;
    
    // Power management
    virtual IOReturn setPowerState(unsigned long powerStateOrdinal, IOService* whatDevice) override;
    
    // User client support for Metal/acceleration
    virtual IOReturn newUserClient(task_t owningTask, void* security_id, UInt32 type, IOUserClient** clientH) override;
};

#endif /* __VMVirtIOFramebuffer_H__ */

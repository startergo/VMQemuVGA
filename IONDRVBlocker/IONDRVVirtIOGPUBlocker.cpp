#include <IOKit/IOService.h>
#include <IOKit/pci/IOPCIDevice.h>
#include <IOKit/IOLib.h>
#include <libkern/c++/OSBoolean.h>

// No manual kmod_info needed - let IOKit handle it properly

class IONDRVVirtIOGPUBlocker : public IOService
{
    OSDeclareDefaultStructors(IONDRVVirtIOGPUBlocker)
    
    typedef IOService super;
    
public:
    virtual bool start(IOService* provider) override;
    virtual void stop(IOService* provider) override;
    virtual IOService* probe(IOService* provider, SInt32* score) override;
};

OSDefineMetaClassAndStructors(IONDRVVirtIOGPUBlocker, IOService)

IOService* IONDRVVirtIOGPUBlocker::probe(IOService* provider, SInt32* score)
{
    IOPCIDevice* pciDevice = OSDynamicCast(IOPCIDevice, provider);
    if (!pciDevice) {
        return NULL;
    }
    
    // Check if this is a VirtIO GPU/VGA device
    // 0x1050 = VirtIO GPU, 0x1051 = VirtIO VGA-GL, 0x1052 = VirtIO VGA
    // Get vendor and device ID from IORegistry properties
    OSData* vendorData = OSDynamicCast(OSData, pciDevice->getProperty("vendor-id"));
    OSData* deviceData = OSDynamicCast(OSData, pciDevice->getProperty("device-id"));
    
    if (!vendorData || !deviceData) {
        return NULL;
    }
    
    UInt16 vendorID = *(UInt16*)vendorData->getBytesNoCopy();
    UInt16 deviceID = *(UInt16*)deviceData->getBytesNoCopy();
    
    if (vendorID == 0x1af4 && (deviceID == 0x1050 || deviceID == 0x1051 || deviceID == 0x1052)) {
        IOLog("IONDRVVirtIOGPUBlocker: Blocking IONDRVFramebuffer for VirtIO GPU device %04x:%04x\n", vendorID, deviceID);
        
        // Set properties to prevent IONDRVFramebuffer matching
        pciDevice->setProperty("IONDRVIgnore", kOSBooleanTrue);
        pciDevice->setProperty("AAPL,ignore-ioframebuffer", kOSBooleanTrue);
        pciDevice->setProperty("AAPL,ndrv-dev", kOSBooleanFalse);
        
        // High score to ensure we match before IONDRVFramebuffer
        *score = 500000;
        return this;
    }
    
    return NULL;
}

bool IONDRVVirtIOGPUBlocker::start(IOService* provider)
{
    if (!super::start(provider)) {
        return false;
    }
    
    IOLog("IONDRVVirtIOGPUBlocker: Started - VirtIO GPU device blocked from IONDRVFramebuffer\n");
    
    // Don't register as we're just a blocker
    return true;
}

void IONDRVVirtIOGPUBlocker::stop(IOService* provider)
{
    IOLog("IONDRVVirtIOGPUBlocker: Stopped\n");
    super::stop(provider);
}

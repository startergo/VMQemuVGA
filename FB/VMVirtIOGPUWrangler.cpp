//
//  VMVirtIOGPUWrangler.cpp
//  VMQemuVGA
//
//  VirtIO GPU Wrangler implementation for AppleGPUWrangler integration
//

#include "VMVirtIOGPUWrangler.h"
#include "VMVirtIOGPU.h"
#include <IOKit/IORegistryEntry.h>

#define super IOService
OSDefineMetaClassAndStructors(VMVirtIOGPUWrangler, IOService);

bool VMVirtIOGPUWrangler::init(OSDictionary* properties)
{
    IOLog("VMVirtIOGPUWrangler::init() - Initializing VirtIO GPU Wrangler\n");
    
    if (!super::init(properties)) {
        return false;
    }
    
    m_gpu_device = nullptr;
    
    return true;
}

void VMVirtIOGPUWrangler::free()
{
    IOLog("VMVirtIOGPUWrangler::free() - Releasing VirtIO GPU Wrangler\n");
    
    if (m_gpu_device) {
        detachFromVirtIOGPU();
    }
    
    super::free();
}

bool VMVirtIOGPUWrangler::start(IOService* provider)
{
    IOLog("VMVirtIOGPUWrangler::start() - Starting VirtIO GPU Wrangler\n");
    
    if (!super::start(provider)) {
        IOLog("VMVirtIOGPUWrangler::start() - Failed to start parent\n");
        return false;
    }
    
    // Set properties that identify this as a VirtIO graphics device control
    // DON'T impersonate Apple classes - be our own class that integrates properly
    setProperty("IOClass", "VMVirtIOGPUWrangler");
    setProperty("IOMatchCategory", "GraphicsDeviceControl");  // Generic category, not Apple-specific
    setProperty("IOProviderClass", "VMVirtIOGPU");
    setProperty("VirtIOGPUWrangler", true);  // Our own identifier
    
    // VirtIO GPU vendor information
    setProperty("vendor-id", (UInt32)0x1af4);  // Red Hat VirtIO vendor ID
    setProperty("device-id", (UInt32)0x1050);  // VirtIO GPU device ID
    setProperty("subsystem-vendor-id", (UInt32)0x1af4);
    setProperty("subsystem-id", (UInt32)0x1100);
    
    // GPU capabilities
    setProperty("gpu-core-count", (UInt32)1);
    setProperty("gpu-memory-size", (UInt32)(256 * 1024 * 1024)); // 256MB default
    setProperty("gpu-type", "VirtIO GPU");
    setProperty("gpu-3d-acceleration", false); // VirtIO GPU typically doesn't have 3D
    
    // Register with AppleGPUWrangler if available
    IOReturn result = registerWithGPUWrangler();
    if (result != kIOReturnSuccess) {
        IOLog("VMVirtIOGPUWrangler::start() - Warning: Failed to register with GPU Wrangler (0x%x)\n", result);
        // Continue anyway - GPUWrangler might not be available in older systems
    }
    
    IOLog("VMVirtIOGPUWrangler::start() - VirtIO GPU Wrangler started successfully\n");
    return true;
}

void VMVirtIOGPUWrangler::stop(IOService* provider)
{
    IOLog("VMVirtIOGPUWrangler::stop() - Stopping VirtIO GPU Wrangler\n");
    
    unregisterFromGPUWrangler();
    
    if (m_gpu_device) {
        detachFromVirtIOGPU();
    }
    
    super::stop(provider);
}

IOReturn VMVirtIOGPUWrangler::getVendorInfo(UInt32* vendorID, UInt32* deviceID)
{
    if (!vendorID || !deviceID) {
        return kIOReturnBadArgument;
    }
    
    *vendorID = 0x1af4;  // Red Hat VirtIO
    *deviceID = 0x1050;  // VirtIO GPU
    
    return kIOReturnSuccess;
}

IOReturn VMVirtIOGPUWrangler::getGPUCapabilities(UInt32* capabilities)
{
    if (!capabilities) {
        return kIOReturnBadArgument;
    }
    
    // Basic 2D capabilities for VirtIO GPU
    *capabilities = (1 << 0) |  // 2D acceleration
                   (0 << 1) |  // 3D acceleration (typically not available)
                   (1 << 2) |  // Multiple displays
                   (1 << 3);   // Hardware cursor
    
    return kIOReturnSuccess;
}

IOReturn VMVirtIOGPUWrangler::getPowerState(UInt32* state)
{
    if (!state) {
        return kIOReturnBadArgument;
    }
    
    // VirtIO GPU is always on when VM is running
    *state = 1; // Full power
    
    return kIOReturnSuccess;
}

IOReturn VMVirtIOGPUWrangler::setPowerState(UInt32 state)
{
    IOLog("VMVirtIOGPUWrangler::setPowerState() - Setting power state to %u\n", state);
    
    // VirtIO GPU power management is handled by the hypervisor
    // We just acknowledge the state change
    
    return kIOReturnSuccess;
}

IOReturn VMVirtIOGPUWrangler::attachToVirtIOGPU(VMVirtIOGPU* gpu)
{
    if (!gpu) {
        return kIOReturnBadArgument;
    }
    
    if (m_gpu_device) {
        IOLog("VMVirtIOGPUWrangler::attachToVirtIOGPU() - Already attached to a GPU\n");
        return kIOReturnExclusiveAccess;
    }
    
    m_gpu_device = gpu;
    m_gpu_device->retain();
    
    IOLog("VMVirtIOGPUWrangler::attachToVirtIOGPU() - Attached to VirtIO GPU\n");
    return kIOReturnSuccess;
}

void VMVirtIOGPUWrangler::detachFromVirtIOGPU()
{
    if (m_gpu_device) {
        m_gpu_device->release();
        m_gpu_device = nullptr;
        IOLog("VMVirtIOGPUWrangler::detachFromVirtIOGPU() - Detached from VirtIO GPU\n");
    }
}

IOReturn VMVirtIOGPUWrangler::registerWithGPUWrangler()
{
    IOLog("VMVirtIOGPUWrangler::registerWithGPUWrangler() - Registering with display system\n");
    
    // Instead of trying to register with AppleGPUWrangler, 
    // register with IODisplayWrangler which handles display management
    IOService* displayWrangler = IOService::waitForMatchingService(
        IOService::serviceMatching("IODisplayWrangler"), 
        1000000000ULL);  // 1 second timeout
    
    if (displayWrangler) {
        IOLog("VMVirtIOGPUWrangler::registerWithGPUWrangler() - Found IODisplayWrangler\n");
        
        // Publish that we're available as a graphics device
        publishResource("VirtIOGPUAvailable", this);
        
        displayWrangler->release();
        setProperty("registered-with-display-wrangler", true);
        
        // Also try to notify the Window Server that we're available
        IOService* wsService = IOService::waitForMatchingService(
            IOService::serviceMatching("IOWindowServerControllers"),
            500000000ULL);  // 0.5 second timeout
        
        if (wsService) {
            IOLog("VMVirtIOGPUWrangler::registerWithGPUWrangler() - Found WindowServer controllers\n");
            wsService->release();
        }
        
        return kIOReturnSuccess;
    }
    
    IOLog("VMVirtIOGPUWrangler::registerWithGPUWrangler() - Display wrangler not found, trying alternative registration\n");
    
    // Alternative: Publish ourselves as available to the graphics system
    publishResource("GraphicsDeviceAvailable", this);
    setProperty("graphics-device-available", true);
    
    return kIOReturnSuccess;
}

void VMVirtIOGPUWrangler::unregisterFromGPUWrangler()
{
    if (!getProperty("registered-with-gpu-wrangler")) {
        return;  // Not registered
    }
    
    IOLog("VMVirtIOGPUWrangler::unregisterFromGPUWrangler() - Unregistering from AppleGPUWrangler\n");
    
    IOService* gpuWrangler = IOService::waitForMatchingService(
        IOService::serviceMatching("AppleGPUWrangler"), 
        1000000000ULL);  // 1 second timeout
    
    if (gpuWrangler) {
        gpuWrangler->callPlatformFunction(
            OSSymbol::withCString("unregisterGPUDevice"),
            false,  // waitForFunction
            this,   // param1: our device
            nullptr, // param2
            nullptr, // param3
            nullptr  // param4
        );
        
        gpuWrangler->release();
        removeProperty("registered-with-gpu-wrangler");
        IOLog("VMVirtIOGPUWrangler::unregisterFromGPUWrangler() - Unregistered from AppleGPUWrangler\n");
    }
}

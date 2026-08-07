// IOFramebuffer implementation for VMVirtIOGPU
// These methods are copied from VMQemuVGA.cpp minimal implementation
// This file should be appended to VMVirtIOGPU.cpp

#pragma mark -
#pragma mark IOFramebuffer Methods (Minimal Implementation from VMQemuVGA)
#pragma mark -

// Pixel format string
static char const pixelFormatStrings[] = IO32BitDirectPixels "\0";

#define FMT_D(x) static_cast<int>(x)
#define FMT_U(x) static_cast<unsigned>(x)

// ========================================
// Utility Methods (Private)
// ========================================

DisplayModeEntry const* CLASS::GetDisplayMode(IODisplayModeID displayMode)
{
    if (displayMode >= 1 && displayMode <= NUM_DISPLAY_MODES)
        return &modeList[displayMode - 1];
    IOLog("VMVirtIOGPU::GetDisplayMode: Bad mode ID=%d\n", FMT_D(displayMode));
    return nullptr;
}

void CLASS::IOSelectToString(IOSelect io_select, char* output)
{
    *output = static_cast<char>(io_select >> 24);
    output[1] = static_cast<char>(io_select >> 16);
    output[2] = static_cast<char>(io_select >> 8);
    output[3] = static_cast<char>(io_select);
    output[4] = '\0';
}

IODisplayModeID CLASS::TryDetectCurrentDisplayMode(IODisplayModeID defaultMode) const
{
    IODisplayModeID tableDefault = 0;
    // VirtIO GPU default is 1024x768
    uint32_t w = 1024;
    uint32_t h = 768;
    
    for (IODisplayModeID i = 0; i < NUM_DISPLAY_MODES; ++i) 
    {
        if (w == modeList[i].width && h == modeList[i].height)
        {
            return i + 1;
        }
        if (modeList[i].flags & kDisplayModeDefaultFlag)
        {
            tableDefault = i + 1;
        }
    }
    return (tableDefault ? : defaultMode);
}

// ========================================
// IOFramebuffer Required Methods
// ========================================

IOItemCount CLASS::getConnectionCount()
{
    IOLog("VMVirtIOGPU::getConnectionCount\n");
    return 1U;
}

IOReturn CLASS::getCurrentDisplayMode(IODisplayModeID* displayMode, IOIndex* depth)
{
    if (displayMode)
        *displayMode = m_display_mode;
    if (depth)
        *depth = m_depth_mode;
    IOLog("VMVirtIOGPU::getCurrentDisplayMode: mode ID=%d, depth ID=%d\n",
          FMT_D(m_display_mode), FMT_D(m_depth_mode));
    return kIOReturnSuccess;
}

IOItemCount CLASS::getDisplayModeCount()
{
    IOItemCount r = m_num_active_modes;
    IOLog("VMVirtIOGPU::getDisplayModeCount: %u modes\n", FMT_U(r));
    return r;
}

IOReturn CLASS::getDisplayModes(IODisplayModeID* allDisplayModes)
{
    IOLog("VMVirtIOGPU::getDisplayModes\n");
    if (!allDisplayModes)
    {
        return kIOReturnBadArgument;
    }
    memcpy(allDisplayModes, &m_modes[0], m_num_active_modes * sizeof(IODisplayModeID));
    return kIOReturnSuccess;
}

IOReturn CLASS::getInformationForDisplayMode(IODisplayModeID displayMode, IODisplayModeInformation* info)
{
    DisplayModeEntry const* dme;
    
    IOLog("VMVirtIOGPU::getInformationForDisplayMode: mode ID=%d\n", FMT_D(displayMode));
    
    if (!info)
    {
        return kIOReturnBadArgument;
    }
    
    dme = GetDisplayMode(displayMode);
    if (!dme) 
    {
        IOLog("VMVirtIOGPU::getInformationForDisplayMode: Display mode %d not found.\n", FMT_D(displayMode));
        return kIOReturnBadArgument;
    }
    
    bzero(info, sizeof(IODisplayModeInformation));
    info->maxDepthIndex = 0;
    info->nominalWidth = dme->width;
    info->nominalHeight = dme->height;
    info->refreshRate = 60U << 16;
    info->flags = dme->flags;
    
    IOLog("VMVirtIOGPU::getInformationForDisplayMode: wxh=%ux%u\n", 
          FMT_U(info->nominalWidth), FMT_U(info->nominalHeight));
    
    return kIOReturnSuccess;
}

const char* CLASS::getPixelFormats()
{
    IOLog("VMVirtIOGPU::getPixelFormats: %s\n", &pixelFormatStrings[0]);
    return &pixelFormatStrings[0];
}

UInt64 CLASS::getPixelFormatsForDisplayMode(IODisplayModeID displayMode, IOIndex depth)
{
    return 0ULL;
}

IOReturn CLASS::getPixelInformation(IODisplayModeID displayMode, IOIndex depth, 
                                   IOPixelAperture aperture, IOPixelInformation* pixelInfo)
{
    DisplayModeEntry const* dme;
    
    if (!pixelInfo)
    {
        return kIOReturnBadArgument;
    }
    
    if (aperture != kIOFBSystemAperture) 
    {
        IOLog("VMVirtIOGPU::getPixelInformation: aperture=%d not supported\n", FMT_D(aperture));
        return kIOReturnUnsupportedMode;
    }
    
    if (depth) 
    {
        IOLog("VMVirtIOGPU::getPixelInformation: Depth mode %d not found.\n", FMT_D(depth));
        return kIOReturnBadArgument;
    }
    
    dme = GetDisplayMode(displayMode);
    if (!dme) 
    {
        IOLog("VMVirtIOGPU::getPixelInformation: Display mode %d not found.\n", FMT_D(displayMode));
        return kIOReturnBadArgument;
    }
    
    bzero(pixelInfo, sizeof(IOPixelInformation));
    pixelInfo->activeWidth = dme->width;
    pixelInfo->activeHeight = dme->height;
    pixelInfo->flags = dme->flags;
    strlcpy(&pixelInfo->pixelFormat[0], &pixelFormatStrings[0], sizeof(IOPixelEncoding));
    pixelInfo->pixelType = kIORGBDirectPixels;
    pixelInfo->componentMasks[0] = 0xFF0000U;
    pixelInfo->componentMasks[1] = 0x00FF00U;
    pixelInfo->componentMasks[2] = 0x0000FFU;
    pixelInfo->bitsPerPixel = 32U;
    pixelInfo->componentCount = 3U;
    pixelInfo->bitsPerComponent = 8U;
    pixelInfo->bytesPerRow = ((pixelInfo->activeWidth + 7U) & (~7U)) << 2;
    
    return kIOReturnSuccess;
}

IODeviceMemory* CLASS::getApertureRange(IOPixelAperture aperture)
{
    IODeviceMemory* mem;
    
    if (aperture != kIOFBSystemAperture) 
    {
        IOLog("VMVirtIOGPU::getApertureRange: Failed request for aperture=%d\n", FMT_D(aperture));
        return nullptr;
    }
    
    if (!m_vram)
    {
        IOLog("VMVirtIOGPU::getApertureRange: No VRAM\n");
        return nullptr;
    }
    
    // For VirtIO GPU, the entire VRAM is the framebuffer
    IOLog("VMVirtIOGPU::getApertureRange: Returning VRAM as framebuffer\n");
    mem = IODeviceMemory::withSubRange(m_vram, 0, m_vram->getLength());
    if (!mem)
    {
        IOLog("VMVirtIOGPU::getApertureRange: Failed to create IODeviceMemory\n");
    }
    
    return mem;
}

IODeviceMemory* CLASS::getVRAMRange()
{
    IOLog("VMVirtIOGPU::getVRAMRange\n");
    if (!m_vram)
        return nullptr;
    
    m_vram->retain();
    return m_vram;
}

IOReturn CLASS::setDisplayMode(IODisplayModeID displayMode, IOIndex depth)
{
    DisplayModeEntry const* dme;
    
    IOLog("VMVirtIOGPU::setDisplayMode: display ID=%d, depth ID=%d\n",
          FMT_D(displayMode), FMT_D(depth));
    
    if (depth) 
    {
        IOLog("VMVirtIOGPU::setDisplayMode: Depth mode %d not found.\n", FMT_D(depth));
        return kIOReturnBadArgument;
    }
    
    dme = GetDisplayMode(displayMode);
    if (!dme) 
    {
        IOLog("VMVirtIOGPU::setDisplayMode: Display mode %d not found.\n", FMT_D(displayMode));
        return kIOReturnBadArgument;
    }
    
    // TODO: Send VirtIO GPU commands to set display mode
    // For now, just store the mode
    m_display_mode = displayMode;
    m_depth_mode = 0;
    
    IOLog("VMVirtIOGPU::setDisplayMode: Set mode to %ux%u\n", dme->width, dme->height);
    
    return kIOReturnSuccess;
}

IOReturn CLASS::enableController()
{
    IOLog("VMVirtIOGPU::enableController() called - initializing display\n");
    
    IOReturn result = super::enableController();
    if (result != kIOReturnSuccess) {
        IOLog("VMVirtIOGPU: Parent enableController() returned 0x%x, continuing anyway\n", result);
    }
    
    IOLog("VMVirtIOGPU::enableController() completed successfully\n");
    return kIOReturnSuccess;
}

bool CLASS::isConsoleDevice()
{
    IOLog("VMVirtIOGPU::isConsoleDevice\n");
    return 0 != getProvider()->getProperty("AAPL,boot-display");
}

IOReturn CLASS::getAttribute(IOSelect attribute, uintptr_t* value)
{
    IOReturn r;
    char attr[5];
    
    // No hardware cursor for VirtIO GPU
    if (attribute == kIOHardwareCursorAttribute) {
        if (value)
            *value = 0;
        r = kIOReturnSuccess;
    } else {
        r = super::getAttribute(attribute, value);
    }
    
    IOSelectToString(attribute, &attr[0]);
    if (value)
        IOLog("VMVirtIOGPU::getAttribute: attr=%s *value=%#08lx ret=%#08x\n", &attr[0], *value, r);
    else
        IOLog("VMVirtIOGPU::getAttribute: attr=%s ret=%#08x\n", &attr[0], r);
    
    return r;
}

IOReturn CLASS::getAttributeForConnection(IOIndex connectIndex, IOSelect attribute, uintptr_t* value)
{
    IOReturn r;
    char attr[5];
    
    switch (attribute) {
        case kConnectionSupportsAppleSense:
        case kConnectionDisplayParameterCount:
        case kConnectionSupportsLLDDCSense:
        case kConnectionDisplayParameters:
        case kConnectionPower:
        case kConnectionPostWake:
            r = kIOReturnUnsupported;
            break;
        case kConnectionChanged:
            IOLog("VMVirtIOGPU::getAttributeForConnection: kConnectionChanged\n");
            if (value)
                removeProperty("IOFBConfig");
            r = kIOReturnSuccess;
            break;
        case kConnectionEnable:
            IOLog("VMVirtIOGPU::getAttributeForConnection: kConnectionEnable\n");
            if (value)
                *value = 1U;
            r = kIOReturnSuccess;
            break;
        case kConnectionFlags:
            IOLog("VMVirtIOGPU::getAttributeForConnection: kConnectionFlags\n");
            if (value)
                *value = 0U;
            r = kIOReturnSuccess;
            break;
        case kConnectionSupportsHLDDCSense:
            r = kIOReturnUnsupported;
            break;
        default:
            r = super::getAttributeForConnection(connectIndex, attribute, value);
            break;
    }
    
    IOSelectToString(attribute, &attr[0]);
    if (value)
        IOLog("VMVirtIOGPU::getAttributeForConnection: index=%d, attr=%s *value=%#08lx ret=%#08x\n",
              FMT_D(connectIndex), &attr[0], *value, r);
    else
        IOLog("VMVirtIOGPU::getAttributeForConnection: index=%d, attr=%s ret=%#08x\n",
              FMT_D(connectIndex), &attr[0], r);
    
    return r;
}

IOReturn CLASS::setAttribute(IOSelect attribute, uintptr_t value)
{
    IOReturn r;
    char attr[5];
    
    r = super::setAttribute(attribute, value);
    IOSelectToString(attribute, &attr[0]);
    IOLog("VMVirtIOGPU::setAttribute: attr=%s value=%#08lx ret=%#08x\n",
          &attr[0], value, r);
    
    return r;
}

IOReturn CLASS::setAttributeForConnection(IOIndex connectIndex, IOSelect attribute, uintptr_t value)
{
    IOReturn r;
    char attr[5];
    
    switch (attribute) {
        case kConnectionFlags:
            IOLog("VMVirtIOGPU::setAttributeForConnection: kConnectionFlags %lu\n", value);
            r = kIOReturnSuccess;
            break;
        case kConnectionProbe:
            IOLog("VMVirtIOGPU::setAttributeForConnection: kConnectionProbe %lu\n", value);
            r = kIOReturnSuccess;
            break;
        default:
            r = super::setAttributeForConnection(connectIndex, attribute, value);
            break;
    }
    
    IOSelectToString(attribute, &attr[0]);
    IOLog("VMVirtIOGPU::setAttributeForConnection: index=%d, attr=%s value=%#08lx ret=%#08x\n",
          FMT_D(connectIndex), &attr[0], value, r);
    
    return r;
}

IOReturn CLASS::registerForInterruptType(IOSelect interruptType, IOFBInterruptProc proc, 
                                        OSObject* target, void* ref, void** interruptRef)
{
    char int_type[5];
    IOSelectToString(interruptType, &int_type[0]);
    IOLog("VMVirtIOGPU::registerForInterruptType: interruptType=%s\n", &int_type[0]);
    
    if (interruptType != kIOFBConnectInterruptType)
        return kIOReturnUnsupported;
    
    bzero(&m_intr, sizeof m_intr);
    m_intr.target = target;
    m_intr.ref = ref;
    m_intr.proc = proc;
    m_intr_enabled = true;
    if (interruptRef)
        *interruptRef = &m_intr;
    return kIOReturnSuccess;
}

IOReturn CLASS::unregisterInterrupt(void* interruptRef)
{
    IOLog("VMVirtIOGPU::unregisterInterrupt\n");
    if (interruptRef != &m_intr)
        return kIOReturnBadArgument;
    bzero(interruptRef, sizeof m_intr);
    m_intr_enabled = false;
    return kIOReturnSuccess;
}

IOReturn CLASS::setInterruptState(void* interruptRef, UInt32 state)
{
    IOLog("VMVirtIOGPU::setInterruptState\n");
    if (interruptRef != &m_intr)
        return kIOReturnBadArgument;
    m_intr_enabled = (state != 0);
    return kIOReturnSuccess;
}

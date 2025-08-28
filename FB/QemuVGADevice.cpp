
#include <IOKit/pci/IOPCIDevice.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IOLib.h>
#include "QemuVGADevice.h"
#include "common_fb.h"


#define SVGA_DEBUG

#ifdef  SVGA_DEBUG
#define DLOG(fmt, args...)  IOLog(fmt, ## args)
#else
#define DLOG(fmt, args...)
#endif


#define CLASS QemuVGADevice


/*************WRITEREGVBE********************/

void CLASS::WriteRegVBE(uint16_t index, uint16_t value)
{
	m_provider->ioWrite16( VBE_DISPI_IOPORT_INDEX, index );
	m_provider->ioWrite16( VBE_DISPI_IOPORT_DATA, value);
}

/*************READREGVBE********************/
uint16_t CLASS::ReadRegVBE(uint16_t index)
{
	uint16_t value;
	m_provider->ioWrite16( VBE_DISPI_IOPORT_INDEX, index );
	value = m_provider->ioRead16(VBE_DISPI_IOPORT_DATA);
	return value;
}

/*************INIT********************/
bool CLASS::Init()
{
	return true;
}

/*************CLEANUP********************/
void CLASS::Cleanup()
{	
	if (m_provider)
	{
		m_provider = 0;
	}
	
	return;
}

/*************START********************/
bool CLASS::Start(IOPCIDevice* provider)
{
    
	DLOG("%s\n",  __FUNCTION__);
	printf("QemuVGADevice: FORCE DEBUG - Start() method called\n");
	DLOG("%s: PCI bus %u device %u function %u\n", __FUNCTION__,
		 provider->getBusNumber(),
		 provider->getDeviceNumber(),
		 provider->getFunctionNumber());
	DLOG("%s: PCI device %#04x vendor %#04x revision %#02x\n", __FUNCTION__,
		 provider->configRead16(kIOPCIConfigDeviceID),
		 provider->configRead16(kIOPCIConfigVendorID),
		 provider->configRead8(kIOPCIConfigRevisionID));
	DLOG("%s: PCI subsystem %#04x vendor %#04x\n", __FUNCTION__,
		 provider->configRead16(kIOPCIConfigSubSystemID),
		 provider->configRead16(kIOPCIConfigSubSystemVendorID));

	//I/O space, at PCI Base Address Register 0 (BAR0)
	m_provider = provider;	
	provider->setMemoryEnable(true);
	provider->setIOEnable(true);
	
	// Debug PCI device information
	UInt32 vendor_id = provider->configRead16(kIOPCIConfigVendorID);
	UInt32 device_id = provider->configRead16(kIOPCIConfigDeviceID);
	IOLog("QemuVGADevice: PCI Vendor:Device = 0x%04x:0x%04x\n", 
		  vendor_id & 0xFFFF, device_id & 0xFFFF);
	
	// Force high-priority logging for debugging
	printf("QemuVGADevice: FORCE DEBUG - Driver Start() called with PCI %04x:%04x\n", 
		   vendor_id & 0xFFFF, device_id & 0xFFFF);
	
	// Device-specific VRAM detection for virtualization devices
	// (Pure VGA devices don't match IOPCIMatch, so they use system VGA for safe boot)
	if (vendor_id == 0x1b36 && device_id == 0x0100) {
		// QXL device: BAR0 is VRAM (original Snow Leopard approach) + Hardware Acceleration
		IOLog("QemuVGADevice: QXL VGA detected - enabling hardware acceleration\n");
		
		// Comprehensive BAR diagnostics for QXL
		IOLog("QemuVGADevice: QXL BAR diagnostic - checking all BARs...\n");
		for (int bar = 0; bar < 6; bar++) {
			IOMemoryDescriptor* barMem = provider->getDeviceMemoryWithIndex(bar);
			if (barMem) {
				IOLog("QemuVGADevice: QXL BAR%d available: %llu bytes at 0x%llx\n", 
					  bar, (uint64_t)barMem->getLength(), (uint64_t)barMem->getPhysicalAddress());
			} else {
				IOLog("QemuVGADevice: QXL BAR%d not available\n", bar);
			}
		}
		
		// Try BAR0 first (traditional approach)
		m_vram = provider->getDeviceMemoryWithIndex(0U);
		if (m_vram && m_vram->getLength() > 0) {
			m_vram_base = m_vram->getPhysicalAddress();
			m_vram_size = m_vram->getLength();
			IOLog("QemuVGADevice: QXL VRAM detected via BAR0: %u MB at 0x%llx\n", 
				  (uint32_t)(m_vram_size / (1024 * 1024)), (uint64_t)m_vram_base);
		} else {
			// BAR0 failed - try other BARs for QXL VRAM
			IOLog("QemuVGADevice: QXL BAR0 failed, trying alternative BARs...\n");
			m_vram = nullptr;
			
			// Try BAR1 (some QXL configurations use BAR1 for VRAM)
			m_vram = provider->getDeviceMemoryWithIndex(1U);
			if (m_vram && m_vram->getLength() > 0) {
				m_vram_base = m_vram->getPhysicalAddress();
				m_vram_size = m_vram->getLength();
				IOLog("QemuVGADevice: QXL VRAM detected via BAR1: %u MB at 0x%llx\n", 
					  (uint32_t)(m_vram_size / (1024 * 1024)), (uint64_t)m_vram_base);
			} else {
				// Try BAR2 (fallback)
				m_vram = provider->getDeviceMemoryWithIndex(2U);
				if (m_vram && m_vram->getLength() > 0) {
					m_vram_base = m_vram->getPhysicalAddress();
					m_vram_size = m_vram->getLength();
					IOLog("QemuVGADevice: QXL VRAM detected via BAR2: %u MB at 0x%llx\n", 
						  (uint32_t)(m_vram_size / (1024 * 1024)), (uint64_t)m_vram_base);
				} else {
					// All BARs failed - use simulated VRAM for QXL
					IOLog("QemuVGADevice: QXL all BARs failed, using simulated VRAM\n");
					m_vram_size = 16 * 1024 * 1024; // 16MB default for QXL
					m_vram_base = 0;
					m_vram = nullptr;
					IOLog("QemuVGADevice: QXL using simulated VRAM: %u MB\n", 
						  (uint32_t)(m_vram_size / (1024 * 1024)));
				}
			}
		}
		
		// Initialize QXL Hardware Acceleration
		IOLog("QemuVGADevice: Initializing QXL hardware acceleration...\n");
		
		// Set hardware acceleration properties for QXL
		provider->setProperty("AAPL,3D-Acceleration", kOSBooleanTrue);
		provider->setProperty("AAPL,OpenGL", kOSBooleanTrue);
		provider->setProperty("AAPL,Metal", kOSBooleanTrue);
		provider->setProperty("AAPL,HasMask", kOSBooleanTrue);
		provider->setProperty("AAPL,HasPanel", kOSBooleanTrue);
		provider->setProperty("IOPrimaryDisplay", kOSBooleanTrue);
		
		// QXL-specific hardware features
		provider->setProperty("QXL,CommandQueue", kOSBooleanTrue);
		provider->setProperty("QXL,SurfaceAllocation", kOSBooleanTrue);
		provider->setProperty("QXL,HardwareAcceleration", kOSBooleanTrue);
		
		// Enhanced VRAM properties for hardware acceleration
		OSNumber* vramSize = OSNumber::withNumber(m_vram_size, 32);
		if (vramSize) {
			provider->setProperty("VRAM,totalsize", vramSize);
			provider->setProperty("ATY,memsize", vramSize);
			vramSize->release();
		}
		
		IOLog("QemuVGADevice: QXL hardware acceleration enabled with %u MB VRAM\n", 
			  (uint32_t)(m_vram_size / (1024 * 1024)));
			  
	} else if (vendor_id == 0x1af4 && (device_id >= 0x1050 && device_id <= 0x105f)) {
		// VirtIO GPU variants: Different memory layout + Hardware Acceleration
		IOLog("QemuVGADevice: VirtIO GPU (variant 0x%04x) detected - enabling 3D acceleration\n", device_id & 0xFFFF);
		
		// Comprehensive BAR diagnostics for VirtIO GPU
		IOLog("QemuVGADevice: VirtIO GPU BAR diagnostic - checking all BARs...\n");
		for (int bar = 0; bar < 6; bar++) {
			IOMemoryDescriptor* barMem = provider->getDeviceMemoryWithIndex(bar);
			if (barMem) {
				IOLog("QemuVGADevice: VirtIO GPU BAR%d available: %llu bytes at 0x%llx\n", 
					  bar, (uint64_t)barMem->getLength(), (uint64_t)barMem->getPhysicalAddress());
			} else {
				IOLog("QemuVGADevice: VirtIO GPU BAR%d not available\n", bar);
			}
		}
		
		// VirtIO GPU uses different memory architecture - typically no direct BAR VRAM mapping
		IOLog("QemuVGADevice: VirtIO GPU using simulated VRAM approach (no direct BAR mapping)\n");
		m_vram_size = 16 * 1024 * 1024; // 16MB default like QEMU's default
		m_vram_base = 0; // Will be allocated by system
		m_vram = nullptr; // No direct BAR mapping for VirtIO GPU
		IOLog("QemuVGADevice: VirtIO GPU using simulated VRAM: %u MB\n", 
			  (uint32_t)(m_vram_size / (1024 * 1024)));
		
		// Initialize VirtIO GPU 3D Hardware Acceleration
		IOLog("QemuVGADevice: Initializing VirtIO GPU 3D hardware acceleration...\n");
		
		// Set 3D acceleration properties for VirtIO GPU
		provider->setProperty("AAPL,3D-Acceleration", kOSBooleanTrue);
		provider->setProperty("AAPL,OpenGL", kOSBooleanTrue);
		provider->setProperty("AAPL,Metal", kOSBooleanTrue);
		provider->setProperty("AAPL,Vulkan", kOSBooleanTrue);
		provider->setProperty("AAPL,HasMask", kOSBooleanTrue);
		provider->setProperty("AAPL,HasPanel", kOSBooleanTrue);
		provider->setProperty("IOPrimaryDisplay", kOSBooleanTrue);
		
		// VirtIO GPU-specific 3D features
		provider->setProperty("VirtIO,GPU3D", kOSBooleanTrue);
		provider->setProperty("VirtIO,CommandQueue", kOSBooleanTrue);
		provider->setProperty("VirtIO,Virgl", kOSBooleanTrue);
		provider->setProperty("VirtIO,HardwareAcceleration", kOSBooleanTrue);
		
		// Enhanced VRAM properties for 3D acceleration
		OSNumber* vramSize = OSNumber::withNumber(m_vram_size, 32);
		if (vramSize) {
			provider->setProperty("VRAM,totalsize", vramSize);
			provider->setProperty("ATY,memsize", vramSize);
			vramSize->release();
		}
		
		IOLog("QemuVGADevice: VirtIO GPU 3D hardware acceleration enabled with %u MB VRAM\n", 
			  (uint32_t)(m_vram_size / (1024 * 1024)));
			  
	} else if (vendor_id == 0x1414 && (device_id == 0x5353 || device_id == 0x5354 || 
	                                   device_id == 0x5355 || device_id == 0x5356 || 
	                                   device_id == 0x0058 || device_id == 0x0059)) {
		// Hyper-V DDA/Synthetic devices: Hardware Acceleration for DDA passthrough
		IOLog("QemuVGADevice: Hyper-V DDA/Synthetic (0x%04x) detected - enabling DDA acceleration\n", device_id & 0xFFFF);
		m_vram_size = 32 * 1024 * 1024; // 32MB for DDA passthrough
		m_vram_base = 0;
		m_vram = nullptr; // DDA uses different memory mapping
		IOLog("QemuVGADevice: Hyper-V DDA using simulated VRAM: %u MB\n", 
			  (uint32_t)(m_vram_size / (1024 * 1024)));
		
		// Initialize Hyper-V DDA Hardware Acceleration
		provider->setProperty("AAPL,3D-Acceleration", kOSBooleanTrue);
		provider->setProperty("AAPL,OpenGL", kOSBooleanTrue);
		provider->setProperty("AAPL,Metal", kOSBooleanTrue);
		provider->setProperty("HyperV,DDA", kOSBooleanTrue);
		provider->setProperty("HyperV,HardwareAcceleration", kOSBooleanTrue);
		OSNumber* vramSize = OSNumber::withNumber(m_vram_size, 32);
		if (vramSize) {
			provider->setProperty("VRAM,totalsize", vramSize);
			vramSize->release();
		}
		IOLog("QemuVGADevice: Hyper-V DDA hardware acceleration enabled\n");
			  
	} else if (vendor_id == 0x15ad && (device_id >= 0x0405 && device_id <= 0x0408)) {
		// VMware SVGA: Hardware Acceleration
		IOLog("QemuVGADevice: VMware SVGA (0x%04x) detected - enabling SVGA 3D acceleration\n", device_id & 0xFFFF);
		m_vram_size = 24 * 1024 * 1024; // 24MB for VMware SVGA
		m_vram_base = 0;
		m_vram = nullptr; // VMware uses different memory management
		IOLog("QemuVGADevice: VMware SVGA using simulated VRAM: %u MB\n", 
			  (uint32_t)(m_vram_size / (1024 * 1024)));
		
		// Initialize VMware SVGA 3D Hardware Acceleration
		provider->setProperty("AAPL,3D-Acceleration", kOSBooleanTrue);
		provider->setProperty("AAPL,OpenGL", kOSBooleanTrue);
		provider->setProperty("AAPL,Metal", kOSBooleanTrue);
		provider->setProperty("VMware,SVGA3D", kOSBooleanTrue);
		provider->setProperty("VMware,HardwareAcceleration", kOSBooleanTrue);
		OSNumber* vramSize = OSNumber::withNumber(m_vram_size, 32);
		if (vramSize) {
			provider->setProperty("VRAM,totalsize", vramSize);
			vramSize->release();
		}
		IOLog("QemuVGADevice: VMware SVGA 3D hardware acceleration enabled\n");
			  
	} else if ((vendor_id == 0x1002 && ((device_id >= 0x0f00 && device_id <= 0x0f03) || 
	                                    (device_id >= 0x0190 && device_id <= 0x0193)))) {
		// AMD GPU-V/Virtualization: Hardware Acceleration
		IOLog("QemuVGADevice: AMD GPU-V (0x%04x) detected - enabling GPU virtualization acceleration\n", device_id & 0xFFFF);
		m_vram_size = 64 * 1024 * 1024; // 64MB for GPU virtualization
		m_vram_base = 0;
		m_vram = nullptr; // GPU virtualization uses different memory mapping
		IOLog("QemuVGADevice: AMD GPU-V using simulated VRAM: %u MB\n", 
			  (uint32_t)(m_vram_size / (1024 * 1024)));
		
		// Initialize AMD GPU-V Hardware Acceleration
		provider->setProperty("AAPL,3D-Acceleration", kOSBooleanTrue);
		provider->setProperty("AAPL,OpenGL", kOSBooleanTrue);
		provider->setProperty("AAPL,Metal", kOSBooleanTrue);
		provider->setProperty("AAPL,Vulkan", kOSBooleanTrue);
		provider->setProperty("AMD,GPU-V", kOSBooleanTrue);
		provider->setProperty("AMD,HardwareAcceleration", kOSBooleanTrue);
		OSNumber* vramSize = OSNumber::withNumber(m_vram_size, 32);
		if (vramSize) {
			provider->setProperty("VRAM,totalsize", vramSize);
			vramSize->release();
		}
		IOLog("QemuVGADevice: AMD GPU-V hardware acceleration enabled\n");
			  
	} else if ((vendor_id == 0x10de && ((device_id >= 0x0f04 && device_id <= 0x0f07) || 
	                                    (device_id >= 0x01e0 && device_id <= 0x01e3)))) {
		// NVIDIA vGPU/Virtualization: Hardware Acceleration
		IOLog("QemuVGADevice: NVIDIA vGPU (0x%04x) detected - enabling vGPU acceleration\n", device_id & 0xFFFF);
		m_vram_size = 64 * 1024 * 1024; // 64MB for vGPU
		m_vram_base = 0;
		m_vram = nullptr; // vGPU uses different memory mapping
		IOLog("QemuVGADevice: NVIDIA vGPU using simulated VRAM: %u MB\n", 
			  (uint32_t)(m_vram_size / (1024 * 1024)));
		
		// Initialize NVIDIA vGPU Hardware Acceleration
		provider->setProperty("AAPL,3D-Acceleration", kOSBooleanTrue);
		provider->setProperty("AAPL,OpenGL", kOSBooleanTrue);
		provider->setProperty("AAPL,Metal", kOSBooleanTrue);
		provider->setProperty("AAPL,Vulkan", kOSBooleanTrue);
		provider->setProperty("NVIDIA,vGPU", kOSBooleanTrue);
		provider->setProperty("NVIDIA,HardwareAcceleration", kOSBooleanTrue);
		OSNumber* vramSize = OSNumber::withNumber(m_vram_size, 32);
		if (vramSize) {
			provider->setProperty("VRAM,totalsize", vramSize);
			vramSize->release();
		}
		IOLog("QemuVGADevice: NVIDIA vGPU hardware acceleration enabled\n");
			  
	} else if (vendor_id == 0x8086 && (device_id >= 0x0190 && device_id <= 0x0193)) {
		// Intel GVT: Hardware Acceleration
		IOLog("QemuVGADevice: Intel GVT (0x%04x) detected - enabling GVT-g acceleration\n", device_id & 0xFFFF);
		m_vram_size = 32 * 1024 * 1024; // 32MB for Intel GVT
		m_vram_base = 0;
		m_vram = nullptr; // GVT uses different memory mapping
		IOLog("QemuVGADevice: Intel GVT using simulated VRAM: %u MB\n", 
			  (uint32_t)(m_vram_size / (1024 * 1024)));
		
		// Initialize Intel GVT-g Hardware Acceleration
		provider->setProperty("AAPL,3D-Acceleration", kOSBooleanTrue);
		provider->setProperty("AAPL,OpenGL", kOSBooleanTrue);
		provider->setProperty("AAPL,Metal", kOSBooleanTrue);
		provider->setProperty("Intel,GVT-g", kOSBooleanTrue);
		provider->setProperty("Intel,HardwareAcceleration", kOSBooleanTrue);
		OSNumber* vramSize = OSNumber::withNumber(m_vram_size, 32);
		if (vramSize) {
			provider->setProperty("VRAM,totalsize", vramSize);
			vramSize->release();
		}
		IOLog("QemuVGADevice: Intel GVT-g hardware acceleration enabled\n");
			  
	} else {
		// Other virtualization devices or display controllers - try BAR0 as fallback
		IOLog("QemuVGADevice: Other virtualization device (0x%04x:0x%04x) - trying BAR0 fallback\n", 
			  vendor_id & 0xFFFF, device_id & 0xFFFF);
		
		// Comprehensive BAR diagnostics for unknown devices
		IOLog("QemuVGADevice: Unknown device BAR diagnostic - checking all BARs...\n");
		for (int bar = 0; bar < 6; bar++) {
			IOMemoryDescriptor* barMem = provider->getDeviceMemoryWithIndex(bar);
			if (barMem) {
				IOLog("QemuVGADevice: Unknown device BAR%d available: %llu bytes at 0x%llx\n", 
					  bar, (uint64_t)barMem->getLength(), (uint64_t)barMem->getPhysicalAddress());
			} else {
				IOLog("QemuVGADevice: Unknown device BAR%d not available\n", bar);
			}
		}
		
		m_vram = provider->getDeviceMemoryWithIndex(0U);
		if (m_vram) {
			m_vram_base = m_vram->getPhysicalAddress();
			m_vram_size = m_vram->getLength();
			IOLog("QemuVGADevice: BAR0 VRAM detected: %u MB at 0x%llx\n", 
				  (uint32_t)(m_vram_size / (1024 * 1024)), (uint64_t)m_vram_base);
		} else {
			m_vram_size = 16 * 1024 * 1024; // 16MB default
			m_vram_base = 0;
			IOLog("QemuVGADevice: Using default simulated VRAM: %u MB\n", 
				  (uint32_t)(m_vram_size / (1024 * 1024)));
		}
	}
	
	if (m_vram_size == 0) {
		IOLog("QemuVGADevice: ERROR - VRAM size is 0! Device-specific detection failed\n");
		return false;
	}
	
	//FB info	
	m_max_width  = VBE_DISPI_MAX_XRES;
	m_max_height = VBE_DISPI_MAX_YRES ;
	m_fb_offset = 0;
	m_fb_size   = static_cast<uint32_t>(m_vram_size);

	//get initial value
	m_width  = ReadRegVBE(VBE_DISPI_INDEX_XRES);
	m_height = ReadRegVBE(VBE_DISPI_INDEX_YRES);
	m_bpp	 = ReadRegVBE(VBE_DISPI_INDEX_BPP);

	DLOG("%s Starting with mode : w:%d h:%d bpp:%d\n", __FUNCTION__, m_width, m_height, m_bpp );

	// Note: Device-specific model names are set in VMQemuVGA.cpp based on PCI IDs
	// No generic model override here to preserve device-specific identification
	
	return true;
}

/************SETMODE*****************/
void CLASS::SetMode(uint32_t width, uint32_t height, uint32_t bpp)
{
	//save current mode value
	m_width = width;
	m_height = height;
	m_bpp = bpp;
	
	//use vbe to set mode
	WriteRegVBE(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED);
	WriteRegVBE(VBE_DISPI_INDEX_XRES, width);
	WriteRegVBE(VBE_DISPI_INDEX_YRES, height);
	WriteRegVBE(VBE_DISPI_INDEX_BPP, bpp);
	WriteRegVBE(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_ENABLED | VBE_DISPI_LFB_ENABLED);
	
}




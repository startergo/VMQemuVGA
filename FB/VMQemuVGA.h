
#ifndef __VMSVGA2_H__
#define __VMSVGA2_H__

#include <IOKit/IOService.h>
#include <IOKit/graphics/IOFramebuffer.h>

#include "QemuVGADevice.h"
#include "common_fb.h"
#include "VMVirtIOGPU.h"

// Forward declarations
class VMQemuVGAAccelerator;

// Device type enumeration for multi-path architecture
enum VMDeviceType {
	VM_DEVICE_UNKNOWN = 0,
	VM_DEVICE_QXL = 1,				// QXL device (0x1B36:0x0100)
	VM_DEVICE_VIRTIO_GPU = 2,		// VirtIO GPU (0x1AF4:0x1050-105F)
	VM_DEVICE_QEMU_VGA = 3,			// QEMU Standard VGA (0x1234:0x1111)
	VM_DEVICE_VMWARE_SVGA = 4,		// VMware SVGA (0x15AD:0x0405)
	VM_DEVICE_HYPER_V = 5,			// Hyper-V Synthetic GPU (0x1414:0x5353+)
	VM_DEVICE_INTEL_VIRT = 6,		// Intel virtualized GPU
	VM_DEVICE_AMD_VIRT = 7,			// AMD virtualized GPU
	VM_DEVICE_NVIDIA_VIRT = 8		// NVIDIA virtualized GPU
};

class VMQemuVGA : public IOFramebuffer
{
	OSDeclareDefaultStructors(VMQemuVGA);

private:
	//variables
	QemuVGADevice svga;					//the svga device
	IODeviceMemory* m_vram;				//VRAM Framebuffer (BAR0)
	
	// Device type detection and multi-path architecture
	VMDeviceType m_device_type;			//Detected device type for proper code path selection
	bool m_is_virtio_gpu;				//Quick check for VirtIO GPU devices
	bool m_is_qxl_device;				//Quick check for QXL devices
	
	// 3D acceleration support
	VMVirtIOGPU* m_gpu_device;			//VirtIO GPU device for 3D acceleration
	VMQemuVGAAccelerator* m_accelerator; //3D accelerator service
	bool m_3d_acceleration_enabled;		//Whether 3D acceleration is available
	
	// VirtIO GPU capability tracking
	bool m_supports_3d;					//Whether device supports 3D acceleration
	bool m_supports_virgl;				//Whether device supports Virgl
	uint32_t m_max_displays;			//Maximum number of displays supported

	uint32_t m_num_active_modes;		//number of custom mode
	IODisplayModeID m_display_mode;
	IOIndex m_depth_mode;
	IODisplayModeID m_modes[NUM_DISPLAY_MODES];

	struct {
		OSObject* target;
		void* ref;
		IOFBInterruptProc proc;
	} m_intr;

	IOLock* m_iolock;					//mutex for the FIFO
	
	thread_call_t m_restore_call;		//???
	uint32_t m_custom_switch;			//???
	bool m_custom_mode_switched;		//???

	bool m_intr_enabled;				//if interrupt enbaled ?
	bool m_accel_updates;				//if update support accel procedure
	DisplayModeEntry customMode;		//define in common_fb.h

	//functions
	void Cleanup();
	bool init3DAcceleration();
	void cleanup3DAcceleration();
	DisplayModeEntry const* GetDisplayMode(IODisplayModeID displayMode);
	static void IOSelectToString(IOSelect io_select, char* output);
	
	// PCI configuration space helper methods
	bool readPCIConfigSpace(IOPCIDevice* pciDevice, uint16_t* vendorID, uint16_t* deviceID);
	
	// VirtIO GPU detection and initialization helper methods
	IOReturn scanForVirtIOGPUDevices();
	VMVirtIOGPU* createMockVirtIOGPUDevice();
	IOReturn initializeDetectedVirtIOGPU();
	IOReturn queryVirtIOGPUCapabilities();
	IOReturn configureVirtIOGPUOptimalSettings();
	
	// Lilu Issue #2299 workaround: Early device registration for framework compatibility
	void publishDeviceForLiluFrameworks();
	
	// Snow Leopard system graphics integration
	IOReturn registerWithSystemGraphics();
	IOReturn initializeIOSurfaceSupport();

	IODisplayModeID TryDetectCurrentDisplayMode(IODisplayModeID defaultMode) const;

	void CustomSwitchStepSet( uint32_t value);
	void CustomSwitchStepWait(uint32_t value);
	void EmitConnectChangedEvent();
	void RestoreAllModes();
	static void _RestoreAllModes(thread_call_param_t param0,
								thread_call_param_t param1);


public:
	virtual IOService* probe(IOService* provider, SInt32* score) override;
	bool 		start(IOService* provider) override;
	void 		stop(IOService* provider) override;

	//IOFrame buffer stuff
	UInt64   	getPixelFormatsForDisplayMode(IODisplayModeID displayMode, IOIndex depth) override;	
	IOReturn 	setInterruptState(void* interruptRef, UInt32 state) override;
	IOReturn 	unregisterInterrupt(void* interruptRef) override;		
	IOItemCount getConnectionCount() override;
	IOReturn 	getCurrentDisplayMode(IODisplayModeID* displayMode, 
										IOIndex* depth) override;	
	IOReturn 	getDisplayModes(IODisplayModeID* allDisplayModes) override;	
	IOItemCount getDisplayModeCount() override;
	const char* getPixelFormats() override;
	IODeviceMemory* getVRAMRange() override;
	IODeviceMemory* getApertureRange(IOPixelAperture aperture) override;
	bool 		isConsoleDevice() override;
	IOReturn 	getAttribute(IOSelect attribute, uintptr_t* value) override;		
	IOReturn 	getAttributeForConnection(IOIndex connectIndex, IOSelect attribute, uintptr_t* value) override;
	IOReturn 	setAttribute(IOSelect attribute, uintptr_t value) override;
	IOReturn 	setAttributeForConnection(IOIndex connectIndex, IOSelect attribute, uintptr_t value) override;
	IOReturn 	registerForInterruptType(IOSelect interruptType, IOFBInterruptProc proc, OSObject* target, void* ref, void** interruptRef) override;
	IOReturn 	getInformationForDisplayMode(IODisplayModeID displayMode,
										IODisplayModeInformation* info) override;
	IOReturn 	getPixelInformation(IODisplayModeID displayMode, IOIndex depth,
						 IOPixelAperture aperture, IOPixelInformation* pixelInfo) override;										
										


	IOReturn	CustomMode(CustomModeData const* inData, CustomModeData* outData, 
										size_t inSize, size_t* outSize);
	IOReturn 	setDisplayMode(IODisplayModeID displayMode, IOIndex depth) override;
	
	// Canvas 2D hardware acceleration methods for YouTube/browser support
	IOReturn acceleratedCanvasDrawImage(const void* imageData, size_t imageSize, 
										int32_t srcX, int32_t srcY, int32_t srcW, int32_t srcH,
										int32_t dstX, int32_t dstY, int32_t dstW, int32_t dstH);
	IOReturn acceleratedCanvasFillRect(int32_t x, int32_t y, int32_t width, int32_t height, uint32_t color);
	IOReturn acceleratedCanvasDrawText(const char* text, int32_t x, int32_t y, uint32_t fontSize, uint32_t color);
	IOReturn enableCanvasAcceleration(bool enable);
	
	// Snow Leopard IOFramebuffer compatibility methods
	// These methods exist in Snow Leopard's IOFramebuffer but may have different signatures
#if defined(__MAC_OS_X_VERSION_MIN_REQUIRED) && (__MAC_OS_X_VERSION_MIN_REQUIRED < 1070)
	// Snow Leopard compatibility overrides
	virtual bool attach(IOService* provider) override;
	virtual bool terminate(IOOptionBits options) override;  
	virtual bool willTerminate(IOService* provider, IOOptionBits options) override;
	virtual bool didTerminate(IOService* provider, IOOptionBits options, bool* defer) override;
	virtual IOReturn message(UInt32 type, IOService* provider, void* argument) override;
	virtual IOReturn setProperties(OSObject* properties) override;
#endif
	
	/*
	 * Accelerator Support
	 */
	QemuVGADevice* getDevice() { return &svga; }
	VMVirtIOGPU* getGPUDevice() { return m_gpu_device; }
	VMQemuVGAAccelerator* getAccelerator() { return m_accelerator; }
	bool is3DAccelerationEnabled() const { return m_3d_acceleration_enabled; }
	
	// Device type detection and multi-path support
	VMDeviceType getDeviceType() const { return m_device_type; }
	bool isVirtIOGPUDevice() const { return m_is_virtio_gpu; }
	bool isQXLDevice() const { return m_is_qxl_device; }
	VMDeviceType detectDeviceType();
	void configureDeviceSpecificSettings();
	
	// Multi-path 3D acceleration initialization methods
	bool initVirtIOGPUAcceleration();
	bool initTraditionalAcceleration();
	bool initQXLAcceleration();
	bool initGenericAcceleration();
	bool initVMwareAcceleration();
	bool initHyperVAcceleration();
	bool initAcceleratorService();
	
	void lockDevice();
	void unlockDevice();
	void useAccelUpdates(bool state);
};

#endif /* __VMSVGA2_H__ */

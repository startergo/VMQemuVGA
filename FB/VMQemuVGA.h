
#ifndef __VMSVGA2_H__
#define __VMSVGA2_H__

#include <IOKit/IOService.h>
#include <IOKit/graphics/IOFramebuffer.h>

#include "QemuVGADevice.h"
#include "common_fb.h"
#include "VMVirtIOGPU.h"

// Forward declarations
class VMQemuVGAAccelerator;


class VMQemuVGA : public IOFramebuffer
{
	OSDeclareDefaultStructors(VMQemuVGA);

private:
	//variables
	QemuVGADevice svga;					//the svga device
	IODeviceMemory* m_vram;				//VRAM Framebuffer (BAR0)
	
	// 3D acceleration support
	VMVirtIOGPU* m_gpu_device;			//VirtIO GPU device for 3D acceleration
	VMQemuVGAAccelerator* m_accelerator; //3D accelerator service
	bool m_3d_acceleration_enabled;		//Whether 3D acceleration is available

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

	IODisplayModeID TryDetectCurrentDisplayMode(IODisplayModeID defaultMode) const;

	void CustomSwitchStepSet( uint32_t value);
	void CustomSwitchStepWait(uint32_t value);
	void EmitConnectChangedEvent();
	void RestoreAllModes();
	static void _RestoreAllModes(thread_call_param_t param0,
								thread_call_param_t param1);


public:
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
	
	/*
	 * Accelerator Support
	 */
	QemuVGADevice* getDevice() { return &svga; }
	VMVirtIOGPU* getGPUDevice() { return m_gpu_device; }
	VMQemuVGAAccelerator* getAccelerator() { return m_accelerator; }
	bool is3DAccelerationEnabled() const { return m_3d_acceleration_enabled; }
	void lockDevice();
	void unlockDevice();
	void useAccelUpdates(bool state);
	
	
};















#endif /* __VMSVGA2_H__ */

/*
 * VirtGLGLClient.h
 * Userspace client for communicating with VMVirtIOGPUUserClient in kernel
 */

#ifndef VIRTGLGLCLIENT_H
#define VIRTGLGLCLIENT_H

#include <IOKit/IOKitLib.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// VirtGLGL Client handle
typedef struct VirtGLGLClient* VirtGLGLClientRef;

// Initialize connection to kernel driver
VirtGLGLClientRef VirtGLGL_Connect(void);

// Close connection
void VirtGLGL_Disconnect(VirtGLGLClientRef client);

// Submit virgl command buffer
bool VirtGLGL_SubmitCommands(VirtGLGLClientRef client, const void* commands, uint32_t size);

// Create 3D resource
bool VirtGLGL_CreateResource(VirtGLGLClientRef client, uint32_t resourceId, 
                             uint32_t width, uint32_t height, uint32_t format);

// Create 3D context
bool VirtGLGL_CreateContext(VirtGLGLClientRef client, uint32_t contextId);

// Attach resource to context
bool VirtGLGL_AttachResource(VirtGLGLClientRef client, uint32_t contextId, uint32_t resourceId);

// Get capability
uint32_t VirtGLGL_GetCapability(VirtGLGLClientRef client, uint32_t cap);

// Transfer rendered content from 3D resource to host
bool VirtGLGL_TransferToHost2D(VirtGLGLClientRef client, uint32_t resourceId, 
                               uint32_t x, uint32_t y, uint32_t width, uint32_t height);

// Flush resource to display
bool VirtGLGL_FlushResource(VirtGLGLClientRef client, uint32_t resourceId,
                            uint32_t x, uint32_t y, uint32_t width, uint32_t height);

// Set scanout to use a resource
bool VirtGLGL_SetScanout(VirtGLGLClientRef client, uint32_t scanoutId, uint32_t resourceId,
                         uint32_t x, uint32_t y, uint32_t width, uint32_t height);

#ifdef __cplusplus
}
#endif

#endif /* VIRTGLGLCLIENT_H */

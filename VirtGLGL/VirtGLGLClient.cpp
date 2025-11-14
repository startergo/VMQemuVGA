/*
 * VirtGLGLClient.cpp
 * Userspace client for communicating with VMVirtIOGPUUserClient
 */

#include "VirtGLGLClient.h"
#include <IOKit/IOKitLib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// User client method selectors (must match kernel VMVirtIOGPUUserClient)
// NOTE: IOKit reserves selectors 0xX001 and 0xX002 (intercepted before reaching our driver!)
// We use 0x4003 and 0x4004 for CreateResource and CreateContext.
enum {
    kVMVirtIOGPU_SubmitCommands = 0x3000,
    kVMVirtIOGPU_CreateResource = 0x4003,     // Changed from 0x3001 (IOKit reserves X001)
    kVMVirtIOGPU_CreateContext = 0x4004,      // Changed from 0x3002 (IOKit reserves X002)
    kVMVirtIOGPU_AttachResource = 0x3003,
    kVMVirtIOGPU_GetCapability = 0x3004,
    kVMVirtIOGPU_TransferToHost2D = 0x3005,
    kVMVirtIOGPU_FlushResource = 0x3006,
    kVMVirtIOGPU_SetScanout = 0x3007,
};

struct VirtGLGLClient {
    io_connect_t connection;
    uint32_t nextResourceId;
    uint32_t nextContextId;
};

VirtGLGLClientRef VirtGLGL_Connect(void)
{
    kern_return_t kr;
    io_service_t service;
    io_connect_t connection;
    
    // Find VMVirtIOGPUAccelerator service (provides the user client)
    // kIOMasterPortDefault is 0 on Snow Leopard
    mach_port_t masterPort = 0;
    service = IOServiceGetMatchingService(masterPort, 
                                          IOServiceMatching("VMVirtIOGPUAccelerator"));
    if (!service) {
        fprintf(stderr, "VirtGLGL: VMVirtIOGPUAccelerator service not found\n");
        fprintf(stderr, "VirtGLGL: Make sure VMVirtIOGPU kernel driver is loaded\n");
        return NULL;
    }
    
    // Open user client connection (type 4 = VMVirtIOGPUUserClient)
    kr = IOServiceOpen(service, mach_task_self(), 4, &connection);
    IOObjectRelease(service);
    
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "VirtGLGL: Failed to open user client connection: 0x%x\n", kr);
        return NULL;
    }
    
    VirtGLGLClient* client = (VirtGLGLClient*)malloc(sizeof(VirtGLGLClient));
    if (!client) {
        IOServiceClose(connection);
        return NULL;
    }
    
    client->connection = connection;
    client->nextResourceId = 1;
    client->nextContextId = 1;
    
    printf("VirtGLGL: Connected to VMVirtIOGPUUserClient\n");
    return client;
}

void VirtGLGL_Disconnect(VirtGLGLClientRef client)
{
    if (!client) return;
    
    IOServiceClose(client->connection);
    free(client);
    printf("VirtGLGL: Disconnected from VMVirtIOGPUUserClient\n");
}

bool VirtGLGL_SubmitCommands(VirtGLGLClientRef client, const void* commands, uint32_t size)
{
    if (!client || !commands || size == 0) return false;
    
    kern_return_t kr = IOConnectCallStructMethod(
        client->connection,
        kVMVirtIOGPU_SubmitCommands,
        commands,
        size,
        NULL,
        NULL
    );
    
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "VirtGLGL: Failed to submit commands: 0x%x\n", kr);
        return false;
    }
    
    return true;
}

bool VirtGLGL_CreateResource(VirtGLGLClientRef client, uint32_t resourceId,
                             uint32_t width, uint32_t height, uint32_t format)
{
    if (!client) return false;
    
    uint64_t input[4] = { resourceId, width, height, format };
    
    kern_return_t kr = IOConnectCallScalarMethod(
        client->connection,
        kVMVirtIOGPU_CreateResource,
        input,
        4,
        NULL,
        NULL
    );
    
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "VirtGLGL: Failed to create resource: 0x%x\n", kr);
        return false;
    }
    
    printf("VirtGLGL: Created resource %u (%ux%u, format %u)\n", 
           resourceId, width, height, format);
    return true;
}

bool VirtGLGL_CreateContext(VirtGLGLClientRef client, uint32_t contextId)
{
    if (!client) return false;
    
    uint64_t input = contextId;
    
    kern_return_t kr = IOConnectCallScalarMethod(
        client->connection,
        kVMVirtIOGPU_CreateContext,
        &input,
        1,
        NULL,
        NULL
    );
    
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "VirtGLGL: Failed to create context: 0x%x\n", kr);
        return false;
    }
    
    printf("VirtGLGL: Created context %u\n", contextId);
    return true;
}

bool VirtGLGL_AttachResource(VirtGLGLClientRef client, uint32_t contextId, uint32_t resourceId)
{
    if (!client) return false;
    
    uint64_t input[2] = { contextId, resourceId };
    
    kern_return_t kr = IOConnectCallScalarMethod(
        client->connection,
        kVMVirtIOGPU_AttachResource,
        input,
        2,
        NULL,
        NULL
    );
    
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "VirtGLGL: Failed to attach resource: 0x%x\n", kr);
        return false;
    }
    
    printf("VirtGLGL: Attached resource %u to context %u\n", resourceId, contextId);
    return true;
}

uint32_t VirtGLGL_GetCapability(VirtGLGLClientRef client, uint32_t cap)
{
    if (!client) return 0;
    
    uint64_t input = cap;
    uint64_t output = 0;
    uint32_t outputCount = 1;
    
    kern_return_t kr = IOConnectCallScalarMethod(
        client->connection,
        kVMVirtIOGPU_GetCapability,
        &input,
        1,
        &output,
        &outputCount
    );
    
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "VirtGLGL: Failed to get capability: 0x%x\n", kr);
        return 0;
    }
    
    return (uint32_t)output;
}

bool VirtGLGL_TransferToHost2D(VirtGLGLClientRef client, uint32_t resourceId,
                               uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
    if (!client) return false;
    
    uint64_t input[5] = { resourceId, x, y, width, height };
    
    kern_return_t kr = IOConnectCallScalarMethod(
        client->connection,
        kVMVirtIOGPU_TransferToHost2D,
        input,
        5,
        NULL,
        NULL
    );
    
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "VirtGLGL: Failed to transfer to host: 0x%x\n", kr);
        return false;
    }
    
    return true;
}

bool VirtGLGL_FlushResource(VirtGLGLClientRef client, uint32_t resourceId,
                            uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
    if (!client) return false;
    
    uint64_t input[5] = { resourceId, x, y, width, height };
    
    kern_return_t kr = IOConnectCallScalarMethod(
        client->connection,
        kVMVirtIOGPU_FlushResource,
        input,
        5,
        NULL,
        NULL
    );
    
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "VirtGLGL: Failed to flush resource: 0x%x\n", kr);
        return false;
    }
    
    return true;
}

bool VirtGLGL_SetScanout(VirtGLGLClientRef client, uint32_t scanoutId, uint32_t resourceId,
                         uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
    if (!client) return false;
    
    uint64_t input[6] = { scanoutId, resourceId, x, y, width, height };
    
    kern_return_t kr = IOConnectCallScalarMethod(
        client->connection,
        kVMVirtIOGPU_SetScanout,
        input,
        6,
        NULL,
        NULL
    );
    
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "VirtGLGL: Failed to set scanout: 0x%x\n", kr);
        return false;
    }
    
    return true;
}

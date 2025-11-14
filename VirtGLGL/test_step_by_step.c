/*
 * test_step_by_step.c
 * Test VirtGLGL step-by-step to find where it crashes
 */

#include <stdio.h>
#include <stdlib.h>
#include <IOKit/IOKitLib.h>

// Minimal VirtGLGL connection structure
typedef struct {
    io_connect_t connection;
    uint32_t nextResourceId;
    uint32_t nextContextId;
} VirtGLGLClient;

// Selectors
enum {
    kVMVirtIOGPU_CreateResource = 0x3001,
    kVMVirtIOGPU_CreateContext = 0x3002,
};

int main()
{
    printf("=== VirtGLGL Step-by-Step Test ===\n\n");
    
    // Step 1: Find service
    printf("Step 1: Finding VMVirtIOGPUAccelerator...\n");
    mach_port_t masterPort = 0;
    io_service_t service = IOServiceGetMatchingService(masterPort, 
                                                       IOServiceMatching("VMVirtIOGPUAccelerator"));
    if (!service) {
        printf("ERROR: Service not found\n");
        return 1;
    }
    printf("SUCCESS: Service found\n");
    
    // Step 2: Open connection
    printf("\nStep 2: Opening type 4 connection...\n");
    io_connect_t connection;
    kern_return_t kr = IOServiceOpen(service, mach_task_self(), 4, &connection);
    IOObjectRelease(service);
    if (kr != KERN_SUCCESS) {
        printf("ERROR: Failed to open connection: 0x%x\n", kr);
        return 1;
    }
    printf("SUCCESS: Connection opened (0x%x)\n", connection);
    
    // Step 3: Allocate client structure
    printf("\nStep 3: Creating client structure...\n");
    VirtGLGLClient* client = (VirtGLGLClient*)malloc(sizeof(VirtGLGLClient));
    if (!client) {
        printf("ERROR: Failed to allocate memory\n");
        IOServiceClose(connection);
        return 1;
    }
    client->connection = connection;
    client->nextResourceId = 1;
    client->nextContextId = 1;
    printf("SUCCESS: Client structure created\n");
    
    // Step 4: Try to create a resource
    printf("\nStep 4: Attempting CreateResource (selector 0x3001)...\n");
    uint64_t input[4] = { 1, 800, 600, 67 }; // resourceId, width, height, format
    kr = IOConnectCallScalarMethod(connection, kVMVirtIOGPU_CreateResource,
                                   input, 4, NULL, NULL);
    if (kr != KERN_SUCCESS) {
        printf("WARNING: CreateResource failed: 0x%x\n", kr);
    } else {
        printf("SUCCESS: CreateResource succeeded!\n");
    }
    
    // Step 5: Try to create a context
    printf("\nStep 5: Attempting CreateContext (selector 0x3002)...\n");
    uint64_t input2[1] = { 1 }; // contextId
    kr = IOConnectCallScalarMethod(connection, kVMVirtIOGPU_CreateContext,
                                   input2, 1, NULL, NULL);
    if (kr != KERN_SUCCESS) {
        printf("WARNING: CreateContext failed: 0x%x\n", kr);
    } else {
        printf("SUCCESS: CreateContext succeeded!\n");
    }
    
    // Cleanup
    printf("\nStep 6: Cleanup...\n");
    IOServiceClose(connection);
    free(client);
    printf("SUCCESS: Cleanup complete\n");
    
    printf("\n=== Test Complete - No Crashes! ===\n");
    printf("\nNow check kernel logs with:\n");
    printf("sudo dmesg | grep -E 'externalMethod|selector=|CreateResource|CreateContext' | tail -20\n");
    
    return 0;
}

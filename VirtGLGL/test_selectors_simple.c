/*
 * Simple test to verify selectors 0x4003 and 0x4004 reach the driver
 */

#include <stdio.h>
#include <IOKit/IOKitLib.h>

int main(void)
{
    kern_return_t ret;
    io_service_t service;
    io_connect_t connect = 0;
    mach_port_t masterPort;
    
    printf("=== Testing Selectors 0x4003 and 0x4004 ===\n\n");
    
    // Get master port for Snow Leopard compatibility
    ret = IOMasterPort(MACH_PORT_NULL, &masterPort);
    if (ret != KERN_SUCCESS) {
        fprintf(stderr, "❌ IOMasterPort failed: 0x%x\n", ret);
        return 1;
    }
    
    // Find the driver - use VMVirtIOGPUAccelerator (provides the user client)
    service = IOServiceGetMatchingService(masterPort, 
                IOServiceMatching("VMVirtIOGPUAccelerator"));
    if (!service) {
        fprintf(stderr, "❌ Could not find VMVirtIOGPUAccelerator\n");
        return 1;
    }
    printf("✅ Found VMVirtIOGPUAccelerator\n");
    
    // Open connection with type 4 (VMVirtIOGPUUserClient)
    printf("Opening connection with IOServiceOpen (type 4)...\n");
    fflush(stdout);
    ret = IOServiceOpen(service, mach_task_self(), 4, &connect);
    printf("IOServiceOpen returned: 0x%x\n", ret);
    fflush(stdout);
    IOObjectRelease(service);
    if (ret != KERN_SUCCESS) {
        fprintf(stderr, "❌ IOServiceOpen failed: 0x%x\n", ret);
        return 1;
    }
    printf("✅ Opened connection to driver (connect=0x%x)\n\n", (unsigned int)connect);
    
    // Test selector 0x4003 (CreateResource)
    printf("Testing selector 0x4003 (CreateResource)...\n");
    uint64_t scalarIn[5] = {100, 640, 480, 67, 0};  // resourceId, width, height, format, flags
    uint32_t outputCount = 0;
    ret = IOConnectCallScalarMethod(connect, 0x4003, scalarIn, 5, NULL, &outputCount);
    printf("  Result: 0x%x (%s)\n", ret, ret == KERN_SUCCESS ? "SUCCESS" : "FAILED");
    
    // Test selector 0x4004 (CreateContext)  
    printf("Testing selector 0x4004 (CreateContext)...\n");
    uint64_t contextIn[1] = {1};  // contextId
    outputCount = 0;
    ret = IOConnectCallScalarMethod(connect, 0x4004, contextIn, 1, NULL, &outputCount);
    printf("  Result: 0x%x (%s)\n", ret, ret == KERN_SUCCESS ? "SUCCESS" : "FAILED");
    
    IOServiceClose(connect);
    
    printf("\n✅ Test complete\n");
    printf("Check kernel logs with: sudo dmesg | grep -E 'CreateResource|CreateContext'\n");
    
    return 0;
}

/*
 * test_selector_dispatch.c  
 * Test if selector 0x3001 actually reaches the driver or gets intercepted
 */

#include <stdio.h>
#include <IOKit/IOKitLib.h>

int main()
{
    printf("=== Selector Dispatch Test ===\n\n");
    
    // Open connection
    io_service_t service = IOServiceGetMatchingService(0, IOServiceMatching("VMVirtIOGPUAccelerator"));
    if (!service) {
        printf("ERROR: Service not found\n");
        return 1;
    }
    
    io_connect_t connection;
    kern_return_t kr = IOServiceOpen(service, mach_task_self(), 4, &connection);
    IOObjectRelease(service);
    if (kr != KERN_SUCCESS) {
        printf("ERROR: Failed to open connection: 0x%x\n", kr);
        return 1;
    }
    printf("Connection opened: 0x%x\n\n", connection);
    
    // Test selector 0x3001 (CreateResource) 
    printf("Testing selector 0x3001 (CreateResource):\n");
    uint64_t input1[4] = { 1, 800, 600, 67 };
    kr = IOConnectCallScalarMethod(connection, 0x3001, input1, 4, NULL, NULL);
    printf("  Return code: 0x%x (%s)\n", kr, kr == KERN_SUCCESS ? "SUCCESS" : "FAILED");
    printf("  Kernel should log: 'externalMethod() ENTRY: selector=12289 (0x3001)'\n\n");
    
    // Test selector 0x3002 (CreateContext)
    printf("Testing selector 0x3002 (CreateContext):\n");
    uint64_t input2[1] = { 1 };
    kr = IOConnectCallScalarMethod(connection, 0x3002, input2, 1, NULL, NULL);
    printf("  Return code: 0x%x (%s)\n", kr, kr == KERN_SUCCESS ? "SUCCESS" : "FAILED");
    printf("  Kernel should log: 'externalMethod() ENTRY: selector=12290 (0x3002)'\n\n");
    
    // Test selector 0x3003 (AttachResource)
    printf("Testing selector 0x3003 (AttachResource):\n");
    uint64_t input3[2] = { 1, 1 };
    kr = IOConnectCallScalarMethod(connection, 0x3003, input3, 2, NULL, NULL);
    printf("  Return code: 0x%x (%s)\n", kr, kr == KERN_SUCCESS ? "SUCCESS" : "FAILED");
    printf("  Kernel should log: 'externalMethod() ENTRY: selector=12291 (0x3003)'\n\n");
    
    IOServiceClose(connection);
    
    printf("=== Test Complete ===\n");
    printf("Check kernel logs with: sudo dmesg | grep 'externalMethod.*ENTRY' | tail -10\n");
    
    return 0;
}

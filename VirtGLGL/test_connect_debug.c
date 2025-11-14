/*
 * test_connect_debug.c
 * Debug version to test type 4 user client connection
 */

#include <stdio.h>
#include <IOKit/IOKitLib.h>

int main()
{
    printf("=== VirtGLGL Type 4 Connection Debug Test ===\n\n");
    
    // Test 1: Find the service
    printf("1. Looking for VMVirtIOGPUAccelerator service...\n");
    mach_port_t masterPort = 0;
    io_service_t service = IOServiceGetMatchingService(masterPort, 
                                                       IOServiceMatching("VMVirtIOGPUAccelerator"));
    if (!service) {
        printf("   ERROR: Service not found\n");
        return 1;
    }
    printf("   SUCCESS: Found service (handle: 0x%x)\n", service);
    
    // Test 2: Try type 0 first
    printf("\n2. Testing type 0 connection...\n");
    io_connect_t connection0;
    kern_return_t kr = IOServiceOpen(service, mach_task_self(), 0, &connection0);
    if (kr == KERN_SUCCESS) {
        printf("   SUCCESS: Type 0 works (connection: 0x%x)\n", connection0);
        IOServiceClose(connection0);
    } else {
        printf("   FAILED: Type 0 returned 0x%x\n", kr);
    }
    
    // Test 3: Try type 4 (VMVirtIOGPUUserClient)
    printf("\n3. Testing type 4 connection (VMVirtIOGPUUserClient)...\n");
    io_connect_t connection4;
    kr = IOServiceOpen(service, mach_task_self(), 4, &connection4);
    if (kr == KERN_SUCCESS) {
        printf("   SUCCESS: Type 4 works (connection: 0x%x)\n", connection4);
        IOServiceClose(connection4);
    } else {
        printf("   FAILED: Type 4 returned 0x%x\n", kr);
        printf("   Error meanings:\n");
        printf("     0xe00002c2 = kIOReturnUnsupported (type not implemented)\n");
        printf("     0xe00002c7 = kIOReturnNotPrivileged (need root?)\n");
        printf("     0xe00002bd = kIOReturnNoMemory\n");
    }
    
    IOObjectRelease(service);
    
    printf("\n=== Test Complete ===\n");
    return 0;
}

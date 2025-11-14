/*
 * test_simple.c
 * Minimal C test to debug connection issues
 */

#include <stdio.h>
#include <IOKit/IOKitLib.h>

int main()
{
    printf("=== VirtGLGL Simple Connection Test (C version) ===\n\n");
    
    // Test 1: Can we access IOKit?
    printf("1. Testing IOKit access...\n");
    mach_port_t masterPort = 0;
    printf("   Master port: %u\n", masterPort);
    
    // Test 2: Can we find the service?
    printf("2. Looking for VMVirtIOGPUAccelerator service...\n");
    io_service_t service = IOServiceGetMatchingService(masterPort, 
                                                       IOServiceMatching("VMVirtIOGPUAccelerator"));
    if (!service) {
        printf("   ERROR: VMVirtIOGPUAccelerator service not found\n");
        return 1;
    }
    
    printf("   SUCCESS: Found VMVirtIOGPUAccelerator service (handle: 0x%x)\n", service);
    
    // Test 3: Can we open a user client?
    printf("3. Opening user client connection...\n");
    io_connect_t connection;
    kern_return_t kr = IOServiceOpen(service, mach_task_self(), 0, &connection);
    IOObjectRelease(service);
    
    if (kr != KERN_SUCCESS) {
        printf("   ERROR: Failed to open user client: 0x%x\n", kr);
        return 1;
    }
    
    printf("   SUCCESS: User client opened (connection: 0x%x)\n", connection);
    
    // Test 4: Close connection
    printf("4. Closing connection...\n");
    IOServiceClose(connection);
    printf("   SUCCESS: Connection closed\n\n");
    
    printf("=== Test Complete - All Steps Passed! ===\n");
    return 0;
}

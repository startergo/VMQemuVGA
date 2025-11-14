/*
 * test_simple.cpp
 * Minimal test to debug connection issues
 */

#include <stdio.h>
#include <IOKit/IOKitLib.h>

int main()
{
    printf("=== VirtGLGL Simple Connection Test ===\n\n");
    
    // Test 1: Can we access IOKit?
    printf("1. Testing IOKit access...\n");
    mach_port_t masterPort = 0;
    printf("   Master port: %u\n", masterPort);
    
    // Test 2: Can we find the service?
    printf("2. Looking for VMVirtIOGPU service...\n");
    io_service_t service = IOServiceGetMatchingService(masterPort, 
                                                       IOServiceMatching("VMVirtIOGPU"));
    if (!service) {
        printf("   ❌ VMVirtIOGPU service not found\n");
        printf("   Trying to list all services...\n");
        
        // List what services are available
        CFMutableDictionaryRef matchingDict = IOServiceMatching("IOService");
        io_iterator_t iterator;
        kern_return_t kr = IOServiceGetMatchingServices(masterPort, matchingDict, &iterator);
        if (kr == KERN_SUCCESS) {
            io_service_t serv;
            int count = 0;
            while ((serv = IOIteratorNext(iterator)) && count < 10) {
                char name[128];
                IORegistryEntryGetName(serv, name);
                printf("   - %s\n", name);
                IOObjectRelease(serv);
                count++;
            }
            IOObjectRelease(iterator);
        }
        
        return 1;
    }
    
    printf("   ✅ Found VMVirtIOGPU service (handle: 0x%x)\n", service);
    
    // Test 3: Can we open a user client?
    printf("3. Opening user client connection...\n");
    io_connect_t connection;
    kern_return_t kr = IOServiceOpen(service, mach_task_self(), 0, &connection);
    IOObjectRelease(service);
    
    if (kr != KERN_SUCCESS) {
        printf("   ❌ Failed to open user client: 0x%x\n", kr);
        return 1;
    }
    
    printf("   ✅ User client opened (connection: 0x%x)\n", connection);
    
    // Test 4: Close connection
    printf("4. Closing connection...\n");
    IOServiceClose(connection);
    printf("   ✅ Connection closed\n\n");
    
    printf("=== Test Complete - All Steps Passed! ===\n");
    return 0;
}

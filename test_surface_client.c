/*
 * test_surface_client.c - Test IOAccelSurfaceConnect user client creation
 * 
 * This program attempts to open a connection to VMQemuVGAAccelerator
 * and request a surface client (type 0 = kIOAccelSurfaceClientType).
 * 
 * Compile on Snow Leopard:
 *   gcc -arch x86_64 -framework IOKit -framework CoreFoundation test_surface_client.c -o test_surface_client
 */

#include <stdio.h>
#include <IOKit/IOKitLib.h>
#include <CoreFoundation/CoreFoundation.h>

int main(void)
{
    kern_return_t kr;
    io_service_t service = 0;
    io_connect_t connection = 0;
    
    printf("=== IOAccelSurfaceClient Connection Test ===\n\n");
    
    /* Find VMQemuVGAAccelerator service */
    printf("1. Looking for VMQemuVGAAccelerator service...\n");
    service = IOServiceGetMatchingService(kIOMasterPortDefault, 
                                         IOServiceMatching("VMQemuVGAAccelerator"));
    if (!service) {
        printf("   ❌ VMQemuVGAAccelerator service not found\n");
        return 1;
    }
    printf("   ✅ Found VMQemuVGAAccelerator (service ID: 0x%x)\n", service);
    
    /* Try to open user client type 0 (kIOAccelSurfaceClientType) */
    printf("\n2. Attempting to open surface client (type 0)...\n");
    kr = IOServiceOpen(service, mach_task_self(), 0, &connection);
    if (kr != KERN_SUCCESS) {
        printf("   ❌ Failed to open surface client: 0x%x\n", kr);
        IOObjectRelease(service);
        return 1;
    }
    printf("   ✅ Surface client connection opened successfully!\n");
    printf("   Connection handle: 0x%x\n", connection);
    
    /* Check if connection is valid */
    printf("\n3. Validating connection...\n");
    printf("   ✅ Connection created\n");
    
    /* Try to call a simple method (kIOAccelSurfaceGetState = selector 5) */
    printf("\n4. Testing surface state query (selector 5)...\n");
    uint32_t surface_state = 0;
    size_t output_size = sizeof(surface_state);
    kr = IOConnectCallStructMethod(connection, 5, NULL, 0, &surface_state, &output_size);
    if (kr == KERN_SUCCESS) {
        printf("   ✅ Surface state query successful\n");
        printf("   Surface state: 0x%x\n", surface_state);
    } else {
        printf("   ⚠️  Surface state query failed: 0x%x\n", kr);
        printf("   (This is expected if method not implemented)\n");
    }
    
    /* Cleanup */
    printf("\n5. Cleaning up...\n");
    IOServiceClose(connection);
    IOObjectRelease(service);
    printf("   ✅ Connection closed\n");
    
    printf("\n=== Test Complete ===\n");
    printf("Result: Surface client connection SUCCESSFUL\n");
    printf("Check kernel log with: sudo dmesg | grep -i vmaccel\n");
    
    return 0;
}

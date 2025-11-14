/*
 * test_virgl.c
 * Test virgl command submission to kernel driver
 */

#include <stdio.h>
#include <IOKit/IOKitLib.h>

int main()
{
    printf("=== VirtGLGL Virgl Command Test ===\n\n");
    
    // Connect to VMVirtIOGPUAccelerator
    printf("1. Connecting to VMVirtIOGPUAccelerator...\n");
    mach_port_t masterPort = 0;
    io_service_t service = IOServiceGetMatchingService(masterPort, 
                                                       IOServiceMatching("VMVirtIOGPUAccelerator"));
    if (!service) {
        printf("   ERROR: Service not found\n");
        return 1;
    }
    printf("   SUCCESS: Service found\n");
    
    // Open user client (type 4 = VMVirtIOGPUUserClient)
    printf("2. Opening VMVirtIOGPUUserClient (type 4)...\n");
    io_connect_t connection;
    kern_return_t kr = IOServiceOpen(service, mach_task_self(), 4, &connection);
    IOObjectRelease(service);
    
    if (kr != KERN_SUCCESS) {
        printf("   ERROR: Failed to open user client type 4: 0x%x\n", kr);
        return 1;
    }
    printf("   SUCCESS: UserClient opened (connection: 0x%x)\n", connection);
    
    // Test 3: Get capability (selector 0x3004)
    printf("3. Testing GetCapability (selector 0x3004)...\n");
    uint64_t input = 1;  // Cap 1 = Supports 3D
    uint64_t output = 0;
    uint32_t outputCount = 1;
    
    kr = IOConnectCallScalarMethod(connection, 0x3004, &input, 1, &output, &outputCount);
    if (kr == KERN_SUCCESS) {
        printf("   SUCCESS: Capability 1 (Supports 3D) = %llu\n", output);
    } else {
        printf("   ERROR: GetCapability failed: 0x%x\n", kr);
    }
    
    // Test 4: Create context (selector 0x3002)
    printf("4. Testing CreateContext (selector 0x3002)...\n");
    input = 1;  // Context ID 1
    kr = IOConnectCallScalarMethod(connection, 0x3002, &input, 1, NULL, NULL);
    if (kr == KERN_SUCCESS) {
        printf("   SUCCESS: Created context 1\n");
    } else {
        printf("   ERROR: CreateContext failed: 0x%x\n", kr);
    }
    
    // Test 5: Create resource (selector 0x3001)
    printf("5. Testing CreateResource (selector 0x3001)...\n");
    uint64_t resourceInput[4] = { 1, 800, 600, 67 };  // ID=1, 800x600, format=RGBA
    kr = IOConnectCallScalarMethod(connection, 0x3001, resourceInput, 4, NULL, NULL);
    if (kr == KERN_SUCCESS) {
        printf("   SUCCESS: Created resource 1 (800x600)\n");
    } else {
        printf("   ERROR: CreateResource failed: 0x%x\n", kr);
    }
    
    // Test 6: Attach resource (selector 0x3003)
    printf("6. Testing AttachResource (selector 0x3003)...\n");
    uint64_t attachInput[2] = { 1, 1 };  // Context 1, Resource 1
    kr = IOConnectCallScalarMethod(connection, 0x3003, attachInput, 2, NULL, NULL);
    if (kr == KERN_SUCCESS) {
        printf("   SUCCESS: Attached resource 1 to context 1\n");
    } else {
        printf("   ERROR: AttachResource failed: 0x%x\n", kr);
    }
    
    // Test 7: Submit virgl commands (selector 0x3000)
    printf("7. Testing SubmitCommands (selector 0x3000)...\n");
    // Simple virgl CLEAR command
    uint32_t virgl_cmd[8] = {
        0x00000607,  // Header: CLEAR command (7), length 6 dwords
        0x00000004,  // Clear color buffer
        0x3f800000,  // Red = 1.0
        0x00000000,  // Green = 0.0
        0x00000000,  // Blue = 0.0
        0x3f800000,  // Alpha = 1.0
        0x3f800000,  // Depth = 1.0
        0x00000000   // Stencil = 0
    };
    
    kr = IOConnectCallStructMethod(connection, 0x3000, virgl_cmd, sizeof(virgl_cmd), NULL, NULL);
    if (kr == KERN_SUCCESS) {
        printf("   SUCCESS: Submitted virgl CLEAR command (32 bytes)\n");
    } else {
        printf("   ERROR: SubmitCommands failed: 0x%x\n", kr);
    }
    
    // Close
    printf("8. Closing connection...\n");
    IOServiceClose(connection);
    printf("   SUCCESS: Connection closed\n\n");
    
    printf("=== Test Complete ===\n");
    return 0;
}

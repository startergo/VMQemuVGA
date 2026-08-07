/*
 * test_surface_operations.c - Test IOAccelSurface operations
 * 
 * This program tests the actual surface operation methods:
 * - GetState (query surface status)
 * - ReadLock/ReadUnlock (lock for reading)
 * - WriteLock/WriteUnlock (lock for writing)  
 * - Flush (synchronize to display)
 * 
 * Compile on Snow Leopard:
 *   gcc -arch x86_64 -framework IOKit -framework CoreFoundation test_surface_operations.c -o test_surface_operations
 */

#include <stdio.h>
#include <IOKit/IOKitLib.h>
#include <CoreFoundation/CoreFoundation.h>

/* IOAccelSurface method selectors from IOAccelSurfaceConnect.h */
enum {
    kIOAccelSurfaceReadLockOptions = 0,
    kIOAccelSurfaceReadUnlockOptions = 1,
    kIOAccelSurfaceGetState = 2,
    kIOAccelSurfaceWriteLockOptions = 3,
    kIOAccelSurfaceWriteUnlockOptions = 4,
    kIOAccelSurfaceRead = 5,
    kIOAccelSurfaceSetShapeBacking = 6,
    kIOAccelSurfaceSetIDMode = 7,
    kIOAccelSurfaceSetScale = 8,
    kIOAccelSurfaceSetShape = 9,
    kIOAccelSurfaceFlush = 10
};

int main(void)
{
    kern_return_t kr;
    io_service_t service = 0;
    io_connect_t connection = 0;
    
    printf("=== IOAccelSurface Operations Test ===\n\n");
    
    /* Find VMQemuVGAAccelerator service */
    printf("1. Finding VMQemuVGAAccelerator...\n");
    service = IOServiceGetMatchingService(kIOMasterPortDefault, 
                                         IOServiceMatching("VMQemuVGAAccelerator"));
    if (!service) {
        printf("   ❌ Service not found\n");
        return 1;
    }
    printf("   ✅ Found service\n");
    
    /* Open surface client */
    printf("\n2. Opening surface client (type 0)...\n");
    kr = IOServiceOpen(service, mach_task_self(), 0, &connection);
    if (kr != KERN_SUCCESS) {
        printf("   ❌ Failed to open: 0x%x\n", kr);
        IOObjectRelease(service);
        return 1;
    }
    printf("   ✅ Surface client opened\n");
    
    /* Test 1: GetState (selector 2) */
    printf("\n3. Testing GetState (selector 2)...\n");
    uint32_t state = 0;
    kr = IOConnectCallScalarMethod(connection, kIOAccelSurfaceGetState,
                                   NULL, 0,  /* No inputs */
                                   (uint64_t*)&state, (uint32_t[]){1});  /* 1 output */
    if (kr == KERN_SUCCESS) {
        printf("   ✅ GetState SUCCESS - state = 0x%x\n", state);
        if (state & 0x1) {
            printf("      Surface is IDLE\n");
        }
    } else {
        printf("   ⚠️  GetState failed: 0x%x\n", kr);
    }
    
    /* Test 2: WriteLock (selector 3) */
    printf("\n4. Testing WriteLock (selector 3)...\n");
    uint64_t lock_options = 0;  /* kIOAccelSurfaceLockInDontCare */
    kr = IOConnectCallScalarMethod(connection, kIOAccelSurfaceWriteLockOptions,
                                   &lock_options, 1,  /* 1 input */
                                   NULL, NULL);  /* No outputs */
    if (kr == KERN_SUCCESS) {
        printf("   ✅ WriteLock SUCCESS\n");
    } else {
        printf("   ⚠️  WriteLock failed: 0x%x\n", kr);
    }
    
    /* Test 3: WriteUnlock (selector 4) */
    printf("\n5. Testing WriteUnlock (selector 4)...\n");
    uint64_t unlock_options = 0;
    kr = IOConnectCallScalarMethod(connection, kIOAccelSurfaceWriteUnlockOptions,
                                   &unlock_options, 1,
                                   NULL, NULL);
    if (kr == KERN_SUCCESS) {
        printf("   ✅ WriteUnlock SUCCESS\n");
    } else {
        printf("   ⚠️  WriteUnlock failed: 0x%x\n", kr);
    }
    
    /* Test 4: Flush (selector 10) */
    printf("\n6. Testing Flush (selector 10)...\n");
    kr = IOConnectCallScalarMethod(connection, kIOAccelSurfaceFlush,
                                   NULL, 0,  /* No inputs */
                                   NULL, NULL);  /* No outputs */
    if (kr == KERN_SUCCESS) {
        printf("   ✅ Flush SUCCESS - surface synchronized to display\n");
    } else {
        printf("   ⚠️  Flush failed: 0x%x\n", kr);
    }
    
    /* Test 5: ReadLock (selector 0) */
    printf("\n7. Testing ReadLock (selector 0)...\n");
    uint64_t read_lock_options = 0;
    kr = IOConnectCallScalarMethod(connection, kIOAccelSurfaceReadLockOptions,
                                   &read_lock_options, 1,
                                   NULL, NULL);
    if (kr == KERN_SUCCESS) {
        printf("   ✅ ReadLock SUCCESS\n");
    } else {
        printf("   ⚠️  ReadLock failed: 0x%x\n", kr);
    }
    
    /* Test 6: ReadUnlock (selector 1) */
    printf("\n8. Testing ReadUnlock (selector 1)...\n");
    uint64_t read_unlock_options = 0;
    kr = IOConnectCallScalarMethod(connection, kIOAccelSurfaceReadUnlockOptions,
                                   &read_unlock_options, 1,
                                   NULL, NULL);
    if (kr == KERN_SUCCESS) {
        printf("   ✅ ReadUnlock SUCCESS\n");
    } else {
        printf("   ⚠️  ReadUnlock failed: 0x%x\n", kr);
    }
    
    /* Cleanup */
    printf("\n9. Cleaning up...\n");
    IOServiceClose(connection);
    IOObjectRelease(service);
    printf("   ✅ Connection closed\n");
    
    printf("\n=== Test Complete ===\n");
    printf("Check kernel log: sudo dmesg | grep -iE 'vmaccel|lock|flush'\n");
    
    return 0;
}

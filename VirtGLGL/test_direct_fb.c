// test_direct_fb.c - Test writing pixels directly to framebuffer resource
// Bypasses virgl entirely - writes raw pixel data

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <IOKit/IOKitLib.h>
#include <CoreFoundation/CoreFoundation.h>

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 600

int main(void)
{
    printf("=== Direct Framebuffer Write Test ===\n\n");
    
    // Open IOFramebuffer service
    io_service_t service = IOServiceGetMatchingService(kIOMasterPortDefault, 
                                                       IOServiceMatching("IOFramebuffer"));
    if (!service) {
        printf("ERROR: Failed to find IOFramebuffer service\n");
        return 1;
    }
    
    printf("1. Found IOFramebuffer service\n");
    
    // Get VRAM address
    io_connect_t connect = 0;
    kern_return_t kr = IOServiceOpen(service, mach_task_self(), 0, &connect);
    if (kr != KERN_SUCCESS) {
        printf("ERROR: IOServiceOpen failed: 0x%x\n", kr);
        IOObjectRelease(service);
        return 1;
    }
    
    printf("2. Opened IOFramebuffer connection\n");
    
    // Try to map framebuffer memory
    // Method 0: Map framebuffer memory
    mach_vm_address_t address = 0;
    mach_vm_size_t size = 0;
    
    kr = IOConnectMapMemory(connect, 0, mach_task_self(), 
                           &address, &size, kIOMapAnywhere);
    
    if (kr != KERN_SUCCESS) {
        printf("ERROR: IOConnectMapMemory failed: 0x%x\n", kr);
        IOServiceClose(connect);
        IOObjectRelease(service);
        return 1;
    }
    
    printf("3. Mapped framebuffer memory:\n");
    printf("   Address: 0x%llx\n", address);
    printf("   Size: %llu bytes (%llu MB)\n", size, size / (1024*1024));
    
    // Calculate expected size
    size_t expected_size = SCREEN_WIDTH * SCREEN_HEIGHT * 4; // 4 bytes per pixel (BGRA)
    printf("   Expected size: %zu bytes (%.2f MB)\n", expected_size, expected_size / (1024.0*1024.0));
    
    if (size < expected_size) {
        printf("WARNING: Mapped size smaller than expected!\n");
    }
    
    // Cast to uint32_t pointer for pixel manipulation
    uint32_t* pixels = (uint32_t*)address;
    
    // Fill screen with red color (0xFFFF0000 = BGRA format, full alpha red)
    printf("\n4. Writing red pixels to framebuffer...\n");
    
    uint32_t red_pixel = 0xFFFF0000; // BGRA: A=FF, R=FF, G=00, B=00
    
    for (int y = 0; y < SCREEN_HEIGHT; y++) {
        for (int x = 0; x < SCREEN_WIDTH; x++) {
            pixels[y * SCREEN_WIDTH + x] = red_pixel;
        }
        
        if (y % 100 == 0) {
            printf("   Row %d/%d\n", y, SCREEN_HEIGHT);
        }
    }
    
    printf("   SUCCESS: Wrote %d pixels\n", SCREEN_WIDTH * SCREEN_HEIGHT);
    
    printf("\n5. Screen should now be RED!\n");
    printf("   Keeping it red for 5 seconds...\n");
    sleep(5);
    
    // Cleanup
    IOConnectUnmapMemory(connect, 0, mach_task_self(), address);
    IOServiceClose(connect);
    IOObjectRelease(service);
    
    printf("\n=== Test Complete ===\n");
    return 0;
}

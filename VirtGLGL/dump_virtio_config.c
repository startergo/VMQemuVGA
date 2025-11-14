// dump_virtio_config.c - Read VirtIO GPU configuration directly
#include <stdio.h>
#include <stdint.h>
#include <IOKit/IOKitLib.h>
#include <CoreFoundation/CoreFoundation.h>

int main(void)
{
    io_service_t service;
    io_connect_t connect;
    kern_return_t kr;
    
    // Find the VMVirtIOGPUAccelerator service
    service = IOServiceGetMatchingService(kIOMasterPortDefault, 
                                          IOServiceMatching("VMVirtIOGPUAccelerator"));
    if (!service) {
        printf("ERROR: VMVirtIOGPUAccelerator not found\n");
        return 1;
    }
    
    printf("✅ Found VMVirtIOGPUAccelerator service\n");
    
    // Get properties
    CFMutableDictionaryRef props = NULL;
    kr = IORegistryEntryCreateCFProperties(service, &props, kCFAllocatorDefault, 0);
    if (kr == KERN_SUCCESS && props) {
        // Try to find capsets or 3D related properties
        CFTypeRef value;
        
        value = CFDictionaryGetValue(props, CFSTR("num_capsets"));
        if (value) {
            int num = 0;
            CFNumberGetValue((CFNumberRef)value, kCFNumberIntType, &num);
            printf("num_capsets (from ioreg): %d\n", num);
        } else {
            printf("num_capsets property not found in ioreg\n");
        }
        
        value = CFDictionaryGetValue(props, CFSTR("supports3D"));
        if (value) {
            Boolean val = CFBooleanGetValue((CFBooleanRef)value);
            printf("supports3D (from ioreg): %s\n", val ? "YES" : "NO");
        } else {
            printf("supports3D property not found in ioreg\n");
        }
        
        CFRelease(props);
    }
    
    IOObjectRelease(service);
    
    printf("\n=== Summary ===\n");
    printf("Check kernel logs with: sudo dmesg | grep -E 'capset|3D|hardware config'\n");
    
    return 0;
}

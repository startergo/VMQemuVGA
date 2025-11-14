#include <stdio.h>
#include <IOKit/IOKitLib.h>
#include <CoreFoundation/CoreFoundation.h>

int main() {
    printf("Checking VMVirtIOGPU 3D support...\n");
    
    // Find the VMVirtIOGPU service
    CFMutableDictionaryRef matching = IOServiceMatching("VMVirtIOGPU");
    if (!matching) {
        printf("ERROR: Failed to create matching dictionary\n");
        return 1;
    }
    
    io_service_t service = IOServiceGetMatchingService(kIOMasterPortDefault, matching);
    if (!service) {
        printf("ERROR: VMVirtIOGPU service not found\n");
        return 1;
    }
    
    printf("✓ Found VMVirtIOGPU service\n");
    
    // Check if supports3D property exists
    CFTypeRef supports3D = IORegistryEntryCreateCFProperty(service, CFSTR("supports3D"), kCFAllocatorDefault, 0);
    if (supports3D) {
        if (CFGetTypeID(supports3D) == CFBooleanGetTypeID()) {
            Boolean value = CFBooleanGetValue((CFBooleanRef)supports3D);
            printf("✓ supports3D property = %s\n", value ? "TRUE" : "FALSE");
            
            if (value) {
                printf("\n🎉 SUCCESS! 3D acceleration is ENABLED!\n");
                CFRelease(supports3D);
                IOObjectRelease(service);
                return 0;
            } else {
                printf("\n❌ FAILED: 3D acceleration is disabled\n");
            }
        }
        CFRelease(supports3D);
    } else {
        printf("⚠ supports3D property not found in registry\n");
    }
    
    // Check num_capsets
    CFTypeRef num_capsets = IORegistryEntryCreateCFProperty(service, CFSTR("num_capsets"), kCFAllocatorDefault, 0);
    if (num_capsets) {
        if (CFGetTypeID(num_capsets) == CFNumberGetTypeID()) {
            int value = 0;
            CFNumberGetValue((CFNumberRef)num_capsets, kCFNumberIntType, &value);
            printf("✓ num_capsets property = %d\n", value);
            
            if (value > 0) {
                printf("\n🎉 SUCCESS! Hardware reports %d capability sets!\n", value);
                CFRelease(num_capsets);
                IOObjectRelease(service);
                return 0;
            }
        }
        CFRelease(num_capsets);
    } else {
        printf("⚠ num_capsets property not found in registry\n");
    }
    
    IOObjectRelease(service);
    printf("\n❌ 3D acceleration check FAILED\n");
    return 1;
}

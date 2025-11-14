#include <IOKit/IOKitLib.h>
#include <stdio.h>

int main(void) {
    mach_port_t mp = 0;
    io_service_t svc;
    
    printf("Test started\n");
    
    svc = IOServiceGetMatchingService(mp, IOServiceMatching("VMVirtIOGPUAccelerator"));
    printf("IOServiceGetMatchingService returned: %d\n", svc);
    
    if (svc) {
        printf("Found service!\n");
        IOObjectRelease(svc);
    } else {
        printf("Service not found\n");
    }
    
    return 0;
}

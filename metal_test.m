#import <Metal/Metal.h>
#import <Foundation/Foundation.h>

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        // Get the default Metal device
        id<MTLDevice> device = MTLCreateSystemDefaultDevice();
        
        if (device == nil) {
            NSLog(@"Metal is not supported on this device");
            return 1;
        }
        
        NSLog(@"=== Metal Device Information ===");
        NSLog(@"Device Name: %@", device.name);
        NSLog(@"Low Power: %d", device.lowPower);
        NSLog(@"Headless: %d", device.headless);
        NSLog(@"Removable: %d", device.removable);
        
        // Check for hardware acceleration
        if ([device.name rangeOfString:@"Software" options:NSCaseInsensitiveSearch].location != NSNotFound ||
            [device.name rangeOfString:@"Emulated" options:NSCaseInsensitiveSearch].location != NSNotFound) {
            NSLog(@"WARNING: Using SOFTWARE Metal renderer");
        } else {
            NSLog(@"SUCCESS: Using HARDWARE Metal renderer");
        }
        
        NSLog(@"Registry ID: %llu", device.registryID);
        MTLSize maxThreads = device.maxThreadsPerThreadgroup;
        NSLog(@"Max Threads Per Group: width=%lu height=%lu depth=%lu", 
              (unsigned long)maxThreads.width, (unsigned long)maxThreads.height, (unsigned long)maxThreads.depth);
        NSLog(@"Supports Shader Barycentric Coordinates: %d", device.supportsShaderBarycentricCoordinates);
        
        // Try to create a command queue (this proves Metal is functional)
        id<MTLCommandQueue> commandQueue = [device newCommandQueue];
        if (commandQueue) {
            NSLog(@"SUCCESS: Created Metal command queue - Metal is WORKING!");
        } else {
            NSLog(@"ERROR: Failed to create command queue");
        }
        
        NSLog(@"=== End Metal Test ===");
    }
    return 0;
}

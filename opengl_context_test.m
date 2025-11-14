#import <Foundation/Foundation.h>
#import <OpenGL/OpenGL.h>
#import <OpenGL/gl.h>

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        NSLog(@"=== OpenGL Context Creation Test ===");
        NSLog(@"Testing if we can create an OpenGL context without kernel panic...");
        
        // Try to create an OpenGL context
        CGLPixelFormatAttribute attributes[] = {
            kCGLPFAAccelerated,
            kCGLPFAOpenGLProfile, (CGLPixelFormatAttribute)kCGLOGLPVersion_Legacy,
            (CGLPixelFormatAttribute)0
        };
        
        CGLPixelFormatObj pixelFormat = NULL;
        GLint numPixelFormats = 0;
        CGLError error;
        
        NSLog(@"Step 1: Creating pixel format...");
        error = CGLChoosePixelFormat(attributes, &pixelFormat, &numPixelFormats);
        if (error != kCGLNoError) {
            NSLog(@"ERROR: Failed to create pixel format: %s", CGLErrorString(error));
            return 1;
        }
        NSLog(@"SUCCESS: Pixel format created (found %d formats)", numPixelFormats);
        
        CGLContextObj context = NULL;
        NSLog(@"Step 2: Creating OpenGL context...");
        error = CGLCreateContext(pixelFormat, NULL, &context);
        if (error != kCGLNoError) {
            NSLog(@"ERROR: Failed to create context: %s", CGLErrorString(error));
            CGLDestroyPixelFormat(pixelFormat);
            return 1;
        }
        NSLog(@"SUCCESS: OpenGL context created!");
        
        NSLog(@"Step 3: Making context current...");
        error = CGLSetCurrentContext(context);
        if (error != kCGLNoError) {
            NSLog(@"ERROR: Failed to set current context: %s", CGLErrorString(error));
            CGLDestroyContext(context);
            CGLDestroyPixelFormat(pixelFormat);
            return 1;
        }
        NSLog(@"SUCCESS: Context is now current!");
        
        // Get OpenGL info
        const GLubyte* renderer = glGetString(GL_RENDERER);
        const GLubyte* version = glGetString(GL_VERSION);
        const GLubyte* vendor = glGetString(GL_VENDOR);
        
        NSLog(@"");
        NSLog(@"OpenGL Information:");
        NSLog(@"  Vendor: %s", vendor ? (const char*)vendor : "UNKNOWN");
        NSLog(@"  Renderer: %s", renderer ? (const char*)renderer : "UNKNOWN");
        NSLog(@"  Version: %s", version ? (const char*)version : "UNKNOWN");
        NSLog(@"");
        
        // Check if we got hardware acceleration
        if (renderer && strstr((const char*)renderer, "VMQemuVGA")) {
            NSLog(@"✓ SUCCESS: Using VMQemuVGA hardware!");
        } else if (renderer && strstr((const char*)renderer, "Software")) {
            NSLog(@"⚠ WARNING: Using software renderer (no hardware acceleration)");
        } else {
            NSLog(@"? UNKNOWN: Renderer type unclear");
        }
        
        NSLog(@"");
        NSLog(@"Step 4: Cleaning up...");
        CGLSetCurrentContext(NULL);
        CGLDestroyContext(context);
        CGLDestroyPixelFormat(pixelFormat);
        NSLog(@"SUCCESS: Context destroyed cleanly");
        
        NSLog(@"");
        NSLog(@"=== TEST PASSED: No kernel panic! ===");
        NSLog(@"Context creation/destruction works without crashing.");
        
        return 0;
    }
}

//
//  Test OpenGL Application
//  Simple spinning triangle to test SharedGL functionality
//

#import <Cocoa/Cocoa.h>
#import <OpenGL/gl.h>

@interface TestGLView : NSOpenGLView {
    float angle;
}
@end

@implementation TestGLView

- (void)prepareOpenGL {
    [super prepareOpenGL];
    
    glClearColor(0.2f, 0.3f, 0.4f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    
    // Setup viewport
    NSRect bounds = [self bounds];
    glViewport(0, 0, bounds.size.width, bounds.size.height);
    
    // Setup projection
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-2.0, 2.0, -2.0, 2.0, -1.0, 1.0);
    
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    angle = 0.0f;
    
    // Start animation timer
    [NSTimer scheduledTimerWithTimeInterval:0.016 // ~60 FPS
                                     target:self
                                   selector:@selector(animate:)
                                   userInfo:nil
                                    repeats:YES];
}

- (void)animate:(NSTimer*)timer {
    angle += 2.0f;
    if (angle >= 360.0f) {
        angle = 0.0f;
    }
    [self setNeedsDisplay:YES];
}

- (void)drawRect:(NSRect)dirtyRect {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    glLoadIdentity();
    
    // Rotate triangle
    glRotatef(angle, 0.0f, 0.0f, 1.0f);
    
    // Draw triangle
    glBegin(GL_TRIANGLES);
        glColor3f(1.0f, 0.0f, 0.0f);  // Red
        glVertex3f(0.0f, 1.0f, 0.0f);
        
        glColor3f(0.0f, 1.0f, 0.0f);  // Green
        glVertex3f(-1.0f, -1.0f, 0.0f);
        
        glColor3f(0.0f, 0.0f, 1.0f);  // Blue
        glVertex3f(1.0f, -1.0f, 0.0f);
    glEnd();
    
    glFlush();
    [[self openGLContext] flushBuffer];
}

@end

@interface AppDelegate : NSObject <NSApplicationDelegate>
@property (strong) NSWindow *window;
@end

@implementation AppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)notification {
    NSLog(@"========================================");
    NSLog(@"  SharedGL Test Application");
    NSLog(@"  Spinning Triangle Demo");
    NSLog(@"========================================");
    NSLog(@"");
    NSLog(@"If you see OpenGL commands in server log,");
    NSLog(@"the forwarding is working correctly!");
    NSLog(@"");
    
    // Create window
    NSRect frame = NSMakeRect(100, 100, 800, 600);
    NSUInteger styleMask = NSWindowStyleMaskTitled | 
                          NSWindowStyleMaskClosable | 
                          NSWindowStyleMaskMiniaturizable;
    
    _window = [[NSWindow alloc] initWithContentRect:frame
                                          styleMask:styleMask
                                            backing:NSBackingStoreBuffered
                                              defer:NO];
    [_window setTitle:@"SharedGL Test - Spinning Triangle"];
    
    // Create OpenGL view
    NSOpenGLPixelFormatAttribute attrs[] = {
        NSOpenGLPFADoubleBuffer,
        NSOpenGLPFAColorSize, 24,
        NSOpenGLPFADepthSize, 24,
        0
    };
    NSOpenGLPixelFormat *pixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:attrs];
    TestGLView *glView = [[TestGLView alloc] initWithFrame:frame pixelFormat:pixelFormat];
    
    [_window setContentView:glView];
    [_window makeKeyAndOrderFront:nil];
    [_window center];
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender {
    return YES;
}

@end

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        NSApplication *app = [NSApplication sharedApplication];
        AppDelegate *delegate = [[AppDelegate alloc] init];
        [app setDelegate:delegate];
        [app run];
    }
    return 0;
}

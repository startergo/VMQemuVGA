// Native macOS OpenGL test - Rotating triangle with color
// Compile: gcc -arch x86_64 -framework Cocoa -framework OpenGL -o test_rotating_triangle test_rotating_triangle.m

#import <Cocoa/Cocoa.h>
#import <OpenGL/gl.h>

@interface OpenGLView : NSOpenGLView {
    float rotation;
}
@end

@implementation OpenGLView

- (void)prepareOpenGL {
    [super prepareOpenGL];
    glClearColor(0.0, 0.0, 0.0, 1.0);
    
    // Start animation timer
    [NSTimer scheduledTimerWithTimeInterval:1.0/60.0 
                                     target:self 
                                   selector:@selector(animate:) 
                                   userInfo:nil 
                                    repeats:YES];
    rotation = 0.0;
}

- (void)animate:(NSTimer*)timer {
    rotation += 2.0;
    if (rotation > 360.0) rotation -= 360.0;
    [self setNeedsDisplay:YES];
}

- (void)drawRect:(NSRect)rect {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glRotatef(rotation, 0.0, 0.0, 1.0);
    
    // Draw 5,000 rotating triangles - balance between GPU load and overhead
    for (int i = 0; i < 5000; i++) {
        float offsetX = ((i % 71) - 35) * 0.028;
        float offsetY = ((i / 71) - 35) * 0.028;
        float scale = 0.012;
        
        glPushMatrix();
        glTranslatef(offsetX, offsetY, 0.0);
        glRotatef(rotation + i * 1.44, 0.0, 0.0, 1.0);
        
        glBegin(GL_TRIANGLES);
            glColor3f(1.0, 0.0, 0.0);  // Red
            glVertex3f(0.0, scale, 0.0);
            
            glColor3f(0.0, 1.0, 0.0);  // Green
            glVertex3f(-scale, -scale, 0.0);
            
            glColor3f(0.0, 0.0, 1.0);  // Blue
            glVertex3f(scale, -scale, 0.0);
        glEnd();
        
        glPopMatrix();
    }
    
    glFlush();
    [[self openGLContext] flushBuffer];
}

- (void)reshape {
    [super reshape];
    NSRect bounds = [self bounds];
    glViewport(0, 0, bounds.size.width, bounds.size.height);
    
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
}

@end

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        [NSApplication sharedApplication];
        
        NSRect frame = NSMakeRect(0, 0, 800, 600);
        NSWindow *window = [[NSWindow alloc] 
            initWithContentRect:frame
            styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable)
            backing:NSBackingStoreBuffered
            defer:NO];
        
        [window setTitle:@"OpenGL → Metal Test"];
        [window center];
        
        NSOpenGLPixelFormatAttribute attrs[] = {
            NSOpenGLPFADoubleBuffer,
            NSOpenGLPFADepthSize, 24,
            NSOpenGLPFAOpenGLProfile, NSOpenGLProfileVersionLegacy,
            0
        };
        
        NSOpenGLPixelFormat *pixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:attrs];
        OpenGLView *glView = [[OpenGLView alloc] initWithFrame:frame pixelFormat:pixelFormat];
        
        [window setContentView:glView];
        [window makeKeyAndOrderFront:nil];
        
        // Run for 5 minutes then quit
        [NSTimer scheduledTimerWithTimeInterval:300.0
                                         target:[NSApplication sharedApplication]
                                       selector:@selector(terminate:)
                                       userInfo:nil
                                        repeats:NO];
        
        [NSApp run];
    }
    return 0;
}

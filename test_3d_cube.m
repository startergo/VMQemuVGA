/*
 * test_3d_cube.m
 * Test 3D spinning cube with OpenGL software rendering
 * Should work on Snow Leopard with QXL device (no hardware acceleration needed)
 *
 * Compile on Snow Leopard:
 *   gcc -framework Cocoa -framework OpenGL test_3d_cube.m -o test_3d_cube
 *
 * Run:
 *   ./test_3d_cube
 */

#import <Cocoa/Cocoa.h>
#import <OpenGL/gl.h>
#import <OpenGL/glu.h>
#import <OpenGL/OpenGL.h>

@interface CubeView : NSOpenGLView
{
    float rotation;
    NSTimer* animationTimer;
}
- (void)drawRect:(NSRect)bounds;
- (void)animate:(NSTimer*)timer;
@end

@implementation CubeView

- (id)initWithFrame:(NSRect)frame
{
    NSOpenGLPixelFormatAttribute attrs[] = {
        NSOpenGLPFADoubleBuffer,
        NSOpenGLPFADepthSize, 24,
        NSOpenGLPFAColorSize, 24,
        NSOpenGLPFAAlphaSize, 8,
        0
    };
    
    NSOpenGLPixelFormat* pixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:attrs];
    
    self = [super initWithFrame:frame pixelFormat:pixelFormat];
    [pixelFormat release];
    
    if (self) {
        rotation = 0.0f;
        
        // Print OpenGL renderer info
        [[self openGLContext] makeCurrentContext];
        const GLubyte* renderer = glGetString(GL_RENDERER);
        const GLubyte* version = glGetString(GL_VERSION);
        const GLubyte* vendor = glGetString(GL_VENDOR);
        
        NSLog(@"OpenGL Renderer: %s", renderer);
        NSLog(@"OpenGL Version: %s", version);
        NSLog(@"OpenGL Vendor: %s", vendor);
        
        // Check if we're using software or hardware rendering
        if (renderer) {
            if (strstr((const char*)renderer, "Software") != NULL) {
                NSLog(@"Using SOFTWARE rendering (expected for QXL without hardware acceleration)");
            } else {
                NSLog(@"Using HARDWARE rendering");
            }
        }
        
        // Setup OpenGL state
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glClearColor(0.2f, 0.2f, 0.3f, 1.0f);
        glClearDepth(1.0f);
        
        // Enable smooth shading
        glShadeModel(GL_SMOOTH);
        
        // Setup lighting
        glEnable(GL_LIGHTING);
        glEnable(GL_LIGHT0);
        
        GLfloat lightPosition[] = { 1.0f, 1.0f, 1.0f, 0.0f };
        GLfloat lightAmbient[] = { 0.2f, 0.2f, 0.2f, 1.0f };
        GLfloat lightDiffuse[] = { 1.0f, 1.0f, 1.0f, 1.0f };
        
        glLightfv(GL_LIGHT0, GL_POSITION, lightPosition);
        glLightfv(GL_LIGHT0, GL_AMBIENT, lightAmbient);
        glLightfv(GL_LIGHT0, GL_DIFFUSE, lightDiffuse);
        
        // Enable color material
        glEnable(GL_COLOR_MATERIAL);
        glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
        
        // Start animation timer (30 FPS)
        animationTimer = [[NSTimer scheduledTimerWithTimeInterval:1.0/30.0
                                                           target:self
                                                         selector:@selector(animate:)
                                                         userInfo:nil
                                                          repeats:YES] retain];
    }
    
    return self;
}

- (void)dealloc
{
    [animationTimer invalidate];
    [animationTimer release];
    [super dealloc];
}

- (void)drawRect:(NSRect)bounds
{
    [[self openGLContext] makeCurrentContext];
    
    // Clear the screen
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // Setup projection matrix
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    
    GLfloat aspectRatio = bounds.size.width / bounds.size.height;
    gluPerspective(45.0f, aspectRatio, 0.1f, 100.0f);
    
    // Setup modelview matrix
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    // Position camera
    glTranslatef(0.0f, 0.0f, -5.0f);
    
    // Rotate cube
    glRotatef(rotation, 0.0f, 1.0f, 0.0f);  // Y-axis rotation
    glRotatef(rotation * 0.5f, 1.0f, 0.0f, 0.0f);  // X-axis rotation
    
    // Draw a colored cube
    glBegin(GL_QUADS);
    
    // Front face (red)
    glColor3f(1.0f, 0.0f, 0.0f);
    glNormal3f(0.0f, 0.0f, 1.0f);
    glVertex3f(-1.0f, -1.0f,  1.0f);
    glVertex3f( 1.0f, -1.0f,  1.0f);
    glVertex3f( 1.0f,  1.0f,  1.0f);
    glVertex3f(-1.0f,  1.0f,  1.0f);
    
    // Back face (green)
    glColor3f(0.0f, 1.0f, 0.0f);
    glNormal3f(0.0f, 0.0f, -1.0f);
    glVertex3f(-1.0f, -1.0f, -1.0f);
    glVertex3f(-1.0f,  1.0f, -1.0f);
    glVertex3f( 1.0f,  1.0f, -1.0f);
    glVertex3f( 1.0f, -1.0f, -1.0f);
    
    // Top face (blue)
    glColor3f(0.0f, 0.0f, 1.0f);
    glNormal3f(0.0f, 1.0f, 0.0f);
    glVertex3f(-1.0f,  1.0f, -1.0f);
    glVertex3f(-1.0f,  1.0f,  1.0f);
    glVertex3f( 1.0f,  1.0f,  1.0f);
    glVertex3f( 1.0f,  1.0f, -1.0f);
    
    // Bottom face (yellow)
    glColor3f(1.0f, 1.0f, 0.0f);
    glNormal3f(0.0f, -1.0f, 0.0f);
    glVertex3f(-1.0f, -1.0f, -1.0f);
    glVertex3f( 1.0f, -1.0f, -1.0f);
    glVertex3f( 1.0f, -1.0f,  1.0f);
    glVertex3f(-1.0f, -1.0f,  1.0f);
    
    // Right face (magenta)
    glColor3f(1.0f, 0.0f, 1.0f);
    glNormal3f(1.0f, 0.0f, 0.0f);
    glVertex3f( 1.0f, -1.0f, -1.0f);
    glVertex3f( 1.0f,  1.0f, -1.0f);
    glVertex3f( 1.0f,  1.0f,  1.0f);
    glVertex3f( 1.0f, -1.0f,  1.0f);
    
    // Left face (cyan)
    glColor3f(0.0f, 1.0f, 1.0f);
    glNormal3f(-1.0f, 0.0f, 0.0f);
    glVertex3f(-1.0f, -1.0f, -1.0f);
    glVertex3f(-1.0f, -1.0f,  1.0f);
    glVertex3f(-1.0f,  1.0f,  1.0f);
    glVertex3f(-1.0f,  1.0f, -1.0f);
    
    glEnd();
    
    // Swap buffers
    [[self openGLContext] flushBuffer];
}

- (void)animate:(NSTimer*)timer
{
    rotation += 2.0f;
    if (rotation >= 360.0f) {
        rotation -= 360.0f;
    }
    
    [self setNeedsDisplay:YES];
}

@end

int main(int argc, const char* argv[])
{
    NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
    
    [NSApplication sharedApplication];
    
    // Create window
    NSRect frame = NSMakeRect(0, 0, 640, 480);
    NSWindow* window = [[NSWindow alloc] initWithContentRect:frame
                                                    styleMask:NSTitledWindowMask | NSClosableWindowMask | NSMiniaturizableWindowMask
                                                      backing:NSBackingStoreBuffered
                                                        defer:NO];
    
    [window setTitle:@"3D Cube Test - OpenGL Software Rendering"];
    [window center];
    
    // Create OpenGL view
    CubeView* view = [[CubeView alloc] initWithFrame:frame];
    [window setContentView:view];
    [view release];
    
    [window makeKeyAndOrderFront:nil];
    
    NSLog(@"=== 3D Cube Test ===");
    NSLog(@"Window created: 640x480");
    NSLog(@"Using QXL device with OpenGL software rendering");
    NSLog(@"You should see a spinning colored cube");
    NSLog(@"Press Cmd+Q to quit");
    
    [NSApp run];
    
    [window release];
    [pool release];
    
    return 0;
}

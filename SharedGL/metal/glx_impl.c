//
//  GLX Implementation for Metal Translator
//  Provides X11/GLX integration layer for OpenGL applications
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>

// X11 types
typedef struct _XDisplay Display;
typedef unsigned long XID;
typedef XID Window;
typedef XID Drawable;
typedef XID Pixmap;
typedef XID Colormap;
typedef struct {
    void *visual;
    int visualid;
    int screen;
    int depth;
    int c_class;
    unsigned long red_mask;
    unsigned long green_mask;
    unsigned long blue_mask;
    int colormap_size;
    int bits_per_rgb;
} XVisualInfo;

// GLX types
typedef XID GLXDrawable;
typedef XID GLXPixmap;
typedef XID GLXWindow;
typedef XID GLXPbuffer;
typedef struct __GLXcontextRec *GLXContext;
typedef struct __GLXFBConfigRec *GLXFBConfig;
typedef void (*__GLXextFuncPtr)(void);

// GLX FBConfig structure
typedef struct __GLXFBConfigRec {
    int visualType;
    int transparentType;
    int transparentRedValue;
    int transparentGreenValue;
    int transparentBlueValue;
    int transparentAlphaValue;
    int transparentIndexValue;
    int bufferSize;
    int level;
    int renderType;
    int doubleBufferMode;
    int stereoMode;
    int redSize;
    int greenSize;
    int blueSize;
    int alphaSize;
    int depthSize;
    int stencilSize;
    int accumRedSize;
    int accumGreenSize;
    int accumBlueSize;
    int accumAlphaSize;
    int numAuxBuffers;
    int sampleBuffers;
    int samples;
    int maxPbufferWidth;
    int maxPbufferHeight;
    int maxPbufferPixels;
    int drawableType;
    int visualID;
    int xRenderable;
    int fbconfigID;
} GLXFBConfigRec;

// Boolean
typedef int Bool;
#define True 1
#define False 0

// GLX attribute names
#define GLX_USE_GL                1
#define GLX_BUFFER_SIZE           2
#define GLX_LEVEL                 3
#define GLX_RGBA                  4
#define GLX_DOUBLEBUFFER          5
#define GLX_STEREO                6
#define GLX_AUX_BUFFERS           7
#define GLX_RED_SIZE              8
#define GLX_GREEN_SIZE            9
#define GLX_BLUE_SIZE             10
#define GLX_ALPHA_SIZE            11
#define GLX_DEPTH_SIZE            12
#define GLX_STENCIL_SIZE          13
#define GLX_ACCUM_RED_SIZE        14
#define GLX_ACCUM_GREEN_SIZE      15
#define GLX_ACCUM_BLUE_SIZE       16
#define GLX_ACCUM_ALPHA_SIZE      17

// GLX context structure
typedef struct __GLXcontextRec {
    Display *display;
    GLXDrawable drawable;
    int refcount;
    Bool direct;
} GLXContextRec;

// Global state
static GLXContext g_currentContext = NULL;
static GLXDrawable g_currentDrawable = 0;
static Display *g_currentDisplay = NULL;

// X11 library handle
static void *g_x11_handle = NULL;
static void *g_system_gl_handle = NULL;

// X11 function pointers (we need some real X11 functions)
typedef int (*XGetWindowAttributes_t)(Display*, Window, void*);
typedef int (*XSync_t)(Display*, Bool);
static XGetWindowAttributes_t real_XGetWindowAttributes = NULL;
static XSync_t real_XSync = NULL;

//
// GLX API Implementation
//

Bool glXQueryVersion(Display *dpy, int *major, int *minor) {
    fprintf(stderr, "[GLX] glXQueryVersion(dpy=%p)\n", dpy);
    if (major) *major = 1;
    if (minor) *minor = 4;
    return True;
}

Bool glXQueryExtension(Display *dpy, int *errorBase, int *eventBase) {
    fprintf(stderr, "[GLX] glXQueryExtension(dpy=%p)\n", dpy);
    if (errorBase) *errorBase = 0;
    if (eventBase) *eventBase = 0;
    return True;
}

const char* glXQueryExtensionsString(Display *dpy, int screen) {
    fprintf(stderr, "[GLX] glXQueryExtensionsString(dpy=%p, screen=%d)\n", dpy, screen);
    return "GLX_ARB_get_proc_address GLX_ARB_multisample GLX_EXT_visual_info GLX_EXT_visual_rating GLX_EXT_swap_control GLX_MESA_swap_control";
}

const char* glXGetClientString(Display *dpy, int name) {
    fprintf(stderr, "[GLX] glXGetClientString(dpy=%p, name=%d)\n", dpy, name);
    return "SharedGL Metal Translator 1.0";
}

const char* glXQueryServerString(Display *dpy, int screen, int name) {
    fprintf(stderr, "[GLX] glXQueryServerString(dpy=%p, screen=%d, name=%d)\n", dpy, screen, name);
    return "SharedGL Metal Translator 1.0";
}

XVisualInfo* glXChooseVisual(Display *dpy, int screen, int *attribList) {
    fprintf(stderr, "[GLX] glXChooseVisual(dpy=%p, screen=%d, attribList=%p)\n", dpy, screen, attribList);
    
    // Load real X11 library to get visual info
    if (!g_x11_handle) {
        g_x11_handle = dlopen("/opt/X11/lib/libX11.dylib", RTLD_LAZY | RTLD_LOCAL);
        if (!g_x11_handle) {
            fprintf(stderr, "[GLX] Failed to load libX11: %s\n", dlerror());
            return NULL;
        }
    }
    
    // Get XDefaultVisual function
    typedef void* (*XDefaultVisual_t)(Display*, int);
    XDefaultVisual_t XDefaultVisual = dlsym(g_x11_handle, "XDefaultVisual");
    
    // Get XVisualIDFromVisual function
    typedef unsigned long (*XVisualIDFromVisual_t)(void*);
    XVisualIDFromVisual_t XVisualIDFromVisual = dlsym(g_x11_handle, "XVisualIDFromVisual");
    
    if (!XDefaultVisual || !XVisualIDFromVisual) {
        fprintf(stderr, "[GLX] Failed to get X11 visual functions\n");
        return NULL;
    }
    
    // Get default visual
    void *visual = XDefaultVisual(dpy, screen);
    if (!visual) {
        fprintf(stderr, "[GLX] Failed to get default visual\n");
        return NULL;
    }
    
    // Allocate and fill XVisualInfo
    XVisualInfo *vinfo = (XVisualInfo*)calloc(1, sizeof(XVisualInfo));
    if (!vinfo) return NULL;
    
    vinfo->visual = visual;
    vinfo->visualid = XVisualIDFromVisual(visual);
    vinfo->screen = screen;
    vinfo->depth = 24;
    vinfo->c_class = 4; // TrueColor
    vinfo->red_mask = 0xFF0000;
    vinfo->green_mask = 0x00FF00;
    vinfo->blue_mask = 0x0000FF;
    vinfo->colormap_size = 256;
    vinfo->bits_per_rgb = 8;
    
    fprintf(stderr, "[GLX] Created XVisualInfo: visualid=0x%lx, depth=%d\n", 
            vinfo->visualid, vinfo->depth);
    
    return vinfo;
}

int glXGetConfig(Display *dpy, XVisualInfo *vis, int attrib, int *value) {
    fprintf(stderr, "[GLX] glXGetConfig(dpy=%p, vis=%p, attrib=0x%x)\n", dpy, vis, attrib);
    
    if (!value) return 1; // BadValue
    
    // Return sensible defaults for common attributes
    switch (attrib) {
        case GLX_USE_GL:          *value = 1; break;
        case GLX_BUFFER_SIZE:     *value = 32; break;
        case GLX_LEVEL:           *value = 0; break;
        case GLX_RGBA:            *value = 1; break;
        case GLX_DOUBLEBUFFER:    *value = 1; break;
        case GLX_STEREO:          *value = 0; break;
        case GLX_AUX_BUFFERS:     *value = 0; break;
        case GLX_RED_SIZE:        *value = 8; break;
        case GLX_GREEN_SIZE:      *value = 8; break;
        case GLX_BLUE_SIZE:       *value = 8; break;
        case GLX_ALPHA_SIZE:      *value = 8; break;
        case GLX_DEPTH_SIZE:      *value = 24; break;
        case GLX_STENCIL_SIZE:    *value = 8; break;
        case GLX_ACCUM_RED_SIZE:  *value = 0; break;
        case GLX_ACCUM_GREEN_SIZE:*value = 0; break;
        case GLX_ACCUM_BLUE_SIZE: *value = 0; break;
        case GLX_ACCUM_ALPHA_SIZE:*value = 0; break;
        default:                  *value = 0; break;
    }
    
    return 0; // Success
}

GLXContext glXCreateContext(Display *dpy, XVisualInfo *vis, GLXContext shareList, Bool direct) {
    fprintf(stderr, "[GLX] glXCreateContext(dpy=%p, vis=%p, shareList=%p, direct=%d)\n", 
            dpy, vis, shareList, direct);
    
    // Allocate context structure
    GLXContextRec *ctx = (GLXContextRec*)calloc(1, sizeof(GLXContextRec));
    if (!ctx) {
        fprintf(stderr, "[GLX] Failed to allocate context\n");
        return NULL;
    }
    
    ctx->display = dpy;
    ctx->drawable = 0;
    ctx->refcount = 1;
    ctx->direct = direct;
    
    fprintf(stderr, "[GLX] Created context %p\n", ctx);
    return (GLXContext)ctx;
}

void glXDestroyContext(Display *dpy, GLXContext ctx) {
    fprintf(stderr, "[GLX] glXDestroyContext(dpy=%p, ctx=%p)\n", dpy, ctx);
    
    if (!ctx) return;
    
    GLXContextRec *context = (GLXContextRec*)ctx;
    
    // If this is the current context, clear it
    if (g_currentContext == ctx) {
        g_currentContext = NULL;
        g_currentDrawable = 0;
        g_currentDisplay = NULL;
    }
    
    free(context);
}

Bool glXMakeCurrent(Display *dpy, GLXDrawable drawable, GLXContext ctx) {
    fprintf(stderr, "[GLX] glXMakeCurrent(dpy=%p, drawable=%lu, ctx=%p)\n", 
            dpy, (unsigned long)drawable, ctx);
    
    // Update global current context
    g_currentDisplay = dpy;
    g_currentDrawable = drawable;
    g_currentContext = ctx;
    
    if (ctx) {
        GLXContextRec *context = (GLXContextRec*)ctx;
        context->drawable = drawable;
    }
    
    fprintf(stderr, "[GLX] Context made current\n");
    return True;
}

GLXContext glXGetCurrentContext(void) {
    return g_currentContext;
}

GLXDrawable glXGetCurrentDrawable(void) {
    return g_currentDrawable;
}

Display* glXGetCurrentDisplay(void) {
    return g_currentDisplay;
}

Bool glXIsDirect(Display *dpy, GLXContext ctx) {
    fprintf(stderr, "[GLX] glXIsDirect(dpy=%p, ctx=%p)\n", dpy, ctx);
    if (!ctx) return False;
    GLXContextRec *context = (GLXContextRec*)ctx;
    return context->direct;
}

void glXSwapBuffers(Display *dpy, GLXDrawable drawable) {
    fprintf(stderr, "[GLX] glXSwapBuffers(dpy=%p, drawable=%lu)\n", dpy, (unsigned long)drawable);
    
    // Forward to our Metal translator's SwapBuffers implementation
    // This is declared in gl_to_metal_client.c
    void metal_swap_buffers(void *dpy, unsigned long drawable);
    metal_swap_buffers(dpy, drawable);
    
    // Sync with X11 to ensure window updates
    if (!g_x11_handle) {
        g_x11_handle = dlopen("/opt/X11/lib/libX11.dylib", RTLD_LAZY | RTLD_LOCAL);
    }
    if (g_x11_handle && !real_XSync) {
        real_XSync = dlsym(g_x11_handle, "XSync");
    }
    if (real_XSync) {
        real_XSync(dpy, False);
    }
}

void glXWaitGL(void) {
    fprintf(stderr, "[GLX] glXWaitGL()\n");
    // Flush OpenGL commands
    void glFlush(void);
    glFlush();
}

void glXWaitX(void) {
    fprintf(stderr, "[GLX] glXWaitX()\n");
    // Sync with X server
    if (g_currentDisplay && real_XSync) {
        real_XSync(g_currentDisplay, False);
    }
}

__GLXextFuncPtr glXGetProcAddressARB(const unsigned char *procName) {
    if (!procName) {
        fprintf(stderr, "[GLX] glXGetProcAddressARB(NULL)\n");
        return NULL;
    }
    
    fprintf(stderr, "[GLX] glXGetProcAddressARB(\"%s\")\n", procName);
    
    // Return NULL to force applications to use standard OpenGL functions
    // Our symbol exports will handle the actual function calls
    return NULL;
}

__GLXextFuncPtr glXGetProcAddress(const unsigned char *procName) {
    return glXGetProcAddressARB(procName);
}

// GLX 1.3+ functions (FBConfig-based)
GLXFBConfig* glXChooseFBConfig(Display *dpy, int screen, const int *attrib_list, int *nelements) {
    fprintf(stderr, "[GLX] glXChooseFBConfig(dpy=%p, screen=%d)\n", dpy, screen);
    
    // Log requested attributes
    if (attrib_list) {
        fprintf(stderr, "[GLX]   Requested attributes:");
        for (int i = 0; attrib_list[i] != 0; i += 2) {
            fprintf(stderr, " 0x%x=%d", attrib_list[i], attrib_list[i+1]);
        }
        fprintf(stderr, "\n");
    }
    
    // Get a real visual from X11 to extract its ID
    XVisualInfo *vinfo = glXChooseVisual(dpy, screen, NULL);
    int visualID = 0;
    if (vinfo) {
        visualID = vinfo->visualid;
        fprintf(stderr, "[GLX]   Got visual ID: 0x%x from X11\n", visualID);
        free(vinfo); // Free the visual info, we only needed the ID
    } else {
        fprintf(stderr, "[GLX]   WARNING: Could not get visual from X11, using dummy ID\n");
        visualID = 0x21; // Fallback dummy visual ID
    }
    
    // Allocate FBConfig array dynamically so it can be freed by the application
    GLXFBConfigRec *config = (GLXFBConfigRec*)calloc(1, sizeof(GLXFBConfigRec));
    GLXFBConfig *config_list = (GLXFBConfig*)malloc(sizeof(GLXFBConfig));
    
    if (!config || !config_list) {
        free(config);
        free(config_list);
        if (nelements) *nelements = 0;
        return NULL;
    }
    
    // Initialize config with default values
    config->visualType = 0x8004; // GLX_TRUE_COLOR
    config->renderType = 0x00000001; // GLX_RGBA_BIT
    config->doubleBufferMode = 1;
    config->redSize = 8;
    config->greenSize = 8;
    config->blueSize = 8;
    config->alphaSize = 8;
    config->depthSize = 24;
    config->stencilSize = 8;
    config->drawableType = 0x00000001; // GLX_WINDOW_BIT
    config->xRenderable = 1;
    config->visualID = visualID; // Store the real visual ID from X11
    
    config_list[0] = config;
    
    if (nelements) *nelements = 1;
    
    fprintf(stderr, "[GLX]   Returning 1 FBConfig: %p (visualID=0x%x)\n", config, visualID);
    return config_list;
}

GLXFBConfig* glXGetFBConfigs(Display *dpy, int screen, int *nelements) {
    fprintf(stderr, "[GLX] glXGetFBConfigs(dpy=%p, screen=%d)\n", dpy, screen);
    if (nelements) *nelements = 0;
    return NULL;
}

XVisualInfo* glXGetVisualFromFBConfig(Display *dpy, GLXFBConfig config) {
    fprintf(stderr, "[GLX] glXGetVisualFromFBConfig(dpy=%p, config=%p)\n", dpy, config);
    
    if (!dpy || !config) {
        fprintf(stderr, "[GLX] ERROR: Invalid display or config\n");
        return NULL;
    }
    
    // Use glXChooseVisual to get a real visual from X11
    XVisualInfo *vinfo = glXChooseVisual(dpy, 0, NULL);
    if (vinfo) {
        fprintf(stderr, "[GLX] Returning XVisualInfo: visualid=0x%x, depth=%d\n", 
                (unsigned int)vinfo->visualid, vinfo->depth);
    } else {
        fprintf(stderr, "[GLX] ERROR: Failed to get XVisualInfo\n");
    }
    return vinfo;
}

int glXGetFBConfigAttrib(Display *dpy, GLXFBConfig config, int attribute, int *value) {
    fprintf(stderr, "[GLX] glXGetFBConfigAttrib(dpy=%p, config=%p, attr=0x%x)\n", dpy, config, attribute);
    
    if (!value) return 1; // BadValue
    
    // Return sensible defaults matching our glXGetConfig implementation
    switch (attribute) {
        // Basic framebuffer attributes
        case GLX_BUFFER_SIZE:     // 0x2 (old) or 0x8012 (new)
        case 0x8012:
            *value = 32; break;
        case GLX_LEVEL:           // 0x3
        case 0x8014:
            *value = 0; break;
        case GLX_DOUBLEBUFFER:    // 0x5
        case 0x8015:
            *value = 1; break;
        case GLX_STEREO:          // 0x6
        case 0x8016:
            *value = 0; break;
        case GLX_AUX_BUFFERS:     // 0x7
        case 0x8017:
            *value = 0; break;
            
        // Color channel sizes
        case GLX_RED_SIZE:        // 0x8
        case 0x8018:
            *value = 8; break;
        case GLX_GREEN_SIZE:      // 0x9
        case 0x8019:
            *value = 8; break;
        case GLX_BLUE_SIZE:       // 0xa
        case 0x801A:
            *value = 8; break;
        case GLX_ALPHA_SIZE:      // 0xb
        case 0x801B:
            *value = 8; break;
            
        // Depth and stencil
        case GLX_DEPTH_SIZE:      // 0xc
        case 0x801C:
            *value = 24; break;
        case GLX_STENCIL_SIZE:    // 0xd
        case 0x801D:
            *value = 8; break;
            
        // Accumulation buffer (not supported)
        case GLX_ACCUM_RED_SIZE:  // 0xe
            *value = 0; break;
        case GLX_ACCUM_GREEN_SIZE: // 0xf
            *value = 0; break;
        case GLX_ACCUM_BLUE_SIZE: // 0x10
            *value = 0; break;
        case GLX_ACCUM_ALPHA_SIZE: // 0x11
            *value = 0; break;
            
        // GLX 1.3+ attributes
        case 0x8010: // GLX_RENDER_TYPE
            *value = 0x00000001; break; // GLX_RGBA_BIT
        case 0x8011: // GLX_DRAWABLE_TYPE  
            *value = 0x00000001; break; // GLX_WINDOW_BIT
        case 0x8013: // GLX_X_VISUAL_TYPE
            *value = 0x8004; break; // GLX_TRUE_COLOR
        case 0x800b: // GLX_VISUAL_ID
            // Return the visual ID stored in the config
            if (config) {
                GLXFBConfigRec *cfg = (GLXFBConfigRec*)config;
                *value = cfg->visualID;
            } else {
                *value = 0;
            }
            break;
        case 0x22:   // GLX_X_RENDERABLE
            *value = 1; break;
            
        // Multisampling (glmark2 checks this!)
        case 0x186a0: // GLX_SAMPLE_BUFFERS_ARB
            *value = 0; break; // No multisampling
        case 0x186a1: // GLX_SAMPLES_ARB
            *value = 0; break; // No samples
            
        // Config caveat
        case 0x20:   // GLX_CONFIG_CAVEAT
            *value = 0x8000; break; // GLX_NONE
            
        default:
            fprintf(stderr, "[GLX] Unknown attribute 0x%x, returning 0\n", attribute);
            *value = 0; break;
    }
    
    return 0; // Success
}

GLXContext glXCreateNewContext(Display *dpy, GLXFBConfig config, int render_type, 
                                GLXContext share_list, Bool direct) {
    fprintf(stderr, "[GLX] glXCreateNewContext(dpy=%p, config=%p)\n", dpy, config);
    
    // Create a context similar to glXCreateContext
    return glXCreateContext(dpy, NULL, share_list, direct);
}

Bool glXMakeContextCurrent(Display *dpy, GLXDrawable draw, GLXDrawable read, GLXContext ctx) {
    fprintf(stderr, "[GLX] glXMakeContextCurrent(dpy=%p, draw=%lu, read=%lu, ctx=%p)\n",
            dpy, (unsigned long)draw, (unsigned long)read, ctx);
    
    // For simplicity, use the draw drawable
    return glXMakeCurrent(dpy, draw, ctx);
}

GLXDrawable glXGetCurrentReadDrawable(void) {
    return g_currentDrawable;
}

int glXQueryContext(Display *dpy, GLXContext ctx, int attribute, int *value) {
    fprintf(stderr, "[GLX] glXQueryContext(dpy=%p, ctx=%p, attr=%d)\n", dpy, ctx, attribute);
    if (value) *value = 0;
    return 0;
}

void glXSelectEvent(Display *dpy, GLXDrawable draw, unsigned long event_mask) {
    fprintf(stderr, "[GLX] glXSelectEvent(dpy=%p, draw=%lu, mask=0x%lx)\n", 
            dpy, (unsigned long)draw, event_mask);
}

void glXGetSelectedEvent(Display *dpy, GLXDrawable draw, unsigned long *event_mask) {
    fprintf(stderr, "[GLX] glXGetSelectedEvent(dpy=%p, draw=%lu)\n", dpy, (unsigned long)draw);
    if (event_mask) *event_mask = 0;
}

// Window and pixmap creation
GLXWindow glXCreateWindow(Display *dpy, GLXFBConfig config, Window win, const int *attrib_list) {
    fprintf(stderr, "[GLX] glXCreateWindow(dpy=%p, config=%p, win=%lu)\n", dpy, config, (unsigned long)win);
    // Return the Window as GLXWindow (they're the same type)
    return (GLXWindow)win;
}

void glXDestroyWindow(Display *dpy, GLXWindow win) {
    fprintf(stderr, "[GLX] glXDestroyWindow(dpy=%p, win=%lu)\n", dpy, (unsigned long)win);
}

GLXPixmap glXCreatePixmap(Display *dpy, GLXFBConfig config, Pixmap pixmap, const int *attrib_list) {
    fprintf(stderr, "[GLX] glXCreatePixmap(dpy=%p, config=%p, pixmap=%lu)\n", dpy, config, (unsigned long)pixmap);
    return (GLXPixmap)pixmap;
}

void glXDestroyPixmap(Display *dpy, GLXPixmap pixmap) {
    fprintf(stderr, "[GLX] glXDestroyPixmap(dpy=%p, pixmap=%lu)\n", dpy, (unsigned long)pixmap);
}

GLXPbuffer glXCreatePbuffer(Display *dpy, GLXFBConfig config, const int *attrib_list) {
    fprintf(stderr, "[GLX] glXCreatePbuffer(dpy=%p, config=%p)\n", dpy, config);
    // Return a dummy pbuffer ID
    return (GLXPbuffer)0x10000;
}

void glXDestroyPbuffer(Display *dpy, GLXPbuffer pbuf) {
    fprintf(stderr, "[GLX] glXDestroyPbuffer(dpy=%p, pbuf=%lu)\n", dpy, (unsigned long)pbuf);
}

void glXQueryDrawable(Display *dpy, GLXDrawable draw, int attribute, unsigned int *value) {
    fprintf(stderr, "[GLX] glXQueryDrawable(dpy=%p, draw=%lu, attr=%d)\n", 
            dpy, (unsigned long)draw, attribute);
    if (value) *value = 0;
}

// Swap control (GLX_EXT_swap_control / GLX_MESA_swap_control)
static int g_swapInterval = 1; // Default to vsync on

void glXSwapIntervalEXT(Display *dpy, GLXDrawable drawable, int interval) {
    fprintf(stderr, "[GLX] glXSwapIntervalEXT(dpy=%p, drawable=%lu, interval=%d)\n",
            dpy, (unsigned long)drawable, interval);
    g_swapInterval = interval;
}

int glXSwapIntervalMESA(int interval) {
    fprintf(stderr, "[GLX] glXSwapIntervalMESA(interval=%d)\n", interval);
    g_swapInterval = interval;
    return 0; // Success
}

int glXGetSwapIntervalMESA(void) {
    fprintf(stderr, "[GLX] glXGetSwapIntervalMESA() = %d\n", g_swapInterval);
    return g_swapInterval;
}

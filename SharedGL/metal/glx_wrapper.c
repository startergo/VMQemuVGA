//
//  GLX Wrapper Library
//  Intercepts GLX calls from Mesa libGL and translates to Metal
//  This wrapper gets loaded instead of system libGL.dylib
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <OpenGL/gl.h>
#include <OpenGL/OpenGL.h>

// GLX types
typedef struct __GLXcontextRec *GLXContext;
typedef unsigned long XID;
typedef XID GLXDrawable;
typedef struct _XDisplay Display;
typedef struct __GLXFBConfigRec *GLXFBConfig;
typedef XID GLXFBConfigID;
typedef XID GLXPixmap;
typedef XID GLXWindow;
typedef XID GLXPbuffer;
typedef void (*__GLXextFuncPtr)(void);
typedef XID XVisualInfo;

// Metal connection - DISABLED
// glx_wrapper should NOT connect to Metal server
// Only gl_to_metal_client.c connects to avoid multiple client conflicts
// #include <sys/socket.h>
// #include <netinet/in.h>
// #include <arpa/inet.h>
// #include <unistd.h>

// Original Mesa libGL handle and Metal translator
// Make these global so gl_stubs_generated.c can access them
void *original_libgl = NULL;
static void *metal_translator = NULL;

// Function pointer types for original GLX functions
typedef XVisualInfo* (*glXChooseVisual_t)(Display*, int, int*);
typedef GLXContext (*glXCreateContext_t)(Display*, XVisualInfo*, GLXContext, int);
typedef void (*glXDestroyContext_t)(Display*, GLXContext);
typedef int (*glXMakeCurrent_t)(Display*, GLXDrawable, GLXContext);
typedef void (*glXSwapBuffers_t)(Display*, GLXDrawable);
typedef const char* (*glXQueryExtensionsString_t)(Display*, int);
typedef __GLXextFuncPtr (*glXGetProcAddressARB_t)(const unsigned char*);
typedef void (*glXQueryDrawable_t)(Display*, GLXDrawable, int, unsigned int*);

// Original function pointers
static glXChooseVisual_t orig_glXChooseVisual = NULL;
static glXCreateContext_t orig_glXCreateContext = NULL;
static glXDestroyContext_t orig_glXDestroyContext = NULL;
static glXMakeCurrent_t orig_glXMakeCurrent = NULL;
static glXSwapBuffers_t orig_glXSwapBuffers = NULL;
static glXQueryExtensionsString_t orig_glXQueryExtensionsString = NULL;
static glXGetProcAddressARB_t orig_glXGetProcAddressARB = NULL;
static glXQueryDrawable_t orig_glXQueryDrawable = NULL;

// Load original Mesa libGL (non-static so gl_stubs_generated.c can call it)
void load_original_libgl() {
    if (original_libgl) return;
    
    // DON'T load Mesa with dlopen - symbols are already reexported via linker!
    // Loading with RTLD_GLOBAL would bypass fishhook in libGLMetal.dylib
    // Instead, just use dlsym(RTLD_DEFAULT, ...) to find reexported symbols
    printf("[GLX→Metal] Using reexported Mesa symbols (via linker -Wl,-reexport_library)\n");
    printf("[GLX→Metal]   Mesa symbols available, but fishhook in libGLMetal.dylib intercepts them\n");
    fflush(stdout);
    
    original_libgl = (void*)1; // Mark as initialized
    
    // Load function pointers from reexported library
    orig_glXChooseVisual = (glXChooseVisual_t)dlsym(RTLD_DEFAULT, "glXChooseVisual");
    orig_glXCreateContext = (glXCreateContext_t)dlsym(RTLD_DEFAULT, "glXCreateContext");
    orig_glXDestroyContext = (glXDestroyContext_t)dlsym(RTLD_DEFAULT, "glXDestroyContext");
    orig_glXMakeCurrent = (glXMakeCurrent_t)dlsym(RTLD_DEFAULT, "glXMakeCurrent");
    orig_glXSwapBuffers = (glXSwapBuffers_t)dlsym(RTLD_DEFAULT, "glXSwapBuffers");
    orig_glXQueryExtensionsString = (glXQueryExtensionsString_t)dlsym(RTLD_DEFAULT, "glXQueryExtensionsString");
    orig_glXGetProcAddressARB = (glXGetProcAddressARB_t)dlsym(RTLD_DEFAULT, "glXGetProcAddressARB");
    orig_glXQueryDrawable = (glXQueryDrawable_t)dlsym(RTLD_DEFAULT, "glXQueryDrawable");
}

// GLX wrapper functions
XVisualInfo* glXChooseVisual(Display *dpy, int screen, int *attribList) {
    printf("[GLX→Metal] glXChooseVisual() called (Display=%p, screen=%d)\n", (void*)dpy, screen);
    fflush(stdout);
    
    load_original_libgl();
    
    if (!orig_glXChooseVisual) {
        fprintf(stderr, "[GLX→Metal] ERROR: orig_glXChooseVisual is NULL!\n");
        fflush(stderr);
        return NULL;
    }
    
    // BYPASS Mesa entirely - always return a fake visual on FIRST call
    // This avoids the infinite loop of glxgears trying different configs
    static int call_count = 0;
    static XVisualInfo *fake_visual = NULL;
    
    call_count++;
    
    if (call_count == 1) {
        // On first call, try Mesa
        XVisualInfo *result = orig_glXChooseVisual(dpy, screen, attribList);
        if (result) {
            printf("[GLX→Metal]   ✓ Mesa returned valid visual: %p\n", (void*)result);
            fflush(stdout);
            return result;
        }
    }
    
    // Mesa failed or subsequent calls - allocate fake visual (so glxgears can free it)
    // Note: XVisualInfo is typedef'd as XID (unsigned long) but glxgears expects a pointer
    if (!fake_visual) {
        // Allocate 256 bytes for XVisualInfo struct (actual struct, not the typedef)
        fake_visual = (XVisualInfo*)malloc(256);
        memset(fake_visual, 0, 256);
        printf("[GLX→Metal]   Allocated fake XVisualInfo at %p\n", (void*)fake_visual);
    }
    printf("[GLX→Metal]   Returning FAKE XVisualInfo (call #%d) - bypassing Mesa\n", call_count);
    fflush(stdout);
    return fake_visual;
}

GLXContext glXCreateContext(Display *dpy, XVisualInfo *vis, GLXContext shareList, int direct) {
    static GLXContext fake_context = NULL;
    
    if (!vis) {
        fprintf(stderr, "[GLX→Metal] ERROR: XVisualInfo is NULL! Cannot create context.\n");
        fflush(stderr);
        return NULL;
    }
    
    load_original_libgl();
    // Metal connection handled by gl_to_metal_client.c
    
    printf("[GLX→Metal] glXCreateContext() called (vis=%p, direct=%d)\n", (void*)vis, direct);
    fflush(stdout);
    
    // SKIP Mesa entirely - it tries to create CGL context which fails in X11/XQuartz
    // Mesa's CGL backend doesn't work with XQuartz - always use fake context
    
    // Allocate fake context (only once)
    if (!fake_context) {
        // Allocate 256 bytes for GLXContext struct (glxgears may try to free it)
        fake_context = (GLXContext)malloc(256);
        memset(fake_context, 0, 256);
        printf("[GLX→Metal]   Allocated fake GLXContext at %p\n", (void*)fake_context);
    }
    
    printf("[GLX→Metal]   ✓ Returning fake context - Metal will handle all rendering!\n");
    printf("[GLX→Metal]   → GLXContext: %p\n", (void*)fake_context);
    fflush(stdout);
    return fake_context;
}

void glXDestroyContext(Display *dpy, GLXContext ctx) {
    printf("[GLX→Metal] glXDestroyContext() called\n");
    fflush(stdout);
    
    load_original_libgl();
    
    if (orig_glXDestroyContext) {
        orig_glXDestroyContext(dpy, ctx);
    }
}

int glXMakeCurrent(Display *dpy, GLXDrawable drawable, GLXContext ctx) {
    static int call_count = 0;
    call_count++;
    
    if (!ctx) {
        fprintf(stderr, "[GLX→Metal] WARNING: Context is NULL\n");
        fflush(stderr);
    }
    
    load_original_libgl();
    // Metal connection handled by gl_to_metal_client.c
    
    // Try Mesa ONLY on first call
    if (call_count == 1) {
        printf("[GLX→Metal] glXMakeCurrent() called (ctx=%p, drawable=%lu) - trying Mesa...\n", 
               (void*)ctx, (unsigned long)drawable);
        fflush(stdout);
        
        if (orig_glXMakeCurrent) {
            int result = orig_glXMakeCurrent(dpy, drawable, ctx);
            if (result) {
                printf("[GLX→Metal]   ✓ Mesa MakeCurrent succeeded\n");
                fflush(stdout);
                return result;
            }
            printf("[GLX→Metal]   ✗ Mesa MakeCurrent failed - XQuartz lacks GL\n");
        }
    }
    
    // All subsequent calls or if Mesa failed: bypass and return success
    if (call_count == 2) {
        printf("[GLX→Metal] glXMakeCurrent() - bypassing Mesa, returning SUCCESS\n");
        printf("[GLX→Metal]   ✓ Metal will handle all rendering!\n");
        fflush(stdout);
    }
    
    return 1; // Always succeed for Metal rendering
}

// glXSwapBuffers - REMOVED from glx_wrapper, handled by fishhook in gl_to_metal_client.c
// This allows fishhook to properly intercept glXSwapBuffers calls
// The hook in gl_to_metal_client.c will send CMD_METAL_SWAP_BUFFERS and call orig Mesa

const char* glXQueryExtensionsString(Display *dpy, int screen) {
    static int call_count = 0;
    static const char *fake_extensions = "GLX_ARB_get_proc_address GLX_EXT_swap_control";
    
    call_count++;
    
    load_original_libgl();
    
    // Try Mesa ONLY on first call
    if (call_count == 1) {
        printf("[GLX→Metal] glXQueryExtensionsString() called - trying Mesa...\n");
        fflush(stdout);
        
        if (orig_glXQueryExtensionsString) {
            const char *result = orig_glXQueryExtensionsString(dpy, screen);
            if (result && result[0] != '\0') {
                printf("[GLX→Metal]   ✓ Mesa returned: %s\n", result);
                fflush(stdout);
                return result;
            }
            printf("[GLX→Metal]   ✗ Mesa returned NULL or empty\n");
        }
    }
    
    // All subsequent calls or if Mesa failed: return fake extensions
    if (call_count == 2) {
        printf("[GLX→Metal] glXQueryExtensionsString() - returning FAKE extensions\n");
        printf("[GLX→Metal]   → \"%s\"\n", fake_extensions);
        fflush(stdout);
    }
    
    return fake_extensions;
}

__GLXextFuncPtr glXGetProcAddressARB(const unsigned char *procName) {
    printf("[GLX→Metal] glXGetProcAddressARB(%s) called\n", procName);
    fflush(stdout);
    
    load_original_libgl();
    
    if (orig_glXGetProcAddressARB) {
        return orig_glXGetProcAddressARB(procName);
    }
    
    return NULL;
}

void glXQueryDrawable(Display *dpy, GLXDrawable draw, int attribute, unsigned int *value) {
    static int call_count = 0;
    call_count++;
    
    load_original_libgl();
    
    // Try Mesa ONLY on first call
    if (call_count == 1) {
        printf("[GLX→Metal] glXQueryDrawable() called - trying Mesa...\n");
        fflush(stdout);
        
        if (orig_glXQueryDrawable && value) {
            *value = 0;  // Initialize
            orig_glXQueryDrawable(dpy, draw, attribute, value);
            if (*value != 0) {
                printf("[GLX→Metal]   ✓ Mesa returned value: %u\n", *value);
                fflush(stdout);
                return;
            }
            printf("[GLX→Metal]   ✗ Mesa returned 0 or failed\n");
        }
    }
    
    // All subsequent calls or if Mesa failed: return fake value
    if (value) {
        *value = 1024;  // Fake drawable size
        if (call_count == 2) {
            printf("[GLX→Metal] glXQueryDrawable() - returning FAKE value: %u\n", *value);
            fflush(stdout);
        }
    }
}

// Load Metal translator
static void load_metal_translator() {
    if (metal_translator) return;
    
    // Try to load libGLMetal.dylib from home directory
    const char *home = getenv("HOME");
    if (!home) return;
    
    char path[1024];
    snprintf(path, sizeof(path), "%s/libGLMetal.dylib", home);
    
    metal_translator = dlopen(path, RTLD_NOW | RTLD_GLOBAL);
    if (metal_translator) {
        printf("[GLX→Metal] ✅ Loaded Metal translator from %s\n", path);
        printf("[GLX→Metal] OpenGL calls will now be translated to Metal!\n");
        fflush(stdout);
    } else {
        printf("[GLX→Metal] Note: Metal translator not found at %s\n", path);
        printf("[GLX→Metal]   Using Mesa software rendering instead\n");
        fflush(stdout);
    }
}

// Constructor
__attribute__((constructor))
static void glx_wrapper_init(void) {
    printf("========================================\n");
    printf("  GLX→Metal Wrapper Library\n");
    printf("  Intercepting GLX calls for Metal\n");
    printf("========================================\n");
    fflush(stdout);
    
    load_original_libgl();
    // DON'T load Metal translator here - it should be injected via DYLD_INSERT_LIBRARIES
    // This allows fishhook to intercept symbols before they're bound to Mesa
    printf("[GLX→Metal] Note: Use DYLD_INSERT_LIBRARIES=~/libGLMetal.dylib for Metal acceleration\n");
    fflush(stdout);
}

// ========================================================================
// OpenGL Function Forwarding Stubs
// Auto-generated stubs are in gl_stubs_generated.c (1354 functions)
// ========================================================================

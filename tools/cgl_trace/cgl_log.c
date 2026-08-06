/*
 * cgl_log.c — DYLD_INSERT_LIBRARIES interposer for PowerFox CGL/GL tracing
 *
 * Build:
 *   clang -arch x86_64 -mmacosx-version-min=10.6 \
 *         -dynamiclib -o cgl_log.dylib cgl_log.c \
 *         -framework OpenGL -Wno-deprecated-declarations
 *
 * Run:
 *   DYLD_INSERT_LIBRARIES=/path/to/cgl_log.dylib \
 *   /Applications/PowerFox.app/Contents/MacOS/powerfox
 *
 * Hooks the full CGL lifecycle + the GL getters XUL imports, so we can
 * localize where the compositor init fails:
 *   CGLChoosePixelFormat  -> pixel format selection
 *   CGLCreateContext      -> context creation
 *   CGLSetCurrentContext  -> thread binding
 *   CGLSetSurface         -> window-system attachment  <-- the prime suspect
 *   CGLFlushDrawable      -> swap                       <-- and this one
 *   CGLDestroyContext     -> teardown
 *   glGetString           -> renderer/version/extensions probing
 *   glGetError            -> post-call error check (Gecko imports directly)
 *   glFlush               -> force-finish (Gecko imports directly)
 *
 * Uses __DATA,__interpose (sanctioned, two-level-namespace safe). The
 * "original" pointer in each pair is what the replacement calls through
 * to forward, which is the pass-through primitive for free.
 */

#include <OpenGL/OpenGL.h>
#include <OpenGL/gl.h>
#include <stdio.h>
#include <unistd.h>
#include <mach/mach_time.h>

#define LOG_TAG "[cgl-interpose]"

/* Monotonic microsecond timestamps for flush timing. */
static mach_timebase_info_data_t g_tbi;
static void init_tbi(void) {
    if (g_tbi.denom == 0) mach_timebase_info(&g_tbi);
}
static uint64_t now_us(void) {
    init_tbi();
    /* numer/denom are <= 2^16 on macOS, product with mach_absolute_time
     * doesn't overflow uint64 for realistic uptimes. */
    uint64_t abs = mach_absolute_time();
    uint64_t ns  = (abs * (uint64_t)g_tbi.numer) / (uint64_t)g_tbi.denom;
    return ns / 1000ull;
}

/* CGLSetSurface and CGDirectDisplayID were public on 10.6 but have been
 * removed from the modern OpenGL headers (deprecated). The symbols still
 * exist in the 10.6 OpenGL binary, so we forward-declare and call directly.
 * CGDirectDisplayID is a 32-bit display identifier from CoreGraphics.
 */
typedef uint32_t CGDirectDisplayID;
extern CGLError CGLSetSurface(CGLContextObj, CGDirectDisplayID, GLint,
                              GLint, GLint, GLsizei, GLsizei);

static const char *err_name(CGLError e) {
    switch (e) {
        case kCGLNoError:            return "ok";
        case kCGLBadAttribute:       return "BadAttribute";
        case kCGLBadProperty:        return "BadProperty";
        case kCGLBadPixelFormat:     return "BadPixelFormat";
        case kCGLBadRendererInfo:    return "BadRendererInfo";
        case kCGLBadContext:         return "BadContext";
        case kCGLBadDrawable:        return "BadDrawable";
        case kCGLBadDisplay:         return "BadDisplay";
        case kCGLBadState:           return "BadState";
        case kCGLBadValue:           return "BadValue";
        case kCGLBadMatch:           return "BadMatch";
        case kCGLBadEnumeration:     return "BadEnumeration";
        case kCGLBadAlloc:           return "BadAlloc";
        /* kCGLBadCurrentContext = 1009 historically; removed from modern headers */
        case (CGLError)1009:         return "BadCurrentContext";
        default:                     return "?";
    }
}

static void log_attribs(const CGLPixelFormatAttribute *a) {
    fprintf(stderr, LOG_TAG " CGLChoosePixelFormat attrs:");
    if (!a) { fprintf(stderr, " (null)\n"); return; }
    while (*a != 0) {
        const char *name = NULL;
        int has_value = 0;
        switch (*a) {
            case kCGLPFAAccelerated:              name = "Accelerated"; break;
            case kCGLPFARendererID:               name = "RendererID"; has_value = 1; break;
            case kCGLPFADisplayMask:              name = "DisplayMask"; has_value = 1; break;
            case kCGLPFAColorSize:                name = "ColorSize"; has_value = 1; break;
            case kCGLPFAAlphaSize:                name = "AlphaSize"; has_value = 1; break;
            case kCGLPFADepthSize:                name = "DepthSize"; has_value = 1; break;
            case kCGLPFAStencilSize:              name = "StencilSize"; has_value = 1; break;
            case kCGLPFAAuxBuffers:               name = "AuxBuffers"; has_value = 1; break;
            case kCGLPFASampleBuffers:            name = "SampleBuffers"; has_value = 1; break;
            case kCGLPFASamples:                  name = "Samples"; has_value = 1; break;
            case kCGLPFADoubleBuffer:             name = "DoubleBuffer"; break;
            case kCGLPFAOffScreen:                name = "OffScreen"; break;
            case kCGLPFAFullScreen:               name = "FullScreen"; break;
            case kCGLPFAMinimumPolicy:            name = "MinimumPolicy"; break;
            case kCGLPFAMaximumPolicy:            name = "MaximumPolicy"; break;
            case kCGLPFAMultiScreen:              name = "MultiScreen"; break;
            case kCGLPFACompliant:                name = "Compliant"; break;
            case kCGLPFAClosestPolicy:            name = "ClosestPolicy"; break;
            case kCGLPFABackingStore:             name = "BackingStore"; break;
            case kCGLPFAWindow:                   name = "Window"; break;
            case kCGLPFAPBuffer:                  name = "PBuffer"; break;
            case kCGLPFARemotePBuffer:            name = "RemotePBuffer"; break;
            case kCGLPFAAllowOfflineRenderers:    name = "AllowOfflineRenderers"; break;
            case kCGLPFAAcceleratedCompute:       name = "AcceleratedCompute"; break;
            case kCGLPFASupportsAutomaticGraphicsSwitching:
                                                 name = "SupportsAutomaticGraphicsSwitching"; break;
            case kCGLPFAVirtualScreenCount:       name = "VirtualScreenCount"; has_value = 1; break;
            default:                              name = NULL; break;
        }
        if (name) {
            if (has_value) {
                a++;
                fprintf(stderr, " %s=%lu", name, (unsigned long)*a);
            } else {
                fprintf(stderr, " %s", name);
            }
        } else {
            fprintf(stderr, " ?%u", (unsigned)*a);
        }
        a++;
    }
    fprintf(stderr, "\n");
    fflush(stderr);
}

/* ---- CGLChoosePixelFormat ---- */
static CGLError
my_CGLChoosePixelFormat(const CGLPixelFormatAttribute *attribs,
                        CGLPixelFormatObj *pix, GLint *npix) {
    log_attribs(attribs);
    CGLError err = CGLChoosePixelFormat(attribs, pix, npix);
    fprintf(stderr, LOG_TAG "   -> err=%s pix=%p npix=%d\n",
            err_name(err), (void*)(pix ? *pix : NULL),
            (int)(npix ? *npix : -1));
    fflush(stderr);
    return err;
}

/* ---- CGLCreateContext ---- */
static CGLError
my_CGLCreateContext(CGLPixelFormatObj pix, CGLContextObj share, CGLContextObj *ctx) {
    CGLError err = CGLCreateContext(pix, share, ctx);
    fprintf(stderr, LOG_TAG " CGLCreateContext pix=%p share=%p -> err=%s ctx=%p\n",
            (void*)pix, (void*)share, err_name(err),
            (void*)(ctx ? *ctx : NULL));
    fflush(stderr);
    return err;
}

/* ---- CGLDestroyContext ---- */
static CGLError
my_CGLDestroyContext(CGLContextObj ctx) {
    fprintf(stderr, LOG_TAG " CGLDestroyContext ctx=%p\n", (void*)ctx);
    fflush(stderr);
    return CGLDestroyContext(ctx);
}

/* ---- CGLSetCurrentContext ---- */
static CGLError
my_CGLSetCurrentContext(CGLContextObj ctx) {
    CGLError err = CGLSetCurrentContext(ctx);
    fprintf(stderr, LOG_TAG " CGLSetCurrentContext ctx=%p -> err=%s\n",
            (void*)ctx, err_name(err));
    fflush(stderr);
    return err;
}

/* ---- CGLSetSurface  (the prime suspect) ----
 * Signature: CGLSetSurface(CGLContextObj, CGDirectDisplayID, GLint window,
 *                          GLint x, GLint y, GLsizei w, GLsizei h)
 * A failing or no-op SetSurface is the "renders offscreen" symptom.
 */
static CGLError
my_CGLSetSurface(CGLContextObj ctx, CGDirectDisplayID display,
                 GLint window, GLint x, GLint y, GLsizei w, GLsizei h) {
    CGLError err = CGLSetSurface(ctx, display, window, x, y, w, h);
    fprintf(stderr, LOG_TAG " CGLSetSurface ctx=%p display=%u win=%d geom={%d,%d,%dx%d} -> err=%s\n",
            (void*)ctx, (unsigned)display, (int)window,
            (int)x, (int)y, (int)w, (int)h, err_name(err));
    fflush(stderr);
    return err;
}

/* ---- CGLFlushDrawable  (the second suspect — the swap) ----
 * Capture entry and exit timestamps to distinguish:
 *   - long duration inside call  = real rasterization cost (throughput-bound)
 *   - short duration, long gaps  = Gecko not submitting work (idle/throttled)
 * Also log the gap since the previous flush on this context.
 */
static CGLError
my_CGLFlushDrawable(CGLContextObj ctx) {
    static int flush_count = 0;
    static uint64_t last_entry_us = 0;
    static uint64_t last_exit_us = 0;
    static CGLContextObj last_ctx = NULL;

    flush_count++;
    uint64_t entry = now_us();
    uint64_t gap = (last_exit_us && (last_ctx == ctx))
                   ? (entry - last_exit_us) : 0;

    CGLError err = CGLFlushDrawable(ctx);
    uint64_t exit = now_us();
    uint64_t dur = exit - entry;

    /* Always log first 16 to see the pattern, then every 16th. */
    if (flush_count <= 16 || (flush_count % 16) == 0) {
        fprintf(stderr,
                LOG_TAG " CGLFlushDrawable ctx=%p -> err=%s  "
                        "dur=%llu us  gap=%llu us  (#%d)\n",
                (void*)ctx, err_name(err),
                (unsigned long long)dur,
                (unsigned long long)gap,
                flush_count);
        fflush(stderr);
    }
    last_entry_us = entry;
    last_exit_us  = exit;
    last_ctx      = ctx;
    return err;
}

/* ---- glGetString — capture capability probes ----
 * Gecko queries GL_RENDERER, GL_VERSION, GL_EXTENSIONS, GL_VENDOR, GL_SHADING_LANGUAGE_VERSION.
 * We translate the enum to a readable name and log the returned string.
 */
static const char *
glname_name(GLenum name) {
    switch (name) {
        case 0x1F00: return "GL_VENDOR";
        case 0x1F01: return "GL_RENDERER";
        case 0x1F02: return "GL_VERSION";
        case 0x1F03: return "GL_EXTENSIONS";
        case 0x8B8C: return "GL_SHADING_LANGUAGE_VERSION";
        default:     return NULL;
    }
}

static const GLubyte *
my_glGetString(GLenum name) {
    const GLubyte *r = glGetString(name);
    const char *pretty = glname_name(name);
    if (pretty) {
        fprintf(stderr, LOG_TAG " glGetString(%s) -> \"%s\"\n",
                pretty, r ? (const char*)r : "(null)");
    } else {
        fprintf(stderr, LOG_TAG " glGetString(0x%x) -> \"%s\"\n",
                (unsigned)name, r ? (const char*)r : "(null)");
    }
    fflush(stderr);
    return r;
}

/* ---- glGetError — XUL imports directly, fires on every GL call boundary ---- */
static GLenum
my_glGetError(void) {
    GLenum e = glGetError();
    /* Keep it terse — this fires constantly when active. Only log non-zero. */
    if (e != 0) {
        fprintf(stderr, LOG_TAG " glGetError -> 0x%x\n", (unsigned)e);
        fflush(stderr);
    }
    return e;
}

/* ---- glFlush — XUL imports directly ---- */
static void
my_glFlush(void) {
    static int count = 0;
    count++;
    /* Log first few, then summarize so we don't drown the log. */
    if (count <= 8) {
        fprintf(stderr, LOG_TAG " glFlush  (#%d)\n", count);
        fflush(stderr);
    } else if (count == 9) {
        fprintf(stderr, LOG_TAG " glFlush  (#%d, suppressing further)\n", count);
        fflush(stderr);
    }
    glFlush();
}

struct interpose_pair {
    const void *replacement;
    const void *original;
};

#define INTERPOSE(name) \
    __attribute__((used)) \
    static const struct interpose_pair _interpose_##name \
    __attribute__((section("__DATA,__interpose"))) = { \
        (const void *)my_##name, \
        (const void *)name \
    }

INTERPOSE(CGLChoosePixelFormat);
INTERPOSE(CGLCreateContext);
INTERPOSE(CGLDestroyContext);
INTERPOSE(CGLSetCurrentContext);
INTERPOSE(CGLSetSurface);
INTERPOSE(CGLFlushDrawable);
INTERPOSE(glGetString);
INTERPOSE(glGetError);
INTERPOSE(glFlush);

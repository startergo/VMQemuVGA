/*
 * probe_cgs_requester.m — coupling probe, Phase A: does CGS adopt the
 * accelerator's surface path when an app requests a window surface?
 *
 * Pre-registered outcomes (2026-08-14, before the run):
 *   ADOPTED   — CGSAddSurface returns success AND kernel.log shows
 *               VMAccelSurfaceClient lines DURING this program's run.
 *               This program makes NO IOKit calls, so any surface-client
 *               line is WindowServer-originated: a call we didn't make.
 *               Coupling is open; BindSurface's GL-args become the next
 *               locus (CGSBindSurface is 6-arg: cid,wid,sid + 3 GL-side
 *               args — the GLD-coupling question lives there).
 *   REFUSED   — CGSAddSurface returns an error. Record the code; it
 *               discriminates "no usable accel path" from other causes.
 *   SILENT    — AddSurface succeeds but kernel.log stays quiet: CGS
 *               created the surface without consulting the accelerator
 *               (software/IOSurface backing). That is itself decisive
 *               evidence about which path 10.6 chooses for plain
 *               AddSurface — the GLD question then sharpens to "what
 *               makes WindowServer pick the accel-surface path".
 *
 * Requires: gate boot-arg in the RUNNING kernel (vm-accel-surface=1),
 * VMAccelSurfaceClient kext loaded. Handlers still return Unsupported
 * by design — Phase A tests adoption, not memory mapping. Safe.
 *
 * Signatures recovered from the guest's own CoreGraphics binary
 * (2026-08-14 disassembly):
 *   CGSAddSurface(cid, wid, uint32_t *out_sid)          3 args, out NULL-checked
 *   CGSBindSurface(cid, wid, sid, a4, a5, a6)           6 args, sid!=0 checked
 *   CGSFlushSurface(cid, wid, sid)                      tail-calls
 *       CGSFlushSurfaceWithOptions(..., option=1)
 *   CGSOrderSurface(cid, wid, sid, mode, rel)           >=5 args
 *   CGSRemoveSurface(cid, wid, sid)
 *   CGSMainConnectionID(void)
 *
 * Cross-compile against the 10.6 SDK, x86_64.
 */

#import <Cocoa/Cocoa.h>
#include <dlfcn.h>
#include <stdio.h>
#include <unistd.h>
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl.h>

typedef int CGSCID;          /* CGSConnectionID */
typedef int CGSWID;          /* CGSWindowID    */
typedef unsigned int CGSSID; /* CGSSurfaceID   */

static void *cgsh;
static void *glh;

static void *sym(const char *name)
{
    void *p = dlsym(cgsh, name);
    if (!p)
        fprintf(stderr, "cgs_requester: MISSING symbol %s\n", name);
    return p;
}

static void *glsym(const char *name)
{
    void *p = dlsym(glh, name);
    if (!p)
        fprintf(stderr, "cgs_requester: MISSING GL symbol %s\n", name);
    return p;
}

/* Function pointer types for the recovered signatures */
typedef int (*fn_mainconn)(void);
typedef int (*fn_add)(CGSCID, CGSWID, CGSSID *);
typedef int (*fn_flush)(CGSCID, CGSWID, CGSSID);
typedef int (*fn_order)(CGSCID, CGSWID, CGSSID, int, int);
typedef int (*fn_remove)(CGSCID, CGSWID, CGSSID);
/* CGLSetSurface — 4 args, confirmed by disassembly of the real
 * OpenGL.framework (2026-08-14): rdi=ctx (mutex-deref'd),
 * esi/edx/ecx re-loaded for a 4-arg indirect call. The
 * substitute's forward table declares 6 args — latent bug there,
 * not exercised by Gecko so far. */
typedef int (*fn_cglsetsurface)(CGLContextObj, CGSCID, CGSWID, CGSSID);

#define ERRDESC(e) ((e) == 0 ? "OK" : "err")

int main(int argc, char *argv[])
{
    int hold_secs = 20;
    if (argc > 1) hold_secs = atoi(argv[1]);

    printf("=== probe_cgs_requester (2026-08-14, phase A2) ===\n");
    printf("CGS surface + CGL context binding. No IOKit calls are made\n");
    printf("by this program — any VMAccelSurfaceClient kernel.log line\n");
    printf("during this run is WindowServer-originated.\n\n");

    cgsh = dlopen("/System/Library/Frameworks/ApplicationServices.framework/"
                  "Frameworks/CoreGraphics.framework/Versions/A/CoreGraphics",
                  RTLD_LAZY);
    glh = dlopen("/System/Library/Frameworks/OpenGL.framework/"
                 "Versions/A/OpenGL", RTLD_LAZY);
    if (!cgsh || !glh) {
        printf("FAIL: dlopen: %s\n", dlerror());
        return 1;
    }

    fn_mainconn mainconn = (fn_mainconn)sym("CGSMainConnectionID");
    fn_add      add      = (fn_add)sym("CGSAddSurface");
    fn_flush    flush    = (fn_flush)sym("CGSFlushSurface");
    fn_order    order    = (fn_order)sym("CGSOrderSurface");
    fn_remove   remove   = (fn_remove)sym("CGSRemoveSurface");
    fn_cglsetsurface cglsetsurface =
        (fn_cglsetsurface)glsym("CGLSetSurface");
    if (!mainconn || !add || !flush || !order || !remove || !cglsetsurface)
        return 1;

    /* AppKit: own a real window so AddSurface gets a real window id. */
    NSApplication *app = [NSApplication sharedApplication];
    [app setActivationPolicy:NSApplicationActivationPolicyRegular];
    NSRect frame = NSMakeRect(200, 200, 320, 240);
    NSWindow *win = [[NSWindow alloc] initWithContentRect:frame
                                                styleMask:NSTitledWindowMask
                                                  backing:NSBackingStoreBuffered
                                                    defer:NO];
    [win setTitle:@"cgs requester"];
    [win orderFrontRegardless];
    CGSWID wid = [win windowNumber];
    printf("window created: number=%d\n", wid);
    if (!wid) {
        printf("FAIL: no window number — not in a GUI session?\n");
        return 1;
    }

    CGSCID cid = mainconn();
    printf("CGSMainConnectionID=%d\n", cid);

    /* --- Renderer census: what does real CGL see in this registry? --- */
    CGLRendererInfoObj ri = NULL;
    GLint nrend = 0;
    CGLError e_ri = CGLQueryRendererInfo(CGDisplayIDToOpenGLDisplayMask(
                                             CGMainDisplayID()),
                                         &ri, &nrend);
    printf("CGLQueryRendererInfo -> %d nrend=%d\n", e_ri, nrend);
    if (e_ri == kCGLNoError && nrend > 0 && ri) {
        for (GLint i = 0; i < nrend && i < 4; i++) {
            GLint accel = -1, vid = -1;
            CGLDescribeRenderer(ri, i, kCGLRPAccelerated, &accel);
            CGLDescribeRenderer(ri, i, kCGLRPRendererID, &vid);
            printf("  renderer[%d]: accelerated=%d rendererID=0x%x\n",
                   i, accel, vid);
        }
        CGLDestroyRendererInfo(ri);
    }

    /* --- Pixel format: accelerated first, plain fallback --- */
    CGLPixelFormatObj pf = NULL;
    GLint npix = 0;
    CGLError e_pf;
    CGLPixelFormatAttribute acc_attrs[] = {
        kCGLPFAAccelerated, kCGLPFADoubleBuffer, (CGLPixelFormatAttribute)0 };
    CGLPixelFormatAttribute plain_attrs[] = {
        kCGLPFADoubleBuffer, (CGLPixelFormatAttribute)0 };
    e_pf = CGLChoosePixelFormat(acc_attrs, &pf, &npix);
    printf("CGLChoosePixelFormat(accelerated) -> %d npix=%d\n", e_pf, npix);
    if (e_pf != kCGLNoError || !pf) {
        pf = NULL;
        e_pf = CGLChoosePixelFormat(plain_attrs, &pf, &npix);
        printf("CGLChoosePixelFormat(plain) -> %d npix=%d\n", e_pf, npix);
    }

    /* --- Surface --- */
    CGSSID sid = 0;
    int e_add = add(cid, wid, &sid);
    printf("CGSAddSurface -> %d (%s) surfaceID=0x%x\n",
           e_add, ERRDESC(e_add), sid);

    /* --- Context + THE BINDING STEP --- */
    CGLContextObj ctx = NULL;
    if (pf) {
        CGLError e_ctx = CGLCreateContext(pf, NULL, &ctx);
        printf("CGLCreateContext -> %d ctx=%p\n", e_ctx, (void *)ctx);
        if (e_ctx == kCGLNoError && ctx && sid != 0) {
            CGLError e_set = (CGLError)cglsetsurface(ctx, cid, wid, sid);
            printf("CGLSetSurface(ctx,cid,wid,sid) -> %d (%s)   "
                   "<== the coupling step\n", e_set, ERRDESC(e_set));
            int e_fl = flush(cid, wid, sid);
            printf("CGSFlushSurface -> %d (%s)\n", e_fl, ERRDESC(e_fl));
            /* RUNG 39: ORDER the surface BEFORE drawing (real apps'
             * sequence; the bounds query returns 0x0 until ordered). */
            int e_ord0 = order(cid, wid, sid, 1, 0);
            printf("CGSOrderSurface(pre-draw) -> %d (%s)\n", e_ord0, ERRDESC(e_ord0));
            fflush(stdout);
            /* RUNG 34: the full app draw cycle — current, draw,
             * swap. CGLFlushDrawable is the compositing call, the
             * first flush through this driver. */
            CGLError e_cur = CGLSetCurrentContext(ctx);
            printf("CGLSetCurrentContext -> %d\n", e_cur);
            /* RUNG 50 — the limits census: what does the engine answer
             * before/after the config block is filled? */
            {
                GLint tex = -1, vp[2] = { -1, -1 };
                GLint r = -1, g = -1, b = -1, a = -1, dbits = -1, sbits = -1;
                glGetIntegerv(0x0D33, &tex);          /* MAX_TEXTURE_SIZE */
                glGetIntegerv(0x0D3A, vp);            /* MAX_VIEWPORT_DIMS */
                glGetIntegerv(0x0D52, &r);            /* RED_BITS */
                glGetIntegerv(0x0D53, &g);
                glGetIntegerv(0x0D54, &b);
                glGetIntegerv(0x0D55, &a);
                glGetIntegerv(0x0D56, &dbits);        /* DEPTH_BITS */
                glGetIntegerv(0x0D57, &sbits);        /* STENCIL_BITS */
                printf("LIMITS: tex=%d vp=%dx%d bits r%d g%d b%d a%d d%d s%d\n",
                       tex, vp[0], vp[1], r, g, b, a, dbits, sbits);
                fflush(stdout);
            }
            glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
            glClearDepth(1.0);
            glClearStencil(0);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT
                    | GL_STENCIL_BUFFER_BIT);
            printf("glClear done, glGetError = 0x%x\n", glGetError());
            /* rung 38: the readback proof — fires the ReadPixels slot */
            {
                unsigned char px[64];
                glReadPixels(0, 0, 4, 4, GL_RGBA, GL_UNSIGNED_BYTE, px);
                printf("glReadPixels done, glGetError = 0x%x, "
                       "first pixel = %02x %02x %02x %02x\n",
                       glGetError(), px[0], px[1], px[2], px[3]);
            }
            CGLError e_swap = CGLFlushDrawable(ctx);
            printf("CGLFlushDrawable -> %d  <== the swap\n", e_swap);
            fflush(stdout);
        }
    } else {
        printf("NO PIXEL FORMAT — real CGL cannot create any context.\n");
        printf("That datum itself gates the coupling question on the\n");
        printf("capability flip (no renderer visible in registry).\n");
    }

    if (e_add == 0 && sid != 0) {
        int e_ord = order(cid, wid, sid, 1, 0);
        printf("CGSOrderSurface -> %d (%s)\n", e_ord, ERRDESC(e_ord));
    }

    printf("\nHolding for %ds — watch kernel.log for VMAccelSurfaceClient "
           "lines not made by this program...\n", hold_secs);
    fflush(stdout);
    sleep(hold_secs);

    if (ctx) CGLReleaseContext(ctx);
    if (pf) CGLReleasePixelFormat(pf);
    if (e_add == 0 && sid != 0) {
        int e_rm = remove(cid, wid, sid);
        printf("CGSRemoveSurface -> %d (%s)\n", e_rm, ERRDESC(e_rm));
    }
    [win release];
    printf("=== done ===\n");
    return 0;
}

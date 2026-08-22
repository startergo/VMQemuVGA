/*
 * probe_r7.c — rung 7a: which CGL operation triggers gldGetRendererInfo
 * on a registry-named GLD? Behavioral discriminator (pre-registered
 * candidate readings: (i) engine consults only its own float path;
 * (ii) lazy at ChoosePixelFormat, gated by attributes; (iii) display
 * mask mismatch).
 *
 * Each mode runs in its OWN process (the loader's cycle is per-process).
 * The stub in /S/L/E logs any entry call to /tmp/vm_gld_stub.log — read
 * it between runs. Pure CGL: no window, no AppKit.
 *
 * Modes: c = control census (as probe_cgs_requester)
 *        m = CGLQueryRendererInfo(0xFFFFFFFF) — all-display mask
 *        d = census + CGLDescribeRenderer walk of every property of the
 *            enumerated renderer(s)
 *        p = census + extended pixel-format attempts (offscreen, robust,
 *            stereo, fullscreen-accelerated variants)
 */

#include <OpenGL/OpenGL.h>
#include <OpenGL/gl.h>
#include <ApplicationServices/ApplicationServices.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void census(const char *tag)
{
    CGLRendererInfoObj ri = NULL;
    GLint nrend = 0;
    CGLError e = CGLQueryRendererInfo(0xFFFFFFFFu, &ri, &nrend);
    printf("[%s] QueryRendererInfo(0xFFFFFFFF) -> %d nrend=%d\n", tag, e, nrend);
    if (e != kCGLNoError || !ri || nrend <= 0) return;
    for (GLint i = 0; i < nrend && i < 4; i++) {
        GLint accel = -1, rid = -1;
        CGLDescribeRenderer(ri, i, kCGLRPAccelerated, &accel);
        CGLDescribeRenderer(ri, i, kCGLRPRendererID, &rid);
        printf("  [%s] renderer[%d]: accelerated=%d rid=0x%x\n",
               tag, i, accel, rid);
    }
    CGLDestroyRendererInfo(ri);
}

static void describe_walk(const char *tag)
{
    CGLRendererInfoObj ri = NULL;
    GLint nrend = 0;
    if (CGLQueryRendererInfo(0xFFFFFFFFu, &ri, &nrend) != kCGLNoError || !ri)
        return;
    static const CGLRendererProperty props[] = {
        kCGLRPOffScreen, kCGLRPRendererCount, kCGLRPAccelerated,
        kCGLRPRobust, kCGLRPBackingStore, kCGLRPMPSafe,
        kCGLRPWindow, kCGLRPMultiScreen, kCGLRPCompliant,
        kCGLRPDisplayMask, kCGLRPBufferModes, kCGLRPColorModes,
        kCGLRPAccumModes, kCGLRPDepthModes, kCGLRPStencilModes,
        kCGLRPMaxAuxBuffers, kCGLRPMaxSampleBuffers, kCGLRPMaxSamples,
        kCGLRPSampleModes, kCGLRPSampleAlpha, kCGLRPVideoMemory,
        kCGLRPTextureMemory, kCGLRPGPUVertProcCapable,
        kCGLRPGPUFragProcCapable, kCGLRPRendererID,     };
    static const int nprops = sizeof(props) / sizeof(props[0]);
    for (GLint i = 0; i < nrend && i < 2; i++) {
        for (int p = 0; p < nprops; p++) {
            GLint v = -1;
            CGLDescribeRenderer(ri, i, props[p], &v);
        }
        printf("[%s] describe-walk done renderer[%d] (%d props)\n",
               tag, i, nprops);
    }
    CGLDestroyRendererInfo(ri);
}

static void pixfmt(const char *tag, CGLPixelFormatAttribute *attrs,
                   const char *name)
{
    CGLPixelFormatObj pf = NULL;
    GLint npix = 0;
    printf("[%s] about-to-call pf(%s)\n", tag, name);
    fflush(stdout);
    CGLError e = CGLChoosePixelFormat(attrs, &pf, &npix);
    printf("[%s] pf(%s) -> %d npix=%d\n", tag, name, e, npix);
    fflush(stdout);
    if (pf) CGLDestroyPixelFormat(pf);
}

int main(int argc, char *argv[])
{
    const char mode = (argc > 1) ? argv[1][0] : 'c';
    printf("=== probe_r7 mode=%c pid=%d ===\n", mode, (int)getpid());
    fflush(stdout);

    switch (mode) {
    case 'c': {
        /* control: the main-display mask census, as before */
        CGLRendererInfoObj ri = NULL;
        GLint nrend = 0;
        CGLError e = CGLQueryRendererInfo(
            CGDisplayIDToOpenGLDisplayMask(CGMainDisplayID()), &ri, &nrend);
        printf("[c] QueryRendererInfo(main) -> %d nrend=%d\n", e, nrend);
        if (ri) CGLDestroyRendererInfo(ri);
        break;
    }
    case 'm':
        census("m");
        break;
    case 'x': {
        /* rung 11: query an arbitrary display mask (argv[2], hex) */
        unsigned mask = (argc > 2) ? strtoul(argv[2], NULL, 16) : 0x2;
        CGLRendererInfoObj ri = NULL;
        GLint nrend = 0;
        CGLError e = CGLQueryRendererInfo(mask, &ri, &nrend);
        printf("[x] QueryRendererInfo(0x%x) -> %d nrend=%d\n", mask, e, nrend);
        if (e == kCGLNoError && ri && nrend > 0) {
            for (GLint i = 0; i < nrend && i < 4; i++) {
                GLint accel = -1, rid = -1;
                CGLDescribeRenderer(ri, i, kCGLRPAccelerated, &accel);
                CGLDescribeRenderer(ri, i, kCGLRPRendererID, &rid);
                printf("  [x] renderer[%d]: accelerated=%d rid=0x%x\n",
                       i, accel, rid);
            }
        }
        if (ri) CGLDestroyRendererInfo(ri);
        break;
    }
    case 'd':
        census("d");
        describe_walk("d");
        break;
    case 'p': {
        census("p");
        CGLPixelFormatAttribute a1[] = { kCGLPFAAccelerated, (CGLPixelFormatAttribute)0 };
        CGLPixelFormatAttribute a2[] = { kCGLPFAOffScreen, (CGLPixelFormatAttribute)0 };
        CGLPixelFormatAttribute a3[] = { kCGLPFAAccelerated, kCGLPFADoubleBuffer, (CGLPixelFormatAttribute)0 };
        CGLPixelFormatAttribute a4[] = { kCGLPFARobust, (CGLPixelFormatAttribute)0 };
        CGLPixelFormatAttribute a5[] = { kCGLPFAAllRenderers, (CGLPixelFormatAttribute)0 };
        CGLPixelFormatAttribute a6[] = { kCGLPFAAllRenderers, kCGLPFAAccelerated, (CGLPixelFormatAttribute)0 };
        /* 18(b) 0x9dcd discriminator (pre-registered, LEDGER): the
         * 0x9dcd condition is "accelerated AND popcount(mask)!=1";
         * a7 carries an explicit single-display mask (popcount=1) —
         * prediction: passes 0x9dcd -> a GLD consult appears. a8 is
         * the mask-without-acceleration control. */
        CGLPixelFormatAttribute a7[] = { kCGLPFAAccelerated, kCGLPFADoubleBuffer, kCGLPFADisplayMask, (CGLPixelFormatAttribute)0x1, (CGLPixelFormatAttribute)0 };
        CGLPixelFormatAttribute a8[] = { kCGLPFADoubleBuffer, kCGLPFADisplayMask, (CGLPixelFormatAttribute)0x1, (CGLPixelFormatAttribute)0 };
        pixfmt("p", a1, "accelerated");
        pixfmt("p", a2, "offscreen");
        pixfmt("p", a3, "accel+double");
        pixfmt("p", a4, "robust");
        pixfmt("p", a5, "ALL_RENDERERS");
        pixfmt("p", a6, "ALL+accelerated");
        pixfmt("p", a7, "accel+double+mask1");
        pixfmt("p", a8, "double+mask1");
        break;
    }
    case 'q': {
        /* 18(b) discriminator: ONE set, ONE process — the engine
         * caches identical GLD consults, so which set produced the
         * trailer-only [0x4] consult is only separable per-process.
         * argv[2] = set number 1..8, same sets as mode p. */
        CGLPixelFormatAttribute sets[][6] = {
            { kCGLPFAAccelerated, 0 },
            { kCGLPFAOffScreen, 0 },
            { kCGLPFAAccelerated, kCGLPFADoubleBuffer, 0 },
            { kCGLPFARobust, 0 },
            { kCGLPFAAllRenderers, 0 },
            { kCGLPFAAllRenderers, kCGLPFAAccelerated, 0 },
            { kCGLPFAAccelerated, kCGLPFADoubleBuffer, kCGLPFADisplayMask, (CGLPixelFormatAttribute)0x1, 0 },
            { kCGLPFADoubleBuffer, kCGLPFADisplayMask, (CGLPixelFormatAttribute)0x1, 0 },
        };
        const char *names[] = { "accelerated", "offscreen", "accel+double",
                                "robust", "ALL_RENDERERS", "ALL+accelerated",
                                "accel+double+mask1", "double+mask1" };
        int which = (argc > 2) ? atoi(argv[2]) : 1;
        if (which < 1 || which > 8) { printf("bad set\n"); return 1; }
        census("q");
        printf("[q] about-to-call pf(%s)\n", names[which - 1]);
        fflush(stdout);
        CGLPixelFormatObj pf = NULL; GLint npix = 0;
        CGLError e = CGLChoosePixelFormat(sets[which - 1], &pf, &npix);
        printf("[q] pf(%s) -> %d npix=%d\n", names[which - 1], e, npix);
        if (pf) CGLDestroyPixelFormat(pf);
        break;
    }
    case 'k': {
        /* rung 27 (pre-registered, LEDGER fc99001): the context
         * rung — choose the honest {5,84,1} format; if it counts,
         * create a context; if created, ONE glGetString (the first
         * real GL call). Probe-side only; stub refusals stand. */
        CGLPixelFormatAttribute ak[] = {
            kCGLPFADoubleBuffer, kCGLPFADisplayMask,
            (CGLPixelFormatAttribute)0x1, (CGLPixelFormatAttribute)0 };
        CGLPixelFormatObj pf = NULL; GLint npix = 0;
        CGLError e = CGLChoosePixelFormat(ak, &pf, &npix);
        printf("[k] pf(double+mask1) -> %d npix=%d pf=%p\n", e, npix, (void*)pf);
        fflush(stdout);
        if (e == 0 && pf && npix > 0) {
            CGLContextObj ctx = NULL;
            CGLError ec = CGLCreateContext(pf, NULL, &ctx);
            printf("[k] CGLCreateContext -> %d ctx=%p\n", ec, (void*)ctx);
            fflush(stdout);
            if (ec == 0 && ctx) {
                CGLError em = CGLSetCurrentContext(ctx);
                printf("[k] CGLSetCurrentContext -> %d\n", em);
                fflush(stdout);
                const GLubyte *v = glGetString(GL_VERSION);
                printf("[k] glGetString(GL_VERSION) = %s\n", v ? (const char*)v : "(NULL)");
                const GLubyte *ve = glGetString(GL_VENDOR);
                printf("[k] glGetString(GL_VENDOR)  = %s\n", ve ? (const char*)ve : "(NULL)");
                const GLubyte *r = glGetString(GL_RENDERER);
                printf("[k] glGetString(GL_RENDERER) = %s\n", r ? (const char*)r : "(NULL)");
                fflush(stdout);
                /* rung 32 follow-up: the first DISPATCH-CLASS call —
                 * does gldInitDispatch fire here? */
                glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
                glClear(GL_COLOR_BUFFER_BIT);
                printf("[k] glClear done, glGetError = 0x%x\n", glGetError());
                fflush(stdout);
                CGLReleaseContext(ctx);
            }
            CGLDestroyPixelFormat(pf);
        }
        break;
    }
    default:
        printf("unknown mode\n");
        return 1;
    }
    printf("=== done ===\n");
    return 0;
}

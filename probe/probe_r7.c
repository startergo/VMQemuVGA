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
        pixfmt("p", a1, "accelerated");
        pixfmt("p", a2, "offscreen");
        pixfmt("p", a3, "accel+double");
        pixfmt("p", a4, "robust");
        pixfmt("p", a5, "ALL_RENDERERS");
        pixfmt("p", a6, "ALL+accelerated");
        break;
    }
    default:
        printf("unknown mode\n");
        return 1;
    }
    printf("=== done ===\n");
    return 0;
}

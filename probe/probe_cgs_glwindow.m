/*
 * probe_cgs_glwindow.m — outcome-2 DISCRIMINATOR (prediction committed
 * in LEDGER before this run, 2026-08-21): does the APPKIT GL attach
 * chain — NSOpenGLContext setView: → private __NS_CGL* wrappers → CGS
 * — trigger driver-side surface adoption under the vm-cap3d flip, where
 * the plain-window + direct-CGLSetSurface chain (probe_cgs_requester)
 * did not?
 *
 * This is the chain Gecko's swizzle suppresses (cgl_shim.mm
 * shim_setView: never calls the original). Standalone app: no
 * substitute loaded, no IOKit calls made — any VMAccelSurfaceClient /
 * newUserClient kernel.log line during this run is
 * WindowServer-originated by construction.
 *
 * Bonus datum: CGLGetSurface on the REAL context (legal here — a real
 * CGLContextObj, not a shim token) returns the drawable's
 * {surface id, type, w, h}.
 *
 * Every step prints a timestamped marker so kernel lines correlate to
 * the exact call.
 */

#import <Cocoa/Cocoa.h>
#import <OpenGL/OpenGL.h>
#import <OpenGL/gl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void mark(const char *what)
{
    char buf[64];
    time_t t = time(NULL);
    struct tm tmv;
    localtime_r(&t, &tmv);
    strftime(buf, sizeof(buf), "%H:%M:%S", &tmv);
    printf("[%s] %s\n", buf, what);
    fflush(stdout);
}

int main(int argc, char *argv[])
{
    int hold_secs = 20;
    if (argc > 1) hold_secs = atoi(argv[1]);

    printf("=== probe_cgs_glwindow — AppKit GL idiom discriminator ===\n");
    printf("No IOKit calls; kernel surface-client lines during this run\n");
    printf("are WindowServer-originated.\n\n");

    [NSApplication sharedApplication];

    mark("creating window");
    NSRect frame = NSMakeRect(240, 240, 320, 240);
    NSWindow *win = [[NSWindow alloc] initWithContentRect:frame
                                                styleMask:NSTitledWindowMask
                                                  backing:NSBackingStoreBuffered
                                                    defer:NO];
    [win setTitle:@"glwindow discriminator"];
    [win orderFrontRegardless];
    printf("window created: number=%d\n", (int)[win windowNumber]);

    mark("creating NSOpenGLContext (plain double-buffered)");
    NSOpenGLPixelFormatAttribute attrs[] = {
        NSOpenGLPFADoubleBuffer,
        (NSOpenGLPixelFormatAttribute)0
    };
    NSOpenGLPixelFormat *pf = [[NSOpenGLPixelFormat alloc]
                               initWithAttributes:attrs];
    if (!pf) {
        printf("FAIL: no pixel format\n");
        return 1;
    }
    NSOpenGLContext *ctx = [[NSOpenGLContext alloc]
                            initWithFormat:pf shareContext:nil];
    if (!ctx) {
        printf("FAIL: no context\n");
        return 1;
    }

    mark("setView: — THE APPKIT ATTACH CHAIN");
    [ctx setView:[win contentView]];
    mark("setView: returned");
    [ctx update];
    mark("update returned");

    [ctx makeCurrentContext];
    mark("makeCurrentContext returned");
    glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    mark("glClear returned");
    [ctx flushBuffer];
    mark("flushBuffer returned");

    /* CGLGetSurface bonus datum DROPPED (2026-08-21): not in the public
     * SDK headers, and its ABI on this system is unverified — the same
     * class of trap as CGLSetSurface's 6-vs-4-arg discrepancy the Aug-14
     * disassembly exposed. If adoption occurs, the kernel's SetIDMode
     * lines carry the surface ids; if it doesn't, the id is unobservable
     * here anyway. Not worth a mis-shaped call. */

    printf("\nHolding for %ds — watch kernel.log...\n", hold_secs);
    mark("HOLD START");
    fflush(stdout);
    sleep(hold_secs);
    mark("HOLD END");

    [ctx clearDrawable];
    [ctx release];
    [pf release];
    [win release];
    printf("=== done ===\n");
    return 0;
}

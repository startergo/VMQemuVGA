// probe_ga_plugin.c — the GA CFPlugIn milestone-1 negative control
//
// The exact call sequence readfb died on: instantiate the GA plugin from
// the FRAMEBUFFER service and drive its interface. PASS = IOCreatePlugIn
// InterfaceForService returns a live plugin, QueryInterface yields
// IOGraphicsAcceleratorInterface, Probe + Start succeed, and the kernel
// log shows the type-2 "2D context started" line (Start's IOServiceOpen).
//
// FAIL modes, pre-registered (docs/ga-cfplugin.md):
//   - factory never called          -> bundle not found: check the kext
//                                      PlugIns install + the IOCFPlugInTypes
//                                      property VALUE (bundle NAME string)
//   - Start fails at FindAccelerator-> FB trio wrong (should not happen —
//                                      verified in ioreg)
//   - Start fails at IOServiceOpen  -> type-2 user client not wired
//
// Build (cross, from the repo root):
//   clang -target x86_64-apple-macos10.6 \
//       -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.6.sdk \
//       -Wall -O2 -o probe_ga_plugin probe/probe_ga_plugin.c \
//       -framework IOKit -framework CoreFoundation

#include <stdio.h>
#include <stdlib.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/graphics/IOGraphicsInterface.h>

int main(void)
{
    /* The milestone-1 gate: the plugin refuses Start unless this env
     * var is set in ITS process — which is this one. */
    setenv("VM_GA_PROBE", "1", 1);

    kern_return_t kr;
    io_service_t fb;
    IOCFPlugInInterface** plug = NULL;
    void* ga = NULL;   /* the OBJECT (COM-style; deref for the vtable) */
    SInt32 score = 0;
    HRESULT qrc;

    // The framebuffer service CGS starts from.
    CFMutableDictionaryRef matching = IOServiceMatching("VMVirtIOFramebuffer");
    if (!matching) { printf("FAIL: IOServiceMatching\n"); return 1; }
    fb = IOServiceGetMatchingService(kIOMasterPortDefault, matching);
    if (fb == IO_OBJECT_NULL) { printf("FAIL: no VMVirtIOFramebuffer\n"); return 1; }
    printf("fb service = %u\n", (unsigned)fb);

    // Dump the FB's IOCFPlugInTypes property — the instantiation input.
    CFTypeRef plugTypes = IORegistryEntryCreateCFProperty(
        fb, CFSTR("IOCFPlugInTypes"), kCFAllocatorDefault, 0);
    if (!plugTypes) {
        printf("FAIL: FB has no IOCFPlugInTypes property\n");
        return 2;
    }
    CFShow(plugTypes);
    CFRelease(plugTypes);

    // THE call readfb died on.
    kr = IOCreatePlugInInterfaceForService(
        fb, kIOGraphicsAcceleratorTypeID, kIOGraphicsAcceleratorInterfaceID,
        &plug, &score);
    printf("IOCreatePlugInInterfaceForService -> 0x%x (score %d)\n", kr, (int)score);
    if (kr != KERN_SUCCESS || !plug) {
        printf("*** FAIL: plugin instantiation — bundle not found or "
               "factory refused ***\n");
        return 3;
    }

    qrc = (*plug)->QueryInterface(plug,
        CFUUIDGetUUIDBytes(kIOGraphicsAcceleratorInterfaceID),
        (LPVOID*)&ga);
    printf("QueryInterface -> 0x%lx, ga=%p\n", (unsigned long)qrc, (void*)ga);
    if (qrc != S_OK || !ga) {
        printf("*** FAIL: QueryInterface refused the GA interface ***\n");
        return 4;
    }

    /* COM shape (observed in the raw dump 2026-08-21): QueryInterface
     * returns the OBJECT — first field points at the vtable struct
     * (GAType._interface -> the plugin's static `ga`). Dereference once;
     * pass the OBJECT as thisPointer. Calling slots directly off the
     * object was the teardown segfault: misaligned reads, RIP=0. */
    IOGraphicsAcceleratorInterface* vt =
        *(IOGraphicsAcceleratorInterface***)ga;
    printf("vtable deref: object=%p vtable=%p\n", (void*)ga, (void*)vt);
    if (!vt) { printf("*** FAIL: object has no vtable pointer ***\n"); return 5; }

    IOReturn prc = vt->Probe(ga, NULL, fb, &score);
    printf("Probe -> 0x%x\n", prc);
    if (prc != kIOReturnSuccess) { printf("*** FAIL: Probe ***\n"); return 6; }

    prc = vt->Start(ga, NULL, fb);
    printf("Start -> 0x%x\n", prc);
    if (prc != kIOReturnSuccess) {
        printf("*** FAIL: Start (see mode above: FindAccelerator vs "
               "IOServiceOpen) ***\n");
        return 7;
    }

    printf("*** PASS — plugin instantiated, started, type-2 context open "
           "(verify kernel log: '2D context started') ***\n");
    fflush(stdout);

    fprintf(stderr, "probe: teardown: Stop\n");
    vt->Stop(ga);
    fprintf(stderr, "probe: teardown: Release(x2 — object + plugin handle)\n");
    vt->Release(ga);
    (*plug)->Release(plug);
    fprintf(stderr, "probe: teardown: IOObjectRelease(fb)\n");
    IOObjectRelease(fb);
    fprintf(stderr, "probe: teardown complete\n");
    return 0;
}

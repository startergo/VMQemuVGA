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

int main(int argc, char** argv)
{
    /* The milestone-1 gate: the plugin refuses Start unless this env
     * var is set in ITS process — which is this one. */
    setenv("VM_GA_PROBE", "1", 1);

    /* Milestone-2 rung 2: optional CGS surface id (argv[1]) — a real
     * wID from the kernel log's SetIDMode lines. With it, the probe
     * exercises Allocate/Lock/Unlock/Free; without it, that rung
     * skips. argv[2], if present, is a deliberate UNKNOWN id for the
     * negative control. */
    int have_id = (argc > 1 && argv[1][0]);
    unsigned long cgs_id = have_id ? strtoul(argv[1], NULL, 0) : 0;
    unsigned long bad_id = (argc > 2 && argv[2][0]) ? strtoul(argv[2], NULL, 0)
                                                    : 0xDEAD;

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

    /* USABILITY RUNG (milestone 2, the SetIDMode lesson): prove the
     * context USABLE, not merely open — a real destination binding
     * through the interface. SetDestination(fb, NULL) takes the safe
     * framebuffer path (no surface deref). */
    prc = vt->SetDestination(ga, kIOBlitFramebufferDestination, NULL);
    printf("SetDestination(fb) -> 0x%x\n", prc);
    fflush(stdout);
    if (prc != kIOReturnSuccess) {
        printf("*** context OPEN but NOT USABLE — first real 2D call "
               "refused ***\n");
        fflush(stdout);
    }

    if (have_id) {
        /* CGS-surface rung: Allocate(kIOBlitHasCGSSurface, id) binds
         * through the registry; Lock hands the app-task view. */
        IOBlitSurface surf;
        memset(&surf, 0, sizeof(surf));
        surf.pixelFormat = kIO32BGRAPixelFormat;
        surf.size.width = 0; surf.size.height = 0;   /* bound surface's */
        prc = vt->AllocateSurface(ga, kIOBlitHasCGSSurface, &surf,
                                  (void*)cgs_id);
        printf("AllocateSurface(cgsID=0x%lx) -> 0x%x\n", cgs_id, prc);
        fflush(stdout);
        if (prc == kIOReturnSuccess) {
            vm_address_t addr = 0;
            prc = vt->LockSurface(ga, 0, &surf, &addr);
            printf("LockSurface -> 0x%x addr=0x%lx rowBytes=%u\n",
                   prc, (unsigned long)addr, surf.rowBytes);
            fflush(stdout);
            if (prc == kIOReturnSuccess && addr) {
                /* Read the first pixel — the view is REAL if it reads. */
                volatile uint32_t* px = (volatile uint32_t*)addr;
                printf("LockSurface: first pixel = 0x%x\n", *px);
                fflush(stdout);
            }
            IOOptionBits swap = 0;
            prc = vt->UnlockSurface(ga, 0, &surf, &swap);
            printf("UnlockSurface -> 0x%x swap=0x%x\n", prc, swap);
            fflush(stdout);
            prc = vt->FreeSurface(ga, 0, &surf);
            printf("FreeSurface -> 0x%x\n", prc);
            fflush(stdout);
        }

        /* Negative control: an id that cannot be in the registry must
         * be REFUSED (NoResources) — a silent success here would be
         * an unbacked bind. */
        IOBlitSurface bad;
        memset(&bad, 0, sizeof(bad));
        bad.pixelFormat = kIO32BGRAPixelFormat;
        prc = vt->AllocateSurface(ga, kIOBlitHasCGSSurface, &bad,
                                  (void*)bad_id);
        printf("NEGATIVE AllocateSurface(unknown 0x%lx) -> 0x%x "
               "(expect 0xe00002bd NoResources)\n", bad_id, prc);
        fflush(stdout);
    } else {
        printf("(no cgs id argument — surface rung skipped)\n");
        fflush(stdout);
    }

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

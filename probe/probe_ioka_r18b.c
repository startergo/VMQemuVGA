/*
 * probe_ioka_r18b.c — rung 18(b): the direct IOAccelFindAccelerator
 * instrument. Pre-registered in LEDGER.md (commit d054b16) BEFORE this
 * file existed; predictions (i)/(ii)/(iii) live there and decide the
 * reading, not this header.
 *
 * SIGNATURES — CONFIRMED AT THE CALL SITES AND CALLEE BODY (the first
 * draft of this probe hypothesized CGSServiceForDisplayNumber as
 * 1-arg-returning-a-port; it segfaulted (kCGErrorIllegalArgument) —
 * the sixth instance of the signature-hypothesis class, this time by
 * not reading the call site first):
 *
 *   CGSServiceForDisplayNumber(CGDirectDisplayID, io_service_t *out)
 *       — OpenGL.framework x86_64 @0x4e0f: edi = display id from the
 *         per-display array, rsi = &out; testl %eax (CGError), jne
 *         skips the display. 2 args, status return.
 *
 *   IOAccelFindAccelerator(io_registry_entry_t displayService,
 *                          uint32_t *out1, uint32_t *out2)
 *       — callee body (IOKit @0xef03): both outs ZEROED ON ENTRY;
 *         makes its OWN master port (IOMasterPort @0xef41 — arg1 is
 *         NOT a master port, correcting the earlier reading);
 *         reads the display service's OWN properties
 *         (IORegistryEntryCreateCFProperties), then
 *         "IOAccelerator" (CFString) -> CFStringGetCString(Ptr)
 *         -> IORegistryEntryFromPath -> IOObjectConformsTo(
 *         "IOAccelerator") -> "IOAccelIndex" (CFNumber, SInt32)
 *         -> *out2. Returns 0 on success; 0xe00002bc
 *         (kIOReturnNotFound) when the string is absent, the path
 *         fails, or conformance fails. It is a VALIDATOR: the GLC
 *         caller stores only the index byte.
 *
 * Three prints, per the registration: the CGS-faithful node (class +
 * its OWN "IOAccelerator" property), the CGS-faithful call, and the
 * same call with the FB found by class matching.
 *
 * Output discipline: stdout UNBUFFERED (the first draft's buffered
 * stdout was eaten by the crash); run redirected to a file and read
 * back whole (no truncation pipes — the rung-17 SIGPIPE lesson).
 *
 * Build (readfb-era pattern, LEDGER):
 *   xcrun clang -arch x86_64 -mmacosx-version-min=10.6 \
 *     -isysroot <MacOSX10.6.sdk> probe_ioka_r18b.c \
 *     -framework IOKit -framework ApplicationServices \
 *     -framework CoreFoundation -o probe_ioka_r18b
 */

#include <stdio.h>
#include <stdint.h>
#include <mach/mach.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <ApplicationServices/ApplicationServices.h>

/* Confirmed at the call sites / callee body — see header comment. */
extern int CGSServiceForDisplayNumber(CGDirectDisplayID display,
                                      io_service_t *outService);
extern kern_return_t IOAccelFindAccelerator(
    io_registry_entry_t displayService, uint32_t *out1, uint32_t *out2);

static void describe_node(const char *tag, io_registry_entry_t node)
{
    if (!node) {
        printf("%s: node=0x0 (NULL)\n", tag);
        return;
    }
    io_name_t cls;
    kern_return_t kc = IOObjectGetClass(node, cls);
    printf("%s: node=0x%x class=%s (IOObjectGetClass->0x%x)\n",
           tag, (unsigned)node,
           kc == KERN_SUCCESS ? cls : "(class-fail)", kc);

    /* Mirror the target's read: the node's OWN properties only. */
    CFMutableDictionaryRef props = NULL;
    kern_return_t kr = IORegistryEntryCreateCFProperties(
        node, &props, kCFAllocatorDefault, 0);
    if (kr != KERN_SUCCESS || !props) {
        printf("  own properties: CreateCFProperties->0x%x props=%p\n",
               kr, (void *)props);
        return;
    }
    CFTypeRef v = CFDictionaryGetValue(props, CFSTR("IOAccelerator"));
    if (!v) {
        printf("  own \"IOAccelerator\": ABSENT\n");
    } else {
        CFTypeID t = CFGetTypeID(v);
        printf("  own \"IOAccelerator\": present, CFTypeID=%lu",
               (unsigned long)t);
        if (t == CFStringGetTypeID()) {
            char buf[256];
            if (CFStringGetCString(v, buf, sizeof(buf),
                                   kCFStringEncodingUTF8))
                printf(" value=\"%s\"", buf);
        } else if (t == CFDataGetTypeID()) {
            printf(" (CFData, %lu bytes)",
                   (unsigned long)CFDataGetLength(v));
        }
        printf("\n");
    }
    CFTypeRef ix = CFDictionaryGetValue(props, CFSTR("IOAccelIndex"));
    if (ix && CFGetTypeID(ix) == CFNumberGetTypeID()) {
        long n = -1;
        CFNumberGetValue(ix, kCFNumberLongType, &n);
        printf("  own \"IOAccelIndex\": %ld\n", n);
    } else {
        printf("  own \"IOAccelIndex\": %s\n",
               ix ? "(non-number)" : "ABSENT");
    }
    CFRelease(props);
}

static void try_find(const char *tag, io_registry_entry_t node)
{
    uint32_t out1 = 0xC0FFEE11;
    uint32_t out2 = 0xC0FFEE22;
    kern_return_t r = IOAccelFindAccelerator(node, &out1, &out2);
    printf("IOAccelFindAccelerator(%s) -> ret=0x%x out1=0x%x "
           "out2(IOAccelIndex)=0x%x (%u)\n",
           tag, r, out1, out2, out2);
    if (r != 0)
        printf("  0x%x == 0xe00002bc(kIOReturnNotFound)? %s\n",
               r, r == 0xe00002bc ? "YES" : "NO");
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== probe_ioka_r18b (rung 18b, direct instrument, "
           "signatures confirmed at call sites) ===\n");

    CGDirectDisplayID d = CGMainDisplayID();
    printf("CGMainDisplayID -> 0x%x\n", d);

    /* (1) The node CGS itself passes on the CGL path. */
    io_service_t cgsnode = MACH_PORT_NULL;
    int cg = CGSServiceForDisplayNumber(d, &cgsnode);
    printf("CGSServiceForDisplayNumber(0x%x) -> ret=%d "
           "service=0x%x\n", d, cg, (unsigned)cgsnode);
    describe_node("CGS node", cgsnode);

    /* (2) The CGS-faithful call — the rung-2 instrument. */
    if (cg == 0 && cgsnode)
        try_find("CGS node", cgsnode);
    else
        printf("IOAccelFindAccelerator(CGS node): SKIPPED "
               "(no service from CGS)\n");

    /* (3) The milestone-1 shape: the FB by class matching. */
    io_iterator_t it = MACH_PORT_NULL;
    io_service_t fb = MACH_PORT_NULL;
    kern_return_t kf = IOServiceGetMatchingServices(
        MACH_PORT_NULL, IOServiceMatching("VMVirtIOFramebuffer"), &it);
    printf("IOServiceGetMatchingServices(VMVirtIOFramebuffer) -> 0x%x\n",
           kf);
    if (kf == KERN_SUCCESS && it) {
        fb = IOIteratorNext(it);
        IOObjectRelease(it);
    }
    describe_node("FB node", fb);
    if (fb)
        try_find("FB node", fb);
    else
        printf("IOAccelFindAccelerator(FB node): SKIPPED "
               "(no FB service found)\n");

    printf("=== done ===\n");
    return 0;
}

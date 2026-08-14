/*
 * probe_accel_surface_mig.c — MIG boundary probe for VMAccelSurfaceClient
 *
 * Question: does a scalar-method call with non-default argument counts
 * cross the 10.6 IPC boundary and reach kernel code, when the user client
 * uses PURE OLD-STYLE dispatch (getTargetAndMethodForIndex table, no
 * externalMethod() override)?
 *
 * Pre-registered outcomes (2026-08-13, before the run):
 *   PASS   — kr != 0x10000003 AND kernel.log shows getTargetAndMethodForIndex
 *            + handler-entry lines. Old-style dispatch works; the note's
 *            catch-22 (externalMethod override suppressing metadata) is
 *            confirmed as the d98-era cause. Gate moves to the coupling
 *            probe (content in, WindowServer composites).
 *   FAIL   — kr == 0x10000003 (MIG_BAD_ARGUMENTS) with NO kernel log. The
 *            note's mechanism is falsified even for pure old-style; the
 *            no-receiver/alternative explanations become leading.
 *   MIXED  — some selectors pass, others reject. Per-selector count
 *            disagreement between our table and 10.6 userland; the
 *            SetIDMode=2/SetShape=2 entries are the suspects.
 *
 * Evidence status: notes/IOACCELSURFACE_SNOW_LEOPARD_ISSUE.md describes a
 * November 2025 binary this project never compiled (file absent from
 * project.pbxproj across all git history; source pristine since ff9f3d8).
 * This probe attributes its result to TODAY'S build only.
 *
 * Requires boot-arg vm-accel-surface=1 (newUserClient type-0 gate).
 * Handler returns are kIOReturnUnsupported by design — the probe's pass
 * condition is kernel-reach, not handler success. GetState is the
 * known-good control (0-in/1-out).
 *
 * Cross-compile against the 10.6 SDK, x86_64.
 */

#include <stdio.h>
#include <stdint.h>
#include <IOKit/IOKitLib.h>
#include <CoreFoundation/CoreFoundation.h>
#include <mach/mach_error.h>

/* Selector indices from IOKit/graphics/IOAccelSurfaceConnect.h (10.6 SDK).
 * Self-defined so the probe needs no private header at compile time.
 * Order verified against the SDK enum 2026-08-13. */
enum {
    kSelReadLockOptions   = 0,
    kSelReadUnlockOptions = 1,
    kSelGetState          = 2,
    kSelWriteLockOptions  = 3,
    kSelWriteUnlockOptions= 4,
    kSelFlush             = 10,
    kSelReadLock          = 12,
};

static io_connect_t open_surface_client(void)
{
    /* Try the class names this kext has published under, then a
     * property-based match on IOAcceleratorClassName as fallback. */
    const char *names[] = { "VMQemuVGAAccelerator",
                            "VMVirtIOGPUAccelerator", NULL };
    for (int i = 0; names[i]; i++) {
        io_service_t svc = IOServiceGetMatchingService(
            kIOMasterPortDefault, IOServiceMatching(names[i]));
        if (!svc)
            continue;
        printf("found service via IOClass %s\n", names[i]);
        io_connect_t conn;
        kern_return_t kr = IOServiceOpen(svc, mach_task_self(),
                                         0 /* type 0 = surface client */,
                                         &conn);
        IOObjectRelease(svc);
        if (kr == KERN_SUCCESS) {
            printf("IOServiceOpen(type 0) OK conn=0x%x\n", conn);
            return conn;
        }
        printf("IOServiceOpen(type 0) failed kr=0x%x (%s)\n",
               kr, mach_error_string(kr));
        return MACH_PORT_NULL;
    }

    /* Fallback: match on the published property. */
    CFDictionaryRef match = IOServiceMatching("IOAccelerator");
    if (match) {
        CFDictionarySetValue((CFMutableDictionaryRef)match,
                             CFSTR("IOAcceleratorClassName"),
                             CFSTR("VMVirtIOGPUAccelerator"));
        io_iterator_t it;
        kern_return_t kr = IOServiceGetMatchingServices(
            kIOMasterPortDefault, match, &it);
        if (kr == KERN_SUCCESS) {
            io_service_t svc;
            io_connect_t conn = MACH_PORT_NULL;
            while ((svc = IOIteratorNext(it))) {
                printf("fallback: candidate service\n");
                kr = IOServiceOpen(svc, mach_task_self(), 0, &conn);
                IOObjectRelease(svc);
                if (kr == KERN_SUCCESS) {
                    printf("fallback: IOServiceOpen(type 0) OK\n");
                    IOObjectRelease(it);
                    return conn;
                }
                printf("fallback: IOServiceOpen failed kr=0x%x\n", kr);
            }
            IOObjectRelease(it);
        }
    }
    return MACH_PORT_NULL;
}

static void try_sel(const char *name, io_connect_t conn, uint32_t sel,
                    const uint64_t *in, uint32_t nIn,
                    uint64_t *out, uint32_t *nOut)
{
    kern_return_t kr = IOConnectCallScalarMethod(conn, sel, in, nIn,
                                                 out, nOut);
    printf("%-20s sel=%2u in=%u out=%u kr=0x%x (%s)",
           name, sel, nIn, nOut ? *nOut : 0, kr, mach_error_string(kr));
    if (kr == 0x10000003)
        printf("  <-- MIG_BAD_ARGUMENTS: rejected at IPC boundary");
    if (out && nOut && *nOut > 0 && kr == KERN_SUCCESS)
        printf(" out[0]=0x%llx", (unsigned long long)out[0]);
    printf("\n");
}

int main(void)
{
    printf("=== probe_accel_surface_mig (2026-08-13) ===\n");
    printf("Requires boot-arg vm-accel-surface=1 and kext with "
           "VMAccelSurfaceClient compiled in.\n\n");

    io_connect_t conn = open_surface_client();
    if (conn == MACH_PORT_NULL) {
        printf("\nFAIL(open): no surface client. Either the service was "
               "not found or type 0 returned Unsupported (boot-arg gate "
               "off, or kext predates this probe).\n");
        return 1;
    }

    printf("\n--- MIG boundary calls ---\n");
    uint64_t out[1] = { 0xDEADBEEFDEADBEEFULL };

    /* Control: 0-in/1-out, the signature that worked in every era. */
    uint32_t nOut1 = 1;
    try_sel("GetState(ctrl)", conn, kSelGetState, NULL, 0, out, &nOut1);

    /* The previously-failing signatures. */
    uint64_t opt = 0;
    try_sel("WriteLockOptions", conn, kSelWriteLockOptions, &opt, 1, NULL, 0);
    try_sel("WriteUnlockOpt", conn, kSelWriteUnlockOptions, &opt, 1, NULL, 0);
    try_sel("ReadLockOptions", conn, kSelReadLockOptions, &opt, 1, NULL, 0);
    try_sel("Flush(0/0)", conn, kSelFlush, NULL, 0, NULL, 0);
    try_sel("ReadLock(0/0)", conn, kSelReadLock, NULL, 0, NULL, 0);

    /* The WindowServer-captured counts (2-in/0-out) — the entries most
     * likely to discriminate a MIXED outcome: if these pass while the
     * 1-in locks fail, the table is partly right and the fix is
     * per-entry, not architectural. */
    uint64_t two[2] = { 0, 0 };
    try_sel("SetIDMode(2/0)", conn, 7, two, 2, NULL, 0);
    try_sel("SetShape(2/0)", conn, 9, two, 2, NULL, 0);

    IOServiceClose(conn);
    printf("\nDone. Verdict rule: kr != 0x10000003 on the non-control "
           "calls AND matching kernel.log handler lines => PASS.\n");
    return 0;
}

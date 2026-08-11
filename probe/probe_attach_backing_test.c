// probe_attach_backing_test.c
//
// Userspace driver for the ATTACH_BACKING-with-userspace-memory probe
// (kext selector 0x5000 on VMVirtIOGPUUserClient). Cross-compile for
// x86_64-apple-macos10.6 and run on the SL guest:
//
//   clang -target x86_64-apple-macos10.6 \
//       -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.6.sdk \
//       -o probe_attach_backing_test probe_attach_backing_test.c \
//       -framework IOKit -framework CoreFoundation
//
// What this tests (LEDGER.md:769):
//   Whether IOMemoryDescriptor::withAddressRange + persistent prepare()
//   works on 10.6 for userspace malloc'd memory. If yes, the entire
//   IOKit winsys is bookkeeping on proven transport. If no, the model
//   needs kext-allocated backing via clientMemoryForType.
//
// Position-dependent pattern + per-dword verification means a wrong
// middle segment names itself by index. The buffer is deliberately
// misaligned (offset 17 from malloc base) so the first physical
// segment is a partial page; if the allocator hands back contiguous
// page-aligned memory by luck, nr_entries will be 1 and the per-segment
// log in the kernel will flag it.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <mach/mach.h>
#include <IOKit/IOKitLib.h>

#define PROBE_SELECTOR       0x5000
#define PROBE_BUF_SIZE       16384      // 64 * 64 * 4 — matches kext PROBE_W*PROBE_H*4
#define PROBE_ALLOC_SIZE     (PROBE_BUF_SIZE + 128)
#define PROBE_OFFSET         17         // non-page-aligned, non-dword-aligned
#define PATTERN_XOR          0xA5A5A5A5u
#define NEG_CONTROL_BYTE     0xCD       // matches probeTransport3D's convention

int main(int argc, char** argv)
{
    (void)argc; (void)argv;  // no CLI args yet
    kern_return_t  kr;
    io_service_t   service  = IO_OBJECT_NULL;
    io_connect_t   connect  = IO_OBJECT_NULL;
    uint8_t*       base     = NULL;
    int            exit_code = 0;

    // --- Find VMQemuVGAAccelerator in IORegistry -----------------------
    // ioreg shows the published service is the BASE class VMQemuVGAAccelerator
    // (VMVirtIOGPUAccelerator subclass exists in code but registers 0 instances
    // on this configuration). Base class's newUserClient at VMQemuVGAAccelerator.cpp:344
    // handles type=4 → returns VMVirtIOGPUUserClient, which has the probe selector.
    CFMutableDictionaryRef matching = IOServiceMatching("VMQemuVGAAccelerator");
    if (!matching) {
        fprintf(stderr, "FAIL: IOServiceMatching returned NULL\n");
        return 1;
    }
    service = IOServiceGetMatchingService(kIOMasterPortDefault, matching);
    // matching is consumed by IOServiceGetMatchingService on success.
    if (service == IO_OBJECT_NULL) {
        fprintf(stderr, "FAIL: VMQemuVGAAccelerator not found in IORegistry.\n");
        fprintf(stderr, "      Is the VMQemuVGA.kext loaded? (run: kextstat | grep VMQemu)\n");
        return 1;
    }
    printf("found VMQemuVGAAccelerator: service=0x%x\n", service);

    // --- Open VMVirtIOGPUUserClient (type 4) ---------------------------
    kr = IOServiceOpen(service, mach_task_self(), 4, &connect);
    IOObjectRelease(service);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "FAIL: IOServiceOpen(type=4) returned 0x%x (%s)\n",
                kr, mach_error_string(kr));
        return 1;
    }
    printf("opened VMVirtIOGPUUserClient: connect=0x%x\n", connect);

    // --- Allocate deliberately misaligned buffer -----------------------
    // macOS malloc(>=16KB) typically routes through the large allocator
    // and returns page-aligned mmap'd memory. To force a partial first
    // page (and real multi-segment scatter list), allocate len + 128
    // and probe at offset 17. Keep base for free().
    base = (uint8_t*)malloc(PROBE_ALLOC_SIZE);
    if (!base) {
        fprintf(stderr, "FAIL: malloc(%d) failed\n", PROBE_ALLOC_SIZE);
        IOServiceClose(connect);
        return 1;
    }
    uint8_t*  buf = base + PROBE_OFFSET;
    uint32_t* px  = (uint32_t*)buf;  // x86_64 tolerates unaligned dword access
    uint32_t  px_count = PROBE_BUF_SIZE / sizeof(uint32_t);  // 4096 dwords

    printf("allocated base=%p buf=%p (offset %d) — %u dwords\n",
           base, buf, PROBE_OFFSET, px_count);

    // --- Fill position-dependent pattern -------------------------------
    // px[i] = i ^ 0xA5A5A5A5. A uniform memset pattern can't detect a
    // wrong middle segment — that's exactly the failure mode a multi-
    // segment scatter list introduces. With position-dependent fill,
    // the first mismatching dword's index names the failing segment.
    // LEDGER.md:911 correction.
    for (uint32_t i = 0; i < px_count; i++) {
        px[i] = i ^ PATTERN_XOR;
    }

    // --- Phase 1: attach backing + transfer_to_host_3d -----------------
    uint64_t addr = (uint64_t)(uintptr_t)buf;
    uint64_t len  = PROBE_BUF_SIZE;
    uint64_t scalars[5] = {
        1,                                  // phase
        (uint32_t)(addr & 0xFFFFFFFFull),    // addr_lo
        (uint32_t)(addr >> 32),              // addr_hi
        (uint32_t)(len  & 0xFFFFFFFFull),    // len_lo
        (uint32_t)(len  >> 32),              // len_hi
    };

    printf("phase 1: addr=0x%llx len=%llu — attach backing + TRANSFER_TO_HOST_3D\n",
           addr, len);
    kr = IOConnectCallMethod(connect, PROBE_SELECTOR,
                             scalars, 5,
                             NULL, 0,           // no structure input
                             NULL, NULL,        // no scalar output
                             NULL, NULL);       // no structure output
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "FAIL: phase 1 returned 0x%x (%s)\n",
                kr, mach_error_string(kr));
        fprintf(stderr, "     Check kernel log for 'probeAttachBacking: FAIL' lines.\n");
        exit_code = 1;
        goto cleanup;
    }
    printf("phase 1 ok — kext holds descriptor prepared across calls.\n");

    // --- Negative control: zero buf as 0xCD marker ---------------------
    // After Phase 2, every dword should be the pattern, NOT 0xCDCDCDCD.
    // 0xCDCDCDCD in the readback means host wrote nothing.
    printf("zeroing buf with 0x%02x as negative-control marker.\n", NEG_CONTROL_BYTE);
    memset(buf, NEG_CONTROL_BYTE, PROBE_BUF_SIZE);

    // --- Phase 2: transfer_from_host_3d + teardown ---------------------
    scalars[0] = 2;  // phase
    scalars[1] = scalars[2] = scalars[3] = scalars[4] = 0;  // ignored

    printf("phase 2: TRANSFER_FROM_HOST_3D + teardown\n");
    kr = IOConnectCallMethod(connect, PROBE_SELECTOR,
                             scalars, 5,
                             NULL, 0,
                             NULL, NULL,
                             NULL, NULL);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "WARN: phase 2 returned 0x%x (%s) — bytes may be wrong.\n",
                kr, mach_error_string(kr));
        // Continue to verification — the readback is the real signal.
    } else {
        printf("phase 2 ok.\n");
    }

    // --- Verify every dword, report first mismatch ---------------------
    uint32_t mismatch_count = 0;
    uint32_t first_mm_idx      = 0xFFFFFFFFu;
    uint32_t first_mm_actual   = 0;
    uint32_t first_mm_expected = 0;
    // Also track byte-offset of the boundary between "all good so far"
    // and "first wrong" — useful for mapping to scatter-list segment.
    for (uint32_t i = 0; i < px_count; i++) {
        uint32_t expected = i ^ PATTERN_XOR;
        if (px[i] != expected) {
            if (first_mm_idx == 0xFFFFFFFFu) {
                first_mm_idx      = i;
                first_mm_actual   = px[i];
                first_mm_expected = expected;
            }
            mismatch_count++;
        }
    }

    printf("\n");
    if (mismatch_count == 0) {
        printf("*** PROBE PASS — %u/%u dwords match. withAddressRange +\n", px_count, px_count);
        printf("    persistent prepare() works on 10.6 for userspace malloc'd memory.\n");
        printf("    Winsys = bookkeeping on proven transport. ***\n");
        exit_code = 0;
    } else {
        printf("*** PROBE FAIL — %u/%u dwords mismatch.\n", mismatch_count, px_count);
        printf("    First mismatch: index %u (byte offset %u, page %u)\n",
               first_mm_idx,
               first_mm_idx * (uint32_t)sizeof(uint32_t),
               (first_mm_idx * (uint32_t)sizeof(uint32_t)) / 4096u);
        printf("                     got 0x%08x expected 0x%08x\n",
               first_mm_actual, first_mm_expected);

        // Failure-pattern bucketing — pre-registered predictions (LEDGER.md:825).
        if (first_mm_actual == 0xCDCDCDCDu && mismatch_count == px_count) {
            printf("    Pattern: every dword is 0xCDCDCDCD.\n");
            printf("    Cause:   host wrote zero bytes. Either scatter list wrong, or\n");
            printf("             withAddressRange + prepare() didn't wire pages.\n");
            printf("    Next:    check Phase 1 'nr_entries' + per-segment (addr, length)\n");
            printf("             in kernel log; check prepare() return value.\n");
        } else if (first_mm_actual == 0xCDCDCDCDu) {
            printf("    Pattern: 0xCDCDCDCD in some dwords (others correct).\n");
            printf("    Cause:   partial corruption — some scatter-list entries pointed\n");
            printf("             at stale physical addresses. Wiring isn't holding across\n");
            printf("             Phase 1 -> Phase 2 for those pages.\n");
            printf("    Next:    pivot to clientMemoryForType/IOConnectMapMemory (kext-\n");
            printf("             allocated backing, already-wired kernel memory).\n");
        } else if (first_mm_idx == 0) {
            printf("    Pattern: very first dword wrong (not 0xCDCDCDCD).\n");
            printf("    Cause:   host wrote *different* bytes than expected.\n");
            printf("    Next:    check kext PROBE_FMT, PROBE_W, PROBE_H match host's idea\n");
            printf("             of the resource; check Phase 1 ATTACH_BACKING response.\n");
        } else {
            printf("    Pattern: corruption starts mid-buffer.\n");
            printf("    Cause:   first page mapped correctly, later pages wrong. Likely\n");
            printf("             a segment-walk bug or partial wiring.\n");
            printf("    Next:    map first_mm_idx * 4 to the scatter-list segment log in\n");
            printf("             the kernel to find which entry is wrong.\n");
        }
        printf(" ***\n");
        exit_code = 2;
    }

cleanup:
    if (base) free(base);
    if (connect != IO_OBJECT_NULL) {
        // Closing also fires probeAttachBackingUserCleanup() in the kext —
        // any held descriptor is released even if we crashed between phases.
        IOServiceClose(connect);
    }
    return exit_code;
}

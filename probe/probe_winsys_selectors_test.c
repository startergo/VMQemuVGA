// probe_winsys_selectors_test.c
//
// Userspace driver for the virgl_iokit_winsys kext selectors (0x6000 range).
// Cross-compile for x86_64-apple-macos10.6 and run on the SL guest:
//
//   clang -target x86_64-apple-macos10.6 \
//       -isysroot .../MacOSX10.6.sdk \
//       -Wall -O2 \
//       -o probe_winsys_selectors_test probe_winsys_selectors_test.c \
//       -framework IOKit -framework CoreFoundation
//
// Re-implements probeTransport3D's clear+readback entirely from userspace
// via the new 0x6000-range kext selectors — no Mesa, no kext-internal probe.
// Tests the same primitives the virgl_iokit_winsys will use.
//
// PASS = every dword matches the clear color's unorm encoding after
// TRANSFER_FROM_HOST_3D. The position-dependent pre-fill makes
// "host wrote nothing" (buffer stays at i ^ 0xA5A5A5A5) distinguishable
// from "host wrote zeros" (buffer all zero) from "host wrote right color"
// (buffer all 0xff996633 or 0xff6633cc).

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <mach/mach.h>
#include <IOKit/IOKitLib.h>

// --- kext selector numbers (match FB/VMVirtIOGPU.cpp externalMethod switch) ---
#define SEL_CREATE_CTX          0x6000
#define SEL_DESTROY_CTX         0x6001
#define SEL_CREATE_RES_3D       0x6002
#define SEL_ATTACH_BACKING      0x6003
#define SEL_DETACH_BACKING      0x6004
#define SEL_RESOURCE_UNREF      0x6005
#define SEL_GET_CAPSET_INFO     0x6006
#define SEL_GET_CAPSET          0x6007
#define SEL_SUBMIT_CMDS_EX      0x6008
#define SEL_CTX_ATTACH_RESOURCE 0x6009
#define SEL_TRANSFER_FROM_3D    0x3009

// --- virgl constants (from FB/virgl_protocol.h, inlined for cross-compile) ---
#define VIRGL_TARGET_2D                 2
#define VIRGL_FORMAT_R8G8B8A8_UNORM     67
#define VIRGL_BIND_RENDER_TARGET        (1 << 1)
#define VIRGL_CMD0(cmd, obj, len)      ((cmd) | ((obj) << 8) | ((len) << 16))
#define VIRGL_CCMD_CREATE_OBJECT        1
#define VIRGL_CCMD_SET_FRAMEBUFFER_STATE 5
#define VIRGL_CCMD_CLEAR                7
#define VIRGL_CCMD_NOP                  0
#define VIRGL_OBJECT_SURFACE            8     // enum position in virgl_object_type (NULL=0, BLEND=1, ..., SURFACE=8)
#define VIRGL_OBJ_SURFACE_SIZE          5
#define VIRGL_OBJ_SURFACE_HANDLE        1
#define VIRGL_OBJ_SURFACE_RES_HANDLE    2
#define VIRGL_OBJ_SURFACE_FORMAT        3
#define VIRGL_OBJ_SURFACE_TEXTURE_LEVEL 4
#define VIRGL_OBJ_SURFACE_TEXTURE_LAYERS 5
#define VIRGL_OBJ_CLEAR_SIZE            8
#define PIPE_CLEAR_COLOR0               0x04

// Probe constants
#define PROBE_W             64
#define PROBE_H             64
#define PROBE_BUF_SIZE      (PROBE_W * PROBE_H * 4)   // 16384 bytes
#define PROBE_ALLOC_SIZE    (PROBE_BUF_SIZE + 128)
#define PROBE_OFFSET        17                          // unaligned, multi-segment
#define PROBE_SURF_HANDLE   1                           // virgl object handle for the surface

// Non-boundary clear colors (LEDGER convention — avoid .5 ULP rounding):
// (0.20, 0.40, 0.60, 1.00) → (51, 102, 153, 255) packed RGBA = 0xff996633
// (0.80, 0.20, 0.40, 1.00) → (204, 51, 102, 255) packed RGBA = 0xff6633cc
static const float CLEAR1_RGBA[4] = { 0.20f, 0.40f, 0.60f, 1.00f };
static const float CLEAR2_RGBA[4] = { 0.80f, 0.20f, 0.40f, 1.00f };
#define EXPECTED_COLOR1_DWORD   0xff996633u   // R=51 G=102 B=153 A=255 little-endian
#define EXPECTED_COLOR2_DWORD   0xff6633ccu   // R=204 G=51 B=102 A=255 little-endian

static inline uint32_t virgl_pack_float(float f) {
    uint32_t u;
    memcpy(&u, &f, 4);
    return u;
}

// Reproduce the original pattern fill at idx — used for "host didn't touch
// this dword" detection in the failure diagnostic.
static inline uint32_t expected_pattern_at(uint32_t idx) {
    return idx ^ 0xA5A5A5A5u;
}

// Helper: call a kext selector with scalar inputs only.
static kern_return_t call_scalar_in(io_connect_t c, uint32_t sel,
                                     const uint64_t* in, uint32_t n_in)
{
    return IOConnectCallMethod(c, sel,
                               in, n_in,
                               NULL, 0,
                               NULL, NULL, NULL, NULL);
}

// Helper: call a kext selector with scalar in + scalar out.
// 10.6 SDK's IOConnectCallMethod takes outputCnt as uint32_t* (in/out),
// unlike newer macOS where it's a value. Treat as in: caller's n_out is
// capacity; we don't read back the actual count.
static kern_return_t call_scalar_in_out(io_connect_t c, uint32_t sel,
                                         const uint64_t* in, uint32_t n_in,
                                         uint64_t* out, uint32_t n_out)
{
    uint32_t out_cap = n_out;
    return IOConnectCallMethod(c, sel,
                               in, n_in,
                               NULL, 0,
                               out, &out_cap,
                               NULL, NULL);
}

int main(int argc, char** argv)
{
    (void)argc; (void)argv;
    kern_return_t  kr;
    io_service_t   service  = IO_OBJECT_NULL;
    io_connect_t   connect  = IO_OBJECT_NULL;
    uint8_t*       base     = NULL;
    int            exit_code = 0;
    uint32_t       ctx_id = 0, resource_id = 0;

    // --- Find VMQemuVGAAccelerator (base class registers; subclass doesn't) ---
    CFMutableDictionaryRef matching = IOServiceMatching("VMQemuVGAAccelerator");
    if (!matching) { fprintf(stderr, "FAIL: IOServiceMatching NULL\n"); return 1; }
    service = IOServiceGetMatchingService(kIOMasterPortDefault, matching);
    if (service == IO_OBJECT_NULL) {
        fprintf(stderr, "FAIL: VMQemuVGAAccelerator not found (kext loaded?)\n");
        return 1;
    }
    printf("found service=0x%x\n", service);

    // type=4 → VMVirtIOGPUUserClient (see VMQemuVGAAccelerator.cpp:425)
    kr = IOServiceOpen(service, mach_task_self(), 4, &connect);
    IOObjectRelease(service);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "FAIL: IOServiceOpen type=4 0x%x (%s)\n", kr, mach_error_string(kr));
        return 1;
    }
    printf("opened connect=0x%x\n", connect);

    // --- 0x6000 createVirglContextEx ---
    {
        uint64_t out[1] = {0};
        kr = call_scalar_in_out(connect, SEL_CREATE_CTX, NULL, 0, out, 1);
        if (kr != KERN_SUCCESS) {
            fprintf(stderr, "FAIL 0x6000 createVirglContextEx: 0x%x\n", kr);
            exit_code = 1; goto cleanup;
        }
        ctx_id = (uint32_t)out[0];
        printf("0x6000 createVirglContextEx -> ctx_id=0x%x\n", ctx_id);
    }

    // --- 0x6006 getCapsetInfo(0) ---
    {
        uint64_t in[1] = { 0 };  // capset_index
        uint64_t out[3] = {0};
        kr = call_scalar_in_out(connect, SEL_GET_CAPSET_INFO, in, 1, out, 3);
        if (kr != KERN_SUCCESS) {
            fprintf(stderr, "WARN 0x6006 getCapsetInfo: 0x%x (continuing)\n", kr);
        } else {
            printf("0x6006 getCapsetInfo(0) -> id=%llu version=%llu size=%llu\n",
                   out[0], out[1], out[2]);

            // --- 0x6007 getCapset(id, version) ---
            uint8_t blob[2048];
            size_t blob_size = sizeof(blob);
            uint64_t cap_in[2] = { out[0], out[1] };
            kr = IOConnectCallMethod(connect, SEL_GET_CAPSET,
                                     cap_in, 2,
                                     NULL, 0,
                                     NULL, NULL,
                                     blob, &blob_size);
            if (kr != KERN_SUCCESS) {
                fprintf(stderr, "WARN 0x6007 getCapset: 0x%x (continuing)\n", kr);
            } else {
                printf("0x6007 getCapset(id=%llu,ver=%llu) -> %zu bytes, first 16: ",
                       out[0], out[1], blob_size);
                for (int i = 0; i < 16 && i < (int)blob_size; i++) {
                    printf("%02x ", blob[i]);
                }
                printf("\n");
            }
        }
    }

    // --- 0x6002 createResource3DEx (11 scalars: ctx_id + 10 fields) ---
    {
        uint64_t in[11] = {
            ctx_id,                         // ctx_id (probeTransport3D sets hdr.ctx_id explicitly)
            VIRGL_TARGET_2D,               // target
            VIRGL_FORMAT_R8G8B8A8_UNORM,   // format
            VIRGL_BIND_RENDER_TARGET,      // bind
            PROBE_W, PROBE_H,              // width, height
            1,                              // depth
            1,                              // array_size
            0,                              // last_level
            0,                              // nr_samples
            0                               // flags
        };
        uint64_t out[1] = {0};
        kr = call_scalar_in_out(connect, SEL_CREATE_RES_3D, in, 11, out, 1);
        if (kr != KERN_SUCCESS) {
            fprintf(stderr, "FAIL 0x6002 createResource3DEx: 0x%x\n", kr);
            exit_code = 1; goto cleanup;
        }
        resource_id = (uint32_t)out[0];
        printf("0x6002 createResource3DEx -> resource_id=0x%x\n", resource_id);
    }

    // --- malloc + fill position-dependent pattern ---
    base = (uint8_t*)malloc(PROBE_ALLOC_SIZE);
    if (!base) {
        fprintf(stderr, "FAIL malloc(%d)\n", PROBE_ALLOC_SIZE);
        exit_code = 1; goto cleanup;
    }
    uint8_t* buf = base + PROBE_OFFSET;
    uint32_t* px = (uint32_t*)buf;
    uint32_t px_count = PROBE_BUF_SIZE / 4;
    for (uint32_t i = 0; i < px_count; i++) {
        px[i] = i ^ 0xA5A5A5A5u;
    }
    printf("allocated base=%p buf=%p (offset %d) %u dwords, filled pattern\n",
           base, buf, PROBE_OFFSET, px_count);

    // --- 0x6003 attachBackingUser ---
    {
        uint64_t addr = (uint64_t)(uintptr_t)buf;
        uint64_t len  = PROBE_BUF_SIZE;
        uint64_t in[5] = {
            resource_id,
            (uint32_t)(addr & 0xFFFFFFFFull),
            (uint32_t)(addr >> 32),
            (uint32_t)(len  & 0xFFFFFFFFull),
            (uint32_t)(len  >> 32)
        };
        kr = call_scalar_in(connect, SEL_ATTACH_BACKING, in, 5);
        if (kr != KERN_SUCCESS) {
            fprintf(stderr, "FAIL 0x6003 attachBackingUser: 0x%x\n", kr);
            exit_code = 1; goto cleanup;
        }
        printf("0x6003 attachBackingUser ok (descriptor held)\n");
    }

    // --- 0x6009 ctxAttachResource (binds resource to context) ---
    // Required before SET_FRAMEBUFFER_STATE can reference a surface built
    // on this resource — the legacy 0x3003 is a stub, this one actually
    // sends the command. Mirrors probeTransport3D Phase E.
    {
        uint64_t in[2] = { ctx_id, resource_id };
        kr = call_scalar_in(connect, SEL_CTX_ATTACH_RESOURCE, in, 2);
        if (kr != KERN_SUCCESS) {
            fprintf(stderr, "WARN 0x6009 ctxAttachResource: 0x%x (continuing)\n", kr);
        } else {
            printf("0x6009 ctxAttachResource(ctx=0x%x, res=0x%x) ok\n",
                   ctx_id, resource_id);
        }
    }

    // --- Helper to build CLEAR command buffer and submit via 0x6008 ---
    // Shape: CREATE_OBJECT(SURFACE) + SET_FRAMEBUFFER_STATE + CLEAR + NOP
    // = 6 + 4 + 9 + 1 = 20 dwords
    //
    // CREATE_OBJECT references the surface handle (PROBE_SURF_HANDLE=1),
    // NOT the raw resource id. SET_FRAMEBUFFER_STATE then binds the
    // surface handle as cbuf[0]. This is what probeTransport3D's phase F
    // (VMVirtIOGPU.cpp:3855-3878) + phase G (3942-3957) does.
    for (int round = 0; round < 2; round++) {
        const float* rgba = (round == 0) ? CLEAR1_RGBA : CLEAR2_RGBA;
        uint32_t expected = (round == 0) ? EXPECTED_COLOR1_DWORD : EXPECTED_COLOR2_DWORD;

        // Build command buffer.
        uint32_t cmdbuf[20];
        unsigned idx = 0;

        // CREATE_OBJECT (6 dwords): handle=1, res_handle=resource_id, format, etc.
        cmdbuf[idx++] = VIRGL_CMD0(VIRGL_CCMD_CREATE_OBJECT, VIRGL_OBJECT_SURFACE, VIRGL_OBJ_SURFACE_SIZE);
        cmdbuf[idx++] = PROBE_SURF_HANDLE;             // surface handle (caller-chosen)
        cmdbuf[idx++] = resource_id;                    // res_handle (kext-allocated)
        cmdbuf[idx++] = VIRGL_FORMAT_R8G8B8A8_UNORM;    // format
        cmdbuf[idx++] = 0;                              // texture_level
        cmdbuf[idx++] = 0;                              // texture_layers

        // SET_FRAMEBUFFER_STATE (4 dwords): nr_cbufs=1, zsurf=0, cbuf[0]=surface handle
        cmdbuf[idx++] = VIRGL_CMD0(VIRGL_CCMD_SET_FRAMEBUFFER_STATE, 0, 3);
        cmdbuf[idx++] = 1;                              // nr_cbufs
        cmdbuf[idx++] = 0;                              // zsurf_handle
        cmdbuf[idx++] = PROBE_SURF_HANDLE;              // cbuf[0] = surface handle

        // CLEAR (9 dwords): PIPE_CLEAR_COLOR0 + RGBA packed floats + depth/stencil
        cmdbuf[idx++] = VIRGL_CMD0(VIRGL_CCMD_CLEAR, 0, VIRGL_OBJ_CLEAR_SIZE);
        cmdbuf[idx++] = PIPE_CLEAR_COLOR0;
        cmdbuf[idx++] = virgl_pack_float(rgba[0]);
        cmdbuf[idx++] = virgl_pack_float(rgba[1]);
        cmdbuf[idx++] = virgl_pack_float(rgba[2]);
        cmdbuf[idx++] = virgl_pack_float(rgba[3]);
        cmdbuf[idx++] = 0;  // depth lo
        cmdbuf[idx++] = 0;  // depth hi
        cmdbuf[idx++] = 0;  // stencil

        // NOP (1 dword)
        cmdbuf[idx++] = VIRGL_CMD0(VIRGL_CCMD_NOP, 0, 0);

        // Submit via 0x6008 with ctx_id as scalar + command buffer as struct input.
        // IOConnectCallMethod: scalar in (1), struct in (cmdbuf), nothing out.
        kr = IOConnectCallMethod(connect, SEL_SUBMIT_CMDS_EX,
                                 (uint64_t*)&ctx_id, 1,
                                 cmdbuf, idx * sizeof(uint32_t),
                                 NULL, NULL, NULL, NULL);
        if (kr != KERN_SUCCESS) {
            fprintf(stderr, "FAIL 0x6008 submitVirglCommandsEx (round %d): 0x%x\n",
                    round, kr);
            exit_code = 1; goto cleanup;
        }
        printf("round %d: 0x6008 submitted %u dwords (CREATE_OBJECT+SET_FB+CLEAR+NOP)\n",
               round, idx);

        // For round 1 only: write the negative-control marker between rounds.
        // (For round 0 the buffer still has the position-dependent pattern fill
        // from earlier — host overwrites on CLEAR regardless.)
        if (round == 1) {
            memset(buf, 0xCD, PROBE_BUF_SIZE);
            printf("round 1: memset(buf, 0xCD) — negative-control marker\n");
        }

        // 0x3009 transferFromHost3D — host writes through scatter list back
        // into the userspace buffer.
        uint64_t xfer_in[9] = {
            resource_id,            // 0: resourceId
            0,                       // 1: level
            0, 0, 0,                 // 2,3,4: x, y, z
            PROBE_W, PROBE_H, 1,     // 5,6,7: width, height, depth
            ctx_id                   // 8: ctx_id
        };
        kr = call_scalar_in(connect, SEL_TRANSFER_FROM_3D, xfer_in, 9);
        if (kr != KERN_SUCCESS) {
            fprintf(stderr, "FAIL 0x3009 transferFromHost3D (round %d): 0x%x\n",
                    round, kr);
            exit_code = 1; goto cleanup;
        }
        printf("round %d: 0x3009 transferFromHost3D ok\n", round);

        // Verify every dword matches expected color.
        uint32_t mismatch_count = 0;
        uint32_t first_mm_idx = 0xFFFFFFFFu;
        uint32_t first_mm_actual = 0;
        for (uint32_t i = 0; i < px_count; i++) {
            if (px[i] != expected) {
                if (first_mm_idx == 0xFFFFFFFFu) {
                    first_mm_idx = i;
                    first_mm_actual = px[i];
                }
                mismatch_count++;
            }
        }

        if (mismatch_count == 0) {
            printf("*** round %d PASS — %u/%u dwords = 0x%08x ***\n",
                   round, px_count, px_count, expected);
        } else {
            printf("*** round %d FAIL — %u/%u dwords mismatch\n",
                   round, mismatch_count, px_count);
            printf("    First mismatch: idx %u (byte %u, page %u) got 0x%08x expected 0x%08x\n",
                   first_mm_idx, first_mm_idx * 4, (first_mm_idx * 4) / 4096u,
                   first_mm_actual, expected);
            if (first_mm_actual == 0xCDCDCDCDu) {
                printf("    Pattern: 0xCDCDCDCD → host wrote nothing\n");
            } else if (first_mm_actual == 0) {
                printf("    Pattern: 0x00000000 → host wrote zeros (not the colour)\n");
            } else if (first_mm_actual == expected_pattern_at(first_mm_idx)) {
                printf("    Pattern: original fill intact → host didn't touch this dword\n");
            }
            exit_code = 2;
            // Don't goto cleanup on first round failure — round 2 may still work.
            // But if round 0 failed, round 2's memset 0xCD won't happen (only runs in round==1).
            // So just break out of the loop.
            break;
        }
    }

cleanup:
    if (resource_id != 0) {
        uint64_t in[1] = { resource_id };
        kern_return_t r = call_scalar_in(connect, SEL_RESOURCE_UNREF, in, 1);
        printf("0x6005 resourceUnref(0x%x): 0x%x (also detaches backing)\n",
               resource_id, r);
    }
    if (ctx_id != 0) {
        uint64_t in[1] = { ctx_id };
        kern_return_t r = call_scalar_in(connect, SEL_DESTROY_CTX, in, 1);
        printf("0x6001 destroyVirglContextEx(0x%x): 0x%x\n", ctx_id, r);
    }
    if (base) free(base);
    if (connect != IO_OBJECT_NULL) IOServiceClose(connect);
    if (exit_code == 0) {
        printf("\n*** PROBE PASS — winsys selectors verified end-to-end ***\n");
    }
    return exit_code;
}

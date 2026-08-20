// probe_fmt_renderable.c
//
// Format/renderability discriminator for the empty-window-front finding
// (2026-08-20), v3 — blit-based, upload-controlled, interleaved.
//
// Background: the drawable front (pipe 53 = wire 67 R8G8B8A8_UNORM,
// 1531x904, bind RT) reads uniformly ZERO device-side while layer surfaces
// (pipe 54 = wire 1 B8G8R8A8_UNORM) carry content. Transport-level creates
// of window-sized RGBA RTs succeed (heartbeat ret=0x0, zero host decode
// errors), yet readbacks of wire-67 clears are FLAKY: 1531x904 RGBA passed
// one run, failed the next — so single-run verdicts here are worthless and
// order is a confound (RGBA always ran before BGRA). v3 controls for that.
//
// Four configs, three rounds, alternating order (A,B,C,D / D,C,B,A / A,B,C,D):
//   A. 64x64    wire 67 — CLEAR + readback + blit   (historical control)
//   B. 1531x904 wire 67 — CLEAR + readback + blit   (the window front, both axes)
//   C. 1531x904 wire 1  — CLEAR + readback + blit   (format control)
//   D. 512x512  wire 67 — UPLOAD (0x3008) + blit    (blit-format control: no
//        clear in the chain — separates "0x600C is format-sensitive" from
//        "clears never land on wire-67")
//
// The blit (0x600C, GPU-side texture->scanout copy in the kext's own ctx)
// takes readback out of the chain entirely. Screen rects (1680x1050 desktop):
//   A @(16,150,256,256)  B @(300,150,512,384)  C @(850,150,512,384)
//   D @(1400,150,256,256)
// All four use the same color (0.20,0.40,0.60,1.00 = rgb(51,102,153)); the
// screen verdict is per-patch presence, not hue.
//
// Pre-registered outcomes (stated before the run):
//   all patches visible        -> clears land; readback is the liar
//   C+D visible, A/B missing   -> clear path broken for wire-67 host-side
//   D missing too              -> 0x600C blit itself wire-67-sensitive
//   readback flips with order  -> warm-up/ordering artifact, not format
//
// Resources are deliberately NOT unref'd at round end: the kext's re-blit
// rule re-issues from the stored source resource after desktop refreshes, so
// the patches persist only while the sources stay alive.
//
// Build (cross, from the repo root):
//   clang -target x86_64-apple-macos10.6 \
//       -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.6.sdk \
//       -Wall -O2 -o probe_fmt_renderable probe/probe_fmt_renderable.c \
//       -framework IOKit -framework CoreFoundation

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <mach/mach.h>
#include <IOKit/IOKitLib.h>

#define SEL_CREATE_CTX          0x6000
#define SEL_DESTROY_CTX         0x6001
#define SEL_CREATE_RES_3D       0x6002
#define SEL_ATTACH_BACKING      0x6003
#define SEL_TRANSFER_TO_3D      0x3008
#define SEL_RESOURCE_UNREF      0x6005
#define SEL_GET_CAPSET_INFO     0x6006
#define SEL_GET_CAPSET          0x6007
#define SEL_SUBMIT_CMDS_EX      0x6008
#define SEL_CTX_ATTACH_RESOURCE 0x6009
#define SEL_FENCE_WAIT          0x600B
#define SEL_HOST_BLIT           0x600C
#define SEL_TRANSFER_FROM_3D    0x3009

#define VIRGL_TARGET_2D                 2
#define VIRGL_FORMAT_B8G8R8A8_UNORM     1
#define VIRGL_FORMAT_R8G8B8A8_UNORM     67
#define VIRGL_BIND_RENDER_TARGET        (1 << 1)
#define VIRGL_CMD0(cmd, obj, len)      ((cmd) | ((obj) << 8) | ((len) << 16))
#define VIRGL_CCMD_CREATE_OBJECT        1
#define VIRGL_CCMD_SET_FRAMEBUFFER_STATE 5
#define VIRGL_CCMD_CLEAR                7
#define VIRGL_CCMD_NOP                  0
#define VIRGL_OBJECT_SURFACE            8
#define VIRGL_OBJ_SURFACE_SIZE          5
#define VIRGL_OBJ_CLEAR_SIZE            8
#define PIPE_CLEAR_COLOR0               0x04
#define FCE1_MAGIC                      0x31454346u

static const float CLEAR_RGBA[4] = { 0.20f, 0.40f, 0.60f, 1.00f };
#define EXPECT_RGBA_DWORD   0xff996633u   // wire 67: bytes r,g,b,a
#define EXPECT_BGRA_DWORD   0xff336699u   // wire 1:  bytes b,g,r,a

static inline uint32_t virgl_pack_float(float f) {
    uint32_t u;
    memcpy(&u, &f, 4);
    return u;
}

static kern_return_t call_scalar_in(io_connect_t c, uint32_t sel,
                                     const uint64_t* in, uint32_t n_in)
{
    return IOConnectCallMethod(c, sel, in, n_in, NULL, 0, NULL, NULL, NULL, NULL);
}

static kern_return_t call_scalar_in_out(io_connect_t c, uint32_t sel,
                                         const uint64_t* in, uint32_t n_in,
                                         uint64_t* out, uint32_t n_out)
{
    uint32_t out_cap = n_out;
    return IOConnectCallMethod(c, sel, in, n_in, NULL, 0,
                               out, &out_cap, NULL, NULL);
}

// capset render mask: v1 layout max_version@0, sampler@4, render@68 (16 words)
static void decode_capset(io_connect_t connect)
{
    uint64_t in[1] = { 0 };
    uint64_t out[3] = {0};
    if (call_scalar_in_out(connect, SEL_GET_CAPSET_INFO, in, 1, out, 3)
        != KERN_SUCCESS) {
        printf("capset: getCapsetInfo FAIL\n");
        return;
    }
    printf("capset: id=%llu ver=%llu size=%llu\n", out[0], out[1], out[2]);
    uint8_t blob[2048];
    size_t blob_size = sizeof(blob);
    uint64_t cap_in[2] = { out[0], out[1] };
    if (IOConnectCallMethod(connect, SEL_GET_CAPSET, cap_in, 2,
                            NULL, 0, NULL, NULL, blob, &blob_size)
        != KERN_SUCCESS || blob_size < 132) {
        printf("capset: getCapset FAIL or too small (%zu)\n", blob_size);
        return;
    }
    const uint32_t* render = (const uint32_t*)(blob + 68);
    printf("capset: render w0..3: %08x %08x %08x %08x\n",
           render[0], render[1], render[2], render[3]);
    printf("capset: fmt 1 (B8G8R8A8): %s | fmt 67 (R8G8B8A8): %s\n",
           (render[0] >> 1) & 1u ? "RENDERABLE" : "NOT-RENDERABLE",
           (render[2] >> 3) & 1u ? "RENDERABLE" : "NOT-RENDERABLE");
}

// one config round: create, attach, clear-or-upload, fence-wait, readback
// (A/B/C), blit. Returns 0 = readback exact, 1 = soft fail, 2 = readback
// mismatch, 3 = hard error. D (upload mode) skips readback (blit is primary).
static int run_config(io_connect_t connect, uint32_t ctx_id, int round,
                      const char* label, uint32_t fmt_wire,
                      uint32_t w, uint32_t h, uint32_t surf_handle,
                      uint32_t expect_dword, int upload_mode,
                      uint32_t bx, uint32_t by, uint32_t bw, uint32_t bh)
{
    printf("\n=== r%d %s: fmt=%u %ux%u mode=%s ===\n",
           round, label, fmt_wire, w, h, upload_mode ? "UPLOAD" : "CLEAR");

    uint32_t resource_id = 0;
    {
        uint64_t in[11] = { ctx_id, VIRGL_TARGET_2D, fmt_wire,
                            VIRGL_BIND_RENDER_TARGET, w, h,
                            1, 1, 0, 0, 0 };
        uint64_t out[1] = {0};
        if (call_scalar_in_out(connect, SEL_CREATE_RES_3D, in, 11, out, 1)
            != KERN_SUCCESS) {
            printf("%s: 0x6002 FAIL\n", label);
            return 3;
        }
        resource_id = (uint32_t)out[0];
        printf("%s: res=%u\n", label, resource_id);
    }

    const size_t buf_size = (size_t)w * h * 4;
    uint8_t* base = (uint8_t*)malloc(buf_size + 128);
    if (!base) return 3;
    uint8_t* buf = base + 17;

    {
        uint64_t addr = (uint64_t)(uintptr_t)buf;
        uint64_t in[5] = {
            resource_id,
            (uint32_t)(addr & 0xFFFFFFFFull), (uint32_t)(addr >> 32),
            (uint32_t)((uint64_t)buf_size & 0xFFFFFFFFull),
            (uint32_t)((uint64_t)buf_size >> 32),
        };
        if (call_scalar_in(connect, SEL_ATTACH_BACKING, in, 5) != KERN_SUCCESS) {
            printf("%s: 0x6003 FAIL\n", label);
            free(base);
            return 3;
        }
    }
    {
        uint64_t in[2] = { ctx_id, resource_id };
        call_scalar_in(connect, SEL_CTX_ATTACH_RESOURCE, in, 2);
    }

    if (upload_mode) {
        // CPU-fill the backing with the same color dword, upload via 0x3008
        uint32_t* px = (uint32_t*)buf;
        for (uint32_t i = 0; i < buf_size / 4; i++)
            px[i] = expect_dword;
        uint64_t stride = (uint64_t)w * 4;
        uint64_t in[12] = {
            resource_id, 0,
            0, 0, 0,
            w, h, 1,
            ctx_id,
            stride, stride * h, 0,
        };
        if (call_scalar_in(connect, SEL_TRANSFER_TO_3D, in, 12) != KERN_SUCCESS) {
            printf("%s: 0x3008 upload FAIL\n", label);
            free(base);
            return 3;
        }
        printf("%s: 0x3008 uploaded 0x%08x pattern\n", label, expect_dword);
    } else {
        // CLEAR via 0x6008, FCE1-framed (fence era contract)
        uint32_t cmd[20];
        unsigned idx = 0;
        cmd[idx++] = VIRGL_CMD0(VIRGL_CCMD_CREATE_OBJECT, VIRGL_OBJECT_SURFACE, VIRGL_OBJ_SURFACE_SIZE);
        cmd[idx++] = surf_handle;
        cmd[idx++] = resource_id;
        cmd[idx++] = fmt_wire;
        cmd[idx++] = 0;  // texture_level
        cmd[idx++] = 0;  // texture_layers
        cmd[idx++] = VIRGL_CMD0(VIRGL_CCMD_SET_FRAMEBUFFER_STATE, 0, 3);
        cmd[idx++] = 1;  // nr_cbufs
        cmd[idx++] = 0;  // zsurf
        cmd[idx++] = surf_handle;
        cmd[idx++] = VIRGL_CMD0(VIRGL_CCMD_CLEAR, 0, VIRGL_OBJ_CLEAR_SIZE);
        cmd[idx++] = PIPE_CLEAR_COLOR0;
        cmd[idx++] = virgl_pack_float(CLEAR_RGBA[0]);
        cmd[idx++] = virgl_pack_float(CLEAR_RGBA[1]);
        cmd[idx++] = virgl_pack_float(CLEAR_RGBA[2]);
        cmd[idx++] = virgl_pack_float(CLEAR_RGBA[3]);
        cmd[idx++] = 0;  // depth lo
        cmd[idx++] = 0;  // depth hi
        cmd[idx++] = 0;  // stencil
        cmd[idx++] = VIRGL_CMD0(VIRGL_CCMD_NOP, 0, 0);

        uint32_t frame[3 + 20];
        frame[0] = FCE1_MAGIC;
        frame[1] = 1;
        frame[2] = resource_id;
        memcpy(&frame[3], cmd, idx * sizeof(uint32_t));

        uint64_t ctx_scalar = ctx_id;
        if (IOConnectCallMethod(connect, SEL_SUBMIT_CMDS_EX,
                                &ctx_scalar, 1,
                                frame, (3 + idx) * sizeof(uint32_t),
                                NULL, NULL, NULL, NULL) != KERN_SUCCESS) {
            printf("%s: 0x6008 FAIL\n", label);
            free(base);
            return 3;
        }
        printf("%s: 0x6008 ok (%u dwords)\n", label, idx);
    }

    // fence wait — blit contract requires source complete
    {
        uint64_t in[2] = { resource_id, 0 };
        call_scalar_in(connect, SEL_FENCE_WAIT, in, 2);
    }

    int verdict = 1;
    if (!upload_mode) {
        memset(buf, 0xCD, buf_size);   // negative-control marker
        uint64_t stride = (uint64_t)w * 4;
        uint64_t in[12] = {
            resource_id, 0,
            0, 0, 0,
            w, h, 1,
            ctx_id,
            stride, stride * h, 0,
        };
        if (call_scalar_in(connect, SEL_TRANSFER_FROM_3D, in, 12)
            != KERN_SUCCESS) {
            printf("%s: 0x3009 FAIL\n", label);
            free(base);
            return 3;
        }
        const uint32_t* px = (const uint32_t*)buf;
        uint32_t total = (uint32_t)(buf_size / 4);
        uint32_t mismatch = 0, zeros = 0;
        for (uint32_t i = 0; i < total; i++) {
            if (px[i] != expect_dword) {
                mismatch++;
                if (px[i] == 0) zeros++;
            }
        }
        if (mismatch == 0) {
            printf("*** r%d %s READBACK PASS (%u/%u = 0x%08x) ***\n",
                   round, label, total, total, expect_dword);
            verdict = 0;
        } else {
            printf("*** r%d %s READBACK FAIL — %u/%u mismatch (zeros=%u) ***\n",
                   round, label, mismatch, total, zeros);
            verdict = 2;
        }
    }

    // the blit: GPU-side present, readback not in the chain
    {
        uint64_t in[5] = { resource_id, bx, by, bw, bh };
        kern_return_t kr = call_scalar_in(connect, SEL_HOST_BLIT, in, 5);
        printf("%s: 0x600C blit %ux%u@%u,%u -> 0x%x\n",
               label, bw, bh, bx, by, kr);
        if (kr != KERN_SUCCESS) verdict = 3;
    }

    // NOTE: resource intentionally kept alive (re-blit rule re-issues it)
    free(base);
    return verdict;
}

int main(void)
{
    kern_return_t kr;
    io_service_t service = IO_OBJECT_NULL;
    io_connect_t connect = IO_OBJECT_NULL;
    uint32_t ctx_id = 0;

    CFMutableDictionaryRef matching = IOServiceMatching("VMQemuVGAAccelerator");
    if (!matching) return 1;
    service = IOServiceGetMatchingService(kIOMasterPortDefault, matching);
    if (service == IO_OBJECT_NULL) { printf("FAIL: no service\n"); return 1; }
    kr = IOServiceOpen(service, mach_task_self(), 4, &connect);
    IOObjectRelease(service);
    if (kr != KERN_SUCCESS) { printf("FAIL: open 0x%x\n", kr); return 1; }
    printf("connected\n");

    {
        uint64_t out[1] = {0};
        if (call_scalar_in_out(connect, SEL_CREATE_CTX, NULL, 0, out, 1)
            != KERN_SUCCESS) {
            printf("FAIL: create ctx\n");
            IOServiceClose(connect);
            return 1;
        }
        ctx_id = (uint32_t)out[0];
        printf("ctx=0x%x\n", ctx_id);
    }

    decode_capset(connect);

    // rounds: 0 = A,B,C,D   1 = D,C,B,A (counterbalanced)   2 = A,B,C,D
    for (int round = 0; round < 3; round++) {
        if (round == 1) {
            run_config(connect, ctx_id, round, "D-upload-512-RGBA", 67, 512, 512,
                       30 + round, EXPECT_RGBA_DWORD, 1, 1400, 150, 256, 256);
            run_config(connect, ctx_id, round, "C-1531x904-BGRA", 1, 1531, 904,
                       20 + round, EXPECT_BGRA_DWORD, 0, 850, 150, 512, 384);
            run_config(connect, ctx_id, round, "B-1531x904-RGBA", 67, 1531, 904,
                       10 + round, EXPECT_RGBA_DWORD, 0, 300, 150, 512, 384);
            run_config(connect, ctx_id, round, "A-64x64-RGBA", 67, 64, 64,
                       1 + round, EXPECT_RGBA_DWORD, 0, 16, 150, 256, 256);
        } else {
            run_config(connect, ctx_id, round, "A-64x64-RGBA", 67, 64, 64,
                       1 + round, EXPECT_RGBA_DWORD, 0, 16, 150, 256, 256);
            run_config(connect, ctx_id, round, "B-1531x904-RGBA", 67, 1531, 904,
                       10 + round, EXPECT_RGBA_DWORD, 0, 300, 150, 512, 384);
            run_config(connect, ctx_id, round, "C-1531x904-BGRA", 1, 1531, 904,
                       20 + round, EXPECT_BGRA_DWORD, 0, 850, 150, 512, 384);
            run_config(connect, ctx_id, round, "D-upload-512-RGBA", 67, 512, 512,
                       30 + round, EXPECT_RGBA_DWORD, 1, 1400, 150, 256, 256);
        }
    }

    {
        uint64_t in[1] = { ctx_id };
        call_scalar_in(connect, SEL_DESTROY_CTX, in, 1);
    }
    IOServiceClose(connect);
    printf("\nrounds done — screen verdict: four steel-blue patches expected at "
           "y=150: A x=16 B x=300 C x=850 D x=1400\n");
    return 0;
}

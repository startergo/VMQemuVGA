/*
 * gld_stub.c — rung-3 stub GLD (LEDGER 2026-08-21 night, pre-registered).
 * Per-entry HONEST REFUSALS per trampoline conventions
 * (../../VMsvga2-modern/GLD/VMsvga2GLDriver.h + .c fallbacks):
 *   GLDReturn entries (24 explicit + 47 generic) -> return -1
 *     (gldGetRendererInfo fallback returns -1; 0 is SUCCESS-with-filled-struct)
 *   _Bool entry (gldGetVersion)                  -> return false
 *   void entries (5, below)                      -> log only
 * No pointer-returning entries exist in the header — all non-void/bool
 * returns are int-shaped. UNDER-CLAIM BY CONSTRUCTION: a loaded stub
 * refuses every capability query; the first-caller log is a primary
 * observable. Entry names generated verbatim from
 * ../../VMsvga2-modern/GLD/EntryPointNames.c (Zenith432, MIT).
 * Loaded via /S/L/E/VMVirtIOGLEngine.bundle (Contents/MacOS layout,
 * stock-GLD shape; additive — no Apple bundle touched).
 */
#include <stdio.h>
#include <time.h>
#include <unistd.h>

static void ep_log(const char *tag)
{
    FILE *f = fopen("/tmp/vm_gld_stub.log", "a");
    if (!f) return;
    time_t t = time(NULL);
    char ts[32];
    strftime(ts, sizeof(ts), "%H:%M:%S", localtime(&t));
    fprintf(f, "[%s pid=%d] %s\n", ts, (int)getpid(), tag);
    fclose(f);
}

__attribute__((constructor))
static void gld_stub_loaded(void)
{
    ep_log("STUB LOADED (constructor, rung 3)");
}

/* OUT-ZERO RULE (rung 13 lesson, applied across ALL entries in one
 * pass): every entry with an out-parameter writes NULL to it before
 * ANY return path. The GLEngine caller's stack slot is never
 * initialized — a refusal that doesn't write *out leaves garbage
 * that the caller dereferences (the rung-12/13 SIGBUS mechanism).
 * The generic form: first arg is void* (the commonest out shape);
 * we zero it ONLY if non-NULL and ONLY for entries in the
 * out-parameter families (creators, queries, format entries).
 * Risk: if an entry's first arg is an integer handle rather than a
 * pointer, writing through it faults — accepted for a gated stub;
 * each entry gets a typed signature when its call site is read. */
/* RUNG 64i — THE FLOAT TRAMPOLINE: forward engine calls to the REAL
 * GLRendererFloat. Its attach builds the true drawbuffer (ctx+0x218),
 * its machinery rasterizes, its own swap blits into the GA surface —
 * and our swap WRAPPER (installed over block1+0xE8) pushes the
 * surface afterward (0x600D). We keep OUR: pixel format (the depth
 * codes), strings (capset), and the wrap layer. The float's entries
 * take <= 6 args per the plugin ABI, so the uniform 6-arg void*
 * forward is ABI-safe on x86_64. */
#include <dlfcn.h>
#include <mach-o/dyld.h>
static void* g_float_lib = 0;
static int g_fwd_armed = 0;   /* RUNG 64i gate: the loader probes
                               * informational entries BEFORE any
                               * Initialize — forwarding those to an
                               * UNINITIALIZED float corrupts the
                               * engine's registration (observed:
                               * CGLCreateContext dying pre-renderer).
                               * float_sym refuses until OUR Initialize
                               * completes, then the trampoline arms. */
/* the float's install (+0x50) / attach (+0x48) entries, captured from
 * the float-filled dispatch block; the float's swap, captured at wrap
 * time; the engine call block. */
static long (*g_float_install)(void*,void*,void*,void*,void*,void*) = 0;
static long (*g_float_attach)(void*,void*,void*,void*,void*,void*) = 0;
static long (*g_float_swap_fn)(void*,void*,void*,void*,void*,void*) = 0;
static void* g_block1 = 0;
long gld_swap_wrapper(void* dctx, void* a1, void* a2, void* a3,
                      void* a4, void* a5);
static void* float_sym(const char* name)
{
    if (!g_fwd_armed)
        return (void*)0;         /* not yet: see the gate comment above */
    if (!g_float_lib) {
        g_float_lib = dlopen(
            "/System/Library/Frameworks/OpenGL.framework/Versions/A/"
            "Resources/GLRendererFloat.bundle/GLRendererFloat",
            RTLD_NOW | RTLD_LOCAL);
        char b[160];
        snprintf(b, sizeof(b), "rung64i: float dlopen -> %p%s",
                 g_float_lib, g_float_lib ? "" : " (FAILED)");
        ep_log(b);
    }
    return g_float_lib ? dlsym(g_float_lib, name) : (void*)0;
}
#define GLD_FWD(name, a0,a1,a2,a3,a4,a5, fb) do { \
    static long (*fn)(void*,void*,void*,void*,void*,void*) = 0; \
    if (!fn) fn = (long(*)(void*,void*,void*,void*,void*,void*)) \
                 float_sym(name); \
    if (fn) return (long)fn(a0,a1,a2,a3,a4,a5); \
} while (0)
#define GLD_FWDV(name, a0,a1,a2,a3,a4,a5) do { \
    static void (*fn)(void*,void*,void*,void*,void*,void*) = 0; \
    if (!fn) fn = (void(*)(void*,void*,void*,void*,void*,void*)) \
                 float_sym(name); \
    if (fn) { fn(a0,a1,a2,a3,a4,a5); return; } \
} while (0)

/* RUNG 64i second correction: the CONTENT objects (textures, programs,
 * SetInteger...) arrive through these EXPORTED entries — they must
 * forward once armed (the loader-time probes stay refused via the
 * gate). gldCreateShared stays OURS (it fires inside CGLCreateContext
 * before any create; the float's version there broke the context path).
 * Blanket + gate + our-CreateShared = the 22:43-successful shape plus
 * content objects. */
/* RUNG 66e — the export census: file-scope counters on every
 * macro-generated entry (the draw door IS the export table). */
#define EPR(n) static long g_ec_##n; \
long n(void* a0, void* a1, void* a2, void* a3, void* a4, void* a5) { \
    g_ec_##n++; \
    GLD_FWD(#n, a0,a1,a2,a3,a4,a5, -1); \
    if (a0) *(void**)a0 = (void*)0; \
    ep_log("CALL " #n " -> -1 (refusal; out zeroed)"); return -1; }
#define EPB(n) static long g_ec_##n; \
long n(void* a0, void* a1, void* a2, void* a3, void* a4, void* a5) { \
    g_ec_##n++; \
    GLD_FWD(#n, a0,a1,a2,a3,a4,a5, 0); \
    if (a0) *(void**)a0 = (void*)0; \
    ep_log("CALL " #n " -> false (out zeroed)"); return 0; }
#define EPV(n) static long g_ec_##n; \
void n(void* a0, void* a1, void* a2, void* a3, void* a4, void* a5) { \
    g_ec_##n++; \
    GLD_FWDV(#n, a0,a1,a2,a3,a4,a5); \
    if (a0) *(void**)a0 = (void*)0; \
    ep_log("CALL " #n " (void; out zeroed)"); }

/* RUNG 6 (pre-registered 2026-08-21 late): the VM lifecycle pair goes
 * REAL — first ACTING rung. Disassembly of the working GLD: real
 * Initialize is SIX-arg (reads %r9d; the trampoline header's 5-arg
 * void signature is incomplete), stores GLDisplayMask into
 * gld_io_data, tail-calls glvmPreInit(arg6 & 1) and PROPAGATES its
 * return. Terminate tail-calls glvmPostTerm. glvmPreInit lives in
 * GLEngine (direct-grep; loads before any GLD — RTLD_DEFAULT has
 * guaranteed ordering). gldGetVersion becomes GUARDED, mirroring the
 * working GLD's own honesty: version-true only after a successful VM
 * forward — never claims what the VM hasn't backed. */
#include <dlfcn.h>
#include <pthread.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/graphics/IOGraphicsInterface.h>
static int g_vm_ok = 0;    /* rung 6b: mask-store guard — the working GLD's actual structure */
static int g_vm_mask = 0;  /* mirror of the real gld_io_data mask store */

/* RUNG 29: the shared object's processor block stand-in — writable,
 * zeroed (the float uses its own glg_processor_default_data). */
static unsigned long g_proc_stand_in[64];

/* RUNG 48 — the engine sub-block (gldCreateContext's a4); the
 * engine context base = sub-block − 0x79b8 (renderer idx 0). */
static void* g_engine_subblock = NULL;

/* RUNG 64 — the engine-side blocks the install entry receives (a2/a3
 * = &engine[0x65c8]/&engine[0x66d0]); the swap scan follows them
 * (deriving from the sub-block when the direct install ran). */
static void* g_engine_block1 = 0;
static void* g_engine_block2 = 0;

/* RUNG 64d — THE HEAP HUNT: with our stub as renderer (no renderer
 * side buffer), the engine's raster target is SOME malloc'd block.
 * Enumerate the zones at swap, log every >=1MB block's head twice;
 * the block whose bytes CHANGE between swaps is the live frame. */
static int g_fb_w = 4, g_fb_h = 4;   /* rung 39: window-sized when attached */
#include <malloc/malloc.h>
#include <mach/mach_vm.h>
static kern_return_t heap_reader(task_t t, vm_address_t a, vm_size_t s,
                                 void** p)
{
    (void)t; (void)s; (void)a;
    *p = (void*)a;                    /* in-process: identity reader */
    return KERN_SUCCESS;
}
static void heap_recorder(task_t t, void* ctx, unsigned type,
                          vm_range_t* ranges, unsigned count)
{
    (void)t; (void)ctx; (void)type;
    /* RUNG 64e: capture the frame-sized blocks (w*h*4) — the GLVM
     * double buffer pair; the swap presents the live one. */
    extern unsigned char* g_frame_bufs[2];
    extern int g_frame_buf_count;
    for (unsigned i = 0; i < count; i++) {
        size_t want = (size_t)g_fb_w * g_fb_h * 4;
        if (g_frame_buf_count < 2 && want
            && ranges[i].size >= want && ranges[i].size <= want + 0x1000) {
            g_frame_bufs[g_frame_buf_count++] =
                (unsigned char*)ranges[i].address;
        }
        if (ranges[i].size < 0x40000 || ranges[i].size > 0x1000000)
            continue;
        char hb[80]; int ho = 0;
        unsigned char* p = (unsigned char*)ranges[i].address;
        for (int c = 0; c < 16 && ho < 60; c++)
            ho += snprintf(hb + ho, sizeof(hb) - ho, "%02x", p[c]);
        char sb[208];
        snprintf(sb, sizeof(sb), "rung64: HEAP %llx sz=%llx b:%s",
                 (unsigned long long)ranges[i].address,
                 (unsigned long long)ranges[i].size, hb);
        ep_log(sb);
        /* RUNG 64f census: quarter-points of every candidate — the
         * live frame reads VARIED bytes; dead staging reads uniform. */
        {
            static int s_census_left = 4;
            if (s_census_left-- > 0) {
                size_t sz = ranges[i].size;
                char cb[320]; int co = 0;
                co += snprintf(cb + co, sizeof(cb) - co,
                               "rung64: CENSUS %llx:",
                               (unsigned long long)ranges[i].address);
                for (int q = 1; q <= 4 && co < 280; q++) {
                    unsigned char* s = p + (((size_t)sz * q) / 5 & ~(size_t)15);
                    co += snprintf(cb + co, sizeof(cb) - co, " q%d:", q);
                    for (int c = 0; c < 8 && co < 295; c++)
                        co += snprintf(cb + co, sizeof(cb) - co,
                                       "%02x", s[c]);
                }
                ep_log(cb);
            }
        }
    }
}
unsigned char* g_frame_bufs[2] = { 0, 0 };
int g_frame_buf_count = 0;
/* RUNG 64g — THE VM HUNT: GLVM-era raster buffers are mach-vm
 * allocated (NOT malloc) — the malloc zones hold only staging. Walk
 * the writable VM regions, census each candidate's quarter-points. */
static void vm_census(mach_vm_address_t addr, mach_vm_size_t size,
                      int passno)
{
    char cb[336]; int co = 0;
    co += snprintf(cb + co, sizeof(cb) - co,
                   "rung64: VM%u %llx sz=%llx:",
                   passno, (unsigned long long)addr,
                   (unsigned long long)size);
    unsigned char* p = (unsigned char*)(uintptr_t)addr;
    for (int q = 0; q <= 4 && co < 300; q++) {
        unsigned char* s = p + (((size_t)size * q) / 5 & ~(size_t)15);
        co += snprintf(cb + co, sizeof(cb) - co, " q%d:", q);
        for (int c = 0; c < 8 && co < 312; c++)
            co += snprintf(cb + co, sizeof(cb) - co, "%02x", s[c]);
    }
    ep_log(cb);
}
static void vm_hunt_pass(int passno)
{
    mach_vm_address_t addr = 0;
    mach_vm_size_t size = 0;
    int n = 0;
    while (n++ < 400) {
        vm_region_basic_info_data_64_t info;
        mach_msg_type_number_t cnt = VM_REGION_BASIC_INFO_COUNT_64;
        mach_port_t obj = MACH_PORT_NULL;
        if (mach_vm_region(mach_task_self(), &addr, &size,
                           VM_REGION_BASIC_INFO_64,
                           (vm_region_info_t)&info, &cnt,
                           &obj) != KERN_SUCCESS)
            break;
        if (size >= 0x100000 && size <= 0x8000000
            && (info.protection & VM_PROT_WRITE))
            vm_census(addr, size, passno);
        addr += size;
    }
}
static void heap_hunt_pass(int passno)
{
    vm_address_t* zones = NULL;
    unsigned nz = 0;
    kern_return_t kr = malloc_get_all_zones(mach_task_self(),
                                            heap_reader, &zones, &nz);
    char zb[80];
    snprintf(zb, sizeof(zb), "rung64: HEAP pass %d zones=%u kr=0x%x",
             passno, nz, kr);
    ep_log(zb);
    for (unsigned i = 0; zones && i < nz; i++) {
        malloc_zone_t* z = (malloc_zone_t*)zones[i];
        if (z && z->introspect && z->introspect->enumerator)
            z->introspect->enumerator(mach_task_self(), NULL,
                                      MALLOC_PTR_IN_USE_RANGE_TYPE,
                                      (vm_address_t)z, heap_reader,
                                      heap_recorder);
    }
}

/* RUNG 52 — THE CAPSET: virgl_hw.h's structs, copied verbatim
 * (same x86_64 layout rules as the host's producer; v1 = 77 words
 * = 308 bytes = the boot-logged VIRGL size exactly). */
struct vm_format_mask { uint32_t bitmask[16]; };
struct vm_caps_v1 {
    uint32_t max_version;
    struct vm_format_mask sampler, render, depthstencil, vertexbuffer;
    uint32_t bset;   /* the bool set: one 32-bit bitfield word */
    uint32_t glsl_level, max_texture_array_layers, max_streamout_buffers,
             max_dual_source_render_targets, max_render_targets, max_samples,
             prim_mask, max_tbo_size, max_uniform_blocks, max_viewports,
             max_texture_gather_components;
};
/* RUNG 52b — the layout proof: the device advertises
 * capset_max_size 308 for id=1; if OUR v1 struct is not
 * exactly 308 bytes, the layout is wrong and every field
 * read is suspect. Compile-time fatal. */
struct vm_caps_v2 {
    struct vm_caps_v1 v1;
    float min_aliased_point_size, max_aliased_point_size;
    float min_smooth_point_size, max_smooth_point_size;
    float min_aliased_line_width, max_aliased_line_width;
    float min_smooth_line_width, max_smooth_line_width;
    float max_texture_lod_bias;
    uint32_t max_geom_output_vertices, max_geom_total_output_components,
             max_vertex_outputs, max_vertex_attribs, max_shader_patch_varyings;
    int32_t min_texel_offset, max_texel_offset,
            min_texture_gather_offset, max_texture_gather_offset;
    uint32_t texture_buffer_offset_alignment, uniform_buffer_offset_alignment,
             shader_buffer_offset_alignment, capability_bits;
    uint32_t sample_locations[8];
    uint32_t max_vertex_attrib_stride, max_shader_buffer_frag_compute,
             max_shader_buffer_other_stages, max_shader_image_frag_compute,
             max_shader_image_other_stages, max_image_samples,
             max_compute_work_group_invocations, max_compute_shared_memory_size;
    uint32_t max_compute_grid_size[3], max_compute_block_size[3];
    uint32_t max_texture_2d_size, max_texture_3d_size, max_texture_cube_size;
    uint32_t max_combined_shader_buffers;
    uint32_t max_atomic_counters[6], max_atomic_counter_buffers[6];
    uint32_t max_combined_atomic_counters, max_combined_atomic_counter_buffers;
    uint32_t host_feature_check_version;
    struct vm_format_mask supported_readback_formats, scanout;
    uint32_t capability_bits_v2, max_video_memory;
    char renderer[64];
    float max_anisotropy;
};
static struct vm_caps_v2 g_caps;
static int g_caps_fetched = 0, g_caps_v2_ok = 0;
/* RUNG 52b — THE LAYOUT PROOF + THE TRUNCATION GUARD:
 * the device advertises capset_max_size 308 for id=1 — if our v1
 * struct is not exactly 308 bytes the layout is wrong and every
 * field read is suspect (compile-time fatal). max_samples lives at
 * v1+0x11C = 284, inside BOTH the 308-byte v1 blob and the 764
 * delivered of v2 — cross-checked at runtime against the v1 blob. */
_Static_assert(sizeof(struct vm_caps_v1) == 308,
               "v1 capset layout != 308 — the device's advertised size");
static uint32_t g_v1_blob_samples = 0xFFFFFFFF;  /* the cross-check */
static io_connect_t g_virgl_conn;   /* tentative — defined with the
                                      * transport below (rung 37) */

/* The fetch — the winsys's own calls verbatim (id=2 first, v1
 * fallback): 0x6006 GET_CAPSET_INFO(idx) → {id, ver, size};
 * 0x6007 GET_CAPSET(id, ver) → the blob. */
static void virgl_fetch_capset(void)
{
    for (uint32_t want_id = 2; want_id >= 1; want_id--) {
        for (uint32_t idx = 0; idx < 2; idx++) {
            uint64_t in[1] = { idx };
            uint64_t out[3] = { 0, 0, 0 };
            uint32_t out_cnt = 3;
            kern_return_t kr = IOConnectCallMethod(
                g_virgl_conn, 0x6006, in, 1, NULL, 0,
                out, &out_cnt, NULL, NULL);
            if (kr != KERN_SUCCESS) continue;
            uint32_t id = (uint32_t)out[0], ver = (uint32_t)out[1],
                     size = (uint32_t)out[2];
            if (size == 0 || size > 2048 || id != want_id) continue;
            uint8_t blob[2048];
            size_t bsz = size;
            uint64_t cin[2] = { id, ver };
            kr = IOConnectCallMethod(g_virgl_conn, 0x6007,
                                     cin, 2, NULL, 0,
                                     NULL, NULL, blob, &bsz);
            if (kr != KERN_SUCCESS) continue;
            memset(&g_caps, 0, sizeof(g_caps));
            size_t copy = bsz < sizeof(g_caps) ? bsz : sizeof(g_caps);
            memcpy(&g_caps, blob, copy);
            g_caps_v2_ok = (id == 2);
            g_caps_fetched = 1;
            /* RUNG 52b — the cross-blob check: when the V2 blob is in
             * hand, ALSO fetch the V1 blob (308 bytes — the entire v1
             * struct, zero truncation margin doubt) and compare
             * max_samples: agreement proves the value is a host fact,
             * not a 764-byte-truncation artifact. */
            if (id == 2 && want_id == 2) {
                for (uint32_t j = 0; j < 2; j++) {
                    uint64_t ji[1] = { j };
                    uint64_t jo[3] = { 0, 0, 0 };
                    uint32_t jc = 3;
                    if (IOConnectCallMethod(g_virgl_conn, 0x6006,
                                            ji, 1, NULL, 0,
                                            jo, &jc, NULL, NULL)
                            != KERN_SUCCESS) continue;
                    if ((uint32_t)jo[0] != 1) continue;
                    uint8_t v1b[512];
                    size_t v1sz = (jo[2] < sizeof(v1b)) ? jo[2] : sizeof(v1b);
                    uint64_t vi[2] = { jo[0], jo[1] };
                    if (IOConnectCallMethod(g_virgl_conn, 0x6007,
                                            vi, 2, NULL, 0,
                                            NULL, NULL, v1b, &v1sz)
                            != KERN_SUCCESS) break;
                    struct vm_caps_v1 v1x;
                    memset(&v1x, 0, sizeof(v1x));
                    memcpy(&v1x, v1b,
                           v1sz < sizeof(v1x) ? v1sz : sizeof(v1x));
                    g_v1_blob_samples = v1x.max_samples;
                    /* RAW-DWORD control: the words at 0x100..0x12C of
                     * BOTH blobs, pre-struct — removes the last decode
                     * doubt (a struct-layout error would shift MANY
                     * fields; the raw bytes are the ground truth). */
                    char cb[240]; int co = 0;
                    co += snprintf(cb + co, sizeof(cb) - co,
                                   "rung52b: v1(%uB) samples=%u (v2: %u) "
                                   "glsl=%u RAW v1[0x100..]:",
                                   (unsigned)v1sz, v1x.max_samples,
                                   g_caps.v1.max_samples, v1x.glsl_level);
                    for (int wd = 0; wd < 8 && co < 220; wd++)
                        co += snprintf(cb + co, sizeof(cb) - co, " %08x",
                                       ((uint32_t*)v1b)[0x40 + wd]);
                    ep_log(cb);
                    char cb2[200]; int co2 = 0;
                    co2 += snprintf(cb2 + co2, sizeof(cb2) - co2,
                                    "rung52b: RAW v2[0x100..]:");
                    uint32_t v2raw[512];
                    memcpy(v2raw, blob, copy < sizeof(v2raw) ? copy : sizeof(v2raw));
                    for (int wd = 0; wd < 8 && co2 < 180; wd++)
                        co2 += snprintf(cb2 + co2, sizeof(cb2) - co2,
                                        " %08x", v2raw[0x40 + wd]);
                    ep_log(cb2);
                    break;
                }
            }
            char rb[160];
            snprintf(rb, sizeof(rb), "rung52: capset id=%u ver=%u size=%u "
                     "copy=%u 2d=%u 3d=%u cube=%u layers=%u rt=%u samples=%u "
                     "glsl=%u renderer=%.32s",
                     id, ver, size, (unsigned)copy,
                     g_caps.max_texture_2d_size, g_caps.max_texture_3d_size,
                     g_caps.max_texture_cube_size,
                     g_caps.v1.max_texture_array_layers,
                     g_caps.v1.max_render_targets, g_caps.v1.max_samples,
                     g_caps.v1.glsl_level, g_caps.renderer);
            ep_log(rb);
            return;
        }
    }
    ep_log("rung52: NO CAPSET returned (0x6006/0x6007)");
}

/* RUNG 37 — THE FIRST BRIDGE SLOT: gldClear through the virgl
 * transport, direct (no Mesa yet — the plumbing proof: a GLD
 * dispatch call reaching the device). The call sequence mirrors
 * the iokit winsys (Mesa-VirGL, cross-10.6): matching
 * VMQemuVGAAccelerator → IOServiceOpen type=4 → 0x6000 ctx
 * create → 0x6008 submit with the FCE1 fence frame
 * (['FCE1'][cres=0][blob]). The blob is VIRGL_CCMD_CLEAR (=7),
 * VIRGL_OBJ_CLEAR_SIZE=8: header 7|(0<<8)|(8<<16)=0x00080007,
 * then pipe-mask, 4 color dwords, depth qword, stencil. Color
 * (0,0,0,0) for this rung — the plumbing is the point; the
 * color path is a later slot's business. */
static io_connect_t g_virgl_conn = MACH_PORT_NULL;
static uint32_t g_virgl_ctx = 0;
static int g_virgl_state = 0;   /* 0=untried, 1=ok, -1=failed */

static int virgl_transport_init(void)
{
    if (g_virgl_state) return g_virgl_state;
    g_virgl_state = -1;
    CFMutableDictionaryRef matching =
        IOServiceMatching("VMQemuVGAAccelerator");
    if (!matching) { ep_log("rung37: IOServiceMatching FAILED"); return -1; }
    io_service_t service =
        IOServiceGetMatchingService(kIOMasterPortDefault, matching);
    if (!service) { ep_log("rung37: no VMQemuVGAAccelerator service"); return -1; }
    kern_return_t kr = IOServiceOpen(service, mach_task_self(), 4,
                                     &g_virgl_conn);
    IOObjectRelease(service);
    if (kr != KERN_SUCCESS) {
        char b[64]; snprintf(b, sizeof(b), "rung37: IOServiceOpen type=4 FAIL 0x%x", kr);
        ep_log(b); return -1;
    }
    uint64_t out[1] = { 0 };
    uint32_t out_cnt = 1;
    kr = IOConnectCallMethod(g_virgl_conn, 0x6000,
                             NULL, 0, NULL, 0,
                             out, &out_cnt, NULL, NULL);
    if (kr != KERN_SUCCESS) {
        char b[64]; snprintf(b, sizeof(b), "rung37: 0x6000 createVirglContextEx FAIL 0x%x", kr);
        ep_log(b); IOServiceClose(g_virgl_conn); g_virgl_conn = MACH_PORT_NULL; return -1;
    }
    g_virgl_ctx = (uint32_t)out[0];
    g_virgl_state = 1;
    char b[64]; snprintf(b, sizeof(b), "rung37: virgl ctx %u OPEN (transport live)", g_virgl_ctx);
    ep_log(b);
    return 1;
}

static long g_clear_count = 0;

/* RUNG 55/59 — THE MESA LINKAGE, SELF-CONTAINED: the bundle must
 * LOAD IN ANY PROCESS (GLMark, any app — no rpath requirements), so
 * libOSMesa is dlopen'd at runtime, NOT hard-linked. The rung-55
 * "dlopen crashes" was the ARITY BUG (a 4-param extern for a 5-param
 * function), not the link mode — the linked ostest passed five args
 * by coincidence. dlopen + the correct arity is the right shape. The
 * gl* entries resolve through OSMesaGetProcAddress. */
static void* g_os_lib = NULL;
static void* g_os_ctx = NULL;
static unsigned char* g_os_buffer = NULL;
static int g_os_created = 0, g_os_made_current = 0;
static void (*os_glClearColor)(float, float, float, float);
static void (*os_glClear)(unsigned);
static void (*os_glFinish)(void);
static void (*os_glReadPixels)(int, int, int, int, unsigned, unsigned, void*);
static const unsigned char* (*os_glGetString)(unsigned);
static void (*os_glBegin)(unsigned);
static void (*os_glEnd)(void);
static void (*os_glVertex2f)(float, float);
static void (*os_glColor3f)(float, float, float);
/* RUNG 78 — the GL2 shader entries (first shader through the
 * embedded Mesa context). */
static unsigned (*os_glCreateShader)(unsigned);
static void (*os_glShaderSource)(unsigned, int, const char* const*, const int*);
static void (*os_glCompileShader)(unsigned);
static void (*os_glGetShaderiv)(unsigned, unsigned, int*);
static void (*os_glGetShaderInfoLog)(unsigned, int, int*, char*);
static unsigned (*os_glCreateProgram)(void);
static void (*os_glAttachShader)(unsigned, unsigned);
static void (*os_glLinkProgram)(unsigned);
static void (*os_glGetProgramiv)(unsigned, unsigned, int*);
static void (*os_glUseProgram)(unsigned);
/* RUNG 82 — uniforms + texture for sampler/constant binding */
static int (*os_glGetUniformLocation)(unsigned, const char*);
static void (*os_glUniform1i)(int, int);
static void (*os_glUniform4fv)(int, int, const float*);
static unsigned (*os_glGenTextures)(int, unsigned*);
static void (*os_glBindTexture)(unsigned, unsigned);
static void (*os_glTexImage2D)(unsigned, int, int, int, int, int,
                               unsigned, unsigned, const void*);
static void (*os_glTexParameteri)(unsigned, unsigned, int);
static int (*os_glUniform1f)(int, float);   /* rung 85 */
static int g_r78_prog;   /* >0 linked program, -1 failed, 0 not tried */

static void osmesa_create_at_load(void)
{
    if (g_os_created) return;
    g_os_created = 1;
    setenv("GALLIUM_DRIVER", "virgl", 1);
    /* RUNG 75: the bundle carries libOSMesa+libglapi with the dep
     * rewritten to @loader_path — no dyld env needed (runtime setenv
     * cannot reach dyld). Resolve our own bundle dir by image walk. */
    char osglib_path[512];
    osglib_path[0] = 0;
    {
        uint32_t n = _dyld_image_count();
        for (uint32_t i = 0; i < n; i++) {
            const char* nm = _dyld_get_image_name(i);
            if (nm && strstr(nm, "VMVirtIOGLEngine")) {
                const char* slash = strrchr(nm, '/');
                if (slash) {
                    size_t L = (size_t)(slash - nm);
                    if (L > sizeof(osglib_path) - 40) L = 0;
                    memcpy(osglib_path, nm, L);
                    osglib_path[L] = 0;
                    strcat(osglib_path, "/libOSMesa.8.dylib");
                }
                break;
            }
        }
    }
    char b[256];
    if (!osglib_path[0]) {
        ep_log("rung75: bundle dir not found (image walk)");
        return;
    }
    /* RTLD_LOCAL IS LOAD-BEARING: plain RTLD_LAZY is RTLD_GLOBAL —
     * OSMesa's gl* symbols then win the app's runtime dlsym races
     * (glmark's GLStateMacOS::valid() dlsym'd glGenVertexArrays,
     * got Mesa's, and crashed with no Mesa ctx current — 14:12:57
     * crash). LOCAL keeps the embedded stack private to the stub. */
    g_os_lib = dlopen(osglib_path, RTLD_LAZY | RTLD_LOCAL);
    if (!g_os_lib) {
        const char* e = dlerror();
        snprintf(b, sizeof(b), "rung59/75: OSMesa dlopen FAILED: %s",
                 e ? e : "(no dlerror)");
        ep_log(b);
        return;
    }
    void* (*create)(unsigned, int, int, int, void*) =
        (void* (*)(unsigned, int, int, int, void*))dlsym(
            g_os_lib, "OSMesaCreateContextExt");
    int (*makecur)(void*, void*, unsigned, int, int) =
        (int (*)(void*, void*, unsigned, int, int))dlsym(
            g_os_lib, "OSMesaMakeCurrent");
    void* (*getproc)(const char*) =
        (void* (*)(const char*))dlsym(g_os_lib, "OSMesaGetProcAddress");
    if (!create || !makecur || !getproc) {
        ep_log("rung59: OSMesa symbols missing"); return;
    }
    /* RUNG 58: DEPTHLESS (0/0) — the read path stages the depth
     * surface as a fmt-19 array vrend rejects. FIVE args — the
     * arity bug that crashed two rungs lives here. */
    g_os_ctx = create(0x1908 /*GL_RGBA*/, 0, 0, 0, NULL);
    snprintf(b, sizeof(b), "rung59: load-time dlopen+create -> ctx=%p",
             g_os_ctx);
    ep_log(b);
    (void)makecur; (void)getproc;
}

static int osmesa_link_init(int w, int h)
{
    if (!g_os_ctx || !g_os_lib) return -1;
    if (g_os_made_current) return 0;
    g_os_made_current = 1;
    int (*makecur)(void*, void*, unsigned, int, int) =
        (int (*)(void*, void*, unsigned, int, int))dlsym(
            g_os_lib, "OSMesaMakeCurrent");
    void* (*getproc)(const char*) =
        (void* (*)(const char*))dlsym(g_os_lib, "OSMesaGetProcAddress");
    g_os_buffer = (unsigned char*)calloc(1, (size_t)w * h * 4);
    if (!makecur(g_os_ctx, g_os_buffer, 0x1401, w, h)) {
        ep_log("rung59: OSMesaMakeCurrent FAILED"); return -1;
    }
    os_glClearColor = (void (*)(float, float, float, float))getproc("glClearColor");
    os_glClear = (void (*)(unsigned))getproc("glClear");
    os_glFinish = (void (*)(void))getproc("glFinish");
    os_glReadPixels = (void (*)(int, int, int, int, unsigned, unsigned, void*))getproc("glReadPixels");
    os_glGetString = (const unsigned char* (*)(unsigned))getproc("glGetString");
    os_glBegin   = (void (*)(unsigned))getproc("glBegin");
    os_glEnd     = (void (*)(void))getproc("glEnd");
    os_glVertex2f = (void (*)(float, float))getproc("glVertex2f");
    os_glColor3f  = (void (*)(float, float, float))getproc("glColor3f");
    /* RUNG 78 */
    os_glCreateShader = (unsigned (*)(unsigned))getproc("glCreateShader");
    os_glShaderSource = (void (*)(unsigned, int, const char* const*, const int*))getproc("glShaderSource");
    os_glCompileShader = (void (*)(unsigned))getproc("glCompileShader");
    os_glGetShaderiv = (void (*)(unsigned, unsigned, int*))getproc("glGetShaderiv");
    os_glGetShaderInfoLog = (void (*)(unsigned, int, int*, char*))getproc("glGetShaderInfoLog");
    os_glCreateProgram = (unsigned (*)(void))getproc("glCreateProgram");
    os_glAttachShader = (void (*)(unsigned, unsigned))getproc("glAttachShader");
    os_glLinkProgram = (void (*)(unsigned))getproc("glLinkProgram");
    os_glGetProgramiv = (void (*)(unsigned, unsigned, int*))getproc("glGetProgramiv");
    os_glUseProgram = (void (*)(unsigned))getproc("glUseProgram");
    /* RUNG 82 */
    os_glGetUniformLocation = (int (*)(unsigned, const char*))getproc("glGetUniformLocation");
    os_glUniform1i = (void (*)(int, int))getproc("glUniform1i");
    os_glUniform4fv = (void (*)(int, int, const float*))getproc("glUniform4fv");
    os_glGenTextures = (unsigned (*)(int, unsigned*))getproc("glGenTextures");
    os_glBindTexture = (void (*)(unsigned, unsigned))getproc("glBindTexture");
    os_glTexImage2D = (void (*)(unsigned, int, int, int, int, int, unsigned, unsigned, const void*))getproc("glTexImage2D");
    os_glTexParameteri = (void (*)(unsigned, unsigned, int))getproc("glTexParameteri");
    os_glUniform1f = (int (*)(int, float))getproc("glUniform1f");
    ep_log("rung59: OSMesa LINKED (dlopen route, ctx + buffer live)");
    /* RUNG 75: the driver verdict — virgl or software fallback */
    if (os_glGetString) {
        const unsigned char* rend = os_glGetString(0x1F01 /*GL_RENDERER*/);
        const unsigned char* vend = os_glGetString(0x1F00 /*GL_VENDOR*/);
        char b[192];
        snprintf(b, sizeof(b), "rung75: OSMesa GL_VENDOR='%s' "
                 "GL_RENDERER='%s'", vend ? (const char*)vend : "?",
                 rend ? (const char*)rend : "?");
        ep_log(b);
    }
    return 0;
}

/* RUNG 78 — the first shader through the embedded Mesa context.
 * Trivial GLSL-110 pair, compiled ONCE, loud on every failure branch
 * with the info log; the render path falls back to the rung-76
 * fixed-function frame unchanged when this returns 0. The fragment
 * color (1,0,1) is chosen so the readback self-check is exact: the
 * shader pass covers the whole viewport, so every read-back pixel
 * must be (255,0,255,255) — anything else is a MISMATCH. */
static int rung78_shader_prog(void)
{
    static int s_done = 0;
    if (s_done) return g_r78_prog > 0 ? g_r78_prog : 0;
    s_done = 1;
    if (!os_glCreateShader || !os_glShaderSource || !os_glCompileShader
            || !os_glGetShaderiv || !os_glCreateProgram
            || !os_glAttachShader || !os_glLinkProgram
            || !os_glGetProgramiv || !os_glUseProgram) {
        ep_log("rung78: shader entries UNRESOLVED via getproc");
        g_r78_prog = -1;
        return 0;
    }
    static const char* vs_src =
        "void main() { gl_Position = gl_Vertex; }";
    static const char* fs_src =
        "void main() { gl_FragColor = vec4(1.0, 0.0, 1.0, 1.0); }";
    const char* src[1];
    unsigned v = os_glCreateShader(0x8B31 /*GL_VERTEX_SHADER*/);
    unsigned f = os_glCreateShader(0x8B30 /*GL_FRAGMENT_SHADER*/);
    src[0] = vs_src; os_glShaderSource(v, 1, src, NULL);
    os_glCompileShader(v);
    src[0] = fs_src; os_glShaderSource(f, 1, src, NULL);
    os_glCompileShader(f);
    int cv = 0, cf = 0;
    os_glGetShaderiv(v, 0x8B81 /*GL_COMPILE_STATUS*/, &cv);
    os_glGetShaderiv(f, 0x8B81, &cf);
    if (!cv || !cf) {
        char lv[160] = "", lf[160] = "";
        int n1 = 0, n2 = 0;
        if (os_glGetShaderInfoLog) {
            os_glGetShaderInfoLog(v, (int)sizeof(lv) - 1, &n1, lv);
            os_glGetShaderInfoLog(f, (int)sizeof(lf) - 1, &n2, lf);
        }
        char b[400];
        snprintf(b, sizeof(b),
            "rung78: COMPILE FAIL vs=%d fs=%d | VS: %.150s | FS: %.150s",
            cv, cf, lv, lf);
        ep_log(b);
        g_r78_prog = -1;
        return 0;
    }
    unsigned p = os_glCreateProgram();
    os_glAttachShader(p, v);
    os_glAttachShader(p, f);
    os_glLinkProgram(p);
    int cl = 0;
    os_glGetProgramiv(p, 0x8B82 /*GL_LINK_STATUS*/, &cl);
    if (!cl) {
        char lp[200] = "";
        int n3 = 0;
        if (os_glGetShaderInfoLog)
            os_glGetShaderInfoLog(p, (int)sizeof(lp) - 1, &n3, lp);
        char b[280];
        snprintf(b, sizeof(b), "rung78: LINK FAIL | %.200s", lp);
        ep_log(b);
        g_r78_prog = -1;
        return 0;
    }
    {
        char b[128];
        snprintf(b, sizeof(b),
            "rung78: COMPILED+LINKED prog=%u (vs=%u fs=%u)", p, v, f);
        ep_log(b);
    }
    g_r78_prog = (int)p;
    return (int)p;
}

/* The gl-call shims the clear forward uses (bound to the dlopen'd
 * lib's dispatch — resolved lazily above; direct declarations are
 * gone with the hard link). */
static void glClearColor_shim(float r, float g, float b, float a)
{ if (os_glClearColor) os_glClearColor(r, g, b, a); }
static void glClear_shim(unsigned m) { if (os_glClear) os_glClear(m); }
static void glFinish_shim(void) { if (os_glFinish) os_glFinish(); }

/* RUNG 39 — the saved window triple + the lazy bounds query */
static unsigned g_sid_cid = 0, g_sid_wid = 0, g_sid_sid = 0;
static int g_bounds_locked = 0;
/* g_fb_w/g_fb_h defined with the rung-64d heap hunt (top of file) */
static void virgl_set_window_target(int w, int h);   /* fwd (lazy bounds call) */

/* RUNG 40 — THE GA BIND WIRE (the milestone-2 machinery, integrated):
 * instantiate the GA CFPlugin from the framebuffer service, Start it
 * (the env gate), AllocateSurface(kIOBlitHasCGSSurface, sid) — the
 * bind that gives the raw CGS surface its geometry — and LockSurface
 * for the dims (and, later, the presentation view). */
static void* g_ga_obj = NULL;
static IOGraphicsAcceleratorInterface* g_ga_vt = NULL;
static IOBlitSurface g_ga_surf;
static vm_address_t g_ga_view = 0;

static io_connect_t g_surf_conn = MACH_PORT_NULL;   /* the type-0 client (kept open) */

static int ga_bind_surface(unsigned sid)
{
    if (g_ga_vt) return 0;                    /* once per process */
    setenv("VM_GA_PROBE", "1", 1);            /* the plugin's Start gate */
    /* RUNG 41 — THE REGISTRATION: our sid must be in the kernel's
     * cross-client registry (the ONLY add site is setIDMode,
     * VMAccelSurfaceClient.cpp:661). Open the surface client (type 0,
     * gated by vm-accel-surface=1 — on) and call SetIDMode: user
     * selector 0x83 (enum index 7 + 0x7C, verified across six table
     * rows), scalars (wID, modebits); 0xA = BGRA32 (bpp 4). */
    {
        CFMutableDictionaryRef am = IOServiceMatching("VMQemuVGAAccelerator");
        io_service_t asvc = am ? IOServiceGetMatchingService(kIOMasterPortDefault, am)
                               : IO_OBJECT_NULL;
        if (asvc == IO_OBJECT_NULL) {
            ep_log("rung41: no accelerator service");
        } else {
            kern_return_t kr = IOServiceOpen(asvc, mach_task_self(), 0,
                                             &g_surf_conn);
            IOObjectRelease(asvc);
            if (kr != KERN_SUCCESS) {
                char b[80]; snprintf(b, sizeof(b), "rung41: type-0 open FAIL 0x%x", kr);
                ep_log(b);
            } else {
                /* CORRECTION: the direct user-client call uses the TABLE
                 * INDEX (the eIOAccelSurfaceMethods enum), not the 0x8x
                 * worked-example notes — 0x83 never reached the handler
                 * (0xe00002c7, no kernel line). SetIDMode = index 7. */
                uint64_t sm[2] = { sid, 0xA };
                kr = IOConnectCallMethod(g_surf_conn, 7,
                                         sm, 2, NULL, 0,
                                         NULL, NULL, NULL, NULL);
                char b[96];
                snprintf(b, sizeof(b),
                         "rung41: SetIDMode(idx 7) sid=0x%x mode=0xA -> 0x%x",
                         sid, kr);
                ep_log(b);
                /* RUNG 42 — THE SHAPE: setShape (index 9; 2 scalars
                 * (options, fbIndex) + the IOAccelDeviceRegion struct-in:
                 * {u32 num_rects; i16 x,y,w,h}) stores width/height into
                 * the registry's surface — the fields the 2D bind reads
                 * (the 0x0 bpp=4 row=0 bind was the shapeless surface).
                 * Bounds source: CGSGetWindowBounds(cid, wid). */
                {
                    void* cg2 = dlopen(
                        "/System/Library/Frameworks/ApplicationServices.framework/"
                        "Frameworks/CoreGraphics.framework/Versions/A/CoreGraphics",
                        RTLD_LAZY);
                    if (cg2) {
                        typedef int (*gwb_t)(unsigned, unsigned, void*);
                        gwb_t gwb = (gwb_t)dlsym(cg2, "CGSGetWindowBounds");
                        if (gwb) {
                            double wb[4] = { 0, 0, 0, 0 };
                            int wr = gwb(g_sid_cid, g_sid_wid, wb);
                            struct { uint32_t n; int16_t x, y, w, h; } rgn;
                            rgn.n = 0;
                            rgn.x = (int16_t)wb[0]; rgn.y = (int16_t)wb[1];
                            rgn.w = (int16_t)wb[2]; rgn.h = (int16_t)wb[3];
                            /* The stores gate on the IdentityScaleBit
                             * (kIOAccelSurfaceShapeIdentityScaleBit =
                             * 0x4, IOAccelSurfaceConnect.h:141) —
                             * options=0 leaves geometry untouched. */
                            uint64_t ss[2] = { 0x4 /*IdentityScale*/, 0 };
                            kern_return_t kr2 = IOConnectCallMethod(
                                g_surf_conn, 9, ss, 2,
                                &rgn, sizeof(rgn),
                                NULL, NULL, NULL, NULL);
                            char b2[128];
                            snprintf(b2, sizeof(b2),
                                     "rung42: GetWindowBounds -> %d [%g %g %g %g]; "
                                     "SetShape(9) %dx%d -> 0x%x",
                                     wr, wb[0], wb[1], wb[2], wb[3],
                                     (int)rgn.w, (int)rgn.h, kr2);
                            ep_log(b2);
                            /* RUNG 43 — THE WRITE-LOCK (the lock's
                             * named requirement: "backing not yet created
                             * (no WindowServer write-lock)"). writeLockSurface
                             * (index 14, StructO-only) LAZY-CREATES the
                             * backing at first lock; our surface passes
                             * every gate (id, geometry, bpp, base extent).
                             * After this, the GA lock maps the backing. */
                            if (kr2 == KERN_SUCCESS && rgn.w > 0) {
                                uint8_t info[128];
                                size_t isz = sizeof(info);
                                kern_return_t kr3 = IOConnectCallMethod(
                                    g_surf_conn, 14,
                                    NULL, 0, NULL, 0,
                                    NULL, 0, info, &isz);
                                uint64_t* iw = (uint64_t*)info;
                                uint32_t* iu = (uint32_t*)(info + 32);
                                char b3[144];
                                snprintf(b3, sizeof(b3),
                                         "rung43: WriteLock(14) -> 0x%x "
                                         "addr=0x%llx row=%u %ux%u",
                                         kr3,
                                         (unsigned long long)iw[0],
                                         iu[0], iu[1], iu[2]);
                                ep_log(b3);
                                /* THE WINDOW TARGET GOES LIVE: the lock's
                                 * info carries the true dims (iu[1]=width,
                                 * iu[2]=height). */
                                if (kr3 == KERN_SUCCESS && iu[1] > 0 && iu[2] > 0) {
                                    virgl_set_window_target(iu[1], iu[2]);
                                    g_bounds_locked = 1;
                                }
                            }
                        }
                    }
                }
                /* the client stays OPEN — its m_surface is the registry
                 * entry; close = stop = unregister */
            }
        }
    }
    CFMutableDictionaryRef m = IOServiceMatching("VMVirtIOFramebuffer");
    if (!m) { ep_log("rung40: FB matching FAILED"); return -1; }
    io_service_t fb = IOServiceGetMatchingService(kIOMasterPortDefault, m);
    if (fb == IO_OBJECT_NULL) { ep_log("rung40: no FB service"); return -1; }
    IOCFPlugInInterface** plug = NULL;
    SInt32 score = 0;
    kern_return_t kr = IOCreatePlugInInterfaceForService(
        fb, kIOGraphicsAcceleratorTypeID,
        kIOGraphicsAcceleratorInterfaceID, &plug, &score);
    IOObjectRelease(fb);
    if (kr != KERN_SUCCESS || !plug) {
        char b[80]; snprintf(b, sizeof(b), "rung40: plugin instantiate FAIL 0x%x", kr);
        ep_log(b); return -1;
    }
    HRESULT q = (*plug)->QueryInterface(
        plug, CFUUIDGetUUIDBytes(kIOGraphicsAcceleratorInterfaceID),
        (LPVOID*)&g_ga_obj);
    (*plug)->Release(plug);   /* per the probe's shape: the object holds itself */
    if (q != S_OK || !g_ga_obj) {
        ep_log("rung40: QueryInterface refused");
        return -1;
    }
    g_ga_vt = *(IOGraphicsAcceleratorInterface***)g_ga_obj;
    IOReturn r = g_ga_vt->Probe(g_ga_obj, NULL, fb, &score);
    if (r == kIOReturnSuccess)
        r = g_ga_vt->Start(g_ga_obj, NULL, fb);
    if (r != kIOReturnSuccess) {
        char b[80]; snprintf(b, sizeof(b), "rung40: GA Start FAIL 0x%x", r);
        ep_log(b); return -1;
    }
    memset(&g_ga_surf, 0, sizeof(g_ga_surf));
    g_ga_surf.pixelFormat = kIO32BGRAPixelFormat;
    r = g_ga_vt->AllocateSurface(g_ga_obj, kIOBlitHasCGSSurface,
                                 &g_ga_surf, (void*)(uintptr_t)sid);
    if (r != kIOReturnSuccess) {
        char b[96];
        snprintf(b, sizeof(b), "rung40: AllocateSurface(sid=0x%x) FAIL 0x%x", sid, r);
        ep_log(b); return -1;
    }
    r = g_ga_vt->LockSurface(g_ga_obj, 0, &g_ga_surf, &g_ga_view);
    char b[128];
    snprintf(b, sizeof(b),
             "rung40: GA BOUND+LOCKED sid=0x%x -> 0x%x view=0x%lx %ux%u rowBytes=%u",
             sid, r, (unsigned long)g_ga_view,
             (unsigned)g_ga_surf.size.width, (unsigned)g_ga_surf.size.height,
             g_ga_surf.rowBytes);
    ep_log(b);
    if (r != kIOReturnSuccess || !g_ga_view) return -1;
    return 0;
}

static void virgl_query_window_bounds(void)
{
    if (g_bounds_locked || !g_sid_cid) return;
    /* RUNG 40: the GA bind FIRST — the geometry lives behind it
     * (the raw CGS surface has no bounds until AllocateSurface binds
     * it; LockSurface fills the dims). On success the CGS query is
     * unnecessary. */
    if (ga_bind_surface(g_sid_sid) == 0) {
        int w = (int)g_ga_surf.size.width, h = (int)g_ga_surf.size.height;
        if (w > 0 && h > 0) {
            virgl_set_window_target(w, h);
            g_bounds_locked = 1;
            return;
        }
    }
    void* cgh = dlopen(
        "/System/Library/Frameworks/ApplicationServices.framework/"
        "Frameworks/CoreGraphics.framework/Versions/A/CoreGraphics",
        RTLD_LAZY);
    if (!cgh) return;
    typedef int (*gsb_t)(unsigned, unsigned, unsigned, void*);
    gsb_t gsb = (gsb_t)dlsym(cgh, "CGSGetSurfaceBounds");
    if (!gsb) return;
    double rect[4] = { 0, 0, 0, 0 };
    int r = gsb(g_sid_cid, g_sid_wid, g_sid_sid, rect);
    int w = (int)rect[2], h = (int)rect[3];
    char b[128];
    snprintf(b, sizeof(b),
             "rung39: CGSGetSurfaceBounds(0x%x,0x%x,0x%x) -> %d rect=[%g %g %g %g] %dx%d",
             g_sid_cid, g_sid_wid, g_sid_sid, r,
             rect[0], rect[1], rect[2], rect[3], w, h);
    ep_log(b);
    if (r == 0 && w > 0 && h > 0) {
        virgl_set_window_target(w, h);
        g_bounds_locked = 1;   /* locked — the target is the window */
    }
}

/* RUNG 38 — the readback resources: one 4x4 B8G8R8A8 render
 * target per process, created+backed on first use. */
static uint32_t g_fb_res = 0;
static uint32_t g_depth_res = 0;             /* rung 51: D24S8 target */
static unsigned char* g_fb_backing = NULL;   /* 4*4*4 = 64 bytes */
static const unsigned char kProofColor[4] = { 0x40, 0x80, 0xBF, 0xFF }; /* B,G,R,A = .25/.5/.75/1 */

/* RUNG 39 — retarget at the real window dims (from
 * CGSGetSurfaceBounds at attach). Recreates the resource at
 * window size; the clear and the proof follow. */
static void virgl_set_window_target(int w, int h)
{
    if (g_fb_res && w == g_fb_w && h == g_fb_h) return;
    g_fb_w = w; g_fb_h = h;
    if (g_fb_res) {
        /* window resized: drop the old target (simplest correct
         * path — unref and recreate on next use) */
        uint64_t un[1] = { g_fb_res };
        IOConnectCallMethod(g_virgl_conn, 0x6005, un, 1, NULL, 0,
                            NULL, NULL, NULL, NULL);
        free(g_fb_backing);
        g_fb_res = 0; g_fb_backing = NULL;
    }
    char b[80];
    snprintf(b, sizeof(b), "rung39: window target %dx%d (pending create)", w, h);
    ep_log(b);
}

static int virgl_ensure_fb(void)
{
    if (g_fb_res) return 0;
    if (virgl_transport_init() < 0) return -1;
    uint64_t in[11] = {
        g_virgl_ctx, 2 /*PIPE_TEXTURE_2D*/, 1 /*B8G8R8A8_UNORM*/,
        2 /*VIRGL_BIND_RENDER_TARGET, virgl_hw.h:595 — 0x4 was the
          host-rejected SAMPLER class; the debug log named it:
          "Invalid texture bind flags 0x4"*/,
        (uint64_t)g_fb_w, (uint64_t)g_fb_h, 1, 0, 0, 0, 0
    };
    uint64_t out[1] = { 0 }; uint32_t out_cnt = 1;
    kern_return_t kr = IOConnectCallMethod(g_virgl_conn, 0x6002,
                                           in, 11, NULL, 0,
                                           out, &out_cnt, NULL, NULL);
    if (kr != KERN_SUCCESS) {
        char b[64]; snprintf(b, sizeof(b), "rung38: 0x6002 createRes FAIL 0x%x", kr);
        ep_log(b); return -1;
    }
    g_fb_res = (uint32_t)out[0];
    size_t sz = (size_t)g_fb_w * g_fb_h * 4;
    g_fb_backing = (unsigned char*)calloc(1, sz);
    uint64_t addr = (uint64_t)(uintptr_t)g_fb_backing;
    uint64_t ab[5] = { g_fb_res,
                       (uint32_t)(addr & 0xFFFFFFFFull), (uint32_t)(addr >> 32),
                       (uint32_t)(sz & 0xFFFFFFFFull), (uint32_t)(sz >> 32) };
    kr = IOConnectCallMethod(g_virgl_conn, 0x6003, ab, 5, NULL, 0,
                             NULL, NULL, NULL, NULL);
    if (kr != KERN_SUCCESS) {
        char b[64]; snprintf(b, sizeof(b), "rung38: 0x6003 attach FAIL 0x%x", kr);
        ep_log(b); return -1;
    }
    /* 0x6009 ctxAttachResource — REQUIRED before SET_FRAMEBUFFER_STATE
     * can reference a surface built on this resource (the winsys's own
     * comment, LEDGER commit 6d9a278; re-derived 2026-08-22 when the
     * proof readback returned zeros with every transport call clean). */
    uint64_t ca[2] = { g_virgl_ctx, g_fb_res };
    kr = IOConnectCallMethod(g_virgl_conn, 0x6009, ca, 2, NULL, 0,
                             NULL, NULL, NULL, NULL);
    char b[96];
    snprintf(b, sizeof(b),
             "rung38/39: fb res %u created+backed+ctxAttached %dx%d (0x6009 -> 0x%x)",
             g_fb_res, g_fb_w, g_fb_h, kr);
    ep_log(b);
    /* RUNG 51 — THE DEPTH SURFACE: D24S8 =
     * VIRGL_FORMAT_Z24_UNORM_S8_UINT (19), bind =
     * VIRGL_BIND_DEPTH_STENCIL (1<<0 = 1 — bit 0, the bind
     * whose absence the color rung's 0x4 mistake taught).
     * Created+backed+attached alongside the color target;
     * the SET_FB carries it as zsurf. */
    {
        /* The format: 16 (Z16_UNORM) — MESA'S OWN CHOICE in the
         * rung-38 captured stream, and the PROVEN one here: fmt 19
         * (D24S8) tripped vrend's "Illegal resource" at the depth
         * surface create (the resource create returned 0x1100 but
         * the surface lookup failed — the whole batch then aborted,
         * zeroing color too). Z16 decodes, executes, and the color
         * proof passes with the depth surface bound. GLD_DEPTH_FMT
         * overrides for the D24S8 retry. */
        unsigned dfmt = 16;
        const char* e = getenv("GLD_DEPTH_FMT");
        if (e) dfmt = (unsigned)strtoul(e, 0, 0);
        uint64_t dz[11] = { g_virgl_ctx, 2 /*2D*/, dfmt,
                            1 /*BIND_DEPTH_STENCIL*/,
                            (uint64_t)g_fb_w, (uint64_t)g_fb_h,
                            1, 0, 0, 0, 0 };
        uint64_t dout[1] = { 0 }; uint32_t dcnt = 1;
        kr = IOConnectCallMethod(g_virgl_conn, 0x6002,
                                 dz, 11, NULL, 0,
                                 dout, &dcnt, NULL, NULL);
        if (kr != KERN_SUCCESS) {
            ep_log("rung51: depth res create FAIL"); return -1;
        }
        g_depth_res = (uint32_t)dout[0];
        size_t dsz = (size_t)g_fb_w * g_fb_h * 4;   /* D24S8 = 4 bytes */
        unsigned char* dbuf = (unsigned char*)calloc(1, dsz);
        uint64_t daddr = (uint64_t)(uintptr_t)dbuf;
        uint64_t dab[5] = { g_depth_res,
                            (uint32_t)(daddr & 0xFFFFFFFFull),
                            (uint32_t)(daddr >> 32),
                            (uint32_t)(dsz & 0xFFFFFFFFull),
                            (uint32_t)(dsz >> 32) };
        kr = IOConnectCallMethod(g_virgl_conn, 0x6003, dab, 5, NULL, 0,
                                 NULL, NULL, NULL, NULL);
        uint64_t dca[2] = { g_virgl_ctx, g_depth_res };
        kr = IOConnectCallMethod(g_virgl_conn, 0x6009, dca, 2, NULL, 0,
                                 NULL, NULL, NULL, NULL);
        char b2[112];
        snprintf(b2, sizeof(b2),
                 "rung51: depth res %u D24S8 created+backed+ctxAttached "
                 "%ux%u (0x6009 -> 0x%x)",
                 g_depth_res, g_fb_w, g_fb_h, kr);
        ep_log(b2);
    }
    return 0;
}

/* submit SET_FRAMEBUFFER_STATE + CLEAR (the distinctive proof
 * color). RUNG 39: a FRESH surface handle per submit (recreating
 * an existing handle in vrend's object table is undefined). */
static unsigned g_surf_handle = 0;
/* RUNG 49 — the app's own clear color: the ENGINE mirrors RGBA
 * floats at [ctx+0x740]+0x2ea0 (the float's gldClear reads exactly
 * there, grf64 0x12b91-0x12ba9). Read at each clear; the proof
 * floats remain the fallback. */
static float g_clear_rgba[4] = { 0.25f, 0.5f, 0.75f, 1.0f };
static void virgl_read_app_clear_color(void* ctx)
{
    if (!ctx) return;
    void* shared = *(void**)((char*)ctx + 0x740);
    if (!shared) return;
    float* c = (float*)((char*)shared + 0x2ea0);
    g_clear_rgba[0] = c[0]; g_clear_rgba[1] = c[1];
    g_clear_rgba[2] = c[2]; g_clear_rgba[3] = c[3];
}
static int virgl_submit_fb_clear(unsigned pipe_mask)
{
    unsigned sh = ++g_surf_handle;
    unsigned zsh = ++g_surf_handle;   /* rung 51: the depth surface */
    /* the app's color (rung 49) — raw float bits into the blob */
    uint32_t cr, cg, cb, ca;
    memcpy(&cr, &g_clear_rgba[0], 4);
    memcpy(&cg, &g_clear_rgba[1], 4);
    memcpy(&cb, &g_clear_rgba[2], 4);
    memcpy(&ca, &g_clear_rgba[3], 4);
    /* RUNG 51: the batch grows the depth-surface object (6 dwords)
     * and the SET_FB carries zsurf. 25 dwords total.
     * GLD_NO_ZSURF=1: the black-screen bisect — everything created,
     * the depth object still sent, but SET_FB zsurf=0 (splits
     * "the binding breaks the color clear" from "the object does"). */
    int no_zsurf = getenv("GLD_NO_ZSURF") != NULL;
    /* GLD_NO_DEPTH=1: the FULL neuter — the pre-rung-51 batch shape
     * (19 dwords, no depth object, zsurf=0). Splits "the depth
     * addition breaks the color clear" from "the reboot's 1680x1050
     * desktop changed something else". */
    if (getenv("GLD_NO_DEPTH")) {
        uint32_t old[19] = {
            0x00050801u, sh, g_fb_res, 1u, 0u, 0u,
            0x00030005u, 1u, 0u, sh,
            0x00080007u, pipe_mask,
            cr, cg, cb, ca,
            0u, 0x3FF00000u, 0u
        };
        uint32_t oframe[3 + 19] = { 0x31454346u, 1, g_fb_res };
        for (int i = 0; i < 19; i++) oframe[3 + i] = old[i];
        uint64_t scalar0 = g_virgl_ctx;
        kern_return_t kr0 = IOConnectCallMethod(
            g_virgl_conn, 0x6008, &scalar0, 1, oframe, sizeof(oframe),
            NULL, NULL, NULL, NULL);
        return (kr0 == KERN_SUCCESS) ? 0 : -1;
    }
    uint32_t blob[25] = {
        /* CREATE_OBJECT color surface (fresh handle) */
        0x00050801u, sh, g_fb_res, 1u, 0u, 0u,
        /* CREATE_OBJECT depth surface (fresh handle, fmt 19) */
        0x00050801u, zsh, g_depth_res, 19u, 0u, 0u,
        /* SET_FRAMEBUFFER_STATE: nr=1, zsurf (0 under GLD_NO_ZSURF), cbuf */
        0x00030005u, 1u, no_zsurf ? 0u : zsh, sh,
        /* CLEAR — the APP's color (rung 49) */
        0x00080007u, pipe_mask,
        cr, cg, cb, ca,
        0u, 0x3FF00000u, 0u
    };
    /* RUNG 38 fix 3: the FCE1 frame must DECLARE the resource
     * (cres=1) — the fence era's design: the kext tracks last_seq
     * per listed resource, making the 0x600B wait real. cres=0 left
     * the wait vacuous (nothing tracked), racing the async batch. */
    uint32_t frame[3 + 25] = { 0x31454346u, 1, g_fb_res };
    for (int i = 0; i < 25; i++) frame[3 + i] = blob[i];
    uint64_t scalar = g_virgl_ctx;
    kern_return_t kr = IOConnectCallMethod(g_virgl_conn, 0x6008,
                                           &scalar, 1, frame, sizeof(frame),
                                           NULL, NULL, NULL, NULL);
    return (kr == KERN_SUCCESS) ? 0 : -1;
}

static void gld_clear_real(void* ctx, unsigned mask,
                           void* a2, void* a3, void* a4, void* a5)
{
    (void)ctx; (void)a2; (void)a3; (void)a4; (void)a5;
    virgl_read_app_clear_color(ctx);   /* RUNG 49: the app's color */
    if (++g_clear_count <= 5 || (g_clear_count % 500) == 0) {
        char b[112];
        snprintf(b, sizeof(b),
                 "CLEAR-REAL #%ld mask=0x%x color %f %f %f %f (rung 49)",
                 g_clear_count, mask,
                 g_clear_rgba[0], g_clear_rgba[1],
                 g_clear_rgba[2], g_clear_rgba[3]);
        ep_log(b);
    }
    virgl_query_window_bounds();   /* lazy: the surface is ordered by now */
    if (virgl_ensure_fb() < 0) return;
    /* RUNG 38 fix 4 — THE MESA STREAM DIFF's first finding: Mesa's
     * color-clear mask is 4 (captured: 00080007 00000004 with
     * clearA .2/.4/.6). Mask 1 clears DEPTH — into a framebuffer with
     * no depth surface: a no-op. THAT was the zero content. */
    uint32_t pipe_mask = ((mask & 0x4000u) ? 0x4u : 0u)
                       | ((mask & 0x0100u) ? 0x2u : 0u)
                       | ((mask & 0x0400u) ? 0x1u : 0u);
    if (virgl_submit_fb_clear(pipe_mask) < 0 && g_clear_count <= 5)
        ep_log("  fb+clear submit FAILED");
    /* RUNG 55 — THE MESA FORWARD: the same clear through OSMesa's
     * full stack (state tracker → virgl → OUR transport → host →
     * readback into the private buffer). The linkage proof: the
     * private buffer reads the app's color — Mesa-rendered. */
    if (g_clear_count <= 3 &&
        osmesa_link_init(g_fb_w, g_fb_h) == 0) {
        glClearColor_shim(g_clear_rgba[0], g_clear_rgba[1],
                          g_clear_rgba[2], g_clear_rgba[3]);
        glClear_shim(0x4000 /*GL_COLOR_BUFFER_BIT*/);
        glFinish_shim();
        /* RUNG 57 — the routing probe: the linked lib's glGetString
         * answers Mesa's renderer iff our calls route through Mesa's
         * dispatch; anything else = the calls bypass Mesa. */
        {
            const unsigned char* s2 = os_glGetString ? os_glGetString(0x1F01) : NULL;
            char rb3[112];
            snprintf(rb3, sizeof(rb3),
                     "rung57: linked glGetString(GL_RENDERER) = \"%s\"",
                     s2 ? (const char*)s2 : "(NULL)");
            ep_log(rb3);
        }
        /* RUNG 57 — THE READBACK PATH apps actually use: linked-lib
         * glReadPixels (st read_pixels -> transfer_from_host -> these
         * bytes) — bypasses flush_front entirely. 2x2 from (0,0). */
        unsigned char rb_px[16];
        if (os_glReadPixels)
            os_glReadPixels(0, 0, 2, 2, 0x1908, 0x1401, rb_px);
        char ob[96]; int oo = 0;
        for (int i = 0; i < 8 && oo < 40; i++)
            oo += snprintf(ob + oo, sizeof(ob) - oo, "%02x",
                           rb_px[i]);
        char b4[128];
        snprintf(b4, sizeof(b4),
                 "rung55: OSMesa clear DONE — private buffer[0..7]: %s",
                 ob);
        ep_log(b4);
    }
}

/* THE READBACK SLOT (+0x10) — the proof instrument: wait the
 * resource, TRANSFER_FROM_HOST, byte-compare against the proof
 * color. THE ROUND TRIP: device pixels, not log lines. */
static long g_readpix_count = 0;
static void gld_readpixels_real(void* ctx, void* a1, void* a2, void* a3,
                                void* a4, void* a5)
{
    (void)ctx; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    if (++g_readpix_count > 3) return;   /* the proof runs thrice, no flood */
    ep_log("READPIXELS-REAL (rung 38) — the readback proof");
    if (virgl_ensure_fb() < 0) return;
    if (virgl_submit_fb_clear(0x1) < 0) {
        ep_log("  proof clear submit FAILED"); return;
    }
    /* fence-era wait: 0x600B (res, flags) */
    uint64_t w_in[2] = { g_fb_res, 0 };
    kern_return_t kr = IOConnectCallMethod(g_virgl_conn, 0x600B,
                                           w_in, 2, NULL, 0,
                                           NULL, NULL, NULL, NULL);
    char b[64]; snprintf(b, sizeof(b), "  0x600B wait -> 0x%x", kr);
    ep_log(b);
    /* transfer_from at WINDOW size (rung 39): res, level, x,y,z,
     * w,h,d, ctx, stride, lstride, off */
    uint32_t stride = (uint32_t)g_fb_w * 4;
    uint64_t in[12] = { g_fb_res, 0, 0,0,0,
                        (uint64_t)g_fb_w, (uint64_t)g_fb_h, 1,
                        g_virgl_ctx, stride,
                        (uint64_t)stride * g_fb_h, 0 };
    kr = IOConnectCallMethod(g_virgl_conn, 0x3009,
                             in, 12, NULL, 0, NULL, NULL, NULL, NULL);
    snprintf(b, sizeof(b), "  0x3009 transfer_from (%dx%d) -> 0x%x",
             g_fb_w, g_fb_h, kr);
    ep_log(b);
    /* the verdict — RUNG 49: the expected bytes are COMPUTED from
     * the app's clear color (the same floats the clear submitted).
     * Layout (rung 38's observation): B,G,R,A per byte on a
     * B8G8R8A8 resource. Corners + center of the WINDOW. */
    unsigned char kObserved[4];
    {
        float b_c = g_clear_rgba[2], g_c = g_clear_rgba[1],
              r_c = g_clear_rgba[0], a_c = g_clear_rgba[3];
        if (b_c < 0) b_c = 0; if (b_c > 1) b_c = 1;
        if (g_c < 0) g_c = 0; if (g_c > 1) g_c = 1;
        if (r_c < 0) r_c = 0; if (r_c > 1) r_c = 1;
        if (a_c < 0) a_c = 0; if (a_c > 1) a_c = 1;
        kObserved[0] = (unsigned char)(b_c * 255.0f + 0.5f);
        kObserved[1] = (unsigned char)(g_c * 255.0f + 0.5f);
        kObserved[2] = (unsigned char)(r_c * 255.0f + 0.5f);
        kObserved[3] = (unsigned char)(a_c * 255.0f + 0.5f);
    }
    size_t last = (size_t)g_fb_w * g_fb_h - 1;
    size_t mid  = ((size_t)g_fb_h / 2) * g_fb_w + g_fb_w / 2;
    int match = 1;
    size_t probes[3] = { 0, mid, last };
    for (int p = 0; p < 3; p++)
        for (int c = 0; c < 4; c++)
            if (g_fb_backing[probes[p]*4+c] != kObserved[c]) { match = 0; break; }
    char bytes[128];
    int o = 0;
    for (int i = 0; i < 16 && o < 100; i++)
        o += snprintf(bytes + o, sizeof(bytes) - o, "%02x", g_fb_backing[i]);
    snprintf(b, sizeof(b), "  backing[0..15]: %s", bytes);
    ep_log(b);
    ep_log(match
           ? "  *** ROUND TRIP PROVEN AT WINDOW SIZE: corners+center == proof color ***"
           : "  readback MISMATCH at window size");
    /* RUNG 51 continuation — THE DEPTH READBACK: where did the clear
     * land? Green bytes here = roles swapped (color into the depth
     * target); 0xFFFFFF00-ish = depth cleared to 1.0 with color
     * skipped; zeros = nothing cleared. */
    if (g_depth_res) {
        unsigned char* dbuf = calloc(1, (size_t)g_fb_w * g_fb_h * 4);
        uint64_t daddr = (uint64_t)(uintptr_t)dbuf;
        uint64_t dab[5] = { g_depth_res,
                            (uint32_t)(daddr & 0xFFFFFFFFull),
                            (uint32_t)(daddr >> 32),
                            (uint32_t)((uint64_t)g_fb_w * g_fb_h * 4 & 0xFFFFFFFFull),
                            (uint32_t)(((uint64_t)g_fb_w * g_fb_h * 4) >> 32) };
        IOConnectCallMethod(g_virgl_conn, 0x6003, dab, 5, NULL, 0,
                            NULL, NULL, NULL, NULL);
        uint64_t din[12] = { g_depth_res, 0, 0,0,0,
                             (uint64_t)g_fb_w, (uint64_t)g_fb_h, 1,
                             g_virgl_ctx, (uint64_t)g_fb_w * 4,
                             (uint64_t)g_fb_w * g_fb_h * 4, 0 };
        kr = IOConnectCallMethod(g_virgl_conn, 0x3009,
                                 din, 12, NULL, 0, NULL, NULL, NULL, NULL);
        char db[128]; int do_ = 0;
        for (int i = 0; i < 16 && do_ < 60; i++)
            do_ += snprintf(db + do_, sizeof(db) - do_, "%02x", dbuf[i]);
        char b6[160];
        snprintf(b6, sizeof(b6),
                 "rung51: DEPTH readback (res %u, xfer 0x%x): %s",
                 g_depth_res, kr, db);
        ep_log(b6);
        free(dbuf);
    }
    /* RUNG 44 — THE PRESENTATION WRITE: the kernel's relay present
     * (0x600C hostRelayBlit, FB/VMVirtIOGPU.cpp:8037). Its GA branch
     * re-reads this resource kernel-side, row-copies into the
     * GA-BOUND surface backing (stride 2080 — the extent rule lives
     * there), flushes the shape rect into the desktop backing, and
     * pushes the rect to the host scanout. Contract honored: the
     * 0x600B fence wait above. scalars {res, x, y, w, h} — the GA
     * branch uses w,h (row copy); x,y belong to the non-GA
     * fallback. */
    {
        uint64_t rb[5] = { g_fb_res, 0, 0,
                           (uint64_t)g_fb_w, (uint64_t)g_fb_h };
        kr = IOConnectCallMethod(g_virgl_conn, 0x600C,
                                 rb, 5, NULL, 0,
                                 NULL, NULL, NULL, NULL);
        char b2[80];
        snprintf(b2, sizeof(b2),
                 "rung44: 0x600C hostRelayBlit res=%u %ux%u -> 0x%x",
                 g_fb_res, g_fb_w, g_fb_h, kr);
        ep_log(b2);
        /* RUNG 44 CONTINUATION — the black-rect discriminator: the
         * surface backing is mapped in THIS process (g_ga_view).
         * (A) rows zero -> the relay's surface write never happened
         * (its silent readBytes/writeBytes break); (B) rows blue ->
         * steps 1-2 fine, the divergence is the flush's source. */
        if (g_ga_view && g_ga_surf.rowBytes) {
            unsigned char* v = (unsigned char*)g_ga_view;
            unsigned rb = g_ga_surf.rowBytes;
            char vb[640]; int o = 0;
            /* RUNG 45 sweep: hunt the write ANYWHERE in the
             * allocation (rows 0..849 step 32, first 4 bytes).
             * Pre-fix the blue sat at r0..261; post-fix it should
             * move to r588..849. All-zero = the relay's row loop
             * never wrote (its silent break — next rung logs it). */
            int hits = 0, first_hit = -1;
            unsigned maxrow = 850;
            for (unsigned row = 0; row < maxrow && o < 600; row += 32) {
                unsigned nz = 0;
                for (int c = 0; c < 4; c++)
                    if (v[(size_t)row * rb + c]) { nz = 1; break; }
                if (nz) {
                    hits++;
                    if (first_hit < 0) first_hit = (int)row;
                    o += snprintf(vb + o, sizeof(vb) - o, " r%u:", row);
                    for (int c = 0; c < 8 && o < 620; c++)
                        o += snprintf(vb + o, sizeof(vb) - o, "%02x",
                                      v[(size_t)row * rb + c]);
                }
            }
            char b3[704];
            snprintf(b3, sizeof(b3),
                     "rung45: VIEW sweep (stride %u, step 32) hits=%d "
                     "first=%d%s", rb, hits, first_hit,
                     hits ? vb : " — ALLOCATION ALL ZERO");
            ep_log(b3);
            /* THE PRECISE READ: the write starts at byte 800 of row
             * 588 (shape_off = 588*2080 + 200*4 — the X offset; the
             * sweep above probes bytes 0..3 of each row and CANNOT
             * see a window starting at column 200). */
            {
                size_t off = (size_t)588 * rb + 200 * 4;
                char b4[96]; int o4 = 0;
                for (int c = 0; c < 16 && o4 < 60; c++)
                    o4 += snprintf(b4 + o4, sizeof(b4) - o4, "%02x",
                                   v[off + c]);
                char b5[128];
                snprintf(b5, sizeof(b5),
                         "rung45: VIEW at shape_off (r588+800): %s", b4);
                ep_log(b5);
            }
        }
    }
}

/* RUNG 46 — THE SWAP ENTRY: what CGLFlushDrawable tail-calls
 * ([engine+0x66b0], driver ctx in rdi). The presentation, at the
 * app's own flush: the same chain the proof runs — ensure_fb,
 * submit the clear (color mask 4), fence-wait (0x600B, the relay's
 * contract), relay present (0x600C). */
static long g_swap_count = 0;
long gld_swap_entry(void* dctx, void* a1, void* a2, void* a3,
                    void* a4, void* a5)
{
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    if (++g_swap_count > 5 && (g_swap_count % 500) != 0) return 0;
    {
        static int s_heap_left = 2;
        if (s_heap_left > 0) {
            int pn = 3 - s_heap_left--;
            heap_hunt_pass(pn);
            vm_hunt_pass(pn);
        }
    }
    /* RUNG 64 — WHERE IS THE ENGINE'S FRAME? The GA lock mapped the
     * drawable surface in the 0x105_000000 band (LockSurface
     * addr=0x105100000 row=3600, kernel log). Scan the gli ctx and
     * its +0x740 shared block for pointers into that band; dump 16
     * bytes at the FIRST hit — clear-color bytes mean nothing but our
     * relay ever wrote there; VARIED bytes mean the engine's frames. */
    {
        static int s_scan_left = 3;
        if (dctx && s_scan_left > 0) {
            s_scan_left--;
            unsigned char* shared = *(unsigned char**)
                                    ((char*)dctx + 0x740);
            struct { const char* tag; unsigned char* base; size_t span; }
            regs[2] = {
                { "ctx",    (unsigned char*)dctx, 0x800 },
                { "shared", shared,               0x4000 },
            };
            int hits = 0;
            for (int r = 0; r < 2 && hits < 2; r++) {
                if (!regs[r].base) continue;
                for (size_t off = 0; off + 8 <= regs[r].span && hits < 2;
                     off += 8) {
                    uint64_t v;
                    memcpy(&v, regs[r].base + off, 8);
                    if (v >= 0x105000000ull && v < 0x106000000ull) {
                        char hb[80]; int ho = 0;
                        unsigned char* p = (unsigned char*)(uintptr_t)v;
                        for (int c = 0; c < 16 && ho < 60; c++)
                            ho += snprintf(hb + ho, sizeof(hb) - ho,
                                           "%02x", p[c]);
                        char sb[224];
                        snprintf(sb, sizeof(sb),
                                 "rung64: SCAN %s+0x%llx -> 0x%llx bytes:%s",
                                 regs[r].tag, (unsigned long long)off,
                                 (unsigned long long)v, hb);
                        ep_log(sb);
                        hits++;
                    }
                }
            }
            if (!hits)
                ep_log("rung64: SCAN — no 0x105-band pointer in ctx/shared");
            /* RUNG 64c — THE DRAWABLE OBJECT at ctx+0x218 (rung 33's
             * decode: the float computes buffer sizes from it — the
             * renderer-surface home our attach mirror skipped). Dump
             * its words; content-check every heap-band pointer it
             * holds (a raster buffer reads VARIED; objects read
             * pointers/zeros). */
            {
                unsigned char* drw = NULL;
                if (dctx) drw = *(unsigned char**)
                                ((char*)dctx + 0x218);
                if (drw && (uintptr_t)drw > 0x10000
                        && (uintptr_t)drw < 0x800000000000ull) {
                    char db[512]; int dO = 0;
                    dO += snprintf(db + dO, sizeof(db) - dO,
                                   "rung64: drawable[0x218 -> %llx]:",
                                   (unsigned long long)(uintptr_t)drw);
                    for (int i = 0; i < 24 && dO < 470; i++)
                        dO += snprintf(db + dO, sizeof(db) - dO,
                                       " %llx",
                                       *(unsigned long*)(drw + i * 8));
                    ep_log(db);
                    int content_dumps = 0;
                    for (size_t off = 0; off < 0xC0
                                        && content_dumps < 3;
                         off += 8) {
                        uint64_t v = *(unsigned long*)(drw + off);
                        if (v >= 0x100000000ull && v < 0x110000000ull
                            && (v & 0xFFF) == 0) {
                            char hb[80]; int ho = 0;
                            unsigned char* p =
                                (unsigned char*)(uintptr_t)v;
                            for (int c = 0; c < 16 && ho < 60; c++)
                                ho += snprintf(hb + ho,
                                               sizeof(hb) - ho,
                                               "%02x", p[c]);
                            char sb[224];
                            snprintf(sb, sizeof(sb),
                                     "rung64: drawable+0x%llx -> "
                                     "0x%llx bytes:%s",
                                     (unsigned long long)off,
                                     (unsigned long long)v, hb);
                            ep_log(sb);
                            content_dumps++;
                        }
                    }
                } else {
                    ep_log("rung64: drawable[0x218] absent");
                }
            }
            /* RUNG 64b — the engine blocks from the install entry
             * (&engine[0x65c8], &engine[0x66d0]): dump both, and
             * follow every app-heap-band pointer found in them with a
             * 16-byte content read (the engine's raster buffer shows
             * VARIED bytes; object headers show pointers). */
            for (int bi = 0; bi < 2; bi++) {
                unsigned char* blk = bi ? (unsigned char*)g_engine_block2
                                        : (unsigned char*)g_engine_block1;
                if (!blk && g_engine_subblock) {
                    /* derive from the create-time sub-block (the
                     * direct-install path never passes the blocks) */
                    unsigned char* eng = (unsigned char*)g_engine_subblock
                                        - 0x79b8;
                    blk = eng + (bi ? 0x66d0 : 0x65c8);
                }
                if (!blk) continue;
                char db[288]; int dO = 0;
                dO += snprintf(db + dO, sizeof(db) - dO,
                               "rung64: block%d[%llx]:", bi,
                               (unsigned long long)(uintptr_t)blk);
                for (int i = 0; i < 16 && dO < 250; i++)
                    dO += snprintf(db + dO, sizeof(db) - dO, " %llx",
                                   *(unsigned long*)(blk + i * 8));
                ep_log(db);
                for (size_t off = 0; off < 0x100; off += 8) {
                    uint64_t v = *(unsigned long*)(blk + off);
                    if (v >= 0x100000000ull && v < 0x110000000ull
                        && (v & 0xFFF) == 0) {
                        char hb[80]; int ho = 0;
                        unsigned char* p = (unsigned char*)(uintptr_t)v;
                        for (int c = 0; c < 16 && ho < 60; c++)
                            ho += snprintf(hb + ho, sizeof(hb) - ho,
                                           "%02x", p[c]);
                        char sb[208];
                        snprintf(sb, sizeof(sb),
                                 "rung64: block%d+0x%llx -> 0x%llx bytes:%s",
                                 bi, (unsigned long long)off,
                                 (unsigned long long)v, hb);
                        ep_log(sb);
                    }
                }
            }
        }
    }
    virgl_read_app_clear_color(dctx);   /* RUNG 49: the app's color */
    if (getenv("GLD_SWAP_RELAY_OFF")) {
        /* RUNG 64 instrument: present NOTHING — the engine's own
         * writes (if any) stay untouched for the scan and the eye. */
        static int s_off = 0;
        if (!s_off) { s_off = 1;
            ep_log("rung64: SWAP relay OFF (GLD_SWAP_RELAY_OFF)"); }
        return 0;
    }
    if (!getenv("GLD_SWAP_LEGACY")) {
        /* RUNG 64 — THE FRAME PRESENT, in order of preference:
         *   1. 0x600E: the engine's raster frame — the heap hunt
         *      captured the GLVM double buffer (two adjacent w*h*4
         *      mallocs); present the one whose midpoint changed since
         *      the last swap (the live buffer).
         *   2. 0x600D: push-only (no frame found — the proven
         *      fallback; a no-op unless something wrote the surface).
         * GLD_SWAP_LEGACY=1 restores the rung-46 clear+relay. */
        static int s_push_logged = 0;
        kern_return_t kr;
        const char* how = "0x600D push";
        if (g_frame_buf_count == 2 && g_fb_w > 1 && g_fb_h > 1) {
            size_t fsz = (size_t)g_fb_w * g_fb_h * 4;
            static unsigned char snap[2][16];
            static int snapped = 0;
            int pick = -1;
            for (int b = 0; b < 2; b++) {
                if (snapped && memcmp(snap[b], g_frame_bufs[b] + fsz / 2,
                                      16) != 0)
                    pick = b;
                memcpy(snap[b], g_frame_bufs[b] + fsz / 2, 16);
            }
            snapped = 1;
            if (pick < 0) pick = 0;    /* first swap / no change: buf 0 */
            uint64_t in[4] = {
                (uint64_t)(uintptr_t)g_frame_bufs[pick],
                (uint64_t)(g_fb_w * 4),
                (uint64_t)g_fb_w, (uint64_t)g_fb_h };
            kr = IOConnectCallMethod(g_virgl_conn, 0x600E,
                                     in, 4, NULL, 0,
                                     NULL, NULL, NULL, NULL);
            how = pick ? "0x600E frame[1]" : "0x600E frame[0]";
        } else {
            kr = IOConnectCallMethod(g_virgl_conn, 0x600D,
                                     NULL, 0, NULL, 0,
                                     NULL, NULL, NULL, NULL);
        }
        if (!s_push_logged || (g_swap_count % 500) == 0) {
            s_push_logged = 1;
            char b[96];
            snprintf(b, sizeof(b), "rung64: SWAP %s -> 0x%x", how, kr);
            ep_log(b);
        }
        return 0;
    }
    ep_log("rung46: SWAP fired (engine [0x66b0]) — presenting");
    if (virgl_ensure_fb() < 0) {
        ep_log("rung46: swap — ensure_fb FAILED"); return 0;
    }
    if (virgl_submit_fb_clear(0x4) < 0) {
        ep_log("rung46: swap — clear submit FAILED"); return 0;
    }
    uint64_t w_in[2] = { g_fb_res, 0 };
    kern_return_t kr = IOConnectCallMethod(g_virgl_conn, 0x600B,
                                           w_in, 2, NULL, 0,
                                           NULL, NULL, NULL, NULL);
    uint64_t rb[5] = { g_fb_res, 0, 0,
                       (uint64_t)g_fb_w, (uint64_t)g_fb_h };
    kern_return_t kr2 = IOConnectCallMethod(g_virgl_conn, 0x600C,
                                            rb, 5, NULL, 0,
                                            NULL, NULL, NULL, NULL);
    char b[96];
    snprintf(b, sizeof(b),
             "rung46: SWAP presented (wait 0x%x, relay 0x%x) %ux%u",
             kr, kr2, g_fb_w, g_fb_h);
    ep_log(b);
    return 0;
}

/* The flush entry ([engine+0x66a8]) — log-only this rung; the
 * probe's CGLFlushDrawable routes to the swap, not the flush. */
long gld_flush_entry(void* dctx, void* a1, void* a2, void* a3,
                     void* a4, void* a5)
{
    (void)dctx; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    ep_log("rung46: FLUSH fired (engine [0x66a8]) — log-only");
    return 0;
}

/* RUNG 46 — THE INSTALL ENTRY (dispatch table slot +0x50): the
 * engine calls it as (driver_ctx, &engine[0x65c8], &engine[0x66d0])
 * (gle 0x15d132) and the driver writes its engine-call entries
 * into the FIRST block: +0xE0 = flush ([engine+0x66a8]), +0xE8 =
 * swap ([engine+0x66b0]). The rung-34 kSlots omitted +0x50 — the
 * float's stores there (grf 0x14f8a-0x14f9e) were past the region
 * rung 34 mirrored. */
long gld_fill_engine_calls(void* dctx, void* block1, void* block2,
                           void* a3, void* a4, void* a5)
{
    (void)dctx; (void)a3; (void)a4; (void)a5;
    g_engine_block1 = block1;
    g_engine_block2 = block2;
    char b[96];
    snprintf(b, sizeof(b),
             "rung46: install entry +0x50 CALLED (dctx=%p block1=%p)",
             dctx, block1);
    ep_log(b);
    /* RUNG 64i — FLOAT INSTALL FIRST: the float selects and writes
     * its conditional flush/swap pair into block1 (+0xE0/+0xE8, per
     * grf.t 0x1f912-0x1f921). We then WRAP the swap: save the float's
     * entry, install our wrapper (which forwards then pushes the GA
     * surface via 0x600D). */
    if (g_float_install && block1) {
        long r = (long)g_float_install(dctx, block1, block2,
                                       a3, a4, a5);
        g_block1 = block1;
        g_float_swap_fn =
            (long(*)(void*,void*,void*,void*,void*,void*))
            *(void**)((char*)block1 + 0xE8);
        *(void**)((char*)block1 + 0xE8) = (void*)&gld_swap_wrapper;
        char w[128];
        snprintf(w, sizeof(w),
                 "rung64i: float swap %p WRAPPED (push after)",
                 (void*)g_float_swap_fn);
        ep_log(w);
        return r;
    }
    if (block1) {
        *(void**)((char*)block1 + 0xE0) = (void*)&gld_flush_entry;
        *(void**)((char*)block1 + 0xE8) = (void*)&gld_swap_entry;
        ep_log("rung46: block1 filled (+0xE0 flush, +0xE8 swap)");
    }
    return 0;
}

/* RUNG 48 — THE ATTACH SLOT (+0x48): the engine's
 * gliAttachDrawableWithOptions calls here (gle 0xf444) with the
 * drawable and PARSES THE RESULT (0xf44d+); a passing return is
 * what sets [engine+0x6570] — the hardware classification. Log
 * the raw args (the table call's convention may differ from the
 * entry-level attach) and return success. */
long gld_table_attach(void* a0, void* a1, void* a2, void* a3,
                      void* a4, void* a5)
{
    (void)a3; (void)a4; (void)a5;
    if (g_float_attach) {                     /* RUNG 64i: real attach */
        long r = (long)g_float_attach(a0, a1, a2, a3, a4, a5);
        /* The float's attach ran its flush/swap selection, writing
         * block1+0xE8 (= ctx+0x66b0) with ITS swap. WRAP IT: save the
         * float's entry, install our push-after wrapper. block1 =
         * ctx + 0x65c8 (the install-entry geometry). */
        if (a0) {
            void** slot = (void**)((char*)a0 + 0x65c8 + 0xE8);
            g_float_swap_fn =
                (long(*)(void*,void*,void*,void*,void*,void*))*slot;
            if (g_float_swap_fn != (void*)&gld_swap_wrapper) {
                *slot = (void*)&gld_swap_wrapper;
                char w[128];
                snprintf(w, sizeof(w),
                         "rung64i: attach-wrap: float swap %p WRAPPED "
                         "(ctx=%p)", (void*)g_float_swap_fn, a0);
                ep_log(w);
            }
        }
        return r;
    }
    char b[128];
    snprintf(b, sizeof(b),
             "rung48: TABLE attach(+0x48) CALLED a0=%p a1=0x%lx a2=%p -> 0",
             a0, (unsigned long)(uintptr_t)a1, a2);
    ep_log(b);
    return 0;
}

/* RUNG 21 (pre-registered, LEDGER 29422cc) — measure the
 * registered device id, don't guess it. libGFXShared's
 * _gfx_plugin_head is an exported global; the stub shares its
 * process. Walk the plugin list ONCE per process, log +0x110
 * (deviceID — the id-plane lookup key) and +0x118 (accumulated
 * display mask) per plugin. The pf object's +8 id is then derived
 * from the MEASURED plane | our low bits, so the decoration and
 * the id-keyed lookups stay inside the registered plane. */
static int g_plugins_dumped = 0;
static unsigned g_measured_plane = 0;
static unsigned g_device_id = 0;   /* rung 28: the canonical id (device+0x10) */

static void dump_plugins_once(void)
{
    if (g_plugins_dumped) return;
    g_plugins_dumped = 1;
    void* h = dlopen("/System/Library/Frameworks/OpenGL.framework/"
                     "Versions/A/Libraries/libGFXShared.dylib", RTLD_LAZY);
    if (!h) {
        ep_log("rung21: libGFXShared dlopen FAILED");
        return;
    }
    /* _gfx_plugin_head is internal (not exported); _gfxGetPlugins IS
     * exported (T) and returns the list head — the engine's own
     * gliChoosePixelFormat uses it exactly this way (rax = head). */
    void* (*getplugins)(void) = (void* (*)(void))dlsym(h, "gfxGetPlugins");
    if (!getplugins) {
        ep_log("rung21: gfxGetPlugins UNRESOLVED");
        return;
    }
    int n = 0;
    for (void* p = getplugins(); p && n < 8; p = *(void**)p, n++) {
        unsigned id   = *(unsigned*)((char*)p + 0x110);
        unsigned mask = *(unsigned*)((char*)p + 0x118);
        char buf[128];
        snprintf(buf, sizeof(buf),
                 "rung21: plugin[%d] %p +0x110(id)=0x%x +0x118(mask)=0x%x",
                 n, p, id, mask);
        ep_log(buf);
        if (n == 0)
            g_measured_plane = id & 0xffff00;
    }
    if (!n)
        ep_log("rung21: plugin list EMPTY");
    /* RUNG 28: the shared-state path resolves the DEVICE by
     * id & 0xffffff00 (_gfxGetDeviceWithDeviceID, exact) — measure
     * the device list too (device: next@+0, plugin@+8, id@+0x10,
     * mask@+0x14), plus the exported float device id. */
    void* (*getdevices)(void) = (void* (*)(void))dlsym(h, "gfxGetDevices");
    unsigned* float_id = (unsigned*)dlsym(h, "gfx_float_device_id");
    if (float_id) {
        char buf[64];
        snprintf(buf, sizeof(buf), "rung28: gfx_float_device_id = 0x%x", *float_id);
        ep_log(buf);
    }
    if (getdevices) {
        int m = 0;
        for (void* d = getdevices(); d && m < 8; d = *(void**)d, m++) {
            unsigned did  = *(unsigned*)((char*)d + 0x10);
            unsigned dmsk = *(unsigned*)((char*)d + 0x14);
            char buf[128];
            snprintf(buf, sizeof(buf),
                     "rung28: device[%d] %p +0x10(id)=0x%x +0x14(mask)=0x%x",
                     m, d, did, dmsk);
            ep_log(buf);
        }
        if (!m)
            ep_log("rung28: device list EMPTY (no devices registered)");
        else if (!g_device_id)
            g_device_id = *(unsigned*)((char*)getdevices() + 0x10);
    }
}

int gldInitializeLibrary(int* psvc, void* arg1, int GLDisplayMask,
                         void* arg3, void* arg4, int arg5)
{
    /* RUNG 64i CORRECTION: do NOT forward Initialize to the float —
     * its registration (plugin/device tables, same 0x1020400 id we
     * claim) CORRUPTS the engine's renderer state and CGLCreate-
     * Context dies before reaching any renderer entry (observed
     * 22:36/22:38). The float's per-call entries run fine on our
     * registration. */
    FILE *f = fopen("/tmp/vm_gld_stub.log", "a");
    if (f) {
        time_t t = time(NULL);
        char ts[32];
        strftime(ts, sizeof(ts), "%H:%M:%S", localtime(&t));
        fprintf(f, "[%s pid=%d] CALL gldInitializeLibrary(6-arg) "
                 "psvc=%p%s arg1=%p mask=0x%x arg3=%p arg4=%p arg5=0x%x\n",
                 ts, (int)getpid(), (void*)psvc,
                 psvc ? " (*psvc below)" : " (NULL)",
                 arg1, GLDisplayMask, arg3, arg4, arg5);
        if (psvc)
            fprintf(f, "[%s pid=%d]   *psvc = %d (0x%x)\n",
                    ts, (int)getpid(), *psvc, *psvc);
        int (*preinit)(int) = (int (*)(int))dlsym(RTLD_DEFAULT, "glvmPreInit");
        if (!preinit) {
            fprintf(f, "[%s pid=%d]   glvmPreInit UNRESOLVED (RTLD_DEFAULT)"
                     " — VM not initialized, version will answer FALSE\n",
                     ts, (int)getpid());
            g_vm_mask = 0;
            fclose(f);
            return -1;
        }
        int rc = preinit(arg5 & 1);
        g_vm_mask = GLDisplayMask;
        g_vm_ok = (g_vm_mask != 0);   /* rung 6b: the REAL guard structure */
        /* RUNG 55: the OSMesa create at LOAD TIME — no GL current,
         * the glapi dispatch uncontested (creating inside
         * glClear_Exec crashed at st_create_context+40). */
        if (g_vm_ok) osmesa_create_at_load();
        fprintf(f, "[%s pid=%d]   glvmPreInit(0x%x) -> %d (rc propagated; "
                 "guard=mask!=0 -> %d)\n",
                ts, (int)getpid(), arg5 & 1, rc, g_vm_ok);
        fclose(f);
        g_fwd_armed = 1;   /* RUNG 64i: our registration complete —
                            * the float trampoline may now serve */
        return rc;
    }
    g_vm_ok = 0;
    g_vm_mask = 0;
    return -1;
}

void gldTerminateLibrary(void)
{
    FILE *f = fopen("/tmp/vm_gld_stub.log", "a");
    void (*postterm)(void) = (void (*)(void))dlsym(RTLD_DEFAULT, "glvmPostTerm");
    if (f) {
        ep_log("CALL gldTerminateLibrary -> glvmPostTerm forwarded");
        fclose(f);
    }
    if (postterm) postterm();
    g_vm_ok = 0;
    g_vm_mask = 0;
}

/* ==== generated from VMsvga2 EntryPointNames.c + header return types ==== */
/* RUNG 11a (pre-registered): the honest renderer record, claiming a
 * display we don't have. Contract per GLRendererFloat gldGetRendererInfo
 * @0x1775b (rung-5 decode): ~0x88-byte record; mask protocol — answer
 * ONLY for query masks within the claim, else 0x2716 (kCGLBadMatch),
 * the float's EXACT condition: (!(claim & q) || (~claim & q)). Claim =
 * 0x2 (nonexistent display on this VM — zero desktop reach by
 * construction). Fields: OUR header anchor, OUR renderer id (vendor
 * space 0x1AF4), SOFTWARE-class caps (honest — the stub is software),
 * modest constants where the float's fields were otool-unreadable. */
#define GLD_BAD_MATCH 0x2716
#define RUNG11_CLAIM  0x1   /* 11b: the real display — enumerate-first */
#include <string.h>
#include <stdlib.h>
#include <mach-o/loader.h>
extern struct mach_header_64 _mh_bundle_header;
long gldGetRendererInfo(void* rec, int query_mask)
{
    FILE *f = fopen("/tmp/vm_gld_stub.log", "a");
    if (f) {
        time_t t = time(NULL);
        char ts[32];
        strftime(ts, sizeof(ts), "%H:%M:%S", localtime(&t));
        fprintf(f, "[%s pid=%d] CALL gldGetRendererInfo(rec=%p, q=0x%x) "
                 "claim=0x%x -> ", ts, (int)getpid(), rec, query_mask,
                 RUNG11_CLAIM);
        fclose(f);
    }
    unsigned* r = (unsigned*)rec;
    int claim = RUNG11_CLAIM;
    if (!(claim & query_mask) || (~claim & query_mask)) {
        ep_log("  kCGLBadMatch 0x2716 (outside claim)");
        return GLD_BAD_MATCH;
    }
    memset(r, 0, 0x88);
    *(unsigned long*)&r[0] = (unsigned long)&_mh_bundle_header; /* +0 anchor */
    r[2] = 0x1AF40100;   /* +8  our renderer id (our vendor space) */
    r[3] = 0x17CD;       /* +0xc class field — RUNG 47: HARDWARE class
                           (0x6CD | 0x1000), the worked example's own
                           recorded value (VMsvga2GLDriver.c:139). The
                           transport, readback, and presentation back
                           the claim since rung 45. */
    r[4] = 0xD;          /* +0x10 */
    r[5] = 0x8008000;    /* +0x14 caps word (software class) */
    r[6] = 0x20000000;   /* +0x18 caps word */
    r[7] = 0x1001;       /* +0x1c */
    r[8] = 0x81;         /* +0x20 */
    r[9] = claim;        /* +0x24 the claim */
    r[10] = 0x00010004;  /* +0x28 word=4, +0x2a word=1 (packed per float) */
    r[11] = 0x00010010;  /* +0x2c word=0x10, +0x2e byte=1 — CORRECTED from
                            0x01000010 (rung-11a packing error: the 1 was at
                            +0x2f, not +0x2e; the accelerated byte was NEVER
                            SET — this is the rung-16 probe AND a bug fix) */
    r[12] = 1;           /* +0x30 */
    /* +0x3c..+0x84 limit fields — modest constants, documented as
     * guesses-with-margin (float's were otool-unreadable); consumed
     * only by describe-queries against the nonexistent display. */
    for (int off = 0x3c; off < 0x88; off += 4)
        *(unsigned*)((char*)r + off) = 256;
    ep_log("  RECORD filled (id=0x1AF40100, software caps, claim printed above)");
    return 0;
}

/* RUNG 5→10 EVOLUTION: values per the working GLD's disassembly, with
 * rung 9's correction: the loader (libGFXShared @0x157a-0x15b5)
 * validates (RET!=0, a0==3, a1==1, a2==0, a3 bits only in 0xFF00).
 * The rung-5 "&_mh_bundle_header" was an otool symbol-displacement
 * MISREAD — the real GLD writes 0; a2 nonzero rejected the plugin on
 * every prior rung. RUNG 6 guard kept: true only after the
 * Initialize mask store (the working GLD's own honesty structure). */
#include <mach-o/loader.h>
extern struct mach_header_64 _mh_bundle_header;
long gldGetVersion(int* a0, int* a1, int* a2, int* a3)
{
    if (!g_vm_ok) {
        ep_log("CALL gldGetVersion -> FALSE (guarded: VM init not ok)");
        return 0;
    }
    ep_log("CALL gldGetVersion -> TRUE (3,1,NULL,0x400; VM ok; rung 10)");
    if (a0) *a0 = 3;
    if (a1) *a1 = 1;
    if (a2) *a2 = 0;
    if (a3) *a3 = 0x400;
    return 1;
}

/* RUNG 14 (pre-registered): the honest pf entry — rsi IS the caller's
 * raw CGL attribute array (GLEngine gliChoosePixelFormat @0x13cf, the
 * only call site; verified: movq %r14,%rsi — no rewriting, no mask).
 * rdx is NEVER SET by the caller (garbage — do not read it). The 87-
 * case float parser from rung 12 IS the right contract model here
 * (correct work aimed at the wrong site, now correctly aimed). The
 * out-zero rule applies: *out = NULL before ANY return path.
 *
 * RUNG 18(a) (pre-registered in LEDGER, commit 8cd4e38): the dead
 * gate is GONE. The rung-14 transcription put `case 0: gate=1`
 * inside `while (*p)` where code 0 is unreachable — the parser
 * refused EVERY list (observed: caller error == stub return,
 * 0x2716→10006, for all eight probe sets). Per the float's own
 * default (goto build = TRUNCATE-AND-BUILD) and the observed list
 * shape (caller attrs + trailer 4 + 0-terminator): the terminator
 * completes the walk; an unknown attr truncates it; BOTH build.
 * The object stays software-honest (no accelerated claim). */
long gldChoosePixelFormat(void** out, int* attrs, void* rdx_unused)
{
    if (out) *out = (void*)0;               /* THE RULE — always, first */
    dump_plugins_once();                    /* rung 21: measure, then derive */
    FILE *f = fopen("/tmp/vm_gld_stub.log", "a");
    if (f) {
        time_t t = time(NULL);
        char ts[32];
        strftime(ts, sizeof(ts), "%H:%M:%S", localtime(&t));
        fprintf(f, "[%s pid=%d] CALL gldChoosePixelFormat attrs=[", ts, (int)getpid());
        for (int i = 0; i < 12 && attrs[i]; i++)
            fprintf(f, "%s0x%x", i ? " " : "", attrs[i]);
        fprintf(f, "] (out zeroed)\n");
        fclose(f);
    }
    if (!attrs || !attrs[0]) {
        ep_log("  gldChoosePixelFormat -> 0x2716 (no attrs)");
        return GLD_BAD_MATCH;
    }
    /* The 87-case float parser — SUPERSEDED PROVENANCE WARNING
     * (LEDGER, 2026-08-22): this case table is the LAST thing
     * carried from the rung-12 transcription. That transcription
     * produced three errors found by reading the float's own
     * builder (the unreachable case-0 gate; obj+0 read as a bundle
     * header where it is the chain link; +0x14 read as 0x8000
     * where the float writes 0x8000000). No case here is trusted
     * by default: the shortcut (2/50/53) is confirmed behaviorally
     * by stub-log observation; the rest is UNVALIDATED against the
     * float's own switch (grf.t jump table at 0x17920 — the
     * replacement source). Cases verified since: 5 and 6 (rung 24,
     * from the request-constructor read, not this table). */
    unsigned flags = 0x4C8;   /* rung 24, read-justified: 0x480 baseline
                               * (required by EVERY request — the constructor
                               * defaults [request+0xc]=0x480) | 0x40 robust
                               * (attr 75) | 0x8 backing (attr 76). NO 0x100 —
                               * that is the attr-73 HARDWARE bit, honest only
                               * with functional 3D. */
    unsigned mode10 = 0;      /* the +0x10 exact-match echo */
    int depth_seen = 0, stencil_seen = 0;   /* RUNG 62: the slots are
                                              * FORMAT CODES, not bit
                                              * counts (float-reference
                                              * dump: any depth size ->
                                              * 0x1000, any stencil ->
                                              * 0x80, absent -> 1) */
    int complete = 0, si = 0;
    int* p = attrs;
    int walked = 0;
    while (*p) {
        int code = *p;
        walked += 4;
        switch (code) {
        case 1:  break;                       /* AllRenderers (request CLEARS 0x400) */
        case 2: case 50: case 53:
            ep_log("  gldChoosePixelFormat -> 0 (shortcut, no object; out NULL)");
            return 0;                        /* float's shortcut: *out stays NULL */
        case 5:  mode10 |= 0x8; break;        /* DoubleBuffer — echo (0xb7d0) */
        case 6:  mode10 |= 0x2; break;        /* Stereo — echo (0xb7db) */
        case 3: case 4: case 7: case 8: case 9: case 10:
        case 51: case 52: case 57:
        case 11:                             /* RUNG 60: AlphaSize — the
                                                 NS path forwards it RAW
                                                 (observed in GLMark's
                                                 [5,8,1,b,1,c,1,4]); the
                                                 missing case truncated
                                                 the walk BEFORE depth */
        case 12: depth_seen = 1; p++; walked += 4; break;   /* RUNG 62:
                                                 DepthSize — value attr;
                                                 the SIZE is ignored by
                                                 the scorer (float
                                                 answers 1/16/24/32
                                                 identically); what
                                                 matters is the CODE */
        case 13: stencil_seen = 1; p++; walked += 4; break; /* RUNG 62:
                                                 StencilSize — same */
        case 14: case 15:                     /* Accum/Aux — same class */
            p++; walked += 4; break;          /* value attrs: consumed */
        case 47: case 48: case 72: case 54: break;  /* no-op pass */
        case 49: flags |= 4; si = 1; break;
        case 73: flags |= 0x100; break;       /* RUNG 47 — kCGLPFAccelerated:
                                                 the HARDWARE claim. Held back
                                                 since rung 24 ("honest only
                                                 with functional 3D"); the
                                                 transport, readback, and
                                                 on-screen presentation now
                                                 back it. Worked-example
                                                 value: 0x501-class flags. */
        case 76: flags |= 1; break;           /* BackingStore */
        case 86: flags |= 0x2000; break;
        case 80: p++; walked += 4; break;     /* Window: consumes mask value */
        default:
            ep_log("  walk TRUNCATED at unknown attr (build anyway; 18a)");
            goto build;                      /* TRUNCATE — the 63-code default */
        }
        p++;
        if (walked > 0xc3) {
            ep_log("  gldChoosePixelFormat -> 0x2710 (attr overflow; out NULL)");
            return 0x2710;
        }
    }
    complete = 1;                            /* terminator: walk complete — 18a */
    /* RUNG 26 — the float's OWN conditional, mirrored from its build
     * tail (grf.t 0x17ca6: orl $1 / testb $4 / cmovel): the float
     * ORs bit 0x1 into its flags when bit 0x4 is ABSENT. Bisect
     * result (T0-T6 + endpoint x2, {53} control stable throughout):
     * flags bit 0 is the scorer's positive requirement — 0x4C8
     * never passes, 0x4C9 always does. Not a hardware claim: the
     * float's software objects carry it. */
build:
    /* RUNG 47 CORRECTION — the claim is UNCONDITIONAL: the engine
     * strips attr 73 before forwarding (observed: the {73,5} request
     * arrives as [0x5 0x4] — the accelerated criterion filters
     * RENDERERS via the census engine-side; the driver never sees
     * 73). A hardware renderer claims hardware on EVERY object —
     * the worked example's shape (VMsvga2GLDriver.c:169: p[1]=0x501
     * applied to every return, no conditional). The scorer is
     * requirement-based (rung 26: extra bits harmless — 0x4C9 passed
     * a 0x4C0-composed request), so the extra bit cannot break plain
     * requests. */
    flags |= 0x100;
    unsigned* obj = (unsigned*)calloc(1, 0x38);
    if (!obj) {
        ep_log("  gldChoosePixelFormat -> 0x2716 (alloc fail; out NULL)");
        return GLD_BAD_MATCH;
    }
    /* RUNG 19 — the chain contract, read from gliChoosePixelFormat
     * (GLEngine 0x13cf-0x149b): a pf object's +0 IS THE CHAIN LINK
     * to the next pf object (multi-slot returns are a linked list;
     * the engine appends our object at the tail via [tail+0]=obj,
     * decorates EACH node's own +8 with 0x20000 at 0x1444, and
     * follows +0 until NULL). +8 is the renderer id ON the object.
     * Single-slot return: +0 MUST be NULL.
     *   - +0 = &_mh_bundle_header (rung 14): the walk decorated
     *     header+8 — read-only __TEXT, SIGBUS at stub_base+8.
     *   - +0 = &g_driver_obj (rung 18a): the walk APPENDED the fake
     *     driver object as a second "pixel format" — chain
     *     poisoning; npix=0 by validation failure on the bogus
     *     node, not by filtering. Both were misreads of rax's
     *     provenance: 0x8(%rax) is the OBJECT's own +8, not a
     *     pointed-to driver's. */
    /* RUNG 28 (final form): the +8 id is the MEASURED DEVICE id —
     * 0x1020400, the version-composed registration id (0x1020000 |
     * a3&0xFF00 — rung 9's decode, RIGHT all along; the rung-21
     * "correction" to the plugin field's 0x20400 was itself wrong:
     * +0x110 stores the id 16-bit-masked, and 0x1020400 & 0xffff00
     * = 0x20400 RESOLVES the plugin). The device lookup
     * (_gfxCreateSharedState 0x1826: id & 0xffffff00, exact vs
     * device+0x10) demands the full id. 0x1020400 passes ALL FOUR
     * id checks: plugin (0xffff00-plane), device (0xffffff00
     * exact), gliCreateContext's 0xff0000-plane (==0x20000 — the
     * decoration bit is IN the composed id), and the 0x7f00
     * preferred index (==0x400). */
    unsigned use_id = g_device_id ? g_device_id
                   : (g_measured_plane ? g_measured_plane : 0x1AF40100);
    *(unsigned long*)&obj[0] = 0;    /* chain terminator: single slot */
    obj[2] = use_id;       /* +8  (engine ORs 0x20000 — idempotent in-plane) */
    /* RUNG 26 bisect instrument: GLD_PF_FLAGS overrides +0xc per run
     * (env reaches the stub — same process). Registered protocol:
     * every point runs {75} (test) and {53} (control, expected
     * 0/npix=0 always); deviations invalidate the point; the passing
     * endpoint re-runs to close. A 0x100 positive is a PROBE RESULT
     * — "the scorer requires a claim we cannot yet back" — and
     * reverts after the bisect (LEDGER, registered). */
    if (!(flags & 0x4))
        flags |= 0x1;          /* the float's conditional (0x17ca6) */
    {
        const char* e = getenv("GLD_PF_FLAGS");
        if (e) flags = (unsigned)strtoul(e, 0, 0);   /* bisect override */
        /* RUNG 51 — the mode10 bisect (the scorer EXACT-tests +0x10,
         * rung 24; the depth/stencil request composes bits there the
         * walk doesn't echo yet — the bits are being found empirically
         * per the rung-26 protocol). */
        const char* e2 = getenv("GLD_PF_MODE10");
        if (e2) mode10 = (unsigned)strtoul(e2, 0, 0);
    }
    obj[3] = flags;        /* +0xc — rung-24 honest subset 0x4C8 unless
                            * GLD_PF_FLAGS overrides (bisect only) */
    obj[4] = mode10;       /* +0x10 — the buffer-modes ECHO of the walked
                            * attrs (5->0x8, 6->0x2), matching the request's
                            * own +0x10 composition for the exact test */
    obj[5] = 0x8000000;    /* +0x14 — THE FLOAT'S OWN CONSTANT (0x17c5d:
                            * movl $0x8000000). The rung-12 transcription
                            * read 0x8000 — off by 0x1000x — and the scorer
                            * EXACT-TESTS this field (0x5567 at 0xbb5f,
                            * equality at 0xbb67). That misread rejected
                            * every request. */
    obj[6] = 1;            /* +0x18 — the float's 0x17c56: movl $0x1
                            * (never set in our builds) */
    obj[7] = depth_seen ? 0x1000u : 1u;  /* +0x1c — RUNG 62: the DEPTH
                   * FORMAT CODE, not a bit count. RunG 59's "24" was
                   * the wrong semantic — the float-reference dump
                   * (bundle aside, float serving) shows the accepted
                   * object carries 0x1000 for ANY depth request
                   * (sizes 1/16/24/32 identical) and 1 when absent.
                   * The depth surface itself is real (rungs 51/58);
                   * this field never described its bits. */
    obj[8] = stencil_seen ? 0x80u : 1u;  /* +0x20 — the STENCIL code,
                   * same dump: 0x80 with stencil, 1 without */
    obj[13] = RUNG11_CLAIM; /* +0x34 our display claim (rung 22 reverted:
                             * gate 1 exonerated; honest value stands) */
    *out = obj;
    {
        char buf[128];
        snprintf(buf, sizeof(buf),
                 "  gldChoosePixelFormat -> 0 (object BUILT, +0=NULL, "
                 "id=0x%x from plane 0x%x)", use_id, g_measured_plane);
        ep_log(buf);
    }
    return 0;
}
/* RUNG 19 — the ownership contract's other half. gliDestroyPixelFormat
 * (GLEngine 0x149c) walks the pf chain (+0 links) and calls THIS entry
 * per node via plugin slot 0x138/8=39, expecting 0 on success
 * (0x1505: testl %eax; je -> success path). The refusal macro leaked
 * every object AND failed teardown. The chain objects are ours
 * (calloc'd in gldChoosePixelFormat): free them here, answer 0. */
long gldDestroyPixelFormat(void* obj, void* a1, void* a2, void* a3, void* a4, void* a5)
{
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    ep_log("CALL gldDestroyPixelFormat -> 0 (freed; rung 19 ownership)");
    free(obj);
    return 0;
}
/* RUNG 29 — the honest gldCreateShared, mirrored from the float's
 * own implementation (grf.t 0x13ed9): mask gate (request ⊆ the
 * Initialize-stored mask, nonempty — the rung-6b g_vm_mask playing
 * gld_io_data's role), malloc(0x70), pthread mutex at +0, arg3 at
 * +0x40, refcount dword at +0x48 (0 — gldDestroyContext decrements
 * it), NULL list heads +0x50/58/60, processor block pointer at
 * +0x68. Writability contract: heap, persistent, freed at the
 * matching destroy. */
long gldCreateShared(void** out, unsigned mask, void* arg3,
                     void* a3, void* a4, void* a5)
{
    /* RUNG 64i: NOT forwarded — this fires inside CGLCreateContext
     * before any renderer create; the float's version on un-Initialized
     * state broke the context path (the 22:41/22:42 failures). Our
     * rung-29 mirror is the proven answer. */
    (void)a3; (void)a4; (void)a5;
    if (out) *out = (void*)0;               /* THE RULE — always, first */
    char buf[96];
    snprintf(buf, sizeof(buf),
             "CALL gldCreateShared mask=0x%x arg3=%p (vm_mask=0x%x)",
             mask, arg3, g_vm_mask);
    ep_log(buf);
    if (!(mask & g_vm_mask) || (mask & ~g_vm_mask)) {
        ep_log("  gldCreateShared -> 0x2716 (mask gate, per the float)");
        return GLD_BAD_MATCH;
    }
    unsigned long* obj = (unsigned long*)calloc(1, 0x70);
    if (!obj) {
        ep_log("  gldCreateShared -> 0x2716 (alloc fail)");
        return GLD_BAD_MATCH;
    }
    pthread_mutex_init((pthread_mutex_t*)obj, NULL);   /* +0 mutex */
    obj[8]  = (unsigned long)arg3;          /* +0x40 */
    /* +0x48 refcount = 0 (calloc) */
    /* +0x50/58/60 = NULL (calloc) */
    obj[13] = (unsigned long)&g_proc_stand_in; /* +0x68 */
    *out = obj;
    ep_log("  gldCreateShared -> 0 (object 0x70 built, rung 29)");
    return 0;
}
long gldDestroyShared(void* obj, void* a1, void* a2, void* a3, void* a4, void* a5)
{
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    ep_log("CALL gldDestroyShared -> 0 (freed; rung 29 ownership)");
    free(obj);
    return 0;
}
/* RUNG 30 — the honest gldCreateContext, mirrored from the float
 * (grf.t 0x1403d): gldValidatePixelFormat first (refuse 0x2712 if
 * no pf), gldVecAlloc(0xC60), GL-state defaults at +4..+0x1c
 * (engine-irrelevant), args at +0x738/0x740/0x748 — the offsets
 * the float's own gldDestroyContext confirms (+0x738 = the shared;
 * it locks the shared's +0 mutex and decrements +0x48). The
 * engine treats the GLD context as opaque except through entries.
 * Mirror: the float's SIZE, the arg offsets, zeros elsewhere. */
long gldCreateContext(void** out, void* pf, void* shared,
                      void* a4, void* a5, void* a6)
{
    if (a4) g_engine_subblock = a4;   /* RUNG 66: engine = a4 - 0x79b8 */
    {
        static long (*fn)(void*,void*,void*,void*,void*,void*) = 0;
        if (!fn) fn = (long(*)(void*,void*,void*,void*,void*,void*))
                     float_sym("gldCreateContext");
        if (fn) {
            long r = (long)fn(out, pf, shared, a4, a5, a6);
            char fb[128];
            snprintf(fb, sizeof(fb),
                     "rung64i: float CreateContext -> 0x%lx (*out=%p "
                     "pf=%p sub=%p)", r, out ? *out : (void*)0, pf, a4);
            ep_log(fb);
            return r;
        }
    }
    if (out) *out = (void*)0;               /* THE RULE — always, first */
    char buf[96];
    snprintf(buf, sizeof(buf),
             "CALL gldCreateContext pf=%p shared=%p (rung 30)",
             pf, shared);
    ep_log(buf);
    /* RUNG 46 — THE SWAP CAPABILITY: the engine's 6-arg call's 4th
     * arg (rcx = this a4) is the per-renderer SUB-BLOCK (engine ctx
     * +0x79b8 + idx*0x888, stored as [engine+0x65c0]). Its byte at
     * +0x2d is the driver-declared "has swap" capability — the
     * engine copies it to [engine+0x798c] (gle 0x23bc), and
     * gliSwapBuffers SKIPS the driver call when it is zero. The
     * rung-30 mirror ignored the arg; write the byte now. */
    if (a4) {
        /* RUNG 52 — fetch the capset FIRST (device transport + the
         * winsys's own selector pair), then derive the limits. */
        if (virgl_transport_init() > 0 && !g_caps_fetched)
            virgl_fetch_capset();
        unsigned char* sb = (unsigned char*)a4;
        /* RUNG 50 — THE CONFIG BLOCK: the float's gldSetConfigData
         * map (grf64 0x139ae), filled here at create. The engine
         * answers GL limits queries from this block; unfilled, every
         * query returned ZERO (measured). */
        unsigned* w = (unsigned*)sb;
        unsigned short* h = (unsigned short*)sb;
        w[0] = 0xC;  w[1] = 0x3F800000;             /* +0, +4: 1.0f */
        /* +8, +C: max tex dims — RUNG 52: DEVICE-sourced from the
         * capset when v2 arrived (max_texture_2d_size); the float's
         * 0x4000 remains the fallback. */
        uint32_t max2d = (g_caps_fetched && g_caps_v2_ok &&
                          g_caps.max_texture_2d_size) ? g_caps.max_texture_2d_size
                                                      : 0x4000;
        w[2] = max2d; w[3] = max2d;
        if (g_caps_fetched && g_caps_v2_ok && max2d != 0x4000) {
            char b3[80];
            snprintf(b3, sizeof(b3),
                     "rung52: max tex dims 0x4000 -> 0x%x (device)", max2d);
            ep_log(b3);
        }
        w[4] = 1; w[5] = 1;                          /* +10, +14 */
        sb[0x18] = 0xA; sb[0x19] = 8; sb[0x1A] = 8;  /* +18..+1A */
        sb[0x1B] = 0; sb[0x1C] = 0xC; h[0x0F] = 0;   /* +1B, +1C, +1E */
        sb[0x2C] = 0; sb[0x2D] = 1; sb[0x2E] = 0; sb[0x2F] = 1;
        sb[0x30] = 0; sb[0x31] = 1; sb[0x32] = 8;
        sb[0x34] = 8; sb[0x35] = 8; sb[0x36] = 8; sb[0x37] = 8;  /* RGBA sizes */
        sb[0x38] = 0; sb[0x39] = 0; sb[0x3A] = 0; sb[0x3B] = 0;  /* accum */
        sb[0x3C] = 0; sb[0x3D] = 0; sb[0x3E] = 0; sb[0x3F] = 0;
        sb[0x58] = 0; sb[0x59] = 0; sb[0x5B] = 1;
        sb[0x5C] = 0; sb[0x5D] = 1; sb[0x5E] = 1;
        w[0x68/4] = 0x1000; w[0x6C/4] = 0; w[0x70/4] = 4;
        w[0x74/4] = 0x13; w[0x78/4] = 0;
        w[0x7C/4] = 0x80; w[0x80/4] = 0x80; w[0x84/4] = 0x20;
        w[0x88/4] = 0x41800000; w[0x8C/4] = 0x41800000;  /* 16.0f */
        h[0x90/2] = 8; h[0x92/2] = 0x10; h[0x94/2] = 0x10; h[0x96/2] = 8;
        h[0x98/2] = (unsigned short)max2d; h[0x9A/2] = (unsigned short)max2d;
        h[0x9C/2] = (unsigned short)max2d; h[0x9E/2] = (unsigned short)max2d;
        h[0xA0/2] = 0x2000; sb[0xA2] = 5;
        h[0xA4/2] = 0x83f0; h[0xA6/2] = 0x83f1;
        h[0xA8/2] = 0x83f2; h[0xAA/2] = 0x83f3; h[0xAC/2] = 0x8837;
        w[0xC4/4] = 0x4000;
        w[0x130/4] = 0x1000; w[0x134/4] = 0x40; w[0x13C/4] = 0x10;
        w[0x140/4] = 0x40; w[0x144/4] = 0x1000; w[0x148/4] = 0x400;
        w[0x14C/4] = 0x1000; w[0x154/4] = 0x100000;
        w[0x158/4] = 0xFFFFFFF8; w[0x15C/4] = 7; w[0x160/4] = 0x40;
        w[0x168/4] = 0x10; w[0x16C/4] = 0x20; w[0x170/4] = 0x40;
        w[0x174/4] = 0x20; w[0x178/4] = 0x20;
        sb[0x17C] = 1; h[0x17E/2] = 1;
        w[0x198/4] = 0x2683A001 | 0x197C5FFE;
        w[0x19C/4] = 0x20000000 | 0xC0000000;
        w[0x1A0/4] = 4 | 8 | 0x20000000 | 0x590000;
        g_engine_subblock = a4;   /* rung 48: the engine base derives
                                   * from it (sub-block = engine +
                                   * 0x79b8 + idx*0x888, idx=0) */
        snprintf(buf, sizeof(buf),
                 "rung50: config block %p FILLED (limits+caps; +0x2d=1)",
                 a4);
        ep_log(buf);
    }
    if (!pf) {
        ep_log("  gldCreateContext -> 0x2712 (no pf, per the float's validate)");
        return 0x2712;
    }
    unsigned char* ctx = (unsigned char*)calloc(1, 0xC60);
    if (!ctx) {
        ep_log("  gldCreateContext -> 0x2712 (alloc fail)");
        return 0x2712;
    }
    *(void**)(ctx + 0x738) = shared;   /* the float's arg3 slot; DestroyContext locks it */
    *(void**)(ctx + 0x740) = a5;
    *(void**)(ctx + 0x748) = a6;
    *out = ctx;
    ep_log("  gldCreateContext -> 0 (object 0xC60 built, rung 30)");
    return 0;
}
/* RUNG 30 — the destroy mirrors the float's handshake: lock the
 * shared's +0 mutex, decrement the shared's +0x48 refcount (the
 * field the float initialized in CreateShared), unlock, free. */
long gldDestroyContext(void* ctx, void* a1, void* a2, void* a3, void* a4, void* a5)
{
    GLD_FWD("gldDestroyContext", ctx, a1, a2, a3, a4, a5, 0);
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    ep_log("CALL gldDestroyContext (rung 30)");
    if (ctx) {
        void* shared = *(void**)((char*)ctx + 0x738);
        if (shared) {
            pthread_mutex_lock((pthread_mutex_t*)shared);
            (*(int*)((char*)shared + 0x48))--;   /* the float's refcount handshake */
            pthread_mutex_unlock((pthread_mutex_t*)shared);
        }
        free(ctx);
    }
    ep_log("  gldDestroyContext -> 0 (freed; refcount handshake done)");
    return 0;
}
EPR(gldReclaimContext)
/* RUNG 33 — the honest gldAttachDrawable, mirrored from the float
 * (grf.t 0x1745d): esi is a DRAWABLE-TYPE code — 0x36
 * (fullscreen-class) is REFUSED with 0x271c even by the float;
 * other types run the float's glsAssignDrawable (renderer-side
 * surface allocation), store the type at ctx+0x210, and compute
 * buffer sizes from the drawable object at ctx+0x218. The stub
 * has no renderer surface machinery yet (bridge territory): the
 * mirror keeps the type store + the float's refusal, answers 0,
 * and lets gldInitDispatch (the attach's follower) fire. */
long gldAttachDrawable(void* ctx, unsigned type, void* a3,
                       void* a4, void* a5, void* a6)
{
    /* RUNG 64i — REAL ATTACH: the float's entry-level attach builds
     * the true drawable (drawbuffer at ctx+0x218) — the contract our
     * mirror skipped. Forward it. */
    {
        static long (*fn)(void*,void*,void*,void*,void*,void*) = 0;
        if (!fn) fn = (long(*)(void*,void*,void*,void*,void*,void*))
                     float_sym("gldAttachDrawable");
        if (fn) {
            long r = (long)fn(ctx, (void*)(uintptr_t)type,
                              a3, a4, a5, a6);
            /* attach-wrap (same as the +0x48 site): the float's
             * selection wrote ctx+0x66b0; wrap it. */
            if (ctx) {
                void** slot = (void**)((char*)ctx + 0x65c8 + 0xE8);
                g_float_swap_fn =
                    (long(*)(void*,void*,void*,void*,void*,void*))*slot;
                if (g_float_swap_fn != (void*)&gld_swap_wrapper) {
                    *slot = (void*)&gld_swap_wrapper;
                    char w[144];
                    snprintf(w, sizeof(w),
                             "rung64i: entry-attach-wrap: float swap %p "
                             "WRAPPED (ctx=%p)", (void*)g_float_swap_fn,
                             ctx);
                    ep_log(w);
                }
            }
            return r;
        }
    }
    (void)a3; (void)a4; (void)a5; (void)a6;
    char buf[112];
    snprintf(buf, sizeof(buf),
             "CALL gldAttachDrawable type=0x%x a3=%p a4=%p ra=%p (rung 48)",
             type, a3, a4, __builtin_return_address(0));
    ep_log(buf);
    /* RUNG 35: the float's offscreen path reads arg3 (rdx) as a
     * DESCRIPTOR: +0 width, +4 height, +8 bytes-per-row, +0x10
     * backing. Dump the window-class (0x50) descriptor's first
     * words — field separation: dims are engine-relevant; backing/
     * stride are float-internal and will NOT be mirrored. */
    if (a3 && (unsigned long)a3 > 0x1000) {
        unsigned* d = (unsigned*)a3;
        /* RUNG 39 cross-check: the FULL descriptor (16 dwords) —
         * compared against the probe's printed cid/wid/sid, this
         * names which fields the bounds call wants. */
        char b2[256];
        int o = 0;
        o += snprintf(b2 + o, sizeof(b2) - o, "  desc[16]:");
        for (int i = 0; i < 16; i++)
            o += snprintf(b2 + o, sizeof(b2) - o, " %x", d[i]);
        ep_log(b2);
        /* RUNG 64 follow block DELETED (cost one crash, its answer
         * already banked): desc+0x30 is NOT a pointer on every
         * descriptor — one rung carried {0,4} there, the "range
         * guard" accepted 0x400000000, and the blind read faulted
         * (glmark2 crash 22:14, RIP gldAttachDrawable+672, CR2
         * 0x400000000). NEVER blind-deref a descriptor field. */
    }
    if (!ctx) {
        ep_log("  gldAttachDrawable -> -1 (no ctx)");
        return -1;
    }
    if (type == 0x36) {
        ep_log("  gldAttachDrawable -> 0x271c (fullscreen-class, per the float)");
        return 0x271C;
    }
    *(unsigned*)((char*)ctx + 0x210) = type;   /* the float's store */
    /* RUNG 39 — the REAL target: the float's window path calls
     * CGSGetSurfaceBounds(desc[0], desc[4], desc[8], &rect) (grf.t
     * 0x210c6) and takes w/h from the rect. Mirror it: dlsym CGS,
     * query the bounds, create a WINDOW-SIZED virgl resource, and
     * retarget the clear at it. */
    if (type == 0x50 && a3 && (unsigned long)a3 > 0x1000) {
        /* THE LIFETIME RULE (rung 39, cost one run): the descriptor
         * is ENGINE-OWNED SCRATCH, valid only DURING the call — the
         * bounds query ran ~1s later (after CoreGraphics' dlopen) and
         * read REUSED memory (0x0/0xf where the entry dump showed
         * wid/sid). COPY the triple at entry; use the copies. */
        unsigned id0 = ((unsigned*)a3)[0];
        unsigned id1 = ((unsigned*)a3)[1];
        unsigned id2 = ((unsigned*)a3)[2];
        /* RUNG 39 second finding: at attach time the surface is NOT
         * YET ORDERED — CGSGetSurfaceBounds succeeds but returns 0x0.
         * SAVE the triple (the lifetime rule) and query LAZILY at the
         * first clear, when the surface is on screen. */
        g_sid_cid = id0; g_sid_wid = id1; g_sid_sid = id2;
        char b3[96];
        snprintf(b3, sizeof(b3),
                 "rung39: triple saved (0x%x,0x%x,0x%x) — bounds deferred to first clear",
                 id0, id1, id2);
        ep_log(b3);
    }
    ep_log("  gldAttachDrawable -> 0 (type stored; rung 39 target handling done)");
    return 0;
}
/* RUNG 34 — the honest gldInitDispatch: NOOPS IN EVERY SLOT (the
 * float's own pattern for unsupported capability, grf.t 0x14d3b).
 * Signature (ctx(rdi), dispatch_block(rsi), limits_out(rdx)); the
 * float fills exactly the offsets below and zeroes the limits block
 * (no-drawable branch: maxes [0]/[4] = 0 without ctx+0x218). NO
 * slots beyond the float's writes — the block's upper extent is
 * engine-owned. Every rendering call through this table succeeds
 * vacuously and renders nothing — consistent with the driver's
 * stated identity. The bridge replaces noops with Mesa calls,
 * slot by slot. */
static long g_noop_count = 0;
static void gld_noop_body(long slot, void* a0)
{
    (void)a0;
    if (++g_noop_count <= 16 || (g_noop_count % 1000) == 0) {
        char buf[80];
        snprintf(buf, sizeof(buf), "NOOP dispatch #%ld SLOT 0x%lx",
                 g_noop_count, slot);
        ep_log(buf);
    }
}
/* RUNG 54 — the identification thunks: one per slot offset, so the
 * log NAMES which noop the engine actually calls. The choice of the
 * next real slot follows from what fires. */
#define NOOP_FN(name, slot) \
    static void name(void* a0, void* a1, void* a2, void* a3, void* a4, void* a5) \
    { (void)a1;(void)a2;(void)a3;(void)a4;(void)a5; gld_noop_body(slot, a0); }
NOOP_FN(gld_noop_00, 0x00)
NOOP_FN(gld_noop_18, 0x18)
NOOP_FN(gld_noop_20, 0x20)
NOOP_FN(gld_noop_28, 0x28)
NOOP_FN(gld_noop_30, 0x30)
NOOP_FN(gld_noop_38, 0x38)
NOOP_FN(gld_noop_40, 0x40)
NOOP_FN(gld_noop_80, 0x80)
NOOP_FN(gld_noop_88, 0x88)
NOOP_FN(gld_noop_90, 0x90)
NOOP_FN(gld_noop_98, 0x98)
NOOP_FN(gld_noop_a0, 0xa0)
NOOP_FN(gld_noop_b8, 0xb8)
NOOP_FN(gld_noop_c0, 0xc0)
NOOP_FN(gld_noop_c8, 0xc8)
NOOP_FN(gld_noop_d0, 0xd0)
NOOP_FN(gld_noop_f0, 0xf0)
NOOP_FN(gld_noop_f8, 0xf8)
NOOP_FN(gld_noop_100, 0x100)
/* (the g_float_* wrapper globals and gld_swap_wrapper live with the
 * RUNG 64i trampoline helpers at the top of the file) */

/* our swap WRAPPER: the float's swap blits the frame into the GA
 * surface; we then push the surface to the desktop (0x600D). */
/* RUNG 66 — THE DRAW DOOR CENSUS: counting wrappers over every
 * dispatch slot the float fills. Per-frame deltas (logged at swap)
 * name the draw pipeline's slots — the ABI surface for Mesa replay. */
#define FSLOT(i) \
    static long (*g_fs_##i)(void*,void*,void*,void*,void*,void*); \
    static long g_fc_##i; \
    static long fw_##i(void* a0, void* a1, void* a2, void* a3, \
                       void* a4, void* a5) { \
        g_fc_##i++; \
        return g_fs_##i ? (long)g_fs_##i(a0,a1,a2,a3,a4,a5) : 0; }
FSLOT(00) FSLOT(08) FSLOT(10) FSLOT(18) FSLOT(20) FSLOT(28)
FSLOT(30) FSLOT(38) FSLOT(40) FSLOT(48) FSLOT(50) FSLOT(58)
FSLOT(60) FSLOT(68) FSLOT(70) FSLOT(78) FSLOT(80) FSLOT(88)
FSLOT(90) FSLOT(98) FSLOT(a0) FSLOT(a8) FSLOT(b0) FSLOT(b8)
FSLOT(c0) FSLOT(c8) FSLOT(d0) FSLOT(d8) FSLOT(e0) FSLOT(e8)
FSLOT(f0) FSLOT(f8) FSLOT(100)

/* RUNG 66c — THE BLOCK1 HOOK: capture the engine base (sub-block at
 * the create forward; engine = sub − 0x79b8), one-shot dump the
 * flush/swap neighborhood on BOTH the ctx and the engine, then poll
 * the two candidate slots (ctx+0x66b0, engine+0x66b0) until the
 * float's late selection lands; wrap on first sighting, per-site. */
static long (*g_site_fn[2])(void*,void*,void*,void*,void*,void*);
static int g_site_done[2] = { 0, 0 };
static long gld_swap_wrapper_impl(int site, void* dctx, void* a1,
                                  void* a2, void* a3, void* a4, void* a5);
static long gld_swap_wrapper_a(void* d, void* a1, void* a2, void* a3,
                               void* a4, void* a5)
{ return gld_swap_wrapper_impl(0, d, a1, a2, a3, a4, a5); }
static long gld_swap_wrapper_b(void* d, void* a1, void* a2, void* a3,
                               void* a4, void* a5)
{ return gld_swap_wrapper_impl(1, d, a1, a2, a3, a4, a5); }
static void census_report(long swapno);   /* fwd */
static void export_census_report(long swapno);   /* fwd: after EPRs */
static void rung80_compile_pending(void);        /* fwd: rung-80 section */
static const char* vsrc_r85(void)
{
    return "void main() { gl_Position = gl_Vertex; }";
}
static float g_r85_u;             /* rung 85: tracked engine value */
static int   g_r85_u_valid;
static unsigned g_r85_prog;       /* rung 85: validation program */
static void r83_probe(const char* tag, void* a0, void* a1, void* a2,
                      void* a3);                    /* fwd: rung-83 */
static void r85_poll(void* dctx);                   /* fwd: rung-85 */
static size_t r83_region_size(void* p);             /* fwd: rung-83 */
static void r86_poll(void* dctx);                   /* fwd: rung-86 */
static unsigned g_r81_prog;   /* fwd: rung-81 live passthrough (rung-80
                               * section holds the real definition) */
static int g_r82_exp[4];       /* fwd: rung-82 expected live pixel */
static int g_r84_tol;          /* fwd: rung-84 per-shape tolerance */
static int g_r84_verdict_arm;  /* fwd: re-arm verdict on GOES LIVE */
/* rung 71b NOTE: the per-frame stream re-watch (VMGLD_PPTICK) was
 * REMOVED at rung 72 — its in-process glpPPDisassemble calls are the
 * banned pattern (.claude/rules/instrumentation.md); the liveness
 * finding it produced is banked in LEDGER.md rung 71b. */
/* rung 73 NOTE: pp_draw_state(dctx) is forward-declared below the
 * pipeline instrument; the swap-site ctx is the drawbuffer owner
 * (rung 69) — poll/wrap HERE as well as at modify, because the
 * modify-site ctx's +0x188 proved DEAD (0 calls, 0 swaps, zeros
 * around it) while its texture array lives: the two engine handles
 * (+0x7120/+0x7128) are different sub-contexts. */
static void pp_draw_state(void* ctx, const char* site);
static long gld_swap_wrapper_impl(int site, void* dctx, void* a1,
                                  void* a2, void* a3, void* a4, void* a5)
{
    /* RUNG 83: the service entries fire only pre-draw; the FBO state
     * lives in the ctx (rung 66e generalized) — observe it per SWAP. */
    r83_probe(site == 0 ? "swap" : "present", dctx, a1, a2, a3);
    r85_poll(dctx);
    r86_poll(dctx);
    /* RUNG 76 — THE STUB-DRIVEN GA BINDING. Diagnosis: the GA path
     * was never automatic — SetSurface(0x800) is an APP-SIDE call
     * nothing makes (the milestone-2 boot had it because the probe
     * made it); the boot-arg gate (vm-accel-surface=1) is OPEN.
     * The stub becomes its own surface client:
     *   type 0 → sel 7  SetIDMode(0x66, 0x24)   registers surface
     *           → sel 9  SetShape(0x4, 0, rgn)  geometry (Identity-
     *                                              ScaleBit ONLY)
     *           → sel 14 WriteLock               creates the backing
     *                                            (stride base_w*4)
     *   type 2 → sel 0  SetSurface(0x66, 0x800)  GA-BOUND
     *   then 0x600E per frame: pitch-correct write at the shape +
     *   flush + push. Region struct: IOAccelDeviceRegion
     *   {u32 num_rects; 4×SInt16 bounds; 4×SInt16 rect[1]}.
     * Pre-registered: kernel logs SetIDMode STORED / SetShape
     * STORED / WriteLock alloc / SetSurface BOUND; visual = the
     * hue+triangle band at desktop (100,100)-(900,700). */
    {
        static int s_r76 = -1;
        static io_connect_t s_surf_conn;
        static io_connect_t s_3d_conn;
        if (s_r76 < 0) s_r76 = getenv("VMGLD_GPUTEST") ? 1 : 0;
        if (s_r76 && !g_virgl_conn)
            virgl_transport_init();
        if (s_r76 && g_virgl_conn && !s_surf_conn && dctx) {
            kern_return_t kr;
            io_service_t svc =
                IOServiceGetMatchingService(kIOMasterPortDefault,
                    IOServiceMatching("VMQemuVGAAccelerator"));
            if (svc) {
                kr = IOServiceOpen(svc, mach_task_self(), 0,
                                   &s_surf_conn);
                char b[96];
                snprintf(b, sizeof(b), "rung76: type0 open 0x%x "
                         "(conn=%p)", kr, (void*)s_surf_conn);
                ep_log(b);
                if (kr == KERN_SUCCESS) {
                    uint64_t sid[2] = { 0x66, 0x24 };
                    kr = IOConnectCallMethod(s_surf_conn, 7,
                        sid, 2, NULL, 0, NULL, NULL, NULL, NULL);
                    snprintf(b, sizeof(b), "rung76: SetIDMode 0x%x",
                             kr);
                    ep_log(b);
                    struct { uint32_t n; int16_t bx, by, bw, bh;
                             int16_t x, y, w, h; } rgn = {
                        1, 100, 100, 800, 600, 100, 100, 800, 600 };
                    uint64_t ss[2] = { 0x4 /*IdentityScale*/, 0 };
                    kr = IOConnectCallMethod(s_surf_conn, 9,
                        ss, 2, &rgn, sizeof(rgn),
                        NULL, NULL, NULL, NULL);
                    snprintf(b, sizeof(b), "rung76: SetShape 0x%x",
                             kr);
                    ep_log(b);
                    uint8_t info[128];
                    size_t infoLen = sizeof(info);
                    kr = IOConnectCallMethod(s_surf_conn, 14,
                        NULL, 0, NULL, 0, NULL, NULL,
                        info, &infoLen);
                    snprintf(b, sizeof(b), "rung76: WriteLock 0x%x "
                             "infoLen=%lu", kr,
                             (unsigned long)infoLen);
                    ep_log(b);
                    uint64_t un[1] = { 0 };
                    IOConnectCallMethod(s_surf_conn, 15,
                        un, 1, NULL, 0, NULL, NULL, NULL, NULL);
                }
                kr = IOServiceOpen(svc, mach_task_self(), 2,
                                   &s_3d_conn);
                if (kr == KERN_SUCCESS && s_surf_conn) {
                    uint64_t bs[2] = { 0x66, 0x800 };
                    uint8_t out44[44];
                    size_t o44 = sizeof(out44);
                    kr = IOConnectCallMethod(s_3d_conn, 0,
                        bs, 2, NULL, 0, NULL, NULL,
                        out44, &o44);
                    snprintf(b, sizeof(b), "rung76: SetSurface(0x66,"
                             "0x800) 0x%x — GA-BOUND if 0", kr);
                    ep_log(b);
                } else {
                    char b2[96];
                    snprintf(b2, sizeof(b2), "rung76: type2 open "
                             "0x%x", kr);
                    ep_log(b2);
                }
                IOObjectRelease(svc);
            }
        }
        if (s_r76 && g_virgl_conn && dctx && s_surf_conn) {
            unsigned char* drw = *(unsigned char**)
                ((char*)dctx + 0x218);
            if (drw && (uintptr_t)drw > 0x10000
                    && (uintptr_t)drw < 0x800000000000ull) {
                int w76 = *(int*)(drw + 0x8);
                int h76 = *(int*)(drw + 0xc);
                if (w76 > 16 && h76 > 16
                        && osmesa_link_init(w76, h76) == 0
                        && os_glClear && os_glFinish) {
                    {
                        static int (*s_mk)(void*, void*, unsigned,
                                           int, int);
                        if (!s_mk) s_mk = (int (*)(void*, void*,
                            unsigned, int, int))dlsym(
                                g_os_lib, "OSMesaMakeCurrent");
                        if (s_mk) s_mk(g_os_ctx, g_os_buffer,
                                       0x1401, w76, h76);
                    }
                    static float s_hue;
                    static long s_n;
                    int r78 = rung78_shader_prog();
                    s_hue += 0.02f; if (s_hue > 1.0f) s_hue -= 1.0f;
                    os_glClearColor(s_hue, 0.25f, 1.0f - s_hue, 1.0f);
                    os_glClear(0x4000);
                    if (os_glBegin && os_glEnd && os_glVertex2f
                            && os_glColor3f) {
                        os_glBegin(4);
                        os_glColor3f(1, 1, 1);
                        os_glVertex2f(-0.6f, -0.5f);
                        os_glColor3f(1, 1, 0);
                        os_glVertex2f(0.6f, -0.5f);
                        os_glColor3f(0, 1, 1);
                        os_glVertex2f(0.0f, 0.7f);
                        os_glEnd();
                    }
                    /* RUNG 78/81: full-screen pass through the
                     * compiled shader — the rung-81 passthrough when
                     * live (predicted (128,128,128,255)), else the
                     * rung-78 fixed (1,0,1,1). */
                    /* RUNG 85: the engine's live uniform word
                     * (block A word 10, jellyfish-localized) rendered
                     * AS the frame — pixel must equal quantize(u). */
                    if (g_r85_u_valid && os_glUniform1f
                            && os_glCreateShader && os_glUseProgram) {
                        static int s_built = 0;
                        if (!s_built) {
                            s_built = 1;
                            static const char* fs85 =
                                "uniform float u_engine;\n"
                                "void main() { gl_FragColor = "
                                "vec4(u_engine, u_engine, u_engine, "
                                "1.0); }\n";
                            const char* v85 = vsrc_r85();
                            unsigned vs = os_glCreateShader(0x8B31);
                            unsigned fs = os_glCreateShader(0x8B30);
                            const char* fs85p = fs85;
                            os_glShaderSource(vs, 1, &v85, NULL);
                            os_glCompileShader(vs);
                            os_glShaderSource(fs, 1, &fs85p, NULL);
                            os_glCompileShader(fs);
                            unsigned p = os_glCreateProgram();
                            os_glAttachShader(p, vs);
                            os_glAttachShader(p, fs);
                            os_glLinkProgram(p);
                            int cl = 0;
                            os_glGetProgramiv(p, 0x8B82, &cl);
                            g_r85_prog = cl ? p : 0;
                            char b2[96];
                            snprintf(b2, sizeof(b2),
                                "rung85: validation prog=%u link=%d",
                                p, cl);
                            ep_log(b2);
                        }
                    }
                    int drawprog = g_r81_prog ? (int)g_r81_prog : r78;
                    if (g_r85_prog) drawprog = (int)g_r85_prog;
                    if (drawprog > 0 && os_glUseProgram) {
                        os_glUseProgram((unsigned)drawprog);
                        if (drawprog == (int)g_r85_prog
                                && os_glGetUniformLocation) {
                            int loc = os_glGetUniformLocation(
                                g_r85_prog, "u_engine");
                            if (loc >= 0 && os_glUniform1f)
                                os_glUniform1f(loc, g_r85_u);
                        }
                        if (os_glBegin && os_glEnd && os_glVertex2f) {
                            os_glBegin(0x0005 /*GL_TRIANGLE_STRIP*/);
                            os_glVertex2f(-1.0f, -1.0f);
                            os_glVertex2f(1.0f, -1.0f);
                            os_glVertex2f(-1.0f, 1.0f);
                            os_glVertex2f(1.0f, 1.0f);
                            os_glEnd();
                        }
                        os_glUseProgram(0);
                    }
                    os_glFinish();
                    if (os_glReadPixels)
                        os_glReadPixels(0, 0, w76, h76,
                                        0x1908, 0x1401,
                                        g_os_buffer);
                    unsigned long sum = 0;
                    for (int i = 0; i < w76 * 2; i++)
                        sum += g_os_buffer[i * 4];
                    int r78mism = -1;
                    int r81mism = -1;
                    int r85mism = -1;
                    if (g_r85_prog && g_r85_u_valid) {
                        float uc = g_r85_u < 0 ? 0 :
                                   g_r85_u > 1 ? 1 : g_r85_u;
                        int ev = (int)(uc * 255.0f + 0.5f);
                        r85mism = 0;
                        for (int i = 0; i < w76 * 2; i++) {
                            int d0 = g_os_buffer[i * 4] - ev;
                            int d1 = g_os_buffer[i * 4 + 1] - ev;
                            int d2 = g_os_buffer[i * 4 + 2] - ev;
                            if (d0 < 0) d0 = -d0; if (d1 < 0) d1 = -d1;
                            if (d2 < 0) d2 = -d2;
                            if (d0 > 1 || d1 > 1 || d2 > 1
                                    || g_os_buffer[i * 4 + 3] != 255)
                                r85mism++;
                        }
                        /* periodic per-sample verdict: u varies with
                         * the jellyfish sine — sample across its range */
                        if ((s_n & 0x7) == 1 && s_n > 16
                                && g_r85_u >= 0.0f) {
                            char vb[160];
                            if (r85mism == 0)
                                snprintf(vb, sizeof(vb),
                                    "rung85[%ld]: MATCH u=%.4g -> %d",
                                    s_n, g_r85_u, ev);
                            else
                                snprintf(vb, sizeof(vb),
                                    "rung85[%ld]: MISMATCH mism=%d "
                                    "u=%.4g expected=%d "
                                    "px0=%02x%02x%02x%02x",
                                    s_n, r85mism, g_r85_u, ev,
                                    g_os_buffer[0], g_os_buffer[1],
                                    g_os_buffer[2], g_os_buffer[3]);
                            ep_log(vb);
                        }
                    } else if (g_r81_prog) {
                        /* RUNG 81: the translated passthrough must
                         * render EXACTLY att0 = vec4(0.5,0.5,0.5,1) */
                        r81mism = 0;
                        for (int i = 0; i < w76 * 2; i++) {
                            int d0 = g_os_buffer[i * 4] - g_r82_exp[0];
                            int d1 = g_os_buffer[i * 4 + 1] - g_r82_exp[1];
                            int d2 = g_os_buffer[i * 4 + 2] - g_r82_exp[2];
                            int d3 = g_os_buffer[i * 4 + 3] - g_r82_exp[3];
                            if (d0 < 0) d0 = -d0; if (d1 < 0) d1 = -d1;
                            if (d2 < 0) d2 = -d2; if (d3 < 0) d3 = -d3;
                            if (d0 > g_r84_tol || d1 > g_r84_tol
                                    || d2 > g_r84_tol || d3 > g_r84_tol)
                                r81mism++;
                        }
                        /* verdict on a LOGGED (steady) frame — the
                         * very first live frame after link mismatched
                         * once (19/19 steady frames exact; cause
                         * unverified, first-use upload is an
                         * INFERENCE). On mismatch, log the pixel. */
                        static int s_r81_verdict = 0;
                        if (g_r84_verdict_arm) {
                            g_r84_verdict_arm = 0;
                            s_r81_verdict = 0;
                        }
                        if (!s_r81_verdict && (s_n & 0xf) == 1
                                && s_n > 16) {
                            s_r81_verdict = 1;
                            if (r81mism == 0)
                                ep_log("rung81: SEMANTIC VERIFIED — "
                                       "translated passthrough renders "
                                       "(128,128,128,255) exactly");
                            else {
                                char vb[128];
                                snprintf(vb, sizeof(vb),
                                    "rung81: MISMATCH mism=%d "
                                    "px0=%02x%02x%02x%02x", r81mism,
                                    g_os_buffer[0], g_os_buffer[1],
                                    g_os_buffer[2], g_os_buffer[3]);
                                ep_log(vb);
                            }
                        }
                    } else if (r78 > 0) {
                        r78mism = 0;
                        for (int i = 0; i < w76 * 2; i++)
                            if (g_os_buffer[i * 4] != 255
                                    || g_os_buffer[i * 4 + 1] != 0
                                    || g_os_buffer[i * 4 + 2] != 255)
                                r78mism++;
                        static int s_r78_verdict = 0;
                        if (!s_r78_verdict) {
                            s_r78_verdict = 1;
                            ep_log(r78mism == 0
                                ? "rung78: VERIFIED — first shader frame "
                                  "pixel-exact (255,0,255)"
                                : "rung78: MISMATCH — shader pass ran, "
                                  "pixels not (255,0,255)");
                        }
                    }
                    kern_return_t pr = 0xe00002c2;
                    uint64_t sc76[4] = {
                        (uint64_t)(uintptr_t)g_os_buffer,
                        (uint64_t)(w76 * 4), (uint64_t)w76,
                        (uint64_t)h76 };
                    if (g_virgl_conn)
                        pr = IOConnectCallMethod(g_virgl_conn,
                            0x600E, sc76, 4, NULL, 0,
                            NULL, NULL, NULL, NULL);
                    if ((++s_n & 0xf) == 1) {
                        char b[192];
                        snprintf(b, sizeof(b),
                            "rung76/78/81: frame %ld %dx%d hue=%.2f "
                            "sum=%lu 0x600E=0x%x r78=%s mism=%d "
                            "r81=%s mism=%d",
                            s_n, w76, h76, s_hue, sum, pr,
                            r78 > 0 ? "on" : "FAIL", r78mism,
                            g_r81_prog ? "LIVE" : "-",
                            r81mism);
                        ep_log(b);
                    }
                    /* RUNG 80: compile pending live translations on
                     * this context (after the frame push; compile/link
                     * leave GL state untouched). */
                    rung80_compile_pending();
                }
            }
        }
    }
    long r = g_site_fn[site]
        ? (long)g_site_fn[site](dctx, a1, a2, a3, a4, a5) : 0;
    /* 0x600D removed: without a GA binding it failed loudly per swap
     * and FLOODED the kernel log (~3/s for hours — the flood that
     * destroyed this boot's diagnostic history). The 0x600E path
     * flushes and pushes on its own. */
    pp_draw_state(dctx, "swap");
    /* RUNG 69 — THE DISCRIMINATING EXPERIMENT: watch the drawbuffer
     * (ctx+0x218 → drawable → backing) for GLVM writes. The float's
     * swap ran above (r); if GLVM executed, the backing changed. */
    {
        static unsigned char s_prev[16];
        static int s_have_prev = 0;
        static int s_watch_left = 8;   /* first 8 swaps */
        if (dctx && s_watch_left > 0) {
            s_watch_left--;
            unsigned char* drw = *(unsigned char**)
                ((char*)dctx + 0x218);
            if (drw && (uintptr_t)drw > 0x10000
                    && (uintptr_t)drw < 0x800000000000ull) {
                /* the drawable: +0x8 w, +0xc h, +0x14 ptr chain */
                uint64_t w = *(uint32_t*)(drw + 0x8);
                uint64_t h = *(uint32_t*)(drw + 0xc);
                uint64_t chain = *(uint64_t*)(drw + 0x14);
                /* dump the drawable's words for the ABI record */
                char db[256]; int dO = 0;
                dO += snprintf(db + dO, sizeof(db) - dO,
                    "rung69: DRAWABLE %p w=%llu h=%llu chain=0x%llx:",
                    (void*)drw, (unsigned long long)w,
                    (unsigned long long)h,
                    (unsigned long long)chain);
                for (int i = 0; i < 8 && dO < 220; i++)
                    dO += snprintf(db + dO, sizeof(db) - dO,
                        " %llx",
                        *(unsigned long*)(drw + i * 8));
                ep_log(db);
                /* sample ALL candidate pointers in the drawable */
                for (int po = 0; po < 0x40; po += 8) {
                    uint64_t bk = *(uint64_t*)(drw + po);
                    if (bk <= 0x100000000ull || bk >= 0x110000000ull)
                        continue;
                    unsigned char* p = (unsigned char*)(uintptr_t)bk;
                    size_t mid = (size_t)(w * h * 4) / 2 & ~(size_t)15;
                    if (mid > 16) mid /= 2;
                    unsigned char cur[16];
                    memcpy(cur, p + mid, 16);
                    char cb[160]; int co = 0;
                    co += snprintf(cb + co, sizeof(cb) - co,
                        "rung69: PTR(+0x%x)=0x%llx mid:",
                        po, (unsigned long long)bk);
                    for (int c = 0; c < 8 && co < 110; c++)
                        co += snprintf(cb + co, sizeof(cb) - co,
                            "%02x", cur[c]);
                    /* nonzero check */
                    int nz = 0;
                    for (int c = 0; c < 16; c++) if (cur[c]) { nz = 1; break; }
                    co += snprintf(cb + co, sizeof(cb) - co,
                        " %s", nz ? "NONZERO" : "zero");
                    ep_log(cb);
                }
            } else {
                ep_log("rung69: ctx+0x218 ABSENT (no drawable)");
            }
        }
    }
    {
        static long s_swaps = 0;
        s_swaps++;
        if (s_swaps <= 5 || (s_swaps % 500) == 0) {
            census_report(s_swaps);
            export_census_report(s_swaps);   /* RUNG 66e: draw door */
        }
    }
    return r;
}
static void fw_poll(void* ctx)
{
    static int s_dumped = 0;
    if (ctx && !s_dumped) {
        s_dumped = 1;
        char b[760]; int o = 0;
        o += snprintf(b + o, sizeof(b) - o, "rung66: ctx%+[0x6580..]:",
                      0);
        for (size_t off = 0x6580; off < 0x6700 && o < 700; off += 8)
            o += snprintf(b + o, sizeof(b) - o, " %llx",
                (unsigned long long)*(unsigned long*)((char*)ctx + off));
        ep_log(b);
        if (g_engine_subblock) {
            unsigned char* eng = (unsigned char*)g_engine_subblock
                                - 0x79b8;
            char e[520]; int eo = 0;
            eo += snprintf(e + eo, sizeof(e) - eo,
                           "rung66: eng %p[0x6680..]:", (void*)eng);
            for (size_t off = 0x6680; off < 0x66e0 && eo < 470; off += 8)
                eo += snprintf(e + eo, sizeof(e) - eo, " %llx",
                    (unsigned long long)*(unsigned long*)(eng + off));
            ep_log(e);
        }
    }
    if (!ctx) return;
    void** slots[2] = { (void**)((char*)ctx + 0x66b0), NULL };
    if (g_engine_subblock)
        slots[1] = (void**)((unsigned char*)g_engine_subblock
                            - 0x79b8 + 0x66b0);
    static void* const wraps[2] = { (void*)&gld_swap_wrapper_a,
                                    (void*)&gld_swap_wrapper_b };
    for (int s = 0; s < 2; s++) {
        if (!slots[s] || g_site_done[s]) continue;
        void* p = *slots[s];
        if (p && p != wraps[s]) {
            g_site_fn[s] =
                (long(*)(void*,void*,void*,void*,void*,void*))p;
            *slots[s] = wraps[s];
            g_site_done[s] = 1;
            char w[144];
            snprintf(w, sizeof(w),
                     "rung66: SITE%d WRAPPED: float swap %p at %p",
                     s, p, (void*)slots[s]);
            ep_log(w);
        }
    }
}
static void census_install(unsigned long* dispatch)
{
    struct { int off; void** save; long** cnt; void* wrap; } S[] = {
#define SE(nm, off) { off, &g_fs_##nm, &g_fc_##nm, (void*)&fw_##nm }
        SE(00, 0x00), SE(08, 0x08), SE(10, 0x10), SE(18, 0x18),
        SE(20, 0x20), SE(28, 0x28), SE(30, 0x30), SE(38, 0x38),
        SE(40, 0x40), SE(48, 0x48), SE(50, 0x50), SE(58, 0x58),
        SE(60, 0x60), SE(68, 0x68), SE(70, 0x70), SE(78, 0x78),
        SE(80, 0x80), SE(88, 0x88), SE(90, 0x90), SE(98, 0x98),
        SE(a0, 0xa0), SE(a8, 0xa8), SE(b0, 0xb0), SE(b8, 0xb8),
        SE(c0, 0xc0), SE(c8, 0xc8), SE(d0, 0xd0), SE(d8, 0xd8),
        SE(e0, 0xe0), SE(e8, 0xe8), SE(f0, 0xf0), SE(f8, 0xf8),
        SE(100, 0x100),
#undef SE
    };
    for (unsigned k = 0; k < sizeof(S) / sizeof(S[0]); k++) {
        void* p = *(void**)((char*)dispatch + S[k].off);
        if (!p) continue;
        if (p == S[k].wrap) continue;         /* already wrapped */
        *S[k].save = p;
        *S[k].cnt = 0;
        *(void**)((char*)dispatch + S[k].off) = S[k].wrap;
    }
}
static void census_report(long swapno)
{
    char b[640]; int o = 0;
    o += snprintf(b + o, sizeof(b) - o, "rung66 swap %ld deltas:", swapno);
#define CR(nm, off) if (g_fc_##nm) o += snprintf(b + o, sizeof(b) - o, \
        " %03x:+%ld", off, g_fc_##nm), g_fc_##nm = 0
    CR(00, 0x00); CR(08, 0x08); CR(10, 0x10); CR(18, 0x18);
    CR(20, 0x20); CR(28, 0x28); CR(30, 0x30); CR(38, 0x38);
    CR(40, 0x40); CR(48, 0x48); CR(50, 0x50); CR(58, 0x58);
    CR(60, 0x60); CR(68, 0x68); CR(70, 0x70); CR(78, 0x78);
    CR(80, 0x80); CR(88, 0x88); CR(90, 0x90); CR(98, 0x98);
    CR(a0, 0xa0); CR(a8, 0xa8); CR(b0, 0xb0); CR(b8, 0xb8);
    CR(c0, 0xc0); CR(c8, 0xc8); CR(d0, 0xd0); CR(d8, 0xd8);
    CR(e0, 0xe0); CR(e8, 0xe8); CR(f0, 0xf0); CR(f8, 0xf8);
    CR(100, 0x100);
#undef CR
    ep_log(b);
}

long gld_swap_wrapper(void* dctx, void* a1, void* a2, void* a3,
                      void* a4, void* a5)
{
    long r = g_float_swap_fn
        ? (long)g_float_swap_fn(dctx, a1, a2, a3, a4, a5) : 0;
    if (g_virgl_conn)
        IOConnectCallMethod(g_virgl_conn, 0x600D, NULL, 0, NULL, 0,
                            NULL, NULL, NULL, NULL);
    {
        static long s_swaps = 0;
        s_swaps++;
        if (s_swaps <= 5 || (s_swaps % 500) == 0)
            census_report(s_swaps);
    }
    return r;
}

long gldInitDispatch(void* ctx, unsigned long* dispatch,
                     unsigned* limits, void* a3, void* a4, void* a5)
{
    (void)ctx; (void)a3; (void)a4; (void)a5;
    char buf[96];
    snprintf(buf, sizeof(buf),
             "CALL gldInitDispatch ctx=%p dispatch=%p limits=%p (rung 34/64i)",
             ctx, (void*)dispatch, (void*)limits);
    ep_log(buf);
    if (!dispatch) {
        ep_log("  gldInitDispatch -> -1 (no dispatch block)");
        return -1;
    }
    /* RUNG 64i — FLOAT DISPATCH FIRST: the float fills the table with
     * its REAL entries (draws, textures, clears — the working
     * software GL). Then we overlay +0x50/+0x48 with OUR wrappers so
     * the swap gets wrapped after the float's install fills block1,
     * and the attach keeps our log/pf-side bookkeeping ahead of the
     * float's. If the float is unavailable, fall through to the
     * identifying-thunk fill below (the rung-54 behavior). */
    {
        static long (*fn)(void*,void*,void*,void*,void*,void*) = 0;
        if (!fn) fn = (long(*)(void*,void*,void*,void*,void*,void*))
                     float_sym("gldInitDispatch");
        if (fn) {
            long r = (long)fn(ctx, dispatch, limits, a3, a4, a5);
            g_float_install =
                (long(*)(void*,void*,void*,void*,void*,void*))
                *(void**)((char*)dispatch + 0x50);
            g_float_attach =
                (long(*)(void*,void*,void*,void*,void*,void*))
                *(void**)((char*)dispatch + 0x48);
            census_install(dispatch);        /* RUNG 66: count draws */
            *(void**)((char*)dispatch + 0x50) =
                (void*)&gld_fill_engine_calls;
            *(void**)((char*)dispatch + 0x48) =
                (void*)&gld_table_attach;
            ep_log("  rung64i: float dispatch installed; census on; "
                   "+0x50/+0x48 wrapped");
            return r;
        }
    }
    /* Every offset the float writes (grf.t 0x14da0-0x14f73) — one
     * IDENTIFYING thunk per still-noop slot (rung 54). */
    *(void**)((char*)dispatch + 0x00)  = (void*)&gld_noop_00;
    *(void**)((char*)dispatch + 0x18)  = (void*)&gld_noop_18;
    *(void**)((char*)dispatch + 0x20)  = (void*)&gld_noop_20;
    *(void**)((char*)dispatch + 0x28)  = (void*)&gld_noop_28;
    *(void**)((char*)dispatch + 0x30)  = (void*)&gld_noop_30;
    *(void**)((char*)dispatch + 0x38)  = (void*)&gld_noop_38;
    *(void**)((char*)dispatch + 0x40)  = (void*)&gld_noop_40;
    *(void**)((char*)dispatch + 0x80)  = (void*)&gld_noop_80;
    *(void**)((char*)dispatch + 0x88)  = (void*)&gld_noop_88;
    *(void**)((char*)dispatch + 0x90)  = (void*)&gld_noop_90;
    *(void**)((char*)dispatch + 0x98)  = (void*)&gld_noop_98;
    *(void**)((char*)dispatch + 0xa0)  = (void*)&gld_noop_a0;
    *(void**)((char*)dispatch + 0xb8)  = (void*)&gld_noop_b8;
    *(void**)((char*)dispatch + 0xc0)  = (void*)&gld_noop_c0;
    *(void**)((char*)dispatch + 0xc8)  = (void*)&gld_noop_c8;
    *(void**)((char*)dispatch + 0xd0)  = (void*)&gld_noop_d0;
    *(void**)((char*)dispatch + 0xf0)  = (void*)&gld_noop_f0;
    *(void**)((char*)dispatch + 0xf8)  = (void*)&gld_noop_f8;
    *(void**)((char*)dispatch + 0x100) = (void*)&gld_noop_100;
    *(void**)((char*)dispatch + 0x8) = (void*)&gld_clear_real;   /* RUNG 37/38 */
    *(void**)((char*)dispatch + 0x10) = (void*)&gld_readpixels_real; /* RUNG 38 */
    /* RUNG 46: the install entry — the engine calls this slot
     * (via [engine+0x6758], gle 0x15d132) to have the driver fill
     * its flush/swap entries into the engine's call block. NOT in
     * kSlots (the rung-34 gap — the float stores here too, grf
     * 0x14f9e). */
    *(void**)((char*)dispatch + 0x50) = (void*)&gld_fill_engine_calls;
    /* RUNG 48 — THE ATTACH SLOT: gliAttachDrawableWithOptions calls
     * the driver through table+0x48 (gle 0xf444, driver-obj+0x168)
     * and CHECKS THE RESULT (0xf44d+); a passing attach is what
     * sets [engine+0x6570] (the drawable object) — the hardware
     * classification that gates the install and the swap. The
     * noop's void/garbage return failed the parse. */
    *(void**)((char*)dispatch + 0x48) = (void*)&gld_table_attach;
    if (limits) {
        for (int i = 0; i < 6; i++)          /* +0..+0x14, the float's zeros */
            limits[i] = 0;
        /* RUNG 35 (prediction (i) test): the limits are CAPABILITY
         * MAXES (the float clamps drawable-derived values to 0x4000);
         * hypothesis: the engine's 0x506 dispatch gate is nonzero
         * limits. The float's own cap is honest for a noop table
         * (vacuously satisfiable — nothing is ever touched). */
        limits[0] = 0x4000;
        limits[1] = 0x4000;
    }
    ep_log("  gldInitDispatch -> 0 (22 noop slots; limits maxes=0x4000)");
    /* RUNG 48 — THE DIRECT INSTALL: the classification transition
     * ran BEFORE this function filled the table (attach precedes
     * InitDispatch; the engine's [0x6758] call hit a pre-fill table
     * and the transition never repeats). The sub-block pointer
     * gives the engine base (sub-block = engine+0x79b8, idx 0);
     * block1 = engine+0x65c8; write the flush/swap entries the
     * engine would have installed: +0xE0 = flush ([0x66a8]), +0xE8
     * = swap ([0x66b0]). */
    if (g_engine_subblock) {
        char* eng = (char*)g_engine_subblock - 0x79b8;
        char* block1 = eng + 0x65c8;
        *(void**)(block1 + 0xE0) = (void*)&gld_flush_entry;
        *(void**)(block1 + 0xE8) = (void*)&gld_swap_entry;
        char b2[128];
        snprintf(b2, sizeof(b2),
                 "rung48: DIRECT INSTALL engine=%p block1=%p "
                 "(+0xE0 flush, +0xE8 swap written)",
                 (void*)eng, (void*)block1);
        ep_log(b2);
    }
    return 0;
}
/* RUNG 36 — gldUpdateDispatch is THE DISPATCHER'S ENTRY (slot 11,
 * [engine-ctx+0x6760] after the engine memcpy'd our whole table
 * into its context at +0x6708). gleDoSelectiveDispatchCore calls
 * it as (driver_ctx, template_block, dirty_block) and gates on
 * the return bits: bit 2 (4) to continue, bit 0 (1) vs the
 * engine's 0x798b. The FLOAT's base return is 4 (or 0xC), bit 2
 * always set (grf.t 0x15399/0x153d8); its dirty-block ORs
 * (0x80/0x100/0x10000380) mark real state changes. The stub has
 * no state to change: return the float's base, mark nothing. */
long gldUpdateDispatch(void* ctx, void* template_, void* dirty,
                       void* a3, void* a4, void* a5)
{
    fw_poll(ctx);            /* RUNG 66: BEFORE the forward (it returns) */
    GLD_FWD("gldUpdateDispatch", ctx, template_, dirty, a3, a4, a5, 0);
    (void)template_; (void)dirty; (void)a3; (void)a4; (void)a5;
    static long n = 0;
    if (++n <= 5 || (n % 500) == 0) {
        char buf[80];
        snprintf(buf, sizeof(buf),
                 "CALL gldUpdateDispatch #%ld (ctx=%p) -> 4 (rung 36)",
                 n, ctx);
        ep_log(buf);
    }
    return 4;
}
/* RUNG 31 — the first return-something-real entry. The float's
 * shape (grf.t 0x1dafc): switch on (name - 0x1F00), six names,
 * const char* or NULL; ctx unused. The honest string set: our
 * identity for vendor/renderer, a version that claims NO GL
 * capability the stub refuses to implement. The bridge later
 * replaces these with Mesa's real answers. */
const char* gldGetString(void* ctx, unsigned name,
                         void* a2, void* a3, void* a4, void* a5)
{
    (void)ctx; (void)a2; (void)a3; (void)a4; (void)a5;
    const char* s = NULL;
    switch (name) {
    case 0x1F00: s = "VMQemuVGA Project";                 break; /* GL_VENDOR   */
    case 0x1F01:                                            /* GL_RENDERER */
        /* RUNG 53 — the DEVICE'S NAME (a fact statement: the
         * hardware that executes what we submit, named by the
         * capset). Built once; NUL-forced; the stub string is
         * the fallback when the capset is absent. GL_VERSION
         * stays honest — a version is a capability claim and
         * most entries are still noops. */
        {
            static char rend[80];
            static int rend_built = 0;
            if (!rend_built) {
                rend_built = 1;
                rend[0] = 0;
                if (g_caps_fetched && g_caps_v2_ok && g_caps.renderer[0]) {
                    char tmp[65];
                    memcpy(tmp, g_caps.renderer, 64);
                    tmp[64] = 0;
                    size_t l = strlen(tmp);
                    while (l && (tmp[l-1] == ' ' || tmp[l-1] == 0)) tmp[--l] = 0;
                    snprintf(rend, sizeof(rend), "%s (virgl)", tmp);
                }
            }
            s = rend[0] ? rend : "VirtIO GPU stub (software, no rendering)";
        }
        break;
    case 0x1F02: {                                          /* GL_VERSION — RUNG 59 */
        /* DERIVED FROM THE CAPSET (the device-truth principle, rung
         * 52): glsl_level 410 = the host's GL 4.1-class capability —
         * the same blob that names the renderer and the limits. The
         * engine's complete software GL is the named fallback (and
         * the rasterizer for what doesn't route to the host). */
        static char vsn[80];
        static int vsn_built = 0;
        if (!vsn_built) {
            vsn_built = 1;
            if (g_caps_fetched && g_caps.v1.glsl_level >= 100) {
                /* RUNG 61 correction (the rung-60 attribution was
                 * WRONG): UTM's Display panel shows "Apple Core
                 * OpenGL" was already the renderer backend — the
                 * ANGLE/dxmt env lines in the debug log belong to
                 * UTM's own display pipeline (CocoaSpice drawing the
                 * guest screen), NOT to virglrenderer's context. The
                 * capset numbers were measured under a desktop GL
                 * host all along. */
                snprintf(vsn, sizeof(vsn), "%u.%u (virgl, Apple Core OpenGL backend; engine software fallback)",
                         g_caps.v1.glsl_level / 100, g_caps.v1.glsl_level % 100 / 10);
            } else {
                snprintf(vsn, sizeof(vsn), "2.1 VMQemuVGA (engine software)");
            }
        }
        s = vsn;
        break;
    }
    case 0x8B8C:                                            /* GL_SHADING_LANGUAGE_VERSION */
        /* RUNG 59 — glmark2's GL-state gate requires it; derived from
         * the capset's glsl_level (410 -> "4.10"), the same
         * device-truth source as the version and renderer. */
        {
            static char glsl[16];
            static int glsl_built = 0;
            if (!glsl_built) {
                glsl_built = 1;
                unsigned lv = (g_caps_fetched && g_caps.v1.glsl_level)
                              ? g_caps.v1.glsl_level : 120;
                snprintf(glsl, sizeof(glsl), "%u.%02u", lv / 100, lv % 100);
            }
            s = glsl;
        }
        break;
    default:     s = NULL;                                 break;
    }
    char buf[96];
    snprintf(buf, sizeof(buf),
             "CALL gldGetString(0x%x) -> \"%s\" (rung 31)",
             name, s ? s : "(NULL)");
    ep_log(buf);
    return s;
}
EPV(gldGetError)
EPR(gldSetInteger)
EPR(gldGetInteger)
EPR(gldFlush)
EPR(gldFinish)
EPR(gldTestObject)
EPR(gldFlushObject)
EPR(gldFinishObject)
EPR(gldWaitObject)
EPR(gldIsTextureResident)
EPV(gldUnbindTexture)
EPR(gldReclaimTexture)
EPV(gldDestroyTexture)
EPR(gldGetTextureLevelInfo)
EPR(gldGetTextureLevelImage)
EPR(gldDestroyTextureLevel)
EPR(gldCreateBuffer)
EPR(gldLoadBuffer)
EPR(gldFlushBuffer)
EPR(gldPageoffBuffer)
EPR(gldUnbindBuffer)
EPR(gldReclaimBuffer)
EPR(gldDestroyBuffer)
EPR(gldGetMemoryPlugin)
EPR(gldSetMemoryPlugin)
EPR(gldTestMemoryPlugin)
EPR(gldFlushMemoryPlugin)
EPR(gldDestroyMemoryPlugin)
/* RUNG 71 — THE PP DUMP INSTRUMENT (pre-registered in the ledger
 * 2026-08-24, before this code): at create/modify time, dump the
 * descriptor's raw fields and call the float's own disassembler on
 * the linked PP stream. All facts used here are from the static
 * decode in docs/pipeline-program-abi.md:
 *   desc+0x00 u16 type (0x8B30 frag / 0x8B31 vert)
 *   desc+0x02 byte target (float's create accepts only 2)
 *   desc+0x08    the linked PP stream (engine prog+0xfe8 pre-link 0)
 *   stream+0x02  u16 refcount, stream+0x10 u32 word count (8*count bytes)
 *   glpPPDisassemble(stream) -> malloc'd text; non-exported at file
 *     offset 0xd5ba5 of the x86_64 slice; guest's libGLProgrammability
 *     md5 == the disassembled copy (a0185546b98c1a020bb9474391155c75),
 *     __TEXT vmaddr 0, so runtime addr = image_header + 0xd5ba5.
 * The thunk-timing hazard is why capture is at create/modify: by draw
 * time ctx->0x188 has been swapped by glvmRequestFunctionPointerWrite. */
#include <sys/stat.h>
#include <sys/time.h>
/* RUNG 72 — THE RAW-WORD CAPTURE CORPUS. Per
 * .claude/rules/instrumentation.md: capture copies bytes to a file;
 * decoding happens OFFLINE in a host tool (ppdecode). No in-process
 * disassembler — the rung-71b lesson. The dump extent is exactly
 * what the float's own compiler consumes:
 * glvmBuildModularFunctionDeferred's memcpy of 8*stream->0x10 bytes
 * from the stream start (header words included — word 0 carries the
 * type, so each file self-identifies).
 * Both target-2 types are captured: 0x8B30 (GLSL fragment) and
 * 0x8804 (raster-op — the gldAddRasterOpsToPPStream merge). 0x8B31
 * never arrives (rung 71), but is accepted if it ever does. */
static int g_ppdump_seq;

/* RUNG 83 — THE BIND-TIME INSTRUMENT (gated VMGLD_R83). The
 * gldClearDrawBuffer crash (rung 82b): the float clears ctx draw-rect
 * over ctx+0x360[i] images with no bounds knowledge. Here, at the
 * texture/framebuffer service entries, log the LIVE state: rect
 * (ctx+0x230..0x244), the 8 image pointers, and each image's
 * mach_vm_region SIZE (= the true allocated extent; no object-graph
 * chase). Verdict per slot: need = w*h*4 vs region size — DISAGREE
 * names the index i (the rung-83 addition). Reads are flat fields of
 * the arg object, vm-guarded first; precedent: rung-73 pp_draw_state. */
#include <mach/mach_vm.h>
static float bits_to_f(unsigned int u)
{
    float f;
    memcpy(&f, &u, 4);
    return f;
}
static float g_r80_slot_tmp[16][4];
static int   g_r80_nprm_tmp;
static int g_r83_gate = -1;
static long g_r83_logged;
/* RUNG 85 — the operation-stack blocks (rung 74: ctx+0xe00/+0xf50 hold
 * the per-draw live uniform values; DIRECT fixed-offset reads only,
 * per the rung-74 crash rules). Animation finder: static copies, log
 * CHANGED words per swap; the jellyfish uCurrentTime signature (rung
 * 71b: animates every frame) should surface as one continuously
 * changing float. Gate VMGLD_R85. */
#define R85_WORDS 0x30
static void r85_poll(void* dctx)
{
    static int s_gate = -1;
    static unsigned long long s_a[R85_WORDS], s_b[R85_WORDS];
    static int s_have = 0;
    static long s_n = 0;
    if (s_gate < 0) s_gate = getenv("VMGLD_R85") ? 1 : 0;
    if (!s_gate || !dctx) return;
    if ((uintptr_t)dctx < 0x1000) return;
    unsigned long long* A = (unsigned long long*)((char*)dctx + 0xe00);
    unsigned long long* B = (unsigned long long*)((char*)dctx + 0xf50);
    int nchg = 0;
    char buf[480]; int o = 0;
    o += snprintf(buf + o, sizeof(buf) - o, "rung85[%ld]:", s_n);
    for (int i = 0; i < R85_WORDS; i++) {
        unsigned long long a = A[i], b = B[i];
        int ca = s_have && a != s_a[i];
        int cb = s_have && b != s_b[i];
        s_a[i] = a; s_b[i] = b;
        if (i == 10) {                  /* the jellyfish-animated slot */
            memcpy(&g_r85_u, &a, 4);
            g_r85_u_valid = 1;
        }
        if ((ca || cb) && o < (int)sizeof(buf) - 60) {
            float fa, fb;
            memcpy(&fa, &a, 4); memcpy(&fb, &b, 4);
            o += snprintf(buf + o, sizeof(buf) - o,
                " %s%d=%llx(%.4g)%s", ca ? "A" : "B", i,
                ca ? a : b, ca ? fa : fb, "");
            nchg++;
        }
    }
    s_n++;
    if (!s_have) { s_have = 1; ep_log("rung85: baseline set"); return; }
    if (nchg) ep_log(buf);          /* log every swap WITH changes */
    else if ((s_n & 0x3f) == 0) ep_log("rung85: no-change swap");
}
/* RUNG 86 — the DESCRIPTOR DECODE (rung 74's disassembly, exact
 * arithmetic from libGLVMPlugin x86_64):
 *   glvmPreloadFPTransformFour(token, ..., stack-args):
 *     flags = token[+0x08]; n = u32@token+0x7c;
 *     per set bit 0x4000<<k: desc = token_word[n+1+k]
 *       -> glvmOperationStackPreloadBuffer(rdi=stack+0x18, ..., desc):
 *            sel  = (desc & 0xFF) >> 3; counts = desc>>32 & 0xFFFF;
 *            type = desc>>16 & 0xF;
 *            addr = rdi + (sel-10)*8 + scale(counts,type,args)
 * VALIDATION: some descriptor resolves to block A word 10 — the
 * rung-85 animated float. Gate VMGLD_R86; vm-guarded token deref. */
static void r86_poll(void* dctx)
{
    static int s_gate = -1;
    static long s_n = 0;
    if (s_gate < 0) s_gate = getenv("VMGLD_R86") ? 1 : 0;
    if (!s_gate || !dctx) return;
    if ((++s_n & 0x3f) != 1) return;
    void* tok = *(void**)((char*)dctx + 0x198);
    if ((uintptr_t)tok < 0x10000) return;
    if (r83_region_size(tok) < 0x400) return;
    unsigned long long flags = ((unsigned long long*)tok)[1];
    unsigned n = *(unsigned*)((char*)tok + 0x7c);
    if (n > 0x100) { ep_log("rung86: n out of range"); return; }
    char b[480];
    int o = snprintf(b, sizeof(b),
        "rung86: tok=%p flags=%llx n=%u", tok, flags, n);
    unsigned long long* tw = (unsigned long long*)tok;
    for (int k = 0; k < 7; k++) {
        if (!(flags & (0x4000ULL << k))) continue;
        unsigned long long d = tw[n + 1 + k];
        unsigned sel = d & 0xFF;
        unsigned cnt = (unsigned)((d >> 32) & 0xFFFF);
        unsigned typ = (unsigned)((d >> 16) & 0xF);
        if (o < (int)sizeof(b) - 90)
            o += snprintf(b + o, sizeof(b) - o,
                " | k%d sel=%u(s3=%u) type=%u cnt=%u",
                k, sel, sel >> 3, typ, cnt);
    }
    {
        unsigned long long* A =
            (unsigned long long*)((char*)dctx + 0xe00);
        if (o < (int)sizeof(b) - 90)
            o += snprintf(b + o, sizeof(b) - o,
                " || A10=%llx", A[10]);
    }
    ep_log(b);
}
void* g_float_base = 0;   /* RUNG 83: the float's runtime load address,
                           * published for gdb (single-session breakpoint
                           * math; the slide drifts run to run) */
static void r83_find_float_base(void)
{
    if (g_float_base) return;
    uint32_t n = _dyld_image_count();
    for (uint32_t i = 0; i < n; i++) {
        const char* nm = _dyld_get_image_name(i);
        if (nm && strstr(nm, "GLRendererFloat")) {
            g_float_base = (void*)_dyld_get_image_header(i);
            char b[96];
            snprintf(b, sizeof(b), "rung83: FLOAT BASE=%p (idx %u)",
                     g_float_base, i);
            ep_log(b);
            return;
        }
    }
}
static size_t r83_region_size(void* p)
{
    mach_vm_address_t addr = (mach_vm_address_t)p;
    mach_vm_size_t sz = 0;
    vm_region_basic_info_data_64_t info;
    mach_msg_type_number_t cnt = VM_REGION_BASIC_INFO_COUNT_64;
    mach_port_t obj = MACH_PORT_NULL;
    if (mach_vm_region(mach_task_self(), &addr, &sz,
                       VM_REGION_BASIC_INFO_64,
                       (vm_region_info_t)&info, &cnt,
                       &obj) != KERN_SUCCESS)
        return 0;
    /* object port intentionally leaked-once (deallocation not in the
       10.6 SDK headers reachable from here); one port per probe call */
    return (size_t)sz;
}

/* first-calls-per-tag raw logging: settles WHETHER the entry fires and
 * WHICH arg is the renderer ctx, before any filter can hide it */
static void r83_raw(const char* tag, void* a0, void* a1, void* a2, void* a3)
{
    static char seen[16][40];
    static int nseen;
    for (int i = 0; i < nseen; i++)
        if (strncmp(seen[i], tag, 39) == 0) return;
    if (nseen < 16) { strncpy(seen[nseen], tag, 39); nseen++; }
    char b[320];
    snprintf(b, sizeof(b),
        "rung83 RAW[%s]: a0=%p(r%zu) a1=%p(r%zu) a2=%p(r%zu) a3=%p(r%zu)",
        tag, a0, r83_region_size(a0), a1, r83_region_size(a1),
        a2, r83_region_size(a2), a3, r83_region_size(a3));
    ep_log(b);
}
static void r83_probe(const char* tag, void* a0, void* a1, void* a2,
                      void* a3)
{
    if (g_r83_gate < 0) g_r83_gate = getenv("VMGLD_R83") ? 1 : 0;
    r83_find_float_base();          /* unconditional: gdb needs it early */
    if (!g_r83_gate || !a0) return;
    if ((uintptr_t)a0 < 0x1000) return;
    if (r83_region_size(a0) == 0) return;          /* unmapped: skip */
    unsigned rect[4];
    memcpy(rect, (char*)a0 + 0x230, sizeof(rect)); /* x,y,w,h */
    if (rect[2] > 16384 || rect[3] > 16384)
        return;                                    /* not a ctx-shaped obj */
    int have_rect = rect[2] != 0 && rect[3] != 0;
    unsigned long long need = (unsigned long long)rect[2] * rect[3] * 4;
    char b[512];
    int o = snprintf(b, sizeof(b),
        "rung83[%s]: a0=%p a1=%p a2=%p a3=%p rect=%u,%u %ux%u need=%llu",
        tag, a0, a1, a2, a3, rect[0], rect[1], rect[2], rect[3], need);
    int disagree[8];
    for (int i = 0; i < 8; i++) {
        void* img = *(void**)((char*)a0 + 0x360 + i * 8);
        disagree[i] = 0;
        if (img && (uintptr_t)img > 0x1000) {
            size_t rsz = r83_region_size(img);
            int bad = (rsz > 0 && rsz < need);
            disagree[i] = bad;
            if (o < (int)sizeof(b) - 80)
                o += snprintf(b + o, sizeof(b) - o,
                    " [%d]=%p/%zu%s", i, img, rsz, bad ? " DISAGREE" : "");
        }
    }
    int any = 0;
    for (int i = 0; i < 8; i++) any |= disagree[i];
    if (any || (have_rect && g_r83_logged < 40) || (!have_rect && g_r83_logged < 3)) {
        g_r83_logged++;
        ep_log(b);
    }
}
/* explicit bodies for the twelve instrumented entries — semantics
 * identical to EPR (forward, refuse, zero out) with the probe before */
#define R83E(n) static long g_ec_##n; \
long n(void* a0, void* a1, void* a2, void* a3, void* a4, void* a5) { \
    g_ec_##n++; \
    r83_raw(#n, a0, a1, a2, a3); \
    r83_probe(#n, a0, a1, a2, a3); \
    GLD_FWD(#n, a0,a1,a2,a3,a4,a5, -1); \
    if (a0) *(void**)a0 = (void*)0; \
    return -1; }

R83E(gldCreateFramebuffer)
R83E(gldUnbindFramebuffer)
R83E(gldReclaimFramebuffer)
R83E(gldDestroyFramebuffer)
R83E(gldDiscardFramebuffer)
R83E(gldCreateTextureLevel)
R83E(gldModifyTextureLevel)
R83E(gldGetTextureLevel)
R83E(gldCreateTexture)
R83E(gldModifyTexture)
R83E(gldLoadTexture)
R83E(gldSyncTexture)

static void rung80_translate(const unsigned long long* ws, unsigned words);
static void pp_dump_desc(void* desc, const char* site)
{
    static struct { void* desc; int dumped; } seen[64];
    if (!desc || desc == (void*)0x123) return;
    int slot = -1, freeb = -1;
    for (int i = 0; i < 64; i++) {
        if (seen[i].desc == desc) { slot = i; break; }
        if (!seen[i].desc && freeb < 0) freeb = i;
    }
    if (slot >= 0 && seen[slot].dumped) return;   /* already dumped */
    if (slot < 0) {
        if (freeb < 0) return;          /* table full: drop silently */
        seen[freeb].desc = desc;
        seen[freeb].dumped = 0;
        slot = freeb;
    }
    unsigned stype = *(unsigned short*)desc;
    unsigned target = *(unsigned char*)((char*)desc + 2);
    void* stream = *(void**)((char*)desc + 8);
    void* texinfo = *(void**)((char*)desc + 0x18);
    char b[224];
    snprintf(b, sizeof(b),
        "rung72[%s]: desc=%p type=0x%04x target=%u stream=%p texinfo=%p",
        site, desc, stype, target, stream, texinfo);
    ep_log(b);
    if (!stream) return;                /* pre-link: retry next modify */
    unsigned rtype = *(unsigned short*)stream;
    unsigned refc   = *(unsigned short*)((char*)stream + 2);
    unsigned words  = *(unsigned*)((char*)stream + 0x10);
    if (rtype != 0x8b30 && rtype != 0x8804 && rtype != 0x8b31) {
        snprintf(b, sizeof(b), "rung72[%s]: SKIP (unknown type 0x%04x)",
                 site, rtype);
        ep_log(b);
        return;
    }
    if (words == 0 || words > 0x100000u) {
        snprintf(b, sizeof(b), "rung72[%s]: SKIP (words=%u outside "
                 "sane range)", site, words);
        ep_log(b);
        return;
    }
    seen[slot].dumped = 1;
    snprintf(b, sizeof(b),
        "rung72[%s]: stream=%p type=0x%04x refc=%u words=%u bytes=%u",
        site, stream, rtype, refc, words, words * 8u);
    ep_log(b);
    /* one fopen+fwrite+fclose per stream — allocations freed before
     * return (the instrumentation rule's lifetime bullet). Files are
     * per-pid: root (WindowServer) and user processes never collide. */
    mkdir("/tmp/ppdump", 0777);
    chmod("/tmp/ppdump", 0777);
    char path[96];
    snprintf(path, sizeof(path), "/tmp/ppdump/p%d_s%d_t%04x_w%u.bin",
             (int)getpid(), ++g_ppdump_seq, rtype, words);
    FILE* f = fopen(path, "w");
    if (!f) { ep_log("rung72: fopen FAILED"); return; }
    size_t n = fwrite(stream, 8u, words, f);
    fclose(f);
    snprintf(b, sizeof(b), "rung72: WROTE %s (%llu/%u words)",
             path, (unsigned long long)n, words);
    ep_log(b);
    if (n != words)
        ep_log("rung72: MISMATCH — short write; corpus file incomplete");
    /* RUNG 80: fragment streams additionally feed the live translator
     * (pure string build; compile happens at the swap site). */
    if (rtype == 0x8b30)
        rung80_translate((const unsigned long long*)stream, words);
}

/* RUNG 80 — LIVE TRANSLATION IN-STUB. The rung-79 tables as C; the
 * stream is translated to GLSL at modify/create time (pure string
 * building — no GL calls, no engine state touched) and compiled on
 * the embedded OSMesa context at the NEXT swap site (rung-78 path).
 * Everything here is corpus-derived (LEDGER rung 79):
 *   anchor = word after the UNIQUE 0x4c0 marker (22/22 corpus files);
 *   L = n+1 uniform; +1 word for IF (target) and ENDIF (index);
 *   src word: class=(low24>>6)&3, index=u16@+0x6, swizzle=delta table,
 *             negate=bit4;
 *   dst word: index=u16@+0x6, mask=table keyed low32&0x0FFFFFFF;
 *   TEX word3 = sampler (prm ref), word4 = kind (3 = 2D);
 *   IF's condition word is ZERO (like RET's) — the condition lives in
 *   the engine's runtime bool table, NOT in the stream: emitted as a
 *   ppcond[] uniform (semantics open, compile-valid).
 * Gate: VMGLD_GPUTEST (same boot gate as rungs 76/78 — one variable). */
static const struct { unsigned op; const char* mn; short n; } kR80Ops[] = {
    {0,"MOV",2},{31,"ADD",3},{32,"SUB",3},{34,"MUL",3},{58,"DIV",3},
    {39,"DOT",3},{45,"MAX",3},{44,"MIN",3},{21,"NRM",2},{18,"LEN",2},
    {56,"POW",3},{60,"LRP",4},{47,"RFL",3},{50,"SGE",3},{51,"SGT",3},
    {53,"SLT",3},{36,"ANL",3},{66,"TEX",4},{77,"RET",1},{85,"IF",1},
    {89,"ENDIF",0},{16,"EX2",1},{76,"CAL",1},
};
static const struct { unsigned d; const char* sw; char nc; } kR80Swz[] = {
    {0x000000,"x",1},{0x00aa00,"y",1},{0x08a800,"xy",2},{0x114800,"xyz",3},
    {0x18a800,"xyyy",4},{0x19c800,"xyzw",4},{0x1fc800,"xyzw",4},
    {0x01fe00,"w",1},   /* live w170: DIV tmp.xyz, tmp.xyz, tmp.w */
    {0x015400,"z",1},   /* live w80: MUL tmp.x, tmp.z, tmp.w */
};
static const struct { unsigned k; char cls; const char* mask; } kR80Mask[] = {
    {0x2041000,'t',"x"},{0x2261000,'t',"xy"},{0x2261000,'t',"xy"},
    {0x2241000,'t',"x_"},{0x0221000,'t',"_x"},{0x2471000,'t',"xyz"},
    {0x2641000,'t',"x___"},{0x2671000,'t',"xyz_"},{0x2679000,'t',"xyzw"},
    {0x2609000,'t',"___x"},
    {0x267b000,'r',"xyzw"},{0x2673000,'r',"xyz_"},{0x260b000,'r',"___x"},
};
#define R80_NOPS   (int)(sizeof(kR80Ops)/sizeof(kR80Ops[0]))
#define R80_NSWZ   (int)(sizeof(kR80Swz)/sizeof(kR80Swz[0]))
#define R80_NMSK   (int)(sizeof(kR80Mask)/sizeof(kR80Mask[0]))
#define R80_CAP    32768
#define R80_NSLOTS 4
static struct {
    char glsl[R80_CAP];
    unsigned words, seq;
    char need, passthrough, shape2, shape3;
    int nprm;                 /* inline tail params decoded (rung 84) */
    float prm[16][4];
} g_r80[R80_NSLOTS];
static unsigned g_r80_seq;
static unsigned g_r81_prog;   /* rung 81: verified passthrough, live */
static int g_r82_exp[4] = {128, 128, 128, 255};  /* expected live pixel */
static int g_r82_shape2_live;                    /* TEX+MUL shape live */
static int g_r84_shape3_live;                    /* LRP/EX2 shape live */
static void r81_verdict_reset(void) { g_r84_verdict_arm = 1; }
static int g_r84_tol;                            /* per-shape tolerance */

/* render a source operand word into out[]; returns swizzle size 1-4 */
static int r80_src(unsigned long long w, char* out, size_t cap)
{
    unsigned low = (unsigned)(w & 0xFFFFFF);
    unsigned cls = (low >> 6) & 3;
    unsigned delta = low & ~0xC0u;
    int neg = (delta & 0x10) != 0;
    if (neg) delta &= ~0x10u;
    int s;
    for (s = 0; s < R80_NSWZ; s++)
        if (kR80Swz[s].d == delta) break;
    if (s == R80_NSWZ) return -1;
    unsigned idx = (unsigned)((w >> 48) & 0xFFFF);
    snprintf(out, cap, "%s%s%d.%s", neg ? "-" : "",
             cls == 0 ? "att" : cls == 1 ? "tmp" : "prm", idx,
             kR80Swz[s].sw);
    return kR80Swz[s].nc;
}
/* render a destination word: name ("tmpN"/"gl_FragColor") + component
 * string (position-based: x_ -> x, _x -> y, ___x -> w). 0 ok, -1 unk */
static int r80_dst(unsigned long long w, char* name, size_t ncap,
                   char* comp, size_t ccap)
{
    unsigned key = (unsigned)(w & 0xFFFFFFFF) & 0x0FFFFFFFu;
    int m;
    for (m = 0; m < R80_NMSK; m++)
        if (kR80Mask[m].k == key) break;
    if (m == R80_NMSK) return -1;
    unsigned idx = (unsigned)((w >> 48) & 0xFFFF);
    int c = 0;
    const char* mask = kR80Mask[m].mask;
    for (int i = 0; mask[i] && c < 7; i++)
        if (mask[i] != '_') comp[c++] = "xyzw"[i];
    comp[c] = 0;
    if (kR80Mask[m].cls == 'r')
        snprintf(name, ncap, "gl_FragColor");
    else
        snprintf(name, ncap, "tmp%u", idx);
    return 0;
}
/* record operand-name usage for the declaration list */
static void r80_note(const char* s, int* att, int* prm, int* tmp)
{
    if (s[0] == '-') s++;
    if (!strncmp(s, "att", 3)) { int t = atoi(s + 3); if (t < 256) att[t] = 1; }
    else if (!strncmp(s, "prm", 3)) { int t = atoi(s + 3); if (t < 256) prm[t] = 1; }
    else if (!strncmp(s, "tmp", 3)) { int t = atoi(s + 3); if (t < 256) tmp[t] = 1; }
}

static int g_r80_gate = -1;
static void rung80_translate(const unsigned long long* ws, unsigned words)
{
    if (g_r80_gate < 0) g_r80_gate = getenv("VMGLD_GPUTEST") ? 1 : 0;
    if (!g_r80_gate || !ws || words < 8 || words > 0x10000u) return;
    int marker = -1;
    for (unsigned i = 2; i < words; i++)
        if (ws[i] == 0x4c0ULL) marker = (int)i;   /* keep the LAST */
    if (marker < 0) { ep_log("rung80: no 0x4c0 marker; skip"); return; }
    int i = marker + 1;
    static char body[R80_CAP];
    int bo = 0;
    int att_seen[256] = {0}, prm_vec[256] = {0}, prm_smp[256] = {0},
        tmp_seen[256] = {0}, ncond = 0, ninstr = 0;
    char dname[32], dcomp[8], a[48], b[48], c[48], lhs[64];
    #define R80A(fmt, ...) do { \
        if (bo < (int)sizeof(body) - 256) bo += snprintf(body + bo, \
            sizeof(body) - bo, "    " fmt "\n", __VA_ARGS__); \
    } while (0)
    /* RUNG 81 — collect, then emit in INLINE ORDER. Subroutine
     * streams carry the label body FIRST (ending in a mid-stream
     * RET), main second; CAL (op + zero cond = 2 words, label ref
     * in the op word's high dword — the live w170 stream) splices
     * the label body inline at its call site. */
    static struct { unsigned opc; int wi; } is[280];
    int nis = 0, nret = 0, r1 = -1, r2 = -2, ncal = 0;
    while (i < (int)words - 3 && nis < 280) {
        if (ws[i] == 0 || ws[i] == 8) break;
        unsigned opc = (unsigned)((ws[i] & 0x3FFF) >> 6);
        int o;
        for (o = 0; o < R80_NOPS; o++) if (kR80Ops[o].op == opc) break;
        if (o == R80_NOPS) break;
        int n = kR80Ops[o].n;
        if (opc == 77) {                       /* RET: region boundary */
            is[nis].opc = 77; is[nis].wi = i; nis++;
            if (r1 < 0) r1 = nis - 1; else if (r2 < 0) r2 = nis - 1;
            nret++;
            i += 2;
            if (i < (int)words - 3 && ws[i] == 0) break;  /* final RET */
            continue;                                      /* label end */
        }
        if (opc == 76) ncal++;
        is[nis].opc = opc; is[nis].wi = i; nis++;
        /* CAL = op + zero cond = 2 words (its label ref rides the op
         * word's high dword, 0x4000 — unlike IF, which carries a
         * separate target word); IF/ENDIF keep their extra word */
        i += n + 1 + ((opc == 85 || opc == 89) ? 1 : 0);
    }
    if (nret > 2) {
        ep_log("rung80: >2 RETs (multi-label) — skipped");
        return;
    }
    if (ncal && nret < 2) {
        ep_log("rung80: CAL without a label region — skipped");
        return;
    }
    static int ord[560];
    int no = 0;
    if (nret == 2) {          /* label = is[0..r1), main = (r1..nis) */
        for (int k = r1 + 1; k < nis && no < 560; k++) {
            if (is[k].opc == 77) continue;
            if (is[k].opc == 76) {
                for (int m = 0; m < r1 && no < 560; m++) ord[no++] = m;
                continue;
            }
            ord[no++] = k;
        }
    } else {
        for (int k = 0; k < nis && no < 560; k++) {
            if (is[k].opc == 77 || is[k].opc == 76) continue;
            ord[no++] = k;
        }
    }
    /* RUNG 81 shape tracking: the passthrough (att -> tmp ->
     * gl_FragColor, two full MOVs, nothing else) is the semantic-
     * verification shader — its output is statically known. */
    int shape_ok = 1, shape2_ok = 1, shape3_ok = 1;
    for (int kk = 0; kk < no; kk++) {
        unsigned opc = is[ord[kk]].opc;
        int o;
        for (o = 0; o < R80_NOPS; o++) if (kR80Ops[o].op == opc) break;
        int n = kR80Ops[o].n;
        const unsigned long long* W = ws + is[ord[kk]].wi + 1;
        a[0] = b[0] = c[0] = 0;
        if (opc == 76) { ninstr++; continue; }   /* spliced above */
        if (opc == 85) {                              /* IF: cond is in
            the engine's bool table, not the stream (zero word) */
            shape_ok = 0;
            R80A("if (ppcond[%d] > 0.5) {", ncond); ncond++;
            ninstr++; continue;
        }
        if (opc == 89) {
            shape_ok = 0;
            if (bo < (int)sizeof(body) - 256)
                bo += snprintf(body + bo, sizeof(body) - bo, "    }\n");
            ninstr++; continue;
        }
        int wa = -1;
        if (r80_dst(W[0], dname, sizeof(dname), dcomp, sizeof(dcomp)) != 0
                || (wa = r80_src(W[1], a, sizeof(a))) < 0) goto bad;
        if (!strcmp(dcomp, "xyzw"))
            snprintf(lhs, sizeof(lhs), "%s", dname);
        else
            snprintf(lhs, sizeof(lhs), "%s.%s", dname, dcomp);
        if (dname[0] == 't') { int t = atoi(dname + 3); if (t < 256) tmp_seen[t] = 1; }
        r80_note(a, att_seen, prm_vec, tmp_seen);
        if (ninstr == 0) {
            if (opc != 0 || strncmp(a, "att", 3) != 0
                    || strcmp(dcomp, "xyzw")) shape_ok = 0;
        } else if (ninstr == 1) {
            if (opc != 0 || dname[0] != 'g') shape_ok = 0;
        } else shape_ok = 0;
        /* RUNG 82 shape-2: MOV att, MOV att, TEX, MUL->gl_FragColor */
        if (ninstr == 0) {
            if (opc != 0 || strncmp(a, "att", 3) != 0) shape2_ok = 0;
        } else if (ninstr == 1) {
            if (opc != 0 || strncmp(a, "att", 3) != 0
                    || strcmp(dcomp, "xyzw")) shape2_ok = 0;
        } else if (ninstr == 2) {
            if (opc != 66) shape2_ok = 0;
        } else if (ninstr == 3) {
            if (opc != 34 || dname[0] != 'g') shape2_ok = 0;
        } else shape2_ok = 0;
        /* RUNG 84 shape-3 (w80 peel): 10 instrs, first MOV att,
         * contains EX2, last is LRP to gl_FragColor */
        if (ninstr == 0) {
            if (opc != 0 || strncmp(a, "att", 3) != 0) shape3_ok = 0;
        } else if (ninstr == 8) {
            if (opc != 16) shape3_ok = 0;             /* EX2 */
        } else if (ninstr == 9) {
            if (opc != 60 || dname[0] != 'g') shape3_ok = 0;  /* LRP */
        } else if (ninstr >= 10) shape3_ok = 0;
        /* RUNG 80b: masked dst with wider rhs — PP semantics write the
         * first len(comp) components; GLSL needs an explicit selector */
        char rhs[96];
        /* wd = expression width (operand swizzle size, 4 for TEX,
         * 1 for scalar-producing ops): select only when WIDER than
         * the dst write set — the rung-80c lesson ((float).x). */
        #define R80SEL(expr) R80W(expr, wa)
        #define R80W(expr, wd) do { \
            if (!strcmp(dcomp, "xyzw") || (wd) <= (int)strlen(dcomp)) \
                snprintf(rhs, sizeof(rhs), "%s", expr); \
            else snprintf(rhs, sizeof(rhs), "(%s).%s", expr, dcomp); \
        } while (0)
        if (opc == 0) {                               /* MOV */
            R80SEL(a);
            R80A("%s = %s;", lhs, rhs);
        } else if (opc == 66) {                       /* TEX */
            if (r80_src(W[2], b, sizeof(b)) < 0 || b[0] == '-') goto bad;
            {   /* the sampler word is a prm ref WITH a .x swizzle —
                 * samplers cannot be swizzled; use the bare name */
                char* dot = strchr(b, '.'); if (dot) *dot = 0;
                r80_note(b, att_seen, prm_vec, tmp_seen);
                int t = atoi(b + 3); if (t < 256) prm_smp[t] = 1; prm_vec[t] = 0;
            }
            {
                char tex[96];
                snprintf(tex, sizeof(tex), "texture2D(%s, %s.xy)", b, a);
                R80W(tex, 4);
                R80A("%s = %s;", lhs, rhs);
            }
        } else if (n >= 3 && r80_src(W[2], b, sizeof(b)) < 0) goto bad;
        else {
            if (n >= 3) r80_note(b, att_seen, prm_vec, tmp_seen);
            if (opc == 31) { char rhsP[96]; snprintf(rhsP, sizeof(rhsP), "%s + %s", a, b); R80SEL(rhsP); R80A("%s = %s;", lhs, rhs); }
            else if (opc == 32) { char rhsM[96]; snprintf(rhsM, sizeof(rhsM), "%s - %s", a, b); R80SEL(rhsM); R80A("%s = %s;", lhs, rhs); }
            else if (opc == 34) { char rhsT[96]; snprintf(rhsT, sizeof(rhsT), "%s * %s", a, b); R80SEL(rhsT); R80A("%s = %s;", lhs, rhs); }
            else if (opc == 58) { char rhsD[96]; snprintf(rhsD, sizeof(rhsD), "%s / %s", a, b); R80SEL(rhsD); R80A("%s = %s;", lhs, rhs); }
            else if (opc == 45) { char rhsmax[96]; snprintf(rhsmax, sizeof(rhsmax), "max(%s, %s)", a, b); R80W(rhsmax, 1); R80A("%s = %s;", lhs, rhs); }
            else if (opc == 44) { char rhsmin[96]; snprintf(rhsmin, sizeof(rhsmin), "min(%s, %s)", a, b); R80W(rhsmin, 1); R80A("%s = %s;", lhs, rhs); }
            else if (opc == 56) { char rhspow[96]; snprintf(rhspow, sizeof(rhspow), "pow(%s, %s)", a, b); R80W(rhspow, 1); R80A("%s = %s;", lhs, rhs); }
            else if (opc == 39) { char rhsdot[96]; snprintf(rhsdot, sizeof(rhsdot), "dot(%s, %s)", a, b); R80W(rhsdot, 1); R80A("%s = %s;", lhs, rhs); }
            else if (opc == 16) { char rhsex2[96];
                  snprintf(rhsex2, sizeof(rhsex2), "exp2(%s)", a);
                  R80SEL(rhsex2); R80A("%s = %s;", lhs, rhs); }
            else if (opc == 21) { char rhsnormalize[96]; snprintf(rhsnormalize, sizeof(rhsnormalize), "normalize(%s)", a); R80SEL(rhsnormalize); R80A("%s = %s;", lhs, rhs); }
            else if (opc == 18) { char rhslength[96]; snprintf(rhslength, sizeof(rhslength), "length(%s)", a); R80W(rhslength, 1); R80A("%s = %s;", lhs, rhs); }
            else if (opc == 47) { char rhsreflect[96]; snprintf(rhsreflect, sizeof(rhsreflect), "reflect(%s, %s)", a, b); R80SEL(rhsreflect); R80A("%s = %s;", lhs, rhs); }
            else if (opc == 60) {                     /* LRP (ARB order unv.) */
                if (r80_src(W[3], c, sizeof(c)) < 0) goto bad;
                r80_note(c, att_seen, prm_vec, tmp_seen);
                { char rhsmix[96];
                  /* RUNG 84 CORRECTION: ARB LRP dst,a,b,c = a*b+(1-a)*c
                   * = GLSL mix(c, b, a) — the FIRST operand is the
                   * blend factor (Khronos ARB_fragment_program). The
                   * old mix(a,b,c) was equal only when b==(1,1,1,1). */
                  snprintf(rhsmix, sizeof(rhsmix), "mix(%s, %s, %s)", c, b, a);
                  R80SEL(rhsmix); R80A("%s = %s;", lhs, rhs); }
            }
            else if (opc == 50 || opc == 51 || opc == 53) {
                const char* x = opc == 50 ? ">=" : opc == 51 ? ">" : "<";
                { char rhscmp[96];
                  snprintf(rhscmp, sizeof(rhscmp), "(%s %s %s) ? 1.0 : 0.0", a, x, b);
                  R80W(rhscmp, 1); R80A("%s = %s;", lhs, rhs); }
            }
            else if (opc == 36)
                { char rhsanl[96];
                  snprintf(rhsanl, sizeof(rhsanl),
                           "(%s > 0.0 && %s > 0.0) ? 1.0 : 0.0", a, b);
                  R80W(rhsanl, 1); R80A("%s = %s;", lhs, rhs); }
            else goto bad;
        }
        ninstr++;
        continue;
    bad:
        {
            char bb[160];
            snprintf(bb, sizeof(bb),
                "rung80: translate STOP at word %d (op %u %s): "
                "unknown form", is[ord[kk]].wi, opc, kR80Ops[o].mn);
            ep_log(bb);
        }
        return;
    }
    /* RUNG 84: decode the inline PARAM tail (even-aligned block,
     * 2 words per param, (c2<<32|c1)(c4<<32|c3)) into the slot */
    {
        int j = i;
        while (j < (int)words && ws[j] == 0) j++;
        if (j < (int)words && (j % 2) == 0) {
            int np = 0;
            while (j + 1 < (int)words && np < 16) {
                unsigned long long w1 = ws[j], w2 = ws[j + 1];
                if (!w1 && !w2) break;
                unsigned int u;
                memcpy(&u, &w1, 4);        g_r80_slot_tmp[np][0] = bits_to_f(u);
                memcpy(&u, (char*)&w1 + 4, 4); g_r80_slot_tmp[np][1] = bits_to_f(u);
                memcpy(&u, &w2, 4);        g_r80_slot_tmp[np][2] = bits_to_f(u);
                memcpy(&u, (char*)&w2 + 4, 4); g_r80_slot_tmp[np][3] = bits_to_f(u);
                np++; j += 2;
            }
            g_r80_nprm_tmp = np;
        } else g_r80_nprm_tmp = 0;
    }
    /* compose: header (declarations) + body */
    int s = (int)(g_r80_seq++ % R80_NSLOTS);
    int ho = snprintf(g_r80[s].glsl, R80_CAP, "#version 110\n");
    for (int t = 0; t < 256; t++) {
        if (prm_smp[t] && !prm_vec[t])
            ho += snprintf(g_r80[s].glsl + ho, R80_CAP - ho,
                           "uniform sampler2D prm%d;\n", t);
        else if (prm_vec[t])
            ho += snprintf(g_r80[s].glsl + ho, R80_CAP - ho,
                           "uniform vec4 prm%d;\n", t);
    }
    for (int t = 0; t < 256; t++)
        if (att_seen[t])
            ho += snprintf(g_r80[s].glsl + ho, R80_CAP - ho,
                           "varying vec4 att%d;\n", t);
    if (ncond)
        ho += snprintf(g_r80[s].glsl + ho, R80_CAP - ho,
                       "uniform float ppcond[8];\n");
    ho += snprintf(g_r80[s].glsl + ho, R80_CAP - ho, "void main() {\n");
    for (int t = 0; t < 256; t++)
        if (tmp_seen[t])
            ho += snprintf(g_r80[s].glsl + ho, R80_CAP - ho,
                           "    vec4 tmp%d;\n", t);
    if (R80_CAP - ho > bo + 2) {
        memcpy(g_r80[s].glsl + ho, body, (size_t)bo);
        g_r80[s].glsl[ho + bo] = 0;
        strcat(g_r80[s].glsl, "}\n");
    } else {
        ep_log("rung80: GLSL too long; drop");
        return;
    }
    g_r80[s].words = words;
    g_r80[s].seq = g_r80_seq;
    g_r80[s].need = 1;
    g_r80[s].passthrough = (char)(shape_ok && ninstr == 2);
    g_r80[s].nprm = g_r80_nprm_tmp;
    memcpy(g_r80[s].prm, g_r80_slot_tmp, sizeof(g_r80[s].prm));
    g_r80[s].shape2 = (char)(shape2_ok && ninstr == 4);
    g_r80[s].shape3 = (char)(shape3_ok && ninstr == 10);
    {
        char bb[144];
        snprintf(bb, sizeof(bb),
            "rung80: translate OK seq=%u words=%u instrs=%d conds=%d "
            "regions=%d spliced=%d%s",
            g_r80_seq, words, ninstr, ncond, nret, ncal,
            g_r80[s].passthrough ? " PASSTHRU"
            : g_r80[s].shape2 ? " SHAPE2(TEX+MUL)" : "");
        ep_log(bb);
    }
    #undef R80A
}

/* compile every pending translated shader on the embedded context.
 * Called at the swap site AFTER the frame push — compile/link change
 * no GL state (the programs are never UseProgram'd). The synthetic VS
 * writes every varying the FS declares (the rung-79 link lesson). */
static void rung80_compile_pending(void)
{
    static int s_first_ok = 0;
    if (!os_glCreateShader || !os_glShaderSource || !os_glCompileShader
            || !os_glGetShaderiv || !os_glCreateProgram
            || !os_glAttachShader || !os_glLinkProgram
            || !os_glGetProgramiv) return;
    for (int s = 0; s < R80_NSLOTS; s++) {
        if (!g_r80[s].need) continue;
        g_r80[s].need = 0;
        const char* fsrc = g_r80[s].glsl;
        /* synthetic VS: re-declare the FS's varyings and write them */
        static char vsbuf[4096];
        int vo = 0, aa = 0;
        char assigns[1024]; int alen = 0;
        const char* p = fsrc;
        while (*p && (size_t)vo < sizeof(vsbuf) - 512) {
            const char* nl = strchr(p, '\n');
            size_t ll = nl ? (size_t)(nl - p) : strlen(p);
            if (strncmp(p, "varying ", 8) == 0 && ll < 100) {
                char name[48] = "";
                if (sscanf(p + 8, "%*s %47s", name) == 1 && name[0]) {
                    char* sc = strchr(name, ';'); if (sc) *sc = 0;
                    vo += snprintf(vsbuf + vo, sizeof(vsbuf) - vo,
                                   "%.*s\n", (int)ll, p);
                    alen += snprintf(assigns + alen, sizeof(assigns) - alen,
                                     " %s = vec4(0.25,0.75,0.5,1.0);", name);
                    aa = 1;
                }
            }
            if (!nl) break;
            p = nl + 1;
        }
        vo += snprintf(vsbuf + vo, sizeof(vsbuf) - vo,
                       "void main() { gl_Position = gl_Vertex;%s }",
                       aa ? assigns : "");
        unsigned vs = os_glCreateShader(0x8B31);
        unsigned fs = os_glCreateShader(0x8B30);
        const char* vsrc = vsbuf;
        os_glShaderSource(vs, 1, &vsrc, NULL); os_glCompileShader(vs);
        os_glShaderSource(fs, 1, &fsrc, NULL); os_glCompileShader(fs);
        int cv = 0, cf = 0;
        os_glGetShaderiv(vs, 0x8B81, &cv);
        os_glGetShaderiv(fs, 0x8B81, &cf);
        unsigned pr = 0; int cl = 0;
        if (cv && cf) {
            pr = os_glCreateProgram();
            os_glAttachShader(pr, vs); os_glAttachShader(pr, fs);
            os_glLinkProgram(pr);
            os_glGetProgramiv(pr, 0x8B82, &cl);
        }
        char bb[288];
        if (cv && cf && cl) {
            snprintf(bb, sizeof(bb),
                "rung80: COMPILE OK seq=%u words=%u prog=%u", g_r80[s].seq,
                g_r80[s].words, pr);
            if (!s_first_ok) {
                s_first_ok = 1;
                strcat(bb, " — FIRST LIVE TRANSLATE+COMPILE");
            }
            /* RUNG 81/82: the passthrough (expected 128,128,128,255)
             * then the TEX+MUL shape (texel x att) render live; each
             * output is statically known. */
            if (g_r80[s].passthrough && !g_r81_prog) {
                g_r81_prog = pr;
                /* varyings (0.25,0.75,0.5,1): 0.25*255=64, 0.75*255=191 */
                g_r82_exp[0] = 64; g_r82_exp[1] = 191;
                g_r82_exp[2] = 128; g_r82_exp[3] = 255;
                g_r84_tol = 0;
                r81_verdict_reset();
                char b2[96];
                snprintf(b2, sizeof(b2),
                    "rung81: passthrough prog=%u GOES LIVE", pr);
                ep_log(b2);
            }
            /* RUNG 82: 1x1 NEAREST texture (128,128,64,255) on unit 0;
             * every sampler uniform bound to it. Output of shape-2 =
             * texel * att0(0.5) = (64,64,32,255) — all products off
             * the .5 rounding boundary. NOTE: a 1x1 texture validates
             * sampler plumbing + MUL + varyings, NOT UV swizzles. */
            if (os_glGenTextures && os_glBindTexture
                    && os_glTexImage2D && os_glGetUniformLocation
                    && os_glUniform1i) {
                static unsigned s_tex = 0;
                if (!s_tex) {
                    unsigned char px[16] = { 64,   0,   0, 255,
                                            192,  64,   0, 255,
                                              0, 128,   0, 255,
                                              0,   0, 255, 255 };
                    os_glGenTextures(1, &s_tex);
                    os_glBindTexture(0x0DE1 /*GL_TEXTURE_2D*/, s_tex);
                    /* default min filter NEAREST_MIPMAP_LINEAR makes a
                     * level-less 2x2 INCOMPLETE (black) — the rung-84
                     * mism=1600 lesson; set both filters explicitly */
                    if (os_glTexParameteri) {
                        os_glTexParameteri(0x0DE1, 0x2801, 0x2600);
                        os_glTexParameteri(0x0DE1, 0x2800, 0x2600);
                    }
                    os_glTexImage2D(0x0DE1, 0, 0x1908, 2, 2, 0,
                                    0x1908, 0x1401, px);
                    char b2[96];
                    snprintf(b2, sizeof(b2),
                        "rung84: 2x2 NEAREST texture=%u bound unit 0",
                        s_tex);
                    ep_log(b2);
                }
                /* bind every declared sampler to unit 0 */
                const char* p2 = fsrc;
                int nsamp = 0;
                while ((p2 = strstr(p2, "uniform sampler2D ")) != 0) {
                    unsigned tn = 999;
                    if (sscanf(p2 + 18, "prm%u", &tn) == 1
                            && tn < 256) {
                        char nm[16];
                        snprintf(nm, sizeof(nm), "prm%u", tn);
                        int loc = os_glGetUniformLocation(pr, nm);
                        if (loc >= 0) {
                            os_glUniform1i(loc, 0);
                            nsamp++;
                        }
                    }
                    p2 += 18;
                }
                if (nsamp) {
                    char b2[96];
                    snprintf(b2, sizeof(b2),
                        "rung82: prog=%u samplers bound=%d", pr, nsamp);
                    ep_log(b2);
                }
            }
            /* RUNG 84: bind the decoded inline PARAM tail as
             * uniform vec4s. Mapping heuristic: if prm0 is a SAMPLER
             * (some TEX sampled it) the tail starts at prm1, else at
             * prm0 (corpus: s13/s15/w42 vs s7/w80 classes). */
            if (os_glGetUniformLocation && os_glUniform4fv
                    && g_r80[s].nprm > 0) {
                int off = strstr(fsrc, "uniform sampler2D prm0;")
                          ? 1 : 0;
                int bound = 0;
                for (int k = 0; k < g_r80[s].nprm; k++) {
                    char nm[16];
                    snprintf(nm, sizeof(nm), "prm%d", k + off);
                    int loc = os_glGetUniformLocation(pr, nm);
                    if (loc >= 0) {
                        os_glUniform4fv(loc, 1, g_r80[s].prm[k]);
                        bound++;
                    }
                }
                if (bound) {
                    char b2[96];
                    snprintf(b2, sizeof(b2),
                        "rung84: prm bind prog=%u n=%d/%d off=%d",
                        pr, bound, g_r80[s].nprm, off);
                    ep_log(b2);
                }
            }
            if (g_r80[s].shape3 && !g_r84_shape3_live) {
                g_r84_shape3_live = 1;
                g_r81_prog = pr;
                /* w80: att=(0.25,0.75,0.5,1); prms -2/(-,0.5,.8,.8)/(1)
                 * tmp6=exp2(-.125)=0.917; mix(prm1,prm2,tmp6)=
                 * (234,244,251,251) — tolerance +-2 (exp2 ULPs) */
                g_r82_exp[0] = 234; g_r82_exp[1] = 244;
                g_r82_exp[2] = 251; g_r82_exp[3] = 251;
                g_r84_tol = 2;
                r81_verdict_reset();
                char b2[128];
                snprintf(b2, sizeof(b2),
                    "rung84: LRP/EX2 prog=%u GOES LIVE "
                    "(expect 234,244,251,251 tol 2)", pr);
                ep_log(b2);
            }
            if (g_r80[s].shape2 && !g_r82_shape2_live) {
                g_r82_shape2_live = 1;
                g_r81_prog = pr;
                /* UV=(0.25,0.75) -> texel(0,1)=(0,128,0,255) x att0 */
                g_r82_exp[0] = 0; g_r82_exp[1] = 96;
                g_r82_exp[2] = 0; g_r82_exp[3] = 255;
                g_r84_tol = 0;
                r81_verdict_reset();
                char b2[112];
                snprintf(b2, sizeof(b2),
                    "rung82: TEX+MUL prog=%u GOES LIVE (expect "
                    "64,64,32,255)", pr);
                ep_log(b2);
            }
        } else {
            char log1[160] = ""; int len1 = 0;
            if (os_glGetShaderInfoLog)
                os_glGetShaderInfoLog(cf ? vs : fs, 159, &len1, log1);
            snprintf(bb, sizeof(bb),
                "rung80: COMPILE FAIL seq=%u words=%u vs=%d fs=%d "
                "link=%d | %.90s", g_r80[s].seq, g_r80[s].words, cv, cf,
                cl, log1);
        }
        ep_log(bb);
    }
}
EPR(gldGetPipelineProgramInfo)
/* the two instrumented entries: dump BEFORE the forward (the float
 * must still see the call; the instrument is passive) */
static long g_ec_gldCreatePipelineProgram;
long gldCreatePipelineProgram(void* a0, void* a1, void* a2, void* a3,
                              void* a4, void* a5)
{
    g_ec_gldCreatePipelineProgram++;
    pp_dump_desc(a2, "create");          /* rdx = descriptor */
    GLD_FWD("gldCreatePipelineProgram", a0,a1,a2,a3,a4,a5, -1);
    if (a1) *(void**)a1 = (void*)0;      /* rsi = out slot */
    ep_log("CALL gldCreatePipelineProgram -> -1 (refusal; out zeroed)");
    return -1;
}
static long g_ec_gldModifyPipelineProgram;
/* RUNG 73 — PER-DRAW STATE CAPTURE. Rung 66e proved draws
 * never cross as entry calls (steady frames = Modify:+2
 * only): the engine drives the raster through pointers
 * INSIDE the shared ctx object. Two instruments, both
 * rule-clean (memcpy-class reads; every allocation freed
 * before return):
 *
 * (a) pp_draw_state(ctx) — the read-only per-poll snapshot
 *     of the draw contract: +0x188 transform fn (CLASSIFIED
 *     against the float's own thunk/interpreter addresses,
 *     resolved by slide for identification only — never
 *     called), +0x190 current func, +0x198 token, +0x778
 *     bound prog, raster block @ +0x2e0, texture occupancy
 *     @ +0x780[32], uniform cache @ +0x538.
 * (b) pp_transform_hook — best-effort wrap of ctx+0x188;
 *     logs the first 4 calls' six args per poll window plus
 *     a running count, then passes through to the previous
 *     slot value. Bypass window: the float's
 *     glvmRequestFunctionPointerWrite swaps the slot to the
 *     JIT after its build, which happens AFTER our poll —
 *     those calls are counted as bypass at the next poll.
 *     Acceptable for capture; noted in the ledger. */
#include <mach-o/dyld.h>
long pp_transform_hook(void*, void*, void*, void*, void*, void*);
static int pp_r73_sane(void*);
static void* pp_r73_floatbase(void)
{
    /* the float image's base, for ADDRESS IDENTIFICATION only */
    static void* base = (void*)1;
    if (base == (void*)1) {
        base = 0;
        uint32_t n = _dyld_image_count();
        for (uint32_t i = 0; i < n; i++) {
            const char* nm = _dyld_get_image_name(i);
            if (nm && strstr(nm, "GLRendererFloat")) {
                base = (void*)_dyld_get_image_header(i);
                break;
            }
        }
    }
    return base;
}
static const char* pp_r73_classify(void* fn)
{
    void* b = pp_r73_floatbase();
    if (!b || !fn) return "?";
    if (fn == (char*)b + 0x1e00) return "SET-thunk";
    if (fn == (char*)b + 0x1b80) return "interp";
    if (fn == (char*)b + 0x1860) return "fallback";
    if (fn == pp_transform_hook)  return "OUR-hook";
    return "other/JIT";
}
static long (*g_r73_prev)(void*,void*,void*,void*,void*,void*) = 0;
static long g_r73_calls, g_r73_logged, g_r73_bypass;
long pp_transform_hook(void* c0, void* a1, void* a2, void* a3,
                       void* a4, void* a5)
{
    g_r73_calls++;
    if (g_r73_logged < 4) {
        g_r73_logged++;
        char b[256];
        snprintf(b, sizeof(b),
            "rung73 DRAW#%lld ctx=%p esi=%ld edx=%ld rcx=%p r8=%p r9d=%u",
            (long long)g_r73_calls, c0, (long)a1, (long)a2, a3, a4,
            (unsigned)(uintptr_t)a5);
        ep_log(b);
        /* RUNG 74b: the draw-time body deref was REMOVED with the
         * poll-path chained derefs (same crash class — reads through
         * pointers whose lifetime the stub doesn't control). The
         * pointer VALUES alone are logged with the draw line. */
        if (g_r73_logged == 1 && pp_r73_sane(c0)) {
            void** uc = (void**)((char*)c0 + 0x538);
            snprintf(b, sizeof(b), "rung74[draw] u:[%p %p %p %p]",
                     uc[0], uc[1], uc[2], uc[3]);
            ep_log(b);
        }
    }
    /* RUNG 86 SECOND HALF: evaluate the token's descriptors HERE —
     * the transform call's stack args exist only in this frame. The
     * C-ABI stack slots match glvmPreloadFPTransformFour's reads
     * (rbp+0x18 = 8th arg = block base rdi; +0x30 = 11th = ecx;
     * +0x38 = 12th = r8d). Gate VMGLD_R86; first 50 calls only. */
    {
        static int s_gate86 = -1;
        static int s_n86 = 0;
        static int s_dumped86 = 0;
        if (s_gate86 < 0)
            s_gate86 = getenv("VMGLD_R86") ? 1 : 0;
        {
        struct timeval tv0;
        gettimeofday(&tv0, 0);
        long long now0 = (long long)tv0.tv_sec * 1000
                       + tv0.tv_usec / 1000;
        static long long s_dump_last = -100000;
        int time_dump = s_dumped86 < 10 && now0 - s_dump_last >= 2000;
        if (time_dump) s_dump_last = now0;
        if (s_gate86 && (s_n86 < 50 || time_dump) && pp_r73_sane(a4)) {
            s_n86++;
            /* JIT-hypothesis ABI (rung-86b): token at ctx+0x198; the
             * block pair rides rcx/r8 (a3/a4, 0x150 apart); scale args
             * unknown — try r9d (a5) as ecx, 0 as r8 */
            char* fp = (char*)__builtin_frame_address(0);
            unsigned long long rdi_b = *(unsigned long long*)(fp + 0x18);
            unsigned ecx_a = *(unsigned*)(fp + 0x30);
            unsigned r8_a  = *(unsigned*)(fp + 0x38);
            void* tok_j = pp_r73_sane(c0)
                ? *(void**)((char*)c0 + 0x198) : 0;
            unsigned long long rdi_j =
                (unsigned long long)(uintptr_t)a3;
            unsigned ecx_j = (unsigned)(uintptr_t)a5;
            unsigned r8_j = 0;
            if (s_n86 <= 8 && pp_r73_sane(tok_j))
                ep_log("rung86[jit-hyp] active: tok_j set, rdi=a3");
            unsigned long long* tw = (unsigned long long*)a4;
            unsigned long long flags = tw[1];
            unsigned n = *(unsigned*)((char*)a4 + 0x7c);
            int jit_mode = 0;
            unsigned long long d0 = 0;
            /* token-stability: same token must yield same flags */
            static unsigned long long s_last_tok, s_last_flags;
            static int s_have_last;
            int unstable = s_have_last
                && (unsigned long long)(uintptr_t)a4 == s_last_tok
                && flags != s_last_flags;
            s_last_tok = (unsigned long long)(uintptr_t)a4;
            s_last_flags = flags;
            s_have_last = 1;
            if (flags == 0 || n > 0x100 || unstable
                    || !(rdi_b > 0x100000000ull
                         && rdi_b < 0x10000000000ull)) {
                /* interpreter-ABI decode implausible — JIT mode */
                jit_mode = 1;
                if (!pp_r73_sane(tok_j)) goto skip86;
                tw = (unsigned long long*)tok_j;
                flags = tw[1];
                n = *(unsigned*)((char*)tok_j + 0x7c);
                rdi_b = rdi_j;
                ecx_a = ecx_j;
                /* RUNG 86d: the scale arg derived FROM the k0
                 * descriptor (block-size constraint: r8p must be 0,
                 * so r8_a = (d>>48) - 1) — replaces the assumed 0 */
                d0 = (n + 1 <= 0x100) ? tw[n + 1] : 0;
                r8_a = (unsigned)(((d0 >> 48) & 0xFFFF) - 1);
                r8_j = r8_a;
                if (n > 0x100) goto skip86;
            }
            if (n <= 0x100) {
                char b[420];
                int o = snprintf(b, sizeof(b),
                    "rung86[h%d]%s: tok=%p rdi=%llx ecx=%u "
                    "r8=%u(d0hi=%llx) flags=%llx n=%u",
                    s_n86, jit_mode ? "J" : "",
                    jit_mode ? tok_j : a4, rdi_b, ecx_a, r8_a,
                    (d0 >> 48) & 0xFFFF, flags, n);
                for (int k = 0; k < 7; k++) {
                    if (!(flags & (0x4000ULL << k))) continue;
                    unsigned long long d = tw[n + 1 + k];
                    unsigned sel = ((unsigned)(d & 0xFF)) >> 3;
                    unsigned cnt = (unsigned)((d >> 32) & 0xFFFF);
                    unsigned typ = (unsigned)((d >> 16) & 0xF);
                    long r8p = (d & 0x100000ULL)
                        ? (long)r8_a
                        : (long)((d >> 48) & 0xFFFF) - (long)r8_a - 1;
                    long off = r8p * (long)cnt + (long)ecx_a;
                    if (typ == 1 || typ == 2) off <<= 4;
                    else if (typ == 3) off <<= 2;
                    else if (typ == 4) off <<= 1;
                    else off = 0;
                    unsigned long long addr =
                        rdi_b + (unsigned long long)(sel - 10) * 8
                        + (unsigned long long)off;
                    int onA10 = pp_r73_sane(c0)
                        && addr == (unsigned long long)((char*)c0 + 0xe00 + 80);
                    int inBlk = addr >= rdi_b
                        && addr < rdi_b + 0x150;
                    if (o < (int)sizeof(b) - 90)
                        o += snprintf(b + o, sizeof(b) - o,
                            " | k%d sel=%u t=%u c=%u addr=%llx%s%s",
                            k, sel, typ, cnt, addr,
                            onA10 ? " **A10**" : "",
                            inBlk ? " inBLK" : "");
                }
                if (s_n86 <= 50) ep_log(b);
                /* RUNG 86c: dump on the WALL-CLOCK trigger (the h-cap
                 * no longer gates this) */
                if (jit_mode && time_dump) {
                    s_dumped86++;
                    unsigned long long k0a = 0;
                    if (flags & 0x4000ULL) {
                        unsigned long long d0 = tw[n + 1];
                        unsigned sel0 = ((unsigned)(d0 & 0xFF)) >> 3;
                        unsigned cnt0 = (unsigned)((d0 >> 32) & 0xFFFF);
                        unsigned typ0 = (unsigned)((d0 >> 16) & 0xF);
                        long r8p0 = (d0 & 0x100000ULL)
                            ? 0
                            : (long)((d0 >> 48) & 0xFFFF) - (long)r8_a - 1;
                        long off0 = r8p0 * (long)cnt0 + (long)ecx_a;
                        if (typ0 == 1 || typ0 == 2) off0 <<= 4;
                        else if (typ0 == 3) off0 <<= 2;
                        else if (typ0 == 4) off0 <<= 1;
                        else off0 = 0;
                        k0a = rdi_b
                            + (unsigned long long)(sel0 - 10) * 8
                            + (unsigned long long)off0;
                    }
                    if (k0a > 0x10100
                            && k0a < 0x10000000000ull) {
                        /* wide: nonzero words only, +-%#x window */
                        unsigned long long* W =
                            (unsigned long long*)(k0a - 0x100);
                        char d[460];
                        int p = snprintf(d, sizeof(d),
                            "rung86c[%d t=%lld] k0=%llx nz:",
                            s_dumped86, now0, k0a);
                        int shown = 0;
                        for (int i = 0; i < 66 && shown < 12
                                && p < 420; i++) {
                            unsigned long long w = W[i];
                            if (!w) continue;
                            float lo, hi;
                            memcpy(&lo, &w, 4);
                            memcpy(&hi, (char*)&w + 4, 4);
                            p += snprintf(d + p, sizeof(d) - p,
                                " %+d:%llx(%.4g,%.4g)",
                                (i - 32) * 8, w, lo, hi);
                            shown++;
                        }
                        if (!shown)
                            p += snprintf(d + p, sizeof(d) - p,
                                " (all-zero +-%#x)", 0x100);
                        ep_log(d);
                    }
                }
            }
        }
    }
    }
    skip86:;
    if (g_r73_prev) return g_r73_prev(c0, a1, a2, a3, a4, a5);
    return 0;
}
static int pp_r73_sane(void* p)
{
    return p && (uintptr_t)p > 0x1000 && (uintptr_t)p < 0x800000000000ull;
}
static void pp_draw_state(void* ctx, const char* tagsite)
{
    if (!pp_r73_sane(ctx)) return;
    void** slot188 = (void**)((char*)ctx + 0x188);
    void* cur = *slot188;
    /* (b) wrap — SWAP SITE ONLY. The modify-site sub-context is a
     * different object class; its +0x188 was 0 and writing the hook
     * there (rung 74b first attempt) preceded a BUS error — the
     * field may not be a function slot in that object at all.
     * Rung 73's three clean scenes wrapped both sites: luck. */
    if (cur != (void*)pp_transform_hook && !strcmp(tagsite, "swap")) {
        g_r73_prev = cur;
        *slot188 = (void*)pp_transform_hook;
    }
    /* RUNG 74b STABILITY FIX: a modify poll fired during glClear's
     * deferred-state update at scene RESET and pp_draw_state crashed
     * the process (pp_draw_state+533, chained deref through a ctx
     * mid-mutation — 13:52:47 crash report). ALL chained derefs are
     * removed from the poll path; only DIRECT ctx-field reads and
     * the pointer VALUES remain. The uniform-value location moves
     * to the consumer-side decode (glvmPreloadFPTransformFour) —
     * static, zero guest risk. */
    char b[320];
    int o = snprintf(b, sizeof(b),
        "rung73[%s] POLL ctx=%p: 188=%s(%p) 190=%p 198=%p",
        tagsite, ctx,
        pp_r73_classify(cur), cur,
        *(void**)((char*)ctx + 0x190),
        *(void**)((char*)ctx + 0x198));
    void* prog = *(void**)((char*)ctx + 0x778);
    o += snprintf(b + o, sizeof(b) - o, " prog=%p", prog);
    /* uniform-cache POINTER VALUES only (no deref): */
    void** uc = (void**)((char*)ctx + 0x538);
    o += snprintf(b + o, sizeof(b) - o, " u:[%p %p %p %p]",
        uc[0], uc[1], uc[2], uc[3]);
    /* texture occupancy: nonzero entries in ctx+0x780[32] */
    int tex = 0;
    void** ta = (void**)((char*)ctx + 0x780);
    for (int i = 0; i < 32; i++) if (pp_r73_sane(ta[i])) tex++;
    o += snprintf(b + o, sizeof(b) - o, " tex=%d/32", tex);
    /* raster block head @ +0x2e0 (direct ctx fields) */
    unsigned long* rb = (unsigned long*)((char*)ctx + 0x2e0);
    o += snprintf(b + o, sizeof(b) - o, " rb:");
    for (int i = 0; i < 4 && o < 300; i++)
        o += snprintf(b + o, sizeof(b) - o, " %llx", rb[i]);
    o += snprintf(b + o, sizeof(b) - o,
        " calls=%lld(logged %lld)", (long long)g_r73_calls,
        (long long)g_r73_logged);
    ep_log(b);
    /* reset the per-poll window; total kept in calls */
    g_r73_logged = 0;
}
long gldModifyPipelineProgram(void* a0, void* a1, void* a2, void* a3,
                              void* a4, void* a5)
{
    g_ec_gldModifyPipelineProgram++;
    r83_probe("modify", a0, a1, a2, a3);  /* rung 83: ctx state at draw
                                           * density — the setup window */
    pp_draw_state(a0, "modify");          /* per-poll draw-state + wrap */
    if (a1 && a1 != (void*)0x123) {      /* rsi = GLD program obj */
        void* desc = *(void**)a1;        /* prog->+0x00 */
        if (desc) pp_dump_desc(desc, "modify");
    }
    GLD_FWD("gldModifyPipelineProgram", a0,a1,a2,a3,a4,a5, -1);
    return 0;                            /* the float returns 0 always */
}
EPR(gldUnbindPipelineProgram)
EPR(gldDestroyPipelineProgram)
EPR(gldCreateProgram)
EPR(gldDestroyProgram)
EPR(gldCreateVertexArray)
EPR(gldModifyVertexArray)
EPR(gldFlushVertexArray)
EPR(gldUnbindVertexArray)
EPR(gldReclaimVertexArray)
EPR(gldDestroyVertexArray)
EPR(gldCreateFence)
EPR(gldDestroyFence)
EPR(gldCreateQuery)
EPR(gldGetQueryInfo)
EPR(gldDestroyQuery)
EPR(gldObjectPurgeable)
EPR(gldObjectUnpurgeable)
EPR(gldCreateComputeContext)
EPR(gldDestroyComputeContext)
EPR(gldLoadHostBuffer)
EPR(gldSyncBufferObject)
EPR(gldGenerateTexMipmaps)
EPR(gldCopyTexSubImage)
EPR(gldModifyTexSubImage)
EPR(gldBufferSubData)
EPR(gldModifyQuery)
EPR(gldDeleteTextureLevel)
EPR(gldDeleteTexture)
EPR(gldAllocVertexBuffer)
EPR(gldCompleteVertexBuffer)
EPR(gldFreeVertexBuffer)
EPR(gldGetMemoryPluginData)
EPR(gldSetMemoryPluginData)
EPR(gldFinishMemoryPluginData)
EPR(gldTestMemoryPluginData)
EPR(gldDestroyMemoryPluginData)

/* RUNG 66e — the export census report (defined after the last
 * instantiation so every g_ec_ counter is in scope). Nonzero deltas
 * only, chunked lines, reset on report. */
static void export_census_report(long swapno)
{
    char b[640];
    int o = snprintf(b, sizeof(b), "rung66e s%ld:", swapno);
#define ECR(n) do { \
    if (g_ec_##n) { \
        int w = snprintf(b + o, sizeof(b) - o, " %s:+%ld", \
                         #n + 3, g_ec_##n); \
        if (w > 0) o += w; \
        g_ec_##n = 0; \
        if (o > (int)sizeof(b) - 90) { ep_log(b); \
            o = snprintf(b, sizeof(b), "rung66e:"); } \
    } } while (0)
    ECR(gldAllocVertexBuffer); ECR(gldBufferSubData);
    ECR(gldCompleteVertexBuffer); ECR(gldCopyTexSubImage);
    ECR(gldCreateBuffer); ECR(gldCreateComputeContext);
    ECR(gldCreateFence); ECR(gldCreateFramebuffer);
    ECR(gldCreatePipelineProgram); ECR(gldCreateProgram);
    ECR(gldCreateQuery); ECR(gldCreateTexture);
    ECR(gldCreateTextureLevel); ECR(gldCreateVertexArray);
    ECR(gldDeleteTexture); ECR(gldDeleteTextureLevel);
    ECR(gldDestroyBuffer); ECR(gldDestroyComputeContext);
    ECR(gldDestroyFence); ECR(gldDestroyFramebuffer);
    ECR(gldDestroyMemoryPlugin); ECR(gldDestroyMemoryPluginData);
    ECR(gldDestroyPipelineProgram); ECR(gldDestroyProgram);
    ECR(gldDestroyQuery); ECR(gldDestroyTexture);
    ECR(gldDestroyTextureLevel); ECR(gldDestroyVertexArray);
    ECR(gldDiscardFramebuffer); ECR(gldFinish);
    ECR(gldFinishMemoryPluginData); ECR(gldFinishObject);
    ECR(gldFlush); ECR(gldFlushBuffer);
    ECR(gldFlushMemoryPlugin); ECR(gldFlushObject);
    ECR(gldFlushVertexArray); ECR(gldFreeVertexBuffer);
    ECR(gldGenerateTexMipmaps); ECR(gldGetError);
    ECR(gldGetInteger); ECR(gldGetMemoryPlugin);
    ECR(gldGetMemoryPluginData); ECR(gldGetPipelineProgramInfo);
    ECR(gldGetQueryInfo); ECR(gldGetTextureLevel);
    ECR(gldGetTextureLevelImage); ECR(gldGetTextureLevelInfo);
    ECR(gldIsTextureResident); ECR(gldLoadBuffer);
    ECR(gldLoadHostBuffer); ECR(gldLoadTexture);
    ECR(gldModifyPipelineProgram); ECR(gldModifyQuery);
    ECR(gldModifyTexSubImage); ECR(gldModifyTexture);
    ECR(gldModifyTextureLevel); ECR(gldModifyVertexArray);
    ECR(gldObjectPurgeable); ECR(gldObjectUnpurgeable);
    ECR(gldPageoffBuffer); ECR(gldReclaimBuffer);
    ECR(gldReclaimContext); ECR(gldReclaimFramebuffer);
    ECR(gldReclaimTexture); ECR(gldReclaimVertexArray);
    ECR(gldSetInteger); ECR(gldSetMemoryPlugin);
    ECR(gldSetMemoryPluginData); ECR(gldSyncBufferObject);
    ECR(gldSyncTexture); ECR(gldTestMemoryPlugin);
    ECR(gldTestMemoryPluginData); ECR(gldTestObject);
    ECR(gldUnbindBuffer); ECR(gldUnbindFramebuffer);
    ECR(gldUnbindPipelineProgram); ECR(gldUnbindTexture);
    ECR(gldUnbindVertexArray); ECR(gldWaitObject);
#undef ECR
    if (o > 14) ep_log(b);
}

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
#define EPR(n) long n(void* a0, void* a1, void* a2, void* a3, void* a4, void* a5) { \
    if (a0) *(void**)a0 = (void*)0; \
    ep_log("CALL " #n " -> -1 (refusal; out zeroed)"); return -1; }
#define EPB(n) long n(void* a0, void* a1, void* a2, void* a3, void* a4, void* a5) { \
    if (a0) *(void**)a0 = (void*)0; \
    ep_log("CALL " #n " -> false (out zeroed)"); return 0; }
#define EPV(n) void n(void* a0, void* a1, void* a2, void* a3, void* a4, void* a5) { \
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

/* RUNG 39 — the saved window triple + the lazy bounds query */
static unsigned g_sid_cid = 0, g_sid_wid = 0, g_sid_sid = 0;
static int g_bounds_locked = 0;
static int g_fb_w = 4, g_fb_h = 4;      /* rung 39: window-sized when attached */
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
    (void)dctx; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    if (++g_swap_count > 5 && (g_swap_count % 500) != 0) return 0;
    virgl_read_app_clear_color(dctx);   /* RUNG 49: the app's color */
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
    (void)dctx; (void)block2; (void)a3; (void)a4; (void)a5;
    char b[96];
    snprintf(b, sizeof(b),
             "rung46: install entry +0x50 CALLED (dctx=%p block1=%p)",
             dctx, block1);
    ep_log(b);
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
        fprintf(f, "[%s pid=%d]   glvmPreInit(0x%x) -> %d (rc propagated; "
                 "guard=mask!=0 -> %d)\n",
                ts, (int)getpid(), arg5 & 1, rc, g_vm_ok);
        fclose(f);
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
        case 12: case 13:                     /* RUNG 51: Depth/Stencil
                                                 sizes — value attrs */
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
    obj[7] = 0x1;          /* +0x1c */
    obj[8] = 0x1;          /* +0x20 */
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
static void gld_noop(void* a0, void* a1, void* a2, void* a3,
                     void* a4, void* a5)
{
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    if (++g_noop_count <= 10 || (g_noop_count % 1000) == 0) {
        char buf[64];
        snprintf(buf, sizeof(buf), "NOOP dispatch #%ld", g_noop_count);
        ep_log(buf);
    }
}
long gldInitDispatch(void* ctx, unsigned long* dispatch,
                     unsigned* limits, void* a3, void* a4, void* a5)
{
    (void)ctx; (void)a3; (void)a4; (void)a5;
    char buf[96];
    snprintf(buf, sizeof(buf),
             "CALL gldInitDispatch ctx=%p dispatch=%p limits=%p (rung 34)",
             ctx, (void*)dispatch, (void*)limits);
    ep_log(buf);
    if (!dispatch) {
        ep_log("  gldInitDispatch -> -1 (no dispatch block)");
        return -1;
    }
    /* Every offset the float writes (grf.t 0x14da0-0x14f73): */
    static const unsigned kSlots[] = {
        0x00, 0x08, 0x10, 0x18, 0x20, 0x28, 0x30, 0x38,
        0x40, 0x48, 0x80, 0x88, 0x90, 0x98, 0xa0, 0xb8,
        0xc0, 0xc8, 0xd0, 0xf0, 0xf8, 0x100
    };
    for (unsigned i = 0; i < sizeof(kSlots)/sizeof(kSlots[0]); i++)
        *(void**)((char*)dispatch + kSlots[i]) = (void*)&gld_noop;
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
    case 0x1F02: s = "0.0 stub";                           break; /* GL_VERSION  */
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
EPR(gldCreateTexture)
EPR(gldIsTextureResident)
EPR(gldModifyTexture)
EPR(gldLoadTexture)
EPV(gldUnbindTexture)
EPR(gldReclaimTexture)
EPV(gldDestroyTexture)
EPR(gldCreateTextureLevel)
EPR(gldGetTextureLevelInfo)
EPR(gldGetTextureLevelImage)
EPR(gldModifyTextureLevel)
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
EPR(gldCreateFramebuffer)
EPR(gldUnbindFramebuffer)
EPR(gldReclaimFramebuffer)
EPR(gldDestroyFramebuffer)
EPR(gldCreatePipelineProgram)
EPR(gldGetPipelineProgramInfo)
EPR(gldModifyPipelineProgram)
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
EPR(gldSyncTexture)
EPR(gldGenerateTexMipmaps)
EPR(gldCopyTexSubImage)
EPR(gldModifyTexSubImage)
EPR(gldBufferSubData)
EPR(gldModifyQuery)
EPR(gldDiscardFramebuffer)
EPR(gldGetTextureLevel)
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

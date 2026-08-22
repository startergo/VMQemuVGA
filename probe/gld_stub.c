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
static int g_vm_ok = 0;    /* rung 6b: mask-store guard — the working GLD's actual structure */
static int g_vm_mask = 0;  /* mirror of the real gld_io_data mask store */

/* RUNG 29: the shared object's processor block stand-in — writable,
 * zeroed (the float uses its own glg_processor_default_data). */
static unsigned long g_proc_stand_in[64];

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
    r[3] = 0x6CD;        /* +0xc class field (software, per float) */
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
            p++; walked += 4; break;          /* value attrs: consumed */
        case 47: case 48: case 72: case 54: break;  /* no-op pass */
        case 49: flags |= 4; si = 1; break;
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
EPR(gldCreateContext)
EPR(gldReclaimContext)
EPR(gldDestroyContext)
EPR(gldAttachDrawable)
EPR(gldInitDispatch)
EPR(gldUpdateDispatch)
EPR(gldGetString)
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

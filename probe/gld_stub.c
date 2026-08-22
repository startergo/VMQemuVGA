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

#define EPR(n) long n(void) { ep_log("CALL " #n " -> -1 (GLDReturn refusal)"); return -1; }
#define EPB(n) long n(void) { ep_log("CALL " #n " -> false"); return 0; }
#define EPV(n) void n(void) { ep_log("CALL " #n " (void)"); }

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
static int g_vm_ok = 0;    /* rung 6b: mask-store guard — the working GLD's actual structure */
static int g_vm_mask = 0;  /* mirror of the real gld_io_data mask store */

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

EPR(gldGetRendererInfo)
EPR(gldChoosePixelFormat)
EPR(gldDestroyPixelFormat)
EPR(gldCreateShared)
EPR(gldDestroyShared)
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

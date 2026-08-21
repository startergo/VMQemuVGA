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

/* RUNG 4 (pre-registered): the handshake entries — declared in the
 * trampoline header with real signatures but ABSENT from the 92-name
 * table; hypothesized separately-dlsym'd by the loader as the GLD
 * init/teardown connection. gldInitializeLibrary LOGS ITS ARGUMENTS
 * (psvc dereferenced when non-NULL — the first kernel-side datum) and
 * returns nothing (void: no success claim possible). */
void gldInitializeLibrary(int* psvc, void* arg1, int GLDisplayMask,
                          void* arg3, void* arg4)
{
    FILE *f = fopen("/tmp/vm_gld_stub.log", "a");
    if (f) {
        time_t t = time(NULL);
        char ts[32];
        strftime(ts, sizeof(ts), "%H:%M:%S", localtime(&t));
        fprintf(f, "[%s pid=%d] CALL gldInitializeLibrary "
                 "psvc=%p%s arg1=%p GLDisplayMask=0x%x arg3=%p arg4=%p\n",
                 ts, (int)getpid(), (void*)psvc,
                 psvc ? " (*psvc below)" : " (NULL)",
                 arg1, GLDisplayMask, arg3, arg4);
        if (psvc)
            fprintf(f, "[%s pid=%d]   *psvc = %d (0x%x)\n",
                    ts, (int)getpid(), *psvc, *psvc);
        fclose(f);
    }
}

void gldTerminateLibrary(void)
{
    ep_log("CALL gldTerminateLibrary (void)");
}

/* ==== generated from VMsvga2 EntryPointNames.c + header return types ==== */
/* RUNG 5 PHASE 2 (pre-registered): gldGetVersion answers TRUE with the
 * values OBSERVED by disassembling the working GLD (GLRendererFloat,
 * gldGetVersion @0x18d05): (3, 1, &_mh_bundle_header, 0x400). The only
 * entry that stops refusing — every other entry keeps its refusal. */
#include <mach-o/loader.h>
extern struct mach_header_64 _mh_bundle_header;
long gldGetVersion(int* a0, int* a1, int* a2, int* a3)
{
    ep_log("CALL gldGetVersion -> TRUE (observed values 3,1,&hdr,0x400)");
    if (a0) *a0 = 3;
    if (a1) *a1 = 1;
    if (a2) *a2 = (int)(unsigned long)&_mh_bundle_header;
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

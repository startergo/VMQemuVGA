/*
 * gld_stub.c — Phase B1 of the pre-registered stub-GLD first rung
 * (LEDGER 2026-08-21 evening). Logging stub, no rendering, no
 * capability claims. Placed at the loader path OBSERVED in Phase A:
 *   /System/Library/Frameworks/OpenGL.framework/Versions/A/Resources/
 *   VMVirtIOGLEngine.bundle/VMVirtIOGLEngine
 * (flat layout — the trace showed dlopen of GLEngine.bundle/GLEngine
 * directly, no Contents/MacOS; name source = the accelerator node's
 * IOGLBundleName="VMVirtIOGLEngine", still live per the FB comment).
 *
 * Every export appends "CALL <name>" to /tmp/vm_gld_stub.log and
 * returns 0 — the REQUIRED instrumentation separating outcome 2
 * (NEVER LOADED) from outcome 3 (LOADED, NOT ENUMERATED) by evidence.
 * x86-64 SysV: caller-managed args make void-bodied, long-returning
 * stubs safe for unknown signatures (args untouched, RAX=0).
 *
 * Entry-point names generated verbatim from
 * ../../VMsvga2-modern/GLD/EntryPointNames.c (Zenith432, MIT).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
    ep_log("STUB LOADED (constructor)");
}

#define EP(n) long n(void) { ep_log("CALL " #n); return 0; }

/* ==== generated from VMsvga2 EntryPointNames.c ==== */
EP(gldGetVersion)
EP(gldGetRendererInfo)
EP(gldChoosePixelFormat)
EP(gldDestroyPixelFormat)
EP(gldCreateShared)
EP(gldDestroyShared)
EP(gldCreateContext)
EP(gldReclaimContext)
EP(gldDestroyContext)
EP(gldAttachDrawable)
EP(gldInitDispatch)
EP(gldUpdateDispatch)
EP(gldGetString)
EP(gldGetError)
EP(gldSetInteger)
EP(gldGetInteger)
EP(gldFlush)
EP(gldFinish)
EP(gldTestObject)
EP(gldFlushObject)
EP(gldFinishObject)
EP(gldWaitObject)
EP(gldCreateTexture)
EP(gldIsTextureResident)
EP(gldModifyTexture)
EP(gldLoadTexture)
EP(gldUnbindTexture)
EP(gldReclaimTexture)
EP(gldDestroyTexture)
EP(gldCreateTextureLevel)
EP(gldGetTextureLevelInfo)
EP(gldGetTextureLevelImage)
EP(gldModifyTextureLevel)
EP(gldDestroyTextureLevel)
EP(gldCreateBuffer)
EP(gldLoadBuffer)
EP(gldFlushBuffer)
EP(gldPageoffBuffer)
EP(gldUnbindBuffer)
EP(gldReclaimBuffer)
EP(gldDestroyBuffer)
EP(gldGetMemoryPlugin)
EP(gldSetMemoryPlugin)
EP(gldTestMemoryPlugin)
EP(gldFlushMemoryPlugin)
EP(gldDestroyMemoryPlugin)
EP(gldCreateFramebuffer)
EP(gldUnbindFramebuffer)
EP(gldReclaimFramebuffer)
EP(gldDestroyFramebuffer)
EP(gldCreatePipelineProgram)
EP(gldGetPipelineProgramInfo)
EP(gldModifyPipelineProgram)
EP(gldUnbindPipelineProgram)
EP(gldDestroyPipelineProgram)
EP(gldCreateProgram)
EP(gldDestroyProgram)
EP(gldCreateVertexArray)
EP(gldModifyVertexArray)
EP(gldFlushVertexArray)
EP(gldUnbindVertexArray)
EP(gldReclaimVertexArray)
EP(gldDestroyVertexArray)
EP(gldCreateFence)
EP(gldDestroyFence)
EP(gldCreateQuery)
EP(gldGetQueryInfo)
EP(gldDestroyQuery)
EP(gldObjectPurgeable)
EP(gldObjectUnpurgeable)
EP(gldCreateComputeContext)
EP(gldDestroyComputeContext)
EP(gldLoadHostBuffer)
EP(gldSyncBufferObject)
EP(gldSyncTexture)
EP(gldGenerateTexMipmaps)
EP(gldCopyTexSubImage)
EP(gldModifyTexSubImage)
EP(gldBufferSubData)
EP(gldModifyQuery)
EP(gldDiscardFramebuffer)
EP(gldGetTextureLevel)
EP(gldDeleteTextureLevel)
EP(gldDeleteTexture)
EP(gldAllocVertexBuffer)
EP(gldCompleteVertexBuffer)
EP(gldFreeVertexBuffer)
EP(gldGetMemoryPluginData)
EP(gldSetMemoryPluginData)
EP(gldFinishMemoryPluginData)
EP(gldTestMemoryPluginData)
EP(gldDestroyMemoryPluginData)

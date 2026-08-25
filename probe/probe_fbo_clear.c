/*
 * probe_fbo_clear.c — RUNG 83: minimal reproducer for the
 * gldClearDrawBuffer size-disagreement crash (rung 82b disassembly:
 * clear walks ctx draw rect over ctx+0x360[i] image with no bounds
 * knowledge; the glmark desktop scene crashed once in ~10 suites).
 *
 * Each variation targets one way the rect and the allocation can
 * disagree. Run in a shell loop; the last printed marker before a
 * SIGSEGV names the reproducing variation.
 *
 *   V1 control: tex 800x600, viewport 800x600, clear
 *   V2 viewport LARGER than texture (stale window rect on small FBO)
 *   V3 control: tex and viewport both 512
 *   V4 realloc texture to a different size WHILE ATTACHED, then clear
 *   V5 two FBOs, different-size textures, ALTERNATE binds + clear
 *   V6 attach level 1 (smaller mip), viewport of level 0, clear
 *
 * Build (guest): cc -o fbo_clear probe_fbo_clear.c \
 *                 -framework OpenGL -framework ApplicationServices
 * Usage: fbo_clear [cycles]
 */
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl.h>
#include <OpenGL/glext.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static CGLContextObj g_ctx;   /* ONE context, never destroyed */
static CGLContextObj mk_ctx(int w, int h)
{
    if (g_ctx) { CGLSetCurrentContext(g_ctx); return g_ctx; }
    static const CGLPixelFormatAttribute combos[][6] = {
        { kCGLPFAOffScreen, kCGLPFAColorSize, 32, 0 },
        { kCGLPFAAllRenderers, kCGLPFAOffScreen, kCGLPFAColorSize, 32, 0 },
        { kCGLPFAOffScreen, kCGLPFAColorSize, 32, kCGLPFAAlphaSize, 8, 0 },
        { kCGLPFAColorSize, 32, 0 },
    };
    CGLPixelFormatObj pf = NULL;
    GLint npix = 0;
    CGLError e = kCGLNoError;
    int ci;
    for (ci = 0; ci < 4; ci++) {
        e = CGLChoosePixelFormat((CGLPixelFormatAttribute*)combos[ci],
                                 &pf, &npix);
        if (e == kCGLNoError && pf && npix > 0) break;
        pf = NULL;
    }
    if (!pf) { printf("pf: no working combo (last err %d)\n", e); exit(1); }
    CGLContextObj ctx = NULL;
    e = CGLCreateContext(pf, NULL, &ctx);
    if (e != kCGLNoError || !ctx) { printf("ctx err %d\n", e); exit(1); }
    CGLDestroyPixelFormat(pf);
    CGLSetCurrentContext(ctx);
    static unsigned char* buf;
    static int bw, bh;
    if (!buf || bw != w || bh != h) {
        free(buf);
        buf = malloc((size_t)w * h * 4);
        bw = w; bh = h;
    }
    CGLSetOffScreen(ctx, w, h, w * 4, buf);
    return ctx;
}

static GLuint mk_fbo_tex(int w, int h, int level, int alloc)
{
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    if (alloc)
        glTexImage2D(GL_TEXTURE_2D, level, GL_RGBA, w, h, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    return tex;
}

static GLuint attach(GLuint tex)
{
    GLuint fbo = 0;
    glGenFramebuffersEXT(1, &fbo);
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbo);
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT,
                              GL_COLOR_ATTACHMENT0_EXT,
                              GL_TEXTURE_2D, tex, 0);
    return fbo;
}

static void mark(const char* fmt, int a, int b)
{
    printf(fmt, a, b);
    fflush(stdout);
}

int main(int argc, char** argv)
{
    int cycles = argc > 1 ? atoi(argv[1]) : 50;
    printf("glmark-fbo-repro start (V1..V6 x %d)\n", cycles);
    fflush(stdout);
    int c;
    for (c = 0; c < cycles; c++) {
        mark("cycle %d V1", c, 0);
        {   /* V1: control */
            CGLContextObj ctx = mk_ctx(800, 600);
            GLuint tex = mk_fbo_tex(800, 600, 0, 1);
            GLuint fbo = attach(tex);
            glViewport(0, 0, 800, 600);
            glClearColor(0, 0, 0, 1);
            glClear(GL_COLOR_BUFFER_BIT);
            glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
            glDeleteFramebuffersEXT(1, &fbo);
            glDeleteTextures(1, &tex);
        }
        mark("cycle %d V2", c, 0);
        {   /* V2: viewport larger than the texture */
            CGLContextObj ctx = mk_ctx(800, 600);
            GLuint tex = mk_fbo_tex(512, 512, 0, 1);
            GLuint fbo = attach(tex);
            glViewport(0, 0, 800, 600);      /* rect 800x600 on 512x512 */
            glClearColor(0, 0, 0, 1);
            glClear(GL_COLOR_BUFFER_BIT);
            glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
            glDeleteFramebuffersEXT(1, &fbo);
            glDeleteTextures(1, &tex);
        }
        mark("cycle %d V3", c, 0);
        {   /* V3: control, matching sizes */
            CGLContextObj ctx = mk_ctx(512, 512);
            GLuint tex = mk_fbo_tex(512, 512, 0, 1);
            GLuint fbo = attach(tex);
            glViewport(0, 0, 512, 512);
            glClearColor(0, 0, 0, 1);
            glClear(GL_COLOR_BUFFER_BIT);
            glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
            glDeleteFramebuffersEXT(1, &fbo);
            glDeleteTextures(1, &tex);
        }
        mark("cycle %d V4", c, 0);
        {   /* V4: realloc while attached, then clear (old rect? old image?) */
            CGLContextObj ctx = mk_ctx(800, 600);
            GLuint tex = mk_fbo_tex(800, 600, 0, 1);
            GLuint fbo = attach(tex);
            glViewport(0, 0, 800, 600);
            glBindTexture(GL_TEXTURE_2D, tex);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0,
                         GL_RGBA, GL_UNSIGNED_BYTE, NULL);  /* shrink */
            glClear(GL_COLOR_BUFFER_BIT);
            glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
            glDeleteFramebuffersEXT(1, &fbo);
            glDeleteTextures(1, &tex);
        }
        mark("cycle %d V5", c, 0);
        {   /* V5: two FBOs, alternate binds + clear */
            CGLContextObj ctx = mk_ctx(800, 600);
            GLuint ta = mk_fbo_tex(800, 600, 0, 1);
            GLuint tb = mk_fbo_tex(256, 256, 0, 1);
            GLuint fa = attach(ta);
            GLuint fb = attach(tb);
            int k;
            for (k = 0; k < 8; k++) {
                glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fa);
                glViewport(0, 0, 800, 600);
                glClear(GL_COLOR_BUFFER_BIT);
                glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fb);
                glViewport(0, 0, 256, 256);
                glClear(GL_COLOR_BUFFER_BIT);
            }
            glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
            glDeleteFramebuffersEXT(1, &fa);
            glDeleteFramebuffersEXT(1, &fb);
            glDeleteTextures(1, &ta);
            glDeleteTextures(1, &tb);
        }
        mark("cycle %d V6", c, 0);
        {   /* V6: attach a SMALLER mip level, viewport of level 0 */
            CGLContextObj ctx = mk_ctx(800, 600);
            GLuint tex = 0;
            glGenTextures(1, &tex);
            glBindTexture(GL_TEXTURE_2D, tex);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                            GL_NEAREST_MIPMAP_NEAREST);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 512, 512, 0,
                         GL_RGBA, GL_UNSIGNED_BYTE, NULL);
            glTexImage2D(GL_TEXTURE_2D, 1, GL_RGBA, 256, 256, 0,
                         GL_RGBA, GL_UNSIGNED_BYTE, NULL);
            GLuint fbo = 0;
            glGenFramebuffersEXT(1, &fbo);
            glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbo);
            glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT,
                                      GL_COLOR_ATTACHMENT0_EXT,
                                      GL_TEXTURE_2D, tex, 1); /* level 1 */
            glViewport(0, 0, 512, 512);      /* level-0 viewport */
            glClear(GL_COLOR_BUFFER_BIT);
            glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
            glDeleteFramebuffersEXT(1, &fbo);
            glDeleteTextures(1, &tex);
        }
        mark("cycle %d END\n", c, 0);
    }
    printf("fbo_clear COMPLETED CLEAN\n");
    return 0;
}

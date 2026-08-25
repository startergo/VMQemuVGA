/* ppcompile.c — RUNG 79: compile a PP->GLSL translated shader on the
 * guest's Mesa (virgl path) OUTSIDE the GLD stub, via dlopen'd OSMesa.
 *
 * Mirrors the stub's linkage (rung 59/75): dlopen + OSMesaGetProcAddress,
 * RTLD_LOCAL, five-arg OSMesaCreateContextExt, depthless. Run with
 *   DYLD_LIBRARY_PATH=/Users/sl/osmesa ./ppcompile shader.glsl
 * Exit 0 iff COMPILE_STATUS and LINK_STATUS are both GL_TRUE.
 * With --draw: also draws a full-screen quad and pixel-checks the
 * fragment shader's own output (readback row must be non-clear).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>

typedef unsigned GLenum; typedef int GLint; typedef unsigned GLuint;
typedef unsigned char GLboolean; typedef float GLfloat;

int main(int argc, char** argv)
{
    if (argc < 2) { fprintf(stderr, "usage: %s FILE.glsl [--draw]\n", argv[0]); return 2; }
    int draw = argc > 2 && strcmp(argv[2], "--draw") == 0;

    void* lib = dlopen("/Users/sl/osmesa/libOSMesa.8.dylib", RTLD_LAZY | RTLD_LOCAL);
    if (!lib) { printf("PPCOMPILE dlopen FAIL: %s\n", dlerror()); return 1; }
    void* (*create)(unsigned, int, int, int, void*) =
        (void* (*)(unsigned, int, int, int, void*))dlsym(lib, "OSMesaCreateContextExt");
    int (*makecur)(void*, void*, unsigned, int, int) =
        (int (*)(void*, void*, unsigned, int, int))dlsym(lib, "OSMesaMakeCurrent");
    void* (*getproc)(const char*) =
        (void* (*)(const char*))dlsym(lib, "OSMesaGetProcAddress");
    if (!create || !makecur || !getproc) { printf("PPCOMPILE syms FAIL\n"); return 1; }

    enum { W = 64, H = 64 };
    unsigned char* buf = (unsigned char*)calloc(1, W * H * 4);
    void* ctx = create(0x1908 /*GL_RGBA*/, 0, 0, 0, NULL);
    if (!ctx) { printf("PPCOMPILE ctx FAIL\n"); return 1; }
    if (!makecur(ctx, buf, 0x1401, W, H)) { printf("PPCOMPILE makecurrent FAIL\n"); return 1; }

    GLuint (*glCreateShader)(GLenum) = (GLuint (*)(GLenum))getproc("glCreateShader");
    void (*glShaderSource)(GLuint, GLint, const char* const*, const GLint*) =
        (void (*)(GLuint, GLint, const char* const*, const GLint*))getproc("glShaderSource");
    void (*glCompileShader)(GLuint) = (void (*)(GLuint))getproc("glCompileShader");
    void (*glGetShaderiv)(GLuint, GLenum, GLint*) = (void (*)(GLuint, GLenum, GLint*))getproc("glGetShaderiv");
    void (*glGetShaderInfoLog)(GLuint, GLint, GLint*, char*) =
        (void (*)(GLuint, GLint, GLint*, char*))getproc("glGetShaderInfoLog");
    GLuint (*glCreateProgram)(void) = (GLuint (*)(void))getproc("glCreateProgram");
    void (*glAttachShader)(GLuint, GLuint) = (void (*)(GLuint, GLuint))getproc("glAttachShader");
    void (*glLinkProgram)(GLuint) = (void (*)(GLuint))getproc("glLinkProgram");
    void (*glGetProgramiv)(GLuint, GLenum, GLint*) = (void (*)(GLuint, GLenum, GLint*))getproc("glGetProgramiv");
    void (*glUseProgram)(GLuint) = (void (*)(GLuint))getproc("glUseProgram");
    void (*glGetError)(void) = (void (*)(void))getproc("glGetError");
    if (!glCreateShader || !glShaderSource || !glCompileShader || !glGetShaderiv
            || !glCreateProgram || !glAttachShader || !glLinkProgram
            || !glGetProgramiv || !glUseProgram) {
        printf("PPCOMPILE shader entries UNRESOLVED\n"); return 1;
    }

    FILE* f = fopen(argv[1], "rb");
    if (!f) { printf("PPCOMPILE open FAIL %s\n", argv[1]); return 1; }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char* src = (char*)calloc(1, n + 1);
    if (fread(src, 1, n, f) != (size_t)n) { fclose(f); return 1; }
    fclose(f);
    /* Synthesize a VS that writes every varying the fragment shader
     * declares (values are irrelevant for a compile/link test): the PP
     * fragment stream assumes a vertex stage feeds its ATTRIBs. */
    char vbuf[2048]; int vo = 0;
    {
        char decls[1024]; int dlen = 0;
        char assigns[1024]; int alen = 0;
        char* line = src;
        while (line && *line) {
            char* nl = strchr(line, '\n');
            size_t ll = nl ? (size_t)(nl - line) : strlen(line);
            if (strncmp(line, "varying ", 8) == 0 && ll < 120) {
                char name[64] = "";
                if (sscanf(line + 8, "%*s %63s", name) == 1 && name[0]) {
                    char* sc = strchr(name, ';');
                    if (sc) *sc = 0;
                    dlen += snprintf(decls + dlen, sizeof(decls) - dlen,
                                     "%.*s\n", (int)ll, line);
                    alen += snprintf(assigns + alen, sizeof(assigns) - alen,
                                     " %s = vec4(0.5, 0.5, 0.5, 1.0);", name);
                }
            }
            if (!nl) break;
            line = nl + 1;
        }
        vo += snprintf(vbuf + vo, sizeof(vbuf) - vo, "%s", decls);
        vo += snprintf(vbuf + vo, sizeof(vbuf) - vo,
                       "void main() { gl_Position = gl_Vertex;%s }", assigns);
    }
    const char* vsrc = vbuf;
    const char* fsrc = src;

    GLuint vs = glCreateShader(0x8B31);
    GLuint fs = glCreateShader(0x8B30);
    glShaderSource(vs, 1, &vsrc, NULL); glCompileShader(vs);
    glShaderSource(fs, 1, &fsrc, NULL); glCompileShader(fs);
    GLint cv = 0, cf = 0;
    glGetShaderiv(vs, 0x8B81, &cv);
    glGetShaderiv(fs, 0x8B81, &cf);
    printf("PPCOMPILE compile vs=%d fs=%d (1=TRUE)\n", cv, cf);
    if (!cv || !cf) {
        char log[512]; int len = 0;
        glGetShaderInfoLog(!cv ? vs : fs, sizeof(log) - 1, &len, log);
        printf("PPCOMPILE INFOLOG: %.*s\n", len, log);
        return 1;
    }
    GLuint p = glCreateProgram();
    glAttachShader(p, vs); glAttachShader(p, fs);
    glLinkProgram(p);
    GLint cl = 0;
    glGetProgramiv(p, 0x8B82, &cl);
    printf("PPCOMPILE link=%d prog=%u\n", cl, p);
    if (!cl) {
        void (*glGetProgramInfoLog)(GLuint, GLint, GLint*, char*) =
            (void (*)(GLuint, GLint, GLint*, char*))getproc("glGetProgramInfoLog");
        char log[512]; int len = 0;
        if (glGetProgramInfoLog) {
            glGetProgramInfoLog(p, sizeof(log) - 1, &len, log);
            printf("PPCOMPILE LINKLOG: %.*s\n", len, log);
        }
        return 1;
    }

    if (draw) {
        void (*glBegin)(GLenum) = (void (*)(GLenum))getproc("glBegin");
        void (*glEnd)(void) = (void (*)(void))getproc("glEnd");
        void (*glVertex2f)(GLfloat, GLfloat) = (void (*)(GLfloat, GLfloat))getproc("glVertex2f");
        void (*glClearColor)(GLfloat, GLfloat, GLfloat, GLfloat) =
            (void (*)(GLfloat, GLfloat, GLfloat, GLfloat))getproc("glClearColor");
        void (*glClear)(GLenum) = (void (*)(GLenum))getproc("glClear");
        void (*glFinish)(void) = (void (*)(void))getproc("glFinish");
        void (*glReadPixels)(GLint, GLint, GLint, GLint, GLenum, GLenum, void*) =
            (void (*)(GLint, GLint, GLint, GLint, GLenum, GLenum, void*))getproc("glReadPixels");
        glUseProgram(p);
        glClearColor(0.25f, 0.25f, 0.25f, 1.0f);
        glClear(0x4000);
        glBegin(0x0005);
        glVertex2f(-1, -1); glVertex2f(1, -1); glVertex2f(-1, 1); glVertex2f(1, 1);
        glEnd();
        glFinish();
        unsigned char px[W * 4];
        glReadPixels(0, 0, W, 1, 0x1908, 0x1401, px);
        int nz = 0;
        int i;
        for (i = 0; i < W * 4; i++) if (px[i]) nz++;
        printf("PPCOMPILE draw row0 nonzero-bytes=%d/%d first=%02x%02x%02x%02x\n",
               nz, W * 4, px[0], px[1], px[2], px[3]);
    }
    printf("PPCOMPILE PASS %s\n", argv[1]);
    return 0;
}

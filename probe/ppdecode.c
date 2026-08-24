/* ppdecode — RUNG 72 offline PP-stream decoder (host-side).
 *
 * Per .claude/rules/instrumentation.md: streams are captured as raw
 * words by the GLD stub (/tmp/ppdump/p*.bin) and decoded HERE, never
 * in-process on the guest. This tool loads the host copy of the
 * guest's libGLProgrammability (md5 a0185546b98c1a020bb9474391155c75
 * — RE-VERIFY against the guest before trusting output; see the
 * rule) and calls the non-exported disassembler at file offset
 * 0xd5ba5 of the x86_64 slice (recorded at rung 71; __TEXT vmaddr 0,
 * so runtime address = image header + offset).
 *
 * Build (x86_64; runs under Rosetta on arm64 hosts):
 *   xcrun clang -arch x86_64 probe/ppdecode.c -o /tmp/ppdecode \
 *       -framework Carbon 2>/dev/null || \
 *   xcrun clang -arch x86_64 probe/ppdecode.c -o /tmp/ppdecode
 * Usage: ppdecode FILE [FILE...]   (each = one captured stream)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <stdint.h>
#include <mach-o/dyld.h>

#define GLP_PATH_DEF \
    "OpenGL.framework.snowleopard/Libraries/libGLProgrammability.dylib"
#define GLP_DISASM_OFF 0xd5ba5ul   /* _glpPPDisassemble, x86_64 slice */

static char* (*glp_disasm)(void*) = 0;

static void resolve(const char* dylib)
{
    void* h = dlopen(dylib, RTLD_NOW | RTLD_LOCAL);
    if (!h) {
        fprintf(stderr, "ppdecode: dlopen(%s) FAILED: %s\n", dylib,
                dlerror());
        exit(1);
    }
    /* force any lazy setup, then find the image base */
    dlsym(h, "PPStreamTokenFree");
    uint32_t n = _dyld_image_count();
    for (uint32_t i = 0; i < n; i++) {
        const char* nm = _dyld_get_image_name(i);
        if (nm && strstr(nm, "libGLProgrammability")) {
            glp_disasm = (char* (*)(void*))
                ((char*)_dyld_get_image_header(i) + GLP_DISASM_OFF);
            fprintf(stderr, "ppdecode: %s at %p, disasm %p\n", nm,
                    (void*)_dyld_get_image_header(i), (void*)glp_disasm);
            return;
        }
    }
    fprintf(stderr, "ppdecode: image not found after dlopen\n");
    exit(1);
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s FILE [FILE...]\n", argv[0]);
        return 2;
    }
    const char* dylib = getenv("PPDECODE_GLP");
    if (!dylib || !*dylib) {
        /* default order: the guest's own system glp (the native decode
         * habitat — every dependency present at the right vintage),
         * else the repo copy (host decode; see the rung-72 ledger note
         * for why host decode fails on modern macOS: absolute install
         * names + missing 10.6 symbols in the modern dependency chain) */
        static const char* sys =
            "/System/Library/Frameworks/OpenGL.framework/Libraries/"
            "libGLProgrammability.dylib";
        FILE* probe = fopen(sys, "rb");
        if (probe) { fclose(probe); dylib = sys; }
        else dylib = GLP_PATH_DEF;
    }
    resolve(dylib);

    for (int a = 1; a < argc; a++) {
        FILE* f = fopen(argv[a], "rb");
        if (!f) { fprintf(stderr, "ppdecode: open %s FAILED\n", argv[a]);
                  continue; }
        fseek(f, 0, SEEK_END);
        long fsz = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (fsz < 0x18) {
            fprintf(stderr, "ppdecode: %s too small (%ld)\n", argv[a], fsz);
            fclose(f);
            continue;
        }
        /* allocate with slack: if the header claims more words than
         * the file holds, the disassembler still has a bounded walk */
        unsigned words = 0;
        { unsigned char hdr[0x18];
          if (fread(hdr, 1, 0x18, f) != 0x18) { fclose(f); continue; }
          words = *(unsigned*)(hdr + 0x10); }
        size_t want = 8ull * words;
        size_t alloc = (want > (size_t)fsz ? want : (size_t)fsz) + 64;
        unsigned char* buf = (unsigned char*)malloc(alloc);
        memset(buf, 0, alloc);
        fseek(f, 0, SEEK_SET);
        size_t got = fread(buf, 1, (size_t)fsz, f);
        fclose(f);
        (void)got;
        unsigned type = *(unsigned short*)buf;
        unsigned refc = *(unsigned short*)(buf + 2);
        printf("=== %s: type=0x%04x refc=%u words=%u file=%ldB%s\n",
               argv[a], type, refc, words, fsz,
               want > (size_t)fsz ? " (HEADER EXCEEDS FILE — padded)" : "");
        char* txt = glp_disasm(buf);
        if (!txt) { printf("    (disasm returned NULL)\n\n"); continue; }
        fputs(txt, stdout);
        fputs("\n\n", stdout);
        free(txt);
        free(buf);
    }
    return 0;
}

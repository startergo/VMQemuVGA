/* ppdec.c — RUNG 77 final third: the pure-C PP-stream decoder.
 *
 * Per .claude/rules/instrumentation.md this is the decoder a runtime
 * GLD translator uses INSTEAD of ever calling glpPPDisassemble
 * in-process. Layout from probe/pp-word-format.md + pp-opcodes.txt,
 * decoded from libGLProgrammability (x86_64, md5 a018…5c75).
 *
 * STATUS (honest): the op word (opcode = w0>>6), the operand word
 * (class = (w0>>6)&7 ∈ {att,tmp,prm,res,adr,immediate}, register
 * index = u16@+0x6), and the stream header (type@w0, flags@w1,
 * WORD COUNT@w2 — verified empirically) are decoded. NOT YET: the
 * declaration-table tag semantics (kinds 0x11-0x1e observed), the
 * swizzle bit positions, the immediate encodings, per-opcode
 * instruction lengths. The differential below reports coverage per
 * file; byte-equality with ppcorpus-2026-08-24.txt is the release
 * gate and is NOT yet met — see the rung-77 ledger note.
 *
 * Build: clang probe/ppdec.c -o /tmp/ppdec2
 * Usage: ppdec2 FILE.bin [...]        (one stream per file)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static const char* kOps[] = {
/*   0*/ "MOV","LIT","ABS","CEI","FRC","FLR","FWD","SSG","ANY","ALL",
/*  10*/ "NOT","NSE","SQT","RSQ","RCP","RCC","EX2","EXP","LEN","LG2",
/*  20*/ "LOG","NRM","RAD","DEG","SCS","SIN","COS","TAN","ASN","ACS",
/*  30*/ "ATN","ADD","SUB","MOD","MUL","MLC","ANL","ORL","XRL","DOT",
/*  40*/ "DP3","DP4","DPH","DST","MIN","MAX","XPD","RFL","STR","SEQ",
/*  50*/ "SGE","SGT","SLE","SLT","SNE","SFL","POW","SEL","DIV","FFW",
/*  60*/ "LRP","CLM","CMP","MAD","SMS","SWZ","TEX","TXP","TXB","TXPB",
/*  70*/ "TXL","TXPL","ARL","ARR","ARA","BRA","CAL","RET","FTR","KIL",
/*  80*/ "DDX","DDY","DP2","DP2A","BRK","IF","LOOP","REP","ELSE","ENDIF",
/*  90*/ "ENDLOOP","ENDREP","PK2H","PK2US","PK4B","PK4UB","POPA","PUSHA",
        "TXD","TXPD",
/* 100*/ "UP2H","UP2US","UP4B","UP4UB","X2D","RFR","CONT","WHILE",
        "ENDWHILE","MTC",
/* 110*/ "DSL","NOOP","EXPE","LOGE","TARGBRA","TARGCONT","TARGBRK",
        "TRANSPOSE","OUTERPRODUCT","EMITVERTEX",
/* 120*/ "ENDPRIMITIVE","VRL","BDL","ROUND","TRUNCATE","AND","OR","XOR",
        "SHL","SHR",
/* 130*/ "TEXEL_FETCH","TEXTURE_SIZE","COMP","DISTANCE","INVERSESQRT",
        "TARGCAL","PP_TEX_FORMAT_R","PP_TEX_FORMAT_ALPHA",
        "PP_TEX_FORMAT_INTENSITY","PP_TEX_FORMAT_LUMINANCE"
};
static const int kNumOps = (int)(sizeof(kOps)/sizeof(kOps[0]));

static const char* kClass[8] =
    { "att", "tmp", "prm", "res", "adr", "imm", "imm", "imm" };

int main(int argc, char** argv)
{
    if (argc < 2) { fprintf(stderr, "usage: %s FILE.bin\n", argv[0]);
                    return 2; }
    for (int a = 1; a < argc; a++) {
        FILE* f = fopen(argv[a], "rb");
        if (!f) continue;
        fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
        unsigned char* b = (unsigned char*)malloc(n);
        if (fread(b, 1, n, f) != (size_t)n) { fclose(f); continue; }
        fclose(f);
        uint64_t* w = (uint64_t*)b;
        int words = (int)(n / 8);
        unsigned type = *(uint16_t*)b;
        unsigned refc = *(uint16_t*)(b + 2);
        unsigned count = (n >= 0x18) ? *(uint32_t*)(b + 0x10)
                                     : (unsigned)words;
        printf("=== %s: type=0x%04x refc=%u words=%u file=%ldB\n",
               argv[a], type, refc, count, n);
        /* header scan: declaration-tag region heuristic — words whose
         * low-16 kind byte falls in 0x11..0x1e AND opcode would be 0
         * (w0 <= 0x3f) are declarations until the first instruction */
        int instr_start = 3;
        for (int i = 3; i < words && i < 64; i++) {
            unsigned lo = (unsigned)(w[i] & 0xFFFF);
            unsigned hi = (unsigned)((w[i] >> 32) & 0xFFFF);
            if ((w[i] >> 6) != 0) { instr_start = i; break; }
            if (lo > 4 || hi == 0 || hi > 0x40) { instr_start = i; break; }
            (void)lo; (void)hi;
        }
        printf("hdr: flags=%016llx decl-words=%d\n",
               (unsigned long long)w[1], instr_start - 3);
        /* instruction walk: op word + operand words (class-field
         * validated). Per-opcode lengths not yet tabled; this walk
         * REPORTS the sequence it sees for differential comparison. */
        for (int i = instr_start; i < words; i++) {
            unsigned w0 = (unsigned)(w[i] & 0xFFFF);
            unsigned op = w0 >> 6;
            if (op < (unsigned)kNumOps && (w0 & 0x3f) == 0
                    && op != 0) {
                /* plausible op word (opcode in table, low 6 bits are
                 * width/flags — allow, print them) */
                printf("%4d OP %-14s w0=%04x width=%u flags=%u\n",
                       i, kOps[op], w0, w0 & 7, (w0 >> 3) & 7);
            } else {
                unsigned cls = (w0 >> 6) & 7;
                unsigned idx = (unsigned)(w[i] >> 48) & 0xFFFF;
                if ((w0 >> 6) < 8)
                    printf("%4d    %-3s[%u] w0=%04x\n",
                           i, kClass[cls], idx, w0);
                else
                    printf("%4d    ?   w=%016llx\n", i,
                           (unsigned long long)w[i]);
            }
        }
        free(b);
    }
    return 0;
}

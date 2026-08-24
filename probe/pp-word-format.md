# The PP-stream word format

Decoded from `libGLProgrammability` (x86_64 slice, md5
`a0185546b98c1a020bb9474391155c75`): statically from
`glpDisassemble1Op` (0xd3dfb) and its operand formatter (~0xd2c00),
empirically verified against the rung-72 corpus by the Rosetta walk.
See `pp-opcodes.txt` for the 140-entry mnemonic table.

## The op word (first word of each instruction)

```
+0x00 u16:
  bits  0-2   width/type (the ":4F/:3F/:1F/:1I" annotation, 3 bits)
  bits  6-15  OPCODE  =  w0 >> 6   (indexes ppstreamOpString[])
+0x04 u32:
  bits 14-31  field (unnamed yet — flags/operand count class)
```

Instructions are MULTI-WORD: the op word plus operand words; the
stream interleaves declaration/binding tables (regular data runs —
e.g. the w362 streams' `?1 ?3 ?3 ?3` pattern at word 49 — not code).
Per-opcode lengths live in glpDisassemble1Op's 140-way jump table.

## The operand word (8 bytes each)

```
+0x00 u16:
  bits  0-2   width/type (same annotation field as the op word)
  bits  3-5   flags (sign/saturation modifiers; per-component
              negatives "-x..-w" formatted at 0xd3308+)
  bits  6-8   REGISTER CLASS  =  (w0 >> 6) & 7:
                0 = att   (attribute;   handler 0xd2d91)
                1 = tmp   (temporary;   handler 0xd2ca4)
                2 = prm   (parameter;   handler 0xd2dbf)
                3 = res   (result;      handler 0xd2e96)
                4 = adr   (address reg; handler 0xd2ec0)
              5-7 = immediate/constant path (0xd2f02; the
                    '{-1}', '{0}', float-literal forms)
  bits 12-13  abs / negate flags (testb $0x10 / $0x20 at 0xd2c00/34)
+0x06 u16:    REGISTER INDEX — indexes the per-class NAME TABLE
              (pretty names like fragment.texcoord[0]; a -1 or
              missing entry falls back to numeric at 0xd30cf)
swizzle:      decoded after the class name (.x/.y/.z/.w loop at
              0xd2f9e+; likely 2 bits × 4 components in the
              remaining word1 space — exact position unverified)
```

Name resolution: each class has a per-program name table (arrays of
char* indexed by the register index; `ncpy` copies the pretty name;
the `[name]` bracketed form at 0xd2d1d is the NAMED-binding display,
e.g. `prm0` shown as its binding when the table carries one).

## Still to decode (enumerated)

1. Per-opcode instruction lengths (the 140-way jump-table handlers).
2. Swizzle bit positions in word1.
3. The immediate encoding (float vs int discriminated by the width
   field's low bit — `F` vs `I` suffixes).
4. The TEX sampler operand (`texture[prm0.x:1], 2D` — its own
   formatter at 0xd5152 with the target kinds 1D/2D/3D/CUBE/RECT/
   SHADOW* at 0xfe736-0xfe760).
5. The declaration-table formats (TEMP/ATTRIB/PARAM/OUTPUT rows).

## Verification method (for the eventual probe/ppdec.c)

Differential: decode every corpus .bin with the pure-C decoder and
require byte-equality with the banked glp text
(`ppcorpus-2026-08-24.txt`) for all 22 streams before the decoder is
trusted anywhere.

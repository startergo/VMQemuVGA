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
  bits  6-13  OPCODE  =  (w0 & 0x3FFF) >> 6   (indexes ppstreamOpString[])
  bit  14     MODIFIER (set on many op words; NOT part of the opcode —
              masking it was the rung-77 part-8 breakthrough)
+0x04 u32:
  bits 14-31  field (unnamed yet — flags/operand count class)
```

## Instruction length (SOLVED — uniform)

Every instruction is `op word + n operand words`: **L = n + 1**,
with exactly two class overrides:

- **branch ops** (IF, and by table symmetry BRA/CAL/LOOP/REP/WHILE):
  L = n + 2 — the extra word is the branch target (the disassembler's
  `# Target:` comment).
- **END ops** (ENDIF; by symmetry ENDLOOP/ENDREP/ENDWHILE/ELSE):
  L = n + 2 — the extra word is the block index (`# Index:`).

Evidence (rung 77 part 9): the differential gate walks all 22 corpus
streams end-to-end with per-instruction opcode verification under this
rule and nothing else — no per-opcode length table, no annotation
distinction, no per-opcode extras. `probe/ppgate.py` (exit 0 = pass).

Falsified en route, for the record: "raster-op MOV = 2 words" (it is
3 = n+1; proven structurally — the `MOV res0, att0` words in the
raster pair are byte-identical operand encodings to the fragment
streams' res0/tmp0/att0 operand words); "annotated L=n+1, plain L=n"
(plain is also n+1); "TEX/NRM/SUB/MAD need per-opcode extras" (none
do); "uniform 3-word trailer" (the tail is the PARAM block below; the
"3 zeros" were end-of-buffer padding on files with no inline params).

## Stream tail after the last instruction

- Fragment (0x8B30): zero padding, then the **inline PARAM constant
  block at the next EVEN word index** (128-bit aligned), then zeros to
  the end of the buffer. Each inline `PARAM prmN = {c1,c2,c3,c4}` is
  2 words: `(c2<<32|c1)` then `(c4<<32|c3)`. `program.local[]` /
  `state.*` params store nothing (their files end in plain zeros).
- Raster-op (0x8804): the 2-word epilogue `(0x8, 0x0)`.

Verified exactly: every corpus file's tail equals its declarations'
float values bit-for-bit (see `tail_ok()` in ppgate.py).

## Operand-word identities (anchors for further decode)

From cross-file structural alignment: `att0 = 0x19c800`,
`tmp0 = 0x19c840` (source-position operand words, class prefix
0x19c8xx with the register index in the low bits), `res0 = 0x7267b000`
and tmp destinations `0x7267xxx` (destination-position words, a
different encoding class — s2's `tmp0` as destination is `0x72679000`
while s13's `tmp0` as source is `0x19c840`).

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

1. The OPERAND-COUNT encoding: lengths are uniform (n+1) but a
   standalone decoder still needs n from the stream — either a field
   in the op word (not the +0x04 high dword: s13's n=2 MOVs and n=4
   TEX share 0x31 there) or a static per-opcode arity table cut from
   glpDisassemble1Op's 140-way jump table. The gate derives n from the
   banked text, so this is open for the runtime translator.
2. Swizzle bit positions in word1.
3. The immediate encoding (float vs int discriminated by the width
   field's low bit — `F` vs `I` suffixes).
4. The TEX sampler operand (`texture[prm0.x:1], 2D` — its own
   formatter at 0xd5152 with the target kinds 1D/2D/3D/CUBE/RECT/
   SHADOW* at 0xfe736-0xfe760).
5. The declaration-table formats (TEMP/ATTRIB/PARAM/OUTPUT rows).
6. RET's length is formally 1-or-2 ambiguous (trailing zeros absorb
   the difference in every corpus file); the gate carries 2 = n+1.
   The raster epilogue word 0x8 is likewise uninterpreted.

## Verification (the gate — CLOSED 22/22, 2026-08-24)

`python3 probe/ppgate.py` walks every corpus stream under the model
above (anchor = first nonzero word whose opcode matches the first
mnemonic; every instruction's op word checked against its mnemonic's
table index; tail checked against the PARAM block / raster epilogue).
22/22 PASS, single anchor per file. The stronger release gate for a
future text-emitting decoder remains byte-equality with
`ppcorpus-2026-08-24.txt`.

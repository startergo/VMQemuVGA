#!/usr/bin/env python3
"""pptrans.py — RUNG 79: the PP-stream -> GLSL translator (offline).

Decodes streams from RAW WORDS ONLY (no banked text) using:
  - opcode      = (w0 & 0x3FFF) >> 6          (rung 77 part 8)
  - length      = n + 1                       (rung 77 part 9, uniform)
                  +1 for branch ops (IF target word) / END ops (ENDIF index)
  - n (operand count) = ARITY[op]             (rung 79: uniform per opcode
                  across all 22 corpus files — corpus-derived)
  - source operand word: class = (low24>>6)&3 {0 att, 1 tmp, 2 prm},
                  register index = u16 @ +0x6 (bits 48-63),
                  swizzle = SWZ[delta], delta = low24 minus class bits,
                  negate = bit 4 of delta
  - destination word: index = bits 48-63, mask = MASK[low32 & 0x0FFFFFFF]
                  (top nibble of low32 varies 0x1/0x7 between files with
                  identical text — masked off; unexplained variance)
  - TEX: word3 = sampler (prm source word), word4 = kind (3 = 2D)
  - stream tail = inline PARAM block at the next even index, 2 words per
                  param (c2<<32|c1)(c4<<32|c3)          (rung 77 part 9)

Usage:
  pptrans.py FILE.bin ANCHOR            decode + regenerate instruction text
  pptrans.py FILE.bin ANCHOR --glsl     also emit a GLSL 1.10 fragment shader
  pptrans.py --selftest                 s13 text regen + param round-trip
Anchors for the corpus are validated unique per file by ppgate.py;
text-free anchor discovery remains open (see ledger rung 79).
"""
import struct
import sys

def load_ops(path):
    ops = {}
    for line in open(path):
        p = line.split()
        if len(p) == 2 and p[0].isdigit():
            ops[int(p[0])] = p[1]
    return ops

ARITY = {
    'MOV': 2, 'ADD': 3, 'SUB': 3, 'MUL': 3, 'DIV': 3, 'DOT': 3, 'MAX': 3,
    'MIN': 3, 'NRM': 2, 'LEN': 2, 'POW': 3, 'LRP': 4, 'RFL': 3,
    'SGE': 3, 'SGT': 3, 'SLT': 3, 'ANL': 3, 'TEX': 4, 'RET': 1,
    'IF': 1, 'ENDIF': 0,
}
BRANCH = {'IF', 'BRA', 'CAL', 'LOOP', 'REP', 'WHILE'}
ENDOPS = {'ENDIF', 'ENDLOOP', 'ENDREP', 'ENDWHILE', 'ELSE'}

def ilen(mnem):
    return ARITY[mnem] + 1 + (1 if mnem in BRANCH or mnem in ENDOPS else 0)

# delta (low24 minus class bits) -> (swizzle, size)
SWZ = {
    0x000000: ('x', 1), 0x00aa00: ('y', 1), 0x08a800: ('xy', 2),
    0x114800: ('xyz', 3), 0x18a800: ('xyyy', 4), 0x19c800: ('xyzw', 4),
    0x1fc800: ('xyzw', 4),   # prm matrix variant (":4:4" in banked text)
}
NEG = 0x10

# low32 & 0x0FFFFFFF -> (register class, mask); class+mask are ONE code —
# the nibble at bits 8-11 is 0/1 on xy-family words and 9/b on full-family
# words, so no separate class field is extractable at a fixed position
MASK = {
    0x2041000: ('tmp', 'x'), 0x2261000: ('tmp', 'xy'),
    0x2241000: ('tmp', 'x_'), 0x0221000: ('tmp', '_x'),
    0x2471000: ('tmp', 'xyz'), 0x2641000: ('tmp', 'x___'),
    0x2671000: ('tmp', 'xyz_'), 0x2679000: ('tmp', 'xyzw'),
    0x2609000: ('tmp', '___x'),
    0x267b000: ('res', 'xyzw'), 0x2673000: ('res', 'xyz_'),
    0x260b000: ('res', '___x'),
}
CLASS = {0: 'att', 1: 'tmp', 2: 'prm'}
TEXKIND = {3: '2D'}

def op_of(w): return (w & 0x3FFF) >> 6
def idx_of(w): return (w >> 48) & 0xFFFF

def dec_src(w):
    low = w & 0xFFFFFF
    idx = idx_of(w)
    cls = (low >> 6) & 3
    delta = low & ~0xC0
    neg = bool(delta & NEG)
    key = delta & ~NEG if neg else delta
    if key not in SWZ:
        raise ValueError('unknown swizzle delta %#x' % delta)
    swz, size = SWZ[key]
    return '%s%d' % (CLASS.get(cls, 'cls%d' % cls), idx), swz, size, neg

def dec_dst(w):
    key = w & 0x0FFFFFFF
    if key not in MASK:
        raise ValueError('unknown dst mask %#x (w=%#x)' % (key, w))
    cls, mask = MASK[key]
    return '%s%d' % (cls, idx_of(w)), mask

def decode(path, anchor, ops):
    d = open(path, 'rb').read()
    total = len(d) // 8
    ws = struct.unpack('<%dQ' % total, d)
    i = anchor
    instrs = []
    while i < total - 3:
        # stop heuristics (instruction-region boundary, corpus-true):
        # RET terminates fragment main; a zero word is padding/params;
        # 0x8 is the raster-op (0x8804) epilogue word.
        if instrs and instrs[-1][0] == 'RET':
            break
        if ws[i] == 0 or ws[i] == 8:
            break
        m = ops.get(op_of(ws[i]))
        if m is None or m not in ARITY:
            break
        words = [ws[i + 1 + k] for k in range(ilen(m) - 1)]
        instrs.append((m, i, words))
        i += ilen(m)
    return ws, instrs, i

def render(m, words):
    if m == 'RET':
        return 'RET (TR.xxxx);'
    if m == 'ENDIF':
        return 'ENDIF;'
    dst, mask = dec_dst(words[0])
    parts = []
    for k, w in enumerate(words):
        if m == 'TEX' and k == 3:
            parts.append(TEXKIND.get(w, 'kind%d' % w)); continue
        if m == 'TEX' and k == 2:
            nm, swz, sz, _ = dec_src(w)
            parts.append('texture[%s.%s:%d]' % (nm, swz, sz)); continue
        if k == 0:
            if mask == 'xyzw':
                sfx = ''
            elif '_' in mask:
                sfx = ':2' if mask in ('x_', '_x') else ''
            else:
                sfx = ':%d' % len(mask)
            parts.append('%s.%s%s' % (dst, mask, sfx))
        else:
            nm, swz, sz, neg = dec_src(w)
            body = '%s.%s%s' % (nm, swz, ':%d' % sz if sz < 4 else '')
            parts.append(('-' + body) if neg else body)
    return '%s %s;' % (m, ', '.join(parts))

def f32u(x): return struct.unpack('<I', struct.pack('<f', x))[0]
def u2f(x): return struct.unpack('<f', struct.pack('<I', x & 0xFFFFFFFF))[0]

def decode_params(ws, end):
    total = len(ws)
    j = end
    while j < total and ws[j] == 0:
        j += 1
    if j >= total or j % 2:
        return [], None
    params = []
    last_nz = -2
    k = j
    while k + 1 < total:
        params.append((u2f(ws[k]), u2f(ws[k] >> 32),
                       u2f(ws[k + 1]), u2f(ws[k + 1] >> 32)))
        if ws[k] or ws[k + 1]:
            last_nz = k + 1 - j
        k += 2
    return params[:last_nz // 2 + 1], j

def encode_params(params):
    out = []
    for (c1, c2, c3, c4) in params:
        out += [(f32u(c2) << 32) | f32u(c1), (f32u(c4) << 32) | f32u(c3)]
    return out

def emit_glsl(m, words, body):
    dst, mask = dec_dst(words[0])
    if dst == 'res0':
        dst = 'gl_FragColor'
    if m == 'MOV':
        nm, swz, sz, neg = dec_src(words[1])
        if mask == 'xyzw' and swz == 'xyzw':
            body.append('%s = %s%s;' % (dst, '-' if neg else '', nm))
        else:
            body.append('%s.%s = %s%s.%s;'
                        % (dst, mask, '-' if neg else '', nm,
                           swz if swz != 'xyzw' else 'xyzw'))
        return
    if m == 'TEX':
        cnm, cswz, csz, _ = dec_src(words[1])
        snm, _, _, _ = dec_src(words[2])
        fn = {'2D': 'texture2D'}[TEXKIND.get(words[3], '2D')]
        sel = 'xyzw' if mask == 'xyzw' else mask[0]
        body.append('%s%s = %s(%s, %s.%s).%s;'
                    % (dst, '' if mask == 'xyzw' else '.' + mask,
                       fn, snm, cnm, cswz[:2], sel))
        return
    raise NotImplementedError('GLSL emit for %s' % m)

def glsl_for(instrs):
    samplers, attrs, tmps = set(), set(), set()
    for (m, pos, words) in instrs:
        if m == 'TEX':
            samplers.add(dec_src(words[2])[0])
        for w in ([words[1]] if m == 'TEX' else words[1:]):
            nm = dec_src(w)[0]
            if nm.startswith('att'):
                attrs.add(nm)
            elif nm.startswith('tmp'):
                tmps.add(nm)
    body = []
    for (m, pos, words) in instrs:
        emit_glsl(m, words, body)
    out = [b.replace('res0.', 'gl_FragColor.') for b in body]
    src = ['#version 110']
    src += ['uniform sampler2D %s;' % s for s in sorted(samplers)]
    src += ['varying vec4 %s;' % a for a in sorted(attrs)]
    src.append('void main() {')
    if tmps:
        src.append('    vec4 %s;' % ', '.join(sorted(tmps)))
    src += ['    ' + b for b in out]
    src.append('}')
    return '\n'.join(src)

def main():
    here = '/'.join(__file__.split('/')[:-1])
    ops = load_ops(here + '/pp-opcodes.txt')
    if len(sys.argv) >= 2 and sys.argv[1] == '--selftest':
        return selftest(here, ops)
    path, anchor = sys.argv[1], int(sys.argv[2])
    ws, instrs, end = decode(path, anchor, ops)
    for (m, pos, words) in instrs:
        print('  ' + render(m, words))
    ps, pstart = decode_params(ws, end)
    print('  end=%d params=%d' % (end, len(ps)))
    if '--glsl' in sys.argv:
        print(glsl_for(instrs))

def selftest(here, ops):
    ok = True
    name = 'ppcorpus-2026-08-24/p2773_s13_t8b30_w36.bin'
    ws, instrs, end = decode(here + '/' + name, 22, ops)
    want = ['MOV tmp1.xy:2, att0.xy:2;',
            'TEX tmp0.xyzw, tmp1.xyyy, texture[prm0.x:1], 2D;',
            'MOV res0.xyzw, tmp0.xyzw;',
            'RET (TR.xxxx);']
    got = [render(m, words) for (m, _, words) in instrs]
    for g, w in zip(got, want):
        good = g == w
        print('%s %s' % ('OK ' if good else 'DIFF', g))
        if not good:
            print('     want: %s' % w)
            ok = False
    for name, anchor, nprm in [
            ('ppcorpus-2026-08-24/p2773_s7_t8b30_w110.bin', 39, 7),
            ('ppcorpus-2026-08-24/p2773_s15_t8b30_w362.bin', 94, 18)]:
        ws, instrs, end = decode(here + '/' + name, anchor, ops)
        ps, pstart = decode_params(ws, end)
        re_enc = encode_params(ps)
        raw = list(ws[pstart:pstart + len(re_enc)]) if pstart is not None else []
        same = re_enc == raw
        good = same and len(ps) == nprm
        print('%s %s params=%d/%d roundtrip bits %s'
              % ('OK ' if good else 'DIFF', name.split('/')[-1],
                 len(ps), nprm, 'EXACT' if same else 'MISMATCH'))
        if not good:
            ok = False
    print('SELFTEST', 'PASS' if ok else 'FAIL')
    return 0 if ok else 1

if __name__ == '__main__':
    sys.exit(main() or 0)

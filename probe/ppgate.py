#!/usr/bin/env python3
"""ppgate.py — the rung-77 differential gate (CLOSED 2026-08-24, 22/22).

Walks the instruction sequence of every corpus stream and verifies the
PP word format end-to-end against the banked glpPPDisassemble text.

Model under test (see pp-word-format.md):
  opcode      = (w0 & 0x3FFF) >> 6            (bit 14 = modifier)
  length      = n + 1                         (op word + n operand words)
                +1 for branch ops (IF: target word)
                +1 for END ops    (ENDIF: index word)
  anchor      = first nonzero word whose opcode == first mnemonic's
  tail        = zero pad, then the inline PARAM constant block at the
                next EVEN word index (2 words per param:
                (c2<<32|c1),(c4<<32|c3)), then zeros.
                Raster (type 0x8804) instead: the 2-word epilogue (0x8, 0).

Inputs (relative to this file): pp-opcodes.txt, ppcorpus-2026-08-24.txt,
ppcorpus-2026-08-24/*.bin. The opcode table is LOADED, never hand-copied.

Usage: python3 probe/ppgate.py     (from the repo root)
"""
import os
import re
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))

opidx = {}
for line in open(os.path.join(HERE, 'pp-opcodes.txt')):
    p = line.split()
    if len(p) == 2 and p[0].isdigit():
        opidx[p[1]] = int(p[0])
assert len(opidx) == 140, len(opidx)

def op(w): return (w & 0x3FFF) >> 6

BR = {'IF', 'BRA', 'CAL', 'LOOP', 'REP', 'WHILE'}
EN = {'ENDIF', 'ENDLOOP', 'ENDREP', 'ENDWHILE', 'ELSE'}
def Lof(m, n): return n + 1 + (1 if m in BR or m in EN else 0)

def f32(x): return struct.unpack('<I', struct.pack('<f', x))[0]

seqs = {}; prms = {}; cur = None; decl = True
for line in open(os.path.join(HERE, 'ppcorpus-2026-08-24.txt')):
    m = re.match(r'=== (\S+?):', line)
    if m:
        cur = m.group(1); seqs[cur] = []; prms[cur] = []; decl = True
        continue
    if cur is None: continue
    s = line.strip()
    if s == 'main:':
        decl = False; continue
    if decl:
        pm = re.match(r'PARAM (\S+?):\S+ = \{([^}]*)\};', s)
        if pm: prms[cur].append([float(x) for x in pm.group(2).split(',')])
        continue
    if not s or s == 'END': continue
    s = s.split('#')[0].rstrip()
    if not s: continue
    m = re.match(r'([A-Z][A-Z0-9]*)(?::([0-9]+[FIB]))?\s*(.*);', s)
    if not m: continue
    mnem, rest = m.group(1), m.group(3)
    n = 0
    if rest:
        d = 0; n = 1
        for ch in rest:
            if ch in '([': d += 1
            elif ch in ')]': d -= 1
            elif ch == ',' and d == 0: n += 1
    seqs[cur].append((mnem, n))

def tail_ok(ws, E, raster, params):
    total = len(ws)
    if E >= total: return False, 'overrun'
    rest = ws[E:]
    if raster:
        return (list(rest) == [8, 0]), 'raster epi (0x8,0)'
    j = 0
    while j < len(rest) and rest[j] == 0:
        j += 1
    if j == len(rest):
        return True, 'all zeros'
    start = E + j
    if start % 2 != 0:
        return False, 'param block at odd index %d' % start
    exp = []
    for v in params:
        exp += [(f32(v[1]) << 32) | f32(v[0]), (f32(v[3]) << 32) | f32(v[2])]
    got = list(rest[j:])
    if not exp:
        return False, 'nonzero tail but no inline params (%#x)' % got[0]
    if got[:len(exp)] != exp:
        return False, 'param values mismatch'
    if any(x != 0 for x in got[len(exp):]):
        return False, 'nonzero after param block'
    return True, 'params@%d (%dx2w) + zeros' % (start, len(exp) // 2)

npass = 0
for name in sorted(seqs):
    seq = seqs[name]; params = prms[name]
    d = open(os.path.join(HERE, 'ppcorpus-2026-08-24', name), 'rb').read()
    total = len(d) // 8
    ws = struct.unpack('<%dQ' % total, d)
    raster = 't8804' in name
    want0 = opidx[seq[0][0]]
    good = []
    for h in range(2, total):
        if ws[h] == 0 or op(ws[h]) != want0: continue
        i = h; ok = True
        for (m, n) in seq:
            L = Lof(m, n)
            if i + L > total or op(ws[i]) != opidx.get(m, -1): ok = False; break
            i += L
        if ok:
            tok, why = tail_ok(ws, i, raster, params)
            if tok: good.append((h, i, why))
    if good:
        npass += 1
        h, i, why = good[0]
        print('PASS %s: anchor h=%d end=%d anchors=%d tail: %s'
              % (name, h, i, len(good), why))
    else:
        print('FAIL %s' % name)
print('\nGATE: %d/%d' % (npass, len(seqs)))
sys.exit(0 if npass == len(seqs) else 1)

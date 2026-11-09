#!/usr/bin/env python3
"""build_globals_cmp.py --orig <snapshot prefix> --core <snapshot prefix> [--exe <snapshot prefix of the bare image>] [--ignore file]
Compares the core's startup state (tools/build_globals_test) with the original's at the same point (the oracle tracer
core/oracle/build_globals.cpp: REFLEX_GLOBALS_SNAP). Snapshot = <p>.img (0x00916000 .. 0x00dd1000 raw), <p>.heap ({u32 addr,
4096 bytes} pages), and for the core <p>.blocks ("addr size" per live heap block).
Walk: every word of the image is compared; two words that are both heap pointers (0x10000000 .. 0x70000000) are paired as
(original block, core block) and the core block's payload is compared word by word (recursively; the pairing must stay consistent).
With --exe (the core's snapshot of the image alone): words the original changed (orig != exe) and the core does not reproduce are
'missing'; words the original left alone and the core changed are 'extra'. Output: counts per 64 KB region, the first lines of
each class, and with -v every difference."""
import argparse, bisect, struct, sys, collections
LO, HI = 0x00916000, 0x00dd1000
HEAP_LO, HEAP_HI = 0x10000000, 0x70000000

def load(prefix, blocks=False):
    img = open(prefix + '.img', 'rb').read()
    pages = {}
    try:
        h = open(prefix + '.heap', 'rb').read()
        for i in range(0, len(h), 4100):
            pages[struct.unpack_from('<I', h, i)[0]] = h[i + 4:i + 4100]
    except FileNotFoundError:
        pass
    blk = []
    if blocks:
        for l in open(prefix + '.blocks'):
            a, s = l.split(); blk.append((int(a, 16), int(s, 16)))
        blk.sort()
    return img, pages, blk

def word(img, pages, a):
    if LO <= a < HI: return struct.unpack_from('<I', img, a - LO)[0]
    p = pages.get(a & ~0xfff)
    return None if p is None else struct.unpack_from('<I', p, a & 0xfff)[0]

def is_heap(v): return HEAP_LO <= v < HEAP_HI

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--orig', required=True); ap.add_argument('--core', required=True)
    ap.add_argument('--exe'); ap.add_argument('--ignore'); ap.add_argument('-v', action='store_true')
    ap.add_argument('--range', help='lo-hi hex: only this part of the image')
    ap.add_argument('--max', type=int, default=40)
    a = ap.parse_args()
    oi, op, _ = load(a.orig); ci, cp, cb = load(a.core, True)
    ei = load(a.exe)[0] if a.exe else None
    ign = []
    if a.ignore:
        for l in open(a.ignore):
            l = l.split('#')[0].split()
            if len(l) >= 2: ign.append((int(l[0], 16), int(l[1], 16), ' '.join(l[2:])))
    def ignored(addr):
        for lo, hi, why in ign:
            if lo <= addr < hi: return why
        return None
    lo, hi = LO, HI
    if a.range: lo, hi = [int(x, 16) for x in a.range.split('-')]
    cstarts = [b[0] for b in cb]
    def cblock(addr):
        i = bisect.bisect_right(cstarts, addr) - 1
        if i >= 0 and addr < cb[i][0] + cb[i][1]: return cb[i]
        return None
    # ---- the image words
    diffs = []     # (addr, orig, core, kind)
    pairs = {}     # core block start -> original block start
    work = []
    for ad in range(lo, hi, 4):
        o = struct.unpack_from('<I', oi, ad - LO)[0]; c = struct.unpack_from('<I', ci, ad - LO)[0]
        if o == c: continue
        if is_heap(o) and is_heap(c):
            work.append((o, c, ad)); continue
        diffs.append((ad, o, c, 'img'))
    # ---- pointer pairs: follow
    inconsistent = 0; walked = 0
    while work:
        o, c, src = work.pop()
        b = cblock(c)
        if b is None:
            diffs.append((src, o, c, 'ptr-nocoreblock')); continue
        off = c - b[0]; ob = o - off
        if b[0] in pairs:
            if pairs[b[0]] != ob:
                inconsistent += 1; diffs.append((src, o, c, 'ptr-inconsistent'))
            continue
        pairs[b[0]] = ob; walked += 1
        for k in range(0, b[1], 4):
            wo = word(oi, op, ob + k); wc = word(ci, cp, b[0] + k)
            if wo is None or wc is None:
                diffs.append((b[0] + k, wo if wo is not None else -1, wc if wc is not None else -1, 'heap-missing-page')); continue
            if wo == wc: continue
            if is_heap(wo) and is_heap(wc): work.append((wo, wc, b[0] + k)); continue
            diffs.append((b[0] + k, wo, wc, 'heap'))
    print('image words compared %d; heap blocks paired %d; inconsistent pointers %d; differing words %d' % ((hi - lo) // 4, walked, inconsistent, len(diffs)))
    cls = collections.Counter(); regions = collections.defaultdict(list)
    for ad, o, c, kind in diffs:
        why = ignored(ad) if kind == 'img' else None
        if why: cls['ignored: ' + why] += 1; continue
        if kind == 'img' and ei is not None:
            e = struct.unpack_from('<I', ei, ad - LO)[0]
            k2 = 'missing(orig!=exe, core==exe)' if c == e else ('extra(orig==exe)' if o == e else 'differs(both changed)')
        else: k2 = kind
        cls[k2] += 1; regions[(k2, ad >> 12 if kind == 'img' else kind)].append((ad, o, c))
    for k, n in sorted(cls.items()): print('  %-34s %d' % (k, n))
    rows = sorted(regions.items(), key=lambda kv: (kv[0][0], str(kv[0][1])))
    shown = 0
    for (k2, reg), l in rows:
        if shown >= a.max and not a.v: break
        shown += 1
        print('%s %s: %d words, first %08x orig %08x core %08x' % (k2, ('page %08x' % (reg << 12)) if isinstance(reg, int) else reg, len(l), l[0][0], l[0][1], l[0][2]))
        if a.v:
            for ad, o, c in l: print('    %08x orig %08x core %08x' % (ad, o, c))
    return 0

sys.exit(main())

#!/usr/bin/env python3
"""rx_canon_cmp.py --orig <canon> <world> --core <canon> [<world>] [--show N] anchor... -- anchored comparison of a core-built (partial)
world with the original's (core/notes/construction.md).

world_compare.py compares two whole worlds by node names (first paths from the roots). While the core builds only part of the world,
names differ (a node is named through objects the core has not built yet), so this tool pairs objects from anchors and walks both heaps
in parallel along pointer words: two pointers at the same word of paired nodes pair their targets (same offset into the target), plain
words must be equal. Where the original has a pointer and the core a 0 the subtree is MISSING (not built yet: counted, not walked).
Inputs: the original's canonical dump and world image (reflex_boot REFLEX_WORLD_CANON / REFLEX_WORLD_OUT); the core's canonical dump
(rx_canon_write; its <canon>.addr sidecar gives the addresses) and optionally its image (rx_canon_image, needed for G: anchors).
Anchors (several allowed):
  G:<global>                    the pointer stored at this .data address in both worlds
  A:<orig addr>=<core addr>     an object in each world (an address inside a node; the offsets into the nodes must agree)
  H:<vtable>:<name>=<core addr> the original's asset handle of that vtable whose inline name (+4) is <name>; its +0x44 (the asset)
--cstr CLASS:field:stride: the block at that field is C strings in stride-byte slots (compared up to each NUL).
--undef <core image 2>: the core's image of a second build with RX_BUILD_FILL set: bytes the core never wrote are skipped.
--ignore CLASS:off,... skips those words of objects of that class (offsets from the object's vtable; fields another unit writes).
Output: pairs walked, identical, differing (first differences with the original's path), missing; exit 0 when nothing differs.
"""
import collections, os, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import build_map as bm

def load(canon, world=None):
    cb, nodes = bm.parse_canon(canon)
    side = {'cb': cb, 'nodes': nodes, 'pages': None}
    if os.path.exists(canon + '.addr'):
        d = open(canon + '.addr', 'rb').read()
        for i in range(0, len(d), 12):
            nm, a = struct.unpack_from('<QI', d, i)
            if nm in nodes: nodes[nm]['addr'] = a
    if world:
        side['pages'] = bm.image_pages(world)
        if not os.path.exists(canon + '.addr'): bm.resolve(cb, nodes, side['pages'])
    res = sorted((n['addr'], n['name']) for n in nodes.values() if n['addr'] is not None)
    side['starts'] = [a for a, _ in res]; side['names'] = [nm for _, nm in res]
    return side

def word_img(side, a):
    p = side['pages'].get(a & ~0xfff) if side['pages'] else None
    return None if p is None else struct.unpack_from('<I', p, a & 0xfff)[0]

def node_at(side, a):
    import bisect
    i = bisect.bisect_right(side['starts'], a) - 1
    if i < 0: return None, 0
    n = side['nodes'][side['names'][i]]
    return (n, a - n['addr']) if a < n['addr'] + max(n['size'], 1) else (None, 0)

def words(side, n):
    cb = side['cb']
    w = list(struct.unpack_from('<%dI' % n['nw'], cb, n['words_at']))
    p = {wi: (kind, target, toff) for wi, kind, target, toff in n['ptrs']}
    return w, p

VT = {}
def classes():
    if VT: return VT
    R = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '_re', 'out', 'classes.tsv')
    for l in open(R, encoding='latin-1').readlines()[1:]:
        c = l.rstrip('\n').split('\t')
        try: VT[int(c[1], 16)] = c[0]
        except (ValueError, IndexError): pass
    return VT

def label(side, n):
    w, p = words(side, n)
    for k in (0, 4):   # the object's vtable: block start, or the payload behind the _aligned_malloc header
        if k < len(w) and k not in p and w[k] in classes(): return '%s(%x)' % (classes()[w[k]], n['size'])
    return 'heap%x' % n['size']

def main():
    a = sys.argv[1:]; orig = core = None; anchors = []; show = 20; ignore = {}; cstr = {}; undef = None
    i = 0
    while i < len(a):
        if a[i] == '--orig': orig = (a[i + 1], a[i + 2]); i += 3
        elif a[i] == '--core':
            if i + 2 < len(a) and not a[i + 2].startswith('-') and ':' not in a[i + 2]: core = (a[i + 1], a[i + 2]); i += 3
            else: core = (a[i + 1], None); i += 2
        elif a[i] == '--cstr':   # CLASS:field:stride  the block that field points to holds C strings in stride-byte slots: compare
            c, fo, st = a[i + 1].split(':'); cstr[(c, int(fo, 16))] = int(st, 16); i += 2   # each slot up to its NUL only
        elif a[i] == '--undef':   # the core's image from a second build with another RX_BUILD_FILL: bytes that differ were never written
            undef = bm.image_pages(a[i + 1]); i += 2
        elif a[i] == '--show': show = int(a[i + 1]); i += 2
        elif a[i] == '--ignore':   # CLASS:off,off,...  words the comparison skips (fields another unit writes)
            c, offs = a[i + 1].split(':'); ignore.setdefault(c, set()).update(int(x, 16) for x in offs.split(',')); i += 2
        elif a[i].startswith('@'): anchors += [l.strip() for l in open(a[i][1:]) if l.strip()]; i += 1
        else: anchors.append(a[i]); i += 1
    O = load(*orig); C = load(*core)
    pairs = []
    for an in anchors:
        kind, rest = an.split(':', 1)
        if kind == 'G':
            g = int(rest, 16); pairs.append((word_img(O, g), word_img(C, g), an))
        elif kind == 'A':
            x, y = rest.split('='); pairs.append((int(x, 16), int(y, 16), an))
        elif kind == 'H':
            spec, y = rest.rsplit('=', 1); vt, nm = spec.split(':', 1)
            pairs.append((('H', int(vt, 16), nm), int(y, 16), an))
    # H anchors: one pass over the original's nodes per vtable -> {handle name: target of +0x44}
    hmaps = {}
    for vt in {p[0][1] for p in pairs if isinstance(p[0], tuple)}:
        m = hmaps[vt] = {}
        for n in O['nodes'].values():
            if n['addr'] is None or n['size'] < 0x50: continue
            w, p = words(O, n)
            for k in range(len(w) - 0x11):
                if w[k] == vt and k not in p and (k + 0x11) in p:
                    nm = b''.join(struct.pack('<I', x) for x in w[k + 1:k + 17]).split(b'\0')[0].decode('latin-1')
                    q = p[k + 0x11]
                    if q[0] == 1 and nm not in m: m[nm] = O['nodes'][q[1]]['addr'] + q[2]
    pairs = [((hmaps[p[0][1]].get(p[0][2]) if isinstance(p[0], tuple) else p[0]), p[1], p[2]) for p in pairs]
    unanch = []
    seen_o = {}; seen_c = {}
    queue = collections.deque()
    for oa, ca, an in pairs:
        if oa is None or ca is None: unanch.append(an); continue
        on, oo = node_at(O, oa); cn, co = node_at(C, ca)
        if on is None or cn is None: print('anchor %s: no node (orig %08x %s, core %08x %s)' % (an, oa, on is not None, ca, cn is not None)); continue
        if oo != co: print('anchor %s: offsets into the nodes differ (orig +%x, core +%x)' % (an, oo, co))
        queue.append((on['name'], cn['name'], 'anchor ' + an, 0))
    hist = collections.Counter()
    same = differ = 0; missing = collections.Counter(); diffs = []; extra = 0
    path = {}
    while queue:
        onm, cnm, via, stride = queue.popleft()
        if onm in seen_o or cnm in seen_c:
            if seen_o.get(onm) != cnm: diffs.append((via, 'pairing conflict: original node already paired with another core node')); differ += 1
            continue
        seen_o[onm] = cnm; seen_c[cnm] = onm; path[onm] = via
        on = O['nodes'][onm]; cn = C['nodes'][cnm]
        if stride:   # C-string slots (the bytes after each NUL are uninitialized heap in the original)
            def raw(side, n): return b''.join(struct.pack('<I', word_img(side, n['addr'] + 4 * k) or 0) for k in range(n['size'] // 4))
            ro, rc = raw(O, on), raw(C, cn); bad = []
            for st in range(0x10, min(len(ro), len(rc)) - stride + 1, stride):
                if ro[st:st + stride].split(b'\0')[0] != rc[st:st + stride].split(b'\0')[0]: bad.append('slot +%x' % st)
            if on['size'] != cn['size']: bad.append('size')
            if bad: differ += 1; diffs.append((via, 'strings: ' + ' '.join(bad[:8])))
            else: same += 1
            continue
        ow, op = words(O, on); cw, cp = words(C, cn)
        bad = []
        if on['size'] != cn['size']: bad.append('size %x / core %x' % (on['size'], cn['size']))
        lab = label(O, on); cname = lab.split('(')[0]; skip = set()
        for k0 in (0, 4):   # ignored offsets are relative to the object (block start, or the payload behind the aligned header)
            if k0 < len(ow) and k0 not in op and classes().get(ow[k0]) == cname: skip = {k0 + o // 4 for o in ignore.get(cname, ())}
        for k in range(min(len(ow), len(cw))):
            if k in skip: continue
            po, pc = op.get(k), cp.get(k)
            # data that looks like a pointer on one side only: the raw words decide (the original's from its image)
            if po is not None and pc is None and O['pages'] and word_img(O, on['addr'] + 4 * k) == cw[k]: continue
            if pc is not None and po is None and C['pages'] and word_img(C, cn['addr'] + 4 * k) == ow[k]: continue
            dmask = 0xffffffff
            if undef is not None:
                ca_ = cn['addr'] + 4 * k; p2 = undef.get(ca_ & ~0xfff); v2 = struct.unpack_from('<I', p2, ca_ & 0xfff)[0] if p2 else 0
                v1 = word_img(C, ca_) or 0; x = v1 ^ v2
                dmask = 0
                for bi in range(4):
                    if not (x >> (8 * bi)) & 0xff: dmask |= 0xff << (8 * bi)
                if dmask == 0: continue
                if (po is not None or pc is not None) and dmask != 0xffffffff: continue
            if po is None and pc is None:
                if (ow[k] ^ cw[k]) & dmask: bad.append('+%04x %08x / core %08x' % (4 * k, ow[k], cw[k]))
            elif po is not None and pc is not None:
                if O['pages'] and C['pages'] and word_img(O, on['addr'] + 4 * k) == word_img(C, cn['addr'] + 4 * k): continue   # data
                if po[0] != pc[0] or po[2] != pc[2]: bad.append('+%04x ptr kind/off %d+%x / core %d+%x' % (4 * k, po[0], po[2], pc[0], pc[2])); continue
                if po[0] == 1:
                    st = 0
                    for k0 in (0, 4):
                        if k0 < len(ow) and k0 not in op and classes().get(ow[k0]) == cname: st = cstr.get((cname, 4 * (k - k0)), 0)
                    queue.append((po[1], pc[1], '%s+%x' % (label(O, on), 4 * k), st))
            elif po is not None and pc is None and cw[k] == 0:
                missing['%s+%x' % (label(O, on), 4 * k)] += 1
            elif po is None and pc is not None and ow[k] == 0:
                extra += 1; bad.append('+%04x 0 / core pointer' % (4 * k))
            else:
                bad.append('+%04x %s / core %s' % (4 * k, 'ptr' if po else '%08x' % ow[k], 'ptr' if pc else '%08x' % cw[k]))
        for b_ in bad: hist['%s %s' % (label(O, on), b_.split(' ')[0])] += 1
        if bad: differ += 1; diffs.append(('%s@%08x/core %08x <- %s' % (label(O, on), on['addr'] or 0, cn['addr'] or 0, via), '; '.join(bad[:8]) + (' ... (%d words)' % len(bad) if len(bad) > 8 else '')))
        else: same += 1
    if unanch: print('%d anchors not in the original world (e.g. %s)' % (len(unanch), unanch[0]))
    print('pairs walked %d: identical %d, differing %d; missing subtrees (original pointer, core 0) %d at %d fields; core-only pointers %d'
          % (same + differ, same, differ, sum(missing.values()), len(missing), extra))
    for via, d in diffs[:show]: print('  DIFF %s: %s' % (via, d))
    if hist:
        print('differing words by (object, offset into the node):')
        for k, v in hist.most_common(show): print('  %-60s x%d' % (k, v))
    if missing:
        print('missing (not built by the core yet):')
        for k, v in missing.most_common(show): print('  %-60s x%d' % (k, v))
    print('VERDICT: %s' % ('IDENTICAL' if not differ else 'DIFFERENT'))
    return 0 if not differ else 1

if __name__ == '__main__':
    sys.exit(main())

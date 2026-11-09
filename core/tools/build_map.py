#!/usr/bin/env python3
"""build_map.py <canon.bin> <world.bin> <race.rmap> [out.tsv] -- the construction scope of a race, per heap node.

Joins three products of the same event (core/notes/construction.md):
  <canon.bin>  the original's canonical heap dump (reflex_boot REFLEX_WORLD_CANON, Boot::write_canonical), written at the world capture;
  <world.bin>  the world image (RXWI) of the same boot (REFLEX_WORLD_OUT), i.e. the memory the race starts from;
  <race.rmap>  the read map of a race on that world (REFLEX_READSET: 1 bit per 16-byte granule).
The canonical dump has no addresses: they are recovered from the image (root nodes: the word at the global that names them; every
other node: its parent's pointer word minus the stored target offset), then checked against the node's plain words.
Output (tsv, one row per reached node with at least one read granule): name, address, size, class, read bytes, owner (the nearest
typed ancestor and the field path to it), root path. A summary per class and per owner group goes to stdout.
"""
import bisect, collections, gzip, os, struct, sys, zlib

def parse_canon(path):
    b = gzip.open(path, 'rb').read()
    magic, ver, count, phase1, reached = struct.unpack_from('<5I', b, 0)
    assert magic == 0x4e435852 and ver == 1, path
    at = 20; nodes = {}
    for _ in range(count):
        phase, kind = b[at], b[at + 1]
        name, parent, off, size, vt = struct.unpack_from('<QQIII', b, at + 2); at += 30
        (cl,) = struct.unpack_from('<H', b, at); at += 2
        cls = b[at:at + cl].decode('latin-1'); at += cl
        h, nw = struct.unpack_from('<QI', b, at); at += 12
        words_at = at; at += 4 * nw
        (npt,) = struct.unpack_from('<I', b, at); at += 4
        ptrs = [struct.unpack_from('<IBQI', b, at + 17 * i) for i in range(npt)]; at += 17 * npt
        if phase == 3: continue
        nodes[name] = dict(phase=phase, kind=kind, name=name, parent=parent, off=off, size=size, vt=vt, cls=cls,
                           words_at=words_at, nw=nw, ptrs=ptrs, addr=None)
    return b, nodes

def image_pages(path):
    b = open(path, 'rb').read()
    magic, ver, n, sim = struct.unpack_from('<4I', b, 0)
    pages = {}
    for k in range(n):
        a, off, size = struct.unpack_from('<3I', b, 80 + 12 * k)
        pages[a] = bytes(4096) if size == 0 else b[off:off + 4096] if size == 4096 else zlib.decompress(b[off:off + size])
    return pages

def resolve(cb, nodes, pages):
    """Recovers each node's address from a world image (n['addr']); returns (word, plain_ok) helpers."""
    def word(a):
        p = pages.get(a & ~0xfff)
        if p is None or (a & 0xfff) > 0xffc: return None
        return struct.unpack_from('<I', p, a & 0xfff)[0]
    def plain_ok(n, a):   # the node's plain words (pointer words excluded) equal the image at a (first 16 checked)
        pw = {p[0] for p in n['ptrs']}; checked = 0
        for i in range(min(n['nw'], 64)):
            if i in pw or 4 * i + 4 > n['size']: continue
            v = struct.unpack_from('<I', cb, n['words_at'] + 4 * i)[0]
            if word(a + 4 * i) != v: return False
            checked += 1
            if checked >= 16: break
        return True
    # seeds: global roots (phase 1, parent 0, off = the global's address). Strong roots point at the start; weak roots (found in the
    # second pass) point inside: the start is found by matching the node's plain words in [v - size + 4, v] (4-byte steps)
    def find_start(n, v):
        if plain_ok(n, v): return v
        pw = {p[0] for p in n['ptrs']}
        sig = []
        for i in range(n['nw']):
            if i in pw or 4 * i + 4 > n['size']: continue
            w = struct.unpack_from('<I', cb, n['words_at'] + 4 * i)[0]
            if w: sig.append((i, w))
            if len(sig) >= 6: break
        if not sig: return None
        lo = max(v - n['size'] + 1, 0x10000) & ~3
        for s0 in range(v & ~3, lo - 1, -4):
            if all(word(s0 + 4 * i) == w for i, w in sig) and plain_ok(n, s0): return s0
        return None
    for n in nodes.values():
        if n['parent'] == 0 and n['phase'] == 1 and n['off'] >= 0x10000:
            v = word(n['off'])
            if v is not None: n['addr'] = find_start(n, v)
    # propagate along pointer words (kind 1: target name, offset into it)
    queue = [n for n in nodes.values() if n['addr'] is not None]
    while queue:
        n = queue.pop()
        for wi, kind, target, toff in n['ptrs']:
            if kind != 1: continue
            t = nodes.get(target)
            if t is None or t['addr'] is not None: continue
            v = word(n['addr'] + 4 * wi)
            if v is None: continue
            t['addr'] = (v - toff) & 0xffffffff; queue.append(t)
    return word, plain_ok

def main():
    canon, world, rmap_path = sys.argv[1:4]
    out = sys.argv[4] if len(sys.argv) > 4 else None
    cb, nodes = parse_canon(canon)
    pages = image_pages(world)
    word, plain_ok = resolve(cb, nodes, pages)
    resolved = [n for n in nodes.values() if n['addr'] is not None]
    bad = sum(1 for n in resolved if not plain_ok(n, n['addr']))
    print('nodes %d, addresses resolved %d (%d inconsistent)' % (len(nodes), len(resolved), bad))
    # read map -> granules per node
    import numpy as np
    rm = np.frombuffer(open(rmap_path, 'rb').read(), dtype=np.uint8)
    bits = np.unpackbits(rm, bitorder='little')
    gran = (np.nonzero(bits)[0].astype(np.uint64) * 16)
    ivs = sorted((n['addr'], n['addr'] + max(n['size'], 1), n['name']) for n in resolved)
    starts = np.array([x[0] for x in ivs], dtype=np.uint64); ends = np.array([x[1] for x in ivs], dtype=np.uint64)
    idx = np.searchsorted(starts, gran, side='right') - 1
    readb = collections.Counter(); unattributed = collections.Counter()
    ok = (idx >= 0) & (gran < ends[np.maximum(idx, 0)])
    for i in idx[ok]: readb[ivs[int(i)][2]] += 16
    for a in gran[~ok]:
        a = int(a)
        region = ('code' if 0x400000 <= a < 0x916000 else 'rdata' if a < 0x9a6000 and a >= 0x916000 else
                  'data' if 0x9a6000 <= a < 0xdd0000 else 'crt' if 0x78400000 <= a < 0x78600000 else 'other')
        unattributed[region] += 16
    # inner objects: the nearest vtable word (classes.tsv) at or below a read granule inside its node, within 0x4000 bytes
    R = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '_re', 'out')
    vtc = {}
    for l in open(os.path.join(R, 'classes.tsv'), encoding='latin-1').readlines()[1:]:
        c = l.rstrip('\n').split('\t')
        try: vtc[int(c[1], 16)] = c[0]
        except (ValueError, IndexError): pass
    vtarr = np.array(sorted(vtc), dtype=np.uint32)
    inner = collections.defaultdict(collections.Counter)     # node name -> class -> bytes
    inner_objs = collections.defaultdict(set)
    by_node = collections.defaultdict(list)
    for g, i in zip(gran[ok], idx[ok]): by_node[int(i)].append(int(g))
    for i, gl in by_node.items():
        s0, e0, nm = ivs[i]
        n = nodes[nm]; a0 = s0 & ~3; cnt = (min(e0, s0 + n['size']) - a0) // 4
        buf = bytearray()
        for pa in range(a0 & ~0xfff, a0 + 4 * cnt + 0xfff, 0x1000): buf += pages.get(pa, bytes(4096))
        w = np.frombuffer(bytes(buf), dtype='<u4')[(a0 & 0xfff) // 4:(a0 & 0xfff) // 4 + cnt]
        pos = np.nonzero(np.isin(w, vtarr))[0]
        for g in gl:
            k = np.searchsorted(pos, (g - a0) // 4 + 3, side='right') - 1
            if k >= 0 and g - (a0 + 4 * int(pos[k])) < 0x4000:
                cls = vtc[int(w[pos[k]])]; inner[nm][cls] += 16; inner_objs[(nm, cls)].add(int(pos[k]))
            else: inner[nm]['(raw)'] += 16
    print('read bytes in reached nodes: %d; outside: %s' % (sum(readb.values()), dict(unattributed)))
    # owners: nearest typed ancestor (with class) and the field path to it
    def owner(n):
        path = []; cur = n; depth = 0
        while cur is not None and depth < 12:
            if cur['cls'] and cur is not n: return cur['cls'], '+'.join('%x' % o for o in reversed(path))
            if cur['parent'] == 0: return ('global %08x' % cur['off'] if cur['phase'] == 1 and cur['off'] >= 0x10000 else 'root'), '+'.join('%x' % o for o in reversed(path))
            path.append(cur['off']); cur = nodes.get(cur['parent']); depth += 1
        return '?', ''
    def rootpath(n, depth=10):
        parts = []; cur = n
        while cur is not None and depth > 0:
            if cur['parent'] == 0:
                parts.append('global %08x' % cur['off'] if cur['phase'] == 1 and cur['off'] >= 0x10000 else 'root+%x' % cur['off']); break
            p = nodes.get(cur['parent'])
            parts.append('%s+%x' % ((p['cls'] or 'heap%x' % p['size']) if p else '?', cur['off'])); cur = p; depth -= 1
        return ' <- '.join(parts)
    # referrers: who points at each read node (typed: the class of the object the pointer word lies in, by vtable scan, and the
    # field offset from that object's start; untyped: the referrer node's size; globals: the .data address)
    def node_words(nm):
        n = nodes[nm]; a0 = n['addr'] & ~3; cnt = n['size'] // 4
        buf = bytearray()
        for pa in range(a0 & ~0xfff, a0 + 4 * cnt + 0xfff, 0x1000): buf += pages.get(pa, bytes(4096))
        return a0, np.frombuffer(bytes(buf), dtype='<u4')[(a0 & 0xfff) // 4:(a0 & 0xfff) // 4 + cnt]
    want = set(readb)
    refs = collections.defaultdict(list)
    for src in nodes.values():
        if src['addr'] is None: continue
        for wi, kind, target, toff in src['ptrs']:
            if kind == 1 and target in want and target != src['name']: refs[target].append((src['name'], wi, toff))
    gl = np.frombuffer(b''.join(pages.get(pa, bytes(4096)) for pa in range(0x9a6000, 0xdd0000, 0x1000)), dtype='<u4')
    gi = np.searchsorted(starts, gl.astype(np.uint64), side='right') - 1
    gok = (gi >= 0) & (gl.astype(np.uint64) < ends[np.maximum(gi, 0)])
    for k in np.nonzero(gok)[0]:
        nm = ivs[int(gi[k])][2]
        if nm in want: refs[nm].append(('G', 0x9a6000 + 4 * int(k), int(gl[k]) - ivs[int(gi[k])][0]))
    wcache = {}
    def ref_label(src, wi):
        if src == 'G': return 'global %08x' % wi
        if src not in wcache: wcache[src] = node_words(src)
        a0, w = wcache[src]
        pos = np.nonzero(np.isin(w[max(0, wi - 0x1000):wi + 1], vtarr))[0]
        if len(pos):
            k = max(0, wi - 0x1000) + int(pos[-1])
            return '%s+%x' % (vtc[int(w[k])], 4 * (wi - k))
        return 'heap%x+%x' % (nodes[src]['size'], 4 * wi)
    owners = {}
    for nm in want:
        lab = collections.Counter(ref_label(src, wi) + ('' if toff == 0 else '(+%x)' % toff) for src, wi, toff in refs.get(nm, [])[:64])
        owners[nm] = ','.join('%s%s' % (k, '' if v == 1 else 'x%d' % v) for k, v in lab.most_common(4))
    # the typed owner chain of an untyped node: follow referrers that point at the payload start (+0x10 behind the _aligned_malloc
    # header, or +0) upwards until a typed object (vtable scan) or a global; prefer typed referrers
    allrefs = collections.defaultdict(list)
    for src in nodes.values():
        if src['addr'] is None: continue
        for wi, kind, target, toff in src['ptrs']:
            if kind == 1 and toff in (0, 0x10) and target != src['name']: allrefs[target].append((src['name'], wi, toff))
    for k in np.nonzero(gok)[0]:
        if int(gl[k]) - ivs[int(gi[k])][0] in (0, 0x10): allrefs[ivs[int(gi[k])][2]].append(('G', 0x9a6000 + 4 * int(k), 0))
    def chain(nm, depth=0, seen=None):
        seen = seen or set(); seen.add(nm)
        best = None
        for src, wi, toff in allrefs.get(nm, []):
            if src in seen: continue
            lab = ref_label(src, wi)
            if not lab.startswith('heap'): return lab
            if best is None: best = (src, lab)
        if best is None or depth >= 5: return best[1] if best else '?'
        up = chain(best[0], depth + 1, seen)
        return '%s > %s' % (up, best[1])
    chains = {nm: chain(nm) for nm in want}
    per_chain = collections.defaultdict(lambda: [0, 0, 0])
    for nm in want:
        if nodes[nm]['cls']: continue
        if sum(v for k, v in inner[nm].items() if k != '(raw)') > inner[nm]['(raw)']: continue
        c = chains[nm]
        # collapse the leaf field offsets of the typed head: class+off > heapN+off ...
        per_chain[c][0] += 1; per_chain[c][1] += readb[nm]; per_chain[c][2] += nodes[nm]['size']
    print('\nraw nodes by typed owner chain (owner class+field > untyped hops)')
    for c, v in sorted(per_chain.items(), key=lambda kv: -kv[1][1])[:150]:
        print('  %-100s %5d %9d %9d' % (c[:100], *v))
    allc = collections.Counter(); allo = collections.Counter()
    for nm, c in inner.items():
        for k, v in c.items(): allc[k] += v
    for (nm, k), st in inner_objs.items(): allo[k] += len(st)
    print('\ninner classes (vtable scan inside nodes): %d' % len(allc))
    for k, v in allc.most_common(80): print('  %-56s %6d objs %9d bytes' % (k[:56], allo[k], v))
    rows = []
    per_class = collections.defaultdict(lambda: [0, 0, 0])
    per_owner = collections.defaultdict(lambda: [0, 0, 0])
    for nm, rb in readb.items():
        n = nodes[nm]
        oc, of = owner(n)
        label = n['cls'] or '(untyped)'
        per_class[label][0] += 1; per_class[label][1] += rb; per_class[label][2] += n['size']
        if not n['cls']:
            k = '%s +%s' % (oc, of)
            per_owner[k][0] += 1; per_owner[k][1] += rb; per_owner[k][2] += n['size']
        top = ','.join('%s:%d' % kv for kv in inner[nm].most_common(6))
        rows.append((nm, n['addr'], n['size'], n['cls'], rb, oc, of, rootpath(n), top, owners.get(nm, '') + ' || ' + chains.get(nm, '')))
    if out:
        with open(out.replace('.tsv', '') + '.classes.tsv', 'w') as f:
            f.write('class\tobjects\tread\n')
            for k, v in allc.most_common(): f.write('%s\t%d\t%d\n' % (k, allo[k], v))
        with open(out.replace('.tsv', '') + '.chains.tsv', 'w') as f:
            f.write('chain\tnodes\tread\tsize\n')
            for c, v in sorted(per_chain.items(), key=lambda kv: -kv[1][1]): f.write('%s\t%d\t%d\t%d\n' % (c, *v))
        with open(out, 'w') as f:
            f.write('name\taddr\tsize\tclass\tread\towner\towner_path\troot_path\tinner\treferrers\n')
            for r in sorted(rows, key=lambda r: -r[4]):
                f.write('%016x\t%08x\t%d\t%s\t%d\t%s\t%s\t%s\t%s\t%s\n' % r)
    print('\n%-58s %6s %9s %9s' % ('class', 'nodes', 'read', 'size'))
    for c, v in sorted(per_class.items(), key=lambda kv: -kv[1][1])[:120]:
        print('%-58s %6d %9d %9d' % (c[:58], *v))
    print('\nuntyped blocks by owner (nearest typed ancestor + field path)')
    for c, v in sorted(per_owner.items(), key=lambda kv: -kv[1][1])[:120]:
        print('%-70s %6d %9d %9d' % (c[:70], *v))

if __name__ == '__main__':
    main()

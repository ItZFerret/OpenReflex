#!/usr/bin/env python3
"""world_compare.py <a-canon.bin> <b-canon.bin> [--classes N] [--examples K] [--class NAME] -- compares two worlds without their layout.

Loading ports change where things live on the heap (allocation order, natives' own allocations), so world images from two boots are
not byte-comparable. reflex_boot writes a canonical dump next to the world image (REFLEX_WORLD_CANON=, boot.cpp Boot::write_canonical):
every heap block / VirtualAlloc region reached from the modules' writable globals and the threads' TLS (then from the live stacks),
named by its first breadth-first path from those roots (a 64-bit hash of parent name and field offset), with its class (RTTI), size,
the plain words, and every pointer word as (target name, offset into the target) instead of an address. Two worlds are the same world
when the same named nodes exist with the same contents; this prints, per class, how many nodes are identical, differ, or exist on one
side only, then examples of differences (the node's path from its root and the first differing words). Unreached blocks (garbage) are
compared as a multiset of (class, size, contents). Exit status 0 when identical.
(native/game notes/loading.md, "Acceptance".)"""
import collections, gzip, struct, sys, zlib
import numpy as np

def parse(path):
    b = gzip.open(path, 'rb').read()
    magic, ver, count, phase1, reached = struct.unpack_from('<5I', b, 0)
    assert magic == 0x4e435852 and ver == 1, path
    at = 20
    nodes, garbage = {}, collections.Counter()
    for _ in range(count):
        phase, kind = b[at], b[at + 1]
        name, parent, off, size, vt = struct.unpack_from('<QQIII', b, at + 2); at += 2 + 28
        (cl,) = struct.unpack_from('<H', b, at); at += 2
        cls = b[at:at + cl].decode('latin-1'); at += cl
        h, nw = struct.unpack_from('<QI', b, at); at += 12
        words_at = at; at += 4 * nw
        (np_,) = struct.unpack_from('<I', b, at); at += 4
        ptr_at = at; at += 17 * np_
        rec = (phase, kind, name, parent, off, size, vt, cls, h, words_at, nw, ptr_at, np_)
        if phase == 3: garbage[(cls, size, h)] += 1
        else: nodes[name] = rec
    return b, nodes, garbage

def label(rec):
    cls = rec[7]
    if cls: return cls
    kind = {1: 'heap', 2: 'device', 3: 'region'}[rec[1]]
    s = rec[5]
    bucket = s if s <= 64 else 1 << (s - 1).bit_length()
    return '(%s %s%d)' % (kind, '' if s <= 64 else '<=', bucket)

def path_of(nodes, rec, depth=8):
    parts = []
    while rec is not None and depth > 0:
        parent, off = rec[3], rec[4]
        if parent == 0:
            root = 'global %08x' % off if rec[0] == 1 and off >= 0x10000 else ('tls+%x' % off if rec[0] == 1 else 'stack-%x' % off)
            parts.append(root); break
        p = nodes.get(parent)
        parts.append('+%x' % off)
        rec = p; depth -= 1
        if p is not None: parts[-1] = '%s+%x' % (label(p), off)
    return ' <- '.join(parts)

def words(buf, rec):
    w = np.frombuffer(buf, dtype='<u4', count=rec[10], offset=rec[9]).copy()
    ptr = {}
    for i in range(rec[12]):
        wi, kind, target, toff = struct.unpack_from('<IBQI', buf, rec[11] + 17 * i)
        ptr[wi] = (kind, target, toff)
    return w, ptr

def render(nodes, p):
    kind, target, toff = p
    if kind == 1:
        t = nodes.get(target)
        return '->%s+%x#%04x' % (label(t) if t else '?', toff, target & 0xffff)
    return {2: '->(freed)', 3: '->stack%d-%x' % (target, toff)}.get(kind, '?')

def image_pages(path):   # an RXWI world image (boot.cpp write_world): page address -> 4096 bytes
    b = open(path, 'rb').read()
    magic, ver, n, sim = struct.unpack_from('<4I', b, 0)
    pages = {}
    for k in range(n):
        a, off, size = struct.unpack_from('<3I', b, 80 + 12 * k)
        pages[a] = bytes(4096) if size == 0 else b[off:off + 4096] if size == 4096 else zlib.decompress(b[off:off + size])
    return pages

def image_diff(pa, pb):   # --images: page-level differences of two world images (stacks included: dead below each thread's ESP)
    A, B = image_pages(pa), image_pages(pb)
    diff = [a for a in sorted(set(A) | set(B)) if A.get(a) != B.get(a)]
    words = sum(sum(1 for i in range(0, 4096, 4) if (A.get(a) or bytes(4096))[i:i + 4] != (B.get(a) or bytes(4096))[i:i + 4]) for a in diff)
    print('world images: %d / %d pages differ (%d words)%s' % (len(diff), len(A), words, ': ' + ' '.join('%08x' % a for a in diff[:24]) if diff else ''))
    return 0 if not diff else 1

def main():
    args = sys.argv[1:]
    if args and args[0] == '--images': return image_diff(args[1], args[2])
    opt = {'--classes': 40, '--examples': 3, '--class': None}
    files = []
    i = 0
    while i < len(args):
        if args[i] in opt: opt[args[i]] = args[i + 1] if args[i] == '--class' else int(args[i + 1]); i += 2
        else: files.append(args[i]); i += 1
    ba, A, GA = parse(files[0]); bb, B, GB = parse(files[1])
    per = collections.defaultdict(lambda: [0, 0, 0, 0, 0, 0])   # nodes A, B, identical, differ, only A, only B
    diff_examples = collections.defaultdict(list)
    same_bytes = all_bytes = 0
    for name, ra in A.items():
        la = label(ra); per[la][0] += 1
        rb = B.get(name)
        if rb is None: per[la][4] += 1; continue
        all_bytes += ra[5]
        if ra[8] == rb[8] and ra[5] == rb[5]: per[la][2] += 1; same_bytes += ra[5]
        else: per[la][3] += 1; diff_examples[la].append((ra, rb))
    for name, rb in B.items():
        lb = label(rb); per[lb][1] += 1
        if name not in A: per[lb][5] += 1
    tot = [sum(v[k] for v in per.values()) for k in range(6)]
    gar_a, gar_b = sum(GA.values()), sum(GB.values())
    gar_common = sum((GA & GB).values())
    identical = tot[3] == 0 and tot[4] == 0 and tot[5] == 0 and GA == GB
    print('A %s: %d named nodes, %d unreached' % (files[0], len(A), gar_a))
    print('B %s: %d named nodes, %d unreached' % (files[1], len(B), gar_b))
    print('named nodes: %d identical, %d differ, %d only in A, %d only in B; %.2f%% of the common bytes in identical nodes; '
          'unreached: %d of %d / %d match' % (tot[2], tot[3], tot[4], tot[5], 100.0 * same_bytes / max(1, all_bytes), gar_common, gar_a, gar_b))
    print('VERDICT: %s' % ('IDENTICAL' if identical else 'DIFFERENT'))
    if identical: return 0
    rows = sorted(per.items(), key=lambda kv: -(kv[1][3] + kv[1][4] + kv[1][5]))
    print('\n%-56s %8s %8s %8s %8s %7s %7s' % ('class', 'A', 'B', 'same', 'differ', 'onlyA', 'onlyB'))
    for cls, v in rows[:opt['--classes']]:
        if v[3] + v[4] + v[5] == 0: break
        print('%-56s %8d %8d %8d %8d %7d %7d' % (cls[:56], *v))
    show = [opt['--class']] if opt['--class'] else [c for c, v in rows[:8] if v[3]]
    for cls in show:
        ex = diff_examples.get(cls, [])
        if not ex: continue
        print('\n== %s: %d differing nodes, first %d' % (cls, len(ex), min(len(ex), opt['--examples'])))
        for ra, rb in ex[:opt['--examples']]:
            print('  node %016x size %x/%x  path %s' % (ra[2], ra[5], rb[5], path_of(A, ra)))
            wa, pa = words(ba, ra); wb, pb = words(bb, rb)
            n = min(len(wa), len(wb)); shown = 0
            for k in range(n):
                xa = render(A, pa[k]) if k in pa else '%08x' % wa[k]
                xb = render(B, pb[k]) if k in pb else '%08x' % wb[k]
                if xa != xb:
                    print('    +%04x  A %-34s B %s' % (4 * k, xa, xb)); shown += 1
                    if shown >= 8: print('    ...'); break
    return 1

if __name__ == '__main__':
    sys.exit(main())

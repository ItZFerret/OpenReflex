#!/usr/bin/env python3
"""cam_cmp.py <original world image>=<product dump> [...] [--show N] -- compares the replay cameras (MXReplayTower / ZipLine / Spline /
Animated camera, core/game/loop_cam_replay.c) the product builds from a level's scene with the ones in the original's world image of the
same level.

The original side: the director list of the event world ([0xa946b8] + 0x5a54 -> world + 0x21dd8, links +0x310) in a world image
(reflex_boot REFLEX_WORLD_OUT of a baton boot, testdata/boot/worlds/*-world.bin). The product side: the file stop_audit_run writes with
STOP_AUDIT_DIRDUMP=<file> (development build; the same list once the race runs). Lines "DIR <vtable> <entity id> <size> <words...>" and
"AREA <count> <words...>" (the ActivationArea list's data). Several image=dump pairs (levels) are compared together.
Pairing: by entity id. Compared word for word, except the words that hold addresses or per-run values: +0x10 (the scene), +0x18 (the
name), +0x20 / +0x24 (the tick timers: frames since the entity was made), +0x2d0 / +0x2d4 (the global camera list: ignored, its links depend on the order of every camera of the session), +0x310 / +0x314 (the
director list), +0x3e0 (+0x3e4 animated: the list data), +0x4a0 (spline: the sample block), +0x78 (the scene's sort key). A pointer that is
non-zero in one image and zero in the other is a difference. The fourth lane of an ActivationArea element is never written.
Stale words: the constructors never write some words (padding, the w lane of the vec3 slots); the product's zeroed arena leaves 0 and the
original's recycled heap blocks hold old bytes that differ from camera to camera. A word where the product is 0 in every camera and the
original's values are not all equal over the cameras of the base region (or of the class, past +0x3c0) of all the pairs together is "stale":
counted, not a difference. Exit 0 when every pair is identical and no camera is missing on either side."""
import os, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import build_map as bm

VT = {0x0095ab04: ('tower', 0x430), 0x0095abac: ('zipline', 0x450), 0x0095ac54: ('spline', 0x4b0), 0x0095acfc: ('animated', 0x450)}
PTR = {0x10, 0x18, 0x20, 0x24, 0x310, 0x314, 0x78}
IGNORE = {0x2d0, 0x2d4}   # the global camera list: its links depend on the order every camera of the session was made in

def load_orig(img):
    pages = bm.image_pages(img)
    def rd(addr):
        p = pages.get(addr & ~0xfff)
        return None if p is None else struct.unpack_from('<I', p, addr & 0xfff)[0]
    g = rd(0x00a946b8); w = rd(g + 0x5a54)
    out = []
    d = rd(w + 0x21dd8)
    while d:
        vt = rd(d)
        if vt not in VT: print('original: unexpected class %08x at %08x' % (vt, d)); break
        words = [rd(d + o) for o in range(0, VT[vt][1], 4)]
        lo = 0x3e4 if vt == 0x0095acfc else 0x3e0
        cnt, data = rd(d + lo + 4), rd(d + lo)
        area = [rd(data + 4 * k) for k in range(cnt * 4)] if data and cnt else []
        out.append((vt, rd(d + 0x14), words, cnt, area))
        d = rd(d + 0x310)
    return out

def load_prod(path):
    out = []
    lines = open(path).read().splitlines()
    for i in range(0, len(lines), 2):
        t = lines[i].split(); ar = lines[i + 1].split()
        out.append((int(t[1], 16), int(t[2]), [int(x, 16) for x in t[4:]], int(ar[1]), [int(x, 16) for x in ar[2:]]))
    return out

def main():
    a = sys.argv[1:]; show = int(a[a.index('--show') + 1]) if '--show' in a else 20
    pairs = [x for x in a if '=' in x]
    sets = []
    for pr in pairs:
        img, dump = pr.split('=', 1)
        o, p = load_orig(img), load_prod(dump)
        print('%s: original %d cameras (%s); product %d' % (os.path.basename(os.path.dirname(img)) or img, len(o), ', '.join('%s %d' % (n, sum(1 for c in o if c[0] == v)) for v, (n, _) in VT.items()), len(p)))
        sets.append((o, p))
    allo = [c for o, _ in sets for c in o]; allp = [c for _, p in sets for c in p]
    stale = {}
    base = set()   # the shared base (below the classes' own fields, 0x3c0): judged over all classes together
    for k in range(0x3c0 // 4):
        if all(q[2][k] == 0 for q in allp) and len({c[2][k] for c in allo}) > 1: base.add(k)
    for vt in VT:
        po = [c for c in allo if c[0] == vt]; pp = [c for c in allp if c[0] == vt]
        st = set(base)
        if po and pp:
            for k in range(0x3c0 // 4, VT[vt][1] // 4):
                if all(q[2][k] == 0 for q in pp) and len({c[2][k] for c in po}) > 1: st.add(k)
        stale[vt] = st
    nstale = ndiff = nwords = npair = shown = 0
    for (orig, prod) in sets:
        byid = {(c[0], c[1]): c for c in prod}
        for o in orig:
            p = byid.pop((o[0], o[1]), None)
            if p is None: print('MISSING in the product: %s id %d' % (VT[o[0]][0], o[1])); ndiff += 1; continue
            npair += 1
            lo = 0x3e4 if o[0] == 0x0095acfc else 0x3e0
            skip = set(PTR) | {lo}
            if o[0] == 0x0095ac54: skip.add(0x4a0)
            diffs = []
            for k, (x, y) in enumerate(zip(o[2], p[2])):
                off = 4 * k
                if off in IGNORE: continue
                if off in skip:
                    if (x == 0) != (y == 0): diffs.append((off, x, y))
                    continue
                if x != y:
                    if k in stale[o[0]]: nstale += 1; continue
                    diffs.append((off, x, y))
            if o[3] != p[3] or len(o[4]) != len(p[4]) or any(x != y and k % 4 != 3 for k, (x, y) in enumerate(zip(o[4], p[4]))):
                diffs.append(('area', o[3], p[3]))
            if diffs:
                ndiff += 1; nwords += len(diffs)
                if shown < show:
                    shown += 1
                    print('DIFF %s id %d: %s' % (VT[o[0]][0], o[1], ', '.join(('+%x %08x/%08x' % d) if d[0] != 'area' else 'area %s/%s' % (d[1], d[2]) for d in diffs[:12])))
        for k in byid: print('EXTRA in the product: %s id %d' % (VT[k[0]][0], k[1])); ndiff += 1
    print('pairs %d, with differences %d (%d words); %d stale words skipped' % (npair, ndiff, nwords, nstale))
    sys.exit(1 if ndiff else 0)

if __name__ == '__main__':
    main()

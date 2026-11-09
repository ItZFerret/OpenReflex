#!/usr/bin/env python3
"""build_ctors.py <classes.tsv> <boot-coverage.bin> [out.tsv] -- who constructs the classes a race reads (core/notes/construction.md).

<classes.tsv>: build_map.py's per-class read table (class, objects, read bytes). <boot-coverage.bin>: REFLEX_COVERAGE of the boot
that built the world (instruction counts per exe address). For every class: the functions of the decompile that store its vtable
(`Class::vftable` assignments: constructors, also destructors / base resets) that ran in the boot, and their callers that ran
(the factories / loaders: one and two levels up, from functions.tsv's call graph). Output: class, objects, read, ctors, creators.
"""
import collections, glob, os, re, struct, sys
R = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '_re', 'out')
funcs = []
for p in sorted(glob.glob(os.path.join(R, 'decomp', '*.c'))):
    txt = open(p, encoding='latin-1').read()
    parts = re.split(r'(?m)^// ([0-9a-f]{8}) (\S+)\n', txt)
    for i in range(1, len(parts), 3): funcs.append((int(parts[i], 16), parts[i + 1], parts[i + 2]))
funcs.sort()
name_of = {a: n for a, n, _ in funcs}
addr_of = {}
for a, n, _ in funcs: addr_of.setdefault(n, a)
size = {}; callees = collections.defaultdict(list)
for l in open(os.path.join(R, 'functions.tsv'), encoding='latin-1').readlines()[1:]:
    c = l.rstrip('\n').split('\t')
    a = int(c[0], 16); size[a] = int(c[3] or 0)
    for cn in (c[7].split(',') if len(c) > 7 and c[7] else []):
        cn = cn.strip()
        if cn in addr_of: callees[a].append(addr_of[cn])
callers = collections.defaultdict(set)
for a, cs in callees.items():
    for b in cs: callers[b].add(a)
cov = open(sys.argv[2], 'rb').read()
counts = memoryview(cov)[16:].cast('I')
def ran(a):
    i = a - 0x400000
    return 0 <= i < len(counts) and counts[i] > 0
vt_store = collections.defaultdict(set)
pat = re.compile(r'=\s*&?([A-Za-z_][\w:<>,@$ ]*?)::vftable(?:_o[0-9a-f]+)?\s*;')
for a, n, t in funcs:
    for m in pat.finditer(t): vt_store[m.group(1).strip()].add(a)
rows = []
for l in open(sys.argv[1]).readlines()[1:]:
    cls, objs, rd = l.rstrip('\n').split('\t')
    if cls == '(raw)': continue
    short = cls.split('::')[-1] if cls not in vt_store else cls
    cands = vt_store.get(cls) or vt_store.get(short) or set()
    ctors = sorted(a for a in cands if ran(a) and 'dtor' not in name_of.get(a, ''))
    up = collections.Counter()
    for a in ctors:
        for c in callers.get(a, ()):
            if ran(c) and c not in ctors: up[c] += 1
    rows.append((cls, int(objs), int(rd), ctors, [c for c, _ in up.most_common(6)]))
fmt = lambda a: '%08x %s' % (a, name_of.get(a, '?'))
out = open(sys.argv[3], 'w') if len(sys.argv) > 3 else sys.stdout
out.write('class\tobjects\tread\tctors\tcreators\n')
for cls, objs, rd, ctors, up in rows:
    out.write('%s\t%d\t%d\t%s\t%s\n' % (cls, objs, rd, '; '.join(fmt(a) for a in ctors[:6]), '; '.join(fmt(a) for a in up)))

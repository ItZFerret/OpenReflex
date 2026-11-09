#!/usr/bin/env python3
"""gridpair_cmp.py <product trace> <oracle trace> [--every N] [--json out] -- the paired ride (w-gridpair): the product's and the
original's core/tools/gridpair_line.inc lines, aligned on the countdown entry (k = 0: the first frame ending in activity state 1 after one
ending in 0), compared field by field.

Prints: the grid pass (the oracle's G line, the zone and the game LCG at k = 0 on both sides); the first k at which each field group
differs (bit for bit); per checkpoint the local bike's distance |dpos| (ft), |dv| (ft/s), the orientation angle between the two
quaternions (deg), |dw| (rad/s), the grip flags and the AI riders' mean / max position distance."""
import math, struct, sys, json

LABELS = {'air', 'first', 'c', 'w', 'q', 'flt', 'fade', 't', 'g', 'pos', 'v', 'rear', 'front', 'grip', 'gear', 'rpm'}
# w-parity4w: the four-wheel sims' L fields (gridpair_line.inc pair_fw_local): class, rider off, input block, bodies, wheels, spins, hash
LABELS |= {'cls', 'off', 'in', 'b', 'wh', 'spin', 'h'}

def f32(h):
    return struct.unpack('<f', struct.pack('<I', int(h, 16)))[0]

def parse(path):
    rows, grids = [], []
    for line in open(path):
        r = line.split()
        if not r: continue
        if r[0] == 'G': grids.append(line.strip()); continue
        if r[0] != 'P': continue
        d = {'frame': int(r[1]), 'st': int(r[3]), 'ms': int(r[5]), 'gt': r[7], 'gs': r[9], 'gc': int(r[11]), 'rs': r[13], 'zone': int(r[15])}
        i = r.index('L'); j = r.index('R')
        L = {}; key = None
        for t in r[i + 1:j]:
            if t == '-': break
            if t in LABELS: key = t; L[key] = []
            else: L[key].append(t)
        d['L'] = L
        R = {}
        cur = None
        for t in r[j + 1:]:
            if ':' in t:
                s, x = t.split(':'); cur = int(s); R[cur] = [x]
            else: R[cur].append(t)
        d['R'] = R
        rows.append(d)
    return rows, grids

def entry(rows):
    for n in range(1, len(rows)):
        if rows[n]['st'] == 1 and rows[n - 1]['st'] == 0: return n
    return None

def vec(L, k): return [f32(x) for x in L.get(k, [])]

def qangle(a, b):
    if len(a) != 4 or len(b) != 4: return float('nan')
    d = abs(sum(x * y for x, y in zip(a, b)))
    na = math.sqrt(sum(x * x for x in a)); nb = math.sqrt(sum(x * x for x in b))
    if na == 0 or nb == 0: return float('nan')
    return math.degrees(2 * math.acos(min(1.0, d / (na * nb))))

def dist(a, b): return math.sqrt(sum((x - y) ** 2 for x, y in zip(a, b))) if a and b else float('nan')

def main():
    a = sys.argv[1:]
    every = 60; js = None
    if '--every' in a: i = a.index('--every'); every = int(a[i + 1]); del a[i:i + 2]
    if '--json' in a: i = a.index('--json'); js = a[i + 1]; del a[i:i + 2]
    P, gp = parse(a[0]); O, go = parse(a[1])
    ep, eo = entry(P), entry(O)
    print(f'product: {len(P)} frames, countdown entry at frame {P[ep]["frame"] if ep is not None else None}')
    print(f'oracle:  {len(O)} frames, countdown entry at frame {O[eo]["frame"] if eo is not None else None}')
    for g in go: print('oracle grid pass:', g)
    for g in gp: print('product grid pass:', g)
    if ep is None or eo is None: print('no countdown entry on one side'); return 1
    for name, rows, e in (('product', P, ep), ('oracle', O, eo)):
        r = rows[e]; print(f'{name:8s} k=0: zone {r["zone"]} game LCG {r["gs"]} count {r["gc"]} CRT {r["rs"]} clock {r["gt"]}')
    n = min(len(P) - ep, len(O) - eo)
    groups = ['st', 'gt', 'gs', 'gc', 'rs', 'zone'] + sorted(LABELS) + ['R']
    first = {}
    out = []
    racing_k = None
    for k in range(n):
        p, o = P[ep + k], O[eo + k]
        if racing_k is None and p['st'] >= 2 and o['st'] >= 2: racing_k = k
        for g in groups:
            if g in first: continue
            if g in ('st', 'gt', 'gs', 'gc', 'rs', 'zone'): same = p[g] == o[g]
            elif g == 'R': same = p['R'] == o['R']
            else: same = p['L'].get(g) == o['L'].get(g)
            if not same: first[g] = k
        pp, op = vec(p['L'], 'pos'), vec(o['L'], 'pos')
        pv, ov = vec(p['L'], 'v'), vec(o['L'], 'v')
        rd = [dist([f32(x) for x in p['R'][s][:3]], [f32(x) for x in o['R'][s][:3]]) for s in p['R'] if s in o['R']]
        out.append({'k': k, 'st': (p['st'], o['st']), 'dpos': dist(pp, op), 'dv': dist(pv, ov),
                    'dq': qangle(vec(p['L'], 'q'), vec(o['L'], 'q')), 'dw': dist(vec(p['L'], 'w'), vec(o['L'], 'w')),
                    'speed': (math.sqrt(sum(x * x for x in pv)) if pv else float('nan'), math.sqrt(sum(x * x for x in ov)) if ov else float('nan')),
                    'grip': (p['L'].get('rear', [''])[1:3], o['L'].get('rear', [''])[1:3]),
                    'ai_mean': sum(rd) / len(rd) if rd else float('nan'), 'ai_max': max(rd) if rd else float('nan'),
                    'ai_same': sum(1 for s in p['R'] if s in o['R'] and p['R'][s] == o['R'][s]), 'ai_n': len(rd),
                    'ctl_same': p['L'].get('c') == o['L'].get('c')})
    print(f'aligned frames: {n}; first racing frame (both st >= 2): k={racing_k}')
    print('first differing k per field group (absent = identical over all aligned frames):')
    for g in groups:
        print(f'  {g:6s} {first.get(g, "-")}')
    print(' k     st(p/o)  |dpos| ft   |dv| ft/s   dq deg   |dw| r/s   speed p/o ft/s     ctl  AI identical  AI mean/max ft')
    ks = sorted(set(list(range(0, n, every)) + [n - 1] + [v for v in first.values()] + ([racing_k] if racing_k is not None else [])))
    for k in ks:
        r = out[k]
        print(f'{k:5d}  {r["st"][0]}/{r["st"][1]}  {r["dpos"]:10.4f}  {r["dv"]:10.4f}  {r["dq"]:8.3f}  {r["dw"]:9.4f}  '
              f'{r["speed"][0]:7.2f}/{r["speed"][1]:7.2f}  {"=" if r["ctl_same"] else "x":>3s}  {r["ai_same"]:2d}/{r["ai_n"]:2d}  '
              f'{r["ai_mean"]:9.3f}/{r["ai_max"]:9.3f}')
    if racing_k is not None:   # how long the two local bikes stay close after the gate drop
        print('after the gate drop (k = first racing frame): first frame with')
        for name, key, lims in (('|dpos| ft', 'dpos', (0.001, 0.01, 0.1, 1, 10)), ('|dv| ft/s', 'dv', (0.01, 0.1, 1, 10)),
                                ('dq deg', 'dq', (0.01, 0.1, 1, 10))):
            cells = []
            for lim in lims:
                hit = next((r['k'] for r in out[racing_k:] if r[key] > lim), None)
                cells.append(f'>{lim}: ' + (f'+{hit - racing_k} ({(hit - racing_k) / 60:.2f} s)' if hit is not None else 'never'))
            print(f'  {name:10s} ' + '   '.join(cells))
    if js: json.dump({'first': first, 'racing_k': racing_k, 'rows': out}, open(js, 'w'))
    return 0

if __name__ == '__main__':
    sys.exit(main())

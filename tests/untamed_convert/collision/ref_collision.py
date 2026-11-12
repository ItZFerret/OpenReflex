#!/usr/bin/env python3
"""Reference values for tests/untamed_convert/collision (the port of ux_collision.py), written by the Python reference itself.

    python -I -B ref_collision.py OUT_DIR [UX_PY_ROOT]

Writes OUT_DIR/qhull2d.json (2-D point sets and scipy.spatial.ConvexHull(P).vertices: the stage's own inputs, random sets in general
position, sets with exactly collinear points) and OUT_DIR/synthetic.json (cos / sin of the ring angles, default_rng(12345) values,
'%g' / '%.nf' strings, 3-D hull equations, convert_collis + check_source records of synthetic .collis files).
Nothing is written anywhere else: the reference tools are imported read-only (-B: no byte code).
"""
import json, os, struct, sys

out_dir = sys.argv[1]
root = sys.argv[2] if len(sys.argv) > 2 else os.environ['UX_PY_ROOT']
sys.path.insert(0, os.path.join(root, 'tools'))
import ux_common as U                     # first (pycache prefix); -B keeps it from writing anything
import numpy as np
import scipy.spatial as SS
import uxmesh
import ux_collision as C

os.makedirs(out_dir, exist_ok=True)


def hx(x):
    return struct.pack('>d', float(x)).hex()


def hxs(a):
    return [hx(x) for x in np.asarray(a, np.float64).reshape(-1)]


# ------------------------------------------------------------------------------------------------ the stage's own hull inputs
RealHull = SS.ConvexHull
calls = []


class Hull:
    def __init__(self, P):
        self.h = RealHull(P)
        calls.append((np.array(P), self.h))

    def __getattr__(self, k):
        return getattr(self.h, k)


SS.ConvexHull = Hull
real2d, real3d, seen = [], [], set()
for lv in U.LEVELS:
    d = U.read_json(U.work_dir(lv) / 'level.json')
    dfr = U.read_json(U.work_dir(lv) / 'level_fr.json', default=None)
    dyn = C.dynamic_sources(d, dfr)
    for cn, cr in sorted(d['closure']['collis'].items()):
        if cn not in dyn:
            continue
        data = (U.extract_dir(lv) / cr['file']).read_bytes()
        key = U.md5_bytes(data)
        dd = uxmesh.read_collis(data)
        if dd['kind'] != 0 or key in seen:
            continue
        seen.add(key)
        calls.clear()
        C.fit_prism_boxes(dd['tris'])
        first3 = True
        for P, h in calls:
            if P.shape[1] == 2:
                real2d.append(dict(P=hxs(P), v=[int(x) for x in h.vertices], src=cn))
            elif first3:
                real3d.append(dict(P=hxs(P), eq=hxs(h.equations), src=cn)); first3 = False
SS.ConvexHull = RealHull

# ------------------------------------------------------------------------------------------------ random 2-D sets
rs = np.random.RandomState(20261111)


def hull_case(P):
    P = np.ascontiguousarray(P, np.float64)
    try:
        h = RealHull(P)
    except Exception:
        return None
    return dict(P=hxs(P), v=[int(x) for x in h.vertices])


def dup_shuffle(P):
    rep = rs.randint(1, 4, len(P))
    Q = np.repeat(P, rep, 0)
    return Q[rs.permutation(len(Q))]


groups = {'real (the box-fit meshes of the 12 levels)': real2d}
g = []
for _ in range(600):
    n = rs.randint(3, 41)
    g.append(hull_case(rs.uniform(-5, 5, (n, 2))))
groups['general: uniform float64'] = [c for c in g if c]
g = []
for _ in range(600):
    n = rs.randint(3, 25)
    g.append(hull_case(dup_shuffle(np.float32(rs.uniform(-4, 4, (n, 2))).astype(np.float64))))
groups['general: float32 values, duplicated points'] = [c for c in g if c]
g = []
for _ in range(600):
    n = rs.randint(3, 31)
    a = np.sort(rs.uniform(0, 2 * np.pi, n)); r = rs.uniform(1.0, 4.0) * (1 + rs.uniform(-0.2, 0.2, n))
    P = np.stack([r * np.cos(a) + rs.uniform(-3, 3), r * np.sin(a) + rs.uniform(-3, 3)], 1)
    g.append(hull_case(dup_shuffle(np.float32(P).astype(np.float64))))
groups['general: polygons, duplicated points'] = [c for c in g if c]
g = []
for _ in range(400):
    n = rs.randint(4, 60)
    P = rs.standard_normal((n, 2)) * rs.uniform(0.1, 50)
    g.append(hull_case(P))
groups['general: gaussian float64'] = [c for c in g if c]
g = []
for _ in range(300):
    w, h = rs.uniform(0.5, 6, 2)
    corners = np.array([[-w, -h], [w, -h], [w, h], [-w, h]])
    extra = []
    for k in range(rs.randint(0, 6)):
        i = rs.randint(4); t = rs.randint(1, 4) / 4.0
        extra.append(corners[i] + (corners[(i + 1) % 4] - corners[i]) * t)
    P = np.concatenate([corners, np.array(extra).reshape(-1, 2)])
    g.append(hull_case(dup_shuffle(np.float32(P).astype(np.float64))))
groups['degenerate: rectangles with points on their edges'] = [c for c in g if c]
g = []
for _ in range(300):
    n = rs.randint(4, 30)
    g.append(hull_case(rs.randint(-3, 4, (n, 2)).astype(np.float64)))
groups['degenerate: integer grid points'] = [c for c in g if c]
rb = np.random.RandomState(99)                         # larger sets (their own stream: the groups above keep their points)
groups['general: 50-400 uniform points'] = [c for c in (hull_case(rb.uniform(-100, 100, (rb.randint(50, 400), 2))) for _ in range(300)) if c]
g = []
for _ in range(300):
    n = rb.randint(30, 300)
    a = rb.uniform(0, 2 * np.pi, n); r = 10 + rb.uniform(-0.01, 0.01, n)
    g.append(hull_case(np.stack([r * np.cos(a), r * np.sin(a)], 1)))
groups['general: near a circle (up to 300 hull vertices)'] = [c for c in g if c]
g = []
for _ in range(300):
    n = rb.randint(20, 200)
    P = np.float32(rb.standard_normal((n, 2)) * [5, 0.05]).astype(np.float64)
    Q = np.repeat(P, rb.randint(1, 3, n), 0)
    g.append(hull_case(Q[rb.permutation(len(Q))]))
groups['general: thin clouds, float32 values, duplicated points'] = [c for c in g if c]
g = []
for _ in range(300):
    n = rb.randint(3, 8)
    g.append(hull_case(np.float32(rb.uniform(-1, 1, (n, 2)) * 10.0 ** rb.randint(-3, 4)).astype(np.float64)))
groups['general: 3-7 points, scales 1e-3 to 1e3'] = [c for c in g if c]
json.dump(groups, open(os.path.join(out_dir, 'qhull2d.json'), 'w'))

# ------------------------------------------------------------------------------------------------ helper values
syn = {}
trig = []
sets = [2 * np.pi * np.arange(n) / n for n in (16, 64, 128)] + [np.pi * np.arange(n + 1) / n for n in (8, 32)] + [np.array([np.pi / 128])]
for a in sets:
    c, s = np.cos(a), np.sin(a)
    for i in range(len(a)):
        trig.append(['cos', hx(a[i]), hx(c[i])]); trig.append(['sin', hx(a[i]), hx(s[i])])
syn['trig'] = trig
syn['rng_random'] = hxs(np.random.default_rng(12345).random(200))
rng = np.random.default_rng(12345)
p = rs.uniform(0.01, 3.0, 37); p = p / p.sum()
syn['rng_choice'] = dict(p=hxs(p), k=[int(x) for x in rng.choice(len(p), 700, p=p)], after=hxs(rng.random(8)))
fm = []
vals = [64.0, 0.05, 0.25, 0.15, 5.0, 0.002, 0.0, -0.0, 1e-5, 123456.5, 999999.5, 9999995.0, 1e16, 0.0001, 0.00009999995, 2.5, 0.125, 0.0245, 90.7, 100 * 0.907,
        32.0, 1.0, 12345678.9, 0.5, 1.5, 2.675, 1e-7, 3.14159265358979, 31.872018814086914, 2.6702880859375e-05, 99.95, 0.9995, 0.024055, -0.0004, float('inf')]
vals += [float(x) for x in rs.standard_normal(60) * 10.0 ** rs.randint(-6, 7, 60)]
for v in vals:
    fm.append(['g', hx(v), 6, '%g' % v]); fm.append(['g', hx(v), 4, '%.4g' % v])
    for prec in (0, 1, 2, 3, 4):
        fm.append(['f', hx(v), prec, '%.*f' % (prec, v)])
syn['format'] = fm

# ------------------------------------------------------------------------------------------------ 3-D hulls
h3 = list(real3d)
for _ in range(40):
    n = rs.randint(5, 40)
    P = np.float32(rs.uniform(-3, 3, (n, 3))).astype(np.float64)
    h3.append(dict(P=hxs(P), eq=hxs(RealHull(P).equations)))
for _ in range(20):                                     # boxes with points on their faces and edges (coplanar sets)
    hw = rs.uniform(0.5, 4, 3)
    c = np.array([[sx * hw[0], sy * hw[1], sz * hw[2]] for sx in (-1, 1) for sy in (-1, 1) for sz in (-1, 1)])
    extra = [(c[rs.randint(8)] + c[rs.randint(8)]) / 2 for _ in range(rs.randint(0, 8))]
    P = np.concatenate([c, np.array(extra).reshape(-1, 3)])
    P = np.float32(np.repeat(P, 2, 0)).astype(np.float64)
    h3.append(dict(P=hxs(P), eq=hxs(RealHull(P).equations)))
syn['hull3d'] = h3


# ------------------------------------------------------------------------------------------------ synthetic .collis files
def k0(verts, tris, matrix=None):
    m = np.concatenate([np.eye(3), np.zeros((1, 3))]) if matrix is None else np.asarray(matrix, np.float64)
    o = struct.pack('>I4sIII', 1234, b'col\0', 3, 0, 0) + struct.pack('>12f', *m.reshape(-1))
    o += struct.pack('>5I', 0, len(verts), 0, len(tris), 0)
    o += b''.join(struct.pack('>3f', *[float(x) for x in v]) for v in verts)
    o += struct.pack('>I', 0)
    for a, b, c in tris:
        o += struct.pack('>f3f3H', 0.0, 0.0, 0.0, 0.0, a, b, c)
    return o


def prim(ty, m, prm, m1=0xFFFFFFFF, pad=0xCDCDCDCD):
    b = struct.pack('>I16fI', ty, *np.asarray(m, np.float64).reshape(-1), m1) + struct.pack('>%df' % len(prm), *prm)
    if ty == 5:
        b += struct.pack('>I', pad)
    return b


def k15(prims, tags=b''):
    return struct.pack('>I4sIIIII', 1234, b'col\0', 3, 15, 0, len(prims), len(tags)) + tags + b''.join(prims)


def rot_y(deg):
    a = np.radians(deg); c, s = np.cos(a), np.sin(a)
    return np.array([[c, 0, -s], [0, 1, 0], [s, 0, c]])


def prism(poly, y0, y1, R=None, T=(0, 0, 0)):
    """A convex polygon (CCW seen from +y, x-z plane) extruded along y: (verts, tris) with outward winding."""
    n = len(poly)
    v = [(x, y0, z) for x, z in poly] + [(x, y1, z) for x, z in poly]
    t = []
    for i in range(1, n - 1):
        t.append((n, n + i, n + i + 1)); t.append((0, i + 1, i))
    for i in range(n):
        j = (i + 1) % n
        t.append((i, j, n + j)); t.append((i, n + j, n + i))
    v = np.array(v, np.float64)
    tr = np.array(t)
    # outward: flip the triangles that face the centre
    cen = v.mean(0)
    out = []
    for a, b, c in tr:
        nrm = np.cross(v[b] - v[a], v[c] - v[a])
        out.append((a, b, c) if ((v[a] + v[b] + v[c]) / 3 - cen) @ nrm > 0 else (a, c, b))
    if R is not None:
        v = v @ np.asarray(R)
    return v + np.asarray(T), out


m_id = np.eye(4)
m_tr = np.eye(4); m_tr[3, :3] = (1, 2, 3)
m_rot = np.eye(4); m_rot[:3, :3] = rot_y(33.0) @ np.array([[1, 0, 0], [0, np.cos(0.4), np.sin(0.4)], [0, -np.sin(0.4), np.cos(0.4)]]); m_rot[3, :3] = (-4.5, 7.25, 12.125)
cases = []
cases.append(('k17', struct.pack('>I4sIIIff', 1234, b'col\0', 3, 17, 0, 1.12, 3.56), {}))
cases.append(('k19', struct.pack('>I4sIIII', 1234, b'col\0', 3, 19, 0, 2) + bytes(64), {}))
cases.append(('k15_tags_sphere_cyl_box', k15([prim(7, m_tr, (2.5,)), prim(8, m_tr, (1.0, 4.0)), prim(5, m_tr, (1.0, 2.0, 3.0))], b'soft'), {}))
cases.append(('k15_rotated', k15([prim(7, m_rot, (0.75,)), prim(8, m_rot, (1.3, 2.9)), prim(5, m_rot, (0.4, 2.0, 3.5))]), {}))
cases.append(('k15_type9', k15([prim(9, m_tr, (2.0, 1.25, 4.5)), prim(5, m_rot, (1.0, -2.0, 3.0))]), {}))
cases.append(('k15_type9', k15([prim(9, m_tr, (2.0, 1.25, 4.5)), prim(5, m_rot, (1.0, -2.0, 3.0))]), dict(type9='accept')))
cases.append(('k15_type9_small_bottom', k15([prim(9, m_rot, (0.5, 1.75, 2.0))]), dict(type9='accept')))
cases.append(('k15_negative_half', k15([prim(5, m_rot, (-1.5, 2.0, -0.25)), prim(8, m_id, (0.5, 1.0))]), {}))
cases.append(('k15_empty', k15([]), {}))
cases.append(('k15_version2', struct.pack('>I4sIIIII', 1234, b'col\0', 2, 15, 0, 0, 0), {}))
cases.append(('k15_unknown_type', k15([struct.pack('>I', 6) + bytes(84)]), {}))
cases.append(('k11', struct.pack('>I4sIII3fI', 1234, b'col\0', 3, 11, 0, 4.0688682, 0.9408225, 1.0625365, 0xCDCDCDCD), {}))
cases.append(('k11_negative', struct.pack('>I4sIII3fI', 1234, b'col\0', 3, 11, 0, -1.5, 0.25, -3.0, 0), dict(dynamic=True)))
cases.append(('k2', struct.pack('>I4sIII', 1234, b'col\0', 3, 2, 0) + struct.pack('>16f', *m_id.reshape(-1)) + struct.pack('>3I', 0, 0, 0), {}))
cases.append(('k7_unknown', struct.pack('>I4sIII', 1234, b'col\0', 3, 7, 0) + bytes(16), {}))
cases.append(('not_collis', b'\0' * 32, {}))
big = k0([(0, 0, 0), (0, 0, 200), (200, 0, 0), (1, 1, 1)], [(0, 1, 2), (0, 0, 1), (0, 1, 3)])
cases.append(('k0_big_triangle', big, {})); cases.append(('k0_big_triangle', big, dict(max_edge=0.0)))
cases.append(('k0_big_triangle', big, dict(max_edge=20.0)))
rv = rs.uniform(-60, 60, (40, 3)); rt = [tuple(int(x) for x in rs.choice(40, 3, replace=False)) for _ in range(120)]
cases.append(('k0_random_static', k0(rv, rt, np.concatenate([rot_y(20.0), [[10.0, -3.0, 5.5]]])), {}))
bv, bt = prism([(-2, -1), (2, -1), (2, 1), (-2, 1)], -0.75, 0.75)
cases.append(('k0_box', k0(bv, bt), {})); cases.append(('k0_box', k0(bv, bt), dict(dynamic=True)))
wv, wt = prism([(-3.5, -1.6), (3.5, -1.6), (3.5, 1.6), (-3.5, 0.14)], -1.0, 1.05, rot_y(27.0), (0.3, 0.1, -0.2))
cases.append(('k0_wedge_rotated', k0(wv, wt), dict(dynamic=True)))
hv, ht = prism([(2.3, 0.0), (1.2, 2.1), (-1.1, 2.0), (-2.4, 0.1), (-1.2, -2.0), (1.0, -2.2)], 0.0, 3.4, rot_y(-71.5))
cases.append(('k0_hexagon_rotated', k0(hv, ht), dict(dynamic=True)))
tv, tt = prism([(-2, -1), (2, -1), (2, 1), (-2, 1)], -0.75, 0.75, np.array([[1, 0, 0], [0, np.cos(0.5), np.sin(0.5)], [0, -np.sin(0.5), np.cos(0.5)]]))
cases.append(('k0_box_tilted_axis', k0(tv, tt), dict(dynamic=True)))
lv_, lt = prism([(-2, -2), (2, -2), (2, 0), (0, 0), (0, 2), (-2, 2)], 0.0, 1.0)             # an L: not convex
cases.append(('k0_L_shape', k0(lv_, lt), dict(dynamic=True)))
pv = [(-1, 0, -1), (1, 0, -1), (1, 0, 1), (-1, 0, 1), (0, 2, 0)]                             # a pyramid: not a prism
cases.append(('k0_pyramid', k0(pv, [(0, 1, 2), (0, 2, 3), (0, 4, 1), (1, 4, 2), (2, 4, 3), (3, 4, 0)]), dict(dynamic=True)))
cases.append(('k0_empty', k0([(0, 0, 0), (1, 0, 0)], [(0, 1, 1)]), dict(dynamic=True)))
src = []
for name, data, kw in cases:
    kw = dict(dict(type9='refuse', max_edge=64.0, dynamic=False), **kw)
    e = dict(name=name, data=data.hex(), **kw)
    try:
        rec, out, chk = C.convert_collis(data, name, **kw)
        if out is not None:
            pr, st = C.check_source(rec, out, chk)
            rec['check'] = dict(ok=not pr, problems=pr, **st)
        e.update(rec=json.loads(U.dumps_json(rec)), out=out.hex() if out is not None else None, error=None)
    except Exception as ex:
        e.update(rec=None, out=None, error=str(ex))
    src.append(e)
    print('%-28s %s' % (name, e['error'] or (e['rec']['action'], (e['rec'].get('check') or {}).get('problems'))))
syn['sources'] = src

# ------------------------------------------------------------------------------------------------ the pop clip's parts
m_low = np.eye(4); m_low[3, :3] = (0.5, 1.0, -0.25)
m_high = np.eye(4); m_high[3, :3] = (-2.0, 6.0, 1.0)
m_near = np.eye(4); m_near[:3, :3] += 4e-7; m_near[3, :3] = (0.0, 2.0, 0.0)          # within np.allclose of the identity
box = lambda m, h: dict(type=5, matrix=m, params=np.array(h, np.float64), m1=0xFFFFFFFF)
cyl = lambda m, r, h: dict(type=8, matrix=m, params=np.array([r, h], np.float64), m1=0xFFFFFFFF)
psets = {
    'sign boxes': [box(m_low, (7.0, 2.8, 0.2)), box(np.eye(4), (0.3, 0.5, 2.0)), box(m_high, (1.0, 0.4, 1.0))],
    'nearly aligned box': [box(m_near, (1.0, 2.0, 0.5)), box(np.eye(4), (2.0, 0.25, 2.0))],
    'rotated box': [box(m_rot, (1.0, 2.0, 3.0)), box(np.eye(4), (0.5, 0.5, 0.5))],
    'box and cylinder': [box(m_low, (1.0, 3.0, 1.0)), cyl(np.eye(4), 0.75, 2.0)],
}
clip, sunk = [], []
pdata = {k: C._write_prims15(v) for k, v in psets.items()}
for name, data in pdata.items():
    prims = C.parse_pc_collis(data)['prims']
    T = np.concatenate([C.prim_triangles(p) for p in prims])
    lo, hi = float(T[..., 1].min()), float(T[..., 1].max())
    ys = [lo - 1.0, lo + 1e-4, lo + 0.3 * (hi - lo), (lo + hi) / 2, hi - 1e-7, hi, hi + 1.0, 1.0, 1.0 + 5e-7, 5.6, 5.6 + 1e-6, 0.5, -0.5, 0.25]
    for cy in ys:
        e = dict(name=name, data=data.hex(), clip_y=hx(cy))
        try:
            np2, rep = C.clip_prims15(prims, cy)
            e.update(out=C._write_prims15(np2).hex(), rep=json.loads(U.dumps_json(rep)), error=None)
        except ValueError as ex:
            e.update(out=None, rep=None, error=str(ex))
        clip.append(e)
for lvname in ('Fairview_STADIUM', 'Norton_STADIUM', 'BridgePort_STADIUM', None):
    hf = C._hf_sampler(U.level(lvname)) if lvname else None
    for trial in range(8):
        name = list(pdata)[trial % len(pdata)]
        prims = C.parse_pc_collis(pdata[name])['prims']
        mats = []
        for _ in range(rs.randint(1, 5)):
            a, b = rs.uniform(-0.1, 0.1, 2); yaw = rs.uniform(0, 2 * np.pi)
            Rm = rot_y(np.degrees(yaw)) @ np.array([[1, 0, 0], [0, np.cos(a), np.sin(a)], [0, -np.sin(a), np.cos(a)]]) @ np.array([[np.cos(b), np.sin(b), 0], [-np.sin(b), np.cos(b), 0], [0, 0, 1]])
            M = np.eye(4); M[:3, :3] = Rm * rs.uniform(0.9, 1.2); M[3, :3] = (rs.uniform(-530, 530), rs.uniform(-5, 40), rs.uniform(-530, 530))
            mats.append([float(x) for x in np.float32(M).reshape(-1)])
        lift = float(rs.choice([10.0, 25.0, 66.0]))
        sunk.append(dict(level=lvname, data=pdata[name].hex(), mats=[hxs(m) for m in mats], lift=hx(lift), value=hx(C._sunk_excess(prims, mats, lift, hf))))
syn['clip'] = clip; syn['sunk'] = sunk
print('clip cases', len(clip), 'errors', sum(1 for c in clip if c['error']), ' sunk cases', len(sunk))
json.dump(syn, open(os.path.join(out_dir, 'synthetic.json'), 'w'))
print('qhull2d groups:', {k: len(v) for k, v in groups.items()})
print('hull3d sets:', len(h3), ' synthetic sources:', len(src))

# ------------------------------------------------------------------------------------------------ option variants of the 12 levels
# convert(..., write=False): the current tool's manifest in memory (nothing is written; every output's md5 and size is in it). The file
# holds the bytes ux_common.write_json would write as collision.json.
VARIANTS = (('refuse-popclip', dict(type9='refuse')),
            ('accept-nopopclip', dict(type9='accept', pop_clip='off')),
            ('accept-maxedge24', dict(type9='accept', max_edge=24.0)))
os.makedirs(os.path.join(out_dir, 'levels'), exist_ok=True)
for lv in U.LEVELS:
    for vname, kw in VARIANTS:
        man = C.convert(lv.disc, write=False, overlay=False, log=lambda *a: None, **kw)
        with open(os.path.join(out_dir, 'levels', '%s.%s.json' % (lv.disc, vname)), 'w', encoding='utf-8', newline='\n') as f:
            f.write(U.dumps_json(man))
    print('variants', lv.disc, man['totals']['by_action'], len(man['problems']), 'problems')

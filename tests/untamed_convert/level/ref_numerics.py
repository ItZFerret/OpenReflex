# tests/untamed_convert/level/ref_numerics.py <out dir> [level ...]
# Reference numbers for the level stage's numerics, unrounded (JSON floats are repr: exact):
#   <out dir>/numerics/<Level>.json   floor_y (ux_level.terrain_floor), yaw_deg (the free-ride start pose), entities [[id, rad, el95,
#                                     top]] = sky_dome_facts and placed_top_y of every merged ModelRSData the policy keeps
#   <out dir>/numerics/f32.json       [[float32 bits, float(str(np.float32))]]: numpy's shortest float32 text read back as a double
#   <out dir>/numerics/atan2.json     [[y, x, math.atan2(y, x), the correctly rounded arctan2]] (the last by decimal arithmetic)
#   <out dir>/numerics/fixed.json     [[x, digits, '%.*f' % (digits, x)]]
# Nothing is written outside <out dir>; run with python -I -B.
import json, math, os, random, struct, sys
from collections import OrderedDict
from decimal import Decimal, getcontext
sys.dont_write_bytecode = True
ROOT = os.environ['UX_PY_ROOT']
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import ux_common as U      # noqa: E402
import ux_level as L       # noqa: E402
import uxscene             # noqa: E402
import numpy as np         # noqa: E402

out = os.path.join(sys.argv[1], 'numerics')
os.makedirs(out, exist_ok=True)


def dump(name, obj):
    with open(os.path.join(out, name), 'w', encoding='utf-8') as f:
        json.dump(obj, f)


def level_numbers(name):
    lv = U.level(name); disc = lv.disc
    subs = uxscene.load(U.disc_files(lv)['scene'])
    by_short = OrderedDict((s.short(disc), s) for s in subs)
    champ = U.CHAMPIONSHIP_SUBSCENE[lv.mode]
    base, ch = by_short['Base'], by_short[champ]
    merged = uxscene.merged_entities([base, ch], disc)
    order = [('Base', base, True), (champ, ch, True)]
    if 'FreeRide' in by_short:
        order.append(('FreeRide', by_short['FreeRide'], False))
    order += [(sh, s, False) for sh, s in by_short.items() if sh not in ('Base', champ, 'FreeRide')]
    fr, _ = L.find_freeride_start(order)
    ix = L.ExtractIndex(lv)
    rows = []
    for eid, (sh, e) in merged.items():
        if e.type != 'ModelRSData' or L.classify(e.type, e.name, 'sx')[0] != 'keep' or not e.matrix:
            continue
        sa = e.props.get('SurfaceAsset')
        a = ix.get(sa.get('asset'), 'surface') if isinstance(sa, dict) else None
        if a is None:
            continue
        rad, el = L.sky_dome_facts(ix.path_of(a), e.matrix)
        rows.append([e.id, rad, el, L.placed_top_y(ix.path_of(a), e.matrix)])
    return dict(floor_y=L.terrain_floor(ix, merged), yaw_deg=fr['yaw_deg'] if fr else None, entities=rows)


def atan_dec(t):
    """arctan(t) for a Decimal 0 <= t <= 1 (argument halving, then the Taylor series)."""
    one = Decimal(1)
    for _ in range(6):
        t = t / (one + (one + t * t).sqrt())
    t2 = t * t; s = Decimal(0); p = t; n = 0
    while abs(p) > Decimal(10) ** -(getcontext().prec + 5):
        s += p / (2 * n + 1) if n % 2 == 0 else -p / (2 * n + 1)
        p *= t2; n += 1
    return s * 64


def atan2_exact(y, x, pi):
    """The correctly rounded arctan2 of two finite non-zero doubles (90 digits of decimal arithmetic, then one rounding)."""
    ax, ay = abs(Decimal(x)), abs(Decimal(y))
    a = atan_dec(ay / ax) if ay <= ax else pi / 2 - atan_dec(ax / ay)
    if x < 0:
        a = pi - a
    return float(-a if y < 0 else a)


def vectors():
    rnd = random.Random(20261111)
    f32 = []
    edge = [0x00000001, 0x00800000, 0x007FFFFF, 0x3F800000, 0x3F800001, 0x4B800000, 0x4B7FFFFF, 0x7F7FFFFF, 0x3DCCCCCD, 0x41200000,
            0x4CBEBC20, 0x5D5E0B6B, 0x0DA24260, 0x58635FA9, 0x4E6E6B28, 0x461C4000, 0x3A83126F, 0x38D1B717, 0x80000000, 0xBF000000]
    for b in edge + [rnd.getrandbits(32) for _ in range(60000)]:
        v = struct.unpack('<f', struct.pack('<I', b))[0]
        if not math.isfinite(v):
            continue
        f32.append([b, float(str(np.float32(v)))])
    # values with few digits (what scenes hold) and powers of two (the asymmetric rounding interval)
    for _ in range(20000):
        v = np.float32(round(rnd.uniform(-2000, 2000), rnd.randint(0, 6)))
        f32.append([struct.unpack('<I', struct.pack('<f', v))[0], float(str(v))])
    for e in range(-149, 128):
        v = np.float32(2.0 ** e)
        f32.append([struct.unpack('<I', struct.pack('<f', v))[0], float(str(v))])
    dump('f32.json', f32)

    getcontext().prec = 90
    pi = 16 * atan_dec(Decimal(1) / 5) - 4 * atan_dec(Decimal(1) / 239)
    at = []
    for k in range(6000):
        mode = k % 6
        if mode == 0:
            y, x = rnd.uniform(-1, 1), rnd.uniform(-1, 1)
        elif mode == 1:
            y, x = float(np.float32(rnd.uniform(-1, 1))), float(np.float32(rnd.uniform(-1, 1)))
        elif mode == 2:
            y, x = rnd.uniform(-20000, 20000), rnd.uniform(1e-6, 20000)
        elif mode == 3:
            y, x = rnd.uniform(-1, 1) * 10.0 ** rnd.randint(-30, 30), rnd.uniform(-1, 1) * 10.0 ** rnd.randint(-30, 30)
        elif mode == 4:
            a = rnd.uniform(-1, 1); y, x = a, a * (1 + rnd.uniform(-1e-9, 1e-9))
        else:
            y, x = rnd.uniform(-1e-12, 1e-12), rnd.uniform(-1, 1)
        if x == 0 or y == 0:
            continue
        at.append([y, x, math.atan2(y, x), atan2_exact(y, x, pi)])
    dump('atan2.json', at)

    fx = []
    for _ in range(4000):
        x = rnd.uniform(-1, 1) * 10.0 ** rnd.randint(-3, 6)
        for d in (0, 1, 2):
            fx.append([x, d, '%.*f' % (d, x)])
    for x in (0.5, 1.5, 2.5, -0.5, 0.05, 0.15, 0.25, 0.125, 1e22, -0.0, 0.0, 2.675, 1.005, 0.045, 999.5, 9.95, 99.995):
        for d in (0, 1, 2):
            fx.append([x, d, '%.*f' % (d, x)])
    dump('fixed.json', fx)


if not os.path.exists(os.path.join(out, 'fixed.json')) or os.environ.get('UX_REFS'):
    vectors()
    print('vectors', flush=True)
for name in sys.argv[2:] or [lv.disc for lv in U.LEVELS]:
    lv = U.level(name)
    d = level_numbers(lv.disc)
    dump(lv.disc + '.json', d)
    print(lv.disc, len(d['entities']), 'placed surfaces', flush=True)

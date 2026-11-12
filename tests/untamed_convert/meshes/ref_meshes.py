#!/usr/bin/env python3
"""tests/untamed_convert/meshes/ref_meshes.py <out json> [<reference root>]: digests of ux_meshes.convert_model for the variants the
default level run does not reach: tiny limits (static and crowd splits, bisected strips, _pN parts, baked cells in several parts),
explicit material lists, rigid='bound', collision bounds. Everything is converted in memory (convert_model writes nothing); the only
file written is <out json>. Run with the reference's Python: python.exe -I -B ref_meshes.py ..."""
import sys, os, json, hashlib
from collections import Counter
root = sys.argv[2] if len(sys.argv) > 2 else __import__('os').environ['UX_PY_ROOT']
sys.path.insert(0, root + '/tools')
import ux_common as U                                    # first (pycache prefix)
import ux_meshes as M

CASES = [
    # (level, [models], limits, rigid, materials: None | 'rev' (the default list reversed) | 'x' (other names))
    ('norton', ['Norton_TierA', 'Norton_railing', 'Norton_SupportTruss', 'NCrowdSec05', 'HayBale_Single', 'MX_Scoretower_01', 'Ambulance_NEW'], (300, 600, 24), 'all', None),
    ('norton', ['Norton_TierA', 'NCrowdSec05', 'Ambulance_NEW'], (64, 96, 5), 'all', 'x'),
    ('foothills', ['FHCrowdSec11', 'FHCrowdSec01', 'Foot_Ypost', 'Foothills_SXAds'], (40, 70, 4), 'all', None),
    ('harrispark', ['HPCrowdSec73', 'Harrispark_ExitS1', 'Harrispark_Jumbo'], (1000, 9600, 24), 'bound', 'rev'),
    ('bridgeport', ['Bridgeport_Tier01Radius_Baked'], (500, 900, 3), 'all', None),
    ('capitalcity', ['Capitalcity_Armrailing'], (4800, 9600, 24), 'all', 'x'),
    ('fairview', ['YellowPillon'], (4800, 9600, 24), 'bound', None),
    ('lincoln', ['YellowPillon'], (4800, 9600, 24), 'all', 'rev'),
]


def md5(b):
    return hashlib.md5(b).hexdigest()


def main():
    out = []
    for lvn, names, lim, rigid, mats in CASES:
        lv = U.level(lvn)
        cm = M.level_data(lv)[1]['closure']['model']
        # every rigid multi-node / skinned model of the level's extract too (rigid rule, skip reasons)
        todo = [(n, True) for n in names]
        if rigid == 'bound':
            import uxmesh
            for p in sorted((U.EXTRACT / lv.disc / 'model').glob('*.model')):
                try:
                    m = uxmesh.read_model(p.read_bytes())
                except Exception:
                    continue
                if m.skinned and m.boned and '@' not in p.stem and p.stem not in names:
                    todo.append((p.stem, False))
        for name, in_closure in todo:
            key = M._ci(cm, name)
            cb = bool(cm[key].get('with_collision')) if key else False
            cbounds = M.collision_bounds(lv, key) if cb else None
            materials = None
            if mats and key:
                d = list(cm[key].get('default_materials') or [])
                materials = list(reversed(d)) if mats == 'rev' else ['UXT_%d_%s' % (i, s) for i, s in enumerate(d)]
            fixes = Counter()
            try:
                r = M.convert_model(lv, key or name, cb, cbounds, limits=lim, fixes=fixes, materials=materials, rigid=rigid)
            except Exception as ex:                                     # noqa: BLE001
                out.append(dict(level=lv.disc, model=key or name, limits=list(lim), rigid=rigid, materials=materials, collision=cb,
                                bounds=cbounds is not None, error=str(ex)))
                continue
            parts = []
            for p in r['parts']:
                sb, mb = p.pop('surface_bytes'), p.pop('model_bytes')
                parts.append(dict(part=p['part'], surface_md5=md5(sb), model_md5=md5(mb), json=p))
            info = dict(r['info'])
            uv = info.pop('uv', [])
            out.append(dict(level=lv.disc, model=key or name, limits=list(lim), rigid=rigid, materials=materials, collision=cb,
                            bounds=cbounds is not None, skipped=r['skipped'], parts=parts, info=info, uv=uv, fixes=dict(fixes)))
            print(lv.short, key or name, lim, rigid, 'skipped: %s' % r['skipped'] if r['skipped'] else '%d parts' % len(parts))
    with open(sys.argv[1], 'w', encoding='utf-8') as f:
        f.write(U.dumps_json(dict(cases=out)))
    negative(os.path.dirname(os.path.abspath(sys.argv[1])))


def negative(ref_dir):
    """The read-back checks on damaged output: a copy of the reference's Norton part files in <ref dir>/neg/<Level>/meshes with six
    files damaged, and ux_meshes.verify_level's problems and statistics for that folder (-> <ref dir>/neg.json). The port's
    VerifyLevel must report the same problems in the same words."""
    import shutil, struct
    from pathlib import Path
    lv = U.level('norton')
    src = U.WORK / lv.disc / 'meshes'
    top = Path(ref_dir) / 'neg'
    dst = top / lv.disc / 'meshes'
    if top.exists():
        shutil.rmtree(top)                                 # below the scratch folder given on the command line
    dst.mkdir(parents=True)
    for f in src.iterdir():
        if f.suffix in ('.surface', '.model') or f.name == 'meshes.json':
            shutil.copyfile(f, dst / f.name)
    meshes = json.loads((dst / 'meshes.json').read_text(encoding='utf-8'))
    done = []

    def edit(name, fn):
        b = bytearray((dst / name).read_bytes())
        b = fn(b) or b
        (dst / name).write_bytes(bytes(b))
        done.append(name)

    def vertex0(b):                                        # offset of group 0's first vertex
        ng = struct.unpack_from('<I', b, 8)[0]
        return 16 + ng * 0x160 + struct.unpack_from('<I', b, 16 + 20)[0]

    def zero_normal(b):
        struct.pack_into('<3f', b, vertex0(b) + 8, 0.0, 0.0, 0.0)

    def swap_strip(b):                                     # the first two different neighbours of group 0's strip change places
        ng = struct.unpack_from('<I', b, 8)[0]
        q = 16 + ng * 0x160
        while b[q:q + 2] == b[q + 2:q + 4]:
            q += 2
        b[q:q + 2], b[q + 2:q + 4] = bytes(b[q + 2:q + 4]), bytes(b[q:q + 2])

    def nudge_pos(b):
        b[vertex0(b)] ^= 1

    def bad_bounds(b):
        r = struct.unpack_from('<f', b, 0xF4)[0]
        struct.pack_into('<f', b, 0xF4, r * 2 + 1)
        c = struct.unpack_from('<f', b, 0xF8)[0]
        struct.pack_into('<f', b, 0xF8, c + 1000.0)

    cell = next(p['files']['surface'] for k, v in meshes.items() if k != 'skipped' for p in v if p['kind'] == 'cell')
    edit('Ambulance_NEW.surface', zero_normal)
    edit('HayBale_Single.model', lambda b: struct.pack_into('<I', b, 0xA8, 1234))
    edit('NCrowdSec05.surface', swap_strip)
    edit(cell, nudge_pos)
    edit('CameraBase.surface', lambda b: b + bytes(4))
    edit('MX_Scoretower_01.model', bad_bounds)
    probs, stats = M.verify_level(lv, out_dir=dst)
    (Path(ref_dir) / 'neg.json').write_text(U.dumps_json(dict(level=lv.disc, folder='neg/%s/meshes' % lv.disc, damaged=done, problems=probs,
                                                              stats=stats)), encoding='utf-8')
    print('negative: %d files damaged, %d problems' % (len(done), len(probs)))
    for x in probs:
        print('   ', x)


if __name__ == '__main__':
    main()

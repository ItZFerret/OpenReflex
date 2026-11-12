#!/usr/bin/env python3
"""Reference digest of ux_shaderprog.py over every Untamed .shader of the 12 levels (read-only on the reference; run with -I -B).

    python.exe -I -B ref_shaderprog.py <out dir> [<untamed root>] [level ...]

Writes <out dir>/shaderprog/<Level>.json: {shader file name: {program name: {kind, lines, md5 (of the listing), pixel, target, samplers,
floats, alpha_sampler, literals, alpha_output, albedo, fetch_uv[0..7], composite[0..3], fetch_dest[0..3]}}} (or {'error': true} for a
file the module cannot read). The C# test (tests/untamed_convert/materials/Program.cs, 'shaders') builds the same document from the disc.
"""
import hashlib, json, sys
from pathlib import Path

out = Path(sys.argv[1])
root = Path(sys.argv[2]) if len(sys.argv) > 2 else Path(__import__('os').environ['UX_PY_ROOT'])
sys.path.insert(0, str(root / 'tools'))
sys.dont_write_bytecode = True
import ux_shaderprog as SP  # noqa: E402

LEVELS = ['Norton', 'Capital_City', 'Columbus', 'Denton', 'HarrisPark', 'Foothills', 'WestStone', 'Jefferson', 'Wilford', 'BridgePort',
          'Fairview', 'Lincoln']
levels = sys.argv[3:] or [n + '_STADIUM' for n in LEVELS]


def digest(p):
    data = p.read_bytes()
    ent = {}
    rs = SP.read_shader(p)
    for pr in SP.programs(data):
        lines = SP.disassemble(pr['blob'])
        ent[pr['name']] = dict(kind=pr['kind'], lines=len(lines), md5=hashlib.md5('\n'.join(lines).encode('utf-8')).hexdigest())
    for name, e in rs.items():
        j = ent[name]
        j.update(pixel=e['pixel'], target=e['target'], samplers={str(k): v for k, v in e['samplers'].items()}, floats=e['floats'])
        if not e['pixel']:
            continue
        j['alpha_sampler'] = 'error' if 'alpha_sampler_error' in e else e['alpha_sampler']
        j['literals'] = 'error' if 'literals_error' in e else {str(k): v for k, v in e['literals'].items()}
        try:
            info = SP.shader_info(p, name)
        except Exception:                                   # noqa: BLE001
            j['info'] = 'error'; continue
        if info is None:
            j['info'] = None; continue
        try:
            ao = SP.alpha_output(info)
            if ao is not None and ao['kind'] == 'other':
                ao = dict(kind='other', raw=ao['raw'])
            j['alpha_output'] = ao
        except Exception:                                   # noqa: BLE001
            j['alpha_output'] = 'error'
        try:
            al = SP.albedo_scale(info, ())
            j['albedo'] = dict(rgb=al['rgb'], how=al['how'], fog_only=al['fog_only'])
        except Exception:                                   # noqa: BLE001
            j['albedo'] = 'error'
        uv = []
        for k in range(8):
            try:
                kind, f = SP.fetch_uv(info, k)
                uv.append(None if kind is None else [kind, None if f['terms'] is None else [list(t) for t in f['terms']], f['coord']])
            except Exception:                               # noqa: BLE001
                uv.append('error')
        j['fetch_uv'] = uv
        comp, dest = [], []
        for k in range(4):
            try:
                c = SP.analyse(info['data'], k, info['literals'])['composite']
                comp.append(None if c is None else dict(bg=c['bg'], k=c['k'], lerp=c['lerp'], lines=c['lines'], program=c['program']))
            except Exception:                               # noqa: BLE001
                comp.append('error')
            try:
                d = None
                for I in info['ins']:
                    if I['op'] == 'tfetch' and I['parts'][-1].split()[0] == 'tf%d' % k:
                        d = SP.dreg(I['parts'][0])[1]; break
                dest.append(d)
            except Exception:                               # noqa: BLE001
                dest.append('error')
        j['composite'] = comp; j['fetch_dest'] = dest
    return ent


(out / 'shaderprog').mkdir(parents=True, exist_ok=True)
for lv in levels:
    doc = {}
    for p in sorted((root / 'extract' / lv / 'shader').glob('*.shader')):
        try:
            doc[p.name] = digest(p)
        except Exception:                                   # noqa: BLE001
            doc[p.name] = {'error': True}
    (out / 'shaderprog' / (lv + '.json')).write_text(json.dumps(doc, indent=None, sort_keys=True), encoding='utf-8')
    print(lv, len(doc), 'shaders', sum(len(v) for v in doc.values()), 'programs', flush=True)

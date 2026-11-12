#!/usr/bin/env python3
"""A fresh, in-memory run of the reference's CURRENT ux_materials.py (write=False: nothing is written below the reference; run with
-I -B), saved to the scratch folder: the stored work/<Level>/materials/ can be older than its inputs, and holds the default switches only.

    python.exe -I -B ref_materials.py <out dir> [<untamed root>] [--variants] [level ...]

Writes <out dir>/materials-fresh/<Level>/: materials.json (ux_common.dumps_json of the manifest), <asset>.material,
textures/<name>.texture (the stock and generated ones the run made) and shaders/<Template>.shader, i.e. what convert(write=True)
would write to work/<Level>/materials/ except the preview sheet.
--variants: instead, the 16 switch sets of ux_materials.selftest (every option at its other values) into
<out dir>/materials-variants/<tag>/<Level>/ (tag = Opts.tag(), the folder name the tool itself would use: materials-<tag>).
"""
import sys
from collections import OrderedDict
from pathlib import Path

args = [a for a in sys.argv[1:] if a != '--variants']
variants = '--variants' in sys.argv
out = Path(args[0])
root = Path(args[1]) if len(args) > 1 else Path(__import__('os').environ['UX_PY_ROOT'])
sys.dont_write_bytecode = True
sys.path.insert(0, str(root / 'tools'))
import ux_common as U  # noqa: E402
import ux_materials as X  # noqa: E402

VAR = OrderedDict([('prelit_off', dict(prelit_off=True)), ('nrm_off', dict(nrm_off=True)),
                   ('crowd_off', dict(crowd_off=True)), ('alpha_diffuse_alpha', dict(alpha='diffuse_alpha')),
                   ('alpha_opaque', dict(alpha='opaque')), ('alpha_drop', dict(alpha='drop')),
                   ('blend_test', dict(blend='test')), ('bale_gray', dict(bale_base='gray')), ('bale_white', dict(bale_base='white')),
                   ('bale_logo', dict(bale_base='logo')), ('mip_keep', dict(mipfilter='keep')),
                   ('composite_off', dict(composite='off')), ('albedo_default', dict(literal_albedo='default')),
                   ('alphasrc_diffuse', dict(alpha_source='diffuse')), ('pick_order', dict(diffuse_pick='order')),
                   ('program_off', dict(composite='off', literal_albedo='default', alpha_source='diffuse', diffuse_pick='order')),
                   ('safe', dict(alpha='drop', prelit_off=True, nrm_off=True, crowd_off=True))])


def save(d, man, blobs):
    (d / 'shaders').mkdir(parents=True, exist_ok=True)
    (d / 'textures').mkdir(parents=True, exist_ok=True)
    n = 0
    for e in X.entries(man).values():
        if e['action'] == 'convert':
            (d / e['file']).write_bytes(blobs[e['asset']]); n += 1
    for key, data in blobs.items():
        if key.startswith('textures/'):
            (d / key).write_bytes(data); n += 1
    for T in man['shaders_needed']:
        (d / 'shaders' / ('%s.shader' % T)).write_bytes(X.lib()[T]['shader']); n += 1
    (d / 'materials.json').write_bytes(U.dumps_json(man).encode('utf-8'))
    return n + 1


levels = args[2:] or [lv.disc for lv in U.LEVELS]
for name in levels:
    lv = U.level(name)
    if not variants:
        man, blobs = X.convert(lv.disc, write=False, sheet=False, return_bytes=True, log=lambda *a: None)
        n = save(out / 'materials-fresh' / lv.disc, man, blobs)
        print(lv.disc, n, 'files,', len(man['warnings']), 'warnings,', len(man['problems']), 'problems', flush=True)
        continue
    for vn, kw in VAR.items():
        man, blobs = X.convert(lv.disc, write=False, sheet=False, return_bytes=True, log=lambda *a: None, **kw)
        tag = X.Opts(**man['options']).tag()
        n = save(out / 'materials-variants' / tag / lv.disc, man, blobs)
        print(lv.disc, vn, tag, n, 'files,', dict(man['actions']), len(man['problems']), 'problems', flush=True)

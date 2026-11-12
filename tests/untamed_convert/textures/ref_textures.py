# tests/untamed_convert/textures/ref_textures.py <out dir>
# Reference digests for the parts of ux_textures.py that the stage outputs in work/<Level>/textures/ do not cover, computed with the
# CURRENT Python code (nothing is written outside <out dir>: the Python stage itself is only called with write=False):
#   <out>/tools.json                 md5 of ux_textures.py and uxtex.py, numpy / Pillow versions
#   <out>/generated/<Level>.json     ux_textures.generated_texture for every recipe of work/<Level>/materials/materials.json
#                                    (generated_textures with a 'recipe'): md5, size, info
#   <out>/texmax/<Level>-<tag>.json  the manifest of convert(level, tex_max=N, npot=..., write=False, sheet=False)
#   <out>/rgba/<Level>.json          md5 of pc_rgba(file, entry) for the written textures of work/<Level>/textures
#   <out>/grey.json                  grey_dxn_facts of every DXN closure texture (exact float hex of the unrounded values is not
#                                    available from the tool; the rounded facts and the decision are what it returns)
#   $UX_PYTHON -I -B ref_textures.py <scratch>/ref
import sys, os, json, hashlib
sys.dont_write_bytecode = True
ROOT = os.environ['UX_PY_ROOT']
sys.path.insert(0, ROOT + '/tools')
import ux_common as U
import numpy as np
import PIL
import uxtex
import ux_textures as TX

out = sys.argv[1]
md5 = lambda b: hashlib.md5(b).hexdigest()
for d in ('generated', 'texmax', 'rgba'):
    os.makedirs(os.path.join(out, d), exist_ok=True)


def dump(path, obj):
    with open(os.path.join(out, path), 'w', encoding='utf-8', newline='\n') as f:
        f.write(U.dumps_json(obj))


dump('tools.json', dict(tool_md5=U.md5_file(TX.__file__), uxtex_md5=U.md5_file(uxtex.__file__), numpy=np.__version__, pillow=PIL.__version__))

# ---- generated textures (ux_materials calls generated_texture with the closure texture's bytes)
for lv in U.LEVELS:
    L = U.read_json(U.work_dir(lv) / 'level.json')
    clos = {k.lower(): r for k, r in L['closure']['texture'].items()}
    M = U.read_json(U.work_dir(lv) / 'materials' / 'materials.json')
    res = {}
    for name, g in sorted((M.get('generated_textures') or {}).items()):
        rec = g.get('recipe') if isinstance(g, dict) else None
        if not rec:
            continue
        src = None; f = None
        if rec.get('source'):
            f = clos[rec['source'].lower()]['file']
            src = (U.EXTRACT / lv.disc / f).read_bytes()
        data, info = TX.generated_texture(src, rec)
        res[name] = dict(recipe=rec, source_file=f, md5=md5(data), size=len(data), info=info, stored_md5=g.get('md5'))
    dump('generated/%s.json' % lv.disc, res)
    print('generated', lv.disc, len(res), sum(1 for r in res.values() if r['md5'] != r['stored_md5']), 'differ from materials.json', flush=True)

# ---- --tex-max / --npot (no current build uses them)
for level, tex_max, npot in (('HarrisPark_STADIUM', 256, 'nearest'), ('HarrisPark_STADIUM', 64, 'floor'), ('Capital_City_STADIUM', 64, 'nearest'),
                             ('BridgePort_STADIUM', 32, 'nearest'), ('Lincoln_STADIUM', 1, 'nearest')):
    man = TX.convert(level, tex_max=tex_max, npot=npot, write=False, sheet=False, log=lambda *a: None)
    dump('texmax/%s-max%d-%s.json' % (level, tex_max, npot), man)
    print('texmax', level, tex_max, npot, man['totals']['written'], len(man['problems']), flush=True)

# ---- pc_rgba (the PC decoder) on the stage's own outputs
for level in ('Norton_STADIUM', 'Capital_City_STADIUM', 'BridgePort_STADIUM'):
    lv = U.level(level)
    od = TX.out_dir_for(lv)
    man = U.read_json(od / 'textures.json')
    res = {}
    for name, r in sorted(man['textures'].items()):
        if not r.get('file'):
            continue
        data = (od / r['file']).read_bytes()
        t = uxtex.PcTexture(data)
        ents = sorted({min(len(t.entries) - 1, e) for e in (2, 5, len(t.entries) - 1)} | ({0} if r['w'] * r['h'] <= 256 * 256 else set()))
        res[r['file']] = [[e, md5(np.ascontiguousarray(TX.pc_rgba(data, e)).tobytes())] for e in ents]
    dump('rgba/%s.json' % level, res)
    print('rgba', level, len(res), flush=True)

# ---- grey_dxn_facts
res = {}
for lv in U.LEVELS:
    L = U.read_json(U.work_dir(lv) / 'level.json')
    for name, r in sorted(L['closure']['texture'].items()):
        if r.get('format') != 'DXN':
            continue
        res['%s/%s' % (lv.disc, r['file'])] = TX.grey_dxn_facts((U.EXTRACT / lv.disc / r['file']).read_bytes())
dump('grey.json', res)
print('grey', len(res), sum(1 for v in res.values() if v and v['grey']), flush=True)

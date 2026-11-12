# tests/untamed_convert/level/ref_level.py <out dir> [level ...]
# The reference of the level stage: what the CURRENT reference's ux_level.py (build_level) writes for every level and kind, dumped
# with ux_common.dumps_json (= U.write_json) into
#   <out dir>/<Level>/level.json, level_fr.json          rigid_models 'static' (the default of ux_build_track and of every current build)
#   <out dir>/rigid-drop/<Level>/level.json              kind sx with rigid_models 'drop' (the legacy option), for the levels in RIGID_DROP
# Nothing is written outside <out dir> (the tools are imported from UX_PY_ROOT/tools, read only; run with python -I -B).
import os, sys
sys.dont_write_bytecode = True
ROOT = os.environ['UX_PY_ROOT']
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import ux_common as U      # noqa: E402
import ux_level as L       # noqa: E402

RIGID_DROP = ('Norton_STADIUM', 'HarrisPark_STADIUM', 'Lincoln_STADIUM')    # no rigid model; Harrispark_Jumbo; YellowPillon


def dump(folder, name, d):
    os.makedirs(folder, exist_ok=True)
    with open(os.path.join(folder, name), 'wb') as f:
        f.write(U.dumps_json(d).encode('utf-8'))


out = sys.argv[1]
levels = sys.argv[2:] or [lv.disc for lv in U.LEVELS]
for name in levels:
    lv = U.level(name)
    for kind in ('sx', 'fr'):
        d = L.build_level(lv.disc, kind)
        dump(os.path.join(out, lv.disc), 'level.json' if kind == 'sx' else 'level_fr.json', d)
        print(lv.disc, kind, len(d['entities']), 'entities', len(d['problems']), 'problems', flush=True)
    if lv.disc in RIGID_DROP:
        d = L.build_level(lv.disc, 'sx', rigid_models='drop')
        dump(os.path.join(out, 'rigid-drop', lv.disc), 'level.json', d)
        print(lv.disc, 'sx rigid_models drop', len(d['entities']), 'entities', len(d['problems']), 'problems', flush=True)

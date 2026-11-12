# tests/untamed_convert/race/ref_race.py <out dir> [--levels a,b] [--no-variants] [--only-extras]
# Writes the reference output of the "race" stage (S8) the way the CURRENT builds run it: ux_build_track calls
# ux_race.build_race in memory with the stock clone base (ux_race.Template(templates/stock-SX_FortDodge)), the base default
# --dropgate-materials template and each track's own options from work/combined/commands.json (--bale-edge-clear FT,
# --startgate-visual hide). work/<Level>/race.json of the reference is older (the ux_race.py CLI on the sketch base), so the
# files are written here exactly as ux_race.run_level(write=True, plot=False) writes them:
#   <out>/work/<Level>/race.json                 U.dumps_json of the race dict (+ 'assets', tdf 'file')
#   <out>/work/<Level>/race/<slot>-sx.tdf        the tdf asset
#   <out>/work/<Level>/race/<name>.<type>        the start-gate patch assets (UX_StartGate.model / .surface, UX_StartGate_COL.collis)
#   <out>/options.json                           the options each level was run with (the C# test checks its own table against them)
#   <out>/variants/<name>/<Level>/...            the same for other option values (the options no current build uses), listed in
#   <out>/variants.json                          {name: {level, options}}
#   <out>/lappacing/<Level>.bxml, .json          lappacing_to_reflex.to_reflex + engine_check of every level's lap pacing file
#   <out>/startpack/<Level>.json                 ux_startpack: the class report and the reseat map (clear 4, reseat 6) of every level
# Read-only on the reference (nothing is written there; byte code is not written); run with
#   $UX_PYTHON -I -B ref_race.py <scratch>/ref
import sys, os, json, shlex
sys.dont_write_bytecode = True
ROOT = os.environ['UX_PY_ROOT']
sys.path.insert(0, ROOT + '/tools')
from collections import OrderedDict
from pathlib import Path
import ux_common as U
import ux_race as R
import ux_base as UB
import ux_entities as UE
import ux_startpack as SP
import lappacing_to_reflex as LP

# other option values on single levels: every option of UxRaceOptions away from the current builds' value once
VARIANTS = OrderedDict([
    ('norton-hide', ('Norton_STADIUM', dict(startgate_visual='hide'))),
    ('foothills-patch-off', ('Foothills_STADIUM', dict(gate_patch='off'))),
    ('foothills-keep', ('Foothills_STADIUM', dict(startgate_visual='keep'))),
    ('jefferson-deck-auto', ('Jefferson_STADIUM', dict(deck_respawn='auto'))),
    ('bridgeport-deck-off', ('BridgePort_STADIUM', dict(deck_respawn='off'))),
    ('norton-minimal', ('Norton_STADIUM', dict(minimal=True))),
    ('norton-sx195', ('Norton_STADIUM', dict(sx_flags=195))),
    ('norton-pad-keep', ('Norton_STADIUM', dict(tdf_pad='keep'))),
    ('norton-stock9', ('Norton_STADIUM', dict(dropgate_materials='stock9'))),
    ('norton-lappacing', ('Norton_STADIUM', dict(lappacing='untamed'))),
    ('fairview-lappacing', ('Fairview_STADIUM', dict(lappacing='untamed'))),
    ('columbus-no-board-flip', ('Columbus_STADIUM', dict(board_flip=False))),
    ('lincoln-clear4', ('Lincoln_STADIUM', dict(bale_edge_clear=4.0))),
    ('denton-clear0', ('Denton_STADIUM', dict(bale_edge_clear=0.0))),
    ('denton-clear8', ('Denton_STADIUM', dict(bale_edge_clear=8.0))),
])


def track_options():
    """{level disc: dict(bale_edge_clear, startgate_visual)} from the current build commands."""
    out = OrderedDict()
    cmds = json.loads((Path(ROOT) / 'work' / 'combined' / 'commands.json').read_text(encoding='utf-8'))
    for name, steps in cmds.items():
        if name.startswith('_'):
            continue
        a = shlex.split(steps[0])
        lv = U.level(a[a.index('--level') + 1]).disc
        o = OrderedDict(build=name, bale_edge_clear=0.0, startgate_visual='keep')
        if '--bale-edge-clear' in a:
            o['bale_edge_clear'] = float(a[a.index('--bale-edge-clear') + 1])
        if '--startgate-visual' in a:
            o['startgate_visual'] = a[a.index('--startgate-visual') + 1]
        for k in ('--minimal-tdf', '--sx-flags', '--gate-patch', '--tdf-pad', '--lappacing', '--deck-norespawn',
                  '--dropgate-materials', '--base', '--template', '--legacy-defaults', '--no-lappacing'):
            if k in a:
                raise SystemExit('%s: option %s is not covered by this reference script' % (name, k))
        out[lv] = o
    return out


def run(lv, slot, tp, w, **opts):
    """ux_race.run_level(write=True, plot=False) with the files below the folder w."""
    kw = dict(dropgate_materials='template')
    kw.update(opts)
    r = R.build_race(lv, slot, template=tp, **kw)
    (w / 'race').mkdir(parents=True, exist_ok=True)
    for f in (w / 'race').iterdir():
        f.unlink()
    data_out = r.pop('_tdf_bytes')
    lp_out = r.pop('_lappacing', None)
    assets = r.pop('_assets', None) or {}
    r['assets'] = OrderedDict(('%s.%s' % k, OrderedDict(bytes=len(v), md5=U.md5_bytes(v))) for k, v in assets.items())
    assert U.md5_bytes(data_out) == r['tdf']['facts']['md5']
    tp_ = w / 'race' / ('%s-sx.tdf' % slot)
    tp_.write_bytes(data_out)
    r['tdf']['file'] = tp_.as_posix()
    for (n, t), v in assets.items():
        (w / 'race' / ('%s.%s' % (n, t))).write_bytes(v)
    if lp_out is not None:
        lpp = w / 'race' / ('%s-lappacing.bxml' % slot)
        lpp.write_bytes(lp_out)
        r['lappacing']['file'] = lpp.as_posix()
    (w / 'race.json').write_bytes(U.dumps_json({k: v for k, v in r.items() if k != '_plot'}).encode('utf-8'))
    print('%-22s %-60s tdf %d B md5 %s, %d moves, assets %s, problems %s, warnings %d' % (
        lv.disc, opts, len(data_out), r['tdf']['facts']['md5'], len(r['moves']), sorted(r['assets']), r['problems'],
        len(r['warnings'])), flush=True)


def main():
    out = Path(sys.argv[1]).resolve()
    if Path(ROOT).resolve() in [out] + list(out.parents):
        raise SystemExit('the output folder must not be inside the reference')
    levels = None
    if '--levels' in sys.argv:
        levels = [U.level(x).disc for x in sys.argv[sys.argv.index('--levels') + 1].split(',')]
    opts = track_options()
    slot = U.slot_name('sx', U.DEFAULT_SLOT)
    tp = R.Template(UB.stock_dir())
    out.mkdir(parents=True, exist_ok=True)
    (out / 'options.json').write_text(json.dumps(opts, indent=1), encoding='utf-8')
    if '--only-extras' not in sys.argv:
        for lv in U.LEVELS:
            if levels and lv.disc not in levels:
                continue
            o = opts[lv.disc]
            run(lv, slot, tp, out / 'work' / lv.disc, bale_edge_clear=o['bale_edge_clear'], startgate_visual=o['startgate_visual'])
    if '--no-variants' not in sys.argv:
        (out / 'variants.json').write_text(json.dumps(OrderedDict((k, dict(level=v[0], options=v[1])) for k, v in VARIANTS.items()), indent=1),
                                           encoding='utf-8')
        for name, (lvn, vo) in VARIANTS.items():
            lv = U.level(lvn)
            if levels and lv.disc not in levels:
                continue
            o = opts[lv.disc]
            kw = dict(bale_edge_clear=o['bale_edge_clear'], startgate_visual=o['startgate_visual'])
            kw.update(vo)
            run(lv, slot, tp, out / 'variants' / name / lv.disc, **kw)
    # lap pacing and the start-pack class / reseat on every level
    (out / 'lappacing').mkdir(exist_ok=True); (out / 'startpack').mkdir(exist_ok=True)
    for lv in U.LEVELS:
        if levels and lv.disc not in levels:
            continue
        src = U.DISC_AI / ('%s-lappacing.bxml' % lv.disc)
        raw = src.read_bytes()
        b, info = LP.to_reflex(raw, 'Supercross')
        tab, probs = LP.engine_check(b, 'Supercross', 0)
        tab0, probs0 = LP.engine_check(raw, 'Supercross', 0)
        b2, keys2, map2 = LP.convert(raw, 'Supercross')
        (out / 'lappacing' / ('%s.bxml' % lv.disc)).write_bytes(b)
        (out / 'lappacing' / ('%s.json' % lv.disc)).write_text(U.dumps_json(dict(
            info=info, table=tab, problems=probs, source_table=tab0, source_problems=probs0, convert_md5=U.md5_bytes(b2), convert_keys=keys2,
            convert_mapping=map2)), encoding='utf-8')
        d = U.read_json(U.work_dir(lv) / 'level.json')
        hf = UE.HeightField(lv, d)
        cw = UE.CollisionWorld(lv, d)
        bales = SP.footprints(lv, d, cw)
        secs = SP.tdf_sections(lv)
        models = SP.chute_models(secs, hf)
        cls = SP.class_report(models, bales, SP.classify(models, bales))
        rs, rcls = SP.reseat(secs, bales, hf, 6.0)
        (out / 'startpack' / ('%s.json' % lv.disc)).write_text(U.dumps_json(OrderedDict(
            popdowns=len(bales), footprint_points=[int(len(b_[1])) for b_ in bales], chutes=[M['si'] for M in models],
            turn=[[float(t) for t in M['turn']] for M in models], outside=[M['outside'] for M in models],
            frozen=[[bool(x) for x in M['frozen']] for M in models], cls=cls, reseat=[[k, v] for k, v in rs.items()], reseat_class=rcls)),
            encoding='utf-8')
        print('%-22s lap pacing %d B added %d problems %s; start pack: %d PopDowns, class %s, reseat %d' % (
            lv.disc, len(b), len(info['added']), probs, len(bales), [c['id'] for c in cls], len(rs)), flush=True)


if __name__ == '__main__':
    main()

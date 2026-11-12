#!/usr/bin/env python3
"""tests/untamed_convert/assembly_parts: the reference values of the "assembly parts" (ux_base, ux_build_assets, ux_tuff, ux_water,
ux_screens, ux_ads, ux_splitgate, ux_dirtlook), as the current Python code returns them IN MEMORY for the 12 levels with each track's
own build options (work/combined/commands.json). Run with the reference's Python, read-only on the reference:

    python.exe -I -B ref_parts.py <out dir> [family[,family...]] [level[,level...]]

families: base (level independent), assets, closure, tuff, water, screens, ads, splitgate, dirtlook (default: all). Everything is written below
<out dir>; ux_common.check_writable is replaced by a guard that refuses any other path (ux_splitgate writes its material folder:
its out_dir is pointed into <out dir>). UX_PY_ROOT names the reference.

Formats: JSON documents through ux_common.dumps_json (sorted keys, the builder's own writer); ordered things are lists. Entity Elems are
written as back-to-back SceneChunk documents (ux_entities.encode_chunks), '<name>.chunks'. Asset maps are rows
[name, type, md5, bytes, source, origin].
"""
import copy, json, os, shlex, sys, time
from collections import OrderedDict
from pathlib import Path

PY_ROOT = os.environ['UX_PY_ROOT']
sys.path.insert(0, PY_ROOT + '/tools')
import ux_common as U  # noqa: E402

OUT = Path(sys.argv[1]).resolve()
FAMILIES = sys.argv[2].split(',') if len(sys.argv) > 2 and sys.argv[2] != 'all' else ['base', 'assets', 'closure', 'tuff',
                                                                                     'water', 'screens', 'ads', 'splitgate', 'dirtlook']
ONLY = sys.argv[3].split(',') if len(sys.argv) > 3 else None


def _guard(path):
    p = Path(path).resolve()
    if OUT != p and OUT not in p.parents:
        raise SystemExit('refusing to write %s (outside %s)' % (p, OUT))
    return Path(path)


U.check_writable = _guard

import numpy as np  # noqa: E402
import ux_base as UB  # noqa: E402
import ux_build_assets as BA  # noqa: E402
import ux_build_track as BT  # noqa: E402
import ux_entities as UE  # noqa: E402

bxml = U.skill('bxml'); scene = U.skill('scene'); pkg = U.skill('pkg')
T0 = time.time()


def log(*a):
    print('[%6.1f] %s' % (time.time() - T0, ' '.join(str(x) for x in a)), flush=True)


def quiet(*a, **k):
    pass


def J(rel, obj):
    p = OUT / rel
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_bytes(U.dumps_json(obj, indent=1).encode('utf-8'))


def W(rel, data):
    p = OUT / rel
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_bytes(bytes(data))


def chunks(elems):
    return b''.join(UE.encode_chunks(list(elems)))


def rows(assets):
    """{(name, type): (bytes, source, origin, ...)} -> [[name, type, md5, bytes, source, origin]]"""
    return [[k[0], k[1], U.md5_bytes(v[0]), len(v[0]), v[1], v[2]] for k, v in assets.items()]


def commands():
    d = json.load(open(PY_ROOT + '/work/combined/commands.json', encoding='utf-8'))
    out = OrderedDict()
    for k, v in d.items():
        if k.startswith('_'):
            continue
        argv = shlex.split(v[0].split('ux_build_track.py ', 1)[1])
        o = BT.parse(argv)
        out[U.level(o.level).disc] = (k, o, argv)
    return out


def s7_options(o, st):
    """ux_build_track.Builder.build: the S7 call of this build."""
    mp = {'collision': st.dirs['collision'] / 'collision.json', 'materials': st.dirs['materials'] / 'materials.json'}
    opts = dict(static_bales=o.static_bales, static_pop=o.static_pop, crowd_off=o.crowd_off, manifest_paths=mp,
                crowd_tier=o.crowd_tier, group_number=o.group_number, hb_freeride=o.hb_freeride, buried=o.buried,
                fr_clearance=o.fr_clearance, bale_trigger_move=UE.parse_move(o.bale_trigger_move) if o.kind == 'sx' else None)
    if o.drop_gate_top: opts['drop_gate_top'] = True
    if o.bale_reseat and o.kind == 'sx': opts['bale_reseat'] = float(o.bale_reseat)
    return opts


# ====================================================================================================== base
def base_family():
    base = UB.resolve('stock', UB.DEFAULT_STOCK_LEVEL, None, 'sx', quiet)
    f = U.slot_files(base.folder, base.stem)
    raw = scene.load(str(f['scene']))
    keep = []
    for s in raw:
        suf = s.name[len(base.stem):] if s.name.startswith(base.stem) else None
        for e in s.entities():
            ok, why = UB.keep_entity(base.policy, suf, e)
            keep.append([s.name, UB._id(e), scene.ns(e), UB._name(e), bool(ok), why])
    J('base/keep.json', keep)
    ts, subs, lvb, f, rec = UB.load_prepared(base, quiet)
    scene.save(str(OUT / 'base' / 'prepared.scene'), subs)
    W('base/prepared.level', lvb.to_bytes())
    J('base/prepare.json', rec)
    J('base/record.json', base.record())
    roles = UB.hf_roles(subs, base.stem, ts)
    J('base/hf_roles.json', roles)
    slot = U.slot_name('sx', 1)
    sc = OrderedDict()
    for which in ('stock', 'all', 'none'):
        kinds = UB.script_kinds(base, which)
        ren = UB.script_renames(base, ts, slot, kinds)
        sc[which] = OrderedDict(kinds=list(kinds), renames=[[k[0], k[1], v] for k, v in ren.items()])
    sc['default_kinds'] = [[k[0], k[1], v] for k, v in UB.script_renames(base, ts, slot).items()]
    sc['track_part'] = OrderedDict((s, UB.intro_track_part(s)) for s in (slot, 'Beta_Track_Slot_1', 'SX_FortDodge', 'NoUnderscore',
                                                                         'A_b', 'Beta_Sx_Track_Slot_12'))
    J('base/scripts.json', sc)
    ents = [e for s_ in subs for e in s_.entities()]
    types = {scene.ns(e) for e in ents}
    lit = OrderedDict()
    for nm, tt in (('base', types), ('water', types | {'WaterSystemRSData'}), ('crowd', types | {'BillboardCrowdRSData'}),
                   ('forest', {'ForestSystemRSData'}), ('none', set())):
        lit[nm] = OrderedDict(types=sorted(tt), rows=[[r[0], list(r[1]), r[2], r[3], r[4]] for r in UB.literal_rows(tt, 'sx')],
                              roots=[list(k) for k in UB.literal_roots(tt, 'sx')])
    J('base/literals.json', lit)
    J('base/core_global.json', sorted([k[0], k[1], v] for k, v in UB.core_global_assets().items()))
    gl = BA.global_assets()
    J('base/global_assets.json', sorted([k[0], k[1], v] for k, v in gl.items()))
    AS = UB.anim_sets()
    J('base/anim_sets.json', OrderedDict(sets=[[k, v] for k, v in AS['sets'].items()], vehicle=[[k, v] for k, v in AS['vehicle'].items()],
                                         md5=AS['md5']))
    J('base/session_anims.json', UB.session_anims())
    # closure as ux_build_track.base_prune runs it, over the base alone
    by = OrderedDict(((a.name, a.type), a) for a in ts.assets)
    TA = BA.TemplateAssets(ts)
    ts.prefetch(3, types=tuple(UB.FOLLOW))
    index = OrderedDict()
    for k in by:
        index.setdefault(k[0].lower(), []).append(k)
    ren = UB.script_renames(base, ts, slot, UB.script_kinds(base, 'stock'))
    roots = [(base.stem + '-sx', 'tdf')] + [(k[0], k[1]) for k in ren] + [(n, 'sound') for n in base.policy.get('implicit_sounds', ())]
    roots += [tuple(r) for r in base.policy.get('implicit_roots', ()) if tuple(r) not in roots]
    litr = UB.literal_roots(types, 'sx')
    roots += [k for k in litr if k not in roots]
    set_roots = [s for s, t in UB.ENGINE_ANIM_SETS.items() if t is None or t in types]
    info = {}
    kept, unres, glob = UB.closure(ents, roots, index, lambda k: TA.logical(by[k]), gl, sets=AS['sets'], set_roots=set_roots, info=info)
    J('base/closure.json', OrderedDict(roots=[list(k) for k in roots], set_roots=set_roots,
                                       kept=[[k[0], k[1], v] for k, v in kept.items()], unresolved=[list(u) for u in unres],
                                       glob=sorted(list(g) for g in glob), sets=[[k, v] for k, v in info['sets'].items()],
                                       nowhere=[list(x) for x in info['set_members_nowhere']]))
    # the same closure without the animation sets and scripts (the 'none' variant) and with strings of every asset type
    kept2, unres2, glob2 = UB.closure(ents, [(base.stem + '-sx', 'tdf')], index, lambda k: TA.logical(by[k]), gl)
    J('base/closure_plain.json', OrderedDict(kept=[[k[0], k[1], v] for k, v in kept2.items()], unresolved=[list(u) for u in unres2],
                                             glob=sorted(list(g) for g in glob2)))
    es = [[UB._name(e), UB.entity_strings(e)] for e in ents]
    J('base/entity_strings.json', es)
    ast = OrderedDict()
    for k in list(by)[::37]:
        if k[1] in UB.FOLLOW:
            ast['%s.%s' % k] = UB.asset_strings(TA.logical(by[k]), k[1])
    J('base/asset_strings.json', ast)
    # constant textures: the builder's two (--base-layout neutral) and synthetic ones; tex_bc on a patterned image
    import uxtex
    from PIL import Image
    ct = []
    for role, rgba in (('Custom_DEF', BT.SOFTNESS_CONST_RGB + (255,)), ('Custom_CHR_LUM', BT.CHROMLUM_CONST_RGBA)):
        nm = roles['by_role'][role]
        old = uxtex.PcTexture(ts.read_logical(ts.get(nm, 'texture')))
        n_, w_, h_, f_ = old.recs[0]
        b = UB.constant_texture(rgba, w_, h_, f_)
        ct.append([role, nm, list(rgba), w_, h_, f_.decode(), U.md5_bytes(b), len(b)])
    for rgba, w_, h_, f_ in (((10, 20, 30, 255), 64, 64, b'DXT1'), ((200, 100, 50, 128), 32, 16, b'DXT5'), ((255, 255, 255, 0), 4, 4, b'DXT5'),
                             ((128, 128, 128, 255), 1024, 1024, b'DXT1'), ((1, 2, 3, 4), 8, 2, b'DXT5'), ((77, 0, 255, 255), 2, 16, b'DXT1')):
        b = UB.constant_texture(rgba, w_, h_, f_)
        ct.append(['synthetic', None, list(rgba), w_, h_, f_.decode(), U.md5_bytes(b), len(b)])
    J('base/constant_textures.json', ct)
    tb = []
    for w_, h_, mode, f_ in ((64, 32, 'RGB', b'DXT1'), (64, 32, 'RGBA', b'DXT5'), (16, 16, 'RGBA', b'DXT5'), (128, 128, 'RGB', b'DXT1'), (8, 32, 'RGB', b'DXT1')):
        a = np.zeros((h_, w_, 4), np.uint8)
        yy, xx = np.mgrid[0:h_, 0:w_]
        a[..., 0] = (xx * 7 + yy * 13) % 256; a[..., 1] = ((xx * 3) ^ (yy * 5)) % 256; a[..., 2] = (xx + yy) % 256
        a[..., 3] = (xx * yy + 17) % 256
        img = Image.fromarray(np.ascontiguousarray(a if mode == 'RGBA' else a[..., :3]), mode)
        b = UB.tex_bc(img, f_)
        tb.append([w_, h_, mode, f_.decode(), U.md5_bytes(b), len(b)])
    J('base/tex_bc.json', tb)
    # tex_bc(straight_alpha=True) (the dirt-look maps): RGBA with a patterned alpha, with alpha 0 (a DetailMask) and RGB (no effect)
    ts_ = []
    for w_, h_, mode, f_, alpha in ((64, 32, 'RGBA', b'DXT5', 'pattern'), (64, 64, 'RGBA', b'DXT5', 'zero'), (16, 16, 'RGBA', b'DXT5', 'pattern'),
                                    (128, 128, 'RGB', b'DXT1', None), (8, 32, 'RGBA', b'DXT5', 'zero'), (256, 256, 'RGBA', b'DXT5', 'pattern')):
        a = np.zeros((h_, w_, 4), np.uint8)
        yy, xx = np.mgrid[0:h_, 0:w_]
        a[..., 0] = (xx * 7 + yy * 13) % 256; a[..., 1] = ((xx * 3) ^ (yy * 5)) % 256; a[..., 2] = (xx + yy) % 256
        a[..., 3] = 0 if alpha == 'zero' else (xx * yy + 17) % 256
        img = Image.fromarray(np.ascontiguousarray(a if mode == 'RGBA' else a[..., :3]), mode)
        b = BT.tex_bc(img, f_, straight_alpha=True)
        ts_.append([w_, h_, mode, f_.decode(), alpha, U.md5_bytes(b), len(b), U.md5_bytes(BT.tex_bc(img, f_))])
    J('base/tex_bc_straight.json', ts_)
    # ux_water.splash on the base's own ParticleManager kit (as ux_build_track.water_splash calls it) + retarget_particles
    import ux_water as UW
    pms = [e for s_ in subs if s_.name == base.stem + 'Base' for e in s_.entities() if scene.ns(e) == 'ParticleManagerRSData']
    roles_pm = UW.particle_manager_assets(pms[0])
    kit = roles_pm['ParticleEmitterAsset'][0]
    kb = {t: TA.logical(TA.clash(kit, t)) for t in ('particle', 'partattr', 'texture')}
    srec, sassets = UW.splash(kb['particle'], kb['partattr'], kb['texture'], kit, log=quiet)
    for k, v in sassets.items():
        W('base/splash/%s.%s' % k, v[0])
    e2 = copy.deepcopy(pms[0])
    UW.retarget_particles(e2, srec['assets'])
    W('base/splash_pm.chunks', chunks([e2]))
    J('base/splash.json', OrderedDict(record=srec, assets=rows(sassets), kit=kit, roles=OrderedDict((k, list(v)) for k, v in roles_pm.items()),
                                      roles_after=OrderedDict((k, list(v)) for k, v in UW.particle_manager_assets(e2).items()),
                                      tire_in_water=sum(1 for e in bxml.to_tree(bxml.Bxml(sassets[(srec['assets'], 'partattr')][0])).findall('Emitter')
                                                        if UW.tire_in_water(e))))
    # ux_water.reflection_cube on the base's level cube (as ux_build_track.water_reflection calls it) + reflection_retarget
    amb = [e for s_ in subs if s_.name == base.stem + 'Base' for e in s_.entities() if scene.ns(e) == 'AmbientDirectionalLightRSData']
    cube, ctyp = UW.reflection_map(amb[0])
    cb = TA.logical(TA.clash(cube, ctyp))
    rrecs = OrderedDict()
    for mode in ('dim', 'nolights'):
        b, rrec = UW.reflection_cube(cb, mode)
        W('base/reflection/%s.texture' % mode, b)
        rrecs[mode] = rrec
    e3 = copy.deepcopy(amb[0])
    old = UW.reflection_retarget(e3, UW.REFLECTION_PREFIX + cube)
    W('base/reflection_amb.chunks', chunks([e3]))
    J('base/reflection.json', OrderedDict(cube=cube, type=ctyp, old=old, after=list(UW.reflection_map(e3)), records=rrecs))
    # Image.resize runs its vertical pass first for some sizes (the intermediate image is 8-bit, so the order decides bytes):
    # the md5 of the result for sizes on both sides of that rule, three modes and three filters
    ro = []
    for (w_, h_, ow, oh) in ((8, 32, 4, 16), (16, 64, 8, 32), (64, 512, 32, 256), (40, 80, 21, 79), (40, 80, 20, 79), (60, 60, 31, 1), (60, 60, 30, 1),
                             (80, 40, 61, 1), (80, 40, 60, 1), (50, 53, 51, 52), (50, 53, 49, 52), (30, 100, 11, 37), (100, 30, 37, 11), (24, 115, 106, 117),
                             (14, 105, 110, 24), (81, 96, 3, 97), (256, 256, 512, 190), (512, 512, 512, 190), (128, 256, 100, 60), (7, 33, 3, 10), (33, 7, 10, 3),
                             (64, 64, 64, 9), (64, 64, 9, 64), (22, 113, 1, 60), (82, 111, 21, 10), (73, 70, 47, 11)):
        a = np.zeros((h_, w_, 4), np.uint8)
        yy, xx = np.mgrid[0:h_, 0:w_]
        a[..., 0] = (xx * 7 + yy * 13) % 256; a[..., 1] = ((xx * 3) ^ (yy * 5)) % 256; a[..., 2] = (xx + yy) % 256
        a[..., 3] = (xx * yy + 17) % 256
        for mode in ('L', 'RGB', 'RGBA'):
            img = Image.fromarray(np.ascontiguousarray(a[..., 0] if mode == 'L' else a if mode == 'RGBA' else a[..., :3]), mode)
            for fname, flt in (('lanczos', Image.LANCZOS), ('bilinear', Image.BILINEAR), ('box', Image.BOX)):
                ro.append([w_, h_, ow, oh, mode, fname, U.md5_bytes(img.resize((ow, oh), flt).tobytes())])
    J('base/resize_order.json', ro)
    log('base: %d kept assets, %d unresolved' % (len(kept), len(unres)))


# ====================================================================================================== per level
class Level:
    def __init__(self, disc, key, o):
        self.lv = U.level(disc); self.key = key; self.o = o
        self.st = BT.Stages(self.lv, o, quiet)
        self.dirs = OrderedDict((k, v) for k, v in self.st.dirs.items() if k != 'level')
        self._items = None; self._S = None

    def rel(self, name):
        return '%s/%s' % (self.lv.disc, name)

    def sources(self):
        return BA.Sources(self.lv, self.dirs)

    def items(self):
        if self._items is None:
            items, edoc = UE.build_entities(self.lv, 'sx', write=False, **s7_options(self.o, self.st))
            self._items = list(items)
            for sub in ('Base', 'FreeRide'):
                W(self.rel('items_%s.chunks' % sub), chunks(e for s, e in self._items if s == sub))
            J(self.rel('items.json'), OrderedDict(subs=[s for s, _ in self._items], build=self.key,
                                                  options=OrderedDict((k, getattr(self.o, k)) for k in (
                                                      'bales', 'tuff_collision', 'tuff_wraps', 'ec_water', 'ec_water_depth', 'ec_water_kit_order',
                                                      'ec_water_look', 'ec_water_splash', 'ec_water_shore', 'dirt_look', 'dirt_tint', 'hf_params',
                                                      'screens', 'screen_light', 'massive_ads', 'split_gate', 'group_number', 'type9', 'tex_max',
                                                      'prims_to_22', 'bale_trigger_move', 'bale_reseat', 'base_scripts')),
                                                  dirs=OrderedDict((k, v.relative_to(U.work_dir(self.lv)).as_posix()) for k, v in self.dirs.items()),
                                                  mat_opts=self.st.mat_opts.as_dict(), lift=U.y_lift(self.lv)))
        return copy.deepcopy(self._items)


_BASE = None


def base_set():
    global _BASE
    if _BASE is None:
        base = UB.resolve('stock', UB.DEFAULT_STOCK_LEVEL, None, 'sx', quiet)
        ts, subs, lvb, f, rec = UB.load_prepared(base, quiet)
        _BASE = (base, ts, subs, BA.TemplateAssets(ts))
    return _BASE


def put_items(L, name, items):
    for sub in ('Base', 'FreeRide'):
        W(L.rel('%s_%s.chunks' % (name, sub)), chunks(e for s, e in items if s == sub))
    return [s for s, _ in items]


def assets_family(L):
    base, ts, subs, TA = base_set()
    S = L.sources()
    arows = []
    for k, r in S.assets.items():
        arows.append([k[0], k[1], r['file'].as_posix(), r['md5'], r['origin'], sorted(x for x in r if x not in ('file', 'md5', 'origin')),
                      U.md5_bytes(U.dumps_json(r['entry']).encode('utf-8')) if 'entry' in r else None, r.get('in_template'),
                      U.md5_bytes(S.read(k))])
    doc = OrderedDict(assets=arows, problems=S.problems, warnings=S.warnings, manifests=S.manifests,
                      mesh_parts=sorted(S.mesh_parts), dropped_materials=sorted(S.dropped_materials),
                      texture_renames=[[k, v] for k, v in S.texture_renames.items()], template_textures=sorted(S.template_textures),
                      material_entries=list(S.material_entries),
                      clash=[[k[0], k[1], (TA.clash(*k).name if TA.clash(*k) is not None else None), TA.has(*k)] for k in S.assets],
                      template_keys=len(TA.keys), template_lower=len(TA.lower))
    J(L.rel('sources.json'), doc)
    items = L.items()
    J(L.rel('entity_refs.json'), [[list(r) for r in BA.entity_refs(e)] for _, e in items])
    # rename_refs: the first six distinct references in sorted order get a UX_ prefix
    allr = sorted({r for _, e in items for r in BA.entity_refs(e)})[:6]
    ren = {r: 'UX_' + r[0] for r in allr}
    it2 = L.items()
    n = sum(BA.rename_refs(e, ren) for _, e in it2)
    put_items(L, 'renamed', it2)
    J(L.rel('rename_refs.json'), OrderedDict(renames=[[k[0], k[1], v] for k, v in sorted(ren.items())], changes=n))
    # filter_dropped as the builder calls it (the stage's dropped materials; fallback = the base's Editor_Default material)
    it3 = L.items()
    fb = 'Editor_Default' if TA.has('Editor_Default', 'material') else None
    out, new_assets, rep = BA.filter_dropped(it3, S.dropped_materials, lambda name: (S.read((name, 'model')), S.read((name, 'surface'))),
                                             quiet, fallback_material=fb)
    subs_out = put_items(L, 'filtered', out)
    J(L.rel('filter_dropped.json'), OrderedDict(report=rep, fallback=fb, subs=subs_out,
                                                new_assets=[[k[0], k[1], U.md5_bytes(v), len(v)] for k, v in new_assets.items()]))
    log(L.lv.disc, 'assets: %d sources, %d dropped materials, filter changed %d removed %d variants %d' % (
        len(S.assets), len(S.dropped_materials), rep['entities_changed'], len(rep['entities_removed']), len(rep['variants'])))


def closure_family(L):
    """ux_base.closure as ux_build_track.base_prune runs it: the prepared base entities + the level's converted entities over the
    base package + the stage outputs (additions)."""
    base, ts, subs, TA = base_set()
    S = L.sources()
    by = OrderedDict(((a.name, a.type), a) for a in ts.assets)
    index = OrderedDict()
    for k in list(by) + list(S.assets):
        index.setdefault(k[0].lower(), []).append(k)
    ents = [e for s_ in subs for e in s_.entities()] + [e for _, e in L.items()]
    types = {scene.ns(e) for e in ents}
    slot = U.slot_name('sx', 1)
    ren = UB.script_renames(base, ts, slot, UB.script_kinds(base, L.o.base_scripts))
    roots = [(base.stem + '-sx', 'tdf')] + [(k[0], k[1]) for k in ren] + [(n, 'sound') for n in base.policy.get('implicit_sounds', ())]
    roots += [tuple(r) for r in base.policy.get('implicit_roots', ()) if tuple(r) not in roots]
    lit = UB.literal_roots(types, 'sx')
    roots += [k for k in lit if k not in roots]
    AS = UB.anim_sets()
    set_roots = [s for s, t in UB.ENGINE_ANIM_SETS.items() if t is None or t in types]
    info = {}
    kept, unres, glob = UB.closure(ents, roots, index, lambda k: S.read(k) if k in S.assets else TA.logical(by[k]), BA.global_assets(),
                                   sets=AS['sets'], set_roots=set_roots, info=info)
    J(L.rel('closure.json'), OrderedDict(roots=[list(k) for k in roots], set_roots=set_roots, literal_roots=[list(k) for k in lit],
                                         kept=[[k[0], k[1], v] for k, v in kept.items()], unresolved=[list(u) for u in unres],
                                         glob=sorted(list(g) for g in glob), sets=[[k, v] for k, v in info['sets'].items()],
                                         nowhere=[list(x) for x in info['set_members_nowhere']]))
    log(L.lv.disc, 'closure: %d kept (%d of the base), %d unresolved' % (len(kept), sum(1 for k in kept if k in by), len(unres)))


def tuff_family(L):
    import ux_tuff as UT
    doc = OrderedDict()
    for coll, wraps in (('stock', 'rule'), ('untamed', 'rule'), ('stock', 'fortdodge')):
        items = L.items()
        rec, need = UT.swap(items, L.lv, collision=coll, wraps=wraps, log=quiet)
        tag = '%s_%s' % (coll, wraps)
        put_items(L, 'tuff_%s' % tag, items)
        doc[tag] = OrderedDict(record=rec, need=[list(k) for k in need], assets=rows(UT.assets(need)))
    J(L.rel('tuff.json'), doc)
    log(L.lv.disc, 'tuff: %d bodies' % doc['stock_rule']['record']['bodies'])


def tuff_static():
    import ux_tuff as UT
    gm = [[m, w, U.md5_bytes(UT.generate_material(m, w))] for m in ('Single', 'Double', 'Wedge', 'Corner') for w in UT.POOL]
    sp = ['Thor', 'FMF', 'smith', 'RK Chains', 'Monster', 'RedBull', 'Yamaha', 'Honda', 'Suzuki', 'Kawasaki', 'KTM', 'Fox', 'ONeal',
          'Dunlop', 'Bridgestone', 'Pirelli', 'Michelin', 'Maxxis', 'Acerbis', 'Answer', 'DG', '', 'Shift', 'sidi', 'CERNICS', 'Hiper',
          'Toyota', 'THQ', 'Rainbow', 'Xbox', 'AMP', 'Makita', 'Parts Unlimited', 'Pro-Circuit', 'Scott', 'Spy', 'Troy Lee Designs']
    sm = OrderedDict()
    for short in ('norton', 'fairview', 'x'):
        for wraps in ('rule', 'fortdodge'):
            for n in (5, 12, len(sp)):
                sm['%s/%s/%d' % (short, wraps, n)] = [[k, v[0], v[1]] for k, v in UT.sponsor_map(short, sp[:n], wraps).items()]
    mt = []
    for m in ('Single', 'Double', 'Wedge', 'Corner'):
        t = UT.material_texture(UT.generate_material(m, 'KTM'))
        mt.append([m, t[0], t[1]])
    J('tuff_static.json', OrderedDict(generated=gm, sponsor_map=sm, material_texture=mt,
                                      untamed_sponsor=[[s, UT.untamed_sponsor(s)] for s in (
                                          'HayBale_Single_Thor', 'UX_HayBale_Double_FMF', 'haybale_t1_Fox_Racing', 'HayBale_Corner_', 'Other', None,
                                          'HayBale_Wedge_A_B', 'UX_UX_HayBale_Single_X')]))
    log('tuff static: %d generated materials' % len(gm))


def water_family(L):
    import ux_water as UW
    hf = np.load(L.st.dirs['terrain'] / 'hf.npy')
    doc = OrderedDict()
    bcap = float(L.o.ec_water_depth) if L.o.ec_water_depth != 'untamed' else 1e9
    # 'cap' = the build's own call (ux_build_track: look = --ec-water-look, default arena since the water-look round);
    # 'v4look' = the same with look untamed (the fairview-v4 / lincoln-v4 bytes)
    for tag, cap, order, look in (('cap', bcap, L.o.ec_water_kit_order, L.o.ec_water_look), ('v4look', bcap, L.o.ec_water_kit_order, 'untamed'),
                                  ('untamed', 1e9, 'first', 'untamed'), ('last', 1.25, 'last', 'untamed'), ('cap05', 0.5, 'first', 'arena')):
        rec, assets, elems = UW.build(L.lv, hf, cap, log=quiet, kit_order=order, look=look)
        W(L.rel('water_%s.chunks' % tag), chunks(elems))
        doc[tag] = OrderedDict(record=rec, assets=rows(assets), entities=len(elems), cap=cap, kit_order=order, look=look)
    Lj = U.read_json(U.work_dir(L.lv) / 'level.json')
    # dirt-look rollout (DESIGN 23.11): the shore colour from the built ground, as ux_build_track.water_ground gives it: 'ground' =
    # the build's own call (--ec-water-shore ground on the dirt-look stock TopDown), 'ground_untamed' = on the terrain stage's
    # TopDown (--dirt-look untamed); a level without water takes no ground (the builder asks for none)
    import ux_dirtlook as DL
    S = float((U.HF_SIZE - 1) * 1)
    grounds = OrderedDict(ground=None, ground_untamed=None)
    if any(e['type'] == 'WaterSystemRSData' for e in Lj['entities']):
        tdir = L.st.dirs['terrain']
        tj = U.read_json(tdir / 'terrain.json')
        grounds['ground'] = (dirtlook_maps(L)[2].astype(np.float64) / 255.0, S)
        grounds['ground_untamed'] = (DL.on_client(DL._png(tdir / (tj.get('ground') or {}).get('default_topdown', 'topdown_occ.png')), S), S)
    for tag, ground in grounds.items():
        rec, assets, elems = UW.build(L.lv, hf, bcap, log=quiet, kit_order=L.o.ec_water_kit_order, look='arena', shore='ground', ground=ground)
        W(L.rel('water_%s.chunks' % tag), chunks(elems))
        doc[tag] = OrderedDict(record=rec, assets=rows(assets), entities=len(elems), cap=bcap, kit_order=L.o.ec_water_kit_order, look='arena',
                               shore='ground')
    doc['wet_ground'] = [[list(g), None if o_ is None else list(o_), [float(x).hex() for x in UW.wet_ground(g, o_)]] for g, o_ in (
        ((0.3004, 0.25, 0.1985), None), ((0.3004, 0.25, 0.1985), (0.22, 0.2, 0.25)), ((0.4, 0.35, 0.3), (0.2, 0.25, 0.3)),
        ((0.9, 0.9, 0.2), None), ((0.05, 0.04, 0.03), (0.07, 0.07, 0.07)), ((0.268, 0.2159, 0.1641), (0.1, 0.25, 0.2)))]
    ps = OrderedDict()
    for e in Lj['entities']:
        if e['type'] == 'WaterSystemRSData':
            un = e['props']['WaterAsset']['asset']
            p = UW.parse((U.extract_dir(L.lv) / 'water' / ('%s.water' % un)).read_bytes())
            d = p.pop('desc')
            p['desc'] = [d[0], d[1], U.md5_bytes(d[2])] if d else None
            p['extent'] = list(p['extent'])
            ps[un] = p
    doc['parse'] = ps
    J(L.rel('water.json'), doc)
    log(L.lv.disc, 'water: %d surfaces' % len(doc['cap']['record']['surfaces']))


# ====================================================================================================== dirt look
def dirtlook_inputs(L):
    """(the tdf of the track's reference build, the base HF material's (shader, textures, params), the four detail layers' mean colours)."""
    import ux_dirtlook as DL
    if getattr(L, '_dl_in', None) is None:
        bd = Path(PY_ROOT) / 'build' / L.key
        info = json.load(open(bd / 'build_info.json', encoding='utf-8'))
        slot = info['slot_name']
        bts = pkg.TrackSet(str(bd / (slot + '.dx9.database')), str(bd / (slot + '.dx9.package')))
        tdf = bts.read_logical(bts.get(slot + '-sx', 'tdf'))
        base, ts, subs, TA = base_set()
        roles = UB.hf_roles(subs, base.stem, ts)
        mat = DL.material_textures(ts.read_logical(ts.get(roles['by_role']['Custom_HFTRN'], 'material')))
        means = DL.layer_means([ts.read_logical(ts.get(t, 'texture')) for t in mat[1][1:5]])
        L._dl_in = (tdf, mat, means)
    return L._dl_in


def dirtlook_maps(L):
    """ux_dirtlook.stock_look as ux_build_track.dirt_look_maps calls it -> (mask, chromlum, topdown, record)."""
    import ux_dirtlook as DL
    if getattr(L, '_dl_maps', None) is None:
        tdf, mat, means = dirtlook_inputs(L)
        tdir = L.st.dirs['terrain']
        L._dl_maps = DL.stock_look(tdir, U.read_json(tdir / 'terrain.json'), tdf, means, S=float((U.HF_SIZE - 1) * 1), log=quiet)
    return L._dl_maps


def math_inputs(n, lo, hi, mul):
    """n doubles in [lo, hi): the same expression in the C# test (integer arithmetic, one division, one product, one sum)."""
    return np.array([lo + ((i * mul) % 1000003) / 1000003.0 * (hi - lo) for i in range(n)], np.float64)


def dirtlook_static():
    """The C library functions ux_dirtlook reaches through numpy (np.exp, np.log, v ** k, np.hypot) on fixed inputs, as raw float64."""
    x = math_inputs(200000, -7.0, 0.0, 7919)
    W('dirtlook/exp.f64', np.exp(x).tobytes())
    v = math_inputs(200000, 0.5, 1.35, 104729)
    W('dirtlook/log.f64', np.log(v).tobytes())
    ks = [0.9669, 0.5, 0.25, 0.731, 0.123456789]
    for i, k in enumerate(ks):
        W('dirtlook/pow_%d.f64' % i, (v ** k).tobytes())
    a = math_inputs(200000, -9.0, 9.0, 15485863); b = math_inputs(200000, -9.0, 9.0, 32452843)
    a[:2000] = np.round(a[:2000] * 2) / 2; b[:2000] = np.round(b[:2000] * 2) / 2            # halves: exact and near-exact lengths
    b[2000:3000] = 0.0
    W('dirtlook/hypot_a.f64', a.tobytes()); W('dirtlook/hypot_b.f64', b.tobytes())
    W('dirtlook/hypot.f64', np.hypot(a, b).tobytes())
    # the correctly rounded values of the first 50,000 (decimal, 45 digits; float() of a Decimal rounds correctly): what the port's
    # double-double functions must return, whatever the C library does
    from decimal import Decimal, getcontext
    getcontext().prec = 45
    m = 50000
    W('dirtlook/exp_cr.f64', np.array([float(Decimal(float(t)).exp()) for t in x[:m]]).tobytes())
    W('dirtlook/log_cr.f64', np.array([float(Decimal(float(t)).ln()) for t in v[:m]]).tobytes())
    W('dirtlook/pow_cr.f64', np.array([float((Decimal(float(t)).ln() * Decimal(ks[0])).exp()) for t in v[:m]]).tobytes())
    import ux_dirtlook as DL
    J('dirtlook/static.json', OrderedDict(
        n=200000, pow_exponents=ks, mask_scale=float(DL.MASK_SCALE).hex(),
        fractions=[float(f).hex() for f in np.round(np.linspace(-1, 1, 41), 3)],
        client_index=OrderedDict((str(n), U.md5_bytes(DL.client_index(n).astype('<i8').tobytes())) for n in (512, 1024, 2048, 4096, 1000)),
        interp=[float(y).hex() for y in np.interp(math_inputs(4000, -40.0, 60.0, 7919), DL.PROFILE_SD, DL.PROFILE_A)],
        composite=U.md5_bytes(DL.composite(
            np.stack([(np.arange(4096) * 7 % 256), (np.arange(4096) * 13 % 256), (np.arange(4096) * 29 % 256), np.zeros(4096)], -1).reshape(64, 64, 4) / 255.0,
            np.stack([(np.arange(4096) * 5 % 256)] * 3 + [(np.arange(4096) * 11 % 256)], -1).reshape(64, 64, 4) / 255.0,
            np.array([[0.30, 0.24, 0.18], [0.20, 0.17, 0.12], [0.41, 0.34, 0.27], [0.67, 0.56, 0.40]])).tobytes())))
    log('dirtlook static')


def dirtlook_family(L):
    import ux_dirtlook as DL
    tdf, (sh, texs, params), means = dirtlook_inputs(L)
    tdir = L.st.dirs['terrain']
    tj = U.read_json(tdir / 'terrain.json')
    S = float((U.HF_SIZE - 1) * 1)
    W(L.rel('dirtlook.tdf'), tdf)
    m8, c8, td8, rec = dirtlook_maps(L)
    # the intermediate values stock_look does not return: its own expressions on its own functions
    fp = DL.on_client(np.load(tdir / 'footprint.npy'), S)
    corr = DL.corridor_mask(tdf, S)
    g = (tj.get('ground') or {})
    chl = ((g.get('tmask') or {}).get('channel_layers') or {})
    ch = next((c for c, t in chl.items() if t == DL.RUTS_TEX), None)
    dm = DL.on_client(DL._png(tdir / 'detailmask.png'), S)
    lanes = (dm[..., 'RGB'.index(ch)] > DL.LANE_MIN) & (fp == 1) if ch is not None else None
    track = corr | (lanes if lanes is not None else False)
    sd = DL.signed_distance(track, S / DL.G)
    lm = g['layer_means']
    occ = DL.on_client(DL._png(tdir / 'topdown_occ.png'), S).mean(-1)
    col = np.broadcast_to(np.array(lm['base']), dm.shape).copy()
    for i, k in enumerate('rgb'):
        col = col * (1 - dm[..., i:i + 1]) + np.array(lm['dirt_' + k]) * dm[..., i:i + 1]
    ratio = (occ + 1e-3) / (col.mean(-1) + 1e-3)
    on = track & (fp == 1)
    med = float(np.median(ratio[on])) if on.any() else 1.0
    v = np.clip(ratio / max(med, 1e-3), *DL.VAR_CLIP)
    inner = (sd < -4) & (fp == 1)
    lg = np.log(v[inner])
    sl = float(lg.std()) if inner.any() else 0.0
    k = min(1.0, DL.VAR_TARGET / sl) if sl > 1e-6 else 0.0
    W(L.rel('dirtlook_log.f64'), lg.tobytes())
    if 0.0 < k < 1.0:
        W(L.rel('dirtlook_pow.f64'), (v ** k).tobytes())
    from PIL import Image
    tex = OrderedDict()
    for nm, arr, fmt in (('topdown', td8, b'DXT1'), ('mask', m8, b'DXT5'), ('chromlum', c8, b'DXT5')):
        b = BT.tex_bc(Image.fromarray(arr, 'RGB' if arr.shape[-1] == 3 else 'RGBA'), fmt, straight_alpha=True)
        tex[nm] = [U.md5_bytes(b), len(b)]
    J(L.rel('dirtlook.json'), OrderedDict(
        build=L.key, tdf_md5=U.md5_bytes(tdf),
        material=OrderedDict(shader=sh, textures=texs, params=OrderedDict((k_, params[k_]) for k_ in sorted(params))),
        means=[float(x).hex() for x in means.reshape(-1)],
        footprint=U.md5_bytes(fp.astype(np.uint8).tobytes()), corridor=U.md5_bytes(corr.astype(np.uint8).tobytes()),
        lanes=None if lanes is None else U.md5_bytes(lanes.astype(np.uint8).tobytes()), track=U.md5_bytes(track.astype(np.uint8).tobytes()),
        sd=U.md5_bytes(sd.astype('<f8').tobytes()), v=U.md5_bytes(v.astype('<f8').tobytes()),
        median=med.hex(), std_log=float(sl).hex(), exponent=float(k).hex(), inner_px=int(inner.sum()),
        maps=OrderedDict(mask=U.md5_bytes(m8.tobytes()), chromlum=U.md5_bytes(c8.tobytes()), topdown=U.md5_bytes(td8.tobytes())),
        textures=tex, record=rec))
    log(L.lv.disc, 'dirtlook: track %d px, exponent %r' % (rec['track']['track_px'], k))


def screens_family(L):
    import ux_screens as US
    S = L.sources()
    doc = OrderedDict()
    for tag, gain in (('default', float(L.o.screen_light)), ('gain07', 0.7), ('gain1', 1.0)):
        rec, mats, texs = US.build(L.lv, S.material_entries, gain=gain, log=quiet)
        doc[tag] = OrderedDict(record=rec, materials=[[k, U.md5_bytes(v[0]), len(v[0]), list(v[1]), v[2]] for k, v in mats.items()],
                               textures=rows(texs))
        if tag == 'default':
            for k, v in texs.items():
                W(L.rel('screens/%s.%s' % k), v[0])
            for k, v in mats.items():
                W(L.rel('screens/%s.material' % k), v[0])
    J(L.rel('screens.json'), doc)
    log(L.lv.disc, 'screens: %d materials, %d textures' % (len(doc['default']['materials']), len(doc['default']['textures'])))


def ads_family(L):
    import ux_ads as UA
    base, ts, subs, TA = base_set()
    doc = OrderedDict()
    for mode in ('keep', 'model', 'drop'):
        S = L.sources()
        rec, assets, elems = UA.build(L.lv, mode, L.st.mat_opts, S, TA, U.y_lift(L.lv), group_number=L.o.group_number, log=quiet)
        W(L.rel('ads_%s.chunks' % mode), chunks(elems))
        doc[mode] = OrderedDict(record=rec, assets=rows(assets), entities=len(elems))
        if mode == 'keep':
            for k, v in assets.items():
                W(L.rel('ads/%s.%s' % k), v[0])
    J(L.rel('ads.json'), doc)
    log(L.lv.disc, 'ads: %d panels, %d assets' % (len(doc['keep']['record']['panels']), len(doc['keep']['assets'])))


def splitgate_family(L):
    import ux_splitgate as SG
    base, ts, subs, TA = base_set()
    od = OUT / L.lv.disc / 'splitgate'
    SG.out_dir = lambda lv, mat_opts: od / ('materials' + (('-' + mat_opts.tag()) if mat_opts.tag() else ''))
    S = L.sources()
    try:
        rec = SG.build(L.lv, L.st.mat_opts, S, TA, U.y_lift(L.lv), group_number=L.o.group_number, log=quiet)
    except SG.SplitGateError as ex:
        J(L.rel('splitgate.json'), OrderedDict(error=str(ex)))
        log(L.lv.disc, 'splitgate: SplitGateError %s' % ex)
        return
    assets = rec.pop('_assets'); srcs = rec.pop('_sources'); dgp = rec.pop('_dropgate_props'); elems = rec.pop('_entities')
    W(L.rel('splitgate.chunks'), chunks(elems))
    for k, b in assets.items():
        W(L.rel('splitgate_assets/%s.%s' % k), b)
    J(L.rel('splitgate.json'), OrderedDict(record=rec, dropgate_props=dgp, entities=len(elems),
                                           assets=[[k[0], k[1], U.md5_bytes(b), len(b), srcs[k][0], srcs[k][1]] for k, b in assets.items()]))
    log(L.lv.disc, 'splitgate: %d assets' % len(assets))


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    # the reference modules these values come from (the reference is still being edited: the test prints these)
    J('tools_md5.json', OrderedDict((n, U.md5_file(U.TOOLS / n)) for n in (
        'ux_base.py', 'ux_build_assets.py', 'ux_tuff.py', 'ux_water.py', 'ux_screens.py', 'ux_ads.py', 'ux_splitgate.py', 'ux_stocklib.py',
        'ux_build_track.py', 'ux_entities.py', 'ux_materials.py', 'ux_meshes.py', 'ux_textures.py', 'ux_common.py', 'ux_dirtlook.py',
        'ux_verify_race.py')))
    if 'base' in FAMILIES:
        base_family()
    if 'tuff' in FAMILIES:
        tuff_static()
    if 'dirtlook' in FAMILIES:
        dirtlook_static()
    per = [f for f in FAMILIES if f != 'base']
    if per:
        for disc, (key, o, argv) in commands().items():
            if ONLY and disc not in ONLY and U.level(disc).short not in ONLY:
                continue
            L = Level(disc, key, o)
            for fam in per:
                t = time.time()
                try:
                    globals()[fam + '_family'](L)
                except Exception as ex:                                  # noqa: BLE001
                    import traceback
                    J(L.rel('%s_error.json' % fam), OrderedDict(error='%s: %s' % (type(ex).__name__, ex), trace=traceback.format_exc()))
                    log(disc, fam, 'ERROR %s: %s' % (type(ex).__name__, ex))
    (OUT / 'done.txt').write_text('ok\n')
    log('done')


if __name__ == '__main__':
    main()

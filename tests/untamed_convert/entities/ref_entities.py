"""tests/untamed_convert/entities: reference documents of ux_entities.build_entities for option sets no stage output on disk covers.

  python -I -B ref_entities.py <out dir> [<py root>]

Runs the reference converter's ux_entities.build_entities(level, kind, write=False, **options) for every variant in VARIANTS and writes
<out dir>/<variant>/<Level>.json (ux_common.dumps_json of the document, as write_json would) and <Level>.<Sub>.chunks (the encoded
chunks, rebuilt from the returned Elems). Nothing is written outside <out dir>; the reference folder is only read (write=False, python -B).
The variant table is repeated in Program.cs (Variants): keep the two in step.
"""
import sys
from pathlib import Path

out = Path(sys.argv[1])
root = Path(sys.argv[2] if len(sys.argv) > 2 else __import__('os').environ['UX_PY_ROOT'])
sys.path.insert(0, str(root / 'tools'))
import ux_common as U  # noqa: E402
import ux_entities as UE  # noqa: E402

# name -> (levels, options); a manifest path 'missing/...' names a file that does not exist (the stage's stub for that manifest)
VARIANTS = [
    ('legacy', ['Norton_STADIUM', 'WestStone_STADIUM', 'Lincoln_STADIUM'],
     dict(crowd_tier='untamed', group_number='copy', hb_freeride='base', buried='keep', fr_clearance='legacy')),
    ('static-bales', ['Norton_STADIUM', 'Wilford_STADIUM'], dict(static_bales=True)),
    ('static-pop-initial', ['Norton_STADIUM'], dict(static_bales=True, static_pop='initial')),
    ('static-pop-drop', ['Denton_STADIUM'], dict(static_bales=True, static_pop='drop', crowd_off=True)),
    ('editor-male', ['Wilford_STADIUM', 'Fairview_STADIUM'], dict(static_male='editor_default')),
    ('gate-top', ['Foothills_STADIUM', 'Norton_STADIUM'], dict(drop_gate_top=True)),
    ('hb-freeride', ['Norton_STADIUM'], dict(hb_freeride='freeride')),
    ('renames', ['Wilford_STADIUM'], dict(renames={'material': {'MrGray': 'UX_MrGray'}, 'model': {'Stadiums_Box': 'UX_Stadiums_Box'},
                                                   'surface': {'Stadiums_Box': 'UX_Stadiums_Box_S'}, 'collis': {'Ambulance': 'UX_Ambulance'}})),
    ('reseat-columbus', ['Columbus_STADIUM'], dict(bale_reseat=6.0, bale_trigger_move=(12.5, -3.25))),
    ('stub-collision', ['Wilford_STADIUM', 'Lincoln_STADIUM'], dict(manifest_paths={'collision': 'missing/collision.json'})),
    ('stub-meshes', ['Norton_STADIUM'], dict(manifest_paths={'meshes': 'missing/meshes.json'})),
    ('stub-hf', ['Wilford_STADIUM', 'Lincoln_STADIUM', 'Denton_STADIUM'], dict(hf='missing/hf.npy', bale_reseat=6.0)),
    ('stub-all', ['Wilford_STADIUM', 'Fairview_STADIUM', 'BridgePort_STADIUM'],
     dict(manifest_paths={'meshes': 'missing/meshes.json', 'collision': 'missing/collision.json', 'terrain': 'missing/terrain.json',
                          'materials': 'missing/materials.json', 'hf': 'missing/hf.npy'})),
]

n = 0
for name, levels, opts in VARIANTS:
    for lv in levels:
        o = dict(opts)
        if 'manifest_paths' in o:
            o['manifest_paths'] = {k: U.work_dir(lv) / v for k, v in o['manifest_paths'].items()}
        if 'hf' in o:                                    # the stub height field: the file is not there
            o['hf'] = UE.HeightField(lv, path=U.work_dir(lv) / o['hf'])
        items, doc = UE.build_entities(lv, 'sx', write=False, **o)
        d = out / name
        d.mkdir(parents=True, exist_ok=True)
        (d / ('%s.json' % lv)).write_bytes(U.dumps_json(doc).encode('utf-8'))
        for sub in ('Base', 'FreeRide'):
            data = b''.join(UE.encode_chunks([e for s, e in items if s == sub]))
            assert U.md5_bytes(data) == doc['chunks'][sub]['md5']
            (d / ('%s.%s.chunks' % (lv, sub))).write_bytes(data)
        n += 1
        print('%s %s: %d specs, %d problems, %s' % (name, lv, len(items), len(doc['problems']),
                                                   'ok' if all(c['ok'] for c in doc['checks']) else 'checks fail'))
print('%d reference documents in %s' % (n, out))

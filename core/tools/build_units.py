#!/usr/bin/env python3
"""build_units.py <boot-coverage.bin> <world.bin> [out.tsv [front-end-coverage.bin]] -- the construction units of the race world and their functions.

Construction = functions that ran in the boot (REFLEX_COVERAGE of the baton boot that built the world) and do not run in race frames
(native/game/plan/frame-functions.tsv) and, given a front-end-only boot's coverage, not before the event (startup / front end). Each is assigned to the first unit (priority order below) whose roots reach it through direct
calls / tail jumps (E8 / E9 rel32 decoded from .text of the world image: more complete than Ghidra's call graph) or which owns it by
class (a vfunc of a class listed for the unit, Ghidra's `Class::vfN` names). Virtual calls are not followed: each unit lists the
classes whose vfuncs belong to it. Output: function, address, unit, size, boot instructions; a summary per unit on stdout.
"""
import bisect, collections, os, re, struct, sys, zlib
H = os.path.dirname(os.path.abspath(__file__))
R = os.path.join(H, '..', '..', '_re', 'out')
PLAN = os.path.join(H, '..', '..', 'native', 'game', 'plan')

# unit -> (root functions, classes whose vfuncs / ctors belong to it, name patterns). Priority = list order.
UNITS = [
 ('alloc',        ['0078fbe0', '00401f90', '004eef20', '00775540', '008236d0', '00823700'], ['StdMemoryAllocator', 'SimpleMemoryAllocator', 'IMemoryAllocator'], []),
 ('vehicle',      ['00542b90'], ['RavenVehicleMotocross', 'RavenSimulationMotocross', 'RavenVehicle', 'RavenSimulation', 'RavenVehicleATV',
                   'RavenSimulationATV', 'VehicleDamage', 'MXVehicleMoto', 'MainBodyNode', 'ConstraintSlidingRotor', 'ConstraintFixedBody',
                   'CollisionShapeDynamicRays', 'CollisionShapeCircle', 'WreckChecker'], ['^Raven', '^Vehicle']),
 ('rider',        ['0053e920', '0053eab0', '00624750'], ['RiderAnimation', 'RiderCharacter', 'Character', 'MotionAnimator', 'BlendAnimator',
                   'AdditiveAnimator', 'SwitchAnimator', 'InverseKinematicsAnimator', 'ProceduralAnimator', 'ExpressionBoneAnimator',
                   'ActionContext', 'PhysicsCharacter'], ['^Rider', 'Animator']),
 ('activity',     ['0075f890', '007665f0', '007669f0', '0055ef80', '0050ed80', '00566ea0', '00565e00', '005490f0', '00552ad0'],
                  ['MXActivityRace', 'MXActivityRaceStub', 'MXActivityFreeRide', 'RaceDriver', 'MXPlayer', 'RaceIntroScenario',
                   'RaceFinishScenario', 'RaceResultsScenario', 'BehaviorAvoidFlipBack', 'BehaviorSeperate', 'BehaviorLapPacing',
                   'BehaviorHandleBraking', 'BehaviorAirLeveling', 'BehaviorOfftrack', 'BehaviorNavigateTrack', 'BehaviorAvoidWreck',
                   'CrewChief', 'TrackType'], ['^MXActivity', '^Behavior', '^Race', 'Activity']),
 ('world',        ['00759f00'], ['MXWorld', 'PhysicsWorld', 'ProxyObjectHandle', 'StaticObjectHandle', 'PhysicsObjectHandle'], ['^MXWorld']),
 ('scene',        ['004046a0', '007d3e90', '007d3f00'], ['ModelEntity', 'CollisionEntity', 'PhysicsEntity', 'AnchorEntity',
                   'InstancedAnimationEntity', 'AnimEntitySingleAnimLoop', 'AnimEntitySplineAnimLoop', 'ScenarioAnimEntity', 'Entity',
                   'HFTerrainEntity', 'HFTerrainSurface', 'DeformablePatch', 'TerrainTracks', 'BillboardCrowdEntity', 'PopUpBale',
                   'PopDownBale', 'DropGate', 'SubScene', 'Scene', 'SceneManager'], ['Entity$', '^HFTerrain', '^Scene', '^SubScene']),
 ('collision',    ['00844360', '008446e0', '00848cd0', '00818ef0', '00818f30'], ['CollisionShapeStaticComposite', 'CollisionShapeStaticBox',
                   'CollisionShapeStaticCylinder', 'CollisionShapeSphere', 'CollisionShapeTriangles', 'CollisionShapeHandle',
                   'CollisionAsset', 'XenonTriangleSurface'], ['^CollisionShape', '^Collision']),
 ('asset_anim',   ['0082cc70', '0082ccc0', '0082b460', '0082b490'], ['AnimAsset', 'AnimAssetHandle', 'ActionAsset'], []),
 ('asset_model',  ['007d53c0', '007d5410'], ['ModelAsset', 'ModelAssetHandle'], []),
 ('asset_terrain',['006d8900', '006d8970', '006d8bd0', '006d8c30'], ['TerrainAsset', 'TerrainCollisionAsset'], []),
 ('asset_data',   ['008221f0', '00822280', '007f8f30', '007f8f70', '004f0170', '004f01f0', '0053a0c0', '0053a110'],
                  ['BXMLAsset', 'BXMLAssetHandle', 'PackedDataAsset', 'TableDataAsset', 'TDFAsset'], []),
 ('asset_forest', ['00661f10', '00661f50', '006623a0', '006623f0', '00668aa0', '00668af0', '00671ee0', '00671f50'],
                  ['ForestAsset', 'ForestSystem', 'TreeAsset', 'GrassAsset'], ['^Forest', '^Tree', '^Grass']),
 ('fx',           ['006940a0', '006940e0', '007e3a10', '007e3a50'], ['ParticleEmitter', 'PlaceableParticleEffect', 'FireworksParticleEffect',
                   'GenericParticleController', 'ParticleAsset', 'PlatformParticleManager2D'], ['Particle']),
 ('camera',       ['0063dc30', '007909f0'], ['Camera', 'MXThirdPersonCamera', 'MXReplayTowerCamera', 'MXCameraPath'], ['Camera']),
 ('render_asset', ['00793e70', '00793ea0', '00793ed0', '00794750', '00794780', '007947b0', '007947e0', '00794810', '00794860', '007a10d0'],
                  ['TextureAsset', 'XenonMaterialAsset', 'XenonShaderAsset', 'MaterialAssetHandle', 'XenonSurfaceAsset'], ['^Xenon', 'Texture', 'Shader']),
 ('audio',        ['00808830', '00808880', '00808a60', '00808ab0', '00808b20', '00808b50'], ['FMODEvent', 'VehicleAudioManager'], ['Audio', 'Sound', 'FMOD']),
 ('ui',           [], [], ['^UI', '^MXUI']),
]

def load_image(path):
    b = open(path, 'rb').read()
    magic, ver, n, sim = struct.unpack_from('<4I', b, 0)
    pages = {}
    for k in range(n):
        a, off, size = struct.unpack_from('<3I', b, 80 + 12 * k)
        if 0x401000 <= a < 0x916000:
            pages[a] = bytes(4096) if size == 0 else b[off:off + 4096] if size == 4096 else zlib.decompress(b[off:off + size])
    return b''.join(pages.get(a, bytes(4096)) for a in range(0x401000, 0x916000, 0x1000))

def main():
    cov = open(sys.argv[1], 'rb').read(); counts = memoryview(cov)[16:].cast('I')
    text = load_image(sys.argv[2])
    F = []
    for l in open(os.path.join(R, 'functions.tsv'), encoding='latin-1').readlines()[1:]:
        c = l.rstrip('\n').split('\t'); F.append((int(c[0], 16), c[1], int(c[3] or 0)))
    F.sort(); starts = [f[0] for f in F]; name = {f[0]: f[1] for f in F}; size = {f[0]: f[2] for f in F}
    ran = {a for a, _, _ in F if 0 <= a - 0x400000 < len(counts) and counts[a - 0x400000]}
    instr = {a: sum(counts[a - 0x400000 + i] for i in range(max(size[a], 1)) if a - 0x400000 + i < len(counts)) for a in ran}
    frame = set()
    for l in open(os.path.join(PLAN, 'frame-functions.tsv')).readlines()[1:]: frame.add(int(l.split('\t')[0], 16))
    cons = ran - frame
    fe = set()
    if len(sys.argv) > 4:   # a front-end-only boot's coverage: functions it runs are startup / front end, not the event's construction
        fc = memoryview(open(sys.argv[4], 'rb').read())[16:].cast('I')
        fe = {a for a in ran if fc[a - 0x400000]}
        print('front-end boot ran %d of them' % len(ran & fe))
        cons = cons - fe
    # direct calls / jumps per function (rel32 targets that are function entries that ran)
    succ = collections.defaultdict(set)
    for a in ran:
        lo = a - 0x401000; hi = lo + max(size[a], 1)
        for m in re.finditer(b'[\xe8\xe9]', text[lo:hi]):
            o = lo + m.start()
            if o + 5 > len(text): continue
            t = (0x401000 + o + 5 + struct.unpack_from('<i', text, o + 1)[0]) & 0xffffffff
            if t in ran and t != a: succ[a].add(t)
    vfn = re.compile(r'^(.*)::(vf\d+(_o[0-9a-f]+)?|sdtor|ctor)$')
    unit_of = {}; why = {}
    for u, roots, classes, pats in UNITS:
        seeds = {int(r, 16) for r in roots} & ran
        cl = set(classes); pr = [re.compile(p) for p in pats]
        for a in cons:
            m = vfn.match(name.get(a, ''))
            if m:
                c = m.group(1).split('::')[-1]
                if c in cl or any(p.search(c) for p in pr): seeds.add(a)
        stack = list(seeds); seen = set(seeds)
        while stack:
            x = stack.pop()
            for t in succ.get(x, ()):
                if t not in seen and t in cons: seen.add(t); stack.append(t)
        for a in seen:
            if a in cons and a not in unit_of: unit_of[a] = u; why[a] = 'root' if a in seeds else 'reached'
    for a in cons:
        if a not in unit_of: unit_of[a] = 'other'
    per = collections.defaultdict(lambda: [0, 0, 0])
    for a in cons: per[unit_of[a]][0] += 1; per[unit_of[a]][1] += size[a]; per[unit_of[a]][2] += instr[a]
    print('boot functions %d, also in race frames %d, construction-only %d' % (len(ran), len(ran & frame), len(cons)))
    print('%-14s %6s %9s %14s' % ('unit', 'funcs', 'bytes', 'boot instr'))
    for u, v in sorted(per.items(), key=lambda kv: -kv[1][1]): print('%-14s %6d %9d %14d' % (u, *v))
    if len(sys.argv) > 3:
        with open(sys.argv[3], 'w') as f:
            f.write('addr\tname\tunit\tsize\tboot_instructions\thow\n')
            for a in sorted(cons, key=lambda a: (unit_of[a], a)):
                f.write('%08x\t%s\t%s\t%d\t%d\t%s\n' % (a, name[a], unit_of[a], size[a], instr[a], why.get(a, '')))

if __name__ == '__main__':
    main()

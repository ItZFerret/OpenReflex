"""Compare continuous full-state integration with the independent Python bridges.

ARM writes per-update hashes only after comparing all 38,656 state bytes. Intel
then runs the same input scenarios against those hashes. Neither uses recorded
motion after construction. This is adapter/composition evidence, not an original
executable whole-step oracle.
"""
import argparse
import ctypes as C
import hashlib
import json
import platform
import struct
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
BASE = Path(__file__).resolve().parent
FIELDS = ('throttle', 'reverse', 'front_brake', 'rear_brake', 'steer_x',
          'steer_y', 'rider_x', 'rider_y')


class Input(C.Structure):
    _fields_ = [(name, C.c_float) for name in FIELDS] + [
        ('clutch', C.c_uint32), ('substeps', C.c_uint32), ('world_dt', C.c_float)]


def scenarios():
    def value(**kwargs):
        return dict.fromkeys(FIELDS, 0.0) | kwargs
    # Start each independent scenario at rest. Stay inside the finite pad.
    yield 'idle', [value() for _ in range(220)]
    yield 'throttle', [value(throttle=float(i >= 30)) for i in range(220)]
    yield 'front_brake', [value(throttle=float(30 <= i < 130), front_brake=float(i >= 130)) for i in range(220)]
    yield 'rear_brake', [value(throttle=float(30 <= i < 130), rear_brake=float(i >= 130)) for i in range(220)]
    for direction in (-1, 1):
        yield 'steer_' + str(direction), [value(throttle=float(i >= 30), steer_x=direction * .2 if i >= 90 else 0) for i in range(170)]
        yield 'rider_' + str(direction), [value(throttle=float(i >= 30), rider_x=direction * .1 if 90 <= i < 120 else 0) for i in range(150)]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--library', type=Path, required=True)
    parser.add_argument('--expected', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    expected = json.loads(args.expected.read_text()) if args.expected else None
    if expected is None:
        sys.path.insert(0, str(ROOT / '_re/trace'))
        from bike_playable_runtime import BikeRuntime
        import bike_step_bridges as bridge
        source = json.loads((ROOT / '_re/trace/build/bike-playable-seed.json').read_text())
    lib = C.CDLL(str(args.library.resolve()))
    lib.reflex_test_bike_create.argtypes = [C.c_void_p, C.c_uint32]
    lib.reflex_test_bike_create.restype = C.c_void_p
    lib.reflex_test_bike_destroy.argtypes = [C.c_void_p]
    lib.reflex_test_bike_update.argtypes = [C.c_void_p, C.POINTER(Input)]
    lib.reflex_test_bike_update.restype = C.c_uint32
    lib.reflex_test_bike_state.argtypes = [C.c_void_p, C.c_void_p, C.c_uint32]
    lib.reflex_test_bike_error.argtypes = [C.c_void_p]
    lib.reflex_test_bike_error.restype = C.c_char_p
    seed = (BASE / 'bike-test-seed.bin').read_bytes()
    output = C.create_string_buffer(38656)
    results = {}
    started = time.monotonic()
    for name, sequence in scenarios():
        context = lib.reflex_test_bike_create(seed, len(seed))
        assert context, lib.reflex_test_bike_error(None)
        reference = None
        if expected is None:
            reference = BikeRuntime(source)
            reference.contacts()
        hashes = []
        samples = []
        try:
            for index, actions in enumerate(sequence):
                request = Input(**actions, clutch=0, substeps=2, world_dt=.015)
                assert lib.reflex_test_bike_update(context, C.byref(request)) == 0, (name, index, lib.reflex_test_bike_error(context))
                assert lib.reflex_test_bike_state(context, output, len(output)) == 0
                state = output.raw
                if reference is not None:
                    reference.sample_input(actions['throttle'], actions['steer_x'], actions['rider_x'],
                        actions['reverse'], actions['front_brake'], actions['rear_brake'], actions['steer_y'], actions['rider_y'])
                    reference.step(); reference.step()
                    reference.state = bytearray(bridge.scheduler_post(bytes(reference.state), bytes(reference.control), struct.unpack('<I', struct.pack('<f', .015))[0]))
                    assert state == reference.state, (name, index, 'full state differs')
                digest = hashlib.sha256(state).hexdigest()
                if expected is not None:
                    assert digest == expected['scenarios'][name]['state_sha256'][index], (name, index, 'architecture differs')
                hashes.append(digest)
                position = struct.unpack_from('<3f', state, 0xf0)
                assert abs(position[0]) < 140 and abs(position[2]) < 140, (name, index, position)
                if index in (0, 30, 90, 130, len(sequence)-1):
                    samples.append(dict(update=index, position=position, speed=struct.unpack_from('<f', state, 0x94)[0], gear=struct.unpack_from('<i', state, 0x8ac0)[0]))
            results[name] = dict(world_updates=len(sequence), substeps=len(sequence)*2, state_sha256=hashes, samples=samples)
            print(name, len(sequence), 'matched', samples[-1], flush=True)
        finally:
            lib.reflex_test_bike_destroy(context)
    assert len({row['state_sha256'][-1] for row in results.values()}) == len(results), 'Input scenarios did not diverge'
    report = dict(success=True, architecture=platform.machine(),
                  claim='Continuous composition comparison; not an original whole-step oracle',
                  library_sha256=hashlib.sha256(args.library.read_bytes()).hexdigest(),
                  seed_sha256=hashlib.sha256(seed).hexdigest(),
                  elapsed_seconds=time.monotonic()-started, scenarios=results,
                  world_updates=sum(row['world_updates'] for row in results.values()),
                  substeps=sum(row['substeps'] for row in results.values()))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps({key: value for key, value in report.items() if key != 'scenarios'}))


if __name__ == '__main__':
    main()

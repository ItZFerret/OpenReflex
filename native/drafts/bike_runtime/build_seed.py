"""Rebuild the initial test-world state; contains no future motion frames."""
import hashlib
import json
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / '_re/trace'))
from bike_playable_runtime import BikeRuntime


def encode(bike):
    row = struct.pack('<7I5f', 0x4b425852, 1, bike.dt, bike.profile,
                      bike.nx, bike.nz, bike.stride, bike.extent, bike.extent,
                      bike.spacing, bike.gain, bike.normal)
    row += bike.state + bike.control + b''.join(bike.filters)
    row += struct.pack('<I', len(bike.runtime))
    for address, value in sorted(bike.runtime.items()):
        row += struct.pack('<II', address, len(value)) + value
    row += bike.base + bike.deform + bike.bounds + bike.triangles
    row += struct.pack('<I', len(bike.regions))
    for region in bike.regions:
        row += struct.pack('<III', region['address'], len(region['data']),
                           int(region['writable'])) + region['data']
    return row


if __name__ == '__main__':
    source = ROOT / '_re/trace/build/bike-playable-seed.json'
    data = encode(BikeRuntime(json.loads(source.read_text())))
    target = Path(__file__).with_name('bike-test-seed.bin')
    target.write_bytes(data)
    print(json.dumps({'path': str(target), 'bytes': len(data),
                      'sha256': hashlib.sha256(data).hexdigest()}))

#!/usr/bin/env python3
"""gen_compile_commands.py -- writes core/compile_commands.json so editors (clangd) see the core's include paths: C11 for
src/ game/ api/ tools/*.c (product flags), C++17 for oracle/*.cpp and src/*.cpp (oracle / helper flags). Re-run after adding files."""
import glob, json, os
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
cap = os.path.join(root, '..', '_re', '.venv', 'lib', 'python3.14', 'site-packages', 'capstone', 'include')
inc = ['-Iinclude', '-Isrc', '-Igame', '-I../native/game', '-I../native/drafts/world_runtime', '-I' + cap]
out = []
for f in sorted(glob.glob(os.path.join(root, '*', '*.c')) + glob.glob(os.path.join(root, 'src', 'f80', '*.cpp')) + glob.glob(os.path.join(root, '*', '*.cpp'))):
    rel = os.path.relpath(f, root)
    if f.endswith('.c'):
        args = ['clang', '-std=c11', '-fno-fast-math', '-ffp-contract=off', '-fwrapv'] + inc + ['-c', rel]
        if rel.startswith('oracle'): args.insert(2, '-DRX_ORACLE')
    else:
        args = ['clang++', '-std=c++17', '-DRX_ORACLE'] + inc + ['-c', rel]
    out.append({'directory': root, 'file': rel, 'arguments': args})
json.dump(out, open(os.path.join(root, 'compile_commands.json'), 'w'), indent=1)
print(len(out), 'entries')

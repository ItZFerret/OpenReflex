#!/usr/bin/env python3
"""Build the current client with the opt-in Freestyle arena probe (FreestyleArenaProbe.cs) in an isolated staged project (batch mode only).

    FSARENA_CLIENT=client-a python3 tests/freestyle_arena/build.py      -> builds/fsarena/client-a/FreestyleArenaTest.app

A new client folder is seeded (APFS clone, imported Library included) from the first existing staged project so the batch build does
not reimport everything; delete builds/fsarena/<client>/Reflex (the big Library) when done. Never opens the user's editor.
"""
import os, shutil, subprocess
from pathlib import Path
R = Path(__file__).resolve().parents[2]
out = R / 'builds/fsarena' / os.environ.get('FSARENA_CLIENT', 'client-current'); stage = out / 'Reflex'; out.mkdir(parents=True, exist_ok=True)
if not stage.exists():
    seeds = sorted((R / 'builds/fsarena').glob('client-*/Reflex'), key=lambda p: p.stat().st_mtime, reverse=True)
    seeds += sorted((R / 'builds/alive').glob('client-*/Reflex'), key=lambda p: p.stat().st_mtime, reverse=True)
    seeds += [R / 'builds/sliderock_canyon/client-wsr/Reflex', R / 'builds/player/stage/Reflex']
    seed = next(p for p in seeds if p.is_dir() and p != stage)
    subprocess.run(['/bin/cp', '-cR', str(seed), str(stage)], check=True)
subprocess.run(['rsync', '-a', '--delete', '--exclude', 'Temp/', '--exclude', 'Logs/', '--exclude', 'Builds/', '--exclude', 'Library/',
                str(R / 'Unity/Reflex') + '/', str(stage) + '/'], check=True)
S = stage / 'Assets/Scripts'
shutil.copy2(R / 'tests/freestyle_arena/FreestyleArenaProbe.cs', S / 'OpenFrontEnd/FreestyleArenaProbe.cs')
p = S / 'OriginalWorld/OriginalWorldRuntime.cs'; s = p.read_text()
for old, new in [('#if UNITY_EDITOR\n        [Tooltip("Editor tests: DirectInput scan codes held in addition to the keyboard.")]\n        public int[] testHeldKeys = Array.Empty<int>();\n#endif', '        public int[] testHeldKeys = Array.Empty<int>();'),
                 ('#if UNITY_EDITOR\n            foreach (int k in testHeldKeys) keys[k & 255] = 0x80;\n#endif', '            foreach (int k in testHeldKeys) keys[k & 255] = 0x80;')]:
    assert s.count(old) == 1, old; s = s.replace(old, new)
# the probe owns the input: the user's keyboard / pads never reach the test player
s = s.replace('            foreach (int k in testHeldKeys) keys[k & 255] = 0x80;', '            if (Environment.GetEnvironmentVariable("REFLEX_FSARENA_TEST") == "1") Array.Clear(keys, 0, keys.Length);\n            foreach (int k in testHeldKeys) keys[k & 255] = 0x80;')
s = s.replace('uint mask = ReadPads(!menu && !paused);', 'uint mask = ReadPads(!menu && !paused); if (Environment.GetEnvironmentVariable("REFLEX_FSARENA_TEST") == "1") { mask = 0; Array.Clear(pads, 0, pads.Length); }')
p.write_text(s)
env = dict(os.environ, REFLEX_RXCORE_PATH=str(R / 'core/build/librxcore.dylib'), REFLEX_OPEN_PLAYER=str(out / 'FreestyleArenaTest.app'))
r = subprocess.run(['/Users/payton/.unity/bin/unity', 'run', str(stage), '--timeout', '2400', '--log-file', str(out / 'build.log'), '--no-tail', '--',
                    '-nographics', '-executeMethod', 'Reflex.Port.EditorTools.OpenReflexBuild.BuildMacBatch'], env=env)
raise SystemExit(r.returncode)

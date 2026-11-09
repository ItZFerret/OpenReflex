#!/usr/bin/env python3
"""Build the current client with the opt-in Alive ride probe (AliveRideProbe.cs) in an isolated staged project (batch mode only).

    ALIVE_CLIENT=client-a1 python3 tests/alive_tracks/build.py      -> builds/alive/client-a1/AliveTest.app
    ALIVE_TREE=HEAD ALIVE_CLIENT=client-a0 ...  the Unity project of HEAD (a "before" client) instead of the working tree

A new client folder is seeded (APFS clone, imported Library included) from the first existing staged project so the batch build does
not reimport everything. Never opens the user's editor.
"""
import os, shutil, subprocess
from pathlib import Path
R = Path(__file__).resolve().parents[2]
out = R / 'builds/alive' / os.environ.get('ALIVE_CLIENT', 'client-current'); stage = out / 'Reflex'; out.mkdir(parents=True, exist_ok=True)
if not stage.exists():
    seeds = sorted((R / 'builds/alive').glob('client-*/Reflex'), key=lambda p: p.stat().st_mtime, reverse=True)
    seeds += [R / 'builds/sliderock_canyon/client-wsr/Reflex', R / 'builds/player/stage/Reflex']
    seed = next(p for p in seeds if p.is_dir() and p != stage)
    subprocess.run(['/bin/cp', '-cR', str(seed), str(stage)], check=True)
src = R / 'Unity/Reflex'
if os.environ.get('ALIVE_TREE'):   # a before / after pair: the Unity project of this commit (git archive) instead of the working tree; the probes below are the working tree's
    import io, tarfile
    tree = out / 'tree'; shutil.rmtree(tree, ignore_errors=True); tree.mkdir()
    tarfile.open(fileobj=io.BytesIO(subprocess.run(['git', '-C', str(R), 'archive', os.environ['ALIVE_TREE'], 'Unity/Reflex'], check=True, capture_output=True).stdout)).extractall(tree)
    src = tree / 'Unity/Reflex'
subprocess.run(['rsync', '-a', '--delete', '--exclude', 'Temp/', '--exclude', 'Logs/', '--exclude', 'Builds/', '--exclude', 'Library/',
                str(src) + '/', str(stage) + '/'], check=True)
S = stage / 'Assets/Scripts'
shutil.copy2(R / 'tests/alive_tracks/AliveRideProbe.cs', S / 'OpenFrontEnd/AliveRideProbe.cs')
shutil.copy2(R / 'tests/alive_tracks/AliveImportProbe.cs', S / 'OpenFrontEnd/AliveImportProbe.cs')   # run_import.py (REFLEX_ALIVE_IMPORT=1)
p = S / 'OriginalWorld/OriginalWorldRuntime.cs'; s = p.read_text()
for old, new in [('#if UNITY_EDITOR\n        [Tooltip("Editor tests: DirectInput scan codes held in addition to the keyboard.")]\n        public int[] testHeldKeys = Array.Empty<int>();\n#endif', '        public int[] testHeldKeys = Array.Empty<int>();'),
                 ('#if UNITY_EDITOR\n            foreach (int k in testHeldKeys) keys[k & 255] = 0x80;\n#endif', '            foreach (int k in testHeldKeys) keys[k & 255] = 0x80;')]:
    assert s.count(old) == 1, old; s = s.replace(old, new)
# the probe owns the input: the user's keyboard / pads never reach the test player
s = s.replace('            foreach (int k in testHeldKeys) keys[k & 255] = 0x80;', '            if (Environment.GetEnvironmentVariable("REFLEX_ALIVE_TEST") == "1") Array.Clear(keys, 0, keys.Length);\n            foreach (int k in testHeldKeys) keys[k & 255] = 0x80;')
s = s.replace('uint mask = ReadPads(!menu && !paused);', 'uint mask = ReadPads(!menu && !paused); if (Environment.GetEnvironmentVariable("REFLEX_ALIVE_TEST") == "1") { mask = 0; Array.Clear(pads, 0, pads.Length); }')
p.write_text(s)
env = dict(os.environ, REFLEX_RXCORE_PATH=str(R / 'core/build/librxcore.dylib'), REFLEX_OPEN_PLAYER=str(out / 'AliveTest.app'))
r = subprocess.run(['/Users/payton/.unity/bin/unity', 'run', str(stage), '--timeout', '2400', '--log-file', str(out / 'build.log'), '--no-tail', '--',
                    '-nographics', '-executeMethod', 'Reflex.Port.EditorTools.OpenReflexBuild.BuildMacBatch'], env=env)
raise SystemExit(r.returncode)

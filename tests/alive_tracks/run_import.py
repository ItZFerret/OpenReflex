#!/usr/bin/env python3
"""End-to-end acceptance of the in-client MX vs ATV Alive importer, driven through the real UI, then a ride of an imported level.

    ALIVE_CLIENT=client-import python3 tests/alive_tracks/run_import.py [--levels FR_Dunes,FR_DLC_JSComp] [--ride FR_Dunes]
        [--source "/Users/payton/Downloads/MX vs. ATV Alive (USA, Europe).iso" --source /Users/payton/Downloads/5451087B]

The player (built by build.py, which injects AliveImportProbe + AliveRideProbe) gets a sandboxed content folder: a fresh tree of
symbolic links to the base Reflex install (real Database/ and Assets/<type>/ folders holding links, as alive_install.link_base), which
the importer writes its new real files into. AliveImportProbe drives hub GAME LIBRARY -> MX vs ATV ALIVE -> the sources -> FIND LEVELS ->
the chosen levels -> IMPORT -> the results -> the hub; AliveRideProbe then rides --ride from the hub's MX vs ATV Alive entry.
Pass: both probes pass, every level imported, the player's sources and the base install unchanged (file list, sizes, mtimes), and no
imported file is a link or lies outside the sandbox. Writes builds/alive/runs/<UTC>-import-<ride>/ (logs, screenshots, RESULT.json);
the sandbox content folder is removed afterwards unless --keep-content.
Same isolation as run.py: separate app identity, sandboxed save, the test window never takes the focus (tests/nofocus.py), 60 fps cap,
writes to the protected profile locations, the base install and the sources denied by a sandbox profile.
"""
import argparse, datetime, hashlib, json, os, plistlib, shutil, subprocess, sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1])); import nofocus
R = Path(__file__).resolve().parents[2]
ap = argparse.ArgumentParser()
ap.add_argument('--levels', default='FR_Dunes,FR_DLC_JSComp'); ap.add_argument('--ride', default='FR_Dunes')
ap.add_argument('--source', action='append', type=Path)
ap.add_argument('--base', type=Path, default=R / 'builds/sol_garage/game files')
ap.add_argument('--keep-content', action='store_true')
a = ap.parse_args()
sources = [s.resolve() for s in (a.source or [Path('/Users/payton/Downloads/MX vs. ATV Alive (USA, Europe).iso'), Path('/Users/payton/Downloads/5451087B')])]
levels = a.levels.split(',')
app = R / 'builds/alive' / os.environ.get('ALIVE_CLIENT', 'client-current') / 'AliveTest.app'
assert app.is_dir(), app
base = a.base.resolve(); assert (base / 'reflex-extracted-v1.txt').is_file(), 'base must be an extracted Reflex install'
for l in levels: assert not (base / 'Database' / (l + '.dx9.database')).exists(), f'base already has {l}'
# the run folder by its real path: the importer refuses a content folder reached through a link (builds/alive is one)
out = (R / 'builds/alive/runs').resolve() / (datetime.datetime.utcnow().strftime('%Y%m%dT%H%M%SZ') + '-import-' + a.ride); out.mkdir(parents=True)

# --- the sandbox content folder: links to the base, real folders where the importer writes
content = out / 'content'; content.mkdir()
for p in base.iterdir():
    if p.name not in ('Database', 'Assets'): (content / p.name).symlink_to(p)
(content / 'Database').mkdir(); (content / 'Assets').mkdir()
for p in (base / 'Database').iterdir(): (content / 'Database' / p.name).symlink_to(p)
for typ in (base / 'Assets').iterdir():
    if not typ.is_dir(): (content / 'Assets' / typ.name).symlink_to(typ); continue
    d = content / 'Assets' / typ.name; d.mkdir()
    for p in typ.iterdir(): (d / p.name).symlink_to(p)

def listing(root):
    """(path, size, mtime) of every file under root (or of root itself): the player's sources must stay as they are."""
    root = Path(root)
    if root.is_file(): st = root.stat(); return hashlib.sha256(f'{root}|{st.st_size}|{st.st_mtime_ns}'.encode()).hexdigest()
    h = hashlib.sha256()
    for q in sorted(root.rglob('*')):
        st = q.lstat(); h.update(f'{q.relative_to(root)}|{st.st_size}|{st.st_mtime_ns}|{q.is_symlink()}\n'.encode())
    return h.hexdigest()
watched = sources + [base / 'Database', base / 'Assets']
before = {str(p): listing(p) for p in watched}

copy = out / 'AliveTest.app'; shutil.copytree(app, copy, symlinks=True)
p = copy / 'Contents/Info.plist'; info = plistlib.loads(p.read_bytes()); original = info['CFBundleIdentifier']
info.update(CFBundleIdentifier='local.reflex.alive.test-' + out.name.lower().replace('_', '-'), CFBundleName='Alive Import Test', LSAppNapIsDisabled=True); p.write_bytes(plistlib.dumps(info))
exe = copy / 'Contents/MacOS' / info['CFBundleExecutable']
save = out / 'save'; save.mkdir(); shots = out / 'screenshots'; shots.mkdir()
support = Path.home() / 'Library/Application Support'; prefs = Path.home() / 'Library/Preferences'
protect = [support / 'DefaultCompany/Reflex', support / 'DefaultCompany/OpenReflex', support / 'DefaultCompany/Reflex Core Garage Bench', support / original,
           prefs / (original + '.plist'), base] + sources
sb = out / 'protection.sb'; sb.write_text('(version 1)\n(allow default)\n' + ''.join('(deny file-write* (subpath ' + json.dumps(str(p)) + '))\n' for p in protect))
subprocess.run(['codesign', '--force', '--sign', '-', str(copy)], check=True, capture_output=True)
env = {k: v for k, v in os.environ.items() if not k.startswith(('REFLEX_', 'RX_', 'DYLD_'))}
env.update(REFLEX_SIM='open', REFLEX_FRONTEND='open', REFLEX_SAVE_DIR=str(save), REFLEX_GAME_DIR=str(content), REFLEX_ALIVE_TEST='1', REFLEX_ALIVE_LEVEL=a.ride,
           REFLEX_ALIVE_IMPORT='1', REFLEX_ALIVE_IMPORT_SOURCES='|'.join(str(s) for s in sources), REFLEX_ALIVE_IMPORT_LEVELS=','.join(levels),
           REFLEX_PREVIEW_SHOTS=str(shots), REFLEX_FPS_CAP='60', REFLEX_PACING='interpolated', REFLEX_ALIVE_OFFSCREEN='1', REFLEX_NO_WELCOME='1')
args = ['/usr/bin/sandbox-exec', '-f', str(sb), str(exe), '-reflexTestWindow', '-screen-fullscreen', '0', '-screen-width', '1280', '-screen-height', '720', '-logFile', str(out / 'player.log')]
(out / 'plan.json').write_text(json.dumps({'argv': args, 'env': {k: v for k, v in env.items() if k.startswith('REFLEX_')}}, indent=2))
with (out / 'console.log').open('w') as log:
    front = nofocus.frontmost()
    proc = subprocess.Popen(args, cwd=R, env=env, stdout=log, stderr=subprocess.STDOUT); print(str(out), flush=True)
    nofocus.hand_back(front)
    try: code = proc.wait(timeout=1800)
    except subprocess.TimeoutExpired: proc.terminate(); proc.wait(timeout=10); code = 124
text = (out / 'player.log').read_text(errors='replace') if (out / 'player.log').exists() else ''
after = {str(p): listing(p) for p in watched}; unchanged = before == after

# what the importer wrote: real files inside the sandbox only
written, links, outside = [], [], []
for l in levels:
    m = content / 'Database' / f'alive-import-{l}.json'
    if not m.is_file(): continue
    for f in json.loads(m.read_text())['files']:
        q = content / f['path']; written.append(f['path'])
        rel = Path(f['path']); chain = [content / rel] + [content / par for par in rel.parents if str(par) != '.']
        if any(c.is_symlink() for c in chain): links.append(f['path'])
        if not str(q.resolve()).startswith(str(content) + os.sep): outside.append(f['path'])
registry = json.loads((content / 'Database/alive-levels.json').read_text()) if (content / 'Database/alive-levels.json').is_file() else {'levels': []}
installed = [e['level'] for e in registry['levels']]
required = ['main-menu', 'alive-category', 'setup', 'spawn', 'ride', 'ride-end', 'vista', 'top']
lines = [l for l in text.splitlines() if '[alive-test]' in l]
checks = {'exit_code_0': code == 0, 'import_done': '[alive-test] IMPORT DONE' in text, 'ride_pass': '[alive-test] PASS' in text,
          'no_fail': '[alive-test] FAIL' not in text, 'sources_and_base_unchanged': unchanged, 'all_levels_installed': all(l in installed for l in levels),
          'imported_files_real': bool(written) and not links and not outside, 'ride_screenshots': all((shots / (n + '.png')).exists() for n in required)}
ok = all(checks.values())
(out / 'RESULT.json').write_text(json.dumps({'passed': ok, 'checks': checks, 'levels': levels, 'ride': a.ride, 'sources': [str(s) for s in sources], 'exit_code': code,
                                             'registry_levels': installed, 'imported_files': len(written), 'linked_files': links[:20], 'outside_files': outside[:20],
                                             'screenshots': sorted(p.name for p in shots.glob('*.png')), 'probe': lines[-60:]}, indent=2))
shutil.rmtree(copy, ignore_errors=True)
if not a.keep_content: shutil.rmtree(content, ignore_errors=True)
print('PASS' if ok else 'FAIL', out); raise SystemExit(0 if ok else 1)

#!/usr/bin/env python3
"""Rendered real-player ride acceptance of one converted Alive level, with a separate app identity, sandboxed save and no focus use.

    ALIVE_CLIENT=client-a1 python3 tests/alive_tracks/run.py OVERLAY LEVEL [--copy-shots "~/Desktop/Reflex Review/Alive Tracks/<name>"]

OVERLAY is an alive_install.py output (builds/alive/...). Writes builds/alive/runs/<UTC>-<LEVEL>/ (player log, screenshots, RESULT.json).
The test player never activates its window (tests/nofocus.py hands the focus back), runs at a 60 fps cap with VSync off and
interpolated pacing, and its writes to the protected profile locations and the base install are denied by a sandbox profile.
"""
import argparse, datetime, hashlib, json, os, plistlib, shutil, subprocess, sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1])); import nofocus
R = Path(__file__).resolve().parents[2]
ap = argparse.ArgumentParser(); ap.add_argument('overlay', type=Path); ap.add_argument('level'); ap.add_argument('--copy-shots', type=Path)
ap.add_argument('--batch', action='store_true', help='batch mode (no window; the menu world stalls there: diagnostics only)')
ap.add_argument('--presented', action='store_true', help='present every frame and capture the window (needs the test window visible: otherwise ~1 fps)')
ap.add_argument('--env', action='append', default=[], metavar='K=V', help='an extra REFLEX_* variable for the test player (comparison renders)')
a = ap.parse_args()
app = R / 'builds/alive' / os.environ.get('ALIVE_CLIENT', 'client-current') / 'AliveTest.app'
assert app.is_dir(), app
map_dir = a.overlay.resolve()
assert (map_dir / 'Database/alive-levels.json').is_file(), 'not an Alive overlay'
out = R / 'builds/alive/runs' / (datetime.datetime.utcnow().strftime('%Y%m%dT%H%M%SZ') + '-' + a.level); out.mkdir(parents=True)
copy = out / 'AliveTest.app'; shutil.copytree(app, copy, symlinks=True)
p = copy / 'Contents/Info.plist'; info = plistlib.loads(p.read_bytes()); original = info['CFBundleIdentifier']
# LSAppNapIsDisabled: a background (never activated, possibly occluded) test player must not be timer-throttled by App Nap
info.update(CFBundleIdentifier='local.reflex.alive.test-' + out.name.lower().replace('_', '-'), CFBundleName='Alive Ride Test', LSAppNapIsDisabled=True); p.write_bytes(plistlib.dumps(info))
exe = copy / 'Contents/MacOS' / info['CFBundleExecutable']
save = out / 'save'; save.mkdir(); shots = out / 'screenshots'; shots.mkdir()
support = Path.home() / 'Library/Application Support'; prefs = Path.home() / 'Library/Preferences'
protect = [support / 'DefaultCompany/Reflex', support / 'DefaultCompany/OpenReflex', support / 'DefaultCompany/Reflex Core Garage Bench', support / original,
           prefs / (original + '.plist'), R / 'builds/sol_garage/game files']
def fingerprint(p):
    if p.is_file(): return hashlib.sha256(p.read_bytes()).hexdigest()
    if p.is_dir(): return {str(q.relative_to(p)): hashlib.sha256(q.read_bytes()).hexdigest() for q in p.rglob('*') if q.is_file()}
    return None
before = {str(p): fingerprint(p) for p in protect[:-1]}
sb = out / 'protection.sb'; sb.write_text('(version 1)\n(allow default)\n' + ''.join('(deny file-write* (subpath ' + json.dumps(str(p)) + '))\n' for p in protect))
subprocess.run(['codesign', '--force', '--sign', '-', str(copy)], check=True, capture_output=True)
env = {k: v for k, v in os.environ.items() if not k.startswith(('REFLEX_', 'RX_', 'DYLD_'))}
env.update(REFLEX_SIM='open', REFLEX_FRONTEND='open', REFLEX_SAVE_DIR=str(save), REFLEX_GAME_DIR=str(map_dir), REFLEX_ALIVE_TEST='1', REFLEX_ALIVE_LEVEL=a.level,
           REFLEX_PREVIEW_SHOTS=str(shots), REFLEX_FPS_CAP='60', REFLEX_PACING='interpolated', REFLEX_ALIVE_OFFSCREEN='0' if a.presented else '1',
           REFLEX_NO_WELCOME='1')   # the first-run MOTOCARD welcome would hold the fresh test profile before the hub
for kv in a.env:
    k, _, v = kv.partition('='); assert k.startswith('REFLEX_'), kv; env[k] = v
args = ['/usr/bin/sandbox-exec', '-f', str(sb), str(exe), '-reflexTestWindow', '-screen-fullscreen', '0', '-screen-width', '1280', '-screen-height', '720', '-logFile', str(out / 'player.log')]
if a.batch: args.insert(4, '-batchmode')
(out / 'plan.json').write_text(json.dumps({'argv': args, 'env': {k: v for k, v in env.items() if k.startswith('REFLEX_')},
                                           'core_sha256': hashlib.sha256((copy / 'Contents/PlugIns/librxcore.dylib').read_bytes()).hexdigest()}, indent=2))
with (out / 'console.log').open('w') as log:
    front = nofocus.frontmost()
    proc = subprocess.Popen(args, cwd=R, env=env, stdout=log, stderr=subprocess.STDOUT); print(str(out), flush=True)
    nofocus.hand_back(front)
    try: code = proc.wait(timeout=1500)
    except subprocess.TimeoutExpired: proc.terminate(); proc.wait(timeout=10); code = 124
text = (out / 'player.log').read_text(errors='replace') if (out / 'player.log').exists() else ''
after = {str(p): fingerprint(p) for p in protect[:-1]}; unchanged = before == after
required = ['main-menu', 'alive-category', 'setup', 'spawn', 'ride', 'ride-end', 'around-sun', 'around-away', 'sky-sun', 'vista', 'top']
ok = code == 0 and '[alive-test] PASS' in text and '[alive-test] FAIL' not in text and unchanged and all((shots / (n + '.png')).exists() for n in required)
lines = [l for l in text.splitlines() if '[alive-test]' in l]
# a reset that is not a wreck respawn passes only over the level's authored reset material (layer-0 zone value 7, as the native
# regression classifies it); the probe logs the bike position of the frame before the reset
import re
sys.path.insert(0, str(R / 'tools/formats'))
from alive_heights import zone_value_at
resets = []
for l in lines:
    m = re.search(r'RESET at frame (\d+) pos=([-\d.Ee]+),([-\d.Ee]+),([-\d.Ee]+)', l)
    if m:
        # the reset material under the bike: the logged centre is the frame before the reset and the core tests the wheels'
        # contacts, so the material within 6 ft counts (a 3 x 3 sample)
        x, z = float(m.group(2)), float(m.group(4))
        zones = {int(zone_value_at(map_dir, a.level, x + dx, z + dz)) for dx in (-6, 0, 6) for dz in (-6, 0, 6)}
        resets.append({'frame': int(m.group(1)), 'pos': [x, z], 'zones': sorted(zones), 'authored': 7 in zones})
ok = ok and all(r['authored'] for r in resets)
(out / 'RESULT.json').write_text(json.dumps({'passed': ok, 'level': a.level, 'overlay': str(map_dir), 'exit_code': code, 'protected_unchanged': unchanged,
                                             'resets': resets, 'screenshots': sorted(p.name for p in shots.glob('*.png')), 'probe': lines[-40:]}, indent=2))
shutil.rmtree(copy, ignore_errors=True)   # the app copy is regenerable (the client folder keeps the build)
if a.copy_shots and ok:
    dest = a.copy_shots.expanduser() / out.name; dest.mkdir(parents=True, exist_ok=True)
    for png in shots.glob('*.png'): shutil.copy2(png, dest / png.name)
print('PASS' if ok else 'FAIL', out); raise SystemExit(0 if ok else 1)

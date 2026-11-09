#!/usr/bin/env python3
"""Rendered Freestyle arena check in a real test player, with a separate app identity, sandboxed save and no focus use.

    FSARENA_CLIENT=client-a python3 tests/freestyle_arena/run.py EVENT_KEY [--ai N] [--duration N] [--name NAME]

EVENT_KEY e.g. Freestyle/FMX_BuenaVista or FreeRide/FMX_BuenaVista. Writes builds/fsarena/runs/<UTC>-<name>/ (player log, screenshots,
RESULT.json). The player never activates its window (tests/nofocus.py hands the focus back), renders offscreen at a 60 fps cap and
its writes to the profile locations and the install are denied by a sandbox profile.
"""
import argparse, datetime, json, os, plistlib, shutil, subprocess, sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1])); import nofocus
R = Path(__file__).resolve().parents[2]
ap = argparse.ArgumentParser(); ap.add_argument('event'); ap.add_argument('--ai', type=int, default=0); ap.add_argument('--duration', type=int, default=-1, help='the setup page Event Duration index (-1: its default)')
ap.add_argument('--name'); ap.add_argument('--focus', default='', help='comma list: close views of the renderers whose names contain these'); ap.add_argument('--perf', type=int, default=0, help='frame times over this many frames at the overview (fps cap 240)'); ap.add_argument('--game', type=Path, default=R / 'game files/Archive')
a = ap.parse_args()
app = R / 'builds/fsarena' / os.environ.get('FSARENA_CLIENT', 'client-current') / 'FreestyleArenaTest.app'
assert app.is_dir(), app
name = a.name or a.event.replace('/', '_')
out = R / 'builds/fsarena/runs' / (datetime.datetime.utcnow().strftime('%Y%m%dT%H%M%SZ') + '-' + name); out.mkdir(parents=True)
copy = out / 'FreestyleArenaTest.app'; shutil.copytree(app, copy, symlinks=True)
p = copy / 'Contents/Info.plist'; info = plistlib.loads(p.read_bytes()); original = info['CFBundleIdentifier']
info.update(CFBundleIdentifier='local.reflex.fsarena.test-' + out.name.lower().replace('_', '-'), CFBundleName='Freestyle Arena Test', LSAppNapIsDisabled=True); p.write_bytes(plistlib.dumps(info))
exe = copy / 'Contents/MacOS' / info['CFBundleExecutable']
save = out / 'save'; save.mkdir(); shots = out / 'screenshots'; shots.mkdir()
support = Path.home() / 'Library/Application Support'; prefs = Path.home() / 'Library/Preferences'
protect = [support / 'DefaultCompany/Reflex', support / 'DefaultCompany/OpenReflex', support / 'DefaultCompany/Reflex Core Garage Bench', support / original,
           prefs / (original + '.plist'), a.game.resolve()]
sb = out / 'protection.sb'; sb.write_text('(version 1)\n(allow default)\n' + ''.join('(deny file-write* (subpath ' + json.dumps(str(p)) + '))\n' for p in protect))
subprocess.run(['codesign', '--force', '--sign', '-', str(copy)], check=True, capture_output=True)
env = {k: v for k, v in os.environ.items() if not k.startswith(('REFLEX_', 'RX_', 'DYLD_'))}
env.update(REFLEX_SIM='open', REFLEX_FRONTEND='open', REFLEX_SAVE_DIR=str(save), REFLEX_GAME_DIR=str(a.game.resolve()), REFLEX_FSARENA_TEST='1',
           REFLEX_FSARENA_EVENT=a.event, REFLEX_FSARENA_AI=str(a.ai), REFLEX_FSARENA_DURATION=str(a.duration), REFLEX_FSARENA_FOCUS=a.focus, REFLEX_FSARENA_PERF=str(a.perf), REFLEX_FSARENA_OFFSCREEN='1',
           REFLEX_PREVIEW_SHOTS=str(shots), REFLEX_FPS_CAP='240' if a.perf else '60', REFLEX_PACING='interpolated', REFLEX_NO_WELCOME='1')
args = ['/usr/bin/sandbox-exec', '-f', str(sb), str(exe), '-reflexTestWindow', '-screen-fullscreen', '0', '-screen-width', '1280', '-screen-height', '720', '-logFile', str(out / 'player.log')]
with (out / 'console.log').open('w') as log:
    front = nofocus.frontmost()
    proc = subprocess.Popen(args, cwd=R, env=env, stdout=log, stderr=subprocess.STDOUT); print(str(out), flush=True)
    nofocus.hand_back(front)
    try: code = proc.wait(timeout=1200)
    except subprocess.TimeoutExpired: proc.terminate(); proc.wait(timeout=10); code = 124
text = (out / 'player.log').read_text(errors='replace') if (out / 'player.log').exists() else ''
lines = [l for l in text.splitlines() if '[fsarena-test]' in l]
ok = code == 0 and any('PASS' in l for l in lines) and not any('FAIL' in l for l in lines)
(out / 'RESULT.json').write_text(json.dumps({'passed': ok, 'event': a.event, 'exit_code': code, 'screenshots': sorted(p.name for p in shots.glob('*.png')), 'probe': lines}, indent=2))
shutil.rmtree(copy, ignore_errors=True)
print('PASS' if ok else 'FAIL', out); raise SystemExit(0 if ok else 1)

#!/usr/bin/env python3
"""Run the rendered menu/ride acceptance with a separate app identity and save."""
import datetime,hashlib,json,os,plistlib,shutil,subprocess,sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1])); import nofocus   # test players never steal the user's focus
R=Path(__file__).resolve().parents[2]
# SLIDEROCK_CLIENT: the test client folder (tests/sliderock_canyon/build.py); SLIDEROCK_MAP: the overlay under builds/sliderock_canyon
app=R/'builds/sliderock_canyon'/os.environ.get('SLIDEROCK_CLIENT','current-client')/'Sliderock.app'
map_dir=R/'builds/sliderock_canyon'/os.environ.get('SLIDEROCK_MAP','map-preview-v9')
assert app.is_dir()
out=R/'builds/sliderock_canyon/runs'/datetime.datetime.utcnow().strftime('%Y%m%dT%H%M%SZ');out.mkdir(parents=True)
copy=out/'SliderockTest.app';shutil.copytree(app,copy,symlinks=True)
p=copy/'Contents/Info.plist';info=plistlib.loads(p.read_bytes());original=info['CFBundleIdentifier'];info.update(CFBundleIdentifier='local.reflex.sliderock.test-'+out.name.lower(),CFBundleName='Sliderock Ride Test');p.write_bytes(plistlib.dumps(info))
exe=copy/'Contents/MacOS'/info['CFBundleExecutable']
save=out/'save';save.mkdir();shots=out/'screenshots';shots.mkdir()
support=Path.home()/'Library/Application Support';prefs=Path.home()/'Library/Preferences'
protect=[support/'DefaultCompany/Reflex',support/'DefaultCompany/OpenReflex',support/'DefaultCompany/Reflex Core Garage Bench',support/original,prefs/(original+'.plist'),R/'builds/sol_garage/game files']
def fingerprint(p):
 if p.is_file():return hashlib.sha256(p.read_bytes()).hexdigest()
 if p.is_dir():return {str(q.relative_to(p)):hashlib.sha256(q.read_bytes()).hexdigest() for q in p.rglob('*') if q.is_file()}
 return None
# Game assets are protected by the sandbox; snapshot only small real profile locations.
before={str(p):fingerprint(p) for p in protect[:-1]}
sb=out/'protection.sb';sb.write_text('(version 1)\n(allow default)\n'+''.join('(deny file-write* (subpath '+json.dumps(str(p))+'))\n' for p in protect))
subprocess.run(['codesign','--force','--sign','-',str(copy)],check=True)
env={k:v for k,v in os.environ.items() if not k.startswith(('REFLEX_','RX_','DYLD_'))}
env.update(REFLEX_SIM='open',REFLEX_FRONTEND='open',REFLEX_SAVE_DIR=str(save),REFLEX_GAME_DIR=str(map_dir),REFLEX_SLIDEROCK_TEST='1',REFLEX_PREVIEW_SHOTS=str(shots),REFLEX_FPS_CAP='60',REFLEX_PACING='interpolated')
# frame cap, VSync off: renders in the background (never raised). Interpolated pacing: whole 1/60 s game frames, so the probe's
# frame-counted throttle is the same simulated time on a loaded machine (Original pacing stretches each game frame to the real
# frame time: at 17 fps the 120 throttle frames became ~7 s and the bike legitimately crashed into the campsite rocks).
args=['/usr/bin/sandbox-exec','-f',str(sb),str(exe),'-reflexTestWindow','-screen-fullscreen','0','-screen-width','1280','-screen-height','720','-logFile',str(out/'player.log')]
(out/'plan.json').write_text(json.dumps({'argv':args,'env':{k:v for k,v in env.items() if k.startswith('REFLEX_')},'core_sha256':hashlib.sha256((copy/'Contents/PlugIns/librxcore.dylib').read_bytes()).hexdigest()},indent=2))
with (out/'console.log').open('w') as log:
 _front = nofocus.frontmost()
 p=subprocess.Popen(args,cwd=R,env=env,stdout=log,stderr=subprocess.STDOUT);print(str(out),flush=True)
 nofocus.hand_back(_front)
 try:code=p.wait(timeout=1500)
 except subprocess.TimeoutExpired:p.terminate();p.wait(timeout=10);code=124
text=(out/'player.log').read_text(errors='replace') if (out/'player.log').exists() else ''
after={str(p):fingerprint(p) for p in protect[:-1]};unchanged=before==after
required=['main-menu','alive-category','setup','spawn','ride','ride-end','vegetation','vegetation-close','vegetation-far','grass','sky-sun','sky-away','camp','rocks','vista','water','lake-top','water-close']
ok=code==0 and '[sliderock-test] PASS' in text and '[sliderock-test] FAIL' not in text and unchanged and all((shots/(n+'.png')).exists() for n in required)
(out/'RESULT.json').write_text(json.dumps({'passed':ok,'exit_code':code,'protected_unchanged':unchanged,'screenshots':required},indent=2))
print('PASS' if ok else 'FAIL',out);raise SystemExit(0 if ok else 1)

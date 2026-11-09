#!/usr/bin/env python3
"""Build current client with an opt-in menu/ride probe in an isolated project."""
import os,shutil,subprocess
from pathlib import Path
R=Path(__file__).resolve().parents[2]
# SLIDEROCK_CLIENT: the output folder under builds/sliderock_canyon (default current-client); a new folder is seeded (APFS clone,
# imported Library included) from the first existing staged project so the batch build does not reimport everything.
out=R/'builds/sliderock_canyon'/os.environ.get('SLIDEROCK_CLIENT','current-client');stage=out/'Reflex';out.mkdir(parents=True,exist_ok=True)
if not stage.exists():
 seed=next(p for p in (R/'builds/player/stage/Reflex',R/'builds/sliderock_canyon/current-client/Reflex') if p.is_dir())
 subprocess.run(['/bin/cp','-cR',str(seed),str(stage)],check=True)
subprocess.run(['rsync','-a','--delete','--exclude','Temp/','--exclude','Logs/','--exclude','Builds/','--exclude','Library/',str(R/'Unity/Reflex')+'/',str(stage)+'/'],check=True)
S=stage/'Assets/Scripts';shutil.copy2(R/'tests/sliderock_canyon/SliderockRideProbe.cs',S/'OpenFrontEnd/SliderockRideProbe.cs')
p=S/'OriginalWorld/OriginalWorldRuntime.cs';s=p.read_text()
for old,new in [('#if UNITY_EDITOR\n        [Tooltip("Editor tests: DirectInput scan codes held in addition to the keyboard.")]\n        public int[] testHeldKeys = Array.Empty<int>();\n#endif','        public int[] testHeldKeys = Array.Empty<int>();'),('#if UNITY_EDITOR\n            foreach (int k in testHeldKeys) keys[k & 255] = 0x80;\n#endif','            foreach (int k in testHeldKeys) keys[k & 255] = 0x80;')]:
 assert s.count(old)==1,old;s=s.replace(old,new)
s=s.replace('            foreach (int k in testHeldKeys) keys[k & 255] = 0x80;', '            if (Environment.GetEnvironmentVariable("REFLEX_SLIDEROCK_TEST") == "1") Array.Clear(keys, 0, keys.Length);\n            foreach (int k in testHeldKeys) keys[k & 255] = 0x80;')
s=s.replace('uint mask = ReadPads(!menu && !paused);', 'uint mask = ReadPads(!menu && !paused); if (Environment.GetEnvironmentVariable("REFLEX_SLIDEROCK_TEST") == "1") { mask = 0; Array.Clear(pads, 0, pads.Length); }')
p.write_text(s)
env=dict(os.environ,REFLEX_RXCORE_PATH=str(R/'core/build/librxcore.dylib'),REFLEX_OPEN_PLAYER=str(out/'Sliderock.app'))
r=subprocess.run(['/Users/payton/.unity/bin/unity','run',str(stage),'--timeout','1800','--log-file',str(out/'build.log'),'--no-tail','--','-nographics','-executeMethod','Reflex.Port.EditorTools.OpenReflexBuild.BuildMacBatch'],env=env)
raise SystemExit(r.returncode)

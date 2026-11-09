#!/usr/bin/env python3
"""build.py <ABSOLUTE out dir> -- the moto_career_flow test player (tests/moto_career_flow/build.py) plus the reverb impulse probe
(fixture/ReverbImpulseProbe.cs). Test only: the moto build script runs unchanged except that its fixture table also places the probe and
the crowd shot probe (which starts any event from the event browser) stays in the race for REVERB_RACE_SECONDS (default 12) instead of 12 s.
Run it with tests/moto_career_flow/run.py --crowd "Supercross|Supercross/SX_Kingston" and REVERB_PROBE=1 REVERB_RACE_SECONDS=60 in the
environment, then analyse the WAVs in the run's screenshots folder with tests/w_reverb/analyze.py."""
import sys, tempfile
from pathlib import Path
here = Path(__file__).resolve().parent
moto = here.parent / "moto_career_flow/build.py"
src = moto.read_text()
old = '"HudRestRideProbe.cs": "OpenFrontEnd"}'
copy = '    shutil.copy2(F / name, S / folder / name)'
assert src.count(old) == 1 and src.count(copy) == 1
# the crowd probe, patched into a scratch copy (the shipped fixture is untouched)
crowd = (here.parent / "moto_career_flow/fixture/CrowdShotProbe.cs").read_text()
pin = "if (now - raceStarted > 12 && raceShots >= 3"
assert crowd.count(pin) == 1
crowd = crowd.replace(pin, 'if (now - raceStarted > (float.TryParse(Environment.GetEnvironmentVariable("REVERB_RACE_SECONDS"), out var rs) ? rs : 12f) && raceShots >= 3')
scratch = Path(tempfile.mkdtemp(prefix="w_reverb_fixture_"))
(scratch / "CrowdShotProbe.cs").write_text(crowd)
(scratch / "ReverbImpulseProbe.cs").write_text((here / "fixture/ReverbImpulseProbe.cs").read_text())
src = src.replace(old, '"HudRestRideProbe.cs": "OpenFrontEnd", "ReverbImpulseProbe.cs": "OpenFrontEnd"}')
src = src.replace(copy, '    shutil.copy2((Path(r"%s") if name in ("ReverbImpulseProbe.cs", "CrowdShotProbe.cs") else F) / name, S / folder / name)' % scratch)
sys.argv = [str(moto)] + sys.argv[1:]
exec(compile(src, str(moto), "exec"), {"__name__": "__main__", "__file__": str(moto)})

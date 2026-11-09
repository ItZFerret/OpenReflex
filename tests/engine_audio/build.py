#!/usr/bin/env python3
"""build.py <ABSOLUTE out dir> -- the moto_career_flow test player (tests/moto_career_flow/build.py) plus the engine audio probe
(fixture/EngineAudioProbe.cs). Test only: the moto build script runs unchanged except that its fixture table also places the probe.
Run the player with tests/moto_career_flow/run.py (e.g. --arcade --laps 1); ENGINE_AUDIO_TEST_PACK=first makes the first engine pack the
default for every bike. The probe logs "[engine-audio]" lines and writes <run>/screenshots/engine-audio.wav."""
import sys
from pathlib import Path
here = Path(__file__).resolve().parent
moto = here.parent / "moto_career_flow/build.py"
src = moto.read_text()
old = '"FreeRideArrivalProbe.cs": "OpenFrontEnd"}'
copy = '    shutil.copy2(F / name, S / folder / name)'
assert src.count(old) == 1 and src.count(copy) == 1
src = src.replace(old, '"FreeRideArrivalProbe.cs": "OpenFrontEnd", "EngineAudioProbe.cs": "OpenFrontEnd"}')
src = src.replace(copy, '    shutil.copy2((Path(r"%s") if name == "EngineAudioProbe.cs" else F) / name, S / folder / name)' % (here / "fixture"))
sys.argv = [str(moto)] + sys.argv[1:]
exec(compile(src, str(moto), "exec"), {"__name__": "__main__", "__file__": str(moto)})

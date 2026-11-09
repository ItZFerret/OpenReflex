#!/usr/bin/env python3
"""plan-alivelook.py OUT_DIR LEVEL... -- writes one lighting-showcase plan per MX vs ATV Alive level for the ALIVE LOOK comparison (w-alivereflex):
the same bike-relative views in both looks (run.py --plan PLAN --game <Alive overlay> --env REFLEX_ALIVE_LOOK=alive|reflex), ORIGINAL lighting."""
import json, sys
from pathlib import Path

# world views of a level's own places (w-alivefix): the Stewart Compound's two supercross tracks (the red clay decals FR_JS_Decal_SX_* around
# (-75, -340) and (-212, 330)) from above and low; the holeshot inflatable (GL_TO_HoleShot_A) of three nationals, 4 consecutive frames with the
# world running (its swing, or the thrash of the swapped rotation keys)
_JS7 = [
    {"name": "sx_top", "cam": "pose", "t": 6, "at": [-75, 24, -340], "frame": "world", "offset": [0, 300, 1], "look": "anchor", "lookY": 0, "fov": 60},
    {"name": "sx_low", "cam": "pose", "t": 6.5, "at": [-75, 24, -340], "frame": "world", "offset": [0, 45, 150], "look": "anchor", "lookY": 0, "fov": 60},
    {"name": "sx2_top", "cam": "pose", "t": 7, "at": [-212, 27, 330], "frame": "world", "offset": [0, 250, 1], "look": "anchor", "lookY": 0, "fov": 60},
]
def _arch(at, offset):
    return [{"name": "arch", "cam": "pose", "t": 6, "at": at, "frame": "world", "offset": offset, "look": "anchor", "lookY": 8, "fov": 55, "freeze": False, "seq": 4}]
EXTRA = {"FR_DLC_JSComp": _JS7, "NAT_DLC_JSComp_A": _JS7, "NAT_DLC_JSComp_B": _JS7,
         "NAT_Farm": _arch([-36.8, 829.8, 68.5], [-35, 8, 5]), "NAT_Southwest": _arch([-525.0, 898.3, 142.8], [-32, 8, 14]),
         "NAT_Tropic_B": _arch([215.7, 693.6, 169.6], [8, 8, 34])}

out = Path(sys.argv[1]); out.mkdir(parents=True, exist_ok=True)
for level in sys.argv[2:]:
    plan = {"event": "FreeRide/" + level, "prefix": level, "settle": 8, "adapt": 3, "perf": 0, "modes": ["original"], "views": [
        {"name": "chase", "cam": "pose", "t": 4, "frame": "bike", "offset": [0, 2.4, -7.5], "look": "anchor", "lookY": 1.2, "fov": 62},
        {"name": "ground", "cam": "pose", "t": 4.5, "frame": "bike", "offset": [3.5, 1.6, 6], "look": "anchor", "lookY": 0.2, "fov": 55},
        {"name": "wide", "cam": "pose", "t": 5, "frame": "bike", "offset": [0, 30, -70], "look": "anchor", "lookY": 0, "fov": 62},
        {"name": "sun", "cam": "pose", "t": 5.5, "frame": "sun", "offset": [0, 18, -45], "look": "yawpitch", "yaw": 0, "pitch": -8, "fov": 70},
    ] + EXTRA.get(level, [])}
    (out / f"plan-{level}.json").write_text(json.dumps(plan, indent=1))
    print(out / f"plan-{level}.json")

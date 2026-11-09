#!/usr/bin/env python3
"""plan-rmvol.py OUT_DIR [NAME...] -- the VOLUMETRIC LIGHT comparison plans (w-rmvol): per level the original's fake shafts (ENHANCED,
VOLUMETRIC LIGHT off: mode enhanced-novol) against the volumetric light (enhanced), optionally ORIGINAL, on the game's own cameras, views
towards / across the sun and a short camera path for tests/lighting_showcase/temporal.py (run.py --plan OUT_DIR/plan-NAME.json)."""
import json, sys
from pathlib import Path

PATH = {"name": "path", "cam": "path", "t": 6, "frame": "bike", "offset": [0, 2.6, -8.5], "look": "anchor", "lookY": 1.0, "fov": 62,
        "frames": 16, "step": 0.18, "bobPitch": 0.35, "bobHeight": 0.03, "bobPeriod": 8}
SUN = [
    {"name": "tosun", "cam": "pose", "t": 5, "frame": "sun", "offset": [0, 3.5, -28], "look": "yawpitch", "yaw": 0, "pitch": 9, "fov": 70},
    {"name": "acrosssun", "cam": "pose", "t": 5.5, "frame": "sun", "offset": [-26, 4, 6], "look": "yawpitch", "yaw": 35, "pitch": 4, "fov": 70},
    # through a tree's crown towards the sun (the fixture's "treesun" frame: the camera on the tree's shadow side)
    {"name": "treesun", "cam": "pose", "t": 6, "frame": "treesun", "tree": 3, "treeDist": 14, "treeMin": 8, "lookY": 1.7, "look": "yawpitch", "yaw": 0, "pitch": -5, "fov": 70},
    {"name": "treeside", "cam": "pose", "t": 6.5, "frame": "treesun", "tree": 1, "treeDist": 18, "treeMin": 8, "lookY": 1.7, "look": "yawpitch", "yaw": 50, "pitch": -12, "fov": 70},
]
STADIUM = [
    {"name": "roof", "cam": "pose", "t": 5, "frame": "bike", "offset": [0, 6, -30], "look": "yawpitch", "yaw": 0, "pitch": 16, "fov": 75},
    {"name": "wide", "cam": "pose", "t": 5.5, "frame": "bike", "offset": [0, 22, -62], "look": "yawpitch", "yaw": 0, "pitch": -4, "fov": 70},
    {"name": "side", "cam": "pose", "t": 6, "frame": "bike", "offset": [-30, 5, 0], "look": "yawpitch", "yaw": 90, "pitch": 10, "fov": 75},
]
GAME = [{"name": "intro", "cam": "game", "t": 3}, {"name": "gate", "cam": "game", "t": 12}]
LEVELS = {
    "kingston":    ("Supercross/SX_Kingston", "V_Kingston", STADIUM),
    "freightyard": ("SX_DLC01_FreightYard", "V_FreightYard", STADIUM),
    "manchester":  ("SX_Manchester", "V_Manchester", STADIUM),
    "concord":     ("National/NAT_Concord", "V_Concord", SUN),
    "alpine":      ("FreeRide/FR_Alpine", "V_Alpine", SUN + [
        {"name": "vista", "cam": "pose", "t": 6, "frame": "sun", "offset": [0, 40, 70], "look": "yawpitch", "yaw": 180, "pitch": -14, "fov": 70},
        {"name": "backlit", "cam": "pose", "t": 6.5, "frame": "sun", "offset": [0, 30, -70], "look": "yawpitch", "yaw": 0, "pitch": -6, "fov": 70}]),
    "dover":       ("Freestyle/FMX_Dover", "V_Dover_dusk", SUN),
    "daltonb":     ("National/NAT_DLC06_DaltonB", "V_DaltonB_overcast", SUN),
    "bedfordb":    ("National/NAT_BedfordB", "V_BedfordB", SUN),
    "tropic":      ("FreeRide/NAT_Tropic", "V_Tropic_alive", SUN),
}

out = Path(sys.argv[1]); out.mkdir(parents=True, exist_ok=True)
names = sys.argv[2:] or list(LEVELS)
modes = ["enhanced-novol", "enhanced"]
for n in names:
    ev, prefix, views = LEVELS[n]
    plan = {"event": ev, "prefix": prefix, "settle": 8, "adapt": 2.5, "perf": 3, "modes": modes,
            "views": GAME + views + [PATH]}
    (out / f"plan-{n}.json").write_text(json.dumps(plan, indent=1))
    print(out / f"plan-{n}.json")

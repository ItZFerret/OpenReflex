"""gen_stats.py OUT_JSON EVENT PREFIX X Z YAW MODES [GROUND] -- the LOD-state tests from one world pose (run with REFLEX_FOREST_STATS=1):
 jitter: the camera still, the yaw shaking +-3 degrees (steering) with a 7 frame period      -> flip-flops of the LOD state
 sweep:  the camera still, turning 0.5 degrees per frame                                      -> LOD changes from turning alone
 ride:   the camera moving 1.5 units per frame (a rider at ~60 mph at 60 fps) with a bob      -> changes with distance, and flips from the bob"""
import json, sys
out, ev, prefix = sys.argv[1:4]
x, z, yaw = [float(v) for v in sys.argv[4:7]]
modes = sys.argv[7].split(",")
g = float(sys.argv[8]) if len(sys.argv) > 8 else 6
base = {"cam": "path", "frame": "world", "at": [x, 0, z], "offset": [0, 0, 0], "pitch": -3, "fov": 70, "ground": g, "every": 5}
views = [
    dict(base, name="jitter", yaw=yaw, t=4, frames=120, step=0.0, bobYaw=3.0, bobPeriod=7),
    dict(base, name="sweep", yaw=yaw - 30, t=4.2, frames=120, step=0.0, turn=0.5),
    dict(base, name="ride", yaw=yaw, t=4.4, frames=150, step=1.5, bobPitch=0.4, bobHeight=0.25, bobPeriod=9),
]
json.dump({"event": ev, "prefix": prefix, "settle": 6, "adapt": 1.0, "perf": 0, "modes": modes, "views": views}, open(out, "w"), indent=1)
print(out)

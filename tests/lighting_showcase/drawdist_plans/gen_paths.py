"""gen_paths.py OUT_JSON EVENT PREFIX X Y Z YAW MODES [PERF_SECONDS] -- a plan with a slow dolly and a pure-rotation sweep from one world pose."""
import json, sys
out, ev, prefix = sys.argv[1:4]
x, y, z, yaw = [float(v) for v in sys.argv[4:8]]
modes = sys.argv[8].split(",")
perf = float(sys.argv[9]) if len(sys.argv) > 9 else 0
views = [
    {"name": "dolly", "cam": "path", "frame": "world", "at": [x, y, z], "offset": [0, 0, 0], "yaw": yaw, "pitch": -3, "fov": 70, "t": 4,
     "frames": 100, "step": 0.12, "every": 1},
    {"name": "sweep", "cam": "path", "frame": "world", "at": [x, y, z], "offset": [0, 0, 0], "yaw": yaw - 25, "pitch": -3, "fov": 70, "t": 4.5,
     "frames": 100, "step": 0.0, "turn": 0.5, "every": 1},
]
json.dump({"event": ev, "prefix": prefix, "settle": 6, "adapt": 1.0, "perf": perf, "modes": modes, "views": views}, open(out, "w"), indent=1)
print(out)

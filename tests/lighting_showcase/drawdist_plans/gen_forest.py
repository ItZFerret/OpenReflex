"""gen_forest.py OUT_JSON EVENT PREFIX MODES RANKS YAW [OFFSET_X OFFSET_Z] -- one still per densest-cluster rank (1-frame paths at the cluster centre, 6 above the ground)."""
import json, sys
out, ev, prefix = sys.argv[1:4]
modes = sys.argv[4].split(","); ranks = [int(v) for v in sys.argv[5].split(",")]; yaw = float(sys.argv[6])
ox = float(sys.argv[7]) if len(sys.argv) > 7 else 0; oz = float(sys.argv[8]) if len(sys.argv) > 8 else 0
views = [{"name": "r%d" % r, "cam": "path", "frame": "forest", "tree": r, "at": None, "offset": [ox, 0, oz], "yaw": yaw, "pitch": -2, "fov": 70, "t": 4 + 0.1 * i,
          "frames": 1, "step": 0.0, "ground": 6} for i, r in enumerate(ranks)]
for v in views: del v["at"]
json.dump({"event": ev, "prefix": prefix, "settle": 6, "adapt": 3, "perf": 0, "modes": modes, "views": views}, open(out, "w"), indent=1)
print(out)

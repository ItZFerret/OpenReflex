#!/usr/bin/env python3
"""Build and run the strict native Free Ride regression (reset_regression.c) for converted Alive levels.

    python3 tests/alive_tracks/regression.py OVERLAY LEVEL [LEVEL ...] [--out DIR]

Links the test against the current core objects (core/build.sh first): the objects of core/src/*.c, game/*.c, api/*.c,
build/gen/rx_fnsig.c, src/*.cpp and src/f80/*.cpp, named exactly as core/build.sh names them (a stale object of a
deleted source is never linked). The height window comes from the overlay's registry (alive-levels.json: the
terrain height range is not stored there, so the conversion's terrain heightfield is read). No window, no focus use.
"""
import argparse, json, subprocess, sys
from pathlib import Path
R = Path(__file__).resolve().parents[2]
C = R / "core"


# Levels whose start faces an obstacle within the 2 s throttle run: they brake after the throttle, as their own tests do (Sliderock
# Canyon: coasting reaches the campsite rocks, a legitimate crash that the accepted map-wsr3 shows the same way; Yukiyama MX
# (NAT_Northwest_C): coasting runs into a group of trackside spectators, AnimEntitySingleAnimLoopInstanced MaleCrowd01_Long whose
# authored AnimationCollisionAsset (Editor_Default, a kind-2 box list) is solid in Alive too: a wreck, reason 2).
BRAKE_LEVELS = {"FR_DLC_Hills", "NAT_Northwest_C"}


def core_objects():
    objs = []
    for pattern in ("src/*.c", "game/*.c", "api/*.c", "build/gen/rx_fnsig.c", "src/*.cpp", "src/f80/*.cpp"):
        for f in sorted(C.glob(pattern)):
            rel = str(f.relative_to(C)) if "build/gen" not in pattern else "build/gen/rx_fnsig.c"
            o = C / "build/obj" / (rel.replace("/", "_").replace(".", "_") + ".o")
            if "build/gen" in pattern:
                o = C / "build/obj" / ("build/gen/rx_fnsig.c".replace("/", "_").replace(".", "_") + ".o")
            if not o.is_file() or o.stat().st_mtime < f.stat().st_mtime:
                raise SystemExit(f"stale or missing core object {o.name}: run core/build.sh")
            objs.append(str(o))
    return objs


def build(out):
    binary = out / "reset_regression"
    subprocess.run(["clang++", "-arch", "arm64", "-O2", "-x", "c", "-std=c11", "-I", str(C / "include"), "-I", str(C / "src"),
                    "-I", str(C / "game"), "-I", str(C / "api"), "-c", str(Path(__file__).parent / "reset_regression.c"),
                    "-o", str(out / "reset_regression.o")], check=True)
    subprocess.run(["clang++", "-arch", "arm64", "-o", str(binary), str(out / "reset_regression.o"), *core_objects(), "-lz"], check=True)
    return binary


def height_range(overlay, level):
    sys.path.insert(0, str(R / "tools/formats"))
    from alive_heights import terrain_height_range
    return terrain_height_range(overlay, level)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("overlay", type=Path); ap.add_argument("levels", nargs="+"); ap.add_argument("--out", type=Path)
    ap.add_argument("--brake", action="store_true", help="brake after the throttle (the Sliderock test) instead of coasting")
    a = ap.parse_args()
    out = (a.out or (a.overlay.parent / ("regression-" + a.overlay.name))).resolve(); out.mkdir(parents=True, exist_ok=True)
    binary = build(out)
    results = {}
    for level in a.levels:
        lo, hi = height_range(a.overlay, level)
        log = out / f"reset-regression-{level}.log"
        with log.open("w") as f:
            code = subprocess.run([str(binary), str(a.overlay.resolve()), level, f"{lo:.3f}", f"{hi:.3f}", "brake" if a.brake or level in BRAKE_LEVELS else "coast"], stdout=f, stderr=subprocess.STDOUT, cwd=out).returncode
        text = log.read_text(errors="replace")
        # A reset over the level's own reset material (layer-0 zone value 7, reason 6: Player_CheckResetTerrain) is authored
        # (the out-of-bounds area around a race track), not spurious; every other reset fails the ride.
        import re
        sys.path.insert(0, str(R / "tools/formats")); from alive_heights import zone_value_at
        resets = [(int(m.group(1)), float(m.group(2)), float(m.group(3))) for m in
                  re.finditer(r"RESET frame=\d+ reason=(\d+) timer=[\d.]+ pos=([-\d.]+),[-\d.]+,([-\d.]+)", text)]
        authored = [r for r in resets if r[0] == 6 and zone_value_at(a.overlay, level, r[1], r[2]) == 7]
        spurious = [r for r in resets if r not in authored]
        failed_only_resets = code != 0 and re.search(r"FAIL line \d+: resets==0", text) is not None
        passed = (code == 0 and "PASS ride" in text) or (failed_only_resets and resets and not spurious)
        results[level] = {"passed": passed, "exit": code, "log": str(log), "resets": len(resets),
                          "authored_reset_zone": len(authored), "spurious_resets": len(spurious),
                          "summary": next((l for l in text.splitlines() if l.startswith(("PASS", "summary"))), text.strip().splitlines()[-1:] or "")}
        print(level, "PASS" if results[level]["passed"] else "FAIL", results[level]["summary"],
              f"(authored reset-zone resets {len(authored)}, spurious {len(spurious)})" if resets else "", flush=True)
    (out / "results.json").write_text(json.dumps(results, indent=1) + "\n")
    raise SystemExit(0 if all(r["passed"] for r in results.values()) else 1)


if __name__ == "__main__":
    main()

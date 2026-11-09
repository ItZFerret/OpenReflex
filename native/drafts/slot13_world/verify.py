#!/usr/bin/env python3
"""Run the native slot13 world draft (ARM64 and x86-64) on every captured live
world and compare with the live slot13 exit snapshot of the same call."""
import argparse, hashlib, json, subprocess, sys, tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
GRIDS = ROOT / "_re/formats/out/alpine_collision_world"


def record(path):
    data = Path(path).read_bytes()
    return dict(path=str(path), bytes=len(data), sha256=hashlib.sha256(data).hexdigest())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("captures", nargs="+", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    binaries = {arch: HERE / f"slot13-world-{arch}" for arch in ("arm64", "x86_64")}
    results, ok = [], True
    for capture in args.captures:
        rows = {}
        with (capture / "bike_stage.jsonl").open() as stream:
            for line in stream:
                row = json.loads(line)
                if row["slot"] == 13:
                    rows[(row["call_id"], row["phase"])] = row
        for world in sorted((capture / "terrain").glob("world-*.bin"), key=lambda p: int(p.stem.split("-")[1])):
            call = int(world.stem.split("-")[1])
            entry, exit_ = rows[(call, "entry")], rows[(call, "exit")]
            expected = bytes.fromhex(exit_["state_bits"]) + b"".join(bytes.fromhex(f["bits"]) for f in exit_["filters"])
            entry_state = bytes.fromhex(entry["state_bits"])
            item = dict(capture=capture.name, call=call)
            for arch, binary in binaries.items():
                with tempfile.NamedTemporaryFile() as out:
                    command = ([] if arch == "arm64" else ["arch", "-x86_64"]) + [str(binary), str(world), str(GRIDS / "base.bin"),
                               str(GRIDS / "deform.bin"), entry["dt_bits"], entry["x87_cw"], out.name]
                    proc = subprocess.run(command, capture_output=True, text=True)
                    got = Path(out.name).read_bytes()
                equal = proc.returncode == 0 and got[:len(expected)] == expected
                item[arch] = dict(returncode=proc.returncode, equal=equal, error=proc.stderr.strip()[-300:] or None,
                                  locks=list(memoryview(got[len(expected):]).cast("I")) if len(got) == len(expected) + 8 else None)
                ok &= equal
            item["changed_words"] = sum(1 for o in range(0, 0x9700, 4) if entry_state[o:o + 4] != expected[o:o + 4])
            results.append(item)
            print(json.dumps(item), flush=True)
    report = dict(schema="reflex.slot13-world-native.v1", success=ok and bool(results), calls=len(results),
                  source=record(HERE / "slot13_world_draft.cpp"), program=record(HERE / "program.inc"),
                  program_json=record(ROOT / "_re/trace/bike_slot13_objects_program.json"),
                  binaries={a: record(b) for a, b in binaries.items()},
                  grids={n: record(GRIDS / n) for n in ("base.bin", "deform.bin")},
                  captures=[str(c) for c in args.captures], results=results)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(dict(success=report["success"], calls=len(results))))
    return 0 if report["success"] else 1


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""report.py <run dir>/profile/<name>.raw [...] -- GC.Alloc reports of run.py --profile captures (test only).

Opens the build's staged project (builds/gc_spikes/stage/Reflex, made by build.py: it has fixture/GcAllocReport.cs) in a batch Editor and
writes <name>.txt next to each capture: the main thread's managed allocations per frame by marker and by call site, and the slowest
frames' markers. REFLEX_PROFILE_STACK=<n> sets the call-site depth (default 4)."""
import os, subprocess, sys
from pathlib import Path

R = Path(__file__).resolve().parents[2]
UNITY = "/Applications/Unity/Hub/Editor/6000.6.1f1/Unity.app/Contents/MacOS/Unity"
stage = R / "builds/gc_spikes/stage/Reflex"
assert (stage / "Assets/Editor/Profiling/GcAllocReport.cs").is_file(), "build.py first (the staged project)"
for raw in map(Path, sys.argv[1:]):
    raw = raw.resolve(); out = raw.with_suffix(".txt")
    env = dict(os.environ, REFLEX_PROFILE_FILE=str(raw), REFLEX_PROFILE_REPORT=str(out))
    subprocess.run(["perl", "-e", "alarm shift; exec @ARGV", "3600", UNITY, "-batchmode", "-nographics", "-projectPath", str(stage),
                    "-executeMethod", "GcAllocReport.Run", "-logFile", str(raw.with_suffix(".editor.log"))], env=env)
    print(out, "written" if out.exists() else "MISSING")

#!/usr/bin/env python3
"""base_shaders.py <out dir> <commit> <Shaders/X.shader> [...] -- TEST ONLY: the shipped shaders as they were at <commit>, renamed
"PerfBench/Base/<their name>" (includes point at Assets/Shaders), for the bench's in-process identity check of a shader change:
bench.py build --extra-shaders <out dir>; run --ablations "o:swap=Reflex/Original HFTerrain=PerfBench/Base/Reflex/Original HFTerrain"
renders the same frozen frame with the old and the new program and logs the per-pixel difference (COMPARE ...)."""
import re, subprocess, sys
from pathlib import Path

R = Path(__file__).resolve().parents[3]
out, commit, files = Path(sys.argv[1]), sys.argv[2], sys.argv[3:]
out.mkdir(parents=True, exist_ok=True)
for f in files:
    s = subprocess.run(["git", "-C", str(R), "show", f"{commit}:Unity/Reflex/Assets/{f}"], check=True, capture_output=True, text=True).stdout
    name = re.search(r'Shader\s+"([^"]+)"', s).group(1)
    s = s.replace(f'Shader "{name}"', f'Shader "PerfBench/Base/{name}"', 1).replace('#include "Reflex', '#include "Assets/Shaders/Reflex')
    (out / ("PerfBench_Base_" + Path(f).stem + ".shader")).write_text(f"// TEST ONLY (tests/perf_profile/fixture/base_shaders.py): {f} at {commit}\n" + s)
    print("PerfBench/Base/" + name)

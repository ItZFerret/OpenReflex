#!/usr/bin/env python3
"""Private whole-force draft against fixed original 43e100 boundary outputs."""
from pathlib import Path
import datetime
import hashlib
import json
import re
import subprocess
import time

ROOT = Path(__file__).resolve().parents[3]


def record(path):
    data = path.read_bytes()
    return dict(path=str(path.relative_to(ROOT)), bytes=len(data), sha256=hashlib.sha256(data).hexdigest())


def main():
    out = ROOT / '_re/trace/out/bike-force-stage-native-draft-original'
    out.mkdir(exist_ok=True)
    source = Path(__file__).with_name('force_stage_draft.cpp')
    runs = [ROOT / '_re/trace/out' / ('original-math-force-stage-' + mode + '-branches') for mode in ('active', 'inactive')]
    sources = [Path(__file__), source]
    sources += [ROOT / p for p in (
        'native/drafts/tire/tire_draft.cpp', 'native/drafts/tire/program.inc',
        'native/src/bike_powertrain_draft.cpp', 'native/src/bike_force_draft.cpp',
        'native/src/fp32.h', 'native/include/reflex_core.h')]
    for run in runs:
        sources.extend(run / name for name in ('inputs.bin', 'original.bin', 'report.json'))
        if not json.loads((run / 'report.json').read_text())['success']:
            raise RuntimeError('Original oracle did not accept this dataset')
    before = [record(p) for p in sources]
    results = []
    commands = []
    disassemblies = []
    for architecture in ('arm64', 'x86_64'):
        exe = ROOT / f'native/build/force-stage-draft-{architecture}'
        command = ['clang++', '-std=c++17', '-O2', '-Wall', '-Wextra', '-Werror',
                   '-fno-fast-math', '-ffp-contract=off', '-frounding-math', '-arch', architecture,
                   '-I', str(ROOT / 'native/include'), str(source), '-o', str(exe)]
        commands.append(command)
        process = subprocess.run(command, capture_output=True, text=True)
        (out / f'compile-{architecture}.log').write_text(process.stdout + process.stderr)
        if process.returncode:
            raise RuntimeError('Native whole force draft compilation failed')
        invoke = [str(exe)] if architecture == 'arm64' else ['arch', '-x86_64', str(exe)]
        for run in runs:
            mode = 'inactive' if 'inactive' in run.name else 'active'
            answer = out / f'{mode}-{architecture}.bin'
            log = out / f'{mode}-{architecture}.log'
            command = invoke + [str(run / 'inputs.bin'), str(answer)]
            commands.append(command)
            started = time.monotonic()
            process = subprocess.run(command, capture_output=True, text=True)
            elapsed = time.monotonic() - started
            log.write_text(process.stdout + process.stderr)
            expected = (run / 'original.bin').read_bytes()
            actual = answer.read_bytes()
            original_report = json.loads((run / 'report.json').read_text())
            result = dict(architecture=architecture, mode=mode,
                          exact=process.returncode == 0 and actual == expected,
                          cases=original_report['cases'],
                          mutable_output_words=original_report['mutable_output_words'],
                          full_simulation_words=len(expected) // 4, seconds=elapsed,
                          output=record(answer), log=record(log), executable=record(exe))
            results.append(result)
            print(json.dumps({k: result[k] for k in ('architecture', 'mode', 'exact', 'cases', 'mutable_output_words', 'seconds')}), flush=True)
            if not result['exact']:
                raise RuntimeError('Native whole-force output differs from original')
        process = subprocess.run(['otool', '-tvV', str(exe)], capture_output=True, text=True, check=True)
        disassembly = out / f'disassembly-{architecture}.txt'
        disassembly.write_text(process.stdout)
        fused = re.findall(r'\b(?:fmadd|fmsub|fnmadd|fnmsub|fmla|fmls|vfmadd\w*|vfmsub\w*|vfnmadd\w*|vfnmsub\w*)\b', process.stdout)
        disassemblies.append(dict(architecture=architecture, file=record(disassembly), fused_instruction_matches=fused))
        if fused:
            raise RuntimeError('Unexpected fused arithmetic in native binary')
    after = [record(p) for p in sources]
    if before != after:
        raise RuntimeError('Native sources or fixed original datasets changed during proof')
    report = dict(schema='reflex.force-stage-native-draft-evidence.v1',
                  checked_at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
                  success=True,
                  scope='Private 0043e100 whole-force composition from one initial simulation/control/globals snapshot. Every complete final simulation byte compared to isolated original boundary outputs. Contacts and controls are supplied inputs; no collision queries or gameplay loop are implemented here. No registered ABI or Unity changes.',
                  profile_mapping='Parent bit2 clear: x87 CW007f (24 bits); bit2 set: CW027f (53 bits). Damping draft receives profile XOR2 because its standalone convention is reversed. Low bit selects the original equivalent bounded conversion path.',
                  cases_per_architecture=sum(r['cases'] for r in results if r['architecture'] == 'arm64'),
                  mutable_words_per_architecture=sum(r['mutable_output_words'] for r in results if r['architecture'] == 'arm64'),
                  full_simulation_words_per_architecture=sum(r['full_simulation_words'] for r in results if r['architecture'] == 'arm64'),
                  sources=before, sources_unchanged=True, commands=commands,
                  results=results, disassemblies=disassemblies)
    (out / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())

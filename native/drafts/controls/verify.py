#!/usr/bin/env python3
"""Verify private ARM/Intel controls against frozen original output records."""
from pathlib import Path
import datetime,hashlib,json,random,struct,subprocess,sys,time
from fractions import Fraction
ROOT=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(ROOT/'_re/trace'))
from bike_controls_model import round_significand
from bike_tire_fp import F32

def record(path):
 data=path.read_bytes();return dict(path=str(path.relative_to(ROOT)),bytes=len(data),sha256=hashlib.sha256(data).hexdigest())

def arithmetic_vectors(out):
 rng=random.Random(0x44c17053);inputs=bytearray();answers=bytearray()
 for op in range(3):
  for index in range(1000):
   # Finite binary32 operands span signs and exponents; sums remain within the
   # explicitly supported integer-alignment bound. Products need up48 bits.
   exponent=rng.randrange(25,225);other=exponent+rng.randrange(-20,21) if op==0 else rng.randrange(1,254)
   aa=(rng.randrange(2)<<31)|(exponent<<23)|rng.randrange(1<<23)
   bb=(rng.randrange(2)<<31)|(other<<23)|rng.randrange(1<<23)
   if index%50==0:bb=aa^0x80000000
   a,b=F32.value(aa),F32.value(bb)
   for precision in (24,53):
    exact=a+b if op==0 else a*b if op==1 else a-int(a/b)*b
    exact=round_significand(exact,precision) if op<2 else exact
    value=float(exact)
    if not exact and (bool(aa>>31)^bool(bb>>31) if op==1 else bool(aa>>31) if op==2 else bool(aa>>31) and bool(bb>>31)):value=-0.0
    inputs+=struct.pack('<IIdd',op,precision,float(a),float(b));answers+=struct.pack('<d',value)
 (out/'arithmetic-inputs.bin').write_bytes(inputs);(out/'arithmetic-expected.bin').write_bytes(answers)
 return 6000

def main():
 out=ROOT/'_re/trace/out/bike-controls-native-draft-original';out.mkdir(exist_ok=True)
 draft=Path(__file__).parent;source=draft/'controls_draft.cpp'
 runs=[ROOT/'_re/trace/out'/name for name in ('original-math-controls-pilot','original-math-controls-generated')]
 sources=[source,Path(__file__),draft/'arithmetic_tests.cpp',draft/'program.inc',draft/'generate_program.py',ROOT/'native/src/fp32.h',ROOT/'_re/trace/bike_controls_model.py',ROOT/'_re/trace/bike_tire_fp.py',ROOT/'_re/trace/bike_controls_program.json']
 for run in runs:
  report=json.loads((run/'report.json').read_text())
  if report.get('success') is not True or report.get('exact') is not True:raise ValueError('Original controls oracle not accepted')
  if record(run/'inputs.bin')!=report.get('inputs') or record(run/'original.bin')!=report.get('original'):
   raise ValueError('Original controls vectors differ from accepted oracle report')
  sources.extend(run/name for name in ('inputs.bin','original.bin','report.json'))
 before=[record(p) for p in sources];commands=[];results=[];arithmetic_count=arithmetic_vectors(out)
 for arch in ('arm64','x86_64'):
  exe=ROOT/f'native/build/controls-draft-{arch}';arithmetic=ROOT/f'native/build/controls-arithmetic-{arch}'
  for src,target in ((source,exe),(draft/'arithmetic_tests.cpp',arithmetic)):
   command=['clang++','-std=c++17','-O2','-Wall','-Wextra','-Werror','-fno-fast-math','-ffp-contract=off','-frounding-math','-arch',arch,str(src),'-o',str(target)]
   p=subprocess.run(command,capture_output=True,text=True);(out/f'compile-{target.name}.log').write_text(p.stdout+p.stderr);commands.append(command)
   if p.returncode:raise RuntimeError('Controls draft compilation failed')
  invoke=[str(exe)] if arch=='arm64' else ['arch','-x86_64',str(exe)]
  p=subprocess.run(invoke+['--self-test'],capture_output=True,text=True);(out/f'directed-{arch}.log').write_text(p.stdout+p.stderr)
  if p.returncode:raise RuntimeError('Controls native directed arithmetic checks failed')
  command=([str(arithmetic)] if arch=='arm64' else ['arch','-x86_64',str(arithmetic)])+[str(out/'arithmetic-inputs.bin'),str(out/f'arithmetic-{arch}.bin')]
  p=subprocess.run(command,capture_output=True,text=True);commands.append(command);(out/f'arithmetic-{arch}.log').write_text(p.stdout+p.stderr)
  if p.returncode or (out/f'arithmetic-{arch}.bin').read_bytes()!=(out/'arithmetic-expected.bin').read_bytes():raise RuntimeError('Controls generated arithmetic differs')
  for run in runs:
   mode='pilot' if 'pilot' in run.name else 'generated';answer=out/f'{mode}-{arch}.bin';log=out/f'{mode}-{arch}.log'
   command=invoke+[str(run/'inputs.bin'),str(answer)];commands.append(command);start=time.monotonic();p=subprocess.run(command,capture_output=True,text=True);elapsed=time.monotonic()-start;log.write_text(p.stdout+p.stderr)
   actual=answer.read_bytes();original=(run/'original.bin').read_bytes();report=json.loads((run/'report.json').read_text())
   result=dict(architecture=arch,mode=mode,exact=p.returncode==0 and actual==original,cases=report['cases'],mutable_output_words=report['mutable_output_words'],complete_record_words=len(original)//4,seconds=elapsed,output=record(answer),log=record(log),executable=record(exe));results.append(result)
   print(json.dumps({k:result[k] for k in ('architecture','mode','exact','cases','mutable_output_words','seconds')}),flush=True)
   if not result['exact']:raise RuntimeError('Native controls output differs from original')
 after=[record(p) for p in sources]
 if before!=after:raise RuntimeError('Native controls sources or original vectors changed during verification')
 report=dict(schema='reflex.controls-native-draft-evidence.v1',checked_at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),success=all(r['exact'] for r in results),
  scope='Private instruction-derived controls draft, no public ABI or Unity installation. Complete state/control/filter/runtime/query endpoint records compared against fixed original outputs. Query result is explicit external input; original registry selection executes; terrain traversal is not implemented.',
  accepted_generated_cases_per_architecture=1000,generated_mutable_words_per_architecture=74744,generated_arithmetic_checks_per_architecture=arithmetic_count,directed_arithmetic_checks_per_architecture=12,
  sources=before,commands=commands,results=results,arithmetic_vectors=[record(out/name) for name in ('arithmetic-inputs.bin','arithmetic-expected.bin','arithmetic-arm64.bin','arithmetic-x86_64.bin')])
 (out/'report.json').write_text(json.dumps(report,indent=2)+'\n');return 0
if __name__=='__main__':raise SystemExit(main())

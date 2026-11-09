#!/usr/bin/env python3
"""Compile both private tire drafts and compare fixed original output bytes."""
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
import datetime,hashlib,json,subprocess,time
ROOT=Path(__file__).resolve().parents[3]
def record(path):
 data=path.read_bytes();return dict(path=str(path.relative_to(ROOT)),bytes=len(data),sha256=hashlib.sha256(data).hexdigest())
def main():
 out=ROOT/'_re/trace/out/bike-tire-native-draft-original';out.mkdir(exist_ok=True)
 source=ROOT/'native/drafts/tire/tire_draft.cpp'
 runs=[ROOT/'_re/trace/out'/name for name in ('original-math-tire-inactive-final-pinned','original-math-tire-active-final-pinned')]
 sources=[source,Path(__file__),source.with_name('program.inc'),source.with_name('generate_program.py'),ROOT/'native/src/fp32.h',ROOT/'_re/trace/bike_tire_program.json']
 for run in runs:sources.extend(run/name for name in ('inputs.bin','original.bin','report.json'))
 before=[record(p) for p in sources];commands=[];results=[]
 for arch in ('arm64','x86_64'):
  exe=ROOT/f'native/build/tire-draft-{arch}'
  command=['clang++','-std=c++17','-O2','-Wall','-Wextra','-Werror','-fno-fast-math','-ffp-contract=off','-frounding-math','-arch',arch,str(source),'-o',str(exe)]
  start=time.monotonic();p=subprocess.run(command,capture_output=True,text=True);(out/f'compile-{arch}.log').write_text(p.stdout+p.stderr);commands.append(command)
  if p.returncode:raise RuntimeError('Tire draft compilation failed')
  invoke=[str(exe)] if arch=='arm64' else ['arch','-x86_64',str(exe)]
  p=subprocess.run(invoke+['--self-test'],capture_output=True,text=True);(out/f'arithmetic-{arch}.log').write_text(p.stdout+p.stderr)
  if p.returncode:raise RuntimeError('Tire native arithmetic checks failed')
  for run in runs:
   mode='inactive' if 'inactive' in run.name else 'active';answer=out/f'{mode}-{arch}.bin';log=out/f'{mode}-{arch}.log'
   command=invoke+[str(run/'inputs.bin'),str(answer)];commands.append(command);start=time.monotonic();p=subprocess.run(command,capture_output=True,text=True);elapsed=time.monotonic()-start;log.write_text(p.stdout+p.stderr)
   actual=answer.read_bytes();original=(run/'original.bin').read_bytes();report=json.loads((run/'report.json').read_text())
   result=dict(architecture=arch,mode=mode,exact=p.returncode==0 and actual==original,cases=report['cases'],mutable_output_words=report['mutable_output_words'],full_simulation_words=len(original)//4,seconds=elapsed,output=record(answer),log=record(log),executable=record(exe));results.append(result)
   print(json.dumps({k:result[k] for k in ('architecture','mode','exact','cases','mutable_output_words','seconds')}),flush=True)
   if not result['exact']:raise RuntimeError('Native tire output differs from original')
 after=[record(p) for p in sources]
 if before!=after:raise RuntimeError('Tire draft/source original vectors changed during verification')
 report=dict(schema='reflex.tire-native-draft-evidence.v1',checked_at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),success=all(r['exact'] for r in results),
  scope='Private instruction-derived native tire consumer, no registered public ABI or Unity installation. One initial mutable simulation image per original call; all raw final state bytes compared.',
  cases_per_architecture=1256,mutable_words_per_architecture=46824,arithmetic_directed_checks_per_architecture=6,
  review='Focused independent review of exact24/53-bit x87 adapter, memory aliasing and FP environment passed; a full independent opcode-by-opcode review is not claimed.',
  sources=before,commands=commands,results=results)
 (out/'report.json').write_text(json.dumps(report,indent=2)+'\n');return 0
if __name__=='__main__':raise SystemExit(main())

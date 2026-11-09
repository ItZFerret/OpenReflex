#!/usr/bin/env python3
"""Compare the private file and callable interfaces with accepted originals."""
from pathlib import Path
import datetime,hashlib,json,subprocess,time

ROOT=Path(__file__).resolve().parents[3]
def record(path):
 b=path.read_bytes();return dict(path=str(path.relative_to(ROOT)),bytes=len(b),sha256=hashlib.sha256(b).hexdigest())

def main():
 folder=Path(__file__).parent;out=ROOT/'_re/trace/out/bike-slot15-native-draft-original';out.mkdir(exist_ok=True)
 runs=[ROOT/'_re/trace/out'/name for name in ('original-math-bike-slot15-pilot-relocated','original-math-bike-slot15-generated')]
 sources=[folder/name for name in ('bike_slot15_draft.cpp','bike_slot15_callable.cpp','bike_slot15_callable.h','callable_tests.cpp','generate_program.py','program.inc','verify.py')]
 sources += [ROOT/'native/src/fp32.h',ROOT/'_re/trace/bike_slot15_program.json',ROOT/'_re/trace/bike_slot15_layout.json']
 for run in runs:
  report=json.loads((run/'report.json').read_text())
  if report.get('success') is not True or report.get('exact') is not True or record(run/'inputs.bin')!=report.get('inputs') or record(run/'original.bin')!=report.get('original'):
   raise ValueError('Original slot15 source is not accepted or has changed')
  sources += [run/name for name in ('inputs.bin','original.bin','report.json')]
 before=[record(p)for p in sources];commands=[];results=[]
 flags=['clang++','-std=c++17','-O2','-Wall','-Wextra','-Werror','-fno-fast-math','-ffp-contract=off','-frounding-math']
 def execute(command,log):
  commands.append(command);start=time.monotonic();p=subprocess.run(command,capture_output=True,text=True);log.write_text(p.stdout+p.stderr)
  if p.returncode:raise RuntimeError('Slot15 native command failed: '+str(log))
  return time.monotonic()-start
 for arch in ('arm64','x86_64'):
  exe=ROOT/f'native/build/bike-slot15-draft-{arch}';tests=ROOT/f'native/build/bike-slot15-callable-tests-{arch}'
  execute(flags+['-arch',arch,str(folder/'bike_slot15_draft.cpp'),'-o',str(exe)],out/f'compile-{arch}.log')
  execute(flags+['-arch',arch,str(folder/'bike_slot15_callable.cpp'),str(folder/'callable_tests.cpp'),'-o',str(tests)],out/f'compile-callable-{arch}.log')
  prefix=[]if arch=='arm64'else['arch','-x86_64']
  for run in runs:
   mode='pilot'if 'pilot'in run.name else'generated';output=out/f'{mode}-{arch}.bin'
   elapsed=execute(prefix+[str(exe),str(run/'inputs.bin'),str(output)],out/f'{mode}-{arch}.log')
   if output.read_bytes()!=(run/'original.bin').read_bytes():raise RuntimeError('Native full slot15 bytes differ')
   execute(prefix+[str(tests),str(run/'inputs.bin'),str(run/'original.bin')],out/f'{mode}-callable-{arch}.log')
   original=json.loads((run/'report.json').read_text())
   result=dict(architecture=arch,mode=mode,cases=original['cases'],exact=True,complete_record_words=original['output_words'],
    affected_output_words=original['affected_output_words'],seconds=elapsed,output=record(output),executable=record(exe),callable_test_executable=record(tests));results.append(result)
   print(json.dumps({k:result[k]for k in ('architecture','mode','cases','exact','affected_output_words')}),flush=True)
 library=ROOT/'native/build/libreflex_bike_slot15_draft.dylib'
 execute(flags+['-dynamiclib','-arch','arm64','-arch','x86_64',str(folder/'bike_slot15_callable.cpp'),'-o',str(library)],out/'compile-callable-universal.log')
 if before!=[record(p)for p in sources]:raise RuntimeError('Slot15 evidence/source changed during native verification')
 report=dict(schema='reflex.bike-slot15-native-draft-evidence.v1',success=True,checked_at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
  scope='Private complete443c30 slot15 from one generated shared state. Literal contact/world selectors, node7, velocity corrections and original virtual predicates. No public ABI, complete live step or gameplay claim.',
  callable_symbol='reflex_draft_bike_slot15',record_bytes=45900,contracts=['atomic failure','unaligned/overlapping buffers','FP controls/status preservation'],
  sources=before,commands=commands,results=results,universal_library=record(library))
 (out/'report.json').write_text(json.dumps(report,indent=2)+'\n');return 0
if __name__=='__main__':raise SystemExit(main())

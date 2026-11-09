#!/usr/bin/env python3
"""Compile both private contact_response drafts and compare fixed original output bytes."""
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
import datetime,hashlib,json,subprocess,time
ROOT=Path(__file__).resolve().parents[3]
def record(path):
 data=path.read_bytes();return dict(path=str(path.relative_to(ROOT)),bytes=len(data),sha256=hashlib.sha256(data).hexdigest())
def main():
 out=ROOT/'_re/trace/out/bike-contact-response-native-draft-original';out.mkdir(exist_ok=True)
 source=ROOT/'native/drafts/contact_response/contact_response_draft.cpp'
 runs=[ROOT/'_re/trace/out'/name for name in ('original-math-contact-response-pilot','original-math-contact-response-generated')]
 sources=[source,Path(__file__),source.with_name('program.inc'),source.with_name('generate_program.py'),ROOT/'native/src/fp32.h',ROOT/'_re/trace/bike_contact_response_program.json',ROOT/'_re/trace/bike_contact_response_layout.json']
 for run in runs:
  accepted=json.loads((run/'report.json').read_text())
  if accepted.get('success') is not True or accepted.get('exact') is not True or record(run/'inputs.bin')!=accepted.get('inputs') or record(run/'original.bin')!=accepted.get('original'):raise ValueError('Original contact response evidence changed')
  sources.extend(run/name for name in ('inputs.bin','original.bin','report.json'))
 before=[record(p) for p in sources];commands=[];results=[]
 for arch in ('arm64','x86_64'):
  exe=ROOT/f'native/build/contact_response-draft-{arch}'
  command=['clang++','-std=c++17','-O2','-Wall','-Wextra','-Werror','-fno-fast-math','-ffp-contract=off','-frounding-math','-arch',arch,str(source),'-o',str(exe)]
  start=time.monotonic();p=subprocess.run(command,capture_output=True,text=True);(out/f'compile-{arch}.log').write_text(p.stdout+p.stderr);commands.append(command)
  if p.returncode:raise RuntimeError('Contact response draft compilation failed')
  invoke=[str(exe)] if arch=='arm64' else ['arch','-x86_64',str(exe)]
  for run in runs:
   mode='pilot' if 'pilot' in run.name else 'generated';answer=out/f'{mode}-{arch}.bin';log=out/f'{mode}-{arch}.log'
   command=invoke+[str(run/'inputs.bin'),str(answer)];commands.append(command);start=time.monotonic();p=subprocess.run(command,capture_output=True,text=True);elapsed=time.monotonic()-start;log.write_text(p.stdout+p.stderr)
   actual=answer.read_bytes();original=(run/'original.bin').read_bytes();report=json.loads((run/'report.json').read_text())
   result=dict(architecture=arch,mode=mode,exact=p.returncode==0 and actual==original,cases=report['cases'],affected_output_words=report['affected_output_words'],complete_record_words=len(original)//4,seconds=elapsed,output=record(answer),log=record(log),executable=record(exe));results.append(result)
   print(json.dumps({k:result[k] for k in ('architecture','mode','exact','cases','affected_output_words','seconds')}),flush=True)
   if not result['exact']:raise RuntimeError('Native contact_response output differs from original')
 after=[record(p) for p in sources]
 if before!=after:raise RuntimeError('Contact response draft/source original vectors changed during verification')
 report=dict(schema='reflex.contact_response-native-draft-evidence.v1',checked_at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),success=all(r['exact'] for r in results),
  scope='Private complete4360b0 contact response, no registered public ABI or Unity integration. Real selector/index helpers execute on an explicit owned graph, one initial mutable image, all raw final state/runtime/region bytes compared.',
  accepted_generated_cases_per_architecture=1000,affected_generated_words_per_architecture=31636,
  review='Original harness independent reviews passed. Native focused review pending; frozen original outputs are the numerical reference.',
  sources=before,commands=commands,results=results)
 (out/'report.json').write_text(json.dumps(report,indent=2)+'\n');return 0
if __name__=='__main__':raise SystemExit(main())

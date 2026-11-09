#!/usr/bin/env python3
"""Compare the private full slot13 callable to accepted original executions."""
import argparse,datetime,hashlib,json,subprocess,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[3]
def record(path):
 path=Path(path);b=path.read_bytes();return dict(path=str(path.relative_to(ROOT)),bytes=len(b),sha256=hashlib.sha256(b).hexdigest())
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--reference',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
 ref=a.reference.resolve();out=a.output.resolve();out.mkdir(exist_ok=False);proof=json.loads((ref/'report.json').read_text())
 if not proof['success']or not proof['exact']:raise ValueError('Original slot13 proof not accepted')
 for key in('inputs','original'):
  if record(ROOT/proof[key]['path'])!=proof[key]:raise ValueError('Original input/output bytes changed')
 source=Path(__file__).with_name('slot13_draft.cpp');contract=source.with_name('contract.cpp')
 files=[source,contract,source.with_name('program.inc'),source.with_name('generate_program.py'),Path(__file__),ROOT/'native/src/fp32.h',ROOT/'_re/trace/bike_slot13_program.json',ref/'report.json',ref/'inputs.bin',ref/'original.bin'];before=[record(f)for f in files]
 results=[];commands=[]
 for arch in('arm64','x86_64'):
  binary=out/('slot13-'+arch);test=out/('contracts-'+arch);flags=['clang++','-std=c++17','-O2','-Wall','-Wextra','-Werror','-fno-fast-math','-ffp-contract=off','-frounding-math','-arch',arch]
  for name,cmd in(('draft',flags+[str(source),'-o',str(binary)]),('contract',flags+['-DREFLEX_SLOT13_NO_MAIN',str(source),str(contract),'-o',str(test)])):
   commands.append(cmd);r=subprocess.run(cmd,capture_output=True,text=True);(out/f'compile-{name}-{arch}.log').write_text(r.stdout+r.stderr)
   if r.returncode:raise RuntimeError('Native slot13 compile failed')
  prefix=[]if arch=='arm64'else['arch','-x86_64'];cmd=prefix+[str(test),str(ref/'inputs.bin')];commands.append(cmd);r=subprocess.run(cmd,capture_output=True,text=True)
  contract_log=out/f'contracts-{arch}.log';contract_log.write_text(r.stdout+r.stderr)
  if r.returncode:raise RuntimeError('Native slot13 contract failed')
  answer=out/f'original-{arch}.bin';log=out/f'original-{arch}.log';cmd=prefix+[str(binary),str(ref/'inputs.bin'),str(answer)];commands.append(cmd);start=time.monotonic();r=subprocess.run(cmd,capture_output=True,text=True);elapsed=time.monotonic()-start;log.write_text(r.stdout+r.stderr)
  item=dict(architecture=arch,cases=proof['cases'],compared_words=proof['output_words'],mutable_words=proof['mutable_output_words'],exact=r.returncode==0 and answer.read_bytes()==(ref/'original.bin').read_bytes(),seconds=elapsed,output=record(answer),log=record(log),executable=record(binary),contracts=record(contract_log));results.append(item);print(json.dumps(item),flush=True)
  if not item['exact']:raise RuntimeError('Native slot13 original bytes differ')
 if before!=[record(f)for f in files]:raise RuntimeError('Native slot13 proof sources changed')
 report=dict(schema='reflex.slot13-native-draft.v1',success=True,checked_at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),sources=before,commands=commands,results=results,
  scope='Private whole444b90 terrain-only stage; original12 body probes, actual wheel/body queries, six filters, joint corrections. Empty dynamic descriptor range is an explicit world contract. No liveGPU binding or full consecutive gameplay claim.',
  boundary='Full91152-byte input snapshotted;58360-byte output atomically committed. Arbitrary input/output overlap tested; supported FP required and caller environment restored. No public ABI registration.')
 (out/'report.json').write_text(json.dumps(report,indent=2)+'\n')
if __name__=='__main__':main()

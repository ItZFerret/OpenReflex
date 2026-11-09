#!/usr/bin/env python3
"""Build private contact drafts and compare ARM/Intel against actual original bytes."""
import argparse,datetime,hashlib,json,subprocess,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[3]
def record(path):
    data=path.read_bytes();return dict(path=str(path.relative_to(ROOT)),bytes=len(data),sha256=hashlib.sha256(data).hexdigest())
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--reference',type=Path,default=ROOT/'_re/trace/out/original-math-contact-generated');p.add_argument('--output',type=Path,default=ROOT/'_re/trace/out/bike-contact-native-draft-original');a=p.parse_args()
    ref=a.reference.resolve();out=a.output.resolve();out.mkdir(exist_ok=False);original_report=json.loads((ref/'report.json').read_text())
    if not original_report['success']:raise ValueError('Original oracle did not pass')
    source=Path(__file__).with_name('contact_draft.cpp');contract=source.with_name('contract.cpp')
    files=[source,contract,source.with_name('program.inc'),source.with_name('generate_program.py'),Path(__file__),ROOT/'native/src/fp32.h',ROOT/'_re/trace/bike_contact_program.json',ref/'report.json']
    for c in original_report['checks']:
        if not c['exact']:raise ValueError('Original mode did not pass')
        files += [ref/(c['mode']+'-inputs.bin'),ref/(c['mode']+'-original.bin')]
        for key in ('inputs','observed'):
            if record(ROOT/c[key]['path'])!=c[key]:raise ValueError('Original recorded input/output changed')
    before=[record(x) for x in files];results=[];commands=[];contracts=[]
    for arch in ('arm64','x86_64'):
        binary=out/('contact-'+arch);test=out/('contracts-'+arch)
        flags=['clang++','-std=c++17','-O2','-Wall','-Wextra','-Werror','-fno-fast-math','-ffp-contract=off','-frounding-math','-arch',arch]
        for name,command in [('draft',flags+[str(source),'-o',str(binary)]),('contract',flags+['-DREFLEX_CONTACT_NO_MAIN',str(source),str(contract),'-o',str(test)])]:
            commands.append(command);run=subprocess.run(command,capture_output=True,text=True);(out/f'compile-{name}-{arch}.log').write_text(run.stdout+run.stderr)
            if run.returncode:raise RuntimeError('Contact native compilation failed')
        invoke=[] if arch=='arm64' else ['arch','-x86_64']
        run=subprocess.run(invoke+[str(test)],capture_output=True,text=True);log=out/f'contracts-{arch}.log';log.write_text(run.stdout+run.stderr)
        contracts.append(dict(architecture=arch,success=run.returncode==0,log=record(log),executable=record(test)))
        if run.returncode:raise RuntimeError('Contact contracts failed')
        for check in original_report['checks']:
            mode=check['mode'];answer=out/f'{mode}-{arch}.bin';log=out/f'{mode}-{arch}.log';command=invoke+[str(binary),mode,str(ref/(mode+'-inputs.bin')),str(answer)];commands.append(command)
            start=time.monotonic();run=subprocess.run(command,capture_output=True,text=True);elapsed=time.monotonic()-start;log.write_text(run.stdout+run.stderr)
            result=dict(architecture=arch,mode=mode,cases=check['cases'],compared_words=check['output_words'],mutable_words=check['generation']['mutable_words'],exact=run.returncode==0 and answer.read_bytes()==(ref/(mode+'-original.bin')).read_bytes(),seconds=elapsed,output=record(answer),log=record(log),executable=record(binary));results.append(result);print(json.dumps(result),flush=True)
            if not result['exact']:raise RuntimeError('Contact original/native bytes differ')
    if before!=[record(x) for x in files]:raise RuntimeError('Contact dependencies changed during comparison')
    report=dict(schema='reflex.contact-native-draft.v1',success=True,checked_at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),sources=before,commands=commands,contracts=contracts,results=results,
        mutable_words_per_architecture=sum(c['generation']['mutable_words'] for c in original_report['checks']),
        scope='Unregistered instruction-derived numeric draft438260/42db20; generated supplied triangles and seeded wheel records. All raw output bytes compared; preserved words reported separately from mutable counts. No terrain/deformation/object-query or full bike-step claim.',
        floating_point='Nearest/even gradual-underflow SSE32 value semantics; no fused operations. Explicit invalid/NaN adapters for masked intermediate lanes; accepted final numerical outputs finite. Sticky exception flags are not compared.')
    (out/'report.json').write_text(json.dumps(report,indent=2)+'\n');return 0
if __name__=='__main__':raise SystemExit(main())

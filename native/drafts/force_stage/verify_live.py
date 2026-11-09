#!/usr/bin/env python3
"""Replay observed live whole-force boundary bytes, without model answers."""
from pathlib import Path
import argparse
import hashlib
import json
import struct
import subprocess
import time

ROOT=Path(__file__).resolve().parents[3]
STATE=0x9700


def record(path):
    path=path.resolve();data=path.read_bytes()
    return dict(path=str(path.relative_to(ROOT)),bytes=len(data),sha256=hashlib.sha256(data).hexdigest())


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('capture',type=Path,nargs='+');args=parser.parse_args()
    out=ROOT/'_re/trace/out/bike-force-stage-native-draft-live';out.mkdir(exist_ok=True)
    proof_path=ROOT/'_re/trace/out/bike-force-stage-native-draft-original/report.json'
    proof=json.loads(proof_path.read_text());sources=[Path(__file__),proof_path]
    for item in proof['sources']:
        if item['path'].startswith('native/'):
            path=ROOT/item['path']
            if record(path)!=item:raise ValueError('Source differs from original native proof')
            sources.append(path)
    for item in proof['results']:
        exe=ROOT/item['executable']['path']
        if record(exe)!=item['executable']:raise ValueError('Binary differs from original native proof')
    for capture in args.capture:sources.extend((capture/'force_stage.jsonl',capture/'result.json'))
    before=[record(path) for path in sources];results=[]
    for capture in args.capture:
        accepted=json.loads((capture/'result.json').read_text())
        if accepted.get('success') is not True:raise ValueError('Capture was not accepted')
        calls={};event=0
        with (capture/'force_stage.jsonl').open() as stream:
            for line in stream:
                row=json.loads(line);event+=1
                if row['event_id']!=event:raise ValueError('Observed event sequence changed')
                pair=calls.setdefault(row['call_id'],{})
                if row['phase'] in pair:raise ValueError('Duplicate observed phase')
                pair[row['phase']]=row
        if set(calls)!=set(range(1,len(calls)+1)):raise ValueError('Observed calls incomplete')
        inputs=bytearray();answers=bytearray()
        for pair in calls.values():
            if set(pair)!={'entry','exit'}:raise ValueError('Observed pair incomplete')
            row=pair['entry'];profile=int.from_bytes(bytes.fromhex(row['conversion_flag_bits']),'little')
            if profile not in (0,1) or row['x87_cw'] not in ('007f','027f'):raise ValueError('Unsupported observed profile')
            profile|=2 if row['x87_cw']=='027f' else 0
            inputs+=bytes.fromhex(row['state_bits'])+bytes.fromhex(row['control_bits'])
            inputs+=b''.join(bytes.fromhex(row['globals_bits'][name]) for name in ('axis_x','axis_y','numerator','g'))
            inputs+=struct.pack('<III',int(row['dt_bits'],16),0,profile)
            answers+=bytes.fromhex(pair['exit']['state_bits'])
        input_path=out/(capture.name+'-inputs.bin');input_path.write_bytes(inputs)
        expected_path=out/(capture.name+'-original.bin');expected_path.write_bytes(answers)
        for architecture in ('arm64','x86_64'):
            exe=ROOT/f'native/build/force-stage-draft-{architecture}'
            output=out/(capture.name+'-'+architecture+'.bin');log=output.with_suffix('.log')
            invoke=[str(exe)] if architecture=='arm64' else ['arch','-x86_64',str(exe)]
            start=time.monotonic();process=subprocess.run(invoke+[str(input_path),str(output)],capture_output=True,text=True)
            elapsed=time.monotonic()-start;log.write_text(process.stdout+process.stderr)
            exact=process.returncode==0 and output.read_bytes()==answers
            result=dict(capture=capture.name,architecture=architecture,calls=len(calls),full_simulation_words=len(answers)//4,
                exact=exact,seconds=elapsed,input=record(input_path),original=record(expected_path),output=record(output),log=record(log),executable=record(exe))
            results.append(result);print(json.dumps({key:result[key] for key in ('capture','architecture','calls','exact','seconds')}),flush=True)
            if not exact:raise ValueError('Native live whole-force replay differs')
    if before!=[record(path) for path in sources]:raise ValueError('Inputs or proof sources changed during replay')
    report=dict(schema='reflex.force-stage-native-live-evidence.v1',success=True,
        scope='Offline native replay of accepted original live43e100 entry and exit bytes. Existing strict runner audit is reused as capture evidence; no model-generated numerical answers or cross-frame state feedback are used.',
        sources=before,sources_unchanged=True,results=results)
    (out/'report.json').write_text(json.dumps(report,indent=2)+'\n')


if __name__=='__main__':main()

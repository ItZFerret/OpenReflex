#!/usr/bin/env python3
"""Prepare and replay the raw-state adapter from accepted connected original data."""
import argparse,ctypes as C,hashlib,json,struct,subprocess,sys,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(ROOT/'_re/trace'))
from bike_force_core_model import merge_core_payload
GLOBAL_ORDER=('axis_x','axis_y','axis_z','g','sign_x','sign_y','sign_z','conjugate','mask','one','two','negative_two','negative_zero','numerator','extra')
def pin(path):
 path=Path(path).resolve();b=path.read_bytes();return dict(path=str(path),bytes=len(b),sha256=hashlib.sha256(b).hexdigest())
def prepare(capture,library,out):
 accepted=ROOT/'_re/trace/out/force-core-connected-live.json';proof=json.loads(accepted.read_text());assert proof['success'] and proof['calls']==942
 files=[capture/'force_stage.jsonl',capture/'mechanism_core.jsonl'];pinned={p['path']:p for p in proof['files']}
 for p in files:assert pin(p)==pinned[str(p.resolve())],'Accepted original stream changed'
 sources=[pin(p) for p in [accepted,library,*files,Path(__file__),ROOT/'_re/trace/bike_force_core_model.py']]
 lib=C.CDLL(str(library));force=lib.reflex_bike_force_stage;force.argtypes=[C.c_void_p,C.c_uint32,C.c_void_p,C.c_uint32];force.restype=C.c_uint32
 cores={}
 for line in files[1].open():
  row=json.loads(line);key=(row['thread'],row['parent_step']);assert row['phase'] not in cores.setdefault(key,{})
  cores[key][row['phase']]=row
 pending={};claimed=set();calls=0;active=0
 with (out/'inputs.bin').open('wb') as inputs,(out/'expected.bin').open('wb') as expected:
  for line in files[0].open():
   row=json.loads(line);key=(row['thread'],row['parent_step'])
   if row['phase']=='entry':assert key not in pending and key not in claimed;pending[key]=row;continue
   before=pending.pop(key);core=cores[key];entry=core['entry'];after=core['exit']
   for field in ('simulation','simulation_vtable','vehicle_ptr','dt_bits','x87_cw','outer_step','parent_scheduler'):
    assert before[field]==row[field]==entry[field]==after[field]
   assert before['solver_event_id']==row['solver_event_id']==entry['solver_event_id']
   assert all(entry['globals_bits'][k]==v for k,v in before['globals_bits'].items())
   raw=bytes.fromhex(before['state_bits'])+bytes.fromhex(before['control_bits'])
   raw+=b''.join(bytes.fromhex(before['globals_bits'][k])for k in ('axis_x','axis_y','numerator','g'))
   raw+=struct.pack('<3I',int(before['dt_bits'],16),int.from_bytes(bytes.fromhex(before['conversion_flag_bits']),'little'),int(before['x87_cw'],16))
   source=C.create_string_buffer(raw);result=C.create_string_buffer(38656)
   assert force(source,len(raw),result,len(result))==0 and result.raw==bytes.fromhex(row['state_bits'])
   forced=result.raw;simulation=int(before['simulation'],16)
   adapter=forced+struct.pack('<4I',simulation,0x400000,int(before['dt_bits'],16),0)
   adapter+=b''.join(bytes.fromhex(entry['globals_bits'][k])for k in GLOBAL_ORDER);assert len(adapter)==38864
   inputs.write(adapter);expected.write(merge_core_payload(forced,simulation,after));calls+=1;active+=before['capture_eligible'];claimed.add(key)
 assert not pending and claimed==set(cores) and calls==proof['calls']
 assert sources==[pin(p['path'])for p in sources]
 return dict(calls=calls,active_calls=active,sources=sources,input=pin(out/'inputs.bin'),expected=pin(out/'expected.bin'))
def main():
 p=argparse.ArgumentParser();p.add_argument('--capture',type=Path,default=ROOT/'_re/trace/out/20261001T224811.827298Z-alpine-force-core-bike');p.add_argument('--library',type=Path,default=ROOT/'native/build-v24-canonical/libreflexcore.dylib');p.add_argument('--output',type=Path,required=True);a=p.parse_args()
 a.output.mkdir(exist_ok=False);report=prepare(a.capture.resolve(),a.library.resolve(),a.output);results=[];source=Path(__file__).with_name('bike_core_bridge.cpp');contract=source.with_name('contract.cpp')
 code=[pin(p)for p in (source,contract,ROOT/'native/include/reflex_core.h',ROOT/'native/src/fp32.h')]
 for arch,libdir in (('arm64',ROOT/'native/build-v24-canonical'),('x86_64',ROOT/'native/build-universal')):
  binary=a.output/('contracts-'+arch);cmd=['clang++','-std=c++17','-O2','-Wall','-Wextra','-Werror','-fno-fast-math','-ffp-contract=off','-frounding-math','-arch',arch,str(source),str(contract),'-L'+str(libdir),'-lreflexcore','-Wl,-rpath,'+str(libdir),'-o',str(binary)]
  r=subprocess.run(cmd,capture_output=True,text=True);(a.output/f'compile-{arch}.log').write_text(r.stdout+r.stderr);assert r.returncode==0,r.stderr
  start=time.monotonic();cmd=([]if arch=='arm64'else['arch','-x86_64'])+[str(binary),str(a.output/'inputs.bin'),str(a.output/'expected.bin')]
  r=subprocess.run(cmd,capture_output=True,text=True);(a.output/f'contracts-{arch}.log').write_text(r.stdout+r.stderr);assert r.returncode==0,r.stderr
  results.append(dict(architecture=arch,calls=report['calls'],raw_words=report['calls']*9664,seconds=time.monotonic()-start,exact=True,library=pin(libdir/'libreflexcore.dylib'),executable=pin(binary),log=pin(a.output/f'contracts-{arch}.log')));print(json.dumps(results[-1]),flush=True)
 assert code==[pin(p['path'])for p in code]
 report.update(schema='reflex.bike-core-raw-adapter.v1',success=True,results=results,code=code,scope='Native-computed full force output is the sole mutable raw input to the core bridge. Every captured final core byte matches original; full state outside those spans is preserved from input and is not independently observed at core exit. No new gameplay or original whole-step claim.')
 (a.output/'report.json').write_text(json.dumps(report,indent=2)+'\n')
if __name__=='__main__':main()

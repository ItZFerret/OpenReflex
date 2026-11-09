"""Private native composition check against independent controls+ray models.

This is composition parity, not a new unchanged-original full parent capture.
The two component original proofs are pinned explicitly.
"""
import argparse,ctypes,datetime,hashlib,json,subprocess,sys,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[3];TRACE=ROOT/'_re/trace';sys.path.insert(0,str(TRACE))

def record(p):
 p=Path(p).resolve();b=p.read_bytes();return dict(path=str(p.relative_to(ROOT)),bytes=len(b),sha256=hashlib.sha256(b).hexdigest())
def replay(library,inputs,expected,output):
 lib=ctypes.CDLL(str(library));f=lib.reflex_draft_bike_controls_terrain;f.argtypes=[ctypes.c_void_p,ctypes.c_uint32,ctypes.c_void_p,ctypes.c_uint32];f.restype=ctypes.c_uint32
 raw=Path(inputs).read_bytes();truth=Path(expected).read_bytes();count=len(raw)//55756;answer=bytearray()
 if len(raw)%55756 or len(truth)!=count*39188:raise ValueError('Record sizes')
 for i in range(count):
  row=ctypes.create_string_buffer(raw[i*55756:(i+1)*55756]);out=ctypes.create_string_buffer(39188);status=f(row,55756,out,39188)
  if status or out.raw!=truth[i*39188:(i+1)*39188]:raise ValueError(f'Controls/terrain native mismatch{ i } status{status}')
  answer.extend(out.raw)
 Path(output).write_bytes(answer);print(json.dumps(dict(calls=count,full_output_words=len(answer)//4,exact=True)))
 if count:
  row=raw[:55756];truth0=truth[:39188];overlaps=0
  for delta in(-39188,-4,0,1,4,16,1024,30000,55752):
   memory=ctypes.create_string_buffer(55756+2*39188+1024);base=ctypes.addressof(memory)+39188+512;ctypes.memmove(base,row,len(row));before=memory.raw
   if f(base,55756,base+delta,39188)or ctypes.string_at(base+delta,39188)!=truth0:raise ValueError('Overlap mismatch')
   start=39188+512+delta;after=memory.raw
   if after[:start]!=before[:start]or after[start+39188:]!=before[start+39188:]:raise ValueError('Write outside output')
   overlaps+=1
  sentinel=bytes([0x6b])*39188
  for pos,value in((39128,4),(39132+24,1),(39132+44,0),(39132+12,0),(0x5c30,0)):
   bad=bytearray(row);bad[pos:pos+4]=value.to_bytes(4,'little');out=ctypes.create_string_buffer(sentinel,39188)
   if f(bytes(bad),55756,out,39188)==0 or out.raw!=sentinel:raise ValueError('Atomic error failed')
  print(json.dumps(dict(arbitrary_overlaps=overlaps,atomic_join_errors=5)))

def main():
 if len(sys.argv)>1 and sys.argv[1]=='--replay':replay(*sys.argv[2:]);return
 p=argparse.ArgumentParser();p.add_argument('--cases',type=int,default=256);p.add_argument('--output',type=Path,required=True);a=p.parse_args();out=a.output.resolve();out.mkdir(exist_ok=False)
 from bike_controls_terrain_cases import input_record
 from bike_controls_terrain_model import calculate_record
 src=Path(__file__).parent;files=[src/x for x in('controls_terrain_draft.cpp','terrain_ray_bridge.cpp','controls_terrain_callable.h','record-layout.json','verify.py')]
 files +=[ROOT/'native/drafts/terrain_ray/terrain_ray_draft.cpp',ROOT/'native/drafts/terrain_ray/program.inc',ROOT/'native/drafts/controls/program.inc',ROOT/'native/src/fp32.h']
 files +=[TRACE/x for x in('bike_controls_terrain_model.py','bike_controls_terrain_cases.py','bike_controls_model.py','bike_controls_program.json','terrain_ray_model.py','terrain_ray_program.json','bike_controls_cases.py','bike_tire_model.py','bike_tire_fp.py','bike_tire_program.py','bike_tire_program.json','bike_contact_model.py','bike_contact_program.json','scheduler_model.py','pose_model.py')]
 refs=[TRACE/'out/original-math-controls-generated/report.json',TRACE/'out/original-math-terrain-ray-generated/report.json']
 for r in refs:
  if not json.loads(r.read_text())['success']:raise ValueError('Component original proof failed')
 before=[record(p)for p in files+refs];rows=[];answers=[];paths=dict(skipped=0,hit=0,miss=0)
 for i in range(a.cases):
  row=input_record(i//4,i%4);answer,trace=calculate_record(row);rows.append(row);answers.append(answer);qs=trace['terrain_queries'];paths['skipped'if not qs else'hit'if int.from_bytes(qs[0]['answer'][:4],'little')else'miss']+=1
 (out/'inputs.bin').write_bytes(b''.join(rows));(out/'expected.bin').write_bytes(b''.join(answers));results=[]
 for arch in('arm64','x86_64'):
  lib=out/f'controls-terrain-{arch}.dylib';cmd=['clang++','-std=c++17','-O2','-Wall','-Wextra','-Werror','-fno-fast-math','-ffp-contract=off','-frounding-math','-arch',arch,'-dynamiclib',str(src/'controls_terrain_draft.cpp'),str(src/'terrain_ray_bridge.cpp'),'-o',str(lib)]
  r=subprocess.run(cmd,capture_output=True,text=True);(out/f'compile-{arch}.log').write_text(r.stdout+r.stderr)
  if r.returncode:raise RuntimeError('Compilation failed')
  command=[sys.executable,str(__file__),'--replay']if arch=='arm64'else['arch','-x86_64','/usr/bin/python3',str(__file__),'--replay']
  command +=[str(lib),str(out/'inputs.bin'),str(out/'expected.bin'),str(out/f'actual-{arch}.bin')]
  r=subprocess.run(command,capture_output=True,text=True);log=out/f'check-{arch}.log';log.write_text(r.stdout+r.stderr)
  if r.returncode:raise RuntimeError(log.read_text())
  results.append(dict(architecture=arch,library=record(lib),log=record(log),output=record(out/f'actual-{arch}.bin')))
 if before!=[record(p)for p in files+refs]:raise ValueError('Sources changed')
 report=dict(schema='reflex.private-controls-terrain-composition.v1',success=True,checked_at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),scope='Native controls44c170 invokes actual recovered7c6cc0 traversal on supplied base grid/coarse bounds and current shared control-stack query vectors. No supplied hit Boolean. Component original proofs are pinned; this new connected comparison is native versus independent exact models, not a whole-original connected capture.',cases=a.cases,full_output_words=a.cases*9797,paths=paths,sources=before,results=results,input=record(out/'inputs.bin'),expected=record(out/'expected.bin'))
 (out/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(dict(success=True,cases=a.cases,paths=paths,report=str(out/'report.json'))))
if __name__=='__main__':main()

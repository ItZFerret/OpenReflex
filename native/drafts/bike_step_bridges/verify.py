#!/usr/bin/env python3
"""Compare five native whole-state bridges with adjacent original captures."""
from pathlib import Path
import collections,datetime,hashlib,json,struct,subprocess,sys,time

ROOT=Path(__file__).resolve().parents[3];TRACE=ROOT/'_re/trace'
CAPTURE=TRACE/'out/20261001T235505.956913Z-alpine-bike-step-pilot'
CANDIDATE=TRACE/'candidates/bike-step-v25b'
GLOBALS=[(0xd615c0,16),(0xd616a0,16),(0xd61680,16),(0xd61670,16),
 (0x93a1f0,16),(0x93a200,16),(0x93a210,16),(0x93a220,16),(0x92db80,16),
 (0x931f24,4),(0x92e330,4),(0x9d7b30,4),(0x9774a8,4),(0xd61660,16)]
def record(path):
 h=hashlib.sha256()
 with path.open('rb')as f:
  while block:=f.read(1<<20):h.update(block)
 return dict(path=str(path.relative_to(ROOT)),bytes=path.stat().st_size,sha256=h.hexdigest())

def local_dependencies():
 paths=set()
 for module in tuple(sys.modules.values()):
  name=getattr(module,'__file__',None)
  if name:
   path=Path(name).resolve()
   if path.is_relative_to(ROOT) and path.suffix=='.py':paths.add(path)
 return paths

def main():
 folder=Path(__file__).parent;out=TRACE/'out/bike-step-bridges-native-live';out.mkdir(exist_ok=True)
 source_paths=[folder/n for n in ('bike_step_bridges.cpp','bike_step_bridges.h','replay.cpp','verify.py')]
 source_paths += [ROOT/'native/src'/n for n in ('pose.cpp','fp32.h','node6_draft.cpp','node2_draft.cpp','bike_postpose_draft.cpp','bike_poststep_draft.cpp','bike_force_draft.cpp')]
 source_paths += [ROOT/'native/include/reflex_core.h',*(CAPTURE/n for n in ('bike_stage.jsonl','force_stage.jsonl','solver.csv','physics.csv','result.json'))]
 sys.path.insert(0,str(CANDIDATE/'runtime'))
 from bike_stage_validate import check_bike_stage
 # Pin the actual complete local import closure, including protocol helpers and
 # transitive arithmetic modules imported by the candidate validator. No audit
 # result is trusted before these fingerprints are frozen.
 dependencies=local_dependencies()
 source_paths=sorted(set(source_paths)|dependencies)
 before=[record(p)for p in source_paths]
 run=json.loads((CAPTURE/'result.json').read_text())
 if run.get('success')is not True:raise ValueError('Original capture was not accepted')
 audit=check_bike_stage(CAPTURE/'bike_stage.jsonl',CAPTURE/'force_stage.jsonl',CAPTURE/'solver.csv',CAPTURE/'physics.csv')
 if not audit['valid']:raise ValueError('Fresh observer audit failed: '+audit['reason'])
 counts=collections.Counter();eligible=collections.Counter();history={}
 with (out/'inputs.bin').open('wb')as inputs,(out/'original.bin').open('wb')as originals:
  def emit(stage,a,b):
   state=bytes.fromhex(a['state_bits']);control=bytes.fromhex(a['control_bits'])
   runtime=b''.join(bytes.fromhex(a['globals_bits'][f'{address:08x}'])for address,size in GLOBALS)
   if len(state)!=38656 or len(control)!=256 or len(runtime)!=176:raise ValueError('Bridge snapshot layout')
   row=state+control+struct.pack('<II',int(a['simulation'],16),int(a['dt_bits'],16))+runtime
   if len(row)!=39096:raise ValueError('Bridge record size')
   inputs.write(struct.pack('<I',stage)+row);originals.write(bytes.fromhex(b['state_bits']));counts[stage]+=1;eligible[stage]+=a['capture_eligible']
  for line in (CAPTURE/'bike_stage.jsonl').open():
   row=json.loads(line);step=row['parent_step'];slot=row['slot'];phase=row['phase'];h=history.setdefault(step,{})
   if(slot,phase)==(16,'entry'):emit(1,h[(15,'exit')],row)
   elif(slot,phase)==(16,'exit'):emit(2,h[(16,'entry')],row)
   elif(slot,phase)==(13,'entry'):emit(3,h[(16,'exit')],row)
   elif(slot,phase)==(17,'exit'):emit(4,h[(17,'entry')],row)
   elif(slot,phase)==(0,'exit'):emit(5,h[(17,'exit')],row)
   h[(slot,phase)]=row
   if(slot,phase)==(0,'exit'):del history[step]
 if history or dict(counts)!={i:549 for i in range(1,6)}:raise ValueError('Incomplete bridge coverage')
 flags=['clang++','-std=c++17','-O2','-Wall','-Wextra','-Werror','-fno-fast-math','-ffp-contract=off','-frounding-math','-fvisibility=hidden','-I',str(ROOT/'native/include')]
 commands=[];results=[]
 def command(args,log):
  commands.append(args);start=time.monotonic();p=subprocess.run(args,capture_output=True,text=True);log.write_text(p.stdout+p.stderr)
  if p.returncode:raise RuntimeError('Native bridge command failed: '+str(log))
  return time.monotonic()-start
 for arch in ('arm64','x86_64'):
  exe=ROOT/f'native/build/bike-step-bridges-replay-{arch}'
  command(flags+['-arch',arch,str(folder/'bike_step_bridges.cpp'),str(folder/'replay.cpp'),'-o',str(exe)],out/f'compile-{arch}.log')
  invocation=[str(exe)]if arch=='arm64'else['arch','-x86_64',str(exe)]
  result_path=out/f'output-{arch}.bin';elapsed=command(invocation+[str(out/'inputs.bin'),str(out/'original.bin'),str(result_path)],out/f'replay-{arch}.log')
  if record(result_path)['sha256']!=record(out/'original.bin')['sha256']:raise ValueError('Native bridge result differs')
  result=dict(architecture=arch,exact=True,calls=sum(counts.values()),complete_state_words=sum(counts.values())*38656//4,seconds=elapsed,executable=record(exe),output=record(result_path));results.append(result);print(json.dumps({k:result[k]for k in ('architecture','exact','calls','complete_state_words')}),flush=True)
 lib=ROOT/'native/build/libreflex_bike_step_bridges_draft.dylib'
 command(flags+['-dynamiclib','-arch','arm64','-arch','x86_64',str(folder/'bike_step_bridges.cpp'),'-o',str(lib)],out/'compile-universal.log')
 if local_dependencies()!=dependencies:raise ValueError('Audit imported an unpinned local dependency')
 if before!=[record(p)for p in source_paths]:raise ValueError('Bridge sources/captures changed during verification')
 report=dict(schema='reflex.bike-step-bridges-native-live.v1',success=True,checked_at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
  scope='Five adjacent full-state original bridges from current inputs. No complete frame, external-world closure, or consecutive gameplay claim.',
  input_bytes=39096,output_bytes=38656,stages={1:'pose',2:'steering_anchors',3:'node6',4:'post_contact',5:'filtered_speed'},counts=dict(counts),eligible_counts=dict(eligible),fresh_observer_audit=audit,
  sources=before,commands=commands,results=results,inputs=record(out/'inputs.bin'),original=record(out/'original.bin'),universal_library=record(lib))
 (out/'report.json').write_text(json.dumps(report,indent=2)+'\n');return 0
if __name__=='__main__':raise SystemExit(main())

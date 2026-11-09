"""Replay the private callable against hash-bound accepted original outputs."""
import ctypes,datetime,hashlib,json,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[3];HERE=Path(__file__).parent
SOURCE=ROOT/'_re/trace/out/original-math-slot12-final-generated'
OUT=ROOT/'_re/trace/out/bike-slot12-native-draft-original'
def rec(p):
 p=Path(p).resolve();b=p.read_bytes();return dict(path=str(p.relative_to(ROOT)),bytes=len(b),sha256=hashlib.sha256(b).hexdigest())
def replay(lib,inputs,truth,out):
 l=ctypes.CDLL(lib);f=l.reflex_bike_slot12_draft;f.argtypes=[ctypes.c_void_p,ctypes.c_size_t,ctypes.c_void_p,ctypes.c_size_t];f.restype=ctypes.c_int
 data=Path(inputs).read_bytes();expected=Path(truth).read_bytes();answer=bytearray();count=len(data)//39104
 assert len(data)%39104==0 and len(expected)==count*39096
 for i in range(count):
  src=ctypes.create_string_buffer(data[i*39104:(i+1)*39104]);dst=ctypes.create_string_buffer(39096);status=f(src,39104,dst,39096)
  if status or dst.raw!=expected[i*39096:(i+1)*39096]:raise ValueError(f'Callable mismatch case{i}, status{status}')
  answer.extend(dst.raw)
 Path(out).write_bytes(answer);print(json.dumps(dict(calls=count,full_output_words=len(answer)//4,exact=True)))
def main():
 if len(sys.argv)>1 and sys.argv[1]=='--replay':replay(*sys.argv[2:]);return
 OUT.mkdir(exist_ok=False);proof=json.loads((SOURCE/'report.json').read_text());assert proof['success'] and proof['exact'] and proof['returncode']==0
 for key in('inputs','expected','original'):
  assert rec(ROOT/proof[key]['path'])==proof[key],key
 files=[HERE/x for x in('slot12_draft.cpp','slot12_callable.h','program.inc','generate_program.py','contract.cpp','verify.py')]+[ROOT/'native/src/fp32.h',SOURCE/'report.json']
 baseline=[rec(p)for p in files];results=[]
 for arch in('arm64','x86_64'):
  flags=['clang++','-std=c++17','-O2','-Wall','-Wextra','-Werror','-fno-fast-math','-ffp-contract=off','-frounding-math','-arch',arch,'-DREFLEX_SLOT12_NO_MAIN']
  lib=OUT/f'slot12-{arch}.dylib';contract=OUT/f'contract-{arch}'
  for name,cmd in(('library',flags+['-dynamiclib',str(HERE/'slot12_draft.cpp'),'-o',str(lib)]),('contract',flags+[str(HERE/'slot12_draft.cpp'),str(HERE/'contract.cpp'),'-o',str(contract)])):
   p=subprocess.run(cmd,capture_output=True,text=True);(OUT/f'compile-{arch}-{name}.log').write_text(p.stdout+p.stderr);assert p.returncode==0,p.stderr
  prefix=[sys.executable]if arch=='arm64'else['arch','-x86_64','/usr/bin/python3']
  p=subprocess.run(prefix+[str(__file__),'--replay',str(lib),str(SOURCE/'inputs.bin'),str(SOURCE/'original.bin'),str(OUT/f'actual-{arch}.bin')],capture_output=True,text=True)
  (OUT/f'check-{arch}.log').write_text(p.stdout+p.stderr);assert p.returncode==0,p.stdout+p.stderr
  p=subprocess.run(([]if arch=='arm64'else['arch','-x86_64'])+[str(contract),str(SOURCE/'inputs.bin'),str(SOURCE/'original.bin')],capture_output=True,text=True)
  (OUT/f'contract-{arch}.log').write_text(p.stdout+p.stderr);assert p.returncode==0,p.stdout+p.stderr
  results.append(dict(architecture=arch,library=rec(lib),output=rec(OUT/f'actual-{arch}.bin'),check=rec(OUT/f'check-{arch}.log'),contract=rec(OUT/f'contract-{arch}.log')))
 assert baseline==[rec(p)for p in files]
 report=dict(schema='reflex.private-slot12-native-original.v1',success=True,checked_at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),cases=proof['cases'],full_output_words_per_architecture=proof['output_words'],mutable_output_words_per_architecture=proof['mutable_output_words'],sources=baseline,inputs=proof['inputs'],original=proof['original'],coverage=proof['coverage'],results=results,scope='Original whole443610 rider correction, ground/air/alternate branches and full owned-state output. Private record API, no public ABI registration. Native atan2 has tested bit parity against actual VC90 only, not a universal CRT claim. Arbitrary overlaps, unaligned input and atomic failures; host FP environment restored.')
 (OUT/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(dict(success=True,cases=proof['cases'],full_words=proof['output_words'],report=str(OUT/'report.json'))))
if __name__=='__main__':main()

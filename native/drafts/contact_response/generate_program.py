#!/usr/bin/env python3
"""Emit typed non-executable instruction data for the private native draft."""
import hashlib,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[3]
source=ROOT/'_re/trace/bike_contact_response_program.json';raw=source.read_bytes();p=json.loads(raw)
assert p['exe_sha256']=='917d14e7ef7ce492b665fea7d0c87c9a39b549e2a43b09883d2a786988e7c02c'
regs={v:i for i,v in enumerate(('eax','ebx','ecx','edx','esi','edi','ebp','esp')+tuple(f'xmm{i}' for i in range(8)))}
regs.update(al=16)
# The copied private interpreter has additional unreachable tire operations.
old=json.loads((ROOT/'_re/trace/bike_tire_program.json').read_text())
controls=json.loads((ROOT/'_re/trace/bike_controls_program.json').read_text())
ops=sorted({r[2] for r in p['instructions']} | {r[2] for r in old['instructions']} | {r[2] for r in controls['instructions']})
lines=['// Generated data only. Source JSON SHA256 '+hashlib.sha256(raw).hexdigest(),
'enum Opcode { '+','.join('OP_'+x for x in ops)+' };',
'static const Instruction program[] = {']
for at,size,opcode,operands in p['instructions']:
 encoded=[]
 for o in operands:
  kind,width,*_=o
  if kind=='r':fields=(0,width,regs[o[2]],-1,-1,0,0,0)
  elif kind=='i':fields=(1,width,-1,-1,-1,0,0,o[2]&0xffffffffffffffff)
  else:fields=(2,width,-1,regs.get(o[2],-1),regs.get(o[3],-1),o[4],o[5],0)
  encoded.append('{'+','.join(str(v)+'ull' if j==7 else str(v) for j,v in enumerate(fields))+'}')
 encoded+=['{}']*(3-len(encoded))
 lines.append('{'+f'{at}u,{size},OP_{opcode},{len(operands)},'+'{'+','.join(encoded)+'}},')
lines+=['};','static const Constant constants[] = {']
for at,bits in p['constants'].items():
 b=bytes.fromhex(bits);lines.append('{'+str(at)+'u,'+str(len(b))+',{'+','.join(str(x) for x in b)+'}},')
lines+=['};','struct RuntimeSpan {uint32_t address;size_t size;};','static const RuntimeSpan runtime_spans[] = {']
for at,size in sorted((int(a),n) for a,n in p['runtime_spans'].items()):lines.append('{'+str(at)+'u,'+str(size)+'},')
lines+=['};']
layout=json.loads((ROOT/'_re/trace/bike_contact_response_layout.json').read_text())
lines+=['struct Region {uint32_t address;size_t size;bool writable;};','static const Region input_regions[] = {']
for r in layout['regions']:lines.append('{'+str(r['address'])+'u,'+str(r['bytes'])+','+str(r['writable']).lower()+'},')
lines+=['};']
Path(__file__).with_name('program.inc').write_text('\n'.join(lines)+'\n')
print(len(p['instructions']),'instructions',len(p['constants']),'constant spans')

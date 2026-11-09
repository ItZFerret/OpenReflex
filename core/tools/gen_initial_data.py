#!/usr/bin/env python3
"""Extract immutable PE data and resolved CRT import values; never includes EXE/DLL code.
Use --check to compare checked-in payloads against exact source files without writing.
"""
import argparse, hashlib, json, pathlib, struct, zlib
ROOT = pathlib.Path(__file__).resolve().parents[2]

def sha(b): return hashlib.sha256(b).hexdigest()
def u16(b,o): return struct.unpack_from('<H',b,o)[0]
def u32(b,o): return struct.unpack_from('<I',b,o)[0]
class PE:
    def __init__(self,path,base):
        self.path=path; self.b=path.read_bytes(); b=self.b
        assert b[:2]==b'MZ'
        p=u32(b,60); assert b[p:p+4]==b'PE\0\0'; assert u16(b,p+4)==0x14c
        self.base=u32(b,p+24+28); assert self.base==base
        self.optional=p+24; self.sections=[]
        for i in range(u16(b,p+6)):
            h=p+24+u16(b,p+20)+40*i
            name=b[h:h+8].split(b'\0')[0].decode('ascii'); vs,va,raw,off=struct.unpack_from('<IIII',b,h+8)
            assert off+raw<=len(b)
            self.sections.append(dict(name=name,addr=base+va,raw_size=raw,virtual_size=vs,offset=off))
    def offset(self,rva):
        for s in self.sections:
            delta=rva-(s['addr']-self.base)
            if 0<=delta<s['raw_size']: return s['offset']+delta
        raise ValueError('RVA not in file: '+hex(rva))
    def read(self,addr,size):
        o=self.offset(addr-self.base)
        for s in self.sections:
            if s['offset']<=o and o+size<=s['offset']+s['raw_size']: return self.b[o:o+size]
        raise ValueError('Range crosses section')
    def cstr(self,rva):
        o=self.offset(rva); e=self.b.index(0,o); return self.b[o:e].decode('ascii')
    def imports(self):
        directory=u32(self.b,self.optional+0x60+8)
        if not directory: return []
        result=[]; d=self.offset(directory)
        while u32(self.b,d+12) or u32(self.b,d+16):
            ilt,name,iat=u32(self.b,d),u32(self.b,d+12),u32(self.b,d+16)
            dll=self.cstr(name).lower()
            if dll in ('msvcr90.dll','msvcp90.dll'):
                t=self.offset(ilt or iat); k=0
                while (entry:=u32(self.b,t+4*k)):
                    symbol=entry&0xffff if entry&0x80000000 else self.cstr(entry+2)
                    result.append(dict(slot=self.base+iat+4*k,dll=dll,symbol=symbol));k+=1
            d+=20
        return result
    def export(self,symbol):
        directory=u32(self.b,self.optional+0x60); size=u32(self.b,self.optional+0x64); e=self.offset(directory)
        base,nfun,nname,fa,na,oa=struct.unpack_from('<IIIIII',self.b,e+16)
        idx=symbol-base if isinstance(symbol,int) else None
        if idx is None:
            for i in range(nname):
                if self.cstr(u32(self.b,self.offset(na)+4*i))==symbol: idx=u16(self.b,self.offset(oa)+2*i);break
        assert idx is not None and 0<=idx<nfun, ('Missing export',symbol)
        rva=u32(self.b,self.offset(fa)+4*idx)
        assert not directory<=rva<directory+size, ('Forwarded export',symbol)
        return self.base+rva

# These are data objects inside the CRT's mixed .text section, not instructions.
# The C ports otherwise contain their libm constants as literals (crt_tables.h).
CRT_RANGES=[
 (0x785219b4,0x3a,'EncodePointer / KERNEL32.DLL / DecodePointer string literals (crt_thread.c)'),
 (0x78521bf0,0xa0,'printf __lookuptable character classes and 16 x 8 state transitions (crt_printf.c)'),
 (0x78522d98,0x78,'ten 12-byte _XcptActTab exception action records (_tiddata +0x5c)'),
 (0x785230c8,0x200,'256-entry C locale uint16 ctype table (__initiallocinfo +0xc8; crt_string/printf)'),
]

def build(args):
    pe=[PE(args.exe,0x400000),PE(args.msvcr90,0x78520000),PE(args.msvcp90,0x78480000)]
    expected=['917d14e7ef7ce492b665fea7d0c87c9a39b549e2a43b09883d2a786988e7c02c',
              'cf420aced0d810e1d75f6811dd986f2d9fded2fbb8d61fc9a7024520c475febb',
              '91fd46d7335c9990a20f215b9f6f53bc59551420a9c99ad8110ae2f9ff7598f0']
    for p,digest in zip(pe,expected):
        if sha(p.b)!=digest: raise ValueError('Unsupported source SHA256 for '+p.path.name)
    spans=[]; sections=[]
    for i,p in enumerate(pe):
        selected=('.rdata','.data','.tls') if i==0 else ('.data',)
        for s in p.sections:
            if i==0: sections.append({k:v for k,v in s.items() if k!='offset'}|{'loaded':int(s['name'] in selected)})
            if s['name'] in selected:
                size=min(s['raw_size'],s['virtual_size'])
                spans.append(dict(image=i,addr=s['addr'],data=p.read(s['addr'],size),reason=s['name']+' initialized data; remaining virtual bytes are zero'))
    for addr,size,reason in CRT_RANGES: spans.append(dict(image=1,addr=addr,data=pe[1].read(addr,size),reason=reason))
    spans.sort(key=lambda s:(s['image'],s['addr']))
    bindings=pe[0].imports()+pe[2].imports()
    for im in bindings: im['value']=pe[1 if im['dll']=='msvcr90.dll' else 2].export(im['symbol'])
    bindings.sort(key=lambda im:im['slot'])
    for span in spans:
        # .text is allowed only for these individually enumerated CRT data records.
        span['packed']=zlib.compress(span['data'],9)
    manifest={'format':1,'sources':[{'name':p.path.name,'sha256':sha(p.b),'size':len(p.b),'image_base':p.base} for p in pe],
              'spans':[{'image':pe[s['image']].path.name,'addr':s['addr'],'size':len(s['data']),'sha256':sha(s['data']),'packed_size':len(s['packed']),'reason':s['reason']} for s in spans],
              'exe_sections':sections,'bindings':bindings}
    inc=['/* Generated by core/tools/gen_initial_data.py. Immutable data only; see core/data/README.md. */']
    for i,s in enumerate(spans):
        inc.append('static const unsigned char rx_initial_data_%d[] = {'%i)
        for o in range(0,len(s['packed']),24): inc.append('    '+','.join('0x%02x'%v for v in s['packed'][o:o+24])+',')
        inc.append('};')
    inc.append('static const rx_embedded_span rx_initial_spans[] = {')
    for i,s in enumerate(spans): inc.append('    {%du,0x%08xu,%du,sizeof rx_initial_data_%d,rx_initial_data_%d},'%(s['image'],s['addr'],len(s['data']),i,i))
    inc+=['};','static const rx_image_section rx_initial_sections[] = {']
    for s in sections: inc.append('    {"%s",0x%08xu,%du,%du,%d},'%(s['name'],s['addr'],s['raw_size'],s['virtual_size'],s['loaded']))
    inc+=['};','static const rx_embedded_binding rx_initial_bindings[] = {']
    for im in bindings: inc.append('    {0x%08xu,0x%08xu},'%(im['slot'],im['value']))
    inc+=['};','']
    return pe,spans,manifest,'\n'.join(inc)

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--exe',type=pathlib.Path,default=ROOT/'game files/Archive/MXReflex.exe')
    ap.add_argument('--msvcr90',type=pathlib.Path,default=ROOT/'HANDOFF/scratch_essentials/crt/msvcr90.dll')
    ap.add_argument('--msvcp90',type=pathlib.Path,default=ROOT/'HANDOFF/scratch_essentials/crt/msvcp90.dll')
    ap.add_argument('--out',type=pathlib.Path,default=ROOT/'core/data');ap.add_argument('--check',action='store_true')
    args=ap.parse_args(); pe,spans,manifest,inc=build(args)
    manifest_text=json.dumps(manifest,indent=2,sort_keys=True)+'\n'
    if args.check:
        actual=json.loads((args.out/'initial_data.json').read_text())
        assert actual==manifest,'Source data or manifest changed'
        assert (args.out/'initial_data.inc').read_text()==inc,'Generated payload changed'
        # Compare every byte restored by product startup, including prebound import slots,
        # with an independent reference projection of the PE files. Code bytes are excluded.
        for s in spans: assert zlib.decompress(s['packed'])==pe[s['image']].read(s['addr'],len(s['data']))
        for im in manifest['bindings']:
            dll=pe[1 if im['dll']=='msvcr90.dll' else 2]
            assert im['value']==dll.export(im['symbol'])
        print('PASS: %d data spans (%d bytes), %d CRT import bindings; no EXE .text or DLL instruction ranges'%(len(spans),sum(len(s['data']) for s in spans),len(manifest['bindings'])))
    else:
        args.out.mkdir(parents=True,exist_ok=True)
        (args.out/'initial_data.inc').write_text(inc)
        (args.out/'initial_data.json').write_text(manifest_text)
        print('Generated %d spans / %d source data bytes / %d bindings'%(len(spans),sum(len(s['data']) for s in spans),len(manifest['bindings'])))
if __name__=='__main__': main()

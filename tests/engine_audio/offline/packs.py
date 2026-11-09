# Mirror of EngineSoundPacks.cs parsing (Block parser, sample tables) for the offline renderer.
import os, re, glob
def parse(text):
    root={'v':{},'b':[]}; st=[root]; pending=None
    for raw in text.replace('\r','').split('\n'):
        line=raw
        idx=[i for i in (line.find(';'),line.find('#'),line.find('//')) if i>=0]
        if idx: line=line[:min(idx)]
        line=line.strip()
        if not line: continue
        if line=='{': b={'v':{},'b':[]}; st[-1]['b'].append((pending or '',b)); st.append(b); pending=None; continue
        if line=='}':
            if len(st)>1: st.pop()
            pending=None; continue
        if line.endswith('{'): b={'v':{},'b':[]}; st[-1]['b'].append((line.rstrip('{').strip(),b)); st.append(b); pending=None; continue
        eq=line.find('=')
        if eq>0: st[-1]['v'][line[:eq].strip().lower()]=line[eq+1:].strip().strip('"'); pending=None
        else: pending=line
    return root
def child(b,name):
    for n,c in b['b']:
        if n.lower()==name.lower(): return c
    return None
def F(b,k,d):
    try: return float(b['v'][k.lower()].strip().rstrip('fF'))
    except Exception: return d
class Sample: pass
class Pack: pass
def load(d):
    files=[f for f in glob.glob(os.path.join(d,'**','*'),recursive=True) if os.path.isfile(f)]
    def find(name):
        if not name: return ''
        leaf=name.replace('\\','/').split('/')[-1].strip()
        for f in files:
            if os.path.basename(f).lower()==leaf.lower(): return f
        return ''
    p=Pack(); p.folder=d; p.name=os.path.basename(d.rstrip('/'))
    cf=find('sfx.cfg'); cfg=child(parse(open(cf,encoding='latin1').read()),'engine') if cf else None
    scl=parse(open(find('engine.scl'),encoding='latin1').read())
    p.maxValue=F(scl,'MaxValue',14000); p.autoPitch=F(scl,'AutoPitch',1)!=0
    p.layers=[[],[]]
    for l in range(2):
        L=child(scl,'Layer%d'%l)
        if not L: continue
        for s in range(max(int(F(L,'NumSamples',0)),len(L['b']))):
            b=child(L,'Sample%d'%s)
            if not b: continue
            sm=Sample(); sm.ref=F(b,'RefPitch',1); sm.minp=F(b,'MinPitch',0); sm.maxp=F(b,'MaxPitch',4)
            sm.wav=find(b['v'].get('wav',''))
            sm.val=[];sm.vol=[]
            for q in range(max(int(F(b,'NumPoints',0)),len(b['b']))):
                pt=child(b,'Point%d'%q)
                if pt: sm.val.append(F(pt,'Value',0)); sm.vol.append(F(pt,'Volume',0))
            if sm.wav: p.layers[l].append(sm)
    # pack.ini (ours): gain = w-enginepacks' multiplier of MixGain (now only the fallback when the loudness cannot be measured), trim = a
    # multiplier on the measured loudness (w-packloud)
    p.iniGain = None; p.trim = 1.0
    ini = find('pack.ini')
    if ini:
        b = parse(open(ini, encoding='latin1').read())
        if 'gain' in b['v']: p.iniGain = min(4.0, max(0.0, F(b, 'gain', 1.0)))
        p.trim = min(4.0, max(0.0, F(b, 'trim', 1.0)))
    p.maxRpm=p.maxValue; p.idleThrottle=0; p.blendExp=1.5; p.minDistance=3; p.limiterWav=''; p.limiterVolume=0.8; p.hasCfg=cfg is not None
    if cfg:
        p.maxRpm=F(cfg,'max_rpm',p.maxValue); p.idleThrottle=F(cfg,'idlethrottle',0); p.blendExp=F(cfg,'blend_exp',1.5); p.minDistance=F(cfg,'min_distance',3)
        lim=child(cfg,'limiter')
        if lim: p.limiterWav=find(lim['v'].get('wav','')); p.limiterVolume=F(lim,'volume',0.8)
    if not p.limiterWav:
        c=[f for f in files if f.lower().endswith('.wav') and 'limiter' in os.path.basename(f).lower()]
        p.limiterWav=c[0] if c else ''
    return p
def idle_norm(p):
    for l in (p.layers[1],p.layers[0]):
        if l and l[0].ref>0: return min(max(l[0].ref,0.02),0.6)
    return 0.18
if __name__=='__main__':
    import sys
    for d in sys.argv[1:]:
        p=load(d); M=p.maxValue
        print(f"== {p.name}: MaxValue {M:.0f} max_rpm {p.maxRpm:.0f} cfg={p.hasCfg} blend {p.blendExp} idlethr {p.idleThrottle} lim {os.path.basename(p.limiterWav)} @{p.limiterVolume} idleNorm {idle_norm(p)*M:.0f}")
        for l in range(2):
            for s in p.layers[l]:
                pts=' '.join(f"{v*M:.0f}:{w:.2f}" for v,w in zip(s.val,s.vol))
                print(f"  L{l} {os.path.basename(s.wav):24s} ref {s.ref*M:6.0f} pitch[{s.minp:.3f},{s.maxp:.3f}] -> rpm[{s.minp*s.ref*M:.0f},{s.maxp*s.ref*M:.0f}] pts {pts}")

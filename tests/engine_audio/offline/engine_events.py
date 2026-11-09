# Exports the original engine events the OriginalEngineAudio emitter plays (events.json of _re/formats/audio_export.py) as two small CSVs for
# the engine_logic harness and the mixer: events.csv (path,loop,entry lengths) and states.csv (size,state,path).
import json, sys, wave, os
AUD = os.environ.get('REFLEX_AUDIO_EXPORT', '/Users/payton/Documents/Development/REFLEX/_re/formats/out/audio')
SIZES = ['MX50cc','MX125cc','MX250cc','MX250f','MX450f','ATV450f','UTV660cc','Pro2V8','ProLiteLS7','SuperBuggy16V','Undefined']
def main(out):
    d = json.load(open(os.path.join(AUD, 'events.json')))
    st = d['engine_states']
    with open(os.path.join(out, 'states.csv'), 'w') as f:
        for i, n in enumerate(SIZES):
            e = st.get('MXVehicleEngineSize_' + n)
            if not e: continue
            for state, p in e['states'].items(): f.write('%d,%s,%s/%s\n' % (i, state, e['fev_project'], p))
    paths = set()
    for n in SIZES:
        e = st.get('MXVehicleEngineSize_' + n)
        if e:
            for p in e['states'].values(): paths.add(e['fev_project'] + '/' + p)
    with open(os.path.join(out, 'events.csv'), 'w') as f:
        for p in sorted(paths):
            ev = d['events'].get(p)
            if not ev: continue
            loop = any(si['loop_mode'] == 'loop' for L in ev['layers'] for si in L['sound_instances'])
            lens = []
            for L in ev['layers']:
                for si in L['sound_instances']:
                    sd = ev['sound_defs'].get(si['sound_def']) or {}
                    for en in sd.get('entries') or []:
                        if en.get('file'):
                            w = wave.open(os.path.join(AUD, en['file'])); lens.append(w.getnframes() / w.getframerate())
                    break
                break
            f.write('%s,%d,%s\n' % (p, 1 if loop else 0, ';'.join('%.4f' % x for x in lens) or '1'))
if __name__ == '__main__': main(sys.argv[1])

"""Run the native AI finish test for every packaged MTX venue.
Usage: python3 tools/mtx/test_racing.py <alive_race_test> <game> <packages> <output> [riders] [skill]
Build the driver with core/tools/product_link.sh against the current product objects.
"""
import concurrent.futures,json,os,pathlib,re,subprocess,sys
from catalog import VENUES

def main():
    driver,game,packages,output=map(lambda p:pathlib.Path(p).resolve(),sys.argv[1:5])
    riders=int(sys.argv[5]) if len(sys.argv)>5 else 4
    skill=int(sys.argv[6]) if len(sys.argv)>6 else 95
    if not 2<=riders<=32:raise ValueError('Rider count must include at least one opponent')
    output.mkdir(parents=True,exist_ok=True)
    def run(item):
        cache,(_,name,kind)=item;stem=''.join(c for c in name if c.isalnum())
        env=dict(os.environ,RX_BACKTRACE='1',RX_MAP_SCALE='1.8' if cache=='buddscreek' else '1.6',RX_RACE_MOUNT=str(packages/(stem+'.rxtrack')))
        log=output/(stem+'.log')
        with log.open('w') as f:
            result=subprocess.run([str(driver),str(game),kind,'8' if kind=='Supercross' else '10','NAT_Concord','-1','2','1',str(skill),'54000',str(riders)],env=env,stdout=f,stderr=subprocess.STDOUT,timeout=600)
        text=log.read_text();summary=next((line for line in reversed(text.splitlines()) if line.startswith('RESULT')),text[-500:])
        times=[float(v) for v in re.findall(r' best\s+([0-9.]+) crashes',text)]
        ok=result.returncode==0 and f'{riders-1} / {riders-1} finished' in summary and len(times)==riders-1 and min(times)>20
        record=dict(track=stem,passed=ok,summary=summary);print(json.dumps(record),flush=True);return record
    with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:results=list(pool.map(run,VENUES.items()))
    (output/'results.json').write_text(json.dumps(results,indent=2)+'\n')
    return 0 if all(r['passed'] for r in results) else 1
if __name__=='__main__':sys.exit(main())

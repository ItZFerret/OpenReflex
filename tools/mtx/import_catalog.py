#!/usr/bin/env python3
"""Import the retail nationals/supercross catalog from a local Unleashed cache.
Original PRE archives must already be extracted into --objects-dir. Existing
packages are never overwritten; choose a fresh output directory for rebuilds.
"""
import argparse
from concurrent.futures import ProcessPoolExecutor
from pathlib import Path
from catalog import VENUES, filename
from import_national import convert

def job(args):
    cache,out,objects,level=args
    convert(cache/level,out/filename(level),VENUES[level][1],None,objects)

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('cache',type=Path);p.add_argument('output',type=Path)
    p.add_argument('--objects-dir',type=Path,required=True)
    p.add_argument('--tracks',nargs='+',choices=VENUES,default=list(VENUES))
    p.add_argument('--jobs',type=int,default=2)
    a=p.parse_args()
    if not 1<=a.jobs<=4:p.error('--jobs must be between 1 and 4')
    for level in a.tracks:
        if (a.output/filename(level)).exists():p.error('Output already exists: '+str(a.output/filename(level)))
        for suffix in ('Zpre','scnZpre','colZpre'):
            if not (a.objects_dir/(VENUES[level][0]+suffix)).is_file():p.error('Missing original archive for '+level)
    with ProcessPoolExecutor(max_workers=a.jobs) as pool:
        list(pool.map(job,[(a.cache,a.output,a.objects_dir,k) for k in a.tracks]))

# tests/untamed_convert/foundation/ref_fortdodge.py <out file>
# Writes the md5 of every logical asset of templates/stock-SX_FortDodge, decoded by the Python reference (uxpkg.decode_blob, the rules of
# pkg.decode_blob): the foundation's "stock" test compares the library's SX_FortDodge with it. Read-only on the reference folder.
#   $UX_PYTHON -I -B ref_fortdodge.py <scratch>/ref/fortdodge.json
import sys, os, json, hashlib, struct
sys.dont_write_bytecode = True
ROOT = os.environ['UX_PY_ROOT']
sys.path.insert(0, ROOT + '/tools')
import uxpkg
from concurrent.futures import ProcessPoolExecutor
D = ROOT + '/templates/stock-SX_FortDodge/SX_FortDodge.dx9.'
def one(a):
    off, size, comp = a
    with open(D + 'package', 'rb') as f:
        f.seek(off); b = f.read(size)
    if comp:
        d, n = uxpkg.decode_blob(b); d = d[:n]
    else:
        d = b
    return hashlib.md5(d).hexdigest(), len(d)
if __name__ == '__main__':
    b = uxpkg.Bxml.load(D + 'database')
    recs = []; work = []
    for p in b.findall(b.find(0, 'Packages'), 'Package'):
        for a in b.findall(p, 'Asset'):
            h = b.find(a, 'Heap'); c = b.find(a, 'Compress') is not None
            off = b.attr(p, 'offset') + b.attr(h, 'offset')
            recs.append(dict(name=b.attr(a, 'name'), type=b.attr(a, 'type'), package=b.attr(p, 'name'), offset=off, stored_size=b.attr(h, 'size'), compressed=c))
            work.append((off, b.attr(h, 'size'), c))
    with ProcessPoolExecutor(max_workers=8) as ex:
        for r, (m, n) in zip(recs, ex.map(one, work, chunksize=8)):
            r['md5'] = m; r['size'] = n
    json.dump(dict(level='SX_FortDodge', assets=recs), open(sys.argv[1], 'w'), indent=1)
    print(len(recs), 'assets')

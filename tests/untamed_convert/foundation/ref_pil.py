# tests/untamed_convert/foundation/ref_pil.py <out dir>
# Writes <out>/pil.json: digests of Pillow's Image.resize (BOX / BILINEAR / LANCZOS) and of the reference's rtex.bc_encode (Pillow's DDS
# writer) on images both sides can build (bytes of a 32-bit LCG). The foundation's "readers" test recomputes them with UxPil.
#   $UX_PYTHON -I -B ref_pil.py <scratch>/ref
import sys, os, json, hashlib
sys.dont_write_bytecode = True
ROOT = os.environ['UX_PY_ROOT']
sys.path.insert(0, ROOT + '/tools')
import numpy as np
import PIL
from PIL import Image
import ux_common as U
rtex = U.skill('rtex')


def lcg(n, seed):
    x = seed; o = np.zeros(n, np.uint8)
    for i in range(n):
        x = (x * 1664525 + 1013904223) & 0xFFFFFFFF
        o[i] = x >> 24
    return o


def image(h, w, c, seed, variant):
    a = lcg(h * w * c, seed).reshape(h, w, c).copy()
    if variant == 1: a = (a // 16 * 16).astype(np.uint8)                 # few distinct values: many ties
    if variant == 2 and c == 4: a[..., 3] = np.where(a[..., 3] > 200, 255, np.where(a[..., 3] < 40, 0, a[..., 3]))
    if variant == 3: a[...] = 77                                         # one flat colour
    return a


def pil(a):
    return Image.fromarray(a[..., 0].copy() if a.shape[2] == 1 else a, {1: 'L', 3: 'RGB', 4: 'RGBA'}[a.shape[2]])


md5 = lambda b: hashlib.md5(b).hexdigest()
FILTERS = {'box': Image.BOX, 'bilinear': Image.BILINEAR, 'lanczos': Image.LANCZOS}
res = dict(pillow=PIL.__version__, resize=[], bc=[], chain=[])
seed = 1
for (h, w, oh, ow) in ((64, 64, 32, 32), (64, 48, 16, 24), (50, 70, 128, 128), (256, 256, 1, 1), (17, 33, 40, 9), (2, 3, 4, 4), (512, 512, 256, 256), (300, 200, 64, 64), (8, 8, 8, 4), (1, 1, 4, 4)):
    for c in (1, 3, 4):
        for variant in (0, 2):
            a = image(h, w, c, seed, variant)
            for name, flt in FILTERS.items():
                o = np.asarray(pil(a).resize((ow, oh), flt))
                res['resize'].append(dict(h=h, w=w, c=c, oh=oh, ow=ow, seed=seed, variant=variant, filter=name, md5=md5(np.ascontiguousarray(o).tobytes())))
            seed += 1
for (h, w) in ((16, 16), (32, 20), (13, 9), (64, 64), (8, 8), (128, 128), (5, 7), (4, 4), (1, 1), (2, 3), (3, 8), (256, 64), (1, 16)):
    for c in (3, 4):
        for variant in (0, 1, 2, 3):
            a = image(h, w, c, seed, variant)
            res['bc'].append(dict(h=h, w=w, c=c, seed=seed, variant=variant, dxt1=md5(rtex.bc_encode(pil(a), rtex.DXT1)), dxt5=md5(rtex.bc_encode(pil(a), rtex.DXT5))))
            seed += 1
# a full LANCZOS mip chain, each level encoded (the shape of ux_base.tex_bc / ux_terrain's ground textures)
for (h, w, c, dxt5) in ((256, 256, 3, False), (128, 256, 4, True), (64, 64, 4, False), (512, 512, 3, True)):
    a = image(h, w, c, seed, 0); img = pil(a); n = int(np.log2(max(w, h))) + 1; parts = []
    for i in range(n):
        mw, mh = max(1, w >> i), max(1, h >> i)
        cur = img if i == 0 else img.resize((mw, mh), Image.LANCZOS)
        parts.append(rtex.bc_encode(cur, rtex.DXT5 if dxt5 else rtex.DXT1))
    res['chain'].append(dict(h=h, w=w, c=c, seed=seed, dxt5=dxt5, mips=n, md5=md5(b''.join(parts)), sizes=[len(p) for p in parts]))
    seed += 1
os.makedirs(sys.argv[1], exist_ok=True)
json.dump(res, open(sys.argv[1] + '/pil.json', 'w'))
print('pil.json: %d resize, %d bc, %d chain cases (Pillow %s)' % (len(res['resize']), len(res['bc']), len(res['chain']), PIL.__version__))

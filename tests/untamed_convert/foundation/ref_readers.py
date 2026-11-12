# tests/untamed_convert/foundation/ref_readers.py <out dir> [--levels a,b] [--jobs N] [--stock]
# Writes the digests the foundation's "readers" test compares against, by running the Python reference's shared modules
# (untamed/tools uxmesh, uxmat, uxtex, uxterrain, uxscene, d3d9sh and the skill's rmesh, scene, bxml) on the extracted levels:
#   <out>/readers/<Level>.json   one per level        <out>/readers/common.json   level-independent checks (numpy rules, sorts, EDT)
# Read-only on the reference; run with:  $UX_PYTHON -I -B ref_readers.py <scratch>/ref
import sys, os, json, hashlib, struct, io
sys.dont_write_bytecode = True
ROOT = os.environ['UX_PY_ROOT']
sys.path.insert(0, ROOT + '/tools')
import numpy as np
import ux_common as U
import uxmesh, uxmat, uxtex, uxterrain, uxscene, d3d9sh
rmesh = U.skill('rmesh'); scene = U.skill('scene'); bxml = U.skill('bxml')

STRIP_MAX_TRIS = 1500        # stripify / cluster only the smaller meshes (pure Python)
STRIP_MAX_GROUPS = 120
CLUSTER_MAX_TRIS = 6000


def md5(*parts):
    h = hashlib.md5()
    for p in parts:
        h.update(b'<none>' if p is None else p)
    return h.hexdigest()


def arr(a, dt):
    return None if a is None else np.ascontiguousarray(a, dtype=dt).tobytes()


def val(a):
    """A decoded vertex element: tag + bytes (float32 or uint8)."""
    if a is None: return b'N'
    a = np.asarray(a)
    comps = a.shape[1] if a.ndim == 2 else 1
    if a.dtype == np.uint8: return b'U' + bytes([comps]) + np.ascontiguousarray(a).tobytes()
    return b'F' + bytes([comps]) + np.ascontiguousarray(a, '<f4').tobytes()


def err(e):
    return 'ERR:' + type(e).__name__


def jdump(o):
    return md5(U.dumps_json(o).encode('utf-8'))


def jsonable(o):
    if isinstance(o, dict): return {str(k): jsonable(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)): return [jsonable(v) for v in o]
    if isinstance(o, (bytes, bytearray)): return bytes(o).hex()
    if isinstance(o, np.generic): return o.item()
    return o


# ------------------------------------------------------------------------------------------------ surfaces
def surface_digest(data, budget):
    out = []
    for g in uxmesh.read_surface(data):
        d = dict(nverts=g.nverts, prim=g.prim, ntris=len(g.tris))
        d['head'] = md5(struct.pack('<5I', g.flags, g.prim, g.nverts, g.npal, len(g.decl)), b''.join(struct.pack('<6I', *x) for x in g.decl),
                        arr(g.strides, '<i4'), bytes(g.palette), arr(g.strip, '<i4'), arr(g.tris, '<i4'))
        d['elems'] = md5(val(g.pos), val(g.pos_w), val(g.nrm), val(g.uv0), val(g.uv1), val(g.col), val(g.tan), val(g.bidx), val(g.bwgt))
        with np.errstate(all='ignore'):
            try: d['rv'] = md5(uxmesh.reflex_vertices(g).tobytes())
            except Exception as e: d['rv'] = err(e)
            if g.nverts and g.pos is not None:
                org = ((g.pos.min(0) + g.pos.max(0)) / 2).astype(np.float64)
                try: d['rvo'] = md5(uxmesh.reflex_vertices(g, org).tobytes())
                except Exception as e: d['rvo'] = err(e)
            if g.nrm is not None and g.uv0 is not None and g.pos is not None:
                try: d['crowd'] = md5(uxmesh.reflex_crowd_vertices(g).tobytes())
                except Exception as e: d['crowd'] = err(e)
        if 0 < len(g.tris) <= STRIP_MAX_TRIS and budget[0] > 0:
            budget[0] -= 1
            s = rmesh.stripify(g.tris)
            d['strip2'] = md5(arr(s, '<i4')); d['canon'] = md5(arr(rmesh.canon(g.tris), '<i4'))
            d['strip2_ok'] = bool((rmesh.canon(rmesh.strip_to_tris(s)) == rmesh.canon(g.tris)).all())
        out.append(d)
    return out


def model_digest(data, surf):
    m = uxmesh.read_model(data)
    d = dict(skinned=bool(m.skinned), boned=bool(m.boned), name=m.name, ngroups=m.ngroups, hdr=list(m.hdr))
    if not m.skinned:
        d.update(draw_distance=m.draw_distance, name2=m.name2, lods=jsonable(m.lods), recs=jsonable(m.recs), flags=bytes(m.flags).hex(), order=list(m.order),
                 materials=m.materials, tail=len(m.tail), num=md5(arr(m.matrix, '<f4'), struct.pack('<f', m.radius), arr(m.center, '<f4'), arr(m.extents, '<f4')))
        try: d['m2r'] = md5(uxmesh.model_to_reflex(data))
        except Exception as e: d['m2r'] = err(e)
    else:
        d['nbones'] = m.nbones
    if m.boned:
        try:
            r = uxmesh.read_model_rigid(data)
            d['rigid'] = dict(names=r.node_names, words=r.node_words, name2=r.name2, materials=r.materials, lods=jsonable(r.lods), recs=jsonable(r.recs), order=list(r.order),
                              num=md5(b''.join(arr(x, '<f4') for x in r.node_matrices), struct.pack('<f', r.radius), arr(r.center, '<f4'), arr(r.extents, '<f4')))
            if surf is not None:
                d['rigid']['problems'] = uxmesh.rigid_static_problems(r, uxmesh.read_surface(surf))
        except Exception as e:
            d['rigid'] = err(e)
    return d


def collis_digest(data):
    try:
        c = uxmesh.read_collis(data)
    except Exception as e:
        return dict(read=err(e))
    k = c['kind']
    d = dict(kind=k, version=c['version'], size=c['size'], tail=c.get('tail'))
    if k == 0:
        d['num'] = md5(arr(c['matrix'], '<f8'), arr(c['verts'], '<f8'), arr(c['tris'], '<f8'), arr(c['local_tris'], '<f8'), c['nodes'].tobytes(), c['tri'].tobytes(),
                       struct.pack('<3I', c['root'], *c['z']))
    elif k == 2:
        d['num'] = md5(arr(c['matrix'], '<f8'), arr(c['points'], '<f8'), arr(c['point_ref'], '<f8'), c['nodes'].tobytes(), struct.pack('<I', c['nleaf']))
        d['swap'] = md5(uxmesh.swap_collis_kind2(data))
    elif k == 11:
        d['num'] = md5(arr(c['half'], '<f8'), bytes(c['pad']))
    elif k == 15:
        d['tags'] = bytes(c['tags']).hex()
        d['prims'] = [[p['type'], p['m1'], md5(arr(p['matrix'], '<f8'), arr(p['params'], '<f8'), bytes(p['raw']))] for p in c['prims']]
    elif k == 17:
        d['num'] = md5(struct.pack('<2d', c['radius'], c['half_height']))
    elif k == 19:
        d['count'] = c['count']
    try: d['prim'] = md5(uxmesh.collis_to_reflex_primitive(data))
    except Exception as e: d['prim'] = err(e)
    try:
        t = uxmesh.collis_triangles(data)
        d['tris'] = None if t is None else md5(arr(t, '<f8'))
        if t is not None and 0 < len(t) <= CLUSTER_MAX_TRIS:
            cl = rmesh.cluster_triangles(t)
            d['clusters'] = md5(rmesh.write_collis(cl)); d['nclusters'] = len(cl)
            d['bounds'] = md5(struct.pack('<d', rmesh.bounds(np.asarray(t, np.float64).reshape(-1, 3))[0]),
                              arr(rmesh.bounds(np.asarray(t, np.float64).reshape(-1, 3))[1], '<f8'), arr(rmesh.bounds(np.asarray(t, np.float64).reshape(-1, 3))[2], '<f8'))
    except Exception as e:
        d['tris'] = err(e)
    return d


# ------------------------------------------------------------------------------------------------ materials
def material_digest(data):
    try:
        m = uxmat.read_material(bytes(data))
    except Exception as e:
        return dict(read=err(e))
    out = dict(version=m['version'], passes=[])
    for P in m['passes']:
        d = dict(vs=P['vs'], vs_entry=P['vs_entry'], ps=P['ps'], ps_entry=P['ps_entry'], textures=jsonable(P['textures']), states=jsonable(P['states']),
                 params=None if P['params'] is None else [[n, jsonable(a), jsonable(v)] for n, a, v in P['params']])
        try: d['pc'] = jsonable(uxmat.xenon_states_to_pc(P['states']))
        except Exception as e: d['pc'] = err(e)
        try: d['spc'] = [jsonable(uxmat.xenon_sampler_to_pc(ss)) for _, ss in P['textures']]
        except Exception as e: d['spc'] = err(e)
        d['named'] = jsonable(uxmat.named_states(P['states']))
        d['blend'] = uxmat.blend_class(P['states'])
        tex = [t for t, _ in P['textures']]
        d['roles'] = [[k, v] for k, v in uxmat.guess_roles(tex).items()]
        d['role_each'] = [uxmat.texture_role(t) for t in tex]
        out['passes'].append(d)
    if m['passes']:
        out['suggest'] = [jsonable(uxmat.suggest_reflex_template(m)), jsonable(uxmat.suggest_reflex_template(m, uses_uv1=False)),
                          jsonable(uxmat.suggest_reflex_template(m, vertex_coloured=True))]
    return out


# ------------------------------------------------------------------------------------------------ textures
def texture_digest(data):
    try:
        t = uxtex.UxTexture(data)
    except Exception as e:
        return dict(read=err(e))
    s0 = t.surfaces[0]
    d = dict(info=t.info(), check=t.check_size() or None, name=s0['name'], gpufmt=s0['gpufmt'])
    d['layout'] = md5(b''.join(struct.pack('<10q', s, f, m, *loc) for (s, f, m), loc in t.level_loc.items()), struct.pack('<q', t.data_end))
    isfloat = t._elem(0)[0] in ('16_16_16_16_EXPAND', '16_16_16_16_FLOAT')
    d['float'] = isfloat
    try:
        pc = t.to_pc()
        d['pc'] = md5(pc); d['pc_size'] = len(pc)
        p = uxtex.PcTexture(pc)
        d['pcread'] = dict(nsurf=p.nsurf, nentry=p.nentry, kind=p.kind, flags=p.flags, end=p.end, data_offset=p.data_offset,
                           recs=[[a, b, c, bytes(f).hex()] for a, b, c, f in p.recs], table=jsonable(p.table))
    except Exception as e:
        d['pc'] = err(e)
    try: d['pc2'] = md5(t.to_pc(max_mips=2))
    except Exception as e: d['pc2'] = err(e)
    try:
        im = t.rgba(0, 0, 0)
        d['rgba'] = md5(im.tobytes()); d['rgba_shape'] = list(im.shape)
        lm = max(1, s0['mips']) - 1
        d['rgba_last'] = md5(t.rgba(0, t.faces - 1, lm).tobytes())
        d['rgba_raw'] = md5(t.rgba(0, 0, 0, nm_preview=False).tobytes())
    except Exception as e:
        d['rgba'] = err(e)
    if isfloat:
        d['floatlevel'] = md5(arr(t.float_level(0, 0, 0), '<f4'))
    return d


# ------------------------------------------------------------------------------------------------ terrain
def terrain_digest(tdata, cdata):
    d = {}
    T = C = None
    if tdata is not None:
        T = uxterrain.read_terrain(tdata)
        d['terrain'] = dict(version=T.version, nmat=T.nmat, nzx=T.nzx, nzz=T.nzz, header_size=T.header_size, size=T.size,
                            head=md5(arr(T.centre, '<f8'), arr(T.half, '<f8'), arr(T.w30, '<u4'), bytes(T.decl_raw), arr(T.zone_centre, '<f8'), arr(T.zone_half, '<f8'), arr(T.ranges, '<u4')),
                            mats=[md5(arr(m.indices, '<u4'), arr(m.pos, '<f8'), arr(m.normal, '<f8'), arr(m.uv0, '<f4'), arr(m.uv1, '<f4'), arr(m.color, '<u4'), arr(m.tangent, '<f8'),
                                      arr(m.tris, '<i4'), arr(m.zone, '<i4')) for m in T.materials],
                            pc=md5(uxterrain.terrain_to_pc(tdata)))
        pc = uxterrain.read_terrain(uxterrain.terrain_to_pc(tdata))
        d['terrain']['pc_back'] = md5(*[arr(m.tris, '<i4') for m in pc.materials], *[arr(m.normal, '<f8') for m in pc.materials])
    if cdata is not None:
        C = uxterrain.read_trncol(cdata)
        d['trncol'] = dict(version=C.version, grid=list(C.grid), cell=C.cell, bmin=C.bmin.tolist(), bmax=C.bmax.tolist(), ncells=len(C.cells),
                           num=md5(arr(C.verts, '<f8'), arr(C.vw, '<f4'), arr(C.vnormals, '<f8'), arr(C.vattr, '<u4'), arr(C.planes, '<f8'), arr(C.tris, '<i4'), arr(C.flags, '<u4'), arr(C.lists, '<u4')),
                           swap=md5(uxterrain.trncol_swap(cdata)))
    if T is not None and C is not None:
        cm = uxterrain.collision_materials(T, C)
        d['cm'] = md5(arr(cm, '<i4')); d['cm_unmatched'] = int((cm < 0).sum())
        hf = uxterrain.heightfield(C, size=1024)
        R = hf['raster']
        d['hf'] = dict(height=md5(arr(hf['height'], '<f4')), covered=md5(arr(hf['covered'], 'u1')), ncovered=int(hf['covered'].sum()), x0=hf['x0'], z1=hf['z1'],
                       raster=md5(arr(R['top'], '<f4'), arr(R['up_top'], '<f4'), arr(R['bottom'], '<f4'), arr(R['down_max'], '<f4'), arr(R['down_min'], '<f4'), arr(R['nup'], '<i4'),
                                  arr(R['ny'], '<f4'), arr(R['tri'], '<i8'), arr(R['tri_bottom'], '<i8')))
        # an excluded material (the last one) and a coarser, off-centre grid without fill
        ex = uxterrain.heightfield(C, cm, exclude=(T.nmat - 1, -1), size=256, res=2.0, centre=(10.0, -20.0), y_offset=10.0, fill=False)
        d['hf_ex'] = dict(height=md5(arr(ex['height'], '<f4')), covered=md5(arr(ex['covered'], 'u1')), excluded=md5(arr(ex['excluded'], '<i4')), n=len(ex['excluded']))
        # the soup's own grid (origin None)
        R2 = uxterrain.rasterize(C.verts, C.tris[::7], res=4.0)
        d['raster_own'] = dict(shape=list(R2['top'].shape), x0=R2['x0'], z1=R2['z1'], num=md5(arr(R2['top'], '<f4'), arr(R2['nup'], '<i4'), arr(R2['tri'], '<i8')))
    return d


# ------------------------------------------------------------------------------------------------ scene
def scene_digest(path, level):
    subs = uxscene.load(path)
    d = dict(subs=[[s.name, len(s.chunks), s.nent, s.offset, len(s.entities())] for s in subs])
    out = {}
    for s in subs:
        out[s.name] = [uxscene._jsonable(e.to_dict()) for e in s.entities()]
    d['entities'] = md5(json.dumps(out, indent=1).encode('utf-8'))
    d['per_sub'] = {s.name: md5(json.dumps(out[s.name], indent=1).encode('utf-8')) for s in subs}
    me = uxscene.merged_entities(subs, level)
    d['merged'] = md5(json.dumps([[k, v[0], v[1].type, v[1].name] for k, v in me.items()]).encode('utf-8')); d['nmerged'] = len(me)
    d['inventory'] = {s.name: [[k, v] for k, v in uxscene.inventory(s).items()] for s in subs}
    d['assets'] = md5(json.dumps([[list(a) for a in e.assets()] for s in subs for e in s.entities()]).encode('utf-8'))
    d['xml'] = md5(*[b.to_xml().encode('utf-8') for s in subs for b in s.chunks])
    d['position'] = md5(json.dumps([e.position for s in subs for e in s.entities()]).encode('utf-8'))
    return d


def level_digests(args):
    disc, out = args
    lv = U.level(disc)
    root = U.extract_dir(lv)
    idx = U.read_json(root / 'index.json')
    res = dict(level=disc, surface={}, model={}, collis={}, material={}, texture={}, terrain={})
    files = {}
    for a in idx['assets']:
        files.setdefault(a['type'], []).append(a['file'].replace('\\', '/'))
    budget = [STRIP_MAX_GROUPS]
    for f in files.get('surface', []):
        try: res['surface'][f] = surface_digest((root / f).read_bytes(), budget)
        except Exception as e: res['surface'][f] = err(e)
    surf_by_name = {f.split('/')[-1].rsplit('.', 1)[0].lower(): f for f in files.get('surface', [])}
    for f in files.get('model', []):
        sf = surf_by_name.get(f.split('/')[-1].rsplit('.', 1)[0].lower())
        try: res['model'][f] = model_digest((root / f).read_bytes(), (root / sf).read_bytes() if sf else None)
        except Exception as e: res['model'][f] = err(e)
    for f in files.get('collis', []):
        res['collis'][f] = collis_digest((root / f).read_bytes())
    for f in files.get('material', []):
        res['material'][f] = jdump(material_digest((root / f).read_bytes()))
    for f in files.get('texture', []):
        res['texture'][f] = texture_digest((root / f).read_bytes())
    tf = files.get('terrain', []); cf = files.get('trncol', [])
    res['terrain_files'] = [tf, cf]
    res['terrain'] = terrain_digest((root / tf[0]).read_bytes() if tf else None, (root / cf[0]).read_bytes() if cf else None)
    res['scene'] = scene_digest(str(U.disc_files(lv)['scene']), disc)
    os.makedirs(out + '/readers', exist_ok=True)
    with open(out + '/readers/%s.json' % disc, 'w') as fh:
        json.dump(res, fh)
    return disc, {k: len(v) for k, v in res.items() if isinstance(v, dict)}


# ------------------------------------------------------------------------------------------------ level-independent
def lcg(n, seed):
    x = seed; o = np.zeros(n, np.uint32)
    for i in range(n):
        x = (x * 1664525 + 1013904223) & 0xFFFFFFFF
        o[i] = x
    return o


def dvals(n, seed):
    """Doubles both sides build exactly: ((u >> 5) - 2**26) / 1024."""
    return ((lcg(n, seed) >> 5).astype(np.float64) - 2.0 ** 26) / 1024.0


def common(out):
    res = {}
    # ---- BLAS-backed numpy operations
    blas = []
    seed = 100
    for (m, k, n) in ((3, 3, 3), (4, 4, 4), (50, 3, 3), (8, 3, 3), (1, 3, 3), (7, 5, 6), (2, 2, 2), (3, 4, 4), (12, 4, 4)):
        A = dvals(m * k, seed).reshape(m, k); B = (dvals(k * n, seed + 1) / 4096.0).reshape(k, n); seed += 2
        blas.append(dict(op='matmul', m=m, k=k, n=n, seed=seed - 2, r=(A @ B).ravel().tolist()))
        Af = A.astype(np.float32); Bf = B.astype(np.float32)
        blas.append(dict(op='matmul32', m=m, k=k, n=n, seed=seed - 2, r=[float(x) for x in (Af @ Bf).ravel()]))
    for n in range(1, 16):
        a = dvals(n, seed); b = dvals(n, seed + 1) / 4096.0; seed += 2
        blas.append(dict(op='dot', n=n, seed=seed - 2, r=float(np.dot(a, b)), norm=float(np.linalg.norm(a))))
    for m in (1, 2, 3, 9):
        A = dvals(m * 3, seed).reshape(m, 3); x = dvals(3, seed + 1) / 4096.0; seed += 2
        blas.append(dict(op='matvec3', m=m, seed=seed - 2, r=(A @ x).tolist()))
    for n in (3, 4):
        for rep in range(6):
            y = dvals(n, seed); A = (dvals(n * n, seed + 1) / 4096.0).reshape(n, n); seed += 2
            blas.append(dict(op='vecmat', n=n, seed=seed - 2, r=(y @ A).tolist()))
    res['blas'] = blas
    # ---- np.sum / np.mean (pairwise summation)
    sums = []
    for n in (1, 5, 7, 8, 9, 15, 16, 64, 127, 128, 129, 200, 257, 1000, 4097, 100000):
        d = dvals(n, seed) / 3.0; f = d.astype(np.float32); seed += 1
        m2 = (dvals(40 * 9, seed) / 7.0).reshape(40, 9); seed += 1
        sums.append(dict(n=n, seed=seed - 2, sum=float(d.sum()), mean=float(d.mean()), sum32=float(f.sum()), mean32=float(f.mean()),
                         rows=m2.sum(axis=1).tolist(), cols=m2.sum(axis=0).tolist(), strided=float(d[::3].sum())))
    res['sums'] = sums
    # ---- argsort of a structured array (numpy's generic introsort)
    sorts = []
    for n, mod, seed2 in ((5, 3, 1), (16, 4, 2), (17, 4, 3), (18, 2, 4), (100, 7, 5), (1000, 13, 6), (5000, 50, 7), (40000, 300, 8), (3000, 1, 9), (2000, 2, 10), (70000, 100000, 11)):
        u = lcg(3 * n, seed2).reshape(n, 3) % mod
        k = np.zeros(n, [('a', np.int64), ('b', np.int64), ('c', np.int64)])
        k['a'] = u[:, 0] % 3; k['b'] = u[:, 1] % 2; k['c'] = u[:, 2]
        o = np.argsort(k)
        sorts.append(dict(n=n, mod=mod, seed=seed2, md5=md5(arr(o, '<i4'))))
    for n in (64, 1000, 20000):       # sorted, reversed
        for kind in ('asc', 'desc', 'organ'):
            v = np.arange(n) if kind == 'asc' else np.arange(n)[::-1] if kind == 'desc' else np.concatenate([np.arange(n // 2), np.arange(n - n // 2)[::-1]])
            k = np.zeros(n, [('a', np.int64), ('b', np.int64), ('c', np.int64)]); k['a'] = v // 3
            sorts.append(dict(n=n, kind=kind, md5=md5(arr(np.argsort(k), '<i4'))))
    res['sorts'] = sorts
    # ---- scipy.ndimage
    from scipy import ndimage
    nd = []
    for h, w, thr, seed3 in ((17, 23, 200, 1), (64, 64, 250, 2), (64, 64, 128, 3), (150, 200, 254, 4), (40, 30, 10, 5), (9, 9, 256, 6), (9, 9, -1, 7), (1, 50, 240, 8), (50, 1, 240, 9), (300, 300, 255, 10)):
        mask = ((lcg(h * w, seed3) >> 24) < thr).reshape(h, w)       # True = to be filled; False = feature
        if thr == 256: mask[4, 4] = False
        dist, ind = ndimage.distance_transform_edt(mask, return_distances=True, return_indices=True)
        nd.append(dict(h=h, w=w, thr=thr, seed=seed3, ind=md5(arr(ind[0], '<i4'), arr(ind[1], '<i4')), dist=md5(arr(dist, '<f8')),
                       dil1=md5(arr(ndimage.binary_dilation(~mask, np.ones((3, 3), bool), iterations=1), 'u1')),
                       dil3=md5(arr(ndimage.binary_dilation(~mask, np.ones((3, 3), bool), iterations=3), 'u1')),
                       dilc=md5(arr(ndimage.binary_dilation(~mask, iterations=2), 'u1'))))
    res['ndimage'] = nd
    # ---- float16 conversion of float64 / float32 (astype('<f2'))
    d = dvals(20000, 77) / 97.0
    x = np.concatenate([d, d * 1e-6, d * 300.0, np.array([0.0, -0.0, 65519.99, 65520.0, 1e-8, 5.960464477539063e-08, 2.9802322387695312e-08, 2.98023223876953125e-08 * 1.0000001, np.inf, -np.inf, np.nan])])
    with np.errstate(all='ignore'):
        res['half'] = dict(f64=md5(x.astype('<f2').tobytes()), f32=md5(x.astype(np.float32).astype('<f2').tobytes()),
                           round=md5(np.round(d * 3.7).astype('<i8').tobytes()), round2=md5(arr(np.round(d, 2), '<f8')))
    res['pyround'] = [[repr(v), round(v, 6), round(v, 2), round(v)] for v in (d[:400] / 3.0).tolist()]
    # ---- shaders (reflex-lib) through d3d9sh
    sh = {}
    for p in sorted((U.LIB_SHADERS).glob('*.shader')):
        sh[p.name] = jdump([[e['entry'], e['kind'], jsonable(e['ctab']), jsonable(e['decls'])] for e in d3d9sh.summarize(p.read_bytes())])
    res['shaders'] = sh
    # ---- PC scene and BXML tree codec on the stock base (templates/stock-SX_FortDodge) and prototypes
    fd = ROOT + '/templates/stock-SX_FortDodge/SX_FortDodge.dx9.'
    subs = scene.load(fd + 'scene')
    import tempfile
    with tempfile.TemporaryDirectory() as td:
        scene.save(td + '/s.scene', subs)
        saved = open(td + '/s.scene', 'rb').read()
    res['pcscene'] = dict(subs=[[s.name, len(s.chunks), s.nent, len(list(s.entities()))] for s in subs], save=md5(saved), same_as_file=saved == open(fd + 'scene', 'rb').read(),
                          ns=md5(json.dumps([[scene.ns(e), (scene.prop(e, 'Name').children[0].value if scene.prop(e, 'Name') is not None and scene.prop(e, 'Name').children else None)]
                                             for s in subs for e in s.entities()]).encode()),
                          matrices=md5(json.dumps([scene.get_matrix(e) for s in subs for e in s.entities() if scene.prop(e, 'Matrix') is not None]).encode()))
    lvb = bxml.Bxml.load(fd + 'level'); dbb = bxml.Bxml.load(fd + 'database')
    res['bxml'] = dict(level_xml=md5(lvb.to_xml().encode()), level_tree=md5(bxml.encode_tree(bxml.to_tree(lvb), lvb.version, lvb.unknown)[0]),
                       db_tree=md5(bxml.encode_tree(bxml.to_tree(dbb), dbb.version, dbb.unknown)[0]), db_bytes=md5(dbb.to_bytes()), db_xml=md5(dbb.to_xml().encode()))
    # canon_namespaces on the base scene (the DE spelling test): lower-case every namespace, canonise, compare
    import copy
    ents = [copy.deepcopy(e) for s in subs[:1] for e in s.entities()]
    for e in ents:
        for x in e.iter():
            for a in x.attrs:
                if a[0] == 'namespace' and a[1] == 1: a[2] = a[2].lower()
    cnt = U.canon_namespaces(ents)
    res['canon'] = dict(changes=[[k[0], k[1], v] for k, v in cnt.items()], after=md5(json.dumps([[a[2] for x in e.iter() for a in x.attrs if a[0] == 'namespace'] for e in ents]).encode()))
    with open(out + '/readers/common.json', 'w') as fh:
        json.dump(res, fh)
    return res


if __name__ == '__main__':
    out = sys.argv[1]
    levels = [lv.disc for lv in U.LEVELS]
    jobs = 6
    for i, a in enumerate(sys.argv):
        if a == '--levels': levels = [U.level(x).disc for x in sys.argv[i + 1].split(',')]
        if a == '--jobs': jobs = int(sys.argv[i + 1])
    os.makedirs(out + '/readers', exist_ok=True)
    if '--no-common' not in sys.argv:
        common(out); print('common done', flush=True)
    if '--no-levels' not in sys.argv:
        from concurrent.futures import ProcessPoolExecutor
        with ProcessPoolExecutor(max_workers=jobs) as ex:
            for disc, n in ex.map(level_digests, [(d, out) for d in levels]):
                print(disc, n, flush=True)

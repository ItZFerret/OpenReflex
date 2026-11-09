"""blender_rxmod.py -- exports a bike from Blender into an Open Reflex .rxmesh (core/plan/RXMOD.md "Mesh").

Run it inside Blender (headless is fine):

  blender -b <bike.blend> --python tools/rxmod/blender_rxmod.py -- --recipe <recipe.json> --out <mod folder>/<bike>.rxmesh
          [--report <report.json>] [--preview <side.png>]

The recipe says which Blender objects are which part of the bike (the node of the original MX_Hierarchy that moves them):

  {
    "forward": "auto",                  the bike's nose in Blender: "+X", "-X", "+Y", "-Y" or "auto" (the end the handlebars are at)
    "unit": 1.0,                        metres per Blender unit (default: the scene's unit scale)
    "include": [".*"], "exclude": [],   object name patterns (regular expressions, case-insensitive); hidden / non-render objects are out
    "nodes": [                          first match wins: "obj:<pattern>" an object name, "mat:<pattern>" a material name
      ["obj:front.*(tire|tyre|rim|wheel|spoke|hub|disc)", "OBJ_FRT_Tire"],
      ["obj:swingarm|chain", "OBJ_Swingarm"], ...
    ],
    "default": "OBJ_Frame",             the node of everything no rule names
    "split": false,                     true: each object's loose parts are matched on their own (by "where" rules below)
    "where": [                          position rules for what "nodes" leaves to the default (bike space, metres: z forward from the
                                        rear axle, y up from the rear axle, x right):
      {"node": "OBJ_FRT_Tire", "in_wheel": "front", "max_x": 0.09},   a part entirely inside the front wheel's circle, within 9 cm of the centre plane
      {"node": "OBJ_UPR_Forks", "box": [[xmin,ymin,zmin],[xmax,ymax,zmax]]}
    ],
    "skin": [{"match": "obj:shock", "top": "OBJ_Frame", "bottom": "OBJ_Swingarm"}],   weighted along the part's long axis
    "materials": [["mat:tire|tyre", "tires"], [".*", "bike"]],   Blender material (or object) -> the mod's material name in mod.json
    "wheels": {"front": "obj:...", "rear": "obj:..."}            the parts whose centres are the axles (default: the OBJ_*_Tire parts')
    "axles": {"front": [x, y, z], "rear": [x, y, z]},             or the axles themselves (Blender coordinates): a bike without wheels
                                                                 (the game then draws the original bike's tyres and rims on them)
    "offsets": {"<object>": [dx, dy, dz]},                        moves an object (a variant laid out beside the assembled bike) into place
    "steer": [[x, y, z], [x, y, z]],                              optional: two points on the steering axis (top, bottom), Blender coordinates
    "pivot": [x, y, z]                                            optional: the swingarm pivot, Blender coordinates
  }

The mesh is written in the game's space: feet, X right, Y up, Z forward (Blender's right-handed Z-up becomes left-handed Y-up: the
triangles are reversed so their front faces stay outside), UVs as Blender has them (the game's reader flips them for D3D).
"""
import bpy, bmesh, json, math, os, re, struct, sys
from mathutils import Matrix, Vector

FEET = 3.280839895
NODES = ["OBJ_Frame", "OBJ_UPR_Forks", "OBJ_LWR_Forks", "OBJ_FRT_Tire", "OBJ_Swingarm", "OBJ_BCK_Tire", "OBJ_Throttle", "OBJ_ClutchLVR", "OBJ_BrakeLVR"]
PARENT = {"OBJ_Frame": None, "OBJ_UPR_Forks": "OBJ_Frame", "OBJ_LWR_Forks": "OBJ_UPR_Forks", "OBJ_FRT_Tire": "OBJ_LWR_Forks", "OBJ_Swingarm": "OBJ_Frame",
          "OBJ_BCK_Tire": "OBJ_Swingarm", "OBJ_Throttle": "OBJ_UPR_Forks", "OBJ_ClutchLVR": "OBJ_UPR_Forks", "OBJ_BrakeLVR": "OBJ_UPR_Forks"}
COLORS = {"OBJ_Frame": (0.6, 0.6, 0.6), "OBJ_UPR_Forks": (0.1, 0.4, 1.0), "OBJ_LWR_Forks": (0.0, 0.9, 0.9), "OBJ_FRT_Tire": (1.0, 0.8, 0.0),
          "OBJ_Swingarm": (0.9, 0.1, 0.9), "OBJ_BCK_Tire": (1.0, 0.4, 0.0), "OBJ_Throttle": (0.0, 1.0, 0.2), "OBJ_ClutchLVR": (0.5, 1.0, 0.5),
          "OBJ_BrakeLVR": (0.2, 0.6, 0.2), "SKIN": (1.0, 0.1, 0.1)}


def args():
    a = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    o = {"recipe": None, "out": None, "report": None, "preview": None, "texture": None, "view": "side"}
    for i in range(0, len(a) - 1, 2): o[a[i].lstrip("-")] = a[i + 1]
    return o


def matcher(rule):
    kind, _, pat = rule.partition(":") if rule[:4] in ("obj:", "mat:") else ("any", "", rule)
    rx = re.compile(pat, re.I)
    return lambda obj, mat: bool((kind in ("obj", "any") and rx.search(obj)) or (kind in ("mat", "any") and mat and rx.search(mat)))


def main():
    o = args()
    recipe = json.load(open(o["recipe"])) if o["recipe"] else {}
    unit = recipe.get("unit") or bpy.context.scene.unit_settings.scale_length or 1.0
    inc = [re.compile(p, re.I) for p in recipe.get("include", [".*"])]
    exc = [re.compile(p, re.I) for p in recipe.get("exclude", [])]
    rules = [(matcher(r), n) for r, n in recipe.get("nodes", [])]
    mats = [(matcher(r), n) for r, n in recipe.get("materials", [])]
    skins = [(matcher(s["match"]), s) for s in recipe.get("skin", [])]
    default = recipe.get("default", "OBJ_Frame")

    deps = bpy.context.evaluated_depsgraph_get()
    vl = bpy.context.view_layer
    # every visible, rendered mesh, evaluated (modifiers applied), triangulated, in world space (metres)
    parts = []   # dict(obj, mat, node, skin, verts[(co, normal, uv, uv1)], tris)
    for ob in bpy.context.scene.objects:
        if ob.type not in ("MESH", "CURVE", "SURFACE", "META", "FONT"): continue
        if ob.hide_render or not ob.visible_get(view_layer=vl): continue
        if not any(r.search(ob.name) for r in inc) or any(r.search(ob.name) for r in exc): continue
        ev = ob.evaluated_get(deps)
        try: me = ev.to_mesh()
        except RuntimeError: continue
        if me is None or len(me.polygons) == 0: ev.to_mesh_clear(); continue
        me.calc_loop_triangles()
        try: me.calc_normals_split()
        except AttributeError: pass   # (Blender 4.1+: corner normals are always there)
        uv = me.uv_layers.active.data if me.uv_layers.active else None
        uv1 = None
        if len(me.uv_layers) > 1:
            other = [l for l in me.uv_layers if l != me.uv_layers.active]
            uv1 = other[0].data if other else None
        W = ob.matrix_world.copy(); N = W.to_3x3().inverted_safe().transposed()
        off = recipe.get("offsets", {}).get(ob.name)
        if off: W = Matrix.Translation(Vector(off)) @ W
        flip = W.determinant() < 0
        by_mat = {}
        for t in me.loop_triangles:
            by_mat.setdefault(t.material_index, []).append(t)
        cn = me.corner_normals if hasattr(me, "corner_normals") else None
        for mi, tris in by_mat.items():
            mat = ob.material_slots[mi].material.name if mi < len(ob.material_slots) and ob.material_slots[mi].material else ""
            verts, index, out_tris = [], {}, []
            for t in tris:
                tri = []
                for k in range(3):
                    li = t.loops[k]; vi = t.vertices[k]
                    co = W @ me.vertices[vi].co
                    n = (N @ (cn[li].vector if cn is not None else me.loops[li].normal)).normalized()
                    u = tuple(uv[li].uv) if uv else (0.0, 0.0); u1 = tuple(uv1[li].uv) if uv1 else (0.0, 0.0)
                    key = (vi, round(n.x, 4), round(n.y, 4), round(n.z, 4), round(u[0], 6), round(u[1], 6), round(u1[0], 6), round(u1[1], 6))
                    j = index.get(key)
                    if j is None: j = index[key] = len(verts); verts.append((co.copy(), n.copy(), u, u1))
                    tri.append(j)
                out_tris.append(tri if not flip else [tri[0], tri[2], tri[1]])
            parts.append(dict(obj=ob.name, mat=mat, verts=verts, tris=out_tris))
        ev.to_mesh_clear()
    if not parts: raise SystemExit("blender_rxmod: no visible mesh in the file")

    # split into loose parts (connected triangles) when the recipe asks
    if recipe.get("split"):
        out = []
        for p in parts:
            parent = list(range(len(p["verts"])))
            def find(a):
                while parent[a] != a: parent[a] = parent[parent[a]]; a = parent[a]
                return a
            for a, b, c in p["tris"]:
                for x, y in ((a, b), (b, c)):
                    rx, ry = find(x), find(y)
                    if rx != ry: parent[rx] = ry
            groups = {}
            for t in p["tris"]: groups.setdefault(find(t[0]), []).append(t)
            for g, tris in groups.items():
                remap = {}; verts = []
                for t in tris:
                    for v in t:
                        if v not in remap: remap[v] = len(verts); verts.append(p["verts"][v])
                out.append(dict(obj=p["obj"], mat=p["mat"], verts=verts, tris=[[remap[v] for v in t] for t in tris], piece=len(out)))
        parts = out

    # nodes by name rules
    for p in parts:
        p["node"] = next((n for m, n in rules if m(p["obj"], p["mat"])), None)
        p["skin"] = next((s for m, s in skins if m(p["obj"], p["mat"])), None)

    # the bike's frame: forward (the nose), up (Blender Z), the axles (the wheel parts' centres)
    def centre(ps):
        lo = Vector((1e9, 1e9, 1e9)); hi = -lo
        for p in ps:
            for v in p["verts"]:
                lo = Vector(map(min, lo, v[0])); hi = Vector(map(max, hi, v[0]))
        return (lo + hi) / 2, lo, hi
    def wheel_parts(which):
        sel = recipe.get("wheels", {}).get(which)
        if sel: m = matcher(sel); return [p for p in parts if m(p["obj"], p["mat"])]
        return [p for p in parts if p["node"] == ("OBJ_FRT_Tire" if which == "front" else "OBJ_BCK_Tire")]
    fw, rw = wheel_parts("front"), wheel_parts("rear")
    if "axles" in recipe:
        F, Rr = Vector(recipe["axles"]["front"]), Vector(recipe["axles"]["rear"])
        flo = fhi = F; rlo = rhi = Rr
        if fw: _, flo, fhi = centre(fw)
        if rw: _, rlo, rhi = centre(rw)
    else:
        if not fw or not rw: raise SystemExit("blender_rxmod: the recipe names no front / rear wheel parts (nodes OBJ_FRT_Tire / OBJ_BCK_Tire, \"wheels\" or \"axles\")")
        F, flo, fhi = centre(fw); Rr, rlo, rhi = centre(rw)
    up = Vector((0, 0, 1))
    fwd = F - Rr; fwd.z = 0; fwd.normalize()
    right = fwd.cross(up).normalized()
    radius_f = max(fhi.z - flo.z, 0) / 2; radius_r = max(rhi.z - rlo.z, 0) / 2
    s = unit * FEET
    def game(co):      # metres world -> feet, game axes (X right, Y up, Z forward) from the rear axle
        d = co - Rr
        return Vector((d.dot(right) * s, d.dot(up) * s, d.dot(fwd) * s))
    def gdir(n):
        return Vector((n.dot(right), n.dot(up), n.dot(fwd))).normalized()

    # position rules
    for p in parts:
        if p["node"] is not None: continue
        g = [game(v[0]) for v in p["verts"]]
        lo = Vector((min(v.x for v in g), min(v.y for v in g), min(v.z for v in g))); hi = Vector((max(v.x for v in g), max(v.y for v in g), max(v.z for v in g)))
        for w in recipe.get("where", []):
            ok = True
            if "in_wheel" in w:
                c = game(F if w["in_wheel"] == "front" else Rr); r = (radius_f if w["in_wheel"] == "front" else radius_r) * s * w.get("radius", 1.02)
                ok &= all(math.hypot(v.y - c.y, v.z - c.z) <= r for v in g)
            if "max_x" in w: ok &= max(abs(lo.x), abs(hi.x)) <= w["max_x"] * FEET
            if "box" in w:
                (a, b) = w["box"]; ok &= all(lo[k] >= a[k] * FEET and hi[k] <= b[k] * FEET for k in range(3))
            if ok: p["node"] = w["node"]; break
        if p["node"] is None: p["node"] = default
        p["lo"], p["hi"] = lo, hi

    # node frames (mod space): axles; the steering axis from the fork parts (their principal direction in the side view)
    world = {}
    def T(pos, yaxis=Vector((0, 1, 0)), zaxis=Vector((0, 0, 1))):
        y = yaxis.normalized(); x = y.cross(zaxis).normalized(); z = x.cross(y).normalized()
        if x.x < 0: x = -x; z = x.cross(y)
        return [x.x, y.x, z.x, pos.x, x.y, y.y, z.y, pos.y, x.z, y.z, z.z, pos.z, 0, 0, 0, 1]
    world["OBJ_Frame"] = T(Vector((0, 0, 0)))
    gF, gR = game(F), game(Rr)
    world["OBJ_FRT_Tire"] = T(gF); world["OBJ_BCK_Tire"] = T(gR)
    fork = [game(v[0]) for p in parts if p["node"] in ("OBJ_UPR_Forks", "OBJ_LWR_Forks") for v in p["verts"]]
    axis = Vector((0, 0.9, -0.45)); head = gF + Vector((0, 2.5, -1.2))
    if recipe.get("steer"):
        top, bot = Vector(recipe["steer"][0]) , Vector(recipe["steer"][1])
        head, axis = game(top), (game(top) - game(bot))
    elif len(fork) > 50:
        # the fork tubes: the vertices near the centre plane excluded (the clamps), a line fit in the side view through their centre
        tubes = [v for v in fork if abs(v.x) > 0.15] or fork
        cy = sum(v.y for v in tubes) / len(tubes); cz = sum(v.z for v in tubes) / len(tubes)
        syy = sum((v.y - cy) ** 2 for v in tubes); szz = sum((v.z - cz) ** 2 for v in tubes); syz = sum((v.y - cy) * (v.z - cz) for v in tubes)
        ang = 0.5 * math.atan2(2 * syz, syy - szz)
        axis = Vector((0, math.cos(ang), math.sin(ang)))
        if axis.y < 0: axis = -axis
        head = Vector((0, cy, cz)) + axis * (max(v.y for v in tubes) - cy)
    axis.normalize()
    world["OBJ_UPR_Forks"] = T(head, axis, Vector((0, 0, 1)))
    lower = head + axis * (gF - head).dot(axis)
    world["OBJ_LWR_Forks"] = T(lower, axis, Vector((0, 0, 1)))
    sw = [game(v[0]) for p in parts if p["node"] == "OBJ_Swingarm" for v in p["verts"]]
    pivot = Vector((0, sum(v.y for v in sw) / len(sw), max(v.z for v in sw) - 0.15)) if sw else gR + Vector((0, 0.35, 1.8))
    if recipe.get("pivot"): pivot = game(Vector(recipe["pivot"]))
    world["OBJ_Swingarm"] = T(pivot)
    for lever in ("OBJ_Throttle", "OBJ_ClutchLVR", "OBJ_BrakeLVR"):
        pts = [game(v[0]) for p in parts if p["node"] == lever for v in p["verts"]]
        if pts: world[lever] = T(sum(pts, Vector()) / len(pts), axis)
    used = [n for n in NODES if n in world]
    def parent(n):
        q = PARENT[n]
        while q is not None and q not in used: q = PARENT[q]
        return used.index(q) if q is not None else -1

    # materials
    def mat_of(p):
        return next((n for m, n in mats if m(p["obj"], p["mat"])), p["mat"] or "bike")
    names = []
    for p in parts:
        p["material"] = mat_of(p)
        if p["material"] not in names: names.append(p["material"])

    # surfaces: one per (node, material, skin) group
    groups = {}
    for p in parts:
        key = (p["node"], p["material"], json.dumps(p["skin"], sort_keys=True) if p["skin"] else "")
        groups.setdefault(key, []).append(p)
    blob = bytearray(b"RXMS" + struct.pack("<II", 1, 0))
    def string(x): b = x.encode("utf-8"); return struct.pack("<H", len(b)) + b
    blob += struct.pack("<I", len(used))
    for n in used: blob += string(n) + struct.pack("<i", parent(n)) + struct.pack("<16f", *world[n])
    blob += struct.pack("<I", len(names))
    for n in names: blob += string(n)
    blob += struct.pack("<I", len(groups))
    report = {"axles_ft": {"front": list(gF), "rear": list(gR)}, "wheelbase_ft": (gF - gR).length, "radius_in": {"front": radius_f * unit * 39.37, "rear": radius_r * unit * 39.37},
              "forward": list(fwd), "steer_axis": list(axis), "steer_head_ft": list(head), "swingarm_pivot_ft": list(pivot), "surfaces": [], "objects": {}}
    for (node, material, skin), ps in groups.items():
        P, Nn, U, U1, B, Wt, I = [], [], [], [], [], [], []
        sk = json.loads(skin) if skin else None
        for p in ps:
            base = len(P)
            g = [game(v[0]) for v in p["verts"]]
            if sk:
                # weighted along the part's long axis (game space): the end nearer the top node -> top, the other -> bottom
                c = sum(g, Vector()) / len(g)
                cov = [[sum((v[a] - c[a]) * (v[b] - c[b]) for v in g) for b in range(3)] for a in range(3)]
                d = Vector((0, 1, 0))
                for _ in range(30): d = Vector([sum(cov[a][b] * d[b] for b in range(3)) for a in range(3)]).normalized()
                if d.y < 0: d = -d
                ts = [(v - c).dot(d) for v in g]; t0, t1 = min(ts), max(ts)
                ramp = sk.get("ramp", [0.35, 0.65])
            for k, (co, n, u, u1) in enumerate(p["verts"]):
                P.append(g[k]); Nn.append(gdir(n)); U.append(u); U1.append(u1)
                if sk:
                    f = (ts[k] - t0) / max(1e-6, t1 - t0)            # 0 bottom .. 1 top
                    w = min(1.0, max(0.0, (f - ramp[0]) / max(1e-6, ramp[1] - ramp[0])))
                    B.append((used.index(sk["top"]), used.index(sk["bottom"]), 255, 255)); Wt.append((w, 1 - w, 0, 0))
            for a, b, c in p["tris"]: I.append((base + a, base + c, base + b))   # (reversed: Blender's front faces are counter-clockwise)
            report["objects"].setdefault(p["obj"], set()).add(node)
        flags = (1 if sk else 0) | 2
        blob += struct.pack("<IIIII", names.index(material), used.index(node) if node in used else 0, flags, len(P), len(I) * 3)
        blob += struct.pack("<%df" % (3 * len(P)), *[c for v in P for c in v])
        blob += struct.pack("<%df" % (3 * len(P)), *[c for v in Nn for c in v])
        blob += struct.pack("<%df" % (2 * len(P)), *[c for v in U for c in v])
        blob += struct.pack("<%df" % (2 * len(P)), *[c for v in U1 for c in v])
        if sk:
            blob += bytes([c for v in B for c in v])
            blob += struct.pack("<%df" % (4 * len(P)), *[c for v in Wt for c in v])
        blob += struct.pack("<%dI" % (3 * len(I)), *[c for t in I for c in t])
        report["surfaces"].append({"node": node, "material": material, "skinned": bool(sk), "vertices": len(P), "triangles": len(I), "parts": len(ps)})
    os.makedirs(os.path.dirname(os.path.abspath(o["out"])), exist_ok=True)
    with open(o["out"], "wb") as f: f.write(blob)
    report["objects"] = {k: sorted(v) for k, v in report["objects"].items()}
    report["materials"] = names
    report["blender_materials"] = sorted({p["mat"] for p in parts})
    print("blender_rxmod: %s: %d surfaces, %d vertices, %d triangles, wheelbase %.3f ft" % (o["out"], len(groups), sum(s["vertices"] for s in report["surfaces"]),
          sum(s["triangles"] for s in report["surfaces"]), report["wheelbase_ft"]))
    if o["report"]:
        with open(o["report"], "w") as f: json.dump(report, f, indent=1, default=list)
    if o["preview"]: preview(parts, o["preview"], F, Rr, fwd, right, o["texture"], o["view"])


def preview(parts, path, F, R, fwd, right, texture=None, view="side"):
    """A view of the bike as exported (QA of the recipe), rendered with Cycles on the CPU: each part coloured by its node (red: skinned),
    or with `texture` (the diffuse atlas) on its UVs. view: side (from the right), left, front, top, or three4 (front-right, from above)."""
    scene = bpy.context.scene
    for v in scene.view_layers: v.use = True
    for ob in list(scene.objects): ob.hide_render = True
    mesh = bpy.data.meshes.new("rxmod preview"); ob = bpy.data.objects.new("rxmod preview", mesh); scene.collection.objects.link(ob)
    bm = bmesh.new(); col = bm.loops.layers.color.new("node"); uvl = bm.loops.layers.uv.new("uv")
    for p in parts:
        c = COLORS["SKIN"] if p["skin"] else COLORS.get(p["node"], (1, 1, 1))
        vs = [bm.verts.new(v[0]) for v in p["verts"]]
        for t in p["tris"]:
            try: f = bm.faces.new([vs[t[0]], vs[t[1]], vs[t[2]]])
            except ValueError: continue
            for l, k in zip(f.loops, t): l[col] = (*c, 1); l[uvl].uv = p["verts"][k][2]
    bm.to_mesh(mesh); bm.free()
    for poly in mesh.polygons: poly.use_smooth = True
    mat = bpy.data.materials.new("rxmod preview"); mat.use_nodes = True; nt = mat.node_tree
    bsdf = nt.nodes.get("Principled BSDF"); bsdf.inputs["Roughness"].default_value = 0.6
    if texture:
        img = nt.nodes.new("ShaderNodeTexImage"); img.image = bpy.data.images.load(texture); img.interpolation = "Linear"
        nt.links.new(img.outputs["Color"], bsdf.inputs["Base Color"])
    else:
        vc = nt.nodes.new("ShaderNodeVertexColor"); vc.layer_name = "node"; nt.links.new(vc.outputs["Color"], bsdf.inputs["Base Color"])
    mesh.materials.append(mat)
    ob.hide_render = False
    world = bpy.data.worlds.new("rxmod w"); scene.world = world; world.use_nodes = True
    world.node_tree.nodes["Background"].inputs[0].default_value = (0.8, 0.82, 0.86, 1); world.node_tree.nodes["Background"].inputs[1].default_value = 1.4
    sun = bpy.data.objects.new("rxmod sun", bpy.data.lights.new("rxmod sun", "SUN")); scene.collection.objects.link(sun)
    sun.data.energy = 3.0; sun.rotation_euler = (math.radians(50), 0, math.radians(30))
    cam = bpy.data.objects.new("rxmod cam", bpy.data.cameras.new("rxmod cam")); scene.collection.objects.link(cam)
    mid = (F + R) / 2 + Vector((0, 0, 0.35 * (F - R).length / 1.48))
    d = {"side": right, "left": -right, "front": fwd, "top": Vector((0, 0, 1)), "three4": (right + fwd * 0.8 + Vector((0, 0, 0.5)))}[view].normalized()
    cam.location = mid + d * (F - R).length * 4.0; cam.data.type = "ORTHO"; cam.data.ortho_scale = (F - R).length * 1.75; cam.data.clip_end = 1e5
    cam.rotation_euler = (-d).to_track_quat("-Z", "Y" if view != "top" else "Z").to_euler()
    scene.camera = cam
    scene.render.engine = "CYCLES"; scene.cycles.device = "CPU"; scene.cycles.samples = 24; scene.cycles.use_denoising = False
    scene.render.film_transparent = False; scene.render.use_border = False
    scene.render.resolution_x, scene.render.resolution_y = 1600, 1000; scene.render.resolution_percentage = 100
    scene.render.filepath = path; scene.render.image_settings.file_format = "PNG"
    bpy.ops.render.render(write_still=True)


main()

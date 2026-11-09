// A bike mod's run-time number plates (w-fb-modplates, core/plan/QUEUE.md): the rider's number on the front plate and the side plates of a
// community bike mod, where the original bike it re-skins draws it.
//
// How the original puts the number on a bike: MXPlayer_MakeNumberPlate 005520d0 makes the plate texture (ReflexPlates, P+0xb00) and
// FUN_0054a050 binds it with Model_OverrideTexture( V+0x6a0, "Numbers", tex ) 005510a0, which looks the texture asset up BY NAME and
// overrides it for every material of the vehicle model that samples it. The stock plastics (Template_Vehicle_Plstc_Fnl and its kin) sample
// "Numbers" at the second UV set, which the stock chassis lays out over the front plate and the two side plates (the 128 x 128 plate
// texture's number box), and at a corner of the texture (clear) everywhere else.
//
// The classic PC bike mods (the user's "New Bike Models": all 20 MX chassis and all 334 of its kits) draw their chassis and kit with the
// rider jersey's program Jersey_FNL, whose plate slot (s_sampler2, lrp(diffuse, plate, plate.a) at the second UV set) holds the mod's own
// "<...>_NoPlate" texture - a fully transparent 4 x 4 DXT5 - and never names "Numbers"; their second UV set is a copy of the first (every
// vertex). The original's name lookup therefore misses them: the original game draws no number on these mods either, and binding the plate
// to that slot as it stands would smear the number over the whole kit atlas. Their kits leave the plates blank (white) where the stock
// plates are.
//
// The port's fallback (not the original's code, w-fb-modplates2): the number goes on the mod's OWN front number plate, found in its geometry.
// FRONT PLATE ONLY (the user's call after w-fb-modplates: the mods' side panels vary too much from bike to bike for a number to read right;
// they stay exactly as the mod made them, no number).
// - The stock chassis the mod re-skins says where its front plate is (the centre, facing, node and size of its "Numbers" triangles).
// - Candidates: the mod's kit plastics of the stock plate binding (MX_Plstc, Jersey_FNL with a transparent plate slot, static surfaces) on
//   the stock plate's node (OBJ_UPR_Forks: it steers with the bars), facing within 35 degrees of the stock plate, within 1.1 ft of it.
// - Panels: the candidates grown into connected flat regions (neighbours within 15 degrees, within 20 of the region's mean; the flatness under the box decides), each joined
//   with every candidate coplanar with it (10 degrees; within 0.015 ft - one layer - and within 0.03 ft - the plate's layers together: a plate split by seams, or not welded at all, is one panel); two neighbouring regions up to
//   30 degrees apart (a plate folded down its middle) are one panel too.
// - Each panel is rasterised in its own plane (axes: the reading direction across the plate's normal and the bike's up) with its colour.
//   Every other part of the built bike (anything more than 0.03 ft in front) is drawn into the same view, straight and from 12 degrees
//   above / either side; the plate's own other surfaces straight only (its lip; its outer shell 0.005 ft in front: the panel is then an
//   inner layer). What is thinner than 0.07 ft (a cable or a brake line
//   across the plate: the number sits behind it, as on a real bike) does not count. The number's box
//   (the stock digits' aspect plus a margin) is the largest that is the panel in 98.5 % of its cells, the panel's background (one of its
//   three most common colours, within 0.18) in 97 %, under a solid part in 8 % at most head-on and 20 %
//   from the tilted views, flat under it (0.05 ft), no larger than 85 % of the stock plate, no smaller than 30 % of it, with an 8 % margin,
//   nearest the stock spot; a light background counts 25 % more.
// - Its triangles get vertices of their own (the plate never bleeds into the next part) with a planar projection on the panel's plane:
//   upright, uniform scale. Every other vertex of the kit plastics: a clear corner of the plate texture (as the transparent slot was).
// - No panel passes: no number (a blank plate rather than a broken one).
// - Colour (modPlateDark): dark digits on a light background, light on a dark one, outlined in the other colour.
// The slot is tagged as the "Numbers" plate: ReflexPlates binds the rider's plate to it wherever a bike is shown (garage, hub, race, replays,
// podium). A mod with its own "Numbers" slot keeps it (the original's binding works there). ModPlates = false: off.
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using UnityEngine;
using Debug = UnityEngine.Debug;

namespace Reflex.Port.Content
{
    public sealed partial class ReflexBikeModel
    {
        /// The port's mod number plates (header). False: a mod bike stays as the original would draw it (no number).
        public static bool ModPlates = true;

        /// The program of a mod surface's material and its plate decal's texture slot (sampled at the second UV set).
        static readonly Dictionary<string, int> PlateSlotOf = new Dictionary<string, int>(StringComparer.Ordinal) { { "Jersey_FNL", 2 } };
        /// ... and its diffuse's slot (the colour under the plate: Jersey_FNL lrp(s3 diffuse, s2 plate, plate.a)).
        static readonly Dictionary<string, int> DiffuseSlotOf = new Dictionary<string, int>(StringComparer.Ordinal) { { "Jersey_FNL", 3 } };

        /// A mod's front plate's digits (ReflexPlates.Apply; null: not a mod plate, the original's rule): dark on a light plate, light on a
        /// dark one, outlined in the other colour. The original picks one colour per bike (dark but on a 125 cc motocross bike / ATV,
        /// 0054bec0) for its stock kits' plates; the mods' plates are white on every class, where the 125s' light digits would vanish.
        public bool? modPlateDark;

        static readonly Vector2 Clear = new Vector2(-1f, 2f);   // (D3D -1, -1: the plate texture's clamped corner texel, clear)
        // the digits' ink in the 128 plate (ReflexPlateRaster.Bike, 3 digits: x 9 .. 119, y 26 .. 119) as UV, and the box's margin
        const float InkU0 = 0.06f, InkU1 = 0.94f, InkV0 = 0.19f, InkV1 = 0.94f, BoxMargin = 0.08f;
        const float MinSize = 0.3f, MaxSize = 0.85f, CellsPerPlate = 48f, Coverage = 0.985f, BackgroundShare = 0.97f, HiddenHeadOn = 0.08f, HiddenTilted = 0.2f, Flatness = 0.05f, ColourTolerance = 0.18f;

        sealed class Projector
        {
            public string node; public Vector3 normal, min, max;
            public readonly List<(Vector3 a, Vector3 b, Vector3 c, Vector3 n, Vector2 ua, Vector2 ub, Vector2 uc)> tris = new List<(Vector3, Vector3, Vector3, Vector3, Vector2, Vector2, Vector2)>();
        }

        /// A stock plate: its centre, its outward normal, the size of its plate texture (ft per UV unit) and the part group it hangs on.
        sealed class StockPlate { public string name, group, node; public Vector3 centre, normal; public float size; }

        /// What was placed on one plate (for the log) and the triangles that take it.
        sealed class Placed
        {
            public string name; public bool ok; public string why = ""; public float size, stockSize, tilt, offset; public Vector3 colour; public bool dark;
            public override string ToString() => ok
                ? $"{name} {(dark ? "dark" : "light")} {size:0.00} ft ({100 * size / stockSize:0}% of stock), panel {tilt:0} deg off the stock plate, {offset:0.00} ft from it, background {colour.ToString("F2")}"
                : $"{name} none ({why})";
        }

        /// The kit's surfaces' new triangles (by their index in the chassis' part list): the triangle and its corners' second UVs (Unity's).
        sealed class Done
        {
            public readonly Dictionary<int, List<(int tri, Vector2 a, Vector2 b, Vector2 c)>> split = new Dictionary<int, List<(int, Vector2, Vector2, Vector2)>>();
            public readonly List<Placed> plates = new List<Placed>(); public long ms;
        }
        static readonly Dictionary<string, Done> doneCache = new Dictionary<string, Done>();

        /// The mod chassis' parts (LOD 0: the part, its node, its binding, material and definition) and every built part of the bike (the
        /// panel's occluders): see the header.
        void ProjectModPlates(BikeSpec spec, BikeSpec partSpec, Rfx.Install install, VehicleCatalog cat, Rfx.Model hier, List<(string db, string pkg)> chosen,
                              List<(Transform part, string node, string binding, UnityEngine.Material material, VehicleMaterialDef def)> chassis)
        {
            if (!ModPlates || chassis.Count == 0) return;
            if (chassis.Any(p => p.def != null && p.def.textures.Any(t => string.Equals(t.name, "Numbers", StringComparison.OrdinalIgnoreCase)))) return;   // the mod's own plate
            var sw = Stopwatch.StartNew();
            var projectors = StockPlates(spec, install, cat, chosen, out string source, out var plateBindings);
            if (projectors.Count == 0) { Debug.Log($"Reflex bike {spec.name}: no stock number plate ({source})"); return; }
            string Norm(string b) => b.StartsWith("HR_", StringComparison.OrdinalIgnoreCase) ? b.Substring(3) : b;
            var kit = new HashSet<string>(chosen.Select(c => c.db + "/" + c.pkg), StringComparer.OrdinalIgnoreCase);
            // the surfaces that may take it: the kit's plastics of the stock plate binding (MX_Plstc), Jersey_FNL with a transparent plate slot
            var takers = chassis.Where(p => p.material && p.def != null && plateBindings.Contains(Norm(p.binding)) && kit.Contains(p.def.db + "/" + p.def.pkg)
                                            && PlateSlotOf.TryGetValue(ReflexObjectShading.ProgramName(p.material), out int k)
                                            && k < p.def.textures.Count && Transparent(install, p.def.textures[k])).ToList();
            if (takers.Count == 0) { Debug.Log($"Reflex bike {spec.name}: no mod surface takes the stock number plate ({string.Join(", ", plateBindings)})"); return; }
            string cacheKey = BikeMods.Version + "|" + spec.chassisDb + "/" + spec.chassisPkg + "/" + spec.chassisModel + "|" + string.Join(",", chosen.Select(c => c.db + "/" + c.pkg))
                            + "|" + string.Join(",", (partSpec ?? spec).partModels.Select(m => m.db + "/" + m.pkg));
            Done done;
            lock (doneCache) doneCache.TryGetValue(cacheKey, out done);
            bool cached = done != null;
            if (done == null)
            {
                done = Compute(chassis, takers, projectors, hier, install);
                lock (doneCache) { if (doneCache.Count >= 8) doneCache.Clear(); doneCache[cacheKey] = done; }
            }
            // the takers: every vertex clear, the plate's triangles on vertices of their own with the planar UVs
            foreach (var p in takers)
            {
                var mesh = p.part.GetComponent<MeshFilter>()?.sharedMesh; if (!mesh) continue;
                done.split.TryGetValue(chassis.IndexOf(p), out var tris);
                Split(mesh, tris);
            }
            modPlateDark = done.plates.FirstOrDefault(x => x.ok)?.dark ?? true;
            foreach (var m in takers.Select(p => (p.material, p.def)).Distinct())
            {
                int k = PlateSlotOf[ReflexObjectShading.ProgramName(m.material)]; var t = m.def.textures[k];
                m.material.SetTexture("_Tex" + k, VehicleCatalog.ClearTexture);
                m.material.SetOverrideTag(ReflexPlates.Tag, "_Tex" + k + ":Numbers:" + t.addrU + ":" + t.addrV + ":" + t.mipFilter);
            }
            Debug.Log($"Reflex bike {spec.name}: the mod's number plates (stock {source}): {string.Join("; ", done.plates)}; {(cached ? "cached" : done.ms + " ms")}");
        }

        /// The mesh's second UV set made clear, and the listed triangles moved onto vertices of their own with the given UVs (a copy of
        /// every attribute: position, normal, tangent, UV, the rest position of MATERIALS: ENHANCED).
        static void Split(Mesh mesh, List<(int tri, Vector2 a, Vector2 b, Vector2 c)> tris)
        {
            int n = mesh.vertexCount;
            if (tris == null || tris.Count == 0) { mesh.uv2 = Enumerable.Repeat(Clear, n).ToArray(); return; }
            int add = tris.Count * 3, m = n + add;
            var pos = mesh.vertices; var nrm = mesh.normals; var tan = mesh.tangents; var uv = mesh.uv;
            var rest = new List<Vector3>(); mesh.GetUVs(2, rest);
            var tri = mesh.triangles;
            Array.Resize(ref pos, m); if (nrm.Length == n) Array.Resize(ref nrm, m); if (tan.Length == n) Array.Resize(ref tan, m); if (uv.Length == n) Array.Resize(ref uv, m);
            var uv2 = Enumerable.Repeat(Clear, m).ToArray();
            int w = n;
            foreach (var (t, a, b, c) in tris)
            {
                var corner = new[] { a, b, c };
                for (int k = 0; k < 3; k++)
                {
                    int src = tri[t * 3 + k];
                    pos[w] = pos[src]; if (nrm.Length == m) nrm[w] = nrm[src]; if (tan.Length == m) tan[w] = tan[src]; if (uv.Length == m) uv[w] = uv[src];
                    if (rest.Count == n) rest.Add(rest[src]);
                    uv2[w] = corner[k]; tri[t * 3 + k] = w; w++;
                }
            }
            mesh.vertices = pos; if (nrm.Length == m) mesh.normals = nrm; if (tan.Length == m) mesh.tangents = tan; if (uv.Length == m) mesh.uv = uv;
            mesh.uv2 = uv2; if (rest.Count == m) mesh.SetUVs(2, rest);
            mesh.triangles = tri;
        }

        /// The stock plates (front: the faces on an articulated node; left / right: the frame's, by their facing) from the projectors: the
        /// area-weighted centre and normal of the triangles over the plate texture's middle, the size of the plate texture in ft.
        static List<StockPlate> StockPlateList(List<Projector> projectors, Func<string, string> group)
        {
            var acc = new Dictionary<string, (StockPlate p, float a3, float auv, Vector3 c, Vector3 n)>();
            foreach (var pr in projectors)
            {
                string g = group(pr.node);
                foreach (var t in pr.tris)
                {
                    var cuv = (t.ua + t.ub + t.uc) / 3f;
                    if (cuv.x < 0.1f || cuv.x > 0.9f || cuv.y < 0.1f || cuv.y > 0.9f) continue;
                    float a3 = Vector3.Cross(t.b - t.a, t.c - t.a).magnitude * 0.5f;
                    float auv = Mathf.Abs((t.ub.x - t.ua.x) * (t.uc.y - t.ua.y) - (t.uc.x - t.ua.x) * (t.ub.y - t.ua.y)) * 0.5f;
                    string name = g.Length > 0 ? (g.IndexOf("UPR_Forks", StringComparison.OrdinalIgnoreCase) >= 0 ? "front" : null) : t.n.x >= 0 ? "right" : "left";
                    if (name == null) continue;   // (a swingarm face: no plate there)
                    if (!acc.TryGetValue(name, out var e)) e = (new StockPlate { name = name, group = g, node = pr.node }, 0, 0, Vector3.zero, Vector3.zero);
                    e.a3 += a3; e.auv += auv; e.c += (t.a + t.b + t.c) / 3f * a3; e.n += t.n * a3;
                    acc[name] = e;
                }
            }
            var list = new List<StockPlate>();
            foreach (var (p, a3, auv, c, n) in acc.Values)
            {
                if (a3 <= 0 || auv <= 0) continue;
                p.centre = c / a3; p.normal = n.normalized; p.size = Mathf.Sqrt(a3 / auv);
                list.Add(p);
            }
            return list.OrderBy(p => p.name).ToList();
        }

        static Done Compute(List<(Transform part, string node, string binding, UnityEngine.Material material, VehicleMaterialDef def)> chassis,
                            List<(Transform part, string node, string binding, UnityEngine.Material material, VehicleMaterialDef def)> takers,
                            List<Projector> projectors, Rfx.Model hier, Rfx.Install install)
        {
            var done = new Done(); var sw = Stopwatch.StartNew();
            // a node's articulated group: the root's child it hangs under when that child has children (OBJ_UPR_Forks, OBJ_Swingarm), else ""
            string Group(string node)
            {
                int i = hier.NodeIndex(node); if (i < 0) return node;
                while (hier.nodes[i].parent >= 0 && hier.nodes[hier.nodes[i].parent].parent >= 0) i = hier.nodes[i].parent;
                if (hier.nodes[i].parent < 0) return "";
                for (int j = 0; j < hier.nodes.Length; j++) if (hier.nodes[j].parent == i) return hier.nodes[i].name;
                return "";
            }
            var plates = StockPlateList(projectors, Group);

            // every built part of the bike in the chassis' model space (the built parts sit at the model's origin): the occluders
            var root = chassis[0].part.parent;
            var occluders = new List<(Vector3[] v, int[] t, Bounds b, Transform part)>();
            foreach (var mf in root.GetComponentsInChildren<MeshFilter>(true))
            {
                var mesh = mf.sharedMesh; if (!mesh) continue;
                var m = root.worldToLocalMatrix * mf.transform.localToWorldMatrix;
                var v = mesh.vertices; for (int i = 0; i < v.Length; i++) v[i] = m.MultiplyPoint3x4(v[i]);
                var b = new Bounds(v.Length > 0 ? v[0] : Vector3.zero, Vector3.zero); foreach (var x in v) b.Encapsulate(x);
                occluders.Add((v, mesh.triangles, b, mf.transform));
            }

            // the takers' triangles (static surfaces: their mesh is in model space and is never re-skinned)
            var tk = new List<(int index, Vector3[] v, int[] t, Vector2[] uv0, Texels tex, string group, string node, Transform part)>();
            foreach (var p in takers)
            {
                var mf = p.part.GetComponent<MeshFilter>(); var mesh = mf ? mf.sharedMesh : null;
                if (!mesh || p.part.GetComponent<OriginalWorld.OriginalBikeSkin>() != null) continue;
                var tex = DiffuseSlotOf.TryGetValue(ReflexObjectShading.ProgramName(p.material), out int ds) && ds < p.def.textures.Count ? Texels.Of(install, p.def.textures[ds]) : null;
                tk.Add((chassis.IndexOf(p), mesh.vertices, mesh.triangles, mesh.uv, tex, Group(p.node), p.node, p.part));
            }

            // the front plate only (the user's call: the mods' side panels differ too much from bike to bike for a number to read right there;
            // they stay as the mod made them)
            foreach (var sp in plates.Where(x => x.name == "front"))
            {
                var placed = new Placed { name = sp.name, stockSize = sp.size };
                done.plates.Add(placed);
                // candidates: kit triangles on the plate's part, facing the plate (either side: single-sided shells), near it
                var cand = new List<(int k, int t, Vector3 n, float area, Vector3 c)>();
                for (int k = 0; k < tk.Count; k++)
                {
                    if (tk[k].group != sp.group || !string.Equals(tk[k].node, sp.node, StringComparison.OrdinalIgnoreCase)) continue;   // (on the plate's own node)
                    var v = tk[k].v; var T = tk[k].t;
                    for (int t = 0; t + 2 < T.Length; t += 3)
                    {
                        Vector3 a = v[T[t]], b = v[T[t + 1]], c = v[T[t + 2]];
                        var cr = Vector3.Cross(b - a, c - a); float ar = cr.magnitude * 0.5f; if (ar < 1e-9f) continue;
                        var n = cr / (2 * ar); if (Vector3.Dot(n, sp.normal) < 0) n = -n;
                        var cen = (a + b + c) / 3f;
                        if (Vector3.Dot(n, sp.normal) < Mathf.Cos(35 * Mathf.Deg2Rad) || (cen - sp.centre).magnitude > 1.1f || cen.y < sp.centre.y - 0.45f) continue;   // (not the engine's covers below it)
                        cand.Add((k, t / 3, n, ar, cen));
                    }
                }
                if (cand.Count == 0) { placed.why = "no kit panel faces the plate"; continue; }
                // connected flat regions over welded positions
                var vid = new Dictionary<Vector3Int, int>();
                int V(Vector3 x) { var q = new Vector3Int(Mathf.RoundToInt(x.x * 1e4f), Mathf.RoundToInt(x.y * 1e4f), Mathf.RoundToInt(x.z * 1e4f)); if (!vid.TryGetValue(q, out int i)) vid[q] = i = vid.Count; return i; }
                var byEdge = new Dictionary<long, List<int>>();
                for (int i = 0; i < cand.Count; i++)
                {
                    var (k, t, _, _, _) = cand[i]; var T = tk[k].t; var v = tk[k].v;
                    int a = V(v[T[t * 3]]), b = V(v[T[t * 3 + 1]]), c = V(v[T[t * 3 + 2]]);
                    foreach (var (x, y) in new[] { (a, b), (b, c), (c, a) })
                    {
                        long key = (long)Math.Min(x, y) << 32 | (uint)Math.Max(x, y);
                        if (!byEdge.TryGetValue(key, out var l)) byEdge[key] = l = new List<int>();
                        l.Add(i);
                    }
                }
                var nb = new List<int>[cand.Count]; for (int i = 0; i < nb.Length; i++) nb[i] = new List<int>();
                foreach (var l in byEdge.Values) foreach (int x in l) foreach (int y in l) if (x != y) nb[x].Add(y);
                var seen = new bool[cand.Count]; var regions = new List<List<int>>(); var all = new List<List<int>>();
                foreach (int s0 in Enumerable.Range(0, cand.Count).OrderBy(i => (cand[i].c - sp.centre).sqrMagnitude))
                {
                    if (seen[s0]) continue;
                    var reg = new List<int> { s0 }; seen[s0] = true; var mean = cand[s0].n * cand[s0].area; var q = new Stack<int>(); q.Push(s0);
                    while (q.Count > 0)
                    {
                        int x = q.Pop();
                        foreach (int y in nb[x])
                        {
                            if (seen[y]) continue;
                            if (Vector3.Dot(cand[y].n, mean.normalized) < Mathf.Cos(20 * Mathf.Deg2Rad) || Vector3.Dot(cand[y].n, cand[x].n) < Mathf.Cos(15 * Mathf.Deg2Rad)) continue;
                            seen[y] = true; reg.Add(y); q.Push(y); mean += cand[y].n * cand[y].area;
                        }
                    }
                    all.Add(reg);
                    if (reg.Sum(i => cand[i].area) >= MinSize * MinSize * sp.size * sp.size) regions.Add(reg);
                }
                // a plate folded down its middle (a shallow V: two halves up to 30 degrees apart, each within 15 of their mean) is one panel too
                {
                    var regionOf = new int[cand.Count];
                    for (int r = 0; r < all.Count; r++) foreach (int i in all[r]) regionOf[i] = r;
                    Vector3 Mean(List<int> reg) { var m = Vector3.zero; foreach (int i in reg) m += cand[i].n * cand[i].area; return m.normalized; }
                    var pairs = new HashSet<(int, int)>();
                    for (int i = 0; i < cand.Count; i++) foreach (int j in nb[i]) { int a = regionOf[i], b = regionOf[j]; if (a < b) pairs.Add((a, b)); }
                    foreach (var (a, b) in pairs)
                    {
                        Vector3 na = Mean(all[a]), nbm = Mean(all[b]);
                        if (Vector3.Dot(na, nbm) < Mathf.Cos(30 * Mathf.Deg2Rad)) continue;
                        var u = all[a].Concat(all[b]).ToList(); var nu = Mean(u);
                        if (Vector3.Dot(na, nu) < Mathf.Cos(15 * Mathf.Deg2Rad) || Vector3.Dot(nbm, nu) < Mathf.Cos(15 * Mathf.Deg2Rad)) continue;
                        if (u.Sum(i => cand[i].area) >= MinSize * MinSize * sp.size * sp.size) regions.Add(u);
                    }
                }
                // each region with every candidate coplanar with it; planes found twice are one
                var planes = new List<(Vector3 n, Vector3 o)>(); var panels = new List<List<int>>();
                // (every region seeds a plane, small ones too: a plate whose triangles do not share welded corners is many small regions; the
                // plane's coplanar candidates together must be big enough)
                foreach (var reg in regions.OrderByDescending(r => r.Sum(i => cand[i].area)).Concat(all.Where(r => !regions.Contains(r)).OrderByDescending(r => r.Sum(i => cand[i].area))).Take(48))
                {
                    float A = 0; Vector3 n = Vector3.zero, o = Vector3.zero;
                    foreach (int i in reg) { A += cand[i].area; n += cand[i].n * cand[i].area; o += cand[i].c * cand[i].area; }
                    n.Normalize(); o /= A;
                    if (planes.Any(pl => Vector3.Dot(pl.n, n) > Mathf.Cos(5 * Mathf.Deg2Rad) && Mathf.Abs(Vector3.Dot(o - pl.o, pl.n)) < 0.01f)) continue;   // (layers of a plate 0.02 - 0.04 ft apart are separate panels)
                    planes.Add((n, o));
                    // the plane joined with its coplanar candidates twice: within 0.015 ft (one layer of the plate) and within 0.03 ft (the plate's
                    // layers together, where its outer shell is not one surface of its own)
                    foreach (float tol in new[] { 0.015f, 0.03f })
                    {
                        var set = new HashSet<int>(reg);
                        for (int i = 0; i < cand.Count; i++)
                        {
                            if (set.Contains(i) || Vector3.Dot(cand[i].n, n) < Mathf.Cos(10 * Mathf.Deg2Rad)) continue;
                            var (k, t, _, _, _) = cand[i]; var T = tk[k].t; var v = tk[k].v;
                            if (Mathf.Abs(Vector3.Dot(v[T[t * 3]] - o, n)) <= tol && Mathf.Abs(Vector3.Dot(v[T[t * 3 + 1]] - o, n)) <= tol && Mathf.Abs(Vector3.Dot(v[T[t * 3 + 2]] - o, n)) <= tol) set.Add(i);
                        }
                        if (set.Sum(i => cand[i].area) < MinSize * MinSize * sp.size * sp.size) continue;
                        var list = set.OrderBy(i => i).ToList();
                        if (!panels.Any(x => x.SequenceEqual(list))) panels.Add(list);
                    }
                }
                if (panels.Count == 0) { placed.why = "no flat kit panel big enough"; continue; }

                Fit best = null; float bestFree = 0; string hider = null;
                foreach (var panel in panels)
                {
                    var f = FitPanel(sp, panel, cand, tk, occluders, (best?.Score ?? 0) / 1.25f, out float free, out string by);
                    if (free > bestFree) { bestFree = free; hider = by; }
                    if (f != null && (best == null || f.Score > best.Score)) best = f;
                }
                if (best == null)
                {
                    placed.why = bestFree >= MinSize * sp.size ? $"the free panel is hidden by another part (unhidden it would take {100 * bestFree / sp.size:0}% of the stock plate; {(string.IsNullOrEmpty(hider) ? "under the plate's own outer layer" : "hidden by " + hider)})"
                                                               : $"no flat, even-coloured panel takes {MinSize * 100:0}% of the stock plate (largest {100 * bestFree / sp.size:0}%)";
                    continue;
                }
                placed.ok = true; placed.size = best.size; placed.colour = best.colour; placed.dark = 0.299f * best.colour.x + 0.587f * best.colour.y + 0.114f * best.colour.z > 0.4f;
                placed.tilt = Mathf.Acos(Mathf.Clamp(Vector3.Dot(best.n, sp.normal), -1, 1)) * Mathf.Rad2Deg; placed.offset = (best.centre - sp.centre).magnitude;
                // the panel's triangles: planar UVs (the plate texture's square of side size; the ink box at the fitted place)
                foreach (int i in best.panel)
                {
                    var (k, t, _, _, _) = cand[i]; var T = tk[k].t; var v = tk[k].v;
                    Vector2 UV(Vector3 x)
                    {
                        float s = Vector3.Dot(x - best.o, best.e1), tt = Vector3.Dot(x - best.o, best.e2);
                        float u = (s - best.texLeft) / best.size, vd = (best.texTop - tt) / best.size;   // (D3D v grows downwards)
                        return new Vector2(u, 1 - vd);   // (the plate texture clamps: clear past its border)
                    }
                    if (!done.split.TryGetValue(tk[k].index, out var l)) done.split[tk[k].index] = l = new List<(int, Vector2, Vector2, Vector2)>();
                    l.Add((t, UV(v[T[t * 3]]), UV(v[T[t * 3 + 1]]), UV(v[T[t * 3 + 2]])));
                }
            }
            done.ms = sw.ElapsedMilliseconds;
            return done;
        }

        /// A number box on one panel.
        sealed class Fit
        {
            public List<int> panel; public Vector3 o, n, e1, e2, centre, colour; public float size, texLeft, texTop;
            /// The size, a light background (the kits' number-plate white) counting 25 % more than a dark graphic.
            public float Score => size * (0.299f * colour.x + 0.587f * colour.y + 0.114f * colour.z > 0.55f ? 1.25f : 1f);
        }

        /// The largest number box that fits on the panel (see the header), or null; free: the largest that would fit with nothing hiding it.
        static Fit FitPanel(StockPlate sp, List<int> panel, List<(int k, int t, Vector3 n, float area, Vector3 c)> cand,
                            List<(int index, Vector3[] v, int[] t, Vector2[] uv0, Texels tex, string group, string node, Transform part)> tk,
                            List<(Vector3[] v, int[] t, Bounds b, Transform part)> occluders, float beat, out float free, out string hiddenBy)
        {
            free = 0; hiddenBy = null;
            float A = 0; Vector3 n = Vector3.zero, o = Vector3.zero;
            foreach (int i in panel) { A += cand[i].area; n += cand[i].n * cand[i].area; o += cand[i].c * cand[i].area; }
            n.Normalize(); o /= A;
            // the reading direction seen from outside (left-handed, as the game and Unity: a camera looking along -n has its right at up x -n)
            var e1 = Vector3.Cross(Vector3.up, -n); if (e1.sqrMagnitude < 1e-6f) return null; e1.Normalize();
            var e2 = (Vector3.up - Vector3.Dot(Vector3.up, n) * n).normalized;   // up on the panel
            float cs = sp.size / CellsPerPlate;
            float s0 = float.MaxValue, s1 = float.MinValue, t0 = float.MaxValue, t1 = float.MinValue;
            var tris = new List<(Vector3 a, Vector3 b, Vector3 c, Vector2 ua, Vector2 ub, Vector2 uc, Texels tex)>();
            foreach (int i in panel)
            {
                var (k, t, _, _, _) = cand[i]; var T = tk[k].t; var v = tk[k].v; var uv0 = tk[k].uv0;
                int ia = T[t * 3], ib = T[t * 3 + 1], ic = T[t * 3 + 2];
                tris.Add((v[ia], v[ib], v[ic], uv0[ia], uv0[ib], uv0[ic], tk[k].tex));
                foreach (var x in new[] { v[ia], v[ib], v[ic] }) { float s = Vector3.Dot(x - o, e1), tt = Vector3.Dot(x - o, e2); s0 = Mathf.Min(s0, s); s1 = Mathf.Max(s1, s); t0 = Mathf.Min(t0, tt); t1 = Mathf.Max(t1, tt); }
            }
            int W = Mathf.CeilToInt((s1 - s0) / cs) + 1, H = Mathf.CeilToInt((t1 - t0) / cs) + 1;
            if (W * H > 600000 || W < 2 || H < 2) return null;
            float boxW = (InkU1 - InkU0 + 2 * BoxMargin), boxH = (InkV1 - InkV0 + 2 * BoxMargin);   // (in plate sizes)
            float maxSize = Mathf.Min(MaxSize * sp.size, (s1 - s0) / boxW, (t1 - t0) / boxH);
            if (maxSize < MinSize * sp.size || maxSize <= beat)
            {
                if (Environment.GetEnvironmentVariable("REFLEX_MODPLATES_DEBUG") == "1") Debug.Log($"[modplates] panel {panel.Count} tris, n {n.ToString("F2")}, {s1 - s0:0.00} x {t1 - t0:0.00} ft: too small ({maxSize:0.00} ft, beat {beat:0.00})");
                return null;
            }
            var h = new float[W * H]; var col = new Vector3[W * H]; var cov = new bool[W * H];
            for (int i = 0; i < h.Length; i++) h[i] = float.NegativeInfinity;
            void Raster(Vector3 a, Vector3 b, Vector3 c, Action<int, float, float, float, float> hit, Vector2 tilt = default)
            {
                float ha = Vector3.Dot(a - o, n), hb = Vector3.Dot(b - o, n), hc = Vector3.Dot(c - o, n);
                // (tilt: a view off the normal - a point in front of the panel lands shifted by its height x tilt)
                Vector2 pa = new Vector2(Vector3.Dot(a - o, e1), Vector3.Dot(a - o, e2)) + tilt * Mathf.Max(0, ha), pb = new Vector2(Vector3.Dot(b - o, e1), Vector3.Dot(b - o, e2)) + tilt * Mathf.Max(0, hb),
                        pc = new Vector2(Vector3.Dot(c - o, e1), Vector3.Dot(c - o, e2)) + tilt * Mathf.Max(0, hc);
                float den = (pb.y - pc.y) * (pa.x - pc.x) + (pc.x - pb.x) * (pa.y - pc.y); if (Mathf.Abs(den) < 1e-12f) return;
                int x0 = Mathf.Max(0, Mathf.FloorToInt((Mathf.Min(pa.x, Mathf.Min(pb.x, pc.x)) - s0) / cs)), x1 = Mathf.Min(W - 1, Mathf.FloorToInt((Mathf.Max(pa.x, Mathf.Max(pb.x, pc.x)) - s0) / cs));
                int y0 = Mathf.Max(0, Mathf.FloorToInt((Mathf.Min(pa.y, Mathf.Min(pb.y, pc.y)) - t0) / cs)), y1 = Mathf.Min(H - 1, Mathf.FloorToInt((Mathf.Max(pa.y, Mathf.Max(pb.y, pc.y)) - t0) / cs));
                for (int y = y0; y <= y1; y++)
                    for (int x = x0; x <= x1; x++)
                    {
                        float px = s0 + (x + 0.5f) * cs, py = t0 + (y + 0.5f) * cs;
                        float wa = ((pb.y - pc.y) * (px - pc.x) + (pc.x - pb.x) * (py - pc.y)) / den, wb = ((pc.y - pa.y) * (px - pc.x) + (pa.x - pc.x) * (py - pc.y)) / den, wc = 1 - wa - wb;
                        if (wa < 0 || wb < 0 || wc < 0) continue;
                        hit(y * W + x, wa * ha + wb * hb + wc * hc, wa, wb, wc);
                    }
            }
            foreach (var tr in tris)
                Raster(tr.a, tr.b, tr.c, (k, hh, wa, wb, wc) =>
                {
                    if (hh <= h[k]) return;
                    h[k] = hh; cov[k] = true;
                    var uv = wa * tr.ua + wb * tr.ub + wc * tr.uc;   // (Unity's v: 1 - D3D)
                    col[k] = tr.tex?.Rgb(uv.x, 1 - uv.y) ?? new Vector3(0.9f, 0.9f, 0.9f);
                });
            // the plate background: the panel's most common colours (8 levels a channel)
            var counts = new Dictionary<int, int>();
            for (int k = 0; k < cov.Length; k++) if (cov[k]) { int c = Code(col[k]); counts[c] = counts.TryGetValue(c, out int x) ? x + 1 : 1; }
            var backs = counts.OrderByDescending(x => x.Value).Take(3).Select(x => x.Key).ToList();
            var target = new Vector2(Vector3.Dot(sp.centre - o, e1), Vector3.Dot(sp.centre - o, e2));

            Fit Search(bool[] ok, bool[] covered, bool[] coveredHeadOn, out Vector3 background)
            {
                background = default; Fit found = null;
                foreach (int code in backs)
                {
                    Vector3 mean = Vector3.zero; int cnt = 0;
                    for (int k = 0; k < cov.Length; k++) if (ok[k] && Code(col[k]) == code) { mean += col[k]; cnt++; }
                    if (cnt == 0) continue;
                    mean /= cnt;
                    var valid = new bool[cov.Length]; for (int k = 0; k < cov.Length; k++) valid[k] = ok[k] && (col[k] - mean).magnitude < ColourTolerance;
                    int Wp = W + 1; var sat = new int[Wp * (H + 1)]; var satOk = new int[Wp * (H + 1)]; var satHid = new int[Wp * (H + 1)]; var satHead = new int[Wp * (H + 1)];
                    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++)
                    {
                        sat[(y + 1) * Wp + x + 1] = (valid[y * W + x] ? 1 : 0) + sat[(y + 1) * Wp + x] + sat[y * Wp + x + 1] - sat[y * Wp + x];
                        satOk[(y + 1) * Wp + x + 1] = (ok[y * W + x] ? 1 : 0) + satOk[(y + 1) * Wp + x] + satOk[y * Wp + x + 1] - satOk[y * Wp + x];
                        satHid[(y + 1) * Wp + x + 1] = (covered != null && covered[y * W + x] ? 1 : 0) + satHid[(y + 1) * Wp + x] + satHid[y * Wp + x + 1] - satHid[y * Wp + x];
                        satHead[(y + 1) * Wp + x + 1] = (coveredHeadOn != null && coveredHeadOn[y * W + x] ? 1 : 0) + satHead[(y + 1) * Wp + x] + satHead[y * Wp + x + 1] - satHead[y * Wp + x];
                    }
                    float weight = 0.299f * mean.x + 0.587f * mean.y + 0.114f * mean.z > 0.55f ? 1.25f : 1f;   // (as Fit.Score)
                    for (float size = maxSize; size >= MinSize * sp.size && size > beat && size * weight > (found?.Score ?? 0); size *= 0.96f)
                    {
                        int w = Mathf.CeilToInt(boxW * size / cs), hh = Mathf.CeilToInt(boxH * size / cs);
                        if (w >= W || hh >= H) continue;
                        var spots = new List<(float d, int x, int y)>();
                        for (int y = 0; y + hh <= H; y++)
                            for (int x = 0; x + w <= W; x++)
                            {
                                int sum = sat[(y + hh) * Wp + x + w] - sat[y * Wp + x + w] - sat[(y + hh) * Wp + x] + sat[y * Wp + x];
                                int sumOk = satOk[(y + hh) * Wp + x + w] - satOk[y * Wp + x + w] - satOk[(y + hh) * Wp + x] + satOk[y * Wp + x];
                                int sumHid = satHid[(y + hh) * Wp + x + w] - satHid[y * Wp + x + w] - satHid[(y + hh) * Wp + x] + satHid[y * Wp + x];
                                int sumHead = satHead[(y + hh) * Wp + x + w] - satHead[y * Wp + x + w] - satHead[(y + hh) * Wp + x] + satHead[y * Wp + x];
                                // (the panel everywhere; its background nearly everywhere; another part - a brake hose, a cable across the plate - over
                                // 8 % at most head-on and 20 % from any of the tilted views)
                                if (sumOk < Coverage * w * hh || sum < BackgroundShare * w * hh || sumHead > HiddenHeadOn * w * hh || sumHid > HiddenTilted * w * hh) continue;
                                float cx = s0 + (x + w * 0.5f) * cs, cy = t0 + (y + hh * 0.5f) * cs;
                                spots.Add(((cx - target.x) * (cx - target.x) + (cy - target.y) * (cy - target.y), x, y));
                            }
                        foreach (var (d, x, y) in spots.OrderBy(z => z.d).Take(64))
                        {
                            float lo = float.MaxValue, hi = float.MinValue;
                            for (int yy = y; yy < y + hh; yy++) for (int xx = x; xx < x + w; xx++) { float v = h[yy * W + xx]; if (float.IsInfinity(v)) continue; lo = Mathf.Min(lo, v); hi = Mathf.Max(hi, v); }
                            if (hi - lo > Flatness) continue;
                            // the texture's square: the ink box (InkU / InkV) inside the margin of the box at (x, y)
                            float left = s0 + x * cs, top = t0 + (y + hh) * cs;
                            found = new Fit { size = size, texLeft = left + (BoxMargin - InkU0) * size, texTop = top - (BoxMargin - InkV0) * size, colour = mean,
                                              centre = o + e1 * (left + boxW * size * 0.5f) + e2 * (top - boxH * size * 0.5f) };
                            background = mean;
                            break;
                        }
                        if (found != null && Mathf.Approximately(found.size, size)) break;
                    }
                }
                return found;
            }
            // first unhidden (the panel alone): the size it could take; then with every other part of the bike in front of it
            var free0 = Search(cov, null, null, out _);
            free = free0?.size ?? 0;
            if (free0 == null)
            {
                if (Environment.GetEnvironmentVariable("REFLEX_MODPLATES_DEBUG") == "1")
                {
                    float lo = float.MaxValue, hi = float.MinValue; foreach (float v in h) if (!float.IsInfinity(v)) { lo = Mathf.Min(lo, v); hi = Mathf.Max(hi, v); }
                    Debug.Log($"[modplates] panel {panel.Count} tris, n {n.ToString("F2")}, {s1 - s0:0.00} x {t1 - t0:0.00} ft, {cov.Count(x => x)} cells, depth {hi - lo:0.000} ft, colours {string.Join(" ", counts.OrderByDescending(x => x.Value).Take(3).Select(x => x.Key + ":" + x.Value))}: nothing fits");
                }
                return null;
            }
            var front = new float[W * H]; for (int i = 0; i < front.Length; i++) front[i] = float.NegativeInfinity;
            var mine = new HashSet<(Vector3, Vector3, Vector3)>(tris.Select(t => (t.a, t.b, t.c)));
            var lo3 = o + e1 * s0 + e2 * t0; var box = new Bounds(o, Vector3.zero);
            foreach (var x in new[] { o + e1 * s0 + e2 * t0, o + e1 * s1 + e2 * t0, o + e1 * s0 + e2 * t1, o + e1 * s1 + e2 * t1 }) { box.Encapsulate(x); box.Encapsulate(x + n * 2f); }
            var straight = new float[W * H]; for (int i = 0; i < straight.Length; i++) straight[i] = float.NegativeInfinity;
            var frontBy = new int[W * H]; var own = new float[W * H]; for (int i = 0; i < own.Length; i++) own[i] = float.NegativeInfinity;
            var panelParts = new HashSet<Transform>(panel.Select(i => tk[cand[i].k].part));
            for (int oi = 0; oi < occluders.Count; oi++)
            {
                var oc = occluders[oi];
                if (!oc.b.Intersects(box)) continue;
                bool ownPart = panelParts.Contains(oc.part);   // (the plate's own surface: its lip and its other layers hide only what they cover head-on)
                var v = oc.v; var T = oc.t;
                for (int t = 0; t + 2 < T.Length; t += 3)
                {
                    Vector3 a = v[T[t]], b = v[T[t + 1]], c = v[T[t + 2]];
                    if (Vector3.Dot(a - o, n) < -0.05f && Vector3.Dot(b - o, n) < -0.05f && Vector3.Dot(c - o, n) < -0.05f) continue;   // (behind the panel)
                    if (mine.Contains((a, b, c))) continue;
                    if (ownPart) { Raster(a, b, c, (k, hh, wa, wb, wc) => { if (hh > own[k]) own[k] = hh; }); continue; }
                    // another part: seen straight, and 12 degrees from above and either side (the garage's and the cameras' angles)
                    foreach (var tilt in Tilts) Raster(a, b, c, (k, hh, wa, wb, wc) => { if (hh > front[k]) { front[k] = hh; frontBy[k] = oi; } }, tilt);
                    Raster(a, b, c, (k, hh, wa, wb, wc) => { if (hh > straight[k]) straight[k] = hh; });
                }
            }
            var hidden = new bool[W * H]; var hiddenHeadOn = new bool[W * H];
            for (int k = 0; k < hidden.Length; k++) { hidden[k] = cov[k] && front[k] > h[k] + 0.03f; hiddenHeadOn[k] = cov[k] && straight[k] > h[k] + 0.03f; }
            // a cable or a brake line across the plate (thinner than 0.07 ft) is no cover: the number sits behind it as on a real bike; an
            // opening of the masks (erode, then dilate, by 0.035 ft) keeps only the solid parts
            int rr = Mathf.Max(1, Mathf.RoundToInt(0.035f / cs));
            hidden = Opening(hidden, W, H, rr); hiddenHeadOn = Opening(hiddenHeadOn, W, H, rr);   // (a layer within 0.03 ft: the plate's own decal sheet or a twin of it, not a cover)
            var shown = new bool[W * H];
            for (int k = 0; k < shown.Length; k++) shown[k] = cov[k] && !(own[k] > h[k] + 0.005f);   // (the plate's own outer shell over it: an inner layer)
            var fit = Search(shown, hidden, hiddenHeadOn, out _);
            if (Environment.GetEnvironmentVariable("REFLEX_MODPLATES_DEBUG") == "1")
            {
                int nc = cov.Count(x => x), no = 0, nh = 0, nhh = 0; for (int k = 0; k < cov.Length; k++) { if (cov[k] && !shown[k]) no++; if (hidden[k]) nh++; if (hiddenHeadOn[k]) nhh++; }
                Debug.Log($"[modplates] panel {panel.Count} tris, {nc} cells: own layers over {no}, others over {nh} (head-on {nhh}); free {free:0.00} ft, fit {(fit != null ? fit.size.ToString("0.00") : "none")}");
            }
            if (fit == null)
            {   // (what hides it, for the log)
                var by = new Dictionary<int, int>(); for (int k = 0; k < hidden.Length; k++) if (hidden[k]) by[frontBy[k]] = by.TryGetValue(frontBy[k], out int c0) ? c0 + 1 : 1;
                hiddenBy = string.Join(", ", by.OrderByDescending(x => x.Value).Take(3).Select(x => occluders[x.Key].part.name + " " + x.Value));
                return null;
            }
            fit.panel = panel; fit.o = o; fit.n = n; fit.e1 = e1; fit.e2 = e2;
            return fit;
        }

        static readonly Vector2[] Tilts = { Vector2.zero, new Vector2(0, -0.213f), new Vector2(0.213f, 0), new Vector2(-0.213f, 0) };   // (tan 12 degrees)

        /// A morphological opening of a mask (a square of radius r): what is thinner than 2 r + 1 cells goes.
        static bool[] Opening(bool[] m, int W, int H, int r)
        {
            bool[] Pass(bool[] src, bool erode)
            {
                var row = new bool[src.Length]; var o = new bool[src.Length];
                for (int y = 0; y < H; y++)
                    for (int x = 0; x < W; x++)
                    {
                        bool v = erode;
                        for (int d = -r; d <= r; d++) { int xx = x + d; bool s = xx >= 0 && xx < W ? src[y * W + xx] : erode; if (erode ? !s : s) { v = !erode; break; } }
                        row[y * W + x] = v;
                    }
                for (int y = 0; y < H; y++)
                    for (int x = 0; x < W; x++)
                    {
                        bool v = erode;
                        for (int d = -r; d <= r; d++) { int yy = y + d; bool s = yy >= 0 && yy < H ? row[yy * W + x] : erode; if (erode ? !s : s) { v = !erode; break; } }
                        o[y * W + x] = v;
                    }
                return o;
            }
            return Pass(Pass(m, true), false);
        }

        static int Code(Vector3 c) => Mathf.Clamp((int)(c.x * 8), 0, 7) * 64 + Mathf.Clamp((int)(c.y * 8), 0, 7) * 8 + Mathf.Clamp((int)(c.z * 8), 0, 7);

        /// The stock chassis the mod re-skins (its database without the mod's namespace; the HR twin's normal model when the HR one is not
        /// installed): every LOD 0 triangle of a "Numbers" material whose second UV set covers the plate texture's middle, grouped by node and
        /// facing (in the chassis' model space, as the built meshes).
        static List<Projector> StockPlates(BikeSpec spec, Rfx.Install install, VehicleCatalog cat, List<(string db, string pkg)> chosen, out string source, out HashSet<string> plateBindings)
        {
            var result = new List<Projector>();
            plateBindings = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            string Lr(string s) => s != null && s.StartsWith("HR_", StringComparison.Ordinal) ? s.Substring(3) : s;
            string db = BikeMods.BaseDb(spec.chassisDb), pkg = spec.chassisPkg, model = spec.chassisModel;
            var stockPkg = install.Pkg(db, pkg);
            if (stockPkg == null || stockPkg.Find(model, "model") == null) { db = Lr(db); pkg = Lr(pkg); model = Lr(model); stockPkg = install.Pkg(db, pkg); }
            source = db + "/" + pkg + "/" + model;
            if (stockPkg == null || stockPkg.Find(model, "model") == null) return result;
            Loaded stock;
            try { stock = Load(install, db, pkg, model); }
            catch (InvalidDataException e) { Debug.LogWarning($"Reflex bike {spec.name}: the stock chassis {source}: {e.Message}"); return result; }
            bool hr = db.StartsWith("HR_", StringComparison.Ordinal);
            var pool = new List<(string db, string pkg)>();
            foreach (var c in chosen)
            {
                string cdb = BikeMods.BaseDb(c.db), cpkg = c.pkg;
                if (!hr && cdb.StartsWith("HR_", StringComparison.Ordinal)) { cdb = Lr(cdb); cpkg = Lr(cpkg); }
                if (install.Pkg(cdb, cpkg) != null) pool.Add((cdb, cpkg));
            }
            var numbers = new Dictionary<string, bool>(StringComparer.OrdinalIgnoreCase);
            foreach (var b in stock.model.Lod(0))
            {
                if (!numbers.TryGetValue(b.material, out bool plate))
                {
                    var def = GameMaterial(cat, b.material, (db, pkg), pool) ?? cat.Resolve(b.material, (db, pkg), pool);
                    numbers[b.material] = plate = def != null && def.textures.Any(t => string.Equals(t.name, "Numbers", StringComparison.OrdinalIgnoreCase));
                }
                if (!plate) continue;
                plateBindings.Add(b.material.StartsWith("HR_", StringComparison.OrdinalIgnoreCase) ? b.material.Substring(3) : b.material);
                var s = stock.surfaces[b.surface];
                var m = s.skinned ? Matrix4x4.identity : stock.model.nodes[b.node].world;
                string node = stock.model.nodes[b.node].name;
                var tri = s.Triangles();
                var mine = new List<Projector>();
                for (int i = 0; i + 2 < tri.Count; i += 3)
                {
                    Vector2 ua = s.uv1[tri[i]], ub = s.uv1[tri[i + 1]], uc = s.uv1[tri[i + 2]];
                    if (!(Finite(ua) && Finite(ub) && Finite(uc))) continue;
                    float area = Mathf.Abs((ub.x - ua.x) * (uc.y - ua.y) - (uc.x - ua.x) * (ub.y - ua.y));
                    if (area < 1e-6f) continue;   // (the rest of the plastics: the whole triangle on the texture's clear corner)
                    if (Mathf.Max(ua.x, Mathf.Max(ub.x, uc.x)) < 0.05f || Mathf.Min(ua.x, Mathf.Min(ub.x, uc.x)) > 0.95f
                        || Mathf.Max(ua.y, Mathf.Max(ub.y, uc.y)) < 0.05f || Mathf.Min(ua.y, Mathf.Min(ub.y, uc.y)) > 0.95f) continue;
                    Vector3 a = m.MultiplyPoint3x4(s.positions[tri[i]]), bb = m.MultiplyPoint3x4(s.positions[tri[i + 1]]), c = m.MultiplyPoint3x4(s.positions[tri[i + 2]]);
                    var cross = Vector3.Cross(bb - a, c - a); if (cross.sqrMagnitude < 1e-12f) continue;
                    // (the strip's winding is not the facing: the vertex normals say which side is out; |cross| = twice the area)
                    var vn = m.MultiplyVector(s.normals[tri[i]]) + m.MultiplyVector(s.normals[tri[i + 1]]) + m.MultiplyVector(s.normals[tri[i + 2]]);
                    var dir = Vector3.Dot(cross, vn) < 0 ? -cross : cross;
                    var pr = mine.FirstOrDefault(x => Vector3.Dot(x.normal.normalized, dir.normalized) > 0.6f);
                    if (pr == null) mine.Add(pr = new Projector { node = node, normal = Vector3.zero });
                    pr.normal += dir;   // (area weighted until normalised below)
                    pr.tris.Add((a, bb, c, dir.normalized, ua, ub, uc));
                }
                result.AddRange(mine);
            }
            foreach (var pr in result) pr.normal = pr.normal.normalized;
            return result;
        }

        /// Level 0 of a block-compressed install texture, sampled on the CPU (nearest texel; DXT1 / DXT3 / DXT5, A8R8G8B8).
        sealed class Texels
        {
            byte[] b; int at, w, h, block; uint fmt;
            static readonly Dictionary<string, Texels> cache = new Dictionary<string, Texels>(StringComparer.OrdinalIgnoreCase);
            public static Texels Of(Rfx.Install install, VehicleMaterialDef.Tex t)
            {
                if (t?.asset == null) return null;
                string key = t.asset.package.database.name + "/" + t.asset.package.name + "/" + t.asset.name;
                lock (cache) if (cache.TryGetValue(key, out var c)) return c;
                Texels r = null;
                try
                {
                    var b = install.Read(t.asset);
                    if (b.Length >= 64 && Rfx.U32(b, 0) == 0x77ddcc00u)
                    {
                        int levels = Rfx.I32(b, 40), w = Rfx.I32(b, 44), h = Rfx.I32(b, 48); uint fmt = Rfx.U32(b, 52);
                        int block = fmt == 0x31545844u ? 8 : fmt == 0x33545844u || fmt == 0x35545844u ? 16 : fmt == 21 ? 0 : -1;
                        int at = 56 + 8 * Math.Max(0, levels);
                        long need = block > 0 ? (long)((w + 3) / 4) * ((h + 3) / 4) * block : (long)w * h * 4;
                        if (block >= 0 && levels >= 1 && w >= 1 && h >= 1 && w <= 8192 && h <= 8192 && at + need <= b.Length) r = new Texels { b = b, at = at, w = w, h = h, block = block, fmt = fmt };
                    }
                }
                catch (Exception e) when (e is IOException || e is InvalidDataException) { }
                lock (cache) { if (cache.Count > 64) cache.Clear(); cache[key] = r; }
                return r;
            }
            /// The colour (0 .. 1) at a D3D UV (top origin, wrapped); null: not decodable.
            public Vector3? Rgb(float u, float v)
            {
                if (!float.IsFinite(u) || !float.IsFinite(v)) return null;
                int x = Mathf.Clamp((int)((u - Mathf.Floor(u)) * w), 0, w - 1), y = Mathf.Clamp((int)((v - Mathf.Floor(v)) * h), 0, h - 1);
                float r, g, bl;
                if (block == 0) { int o = at + (y * w + x) * 4; bl = b[o] / 255f; g = b[o + 1] / 255f; r = b[o + 2] / 255f; }
                else
                {
                    int o = at + ((y / 4) * ((w + 3) / 4) + x / 4) * block + (block == 16 ? 8 : 0);
                    int c0 = b[o] | b[o + 1] << 8, c1 = b[o + 2] | b[o + 3] << 8;
                    int code = (int)((BitConverter.ToUInt32(b, o + 4) >> (2 * ((y & 3) * 4 + (x & 3)))) & 3);
                    Vector3 C(int c) => new Vector3(((c >> 11) & 31) / 31f, ((c >> 5) & 63) / 63f, (c & 31) / 31f);
                    Vector3 a = C(c0), d = C(c1);
                    if (block == 8 && c0 <= c1 && code == 3) return Vector3.zero;   // (DXT1's transparent texel: black, the programs draw it opaque)
                    Vector3 col = code == 0 ? a : code == 1 ? d : c0 > c1 || block == 16 ? (code == 2 ? (2 * a + d) / 3 : (a + 2 * d) / 3) : code == 2 ? (a + d) / 2 : Vector3.zero;
                    r = col.x; g = col.y; bl = col.z;
                }
                return new Vector3(r, g, bl);
            }
        }

        static bool Finite(Vector2 v) => float.IsFinite(v.x) && float.IsFinite(v.y);

        /// Whether a material's texture is fully transparent (the mods' "NoPlate" decal: alpha 0 in every texel of its first level).
        static bool Transparent(Rfx.Install install, VehicleMaterialDef.Tex t)
        {
            if (t?.asset == null) return false;
            byte[] b;
            try { b = install.Read(t.asset); } catch (Exception e) when (e is IOException || e is InvalidDataException) { return false; }
            if (b.Length < 64 || Rfx.U32(b, 0) != 0x77ddcc00u) return false;
            int levels = Rfx.I32(b, 40), w = Rfx.I32(b, 44), h = Rfx.I32(b, 48); uint fmt = Rfx.U32(b, 52);
            if (levels < 1 || levels > 14 || w < 1 || h < 1 || w > 4096 || h > 4096) return false;
            int o = 56 + 8 * levels, size = Rfx.I32(b, 56);
            if (size <= 0 || o + size > b.Length) return false;
            if (fmt == 0x35545844u)   // DXT5: the interpolated alpha of each 4 x 4 block
            {
                for (int k = o; k + 16 <= o + size; k += 16)
                {
                    int a0 = b[k], a1 = b[k + 1];
                    ulong bits = 0; for (int j = 0; j < 6; j++) bits |= (ulong)b[k + 2 + j] << (8 * j);
                    for (int i = 0; i < 16; i++)
                    {
                        int code = (int)((bits >> (3 * i)) & 7);
                        int a = code == 0 ? a0 : code == 1 ? a1 : a0 > a1 ? ((8 - code) * a0 + (code - 1) * a1) / 7 : code == 6 ? 0 : code == 7 ? 255 : ((6 - code) * a0 + (code - 1) * a1) / 5;
                        if (a != 0) return false;
                    }
                }
                return true;
            }
            if (fmt == 0x33545844u)   // DXT3: explicit 4-bit alpha
            {
                for (int k = o; k + 16 <= o + size; k += 16) for (int j = 0; j < 8; j++) if (b[k + j] != 0) return false;
                return true;
            }
            if (fmt == 21)   // A8R8G8B8
            {
                for (int k = o + 3; k < o + size; k += 4) if (b[k] != 0) return false;
                return true;
            }
            return false;   // (DXT1 and the rest: not looked at)
        }
    }
}

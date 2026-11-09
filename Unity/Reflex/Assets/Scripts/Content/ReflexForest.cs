// The level's forest (ForestSystemRSData, 24 levels): SpeedTree trees placed by the level's "forest" asset, drawn by the original
// as 3D trees near the camera and as instanced 360-degree billboards further away. Read at run time from the install (nothing is
// cached in the project); notes: _re/notes/forest.md, names: _re/names/92_forest.tsv.
//
// Formats (all from the level database):
//   forest  BXML ForestAsset (XenonForestAsset::vf3 0065cd90): BillboardAtlas (diffuse, normal), TreeTypes / Type (TreeAsset,
//           Branch / Frond / Leaf / 3DLeaf material, collision and audio, Shadow (texture, invProj), Billboard (width, height, atlas
//           row: uStartOffset, vStartOffset, uScale, vScale, uStep, count)), Clusters (a 20 x 20 grid) / Cluster (bounding box) /
//           TreesOfType (Positions, Rotations, Scales: per tree position, angle, (?, scale))
//   tree    'EERT' version 1 (XenonTreeAsset FUN_00672030): a 0x2ec-byte header (+8 BXML size, then 4 geometry blocks of 0xb8 bytes
//           at +0xc: +0 index bytes, +4 vertex bytes, +0xc stride, +0xe LOD count, +0x10 10 LOD starts, +0x24 10 LOD counts, +0x38
//           the D3DVERTEXELEMENT9 declaration), a BXML (Lods: near, far and the SpeedTree LOD parameters, BoundingBox,
//           ShadowFitPoints, the SPT source), then per block the vertex data and (branch / frond / 3D leaf) 16-bit strip indices.
//           Blocks: 0 branches (strips), 1 fronds (strips), 2 leaves (triangle list, camera-facing cards), 3 3D leaves (unused).
//
// Every number the original derives from these is computed here as the game does (FUN_0065dd30 per type, FUN_0065eb00 the LOD
// records with leafLodMethod 1, the per-tree record of XenonForestAsset::vf3 with its rotation quantised to 45 degrees and its
// sin / cos polynomials) and was checked float for float against the running game's memory (NAT_Concord, FR_Alpine, NAT_Renne,
// TRX_Verde world images: 29 tree types, 25,277 trees, 0 differing floats; ForestTests.Verify repeats it in the bench).
using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Linq;
using UnityEngine;
using UnityEngine.Rendering;
using Node = Reflex.Port.Content.BxmlData.Node;

namespace Reflex.Port.Content
{
    /// One LOD record of a tree type (type + 0x60 + k * 0x70): the 3D tree is drawn with this record while its view depth is below
    /// <see cref="dist"/> (the first record whose distance is larger, FUN_006676d0).
    public sealed class ForestRecord
    {
        public float dist;                                      // +0x74
        public int branchLod, frondLod, leafLod, leaf3dLod;    // +0x60..+0x6c (leafLod = leaf record index >> 1 for leafLodMethod 1)
        public bool transition;                                 // +0x70: draws the next leaf LOD as well (alpha test w)
        public readonly float[] leaf = new float[8];           // +0x80: g_TreeAlphaTests.z = pow(sat(sat(d f0 + f1) f2 + f3), f7) f5 + f4
        public readonly float[] leaf3d = new float[8];         // +0xa0: the same for .w (the next leaf LOD fading in)
    }

    /// One geometry block of a tree asset, decoded (half floats) and ready for a Unity mesh: one submesh per LOD.
    public sealed class ForestGeometry
    {
        public int block, lods, stride;
        public Vector3[] positions, normals; public Vector4[] tangents, uv0, uv1;
        public Vector2[] uv2;                                   // x: NORMAL.w (Alive trees: SpeedTree's ambient occlusion; 1 otherwise)
        public int[][] lodTriangles;                            // per LOD: triangle list (strips converted)
        public int[] lodStarts, lodCounts;                      // the asset's tables (index or vertex units)
        public bool HasLod(int lod) => lod >= 0 && lod < lods && lodTriangles[lod] != null && lodTriangles[lod].Length > 0;
        /// The mesh's submeshes: one per LOD, or (MX vs ATV Alive trees with a Surfaces node) one per surface of a LOD, each with its own
        /// material (`partMaterial`, null: the block's material). `lodParts[lod]`: the submeshes of that LOD.
        public int[][] partTriangles; public string[] partMaterial; public int[][] lodParts;
    }

    public sealed class ForestTreeType
    {
        public int index;
        public string treeAsset, branchMaterial, frondMaterial, leafMaterial, leaf3dMaterial;
        public bool loaded;
        public readonly ForestGeometry[] geometry = new ForestGeometry[4];   // branch, frond, leaf cards, 3D leaves (empty in every Reflex asset; MX vs ATV Alive's palms / ferns use it)
        public Dictionary<string, object> lods;                            // the tree's Lods attributes
        public float near, far;
        public Vector3 boundsCenter, boundsHalf;                           // BoundingBox (asset +0x860 / +0x870)
        public int branchLods, frondLods, leafLods, leaf3dLods;            // asset +0x880, +0x8bc, +0x8f8, +0x930
        /// MX vs ATV Alive (the converter's Surfaces node, alive_environment.tree): the surfaces of a LOD that draws more than one material,
        /// (PC block, LOD, material, start, count in the block's converted units). Empty for every other tree.
        public readonly List<(int block, int lod, string material, int start, int count)> surfaces = new List<(int block, int lod, string material, int start, int count)>();
        // per type (FUN_0065dd30, type struct 0x1d030 + t * 0x5f0)
        public float bbStart;                                               // +0x14: far - (far - near) 2 (1 - leafTransitionFactor) radius
        public readonly float[] branchParams = new float[8];                // +0x20: gTreeLODParams1 / 2 of the branches
        public readonly float[] frondParams = new float[8];                 // +0x40: of the fronds
        public readonly List<ForestRecord> records = new List<ForestRecord>();  // +0x60 (count +0x4c0)
        public readonly float[] billboard = new float[12];                 // g_BBTreeTypes rows (forest + 0x246f0 + t * 0x30)
        public float MaxDistance => records.Count > 0 ? records[records.Count - 1].dist : 0f;
    }

    public sealed class ForestData
    {
        public string name, atlasDiffuse, atlasNormal;
        public readonly List<ForestTreeType> types = new List<ForestTreeType>();
        /// The tree records as the game builds them (48 bytes, here 8 floats each): x, y, z, type + 0.5, scale, rotation (quantised),
        /// sin, cos. Order: cluster by cluster, type by type (the billboard instance stream).
        public float[] trees = Array.Empty<float>();
        public int treeCount;
        public int[] clusterStart = Array.Empty<int>(), clusterCount = Array.Empty<int>();
        public Vector3[] clusterCenter = Array.Empty<Vector3>(), clusterHalf = Array.Empty<Vector3>();
        public int clustersWide, clustersHigh;
        public Vector3 totalCenter, totalHalf;
        /// ForestSystem +0x3074 / +0x3078 (FUN_006626c0): the smallest billboard start and the largest 3D distance of all types.
        public float minBillboardStart, max3dDistance;
    }

    public sealed partial class ReflexLevelContent
    {
        /// The level's forest (null when the scene has no ForestSystemRSData or its asset is missing).
        public ForestData forest;
        /// The level's grass (GrassSystemRSData: NAT_Renne, NAT_RenneB), or null.
        public GrassData grass;
        internal readonly ConcurrentDictionary<string, LevelMaterial> forestMaterials = new ConcurrentDictionary<string, LevelMaterial>(StringComparer.Ordinal);
        string forestAsset;

        void StageForest(int stage, List<Action> jobs)
        {
            if (stage == 1)
            {
                var ge = entities.FirstOrDefault(e => e.Type == "GrassSystemRSData" && !string.IsNullOrEmpty(e.Asset("GrassAsset")));
                if (ge != null)
                    jobs.Add(() =>
                    {
                        try { grass = ParseGrass(ge.Asset("GrassAsset"), ReadAsset(ge.Asset("GrassAsset"), "grass"), ge); }
                        catch (Exception ex) { Warn($"grass {ge.Asset("GrassAsset")}: {ex.Message}"); grass = null; }
                    });
                forestAsset = entities.FirstOrDefault(e => e.Type == "ForestSystemRSData" && !string.IsNullOrEmpty(e.Asset("ForestAsset")))?.Asset("ForestAsset");
                if (forestAsset == null) return;
                jobs.Add(() =>
                {
                    try { forest = ParseForest(forestAsset, ReadAsset(forestAsset, "forest")); }
                    catch (Exception ex) { Warn($"forest {forestAsset}: {ex.Message}"); forest = null; }
                });
            }
            else if (stage == 2)
            {
                if (forest == null) return;
                var f = forest;
                foreach (var t in f.types)
                {
                    var tt = t;
                    jobs.Add(() =>
                    {
                        try { ParseTree(tt, ReadAsset(tt.treeAsset, "tree"), f.types.Count); }
                        catch (Exception ex) { Warn($"tree {tt.treeAsset}: {ex.Message}"); tt.loaded = false; }
                    });
                }
                foreach (var name in f.types.SelectMany(t => new[] { t.branchMaterial, t.frondMaterial, t.leafMaterial }).Where(n => !string.IsNullOrEmpty(n)).Distinct())
                {
                    string n = name;
                    jobs.Add(() =>
                    {
                        try { forestMaterials[n] = LoadMaterial(n); }
                        catch (Exception ex) { Warn($"tree material {n}: {ex.Message}"); }
                    });
                }
            }
            else if (stage == 3)
            {
                if (grass != null)
                    foreach (var name in new[] { grass.atlasTexture, grass.colorTexture }.Where(t => !string.IsNullOrEmpty(t)))
                    {
                        string n = name;
                        jobs.Add(() =>
                        {
                            try { var t = ReflexTex.Parse(n, ReadAsset(n, "texture")); if (t != null) sceneTextures[n] = t; else Warn($"grass texture {n}: unsupported format"); }
                            catch (Exception ex) { Warn($"grass texture {n}: {ex.Message}"); }
                        });
                    }
                if (forest == null) return;
                FinishForest(forest);
                // the surface materials of Alive's multi-material trees (named by the trees, parsed in stage 2)
                foreach (var mn in forest.types.Where(t => t.loaded).SelectMany(t => t.surfaces.Select(q => q.material)).Where(q => !string.IsNullOrEmpty(q)).Distinct().ToList())
                {
                    if (forestMaterials.ContainsKey(mn)) continue;
                    try { forestMaterials[mn] = LoadMaterial(mn); }
                    catch (Exception ex) { Warn($"tree material {mn}: {ex.Message}"); }
                }
                // textures of the tree materials and the billboard atlas (names the scenery stage does not load itself)
                var scenery = new HashSet<string>(sceneMaterials.Values.SelectMany(m => m.textures).Concat(instances.Select(i => i.adTexture)).Where(t => !string.IsNullOrEmpty(t)));
                var names = forestMaterials.Values.SelectMany(m => m.textures).Concat(new[] { forest.atlasDiffuse, forest.atlasNormal })
                    .Where(t => !string.IsNullOrEmpty(t) && !scenery.Contains(t)).Distinct().ToList();
                foreach (var name in names)
                {
                    string n = name;
                    jobs.Add(() =>
                    {
                        try { var t = ReflexTex.Parse(n, ReadAsset(n, "texture")); if (t != null) sceneTextures[n] = t; else Warn($"texture {n}: unsupported format"); }
                        catch (Exception ex) { Warn($"texture {n}: {ex.Message}"); }
                    });
                }
            }
        }

        // ------------------------------------------------------------------------------------------------ forest asset

        static object Attr(Node n, string key) => n != null && n.attributes.TryGetValue(key, out var v) ? v : null;
        static float AttrF(Node n, string key, float def = 0) { var v = Attr(n, key); return v is float f ? f : v is int i ? i : v is uint u ? u : def; }
        static int AttrI(Node n, string key, int def = 0) { var v = Attr(n, key); return v is int i ? i : v is uint u ? (int)u : v is float f ? (int)f : def; }
        static Vector3 AttrV(Node n, string key) { var a = Attr(n, key) as float[]; return a != null && a.Length >= 3 ? new Vector3(a[0], a[1], a[2]) : Vector3.zero; }
        static float[] Floats(object v) => v is float[] a ? a : v is float f ? new[] { f } : Array.Empty<float>();

        internal static ForestData ParseForest(string name, byte[] b)
        {
            var root = Rfx.Bxml(b);
            Rfx.Check(root.tag == "ForestAsset", "forest root");
            var f = new ForestData { name = name };
            var atlas = root.Child("BillboardAtlas");
            f.atlasDiffuse = atlas?.S("diffuse"); f.atlasNormal = atlas?.S("normal");
            var types = root.Child("TreeTypes");
            Rfx.Check(types != null, "TreeTypes");
            foreach (var t in types.children.Where(c => c.tag == "Type"))
            {
                var tt = new ForestTreeType
                {
                    index = f.types.Count, treeAsset = t.S("TreeAsset"), branchMaterial = t.S("BranchMaterial"), frondMaterial = t.S("FrondMaterial"),
                    leafMaterial = t.S("LeafMaterial"), leaf3dMaterial = t.S("3DLeafMaterial"),
                };
                // g_BBTreeTypes rows 0 / 1 (FUN_0065dd30: the Billboard node; row 2 needs the tree's LODs, FinishForest)
                var bb = t.Child("Billboard");
                tt.billboard[0] = AttrF(bb, "width"); tt.billboard[1] = AttrF(bb, "height");
                tt.billboard[4] = AttrF(bb, "uStartOffset"); tt.billboard[5] = AttrF(bb, "vStartOffset");
                tt.billboard[6] = AttrF(bb, "uScale"); tt.billboard[7] = AttrF(bb, "vScale");
                tt.billboard[3] = bb != null && Attr(bb, "uStep") != null ? AttrF(bb, "uStep") : tt.billboard[6];
                int count = AttrI(bb, "count", 1);
                tt.billboard[10] = count; tt.billboard[11] = 1f / count;
                f.types.Add(tt);
            }
            Rfx.Check(f.types.Count > 0 && f.types.Count <= 20, "tree type count");   // 60 constant registers c80.. (3 per type)
            var clusters = root.Child("Clusters");
            Rfx.Check(clusters != null, "Clusters");
            f.clustersWide = AttrI(clusters, "numClustersWide"); f.clustersHigh = AttrI(clusters, "numClustersHigh");
            f.totalCenter = AttrV(clusters, "totalBBCenter"); f.totalHalf = AttrV(clusters, "totalBBHalf");
            var cl = clusters.children.Where(c => c.tag == "Cluster").ToList();
            int total = AttrI(clusters, "totalNumTrees");
            Rfx.Check(total >= 0 && total <= 200000, "tree count");
            var trees = new List<float>(total * 8);
            f.clusterStart = new int[cl.Count]; f.clusterCount = new int[cl.Count]; f.clusterCenter = new Vector3[cl.Count]; f.clusterHalf = new Vector3[cl.Count];
            for (int c = 0; c < cl.Count; c++)
            {
                var n = cl[c];
                f.clusterStart[c] = trees.Count / 8;
                f.clusterCenter[c] = AttrV(n, "boundingBoxCenter"); f.clusterHalf[c] = AttrV(n, "boundingBoxHalf");
                foreach (var to in n.children.Where(x => x.tag == "TreesOfType"))
                {
                    int type = AttrI(to, "type"), nt = AttrI(to, "numTrees");
                    if (nt <= 0) continue;
                    Rfx.Check(type >= 0 && type < f.types.Count, "tree type index");
                    var pos = Floats(to.Child("Positions")?.value); var rot = Floats(to.Child("Rotations")?.value); var scl = Floats(to.Child("Scales")?.value);
                    Rfx.Check(pos.Length >= nt * 3 && rot.Length >= nt && scl.Length >= nt * 2, "tree arrays");
                    for (int i = 0; i < nt; i++)
                    {
                        float q = QuantiseRotation(rot[i]);
                        TreeSinCos(q, out float sn, out float cs);
                        trees.Add(pos[i * 3]); trees.Add(pos[i * 3 + 1]); trees.Add(pos[i * 3 + 2]);
                        trees.Add(type + 0.5f); trees.Add(scl[i * 2 + 1]); trees.Add(q); trees.Add(sn); trees.Add(cs);
                    }
                }
                f.clusterCount[c] = trees.Count / 8 - f.clusterStart[c];
            }
            f.trees = trees.ToArray(); f.treeCount = f.trees.Length / 8;
            return f;
        }

        // Single-precision arithmetic as the game's SSE code does it: every operation rounded to float. (float) casts force that rounding
        // (Mono may keep float expressions at double precision; double then float is exact for one +, -, *, / of two floats.)
        static float M(float a, float b) => (float)((double)a * b);
        static float A(float a, float b) => (float)((double)a + b);
        static float S(float a, float b) => (float)((double)a - b);
        static float D(float a, float b) => (float)((double)a / b);

        // XenonForestAsset::vf3 0065daa4..0065db08: x = rot * 4 / pi (0x977da4), + 8 while negative, floor (the 2^23 rounding trick with
        // the cmpnless correction), * pi / 4 (0x9776e0)
        public static float QuantiseRotation(float rot)
        {
            float x = M(rot, 1.2732395f);               // 0x3fa2f983
            while (0f > x) x = A(x, 8f);
            float m = S(A(x, 8388608f), 8388608f);      // round to nearest (x >= 0 here)
            if (S(m, x) > 0f) m = S(m, 1f);
            return M(m, 0.7853982f);                    // 0x3f490fdb
        }

        // 0065db11..0065dc20: the game's own sin / cos polynomials (single precision, this operation order), stored at +0x18 / +0x1c
        public static void TreeSinCos(float r, out float sn, out float cs)
        {
            float sg = r >= 0f ? 1f : -1f;
            r = M(r, sg);
            float q = (int)M(r, 0.15915494f);           // cvttss2si
            float a = S(3.1415927f, S(r, M(q, 6.2831855f)));
            float x = M(a, a), x2 = M(x, x);
            float c = A(M(x, -2.197296e-07f), 2.420294e-05f); c = M(c, x2); c = A(c, A(M(x, -0.001385879f), 0.04165978f)); c = M(c, x2); c = A(c, A(M(x, -0.4999942f), 0.9999992f));
            cs = S(-0f, c);
            float s = A(M(x, -2.036224e-08f), 2.699715e-06f); s = M(s, x2);
            float t = A(M(x, -0.0001980863f), 0.008332403f), x1 = A(M(x, -0.1666655f), 0.9999996f);
            s = A(s, t); s = M(s, x2); s = A(s, x1); s = M(s, a); s = M(s, sg);
            sn = s;
        }

        // ------------------------------------------------------------------------------------------------ tree asset

        static readonly int[] DeclSizes = { 4, 8, 12, 16, 4, 4, 4, 8, 4, 4, 8, 4, 8, 4, 4, 4, 8 };   // D3DDECLTYPE byte sizes (0..16)

        internal static void ParseTree(ForestTreeType t, byte[] b, int numTypes)
        {
            Rfx.Check(b.Length > 0x2ec && Rfx.U32(b, 0) == 0x54524545 && Rfx.I32(b, 4) == 1, "tree header");   // 'EERT' v1
            int xmlSize = Rfx.I32(b, 8);
            Rfx.Check(xmlSize > 32 && 0x2ec + xmlSize <= b.Length, "tree BXML size");
            var xml = new byte[xmlSize]; Buffer.BlockCopy(b, 0x2ec, xml, 0, xmlSize);
            var root = Rfx.Bxml(xml);
            var lods = root.Child("Lods"); Rfx.Check(lods != null, "tree Lods");
            t.lods = lods.attributes;
            t.near = AttrF(lods, "near"); t.far = AttrF(lods, "far");
            var box = root.Child("BoundingBox");
            t.boundsCenter = AttrV(box, "center"); t.boundsHalf = AttrV(box, "half");
            t.surfaces.Clear();
            var surf = root.Child("Surfaces");
            if (surf != null)
                foreach (var sn in surf.children.Where(c => c.tag == "Surface"))
                {
                    int sb = AttrI(sn, "block"), sl = AttrI(sn, "lod"), ss = AttrI(sn, "start"), sc = AttrI(sn, "count");
                    Rfx.Check(sb >= 0 && sb < 4 && sl >= 0 && sl < 10 && ss >= 0 && sc >= 0, "tree surface");
                    t.surfaces.Add((sb, sl, sn.S("material") ?? "", ss, sc));
                }
            int p = 0x2ec + xmlSize;
            var counts = new int[4];
            for (int k = 0; k < 4; k++)
            {
                int h = 0xc + k * 0xb8;
                int ib = Rfx.I32(b, h), vb = Rfx.I32(b, h + 4), stride = BitConverter.ToUInt16(b, h + 0xc), nl = BitConverter.ToUInt16(b, h + 0xe);
                Rfx.Check(nl <= 10 && stride <= 256 && vb >= 0 && ib >= 0, "tree block header");
                // FUN_00672030: a block with no LODs or no vertices is skipped (its LOD count becomes 0); the branch block reads its
                // indices only when it has some, fronds and 3D leaves always, leaves have none
                if (nl == 0 || vb == 0) { counts[k] = 0; continue; }
                counts[k] = nl;
                Rfx.Check(p + vb <= b.Length, "tree vertex data");
                int vbAt = p; p += vb;
                int ibAt = -1, ibBytes = 0;
                if (k == 1 || k == 3 || (k == 0 && ib != 0)) { Rfx.Check(p + ib <= b.Length, "tree index data"); ibAt = p; ibBytes = ib; p += ib; }
                // 3D leaves (block 3): no Reflex asset has them; Alive's (palm / fern leaf meshes, the converter's strips) draw like fronds
                t.geometry[k] = DecodeBlock(b, h, k, nl, stride, vbAt, vb, ibAt, ibBytes, t.surfaces.Where(q => q.block == k).ToList());
            }
            Rfx.Check(p == b.Length, "tree asset size");
            t.branchLods = counts[0]; t.frondLods = counts[1]; t.leafLods = counts[2]; t.leaf3dLods = counts[3];
            ComputeTypeConstants(t);
            t.loaded = true;
        }

        static float Half(byte[] b, int p)
        {
            int h = b[p] | (b[p + 1] << 8);
            int s = (h >> 15) & 1, e = (h >> 10) & 31, m = h & 1023;
            float v = e == 0 ? m * (1f / 16777216f) : e == 31 ? (m == 0 ? float.PositiveInfinity : float.NaN) : (1f + m / 1024f) * Mathf.Pow(2f, e - 15);
            return s != 0 ? -v : v;
        }

        static ForestGeometry DecodeBlock(byte[] b, int h, int block, int lods, int stride, int vbAt, int vbBytes, int ibAt, int ibBytes,
                                          List<(int block, int lod, string material, int start, int count)> surfaces = null)
        {
            int n = vbBytes / stride;
            Rfx.Check(n * stride == vbBytes && n < 65536, "tree vertex count");
            var g = new ForestGeometry { block = block, lods = lods, stride = stride, positions = new Vector3[n], normals = new Vector3[n], tangents = new Vector4[n], uv0 = new Vector4[n], uv1 = new Vector4[n], uv2 = new Vector2[n] };
            var corner = new Vector4[n]; var pos = new Vector4[n];
            for (int i = 0; i < n; i++) { g.uv2[i] = new Vector2(1, 0); g.uv0[i] = new Vector4(0, 0, 0, 1); corner[i] = new Vector4(0, 0, 0, 1); pos[i] = new Vector4(0, 0, 0, 1); g.tangents[i] = new Vector4(1, 0, 0, 1); }
            for (int e = 0; e < 16; e++)
            {
                int d = h + 0x38 + e * 8;
                int streamNo = BitConverter.ToUInt16(b, d), off = BitConverter.ToUInt16(b, d + 2), type = b[d + 4], usage = b[d + 6], usageIndex = b[d + 7];
                if (streamNo == 0xff) break;
                Rfx.Check(streamNo == 0 && (type == 15 || type == 16 || (type >= 0 && type <= 3)) && off + DeclSizes[type] <= stride, "tree vertex declaration");
                int comps = type == 15 ? 2 : type == 16 ? 4 : type + 1; bool half = type >= 15;
                for (int i = 0; i < n; i++)
                {
                    int at = vbAt + i * stride + off;
                    var v = new Vector4(0, 0, 0, 1);
                    for (int c = 0; c < comps; c++) v[c] = half ? Half(b, at + c * 2) : BitConverter.ToSingle(b, at + c * 4);
                    switch (usage)
                    {
                        case 0: pos[i] = v; break;                                             // POSITION (w: the vertex's LOD value)
                        case 3: g.normals[i] = new Vector3(v.x, v.y, v.z); g.uv2[i] = new Vector2(comps >= 4 ? v.w : 1f, 0); break;   // NORMAL
                        case 6: g.tangents[i] = v; break;                                       // TANGENT
                        case 5:
                            if (usageIndex == 0) g.uv0[i] = v;                                  // TEXCOORD0
                            else if (usageIndex == 4) corner[i] = v;                            // TEXCOORD4: the leaf card corner
                            break;
                    }
                }
            }
            for (int i = 0; i < n; i++)
            {
                g.positions[i] = new Vector3(pos[i].x, pos[i].y, pos[i].z);
                var u = g.uv0[i]; g.uv0[i] = new Vector4(u.x, 1f - u.y, u.z, 1f - u.w);        // Unity's texture row order (v flipped)
                g.uv1[i] = block == 2 ? new Vector4(corner[i].x, corner[i].y, corner[i].z, pos[i].w) : new Vector4(pos[i].w, 0, 0, 0);
            }
            g.lodStarts = new int[lods]; g.lodCounts = new int[lods]; g.lodTriangles = new int[lods][];
            int[] Triangles(int start, int count)
            {
                if (count == 0) return Array.Empty<int>();
                var tri = new List<int>();
                if (block == 2)
                {
                    // DrawPrimitive(TRIANGLELIST, start, count / 3) (FUN_00672aa0)
                    for (int i = 0; i + 2 < count; i += 3) { tri.Add(start + i); tri.Add(start + i + 1); tri.Add(start + i + 2); }
                }
                else
                {
                    if (ibAt < 0) return Array.Empty<int>();   // no index buffer: the original draws nothing
                    Rfx.Check((start + count) * 2 <= ibBytes, "tree LOD index range");
                    // DrawIndexedPrimitive(TRIANGLESTRIP, 0, 0, count, start, count - 2): odd triangles reversed, degenerate ones dropped
                    for (int i = 0; i + 2 < count; i++)
                    {
                        int a = BitConverter.ToUInt16(b, ibAt + (start + i) * 2), c1 = BitConverter.ToUInt16(b, ibAt + (start + i + 1) * 2), c2 = BitConverter.ToUInt16(b, ibAt + (start + i + 2) * 2);
                        if (a == c1 || a == c2 || c1 == c2) continue;
                        if ((i & 1) == 0) { tri.Add(a); tri.Add(c1); tri.Add(c2); } else { tri.Add(c1); tri.Add(a); tri.Add(c2); }
                    }
                }
                foreach (int v in tri) Rfx.Check(v < n, "tree index");
                return tri.ToArray();
            }
            for (int l = 0; l < lods; l++)
            {
                int start = BitConverter.ToUInt16(b, h + 0x10 + l * 2), count = BitConverter.ToUInt16(b, h + 0x24 + l * 2);
                g.lodStarts[l] = start; g.lodCounts[l] = count;
                g.lodTriangles[l] = Triangles(start, count);
            }
            // submeshes: one per LOD, or one per surface (Alive's multi-material LODs; a LOD without surfaces keeps its whole range)
            var parts = new List<int[]>(); var partMat = new List<string>(); g.lodParts = new int[lods][];
            for (int l = 0; l < lods; l++)
            {
                var ls = surfaces?.Where(q => q.lod == l).ToList();
                if (ls == null || ls.Count == 0) { g.lodParts[l] = new[] { parts.Count }; parts.Add(g.lodTriangles[l]); partMat.Add(null); continue; }
                var ids = new List<int>();
                foreach (var q in ls)
                {
                    Rfx.Check(q.start >= g.lodStarts[l] && q.start + q.count <= g.lodStarts[l] + g.lodCounts[l], "tree surface range");
                    ids.Add(parts.Count); parts.Add(Triangles(q.start, q.count)); partMat.Add(string.IsNullOrEmpty(q.material) ? null : q.material);
                }
                g.lodParts[l] = ids.ToArray();
            }
            g.partTriangles = parts.ToArray(); g.partMaterial = partMat.ToArray();
            return g;
        }

        // ------------------------------------------------------------------------------------------------ the per-type constants

        const float C329 = 0.329411775f;   // 0x3ea8a8a9 (84 / 255, targetAlphaValue)
        const float C671 = 0.670588255f;   // 0x3f2babac

        static float L(ForestTreeType t, string key) { var v = t.lods != null && t.lods.TryGetValue(key, out var o) ? o : null; return v is float f ? f : v is int i ? i : v is uint u ? u : 0f; }

        /// FUN_0065dd30 (with the tree's LODs) and FUN_0065eb00: the type's billboard start, branch / frond fade constants and LOD records.
        internal static void ComputeTypeConstants(ForestTreeType t)
        {
            float near = t.near, far = t.far, R = S(far, near);
            float tf = L(t, "transitionFactor"), radius = L(t, "leafLodTransitionRadius"), expo = L(t, "leafLodCurveExponent"), ltf = L(t, "leafTransitionFactor");
            float k = M(M(S(1f, ltf), radius), 2f);
            t.bbStart = S(far, M(R, k));
            float s = D(1.5f, M(R, k));
            t.billboard[2] = L(t, "transitionPercent");
            t.billboard[8] = s; t.billboard[9] = S(1.5f, M(far, s));
            FadeParams(t.branchParams, L(t, "branchFadeDistance"), near, far, tf, radius, expo);
            FadeParams(t.frondParams, L(t, "frondFadeDistance"), near, far, tf, radius, expo);
            BuildRecords(t);
        }

        static void FadeParams(float[] o, float fd, float near, float far, float tf, float radius, float expo)
        {
            if (fd <= 0f) { o[0] = 0; o[1] = 0.6666667f; o[2] = 0; o[3] = 1; o[4] = 0; o[5] = 0; o[6] = 0; o[7] = C329; return; }
            float R = S(far, near);
            float f = D(1f, M(R, fd)), f2 = D(1f, M(M(M(S(1f, tf), R), radius), 2f));
            o[0] = M(f, -0.6666667f); o[1] = M(M(far, f), 0.6666667f); o[2] = f2; o[3] = S(1f, M(far, f2));
            o[4] = D(-0.6666667f, fd); o[5] = expo; o[6] = C671; o[7] = C329;
        }

        static List<float> Thresholds(int n)
        {
            var a = new List<float>();
            if (n > 0) { float inv = (float)(1.0 / n), x = inv; for (int i = 0; i < n - 1; i++) { a.Add(x); x = A(x, inv); } }
            a.Add(1f);
            return a;
        }

        /// FUN_0065eb00 (leafLodMethod 1, the method of every asset): the leaf LOD transition records merged with the branch / frond
        /// LOD thresholds (i / n) into at most 10 records.
        static void BuildRecords(ForestTreeType t)
        {
            float near = t.near, far = t.far;
            float tw = L(t, "transitionWidth"), cl = L(t, "cycleLengthBB"), crest = L(t, "crestWidthBB");    // +0x850, +0x848, +0x840
            float tf = L(t, "transitionFactor"), expo = L(t, "leafLodCurveExponent"), ltf = L(t, "leafTransitionFactor"), radius = L(t, "leafLodTransitionRadius");
            int nb = t.branchLods, nf = t.frondLods, nl = t.leafLods, n3 = t.leaf3dLods;
            var Ab = Thresholds(nb); var Bf = Thresholds(nf); var Dl = Thresholds(n3);
            var P = new float[10][]; var Q = new float[10][]; var F = new int[11]; var C = new float[12];
            for (int i = 0; i < 10; i++) { P[i] = new[] { 1f, 0f, 1f, 0f, C329, C329, 0f, 1f }; Q[i] = (float[])P[i].Clone(); }
            int cmax = 0;
            if (nl > 0)
            {
                Rfx.Check(nl <= 5, "leaf LOD count");
                float a = M(S(1f, crest), cl), bb = S(cl, a);
                float tt = (float)((1.0 - tw) - (double)nl * cl + a);     // x87 (53-bit precision control) then stored as float
                float R = S(far, near), clR = M(cl, R), twc = D(tw, cl), omt = S(1f, tf), tfo = D(tf, omt), inv = D(1f, M(omt, tw));
                int r = 0;
                for (int i = 0; i < nl; i++)
                {
                    F[r] = 0;
                    P[r][2] = 1; P[r][3] = 0; P[r][5] = 0; P[r][6] = 0; P[r][7] = 1;
                    Q[r][0] = 1; Q[r][1] = 0; Q[r][2] = 1; Q[r][3] = 0; Q[r][5] = 0; Q[r][6] = 0; Q[r][7] = 1;
                    float t1 = A(bb, tt), d0 = A(M(R, t1), near), d1 = A(clR, d0);
                    C[r] = t1; F[r + 1] = 1;
                    float g = D(1f, S(d1, d0)), h = S(M(g, d1), twc);
                    P[r][1] = 0; Q[r][4] = C329; P[r][0] = 1; P[r][4] = C329;
                    Q[r + 1][0] = S(-0f, g); Q[r + 1][1] = h;
                    P[r + 1][0] = S(-0f, g); P[r + 1][1] = h; P[r + 1][2] = S(-0f, inv);
                    float ci = M(crest, inv); P[r + 1][3] = A(ci, 1f); Q[r + 1][2] = inv; Q[r + 1][3] = S(S(-0f, ci), tfo);
                    P[r + 1][5] = C671; P[r + 1][6] = 0; P[r + 1][7] = expo; P[r + 1][4] = C329;
                    for (int j = 4; j < 8; j++) Q[r + 1][j] = P[r + 1][j];
                    C[r + 1] = A(a, t1); tt = C[r + 1];
                    r += 2;
                }
                int last = r - 1;
                float u = D(1f, M(M(M(S(1f, ltf), radius), 2f), S(far, near)));
                P[last][7] = expo; P[last][1] = M(far, u); P[last][2] = -1f; P[last][0] = S(-0f, u); P[last][3] = 1f; P[last][4] = C329; P[last][5] = C671; P[last][6] = 0f;
                cmax = last; C[last] = 1f;
            }
            else C[0] = 1f;
            int amax = Math.Max(nb - 1, 0), bmax = Math.Max(nf - 1, 0), dmax = Math.Max(n3 - 1, 0);
            const float eps = 0.0001f;   // 0x9775a4
            int ia = 0, ib = 0, ic = 0, id = 0;
            while (eps > Ab[ia]) ia++;
            while (eps > Bf[ib]) ib++;
            while (eps > C[ic]) ic++;
            while (eps > Dl[id]) id++;
            t.records.Clear();
            while (true)
            {
                float m = Ab[ia];
                if (!(Bf[ib] > m)) m = Bf[ib];
                if (!(C[ic] > m)) m = C[ic];
                if (!(Dl[id] > m)) m = Dl[id];
                var rec = new ForestRecord { dist = A(M(S(far, near), m), near), branchLod = ia, frondLod = ib, leafLod = ic >> 1, leaf3dLod = id, transition = F[ic] != 0 };
                Array.Copy(P[ic], rec.leaf, 8); Array.Copy(Q[ic], rec.leaf3d, 8);
                t.records.Add(rec);
                if (ia >= amax && ib >= bmax && ic >= cmax && id >= dmax) break;
                m = A(m, eps);
                if (m >= Ab[ia] && ia < amax) ia++;
                if (m >= Bf[ib] && ib < bmax) ib++;
                if (m >= C[ic] && ic < cmax) ic++;
                if (m >= Dl[id] && id < dmax) id++;
                if (t.records.Count >= 10) break;
            }
        }

        static void FinishForest(ForestData f)
        {
            // ForestSystem +0x3074 / +0x3078 (FUN_006626c0)
            f.minBillboardStart = float.MaxValue; f.max3dDistance = -float.MaxValue;
            foreach (var t in f.types.Where(t => t.loaded))
            {
                f.minBillboardStart = Math.Min(f.minBillboardStart, t.bbStart);
                f.max3dDistance = Math.Max(f.max3dDistance, t.MaxDistance);
            }
        }

        /// What the loaded forest lacks (types that did not load, missing materials, tree textures or the atlas); empty when complete.
        public List<string> ForestProblems()
        {
            var p = new List<string>();
            if (forest == null) { p.Add("no forest"); return p; }
            foreach (var t in forest.types)
            {
                if (!t.loaded) { p.Add($"type {t.index} {t.treeAsset} not loaded"); continue; }
                if (t.records.Count == 0) p.Add($"{t.treeAsset}: no LOD records");
                foreach (var mn in new[] { t.branchMaterial, t.frondMaterial, t.leafMaterial }.Concat(t.surfaces.Select(q => q.material).Where(q => !string.IsNullOrEmpty(q))).Distinct())
                {
                    if (string.IsNullOrEmpty(mn) || !forestMaterials.TryGetValue(mn, out var lm)) { p.Add($"material {mn} missing"); continue; }
                    foreach (var tx in lm.textures) if (!string.IsNullOrEmpty(tx) && !sceneTextures.ContainsKey(tx)) p.Add($"texture {tx} of {mn} missing");
                }
            }
            if (forest.atlasDiffuse == null || !sceneTextures.ContainsKey(forest.atlasDiffuse)) p.Add($"atlas {forest.atlasDiffuse} missing");
            if (forest.atlasNormal == null || !sceneTextures.ContainsKey(forest.atlasNormal)) p.Add($"atlas normal {forest.atlasNormal} missing");
            lock (warnings) p.AddRange(warnings.Where(w => w.StartsWith("tree", StringComparison.Ordinal) || w.StartsWith("forest", StringComparison.Ordinal)));
            return p.Distinct().ToList();
        }

        // ------------------------------------------------------------------------------------------------ grass

        /// The grass asset (XenonGrassAsset parse FUN_0066cd30): a 0x100-byte header, the world texture (density, tuft, 16-bit height),
        /// the blade template of one cell (x, z, colour per blade; six vertices each), the cell grid. The GrassSystemRSData entity adds the
        /// draw / fade distances, the alpha test value and the mip bias (GrassSystem::vf5 006691a0).
        internal static GrassData ParseGrass(string name, byte[] b, SceneEntity e)
        {
            Rfx.Check(b.Length > 0x120 && Rfx.U32(b, 0) == 0xbb88aa11 && Rfx.I32(b, 4) == 2, "grass header");
            var g = new GrassData { name = name, type = Rfx.I32(b, 8), blades = Rfx.I32(b, 0xc), cells = Rfx.I32(b, 0x10), gridW = Rfx.I32(b, 0x14), gridH = Rfx.I32(b, 0x18) };
            g.center = new Vector3(Rfx.F32(b, 0x1c), Rfx.F32(b, 0x20), Rfx.F32(b, 0x24));
            g.half = new Vector3(Rfx.F32(b, 0x28), Rfx.F32(b, 0x2c), Rfx.F32(b, 0x30));
            // FUN_0066cd30 0066ce1d..0066cec3: the atlas column width by the header's type (1, 2: 0.5, 3: 0.25, else 1 with no offset column)
            float s; switch (g.type) { case 1: case 2: s = 0.5f; g.tuftU = 0.5f; break; case 3: s = 0.25f; g.tuftU = 0.25f; break; default: s = 1f; g.tuftU = 0f; break; }
            g.bladeU = s;
            g.tuftOffset = new Vector4(Rfx.F32(b, 0x34), Rfx.F32(b, 0x38), 0, 0);                         // g_TuftOffset c66 (edi + 0xc0)
            g.tuftScale = new Vector4(Rfx.F32(b, 0x3c), Rfx.F32(b, 0x40), g.tuftU, g.tuftU * 2f);          // g_TuftScale c65 (edi + 0xd0)
            g.atlasTexture = CString(b, 0x44, 64); g.colorTexture = CString(b, 0x84, 64);
            Rfx.Check(g.blades > 0 && g.blades <= 10000 && g.cells == g.gridW * g.gridH && g.cells > 0 && g.cells <= 1 << 20, "grass counts");
            int tw = BitConverter.ToUInt16(b, 0x100), th = BitConverter.ToUInt16(b, 0x102), fmt = Rfx.I32(b, 0x104), size = Rfx.I32(b, 0x108);
            Rfx.Check(fmt == 21 && size == tw * th * 4 && 0x120 + size <= b.Length, "grass world texture");
            g.textureWidth = tw; g.textureHeight = th;
            g.world = new byte[size]; Buffer.BlockCopy(b, 0x120, g.world, 0, size);                       // A8R8G8B8 rows, D3D order (row 0 = v 0)
            int p = 0x120 + size;
            Rfx.Check(p + g.blades * 12 + g.cells * 32 == b.Length, "grass asset size");
            g.bladeX = new float[g.blades]; g.bladeZ = new float[g.blades]; g.bladeColor = new uint[g.blades];
            for (int i = 0; i < g.blades; i++, p += 12) { g.bladeX[i] = Rfx.F32(b, p); g.bladeZ[i] = Rfx.F32(b, p + 4); g.bladeColor[i] = Rfx.U32(b, p + 8); }
            g.cellCenter = new Vector3[g.cells]; g.cellHalf = new Vector2[g.cells]; g.cellSeed = new byte[g.cells * 3];
            for (int i = 0; i < g.cells; i++, p += 32)
            {
                g.cellCenter[i] = new Vector3(Rfx.F32(b, p), Rfx.F32(b, p + 4), Rfx.F32(b, p + 8));
                g.cellSeed[i * 3] = b[p + 0xc]; g.cellSeed[i * 3 + 1] = b[p + 0xd]; g.cellSeed[i * 3 + 2] = b[p + 0xe];
                g.cellHalf[i] = new Vector2(Rfx.F32(b, p + 0x10), Rfx.F32(b, p + 0x18));
            }
            // GrassSystem::vf5: MaxDrawDistance / FadeDistance / ScaleValue (defaults 350 / 150 / 0.5, GrassSystem_Init), MIPBias (0),
            // AlphaTestValue (0.5; a value >= 1 is in 1/255 units)
            if (e.Has("MaxDrawDistance")) { g.maxDrawDistance = e.Float("MaxDrawDistance"); g.fadeDistance = e.Float("FadeDistance"); g.scaleValue = e.Float("ScaleValue"); }
            if (e.Has("MIPBias")) g.mipBias = e.Float("MIPBias");
            g.alphaTestValue = e.Has("AlphaTestValue") ? e.Float("AlphaTestValue") : 0.5f;
            if (g.alphaTestValue >= 1f) g.alphaTestValue = M(g.alphaTestValue, 0.003921569f);
            BuildGrassOrders(g);
            return g;
        }

        static string CString(byte[] b, int at, int max) { int n = 0; while (n < max && b[at + n] != 0) n++; return n == 0 ? null : System.Text.Encoding.ASCII.GetString(b, at, n); }

        /// FUN_0066c880: 16 index orders of the blade template, each sorted far to near (bubble sort, descending) from a point 1500 away
        /// in the direction k pi / 8 (the y rotation of +x by -k pi / 8: (cos, sin) in the blade's (x, z)); the draw picks the order of
        /// the camera's heading (FUN_0066e300 0066f9b2..0066fa3a).
        static void BuildGrassOrders(GrassData g)
        {
            g.orders = new int[16][];
            var key = new float[g.blades];
            for (int k = 0; k < 16; k++)
            {
                double th = -k * (Math.PI / 8);
                float px = (float)(Math.Cos(th) * 1500.0), pz = (float)(-Math.Sin(th) * 1500.0);
                var idx = new List<int>(g.blades);
                for (int i = 0; i < g.blades; i++) { float dz = S(g.bladeZ[i], pz), dx = S(g.bladeX[i], px); key[i] = A(M(dz, dz), M(dx, dx)); idx.Add(i); }
                // stable descending (the original's bubble sort keeps equal keys in order)
                var sorted = idx.OrderByDescending(i => key[i]).ToArray();
                g.orders[k] = sorted;
            }
        }

        /// The blade template mesh: six vertices per blade (FUN_0066cd30 0066d9e0..0066dc78: corners (0.8, 0.85), (0.8, 0), (-0.8, 0),
        /// = v2, (-0.8, 0.85), = v0; uv (s, 0), (s, 1), (0, 1), (0, 0)) and one submesh per heading order. Main thread.
        public GrassBuild BuildGrass(Shader grassShader)
        {
            if (grass == null || !grassShader) return null;
            var g = grass; int n = g.blades;
            var pos = new Vector3[n * 6]; var uv0 = new Vector4[n * 6]; var uv1 = new Vector4[n * 6]; var col = new Color32[n * 6];
            Vector2[] corner = { new Vector2(0.8f, 0.85f), new Vector2(0.8f, 0f), new Vector2(-0.8f, 0f), new Vector2(-0.8f, 0f), new Vector2(-0.8f, 0.85f), new Vector2(0.8f, 0.85f) };
            Vector2[] uv = { new Vector2(g.bladeU, 0), new Vector2(g.bladeU, 1), new Vector2(0, 1), new Vector2(0, 1), new Vector2(0, 0), new Vector2(g.bladeU, 0) };
            for (int i = 0; i < n; i++)
            {
                uint c = g.bladeColor[i];   // D3DCOLOR (COLOR1: x = R, y = G, z = B, w = A)
                var c32 = new Color32((byte)(c >> 16), (byte)(c >> 8), (byte)c, (byte)(c >> 24));
                for (int v = 0; v < 6; v++)
                {
                    pos[i * 6 + v] = Vector3.zero;
                    uv0[i * 6 + v] = new Vector4(g.bladeX[i], g.bladeZ[i], uv[v].x, 1f - uv[v].y);   // POSITION1 (x, z), TEXCOORD0 (v flipped for Unity's rows)
                    uv1[i * 6 + v] = new Vector4(corner[v].x, corner[v].y, 0, 0);                     // POSITION0: the corner
                    col[i * 6 + v] = c32;
                }
            }
            var mesh = new Mesh { name = g.name + " blades", indexFormat = n * 6 > 65535 ? IndexFormat.UInt32 : IndexFormat.UInt16, hideFlags = HideFlags.DontSave };
            mesh.vertices = pos; mesh.SetUVs(0, uv0); mesh.SetUVs(1, uv1); mesh.colors32 = col;
            mesh.subMeshCount = 16;
            for (int k = 0; k < 16; k++)
            {
                var tri = new int[n * 6];
                for (int j = 0; j < n; j++) { int bl = g.orders[k][j]; for (int v = 0; v < 6; v++) tri[j * 6 + v] = bl * 6 + v; }
                mesh.SetTriangles(tri, k, false);
            }
            mesh.bounds = new Bounds(Vector3.zero, Vector3.one * 1e5f);
            mesh.UploadMeshData(true); Track(mesh);
            // the world texture: A8R8G8B8 = BGRA bytes, rows uploaded as stored (Load(x, row) reads the D3D row), point sampled
            var world = new Texture2D(g.textureWidth, g.textureHeight, TextureFormat.BGRA32, false, true) { name = g.name + " world", filterMode = FilterMode.Point, wrapMode = TextureWrapMode.Clamp, hideFlags = HideFlags.DontSave };
            world.SetPixelData(g.world, 0); world.Apply(false, true); Track(world);
            var build = new GrassBuild { data = g, mesh = mesh, world = world };
            Material Make(bool near)
            {
                var m = new Material(grassShader) { name = g.name + (near ? " near" : " far"), hideFlags = HideFlags.DontSave, renderQueue = (int)RenderQueue.Transparent + (near ? 1 : 0) };
                m.SetTexture("_WorldTex", world);
                // g_GrassTexture: clamp, point (sampler states 3 / 1); g_ColorTexture: clamp, bilinear, point mips. MX vs ATV Alive's tuft atlases
                // are dilated for filtering (the texels under alpha 0 .. 0.03 carry the grass colour in every mip) and its executable's sampler
                // state is unknown (encrypted): bilinear, so a near flower or tuft is not drawn as big square texels (w-alivetrees)
                var grassFilter = AliveLevels.Is(level) ? FilterMode.Bilinear : FilterMode.Point;
                if (g.atlasTexture != null && sceneTextures.TryGetValue(g.atlasTexture, out var at)) { var t = ReflexTex.ToTexture(at, TextureWrapMode.Clamp, TextureWrapMode.Clamp, grassFilter); if (t) { Track(t); m.SetTexture("_GrassTex", t); } }
                else Warn($"grass atlas {g.atlasTexture} missing");
                if (g.colorTexture != null && sceneTextures.TryGetValue(g.colorTexture, out var ct)) { var t = ReflexTex.ToTexture(ct, TextureWrapMode.Clamp, TextureWrapMode.Clamp, FilterMode.Bilinear); if (t) { Track(t); m.SetTexture("_ColorTex", t); } }
                else Warn($"grass colour map {g.colorTexture} missing");
                m.SetFloat("_Near", near ? 1f : 0f);
                // far: ALPHAREF 2, SRCALPHA / INVSRCALPHA; near: ALPHAREF trunc(AlphaTestValue 256), no blending (FUN_006700d0 / FUN_0066e300)
                m.SetFloat("_AlphaRef", near ? (float)(int)(g.alphaTestValue * 256f) : 2f);
                m.SetFloat("_SrcBlend", near ? (float)BlendMode.One : (float)BlendMode.SrcAlpha);
                m.SetFloat("_DstBlend", near ? (float)BlendMode.Zero : (float)BlendMode.OneMinusSrcAlpha);
                Track(m);
                return m;
            }
            build.far = Make(false); build.near = Make(true);
            return build;
        }

        // ------------------------------------------------------------------------------------------------ alpha tests (per tree, per frame)

        /// g_TreeAlphaTests c68 of one 3D tree at view depth d with record r (00660720 0660d7e..00660f89): branch, frond, leaf, next leaf.
        public static Vector4 TreeAlphaTests(ForestTreeType t, ForestRecord r, float d)
        {
            var b = t.branchParams; var f = t.frondParams; var L = r.leaf; var Q = r.leaf3d;
            float x = A(M(Pow01(A(M(d, b[2]), b[3]), b[5]), b[6]), b[7]);
            float y = A(M(Pow01(A(M(d, f[2]), f[3]), f[5]), f[6]), f[7]);
            float z = A(M(Pow01(A(M(Sat(A(M(d, L[0]), L[1])), L[2]), L[3]), L[7]), L[5]), L[4]);
            float w = A(M(Pow01(A(M(Sat(A(M(d, Q[0]), Q[1])), Q[2]), Q[3]), Q[7]), Q[5]), Q[4]);
            return new Vector4(x, y, z, w);
        }
        static float Sat(float v) => v >= 1f ? 1f : v <= 0f ? 0f : v;
        static float Pow01(float v, float e) => (float)Math.Pow(Sat(v), e);   // cvtps2pd, the CRT pow, cvtsd2ss

        // ------------------------------------------------------------------------------------------------ Unity objects (main thread)

        /// Meshes and materials of the forest: per type the branch / frond / leaf meshes (one submesh per LOD) and the Reflex/Tree
        /// materials of their tree materials, the billboard material. Main thread.
        public ForestBuild BuildForest(Shader treeShader, Shader billboardShader)
        {
            if (forest == null || !treeShader || !billboardShader) return null;
            var build = new ForestBuild { data = forest, meshes = new Mesh[forest.types.Count, 4], materials = new Material[forest.types.Count, 4], partMaterials = new Material[forest.types.Count, 4][] };
            var mats = new Dictionary<string, Material>(StringComparer.Ordinal);
            foreach (var t in forest.types)
            {
                if (!t.loaded) continue;
                for (int k = 0; k < 4; k++)
                {
                    var g = t.geometry[k];
                    if (g == null) continue;
                    var mesh = new Mesh { name = $"{t.treeAsset} {(k == 0 ? "branches" : k == 1 ? "fronds" : k == 2 ? "leaves" : "3D leaves")}", indexFormat = IndexFormat.UInt16, hideFlags = HideFlags.DontSave };
                    mesh.vertices = g.positions; mesh.normals = g.normals; mesh.tangents = g.tangents;
                    mesh.SetUVs(0, g.uv0); mesh.SetUVs(1, g.uv1); if (g.uv2 != null) mesh.SetUVs(2, g.uv2);
                    mesh.subMeshCount = g.partTriangles.Length;
                    for (int i = 0; i < g.partTriangles.Length; i++) mesh.SetTriangles(g.partTriangles[i] ?? Array.Empty<int>(), i, false);
                    mesh.bounds = new Bounds(t.boundsCenter, t.boundsHalf * 2f + Vector3.one);
                    mesh.UploadMeshData(true); Track(mesh);
                    build.meshes[t.index, k] = mesh;
                    string mn = k == 0 ? t.branchMaterial : k == 1 ? t.frondMaterial : t.leafMaterial;
                    if (string.IsNullOrEmpty(mn) || !forestMaterials.TryGetValue(mn, out var lm)) { Warn($"tree {t.treeAsset}: material {mn} missing"); continue; }
                    if (!mats.TryGetValue(mn, out var m)) { m = CreateTreeMaterial(lm, treeShader); mats[mn] = m; }
                    build.materials[t.index, k] = m;
                    // per submesh: the surface's own material (Alive), else the block's
                    var pm = new Material[g.partTriangles.Length];
                    for (int i = 0; i < pm.Length; i++)
                    {
                        string sn = g.partMaterial[i];
                        if (sn == null || sn == mn) { pm[i] = m; continue; }
                        if (!mats.TryGetValue(sn, out var sm))
                        {
                            if (forestMaterials.TryGetValue(sn, out var slm)) { sm = CreateTreeMaterial(slm, treeShader); mats[sn] = sm; }
                            else { Warn($"tree {t.treeAsset}: surface material {sn} missing"); sm = m; }
                        }
                        pm[i] = sm;
                    }
                    build.partMaterials[t.index, k] = pm;
                }
            }
            // billboards: point-sampled diffuse, bilinear normal map (XenonForestAsset::vf4 sampler states 1 / 2, 1 mip filter)
            var bbMat = new Material(billboardShader) { name = forest.name + " billboards", hideFlags = HideFlags.DontSave, renderQueue = (int)RenderQueue.AlphaTest };
            if (forest.atlasDiffuse != null && sceneTextures.TryGetValue(forest.atlasDiffuse, out var ad)) { var tx = ReflexTex.ToTexture(ad, TextureWrapMode.Repeat, TextureWrapMode.Repeat, FilterMode.Point); if (tx) { Track(tx); bbMat.SetTexture("_BaseMap", tx); } }
            else Warn($"forest atlas {forest.atlasDiffuse} missing");
            if (forest.atlasNormal != null && sceneTextures.TryGetValue(forest.atlasNormal, out var an)) { var tx = ReflexTex.ToTexture(an, TextureWrapMode.Repeat, TextureWrapMode.Repeat, FilterMode.Bilinear); if (tx) { Track(tx); bbMat.SetTexture("_NormalMap", tx); } }
            Track(bbMat);
            build.billboardMaterial = bbMat;
            build.billboardRows = new Vector4[60];
            foreach (var t in forest.types)
                for (int r = 0; r < 3; r++) build.billboardRows[t.index * 3 + r] = new Vector4(t.billboard[r * 4], t.billboard[r * 4 + 1], t.billboard[r * 4 + 2], t.billboard[r * 4 + 3]);
            return build;
        }

        Material CreateTreeMaterial(LevelMaterial lm, Shader shader)
        {
            var m = new Material(shader) { name = lm.name + " (tree)", hideFlags = HideFlags.DontSave, renderQueue = (int)RenderQueue.AlphaTest };
            string ps = lm.pixelShader ?? "";
            int template = ps.IndexOf("Branch_Detail", StringComparison.Ordinal) >= 0 ? 2 : ps.IndexOf("Branch_Nm", StringComparison.Ordinal) >= 0 ? 1 : 0;
            // MX vs ATV Alive tree programs (T_EcoBark / T_EcoBranches / T_EcoLeaves): s0 usr_sampler_diffuse, s1 usr_sampler_normal_map
            // (the converted constant tables; older overlays have no shader resource: the slots in that order). They are translated by
            // template 0 (leaves, fronds) and 1 (bark) below, so only a program that is neither warns.
            bool alive = AliveLevels.Is(level);
            if (template == 0 && ps.IndexOf("Template_Eco_", StringComparison.Ordinal) < 0 && !(alive && ps.StartsWith("T_Eco", StringComparison.Ordinal)))
                Warn($"tree material {lm.name}: pixel program {ps} drawn as Template_Eco_Frond");
            if (alive && lm.name.Contains("_Br")) template = 1;
            m.SetFloat("_Template", template);
            // Alive's normal pass for the leaf (_Lf: T_EcoLeaves, rgb normal) and frond (_Fr: T_EcoBranches, rg normal) materials
            m.SetFloat("_AliveNormal", !alive || template != 0 ? 0f : lm.name.Contains("_Lf") ? 1f : lm.name.Contains("_Fr") ? 2f : 0f);
            // Alive's DL_AmdDirLight applies the dynamic shadow to every deferred pixel (the bark included)
            m.SetFloat("_UseDynShadow", alive || (lm.shader != null && lm.shader.Has("samplerDynamicShadowMap")) ? 1f : 0f);
            m.SetFloat("_AlphaRef", alive ? 0f : lm.alphaTest ? lm.alphaRef : -1f);
            m.SetFloat("_Cull", lm.cull == 1 ? (float)CullMode.Off : lm.cull == 2 ? (float)CullMode.Front : (float)CullMode.Back);
            m.SetFloat("_ZWrite", lm.zWrite ? 1f : 0f);
            for (int i = 0; i < lm.textures.Count; i++)
            {
                var tx = TextureFor(lm.textures[i], lm.samplerStates[i]); if (!tx) continue;
                string smp = i < lm.userSamplers.Length ? lm.userSamplers[i] : "";
                if (alive)
                {
                    bool nrm = smp.Length > 0 ? smp.IndexOf("normal", StringComparison.OrdinalIgnoreCase) >= 0 : i == 1;
                    if (smp.Length > 0 || i < 2) m.SetTexture(nrm ? "_NormalMap" : "_BaseMap", tx);
                    continue;
                }
                if (smp.EndsWith("normal_texture", StringComparison.Ordinal)) m.SetTexture("_NormalMap", tx);
                else if (smp.EndsWith("detail_branch_texture", StringComparison.Ordinal)) m.SetTexture("_DetailMap", tx);
                else m.SetTexture("_BaseMap", tx);   // usr_sampler_diffuse_texture, usr_sampler_main_branch_texture
            }
            Track(m);
            return m;
        }
    }

    public sealed class GrassData
    {
        public string name, atlasTexture, colorTexture;
        public int type, blades, cells, gridW, gridH, textureWidth, textureHeight;
        public Vector3 center, half;                 // the grid's box (header +0x1c / +0x28)
        public float bladeU, tuftU;                  // atlas column width of a blade (uv), g_TuftScale.z (w = 2 z)
        public Vector4 tuftOffset, tuftScale;        // g_TuftOffset c66, g_TuftScale c65
        public byte[] world;                         // the world texture: per texel B (height low), G (tuft), R (density), A (height high)
        public float[] bladeX, bladeZ; public uint[] bladeColor;
        public Vector3[] cellCenter; public Vector2[] cellHalf; public byte[] cellSeed;   // g_RandomSeed yzw = cell bytes +0xc..+0xe
        public int[][] orders;                       // 16 blade orders (heading k)
        public float maxDrawDistance = 350f, fadeDistance = 150f, scaleValue = 0.5f, mipBias, alphaTestValue = 0.5f;
    }

    /// The grass's Unity objects (ReflexLevelContent.BuildGrass), drawn by OriginalForest.
    public sealed class GrassBuild
    {
        public GrassData data;
        public Mesh mesh;                            // the blade template, 16 submeshes (the heading orders)
        public Texture2D world;
        public Material far, near;                   // PixelShaderFar (blended, ALPHAREF 2) / PixelShaderNear (alpha test, VSM shadow)
    }

    /// The forest's Unity objects (ReflexLevelContent.BuildForest), drawn every frame by OriginalWorld/OriginalForest.
    public sealed class ForestBuild
    {
        public ForestData data;
        public Mesh[,] meshes;              // [type, 0 branches / 1 fronds / 2 leaves / 3 3D leaves]
        public Material[,] materials;
        public Material[,][] partMaterials; // [type, block][submesh]: the material of each submesh (ForestGeometry.lodParts)
        public Material billboardMaterial;
        public Vector4[] billboardRows;     // g_BBTreeTypes (60 registers)
    }
}

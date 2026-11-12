// MX vs ATV Untamed converter, stage S3 "meshes": the port of untamed/tools/ux_meshes.py (DESIGN 5.4, 18, 19, 22).
// Untamed .surface / .model (Xbox 360) -> Reflex PC part models in the work store: meshes/<part>.surface + <part>.model, meshes.json
// (the contract for the entity stage and the builder) and meshes_info.json (statistics, the read-back checks).
//   * static groups (28-byte Xenon layout) -> the Reflex 44-byte vertex, crowd billboards (20-byte) -> the 28-byte crowd vertex;
//   * groups above the limits (4,800 vertices / 9,600 strip indices) are split by triangle partition and stripified again (crowd groups
//     only between billboards); more than 24 groups -> extra part models <name>_p<k>;
//   * two-stream FLOAT3 groups are cut into 128 ft cells by triangle centroid, each cell a part model <name>_c<k> with its own origin
//     (a non-identity model pivot is baked into the vertices first);
//   * bounds recomputed from the written positions (+ collision bounds on the part that carries collision, + sprite margin on crowds);
//   * draw distance 0 for a model that a collision-bearing entity uses.
// Byte parity with the Python stage is the goal: every float32 / float64 step follows numpy (see core/plan/UNTAMED_CONVERT.md).
// Entry points: UxMeshes.Convert (convert_level: the stage), UxMeshes.ConvertModel (convert_model: one model in memory, also for the
// builder's panels and gates, with its own material list and the rigid rule), UxMeshes.VerifyLevel (verify_level: the read-back
// checks), UxMeshLevel (level_data: the level's disc assets + level.json). The models of a level are converted and checked in
// parallel; results are merged in name order, so the output does not depend on the thread count.
// Unity-free (System only). Not ported: the CLI, --selftest and its synthetic tests (tests/untamed_convert/meshes covers them).
using System;
using System.Collections.Generic;
using System.Globalization;
using System.Threading;
using System.Threading.Tasks;
using Reflex.Port.Content.AliveImport;

namespace Reflex.Port.Content.UntamedImport
{
    /// The options of ux_meshes.convert_level / convert_model. The current builds use the defaults for all 12 levels.
    public sealed class UxMeshesOptions
    {
        /// limits = (max vertices per group, max strip indices per group, max groups per part model): U.LIMITS (--limits is for tests).
        public int maxGroupVerts = UxCommon.Limits.MaxGroupVerts, maxGroupStripIndices = UxCommon.Limits.MaxGroupStripIndices, maxGroupsPerModel = UxCommon.Limits.MaxGroupsPerModel;
        /// write=False (--no-write): convert in memory only (no files, no read-back check).
        public bool write = true;
        /// check=False (--no-check): skip the read-back verification (meshes_info.json 'checks' is null then).
        public bool check = true;
        /// The builder's rule (ux_build_track): a FAIL of the read-back checks is an error. Convert throws after the files are written.
        public bool failOnProblems = true;
        /// Worker threads for the models of a level (0 = one per processor). The result does not depend on it.
        public int threads = 0;
        /// meshes_info.json 'tool_md5s' (Python: the md5s of ux_meshes.py, uxmesh.py and ux_common.py; environment data). null = {}.
        public JObj toolMd5s;
    }

    /// One part model of a converted source model (the part dict of meshes.json plus the two files' bytes).
    public sealed class UxMeshPart
    {
        public string part; public int index; public string kind; public long[] cell; public double[] origin_offset;
        public int[] groups, material_index_map; public int[][] pieces; public string[] layouts, materials;
        public int verts, indices, tris; public int[] group_verts, group_indices;
        public long draw_distance; public bool carries_collision, crowd, pivot_baked; public float[] pivot;
        public double[] center, extents; public double radius; public float cw, ew;
        public byte[] surface_bytes, model_bytes;
        /// 'model' and 'surface': the asset names (= part).
        public string model => part;
        public string surface => part;

        /// The part dict as convert_model returns it (without the bytes), keys in Python's order.
        public JObj ToJson()
        {
            var piecesList = new List<object>(pieces.Length);
            foreach (var p in pieces) piecesList.Add(new long[] { p[0], p[1] });
            return new JObj().Set("part", part).Set("model", part).Set("surface", part).Set("index", (long)index).Set("kind", kind).Set("cell", cell)
                .Set("origin_offset", origin_offset).Set("groups", groups).Set("material_index_map", material_index_map).Set("pieces", piecesList)
                .Set("layouts", layouts).Set("materials", materials).Set("verts", (long)verts).Set("indices", (long)indices).Set("tris", (long)tris)
                .Set("group_verts", group_verts).Set("group_indices", group_indices).Set("draw_distance", draw_distance)
                .Set("carries_collision", carries_collision).Set("crowd", crowd).Set("pivot_baked", pivot_baked).Set("pivot", pivot)
                .Set("bounds", new JObj().Set("center", center).Set("extents", extents).Set("radius", radius).Set("w", new[] { cw, ew }));
        }
    }

    /// convert_model's result: {'model', 'parts', 'skipped', 'info'}.
    public sealed class UxMeshModel
    {
        public string model; public List<UxMeshPart> parts = new List<UxMeshPart>(); public string skipped; public JObj info = new JObj();
        /// info['uv']: per source group UV statistics (group, ml_index, verts, uv1_in01, uv1_eq_uv0, uv0_span, uv1_span).
        public List<UxMeshUv> uv = new List<UxMeshUv>();
    }
    public sealed class UxMeshUv { public int group, ml_index, verts; public double uv1_in01, uv1_eq_uv0, uv0_span, uv1_span; }

    /// Counter of applied vertex repairs (nonfinite_uv0, nonfinite_uv1, nonfinite_nrm, zero_normal, zero_tangent).
    public sealed class UxMeshFixes
    {
        public readonly SortedDictionary<string, long> counts = new SortedDictionary<string, long>(StringComparer.Ordinal);
        public void Add(string key, long n) { counts.TryGetValue(key, out long v); counts[key] = v + n; }
        public void Add(UxMeshFixes other) { foreach (var kv in other.counts) Add(kv.Key, kv.Value); }
        public JObj ToJson() { var o = new JObj(); foreach (var kv in counts) o.items.Add(new KeyValuePair<string, object>(kv.Key, kv.Value)); return o; }
    }

    /// What convert_level returns and writes.
    public sealed class UxMeshesResult
    {
        public JObj meshes, info; public List<string> problems; public bool ChecksPassed => problems == null || problems.Count == 0;
        /// Seconds: conversion, read-back checks.
        public double convertSeconds, checkSeconds;
    }

    /// ux_meshes.level_data + the lookups convert_model needs: the level's disc assets and work/<Level>/level.json.
    public sealed class UxMeshLevel
    {
        public readonly UxLevelRow lv; public readonly UxDiscLevel disc; public readonly JObj level;
        readonly Dictionary<string, JObj> _model = new Dictionary<string, JObj>(StringComparer.Ordinal), _surface = new Dictionary<string, JObj>(StringComparer.Ordinal),
            _collis = new Dictionary<string, JObj>(StringComparer.Ordinal);
        readonly Dictionary<string, string> _modelCi = new Dictionary<string, string>(StringComparer.Ordinal), _surfaceCi = new Dictionary<string, string>(StringComparer.Ordinal),
            _collisCi = new Dictionary<string, string>(StringComparer.Ordinal);
        readonly Dictionary<string, List<JObj>> _entities = new Dictionary<string, List<JObj>>(StringComparer.Ordinal);
        public readonly List<string> modelNames = new List<string>();

        public UxMeshLevel(UxDiscLevel disc, JObj level)
        {
            this.disc = disc; this.level = level; lv = disc.lv;
            var closure = UxJson.Obj(level.Get("closure"));
            Index(UxJson.Obj(closure.Get("model")), _model, _modelCi, modelNames);
            Index(UxJson.Obj(closure.Get("surface")), _surface, _surfaceCi, null);
            Index(UxJson.Obj(closure.Get("collis")), _collis, _collisCi, null);
            foreach (object eo in UxJson.Arr(level.Get("entities")))
            {
                var e = UxJson.Obj(eo);
                string cls = e.Get("cls") as string;
                if (cls != "keep" && cls != "demote") continue;
                var m = (e.Get("props") as JObj)?.Get("ModelAsset") as JObj;
                if (m == null) continue;
                string key = UxCommon.PyLower(m.Get("asset") as string ?? "");
                if (!_entities.TryGetValue(key, out var l)) _entities[key] = l = new List<JObj>();
                l.Add(e);
            }
        }
        static void Index(JObj d, Dictionary<string, JObj> exact, Dictionary<string, string> ci, List<string> names)
        {
            foreach (var kv in d.items)
            {
                exact[kv.Key] = UxJson.Obj(kv.Value); names?.Add(kv.Key);
                string lo = UxCommon.PyLower(kv.Key);
                if (!ci.ContainsKey(lo)) ci[lo] = kv.Key;
            }
        }
        /// _ci(d, name): the canonical key of a closure dict ('model', 'surface', 'collis') ignoring case, or null.
        public string Ci(string closure, string name)
        {
            var (exact, ci) = Closure(closure);
            if (exact.ContainsKey(name)) return name;
            return ci.TryGetValue(UxCommon.PyLower(name), out string k) ? k : null;
        }
        public JObj Record(string closure, string key) => Closure(closure).Item1[key];
        (Dictionary<string, JObj>, Dictionary<string, string>) Closure(string closure)
        {
            switch (closure) { case "model": return (_model, _modelCi); case "surface": return (_surface, _surfaceCi); case "collis": return (_collis, _collisCi); }
            throw new UxException("closure " + closure);
        }
        static readonly List<JObj> None = new List<JObj>();
        /// model_entities(level, model_name): the kept / demoted entities whose ModelAsset is the model, in merge order.
        public List<JObj> ModelEntities(string modelName) => _entities.TryGetValue(UxCommon.PyLower(modelName), out var l) ? l : None;
        /// _material_list(e): the asset names of the entity's MaterialList (null for an entry without one).
        public static List<string> MaterialList(JObj e)
        {
            object ml = (e.Get("props") as JObj)?.Get("MaterialList");
            var o = new List<string>();
            if (!Truthy(ml)) return o;
            if (ml is List<object> list) foreach (object x in list) o.Add((x as JObj)?.Get("asset") as string);
            else o.Add((ml as JObj)?.Get("asset") as string);
            return o;
        }
        internal static bool Truthy(object v)
        {
            switch (v)
            {
                case null: return false;
                case bool b: return b;
                case string s: return s.Length > 0;
                case long l: return l != 0;
                case double d: return d != 0;
                case List<object> a: return a.Count > 0;
                case JObj j: return j.items.Count > 0;
            }
            return true;
        }
        /// collision_bounds(level, model_name): (lo, hi) over the Female collision of every kept collision-bearing entity using the
        /// model (model space), or false. Kinds 0 (mesh), 11, 15, 17 (box proxies of the primitives); kinds 2 / 19 give no triangles.
        public bool CollisionBounds(string modelName, out double[] lo, out double[] hi, CancellationToken cancel = default)
        {
            lo = hi = null;
            foreach (var e in ModelEntities(modelName))
            {
                if (!Truthy(e.Get("collision"))) continue;
                var f = (e.Get("props") as JObj)?.Get("FemaleCollisionAsset") as JObj;
                string asset = f?.Get("asset") as string;
                string k = !string.IsNullOrEmpty(asset) ? Ci("collis", asset) : null;
                if (k == null) continue;
                double[] t = UxMesh.CollisTriangles(disc.ReadExtractFile(UxJson.Str(_collis[k].Get("file")), cancel));
                if (t == null || t.Length == 0) continue;
                var a = new[] { t[0], t[1], t[2] }; var b = new[] { t[0], t[1], t[2] };
                for (int i = 3; i < t.Length; i++) { int c = i % 3; if (t[i] < a[c]) a[c] = t[i]; if (t[i] > b[c]) b[c] = t[i]; }
                if (lo == null) { lo = a; hi = b; }
                else for (int c = 0; c < 3; c++) { lo[c] = Math.Min(lo[c], a[c]); hi[c] = Math.Max(hi[c], b[c]); }
            }
            return lo != null;
        }
    }

    public static class UxMeshes
    {
        public const string SCHEMA = "ux_meshes/1";
        public const double CELL_FT = 128.0;                 // DESIGN 5.4: re-centring cells for FLOAT3 groups
        public const double ORIGIN_STEP = 0.25;              // cell origins rounded to 1/4 ft (exact in float32)
        public const double MAX_RECENTRE_ERR = 1.0 / 16;     // ft, acceptance target for re-centred positions
        public const double NORMAL_TOL = 1e-5;               // P0 (DESIGN 18): every written normal |n^2-1| < this
        public static readonly double[] CROWD_PAD = { 2.6, 7.2, 2.6 };   // ft, billboard sprite margin on the half extents
        const uint XF16_4 = 0x1A2360, XFLOAT3 = 0x2A23B9, XF16_2 = 0x2C235F;
        const int STATIC = 0, CROWD = 1, FLOAT3 = 2, NOLAYOUT = -1;
        static readonly string[] LayoutName = { "static", "crowd", "float3" };
        static readonly byte[] STATIC_DECL_BLOCK = UxRmesh.DECL_BLOCK;
        static readonly byte[] CROWD_DECL_BLOCK = UxBin.Concat(UxMesh.CROWD_DECL_PC, new byte[160 - UxMesh.CROWD_DECL_PC.Length]);
        static int Stride(int layout) => layout == CROWD ? UxMesh.CROWD_STRIDE : UxMesh.VTX_STRIDE;
        static readonly CultureInfo Inv = CultureInfo.InvariantCulture;

        // =================================================================================================== PC surface codec
        /// One group of a Reflex PC surface: strip, vertex bytes (44 per vertex: 'static', 28: 'crowd').
        public sealed class PcGroup { public ushort[] strip; public byte[] verts; public string layout; public int VertexCount => verts.Length / (layout == "crowd" ? UxMesh.CROWD_STRIDE : UxMesh.VTX_STRIDE); }

        /// write_surface_pc(groups) (= rmesh.write_surface when every group is static): Reflex PC v4 surface bytes. Record layout:
        /// u32 0, 0xFFFFFFFF, 0, primcount, nverts, index bytes, decl[160], u32 1, vertex bytes, 160 zero bytes.
        public static byte[] WriteSurfacePc(IList<PcGroup> groups)
        {
            int size = 16 + groups.Count * 0x160;
            foreach (var g in groups) size += g.strip.Length * 2 + g.verts.Length;
            var o = new byte[size];
            UxBin.PutU32(o, 0, UxRmesh.SURF_MAGIC); UxBin.PutU32(o, 4, 4); UxBin.PutU32(o, 8, (uint)groups.Count);
            int r = 16, p = 16 + groups.Count * 0x160;
            foreach (var g in groups)
            {
                bool crowd = g.layout == "crowd";
                int stride = crowd ? UxMesh.CROWD_STRIDE : UxMesh.VTX_STRIDE, nidx = g.strip.Length, nv = g.verts.Length / stride;
                int max = -1; foreach (ushort s in g.strip) if (s > max) max = s;
                UxBin.Require(g.verts.Length % stride == 0 && nv <= 65536 && (nidx == 0 || max < nv), "surface group: strip index outside its " + nv + " vertices");
                UxBin.PutU32(o, r + 4, 0xFFFFFFFF); UxBin.PutU32(o, r + 12, (uint)Math.Max(nidx - 2, 0)); UxBin.PutU32(o, r + 16, (uint)nv); UxBin.PutU32(o, r + 20, (uint)(nidx * 2));
                Buffer.BlockCopy(crowd ? CROWD_DECL_BLOCK : STATIC_DECL_BLOCK, 0, o, r + 0x18, 160);
                UxBin.PutU32(o, r + 0xB8, 1); UxBin.PutU32(o, r + 0xBC, (uint)(nv * stride));
                r += 0x160;
                for (int i = 0; i < nidx; i++) UxBin.PutU16(o, p + 2 * i, g.strip[i]);
                p += nidx * 2;
                Buffer.BlockCopy(g.verts, 0, o, p, g.verts.Length); p += g.verts.Length;
            }
            return o;
        }

        /// read_surface_pc(data): a Reflex PC v4 surface with the static and / or crowd declaration. Strict: unknown declarations,
        /// record fields or trailing bytes throw.
        public static List<PcGroup> ReadSurfacePc(byte[] data)
        {
            if (data.Length < 16 || UxBin.U32LE(data, 0) != UxRmesh.SURF_MAGIC || UxBin.U32LE(data, 4) != 4 || UxBin.U32LE(data, 12) != 0) throw new UxException("not a Reflex v4 surface");
            long ngl = UxBin.U32LE(data, 8);
            UxBin.Require(0x10 + ngl * 0x160 <= data.Length, "surface: group records outside the file");
            int ng = (int)ngl; long p = 0x10 + ng * 0x160;
            var o = new List<PcGroup>(ng);
            for (int gi = 0; gi < ng; gi++)
            {
                int r = 0x10 + gi * 0x160;
                uint a = UxBin.U32LE(data, r), m1 = UxBin.U32LE(data, r + 4), fl = UxBin.U32LE(data, r + 8), prim = UxBin.U32LE(data, r + 12), nv = UxBin.U32LE(data, r + 16), ib = UxBin.U32LE(data, r + 20);
                uint ns = UxBin.U32LE(data, r + 0xB8), vb = UxBin.U32LE(data, r + 0xBC);
                string lay = UxBin.Same(data, r + 0x18, STATIC_DECL_BLOCK, 0, 160) ? "static" : UxBin.Same(data, r + 0x18, CROWD_DECL_BLOCK, 0, 160) ? "crowd" : null;
                if (lay == null) throw new UxException("group " + gi + ": unknown vertex declaration");
                uint stride = lay == "crowd" ? (uint)UxMesh.CROWD_STRIDE : UxMesh.VTX_STRIDE;
                bool zeros = true; for (int i = 0xC0; i < 0x160 && zeros; i++) zeros = data[r + i] == 0;
                if (a != 0 || m1 != 0xFFFFFFFF || fl != 0 || ns != 1 || vb != (ulong)nv * stride || ib % 2 != 0 || (ib != 0 && prim != ib / 2 - 2) || !zeros)
                    throw new UxException("group " + gi + ": unexpected record fields");
                UxBin.Require(p + ib + vb <= data.Length, "group " + gi + ": data outside the file");
                var strip = new ushort[ib / 2]; int max = -1;
                for (int i = 0; i < strip.Length; i++) { strip[i] = UxBin.U16LE(data, (int)p + 2 * i); if (strip[i] > max) max = strip[i]; }
                p += ib;
                var g = new PcGroup { strip = strip, verts = UxBin.Slice(data, (int)p, (int)vb), layout = lay };
                p += vb;
                if (strip.Length > 0 && max >= nv) throw new UxException("group " + gi + ": index out of range");
                o.Add(g);
            }
            if (p != data.Length) throw new UxException((data.Length - p) + " trailing bytes");
            return o;
        }

        // =================================================================================================== vertex conversion
        /// group_layout(g): 'static' (28-byte F16 position), 'crowd' (20-byte billboard), 'float3' (two-stream FLOAT3 position) or -1.
        static int GroupLayout(UxGroup g)
        {
            if (g.flags != 0 || g.bidx != null) return NOLAYOUT;
            uint pos = 0; int n = 0;
            foreach (var e in g.decl) if (e.usage == 0) { pos = e.type; n++; }
            if (n != 1) return NOLAYOUT;
            if (pos == XFLOAT3) return g.uv0 != null ? FLOAT3 : NOLAYOUT;
            if (pos != XF16_4 || g.uv0 == null) return NOLAYOUT;
            if (g.uv1 == null && g.tan == null && g.strides.Length == 1 && g.strides[0] == 20) return CROWD;
            return STATIC;
        }

        /// A source group as convert_vertices sees it: the group (after bake_pivot: with the baked normals and tangents) and, for a
        /// FLOAT3 group, its positions in float64 (the source values, or the baked ones).
        sealed class Src { public UxGroup g; public double[] pos64; }

        static float Norm32(float x, float y, float z)
        {   // np.linalg.norm(float32 (k,3), axis=1): sqrt((x*x + y*y) + z*z), every step rounded to float32
            float s = (float)(x * x); s = (float)(s + (float)(y * y)); s = (float)(s + (float)(z * z));
            return (float)Math.Sqrt(s);
        }
        static bool Finite(float v) => !float.IsNaN(v) && !float.IsInfinity(v);
        const float EPS32 = 1e-6f;       // a float32 array compared with the Python float 1e-6 (numpy 2: the scalar becomes float32)

        /// np.allclose(m, np.eye(n), atol=atol) for the top-left n x n of a row-major matrix with `width` columns.
        static bool CloseToIdentity(float[] m, int width, int n, double atol)
        {
            for (int i = 0; i < n; i++)
                for (int j = 0; j < n; j++)
                {
                    double a = m[i * width + j], b = i == j ? 1.0 : 0.0;
                    if (double.IsNaN(a) || double.IsInfinity(a) || !(Math.Abs(a - b) <= atol + 1e-5 * Math.Abs(b))) return false;
                }
            return true;
        }

        /// np.linalg.inv of a 3 x 3 float64 matrix: LAPACK dgesv (LU with partial pivoting, then the two triangular solves against the
        /// identity) written out. Equal bit for bit to numpy on the five pivots this stage inverts (see the stage report).
        static double[] Inv3(double[] R)
        {
            const int n = 3;
            var A = (double[])R.Clone(); var piv = new[] { 0, 1, 2 };
            for (int k = 0; k < n; k++)
            {
                int p = k;
                for (int i = k + 1; i < n; i++) if (Math.Abs(A[i * n + k]) > Math.Abs(A[p * n + k])) p = i;
                if (A[p * n + k] == 0) throw new UxException("Singular matrix");
                if (p != k)
                {
                    for (int j = 0; j < n; j++) { double t = A[k * n + j]; A[k * n + j] = A[p * n + j]; A[p * n + j] = t; }
                    int tp = piv[k]; piv[k] = piv[p]; piv[p] = tp;
                }
                for (int i = k + 1; i < n; i++)
                {
                    A[i * n + k] = A[i * n + k] / A[k * n + k];
                    for (int j = k + 1; j < n; j++) A[i * n + j] = A[i * n + j] - A[i * n + k] * A[k * n + j];
                }
            }
            var X = new double[9]; var b = new double[3];
            for (int c = 0; c < n; c++)
            {
                for (int i = 0; i < n; i++) b[i] = piv[i] == c ? 1.0 : 0.0;
                for (int i = 0; i < n; i++) for (int j = 0; j < i; j++) b[i] = b[i] - A[i * n + j] * b[j];
                for (int i = n - 1; i >= 0; i--)
                {
                    for (int j = i + 1; j < n; j++) b[i] = b[i] - A[i * n + j] * b[j];
                    b[i] = b[i] / A[i * n + i];
                }
                for (int i = 0; i < n; i++) X[i * n + c] = b[i];
            }
            return X;
        }
        static double Det3(double[] m) => m[0] * (m[4] * m[8] - m[5] * m[7]) - m[1] * (m[3] * m[8] - m[5] * m[6]) + m[2] * (m[3] * m[7] - m[4] * m[6]);

        static double[] ToDouble(float[] a) { var d = new double[a.Length]; for (int i = 0; i < a.Length; i++) d[i] = a[i]; return d; }

        /// The unit() of bake_pivot: rows of a float64 (k,3) array normalised (norm > 1e-12, else 0), as float32.
        static float[] UnitRows(double[] v)
        {
            var o = new float[v.Length];
            for (int i = 0; i + 2 < v.Length; i += 3)
            {
                double x = v[i], y = v[i + 1], z = v[i + 2];
                double n = Math.Sqrt((x * x + y * y) + z * z);
                bool ok = n > 1e-12;
                o[i] = ok ? (float)(x / n) : 0f; o[i + 1] = ok ? (float)(y / n) : 0f; o[i + 2] = ok ? (float)(z / n) : 0f;
            }
            return o;
        }

        /// bake_pivot(g, pivot): the group with the model pivot (row-major 4 x 4, row 3 = translation) applied to its vertices:
        /// positions p @ R + t (float64), tangents normalise(t @ R), normals normalise(n @ inv(R).T).
        static Src BakePivot(UxGroup g, float[] pivot)
        {
            var R = new double[9]; var t = new double[3];
            for (int i = 0; i < 3; i++) { for (int j = 0; j < 3; j++) R[3 * i + j] = pivot[4 * i + j]; t[i] = pivot[12 + i]; }
            int n = g.pos.Length / 3;
            var pos = UxNp.MatMul(ToDouble(g.pos), n, 3, R, 3);
            for (int i = 0; i < n; i++) for (int c = 0; c < 3; c++) pos[3 * i + c] += t[c];
            var b = Clone(g);
            if (g.nrm != null)
            {
                var inv = Inv3(R); var invT = new double[9];
                for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) invT[3 * i + j] = inv[3 * j + i];
                b.nrm = UnitRows(UxNp.MatMul(ToDouble(g.nrm), g.nrm.Length / 3, 3, invT, 3));
            }
            if (g.tan != null) b.tan = UnitRows(UxNp.MatMul(ToDouble(g.tan), g.tan.Length / 3, 3, R, 3));
            return new Src { g = b, pos64 = pos };
        }
        static UxGroup Clone(UxGroup g) => new UxGroup
        {
            flags = g.flags, decl = g.decl, strip = g.strip, npal = g.npal, palette = g.palette, prim = g.prim, nverts = g.nverts, pos = g.pos, pos_w = g.pos_w,
            nrm = g.nrm, tan = g.tan, uv0 = g.uv0, uv1 = g.uv1, col = g.col, bidx = g.bidx, bwgt = g.bwgt, tris = g.tris, strides = g.strides,
        };

        /// convert_vertices(g, layout, origin, fixes): the Reflex vertices of an Untamed group (the structured array's bytes; 'static' /
        /// 'float3' -> 44 bytes, 'crowd' -> 28). origin is subtracted from the positions (FLOAT3 groups). fixes: the applied repairs.
        static byte[] ConvertVertices(Src s, int layout, double ox, double oy, double oz, UxMeshFixes fixes)
        {
            UxGroup g = s.g; int n = g.nverts; byte[] v; int stride;
            if (layout == CROWD) { v = UxMesh.ReflexCrowdVertices(g); stride = UxMesh.CROWD_STRIDE; }
            else
            {
                v = UxMesh.ReflexVertices(g, ox, oy, oz); stride = UxMesh.VTX_STRIDE;
                if (s.pos64 != null)
                    for (int i = 0; i < n; i++)
                    {   // v['pos'][:, :3] = g.pos - origin, with the float64 positions (bake_pivot's; equal to the float32 ones otherwise)
                        int o = i * stride;
                        UxBin.PutU16(v, o, UxNp.DoubleToHalf(s.pos64[3 * i] - ox)); UxBin.PutU16(v, o + 2, UxNp.DoubleToHalf(s.pos64[3 * i + 1] - oy));
                        UxBin.PutU16(v, o + 4, UxNp.DoubleToHalf(s.pos64[3 * i + 2] - oz));
                    }
            }
            // non-finite source UVs: the whole row becomes 0
            for (int k = 0; k < 2; k++)
            {
                if (k == 1 && layout == CROWD) continue;
                UxVal src = k == 0 ? g.uv0 : g.uv1;
                if (src == null) continue;
                UxBin.Require(src.f != null && src.comps >= 2, "uv element kind");
                int at = k == 0 ? (layout == CROWD ? UxMesh.CROWD_UV0 : UxMesh.VTX_UV0) : UxMesh.VTX_UV1; long bad = 0;
                for (int i = 0; i < n; i++)
                    if (!Finite(src.f[i * src.comps]) || !Finite(src.f[i * src.comps + 1]))
                    { bad++; int o = i * stride + at; v[o] = v[o + 1] = v[o + 2] = v[o + 3] = 0; }
                if (bad > 0) fixes?.Add(k == 0 ? "nonfinite_uv0" : "nonfinite_uv1", bad);
            }
            // normals: non-finite rows -> 0, zero-length -> +Y, then unit length in float64 (P0, DESIGN 18)
            int nrmAt = layout == CROWD ? UxMesh.CROWD_NRM : UxMesh.VTX_NRM;
            var nrm = new float[n * 3]; long nonfinite = 0, zero = 0;
            for (int i = 0; i < n; i++)
            {
                int o = i * stride + nrmAt;
                float x = UxBin.F32LE(v, o), y = UxBin.F32LE(v, o + 4), z = UxBin.F32LE(v, o + 8);
                if (!Finite(x) || !Finite(y) || !Finite(z)) { nonfinite++; x = y = z = 0f; }
                if (Norm32(x, y, z) < EPS32) { zero++; x = 0f; y = 1f; z = 0f; }
                nrm[3 * i] = x; nrm[3 * i + 1] = y; nrm[3 * i + 2] = z;
            }
            if (nonfinite > 0) fixes?.Add("nonfinite_nrm", nonfinite);
            if (zero > 0) fixes?.Add("zero_normal", zero);
            double[] unit = UxMesh.UnitNormals(nrm);
            for (int i = 0; i < n * 3; i++) nrm[i] = (float)unit[i];
            for (int i = 0; i < n; i++) { int o = i * stride + nrmAt; UxBin.PutF32(v, o, nrm[3 * i]); UxBin.PutF32(v, o + 4, nrm[3 * i + 1]); UxBin.PutF32(v, o + 8, nrm[3 * i + 2]); }
            if (layout != CROWD)
            {   // zero-length tangents -> a unit vector perpendicular to the normal (_perp_tangents)
                long zt = 0;
                for (int i = 0; i < n; i++)
                {
                    int o = i * stride + UxMesh.VTX_TAN;
                    float x = UxBin.F32LE(v, o), y = UxBin.F32LE(v, o + 4), z = UxBin.F32LE(v, o + 8);
                    if (Norm32(x, y, z) >= EPS32) continue;
                    zt++;
                    double n0 = nrm[3 * i], n1 = nrm[3 * i + 1], n2 = nrm[3 * i + 2];
                    bool nearx = Math.Abs(n0) > 0.9;
                    double a0 = nearx ? 0.0 : 1.0, a1 = 0.0, a2 = nearx ? 1.0 : 0.0;
                    double d = (a0 * n0 + a1 * n1) + a2 * n2;
                    double t0 = a0 - n0 * d, t1 = a1 - n1 * d, t2 = a2 - n2 * d;
                    double ln = Math.Sqrt((t0 * t0 + t1 * t1) + t2 * t2);
                    UxBin.PutF32(v, o, (float)(t0 / ln)); UxBin.PutF32(v, o + 4, (float)(t1 / ln)); UxBin.PutF32(v, o + 8, (float)(t2 / ln));
                }
                if (zt > 0) fixes?.Add("zero_tangent", zt);
            }
            for (int i = 0; i < n; i++)
            {
                int o = i * stride;
                for (int c = 0; c < 4; c++) if ((UxBin.U16LE(v, o + 2 * c) & 0x7C00) == 0x7C00) throw new UxException("non-finite position after conversion");
            }
            return v;
        }

        /// convert_vertices(g, layout, origin) again for another origin, in place: the results for two origins differ in the position
        /// element only (FLOAT16_4 of g.pos - origin), and the non-finite check is that of the position. Not for the crowd layout (it
        /// takes no origin).
        static void Reposition(byte[] v, Src s, double ox, double oy, double oz)
        {
            UxGroup g = s.g; int n = g.nverts;
            for (int i = 0; i < n; i++)
            {
                double x = s.pos64 != null ? s.pos64[3 * i] : g.pos[3 * i], y = s.pos64 != null ? s.pos64[3 * i + 1] : g.pos[3 * i + 1], z = s.pos64 != null ? s.pos64[3 * i + 2] : g.pos[3 * i + 2];
                ushort hx = UxNp.DoubleToHalf(x - ox), hy = UxNp.DoubleToHalf(y - oy), hz = UxNp.DoubleToHalf(z - oz);
                if ((hx & 0x7C00) == 0x7C00 || (hy & 0x7C00) == 0x7C00 || (hz & 0x7C00) == 0x7C00) throw new UxException("non-finite position after conversion");
                int o = i * UxMesh.VTX_STRIDE;
                UxBin.PutU16(v, o, hx); UxBin.PutU16(v, o + 2, hy); UxBin.PutU16(v, o + 4, hz);
            }
        }

        // =================================================================================================== splitting
        /// One output piece of a split: the strip in local indices and the sorted source vertex indices.
        sealed class Piece { public int[] strip, vidx; }

        /// Per-vertex work arrays of one model conversion (a group has at most 65,536 vertices), reused by every split of the model:
        /// "vertex seen" marks as generation stamps, and the source -> local index map.
        sealed class Scratch
        {
            public readonly int[] stamp, local; int _gen;
            public Scratch(int nverts) { stamp = new int[nverts]; local = new int[nverts]; }
            public int Next() => ++_gen;
        }

        /// _chunks_by_vertices(tris, max_verts): contiguous runs [s, e) of triangles holding at most max_verts distinct vertices each.
        static List<int[]> ChunksByVertices(int[] tris, int maxVerts, Scratch sc)
        {
            var o = new List<int[]>(); int nt = tris.Length / 3;
            int[] stamp = sc.stamp; int gen = sc.Next(), seen = 0, s = 0;
            for (int j = 0; j < nt; j++)
            {
                int a = tris[3 * j], b = tris[3 * j + 1], c = tris[3 * j + 2];
                int nw = 0;
                if (stamp[a] != gen) nw++;
                if (b != a && stamp[b] != gen) nw++;
                if (c != a && c != b && stamp[c] != gen) nw++;
                if (seen + nw > maxVerts) { o.Add(new[] { s, j }); gen = sc.Next(); seen = 0; s = j; }
                if (stamp[a] != gen) { stamp[a] = gen; seen++; }
                if (stamp[b] != gen) { stamp[b] = gen; seen++; }
                if (stamp[c] != gen) { stamp[c] = gen; seen++; }
            }
            if (s < nt) o.Add(new[] { s, nt });
            return o;
        }

        /// _local_strip(tris[s:e]): (the strip in local indices, the sorted source vertex indices).
        static Piece LocalStrip(int[] tris, int s, int e, Scratch sc)
        {
            int[] stamp = sc.stamp, local = sc.local; int gen = sc.Next(), count = 0;
            for (int i = 3 * s; i < 3 * e; i++) if (stamp[tris[i]] != gen) { stamp[tris[i]] = gen; count++; }
            var vidx = new int[count]; int k = 0; gen = sc.Next();
            for (int i = 3 * s; i < 3 * e; i++) if (stamp[tris[i]] != gen) { stamp[tris[i]] = gen; vidx[k++] = tris[i]; }
            Array.Sort(vidx);                                              // np.unique: the distinct indices, ascending
            for (int i = 0; i < count; i++) local[vidx[i]] = i;           // np.searchsorted(vidx, tris)
            var loc = new int[3 * (e - s)];
            for (int i = 0; i < loc.Length; i++) loc[i] = local[tris[3 * s + i]];
            return new Piece { strip = UxRmesh.Stripify(loc), vidx = vidx };
        }

        /// _fit(tris[s:e]): strip one run of triangles; bisect while the strip is too long.
        static void Fit(int[] tris, int s, int e, Scratch sc, int maxVerts, int maxIdx, List<Piece> o)
        {
            var p = LocalStrip(tris, s, e, sc);
            if (p.strip.Length <= maxIdx && p.vidx.Length <= maxVerts) { o.Add(p); return; }
            if (e - s == 1) throw new UxException("a single triangle does not fit the limits");
            int h = (e - s) / 2;
            Fit(tris, s, s + h, sc, maxVerts, maxIdx, o); Fit(tris, s + h, e, sc, maxVerts, maxIdx, o);
        }

        /// split_triangles(tris, max_verts, max_idx): the triangle partition of one group (source indices, front-face winding, source
        /// strip order) into pieces within the limits, each stripified again.
        static List<Piece> SplitTriangles(int[] tris, Scratch sc, int maxVerts, int maxIdx)
        {
            var o = new List<Piece>();
            foreach (var c in ChunksByVertices(tris, maxVerts, sc)) Fit(tris, c[0], c[1], sc, maxVerts, maxIdx, o);
            return o;
        }

        /// billboard_units(tris, nverts): the connected component of every triangle of a crowd group, numbered in order of first appearance.
        static int[] BillboardUnits(int[] tris, int nverts)
        {
            var parent = new int[nverts]; for (int i = 0; i < nverts; i++) parent[i] = i;
            int Find(int x) { while (parent[x] != x) { parent[x] = parent[parent[x]]; x = parent[x]; } return x; }
            int nt = tris.Length / 3;
            for (int t = 0; t < nt; t++)
            {
                int ra = Find(tris[3 * t]);
                for (int k = 1; k < 3; k++) { int rx = Find(tris[3 * t + k]); if (rx != ra) parent[rx] = ra; }
            }
            var ids = new Dictionary<int, int>(); var o = new int[nt];
            for (int t = 0; t < nt; t++)
            {
                int r = Find(tris[3 * t]);
                if (!ids.TryGetValue(r, out int id)) ids[r] = id = ids.Count;
                o[t] = id;
            }
            return o;
        }

        /// split_crowd(tris, nverts, max_verts, max_idx): a crowd group split only between billboards (units never cut).
        static List<Piece> SplitCrowd(int[] tris, int nverts, Scratch sc, int maxVerts, int maxIdx)
        {
            int nt = tris.Length / 3;
            int[] unit = BillboardUnits(tris, nverts);
            int nu = 0; foreach (int u in unit) if (u + 1 > nu) nu = u + 1;
            var utris = new List<int>[nu]; for (int u = 0; u < nu; u++) utris[u] = new List<int>();
            for (int t = 0; t < nt; t++) utris[unit[t]].Add(t);
            var unv = new int[nu]; var stamp = new int[nverts]; int big = 0;
            for (int u = 0; u < nu; u++)
            {
                int c = 0;
                foreach (int t in utris[u]) for (int k = 0; k < 3; k++) { int vi = tris[3 * t + k]; if (stamp[vi] != u + 1) { stamp[vi] = u + 1; c++; } }
                unv[u] = c; if (c > big) big = c;
            }
            if (nu > 0 && big > maxVerts) throw new UxException("a billboard unit has " + big + " vertices, above the limit");
            var o = new List<Piece>();
            void FitUnits(List<int> us, int s, int e)
            {
                var ts = new List<int>();
                for (int i = s; i < e; i++) ts.AddRange(utris[us[i]]);
                ts.Sort();
                var sub = new int[ts.Count * 3];
                for (int i = 0; i < ts.Count; i++) { sub[3 * i] = tris[3 * ts[i]]; sub[3 * i + 1] = tris[3 * ts[i] + 1]; sub[3 * i + 2] = tris[3 * ts[i] + 2]; }
                var p = LocalStrip(sub, 0, ts.Count, sc);
                if (p.strip.Length <= maxIdx && p.vidx.Length <= maxVerts) { o.Add(p); return; }
                if (e - s == 1) throw new UxException("one billboard unit does not fit the index limit");
                int h = (e - s) / 2;
                FitUnits(us, s, s + h); FitUnits(us, s + h, e);
            }
            var cur = new List<int>(); int nvCur = 0;
            for (int u = 0; u < nu; u++)
            {
                if (cur.Count > 0 && nvCur + unv[u] > maxVerts) { FitUnits(cur, 0, cur.Count); cur = new List<int>(); nvCur = 0; }
                cur.Add(u); nvCur += unv[u];
            }
            if (cur.Count > 0) FitUnits(cur, 0, cur.Count);
            return o;
        }

        // =================================================================================================== model conversion
        /// One output group: layout, strip, vertex bytes, source group, source vertex indices (null = all), piece numbering.
        sealed class Out { public int layout; public int[] strip; public byte[] verts; public int src; public int[] vidx; public int piece, pieceOf; public int VertexCount => verts.Length / Stride(layout); }

        static byte[] Gather(byte[] v, int[] vidx, int stride)
        {
            var o = new byte[vidx.Length * stride];
            for (int i = 0; i < vidx.Length; i++) Buffer.BlockCopy(v, vidx[i] * stride, o, i * stride, stride);
            return o;
        }
        static double HalfAt(byte[] v, int at) => UxNp.HalfToFloat(UxBin.U16LE(v, at));

        /// _bounds(positions, pivot, extra): the AABB of the outputs' decoded positions after the pivot's 3 x 3 (stock Reflex bounds),
        /// unioned with the extra (lo, hi) boxes -> centre, half extents, radius.
        static void Bounds(List<Out> outs, float[] pivot, List<double[][]> extra, out double[] c, out double[] e, out double r)
        {
            int total = 0; foreach (var x in outs) total += x.VertexCount;
            double[] lo = new double[3], hi = new double[3];                // no vertex: P = zeros((1, 3))
            if (total > 0 && CloseToIdentity(pivot, 4, 3, 1e-6))
            {
                bool first = true;
                foreach (var x in outs)
                {
                    int st = Stride(x.layout), n = x.VertexCount;
                    for (int i = 0; i < n; i++)
                    {
                        for (int k = 0; k < 3; k++)
                        {
                            double h = HalfAt(x.verts, i * st + 2 * k);
                            if (first) { lo[k] = hi[k] = h; continue; }
                            if (h < lo[k]) lo[k] = h; if (h > hi[k]) hi[k] = h;
                        }
                        first = false;
                    }
                }
            }
            else if (total > 0)
            {
                var P = new double[total * 3]; int at = 0;
                foreach (var x in outs) { int st = Stride(x.layout), n = x.VertexCount; for (int i = 0; i < n; i++) for (int k = 0; k < 3; k++) P[at++] = HalfAt(x.verts, i * st + 2 * k); }
                var R = new double[9]; for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) R[3 * i + j] = pivot[4 * i + j];
                P = UxNp.MatMul(P, total, 3, R, 3);
                for (int k = 0; k < 3; k++) lo[k] = hi[k] = P[k];
                for (int i = 3; i < P.Length; i++) { int k = i % 3; if (P[i] < lo[k]) lo[k] = P[i]; if (P[i] > hi[k]) hi[k] = P[i]; }
            }
            foreach (var ab in extra) for (int k = 0; k < 3; k++) { lo[k] = Math.Min(lo[k], ab[0][k]); hi[k] = Math.Max(hi[k], ab[1][k]); }
            c = new double[3]; e = new double[3];
            for (int k = 0; k < 3; k++) { c[k] = (lo[k] + hi[k]) / 2; e[k] = (hi[k] - lo[k]) / 2; }
            r = UxNp.Norm(e);
        }

        /// rigid_bound_problems(m, groups, tol): why a rigid multi-node model cannot be drawn as a static model under the BOUND-node
        /// rule (ux_splitgate): [] when every node a model group binds (record word 2) has an identity matrix and no group is skinned.
        public static List<string> RigidBoundProblems(UxModel m, List<UxGroup> groups, double tol = 1e-4)
        {
            var o = new List<string>();
            var used = new SortedSet<long>(); foreach (var r in m.recs) used.Add(r[2]);
            foreach (long k in used)
            {
                if (k >= m.node_matrices.Count) { o.Add("group binds node " + k + " of " + m.node_matrices.Count); continue; }
                if (!CloseToIdentity(m.node_matrices[(int)k], 4, 4, tol)) o.Add("bound node " + k + " (" + m.node_names[(int)k] + ") matrix is not the identity");
            }
            if (groups.Count != m.ngroups) o.Add("surface has " + groups.Count + " groups, model " + m.ngroups);
            for (int gi = 0; gi < groups.Count; gi++)
                if (groups[gi].flags != 0 || groups[gi].bidx != null || groups[gi].bwgt != null) o.Add("surface group " + gi + " is skinned");
            return o;
        }

        static string PyList(IEnumerable<int> items) { var sb = new System.Text.StringBuilder("["); bool first = true; foreach (int i in items) { if (!first) sb.Append(", "); sb.Append(i.ToString(Inv)); first = false; } return sb.Append(']').ToString(); }

        /// convert_model(level, model_name, collision_bearing, collision_bounds, limits, fixes, materials, rigid): one Untamed model
        /// and its surface (looked up in the level closure, else among the disc's assets) -> Reflex part models.
        /// materials: the material names to write, one per source model group (null: the MaterialList of the first entity using the
        /// model, else the Untamed defaults). rigid: "all" = a rigid multi-node model is drawn static only when every node matrix is the
        /// identity; "bound" (ux_splitgate) = when every node a group binds is. cbLo / cbHi: the collision bounds in model space or null.
        public static UxMeshModel ConvertModel(UxMeshLevel L, string modelName, bool collisionBearing, double[] cbLo = null, double[] cbHi = null, UxMeshesOptions limits = null,
            UxMeshFixes fixes = null, IList<string> materials = null, string rigid = "all", CancellationToken cancel = default)
        {
            limits = limits ?? new UxMeshesOptions();
            int maxV = limits.maxGroupVerts, maxI = limits.maxGroupStripIndices, maxG = limits.maxGroupsPerModel;
            UxBin.Require(maxG > 0, "limits: groups per model");
            fixes = fixes ?? new UxMeshFixes();
            string mkey = L.Ci("model", modelName);
            string name = mkey ?? modelName;
            string skey = L.Ci("surface", name);
            var res = new UxMeshModel { model = name };
            byte[] sdata = null;
            byte[] Surface() => sdata ?? (sdata = skey != null ? L.disc.ReadExtractFile(UxJson.Str(L.Record("surface", skey).Get("file")), cancel) : L.disc.ReadExtract(name, "surface", cancel));
            byte[] mdata; UxModel m;
            try
            {
                mdata = mkey != null ? L.disc.ReadExtractFile(UxJson.Str(L.Record("model", mkey).Get("file")), cancel) : L.disc.ReadExtract(modelName, "model", cancel);
                m = UxMesh.ReadModel(mdata);
            }
            catch (OperationCanceledException) { throw; }
            catch (Exception ex) { res.skipped = "model unreadable: " + ex.Message; return res; }
            if (m.skinned && m.boned)
            {   // fix round 2: a rigid multi-node model in the closure is one ux_level kept as static: read it with the static fields
                try
                {
                    var mr = UxMesh.ReadModelRigid(mdata);
                    var sg = UxMesh.ReadSurface(Surface());
                    var pr = rigid == "bound" ? RigidBoundProblems(mr, sg) : UxMesh.RigidStaticProblems(mr, sg);
                    if (pr.Count == 0) m = mr;
                }
                catch (OperationCanceledException) { throw; }
                catch (Exception) { }
            }
            if (m.skinned)
            {
                res.skipped = (m.boned ? "rigid multi-node model (" + m.nbones + " nodes)" : "skinned model") + ": not converted (DESIGN 5.4)";
                return res;
            }
            bool order = m.lods.Length == 1 && m.lods[0][0] == (uint)m.ngroups && m.lods[0][1] == 0 && m.lods[0][2] == 0 && m.order.Length == m.ngroups;
            for (int i = 0; order && i < m.ngroups; i++) order = m.order[i] == (uint)i;
            if (!order) { res.skipped = "model with LODs or a group order other than 0..n-1: not supported"; return res; }
            List<UxGroup> groups;
            try { groups = UxMesh.ReadSurface(Surface()); }
            catch (OperationCanceledException) { throw; }
            catch (Exception ex) { res.skipped = "surface unreadable: " + ex.Message; return res; }
            if (groups.Count != m.ngroups) { res.skipped = "model has " + m.ngroups + " groups, surface " + groups.Count; return res; }
            // model group i draws surface group recs[i][1] with MaterialList entry i
            var surfToMl = new Dictionary<long, int>();
            for (int i = 0; i < m.recs.Length; i++) if (!surfToMl.ContainsKey(m.recs[i][1])) surfToMl[m.recs[i][1]] = i;
            bool perm = surfToMl.Count == groups.Count;
            for (int i = 0; perm && i < groups.Count; i++) perm = surfToMl.ContainsKey(i);
            if (!perm) { res.skipped = "model group -> surface group map is not a permutation"; return res; }
            int ng = groups.Count;
            var lays = new int[ng]; var unsupported = new List<int>();
            for (int i = 0; i < ng; i++) { lays[i] = GroupLayout(groups[i]); if (lays[i] == NOLAYOUT) unsupported.Add(i); }
            if (unsupported.Count > 0) { res.skipped = "unsupported vertex layout in group(s) " + PyList(unsupported); return res; }
            if (materials == null)
            {
                var ents = L.ModelEntities(name);
                List<string> ml = ents.Count > 0 ? UxMeshLevel.MaterialList(ents[0]) : null;
                bool ok = ml != null && ml.Count > 0 && ml.Count == m.ngroups;
                for (int i = 0; ok && i < ml.Count; i++) ok = !string.IsNullOrEmpty(ml[i]);
                materials = ok ? (IList<string>)ml : new List<string>(m.materials);
            }
            float[] pivot = m.matrix;
            bool anyFloat3 = Array.IndexOf(lays, FLOAT3) >= 0;
            bool bake = anyFloat3 && !CloseToIdentity(pivot, 4, 4, 0);
            if (bake)
            {
                var R = new double[9]; for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) R[3 * i + j] = pivot[4 * i + j];
                if (Math.Abs(Det3(R)) < 1e-9) { res.skipped = "FLOAT3 groups under a singular pivot matrix: not supported"; return res; }
            }
            var fgroups = new Src[ng];
            for (int si = 0; si < ng; si++)
                if (lays[si] == FLOAT3) fgroups[si] = bake ? BakePivot(groups[si], pivot) : new Src { g = groups[si], pos64 = ToDouble(groups[si].pos) };
            long ddSrc = m.draw_distance, dd = collisionBearing ? 0 : ddSrc;
            bool isCrowd = Array.IndexOf(lays, CROWD) >= 0;
            int maxNv = 0; foreach (var g in groups) maxNv = Math.Max(maxNv, g.nverts);
            var scratch = new Scratch(maxNv);

            // ---- output groups for the origin-0 base and per cell
            var baseOuts = new List<Out>();
            var cells = new SortedDictionary<(long, long), List<(int si, int[] tris)>>();
            var splitGroups = new List<object>(); var float3Groups = new List<long>(); var emptyGroups = new List<long>();
            for (int si = 0; si < ng; si++)
            {
                cancel.ThrowIfCancellationRequested();
                UxGroup g = groups[si]; int lay = lays[si];
                if (lay == FLOAT3)
                {
                    float3Groups.Add(si);
                    int[] t = g.tris; int nt = t.Length / 3;
                    if (nt == 0) { emptyGroups.Add(si); continue; }
                    double[] pos = fgroups[si].pos64;
                    var byCell = new Dictionary<(long, long), List<int>>();
                    for (int j = 0; j < nt; j++)
                    {   // cen = pos[t].mean(1): (p0 + p1 + p2) / 3 per coordinate; key = floor(cen[[0, 2]] / 128)
                        int a = 3 * t[3 * j], b = 3 * t[3 * j + 1], c = 3 * t[3 * j + 2];
                        double cx = ((pos[a] + pos[b]) + pos[c]) / 3, cz = ((pos[a + 2] + pos[b + 2]) + pos[c + 2]) / 3;
                        var key = ((long)Math.Floor(cx / CELL_FT), (long)Math.Floor(cz / CELL_FT));
                        if (!byCell.TryGetValue(key, out var list)) byCell[key] = list = new List<int>();
                        list.Add(j);
                    }
                    foreach (var kv in byCell)
                    {
                        var sel = new int[kv.Value.Count * 3];
                        for (int i = 0; i < kv.Value.Count; i++) { int j = kv.Value[i]; sel[3 * i] = t[3 * j]; sel[3 * i + 1] = t[3 * j + 1]; sel[3 * i + 2] = t[3 * j + 2]; }
                        if (!cells.TryGetValue(kv.Key, out var cl)) cells[kv.Key] = cl = new List<(int, int[])>();
                        cl.Add((si, sel));
                    }
                    continue;
                }
                bool tooBig = g.nverts > maxV || g.strip.Length > maxI;
                var src = new Src { g = g };
                if (!tooBig)
                {
                    if (g.tris.Length == 0) emptyGroups.Add(si);
                    baseOuts.Add(new Out { layout = lay, strip = g.strip, verts = ConvertVertices(src, lay, 0, 0, 0, fixes), src = si });
                    continue;
                }
                byte[] v = ConvertVertices(src, lay, 0, 0, 0, fixes);
                var pieces = lay == CROWD ? SplitCrowd(g.tris, g.nverts, scratch, maxV, maxI) : SplitTriangles(g.tris, scratch, maxV, maxI);
                splitGroups.Add(new long[] { si, g.nverts, g.strip.Length, pieces.Count });
                foreach (var p in pieces) baseOuts.Add(new Out { layout = lay, strip = p.strip, verts = Gather(v, p.vidx, Stride(lay)), src = si, vidx = p.vidx });
            }

            void PieceNumbers(List<Out> outs)
            {
                var cnt = new Dictionary<int, int>(); var seen = new Dictionary<int, int>();
                foreach (var o in outs) { cnt.TryGetValue(o.src, out int c); cnt[o.src] = c + 1; }
                foreach (var o in outs) { seen.TryGetValue(o.src, out int s); o.piece = s; o.pieceOf = cnt[o.src]; seen[o.src] = s + 1; }
            }

            // cell outputs: per cell, origin = the centre of the cell's vertex bbox (source float positions), rounded
            var cellOuts = new List<((long, long) ck, double[] o, List<Out> outs)>();
            // convert_vertices(g, 'float3', origin=o) per (cell, group): the group is converted once (the first cell counts its vertex
            // fixes, as Python does) and only its positions are written again for the next origin
            var conv = new byte[ng][];
            foreach (var kv in cells)
            {
                cancel.ThrowIfCancellationRequested();
                double[] lo = null, hi = null;
                foreach (var (si, t) in kv.Value)
                {
                    double[] pos = fgroups[si].pos64;
                    foreach (int vi in t)
                        for (int k = 0; k < 3; k++)
                        {
                            double x = pos[3 * vi + k];
                            if (lo == null) { lo = new[] { pos[3 * vi], pos[3 * vi + 1], pos[3 * vi + 2] }; hi = (double[])lo.Clone(); }
                            if (x < lo[k]) lo[k] = x; if (x > hi[k]) hi[k] = x;
                        }
                }
                var o = new double[3];
                for (int k = 0; k < 3; k++) o[k] = UxNp.Round(((lo[k] + hi[k]) / 2) / ORIGIN_STEP) * ORIGIN_STEP;
                var outs = new List<Out>();
                foreach (var (si, t) in kv.Value)
                {
                    if (conv[si] == null) conv[si] = ConvertVertices(fgroups[si], FLOAT3, o[0], o[1], o[2], fixes);
                    else Reposition(conv[si], fgroups[si], o[0], o[1], o[2]);
                    foreach (var p in SplitTriangles(t, scratch, maxV, maxI))
                        outs.Add(new Out { layout = STATIC, strip = p.strip, verts = Gather(conv[si], p.vidx, UxMesh.VTX_STRIDE), src = si, vidx = p.vidx });
                }
                cellOuts.Add((kv.Key, o, outs));
            }
            PieceNumbers(baseOuts);
            var allCell = new List<Out>(); foreach (var c in cellOuts) allCell.AddRange(c.outs);
            PieceNumbers(allCell);

            // ---- pack into part models of <= max_g groups
            var plan = new List<(string pn, string kind, long[] cell, double[] o, List<Out> outs)>();
            for (int k = 0; k < baseOuts.Count; k += maxG)
                plan.Add((k == 0 ? name : name + "_p" + (k / maxG).ToString(Inv), "base", null, new double[3], baseOuts.GetRange(k, Math.Min(maxG, baseOuts.Count - k))));
            for (int ci = 0; ci < cellOuts.Count; ci++)
            {
                var (ck, o, outs) = cellOuts[ci];
                for (int k = 0; k < outs.Count; k += maxG)
                    plan.Add((name + "_c" + ci.ToString(Inv) + (k == 0 ? "" : "_p" + (k / maxG).ToString(Inv)), "cell", new[] { ck.Item1, ck.Item2 }, o, outs.GetRange(k, Math.Min(maxG, outs.Count - k))));
            }

            double[] srcLo = new double[3], srcHi = new double[3];
            for (int k = 0; k < 3; k++) { srcLo[k] = (double)m.center[k] - (double)m.extents[k]; srcHi[k] = (double)m.center[k] + (double)m.extents[k]; }
            float cw = m.center[3], ew = m.extents[3];
            int collPart = -1;
            if (collisionBearing) { collPart = plan.FindIndex(p => p.kind == "base"); if (collPart < 0) collPart = 0; }
            float[] eye = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
            for (int pi = 0; pi < plan.Count; pi++)
            {
                cancel.ThrowIfCancellationRequested();
                var (pn, kind, ck, o, outs) = plan[pi];
                var extra = new List<double[][]>();
                bool carries = pi == collPart;
                if (carries && cbLo != null)
                    extra.Add(new[] { new[] { cbLo[0] - o[0], cbLo[1] - o[1], cbLo[2] - o[2] }, new[] { cbHi[0] - o[0], cbHi[1] - o[1], cbHi[2] - o[2] } });
                bool crowdPart = outs.Exists(x => x.layout == CROWD);
                if (crowdPart)
                {
                    double[] lo = null, hi = null;
                    foreach (var x in outs)
                    {
                        int st = Stride(x.layout), n = x.VertexCount;
                        for (int i = 0; i < n; i++)
                            for (int k = 0; k < 3; k++)
                            {
                                double h = HalfAt(x.verts, i * st + 2 * k);
                                if (lo == null) { lo = new[] { HalfAt(x.verts, i * st), HalfAt(x.verts, i * st + 2), HalfAt(x.verts, i * st + 4) }; hi = (double[])lo.Clone(); }
                                if (h < lo[k]) lo[k] = h; if (h > hi[k]) hi[k] = h;
                            }
                    }
                    UxBin.Require(lo != null, pn + ": a crowd part without vertices");
                    extra.Add(new[] { new[] { lo[0] - CROWD_PAD[0], lo[1] - CROWD_PAD[1], lo[2] - CROWD_PAD[2] }, new[] { hi[0] + CROWD_PAD[0], hi[1] + CROWD_PAD[1], hi[2] + CROWD_PAD[2] } });
                    extra.Add(new[] { new[] { srcLo[0] - o[0], srcLo[1] - o[1], srcLo[2] - o[2] }, new[] { srcHi[0] - o[0], srcHi[1] - o[1], srcHi[2] - o[2] } });
                }
                bool baked = bake && kind == "cell";
                float[] ppivot = baked ? eye : pivot;
                Bounds(outs, ppivot, extra, out double[] c, out double[] e, out double r);
                var mats = new string[outs.Count]; var mlMap = new int[outs.Count];
                for (int i = 0; i < outs.Count; i++) { mlMap[i] = surfToMl[outs[i].src]; mats[i] = materials[mlMap[i]]; }
                var mm = new UxPcModel(pn, mats, (float)r, new[] { (float)c[0], (float)c[1], (float)c[2] }, new[] { (float)e[0], (float)e[1], (float)e[2] },
                    new List<uint[]> { new uint[] { (uint)outs.Count, 0, 0 } }, (uint)dd, (float[])ppivot.Clone());
                mm.center4 = new[] { (float)c[0], (float)c[1], (float)c[2], cw };
                mm.extents4 = new[] { (float)e[0], (float)e[1], (float)e[2], ew };
                byte[] mbytes = UxRmesh.WriteModel(mm);
                var pcg = new List<PcGroup>(outs.Count);
                foreach (var x in outs)
                {
                    var strip = new ushort[x.strip.Length]; for (int i = 0; i < strip.Length; i++) strip[i] = unchecked((ushort)x.strip[i]);
                    pcg.Add(new PcGroup { strip = strip, verts = x.verts, layout = x.layout == CROWD ? "crowd" : "static" });
                }
                byte[] sbytes = WriteSurfacePc(pcg);
                var part = new UxMeshPart
                {
                    part = pn, index = pi, kind = kind, cell = ck, origin_offset = (double[])o.Clone(),
                    groups = new int[outs.Count], material_index_map = mlMap, pieces = new int[outs.Count][], layouts = new string[outs.Count], materials = mats,
                    group_verts = new int[outs.Count], group_indices = new int[outs.Count], draw_distance = dd, carries_collision = carries, crowd = crowdPart,
                    pivot_baked = baked, pivot = (float[])ppivot.Clone(), center = c, extents = e, radius = r, cw = cw, ew = ew, surface_bytes = sbytes, model_bytes = mbytes,
                };
                for (int i = 0; i < outs.Count; i++)
                {
                    var x = outs[i];
                    part.groups[i] = x.src; part.pieces[i] = new[] { x.piece, x.pieceOf }; part.layouts[i] = x.layout == CROWD ? "crowd" : "static";
                    part.group_verts[i] = x.VertexCount; part.group_indices[i] = x.strip.Length;
                    part.verts += x.VertexCount; part.indices += x.strip.Length; part.tris += UxMesh.StripToTris(x.strip).Length / 3;
                }
                res.parts.Add(part);
            }

            // per source group UV statistics (for S4's template choice) and bounds staleness
            for (int si = 0; si < ng; si++)
            {
                UxGroup g = groups[si];
                if (lays[si] == CROWD || g.uv1 == null || g.nverts == 0) continue;
                UxBin.Require(g.uv0.f != null && g.uv1.f != null && g.uv0.comps >= 2 && g.uv1.comps >= 2, "uv element kind");
                int n = g.nverts, c0 = g.uv0.comps, c1 = g.uv1.comps; long in01 = 0, eq = 0; bool any0 = false, any1 = false;
                double[] min0 = new double[2], max0 = new double[2], min1 = new double[2], max1 = new double[2];
                for (int i = 0; i < n; i++)
                {
                    double a0 = g.uv0.f[i * c0], b0 = g.uv0.f[i * c0 + 1], a1 = g.uv1.f[i * c1], b1 = g.uv1.f[i * c1 + 1];
                    bool ok0 = UxNp.Finite(a0, b0), ok1 = UxNp.Finite(a1, b1);
                    if (ok1 && a1 >= -1e-3 && a1 <= 1 + 1e-3 && b1 >= -1e-3 && b1 <= 1 + 1e-3) in01++;
                    if (ok0 && ok1 && a0 == a1 && b0 == b1) eq++;
                    if (ok0)
                    {
                        if (!any0) { min0[0] = max0[0] = a0; min0[1] = max0[1] = b0; any0 = true; }
                        min0[0] = Math.Min(min0[0], a0); max0[0] = Math.Max(max0[0], a0); min0[1] = Math.Min(min0[1], b0); max0[1] = Math.Max(max0[1], b0);
                    }
                    if (ok1)
                    {
                        if (!any1) { min1[0] = max1[0] = a1; min1[1] = max1[1] = b1; any1 = true; }
                        min1[0] = Math.Min(min1[0], a1); max1[0] = Math.Max(max1[0], a1); min1[1] = Math.Min(min1[1], b1); max1[1] = Math.Max(max1[1], b1);
                    }
                }
                res.uv.Add(new UxMeshUv
                {
                    group = si, ml_index = surfToMl[si], verts = n, uv1_in01 = (double)in01 / n, uv1_eq_uv0 = (double)eq / n,
                    uv0_span = any0 ? Math.Max(max0[0] - min0[0], max0[1] - min0[1]) : 0.0, uv1_span = any1 ? Math.Max(max1[0] - min1[0], max1[1] - min1[1]) : 0.0,
                });
            }
            double delta;
            {
                bool any = false; float[] lo = new float[3], hi = new float[3];
                foreach (var g in groups)
                {
                    if (g.nverts == 0) continue;
                    for (int i = 0; i < g.pos.Length; i++)
                    {
                        int k = i % 3; float x = g.pos[i];
                        if (!any) { lo[0] = hi[0] = g.pos[0]; lo[1] = hi[1] = g.pos[1]; lo[2] = hi[2] = g.pos[2]; any = true; }
                        if (x < lo[k]) lo[k] = x; if (x > hi[k]) hi[k] = x;
                    }
                }
                if (any)
                {   // float32 throughout: allpos, m.center and m.extents are float32 arrays
                    float d = float.NegativeInfinity;
                    for (int k = 0; k < 3; k++)
                    {
                        float rc = (float)((float)(lo[k] + hi[k]) / 2f), re = (float)((float)(hi[k] - lo[k]) / 2f);
                        d = Math.Max(d, Math.Max(Math.Abs((float)(rc - m.center[k])), Math.Abs((float)(re - m.extents[k]))));
                    }
                    delta = d;
                }
                else
                {   // allpos = zeros((1, 3)) float64
                    double d = double.NegativeInfinity;
                    for (int k = 0; k < 3; k++) d = Math.Max(d, Math.Max(Math.Abs(0.0 - m.center[k]), Math.Abs(0.0 - m.extents[k])));
                    delta = d;
                }
            }
            var layCount = new JObj();
            foreach (int l in lays) layCount.Set(LayoutName[l], (long)(layCount.Get(LayoutName[l]) ?? 0L) + 1);
            object cbJson = null;
            if (cbLo != null) cbJson = new List<object> { (double[])cbLo.Clone(), (double[])cbHi.Clone() };
            res.info = new JObj().Set("source_groups", (long)ng).Set("layouts", layCount).Set("split_groups", splitGroups).Set("float3_groups", float3Groups)
                .Set("empty_groups", emptyGroups).Set("cells", (long)cellOuts.Count).Set("parts", (long)plan.Count).Set("draw_distance_source", ddSrc).Set("draw_distance", dd)
                .Set("collision_bearing", collisionBearing).Set("collision_bounds", cbJson).Set("pivot_identity", CloseToIdentity(pivot, 4, 4, 1e-6)).Set("pivot_baked", bake)
                .Set("source_bounds_delta", delta).Set("default_materials_source", new List<string>(m.materials)).Set("materials", new List<string>(materials)).Set("crowd", isCrowd);
            if (m.rigid) res.info.Set("rigid_static", new JObj().Set("nodes", (long)m.node_names.Count).Set("node_names", new List<string>(m.node_names)));
            return res;
        }

        // =================================================================================================== level driver
        static int Threads(UxMeshesOptions o) => o.threads > 0 ? o.threads : Math.Max(1, Environment.ProcessorCount);

        /// The stage: convert_level(lv, write, check, limits). Reads ctx.work 'level.json' and the level's disc assets; writes
        /// meshes/<part>.surface, meshes/<part>.model, meshes/meshes.json and meshes/meshes_info.json into ctx.work.
        public static UxMeshesResult Convert(UxContext ctx, UxMeshesOptions options = null, Action<string, int, int> progress = null)
            => Convert(ctx.disc, ctx.work, options, progress, ctx.cancel, ctx.log);

        /// progress(phase, done, total): phase "convert" (models converted), "write" (files), "check" (models read back).
        public static UxMeshesResult Convert(UxDiscLevel disc, UxWork work, UxMeshesOptions options = null, Action<string, int, int> progress = null, CancellationToken cancel = default,
            Action<string> log = null)
        {
            options = options ?? new UxMeshesOptions();
            var clock = System.Diagnostics.Stopwatch.StartNew();
            UxLevelRow lv = disc.lv;
            if (!work.Exists("level.json")) throw new UxException("level.json missing: run the level stage for " + lv.shortName + " first");
            byte[] levelBytes = work.ReadBytes("level.json");
            var L = new UxMeshLevel(disc, UxJson.Obj(UxJson.ParseUtf8(levelBytes)));
            var names = new List<string>(L.modelNames);
            names.Sort(StringComparer.Ordinal);
            var results = new UxMeshModel[names.Count]; var modelFixes = new UxMeshFixes[names.Count];
            int done = 0;
            var po = new ParallelOptions { MaxDegreeOfParallelism = Threads(options), CancellationToken = cancel };
            RunParallel(names.Count, po, i =>
            {
                string mname = names[i];
                bool cb = UxMeshLevel.Truthy(L.Record("model", mname).Get("with_collision"));
                double[] lo = null, hi = null;
                if (cb) L.CollisionBounds(mname, out lo, out hi, cancel);
                var f = new UxMeshFixes();
                results[i] = ConvertModel(L, mname, cb, lo, hi, options, f, null, "all", cancel);
                modelFixes[i] = f;
                progress?.Invoke("convert", Interlocked.Increment(ref done), names.Count);
            });

            var meshes = new JObj(); var skipped = new JObj(); var minfo = new List<KeyValuePair<string, UxMeshModel>>();
            var fixes = new UxMeshFixes();
            var files = new SortedDictionary<string, byte[]>(StringComparer.Ordinal);
            var warnings = new List<string>();
            var namesCi = new Dictionary<string, string>(StringComparer.Ordinal);
            var plist = new List<(string model, UxMeshPart part, JObj json)>();
            for (int i = 0; i < names.Count; i++)
            {
                string mname = names[i]; var r = results[i];
                fixes.Add(modelFixes[i]);
                if (r.skipped != null) { skipped.Set(mname, r.skipped); continue; }
                var parts = new List<object>();
                foreach (var p in r.parts)
                {
                    string low = UxCommon.PyLower(p.part);
                    if (namesCi.TryGetValue(low, out string owner) && owner != mname) throw new UxException("part name " + p.part + " of " + mname + " collides with a part of " + owner);
                    namesCi[low] = mname;
                    var j = p.ToJson();
                    j.Set("files", new JObj().Set("surface", p.part + ".surface").Set("model", p.part + ".model"));
                    j.Set("md5", new JObj().Set("surface", UxBin.Md5(p.surface_bytes)).Set("model", UxBin.Md5(p.model_bytes)));
                    j.Set("surface_size", (long)p.surface_bytes.Length).Set("model_size", (long)p.model_bytes.Length);
                    files[p.part + ".surface"] = p.surface_bytes; files[p.part + ".model"] = p.model_bytes;
                    parts.Add(j); plist.Add((mname, p, j));
                }
                meshes.items.Add(new KeyValuePair<string, object>(mname, parts));
                minfo.Add(new KeyValuePair<string, UxMeshModel>(mname, r));
                bool anyCell = r.parts.Exists(p => p.kind == "cell");
                if (UxJson.Bool(r.info.Get("collision_bearing")) && anyCell) warnings.Add(mname + ": collision-bearing model with re-centred cell parts");
            }
            if (Unconverted(L.level) is JObj unconv)
            {
                var un = new List<KeyValuePair<string, object>>(unconv.items);
                UxNp.StableSort(un, (a, b) => string.CompareOrdinal(a.Key, b.Key));
                foreach (var kv in un)
                {
                    var u = UxJson.Obj(kv.Value);
                    var ents = UxJson.Arr(u.Get("entities")); var types = new List<string>();
                    foreach (object t in UxJson.Arr(u.Get("types"))) types.Add(UxJson.Str(t));
                    types.Sort(StringComparer.Ordinal);
                    string what = (UxMeshLevel.Truthy(u.Get("boned")) ? "rigid multi-node model (" : "skinned model (") + UxJson.Int(u.Get("nodes")).ToString(Inv) + " nodes)";
                    var first = new List<string>();
                    for (int i = 0; i < ents.Count && i < 12; i++) first.Add(PyStr(ents[i]));
                    skipped.Set(kv.Key, what + ": not converted (DESIGN 5.4); " + ents.Count.ToString(Inv) + " entities lose their model (" + string.Join(", ", types) + "): " +
                        string.Join(" ", first) + (ents.Count > 12 ? " ..." : ""));
                }
            }
            // part names must not collide with other Untamed models / surfaces of the level (the extract index = the disc's assets)
            {
                var known = new HashSet<string>(StringComparer.Ordinal);
                foreach (var a in disc.assets) if (a.type == "model" || a.type == "surface") known.Add(UxCommon.PyLower(a.name));
                foreach (var (mname, p, _) in plist)
                    if (p.part != mname && known.Contains(UxCommon.PyLower(p.part))) warnings.Add("part name " + p.part + " collides with an Untamed model/surface asset");
            }
            meshes.Set("skipped", skipped);

            // materials_uv: per material name, UV statistics of the groups drawn with it (any entity's MaterialList)
            var matUv = new SortedDictionary<string, MatUv>(StringComparer.Ordinal);
            foreach (var kv in minfo)
            {
                var lists = new List<List<string>>(); var seenLists = new HashSet<string>(StringComparer.Ordinal);
                foreach (var e in L.ModelEntities(kv.Key))
                {
                    var ml = UxMeshLevel.MaterialList(e);
                    var key = new System.Text.StringBuilder();
                    foreach (string s in ml) key.Append(s == null ? "\u0001" : "\u0002" + s).Append('\u0003');
                    if (seenLists.Add(key.ToString())) lists.Add(ml);
                }
                foreach (var u in kv.Value.uv)
                    foreach (var ml in lists)
                    {
                        string mat = u.ml_index < ml.Count ? ml[u.ml_index] : null;
                        if (string.IsNullOrEmpty(mat)) continue;
                        if (!matUv.TryGetValue(mat, out var s)) matUv[mat] = s = new MatUv();
                        long n0 = s.verts;
                        s.groups++; s.verts += u.verts;
                        s.uv1_in01 = (s.uv1_in01 * n0 + u.uv1_in01 * u.verts) / s.verts;
                        s.uv1_eq_uv0 = (s.uv1_eq_uv0 * n0 + u.uv1_eq_uv0 * u.verts) / s.verts;
                        s.uv0_span_max = Math.Max(s.uv0_span_max, u.uv0_span); s.uv1_span_max = Math.Max(s.uv1_span_max, u.uv1_span);
                        s.models.Add(kv.Key);
                    }
            }
            var matUvJson = new JObj();
            foreach (var kv in matUv)
                matUvJson.items.Add(new KeyValuePair<string, object>(kv.Key, new JObj().Set("groups", kv.Value.groups).Set("verts", kv.Value.verts).Set("uv1_in01", kv.Value.uv1_in01)
                    .Set("uv1_eq_uv0", kv.Value.uv1_eq_uv0).Set("uv0_span_max", kv.Value.uv0_span_max).Set("uv1_span_max", kv.Value.uv1_span_max).Set("models", new List<string>(kv.Value.models))));

            long Sum(Func<UxMeshPart, long> f) { long s = 0; foreach (var x in plist) s += f(x.part); return s; }
            long SumInfo(Func<JObj, long> f) { long s = 0; foreach (var kv in minfo) s += f(kv.Value.info); return s; }
            long extraP = 0; foreach (var x in plist) if (x.part.part.Substring(x.model.Length).Contains("_p")) extraP++;
            long maxGv = 0, maxGi = 0, maxGroups = 0;
            foreach (var x in plist)
            {
                foreach (int n in x.part.group_verts) maxGv = Math.Max(maxGv, n);
                foreach (int n in x.part.group_indices) maxGi = Math.Max(maxGi, n);
                maxGroups = Math.Max(maxGroups, x.part.groups.Length);
            }
            var counts = new JObj().Set("models", (long)minfo.Count).Set("skipped", (long)skipped.items.Count).Set("parts", (long)plist.Count)
                .Set("base_parts", Sum(p => p.kind == "base" ? 1 : 0)).Set("extra_p_parts", extraP).Set("cell_parts", Sum(p => p.kind == "cell" ? 1 : 0))
                .Set("two_stream_models", SumInfo(i => ((List<long>)i.Get("float3_groups")).Count > 0 ? 1 : 0)).Set("two_stream_groups", SumInfo(i => ((List<long>)i.Get("float3_groups")).Count))
                .Set("source_groups", SumInfo(i => (long)i.Get("source_groups"))).Set("output_groups", Sum(p => p.groups.Length))
                .Set("groups_split", SumInfo(i => ((List<object>)i.Get("split_groups")).Count))
                .Set("split_pieces", SumInfo(i => { long s = 0; foreach (object sg in (List<object>)i.Get("split_groups")) s += ((long[])sg)[3]; return s; }))
                .Set("crowd_models", SumInfo(i => (bool)i.Get("crowd") ? 1 : 0)).Set("collision_bearing_models", SumInfo(i => (bool)i.Get("collision_bearing") ? 1 : 0))
                .Set("surface_bytes", Sum(p => p.surface_bytes.Length)).Set("model_bytes", Sum(p => p.model_bytes.Length))
                .Set("verts", Sum(p => p.verts)).Set("indices", Sum(p => p.indices)).Set("tris", Sum(p => p.tris))
                .Set("max_group_verts", maxGv).Set("max_group_indices", maxGi).Set("max_groups_per_model", maxGroups);
            var models = new JObj();
            foreach (var kv in minfo) models.items.Add(new KeyValuePair<string, object>(kv.Key, kv.Value.info));
            var info = new JObj().Set("schema", SCHEMA).Set("level", lv.disc).Set("short", lv.shortName)
                .Set("limits", new long[] { options.maxGroupVerts, options.maxGroupStripIndices, options.maxGroupsPerModel })
                .Set("cell_ft", CELL_FT).Set("origin_step", ORIGIN_STEP).Set("crowd_pad", (double[])CROWD_PAD.Clone())
                .Set("source", new JObj().Set("level_json_md5", UxBin.Md5(levelBytes)))
                .Set("tool_md5s", options.toolMd5s ?? new JObj())
                .Set("counts", counts).Set("fixes", fixes.ToJson()).Set("models", models).Set("materials_uv", matUvJson).Set("warnings", warnings).Set("checks", null);
            var result = new UxMeshesResult { meshes = meshes, info = info, convertSeconds = clock.Elapsed.TotalSeconds };
            if (options.write)
            {
                // stale part files from an earlier run
                foreach (string rel in work.List("meshes"))
                {
                    string fn = rel.Substring(rel.LastIndexOf('/') + 1);
                    if (fn == "meshes.json" || fn == "meshes_info.json" || files.ContainsKey(fn)) continue;
                    if (fn.EndsWith(".surface", StringComparison.Ordinal) || fn.EndsWith(".model", StringComparison.Ordinal)) work.Delete(rel);
                }
                int written = 0;
                foreach (var kv in files)
                {
                    cancel.ThrowIfCancellationRequested();
                    string rel = "meshes/" + kv.Key;
                    if (!(work.Exists(rel) && work.Length(rel) == kv.Value.Length && UxBin.Same(work.ReadBytes(rel), kv.Value))) work.WriteBytes(rel, kv.Value);
                    progress?.Invoke("write", ++written, files.Count);
                }
                work.WriteJson("meshes/meshes.json", meshes);
                if (options.check)
                {
                    var t1 = System.Diagnostics.Stopwatch.StartNew();
                    var probs = VerifyLevel(L, work.Sub("meshes"), options, out JObj stats, progress, cancel);
                    info.Set("checks", new JObj().Set("problems", probs).Set("stats", stats).Set("result", probs.Count == 0 ? "PASS" : "FAIL"));
                    result.problems = probs; result.checkSeconds = t1.Elapsed.TotalSeconds;
                }
                work.WriteJson("meshes/meshes_info.json", info);
            }
            log?.Invoke(lv.shortName + " meshes done in " + clock.Elapsed.TotalSeconds.ToString("0.0", Inv) + " s");
            if (options.failOnProblems && result.problems != null && result.problems.Count > 0)
                throw new UxException("ux_meshes checks: " + string.Join("; ", result.problems.GetRange(0, Math.Min(5, result.problems.Count))));
            return result;
        }
        static object Unconverted(JObj level) { object u = level.Get("unconverted_models"); return UxMeshLevel.Truthy(u) ? u : null; }
        static string PyStr(object v) => v is string s ? s : v is double d ? PyFormat.Repr(d) : v is bool b ? (b ? "True" : "False") : v == null ? "None" : System.Convert.ToString(v, Inv);
        sealed class MatUv { public long groups, verts; public double uv1_in01, uv1_eq_uv0, uv0_span_max, uv1_span_max; public SortedSet<string> models = new SortedSet<string>(StringComparer.Ordinal); }

        /// Parallel.For that rethrows the first failure as it was thrown (not wrapped), and a cancellation as OperationCanceledException.
        static void RunParallel(int n, ParallelOptions po, Action<int> body)
        {
            try { Parallel.For(0, n, po, body); }
            catch (AggregateException ae)
            {
                var inner = ae.Flatten().InnerExceptions;
                foreach (var e in inner) if (e is OperationCanceledException) throw e;
                foreach (var e in inner) if (e is UxException) throw e;
                throw new UxException(inner[0].Message, inner[0]);
            }
        }

        // =================================================================================================== verification
        /// _raw_source_elements: an independent parse of an Untamed surface: per group the raw position element (F16_4 -> (n,4) uint16
        /// bits, FLOAT3 -> (n,3) float32) and the raw uv0 / uv1 bits.
        sealed class Raw { public uint ptype; public ushort[] pos16; public float[] pos32; public ushort[] uv0, uv1; public int nv; }
        static List<Raw> RawSourceElements(byte[] data)
        {
            int ng = (int)UxBin.U32BE(data, 8);
            long p = 0x10 + (long)ng * 0x160;
            UxBin.Require(p <= data.Length, "independent parse: group records outside the file");
            var o = new List<Raw>(ng);
            for (int gi = 0; gi < ng; gi++)
            {
                int r = 0x10 + gi * 0x160;
                uint flags = UxBin.U32BE(data, r + 8); int nv = (int)UxBin.U32BE(data, r + 16), ib = (int)UxBin.U32BE(data, r + 20);
                var decl = new List<(int st, int off, uint ty, int us, int ui)>();
                for (int q = 0x18; q + 12 <= 0x108; q += 12)
                {
                    int st = UxBin.U16BE(data, r + q), off = UxBin.U16BE(data, r + q + 2); uint ty = UxBin.U32BE(data, r + q + 4);
                    if (st == 0xFF || ty == 0xFFFFFFFF) break;
                    decl.Add((st, off, ty, data[r + q + 9], data[r + q + 10]));
                }
                int ns = (int)UxBin.U32BE(data, r + 0x108);
                UxBin.Require(ns >= 0 && 0x10C + 4 * (long)ns <= 0x160, "independent parse: stream count");
                int npal = flags != 0 ? (int)UxBin.U32BE(data, r + 0x14C) : 0;
                p += npal + ib;
                var start = new long[ns]; var stride = new int[ns];
                for (int i = 0; i < ns; i++)
                {
                    int b = (int)UxBin.U32BE(data, r + 0x10C + 4 * i);
                    start[i] = p; stride[i] = nv != 0 ? b / nv : 0; p += b;
                    UxBin.Require(p <= data.Length && (nv == 0 || stride[i] * nv == b), "independent parse: vertex stream");
                }
                var e = new Raw { nv = nv };
                foreach (var (st, off, ty, us, ui) in decl)
                {
                    UxBin.Require(st < ns, "independent parse: stream index");
                    int at = (int)start[st] + off, sd = stride[st];
                    if (us == 0)
                    {
                        e.ptype = ty;
                        if (ty == XF16_4) { e.pos16 = new ushort[nv * 4]; for (int i = 0; i < nv; i++) for (int c = 0; c < 4; c++) e.pos16[4 * i + c] = UxBin.U16BE(data, at + i * sd + 2 * c); }
                        else if (ty == XFLOAT3) { e.pos32 = new float[nv * 3]; for (int i = 0; i < nv; i++) for (int c = 0; c < 3; c++) e.pos32[3 * i + c] = UxBin.F32BE(data, at + i * sd + 4 * c); }
                    }
                    else if (us == 5 && ty == XF16_2 && (ui == 0 || ui == 1))
                    {
                        var uv = new ushort[nv * 2];
                        for (int i = 0; i < nv; i++) for (int c = 0; c < 2; c++) uv[2 * i + c] = UxBin.U16BE(data, at + i * sd + 2 * c);
                        if (ui == 0) e.uv0 = uv; else e.uv1 = uv;
                    }
                }
                o.Add(e);
            }
            if (p != data.Length) throw new UxException("independent parse: " + (data.Length - p) + " bytes left");
            return o;
        }

        /// A vertex record (a row of the structured array) as a dictionary key.
        readonly struct Row : IEquatable<Row>
        {
            readonly byte[] _b; readonly int _at, _n, _hash;
            public Row(byte[] b, int at, int n)
            {
                _b = b; _at = at; _n = n;
                uint h = 2166136261;
                for (int i = 0; i < n; i++) h = (h ^ b[at + i]) * 16777619;
                _hash = (int)h;
            }
            public bool Equals(Row o)
            {
                if (_hash != o._hash || _n != o._n) return false;
                for (int i = 0; i < _n; i++) if (_b[_at + i] != o._b[o._at + i]) return false;
                return true;
            }
            public override bool Equals(object obj) => obj is Row r && Equals(r);
            public override int GetHashCode() => _hash;
        }

        /// _canon: a triangle as its lexicographically smallest rotation (orientation kept).
        static (int, int, int) Canon3(int a, int b, int c)
        {
            var best = (a, b, c);
            var r1 = (b, c, a);
            if (r1.CompareTo(best) < 0) best = r1;
            var r2 = (c, a, b);
            if (r2.CompareTo(best) < 0) best = r2;
            return best;
        }

        sealed class VOut { public string part; public double[] o; public ushort[] strip; public byte[] verts; public string layout; }
        sealed class VStats
        {
            public readonly List<string> problems = new List<string>(); public readonly Dictionary<string, long> st = new Dictionary<string, long>(StringComparer.Ordinal);
            public double maxErr, nmax; public long surfTotal;
            public void Inc(string k, long n = 1) { st.TryGetValue(k, out long v); st[k] = v + n; }
        }

        /// verify_level(level, limits, out_dir): reads every written part file again (the store `outDir` = work/<Level>/meshes) and
        /// checks it against the Untamed source. Returns the problems (empty = PASS) and the statistics.
        public static List<string> VerifyLevel(UxMeshLevel L, UxWork outDir, UxMeshesOptions options, out JObj stats, Action<string, int, int> progress = null, CancellationToken cancel = default)
        {
            options = options ?? new UxMeshesOptions();
            var meshes = outDir.ReadJsonObj("meshes.json");
            var todo = new List<KeyValuePair<string, object>>();
            foreach (var kv in meshes.items) if (kv.Key != "skipped") todo.Add(kv);
            var per = new VStats[todo.Count]; int done = 0;
            var po = new ParallelOptions { MaxDegreeOfParallelism = Threads(options), CancellationToken = cancel };
            RunParallel(todo.Count, po, i =>
            {
                var vs = new VStats();
                VerifyModel(L, outDir, options, todo[i].Key, UxJson.Arr(todo[i].Value), vs, cancel);
                per[i] = vs;
                progress?.Invoke("check", Interlocked.Increment(ref done), todo.Count);
            });
            var probs = new List<string>(); var st = new SortedDictionary<string, long>(StringComparer.Ordinal);
            double maxErr = 0.0, nmax = 0.0; long surfTotal = 0;
            foreach (var vs in per)
            {
                probs.AddRange(vs.problems);
                foreach (var kv in vs.st) { st.TryGetValue(kv.Key, out long v); st[kv.Key] = v + kv.Value; }
                maxErr = Math.Max(maxErr, vs.maxErr); nmax = Math.Max(nmax, vs.nmax); surfTotal += vs.surfTotal;
            }
            if (surfTotal > UxCommon.Limits.MaxSurfaceBytes) probs.Add("surface bytes " + surfTotal + " > " + UxCommon.Limits.MaxSurfaceBytes);
            stats = new JObj();
            foreach (var kv in st) stats.items.Add(new KeyValuePair<string, object>(kv.Key, kv.Value));
            stats.Set("surface_bytes", surfTotal).Set("max_recentre_err_ft", maxErr).Set("normal_max_dev", nmax);
            return probs;
        }

        static int[] IntList(object v) { var l = UxJson.Arr(v); var o = new int[l.Count]; for (int i = 0; i < o.Length; i++) o[i] = (int)UxJson.Int(l[i]); return o; }
        static string Fmt(double v, string f) => v.ToString(f, Inv);
        /// '%.3g' % v.
        static string PyG3(double v)
        {
            if (double.IsNaN(v)) return "nan";
            if (double.IsInfinity(v)) return v > 0 ? "inf" : "-inf";
            if (v == 0) return (1 / v) < 0 ? "-0" : "0";
            string e = v.ToString("E2", Inv);                       // d.ddE+XXX
            int at = e.IndexOf('E'); int x = int.Parse(e.Substring(at + 1), Inv);
            string mant = e.Substring(0, at);
            if (x < -4 || x >= 3)
            {
                if (mant.Contains(".")) mant = mant.TrimEnd('0').TrimEnd('.');
                return mant + "e" + (x < 0 ? "-" : "+") + Math.Abs(x).ToString("00", Inv);
            }
            string f = v.ToString("F" + (2 - x).ToString(Inv), Inv);
            return f.Contains(".") ? f.TrimEnd('0').TrimEnd('.') : f;
        }

        static void VerifyModel(UxMeshLevel L, UxWork outDir, UxMeshesOptions options, string mname, List<object> parts, VStats vs, CancellationToken cancel)
        {
            int maxV = options.maxGroupVerts, maxI = options.maxGroupStripIndices, maxG = options.maxGroupsPerModel;
            var probs = vs.problems;
            vs.Inc("models");
            JObj mrec = L.Record("model", mname);
            string skey = L.Ci("surface", mname);
            if (skey == null) throw new UxException(mname + ": no surface in the level closure");
            byte[] sdata = L.disc.ReadExtractFile(UxJson.Str(L.Record("surface", skey).Get("file")), cancel);
            List<UxGroup> src = UxMesh.ReadSurface(sdata);
            List<Raw> raw = RawSourceElements(sdata);
            byte[] mdataSrc = L.disc.ReadExtractFile(UxJson.Str(mrec.Get("file")), cancel);
            UxModel mSrc = UxMesh.ReadModel(mdataSrc);
            if (mSrc.skinned && mSrc.boned) mSrc = UxMesh.ReadModelRigid(mdataSrc);        // fix round 2: rigid static (see ConvertModel)
            var lays = new int[src.Count]; bool anyFloat3 = false;
            for (int i = 0; i < src.Count; i++) { lays[i] = GroupLayout(src[i]); anyFloat3 |= lays[i] == FLOAT3; }
            bool cb = UxMeshLevel.Truthy(mrec.Get("with_collision"));
            double[] cbLo = null, cbHi = null;
            if (cb) L.CollisionBounds(mname, out cbLo, out cbHi, cancel);
            bool baked = false;
            foreach (object po in parts) baked |= UxMeshLevel.Truthy(UxJson.Obj(po).Get("pivot_baked"));
            float[] piv = mSrc.matrix;
            if (baked != (anyFloat3 && !CloseToIdentity(piv, 4, 4, 0))) probs.Add(mname + ": pivot baking flag does not follow the rule");
            // independent checks of the decoder on the source: F16 positions and UVs survive float32 -> float16 exactly
            for (int si = 0; si < src.Count && si < raw.Count; si++)
            {
                UxGroup g = src[si]; Raw e = raw[si];
                if (e.ptype == XF16_4)
                {
                    bool same = e.pos16 != null && g.pos != null && e.nv == g.nverts;
                    for (int i = 0; same && i < g.nverts; i++)
                        same = UxNp.FloatToHalf(g.pos[3 * i]) == e.pos16[4 * i] && UxNp.FloatToHalf(g.pos[3 * i + 1]) == e.pos16[4 * i + 1] && UxNp.FloatToHalf(g.pos[3 * i + 2]) == e.pos16[4 * i + 2] &&
                            UxNp.FloatToHalf(g.pos_w != null ? g.pos_w[i] : 1f) == e.pos16[4 * i + 3];
                    if (!same) probs.Add(mname + " g" + si + ": decoded F16 positions differ from the raw bits");
                }
                else if (e.ptype == XFLOAT3)
                {
                    bool same = e.pos32 != null && g.pos != null && e.pos32.Length == g.pos.Length;
                    for (int i = 0; same && i < g.pos.Length; i++) same = g.pos[i] == e.pos32[i];
                    if (!same) probs.Add(mname + " g" + si + ": decoded FLOAT3 positions differ from the raw values");
                }
                for (int k = 0; k < 2; k++)
                {
                    UxVal a = k == 0 ? g.uv0 : g.uv1; ushort[] rb = k == 0 ? e.uv0 : e.uv1;
                    if (rb == null || a == null || a.f == null || a.comps < 2) continue;
                    bool same = true;
                    for (int i = 0; same && i < g.nverts; i++)
                    {
                        float u = a.f[i * a.comps], w = a.f[i * a.comps + 1];
                        if (!Finite(u) || !Finite(w)) continue;
                        same = UxNp.FloatToHalf(u) == rb[2 * i] && UxNp.FloatToHalf(w) == rb[2 * i + 1];
                    }
                    if (!same) probs.Add(mname + " g" + si + ": decoded uv" + k + " differs from the raw bits");
                }
            }
            // output files
            var outsBySrc = new Dictionary<int, List<VOut>>();
            int collParts = 0;
            foreach (object po in parts)
            {
                cancel.ThrowIfCancellationRequested();
                JObj p = UxJson.Obj(po);
                string pname = UxJson.Str(p.Get("part"));
                vs.Inc("parts");
                byte[] sb = outDir.ReadBytes(UxJson.Str(UxJson.Get(p, "files", "surface"))), mb = outDir.ReadBytes(UxJson.Str(UxJson.Get(p, "files", "model")));
                vs.surfTotal += sb.Length;
                if (UxBin.Md5(sb) != UxJson.Str(UxJson.Get(p, "md5", "surface")) || UxBin.Md5(mb) != UxJson.Str(UxJson.Get(p, "md5", "model"))) probs.Add(pname + ": md5 differs from meshes.json");
                List<PcGroup> rg;
                try { rg = ReadSurfacePc(sb); }
                catch (Exception ex) { probs.Add(pname + ": surface does not re-read: " + ex.Message); continue; }
                if (rg.TrueForAll(x => x.layout == "static"))
                {
                    try
                    {
                        var g2 = UxRmesh.ReadSurface(sb, out byte[] tail);
                        bool same = tail.Length == 0 && g2.Count == rg.Count;
                        for (int i = 0; same && i < g2.Count; i++)
                        {
                            same = g2[i].strip.Length == rg[i].strip.Length && UxBin.Same(g2[i].verts, rg[i].verts);
                            for (int k = 0; same && k < g2[i].strip.Length; k++) same = g2[i].strip[k] == rg[i].strip[k];
                        }
                        if (!same) probs.Add(pname + ": rmesh.read_surface disagrees with read_surface_pc");
                        else vs.Inc("surfaces_rmesh");
                    }
                    catch (Exception ex) { probs.Add(pname + ": rmesh.read_surface failed: " + ex.Message); }
                    var sg = new List<UxSurfaceGroup>(rg.Count);
                    foreach (var x in rg) sg.Add(new UxSurfaceGroup(x.strip, x.verts));
                    if (!UxBin.Same(UxRmesh.WriteSurface(sg), sb)) probs.Add(pname + ": rmesh.write_surface does not reproduce the file");
                }
                else vs.Inc("surfaces_crowd_reader");
                if (!UxBin.Same(WriteSurfacePc(rg), sb)) probs.Add(pname + ": surface does not re-encode byte-identically");
                UxPcModel mm;
                try { mm = UxRmesh.ReadModel(mb); }
                catch (Exception ex) { probs.Add(pname + ": model does not re-read with rmesh: " + ex.Message); continue; }
                if (!UxBin.Same(UxRmesh.WriteModel(mm), mb)) probs.Add(pname + ": model does not re-encode byte-identically");
                int ng = rg.Count;
                int[] pGroups = IntList(p.Get("groups")); var pLayouts = UxJson.Arr(p.Get("layouts")); var pMats = UxJson.Arr(p.Get("materials"));
                int nMap = UxJson.Arr(p.Get("material_index_map")).Count;
                if (!(mm.materials.Count == ng && ng == pGroups.Length && ng == nMap))
                    probs.Add(pname + ": model groups " + mm.materials.Count + ", surface groups " + ng + ", meshes.json " + pGroups.Length);
                if (ng > maxG) probs.Add(pname + ": " + ng + " groups > " + maxG);
                if (!(mm.lods.Count == 1 && mm.lods[0][0] == (uint)ng && mm.lods[0][1] == 0 && mm.lods[0][2] == 0)) {
                    var ls = new List<string>(); foreach (var l in mm.lods) ls.Add("(" + l[0] + ", " + l[1] + ", " + l[2] + ")");
                    probs.Add(pname + ": lods [" + string.Join(", ", ls) + "]");
                }
                bool matsSame = mm.materials.Count == pMats.Count;
                for (int i = 0; matsSame && i < pMats.Count; i++) matsSame = mm.materials[i] == (pMats[i] as string);
                if (mm.name != pname || !matsSame) probs.Add(pname + ": model name/materials differ from meshes.json");
                long wantDd = cb ? 0 : mSrc.draw_distance;
                if (mm.draw_distance != wantDd || UxJson.Int(p.Get("draw_distance")) != wantDd) probs.Add(pname + ": draw distance " + mm.draw_distance + ", expected " + wantDd);
                if (cb && mm.draw_distance != 0) probs.Add(pname + ": collision-bearing model with draw distance");
                bool pBaked = UxMeshLevel.Truthy(p.Get("pivot_baked"));
                bool pivSame = true;
                for (int i = 0; pivSame && i < 16; i++) pivSame = mm.matrix[i] == (pBaked ? (i % 5 == 0 ? 1f : 0f) : piv[i]);
                if (!pivSame) probs.Add(pname + ": pivot matrix is not the " + (pBaked ? "identity (baked)" : "source pivot"));
                double[] origin = new double[3];
                { var oo = UxJson.Arr(p.Get("origin_offset")); for (int k = 0; k < 3; k++) origin[k] = UxJson.Num(oo[k]); }
                for (int k = 0; k < ng; k++)
                {
                    var x = rg[k]; int stride = x.layout == "crowd" ? UxMesh.CROWD_STRIDE : UxMesh.VTX_STRIDE, nv = x.VertexCount;
                    if (nv > maxV || x.strip.Length > maxI) probs.Add(pname + " g" + k + ": " + nv + " verts / " + x.strip.Length + " indices over the limit");
                    vs.Inc("groups_out");
                    if (nv > 0)
                    {
                        bool fin = true;
                        for (int i = 0; fin && i < nv; i++) for (int c = 0; c < 4; c++) if ((UxBin.U16LE(x.verts, i * stride + 2 * c) & 0x7C00) == 0x7C00) fin = false;
                        if (!fin) probs.Add(pname + " g" + k + ": non-finite positions");
                        // P0 (DESIGN 18): unit normals as written (float64)
                        double dmax = 0; bool finite = true; int over = 0;
                        for (int i = 0; i < nv; i++)
                        {
                            double a = UxBin.F32LE(x.verts, i * stride + 8), b = UxBin.F32LE(x.verts, i * stride + 12), c = UxBin.F32LE(x.verts, i * stride + 16);
                            double dev = Math.Abs(((a * a + b * b) + c * c) - 1.0);
                            if (double.IsNaN(dev) || double.IsInfinity(dev)) finite = false; else if (dev > dmax) dmax = dev;
                            if (!(dev < NORMAL_TOL)) over++;
                        }
                        if (!finite) dmax = double.PositiveInfinity;
                        vs.Inc("normals_checked", nv);
                        vs.nmax = Math.Max(vs.nmax, dmax);
                        if (dmax >= NORMAL_TOL) probs.Add(pname + " g" + k + ": normal |n^2-1| max " + PyG3(dmax) + " >= 1e-05 (" + over + " vertices)");
                    }
                    if (k < pLayouts.Count && (pLayouts[k] as string) != x.layout) probs.Add(pname + " g" + k + ": layout " + x.layout + ", meshes.json " + pLayouts[k]);
                    if (k < pGroups.Length)
                    {
                        if (!outsBySrc.TryGetValue(pGroups[k], out var list)) outsBySrc[pGroups[k]] = list = new List<VOut>();
                        list.Add(new VOut { part = pname, o = origin, strip = x.strip, verts = x.verts, layout = x.layout });
                    }
                }
                // bounds cover the vertices (+ collision for the carrying part)
                double[] c4 = { mm.center4[0], mm.center4[1], mm.center4[2] }, e4 = { mm.extents4[0], mm.extents4[1], mm.extents4[2] };
                int total = 0; foreach (var x in rg) total += x.VertexCount;
                if (total > 0)
                {
                    var P = new double[total * 3]; int at = 0;
                    foreach (var x in rg) { int st = x.layout == "crowd" ? UxMesh.CROWD_STRIDE : UxMesh.VTX_STRIDE, n = x.VertexCount; for (int i = 0; i < n; i++) for (int k = 0; k < 3; k++) P[at++] = HalfAt(x.verts, i * st + 2 * k); }
                    if (!CloseToIdentity(mm.matrix, 4, 3, 1e-6))
                    {
                        var R = new double[9]; for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) R[3 * i + j] = mm.matrix[4 * i + j];
                        P = UxNp.MatMul(P, total, 3, R, 3);
                    }
                    bool cover = true;
                    for (int i = 0; cover && i < P.Length; i++) { int k = i % 3; if (P[i] < c4[k] - e4[k] - 1e-3 || P[i] > c4[k] + e4[k] + 1e-3) cover = false; }
                    if (!cover) probs.Add(pname + ": bounds do not cover the vertices");
                }
                if (Math.Abs(mm.radius - UxNp.Norm(e4)) > 1e-3 * Math.Max(1.0, mm.radius)) probs.Add(pname + ": radius is not |extents|");
                if (UxMeshLevel.Truthy(p.Get("carries_collision")))
                {
                    collParts++;
                    if (cbLo != null)
                    {
                        bool cover = true;
                        for (int k = 0; k < 3; k++) if (cbLo[k] - origin[k] < c4[k] - e4[k] - 1e-3 || cbHi[k] - origin[k] > c4[k] + e4[k] + 1e-3) cover = false;
                        if (!cover) probs.Add(pname + ": bounds do not cover the collision");
                    }
                }
            }
            if (cb && collParts != 1) probs.Add(mname + ": " + collParts + " parts carry collision (expected 1)");
            if (!cb && collParts != 0) probs.Add(mname + ": part carries collision but the model is not collision-bearing");
            // per source group: triangle set and winding (vertex records = the expected conversion with the part origin)
            double[] Rp = new double[9], tp = new double[3];
            for (int i = 0; i < 3; i++) { for (int j = 0; j < 3; j++) Rp[3 * i + j] = piv[4 * i + j]; tp[i] = piv[12 + i]; }
            for (int si = 0; si < src.Count; si++)
            {
                cancel.ThrowIfCancellationRequested();
                UxGroup g = src[si]; int lay = lays[si];
                outsBySrc.TryGetValue(si, out var outs);
                vs.Inc("groups_src");
                int[] srcT = g.tris; int nt = srcT.Length / 3;
                if (outs == null || outs.Count == 0)
                {
                    if (nt > 0) probs.Add(mname + " g" + si + ": " + nt + " source triangles but no output group");
                    continue;
                }
                var used = new bool[nt];
                // The "frame" of an origin (Python keeps one per origin: frames[tuple(o)]): the expected conversion of the whole group
                // with that origin, an id per distinct vertex record, and per canonical id triple the source triangles in ascending
                // order (head / next: a linked list; a taken or used triangle is skipped). One frame is held at a time and built again
                // when the origin changes: `used` carries the state, so a frame built again gives what Python's kept one gives.
                Src expSrc = null; double[] refPos = null; byte[] exp = null; bool haveFrame = false; (double, double, double) cur = (0, 0, 0);
                var ids = new Dictionary<Row, int>(g.nverts); var vid = new int[g.nverts];
                var head = new Dictionary<(int, int, int), int>(nt); var next = new int[nt];
                int[] seen = lay == FLOAT3 ? new int[g.nverts] : null; int seenGen = 0;
                foreach (var x in outs)
                {
                    double[] o = x.o; var fk = (o[0], o[1], o[2]);
                    int stride = lay == CROWD ? UxMesh.CROWD_STRIDE : UxMesh.VTX_STRIDE;
                    if (!haveFrame || !fk.Equals(cur))
                    {
                        if (exp == null)
                        {
                            expSrc = baked && lay == FLOAT3 ? BakePivot(g, piv) : new Src { g = g, pos64 = lay == FLOAT3 ? ToDouble(g.pos) : null };
                            exp = ConvertVertices(expSrc, lay, o[0], o[1], o[2], null);
                        }
                        else if (lay != CROWD) Reposition(exp, expSrc, o[0], o[1], o[2]);
                        int n = exp.Length / stride;
                        ids.Clear();
                        for (int i = 0; i < n; i++)
                        {
                            var row = new Row(exp, i * stride, stride);
                            if (!ids.TryGetValue(row, out int id)) ids[row] = id = ids.Count;
                            vid[i] = id;
                        }
                        head.Clear();
                        for (int ti = nt - 1; ti >= 0; ti--)
                        {
                            var tt = Canon3(vid[srcT[3 * ti]], vid[srcT[3 * ti + 1]], vid[srcT[3 * ti + 2]]);
                            next[ti] = head.TryGetValue(tt, out int h) ? h : -1;
                            head[tt] = ti;
                        }
                        cur = fk; haveFrame = true;
                    }
                    int xstride = x.layout == "crowd" ? UxMesh.CROWD_STRIDE : UxMesh.VTX_STRIDE, xn = x.verts.Length / xstride;
                    var oid = new int[xn];
                    for (int i = 0; i < xn; i++) oid[i] = ids.TryGetValue(new Row(x.verts, i * xstride, xstride), out int id) ? id : -1;
                    int[] ot = UxRmesh.StripToTris(x.strip);
                    bool neg = false; for (int i = 0; !neg && i < ot.Length; i++) neg = oid[ot[i]] < 0;
                    if (ot.Length > 0 && neg)
                    {
                        var bad = new HashSet<int>(); foreach (int vi in ot) if (oid[vi] < 0) bad.Add(vi);
                        probs.Add(mname + " g" + si + " (" + x.part + "): " + bad.Count + " output vertices are not a conversion of a source vertex");
                        continue;
                    }
                    int miss = 0; var mine = new List<int>(ot.Length / 3);
                    for (int i = 0; i + 2 < ot.Length; i += 3)
                    {
                        var tt = Canon3(oid[ot[i]], oid[ot[i + 1]], oid[ot[i + 2]]);
                        if (!head.TryGetValue(tt, out int ti)) { miss++; continue; }
                        while (ti >= 0 && used[ti]) ti = next[ti];
                        if (ti < 0) { head[tt] = -1; miss++; continue; }
                        used[ti] = true; mine.Add(ti); head[tt] = next[ti];
                    }
                    if (miss > 0) probs.Add(mname + " g" + si + " (" + x.part + "): " + miss + " output triangles have no unused source match");
                    if (lay == FLOAT3)
                    {
                        // the re-centring error of every vertex the part uses: f16(p - o) + o against the FLOAT3 source (independent parse)
                        if (refPos == null)
                        {
                            UxBin.Require(raw[si].pos32 != null, mname + " g" + si + ": no raw FLOAT3 positions");
                            refPos = ToDouble(raw[si].pos32);
                            if (baked) { refPos = UxNp.MatMul(refPos, refPos.Length / 3, 3, Rp, 3); for (int i = 0; i < refPos.Length; i++) refPos[i] += tp[i % 3]; }
                        }
                        double e = 0.0; bool any = false;
                        seenGen++;
                        foreach (int ti in mine)
                            for (int k = 0; k < 3; k++)
                            {
                                int vi = srcT[3 * ti + k];
                                if (seen[vi] == seenGen) continue;
                                seen[vi] = seenGen;
                                for (int c = 0; c < 3; c++)
                                {
                                    double d = Math.Abs((HalfAt(exp, vi * stride + 2 * c) + o[c]) - refPos[3 * vi + c]);
                                    if (!any || d > e || double.IsNaN(d)) { e = d; any = true; }
                                }
                            }
                        vs.maxErr = Math.Max(vs.maxErr, e);
                        if (e > MAX_RECENTRE_ERR) probs.Add(mname + " g" + si + " (" + x.part + "): re-centring error " + Fmt(e, "0.0000") + " ft");
                    }
                    else if (o[0] != 0 || o[1] != 0 || o[2] != 0) probs.Add(mname + " g" + si + ": FLOAT16 source re-centred");
                }
                long nUsed = 0; foreach (bool u in used) if (u) nUsed++;
                if (nUsed != nt) probs.Add(mname + " g" + si + ": " + (nt - nUsed) + " source triangles missing from the output");
                vs.Inc("tris_matched", nUsed);
                vs.Inc(lay == FLOAT3 ? "groups_float3" : lay == CROWD ? "groups_crowd" : "groups_static");
            }
        }
    }
}

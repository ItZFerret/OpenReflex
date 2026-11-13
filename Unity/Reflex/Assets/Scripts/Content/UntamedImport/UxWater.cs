// MX vs ATV Untamed converter, assembly parts: the port of the Python reference converter's ux_water.py (DESIGN 23.3): native Reflex water for
// the EnduroCross arenas (ux_build_track --ec-water on, the default of the sx builds; a no-op where Untamed has no WaterSystemRSData).
//   UxWater.Build(ctx, hf, n, cap, kitOrder, paths, log)   (record, {(name, type): (bytes, source, origin)}, [Entity Elem]) for the level's
//                                                           Untamed water surfaces: per surface a PC v5 water asset UXW_<asset> (plane,
//                                                           box and 1 ft description map regenerated from OUR heightfield), a material
//                                                           (the stock Water exemplar with the Untamed colours) and a WaterSystemRSData
//                                                           (a copy of stock SX_DLC01_FreightYard's), plus the kit the core looks up
//                                                           by name (Water_Wake, WaterPressure, Water, WaterFoam)
//   UxWater.Parse / WaterPc / Material / SetEntity          the pieces (parse reads Untamed BE and Reflex PC LE water assets)
//   look (water-look round 2026-11-11, --ec-water-look)     'untamed' = the Untamed colours by name (the fairview-v4 / lincoln-v4
//                                                           bytes); 'arena' (the builder's default, DEFAULT_LOOK) = open-water /
//                                                           shore colours (UxWater.ARENA_*) and the description map's B as a
//                                                           depth blend (ShoreBlend)
//   shore (dirt-look rollout 2026-11-11, --ec-water-shore)  arena look only: 'untamed' = the shore colour (set b) is the Untamed
//                                                           Color x ARENA_WET (the fairview-v5 / lincoln-v5 bytes); 'ground' (the
//                                                           builder's default, DEFAULT_SHORE) = a wet version of the ground the
//                                                           build draws within SHORE_RING_FT of each surface (GroundShore,
//                                                           WetGround; the ground = UxWater.Ground: the near albedo on the client
//                                                           grid, UxDirtLook's TopDown composite)
//   UxWater.Splash / ParticleManagerAssets / RetargetParticles   --ec-water-splash on: the base ParticleManager's particle /
//                                                           partattr / texture kit + the bike water spray of FR_Gibson_Particles
//                                                           (emitters, atlas tiles copied block by block, 24 partattr records)
// Not ported: the look "twin" of the client's water shader (read_cube, cube_sample, look_colour, look_metrics, material_params,
// look_problems) and particle_info: the verifier's W10 / W11 gates.
// Reads level.json and terrain/terrain.json (the grid) from the work store, the Untamed water asset from the disc, the prototype, the
// exemplar and the kit from the stock library. Number rules: numpy float64 arithmetic; round() of a numpy scalar is np.round (the box),
// of a Python float is Python's round. Pure C# (System only).
using System;
using System.Collections.Generic;
using Reflex.Port.Content.AliveImport;

namespace Reflex.Port.Content.UntamedImport
{
    /// ux_water.py.
    public static class UxWater
    {
        public const string SCHEMA = "ux_water/1";
        public const double DEFAULT_CAP = 1.25;
        public const uint MAGIC = 0x77770360;
        public const double CELL = 1.0, MARGIN = 6.0; public const int DILATE = 2;
        public static readonly (string name, string type)[] KIT = { ("Water_Wake", "shader"), ("WaterPressure", "texture"), ("Water", "shader"), ("WaterFoam", "texture") };
        public static readonly string[] LOOKS = { "untamed", "arena" };
        public const string DEFAULT_LOOK = "arena";
        // ARENA: a = open water (B = 1), b = the shore (B = 0); B = depth / SHORE_FT clamped
        public const double ARENA_REFL_MIX = 0.5, ARENA_WAVE_SCALE = 0.2, ARENA_SHORE_FT = 0.5, ARENA_WET = 0.62, ARENA_DEEP_MIN_LUM = 0.22;
        public static readonly double[] ARENA_SKY = { 0.50, 0.53, 0.58 };
        static readonly double[] LUM = { 0.2126, 0.7152, 0.0722 };
        /// rec['look_params']: ARENA as the record holds it.
        static JObj ArenaRecord() => new JObj().Set("refl_mix", ARENA_REFL_MIX).Set("wave_scale", ARENA_WAVE_SCALE).Set("shore_ft", ARENA_SHORE_FT).Set("wet", ARENA_WET)
            .Set("deep_min_lum", ARENA_DEEP_MIN_LUM).Set("sky", new List<object> { ARENA_SKY[0], ARENA_SKY[1], ARENA_SKY[2] });
        static void CheckLook(string look) { if (Array.IndexOf(LOOKS, look) < 0) throw new UxException("look '" + look + "'"); }
        // shore colour source (DESIGN 23.11): SHORE_GROUND = ring_ft (dry ground this close to the wet HF pixels), wet_dark (luminance
        // factor), wet_sat (chroma factor), open_sep (the shore luminance is kept this far below the open water's)
        public static readonly string[] SHORES = { "untamed", "ground" };
        public const string DEFAULT_SHORE = "ground";
        public const double SHORE_RING_FT = 4.0, SHORE_WET_DARK = 0.60, SHORE_WET_SAT = 1.25, SHORE_OPEN_SEP = 0.06;
        static JObj ShoreRecord() => new JObj().Set("option", "ground").Set("ring_ft", SHORE_RING_FT).Set("wet_dark", SHORE_WET_DARK).Set("wet_sat", SHORE_WET_SAT).Set("open_sep", SHORE_OPEN_SEP);
        /// The ground a build draws, for the shore colour: the near albedo (n x n x 3 float64, row 0 = +Z) on the client grid of side S ft.
        public sealed class Ground { public double[] albedo; public int n; public double S; }
        // splash / spray
        public const string SPLASH_SRC = "FR_Gibson_Particles", SPLASH_VEHICLE = "MotocrossBike", SPLASH_PREFIX = "UXW_";
        public static readonly string[] SPLASH_EMITTERS = { "Water_splash_RT", "Water_splash_LF", "Water_Drops_LF", "Water_Drops_RT" };
        public const int EFFECT_SET_MAX = 0x60;
        /// PM_ROLES: the ParticleManagerRSData asset props and their types.
        public static readonly (string role, string type)[] PM_ROLES = { ("ParticleEmitterAsset", "particle"), ("TextureAsset", "texture"), ("ParticleAttribAsset", "partattr") };

        /// parse(b): a water asset.
        public sealed class Asset
        {
            public string endian; public long version, has_flow, has_desc, desc_float, end, size;
            public double[] matrix; public double extentX, extentZ;
            /// desc: (w, h, texel bytes in file order per texel, LE) or null.
            public int descW, descH; public byte[] desc;
        }
        /// parse(b): the water asset reader (Untamed BE / Reflex PC LE).
        public static Asset Parse(byte[] b)
        {
            UxBin.Require(b.Length >= 0xe0, "not a water asset");
            bool be = UxBin.U32(b, 0, true) == MAGIC, le = UxBin.U32(b, 0, false) == MAGIC;
            if (!(be || le)) throw new UxException("not a water asset");
            var o = new Asset { endian = be ? "BE" : "LE", version = UxBin.U32(b, 4, be), matrix = new double[16] };
            for (int i = 0; i < 16; i++) o.matrix[i] = UxBin.F32(b, 0x8c + 4 * i, be);
            o.extentX = UxBin.F32(b, 0xcc, be); o.extentZ = UxBin.F32(b, 0xd0, be);
            o.has_flow = UxBin.U32(b, 0xd4, be); o.has_desc = UxBin.U32(b, 0xd8, be); o.desc_float = UxBin.U32(b, 0xdc, be);
            long p = 0xe0;
            if (o.has_flow != 0) { long w = UxBin.U32(b, (int)p, be), h = UxBin.U32(b, (int)p + 4, be); p += 8 + w * h * 4; }
            if (o.has_desc != 0)
            {
                int w = (int)UxBin.U32(b, (int)p, be), h = (int)UxBin.U32(b, (int)p + 4, be); p += 8;
                if (o.desc_float != 0) p += 8;
                byte[] px = new byte[4 * w * h];
                if (be)
                {
                    int pitch = Math.Max(256, 4 * w);
                    for (int y = 0; y < h; y++)
                        for (int x = 0; x < w; x++)
                            for (int k = 0; k < 4; k++)
                            {
                                long src = p + (long)y * pitch + 4 * x + (3 - k);
                                if (src < b.Length) px[(y * w + x) * 4 + k] = b[src];
                            }
                    p += (long)pitch * h;
                }
                else
                {
                    Buffer.BlockCopy(b, (int)p, px, 0, (int)Math.Min(px.Length, Math.Max(0, b.Length - p)));
                    p += 4L * w * h;
                }
                o.descW = w; o.descH = h; o.desc = px;
            }
            o.end = p; o.size = b.Length;
            return o;
        }

        /// water_pc(matrix, ex, ez, w, h, desc): a Reflex PC v5 water asset without a flow map.
        public static byte[] WaterPc(double[] matrix, double ex, double ez, int w, int h, byte[] desc)
        {
            var o = new UxWriter(0xe8 + desc.Length);
            o.U32(MAGIC).U32(5).U32(0);
            for (int i = 12; i < 0x8c; i++) o.U8(0xcc);
            foreach (double m in matrix) o.F32((float)m);
            o.F32((float)ex).F32((float)ez).U32(0).U32(1).U32(0).U32((uint)w).U32((uint)h).Bytes(desc);
            return o.ToArray();
        }

        static double Num(object v) => v is double d ? d : v is long l ? l : v is int i ? i : throw new UxException("a number was expected");
        static object Round5(object v) => v is double d ? (object)UxNp.PyRound(d, 5) : v;       // round(int, 5) is the int
        static List<object> Vec(object a, object b, object c, object d) => new List<object> { a, b, c, d };

        /// open_colour(props): the arena look's open-water colour (set a): the Untamed DeepColor raised to ARENA_DEEP_MIN_LUM (hue kept).
        public static double[] OpenColour(JObj props)
        {
            var dc = UxJson.Arr(props.Get("DeepColor"));
            var deep = new[] { Num(dc[0]), Num(dc[1]), Num(dc[2]) };
            double dl = UxNp.Dot(deep, LUM);                                          // deep @ LUM (float64, the reference's ddot)
            if (dl < ARENA_DEEP_MIN_LUM)
            {
                double k = ARENA_DEEP_MIN_LUM / Math.Max(dl, 1e-6);
                for (int i = 0; i < 3; i++) deep[i] = Math.Min(Math.Max(deep[i] * k, 0.0), 1.0);
            }
            return deep;
        }

        /// wet_ground(rgb, open_rgb): the wet version of a dry ground colour: luminance x SHORE_WET_DARK, chroma x SHORE_WET_SAT
        /// (clipped to 0..1). With the open-water colour, a shore luminance closer than SHORE_OPEN_SEP to it is lowered to the open
        /// luminance - SHORE_OPEN_SEP (only ever darker).
        public static double[] WetGround(double[] rgb, double[] openRgb = null)
        {
            var g = new[] { rgb[0], rgb[1], rgb[2] };
            double L = UxNp.Dot(g, LUM);
            var s = new double[3];
            for (int i = 0; i < 3; i++) s[i] = SHORE_WET_DARK * (L + SHORE_WET_SAT * (g[i] - L));
            if (openRgb != null)
            {
                double lo = UxNp.Dot(new[] { openRgb[0], openRgb[1], openRgb[2] }, LUM), ls = UxNp.Dot(s, LUM);
                if (Math.Abs(ls - lo) < SHORE_OPEN_SEP && ls > 1e-6)
                {
                    double m = Math.Max(lo - SHORE_OPEN_SEP, 0.02);
                    for (int i = 0; i < 3; i++) s[i] = s[i] * m / ls;
                }
            }
            for (int i = 0; i < 3; i++) s[i] = Math.Min(Math.Max(s[i], 0.0), 1.0);
            return s;
        }

        /// ground_shore(ground, X, Z, wet, res, open_rgb): the shore colour of one surface = WetGround of the mean near albedo of the
        /// dry HF pixels within SHORE_RING_FT of its wet ones. xs / zs: the HF pixel centres (column / row), wet: N x N.
        public static (double[] shore, JObj record) GroundShore(Ground ground, double[] xs, double[] zs, bool[] wet, int N, double res, double[] openRgb = null)
        {
            int n = ground.n; double S = ground.S, cs = S / n; var alb = ground.albedo;
            UxBin.Require(alb != null && alb.Length == n * n * 3, "the ground albedo is not " + n + " x " + n + " x 3");
            var dry = new bool[N * N]; for (int i = 0; i < dry.Length; i++) dry[i] = !wet[i];
            double[] d = UxNd.DistanceTransformEdtDistances(dry, N, N);
            double s0 = 0, s1 = 0, s2 = 0; long ring = 0;
            for (int r = 0; r < N; r++)
                for (int c = 0; c < N; c++)
                {
                    double dist = d[r * N + c] * res;
                    if (!(dist > 0 && dist <= SHORE_RING_FT)) continue;
                    int j = (int)Math.Min(Math.Max(Math.Floor((xs[c] + S / 2) / cs), 0), n - 1), i = (int)Math.Min(Math.Max(Math.Floor((S / 2 - zs[r]) / cs), 0), n - 1);
                    int p = (i * n + j) * 3;
                    s0 += alb[p]; s1 += alb[p + 1]; s2 += alb[p + 2]; ring++;         // .mean(0) of an (m, 3) array: row by row
                }
            if (ring == 0) throw new UxException("no dry ground within " + UxImaging.PyFixed(SHORE_RING_FT, 1) + " ft of the water");
            var g = new[] { s0 / ring, s1 / ring, s2 / ring };
            double[] w0 = WetGround(g), w = WetGround(g, openRgb);
            double dmax = 0; for (int i = 0; i < 3; i++) dmax = Math.Max(dmax, Math.Abs(w[i] - w0[i]));
            List<object> R4(double[] v) => new List<object> { UxNp.PyRound(v[0], 4), UxNp.PyRound(v[1], 4), UxNp.PyRound(v[2], 4) };
            var rec = new JObj().Set("source", "ground").Set("ring_px", ring).Set("ground_rgb", R4(g)).Set("ground_lum", UxNp.PyRound(UxNp.Dot(g, LUM), 4))
                .Set("wet_lum", UxNp.PyRound(UxNp.Dot(w0, LUM), 4))
                .Set("open_lum", openRgb == null ? null : (object)UxNp.PyRound(UxNp.Dot(new[] { openRgb[0], openRgb[1], openRgb[2] }, LUM), 4))
                .Set("separated", dmax > 1e-12).Set("shore_rgb", R4(w)).Set("shore_lum", UxNp.PyRound(UxNp.Dot(w, LUM), 4));
            return (w, rec);
        }

        /// The level has an Untamed WaterSystemRSData (the builder asks before it computes the ground for the shore colour).
        public static bool HasWater(UxWork work)
        {
            foreach (object eo in UxJson.Arr(work.ReadJsonObj("level.json").Get("entities"))) if (UxJson.Obj(eo).Get("type") as string == "WaterSystemRSData") return true;
            return false;
        }

        /// material(props, look, shore_rgb): the stock Water exemplar material (FR_AL_RiverMain) with the usr_* parameters of the look ->
        /// (bytes, {parameter: [4 values rounded to 5 digits]}). untamed: the Untamed entity's colour props by name; arena: a =
        /// open water (the Untamed DeepColor, raised to ARENA_DEEP_MIN_LUM), b = the shore (shoreRgb = null: the Untamed Color x
        /// ARENA_WET, the fairview-v5 / lincoln-v5 bytes; else that colour: WetGround of the built ground), both reflecting ARENA_SKY
        /// at ARENA_REFL_MIX, wave scale ARENA_WAVE_SCALE.
        public static (byte[] data, JObj values) Material(UxStock stock, JObj props, string look = "untamed", double[] shoreRgb = null)
        {
            CheckLook(look);
            var b = new UxBxml(stock.LibMaterialBytes("Water"));
            List<object> Col(object v)
            {
                var l = new List<object>(UxJson.Arr(v));
                double mx = Math.Max(Num(l[0]), Math.Max(Num(l[1]), Num(l[2])));
                if (mx > 1.0) for (int i = 0; i < 3; i++) l[i] = Num(l[i]) / 255.0;
                return l;
            }
            var vals = new JObj();
            foreach (var (sfx, k) in new[] { ("a", ""), ("b", "2") })
            {
                var c = UxJson.Arr(props.Get("Color" + k)); var d = UxJson.Arr(props.Get("DeepColor" + k)); var r = UxJson.Arr(props.Get("ReflectionColor" + k));
                var f = Col(props.Get("ColorFilter" + k));
                double clarity = Num(props.Get("Clarity" + k));
                vals.Set("usr_water_color_" + sfx, Vec(c[0], c[1], c[2], 1.0));
                vals.Set("usr_amount_of_water_color_" + sfx, Vec(c[3], c[3], c[3], c[3]));
                vals.Set("usr_deep_water_color_" + sfx, Vec(d[0], d[1], d[2], 1.0));
                vals.Set("usr_water_clarity_" + sfx, Vec(clarity, clarity, clarity, clarity));
                vals.Set("usr_color_filter_" + sfx, Vec(f[0], f[1], f[2], 1.0));
                vals.Set("usr_reflection_color_" + sfx, Vec(r[0], r[1], r[2], 1.0));
                vals.Set("usr_amount_of_refl_color_" + sfx, Vec(r[3], r[3], r[3], r[3]));
            }
            double hm = Num(props.Get("HeightMultiplier")), tm = Num(props.Get("TileMultiplier"));
            vals.Set("usr_wave_scale", Vec(hm, hm, hm, hm));
            vals.Set("usr_river_tile_multiplier", Vec(tm, tm, tm, tm));
            if (look == "arena")
            {
                var cc = UxJson.Arr(props.Get("Color"));
                var deep = OpenColour(props);
                var shore = new double[3];
                for (int i = 0; i < 3; i++) shore[i] = Math.Min(Math.Max(shoreRgb == null ? Num(cc[i]) * ARENA_WET : shoreRgb[i], 0.0), 1.0);
                foreach (var (sfx, wc) in new[] { ("a", deep), ("b", shore) })
                {
                    vals.Set("usr_water_color_" + sfx, Vec(wc[0], wc[1], wc[2], 1.0));
                    vals.Set("usr_amount_of_water_color_" + sfx, Vec(1.0, 1.0, 1.0, 1.0));
                    vals.Set("usr_reflection_color_" + sfx, Vec(ARENA_SKY[0], ARENA_SKY[1], ARENA_SKY[2], 1.0));
                    vals.Set("usr_amount_of_refl_color_" + sfx, Vec(ARENA_REFL_MIX, ARENA_REFL_MIX, ARENA_REFL_MIX, ARENA_REFL_MIX));
                }
                vals.Set("usr_wave_scale", Vec(ARENA_WAVE_SCALE, ARENA_WAVE_SCALE, ARENA_WAVE_SCALE, ARENA_WAVE_SCALE));
            }
            var done = new HashSet<string>(StringComparer.Ordinal);
            for (int ni = 0; ni < b.nodes.Count; ni++)
            {
                if (b.Name(ni) != "Parameter") continue;
                string nm = b.Attr(ni, "name") as string;
                if (nm == null || !vals.Has(nm)) continue;
                var n = b.nodes[ni];
                if (n[3] != 7) throw new UxException("Water.material parameter " + nm + " is not a 4-float value");
                var v = (List<object>)vals.Get(nm);
                for (int i = 0; i < 4; i++) UxBin.PutF32(b.pool, (int)n[1] + 4 * i, (float)Num(v[i]));
                done.Add(nm);
            }
            if (done.Count != vals.items.Count)
            {
                var missing = new List<string>(); foreach (var kv in vals.items) if (!done.Contains(kv.Key)) missing.Add(kv.Key);
                missing.Sort(string.CompareOrdinal);
                throw new UxException("Water.material lacks parameters [" + string.Join(", ", missing) + "]");
            }
            var rounded = new JObj();
            foreach (var kv in vals.items) { var l = new List<object>(); foreach (object x in (List<object>)kv.Value) l.Add(Round5(x)); rounded.Set(kv.Key, l); }
            return (b.ToBytes(), rounded);
        }

        static byte[] PackF32(params double[] v) { var b = new byte[4 * v.Length]; for (int i = 0; i < v.Length; i++) UxBin.PutF32(b, 4 * i, (float)v[i]); return b; }
        static byte[] PackU32(long v) { var b = new byte[4]; UxBin.PutU32(b, 0, (uint)v); return b; }

        /// set_entity(e, eid, name, matrix, water, mat, flow): fills a copy of the stock water prototype.
        public static UxElem SetEntity(UxElem e, long eid, string name, double[] matrix, string water, string mat, double flow = 0.0)
        {
            foreach (var a in e.attrs) if (a.name == "entityId") { a.raw = PackU32(eid); a.str = null; }
            var seen = new HashSet<string>(StringComparer.Ordinal);
            foreach (var r in e.children[0].children)
            {
                string k = r.Attr("name") as string; var v = r.children[0];
                if (k == "Name") v.value = name;
                else if (k == "Id") v.value = PackU32(eid);
                else if (k == "Matrix") v.value = PackF32(matrix);
                else if (k == "WaterAsset") v.attrs = new List<UxAttr> { new UxAttr("name", water), new UxAttr("type", "water") };
                else if (k == "MaterialAsset") v.attrs = new List<UxAttr> { new UxAttr("name", mat), new UxAttr("type", "material") };
                else if (k == "FlowRate") v.value = PackF32(flow);
                if (k != null) seen.Add(k);
            }
            var lacks = new List<string>();
            foreach (string k in new[] { "FlowRate", "Id", "MaterialAsset", "Matrix", "Name", "WaterAsset" }) if (!seen.Contains(k)) lacks.Add(k);
            if (lacks.Count > 0) throw new UxException("stock water prototype lacks props [" + string.Join(", ", lacks) + "]");
            return e;
        }

        /// shore_blend(depth, look): the desc B byte (a / b parameter blend) of a cell depth (plane - HF, ft; <= 0 = dry): untamed
        /// 255 (the a set); arena round(255 x clip(depth / ARENA_SHORE_FT, 0, 1)).
        public static byte ShoreBlend(double depth, string look)
            => look == "untamed" ? (byte)255 : (byte)UxNp.Round(255 * Math.Min(Math.Max(depth / ARENA_SHORE_FT, 0.0), 1.0));

        /// What Build returns.
        public sealed class Result { public JObj record; public UxAssetMap assets = new UxAssetMap(); public List<UxElem> elems = new List<UxElem>(); }

        /// build(level, hf, cap, log, kit_order). hf: the N x N height field (terrain/hf.npy, row-major; null = read it from the work
        /// store's terrain folder). cap: the depth cap in ft (the builder passes 1e9 for --ec-water-depth untamed); kitOrder 'first'
        /// (default) puts the kit ahead of the water assets, 'last' is the old (crashing) order. shore (arena look only): 'untamed' = the
        /// fairview-v5 / lincoln-v5 shore colour; 'ground' = WetGround of the built ground next to each surface (ground: needed when
        /// the level has water).
        public static Result Build(UxContext ctx, double[] hf = null, int n = 0, double cap = DEFAULT_CAP, string kitOrder = "first", UxPaths paths = null, Action<string> log = null,
            string look = "untamed", string shore = "untamed", Ground ground = null)
        {
            CheckLook(look);
            if (Array.IndexOf(SHORES, shore) < 0) throw new UxException("shore '" + shore + "'");
            paths = paths ?? new UxPaths();
            var lv = ctx.lv; var work = ctx.work; var stock = ctx.stock;
            var L = work.ReadJsonObj("level.json");
            double lift = L.Get("y_lift") != null ? Num(L.Get("y_lift")) : UxCommon.YLift(lv);
            var ents = new List<JObj>();
            foreach (object eo in UxJson.Arr(L.Get("entities"))) { var e = UxJson.Obj(eo); if (e.Get("type") as string == "WaterSystemRSData") ents.Add(e); }
            var res = new Result();
            var surfaces = new List<object>();
            var rec = new JObj().Set("schema", SCHEMA).Set("option", "on").Set("cap_ft", cap).Set("cell_ft", CELL).Set("dilate_cells", (long)DILATE).Set("margin_ft", MARGIN)
                .Set("lift", lift).Set("surfaces", surfaces);
            res.record = rec;
            if (look != "untamed") rec.Set("look", look).Set("look_params", ArenaRecord());
            if (ents.Count == 0) { rec.Set("reason", "no Untamed WaterSystemRSData in this level (no-op)"); return res; }
            bool groundShore = look == "arena" && shore == "ground";
            if (groundShore)
            {
                if (ground == null) throw new UxException("shore 'ground' needs the built ground's near albedo");
                rec.Set("shore", ShoreRecord());
            }
            if (hf == null)
            {
                hf = UxEntities.ReadNpy(work.ReadBytes("terrain/hf.npy"), out int[] shape);
                UxBin.Require(shape.Length == 2 && shape[0] == shape[1], "terrain/hf.npy: a square height field was expected");
                n = shape[0];
            }
            UxBin.Require(n > 0 && hf.Length == n * n, "the height field is not " + n + " x " + n);
            var T = work.ReadJsonObj("terrain/terrain.json");
            double x0 = Num(UxJson.Get(T, "grid", "x0")), z1 = Num(UxJson.Get(T, "grid", "z1")), rs = Num(UxJson.Get(T, "grid", "res_ft"));
            int N = n;
            var xs = new double[N]; var zs = new double[N];
            for (int i = 0; i < N; i++) { xs[i] = x0 + (i + 0.5) * rs; zs[i] = z1 - (i + 0.5) * rs; }
            UxNp.StableSort(ents, (a, b) => UxJson.Int(a.Get("id")).CompareTo(UxJson.Int(b.Get("id"))));
            var assets = new UxAssetMap();
            foreach (var e in ents)
            {
                var props = UxJson.Obj(e.Get("props"));
                string uname = UxJson.Str(UxJson.Get(props, "WaterAsset", "asset"));
                var src = Parse(ctx.disc.ReadExtract(uname, "water", ctx.cancel));
                var m = src.matrix;
                double[] ident = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0 };
                for (int i = 0; i < 12; i++) if (Math.Abs(m[i] - ident[i]) > 1e-6) throw new UxException("Untamed water " + uname + " has a rotated plane");
                double cx0 = m[12], cz0 = m[14], ex0 = src.extentX, ez0 = src.extentZ;
                double wy0 = m[13] + lift;
                var inside = new bool[N * N];
                for (int r = 0; r < N; r++)
                {
                    bool zin = Math.Abs(zs[r] - cz0) <= ez0 / 2;
                    if (!zin) continue;
                    for (int c = 0; c < N; c++) inside[r * N + c] = Math.Abs(xs[c] - cx0) <= ex0 / 2;
                }
                bool anyWet0 = false; double dmax0 = 0.0;
                for (int i = 0; i < inside.Length; i++)
                    if (inside[i] && hf[i] < wy0) { double d = wy0 - hf[i]; if (!anyWet0 || d > dmax0) dmax0 = d; anyWet0 = true; }
                if (!anyWet0) dmax0 = 0.0;
                double lower = Math.Max(0.0, dmax0 - cap);
                double wy = wy0 - lower;
                var srec = new JObj().Set("untamed_entity", e.Get("id")).Set("name", e.Get("name")).Set("untamed_asset", uname)
                    .Set("plane_untamed_lifted", UxNp.PyRound(wy0, 4)).Set("depth_max_untamed", UxNp.PyRound(dmax0, 4)).Set("lowered_ft", UxNp.PyRound(lower, 4))
                    .Set("plane", UxNp.PyRound(wy, 4));
                int cmin = int.MaxValue, cmax = -1, rmin = int.MaxValue, rmax = -1; long wetCount = 0;
                var dep = new List<double>();
                bool[] wet = groundShore ? new bool[N * N] : null;
                for (int r = 0; r < N; r++)
                    for (int c = 0; c < N; c++)
                    {
                        int i = r * N + c;
                        if (!(inside[i] && hf[i] < wy)) continue;
                        wetCount++; dep.Add(wy - hf[i]);
                        if (wet != null) wet[i] = true;
                        if (c < cmin) cmin = c; if (c > cmax) cmax = c; if (r < rmin) rmin = r; if (r > rmax) rmax = r;
                    }
                if (wetCount == 0) { srec.Set("skipped", "no heightfield pixel under the plane"); surfaces.Add(srec); continue; }
                double bx0 = xs[cmin] - 0.5 * rs - MARGIN, bx1 = xs[cmax] + 0.5 * rs + MARGIN;
                double bz0 = zs[rmax] - 0.5 * rs - MARGIN, bz1 = zs[rmin] + 0.5 * rs + MARGIN;
                int w = (int)Math.Ceiling((bx1 - bx0) / CELL), h = (int)Math.Ceiling((bz1 - bz0) / CELL);
                double ex = w * CELL, ez = h * CELL;
                double cx = bx0 + ex / 2, cz = bz0 + ez / 2;
                var valid = new bool[h * w]; var blend = new byte[h * w];
                for (int gy = 0; gy < h; gy++)
                {
                    double gz = bz0 + (gy + 0.5) * CELL;
                    int ri = (int)Math.Min(Math.Max(Math.Floor((z1 - gz) / rs), 0), N - 1);
                    bool zin = Math.Abs(gz - cz0) <= ez0 / 2;
                    for (int gxi = 0; gxi < w; gxi++)
                    {
                        double gx = bx0 + (gxi + 0.5) * CELL;
                        int ci = (int)Math.Min(Math.Max(Math.Floor((gx - x0) / rs), 0), N - 1);
                        valid[gy * w + gxi] = hf[ri * N + ci] < wy && Math.Abs(gx - cx0) <= ex0 / 2 && zin;
                        blend[gy * w + gxi] = ShoreBlend(wy - hf[ri * N + ci], look);
                    }
                }
                valid = UxNd.BinaryDilation(valid, h, w, DILATE, true);
                var tex = new byte[h * w * 4]; long validCells = 0;
                long shoreCells = 0, openCells = 0;
                for (int i = 0; i < h * w; i++)
                {
                    if (valid[i]) { tex[4 * i + 1] = 255; validCells++; if (blend[i] < 255) shoreCells++; else openCells++; }
                    tex[4 * i + 2] = blend[i];
                }
                double[] matrix = { 1.0, 0, 0, 0, 0, 1.0, 0, 0, 0, 0, 1.0, 0, cx, wy, cz, 1.0 };
                string wname = "UXW_" + uname;
                byte[] wdata = WaterPc(matrix, ex, ez, w, h, tex);
                var chk = Parse(wdata);
                if (chk.end != wdata.Length || chk.descW != w || chk.descH != h) throw new UxException("water asset " + wname + " does not re-read");
                double[] shoreRgb = null;
                if (groundShore)
                {
                    var (sr, shrec) = GroundShore(ground, xs, zs, wet, N, rs, OpenColour(props));
                    shoreRgb = sr; srec.Set("shore", shrec);
                    log?.Invoke("water " + uname + " shore from the ground: " + UxJson.Dumps(shrec.Get("ring_px"), -1) + " dry px, ground lum " + UxJson.Dumps(shrec.Get("ground_lum"), -1) +
                        " -> shore lum " + UxJson.Dumps(shrec.Get("shore_lum"), -1));
                }
                var (mdata, mvals) = Material(stock, props, look, shoreRgb);
                assets.Set(wname, "water", wdata, paths.Extract(lv, "water/" + uname + ".water"), "ux_water.water_pc (plane, box and mask regenerated from our heightfield)");
                assets.Set(wname, "material", mdata, paths.Templates("reflex-lib/materials/Water.material"), "ux_water.material (reflex-lib Water.material + the Untamed colours)");
                long eid = UxJson.Int(e.Get("id")) + UxEntities.ID_OFFSET;
                res.elems.Add(SetEntity(stock.Prototype("WaterSystemRSData").entity, eid, e.Get("name") as string, matrix, wname, wname));
                double dmax = dep[0]; foreach (double d in dep) if (d > dmax) dmax = d;
                dep.Sort();
                double p50 = dep.Count % 2 == 1 ? dep[dep.Count / 2] : (dep[dep.Count / 2 - 1] + dep[dep.Count / 2]) / 2.0;
                srec.Set("entity", eid).Set("asset", wname).Set("material", wname)
                    .Set("box", new List<object> { UxNp.Round(bx0, 3), UxNp.Round(bx0 + ex, 3), UxNp.Round(bz0, 3), UxNp.Round(bz0 + ez, 3) })
                    .Set("desc", new List<object> { (long)w, (long)h }).Set("valid_cells", validCells).Set("wet_hf_px", wetCount)
                    .Set("depth_max", UxNp.PyRound(dmax, 4)).Set("depth_p50", UxNp.PyRound(p50, 4))
                    .Set("water_md5", UxBin.Md5(wdata)).Set("material_md5", UxBin.Md5(mdata)).Set("material_params", mvals);
                if (look != "untamed") srec.Set("shore_cells", shoreCells).Set("open_cells", openCells);
                surfaces.Add(srec);
            }
            var kit = new UxAssetMap();
            foreach (var key in KIT)
            {
                byte[] b = stock.Get(key.name, key.type);
                if (b == null) throw new UxException("stock-lib has no " + key.name + "." + key.type + " (run ux_stocklib.py --fetch)");
                kit.Set(key.name, key.type, b, paths.StockLib(key.name, key.type), "stock " + UxStock.Record(key.name, key.type).source + " (templates/stock-lib, md5 pinned)");
            }
            if (kitOrder == "first")
            {   // the core loads a package's assets in record order and the water asset's loader needs Water_Wake already LOADED
                foreach (var kv in kit) res.assets[kv.Key] = kv.Value;
                foreach (var kv in assets) res.assets[kv.Key] = kv.Value;
            }
            else if (kitOrder == "last")
            {
                foreach (var kv in assets) res.assets[kv.Key] = kv.Value;
                foreach (var kv in kit) res.assets[kv.Key] = kv.Value;
            }
            else throw new UxException("kit order '" + kitOrder + "'");
            var ids = new List<object>();
            foreach (object s in surfaces) if (((JObj)s).Has("entity")) ids.Add(((JObj)s).Get("entity"));
            var kl = new List<object>(); foreach (var k in KIT) kl.Add(k.name + "." + k.type);
            rec.Set("entity_ids", ids).Set("kit", kl).Set("kit_order", kitOrder);
            if (log != null)
            {
                var parts = new List<string>();
                foreach (object s in surfaces) { var j = (JObj)s; parts.Add(j.Get("name") + " " + (j.Get("asset") ?? "-") + " plane " + UxJson.Dumps(j.Get("plane"), -1) + " (-" + UxJson.Dumps(j.Get("lowered_ft"), -1) + ")"); }
                log("EC water: " + string.Join("; ", parts));
            }
            return res;
        }

        // ================================================================================================ splash / spray
        static (UxElem pf, List<double[]> frames) Frames(UxElem root)
        {
            var pf = root.Find("ParticleFrames") ?? throw new UxException("particle asset without ParticleFrames");
            var o = new List<double[]>();
            foreach (var f in pf.FindAll("Frame"))
            {
                double l = 0, r = 0, t = 0, b = 0; int got = 0;
                foreach (var a in f.attrs)
                {
                    if (a.type != 5) continue;
                    double v = UxBin.F32LE(a.raw, 0);
                    switch (a.name) { case "Left": l = v; got |= 1; break; case "Right": r = v; got |= 2; break; case "Top": t = v; got |= 4; break; case "Bottom": b = v; got |= 8; break; }
                }
                if (got != 15) throw new UxException("particle frame without Left / Right / Top / Bottom");
                o.Add(new[] { l, r, t, b });
            }
            return (pf, o);
        }
        static double[] F2(UxElem e, string tag)
        {
            var c = e.Find(tag);
            return c == null || c.vtype != 9 ? null : new double[] { UxBin.F32LE((byte[])c.value, 0), UxBin.F32LE((byte[])c.value, 4) };
        }
        static uint? U32Of(object v) => v is byte[] b && b.Length >= 4 ? UxBin.U32LE(b, 0) : (uint?)null;
        /// tire_in_water(e): a partattr Emitter element whose Triggers/TireInWater is Enabled with value 1.
        public static bool TireInWater(UxElem e)
        {
            var tw = e.Find("Triggers")?.Find("TireInWater");
            if (tw == null) return false;
            object en = null; foreach (var a in tw.attrs) if (a.name == "Enabled") { en = a.Value; break; }
            return U32Of(en) == 1 && U32Of(tw.value) == 1;
        }

        /// splash(base_particle, base_partattr, base_texture, base_name): the water spray for a stadium base's ParticleManager ->
        /// (record, {(UXW_<base_name>, type): (bytes, source, origin)}: texture, particle, partattr). The base particle asset gets
        /// the bike water emitters of FR_Gibson_Particles (their FrameRange / FrameStart moved onto appended ParticleFrames), the
        /// base atlas gets their tiles (DXT5 blocks copied mip by mip onto blocks no base frame touches), the base partattr gets
        /// every FR_Gibson MotocrossBike record with TireInWater enabled. The source kit comes from the stock library.
        public static (JObj record, UxAssetMap assets) Splash(UxStock stock, byte[] baseParticle, byte[] basePartattr, byte[] baseTexture, string baseName, Action<string> log = null)
        {
            byte[] Src(string t) => stock.Get(SPLASH_SRC, t) ?? throw new UxException("stock-lib has no " + SPLASH_SRC + "." + t + " (run ux_stocklib.py --fetch)");
            byte[] srcParticle = Src("particle"), srcPartattr = Src("partattr"), srcTexture = Src("texture");
            var pb = new UxBxml(baseParticle); var ab = new UxBxml(basePartattr);
            var P = pb.ToTree(); var A = ab.ToTree();
            var G = new UxBxml(srcParticle).ToTree(); var GA = new UxBxml(srcPartattr).ToTree();
            UxElem pem = P.Find("ParticleEmitters"), gem = G.Find("ParticleEmitters");
            if (pem == null || gem == null) throw new UxException("particle asset without ParticleEmitters");
            foreach (string nm in SPLASH_EMITTERS) if (pem.Find(nm) != null) throw new UxException("base particle asset already has " + nm);
            var (pf, pframes) = Frames(P); var (_, gframes) = Frames(G);
            var add = new List<UxElem>();
            foreach (string nm in SPLASH_EMITTERS) add.Add((gem.Find(nm) ?? throw new UxException(SPLASH_SRC + " has no emitter " + nm)).DeepCopy());
            int lo = int.MaxValue, hi = int.MinValue;
            foreach (var e in add)
            {
                double[] fr = F2(e, "FrameRange"), fs = F2(e, "FrameStart");
                if (fr == null || fs == null) throw new UxException(SPLASH_SRC + ": an emitter without FrameRange / FrameStart");
                lo = Math.Min(lo, (int)fr[0]); hi = Math.Max(hi, (int)Math.Ceiling(Math.Max(fr[1], fs[1])));
            }
            var gidx = new List<int>(); for (int g = lo; g < Math.Min(hi + 1, gframes.Count); g++) gidx.Add(g);
            // atlas
            var tb = new UxPcTexture(baseTexture); var gt = new UxPcTexture(srcTexture);
            if (tb.nsurf != 1 || gt.nsurf != 1 || !UxTex.Fmt(tb.recs[0].fmt, UxTex.PC_DXT5) || !UxTex.Fmt(gt.recs[0].fmt, UxTex.PC_DXT5))
                throw new UxException("particle atlases: want one DXT5 surface each");
            int bm = tb.recs[0].mips, bw = tb.recs[0].w, bh = tb.recs[0].h, gm = gt.recs[0].mips, gw = gt.recs[0].w, gh = gt.recs[0].h;
            var rects = new List<double[]>();                                       // the distinct source rectangles, in frame order
            int RectOf(double[] f) { for (int i = 0; i < rects.Count; i++) if (rects[i][0] == f[0] && rects[i][1] == f[1] && rects[i][2] == f[2] && rects[i][3] == f[3]) return i; return -1; }
            foreach (int g in gidx) if (RectOf(gframes[g]) < 0) rects.Add(gframes[g]);
            int ow = bw / 4, oh = bh / 4; var occ = new bool[oh * ow];
            void Mark(int y0, int y1, int x0, int x1)
            {
                for (int y = Math.Max(0, y0); y < Math.Min(oh, y1); y++) for (int x = Math.Max(0, x0); x < Math.Min(ow, x1); x++) occ[y * ow + x] = true;
            }
            bool Any(int y0, int y1, int x0, int x1)
            {
                for (int y = Math.Max(0, y0); y < Math.Min(oh, y1); y++) for (int x = Math.Max(0, x0); x < Math.Min(ow, x1); x++) if (occ[y * ow + x]) return true;
                return false;
            }
            foreach (var f in pframes)                                              // exact blocks (stock atlases abut their tiles without gutters)
                Mark((int)Math.Floor(f[2] * bh / 4), (int)Math.Ceiling(f[3] * bh / 4), (int)Math.Floor(f[0] * bw / 4), (int)Math.Ceiling(f[1] * bw / 4));
            var order = new List<int>(); for (int i = 0; i < rects.Count; i++) order.Add(i);
            UxNp.StableSort(order, (x, y) => (-(rects[x][1] - rects[x][0]) * (rects[x][3] - rects[x][2])).CompareTo(-(rects[y][1] - rects[y][0]) * (rects[y][3] - rects[y][2])));
            var place = new (int sx, int sy, int pw, int ph, int dx, int dy)[rects.Count]; var placed = new List<int>();
            foreach (int i in order)
            {
                double l = rects[i][0], r = rects[i][1], t = rects[i][2], b = rects[i][3];
                int pw = (int)UxNp.Round((r - l) * gw), ph = (int)UxNp.Round((b - t) * gh), sx = (int)UxNp.Round(l * gw), sy = (int)UxNp.Round(t * gh);
                if (pw % 4 != 0 || ph % 4 != 0 || sx % 4 != 0 || sy % 4 != 0) throw new UxException("a source frame is not on 4x4 blocks");
                int step = Math.Max(4, Math.Min(pw, ph)); int spotX = -1, spotY = -1;
                for (int y = bh - ph; y >= 0 && spotX < 0; y -= step)               // bottom first (FortDodge's free band)
                    for (int x = bw - pw; x >= 0; x -= step)
                        if (!Any(y / 4, (y + ph) / 4, x / 4, (x + pw) / 4)) { spotX = x; spotY = y; break; }
                if (spotX < 0) throw new UxException("no free " + pw + "x" + ph + " area in the base atlas");
                Mark(spotY / 4, (spotY + ph) / 4, spotX / 4, (spotX + pw) / 4);
                place[i] = (sx, sy, pw, ph, spotX, spotY); placed.Add(i);
            }
            var entries = new List<byte[]>(tb.entries); var copied = new List<object>();
            for (int m = 0; m < bm && m < gm; m++)
            {
                int mw = Math.Max(1, bw >> m), sw = Math.Max(1, gw >> m);
                int bp = UxTex.PcPitch(UxTex.PC_DXT5, mw), sp = UxTex.PcPitch(UxTex.PC_DXT5, sw);
                var dst = (byte[])entries[m].Clone(); var srcm = gt.entries[m]; long nTiles = 0;
                foreach (int i in placed)
                {
                    var p = place[i];
                    int sx = p.sx >> m, sy = p.sy >> m, pw = p.pw >> m, ph = p.ph >> m, dx = p.dx >> m, dy = p.dy >> m;
                    if (pw < 4 || ph < 4) continue;                                 // under one block: the base's own (mixed) block stays
                    for (int by = 0; by < ph / 4; by++)
                    {
                        int so = (sy / 4 + by) * sp + (sx / 4) * 16, dof = (dy / 4 + by) * bp + (dx / 4) * 16, len = (pw / 4) * 16;
                        UxBin.Require(so + len <= srcm.Length && dof + len <= dst.Length, "particle atlas: a tile outside mip " + m);
                        Buffer.BlockCopy(srcm, so, dst, dof, len);
                    }
                    nTiles++;
                }
                entries[m] = dst; copied.Add(nTiles);
            }
            if (!UxBin.Same(UxTex.BuildPc(tb.recs, tb.table, tb.entries, tb.kind, tb.flags), baseTexture)) throw new UxException("base atlas does not rebuild byte-identically");
            byte[] texOut = UxTex.BuildPc(tb.recs, tb.table, entries, tb.kind, tb.flags);
            // frames: the source frames lo..hi appended in order (their index offset kept), on the placed tiles
            var protoFrame = pf.FindAll("Frame")[0]; int baseN = pframes.Count;
            foreach (int g in gidx)
            {
                var p = place[RectOf(gframes[g])];
                var f = protoFrame.DeepCopy();
                foreach (var a in f.attrs)
                {
                    double v;
                    switch (a.name)
                    {
                        case "Left": v = (double)p.dx / bw; break; case "Right": v = (double)(p.dx + p.pw) / bw; break;
                        case "Top": v = (double)p.dy / bh; break; case "Bottom": v = (double)(p.dy + p.ph) / bh; break;
                        default: continue;
                    }
                    a.raw = PackF32(v); a.str = null;
                }
                pf.children.Add(f);
            }
            int shift = baseN - lo;
            foreach (var e in add)
            {
                foreach (string tag in new[] { "FrameRange", "FrameStart" }) { var v = F2(e, tag); e.Find(tag).value = PackF32(v[0] + shift, v[1] + shift); }
                pem.children.Add(e);
            }
            // partattr records
            var recs = new List<UxElem>();
            foreach (var e in GA.FindAll("Emitter"))
            {
                if (Array.IndexOf(SPLASH_EMITTERS, e.Attr("Name") as string) < 0 || e.Attr("VehicleType") as string != SPLASH_VEHICLE || !TireInWater(e)) continue;
                recs.Add(e.DeepCopy());
            }
            if (recs.Count != 24) throw new UxException(SPLASH_SRC + ": " + recs.Count + " bike water records (want 24)");
            var per = new SortedDictionary<string, long>(StringComparer.Ordinal);
            var all = new List<UxElem>(A.FindAll("Emitter")); all.AddRange(recs);
            foreach (var r in all)
            {
                string k = (r.Attr("VehicleType") as string ?? "None") + "/" + (r.Attr("EngineClass") as string ?? "None");
                per[k] = (per.TryGetValue(k, out long c) ? c : 0) + 1;
            }
            foreach (var kv in per) if (kv.Value > EFFECT_SET_MAX) throw new UxException("effect set over " + EFFECT_SET_MAX + " records: " + kv.Key + " " + kv.Value);
            A.children.AddRange(recs);
            byte[] pOut = UxBxml.EncodeTree(P, pb.version, pb.unknown), aOut = UxBxml.EncodeTree(A, ab.version, ab.unknown);
            string name = SPLASH_PREFIX + baseName;
            var tiles = new List<object>();
            foreach (int i in placed)
                tiles.Add(new JObj().Set("src", new List<object> { (long)place[i].sx, (long)place[i].sy, (long)place[i].pw, (long)place[i].ph }).Set("dst", new List<object> { (long)place[i].dx, (long)place[i].dy }));
            var rpc = new JObj(); foreach (var kv in per) rpc.Set(kv.Key, kv.Value);
            var em = new List<object>(); foreach (string s in SPLASH_EMITTERS) em.Add(s);
            var rec = new JObj().Set("option", "on").Set("source", SPLASH_SRC + " (stock FR_Gibson via templates/stock-lib)").Set("base", baseName).Set("assets", name)
                .Set("emitters", em).Set("frames_added", (long)gidx.Count).Set("frame_index", new List<object> { (long)baseN, (long)(baseN + gidx.Count) }).Set("tiles", tiles)
                .Set("mips_copied", copied).Set("partattr_records_added", (long)recs.Count).Set("records_per_class", rpc)
                .Set("md5", new JObj().Set("particle", UxBin.Md5(pOut)).Set("partattr", UxBin.Md5(aOut)).Set("texture", UxBin.Md5(texOut)));
            string origin = "ux_water.splash (base " + baseName + " + " + SPLASH_SRC + " bike water emitters)";
            var assets = new UxAssetMap();
            assets.Set(name, "texture", texOut, baseName, origin);
            assets.Set(name, "particle", pOut, baseName, origin);
            assets.Set(name, "partattr", aOut, baseName, origin);
            log?.Invoke("EC water splash: " + name + " = " + baseName + " + " + add.Count + " emitters, " + gidx.Count + " frames, " + placed.Count + " tiles, " + recs.Count + " partattr records");
            return (rec, assets);
        }

        // ================================================================ water glare (ux_water.py, fix round 'water-glare')
        // The lake program adds the cube's HDR excess after the reflection-colour lerp, scaled only by Fresnel, so no material value
        // can dim it; the stadium base's level cube holds the stadium lights as HDR texels (peak ~570), and the ripples turn them into
        // glints that the bloom spreads into white blobs. In Open Reflex only the water samples this cube. --ec-water-reflection gives
        // the level a copy of the cube (UXW_<cube>; never a name ending in 'ReflectionMap', which the object shading would take) and
        // points the AmbientDirectionalLight's _ReflectionMap at it. Header, descriptor table, alpha and every texel whose largest
        // channel is <= REFLECTION_LIGHT keep their bytes:
        //   dim      (default) a light texel is scaled (hue kept) to largest channel REFLECTION_DIM
        //   nolights a light texel is in-painted from its non-light neighbours
        //   level    the level cube itself (the bytes of fairview-v6 / lincoln-v6)
        public static readonly string[] REFLECTION_CHOICES = { "dim", "nolights", "level" };
        public const string REFLECTION_PREFIX = "UXW_";
        public const float REFLECTION_LIGHT = 1.0f, REFLECTION_DIM = 2.0f;

        /// np.float16 -> np.float32 (exact).
        public static float HalfToFloat(ushort h)
        {
            int s = h >> 15, e = (h >> 10) & 31, m = h & 1023;
            uint bits;
            if (e == 0)
            {
                if (m == 0) bits = (uint)s << 31;
                else { int sh = 0; while ((m & 1024) == 0) { m <<= 1; sh++; } bits = ((uint)s << 31) | (uint)((113 - sh) << 23) | (uint)((m & 1023) << 13); }
            }
            else if (e == 31) bits = ((uint)s << 31) | 0x7F800000u | (uint)(m << 13);
            else bits = ((uint)s << 31) | (uint)((e + 112) << 23) | (uint)(m << 13);
            return BitConverter.ToSingle(BitConverter.GetBytes(bits), 0);
        }
        /// np.float32 -> np.float16 (round to nearest, ties to even; overflow to infinity).
        public static ushort FloatToHalf(float f)
        {
            uint x = BitConverter.ToUInt32(BitConverter.GetBytes(f), 0);
            uint s = (x >> 16) & 0x8000u; int e = (int)((x >> 23) & 255); uint m = x & 0x7FFFFFu;
            if (e == 255) return (ushort)(s | 0x7C00u | (m != 0 ? 0x200u | (m >> 13) : 0u));
            if (e == 0) return (ushort)s;   // (zero, or a float32 subnormal: far below the smallest half)
            int he = e - 112;
            if (he >= 31) return (ushort)(s | 0x7C00u);
            if (he <= 0)
            {   // a half subnormal (or zero): the 24-bit significand shifted right by 14 - he bits
                if (he < -10) return (ushort)s;
                uint sig = m | 0x800000u; int shift = 14 - he;
                uint q = sig >> shift, rem = sig & ((1u << shift) - 1), halfway = 1u << (shift - 1);
                if (rem > halfway || rem == halfway && (q & 1) != 0) q++;
                return (ushort)(s | q);   // (a carry into the exponent field is the right value)
            }
            uint qn = m >> 13, remn = m & 0x1FFFu;
            uint v = ((uint)he << 10) | qn;
            if (remn > 0x1000u || remn == 0x1000u && (qn & 1) != 0) v++;   // (a carry runs into the exponent; 31 = infinity)
            return (ushort)(s | v);
        }

        sealed class CubeLevel { public int p, face, level, size, pitch, bytes; }
        /// _cube_layout(b): the face levels of a Reflex PC cube texture, and its format.
        static (List<CubeLevel> lay, int fmt) CubeLayout(byte[] b)
        {
            if (b.Length < 56 || UxBin.U32LE(b, 0) != 0x77ddcc00u || UxBin.I32LE(b, 20) != 2) throw new UxException("not a cube texture");
            int desc = UxBin.I32LE(b, 16), levels = UxBin.I32LE(b, 40), size = UxBin.I32LE(b, 44), fmt = UxBin.I32LE(b, 52);
            if (desc != 6 * levels || !(0 < size && size <= 4096)) throw new UxException("cube texture levels");
            long p = 56 + 8L * desc;
            var o = new List<CubeLevel>();
            if (p > b.Length) throw new UxException("cube texture size");
            for (int f = 0; f < 6; f++)
                for (int lv = 0; lv < levels; lv++)
                {
                    int at = 56 + 8 * (f * levels + lv);
                    int n = UxBin.I32LE(b, at), pitch = UxBin.I32LE(b, at + 4);
                    if (n < 0 || p + n > b.Length) throw new UxException("cube texture size");
                    o.Add(new CubeLevel { p = (int)p, face = f, level = lv, size = Math.Max(1, size >> lv), pitch = pitch, bytes = n });
                    p += n;
                }
            if (p != b.Length) throw new UxException("cube texture size");
            return (o, fmt);
        }

        /// _inpaint(rgb, light): the light texels of one face level filled from the mean of their non-light 8-neighbours, ring by
        /// ring; what has no neighbour left is scaled to largest channel REFLECTION_LIGHT. float32, numpy's order of sums.
        static float[] Inpaint(float[] rgb, bool[] light, int s)
        {
            var o = (float[])rgb.Clone(); var todo = (bool[])light.Clone();
            var acc = new float[s * s * 3]; var cnt = new float[s * s];
            for (;;)
            {
                bool any = false; foreach (bool t in todo) if (t) { any = true; break; }
                if (!any) break;
                Array.Clear(acc, 0, acc.Length); Array.Clear(cnt, 0, cnt.Length);
                for (int dy = -1; dy <= 1; dy++)
                    for (int dx = -1; dx <= 1; dx++)
                    {
                        if (dy == 0 && dx == 0) continue;
                        // the destination (y, x) reads the source (y + dy, x + dx)
                        for (int y = Math.Max(0, -dy); y < s + Math.Min(0, -dy); y++)
                            for (int x = Math.Max(0, -dx); x < s + Math.Min(0, -dx); x++)
                            {
                                int d = y * s + x, q = (y + dy) * s + x + dx;
                                float k = todo[q] ? 0f : 1f;
                                for (int c = 0; c < 3; c++) acc[d * 3 + c] = (float)(acc[d * 3 + c] + (float)(o[q * 3 + c] * k));
                                cnt[d] = (float)(cnt[d] + k);
                            }
                    }
                bool filled = false;
                var fill = new bool[s * s];
                for (int i = 0; i < s * s; i++) if (todo[i] && cnt[i] > 0) { fill[i] = true; filled = true; }
                if (!filled)
                {
                    for (int i = 0; i < s * s; i++)
                    {
                        if (!todo[i]) continue;
                        float m = Math.Max(Math.Max(o[i * 3], o[i * 3 + 1]), o[i * 3 + 2]);
                        float g = (float)(REFLECTION_LIGHT / Math.Max(m, 1e-6f));
                        for (int c = 0; c < 3; c++) o[i * 3 + c] = (float)(o[i * 3 + c] * g);
                    }
                    break;
                }
                for (int i = 0; i < s * s; i++)
                    if (fill[i]) { for (int c = 0; c < 3; c++) o[i * 3 + c] = (float)(acc[i * 3 + c] / cnt[i]); todo[i] = false; }
            }
            return o;
        }

        /// reflection_cube(b, mode): (bytes, record) of a copy of a Reflex PC cube with the lights dimmed ('dim') or in-painted
        /// ('nolights'). A BGRA8 cube cannot exceed 1: returned unchanged.
        public static (byte[] data, JObj record) ReflectionCube(byte[] b, string mode)
        {
            if (mode != "dim" && mode != "nolights") throw new UxException("reflection mode '" + mode + "'");
            var (lay, fmt) = CubeLayout(b);
            var o = (byte[])b.Clone();
            float limit = mode == "dim" ? REFLECTION_DIM : REFLECTION_LIGHT;
            var texels = new List<object>();
            var rec = new JObj().Set("mode", mode).Set("limit", (double)limit).Set("format", (long)fmt).Set("levels", (long)(lay.Count / 6)).Set("size", (long)lay[0].size)
                .Set("light_texels", texels).Set("max_before", 0.0).Set("max_after", 0.0);
            if (fmt == 21 || fmt == 22) return (o, rec);
            if (fmt != 113) throw new UxException("cube format " + fmt);
            float before = 0f, after = 0f;
            foreach (var L in lay)
            {
                int s = L.size, n = s * s;
                if (L.pitch != s * 8) throw new UxException("cube face " + L.face + " level " + L.level + " pitch " + L.pitch + " (want " + s * 8 + ")");
                if (L.bytes < n * 8) throw new UxException("cube texture size");
                var rgb = new float[n * 3]; var light = new bool[n]; int lights = 0;
                for (int i = 0; i < n; i++)
                {
                    float m = float.NegativeInfinity;
                    for (int c = 0; c < 3; c++) { float v = HalfToFloat(UxBin.U16LE(b, L.p + i * 8 + c * 2)); rgb[i * 3 + c] = v; if (v > m) m = v; if (v > before) before = v; }
                    if (m > REFLECTION_LIGHT) { light[i] = true; lights++; }
                }
                if (lights > 0)
                {
                    float[] nw = mode == "nolights" ? Inpaint(rgb, light, s) : null;
                    for (int i = 0; i < n; i++)
                    {
                        if (!light[i]) continue;
                        float g = 0f;
                        if (nw == null)
                        {
                            float m = Math.Max(Math.Max(rgb[i * 3], rgb[i * 3 + 1]), rgb[i * 3 + 2]);
                            g = Math.Min(1.0f, (float)(REFLECTION_DIM / Math.Max(m, 1e-6f)));
                        }
                        for (int c = 0; c < 3; c++)
                        {
                            float v = nw != null ? nw[i * 3 + c] : Math.Min((float)(rgb[i * 3 + c] * g), REFLECTION_DIM);
                            ushort h = FloatToHalf(v);
                            o[L.p + i * 8 + c * 2] = (byte)h; o[L.p + i * 8 + c * 2 + 1] = (byte)(h >> 8);
                        }
                    }
                    texels.Add(new List<object> { (long)L.face, (long)L.level, (long)lights });
                }
                for (int i = 0; i < n; i++)
                    for (int c = 0; c < 3; c++) { float v = HalfToFloat(UxBin.U16LE(o, L.p + i * 8 + c * 2)); if (v > after) after = v; }
            }
            double mb = UxNp.PyRound(before, 3), ma = UxNp.PyRound(after, 4);
            rec.Set("max_before", mb).Set("max_after", ma);
            if (ma > limit) throw new UxException("cube still holds texels over " + limit.ToString("0.0", System.Globalization.CultureInfo.InvariantCulture) + " after " + mode);
            return (o, rec);
        }

        static UxElem ReflectionAsset(UxElem e)
        {
            foreach (var r in e.children[0].children)
                if (r.Attr("name") as string == "_ReflectionMap")
                    foreach (var x in r.Iter()) if (x.tag == "Asset") return x;
            throw new UxException("AmbientDirectionalLight without a _ReflectionMap asset");
        }
        /// reflection_map(e): (name, type) of an AmbientDirectionalLightRSData entity's _ReflectionMap.
        public static (string name, string type) ReflectionMap(UxElem e) { var a = ReflectionAsset(e); return (a.Attr("name") as string, a.Attr("type") as string); }
        /// reflection_retarget(e, name): points the entity's _ReflectionMap at `name` (type kept); returns the old name.
        public static string ReflectionRetarget(UxElem e, string name)
        {
            var a = ReflectionAsset(e);
            string old = a.Attr("name") as string;
            for (int i = 0; i < a.attrs.Count; i++) if (a.attrs[i].name == "name") a.attrs[i] = new UxAttr("name", name);
            return old;
        }

        /// retarget_particles(e, name): points a ParticleManagerRSData entity's particle / texture / partattr at `name`.
        public static UxElem RetargetParticles(UxElem e, string name)
        {
            var done = new HashSet<string>(StringComparer.Ordinal);
            foreach (var r in e.children[0].children)
            {
                string k = r.Attr("name") as string;
                foreach (var (role, typ) in PM_ROLES)
                    if (k == role) { r.children[0].attrs = new List<UxAttr> { new UxAttr("name", name), new UxAttr("type", typ) }; done.Add(role); }
            }
            foreach (var (role, _) in PM_ROLES) if (!done.Contains(role)) throw new UxException("ParticleManager lacks " + role);
            return e;
        }
        /// particle_manager_assets(e): {role: (name, type)} of a ParticleManagerRSData entity.
        public static Dictionary<string, (string name, string type)> ParticleManagerAssets(UxElem e)
        {
            var o = new Dictionary<string, (string, string)>(StringComparer.Ordinal);
            foreach (var r in e.children[0].children)
            {
                string k = r.Attr("name") as string;
                foreach (var (role, _) in PM_ROLES)
                    if (k == role && r.children.Count > 0) o[k] = (r.children[0].Attr("name") as string, r.children[0].Attr("type") as string);
            }
            return o;
        }
    }
}

// RESTORED LOOK (RESTORATION / PORT ADDITION, w-fresnolook; not the original game): the cut supercross levels SX_Fresno ("FRESNO") and
// SX_FresnoB ("FRESNO B", PLAY > FREE RIDE > RESTORED) drawn with a shipped arena's terrain and light instead of their unfinished ones.
//
// What the cut levels shipped with (w-fresnob, builds/fresno-kit): both terrains use HFTerrain_Basic, a flat grey material (Fresno A
// G_50Gray, Fresno B 30gray) with no detail layers, no dirt tint, no tyre tracks (HFT_GenericTrack_Default); both levels carry the same
// placeholder AmbientDirectionalLight (id 1001832, FMX_BuenaVista's): its colour reference map FMX_BV_ColorRef (the light's radiosity,
// which multiplies the sun on the terrain and on every lit object) is Buena Vista's - a small freestyle arena of cyan / purple / white light
// pools in a 0.25 grey surround - so over the Fresno floors the sun drops to about a quarter and turns blue; its fills and bounce are about a
// third of a shipped arena's, so the baked spherical light map (LIGHT_FRAG_0900SLM, the terrain's ambient term) averages (34, 37, 44) / 255
// against (158, 142, 131) on Fort Dodge B. Fresno B's HDRSettings (key 0.2, min adaptation 0.5, as Kingston B's) hold the exposure at a
// shipped arena's, so B comes out near-black; Fresno A's (key 0.15, min adaptation 0.05) let the exposure rise about seven times higher,
// which lifts the same rig into A's pale, washed-out blue-grey. Measured (builds/fresnolook/runs/diag-*): Fort Dodge B's HDR alone leaves B
// dark, the terrain's static shadow (nostatic) and dynamic shadow (noterrainrecv) change nothing, the donor's light alone lights it.
//
// This look, per level (the table below, the only levels it touches):
//   terrain   the donor's HFTerrain material on the real Reflex path (its shader asset, so its variant; the detail fade, ruts and RT_Bump as
//             on any shipped arena): its four detail layers, dirt tint, detail tiling, bump scalar, reflection map and tyre-track textures,
//             and the donor arena's softness (the tyre-track decal strength) over the whole floor. The donor's top-down map, detail mask and
//             chrominance / luminance map are painted for the donor's own layout, so they are replaced by maps made here from the level's own
//             height map (the layout): the raised track gets the donor's loose dirt layer (mask red), jump faces its clumpy layer (blue), the
//             flat floor around the track its packed layer (green), the far floor its base layer; the top-down map (the terrain beyond
//             384 .. 512 ft) is the same mix of the layers' mean colours, and the chrominance / luminance map is neutral with a slight
//             low-frequency variation. The level keeps its own height map, volume shadow map (Editor_Default_Black: no static shadow) and
//             friction zones; the core's physics data is untouched (client side only).
//   light     the donor's AmbientDirectionalLight (sun direction and colour, the four fills, bounce, specular, light offset, its colour
//             reference map, its own baked spherical light map for the terrain), its DistanceFog and HDRSettings. Nothing in the cut levels
//             is baked for their own light (no volume shadow map, no map shadow), so the donor's sun direction is taken with it. The donor's
//             colour reference map lays its own arena's light pools over the Fresno floor (both are 1024 ft arenas centred on the origin).
//   scenery   Fresno B's stadium roof beams, whose model is lost but whose surface survives (Entry.lostModels).
// REFLEX_RESTORED_LOOK=off draws the levels as shipped (the before renders); REFLEX_RESTORED_DONOR=<level> picks another donor and
// REFLEX_RESTORED_PARTS=terrain,light,fog,hdr a subset of the parts (renders, diagnosis).
using System;
using System.Collections.Generic;
using System.Linq;
using UnityEngine;

namespace Reflex.Port.Content
{
    public static class RestoredLook
    {
        public sealed class Entry
        {
            public string donor;
            public bool terrain = true, light = true, fog = true, hdr = true;
            /// the top-down map's brightness against the layers' mean colours (a shipped arena's top-down map is darker than its layers'
            /// mean: Fort Dodge B's (43, 34, 25) against (71, 57, 43))
            public float topDownScale = 0.65f;
            /// scenery whose model asset is lost but whose surface survives: drawn from the surface (one part per material, one node at the
            /// model's origin), placed as the entity placing `placedLike` (the original entity's matrix), with the original entity's materials
            public (string surface, string placedLike, string[] materials)[] lostModels = Array.Empty<(string, string, string[])>();
        }

        /// The RESTORED levels and the shipped arena each one borrows its look from. Fresno B was built from Fort Dodge B (its package carries
        /// FortDodge_SX_MAIN, its cameras use Fort Dodge B's SX_FD_BF_* animations), and Fort Dodge B's look also rendered best on it; Fresno A
        /// from the renders of the shipped arenas (core/plan/QUEUE.md, w-fresnolook).
        static readonly Dictionary<string, Entry> Table = new Dictionary<string, Entry>(StringComparer.Ordinal)
        {
            // Fresno A: Kingston (renders of A with Fort Dodge, Fort Dodge B, Kingston, Kingston B, Manchester and Springfield, REFLEX-lead builds/fresnolook/sheets/donor-pick*:
            // Kingston's even warm light and mid-brown dirt read best on A's long rhythm lanes; Fort Dodge's paler layers washed out under its
            // light pools, Kingston B's and Fort Dodge B's red pools tinted the floor, Manchester's grey-blue and Springfield's yellow cast)
            ["SX_Fresno"] = new Entry { donor = "SX_Kingston" },
            // the stadium roof beams: model SX_FR_Stadium_Beams is lost (FresnoBRepair: the package stops before it), its surface survives (two
            // parts); the original scene's entity 1005225 placed it with the stadium's matrix (that of Fresno_Stadium, SX_FR_Stadium_Cement)
            // and the materials SX_FR_Walls, SX_FR_Light (builds/fresno-kit 3_Fresno_B_original_damaged/SX_FresnoB.dx9.scene)
            ["SX_FresnoB"] = new Entry { donor = "SX_FortDodgeB",
                lostModels = new[] { ("SX_FR_Stadium_Beams", "SX_FR_Stadium_Cement", new[] { "SX_FR_Walls", "SX_FR_Light" }) } },
        };

        /// The look of a level (null: none, every shipped level). Read when the level's content loads.
        public static Entry For(string level)
        {
            if (level == null || !Table.TryGetValue(level, out var e)) return null;
            string off = Env("REFLEX_RESTORED_LOOK");
            if (off == "off" || off == "0" || off == "original") return null;
            var r = new Entry { donor = e.donor, terrain = e.terrain, light = e.light, fog = e.fog, hdr = e.hdr, topDownScale = e.topDownScale, lostModels = e.lostModels };
            string d = Env("REFLEX_RESTORED_DONOR", false);
            if (!string.IsNullOrEmpty(d)) r.donor = d;
            string parts = Env("REFLEX_RESTORED_PARTS");
            if (!string.IsNullOrEmpty(parts))
            {
                var p = new HashSet<string>(parts.Split(',').Select(s => s.Trim()));
                r.terrain = p.Contains("terrain"); r.light = p.Contains("light"); r.fog = p.Contains("fog"); r.hdr = p.Contains("hdr");
            }
            if (float.TryParse(Env("REFLEX_RESTORED_TOPDOWN"), System.Globalization.NumberStyles.Float, System.Globalization.CultureInfo.InvariantCulture, out float td)) r.topDownScale = td;
            return r;
        }

        static string Env(string name, bool lower = true)
        {
            try { var v = Environment.GetEnvironmentVariable(name); return string.IsNullOrEmpty(v) ? null : lower ? v.Trim().ToLowerInvariant() : v.Trim(); }
            catch (Exception) { return null; }
        }
    }

    public sealed partial class ReflexLevelContent
    {
        /// RESTORED LOOK (RestoredLook.cs): the shipped level whose terrain / light this RESTORED level is drawn with (null: none).
        public string RestoredDonor { get; private set; }
        /// RESTORED LOOK: the donor's colour reference map (null: the level's own, ColorReferenceName).
        internal LevelTexture RestoredColorRef { get; private set; }

        // RESTORATION / PORT ADDITION (w-fresnolook)
        void ApplyRestoredLook(RestoredLook.Entry look)
        {
            ReflexLevelContent donor;
            try { donor = Load(look.donor, new Options { activity = "FreeRide", scenery = false, terrain = true, tracks = true, donorOnly = true }); }
            catch (Exception e) { Warn($"RESTORED LOOK: donor {look.donor}: {e.Message}"); return; }
            try
            {
                RestoredDonor = look.donor;
                var l = lighting; var d = donor.lighting;
                if (look.light)
                {
                    l.sunDirection = d.sunDirection; l.sunColor = d.sunColor;
                    for (int i = 0; i < 4; i++) l.lightColors[i] = d.lightColors[i];
                    l.bounceColor = d.bounceColor; l.specularColor = d.specularColor; l.lightDirectionOffset = d.lightDirectionOffset;
                    l.sideLightsEnabled = d.sideLightsEnabled; l.bounceLightEnabled = d.bounceLightEnabled;
                    // the colour reference (the light's radiosity: the stadium's light pools, multiplying the sun on the terrain and every lit
                    // object, sampled over the terrain's world projection): the donor's texel and its map. The placeholder light's map is
                    // FMX_BuenaVista's (a small freestyle arena with cyan / purple pools in a 0.25 grey surround): over the Fresno floor it
                    // dimmed the sun to a quarter and tinted it blue. The donor's map is not in this level's packages, so it is handed to
                    // object shading here (ReflexObjectShading.ApplyLevelGlobals: RestoredColorRef).
                    l.colorReference = d.colorReference; colorReferenceName = null; RestoredColorRef = null;
                    if (!string.IsNullOrEmpty(donor.colorReferenceName) && donor.HasAsset(donor.colorReferenceName, "texture"))
                        try
                        {
                            var t = ReflexTex.Parse(donor.colorReferenceName, donor.ReadAsset(donor.colorReferenceName, "texture"));
                            if (t != null && !t.cube && t.width > 1) RestoredColorRef = t;
                        }
                        catch (Exception e) { Warn($"RESTORED LOOK: colour reference {donor.colorReferenceName}: {e.Message}"); }
                    // the light's own baked spherical light map (AutoSphericalLightMap: the terrain's ambient term, slot 7)
                    l.autoSphericalLightMap = d.autoSphericalLightMap; l.sphericalLightMap = d.sphericalLightMap;
                    if (terrainData != null && !string.IsNullOrEmpty(d.sphericalLightMap) && donor.terrainData != null
                        && donor.terrainData.textures.TryGetValue(d.sphericalLightMap, out var slm))
                        terrainData.textures[d.sphericalLightMap] = slm;
                }
                if (look.fog)
                {
                    l.fogColorStart = d.fogColorStart; l.fogColorEnd = d.fogColorEnd; l.fogAmountStart = d.fogAmountStart; l.fogAmountEnd = d.fogAmountEnd;
                    l.fogDistanceStart = d.fogDistanceStart; l.fogDistanceEnd = d.fogDistanceEnd;
                }
                if (look.hdr) { l.hdr = d.hdr; l.hasHdrSettings = d.hasHdrSettings; }
                if (look.terrain) RestoredTerrain(donor, look);
                Warn($"RESTORED LOOK: {level} drawn with {look.donor}'s" + (look.terrain ? " terrain" : "") + (look.light ? " light" : "") + (look.fog ? " fog" : "")
                     + (look.hdr ? " HDR" : "") + $" (colour reference {(RestoredColorRef != null ? RestoredColorRef.name : "texel " + l.colorReference)}, sun {l.sunDirection} x {l.sunColor}, fills {string.Join(" ", l.lightColors.Select(c => c.ToString("F2")))}, key {l.hdr.keyValue}, min adaptation {l.hdr.minAdaptation})");
            }
            finally { donor.Dispose(); }
        }

        /// The donor's terrain material over this level's height map (see RestoredLook.cs).
        void RestoredTerrain(ReflexLevelContent donor, RestoredLook.Entry look)
        {
            var ti = terrainData; var dt = donor.terrainData;
            if (ti == null || dt?.material == null || ti.heights == null || ti.size < 2) { Warn("RESTORED LOOK: no terrain to restore"); return; }
            var dm = dt.material;
            if (dm.textures.Count < 9) { Warn($"RESTORED LOOK: donor terrain material {dm.name} has {dm.textures.Count} textures"); return; }
            int n = ti.size;

            // ---- the layout from the height map (feet; D3D rows: row 0 = +z)
            var h = ti.heights; var ft = new float[n * n];
            for (int i = 0; i < ft.Length; i++) ft[i] = h[i] * 2048f;
            var sorted = (float[])ft.Clone(); Array.Sort(sorted);
            float floor = sorted[sorted.Length / 20];   // the arena floor (most of the map is the flat floor)
            // track: raised above the floor (smoothed so the edges blend), faces: the steep sides of the jumps, arena: around the track
            var relief = new float[n * n];
            for (int i = 0; i < relief.Length; i++) relief[i] = ft[i] - floor;
            var reliefS = BoxBlur(relief, n, 2);
            var track = new float[n * n]; var slope = new float[n * n];
            for (int y = 0; y < n; y++)
                for (int x = 0; x < n; x++)
                {
                    int i = y * n + x;
                    track[i] = Smooth(0.35f, 1.6f, reliefS[i]);
                    float gx = ft[y * n + Math.Min(n - 1, x + 1)] - ft[y * n + Math.Max(0, x - 1)];
                    float gz = ft[Math.Min(n - 1, y + 1) * n + x] - ft[Math.Max(0, y - 1) * n + x];
                    slope[i] = Mathf.Sqrt(gx * gx + gz * gz) * 0.5f / Math.Max(1, ti.spacing);   // rise per foot
                }
            slope = BoxBlur(slope, n, 1);
            var arena = BoxBlur(BoxBlur(track, n, 12), n, 12);
            for (int i = 0; i < arena.Length; i++) arena[i] = Smooth(0.01f, 0.12f, arena[i]);

            // ---- the donor layers' mean colours (slots 1..4)
            var means = new Vector3[4];
            for (int k = 0; k < 4; k++)
            {
                means[k] = new Vector3(0.3f, 0.25f, 0.2f);
                try
                {
                    var px = ReflexTex.DecodeExactRgba(donor.ReadAsset(dm.textures[k + 1], "texture"), out int w, out int hh);
                    double r = 0, g = 0, b = 0; int c = w * hh;
                    for (int i = 0; i < c; i++) { r += px[i * 4]; g += px[i * 4 + 1]; b += px[i * 4 + 2]; }
                    means[k] = new Vector3((float)(r / c / 255), (float)(g / c / 255), (float)(b / c / 255));
                }
                catch (Exception e) { Warn($"RESTORED LOOK: detail layer {dm.textures[k + 1]}: {e.Message}"); }
            }
            float[] tint = dm.parameters.TryGetValue("usr_dirt_tint_color", out var tv) ? tv : null;

            // ---- the three maps (Unity rows: row 0 = -z, the D3D row n - 1 - y), RGBA32 with a box-filtered mip chain
            var mask = new byte[n * n * 4]; var top = new byte[n * n * 4]; var cl = new byte[n * n * 4];
            for (int y = 0; y < n; y++)
                for (int x = 0; x < n; x++)
                {
                    int i = y * n + x, o = ((n - 1 - y) * n + x) * 4;
                    float t = track[i], a = arena[i];
                    float wr = t;                                                  // loose dirt on the track
                    float wg = (1f - t) * a * 0.85f;                               // packed dirt on the floor around it
                    float wb = t * Smooth(0.35f, 0.9f, slope[i]) * 0.7f;           // clumps on the jump faces
                    // chrominance / luminance: neutral (alpha 0.5) with a low-frequency variation, a little darker off the arena
                    float lum = 0.5f + 0.035f * (ValueNoise(x / 37f, y / 37f) - 0.5f) * 2f + 0.02f * (ValueNoise(x / 9f + 17f, y / 9f + 5f) - 0.5f) * 2f - 0.05f * (1f - a);
                    mask[o] = B(wr); mask[o + 1] = B(wg); mask[o + 2] = B(wb); mask[o + 3] = 0;
                    cl[o] = 0; cl[o + 1] = 0; cl[o + 2] = 0; cl[o + 3] = B(lum);
                    // the far terrain: the same mix of the layers' means, through the same chrominance / luminance adjustment
                    var c = Vector3.Lerp(Vector3.Lerp(Vector3.Lerp(means[0], means[1], wr), means[2], wg), means[3], wb);
                    float lb = Mathf.Clamp01(2f * lum), lo = Mathf.Clamp01(lum - 0.5f);
                    c = (c * lb + new Vector3(lo, lo, lo)) * look.topDownScale;
                    top[o] = B(c.x); top[o + 1] = B(c.y); top[o + 2] = B(c.z); top[o + 3] = 255;
                }
            string prefix = "RESTORED_" + level + "_";
            var topT = Rgba(prefix + "TopDown", top, n); var maskT = Rgba(prefix + "Mask", mask, n); var clT = Rgba(prefix + "ChromLum", cl, n);

            // ---- the material: the donor's, its layout maps replaced
            var m = new LevelMaterial { name = prefix + dm.name };
            for (int s = 0; s < dm.textures.Count; s++)
            {
                m.textures.Add(s == 0 ? topT.name : s == 5 ? maskT.name : s == 6 ? clT.name : dm.textures[s]);
                m.samplerStates.Add(s < dm.samplerStates.Count ? dm.samplerStates[s] : new Dictionary<int, float>());
            }
            foreach (var kv in dm.parameters) m.parameters[kv.Key] = kv.Value;
            ti.material = m; ti.materialName = m.name; ti.shaderAsset = dt.shaderAsset;
            foreach (var kv in dt.textures) ti.textures[kv.Key] = kv.Value;
            ti.textures[topT.name] = topT; ti.textures[maskT.name] = maskT; ti.textures[clT.name] = clT;
            // the donor's tyre tracks (the level's own are the HFT_GenericTrack_Default placeholder)
            for (int k = 0; k < 4; k++) ti.trackNames[k] = dt.trackNames[k];
            lock (ti.trackTextures) { ti.trackTextures.Clear(); foreach (var kv in dt.trackTextures) ti.trackTextures[kv.Key] = kv.Value; }
            // the donor arena's softness (tyre-track decal strength r, rut depth g) over the whole floor: its most common texel
            Color32 soft = new Color32(100, 101, 0, 255);
            try
            {
                string sn = donor.entities.FirstOrDefault(e => e.Type == "HFTerrainRSData")?.Asset("SoftnessMask");
                if (!string.IsNullOrEmpty(sn))
                {
                    var px = ReflexTex.DecodeExactRgba(donor.ReadAsset(sn, "texture"), out int w, out int hh);
                    var counts = new Dictionary<int, int>();
                    for (int i = 0; i < w * hh; i += 7) { int key = px[i * 4] << 16 | px[i * 4 + 1] << 8 | px[i * 4 + 2]; counts[key] = counts.TryGetValue(key, out int v) ? v + 1 : 1; }
                    int best = counts.OrderByDescending(kv => kv.Value).First().Key;
                    soft = new Color32((byte)(best >> 16), (byte)(best >> 8), (byte)best, 255);
                }
            }
            catch (Exception e) { Warn("RESTORED LOOK: donor softness: " + e.Message); }
            var sp = new byte[16 * 16 * 4];
            for (int i = 0; i < 256; i++) { sp[i * 4] = soft.r; sp[i * 4 + 1] = soft.g; sp[i * 4 + 2] = soft.b; sp[i * 4 + 3] = 255; }
            ti.softness = new LevelTexture { name = prefix + "Softness", width = 16, height = 16, format = TextureFormat.RGBA32 };
            ti.softness.mips.Add(sp);
            Warn($"RESTORED LOOK: terrain {dm.name} ({dt.shaderAsset}) layers [{string.Join(", ", dm.textures.Skip(1).Take(4))}] means [{string.Join(" ", means.Select(v => v.ToString("F2")))}]"
                 + $" tint {(tint != null ? string.Join(" ", tint.Select(f => f.ToString("F3"))) : "-")}, floor {floor:F2} ft, softness {soft}");
        }

        /// RESTORATION (w-fresnolook): the entry's lost models drawn from their surviving surfaces (StageScenery stages 1 and 2).
        void RestoredScenery(int stage, List<Action> jobs)
        {
            var look = RestoredLook.For(level);
            if (look == null || look.lostModels.Length == 0) return;
            foreach (var (surface, placedLike, materials) in look.lostModels)
            {
                if (HasAsset(surface, "model") || !HasAsset(surface, "surface")) continue;   // (the model exists: the scene places it as usual)
                var like = placed.FirstOrDefault(e => e.Asset("ModelAsset") == placedLike);
                if (like == null) { Warn($"RESTORED LOOK: {surface}: no {placedLike} placement"); continue; }
                string name = surface; string[] mats = materials;
                if (stage == 1)
                    jobs.Add(() =>
                    {
                        try { sceneModels[name] = SurfaceModel(name, mats); }
                        catch (Exception ex) { Warn($"RESTORED LOOK: {name}: {ex.Message}"); }
                    });
                else if (stage == 2 && sceneModels.TryGetValue(name, out var md))
                {
                    instances.Add(new SceneryInstance
                    {
                        entityId = -like.id, entityType = "ModelRSData", name = name + " (RESTORED)", model = name, group = Group("ModelRSData", name),
                        section = like.section, matrix = like.Matrix, materials = md.bindingMaterials.ToArray(),
                    });
                    foreach (var n in md.bindingMaterials.Distinct())
                    {
                        string mn = n;
                        jobs.Add(() =>
                        {
                            if (sceneMaterials.ContainsKey(mn)) return;
                            try { sceneMaterials[mn] = LoadMaterial(mn); } catch (Exception ex) { Warn($"material {mn}: {ex.Message}"); }
                        });
                    }
                    Warn($"RESTORED LOOK: {name} drawn from its surface ({md.triangles.Count} parts: {string.Join(", ", md.bindingMaterials)}) at {placedLike}'s placement");
                }
            }
        }

        /// A static model made from a surface alone: every part bound at the model origin, part i to materials[i] (the last one for more parts).
        ModelData SurfaceModel(string name, string[] materials)
        {
            var surfaces = Rfx.ParseSurfaces(ReadAsset(name, "surface"));
            Rfx.Check(surfaces.Length > 0, "no surface parts");
            var md = new ModelData { name = name, lodCount = 1 };
            var pos = new List<Vector3>(); var nrm = new List<Vector3>(); var tan = new List<Vector3>(); var uv0 = new List<Vector2>(); var uv1 = new List<Vector2>(); var col = new List<Color32>();
            bool anyTan = false, anyCol = false;
            for (int k = 0; k < surfaces.Length; k++)
            {
                var s = surfaces[k];
                Rfx.Check(!s.skinned, "skinned surface");
                int offset = pos.Count;
                for (int i = 0; i < s.positions.Length; i++)
                {
                    pos.Add(s.positions[i]); nrm.Add(s.normals[i]);
                    tan.Add(s.tangents != null ? s.tangents[i] : Vector3.right);
                    uv0.Add(new Vector2(s.uv0[i].x, 1f - s.uv0[i].y)); uv1.Add(new Vector2(s.uv1[i].x, 1f - s.uv1[i].y));
                    col.Add(s.colors != null ? s.colors[i] : new Color32(255, 255, 255, 255));
                }
                anyTan |= s.tangents != null; anyCol |= s.colors != null;
                md.triangles.Add(s.Triangles().Select(t => t + offset).ToArray());
                md.bindingMaterials.Add(materials[Math.Min(k, materials.Length - 1)]);
            }
            md.positions = pos.ToArray(); md.normals = nrm.ToArray(); md.uv0 = uv0.ToArray(); md.uv1 = uv1.ToArray();
            md.tangents = anyTan ? tan.ToArray() : null; md.colors = anyCol ? col.ToArray() : null;
            return md;
        }

        static byte B(float v) => (byte)Mathf.RoundToInt(Mathf.Clamp01(v) * 255f);
        static float Smooth(float a, float b, float x) { float t = Mathf.Clamp01((x - a) / (b - a)); return t * t * (3f - 2f * t); }

        /// Separable box blur of radius r (clamped edges).
        static float[] BoxBlur(float[] src, int n, int r)
        {
            var tmp = new float[n * n]; var dst = new float[n * n]; float inv = 1f / (2 * r + 1);
            for (int y = 0; y < n; y++)
            {
                float s = 0; for (int k = -r; k <= r; k++) s += src[y * n + Mathf.Clamp(k, 0, n - 1)];
                for (int x = 0; x < n; x++)
                {
                    tmp[y * n + x] = s * inv;
                    s += src[y * n + Math.Min(n - 1, x + r + 1)] - src[y * n + Math.Max(0, x - r)];
                }
            }
            for (int x = 0; x < n; x++)
            {
                float s = 0; for (int k = -r; k <= r; k++) s += tmp[Mathf.Clamp(k, 0, n - 1) * n + x];
                for (int y = 0; y < n; y++)
                {
                    dst[y * n + x] = s * inv;
                    s += tmp[Math.Min(n - 1, y + r + 1) * n + x] - tmp[Math.Max(0, y - r) * n + x];
                }
            }
            return dst;
        }

        /// Smooth value noise in [0, 1] (a fixed hash: the same map on every load).
        static float ValueNoise(float x, float y)
        {
            int x0 = Mathf.FloorToInt(x), y0 = Mathf.FloorToInt(y); float fx = x - x0, fy = y - y0;
            fx = fx * fx * (3f - 2f * fx); fy = fy * fy * (3f - 2f * fy);
            float H(int a, int b) { uint q = (uint)(a * 374761393 + b * 668265263); q = (q ^ (q >> 13)) * 1274126177u; return ((q ^ (q >> 16)) & 0xffff) / 65535f; }
            return Mathf.Lerp(Mathf.Lerp(H(x0, y0), H(x0 + 1, y0), fx), Mathf.Lerp(H(x0, y0 + 1), H(x0 + 1, y0 + 1), fx), fy);
        }

        /// An RGBA32 LevelTexture with its mip chain (2 x 2 box filter).
        static LevelTexture Rgba(string name, byte[] level0, int n)
        {
            var t = new LevelTexture { name = name, width = n, height = n, format = TextureFormat.RGBA32 };
            t.mips.Add(level0);
            byte[] prev = level0; int s = n;
            while (s > 1)
            {
                int ns = Math.Max(1, s / 2); var m = new byte[ns * ns * 4];
                for (int y = 0; y < ns; y++)
                    for (int x = 0; x < ns; x++)
                        for (int c = 0; c < 4; c++)
                        {
                            int x0 = Math.Min(s - 1, 2 * x), x1 = Math.Min(s - 1, 2 * x + 1), y0 = Math.Min(s - 1, 2 * y), y1 = Math.Min(s - 1, 2 * y + 1);
                            m[(y * ns + x) * 4 + c] = (byte)((prev[(y0 * s + x0) * 4 + c] + prev[(y0 * s + x1) * 4 + c] + prev[(y1 * s + x0) * 4 + c] + prev[(y1 * s + x1) * 4 + c] + 2) / 4);
                        }
                t.mips.Add(m); prev = m; s = ns;
            }
            return t;
        }
    }
}

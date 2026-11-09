// ALIVE LOOK (w-alivereflex, an experiment): the MX vs ATV Alive levels drawn with Reflex's own rendering instead of the Alive translation.
// OPTIONS > OPEN REFLEX > ALIVE LOOK: ALIVE / REFLEX (ReflexSettings.AliveLookReflex); -aliveLook alive|reflex or REFLEX_ALIVE_LOOK=alive|reflex
// override the setting (renders, tests). The look is read when a level's content loads (the next track picks a change up).
//
// ALIVE (main's behaviour): AliveTerrain.cs / AliveTerrainVT (the decal virtual texture), ReflexAliveShadow.hlsl, the light buffer
// (RfxAliveAmbient: bounce volume, fill lights only when enabled), squared albedo, the level's MapShadow, its colour LUT and Alive's post chain.
//
// REFLEX: the level's geometry, object / tree / grass textures and baked volume shadow stay; what Alive's renderer did is replaced by Reflex's:
//   terrain   Reflex/Original HFTerrain's own path (_AliveTerrain 0): a Reflex level's four detail layers, dirt tint, bump scalar and tyre-track
//             textures (the donor below), the RT_Bump ruts / tracks, Reflex's terrain lighting (spherical light / reflection map, sun x shadow
//             visibility x RT_Bump.w) and its fog. Alive's decal composite is baked once into the top-down map (the far terrain, as a Reflex
//             top-down map is) and picks the detail layer per pixel: what it shows (grass, snow, grey rock, else dirt; balanced onto the
//             donor layers' palette by its read-back mean) selects the donor layer of that kind (kind by texture name: LayerKind), layers of
//             one kind shared by brightness (_AliveLook* in the terrain shader); the detail takes part of the composite's brightness, the far
//             terrain the picked layers' hue (Alive's hue came from its colour LUT); the level's own chrominance / luminance map is kept; the
//             specular mask is each donor layer's own (its detail mask's alpha under that layer). No virtual-texture rings. Dirt the composite
//             shows far redder than the picked donor layers (JS7's supercross clay under Renne's sand) takes most of the composite's colour
//             instead ("red clay", _AliveLook3).
//   objects   Reflex/Original Scenery, trees, grass, crowds and vehicles with _RfxAlive.x = 0: the fill-light rig of Reflex (the donor's colours
//             around the level's sun), no squared albedo, Reflex's fog ramp; a level program compiled whole (alive_general, albedo already squared)
//             or unlit is brought back to display values (sqrt).
//   shadows   Reflex's: the level's volume shadow map (as Reflex levels) and Unity's main-light shadow where Reflex uses its VSM stand-in;
//             no MapShadow, no Alive dynamic-shadow term on the forest.
//   light     the donor's AmbientDirectionalLight colours (sun, four fills, bounce, specular, fill elevation), DistanceFog and HDRSettings, with
//             the level's own sun direction (its baked shadows were made for it); Reflex's post chain (no Alive LUT / tonemap); the sky dome's
//             Alive HDR composite through Reflex's SkyEntityComp path as it is (_RfxSkyGamma 1; a tuning knob, see AliveLookSkyGamma).
using System;
using System.Collections.Generic;
using System.Linq;
using UnityEngine;

namespace Reflex.Port.Content
{
    public static class AliveLook
    {
        /// This test build's default (REFLEX). Main would keep ALIVE as the default.
        public const bool DefaultReflex = true;

        /// REFLEX when true. The command line / environment override the saved setting.
        public static bool UseReflex
        {
            get
            {
                string o = Override();
                if (o == "reflex") return true;
                if (o == "alive") return false;
                try { return global::Reflex.Port.Options.ReflexSettings.AliveLookReflex; } catch (Exception) { return DefaultReflex; }
            }
        }

        static string s_override; static bool s_read;
        static string Override()
        {
            if (s_read) return s_override;
            string v = null;
            try
            {
                var a = Environment.GetCommandLineArgs(); int i = Array.IndexOf(a, "-aliveLook");
                if (i >= 0 && i + 1 < a.Length) v = a[i + 1];
                if (string.IsNullOrEmpty(v)) v = Environment.GetEnvironmentVariable("REFLEX_ALIVE_LOOK");
            }
            catch (Exception) { }
            s_override = string.IsNullOrEmpty(v) ? null : v.Trim().ToLowerInvariant(); s_read = true;
            return s_override;
        }

        /// The Reflex level whose environment stands in for an Alive level's (lighting, fog, HDR, terrain detail set, tyre tracks).
        /// Chosen by the kind of place, from renders of the Reflex tracks (builds/alivelook): the French farmland -> Concord (tan dirt, sand, grass,
        /// woods), the snowy Japanese mountain tracks -> Cheyenne (dirt, grass, rock and snow layers, pines), the desert / canyon tracks (Southwest,
        /// Sliderock) -> Renne (dry light dirt, rocks, desert hills), Devil's Playground -> Gibson (its sand dunes), the tropical tracks -> Stillwater
        /// (its red-orange clay: Dalton's grey soil lost the Tropic tracks' red dirt), the Welsh quarries -> Halifax (rocky valley, forest), the Stewart Compound -> Renne (its sandy tan dirt; the user chose it over Bedford, Gibson and Laguna), the supercross stadiums ->
        /// Kingston. REFLEX_ALIVE_DONOR=<level> overrides it (renders).
        public static string Donor(string aliveLevel)
        {
            string env = Environment.GetEnvironmentVariable("REFLEX_ALIVE_DONOR");
            if (!string.IsNullOrEmpty(env)) return env.Trim();
            string l = aliveLevel ?? "";
            if (l.StartsWith("SX_", StringComparison.Ordinal)) return "SX_Kingston";
            if (l.Contains("Farm")) return "NAT_Concord";
            if (l.Contains("Northwest")) return "FR_Cheyenne";
            if (l == "FR_Dunes") return "FR_Gibson";
            if (l.Contains("Southwest") || l == "FR_DLC_Hills") return "NAT_Renne";
            if (l.Contains("Tropic")) return "NAT_Stillwater";
            if (l.Contains("Quarry")) return "TRX_Halifax";
            if (l.Contains("JSComp")) return "NAT_Renne";
            return "NAT_Concord";
        }
    }

    public sealed partial class ReflexLevelContent
    {
        /// ALIVE LOOK REFLEX for this load (an Alive level only; read once when the content loads).
        public bool ReflexLook { get; private set; }
        /// The level the Reflex look took its environment from (null: none).
        public string DonorLevel { get; private set; }
        /// Alive's own rendering for this load: an Alive level in the ALIVE look.
        internal bool AliveShading => AliveLevels.Is(level) && !ReflexLook;

        // the donor's terrain material (its slots 1..4 detail layers, 8 the reflection map) and the spherical maps made for the level's sun
        LevelMaterial donorTerrain;
        internal LevelTexture lookLightMap, lookReflectionMap;
        internal readonly Vector4[] lookLayerMeans = new Vector4[4];   // rgb: the mean colour of each donor detail layer, a: its specular mask

        void ApplyDonor()
        {
            DonorLevel = AliveLook.Donor(level);
            ReflexLevelContent donor;
            try { donor = Load(DonorLevel, new Options { activity = "FreeRide", scenery = false, terrain = true, tracks = true, donorOnly = true }); }
            catch (Exception e) { Warn($"ALIVE LOOK REFLEX: donor {DonorLevel}: {e.Message}"); DonorLevel = null; return; }
            var l = lighting; var d = donor.lighting;
            // the donor's light colours around the level's own sun (its volume shadow map was baked for that direction)
            l.sunColor = d.sunColor; for (int i = 0; i < 4; i++) l.lightColors[i] = d.lightColors[i];
            l.bounceColor = d.bounceColor; l.specularColor = d.specularColor; l.lightDirectionOffset = d.lightDirectionOffset;
            l.colorReference = d.colorReference; colorReferenceName = null;
            l.fogColorStart = d.fogColorStart; l.fogColorEnd = d.fogColorEnd; l.fogAmountStart = d.fogAmountStart; l.fogAmountEnd = d.fogAmountEnd;
            l.fogDistanceStart = d.fogDistanceStart; l.fogDistanceEnd = d.fogDistanceEnd;
            l.hdr = d.hdr; l.hasHdrSettings = d.hasHdrSettings;
            l.autoSphericalLightMap = false; l.sphericalLightMap = null;
            l.sideLightsEnabled = true; l.bounceLightEnabled = true;
            l.aliveBounce = null; l.aliveBounceData = null; l.aliveLutData = null; l.mapShadowName = null;
            // the donor's terrain detail set and tyre tracks
            var dt = donor.terrainData;
            if (dt?.material != null && terrainData != null)
            {
                donorTerrain = dt.material;
                foreach (var kv in dt.textures) terrainData.textures[kv.Key] = kv.Value;
                for (int i = 0; i < 4; i++) terrainData.trackNames[i] = dt.trackNames[i];
                lock (terrainData.trackTextures) { terrainData.trackTextures.Clear(); foreach (var kv in dt.trackTextures) terrainData.trackTextures[kv.Key] = kv.Value; }
                // the spherical light / reflection maps turned from the donor's sun azimuth to the level's (the fill rig sits around the sun)
                string slm = d.autoSphericalLightMap && !string.IsNullOrEmpty(d.sphericalLightMap) && donor.HasAsset(d.sphericalLightMap, "texture") ? d.sphericalLightMap
                           : dt.material.textures.Count > 7 ? dt.material.textures[7] : null;
                string srm = dt.material.textures.Count > 8 ? dt.material.textures[8] : null;
                float rot = Mathf.Atan2(d.sunDirection.z, d.sunDirection.x) - Mathf.Atan2(l.sunDirection.z, l.sunDirection.x);
                var sun = l.sunDirection.normalized;
                try { if (slm != null) lookLightMap = RotatedSphereMap(donor.ReadAsset(slm, "texture"), rot, n => Mathf.Clamp01(Vector3.Dot(n, sun)), "ALIVE LOOK light map (" + slm + ")"); }
                catch (Exception e) { Warn("ALIVE LOOK light map: " + e.Message); }
                try { if (srm != null) lookReflectionMap = RotatedSphereMap(donor.ReadAsset(srm, "texture"), rot, n => Mathf.Pow(Mathf.Clamp01(Vector3.Dot(n, sun)), 8f), "ALIVE LOOK reflection map (" + srm + ")"); }
                catch (Exception e) { Warn("ALIVE LOOK reflection map: " + e.Message); }
                // the mean colour of each detail layer (slots 1..4); a: that layer's specular mask on the donor (the donor detail mask's alpha
                // averaged with the layer's weight in the mask chain: w3 = b, w2 = g (1 - b), w1 = r (1 - g)(1 - b), w0 = the rest)
                var spec = new double[4]; var specW = new double[4];
                if (dt.material.textures.Count > 5)
                    try
                    {
                        var px = ReflexTex.DecodeExactRgba(donor.ReadAsset(dt.material.textures[5], "texture"), out int w, out int h);
                        for (int i = 0; i < w * h; i++)
                        {
                            double r = px[i * 4] / 255.0, g = px[i * 4 + 1] / 255.0, b = px[i * 4 + 2] / 255.0, a = px[i * 4 + 3] / 255.0;
                            double w3 = b, w2 = g * (1 - b), w1 = r * (1 - g) * (1 - b), w0 = 1 - w1 - w2 - w3;
                            spec[0] += a * w0; spec[1] += a * w1; spec[2] += a * w2; spec[3] += a * w3;
                            specW[0] += w0; specW[1] += w1; specW[2] += w2; specW[3] += w3;
                        }
                    }
                    catch (Exception e) { Warn("ALIVE LOOK detail mask: " + e.Message); }
                for (int k = 0; k < 4; k++)
                {
                    var mean = new Vector3(0.5f, 0.5f, 0.5f);
                    if (dt.material.textures.Count > k + 1)
                        try
                        {
                            var px = ReflexTex.DecodeExactRgba(donor.ReadAsset(dt.material.textures[k + 1], "texture"), out int w, out int h);
                            double r = 0, g = 0, b = 0; int n = w * h;
                            for (int i = 0; i < n; i++) { r += px[i * 4]; g += px[i * 4 + 1]; b += px[i * 4 + 2]; }
                            mean = new Vector3((float)(r / n / 255), (float)(g / n / 255), (float)(b / n / 255));
                        }
                        catch (Exception e) { Warn($"ALIVE LOOK detail layer {k}: {e.Message}"); }
                    lookLayerMeans[k] = new Vector4(mean.x, mean.y, mean.z, specW[k] > 1 ? (float)(spec[k] / specW[k]) : 0.25f);
                }
            }
            Warn($"ALIVE LOOK REFLEX: environment of {DonorLevel} (sun colour {l.sunColor}, fog {l.fogDistanceStart}..{l.fogDistanceEnd}), detail layers [{string.Join(", ", donorTerrain?.textures.Skip(1).Take(4) ?? Array.Empty<string>())}] means (a: specular mask) [{string.Join(" ", lookLayerMeans.Select(m => ((Color)m).ToString("F2")))}]");
            donor.Dispose();
        }

        /// A spherical map (the terrain's light / reflection lookup: Unity uv = 0.5 + 0.5 (n.x, n.z) of an upper-hemisphere direction) turned
        /// by `rot` radians about +y, its alpha replaced by alpha(n) (the sun terms of the level's own sun). RGBA32, Unity row order.
        static LevelTexture RotatedSphereMap(byte[] raw, float rot, Func<Vector3, float> alpha, string name)
        {
            var src = ReflexTex.DecodeExactRgba(raw, out int w, out int h);   // D3D rows (row 0 = top = Unity v 1)
            float c = Mathf.Cos(rot), s = Mathf.Sin(rot);
            var o = new byte[w * h * 4];
            Color Sample(float u, float v)
            {
                float x = Mathf.Clamp(u * w - 0.5f, 0, w - 1), y = Mathf.Clamp(v * h - 0.5f, 0, h - 1);
                int x0 = (int)x, y0 = (int)y, x1 = Math.Min(x0 + 1, w - 1), y1 = Math.Min(y0 + 1, h - 1); float fx = x - x0, fy = y - y0;
                Color P(int px, int py) { int r = h - 1 - py; int i = (r * w + px) * 4; return new Color(src[i] / 255f, src[i + 1] / 255f, src[i + 2] / 255f, src[i + 3] / 255f); }
                return Color.Lerp(Color.Lerp(P(x0, y0), P(x1, y0), fx), Color.Lerp(P(x0, y1), P(x1, y1), fx), fy);
            }
            for (int y = 0; y < h; y++)
                for (int x = 0; x < w; x++)
                {
                    float nx = 2f * (x + 0.5f) / w - 1f, nz = 2f * (y + 0.5f) / h - 1f;
                    float len = Mathf.Sqrt(nx * nx + nz * nz);
                    if (len > 1f) { nx /= len; nz /= len; }
                    var n = new Vector3(nx, Mathf.Sqrt(Mathf.Max(0f, 1f - nx * nx - nz * nz)), nz);
                    // the donor direction with the same azimuth relative to its sun
                    float dx = nx * c - nz * s, dz = nx * s + nz * c;
                    var col = Sample(0.5f + 0.5f * dx, 0.5f + 0.5f * dz);
                    int i = (y * w + x) * 4;
                    o[i] = (byte)Mathf.RoundToInt(Mathf.Clamp01(col.r) * 255); o[i + 1] = (byte)Mathf.RoundToInt(Mathf.Clamp01(col.g) * 255);
                    o[i + 2] = (byte)Mathf.RoundToInt(Mathf.Clamp01(col.b) * 255); o[i + 3] = (byte)Mathf.RoundToInt(alpha(n) * 255);
                }
            var t = new LevelTexture { name = name, width = w, height = h, format = TextureFormat.RGBA32 };
            t.mips.Add(o);
            return t;
        }

        /// The terrain material of the REFLEX look: the donor's detail set on Reflex/Original HFTerrain's own path (BuildTerrain).
        void BuildLookTerrain(Material mat, TerrainInfo ti)
        {
            mat.SetFloat("_AliveTerrain", 0f);
            var dm = donorTerrain; var lm = ti.material;
            // (the generic slot loop put the vtex material's own textures in the Reflex slots: clear them; s0 is the decal bake, AliveTerrainVT)
            mat.SetTexture("_TopDownMap", Texture2D.grayTexture);
            mat.SetTexture("_DetailMask", Texture2D.blackTexture);
            mat.SetTexture("_SphericalLightMap", Texture2D.grayTexture);
            mat.SetTexture("_SphericalReflectionMap", Texture2D.blackTexture);
            Texture2D Own(LevelTexture t, TextureWrapMode wrap) { if (t == null) return null; var x = ReflexTex.ToTexture(t, wrap, wrap, FilterMode.Bilinear, 1); Track(x); return x; }
            // s1..s4: the donor's detail layers (its sampler states), s5: the donor's mask (unused: the layer is picked from the top-down map)
            for (int i = 1; i <= 5 && dm != null && i < dm.textures.Count; i++)
            {
                var tex = TerrainTexture(dm.textures[i], dm.samplerStates[i], i <= 4 ? DetailAniso : 1);
                if (tex) mat.SetTexture(TerrainSlotProperties[i], tex);
            }
            // s6: the level's own chrominance / luminance map (HFTerrain_vtex slot 0)
            if (lm.textures.Count > 0) { var cl = TerrainTexture(lm.textures[0], lm.samplerStates[0]); if (cl) mat.SetTexture("_ChromLumMap", cl); }
            var slm = Own(lookLightMap, TextureWrapMode.Clamp); if (slm) mat.SetTexture("_SphericalLightMap", slm);
            var srm = Own(lookReflectionMap, TextureWrapMode.Clamp); if (srm) mat.SetTexture("_SphericalReflectionMap", srm);
            float[] P(string n) => dm != null && dm.parameters.TryGetValue(n, out var v) ? v : null;
            if (P("usr_detail_tile") != null) mat.SetVector("_DetailTile", V4(P("usr_detail_tile")));
            if (P("usr_dirt_tint_color") != null) mat.SetVector("_DirtTint", V4(P("usr_dirt_tint_color")));
            if (P("usr_bump_scalar") != null) mat.SetVector("_BumpScalar", V4(P("usr_bump_scalar")));
            // the terrain shader's ALIVE LOOK inputs: x the detail layer from the top-down map, y the chrominance / luminance adjustment on the
            // top-down map (the bake is Alive's composite before it), z how far the detail takes the composite's brightness, w how sharply layers
            // of one kind are shared by brightness
            mat.SetVector("_AliveLook", new Vector4(1f, 1f, LookFloat("REFLEX_ALIVE_LOOK_MATCH", 0.35f), LookFloat("REFLEX_ALIVE_LOOK_SHARP", 200f)));
            // z: the four layers' mean luminance (information; the shader balances by their mean colour)
            float layersLum = lookLayerMeans.Average(m => 0.299f * m.x + 0.587f * m.y + 0.114f * m.z);
            mat.SetVector("_AliveLook2", new Vector4(LookFloat("REFLEX_ALIVE_LOOK_FARHUE", 0.8f), LookFloat("REFLEX_ALIVE_LOOK_SPEC", 1f), layersLum, LookFloat("REFLEX_ALIVE_LOOK_DEBUG", 0f)));   // y: a scale on the layers' specular masks, w: debug view of the pick
            for (int k = 0; k < 4; k++) mat.SetVector("_AliveLayerMean" + k, lookLayerMeans[k]);
            // red clay (_AliveLook3, w-alivefix): dirt the composite shows far redder than the picked donor layers takes the composite's colour
            // (JS7's supercross clay under Renne's sand). x: how far (0.75: the clay at hue 20 / saturation 0.44 against Alive's shown 25 / 0.53, the
            // full composite colour went pink, hue 17); y..z: the excess redness, (r - b) / (r + g + b) of the balanced composite over the layers'
            // mean, that ramps it in: at 0.06 .. 0.14 and 0.10 .. 0.18 the Compound national track's brown ruts (NAT_JS_Decal_Ruts) went red as
            // well, 0.14 .. 0.22 leaves them Renne's and keeps the supercross clay whole (renders builds/alivelook/tune). No other level's dirt is
            // that much redder than its donor's (renders of ten levels before / after). REFLEX_ALIVE_LOOK_CLAY (0: off), _CLAY_LO, _CLAY_HI override.
            mat.SetVector("_AliveLook3", new Vector4(LookFloat("REFLEX_ALIVE_LOOK_CLAY", 0.75f), LookFloat("REFLEX_ALIVE_LOOK_CLAY_LO", 0.14f), LookFloat("REFLEX_ALIVE_LOOK_CLAY_HI", 0.22f), 0f));
            // each donor layer's kind by its texture name (FR_CH_Snow01, NAT_CO_B_Grass, NAT_RE_B_GroundRocks, ..._Dirt / _Sand: dirt)
            var kinds = new float[4]; var present = new float[4];
            for (int k = 0; k < 4; k++)
            {
                string n = dm != null && dm.textures.Count > k + 1 ? dm.textures[k + 1] : "";
                kinds[k] = LayerKind(n); present[(int)kinds[k]] = 1f;
            }
            if (present[0] == 0f) { kinds[0] = 0f; present[0] = 1f; }   // a dirt layer always (the base when none is named so)
            mat.SetVector("_AliveLayerClass", new Vector4(kinds[0], kinds[1], kinds[2], kinds[3]));
            mat.SetVector("_AliveLayerClassPresent", new Vector4(present[0], present[1], present[2], present[3]));
        }

        /// A Reflex detail layer's kind from its texture name: 2 snow, 1 grass (and leaf litter), 3 rock, 0 dirt (dirt, sand, mud, clay, the rest).
        internal static int LayerKind(string textureName)
        {
            string n = (textureName ?? "").ToLowerInvariant();
            if (n.Contains("snow") || n.Contains("ice")) return 2;
            if (n.Contains("grass") || n.Contains("moss") || n.Contains("leaves") || n.Contains("leaf")) return 1;   // (Stillwater's leaf litter: off the track)
            if (n.Contains("rock") || n.Contains("stone") || n.Contains("gravel") || n.Contains("cliff")) return 3;
            return 0;
        }

        /// The sky's exponent in the REFLEX look (_RfxSkyGamma). Alive's tonemap encodes with 1 / 2.2 and Reflex's chain shows raw values, but the
        /// composite drawn raw through Reflex's chain (as Reflex's own SkyEntityComp skies are) kept a blue sky and the clouds' contrast, where
        /// 1 / 2.2 and 0.7 washed it out to white (renders, builds/alivelook/r3sky*): 1. REFLEX_ALIVE_LOOK_SKYGAMMA overrides it.
        internal static float AliveLookSkyGamma => LookFloat("REFLEX_ALIVE_LOOK_SKYGAMMA", 1f);

        static float LookFloat(string env, float def) =>
            float.TryParse(Environment.GetEnvironmentVariable(env), System.Globalization.NumberStyles.Float, System.Globalization.CultureInfo.InvariantCulture, out float v) ? v : def;
    }
}

// MTX track lighting (w-mtxlight). The imported MTX venues borrow NAT_Concord's light rig, terrain material and post chain; this gives
// each venue its own light from MTX's own level data and draws the imported scenery the way the rest of Reflex is drawn.
//
// What was wrong (core/plan/QUEUE.md "w-mtxlight"):
//   scenery   Reflex's world shaders write raw texture values into the HDR target (ReflexCommon.hlsl, _RfxRawPipeline) and the post chain
//             tonemaps them; the MTX scenery textures were sRGB textures, so every sample was decoded to linear first (0.5 -> 0.21,
//             0.25 -> 0.05): banners, trees, buildings, stands and the sky drew at a third to a half of their brightness, the shadows
//             crushed. MTX itself (PC, D3D8) multiplied texture x prelit vertex colour x 2 in display values; raw sampling is that.
//   fog/HDR   the MTX level kept only Concord's terrain, light and sky entities: its DistanceFog and HDRSettings were dropped, so the
//             venues ran on the executable's default exposure instead of the stock nationals' and had no distance haze. Restored, with
//             the adaptation held to exposures 0.8 .. 1 and the default bloom outdoors: MTX's skies are display-white (1.0), and
//             Concord's 0.8 .. 1.8 band and heavy bloom (made for its dark woods) burnt them and pale sand out.
//   light     every venue had Concord's sun direction and colour, whatever its baked vertex lighting showed.
//   terrain   the venues' authored ground colour (RXPK 4) was decoded as sRGB and then lit by Reflex's sun again (dark, contrasty);
//             Paradise Valley's soil calibration (.42 of Bedford's dirt, made under the old rig) went near-black and is .75 here.
//
// Per venue: MTX's `<Level>_Lights` structure (data/levels/<Level>/<Level>_scripts.qb of the MTX install, the struct MTX's
// set_level_lights / motox_set_all_light_values scripts apply; the numbers are copied here so existing packages need no re-export): the key light (heading / pitch / colour), ambient, fog colour and
// strength, and the world / sky colour that MTX's `lighting` script function multiplies into the prelit vertex colours (lev_*, sky_*,
// vc_target / vc_percent). The key light's heading maps to the package frame as azimuth(atan2(z, x)) = heading + 270 degrees: checked
// against least-squares fits of the sun direction to the baked vertex lighting of ten venues (within 30 degrees on most, see QUEUE.md).
// Supercross stadiums under a roof (Atlanta, Denver, Minneapolis, St. Louis: a black or covered sky and MTX's own overhead key light)
// are lit as indoor arenas; the open-air ones keep the daylight their skies show.
//
// REFLEX_MTX_LIGHT=legacy restores the previous rendering exactly (before / after shots). Stock and Alive tracks never reach this code.
using System;
using System.Collections.Generic;
using UnityEngine;

namespace Reflex.Port.Content
{
    public static class ExperimentalMtxLighting
    {
        /// The terrain light map's sky term outdoors (ReflexLevelContent.MtxSphereLightMap).
        public const float SkyFill = 0.2f;
        /// The sky domes' raw values to this power (ReflexMtxScenery _MtxSkyGamma): the tonemap's shoulder and white point compress bright
        /// saturated skies toward white; 1.4 keeps MTX's blue (the old sRGB decode, ^2.2, made them deep but dark).
        public const float SkyGamma = 1.4f;
        /// The authored ground colour's saturation in the raw path (Reflex's sun and fills are near white and the near detail is grey-normalised).
        public const float GroundSaturation = 1.35f;

        /// False only with REFLEX_MTX_LIGHT=legacy (renders of the previous look).
        public static readonly bool Enabled = !string.Equals(Environment.GetEnvironmentVariable("REFLEX_MTX_LIGHT"), "legacy", StringComparison.OrdinalIgnoreCase);

        /// MTX's level lighting structure, as authored (colours 0..255 with 128 = 1 for lev / sky / vc_target).
        public sealed class Venue
        {
            public string source;                  // MTX level name
            public bool indoor;                    // a covered stadium lit from above
            public Vector3 ambient; public float heading, pitch; public Vector3 key, rim; public float rimHeading, rimPitch;
            public Vector3 fog; public float fogAlpha, fogDist;
            public Vector3 world = Vector3.one * 128, sky = Vector3.one * 128; public float vcTarget, vcPercent;
        }

        static Venue V(string source, bool indoor, float ar, float ag, float ab, float h0, float p0, float r0, float g0, float b0, float h1, float p1, float r1, float g1, float b1,
                       float fr, float fg, float fb, float fa, float fd, float lr = 128, float lg = 128, float lb = 128, float sr = 128, float sg = 128, float sb = 128, float vt = 0, float vp = 0)
            => new Venue
            {
                source = source, indoor = indoor, ambient = new Vector3(ar, ag, ab), heading = h0, pitch = p0, key = new Vector3(r0, g0, b0), rimHeading = h1, rimPitch = p1, rim = new Vector3(r1, g1, b1),
                fog = new Vector3(fr, fg, fb), fogAlpha = fa, fogDist = fd, world = new Vector3(lr, lg, lb), sky = new Vector3(sr, sg, sb), vcTarget = vt, vcPercent = vp,
            };

        /// By the package id's MTX level ("mtx-<level>", McKenna Rock "mtx-mckenna-rock"). Values: MTX's <Level>_Lights.
        static readonly Dictionary<string, Venue> Venues = new Dictionary<string, Venue>(StringComparer.OrdinalIgnoreCase)
        {
            { "southwick",   V("Southwick",   false, 29, 33, 35,  -65, -50, 131, 126, 113,  120,  -8, 40, 30, 10,  110, 110, 145, 80, 1400, 138, 138, 138, 108, 108, 118) },
            { "buddscreek",  V("BuddsCreek",  false, 40, 40, 48,   65, -30, 110,  98,  98,  -90,  52, 60, 60, 60,   80,  80, 110, 60, 1100) },
            { "washougal",   V("Washougal",   false, 30, 33, 50,  -70, -50, 150, 145, 113,  -80, 112, 45, 30,  0,  141, 150, 180, 64, 1200) },
            { "broome",      V("Broome",      false, 33, 41, 55,  100, -65, 170, 163,  80,  150, 130, 32, 22,  0,  250, 217,   6, 13,  300) },
            { "bayou",       V("Bayou",       false, 30, 30, 35,  100, -80,  70,  75,  70,  -70, -70, 15, 20, 15,   64,  64,  64, 100, 800, 148, 148, 148, 128, 128, 128, 50, 10) },
            { "europe",      V("Europe",      false, 33, 41, 55,   70, -45, 110, 110, 110,   60, 120, 40, 30, 15,   80,  80,  96, 64,  800) },
            { "hawaii",      V("Hawaii",      false, 33, 41, 55,  -30, -55, 160, 143,  80,    0, 110, 50, 30,  0,  128, 120, 124, 16,   64) },
            { "australia",   V("Australia",   false, 33, 41, 55,  -60, -70, 175, 170,  90,    0,  95, 60, 30,  0,  100, 100, 100, 45, 1000, 138, 138, 138, 148, 148, 98) },
            { "miami",       V("Miami",       false, 37, 37, 43,  140, -35, 210, 170,  40,  -30,  40, 90, 50,  0,  160, 160, 180, 10,   10, 168, 155, 138, 125, 115, 105) },
            { "phoenix",     V("Phoenix",     false, 35, 42, 50,  -30, -55, 195, 175, 105,    0, 120, 60, 30,  0,  126, 165, 190, 30,  600, 128, 128, 118, 118, 118, 128) },
            { "portland",    V("Portland",    false, 33, 41, 55,  130, -35, 160, 143,  90,    0,  90, 60, 35,  0,   60,  55,  50, 15,   50) },
            { "daytona",     V("Daytona",     false, 33, 41, 55, -120, -35, 170, 140,  90,    0,  90, 60, 30, 10,  115, 122, 142, 20, 1000) },
            { "denver",      V("Denver",      true,  31, 38, 52,    0, -90, 170, 170, 175,    0,  90, 48, 28,  0,   70,  77,  90, 35,  250) },
            { "minneapolis", V("Minneapolis", true,  40, 40, 50,    0, -90, 165, 165, 185,    0,  90, 48, 39, 10,   77,  77,  70, 35,  400) },
            { "atlanta",     V("Atlanta",     true,  43, 43, 43,    0, -85, 150, 150, 130,    0,  90, 40, 30, 15,  128, 128, 128, 30,  400) },
            { "saintlouis",  V("SaintLouis",  true,  33, 41, 55,    0, -95, 160, 153, 150,    0,  90, 50, 30, 11,   83,  83,  80, 40,  275) },
        };
        // the package file names of the catalog (tools/mtx/catalog.py), for packages whose metadata has no id
        static readonly Dictionary<string, string> ByFile = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase)
        {
            { "McKennaRock", "southwick" }, { "ParadiseValley", "buddscreek" }, { "DevilsGate", "washougal" }, { "PacificHills", "broome" },
            { "Boggstown", "bayou" }, { "Woodbridge", "europe" }, { "Hawaii", "hawaii" }, { "Australia", "australia" }, { "LosAngeles", "miami" },
            { "Phoenix", "phoenix" }, { "SanFrancisco", "portland" }, { "Denver", "denver" }, { "Minneapolis", "minneapolis" },
            { "Atlanta", "atlanta" }, { "StLouis", "saintlouis" }, { "Daytona", "daytona" },
        };

        static string s_path; static Venue s_venue;
        /// The bound MTX package's venue (null: no MTX track bound or the lighting is off). Unknown packages get a neutral preset of their kind.
        public static Venue Current
        {
            get
            {
                var b = TrackMods.Bound;
                if (!Enabled || b == null || !b.experimentalTerrain) return null;
                if (s_path == b.folder && s_venue != null) return s_venue;
                Venue v = null; string kind = b.kind;
                try
                {
                    var info = ExperimentalTerrain.Info(b.folder); kind = info.kind ?? kind;
                    string id = info.id ?? "";
                    if (id.StartsWith("mtx-", StringComparison.OrdinalIgnoreCase)) id = id.Substring(4);
                    if (id == "mckenna-rock") id = "southwick";
                    Venues.TryGetValue(id, out v);
                }
                catch (Exception) { }
                if (v == null && ByFile.TryGetValue(System.IO.Path.GetFileNameWithoutExtension(b.folder ?? ""), out var k)) Venues.TryGetValue(k, out v);
                v ??= kind == "Supercross" ? Venues["atlanta"] : Venues["washougal"];
                s_path = b.folder; s_venue = v;
                return v;
            }
        }

        /// Towards the key light, in the package (= Unity world) frame: azimuth atan2(z, x) = heading + 270 degrees, elevation -pitch. Outdoors
        /// the elevation is held to 40 .. 70 degrees: MTX's key light lit only the bikes (the scenery's light is baked), while Reflex's sun
        /// also lights the terrain, where a low key light (Budds Creek's 30 degrees) leaves flat ground dim; the azimuth, which sets the
        /// shadows' direction against the baked lighting, is MTX's.
        public static Vector3 SunDirection(Venue v)
        {
            float e = (v.indoor ? Mathf.Clamp(-v.pitch, 8f, 98f) : Mathf.Clamp(-v.pitch, 40f, 70f)) * Mathf.Deg2Rad, h = v.heading * Mathf.Deg2Rad;
            return new Vector3(Mathf.Cos(e) * Mathf.Sin(h), Mathf.Sin(e), -Mathf.Cos(e) * Mathf.Cos(h)).normalized;
        }

        static float Lum(Vector3 c) => 0.3f * c.x + 0.59f * c.y + 0.11f * c.z;
        /// A colour's hue at luminance 1, `amount` of the way from white.
        internal static Vector3 Hue(Vector3 c, float amount) { float l = Mathf.Max(Lum(c), 1e-3f); return Vector3.Lerp(Vector3.one, c / l, amount); }

        /// MTX's `lighting color = lev sky = sky target percent`: the world's prelit colours pushed toward `target` by `percent`, then times
        /// lev / 128; the sky's times sky / 128. The scenery shader takes (world tint rgb, vc target), (sky tint rgb, vc percent).
        public static Vector4 WorldTint(Venue v) => v == null ? new Vector4(1, 1, 1, 0) : new Vector4(v.world.x / 128f, v.world.y / 128f, v.world.z / 128f, v.vcTarget / 255f);
        public static Vector4 SkyTint(Venue v) => v == null ? new Vector4(1, 1, 1, 0) : new Vector4(v.sky.x / 128f, v.sky.y / 128f, v.sky.z / 128f, v.vcPercent / 100f);

        /// The level's light (Concord's, parsed) turned into the venue's: the key light's direction and colour, the fills around it, the fog
        /// and the exposure band. Open-air venues keep Concord's rig strength and HDR settings; covered arenas take the stock stadium's
        /// (SX_Kingston: a white overhead key, warm fills at 20 degrees, a strong bounce, no fog, the executable's default HDR settings).
        internal static string Apply(LevelLighting l, Venue v)
        {
            var donorSun = l.sunDirection;
            l.sunDirection = SunDirection(v);
            // the key light in MTX's hue (half way: MTX's colours lit vertex-lit bikes, Reflex's sun lights its own programs and the terrain),
            // scaled by how bright MTX authored it relative to a typical sunny venue (150)
            Vector3 key = v.key / 255f;
            float strength = Mathf.Clamp(Lum(key) / (150f / 255f), 0.9f, 1.1f);
            Vector3 amb = Hue(v.ambient, 0.5f);
            var fc = v.fog / 255f; var fogC = new Color(fc.x, fc.y, fc.z, 0);
            if (v.indoor)
            {
                l.sunColor = Hue(key, 0.3f) * 1.0f * strength;
                l.lightDirectionOffset = 20f;
                var back = new Vector3(0.273f, 0.255f, 0.227f); var side = new Vector3(0.382f, 0.357f, 0.318f);
                l.lightColors[0] = Vector3.Scale(back, amb); l.lightColors[1] = Vector3.Scale(side, amb);
                l.lightColors[2] = Vector3.Scale(back, amb); l.lightColors[3] = Vector3.Scale(side, amb);
                l.bounceColor = new Vector3(0.449f, 0.364f, 0.351f);
                l.fogColorStart = l.fogColorEnd = fogC; l.fogAmountStart = l.fogAmountEnd = 0f;
                l.hdr = new Rendering.ReflexHdrSettings(); l.hasHdrSettings = false;
            }
            else
            {
                l.sunColor = Hue(key, 0.5f) * Lum(l.sunColor) * strength;
                // the fills all around: Concord's rig puts its strong fills (front / side B, 0.46) on the sun's side and weak ones (0.19 / 0.24)
                // behind, so a slope turned from the venue's sun (McKenna Rock's hillside by the start) got almost nothing and went near-black.
                // MTX's baked light is mostly ambient (fits of its vertex colours: ambient 0.4..0.6 against a sun term of 0.3..0.4): every fill
                // is the rig's mean x 1.35, and the terrain's light map adds a sky term (SkyFill), so shade reads at about half of the sun side
                var mean = (l.lightColors[0] + l.lightColors[1] + l.lightColors[2] + l.lightColors[3]) * 0.25f;
                for (int i = 0; i < 4; i++) l.lightColors[i] = Vector3.Scale(mean, amb) * 1.35f;
                // fog: MTX's colour (raw display values like everything here), only far away and light: Concord's woods reach 20 % at 250 ft;
                // here it starts at 260 ft and reaches 7 % x MTX's fog alpha / 64 (0.35..1.25) at 750 ft (x MAP SCALE): the venues are open, a
                // nearer or thicker haze laid a grey veil over Devils Gate's hills
                float density = Mathf.Clamp(v.fogAlpha / 64f, 0.35f, 1.25f);
                l.fogColorStart = l.fogColorEnd = fogC;
                l.fogAmountStart = 0f; l.fogAmountEnd *= 0.35f * density; l.fogDistanceEnd *= 3f; l.fogDistanceStart = l.fogDistanceEnd * 0.35f;
                // exposure: Concord's key, its adaptation band narrowed from below. MTX's scenery is display-referred (white = 1) and the
                // tonemap whites out at key / adapted luminance x value >= 0.9: Concord's band lets the exposure reach 1.8 when the metered
                // corner is a dark banner (a white sky and pale ground blown out); held to 0.8 .. 1 (1.3 still washed McKenna Rock's sand out)
                var h = l.hdr;
                h.minAdaptation = Mathf.Max(h.minAdaptation, h.keyValue * h.KeyMultiplier / 1.0f);
                h.maxAdaptation = Mathf.Max(h.maxAdaptation, h.minAdaptation);
                // bloom: the executable's defaults (as the stock stadium), not Concord's (bright pass over 0..1 at 0.55 plus 20 % of the whole
                // scene): made for its dark woods, on MTX's display-white skies it added half again and burnt the clouds out
                var d = new Rendering.ReflexHdrSettings();
                h.bloomAttenuation = d.bloomAttenuation; h.minBloomLuminance = d.minBloomLuminance; h.maxBloomLuminance = d.maxBloomLuminance;
                h.wholeSceneBloomAmount = d.wholeSceneBloomAmount;
            }
            return $"[MTX] lighting {v.source}{(v.indoor ? " (indoor arena)" : "")}: sun {l.sunDirection:F2} (Concord {donorSun:F2}) colour {l.sunColor:F2}, fills {l.lightColors[0]:F2} {l.lightColors[3]:F2}, " +
                   $"fog {l.fogColorStart} amount {l.fogAmountStart:F2}..{l.fogAmountEnd:F2} over {l.fogDistanceStart:F0}..{l.fogDistanceEnd:F0}; " +
                   $"HDR key {l.hdr.keyValue:F3} adaptation {l.hdr.minAdaptation:F3}..{l.hdr.maxAdaptation:F3} bloom {l.hdr.bloomAttenuation:F2} x {l.hdr.minBloomLuminance:F2}..{l.hdr.maxBloomLuminance:F2} + {l.hdr.wholeSceneBloomAmount:F2} (level HDRSettings {(l.hasHdrSettings ? "yes" : "no")}); world tint {WorldTint(v)}, sky tint {SkyTint(v)}";
        }

        /// ENHANCED for MTX: indoor arenas as the stock stadiums (no sky ambient from a sky that is a roof, no scenery shadows from the
        /// overhead "sun"); the open-air venues keep the donor's profile.
        public static Rendering.ReflexEnhancedSky.Profile EnhancedProfile(Rendering.ReflexEnhancedSky.Profile donor)
        {
            var v = Current;
            if (v == null) return donor;
            // ENHANCED meters the whole frame (ORIGINAL: its top-left corner): MTX's bright LDR skies hold its exposure at the band's floor,
            // half of what ORIGINAL shows; its filmic shoulder rolls highlights off, so a bias brings the two looks together without clipping
            if (!v.indoor) return donor.With(p => { p.postExposure = 1.3f; });
            return donor.With(p => { p.sky = null; p.skyAmbient = 0.25f; p.sceneryShadows = false; p.postExposure = 1.15f; });
        }
    }

    public sealed partial class ReflexLevelContent
    {
        Vector3 mtxDonorSun;
        internal string mtxLightReport;

        /// After ParseLighting on an MTX track: the venue's light (ExperimentalMtxLighting.Apply).
        void ApplyMtxLighting()
        {
            var v = ExperimentalMtxLighting.Current;
            if (v == null) return;
            mtxDonorSun = lighting.sunDirection;
            mtxLightReport = ExperimentalMtxLighting.Apply(lighting, v);
            Warn(mtxLightReport);
        }

        /// The terrain's spherical light / reflection maps turned from Concord's sun azimuth to the venue's, their sun terms remade for the
        /// venue's elevation (as the ALIVE look does for its donors). Main thread, while the terrain material is built.
        void BindMtxLightMaps(Material mat, TerrainInfo ti)
        {
            if (ExperimentalMtxLighting.Current == null || mtxDonorSun.sqrMagnitude < 0.5f) return;
            var l = lighting; var sun = l.sunDirection.normalized;
            float rot = Mathf.Atan2(mtxDonorSun.z, mtxDonorSun.x) - Mathf.Atan2(sun.z, sun.x);
            Texture2D Own(LevelTexture t) { if (t == null) return null; var x = ReflexTex.ToTexture(t, TextureWrapMode.Clamp, TextureWrapMode.Clamp, FilterMode.Bilinear, 1); Track(x); return x; }
            string srm = ti.material.textures.Count > 8 ? ti.material.textures[8] : null;
            // the light map made from the venue's rig (rgb = sum sat(N.L_i) C_i over the four fills, a = sat(N.sun): what
            // ps_spherical_light bakes, ReflexLevelVerify.VerifyLightRig); the reflection map turned like the ALIVE look's
            try { var t = Own(MtxSphereLightMap(128)); if (t) mat.SetTexture("_SphericalLightMap", t); }
            catch (Exception e) { Warn("MTX light map: " + e.Message); }
            try { if (srm != null) { var t = Own(RotatedSphereMap(ReadAsset(srm, "texture"), rot, n => Mathf.Pow(Mathf.Clamp01(Vector3.Dot(n, sun)), 8f), "MTX reflection map (" + srm + ")")); if (t) mat.SetTexture("_SphericalReflectionMap", t); } }
            catch (Exception e) { Warn("MTX reflection map: " + e.Message); }
        }

        /// The terrain's spherical light map for the current rig (Unity uv = 0.5 + 0.5 (n.x, n.z) of an upper-hemisphere normal; RGBA32).
        LevelTexture MtxSphereLightMap(int size)
        {
            var l = lighting; var dirs = LightDirections(); var sun = l.sunDirection.normalized; var sky = MtxSkyFill();
            var o = new byte[size * size * 4];
            for (int y = 0; y < size; y++)
                for (int x = 0; x < size; x++)
                {
                    float nx = 2f * (x + 0.5f) / size - 1f, nz = 2f * (y + 0.5f) / size - 1f, len = Mathf.Sqrt(nx * nx + nz * nz);
                    if (len > 1f) { nx /= len; nz /= len; }
                    var n = new Vector3(nx, Mathf.Sqrt(Mathf.Max(0f, 1f - nx * nx - nz * nz)), nz);
                    var rgb = Vector3.zero;
                    for (int i = 0; i < 4; i++) rgb += Mathf.Clamp01(Vector3.Dot(n, dirs[i])) * l.lightColors[i];
                    rgb += sky * (0.5f + 0.5f * n.y);
                    int k = (y * size + x) * 4;
                    o[k] = (byte)Mathf.RoundToInt(Mathf.Clamp01(rgb.x) * 255); o[k + 1] = (byte)Mathf.RoundToInt(Mathf.Clamp01(rgb.y) * 255);
                    o[k + 2] = (byte)Mathf.RoundToInt(Mathf.Clamp01(rgb.z) * 255); o[k + 3] = (byte)Mathf.RoundToInt(Mathf.Clamp01(Vector3.Dot(n, sun)) * 255);
                }
            var t = new LevelTexture { name = "MTX light map (" + (ExperimentalMtxLighting.Current?.source ?? "?") + ")", width = size, height = size, format = TextureFormat.RGBA32 };
            t.mips.Add(o);
            return t;
        }

        /// The light a flat, unshadowed piece of ground receives (the terrain program's sun x 1.25 x N.sun + the spherical light map's
        /// ambient straight up): the venues' authored ground colour is divided by it, so open level ground shows MTX's own colour.
        internal float MtxFlatGroundLight()
        {
            var l = lighting; var sun = l.sunDirection.normalized;
            float s = 1.25f * Mathf.Max(0f, sun.y) * (0.3f * l.sunColor.x + 0.59f * l.sunColor.y + 0.11f * l.sunColor.z);
            // the ambient rig straight up: the four fills at their elevation (the spherical light map is made from them)
            var dirs = LightDirections(); float a = 0;
            for (int i = 0; i < 4; i++) a += Mathf.Clamp01(dirs[i].y) * (0.3f * l.lightColors[i].x + 0.59f * l.lightColors[i].y + 0.11f * l.lightColors[i].z);
            var sky = MtxSkyFill();
            a += 0.3f * sky.x + 0.59f * sky.y + 0.11f * sky.z;
            return s + Mathf.Min(a, 1f);
        }

        /// The terrain light map's sky term (outdoors: MTX's ambient hue x ExperimentalMtxLighting.SkyFill, x (0.5 + 0.5 N.y)); none indoors.
        Vector3 MtxSkyFill()
        {
            var v = ExperimentalMtxLighting.Current;
            return v == null || v.indoor ? Vector3.zero : ExperimentalMtxLighting.Hue(v.ambient, 0.5f) * ExperimentalMtxLighting.SkyFill;
        }
    }
}

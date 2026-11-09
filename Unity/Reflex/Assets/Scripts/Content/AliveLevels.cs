// MX vs ATV Alive levels converted into the player's install (overlay written by _re/formats/alive_install.py, or the
// in-client importer). The registry Database/alive-levels.json lists every installed Alive level: its name, the Alive
// display name / pack / kind, and the per-level sidecars (rock detail tiling, sky HDR composite footer, the menu pictures). Older Sliderock-only
// overlays have no registry: their marker Database/sliderock-map-v1.txt registers FR_DLC_Hills with the old sidecar names.
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using UnityEngine;

namespace Reflex.Port.Content
{
    public static class AliveLevels
    {
        public const string Registry = "alive-levels.json";

        [Serializable]
        public sealed class Entry
        {
            public string level, display_name, pack, kind, rock_detail, sky_hdr, bounce, terrain_decals, color_lut, albedo_programs;
            /// Alive's own menu pictures of the level (alive_pictures / AlivePictures: PC texture v4 sidecars): its loading picture, its
            /// track select map or Free Ride map (OpenContent.AlivePicture). Null for a level installed before they existed.
            public string preview, map;
            public int resources, deferred_entities, collision, starts, entities;
            public float height_min, height_max, bounce_light_scale = 1f;
            /// PORT ADDITION (w-aliveai): the level's race when its install carries the race data (alive_install: the track definition, the
            /// lap pacing, the start gate's subscene); null / an empty event_type for a Free Ride-only level.
            public Race race;
            public string DisplayName => string.IsNullOrEmpty(display_name) ? level : display_name;
        }

        /// The registry's "race" of a level: Alive's activity (National / ShortTrack / Supercross), the Reflex event it races as, the race's
        /// subscene, the most riders its start gate holds, whether the lap pacing came with it.
        [Serializable]
        public sealed class Race
        {
            public string activity, event_type, subscene;
            public int event_type_id, riders;
            public bool lap_pacing;
        }

        [Serializable] sealed class File_ { public int version; public Entry[] levels; }

        static List<Entry> s_levels;
        static string s_dir;
        static readonly object s_gate = new object();
        static Dictionary<string, Dictionary<string, float>> s_rockTiles = new Dictionary<string, Dictionary<string, float>>(StringComparer.OrdinalIgnoreCase);

        /// Every installed Alive level (empty when the install has none). Re-read when the install directory changes.
        public static IReadOnlyList<Entry> All
        {
            get
            {
                string dir;
                try { dir = ReflexInstall.DatabaseDir; } catch { return Array.Empty<Entry>(); }
                lock (s_gate)
                {
                    if (s_levels != null && s_dir == dir) return s_levels;
                    s_dir = dir; s_levels = Read(dir); s_rockTiles.Clear(); s_programs.Clear();
                    return s_levels;
                }
            }
        }

        public static void Reset() { lock (s_gate) { s_levels = null; s_dir = null; s_rockTiles.Clear(); s_programs.Clear(); } }

        static List<Entry> Read(string dir)
        {
            var list = new List<Entry>();
            try
            {
                string path = Path.Combine(dir, Registry);
                if (File.Exists(path))
                {
                    var f = JsonUtility.FromJson<File_>(File.ReadAllText(path));
                    foreach (var e in f?.levels ?? Array.Empty<Entry>())
                        if (!string.IsNullOrEmpty(e.level) && RxFiles.Exists(Path.Combine(dir, e.level + ".dx9.database")) && !list.Any(x => x.level == e.level))
                            list.Add(e);
                }
                else if (File.Exists(Path.Combine(dir, "sliderock-map-v1.txt")) && File.Exists(Path.Combine(dir, "FR_DLC_Hills.dx9.database")))
                    list.Add(new Entry { level = "FR_DLC_Hills", display_name = "Sliderock Canyon", pack = "sliderock_canyon", kind = "Free Ride",
                                         rock_detail = "sliderock-rock-detail.json", sky_hdr = "sliderock-sky-hdr.json", starts = 12 });
            }
            catch (Exception ex) { Debug.LogWarning("[Alive] " + Registry + ": " + ex.Message); }
            // Alive levels packed as Tracks/<level>.rxtrack (core/plan/RXARCHIVE.md) carry their registry entry in the archive's metadata ("alive")
            try
            {
                foreach (var (file, meta) in RxFiles.TrackArchives(Path.GetDirectoryName(dir)))
                {
                    string json = JsonObject(meta, "alive");
                    if (json == null) continue;
                    var e = JsonUtility.FromJson<Entry>(json);
                    if (e == null || string.IsNullOrEmpty(e.level) || list.Any(x => string.Equals(x.level, e.level, StringComparison.OrdinalIgnoreCase))) continue;
                    if (RxFiles.Exists(Path.Combine(dir, e.level + ".dx9.database"))) list.Add(e);
                }
            }
            catch (Exception ex) { Debug.LogWarning("[Alive] packed tracks: " + ex.Message); }
            return list;
        }

        /// The text of a JSON object member ("alive": {...}) of a JSON document, or null (a brace matcher that skips strings).
        internal static string JsonObject(string json, string member)
        {
            if (string.IsNullOrEmpty(json)) return null;
            int at = json.IndexOf("\"" + member + "\"", StringComparison.Ordinal);
            if (at < 0) return null;
            int start = json.IndexOf('{', at);
            if (start < 0) return null;
            int depth = 0; bool str = false;
            for (int i = start; i < json.Length; i++)
            {
                char c = json[i];
                if (str) { if (c == '\\') i++; else if (c == '"') str = false; continue; }
                if (c == '"') str = true;
                else if (c == '{' || c == '[') depth++;
                else if ((c == '}' || c == ']') && --depth == 0) return json.Substring(start, i - start + 1);
            }
            return null;
        }

        public static Entry Get(string level) => string.IsNullOrEmpty(level) ? null : All.FirstOrDefault(e => string.Equals(e.level, level, StringComparison.OrdinalIgnoreCase));
        public static bool Is(string level) => Get(level) != null;

        /// BASE SCALE (w-alivegates; core/plan/MAP_SCALE.md "Base scale", ALIVE_RACING.md "Scale"): the default MAP SCALE of an Alive race level, the
        /// scale its events (its race, its Free Ride) ride at when the setup row says 100 %: Alive's layouts are tighter than Reflex's (track width
        /// 0.72x on Nationals, 0.66x on Supercross), Nationals and short tracks (they race as Nationals) 110 %, Supercross 130 %. The game's own
        /// tracks and the Free Ride-only Alive levels: 1.
        public const int NationalScalePercent = 110, SupercrossScalePercent = 130;
        public static int MapScaleBasePercent(string level)
        {
            var e = Get(level);
            if (e?.race == null || string.IsNullOrEmpty(e.race.event_type)) return 100;
            return string.Equals(e.race.event_type, "Supercross", StringComparison.OrdinalIgnoreCase) ? SupercrossScalePercent : NationalScalePercent;
        }
        public static float MapScaleBase(string level) { int p = MapScaleBasePercent(level); return p == 100 ? 1f : p / 100f; }

        public static string DisplayName(string level) => Get(level)?.DisplayName;

        /// A level sidecar's path (registry file name), or null when absent.
        public static string Sidecar(string file)
        {
            if (string.IsNullOrEmpty(file)) return null;
            string p = Path.Combine(ReflexInstall.DatabaseDir, file);
            return RxFiles.Exists(p) ? p : null;   // (packed with its level: RxFiles)
        }

        /// The sky HDR composite footer sidecar of a level, or null.
        public static string SkyHdrPath(string level)
        {
            var e = Get(level);
            if (e == null || string.IsNullOrEmpty(e.sky_hdr)) return null;
            string p = Path.Combine(ReflexInstall.DatabaseDir, e.sky_hdr);
            return RxFiles.Exists(p) ? p : null;
        }

        /// The albedo of one of a level's own Xbox pixel programs as ops of the scenery shader's generic program (_ALIVE_VM; the
        /// converter's alive_recipe.py: the program executed symbolically, its albedo before the square reduced to texture fetches,
        /// vertex colour and arithmetic). ops: 8 floats per op (code, destination, a, b, p0..p3); slots: the material texture slot of
        /// each fetch binding; lit false: an unlit program (the result is the colour before fog, not an albedo); params: the material
        /// parameters it reads (usr_*: colours, UV offsets); alpha: the register of its oC0.w (-1: none). general 1: a whole program
        /// (alive_general.py: general ops, the albedo already squared; emissive: its unlit added colour's register + 1, 0 none; normal:
        /// the register + 1 of its normal pass's world normal, 0 none; cubes: the material texture slots of its cube maps).
        [Serializable] public sealed class AlbedoProgram { public string shader; public float[] ops; public int result; public int[] slots; public bool lit = true; public string[] @params; public int alpha = -1; public int general; public int emissive; public int normal; public int[] cubes; }
        [Serializable] sealed class ProgramFile { public int version; public AlbedoProgram[] programs; }
        static readonly Dictionary<string, Dictionary<string, AlbedoProgram>> s_programs = new Dictionary<string, Dictionary<string, AlbedoProgram>>(StringComparer.OrdinalIgnoreCase);

        public static AlbedoProgram Program(string level, string shader)
        {
            var e = Get(level);
            if (e == null || string.IsNullOrEmpty(shader) || string.IsNullOrEmpty(e.albedo_programs)) return null;
            lock (s_gate)
            {
                if (!s_programs.TryGetValue(e.level, out var map))
                {
                    map = new Dictionary<string, AlbedoProgram>(StringComparer.Ordinal);
                    try
                    {
                        string path = Path.Combine(ReflexInstall.DatabaseDir, e.albedo_programs);
                        if (RxFiles.Exists(path))
                            foreach (var p in JsonUtility.FromJson<ProgramFile>(RxFiles.ReadAllText(path))?.programs ?? Array.Empty<AlbedoProgram>())
                                if (p?.shader != null && p.ops != null && p.ops.Length % 8 == 0 && p.ops.Length > 0 && p.slots != null
                                    && (p.general == 0 ? p.ops.Length <= 8 * 48 && p.slots.Length <= 6 : p.ops.Length <= 8 * 512 && p.slots.Length <= 12 && (p.cubes == null || p.cubes.Length <= 2)))
                                    map[p.shader] = p;
                    }
                    catch (Exception ex) { Debug.LogWarning("[Alive] " + e.albedo_programs + ": " + ex.Message); }
                    s_programs[e.level] = map;
                }
                return map.TryGetValue(shader, out var v) ? v : null;
            }
        }

        /// The detail tiling literal of an Alive rock program (alive_convert.rock_detail_tile; sidecar {"FR_HI_Rock_01_NoTemp": 10.0, ...}).
        public static float? RockTile(string level, string program)
        {
            var e = Get(level);
            if (e == null || string.IsNullOrEmpty(program) || string.IsNullOrEmpty(e.rock_detail)) return null;
            lock (s_gate)
            {
                if (!s_rockTiles.TryGetValue(e.level, out var tiles))
                {
                    tiles = new Dictionary<string, float>(StringComparer.Ordinal);
                    try
                    {
                        string path = Path.Combine(ReflexInstall.DatabaseDir, e.rock_detail);
                        if (RxFiles.Exists(path))
                            foreach (System.Text.RegularExpressions.Match mt in System.Text.RegularExpressions.Regex.Matches(RxFiles.ReadAllText(path), "\"([^\"]+)\"\\s*:\\s*([0-9.]+)"))
                                if (float.TryParse(mt.Groups[2].Value, System.Globalization.NumberStyles.Float, System.Globalization.CultureInfo.InvariantCulture, out float t) && t > 0f && t <= 64f)
                                    tiles[mt.Groups[1].Value] = t;
                    }
                    catch (Exception ex) { Debug.LogWarning("[Alive] " + e.rock_detail + ": " + ex.Message); }
                    s_rockTiles[e.level] = tiles;
                }
                return tiles.TryGetValue(program, out float v) ? v : (float?)null;
            }
        }
    }
}

namespace Reflex.Port.Content
{
    /// MX vs ATV Alive's deferred light buffer for the client's forward shaders (ReflexCommon.hlsl RfxAlive*): Alive programs square their
    /// albedo (tex * vertex colour)^2 and multiply it by the light buffer, which the directional light program DL_AmdDirLight fills with the
    /// sun (N.L x shadow) plus the fill lights only when the level enables them (SideLightsEnabled: front / back / side A / side B;
    /// BounceLightEnabled: sat(-N.y) x bounce colour), and DL_BounceLight adds the bounce light volume: an ambient cube per voxel (the X / Y / Z
    /// atlases, negative direction in u < 0.5) found through two indirection volumes (offset O, scale S) and weighted by |N| / |N|_1, times
    /// the entity's LightScale (BounceLightGridPS; the ABNC sidecar: alive_environment.bounce).
    public sealed class AliveBounce
    {
        public Vector3 centre, half; public Vector3Int grid, atlas;
        public float lightScale = 1f;
        public Texture3D o, s, x, y, z;

        static readonly int AliveId = Shader.PropertyToID("_RfxAlive");
        static readonly int CentreId = Shader.PropertyToID("_RfxAliveBounceCentre"), HalfId = Shader.PropertyToID("_RfxAliveBounceHalf"),
                            DimsId = Shader.PropertyToID("_RfxAliveBounceDims");
        static readonly int OId = Shader.PropertyToID("_RfxAliveBounceO"), SId = Shader.PropertyToID("_RfxAliveBounceS"),
                            XId = Shader.PropertyToID("_RfxAliveBounceX"), YId = Shader.PropertyToID("_RfxAliveBounceY"), ZId = Shader.PropertyToID("_RfxAliveBounceZ");

        /// Reads an ABNC sidecar (any thread): the raw bytes split into the five volumes; Upload creates the textures (main thread).
        public static AliveBounce Read(string path, float lightScale, out byte[][] volumes)
        {
            volumes = null;
            var b = RxFiles.ReadAllBytes(path);
            Rfx.Check(b.Length >= 4 + 4 + 24 + 24 + 8 && b[0] == 'A' && b[1] == 'B' && b[2] == 'N' && b[3] == 'C' && BitConverter.ToInt32(b, 4) == 1, "Alive bounce sidecar");
            float F(int i) => BitConverter.ToSingle(b, 8 + 4 * i);
            int I(int i) => BitConverter.ToInt32(b, 32 + 4 * i);
            var r = new AliveBounce { centre = new Vector3(F(0), F(1), F(2)), half = new Vector3(F(3), F(4), F(5)),
                                      grid = new Vector3Int(I(0), I(1), I(2)), atlas = new Vector3Int(I(3), I(4), I(5)), lightScale = lightScale };
            int at = 32 + 24 + 8;
            long gsize = (long)r.grid.x * r.grid.y * r.grid.z * 8, asize = (long)r.atlas.x * r.atlas.y * r.atlas.z * 4;
            Rfx.Check(r.grid.x > 0 && r.grid.x <= 256 && r.grid.y > 0 && r.grid.y <= 256 && r.grid.z > 0 && r.grid.z <= 256 &&
                      r.atlas.x > 0 && r.atlas.x <= 512 && r.atlas.y > 0 && r.atlas.y <= 512 && r.atlas.z > 0 && r.atlas.z <= 512 &&
                      b.Length == at + 2 * gsize + 3 * asize, "Alive bounce sidecar size");
            volumes = new byte[5][];
            for (int k = 0; k < 5; k++)
            {
                long n = k < 2 ? gsize : asize;
                volumes[k] = new byte[n]; Buffer.BlockCopy(b, at, volumes[k], 0, (int)n); at += (int)n;
            }
            return r;
        }

        public void Upload(byte[][] volumes)
        {
            Texture3D T(Vector3Int d, TextureFormat f, byte[] data, string name, FilterMode filter)
            {
                var t = new Texture3D(d.x, d.y, d.z, f, false) { name = name, wrapMode = TextureWrapMode.Clamp, filterMode = filter, hideFlags = HideFlags.DontSave };
                t.SetPixelData(data, 0); t.Apply(false, true); return t;
            }
            o = T(grid, TextureFormat.RGBAHalf, volumes[0], "Alive bounce indirection O", FilterMode.Point);
            s = T(grid, TextureFormat.RGBAHalf, volumes[1], "Alive bounce indirection S", FilterMode.Point);
            // the brick atlases hold gamma-encoded light (8-bit, A8R8G8B8): sampled as sRGB, i.e. decoded before the bilinear filter
            Texture3D G(Vector3Int d, byte[] data, string name)
            {
                var t = new Texture3D(d.x, d.y, d.z, UnityEngine.Experimental.Rendering.GraphicsFormat.R8G8B8A8_SRGB, UnityEngine.Experimental.Rendering.TextureCreationFlags.None) { name = name, wrapMode = TextureWrapMode.Clamp, filterMode = FilterMode.Bilinear, hideFlags = HideFlags.DontSave };
                t.SetPixelData(data, 0); t.Apply(false, true); return t;
            }
            x = G(atlas, volumes[2], "Alive bounce X"); y = G(atlas, volumes[3], "Alive bounce Y"); z = G(atlas, volumes[4], "Alive bounce Z");
        }

        public void Release() { foreach (var t in new[] { o, s, x, y, z }) if (t) UnityEngine.Object.Destroy(t); o = s = x = y = z = null; }

        /// The colour grade of an Alive level (ColorLUTEntity; the ALUT sidecar of alive_textures.convert_color_lut): 'ALUT', 1, 32,
        /// then 32^3 RGBA8, red along x, green along y, blue along z. PostProcessMBColorLUTPS samples it with the tonemapped display
        /// colour (LUT coordinate = colour x g_LUTScaleOffset.xy + .zw, the texel centres) after the gamma encode.
        public static byte[] ReadLut(string path)
        {
            var b = RxFiles.ReadAllBytes(path);
            Rfx.Check(b.Length == 12 + 32 * 32 * 32 * 4 && b[0] == 'A' && b[1] == 'L' && b[2] == 'U' && b[3] == 'T' &&
                      BitConverter.ToInt32(b, 4) == 1 && BitConverter.ToInt32(b, 8) == 32, "Alive colour LUT sidecar");
            var v = new byte[32 * 32 * 32 * 4]; Buffer.BlockCopy(b, 12, v, 0, v.Length); return v;
        }

        public static Texture3D UploadLut(byte[] volume)
        {
            var t = new Texture3D(32, 32, 32, TextureFormat.RGBA32, false) { name = "Alive colour LUT", wrapMode = TextureWrapMode.Clamp, filterMode = FilterMode.Bilinear, hideFlags = HideFlags.DontSave };
            t.SetPixelData(volume, 0); t.Apply(false, true); return t;
        }

        static readonly int LutId = Shader.PropertyToID("_RfxAliveLut"), LutTexId = Shader.PropertyToID("_RfxAliveLutTex");

        /// The post chain's colour grade: on for an Alive level with a LUT (Shaders/ReflexPost.shader Tonemap).
        public static void ApplyLut(Texture3D lut)
        {
            Shader.SetGlobalVector(LutId, new Vector4(lut ? 1 : 0, 31f / 32f, 0.5f / 32f, 0));
            if (lut) Shader.SetGlobalTexture(LutTexId, lut);
        }

        /// The Alive globals for one level (a Reflex level: off). sideLights / bounceColour: the AmbientDirectionalLight flags.
        public static void ApplyGlobals(bool alive, bool sideLights, bool bounceColour, AliveBounce bounce)
        {
            bool vol = alive && bounce != null && bounce.o;
            Shader.SetGlobalVector(AliveId, new Vector4(alive ? 1 : 0, sideLights ? 1 : 0, bounceColour ? 1 : 0, vol ? 1 : 0));
            if (!vol) return;
            float ms = Reflex.Port.RxCore.RxMapScale.Current;   // MAP SCALE: the bounce volume's box in the scaled world
            Shader.SetGlobalVector(CentreId, bounce.centre * ms);
            Shader.SetGlobalVector(HalfId, bounce.half * ms);
            Shader.SetGlobalVector(DimsId, new Vector4(bounce.grid.x, bounce.grid.y, bounce.grid.z, bounce.lightScale));
            Shader.SetGlobalTexture(OId, bounce.o); Shader.SetGlobalTexture(SId, bounce.s);
            Shader.SetGlobalTexture(XId, bounce.x); Shader.SetGlobalTexture(YId, bounce.y); Shader.SetGlobalTexture(ZId, bounce.z);
        }
    }
}

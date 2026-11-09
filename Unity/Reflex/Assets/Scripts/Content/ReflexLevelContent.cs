// Level content (scenery, terrain material, tyre-track textures, spawn anchors, lighting and fog) read at run time from the
// player's own Reflex install for any level: Database/<level>.dx9.level/.database/.scene/.package. This is the C# port of the
// Python exporters _re/formats/level_export.py (scenery), alpine_scenery_export.py / start_support_export.py (the same entities
// of the Alpine scene), terrain_material_export.py (terrain material, shadow visibility), track_texture_export.py and the
// scenery material/shader evidence of scenery_material_export.py / rock_material_export.py. Nothing derived from the game is
// cached in the project: decoding happens here, in parallel, on every load (optionally a run-time cache under
// ReflexInstall.CacheDir could be added; at ~1 s per level it is not needed).
//
// Load() does all file reading and decoding (no Unity API besides math types), so it may run on a background thread.
// The Build* methods create the Unity objects (meshes, textures, materials, game objects) and must run on the main thread.
//
// Level layout (Database/<level>.dx9.level lists it): one database <level>, packages "Shared" and "<level>Base"; the scene
// has sections "<level>Base" (static world) and one per activity ("<level>FreeRide": start entities, e.g. Alpine's hovercraft).
// Static entities are the types that carry a ModelAsset/SurfaceAsset/MaterialList triple (ModelRSData, CollisionRSData,
// PhysicsRSData, DropGateRSData, PopUp/PopDownBaleRSData); animated/skinned ones are not instanced.
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using UnityEngine;

namespace Reflex.Port.Content
{
    /// A free-ride start entity (StartEntityFreeRideRSData): D3D row-vector matrix, same axes as Unity.
    public sealed class SpawnAnchor
    {
        public int entityId, index; public string name;
        public float[] matrix;
        public Vector3 position;
        public Vector3 forward, up;
        public Quaternion rotation = Quaternion.identity;
    }

    /// AmbientDirectionalLightRSData + DistanceFogRSData of the level (the values behind the shader globals gSunLight c55,
    /// gSunLightColor c56, the four fill lights c45..c52, gBounceLightColor c54, gDistanceFog* c14/c15/c16/c26).
    public sealed class LevelLighting
    {
        public Vector3 sunDirection = Vector3.up;      // unit vector towards the sun = -(light matrix row 2)
        public Vector3 sunColor = Vector3.one;         // SunLightColor * SunLightColorIntensity
        public readonly Vector3[] lightColors = new Vector3[4];       // back, side A, side B, front (colour * intensity)
        public Vector3 bounceColor;                    // BounceLightColor * intensity
        public Vector4 colorReference = Vector4.one;   // texel of the ambient colour reference map (1x1 Editor_Default_White)
        public string sphericalLightMap;               // asset name of the light's own baked spherical light map
        public float lightDirectionOffset;             // "_Light Direction Offset" (degrees)
        public Color fogColorStart, fogColorEnd;
        public float fogAmountStart, fogAmountEnd, fogDistanceStart, fogDistanceEnd;
        public string volumeShadowMap;                 // ShadowMapRSData.VolumeShadowMapAsset
        public Vector3 specularColor;                  // SpecularLightColor (gSpecularLightColor)
        public bool autoSphericalLightMap;             // AutoSphericalLightMap: the light's own SLM replaces terrain slot 7 at draw time
        // MX vs ATV Alive (AliveBounce): the light buffer's fill terms are optional per level, the bounce light volume replaces them
        public bool sideLightsEnabled = true, bounceLightEnabled = true;
        public AliveBounce aliveBounce; public byte[][] aliveBounceData;
        public byte[] aliveLutData; public Texture3D aliveLut;   // the colour grade (AliveBounce.ReadLut)
        // AmbientDirectionalLight.MapShadow (MapShadowEnabled; DirectionShadowMaskMapPS): a baked sun mask in the volume shadow map's world
        // projection, its green channel multiplies the sun (R8, D3D row order; the texture is bound by ReflexScenery.ApplyShaderGlobals)
        public string mapShadowName; public byte[] mapShadowData; public int mapShadowSize; public Texture2D mapShadow;
        /// HDRSettingsRSData (the executable's defaults when the level has none): the post chain's parameters.
        public Reflex.Port.Rendering.ReflexHdrSettings hdr = new Reflex.Port.Rendering.ReflexHdrSettings();
        public bool hasHdrSettings;
    }

    /// One placed static scenery object.
    public sealed class SceneryInstance
    {
        public int entityId; public string entityType, name, model, group; public string section;
        public float[] matrix; public string[] materials;
        public string adTexture;                       // MassiveAdEntityRSData: "Ad Texture" (replaces the diffuse)
        public Vector4 scroll;                         // ScrollingTextureRSData: U / V seconds for wrap, reverse U / V
        public Transform transform;                    // set by BuildScenery
    }

    public sealed partial class ReflexLevelContent : IDisposable
    {
        public readonly string level;
        public readonly string activity;
        public LevelLighting lighting = new LevelLighting();
        public readonly List<SpawnAnchor> anchors = new List<SpawnAnchor>();
        public readonly List<SceneryInstance> instances = new List<SceneryInstance>();
        public readonly List<string> warnings = new List<string>();
        internal void Warn(string message) { lock (warnings) warnings.Add(message); }
        public double loadSeconds;
        public string report = "";

        // package/database access
        Rfx.Install install; string database; string basePackage; List<string> packageOrder;
        readonly object sync = new object();
        internal List<SceneEntity> entities;
        readonly List<UnityEngine.Object> created = new List<UnityEngine.Object>();

        public sealed class Options
        {
            public string activity = "FreeRide";
            public bool scenery = true, terrain = true, tracks = true;
            public bool parallel = true;
            /// The AmbientDirectionalLight whose values light the level, by entity name (null: the scene's first, as before). UI_Compound has two:
            /// the garage's ("UIAmbientDirectionalLight", at the customize spot 5000 ft up) first, then the compound's own ("AmbientDirectionalLight",
            /// whose spherical light map the terrain uses); the menu world (OpenFrontEnd/OpenMenuWorld) draws the compound with the latter.
            public string ambientLight;
            /// ALIVE LOOK (AliveLook.cs): the level only lends its environment to an Alive level (lighting, terrain material and textures,
            /// tyre tracks): its height, shadow and softness maps are not decoded.
            public bool donorOnly;
        }
        string preferredLight; bool donorOnly;

        ReflexLevelContent(string level, string activity) { this.level = level; this.activity = activity; }

        internal void Track(UnityEngine.Object o) { if (o) lock (created) created.Add(o); }

        public void Dispose()
        {
            lock (created) { foreach (var o in created) if (o) UnityEngine.Object.Destroy(o); created.Clear(); }
        }

        // ---------------------------------------------------------------------------------------- loading

        /// Reads and decodes everything the level needs. Thread-safe (no Unity object is created).
        public static ReflexLevelContent Load(string level, Options options = null)
        {
            options = options ?? new Options();
            var sw = Stopwatch.StartNew();
            var c = new ReflexLevelContent(level, options.activity) { preferredLight = options.ambientLight, donorOnly = options.donorOnly };
            // ALIVE LOOK (Content/AliveLook.cs): an Alive level drawn with Reflex's rendering
            c.ReflexLook = !options.donorOnly && AliveLevels.Is(level) && AliveLook.UseReflex;
            c.Open();
            c.ReadScene();
            c.ParseLighting();
            if (ExperimentalTerrain.For(level)) c.ApplyMtxLighting();   // the MTX venue's own light (ExperimentalMtxLighting.cs)
            c.ParseAnchors();
            c.stageTimes.Add($"open + scene {sw.ElapsedMilliseconds} ms");
            // Stages with a barrier in between (a stage's jobs depend on the previous stage's results); jobs inside a stage run in parallel.
            for (int stage = 1; stage <= 3; stage++)
            {
                var jobs = new List<Action>();
                if (options.scenery) c.StageScenery(stage, jobs);
                if (options.terrain) c.StageAliveTerrain(stage, jobs);   // (before StageTerrain: stage 2 re-slots an Alive terrain material)
                if (options.terrain) c.StageTerrain(stage, jobs);
                if (options.tracks) c.StageTracks(stage, jobs);
                if (options.scenery) c.StageForest(stage, jobs);   // WP-R1: forest + grass (ReflexForest.cs)
                c.RunJobs(jobs, options.parallel);
            }
            c.PostLoad();
            if (c.ReflexLook) { c.ApplyDonor(); c.stageTimes.Add($"ALIVE LOOK REFLEX donor {c.DonorLevel} at {sw.ElapsedMilliseconds} ms"); }
            // RESTORATION / PORT ADDITION (w-fresnolook): the RESTORED Fresno levels drawn with a shipped arena's terrain and light (RestoredLook.cs)
            if (!options.donorOnly && RestoredLook.For(level) is RestoredLook.Entry restored)
            { c.ApplyRestoredLook(restored); c.stageTimes.Add($"RESTORED LOOK donor {c.RestoredDonor} at {sw.ElapsedMilliseconds} ms"); }
            sw.Stop(); c.loadSeconds = sw.Elapsed.TotalSeconds;
            c.report = c.Describe();
            return c;
        }

        /// Load() on a thread-pool thread (for a loading screen); call Build* on the main thread once it completes.
        // Keep content startup responsive on shared/remote machines; callers can explicitly raise this bounded cap.
        public static int DecodeWorkers => int.TryParse(Environment.GetEnvironmentVariable("REFLEX_CONTENT_WORKERS"), out int n)
            ? Math.Max(1, Math.Min(8, n)) : 2;

        public static Task<ReflexLevelContent> LoadAsync(string level, Options options = null) => Task.Run(() => Load(level, options));

        /// Spawn anchors only (scene file parse, a few ms).
        public static List<SpawnAnchor> ReadSpawnAnchors(string level, string activity = "FreeRide")
        {
            var c = new ReflexLevelContent(level, activity);
            c.Open(); c.ReadScene(); c.ParseAnchors();
            return c.anchors;
        }

        public readonly List<string> stageTimes = new List<string>();

        void RunJobs(List<Action> jobs, bool parallel)
        {
            var sw = Stopwatch.StartNew(); long longest = 0;
            if (!parallel)
            {
                foreach (var j in jobs) { var t = Stopwatch.StartNew(); j(); longest = Math.Max(longest, t.ElapsedMilliseconds); }
            }
            else
            {
                var errors = new System.Collections.Concurrent.ConcurrentBag<Exception>();
                Parallel.ForEach(jobs, new ParallelOptions { MaxDegreeOfParallelism = DecodeWorkers }, j =>
                {
                    var t = Stopwatch.StartNew();
                    try { j(); } catch (Exception e) { errors.Add(e); }
                    long ms = t.ElapsedMilliseconds; long cur;
                    while (ms > (cur = System.Threading.Interlocked.Read(ref longest))) System.Threading.Interlocked.CompareExchange(ref longest, ms, cur);
                });
                foreach (var e in errors) Warn(e.GetType().Name + ": " + e.Message);
            }
            stageTimes.Add($"{jobs.Count} jobs {sw.ElapsedMilliseconds} ms (longest {longest} ms)");
        }

        void Open()
        {
            install = ReflexInstall.Packages;
            database = level;
            Rfx.Check(install.Has(level), "level database " + level + " missing in the install");
            var db = install.Db(level); levelDb = db;
            // Packages the level lists in <level>.dx9.level (Shared + <level>Base); the Base package holds the scene's assets.
            var order = new List<string>();
            string levelFile = TrackMods.LevelFile(level, ".dx9.level");   // (a custom track's own: TrackMods)
            if (RxFiles.Exists(levelFile))   // (packed in Tracks/<level>.rxtrack: RxFiles)
                foreach (var n in Rfx.Bxml(RxFiles.ReadAllBytes(levelFile)).All())
                    if (n.tag == "Package" && n.S("name") != null && db.packages.ContainsKey(n.S("name"))) order.Add(n.S("name"));
            basePackage = order.FirstOrDefault(p => p.EndsWith("Base", StringComparison.Ordinal))
                          ?? db.packages.Keys.FirstOrDefault(p => p.EndsWith("Base", StringComparison.Ordinal));
            Rfx.Check(basePackage != null, "no Base package in " + level);
            packageOrder = new List<string> { basePackage };
            foreach (var p in order.Concat(db.packages.Keys)) if (!packageOrder.Contains(p)) packageOrder.Add(p);
            engineDb = install.Db("EngineDependencies");
        }

        /// A resource of the level: the Base package first, then the other packages of the level database, then EngineDependencies.
        /// (The database objects are captured in Open(); lookups afterwards only read immutable dictionaries, so worker threads are safe
        /// even while other code loads further databases through the shared install.)
        internal Rfx.Asset FindAsset(string name, string type)
        {
            if (string.IsNullOrEmpty(name)) return null;
            foreach (var p in packageOrder) { var a = levelDb.Find(p)?.Find(name, type); if (a != null) return a; }
            if (engineDb != null) foreach (var pkg in engineDb.packages.Values) { var a = pkg.Find(name, type); if (a != null) return a; }
            return null;
        }
        Rfx.Database levelDb, engineDb;

        /// ModelInstance_Init (007f12c0) remaps individual non-null LOD0 slots.
        /// A short list leaves remaining bindings alone; extra entries do not add mesh bindings.
        internal static List<string> BindingMaterials(IList<string> bindings, IList<string> overrides)
        {
            var result = new List<string>(bindings);
            if (overrides != null)
                for (int i = 0; i < Math.Min(result.Count, overrides.Count); i++)
                    if (!string.IsNullOrEmpty(overrides[i])) result[i] = overrides[i];
            return result;
        }

        /// Higher LODs use the same scene slot indices, capped by the LOD0 difference count (007f12c0).
        /// The count pass includes null differences; the write pass skips null handles.
        internal static List<string> LodBindingMaterials(IList<string> lod0, IList<string> bindings, IList<string> overrides)
        {
            var result = new List<string>(bindings);
            if (overrides == null) return result;
            int limit = 0;
            for (int i = 0; i < Math.Min(lod0.Count, overrides.Count); i++)
                if (!string.Equals(lod0[i], string.IsNullOrEmpty(overrides[i]) ? null : overrides[i], StringComparison.Ordinal)) limit++;
            int changed = 0;
            for (int i = 0; i < Math.Min(result.Count, overrides.Count); i++)
            {
                if (string.IsNullOrEmpty(overrides[i])) continue;
                if (changed >= limit) break;
                if (!string.Equals(result[i], overrides[i], StringComparison.Ordinal))
                {
                    result[i] = overrides[i];
                    changed++;
                }
            }
            return result;
        }

        internal bool HasAsset(string name, string type) => FindAsset(name, type) != null;

        internal byte[] ReadAsset(string name, string type)
        {
            var a = FindAsset(name, type);
            Rfx.Check(a != null, $"{level}: {name}.{type} missing");
            return install.Read(a);
        }

        void ReadScene()
        {
            string scene = TrackMods.LevelFile(level, ".dx9.scene");
            Rfx.Check(RxFiles.Exists(scene), "scene file " + scene);
            var data = RxFiles.ReadAllBytes(scene);
            var names = ReflexScene.SectionNames(data);
            string baseSection = names.Contains(basePackage) ? basePackage : names.FirstOrDefault(n => n.EndsWith("Base", StringComparison.Ordinal)) ?? names[0];
            string stem = baseSection.EndsWith("Base", StringComparison.Ordinal) ? baseSection.Substring(0, baseSection.Length - 4) : baseSection;
            var wanted = new List<string> { baseSection };
            string act = names.FirstOrDefault(n => n == stem + activity) ?? names.FirstOrDefault(n => n.EndsWith(activity, StringComparison.Ordinal) && n != baseSection);
            if (act != null) wanted.Add(act);
            entities = ReflexScene.Read(data, wanted.ToArray());
            // an MTX track (Content/ExperimentalMtxLighting.cs): Concord's terrain, light and sky, and (unless REFLEX_MTX_LIGHT=legacy) its distance fog and
            // HDR settings: the stock nationals' exposure, not the executable's defaults
            if (ExperimentalTerrain.For(level)) entities = entities.Where(e => e.Type == "HFTerrainRSData" || e.Type == "AmbientDirectionalLightRSData" || e.Type == "SkyRSData"
                || ExperimentalMtxLighting.Enabled && (e.Type == "DistanceFogRSData" || e.Type == "HDRSettingsRSData")).ToList();
            sceneSections = wanted;
        }
        List<string> sceneSections;

        static Vector3 V3(float[] a) => new Vector3(a[0], a[1], a[2]);
        static Vector3 Rgb(SceneEntity e, string color, string intensity)
        {
            var c = e.Floats(color); float k = e.Float(intensity, 1);
            return c == null ? Vector3.zero : new Vector3(c[0] * k, c[1] * k, c[2] * k);
        }

        void ParseLighting()
        {
            var l = lighting;
            var amb = (preferredLight != null ? entities.FirstOrDefault(e => e.Type == "AmbientDirectionalLightRSData" && e.String("Name") == preferredLight) : null)
                      ?? entities.FirstOrDefault(e => e.Type == "AmbientDirectionalLightRSData");
            if (amb != null)
            {
                var m = amb.Matrix;
                l.sunDirection = new Vector3(-m[8], -m[9], -m[10]);
                l.sunColor = Rgb(amb, "SunLightColor", "SunLightColorIntensity");
                l.lightColors[0] = Rgb(amb, "BackLightColor", "BackLightColorIntensity");
                l.lightColors[1] = Rgb(amb, "SideLightAColor", "SideLightAColorIntensity");
                l.lightColors[2] = Rgb(amb, "SideLightBColor", "SideLightBColorIntensity");
                l.lightColors[3] = Rgb(amb, "FrontLightColor", "FrontLightColorIntensity");
                l.bounceColor = Rgb(amb, "BounceLightColor", "BounceLightColorIntensity");
                l.lightDirectionOffset = amb.Float("_Light Direction Offset");
                l.sphericalLightMap = amb.String("SphericalLightMap");
                var spec = amb.Floats("SpecularLightColor"); if (spec != null) l.specularColor = new Vector3(spec[0], spec[1], spec[2]);
                l.autoSphericalLightMap = amb.Bool("AutoSphericalLightMap");
                if (amb.Has("SideLightsEnabled")) l.sideLightsEnabled = amb.Bool("SideLightsEnabled");
                if (amb.Has("BounceLightEnabled")) l.bounceLightEnabled = amb.Bool("BounceLightEnabled");
                if (AliveLevels.Get(level) != null && !ReflexLook && amb.Has("MapShadowEnabled") && amb.Bool("MapShadowEnabled")) l.mapShadowName = amb.Asset("MapShadow");
                colorReferenceName = amb.Asset("_ColorReferenceMap");
            }
            var fog = entities.FirstOrDefault(e => e.Type == "DistanceFogRSData");
            if (fog != null)
            {
                var a = fog.Floats("ColorStart"); var b = fog.Floats("ColorEnd");
                l.fogColorStart = new Color(a[0], a[1], a[2], 0); l.fogColorEnd = new Color(b[0], b[1], b[2], 0);
                l.fogAmountStart = fog.Float("AmountStart"); l.fogAmountEnd = fog.Float("AmountEnd");
                l.fogDistanceStart = fog.Float("DistanceStart"); l.fogDistanceEnd = fog.Float("DistanceEnd");
            }
            l.volumeShadowMap = entities.FirstOrDefault(e => e.Type == "ShadowMapRSData")?.Asset("VolumeShadowMapAsset");
            var alive = AliveLevels.Get(level);
            // MX vs ATV Alive: VolumeShadowEnabled off (NAT_Northwest_B, ST_Northwest) draws the light with DirectionShadowMaskNoVolumePS,
            // i.e. without the level's volume shadow map
            if (alive != null && amb != null && amb.Has("VolumeShadowEnabled") && !amb.Bool("VolumeShadowEnabled")) l.volumeShadowMap = null;
            if (alive != null && !ReflexLook && AliveLevels.Sidecar(alive.bounce) is string bouncePath)
            {
                try { l.aliveBounce = AliveBounce.Read(bouncePath, alive.bounce_light_scale, out l.aliveBounceData); }
                catch (Exception e) { Warn("Alive bounce light: " + e.Message); }
            }
            if (alive != null && !ReflexLook && AliveLevels.Sidecar(alive.color_lut) is string lutPath)
            {
                try { l.aliveLutData = AliveBounce.ReadLut(lutPath); }
                catch (Exception e) { Warn("Alive colour LUT: " + e.Message); }
            }
            // HDRSettingsEntity::vf5 (0040c7b0) copies these into the post chain's globals; FUN_007b3950 holds the defaults
            var hdr = entities.FirstOrDefault(e => e.Type == "HDRSettingsRSData");
            if (hdr != null)
            {
                var h = l.hdr; l.hasHdrSettings = true;
                h.bloomAttenuation = hdr.Float("Bloom Attenuation", h.bloomAttenuation);
                h.adaptationRate = hdr.Float("HDR Adaptation Rate", h.adaptationRate);
                h.maxAdaptation = hdr.Float("Max Camera Adaptation", h.maxAdaptation);
                h.minAdaptation = hdr.Float("Min Camera Adaptation", h.minAdaptation);
                h.maxBloomLuminance = hdr.Float("Max Bloom Luminance", h.maxBloomLuminance);
                h.minBloomLuminance = hdr.Float("Min Bloom Luminance", h.minBloomLuminance);
                h.keyValue = hdr.Float("Key Value", h.keyValue);
                h.wholeSceneBloomAmount = hdr.Float("WholeSceneBloomAmount", h.wholeSceneBloomAmount);
                // MX vs ATV Alive HDRSettings (Sliderock Canyon) author the tonemap white point; Reflex levels have no such field
                if (hdr.Has("White Point") && hdr.Float("White Point") > 0f)
                {
                    h.oneOverWhitePointSquared = 1f / (hdr.Float("White Point") * hdr.Float("White Point"));
                    // MX vs ATV Alive: its own bright pass / tonemap (ReflexHdrSettings.alive), with the level's adaptation band as authored
                    h.alive = true;
                    h.bloomThreshold = hdr.Float("Bloom Threshold", h.bloomThreshold);
                    h.bloomDistribution = hdr.Float("Bloom Distribution", h.bloomDistribution);
                }
            }
        }
        /// The fog amount the shaders use. MX vs ATV Alive's fog amounts are applied squared: the authored AmountStart / AmountEnd of the SX stadiums
        /// (0.09 / 0.18 from distance 0) and of NAT_Farm_B / ST_Farm (0.1) would lift the black level of every pixel by 10 to 18 per cent of the fog
        /// colour at the camera (code 60 to 105 of 255 at the stadiums' exposure floor), while the captures show black shadows and a black stadium
        /// ceiling; squared they stay below 0.01 there and the far haze (amounts 0.3 to 1.0) is kept. Calibrated against the captures, see ALIVE_CONVERSION.md.
        internal float FogAmount(float authored) => AliveShading ? authored * authored : authored;   // (ALIVE LOOK REFLEX: the donor's, as authored)
        string colorReferenceName;
        /// The AmbientDirectionalLight's _ColorReferenceMap (null: none) and the level database's packages in lookup order (object shading).
        internal string ColorReferenceName => colorReferenceName;
        internal IEnumerable<Rfx.Package> LevelPackages { get { foreach (var p in packageOrder) { var k = levelDb?.Find(p); if (k != null) yield return k; } } }

        void ParseAnchors()
        {
            foreach (var e in entities)
            {
                if (e.Type != "StartEntityFreeRideRSData") continue;
                var m = e.Matrix; if (m == null) continue;
                var fwd = new Vector3(m[8], m[9], m[10]); var up = new Vector3(m[4], m[5], m[6]);
                anchors.Add(new SpawnAnchor
                {
                    entityId = e.id, index = e.Int("Index"), name = e.String("Name"), matrix = m,
                    position = new Vector3(m[12], m[13], m[14]), forward = fwd, up = up,
                    rotation = fwd.sqrMagnitude > 1e-6f && up.sqrMagnitude > 1e-6f ? Quaternion.LookRotation(fwd, up) : Quaternion.identity,
                });
            }
            anchors.Sort((a, b) => a.index.CompareTo(b.index));
        }

        void PostLoad()
        {
            // the baked sun mask of the level (SX stadiums, Bryn Carreg, the Stewart Compound): the green channel of a DXT1 map, rows flipped to Unity's v
            if (!string.IsNullOrEmpty(lighting.mapShadowName) && HasAsset(lighting.mapShadowName, "texture"))
            {
                try
                {
                    var rgba = ReflexTex.DecodeExactRgba(ReadAsset(lighting.mapShadowName, "texture"), out int mw, out int mh);
                    if (mw == mh && mw > 1)
                    {
                        var g = new byte[mw * mh];
                        for (int y = 0; y < mh; y++) for (int x = 0; x < mw; x++) g[(mh - 1 - y) * mw + x] = rgba[(y * mw + x) * 4 + 1];
                        lighting.mapShadowData = g; lighting.mapShadowSize = mw;
                    }
                }
                catch (Exception e) { Warn("Alive MapShadow: " + e.Message); }
            }
            // the colour reference map is a 1x1 texel (Editor_Default_White) in every shipped level: its value is the constant the
            // original passes as gColorRefSample / samples through gColorRefProjection
            if (colorReferenceName != null)
            {
                try
                {
                    var raw = ReadAsset(colorReferenceName, "texture");
                    var px = ReflexTex.DecodeExactRgba(raw, out int w, out int h);
                    if (w == 1 && h == 1) lighting.colorReference = new Vector4(px[0] / 255f, px[1] / 255f, px[2] / 255f, px[3] / 255f);
                    // a real map (Kingston's SX_KI_A_ColorRef 512x512): ReflexObjectShading.ApplyLevelGlobals binds it as _RfxColorRefMap;
                    // objects, scenery and terrain sample it at gColorRefProjection (ReflexCommon.hlsl RfxColorRefAt); the 1x1 texel stays white
                }
                catch (Exception e) { Warn("colour reference map: " + e.Message); }
            }
        }

        string Describe()
        {
            var sb = new StringBuilder();
            sb.Append($"{level}: {instances.Count} scenery instances, {sceneModels.Count} models, {sceneMaterials.Count} materials, {sceneTextures.Count} textures");
            if (terrainData != null) sb.Append($", terrain {terrainData.size}x{terrainData.size} spacing {terrainData.spacing}, {terrainData.textures.Count} terrain textures");
            sb.Append($"; sun {lighting.sunDirection} x {lighting.sunColor}, colour ref {lighting.colorReference}, light offset {lighting.lightDirectionOffset} deg");
            sb.Append($", {anchors.Count} spawn anchors, loaded in {loadSeconds:F2} s (stages: {string.Join("; ", stageTimes)})");
            foreach (var w in warnings) sb.Append("\n  warning: ").Append(w);
            return sb.ToString();
        }
    }
}

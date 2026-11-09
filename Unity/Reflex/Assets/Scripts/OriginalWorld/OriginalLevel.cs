using System;
using System.Collections.Generic;
using System.Linq;
using UnityEngine;
using Reflex.Port.Content;

namespace Reflex.Port.OriginalWorld
{
    // The level's content, loaded at run time from the player's own Reflex install (nothing from the game is in the project):
    // static scenery (instanced at the original scene matrices with the hand-ported "Reflex/Original Scenery" shader), the
    // terrain material (nine level textures + usr_* parameters + scene lighting and fog) for OriginalTerrain, the tyre-track
    // textures and softness mask for OriginalTracks, and the start gate (DropGate) objects for OriginalAnimatedObjects.
    //
    // Awake runs before OriginalWorldRuntime.Start (which builds the terrain from the live world), so the material, layers and
    // track textures are ready by then; the load is parallel and takes about a second per level (Content.loadSeconds).
    [DefaultExecutionOrder(-200)]
    [DisallowMultipleComponent]
    public sealed class OriginalLevel : MonoBehaviour
    {
        [Tooltip("Database name of the level in the install: NAT_Concord, FR_Alpine ...")]
        public string level = "NAT_Concord";
        [Tooltip("Activity scene whose start entities are added to the Base scene (the Alpine hovercraft is part of FreeRide).")]
        public string activity = "FreeRide";
        public OriginalWorldRuntime runtime;
        public OriginalTerrain terrain;
        [Tooltip("Shaders (serialized so a player build includes them); looked up by name when empty.")]
        public Shader sceneryShader, skyShader, terrainShader, trackShader, rtBumpShader, aliveDecalShader;
        public bool loadScenery = true;
        [Tooltip("Rotate RenderSettings.sun to the level's sun direction (Unity realtime shadows of the bike/rider agree with the baked lighting).")]
        public bool alignSun = true;
        [Tooltip("Create the start gate (DropGate) entries for the OriginalAnimatedObjects component.")]
        public bool animateGates = true;
        [Tooltip("The original renderer's frame structure on the game camera (Rendering/ReflexRenderPipeline: raw FP16 HDR target, the level's HDR post chain).")]
        public bool originalPipeline = true;
        [Tooltip("Load in Awake (play mode). Off: the owner calls LoadNow / LoadFrom itself (the front end's menu world loads the content on a worker thread).")]
        public bool loadOnAwake = true;
        [Tooltip("The camera the level is drawn for (the pipeline, RT_Bump). Empty: the runtime's chase camera, else Camera.main.")]
        public Camera gameCamera;

        [Header("Result (read only)")]
        public float loadSeconds;
        [Tooltip("Grid spacing of the level's terrain (1 << the scene's GridResolution): 1 Concord, 2 Alpine. Read from the scene.")]
        public int terrainSpacing;
        [TextArea(2, 6)] public string report;

        public ReflexLevelContent Content { get; private set; }
        /// The level's gate cards (UI_StartCard_NN, the X cards, the arrow); null when it has none.
        public OriginalGateCards GateCards { get; private set; }
        public GameObject SceneryRoot { get; private set; }
        /// MAP SCALE: the scale this level's file-built parts were made at (RxMapScale.Current when it loaded; 1 = the game's track).
        public float MapScale { get; private set; } = 1f;
        public OriginalAnimatedObjects Animated { get; private set; }
        public TerrainMaterialBuild TerrainBuild { get; private set; }
        public IReadOnlyList<SpawnAnchor> Anchors => Content != null ? Content.anchors : null;
        public bool Loaded => Content != null;

        void Awake() { if (Application.isPlaying && loadOnAwake) LoadNow(); }

        void OnDestroy() { Unload(); }

        public void Unload()
        {
            if (SceneryRoot) { if (Application.isPlaying) Destroy(SceneryRoot); else DestroyImmediate(SceneryRoot); }
            SceneryRoot = null;
            if (RTBump) RTBump.draws.Clear();
            Content?.Dispose(); Content = null;
        }

        static Shader Find(Shader assigned, string name)
        {
            if (assigned) return assigned;
            var s = Shader.Find(name);
            if (!s) Debug.LogWarning("Shader not found (add it to the scene component or Always Included Shaders): " + name);
            return s;
        }

        /// Loads the level now (also callable in edit mode for tests).
        public void LoadNow() => Load(null);

        /// Builds the level from content already read (ReflexLevelContent.LoadAsync on a worker thread; level / activity are the content's).
        /// The component owns it from here (Unload disposes it).
        public void LoadFrom(ReflexLevelContent content)
        {
            if (content == null) throw new ArgumentNullException(nameof(content));
            level = content.level;
            Load(content);
        }

        void Load(ReflexLevelContent preloaded)
        {
            Unload();
            var sw = System.Diagnostics.Stopwatch.StartNew();
            try
            {
                if (!runtime) runtime = FindAnyObjectByType<OriginalWorldRuntime>();
                if (!terrain) terrain = runtime && runtime.terrain ? runtime.terrain : FindAnyObjectByType<OriginalTerrain>();
                Content = preloaded ?? ReflexLevelContent.Load(level, new ReflexLevelContent.Options { activity = activity, scenery = loadScenery });
                terrainSpacing = Content.TerrainSpacing;
                MapScale = Reflex.Port.RxCore.RxMapScale.Current;   // MAP SCALE: the world's scale (1 in the menu world, career, MotoSkills)
                Content.ApplyShaderGlobals();
                Debug.Log("[OriginalLevel] object shading: " + ReflexObjectShading.ApplyLevelGlobals(Content), this);   // the object programs' reflection / colour reference maps (Reflex/Vehicle)
                if (originalPipeline) SetupPipeline();
                if (alignSun && RenderSettings.sun && Content.lighting.sunDirection.sqrMagnitude > 0.5f)
                    RenderSettings.sun.transform.rotation = Quaternion.LookRotation(-Content.lighting.sunDirection.normalized);
                if (loadScenery)
                {
                    SceneryRoot = Content.BuildScenery(transform, new ReflexLevelContent.SceneryShaders { scenery = Find(sceneryShader, "Reflex/Original Scenery"), sky = Find(skyShader, "Reflex/Original Sky") });
                    // MAP SCALE (core/plan/MAP_SCALE.md): the scenery is built at the level's own matrices under its root, then the root is scaled
                    // about the world origin as the core scales the track (the sky follows the camera in its shader: it keeps its real size)
                    if (MapScale != 1f)
                    {
                        SceneryRoot.transform.localScale = Vector3.one * MapScale;
                        var sky = Content.sky?.transform;   // the sky dome keeps its real size
                        if (sky && sky.parent == SceneryRoot.transform) { sky.localScale /= MapScale; sky.localPosition /= MapScale; }
                        Debug.Log($"[OriginalLevel] {level}: MAP SCALE {MapScale * 100f:0} %");
                    }
                    if (animateGates) CreateGates();
                    OriginalProps.Attach(this);   // WP-8: physics props, animated textures, original LOD / draw distance, night sky
                    OriginalWater.Attach(this);   // WP-R2: rivers, lakes, sea and pools (Content/ReflexWater.cs, Shaders/ReflexWater.shader)
                    OriginalLightGlow.Attach(this);   // WP-R3: light glows, light rays / god rays, fog planes, the sun billboard (Content/ReflexLightGlow.cs, Shaders/ReflexLightGlow.shader)
                    OriginalForest.Attach(this);  // WP-R1: forest (3D trees + billboards) and grass (OriginalForest.cs)
                    OriginalVideoScreens.Attach(this);   // WP-B2/R6: in-level Bink screens (VideoEntity) and the projector beam (OriginalVideoScreens.cs)
                    GateCards = OriginalGateCards.Attach(this);   // the Gate Selection page's cards and arrow (hidden until the page shows them)
                }
                SetupTerrain();
                SetupRTBump();
            }
            catch (Exception e)
            {
                Debug.LogError($"OriginalLevel {level}: {e.Message}\n{e.StackTrace}", this);
                // (w-fb-customtrack) a custom track (Mods/Tracks) whose level does not read stops the event with the reason (EVENT STOPPED names
                // the track) instead of a world without its level; an original level keeps the old behaviour (logged)
                var bound = TrackMods.Bound;
                if (bound != null && string.Equals(bound.level, level, StringComparison.OrdinalIgnoreCase) && runtime)
                {
                    var inner = e is AggregateException ae ? ae.Flatten().InnerExceptions.FirstOrDefault() ?? e : e;
                    runtime.FailLevel("the level " + level + " did not load: " + inner.Message);
                }
            }
            sw.Stop();
            loadSeconds = (float)sw.Elapsed.TotalSeconds;
            report = (Content != null ? Content.report : "not loaded") + $"\nOriginalLevel total (incl. Unity object creation): {loadSeconds:F2} s";
            Debug.Log("[OriginalLevel] " + report, this);
        }

        // The original frame structure on the game camera, with the level's HDRSettingsRSData (the executable's defaults without one).
        public Rendering.ReflexRenderPipeline Pipeline { get; private set; }
        void SetupPipeline()
        {
            var cam = gameCamera ? gameCamera : runtime && runtime.chaseCamera ? runtime.chaseCamera : Camera.main;
            if (!cam) { Debug.LogWarning("OriginalLevel: no game camera for the original render pipeline.", this); return; }
            Pipeline = cam.GetComponent<Rendering.ReflexRenderPipeline>();
            if (!Pipeline) Pipeline = cam.gameObject.AddComponent<Rendering.ReflexRenderPipeline>();
            Pipeline.hdr = Content.lighting.hdr;
            var rt = runtime;
            Pipeline.originalFrame = () => rt ? rt.PresentedGameFrame : double.NaN;   // the eye adaptation steps per original frame
            cam.allowHDR = true;
            // OPTIONS > VIDEO > LIGHTING: ENHANCED (not part of the original; ORIGINAL, the default, is untouched by it)
            var enhanced = cam.GetComponent<Rendering.ReflexEnhancedLighting>();
            if (!enhanced) enhanced = cam.gameObject.AddComponent<Rendering.ReflexEnhancedLighting>();
            enhanced.level = this;
        }

        // The terrain's RT_Bump target (Rendering/ReflexRTBump): the level's HFBumpModelRSData meshes and the tyre tracks, every frame
        // for the game camera.
        public Rendering.ReflexRTBump RTBump { get; private set; }
        void SetupRTBump()
        {
            if (Content == null) return;
            if (!RTBump) RTBump = GetComponent<Rendering.ReflexRTBump>() ?? gameObject.AddComponent<Rendering.ReflexRTBump>();
            RTBump.draws = Content.BuildBumpModels(Find(rtBumpShader, "Hidden/Reflex/RTBump"));
            RTBump.terrain = terrain;
            RTBump.tracks = terrain ? terrain.GetComponent<OriginalTracks>() : null;
            RTBump.targetCamera = gameCamera ? gameCamera : runtime && runtime.chaseCamera ? runtime.chaseCamera : null;
        }

        // The start gate: DropGate entities rotate about their local X when the race starts (_re/notes/start-gate.md); the running
        // original world is asked for them by vtable 0x95fc24 and entity id (identifying dword at +0x14, world matrix at +0x30).
        void CreateGates()
        {
            if (!runtime) return;
            var gates = Content.instances.Where(i => i.entityType == "DropGateRSData" && i.transform).ToArray();
            if (gates.Length == 0) return;
            var animated = runtime.GetComponent<OriginalAnimatedObjects>() ?? runtime.gameObject.AddComponent<OriginalAnimatedObjects>();
            animated.entries = gates.Select(g => new OriginalAnimatedObjects.Entry
                { target = g.transform, vtable = 0x95fc24, idOffset = 0x14, id = (uint)g.entityId, matrixOffset = 0x30 }).ToArray();
            runtime.animatedObjects = animated;
            Animated = animated;
        }

        void SetupTerrain()
        {
            if (!terrain) { Debug.LogWarning("OriginalLevel: no OriginalTerrain in the scene; terrain material not applied.", this); return; }
            TerrainBuild = Content.BuildTerrain(Find(terrainShader, "Reflex/Original HFTerrain"));
            if (TerrainBuild == null) return;
            if (TerrainBuild.material) terrain.materialTemplate = TerrainBuild.material;
            if (TerrainBuild.colorMap)
            {
                // fallback when the original-terrain shader is unavailable: the top-down map stretched over the 2047 * spacing grid
                var layer = new TerrainLayer();
                layer.name = level + " top-down map";
                layer.diffuseTexture = TerrainBuild.colorMap;
                layer.tileSize = new Vector2(2047f * TerrainBuild.spacing, 2047f * TerrainBuild.spacing);
                layer.smoothness = 0.05f;
                terrain.layers = new[] { layer };
            }
            // MX vs ATV Alive: the terrain virtual texture's decal layers (AliveTerrainVT)
            if (Content.AliveDecalCount > 0)
            {
                var vt = terrain.GetComponent<AliveTerrainVT>() ?? terrain.gameObject.AddComponent<AliveTerrainVT>();
                vt.decals = Content.BuildAliveTerrainDecals(Find(aliveDecalShader, "Reflex/Alive Terrain Decal"));
                vt.terrain = terrain; vt.terrainMaterial = TerrainBuild.material;
                vt.targetCamera = gameCamera ? gameCamera : runtime && runtime.chaseCamera ? runtime.chaseCamera : null;
                float extent = TerrainBuild.gridSize * TerrainBuild.spacing;
                vt.terrainExtent = extent > 0 ? extent : 2048f; vt.terrainCentre = Vector2.zero; vt.mapScale = MapScale;
                vt.topDownOnly = Content.ReflexLook;   // ALIVE LOOK REFLEX: the composite only as the Reflex terrain's top-down map (Content/AliveLook.cs)
                Debug.Log($"[OriginalLevel] {level}: Alive terrain decal layers {vt.decals.Count} ({string.Join(", ", vt.decals.ConvertAll(d => d.name + ":" + d.program))})");
            }
            var tracks = terrain.GetComponent<OriginalTracks>() ?? terrain.gameObject.AddComponent<OriginalTracks>();
            tracks.trackShader = Find(trackShader, "Reflex/Original HFT Tracks");
            tracks.trackGeneric = TerrainBuild.trackGeneric; tracks.trackMX = TerrainBuild.trackMX;
            tracks.trackATV = TerrainBuild.trackATV; tracks.trackDefault = TerrainBuild.trackDefault;
            tracks.softnessMask = TerrainBuild.softnessMask;
        }
    }
}

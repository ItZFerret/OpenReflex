// The front end's 3D menu world (a port extra, not the original's front end): the game's own UI world level UI_Compound (MXRegistry UISceneName;
// the original loads it at start-up as its UI world, MXGame_SwitchToUIWorld, and rides it in the garage's test drive and the MotoSkills tutorials,
// but its menus draw only the garage models over 2D layers, _re/notes/garage.md section 4) drawn behind the OPEN REFLEX pages: the dark compound
// with its floodlight poles, light beams and sky, the player's rider standing beside his equipped vehicle on the compound floor.
//   loading   once per front-end scene (OpenShell.Connected): the level's content is read on a worker thread (ReflexLevelContent.LoadAsync:
//             scenery, terrain material, sky, lights; no core runs: the compound has no ambient actors, its particles are the tutorials' road
//             flares), then built on the main thread (OriginalLevel.LoadFrom, the terrain from the height map: OriginalTerrain.Build). The world
//             lives with the front-end scene (the hub, the garage, the options, the career and every other page) and goes with it when an event
//             loads; a picture stands in while it loads or when the level cannot be read (OpenShell's background).
//   camera    one camera with the level's renderer (ReflexRenderPipeline with UI_Compound's HDR settings; LIGHTING: ENHANCED adds
//             ReflexEnhancedLighting, as in a race). Where it looks is a stage's (OpenGarageStage over the hero's FeGarage in world mode): the hub's
//             composition (the subject in the screen's part right of the menu, a slow drift: off with REDUCE MOTION), the garage's tabs.
//   the hero  FeGarage in world mode at the hero spot on the compound's central plateau: the player's MOTOCARD rider (his gear) and his equipped
//             vehicle (the per-bike model choice, mods included), the rider at the vehicle's left in the garage idle (RiderUIAnimations).
//   the walk  entering the menu (and after a vehicle is picked in the garage) the rider walks the vehicle into the spot: the game's push-bike
//             clips (Content/ReflexPushAnim) retargeted to his skeleton, the vehicle moved by the clip's own track, then he settles into the idle:
//             two to three steps with the feet planted between them (ReflexPushAnim.BlendPlanted), no skating over the floor.
//   boards    the compound's four status boards show the event the hub continues / the RACE browser's cursor is on, as the original's StatusBoardEntity does
//             (OpenStatusBoards); the rider's portrait on the MOTOCARD card and the garage's RIDER tab is OpenRiderPortrait.
//   the key   a spotlight on the rider and the vehicle (Reflex/Vehicle's menu term: a soft cone from above the camera's side, the colours
//             scaled, not replaced, and a cool rim on their edges; the menu's sun, whose shadows they cast, turned along it), motivated as one of the
//             compound's floodlights; it follows the shot's subject (the pair, the vehicle on VEHICLE / PARTS, the rider on RIDER / GEAR).
//   preview   the garage's VEHICLE list previews the focused vehicle in the hero spot itself, close and large, the rider out of the picture (the
//             models read on a worker thread first; nothing walks while browsing); a pick walks it in with the rider, leaving returns the equipped
//             thread first; no walk while browsing).
// REFLEX_MENU3D=0 (or OpenReflex.Menu3D = 0 in the preferences) keeps the 2D pictures.
using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using Reflex.Port.Content;
using Reflex.Port.FrontEnd;
using Reflex.Port.OriginalWorld;
using UnityEngine;

namespace Reflex.Port.OpenFrontEnd
{
    /// The menu's clock: unscaled real time, or the fixed steps of a deterministic recording (Time.captureFramerate, OpenMenuWorldProbe).
    public static class MenuTime
    {
        public static float Delta => Time.captureFramerate > 0 ? Time.deltaTime : Time.unscaledDeltaTime;
        public static float Now => Time.captureFramerate > 0 ? Time.time : Time.unscaledTime;
    }

    public sealed class OpenMenuWorld : MonoBehaviour
    {
        public enum State { Loading, Ready, Failed }
        public enum Mode { Hub, Garage, Backdrop }

        public static OpenMenuWorld Instance { get; private set; }
        /// The 3D menu is wanted (REFLEX_MENU3D=0 / the preference OpenReflex.Menu3D = 0: the pictures).
        public static bool Wanted
        {
            get
            {
                var env = Environment.GetEnvironmentVariable("REFLEX_MENU3D");
                if (env == "0") return false;
                if (env == "1") return true;
                try { return PlayerPrefs.GetInt(OpenReflex.Prefs + "Menu3D", 1) != 0; } catch (Exception) { return true; }
            }
        }
        public const string Level = "UI_Compound";
        /// The hero spot (feet; the compound's central plateau, flat and open behind for the walk). y comes from the terrain.
        public static Vector3 HeroSpot = new Vector3(4f, 0f, 16f);
        /// Tests: no drift, no turntable turn (a screenshot must not catch them moving).
        public static bool Still;
        /// Tests: the camera is placed by the test (the stages' asks are ignored).
        public static bool ManualCamera;
        /// The event load in the arena (OpenShell.LoadArena.cs): the camera turns slowly around the rider and the vehicle (degrees a second;
        /// not with REDUCE MOTION).
        public bool LoadingTurn;
        public static float LoadingTurnRate = 6f;

        public State Status { get; private set; } = State.Loading;
        public string Error { get; private set; }
        public Mode Current { get; private set; } = Mode.Hub;
        /// Seconds: the worker's read and decode, the main thread's build, from the start to ready.
        public float ReadSeconds { get; private set; }
        public float BuildSeconds { get; private set; }
        public float ReadySeconds { get; private set; }
        public Camera Camera { get; private set; }
        public OriginalLevel LevelObject { get; private set; }
        public FeGarage Hero { get; private set; }
        /// The hub's framing of the hero (and the backdrop's).
        internal OpenGarageStage HubStage { get; private set; }
        /// Called once when the world is ready (or failed).
        public event Action Changed;
        /// The compound's four status boards (the event under the cursor: OpenStatusBoards); null when they could not be built.
        internal OpenStatusBoards Boards { get; private set; }
        /// The hero's vehicle / rider shown (keys), the walk running.
        public string HeroKey { get; private set; }
        public bool Walking => walk != null;
        public float WalkTime => walk?.t ?? -1f;
        /// What the intro motion is: "push", "push-atv", "drive" (null: none).
        public string WalkKind => walk == null ? null : walk.drive ? "drive" : walk.grip != null ? "push-atv" : "push";

        Task<(ReflexLevelContent content, float[] heights, int samples, int spacing, double seconds)> loading;
        float startedAt;
        GameObject sceneCamera;   // the front-end scene's own camera (black clear), off while the world draws
        OriginalTerrain terrain;
        Light sun;
        float groundY;

        // ------------------------------------------------------------------------------------------------ start

        /// The world of this front-end scene (made and its load started on the first call); null when the 3D menu is not wanted.
        public static OpenMenuWorld Ensure(Transform owner)
        {
            if (Instance) return Instance;
            if (!Wanted || ReflexInstall.Dir == null) return null;
            var go = new GameObject("Menu world (" + Level + ", read from the install at run time)");
            if (owner && owner.gameObject.scene.IsValid()) UnityEngine.SceneManagement.SceneManager.MoveGameObjectToScene(go, owner.gameObject.scene);
            return go.AddComponent<OpenMenuWorld>();
        }

        void Awake()
        {
            Instance = this;
            startedAt = Time.realtimeSinceStartup;
            loading = Task.Run(() =>
            {
                var sw = System.Diagnostics.Stopwatch.StartNew();
                var content = ReflexLevelContent.Load(Level, new ReflexLevelContent.Options { activity = "FreeRide", scenery = true, terrain = true, tracks = false, ambientLight = "AmbientDirectionalLight" });
                content.TerrainHeights(out var h, out int n, out int sp);
                try { ReflexPushAnim.Load(); } catch (Exception) { }   // (read with the level: the walk is ready when the hero is)
                return (content, h, n, sp, sw.Elapsed.TotalSeconds);
            });
        }

        void OnDestroy()
        {
            if (Instance == this) Instance = null;
            Shader.SetGlobalVector(SpotPosId, Vector4.zero);   // (the menu's key light off: races and the garage picture draw the original)
            Boards?.Destroy(); Boards = null;
            Hero?.Destroy(); Hero = null;
            if (sceneCamera) sceneCamera.SetActive(true);
            if (pacedMenu) { pacedMenu = false; try { Reflex.Port.Options.ReflexClientLive.ApplyFrameRate(); } catch (Exception) { } }
            if (loading != null && !loading.IsCompleted) { var t = loading; t.ContinueWith(x => { if (x.Status == TaskStatus.RanToCompletion) x.Result.content?.Dispose(); }); }
        }

        void Update()
        {
            using var perf = Reflex.Port.Platform.ReflexMenuPerf.Measure("menu world update");
            if (Status == State.Loading && loading != null && loading.IsCompleted) Build();
            if (Status != State.Ready) return;
            float dt = MenuTime.Delta;
            Frames(dt);
            ReadBump();
            if (regroundAfterWalk && walk == null && Hero != null) { regroundAfterWalk = false; Hero.Reground(); }
            if (Current != Mode.Garage && HubStage != null)
            {
                HubStage.drift = Vector2.zero;   // (no idle drift: a still shot stays stable to the pixel; the camera moves only between shots and with the walk)
                HubStage.autoTurn = false;
                if (LoadingTurn && walk == null && !Still && !OuMotion.Reduced) HubStage.Orbit(LoadingTurnRate * dt, 0f);   // (the event load's arena: a slow turn around them)
                HubStage.Tick(dt);
            }
            if (Hero != null && walk == null && Hero.HoldIdle) Hero.ResumeIdle();   // (no walk holds him: never a frozen rider)
            if (walk != null && OuMotion.Reduced) FinishWalk();
            if (Hero != null && walk == null && !OuMotion.Reduced) Hero.Animate(dt);
            TickWalk(dt);
            // the camera follows the walk (a tracking shot that settles on the composition as they arrive)
            WalkOffset = Vector3.Lerp(WalkOffset, walkTarget, 1f - Mathf.Exp(-dt * TrackRate));
            if (HubStage != null) HubStage.aimOffset = WalkOffset;
            TickPreview();
            Boards?.Tick();
        }

        void LateUpdate()
        {
            if (Status != State.Ready || !Camera || ManualCamera) return;
            float dt = MenuTime.Delta;
            if (!camHave) return;
            if (camSnap) { camEye = wantEye; camLook = wantLook; camSnap = false; easeLeft = 0; }
            else if (easeLeft > 0)
            {   // a change of shot: from where the camera was to where the shot wants it, eased in and out over the time (smoothstep)
                easeLeft = Mathf.Max(0, easeLeft - dt);
                float u = 1f - easeLeft / Mathf.Max(1e-3f, easeTotal); u = u * u * (3f - 2f * u);
                camEye = Vector3.Lerp(easeEye, wantEye, u); camLook = Vector3.Lerp(easeLook, wantLook, u);
            }
            else
            {
                float k = 1f - Mathf.Exp(-dt * 14f);
                camEye = Vector3.Lerp(camEye, wantEye, k); camLook = Vector3.Lerp(camLook, wantLook, k);
                if ((camEye - wantEye).sqrMagnitude < 1e-6f && (camLook - wantLook).sqrMagnitude < 1e-6f) { camEye = wantEye; camLook = wantLook; }
            }
            Camera.transform.SetPositionAndRotation(camEye, Quaternion.LookRotation(camLook - camEye, Vector3.up));
            UpdateSpot(dt);
        }

        // ------------------------------------------------------------------------------------------------ the key light

        /// The key light: gain over the program's colours at the cone's centre, the rim's gain and colour, the cone (degrees: outer, inner half
        /// angles), where it hangs (feet: behind the camera's side of the subject, the turn towards the camera's right, above it).
        public static float SpotGain = 1.15f, SpotRim = 0.55f, SpotOuter = 19f, SpotInner = 9f, SpotWrap = 0.35f, SpotBack = 12f, SpotSide = 38f, SpotHeight = 17f;
        public static Color SpotRimColor = new Color(0.30f, 0.33f, 0.40f);
        /// Where the key aims (world); null: the hub's subject. The garage sets its stage's aim (the vehicle, the rider, a part).
        public Vector3? SpotAim;
        Vector3 spotAim, spotPos; bool spotHave;
        static readonly int SpotPosId = Shader.PropertyToID("_RfxMenuSpotPos"), SpotDirId = Shader.PropertyToID("_RfxMenuSpotDir"),
            SpotConeId = Shader.PropertyToID("_RfxMenuSpotCone"), SpotRimId = Shader.PropertyToID("_RfxMenuRimColor");

        void UpdateSpot(float dt)
        {
            if (Hero == null) return;
            var target = SpotAim ?? Hero.root.transform.position + (HubStage != null ? HubStage.AimPoint + HubStage.aimOffset : new Vector3(0, 2.5f, 0));
            var flat = target - camEye; flat.y = 0;
            if (flat.sqrMagnitude < 1e-4f) flat = Vector3.forward;
            flat.Normalize();
            var from = Quaternion.Euler(0, SpotSide, 0) * -flat;   // (the camera's side of the subject, turned to its right)
            var pos = target + from * SpotBack + Vector3.up * SpotHeight;
            float k = spotHave ? 1f - Mathf.Exp(-dt * 4f) : 1f; spotHave = true;
            spotAim = Vector3.Lerp(spotAim, target, k); spotPos = Vector3.Lerp(spotPos, pos, k);
            if ((spotAim - target).sqrMagnitude < 1e-6f) spotAim = target;
            if ((spotPos - pos).sqrMagnitude < 1e-6f) spotPos = pos;
            var dir = (spotAim - spotPos).normalized;
            Shader.SetGlobalVector(SpotPosId, new Vector4(spotPos.x, spotPos.y, spotPos.z, SpotGain));
            Shader.SetGlobalVector(SpotDirId, new Vector4(dir.x, dir.y, dir.z, Mathf.Cos(SpotOuter * Mathf.Deg2Rad)));
            Shader.SetGlobalVector(SpotConeId, new Vector4(Mathf.Cos(SpotInner * Mathf.Deg2Rad), 80f, SpotWrap, SpotRim));
            Shader.SetGlobalVector(SpotRimId, SpotRimColor);
            if (sun) sun.transform.rotation = Quaternion.LookRotation(dir);   // (their shadows fall away from the key)
        }

        /// The compound's own lighting into the shader globals again (something that built a FeGarage picture after the world set the garage's).
        public void ReapplyGlobals()
        {
            var c = LevelObject?.Content; if (c == null) return;
            c.ApplyShaderGlobals(); ReflexObjectShading.ApplyLevelGlobals(c);
        }

        void Build()
        {
            var sw = System.Diagnostics.Stopwatch.StartNew();
            try
            {
                if (loading.IsFaulted) throw loading.Exception?.GetBaseException() ?? new Exception("the level was not read");
                var r = loading.Result;
                ReadSeconds = (float)r.seconds;
                // the camera (the level's renderer goes on it)
                var cgo = new GameObject("Menu camera");
                cgo.transform.SetParent(transform, false);
                Camera = cgo.AddComponent<Camera>();
                Camera.clearFlags = CameraClearFlags.SolidColor; Camera.backgroundColor = Color.black;
                Camera.fieldOfView = 32f; Camera.nearClipPlane = 0.5f; Camera.farClipPlane = 8000f;
                Camera.depth = -1f; Camera.allowHDR = true; Camera.allowMSAA = false;
                cgo.tag = "MainCamera";
                var data = UnityEngine.Rendering.Universal.CameraExtensions.GetUniversalAdditionalCameraData(Camera);
                data.renderPostProcessing = false;
                // the sun: the level's (OriginalLevel turns it to the level's direction); the riders' real-time shadows in ENHANCED
                var sgo = new GameObject("Menu sun"); sgo.transform.SetParent(transform, false);
                sun = sgo.AddComponent<Light>(); sun.type = LightType.Directional; sun.intensity = 2.3f; sun.color = new Color(1f, 0.95f, 0.86f); sun.shadows = LightShadows.Soft;   // (as the race scene's Sun)
                RenderSettings.sun = sun;
                // the terrain (built from the height map, as the core's grid gives it in a race)
                var tgo = new GameObject("Menu world terrain (the height map of " + Level + ")");
                tgo.transform.SetParent(transform, false);
                terrain = tgo.AddComponent<OriginalTerrain>();
                // the level: loads in LoadFrom (not in Awake)
                var lgo = new GameObject("Original level " + Level);
                lgo.SetActive(false);
                lgo.transform.SetParent(transform, false);
                LevelObject = lgo.AddComponent<OriginalLevel>();
                LevelObject.loadOnAwake = false; LevelObject.terrain = terrain; LevelObject.gameCamera = Camera; LevelObject.animateGates = false;
                LevelObject.level = Level;
                lgo.SetActive(true);
                LevelObject.LoadFrom(r.content);
                if (LevelObject.Content == null) throw new Exception("the level could not be built");
                if (r.heights != null)
                {
                    float half = (r.samples - 1) * 0.5f * r.spacing;
                    var info = new ReflexTerrainInfo { samples = (uint)r.samples, stride = (uint)r.samples, spacing = r.spacing, min_x = -half, min_z = -half, max_x = half, max_z = half, gain = 2f };
                    terrain.Build(info, r.heights, null);
                }
                HideTutorialProps();
                try { Boards = new OpenStatusBoards(this, LevelObject); } catch (Exception e) { Debug.LogWarning("[MenuWorld] status boards: " + e.Message); Boards = null; }
                if (Environment.GetEnvironmentVariable("REFLEX_MENU3D_DUMP") == "1") 
                {
                    Debug.Log("[MenuWorld] materials:\n" + LevelObject.Content.DescribeMaterials());
                    var sb = new System.Text.StringBuilder("[MenuWorld] renderers:\n");
                    foreach (var mr in LevelObject.SceneryRoot.GetComponentsInChildren<MeshRenderer>(true))
                        sb.Append(mr.name).Append(" pos ").Append(mr.transform.position).Append(" bounds ").Append(mr.bounds.size).Append(" mats ").Append(string.Join(",", mr.sharedMaterials.Select(m => m ? m.name + "/" + (m.shader ? m.shader.name : "-") : "null"))).Append('\n');
                    Debug.Log(sb.ToString());
                }
                // the scene's own camera steps aside (it clears the screen black under the pages)
                foreach (var c in FindObjectsByType<Camera>(FindObjectsSortMode.None))
                    if (c != Camera && c.gameObject.scene == gameObject.scene && c.targetTexture == null && c.gameObject.activeSelf && c.transform.parent == null) { sceneCamera = c.gameObject; sceneCamera.SetActive(false); break; }
                // ... with the front end's only AudioListener (FrontEnd.unity's Main Camera): the menu camera hears for it, else the shell's UI sounds and
                // its UI music (FeSounds / FeUiMusic, 2D) play to no listener and the menus are silent (back with the scene camera: OnDestroy, a failed build)
                if (!FindAnyObjectByType<AudioListener>()) Camera.gameObject.AddComponent<AudioListener>();
                if (LevelObject.Pipeline) { LevelObject.Pipeline.convergeAdaptation = true; convergeFrames = 4; }
                MenuFrameRate();
                OpenMenuWorldProbe.Attach(this);
                groundY = GroundAt(HeroSpot.x, HeroSpot.z);
                var heroPos = new Vector3(HeroSpot.x, groundY, HeroSpot.z);
                Hero = new FeGarage(transform, heroPos, Camera, 0) { besideLeft = true, yawOverride = -58f, groundAt = DrawnGroundAt };
                Hero.CameraSink = SetCamera;
                HubStage = new OpenGarageStage(Hero) { autoTurn = false, lift = 0.0f, fitScale = HubFit };
                Status = State.Ready;
                BuildSeconds = (float)sw.Elapsed.TotalSeconds;
                ReadySeconds = Time.realtimeSinceStartup - startedAt;
                Debug.Log($"[MenuWorld] {Level} ready: read {ReadSeconds:F2} s (worker), build {BuildSeconds:F2} s (main thread), {ReadySeconds:F2} s from the start; hero spot {heroPos}; " +
                          $"walk {(ReflexPushAnim.Load() != null ? "SC_CC_Anim_PushBike2" : "none: " + ReflexPushAnim.Error)}\n{LevelObject.report}");
            }
            catch (Exception e)
            {
                Status = State.Failed; Error = e.Message;
                Debug.LogWarning("[MenuWorld] the 3D menu is off (the pictures stay): " + e);
                if (sceneCamera) sceneCamera.SetActive(true);
                if (Camera) Camera.gameObject.SetActive(false);
            }
            loading = null;
            Changed?.Invoke();
        }

        int convergeFrames, stableFrames;
        /// A frame under this long counts towards the scene being settled (the first frames of the compound and the hero compile their pipelines
        /// and upload their textures: 0.5-2 s each on a cold start).
        public static float StableFrameSeconds = 0.05f;
        /// The compound, the rider and his vehicle are built and the last frames rendered in time: the intro can start without a hitch.
        public bool SceneSettled => Status == State.Ready && Hero?.RiderModel != null && stableFrames >= 10;
        bool pacedMenu;
        /// The client's frame pacing (OPTIONS > VIDEO > FRAME RATE CAP; nothing saved: DISPLAY RATE, one frame per refresh) applied again, logged.
        void MenuFrameRate()
        {
            try
            {
                Reflex.Port.Options.ReflexClientLive.ApplyFrameRate();
                if (OpenMenuWorldProbe.RecordDir != null) { QualitySettings.vSyncCount = 0; Application.targetFrameRate = -1; }   // (a recording runs on its own clock: a hidden window must not stop it)
                Debug.Log($"[MenuWorld] frame pacing: vSync {QualitySettings.vSyncCount}, target {Application.targetFrameRate}, display {Screen.currentResolution.refreshRateRatio.value:0} Hz, cap {Reflex.Port.Options.ReflexSettings.FpsCap}");
            }
            catch (Exception e) { Debug.LogWarning("[MenuWorld] frame pacing: " + e.Message); }
        }
        // frame-time statistics (the 3D menu's cost)
        readonly float[] frameTimes = new float[600]; int frameCount, frameAt;
        void Frames(float dt)
        {
            if (convergeFrames > 0 && --convergeFrames == 0 && LevelObject.Pipeline) LevelObject.Pipeline.convergeAdaptation = false;
            stableFrames = dt < StableFrameSeconds && Hero?.RiderModel != null && pendingHero == null ? stableFrames + 1 : 0;
            frameTimes[frameAt] = dt; frameAt = (frameAt + 1) % frameTimes.Length; frameCount = Math.Min(frameCount + 1, frameTimes.Length);
        }
        /// The frame times of the last (up to) 600 frames: mean, 95th percentile, max (ms).
        public (float mean, float p95, float max, int frames) FrameStats()
        {
            if (frameCount == 0) return (0, 0, 0, 0);
            var t = frameTimes.Take(frameCount).Select(x => x * 1000f).OrderBy(x => x).ToArray();
            return (t.Average(), t[Mathf.Clamp((int)(t.Length * 0.95f), 0, t.Length - 1)], t[t.Length - 1], t.Length);
        }
        public void ResetFrameStats() { frameCount = 0; frameAt = 0; }

        /// The tutorials' props (chevron walls, trajectory arrows, road flares, the position marker) are lesson furniture, not the compound's
        /// look; the menu hides them (the chevrons are drawn as opaque walls by the port, QUEUE "WP-T" note).
        void HideTutorialProps()
        {
            int hidden = OpenTutorialProps.Hide(LevelObject.Content);
            if (hidden > 0) Debug.Log("[MenuWorld] tutorial props hidden: " + hidden);
        }

        /// The terrain's height at x / z (the scenery's when no terrain was built).
        public float GroundAt(float x, float z)
        {
            var t = terrain ? terrain.UnityTerrain : null;
            if (t) return t.SampleHeight(new Vector3(x, 0, z)) + t.transform.position.y;
            return 0f;
        }

        // ---- the ground as drawn (w-fb-models): the terrain's vertices are raised by RT_Bump's displacement (RfxRTBumpDisplace: the bump
        // models' blue channel, sampled at the mip whose texel spans two height-map cells), so the compound's floor stands up to a foot above the
        // height map where its bump models lie. The hero (FeGarage.groundAt) stands on GroundAt + that displacement: the bump models drawn
        // top-down once around the hero spot (ReflexRTBump.DisplacementTopDown), box-averaged over +-1 cell at each terrain vertex like the mip,
        // bilinear between the vertices.
        /// The square around the hero spot whose displacement is read (feet) and its texels.
        public static float BumpSpan = 48f; public static int BumpTexels = 768;
        float[] bumpVertex; int bumpN; float bumpX0, bumpZ0, bumpCell; bool bumpRead; int bumpWait;
        /// The displacement read around the hero spot (diagnostics): its largest value in feet, -1 before the read / without one.
        public float BumpMax { get; private set; } = -1f;

        /// The ground as drawn at x / z: the terrain plus RT_Bump's displacement there (the plain terrain before the read and outside its square).
        public float DrawnGroundAt(float x, float z) => GroundAt(x, z) + BumpAt(x, z);

        float BumpAt(float x, float z)
        {
            if (bumpVertex == null) return 0f;
            float fx = (x - bumpX0) / bumpCell, fz = (z - bumpZ0) / bumpCell;
            int i = Mathf.FloorToInt(fx), j = Mathf.FloorToInt(fz);
            if (i < 0 || j < 0 || i + 1 >= bumpN || j + 1 >= bumpN) return 0f;
            float tx = fx - i, tz = fz - j;
            float a = Mathf.Lerp(bumpVertex[j * bumpN + i], bumpVertex[j * bumpN + i + 1], tx), b = Mathf.Lerp(bumpVertex[(j + 1) * bumpN + i], bumpVertex[(j + 1) * bumpN + i + 1], tx);
            return Mathf.Lerp(a, b, tz);
        }

        /// Reads the displacement around the hero spot (once, after the world has drawn a few frames) and puts the hero on the drawn ground.
        void ReadBump()
        {
            if (bumpRead || ++bumpWait < 3) return;
            bumpRead = true;
            var t = terrain ? terrain.UnityTerrain : null; var rb = LevelObject ? LevelObject.RTBump : null;
            if (!t || !rb || rb.draws == null || rb.draws.Count == 0) { BumpMax = 0f; return; }
            using var perf = Reflex.Port.Platform.ReflexMenuPerf.Measure("ground displacement read");
            float span = BumpSpan; int size = BumpTexels; float texel = span / size;
            float x0 = HeroSpot.x - span * 0.5f, z0 = HeroSpot.z - span * 0.5f;
            var disp = Reflex.Port.Rendering.ReflexRTBump.DisplacementTopDown(rb.draws, x0, z0, span, size);
            if (disp == null) { BumpMax = 0f; return; }
            // a summed-area table for the box averages
            var sat = new double[(size + 1) * (size + 1)];
            for (int j = 0; j < size; j++) { double row = 0; for (int i = 0; i < size; i++) { row += disp[j * size + i]; sat[(j + 1) * (size + 1) + i + 1] = sat[j * (size + 1) + i + 1] + row; } }
            double Box(int i0, int j0, int i1, int j1)
            {
                i0 = Mathf.Clamp(i0, 0, size); i1 = Mathf.Clamp(i1, 0, size); j0 = Mathf.Clamp(j0, 0, size); j1 = Mathf.Clamp(j1, 0, size);
                int n = (i1 - i0) * (j1 - j0); if (n <= 0) return 0;
                return (sat[j1 * (size + 1) + i1] - sat[j0 * (size + 1) + i1] - sat[j1 * (size + 1) + i0] + sat[j0 * (size + 1) + i0]) / n;
            }
            // the terrain's vertices inside the square (Unity terrain: its position + k cells)
            var tp = t.transform.position; float cell = t.terrainData.heightmapScale.x;
            int k0 = Mathf.CeilToInt((x0 - tp.x) / cell), l0 = Mathf.CeilToInt((z0 - tp.z) / cell);
            bumpCell = cell; bumpX0 = tp.x + k0 * cell; bumpZ0 = tp.z + l0 * cell;
            bumpN = Mathf.Max(0, Mathf.FloorToInt((span - (bumpX0 - x0)) / cell)); bumpN = Mathf.Min(bumpN, Mathf.FloorToInt((span - (bumpZ0 - z0)) / cell));
            if (bumpN < 2) { BumpMax = 0f; return; }
            int r = Mathf.Max(1, Mathf.RoundToInt(cell / texel));
            var v = new float[bumpN * bumpN]; float max = 0f;
            for (int j = 0; j < bumpN; j++)
                for (int i = 0; i < bumpN; i++)
                {
                    int ci = Mathf.FloorToInt((bumpX0 + i * cell - x0) / texel), cj = Mathf.FloorToInt((bumpZ0 + j * cell - z0) / texel);
                    float d = (float)Box(ci - r, cj - r, ci + r, cj + r);
                    v[j * bumpN + i] = d; max = Mathf.Max(max, d);
                }
            bumpVertex = v; BumpMax = max;
            Debug.Log($"[MenuWorld] ground: RT_Bump displacement around the hero spot read ({size}x{size} over {span} ft, {rb.draws.Count} bump draws): up to {max:F3} ft; " +
                      $"at the spot {BumpAt(HeroSpot.x, HeroSpot.z):F3} ft");
            if (Hero != null && walk == null) { Hero.Reground(); Debug.Log($"[MenuWorld] hero on the drawn ground: vehicle {Hero.BikeGroundLift:+0.000;-0.000} ft, rider {Hero.RiderGroundLift:+0.000;-0.000} ft"); }
            else regroundAfterWalk = true;
        }
        bool regroundAfterWalk;

        // ------------------------------------------------------------------------------------------------ the camera

        Vector3 camEye, camLook, wantEye, wantLook; bool camHave, camSnap; float easeLeft;
        void SetCamera(Vector3 eye, Vector3 look)
        {
            wantEye = eye; wantLook = look;
            if (!camHave) { camHave = true; camSnap = true; }
        }
        /// The next camera moves are eased slowly (a change of shot: hub <-> garage, a tab).
        public void EaseCamera(float seconds = 1.1f) { easeLeft = easeTotal = Mathf.Max(0.05f, seconds); easeEye = camEye; easeLook = camLook; }
        float easeTotal = 1f; Vector3 easeEye, easeLook;
        /// The camera jumps to where it is asked (tests).
        public void SnapCamera() { camSnap = true; WalkOffset = walkTarget; }

        // ------------------------------------------------------------------------------------------------ modes

        /// The hub (and the pages over it): the hero framed in the part of the screen right of the menu (view: OpenGarageStage.viewCenter /
        /// viewHeight / screenAspect / aspect).
        public void HubMode(Vector2 viewCenter, float viewHeight, float screenAspect, float viewAspect, bool backdrop)
        {
            if (Status != State.Ready) return;
            var was = Current;
            Current = backdrop ? Mode.Backdrop : Mode.Hub;
            SpotAim = null;
            EndPreview();
            hubView = (viewCenter, viewHeight, screenAspect, viewAspect);
            ApplyHubView();
            if (was == Mode.Garage) { EaseCamera(); Hero.CameraSink = SetCamera; }
            if (walk == null) { HubStage.holdAim = false; HubStage.SetAngles(HubYaw, HubPitch); HubStage.Frame(OpenGarageStage.Shot.Overview, false, was != Mode.Garage && !camHave); }
        }
        (Vector2 center, float height, float screenAspect, float aspect) hubView = (Vector2.zero, 1f, 16f / 9f, 1f);
        bool intro;
        /// The intro's shot (the first hub entry): the rider and the vehicle framed close in the right-centre third, the camera following the walk
        /// with a little look-ahead; off: the hub's composition, eased out to (IntroEaseSeconds).
        public bool Intro
        {
            get => intro;
            set
            {
                if (intro == value || Status != State.Ready) { intro = value; return; }
                intro = value;
                ApplyHubView();
                if (!intro) EaseCamera(IntroEaseSeconds);
                if (HubStage != null && intro) { HubStage.holdAim = false; HubStage.Frame(OpenGarageStage.Shot.Overview, false, true); SnapCamera(); }   // (the intro's shot from its first frame)
                else if (walk == null && HubStage != null) HubStage.Frame(OpenGarageStage.Shot.Overview, false);
            }
        }
        public static float IntroFit = 0.8f, IntroEaseSeconds = 0.75f, IntroLookAhead = 0.8f, MachineFitScale = 1.5f;
        public static Vector2 IntroCenter = new Vector2(0.36f, 0.04f);
        void ApplyHubView()
        {
            if (HubStage == null) return;
            float big = Hero?.VehicleKind == "machine" ? MachineFitScale : 1f;   // (a truck, a buggy, a UTV and the rider beside it need a wider shot)
            if (intro) { HubStage.viewCenter = IntroCenter; HubStage.viewHeight = 0.86f; HubStage.screenAspect = hubView.screenAspect; HubStage.SetAspect(0.9f); HubStage.fitScale = IntroFit * big; }
            else { HubStage.viewCenter = hubView.center; HubStage.viewHeight = hubView.height; HubStage.screenAspect = hubView.screenAspect; HubStage.SetAspect(hubView.aspect); HubStage.fitScale = HubFit * big; }
        }
        /// The hub's camera angles about the hero (OpenGarageStage: yaw 0 = the vehicle's left, 90 = its front).
        public static float HubYaw = 62f, HubPitch = 5f, HubFit = 0.56f;

        /// The garage drives the camera through its own stages (OpenShell.Garage.cs).
        public void GarageMode()
        {
            if (Status != State.Ready) return;
            if (Current != Mode.Garage) EaseCamera();
            Current = Mode.Garage;
        }

        // ------------------------------------------------------------------------------------------------ the hero

        /// The rider and the vehicle at the hero spot (keyed: the same key builds nothing). walk: he walks the vehicle in (not with REDUCE
        /// MOTION, not when the walk clips are missing).
        public void ShowHero(BikeSpec bike, RiderSpec rider, string key, bool walkIn)
        {
            if (Status != State.Ready || Hero == null) return;
            using var perf = Reflex.Port.Platform.ReflexMenuPerf.Measure("hero shown");
            int serial = Hero.BuildSerial;
            previewWanted = null; previewSpec = null; PreviewKey = null;
            var sw = System.Diagnostics.Stopwatch.StartNew();
            try { Hero.ShowSpecs(bike, rider, true); HeroKey = key; }
            catch (Exception e)
            {
                Debug.LogWarning("[MenuWorld] hero with the high-resolution models: " + e.Message);
                Hero.Unload(); Hero.highRes = false;
                try { Hero.ShowSpecs(bike, rider, true); HeroKey = key; }
                catch (Exception e2) { Debug.LogWarning("[MenuWorld] hero: " + e2.Message); HeroKey = null; }
                finally { Hero.highRes = true; }
            }
            Hero.SetRiderVisible(true);
            bool rebuilt = Hero.BuildSerial != serial;
            if (rebuilt) { ApplyHubView(); LastHeroBuildMs = (float)sw.Elapsed.TotalMilliseconds; Debug.Log($"[MenuWorld] hero {key}: built in {LastHeroBuildMs:F0} ms (main thread){(walkIn ? ", walks in" : "")}"); CancelWalk(); }
            if (Current != Mode.Garage && walk == null && rebuilt) HubStage.Refit();
            if (walkIn) StartWalk();
        }

        /// The hero's specs built on a worker thread first (the HR twins' parse), then shown (and walked in) on the main thread.
        public void ShowHeroAsync(BikeSpec bike, RiderSpec rider, string key, bool walkIn)
        {
            if (Status != State.Ready) return;
            if (key == HeroKey && pendingHero == null) { ShowHero(bike, rider, key, walkIn); return; }
            if (key == pendingHero) { pendingWalk |= walkIn; return; }
            pendingHero = key; pendingWalk = walkIn;
            Task.Run(() => ReflexBikeModel.Prefetch(bike, true)).ContinueWith(_ =>
            {
                lock (main) main.Add(() =>
                {
                    if (pendingHero != key) return;   // (another setup was asked for meanwhile)
                    bool w = pendingWalk; pendingHero = null; pendingWalk = false;
                    ShowHero(bike, rider, key, w);
                });
            });
        }
        string pendingHero; bool pendingWalk;
        /// A pick's walk is on its way (its models are being read).
        public bool PickPending => pendingHero != null && pendingWalk;
        /// The last hero build on the main thread (ms).
        public float LastHeroBuildMs { get; private set; }
        readonly List<Action> main = new List<Action>();

        // ------------------------------------------------------------------------------------------------ the walk

        sealed class Walk
        {
            public ReflexPushAnim anim; public ReflexPushAnim.Binding bind; public string[] names;
            public float t, blend; public Matrix4x4 clipToGarage, clipToWorld; public float dy, radiusF, radiusR, z0;
            public float[] pose, idle, mixed;
            public int serial; public float bikeDy;
            // ATVs (no original push of an ATV exists: the bike's walk, the walker clear of the wider body, his nearer hand on the left grip)
            public Vector3 riderShift; public Matrix4x4? grip;
            public List<(Transform part, Vector3 pivot)> wheels; public float wheelRadius = 1f;
            // trucks, buggies and UTVs: driven in (no original exit animation exists: the driver is cut to the idle beside it at the stop)
            public bool drive; public ReflexRiderAnim steer; public Matrix4x4 seat; public Vector3[] path; public float pathLength, driveSeconds;
            public float[] arc;
            public float Length => drive ? driveSeconds + DriveHoldSeconds : anim.Length;
        }
        /// Drive-in: the seconds on the path, the seconds the driver stays seated at the stop before he is beside it; the path (garage space,
        /// relative to the rest spot: from behind on the left, a curve, straight into the spot).
        public static float DriveSeconds = 4.2f, DriveHoldSeconds = 0.45f;
        public static Vector3[] DrivePath = { new Vector3(-18f, 0, -34f), new Vector3(-18f, 0, -16f), new Vector3(0, 0, -16f), Vector3.zero };
        Walk walk;
        double walkMs; int walkFrames;
        Vector3 walkTarget;
        /// Where the camera's aim is moved by the walk now (garage space; the stages add it).
        public Vector3 WalkOffset { get; private set; }
        /// How fast the camera catches up with the walk (1 / s).
        public static float TrackRate = 2.2f;
        /// The bike's lowering onto its stand after the clip's last frame (s).
        public static float BlendSeconds = 0.9f;
        /// The rider's hand-over from the clip's last frame into the idle stance (s): two steps (foot-planted, ReflexPushAnim.BlendPlanted).
        public static float SettleSeconds = 1.2f;
        /// The longest step of the walk's clock in one frame (s).
        public static float MaxWalkStep = 1f / 30f;
        /// When the last walk started (MenuTime.Now; -1: none yet).
        public float WalkStartedAt { get; private set; } = -1f;
        /// How much of the walk the camera follows (1: all of it; less: it turns towards them more than it travels).
        public static float TrackShare = 0.85f;

        /// The rider walks the vehicle into the spot (from 19 ft behind it) and settles into the idle.
        public void StartWalk()
        {
            CancelWalk();
            if (Hero == null || Hero.RiderModel == null || OuMotion.Reduced || Hero.VehicleKind == null) return;
            var rm = Hero.RiderModel.rider;
            if (!rm || rm.bones == null || rm.boneParents == null || rm.bindWorldMatrices == null) return;
            if (Hero.VehicleKind == "machine") { StartDrive(rm); return; }
            var anim = ReflexPushAnim.Load();
            if (anim == null) return;
            try
            {
                var names = rm.bones.Take(OriginalRider.OriginalBoneCount).Select(b => b ? b.name : "").ToArray();
                var w = new Walk { anim = anim, names = names, pose = new float[OriginalRider.OriginalBoneCount * 16], idle = new float[OriginalRider.OriginalBoneCount * 16], mixed = new float[OriginalRider.OriginalBoneCount * 16] };
                w.bind = anim.Bind(names, rm.boneParents, rm.bindWorldMatrices, ReflexRiderAnim.Load("RiderUIAnimations", "Base", FeGarage.RiderClip));
                // the clip's space onto the garage's: the vehicle's frame node at the clip's end = its rest placement
                var restFrame = Matrix4x4.Translate(Hero.BikeRestOrigin);
                var end = anim.Frame(anim.Length);
                w.clipToGarage = restFrame * end.inverse;
                if (Hero.VehicleKind == "atv")
                {   // the walker beside the ATV's wider body (its left extreme against the bike's bar end), the grip his hand reaches for
                    w.grip = Hero.HierarchyNode("NDE_LFT_Hand");
                    float leftmost = Hero.BikeBounds(out var vb) ? vb.min.x - Hero.BikeRestOrigin.x : -1.2f;
                    w.riderShift = new Vector3(Mathf.Min(0f, leftmost + 1.25f), 0, 0);
                }
                w.wheels = Hero.WheelParts();
                if (w.wheels.Count > 0) w.wheelRadius = Mathf.Max(0.5f, Hero.BikeRestOrigin.y + w.wheels[0].pivot.y);
                w.clipToWorld = Matrix4x4.Translate(Hero.root.transform.position) * w.clipToGarage * Matrix4x4.Translate(w.riderShift);
                // the walker's feet on the floor: his lowest foot at the clip's end against the idle stance's
                anim.Pose(w.bind, anim.Length, w.clipToWorld, 0f, w.pose);
                anim.Pose(w.bind, anim.Length - 1f / 60f, w.clipToWorld, 0f, w.mixed);
                ReflexPushAnim.HandOver(w.bind, w.pose, w.mixed, 1f / 60f);   // (the feet leave the clip at its own speed: the settle's steps start with it)
                Hero.IdleWorld(w.idle);
                w.dy = ReflexPushAnim.LowestFoot(w.bind, names, w.idle) - ReflexPushAnim.LowestFoot(w.bind, names, w.pose);
                w.radiusF = Hero.WheelRadius(true); w.radiusR = Hero.WheelRadius(false);
                w.z0 = anim.Frame(0f).GetColumn(3).z;
                Hero.HoldIdle = true;
                Hero.ShowStand(false);
                w.bikeDy = Hero.root.transform.position.y + Hero.BikeGroundLift - Hero.WheelsLowestY();   // (the HR stand held the wheels off the floor: they roll on it, at the rest's ground height)
                w.serial = Hero.BuildSerial;
                walk = w;
                // the camera holds the shot of the rest placement while they walk in
                if (Current != Mode.Garage) { HubStage.Frame(OpenGarageStage.Shot.Overview, false); HubStage.holdAim = true; }
                walkMs = 0; walkFrames = 0; WalkStartedAt = MenuTime.Now;
                ApplyWalk();
                WalkOffset = walkTarget; SnapCamera();   // (the shot cuts to them where they start)
                Debug.Log($"[MenuWorld] walk {anim.name}: {anim.Length:F2} s, {w.bind.mapped} bones retargeted, feet {w.dy:+0.00;-0.00} ft");
            }
            catch (Exception e) { Debug.LogWarning("[MenuWorld] walk: " + e.Message); CancelWalk(); }
        }

        /// The walk's end state at once (a skip): the vehicle at rest on its stand, the rider in the idle, the camera on the composition.
        public void FinishWalk()
        {
            CancelWalk();
            WalkOffset = Vector3.zero;
            if (HubStage != null) { HubStage.aimOffset = Vector3.zero; HubStage.Refit(); }
            SnapCamera();
        }

        /// ATV push: the walker's hand nearer to the left grip on it (two-bone IK on the posed arm), by weight.
        void ReachGrip(Walk w, float weight)
        {
            if (w.grip == null || weight <= 0.01f) return;
            var ch = Hero.BikeChassis; var bones = Hero.RiderModel?.rider?.bones;
            if (ch == null || bones == null || bones.Length < 42) return;
            Vector3 grip = ch.TransformPoint(w.grip.Value.GetColumn(3));
            bool left = Vector3.Distance(bones[41].position, grip) <= Vector3.Distance(bones[12].position, grip);
            int a = left ? 39 : 10, b = left ? 40 : 11, c = left ? 41 : 12;
            if (!bones[a] || !bones[b] || !bones[c]) return;
            RiderLimbRetarget.TwoBone(bones[a], bones[b], bones[c], Vector3.Lerp(bones[c].position, grip, weight));
        }

        public void CancelWalk()
        {
            if (walk == null) return;
            var was = walk;
            walk = null; walkTarget = Vector3.zero;
            SpinWheels(was, 0);
            if (Hero != null) { Hero.SetBikeMotion(Matrix4x4.identity, 0, 0); Hero.ShowStand(true); Hero.ResumeIdle(); }
            if (HubStage != null) HubStage.holdAim = false;
        }

        void TickWalk(float dt)
        {
            lock (main) { if (main.Count > 0) { var a = main.ToArray(); main.Clear(); foreach (var x in a) x(); } }
            if (walk == null) return;
            if (Hero == null || Hero.RiderModel == null) { walk = null; return; }
            if (Hero.BuildSerial != walk.serial) { CancelWalk(); return; }   // (the garage built other models meanwhile)
            walk.t += Mathf.Min(dt, MaxWalkStep);   // (a hitch slows the walk down for a frame instead of jumping it ahead)
            var sw = System.Diagnostics.Stopwatch.StartNew();
            ApplyWalk();
            walkMs += sw.Elapsed.TotalMilliseconds; walkFrames++;
            if (walk == null) Debug.Log($"[MenuWorld] walk done: {walkFrames} frames, {walkMs / Math.Max(1, walkFrames):F2} ms per frame (main thread)");
        }

        /// Tests: the walk at a time (seconds; past the clip's length: the settling blend).
        public void SetWalkTime(float t) { if (walk == null) return; walk.t = t; ApplyWalk(); }

        void StartDrive(OriginalRider rm)
        {
            try
            {
                var names = rm.bones.Take(OriginalRider.OriginalBoneCount).Select(b => b ? b.name : "").ToArray();
                string clip = DriverClip(Hero.ShownBike);
                var w = new Walk
                {
                    drive = true, names = names, steer = clip != null ? ReflexRiderAnim.Load("Animations", "Animations", clip) : null,
                    seat = Hero.HierarchyNode("NDE_RiderLocator", "NDE_SeatNode") ?? Matrix4x4.identity,
                    pose = new float[OriginalRider.OriginalBoneCount * 16], idle = new float[OriginalRider.OriginalBoneCount * 16], mixed = new float[OriginalRider.OriginalBoneCount * 16],
                    path = DrivePath, driveSeconds = DriveSeconds,
                };
                w.wheels = Hero.WheelParts();
                w.wheelRadius = w.wheels.Count > 0 ? Mathf.Max(0.5f, Hero.BikeRestOrigin.y + w.wheels[0].pivot.y) : 1.5f;
                // the path's arc length (64 steps): the vehicle moves along it at an even pace that eases to the stop
                w.arc = new float[65]; var prev = Bezier(w.path, 0f);
                for (int i = 1; i <= 64; i++) { var p = Bezier(w.path, i / 64f); w.arc[i] = w.arc[i - 1] + Vector3.Distance(prev, p); prev = p; }
                w.pathLength = w.arc[64];
                Hero.HoldIdle = true;
                w.serial = Hero.BuildSerial;
                walk = w;
                if (Current != Mode.Garage) { HubStage.Frame(OpenGarageStage.Shot.Overview, false); HubStage.holdAim = true; }
                walkMs = 0; walkFrames = 0; WalkStartedAt = MenuTime.Now;
                ApplyWalk();
                WalkOffset = walkTarget; SnapCamera();
                Debug.Log($"[MenuWorld] drive-in: {Hero.ShownBike?.name} {w.pathLength:F1} ft in {w.driveSeconds:F1} s, driver {(w.steer != null ? clip : "bind pose (no steering clip)")}, seat {(Hero.HierarchyNode("NDE_RiderLocator", "NDE_SeatNode") != null ? "the hierarchy's" : "none")}, {w.wheels.Count} wheel meshes");
            }
            catch (Exception e) { Debug.LogWarning("[MenuWorld] drive-in: " + e.Message); CancelWalk(); }
        }

        /// The driver's steering clip of a machine class (Animations: Rider_<class>_SteerLeftRight, the time = the steer from full left to full right).
        static string DriverClip(BikeSpec s)
        {
            string id = ((s?.chassisDb ?? "") + "/" + (s?.chassisPkg ?? "")).ToUpperInvariant();
            if (id.Contains("UTV")) return "Rider_UTV_SteerLeftRight";
            if (id.Contains("PRO2")) return "Rider_Pro2_SteerLeftRight";
            if (id.Contains("PROLITE")) return "Rider_ProLight_SteerLeftRight";
            if (id.Contains("SB")) return "Rider_SB_SteerLeftRight";
            if (id.Contains("PRO4")) return "Rider_Pro2_SteerLeftRight";   // (no Pro 4 clip: the Pro 2's, the same seat)
            return "Rider_UTV_SteerLeftRight";
        }

        static Vector3 Bezier(Vector3[] p, float u)
        {
            float v = 1f - u;
            return v * v * v * p[0] + 3f * v * v * u * p[1] + 3f * v * u * u * p[2] + u * u * u * p[3];
        }
        static float ArcToU(float[] arc, float s)
        {
            int n = arc.Length - 1;
            if (s <= 0) return 0; if (s >= arc[n]) return 1;
            int i = 1; while (i < n && arc[i] < s) i++;
            float span = arc[i] - arc[i - 1];
            return (i - 1 + (span > 1e-6f ? (s - arc[i - 1]) / span : 0f)) / n;
        }

        readonly float[] seatRoot = new float[16];
        void ApplyDrive()
        {
            var w = walk;
            float u = Mathf.Clamp01(w.t / w.driveSeconds);
            float eased = 1f - Mathf.Pow(1f - u, 2.2f);   // (rolls in, slows to the stop)
            float s = eased * w.pathLength;
            float pu = ArcToU(w.arc, s);
            var pos = Bezier(w.path, pu);
            var tan = Bezier(w.path, Mathf.Min(1f, pu + 0.01f)) - Bezier(w.path, Mathf.Max(0f, pu - 0.01f)); tan.y = 0;
            float yaw = tan.sqrMagnitude > 1e-8f ? Mathf.Atan2(tan.x, tan.z) * Mathf.Rad2Deg : 0f;
            if (u >= 1f) { pos = Vector3.zero; yaw = 0f; }
            var o = Hero.BikeRestOrigin;
            var motion = Matrix4x4.Translate(o + pos) * Matrix4x4.Rotate(Quaternion.Euler(0, yaw, 0)) * Matrix4x4.Translate(-o);
            float rolled = s - w.pathLength;   // feet before the stop (negative)
            Hero.SetBikeMotion(motion, 0, 0);
            SpinWheels(w, rolled);
            walkTarget = intro && u < 1f ? pos + Quaternion.Euler(0, yaw, 0) * Vector3.forward * IntroLookAhead : pos * TrackShare;
            if (w.t >= w.Length)
            {   // the driver is beside it (no exit clip in the data: a cut to the idle stance)
                walk = null; walkTarget = Vector3.zero; Hero.SetBikeMotion(Matrix4x4.identity, 0, 0); SpinWheels(w, 0); Hero.ResumeIdle();
                if (HubStage != null) HubStage.holdAim = false;
                return;
            }
            // the driver in his seat (the class's steering clip, steered by the path's turn)
            float yaw2 = 0f;
            { var t2 = Bezier(w.path, Mathf.Min(1f, pu + 0.06f)) - Bezier(w.path, Mathf.Min(1f, pu + 0.04f)); t2.y = 0; if (t2.sqrMagnitude > 1e-8f) yaw2 = Mathf.Atan2(t2.x, t2.z) * Mathf.Rad2Deg; }
            float steer = u >= 1f ? 0f : Mathf.Clamp(Mathf.DeltaAngle(yaw, yaw2) / 6f, -1f, 1f);
            var ch = Hero.BikeChassis;
            if (ch == null || w.steer == null) return;
            var root = ch.localToWorldMatrix * w.seat;
            for (int i = 0; i < 3; i++) for (int k = 0; k < 4; k++) seatRoot[i * 4 + k] = k == 3 ? 0 : root[k, i];
            seatRoot[12] = root[0, 3]; seatRoot[13] = root[1, 3]; seatRoot[14] = root[2, 3]; seatRoot[15] = 1;
            w.steer.World(Mathf.Clamp((0.5f + 0.5f * steer) * w.steer.length, 0f, w.steer.length * 0.999f), w.names, Hero.RiderModel.rider.boneParents, seatRoot, w.pose);
            Hero.ApplyRiderWorld(w.pose);
        }

        static void SpinWheels(Walk w, float rolledFeet)
        {
            if (w.wheels == null) return;
            var r = Quaternion.AngleAxis(rolledFeet / w.wheelRadius * Mathf.Rad2Deg, Vector3.right);
            foreach (var (part, pivot) in w.wheels) if (part) { part.localRotation = r; part.localPosition = pivot - r * pivot; }
        }

        void ApplyWalk()
        {
            if (walk.drive) { ApplyDrive(); return; }
            var w = walk; var a = w.anim;
            float tc = Mathf.Min(w.t, a.Length);
            var frame = a.Frame(tc);
            var motion = w.clipToGarage * frame * Matrix4x4.Translate(Hero.BikeRestOrigin).inverse;
            // the tyres on the floor while rolling; back up onto the stand as the rider settles
            float settle = w.t <= a.Length ? 0f : Mathf.Clamp01((w.t - a.Length) / BlendSeconds);
            motion = Matrix4x4.Translate(new Vector3(0, w.bikeDy * (1f - settle), 0)) * motion;
            if (settle > 0f) Hero.ShowStand(true);
            walkTarget = intro && w.t < a.Length ? (Vector3)motion.GetColumn(3) + Vector3.forward * IntroLookAhead * (1f - Mathf.Clamp01(w.t / a.Length) * 0.5f)
                                                 : (Vector3)motion.GetColumn(3) * TrackShare;
            float rolled = frame.GetColumn(3).z - a.Frame(a.Length).GetColumn(3).z;   // feet before the rest spot (negative)
            Hero.SetBikeMotion(motion, rolled / Mathf.Max(0.3f, w.radiusF) * Mathf.Rad2Deg, rolled / Mathf.Max(0.3f, w.radiusR) * Mathf.Rad2Deg);
            SpinWheels(w, rolled);
            a.Pose(w.bind, tc, w.clipToWorld, w.dy, w.pose);
            if (w.t <= a.Length) { Hero.ApplyRiderWorld(w.pose); ReachGrip(w, 1f); return; }
            // he settles into the idle stance: two steps, the feet planted between them (no skating over the floor)
            float u = Mathf.Clamp01((w.t - a.Length) / SettleSeconds); float k = u * u * (3 - 2 * u);
            Hero.IdleWorld(w.idle);
            if (u >= 1f) { walk = null; walkTarget = Vector3.zero; Hero.SetBikeMotion(Matrix4x4.identity, 0, 0); SpinWheels(w, 0); Hero.ShowStand(true); Hero.ResumeIdle(); if (HubStage != null) HubStage.holdAim = false; return; }
            ReflexPushAnim.BlendPlanted(w.bind, w.pose, w.idle, u, SettleSeconds, w.mixed);
            Hero.ApplyRiderWorld(w.mixed);
            ReachGrip(w, 1f - k);
        }

        // ------------------------------------------------------------------------------------------------ the preview

        Task previewPrefetch; string previewWanted; BikeSpec previewSpec; float previewAsked;
        /// The vehicle browsed to in the garage's VEHICLE list (null: the equipped one is shown).
        public string PreviewKey { get; private set; }

        /// The garage's VEHICLE preview: the browsed vehicle stands in the hero spot in place of the equipped one (the rider steps out of the
        /// picture meanwhile), after its models are read on a worker thread; the last one asked wins (a fast scroll builds only where it stops).
        /// Nothing walks until it is picked.
        public void ShowPreview(BikeSpec spec, string key)
        {
            if (Status != State.Ready || spec == null || Hero == null) return;
            if (key == PreviewKey && previewWanted == null) return;
            if (walk != null) CancelWalk();
            previewWanted = key; previewSpec = spec; previewAsked = MenuTime.Now;
            var s = spec;
            previewPrefetch = Task.Run(() => ReflexBikeModel.Prefetch(s, true));
        }

        /// Browsing ended: the rider is back (the caller shows the equipped vehicle again: ShowHero / ShowHeroAsync / the garage's own GarageShow).
        public void EndPreview(bool showRider = true)
        {
            previewWanted = null; previewSpec = null;
            if (PreviewKey != null) { PreviewKey = null; HeroKey = null; }
            if (showRider) Hero?.SetRiderVisible(true);   // (a pick leaves him out until his walk starts: ShowHero)
        }

        void TickPreview()
        {
            if (previewWanted == null || previewPrefetch == null || !previewPrefetch.IsCompleted || MenuTime.Now - previewAsked < PreviewSettle) return;
            var key = previewWanted; var spec = previewSpec; previewWanted = null;
            using var perf = Reflex.Port.Platform.ReflexMenuPerf.Measure("vehicle preview built");
            var sw = System.Diagnostics.Stopwatch.StartNew();
            try { Hero.ShowSpecs(spec, Hero.ShownRider, Hero.ShownRider != null); PreviewKey = key; HeroKey = null; }
            catch (Exception e)
            {
                Debug.LogWarning("[MenuWorld] preview with the high-resolution models: " + e.Message);
                try { Hero.Unload(); Hero.highRes = false; Hero.ShowSpecs(spec, null, false); PreviewKey = key; HeroKey = null; }
                catch (Exception e2) { Debug.LogWarning("[MenuWorld] preview: " + e2.Message); PreviewKey = null; }
                finally { Hero.highRes = true; }
            }
            Hero.SetRiderVisible(false);
            LastPreviewBuildMs = (float)sw.Elapsed.TotalMilliseconds;
        }
        /// How long the focus must rest on a vehicle before its preview is built (a scroll passes by without building).
        public static float PreviewSettle = 0.15f;
        /// The last preview build on the main thread (ms; the models were read before).
        public float LastPreviewBuildMs { get; private set; }
        public bool PreviewBusy => previewWanted != null;
    }
}

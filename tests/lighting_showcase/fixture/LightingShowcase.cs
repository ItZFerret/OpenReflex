// TEST ONLY (tests/lighting_showcase): loads one event straight from the open front end, holds the game camera on chosen views and takes
// matched ORIGINAL / ENHANCED screenshots (OPTIONS > VIDEO > LIGHTING, ReflexEnhancedLighting.Override) plus frame-time numbers for both
// modes. It presses nothing and writes no game state; the save is the run's scratch directory. Plan (REFLEX_LIGHTING_PLAN, JSON):
//   { "event": "National/NAT_Stillwater", "prefix": "01_Stillwater", "settle": 8, "adapt": 3, "perf": 3,   (a custom track of Mods/Tracks: "FreeRide/Beta_Track_Slot_1/@<its folder>")
//     "views": [ { "name": "gate", "cam": "game" },
//                { "name": "vista", "cam": "pose", "anchor": "bike", "frame": "sun", "offset": [0, 6, -30], "look": "anchor", "lookY": 1, "fov": 60 },
//                { "name": "world", "cam": "pose", "at": [x, y, z], "frame": "world", "yaw": 90, "pitch": -5, "fov": 60 } ] }
// "ai": opponents (needed by "frame": "opp" views: "opp" k = anchored to opponent k, -2 = one shot per opponent).
// "modes": original / enhanced, the diagnostics ao / contact / pos / nrm (ReflexEnhancedLighting.DebugView 1..4) and suspects switched off
// for that shot: "enhanced-noshadow-nocontact ..." with noshadow, nocontact, noao, noocc, notrees, noscenery, nodynamic, nostatic,
// noterrainrecv, pixerrN (the terrain's pixel error) (w-darklines).
// ANTI-ALIASING (w-rmaa): taa / aaoff / msaa2 / msaa4 (ReflexAntiAliasing.ModeOverride), sharpN (SHARPENING N %), upq / upb (UPSCALING QUALITY /
// BALANCED), ss2 (the reference: render scale 200 %, no anti-aliasing), e.g. "enhanced-taa-sharp50". Without one: the saved option (TAA).
// cam "path" (the temporal test): the camera starts at `at` (world, yaw / pitch), moves `step` metres per frame along the horizontal direction
// of `yaw` and turns `yawStep` degrees per frame for `frames` frames with a riding bob (pitch +- bobPitch degrees, height +- bobHeight metres,
// period bobPeriod frames); every frame is saved as <prefix>_<view>_<mode>_fNN.png (tests/lighting_showcase/temporal.py measures them).
// w-camfix (the driving cameras): plan "camera" selects that camera entry first (ReflexCameraPresets ids: "custom:1" = the starter CLOSE CHASE,
// turned ON; "game:1" GAME PLAY ...); plan "camlog" N logs every frame for N unpaused seconds to <prefix>_camlog.csv (the race state, the
// custom camera's blend weight, the game camera's / the held distance from the bike, the drawn camera's distance, its near plane and fov); a
// view's "state" waits for that race state (0 Pre, 1 Countdown, 2 InProgress) and then "after" unpaused seconds instead of "t"; a pose
// view's "near" sets the camera's near plane for that shot (the old 2 ft against the new 0.25 ft); every shot logs a CAMSHOT line.
using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using Reflex.Port.Content;
using Reflex.Port.OriginalWorld;
using Reflex.Port.Rendering;
using UnityEngine;
using UnityEngine.Rendering;
using UnityEngine.SceneManagement;

[DefaultExecutionOrder(32000)]
public sealed class LightingShowcase : MonoBehaviour
{
    const string Tag = "[lighting-showcase] ";

    [Serializable] public sealed class View
    {
        public string name = "view", cam = "game", anchor = "bike", frame = "bike", look = "anchor";
        public float[] offset = { 0, 3, -8 }; public float[] at; public float lookY = 1f, yaw, pitch, fov, t = -1; public bool freeze = true;
        public int opp = -1;   // frame "opp": anchored to that opponent's bike (OriginalOpponents.Riders order); -2 = one shot per opponent
        // frame "tree" (w-treecards): the camera `treeDist` metres from the `tree`-th nearest 3D tree to the bike (horizontally, towards the bike),
        // `lookY` metres above the ground at the camera, looking at the tree's crown (`look` "anchor") or along yaw / pitch from the tree-to-bike heading
        public int tree = 0; public float treeDist = 10, treeMin = 0;   // treeMin: only trees with a crown at least this tall; treeType: only this tree asset (substring)
        public int treeId = -1; public string treeType = ""; public float[] treeAt;   // treeAt: [x, z] the ranking centre instead of the bike
        public float orbit; public bool treeBase;   // treeBase: the camera height from the tree's base, not the terrain under it; w-fb-vegetation: the camera turned this many degrees around the tree (the same tree from another side)
        public int frames = 16; public float step = 0.15f, yawStep = 0f, bobPitch = 0f, bobHeight = 0f, bobPeriod = 10f;   // cam "path"
        public int seq;   // other cams: save this many consecutive frames as rendered (<prefix>_<view>_<mode>_fNN.png; "freeze": false keeps the world running)
        public int state = -1; public float after, near;   // w-camfix: wait for this race state (+ after s of game time); a pose view's near plane
    }
    [Serializable] public sealed class Plan
    {
        public string @event = "National/NAT_Stillwater", prefix = "shot";
        public float settle = 8, adapt = 3, perf = 3;
        public int ai;   // opponents (frame "opp" views need them)
        public View[] views = Array.Empty<View>();
        public string[] modes = { "original", "enhanced" };
        public string camera = ""; public float camlog;   // w-camfix: the camera entry to use; seconds of per-frame camera log
    }

    int[] treeOrder; string treeLogged; int pathFrame;
    Plan plan; string dir; View active; StringBuilder perfLog = new StringBuilder(); int oppIndex = -1;

    // the opponents' bikes: (between the wheels, rear -> front wheel) (the posed parts are world placed; the chassis object stays at 0)
    static List<(Vector3 pos, Vector3 fwd)> Opps(OriginalWorldRuntime rt)
    {
        var l = new List<(Vector3, Vector3)>();
        if (!rt || !rt.opponentRiders) return l;
        var field = typeof(OriginalOpponents).GetField("opponents", System.Reflection.BindingFlags.NonPublic | System.Reflection.BindingFlags.Instance);
        if (!(field?.GetValue(rt.opponentRiders) is System.Collections.IList list)) return l;
        foreach (var o in list)
        {
            var t = o.GetType();
            var pos = (Vector3)t.GetField("position").GetValue(o);
            var bike = t.GetField("bike").GetValue(o) as ReflexBikeModel;
            Vector3 fwd = Vector3.forward;
            if (bike != null && bike.parts.Count > 0)
            {
                var part = bike.parts.FirstOrDefault(p => p.node == 0).part ?? bike.parts[0].part;
                if (part) fwd = part.forward;
            }
            l.Add((pos, fwd));
        }
        return l;
    }

    // The suspects of a mode name (enhanced-noshadow-nocontact ...) switched off; returns the undo.
    static Action Suspects(OriginalWorldRuntime rt, string mode)
    {
        var undo = new List<Action>();
        var parts = new HashSet<string>(mode.Split('-'));
        var cam = Cam(rt);
        var pipe = cam ? cam.GetComponent<ReflexRenderPipeline>() : null;
        var e = pipe ? pipe.enhanced : null;
        // MATERIALS: ENHANCED / MUD BUILDUP (w-rmmat): matorig / matenh switch the materials (and the mud) off / on for that shot; dirtNN forces the
        // dirt amount NN % on every vehicle (dirt0 = clean), wet its wetness 0.8
        if (parts.Contains("matorig")) { int was = ReflexEnhancedMaterials.AbOverride; ReflexEnhancedMaterials.AbOverride = 0; ReflexEnhancedMaterials.ShowLabel(); undo.Add(() => ReflexEnhancedMaterials.AbOverride = was); }
        if (parts.Contains("matenh")) { int was = ReflexEnhancedMaterials.AbOverride; ReflexEnhancedMaterials.AbOverride = 1; ReflexEnhancedMaterials.ShowLabel(); undo.Add(() => ReflexEnhancedMaterials.AbOverride = was); }
        // w-mudoff: the saved options as OPTIONS > VIDEO sets them (the run's scratch save): mudon / mudoff (MUD BUILDUP), matsetorig / matsetenh
        // (MATERIALS); menumat: LIGHTING: ORIGINAL (the option, no override) and MATERIALS: ORIGINAL, then the menu's pick of MATERIALS: ENHANCED
        // (ReflexEnhancedMaterials.PickMaterials when the build has it; before it, the menu only stored the option)
        if (parts.Contains("mudon") || parts.Contains("mudoff"))
        { int was = ReflexEnhancedMaterials.MudSetting; ReflexEnhancedMaterials.MudSetting = parts.Contains("mudon") ? 1 : 0; undo.Add(() => ReflexEnhancedMaterials.MudSetting = was); }
        if (parts.Contains("matsetorig") || parts.Contains("matsetenh"))
        { int was = ReflexEnhancedMaterials.MaterialsSetting; ReflexEnhancedMaterials.MaterialsSetting = parts.Contains("matsetenh") ? 1 : 0; undo.Add(() => ReflexEnhancedMaterials.MaterialsSetting = was); }
        if (parts.Contains("menumat"))
        {
            int wasOv = ReflexEnhancedLighting.Override, wasLight = Reflex.Port.Options.ReflexSettings.Lighting, wasMat = ReflexEnhancedMaterials.MaterialsSetting;
            ReflexEnhancedLighting.Override = -1; Reflex.Port.Options.ReflexSettings.Lighting = 0; ReflexEnhancedMaterials.MaterialsSetting = 0;
            var pick = typeof(ReflexEnhancedMaterials).GetMethod("PickMaterials", System.Reflection.BindingFlags.Public | System.Reflection.BindingFlags.Static);
            if (pick != null) pick.Invoke(null, new object[] { true }); else ReflexEnhancedMaterials.MaterialsSetting = 1;
            Debug.Log(Tag + "menumat: " + (pick != null ? "PickMaterials" : "the option only") + " -> LIGHTING " + Reflex.Port.Options.ReflexSettings.Lighting + " MATERIALS " + ReflexEnhancedMaterials.MaterialsSetting);
            undo.Add(() => { ReflexEnhancedLighting.Override = wasOv; Reflex.Port.Options.ReflexSettings.Lighting = wasLight; ReflexEnhancedMaterials.MaterialsSetting = wasMat; });
        }
        foreach (var pd in parts)
            if (pd.StartsWith("dirt") && float.TryParse(pd.Substring(4), out float dirtPct))
            { float was = ReflexDirt.ForceAmount, wasWet = ReflexDirt.ForceWet; ReflexDirt.ForceAmount = dirtPct / 100f; ReflexDirt.ForceWet = parts.Contains("wet") ? 0.8f : 0f; undo.Add(() => { ReflexDirt.ForceAmount = was; ReflexDirt.ForceWet = wasWet; }); }
        // w-rmvol: VOLUMETRIC LIGHT off (the original's fake shafts), its diagnostics (voldbg: the in-scattered light alone), quality vq0..2, fog fogN
        // WEATHER (Rendering/ReflexWeather.cs): wxclear / wxrain / wxsnow / wxdyn, wxiN (intensity 0..2), wxdbgN (the terrain's masks), wxwetNN /
        // wxpudNN / wxcovNN (forced wetness / puddle level / snow cover in %); every transition skipped (settled) for the shot
        {
            int wm = parts.Contains("wxrain") ? 1 : parts.Contains("wxsnow") ? 2 : parts.Contains("wxdyn") ? 3 : parts.Contains("wxclear") ? 0 : -1;
            if (wm >= 0)
            {
                ReflexWeather.Override = wm; ReflexWeather.Settle = true; ReflexWeather.Restart();
                foreach (var pw in parts)
                {
                    if (pw.StartsWith("wxi") && int.TryParse(pw.Substring(3), out int wi)) ReflexWeather.IntensityOverride = wi;
                    if (pw.StartsWith("wxdbg") && int.TryParse(pw.Substring(5), out int wd)) ReflexWeather.DebugView = wd;
                    if (pw.StartsWith("wxwet") && float.TryParse(pw.Substring(5), out float ww)) ReflexWeather.ForceWet = ww / 100f;
                    if (pw.StartsWith("wxpud") && float.TryParse(pw.Substring(5), out float wp)) ReflexWeather.ForcePuddles = wp / 100f;
                    if (pw.StartsWith("wxcov") && float.TryParse(pw.Substring(5), out float wc)) ReflexWeather.ForceCover = wc / 100f;
                }
                undo.Add(() => { ReflexWeather.Override = -1; ReflexWeather.IntensityOverride = -1; ReflexWeather.DebugView = 0; ReflexWeather.ForceWet = ReflexWeather.ForcePuddles = ReflexWeather.ForceCover = -1; ReflexWeather.Settle = false; });
            }
        }
        if (parts.Contains("novol")) { int was = ReflexVolumetricLight.Override; ReflexVolumetricLight.Override = 0; undo.Add(() => ReflexVolumetricLight.Override = was); }
        if (parts.Contains("voldbg")) { ReflexVolumetricLight.DebugView = 1; undo.Add(() => ReflexVolumetricLight.DebugView = 0); }
        foreach (var pe in parts)
        {
            if (pe.Length == 3 && pe.StartsWith("vq") && int.TryParse(pe.Substring(2), out int vq)) { ReflexVolumetricLight.TierOverride = vq; undo.Add(() => ReflexVolumetricLight.TierOverride = -1); }
            if (pe.Length == 4 && pe.StartsWith("fog") && int.TryParse(pe.Substring(3), out int fd)) { ReflexVolumetricLight.DensityOverride = fd; undo.Add(() => ReflexVolumetricLight.DensityOverride = -1); }
        }
        if (parts.Contains("rtfreeze")) Reflex.Port.Rendering.ReflexRTBump.FreezeHold = true;   // (kept for the rest of the run)
        if (parts.Contains("nonear")) { Reflex.Port.Rendering.ReflexRTBump.DebugNoNear = true; undo.Add(() => Reflex.Port.Rendering.ReflexRTBump.DebugNoNear = false); }
        if (parts.Contains("noshadow") && RenderSettings.sun)
        {
            var sun = RenderSettings.sun; var was = sun.shadows; sun.shadows = LightShadows.None; undo.Add(() => sun.shadows = was);
        }
        if (parts.Contains("nocontact") && e != null) { bool was = e.contact; e.contact = false; undo.Add(() => e.contact = was); }
        if (parts.Contains("noao") && e != null) { float was = e.aoIntensity; e.aoIntensity = 0; undo.Add(() => e.aoIntensity = was); }
        if (parts.Contains("noocc") && e != null) { bool was = e.occlusion; e.occlusion = false; undo.Add(() => e.occlusion = was); }
        if (parts.Contains("notrees"))
            foreach (var f in FindObjectsByType<OriginalForest>(FindObjectsSortMode.None)) { var ff = f; bool was = ff.castShadows; ff.castShadows = false; undo.Add(() => ff.castShadows = was); }
        if (parts.Contains("no3d"))
            foreach (var f in FindObjectsByType<OriginalForest>(FindObjectsSortMode.None)) { var ff = f; bool was = ff.draw3d; ff.draw3d = false; undo.Add(() => ff.draw3d = was); }
        if (parts.Contains("nobb"))
            foreach (var f in FindObjectsByType<OriginalForest>(FindObjectsSortMode.None)) { var ff = f; bool was = ff.drawBillboards; ff.drawBillboards = false; undo.Add(() => ff.drawBillboards = was); }
        var scenery = Shader.Find("Reflex/Original Scenery");
        bool noScenery = parts.Contains("noscenery"), noDynamic = parts.Contains("nodynamic");
        if (noScenery || noDynamic)
            foreach (var r in FindObjectsByType<Renderer>(FindObjectsSortMode.None))
            {
                if (r.shadowCastingMode == ShadowCastingMode.Off) continue;
                bool isScenery = r is MeshRenderer && r.sharedMaterial && r.sharedMaterial.shader == scenery;
                if (isScenery ? !noScenery : !noDynamic) continue;
                var rr = r; var was = rr.shadowCastingMode;
                rr.shadowCastingMode = ShadowCastingMode.Off; undo.Add(() => { if (rr) rr.shadowCastingMode = was; });
            }
        if (parts.Contains("nostatic"))
        {
            // the original's static shadows: the terrain's shadow visibility map and the scenery's volume shadow map
            var id = Shader.PropertyToID("_RfxVolumeShadowParams"); var was = Shader.GetGlobalVector(id);
            Shader.SetGlobalVector(id, new Vector4(was.x, 0, was.z, was.w)); undo.Add(() => Shader.SetGlobalVector(id, was));
            foreach (var t in Terrain.activeTerrains) { var m = t.materialTemplate; if (!m || !m.HasProperty("_UseShadowVis")) continue; float w = m.GetFloat("_UseShadowVis"); m.SetFloat("_UseShadowVis", 0); undo.Add(() => m.SetFloat("_UseShadowVis", w)); }
        }
        if (parts.Contains("noterrainrecv"))
            foreach (var t in Terrain.activeTerrains) { var m = t.materialTemplate; if (!m || !m.HasProperty("_UseUnityShadows")) continue; float w = m.GetFloat("_UseUnityShadows"); m.SetFloat("_UseUnityShadows", 0); undo.Add(() => m.SetFloat("_UseUnityShadows", w)); }
        foreach (var pe in parts)
            if (pe.StartsWith("pixerr") && float.TryParse(pe.Substring(6), out float px))
                foreach (var t in Terrain.activeTerrains) { var tt = t; float w = tt.heightmapPixelError; tt.heightmapPixelError = px; undo.Add(() => tt.heightmapPixelError = w); }
        // ANTI-ALIASING (w-rmaa)
        {
            int aa = parts.Contains("taa") ? 3 : parts.Contains("aaoff") || parts.Contains("ss2") ? 0 : parts.Contains("msaa2") ? 1 : parts.Contains("msaa4") ? 2 : -1;
            int wasMode = ReflexAntiAliasing.ModeOverride, wasSharp = ReflexAntiAliasing.SharpenOverride, wasUp = ReflexAntiAliasing.UpscaleOverride;
            if (aa >= 0) ReflexAntiAliasing.ModeOverride = aa;
            foreach (var pe in parts) if (pe.StartsWith("sharp") && int.TryParse(pe.Substring(5), out int sp)) ReflexAntiAliasing.SharpenOverride = sp;
            if (parts.Contains("upq")) ReflexAntiAliasing.UpscaleOverride = 1; else if (parts.Contains("upb")) ReflexAntiAliasing.UpscaleOverride = 2; else if (aa >= 0) ReflexAntiAliasing.UpscaleOverride = 0;
            int wasProf = ReflexTemporalAA.ProfileMode;
            foreach (var pe in parts) if (pe.StartsWith("taaprof") && int.TryParse(pe.Substring(7), out int pm)) ReflexTemporalAA.ProfileMode = pm;
            undo.Add(() => { ReflexTemporalAA.ProfileMode = wasProf; ReflexAntiAliasing.ModeOverride = wasMode; ReflexAntiAliasing.SharpenOverride = wasSharp; ReflexAntiAliasing.UpscaleOverride = wasUp; });
            if (GraphicsSettings.currentRenderPipeline is UnityEngine.Rendering.Universal.UniversalRenderPipelineAsset urp)
            {
                int wasMsaa = urp.msaaSampleCount; float wasScale = urp.renderScale;
                if (aa >= 0) urp.msaaSampleCount = aa == 1 ? 2 : aa == 2 ? 4 : 1;
                if (parts.Contains("ss2")) urp.renderScale = 2f;
                undo.Add(() => { urp.msaaSampleCount = wasMsaa; urp.renderScale = wasScale; });
            }
        }
        Debug.Log(Tag + "MODE " + mode + ": " + undo.Count + " switches");
        return () => { for (int i = undo.Count - 1; i >= 0; i--) undo[i](); };
    }

    // w-weather: the display gamma ramp (ReflexGammaOutput) used to run on offscreen renders too, copying the shot through a Screen-sized
    // (640 x 480 in -batchmode) texture and turning its rows; it skips them now, so the rows come back upright
    static readonly bool ShotFlip = Environment.GetEnvironmentVariable("REFLEX_SHOT_FLIP") == "1";

    [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
    static void Boot()
    {
        string p = Environment.GetEnvironmentVariable("REFLEX_LIGHTING_PLAN"), d = Environment.GetEnvironmentVariable("REFLEX_PREVIEW_SHOTS");
        if (string.IsNullOrEmpty(p) || string.IsNullOrEmpty(d) || FindAnyObjectByType<LightingShowcase>()) return;
        var go = new GameObject("Lighting showcase (test)"); DontDestroyOnLoad(go);
        var s = go.AddComponent<LightingShowcase>();
        s.plan = JsonUtility.FromJson<Plan>(File.ReadAllText(p)); s.dir = d;
        Application.runInBackground = true;
        FrameTimingManager.CaptureFrameTimings();
    }

    // w-fb-vegetation: the mean colour of a texture's texels with alpha > 0.5 (64 x 64 read back), logged as MEANTEX
    void MeanLog(Texture tx, string label)
    {
        if (!tx) return;
        var rtx = RenderTexture.GetTemporary(64, 64, 0, RenderTextureFormat.ARGB32, RenderTextureReadWrite.Linear);
        Graphics.Blit(tx, rtx); var prev = RenderTexture.active; RenderTexture.active = rtx;
        var t2 = new Texture2D(64, 64, TextureFormat.RGBA32, false); t2.ReadPixels(new Rect(0, 0, 64, 64), 0, 0); t2.Apply();
        RenderTexture.active = prev; RenderTexture.ReleaseTemporary(rtx);
        double r = 0, g = 0, b = 0; int n = 0;
        foreach (var c in t2.GetPixels32()) { if (c.a <= 128) continue; r += c.r; g += c.g; b += c.b; n++; }
        Destroy(t2);
        if (n == 0) return;
        r /= n * 255.0; g /= n * 255.0; b /= n * 255.0;
        Debug.Log(Tag + $"MEANTEX {label} | {tx.name} n {n} rgb {r:0.000} {g:0.000} {b:0.000} lum {0.299 * r + 0.587 * g + 0.114 * b:0.000}");
    }

    // muted test player (w-alivetrees): the user's sound is never played by a showcase run
    void Update() { AudioListener.volume = 0f; }

    IEnumerator Start()
    {
        Directory.CreateDirectory(dir);
        Debug.Log(Tag + "BEGIN " + plan.@event + " views " + string.Join(",", plan.views.Select(v => v.name)));
        float t0 = Time.realtimeSinceStartup;
        while (Time.realtimeSinceStartup - t0 < 4f) yield return null;   // the front end settles (fonts, the shell)
        ReflexEnhancedLighting.Override = 0;
        {   // w-fb-customtrack: archives in the client root's Mods/Tracks are installed first (Content/TrackArchives.cs), then listed
            float ta = Time.realtimeSinceStartup;
            try { _ = TrackMods.All; } catch (Exception e) { Debug.Log(Tag + "tracks: " + e.Message); }
            while (TrackArchives.Busy && Time.realtimeSinceStartup - ta < 300) yield return null;
            bool again = TrackMods.Refresh();
            Debug.Log(Tag + "archives settled after " + (Time.realtimeSinceStartup - ta).ToString("0.0") + " s" + (again ? " (rescanned)" : ""));
        }
        // w-fb-modplates2: REFLEX_LIGHTING_PV_ROW = the scratch profile's PlayerVehicle row to ride, or REFLEX_LIGHTING_MODEL = the first row of
        // that VehicleModelId (equipped before the launch; every vehicle unlocked in the scratch profile)
        int pvRow = -1; bool byModel = int.TryParse(Environment.GetEnvironmentVariable("REFLEX_LIGHTING_MODEL"), out int wantModel);
        if (byModel || int.TryParse(Environment.GetEnvironmentVariable("REFLEX_LIGHTING_PV_ROW"), out pvRow))
        {
            ReflexProfile.Disabled = false; CoreGarageProfile.Open();
            if (!CoreGarageProfile.UnlockEverythingEnabled) CoreGarageProfile.SetUnlockEverything(true);   // (every vehicle owned: the rows to pick from)
            ReflexProfile.Open();
            var pv = ReflexProfile.Current.T("PlayerVehicle");
            if (byModel) for (int i = 0; i < pv.rows; i++) if (pv.Int("VehicleModelId", i, -1) == wantModel) { pvRow = i; break; }
            int pvId = pv.Int("PlayerVehicleId", pvRow, -1);
            Debug.Log(Tag + "vehicle row " + pvRow + " of " + pv.rows + ": PlayerVehicleId " + pvId + " model " + pv.Int("VehicleModelId", pvRow, -1));
            if (pvId >= 0) { var task = ReflexProfile.SelectRider(0, -1, pvId); while (!task.IsCompleted) yield return null; Debug.Log(Tag + "vehicle selected: " + task.Result); }
        }
        try
        {
            // MODS/TRACKS: the custom tracks of the client root's Mods/Tracks too (REFLEX_CLIENT_ROOT; e.g. "event": "FreeRide/Beta_Track_Slot_1/@")
            var all = ReflexEvents.All.Concat(TrackMods.Events).ToList();
            foreach (var m in TrackMods.All) Debug.Log(Tag + "custom track " + m.id + ": " + m.name + " " + m.kindLabel + " " + m.level + (m.problem != null ? " BROKEN " + m.problem : ""));
            Debug.Log(Tag + "events: " + string.Join(" ", all.Select(e => e.key)));
            var ev = ReflexEvents.Find(plan.@event) ?? all.FirstOrDefault(e => e.key.StartsWith(plan.@event, StringComparison.OrdinalIgnoreCase))
                     ?? all.FirstOrDefault(e => e.key.IndexOf(plan.@event, StringComparison.OrdinalIgnoreCase) >= 0);
            if (ev == null) throw new InvalidOperationException("no event " + plan.@event);
            Debug.Log(Tag + "LAUNCH " + ev.key);
            ReflexEventSelect.Launch(ev.key, plan.ai, false, "Reflex", 0, null);
        }
        catch (Exception e) { Fail(e.ToString()); yield break; }

        OriginalWorldRuntime rt = null; OriginalLevel level = null;
        float wait = Time.realtimeSinceStartup;
        while (true)
        {
            rt = FindAnyObjectByType<OriginalWorldRuntime>(); level = FindAnyObjectByType<OriginalLevel>();
            if (rt && rt.FailureMessage != null) { Fail("world: " + rt.FailureMessage); yield break; }
            if (rt && level && level.Loaded && rt.Running && !double.IsNaN(rt.PresentedGameFrame)) break;
            if (Time.realtimeSinceStartup - wait > 600) { Fail("the event did not start"); yield break; }
            yield return null;
        }
        Debug.Log(Tag + "RUNNING " + level.level + " load " + (Time.realtimeSinceStartup - wait).ToString("0.0") + " s");
        if (!string.IsNullOrEmpty(plan.camera))
        {   // w-camfix: the camera entry (a custom preset turned ON first); not persisted (a batch run never writes the save anyway)
            if (plan.camera.StartsWith(Reflex.Port.Options.ReflexCameraPresets.CustomPrefix)) Reflex.Port.Options.ReflexCameraPresets.SetOn(plan.camera, true);
            Reflex.Port.Options.ReflexCameraPresets.Select(plan.camera, false);
            Debug.Log(Tag + "CAMERA " + Reflex.Port.Options.ReflexCameraPresets.CurrentId + " dc " + rt.DrivingCameraPreset);
        }
        if (plan.camlog > 0) StartCoroutine(CamLog(rt, plan.camlog));
        if (Environment.GetEnvironmentVariable("REFLEX_LS_PROPS") == "1") StartCoroutine(PropLog(level));   // w-fb-models
        // w-fb-models: REFLEX_LS_FIND=a,b: the scenery renderers whose object or material names hold one of the words (bounds, materials, the
        // materials' programs / textures / parameters); the local bike's parts (name, material, triangles)
        string find = Environment.GetEnvironmentVariable("REFLEX_LS_FIND");
        if (!string.IsNullOrEmpty(find))
        {
            var words = find.Split(',').Where(x => x.Length > 0).ToArray();
            bool Hit(string n) => n != null && words.Any(wd => n.IndexOf(wd, StringComparison.OrdinalIgnoreCase) >= 0);
            var mats = new HashSet<string>();
            int shown = 0;
            foreach (var r in FindObjectsByType<Renderer>(FindObjectsInactive.Include, FindObjectsSortMode.None))
            {
                var names = r.sharedMaterials.Where(m => m).Select(m => m.name).ToArray();
                if (!Hit(r.name) && !Hit(r.transform.parent ? r.transform.parent.name : null) && !names.Any(Hit)) continue;
                foreach (var n in names) mats.Add(n);
                if (shown++ < 80) Debug.Log(Tag + $"FIND {r.name} (parent {(r.transform.parent ? r.transform.parent.name : "-")}) active {r.gameObject.activeInHierarchy} bounds {r.bounds.center:F1} size {r.bounds.size:F1} materials {string.Join(" / ", names)} shader {(r.sharedMaterial ? r.sharedMaterial.shader.name : "-")}");
            }
            Debug.Log(Tag + $"FIND {shown} renderers");
            foreach (var line in level.Content.DescribeMaterials().Split('\n'))
                if (line.Length > 0 && mats.Any(m => line.StartsWith(m.Split(' ')[0] + " ", StringComparison.Ordinal))) Debug.Log(Tag + "FINDMAT " + line);
            foreach (var m in mats.Take(40))
            {
                var mat = FindObjectsByType<Renderer>(FindObjectsInactive.Include, FindObjectsSortMode.None).SelectMany(r => r.sharedMaterials).FirstOrDefault(x => x && x.name == m);
                if (!mat) continue;
                var sb = new StringBuilder();
                foreach (var tn in mat.GetTexturePropertyNames()) { var tx = mat.GetTexture(tn); if (tx) sb.Append($" {tn}={tx.name}({tx.width}x{tx.height} {(tx is Texture2D t2 ? t2.format.ToString() : "")})"); }
                Debug.Log(Tag + $"FINDTEX {m}: shader {mat.shader.name} keywords [{string.Join(",", mat.shaderKeywords)}]{sb}");
            }
            var bm = rt.BikeModel;
            if (bm != null) foreach (var pt in bm.parts) { var mf = pt.part ? pt.part.GetComponent<MeshFilter>() : null; Debug.Log(Tag + $"BIKEPART {pt.name} material {pt.material} triangles {(mf && mf.sharedMesh ? mf.sharedMesh.triangles.Length / 3 : -1)}"); }
        }
        if (Environment.GetEnvironmentVariable("REFLEX_LOOK_DUMP") == "1")
        {
            // ALIVE LOOK (w-alivereflex): the terrain material's textures as PNGs (after a few frames: the top-down bake happens at the first render)
            for (int f = 0; f < 10; f++) yield return null;
            var tm = level.terrain && level.terrain.UnityTerrain ? level.terrain.UnityTerrain.materialTemplate : null;
            if (tm)
                foreach (var prop in new[] { "_TopDownMap", "_DetailBase", "_DetailRed", "_DetailGreen", "_DetailBlue", "_DetailMask", "_ChromLumMap", "_SphericalLightMap", "_SphericalReflectionMap" })
                {
                    var tx = tm.GetTexture(prop); if (!tx) continue;
                    var rtx = RenderTexture.GetTemporary(Mathf.Min(tx.width, 1024), Mathf.Min(tx.height, 1024), 0, RenderTextureFormat.ARGB32, RenderTextureReadWrite.Linear);
                    Graphics.Blit(tx, rtx); var prev = RenderTexture.active; RenderTexture.active = rtx;
                    var t2 = new Texture2D(rtx.width, rtx.height, TextureFormat.RGBA32, false); t2.ReadPixels(new Rect(0, 0, rtx.width, rtx.height), 0, 0); t2.Apply();
                    RenderTexture.active = prev; RenderTexture.ReleaseTemporary(rtx);
                    File.WriteAllBytes(Path.Combine(dir, $"{plan.prefix}_terrain{prop}.png"), t2.EncodeToPNG()); Destroy(t2);
                    Debug.Log(Tag + $"TERRAIN {prop} {tx.name} {tx.width}x{tx.height}");
                }
            Debug.Log(Tag + "TERRAIN params _AliveLook " + (tm && tm.HasProperty("_AliveLook") ? tm.GetVector("_AliveLook").ToString() + " " + tm.GetVector("_AliveLook2").ToString("F3") + " mean " + tm.GetVector("_AliveTopDownMean").ToString("F3") + " kinds " + tm.GetVector("_AliveLayerClass") + " present " + tm.GetVector("_AliveLayerClassPresent") : "-") + " _DirtTint " + (tm ? tm.GetVector("_DirtTint").ToString() : "-") + " _DetailTile " + (tm ? tm.GetVector("_DetailTile").ToString() : "-"));
            Debug.Log(Tag + "CONTENT " + level.Content.report.Replace("\n", " | "));
        }
        {
            var fo = level.Content.forest;
            if (fo != null)
            {
                var cnt = new int[fo.types.Count];
                for (int i = 0; i < fo.treeCount; i++) cnt[(int)fo.trees[i * 8 + 3]]++;
                for (int k = 0; k < fo.types.Count; k++) { var ty = fo.types[k]; Debug.Log(Tag + $"TYPE {k} {ty.treeAsset} x{cnt[k]} loaded {ty.loaded} leaf {ty.leafMaterial} frond {ty.frondMaterial} branch {ty.branchMaterial} bounds {ty.boundsCenter} {ty.boundsHalf} records {ty.records.Count} maxdist {ty.MaxDistance:0} leafLods {ty.leafLods}");
                    Debug.Log(Tag + $"BB {k} {ty.treeAsset} rows [{string.Join(",", ty.billboard.Select(q => q.ToString("0.####")))}] bbStart {ty.bbStart:0.0}");
                    for (int r = 0; r < ty.records.Count; r++)
                    {
                        var rec = ty.records[r]; var sb = new StringBuilder();
                        foreach (float dd in new[] { -20f, -5f, 0f, 2f, 5f, 10f, 20f, 40f, 80f, 150f }) { var a = ReflexLevelContent.TreeAlphaTests(ty, rec, dd); sb.Append($" d{dd:0}:({a.x:0.00},{a.y:0.00},{a.z:0.00},{a.w:0.00})"); }
                        Debug.Log(Tag + $"REC {k} {ty.treeAsset} r{r} dist {rec.dist:0.0} lods b{rec.branchLod} f{rec.frondLod} l{rec.leafLod} trans {rec.transition} leafparams [{string.Join(",", rec.leaf.Select(q => q.ToString("0.###")))}] alpha" + sb);
                    }
                }
            }
        }
        try
        {
            var of = FindAnyObjectByType<OriginalForest>();
            var bf = typeof(OriginalForest).GetField("build", System.Reflection.BindingFlags.NonPublic | System.Reflection.BindingFlags.Instance);
            var fb = bf?.GetValue(of) as ForestBuild;
            if (fb != null)
            {
                void Dump(Texture tx, string name)
                {
                    if (!tx) return;
                    var rtx = RenderTexture.GetTemporary(Mathf.Min(tx.width, 1024), Mathf.Min(tx.height, 1024), 0, RenderTextureFormat.ARGB32, RenderTextureReadWrite.Linear);
                    Graphics.Blit(tx, rtx); var prev = RenderTexture.active; RenderTexture.active = rtx;
                    var t2 = new Texture2D(rtx.width, rtx.height, TextureFormat.RGBA32, false); t2.ReadPixels(new Rect(0, 0, rtx.width, rtx.height), 0, 0); t2.Apply();
                    RenderTexture.active = prev; RenderTexture.ReleaseTemporary(rtx);
                    File.WriteAllBytes(Path.Combine(dir, $"tex_{name}.png"), t2.EncodeToPNG()); Destroy(t2);
                    Debug.Log(Tag + $"TEX {name} {tx.width}x{tx.height} {tx.name}");
                }
                for (int k = 0; k < fb.data.types.Count; k++)
                    for (int g = 0; g < 3; g++) { var m = fb.materials[k, g]; if (m) Dump(m.GetTexture("_BaseMap"), $"type{k}_{g}_{m.name.Replace(' ', '_')}"); }
                Dump(fb.billboardMaterial.GetTexture("_BaseMap"), "billboard_atlas");
            }
            // w-fb-vegetation: the grass atlas / colour map, and the mean colour (texels with alpha > 0.5) of every scenery renderer's diffuse
            var gf = typeof(OriginalForest).GetField("grassBuild", System.Reflection.BindingFlags.NonPublic | System.Reflection.BindingFlags.Instance);
            if (of && gf?.GetValue(of) is GrassBuild gb && gb.far) { MeanLog(gb.far.GetTexture("_GrassTex"), "GRASS atlas"); MeanLog(gb.far.GetTexture("_ColorTex"), "GRASS colourmap"); }
            if (fb != null) { for (int k = 0; k < fb.data.types.Count; k++) for (int g = 0; g < 3; g++) { var m = fb.materials[k, g]; if (m) MeanLog(m.GetTexture("_BaseMap"), $"TREE {g} {m.name}"); } MeanLog(fb.billboardMaterial.GetTexture("_BaseMap"), "TREE bb atlas"); }
            var seen = new HashSet<Texture>();
            foreach (var r in FindObjectsByType<Renderer>(FindObjectsSortMode.None))
                foreach (var m in r.sharedMaterials)
                    if (m && m.HasProperty("_BaseMap") && m.GetTexture("_BaseMap") is Texture tx && seen.Add(tx)) MeanLog(tx, "SCENERY " + m.shader.name + " " + m.name);
        }
        catch (Exception e) { Debug.LogWarning(Tag + "tex dump " + e); }
        float s0 = Time.realtimeSinceStartup; double f0 = rt.PresentedGameFrame;
        while (Time.realtimeSinceStartup - s0 < plan.settle) { HideHud(rt); yield return null; }
        var sunDir = level.Content.lighting.sunDirection.normalized;
        var bike = BikePos(rt);
        Debug.Log(Tag + $"bike {bike} heading {BikeFwd(rt)} sun {sunDir} (azimuth {Mathf.Atan2(sunDir.z, sunDir.x) * Mathf.Rad2Deg:0.0}, elevation {Mathf.Asin(sunDir.y) * Mathf.Rad2Deg:0.0}) camera {Cam(rt)?.transform.position}");

        int vi = 0;
        foreach (var v in plan.views)
        {
            // t: seconds after the world runs (the game's own intro cameras are frozen there); the world is paused for the shots
            // (game time: the game's frames since it started running, so the pauses for the shots do not move the intro)
            while (v.state < 0 && v.t >= 0 && !(rt.PresentedGameFrame - f0 >= v.t * 60.0)) { HideHud(rt); yield return null; }
            if (v.state >= 0)
            {   // w-camfix: the race state, then `after` seconds of running (unpaused) time (PresentedGameFrame is NaN for a moment at the gate drop)
                float sw = Time.realtimeSinceStartup;
                while (!(rt.Race.valid != 0 && rt.Race.state >= v.state) && Time.realtimeSinceStartup - sw < 120f) { HideHud(rt); yield return null; }
                for (float run = 0f; run < v.after; ) { HideHud(rt); yield return null; if (!rt.paused) run += Time.unscaledDeltaTime; }
            }
            if (v.freeze && rt) rt.paused = true;
            active = v.cam == "game" ? null : v; treeOrder = null;
            var oppList = Opps(rt);
            int oppFirst = v.opp == -2 ? 0 : 0, oppCount = v.opp == -2 ? oppList.Count : 1;
            for (int oi = oppFirst; oi < oppCount; oi++)
            {
            if (v.opp == -2) oppIndex = oi; else oppIndex = v.opp;
            string vname = v.opp == -2 ? v.name + "_o" + oi : v.name;
            foreach (var mode in plan.modes)
            {
                // "enhanced-<suspect>[-<suspect>..]": ENHANCED with suspects switched off (the dark-lines hunt, w-darklines)
                ReflexEnhancedLighting.Override = mode == "original" || mode.StartsWith("original-") ? 0 : 1;
                { string d0 = mode.Split('-')[0]; ReflexEnhancedLighting.DebugView = d0 == "ao" ? 1 : d0 == "contact" ? 2 : d0 == "pos" ? 3 : d0 == "nrm" ? 4 : 0; }
                yield return null; yield return null;   // the mode is applied in LateUpdate
                var undo = Suspects(rt, mode);
                float a0 = Time.realtimeSinceStartup;
                while (Time.realtimeSinceStartup - a0 < plan.adapt) { HideHud(rt); yield return null; }
                if (vi == 0 && oi == 0 && plan.perf > 0) yield return Perf(mode, plan.perf);
                if (mode.Contains("-wx")) Debug.Log(Tag + $"WEATHER {vname} {mode}: active {ReflexWeather.Active} {ReflexWeather.Report}");
                { var cc = Cam(rt); var b = BikePos(rt); Debug.Log(Tag + $"CAMSHOT {vname} {mode} race {rt.Race.valid}/{rt.Race.state} camera {Reflex.Port.Options.ReflexCameraPresets.CurrentId} weight {rt.CameraRig.Weight:F2} game {rt.CameraRig.GameDistance:F2} hold {rt.CameraRig.HoldDistance:F2} drawn {(cc ? Vector3.Distance(cc.transform.position, b) : -1):F2} near {(cc ? cc.nearClipPlane : -1):F3} fov {(cc ? cc.fieldOfView : -1):F1}"); }
                if (mode.Contains("matorig") || mode.Contains("matenh")) { ReflexEnhancedMaterials.ShowLabel(); yield return null; }   // (the state label in the shot)
                if (v.cam == "path" || v.seq > 1)
                {
                    // consecutive frames (along the path: the world stays paused, only the camera moves), each kept as rendered and written after
                    // the sequence (encoding while capturing would slow the frames down)
                    int n = v.cam == "path" ? v.frames : v.seq;
                    var seqShots = new List<Texture2D>();
                    for (int k = 0; k < n; k++)
                    {
                        pathFrame = k;
                        yield return new WaitForEndOfFrame();
                        seqShots.Add(ScreenCapture.CaptureScreenshotAsTexture());
                        var pc = Cam(rt); var taa = ReflexTemporalAA.Active;
                        Debug.Log(Tag + $"PATH {vname} {mode} f{k:00} dt {Time.unscaledDeltaTime * 1000f:0.0} ms game {rt.PresentedGameFrame:0.00} cam {pc?.transform.position:F3} yaw {pc?.transform.eulerAngles.y:0.00} pitch {pc?.transform.eulerAngles.x:0.00} fov {pc?.fieldOfView:0.0} {(taa ? taa.report : "no TAA")}");
                        yield return null;
                    }
                    for (int k = 0; k < seqShots.Count; k++) { File.WriteAllBytes(Path.Combine(dir, $"{plan.prefix}_{vname}_{mode}_f{k:00}.png"), seqShots[k].EncodeToPNG()); Destroy(seqShots[k]); }
                    pathFrame = 0;
                    for (int k = 0; k < 3; k++) yield return null;
                }
                if (Application.isBatchMode) yield return null; else yield return new WaitForEndOfFrame();
                string file = Path.Combine(dir, $"{plan.prefix}_{vname}_{mode}.png");
                if (Application.isBatchMode && Environment.GetEnvironmentVariable("REFLEX_SHOTS_TAA") == "1") yield return OffscreenTaa(Cam(rt), file);   // w-alivetrees: with the TAA resolve
                else if (Application.isBatchMode) OffscreenShot(Cam(rt), file);   // -batchmode (run.py --batch): no window, the camera into a target
                else ScreenCapture.CaptureScreenshot(file);
                var cam = Cam(rt);
                var enh = cam ? cam.GetComponent<ReflexEnhancedLighting>() : null;
                var taaNow = cam ? cam.GetComponent<ReflexTemporalAA>() : null;
                if (taaNow) Debug.Log(Tag + $"AA {mode}: {ReflexAntiAliasing.Current} active {taaNow.active} {taaNow.report} resets {taaNow.resets}");
                Debug.Log(Tag + $"SHOT {file} anchor {(active != null && oppIndex >= 0 && oppIndex < Opps(rt).Count ? Opps(rt)[oppIndex].pos.ToString() : "-")} camera {cam?.transform.position} fwd {cam?.transform.forward} fov {cam?.fieldOfView:0.0} opp {oppIndex}" + (enh ? " lighting: " + enh.report : ""));
                if (active != null && level.Content.forest != null)
                {
                    var fo = level.Content.forest; var cm = Cam(rt); var pl = GeometryUtility.CalculateFrustumPlanes(cm); var rows = new List<(float d, string s)>();
                    for (int i = 0; i < fo.treeCount; i++)
                    {
                        var ty = fo.types[(int)fo.trees[i * 8 + 3]]; if (!ty.loaded) continue;
                        var bp = new Vector3(fo.trees[i * 8], fo.trees[i * 8 + 1], fo.trees[i * 8 + 2]); float sc = fo.trees[i * 8 + 4];
                        var mid = bp + Vector3.up * ((ty.boundsCenter.y) * sc); float rad = ty.boundsHalf.magnitude * sc * 0.5f;
                        float dd = Vector3.Distance(cm.transform.position, mid); if (dd > 260f) continue;
                        bool vis = true; foreach (var pp in pl) if (pp.GetDistanceToPoint(mid) < -rad) { vis = false; break; }
                        if (!vis) continue; var sp = cm.WorldToViewportPoint(mid);
                        float dep = Vector3.Dot(bp - cm.transform.position, cm.transform.forward);
                        rows.Add((dd, $"VIS #{i} {ty.treeAsset} at {bp.x:0.0},{bp.y:0.0},{bp.z:0.0} base dist {Vector3.Distance(cm.transform.position, bp):0} depth {dep:0} mid viewport {sp.x:0.00},{sp.y:0.00} scale {sc:0.00} rot {fo.trees[i * 8 + 5]:0.00}"));
                    }
                    rows.Sort((a, b) => a.d.CompareTo(b.d));
                    foreach (var r in rows.Where(q => q.s.Contains("WhiteF")).Take(25)) Debug.Log(Tag + $"{active.name} " + r.s);
                }
                if (active != null && Environment.GetEnvironmentVariable("REFLEX_RENDERER_LOG") == "1")
                {
                    var cm = Cam(rt); var pl = GeometryUtility.CalculateFrustumPlanes(cm); var rr = new List<(float d, string s)>();
                    foreach (var r in FindObjectsByType<Renderer>(FindObjectsSortMode.None))
                    {
                        if (!r.enabled || !r.gameObject.activeInHierarchy) continue;
                        var bb = r.bounds; if (!GeometryUtility.TestPlanesAABB(pl, bb)) continue;
                        float d = Vector3.Distance(cm.transform.position, bb.center); if (d > 400f || bb.size.magnitude < 12f) continue;
                        var sp = cm.WorldToViewportPoint(bb.center);
                        var m = r.sharedMaterial; string mf = r is MeshRenderer ? (r.GetComponent<MeshFilter>()?.sharedMesh?.name ?? "") : r.GetType().Name;
                        rr.Add((d, $"REND {r.name} [{r.GetType().Name}] mesh {mf} mat {(m ? m.name : "-")} shader {(m && m.shader ? m.shader.name : "-")} center {bb.center:0} size {bb.size:0} dist {d:0} viewport {sp.x:0.00},{sp.y:0.00} verts {(r.GetComponent<MeshFilter>()?.sharedMesh?.vertexCount ?? 0)}"));
                    }
                    rr.Sort((a, b) => a.d.CompareTo(b.d));
                    foreach (var q in rr.Take(60)) Debug.Log(Tag + active.name + " " + q.s);
                    if (active == plan.views[0])
                        foreach (var kv in level.Content.sceneMaterials.OrderBy(k => k.Key))
                            if (kv.Value.shader != null && kv.Value.shader.alphaSampler >= 0 && (kv.Value.alphaTest || kv.Value.alphaBlend))
                            {
                                level.Content.SceneryMaterials.TryGetValue(kv.Key, out var um);
                                Debug.Log(Tag + $"ALPHAPS {kv.Key} ps {kv.Value.pixelShader} sampler s{kv.Value.shader.alphaSampler} test {kv.Value.alphaTest} blend {kv.Value.alphaBlend} unity _AlphaSource {(um && um.HasProperty("_AlphaSource") ? um.GetFloat("_AlphaSource") : -1)}");
                            }
                    foreach (var r in FindObjectsByType<MeshRenderer>(FindObjectsSortMode.None))
                    {
                        if (r.name.IndexOf(Environment.GetEnvironmentVariable("REFLEX_RENDERER_DUMP") ?? "~~", StringComparison.OrdinalIgnoreCase) < 0) continue;
                        var mesh = r.GetComponent<MeshFilter>()?.sharedMesh; int mi = 0;
                        foreach (var kv in level.Content.sceneMaterials)
                            if (kv.Key.IndexOf("LeaningTree", StringComparison.OrdinalIgnoreCase) >= 0 || kv.Value.pixelShader == "Template_Eco_Leaf_CamFace")
                            {
                                var lm = kv.Value;
                                try { File.WriteAllBytes(Path.Combine(dir, "shader_" + lm.pixelShader + ".bin"), level.Content.ReadAsset(lm.pixelShader, "shader")); } catch (Exception ex) { Debug.Log(Tag + "shader dump " + ex.Message); }
                                Debug.Log(Tag + $"LM {kv.Key}: ps {lm.pixelShader} vs {lm.vertexShader} cull {lm.cull} alphaTest {lm.alphaTest} ref {lm.alphaRef} func {lm.alphaFunc} blend {lm.alphaBlend} textures [{string.Join(",", lm.textures)}] samplers [{string.Join(",", lm.userSamplers)}] shaderSamplers [{(lm.shader != null ? string.Join(",", lm.shader.samplers.Select(q => q.Key + "=" + q.Value)) : "")}] consts [{(lm.shader != null ? string.Join(",", lm.shader.constants) : "")}] params [{string.Join(";", lm.parameters.Select(q => q.Key + "=" + string.Join(",", q.Value)))}]");
                            }
                        Debug.Log(Tag + $"DUMP {r.name} mesh {mesh?.name} verts {mesh?.vertexCount} submeshes {mesh?.subMeshCount} materials {r.sharedMaterials.Length} pos {r.transform.position} rot {r.transform.rotation.eulerAngles} scale {r.transform.lossyScale}");
                        foreach (var m in r.sharedMaterials)
                        {
                            var sb = new StringBuilder($"DUMP mat[{mi++}] {m.name} shader {m.shader.name} queue {m.renderQueue} keywords {string.Join(",", m.shaderKeywords)}");
                            var sh = m.shader;
                            for (int pi = 0; pi < sh.GetPropertyCount(); pi++)
                            {
                                string pn = sh.GetPropertyName(pi); var pt = sh.GetPropertyType(pi);
                                if (pt == UnityEngine.Rendering.ShaderPropertyType.Texture) { var tx = m.GetTexture(pn); sb.Append($" {pn}={(tx ? tx.name + " " + tx.width + "x" + tx.height + " " + (tx is Texture2D t2 ? t2.format.ToString() : tx.GetType().Name) : "null")};"); }
                                else if (pt == UnityEngine.Rendering.ShaderPropertyType.Float || pt == UnityEngine.Rendering.ShaderPropertyType.Range) sb.Append($" {pn}={m.GetFloat(pn):0.###};");
                                else if (pt == UnityEngine.Rendering.ShaderPropertyType.Color || pt == UnityEngine.Rendering.ShaderPropertyType.Vector) sb.Append($" {pn}={m.GetVector(pn)};");
                            }
                            Debug.Log(Tag + sb);
                            var bt = m.HasProperty("_BaseMap") ? m.GetTexture("_BaseMap") : null;
                            if (bt)
                            {
                                var rtx = RenderTexture.GetTemporary(bt.width, bt.height, 0, RenderTextureFormat.ARGB32, RenderTextureReadWrite.Linear);
                                Graphics.Blit(bt, rtx); var prev = RenderTexture.active; RenderTexture.active = rtx;
                                var t2 = new Texture2D(rtx.width, rtx.height, TextureFormat.RGBA32, false); t2.ReadPixels(new Rect(0, 0, rtx.width, rtx.height), 0, 0); t2.Apply();
                                RenderTexture.active = prev; RenderTexture.ReleaseTemporary(rtx);
                                File.WriteAllBytes(Path.Combine(dir, $"dump_{m.name}.png"), t2.EncodeToPNG()); Destroy(t2);
                            }
                        }
                        break;
                    }
                }
                yield return null; yield return null;
                undo();
            }
            }
            if (v.freeze && rt) rt.paused = false;
            active = null;
            vi++;
        }
        File.WriteAllText(Path.Combine(dir, plan.prefix + "_perf.txt"), perfLog.ToString());
        WriteCamLog();   // (the plan's shots may end before the log's seconds)
        Debug.Log(Tag + "DONE");
        yield return new WaitForSeconds(1f);
        Application.Quit(0);
    }

    // w-camfix: the driving camera every frame (game time from the start of the log; paused frames are skipped)
    IEnumerator CamLog(OriginalWorldRuntime rt, float seconds)
    {
        var sb = camLog = new StringBuilder("run_s,race_state,weight,game_dist,hold_dist,drawn_dist,near,fov,speed\n");
        float run = 0f;   // unpaused seconds (PresentedGameFrame is NaN for a moment at the gate drop)
        while (rt && run < seconds)
        {
            yield return new WaitForEndOfFrame();
            if (!rt || rt.paused) continue;
            run += Time.unscaledDeltaTime;
            var cc = Cam(rt); if (!cc) continue;
            var b = BikePos(rt);
            sb.Append(run.ToString("0.000")).Append(',').Append(rt.Race.valid != 0 ? rt.Race.state : -1).Append(',')
              .Append(rt.CameraRig.Weight.ToString("0.000")).Append(',').Append(rt.CameraRig.GameDistance.ToString("0.00")).Append(',')
              .Append(rt.CameraRig.HoldDistance.ToString("0.00")).Append(',').Append(Vector3.Distance(cc.transform.position, b).ToString("0.00")).Append(',')
              .Append(cc.nearClipPlane.ToString("0.000")).Append(',').Append(cc.fieldOfView.ToString("0.0")).Append(',').Append(rt.Bike.speed.ToString("0.0")).Append('\n');
        }
        WriteCamLog();
    }
    StringBuilder camLog;
    void WriteCamLog()
    {
        if (camLog == null) return;
        File.WriteAllText(Path.Combine(dir, plan.prefix + "_camlog.csv"), camLog.ToString());
        Debug.Log(Tag + "CAMLOG written"); camLog = null;
    }

    // per-pass GPU times (w-rmaa: the TAA's cost on a busy machine, where the whole frame's GPU time is noisy): the samplers of the passes
    List<(string name, UnityEngine.Profiling.Recorder r)> passRecorders;
    static readonly string[] PassKeys = { "Reflex", "Motion", "Depth", "Opaque", "Transparent", "Copy", "Blit", "Shadow" };

    IEnumerator Perf(string mode, float seconds)
    {
        // w-alivetrees: a -batchmode player renders its window-less back buffer at 640 x 480; REFLEX_PERF_SIZE=WxH makes the game camera render
        // into a target of that size for the measurement (the GPU frame time at a real resolution)
        RenderTexture perfTarget = null; Camera perfCam = null;
        var psz = Environment.GetEnvironmentVariable("REFLEX_PERF_SIZE");
        if (Application.isBatchMode && !string.IsNullOrEmpty(psz) && psz.Split('x') is var pp && pp.Length == 2 && int.TryParse(pp[0], out int pw) && int.TryParse(pp[1], out int ph))
        {
            perfCam = Cam(FindAnyObjectByType<OriginalWorldRuntime>());
            if (perfCam) { perfTarget = new RenderTexture(pw, ph, 24, RenderTextureFormat.ARGB32); perfCam.targetTexture = perfTarget; }
            for (int k = 0; k < 30; k++) yield return null;   // (settle at the new size)
            if (perfTarget)
            {
                // the cost of one render of the view: batches of 20 cam.Render() into the target, a 1-pixel read back (waits for the GPU)
                // after each batch; median over the batches (CPU submission + GPU, the frame's own update excluded)
                var px = new Texture2D(1, 1, TextureFormat.RGBA32, false); var per = new List<double>();
                var sw = new System.Diagnostics.Stopwatch(); float until = Time.realtimeSinceStartup + seconds;
                while (Time.realtimeSinceStartup < until)
                {
                    yield return null;
                    var prevA = RenderTexture.active;
                    RenderTexture.active = perfTarget; px.ReadPixels(new Rect(0, 0, 1, 1), 0, 0); px.Apply(false);
                    sw.Restart();
                    for (int k = 0; k < 20; k++) perfCam.Render();
                    px.ReadPixels(new Rect(0, 0, 1, 1), 0, 0); px.Apply(false);
                    sw.Stop(); RenderTexture.active = prevA;
                    per.Add(sw.Elapsed.TotalMilliseconds / 20.0);
                }
                Destroy(px); per.Sort();
                if (per.Count > 0) Debug.Log(Tag + $"PERFRENDER {plan.prefix} {mode}: {per[per.Count / 2]:0.000} ms median per render ({per.Count} batches, p10 {per[per.Count / 10]:0.000}, p90 {per[per.Count * 9 / 10]:0.000}) at {perfTarget.width}x{perfTarget.height}");
            }
        }
        var cpu = new List<float>(); var gpu = new List<double>();
        var timings = new FrameTiming[1];
        if (passRecorders == null && SystemInfo.supportsGpuRecorder)
        {
            passRecorders = new List<(string, UnityEngine.Profiling.Recorder)>();
            var handles = new List<Unity.Profiling.LowLevel.Unsafe.ProfilerRecorderHandle>(); Unity.Profiling.LowLevel.Unsafe.ProfilerRecorderHandle.GetAvailable(handles);
            var names = new HashSet<string>();
            foreach (var h in handles)
            {
                var d = Unity.Profiling.LowLevel.Unsafe.ProfilerRecorderHandle.GetDescription(h);
                if (d.UnitType != Unity.Profiling.ProfilerMarkerDataUnit.TimeNanoseconds) continue;
                if (PassKeys.Any(k => d.Name.IndexOf(k, StringComparison.OrdinalIgnoreCase) >= 0)) names.Add(d.Name);
            }
            foreach (var n in names) { var r = UnityEngine.Profiling.Recorder.Get(n); if (r != null && r.isValid) passRecorders.Add((n, r)); }
        }
        var passSum = new Dictionary<string, double>(); int passFrames = 0;
        if (passRecorders != null) foreach (var (_, r) in passRecorders) r.enabled = true;
        float t0 = Time.realtimeSinceStartup;
        while (Time.realtimeSinceStartup - t0 < seconds)
        {
            yield return null;
            cpu.Add(Time.unscaledDeltaTime * 1000f);
            FrameTimingManager.CaptureFrameTimings();
            if (FrameTimingManager.GetLatestTimings(1, timings) > 0 && timings[0].gpuFrameTime > 0) gpu.Add(timings[0].gpuFrameTime);
            if (passRecorders != null)
            {
                passFrames++;
                foreach (var (n, r) in passRecorders) { double g = r.gpuElapsedNanoseconds / 1e6; if (g > 0) passSum[n] = (passSum.TryGetValue(n, out var s0) ? s0 : 0) + g; }
            }
        }
        if (passRecorders != null)
        {
            foreach (var (_, r) in passRecorders) r.enabled = false;
            var top = passSum.OrderByDescending(kv => kv.Value).Take(18).Select(kv => $"{kv.Key} {kv.Value / Math.Max(1, passFrames):0.000}");
            string pl = $"{plan.prefix} {mode}: GPU ms per frame by pass: " + string.Join("; ", top);
            perfLog.AppendLine(pl); Debug.Log(Tag + "PASSES " + pl);
        }
        string perfAt = perfTarget ? $" at {perfTarget.width}x{perfTarget.height}" : " (back buffer)";
        if (perfCam) perfCam.targetTexture = null;
        if (perfTarget) { perfTarget.Release(); Destroy(perfTarget); }
        cpu.Sort(); gpu.Sort();
        if (gpu.Count > 0) Debug.Log(Tag + $"PERFGPU {plan.prefix} {mode}: median {gpu[gpu.Count / 2]:0.000} ms, mean {gpu.Average():0.000} ms" + perfAt);
        string line = $"{plan.prefix} {mode}: {cpu.Count} frames, frame {cpu.Average():0.00} ms mean, {cpu[(int)(cpu.Count * 0.95f)]:0.00} ms p95"
                      + (gpu.Count > 0 ? $", GPU {gpu.Average():0.00} ms mean, {gpu[(int)(gpu.Count * 0.95f)]:0.00} ms p95" : ", GPU n/a")
                      + $" at {Screen.width}x{Screen.height}, cap {Application.targetFrameRate} vsync {QualitySettings.vSyncCount}, tier {ReflexEnhancedLighting.Tier}";
        perfLog.AppendLine(line);
        Debug.Log(Tag + "PERF " + line);
    }

    // -batchmode players have no window to capture: the game camera rendered into a target of REFLEX_SHOT_SIZE (default 1600x900) a few
    // times (the temporal AA's history settles), read back and saved (w-fresnolook)
    static void OffscreenShot(Camera cam, string file)
    {
        if (!cam) { Debug.Log(Tag + "OFFSCREEN no camera for " + file); return; }
        int W = 1600, H = 900;
        var sz = Environment.GetEnvironmentVariable("REFLEX_SHOT_SIZE");
        if (!string.IsNullOrEmpty(sz) && sz.Split('x') is var p && p.Length == 2) { int.TryParse(p[0], out W); int.TryParse(p[1], out H); }
        var rtx = RenderTexture.GetTemporary(W, H, 24, RenderTextureFormat.ARGB32);
        var prior = cam.targetTexture;
        try
        {
            cam.targetTexture = rtx;
            for (int i = 0; i < 8; i++) cam.Render();
            var prev = RenderTexture.active; RenderTexture.active = rtx;
            var t = new Texture2D(W, H, TextureFormat.RGB24, false);
            t.ReadPixels(new Rect(0, 0, W, H), 0, 0); t.Apply(false);
            RenderTexture.active = prev;
            if (ShotFlip)   // (offscreen renders come back upright: REFLEX_SHOT_FLIP=1 turns the rows)
            {   // (Metal / D3D: the target's rows come back top first)
                var px = t.GetPixels32(); var o = new Color32[px.Length];
                for (int y = 0; y < H; y++) Array.Copy(px, y * W, o, (H - 1 - y) * W, W);
                t.SetPixels32(o); t.Apply(false);
            }
            File.WriteAllBytes(file, t.EncodeToPNG()); Destroy(t);
        }
        catch (Exception e) { Debug.Log(Tag + "OFFSCREEN failed: " + e.Message); }
        finally { cam.targetTexture = prior; RenderTexture.ReleaseTemporary(rtx); }
    }

    // w-alivetrees (REFLEX_SHOTS_TAA=1): the offscreen shot through the temporal AA as a player sees it: one render a frame for 24 frames (the
    // jitter and the dithered foliage advance per frame; the world is paused), the last read back
    static IEnumerator OffscreenTaa(Camera cam, string file)
    {
        if (!cam) yield break;
        int W = 1600, H = 900;
        var sz = Environment.GetEnvironmentVariable("REFLEX_SHOT_SIZE");
        if (!string.IsNullOrEmpty(sz) && sz.Split('x') is var p && p.Length == 2) { int.TryParse(p[0], out W); int.TryParse(p[1], out H); }
        var rtx = RenderTexture.GetTemporary(W, H, 24, RenderTextureFormat.ARGB32);
        var prior = cam.targetTexture;
        ReflexTemporalAA.OffscreenTest = true;
        cam.targetTexture = rtx;
        for (int i = 0; i < 24; i++) { yield return null; cam.Render(); }
        cam.targetTexture = prior;
        ReflexTemporalAA.OffscreenTest = false;
        var prev = RenderTexture.active; RenderTexture.active = rtx;
        var t = new Texture2D(W, H, TextureFormat.RGB24, false);
        t.ReadPixels(new Rect(0, 0, W, H), 0, 0); t.Apply(false);
        RenderTexture.active = prev;
        if (ShotFlip)   // (offscreen renders come back upright: REFLEX_SHOT_FLIP=1 turns the rows)
        {
            var px = t.GetPixels32(); var o = new Color32[px.Length];
            for (int y = 0; y < H; y++) Array.Copy(px, y * W, o, (H - 1 - y) * W, W);
            t.SetPixels32(o); t.Apply(false);
        }
        File.WriteAllBytes(file, t.EncodeToPNG()); Destroy(t);
        RenderTexture.ReleaseTemporary(rtx);
        Debug.Log(Tag + "OFFSCREEN TAA " + (ReflexTemporalAA.Active ? ReflexTemporalAA.Active.report : "inactive"));
    }

    static Camera Cam(OriginalWorldRuntime rt) => rt && rt.chaseCamera ? rt.chaseCamera : Camera.main;
    static Vector3 BikePos(OriginalWorldRuntime rt) => rt && rt.chassis ? rt.chassis.position : Vector3.zero;
    static Vector3 BikeFwd(OriginalWorldRuntime rt)
    {
        var f = rt && rt.chassis ? rt.chassis.forward : Vector3.forward; f.y = 0;
        return f.sqrMagnitude > 1e-4f ? f.normalized : Vector3.forward;
    }

    static void HideHud(OriginalWorldRuntime rt)
    {
        Reflex.Port.Options.ReflexSettings.HudHidden = true;
        if (rt) { if (rt.OriginalHudVisible) rt.OriginalHudVisible = false; if (rt.HudVisible) rt.HudVisible = false; }
    }

    void LateUpdate()
    {
        if (active == null) return;
        var rt = FindAnyObjectByType<OriginalWorldRuntime>(); var level = FindAnyObjectByType<OriginalLevel>();
        var cam = Cam(rt); if (!cam || !level || !level.Loaded) return;
        Vector3 anchor = active.at != null && active.at.Length == 3 ? new Vector3(active.at[0], active.at[1], active.at[2]) : BikePos(rt);
        Vector3 fwd;
        var opps = active.frame == "opp" ? Opps(rt) : null;
        if (opps != null && oppIndex >= 0 && oppIndex < opps.Count)
        {
            anchor = opps[oppIndex].pos; fwd = opps[oppIndex].fwd; fwd.y = 0; fwd = fwd.sqrMagnitude > 1e-4f ? fwd.normalized : Vector3.forward;
        }
        else if (active.frame == "sun") { var s = level.Content.lighting.sunDirection; fwd = new Vector3(s.x, 0, s.z).normalized; }
        else if (active.frame == "world") fwd = Vector3.forward;
        else fwd = BikeFwd(rt);
        if (active.frame == "tree")
        {
            var fo = level.Content.forest;
            if (fo != null && fo.treeCount > 0)
            {
                if (treeOrder == null)
                {
                    var b = (active.treeAt != null && active.treeAt.Length == 2 ? new Vector3(active.treeAt[0], 0, active.treeAt[1]) : BikePos(rt)) / level.MapScale; var l = new List<(float d, int i)>();
                    for (int i = 0; i < fo.treeCount; i++)
                    {
                        var tt = fo.types[(int)fo.trees[i * 8 + 3]]; if (!tt.loaded || tt.records.Count == 0) continue;
                        if ((tt.boundsCenter.y + tt.boundsHalf.y * 0.8f) * fo.trees[i * 8 + 4] < active.treeMin) continue;
                        if (!string.IsNullOrEmpty(active.treeType) && (tt.treeAsset ?? "").IndexOf(active.treeType, StringComparison.OrdinalIgnoreCase) < 0) continue;
                        float dx = fo.trees[i * 8] - b.x, dz = fo.trees[i * 8 + 2] - b.z; l.Add((dx * dx + dz * dz, i));
                    }
                    l.Sort((x, y) => x.d.CompareTo(y.d)); treeOrder = l.Select(e => e.i).ToArray();
                }
                if (treeOrder.Length == 0) return;
                int ti = active.treeId >= 0 ? active.treeId : treeOrder[Mathf.Clamp(active.tree, 0, treeOrder.Length - 1)];
                if (treeLogged != active.name) { treeLogged = active.name; if (active.treeAt != null) for (int q = 0; q < Mathf.Min(12, treeOrder.Length); q++) { int qi = treeOrder[q]; var tq = fo.types[(int)fo.trees[qi * 8 + 3]]; Debug.Log(Tag + $"NEAR {q}: #{qi} {tq.treeAsset} at {fo.trees[qi * 8]:0.0},{fo.trees[qi * 8 + 1]:0.0},{fo.trees[qi * 8 + 2]:0.0} scale {fo.trees[qi * 8 + 4]:0.00} rot {fo.trees[qi * 8 + 5]:0.00} sin {fo.trees[qi * 8 + 6]:0.00} cos {fo.trees[qi * 8 + 7]:0.00}"); } var t0 = fo.types[(int)fo.trees[ti * 8 + 3]]; Debug.Log(Tag + $"TREE {active.name}: #{ti} {t0.treeAsset} leaf {t0.leafMaterial} branch {t0.branchMaterial} at {fo.trees[ti * 8]:0.0},{fo.trees[ti * 8 + 1]:0.0},{fo.trees[ti * 8 + 2]:0.0} scale {fo.trees[ti * 8 + 4]:0.00} rot {fo.trees[ti * 8 + 5]:0.00} bounds {t0.boundsCenter} {t0.boundsHalf} lods {t0.records.Count} maxdist {t0.MaxDistance:0}"); }
                float tms = level.MapScale;   // w-fb-vegetation: the forest's records are in the level's own feet; the world is drawn at MAP SCALE
                var tp = new Vector3(fo.trees[ti * 8], fo.trees[ti * 8 + 1], fo.trees[ti * 8 + 2]) * tms;
                var ty = fo.types[(int)fo.trees[ti * 8 + 3]];
                float crown = (ty.boundsCenter.y + ty.boundsHalf.y * 0.8f) * fo.trees[ti * 8 + 4] * tms;
                var toBike = (active.treeAt != null && active.treeAt.Length == 2 ? new Vector3(active.treeAt[0], 0, active.treeAt[1]) : BikePos(rt)) - tp; toBike.y = 0; if (toBike.sqrMagnitude < 1e-4f) toBike = Vector3.forward;
                if (active.orbit != 0f) toBike = Quaternion.Euler(0, active.orbit, 0) * toBike;
                anchor = tp + Vector3.up * crown; fwd = -toBike.normalized;
                var flat = tp + toBike.normalized * active.treeDist;
                var tr = level.terrain ? level.terrain.UnityTerrain : null;
                if (tr && !active.treeBase) flat.y = tr.SampleHeight(flat) + tr.transform.position.y; else flat.y = tp.y;
                var look0 = Quaternion.LookRotation(fwd, Vector3.up);
                Vector3 cp = flat + Vector3.up * active.lookY;
                Quaternion lr = active.look == "anchor" ? Quaternion.LookRotation(anchor - cp, Vector3.up) : look0 * Quaternion.Euler(-active.pitch, active.yaw, 0);
                cam.transform.SetPositionAndRotation(cp, lr);
                if (active.fov > 1) cam.fieldOfView = active.fov;
                return;
            }
        }
        if (active.cam == "path")
        {
            var pdir = Quaternion.Euler(0, active.yaw, 0) * Vector3.forward;
            float ph = active.bobPeriod > 0 ? 2f * Mathf.PI * pathFrame / active.bobPeriod : 0f;
            Vector3 pp = anchor + pdir * (active.step * pathFrame) + Vector3.up * (active.bobHeight * Mathf.Sin(ph));
            var prot = Quaternion.Euler(-(active.pitch + active.bobPitch * Mathf.Sin(ph)), active.yaw + active.yawStep * pathFrame, 0);
            cam.transform.SetPositionAndRotation(pp, prot);
            if (active.fov > 1) cam.fieldOfView = active.fov;
            return;
        }
        var rot = Quaternion.LookRotation(fwd, Vector3.up);
        Vector3 pos = anchor;
        if (active.offset != null && active.offset.Length == 3) pos += rot * new Vector3(active.offset[0], active.offset[1], active.offset[2]);
        var terrain = level.terrain ? level.terrain.UnityTerrain : null;
        if (terrain) { float g = terrain.SampleHeight(pos) + terrain.transform.position.y + 1.2f; if (pos.y < g) pos.y = g; }
        Quaternion look = active.look == "anchor"
            ? Quaternion.LookRotation((anchor + Vector3.up * active.lookY) - pos, Vector3.up)
            : rot * Quaternion.Euler(-active.pitch, active.yaw, 0);
        cam.transform.SetPositionAndRotation(pos, look);
        if (active.fov > 1) cam.fieldOfView = active.fov;
        if (active.near > 0) cam.nearClipPlane = active.near;   // w-camfix
    }

    // w-fb-models (REFLEX_LS_PROPS=1): every 2 s of real time the physics props the core moved, as the scenery draws them: each moved prop's
    // object against the terrain under it (its renderers' lowest / highest point), and the props hidden
    static IEnumerator PropLog(OriginalLevel level)
    {
        var props = level ? level.GetComponent<OriginalProps>() ?? FindAnyObjectByType<OriginalProps>() : null;
        var tr = level && level.terrain ? level.terrain.UnityTerrain : null;
        var seen = new HashSet<string>();
        for (int k = 0; k < 600; k++)
        {
            yield return new WaitForSecondsRealtime(2f);
            if (!props) { props = FindAnyObjectByType<OriginalProps>(); if (!props) continue; }
            Debug.Log(Tag + $"PROPS t {Time.time:F1}: scene {props.sceneProps}, core {props.gameProps}, matched {props.matched}, moved {props.moved}, updates {props.updatesLastFrame}");
            if (level.Content == null) continue;
            foreach (var inst in level.Content.instances)
            {
                if (!inst.transform || (inst.entityType != "PhysicsRSData" && inst.entityType != "PopUpBaleRSData" && inst.entityType != "PopDownBaleRSData")) continue;
                var rs = inst.transform.GetComponentsInChildren<Renderer>(); if (rs.Length == 0) continue;
                var b = rs[0].bounds; foreach (var r in rs) b.Encapsulate(r.bounds);
                float g = tr ? tr.SampleHeight(b.center) + tr.transform.position.y : float.NaN;
                bool hidden = props.IsHidden(inst.transform);
                var m = inst.matrix; float ms = level.MapScale;
                var p0 = new Vector3(m[12], m[13], m[14]) * ms;
                float d = (inst.transform.position - p0).magnitude;
                if (d < 0.05f && !hidden) continue;
                string key = inst.entityId + (hidden ? "h" : "") + Mathf.RoundToInt(d);
                if (!seen.Add(key) && k % 5 != 0) continue;
                Debug.Log(Tag + $"PROP {inst.entityId} {inst.name}: {d:F2} ft from its place, at {inst.transform.position:F2}, scale {inst.transform.lossyScale.x:F2}, drawn y {b.min.y:F2} .. {b.max.y:F2}, ground {g:F2}" +
                          (hidden ? " HIDDEN" : "") + (b.max.y < g ? " UNDER THE MAP" : b.min.y < g - 0.5f ? " sinks " + (g - b.min.y).ToString("F2") : ""));
            }
        }
    }

    void Fail(string why)
    {
        Debug.LogError(Tag + "FAIL " + why);
        Application.Quit(3);
    }
}

using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Text;
using Reflex.Port.Content;
using Reflex.Port.FrontEnd;
using Reflex.Port.OriginalWorld;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;

namespace Reflex.Port.Editor
{
    // WP-2 checks: the DLC tables merged as the game merges them, the data-driven event list, the scene section of every event
    // (the game's FUN_00555e60 names), the front end's DLC locations, the terrain shader variants, and a raced lap on an event.
    //   Verify: edit mode. EVENTS_REF=<events.txt of the sweep: "key spec" lines> compares the default events' specs; EVENTS_PROFILE=1
    //           also opens a profile session and checks the game's own IsLocked / IsStoreProductDLC answers for the DLC products.
    //   Race:   EVENTS_EVENT=National/NAT_DLC05_Dalton EVENTS_AI=0 EVENTS_TAG=dalton EVENTS_SHOTS=<dir>: plays the race scene with a
    //           race-line autopilot until a lap is completed, with screenshots and the level content / audio / ambient counts.
    // PlayerPrefs: nothing written (ReflexEventSelect.Pending); the user's keys are checked and restored.
    //   Unity -batchmode -projectPath <bench> -executeMethod Reflex.Port.Editor.EventsTests.Verify -logFile <log>
    public static class EventsTests
    {
        static readonly StringBuilder report = new StringBuilder();
        static int bad;
        static void Log(string s) { report.AppendLine(s); Debug.Log("[Events] " + s); }
        static void Check(bool ok, string what) { if (!ok) bad++; Log((ok ? "ok   " : "FAIL ") + what); }
        static string Env(string k, string d) { var v = Environment.GetEnvironmentVariable(k); return string.IsNullOrEmpty(v) ? d : v; }
        static string Shots() { string d = Env("EVENTS_SHOTS", Path.Combine(Path.GetTempPath(), "events-shots")); Directory.CreateDirectory(d); return d; }

        static readonly string[] UserKeys = { "ReflexEvent", "ReflexAI", "ReflexOpponents", "ReflexCameraMode" };
        static (string key, bool had, string s, int i, float f)[] saved;
        static void SavePrefs() => saved = UserKeys.Select(k => (k, PlayerPrefs.HasKey(k), PlayerPrefs.GetString(k, null), PlayerPrefs.GetInt(k, int.MinValue), PlayerPrefs.GetFloat(k, float.NaN))).ToArray();
        static void RestorePrefs()
        {
            bool changed = false;
            foreach (var (key, had, s, i, f) in saved)
            {
                bool now = PlayerPrefs.HasKey(key);
                if (now == had && (!had || PlayerPrefs.GetString(key, null) == s && PlayerPrefs.GetInt(key, int.MinValue) == i && PlayerPrefs.GetFloat(key, float.NaN).Equals(f))) continue;
                changed = true;
                if (!had) PlayerPrefs.DeleteKey(key);
                else if (!string.IsNullOrEmpty(s)) PlayerPrefs.SetString(key, s);
                else if (i != int.MinValue) PlayerPrefs.SetInt(key, i);
                else PlayerPrefs.SetFloat(key, f);
            }
            if (changed) PlayerPrefs.Save();
            Check(!changed, "PlayerPrefs of the user untouched");
        }

        static void Finish(string name)
        {
            RestorePrefs();
            Log(name + ": " + (bad == 0 ? "PASS" : bad + " failure(s)"));
            File.WriteAllText(Path.Combine(Shots(), name + "-report.txt"), report.ToString());
            if (Application.isBatchMode) EditorApplication.Exit(bad == 0 ? 0 : 1);
        }

        static int I(Dictionary<string, object> r, string c) => r != null && r.TryGetValue(c, out var v) && v != null ? Convert.ToInt32(v, CultureInfo.InvariantCulture) : -1;
        static string Spec(ReflexWorldFiles.EventSpec s) => string.Join(",", s.type, s.typeId, s.location, s.locationId, s.trackId, s.vehicleClass, s.durationType, s.duration, s.activity);

        // ---------------------------------------------------------------------------------------------------- verify
        public static void Verify()
        {
            report.Clear(); bad = 0; SavePrefs();
            try
            {
                // DLC discovery and the merged tables
                var dlc = ReflexEvents.DlcDatabases;
                Log($"DLC databases ({dlc.Count}): {string.Join(" ", dlc)}");
                Check(dlc.Contains("MXTables_DLC004") && dlc.Contains("MXTables_DLC005") && dlc.Contains("NAT_DLC05_Dalton") && dlc.Contains("SX_DLC02_FreightYardB"), "the glob *DLC*.dx9.database finds the DLC tables and levels");
                var t = ReflexEvents.Tables;
                var locs = t["EventLocation"].Select(r => I(r, "EventLocationId")).ToList();
                var etl = t["EventTypeLocations"].Select(r => I(r, "EventTypeLocationId")).ToList();
                Check(Enumerable.Range(98, 6).All(locs.Contains) && Enumerable.Range(285, 6).All(etl.Contains), "merged EventLocation 98-103 and EventTypeLocations 285-290");
                Check(t["EventTypeLocations"].Where(r => I(r, "EventTypeLocationId") >= 285).Select(ReflexEvents.SourceOf).Distinct().OrderBy(x => x).SequenceEqual(new[] { "MXTables_DLC004", "MXTables_DLC005" }), "the DLC rows come from MXTables_DLC004 / 005");
                Check(t["EventTypeLocations"].Count(r => !ReflexEvents.IsDlcRow(r)) == 64, "64 default EventTypeLocations rows");

                // the events
                var all = ReflexEvents.All;
                Log($"ReflexEvents.All: {all.Count} events");
                foreach (var e in all) Log($"  {e.key,-36} {Spec(e.spec)}  classes [{string.Join(",", e.vehicleClasses)}] atv {e.Atv} {e.tables} section {e.sceneSection}");
                var dlcKeys = new[] { "National/NAT_DLC05_Dalton", "National/NAT_DLC06_DaltonB", "Supercross/SX_DLC01_FreightYard", "Supercross/SX_DLC02_FreightYardB" };
                var raceTypes = new[] { "National", "Supercross", "WaypointRace", "FreeRide", "Terracross" };
                Check(all.Count(e => raceTypes.Contains(e.spec.type)) == 35 && all.Count(e => e.Dlc && raceTypes.Contains(e.spec.type)) == 4 && dlcKeys.All(k => ReflexEvents.Find(k)?.Dlc == true),
                      $"race types: 31 default + 4 DLC events (all runnable: {all.Count}; FunFactory with Freestyle: {all.Count(e => e.Dlc && e.spec.type == "Freestyle")})");
                var d = ReflexEvents.Find("National/NAT_DLC05_Dalton");
                Check(d != null && Spec(d.spec) == "National,10,NAT_DLC05_Dalton,98,0,3,Laps,2,0" && d.storeProductId == 3508 && d.eventTypeLocationId == 285, "Dalton spec " + (d == null ? "-" : Spec(d.spec)));
                var fy = ReflexEvents.Find("Supercross/SX_DLC02_FreightYardB");
                Check(fy != null && Spec(fy.spec) == "Supercross,8,SX_DLC02_FreightYardB,103,0,3,Laps,2,1" && fy.tables == "MXTables_DLC005", "FreightYardB spec " + (fy == null ? "-" : Spec(fy.spec)));
                string refFile = Env("EVENTS_REF", "");
                if (File.Exists(refFile))
                {
                    int n = 0;
                    foreach (var line in File.ReadAllLines(refFile).Where(l => l.Trim().Length > 0))
                    {
                        var p = line.Split(' '); var e = ReflexEvents.Find(p[0]); n++;
                        Check(e != null && !e.Dlc && Spec(e.spec) == p[1], $"default event {p[0]} spec unchanged ({(e == null ? "missing" : Spec(e.spec))})");
                    }
                    Check(n == 31 && all.Count(e => !e.Dlc && raceTypes.Contains(e.spec.type)) == n, $"the {n} reference events are exactly the default race-type events");
                }
                Check(ReflexEvents.ForLevel("NAT_Concord")?.key == "National/NAT_Concord" && ReflexEvents.ForLevel("FR_Alpine")?.key == "WaypointRace/FR_Alpine/1", "ForLevel unchanged");
                Check(all.Where(e => e.spec.type == "National" || e.spec.type == "Supercross").All(e => e.Atv && e.vehicleClasses.Take(3).SequenceEqual(new[] { 3, 2, 1 }) && e.vehicleClasses.Skip(3).All(c => ReflexEvents.ClassFormat(c) != "Single")), "every National / Supercross (DLC too) offers MX Lite, MX, ATV (then its Rival / Multi classes, WP-V2)");

                // scene sections (FUN_00555e60) exist where the game has them; every multi-track location loads its own
                foreach (var e in all)
                {
                    var names = ReflexEvents.SceneSectionsOf(e.location);
                    bool exhibition = e.sceneSection.Contains("Exhibition");
                    Check(exhibition ? !names.Contains(e.sceneSection) : names.Contains(e.sceneSection), $"{e.key}: section {e.sceneSection} {(exhibition ? "(race: Base only, as the game)" : "present")}");
                }
                foreach (var g in all.GroupBy(e => (e.spec.typeId, e.location)).Where(g => g.Count() > 1))
                    Check(g.Select(e => e.sceneSection).Distinct().Count() == g.Count(), $"{g.Key.location} type {g.Key.typeId}: tracks {string.Join(", ", g.Select(e => e.spec.trackId + " -> " + e.sceneSection))}");
                Check(ReflexEvents.Find("WaypointRace/FR_Alpine/1")?.SceneActivity == "WaypointRace1" && ReflexEvents.Find("WaypointRace/FR_Alpine")?.SceneActivity == "WaypointRace", "OriginalLevel.activity of the Alpine waypoint tracks: WaypointRace1 / WaypointRace");
                Check(ReflexEvents.SceneSection(ReflexEvents.ActivityIndex("HillClimb"), "FR_Alpine", 1) == "FR_AlpineChallengeHillClimbB" && ReflexEvents.SceneSection(ReflexEvents.ActivityIndex("Snake"), "FR_Alpine", 0) == "FR_AlpineMiniGameSnake"
                      && ReflexEvents.SceneSection(ReflexEvents.ActivityIndex("SectionRace"), "NAT_Concord", 0) == "NAT_ConcordSectionRace", "challenge / mini-game section names");
                // the section a track-1 waypoint race reads is its own (ReflexLevelContent)
                foreach (var key in new[] { "WaypointRace/FR_Alpine", "WaypointRace/FR_Alpine/1" })
                {
                    var e = ReflexEvents.Find(key);
                    var c = ReflexLevelContent.Load(e.location, new ReflexLevelContent.Options { activity = e.SceneActivity, scenery = false, terrain = false, tracks = false });
                    var secs = (List<string>)typeof(ReflexLevelContent).GetField("sceneSections", BindingFlags.NonPublic | BindingFlags.Instance).GetValue(c);
                    int ents = ((System.Collections.ICollection)typeof(ReflexLevelContent).GetField("entities", BindingFlags.NonPublic | BindingFlags.Instance).GetValue(c)).Count;
                    Check(secs.SequenceEqual(new[] { e.location + "Base", e.sceneSection }), $"{key}: sections read {string.Join(", ", secs)}; {ents} entities");
                }

                // the data-driven filter: a mode switched on lists its rows (WP-M / WP-V flip these)
                var savedActs = ReflexEvents.RunnableActivities.ToList(); var savedTypes = ReflexEvents.DrawableVehicleTypes.ToList(); int savedCount = ReflexEvents.All.Count;
                void Restore() { ReflexEvents.RunnableActivities.Clear(); ReflexEvents.RunnableActivities.UnionWith(savedActs); ReflexEvents.DrawableVehicleTypes.Clear(); ReflexEvents.DrawableVehicleTypes.AddRange(savedTypes); ReflexEvents.Refresh(); }
                ReflexEvents.RunnableActivities.Add("Freestyle"); ReflexEvents.Refresh();
                var fmx = ReflexEvents.All.Where(e => e.spec.type == "Freestyle").ToList();
                Log("with Freestyle runnable: " + string.Join(", ", fmx.Select(e => e.key + " [" + string.Join(",", e.vehicleClasses) + "] class " + e.spec.vehicleClass)));
                Check(fmx.Count == 8 && fmx.Count(e => e.Dlc) == 2 && fmx.All(e => e.vehicleClasses.Take(2).SequenceEqual(new[] { 25, 24 }) && e.Atv), "Freestyle runnable: 6 + 2 DLC (FunFactory) rows, classes MX Freestyle 25 / ATV Freestyle 24");
                Restore();
                ReflexEvents.DrawableVehicleTypes.AddRange(new[] { 11, 12, 14, 15 }); ReflexEvents.RunnableActivities.Remove("Powercross"); ReflexEvents.Refresh();   // WP-V runs it by default
                var px = ReflexEvents.All.Where(e => e.spec.typeId == 16).ToList();
                Check(px.Count == 0, "Powercross stays off while its activity is not runnable");
                ReflexEvents.RunnableActivities.Add("Powercross"); ReflexEvents.Refresh();
                px = ReflexEvents.All.Where(e => e.spec.typeId == 16).ToList();
                Log("with car types and Powercross: " + string.Join(", ", px.Select(e => e.key + " [" + string.Join(",", e.vehicleClasses) + "] class " + e.spec.vehicleClass)));
                Check(px.Count == 4, "car types drawable + Powercross runnable: the 4 Powercross rows");
                Restore();
                Check(ReflexEvents.All.Count == savedCount, "filter restored");

                // the front end's location lists: DLC rows with the DLC mark, names and icons
                var fe = FeTables.Shared;
                var assets = new FeAssets(Shader.Find("Reflex/FrontEnd UI")) { tables = fe };
                foreach (int type in new[] { 10, 8, 7 })
                {
                    var l = fe.Locations(type);
                    Log($"type {type}: " + string.Join(", ", l.Select(x => (x.locked ? "[locked] " : "") + (x.dlc ? "[DLC] " : "") + x.filename + " " + assets.Str(fe.TitleKey(x.storeProductId)))));
                    foreach (var x in l.Where(x => x.filename.Contains("DLC")))
                    {
                        string title = assets.Str(fe.TitleKey(x.storeProductId));
                        var poster = assets.Icon(x.posterIconId); var prev = assets.Icon(x.iconId); var map = assets.Icon(x.mapIconId);
                        bool runnable = ReflexEvents.All.Any(e => e.spec.typeId == type);
                        Check(x.dlc && (!runnable ? x.locked && x.ev == null : !x.locked && x.ev != null && x.ev.Dlc) && title.Length > 0 && !title.StartsWith("STR_") && poster.tex && prev.tex && map.tex,
                              $"  {x.filename}: dlc {x.dlc} locked {x.locked} -> {x.ev?.key ?? "-"}, title \"{title}\", poster {poster.tex?.width}x{poster.tex?.height} preview {prev.tex?.width} map {map.tex?.width}");
                    }
                }
                Check(fe.Locations(10).Count(x => !x.locked) == 10 && fe.Locations(8).Count(x => !x.locked) == 10, "National and Supercross: 8 + 2 unlocked locations each");

                // terrain shader variants by the material's shader asset
                var terrainShader = Shader.Find("Reflex/Original HFTerrain");
                foreach (var (lvl, asset, kw) in new[] { ("NAT_Concord", "HFTerrain_4d_cl_s_xtra", (string)null), ("TRX_Lynchburg", "HFTerrain_4d_cl_s", "_HFT_4D_CL_S"), ("SX_DLC01_FreightYard", "HFTerrain_sx_fy", "_HFT_SX_FY"), ("FMX_DLC03_FunFactory", "HFTerrain_fmx_ff", "_HFT_FMX_FF") })
                {
                    var c = ReflexLevelContent.Load(lvl, new ReflexLevelContent.Options { activity = "FreeRide", scenery = false, tracks = false });
                    var b = c.BuildTerrain(terrainShader);
                    var on = b?.material ? b.material.shaderKeywords : new string[0];
                    Check(b != null && b.shaderAsset == asset && b.shaderKeyword == kw && (kw == null ? on.Length == 0 : on.SequenceEqual(new[] { kw })),
                          $"{lvl}: terrain shader {b?.shaderAsset} -> keyword {b?.shaderKeyword ?? "(xtra)"}; material keywords [{string.Join(",", on)}] colour ref {b?.material?.GetVector("_ColorRef")}");
                    c.Dispose();
                }
                Check(ReflexLevelContent.TerrainVariantKeyword("HFTerrain_Basic") == null, "HFTerrain_Basic (Fresno, no event) keeps the xtra math");

                // the game's own answers in a profile session (signed in: Profile_ApplyDLCUnlocks)
                if (Env("EVENTS_PROFILE", "0") == "1")
                {
                    bool open = ReflexProfile.Open().Result;
                    var p = ReflexProfile.Current;
                    Check(open && p != null && p.signedIn, $"profile session open {open}, signed in {p?.signedIn} {ReflexProfile.Error}");
                    if (p != null)
                    {
                        foreach (int sp in new[] { 3508, 3509, 3510, 3592, 3593, 3594, 1555 })
                            Log($"  StoreProduct {sp}: in the game's answers {p.products.ContainsKey(sp)} locked {p.IsLocked(sp)} dlc {p.IsDlc(sp)} hidden {p.IsHidden(sp)} new {p.IsNew(sp)}");
                        Check(new[] { 3508, 3509, 3510, 3594 }.All(sp => p.products.ContainsKey(sp) && p.IsDlc(sp) && !p.IsLocked(sp)), "the game (signed in) answers the DLC race locations as DLC and unlocked");
                        Check(p.products.ContainsKey(1555) && !p.IsDlc(1555), "a default product is not DLC");
                        var l = fe.Locations(10).Where(x => x.filename.Contains("DLC")).ToList();
                        Check(l.Count == 2 && l.All(x => x.dlc && !x.locked), "LocationSelect with the game's answers: Dalton / DaltonB DLC, unlocked");
                    }
                    ReflexProfile.Close();
                }
            }
            catch (Exception e) { Check(false, "exception: " + e); }
            Finish("verify");
        }

        // ---------------------------------------------------------------------------------------------------- terrain variants
        // EVENTS_LEVELS=TRX_Lynchburg,SX_DLC01_FreightYard,... : each level in edit mode (OriginalLevel: scenery, sky, terrain material) on a
        // Unity terrain from its height map, rendered near / mid / high with its terrain variant and, for comparison, with the xtra math.
        public static void TerrainShots()
        {
            report.Clear(); bad = 0; SavePrefs(); string dir = Shots();
            foreach (var level in Env("EVENTS_LEVELS", "TRX_Lynchburg,SX_DLC01_FreightYard,SX_DLC02_FreightYardB,FMX_DLC03_FunFactory,NAT_DLC05_Dalton,NAT_DLC06_DaltonB").Split(','))
            {
                try
                {
                    EditorSceneManager.NewScene(NewSceneSetup.EmptyScene, NewSceneMode.Single);
                    var sun = new GameObject("Sun").AddComponent<Light>(); sun.type = LightType.Directional; sun.shadows = LightShadows.Soft; RenderSettings.sun = sun;
                    var otr = new GameObject("Original terrain").AddComponent<OriginalTerrain>();
                    var ol = new GameObject("Original level").AddComponent<OriginalLevel>(); ol.level = level; ol.terrain = otr;
                    ol.sceneryShader = Shader.Find("Reflex/Original Scenery"); ol.skyShader = Shader.Find("Reflex/Original Sky"); ol.terrainShader = Shader.Find("Reflex/Original HFTerrain"); ol.trackShader = Shader.Find("Reflex/Original HFT Tracks");
                    ol.LoadNow();
                    Check(ol.Loaded, $"{level}: loaded; terrain {ol.TerrainBuild?.shaderAsset} -> {ol.TerrainBuild?.shaderKeyword ?? "(xtra)"}");
                    if (!ol.Loaded) continue;
                    int spacing = ol.Content.TerrainSpacing; var h = ReflexLevelVerify.ReadHeights(ol.Content, out int n);
                    int res = 1; while (res < n) res *= 2; res += 1;
                    float lo = float.MaxValue, hi = float.MinValue; foreach (var v in h) { lo = Mathf.Min(lo, v * 2048f); hi = Mathf.Max(hi, v * 2048f); }
                    float yMin = lo - 2f, sizeY = hi - lo + 4f;
                    var data = new TerrainData { heightmapResolution = res }; data.size = new Vector3((res - 1) * spacing, sizeY, (res - 1) * spacing);
                    var grid = new float[res, res];
                    for (int j = 0; j < res; j++) { int r = Mathf.Clamp(n - 1 - j, 0, n - 1); for (int i = 0; i < res; i++) grid[j, i] = (h[r * n + Mathf.Min(i, n - 1)] * 2048f - yMin) / sizeY; }
                    data.SetHeights(0, 0, grid);
                    var terrain = Terrain.CreateTerrainGameObject(data).GetComponent<Terrain>();
                    float half = (n - 1) * spacing * 0.5f;
                    terrain.transform.position = new Vector3(-half, yMin, -half); terrain.drawInstanced = true;
                    var mat = new Material(otr.materialTemplate); mat.SetVector("_GridOrigin", new Vector4(-half, half, 2 * half, 0)); terrain.materialTemplate = mat;
                    float Y(float x, float z) => terrain.SampleHeight(new Vector3(x, 0, z)) + yMin;
                    var c0 = new Vector3(0, Y(0, 0), 0);
                    var views = new (string name, Vector3 pos, Vector3 look)[]
                    {
                        ("near", new Vector3(18, Y(18, 18) + 6, 18), new Vector3(-10, Y(-10, -10), -10)),
                        ("mid", new Vector3(140, Y(140, 140) + 45, 140), c0),
                        ("high", new Vector3(0, c0.y + 350, -half * 0.9f), c0),
                    };
                    var cam = new GameObject("cam").AddComponent<Camera>();
                    cam.fieldOfView = 55; cam.nearClipPlane = 0.15f; cam.farClipPlane = 14000; cam.clearFlags = CameraClearFlags.SolidColor; cam.backgroundColor = Color.black;
                    var rt = new RenderTexture(1600, 900, 24, RenderTextureFormat.ARGB32, RenderTextureReadWrite.sRGB); cam.targetTexture = rt;
                    string kw = ol.TerrainBuild?.shaderKeyword;
                    foreach (bool variant in kw != null ? new[] { true, false } : new[] { true })
                    {
                        if (kw != null) { if (variant) mat.EnableKeyword(kw); else mat.DisableKeyword(kw); }
                        for (int w = 0; w < 4; w++) { cam.transform.position = views[0].pos; cam.transform.LookAt(views[0].look); cam.Render(); System.Threading.Thread.Sleep(300); }
                        foreach (var v in views)
                        {
                            if (!variant && v.name == "high") continue;
                            cam.transform.position = v.pos; cam.transform.LookAt(v.look); cam.Render();
                            RenderTexture.active = rt; var tex = new Texture2D(1600, 900, TextureFormat.RGB24, false); tex.ReadPixels(new Rect(0, 0, 1600, 900), 0, 0); tex.Apply(); RenderTexture.active = null;
                            string file = $"terrain_{level}_{v.name}{(kw == null ? "" : variant ? "_" + kw.Trim('_').ToLowerInvariant() : "_as_xtra")}.png";
                            File.WriteAllBytes(Path.Combine(dir, file), tex.EncodeToPNG()); UnityEngine.Object.DestroyImmediate(tex);
                            Log("shot " + file);
                        }
                    }
                    ol.Unload();
                }
                catch (Exception e) { Check(false, level + ": " + e); }
            }
            Finish("terrain");
        }

        // ---------------------------------------------------------------------------------------------------- race
        static string tag, shotDir; static int stage, ai; static double stageAt, runningAt, raceAt, lastLog; static float maxSpeed;
        static int lastLaps; static double lapAt;

        public static void Race()
        {
            report.Clear(); bad = 0; SavePrefs(); shotDir = Shots();
            string key = Env("EVENTS_EVENT", "National/NAT_DLC05_Dalton"); ai = int.Parse(Env("EVENTS_AI", "0"), CultureInfo.InvariantCulture);
            tag = Env("EVENTS_TAG", key.Replace('/', '_'));
            var ev = ReflexEvents.Find(key);
            if (ev == null) { Check(false, "no event " + key); Finish("race-" + tag); return; }
            Log($"race {key} ({ev.tables}, section {ev.sceneSection}) AI {ai}");
            ReflexEventSelect.Pending = key; ReflexEventSelect.PendingAi = ai; ReflexEventSelect.PendingGarage = null;
            EditorSceneManager.OpenScene("Assets/Scenes/Reflex.unity");
            stage = 0; stageAt = EditorApplication.timeSinceStartup; runningAt = raceAt = lapAt = -1; maxSpeed = 0; lastLaps = 0;
            EditorApplication.update += Step;
            EditorApplication.EnterPlaymode();
        }

        static int[] Steer(OriginalWorldRuntime rt)
        {
            var st = (ReflexBikeState)typeof(OriginalWorldRuntime).GetField("state", BindingFlags.NonPublic | BindingFlags.Instance).GetValue(rt);
            var onboard = rt.onboard;
            if (onboard == null || st.position == null) return new[] { 0x11 };
            var line = (float[])onboard.GetType().GetField("raceLine", BindingFlags.NonPublic | BindingFlags.Instance)?.GetValue(onboard);
            int n = (int)(onboard.GetType().GetField("raceLinePoints", BindingFlags.NonPublic | BindingFlags.Instance)?.GetValue(onboard) ?? 0);
            if (line == null || n < 2) return new[] { 0x11 };
            var pos = new Vector3(st.position[0], 0, st.position[2]); var vel = new Vector3(st.velocity[0], 0, st.velocity[2]);
            float ahead = float.Parse(Env("EVENTS_AHEAD", "45"), CultureInfo.InvariantCulture);
            Vector3 target = new Vector3(line[3 * (n - 1)], 0, line[3 * (n - 1) + 2]);
            for (int i = 0; i < n; i++) { var p = new Vector3(line[3 * i], 0, line[3 * i + 2]); if ((p - pos).magnitude > ahead) { target = p; break; } }
            var d = target - pos;
            if (vel.magnitude < 3f) return new[] { 0x11 };
            float cross = vel.x * d.z - vel.z * d.x;
            int k = Vector3.Angle(vel, d) < 4f ? 0 : (cross > 0 ? 0x1e : 0x20);
            return k == 0 ? new[] { 0x11 } : new[] { 0x11, k };
        }

        static void Shot(OriginalWorldRuntime rt, string name)
        {
            var c = new GameObject("shot cam").AddComponent<Camera>(); c.enabled = false;
            var target = new RenderTexture(1920, 1080, 24) { antiAliasing = 4 };
            var main = rt && rt.chaseCamera ? rt.chaseCamera : Camera.main;
            if (main) c.CopyFrom(main);
            c.targetTexture = target; c.enabled = false;
            var overlays = UnityEngine.Object.FindObjectsByType<Canvas>(FindObjectsSortMode.None).Where(x => x.isRootCanvas && x.renderMode == RenderMode.ScreenSpaceOverlay && x.enabled && x.gameObject.activeInHierarchy).OrderBy(x => x.sortingOrder).ToList();
            foreach (var o in overlays) { o.renderMode = RenderMode.ScreenSpaceCamera; o.worldCamera = c; o.planeDistance = Mathf.Max(c.nearClipPlane + 0.01f, 1f - o.sortingOrder * 0.0005f); }
            Canvas.ForceUpdateCanvases(); c.Render();
            var prev = RenderTexture.active; RenderTexture.active = target;
            var tex = new Texture2D(1920, 1080, TextureFormat.RGB24, false); tex.ReadPixels(new Rect(0, 0, 1920, 1080), 0, 0); tex.Apply(); RenderTexture.active = prev;
            File.WriteAllBytes(Path.Combine(shotDir, tag + "_" + name + ".png"), tex.EncodeToPNG());
            foreach (var o in overlays) o.renderMode = RenderMode.ScreenSpaceOverlay;
            UnityEngine.Object.DestroyImmediate(tex); UnityEngine.Object.DestroyImmediate(c.gameObject); target.Release();
            Log("shot " + tag + "_" + name);
        }

        static ReflexRaceState RaceState(OriginalWorldRuntime rt) => (ReflexRaceState)typeof(OriginalWorldRuntime).GetField("race", BindingFlags.NonPublic | BindingFlags.Instance).GetValue(rt);
        static void Next() { stage++; stageAt = EditorApplication.timeSinceStartup; }
        static double Age => EditorApplication.timeSinceStartup - stageAt;
        static void Fail(string why) { Check(false, why); stage = 100; EditorApplication.ExitPlaymode(); }

        static void Content(OriginalWorldRuntime rt)
        {
            var ol = UnityEngine.Object.FindAnyObjectByType<OriginalLevel>();
            Check(ol && ol.Loaded, $"level {ol?.level} loaded ({ol?.loadSeconds:0.00} s), activity section '{ol?.activity}'");
            if (ol && ol.Loaded)
            {
                Log("level report: " + ol.report);
                var b = ol.TerrainBuild;
                Check(b != null && b.material, $"terrain material {b?.material?.name}: shader {b?.shaderAsset} keyword {b?.shaderKeyword ?? "(xtra)"}");
                var renderers = ol.SceneryRoot ? ol.SceneryRoot.GetComponentsInChildren<Renderer>(true) : new Renderer[0];
                int sky = renderers.Count(r => r.sharedMaterial && r.sharedMaterial.shader && r.sharedMaterial.shader.name.Contains("Sky"));
                Check(ol.Content.instances.Count > 0 && renderers.Length > 0, $"scenery: {ol.Content.instances.Count} instances, {renderers.Length} renderers, sky renderers {sky}");
            }
            var terr = UnityEngine.Object.FindAnyObjectByType<Terrain>();
            Check(terr && terr.materialTemplate, $"terrain {terr?.name}: material {terr?.materialTemplate?.name} keywords [{string.Join(",", terr?.materialTemplate ? terr.materialTemplate.shaderKeywords : new string[0])}]");
            var amb = UnityEngine.Object.FindAnyObjectByType<OriginalAmbient>();
            Log($"ambient: entities {amb?.entities} drawn {amb?.drawn} instances {amb?.instancesDrawn} models {amb?.models} model errors {amb?.modelErrors}");
            var par = UnityEngine.Object.FindAnyObjectByType<OriginalParticles>();
            Log($"particles: normal {par?.Normal} additive {par?.Additive}");
            var au = UnityEngine.Object.FindAnyObjectByType<OriginalAudio>();
            Check(au && au.ClipsLoaded > 0, $"level audio {au?.level}: clips {au?.ClipsLoaded}/{au?.ClipsTotal}, active events {au?.ActiveEvents}, voices {au?.ActiveVoices}");
            var mu = UnityEngine.Object.FindAnyObjectByType<OriginalMusic>();
            Log($"music: {mu?.TrackCount} tracks, now '{mu?.NowPlaying}' {mu?.LoadError}");
        }

        static void Step()
        {
            try
            {
                if (!EditorApplication.isPlaying) { if (stage >= 100) { EditorApplication.update -= Step; Finish("race-" + tag); } else if (Age > 120 && stage == 0) { Check(false, "play mode never started"); EditorApplication.update -= Step; Finish("race-" + tag); } return; }
                var rt = UnityEngine.Object.FindAnyObjectByType<OriginalWorldRuntime>();
                double now = EditorApplication.timeSinceStartup;
                if (rt && rt.Running && runningAt < 0) runningAt = now;
                var race = rt && rt.Running ? RaceState(rt) : default;
                if (rt && raceAt < 0 && race.valid != 0 && race.state >= 2) raceAt = now;
                if (rt) rt.testHeldKeys = raceAt >= 0 ? Steer(rt) : Array.Empty<int>();
                if (rt && raceAt >= 0 && rt.TryGetVehicle(out var cur)) maxSpeed = Mathf.Max(maxSpeed, cur.speed);
                if (rt && rt.Running && now - lastLog > 5) { lastLog = now; Log($"t {now - runningAt:0} race {race.state} lap {race.laps_completed}/{race.num_laps} prog {race.lap_progress:0.000} place {race.place}/{race.riders} lap time {race.lap_time:0.0} speed max {maxSpeed:0.0} sim {rt.SimMilliseconds:0.00} ms"); }
                switch (stage)
                {
                    case 0:
                        if (rt && rt.Running && now - runningAt > 5) { Content(rt); Shot(rt, "01_gate"); Next(); }
                        else if (Age > 480) Fail("world not running after 480 s");
                        break;
                    case 1: if (raceAt >= 0 && now - raceAt > 3) { Shot(rt, "02_race_3s"); Next(); } else if (Age > 240) Fail("the gate did not drop"); break;
                    case 2: if (now - raceAt > 12) { Shot(rt, "03_race_12s"); Next(); } break;
                    case 3: if (now - raceAt > 30) { Shot(rt, "04_race_30s"); Next(); } break;
                    case 4:
                        if (race.laps_completed >= 1)
                        {
                            Check(true, $"lap 1 completed: last lap {race.last_lap:0.00} s, race time {race.race_time:0.0} s, place {race.place}/{race.riders}, max speed {maxSpeed:0.0}");
                            Shot(rt, "05_lap1"); Next();
                        }
                        else if (Age > 420) Fail($"no lap completed in 420 s (lap progress {race.lap_progress:0.000})");
                        break;
                    case 5: if (Age > 2) { Content(rt); stage = 100; EditorApplication.ExitPlaymode(); } break;
                }
            }
            catch (Exception e) { Fail("exception: " + e); }
        }
    }
}

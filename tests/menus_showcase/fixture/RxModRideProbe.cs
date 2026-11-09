// TEST ONLY (tests/menus_showcase fixtures, run with tests/rxmod/rxmod_ride.py): a .rxmod bike (core/plan/RXMOD.md) ridden in a race with AI.
// With REFLEX_RXMOD_PROBE=1 the player launches REFLEX_RXMOD_EVENT (default National/NAT_Concord) with REFLEX_RXMOD_AI computer riders (5) and
// REFLEX_RXMOD_LAPS laps (1) on a pro's bike of VehicleModelId REFLEX_RXMOD_MODEL (the scratch save chooses the mod for it: BikeModel.Model.<id>),
// skips the intro with a virtual pad, rides the race line with GhostRideProbe's autopilot to the finish and:
//   1. checks the local bike is the .rxmod bike (ReflexBikeModel.rxFit) and logs its fit, parts and the original it rides as
//   2. takes shots: the chase camera on the gate, riding, close-ups of the bike from the right / left / front / rear-right (wheels on the ground,
//      the forks and swingarm against the frame), in the air when it jumps, at the finish
//   3. logs the suspension's travel the pose gave the mod's parts (the front wheel along the forks, the rear wheel's height under the frame), min .. max
// Result lines "[RxModRide] ..." with "[RxModRide] PASS" / "FAIL"; the player quits at the end.
using System;
using System.Collections;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using Reflex.Port.Content;
using Reflex.Port.OpenFrontEnd;
using Reflex.Port.OriginalWorld;
using UnityEngine;
using UnityEngine.InputSystem;
using UnityEngine.InputSystem.LowLevel;

namespace Reflex.Port.Showcase
{
    public sealed class RxModRideProbe : MonoBehaviour
    {
        static readonly CultureInfo Inv = CultureInfo.InvariantCulture;
        string shots; Gamepad pad; OriginalWorldRuntime rt; int fails;
        Vector3 bikePos, bikeFwd; float speed; readonly float[] line = new float[3 * 64]; int points; bool have, drive;
        float stuck, reverse; OriginalSimulation.Results last; int airFrames, airShots;
        bool overrideCam; Vector3 camOffset; float camFov = 40f; string pendingShot;
        float frontMin = float.MaxValue, frontMax = float.MinValue, rearMin = float.MaxValue, rearMax = float.MinValue;

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        static void Guest() { if (Environment.GetEnvironmentVariable("REFLEX_RXMOD_PROBE") == "1") ReflexProfile.Disabled = true; }

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
        static void Boot()
        {
            if (Environment.GetEnvironmentVariable("REFLEX_RXMOD_PROBE") != "1" || FindAnyObjectByType<RxModRideProbe>()) return;
            var g = new GameObject("rxmod ride probe (test only)"); DontDestroyOnLoad(g); g.AddComponent<RxModRideProbe>();
        }

        void Start()
        {
            shots = Environment.GetEnvironmentVariable("REFLEX_PREVIEW_SHOTS");
            if (!string.IsNullOrEmpty(shots)) Directory.CreateDirectory(shots);
            Application.runInBackground = true;
            InputSystem.settings.backgroundBehavior = InputSettings.BackgroundBehavior.IgnoreFocus;
            OriginalWorldRuntime.ViewApplied += OnView;
            StartCoroutine(Run());
        }
        void OnDestroy() { OriginalWorldRuntime.ViewApplied -= OnView; }

        static void Log(string s) => Debug.Log("[RxModRide] " + s);
        void Fail(string s) { fails++; Debug.LogError("[RxModRide] FAIL " + s); }
        static string F(float v) => v.ToString("0.000", Inv);
        IEnumerator Frames(int n) { for (int i = 0; i < n; i++) yield return null; }
        IEnumerator Seconds(float s) { float t0 = Time.realtimeSinceStartup; while (Time.realtimeSinceStartup - t0 < s) yield return null; }
        static int Env(string k, int d) => int.TryParse(Environment.GetEnvironmentVariable(k), NumberStyles.Integer, Inv, out int v) ? v : d;

        void OnView(OriginalWorldRuntime world, OriginalSimulation.Results v)
        {
            if (v.bikePose == null || v.bikeNodes < 1) return;
            last = v;
            bikePos = new Vector3(v.bikePose[12], v.bikePose[13], v.bikePose[14]); bikeFwd = new Vector3(v.bikePose[8], v.bikePose[9], v.bikePose[10]);
            speed = v.bike.speed; points = Math.Min(64, v.raceLinePoints); Array.Copy(v.raceLine, line, points * 3); have = true;
            // the suspension as the pose moves the mod's wheels: the front wheel along the forks from the steering head, the rear wheel below the frame
            var bm = rt ? rt.BikeModel : null;
            if (bm != null && bm.hierarchyNodes != null && drive)
            {
                int f = Array.IndexOf(bm.hierarchyNodes, "OBJ_FRT_Tire"), r = Array.IndexOf(bm.hierarchyNodes, "OBJ_BCK_Tire"), u = Array.IndexOf(bm.hierarchyNodes, "OBJ_UPR_Forks");
                if (f < 0)
                {   // an ATV (ATV_Hierarchy): the front right wheel from its upper A-arm's pivot, the rear axle below the frame
                    f = Array.IndexOf(bm.hierarchyNodes, "SWP_FRT_RHT_Tire"); r = Array.IndexOf(bm.hierarchyNodes, "OBJ_Axle"); u = Array.IndexOf(bm.hierarchyNodes, "OBJ_FRT_RHT_UPR_AArm");
                }
                if (f >= 0 && r >= 0 && u >= 0 && v.bikeNodes > Math.Max(f, Math.Max(r, u)))
                {
                    var frame = OriginalBikePose.FromRows(v.bikePose, 0); var inv = frame.inverse;
                    Vector3 P(int n) => inv.MultiplyPoint3x4(OriginalBikePose.FromRows(v.bikePose, n * 16).GetColumn(3));
                    float fl = (P(f) - P(u)).magnitude, rh = P(r).y;
                    frontMin = Mathf.Min(frontMin, fl); frontMax = Mathf.Max(frontMax, fl); rearMin = Mathf.Min(rearMin, rh); rearMax = Mathf.Max(rearMax, rh);
                }
            }
            bool air = v.onboardState.valid != 0 && v.onboardState.front_grounded == 0 && v.onboardState.rear_grounded == 0;
            airFrames = air ? airFrames + 1 : 0;
            if (drive && airFrames == 20 && airShots < 2) { airShots++; StartCoroutine(Shot("air-" + airShots)); }
        }

        IEnumerator Run()
        {
            pad = InputSystem.AddDevice<Gamepad>("rxmod probe pad");
            OpenShell shell = null;
            for (float t = 0; t < 120f && !(shell = OpenShell.Instance); t += Time.unscaledDeltaTime) yield return null;
            if (!shell) { Fail("the shell did not start"); Quit(); yield break; }
            yield return Seconds(2f);
            int model = Env("REFLEX_RXMOD_MODEL", -1);
            var rxMods = BikeMods.All.Where(m => m.rx != null).ToList();
            Log("installed .rxmod mods: " + string.Join(", ", rxMods.Select(m => m.id + " (" + m.rx.hash + ")")) + "; choice for model " + model + ": \"" + BikeMods.ChoiceFor(model) + "\"");
            var chosen = BikeMods.ChosenFor(model);
            var skin = chosen?.rx?.skins.FirstOrDefault(k => k.For(model));   // (a skin mod: its bike, painted)
            var bike = skin?.target ?? chosen?.rx?.bikes.FirstOrDefault(b => b.vehicleModelId == model);
            if (Environment.GetEnvironmentVariable("REFLEX_RXMOD_EXPECT") != "none" && bike == null && skin == null) { Fail("no .rxmod bike or skin chosen for model " + model); }
            // a pro's vehicle of the model (AIRiderVehicle -> AIVehicle.VehicleModelId), his default gear
            var T = ReflexEvents.Tables;
            int I(Dictionary<string, object> r, string k) { try { return r != null && r.TryGetValue(k, out var o) ? Convert.ToInt32(o, Inv) : -1; } catch { return -1; } }
            ReflexWorldFiles.Garage garage = null;
            string evName = Environment.GetEnvironmentVariable("REFLEX_RXMOD_EVENT") ?? "National/NAT_Concord";
            var ev = ReflexEvents.Find(evName) ?? ReflexEvents.All.First(e => e.spec.type == "National" && e.location == "NAT_Concord");
            foreach (var arv in T["AIRiderVehicle"])
            {
                var av = T["AIVehicle"].FirstOrDefault(x => I(x, "AIVehicleId") == I(arv, "AIVehicleId"));
                if (I(av, "VehicleModelId") != model) continue;
                int riderId = I(arv, "AIRiderId"), gear = I(T["AIRider"].FirstOrDefault(x => I(x, "AIRiderId") == riderId), "DefaultRiderGearSetId");
                int type = I(T["VehicleModel"].FirstOrDefault(x => I(x, "VehicleModelId") == model), "VehicleTypeId");
                garage = new ReflexWorldFiles.Garage { riderOwner = ReflexWorldFiles.Garage.AI, riderId = riderId, riderGearSet = gear, vehicleOwner = ReflexWorldFiles.Garage.AI, vehicleId = I(av, "AIVehicleId"), vehicleTypeId = type, vehicleClass = -1 };
                var g2 = type >= 0 ? ev.GarageFor(type) : null; if (g2 != null) garage.vehicleClass = g2.vehicleClass;
                break;
            }
            if (garage == null) { Fail("no pro rides model " + model); Quit(); yield break; }
            int ai = Env("REFLEX_RXMOD_AI", 5), laps = Env("REFLEX_RXMOD_LAPS", 1);
            Log($"launching {ev.key} with {ai} AI, {laps} lap(s), garage {garage}");
            ReflexEventSelect.Launch(ev.key, ai, false, "Reflex", laps, garage);
            for (float t = 0; t < 400f; t += Time.unscaledDeltaTime)
            {
                rt = FindAnyObjectByType<OriginalWorldRuntime>();
                if (rt && rt.FailureMessage != null) { Fail("world: " + rt.FailureMessage); Quit(); yield break; }
                if (rt && rt.Running && (rt.IntroCardShown || rt.EventUi.race_state >= 1)) break;
                yield return null;
            }
            if (!(rt && rt.Running)) { Fail("the world does not run"); Quit(); yield break; }
            if (rt.IntroCardShown) { yield return Seconds(1f); yield return Tap(GamepadButton.South); }
            for (float t = 0; t < 30f && !(rt.BikeModel != null); t += Time.unscaledDeltaTime) yield return null;
            var bm = rt.BikeModel;
            Log($"the local bike: {(bm?.chassis ? bm.chassis.name : "none")}, {bm?.parts.Count} parts, {bm?.bones.Count} skin bones; rx fit: {bm?.rxFit ?? "(not a .rxmod bike)"}; riders {rt.Roster?.entries.Count}");
            if (bike != null && (bm == null || bm.rxFit == null)) Fail("the race's bike is not the .rxmod bike");
            if (skin != null && rt.Roster?.Local?.bike != null && BikeMods.ForPlayer(rt.Roster.Local.bike)?.rxSkin != skin) Fail("the race's bike is not painted by the chosen skin");
            if (bike == null && Environment.GetEnvironmentVariable("REFLEX_RXMOD_EXPECT") == "none" && bm?.rxFit != null) Fail("a .rxmod bike was drawn without one chosen");
            yield return Seconds(1f);
            rt.SetCameraMode(2, false);
            yield return Shot("gate-chase");
            yield return CloseUps("gate");
            for (float t = 0; t < 60f && !(rt.Race.state == 2 || rt.Race.valid == 0); t += Time.unscaledDeltaTime) yield return null;
            drive = true;
            yield return Seconds(6f);
            yield return Shot("riding-chase");
            yield return CloseUps("riding");
            rt.SetCameraMode(1, false); yield return Seconds(1.5f); yield return Shot("riding-onboard"); rt.SetCameraMode(2, false);
            int timeout = Env("REFLEX_RXMOD_TIMEOUT", 480);
            float start = Time.realtimeSinceStartup; bool finished = false; int lastLap = -1;
            while (Time.realtimeSinceStartup - start < timeout)
            {
                var race = rt.Race;
                if (race.laps_completed != lastLap) { lastLap = race.laps_completed; Log($"lap {race.laps_completed}/{race.num_laps} at {Time.realtimeSinceStartup - start:0} s, position {race.place}"); }
                if (race.num_laps > 0 && race.laps_completed >= race.num_laps) { finished = true; break; }
                if (rt.FailureMessage != null) { Fail("world: " + rt.FailureMessage); break; }
                yield return Seconds(0.5f);
            }
            drive = false; InputSystem.QueueStateEvent(pad, new GamepadState());
            Log($"suspension (the pose of the mod's parts): front wheel {F(frontMin)} .. {F(frontMax)} ft from the steering head ({F((frontMax - frontMin) * 12f)} in of travel), rear axle {F(rearMin)} .. {F(rearMax)} ft under the frame origin ({F((rearMax - rearMin) * 12f)} in)");
            if (frontMax - frontMin < 0.05f || rearMax - rearMin < 0.05f) Fail("the suspension did not move the mod's wheels");
            if (finished) { Log($"FINISHED {rt.Race.laps_completed}/{rt.Race.num_laps} laps, position {rt.Race.place}, time {rt.Race.race_time:0.00}"); yield return Seconds(2f); yield return Shot("finish"); }
            else Fail($"not finished in {timeout} s (laps {rt.Race.laps_completed}/{rt.Race.num_laps})");
            Log(fails == 0 ? "PASS" : "FAIL " + fails + " check(s)");
            yield return Frames(30);
            Quit();
        }

        IEnumerator CloseUps(string when)
        {
            overrideCam = true;
            foreach (var (name, off, fov) in new[] { ("right", new Vector3(5.5f, 1.2f, 0.2f), 38f), ("left", new Vector3(-5.5f, 1.2f, 0.2f), 38f),
                                                     ("front", new Vector3(1.5f, 1.5f, 5.0f), 40f), ("rear-right", new Vector3(3.2f, 1.8f, -3.6f), 40f) })
            {
                if (when == "gate" && name == "left") continue;   // (on the gate the left side is the next rider's)
                camOffset = off; camFov = fov; yield return Frames(3); yield return Shot(when + "-" + name);
            }
            overrideCam = false;
        }

        void Update()
        {
            if (pad == null || !pad.added || !drive || !have || points < 2) return;
            int nearest = 0; float best = float.MaxValue;
            for (int i = 0; i < points && i < 6; i++) { float dx = line[3 * i] - bikePos.x, dz = line[3 * i + 2] - bikePos.z, d = dx * dx + dz * dz; if (d < best) { best = d; nearest = i; } }
            float ahead = 18f + speed * 0.6f; int target = nearest;
            for (int i = nearest; i < points; i++) { float dx = line[3 * i] - bikePos.x, dz = line[3 * i + 2] - bikePos.z; target = i; if (dx * dx + dz * dz >= ahead * ahead) break; }
            float err = Mathf.DeltaAngle(Mathf.Atan2(bikeFwd.x, bikeFwd.z) * Mathf.Rad2Deg, Mathf.Atan2(line[3 * target] - bikePos.x, line[3 * target + 2] - bikePos.z) * Mathf.Rad2Deg) * Mathf.Deg2Rad;
            float steer = Mathf.Clamp(err * 2.2f, -1f, 1f), turn = Mathf.Abs(err);
            float desired = turn > 1.2f ? 8f : turn > 0.6f ? 14f : turn > 0.3f ? 20f : 30f;
            float dt = Time.unscaledDeltaTime;
            if (speed < 1f) stuck += dt; else stuck = 0;
            if (stuck > 4f) { reverse = 1.5f; stuck = 0; }
            var s = new GamepadState();
            if (reverse > 0) { reverse -= dt; s.leftTrigger = 1f; s.leftStick = new Vector2(-steer, 0); }
            else { s.leftStick = new Vector2(steer, 0); if (speed < desired) s.rightTrigger = 1f; else if (speed > desired + 6f) s.leftTrigger = 0.63f; else s.rightTrigger = 0.47f; }
            InputSystem.QueueStateEvent(pad, s);
        }

        void LateUpdate()
        {
            if (overrideCam && rt && rt.chaseCamera && rt.chassis)
            {
                var cam = rt.chaseCamera; var aim = rt.chassis.position + rt.chassis.up * 0.6f;
                cam.transform.position = aim + rt.chassis.rotation * camOffset;
                cam.transform.rotation = Quaternion.LookRotation(aim - cam.transform.position, Vector3.up);
                cam.fieldOfView = camFov; cam.nearClipPlane = 0.05f;
            }
            if (pendingShot != null) { OffscreenShot(rt ? rt.chaseCamera : Camera.main, pendingShot); pendingShot = null; }
        }

        static void OffscreenShot(Camera cam, string file)
        {
            if (!cam) { Log("OFFSCREEN no camera for " + file); return; }
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
                File.WriteAllBytes(file, t.EncodeToPNG()); Destroy(t);
            }
            catch (Exception e) { Log("OFFSCREEN failed: " + e.Message); }
            finally { cam.targetTexture = prior; RenderTexture.ReleaseTemporary(rtx); }
        }

        IEnumerator Shot(string name)
        {
            if (Application.isBatchMode)
            {
                if (!string.IsNullOrEmpty(shots)) { pendingShot = Path.Combine(shots, name + ".png"); while (pendingShot != null) yield return null; }
                Log("shot " + name);
                yield break;
            }
            yield return new WaitForEndOfFrame();
            if (!string.IsNullOrEmpty(shots)) ScreenCapture.CaptureScreenshot(Path.Combine(shots, name + ".png"));
            yield return Frames(2);
            Log("shot " + name);
        }
        IEnumerator Tap(GamepadButton b)
        {
            InputSystem.QueueStateEvent(pad, new GamepadState(b)); yield return Frames(3);
            InputSystem.QueueStateEvent(pad, new GamepadState()); yield return Frames(4);
        }

        static void Quit()
        {
#if UNITY_EDITOR
            UnityEditor.EditorApplication.isPlaying = false;
#else
            Application.Quit();
#endif
        }
    }
}

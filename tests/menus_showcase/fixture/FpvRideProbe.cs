// TEST ONLY (w-fpvrewrite; tests/menus_showcase fixtures, run with tests/fpv_camera/fpv_ride.py): the Dynamic FPV camera in a real ride.
// With REFLEX_FPV_PROBE=1 the player launches REFLEX_FPV_EVENT (default National NAT_Concord, no AI) on the profile's bike or the garage of
// REFLEX_FPV_GARAGE ("AIRiderId,AIVehicleId,VehicleTypeId" as GripRideProbe), skips the intro with a virtual pad, switches to the Dynamic FPV
// camera and rides the race line (GhostRideProbe's autopilot) for REFLEX_FPV_SECONDS (default 60). Every rendered frame, after every other
// LateUpdate (the camera placed), one row of <shots>/fpvlog.csv: real time and frame time, the presentation clock, the bike's presented
// chassis pose, velocity and contacts, the camera's pose and FOV, the rider's eye bones and wrists, the wheel hubs. Only the camera's
// transform is read, so the same probe logs the old and the new camera (before / after).
// REFLEX_FPV_REC="t0,seconds,fps": also renders the camera offscreen (-batchmode: no window) from t0 s into the ride for `seconds` at `fps`
// into <shots>/rec_00000.png ... (REFLEX_SHOT_SIZE), a strip / video of the view. REFLEX_FPV_STILLS="t1,t2,...": single offscreen stills.
// w-fpvrevert (the restored camera; w-fpvrewrite's / w-fpvfix's FpvConfig cameras are gone):
//   REFLEX_FPV_COMPARE=1: comparison cameras in the same ride, fed the real camera's input every frame: OLD (the 7a9c99ff camera verbatim,
//             FpvOldCamera.cs), RESTORED AS OLD (w-fpvrevert's feel settings at the old values: must equal OLD), RESTORED (the player's settings);
//             "[FpvProbe] COMPARE <camera>" lines (vs OLD: angle / position median, p95, max; chatter: the view's angular speed and the eye's speed
//             above ~3 Hz, RMS) and <shots>/compare.csv per frame.
//   REFLEX_FPV_EFFECTS=1: COMPARE + one camera per variant (FpvRideProbe.Variants: a setting changed), each line its effect vs OLD.
//   REFLEX_FPV_SHADOW=k: COMPARE + OLD / k and RESTORED / k, fed every k-th frame only (on identical input, a k times lower frame rate), each vs
//             its every-frame twin: the frame-rate independence of the camera's dynamics.
//   REFLEX_FPV_SELECT=player: DYNAMIC FPV is picked the player's way before the launch (ReflexClientLive.SelectCamera(4), saved in the scratch save), so
//             it is the camera from the world's start (intro, gate) on; otherwise the probe switches to it after the start (SetCameraMode(1)).
//   REFLEX_FPV_LIVE=1: the ReflexFpv.* keys changed through the options rows' path (the key, then ReflexClientLive.FpvChanged; the last one the key
//             only) during the ride; "[FpvProbe] LIVE" lines = how far the real camera moved from RESTORED (unchanged settings), then RESET.
//   The log's last column `reason` is the camera's FallbackReason (empty while it is on screen).
// Result lines "[FpvProbe] ..."; the player quits at the end.
using System;
using System.Collections;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text;
using Reflex.Port.Content;
using Reflex.Port.OpenFrontEnd;
using Reflex.Port.OriginalWorld;
using Reflex.Port.Options;
using UnityEngine;
using UnityEngine.InputSystem;
using UnityEngine.InputSystem.LowLevel;

namespace Reflex.Port.Showcase
{
    [DefaultExecutionOrder(32000)]
    public sealed class FpvRideProbe : MonoBehaviour
    {
        static readonly CultureInfo Inv = CultureInfo.InvariantCulture;
        string shots; Gamepad pad; OriginalWorldRuntime rt;
        Vector3 bikePos, bikeFwd; float speed; readonly float[] line = new float[3 * 64]; int points; bool have;
        float stuck, reverse; bool drive, logging; double rideStart;
        // REFLEX_FPV_SPEED: the autopilot's target speeds x this (default 1: GhostRideProbe's)
        readonly float speedScale = float.TryParse(Environment.GetEnvironmentVariable("REFLEX_FPV_SPEED"), NumberStyles.Float, CultureInfo.InvariantCulture, out float sc) ? sc : 1f;
        OriginalSimulation.Results last;
        StreamWriter csv; int frame; int shadowEvery;
        readonly bool selectPlayer = Environment.GetEnvironmentVariable("REFLEX_FPV_SELECT") == "player";
        readonly bool effects = Environment.GetEnvironmentVariable("REFLEX_FPV_EFFECTS") == "1", live = Environment.GetEnvironmentVariable("REFLEX_FPV_LIVE") == "1";
        float recT0 = -1, recDur, recFps = 15; int recIndex; double recNext; float[] stills = new float[0]; int stillIndex;

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        static void Guest() { if (Environment.GetEnvironmentVariable("REFLEX_FPV_PROBE") == "1") ReflexProfile.Disabled = true; }

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
        static void Boot()
        {
            if (Environment.GetEnvironmentVariable("REFLEX_FPV_PROBE") != "1" || FindAnyObjectByType<FpvRideProbe>()) return;
            var g = new GameObject("fpv ride probe (test only)"); DontDestroyOnLoad(g); g.AddComponent<FpvRideProbe>();
        }

        void Start()
        {
            shots = Environment.GetEnvironmentVariable("REFLEX_PREVIEW_SHOTS");
            if (!string.IsNullOrEmpty(shots)) Directory.CreateDirectory(shots);
            Application.runInBackground = true; AudioListener.volume = 0f;
            InputSystem.settings.backgroundBehavior = InputSettings.BackgroundBehavior.IgnoreFocus;
            OriginalWorldRuntime.ViewApplied += OnView;
            var rec = Environment.GetEnvironmentVariable("REFLEX_FPV_REC");
            if (!string.IsNullOrEmpty(rec))
            {
                var p = rec.Split(',').Select(x => float.Parse(x.Trim(), Inv)).ToArray();
                recT0 = p[0]; recDur = p.Length > 1 ? p[1] : 5; recFps = p.Length > 2 ? p[2] : 15;
            }
            var st = Environment.GetEnvironmentVariable("REFLEX_FPV_STILLS");
            if (!string.IsNullOrEmpty(st)) stills = st.Split(',').Select(x => float.Parse(x.Trim(), Inv)).OrderBy(x => x).ToArray();
            StartCoroutine(Run());
        }
        void OnDestroy() { OriginalWorldRuntime.ViewApplied -= OnView; csv?.Dispose(); }

        static void Log(string s) => Debug.Log("[FpvProbe] " + s);
        static string F(float v) => v.ToString("0.#####", Inv);
        IEnumerator Frames(int n) { for (int i = 0; i < n; i++) yield return null; }
        IEnumerator Seconds(float s) { float t0 = Time.realtimeSinceStartup; while (Time.realtimeSinceStartup - t0 < s) yield return null; }

        void OnView(OriginalWorldRuntime world, OriginalSimulation.Results v)
        {
            if (v.bikePose == null || v.bikeNodes < 1) return;
            last = v;
            bikePos = new Vector3(v.bikePose[12], v.bikePose[13], v.bikePose[14]); bikeFwd = new Vector3(v.bikePose[8], v.bikePose[9], v.bikePose[10]);
            speed = v.bike.speed; points = Math.Min(64, v.raceLinePoints); Array.Copy(v.raceLine, line, points * 3); have = true;
        }

        static Vector3 P(float[] a, int i) => new Vector3(a[i * 4], a[i * 4 + 1], a[i * 4 + 2]);
        static Quaternion Q(float[] a, int i) => new Quaternion(a[i * 4], a[i * 4 + 1], a[i * 4 + 2], a[i * 4 + 3]);

        void Row()
        {
            if (csv == null || !rt || last == null || last.bike.renderPosition == null) return;
            var cam = rt.chaseCamera; if (!cam) return;
            var b = last.bike; var o = last.onboardState;
            Vector3 bp = P(b.renderPosition, 0); Quaternion bq = Q(b.renderOrientation, 0);
            Vector3 fw = P(b.renderPosition, 2), rw = P(b.renderPosition, 3);
            if (last.hasVehicle && last.vehicle.FourWheels) { fw = last.vehicle.Front; rw = last.vehicle.Rear; }
            Vector3 eye = Vector3.zero, wr = Vector3.zero, wl = Vector3.zero, head = Vector3.zero, er = Vector3.zero, el = Vector3.zero;
            var r = rt.rider;
            if (r && r.bones != null && r.bones.Length > 41 && r.bones[32] && r.bones[33]) { eye = (r.bones[32].position + r.bones[33].position) * 0.5f; head = r.bones[31].position; wr = r.bones[12].position; wl = r.bones[41].position; er = r.bones[11].position; el = r.bones[40].position; }
            var pres = rt.Presentation;
            var ct = cam.transform; Vector3 cp = ct.position; Quaternion cq = ct.rotation;
            bool active = rt.onboard && rt.onboard.Active;
            var sb = new StringBuilder(512);
            void A(float x) { sb.Append(','); sb.Append(F(x)); }
            void V(Vector3 x) { A(x.x); A(x.y); A(x.z); }
            void Qt(Quaternion x) { A(x.x); A(x.y); A(x.z); A(x.w); }
            sb.Append(frame++);
            A((float)(Time.realtimeSinceStartupAsDouble - rideStart)); A(Time.unscaledDeltaTime); A(pres != null ? (float)(pres.Time - 0) : float.NaN);
            A(b.speed); A(o.valid != 0 ? o.front_grounded : -1); A(o.valid != 0 ? o.rear_grounded : -1); A(o.trick_active); A(o.wrecked); A(active ? 1 : 0); A(rt.cameraMode);
            V(bp); Qt(bq); A(b.velocity[0]); A(b.velocity[1]); A(b.velocity[2]);
            V(cp); Qt(cq); A(cam.fieldOfView); A(cam.nearClipPlane);
            V(eye); V(head); V(wr); V(wl); V(fw); V(rw);
            A(b.inputs[0]); A(b.inputs[1]); A(b.inputs[6]); A(b.inputs[7]); V(er); V(el);
            sb.Append(','); sb.Append(rt.onboard ? rt.onboard.FallbackReason : "no-onboard");
            csv.WriteLine(sb.ToString());
        }
        // ---------------------------------------------------------------------------------------------------------------------------
        // w-fpvrevert: comparison cameras, all fed the same presented state as the real camera, every frame, after every other LateUpdate (the rider posed)
        //   OLD                the 7a9c99ff camera, verbatim (FpvOldCamera.cs), at its defaults
        //   RESTORED AS OLD    this build's camera with w-fpvrevert's feel settings at the old camera's values (HEAD BOB 1, HEAD SMOOTHING 0.035, VIEW
        //                      SMOOTHING 0): must equal OLD
        //   RESTORED           this build's camera with the player's settings (the scratch save: the defaults)
        //   + REFLEX_FPV_EFFECTS=1: one camera per variant (Variants: a row changed from the player's settings) for the tuning / each row's effect
        //   + REFLEX_FPV_SHADOW=k: OLD / k and RESTORED / k fed every k-th frame only (a k times lower frame rate on identical input; each vs its
        //     every-frame twin: the frame-rate independence)
        // Each is compared with OLD (angle / position, median / p95 / max) and measured for chatter: the RMS of the view's angular speed above ~3 Hz
        // (deg/s, total and per axis of the view: pitch / yaw / roll) and of the eye's speed in the bike's frame above ~3 Hz (ft/s), over 15 mph.
        // <shots>/compare.csv: per frame, each camera's angle / position from OLD.
        sealed class Cmp
        {
            public string name; public Camera c; public FpvOldCamera old; public OriginalOnboardCamera cam; public int every = 1; public Cmp twin;
            public readonly System.Collections.Generic.List<float> dang = new System.Collections.Generic.List<float>(), dpos = new System.Collections.Generic.List<float>(), dfov = new System.Collections.Generic.List<float>();
            public bool primed; public Quaternion prevQ; public Vector3 prevP, wLow, vLow; public double w2, wx2, wy2, wz2, v2; public int n, ok, total, kept; public float prevT;
            public bool Apply(Vector3 bp, Quaternion bq, Vector3 fw, Vector3 rw, Vector3 vel, float speed, float lfb, float llr, Terrain t)
                => old ? old.Apply(bp, bq, fw, rw, vel, speed, lfb, llr, t, false) : cam.Apply(bp, bq, fw, rw, vel, speed, lfb, llr, t, false);
            public void Context(bool atv, bool machine, bool active, bool wreck, bool trick, bool rs, bool fg, bool rg, bool contacts, double clock)
            { if (old) old.SetContext(atv, machine, active, wreck, trick, rs, fg, rg, contacts, clock); else cam.SetContext(atv, machine, active, wreck, trick, rs, fg, rg, contacts, clock); }
        }
        System.Collections.Generic.List<Cmp> cmps; Cmp cmpOld, cmpBase; StreamWriter cmpCsv;
        readonly System.Collections.Generic.List<float> realAng = new System.Collections.Generic.List<float>(), realPos = new System.Collections.Generic.List<float>();
        readonly bool compare = Environment.GetEnvironmentVariable("REFLEX_FPV_COMPARE") == "1";
        /// The variants of REFLEX_FPV_EFFECTS: a row of the player's settings changed.
        static readonly (string name, Action<OriginalOnboardCamera> set)[] Variants =
        {
            ("ONLY HEAD SMOOTHING 0.035 -> 0.05", c => { c.headSmoothing = 0.05f; c.viewSmoothing = 0f; }), ("ONLY VIEW SMOOTHING 0 -> 0.03", c => { c.headSmoothing = 0.035f; c.viewSmoothing = 0.03f; }),
            ("HEAD SMOOTHING 0.10", c => c.headSmoothing = 0.1f), ("VIEW SMOOTHING 0.06", c => c.viewSmoothing = 0.06f), ("VIEW SMOOTHING 0.10", c => c.viewSmoothing = 0.1f),
            ("HEAD BOB 0%", c => c.headBob = 0f), ("HEAD BOB 50%", c => c.headBob = 0.5f),
            ("TILT 0 -> 50%", c => c.tilt = 0.5f), ("TILT 0 -> 100%", c => c.tilt = 1f), ("PITCH 0.31 -> 0.80", c => c.pitch = 0.8f),
            ("HEAD SHIFT ON", c => c.headShift = true), ("HEAD SHIFT ON, STRENGTH 0", c => { c.headShift = true; c.shiftLong = c.shiftLat = c.shiftVert = 0f; }),
            ("HEAD MODE CHASSIS", c => c.headMode = 0), ("HEAD MODE TRUE", c => c.headMode = 2), ("CORNER LEAN", c => c.cornerAnticipation = 2),
            ("FIELD OF VIEW 116 -> 96", c => c.fov = 96f), ("EYE UP +0.5 FT", c => c.offset = c.offset + new Vector3(0, 0.5f, 0)),
        };
        Cmp NewCmp(string name, bool oldCamera, Action<OriginalOnboardCamera> set = null, int every = 1)
        {
            var g = new GameObject("fpv compare camera (test only) " + name); var c = g.AddComponent<Camera>(); c.enabled = false; c.aspect = rt.chaseCamera.aspect;
            var x = new Cmp { name = name, c = c, every = every };
            if (oldCamera) { x.old = g.AddComponent<FpvOldCamera>(); x.old.Bind(c, rt.rider); }
            else
            {
                x.cam = g.AddComponent<OriginalOnboardCamera>(); x.cam.Load(); x.cam.ownSettings = true; set?.Invoke(x.cam);
                x.cam.Bind(c, rt.rider);
            }
            return x;
        }
        void FeedCompare()
        {
            if (!(compare || effects || live || shadowEvery > 0) || !rt || !rt.rider || last == null || last.bike.renderPosition == null || !rt.onboard) return;
            if (cmps == null)
            {
                cmps = new System.Collections.Generic.List<Cmp>();
                cmpOld = NewCmp("OLD", true); cmps.Add(cmpOld);
                cmps.Add(NewCmp("RESTORED AS OLD", false, c => { c.headBob = 1f; c.headSmoothing = 0.035f; c.viewSmoothing = 0f; }));
                cmpBase = NewCmp("RESTORED", false); cmps.Add(cmpBase);
                if (effects) foreach (var v in Variants) cmps.Add(NewCmp(v.name, false, v.set));
                if (shadowEvery > 1)
                {
                    var so = NewCmp("OLD / " + shadowEvery, true, null, shadowEvery); so.twin = cmpOld; cmps.Add(so);
                    var sr = NewCmp("RESTORED / " + shadowEvery, false, null, shadowEvery); sr.twin = cmpBase; cmps.Add(sr);
                }
                cmpCsv = new StreamWriter(Path.Combine(shots, "compare.csv"));
                cmpCsv.WriteLine("frame,t,speed," + string.Join(",", cmps.Skip(1).Select(x => "\"" + x.name + " ang\",\"" + x.name + " pos\"")));
            }
            var b = last.bike; var o = last.onboardState; bool valid = o.valid != 0;
            Vector3 fw = P(b.renderPosition, 2), rw = P(b.renderPosition, 3);
            if (last.hasVehicle && last.vehicle.FourWheels) { fw = last.vehicle.Front; rw = last.vehicle.Rear; }
            Vector3 bp = P(b.renderPosition, 0); Quaternion bq = Q(b.renderOrientation, 0); Vector3 vel = new Vector3(b.velocity[0], b.velocity[1], b.velocity[2]);
            var terrainC = FindTerrain();
            float now = (float)(Time.realtimeSinceStartupAsDouble - rideStart);
            var row = new StringBuilder(256); row.Append(frame).Append(',').Append(F(now)).Append(',').Append(F(b.speed));
            foreach (var f in cmps)
            {
                bool feed = frame % f.every == 0;
                if (feed)
                {
                    f.Context(last.hasVehicle && last.vehicle.Atv, last.hasVehicle && last.vehicle.Machine, rt.rider.gameObject.activeInHierarchy && (!valid || o.rider_active != 0),
                        valid && o.wrecked != 0, valid && o.trick_active != 0, OriginalInput.RightStickRides, valid && o.front_grounded != 0, valid && o.rear_grounded != 0,
                        valid && o.contacts_valid != 0, rt.Presentation != null ? rt.Presentation.Time : double.NaN);
                    f.total++;
                    if (!f.Apply(bp, bq, fw, rw, vel, b.speed, b.inputs[7], b.inputs[6], terrainC)) { f.primed = false; if (f != cmpOld) row.Append(",,"); continue; }
                    f.ok++;
                }
                var t = f.c.transform;
                float dt = now - f.prevT;
                if (feed && f.primed && dt > 0 && f.every == 1)
                {
                    Vector3 w = RotVecDeg(t.rotation * Quaternion.Inverse(f.prevQ)) / dt, v = (Quaternion.Inverse(bq) * (t.position - bp) - f.prevP) / dt;
                    float k = 1f - Mathf.Exp(-dt / 0.05f);   // (~3 Hz: the residual above it is the chatter)
                    f.wLow = Vector3.Lerp(f.wLow, w, k); f.vLow = Vector3.Lerp(f.vLow, v, k);
                    if (b.speed > 15f)
                    {
                        Vector3 r = w - f.wLow, rl = Quaternion.Inverse(t.rotation) * r;
                        f.w2 += r.sqrMagnitude; f.wx2 += rl.x * rl.x; f.wy2 += rl.y * rl.y; f.wz2 += rl.z * rl.z; f.v2 += (v - f.vLow).sqrMagnitude; f.n++;
                    }
                }
                if (feed) { f.prevQ = t.rotation; f.prevP = Quaternion.Inverse(bq) * (t.position - bp); f.prevT = now; f.primed = true; }
                var reference = f.twin ?? cmpOld;
                if (f != cmpOld)
                {
                    if (feed && f.primed && reference.primed)
                    {
                        var bt = reference.c.transform;
                        float da = Quaternion.Angle(bt.rotation, t.rotation), dp = (bt.position - t.position).magnitude;
                        f.dang.Add(da); f.dpos.Add(dp); f.dfov.Add(f.c.fieldOfView - reference.c.fieldOfView);
                        row.Append(',').Append(F(da)).Append(',').Append(F(dp));
                    }
                    else row.Append(",,");
                }
            }
            cmpCsv.WriteLine(row.ToString());
            if (rt.onboard.Active && cmpBase.primed && liveName == null) { realAng.Add(Quaternion.Angle(rt.chaseCamera.transform.rotation, cmpBase.c.transform.rotation)); realPos.Add((rt.chaseCamera.transform.position - cmpBase.c.transform.position).magnitude); }
            if (live && cmpBase.primed && rt.onboard.Active && liveName != null && frame - liveFrame >= 3 && frame - liveFrame <= 4)
            {
                var a = rt.chaseCamera.transform; var bt = cmpBase.c.transform;
                Log($"LIVE {liveName}: the real camera vs the unchanged RESTORED, {frame - liveFrame} frames later: angle {Quaternion.Angle(a.rotation, bt.rotation):0.00} deg, position {(a.position - bt.position).magnitude:0.000} ft, vertical FOV {rt.chaseCamera.fieldOfView:0.0} vs {cmpBase.c.fieldOfView:0.0}");
            }
        }
        Terrain terrainCache; bool terrainLooked;
        Terrain FindTerrain() { if (!terrainLooked) { terrainLooked = true; terrainCache = FindAnyObjectByType<Terrain>(); } return terrainCache; }
        static Vector3 RotVecDeg(Quaternion q) { q.ToAngleAxis(out float ang, out Vector3 ax); if (ang > 180f) ang -= 360f; return float.IsNaN(ax.x) || float.IsInfinity(ax.x) ? Vector3.zero : ax * ang; }
        static string Pct(System.Collections.Generic.List<float> l, float p) { if (l.Count == 0) return "-"; var a = l.Select(Mathf.Abs).OrderBy(x => x).ToList(); return a[Mathf.Clamp((int)(p * (a.Count - 1)), 0, a.Count - 1)].ToString("0.0000", Inv); }
        void ReportCompare()
        {
            cmpCsv?.Dispose(); cmpCsv = null;
            if (cmps == null) return;
            Log($"COMPARE the real camera (the runtime's, on screen) vs RESTORED (fed here): angle median {Pct(realAng, .5f)} p95 {Pct(realAng, .95f)} max {Pct(realAng, 1f)} deg, position median {Pct(realPos, .5f)} max {Pct(realPos, 1f)} ft ({realAng.Count} frames)");
            foreach (var f in cmps)
            {
                string vs = f == cmpOld ? "" : $"; vs {(f.twin ?? cmpOld).name}: angle median {Pct(f.dang, .5f)} p95 {Pct(f.dang, .95f)} max {Pct(f.dang, 1f)} deg, position median {Pct(f.dpos, .5f)} p95 {Pct(f.dpos, .95f)} max {Pct(f.dpos, 1f)} ft, vFOV max {Pct(f.dfov, 1f)} deg";
                string chat = f.n > 0 ? $"; chatter (> 3 Hz, over 15 mph, {f.n} frames) view {Math.Sqrt(f.w2 / f.n):0.00} deg/s (pitch {Math.Sqrt(f.wx2 / f.n):0.00}, yaw {Math.Sqrt(f.wy2 / f.n):0.00}, roll {Math.Sqrt(f.wz2 / f.n):0.00}), eye {Math.Sqrt(f.v2 / f.n):0.000} ft/s" : "";
                Log($"COMPARE {f.name}: active {f.ok}/{f.total}{vs}{chat}");
            }
        }
        string liveName; int liveFrame;
        IEnumerator LiveSweep()
        {
            // the options rows' path: the key written, then ReflexClientLive.FpvChanged; the last one writes the key only (the camera's key watch)
            var steps = new (string name, Action set)[]
            {
                ("FIELD OF VIEW 116->96", () => ReflexSave.SetFloat("ReflexFpv.fov", 96f)), ("TILT 0->100%", () => ReflexSave.SetFloat("ReflexFpv.tilt", 1f)),
                ("PITCH 0.31->0.80", () => ReflexSave.SetFloat("ReflexFpv.pitch", 0.8f)), ("HEAD MODE CHASSIS", () => ReflexSave.SetInt("ReflexFpv.mode", 0)),
                ("HEAD BOB 0%", () => ReflexSave.SetFloat("ReflexFpv.headBob", 0f)), ("EYE UP +0.5 FT", () => ReflexSave.SetFloat("ReflexFpv.offY", 0.5f)),
                ("CORNER ANTICIPATION LEAN", () => ReflexSave.SetInt("ReflexFpv.ca", 2)),
            };
            yield return Seconds(6f);
            for (int i = 0; i <= steps.Length; i++)
            {
                bool keyOnly = i == steps.Length;
                var st = keyOnly ? ("TILT 0->100%, the key only (no FpvChanged)", (Action)(() => ReflexSave.SetFloat("ReflexFpv.tilt", 1f))) : steps[i];
                st.Item2(); if (!keyOnly) Reflex.Port.Options.ReflexClientLive.FpvChanged();
                liveName = st.Item1; liveFrame = frame;
                yield return Seconds(2.5f);
                Log($"LIVE {st.Item1}: after 2.5 s the real camera vs the unchanged RESTORED: angle {Quaternion.Angle(rt.chaseCamera.transform.rotation, cmpBase.c.transform.rotation):0.00} deg, position {(rt.chaseCamera.transform.position - cmpBase.c.transform.position).magnitude:0.000} ft, vFOV {rt.chaseCamera.fieldOfView:0.0} vs {cmpBase.c.fieldOfView:0.0} (active {rt.onboard.Active}, reason '{rt.onboard.FallbackReason}', camera tilt {rt.onboard.tilt:0.00} fov {rt.onboard.fov:0})");
                Reflex.Port.Options.ReflexSettings.ResetOnboard(); liveName = null;
                yield return Seconds(1.5f);
                Log($"LIVE after RESET: the real camera vs RESTORED {Quaternion.Angle(rt.chaseCamera.transform.rotation, cmpBase.c.transform.rotation):0.00} deg (camera tilt {rt.onboard.tilt:0.00} fov {rt.onboard.fov:0} mode {rt.onboard.headMode})");
            }
        }
        const string Header = "frame,t,dt,pres,speed,fg,rg,trick,wreck,active,mode,bx,by,bz,bqx,bqy,bqz,bqw,vx,vy,vz,cx,cy,cz,cqx,cqy,cqz,cqw,fov,near," +
                              "ex,ey,ez,hx,hy,hz,wrx,wry,wrz,wlx,wly,wlz,fwx,fwy,fwz,rwx,rwy,rwz,in0,in1,in6,in7,erx,ery,erz,elx,ely,elz,reason";

        // ---------------------------------------------------------------------------------------------------------------------------
        // the autopilot (GhostRideProbe's)
        void Update()
        {
            if (pad == null || !pad.added || !drive || !have || points < 2) return;
            int nearest = 0; float best = float.MaxValue;
            for (int i = 0; i < points && i < 6; i++) { float dx = line[3 * i] - bikePos.x, dz = line[3 * i + 2] - bikePos.z, d = dx * dx + dz * dz; if (d < best) { best = d; nearest = i; } }
            float ahead = 18f + speed * 0.6f; int target = nearest;
            for (int i = nearest; i < points; i++) { float dx = line[3 * i] - bikePos.x, dz = line[3 * i + 2] - bikePos.z; target = i; if (dx * dx + dz * dz >= ahead * ahead) break; }
            float err = Mathf.DeltaAngle(Mathf.Atan2(bikeFwd.x, bikeFwd.z) * Mathf.Rad2Deg, Mathf.Atan2(line[3 * target] - bikePos.x, line[3 * target + 2] - bikePos.z) * Mathf.Rad2Deg) * Mathf.Deg2Rad;
            float steer = Mathf.Clamp(err * 2.2f, -1f, 1f), turn = Mathf.Abs(err);
            float desired = (turn > 1.2f ? 8f : turn > 0.6f ? 14f : turn > 0.3f ? 20f : 30f) * speedScale;
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
            AudioListener.volume = 0f;   // test players are muted (the scratch save's MASTER VOLUME is 0 too)
            if (!logging) return;
            Row();
            FeedCompare();
            double t = Time.realtimeSinceStartupAsDouble - rideStart;
            if (recT0 >= 0 && t >= recT0 && t < recT0 + recDur && t >= recNext && !string.IsNullOrEmpty(shots))
            {
                OffscreenShot(rt.chaseCamera, Path.Combine(shots, $"rec_{recIndex++:00000}.png"), 2);
                recNext = Math.Max(recNext + 1.0 / recFps, t);
            }
            if (stillIndex < stills.Length && t >= stills[stillIndex] && !string.IsNullOrEmpty(shots))
                OffscreenShot(rt.chaseCamera, Path.Combine(shots, $"still_{stills[stillIndex++]:000.0}s.png"), 8);
        }

        // -batchmode (no window): the camera rendered into a target of REFLEX_SHOT_SIZE (default 1280x720), read back and saved (as GripRideProbe)
        static void OffscreenShot(Camera cam, string file, int renders)
        {
            if (!cam) return;
            int W = 1280, H = 720;
            var sz = Environment.GetEnvironmentVariable("REFLEX_SHOT_SIZE");
            if (!string.IsNullOrEmpty(sz) && sz.Split('x') is var p && p.Length == 2) { int.TryParse(p[0], out W); int.TryParse(p[1], out H); }
            var rtx = RenderTexture.GetTemporary(W, H, 24, RenderTextureFormat.ARGB32);
            var prior = cam.targetTexture;
            try
            {
                cam.targetTexture = rtx;
                for (int i = 0; i < renders; i++) cam.Render();
                var prev = RenderTexture.active; RenderTexture.active = rtx;
                var t = new Texture2D(W, H, TextureFormat.RGB24, false);
                t.ReadPixels(new Rect(0, 0, W, H), 0, 0); t.Apply(false);
                RenderTexture.active = prev;
                if (System.Environment.GetEnvironmentVariable("REFLEX_SHOT_FLIP") == "1")   // (offscreen renders come back upright: the display gamma ramp no longer turns them, ReflexGammaOutput)
                {
                    var px = t.GetPixels32(); var o = new Color32[px.Length];
                    for (int y = 0; y < H; y++) Array.Copy(px, y * W, o, (H - 1 - y) * W, W);
                    t.SetPixels32(o); t.Apply(false);
                }
                File.WriteAllBytes(file, t.EncodeToJPG(88)); Destroy(t);
            }
            catch (Exception e) { Log("OFFSCREEN failed: " + e.Message); }
            finally { cam.targetTexture = prior; RenderTexture.ReleaseTemporary(rtx); }
        }

        IEnumerator Tap(GamepadButton b)
        {
            InputSystem.QueueStateEvent(pad, new GamepadState(b)); yield return Frames(3);
            InputSystem.QueueStateEvent(pad, new GamepadState()); yield return Frames(4);
        }

        IEnumerator Run()
        {
            pad = InputSystem.AddDevice<Gamepad>("fpv probe pad");
            OpenShell shell = null;
            for (float t = 0; t < 120f && !(shell = OpenShell.Instance); t += Time.unscaledDeltaTime) yield return null;
            if (!shell) { Log("FAIL the shell did not start"); Quit(); yield break; }
            yield return Seconds(2f);
            string evName = Environment.GetEnvironmentVariable("REFLEX_FPV_EVENT") ?? "National/NAT_Concord";
            var ev = ReflexEvents.Find(evName) ?? ReflexEvents.All.First(e => e.spec.type == "National" && e.location == "NAT_Concord");
            ReflexWorldFiles.Garage garage = null;
            var gs = Environment.GetEnvironmentVariable("REFLEX_FPV_GARAGE");
            if (!string.IsNullOrEmpty(gs))
            {
                var p = gs.Split(',').Select(x => int.Parse(x.Trim(), Inv)).ToArray();
                int gear = -1;
                try { var row = ReflexEvents.Tables["AIRider"].FirstOrDefault(x => Convert.ToInt32(x["AIRiderId"], Inv) == p[0]); if (row != null && row.TryGetValue("DefaultRiderGearSetId", out var gv)) gear = Convert.ToInt32(gv, Inv); } catch { }
                garage = new ReflexWorldFiles.Garage { riderOwner = ReflexWorldFiles.Garage.AI, riderId = p[0], riderGearSet = gear, vehicleOwner = ReflexWorldFiles.Garage.AI, vehicleId = p[1], vehicleTypeId = p.Length > 2 ? p[2] : -1, vehicleClass = -1 };
                if (garage.vehicleTypeId >= 0) { var g2 = ev.GarageFor(garage.vehicleTypeId); if (g2 != null) garage.vehicleClass = g2.vehicleClass; }
            }
            if (selectPlayer) { Reflex.Port.Options.ReflexClientLive.SelectCamera(4); Log("DYNAMIC FPV selected before the launch (the player's way): camera mode saved " + Reflex.Port.Options.ReflexSettings.CameraMode); }
            Log("launching " + ev.key + (garage != null ? " garage " + garage : ""));
            ReflexEventSelect.Launch(ev.key, 0, false, "Reflex", 0, garage);
            for (float t = 0; t < 400f; t += Time.unscaledDeltaTime)
            {
                rt = FindAnyObjectByType<OriginalWorldRuntime>();
                if (rt && rt.FailureMessage != null) { Log("FAIL world: " + rt.FailureMessage); Quit(); yield break; }
                if (rt && rt.Running && (rt.IntroCardShown || rt.EventUi.race_state >= 1)) break;
                yield return null;
            }
            if (!(rt && rt.Running)) { Log("FAIL the world does not run"); Quit(); yield break; }
            if (rt.IntroCardShown) { yield return Seconds(1f); yield return Tap(GamepadButton.South); }
            for (float t = 0; t < 120f && !(rt && (rt.Race.state == 2 || rt.Race.valid == 0)); t += Time.unscaledDeltaTime) yield return null;
            if (!selectPlayer) rt.SetCameraMode(1, false);
            Log($"camera mode {rt.cameraMode}, settings fov {(rt.onboard ? rt.onboard.fov : 0):0} tilt {(rt.onboard ? rt.onboard.tilt : 0):0.00} pitch {(rt.onboard ? rt.onboard.pitch : 0):0.00} mode {(rt.onboard ? rt.onboard.headMode : -1)} headSmoothing {(rt.onboard ? rt.onboard.headSmoothing : 0):0.000} viewSmoothing {(rt.onboard ? rt.onboard.viewSmoothing : 0):0.000} headBob {(rt.onboard ? rt.onboard.headBob : 0):0.00} (own settings {(rt.onboard ? rt.onboard.ownSettings : false)}), FPV active {(rt.onboard ? rt.onboard.Active : false)}, reason '{(rt.onboard ? rt.onboard.FallbackReason : "-")}', driving camera {rt.DrivingCameraPreset}");
            Log($"pacing {rt.Pacing} cap {Application.targetFrameRate} vsync {QualitySettings.vSyncCount}");
            string path = Path.Combine(string.IsNullOrEmpty(shots) ? Application.temporaryCachePath : shots, "fpvlog.csv");
            csv = new StreamWriter(path, false, new UTF8Encoding(false), 1 << 16); csv.WriteLine(Header);
            int.TryParse(Environment.GetEnvironmentVariable("REFLEX_FPV_SHADOW"), out shadowEvery);
            drive = true; rideStart = Time.realtimeSinceStartupAsDouble; logging = true;
            if (live) StartCoroutine(LiveSweep());
            float seconds = float.TryParse(Environment.GetEnvironmentVariable("REFLEX_FPV_SECONDS"), NumberStyles.Float, Inv, out float s) ? s : 60f;
            yield return Seconds(seconds);
            logging = false; drive = false;
            csv.Flush(); csv.Dispose(); csv = null;
            ReportCompare();
            Log($"rows {frame} over {seconds} s ({frame / Mathf.Max(1f, seconds):0.0} fps), recorded {recIndex} frames");
            Log("DONE");
            yield return Frames(30);
            Quit();
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

// TEST ONLY (w-grips; tests/menus_showcase fixtures, run with tests/rider_grips/grip_ride.py): the rider's hands on the grips in a real ride.
// With REFLEX_GRIP_PROBE=1 the player launches REFLEX_GRIP_EVENT (default National NAT_Concord, no AI) on the profile's bike or the garage of
// REFLEX_GRIP_GARAGE ("vehicleOwner,vehicleId,vehicleTypeId,vehicleClass", ReflexWorldFiles.Garage), skips the intro with a virtual pad, rides
// the race line with it (GhostRideProbe's autopilot) and:
//   1. logs the rider's bones, the bike's hierarchy nodes and parts, the glove meshes
//   2. measures each frame, per hand, the fist (the centre and line of its fingers' curl circles, as RiderGripIK sees it) against
//      RiderGripIK's grip: off its axis, along from its end, the angle of the axes; the wrist to the hand locator; the throttle tube's turn
//      -> <shots>/grips.csv and a summary per GRIP IK state (RiderGripIK.Enabled: alternate seconds off / on); <shots>/grips-dump.csv:
//      the bars about each hand locator and the hands (bones, glove vertices) in the forks' model space, GRIP IK off (offline analysis)
//   3. takes matched shots, GRIP IK off / on: the Dynamic FPV view (fpv-*; fpv-down-*: its camera turned 28 degrees further down, the
//      report's picture), close-ups of the bars from the front, above and each side (tp-*) and the follow camera (chase-*)
//   4. tries tricks in the air (the stunt modifier LB + right stick gestures: NoHander, Superman, Taunt, UpCan, SuperCan, Shaolin), logs the
//      hands and shoots each trick that plays as the right hand leaves the bars and when it is off (trick<n>-leaving / -off)
// Result lines "[GripProbe] ..."; the player quits at the end.
// w-gripwidth: -batchmode (grip_ride.py --batch) renders every shot offscreen (the camera into a target, no window); the glove's extent
// along each grip is measured too (where the glove sits on the rubber: its inner / outer edge from the grip's end); tp-grip-r / -l are
// close-ups of each hand on its grip; REFLEX_GRIP_TRICKS=0 skips the trick phase.
using System;
using System.Collections;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Text;
using Reflex.Port.Content;
using Reflex.Port.OpenFrontEnd;
using Reflex.Port.OriginalWorld;
using UnityEngine;
using UnityEngine.InputSystem;
using UnityEngine.InputSystem.LowLevel;

namespace Reflex.Port.Showcase
{
    [DefaultExecutionOrder(32000)]
    public sealed class GripRideProbe : MonoBehaviour
    {
        static readonly CultureInfo Inv = CultureInfo.InvariantCulture;
        string shots; Gamepad pad; OriginalWorldRuntime rt;
        Vector3 bikePos, bikeFwd; float speed; readonly float[] line = new float[3 * 64]; int points; bool have;
        float stuck, reverse; bool drive;
        // the trick attempt (LB + a right stick gesture while in the air)
        string gesture; int gestureStep, gestureFrames; int airFrames; int trickIndex; bool trickUsedThisAir;
        static readonly (string name, string input)[] Tricks = { ("NoHander", "010"), ("Superman", "031"), ("Taunt_L", "032"), ("UpCan_L", "232"), ("SuperCan_L", "012"), ("Shaolin", "020") };
        // the camera override (third-person close-ups, chassis frame)
        bool overrideCam; Vector3 camOffset; float camFov = 40f;
        // measuring
        StringBuilder csv = new StringBuilder();
        Hand[] hands; FieldInfo ikField; ReflexBikeModel bike; int frame;
        readonly Dictionary<bool, Stats[]> stats = new Dictionary<bool, Stats[]>();
        Mesh baked;
        OriginalSimulation.Results last;

        sealed class Hand
        {
            public string side, locator; public int wrist, locNode, gripNode;
            public Vector3 gripC, gripA; public float spanMin, spanMax;   // model space: centre, outward axis, extent along it from the centre
            public HashSet<int> bones = new HashSet<int>();
            public SkinnedMeshRenderer glove; public List<int> verts = new List<int>();
        }
        sealed class Stats { public int n; public double palmR, palmAlong, wristR, locR, wristLoc; public float palmRMax; public int gn; public double gloveIn, gloveOut; }

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        static void Guest() { if (Environment.GetEnvironmentVariable("REFLEX_GRIP_PROBE") == "1") ReflexProfile.Disabled = true; }

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
        static void Boot()
        {
            if (Environment.GetEnvironmentVariable("REFLEX_GRIP_PROBE") != "1" || FindAnyObjectByType<GripRideProbe>()) return;
            var g = new GameObject("grip ride probe (test only)"); DontDestroyOnLoad(g); g.AddComponent<GripRideProbe>();
        }

        void Start()
        {
            shots = Environment.GetEnvironmentVariable("REFLEX_PREVIEW_SHOTS");
            if (!string.IsNullOrEmpty(shots)) Directory.CreateDirectory(shots);
            Application.runInBackground = true;
            InputSystem.settings.backgroundBehavior = InputSettings.BackgroundBehavior.IgnoreFocus;
            OriginalWorldRuntime.ViewApplied += OnView;
            ikField = Type.GetType("Reflex.Port.OriginalWorld.RiderGripIK, Assembly-CSharp")?.GetField("Enabled", BindingFlags.Public | BindingFlags.Static);
            csv.AppendLine("frame,ik,trick,front,rear,side,fistToGrip,fistAlong,fistAngle,wristLoc,speed");
            StartCoroutine(Run());
        }
        void OnDestroy() { OriginalWorldRuntime.ViewApplied -= OnView; }

        static void Log(string s) => Debug.Log("[GripProbe] " + s);
        static string F(float v) => v.ToString("0.000", Inv);
        static string V(Vector3 v) => "(" + F(v.x) + ", " + F(v.y) + ", " + F(v.z) + ")";
        IEnumerator Frames(int n) { for (int i = 0; i < n; i++) yield return null; }
        IEnumerator Seconds(float s) { float t0 = Time.realtimeSinceStartup; while (Time.realtimeSinceStartup - t0 < s) yield return null; }
        bool Ik { get => ikField != null && (bool)ikField.GetValue(null); set { if (ikField != null) ikField.SetValue(null, value); } }

        void OnView(OriginalWorldRuntime world, OriginalSimulation.Results v)
        {
            if (v.bikePose == null || v.bikeNodes < 1) return;
            last = v;
            bikePos = new Vector3(v.bikePose[12], v.bikePose[13], v.bikePose[14]); bikeFwd = new Vector3(v.bikePose[8], v.bikePose[9], v.bikePose[10]);
            speed = v.bike.speed; points = Math.Min(64, v.raceLinePoints); Array.Copy(v.raceLine, line, points * 3); have = true;
            frame++;
            if (hands != null && bike != null) Throttle(v);
            if (hands != null && rt && rt.rider && rt.rider.HasOriginalPose) { Measure(v); if (dumpEvery > 0 && frame % dumpEvery == 0 && v.onboardState.trick_active == 0) Dump(v); }
            // a picture in each trick: as the right hand leaves the bars (0.3 ft past its riding place) and with it far off (1.5 ft)
            if (tricks && v.onboardState.trick_active != 0 && hands != null && rt.rider)
            {
                float wl = (rt.rider.bones[12].position - (Vector3)OriginalBikePose.FromRows(v.bikePose, hands[0].locNode * 16).GetColumn(3)).magnitude;
                if (trickShot == 0 && wl > 0.7f) { trickShot = 1; StartCoroutine(Shot($"trick{trickCount}-leaving")); }
                else if (trickShot == 1 && wl > 1.5f) { trickShot = 2; StartCoroutine(Shot($"trick{trickCount}-off")); }
            }
            else if (trickShot != 0) { trickShot = 0; trickCount++; }
        }

        // ---------------------------------------------------------------------------------------------------------------------------
        // setup: names, the grips from the bar mesh about each hand locator, the gloves' hand vertices
        void Describe()
        {
            var r = rt.rider;
            bike = typeof(OriginalWorldRuntime).GetField("bikeModel", BindingFlags.NonPublic | BindingFlags.Instance)?.GetValue(rt) as ReflexBikeModel;
            Log("rider bones: " + string.Join(" ", Enumerable.Range(0, r.bones.Length).Select(i => i + ":" + (r.bones[i] ? r.bones[i].name : "-") + "<" + (r.boneParents != null && i < r.boneParents.Length ? r.boneParents[i] : -9))));
            Log("rider meshes: " + string.Join(", ", (r.skinnedMeshes ?? new SkinnedMeshRenderer[0]).Where(s => s).Select(s => s.name + " (" + s.sharedMesh?.vertexCount + " v, " + s.bones.Length + " bones)")));
            if (bike == null) { Log("no bike model"); return; }
            Log("bike nodes: " + string.Join(" ", Enumerable.Range(0, bike.hierarchyNodes.Length).Select(i => i + ":" + bike.hierarchyNodes[i])));
            Log("bike parts: " + string.Join(", ", bike.parts.Select(p => p.name + "@" + p.node + "[" + p.material + "]")));
            Log("bike limb offsets (mod): " + (bike.limbOffsets == null ? "none" : string.Join(", ", bike.limbOffsets.Select(o => o.locator + " " + V(o.delta)))));
            var list = new List<Hand>();
            foreach (var (side, loc, wrist) in new[] { ("R", "NDE_RHT_Hand", 12), ("L", "NDE_LFT_Hand", 41) })
            {
                int ln = Array.IndexOf(bike.hierarchyNodes, loc);
                if (ln < 0) { Log("no locator " + loc); continue; }
                var h = new Hand { side = side, locator = loc, wrist = wrist, locNode = ln };
                Vector3 L = bike.hierarchyBind[ln].GetColumn(3);
                // the bar mesh's vertices about the locator (every part; the node is the one with the most of them)
                var near = new List<(Vector3 p, int node, string part)>();
                foreach (var p in bike.parts)
                {
                    if (!p.part) continue;
                    foreach (var mf in p.part.GetComponentsInChildren<MeshFilter>(true))
                    {
                        var m = mf.sharedMesh; if (!m || !m.isReadable) continue;
                        var toModel = p.part.worldToLocalMatrix * mf.transform.localToWorldMatrix;
                        foreach (var vv in m.vertices) { var q = toModel.MultiplyPoint3x4(vv); if ((q - L).sqrMagnitude < 0.35f * 0.35f) near.Add((q, p.node, p.name)); }
                    }
                }
                if (near.Count < 8) { Log($"{loc}: only {near.Count} bike vertices near the locator {V(L)}"); continue; }
                h.gripNode = near.GroupBy(x => x.node).OrderByDescending(g => g.Count()).First().Key;
                Log($"{loc} (node {ln}): at {V(L)}; {near.Count} bar vertices within 0.35 ft: " +
                    string.Join(", ", near.GroupBy(x => x.part + "@" + x.node).Select(g => g.Key + " " + g.Count())));
                var pts = near.Where(x => x.node == h.gripNode).Select(x => x.p).ToList();
                Fit(pts, out var c, out var a);
                if (side == "R" ? a.x < 0 : a.x > 0) a = -a;   // outward
                // refine: the cylinder about the axis (r < 0.1 ft)
                var cyl = pts.Where(q => Vector3.Cross(q - c, a).magnitude < 0.1f).ToList();
                if (cyl.Count > 8) { Fit(cyl, out c, out var a2); if (Vector3.Dot(a2, a) < 0) a2 = -a2; a = a2; }
                float lo = cyl.Count > 0 ? cyl.Min(q => Vector3.Dot(q - c, a)) : 0, hi = cyl.Count > 0 ? cyl.Max(q => Vector3.Dot(q - c, a)) : 0;
                float rad = cyl.Count > 0 ? cyl.Average(q => Vector3.Cross(q - c, a).magnitude) : 0;
                h.gripC = c; h.gripA = a; h.spanMin = lo; h.spanMax = hi;
                Log($"{loc} grip (node {h.gripNode} {bike.hierarchyNodes[h.gripNode]}): centre {V(c)}, axis {V(a)}, span {F(lo)} .. {F(hi)}, mean radius {F(rad)}, the locator from the centre {V(L - c)} (along {F(Vector3.Dot(L - c, a))}, off the axis {F(Vector3.Cross(L - c, a).magnitude)})");
                // the hand: the wrist and its descendants
                h.bones.Add(wrist);
                for (bool grew = true; grew;) { grew = false; for (int i = 0; i < r.boneParents.Length; i++) if (!h.bones.Contains(i) && h.bones.Contains(r.boneParents[i])) { h.bones.Add(i); grew = true; } }
                list.Add(h);
            }
            // the gloves: vertices whose heaviest bone is a hand's
            foreach (var smr in r.skinnedMeshes ?? new SkinnedMeshRenderer[0])
            {
                if (!smr || !smr.sharedMesh || !smr.sharedMesh.isReadable) continue;
                var map = smr.bones.Select(b => Array.IndexOf(r.bones, b)).ToArray();
                var bw = smr.sharedMesh.boneWeights;
                foreach (var h in list)
                {
                    var idx = new List<int>();
                    for (int i = 0; i < bw.Length; i++) { int b = bw[i].boneIndex0; if (b >= 0 && b < map.Length && h.bones.Contains(map[b])) idx.Add(i); }
                    if (idx.Count > (h.verts.Count)) { h.glove = smr; h.verts = idx; }
                }
            }
            foreach (var h in list) Log($"{h.side} hand: {h.bones.Count} bones ({string.Join(",", h.bones.OrderBy(x => x))}), glove {(h.glove ? h.glove.name : "none")} {h.verts.Count} vertices");
            hands = list.ToArray();
        }

        static void Fit(List<Vector3> pts, out Vector3 c, out Vector3 axis)
        {
            c = Vector3.zero; foreach (var p in pts) c += p; c /= Mathf.Max(1, pts.Count);
            double xx = 0, xy = 0, xz = 0, yy = 0, yz = 0, zz = 0;
            foreach (var p in pts) { var d = p - c; xx += d.x * d.x; xy += d.x * d.y; xz += d.x * d.z; yy += d.y * d.y; yz += d.y * d.z; zz += d.z * d.z; }
            Vector3 v = new Vector3(1, 0.1f, 0.1f).normalized;
            for (int k = 0; k < 60; k++) { v = new Vector3((float)(xx * v.x + xy * v.y + xz * v.z), (float)(xy * v.x + yy * v.y + yz * v.z), (float)(xz * v.x + yz * v.y + zz * v.z)); if (v.sqrMagnitude < 1e-20f) break; v.Normalize(); }
            axis = v;
        }

        // the grip of a hand in the world this frame: centre and outward axis (the node's live pose x its bind inverse)
        bool GripWorld(Hand h, OriginalSimulation.Results v, out Vector3 c, out Vector3 a)
        {
            c = a = Vector3.zero;
            if (h.gripNode >= v.bikeNodes) return false;
            var m = OriginalBikePose.FromRows(v.bikePose, h.gripNode * 16) * bike.hierarchyBind[h.gripNode].inverse;
            c = m.MultiplyPoint3x4(h.gripC); a = m.MultiplyVector(h.gripA).normalized; return true;
        }
        // the fist as RiderGripIK sees it: the centre of the four fingers' curl circles and their line (index -> pinky)
        static readonly int[] FingerJoints = { 22, 23, 24, 19, 20, 21, 16, 17, 18, 13, 14, 15 };
        bool Fist(int o, out Vector3 centre, out Vector3 dir)
        {
            var b = rt.rider.bones; centre = dir = Vector3.zero; Vector3 inner = Vector3.zero, outer = Vector3.zero;
            for (int f = 0; f < 4; f++)
            {
                Vector3 p = b[FingerJoints[3 * f] + o].position, q = b[FingerJoints[3 * f + 1] + o].position, r = b[FingerJoints[3 * f + 2] + o].position;
                Vector3 a = p - r, c = q - r, n = Vector3.Cross(a, c); if (n.sqrMagnitude < 1e-8f) return false;
                var cc = r + Vector3.Cross(a.sqrMagnitude * c - c.sqrMagnitude * a, n) / (2f * n.sqrMagnitude);
                centre += cc; if (f < 2) inner += cc; else outer += cc;
            }
            centre *= 0.25f; dir = (outer - inner).normalized; return true;
        }
        // per hand: the fist to its grip (RiderGripIK's grip: the distance to the segment, along from the end, the angle of the axes)
        bool FistToGrip(int k, out float dist, out float along, out float angle)
        {
            dist = along = angle = 0f;
            var g = RiderGripIK.For(bike); var grip = k == 0 ? g.right : g.left;
            if (grip == null || !grip.part || !Fist(k == 0 ? 0 : 29, out var hole, out var dir)) return false;
            Vector3 end = grip.part.TransformPoint(grip.end), axis = grip.part.TransformDirection(grip.axis);
            along = Vector3.Dot(hole - end, axis);
            dist = Vector3.Distance(hole, end + axis * Mathf.Clamp(along, -grip.length, 0f));
            angle = Vector3.Angle(dir, axis);
            return true;
        }

        // the throttle tube against the bars: OBJ_Throttle's turn from its rest relative to its parent node and how far that moves the
        // right grip's end (the game turns the node with the throttle)
        float throttleMaxMove, throttleMaxTurn; double throttleMoveSum; int throttleN;
        void Throttle(OriginalSimulation.Results v)
        {
            int th = Array.IndexOf(bike.hierarchyNodes, "OBJ_Throttle"), fk = Array.IndexOf(bike.hierarchyNodes, "OBJ_UPR_Forks");
            var g = RiderGripIK.For(bike).right;
            if (th < 0 || fk < 0 || th >= v.bikeNodes || g == null) return;
            var live = OriginalBikePose.FromRows(v.bikePose, fk * 16).inverse * OriginalBikePose.FromRows(v.bikePose, th * 16);
            var rest = bike.hierarchyBind[fk].inverse * bike.hierarchyBind[th];
            var d = live * rest.inverse;   // the turn in the forks' frame
            float move = (bike.hierarchyBind[fk].inverse.MultiplyPoint3x4(g.end) - d.MultiplyPoint3x4(bike.hierarchyBind[fk].inverse.MultiplyPoint3x4(g.end))).magnitude;
            float turn = Quaternion.Angle(Quaternion.identity, d.rotation);
            throttleMaxMove = Mathf.Max(throttleMaxMove, move); throttleMaxTurn = Mathf.Max(throttleMaxTurn, turn); throttleMoveSum += move; throttleN++;
            if (frame % 120 == 0) Log($"throttle: turned {F(turn)} deg, the grip end moved {F(move)} ft (max {F(throttleMaxTurn)} deg, {F(throttleMaxMove)} ft; mean {F((float)(throttleMoveSum / throttleN))} ft)");
        }

        void Measure(OriginalSimulation.Results v)
        {
            bool ik = Ik;
            if (!stats.TryGetValue(ik, out var st)) stats[ik] = st = new[] { new Stats(), new Stats() };
            for (int k = 0; k < hands.Length; k++)
            {
                var h = hands[k];
                if (!FistToGrip(h.side == "R" ? 0 : 1, out float fd, out float fa, out float fang)) continue;
                Vector3 W = rt.rider.bones[h.wrist].position, L = OriginalBikePose.FromRows(v.bikePose, h.locNode * 16).GetColumn(3);
                float wl = (W - L).magnitude;
                var o = v.onboardState;
                csv.Append(frame).Append(',').Append(ik ? 1 : 0).Append(',').Append(o.trick_active).Append(',').Append(o.front_grounded).Append(',').Append(o.rear_grounded).Append(',').Append(h.side)
                   .Append(',').Append(F(fd)).Append(',').Append(F(fa)).Append(',').Append(F(fang)).Append(',').Append(F(wl)).Append(',').Append(F(v.bike.speed)).Append('\n');
                if (o.trick_active != 0 || speed < 3f) continue;
                var s = st[k]; s.n++; s.palmR += fd; s.palmAlong += fa; s.wristR += fang; s.wristLoc += wl; s.palmRMax = Mathf.Max(s.palmRMax, fd);
                if (frame % 10 == 0 && GloveAlong(k, out float gIn, out float gOut)) { s.gn++; s.gloveIn += gIn; s.gloveOut += gOut; }
            }
        }

        // the glove of a hand along its grip (RiderGripIK's): the innermost / outermost of its hand vertices that lie within 0.25 ft of the
        // grip's axis, from the grip's end (negative: inward), w-gripwidth
        bool GloveAlong(int k, out float inner, out float outer)
        {
            inner = outer = 0f;
            var h = hands[k]; if (!h.glove || h.verts.Count == 0) return false;
            var g = RiderGripIK.For(bike); var grip = h.side == "R" ? g.right : g.left;
            if (grip == null || !grip.part) return false;
            Vector3 end = grip.part.TransformPoint(grip.end), axis = grip.part.TransformDirection(grip.axis);
            if (!baked) baked = new Mesh();
            h.glove.BakeMesh(baked, true);
            var vs = baked.vertices; var gm = h.glove.transform.localToWorldMatrix;
            float lo = float.MaxValue, hi = float.MinValue; int n = 0;
            foreach (int i in h.verts)
            {
                var w = gm.MultiplyPoint3x4(vs[i]); float t = Vector3.Dot(w - end, axis);
                if (Vector3.Cross(w - end, axis).magnitude > 0.25f) continue;
                lo = Mathf.Min(lo, t); hi = Mathf.Max(hi, t); n++;
            }
            if (n < 8) return false;
            inner = lo; outer = hi; return true;
        }

        // DUMP (analysis): the bars about each hand locator once, then the hands every `dumpEvery` frames, all in the model space of the
        // forks' pose (OBJ_UPR_Forks: the bars are bound to it) -> <shots>/grips-dump.csv (kind, frame, side, part / bone, node, x, y, z)
        StringBuilder dump = new StringBuilder("kind,frame,side,name,node,x,y,z\n"); bool dumpBike; int dumpEvery;
        void Dump(OriginalSimulation.Results v)
        {
            int fork = Array.IndexOf(bike.hierarchyNodes, "OBJ_UPR_Forks"); if (fork < 0) fork = Array.IndexOf(bike.hierarchyNodes, "OBJ_SteeringStem"); if (fork < 0 || fork >= v.bikeNodes) return;
            var toModel = (OriginalBikePose.FromRows(v.bikePose, fork * 16) * bike.hierarchyBind[fork].inverse).inverse;
            void Row(string kind, string side, string name, int node, Vector3 w) { var q = toModel.MultiplyPoint3x4(w); dump.Append(kind).Append(',').Append(frame).Append(',').Append(side).Append(',').Append(name).Append(',').Append(node).Append(',').Append(F(q.x)).Append(',').Append(F(q.y)).Append(',').Append(F(q.z)).Append('\n'); }
            if (!dumpBike)
            {
                dumpBike = true;
                foreach (var h in hands)
                {
                    Vector3 L = OriginalBikePose.FromRows(v.bikePose, h.locNode * 16).GetColumn(3);
                    Row("locator", h.side, h.locator, h.locNode, L);
                    var Lm = toModel.MultiplyPoint3x4(L);
                    foreach (var p in bike.parts)
                    {
                        if (!p.part || !p.part.gameObject.activeInHierarchy) continue;
                        foreach (var mf in p.part.GetComponentsInChildren<MeshFilter>())
                        {
                            var m = mf.sharedMesh; if (!m || !m.isReadable) continue;
                            var mw = mf.transform.localToWorldMatrix;
                            foreach (var vv in m.vertices) { var w = mw.MultiplyPoint3x4(vv); if ((toModel.MultiplyPoint3x4(w) - Lm).sqrMagnitude < 0.8f * 0.8f) Row("bike", h.side, p.name, p.node, w); }
                        }
                    }
                }
            }
            var r = rt.rider;
            foreach (var h in hands)
            {
                Row("locatorlive", h.side, h.locator, h.locNode, OriginalBikePose.FromRows(v.bikePose, h.locNode * 16).GetColumn(3));
                foreach (int b in h.bones.OrderBy(x => x)) Row("bone", h.side, r.bones[b].name, b, r.bones[b].position);
                Row("bone", h.side, r.bones[h.wrist - 1].name, h.wrist - 1, r.bones[h.wrist - 1].position);
                // the wrist's axes (unit points from the wrist)
                var wt = r.bones[h.wrist];
                Row("axis", h.side, "x", h.wrist, wt.position + wt.right * 0.1f); Row("axis", h.side, "y", h.wrist, wt.position + wt.up * 0.1f); Row("axis", h.side, "z", h.wrist, wt.position + wt.forward * 0.1f);
                if (!h.glove) continue;
                if (!baked) baked = new Mesh();
                h.glove.BakeMesh(baked, true);
                var vs = baked.vertices; var gm = h.glove.transform.localToWorldMatrix;
                foreach (int i in h.verts) Row("glove", h.side, "v" + i, -1, gm.MultiplyPoint3x4(vs[i]));
            }
        }

        void Summary(string when)
        {
            foreach (var kv in stats)
                for (int k = 0; k < hands.Length; k++)
                {
                    var s = kv.Value[k]; if (s.n == 0) continue;
                    var gg = RiderGripIK.For(bike); var grip = hands[k].side == "R" ? gg.right : gg.left;
                    string glove = s.gn == 0 ? "" : $"; the glove {F((float)(s.gloveIn / s.gn))} .. {F((float)(s.gloveOut / s.gn))} along (its middle {F((float)((s.gloveIn + s.gloveOut) / 2 / s.gn))})";
                    Log($"SUMMARY {when} ik {(kv.Key ? "on" : "off")} {hands[k].side} hand, {s.n} riding frames: the fist {F((float)(s.palmR / s.n))} ft off its grip (max {F(s.palmRMax)}), {F((float)(s.palmAlong / s.n))} along from the grip's end, its axis {F((float)(s.wristR / s.n))} degrees from the grip's; the wrist {F((float)(s.wristLoc / s.n))} from the hand locator{glove}; the grip {(grip == null ? "none" : F(grip.length) + " ft long (its middle " + F(-grip.length / 2) + ")")}");
                }
        }

        // ---------------------------------------------------------------------------------------------------------------------------
        // the autopilot (GhostRideProbe's) + the trick gestures
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
            // tricks: in the air for 8+ frames, one gesture per air
            bool air = last != null && last.onboardState.valid != 0 && last.onboardState.front_grounded == 0 && last.onboardState.rear_grounded == 0;
            airFrames = air ? airFrames + 1 : 0;
            if (!air) trickUsedThisAir = false;
            if (tricks && air && airFrames >= 8 && !trickUsedThisAir && gesture == null)
            {
                var t = Tricks[trickIndex++ % Tricks.Length]; gesture = t.input; gestureStep = 0; gestureFrames = 0; trickUsedThisAir = true;
                Log($"trick attempt {t.name} ({t.input}) at frame {frame}, speed {F(speed)}");
            }
            if (gesture != null)
            {
                s.buttons |= (uint)(1 << (int)GamepadButton.LeftShoulder);
                if (gestureStep < gesture.Length)
                {
                    float ang = (gesture[gestureStep] - '0') * 45f * Mathf.Deg2Rad;
                    s.rightStick = new Vector2(Mathf.Sin(ang), Mathf.Cos(ang));
                    if (++gestureFrames >= 4) { gestureStep++; gestureFrames = 0; }
                }
                else if (++gestureFrames > 30 || !air) gesture = null;
            }
            InputSystem.QueueStateEvent(pad, s);
        }
        bool tricks; int trickShot, trickCount; float fpvDown;

        void LateUpdate()
        {
            // the Dynamic FPV view looking further down at the bars (the report's picture): its camera turned down about its own right axis
            if (fpvDown > 0f && rt && rt.chaseCamera && rt.cameraMode == 1) { var ct = rt.chaseCamera.transform; ct.rotation = Quaternion.AngleAxis(fpvDown, ct.right) * ct.rotation; }
            if (overrideCam && rt && rt.chaseCamera && rt.chassis && hands != null && last != null)
            {
                // aim at the bars' middle (the two grip centres) or one grip (camAim "R" / "L"), from an offset in the chassis frame
                Vector3 aim = Vector3.zero; int n = 0;
                foreach (var h in hands) if ((camAim == null || camAim == h.side) && GripWorld(h, last, out var c, out _)) { aim += c; n++; }
                if (n > 0)
                {
                    aim /= n;
                    var cam = rt.chaseCamera;
                    cam.transform.position = aim + rt.chassis.rotation * camOffset;
                    cam.transform.rotation = Quaternion.LookRotation(aim - cam.transform.position, rt.chassis.up);
                    cam.fieldOfView = camFov; cam.nearClipPlane = 0.05f;
                }
            }
            if (pendingShot != null) { OffscreenShot(rt ? rt.chaseCamera : Camera.main, pendingShot); pendingShot = null; }
        }
        string camAim, pendingShot;

        // -batchmode (no window): the camera rendered into a target of REFLEX_SHOT_SIZE (default 1600x900) a few times (the temporal AA's history
        // settles), read back and saved, after every other LateUpdate (the pose, the camera) - as tests/lighting_showcase does (w-gripwidth)
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
                if (System.Environment.GetEnvironmentVariable("REFLEX_SHOT_FLIP") == "1")   // (offscreen renders come back upright: the display gamma ramp no longer turns them, ReflexGammaOutput)
                {
                    var px = t.GetPixels32(); var o = new Color32[px.Length];
                    for (int y = 0; y < H; y++) Array.Copy(px, y * W, o, (H - 1 - y) * W, W);
                    t.SetPixels32(o); t.Apply(false);
                }
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
        // a pair: GRIP IK off, then on (the next frames), when the build has it
        IEnumerator Pair(string name)
        {
            Ik = false; yield return Frames(2); yield return Shot(name + "-off");
            if (ikField != null) { Ik = true; yield return Frames(2); yield return Shot(name + "-on"); }
        }
        IEnumerator Tap(GamepadButton b)
        {
            InputSystem.QueueStateEvent(pad, new GamepadState(b)); yield return Frames(3);
            InputSystem.QueueStateEvent(pad, new GamepadState()); yield return Frames(4);
        }

        IEnumerator Run()
        {
            pad = InputSystem.AddDevice<Gamepad>("grip probe pad");
            OpenShell shell = null;
            for (float t = 0; t < 120f && !(shell = OpenShell.Instance); t += Time.unscaledDeltaTime) yield return null;
            if (!shell) { Log("FAIL the shell did not start"); Quit(); yield break; }
            yield return Seconds(2f);
            string evName = Environment.GetEnvironmentVariable("REFLEX_GRIP_EVENT") ?? "National/NAT_Concord";
            var ev = ReflexEvents.Find(evName) ?? ReflexEvents.All.First(e => e.spec.type == "National" && e.location == "NAT_Concord");
            ReflexWorldFiles.Garage garage = null;
            var gs = Environment.GetEnvironmentVariable("REFLEX_GRIP_GARAGE");
            if (!string.IsNullOrEmpty(gs))
            {
                // "<AIRiderId>,<AIVehicleId>,<VehicleTypeId>": a pro's rider (his default gear) and one of his vehicles (AIRiderVehicle)
                var p = gs.Split(',').Select(x => int.Parse(x.Trim(), Inv)).ToArray();
                int gear = -1;
                try { var row = ReflexEvents.Tables["AIRider"].FirstOrDefault(x => Convert.ToInt32(x["AIRiderId"], Inv) == p[0]); if (row != null && row.TryGetValue("DefaultRiderGearSetId", out var gv)) gear = Convert.ToInt32(gv, Inv); } catch { }
                garage = new ReflexWorldFiles.Garage { riderOwner = ReflexWorldFiles.Garage.AI, riderId = p[0], riderGearSet = gear, vehicleOwner = ReflexWorldFiles.Garage.AI, vehicleId = p[1], vehicleTypeId = p.Length > 2 ? p[2] : -1, vehicleClass = -1 };
                if (garage.vehicleTypeId >= 0) { var g2 = ev.GarageFor(garage.vehicleTypeId); if (g2 != null) garage.vehicleClass = g2.vehicleClass; }
            }
            // REFLEX_GRIP_TYPE=<VehicleTypeId>: the event's default garage for that vehicle type (VehicleSelect's first pro with one)
            if (garage == null && int.TryParse(Environment.GetEnvironmentVariable("REFLEX_GRIP_TYPE"), out int vtype)) garage = ev.GarageFor(vtype);
            try
            {
                var T = ReflexEvents.Tables;
                string Row(Dictionary<string, object> r) => string.Join(" ", r.Select(kv => kv.Key + "=" + kv.Value));
                foreach (var m in T["VehicleModel"]) Log("VehicleModel " + Row(m));
                int I(Dictionary<string, object> r, string k) { try { return r != null && r.TryGetValue(k, out var o) ? Convert.ToInt32(o, Inv) : -1; } catch { return -1; } }
                foreach (var m in T["AIRiderVehicle"]) { var av = T["AIVehicle"].FirstOrDefault(x => I(x, "AIVehicleId") == I(m, "AIVehicleId")); Log("AIRiderVehicle " + Row(m) + " | model " + I(av, "VehicleModelId")); }
            }
            catch (Exception e) { Log("tables: " + e.Message); }
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
            Ik = false;
            Describe();
            drive = true;
            // ride, alternating GRIP IK each second (the summary per state); the dump of the hands (GRIP IK off) every 45 frames meanwhile
            dumpEvery = 45;
            for (int i = 0; i < 10; i++) { Ik = (i & 1) == 1; dumpEvery = Ik ? 0 : 45; yield return Seconds(1f); }
            dumpEvery = 0;
            if (!string.IsNullOrEmpty(shots)) File.WriteAllText(Path.Combine(shots, "grips-dump.csv"), dump.ToString());
            Ik = false;
            // the Dynamic FPV view looking down at the bars as in the report (REFLEX_GRIP_FPV_PITCH, REFLEX_GRIP_FPV_FOV)
            if (rt.onboard)
            {
                if (float.TryParse(Environment.GetEnvironmentVariable("REFLEX_GRIP_FPV_PITCH"), NumberStyles.Float, Inv, out float fp)) rt.onboard.pitch = fp;
                if (float.TryParse(Environment.GetEnvironmentVariable("REFLEX_GRIP_FPV_FOV"), NumberStyles.Float, Inv, out float ff)) rt.onboard.fov = ff;
            }
            // shots: Dynamic FPV, the follow camera, the close-ups
            rt.SetCameraMode(1, false); yield return Seconds(1.5f);
            yield return Pair("fpv-a"); yield return Seconds(2f); yield return Pair("fpv-b");
            fpvDown = 28f; yield return Frames(3); yield return Pair("fpv-down-a"); yield return Seconds(2f); yield return Pair("fpv-down-b"); fpvDown = 0f;
            rt.SetCameraMode(2, false); yield return Seconds(1f);
            yield return Pair("chase");
            overrideCam = true;
            foreach (var (name, off, fov) in new[] { ("tp-front", new Vector3(0.6f, 0.9f, 3.2f), 38f), ("tp-above", new Vector3(0f, 2.4f, 1.6f), 50f),
                                                     ("tp-right", new Vector3(2.6f, 0.5f, 0.4f), 34f), ("tp-left", new Vector3(-2.6f, 0.5f, 0.4f), 34f),
                                                     ("tp-rear-right", new Vector3(1.6f, 1.2f, -2.0f), 36f) })
            {
                camOffset = off; camFov = fov; yield return Frames(3); yield return Pair(name);
            }
            // each hand on its grip, close (w-gripwidth): from in front and outside, slightly above
            foreach (var (name, aim, off, fov) in new[] { ("tp-grip-r", "R", new Vector3(0.9f, 0.5f, 1.4f), 30f), ("tp-grip-l", "L", new Vector3(-0.9f, 0.5f, 1.4f), 30f),
                                                          ("tp-grip-r-top", "R", new Vector3(0.1f, 1.6f, 0.5f), 30f), ("tp-grip-l-top", "L", new Vector3(-0.1f, 1.6f, 0.5f), 30f) })
            {
                camAim = aim; camOffset = off; camFov = fov; yield return Frames(3); yield return Pair(name);
            }
            camAim = null;
            overrideCam = false;
            Summary("riding");
            // tricks: 45 s more riding, one gesture per air
            stats.Clear(); tricks = Environment.GetEnvironmentVariable("REFLEX_GRIP_TRICKS") != "0"; Ik = ikField != null;
            rt.SetCameraMode(2, false);
            if (tricks) yield return Seconds(45f);
            tricks = false;
            Summary("with tricks (riding frames only)");
            drive = false;
            if (!string.IsNullOrEmpty(shots)) File.WriteAllText(Path.Combine(shots, "grips.csv"), csv.ToString());
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

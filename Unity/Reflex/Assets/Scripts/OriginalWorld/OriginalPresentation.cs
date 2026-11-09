using System;
using System.Collections.Generic;
using Reflex.Port.RxCore.Bridge;
using UnityEngine;

namespace Reflex.Port.OriginalWorld
{
    // Presentation interpolation (frame pacing). The original game runs at its own 60 Hz cadence (Engine::UpdateTiming floors the
    // frame time at 1/60 s: measured in the running world, 120 Hz frames run the game twice as fast), on the simulation thread
    // (OriginalSimulation), while Unity draws at the display rate (60 / 120 / 144 Hz, uneven frame times). Showing the last
    // completed frame repeats or skips game frames (judder); here the display shows the game at a presentation time between the
    // last two completed frames:
    //  - every completed frame is stamped with the virtual clock it ends at (OriginalSimulation.Results.simTime: the time submitted
    //    so far, which follows the wall clock);
    //  - the presentation time = now - delay, where delay is the smallest that keeps it behind the newest completed frame over the
    //    last half second (how late frames are available: about one game frame at 120 / 144 Hz, less in 60 Hz lockstep); it never
    //    goes backwards and never passes the newest frame (no extrapolation: nothing the game did not compute is shown);
    //  - continuous state is blended at alpha = (t - ta) / (tb - ta): bodies (position lerp, quaternion slerp), every node / bone /
    //    ambient world matrix (decomposed to position, rotation as LookRotation(Z, Y) like every consumer reads it, per-axis scale
    //    and mirror; slerp; recomposed), the game camera (position, rotation, FOV); a jump (> 10 m in a frame: a respawn, a camera
    //    cut) or a mismatched layout shows the nearer frame; particles (no per-particle identity in the export) take the nearer frame.
    //  - discrete results (audio, terrain, tracks, mud, race / HUD state) are applied once, when collected (OriginalWorldRuntime).
    // The simulation is untouched: same inputs, the same frames; only what is drawn between them changes.
    public sealed class OriginalPresentation
    {
        public const float Step = 1f / 60f;            // the game's frame (Engine 0xd728d0: dt floored at 1/60 s)
        public const double MaxDelay = 0.1, Margin = 0.0005, Window = 0.5, Release = 0.05;
        readonly OriginalSimulation sim;
        readonly IReflexSimDriver driver;   // the selected path (legacy: its OriginalSimulation; open: the core driver)
        readonly List<OriginalSimulation.Results> history = new List<OriginalSimulation.Results>();
        readonly Queue<(double time, double need)> needs = new Queue<(double, double)>();
        double clock;                                  // real time seen by Present (s)
        public readonly OriginalSimulation.Results view = new OriginalSimulation.Results();
        public bool Interpolate = true;                // false: the newest frame as it completes (the old presentation)

        public OriginalPresentation(OriginalSimulation simulation) { sim = simulation; sim?.KeepResults(); view.bike = ReflexBikeState.Create(); }
        /// The selected driver (REFLEX_SIM=open or legacy); the ownership of results (KeepResults / Recycle) is forwarded to it.
        public OriginalPresentation(IReflexSimDriver simulation) { driver = simulation; sim = simulation?.Legacy; driver?.KeepResults(); view.bike = ReflexBikeState.Create(); }

        void Recycle(OriginalSimulation.Results r) { if (driver != null) driver.Recycle(r); else sim?.Recycle(r); }

        /// The presentation time shown (s, on the simulation clock), its advance this Unity frame, the blend between the two frames.
        public double Time { get; private set; } = double.NaN;
        public double Delay { get; private set; } = Step;
        public float Alpha { get; private set; }
        public float PresentedDt { get; private set; }
        public bool Held { get; private set; }         // the presentation time was clamped at the newest frame (the simulation was late)
        public OriginalSimulation.Results A { get; private set; }
        public OriginalSimulation.Results B { get; private set; }
        public OriginalSimulation.Results Newest => history.Count > 0 ? history[history.Count - 1] : null;

        /// A completed frame (main thread, right after OriginalSimulation.Collect).
        public void Push(OriginalSimulation.Results r)
        {
            if (r == null) return;
            if (history.Count > 0 && r.simTime <= history[history.Count - 1].simTime) { Recycle(history[history.Count - 1]); history.RemoveAt(history.Count - 1); }
            history.Add(r);
            while (history.Count > 4) { Recycle(history[0]); history.RemoveAt(0); }
        }

        public void Clear() { foreach (var r in history) Recycle(r); history.Clear(); needs.Clear(); Time = double.NaN; A = B = null; view.vehicleAudio.Clear(); vehicleAudioTime = double.NaN; }
        double vehicleAudioTime = double.NaN;   // the frame view.vehicleAudio was copied from (w-fb-hud)

        /// Chooses the presentation time for `now` (the simulation clock at this Unity frame: submitted time + the accumulator) and
        /// blends the view; false before the first frame.
        public bool Present(double now, float realDt)
        {
            Held = false;
            if (history.Count == 0) return false;
            var newest = history[history.Count - 1];
            double tp;
            if (!Interpolate) { tp = newest.simTime; Delay = now - tp; }
            else
            {
                clock += Math.Max(0f, realDt);
                double need = now - newest.simTime;
                needs.Enqueue((clock, need));
                while (needs.Count > 1 && needs.Peek().time < clock - Window) needs.Dequeue();
                double target = 0; foreach (var n in needs) target = Math.Max(target, n.need);
                target = Math.Min(target + Margin, MaxDelay);
                Delay = target >= Delay ? target : Math.Max(target, Delay - realDt * Release);
                tp = now - Delay;
            }
            if (!double.IsNaN(Time) && tp < Time) tp = Time;                 // never backwards
            if (tp >= newest.simTime) { Held = Interpolate && tp > newest.simTime + 1e-6; tp = newest.simTime; }   // never past the newest
            PresentedDt = double.IsNaN(Time) ? 0f : (float)(tp - Time);
            Time = tp;
            // the frames around tp: a = the last at or before it (older ones are no longer needed), b = the next
            int ai = 0;
            for (int i = 0; i < history.Count; i++) if (history[i].simTime <= tp) ai = i;
            while (ai > 0) { Recycle(history[0]); history.RemoveAt(0); ai--; }
            var a = history[0]; var b = history.Count > 1 ? history[1] : a;
            float t = b == a || b.simTime <= a.simTime ? 1f : Mathf.Clamp01((float)((tp - a.simTime) / (b.simTime - a.simTime)));
            if (tp <= a.simTime && b != a) t = 0f;
            A = a; B = b; Alpha = t;
            Blend(a, b, t);
            return true;
        }

        // ------------------------------------------------------------------------------------------------ the blended view
        void Blend(OriginalSimulation.Results a, OriginalSimulation.Results b, float t)
        {
            var v = view; var near = t < 0.5f ? a : b;
            v.replay = b.replay; v.nativeFrame = b.nativeFrame;
            // the local bike: bodies, velocity, inputs (discrete fields from b)
            var bs = v.bike; var sa = a.bike; var sb = b.bike;
            bs.speed = Mathf.Lerp(sa.speed, sb.speed, t); bs.gear = sb.gear; bs.frames = sb.frames; bs.reserved = sb.reserved;
            PoseBlend.Bodies(sa.renderPosition, sa.renderOrientation, sb.renderPosition, sb.renderOrientation, t, bs.renderPosition, bs.renderOrientation, 4);
            PoseBlend.Bodies(sa.position, sa.orientation, sb.position, sb.orientation, t, bs.position, bs.orientation, 4);
            PoseBlend.Lerp(sa.velocity, sb.velocity, t, bs.velocity, 4); PoseBlend.Lerp(sa.inputs, sb.inputs, t, bs.inputs, 8);
            v.bike = bs;
            // the local vehicle (an ATV's six bodies)
            v.hasVehicle = b.hasVehicle; var vb = b.vehicle; var va = a.vehicle;
            if (v.vehicle.renderPosition == null) { v.vehicle.renderPosition = new float[24]; v.vehicle.renderOrientation = new float[24]; }
            var vp = v.vehicle.renderPosition; var vo = v.vehicle.renderOrientation;
            v.vehicle = vb; v.vehicle.renderPosition = vp; v.vehicle.renderOrientation = vo;
            if (vb.renderPosition != null)
            {
                if (a.hasVehicle && va.renderPosition != null && va.type == vb.type) PoseBlend.Bodies(va.renderPosition, va.renderOrientation, vb.renderPosition, vb.renderOrientation, t, vp, vo, 6);
                else { Array.Copy(vb.renderPosition, vp, 24); Array.Copy(vb.renderOrientation, vo, 24); }
                v.vehicle.speed = a.hasVehicle ? Mathf.Lerp(va.speed, vb.speed, t) : vb.speed; v.vehicle.rpm = a.hasVehicle ? Mathf.Lerp(va.rpm, vb.rpm, t) : vb.rpm;
            }
            // bike model nodes, rider bones
            v.bikeNodes = b.bikeNodes;
            if (a.bikeNodes == b.bikeNodes) PoseBlend.Matrices(a.bikePose, b.bikePose, t, v.bikePose, b.bikeNodes); else Array.Copy(b.bikePose, v.bikePose, Math.Max(0, b.bikeNodes) * 16);
            v.riderBones = b.riderBones;
            if (b.riderBones > 0) { if (a.riderBones == b.riderBones) PoseBlend.Matrices(a.riderPose, b.riderPose, t, v.riderPose, b.riderBones); else Array.Copy(b.riderPose, v.riderPose, b.riderBones * 16); }
            // the game camera
            v.hasCamera = b.hasCamera; v.camera = b.camera;
            if (a.hasCamera && b.hasCamera) v.camera = PoseBlend.Camera(a.camera, b.camera, t);
            // scenery the original moves (start gate)
            if (v.objects.Length != b.objects.Length) { v.objects = new float[b.objects.Length]; v.objectsValid = new bool[b.objectsValid.Length]; }
            Array.Copy(b.objectsValid, v.objectsValid, b.objectsValid.Length);
            if (a.objects.Length == b.objects.Length)
                for (int i = 0; i < b.objectsValid.Length; i++) { if (a.objectsValid[i] && b.objectsValid[i]) PoseBlend.Matrix(a.objects, b.objects, t, v.objects, i * 16); else Array.Copy(b.objects, i * 16, v.objects, i * 16, 16); }
            else Array.Copy(b.objects, v.objects, b.objects.Length);
            // the other riders
            BlendSlots(a, b, t);
            // ambient entities
            v.ambient = BlendAmbient(a.ambient, b.ambient, t);
            // particles: the nearer frame (the export has no per-particle identity to blend by)
            v.particles = near.particles; v.particleCount = near.particleCount; v.particleInfo = near.particleInfo;
            // the rest as the newest frame has it
            v.race = b.race; v.hud = b.hud; v.hasHud = b.hasHud; v.drivers = b.drivers; v.driverCount = b.driverCount;
            v.waypoint = b.waypoint; v.hasWaypoint = b.hasWaypoint;
            v.powercross = b.powercross; v.hasPowercross = b.hasPowercross;
            v.onboardState = b.onboardState; v.raceLineClosed = b.raceLineClosed;
            v.raceLine = b.raceLine; v.raceLinePoints = b.raceLinePoints; v.mud = b.mud; v.mudCount = b.mudCount; v.podium = b.podium; v.sceneryModels = b.sceneryModels;
            v.bikeNodeFlags = b.bikeNodeFlags; v.bikeNodeFlagCount = b.bikeNodeFlagCount; v.slotNodeFlags = b.slotNodeFlags; v.slotNodeFlagCounts = b.slotNodeFlagCounts;   // node visibility (ReflexVehicles.NodeFlags)
            // the vehicle sound layer's snapshot of the newer frame (w-fb-hud): the presented frame is what the world applies (ApplyView ->
            // TryGetEngine: the MODERN gauge's REV GAUGE RPM ring) and it never carried one, so the gauge always fell
            // back to the speed ring in a real ride (the gallery's synthetic models had hasRpm). Copied when b is another frame (no garbage).
            if (b.simTime != vehicleAudioTime) { v.vehicleAudio.CopyFrom(b.vehicleAudio); vehicleAudioTime = b.simTime; }
            v.slotMilliseconds = b.slotMilliseconds; v.milliseconds = b.milliseconds; v.simTime = Time; v.dt = PresentedDt;
            v.gameDt = b.gameDt; v.gameTime = Mathf.Lerp(a.gameTime, b.gameTime, t);
        }

        void BlendSlots(OriginalSimulation.Results a, OriginalSimulation.Results b, float t)
        {
            var v = view; int n = b.slotCount;
            if (v.slots.Length < n)
            {
                v.slots = new int[n]; v.slotBikeNodes = new int[n]; v.slotRiderBones = new int[n];
                v.slotBikePose = new float[n][]; v.slotRiderPose = new float[n][];
                for (int i = 0; i < n; i++) { v.slotBikePose[i] = new float[OriginalSimulation.MaxSlotNodes * 16]; v.slotRiderPose[i] = new float[OriginalRider.OriginalBoneCount * 16]; }
            }
            v.slotCount = n;
            for (int i = 0; i < n; i++)
            {
                v.slots[i] = b.slots[i]; int nodes = b.slotBikeNodes[i]; v.slotBikeNodes[i] = nodes;
                int ai = i < a.slotCount && a.slots[i] == b.slots[i] ? i : Array.IndexOf(a.slots, b.slots[i], 0, a.slotCount);
                bool bikeBlended = ai >= 0 && a.slotBikeNodes[ai] == nodes && nodes > 0;
                if (bikeBlended) PoseBlend.Matrices(a.slotBikePose[ai], b.slotBikePose[i], t, v.slotBikePose[i], nodes);
                else if (nodes > 0) Array.Copy(b.slotBikePose[i], v.slotBikePose[i], nodes * 16);
                int bones = b.slotRiderBones[i];
                v.slotRiderBones[i] = bones;
                if (bones <= 0) continue;   // not evaluated this frame (a far rider): OriginalOpponents carries its last pose with the bike
                if (ai >= 0 && a.slotRiderBones[ai] == bones) PoseBlend.Matrices(a.slotRiderPose[ai], b.slotRiderPose[i], t, v.slotRiderPose[i], bones);
                else if (nodes > 0 && bikeBlended) PoseBlend.Carry(b.slotRiderPose[i], bones, b.slotBikePose[i], v.slotBikePose[i], v.slotRiderPose[i]);   // b's skeleton on the blended bike
                else Array.Copy(b.slotRiderPose[i], v.slotRiderPose[i], bones * 16);
            }
        }

        OriginalAmbient.Frame ambientView;
        OriginalAmbient.Frame BlendAmbient(OriginalAmbient.Frame a, OriginalAmbient.Frame b, float t)
        {
            if (b == null) return null;
            if (a == null || a == b || a.count != b.count || a.matrixCount != b.matrixCount) return b;
            const int W = OriginalAmbient.EntitySize;
            for (int i = 0; i < b.count; i++)   // the same entities (object, first matrix, bones) in the same order
                for (int k = 0; k < 24; k++) { int o = i * W + k; if (k < 4 || (k >= 16 && k < 24)) { if (a.entities[o] != b.entities[o]) return b; } }
            ambientView ??= new OriginalAmbient.Frame();
            Buffer.BlockCopy(b.entities, 0, ambientView.entities, 0, b.count * W);
            ambientView.count = b.count; ambientView.matrixCount = b.matrixCount; ambientView.info = b.info;
            Array.Copy(b.nodeFlagStart, ambientView.nodeFlagStart, b.count); Array.Copy(b.nodeFlagCount, ambientView.nodeFlagCount, b.count);
            Array.Copy(b.nodeFlags, ambientView.nodeFlags, b.nodeFlags.Length);
            ambientView.flashCount = b.flashCount; if (b.flashCount > 0) Array.Copy(b.flashes, ambientView.flashes, b.flashCount);   // the newest frame's flashes (not blended)
            PoseBlend.Matrices(a.matrices, b.matrices, t, ambientView.matrices, b.matrixCount);
            return ambientView;
        }

        // ------------------------------------------------------------------------------------------------ pacing log
        System.IO.StreamWriter log; double logStart = double.NaN;
        /// Per Unity frame: real time, real dt, the presentation time, its advance, delay, alpha, held, the frames' stamps, game clock,
        /// simulation cost (OriginalWorldRuntime: REFLEX_PACING_LOG=<file>).
        public void OpenLog(string path)
        {
            try { log = new System.IO.StreamWriter(path, false) { AutoFlush = false }; log.WriteLine("real,realDt,present,presentDt,delay,alpha,held,a,b,newest,frames,merged,gameTime,simMs,frameMs,submissionClock,frameStartClock,collectionClock,presentationObservedClock,padMask,pad0Lx,pad0Ly,pad0Rx,pad0Ry,pad0Lt,pad0Rt,pad0Buttons,metadata"); }
            catch (Exception e) { Debug.LogWarning("pacing log: " + e.Message); log = null; }
        }
        // presentationObservedClock is this scheduling observation after Present; it does not measure display/photon latency.
        // In interpolated mode A/B/alpha identify the displayed blend; input columns describe Newest.
        public void LogFrame(double real, float realDt)
        {
            if (log == null || A == null) return;
            if (double.IsNaN(logStart)) logStart = real;
            var n = Newest;
            log.WriteLine(FormattableString.Invariant($"{real - logStart:0.000000},{realDt:0.000000},{Time:0.000000},{PresentedDt:0.000000},{Delay:0.000000},{Alpha:0.0000},{(Held ? 1 : 0)},{A.simTime:0.000000},{B.simTime:0.000000},{n.simTime:0.000000},{n.framesRun},{(n.merged ? 1 : 0)},{view.gameTime:0.000000},{n.milliseconds:0.000},{n.frameMilliseconds:0.000},{n.inputClock:0.000000},{n.frameStartClock:0.000000},{n.collectionClock:0.000000},{OriginalSimulation.Clock:0.000000},{n.appliedPadCsv ?? "0,0,0,0,0,0,0,0"},{n.corneringMetadata}"));
        }
        public void CloseLog() { try { log?.Flush(); log?.Dispose(); } catch { } log = null; }
    }

    // Blends of the original's pose data. Matrices are the game's row-vector 4x4 (D3D: X axis 0..2, Y 4..6, Z 8..10, translation
    // 12..14; 3, 7, 11, 15 padding, taken from b). The rotation is the one every consumer reads (Quaternion.LookRotation(Z, Y):
    // OriginalBikePose, OriginalRider, the ambient pose job), the scale per axis with the mirror sign (crowd matrices are mirrored).
    public static class PoseBlend
    {
        public const float JumpSqr = 10f * 10f;        // 10 m within one frame (600 m/s): a respawn or a cut, not motion

        public static void Lerp(float[] a, float[] b, float t, float[] o, int n) { for (int i = 0; i < n; i++) o[i] = a[i] + (b[i] - a[i]) * t; }

        static bool Finite(float x) => !float.IsNaN(x) && !float.IsInfinity(x);

        /// n bodies: positions (x, y, z, w padding) lerped, orientations (quaternions x, y, z, w) slerped; a jump takes the nearer.
        public static void Bodies(float[] pa, float[] qa, float[] pb, float[] qb, float t, float[] po, float[] qo, int n)
        {
            for (int i = 0; i < n; i++)
            {
                int k = i * 4;
                float dx = pb[k] - pa[k], dy = pb[k + 1] - pa[k + 1], dz = pb[k + 2] - pa[k + 2];
                bool jump = !(dx * dx + dy * dy + dz * dz <= JumpSqr);
                if (jump) { var s = t < 0.5f ? pa : pb; var q = t < 0.5f ? qa : qb; for (int j = 0; j < 4; j++) { po[k + j] = s[k + j]; qo[k + j] = q[k + j]; } continue; }
                po[k] = pa[k] + dx * t; po[k + 1] = pa[k + 1] + dy * t; po[k + 2] = pa[k + 2] + dz * t; po[k + 3] = pb[k + 3];
                Slerp(qa[k], qa[k + 1], qa[k + 2], qa[k + 3], qb[k], qb[k + 1], qb[k + 2], qb[k + 3], t, out qo[k], out qo[k + 1], out qo[k + 2], out qo[k + 3]);
            }
        }

        public static void Matrices(float[] a, float[] b, float t, float[] o, int count) { for (int i = 0; i < count; i++) Matrix(a, b, t, o, i * 16); }

        /// One matrix at element offset k.
        public static void Matrix(float[] a, float[] b, float t, float[] o, int k)
        {
            if (t >= 1f) { Array.Copy(b, k, o, k, 16); return; }
            if (t <= 0f) { Array.Copy(a, k, o, k, 16); for (int j = 3; j < 16; j += 4) o[k + j] = b[k + j]; return; }
            if (!Decompose(a, k, out float pax, out float pay, out float paz, out float qax, out float qay, out float qaz, out float qaw, out float sax, out float say, out float saz) ||
                !Decompose(b, k, out float pbx, out float pby, out float pbz, out float qbx, out float qby, out float qbz, out float qbw, out float sbx, out float sby, out float sbz) ||
                (sax < 0) != (sbx < 0) ||
                !((pbx - pax) * (pbx - pax) + (pby - pay) * (pby - pay) + (pbz - paz) * (pbz - paz) <= JumpSqr))
            { Array.Copy(t < 0.5f ? a : b, k, o, k, 16); return; }
            Slerp(qax, qay, qaz, qaw, qbx, qby, qbz, qbw, t, out float x, out float y, out float z, out float w);
            float sx = sax + (sbx - sax) * t, sy = say + (sby - say) * t, sz = saz + (sbz - saz) * t;
            float xx = x * x, yy = y * y, zz = z * z, xy = x * y, xz = x * z, yz = y * z, wx = w * x, wy = w * y, wz = w * z;
            o[k + 0] = (1 - 2 * (yy + zz)) * sx; o[k + 1] = 2 * (xy + wz) * sx; o[k + 2] = 2 * (xz - wy) * sx;
            o[k + 4] = 2 * (xy - wz) * sy; o[k + 5] = (1 - 2 * (xx + zz)) * sy; o[k + 6] = 2 * (yz + wx) * sy;
            o[k + 8] = 2 * (xz + wy) * sz; o[k + 9] = 2 * (yz - wx) * sz; o[k + 10] = (1 - 2 * (xx + yy)) * sz;
            o[k + 12] = pax + (pbx - pax) * t; o[k + 13] = pay + (pby - pay) * t; o[k + 14] = paz + (pbz - paz) * t;
            o[k + 3] = b[k + 3]; o[k + 7] = b[k + 7]; o[k + 11] = b[k + 11]; o[k + 15] = b[k + 15];
        }

        // position, rotation = LookRotation(Z, Y) (z = |Z|, x = |Y x z|, y = z x x), scales |X| (negative when mirrored), |Y|, |Z|
        static bool Decompose(float[] m, int k, out float px, out float py, out float pz, out float qx, out float qy, out float qz, out float qw, out float sx, out float sy, out float sz)
        {
            px = m[k + 12]; py = m[k + 13]; pz = m[k + 14]; qx = qy = qz = 0; qw = 1;
            float Xx = m[k], Xy = m[k + 1], Xz = m[k + 2], Yx = m[k + 4], Yy = m[k + 5], Yz = m[k + 6], Zx = m[k + 8], Zy = m[k + 9], Zz = m[k + 10];
            sx = Mathf.Sqrt(Xx * Xx + Xy * Xy + Xz * Xz); sy = Mathf.Sqrt(Yx * Yx + Yy * Yy + Yz * Yz); sz = Mathf.Sqrt(Zx * Zx + Zy * Zy + Zz * Zz);
            if (!(sx > 1e-8f && sy > 1e-8f && sz > 1e-8f) || !Finite(px) || !Finite(py) || !Finite(pz) || !Finite(sx + sy + sz)) return false;
            // mirror: Unity's Cross(right, up) = forward for a proper rotation
            float cx = Xy * Yz - Xz * Yy, cy = Xz * Yx - Xx * Yz, cz = Xx * Yy - Xy * Yx;
            if (cx * Zx + cy * Zy + cz * Zz < 0) sx = -sx;
            float zx = Zx / sz, zy = Zy / sz, zz = Zz / sz;
            float xx = Yy * zz - Yz * zy, xy = Yz * zx - Yx * zz, xz = Yx * zy - Yy * zx;
            float xl = Mathf.Sqrt(xx * xx + xy * xy + xz * xz); if (!(xl > 1e-8f)) return false;
            xx /= xl; xy /= xl; xz /= xl;
            float yx = zy * xz - zz * xy, yy = zz * xx - zx * xz, yz = zx * xy - zy * xx;
            // rotation matrix columns x, y, z -> quaternion (Shepperd)
            float tr = xx + yy + zz;
            if (tr > 0) { float s = Mathf.Sqrt(tr + 1) * 2; qw = 0.25f * s; qx = (yz - zy) / s; qy = (zx - xz) / s; qz = (xy - yx) / s; }
            else if (xx > yy && xx > zz) { float s = Mathf.Sqrt(1 + xx - yy - zz) * 2; qw = (yz - zy) / s; qx = 0.25f * s; qy = (yx + xy) / s; qz = (zx + xz) / s; }
            else if (yy > zz) { float s = Mathf.Sqrt(1 + yy - xx - zz) * 2; qw = (zx - xz) / s; qx = (yx + xy) / s; qy = 0.25f * s; qz = (zy + yz) / s; }
            else { float s = Mathf.Sqrt(1 + zz - xx - yy) * 2; qw = (xy - yx) / s; qx = (zx + xz) / s; qy = (zy + yz) / s; qz = 0.25f * s; }
            return Finite(qw + qx + qy + qz);
        }

        /// Shortest-arc slerp (normalized); nlerp when the arc is tiny.
        public static void Slerp(float ax, float ay, float az, float aw, float bx, float by, float bz, float bw, float t, out float x, out float y, out float z, out float w)
        {
            float d = ax * bx + ay * by + az * bz + aw * bw;
            if (d < 0) { d = -d; bx = -bx; by = -by; bz = -bz; bw = -bw; }
            float ka, kb;
            if (d > 0.9995f) { ka = 1 - t; kb = t; }
            else { float th = Mathf.Acos(Mathf.Min(d, 1f)), s = 1f / Mathf.Sin(th); ka = Mathf.Sin((1 - t) * th) * s; kb = Mathf.Sin(t * th) * s; }
            x = ax * ka + bx * kb; y = ay * ka + by * kb; z = az * ka + bz * kb; w = aw * ka + bw * kb;
            float l = Mathf.Sqrt(x * x + y * y + z * z + w * w);
            if (l > 1e-12f) { x /= l; y /= l; z /= l; w /= l; } else { x = bx; y = by; z = bz; w = bw; }
        }

        /// The game camera: position and FOV lerped, rotation slerped; a cut (> 10 m or > 30 degrees in one frame) takes the nearer.
        public static ReflexCamera Camera(ReflexCamera a, ReflexCamera b, float t)
        {
            var fa = new Vector3(a.fx, a.fy, a.fz); var ua = new Vector3(a.ux, a.uy, a.uz); var fb = new Vector3(b.fx, b.fy, b.fz); var ub = new Vector3(b.ux, b.uy, b.uz);
            var pa = new Vector3(a.px, a.py, a.pz); var pb = new Vector3(b.px, b.py, b.pz);
            if (fa.sqrMagnitude < 0.5f || ua.sqrMagnitude < 0.5f || fb.sqrMagnitude < 0.5f || ub.sqrMagnitude < 0.5f || !((pb - pa).sqrMagnitude <= JumpSqr)) return t < 0.5f ? a : b;
            var qa = Quaternion.LookRotation(fa, ua); var qb = Quaternion.LookRotation(fb, ub);
            if (Quaternion.Angle(qa, qb) > 30f || Mathf.Abs(a.fov - b.fov) > 0.35f) return t < 0.5f ? a : b;
            var q = Quaternion.Slerp(qa, qb, t); var p = Vector3.Lerp(pa, pb, t);
            Vector3 r = q * Vector3.right, u = q * Vector3.up, f = q * Vector3.forward;
            var c = b;
            c.px = p.x; c.py = p.y; c.pz = p.z; c.rx = r.x; c.ry = r.y; c.rz = r.z; c.ux = u.x; c.uy = u.y; c.uz = u.z; c.fx = f.x; c.fy = f.y; c.fz = f.z;
            c.fov = Mathf.Lerp(a.fov, b.fov, t);
            return c;
        }

        /// A skeleton evaluated with the bike at `bikeFrom` (row matrices, node 0 = the bike frame) moved rigidly onto `bikeTo`.
        public static void Carry(float[] bones, int count, float[] bikeFrom, float[] bikeTo, float[] o)
        {
            var d = OriginalBikePose.FromRows(bikeTo, 0) * OriginalBikePose.FromRows(bikeFrom, 0).inverse;   // Unity column convention
            for (int i = 0; i < count; i++)
            {
                var m = d * OriginalBikePose.FromRows(bones, i * 16);
                int k = i * 16;
                for (int row = 0; row < 3; row++) for (int col = 0; col < 4; col++) o[k + col * 4 + row] = m[row, col];
                o[k + 3] = bones[k + 3]; o[k + 7] = bones[k + 7]; o[k + 11] = bones[k + 11]; o[k + 15] = bones[k + 15];
            }
        }
    }
}

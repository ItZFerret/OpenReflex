// The replay's cameras (ReflexReplay, after Open Unleashed's MxaReplay.Cameras): where the game camera is for the moment shown,
// following the chosen rider.
//   DIRECTOR     a TV director: trackside cameras along the rider's line, cut in with chase / helicopter / wheel / onboard shots;
//                deterministic per stretch of track, so scrubbing back shows the same edit
//   TRACKSIDE    the TV cameras only: a fixed camera on a tripod beside the line every few seconds of riding (placed from the rider's
//                recorded path, on the terrain, where the line of sight is clear of the terrain), panning and zooming to keep the rider
//                the same size in frame; when a hill gets between it and the rider anyway, the shot goes to the helicopter until clear
//   CHASE        a soft chase camera behind the rider
//   HELICOPTER   high and wide, a long lens
//   ONBOARD      at the rider's eyes (the local rider: the cockpit rider of the Dynamic FPV view, no head and torso)
//   WHEEL CAM    bolted to the bike beside the rear wheel, looking forward
//   ORBIT        around the rider: mouse (right button) / right stick, wheel / Q E / left stick for the distance
//   GAME CAMERA  the camera the player rode with, as recorded
//   FREE CAMERA  a fly camera: WASD / left stick, Q E (L1 R1) up and down, mouse (right button) / right stick to look
//   KEYFRAMES    the camera keys set along the timeline, flown through as the replay plays (ReflexReplay.Keyframes.cs); without keys
//                it is the free camera
// The smoothing runs on replay time (a frozen replay holds still; slow motion eases slowly); a cut (a new shot, a seek, another rider)
// snaps. Distances are in feet (the game's units).
using System;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.InputSystem;

namespace Reflex.Port.Replay
{
    public sealed partial class ReflexReplay
    {
        public enum Cam { Director, Trackside, Chase, Helicopter, Onboard, Wheel, Orbit, Game, Free, Keyframes }

        public static readonly string[] CamNames = { "DIRECTOR", "TRACKSIDE", "CHASE", "HELICOPTER", "ONBOARD", "WHEEL CAM", "ORBIT", "GAME CAMERA", "FREE CAMERA", "KEYFRAMES" };

        static Cam lastCam = Cam.Director;   // the mode chosen last (kept for the next replay in this run)

        Cam cam = Cam.Director;
        Cam shot = Cam.Trackside;   // the director's shot now
        long shotId = long.MinValue;
        bool snap = true;
        Camera camera;
        Vector3 eyeNow, aimNow;
        Quaternion rotNow = Quaternion.identity;
        float fovNow = 50f;
        float orbYaw, orbPitch = 12f, orbDist = 18f;
        Vector3 freePos;
        float freeYaw, freePitch;
        float flySpeed = 22f;
        float blocked, clear;   // replay seconds the trackside camera's view has been blocked / clear again
        bool fallback;          // the trackside shot is on the helicopter for now
        int lastSpot = -1;

        public Cam CameraMode => cam;
        /// The camera shown (the director's shot inside DIRECTOR; PHOTO MODE's FREE / ORBIT).
        public Cam ShownCamera => Face == View.Photo ? (Photo.orbit ? Cam.Orbit : Cam.Free) : cam == Cam.Director ? shot : cam;
        public string CameraLabel => cam == Cam.Director ? "DIRECTOR · " + CamNames[(int)shot]
                                   : cam == Cam.Keyframes ? (keys.Count == 0 ? "KEYFRAMES · NO KEYS" : $"KEYFRAMES · {keys.Count} KEY{(keys.Count > 1 ? "S" : "")}") : CamNames[(int)cam];
        /// The user's lens (FREE / ORBIT and PHOTO MODE), vertical degrees.
        public float UserFov { get => userFov; set => userFov = Mathf.Clamp(value, 5f, 110f); }
        float userFov = 55f;
        /// The distance from the camera to the followed rider (PHOTO MODE's auto focus).
        public float SubjectDistance { get; private set; } = 20f;
        public Camera ViewCamera => camera;

        public void CycleCamera(int d)
        {
            int n = CamNames.Length;
            SetCamera((Cam)((((int)cam + d) % n + n) % n));
        }

        public void SetCamera(Cam c)
        {
            if (c == Cam.Free && cam != Cam.Free) FreeFromView();
            if (c == Cam.Keyframes && keys.Count == 0) FreeFromView();   // (no keys yet: it flies as the free camera)
            if (c == Cam.Orbit && cam != Cam.Orbit) OrbitFromView();
            cam = c;
            lastCam = c;
            CameraCut();
        }

        public void CameraCut() { snap = true; shotId = long.MinValue; focusSnap = true; Reflex.Port.Rendering.ReflexCinematic.Cut(); }

        void CameraOpen()
        {
            camera = Runtime.chaseCamera;
            cam = lastCam;
            if (camera)
            {
                eyeNow = camera.transform.position;
                rotNow = camera.transform.rotation;
                fovNow = userFov = camera.fieldOfView;
            }
            CameraCut();
        }

        void CameraFaceChanged()
        {
            if (Face == View.Photo)
            {
                // PHOTO MODE starts where the replay's camera is: ORBIT stays an orbit, anything else becomes the free camera
                Photo.orbit = cam == Cam.Orbit;
                if (!Photo.orbit) FreeFromView();
                userFov = camera ? camera.fieldOfView : userFov;
            }
            CameraCut();
        }

        /// PHOTO MODE's CAMERA: ORBIT around the rider or FREE, from where the camera is now.
        public void SetPhotoOrbit(bool on)
        {
            Photo.orbit = on;
            if (on) OrbitFromView(); else FreeFromView();
            CameraCut();
        }

        void FreeFromView()
        {
            if (!camera) return;
            freePos = camera.transform.position;
            var e = camera.transform.rotation.eulerAngles;
            freeYaw = e.y;
            freePitch = e.x > 180f ? e.x - 360f : e.x;
        }

        void OrbitFromView()
        {
            if (!camera || !TargetAt(PlayTime, Target, out Vector3 tp, out _)) return;
            Vector3 d = camera.transform.position - (tp + Vector3.up * 2.6f);
            float len = d.magnitude;
            if (len < 0.5f) return;
            orbDist = Mathf.Clamp(len, 4f, 150f);
            orbYaw = Mathf.Atan2(d.x, d.z) * Mathf.Rad2Deg;
            orbPitch = Mathf.Clamp(Mathf.Asin(Mathf.Clamp(d.y / len, -1f, 1f)) * Mathf.Rad2Deg, -10f, 85f);
        }

        static float Ease(float k, float dt) => 1f - Mathf.Exp(-k * Mathf.Abs(dt));

        static Vector3 Flat(Vector3 v, Vector3 fallback)
        {
            v.y = 0f;
            return v.sqrMagnitude > 1e-4f ? v.normalized : fallback;
        }

        /// The terrain's height at p (the core's own surface), -inf off the terrain.
        float GroundAt(Vector3 p)
        {
            var t = Runtime ? Runtime.terrain : null;
            if (!t) return float.NegativeInfinity;
            float h = t.CoreHeightAt(p.x, p.z, out bool have);
            return have && !float.IsNaN(h) ? h : float.NegativeInfinity;
        }

        Vector3 AboveGround(Vector3 eye, float clearance)
        {
            float g = GroundAt(eye);
            if (!float.IsNegativeInfinity(g) && eye.y < g + clearance) eye.y = g + clearance;
            return eye;
        }

        // ------------------------------------------------------------------------------------------- per frame

        void UpdateCamera(float dt, float replayDt)
        {
            if (!camera) return;
            Cam mode = Face == View.Photo ? (Photo.orbit ? Cam.Orbit : Cam.Free) : cam;
            if (mode == Cam.Keyframes && keys.Count == 0) mode = Cam.Free;
            if (!controlsDone) UserControls(dt, mode);   // (PHOTO MODE's shutter reads them first: ShutterBegin)
            controlsDone = false;
            if (!TargetAt(ViewTime, Target, out Vector3 tp, out Quaternion tq)) return;
            TargetAt(ViewTime - 0.06f, Target, out Vector3 p0, out _);
            TargetAt(ViewTime + 0.06f, Target, out Vector3 p1, out _);
            Vector3 vel = (p1 - p0) / 0.12f;
            Vector3 fwd = Flat(tq * Vector3.forward, Flat(vel, Vector3.forward));
            if (mode == Cam.Director)
            {
                var path = PathFor(Target);
                int spot = path.SpotAt(path.SAt(PlayTime, Buffer));
                long id = ((long)Target << 32) | (uint)spot;
                if (id != shotId) { shotId = id; snap = true; shot = spot >= 0 ? path.spots[spot].shot : Cam.Chase; fallback = false; blocked = clear = 0f; }
                mode = shot;
                if (mode == Cam.Onboard && !TargetEyes(Target, out _)) mode = Cam.Chase;   // (a rider without a skeleton yet)
            }
            if (mode == Cam.Trackside) mode = TracksideOrFallback(tp, replayDt);
            if (mode == Cam.Trackside)
            {
                int spot = PathFor(Target).SpotAt(PathFor(Target).SAt(PlayTime, Buffer));   // the next tripod: a cut
                if (spot != lastSpot) { lastSpot = spot; snap = true; }
            }
            else lastSpot = -1;
            OriginalWorld.OriginalRider cockpitRider = null;
            float near = 0.5f;
            Vector3 eye = eyeNow;
            Quaternion rot = rotNow;
            float fov = fovNow;
            Vector3 subject = tp + Vector3.up * 2.6f;
            switch (mode)
            {
                case Cam.Trackside:
                {
                    var path = PathFor(Target);
                    int spot = path.SpotAt(path.SAt(PlayTime, Buffer));
                    eye = spot >= 0 ? path.spots[spot].eye : tp - fwd * 30f + Vector3.up * 8f;
                    Vector3 aim = subject + vel * 0.12f;
                    aimNow = snap ? aim : Vector3.Lerp(aimNow, aim, Ease(9f, replayDt));
                    rot = Quaternion.LookRotation(aimNow - eye, Vector3.up);
                    float d = Vector3.Distance(eye, aimNow);
                    float want = Mathf.Clamp(2f * Mathf.Atan(7.5f / Mathf.Max(d, 1f)) * Mathf.Rad2Deg, 5f, 52f);
                    fov = snap ? want : Mathf.Lerp(fovNow, want, Ease(3f, replayDt));
                    near = 1f;
                    break;
                }
                case Cam.Chase:
                {
                    Vector3 want = AboveGround(tp - fwd * 17f + Vector3.up * 5.5f, 1.5f);
                    eye = snap ? want : Vector3.Lerp(eyeNow, want, Ease(4.5f, replayDt));
                    eye = AboveGround(eye, 1.2f);
                    rot = Quaternion.LookRotation(subject - eye, Vector3.up);
                    fov = 52f;
                    break;
                }
                case Cam.Helicopter:
                {
                    Vector3 side = new Vector3(fwd.z, 0f, -fwd.x);
                    Vector3 want = AboveGround(tp - fwd * 62f + side * 34f + Vector3.up * 52f, 20f);
                    eye = snap ? want : Vector3.Lerp(eyeNow, want, Ease(1.1f, replayDt));
                    aimNow = snap ? subject : Vector3.Lerp(aimNow, subject, Ease(6f, replayDt));
                    rot = Quaternion.LookRotation(aimNow - eye, Vector3.up);
                    float d = Vector3.Distance(eye, aimNow);
                    fov = Mathf.Clamp(2f * Mathf.Atan(14f / Mathf.Max(d, 1f)) * Mathf.Rad2Deg, 8f, 50f);
                    near = 1f;
                    break;
                }
                case Cam.Onboard:
                {
                    Quaternion look = tq * Quaternion.Euler(9f, 0f, 0f);
                    rot = snap ? look : Quaternion.Slerp(rotNow, look, Ease(14f, replayDt));
                    bool local = Target < actors.Count && actors[Target].local;
                    if (TargetEyes(Target, out Vector3 eyes))
                    {
                        // the local rider: inside the cockpit rider's head; another rider: just in front of his visor (his head stays on)
                        eye = eyes + tq * (local ? new Vector3(0f, 0.05f, 0.12f) : new Vector3(0f, 0.12f, 0.75f));
                        if (local) cockpitRider = actors[Target].rider;
                        near = local ? 0.1f : 0.2f;
                    }
                    else { eye = tp + tq * new Vector3(0f, 5.2f, 0.6f); near = 0.25f; }
                    fov = 70f;
                    break;
                }
                case Cam.Wheel:
                {
                    float side = Target < actors.Count ? actors[Target].halfWidth + 1.1f : 2f;
                    eye = tp + tq * new Vector3(side, 1.05f, -0.9f);
                    rot = tq * Quaternion.Euler(4f, -6f, 0f);
                    fov = 66f;
                    near = 0.2f;
                    break;
                }
                case Cam.Game:
                {
                    Buffer.Locate(PlayTime, out int a, out int b, out float w);
                    var fa = Buffer[a]; var fb = Buffer[b];
                    eye = Vector3.LerpUnclamped(fa.camPos, fb.camPos, w);
                    rot = ReflexReplayBuffer.Nlerp(fa.camRot, fb.camRot, w);
                    fov = Mathf.Lerp(fa.camFov, fb.camFov, w);
                    near = Mathf.Max(0.05f, Mathf.Min(fa.camNear, fb.camNear));
                    break;
                }
                case Cam.Orbit:
                {
                    var o = Quaternion.Euler(orbPitch, orbYaw, 0f);
                    eye = AboveGround(subject + o * Vector3.forward * orbDist, 0.6f);
                    rot = Quaternion.LookRotation(subject - eye, Vector3.up);
                    fov = userFov;
                    near = 0.2f;
                    break;
                }
                case Cam.Keyframes:
                {
                    KeyframeView(PlayTime, subject, out eye, out rot, out fov);
                    near = 0.2f;
                    break;
                }
                case Cam.Free:
                {
                    freePos = AboveGround(freePos, 0.5f);
                    eye = freePos;
                    rot = Quaternion.Euler(freePitch, freeYaw, 0f);
                    fov = userFov;
                    near = 0.2f;
                    break;
                }
            }
            if (Face == View.Photo && Mathf.Abs(Photo.roll) > 0.01f) rot *= Quaternion.Euler(0f, 0f, Photo.roll);
            eyeNow = eye; rotNow = rot; fovNow = fov;
            snap = false;
            SetCockpit(cockpitRider);
            SubjectDistance = Vector3.Distance(eye, subject);
            camera.transform.SetPositionAndRotation(eye, rot);
            camera.fieldOfView = Mathf.Clamp(fov, 1f, 120f);
            camera.nearClipPlane = near;
        }

        /// The trackside camera, or the helicopter while its view of the rider is blocked (0.2 s blocked to leave, 0.6 s clear to come
        /// back: no flicker on a crest).
        Cam TracksideOrFallback(Vector3 tp, float replayDt)
        {
            var path = PathFor(Target);
            int spot = path.SpotAt(path.SAt(PlayTime, Buffer));
            if (spot < 0) return Cam.Trackside;
            bool isClear = Clear(path.spots[spot].eye, tp + Vector3.up * 2.6f);
            float dt = Mathf.Abs(replayDt) > 0f ? Mathf.Abs(replayDt) : (snap ? 1f : 0f);
            if (isClear) { clear += dt; blocked = 0f; } else { blocked += dt; clear = 0f; }
            if (!fallback && blocked > 0.2f) { fallback = true; snap = true; }
            else if (fallback && clear > 0.6f) { fallback = false; snap = true; }
            return fallback ? Cam.Helicopter : Cam.Trackside;
        }

        // ------------------------------------------------------------------------------- orbit / free controls

        void UserControls(float dt, Cam mode)
        {
            if (mode != Cam.Orbit && mode != Cam.Free) return;
            var kb = Keyboard.current;
            var mouse = Mouse.current;
            var pad = Gamepad.current;
            Vector2 look = Vector2.zero, move = Vector2.zero;
            float rise = 0f, zoom = 0f;
            bool photo = Face == View.Photo;
            bool overUi = Ui && Ui.PointerOverPanel;
            if (mouse != null && !overUi)
            {
                if (mouse.rightButton.isPressed) look += mouse.delta.ReadValue() * 0.12f;
                float wheel = mouse.scroll.ReadValue().y;
                if (Mathf.Abs(wheel) > 0.01f)
                {
                    bool shift = kb != null && kb.leftShiftKey.isPressed;
                    if (shift && mode == Cam.Free) flySpeed = Mathf.Clamp(flySpeed * (wheel > 0 ? 1.2f : 1f / 1.2f), 2f, 400f);
                    else zoom -= Mathf.Sign(wheel);
                }
            }
            if (kb != null)
            {
                move.x += (kb.dKey.isPressed ? 1 : 0) - (kb.aKey.isPressed ? 1 : 0);
                move.y += (kb.wKey.isPressed ? 1 : 0) - (kb.sKey.isPressed ? 1 : 0);
                rise += (kb.eKey.isPressed ? 1 : 0) - (kb.qKey.isPressed ? 1 : 0);
            }
            if (pad != null)
            {
                Vector2 r = pad.rightStick.ReadValue();
                if (r.sqrMagnitude > 0.02f) look += new Vector2(r.x, r.y) * 140f * dt;
                Vector2 l = pad.leftStick.ReadValue();
                if (l.sqrMagnitude > 0.02f) move += l;
                if (photo)
                {
                    rise += (pad.rightShoulder.isPressed ? 1 : 0) - (pad.leftShoulder.isPressed ? 1 : 0);
                    zoom += (pad.leftTrigger.ReadValue() - pad.rightTrigger.ReadValue()) * 4f * dt;
                }
            }
            if (Mathf.Abs(zoom) > 1e-4f)
            {
                if (photo || mode == Cam.Free) userFov = Mathf.Clamp(userFov * Mathf.Pow(1.08f, zoom * (Mathf.Abs(zoom) >= 1f ? 1f : 6f)), 5f, 110f);
                else orbDist = Mathf.Clamp(orbDist * Mathf.Pow(1.1f, zoom), 4f, 150f);
            }
            float fast = kb != null && kb.leftShiftKey.isPressed ? 4f : kb != null && kb.leftCtrlKey.isPressed ? 0.25f : 1f;
            if (pad != null && pad.leftStickButton.isPressed) fast *= 4f;
            float lens = Mathf.Clamp(userFov / 60f, 0.15f, 1.2f);   // zoomed in, the look slows down with the lens
            if (mode == Cam.Orbit)
            {
                orbYaw += look.x * lens;
                orbPitch = Mathf.Clamp(orbPitch - look.y * lens, -10f, 85f);
                orbYaw -= move.x * 70f * dt;
                orbDist = Mathf.Clamp(orbDist * Mathf.Pow(1.8f, (-move.y - rise) * dt * fast), 4f, 150f);
            }
            else
            {
                freeYaw += look.x * lens;
                freePitch = Mathf.Clamp(freePitch - look.y * lens, -89f, 89f);
                var q = Quaternion.Euler(freePitch, freeYaw, 0f);
                Vector3 v = q * new Vector3(move.x, 0f, move.y) + Vector3.up * rise;
                freePos += v * flySpeed * fast * dt;
            }
        }

        // ------------------------------------------------------------------------------- the TV camera spots

        sealed class Spot
        {
            public float s0, s1;
            public Vector3 eye;
            public Cam shot;
        }

        sealed class TvPath
        {
            public int frames;
            public float start;
            public float[] s = Array.Empty<float>();
            public Vector3[] p = Array.Empty<Vector3>();
            public readonly List<Spot> spots = new List<Spot>();

            /// The rider's distance along his line at replay time t.
            public float SAt(float t, ReflexReplayBuffer b)
            {
                if (s.Length == 0) return 0f;
                b.Locate(t, out int i, out int j, out float w);
                i = Mathf.Min(i, s.Length - 1); j = Mathf.Min(j, s.Length - 1);
                return Mathf.Lerp(s[i], s[j], w);
            }

            public Vector3 PAt(float d)
            {
                if (p.Length == 0) return Vector3.zero;
                int lo = 0, hi = s.Length - 1;
                if (d <= s[0]) return p[0];
                if (d >= s[hi]) return p[hi];
                while (hi - lo > 1) { int m = (lo + hi) >> 1; if (s[m] <= d) lo = m; else hi = m; }
                float w = s[hi] > s[lo] ? (d - s[lo]) / (s[hi] - s[lo]) : 0f;
                return Vector3.Lerp(p[lo], p[hi], w);
            }

            public int SpotAt(float d)
            {
                if (spots.Count == 0) return -1;
                int lo = 0, hi = spots.Count - 1;
                while (lo < hi) { int m = (lo + hi + 1) >> 1; if (spots[m].s0 <= d) lo = m; else hi = m - 1; }
                return lo;
            }
        }

        readonly Dictionary<int, TvPath> paths = new Dictionary<int, TvPath>();

        // the director's edit, by spot: mostly trackside, a closer shot every few
        static readonly Cam[] Edit =
        {
            Cam.Trackside, Cam.Trackside, Cam.Chase, Cam.Trackside, Cam.Helicopter, Cam.Trackside,
            Cam.Wheel, Cam.Trackside, Cam.Trackside, Cam.Onboard, Cam.Trackside, Cam.Chase,
        };

        TvPath PathFor(int bike)
        {
            if (paths.TryGetValue(bike, out var path) && path.frames == Buffer.Count && path.start == Buffer.StartTime) return path;
            path = new TvPath { frames = Buffer.Count, start = Buffer.StartTime };
            int n = Buffer.Count;
            path.s = new float[n];
            path.p = new Vector3[n];
            int j = bike >= 0 && bike < actors.Count ? actors[bike].layout.fullStart : 0;
            float s = 0f;
            for (int i = 0; i < n; i++)
            {
                Vector3 p = ReflexReplayBuffer.FullPos(Buffer[i].full, j);
                if (i > 0)
                {
                    float d = Vector3.Distance(p, path.p[i - 1]);
                    if (d < 60f) s += d;   // a reset / respawn jumps: no distance (the next spot starts there)
                }
                path.s[i] = s;
                path.p[i] = p;
            }
            // spots: a stretch of about 3.4 s of riding each (140 ft at least), the camera near its far end
            float total = s, s0 = 0f;
            int k = 0;
            int seed = Mathf.Abs((Runtime && Runtime.level != null ? Runtime.level : "").GetHashCode()) % 97;
            while (true)
            {
                float speed = SpeedAt(path, s0);
                float len = Mathf.Clamp(speed * 3.4f, 140f, 420f);
                float s1 = Mathf.Min(total, s0 + len);
                if (total - s1 < len * 0.35f) s1 = total;   // no short last stretch
                var spot = new Spot { s0 = s0, s1 = s1, shot = Edit[(k + seed) % Edit.Length] };
                spot.eye = PlaceSpot(path, s0, s1, k);
                if (k == 0) spot.shot = Cam.Trackside;
                path.spots.Add(spot);
                k++;
                if (s1 >= total || k > 4000) break;
                s0 = s1;
            }
            paths[bike] = path;
            return path;
        }

        float SpeedAt(TvPath path, float d)
        {
            // ft per s around d (from the frames' times)
            int lo = 0, hi = path.s.Length - 1;
            if (hi < 1) return 0f;
            while (hi - lo > 1) { int m = (lo + hi) >> 1; if (path.s[m] <= d) lo = m; else hi = m; }
            int a = Mathf.Max(0, lo - 20), b = Mathf.Min(path.s.Length - 1, lo + 20);
            float dt = Buffer[b].time - Buffer[a].time;
            return dt > 1e-3f ? (path.s[b] - path.s[a]) / dt : 0f;
        }

        Vector3 PlaceSpot(TvPath path, float s0, float s1, int k)
        {
            float len = s1 - s0;
            float anchor = s0 + len * 0.62f;
            Vector3 p = path.PAt(anchor);
            Vector3 dir = Flat(path.PAt(anchor + 12f) - path.PAt(anchor - 12f), Vector3.forward);
            Vector3 perp = new Vector3(dir.z, 0f, -dir.x);
            float side = (k & 1) == 0 ? 1f : -1f;
            Vector3[] look = { path.PAt(s0 + len * 0.15f), path.PAt(s0 + len * 0.4f), p, path.PAt(s0 + len * 0.85f), path.PAt(s1) };
            (float sd, float dist, float h)[] tries =
            {
                (side, 46f, 7f), (side, 66f, 14f), (-side, 46f, 7f), (-side, 70f, 16f), (side, 34f, 26f), (-side, 40f, 28f),
                (side, 90f, 40f), (-side, 90f, 45f), (side, 24f, 60f), (-side, 24f, 70f),
            };
            Vector3 best = Vector3.zero;
            int bestScore = -1;
            foreach (var (sd, dist, h) in tries)
            {
                Vector3 eye = p + perp * (sd * dist) + dir * 8f;
                float g = GroundAt(eye);
                eye.y = Mathf.Max(float.IsNegativeInfinity(g) ? p.y : g, p.y - 4f) + h;
                int score = 0;
                foreach (var q in look) if (Clear(eye, q + Vector3.up * 2.5f)) score++;
                if (score > bestScore) { bestScore = score; best = eye; }
                if (score == look.Length) break;
            }
            return best;
        }

        /// The line of sight from eye to q passes over the terrain (the scenery has no colliders here: a building can still be in the way).
        bool Clear(Vector3 eye, Vector3 q)
        {
            Vector3 d = q - eye;
            float len = d.magnitude;
            if (len < 1f) return true;
            int steps = Mathf.Clamp(Mathf.CeilToInt(len / 5f), 12, 64);
            for (int i = 1; i < steps; i++)
            {
                Vector3 x = eye + d * (i / (float)steps);
                float g = GroundAt(x);
                if (!float.IsNegativeInfinity(g) && g > x.y - 0.3f) return false;
            }
            return true;
        }
    }
}

// The open garage's 3D stage (OpenShell.Garage.cs): FeGarage's models (the rider and the vehicle the game's tables describe) seen through
// an orbit camera that frames what the garage is about (the vehicle, the rider, a piece of gear, a part), turned by the mouse (drag),
// the right stick or , . J K, and zoomed by the wheel, the triggers or - =. The camera is fitted to the models' bounds, so every class
// (MX Lite .. UTV, trucks, buggies) and every rider frames the same way without a table of camera positions.
// Open Unleashed's garage (MxaRiderPreview, Garage.uxml): a stage with a turntable that eases back after a drag and shots per tab.
using System;
using UnityEngine;

namespace Reflex.Port.OpenFrontEnd
{
    internal sealed class OpenGarageStage
    {
        /// Where the camera looks. Rider shots: the body part; vehicle shots: a part of the vehicle along its axis.
        public enum Shot { Overview, Vehicle, Rider, Head, Torso, Legs, Feet, Hands, Front, Rear, Wheels, Low }

        readonly Reflex.Port.FrontEnd.FeGarage garage;
        Vector3 aim, aimGoal;
        float yaw = 38f, yawGoal = 38f, pitch = 10f, pitchGoal = 10f, dist = 14f, distGoal = 14f;
        float userZoom = 1f;
        Shot shot = Shot.Overview;
        float idle;
        Vector3 fwd = Vector3.forward, right = Vector3.right;
        float aspect = 1.4f;
        /// Where on the screen the subject sits: > 0 lifts it above the middle (the cards under the view cover its bottom).
        public float lift = 0.06f;
        /// How much of the models' bounding sphere must fit the view (the sphere is far larger than what the screen shows of a vehicle).
        public const float FitTightness = 0.74f;
        /// The turntable turns by itself while the player does not touch the stage (REDUCE MOTION: it does not).
        public bool autoTurn = true;
        /// Logs the axes and bounds the camera works from (REFLEX_GARAGE_DEBUG / tests).
        public static bool Verbose;
        /// Tests: the turntable stays where it is (a screenshot must not catch it turning).
        public static bool NoAutoTurn;
        /// World mode (the menu world draws the whole screen behind the pages): the part of the screen the subject is framed in, its centre's
        /// offset from the screen's centre (fractions of the half-size; +x right, +y up), its height as a fraction of the screen's and the
        /// screen's aspect. Defaults: the whole view (the garage's own picture).
        public Vector2 viewCenter; public float viewHeight = 1f, screenAspect = 0f;
        /// More bounds the Overview holds (garage space).
        public Func<Bounds?> extraBounds;
        /// The aim stays where it is (the menu's push-bike walk: the pair walks into a fixed shot); Frame / Refit only take the angles.
        public bool holdAim;
        /// A slow offset of the turn and tilt (degrees) the menu world adds (its drift); not part of the goal.
        public Vector2 drift;
        /// The fitted distance times this (< 1: closer; the menu world's hub fills its view more than the garage's turntable).
        public float fitScale = 1f;
        /// The close shots (a body part, a part of the vehicle) times this as well (the menu world draws them over the whole screen, not in a
        /// clipped picture: a little wider keeps the subject inside its view).
        public float detailFitScale = 1f;
        /// Added to the aim (garage space): the menu world's camera follows the push-bike walk with it.
        public Vector3 aimOffset;
        public Shot Current => shot;
        public float Yaw => yaw;
        public float Distance => dist;

        public OpenGarageStage(Reflex.Port.FrontEnd.FeGarage g) { garage = g; }

        static Texture2D s_glow;
        /// A soft round glow behind the vehicle (white, fading to nothing at the rim; the stylesheet tints it).
        public static Texture2D Glow
        {
            get
            {
                if (s_glow) return s_glow;
                const int n = 96;
                var t = new Texture2D(n, n, TextureFormat.RGBA32, false) { wrapMode = TextureWrapMode.Clamp, name = "ou_garage_glow", hideFlags = HideFlags.DontSave };
                var px = new Color32[n * n];
                for (int y = 0; y < n; y++)
                    for (int x = 0; x < n; x++)
                    {
                        float dx = (x + 0.5f) / n * 2f - 1f, dy = (y + 0.5f) / n * 2f - 1f;
                        float d = Mathf.Clamp01(1f - Mathf.Sqrt(dx * dx + dy * dy));
                        px[y * n + x] = new Color32(255, 255, 255, (byte)(Mathf.Pow(d, 1.8f) * 255f));
                    }
                t.SetPixels32(px); t.Apply(false, true);
                return s_glow = t;
            }
        }

        public void SetAspect(float a) { aspect = Mathf.Clamp(a, 0.5f, 3.5f); }

        /// Frames a shot: the camera eases to it. resetAngles also returns the turn to the shot's own (a new tab, a new subject).
        public void Frame(Shot s, bool resetAngles, bool snap = false)
        {
            shot = s;
            if (resetAngles) { userZoom = 1f; idle = 0f; (yawGoal, pitchGoal) = Angles(s); }
            if (!holdAim)
            {
                Aim(out aimGoal, out float radius);
                distGoal = Mathf.Max(FitDistance(radius), HeightDistance()) * userZoom;
            }
            if (snap) { aim = aimGoal; dist = distGoal; yaw = yawGoal; pitch = pitchGoal; Apply(); }
        }

        /// The new models are in: the same shot on their bounds (the turn the player gave it stays).
        public void Refit() => Frame(shot, false);

        static (float yaw, float pitch) Angles(Shot s)
        {
            switch (s)
            {
                case Shot.Head: return (52f, 4f);
                case Shot.Torso: return (50f, 5f);
                case Shot.Legs: case Shot.Hands: return (54f, 5f);
                case Shot.Feet: return (50f, 12f);
                case Shot.Rider: return (46f, 6f);
                case Shot.Front: return (52f, 8f);
                case Shot.Rear: return (-48f, 8f);
                case Shot.Wheels: return (12f, 5f);
                case Shot.Low: return (22f, 3f);
                case Shot.Vehicle: return (34f, 9f);
                default: return (36f, 10f);
            }
        }

        // the vehicle's frame: forward along its wheels, right beside it
        void Axes()
        {
            fwd = Vector3.forward;   // the riders and the vehicles of the game face +z
            if (garage.BikeAxis(out var f, out var r)) { var d = f - r; d.y = 0; if (d.sqrMagnitude > 0.01f) fwd = d.normalized; }
            else if (garage.BikeBounds(out var bb)) fwd = bb.size.x > bb.size.z ? Vector3.right : Vector3.forward;
            right = Vector3.Cross(Vector3.up, fwd);
            if (Verbose)
            {
                garage.BikeBounds(out var bbb); garage.RiderBounds(out var rbb);
                Debug.Log("[garage-stage] axes fwd=" + fwd + " front=" + f + " rear=" + r + " bike=" + bbb.center + "/" + bbb.size + " rider=" + rbb.center + "/" + rbb.size);
            }
        }

        /// The point and the radius (of what must fit) of the shot.
        void Aim(out Vector3 point, out float radius)
        {
            Axes();
            bool bike = garage.BikeBounds(out var b), rider = garage.RiderBounds(out var r);
            var all = bike ? b : r; if (bike && rider) all.Encapsulate(r);
            if (shot == Shot.Overview && extraBounds?.Invoke() is Bounds extra) { if (bike || rider) all.Encapsulate(extra); else { all = extra; bike = true; b = extra; } }
            point = all.center; radius = Mathf.Max(2f, all.extents.magnitude * FitTightness);
            fitHalfHeight = shot == Shot.Overview ? all.extents.y : shot == Shot.Vehicle && bike ? b.extents.y : shot == Shot.Rider && rider ? r.extents.y : 0f;
            if (!bike && !rider) { point = new Vector3(0, 2f, 0); radius = 5f; return; }
            switch (shot)
            {
                case Shot.Vehicle:
                case Shot.Low:
                    if (bike) { point = b.center; radius = Mathf.Max(2f, b.extents.magnitude * FitTightness); if (shot == Shot.Low) { point.y = b.min.y + b.size.y * 0.3f; radius *= 0.8f; } }
                    break;
                case Shot.Front:
                    if (bike) { point = Along(b, 0.8f, 0.55f); radius = Mathf.Max(1.4f, b.extents.magnitude * 0.45f); }
                    break;
                case Shot.Rear:
                    if (bike) { point = Along(b, 0.2f, 0.45f); radius = Mathf.Max(1.4f, b.extents.magnitude * 0.55f); }
                    break;
                case Shot.Wheels:
                    if (bike) { point = Along(b, 0.5f, 0.22f); radius = Mathf.Max(2f, b.extents.magnitude * 0.85f); }
                    break;
                case Shot.Rider: case Shot.Head: case Shot.Torso: case Shot.Legs: case Shot.Feet: case Shot.Hands:
                    if (rider)
                    {
                        float h = Mathf.Max(1f, r.size.y);
                        float cy = shot == Shot.Head ? 0.9f : shot == Shot.Torso ? 0.68f : shot == Shot.Legs ? 0.32f : shot == Shot.Feet ? 0.08f : shot == Shot.Hands ? 0.52f : 0.5f;
                        float rad = shot == Shot.Head ? 0.2f : shot == Shot.Torso ? 0.34f : shot == Shot.Legs ? 0.36f : shot == Shot.Feet ? 0.2f : shot == Shot.Hands ? 0.3f : 0.62f;
                        point = new Vector3(r.center.x, r.min.y + h * cy, r.center.z);
                        radius = Mathf.Max(0.8f, h * rad);
                    }
                    break;
            }
        }

        // a point along the vehicle's axis (0 = rear wheel, 1 = front wheel) at a fraction of its height
        Vector3 Along(Bounds b, float t, float heightFrac)
        {
            Vector3 f, rr;
            if (!garage.BikeAxis(out f, out rr)) { rr = b.center - fwd * b.extents.magnitude * 0.5f; f = b.center + fwd * b.extents.magnitude * 0.5f; }
            var p = Vector3.Lerp(rr, f, t);
            p.y = b.min.y + b.size.y * heightFrac;
            return p;
        }

        float fitHalfHeight;
        /// The whole subjects' height inside the view's height (a wide view, an ultrawide screen: the width fit alone would come too close).
        public static float HeightFill = 1.18f;
        float HeightDistance()
        {
            if (fitHalfHeight <= 0f) return 0f;
            float v = Mathf.Atan(Mathf.Tan(garage.Fov * Mathf.Deg2Rad * 0.5f) * Mathf.Clamp(viewHeight, 0.05f, 1f));
            return fitHalfHeight * HeightFill / Mathf.Tan(v) + fitHalfHeight * 0.5f;
        }

        float FitDistance(float radius)
        {
            float v = Mathf.Atan(Mathf.Tan(garage.Fov * Mathf.Deg2Rad * 0.5f) * Mathf.Clamp(viewHeight, 0.05f, 1f));
            float h = Mathf.Atan(Mathf.Tan(v) * aspect);
            bool detail = shot != Shot.Overview && shot != Shot.Vehicle && shot != Shot.Rider;
            return radius / Mathf.Sin(Mathf.Min(v, h)) * 1.06f * fitScale * (detail ? detailFitScale : 1f);
        }

        // ------------------------------------------------------------------------------------------------ input

        public void Orbit(float yawDelta, float pitchDelta)
        {
            yawGoal += yawDelta; pitchGoal = Mathf.Clamp(pitchGoal + pitchDelta, -4f, 62f);
            idle = 0f;
        }

        /// amount > 0 zooms in.
        public void Zoom(float amount)
        {
            if (Mathf.Abs(amount) < 1e-4f) return;
            userZoom = Mathf.Clamp(userZoom * Mathf.Exp(-amount), 0.3f, 1.9f);
            if (holdAim) return;
            Aim(out _, out float radius);
            distGoal = Mathf.Max(FitDistance(radius), HeightDistance()) * userZoom;
            idle = 0f;
        }

        public void ResetView() => Frame(shot, true);

        /// The turn and tilt now (tests, and a camera that must not ease).
        public void SetAngles(float yawDegrees, float pitchDegrees) { yaw = yawGoal = yawDegrees; pitch = pitchGoal = pitchDegrees; idle = 0f; Apply(); }

        /// Easing, the turntable, and the camera put on the models.
        public void Tick(float dt)
        {
            idle += dt;
            if (autoTurn && !NoAutoTurn && !OuMotion.Reduced && idle > 4f) yawGoal += 7f * dt;
            float k = 1f - Mathf.Exp(-dt * 9f);
            yaw = Mathf.LerpAngle(yaw, yawGoal, k);
            // keep the goal near the angle so the unwinding is short
            if (Mathf.Abs(yawGoal - yaw) > 180f) yawGoal = yaw + Mathf.DeltaAngle(yaw, yawGoal);
            pitch = Mathf.Lerp(pitch, pitchGoal, k);
            dist = Mathf.Lerp(dist, distGoal, k);
            aim = Vector3.Lerp(aim, aimGoal, 1f - Mathf.Exp(-dt * 7f));
            // settled: exactly on the goal (an easing that never arrives keeps the camera creeping by fractions of a pixel, and the fine detail
            // of the ground and the spokes shimmer under it)
            if (Mathf.Abs(Mathf.DeltaAngle(yaw, yawGoal)) < 0.01f) yaw = yawGoal;
            if (Mathf.Abs(pitch - pitchGoal) < 0.01f) pitch = pitchGoal;
            if (Mathf.Abs(dist - distGoal) < 0.002f) dist = distGoal;
            if ((aim - aimGoal).sqrMagnitude < 4e-6f) aim = aimGoal;
            Apply();
        }

        void Apply()
        {
            float a = (yaw + drift.x) * Mathf.Deg2Rad, p = (pitch + drift.y) * Mathf.Deg2Rad;
            // yaw 0 = beside the vehicle on its left, 90 = in front of it, -90 = behind
            var dir = Mathf.Cos(p) * (-right * Mathf.Cos(a) + fwd * Mathf.Sin(a)) + Vector3.up * Mathf.Sin(p);
            float halfHeight = Mathf.Tan(garage.Fov * Mathf.Deg2Rad * 0.5f) * dist;
            var look = aim + aimOffset - Vector3.up * (lift * halfHeight * Mathf.Clamp(viewHeight, 0.05f, 1f));   // the subject rides above the middle
            var eye = look + dir * dist;
            if (viewCenter != Vector2.zero && screenAspect > 0f)
            {   // the subject at the view's part of the screen: the camera moves sideways, the other way (its frustum stays centred)
                var forward = -dir.normalized;
                var camRight = Vector3.Cross(Vector3.up, forward).normalized;
                var camUp = Vector3.Cross(forward, camRight);
                var shift = camRight * (viewCenter.x * halfHeight * screenAspect) + camUp * (viewCenter.y * halfHeight);
                eye -= shift; look -= shift;
            }
            garage.SetCameraNow(eye, look);
        }

        /// The point the camera aims at now and its goal (garage space; tests and the menu world).
        public Vector3 AimPoint => aim;
    }
}

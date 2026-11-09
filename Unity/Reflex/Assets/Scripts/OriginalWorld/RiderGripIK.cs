using System.Collections.Generic;
using System.Runtime.CompilerServices;
using Reflex.Port.Content;
using UnityEngine;

namespace Reflex.Port.OriginalWorld
{
    // OPEN REFLEX: the rider's hands closed on the bike's grips (w-grips; core/plan/QUEUE.md "w-grips"). An improvement over the original.
    //
    // The game poses the hands from its rider animations and their IK targets, the bike hierarchy's NDE_RHT_Hand / NDE_LFT_Hand locators.
    // Every MX and MX Lite model shares one hierarchy (VehicleHierarchyId 2, MX_Hierarchy: the same locators) and one animation set, but
    // each model's bars put its grips somewhere else: the fist the animations close sits 0.06 ft (up to 0.18) off the grip of the MarsF /
    // Earth bikes, its axis turned 21 - 26 degrees from the grip's, and 0.13 ft (up to 0.25) below and beside the throttle grip of an Earth
    // Lite, whose chassis model places it off the hierarchy's OBJ_Throttle (measured with tests/rider_grips). The original draws exactly
    // this (the port's rider pose is the game's); its first-person view hid the rider, the Dynamic FPV cockpit shows it a few inches away.
    //
    // Here, after the game's pose is applied: each hand's fist (the centre and axis of the circles its four fingers curl along, from the
    // finger joints) is turned onto its grip's axis and moved onto it, by a two-bone IK of the arm (upper / lower arm, the elbow kept on
    // the animation's side) that ends at the wrist with the fist's new rotation; the fingers keep their animated curl. The grip is the
    // bike model's own mesh: the outermost tube end about each hand locator (its end ring: centre, axis; its length along the tube),
    // posed every frame with the part that carries it (the bars steer it). Only while the animation
    // holds the hand at its grip (the fist within 0.3 ft of it, fading out by 0.6 ft): a one-hander's or no-hander's free hand, a hand on
    // the seat or the fender, a wreck and a rider off the bike stay as the game poses them.
    //
    // w-gripwidth (QUEUE.md "w-gripwidth"): the fist goes to the middle of its grip (the glove centred on the rubber), not where the
    // animation holds it along the bar (the original's pose leaves the gloves 0.02 - 0.06 ft past the grips' ends); a grip end must have
    // the hand locator on its inner side (on the grip), and the end ring may be rounded (a bike mod's grips: "New Bike Models" took its
    // throttle housing for the right grip, which pulled the right hand 0.4 ft in, and found no left grip at all).
    public static class RiderGripIK
    {
        /// The OPEN REFLEX improvement on (tests compare off / on).
        public static bool Enabled = true;

        const float Near = 0.3f, Far = 0.6f;    // ft: the animation's fist to its grip for the full / no correction
        const float TurnFull = 45f, TurnNone = 75f;   // degrees: the animation's fist to its grip's axis for the full / no turn onto it
        const float HalfHand = 0.13f;           // ft: the fist's centre to the pinky's / index finger's outer side, along the grip
        const float Search = 0.6f;              // ft: the bike's vertices about a hand locator that can carry its grip

        /// A grip in the model space of the part that carries it: the centre of its outer end, its axis (outward, towards that end) and
        /// its length (inward from the end).
        public sealed class Grip { public Transform part; public Vector3 end, axis; public float length, radius; }
        public sealed class Grips { public Grip right, left; }
        static readonly ConditionalWeakTable<ReflexBikeModel, Grips> grips = new ConditionalWeakTable<ReflexBikeModel, Grips>();
        // per rider, each hand's last fist in its wrist's frame (centre, direction; right then left): a hand opening its fingers keeps it
        static readonly ConditionalWeakTable<OriginalRider, Vector3[]> fists = new ConditionalWeakTable<OriginalRider, Vector3[]>();

        // MXRider_Hierarchy: the right arm (RArmA, RArmB, RWrist) and its fingers' joints (index, middle, ring, pinky: base, middle, tip),
        // the left side 29 nodes on (LArmA 39 ...)
        static readonly int[] Fingers = { 22, 23, 24, 19, 20, 21, 16, 17, 18, 13, 14, 15 };
        const int RightArm = 10, LeftOffset = 29;

        /// After the game's rider pose (and a bike mod's limb offsets) is applied: the hands onto the bike's grips. bike: the rider's
        /// vehicle model, posed this frame (OriginalBikePose.Apply).
        public static void Apply(OriginalRider rider, ReflexBikeModel bike)
        {
            if (!Enabled || rider == null || bike == null || !rider.HasOriginalPose || rider.bones == null || rider.bones.Length < OriginalRider.OriginalBoneCount) return;
            var g = For(bike);
            if (g.right == null && g.left == null) return;
            if (!fists.TryGetValue(rider, out var last)) fists.Add(rider, last = new Vector3[4]);
            if (g.right != null) Hand(rider.bones, 0, g.right, last);
            if (g.left != null) Hand(rider.bones, LeftOffset, g.left, last);
        }

        /// The grips of a vehicle model (found once from its meshes; a hand without one: null).
        public static Grips For(ReflexBikeModel bike)
        {
            if (grips.TryGetValue(bike, out var g)) return g;
            g = new Grips { right = Find(bike, "NDE_RHT_Hand"), left = Find(bike, "NDE_LFT_Hand") };
            grips.Add(bike, g);
            return g;
        }

        static void Hand(Transform[] bones, int o, Grip grip, Vector3[] last)
        {
            Transform upper = bones[RightArm + o], lower = bones[RightArm + 1 + o], wrist = bones[RightArm + 2 + o];
            if (!upper || !lower || !wrist || !grip.part) return;
            int k = o == 0 ? 0 : 2;
            if (Fist(bones, o, out var hole, out var dir)) { last[k] = wrist.InverseTransformPoint(hole); last[k + 1] = wrist.InverseTransformDirection(dir); }
            else if (last[k + 1] != Vector3.zero) { hole = wrist.TransformPoint(last[k]); dir = wrist.TransformDirection(last[k + 1]); }   // (the fingers opened: the fist where it was in the hand)
            else return;
            Vector3 end = grip.part.TransformPoint(grip.end), axis = grip.part.TransformDirection(grip.axis);
            // the animation's fist to the grip (the segment from its end inward)
            float t = Vector3.Dot(hole - end, axis);
            float d = Vector3.Distance(hole, end + axis * Mathf.Clamp(t, -grip.length, 0f));
            float w = d <= Near ? 1f : d >= Far ? 0f : Smooth(1f - (d - Near) / (Far - Near));
            if (w <= 0f) return;
            // the fist turned onto the grip's axis (about its centre), then its centre onto the axis at the grip's middle (the glove
            // centred on the rubber: the fist's centre is the glove's middle along the grip within 0.01 ft, tests/rider_grips)
            float turn = Vector3.Angle(dir, axis), tw = turn <= TurnFull ? 1f : turn >= TurnNone ? 0f : Smooth(1f - (turn - TurnFull) / (TurnNone - TurnFull));
            Quaternion q = Quaternion.Slerp(Quaternion.identity, Quaternion.FromToRotation(dir, axis), w * tw);
            float along = -Mathf.Max(HalfHand, grip.length * 0.5f);
            Vector3 target = Vector3.Lerp(hole, end + axis * along, w);
            var rotation = q * wrist.rotation;
            Vector3 wristTarget = target - q * (hole - wrist.position);
            RiderLimbRetarget.TwoBone(upper, lower, wrist, wristTarget);
            wrist.rotation = rotation;
        }
        static float Smooth(float x) => x * x * (3f - 2f * x);

        /// The fist: the centre of the circles the four fingers curl along (each through its three joints) and their line, from the index
        /// finger towards the pinky (outward along a grip for either hand). False: a finger is straight (no fist).
        static bool Fist(Transform[] bones, int o, out Vector3 centre, out Vector3 dir)
        {
            centre = dir = Vector3.zero;
            Vector3 inner = Vector3.zero, outer = Vector3.zero;
            for (int f = 0; f < 4; f++)
            {
                Transform a = bones[Fingers[3 * f] + o], b = bones[Fingers[3 * f + 1] + o], c = bones[Fingers[3 * f + 2] + o];
                if (!a || !b || !c || !Circumcentre(a.position, b.position, c.position, out var cc)) return false;
                centre += cc;
                if (f < 2) inner += cc; else outer += cc;
            }
            centre *= 0.25f; dir = outer - inner;
            if (dir.sqrMagnitude < 1e-6f) return false;
            dir.Normalize();
            return true;
        }
        static bool Circumcentre(Vector3 p, Vector3 q, Vector3 r, out Vector3 centre)
        {
            Vector3 a = p - r, b = q - r, n = Vector3.Cross(a, b);
            float nn = n.sqrMagnitude;
            centre = r;
            if (nn < 1e-8f) return false;
            centre = r + Vector3.Cross(a.sqrMagnitude * b - b.sqrMagnitude * a, n) / (2f * nn);
            return (centre - r).sqrMagnitude < 0.09f;   // (a curl tighter than 0.3 ft: a fist)
        }

        // ---------------------------------------------------------------------------------------------------------------------------
        // the grip from the bike model's meshes (model space): about the hand locator, each part's outermost end (its vertices within
        // 0.06 ft of the part's outermost point along the bars) is a grip's end when it is a ring: flat, 0.035 - 0.1 ft in radius, its axis
        // within 45 degrees of the bars' line. The outermost such ring (the grip's, not the bar end inside it; a hand guard or a lever is not
        // a ring) is the grip; its length is the tube behind it in the same part (vertices within 1.35 ring radii of the axis, at most
        // 0.5 ft). Levers (*LVR nodes) are not grips. The hand locator lies on the grip: inside its end (0.05 ft or more), not past its
        // inner end by over 0.15 ft, within 0.3 ft of its axis (the original grips: 0.2 - 0.25 ft in, 0.09 - 0.15 off; a throttle
        // housing, a perch clamp or a bar end inboard of the hand is not its grip). "Flat": a rounded or bevelled end (the mods' grips)
        // spreads 0.2 of its radius along the axis.
        static Grip Find(ReflexBikeModel bike, string locator)
        {
            int ln = bike.hierarchyNodes == null ? -1 : System.Array.IndexOf(bike.hierarchyNodes, locator);
            if (ln < 0 || bike.hierarchyBind == null || ln >= bike.hierarchyBind.Length) return null;
            Vector3 L = bike.hierarchyBind[ln].GetColumn(3);
            float side = L.x >= 0f ? 1f : -1f;
            Grip best = null; float bestOut = float.NegativeInfinity;
            var near = new List<Vector3>();
            foreach (var p in bike.parts)
            {
                if (!p.part || p.node < 0 || p.node >= bike.hierarchyNodes.Length || bike.hierarchyNodes[p.node].Contains("LVR")) continue;
                var mesh = p.part.GetComponent<MeshFilter>()?.sharedMesh;
                if (!mesh || !mesh.isReadable) continue;
                near.Clear();
                foreach (var v in mesh.vertices) if ((v - L).sqrMagnitude < Search * Search && side * (v.x - L.x) > -0.45f) near.Add(v);
                if (near.Count < 8) continue;
                float outMost = float.NegativeInfinity;
                foreach (var v in near) outMost = Mathf.Max(outMost, side * v.x);
                var ring = near.FindAll(v => side * v.x >= outMost - 0.06f);
                if (ring.Count < 6 || !Ring(ring, side, out var c, out var axis, out float radius)) continue;
                if (radius < 0.035f || radius > 0.1f || Mathf.Abs(axis.x) < 0.7f || (c - L).magnitude > 0.45f) continue;
                // the tube behind the end
                float length = 0f;
                foreach (var v in near)
                {
                    float t = Vector3.Dot(v - c, axis);
                    if (t < 0f && t > -0.5f && Vector3.Cross(v - c, axis).magnitude < 1.35f * radius) length = Mathf.Max(length, -t);
                }
                if (length < 0.2f) length = 0.35f;   // (no tube found behind the end: a grip's length)
                float tl = Vector3.Dot(L - c, axis);
                if (tl > -0.05f || tl < -(length + 0.15f) || Vector3.Cross(L - c, axis).magnitude > 0.3f) continue;   // (the hand is not on it)
                if (outMost > bestOut) { bestOut = outMost; best = new Grip { part = p.part, end = c, axis = axis, length = length, radius = radius }; }
            }
            if (best != null) Debug.Log($"[grips] {bike.chassis?.name}: {locator} grip on {best.part.name}: end {best.end.ToString("F3")}, axis {best.axis.ToString("F3")}, {best.length:F2} ft long, radius {best.radius:F3} (the locator {(L - best.end).ToString("F3")} from its end)");
            else Debug.Log($"[grips] {bike.chassis?.name}: no grip found about {locator}");
            return best;
        }

        /// A flat ring of vertices: its centre, the plane's normal (pointing to `side` along x) and its mean radius. False: not flat.
        static bool Ring(List<Vector3> pts, float side, out Vector3 c, out Vector3 normal, out float radius)
        {
            c = Vector3.zero; foreach (var p in pts) c += p; c /= pts.Count;
            double xx = 0, xy = 0, xz = 0, yy = 0, yz = 0, zz = 0;
            foreach (var p in pts) { var d = p - c; xx += d.x * d.x; xy += d.x * d.y; xz += d.x * d.z; yy += d.y * d.y; yz += d.y * d.z; zz += d.z * d.z; }
            // the smallest eigenvector: power iteration on (trace I - M), whose largest is M's smallest
            double tr = xx + yy + zz;
            Vector3 v = new Vector3(side, 0.05f, 0.05f).normalized;
            for (int k = 0; k < 64; k++)
            {
                var n = new Vector3((float)((tr - xx) * v.x - xy * v.y - xz * v.z), (float)(-xy * v.x + (tr - yy) * v.y - yz * v.z), (float)(-xz * v.x - yz * v.y + (tr - zz) * v.z));
                if (n.sqrMagnitude < 1e-20f) break;
                v = n.normalized;
            }
            normal = v.x * side >= 0f ? v : -v;
            double along = 0, across = 0;
            foreach (var p in pts) { var d = p - c; float a = Vector3.Dot(d, normal); along += a * a; across += (d - normal * a).sqrMagnitude; }
            radius = 0f; foreach (var p in pts) radius += Vector3.Cross(p - c, normal).magnitude; radius /= pts.Count;
            return across > 0 && along / pts.Count < 0.3 * 0.3 * (across / pts.Count) + 1e-6;   // (flat: the spread across the axis well over the spread along it)
        }
    }
}

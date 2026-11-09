using System;
using System.Collections;
using System.Collections.Generic;
using System.Linq;
using UnityEngine;
using Reflex.Port.Content;
using Reflex.Port.RxCore.Bridge;

namespace Reflex.Port.OriginalWorld
{
    // The other riders of the race (the AI, and further humans), drawn 1:1 from the running game: each slot's bike and rider are
    // built at run time from the player's install as the tables give them (ReflexRoster: Content/ReflexBikeModel and
    // ReflexRiderModel, one per frame so the gate is not held up), and posed every completed frame from the simulation thread's
    // per-slot exports (reflex_world_bike_pose_slot: the bike's MX_Hierarchy nodes, as OriginalBikePose binds them;
    // reflex_world_rider_pose_slot: the riding rider's 69 bones, as OriginalRider). Number plates / names: the game's run-time
    // textures, made again by ReflexPlates from the slot's own text (ReflexPlateText).
    [DisallowMultipleComponent]
    public sealed class OriginalOpponents : MonoBehaviour
    {
        // elapsed milliseconds since a Stopwatch.GetTimestamp() (no Stopwatch object a frame, w-gcspikes)
        static double Ms(long since) => (System.Diagnostics.Stopwatch.GetTimestamp() - since) * 1000.0 / System.Diagnostics.Stopwatch.Frequency;
        [Tooltip("Draw the other riders of the race.")]
        public bool show = true;
        [Tooltip("Riders within this distance (feet) of the camera have their skeleton evaluated every frame; farther ones every 'Far rider interval'-th frame (the evaluation runs original code on the simulation thread).")]
        public float riderNearDistance = 200f;
        [Tooltip("1: every rider every frame.")]
        public int farRiderInterval = 3;
        public Camera viewer;
        // RIDER CAP (w-rc-client): the far riders of a field past the original's 12 are drawn simpler. Beyond bigFieldFarDistance (feet, with
        // FarHysteresis) a rider's small bike parts are not drawn (Renderer.forceRenderingOff: a part whose mesh is under farSmallPart feet across),
        // its hands are not closed on the grips (RiderGripIK) and its run-time vehicle constants wait for it to come near. A field of 12 or fewer
        // is drawn exactly as before.
        [Tooltip("RIDER CAP: in a field past 12 riders, riders farther than this (feet) from the camera drop their small parts and extras.")]
        public float bigFieldFarDistance = 220f;
        [Tooltip("RIDER CAP: the bike parts hidden on a far rider of a big field: meshes under this size (feet, the bounds' diagonal).")]
        public float farSmallPart = 1.2f;
        const float FarHysteresis = 0.1f;
        /// The field is past the original's 12 riders (set by Begin from the roster): the far-rider simplification is on.
        public bool BigField { get; private set; }
        /// Diagnostics: the riders drawn simpler now (far, in a big field), and the bike parts that hides.
        public int FarRiders { get; private set; }
        public int FarHiddenParts { get; private set; }

        sealed class Opponent
        {
            public ReflexRoster.Entry entry; public string key;
            public GameObject root; public ReflexBikeModel bike; public ReflexRiderModel rider;
            public bool built, posed; public Vector3 position;
            public Matrix4x4 bikeAtRiderPose = Matrix4x4.identity; public bool riderPosed;
            public Renderer[] smallParts = new Renderer[0];   // RIDER CAP: the parts a far rider of a big field does not draw
            public Renderer[] bikeRenderers;                  // every renderer of the bike (ReflexObjectShading.ApplyVehicleConstants without a search a frame)
            public bool far;
        }
        readonly List<Opponent> opponents = new List<Opponent>();
        readonly Dictionary<int, int> bySlot = new Dictionary<int, int>();
        OriginalSimulation simulation;
        IReflexSimDriver driver;   // REFLEX_SIM=open: the core driver (TrackSlots / SetRiderDetail / farRiderInterval); legacy: null
        Coroutine building;
        bool[] near = new bool[0], sentNear;
        public int Count => opponents.Count;
        /// The riders as built (diagnostics / tests): name, slot, bike, rider, posed yet.
        public IEnumerable<(string name, int slot, ReflexBikeModel bike, ReflexRiderModel rider, bool posed)> Riders =>
            opponents.Select(o => (o.entry.name, o.entry.slot.slot, o.bike, o.rider, o.posed));
        public int Built => opponents.Count(o => o.built);
        public int Posed => opponents.Count(o => o.posed);
        public double buildMilliseconds;
        public double LastSlotMilliseconds { get; private set; }

        /// The world was made (the roster read before the simulation thread started): builds what changed, tracks the slots.
        public void Begin(ReflexRoster roster, OriginalSimulation sim) => Begin(roster, sim, null);
        /// The selected simulation (REFLEX_SIM=open: the core driver; legacy: its OriginalSimulation) - the slot requests go to it.
        public void Begin(ReflexRoster roster, IReflexSimDriver sim) => Begin(roster, sim?.Legacy, sim);
        void Begin(ReflexRoster roster, OriginalSimulation sim, IReflexSimDriver drv)
        {
            simulation = sim; driver = drv;
            // (REFLEX_BIGFIELD_LOD=0: a big field drawn as a field of 12 is, every rider in full: the A / B of the drawing cost, tests only)
            BigField = roster.entries.Count > Reflex.Port.RxCore.RxRiderCap.Original && Environment.GetEnvironmentVariable("REFLEX_BIGFIELD_LOD") != "0";
            var wanted = roster.entries.Where(e => !e.Local).ToList();
            string Key(ReflexRoster.Entry e) => e.slot.slot + "#" + (e.bike?.Key ?? "-") + "#" + (e.rider?.Key ?? "-");
            bool same = wanted.Count == opponents.Count && wanted.Select(Key).SequenceEqual(opponents.Select(o => o.key));
            if (!same)
            {
                Clear();
                foreach (var e in wanted) opponents.Add(new Opponent { entry = e, key = Key(e) });
                if (opponents.Count > 0) building = StartCoroutine(BuildAll());
            }
            foreach (var o in opponents) { o.posed = false; if (o.root) o.root.SetActive(false); }
            bySlot.Clear();
            for (int i = 0; i < opponents.Count; i++) bySlot[opponents[i].entry.slot.slot] = i;
            near = new bool[opponents.Count]; sentNear = null;
            if (driver != null) { driver.farRiderInterval = Mathf.Max(1, farRiderInterval); driver.TrackSlots(opponents.Select(o => o.entry.slot.slot).ToArray()); }
            else if (sim != null) { sim.farRiderInterval = Mathf.Max(1, farRiderInterval); sim.TrackSlots(opponents.Select(o => o.entry.slot.slot).ToArray()); }
            if (opponents.Count > 0) Debug.Log($"Reflex opponents: {opponents.Count} riders (roster read in {roster.milliseconds:0.0} ms)\n  " + string.Join("\n  ", roster.entries));
        }

        IEnumerator BuildAll()
        {
            yield return null;
            var watch = new System.Diagnostics.Stopwatch();
            foreach (var o in opponents)
            {
                watch.Start();
                try
                {
                    o.root = new GameObject($"Opponent {o.entry.slot.slot} · {o.entry.name}"); o.root.transform.SetParent(transform, false); o.root.SetActive(false);
                    o.bike = ReflexBikeModel.Build(o.entry.bike);   // null: the default MarsF Lite
                    foreach (var g in new[] { o.bike.chassis, o.bike.frontWheel.gameObject, o.bike.rearWheel.gameObject }) g.transform.SetParent(o.root.transform, false);
                }
                catch (Exception e) { Debug.LogWarning($"Reflex opponent {o.entry.name}: bike not built: {e.Message}"); }
                watch.Stop();
                yield return null;
                watch.Start();
                try
                {
                    o.rider = ReflexRiderModel.Build(o.entry.rider);
                    o.rider.root.transform.SetParent(o.root.transform, false);
                }
                catch (Exception e) { Debug.LogWarning($"Reflex opponent {o.entry.name}: rider not built: {e.Message}"); }
                watch.Stop();
                ReflexPlates.Apply(o.bike, o.rider, o.entry.plate);
                Prepare(o);
                o.built = o.bike != null;
                yield return null;
            }
            buildMilliseconds = watch.Elapsed.TotalMilliseconds;
            Debug.Log($"Reflex opponents: {Built}/{opponents.Count} built in {buildMilliseconds:0} ms");
            building = null;
        }

        // RIDER CAP: what a far rider of a big field leaves out, collected once per built rider. In a big field the rider's skinned meshes are
        // culled when off screen (their bounds follow the pelvis, MXRider_Root: no skinning for a rider out of view, a grid row behind the
        // camera); the bike's small parts are listed.
        void Prepare(Opponent o)
        {
            if (BigField && o.rider?.rider?.skinnedMeshes != null)   // (a field of 12 or fewer keeps updateWhenOffscreen, as before)
                foreach (var smr in o.rider.rider.skinnedMeshes)
                {
                    if (!smr) continue;
                    smr.updateWhenOffscreen = false;
                    smr.localBounds = new Bounds(Vector3.zero, Vector3.one * RiderBoundsFeet);   // about the root bone: a whole rider, arms and legs out
                }
            if (o.bike == null) return;
            var all = new List<Renderer>(); var small = new List<Renderer>();
            foreach (var g in new[] { o.bike.chassis, o.bike.frontWheel ? o.bike.frontWheel.gameObject : null, o.bike.rearWheel ? o.bike.rearWheel.gameObject : null })
                if (g) all.AddRange(g.GetComponentsInChildren<Renderer>(true));
            foreach (var r in all)
                if (r is MeshRenderer && r.TryGetComponent<MeshFilter>(out var f) && f.sharedMesh && f.sharedMesh.bounds.size.magnitude * r.transform.lossyScale.x < farSmallPart) small.Add(r);
            o.bikeRenderers = all.ToArray(); o.smallParts = small.ToArray();
        }
        /// The rider's skinned bounds about its root bone (feet): the off-screen test of an opponent rider (RIDER CAP; Prepare).
        public const float RiderBoundsFeet = 16f;

        void SetFar(Opponent o, bool far)
        {
            if (o.far == far) return;
            o.far = far;
            foreach (var r in o.smallParts) if (r) r.forceRenderingOff = far;
        }

        /// A completed frame's per-slot poses.
        public void Apply(OriginalSimulation.Results r)
        {
            long watch = System.Diagnostics.Stopwatch.GetTimestamp();
            LastSlotMilliseconds = r.slotMilliseconds;
            Vector3 eye = viewer ? viewer.transform.position : Vector3.zero;
            int farRiders = 0, farParts = 0;
            for (int i = 0; i < r.slotCount; i++)
            {
                if (!bySlot.TryGetValue(r.slots[i], out int k)) continue;
                var o = opponents[k];
                if (!o.built || r.slotBikeNodes[i] < OriginalBikePose.NodeCount) continue;
                if (!show) { if (o.root.activeSelf) o.root.SetActive(false); continue; }
                if (!o.bike.pose.Apply(r.slotBikePose[i], r.slotBikeNodes[i])) continue;
                if (i < r.slotNodeFlagCounts.Length) o.bike.pose.ApplyFlags(r.slotNodeFlags[i], r.slotNodeFlagCounts[i]);   // a machine's damage panels
                var bikeWorld = OriginalBikePose.FromRows(r.slotBikePose[i], 0);
                o.position = bikeWorld.GetColumn(3);
                // RIDER CAP: a far rider of a big field (never in a field of 12 or fewer, never without a camera)
                if (BigField && viewer)
                {
                    float d2 = (o.position - eye).sqrMagnitude, edge = bigFieldFarDistance * (o.far ? 1f - FarHysteresis : 1f + FarHysteresis);
                    SetFar(o, d2 > edge * edge);
                }
                else SetFar(o, false);
                if (o.far) { farRiders++; farParts += o.smallParts.Length; }
                if (o.rider != null)
                {
                    var rt = o.rider.root.transform;
                    if (r.slotRiderBones[i] == OriginalRider.OriginalBoneCount)
                    {
                        rt.SetPositionAndRotation(Vector3.zero, Quaternion.identity);   // the bones are world matrices
                        if (o.rider.rider.ApplyBoneMatrices(r.slotRiderPose[i], r.slotRiderBones[i]))
                        {
                            o.bikeAtRiderPose = bikeWorld; o.riderPosed = true;
                            if (!o.far) RiderGripIK.Apply(o.rider.rider, o.bike);   // OPEN REFLEX: the hands closed on the grips (w-grips; not on a far rider of a big field)
                        }
                    }
                    else if (o.riderPosed)
                    {
                        // a far rider's skeleton not evaluated this frame: its last pose rides along with the bike (frame 0)
                        var d = bikeWorld * o.bikeAtRiderPose.inverse;
                        Vector3 f = d.GetColumn(2), u = d.GetColumn(1);
                        if (f.sqrMagnitude > 1e-6f && u.sqrMagnitude > 1e-6f) rt.SetPositionAndRotation(d.GetColumn(3), Quaternion.LookRotation(f, u));
                    }
                }
                if (!o.posed) { o.posed = true; o.root.SetActive(true); }
                if (ReflexMud.Find(r.mud, r.mudCount, o.entry.slot.slot, out var mud)) ReflexMud.Apply(o.bike, o.rider, mud);   // the slot's mud record
                if (!o.far) ReflexObjectShading.ApplyVehicleConstants(o.entry.slot.vehicle, o.bike, o.bikeRenderers);   // its run-time shader constants (belt scroll, flywheel blur)
                if (k < near.Length) near[k] = !viewer || (o.position - eye).sqrMagnitude < riderNearDistance * riderNearDistance;
            }
            FarRiders = farRiders; FarHiddenParts = farParts;
            // the near flags go to the simulation thread as a new array only when one changed (it keeps the array it was given: no copy a frame, w-rc-client)
            bool changed = sentNear == null || sentNear.Length != near.Length;
            for (int i = 0; !changed && i < near.Length; i++) changed = sentNear[i] != near[i];
            if (driver != null) { driver.farRiderInterval = Mathf.Max(1, farRiderInterval); if (changed) driver.SetRiderDetail(sentNear = (bool[])near.Clone()); }
            else if (simulation != null) { simulation.farRiderInterval = Mathf.Max(1, farRiderInterval); if (changed) simulation.SetRiderDetail(sentNear = (bool[])near.Clone()); }
            LastApplyMilliseconds = Ms(watch);
        }
        /// The live models the post-race scenario hides (OriginalPodium.Frame: each slot's rider / vehicle drawn flags); null or an
        /// invalid frame shows them all again.
        public void ApplyDrawn(OriginalPodium.Frame f, bool hideAll = false)
        {
            foreach (var o in opponents)
            {
                if (!o.built || !o.posed) continue;
                bool riderOn = true, vehicleOn = true;
                if (hideAll) riderOn = vehicleOn = false;
                else if (f != null && f.valid) f.TryDrawn(o.entry.slot.slot, out riderOn, out vehicleOn);
                if (o.bike != null && o.bike.chassis.activeSelf != vehicleOn)
                {
                    o.bike.chassis.SetActive(vehicleOn); o.bike.frontWheel.gameObject.SetActive(vehicleOn); o.bike.rearWheel.gameObject.SetActive(vehicleOn);
                }
                if (o.rider?.root && o.rider.root.activeSelf != riderOn) o.rider.root.SetActive(riderOn);
            }
        }

        /// What posing the riders cost on the main thread for the last completed frame (ms).
        public double LastApplyMilliseconds { get; private set; }

        void Clear()
        {
            if (building != null) { StopCoroutine(building); building = null; }
            foreach (var o in opponents) { o.bike?.Destroy(); o.rider?.Destroy(); if (o.root) Destroy(o.root); }
            opponents.Clear(); bySlot.Clear();
        }

        void OnDestroy() { Clear(); ReflexBikeModel.ReleaseParsed(); ReflexRiderModel.ReleaseParsed(); }
    }
}

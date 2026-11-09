// MUD BUILDUP (OPTIONS > VIDEO; Shaders/ReflexVehiclePBR.hlsl; not part of the original game): the dirt on a vehicle and its rider, one state per vehicle.
// It is drawn with the enhanced materials (ReflexEnhancedMaterials.MudOn) and replaces the original's own mud term then; MUD BUILDUP OFF clears both
// (HidesOriginalMud; without the enhanced materials, ON shows the original's own mud).
// Read-only on the game: ReflexMud.Apply hands over the vehicle's own mud record (amount, wetness, colour: the game's MudRecord_Update, which integrates the
// level's MudManager rate of the terrain type under the vehicle while it moves on the ground, washes it off in water and clears it on a restart) and the
// models' transforms say the rest:
//   fresh      what the wheels throw up while the vehicle rides: grows while a wheel is on the terrain and the vehicle is moving, at a rate that follows the
//              game's own mud rate of the terrain under it (hard pack slow, loam / mud fast; the game's rate is read back from its record's change; none on
//              levels without a MudManager, so the stadiums stay clean), washes off where the game's record washes off (water); lost at once on a restart,
//              a respawn or a reset (the vehicle jumps, or the game's record drops)
//   the game's record  its amount counts too (a rider that already was muddy when this state began: replays, the podium): 1 - exp(-3.2 x amount)
//   shown      max(fresh, record), capped at 0.88 (the shader caps the coverage too: the graphics always show through)
// The shader gets _RfxDirt (amount, wetness, model units per metre, seed), _RfxDirtColor and _RfxDirtF (the vehicle's forward, for the roost side) on every
// Reflex/Vehicle material of the vehicle's models; the dirt mask is the rest-pose height (the mesh as built: ground = 0), so it sticks to the wheels and parts.
using System;
using System.Collections.Generic;
using Reflex.Port.OriginalWorld;
using Reflex.Port.Rendering;
using UnityEngine;

namespace Reflex.Port.Content
{
    [DisallowMultipleComponent, DefaultExecutionOrder(31100)]
    public sealed class ReflexDirt : MonoBehaviour
    {
        /// Tests: forces the shown amount (0..1, -1 = off) and wetness for every vehicle (REFLEX_DIRT=0.5[,0.3] sets it at start).
        public static float ForceAmount = -1, ForceWet = 0;
        /// The original's own mud term gets a zero amount: replaced by this dirt (MUD BUILDUP with the enhanced materials) or MUD BUILDUP OFF.
        public static bool HidesOriginalMud => !ReflexEnhancedMaterials.OriginalMudOn;
        /// Bumped when HidesOriginalMud changes (ReflexMudMaterials rewrites its constants).
        public static int Version { get; private set; }
        static bool lastReplace; static readonly List<ReflexDirt> all = new List<ReflexDirt>();

        static readonly int DirtId = Shader.PropertyToID("_RfxDirt"), ColorId = Shader.PropertyToID("_RfxDirtColor"), FwdId = Shader.PropertyToID("_RfxDirtF");

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        static void ReadEnvironment()
        {
            string v = Environment.GetEnvironmentVariable("REFLEX_DIRT");
            if (string.IsNullOrEmpty(v)) return;
            var p = v.Split(',');
            if (float.TryParse(p[0], System.Globalization.NumberStyles.Float, System.Globalization.CultureInfo.InvariantCulture, out float a)) ForceAmount = a;
            if (p.Length > 1 && float.TryParse(p[1], System.Globalization.NumberStyles.Float, System.Globalization.CultureInfo.InvariantCulture, out float w)) ForceWet = w;
        }

        ReflexBikeModel bike; ReflexRiderModel rider;
        readonly List<Material> materials = new List<Material>();
        int collectedFor = -1;
        ReflexMudState state; bool haveState;
        // dynamics
        float fresh, rate, lastGame = -1, lastChangeTime; Vector3 lastPos; bool havePos; float wheelRadius, unit = 1;
        float shownAmount = -1, shownWet = -1; Vector4 shownColor, shownFwd; float seed;

        /// Called with every valid mud record of a vehicle (ReflexMud.Apply): the models it belongs to and the record.
        public static void Apply(ReflexBikeModel bike, ReflexRiderModel rider, in ReflexMudState s)
        {
            GameObject host = bike?.chassis ? bike.chassis : rider?.root;
            if (!host) return;
            if (!host.TryGetComponent<ReflexDirt>(out var d)) { d = host.AddComponent<ReflexDirt>(); d.hideFlags = HideFlags.DontSave; d.seed = UnityEngine.Random.value * 40f; }
            d.bike = bike; d.rider = rider; d.state = s; d.haveState = true;
        }

        void OnEnable() { all.Add(this); }
        void OnDisable() { all.Remove(this); }

        void Collect()
        {
            materials.Clear();
            void Add(GameObject g)
            {
                if (!g) return;
                foreach (var r in g.GetComponentsInChildren<Renderer>(true))
                    foreach (var m in r.sharedMaterials) if (m && m.HasProperty(DirtId) && !materials.Contains(m)) materials.Add(m);
            }
            if (bike != null) { Add(bike.chassis); if (bike.frontWheel) Add(bike.frontWheel.gameObject); if (bike.rearWheel) Add(bike.rearWheel.gameObject); }
            if (rider != null) Add(rider.root);
            collectedFor = (bike != null ? 1 : 0) + (rider != null ? 2 : 0);
            // the model units per metre: the rear tyre is ~0.66 m (a motocross tyre; ATVs / machines are within a third of that)
            if (bike != null && bike.rearWheel)
            {
                var b = new Bounds(); bool any = false;
                foreach (var r in bike.rearWheel.GetComponentsInChildren<MeshRenderer>(true)) { if (!any) { b = r.bounds; any = true; } else b.Encapsulate(r.bounds); }
                if (any) { wheelRadius = Mathf.Max(b.size.x, b.size.y, b.size.z) * 0.5f; unit = Mathf.Max(0.5f, wheelRadius * 2f / 0.66f); }
            }
        }

        /// Once a frame (ReflexEnhancedMaterials): when MUD BUILDUP (or the look) changed, the original mud constants are rewritten.
        public static void CheckReplace()
        {
            bool replace = HidesOriginalMud;
            if (replace != lastReplace) { lastReplace = replace; Version++; foreach (var d in all) d.RefreshOriginalMud(); }
        }

        void RefreshOriginalMud()
        {
            if (bike != null) { Refresh(bike.chassis); if (bike.frontWheel) Refresh(bike.frontWheel.gameObject); if (bike.rearWheel) Refresh(bike.rearWheel.gameObject); }
            if (rider != null) Refresh(rider.root);
        }
        static void Refresh(GameObject g) { if (g && g.TryGetComponent<ReflexMudMaterials>(out var m)) m.Refresh(); }

        void LateUpdate()
        {
            if (!haveState) return;
            int want = (bike != null ? 1 : 0) + (rider != null ? 2 : 0);
            if (collectedFor != want || materials.Count == 0 || (materials.Count > 0 && !materials[0])) Collect();
            float dt = Mathf.Clamp(Time.deltaTime, 0f, 0.1f);
            float game = Mathf.Clamp01(state.amount.w);

            // ---- the dynamics (the vehicle's wheels, the game's record)
            var wheel = bike != null && bike.rearWheel ? bike.rearWheel : null;
            Vector3 pos = wheel ? wheel.position : (rider != null && rider.root ? rider.root.transform.position : Vector3.zero);
            bool grounded = false; float speed = 0; bool jumped = false;
            if (wheel && havePos && dt > 1e-4f)
            {
                var d = pos - lastPos;
                jumped = d.magnitude > 4.0f * unit;            // 4 m in one frame: a reset / respawn / teleport
                var flat = new Vector3(d.x, 0, d.z);
                speed = flat.magnitude / dt / unit;            // m/s
                grounded = OnGround(pos);
            }
            else if (!wheel) { grounded = false; }
            lastPos = pos; havePos = wheel != null;

            if (lastGame < 0) { lastGame = game; lastChangeTime = Time.time; }
            if (!Mathf.Approximately(game, lastGame))
            {
                float span = Mathf.Max(0.02f, Time.time - lastChangeTime);
                if (game + 0.12f < lastGame || game <= 0.001f && lastGame > 0.05f) { fresh = 0; rate = 0; }   // the game's restart: its record is cleared
                else rate = Mathf.Lerp(rate, (game - lastGame) / span, 1f - Mathf.Exp(-span / 1.5f));
                lastGame = game; lastChangeTime = Time.time;
            }
            else if (Time.time - lastChangeTime > 0.4f) rate *= Mathf.Exp(-dt / 0.5f);
            if (jumped) { fresh = 0; rate = 0; }
            if (dt > 0f)
            {
                if (grounded && speed > 2f && rate > 0)
                {
                    float k = Mathf.Clamp(rate / 0.004f, 0f, 2.5f) * Mathf.Clamp01((speed - 2f) / 12f);
                    fresh += k * dt * 0.0135f * (1f - 0.45f * fresh);
                }
                if (rate < -0.01f) fresh -= dt * Mathf.Clamp01(-rate / 0.05f) * 0.25f;
                fresh = Mathf.Clamp(fresh, 0f, 0.86f);
            }

            float amount = Mathf.Min(0.88f, Mathf.Max(fresh, 1f - Mathf.Exp(-3.2f * game)));
            float wet = Mathf.Max(Mathf.Clamp01(state.wetness.w), Mathf.Clamp01((rate - 0.02f) / 0.06f) * 0.9f);
            if (!ReflexEnhancedMaterials.MudOn) { amount = 0; wet = 0; }
            if (ForceAmount >= 0f) { amount = Mathf.Min(0.88f, ForceAmount); wet = ForceWet; }

            var col = (Vector4)state.color;
            if (col.x + col.y + col.z < 0.02f) col = new Vector4(0.30f, 0.24f, 0.17f, 1f);
            col.w = 1f;
            var part = bike != null && bike.parts.Count > 0 ? (bike.parts.Find(p => p.node == 0).part ?? bike.parts[0].part) : null;
            Vector3 f = part ? part.forward : (rider != null && rider.root ? rider.root.transform.forward : Vector3.forward);
            var fwd = new Vector4(f.x, f.y, f.z, 0);

            // (the forward turns with the vehicle: written only when it moved noticeably, and not at all while clean)
            bool same = Mathf.Abs(amount - shownAmount) < 0.002f && Mathf.Abs(wet - shownWet) < 0.01f && col == shownColor
                        && (amount < 0.004f || (fwd - shownFwd).sqrMagnitude < 0.0025f);
            if (same) return;
            shownAmount = amount; shownWet = wet; shownColor = col; shownFwd = fwd;
            var dirt = new Vector4(amount, wet, unit, seed);
            foreach (var m in materials) { if (!m) continue; m.SetVector(DirtId, dirt); m.SetVector(ColorId, col); m.SetVector(FwdId, fwd); }
        }

        /// Whether the wheel is on the terrain: its lowest point within a few centimetres of the ground under it (true without a terrain).
        bool OnGround(Vector3 wheelCentre)
        {
            var t = Terrain.activeTerrain;
            if (!t || !t.terrainData) return true;
            float ground = t.SampleHeight(wheelCentre) + t.transform.position.y;
            return wheelCentre.y - wheelRadius - ground < 0.25f * wheelRadius + 0.1f * unit;
        }
    }
}

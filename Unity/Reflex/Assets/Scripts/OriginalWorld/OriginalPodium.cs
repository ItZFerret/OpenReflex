using System.Collections.Generic;
using Reflex.Port.Content;
using UnityEngine;

namespace Reflex.Port.OriginalWorld
{
    // The scenario cast (w-podium, w-cast): the game's scenarios play on the level cast (world + 0x20150), one RiderCharacter per player
    // (0074ba40 PodiumRiders_Create: his own gear on the MXRider_Hierarchy skeleton). While the activity is in Pre / Countdown the intro
    // scenario (RaceIntroScenario / FMXIntroScenario / WaypointIntroScenario: the start-line overview, the hero shots, the crew chief's
    // shots) plays them on the start line; in Post the finish scenario (RaceFinishScenario / FMXFinishScenario, core/game/
    // anim_scenario_finish.c) plays the finish replay and the podium with them (scenario 3 / 1 / 2 in Frame). What the renderer draws:
    //  - the RiderCharacters the original draws: a character is submitted (RiderCharacter::vf2_o90 00624b70 -> Model_SubmitSkinned 007f4e50)
    //    when its model instance is visible (+0xa2) and the scenario placed it (+0x70), and is then culled beyond its model's draw distance
    //    from the camera (RiderData.drawDistance, 1000 for the rider model). A step places only its actors: the others keep their spawn
    //    matrix, the world origin, and pass the same test there; drawn with that slot's rider model (Content/ReflexRiderModel, as
    //    OriginalOpponents builds them), posed from their 69 bone world matrices (rx_session_podium), with the player's number / name
    //    plates (ReflexPlates: 0074ba40 binds the player's own plate textures on them) and their own mud constants (+0x3b0 amount from the
    //    vehicle record (0074bcc0), +0x3c0 the level's MudManager colour, +0x3d0 wetness: Content/ReflexMud);
    //  - the live riders and vehicles the scenario hides (the rider's +0xa2, the vehicle model's +0x14e: the intro hides the riding rider
    //    for the cast's start-line pose, the podium hides both): OriginalWorldRuntime / OriginalOpponents read Frame.TryDrawn and hide them
    //    as the original does not draw them;
    //  - the crew chiefs and scenario actors (the TrophyGirl, the podium crowd) are ambient entities (OriginalAmbient);
    //  - the camera is the scenario's MXAnimatedCamera (rx_session_camera while a scenario plays); the finish replay's camera is
    //    the replay renderer's (OwnReplayRenderer).
    [DisallowMultipleComponent]
    public sealed class OriginalPodium : MonoBehaviour
    {
        public const int MaxRiders = Reflex.Port.RxCore.RxRiderCap.Max, Bones = OriginalRider.OriginalBoneCount;

        /// One completed frame's copy of rx_session_podium (OpenReflexResultsAdapter fills it).
        public sealed class Frame
        {
            public bool valid; public int scenario, step, postState;
            public int riderCount; public readonly RiderData[] riders = new RiderData[MaxRiders];
            public int slotCount; public readonly int[] slots = new int[MaxRiders];
            public readonly bool[] riderDrawn = new bool[MaxRiders], vehicleDrawn = new bool[MaxRiders];
            public Frame() { for (int i = 0; i < riders.Length; i++) riders[i] = new RiderData(); }
            /// The slot's live models as the scenario left them (false when the frame has no row for it).
            public bool TryDrawn(int slot, out bool rider, out bool vehicle)
            {
                for (int i = 0; i < slotCount; i++) if (slots[i] == slot) { rider = riderDrawn[i]; vehicle = vehicleDrawn[i]; return true; }
                rider = vehicle = true; return false;
            }
        }

        public sealed class RiderData
        {
            public uint obj; public int slot; public bool visible; public int bones;   // visible: drawn (+0xa2) and placed by the scenario (+0x70)
            public readonly float[] matrices = new float[Bones * 16];
            public Vector4 mudAmount, mudColor, mudWetness;
            public Vector3 root;   // the entity root node's world translation
            public float drawDistance;   // culled beyond this distance from the camera (the model's draw distance / the renderer divisor; 0: never)
        }

        sealed class Character { public uint obj; public int slot; public ReflexRiderModel model; public bool shown; }

        [Tooltip("The camera the original's draw distance is measured from (the game camera); Camera.main when unset.")]
        public Camera viewer;
        [Tooltip("Multiplies the original draw distance (as OriginalAmbient).")]
        public float distanceScale = 1f;

        [Header("Result (read only)")]
        public int characters; public int drawn; public int scenario; public int step;
        readonly Dictionary<uint, Character> byObject = new Dictionary<uint, Character>();
        ReflexRoster roster; bool builtThisFrame;

        /// The roster of the new world (the slots' rider specs and plates). Characters of a previous world are released.
        public void Begin(ReflexRoster r) { Clear(); roster = r; }

        /// A completed frame (null / invalid: nothing of the podium is drawn).
        public void Apply(Frame f)
        {
            drawn = 0; builtThisFrame = false;
            scenario = f != null && f.valid ? f.scenario : 0; step = f != null && f.valid ? f.step : 0;
            foreach (var c in byObject.Values) c.shown = false;
            var cam = viewer ? viewer : Camera.main;
            Vector3 eye = cam ? cam.transform.position : Vector3.zero;
            if (f != null && f.valid)
                for (int i = 0; i < f.riderCount && i < f.riders.Length; i++)
                {
                    var d = f.riders[i];
                    if (!d.visible || d.bones != Bones) continue;
                    // The renderer's draw distance (Model_SubmitSkinned): beyond it from the camera the character is not submitted. This is what
                    // keeps the characters a step does not place (they stand at the origin) out of the picture, except where the origin
                    // is within reach of the camera: there the original draws them too.
                    if (cam && d.drawDistance > 0f)
                    {
                        float reach = d.drawDistance * distanceScale;
                        if ((d.root - eye).sqrMagnitude > reach * reach) continue;
                    }
                    if (!byObject.ContainsKey(d.obj) && builtThisFrame) continue;   // one rider model built per frame (as OriginalOpponents)
                    var c = Ensure(d);
                    if (c?.model?.rider == null) continue;
                    c.model.root.transform.SetPositionAndRotation(Vector3.zero, Quaternion.identity);   // the bones are world matrices
                    if (!c.model.rider.ApplyBoneMatrices(d.matrices, d.bones)) continue;
                    var mud = new ReflexMudState { slot = d.slot, record = 0, amount = d.mudAmount, wetness = d.mudWetness, color = d.mudColor };
                    ReflexMud.Apply(null, c.model, mud);
                    c.shown = true; drawn++;
                }
            foreach (var c in byObject.Values) if (c.model?.root && c.model.root.activeSelf != c.shown) c.model.root.SetActive(c.shown);
        }

        Character Ensure(RiderData d)
        {
            if (byObject.TryGetValue(d.obj, out var c)) return c;
            builtThisFrame = true;
            c = new Character { obj = d.obj, slot = d.slot };
            byObject[d.obj] = c;
            ReflexRoster.Entry entry = null;
            if (roster != null) foreach (var e in roster.entries) if (e.slot.slot == d.slot) { entry = e; break; }
            try
            {
                c.model = ReflexRiderModel.Build(entry?.rider);
                c.model.root.name = $"Podium rider {d.slot} · {entry?.name ?? "?"} (RiderCharacter {d.obj:x8})";
                c.model.root.transform.SetParent(transform, false);
                ReflexPlates.Apply(null, c.model, entry?.plate);   // the player's own number / name plates (0074ba40)
                c.model.root.SetActive(false);
                characters = byObject.Count;
                Debug.Log($"Reflex podium: rider character {d.obj:x8} for slot {d.slot} ({entry?.name ?? "no roster entry"}) built");
            }
            catch (System.Exception e) { Debug.LogWarning($"Reflex podium: rider character for slot {d.slot} not built: {e.Message}"); c.model = null; }
            return c;
        }

        void Clear()
        {
            foreach (var c in byObject.Values) c.model?.Destroy();
            byObject.Clear(); characters = drawn = 0;
        }

        void OnDestroy() => Clear();
    }
}

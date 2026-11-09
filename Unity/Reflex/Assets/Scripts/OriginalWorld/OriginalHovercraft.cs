using System;
using System.Collections.Generic;
using System.Linq;
using Reflex.Port.Content;
using Reflex.Port.FrontEnd;
using UnityEngine;
using UnityEngine.InputSystem;

namespace Reflex.Port.OriginalWorld
{
    // THE DRIVABLE HOVERCRAFT (port extra, not original game logic; core/plan/QUEUE.md "w-hover"). On the Free Ride map FR_Alpine the
    // parked hovercraft (the level's static HC_Static, the craft the arrival brings the rider in on) can be boarded: ride up to it, stop,
    // press E (pad Y / Triangle). The craft's skirt inflates, the fans spool up and it can be driven over land and water. Press the key again
    // to get back on the bike, where it was left.
    //
    // Nothing of the verified game is touched: the core keeps simulating the rider's bike (it gets no input while the craft is driven and
    // stands where it was left); the craft is Unity's own (HovercraftPhysics: a hover model over the live terrain heightfield and the level's
    // water plane). The rider and the bike are drawn on the craft's deck by moving the rider's and the bike's presentation roots after the
    // game's poses are applied each frame (BeforePoses / AfterPoses / LateUpdate), so the game's animation of them goes on.
    // Data driven (HoverSpec.All): another level or a hovercraft-like prop is another row. OriginalWorldRuntime makes the one component per
    // world (Attach); everything is destroyed with the world (End).
    [DisallowMultipleComponent]
    public sealed class OriginalHovercraft : MonoBehaviour
    {
        public enum Mode { Parked, Boarding, Inflating, Driving, Leaving }

        public static OriginalHovercraft Instance { get; private set; }

        OriginalWorldRuntime rt; HoverSpec spec; ReflexLevelContent content;
        public Mode mode = Mode.Parked;
        public bool Active => spec != null;
        /// The craft is the vehicle the player is in (the bike is parked): the bike's HUD, input and engine sound give way.
        public bool Driving => mode == Mode.Inflating || mode == Mode.Driving;
        public bool InCraft => mode == Mode.Inflating || mode == Mode.Driving || mode == Mode.Leaving;
        /// The pad's Y / the E key mean "get in / out" now (the boarding prompt is up, or the rider is in or getting into the craft): SPAWN MARKERS
        /// (OriginalSpawnMarker, w-markerbtns) leave Y to the craft then.
        public bool OwnsInteract => promptWanted || mode != Mode.Parked;
        public HoverPhysics Physics { get; } = new HoverPhysics();
        public HoverBody Body => body;
        public HoverGround Ground => ground;
        public bool PromptVisible => ui != null && ui.PromptVisible;
        public string PromptText => ui?.PromptText;
        public string GlyphText => ui?.GlyphText;
        public int SprayLive => spray?.Live ?? 0;
        public float Cushion => Physics.cushion;
        public string Report { get; private set; } = "";
        /// Test hooks (TEST ONLY fixtures): controls replacing the player's, a board / leave request.
        [NonSerialized] public HoverControls? forcedControls; [NonSerialized] public bool testBoard, testLeave;

        HoverGround ground; HoverBody body; HoverSpray spray; HoverSound sound; HoverUi ui;
        SceneryInstance parked; Transform parkedTransform; bool parkedHidden;
        bool craftPlaced;       // the craft has been boarded once: it stands where it was left, the level's copy stays hidden
        float modeTime, inflateTime;
        bool live, interactSwallowKey, interactSwallowPad;
        HoverControls controls;
        int throttleDik = 0x11, brakeDik = 0x1F, leftDik = 0x1E, rightDik = 0x20, throttlePad = 20, brakePad = 19, leftPad = 1, rightPad = 1;
        bool bindingsRead; int bindingsSaves = -1;
        readonly byte[] heldKeys = new byte[256];
        readonly ReflexPad[] heldPads = new ReflexPad[4];

        // the camera
        float camYaw; Vector3 camPos; bool camReady;
        float fovNow = 62f;

        // the rider and bike carried onto the deck
        bool carrying; Vector3 rawChassisPos, rawFrontPos, rawRearPos, rawProxyPos; Quaternion rawChassisRot, rawFrontRot, rawRearRot, rawProxyRot; bool haveRaw;
        Matrix4x4 carryMatrix = Matrix4x4.identity;
        readonly List<Transform> carriedRoots = new List<Transform>();

        public static OriginalHovercraft Attach(OriginalWorldRuntime runtime, string level)
        {
            var spec = HoverSpec.For(level);
            var existing = runtime.GetComponent<OriginalHovercraft>();
            bool freeRide = runtime.EventIdentity != null && runtime.EventIdentity.StartsWith("FreeRide/", StringComparison.Ordinal);
            if (spec == null || !freeRide) { if (existing) existing.End(); runtime.hovercraft = null; return null; }
            var h = existing ? existing : runtime.gameObject.AddComponent<OriginalHovercraft>();
            h.Begin(runtime, spec);
            runtime.hovercraft = h;
            return h;
        }

        void Begin(OriginalWorldRuntime runtime, HoverSpec s)
        {
            End();
            rt = runtime; spec = s; Instance = this; mode = Mode.Parked; modeTime = 0; craftPlaced = false; bindingsRead = false;
            var level = FindAnyObjectByType<OriginalLevel>();
            content = level ? level.Content : null;
            parked = content?.instances.FirstOrDefault(i => string.Equals(i.name, spec.parkedEntity, StringComparison.OrdinalIgnoreCase));
            parkedTransform = parked?.transform;
            ground = null;   // built when first needed (the terrain may still be starting)
            Report = parkedTransform ? "ready: " + spec.parkedEntity + " at " + parkedTransform.position : "the level has no " + spec.parkedEntity;
            Debug.Log("[hovercraft] " + Report);
        }

        /// The world is going away (or restarting): everything is put back.
        public void End()
        {
            if (Instance == this) Instance = null;
            if (mode != Mode.Parked || carrying) { RestoreRider(); }
            carrying = false; haveRaw = false;
            if (rt && rt.SceneryModels != null && parked != null) rt.SceneryModels.suppressed.Remove(parked.entityId);
            if (parkedTransform && parkedHidden) parkedTransform.gameObject.SetActive(true);
            parkedHidden = false;
            OriginalVehicleSound.MuteLocal = false; StopBoardSound();
            body?.Destroy(); body = null; spray?.Dispose(); spray = null; sound?.Dispose(); sound = null; ui?.Dispose(); ui = null;
            ground = null; spec = null; mode = Mode.Parked; camReady = false;
        }

        void OnDestroy() { End(); }

        // ---------------------------------------------------------------------------------------------------------------- input

        /// OriginalWorldRuntime.Update, after the keyboard and pads are read (live: the world takes input: no menu, not paused). While the craft
        /// is driven the riding actions are taken here and removed from what the game gets (the bike stays where it is); the interact key
        /// is swallowed while it means "get in / out".
        public void Feed(byte[] keys, ReflexPad[] pads, bool isLive)
        {
            live = isLive;
            if (spec == null) return;
            if (!bindingsRead || bindingsSaves != CoreRemapping.Generation) ReadBindings();   // (w-fb-hud: a binding changed in the ride)
            Array.Copy(keys, heldKeys, 256);
            for (int i = 0; i < pads.Length && i < 4; i++) heldPads[i] = pads[i];
            controls = default;
            if (isLive) controls = ReadControls(keys, pads);
            if (forcedControls.HasValue) controls = forcedControls.Value;
            bool interactArea = isLive && (mode == Mode.Driving || PromptWanted);
            // Y / E mean "interact" while the prompt is up or in the craft; the game must not also act on them (Y is SetResetPoint)
            if (interactArea || interactSwallowKey) keys[0x12] = 0;
            if (interactArea || interactSwallowPad) for (int i = 0; i < pads.Length; i++) pads[i].buttons &= unchecked((ushort)~0x8000);
            if (InCraft || mode == Mode.Boarding)
            {
                // the bike is parked: no riding input reaches it (the pause and menu keys still do)
                for (int i = 0; i < 256; i++) if (i != 0x01) keys[i] = 0;
                for (int i = 0; i < pads.Length; i++) { var p = pads[i]; p.buttons &= 0x30; p.leftTrigger = p.rightTrigger = 0; p.lx = p.ly = p.rx = p.ry = 0; pads[i] = p; }
            }
        }

        void ReadBindings()
        {
            bindingsRead = true; bindingsSaves = CoreRemapping.Generation;
            try
            {
                var t = CoreRemapping.Read();
                int Dik(int id) { for (int d = 0; d < 256; d++) if (t.scan[d] == id) return d; return -1; }
                int a = Dik(t.kb[0]), b = Dik(t.kb[1]), l = Dik(t.kb[2]), r = Dik(t.kb[3]);
                if (a > 0) throttleDik = a; if (b > 0) brakeDik = b; if (l > 0) leftDik = l; if (r > 0) rightDik = r;
                throttlePad = t.js[0]; brakePad = t.js[1]; leftPad = t.js[2]; rightPad = t.js[3];
                Debug.Log($"[hovercraft] controls from the game's remap: throttle dik {throttleDik:x2} / pad {throttlePad}, brake {brakeDik:x2} / {brakePad}, steer {leftDik:x2} {rightDik:x2} / {leftPad} {rightPad}");
            }
            catch (Exception e) { Debug.LogWarning("[hovercraft] the controller table was not read (W S A D / triggers / left stick): " + e.Message); }
        }

        // The value of a pad input id (the game's ControllerRemapping ids: 1..4 the sticks, 9..18 the buttons, 19 / 20 the triggers, 21..24 the d-pad).
        static float PadValue(ReflexPad p, int id, int sign)
        {
            float Axis(short v) { float f = v / 32767f; if (Mathf.Abs(f) < 0.14f) return 0f; return sign >= 0 ? Mathf.Max(0f, f) : Mathf.Max(0f, -f); }
            float Btn(int mask) => (p.buttons & mask) != 0 ? 1f : 0f;
            switch (id)
            {
                case 1: return Axis(p.lx); case 2: return Axis(p.ly); case 3: return Axis(p.rx); case 4: return Axis(p.ry);
                case 9: return Btn(0x1000); case 10: return Btn(0x2000); case 11: return Btn(0x4000); case 12: return Btn(0x8000);
                case 13: return Btn(0x100); case 14: return Btn(0x200); case 15: return Btn(0x10); case 16: return Btn(0x20);
                case 17: return Btn(0x40); case 18: return Btn(0x80);
                case 19: return p.leftTrigger / 255f > 0.06f ? p.leftTrigger / 255f : 0f; case 20: return p.rightTrigger / 255f > 0.06f ? p.rightTrigger / 255f : 0f;
                case 21: return Btn(0x1); case 22: return Btn(0x8); case 23: return Btn(0x2); case 24: return Btn(0x4);
            }
            return 0f;
        }

        HoverControls ReadControls(byte[] keys, ReflexPad[] pads)
        {
            var c = new HoverControls();
            bool K(int d) => d > 0 && d < 256 && keys[d] != 0;
            float throttle = K(throttleDik) || K(0xC8) ? 1f : 0f, brake = K(brakeDik) || K(0xD0) ? 1f : 0f;
            float left = K(leftDik) || K(0xCB) ? 1f : 0f, right = K(rightDik) || K(0xCD) ? 1f : 0f;
            for (int i = 0; i < pads.Length && i < 4; i++)
            {
                var p = pads[i];
                throttle = Mathf.Max(throttle, PadValue(p, throttlePad, +1)); brake = Mathf.Max(brake, PadValue(p, brakePad, throttlePad == 2 && brakePad == 2 ? -1 : +1));
                left = Mathf.Max(left, PadValue(p, leftPad, -1)); right = Mathf.Max(right, PadValue(p, rightPad, +1));
            }
            c.throttle = throttle; c.brake = brake; c.steer = Mathf.Clamp(right - left, -1f, 1f);
            return c;
        }

        bool InteractPressed()
        {
            if (testBoard && mode == Mode.Parked) { testBoard = false; return true; }
            if (testLeave && mode == Mode.Driving) { testLeave = false; return true; }
            var kb = Keyboard.current;
            bool key = kb != null && kb.eKey.wasPressedThisFrame && !OriginalOnboardAdjust.KeyboardCaptured;
            bool pad = false;
            if (rt != null) for (int i = 0; i < 4; i++) { var g = rt.PadRouting.Pad(i); if (g != null && g.buttonNorth.wasPressedThisFrame) pad = true; }
            if (key) interactSwallowKey = true; if (pad) interactSwallowPad = true;
            return key || pad;
        }

        // ---------------------------------------------------------------------------------------------------------------- state

        bool promptWanted; string promptSub;
        bool PromptWanted => promptWanted;

        bool EnsureGround()
        {
            if (ground != null && ground.Ready) return true;
            if (rt == null || !rt.terrain || !rt.terrain.Built) return false;
            ground = new HoverGround(rt.terrain, content);
            return ground.Ready;
        }

        // The craft the bike is beside: the drivable craft where it was left, or the level's parked copy at the dock (boarding that one
        // brings the drivable craft there). Which one is near is decided by the bike's place in each craft's frame.
        bool DockVisible => parkedTransform && !parkedHidden && parkedTransform.gameObject.activeInHierarchy && (rt.podium == null || rt.podium.scenario == 0);

        bool NearPose(Vector3 bike, Vector3 p, Quaternion r)
        {
            var local = Quaternion.Inverse(r) * (bike - p);
            return Mathf.Abs(local.x) < spec.halfWidth + 26f && local.z > -(spec.halfLength + 34f) && local.z < spec.halfLength + 38f && Mathf.Abs(local.y) < 45f;
        }

        // near: the bike's place; atDock: it is the level's copy that is near (not the drivable craft)
        bool BikeNear(out float bikeSpeed, out bool atDock)
        {
            bikeSpeed = 0f; atDock = false;
            if (rt == null || !rt.chassis || !rt.TryGetBike(out var bikePos, out var bikeVel)) return false;
            bikeSpeed = bikeVel.magnitude;
            if (craftPlaced && NearPose(bikePos, Physics.position, Physics.Rotation)) return true;
            if (DockVisible && NearPose(bikePos, parkedTransform.position, parkedTransform.rotation)) { atDock = true; return true; }
            return false;
        }

        void Update()
        {
            if (spec == null || rt == null) return;
            float dt = Mathf.Min(Time.unscaledDeltaTime, 0.05f);
            if (!rt.Running) { ui?.Tick(dt, false, null, false, 0, 0); return; }
            ui ??= new HoverUi(transform);
            bool run = live && !rt.paused;
            if (!run) interactSwallowKey = interactSwallowPad = (Keyboard.current?.eKey.isPressed ?? false) || AnyNorthHeld();
            else
            {
                // swallow until released
                if (interactSwallowKey && !(Keyboard.current?.eKey.isPressed ?? false)) interactSwallowKey = false;
                if (interactSwallowPad && !AnyNorthHeld()) interactSwallowPad = false;
            }
            if (boardSoundUntil > 0 && Time.unscaledTime > boardSoundUntil) StopBoardSound();
            float fadeTarget = mode == Mode.Boarding || mode == Mode.Leaving ? 1f : 0f;
            modeTime += dt;
            // is boarding possible: the craft is there, the arrival is over, the bike is near and nearly stopped
            promptWanted = false; promptSub = null;
            bool atDock = false;
            if (mode == Mode.Parked && run && !Challenge())
            {
                bool near = BikeNear(out float bikeSpeed, out atDock);
                if (near)
                {
                    promptWanted = true;
                    if (bikeSpeed > 28f) promptSub = "STOP THE BIKE FIRST";
                    if (InteractPressed())
                    {
                        if (bikeSpeed > 28f) ui.Toast("STOP THE BIKE FIRST");
                        else if (EnsureGround() && BuildCraft()) { boardAtDock = atDock; mode = Mode.Boarding; modeTime = 0; }
                    }
                }
            }
            switch (mode)
            {
                case Mode.Boarding:
                    if (FadeDone) Board();
                    break;
                case Mode.Inflating:
                    inflateTime += run ? dt : 0f;
                    if (inflateTime >= 2.35f && Physics.cushion > 0.9f) { mode = Mode.Driving; modeTime = 0; }
                    goto case Mode.Driving;
                case Mode.Driving:
                    if (run && mode == Mode.Driving && InteractPressed()) { mode = Mode.Leaving; modeTime = 0; Debug.Log("[hovercraft] leaving"); }
                    if (Challenge()) { mode = Mode.Leaving; modeTime = 0; }
                    break;
                case Mode.Leaving:
                    if (FadeDone) Unboard();
                    break;
            }
            if (mode == Mode.Inflating || mode == Mode.Driving || mode == Mode.Leaving || (mode == Mode.Parked && craftPlaced && SettlingAfterUse()))
                Simulate(run ? dt : 0f);
            UpdateVisuals(dt, run);
            ui.Tick(dt, promptWanted && run, promptSub, mode == Mode.Driving || mode == Mode.Inflating, Physics.Speed * 0.6818f, fadeTarget);
        }

        // the screen has faded to black (without the UI document, after the same time)
        bool FadeDone => ui != null && ui.Ready ? ui.Fade >= 0.999f : modeTime > 0.4f;

        bool AnyNorthHeld()
        {
            if (rt == null) return false;
            for (int i = 0; i < 4; i++) { var g = rt.PadRouting.Pad(i); if (g != null && g.buttonNorth.isPressed) return true; }
            return false;
        }

        bool boardAtDock;

        bool Challenge() => rt.HasChallenge && rt.Challenge.valid != 0 && rt.Challenge.state >= 0 && rt.Challenge.state < 5;

        bool SettlingAfterUse() => Physics.cushion > 0.002f || Physics.Speed > 0.4f;

        // The craft exists from the first boarding on (its model is read from the level then).
        bool BuildCraft()
        {
            if (body != null) return true;
            var sceneryShader = rt.ambient ? rt.ambient.sceneryShader : null;
            body = HoverBody.Build(content, spec, sceneryShader, transform);
            if (body == null) { ui.Toast("THE HOVERCRAFT COULD NOT BE LOADED"); return false; }
            body.SetVisible(false);
            spray = new HoverSpray(rt.particles ? rt.particles.particleShader : null);
            sound ??= new HoverSound(transform);
            Debug.Log("[hovercraft] model: " + body.Report);
            return true;
        }

        // The swap, under the fade: the craft takes the level's copy's place, flat; the rider and the bike go onto its deck.
        void Board()
        {
            if (body == null) { mode = Mode.Parked; return; }
            if (boardAtDock && parkedTransform || !craftPlaced)
            {
                // the level's copy at the dock: the drivable craft takes its place, flat (the skirt's bottom on the ground)
                var pos = parkedTransform.position; var rot = parkedTransform.rotation;
                float yaw0 = Mathf.Atan2((rot * Vector3.forward).x, (rot * Vector3.forward).z);
                Physics.Place(pos, yaw0, 0f);
                Physics.up = rot * Vector3.up;
                Physics.position.y = ground.Height(pos.x, pos.z, out _) - Physics.deflatedSink;
                if (!parkedHidden) { parkedHidden = true; rt.SceneryModels.suppressed.Add(parked.entityId); parkedTransform.gameObject.SetActive(false); }
            }
            else Physics.up = Physics.Rotation * Vector3.up;
            float yaw = Physics.yaw;
            craftPlaced = true;
            body.SetVisible(true);
            spray?.Clear();
            StartCarrying();
            OriginalVehicleSound.MuteLocal = true;
            // the camera starts behind the craft (the fade covers the cut) and draws back to its place
            camReady = false; camYaw = yaw;
            inflateTime = 0f; mode = Mode.Inflating; modeTime = 0;
            PlayBoardSound();
            Debug.Log($"[hovercraft] boarded at {Physics.position} heading {yaw * Mathf.Rad2Deg:F0} deg ({(boardAtDock ? "the level's copy at the dock" : "the craft where it was left")}, on {(Physics.onWater ? "water" : "land")})");
        }

        void Unboard()
        {
            RestoreRider();
            OriginalVehicleSound.MuteLocal = false;
            StopBoardSound();
            // The level's copy comes back at the dock (the bike stands on its deck). A craft left within reach of the dock is parked there:
            // it is the level's copy again (the drivable one is put away); farther away it stays where it was left, deflating.
            bool docked = false;
            if (parkedTransform && parkedHidden)
            {
                parkedHidden = false; rt.SceneryModels.suppressed.Remove(parked.entityId); parkedTransform.gameObject.SetActive(true);
                var d = Physics.position - parkedTransform.position; d.y = 0;
                if (d.sqrMagnitude < DockRadius * DockRadius) { docked = true; craftPlaced = false; body?.SetVisible(false); spray?.Clear(); Physics.velocity = Vector3.zero; Physics.cushion = 0f; }
            }
            mode = Mode.Parked; modeTime = 0; camReady = false;
            if (rt.cameraMode == 1 && rt.onboard) rt.onboard.Resnap();   // the first-person view starts again on the bike
            Debug.Log($"[hovercraft] left the craft at {Physics.position}: the rider is back on the bike; " + (docked ? "the craft is parked at the dock (the level's copy)" : "the craft stays here" + (parkedTransform ? ", the level's copy is back at the dock" : "")));
        }
        const float DockRadius = 90f;

        // the original arrival's hovercraft engine sound (the scenario event "VEHICLE" of the map) as the fans start; the craft's own loops carry on
        uint boardWrapper; float boardSoundUntil;
        void PlayBoardSound()
        {
            var audio = rt ? rt.gameAudio : null;
            if (audio == null || string.IsNullOrEmpty(spec.boardSound)) return;
            boardWrapper = OriginalAudio.HostWrapperBase + 0x7000u + (boardWrapper & 0xff) + 1;
            audio.Host(0, boardWrapper, 1, spec.boardSound, "", 0f, Physics.position, false);
            boardSoundUntil = Time.unscaledTime + 4.5f;
        }
        void StopBoardSound()
        {
            var audio = rt ? rt.gameAudio : null;
            if (audio != null && boardWrapper != 0) audio.Host(1, boardWrapper, 1, spec.boardSound, "", 0f, Vector3.zero, false);
            boardWrapper = 0; boardSoundUntil = 0;
        }

        // ---------------------------------------------------------------------------------------------------------------- physics

        void Simulate(float dt)
        {
            if (body == null || ground == null) return;
            bool inCraft = mode == Mode.Inflating || mode == Mode.Driving;
            // the cushion: inflating from flat takes 2.3 s (after the fans spool a little), with an overshoot; leaving deflates it in 2 s
            if (mode == Mode.Inflating)
            {
                float x = Mathf.Clamp01((inflateTime - 0.45f) / 1.85f);
                float c1 = 1.9f, c3 = c1 + 1f;                                   // ease out back: balloons to ~1.1 and settles
                float eased = 1f + c3 * Mathf.Pow(x - 1f, 3f) + c1 * Mathf.Pow(x - 1f, 2f);
                Physics.cushion = Mathf.Max(Physics.cushion, x <= 0f ? 0f : eased);
                if (x >= 1f) Physics.cushion = 1f + (Physics.cushion - 1f) * Mathf.Exp(-6f * dt);
            }
            else if (mode == Mode.Driving) Physics.cushion = 1f;
            else Physics.cushion = Mathf.MoveTowards(Physics.cushion, 0f, dt / 2.2f);
            var c = inCraft && (mode == Mode.Driving || Physics.cushion > 0.9f) ? controls : default;
            if (mode == Mode.Inflating && inflateTime < 2.0f) c = default;
            Physics.Step(dt, c, ground, Time.time);
            if (Physics.impact > 5f) { sound?.Thump(Physics.impact, Gain()); Physics.impact = 0f; } else Physics.impact = 0f;
        }

        float Gain()
        {
            var audio = rt ? rt.gameAudio : null;
            return audio ? audio.ChannelGain(ReflexChannel.Engine) * 1f : 1f;
        }

        void UpdateVisuals(float dt, bool run)
        {
            if (body == null || !body.root) { sound?.Tick(dt, false, 0, 0, 0, 0, Gain(), !run); return; }
            bool shown = craftPlaced;
            if (shown)
            {
                var t = body.root.transform;
                t.SetPositionAndRotation(Physics.position, Physics.Rotation);
                float speed01 = Mathf.Clamp01(Physics.Speed / Physics.maxSpeed);
                bool inCraft = mode == Mode.Inflating || mode == Mode.Driving;
                float spoolTarget = inCraft ? Mathf.Clamp01(inflateTime / 1.6f) : Mathf.Clamp01(Physics.cushion);
                float thr = inCraft ? controls.throttle : 0f;
                body.SetCushion(Physics.cushion, inCraft ? Mathf.Clamp01(Physics.cushion) * (0.25f + speed01) : 0f, Time.time);
                float fan = inCraft ? sound.Spool * (14f + 90f * thr + 30f * speed01) : sound.Spool * 30f;
                if (run) body.Animate(dt, fan, inCraft ? controls.steer : 0f, inCraft && sound.Spool > 0.5f);
                float inflating = mode == Mode.Inflating ? Mathf.Clamp01(1.25f - Mathf.Abs(inflateTime - 1.2f)) : 0f;
                var fanL = new Vector3(-11.8f, 14.9f, -24f); var fanR = new Vector3(11.8f, 14.9f, -24f);
                if (run) spray?.Update(dt, Physics.position, Physics.Rotation, Physics.cushion, Physics.Speed, thr, Physics.onWater, Physics.groundHeight, Physics.airborne, inflating, spec.halfWidth + 4f, spec.halfLength + 6f, fanL, fanR);
                spray?.Draw(rt ? rt.chaseCamera : null, gameObject.layer);
                sound.Tick(dt, inCraft || Physics.cushion > 0.02f, spoolTarget, thr, speed01, Physics.cushion, Gain(), !run);
            }
            else sound?.Tick(dt, false, 0, 0, 0, 0, Gain(), !run);
        }

        // ---------------------------------------------------------------------------------------------------------------- the rider and the bike on the deck

        // OriginalWorldRuntime.ApplyView: the game's poses are about to be applied (the rider's presentation root back to the origin: its bones
        // are set in world space against it), and have just been applied (the bike's raw pose is read).
        public void BeforePoses()
        {
            if (!carrying || !rt || !rt.rider) return;
            rt.rider.transform.SetPositionAndRotation(Vector3.zero, Quaternion.identity);
        }

        public void AfterPoses()
        {
            if (!carrying || rt == null || !rt.chassis) return;
            Capture();
        }

        void Capture()
        {
            rawChassisPos = rt.chassis.position; rawChassisRot = rt.chassis.rotation;
            if (rt.frontWheel) { rawFrontPos = rt.frontWheel.position; rawFrontRot = rt.frontWheel.rotation; }
            if (rt.rearWheel) { rawRearPos = rt.rearWheel.position; rawRearRot = rt.rearWheel.rotation; }
            if (rt.riderProxy) { rawProxyPos = rt.riderProxy.position; rawProxyRot = rt.riderProxy.rotation; }
            haveRaw = true;
        }

        void StartCarrying()
        {
            if (rt == null || !rt.chassis) return;
            Capture(); carrying = true;
        }

        // The rigid transform that takes the bike where it stands (on its wheels' ground, heading as it faces) onto the craft's seat.
        Matrix4x4 CarryMatrix()
        {
            var mid = (rawFrontPos + rawRearPos) * 0.5f;
            var ahead = rawFrontPos - rawRearPos; ahead.y = 0;
            float yaw0 = ahead.sqrMagnitude > 0.01f ? Mathf.Atan2(ahead.x, ahead.z) : Mathf.Atan2((rawChassisRot * Vector3.forward).x, (rawChassisRot * Vector3.forward).z);
            var g0 = new Vector3(mid.x, Mathf.Min(rawFrontPos.y, rawRearPos.y) - 1.08f, mid.z);
            var f0 = Matrix4x4.TRS(g0, Quaternion.Euler(0, yaw0 * Mathf.Rad2Deg, 0), Vector3.one);
            var f1 = Matrix4x4.TRS(body.root.transform.position, body.root.transform.rotation, Vector3.one) * Matrix4x4.Translate(spec.seat);
            return f1 * f0.inverse;
        }

        void LateUpdate()
        {
            if (!carrying || !haveRaw || body == null || !body.root || rt == null || !rt.chassis) return;
            var m = CarryMatrix(); var q = m.rotation;
            carryMatrix = m;
            void Set(Transform t, Vector3 p, Quaternion r) { if (t) t.SetPositionAndRotation(m.MultiplyPoint3x4(p), q * r); }
            Set(rt.chassis, rawChassisPos, rawChassisRot);
            Set(rt.frontWheel, rawFrontPos, rawFrontRot);
            Set(rt.rearWheel, rawRearPos, rawRearRot);
            Set(rt.riderProxy, rawProxyPos, rawProxyRot);
            if (rt.rider) rt.rider.transform.SetPositionAndRotation(m.MultiplyPoint3x4(Vector3.zero), q);
        }

        void RestoreRider()
        {
            if (carrying && haveRaw && rt != null && rt.chassis)
            {
                rt.chassis.SetPositionAndRotation(rawChassisPos, rawChassisRot);
                if (rt.frontWheel) rt.frontWheel.SetPositionAndRotation(rawFrontPos, rawFrontRot);
                if (rt.rearWheel) rt.rearWheel.SetPositionAndRotation(rawRearPos, rawRearRot);
                if (rt.riderProxy) rt.riderProxy.SetPositionAndRotation(rawProxyPos, rawProxyRot);
                if (rt.rider) rt.rider.transform.SetPositionAndRotation(Vector3.zero, Quaternion.identity);
            }
            carrying = false;
        }

        /// The rider's world position now (the bike on the craft, or the bike), for tests.
        public Vector3 CarriedBikePosition => rt && rt.chassis ? rt.chassis.position : Vector3.zero;

        // ---------------------------------------------------------------------------------------------------------------- camera

        /// OriginalWorldRuntime.MoveCamera: the chase camera behind the craft while it is driven (true: the camera is set).
        public bool ApplyCamera(Camera cam)
        {
            if (!cam || spec == null || !craftPlaced || !(mode == Mode.Inflating || mode == Mode.Driving || mode == Mode.Leaving)) return false;
            float dt = Mathf.Min(Time.unscaledDeltaTime, 0.05f);
            var pos = Physics.position; var vel = new Vector3(Physics.velocity.x, 0, Physics.velocity.z); float sp = vel.magnitude;
            float heading = Physics.yaw;
            float velYaw = sp > 25f ? Mathf.Atan2(vel.x, vel.z) : heading;
            float want = Mathf.LerpAngle(heading * Mathf.Rad2Deg, velYaw * Mathf.Rad2Deg, Mathf.Clamp01((sp - 25f) / 60f) * 0.45f) * Mathf.Deg2Rad;
            if (!camReady) camYaw = heading;
            camYaw = Mathf.LerpAngle(camYaw * Mathf.Rad2Deg, want * Mathf.Rad2Deg, camReady ? 1f - Mathf.Exp(-3.2f * dt) : 1f) * Mathf.Deg2Rad;
            float early = mode == Mode.Inflating ? Mathf.Clamp01(1f - inflateTime / 2.4f) : 0f;
            float swing = early * early * 62f * Mathf.Deg2Rad;           // the camera starts at the craft's side and comes round behind while the tubes fill
            var fwd = new Vector3(Mathf.Sin(camYaw + swing), 0, Mathf.Cos(camYaw + swing));
            float speed01 = Mathf.Clamp01(sp / Physics.maxSpeed);
            float dist = Mathf.Lerp(92f, 118f, speed01) - 18f * early, height = Mathf.Lerp(36f, 46f, speed01) - 10f * early;
            var anchor = pos + Vector3.up * 12f;
            var desired = anchor - fwd * dist + Vector3.up * height;
            if (ground != null && ground.Ready) { float g = ground.Height(desired.x, desired.z, out _); if (desired.y < g + 8f) desired.y = g + 8f; }
            camPos = camReady ? Vector3.Lerp(camPos, desired, 1f - Mathf.Exp(-7f * dt)) : desired;
            camReady = true;
            var look = anchor + new Vector3(Mathf.Sin(camYaw), 0, Mathf.Cos(camYaw)) * 22f * (1f - early) + Vector3.up * 2f;
            var rot = Quaternion.LookRotation(look - camPos, Vector3.up);
            float fov = 60f + 12f * speed01;
            fovNow = camReady ? Mathf.Lerp(fovNow, fov, 1f - Mathf.Exp(-4f * dt)) : fov;
            cam.transform.SetPositionAndRotation(camPos, rot);
            cam.fieldOfView = fovNow;
            return true;
        }

        // ---------------------------------------------------------------------------------------------------------------- diagnostics

        public string Describe() => $"mode {mode} craft {(craftPlaced ? Physics.position.ToString("F1") : "-")} speed {Physics.Speed * 0.6818f:F0} mph cushion {Physics.cushion:F2} {(Physics.onWater ? "water" : "land")}{(Physics.airborne ? " air" : "")}" +
            $" spray {spray?.Live ?? 0} fan {sound?.Spool ?? 0:F2}";
    }
}

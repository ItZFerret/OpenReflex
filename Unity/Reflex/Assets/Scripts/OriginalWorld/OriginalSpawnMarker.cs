// SPAWN MARKERS (OPTIONS > OPEN REFLEX > SPAWN MARKERS, on by default; w-spawnmarker), after Open Unleashed's SPAWN MARKER (RideGame.Spawn.cs):
// two controls of our own (w-markerbtns): SET SPAWN MARKER held for HoldSeconds keeps the rider's place (position and heading) as the marker;
// RIDE TO SPAWN MARKER, pressed, rides back there at once. The respawn is the GAME'S OWN reset at the marker, without its wait: core
// rx_session_spawn_at_instant runs the reset key's steps (Player_ResetTo's spot, the fade hold, then Player_Reset: upright, velocity gone, the
// wreck and trick state cleared, the game's ghost time) at once instead of after the 0.5 s white fade (PlayerReset\FadeOutTime), so the physics and
// the rider's state are exactly those of a normal reset (core/tools/spawn_marker_instant_test.c); only the spot is the marker's. The RESET key and
// the track resets keep the original's fade and timing.
//   keys    defaults: the pad's Y (hold) and B (press), the keyboard's F (hold) and V (press): the inputs of the game's own SetResetPoint /
//           UseResetPoint (ControllerRemapping; ControllerPresets.xml: CustomResetPlaceMarker on Y, CustomResetGoToMarker on B), i.e. the original's
//           Free Ride "save spot" (a flag where you are, Player_HandleResetInput in activity mode 8) and "back to spot" (a reset to it; elsewhere B is
//           just another reset key, as X). Where markers work (below) the marker REPLACES that system: its inputs are taken out of what the game gets
//           (FilterGameInput), so the flag is never placed and B never runs the game's reset there. Where markers do not work (races, the career
//           ...) the controls are silent and Y / B / F / V are the game's, as in the original. The D-pad down is the game's look back
//           (UserCamQuickBack) again: no marker input may be on the D-pad, START or the stick clicks (SpawnMarkerInput.PadRefusal), nor on a key the
//           client or the game uses while riding (KeyRefusal: M music, H HUD, P photo, E hovercraft, Q / E / 3 quick looks, the user camera keys).
//           Remappable on the CONTROLS page (ReflexSettings.SpawnMarkerSetKey / SetPad / RideKey / RidePad); the same input on both rows is one
//           control (held = set, tapped = ride back). An input another column of the game's table binds stays the game's (that control is off).
//           The hovercraft's boarding prompt / the craft keep Y (OriginalHovercraft.OwnsInteract).
//   where   Free Ride (all tracks, the Alive levels) and PRACTICE (w-lb-markers): never a race, a Free Ride challenge, MotoSkills or the test
//           ride. (Open Unleashed also allows it in races; here a race's rules, records and rewards stay the original's.)
//           The core checks it too (rx_session_spawn_at_instant refuses outside a Free Ride activity or PRACTICE).
//   practice the MX Bikes rule (core game/practice.h): riding back invalidates the lap in progress (MARKER in the lap list) and the timing restarts from the finish line: the time to the next crossing is an out-lap, the crossing starts a
//           fresh flying lap. Keeping a marker changes nothing. Any time while racing (the RESET key keeps the race's rule).
//   saved   per level and map scale, in the client's save: "OpenReflex.Spawn.<event>[@<scale>]" = "x,y,z,dx,dz" (<event> = "FreeRide/<level>").
//   notice  a HUD toast in the modern HUD's look (both HUD styles): SPAWN MARKER SET, NO SPAWN MARKER ..., HOLD Y TO SET ...
using System;
using System.Globalization;
using Reflex.Port.FrontEnd;
using Reflex.Port.Hud;
using Reflex.Port.Options;
using Reflex.Port.RxCore;
using UnityEngine;
using UnityEngine.InputSystem;
using UnityEngine.UI;

namespace Reflex.Port.OriginalWorld
{
    /// The marker controls' inputs: a pad input id (the game's ControllerRemapping ids 9 .. 24) and the names the CONTROLS page shows.
    public static class SpawnMarkerInput
    {
        public const int PadFirst = 9, PadLast = 24;
        static readonly string[] PadNames = { "A", "B", "X", "Y", "LB", "RB", "START", "BACK", "LEFT STICK CLICK", "RIGHT STICK CLICK", "LT", "RT", "D-PAD UP", "D-PAD RIGHT", "D-PAD DOWN", "D-PAD LEFT" };
        public static bool ValidPad(int id) => id >= PadFirst && id <= PadLast;
        public static string PadName(int id) => ValidPad(id) ? PadNames[id - PadFirst] : "";
        public static bool PadDown(Gamepad g, int id)
        {
            if (g == null) return false;
            switch (id)
            {
                case 9: return g.buttonSouth.isPressed; case 10: return g.buttonEast.isPressed; case 11: return g.buttonWest.isPressed; case 12: return g.buttonNorth.isPressed;
                case 13: return g.leftShoulder.isPressed; case 14: return g.rightShoulder.isPressed; case 15: return g.startButton.isPressed; case 16: return g.selectButton.isPressed;
                case 17: return g.leftStickButton.isPressed; case 18: return g.rightStickButton.isPressed;
                case 19: return g.leftTrigger.ReadValue() > 0.5f; case 20: return g.rightTrigger.ReadValue() > 0.5f;
                case 21: return g.dpad.up.isPressed; case 22: return g.dpad.right.isPressed; case 23: return g.dpad.down.isPressed; case 24: return g.dpad.left.isPressed;
            }
            return false;
        }
        /// Takes a pad input out of what the game gets this frame (ReflexPad: XInput's button bits; the triggers 0).
        public static void ClearPad(ref ReflexPad p, int id)
        {
            switch (id)
            {
                case 19: p.leftTrigger = 0; return;
                case 20: p.rightTrigger = 0; return;
            }
            ushort m = PadMask(id);
            if (m != 0) p.buttons = (ushort)(p.buttons & ~m);
        }
        static ushort PadMask(int id)
        {
            switch (id)
            {
                case 9: return 0x1000; case 10: return 0x2000; case 11: return 0x4000; case 12: return 0x8000;
                case 13: return 0x100; case 14: return 0x200; case 15: return 0x10; case 16: return 0x20; case 17: return 0x40; case 18: return 0x80;
                case 21: return 0x1; case 22: return 0x8; case 23: return 0x2; case 24: return 0x4;
            }
            return 0;
        }
        /// Why a marker control cannot take this pad input (null: it can). The game hard-wires these while riding (Controller_BuildKeyMaps' fixed
        /// slots, 00547690): START the pause; the D-pad the camera (down UserCamQuickBack = look back, left / right the quick looks, up / down the
        /// user camera's dolly); the stick clicks the stick buttons (and the onboard camera's look back).
        public static string PadRefusal(int id)
        {
            if (!ValidPad(id)) return "SPAWN MARKER takes a button or a trigger, not a stick.";
            switch (id)
            {
                case 15: return "START is the pause.";
                case 17: case 18: return "The stick clicks are the game's (the stick buttons, the onboard camera's look back).";
                case 21: case 22: case 23: case 24: return "The D-pad is the game's camera while riding (down: look back; left / right: quick looks).";
            }
            return null;
        }
        /// The Unity key of a DirectInput scan code (Key.None: unbound / unknown).
        public static Key KeyOf(int dik)
        {
            foreach (var kv in DirectInputMap.Table) if (kv.Value == dik) return kv.Key;
            return Key.None;
        }
        public static string KeyName(int dik) { var k = KeyOf(dik); return k == Key.None ? "" : k.ToString().ToUpperInvariant(); }
        /// Keys the port keeps for itself (OriginalWorldRuntime's client keys, Esc): not offered to the marker.
        public static bool ReservedKey(int dik) => dik == 0x01 || dik >= 0x3F && dik <= 0x44 || dik == 0x57 || dik == 0x58 || dik == 0x38 || dik == 0xB8 || dik == 0xD2 || dik == 0xD3;
        /// Why a marker control cannot take this key (null: it can): the client's keys and the keys the game hard-wires while riding (the quick
        /// looks Q / E / 3 and the user camera's Home / End / Page Up / Page Down, Controller_BuildKeyMaps' fixed slots).
        public static string KeyRefusal(int dik)
        {
            if (ReservedKey(dik)) return "The client keeps that key for itself (the function keys, Insert, Delete and Alt).";
            switch (dik)
            {
                case 0x32: return "M is the music on / off.";
                case 0x23: return "H hides the HUD.";
                case 0x19: return "P opens the photo mode.";
                case 0x12: return "E is the hovercraft's key and the game's quick look right.";
                case 0x10: return "Q is the game's quick look left.";
                case 0x04: return "3 is the game's look back.";
                case 0xC7: case 0xCF: case 0xC9: case 0xD1: return "Home, End, Page Up and Page Down move the game's user camera.";
            }
            return null;
        }
        /// The game's remap columns the marker replaces where it works (its own Free Ride save spot): SetResetPoint and UseResetPoint (11 / 13).
        public static bool Replaces(RemapTable t, int column)
        {
            if (t != null && t.columns != null && column < t.columns.Length && !string.IsNullOrEmpty(t.columns[column]))
                return t.columns[column] == "SetResetPoint" || t.columns[column] == "UseResetPoint";
            return column == 11 || column == 13;
        }
        /// The column of the game's table (other than the two the marker replaces) that binds this input in a row (kb: game key ids, js: pad ids); -1 none.
        public static int GameColumn(RemapTable t, int[] row, int id)
        {
            if (t == null || row == null || id == 0) return -1;
            for (int i = 0; i < row.Length; i++) if (row[i] == id && !Replaces(t, i)) return i;
            return -1;
        }
    }

    /// The rider's spawn marker in a Free Ride: input, the saved marker, the notices. One per process (a hidden persistent object that follows
    /// ReflexClientLive.Runtime), so it needs no hook in the ride's own scripts.
    [DefaultExecutionOrder(50)]
    public sealed class OriginalSpawnMarker : MonoBehaviour
    {
        /// Seconds SET SPAWN MARKER is held to set the marker (Open Unleashed's SpawnHoldSeconds).
        public const float HoldSeconds = 0.6f;
        public static OriginalSpawnMarker Instance { get; private set; }

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        static void Boot()
        {
            if (Instance) return;
            var go = new GameObject("SpawnMarker") { hideFlags = HideFlags.HideAndDontSave };
            DontDestroyOnLoad(go);
            Instance = go.AddComponent<OriginalSpawnMarker>();
        }

        OriginalWorldRuntime world; long worldGeneration = -1;
        string loadedKey; bool hasMarker; Vector3 markerPos; Vector2 markerDir;
        readonly SpawnMarkerButtons keys = new SpawnMarkerButtons(), pad = new SpawnMarkerButtons();   // each device's two controls
        bool lastWasPad;
        volatile int pendingResult = int.MinValue;
        SpawnMarkerNotice notice;

        void OnDestroy() { if (Instance == this) Instance = null; }

        // ------------------------------------------------------------------------------------------------ the marker's save
        /// "OpenReflex.Spawn.FreeRide/FR_Alpine" + "@1.35" at another map scale. null: no Free Ride world.
        public static string KeyFor(string eventIdentity, float mapScale)
        {
            if (string.IsNullOrEmpty(eventIdentity)) return null;
            int h = eventIdentity.IndexOf('#'); string e = h >= 0 ? eventIdentity.Substring(0, h) : eventIdentity;
            string k = "OpenReflex.Spawn." + e;
            if (Mathf.Abs(mapScale - 1f) > 1e-3f) k += "@" + mapScale.ToString("0.00", CultureInfo.InvariantCulture);
            return k;
        }
        public static string Encode(Vector3 pos, Vector2 dir) =>
            string.Join(",", pos.x.ToString("R", CultureInfo.InvariantCulture), pos.y.ToString("R", CultureInfo.InvariantCulture), pos.z.ToString("R", CultureInfo.InvariantCulture),
                        dir.x.ToString("R", CultureInfo.InvariantCulture), dir.y.ToString("R", CultureInfo.InvariantCulture));
        public static bool TryDecode(string v, out Vector3 pos, out Vector2 dir)
        {
            pos = default; dir = default;
            if (string.IsNullOrEmpty(v)) return false;
            var p = v.Split(',');
            if (p.Length != 5) return false;
            var f = new float[5];
            for (int i = 0; i < 5; i++) if (!float.TryParse(p[i], NumberStyles.Float, CultureInfo.InvariantCulture, out f[i]) || float.IsNaN(f[i]) || float.IsInfinity(f[i])) return false;
            pos = new Vector3(f[0], f[1], f[2]); dir = new Vector2(f[3], f[4]);
            return dir.sqrMagnitude > 1e-6f;
        }
        string CurrentKey() => KeyFor(world ? world.EventIdentity : null, RxMapScale.Current);
        void LoadMarker()
        {
            string key = CurrentKey();
            if (key == loadedKey) return;
            loadedKey = key; hasMarker = false;
            if (key != null && TryDecode(ReflexSave.GetString(key, ""), out markerPos, out markerDir)) hasMarker = true;
        }
        /// The marker kept for this level and scale (tests).
        public bool TryGetMarker(out Vector3 pos, out Vector2 dir) { LoadMarker(); pos = markerPos; dir = markerDir; return hasMarker; }

        // ------------------------------------------------------------------------------------------------ availability
        /// Why spawn markers are off in this ride ("" = they work here).
        public static string Unavailable(OriginalWorldRuntime rt)
        {
            if (!rt || !rt.Running) return "NOT NOW";
            string id = rt.EventIdentity;
            bool freeRide = id != null && id.StartsWith("FreeRide/", StringComparison.Ordinal) && !rt.IsChallengeWorld && !rt.testRide && !rt.tutorial && !rt.OwnedCareerActive;
            {
                bool practice = rt.IsPractice && id != null && !rt.OwnedCareerActive;   // PRACTICE (w-lb-markers)
                if (!practice && !freeRide) return OnlyHere;
            }
            if (rt.hovercraft && rt.hovercraft.Driving) return "NOT ON THE HOVERCRAFT";
            return "";
        }
        const string OnlyHere = "SPAWN MARKERS ARE FOR FREE RIDE AND PRACTICE ONLY";

        /// "F  /  Y" (the keyboard's input, the pad's) for SET SPAWN MARKER; RideControls() for RIDE TO SPAWN MARKER (the CONTROLS page, the pause page's hint in PRACTICE).
        public static string SetControls() => Both(SpawnMarkerInput.KeyName(ReflexSettings.SpawnMarkerSetKey), SpawnMarkerInput.PadName(ReflexSettings.SpawnMarkerSetPad));
        public static string RideControls() => Both(SpawnMarkerInput.KeyName(ReflexSettings.SpawnMarkerRideKey), SpawnMarkerInput.PadName(ReflexSettings.SpawnMarkerRidePad));
        static string Both(string k, string p) => string.IsNullOrEmpty(k) ? p : string.IsNullOrEmpty(p) ? k : k + " / " + p;
        static bool Blocked(OriginalWorldRuntime rt) =>
            rt.paused || rt.OwnMenuActive || rt.IntroCardShown || rt.optionsMenu && rt.optionsMenu.IsOpen || rt.pauseMenu && rt.pauseMenu.IsOpen ||
            ReflexOptions.IsOpen || Reflex.Port.OpenFrontEnd.PhysicsStudioOverlay.Shown;

        // ------------------------------------------------------------------------------------------------ the game's own table
        // An input another column of the game's controller table binds (the player's saved bindings, else the defaults) is the game's: that
        // control is off for that device. The two columns the marker replaces (SetResetPoint, UseResetPoint) do not count. Read once per world
        // and binding change (the game's table itself changes from the next race).
        long tableGeneration = -2; int tableBindings, tableSaves = -1; bool setKeyTaken, rideKeyTaken, setPadTaken, ridePadTaken;
        void ReadTable(OriginalWorldRuntime rt)
        {
            int b = ReflexSettings.SpawnMarkerSetKey | ReflexSettings.SpawnMarkerRideKey << 8 | ReflexSettings.SpawnMarkerSetPad << 16 | ReflexSettings.SpawnMarkerRidePad << 24;
            long gen = rt ? rt.WorldGeneration : -1;
            int saves = CoreRemapping.Generation;   // (w-fb-hud: a binding changed in the race applies at once, so the table is read again)
            if (gen == tableGeneration && b == tableBindings && saves == tableSaves) return;
            tableGeneration = gen; tableBindings = b; tableSaves = saves;
            setKeyTaken = rideKeyTaken = setPadTaken = ridePadTaken = false;
            try
            {
                var t = CoreRemapping.Read();
                if (t == null) return;
                setKeyTaken = SpawnMarkerInput.GameColumn(t, t.kb, ReflexRemap.KeyId(t, ReflexSettings.SpawnMarkerSetKey)) >= 0;
                rideKeyTaken = SpawnMarkerInput.GameColumn(t, t.kb, ReflexRemap.KeyId(t, ReflexSettings.SpawnMarkerRideKey)) >= 0;
                setPadTaken = SpawnMarkerInput.GameColumn(t, t.js, ReflexSettings.SpawnMarkerSetPad) >= 0;
                ridePadTaken = SpawnMarkerInput.GameColumn(t, t.js, ReflexSettings.SpawnMarkerRidePad) >= 0;
            }
            catch (Exception) { }
        }

        /// OriginalWorldRuntime.Update, after the keyboard and the pads are read for the game: where markers work, the marker's own inputs are
        /// taken out of what the game gets (on their default Y / B and F / V: the game's SetResetPoint / UseResetPoint, its save-spot flag and
        /// its "back to spot" reset, which the marker replaces there). Elsewhere nothing changes. pad = the local rider's pad (pads[0]).
        public static void FilterGameInput(OriginalWorldRuntime rt, byte[] keys, ref ReflexPad pad)
        {
            var m = Instance;
            if (!m || !rt || !ReflexSettings.SpawnMarkers || Unavailable(rt).Length > 0) return;
            m.ReadTable(rt);
            void Key(int dik, bool taken) { if (!taken && dik > 0 && dik < 256 && keys != null) keys[dik] = 0; }
            Key(ReflexSettings.SpawnMarkerSetKey, m.setKeyTaken); Key(ReflexSettings.SpawnMarkerRideKey, m.rideKeyTaken);
            if (!m.setPadTaken) SpawnMarkerInput.ClearPad(ref pad, ReflexSettings.SpawnMarkerSetPad);
            if (!m.ridePadTaken) SpawnMarkerInput.ClearPad(ref pad, ReflexSettings.SpawnMarkerRidePad);
        }

        // ------------------------------------------------------------------------------------------------ per frame
        void Update()
        {
            var rt = ReflexClientLive.Runtime;
            if (!rt)
            {
                if (!ReferenceEquals(world, null)) { world = null; worldGeneration = -1; loadedKey = null; hasMarker = false; Cancel(); }   // (a destroyed world too: `if (world)` kept its graph alive in the menus, w-gcspikes)
                return;
            }
            if (rt != world || rt.WorldGeneration != worldGeneration) { world = rt; worldGeneration = rt.WorldGeneration; loadedKey = null; hasMarker = false; Cancel(); pendingResult = int.MinValue; }
            int result = pendingResult;
            if (result != int.MinValue) { pendingResult = int.MinValue; Answer(result); }
            if (!ReflexSettings.SpawnMarkers) { Cancel(); return; }
            ReadTable(rt);

            // where the marker does not work its inputs are the game's: no notice, nothing armed (Unavailable says why to SetMarker / UseMarker)
            bool blocked = Blocked(rt) || Unavailable(rt).Length > 0;
            bool craftY = rt.hovercraft && rt.hovercraft.OwnsInteract;   // the craft's "get in / out" is Y (and E)

            var kb = Keyboard.current;
            bool KeyDown(int dik, bool taken) { if (kb == null || dik == 0 || taken || OriginalOnboardAdjust.KeyboardCaptured) return false; var k = SpawnMarkerInput.KeyOf(dik); return k != Key.None && kb[k].isPressed; }
            int setDik = ReflexSettings.SpawnMarkerSetKey, rideDik = ReflexSettings.SpawnMarkerRideKey;
            Device(keys, KeyDown(setDik, setKeyTaken), KeyDown(rideDik, rideKeyTaken), setDik != 0 && setDik == rideDik, blocked, false);

            var g = rt.PadRouting.Pad(0);
            bool PadDown(int id, bool taken) => id != 0 && !taken && !(craftY && id == 12) && SpawnMarkerInput.PadDown(g, id);
            int setPad = ReflexSettings.SpawnMarkerSetPad, ridePad = ReflexSettings.SpawnMarkerRidePad;
            Device(pad, PadDown(setPad, setPadTaken), PadDown(ridePad, ridePadTaken), setPad != 0 && setPad == ridePad, blocked, true);
        }

        /// One device's controls (SpawnMarkerButtons: SET held HoldSeconds sets, RIDE pressed rides back; one input on both rows: hold / tap).
        void Device(SpawnMarkerButtons c, bool setDown, bool rideDown, bool same, bool blocked, bool isPad)
        {
            var act = c.Tick(Time.unscaledTime, setDown, rideDown, same, blocked, HoldSeconds);
            if (act == SpawnMarkerButtons.Act.None) return;
            lastWasPad = isPad;
            if ((act & SpawnMarkerButtons.Act.Set) != 0) SetMarker();
            if ((act & SpawnMarkerButtons.Act.Hint) != 0) Toast("SPAWN MARKER", $"HOLD {SetName()} TO SET IT HERE  ·  PRESS {RideName()} TO RIDE BACK TO IT");
            if ((act & SpawnMarkerButtons.Act.Ride) != 0) UseMarker();
        }
        void Cancel() { keys.Cancel(); pad.Cancel(); }

        // the control the player used (the pad's or the keyboard's) for the notices
        static string Pick(bool isPad, string k, string p) => isPad && !string.IsNullOrEmpty(p) || string.IsNullOrEmpty(k) ? p : k;
        string SetName() => Pick(lastWasPad, SpawnMarkerInput.KeyName(ReflexSettings.SpawnMarkerSetKey), SpawnMarkerInput.PadName(ReflexSettings.SpawnMarkerSetPad));
        string RideName() => Pick(lastWasPad, SpawnMarkerInput.KeyName(ReflexSettings.SpawnMarkerRideKey), SpawnMarkerInput.PadName(ReflexSettings.SpawnMarkerRidePad));
        bool OneControl() => lastWasPad ? ReflexSettings.SpawnMarkerSetPad == ReflexSettings.SpawnMarkerRidePad : ReflexSettings.SpawnMarkerSetKey == ReflexSettings.SpawnMarkerRideKey;
        void Toast(string title, string sub)
        {
            if (ReflexSettings.HudHidden) return;
            if (!notice) notice = SpawnMarkerNotice.Create(transform);
            notice.Show(title, sub);
        }

        /// Keeps the rider's place (position and heading) as this level's marker. false (with a notice): not possible now.
        public bool SetMarker()
        {
            var rt = world; if (!rt) return false;
            string why = Unavailable(rt);
            if (why.Length > 0) { Toast(why, ""); return false; }
            var b = rt.Bike;
            if (b.position == null || b.orientation == null || b.position.Length < 3 || b.orientation.Length < 4) return false;
            var pos = new Vector3(b.position[0], b.position[1], b.position[2]);
            var fwd = new Quaternion(b.orientation[0], b.orientation[1], b.orientation[2], b.orientation[3]) * Vector3.forward;
            var dir = new Vector2(fwd.x, fwd.z);
            if (dir.sqrMagnitude < 1e-6f) dir = Vector2.up;
            dir.Normalize();
            if (float.IsNaN(pos.x + pos.y + pos.z) || float.IsInfinity(pos.x + pos.y + pos.z)) return false;
            string key = CurrentKey(); if (key == null) return false;
            loadedKey = key; markerPos = pos; markerDir = dir; hasMarker = true;
            ReflexSave.SetString(key, Encode(pos, dir)); ReflexSave.Save();
            Debug.Log($"[SpawnMarker] set on {key}: ({pos.x:0.0}, {pos.y:0.0}, {pos.z:0.0}) heading ({dir.x:0.00}, {dir.y:0.00})");
            Toast("SPAWN MARKER SET", OneControl() ? $"TAP {RideName()} TO RESPAWN HERE  ·  HOLD TO MOVE IT" : $"PRESS {RideName()} TO RESPAWN HERE  ·  HOLD {SetName()} TO MOVE IT");
            return true;
        }

        /// Rides back to the marker through the game's own reset, at once (rx_session_spawn_at_instant). false (with a notice): no marker / not possible now.
        public bool UseMarker()
        {
            var rt = world; if (!rt) return false;
            string why = Unavailable(rt);
            if (why.Length > 0) { Toast(why, ""); return false; }
            LoadMarker();
            if (!hasMarker) { Toast("NO SPAWN MARKER", $"HOLD {SetName()} TO SET ONE WHERE YOU ARE"); return false; }
            var open = rt.SimDriver?.Open;
            if (open == null) { Toast("NEEDS THE OPEN REFLEX CORE", ""); return false; }
            pendingResult = int.MinValue;
            open.SpawnAt(markerPos.x, markerPos.y, markerPos.z, markerDir.x, markerDir.y, rc => pendingResult = rc, instant: true);
            return true;
        }

        void Answer(int rc)
        {
            switch (rc)
            {
                case 0:
                    Debug.Log($"[SpawnMarker] respawned at ({markerPos.x:0.0}, {markerPos.y:0.0}, {markerPos.z:0.0})");
                    if (world && world.IsPractice)
                    {   // PRACTICE (w-lb-markers): the lap is the marker's; the timing restarts from the line
                        Reflex.Port.Practice.RxPractice.MarkerUsed();
                        Toast("LAP NOT COUNTED  ·  MARKER", "TIMING STARTS AGAIN AT THE FINISH LINE");
                    }
                    break;
                case 2: Toast(OnlyHere, ""); break;   // the core's own refusal: not a Free Ride / PRACTICE
                case 3: Toast("NOT NOW", ""); break;
                default: Toast("NEEDS THE CURRENT CORE", ""); break;
            }
        }
    }

    /// The marker's HUD notice: a panel in the modern HUD's look (ReflexHudKit) over either HUD style, a title and a line under it.
    public sealed class SpawnMarkerNotice : MonoBehaviour
    {
        const float Seconds = 1.8f, Fade = 0.2f;
        CanvasGroup group; Text title, sub; HudRect panel; float shownAt = -10f;
        public static SpawnMarkerNotice Create(Transform parent)
        {
            var go = new GameObject("SpawnMarkerNotice", typeof(RectTransform));
            go.transform.SetParent(parent, false);
            var n = go.AddComponent<SpawnMarkerNotice>();
            var root = ReflexHudKit.CreateCanvas(go, 12, out _);
            n.group = go.AddComponent<CanvasGroup>(); n.group.alpha = 0f; n.group.blocksRaycasts = false; n.group.interactable = false;
            var bc = new Vector2(0.5f, 0f);
            n.panel = ReflexHudKit.Panel(root, "Panel", bc, bc, new Vector2(0, 190), new Vector2(760, 92), 160f, 160f, 0f);
            n.title = ReflexHudKit.Shadowed(ReflexHudKit.Label(n.panel.rectTransform, "Title", ReflexHudKit.Bold, ReflexHudKit.TypeSub, ReflexHudKit.White, TextAnchor.MiddleCenter,
                new Vector2(0.5f, 1f), new Vector2(0.5f, 1f), new Vector2(0, -10), new Vector2(740, 44)));
            n.sub = ReflexHudKit.Label(n.panel.rectTransform, "Sub", ReflexHudKit.Medium, ReflexHudKit.TypeLabel, ReflexHudKit.Dim, TextAnchor.MiddleCenter,
                new Vector2(0.5f, 0f), new Vector2(0.5f, 0f), new Vector2(0, 12), new Vector2(740, 30));
            return n;
        }
        public string Text => title ? title.text : "";
        public void Show(string t, string s)
        {
            title.text = t; sub.text = s ?? ""; sub.gameObject.SetActive(!string.IsNullOrEmpty(s));
            panel.rectTransform.sizeDelta = new Vector2(760, string.IsNullOrEmpty(s) ? 64 : 92);
            title.rectTransform.anchoredPosition = new Vector2(0, string.IsNullOrEmpty(s) ? -10 : -10);
            shownAt = Time.unscaledTime;
        }
        void Update()
        {
            float t = Time.unscaledTime - shownAt;
            group.alpha = t < 0 || t > Seconds ? 0f : Mathf.Clamp01(Mathf.Min(t / Fade, (Seconds - t) / Fade));
        }
    }
}

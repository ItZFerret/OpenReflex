// Core mode: stateless native defaults + separately persisted per-user bindings; applied before the next event.
// Legacy mode retains the profile-session remap backend. Existing visible rows and edit rules are shared.
using System;
using System.Threading.Tasks;
using Reflex.Port.RxCore.Bridge;
using Reflex.Port.Content;
using Reflex.Port.FrontEnd;
using Reflex.Port.OriginalWorld;
using UnityEngine;
using UnityEngine.InputSystem;
using UnityEngine.UIElements;

namespace Reflex.Port.Options
{
    public sealed partial class ReflexOptionsMenu
    {
        static readonly string[] ActionKeys = { "STR_CM_ACTION_05", "STR_CM_ACTION_06", "STR_STEERING_LEFT", "STR_STEERING_RIGHT", "STR_CM_ACTION_18", "STR_CM_ACTION_09",
            "STR_RIDER_REFLEX_LEFT", "STR_RIDER_REFLEX_RIGHT", "STR_RIDER_REFLEX_UP", "STR_RIDER_REFLEX_DOWN", "STR_CM_ACTION_02", "STR_CM_ACTION_15", "STR_CM_ACTION_11",
            "STR_CM_ACTION_16", "STR_CM_ACTION_04" };
        const int RemapRows = 15;
        /// The rows after the game's 15: SPAWN MARKERS' two controls (w-spawnmarker, w-markerbtns), ours, not columns of the game's table:
        /// SET SPAWN MARKER (held; ReflexSettings.SpawnMarkerSetKey / SetPad) and RIDE TO SPAWN MARKER (pressed; SpawnMarkerRideKey / RidePad).
        /// RESET TRACK (w-fb-hud, OriginalWorld/ReflexTrackReset.cs): the row after them, its key and pad input unbound by default (Backspace clears).
        const int SpawnSetRow = 15, SpawnRideRow = 16, ResetRow = 17, TotalRows = 18;
        static bool SpawnRow(int r) => r == SpawnSetRow || r == SpawnRideRow;
        static bool OwnRow(int r) => SpawnRow(r) || r == ResetRow;
        static string SpawnCell(int r, int c) =>
            r == ResetRow ? (c == 0 ? SpawnMarkerInput.KeyName(ReflexSettings.ResetTrackKey) : SpawnMarkerInput.PadName(ReflexSettings.ResetTrackPad)) :
            c == 0 ? SpawnMarkerInput.KeyName(r == SpawnSetRow ? ReflexSettings.SpawnMarkerSetKey : ReflexSettings.SpawnMarkerRideKey)
                   : SpawnMarkerInput.PadName(r == SpawnSetRow ? ReflexSettings.SpawnMarkerSetPad : ReflexSettings.SpawnMarkerRidePad);
        const int DikBackspace = 0x0E;
        const int KeyEsc = 103;

        RemapTable _remap; Task<RemapTable> _remapLoading; Task _remapWrite = Task.CompletedTask; bool _remapChanged;
        volatile RemapTable _remapRefreshed;
        int _listenRow = -1, _listenCol = -1, _listenEnded = -1; float _listenSince; readonly bool[] _padHeld = new bool[25];
        Label _ctlStatus; VisualElement _ctlTable;
        readonly Label[,] _cells = new Label[2, TotalRows];
        string _ctlMessage = "";

        bool CoreControls => OpenReflexSelector.UseOpenReflex;
        bool ControlsListening => _listenRow >= 0 || _listenEnded == Time.frameCount;
        /// The remap table as the page shows it (tests); null while loading / without a profile session.
        public RemapTable Remap => _remap;

        VisualElement BuildControls(VisualElement panel)
        {
            Section(panel, "CONTROLLER");
            var first = Named(OptUi.Toggle(Str("STR_OPT_VIBRATION", "RUMBLE"), () => ReflexSettings.Vibration, v =>
            {
                ReflexSettings.Vibration = v; ReflexClientLive.GameSetting("Vibration", v ? 1 : 0); ReflexSettings.Save();
            }, panel), "rumble");
            Note(panel, "The game's Vibration setting: the game rumbles the pad (crashes, landings, the engine) only when it is on.");
            Named(OptUi.Slider("RUMBLE STRENGTH", () => ReflexSettings.RumbleStrength, v => { ReflexSettings.RumbleStrength = v; ReflexSettings.Save(); }, panel, 0f, 2f, 0.1f, v => Mathf.RoundToInt(v * 100f) + "%"), "rumble-strength");
            Note(panel, "How strongly the pads rumble, against the game's own motor values (100%).");
            Named(OptUi.Slider(Str("STR_OPT_MOUSE_SENSITIVITY", "MOUSE SENSITIVITY"), () => ReflexSettings.MouseSensitivity / 100f, v =>
            {
                int n = Mathf.RoundToInt(v * 100f); ReflexSettings.MouseSensitivity = n; ReflexClientLive.GameSetting("MouseSensitivity", n); ReflexSettings.Save();
            }, panel, 0f, 1f, 0.1f), "mouse-sensitivity");
            Note(panel, "The game's MouseSensitivity: how far the mouse leans the rider. Takes effect at once in a race.");
            Named(OptUi.Toggle("MOUSE LEAN", () => ReflexSettings.MouseLean, v => { ReflexSettings.MouseLean = v; ReflexSettings.Save(); }, panel), "mouse-lean");
            Note(panel, "Off: the mouse does not move the rider (it is not sent to the game). Ours.");
            Named(OptUi.Spinner("STICK LAYOUT", () => ReflexSettings.RightHanded ? "RIGHT-HANDED" : "LEFT-HANDED", d =>
            {
                ReflexSettings.RightHanded = !ReflexSettings.RightHanded; ReflexClientLive.GameSetting("ControllerHandedness", ReflexSettings.RightHanded ? 1 : 0); ReflexSettings.Save();
            }, panel), "handedness");
            Note(panel, "The game's ControllerHandedness. RIGHT-HANDED (the game's default): the left stick steers, the right stick (and the arrow keys) move the rider. LEFT-HANDED swaps them; on the keyboard A / D then move the rider. Applies from the next race.");

            Section(panel, "CONTROLLER REMAPPING");
            _ctlStatus = OptUi.Text("", "mxa-note mxa-keep", panel); _ctlStatus.enableRichText = false;
            var head = OptUi.Div("mxa-ctl-row mxa-controls-head", panel);
            head.style.flexDirection = FlexDirection.Row;
            var hl = OptUi.Text(Str("STR_CM_ACTIONS", "ACTION"), "mxa-ctl-headcell", head); hl.style.width = Length.Percent(40);
            OptUi.Text(Str("STR_CM_KEYBOARD", "KEYBOARD"), "mxa-ctl-headcell", head);
            OptUi.Text(Str("STR_CM_JOYSTICK", "PAD"), "mxa-ctl-headcell", head);
            _ctlTable = OptUi.Div("mxa-ctl-table", panel);
            for (int r = 0; r < TotalRows; r++)
            {
                var row = OptUi.Div("mxa-ctl-row", _ctlTable);
                row.style.flexDirection = FlexDirection.Row; row.style.alignItems = Align.Center;
                var label = OptUi.Div("mxa-ctl-label", row);
                OptUi.Text(r == SpawnSetRow ? "SET SPAWN MARKER  (HOLD)" : r == SpawnRideRow ? "RIDE TO SPAWN MARKER" : r == ResetRow ? "RESET TRACK" : Str(ActionKeys[r], ActionKeys[r].Replace("STR_", "").Replace('_', ' ')), "mxa-row-label mxa-ctl-name", label);
                for (int c = 0; c < 2; c++)
                {
                    int rr = r, cc = c;
                    var cell = OptUi.Div("mxa-ctl-cell", row);
                    cell.name = $"remap-{(c == 0 ? "kb" : "pad")}-{r}";
                    _cells[c, r] = OptUi.Text("", "mxa-ctl-cell-label", cell);
                    OptUi.Item(cell, () => StartListening(rr, cc));
                }
            }
            Named(OptUi.Button(Str("STR_OPT_DEFAULT", "DEFAULTS"), "mxa-link", RemapDefaults, panel), "remap-defaults");
            Note(panel, "Restores the default controls (SPAWN MARKERS: hold Y or F to set, B or V to ride back; RESET TRACK unbound).");
            Note(panel, "Enter on a cell, then press the key or pad input for it (Esc cancels). As in the game: Q, E, 3 and Alt are refused, and an input that is already used elsewhere in the table is refused. Core controls are saved separately for this user; in a race (the pause page's OPTIONS) a change applies at once and stays for the next races, exactly as one made in the menus. " +
                        "SET / RIDE TO SPAWN MARKER (ours, Free Ride and PRACTICE): hold SET to keep your place as the marker, press RIDE to respawn there at once; they apply at once. " +
                        "By default they are the inputs of the game's own reset-point flag (Y / F: set it, B / V: back to it), which the spawn marker replaces in Free Ride and PRACTICE; in races those inputs stay the game's. " +
                        "They cannot share an input with the game's other actions, the D-pad (the game's look back and quick looks), START or the stick clicks. The same input on both rows: hold it to set, tap it to ride back. " +
                        "RESET TRACK (ours, Free Ride, PRACTICE and races without AI, offline): the ruts and tyre tracks back to the untouched track, the rider stays where he is (also on the pause page). Unbound by default; Backspace clears its cell.");
            // in a race the table comes from the game's profile session, which a race does not start by itself: on request
            if (!CoreControls && ReflexClientLive.Runtime && !ReflexProfile.IsOpen && _remap == null && _remapLoading == null)
            {
                _ctlMessage = "The game's controls come from its profile (about a second to start in a race).";
                Named(OptUi.Button("SHOW THE GAME'S CONTROLS", "mxa-link", () => { LoadRemap(); RefreshRemap(); }, panel), "remap-load");
                Note(panel, "Reads the game's controller table from its profile.");
            }
            else LoadRemap();
            RefreshRemap();
            return first;
        }

        void LoadRemap()
        {
            if (_remap != null || _remapLoading != null) return;
            _ctlMessage = "Reading the game's controls ...";
            try
            {
                if (CoreControls)
                {
                    _remap = CoreRemapping.Read(); _ctlMessage = ReflexClientLive.Runtime ? "Changes apply at once and are saved." : "Changes are saved and apply from the next race."; return;
                }
                var open = ReflexProfile.Open();   // (main thread: the first open reads the install's paths)
                _remapLoading = Task.Run(() => open.Result ? ReflexRemap.Read() : null);
            }
            catch (Exception e) { _ctlMessage = "The game's controls are not available: " + e.Message; }
        }

        void RefreshRemap()
        {
            if (_ctlStatus == null || _ctlStatus.panel == null) return;
            for (int r = 0; r < TotalRows; r++)
                for (int c = 0; c < 2; c++)
                {
                    var l = _cells[c, r]; if (l == null) continue;
                    string[] texts = _remap?.Text(c);
                    string t = _listenRow == r && _listenCol == c ? "???" : OwnRow(r) ? SpawnCell(r, c) : texts != null && r < texts.Length ? texts[r] : "";
                    l.text = string.IsNullOrEmpty(t) ? "—" : t.ToUpperInvariant();
                    l.parent?.EnableInClassList("mxa-ctl-unbound", string.IsNullOrEmpty(t));
                }
            _ctlStatus.text = _ctlMessage;
        }

        void TickControls()
        {
            if (_tab != 0) return;
            if (_remapLoading != null && _remapLoading.IsCompleted)
            {
                _remap = _remapLoading.IsFaulted ? null : _remapLoading.Result; _remapLoading = null;
                _ctlMessage = _remap == null ? "The game's controls are not available (no profile session: " + (ReflexProfile.Error ?? "the plugin or the install is missing") + ")." : "";
                RefreshRemap();
            }
            var refreshed = _remapRefreshed;
            if (refreshed != null) { _remapRefreshed = null; _remap = refreshed; RefreshRemap(); }
            if (_listenRow < 0) return;
            if (Time.unscaledTime - _listenSince < 0.15f) return;   // the Enter / A that started it
            var kb = Keyboard.current;
            if (kb != null)
            {
                if (kb.escapeKey.wasPressedThisFrame) { StopListening("Cancelled."); return; }
                foreach (var kv in DirectInputMap.Table)
                    if (kb[kv.Key].wasPressedThisFrame) { KeyPressed(kv.Value); return; }
            }
            var g = Gamepad.current;
            if (g != null && _listenCol == 1)
            {
                bool[] now = new bool[25];
                Vector2 l = g.leftStick.ReadValue(), r = g.rightStick.ReadValue();
                now[1] = Mathf.Abs(l.x) > 0.5f; now[2] = Mathf.Abs(l.y) > 0.5f; now[3] = Mathf.Abs(r.x) > 0.5f; now[4] = Mathf.Abs(r.y) > 0.5f;
                now[9] = g.buttonSouth.isPressed; now[10] = g.buttonEast.isPressed; now[11] = g.buttonWest.isPressed; now[12] = g.buttonNorth.isPressed;
                now[13] = g.leftShoulder.isPressed; now[14] = g.rightShoulder.isPressed; now[15] = g.startButton.isPressed; now[16] = g.selectButton.isPressed;
                now[17] = g.leftStickButton.isPressed; now[18] = g.rightStickButton.isPressed; now[19] = g.leftTrigger.ReadValue() > 0.5f; now[20] = g.rightTrigger.ReadValue() > 0.5f;
                now[21] = g.dpad.up.isPressed; now[22] = g.dpad.right.isPressed; now[23] = g.dpad.down.isPressed; now[24] = g.dpad.left.isPressed;
                for (int i = 1; i < 25; i++) { bool down = now[i] && !_padHeld[i]; _padHeld[i] = now[i]; if (down) { PadPressed(i); return; } }
            }
        }

        void StartListening(int row, int col)
        {
            if (_remap == null) { _ctlMessage = _remapLoading != null ? "Still reading the game's controls ..." : _ctlMessage; RefreshRemap(); return; }
            if (col == 1 && Gamepad.all.Count == 0) { _ctlMessage = "Connect a pad to change the PAD column."; RefreshRemap(); return; }
            if (_listenRow >= 0) return;
            _listenRow = row; _listenCol = col; _listenSince = Time.unscaledTime;
            var g = Gamepad.current;
            if (g != null) { _padHeld[9] = g.buttonSouth.isPressed; }
            _ctlMessage = col == 0 ? "Press a key for " + ActionName(row) + " (Esc cancels)." : "Press a pad input for " + ActionName(row) + " (Esc cancels).";
            RefreshRemap();
        }

        void StopListening(string message)
        {
            _listenRow = _listenCol = -1; _listenEnded = Time.frameCount; _ctlMessage = message ?? "";
            RefreshRemap();
        }

        string ActionName(int row) => row == SpawnSetRow ? "SET SPAWN MARKER" : row == SpawnRideRow ? "RIDE TO SPAWN MARKER" : row == ResetRow ? "RESET TRACK" : row >= 0 && row < ActionKeys.Length ? Str(ActionKeys[row], ActionKeys[row]) : "CAMERA";

        // a key (DIK): the game's key id (its scan-code table); Q, E, 3, Alt refused, as ControllerRemapping.lua's OnKeyBoardInput
        void KeyPressed(int dik)
        {
            if (_listenRow == ResetRow && dik == DikBackspace) { ClearReset(); return; }   // (w-fb-hud) RESET TRACK: Backspace unbinds the cell
            int id = ReflexRemap.KeyId(_remap, dik);
            if (id == 0) { _ctlMessage = "The game does not map that key."; RefreshRemap(); return; }
            if (id == KeyEsc) { StopListening("Cancelled."); return; }
            if (_listenCol != 0) return;   // the PAD column takes pad inputs (Esc cancels)
            if (_listenRow == ResetRow) { BindResetKey(dik, id); return; }
            if (SpawnRow(_listenRow)) { BindSpawnKey(dik, id); return; }
            if (id == 51 || id == 39 || id == 64 || id == 94 || id == 95) { _ctlMessage = "The game keeps that key for itself (Q, E, 3 and Alt are refused)."; RefreshRemap(); return; }
            Bind(id);
        }

        void PadPressed(int id)
        {
            if (_listenRow == ResetRow) BindResetPad(id); else if (SpawnRow(_listenRow)) BindSpawnPad(id); else Bind(id);
        }

        // SPAWN MARKERS' own inputs (w-spawnmarker, w-markerbtns): saved at once in the client's settings. Refused: an input the game's table binds
        // to another action than the two the marker replaces (SetResetPoint / UseResetPoint: the game's wins), and the inputs the game or the
        // client uses while riding (SpawnMarkerInput.KeyRefusal / PadRefusal). Both rows may take the same input (held = set, tapped = ride back).
        void BindSpawnKey(int dik, int id)
        {
            bool set = _listenRow == SpawnSetRow;
            if (dik == (set ? ReflexSettings.SpawnMarkerSetKey : ReflexSettings.SpawnMarkerRideKey)) { StopListening("Unchanged."); return; }
            string why = SpawnMarkerInput.KeyRefusal(dik);
            if (why != null) { _ctlMessage = why; RefreshRemap(); return; }
            if (dik == ReflexSettings.ResetTrackKey) { _ctlMessage = "\"" + SpawnMarkerInput.KeyName(dik) + "\" is used by RESET TRACK: change that one first."; RefreshRemap(); return; }
            int col = SpawnMarkerInput.GameColumn(_remap, _remap?.kb, id);
            if (col >= 0) { _ctlMessage = "\"" + _remap.Name(id).ToUpperInvariant() + "\" is used by the game's " + ActionName(col) + ": change that one first."; RefreshRemap(); return; }
            if (set) ReflexSettings.SpawnMarkerSetKey = dik; else ReflexSettings.SpawnMarkerRideKey = dik;
            ReflexSettings.Save();
            StopListening(ActionName(_listenRow) + ": " + SpawnMarkerInput.KeyName(dik));
        }

        void BindSpawnPad(int id)
        {
            bool set = _listenRow == SpawnSetRow;
            if (id == (set ? ReflexSettings.SpawnMarkerSetPad : ReflexSettings.SpawnMarkerRidePad)) { StopListening("Unchanged."); return; }
            string why = SpawnMarkerInput.PadRefusal(id);
            if (why != null) { _ctlMessage = why; RefreshRemap(); return; }
            if (id == ReflexSettings.ResetTrackPad) { _ctlMessage = "\"" + SpawnMarkerInput.PadName(id) + "\" is used by RESET TRACK: change that one first."; RefreshRemap(); return; }
            int col = SpawnMarkerInput.GameColumn(_remap, _remap?.js, id);
            if (col >= 0) { _ctlMessage = "\"" + SpawnMarkerInput.PadName(id) + "\" is used by the game's " + ActionName(col) + ": change that one first."; RefreshRemap(); return; }
            if (set) ReflexSettings.SpawnMarkerSetPad = id; else ReflexSettings.SpawnMarkerRidePad = id;
            ReflexSettings.Save();
            StopListening(ActionName(_listenRow) + ": " + SpawnMarkerInput.PadName(id));
        }

        // RESET TRACK's own inputs (w-fb-hud): saved at once in the client's settings (as the spawn markers'). Refused: what the spawn markers
        // refuse (SpawnMarkerInput.KeyRefusal / PadRefusal), an input of the game's table (any column) and the spawn markers' inputs.
        void BindResetKey(int dik, int id)
        {
            if (dik == ReflexSettings.ResetTrackKey) { StopListening("Unchanged."); return; }
            string why = SpawnMarkerInput.KeyRefusal(dik);
            if (why != null) { _ctlMessage = why; RefreshRemap(); return; }
            if (dik == ReflexSettings.SpawnMarkerSetKey || dik == ReflexSettings.SpawnMarkerRideKey) { _ctlMessage = "\"" + SpawnMarkerInput.KeyName(dik) + "\" is used by the SPAWN MARKERS: change that one first."; RefreshRemap(); return; }
            if (_remap.Mapped(id)) { int col = Array.IndexOf(_remap.kb, id); _ctlMessage = "\"" + _remap.Name(id).ToUpperInvariant() + "\" is used by the game's " + (col >= 0 ? ActionName(col) : "camera") + ": change that one first."; RefreshRemap(); return; }
            ReflexSettings.ResetTrackKey = dik; ReflexSettings.Save();
            StopListening("RESET TRACK: " + SpawnMarkerInput.KeyName(dik));
        }

        void BindResetPad(int id)
        {
            if (id == ReflexSettings.ResetTrackPad) { StopListening("Unchanged."); return; }
            string why = SpawnMarkerInput.PadRefusal(id);
            if (why != null) { _ctlMessage = why; RefreshRemap(); return; }
            if (id == ReflexSettings.SpawnMarkerSetPad || id == ReflexSettings.SpawnMarkerRidePad) { _ctlMessage = "\"" + SpawnMarkerInput.PadName(id) + "\" is used by the SPAWN MARKERS: change that one first."; RefreshRemap(); return; }
            int col = Array.IndexOf(_remap.js, id);
            if (col >= 0) { _ctlMessage = "\"" + SpawnMarkerInput.PadName(id) + "\" is used by the game's " + ActionName(col) + ": change that one first."; RefreshRemap(); return; }
            ReflexSettings.ResetTrackPad = id; ReflexSettings.Save();
            StopListening("RESET TRACK: " + SpawnMarkerInput.PadName(id));
        }

        void ClearReset()
        {
            if (_listenCol == 0) ReflexSettings.ResetTrackKey = 0; else ReflexSettings.ResetTrackPad = 0;
            ReflexSettings.Save();
            StopListening("RESET TRACK: " + (_listenCol == 0 ? "no key" : "no pad input"));
        }

        /// The game's actions cannot take an input a SPAWN MARKERS control uses (the client's own controls; the game's table is checked the other way
        /// above), except the two actions the marker replaces where it works (SetResetPoint / UseResetPoint): they may share it, as by default.
        /// The row of the marker control that claims it, -1 none.
        int SpawnClaims(int row, int column, int id)
        {
            if (row == 0 ? ReflexSettings.ResetTrackKey != 0 && id == ReflexRemap.KeyId(_remap, ReflexSettings.ResetTrackKey) : ReflexSettings.ResetTrackPad != 0 && id == ReflexSettings.ResetTrackPad) return ResetRow;
            if (SpawnMarkerInput.Replaces(_remap, column)) return -1;
            if (row == 0)
            {
                if (id == ReflexRemap.KeyId(_remap, ReflexSettings.SpawnMarkerSetKey)) return SpawnSetRow;
                if (id == ReflexRemap.KeyId(_remap, ReflexSettings.SpawnMarkerRideKey)) return SpawnRideRow;
                return -1;
            }
            return id == ReflexSettings.SpawnMarkerSetPad ? SpawnSetRow : id == ReflexSettings.SpawnMarkerRidePad ? SpawnRideRow : -1;
        }

        void Bind(int id)
        {
            int row = _listenCol, column = _listenRow;
            var cells = _remap.Row(row);
            if (column < cells.Length && cells[column] == id) { StopListening("Unchanged."); return; }
            int claim = SpawnClaims(row, column, id);
            if (claim >= 0) { _ctlMessage = "\"" + _remap.Name(id).ToUpperInvariant() + "\" is used by " + ActionName(claim) + ": change that one first."; RefreshRemap(); return; }
            if (_remap.Mapped(id)) { _ctlMessage = "\"" + _remap.Name(id).ToUpperInvariant() + "\" is already used: change that one first (the game allows each input once)."; RefreshRemap(); return; }
            if (CoreControls)
            {
                int previous = cells[column]; cells[column] = id;
                bool live;
                try { live = CoreRemapping.SaveAndApply(_remap); }   // (w-fb-hud) saved, and in a race applied at once: the menus' and the pause page's path is one
                catch (Exception e) { cells[column] = previous; StopListening("Controls were not saved: " + e.Message); return; }
                _remap.Text(row)[column] = _remap.Name(id);
                StopListening(ActionName(column) + ": " + _remap.Name(id).ToUpperInvariant() + (live ? " (saved, applied now)" : " (saved)")); return;
            }
            if (column < cells.Length) cells[column] = id;
            var texts = _remap.Text(row); if (column < texts.Length) texts[column] = _remap.Name(id);
            var prev = _remapWrite;
            _remapWrite = Task.Run(async () => { await prev.ConfigureAwait(false); ReflexRemap.Set(row, column, id); });
            _remapChanged = true;
            StopListening(ActionName(column) + ": " + _remap.Name(id).ToUpperInvariant() + (ReflexClientLive.Runtime ? " (from the next race)" : ""));
        }

        void RemapDefaults()
        {
            if (ControlsListening || !CoreControls && _remap == null) return;
            ReflexSettings.SpawnMarkerSetKey = ReflexSettings.SpawnMarkerSetKeyDefault; ReflexSettings.SpawnMarkerSetPad = ReflexSettings.SpawnMarkerSetPadDefault;
            ReflexSettings.SpawnMarkerRideKey = ReflexSettings.SpawnMarkerRideKeyDefault; ReflexSettings.SpawnMarkerRidePad = ReflexSettings.SpawnMarkerRidePadDefault;
            ReflexSettings.ResetTrackKey = 0; ReflexSettings.ResetTrackPad = 0; ReflexSettings.Save();
            if (CoreControls)
            {
                try { var defaults = CoreRemapping.Read(true); bool live = CoreRemapping.SaveAndApply(defaults); _remap = defaults; _ctlMessage = live ? "Default controls saved and applied." : "Default controls saved."; }
                catch (Exception e) { _ctlMessage = "Controls were not saved: " + e.Message; }
                RefreshRemap(); return;
            }
            var prev = _remapWrite;
            _remapWrite = Task.Run(async () => { await prev.ConfigureAwait(false); ReflexRemap.Defaults(); var r = ReflexRemap.Read(); if (r != null) _remapRefreshed = r; });
            _remapChanged = true;
            _ctlMessage = "The game's default controls.";
            RefreshRemap();
        }

        /// Leaving the page: the game saves the table (App.RequestSaveGame) when it changed.
        void CloseControls()
        {
            _listenRow = _listenCol = -1;
            if (!_remapChanged) return;
            _remapChanged = false;
            var w = _remapWrite;
            Task.Run(async () => { await w.ConfigureAwait(false); await ReflexRemap.Save().ConfigureAwait(false); });
        }
    }
}

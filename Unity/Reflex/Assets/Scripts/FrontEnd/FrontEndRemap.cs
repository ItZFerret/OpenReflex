// ControllerRemapping.lua (page "Controller Remap Page" of FrontEndPageGraph.pag: Background01.lyr, ControllerRemapping.lyr,
// ButtonLegend.lyr, Header.lyr; reached from OPTIONS > CONTROLLER OPTIONS, OptionsFrontEnd:OnControllerSettingsKeypress ->
// "GotoControllerRemapPage"). Three lists side by side: ACTIONS (the 15 actions, STR_CM_ACTION_* / STR_STEERING_* / STR_RIDER_REFLEX_*),
// KEY (the keyboard row of the game's ControllerRemapping table) and PAD (its joystick row = the XInput pad); Up / Down move the action,
// Left / Right switch between the KEY and PAD columns, Enter (the keyboard) or A (the pad) starts a binding ("???"), the next key / pad
// input binds it unless the game says it is already mapped (then a press of the cell's own input cancels; Esc cancels; Q, E, 3 and the
// Alt keys are refused). F2 (or a click on the legend's DEFAULT) restores the defaults (App.InitializeRemapTables( true )). The page
// End saves (App.RequestSaveGame) when something changed. Esc / B travel back unless a binding is in progress (App.AllowTravelBack).
//
// Everything about the table is the game's own, in the profile session (ReflexProfile, native remap.cpp): the cells, their names
// (App.GetRemapTableCharByIndex: DirectInput's key names, the pad's "rt" / "ls_x" ...), alreadyMapped (005b99e0), the writes
// (App.SetRemapTableByIndex, App.InitializeRemapTables) and the save (savegame.bxml, where the table lives with the Player* tables).
// Races use it: OriginalInput asks the world to build the local player's KeyMaps from it again after the save is attached
// (reflex_world_remap "apply"; the original's player creation reads it, 00547f30). _re/notes/input-settings.md "Controller remapping".
//
// The UI system keyboard (UISystemKeyboard, 005dc960) as the page configures it (InitializeUIDisplayText, UseAlphaNumeric, UseMisc,
// SetInputSize 3) turns each pressed key into a UIKeyboardInput event (text, game key id, alreadyMapped): letters / digits / space
// append to the display text while it is shorter than 3 and send; Backspace removes one and sends; the other keys (0x47..0x87:
// punctuation, F keys, modifiers, Enter, Esc, Tab, the keypad, arrows) send without appending, also only while the text is short;
// pad inputs send UIJoystickInput ("nothing", pad id, alreadyMapped) (UIListContainer::vf8 005d0000). Ported here from the keys Unity
// reports (DirectInputMap -> DIK -> the game's own scan-code table 00a95848).
using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using Reflex.Port.Content;
using Reflex.Port.OriginalWorld;
using UnityEngine;
using UnityEngine.InputSystem;

namespace Reflex.Port.FrontEnd
{
    /// The game's ControllerRemapping table as the profile session reports it (native remap.cpp "remap").
    public sealed class RemapTable
    {
        public string[] columns = new string[0];
        public int[] kb = new int[0], js = new int[0];
        public string[] kbText = new string[0], jsText = new string[0];
        public readonly byte[] scan = new byte[256];   // DIK -> game key id (g_ScanCodeToGameKey 00a95848)
        public readonly Dictionary<int, string> names = new Dictionary<int, string>();   // game input id -> its name (keys 0x23..0x87, pad 1..24)
        public int[] Row(int row) => row == 0 ? kb : js;
        public string[] Text(int row) => row == 0 ? kbText : jsText;
        /// Remap_IsMapped 005b99e0: any cell of the table (both rows, all 19 columns) holds the input.
        public bool Mapped(int id) => kb.Contains(id) || js.Contains(id);
        public string Name(int id) => names.TryGetValue(id, out var s) ? s : "";

        public static RemapTable Parse(string json)
        {
            var root = OriginalUi.ParseJson(json) as Dictionary<string, object>;
            if (root == null) throw new FormatException("remap: not an object");
            var t = new RemapTable();
            int[] Ints(string k) => root.TryGetValue(k, out var o) && o is List<object> l ? l.Select(x => Convert.ToInt32(x)).ToArray() : new int[0];
            string[] Strs(string k) => root.TryGetValue(k, out var o) && o is List<object> l ? l.Select(x => x as string ?? "").ToArray() : new string[0];
            t.columns = Strs("columns"); t.kb = Ints("kb"); t.js = Ints("js"); t.kbText = Strs("kbText"); t.jsText = Strs("jsText");
            var sc = Ints("scan"); for (int i = 0; i < 256 && i < sc.Length; i++) t.scan[i] = (byte)sc[i];
            foreach (var k in new[] { "keyNames", "joyNames" })
                if (root.TryGetValue(k, out var o) && o is Dictionary<string, object> d) foreach (var kv in d) t.names[int.Parse(kv.Key)] = kv.Value as string ?? "";
            return t;
        }
    }

    /// The game's remap operations in the profile session (blocking: call from a worker thread or accept a short wait; the first
    /// ReflexProfile.Open must come from the main thread).
    public static class ReflexRemap
    {
        /// The table, its names and the scan-code table; null without a session (or an older libreflexworld).
        public static RemapTable Read()
        {
            if (!ReflexProfile.Open().Result) return null;
            string r = ReflexProfile.Command("remap");
            if (r == null || !r.StartsWith("{")) return null;
            try { return RemapTable.Parse(r); } catch (Exception e) { Debug.LogWarning("[Remap] " + e.Message); return null; }
        }
        /// App.SetRemapTableByIndex( row, column, input ).
        public static bool Set(int row, int column, int input) => ReflexProfile.Open().Result && ReflexProfile.Command($"lua App.SetRemapTableByIndex( {row}, {column}, {input} )") != null;
        /// App.InitializeRemapTables( true ): the defaults of 005b9340 written over the table.
        public static bool Defaults() => ReflexProfile.Open().Result && ReflexProfile.Command("lua App.InitializeRemapTables( true )") != null;
        /// App.RequestSaveGame( device ): the save (the Save Popup shows while it writes).
        public static Task<bool> Save() => ReflexProfile.Run("save");
        /// What the game's UISystemKeyboard reports for a DIK scan code (0: a key the game does not map).
        public static int KeyId(RemapTable t, int dik) => t != null && dik >= 0 && dik < 256 ? t.scan[dik] : 0;
    }

    public sealed class ControllerRemappingScript : FeScript
    {
        // the 15 rows of InitializeRemaps (the Pause row is commented out in the Lua; the CameraTweak columns have no row)
        static readonly string[] ActionKeys = { "STR_CM_ACTION_05", "STR_CM_ACTION_06", "STR_STEERING_LEFT", "STR_STEERING_RIGHT", "STR_CM_ACTION_18", "STR_CM_ACTION_09",
            "STR_RIDER_REFLEX_LEFT", "STR_RIDER_REFLEX_RIGHT", "STR_RIDER_REFLEX_UP", "STR_RIDER_REFLEX_DOWN", "STR_CM_ACTION_02", "STR_CM_ACTION_15", "STR_CM_ACTION_11",
            "STR_CM_ACTION_16", "STR_CM_ACTION_04" };
        public const int Rows = 15;
        const int KeyEnter = 102, KeyEsc = 103, KeyF2 = 83, KeyBackspace = 100, PadA = 9;

        FeNode actionList, kbList, jsList;
        RemapTable table; Task<RemapTable> loading;
        readonly HashSet<string> handlers = new HashSet<string>();   // App.AddEventHandler( "UI", "UIKeyboardInput" / "UIJoystickInput", ... )
        int currentDataIndex;                    // m_CurrentDataIndex
        bool kbActive = true, binding, needsSave, allowTravelBack = true;   // m_KBBindsIsActive, m_CurrentlyBinding, m_NeedsSave, App.AllowTravelBack
        int activeRow, activeColumn;             // m_ActiveComponentRow / Column
        FeNode activeText;                       // m_ActiveTextComponent
        string displayText = "";                 // the UI display text (InitializeUIDisplayText clears it)
        const int InputSize = 3;                 // App.SetInputSize( 3 )
        float started; bool enabledKbJs;
        readonly bool[] padHeld = new bool[25];
        Task pendingWrite = Task.CompletedTask;

        public RemapTable Table => table;
        public bool Binding => binding;
        public bool KbActive => kbActive;
        public int CurrentRow => currentDataIndex;
        public bool NeedsSave => needsSave;
        public bool IsLoaded => table != null;
        public string CellText(int row, int index) => Text(row == 0 ? kbList : jsList, index)?.text?.Text ?? "";
        public string ActionText(int index) => Text(actionList, index)?.text?.Text ?? "";
        public Task PendingWrite => pendingWrite;
        /// Pads the original counts (App.GetNumAttachedJoysticks: XenonInputManager::vf1(1), the connected XInput pads of 4).
        public static int AttachedJoysticks => Math.Min(4, Gamepad.all.Count);

        public override void Initialize()
        {
            actionList = layer.Find("ActionList")?.Kid("NavListContainer");
            kbList = layer.Find("KBBindsList")?.Kid("NavListContainer");
            jsList = layer.Find("JSBindsList")?.Kid("NavListContainer");
            functions["OnAcceptMotocardList"] = () => { };
            functions["OnBackKeypress"] = OnBackKeypress;
            functions["OnDefaultKeypress"] = OnDefaultKeypress;
            functions["EnableKBJS"] = EnableKBJS;
            // App.SetNavigationType( list, 0 ) + App.AllowDynamicListWrap( list, true ) on the two binding lists: Left / Right on the
            // focused one move it, which OnActionListFocusedChanged turns into the column switch (a three-slot list kept on its middle
            // slot, so every press moves and wraps).
            foreach (var l in new[] { kbList, jsList }) if (l != null) { l.props["NavigationType"] = "LeftRight"; l.onListChanged = (n, old) => OnActionListFocusedChanged(n); }
            if (actionList != null) actionList.onListChanged = (n, old) => OnActionListFocusedChanged(n);
        }

        public override void Start()
        {
            binding = false;
            fe.SetButtonLegend("back", true, "STR_BACK");
            fe.SetButtonLegend("misc", true, "STR_OPT_DEFAULT");
            fe.SetHeader("STR_OPT_CONTROLLERSETTINGS");
            // MouseActivation_Container on the legend: backContainer -> OnBackKeypress, miscContainer -> OnDefaultKeypress
            fe.legend?.SetAction("back", () => { if (handlers.Contains("MouseActivation_Container")) OnBackKeypress(); });
            fe.legend?.SetAction("misc", () => { if (handlers.Contains("MouseActivation_Container")) OnDefaultKeypress(); });
            needsSave = false; allowTravelBack = true;
            handlers.Clear(); handlers.Add("MouseActivation_Container");
            InitializeUIDisplayText();
            started = Time.unscaledTime; enabledKbJs = false;
            Array.Clear(padHeld, 0, padHeld.Length);
            currentDataIndex = 0; kbActive = true;
            if (actionList != null) { actionList.numItems = Rows; actionList.absIndex = 0; }
            foreach (var l in new[] { kbList, jsList }) if (l != null) { l.numItems = 3; l.absIndex = 1; }
            for (int i = 0; i < Rows; i++) Text(actionList, i)?.SetText(Str(ActionKeys[i]));
            foreach (var l in new[] { actionList, kbList, jsList }) if (l != null) for (int i = Rows; i < l.kids.Count; i++) l.kids[i].Visible = false;   // SetNumItems( 15 ) of 16 slots
            table = null;
            var open = ReflexProfile.Open();   // (main thread: the first open reads the install's paths)
            loading = Task.Run(() => open.Result ? ReflexRemap.Read() : null);
            if (ReflexProfile.IsOpen) loading.Wait(2000);   // the session is up (the title's Load Game): fill the lists now, as the original
            if (loading.IsCompleted) Loaded(loading.Result);
            OnActionListFocusedChanged(actionList);
        }

        public override void End()
        {
            if (needsSave) { var w = pendingWrite; Task.Run(async () => { await w.ConfigureAwait(false); await ReflexRemap.Save().ConfigureAwait(false); }); needsSave = false; }   // App.RequestSaveGame( exclusive device )
            fe.legend?.SetAction("back", () => fe.DispatchKey("Back")); fe.legend?.SetAction("misc", () => fe.DispatchKey("Misc"));
            handlers.Clear();
        }

        void Loaded(RemapTable t)
        {
            loading = null; table = t;
            if (t == null) { Debug.LogWarning("[Remap] the game's remap table is not available: " + ReflexProfile.Error); return; }
            InitializeRemaps();
        }

        /// InitializeRemaps: every row's KEY / PAD text = App.GetRemapTableCharByIndex( 0 / 1, row ).
        void InitializeRemaps()
        {
            if (table == null) return;
            for (int i = 0; i < Rows; i++)
            {
                Text(kbList, i)?.SetText(i < table.kbText.Length ? table.kbText[i] : "");
                Text(jsList, i)?.SetText(i < table.jsText.Length ? table.jsText[i] : "");
            }
        }

        static FeNode Text(FeNode list, int index) => list != null && index >= 0 && index < list.kids.Count ? list.kids[index].Kid("Text") : null;

        public override void Update(float dt)
        {
            if (loading != null && loading.IsCompleted) Loaded(loading.Result);
            LateRefresh();
            if (!enabledKbJs && Time.unscaledTime - started >= 0.5f) { enabledKbJs = true; EnableKBJS(); }   // the layer's UIElapsedTimeTrigger (500 ms, once)
            if (fe.Busy || fe.TopPopup != null) return;
            var kb = Keyboard.current;
            // the page's own travel back (MapKey_UI_Back: Esc / the pad's B), off while a binding is in progress (as it was when the key came:
            // the Esc that cancels a binding does not also leave the page)
            bool back = (kb != null && kb.escapeKey.wasPressedThisFrame || Gamepad.current != null && Gamepad.current.buttonEast.wasPressedThisFrame) && allowTravelBack;
            if (kb != null)
                foreach (var kv in DirectInputMap.Table) if (kb[kv.Key].wasPressedThisFrame) KeyDown(kv.Value);
            PollPad();
            if (back) TravelBack();
        }

        public void TravelBack() { if (allowTravelBack && !fe.Busy) { fe.PlaySound("Back"); fe.SendUIEvent("TravelBack"); } }

        // ------------------------------------------------------------------------------------- the UI system keyboard / pad events
        void InitializeUIDisplayText() => displayText = "";

        /// A key pressed (DIK scan code): the UISystemKeyboard's UIKeyboardInput event, as 005dc960 makes it.
        public void KeyDown(int dik)
        {
            int id = ReflexRemap.KeyId(table, dik);
            if (id == 0 || table == null) return;
            bool send = false;
            if (id == KeyBackspace) { if (displayText.Length > 0) displayText = displayText.Substring(0, displayText.Length - 1); send = true; }
            else if (displayText.Length < InputSize)
            {
                bool shift = Keyboard.current != null && (Keyboard.current.leftShiftKey.isPressed || Keyboard.current.rightShiftKey.isPressed);
                if (id >= 0x23 && id <= 0x3c) { char c = (char)('a' + id - 0x23); displayText += shift ? char.ToUpperInvariant(c) : c; send = true; }   // UseAlphaNumeric: letters
                else if (id >= 0x3d && id <= 0x46) { displayText += (char)('0' + id - 0x3d); send = true; }                                          // digits
                else if (id >= 0x74 && id <= 0x7d) { displayText += (char)('0' + id - 0x74); send = true; }                                          // the keypad's digits
                else if (id == 0x65) { displayText += " "; send = true; }                                                                            // space
                else if (id >= 0x47 && id < 0x88) send = true;                                                                                       // UseMisc
            }
            if (send) Raise("UIKeyboardInput", displayText, id, table.Mapped(id));
        }

        /// A pad input pressed (the game's pad id: 1..4 sticks, 9 A .. 20 RT, 21..24 the d-pad): UIJoystickInput.
        public void PadDown(int id) { if (table != null && id > 0) Raise("UIJoystickInput", "nothing", id, table.Mapped(id)); }

        void PollPad()
        {
            var g = Gamepad.current; if (g == null) return;
            bool[] now = new bool[25];
            Vector2 l = g.leftStick.ReadValue(), r = g.rightStick.ReadValue();
            now[1] = Mathf.Abs(l.x) > 0.5f; now[2] = Mathf.Abs(l.y) > 0.5f; now[3] = Mathf.Abs(r.x) > 0.5f; now[4] = Mathf.Abs(r.y) > 0.5f;
            now[9] = g.buttonSouth.isPressed; now[10] = g.buttonEast.isPressed; now[11] = g.buttonWest.isPressed; now[12] = g.buttonNorth.isPressed;
            now[13] = g.leftShoulder.isPressed; now[14] = g.rightShoulder.isPressed; now[15] = g.startButton.isPressed; now[16] = g.selectButton.isPressed;
            now[17] = g.leftStickButton.isPressed; now[18] = g.rightStickButton.isPressed; now[19] = g.leftTrigger.ReadValue() > 0.5f; now[20] = g.rightTrigger.ReadValue() > 0.5f;
            now[21] = g.dpad.up.isPressed; now[22] = g.dpad.right.isPressed; now[23] = g.dpad.down.isPressed; now[24] = g.dpad.left.isPressed;
            for (int i = 1; i < 25; i++) { if (now[i] && !padHeld[i]) PadDown(i); padHeld[i] = now[i]; }
        }

        void Raise(string evt, string text, int id, bool mapped)
        {
            // the handlers registered when the event is sent (one may register another, which does not see this event)
            var now = handlers.ToList();
            if (evt == "UIKeyboardInput")
            {
                if (now.Contains("OnKeyBoardSelection")) OnKeyBoardSelection(text, id);
                if (now.Contains("OnKeyBoardInput")) OnKeyBoardInput(text, id, mapped);
                if (now.Contains("OnKBInputDuringJSInput")) OnKBInputDuringJSInput(id);
            }
            else
            {
                if (now.Contains("OnJoystickSelection")) OnJoystickSelection(id);
                if (now.Contains("OnJoystickInput")) OnJoystickInput(text, id, mapped);
            }
        }
        void Add(string h) => handlers.Add(h);
        void Remove(string h) => handlers.Remove(h);

        // ------------------------------------------------------------------------------------- the Lua's handlers
        void EnableKBJS() { Add("OnKeyBoardSelection"); Add("OnJoystickSelection"); }

        void OnBackKeypress() { fe.PlaySound("Back"); fe.SendUIEvent("TravelBack"); }

        void OnDefaultKeypress()
        {
            if (binding) return;
            pendingWrite = Chain(() => ReflexRemap.Defaults());
            needsSave = true;
            // InitializeRemaps after the game reset the table: read back (its names)
            pendingWrite = pendingWrite.ContinueWith(_ => { var r = ReflexRemap.Read(); if (r != null) refreshed = r; });
        }
        volatile RemapTable refreshed;
        public override void Present() { }

        Task Chain(Action a) { var prev = pendingWrite; return Task.Run(async () => { await prev.ConfigureAwait(false); a(); }); }

        void StartBinding(int row)
        {
            InitializeUIDisplayText();
            allowTravelBack = false;
            Remove("OnKeyBoardInput"); Remove("OnKeyBoardSelection"); Remove("OnJoystickSelection"); Remove("MouseActivation_Container");
            if (row == 0) Add("OnKeyBoardInput");
            else { Remove("OnJoystickInput"); Add("OnJoystickInput"); Add("OnKBInputDuringJSInput"); }
            activeText = Text(row == 0 ? kbList : jsList, currentDataIndex);
            activeRow = row; activeColumn = currentDataIndex;
            activeText?.SetText("???");
            binding = true;
        }

        void EndBinding(bool fromJoystick)
        {
            if (fromJoystick) { Remove("OnJoystickInput"); Remove("OnKBInputDuringJSInput"); } else Remove("OnKeyBoardInput");
            Add("OnKeyBoardSelection"); Add("OnJoystickSelection"); Add("MouseActivation_Container");
            allowTravelBack = true;
            binding = false;
        }

        void OnKeyBoardSelection(string text, int id)
        {
            if (id == KeyF2) OnDefaultKeypress();
            else if (id == KeyEnter)
            {
                if (kbActive) StartBinding(0);
                else if (AttachedJoysticks != 0) StartBinding(1);
                else { InitializeUIDisplayText(); allowTravelBack = false; }   // the Lua turns travel back off before it finds no pad
            }
        }

        void OnKeyBoardInput(string text, int id, bool mapped)
        {
            //            esc            Q          E          3          alt        r-alt
            if (id == KeyEsc || id == 51 || id == 39 || id == 64 || id == 94 || id == 95 || mapped)
            {
                InitializeUIDisplayText();
                if (Cell(activeRow, activeColumn) == id || id == KeyEsc)
                {
                    activeText?.SetText(CellName(activeRow, activeColumn));
                    EndBinding(false);
                }
            }
            else Bind(id, false);
        }

        void OnKBInputDuringJSInput(int id)
        {
            if (id != KeyEsc) return;
            activeText?.SetText(CellName(activeRow, activeColumn));
            EndBinding(true);
        }

        void OnJoystickSelection(int id)
        {
            if (id != PadA) return;
            InitializeUIDisplayText();
            if (kbActive) StartBinding(0);
            else if (AttachedJoysticks != 0) StartBinding(1);
        }

        void OnJoystickInput(string text, int id, bool mapped)
        {
            if (mapped)
            {
                InitializeUIDisplayText();
                if (Cell(activeRow, activeColumn) == id) { activeText?.SetText(CellName(activeRow, activeColumn)); EndBinding(true); }
            }
            else Bind(id, true);
        }

        /// App.SetRemapTableByIndex( row, column, input ), then the cell's text = App.GetRemapTableCharByIndex( row, column ).
        void Bind(int id, bool fromJoystick)
        {
            int row = activeRow, column = activeColumn;
            var cells = table.Row(row);
            if (column < cells.Length) cells[column] = id;   // the game's cell (written below in its own table)
            var texts = table.Text(row); if (column < texts.Length) texts[column] = table.Name(id);
            pendingWrite = Chain(() => ReflexRemap.Set(row, column, id));
            activeText?.SetText(CellName(row, column));
            EndBinding(fromJoystick);
            needsSave = true;
        }

        int Cell(int row, int column) { var c = table?.Row(row); return c != null && column >= 0 && column < c.Length ? c[column] : -1; }
        string CellName(int row, int column) { var c = table?.Text(row); return c != null && column >= 0 && column < c.Length ? c[column] : ""; }

        /// OnActionListFocusedChanged: Up / Down on the ACTIONS list moves the row; a move of a binding list switches the column.
        void OnActionListFocusedChanged(FeNode container)
        {
            if (container == actionList && actionList != null) currentDataIndex = Mathf.Clamp(actionList.absIndex, 0, Rows - 1);
            else if (container != null) { kbActive = !kbActive; container.absIndex = 1; }
            if (actionList != null) actionList.focusIndex = currentDataIndex;
            if (kbList != null) kbList.focusIndex = kbActive ? currentDataIndex : -1;    // App.SetFocusedComponent / App.UnfocusListComponents
            if (jsList != null) jsList.focusIndex = kbActive ? -1 : currentDataIndex;
        }

        public override void OnNavigated() { }

        // late refresh after the defaults (the game's names of every cell)
        public void LateRefresh() { var r = refreshed; if (r != null) { refreshed = null; table = r; InitializeRemaps(); } }
        public override void OnAnimationComplete(string anim, FeNode scope) { }
    }
}

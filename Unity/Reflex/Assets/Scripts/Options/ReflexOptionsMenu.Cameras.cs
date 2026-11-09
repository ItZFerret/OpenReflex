// OPTIONS > CAMERA & HUD: the cameras the Camera action cycles and the player's own third-person presets (w-campresets; the model: ReflexCameraPresets,
// the view: OriginalWorld/ReflexCameraRig.cs).
//   CAMERA CYCLE    every camera with ON / OFF (the game's views, DYNAMIC FPV, the presets); the last ON one cannot be turned off; turning off the camera
//                   in use moves to the next ON one
//   MY CAMERAS      the presets (name, base view, in the cycle or not): A / click opens the editor; NEW CAMERA makes one from the camera in use (up to 16)
//   editor          NAME (typed in the box; the pad's A opens the on-screen keyboard: OuKeyboard), BASE VIEW, IN CYCLE, FIELD OF VIEW (scale / fixed),
//                   DISTANCE, HEIGHT, SIDE, PITCH, POSITION / ROTATION LAG, ROLL WITH BIKE, SHAKE (+ speed, landings, rough ground), USE THIS CAMERA,
//                   DUPLICATE, RESET, DELETE (asks first). Opened over a race (the pause page's OPTIONS, F4) the page moves aside and the race shows
//                   behind it with the preset on: every change shows at once and the shake moves (ReflexCameraPresets.Preview). Leaving it keeps the
//                   preset as the camera when it is ON in the cycle, else the camera in use before comes back. The values are saved on leaving.
using System;
using System.Collections.Generic;
using Reflex.Port.OpenFrontEnd;
using UnityEngine;
using UnityEngine.UIElements;

namespace Reflex.Port.Options
{
    public sealed partial class ReflexOptionsMenu
    {
        CameraPreset _camEdit;          // the preset the editor shows (null: the editor is not up)
        string _camPrev;                // the camera in use before a live preview
        bool _camLive;
        OuKeyboard _camKeyboard;
        Label _camBar;
        int _camTypeEnded = -10;

        /// The camera editor is up (tests), and the keyboard over it.
        public bool CameraEditorOpen => _camEdit != null;
        public bool CameraKeyboardOpen => _camKeyboard != null && _camKeyboard.IsOpen;

        string CameraEntryName(ReflexCameraPresets.Entry e)
        {
            if (e.Custom) return e.name;
            switch (e.builtin)
            {
                case 0: return Str("STR_OPT_DC_1", "VISUAL");
                case 1: return Str("STR_OPT_DC_2", "GAME PLAY");
                case 2: return Str("STR_OPT_DC_3", "USER");
                case 3: return Str("STR_OPT_DC_4", "1ST PERSON");
                default: return "DYNAMIC FPV";
            }
        }

        static string RowId(string id) => "cam-on-" + id.Replace(':', '-');

        /// The page rebuilt with the focus on the row called focus (its scroll follows).
        void ShowOptionsAt(int tab, string focus)
        {
            ShowOptions(tab);
            var el = focus != null ? _layer.Q<VisualElement>(focus) : null;
            if (el != null && el.userData is OuNavItem) _nav.Focus(el);
        }

        /// A row that opens something: the label, a value on the right, A / click.
        static VisualElement LinkRow(string label, string value, Action submit, VisualElement parent)
        {
            var row = OptUi.Div("mxa-row", parent);
            OptUi.Text(label, "mxa-row-label", row).enableRichText = false;
            var right = OptUi.Div("mxa-row-value", row);
            OptUi.Text(value, "mxa-value", right).enableRichText = false;
            OptUi.Text("›", "mxa-arrow", right);
            return OptUi.Item(row, submit);
        }

        // ------------------------------------------------------------------------------------------------ CAMERA & HUD's rows
        void CameraPresetRows(VisualElement panel)
        {
            Section(panel, "CAMERA CYCLE");
            foreach (var e in ReflexCameraPresets.Entries)
            {
                string id = e.id;
                var row = OptUi.Toggle(CameraEntryName(e), () => ReflexCameraPresets.Find(id)?.on ?? false, v =>
                {
                    if (!ReflexCameraPresets.SetOn(id, v)) { OptUi.Play("Locked"); return; }
                    ShowOptionsAt(ReflexOptions.CameraHud, RowId(id));
                }, panel);
                row.name = RowId(id);
                row.EnableInClassList("rx-cam-custom", e.Custom);
            }
            Note(panel, "Which cameras the Camera action, F7 and the pause page's CAMERA go through, in this order. At least one stays on. A camera turned off is skipped (turning off the one in use moves to the next). Ours.");

            Section(panel, "MY CAMERAS");
            foreach (var p in ReflexCameraPresets.Presets)
            {
                int pid = p.id;
                var row = LinkRow(p.name, ReflexCameraPresets.BaseNames[p.baseView] + (p.on ? "" : "  ·  OFF"), () => ShowCameraEditor(pid, null), panel);
                row.name = "cam-preset-" + pid;
            }
            var make = Named(OptUi.Button(ReflexCameraPresets.Full ? "NEW CAMERA  (" + ReflexCameraPresets.MaxPresets + " MADE: DELETE ONE FIRST)" : "NEW CAMERA", "mxa-link", () =>
            {
                var p = ReflexCameraPresets.Create();
                if (p == null) { OptUi.Play("Locked"); return; }
                ShowCameraEditor(p.id, "cam-name");
            }, panel), "cam-new");
            make.EnableInClassList("rx-cam-full", ReflexCameraPresets.Full);
            Note(panel, "Your own third-person cameras: one of the game's views moved, widened, softened and shaken to taste, with a name of your own that shows when you switch to it. NEW CAMERA starts from the camera in use. Open one to edit it; over a race the race shows behind the editor. Ours.");
            CameraShareImportRows(panel);   // (CAMERA SHARING: PASTE SHARE CODE, files)
        }

        // ------------------------------------------------------------------------------------------------ the editor
        void ShowCameraEditor(int id, string focus)
        {
            var p = ReflexCameraPresets.FindPreset(id);
            if (p == null) { ShowOptionsAt(ReflexOptions.CameraHud, "cam-new"); return; }
            CloseControls();
            CloseCameraKeyboard();
            var rt = ReflexClientLive.Runtime;
            bool live = rt && rt.Running;
            if (_camEdit != p)
            {
                if (_camEdit != null) EndCameraEditor();
                _camEdit = p;
                _camLive = live;
                if (live)
                {   // the race behind shows the preset while it is edited (the camera in use comes back on leaving unless this one is ON)
                    _camPrev = ReflexCameraPresets.CurrentId;
                    ReflexCameraPresets.Select(p.Id, false);
                    ReflexCameraPresets.Preview = p;
                }
            }
            _layer.Clear();
            _layer.AddToClassList("mxa-dim");
            _layer.AddToClassList("mxa-options-open");
            _layer.EnableInClassList("rx-cam-live", _camLive);
            var head = OptUi.Div("mxa-head", _layer);
            OptUi.Text("CAMERA PRESET", "mxa-screen-title", head).enableRichText = false;
            var body = OptUi.Div("ou-opt-body rx-cam-body", _layer);
            var page = OptUi.Div("mxa-panel mxa-options-page rx-cam-editor", body); page.name = "camera-editor";
            _camBar = OptUi.Text(p.name, "ou-bar-title ou-opt-page-bar ou-bar", page); _camBar.enableRichText = false;
            var side = OptUi.Div("mxa-panel ou-opt-side rx-cam-side", body);
            var sideTag = OptUi.Text("", "ou-pill ou-label", side);
            var sideTitle = OptUi.Text("", "ou-opt-side-title ou-display", side);
            var sideHelp = OptUi.Text("", "ou-opt-side-help", side);
            var sv = new ScrollView(ScrollViewMode.Vertical);
            sv.AddToClassList("mxa-options-scroll");
            sv.verticalScrollerVisibility = ScrollerVisibility.Auto;
            sv.horizontalScrollerVisibility = ScrollerVisibility.Hidden;
            page.Add(sv);
            var panel = sv.contentContainer;
            var CiF = Ci;
            void Set(Action change) { change(); ReflexCameraPresets.Touch(p); }

            var nameRow = CameraNameRow(panel, p);
            Note(panel, "The name that shows when you switch to this camera (up to " + ReflexCameraPresets.MaxName + " letters, digits, spaces, - and _). The pad's A opens the keyboard.");
            Named(OptUi.Spinner("BASE VIEW", () => ReflexCameraPresets.BaseNames[p.baseView], d =>
            {
                Set(() => p.baseView = ((p.baseView + d) % 3 + 3) % 3);
                if (_camLive) ReflexClientLive.ApplyView(p.baseView, false);   // (the game's view of the race behind: from its next frame)
            }, panel), "cam-base");
            Note(panel, "Which of the game's own third-person views this camera starts from; everything below moves that view.");
            Named(OptUi.Toggle("IN CAMERA CYCLE", () => p.on, v =>
            {
                if (!ReflexCameraPresets.SetOn(p.Id, v)) OptUi.Play("Locked");
                else if (_camLive && !v) { ReflexCameraPresets.Select(p.Id, false); ReflexCameraPresets.Preview = p; }   // (still previewed while it is edited)
            }, panel), "cam-on");
            Note(panel, "Whether the Camera action and F7 stop at this camera.");

            Section(panel, "VIEW");
            string[] fovModes = { "SCALE", "FIXED" };
            Named(OptUi.Spinner("FIELD OF VIEW", () => fovModes[p.fovMode], d => { Set(() => p.fovMode = 1 - p.fovMode); ShowCameraEditor(p.id, "cam-fov-mode"); }, panel), "cam-fov-mode");
            if (p.fovMode == 0)
                Named(OptUi.Slider("FOV SCALE", () => p.fovScale, v => Set(() => p.fovScale = Mathf.RoundToInt(v)), panel, ReflexCameraPresets.FovScaleMin, ReflexCameraPresets.FovScaleMax, 1f,
                    v => Mathf.RoundToInt(v) + "%" + (Mathf.RoundToInt(v) == 100 ? "  (GAME)" : "")), "cam-fov");
            else
                Named(OptUi.Slider("FOV DEGREES", () => p.fovDegrees, v => Set(() => p.fovDegrees = Mathf.RoundToInt(v)), panel, ReflexCameraPresets.FovDegreesMin, ReflexCameraPresets.FovDegreesMax, 1f,
                    v => Mathf.RoundToInt(v) + "°"), "cam-fov");
            Note(panel, "SCALE: the game view's own field of view made wider or narrower (100% = the game's; CHASE CAMERA FOV is for the game's views only). FIXED: always this many degrees across (as the first-person view's).");
            string Feet(float v) => v.ToString("+0.00;-0.00;0.00", CiF) + " FT";
            Named(OptUi.Slider("DISTANCE", () => p.distance, v => Set(() => p.distance = v), panel, ReflexCameraPresets.DistanceMin, ReflexCameraPresets.DistanceMax, 0.25f, Feet), "cam-distance");
            Note(panel, "Further back (+) or closer in (-) along the view.");
            Named(OptUi.Slider("HEIGHT", () => p.height, v => Set(() => p.height = v), panel, ReflexCameraPresets.HeightMin, ReflexCameraPresets.HeightMax, 0.25f, Feet), "cam-height");
            Note(panel, "Higher (+) or lower (-); the camera keeps looking at the bike.");
            Named(OptUi.Slider("SIDE", () => p.side, v => Set(() => p.side = v), panel, -ReflexCameraPresets.SideMax, ReflexCameraPresets.SideMax, 0.25f, Feet), "cam-side");
            Note(panel, "To the right (+) or left (-) of the bike, still looking at it.");
            Named(OptUi.Slider("PITCH", () => p.pitch, v => Set(() => p.pitch = v), panel, -ReflexCameraPresets.PitchMax, ReflexCameraPresets.PitchMax, 0.5f, v => v.ToString("+0.0;-0.0;0.0", CiF) + "°"), "cam-pitch");
            Note(panel, "Tilts the view down (+) or up (-).");

            Section(panel, "MOTION");
            string Secs(float v) => v < 0.005f ? "OFF" : v.ToString("0.00", CiF) + " S";
            Named(OptUi.Slider("POSITION LAG", () => p.posLag, v => Set(() => p.posLag = v), panel, 0f, ReflexCameraPresets.LagMax, 0.02f, Secs), "cam-poslag");
            Note(panel, "How long the camera's place around the bike takes to follow (OFF: locked to the game's view).");
            Named(OptUi.Slider("ROTATION LAG", () => p.rotLag, v => Set(() => p.rotLag = v), panel, 0f, ReflexCameraPresets.LagMax, 0.02f, Secs), "cam-rotlag");
            Note(panel, "How long the aim takes to follow: the bike drifts in the frame through turns and jumps.");
            Named(OptUi.Slider("ROLL WITH BIKE", () => p.rollFollow, v => Set(() => p.rollFollow = v), panel, 0f, 1f, 0.05f), "cam-roll");
            Note(panel, "0% keeps the horizon level; 100% leans with the bike (not through a crash).");

            Section(panel, "SHAKE");
            Named(OptUi.Slider("SHAKE", () => p.shake / 2f, v => Set(() => p.shake = v * 2f), panel, 0f, 1f, 0.025f, v => Mathf.RoundToInt(v * 200f) + "%"), "cam-shake");
            Note(panel, "All of the shake below at once (100% as set, up to 200%). The shake only moves the view, never the bike.");
            Named(OptUi.Slider("SPEED", () => p.shakeSpeed, v => Set(() => p.shakeSpeed = v), panel, 0f, 1f, 0.05f), "cam-shake-speed");
            Note(panel, "A fine buzz that builds from about 17 mph to 75 mph.");
            Named(OptUi.Slider("LANDINGS AND IMPACTS", () => p.shakeLanding, v => Set(() => p.shakeLanding = v), panel, 0f, 1f, 0.05f), "cam-shake-landing");
            Note(panel, "A jolt on landing (harder from higher), on a hard hit and when you crash, settling in about a second.");
            Named(OptUi.Slider("ROUGH GROUND", () => p.shakeRough, v => Set(() => p.shakeRough = v), panel, 0f, 1f, 0.05f), "cam-shake-rough");
            Note(panel, "Chatter from bumps and braking bumps while the wheels are down.");

            CameraShareEditorRows(panel, p);   // (CAMERA SHARING: ReflexOptionsMenu.CameraShare.cs)

            Section(panel, "THIS CAMERA");
            var use = Named(OptUi.Button(ReflexCameraPresets.CurrentId == p.Id && !_camLive ? "IN USE" : "USE THIS CAMERA", "mxa-link", () =>
            {
                _camPrev = p.Id;
                ReflexCameraPresets.Select(p.Id, true);
                if (!_camLive) ShowCameraEditor(p.id, "cam-use");
            }, panel), "cam-use");
            Note(panel, "Makes it the camera now and at the start of every event (also when it is not in the cycle).");
            Named(OptUi.Button(ReflexCameraPresets.Full ? "DUPLICATE  (FULL)" : "DUPLICATE", "mxa-link", () =>
            {
                ReflexCameraPresets.Changed(p);
                var copy = ReflexCameraPresets.Duplicate(p.id);
                if (copy == null) { OptUi.Play("Locked"); return; }
                ShowCameraEditor(copy.id, "cam-name");
            }, panel), "cam-duplicate");
            Named(OptUi.Button("RESET", "mxa-link", () =>
            {
                Set(() => { var n = new CameraPreset(); p.fovMode = n.fovMode; p.fovScale = n.fovScale; p.fovDegrees = n.fovDegrees; p.distance = p.height = p.side = p.pitch = 0f;
                                  p.posLag = p.rotLag = p.rollFollow = 0f; p.shake = n.shake; p.shakeSpeed = p.shakeLanding = p.shakeRough = 0f; });
                ShowCameraEditor(p.id, "cam-reset");
            }, panel), "cam-reset");
            Note(panel, "DUPLICATE makes a copy to try something on. RESET puts every value back to the game view's own (the name, base view and cycle stay).");
            var del = Named(OptUi.Button("DELETE", "mxa-link rx-cam-delete", null, panel), "cam-delete");
            ((OptNavItem)del.userData).submit = () => ConfirmCameraDelete(p, del);
            Note(panel, "Deletes this camera (it asks first).");

            CollapseNotes(panel, sideHelp, sideTitle, sideTag);
            _nav.OnTab = null;
            _nav.OnCancel = () => { if (CameraKeyboardOpen) return false; LeaveCameraEditor(); return true; };
            _nav.DefaultItem = () => nameRow;
            CameraEditorHints();
            var f = focus != null ? panel.Q<VisualElement>(focus) : null;
            _nav.Focus(f != null && f.userData is OuNavItem ? f : nameRow);
        }

        void CameraEditorHints() => OptUi.Hints(_hints, (OptUi.KeySubmit, Str("STR_SELECT", "SELECT")), (OptUi.KeyCancel, Str("STR_BACK", "BACK")));

        /// NAME: a box the keyboard types into (Enter keeps, Esc drops; leaving it keeps), the pad's A opens the on-screen keyboard instead.
        VisualElement CameraNameRow(VisualElement panel, CameraPreset p)
        {
            var row = OptUi.Div("mxa-row", panel);
            row.name = "cam-name";
            OptUi.Text("NAME", "mxa-row-label", row);
            var field = new TextField { value = p.name, maxLength = ReflexCameraPresets.MaxName, isDelayed = true };
            field.AddToClassList("mxa-path"); field.AddToClassList("mxa-rider-name");
            field.name = "cam-name-field";
            row.Add(field);
            bool typing = false;
            void Apply(string text)
            {
                string now = ReflexCameraPresets.Rename(p.id, text) ?? p.name;
                field.SetValueWithoutNotify(now);
                if (_camBar != null) _camBar.text = now;
            }
            void End(bool keep, bool refocus)
            {
                if (!typing) return;
                typing = false; _camTypeEnded = Time.frameCount;
                if (keep) Apply(field.text); else field.SetValueWithoutNotify(p.name);
                CameraEditorHints();
                if (refocus) _nav.Focus(row);
            }
            field.RegisterCallback<FocusInEvent>(_ =>
            {
                if (typing) return;
                typing = true;
                OptUi.Hints(_hints, (OptUi.KeySubmit, "KEEP"), (OptUi.KeyCancel, "CANCEL"));
            });
            field.RegisterCallback<FocusOutEvent>(_ => End(true, false));
            field.RegisterCallback<KeyDownEvent>(e =>
            {
                if (e.keyCode == KeyCode.Return || e.keyCode == KeyCode.KeypadEnter) { End(true, true); e.StopPropagation(); }
                else if (e.keyCode == KeyCode.Escape) { End(false, true); e.StopPropagation(); }
            }, TrickleDown.TrickleDown);
            field.RegisterCallback<ClickEvent>(e => e.StopPropagation());   // (the click places the caret; the row's own click would take the focus back)
            OptUi.Item(row, () =>
            {
                if (Time.frameCount - _camTypeEnded <= 1) return;   // (the Enter that ended the typing)
                if (OuMenuInput.GamepadLast) { End(true, false); OpenCameraKeyboard(p, row, Apply); }
                else field.Q("unity-text-input")?.Focus();
            });
            return row;
        }

        void OpenCameraKeyboard(CameraPreset p, VisualElement row, Action<string> apply)
        {
            CloseCameraKeyboard();
            OuKeyboard.EnsureStyles(_layer);
            OuKeyboard k = null;
            k = OuKeyboard.Open(_layer, _nav, _hints, "CAMERA NAME", "UP TO " + ReflexCameraPresets.MaxName + " LETTERS, DIGITS, SPACES, - AND _", p.name, ReflexCameraPresets.MaxName,
                                false, true, row, text => { apply(text); CameraEditorHints(); }, CameraEditorHints, () => { if (_camKeyboard == k) _camKeyboard = null; }, OptUi.Play);
            if (k.IsOpen) _camKeyboard = k;
        }

        void CloseCameraKeyboard() { _camKeyboard?.Release(); _camKeyboard = null; }

        /// DELETE asks first (CANCEL focused; Back is CANCEL).
        void ConfirmCameraDelete(CameraPreset p, VisualElement returnTo)
        {
            OuKeyboard.EnsureStyles(_layer);
            var root = OptUi.Div("ou-gdialog-root p2-popup", _layer); root.name = "cam-delete-popup";
            var card = OuWidgets.Dialog(root, "p2");
            OptUi.Text("DELETE CAMERA", "ou-dialog-title ou-display", card).enableRichText = false;
            OptUi.Text("Delete " + p.name + "? It leaves the camera cycle and cannot be brought back.", "ou-dialog-text p2-popup-text", card).enableRichText = false;
            var buttons = OptUi.Div("p2-popup-buttons", card);
            var savedCancel = _nav.OnCancel;
            void Close() { _nav.Modal = null; _nav.OnCancel = savedCancel; root.RemoveFromHierarchy(); CameraEditorHints(); }
            var yes = Named(OptUi.Button("DELETE", "mxa-main-button p2-popup-button", () =>
            {
                Close();
                bool live = _camLive; string prev = _camPrev;
                ReflexCameraPresets.Preview = null;
                _camEdit = null; _camLive = false;
                ReflexCameraPresets.Delete(p.id);
                if (live && prev != null && prev != p.Id && ReflexCameraPresets.Find(prev) != null) ReflexCameraPresets.Select(prev, true);
                _layer.RemoveFromClassList("rx-cam-live");
                ShowOptionsAt(ReflexOptions.CameraHud, "cam-new");
            }, buttons), "cam-delete-yes");
            var no = Named(OptUi.Button("CANCEL", "mxa-main-button p2-popup-button", () => { Close(); _nav.Focus(returnTo); }, buttons), "cam-delete-no");
            OuWidgets.LinkRow(new List<VisualElement> { yes, no });
            OptUi.Hints(_hints, (OptUi.KeySubmit, Str("STR_SELECT", "SELECT")), (OptUi.KeyCancel, "CANCEL"));
            _nav.OnCancel = () => { Close(); _nav.Focus(returnTo); return true; };
            _nav.Modal = card;
            _nav.Focus(no);
        }

        /// Back from the editor: saved, the preview ended, CAMERA & HUD with the preset's row focused.
        void LeaveCameraEditor()
        {
            int id = _camEdit != null ? _camEdit.id : 0;
            EndCameraEditor();
            ShowOptionsAt(ReflexOptions.CameraHud, "cam-preset-" + id);
        }

        /// The editor goes (Back, another page, the options closing): the preset saved; a live preview keeps the preset when it is ON in the cycle,
        /// else gives back the camera that was in use.
        void EndCameraEditor()
        {
            CloseCameraKeyboard();
            var p = _camEdit;
            _camEdit = null;
            _layer.RemoveFromClassList("rx-cam-live");
            if (p == null) return;
            ReflexCameraPresets.Changed(p);
            if (!_camLive) return;
            _camLive = false;
            ReflexCameraPresets.Preview = null;
            bool exists = ReflexCameraPresets.FindPreset(p.id) != null;
            if (exists && (p.on || _camPrev == p.Id)) ReflexCameraPresets.Select(p.Id, true);
            else if (_camPrev != null && ReflexCameraPresets.Find(_camPrev) != null) ReflexCameraPresets.Select(_camPrev, true);
        }
    }
}

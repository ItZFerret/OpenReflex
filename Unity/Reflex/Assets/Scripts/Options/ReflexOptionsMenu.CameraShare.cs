// CAMERA SHARING on the options pages (w-camshare; the model: ReflexCameraShare.cs, the format: CameraShareCodec.cs; the UX of PHYSICS STUDIO's CONFIGS page).
//   camera editor        SHARE: AUTHOR (typed; the pad's A opens the on-screen keyboard), COPY SHARE CODE (ORCAM1:... onto the clipboard), EXPORT .JSON FILE
//   CAMERA & HUD         SHARING (after MY CAMERAS): PASTE SHARE CODE (a camera becomes a new preset ON in the cycle and its editor opens on USE THIS
//                        CAMERA; an FPV setup goes to FIRST-PERSON SETUPS and its page opens on USE THIS SETUP), IMPORT .JSON FILES (Cameras/Import),
//                        OPEN CAMERA FOLDER
//   FIRST-PERSON VIEW    FIRST-PERSON SETUPS ›, COPY SHARE CODE (the settings in use), PASTE SHARE CODE
//   FIRST-PERSON SETUPS  SAVE CURRENT AS NEW SETUP, COPY / PASTE SHARE CODE, the saved setups (IN USE marks the one the settings equal), IMPORT .JSON
//                        FILES, OPEN CAMERA FOLDER
//   a setup              NAME, AUTHOR, USE THIS SETUP (the FPV settings become its values: a running FPV camera at once), SAVE CURRENT SETTINGS INTO IT
//                        (press twice), COPY SHARE CODE, EXPORT .JSON FILE, DELETE (asks first), its values
// Pad / keys: up / down, A / Enter, B / Esc back (a setup -> the setups -> CAMERA & HUD). A status line under the share rows says what happened.
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using Reflex.Port.OpenFrontEnd;
using UnityEngine;
using UnityEngine.UIElements;

namespace Reflex.Port.Options
{
    public sealed partial class ReflexOptionsMenu
    {
        string _shareStatus;   // the status the next share page shows (set before a page is rebuilt)
        string _shareArmed;    // a press-twice action waiting for its second press (on the page shown)
        string _shareArmNext;  // ... armed for the page being built

        /// The FIRST-PERSON SETUPS page / a setup's page is up (tests).
        public bool FpvSetupsOpen => _layer.Q<VisualElement>("fpv-setups-page") != null;
        public bool FpvSetupOpen => _layer.Q<VisualElement>("fpv-setup-page") != null;

        /// A status line kept under the rows (not collapsed into the help); it takes the pending status.
        Label ShareStatus(VisualElement panel, string name)
        {
            var l = Keep(panel, _shareStatus ?? "");
            l.name = name;
            l.AddToClassList("rx-share-status");
            l.style.display = string.IsNullOrEmpty(_shareStatus) ? DisplayStyle.None : DisplayStyle.Flex;
            _shareStatus = null;
            return l;
        }
        static void SetStatus(Label l, string text)
        {
            if (l == null) return;
            l.text = text ?? "";
            l.style.display = string.IsNullOrEmpty(text) ? DisplayStyle.None : DisplayStyle.Flex;
        }

        // ------------------------------------------------------------------------------------------------ the camera editor's SHARE
        void CameraShareEditorRows(VisualElement panel, CameraPreset p)
        {
            Section(panel, "SHARE");
            ShareTextRow(panel, "AUTHOR", "cam-author", () => p.author, text =>
            {
                p.author = CameraConfig.CleanText(text, CameraConfig.MaxAuthor).ToUpperInvariant();
                if (p.author.Length > 0) ReflexCameraShare.DefaultAuthor = p.author;
                ReflexCameraPresets.Touch(p);
                return p.author;
            }, CameraConfig.MaxAuthor, "AUTHOR");
            Note(panel, "Your name on the share code (up to " + CameraConfig.MaxAuthor + " letters). The pad's A opens the keyboard. Ours.");
            Label status = null;
            Named(OptUi.Button("COPY SHARE CODE", "mxa-link", () =>
            {
                try { ReflexCameraPresets.Touch(p); string code = ReflexCameraShare.CopyShareCode(ReflexCameraShare.ConfigOf(p)); SetStatus(status, "SHARE CODE COPIED (" + code.Length + " CHARACTERS): PASTE IT IN CHAT, A FORUM, A DISCORD MESSAGE ..."); OptUi.Play("Select"); }
                catch (Exception e) { SetStatus(status, "NOT COPIED: " + e.Message.ToUpperInvariant()); OptUi.Play("Locked"); }
            }, panel), "cam-share");
            Note(panel, "One line of text (" + CameraConfig.SharePrefix + "...) that carries this camera: its name, author and every value above. The other player uses PASTE SHARE CODE on CAMERA & HUD. Ours.");
            Named(OptUi.Button("EXPORT .JSON FILE", "mxa-link", () =>
            {
                try { ReflexCameraPresets.Touch(p); string path = ReflexCameraShare.Export(ReflexCameraShare.ConfigOf(p)); SetStatus(status, "EXPORTED " + Path.GetFileName(path).ToUpperInvariant() + " (CAMERAS/EXPORT)"); ReflexCameraShare.Reveal(ReflexCameraShare.ExportFolder); }
                catch (Exception e) { SetStatus(status, "NOT EXPORTED: " + e.Message.ToUpperInvariant()); OptUi.Play("Locked"); }
            }, panel), "cam-export");
            Note(panel, "Writes this camera as a .json file into " + ReflexCameraShare.ExportFolder + " and opens that folder. Ours.");
            status = ShareStatus(panel, "cam-share-status");
        }

        // ------------------------------------------------------------------------------------------------ CAMERA & HUD's SHARING
        void CameraShareImportRows(VisualElement panel)
        {
            Section(panel, "SHARING");
            Label status = null;
            Named(OptUi.Button("PASTE SHARE CODE", "mxa-link", () => PasteCameraCode(status), panel), "cam-paste");
            Note(panel, "Reads a camera share code (" + CameraConfig.SharePrefix + "..., anywhere in the copied text) from the clipboard: a camera becomes one of MY CAMERAS (ON in the cycle), first-person settings a FIRST-PERSON SETUP. Nothing changes until you USE it. Ours.");
            Named(OptUi.Button("IMPORT .JSON FILES", "mxa-link", () =>
            {
                try { _shareStatus = ReflexCameraShare.ImportFiles(); }
                catch (Exception e) { _shareStatus = "IMPORT FAILED: " + e.Message.ToUpperInvariant(); }
                if (_shareStatus.StartsWith("PUT ", StringComparison.Ordinal)) ReflexCameraShare.Reveal(ReflexCameraShare.ImportFolder);
                ShowOptionsAt(ReflexOptions.CameraHud, "cam-import");
            }, panel), "cam-import");
            Note(panel, "Every .json camera file in " + ReflexCameraShare.ImportFolder + " is added (checked and clamped first). Ours.");
            Named(OptUi.Button("OPEN CAMERA FOLDER", "mxa-link", () => ReflexCameraShare.Reveal(ReflexCameraShare.Folder), panel), "cam-folder");
            Note(panel, "Your first-person setups are .json files in " + ReflexCameraShare.Folder + " (Export and Import beside them). Ours.");
            status = ShareStatus(panel, "cam-paste-status");
        }

        /// PASTE SHARE CODE (every page that has it): imported, then the new camera's editor or the new setup's page; a failure stays in status.
        void PasteCameraCode(Label status)
        {
            var got = ReflexCameraShare.FromClipboard(out string err);
            if (got == null) { SetStatus(status, err); OptUi.Play("Locked"); return; }
            var r = ReflexCameraShare.Import(got);
            string by = string.IsNullOrEmpty(got.author) ? "" : " BY " + got.author.ToUpperInvariant();
            if (!r.Any) { SetStatus(status, r.Message("PASTED")); OptUi.Play("Locked"); return; }
            _shareStatus = r.Message("PASTED") + by + (r.preset != null ? ". USE THIS CAMERA TO RIDE WITH IT." : ". USE THIS SETUP TO RIDE WITH IT.");
            OptUi.Play("Select");
            if (r.preset != null) ShowCameraEditor(r.preset.id, "cam-use");
            else ShowFpvSetup(r.setup.id, "fpv-use");
        }

        // ------------------------------------------------------------------------------------------------ FIRST-PERSON VIEW's rows
        void FpvShareRows(VisualElement panel)
        {
            Label status = null;
            Named(LinkRow("FIRST-PERSON SETUPS", FpvSetupsValue(), () => ShowFpvSetups(null), panel), "fpv-setups");
            Note(panel, "Your saved first-person setups: save the settings above under a name, switch between them, share them. Ours.");
            Named(OptUi.Button("COPY SHARE CODE", "mxa-link", () =>
            {
                try
                {
                    var inUse = ReflexCameraShare.Setups().FirstOrDefault(ReflexCameraShare.FpvInUse);
                    var c = ReflexCameraShare.ConfigOfFpv(inUse?.name ?? "MY FPV", inUse?.author ?? ReflexCameraShare.DefaultAuthor);
                    string code = ReflexCameraShare.CopyShareCode(c);
                    SetStatus(status, "SHARE CODE COPIED (" + code.Length + " CHARACTERS): THE FIRST-PERSON SETTINGS ABOVE");
                    OptUi.Play("Select");
                }
                catch (Exception e) { SetStatus(status, "NOT COPIED: " + e.Message.ToUpperInvariant()); OptUi.Play("Locked"); }
            }, panel), "fpv-share");
            Note(panel, "One line of text (" + CameraConfig.SharePrefix + "...) that carries the first-person settings above. Ours.");
            Named(OptUi.Button("PASTE SHARE CODE", "mxa-link", () => PasteCameraCode(status), panel), "fpv-paste");
            Note(panel, "Reads a camera share code from the clipboard into FIRST-PERSON SETUPS (or MY CAMERAS); USE THIS SETUP then applies it. Ours.");
            status = ShareStatus(panel, "fpv-share-status");
        }

        static string FpvSetupsValue()
        {
            var list = ReflexCameraShare.Setups();
            var inUse = list.FirstOrDefault(ReflexCameraShare.FpvInUse);
            return inUse != null ? inUse.name + "  ·  IN USE" : list.Count == 0 ? "NONE SAVED" : list.Count + " SAVED";
        }

        // ------------------------------------------------------------------------------------------------ the share pages' frame
        /// A page of its own over the options (the camera editor's frame): the title, the bar, the scrolling rows and the help card.
        VisualElement SharePage(string title, string bar, string pageName, out Label barLabel, out Label help, out Label helpTitle, out Label tag)
        {
            CloseControls();
            CloseCameraKeyboard();
            EndCameraEditor();
            _shareArmed = _shareArmNext; _shareArmNext = null;
            _layer.Clear();
            _layer.AddToClassList("mxa-dim");
            _layer.AddToClassList("mxa-options-open");
            _layer.RemoveFromClassList("rx-cam-live");
            var head = OptUi.Div("mxa-head", _layer);
            OptUi.Text(title, "mxa-screen-title", head).enableRichText = false;
            var body = OptUi.Div("ou-opt-body rx-cam-body", _layer);
            var page = OptUi.Div("mxa-panel mxa-options-page rx-share-page", body); page.name = pageName;
            barLabel = OptUi.Text(bar, "ou-bar-title ou-opt-page-bar ou-bar", page); barLabel.enableRichText = false;
            var side = OptUi.Div("mxa-panel ou-opt-side rx-cam-side", body);
            tag = OptUi.Text("", "ou-pill ou-label", side);
            helpTitle = OptUi.Text("", "ou-opt-side-title ou-display", side);
            help = OptUi.Text("", "ou-opt-side-help", side);
            var sv = new ScrollView(ScrollViewMode.Vertical);
            sv.AddToClassList("mxa-options-scroll");
            sv.verticalScrollerVisibility = ScrollerVisibility.Auto;
            sv.horizontalScrollerVisibility = ScrollerVisibility.Hidden;
            page.Add(sv);
            return sv.contentContainer;
        }

        void FinishSharePage(VisualElement panel, Label help, Label helpTitle, Label tag, VisualElement first, string focus, Func<bool> back)
        {
            CollapseNotes(panel, help, helpTitle, tag);
            _nav.OnTab = null;
            _nav.OnCancel = () => { if (CameraKeyboardOpen) return false; return back(); };
            _nav.DefaultItem = () => first;
            CameraEditorHints();
            var f = focus != null ? panel.Q<VisualElement>(focus) : null;
            if (f != null && f.userData is OuNavItem) _nav.Focus(f);
            else if (first != null) _nav.Focus(first);
        }

        // ------------------------------------------------------------------------------------------------ FIRST-PERSON SETUPS
        public void ShowFpvSetups(string focus)
        {
            var panel = SharePage("FIRST-PERSON SETUPS", "FIRST-PERSON SETUPS", "fpv-setups-page", out _, out var help, out var helpTitle, out var tag);
            Label status = null;

            Section(panel, "THE SETTINGS IN USE");
            var first = Named(OptUi.Button("SAVE CURRENT AS NEW SETUP", "mxa-link", () =>
            {
                try { var c = ReflexCameraShare.SaveCurrentFpv(); _shareStatus = "SAVED AS " + c.name + ": GIVE IT A NAME OF YOUR OWN WITH NAME"; ShowFpvSetup(c.id, "fpv-name"); }
                catch (Exception e) { SetStatus(status, "NOT SAVED: " + e.Message.ToUpperInvariant()); OptUi.Play("Locked"); }
            }, panel), "fpv-save-new");
            Note(panel, "Keeps the first-person settings in use now as a setup of yours (a .json file in " + ReflexCameraShare.Folder + "). Ours.");
            Named(OptUi.Button("COPY SHARE CODE", "mxa-link", () =>
            {
                try
                {
                    var inUse = ReflexCameraShare.Setups().FirstOrDefault(ReflexCameraShare.FpvInUse);
                    string code = ReflexCameraShare.CopyShareCode(ReflexCameraShare.ConfigOfFpv(inUse?.name ?? "MY FPV", inUse?.author ?? ReflexCameraShare.DefaultAuthor));
                    SetStatus(status, "SHARE CODE COPIED (" + code.Length + " CHARACTERS)"); OptUi.Play("Select");
                }
                catch (Exception e) { SetStatus(status, "NOT COPIED: " + e.Message.ToUpperInvariant()); OptUi.Play("Locked"); }
            }, panel), "fpv-list-share");
            Note(panel, "The settings in use as one line of text (" + CameraConfig.SharePrefix + "...). Ours.");
            Named(OptUi.Button("PASTE SHARE CODE", "mxa-link", () => PasteCameraCode(status), panel), "fpv-list-paste");
            Note(panel, "Reads a camera share code from the clipboard into your setups (a chase camera into MY CAMERAS). Ours.");
            status = ShareStatus(panel, "fpv-setups-status");

            Section(panel, "YOUR SETUPS");
            var list = ReflexCameraShare.Setups();
            foreach (var c in list)
            {
                string id = c.id;
                bool inUse = ReflexCameraShare.FpvInUse(c);
                var row = Named(LinkRow(c.name, inUse ? "IN USE" : string.IsNullOrEmpty(c.author) ? "" : "BY " + c.author.ToUpperInvariant(), () => ShowFpvSetup(id, null), panel), "fpv-setup-" + id);
                row.EnableInClassList("rx-share-inuse", inUse);
                Note(panel, (inUse ? "The first-person view has these settings now. " : "") + (string.IsNullOrEmpty(c.author) ? "" : "By " + c.author + ". ") + c.id + ".json. Open it to use, rename, share or delete it. Ours.");
            }
            if (list.Count == 0) Keep(panel, "No setups saved yet: SAVE CURRENT AS NEW SETUP keeps the settings in use, PASTE SHARE CODE adds a friend's.");

            Section(panel, "FILES");
            Named(OptUi.Button("IMPORT .JSON FILES", "mxa-link", () =>
            {
                try { _shareStatus = ReflexCameraShare.ImportFiles(); }
                catch (Exception e) { _shareStatus = "IMPORT FAILED: " + e.Message.ToUpperInvariant(); }
                if (_shareStatus.StartsWith("PUT ", StringComparison.Ordinal)) ReflexCameraShare.Reveal(ReflexCameraShare.ImportFolder);
                ShowFpvSetups("fpv-import");
            }, panel), "fpv-import");
            Note(panel, "Every .json camera file in " + ReflexCameraShare.ImportFolder + " is added (checked and clamped first). Ours.");
            Named(OptUi.Button("OPEN CAMERA FOLDER", "mxa-link", () => ReflexCameraShare.Reveal(ReflexCameraShare.Folder), panel), "fpv-folder");
            Note(panel, "The setups are .json files in " + ReflexCameraShare.Folder + ". Ours.");

            FinishSharePage(panel, help, helpTitle, tag, first, focus, () => { ShowOptionsAt(ReflexOptions.CameraHud, "fpv-setups"); return true; });
        }

        // ------------------------------------------------------------------------------------------------ one setup
        public void ShowFpvSetup(string id, string focus)
        {
            var c = ReflexCameraShare.FindSetup(id);
            if (c == null) { ShowFpvSetups(null); return; }
            var panel = SharePage("FIRST-PERSON SETUP", c.name, "fpv-setup-page", out var bar, out var help, out var helpTitle, out var tag);
            Label status = null;
            void Save() { try { ReflexCameraShare.SaveSetup(c); } catch (Exception e) { SetStatus(status, "NOT SAVED: " + e.Message.ToUpperInvariant()); } }

            var first = ShareTextRow(panel, "NAME", "fpv-name", () => c.name, text =>
            {
                string n = ReflexCameraShare.SetupName(text);
                if (ReflexCameraPresets.CleanName(text).Length > 0 && n != c.name) { c.name = n; Save(); }
                bar.text = c.name;
                return c.name;
            }, ReflexCameraPresets.MaxName, "SETUP NAME");
            Note(panel, "The setup's name (up to " + ReflexCameraPresets.MaxName + " letters, digits, spaces, - and _). The pad's A opens the keyboard. Ours.");
            ShareTextRow(panel, "AUTHOR", "fpv-author", () => c.author, text =>
            {
                c.author = CameraConfig.CleanText(text, CameraConfig.MaxAuthor).ToUpperInvariant();
                if (c.author.Length > 0) ReflexCameraShare.DefaultAuthor = c.author;
                Save();
                return c.author;
            }, CameraConfig.MaxAuthor, "AUTHOR");
            Note(panel, "Your name on the share code. Ours.");

            Section(panel, "THIS SETUP");
            bool inUse = ReflexCameraShare.FpvInUse(c);
            Named(OptUi.Button(inUse ? "IN USE" : "USE THIS SETUP", "mxa-link", () =>
            {
                if (!ReflexCameraShare.ApplyFpv(c)) { SetStatus(status, "THE FIRST-PERSON VIEW CANNOT TAKE IT"); OptUi.Play("Locked"); return; }
                _shareStatus = "IN USE: THE FIRST-PERSON VIEW HAS THESE SETTINGS NOW" + (ReflexClientLive.Runtime ? " (THE RACE TOO)" : "");
                ShowFpvSetup(c.id, "fpv-use");
            }, panel), "fpv-use");
            Note(panel, "Makes these the first-person settings (OPTIONS > CAMERA & HUD > FIRST-PERSON VIEW): at once, in a race too. Ours.");
            Named(OptUi.Button(_shareArmed == "overwrite:" + c.id ? "PRESS AGAIN TO REPLACE ITS VALUES" : "SAVE CURRENT SETTINGS INTO IT", "mxa-link", () =>
            {
                if (_shareArmed != "overwrite:" + c.id)
                {
                    _shareStatus = "PRESS AGAIN TO REPLACE " + c.name + "'S VALUES WITH THE SETTINGS IN USE";
                    _shareArmNext = "overwrite:" + c.id;
                    ShowFpvSetup(c.id, "fpv-overwrite");
                    return;
                }
                _shareArmed = null;
                try { ReflexCameraShare.OverwriteWithCurrent(c); _shareStatus = "SAVED: " + c.name + " HAS THE SETTINGS IN USE"; }
                catch (Exception e) { _shareStatus = "NOT SAVED: " + e.Message.ToUpperInvariant(); }
                ShowFpvSetup(c.id, "fpv-overwrite");
            }, panel), "fpv-overwrite");
            Note(panel, "Replaces this setup's values with the first-person settings in use now (press twice). Ours.");

            Section(panel, "SHARE");
            Named(OptUi.Button("COPY SHARE CODE", "mxa-link", () =>
            {
                try { string code = ReflexCameraShare.CopyShareCode(c); SetStatus(status, "SHARE CODE COPIED (" + code.Length + " CHARACTERS): PASTE IT IN CHAT, A FORUM, A DISCORD MESSAGE ..."); OptUi.Play("Select"); }
                catch (Exception e) { SetStatus(status, "NOT COPIED: " + e.Message.ToUpperInvariant()); OptUi.Play("Locked"); }
            }, panel), "fpv-setup-share");
            Note(panel, "One line of text (" + CameraConfig.SharePrefix + "...) that carries this setup: its name, author and values. Ours.");
            Named(OptUi.Button("EXPORT .JSON FILE", "mxa-link", () =>
            {
                try { string path = ReflexCameraShare.Export(c); SetStatus(status, "EXPORTED " + Path.GetFileName(path).ToUpperInvariant() + " (CAMERAS/EXPORT)"); ReflexCameraShare.Reveal(ReflexCameraShare.ExportFolder); }
                catch (Exception e) { SetStatus(status, "NOT EXPORTED: " + e.Message.ToUpperInvariant()); OptUi.Play("Locked"); }
            }, panel), "fpv-export");
            Note(panel, "Writes this setup as a .json file into " + ReflexCameraShare.ExportFolder + ". Ours.");
            var del = Named(OptUi.Button("DELETE", "mxa-link rx-cam-delete", null, panel), "fpv-delete");
            ((OptNavItem)del.userData).submit = () => ConfirmShare("DELETE SETUP", "Delete " + c.name + "? Its file goes and it cannot be brought back (the settings in use stay as they are).", "fpv-delete-popup", del, () =>
            {
                try { ReflexCameraShare.DeleteSetup(c); _shareStatus = "DELETED " + c.name; }
                catch (Exception e) { _shareStatus = "NOT DELETED: " + e.Message.ToUpperInvariant(); }
                ShowFpvSetups("fpv-save-new");
            });
            Note(panel, "Deletes this setup (it asks first). Ours.");
            status = ShareStatus(panel, "fpv-setup-status");

            Section(panel, "ITS VALUES");
            var sum = Keep(panel, ReflexCameraShare.Summary(c.Section(CameraShare.FpvId)));
            sum.name = "fpv-summary";
            string note = ReflexCameraShare.VersionNote(c);
            if (note.Length > 0) Keep(panel, note);

            FinishSharePage(panel, help, helpTitle, tag, first, focus, () => { ShowFpvSetups("fpv-setup-" + c.id); return true; });
        }

        /// A question before something that cannot be undone (CANCEL focused; Back is CANCEL).
        void ConfirmShare(string title, string text, string popupName, VisualElement returnTo, Action yes)
        {
            OuKeyboard.EnsureStyles(_layer);
            var root = OptUi.Div("ou-gdialog-root p2-popup", _layer); root.name = popupName;
            var card = OuWidgets.Dialog(root, "p2");
            OptUi.Text(title, "ou-dialog-title ou-display", card).enableRichText = false;
            OptUi.Text(text, "ou-dialog-text p2-popup-text", card).enableRichText = false;
            var buttons = OptUi.Div("p2-popup-buttons", card);
            var savedCancel = _nav.OnCancel;
            void Close() { _nav.Modal = null; _nav.OnCancel = savedCancel; root.RemoveFromHierarchy(); CameraEditorHints(); }
            var ok = Named(OptUi.Button(title.Split(' ')[0], "mxa-main-button p2-popup-button", () => { Close(); yes(); }, buttons), popupName + "-yes");
            var no = Named(OptUi.Button("CANCEL", "mxa-main-button p2-popup-button", () => { Close(); _nav.Focus(returnTo); }, buttons), popupName + "-no");
            OuWidgets.LinkRow(new List<VisualElement> { ok, no });
            OptUi.Hints(_hints, (OptUi.KeySubmit, Str("STR_SELECT", "SELECT")), (OptUi.KeyCancel, "CANCEL"));
            _nav.OnCancel = () => { Close(); _nav.Focus(returnTo); return true; };
            _nav.Modal = card;
            _nav.Focus(no);
        }

        // ------------------------------------------------------------------------------------------------ a typed text row
        /// A text row (the camera NAME row's behaviour): the keyboard types in the box (Enter keeps, Esc drops; leaving it keeps), the pad's A opens the
        /// on-screen keyboard. apply stores the text and returns what is kept.
        VisualElement ShareTextRow(VisualElement panel, string label, string rowName, Func<string> get, Func<string, string> apply, int max, string keyboardTitle)
        {
            var row = OptUi.Div("mxa-row", panel);
            row.name = rowName;
            OptUi.Text(label, "mxa-row-label", row);
            var field = new TextField { value = get() ?? "", maxLength = max, isDelayed = true };
            field.AddToClassList("mxa-path"); field.AddToClassList("mxa-rider-name");
            field.name = rowName + "-field";
            row.Add(field);
            bool typing = false;
            void Apply(string text) { field.SetValueWithoutNotify(apply(text) ?? get() ?? ""); }
            void End(bool keep, bool refocus)
            {
                if (!typing) return;
                typing = false; _camTypeEnded = Time.frameCount;
                if (keep) Apply(field.text); else field.SetValueWithoutNotify(get() ?? "");
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
            field.RegisterCallback<ClickEvent>(e => e.StopPropagation());
            OptUi.Item(row, () =>
            {
                if (Time.frameCount - _camTypeEnded <= 1) return;
                if (OuMenuInput.GamepadLast) { End(true, false); OpenShareKeyboard(keyboardTitle, max, get() ?? "", row, Apply); }
                else field.Q("unity-text-input")?.Focus();
            });
            return row;
        }

        void OpenShareKeyboard(string title, int max, string start, VisualElement row, Action<string> apply)
        {
            CloseCameraKeyboard();
            OuKeyboard.EnsureStyles(_layer);
            OuKeyboard k = null;
            k = OuKeyboard.Open(_layer, _nav, _hints, title, "UP TO " + max + " LETTERS, DIGITS, SPACES, - AND _", start, max,
                                false, true, row, text => { apply(text); CameraEditorHints(); }, CameraEditorHints, () => { if (_camKeyboard == k) _camKeyboard = null; }, OptUi.Play);
            if (k.IsOpen) _camKeyboard = k;
        }
    }
}

// PHYSICS STUDIO (the hub's PHYSICS STUDIO chip; after Open Unleashed's MxaMainMenu.PhysicsStudio / MxaPhysicsStudioPanel): every
// value of the vehicle parameter files and the terrain grip table, edited into a config (PhysicsStudio.cs), saved, shared and used.
//   CONFIGS   the built-in presets and the player's library: load one into the editor, name / author / notes, SAVE, SAVE AS COPY,
//             USE FOR MY RIDES (or ORIGINAL PHYSICS), COPY SHARE CODE, PASTE SHARE CODE, EXPORT / IMPORT .JSON FILES, DELETE
//   VEHICLE   the nine vehicle classes, by group or by search: a bar + the number per value, left / right step it, ENTER types an
//             exact value, the alt button (Y / F) puts back the file's value; changed values are marked; reset the group / vehicle / all
//   TERRAIN   the six grip materials (00461680) and the four drag materials (00461760), and who the grip table applies to
// The side panel names the focused value: the file's value, the bounds, the unit, the scope (only the local player's vehicle unless the
// config says otherwise). The config applies from the next event (the core sets it at the parameter files' load point); a changed config
// keeps no offline records for that event, and the race HUD / pause show PHYSICS MODIFIED.
using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using Reflex.Port.Options;
using Reflex.Port.RxCore;
using UnityEngine;
using UnityEngine.UIElements;

namespace Reflex.Port.OpenFrontEnd
{
    public sealed partial class OpenShell
    {
        static readonly string[] PsTabs = { "CONFIGS", "VEHICLE", "TERRAIN" };
        const string PsClassKey = "Physics.StudioClass", PsGroupKey = "Physics.StudioGroup";
        static readonly Color PsAccent = new Color(0.886f, 0.675f, 0.251f);

        int _psTab, _psGroup, _psClass = -1, _psBackArmed;
        string _psSearch = "", _psArmed;
        bool _psDirty;
        PhysicsConfig _psWork;
        PhysicsCatalog _psCat;
        VisualElement _psRows, _psSide, _psHelp;
        ScrollView _psScroll;
        Label _psStatus, _psSub, _psHelpTitle, _psHelpText, _psActive;
        TextField _psField;
        Action<string> _psCommit;
        readonly List<Action> _psRefresh = new List<Action>();
        readonly Dictionary<VisualElement, Action> _psResetOf = new Dictionary<VisualElement, Action>();

        public void ShowPhysicsStudio() => ShowPhysicsStudio(_psTab);

        public void ShowPhysicsStudio(int tab)
        {
            Clear(Screen.PhysicsStudio);
            SetBackground(null);
            var uss = Resources.Load<StyleSheet>(OuUi.ResourceRoot + "OuGarage");
            if (uss && !Root.styleSheets.Contains(uss)) Root.styleSheets.Add(uss);
            OuTheme.Header(_layer, "PHYSICS STUDIO", "EVERY VEHICLE VALUE · TERRAIN GRIP · CONFIGS · SHARE");
            _psCat = PhysicsCatalog.Get();
            if (_psCat == null)
            {
                OuUi.Text(PhysicsCatalog.Error ?? "The physics catalog is not available.", "mxa-note", _layer);
                var back = OuUi.Button("BACK", "mxa-main-button", ShowMain, _layer);
                _nav.DefaultItem = () => back; _nav.OnCancel = () => { ShowMain(); return true; }; _nav.Focus(back);
                Hints((OuUi.KeySubmit, "SELECT"), (OuUi.KeyCancel, "BACK"));
                return;
            }
            if (_psWork == null) { _psWork = PhysicsLibrary.Active(_psCat)?.Clone() ?? NewConfig(); _psDirty = false; }
            if (_psClass < 0) _psClass = Mathf.Clamp(ReflexSave.GetInt(PsClassKey, 2), 0, _psCat.Classes.Count - 1);   // mx250f: the default rider's bike
            _psGroup = Mathf.Clamp(ReflexSave.GetInt(PsGroupKey, 0), -1, PhysicsCatalog.Groups.Length - 1);
            _psTab = Mathf.Clamp(tab, 0, PsTabs.Length - 1);

            var page = OuUi.Div("ou-garage ou-physics", _layer);
            var panel = OuUi.Div("ou-panel ou-garage-panel", page);
            var tabs = OuUi.Div("ou-gtabs", panel);
            for (int i = 0; i < PsTabs.Length; i++)
            {
                int t = i;
                var b = OuUi.Button(PsTabs[i], "ou-gtab" + (i == _psTab ? " ou-gtab-on" : ""), () => PsTab(t), tabs);
                b.name = "ps-tab-" + i; OuUi.Div("ou-gtab-bar", b);
                if (i == _psTab) b.AddToClassList("ou-selected");
            }
            _psSub = OuUi.Text("", "mxa-note", panel);
            _psScroll = new ScrollView(ScrollViewMode.Vertical) { horizontalScrollerVisibility = ScrollerVisibility.Hidden, verticalScrollerVisibility = ScrollerVisibility.Auto };
            _psScroll.AddToClassList("ou-garage-rows"); _psScroll.AddToClassList("mxa-option-rows");
            panel.Add(_psScroll);
            _psRows = _psScroll.contentContainer;
            _psStatus = OuUi.Text("", "mxa-note ou-garage-status", panel);
            var actions = OuUi.Div("ou-garage-actions", panel);
            var use = OuUi.Button("USE FOR MY RIDES", "mxa-start ou-cta ou-garage-ride", PsUse, actions); use.name = "ps-use";
            var done = OuUi.Button("DONE", "mxa-start ou-cta", PsBack, actions); done.name = "ps-done";
            OuTheme.Backdrop(done);
            OuWidgets.LinkRow(new[] { use, done });

            _psSide = OuUi.Div("ou-panel ou-ps-side", page);
            _psSide.style.flexGrow = 1; _psSide.style.marginLeft = 28; _psSide.style.paddingLeft = 28; _psSide.style.paddingRight = 28; _psSide.style.paddingTop = 22;
            _psActive = OuUi.Text("", "ou-eyebrow", _psSide);
            _psHelp = OuUi.Div("ou-ps-help", _psSide);
            _psHelpTitle = OuUi.Text("", "mxa-track-title", _psHelp); _psHelpTitle.style.whiteSpace = WhiteSpace.Normal;
            _psHelpText = OuUi.Text("", "mxa-note", _psHelp); _psHelpText.style.whiteSpace = WhiteSpace.Normal;
            var fieldRow = OuUi.Div("mxa-row", _psSide);
            OuUi.Text("TYPE A VALUE", "mxa-row-label", fieldRow);
            _psField = new TextField { maxLength = 4000 }; _psField.AddToClassList("mxa-path"); _psField.style.flexGrow = 1;
            fieldRow.Add(_psField);
            _psField.RegisterCallback<KeyDownEvent>(e =>
            {
                if (e.keyCode == KeyCode.Return || e.keyCode == KeyCode.KeypadEnter) { var c = _psCommit; _psCommit = null; c?.Invoke(_psField.value); PsFieldDone(); e.StopPropagation(); }
                else if (e.keyCode == KeyCode.Escape) { _psCommit = null; PsFieldDone(); e.StopPropagation(); }
            }, TrickleDown.TrickleDown);
            fieldRow.style.display = DisplayStyle.None;
            fieldRow.name = "ps-field-row";
            var note = OuUi.Text("A config changes only your own vehicle (unless EVERY RIDER is set) and applies from the next event: races, " +
                                 "Free Ride and the test ride; the career, MotoSkills and the challenges too with OPTIONS > OPEN REFLEX > PHYSICS STUDIO EVERYWHERE " +
                                 "(practice there: nothing saved). In a race or Free Ride, PAUSE > PHYSICS STUDIO (or `) edits it live. An event with " +
                                 "changed physics keeps no offline records. AIR CONTROL values add to OPTIONS > OPEN REFLEX > FREER AIR: that option " +
                                 "removes the original's pulls back to the take-off angle and its AIR LOOSENESS eases the values set here; these values set the stick's strength and the spin's damping.", "mxa-note", _psSide);
            note.style.whiteSpace = WhiteSpace.Normal; note.style.marginTop = 18;

            _nav.OnTab = d => PsTab((_psTab + d + PsTabs.Length) % PsTabs.Length);
            _nav.OnCancel = () => { PsBack(); return true; };
            _nav.OnAlt = () => { var f = _nav.Focused; if (f != null && _psResetOf.TryGetValue(f, out var r)) { r(); PsChanged(); } };
            _nav.OnFind = () => { if (_psTab == 1) { var s = _psRows.Q<TextField>("ps-search"); s?.Focus(); } };
            PsBuild();
        }

        PhysicsConfig NewConfig() => new PhysicsConfig { name = "My physics", paramSet = _psCat?.Hash ?? "" };

        void PsTab(int t) { _psTab = t; ShowPhysicsStudio(t); }

        void PsStatus(string s) { if (_psStatus != null) _psStatus.text = s; }

        void PsHints()
        {
            if (_psTab == 1 || _psTab == 2) Hints((OuUi.KeySubmit, "TYPE VALUE"), ("←→", "ADJUST"), (OuUi.KeyAlt, "RESET VALUE"), (OuUi.KeyTabs, "PAGE"), (OuUi.KeyCancel, "BACK"));
            else Hints((OuUi.KeySubmit, "SELECT"), (OuUi.KeyTabs, "PAGE"), (OuUi.KeyCancel, "BACK"));
        }

        void PsSubLine()
        {
            if (_psSub == null || _psWork == null) return;
            string active = PhysicsLibrary.Active(_psCat)?.name;
            _psSub.text = "EDITING  " + (_psWork.name ?? "").ToUpperInvariant() + (_psWork.IsPreset ? "  ·  PRESET" : "") + (_psDirty ? "  ·  UNSAVED" : "") +
                          "  ·  " + (_psWork.IsOriginal ? "NO CHANGES" : _psWork.Changes + " CHANGE" + (_psWork.Changes == 1 ? "" : "S"));
            if (_psActive != null) _psActive.text = "YOUR RIDES USE  " + (active == null ? "ORIGINAL PHYSICS" : active.ToUpperInvariant() + "  ·  PHYSICS MODIFIED");
        }

        void PsChanged()
        {
            _psDirty = true; _psBackArmed = 0;
            foreach (var r in _psRefresh) r();
            PsSubLine();
        }

        void PsBuild()
        {
            _psRows.Clear(); _psRefresh.Clear(); _psResetOf.Clear();
            if (_psTab == 0) PsConfigsPage();
            else if (_psTab == 1) PsVehiclePage();
            else PsTerrainPage();
            PsSubLine(); PsHints();
            var first = OuUi.FirstItem(_psRows);
            _nav.DefaultItem = () => OuUi.FirstItem(_psRows);
            if (first != null) _nav.Focus(first);
        }

        void PsHelp(string title, string text) { if (_psHelpTitle != null) { _psHelpTitle.text = title; _psHelpText.text = text; } }

        VisualElement PsButton(string label, string name, Action submit, string help = null)
        {
            var b = OuUi.Button(label, "mxa-secondary mxa-library-action", submit, _psRows);
            b.name = name; b.style.marginTop = 4;
            if (help != null) b.RegisterCallback<FocusInEvent>(_ => PsHelp(label, help));
            return b;
        }

        // ------------------------------------------------------------------------------------------- CONFIGS

        void PsConfigsPage()
        {
            OuTheme.Section(_psRows, "THE CONFIG IN THE EDITOR");
            PsTextRow("NAME", "ps-name", () => _psWork.name, v => { _psWork.name = string.IsNullOrWhiteSpace(v) ? "Untitled" : v.Trim(); }, 60);
            PsTextRow("AUTHOR", "ps-author", () => _psWork.author, v => _psWork.author = (v ?? "").Trim(), 40);
            PsTextRow("NOTES", "ps-notes", () => _psWork.description, v => _psWork.description = v ?? "", 400);
            PsButton("SAVE", "ps-save", () => PsSave(false), "Keeps the config in your library (" + PhysicsLibrary.Folder + "). A preset is saved as a new config of yours.");
            PsButton("SAVE AS A COPY", "ps-save-copy", () => PsSave(true), "A new config in your library with these values.");
            PsButton("NEW (ORIGINAL VALUES)", "ps-new", () => PsLoad(NewConfig(), "NEW CONFIG: EVERY VALUE IS THE ORIGINAL'S"), "Starts a config from the game's own values.");
            PsButton("COPY SHARE CODE", "ps-share", () =>
            {
                try { PhysicsLibrary.CopyShareCode(_psWork); PsStatus("SHARE CODE COPIED: PASTE IT IN CHAT, A FORUM, A DISCORD MESSAGE ..."); }
                catch (Exception e) { PsStatus("NOT COPIED: " + e.Message.ToUpperInvariant()); }
            }, "One line of text (" + PhysicsConfig.SharePrefix + "...) that carries the whole config: paste it anywhere, the other player uses PASTE SHARE CODE.");
            PsButton("EXPORT .JSON FILE", "ps-export", () =>
            {
                try { string p = PhysicsLibrary.Export(_psWork); PsStatus("EXPORTED " + System.IO.Path.GetFileName(p).ToUpperInvariant() + " (PHYSICS/EXPORT)"); Reveal(PhysicsLibrary.ExportFolder); }
                catch (Exception e) { PsStatus("NOT EXPORTED: " + e.Message.ToUpperInvariant()); }
            }, "Writes the config as a .json file into " + PhysicsLibrary.ExportFolder + " and opens that folder.");
            if (!string.IsNullOrEmpty(_psWork.id) && !_psWork.IsPreset)
                PsButton(_psArmed == "delete:" + _psWork.id ? "PRESS AGAIN TO DELETE" : "DELETE", "ps-delete", () =>
                {
                    if (_psArmed != "delete:" + _psWork.id) { _psArmed = "delete:" + _psWork.id; PsStatus("PRESS DELETE AGAIN TO DELETE " + _psWork.name.ToUpperInvariant()); PsBuild(); return; }
                    _psArmed = null;
                    try { PhysicsLibrary.Delete(_psWork); PsLoad(NewConfig(), "DELETED"); } catch (Exception e) { PsStatus("NOT DELETED: " + e.Message.ToUpperInvariant()); }
                }, "Removes the config's file from your library.");

            OuTheme.Section(_psRows, "YOUR RIDES");
            var active = PhysicsLibrary.Active(_psCat);
            PsButton(active == null ? "ORIGINAL PHYSICS (IN USE)" : "USE ORIGINAL PHYSICS", "ps-original", () => { PhysicsLibrary.SetActive(null); PsStatus("YOUR RIDES USE THE ORIGINAL PHYSICS"); PsBuild(); },
                     "Nothing is changed: the core is not touched at all (byte for byte the original game).");

            OuTheme.Section(_psRows, "LIBRARY");
            foreach (var c in PhysicsLibrary.All(_psCat))
            {
                var cfg = c;
                var row = PhysicsLibrary.ConfigRow(_psRows, cfg, active, _psCat, PsAccent,
                    () => { if (_psDirty && _psArmed != "load:" + cfg.id) { _psArmed = "load:" + cfg.id; PsStatus("UNSAVED CHANGES: PRESS AGAIN TO LOAD " + cfg.name.ToUpperInvariant()); return; } _psArmed = null; PsLoad(cfg.Clone(), "LOADED " + cfg.name.ToUpperInvariant()); });
                row.RegisterCallback<FocusInEvent>(_ => PsHelp(cfg.name.ToUpperInvariant(), PhysicsLibrary.ConfigHelp(cfg, _psCat) + "\nSELECT loads it into the editor."));
            }

            OuTheme.Section(_psRows, "IMPORT");
            PsButton("PASTE SHARE CODE", "ps-paste", () =>
            {
                var got = PhysicsLibrary.FromClipboard(_psCat, out string err);
                if (got == null) { PsStatus(err); return; }
                PsLoad(got, "PASTED " + got.name.ToUpperInvariant() + " (NOT SAVED YET)" + (got.unknown > 0 ? ": " + got.unknown + " UNKNOWN VALUE(S) LEFT OUT" : ""));
                _psDirty = true; PsSubLine();
            }, "Reads a share code (or a config's JSON) from the clipboard into the editor; SAVE keeps it.");
            PsButton("IMPORT .JSON FILES", "ps-import", () =>
            {
                var refused = new List<string>();
                try
                {
                    var got = PhysicsLibrary.ImportFiles(_psCat, refused);
                    PsBuild();
                    PsStatus(PhysicsLibrary.ImportReport(got.Count, refused));
                    if (got.Count == 0 && refused.Count == 0) Reveal(PhysicsLibrary.ImportFolder);
                }
                catch (Exception e) { PsStatus("IMPORT FAILED: " + e.Message.ToUpperInvariant()); }
            }, "Every .json file in " + PhysicsLibrary.ImportFolder + " becomes a config of your library (checked and clamped first).");
            PsButton("OPEN CONFIG FOLDER", "ps-folder", () => Reveal(PhysicsLibrary.Folder), "Your configs are .json files in " + PhysicsLibrary.Folder + ".");
        }

        internal static void Reveal(string dir)
        {
            try { System.IO.Directory.CreateDirectory(dir); Application.OpenURL(new Uri(dir + System.IO.Path.DirectorySeparatorChar).AbsoluteUri); }
            catch (Exception e) { Debug.LogWarning("[PhysicsStudio] " + e.Message); }
        }

        void PsTextRow(string label, string name, Func<string> get, Action<string> set, int max)
        {
            var row = OuUi.Div("mxa-row", _psRows); row.name = name;
            OuUi.Text(label, "mxa-row-label", row);
            var field = new TextField { value = get() ?? "", maxLength = max };
            field.AddToClassList("mxa-path"); field.style.flexGrow = 1;
            row.Add(field);
            field.RegisterCallback<FocusOutEvent>(_ => { if ((field.value ?? "") != (get() ?? "")) { set(field.value); PsChanged(); } });
            field.RegisterCallback<KeyDownEvent>(e => { if (e.keyCode == KeyCode.Return || e.keyCode == KeyCode.KeypadEnter || e.keyCode == KeyCode.Escape) { row.Focus(); e.StopPropagation(); } }, TrickleDown.TrickleDown);
            OuUi.Item(row, () => field.Focus());
        }

        void PsLoad(PhysicsConfig c, string message)
        {
            _psWork = c; _psDirty = false; _psArmed = null;
            PsBuild(); PsStatus(message);
        }

        void PsSave(bool copy)
        {
            try
            {
                var c = _psWork;
                if (copy || c.IsPreset) { c = c.Clone(); c.id = ""; if (copy) c.name += " copy"; }
                string path = PhysicsLibrary.Save(c);
                bool wasActive = PhysicsLibrary.ActiveId == c.id;
                _psWork = c; _psDirty = false;
                if (wasActive) PhysicsLibrary.SyncPending();
                PsBuild();
                PsStatus("SAVED " + System.IO.Path.GetFileName(path).ToUpperInvariant());
            }
            catch (Exception e) { PsStatus("NOT SAVED: " + e.Message.ToUpperInvariant()); }
        }

        void PsUse()
        {
            if (_psWork.IsOriginal) { PhysicsLibrary.SetActive(null); PsStatus("YOUR RIDES USE THE ORIGINAL PHYSICS"); PsSubLine(); return; }
            if (_psDirty || !_psWork.IsPreset && string.IsNullOrEmpty(_psWork.id)) PsSave(false);   // (a preset as it ships is used by its id)
            if (_psDirty) return;   // (not saved: PsSave said why)
            PhysicsLibrary.SetActive(_psWork);
            PsStatus("YOUR RIDES NOW USE " + _psWork.name.ToUpperInvariant() + ": FROM THE NEXT EVENT");
            if (_psTab == 0) PsBuild(); else PsSubLine();
        }

        void PsBack()
        {
            if (_psDirty && _psBackArmed == 0) { _psBackArmed = 1; PsStatus("UNSAVED CHANGES: SAVE, OR PRESS BACK AGAIN TO LEAVE THEM"); return; }
            _psBackArmed = 0;
            if (_psDirty) { _psWork = null; _psDirty = false; }
            ShowMain();
        }

        // ------------------------------------------------------------------------------------------- VEHICLE

        void PsVehiclePage()
        {
            var cls = _psCat.Classes[_psClass];
            OuTheme.Section(_psRows, "VEHICLE");
            var vrow = OuUi.Spinner("VEHICLE", () => _psCat.Classes[_psClass].label + "  ·  " + PsCount(_psCat.Classes[_psClass].id, null), d =>
            {
                _psClass = (_psClass + d + _psCat.Classes.Count) % _psCat.Classes.Count; ReflexSave.SetInt(PsClassKey, _psClass);
                PsBuild(); var r = _psRows.Q("ps-class"); if (r != null) _nav.Focus(r);
            }, _psRows);
            vrow.name = "ps-class";
            vrow.RegisterCallback<FocusInEvent>(_ => PsHelp(cls.label, "The parameter file " + cls.file + ": " + cls.parms.Count + " parameters, " + cls.parms.Sum(p => p.comps.Count) +
                                                          " values. Every bike of this engine size uses it (the bike model picks the file by engine size)." +
                                                          (cls.live ? "" : " Four-wheel vehicles take a change from the next event.")));
            var grow = OuUi.Spinner("GROUP", () => _psGroup < 0 ? "ALL GROUPS" : PhysicsCatalog.Groups[_psGroup] + "  ·  " + PsCount(_psCat.Classes[_psClass].id, PhysicsCatalog.Groups[_psGroup]), d =>
            {
                _psGroup = (_psGroup + 1 + d + PhysicsCatalog.Groups.Length + 1) % (PhysicsCatalog.Groups.Length + 1) - 1; ReflexSave.SetInt(PsGroupKey, _psGroup);
                PsBuild(); var r = _psRows.Q("ps-group"); if (r != null) _nav.Focus(r);
            }, _psRows);
            grow.name = "ps-group";
            grow.RegisterCallback<FocusInEvent>(_ => PsHelp("GROUP", "Engine & power, gearing, brakes, chassis & mass, steering & lean, suspension, tyres & grip, air control, rider: the parameter files' own sections."));

            var srow = OuUi.Div("mxa-row", _psRows); srow.name = "ps-search-row";
            OuUi.Text("SEARCH", "mxa-row-label", srow);
            var search = new TextField { value = _psSearch, maxLength = 40, name = "ps-search" };
            search.AddToClassList("mxa-path"); search.style.flexGrow = 1;
            search.textEdition.placeholder = "E.G. SPRING, GEAR, GRIP, DAMPING (" + OuUi.KeyFind + ")";
            srow.Add(search);
            search.RegisterCallback<KeyDownEvent>(e =>
            {
                if (e.keyCode == KeyCode.Return || e.keyCode == KeyCode.KeypadEnter || e.keyCode == KeyCode.Escape || e.keyCode == KeyCode.DownArrow)
                { _psSearch = search.value ?? ""; e.StopPropagation(); PsBuild(); var r = _psRows.Q("ps-search-row"); var n = PsFirstValue(); _nav.Focus(n ?? r); }
            }, TrickleDown.TrickleDown);
            OuUi.Item(srow, () => search.Focus());
            srow.RegisterCallback<FocusInEvent>(_ => PsHelp("SEARCH", "Finds values of this vehicle by label, description, raw file name, group or unit, in every group (every word must match; ENTER to search, empty to show the group)."));
            var vehAll = OuUi.Toggle("VALUES FOR", () => _psWork.allVehicles, v => { _psWork.allVehicles = v; PsChanged(); }, _psRows, "EVERY RIDER", "MY VEHICLE ONLY");
            vehAll.name = "ps-all-vehicles";
            vehAll.RegisterCallback<FocusInEvent>(_ => PsHelp("VALUES FOR", "MY VEHICLE ONLY (the default): the computer riders keep the original physics. EVERY RIDER: the computer riders of these classes get the same values (a level field)."));

            string q = (_psSearch ?? "").Trim();
            IEnumerable<PhysicsCatalog.Param> list = cls.parms;
            if (q.Length > 0) list = list.Where(p => p.Matches(q));   // the label, the description, the raw name, the group and the unit
            else if (_psGroup >= 0) list = list.Where(p => p.group == PhysicsCatalog.Groups[_psGroup]);
            int n = 0;
            string lastGroup = null;
            foreach (var p in list.Take(140))
            {
                if (p.group != lastGroup && (q.Length > 0 || _psGroup < 0)) { OuTheme.Section(_psRows, p.group); lastGroup = p.group; }
                else if (lastGroup == null) { OuTheme.Section(_psRows, p.group); lastGroup = p.group; }
                foreach (var c in p.comps) { PsValueRow(cls, p, c); n++; }
            }
            if (n == 0) OuUi.Text("NO VALUES MATCH", "mxa-note", _psRows);
            OuTheme.Section(_psRows, "RESET");
            if (_psGroup >= 0 && q.Length == 0)
                PsButton("RESET " + PhysicsCatalog.Groups[_psGroup] + " (" + cls.label + ")", "ps-reset-group", () => { PsResetWhere(k => k[0] == cls.id && cls.parms.Any(p => p.name == k[1] && p.group == PhysicsCatalog.Groups[_psGroup])); PsStatus("GROUP RESET TO THE ORIGINAL VALUES"); });
            PsButton("RESET " + cls.label, "ps-reset-class", () => { PsResetWhere(k => k[0] == cls.id); PsStatus(cls.label + " RESET TO THE ORIGINAL VALUES"); });
            PsButton("RESET EVERYTHING", "ps-reset-all", () => { _psWork.values.Clear(); _psWork.grip.Clear(); _psWork.drag.Clear(); PsChanged(); PsStatus("EVERY VALUE IS THE ORIGINAL'S"); },
                     "Every vehicle value and the terrain table back to the original's (the config then changes nothing).");
        }

        VisualElement PsFirstValue() => _psRows.Query<VisualElement>(className: "ps-value-row").First();

        string PsCount(string cls, string group)
        {
            int n = _psWork.values.Keys.Count(k => k.StartsWith(cls + "|", StringComparison.OrdinalIgnoreCase) &&
                                                (group == null || _psCat.Find(cls, k.Split('|')[1])?.group == group));
            return n == 0 ? "ORIGINAL" : n + " CHANGED";
        }

        void PsResetWhere(Func<string[], bool> match)
        {
            foreach (var k in _psWork.values.Keys.ToList()) { var s = k.Split('|'); if (s.Length == 3 && match(s)) _psWork.values.Remove(k); }
            PsChanged();
        }

        void PsValueRow(PhysicsCatalog.VehicleClass cls, PhysicsCatalog.Param p, PhysicsCatalog.Comp c)
        {
            string key = PhysicsConfig.Key(cls.id, p.name, c.index);
            string label = p.Label.ToUpperInvariant() + (p.comps.Count > 1 ? "  ·  " + c.label : "");
            string help = p.Describe(c) + "\n" +
                          (p.kind == 2 ? "A curve's knot (x, y) or end slope: the game interpolates between the knots. " : p.kind == 3 ? "Three values the game blends by the surface value under the tyre (the terrain table's material value plus the tyre tuning): 0, 0.5 and 1. " : "") +
                          (p.group == "AIR CONTROL" ? "Air control: with FREER AIR (OPTIONS > OPEN REFLEX) the original's auto-level and trick hold are off; these values still set the stick's rates, the spin damping and the springs. " : "") +
                          (p.inFile ? "" : "Not in the parameter file: the registration's default. ") + "Left / right: change  ·  ENTER: type it  ·  " + OuUi.KeyAlt + ": the file's value.";
            PsBarRow(label, key, () => _psWork.values.TryGetValue(key, out float v) ? v : c.def, () => _psWork.values.ContainsKey(key),
                     v => { if (Mathf.Abs(v - c.def) <= Mathf.Abs(c.def) * 1e-6f) _psWork.values.Remove(key); else _psWork.values[key] = v; },
                     () => _psWork.values.Remove(key), c.min, c.max, c.def, PhysicsCatalog.Param.ShortUnit(c.unit), label, help, p.name, p.Toggle ? 1f : 0f);
        }

        static string Num(float v) => Mathf.Abs(v) >= 1000f || v == Mathf.Round(v) ? v.ToString("0.###", CultureInfo.InvariantCulture) : v.ToString("0.#####", CultureInfo.InvariantCulture);
        static float NiceStep(float span)
        {
            float want = span / 100f, best = 0.0001f;
            foreach (float st in new[] { 0.0001f, 0.0005f, 0.001f, 0.005f, 0.01f, 0.05f, 0.1f, 0.5f, 1f, 5f, 10f, 50f, 100f, 500f, 1000f }) if (st <= want) best = st;
            return best;
        }

        /// A value row: label, a bar over min .. max, the number (marked when changed); left / right step it (to the file's value on the
        /// way), the mouse sets it on the bar, ENTER types it, the alt button puts back the file's value.
        void PsBarRow(string label, string name, Func<float> get, Func<bool> changed, Action<float> set, Action reset, float min, float max, float def, string unit, string title, string help,
                      string raw = null, float stepOverride = 0f)
        {
            var row = OuUi.Div("mxa-row ps-value-row", _psRows); row.name = "ps-v-" + name;
            var lab = OuUi.Text(label, "mxa-row-label", row);
            if (raw != null)
            {   // the file's raw name, small, along the bottom of the row (share codes use the raw names)
                var small = OuUi.Text(raw, null, row);
                small.style.position = Position.Absolute; small.style.left = 20; small.style.bottom = 2; small.style.fontSize = 12; small.style.opacity = 0.5f;
                small.style.letterSpacing = 0; small.style.unityTextAlign = TextAnchor.LowerLeft;
            }
            var right = OuUi.Div("mxa-row-value", row);
            OuUi.Text("‹", "mxa-arrow", right);
            var track = OuUi.Div("mxa-slider", right);
            var fill = OuUi.Div("mxa-slider-fill", track);
            var val = OuUi.Text("", "mxa-value mxa-value-narrow", right);
            OuUi.Text("›", "mxa-arrow", right);
            float step = stepOverride > 0f ? stepOverride : NiceStep(max - min);
            bool focused = false;   // the focused row's fill is the accent: its own (dark) text then, not the changed mark's accent
            void Show()
            {
                float v = get();
                fill.style.width = Length.Percent(max > min ? Mathf.Clamp01((v - min) / (max - min)) * 100f : 0f);
                bool ch = changed();
                val.text = (ch ? "● " : "") + Num(v) + (string.IsNullOrEmpty(unit) ? "" : " " + unit);
                val.style.color = ch && !focused ? new StyleColor(PsAccent) : new StyleColor(StyleKeyword.Null);
                lab.style.color = ch && !focused ? new StyleColor(PsAccent) : new StyleColor(StyleKeyword.Null);
            }
            void Set(float v)
            {
                if (float.IsNaN(v) || float.IsInfinity(v)) return;
                set(Mathf.Clamp(v, min, max)); Show(); PsChanged();
            }
            track.pickingMode = PickingMode.Position;
            track.RegisterCallback<PointerDownEvent>(e =>
            {
                float w = track.resolvedStyle.width;
                if (w > 1) Set(min + Mathf.Clamp01(e.localPosition.x / w) * (max - min));
                e.StopPropagation();
            });
            OuUi.Item(row, () => PsType(title + "  ·  NOW " + Num(get()), Num(get()), s =>
            {
                if (float.TryParse((s ?? "").Trim(), NumberStyles.Float, CultureInfo.InvariantCulture, out float v) && !float.IsNaN(v) && !float.IsInfinity(v))
                {
                    if (v < min || v > max) PsStatus("CLAMPED TO " + Num(min) + " .. " + Num(max));
                    Set(v);
                }
                else PsStatus("TYPE ONE NUMBER (E.G. 1.25)");
            }), d =>
            {
                float v = get(), next = v + d * step;
                if ((v < def && next > def) || (v > def && next < def)) next = def;   // stop at the file's value on the way
                Set(next);
            });
            _psResetOf[row] = () => { reset(); Show(); };
            _psRefresh.Add(Show);
            row.RegisterCallback<FocusInEvent>(_ => { focused = true; Show(); PsHelp(title, help); });
            row.RegisterCallback<FocusOutEvent>(_ => { focused = false; Show(); });
            Show();
        }

        void PsType(string title, string value, Action<string> commit)
        {
            var row = _psSide?.Q("ps-field-row");
            if (row == null) return;
            row.style.display = DisplayStyle.Flex;
            _psCommit = commit;
            _psField.value = value;
            PsHelp(title, "Type the value and press ENTER (ESC cancels).");
            _psField.schedule.Execute(() => { _psField.Focus(); _psField.SelectAll(); }).ExecuteLater(1);
            _psReturnTo = _nav.Focused;
        }
        VisualElement _psReturnTo;
        void PsFieldDone()
        {
            var row = _psSide?.Q("ps-field-row");
            if (row != null) row.style.display = DisplayStyle.None;
            if (_psReturnTo != null && _psReturnTo.panel != null) _nav.Focus(_psReturnTo);
        }

        // ------------------------------------------------------------------------------------------- TERRAIN

        void PsTerrainPage()
        {
            OuTheme.Section(_psRows, "TERRAIN GRIP (00461680)");
            var scope = OuUi.Toggle("GRIP TABLE FOR", () => _psWork.gripGlobal, v => { _psWork.gripGlobal = v; PsChanged(); }, _psRows, "EVERY RIDER", "MY VEHICLE ONLY");
            scope.name = "ps-grip-global";
            scope.RegisterCallback<FocusInEvent>(_ => PsHelp("GRIP TABLE FOR", "MY VEHICLE ONLY: the grip values reach your own tyres; EVERY RIDER: the computer riders' too. The drag table below is always for every rider (the game's drag lookup has no vehicle)."));
            foreach (var m in _psCat.Grip)
            {
                var mat = m;
                PsBarRow(mat.label, "grip-" + mat.index, () => _psWork.grip.TryGetValue(mat.index, out float v) ? v : mat.def, () => _psWork.grip.ContainsKey(mat.index),
                         v => { if (v == mat.def) _psWork.grip.Remove(mat.index); else _psWork.grip[mat.index] = v; }, () => _psWork.grip.Remove(mat.index), mat.min, mat.max, mat.def, "",
                         mat.label, $"The surface value of terrain material {mat.index} (the HFTerrain's friction zones, layer 0). FILE VALUE {Num(mat.def)}. A tyre's grip is this plus its tuning term; " +
                                    "the tyres' three-value parameters (ON SURFACE 0 / 0.5 / 1) are blended by it.");
            }
            OuTheme.Section(_psRows, "TERRAIN DRAG (00461760, EVERY RIDER)");
            foreach (var m in _psCat.Drag)
            {
                var mat = m;
                PsBarRow(mat.label, "drag-" + mat.index, () => _psWork.drag.TryGetValue(mat.index, out float v) ? v : mat.def, () => _psWork.drag.ContainsKey(mat.index),
                         v => { if (v == mat.def) _psWork.drag.Remove(mat.index); else _psWork.drag[mat.index] = v; }, () => _psWork.drag.Remove(mat.index), mat.min, mat.max, mat.def, "",
                         mat.label, $"The drag factor of terrain material layer 1. FILE VALUE {Num(mat.def)}. It is global: every rider on that ground.");
            }
            OuTheme.Section(_psRows, "RESET");
            PsButton("RESET THE TERRAIN TABLES", "ps-reset-terrain", () => { _psWork.grip.Clear(); _psWork.drag.Clear(); PsChanged(); PsStatus("TERRAIN TABLES RESET"); });
        }
    }
}

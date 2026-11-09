// PHYSICS STUDIO · LIVE (w-physstudio2): the in-race editor of the physics config, after Open Unleashed's MxaPhysicsStudioPanel. Offline
// races and Free Ride only (RxPhysicsStudio.LiveEditAllowed; the session armed its live path before the load: EventLiveArmed).
//   open      the pause page's PHYSICS STUDIO row: the panel has the pad / keys over the paused race (changes reach the bike when you ride on)
//             or ` (backquote, unless the game's controls bind it): the panel pinned at the right while you ride, the mouse edits it,
//             the pad and keys keep riding. RIDE WITH PANEL / EDIT WITH PAD switch between the two.
//   rows      the hub page's: VEHICLE (the class; opens on the one you ride), GROUP, SEARCH, a bar + number per value (changed: gold dot),
//             left / right step, ENTER types, Y / F resets the value
//   apply     every change goes to the core within a tenth of a second (rx_session_phys_set + rx_session_phys_apply_live: the parameter
//             block rewritten and the masses + vf35 rerun, as the original's own reloads; every class) at the next frame of the race
//   pages     VALUES (the rows above) and CONFIGS (two tabs; the pad's bumpers switch them; w-psload)
//   CONFIGS   NAME (typed; unnamed live saves get "Live <date time>"), SAVE TO CONFIG, SAVE AS, USE FOR MY RIDES, REVERT TO THE CONFIG (the values this event started with),
//             REVERT TO ORIGINAL; LIBRARY: the presets and the player's saved configs (the hub's list and states): selecting one loads it into the panel and applies it to
//             this ride at once (what YOUR RIDES use does not change until USE FOR MY RIDES; unsaved changes: press again, like the hub); SHARE: COPY / PASTE SHARE CODE
//             (a pasted code loads and applies), EXPORT .JSON FILE (the path is shown; the folder is never opened from a ride), IMPORT .JSON FILES. The logic is
//             PhysicsLibrary's, shared with the hub's CONFIGS page.
// The core's guard stays: a vehicle the values make diverge gets the file's values back (the badge says UNSTABLE). An event with a config
// at any point keeps no offline records.
using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using Reflex.Port.Options;
using Reflex.Port.OriginalWorld;
using Reflex.Port.RxCore;
using UnityEngine;
using UnityEngine.InputSystem;
using UnityEngine.UIElements;

namespace Reflex.Port.OpenFrontEnd
{
    [DisallowMultipleComponent]
    public sealed class PhysicsStudioOverlay : MonoBehaviour
    {
        static PhysicsStudioOverlay s_inst;
        /// The panel has the pad and keys (the pause page waits meanwhile).
        public static bool Modal => s_inst && s_inst._open && s_inst._modal;
        public static bool Shown => s_inst && s_inst._open;
        static readonly Color Accent = new Color(0.886f, 0.675f, 0.251f);

        OriginalWorldRuntime _rt;
        GameObject _host;
        VisualElement _root, _panel, _rows, _hints;
        ScrollView _scroll;
        Label _sub, _status, _help;
        TextField _field; VisualElement _fieldRow; Action<string> _commit; VisualElement _returnTo;
        OuMenuInput _input;
        readonly OuNav _nav = new OuNav();
        readonly List<Action> _refresh = new List<Action>();
        readonly Dictionary<VisualElement, Action> _resetOf = new Dictionary<VisualElement, Action>();
        bool _open, _modal, _pending, _dirty;
        float _applyAt, _statusAt, _statusFor = 5f;
        int _page;           // 0 VALUES, 1 CONFIGS
        string _armed;       // the library entry / paste that asked "press again" (unsaved changes)
        VisualElement[] _tabs;
        Action _back;
        PhysicsCatalog _cat;
        PhysicsConfig _work, _start;
        int _cls = -1, _group = 0;
        string _search = "";
        volatile string _localCls;

        void Awake() { s_inst = this; _rt = GetComponent<OriginalWorldRuntime>(); }
        void OnDestroy() { _input?.Dispose(); if (_host) Destroy(_host); if (s_inst == this) s_inst = null; }

        Reflex.Port.RxCore.OpenReflexSimulation Sim => _rt ? _rt.SimDriver?.Open : null;

        /// The pause page shows its PHYSICS STUDIO row when the running event can be edited live.
        public static bool Available =>
            RxPhysicsStudio.EventLiveArmed && RxPhysicsStudio.LiveEditAllowed((PhysicsContext)RxPhysicsStudio.EventContext) && s_inst && s_inst.Sim != null;

        /// From the pause page: the panel with the pad over the paused race; back returns to the pause page.
        public static void OpenFromPause(Action back) { if (s_inst) s_inst.Open(true, back); }

        void Open(bool modal, Action back)
        {
            if (!Available) return;
            _cat ??= PhysicsCatalog.Get();
            if (_cat == null) return;
            if (_work == null)
            {   // the config this event started with (the core's), else the original's values
                _start = RxPhysicsStudio.EventActive ? (PhysicsLibrary.Active(_cat)?.Clone() ?? new PhysicsConfig { name = "Live config" }) : new PhysicsConfig { name = "Live config" };
                _work = _start.Clone();
            }
            if (_localCls == null) Sim?.BetweenFrames(s => { try { _localCls = LocalClass(RxPhysicsNative.LiveValues(s)) ?? ""; } catch (Exception) { _localCls = ""; } });
            _modal = modal; _back = back;
            if (_root == null) Build();
            _open = true;
            CursorFor(modal);
            _root.style.display = DisplayStyle.Flex;
            _input?.Dispose(); _input = modal ? new OuMenuInput() : null;
            Rebuild();
        }

        // from the pause (pad navigation): a pointer for a mouse player; while riding the mouse tunes, so it is shown whatever was used last
        void CursorFor(bool modal) { Reflex.Port.Platform.ReflexCursor.Want(this, modal); Reflex.Port.Platform.ReflexCursor.Need(this, !modal); }

        void Close(bool toPause)
        {
            if (!_open) return;
            _open = false;
            _input?.Dispose(); _input = null;
            if (_root != null) _root.style.display = DisplayStyle.None;
            Reflex.Port.Platform.ReflexCursor.Want(this, false); Reflex.Port.Platform.ReflexCursor.Need(this, false);
            var b = _back; _back = null;
            if (toPause) b?.Invoke();
        }

        static string LocalClass(string live)
        {
            foreach (var l in (live ?? "").Split('\n'))
            {
                var f = l.Split('\t');
                if (f.Length >= 5 && f[0] == "L" && f[4] == "1") return f[2];
            }
            return null;
        }

        // ------------------------------------------------------------------------------------------- frame

        void Update()
        {
            if (_pending && Time.unscaledTime >= _applyAt) ApplyNow();
            if (_status != null && _status.text.Length > 0 && Time.unscaledTime - _statusAt > _statusFor) _status.text = "";
            if (_open && (!_rt || !_rt.Running)) { Close(false); return; }
            if (_cls < 0 && _cat != null && !string.IsNullOrEmpty(_localCls))
            {
                int i = _cat.Classes.FindIndex(c => c.id == _localCls);
                if (i >= 0) { _cls = i; if (_open) Rebuild(); }
            }
            var kb = Keyboard.current;
            bool menus = _rt && _rt.OwnMenuActive || ReflexOptions.IsOpen;
            if (!_modal && kb != null && kb.backquoteKey.wasPressedThisFrame && !OriginalInput.GameBinds(Key.Backquote) && !Typing && !menus && !OriginalOnboardAdjust.KeyboardCaptured)
            { if (_open) Close(false); else Open(false, null); return; }
            if (_root != null) { var show = _open && (_modal || !menus) ? DisplayStyle.Flex : DisplayStyle.None; if (_root.style.display != show) _root.style.display = show; }   // (a pinned panel steps aside for the pause page)
            if (_open && _modal && _input != null) _nav.Update(_input);
        }

        bool Typing => _root?.focusController?.focusedElement is VisualElement f && (f is TextField || f.parent is TextField);

        void Status(string s, float secs = 5f) { if (_status == null) return; _status.text = s; _statusAt = Time.unscaledTime; _statusFor = secs; }

        void Changed(bool dirty = true)
        {
            if (dirty) { _dirty = true; _armed = null; }
            _pending = true;
            if (_applyAt < Time.unscaledTime) _applyAt = Time.unscaledTime + 0.1f;
            foreach (var r in _refresh) r();
            Sub();
        }

        /// The working config to the core: set + live apply at the next frame of the race (the worker's between-frames queue).
        void ApplyNow()
        {
            _pending = false;
            var sim = Sim;
            if (sim == null || _work == null) return;
            string text = _work.NativeText(), name = _work.name;
            sim.BetweenFrames(s =>
            {
                try
                {
                    int n = RxPhysicsNative.Set(s, text, out _);
                    RxPhysicsNative.ApplyLive(s);
                    RxPhysicsStudio.EventValues = Math.Max(0, n);
                    RxPhysicsStudio.EventActive = n > 0;
                    RxPhysicsStudio.EventName = n > 0 ? name : "";
                    RxPhysicsStudio.EventRestored = false;
                }
                catch (Exception) { }
            });
        }

        // ------------------------------------------------------------------------------------------- build

        void Build()
        {
            _host = new GameObject("Physics Studio live"); _host.transform.SetParent(transform, false);
            _root = OuUi.CreateDocument(_host, 72); OuTheme.Apply(_root);
            _root.pickingMode = PickingMode.Ignore;
            _panel = OuUi.Div("ou-panel", _root);
            _panel.style.position = Position.Absolute; _panel.style.right = 24; _panel.style.top = 70; _panel.style.bottom = 70; _panel.style.width = 640;
            _panel.style.backgroundColor = new Color(0.06f, 0.06f, 0.07f, 0.82f);
            _panel.style.paddingLeft = 18; _panel.style.paddingRight = 18; _panel.style.paddingTop = 14; _panel.style.paddingBottom = 12;
            _panel.style.borderLeftWidth = 4; _panel.style.borderLeftColor = Accent;
            var title = OuUi.Text("PHYSICS STUDIO  ·  LIVE", "mxa-track-title", _panel);
            title.style.fontSize = 30;
            _sub = OuUi.Text("", "mxa-note", _panel); _sub.style.whiteSpace = WhiteSpace.Normal;
            var tabRow = OuUi.Div("", _panel); tabRow.style.flexDirection = FlexDirection.Row; tabRow.style.marginTop = 6; tabRow.style.marginBottom = 4; tabRow.style.flexShrink = 0;
            _tabs = new VisualElement[2];
            for (int i = 0; i < 2; i++)
            {
                int t = i;
                var b = OuUi.Button(i == 0 ? "VALUES" : "CONFIGS", "mxa-secondary mxa-library-action", () => SetPage(t), tabRow);
                b.name = "pl-tab-" + i; b.style.flexGrow = 1; b.style.flexBasis = 0; b.style.height = 40; b.style.marginRight = i == 0 ? 6 : 0;
                b.style.borderBottomWidth = 3;
                _tabs[i] = b;
            }
            _scroll = new ScrollView(ScrollViewMode.Vertical) { horizontalScrollerVisibility = ScrollerVisibility.Hidden, verticalScrollerVisibility = ScrollerVisibility.Auto };
            _scroll.AddToClassList("mxa-option-rows"); _scroll.style.flexGrow = 1; _scroll.style.minHeight = 0;
            _panel.Add(_scroll);
            _rows = _scroll.contentContainer;
            _fieldRow = OuUi.Div("mxa-row", _panel);
            OuUi.Text("TYPE A VALUE", "mxa-row-label", _fieldRow);
            _field = new TextField { maxLength = 40 }; _field.AddToClassList("mxa-path"); _field.style.flexGrow = 1; _fieldRow.Add(_field);
            _field.RegisterCallback<KeyDownEvent>(e =>
            {
                if (e.keyCode == KeyCode.Return || e.keyCode == KeyCode.KeypadEnter) { var c = _commit; _commit = null; c?.Invoke(_field.value); FieldDone(); e.StopPropagation(); }
                else if (e.keyCode == KeyCode.Escape) { _commit = null; FieldDone(); e.StopPropagation(); }
            }, TrickleDown.TrickleDown);
            _fieldRow.style.display = DisplayStyle.None;
            _help = OuUi.Text("", "mxa-note", _panel); _help.style.whiteSpace = WhiteSpace.Normal; _help.style.fontSize = 18;
            _status = OuUi.Text("", "mxa-note ou-garage-status", _panel); _status.style.whiteSpace = WhiteSpace.Normal;
            _hints = OuUi.Div("mxa-hints", _root);
            _nav.Screen = _panel;
            _nav.OnTab = d => SetPage((_page + d + 2) % 2);
            _nav.OnCancel = () => { if (_fieldRow.style.display == DisplayStyle.Flex) { _commit = null; FieldDone(); return true; } Close(true); return true; };
            _nav.OnAlt = () => { var f = _nav.Focused; if (f != null && _resetOf.TryGetValue(f, out var r)) { r(); Changed(); } };
            _nav.OnFind = () => _rows.Q<TextField>("pl-search")?.Focus();
        }

        void Sub()
        {
            if (_sub == null || _work == null) return;
            _sub.text = (_work.IsOriginal ? "ORIGINAL VALUES" : (_work.name ?? "").ToUpperInvariant() + "  ·  " + _work.Changes + " CHANGE" + (_work.Changes == 1 ? "" : "S")) +
                        (_modal ? "  ·  PAUSED: THE CHANGES REACH THE BIKE WHEN YOU RIDE ON" : "  ·  APPLIES AT ONCE") +
                        (RxPhysicsStudio.EventRestored ? "  ·  UNSTABLE: THE ORIGINAL VALUES WERE PUT BACK" : "") + "  ·  NO RECORDS THIS EVENT" +
                        "  ·  " + (RidesUse ? "YOUR RIDES USE IT" : "THIS RIDE ONLY");
        }

        void Hints()
        {
            if (_modal && _page == 0) OuUi.Hints(_hints, (OuUi.KeySubmit, "TYPE VALUE"), ("←→", "ADJUST"), (OuUi.KeyAlt, "RESET VALUE"), (OuUi.KeyTabs, "PAGE"), (OuUi.KeyCancel, "BACK"));
            else if (_modal) OuUi.Hints(_hints, (OuUi.KeySubmit, "SELECT"), (OuUi.KeyTabs, "PAGE"), (OuUi.KeyCancel, "BACK"));
            else OuUi.Hints(_hints, ("`", "CLOSE"), ("MOUSE", _page == 0 ? "TUNE" : "SELECT"));
        }

        /// Whether the working config is what the player's rides use (so USE FOR MY RIDES has nothing left to do).
        bool RidesUse
        {
            get
            {
                if (_work == null || _dirty) return false;
                string id = PhysicsLibrary.ActiveId;
                if (string.IsNullOrEmpty(id) || id == "preset-original") return _work.IsOriginal;
                return _work.id == id;
            }
        }

        void SetPage(int page)
        {
            if (page == _page) return;
            if (_fieldRow != null && _fieldRow.style.display == DisplayStyle.Flex) { _commit = null; FieldDone(); }
            _page = page; _armed = null;
            if (_help != null) _help.text = "";
            if (_scroll != null) _scroll.scrollOffset = Vector2.zero;
            Rebuild();
        }

        /// Rebuild the page, the focus back on the row it was on (by name) with that row scrolled into view once the layout has run.
        void RebuildKeep()
        {
            string keep = _nav.Focused?.name;
            Rebuild();
            if (!string.IsNullOrEmpty(keep)) FocusNamed(keep);
        }

        void FocusNamed(string name)
        {
            var e = _rows?.Q(name);
            if (e == null) return;
            _nav.Focus(e);
            e.schedule.Execute(() => { if (e.panel != null && e.focusController?.focusedElement == e) _scroll.ScrollTo(e); }).ExecuteLater(30);
        }

        void Rebuild()
        {
            _rows.Clear(); _refresh.Clear(); _resetOf.Clear();
            for (int i = 0; i < _tabs.Length; i++)
            {
                bool on = i == _page;
                _tabs[i].style.borderBottomColor = on ? new StyleColor(Accent) : new StyleColor(Color.clear);
                _tabs[i].Q<Label>()?.EnableInClassList("ou-selected", on);
                var lab = _tabs[i].Q<Label>(); if (lab != null) lab.style.color = on ? new StyleColor(Accent) : new StyleColor(StyleKeyword.Null);
            }
            if (_cls < 0) _cls = Mathf.Clamp(ReflexSave.GetInt("Physics.StudioClass", 2), 0, _cat.Classes.Count - 1);
            if (_page == 0) ValuesPage(); else ConfigsPage();
            Sub(); Hints();
            if (_modal) { var first = OuUi.FirstItem(_rows); _nav.DefaultItem = () => OuUi.FirstItem(_rows); if (first != null && _nav.Focused == null) _nav.Focus(first); }
        }

        void ValuesPage()
        {
            var cls = _cat.Classes[_cls];
            var v = OuUi.Spinner("VEHICLE", () => _cat.Classes[_cls].label + (_cat.Classes[_cls].id == _localCls ? "  ·  YOURS" : ""), d =>
            { _cls = (_cls + d + _cat.Classes.Count) % _cat.Classes.Count; Rebuild(); _nav.Focus(_rows.Q("pl-class")); }, _rows);
            v.name = "pl-class";
            var g = OuUi.Spinner("GROUP", () => PhysicsCatalog.Groups[_group], d =>
            { _group = (_group + d + PhysicsCatalog.Groups.Length) % PhysicsCatalog.Groups.Length; Rebuild(); _nav.Focus(_rows.Q("pl-group")); }, _rows);
            g.name = "pl-group";
            var srow = OuUi.Div("mxa-row", _rows); srow.name = "pl-search-row";
            OuUi.Text("SEARCH", "mxa-row-label", srow);
            var search = new TextField { value = _search, maxLength = 40, name = "pl-search" };
            search.AddToClassList("mxa-path"); search.style.flexGrow = 1; srow.Add(search);
            search.RegisterCallback<KeyDownEvent>(e =>
            {
                if (e.keyCode == KeyCode.Return || e.keyCode == KeyCode.KeypadEnter || e.keyCode == KeyCode.Escape)
                { _search = search.value ?? ""; e.StopPropagation(); Rebuild(); _nav.Focus(_rows.Q("pl-search-row")); }
            }, TrickleDown.TrickleDown);
            OuUi.Item(srow, () => search.Focus());

            string q = (_search ?? "").Trim();   // the label, description, raw name, group and unit are searched
            var list = q.Length > 0 ? cls.parms.Where(p => p.Matches(q)) : cls.parms.Where(p => p.group == PhysicsCatalog.Groups[_group]);
            int n = 0;
            foreach (var p in list.Take(60))
                foreach (var c in p.comps) { ValueRow(cls, p, c); n++; }
            if (n == 0) OuUi.Text("NO VALUES MATCH", "mxa-note", _rows);
        }

        void ConfigsPage()
        {
            OuTheme.Section(_rows, "CONFIG");
            var nrow = OuUi.Div("mxa-row", _rows); nrow.name = "pl-name-row";
            OuUi.Text("NAME", "mxa-row-label", nrow);
            var nameField = new TextField { value = _work.name == "Live config" ? "" : _work.name, maxLength = 60, name = "pl-name" };
            nameField.AddToClassList("mxa-path"); nameField.style.flexGrow = 1; nrow.Add(nameField);
            void Rename() { if (!string.IsNullOrWhiteSpace(nameField.value) && nameField.value.Trim() != _work.name) { _work.name = nameField.value.Trim(); _dirty = true; _armed = null; Sub(); } }
            nameField.RegisterCallback<KeyDownEvent>(e =>
            {
                if (e.keyCode == KeyCode.Return || e.keyCode == KeyCode.KeypadEnter || e.keyCode == KeyCode.Escape)
                {
                    if (e.keyCode != KeyCode.Escape && !string.IsNullOrWhiteSpace(nameField.value)) { Rename(); Status("NAME: " + _work.name.ToUpperInvariant() + " (SAVE TO KEEP IT)"); }
                    e.StopPropagation(); Rebuild(); _nav.Focus(_rows.Q("pl-name-row"));
                }
            }, TrickleDown.TrickleDown);
            nameField.RegisterCallback<FocusOutEvent>(_ => Rename());
            OuUi.Item(nrow, () => nameField.Focus());
            Button("SAVE TO CONFIG", "pl-save", () => Save(false));
            Button("SAVE AS A NEW CONFIG", "pl-save-new", () => Save(true));
            Button("USE FOR MY RIDES", "pl-use", UseForMyRides);
            Button("REVERT TO THE CONFIG", "pl-revert", () => { _work = _start.Clone(); _dirty = false; Changed(false); RebuildKeep(); Status("BACK TO THE VALUES THIS EVENT STARTED WITH"); });
            Button("REVERT TO ORIGINAL", "pl-revert-original", () => { _work = new PhysicsConfig { name = _work.name, id = _work.id, author = _work.author, description = _work.description }; Changed(); RebuildKeep(); Status("EVERY VALUE IS THE ORIGINAL'S"); });

            OuTheme.Section(_rows, "LIBRARY");
            var active = PhysicsLibrary.Active(_cat);
            int shown = 0;
            foreach (var c in PhysicsLibrary.All(_cat))
            {
                if (++shown > 200) { OuUi.Text("THE FIRST 200 CONFIGS ARE LISTED", "mxa-note", _rows); break; }
                var cfg = c;
                var row = PhysicsLibrary.ConfigRow(_rows, cfg, active, _cat, Accent, () =>
                {
                    if (!Proceed("load:" + cfg.id, "LOAD " + cfg.name.ToUpperInvariant())) return;
                    Load(cfg.Clone(), cfg.name.ToUpperInvariant(), false);
                });
                row.RegisterCallback<FocusInEvent>(_ => _help.text = PhysicsLibrary.ConfigHelp(cfg, _cat) + "\nSELECT loads it into the panel and applies it to this ride; USE FOR MY RIDES keeps it for your rides.");
            }

            OuTheme.Section(_rows, "SHARE");
            Button("COPY SHARE CODE", "pl-share", () =>
            {
                try { PhysicsLibrary.CopyShareCode(_work); Status("SHARE CODE COPIED: PASTE IT IN CHAT, A FORUM, A DISCORD MESSAGE ...", 8f); }
                catch (Exception e) { Status("NOT COPIED: " + e.Message.ToUpperInvariant()); }
            }, "One line of text (" + PhysicsConfig.SharePrefix + "...) that carries the whole config as it is in the panel now.");
            Button("PASTE SHARE CODE", "pl-paste", () =>
            {
                var got = PhysicsLibrary.FromClipboard(_cat, out string err);
                if (got == null) { Status(err); return; }
                if (!Proceed("paste", "PASTE " + got.name.ToUpperInvariant())) return;
                Load(got, got.name.ToUpperInvariant() + (got.unknown > 0 ? " (" + got.unknown + " UNKNOWN VALUE(S) LEFT OUT)" : ""), true);
            }, "Reads a share code (or a config's JSON) from the clipboard into the panel and applies it to this ride; SAVE AS A NEW CONFIG keeps it.");
            Button("EXPORT .JSON FILE", "pl-export", () =>
            {
                try { string path = PhysicsLibrary.Export(_work); Status("EXPORTED TO " + path, 12f); }
                catch (Exception e) { Status("NOT EXPORTED: " + e.Message.ToUpperInvariant()); }
            }, "Writes the config as a .json file into " + PhysicsLibrary.ExportFolder + " (no window is opened over the game).");
            Button("IMPORT .JSON FILES", "pl-import", () =>
            {
                var refused = new List<string>();
                try
                {
                    var got = PhysicsLibrary.ImportFiles(_cat, refused);
                    RebuildKeep();
                    Status(got.Count == 0 && refused.Count == 0 ? "PUT .JSON FILES IN " + PhysicsLibrary.ImportFolder.ToUpperInvariant() + " FIRST" : PhysicsLibrary.ImportReport(got.Count, refused), 10f);
                }
                catch (Exception e) { Status("IMPORT FAILED: " + e.Message.ToUpperInvariant()); }
            }, "Every .json file in " + PhysicsLibrary.ImportFolder + " becomes a config of your library (checked and clamped first); it shows in the LIBRARY above.");
            if (!UnityEngine.Screen.fullScreen)   // (opening a folder from fullscreen would pull the player out of the game)
                Button("OPEN CONFIG FOLDER", "pl-folder", () => OpenShell.Reveal(PhysicsLibrary.Folder), "Your configs are .json files in " + PhysicsLibrary.Folder + ".");

            OuTheme.Section(_rows, "PANEL");
            Button(_modal ? "RIDE WITH THE PANEL (MOUSE)" : "EDIT WITH THE PAD (PAUSE)", "pl-mode", () =>
            {
                if (_modal) { var b = _back; _back = null; _modal = false; _input?.Dispose(); _input = null; GetComponent<OwnRacePause>()?.Close(); _open = true; _root.style.display = DisplayStyle.Flex; CursorFor(false); Rebuild(); Status("RIDING: ` CLOSES THE PANEL, THE MOUSE TUNES"); }
                else { Close(false); GetComponent<OwnRacePause>()?.Open(); }
            });
            Button("CLOSE", "pl-close", () => Close(_modal));
        }

        /// The "press again" guard of the hub: unsaved changes ask once before a load / paste replaces them.
        bool Proceed(string key, string what)
        {
            if (_dirty && _armed != key) { _armed = key; Status("UNSAVED CHANGES: PRESS AGAIN TO " + what, 8f); return false; }
            _armed = null;
            return true;
        }

        /// A config from the library / a share code into the panel and onto this ride at once (the same apply path as an edit); YOUR RIDES keep their config.
        void Load(PhysicsConfig c, string name, bool unsaved)
        {
            if (!Available) { Status("LIVE EDITING IS NOT ALLOWED IN THIS EVENT"); return; }
            _work = c; _dirty = unsaved; _armed = null;
            _pending = true; _applyAt = Time.unscaledTime;
            Rebuild();
            if (!string.IsNullOrEmpty(c.id)) FocusNamed("ps-config-" + c.id); else FocusNamed("pl-paste");
            string mine = PhysicsLibrary.Active(_cat)?.name;
            Status((unsaved ? "PASTED " : "LOADED ") + name + ": ON THIS RIDE NOW" + (RidesUse ? ", AND YOUR RIDES USE IT" : "; YOUR RIDES STILL USE " + (mine == null ? "THE ORIGINAL PHYSICS" : mine.ToUpperInvariant()) + " (USE FOR MY RIDES TO CHANGE THAT)") +
                   (unsaved ? ". NOT SAVED YET" : ""), 10f);
        }

        void UseForMyRides()
        {
            if (_work.IsOriginal) { PhysicsLibrary.SetActive(null); RebuildKeep(); Status("YOUR RIDES USE THE ORIGINAL PHYSICS"); return; }
            if (_dirty || !_work.IsPreset && string.IsNullOrEmpty(_work.id)) Save(false);   // (a preset as it ships is used by its id)
            if (_dirty || string.IsNullOrEmpty(_work.id)) return;   // (not saved: Save said why)
            PhysicsLibrary.SetActive(_work);
            RebuildKeep();
            Status("YOUR RIDES NOW USE " + _work.name.ToUpperInvariant() + " (FROM THE NEXT EVENT; THIS RIDE HAS IT NOW)", 8f);
        }

        void Button(string label, string name, Action a, string help = null)
        {
            var b = OuUi.Button(label, "mxa-secondary mxa-library-action", a, _rows);
            b.name = name; b.style.marginTop = 3; b.style.height = 44;
            if (help != null) b.RegisterCallback<FocusInEvent>(_ => _help.text = label + ": " + help);
        }

        void Save(bool asNew)
        {
            try
            {
                var c = _work;
                if (asNew || c.IsPreset || string.IsNullOrEmpty(c.id)) { c = c.Clone(); c.id = ""; if (asNew || string.IsNullOrWhiteSpace(c.name) || c.name == "Live config") c.name = (string.IsNullOrWhiteSpace(c.name) || c.name == "Live config" ? "Live " + DateTime.Now.ToString("MMM d HH:mm", CultureInfo.InvariantCulture) : c.name) + (asNew ? " copy" : ""); }
                string path = PhysicsLibrary.Save(c);
                _work = c; _dirty = false; _armed = null;
                if (PhysicsLibrary.ActiveId == c.id) PhysicsLibrary.SyncPending();
                if (_page == 1) RebuildKeep();   // (the library lists it now)
                Status("SAVED " + System.IO.Path.GetFileName(path).ToUpperInvariant());
                Sub();
            }
            catch (Exception e) { Status("NOT SAVED: " + e.Message.ToUpperInvariant()); }
        }

        static string Num(float v) => Mathf.Abs(v) >= 1000f || v == Mathf.Round(v) ? v.ToString("0.###", CultureInfo.InvariantCulture) : v.ToString("0.#####", CultureInfo.InvariantCulture);
        static float NiceStep(float span)
        {
            float want = span / 100f, best = 0.0001f;
            foreach (float st in new[] { 0.0001f, 0.0005f, 0.001f, 0.005f, 0.01f, 0.05f, 0.1f, 0.5f, 1f, 5f, 10f, 50f, 100f, 500f, 1000f }) if (st <= want) best = st;
            return best;
        }

        void ValueRow(PhysicsCatalog.VehicleClass cls, PhysicsCatalog.Param p, PhysicsCatalog.Comp c)
        {
            string key = PhysicsConfig.Key(cls.id, p.name, c.index);
            string label = p.Label.ToUpperInvariant() + (p.comps.Count > 1 ? " · " + c.label : "");
            string unit = PhysicsCatalog.Param.ShortUnit(c.unit);
            float min = c.min, max = c.max, def = c.def, step = p.Toggle ? 1f : NiceStep(max - min);
            float Get() => _work.values.TryGetValue(key, out float x) ? x : def;
            bool Changed_() => _work.values.ContainsKey(key);
            var row = OuUi.Div("mxa-row", _rows);
            row.style.height = 46;
            var lab = OuUi.Text(label, "mxa-row-label", row); lab.style.fontSize = 18; lab.style.marginTop = -6;
            var raw = OuUi.Text(p.name, null, row);   // the file's raw name, small (share codes use it)
            raw.style.position = Position.Absolute; raw.style.left = 20; raw.style.bottom = 1; raw.style.fontSize = 11; raw.style.opacity = 0.5f; raw.style.letterSpacing = 0;
            var right = OuUi.Div("mxa-row-value", row);
            var track = OuUi.Div("mxa-slider", right);
            var fill = OuUi.Div("mxa-slider-fill", track);
            var val = OuUi.Text("", "mxa-value mxa-value-narrow", right);
            void Show()
            {
                float x = Get(); bool ch = Changed_();
                fill.style.width = Length.Percent(max > min ? Mathf.Clamp01((x - min) / (max - min)) * 100f : 0f);
                val.text = (ch ? "● " : "") + Num(x) + (unit.Length > 0 ? " " + unit : "");
                var col = ch ? new StyleColor(Accent) : new StyleColor(StyleKeyword.Null);
                val.style.color = col; lab.style.color = col;
            }
            void Set(float x)
            {
                if (float.IsNaN(x) || float.IsInfinity(x)) return;
                x = Mathf.Clamp(x, min, max);
                if (Mathf.Abs(x - def) <= Mathf.Abs(def) * 1e-6f) _work.values.Remove(key); else _work.values[key] = x;
                Show(); Changed();
            }
            track.pickingMode = PickingMode.Position;
            track.RegisterCallback<PointerDownEvent>(e => { float w = track.resolvedStyle.width; if (w > 1) Set(min + Mathf.Clamp01(e.localPosition.x / w) * (max - min)); e.StopPropagation(); });
            OuUi.Item(row, () => Type(label, Num(Get()), s =>
            {
                if (float.TryParse((s ?? "").Trim(), NumberStyles.Float, CultureInfo.InvariantCulture, out float x) && !float.IsNaN(x) && !float.IsInfinity(x)) Set(x);
                else Status("TYPE ONE NUMBER (E.G. 1.25)");
            }), d =>
            {
                float x = Get(), next = x + d * step;
                if ((x < def && next > def) || (x > def && next < def)) next = def;
                Set(next);
            });
            _resetOf[row] = () => { _work.values.Remove(key); Show(); };
            _refresh.Add(Show);
            row.RegisterCallback<FocusInEvent>(_ => _help.text = label + ": " + p.Describe(c));
            Show();
        }

        void Type(string title, string value, Action<string> commit)
        {
            _fieldRow.style.display = DisplayStyle.Flex;
            _commit = commit; _field.value = value; _help.text = title + ": type the value, ENTER sets it, ESC cancels";
            _returnTo = _nav.Focused;
            _field.schedule.Execute(() => { _field.Focus(); _field.SelectAll(); }).ExecuteLater(1);
        }
        void FieldDone()
        {
            _fieldRow.style.display = DisplayStyle.None;
            if (_returnTo != null && _returnTo.panel != null && _modal) _nav.Focus(_returnTo);
        }
    }
}

// GAME LIBRARY  ·  MX vs ATV ALIVE: the player's own Alive content (Xbox 360) imported into the client's content folder by the in-client
// importer (Content/AliveImport: no Python, no native code, macOS and Windows).
//   sources   a folder / file browser that works with a gamepad: the disc image (.iso), an extracted disc folder, the 5451087B content
//             folder of an Xbox 360 drive (or its 00000002 folder of LIVE packages), single LIVE packages; ADD THIS FOLDER / a file adds it
//   levels    FIND LEVELS opens them on a worker thread (STFS hash trees, the original-release check, the game's level names), adds the
//             menu pictures of levels imported before they existed (AliveImporter.AddPictures) and lists every level found: choose them, IMPORT
//   import    the chosen levels converted and installed one after another on a worker thread (progress, CANCEL), then each level's
//             result; AliveLevels.Reset() so the MX vs ATV Alive category lists them at once. The player's sources are only read.
//   bikes     the Alive motorcycles found (AliveBikes: each rides as the Reflex bike of its class and brand) are listed under the
//             levels, all chosen; IMPORT converts the chosen ones into <content>/AliveBikes (.rxmod folders) after the levels and the
//             garage lists them as MOD cards (BikeMods.Refresh)
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using Reflex.Port.Content;
using Reflex.Port.Content.AliveImport;
using UnityEngine;
using UnityEngine.UIElements;

namespace Reflex.Port.OpenFrontEnd
{
    public sealed partial class OpenShell
    {
        readonly List<string> _aliveSourcePaths = new List<string>();
        string _aliveDir;
        AliveSources _aliveFound;
        readonly HashSet<string> _aliveChosen = new HashSet<string>(StringComparer.Ordinal);
        Task _aliveTask; CancellationTokenSource _aliveCancel;
        string _aliveStatus = ""; AliveProgress _aliveProgress;
        Label _aliveStatusLabel; VisualElement _aliveFill;
        List<AliveLevelResult> _aliveResults; Exception _aliveError;
        List<AliveBikeResult> _aliveBikeResults;
        const string BikeKey = "bike:";
        bool _aliveImporting; string _aliveNotice;
        readonly List<string> _aliveOrder = new List<string>();
        public bool AliveImportRunning => _aliveTask != null && !_aliveTask.IsCompleted;
        /// The last import's per-level results (null before / during one; tests read them).
        public IReadOnlyList<AliveLevelResult> AliveResults => AliveImportRunning ? null : _aliveResults;

        /// The content folder the importer writes into: the connected install when it is an extracted Reflex folder (null otherwise).
        static string AliveContent()
        {
            string dir = ReflexInstall.Dir;
            return dir != null && AliveImporter.ContentProblem(dir) == null ? dir : null;
        }

        /// The library card (ShowLibrary): the Alive levels installed, IMPORT ALIVE CONTENT.
        void AliveLibraryCard(VisualElement list)
        {
            var card = OuUi.Div("mxa-library-card", list);
            card.style.flexWrap = Wrap.Wrap;
            var copy = OuUi.Div("mxa-library-copy", card);
            copy.style.flexGrow = 1; copy.style.flexShrink = 1; copy.style.minWidth = 240;
            OuUi.Text("OPTIONAL  ·  XBOX 360 CONTENT", "mxa-library-platform", copy);
            OuUi.Text("MX vs ATV ALIVE", "mxa-track-title", copy);
            string content = AliveContent();
            int count = 0; try { count = AliveLevels.All.Count; } catch (Exception) { }
            OuUi.Text(content == null ? (ReflexInstall.Dir == null ? "Connect the Reflex game folder first." : AliveImporter.ContentProblem(ReflexInstall.Dir))
                                      : "Import the levels of your own MX vs ATV Alive disc and downloaded level packs. Your files are only read.", "mxa-library-description", copy).style.whiteSpace = WhiteSpace.Normal;
            OuUi.Text(count > 0 ? $"{count} {(count == 1 ? "LEVEL" : "LEVELS")} INSTALLED  ·  " + string.Join(", ", AliveLevels.All.Select(e => e.DisplayName.ToUpperInvariant()))
                                : "NO ALIVE LEVELS INSTALLED", "mxa-library-state", copy).style.whiteSpace = WhiteSpace.Normal;
            var actions = OuUi.Div("ou-library-actions", card);
            actions.style.flexDirection = FlexDirection.Column; actions.style.width = 300; actions.style.flexShrink = 0;
            var b = OuUi.Button(AliveImportRunning ? "IMPORT IN PROGRESS" : "IMPORT ALIVE CONTENT", "mxa-secondary mxa-library-action", () => { if (AliveImportRunning) ShowAliveProgress(); else ShowAliveSources(); }, actions);
            b.name = "ou-library-alive-import";
            b.style.width = 300; b.style.height = 54; b.style.marginTop = 6;
            var label = b.Q<Label>(); if (label != null) { label.style.fontSize = 26; label.style.letterSpacing = 2; }
            b.SetEnabled(content != null);
        }

        // --------------------------------------------------------------------------------------------- sources

        public void ShowAliveSources()
        {
            Clear(Screen.Install, "import");
            SetBackground(null);
            DriveScanner.Default.BeginRefresh();
            if (string.IsNullOrEmpty(_aliveDir) || !ReflexFs.Within(2500, () => Directory.Exists(_aliveDir), out _))
            {
                string downloads = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.UserProfile), "Downloads");
                _aliveDir = ReflexFs.Within(2500, () => Directory.Exists(downloads), out _) ? downloads : Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);
            }
            var box = OuUi.Div("mxa-panel mxa-install", _layer);
            box.name = "ou-alive-sources";
            OuUi.Text("MX vs ATV ALIVE", "mxa-screen-title", box);
            OuUi.Text("Choose your MX vs ATV Alive content: the disc image (.iso), an extracted disc folder, the 5451087B folder of an Xbox 360 drive's " +
                      "Content folder (or its 00000002 folder), or single downloaded level packs. Downloads are checked against their own hashes; " +
                      "only the original levels are imported. Nothing is changed in your files.", "mxa-install-text", box).style.whiteSpace = WhiteSpace.Normal;
            var chosen = OuUi.Text("", "mxa-note", box); chosen.style.whiteSpace = WhiteSpace.Normal;
            void ShowChosen() => chosen.text = _aliveSourcePaths.Count == 0 ? "No source added yet." : "SOURCES:  " + string.Join("  ·  ", _aliveSourcePaths.Select(p => ReflexPaths.LabelOf(p)));
            ShowChosen();
            var field = new TextField { value = _aliveDir }; field.name = "ou-alive-path"; field.AddToClassList("mxa-path"); box.Add(field);
            var status = OuUi.Text(_aliveNotice ?? "", "mxa-install-status", box); status.style.whiteSpace = WhiteSpace.Normal;
            _aliveNotice = null;
            var sv = new ScrollView(ScrollViewMode.Vertical); sv.AddToClassList("mxa-dir-list"); box.Add(sv);
            var buttons = OuUi.Div("mxa-install-buttons", box);
            VisualElement find = null;

            void Add(string path)
            {
                try { path = Path.GetFullPath(path); }
                catch (Exception e) when (e is ArgumentException || e is NotSupportedException || e is PathTooLongException) { status.text = "That path can't be used: " + e.Message; return; }
                if (!_aliveSourcePaths.Any(p => ReflexPaths.SamePath(p, path))) _aliveSourcePaths.Add(path);
                status.text = "Added " + ReflexPaths.LabelOf(path);
                ShowChosen(); find?.SetEnabled(true);
            }
            void Refresh(string dir)
            {
                _aliveDir = dir; field.SetValueWithoutNotify(dir);
                sv.contentContainer.Clear();
                OuUi.Text("DRIVES & PLACES", "mxa-dir-header", sv.contentContainer);
                foreach (var (path, label) in InstallPlaces(false))
                {
                    var pp = path;
                    OuUi.Item(OuUi.Text(label, "mxa-dir", sv.contentContainer), () => Refresh(pp));
                }
                OuUi.Text("IN " + ShortPath(dir).ToUpperInvariant(), "mxa-dir-header", sv.contentContainer);
                string parent = ReflexPaths.Parent(dir);
                if (parent != null) OuUi.Item(OuUi.Text("←  ..", "mxa-dir", sv.contentContainer), () => Refresh(parent));
                try
                {
                    foreach (var d in Directory.GetDirectories(dir).OrderBy(x => x, StringComparer.OrdinalIgnoreCase))
                    {
                        string name = Path.GetFileName(d);
                        if (name.StartsWith(".") || ReflexMods.IsJunk(name)) continue;
                        var dd = d; bool alive = LooksLikeAliveFolder(d);
                        var it = OuUi.Item(OuUi.Text(name + (alive ? "   ✓ ALIVE" : ""), "mxa-dir", sv.contentContainer), () => Refresh(dd));
                        it.EnableInClassList("mxa-ok", alive);
                    }
                    int shown = 0;
                    foreach (var f in Directory.GetFiles(dir).OrderBy(x => x, StringComparer.OrdinalIgnoreCase))
                    {
                        if (shown >= 200) break;
                        string name = Path.GetFileName(f);
                        if (name.StartsWith(".") || !LooksLikeAliveFile(f)) continue;
                        shown++;
                        var ff = f;
                        var it = OuUi.Item(OuUi.Text("+  " + name + "   " + OpenWorldCache.SizeText(new FileInfo(f).Length), "mxa-dir", sv.contentContainer), () => Add(ff));
                        it.EnableInClassList("mxa-ok", true);
                    }
                }
                catch (Exception e) { OuUi.Text("This folder can't be read: " + e.Message, "mxa-error", sv.contentContainer); }
            }
            field.isDelayed = true;
            field.RegisterValueChangedCallback(e =>
            {
                // any drive letter, a network path, a disc image anywhere; clear sentences for what is not a path or not there
                string s = CleanPath(e.newValue), why = ReflexPaths.Validate(s, ReflexInstall.WindowsHost);
                try
                {
                    int look = 0; bool timedOut = false;
                    if (why == null) look = ReflexFs.Within(3000, () => File.Exists(s) ? 2 : Directory.Exists(s) ? 1 : 0, out timedOut);
                    if (why != null) { }
                    else if (timedOut) why = "That location did not answer (a network drive that is offline?): " + s;
                    else if (look == 2) { Add(s); Refresh(Path.GetDirectoryName(Path.GetFullPath(s))); return; }
                    else if (look == 1) { Refresh(Path.GetFullPath(s)); return; }
                    else why = ReflexPaths.NotFound(s, PeekRoots());
                }
                catch (Exception ex) when (ex is IOException || ex is ArgumentException || ex is NotSupportedException || ex is UnauthorizedAccessException) { why = "That path can't be opened: " + ex.Message; }
                status.text = why; field.SetValueWithoutNotify(_aliveDir);
            });
            var add = OuUi.Button("ADD THIS FOLDER", "mxa-secondary", () => Add(_aliveDir), buttons); add.name = "ou-alive-add";
            find = OuUi.Button("FIND LEVELS", "mxa-start", BeginAliveScan, buttons); find.name = "ou-alive-find";
            find.SetEnabled(_aliveSourcePaths.Count > 0);
            OuUi.Button("CLEAR", "mxa-secondary", () => { _aliveSourcePaths.Clear(); ShowChosen(); find.SetEnabled(false); status.text = ""; }, buttons);
            OuUi.Button("BACK", "mxa-secondary", LeaveAlive, buttons);
            Refresh(_aliveDir);
            DrivesChanged();
            ScreenUpdate = () => { if (DrivesChanged()) Refresh(_aliveDir); };
            _nav.OnCancel = () => { LeaveAlive(); return true; };
            _nav.DefaultItem = () => _aliveSourcePaths.Count > 0 ? find : add;
            Hints((OuUi.KeySubmit, "OPEN / ADD"), (OuUi.KeyCancel, "BACK"));
            _nav.Focus(_aliveSourcePaths.Count > 0 ? find : add);
        }

        /// Back to the library: the sources found are closed (their files are not held open).
        void LeaveAlive()
        {
            if (AliveImportRunning) return;
            _aliveFound?.Dispose(); _aliveFound = null;
            ShowLibrary(_libraryFromMain);
        }

        /// A folder that holds Alive data (cheap: names only).
        static bool LooksLikeAliveFolder(string d)
        {
            try
            {
                string n = Path.GetFileName(d);
                if (string.Equals(n, "5451087B", StringComparison.OrdinalIgnoreCase)) return true;
                if (Directory.Exists(Path.Combine(d, "5451087B"))) return true;
                return Directory.EnumerateFiles(d, "*.xenon.level").Any() || Directory.Exists(Path.Combine(d, "Database")) && Directory.EnumerateFiles(Path.Combine(d, "Database"), "*.xenon.level").Any();
            }
            catch (Exception) { return false; }
        }

        /// A disc image or a LIVE / PIRS / CON package.
        static bool LooksLikeAliveFile(string f)
        {
            if (f.EndsWith(".iso", StringComparison.OrdinalIgnoreCase)) return true;
            try
            {
                if (new FileInfo(f).Length < 0x971a) return false;
                using (var s = new FileStream(f, FileMode.Open, FileAccess.Read, FileShare.Read))
                {
                    var b = new byte[4]; if (s.Read(b, 0, 4) != 4) return false;
                    string m = System.Text.Encoding.ASCII.GetString(b);
                    return m == "LIVE" || m == "PIRS" || m == "CON ";
                }
            }
            catch (Exception) { return false; }
        }

        // --------------------------------------------------------------------------------------------- scan / import (worker thread)

        void BeginAliveScan()
        {
            if (AliveImportRunning || _aliveSourcePaths.Count == 0) return;
            _aliveFound?.Dispose(); _aliveFound = null; _aliveError = null; _aliveImporting = false;
            _aliveCancel = new CancellationTokenSource();
            var paths = _aliveSourcePaths.ToList(); var token = _aliveCancel.Token;
            string content = AliveContent();
            _aliveStatus = "Opening your sources…";
            _aliveTask = Task.Run(() =>
            {
                var found = AliveImporter.Scan(paths, s => _aliveStatus = s, token);
                // the menu pictures of levels imported before they existed, from these sources (no new import; AlivePictures)
                if (content != null)
                {
                    _aliveStatus = "Adding the levels' pictures";
                    try { AliveImporter.AddPictures(found, content, token); }
                    catch (OperationCanceledException) { throw; }
                    catch (Exception ex) { found.warnings.Add("Level pictures: " + ex.Message); }
                }
                _aliveFound = found;
            }, token);
            ShowAliveProgress();
        }

        void BeginAliveImport()
        {
            string content = AliveContent();
            if (AliveImportRunning || _aliveFound == null || content == null) return;
            var chosen = _aliveFound.levels.Where(l => l.recognized && _aliveChosen.Contains(l.level)).ToList();
            var bikes = _aliveFound.bikes.Where(b => b.Importable && _aliveChosen.Contains(BikeKey + b.aliveId)).ToList();
            if (chosen.Count == 0 && bikes.Count == 0) return;
            _aliveCancel = new CancellationTokenSource(); _aliveResults = null; _aliveBikeResults = null; _aliveError = null; _aliveImporting = true; _aliveProgress = null;
            _aliveOrder.Clear(); _aliveOrder.AddRange(chosen.Select(c => c.level));
            var token = _aliveCancel.Token;
            _aliveStatus = "Starting…";
            Debug.Log("[OpenReflex] Alive import: " + string.Join(", ", chosen.Select(c => c.level)) + " -> " + content);
            _aliveTask = Task.Run(() =>
            {
                _aliveResults = chosen.Count == 0 ? new List<AliveLevelResult>() : AliveImporter.Import(chosen, content, new AliveImportOptions(), p => Interlocked.Exchange(ref _aliveProgress, p), token);
                PackAliveLevels(content, token);
                if (bikes.Count > 0 && !token.IsCancellationRequested)
                {
                    var found = _aliveFound; int n = 0;
                    _aliveBikeResults = AliveBikes.Import(bikes, found, content, s => { _aliveStatus = s; Interlocked.Exchange(ref _aliveProgress, new AliveProgress { level = BikeKey, stage = s, done = n++, total = bikes.Count }); }, token);
                    foreach (var r in _aliveBikeResults) Debug.Log("[OpenReflex] Alive bike " + (r.ok ? "OK " : "FAILED ") + r.bike.Label + (r.ok ? $" -> {r.folder} ({r.surfaces} surfaces, {r.maps} maps, {r.bytes >> 10} KB, {r.seconds:0.0} s)" : ": " + r.error));
                }
            }, token);
            ShowAliveProgress();
        }

        /// A packed library stays packed: the importer writes the new levels loose (and retires a level's old archive), so they are packed here
        /// as the game import packs (ReflexDataPack: only the loose levels; the rest of the library is already packed) instead of the startup
        /// prompt asking at the next start. A library that was never packed (or REFLEX_IMPORT_LOOSE=1) keeps them loose. A pack that stops
        /// leaves the rest loose and working: GAME LIBRARY > PACK GAME FILES finishes it.
        void PackAliveLevels(string content, CancellationToken token)
        {
            if (_aliveResults == null || !_aliveResults.Any(r => r.ok) || token.IsCancellationRequested || ReflexDataPack.Running) return;
            if (!ReflexAssetImport.PackOnImport(null)) return;
            bool packed;
            try { packed = File.Exists(Path.Combine(content, "Assets.rxdata")) || Directory.Exists(Path.Combine(content, "Tracks")) && Directory.EnumerateFiles(Path.Combine(content, "Tracks"), "*.rxtrack").Any(); }
            catch (Exception) { packed = false; }
            if (!packed || !ReflexDataPack.Worth(content)) return;
            string level = _aliveResults.Last(r => r.ok).level;
            Interlocked.Exchange(ref _aliveProgress, new AliveProgress { level = level, stage = "Packing" });
            try
            {
                var r = ReflexDataPack.PackNow(content, pp => Interlocked.Exchange(ref _aliveProgress, new AliveProgress { level = level, stage = "Packing", done = (int)(pp.done >> 20), total = (int)Math.Max(1, pp.total >> 20) }), token);
                Debug.Log($"[pack] after the Alive import: {r.levels} levels, {r.files} files: {r.looseBytes >> 20} MB loose -> {r.packedBytes >> 20} MB packed in {r.seconds:0.0} s");
            }
            catch (OperationCanceledException) { Debug.Log("[pack] after the Alive import: stopped; what was packed stays packed, the rest loose"); }
            catch (Exception e) { Debug.LogWarning("[pack] after the Alive import: stopped, the rest stays loose (GAME LIBRARY > PACK GAME FILES): " + e); }
            finally { RxFiles.Reset(); }
        }

        /// The running scan / import (CANCEL).
        void ShowAliveProgress()
        {
            Clear(Screen.Install, "import");
            var box = OuUi.Div("mxa-panel mxa-install", _layer);
            box.name = "ou-alive-progress";
            OuUi.Text(_aliveImporting ? "IMPORTING ALIVE CONTENT" : "FINDING ALIVE LEVELS AND BIKES", "mxa-screen-title", box);
            OuUi.Text(_aliveImporting ? "Converting and installing into " + AliveContent() + ". Your source files are only read." : "Checking your sources (downloads: every hash of the package).", "mxa-install-text", box).style.whiteSpace = WhiteSpace.Normal;
            _aliveStatusLabel = OuUi.Text(_aliveStatus, "mxa-install-status", box); _aliveStatusLabel.style.whiteSpace = WhiteSpace.Normal;
            var bar = OuUi.Div("ou-progress", box);
            _aliveFill = OuUi.Div("ou-progress-fill", bar); OuTheme.Backdrop(_aliveFill);
            _aliveFill.style.width = Length.Percent(0);
            var cancel = OuUi.Button("CANCEL", "mxa-secondary", () => _aliveCancel?.Cancel(), box);
            cancel.name = "ou-alive-cancel";
            _nav.DefaultItem = () => cancel; _nav.OnCancel = () => { _aliveCancel?.Cancel(); return true; };
            Hints((OuUi.KeySubmit, "SELECT"), (OuUi.KeyCancel, "CANCEL"));
            _nav.Focus(cancel);
        }

        /// Called every frame (UpdateBuild): the worker's progress, then its result screen.
        void UpdateAliveImport()
        {
            if (_aliveTask == null) return;
            bool onScreen = Current == Screen.Install && _layer.Q("ou-alive-progress") != null;
            if (!_aliveTask.IsCompleted)
            {
                if (!onScreen || _aliveStatusLabel == null) return;
                var p = Interlocked.CompareExchange(ref _aliveProgress, null, null);
                if (_aliveImporting && p != null)
                {
                    var lv = _aliveFound?.levels.FirstOrDefault(l => l.level == p.level);
                    if (p.level == BikeKey)
                    {
                        _aliveStatusLabel.text = p.stage.ToUpperInvariant();
                        _aliveFill.style.width = Length.Percent((float)(100.0 * p.done / Math.Max(1, p.total)));
                        return;
                    }
                    if (p.stage == "Packing")
                    {   // (PackAliveLevels: the whole bar again, for the new levels' packing)
                        _aliveStatusLabel.text = "PACKING THE NEW LEVELS" + (p.total > 1 ? $"  ·  {p.Fraction * 100:0}%" : "");
                        _aliveFill.style.width = Length.Percent((float)(100.0 * p.Fraction));
                        return;
                    }
                    _aliveStatusLabel.text = (lv?.Label ?? p.level).ToUpperInvariant() + "  ·  " + p.stage.ToUpperInvariant() + (p.total > 1 ? $"  {p.Fraction * 100:0}%" : "");
                    int i = Math.Max(0, _aliveOrder.IndexOf(p.level));
                    double stageWeight = p.stage == "Converting" ? 0.85 * p.Fraction : p.stage == "Reading" ? 0 : p.stage == "Installing" ? 0.88 : p.stage == "Publishing" ? 0.9 + 0.1 * p.Fraction : 1;
                    _aliveFill.style.width = Length.Percent((float)(100.0 * (i + stageWeight) / Math.Max(1, _aliveOrder.Count)));
                }
                else _aliveStatusLabel.text = _aliveStatus;
                return;
            }
            var t = _aliveTask; _aliveTask = null; _aliveCancel?.Dispose(); _aliveCancel = null;
            if (t.IsFaulted) _aliveError = t.Exception?.GetBaseException();
            bool cancelled = t.IsCanceled || _aliveError is OperationCanceledException || (_aliveResults?.Any(r => r.error == "Cancelled") ?? false);
            if (_aliveImporting)
            {
                // the new levels: the registry, the event list and the shell's content read again (the hub's MX vs ATV Alive entry)
                AliveLevels.Reset();
                if (_aliveResults != null && _aliveResults.Any(r => r.ok)) { ReflexEvents.Refresh(); OpenContent.Reset(); }
                if (_aliveBikeResults != null && _aliveBikeResults.Any(r => r.ok)) BikeMods.Refresh();   // (the garage's MOD cards)
                if (_aliveError != null && !cancelled) Debug.LogException(_aliveError);
                foreach (var r in _aliveResults ?? new List<AliveLevelResult>()) Debug.Log("[OpenReflex] Alive import " + (r.ok ? "OK " : "FAILED ") + r);
                ShowAliveResults(cancelled);
            }
            else
            {
                if (_aliveError != null && !cancelled) Debug.LogWarning("[OpenReflex] Alive sources: " + _aliveError.Message);
                if (cancelled || _aliveFound == null) { _aliveNotice = cancelled ? "Cancelled." : "These sources could not be read: " + _aliveError?.Message; ShowAliveSources(); return; }
                if (_aliveFound.picturesAdded.Count > 0)
                {
                    Debug.Log("[OpenReflex] Alive pictures added: " + string.Join(", ", _aliveFound.picturesAdded));
                    AliveLevels.Reset();
                }
                _aliveChosen.Clear();
                foreach (var l in _aliveFound.levels) if (l.recognized && (l.kind.StartsWith("Free Ride", StringComparison.Ordinal))) _aliveChosen.Add(l.level);
                foreach (var b in _aliveFound.bikes) if (b.Importable) _aliveChosen.Add(BikeKey + b.aliveId);
                ShowAliveLevels();
            }
        }

        // --------------------------------------------------------------------------------------------- levels / results

        void ShowAliveLevels()
        {
            Clear(Screen.Install, "import");
            var box = OuUi.Div("mxa-panel mxa-install mxa-library", _layer);
            box.name = "ou-alive-levels";
            OuUi.Text(_aliveFound != null && _aliveFound.bikes.Count > 0 ? "ALIVE LEVELS & BIKES" : "ALIVE LEVELS", "mxa-screen-title", box);
            string content = AliveContent();
            var found = _aliveFound;
            OuUi.Text(found.levels.Count == 0 && found.bikes.Count == 0 ? "No MX vs ATV Alive level was found in these sources."
                      : $"{found.levels.Count} levels" + (found.bikes.Count > 0 ? $" and {found.bikes.Count} bikes" : "") + " found. Choose what to import; each level is installed as a Free Ride map of the MX vs ATV Alive category"
                        + (found.bikes.Count > 0 ? ", each bike as a MOD card in the garage." : "."),
                      "mxa-install-text", box).style.whiteSpace = WhiteSpace.Normal;
            OuUi.Text("Converted on this computer from your own files, as new files in " + (content ?? "the client's content folder") + ".", "mxa-note", box).style.whiteSpace = WhiteSpace.Normal;
            foreach (var w in found.warnings.Take(6)) OuUi.Text(w, "mxa-note", box).style.whiteSpace = WhiteSpace.Normal;
            if (found.picturesAdded.Count > 0)
                OuUi.Text($"Menu pictures added for {found.picturesAdded.Count} {(found.picturesAdded.Count == 1 ? "level" : "levels")} imported before (no new import needed).", "mxa-note", box).style.whiteSpace = WhiteSpace.Normal;
            var scroll = new ScrollView(ScrollViewMode.Vertical); scroll.AddToClassList("mxa-library-list"); box.Add(scroll);
            var buttons = OuUi.Div("mxa-install-buttons", box);
            VisualElement import = null;
            void Count()
            {
                int n = found.levels.Count(l => l.recognized && _aliveChosen.Contains(l.level)), nb = found.bikes.Count(b => b.Importable && _aliveChosen.Contains(BikeKey + b.aliveId));
                var parts = new List<string>();
                if (n > 0) parts.Add($"{n} {(n == 1 ? "LEVEL" : "LEVELS")}");
                if (nb > 0) parts.Add($"{nb} {(nb == 1 ? "BIKE" : "BIKES")}");
                var lbl = import?.Q<Label>(); if (lbl != null) lbl.text = parts.Count > 0 ? "IMPORT " + string.Join(" + ", parts) : "IMPORT";
                import?.SetEnabled(n + nb > 0 && content != null);
            }
            VisualElement first = null;
            foreach (var l in found.levels)
            {
                var row = OuUi.Div("ou-cache-row", scroll.contentContainer);
                var c = OuUi.Div("ou-cache-copy", row);
                var title = OuUi.Text("", "mxa-track-name ou-cache-name", c);
                bool installed = content != null && AliveImporter.Installed(content, l.level);
                string detail = string.Join("  ·  ", new[] { l.kind, l.packDisplay ?? l.volume.displayName, OpenWorldCache.SizeText(l.bytes) }.Where(x => !string.IsNullOrEmpty(x)))
                                + (installed ? "  ·  INSTALLED (IMPORT AGAIN REPLACES IT)" : "");
                OuUi.Text(l.recognized ? detail : l.problem, l.recognized ? "ou-card-sub ou-cache-sub" : "mxa-error", c).style.whiteSpace = WhiteSpace.Normal;
                var level = l.level;
                void Title() => title.text = (l.recognized ? (_aliveChosen.Contains(level) ? "☑  " : "☐  ") : "✕  ") + l.Label.ToUpperInvariant();
                Title();
                if (l.recognized)
                {
                    OuUi.Item(row, () => { if (!_aliveChosen.Remove(level)) _aliveChosen.Add(level); Title(); Count(); });
                    row.name = "ou-alive-level-" + level;
                    if (first == null) first = row;
                }
            }
            // the bikes: each rides as the Reflex bike of its class and brand (its physics), drawn with Alive's own model
            if (found.bikes.Count > 0)
            {
                OuUi.Text($"ALIVE BIKES  ·  {found.bikes.Count(b => b.Importable)} OF {found.bikes.Count} CAN BE IMPORTED  ·  EACH RIDES AS THE REFLEX VEHICLE OF ITS CLASS, THE 50CC, THE 650 AND THE ATV 1000 ON ALIVE'S OWN PHYSICS", "mxa-dir-header", scroll.contentContainer);
                foreach (var b in found.bikes)
                {
                    var row = OuUi.Div("ou-cache-row", scroll.contentContainer);
                    var c = OuUi.Div("ou-cache-copy", row);
                    var title = OuUi.Text("", "mxa-track-name ou-cache-name", c);
                    bool installed = content != null && AliveBikes.Installed(content, b);
                    string detail = b.Importable ? string.Join("  ·  ", new[] { b.ownClass != null ? "ITS OWN ALIVE PHYSICS (" + (b.ownClass == "mx50" ? "50CC" : b.ownClass == "atv1000" ? "ATV 1000" : "650") + ")" : "RIDES AS THE " + b.ridesAsName.ToUpperInvariant(), b.pack }.Where(x => !string.IsNullOrEmpty(x))) + (installed ? "  ·  INSTALLED (IMPORT AGAIN REPLACES IT)" : "")
                                  : b.problem;
                    OuUi.Text(detail, "ou-card-sub ou-cache-sub", c).style.whiteSpace = WhiteSpace.Normal;
                    string key = BikeKey + b.aliveId;
                    void Title() => title.text = (b.Importable ? (_aliveChosen.Contains(key) ? "☑  " : "☐  ") : "✕  ") + b.Label.ToUpperInvariant();
                    Title();
                    if (b.Importable)
                    {
                        OuUi.Item(row, () => { if (!_aliveChosen.Remove(key)) _aliveChosen.Add(key); Title(); Count(); });
                        row.name = "ou-alive-bike-" + b.aliveId;
                        if (first == null) first = row;
                    }
                }
            }
            import = OuUi.Button("IMPORT", "mxa-start", BeginAliveImport, buttons); import.name = "ou-alive-import";
            OuUi.Button("ALL", "mxa-secondary", () => { foreach (var l in found.levels.Where(x => x.recognized)) _aliveChosen.Add(l.level); foreach (var b in found.bikes.Where(x => x.Importable)) _aliveChosen.Add(BikeKey + b.aliveId); ShowAliveLevels(); }, buttons).name = "ou-alive-all";
            OuUi.Button("NONE", "mxa-secondary", () => { _aliveChosen.Clear(); ShowAliveLevels(); }, buttons).name = "ou-alive-none";
            OuUi.Button("BACK", "mxa-secondary", ShowAliveSources, buttons);
            Count();
            _nav.OnCancel = () => { ShowAliveSources(); return true; };
            _nav.DefaultItem = () => import.enabledInHierarchy ? import : first ?? import;
            Hints((OuUi.KeySubmit, "CHOOSE"), (OuUi.KeyCancel, "BACK"));
            _nav.Focus(first ?? import);
        }

        void ShowAliveResults(bool cancelled)
        {
            Clear(Screen.Install, "import");
            var box = OuUi.Div("mxa-panel mxa-install mxa-library", _layer);
            box.name = "ou-alive-results";
            OuUi.Text("ALIVE IMPORT", "mxa-screen-title", box);
            var results = _aliveResults ?? new List<AliveLevelResult>();
            var bikeResults = _aliveBikeResults ?? new List<AliveBikeResult>();
            int ok = results.Count(r => r.ok), okb = bikeResults.Count(r => r.ok);
            string head = cancelled ? "Import cancelled. The level in progress was removed; levels finished before stay installed."
                        : _aliveError != null ? "Import stopped: " + _aliveError.Message
                        : ok == results.Count && okb == bikeResults.Count ? string.Join(" ", new[] { results.Count > 0 ? $"{ok} {(ok == 1 ? "level" : "levels")} installed. They are listed under MX vs ATV Alive." : null,
                                                                                                    bikeResults.Count > 0 ? $"{okb} {(okb == 1 ? "bike" : "bikes")} installed: GARAGE > VEHICLE lists each as a MOD card under the bike it rides as." : null }.Where(x => x != null))
                        : $"{ok} of {results.Count} levels, {okb} of {bikeResults.Count} bikes installed.";
            OuUi.Text(head, cancelled || _aliveError != null || ok < results.Count || okb < bikeResults.Count ? "mxa-error" : "mxa-install-text", box).style.whiteSpace = WhiteSpace.Normal;
            var scroll = new ScrollView(ScrollViewMode.Vertical); scroll.AddToClassList("mxa-library-list"); box.Add(scroll);
            foreach (var r in results)
            {
                var row = OuUi.Div("ou-cache-row", scroll.contentContainer);
                var c = OuUi.Div("ou-cache-copy", row);
                OuUi.Text((r.ok ? "✓  " : "✕  ") + r.displayName.ToUpperInvariant(), "mxa-track-name ou-cache-name", c);
                OuUi.Text(r.ok ? $"{r.resources} RESOURCES  ·  {r.starts} FREE RIDE STARTS  ·  {r.pending} NOT CONVERTED YET  ·  {OpenWorldCache.SizeText(r.bytes)}  ·  {r.seconds:0} S"
                               : r.error, r.ok ? "ou-card-sub ou-cache-sub" : "mxa-error", c).style.whiteSpace = WhiteSpace.Normal;
            }
            foreach (var r in bikeResults)
            {
                var row = OuUi.Div("ou-cache-row", scroll.contentContainer);
                var c = OuUi.Div("ou-cache-copy", row);
                OuUi.Text((r.ok ? "✓  " : "✕  ") + r.bike.Label.ToUpperInvariant(), "mxa-track-name ou-cache-name", c);
                OuUi.Text(r.ok ? $"BIKE  ·  {(r.bike.ownClass != null ? "ITS OWN ALIVE PHYSICS" : "RIDES AS THE " + r.bike.ridesAsName.ToUpperInvariant())}  ·  {r.surfaces} SURFACES  ·  {r.maps} MAPS  ·  {OpenWorldCache.SizeText(r.bytes)}"
                               : r.error, r.ok ? "ou-card-sub ou-cache-sub" : "mxa-error", c).style.whiteSpace = WhiteSpace.Normal;
            }
            var buttons = OuUi.Div("mxa-install-buttons", box);
            var done = OuUi.Button("DONE", "mxa-start", LeaveAlive, buttons);
            done.name = "ou-alive-done";
            OuUi.Button("MORE LEVELS", "mxa-secondary", () => { if (_aliveFound != null) ShowAliveLevels(); else ShowAliveSources(); }, buttons);
            _nav.OnCancel = () => { LeaveAlive(); return true; };
            _nav.DefaultItem = () => done;
            Hints((OuUi.KeySubmit, "SELECT"), (OuUi.KeyCancel, "BACK"));
            _nav.Focus(done);
        }
    }
}

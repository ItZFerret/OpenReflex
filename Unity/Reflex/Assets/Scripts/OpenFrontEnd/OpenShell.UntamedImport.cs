// GAME LIBRARY  ·  MX vs ATV UNTAMED: the stadiums of the player's own Untamed disc (Xbox 360) imported as custom tracks by the in-client
// converter (Content/UntamedImport: no Python, no native code; plan: core/plan/UNTAMED_TRACKS.md).
//   sources   the folder / file browser of the Alive import (a gamepad works): the disc image (.iso) or the folder the disc was extracted
//             into (the one holding Database and AI); a file adds it, ADD THIS FOLDER a folder
//   tracks    FIND TRACKS reads them on a worker thread (UntamedImporter.Scan: each stadium's files against the supported disc; then what
//             the converter needs of the Reflex game files) and lists the 12 stadiums: choose them, IMPORT n TRACKS. Ready ones are
//             chosen; one installed already (imported before: Mods/Tracks/Untamed/<short>; or by hand: <short>-v<n>) is marked and
//             not chosen: choosing it imports it again (a hand-installed copy is never changed)
//   import    the chosen stadiums converted and installed one after another on a worker thread (UntamedImporter.Import: the current
//             track and its step, a bar for the track and one for the whole import, CANCEL), then each stadium's result and where the
//             tracks are; TrackMods.Refresh() so FREE RIDE / RACE > MX vs ATV Untamed lists them at once. The disc and the Reflex game
//             files are only read; the tracks are new files in Mods/Tracks/Untamed.
//   leaving   the shell destroyed while a scan or an import runs (a ride starts, the game quits) cancels it (StopUntamedImport); what an
//             import that did not end left in Mods/Tracks is removed when the shell first starts (CleanUntamedStaging) and by FIND TRACKS
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using Reflex.Port.Content;
using Reflex.Port.Content.UntamedImport;
using UnityEngine;
using UnityEngine.UIElements;

namespace Reflex.Port.OpenFrontEnd
{
    public sealed partial class OpenShell
    {
        readonly List<string> _untamedSourcePaths = new List<string>();
        string _untamedDir;
        UntamedSources _untamedFound;
        /// The stadiums chosen for the import (their disc level names).
        readonly HashSet<string> _untamedChosen = new HashSet<string>(StringComparer.Ordinal);
        Task _untamedTask; CancellationTokenSource _untamedCancel;
        string _untamedStatus = ""; float _untamedFraction;
        UntamedProgress _untamedProgress;
        /// The progress screen: the step, the bar of the scan / of the track in progress; importing, the whole import's line and bar.
        Label _untamedStatusLabel, _untamedAllLabel; VisualElement _untamedFill, _untamedAllFill;
        Exception _untamedError; string _untamedNotice;
        bool _untamedImporting; UntamedImportResult _untamedResult;
        /// Why the Reflex game files can't give the converter what it needs (null: they can): for every stadium, and for the EnduroCross
        /// arenas besides (UntamedImporter.LibraryProblem, asked by the scan's worker).
        string _untamedLibraryProblem, _untamedArenaProblem;
        bool UntamedBusy => _untamedTask != null && !_untamedTask.IsCompleted;
        public bool UntamedScanRunning => UntamedBusy && !_untamedImporting;
        public bool UntamedImportRunning => UntamedBusy && _untamedImporting;
        /// The last scan's stadiums (null before / during a scan or an import; tests read them).
        public IReadOnlyList<UntamedTrackSource> UntamedTracks => UntamedBusy ? null : _untamedFound?.tracks;
        /// The last import's per-stadium results (null before / during one; tests read them).
        public IReadOnlyList<UntamedTrackResult> UntamedResults => UntamedBusy ? null : _untamedResult?.tracks;

        /// The library card (ShowLibrary): the Untamed tracks converted before, CHOOSE UNTAMED DISC.
        void UntamedLibraryCard(VisualElement list)
        {
            var card = OuUi.Div("mxa-library-card", list);
            card.style.flexWrap = Wrap.Wrap;
            var copy = OuUi.Div("mxa-library-copy", card);
            copy.style.flexGrow = 1; copy.style.flexShrink = 1; copy.style.minWidth = 240;
            OuUi.Text("OPTIONAL  ·  XBOX 360 DISC", "mxa-library-platform", copy);
            OuUi.Text("MX vs ATV UNTAMED", "mxa-track-title", copy);
            OuUi.Text("Import the 10 supercross and 2 EnduroCross stadiums of your own MX vs ATV Untamed disc (the disc image, .iso, or the extracted disc " +
                      "folder) as custom tracks. Your files are only read.",
                      "mxa-library-description", copy).style.whiteSpace = WhiteSpace.Normal;
            var installed = new List<UntamedKnown>();
            try { installed = UntamedImporter.Installed(TrackMods.Folder); } catch (Exception) { }
            OuUi.Text(installed.Count > 0 ? $"{installed.Count} UNTAMED {(installed.Count == 1 ? "TRACK" : "TRACKS")} INSTALLED  ·  " + string.Join(", ", installed.Select(k => k.name.ToUpperInvariant()))
                                          : "NO UNTAMED TRACKS INSTALLED", "mxa-library-state", copy).style.whiteSpace = WhiteSpace.Normal;
            var actions = OuUi.Div("ou-library-actions", card);
            actions.style.flexDirection = FlexDirection.Column; actions.style.width = 300; actions.style.flexShrink = 0;
            var b = OuUi.Button(UntamedImportRunning ? "IMPORT IN PROGRESS" : UntamedScanRunning ? "CHECK IN PROGRESS" : "CHOOSE UNTAMED DISC", "mxa-secondary mxa-library-action", () => { if (UntamedBusy) ShowUntamedProgress(); else ShowUntamedSources(); }, actions);
            b.name = "ou-library-untamed-import";
            b.style.width = 300; b.style.height = 54; b.style.marginTop = 6;
            var label = b.Q<Label>(); if (label != null) { label.style.fontSize = 26; label.style.letterSpacing = 2; }
        }

        // --------------------------------------------------------------------------------------------- sources

        public void ShowUntamedSources()
        {
            Clear(Screen.Install, "import");
            SetBackground(null);
            DriveScanner.Default.BeginRefresh();
            if (string.IsNullOrEmpty(_untamedDir) || !ReflexFs.Within(2500, () => Directory.Exists(_untamedDir), out _))
            {
                string downloads = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.UserProfile), "Downloads");
                _untamedDir = ReflexFs.Within(2500, () => Directory.Exists(downloads), out _) ? downloads : Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);
            }
            var box = OuUi.Div("mxa-panel mxa-install", _layer);
            box.name = "ou-untamed-sources";
            OuUi.Text("MX vs ATV UNTAMED", "mxa-screen-title", box);
            OuUi.Text("Choose your MX vs ATV Untamed disc image (.iso), or the folder you extracted the disc into (the one that holds Database and AI). " +
                      "Each stadium is checked against the supported disc; then you choose the ones to import. Nothing is changed in your files.", "mxa-install-text", box).style.whiteSpace = WhiteSpace.Normal;
            var chosen = OuUi.Text("", "mxa-note", box); chosen.style.whiteSpace = WhiteSpace.Normal;
            void ShowChosen() => chosen.text = _untamedSourcePaths.Count == 0 ? "No source added yet." : "SOURCES:  " + string.Join("  ·  ", _untamedSourcePaths.Select(p => ReflexPaths.LabelOf(p)));
            ShowChosen();
            var field = new TextField { value = _untamedDir }; field.name = "ou-untamed-path"; field.AddToClassList("mxa-path"); box.Add(field);
            var status = OuUi.Text(_untamedNotice ?? "", "mxa-install-status", box); status.style.whiteSpace = WhiteSpace.Normal;
            _untamedNotice = null;
            var sv = new ScrollView(ScrollViewMode.Vertical); sv.AddToClassList("mxa-dir-list"); box.Add(sv);
            var buttons = OuUi.Div("mxa-install-buttons", box);
            VisualElement find = null;

            void Add(string path)
            {
                try { path = Path.GetFullPath(path); }
                catch (Exception e) when (e is ArgumentException || e is NotSupportedException || e is PathTooLongException) { status.text = "That path can't be used: " + e.Message; return; }
                if (!_untamedSourcePaths.Any(p => ReflexPaths.SamePath(p, path))) _untamedSourcePaths.Add(path);
                status.text = "Added " + ReflexPaths.LabelOf(path);
                ShowChosen(); find?.SetEnabled(true);
            }
            void Refresh(string dir)
            {
                _untamedDir = dir; field.SetValueWithoutNotify(dir);
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
                        var dd = d; bool untamed = LooksLikeUntamedFolder(d);
                        var it = OuUi.Item(OuUi.Text(name + (untamed ? "   ✓ UNTAMED" : ""), "mxa-dir", sv.contentContainer), () => Refresh(dd));
                        it.EnableInClassList("mxa-ok", untamed);
                    }
                    int shown = 0;
                    foreach (var f in Directory.GetFiles(dir).OrderBy(x => x, StringComparer.OrdinalIgnoreCase))
                    {
                        if (shown >= 200) break;
                        string name = Path.GetFileName(f);
                        if (name.StartsWith(".") || !name.EndsWith(".iso", StringComparison.OrdinalIgnoreCase)) continue;
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
                status.text = why; field.SetValueWithoutNotify(_untamedDir);
            });
            var add = OuUi.Button("ADD THIS FOLDER", "mxa-secondary", () => Add(_untamedDir), buttons); add.name = "ou-untamed-add";
            find = OuUi.Button("FIND TRACKS", "mxa-start", BeginUntamedScan, buttons); find.name = "ou-untamed-find";
            find.SetEnabled(_untamedSourcePaths.Count > 0);
            OuUi.Button("CLEAR", "mxa-secondary", () => { _untamedSourcePaths.Clear(); ShowChosen(); find.SetEnabled(false); status.text = ""; }, buttons);
            OuUi.Button("BACK", "mxa-secondary", LeaveUntamed, buttons);
            Refresh(_untamedDir);
            DrivesChanged();
            ScreenUpdate = () => { if (DrivesChanged()) Refresh(_untamedDir); };
            _nav.OnCancel = () => { LeaveUntamed(); return true; };
            _nav.DefaultItem = () => _untamedSourcePaths.Count > 0 ? find : add;
            Hints((OuUi.KeySubmit, "OPEN / ADD"), (OuUi.KeyCancel, "BACK"));
            _nav.Focus(_untamedSourcePaths.Count > 0 ? find : add);
        }

        /// Back to the library: the sources found are closed (their files are not held open).
        void LeaveUntamed()
        {
            if (UntamedBusy) return;
            _untamedFound?.Dispose(); _untamedFound = null;
            ShowLibrary(_libraryFromMain);
        }

        /// The shell goes (a ride starts, the game quits) while a scan or an import runs: cancelled, so no stadium is converted behind a
        /// ride (OnDestroy). The import ends within moments, removes the track in progress and its staging folder; the tracks finished
        /// before stay. The sources are closed when the worker has ended (no screen is left to do it).
        void StopUntamedImport()
        {
            var task = _untamedTask;
            try { _untamedCancel?.Cancel(); } catch (ObjectDisposedException) { }
            if (task == null || task.IsCompleted) { _untamedFound?.Dispose(); _untamedFound = null; return; }
            task.ContinueWith(_ => { try { _untamedFound?.Dispose(); } catch (Exception) { } }, TaskScheduler.Default);
        }

        static bool s_untamedStagingChecked;
        /// Once a run, when the shell first starts: what an Untamed import that did not end left in Mods/Tracks (the game was closed, or
        /// crashed, while it ran: a hidden staging folder of up to about 210 MB, and in it the track a re-import had just set aside) is
        /// removed on a worker thread, that track put back first; the browser reads the folder again when it is opened. A staging
        /// folder whose lock is held belongs to an import that is running and stays (UntamedImporter.CleanStaging).
        void CleanUntamedStaging()
        {
            if (s_untamedStagingChecked) return;
            s_untamedStagingChecked = true;
            string tracks;
            try { tracks = TrackMods.Folder; } catch (Exception) { return; }   // (main thread: the client root comes from Unity's paths)
            if (string.IsNullOrEmpty(tracks)) return;
            Task.Run(() =>
            {
                try { UntamedImporter.CleanStaging(tracks, s => Debug.Log("[OpenReflex] " + s)); }
                catch (Exception ex) { Debug.LogWarning("[OpenReflex] Untamed import, staging folders: " + ex.Message); }
            });
        }

        /// An extracted Untamed disc folder: Database with the stadium levels, and AI with their race lines (cheap: names only). Not the
        /// Database folder itself: the race lines are next to it.
        static bool LooksLikeUntamedFolder(string d)
        {
            try
            {
                string db = Path.Combine(d, "Database");
                return Directory.Exists(db) && Directory.EnumerateFiles(db, "*_STADIUM.xenon.level").Any() && Directory.Exists(Path.Combine(d, "AI"));
            }
            catch (Exception) { return false; }
        }

        // --------------------------------------------------------------------------------------------- scan / import (worker thread)

        void BeginUntamedScan()
        {
            if (UntamedBusy || _untamedSourcePaths.Count == 0) return;
            _untamedFound?.Dispose(); _untamedFound = null; _untamedError = null; _untamedImporting = false;
            _untamedLibraryProblem = null; _untamedArenaProblem = null;
            _untamedCancel = new CancellationTokenSource();
            var paths = _untamedSourcePaths.ToList(); var token = _untamedCancel.Token;
            string tracks = TrackMods.Folder, library = ReflexInstall.Dir;   // (main thread: the client root and the game folder come from Unity's paths and settings)
            _untamedStatus = "Opening your sources…"; _untamedFraction = 0;
            _untamedTask = Task.Run(() =>
            {
                // what an import that did not end left in Mods/Tracks (the game closed, a crash): its staging folder goes now
                try { UntamedImporter.CleanStaging(tracks, s => Debug.Log("[OpenReflex] " + s)); }
                catch (Exception ex) { Debug.LogWarning("[OpenReflex] Untamed import, staging folders: " + ex.Message); }
                var found = UntamedImporter.Scan(paths, tracks, (s, f) => { _untamedStatus = s; _untamedFraction = f; }, token);
                // what the converter needs of the Reflex game files, so the list says it before IMPORT (the import checks it again)
                _untamedStatus = "Checking your Reflex game files";
                try
                {
                    _untamedLibraryProblem = UntamedImporter.LibraryProblem(library, found.tracks.Where(t => t.known.mode != "ec").Select(t => t.known).ToList());
                    if (_untamedLibraryProblem == null) _untamedArenaProblem = UntamedImporter.LibraryProblem(library, found.tracks.Select(t => t.known).ToList());
                }
                catch (OperationCanceledException) { found.Dispose(); throw; }
                catch (Exception ex) { _untamedLibraryProblem = "Your MX vs ATV Reflex game files can't be read: " + ex.Message; }
                _untamedFound = found;
            }, token);
            ShowUntamedProgress();
        }

        /// Why a stadium found can't be chosen for the import (null: it can).
        string UntamedBlocked(UntamedTrackSource t)
        {
            if (!t.Ok) return t.problem;
            if (_untamedLibraryProblem != null) return "Needs your MX vs ATV Reflex game files (see above)";
            if (t.known.mode == "ec" && _untamedArenaProblem != null) return _untamedArenaProblem;
            // something of the player's own where the track goes (Untamed/<short> without the importer's manifest, whatever it holds and
            // whatever copies are next to it; a file; a link): never replaced. The import's own test (UntamedImporter.DestinationProblem)
            return t.inTheWay;
        }

        void BeginUntamedImport()
        {
            if (UntamedBusy || _untamedFound == null) return;
            string tracks = TrackMods.Folder, library = ReflexInstall.Dir;   // (main thread)
            var found = _untamedFound;
            var chosen = found.tracks.Where(t => UntamedBlocked(t) == null && _untamedChosen.Contains(t.known.level)).ToList();
            if (chosen.Count == 0 || tracks == null || library == null) return;
            // a track ridden since the start keeps its package file open: closed, so a re-import can replace it (TrackMods opens it again)
            try { TrackMods.Release(UntamedImporter.TracksSubfolder + "/"); } catch (Exception e) { Debug.LogWarning("[OpenReflex] Untamed import: " + e.Message); }
            _untamedCancel = new CancellationTokenSource(); _untamedResult = null; _untamedError = null; _untamedImporting = true; _untamedProgress = null;
            var token = _untamedCancel.Token;
            // choosing a stadium imported before asks for the re-import
            var options = new UntamedImportOptions { replace = true, freeBytes = ReflexAssetImport.FreeBytes, log = s => Debug.Log("[OpenReflex] " + s) };
            _untamedStatus = "Starting…"; _untamedFraction = 0;
            Debug.Log("[OpenReflex] Untamed import: " + string.Join(", ", chosen.Select(c => c.known.level)) + " -> " + Path.Combine(tracks, UntamedImporter.TracksSubfolder) + " (Reflex game files: " + library + ")");
            _untamedTask = Task.Run(() =>
            {
                _untamedResult = UntamedImporter.Import(found, chosen, tracks, library, options, p => Interlocked.Exchange(ref _untamedProgress, p), token);
            }, token);
            ShowUntamedProgress();
        }

        /// The running scan / import (CANCEL).
        void ShowUntamedProgress()
        {
            Clear(Screen.Install, "import");
            var box = OuUi.Div("mxa-panel mxa-install", _layer);
            box.name = "ou-untamed-progress";
            OuUi.Text(_untamedImporting ? "IMPORTING UNTAMED TRACKS" : "CHECKING YOUR UNTAMED DISC", "mxa-screen-title", box);
            string tracks = TrackMods.Folder;
            OuUi.Text(_untamedImporting ? "Converting each stadium on this computer and installing it into " + (tracks != null ? Path.Combine(tracks, UntamedImporter.TracksSubfolder) : "Mods/Tracks/" + UntamedImporter.TracksSubfolder) +
                                          ". Your disc and your Reflex game files are only read. CANCEL removes the track in progress; tracks finished before stay."
                                        : "Reading each stadium's files and checking them against the supported disc. Your source files are only read.", "mxa-install-text", box).style.whiteSpace = WhiteSpace.Normal;
            _untamedStatusLabel = OuUi.Text(_untamedStatus, "mxa-install-status", box); _untamedStatusLabel.style.whiteSpace = WhiteSpace.Normal;
            var bar = OuUi.Div("ou-progress", box);
            bar.name = "ou-untamed-bar";
            _untamedFill = OuUi.Div("ou-progress-fill", bar); OuTheme.Backdrop(_untamedFill);
            _untamedFill.style.width = Length.Percent(0);
            _untamedAllLabel = null; _untamedAllFill = null;
            if (_untamedImporting)
            {   // the whole import under the track in progress
                _untamedAllLabel = OuUi.Text("", "mxa-note", box); _untamedAllLabel.style.whiteSpace = WhiteSpace.Normal;
                var all = OuUi.Div("ou-progress", box);
                all.name = "ou-untamed-bar-all";
                _untamedAllFill = OuUi.Div("ou-progress-fill", all); OuTheme.Backdrop(_untamedAllFill);
                _untamedAllFill.style.width = Length.Percent(0);
            }
            var cancel = OuUi.Button("CANCEL", "mxa-secondary", () => _untamedCancel?.Cancel(), box);
            cancel.name = "ou-untamed-cancel";
            _nav.DefaultItem = () => cancel; _nav.OnCancel = () => { _untamedCancel?.Cancel(); return true; };
            Hints((OuUi.KeySubmit, "SELECT"), (OuUi.KeyCancel, "CANCEL"));
            _nav.Focus(cancel);
        }

        /// Called every frame (UpdateBuild): the worker's progress, then the stadiums found or the import's results.
        void UpdateUntamedImport()
        {
            if (_untamedTask == null) return;
            if (!_untamedTask.IsCompleted)
            {
                if (Current != Screen.Install || _layer.Q("ou-untamed-progress") == null || _untamedStatusLabel == null || _untamedFill == null) return;
                var p = _untamedImporting ? Interlocked.CompareExchange(ref _untamedProgress, null, null) : null;
                if (p != null)
                {
                    _untamedStatusLabel.text = (p.name ?? p.level ?? "").ToUpperInvariant() + "  ·  " + (p.stage ?? "").ToUpperInvariant();
                    _untamedFill.style.width = Length.Percent(Mathf.Clamp01(p.track) * 100f);
                    if (_untamedAllLabel != null) _untamedAllLabel.text = $"TRACK {p.index + 1} OF {p.count}  ·  {Mathf.Clamp01(p.overall) * 100f:0}% OF THE IMPORT";
                    if (_untamedAllFill != null) _untamedAllFill.style.width = Length.Percent(Mathf.Clamp01(p.overall) * 100f);
                }
                else
                {
                    _untamedStatusLabel.text = _untamedStatus;
                    _untamedFill.style.width = Length.Percent(Mathf.Clamp01(_untamedFraction) * 100f);
                }
                return;
            }
            var t = _untamedTask; _untamedTask = null; _untamedCancel?.Dispose(); _untamedCancel = null;
            if (t.IsFaulted) _untamedError = t.Exception?.GetBaseException();
            bool cancelled = t.IsCanceled || _untamedError is OperationCanceledException;
            if (_untamedImporting)
            {
                _untamedImporting = false;
                if (_untamedResult != null && _untamedResult.cancelled) cancelled = true;
                // the new tracks: Mods/Tracks is read again, so FREE RIDE / RACE > MX vs ATV Untamed lists them without a restart (the shell's
                // event list follows TrackMods.Version)
                try { TrackMods.Refresh(); } catch (Exception e) { Debug.LogWarning("[OpenReflex] Untamed import, custom tracks: " + e.Message); }
                if (_untamedError != null && !cancelled)
                {
                    if (_untamedError is UntamedImportException) Debug.LogWarning("[OpenReflex] Untamed import: " + _untamedError.Message);
                    else Debug.LogException(_untamedError);
                }
                if (_untamedResult != null)
                    foreach (var r in _untamedResult.tracks) Debug.Log("[OpenReflex] Untamed import " + r + (r.Ok ? $" ({r.bytes >> 20} MB, {r.seconds:0.0} s, tested build: {r.testedBuild})" : ""));
                // (the conversion needs over 1 GB for a while; outside Unity the client's Mono kept it reserved afterwards: the log says what
                // it is here, core/plan/UNTAMED_TRACKS.md "Open")
                Debug.Log($"[OpenReflex] Untamed import: managed heap {UnityEngine.Profiling.Profiler.GetMonoUsedSizeLong() >> 20} MB used of {UnityEngine.Profiling.Profiler.GetMonoHeapSizeLong() >> 20} MB reserved");
                RefreshUntamedInstalled();
                ShowUntamedResults(cancelled);
                return;
            }
            if (_untamedError != null && !cancelled) Debug.LogWarning("[OpenReflex] Untamed sources: " + _untamedError.Message);
            if (cancelled || _untamedFound == null)
            {
                _untamedFound?.Dispose(); _untamedFound = null;
                _untamedNotice = cancelled ? "Cancelled." : "These sources could not be read: " + _untamedError?.Message;
                ShowUntamedSources(); return;
            }
            foreach (var tr in _untamedFound.tracks) Debug.Log("[OpenReflex] Untamed " + tr.known.level + ": " + (tr.Ok ? "ready" : tr.problem) + (tr.installed != null ? " (installed: " + tr.installed + ")" : ""));
            if (_untamedLibraryProblem != null) Debug.Log("[OpenReflex] Untamed import not possible: " + _untamedLibraryProblem);
            else if (_untamedArenaProblem != null) Debug.Log("[OpenReflex] Untamed EnduroCross arenas not possible: " + _untamedArenaProblem);
            // chosen: every stadium that can be imported and is not installed yet (one installed is chosen by the player: a re-import)
            _untamedChosen.Clear();
            // ... and one imported by an older converter whose files this one writes differently (an update: Fairview and Lincoln after the water-glare round)
            foreach (var tr in _untamedFound.tracks) { try { tr.outdated = UntamedImporter.Outdated(TrackMods.Folder, tr.known); } catch (Exception) { tr.outdated = false; } }
            foreach (var tr in _untamedFound.tracks) if (UntamedBlocked(tr) == null && (tr.installed == null || tr.outdated)) _untamedChosen.Add(tr.known.level);
            ShowUntamedTracks();
        }

        /// After an import: what is installed now, for the list (MORE TRACKS); the stadiums just imported are not chosen again.
        void RefreshUntamedInstalled()
        {
            if (_untamedFound == null) return;
            string folder = TrackMods.Folder;
            foreach (var tr in _untamedFound.tracks)
            {
                tr.installed = UntamedImporter.InstalledFolder(folder, tr.known);
                tr.imported = UntamedImporter.ImportedFolder(folder, tr.known);
                try { tr.outdated = UntamedImporter.Outdated(folder, tr.known); } catch (Exception) { tr.outdated = false; }
                tr.handInstalled = UntamedImporter.HandInstalled(folder, tr.known);
                tr.inTheWay = UntamedImporter.DestinationProblem(folder, tr.known);
            }
            if (_untamedResult != null)
                foreach (var r in _untamedResult.tracks) if (r.Ok) _untamedChosen.Remove(r.level);
        }

        // --------------------------------------------------------------------------------------------- tracks / results

        static string UntamedKindLabel(UntamedKnown k) => k.mode == "ec" ? "ENDUROCROSS (RACES AS SUPERCROSS)" : "SUPERCROSS";

        void ShowUntamedTracks()
        {
            Clear(Screen.Install, "import");
            var box = OuUi.Div("mxa-panel mxa-install mxa-library", _layer);
            box.name = "ou-untamed-tracks";
            OuUi.Text("UNTAMED STADIUMS", "mxa-screen-title", box);
            var found = _untamedFound;
            string tracksFolder = TrackMods.Folder;
            string into = tracksFolder != null ? Path.Combine(tracksFolder, UntamedImporter.TracksSubfolder) : "Mods/Tracks/" + UntamedImporter.TracksSubfolder;
            int ready = found.Ready.Count();
            OuUi.Text(found.tracks.Count == 0 ? "No MX vs ATV Untamed stadium was found in these sources."
                      : $"{ready} of {UntamedImporter.StadiumCount} stadiums found on your disc are ready. Choose the ones to import; each is converted on this computer and installed as a custom supercross track (about 50 MB each).",
                      found.tracks.Count == 0 ? "mxa-error" : "mxa-install-text", box).style.whiteSpace = WhiteSpace.Normal;
            OuUi.Text("Converted from your own disc and your Reflex game files, as new files in " + into + ".", "mxa-note", box).style.whiteSpace = WhiteSpace.Normal;
            if (_untamedLibraryProblem != null && found.tracks.Count > 0) OuUi.Text(_untamedLibraryProblem, "mxa-error", box).style.whiteSpace = WhiteSpace.Normal;
            if (tracksFolder == null) OuUi.Text("The custom tracks folder (Mods/Tracks) was not found.", "mxa-error", box).style.whiteSpace = WhiteSpace.Normal;
            foreach (var s in found.sources) OuUi.Text(s, "mxa-note", box).style.whiteSpace = WhiteSpace.Normal;
            foreach (var w in found.warnings.Take(6)) OuUi.Text(w, "mxa-note", box).style.whiteSpace = WhiteSpace.Normal;
            var scroll = new ScrollView(ScrollViewMode.Vertical); scroll.AddToClassList("mxa-library-list"); box.Add(scroll);
            var buttons = OuUi.Div("mxa-install-buttons", box);
            VisualElement import = null, first = null;
            void Count()
            {
                int n = found.tracks.Count(t => UntamedBlocked(t) == null && _untamedChosen.Contains(t.known.level));
                var lbl = import?.Q<Label>(); if (lbl != null) lbl.text = n > 0 ? $"IMPORT {n} {(n == 1 ? "TRACK" : "TRACKS")}" : "IMPORT";
                import?.SetEnabled(n > 0 && tracksFolder != null);
            }
            foreach (var t in found.tracks)
            {
                var row = OuUi.Div("ou-cache-row", scroll.contentContainer);
                row.name = "ou-untamed-track-" + t.known.shortName;
                var c = OuUi.Div("ou-cache-copy", row);
                var title = OuUi.Text("", "mxa-track-name ou-cache-name", c);
                string blocked = UntamedBlocked(t);
                // installed already: marked, and not chosen unless the player chooses it
                string state = t.imported != null && t.outdated ? $"IMPORTED BY AN OLDER VERSION (Mods/Tracks/{t.imported})  ·  IMPORT IT AGAIN TO UPDATE IT (REPLACES IT)"
                             : t.imported != null ? $"IMPORTED (Mods/Tracks/{t.imported})  ·  CHOOSE IT TO IMPORT IT AGAIN (REPLACES IT)"
                             : t.handInstalled.Count > 0 ? $"INSTALLED BY HAND (Mods/Tracks/{t.handInstalled[0]})  ·  CHOOSE IT TO IMPORT A NEW COPY; THE OLDER ONE STAYS AS IT IS"
                             : "READY";
                OuUi.Text(blocked ?? $"{UntamedKindLabel(t.known)}  ·  {OpenWorldCache.SizeText(t.bytes)}  ·  {state}",
                          blocked == null ? "ou-card-sub ou-cache-sub" : "mxa-error", c).style.whiteSpace = WhiteSpace.Normal;
                var source = t; string level = t.known.level;
                void Title() => title.text = (blocked == null ? (_untamedChosen.Contains(level) ? "☑  " : "☐  ") : "✕  ") + source.Label.ToUpperInvariant();
                Title();
                if (blocked == null)
                {
                    OuUi.Item(row, () => { if (!_untamedChosen.Remove(level)) _untamedChosen.Add(level); Title(); Count(); });
                    if (first == null) first = row;
                }
            }
            if (found.unsupported.Count > 0)
                OuUi.Text("NOT SUPPORTED YET:  " + string.Join(", ", found.unsupported.Select(l => l.EndsWith("_STADIUM", StringComparison.OrdinalIgnoreCase) ? l.Substring(0, l.Length - "_STADIUM".Length) : l)),
                          "mxa-note", scroll.contentContainer).style.whiteSpace = WhiteSpace.Normal;
            import = OuUi.Button("IMPORT", "mxa-start", BeginUntamedImport, buttons); import.name = "ou-untamed-import";
            OuUi.Button("ALL", "mxa-secondary", () => { foreach (var t in found.tracks) if (UntamedBlocked(t) == null) _untamedChosen.Add(t.known.level); ShowUntamedTracks(); }, buttons).name = "ou-untamed-all";
            OuUi.Button("NONE", "mxa-secondary", () => { _untamedChosen.Clear(); ShowUntamedTracks(); }, buttons).name = "ou-untamed-none";
            OuUi.Button("BACK", "mxa-secondary", ShowUntamedSources, buttons).name = "ou-untamed-other";
            Count();
            _nav.OnCancel = () => { ShowUntamedSources(); return true; };
            _nav.DefaultItem = () => import.enabledInHierarchy ? import : first ?? import;
            Hints((OuUi.KeySubmit, "CHOOSE"), (OuUi.KeyCancel, "BACK"));
            _nav.Focus(first ?? import);
        }

        void ShowUntamedResults(bool cancelled)
        {
            Clear(Screen.Install, "import");
            var box = OuUi.Div("mxa-panel mxa-install mxa-library", _layer);
            box.name = "ou-untamed-results";
            OuUi.Text("UNTAMED IMPORT", "mxa-screen-title", box);
            var results = _untamedResult != null ? _untamedResult.tracks : new List<UntamedTrackResult>();
            int ok = results.Count(r => r.Ok);
            string folder = _untamedResult?.tracksFolder ?? TrackMods.Folder;
            string into = folder != null ? Path.Combine(folder, UntamedImporter.TracksSubfolder) : "Mods/Tracks/" + UntamedImporter.TracksSubfolder;
            string listed = "They are listed in FREE RIDE and in RACE under MX vs ATV Untamed.";
            string head = cancelled ? "Import cancelled. The track in progress was removed" + (ok > 0 ? $"; the {ok} finished before {(ok == 1 ? "stays" : "stay")} installed. " + listed : ".")
                        : _untamedError != null ? "Import stopped: " + _untamedError.Message
                        : ok == results.Count ? $"{ok} {(ok == 1 ? "track" : "tracks")} installed. " + listed
                        : $"{ok} of {results.Count} tracks installed." + (ok > 0 ? " " + listed : "");
            OuUi.Text(head, cancelled || _untamedError != null || ok < results.Count ? "mxa-error" : "mxa-install-text", box).style.whiteSpace = WhiteSpace.Normal;
            if (ok > 0) OuUi.Text("Installed in " + into + ".", "mxa-note", box).style.whiteSpace = WhiteSpace.Normal;
            var older = results.Where(r => r.Ok).SelectMany(r => r.olderCopies).ToList();
            if (older.Count > 0)
                OuUi.Text((older.Count == 1 ? "An older copy installed by hand is still there and was not changed: " : "Older copies installed by hand are still there and were not changed: ") +
                          string.Join(", ", older.Select(o => "Mods/Tracks/" + o)) + ". It is listed under CUSTOM TRACKS; you can delete the older " + (older.Count == 1 ? "folder" : "folders") + " yourself.",
                          "mxa-note", box).style.whiteSpace = WhiteSpace.Normal;
            var scroll = new ScrollView(ScrollViewMode.Vertical); scroll.AddToClassList("mxa-library-list"); box.Add(scroll);
            foreach (var r in results)
            {
                var row = OuUi.Div("ou-cache-row", scroll.contentContainer);
                row.name = "ou-untamed-result-" + r.shortName;
                var c = OuUi.Div("ou-cache-copy", row);
                OuUi.Text((r.Ok || r.state == UntamedImportState.AlreadyImported ? "✓  " : "✕  ") + r.displayName.ToUpperInvariant(), "mxa-track-name ou-cache-name", c);
                string detail;
                switch (r.state)
                {
                    case UntamedImportState.Imported:
                        detail = $"{UntamedKindLabel(r.known)}  ·  {OpenWorldCache.SizeText(r.bytes)}  ·  {r.seconds:0} S  ·  Mods/Tracks/{r.folder}"
                                 + (r.replaced ? "  ·  REPLACED THE TRACK IMPORTED BEFORE" : "")
                                 + (r.olderCopies.Count > 0 ? "  ·  THE COPY INSTALLED BY HAND IS STILL THERE (" + string.Join(", ", r.olderCopies) + ")" : "")
                                 + (r.testedBuild ? "" : "  ·  ITS FILES DIFFER FROM THE TESTED BUILD");
                        break;
                    case UntamedImportState.AlreadyImported: detail = "ALREADY IMPORTED  ·  Mods/Tracks/" + r.folder; break;
                    case UntamedImportState.Cancelled: detail = "CANCELLED  ·  NOT INSTALLED"; break;
                    default: detail = string.IsNullOrEmpty(r.error) ? "Not installed" : r.error; break;
                }
                OuUi.Text(detail, r.state == UntamedImportState.Failed ? "mxa-error" : "ou-card-sub ou-cache-sub", c).style.whiteSpace = WhiteSpace.Normal;
            }
            var buttons = OuUi.Div("mxa-install-buttons", box);
            var done = OuUi.Button("DONE", "mxa-start", LeaveUntamed, buttons);
            done.name = "ou-untamed-done";
            OuUi.Button("MORE TRACKS", "mxa-secondary", () => { if (_untamedFound != null) ShowUntamedTracks(); else ShowUntamedSources(); }, buttons).name = "ou-untamed-more";
            if (ok > 0) OuUi.Button("OPEN TRACKS FOLDER", "mxa-secondary", TrackMods.OpenFolder, buttons).name = "ou-untamed-open";
            _nav.OnCancel = () => { LeaveUntamed(); return true; };
            _nav.DefaultItem = () => done;
            Hints((OuUi.KeySubmit, "SELECT"), (OuUi.KeyCancel, "BACK"));
            _nav.Focus(done);
        }
    }
}

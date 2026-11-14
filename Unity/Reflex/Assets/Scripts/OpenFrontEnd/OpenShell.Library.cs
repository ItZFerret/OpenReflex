// GAME LIBRARY and CONNECT FOLDER, after Open Unleashed's MxaMainMenu.Library.cs / ShowInstall:
//   library   the MX vs ATV REFLEX card (CONNECTED / NOT FOUND, the folder, CHANGE FOLDER), then the world cache: every world made from the
//             install (ReflexWorldFiles: <persistentDataPath>/ReflexCache/worlds/<version>), its event and setup, size and last ride,
//             REBUILD (made again now from the install, with the build's progress; a world with a Vehicle Select choice is removed and
//             made on its next ride) and REMOVE. Reached from the hub (GAME LIBRARY) and from options (GAME FOLDER).
//   install   CONNECT FOLDER: first run with no install found, or CHANGE FOLDER. A folder browser that works with a gamepad (drives
//             and places, then the folder's subfolders; a found install is marked), a path field (a pasted path, a file in the
//             install, its Database folder: the install above it), the native folder dialog in the Editor; USE THIS FOLDER checks
//             ReflexInstall's rule (MXReflex.exe and Database/) and saves it through ReflexInstall.Set.
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using Reflex.Port.Content;
using UnityEngine;
using UnityEngine.UIElements;

namespace Reflex.Port.OpenFrontEnd
{
    /// The world cache as the library shows it.
    public static class OpenWorldCache
    {
        public sealed class Entry
        {
            public string name, eventKey, label, detail;
            public long bytes; public DateTime used;
            public int opponents, aiSkill = ReflexWorldFiles.AiSkillNormal, laps;
            public bool garage, career;
            /// REBUILD can make it again now (an event of the tables without a Vehicle Select choice).
            public bool Rebuildable => eventKey != null && !garage && !career;
        }

        public static string Dir => Path.Combine(ReflexInstall.CacheDir, "worlds", ReflexWorldFiles.Version);
        static readonly string[] Suffixes = { "-world.bin", "-deform.bin", "-deform-model.bin", "-boot.log", "-build.log" };

        public static long SizeOf(ReflexWorldFiles w) => SizeOf(Path.GetDirectoryName(w.World), w.name);
        static long SizeOf(string dir, string name)
        {
            long n = 0;
            foreach (var s in Suffixes) { var f = new FileInfo(Path.Combine(dir, name + s)); if (f.Exists) n += f.Length; }
            return n;
        }
        public static string SizeText(long bytes) => bytes >= 1L << 30 ? (bytes / (double)(1L << 30)).ToString("0.0") + " GB" : Math.Max(1, (bytes + (1 << 19)) >> 20) + " MB";

        /// The built worlds, most recently used first.
        public static List<Entry> List()
        {
            var o = new List<Entry>();
            if (ReflexInstall.Dir == null || !Directory.Exists(Dir)) return o;
            var keys = OpenContent.Events.Select(e => (e, prefix: e.key.Replace('/', '_'))).OrderByDescending(x => x.prefix.Length).ToList();
            foreach (var stamp in new DirectoryInfo(Dir).GetFiles("*.ready").OrderByDescending(f => f.LastWriteTimeUtc))
            {
                string name = Path.GetFileNameWithoutExtension(stamp.Name);
                var en = new Entry { name = name, used = stamp.LastWriteTime, bytes = SizeOf(Dir, name) };
                if (name.StartsWith("Career_", StringComparison.Ordinal)) { en.career = true; en.label = "MOTOCAREER"; en.detail = "SERIES " + name.Substring(7).Split('_')[0].TrimStart('s'); o.Add(en); continue; }
                var match = keys.FirstOrDefault(k => name == k.prefix || name.StartsWith(k.prefix + "_", StringComparison.Ordinal));
                if (match.e == null) { en.label = name.ToUpperInvariant(); en.detail = "EVENT OF THE GAME"; o.Add(en); continue; }
                en.eventKey = match.e.key; en.label = match.e.locationName + "  ·  " + match.e.typeName + (match.e.ev.spec.trackId > 0 ? "  ·  TRACK " + (match.e.ev.spec.trackId + 1) : "");
                var parts = new List<string>();
                foreach (var tok in name.Substring(match.prefix.Length).Split(new[] { '_' }, StringSplitOptions.RemoveEmptyEntries))
                {
                    if (tok.StartsWith("ai") && int.TryParse(tok.Substring(2), out int ai)) en.opponents = ai;
                    else if (tok.StartsWith("s") && int.TryParse(tok.Substring(1), out int sk)) en.aiSkill = sk;
                    else if (tok.StartsWith("l") && int.TryParse(tok.Substring(1), out int l)) en.laps = l;
                    else if (tok.StartsWith("vs") || tok.StartsWith("c")) en.garage = true;
                }
                if (en.opponents > 0)
                {
                    int i = Array.FindIndex(ReflexWorldFiles.AiSkills, a => a.skill == en.aiSkill);
                    parts.Add("AI " + (i >= 0 ? OpenContent.AiName(i + 1) : en.aiSkill.ToString()));
                }
                else if (!match.e.freeRide) parts.Add("NO AI");
                if (en.laps > 0) parts.Add(en.laps + " LAPS");
                if (en.garage) parts.Add("VEHICLE SELECT");
                en.detail = parts.Count > 0 ? string.Join("  ·  ", parts) : "DEFAULT SETUP";
                o.Add(en);
            }
            return o;
        }

        /// Removes a world (the stamp first: a half-deleted world is never taken as ready).
        public static void Remove(string name)
        {
            try
            {
                string stamp = Path.Combine(Dir, name + ".ready"); if (File.Exists(stamp)) File.Delete(stamp);
                foreach (var s in Suffixes) { string f = Path.Combine(Dir, name + s); if (File.Exists(f)) File.Delete(f); }
            }
            catch (Exception e) { Debug.LogWarning("[OpenReflex] world " + name + " not removed: " + e.Message); }
        }

        /// The world of an entry, to build again (null: not rebuildable).
        public static ReflexWorldFiles WorldOf(Entry en) => en.Rebuildable ? ReflexWorldFiles.For(en.eventKey, en.opponents, en.aiSkill, en.laps) : null;
    }

    public sealed partial class OpenShell
    {
        bool _libraryFromMain;
        VisualElement _cacheList; Label _cacheTotal;
        // a REBUILD in progress (the shell's own; the library shows its progress)
        ReflexWorldFiles _building; Task _buildTask; string _buildName; float _buildProgress; string _buildStatus = "";
        public bool BuildRunning => _buildTask != null && !_buildTask.IsCompleted;

        public void ShowLibrary(bool fromMain)
        {
            _libraryFromMain = fromMain;
            Clear(Screen.Library, "import");
            _layer.AddToClassList("mxa-dim");
            var box = OuUi.Div("mxa-panel mxa-install mxa-library", _layer);
            box.name = "ou-game-library";
            OuUi.Text("GAME LIBRARY", "mxa-screen-title", box);
            bool core = Reflex.Port.RxCore.Bridge.OpenReflexSelector.UseOpenReflex;
            OuUi.Text(core ? "Extract your own Reflex assets into the client’s game files folder. Browse the organized index or open the folder at any time." : $"{OpenReflex.Name} reads everything it shows from your own MX vs ATV Reflex install, and makes each event's world from it once.", "mxa-install-text", box);
            var scroll = new ScrollView(ScrollViewMode.Vertical); scroll.AddToClassList("mxa-library-list"); box.Add(scroll);
            var list = scroll.contentContainer;
            string dir = ReflexInstall.Dir;
            bool ready = dir != null;
            var card = OuUi.Div("mxa-library-card", list);
            var copy = OuUi.Div("mxa-library-copy", card);
            card.style.flexWrap = Wrap.Wrap;
            copy.style.flexGrow = 1; copy.style.flexShrink = 1; copy.style.minWidth = 240;
            OuUi.Text("REQUIRED  ·  PC GAME FOLDER", "mxa-library-platform", copy);
            OuUi.Text("MX vs ATV REFLEX", "mxa-track-title", copy);
            OuUi.Text(ready ? dir : "The folder that holds the game’s Database folder.", "mxa-library-description", copy);
            OuUi.Text(ready ? $"CONNECTED  ·  {OpenContent.Events.Count} EVENTS{(OpenContent.Events.Any(e => e.dlc) ? "  ·  DLC FOUND" : "")}" : "GAME FOLDER NEEDED", "mxa-library-state", copy);
            if (core && ready && File.Exists(Path.Combine(dir, ReflexAssetImport.ExtractedMarker))) PackStatus(dir, copy);   // PACKED / LOOSE FILES and the folder's size (OpenShell.Pack.cs)
            string reportPath = ready ? Path.Combine(dir, "import-report.txt") : null;
            bool hasReport = core && reportPath != null && File.Exists(reportPath);
            if (hasReport)
            {
                string[] lines = File.ReadAllLines(reportPath);
                long count = 0;
                if (lines.Length > 0 && lines[0].StartsWith("Extracted assets: ", StringComparison.Ordinal)) long.TryParse(lines[0].Substring("Extracted assets: ".Length), out count);
                var warnings = lines.Skip(2).Where(x => !string.IsNullOrWhiteSpace(x) && x != "No source data warnings.").ToList();
                string summary = count.ToString("N0", System.Globalization.CultureInfo.InvariantCulture) + " assets imported.";
                if (warnings.Count == 1 && warnings[0].StartsWith("SX_FresnoB was excluded", StringComparison.Ordinal)) summary += " One unused level could not be imported; see import-report.txt for details.";
                else if (warnings.Count > 0) summary += " " + warnings.Count + " import " + (warnings.Count == 1 ? "warning" : "warnings") + "; see import-report.txt for details.";
                // (w-gearmods) an optional DLC file the import left out says which and why here, not only in the report
                foreach (var w in warnings.Where(x => x.StartsWith("Skipped ", StringComparison.Ordinal)).Take(3)) summary += "\n" + (w.Length > 260 ? w.Substring(0, 260) + "…" : w);
                var note = OuUi.Text(summary, "mxa-note", copy); note.style.whiteSpace = WhiteSpace.Normal;
            }
            var actions = OuUi.Div("ou-library-actions", card);
            actions.style.flexDirection = FlexDirection.Column; actions.style.alignItems = Align.Stretch;
            actions.style.width = 300; actions.style.flexShrink = 0; actions.style.marginTop = 4; actions.style.marginBottom = 4;
            VisualElement LibraryAction(string text, Action action, string name)
            {
                var button = OuUi.Button(text, "mxa-secondary mxa-library-action", action, actions); button.name = name;
                button.style.width = 300; button.style.minWidth = 300; button.style.height = 54; button.style.flexShrink = 0;
                button.style.marginTop = 6; button.style.marginLeft = 0; button.style.marginRight = 0;
                var label = button.Q<Label>(); if (label != null) { label.style.fontSize = 26; label.style.letterSpacing = 2; }
                return button;
            }
            if (core)
            {   // (w-importflow) the game files on another drive, or missing
                string moved = ReflexAssetImport.MovedDestination;
                if (moved != null && Directory.Exists(moved) && ready && ReflexPaths.SamePath(moved, dir))
                    OuUi.Text("ON " + ReflexAssetImport.PlaceName(moved).ToUpperInvariant() + "  ·  " + OpenReflex.Name + " LOOKS HERE FIRST (" + ReflexAssetImport.PointerName + ")", "mxa-library-state", copy).style.whiteSpace = WhiteSpace.Normal;
                else if (moved != null && !Directory.Exists(moved))
                    OuUi.Text("Your imported game files were on " + moved + ", which isn't there now. Connect that drive and restart, or import again.", "mxa-error", copy).style.whiteSpace = WhiteSpace.Normal;
            }
            var change = LibraryAction(ready ? "CHANGE FOLDER" : core ? "IMPORT YOUR GAME" : "CONNECT FOLDER", () => { if (core && !ready) ShowImportWelcome(true); else ShowInstall(true); }, "ou-library-folder");
            if (core)
            {
                string libraryDir = ready && File.Exists(Path.Combine(dir, ReflexAssetImport.ExtractedMarker)) ? dir : ReflexAssetImport.Destination;
                bool imported = Directory.Exists(libraryDir);
                LibraryAction("OPEN GAME FILES", () => Application.OpenURL(new Uri(libraryDir + Path.DirectorySeparatorChar).AbsoluteUri), "ou-library-open-files").SetEnabled(imported);
                LibraryAction("VIEW FILE INDEX", () => Application.OpenURL(new Uri(Path.Combine(libraryDir, "asset-index.html")).AbsoluteUri), "ou-library-file-index").SetEnabled(File.Exists(Path.Combine(libraryDir, "asset-index.html")));
                if (hasReport) LibraryAction("VIEW IMPORT REPORT", () => Application.OpenURL(new Uri(reportPath).AbsoluteUri), "ou-library-import-report");
                if (ready && !File.Exists(Path.Combine(dir, ReflexAssetImport.ExtractedMarker)))
                    LibraryAction("IMPORT THIS GAME", () => { _imp = null; ShowImportCheck(dir); if (_imp != null) _imp.fromLibrary = true; }, "ou-library-import-assets");
                if (ImportRunning) LibraryAction("IMPORT IN PROGRESS", ShowImportProgress, "ou-library-import-progress");
                else if (ready) PackAction(dir, LibraryAction);   // (experimental) PACK GAME FILES: OpenShell.Pack.cs
            }

            if (ready) AliveLibraryCard(list);   // MX vs ATV ALIVE (OpenShell.AliveImport.cs)
            if (ready) UntamedLibraryCard(list);   // MX vs ATV UNTAMED (OpenShell.UntamedImport.cs)
            if (ready && !core)
            {
                var head = OuUi.Div("ou-cache-head", list);
                OuTheme.Section(head, "WORLD CACHE");
                _cacheTotal = OuUi.Text("", "mxa-note ou-cache-total", list);
                _cacheList = OuUi.Div("ou-cache-list", list);
                FillCache();
            }
            var buttons = OuUi.Div("mxa-install-buttons", box);
            VisualElement done = null;
            if (ready) done = OuUi.Button("DONE", "mxa-start", () => { if (_libraryFromMain) ShowMain(); else { ShowMain(); ShowOptions(Options.ReflexOptions.System); } }, buttons);
            done?.SetEnabled(true);
            if (done != null) done.name = "ou-library-done";
            OuUi.Text(core ? "Individual assets are decompressed and grouped by type, database and package. Original executables, DLLs, installers and package payloads are excluded." : "Each world is about 40 to 60 MB; the cache keeps the " + ReflexWorldFiles.CacheKeep + " used last and makes any other again when it is ridden.", "mxa-note", box);
            _nav.DefaultItem = () => change;
            _nav.OnCancel = () => { if (!ready) return false; if (_libraryFromMain) ShowMain(); else { ShowMain(); ShowOptions(Options.ReflexOptions.System); } return true; };
            Hints(ready, null);
            _nav.Focus(done ?? change);
        }

        void FillCache()
        {
            if (_cacheList == null) return;
            string focused = _nav.Focused != null && _cacheList.Contains(_nav.Focused) ? _nav.Focused.name : null;
            _cacheList.Clear();
            var entries = OpenWorldCache.List();
            if (BuildRunning && !entries.Any(x => x.name == _buildName))
                entries.Insert(0, new OpenWorldCache.Entry { name = _buildName, label = _building.ev != null && OpenContent.Find(_building.ev.key) is OpenEvent be ? be.locationName + "  \u00B7  " + be.typeName : _buildName, detail = "BUILDING" });
            _cacheTotal.text = entries.Count == 0 ? "No world built yet: an event's first ride makes its world from your install (about 15 to 40 s)."
                                                  : $"{entries.Count} {(entries.Count == 1 ? "WORLD" : "WORLDS")}  ·  {OpenWorldCache.SizeText(entries.Sum(x => x.bytes))}  ·  {OpenWorldCache.Dir}";
            foreach (var en in entries)
            {
                var row = OuUi.Div("ou-cache-row", _cacheList);
                var c = OuUi.Div("ou-cache-copy", row);
                OuUi.Text(en.label, "mxa-track-name ou-cache-name", c);
                bool building = BuildRunning && en.name == _buildName;
                string when = en.used == default ? "" : "  ·  " + (DateTime.Now - en.used < TimeSpan.FromDays(1) ? "TODAY " + en.used.ToString("HH:mm") : en.used.ToString("yyyy-MM-dd"));
                OuUi.Text(building ? $"BUILDING  {_buildProgress * 100:0}%  ·  {_buildStatus}" : en.detail + "  ·  " + OpenWorldCache.SizeText(en.bytes) + when, "ou-card-sub ou-cache-sub", c);
                if (building)
                {
                    var bar = OuUi.Div("ou-progress", c);
                    var fill = OuUi.Div("ou-progress-fill", bar); OuTheme.Backdrop(fill);
                    fill.style.width = Length.Percent(Mathf.Clamp01(_buildProgress) * 100f);
                    continue;
                }
                var name = en.name;
                var rb = OuUi.Button("REBUILD", "ou-chip", () => Rebuild(en), row);
                rb.name = "ou-rebuild-" + name;
                rb.SetEnabled(!BuildRunning);
                var rm = OuUi.Button("REMOVE", "ou-chip", () => { OpenWorldCache.Remove(name); FillCache(); }, row);
                rm.name = "ou-remove-" + name;
                rm.SetEnabled(!BuildRunning);
            }
            if (focused != null) { var again = _cacheList.Q<VisualElement>(focused); if (again != null && again.enabledInHierarchy) _nav.Focus(again); }
        }

        /// REBUILD: the world removed and made again now (a Vehicle Select world: removed, made on its next ride).
        public void Rebuild(OpenWorldCache.Entry en)
        {
            if (BuildRunning) return;
            var w = OpenWorldCache.WorldOf(en);
            OpenWorldCache.Remove(en.name);
            if (w == null) { FillCache(); return; }
            StartBuild(w);
        }

        /// Builds a world in the background (the library shows its progress).
        public void StartBuild(ReflexWorldFiles w)
        {
            if (BuildRunning || w == null) return;
            _building = w; _buildName = w.name; _buildProgress = 0; _buildStatus = "starting";
            try { _buildTask = w.Build(); }
            catch (Exception e) { Debug.LogWarning("[OpenReflex] world build not started: " + e.Message); _buildTask = null; }
            Debug.Log("[OpenReflex] rebuilding world " + w.name);
            FillCache();
        }

        float _buildPollAt;
        void UpdateBuild()
        {
            UpdateAssetImport();
            UpdateAliveImport();
            UpdateUntamedImport();
            if (_buildTask == null) return;
            if (!_buildTask.IsCompleted)
            {
                if (Time.unscaledTime < _buildPollAt) return;
                _buildPollAt = Time.unscaledTime + 0.25f;
                try { _buildProgress = ReflexWorldFiles.Progress(out _buildStatus); } catch (Exception) { }
                if (Current == Screen.Library) FillCache();
                return;
            }
            var t = _buildTask; _buildTask = null;
            Debug.Log("[OpenReflex] world " + _buildName + (t.IsFaulted ? " not built: " + t.Exception?.GetBaseException().Message : " built"));
            if (Current == Screen.Library) FillCache();
        }

        float _libraryAt;
        void UpdateLibrary()
        {
            if (Time.unscaledTime < _libraryAt || BuildRunning) return;
            _libraryAt = Time.unscaledTime + 2f;   // worlds made elsewhere (a race in progress) show up
        }

        // --------------------------------------------------------------------------------------------- install

        string _installDir;
        bool _installFromLibrary;

        /// CONNECT FOLDER (first run: fromLibrary false) / CHANGE FOLDER.
        public void ShowInstall(bool fromLibrary)
        {
            _installFromLibrary = fromLibrary;
            Clear(Screen.Install);
            SetBackground(null);
            DriveScanner.Default.BeginRefresh(); ReflexInstall.Detect();
            string cur = ReflexInstall.Dir ?? CleanPath(ReflexInstall.Saved), savedMissing = null;
            bool curThere = !string.IsNullOrEmpty(cur) && ReflexFs.Within(2500, () => Directory.Exists(cur), out _);
            if (!curThere && !string.IsNullOrEmpty(cur) && ReflexInstall.Dir == null) savedMissing = "The saved game folder is not there now. " + ReflexPaths.NotFound(cur, PeekRoots());
            _installDir = curThere ? cur : Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);
            var box = OuUi.Div("mxa-panel mxa-install", _layer);
            box.name = "ou-connect-folder";
            bool core = CoreImport;
            OuUi.Text(core ? "FIND YOUR GAME FOLDER" : fromLibrary ? "GAME FOLDER" : "CONNECT FOLDER", "mxa-screen-title", box);
            if (core)
            {
                var t = OuUi.Text("Pick your MX vs ATV Reflex folder: the one with a folder named Database inside (it also has MXReflex.exe). " +
                                  "Steam: right-click MX vs ATV Reflex > Manage > Browse local files shows it (usually ...\\steamapps\\common\\MX vs ATV Reflex); paste its address below. " +
                                  "A folder " + OpenReflex.Name + " imported before (it has reflex-extracted-v1.txt) can be picked too. Your original install is only read.", "mxa-install-text", box);
                t.style.whiteSpace = WhiteSpace.Normal;
            }
            else
            {
                OuUi.Text($"Point {OpenReflex.Name} at your MX vs ATV Reflex install: the folder that holds the game’s Database folder. Everything the game shows is read from there at run time; nothing of the game is copied.", "mxa-install-text", box);
                if (!fromLibrary) OuUi.Text("No install was found (the saved folder, REFLEX_GAME_DIR, reflex-game-dir.txt beside the game, \"game files/Archive\" above it).", "mxa-error", box);
            }
            var field = new TextField { value = _installDir };
            field.name = "ou-install-path";
            field.AddToClassList("mxa-path");
            box.Add(field);
            var status = OuUi.Text("", "mxa-install-status", box);
            status.name = "ou-install-status"; status.style.whiteSpace = WhiteSpace.Normal;
            var sv = new ScrollView(ScrollViewMode.Vertical); sv.AddToClassList("mxa-dir-list"); box.Add(sv);
            var buttons = OuUi.Div("mxa-install-buttons", box);
            VisualElement use = null;

            void Refresh(string dir)
            {
                _installDir = dir;
                field.SetValueWithoutNotify(dir);
                bool ok = ReflexInstall.IsInstall(dir);
                status.text = ok ? (core && File.Exists(Path.Combine(dir, ReflexAssetImport.ExtractedMarker)) ? "✓  Imported game files: USE THEM" : "✓  MX vs ATV Reflex found here") : core ? ImportWhy(dir) : ReflexInstall.Why(dir);
                status.EnableInClassList("mxa-ok", ok);
                sv.contentContainer.Clear();
                OuUi.Text("DRIVES & PLACES", "mxa-dir-header", sv.contentContainer);
                foreach (var (path, label) in InstallPlaces())
                {
                    var pp = path;
                    var it = OuUi.Item(OuUi.Text(label + (ReflexInstall.IsInstall(pp) ? "   ✓" : ""), "mxa-dir", sv.contentContainer), () => Refresh(pp));
                    it.EnableInClassList("mxa-dir-current", ReflexPaths.SamePath(pp, dir));
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
                        var dd = d; bool inst = ReflexInstall.IsInstall(d);
                        var it = OuUi.Item(OuUi.Text(name + (inst ? "   ✓" : ""), "mxa-dir", sv.contentContainer), () => Refresh(dd));
                        it.EnableInClassList("mxa-ok", inst);
                    }
                }
                catch (Exception e) { OuUi.Text("This folder can't be read: " + e.Message, "mxa-error", sv.contentContainer); }
            }

            field.isDelayed = true;
            field.RegisterValueChangedCallback(e =>
            {
                string target = ResolveTypedFolder(e.newValue, out string why);
                if (target != null) Refresh(target);
                else { status.text = why; status.RemoveFromClassList("mxa-ok"); field.SetValueWithoutNotify(_installDir); }
            });
            use = OuUi.Button(core ? "USE THIS FOLDER" : "USE THIS FOLDER", "mxa-start", () => TryUse(_installDir, status), buttons);
            use.name = "ou-install-use";
#if UNITY_EDITOR
            OuUi.Button("BROWSE…", "mxa-secondary", () =>
            {
                string p = UnityEditor.EditorUtility.OpenFolderPanel("MX vs ATV Reflex game data (the folder holding Database/)", _installDir, "");
                if (!string.IsNullOrEmpty(p)) Refresh(p);
            }, buttons);
#endif
            if (core) OuUi.Button("BACK", "mxa-secondary", () => { if (_imp != null) ShowImportFind(); else if (fromLibrary) ShowLibrary(_libraryFromMain); else ShowImportWelcome(false); }, buttons);
            else if (fromLibrary) OuUi.Button("BACK", "mxa-secondary", () => ShowLibrary(_libraryFromMain), buttons);
            else OuUi.Button("QUIT", "mxa-secondary", Quit, buttons);
            Refresh(_installDir);
            if (savedMissing != null) status.text = savedMissing;
            DrivesChanged();
            ScreenUpdate = () => { if (DrivesChanged()) { Refresh(_installDir); if (savedMissing != null) status.text = savedMissing; } };
            _nav.OnCancel = () =>
            {
                if (core) { if (_imp != null) ShowImportFind(); else if (_installFromLibrary) ShowLibrary(_libraryFromMain); else ShowImportWelcome(false); return true; }
                if (_installFromLibrary) { ShowLibrary(_libraryFromMain); return true; } return false;
            };
            _nav.DefaultItem = () => use;
            Hints((OuUi.KeySubmit, "OPEN / SELECT"), (OuUi.KeyCancel, "BACK"));
            _nav.Focus(use);
        }

        /// The path field's text as typed (tests).
        public void SetInstallPath(string text) { var f = _layer.Q<TextField>("ou-install-path"); if (f != null) f.value = text; }

        void TryUse(string dir, Label status)
        {
            if (!ReflexInstall.IsInstall(dir)) { status.text = CoreImport ? ImportWhy(dir) : ReflexInstall.Why(dir); status.RemoveFromClassList("mxa-ok"); OuSound.Play("Locked"); return; }
            if (Reflex.Port.RxCore.Bridge.OpenReflexSelector.UseOpenReflex && !File.Exists(Path.Combine(dir, ReflexAssetImport.ExtractedMarker)))
            {
                ShowImportCheck(dir); return;   // the pre-flight (OpenShell.ImportFlow.cs): space, mods, where the files go
            }
            bool had = OpenContent.Events.Count > 0 && ReflexInstall.Dir != null;
            string before = ReflexInstall.Dir;
            try { ReflexInstall.Set(dir); }
            catch (Exception e) { status.text = e.Message; status.RemoveFromClassList("mxa-ok"); return; }
            bool changed = !string.Equals(before, ReflexInstall.Dir, StringComparison.Ordinal);
            if (changed) ReflexInstall.ResetReaders();
            OpenContent.Reset();
            Connected();
            if (had && changed) Debug.Log("[OpenReflex] game folder changed: some content read before keeps until the next start");
            ShowLibrary(_installFromLibrary ? _libraryFromMain : true);
        }

        /// Cleans a typed or pasted path (ReflexPaths.Clean): surrounding quotes, file:// URLs, \\?\ prefixes, %VARIABLES%, ~, a bare drive
        /// letter, forward slashes, doubled and trailing separators. Any drive letter, a network path (\\server\share) too.
        static string CleanPath(string value) => ReflexInstall.Clean(value);

        /// The folder a typed path points at, null with an error sentence: a folder, the folder of a pasted file, or the install above a
        /// pasted Database folder. The existence checks wait a few seconds at most (a network location that is offline).
        static string ResolveTypedFolder(string text, out string error)
        {
            string s = CleanPath(text);
            error = ReflexPaths.Validate(s, ReflexInstall.WindowsHost);
            if (error != null) return null;
            try
            {
                var look = ReflexFs.Within(3000, () => File.Exists(s) ? 2 : Directory.Exists(s) ? 1 : 0, out bool timedOut);
                if (timedOut) { error = "That location did not answer (a network drive that is offline?): " + s; return null; }
                if (look == 2) s = Path.GetDirectoryName(s);
                else if (look == 0) { error = ReflexPaths.NotFound(s, PeekRoots()); return null; }
                s = Path.GetFullPath(s);
                string parent = ReflexPaths.Parent(s);
                if (!ReflexInstall.IsInstall(s) && parent != null && string.Equals(ReflexPaths.LabelOf(s), "Database", StringComparison.OrdinalIgnoreCase) && ReflexInstall.IsInstall(parent)) s = parent;   // pasted ...\Database
                return s;
            }
            catch (Exception e) when (e is IOException || e is ArgumentException || e is NotSupportedException || e is UnauthorizedAccessException || e is System.Security.SecurityException)
            { error = "That folder can't be opened: " + e.Message; return null; }
        }

        /// The roots of the drives the last scan found ready (none before it finished, or off Windows).
        static IEnumerable<string> PeekRoots() => ReflexInstall.WindowsHost ? DriveScanner.Default.Peek()?.Select(d => d.Root) : null;

        /// Every ready drive (Windows: any letter, USB / removable, mounted ISOs and optical drives, network drives; ones that do not answer
        /// are left out without waiting) or the startup disk and /Volumes (macOS), the installs found on them, plus home folders. Drives and
        /// installs come in from background scans: a screen that shows them polls DrivesChanged.
        static List<(string path, string label)> InstallPlaces(bool installs = true)
        {
            var places = new List<(string, string)>();
            try
            {
                if (ReflexInstall.WindowsHost)
                {
                    DriveScanner.Default.BeginRefresh();
                    foreach (var d in DriveScanner.Default.Peek() ?? new List<DriveEntry>())
                        places.Add((d.Root, d.Kind + "  " + d.Root.TrimEnd('\\') + (d.Label.Length > 0 ? "  " + d.Label : "")));
                }
                else
                {
                    places.Add(("/", "DRIVE  Startup disk"));
                    if (Directory.Exists("/Volumes")) foreach (var v in Directory.GetDirectories("/Volumes").OrderBy(x => x)) if (!MacSystemVolumes.Contains(Path.GetFileName(v))) places.Add((v, "DRIVE  " + Path.GetFileName(v)));
                }
            }
            catch { }
            string home = Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);
            foreach (var (path, label) in new[] { (home, "Home"), (Environment.GetFolderPath(Environment.SpecialFolder.Desktop), "Desktop"), (Path.Combine(home, "Downloads"), "Downloads") })
                if (!string.IsNullOrEmpty(path) && ReflexFs.Within(1500, () => Directory.Exists(path), out _)) places.Add((path, label));
            if (!installs) return places;
            var seen = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            foreach (var c in ReflexInstall.Candidates(false).Take(8))
                try { if (ReflexInstall.IsInstall(c)) { string full = Path.GetFullPath(c); if (seen.Add(full)) places.Add((full, "FOUND  " + ReflexPaths.LabelOf(full))); } } catch { }
            ReflexInstall.Detect();
            foreach (var f in ReflexInstall.Detected)
                if (seen.Add(f.Path)) places.Add((f.Path, "FOUND  " + f.Source + "  " + f.Path));
            return places;
        }

        /// A background drive or install search finished with something new since this was last asked (the drive list should be shown again).
        int _placesSeen = -1;
        bool DrivesChanged()
        {
            int now = DriveScanner.Default.Version * 1009 + ReflexInstall.DetectedVersion;
            if (now == _placesSeen) return false;
            _placesSeen = now; return true;
        }
    }
}

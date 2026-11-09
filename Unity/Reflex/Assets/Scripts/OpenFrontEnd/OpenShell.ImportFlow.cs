// IMPORT YOUR GAME (w-importflow): the first-run import of the player's own MX vs ATV Reflex, step by step, in the GAME LIBRARY's look
// (pad, keys and mouse as every shell page). Support's most common failures had their own pages: a full C: drive (the import stopped with
// "Win32 IO returned 112") and community mods left in the game's Database folder ("While reading MXTables_DLC008.dx9.database: Specified
// cast is not valid").
//   1 ABOUT     what the import does and why: the game reads your own copy; nothing is downloaded; the original is not changed
//   2 FIND      the installs found (Steam libraries of every drive and libraryfolders.vdf, retail folders, SteamOS / Proton's Linux Steam
//               and microSD card, macOS bottles: ReflexInstall.Detect) or BROWSE (CONNECT FOLDER's browser, with what to pick)
//   3 CHECK     pre-flight on a worker: the mods in Database (ImportModScan: unreadable ones are left out), the exact space the import
//               needs (ReflexAssetImport.Measure) against the destination drive's free space (+ margin), write access, Windows' path
//               limit, OneDrive / network folders, a game files folder already there, unfinished imports left behind (RESUME / CLEAN UP),
//               the time it takes; START IMPORT only when nothing blocks it
//   SPACE       NOT ENOUGH SPACE: "Needs 14.8 GB, C: has 3.1 GB free": the drives (and the microSD card / volumes) with their free space,
//               BROWSE for a folder; the choice is remembered at the client root (ReflexAssetImport.RememberDestination) for every later
//               start and update. Also where a full disk during the import lands (nothing left behind), with COPY ERROR DETAILS.
//   MODS        MODS IN YOUR GAME FOLDER: each pack, its files and folder, what happens to it; MOVE TO MODS (asks first; gear / kit packs
//               to Mods/Gear, beta-slot tracks to Mods/Tracks), LEAVE THEM OUT, OPEN FOLDER; how to delete them (Steam never does).
//               Also where an import that stopped on one of the install's files lands.
//   4 IMPORT    progress: the step, the bytes of the measured total, the time left, the file; CANCEL (asks; removes what was written)
//   5 DONE      YOUR GAME IS READY: what was imported, where, what was left out; PLAY / OPEN GAME FILES
//   FAILED      any other failure in plain words, TRY AGAIN, COPY ERROR DETAILS (the raw error, version and system; nothing sent)
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
    public sealed partial class OpenShell
    {
        /// The import being prepared or run.
        sealed class ImportState
        {
            public string source, found;                  // the install (the folder holding Database); how it was found ("STEAM")
            public string destination;                    // where the game files go
            public bool fromLibrary;
            public readonly HashSet<string> skip = new HashSet<string>(StringComparer.OrdinalIgnoreCase);   // Database files left out
            public bool userSkip;                         // the player chose LEAVE THEM OUT (else only unreadable mods are left out)
            public string resume;                         // a leftover staging folder to go on with
            // the check's results
            public Task check; public CancellationTokenSource checkCancel; public volatile float checkProgress; public volatile string checkStep = "";
            public ImportPlan plan; public string planKey;
            public List<DbModGroup> mods; public List<ImportLeftover> leftovers;
            public long? free; public int cluster = 4096; public string writeProblem; public bool destExists, destImported;
            public Exception checkError;
            public string note;                           // why the page was opened (a full disk, a file that stopped the import)
            public Exception error;                       // the failure the note is about (COPY ERROR DETAILS)
            public string badFile;                        // the install's file an import stopped at
            public long Need => plan == null ? 0 : Math.Max(0, plan.Required(cluster) - (resume != null ? leftovers?.FirstOrDefault(l => l.path == resume)?.bytes ?? 0 : 0));
            public bool Moved => destination != null && !ReflexPaths.SamePath(destination, ReflexAssetImport.DefaultDestination);
        }
        ImportState _imp;

        static bool CoreImport => Reflex.Port.RxCore.Bridge.OpenReflexSelector.UseOpenReflex;

        // ------------------------------------------------------------------------------------------------ page frame

        static readonly string[] ImportSteps = { "ABOUT", "YOUR GAME", "CHECK", "IMPORT", "PLAY" };
        static readonly Color ImpOrange = new Color(204 / 255f, 95 / 255f, 41 / 255f), ImpGold = new Color(1f, 0.78f, 0.31f), ImpDim = new Color(0.62f, 0.62f, 0.6f),
                              ImpGreen = new Color(0.45f, 0.82f, 0.45f), ImpRed = new Color(0.94f, 0.47f, 0.38f), ImpPaper = new Color(0.93f, 0.92f, 0.89f);

        /// An import page: the panel, its title bar, the steps line, a scrolling body and the button row.
        VisualElement ImportPage(string name, string title, int step, out VisualElement body, out VisualElement buttons)
        {
            Clear(Screen.Install, "import");
            SetBackground(null);
            _layer.AddToClassList("mxa-dim");
            var box = OuUi.Div("mxa-panel mxa-install mxa-library ou-importflow", _layer);
            box.name = name;
            OuUi.Text(title, "mxa-screen-title", box);
            if (step > 0)
            {
                var steps = OuUi.Div("ou-import-steps", box);
                steps.style.flexDirection = FlexDirection.Row; steps.style.flexWrap = Wrap.Wrap; steps.style.marginBottom = 14; steps.style.flexShrink = 0;
                for (int i = 0; i < ImportSteps.Length; i++)
                {
                    var l = OuUi.Text((i + 1) + "  " + ImportSteps[i], "mxa-library-state", steps);
                    l.style.marginRight = 34; l.style.fontSize = 22;
                    l.style.color = i + 1 < step ? ImpGreen : i + 1 == step ? ImpGold : ImpDim;
                    if (i + 1 == step) { l.style.borderBottomWidth = 3; l.style.borderBottomColor = ImpOrange; l.style.paddingBottom = 2; }
                    if (i + 1 < step) l.text = "✓  " + ImportSteps[i];
                }
            }
            var scroll = new ScrollView(ScrollViewMode.Vertical); scroll.AddToClassList("mxa-library-list"); box.Add(scroll);
            _nav.TextScroll = scroll;   // (the right stick scrolls the page's text; the cards hold few items to focus)
            body = scroll.contentContainer;
            buttons = OuUi.Div("mxa-install-buttons", box);
            buttons.style.flexWrap = Wrap.Wrap;
            return box;
        }

        static Label Para(string text, VisualElement parent, string cls = "mxa-install-text")
        {
            var l = OuUi.Text(text, cls, parent); l.style.whiteSpace = WhiteSpace.Normal; l.enableRichText = false; l.style.marginBottom = 10;
            return l;
        }

        /// A line of the check / a card of a list: a mark (✓ ! × …), a title, a detail and buttons on the right.
        VisualElement ImportRow(VisualElement parent, string mark, string title, string detail, params (string label, string name, Action action)[] actions)
        {
            var card = OuUi.Div("mxa-library-card ou-import-row", parent);
            card.style.flexDirection = FlexDirection.Row; card.style.alignItems = Align.Center; card.style.flexWrap = Wrap.Wrap;
            card.style.paddingTop = 12; card.style.paddingBottom = 12; card.style.marginBottom = 10;
            var m = OuUi.Text(mark, "mxa-track-title", card);
            m.style.width = 56; m.style.fontSize = 38; m.style.unityTextAlign = TextAnchor.MiddleCenter; m.style.flexShrink = 0;
            m.style.color = mark == "✓" ? ImpGreen : mark == "×" ? ImpRed : mark == "!" ? ImpGold : ImpDim;
            var copy = OuUi.Div("mxa-library-copy", card);
            copy.style.flexGrow = 1; copy.style.flexShrink = 1; copy.style.minWidth = 300; copy.style.marginLeft = 8;
            var t = OuUi.Text(title, "mxa-library-description", copy); t.style.whiteSpace = WhiteSpace.Normal; t.style.fontSize = 28; t.enableRichText = false;
            if (!string.IsNullOrEmpty(detail)) { var d = OuUi.Text(detail, "mxa-note", copy); d.style.whiteSpace = WhiteSpace.Normal; d.enableRichText = false; d.style.marginTop = 2; }
            if (actions.Length > 0)
            {
                var row = OuUi.Div("ou-import-actions", card);
                row.style.flexDirection = FlexDirection.Row; row.style.flexShrink = 0; row.style.marginLeft = 12;
                foreach (var (label, name, action) in actions)
                {
                    var b = OuUi.Button(label, "ou-chip", action, row); b.name = name; b.style.marginLeft = 8;
                }
            }
            return card;
        }

        /// A card the player can pick (pad, keys, mouse): the gold edge and tint while focused.
        static VisualElement Selectable(VisualElement row, Action submit)
        {
            OuUi.Item(row, submit);
            row.RegisterCallback<FocusInEvent>(_ => { row.style.borderLeftColor = ImpGold; row.style.borderLeftWidth = 10; row.style.backgroundColor = new Color(1f, 0.78f, 0.31f, 0.16f); });
            row.RegisterCallback<FocusOutEvent>(_ => { row.style.borderLeftWidth = StyleKeyword.Null; row.style.borderLeftColor = StyleKeyword.Null; row.style.backgroundColor = StyleKeyword.Null; });
            return row;
        }

        VisualElement ImportButton(VisualElement buttons, string label, string name, Action action, bool primary = false)
        {
            var b = OuUi.Button(label, primary ? "mxa-start" : "mxa-secondary", action, buttons); b.name = name;
            return b;
        }

        void ImportHints() => Hints((OuUi.KeySubmit, "SELECT"), (OuUi.KeyCancel, "BACK"));
        /// "your startup disk has" at the start of a sentence: "Your startup disk has".
        static string Cap(string s) => string.IsNullOrEmpty(s) ? s : char.ToUpperInvariant(s[0]) + s.Substring(1);

        /// Where BACK from the first pages goes: the library it came from, or (the first run) nowhere but QUIT.
        void ImportLeave()
        {
            _imp?.checkCancel?.Cancel();
            if (_imp != null && _imp.fromLibrary && ReflexInstall.Dir != null) ShowLibrary(_libraryFromMain);
            else ShowImportWelcome(_imp?.fromLibrary ?? false);
        }

        // ------------------------------------------------------------------------------------------------ 1 ABOUT

        /// The first page of the import (the first run, or the library's IMPORT): what it does and why.
        public void ShowImportWelcome(bool fromLibrary)
        {
            if (!CoreImport) { ShowInstall(fromLibrary); return; }
            if (_assetImport != null) { ShowImportProgress(); return; }
            _imp = _imp ?? new ImportState(); _imp.fromLibrary = fromLibrary;
            DriveScanner.Default.BeginRefresh(); ReflexInstall.Detect();
            ReflexAssetImport.EnsureHelp();
            ImportPage("ou-import-welcome", "IMPORT YOUR GAME", 1, out var body, out var buttons);
            OuTheme.Section(body, "HOW " + OpenReflex.Name + " WORKS");
            Para(OpenReflex.Name + " plays with your own copy of MX vs ATV Reflex: the tracks, bikes, riders, sounds and videos all come from the game you own. " +
                 "Before the first ride it copies them out of your game's install into a folder it can read quickly. This happens once.", body);
            ImportRow(body, "✓", "Nothing is downloaded", "Everything comes from the game already on your computer. No game files come with " + OpenReflex.Name + ".");
            ImportRow(body, "✓", "Your game isn't changed", "The original install is only read. You can keep playing it as before.");
            ImportRow(body, "…", "It needs some room", "About 15 GB of free space while importing (the next pages check this and offer another drive if needed); packed at the end, the game files take about 6 GB. Usually 3 to 15 minutes.");
            string moved = ReflexAssetImport.MovedDestination;
            if (moved != null && !Directory.Exists(moved))
                ImportRow(body, "!", "Your imported game files are missing", "They were imported to " + moved + ", which isn't there now. If that drive is unplugged, connect it and restart " + OpenReflex.Name + ". Otherwise import again.");
            var next = ImportButton(buttons, "GET STARTED", "ou-import-start", ShowImportFind, true);
            if (fromLibrary) ImportButton(buttons, "BACK", "ou-import-back", () => ShowLibrary(_libraryFromMain));
            else
            {
                ImportButton(buttons, "I ALREADY IMPORTED", "ou-import-have", () => ShowInstall(false));
                ImportButton(buttons, "QUIT", "ou-import-quit", Quit);
            }
            _nav.DefaultItem = () => next;
            _nav.OnCancel = () => { if (fromLibrary) { ShowLibrary(_libraryFromMain); return true; } return false; };
            if (fromLibrary) ImportHints(); else Hints((OuUi.KeySubmit, "SELECT"));
            _nav.Focus(next);
        }

        // ------------------------------------------------------------------------------------------------ 2 YOUR GAME

        /// The installs found on this machine; BROWSE when it is not among them.
        public void ShowImportFind()
        {
            _imp = _imp ?? new ImportState();
            ImportPage("ou-import-find", "FIND YOUR GAME", 2, out var body, out var buttons);
            Para("Choose your MX vs ATV Reflex install. " + OpenReflex.Name + " looks in your Steam libraries on every drive (and on SteamOS, the microSD card) and in the usual install folders.", body);
            var list = OuUi.Div("ou-import-found", body);
            var status = Para("", body, "mxa-note");
            var browse = ImportButton(buttons, "BROWSE FOR THE FOLDER", "ou-import-browse", () => ShowInstall(_imp.fromLibrary), true);
            ImportButton(buttons, "BACK", "ou-import-back", () => ShowImportWelcome(_imp.fromLibrary));
            float since = Time.unscaledTime; string shownSig = null;
            VisualElement first = null;
            void Fill()
            {
                var rows = ImportCandidates();
                string sig = string.Join("|", rows.Select(r => r.path));
                if (sig == shownSig && rows.Count > 0) return;
                shownSig = sig;
                bool hadFocus = first != null && _nav.Focused != null && list.Contains(_nav.Focused);
                list.Clear(); first = null;
                foreach (var (path, source, imported) in rows)
                {
                    string p = path;
                    var row = ImportRow(list, imported ? "✓" : "›", (imported ? "ALREADY IMPORTED  ·  " : source + "  ·  ") + p,
                        imported ? "Game files " + OpenReflex.Name + " imported before. Choose it to use them." : "MX vs ATV Reflex is installed here.");
                    Selectable(row, () => { if (imported) TryUse(p, status); else ShowImportCheck(p, source); });
                    row.name = "ou-import-found-" + list.childCount;
                    first = first ?? row;
                }
                bool searching = Time.unscaledTime - since < 6f && ReflexInstall.WindowsHost;
                status.text = rows.Count > 0 ? (rows.Count == 1 ? "Found 1 install." : "Found " + rows.Count + " installs.") + " Pick one, or BROWSE if yours isn't listed."
                    : searching ? "Looking on your drives and Steam libraries…"
                    : "No install found automatically. Choose BROWSE FOR THE FOLDER and pick the game's folder: the one with the Database folder inside. " +
                      "In Steam: right-click MX vs ATV Reflex > Manage > Browse local files, and copy the address of that window. From a disc install it is often C:\\Program Files (x86)\\THQ\\MX vs ATV Reflex.";
                if (first != null && (hadFocus || _nav.Focused == null || _nav.Focused == browse || _nav.Focused.panel == null)) _nav.Focus(first);
            }
            Fill();
            float next = 0;
            ScreenUpdate = () => { if (Time.unscaledTime >= next) { next = Time.unscaledTime + 0.5f; if (Time.unscaledTime - since < 8f || ReflexInstall.DetectedVersion > 0) Fill(); } };
            _nav.DefaultItem = () => first ?? browse;
            _nav.OnCancel = () => { ShowImportWelcome(_imp.fromLibrary); return true; };
            ImportHints();
            _nav.Focus(first ?? browse);
        }

        /// The installs to offer: found by the search, the configured candidates, the saved folder; extracted game files folders as such.
        static List<(string path, string source, bool imported)> ImportCandidates()
        {
            var o = new List<(string, string, bool)>(); var seen = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            void Add(string p, string source)
            {
                try
                {
                    if (string.IsNullOrEmpty(p) || !ReflexInstall.IsInstall(p)) return;
                    string full = Path.GetFullPath(p); if (!seen.Add(full.TrimEnd('/', '\\'))) return;
                    o.Add((full, source, File.Exists(Path.Combine(full, ReflexAssetImport.ExtractedMarker))));
                }
                catch (Exception) { }
            }
            foreach (var f in ReflexInstall.Detected) Add(f.Path, f.Source);
            foreach (var c in ReflexInstall.Candidates(false).Take(8)) Add(c, "FOUND");
            return o.OrderBy(x => x.Item3 ? 1 : 0).ToList();
        }

        /// What is wrong with a folder chosen as the game's install, in the player's words ("" when it is one).
        internal static string ImportWhy(string dir)
        {
            if (string.IsNullOrEmpty(dir)) return "No folder chosen.";
            string why = ReflexInstall.Why(dir);
            if (why.Length == 0) return "";
            if (!Directory.Exists(dir)) return why;
            try
            {
                if (string.Equals(ReflexPaths.LabelOf(dir), "Database", StringComparison.OrdinalIgnoreCase)) return "This is the Database folder itself. Go up one folder and pick the folder that contains it.";
                var inside = Directory.GetDirectories(dir).FirstOrDefault(ReflexInstall.IsInstall);
                if (inside != null) return "Almost: the game is one folder down, in \"" + Path.GetFileName(inside) + "\". Open it.";
                if (Directory.GetFiles(dir, "*.dx9.database").Length > 0) return "This folder holds game databases, but the game's folder is the one above it (with Database inside).";
                if (File.Exists(Path.Combine(dir, "MXReflex.exe"))) return "MXReflex.exe is here, but the Database folder is missing. The install may be incomplete: in Steam, use Verify integrity of game files.";
            }
            catch (Exception) { }
            return "No Database folder here. Pick the game's folder: the one with a folder named Database inside (usually ...\\steamapps\\common\\MX vs ATV Reflex).";
        }

        // ------------------------------------------------------------------------------------------------ 3 CHECK

        /// The pre-flight of an import of source (the install): runs the checks, then shows what they found.
        public void ShowImportCheck(string source, string found = null)
        {
            _imp = _imp ?? new ImportState();
            if (_imp.source == null || !ReflexPaths.SamePath(_imp.source, source))
            {   // another install: the choices made for the last one go (where the game files go stays)
                bool from = _imp.fromLibrary; string dest = _imp.destination; _imp = new ImportState { fromLibrary = from, destination = dest };
            }
            _imp.source = Path.GetFullPath(source); _imp.found = found ?? _imp.found;
            _imp.destination = _imp.destination ?? ReflexAssetImport.Destination;
            StartImportCheck();
            ShowImportCheckPage();
        }

        /// The checks, on a worker (the paths Unity owns are read here, on the main thread).
        void StartImportCheck()
        {
            var st = _imp;
            st.checkCancel?.Cancel();
            var cancel = st.checkCancel = new CancellationTokenSource();
            st.checkError = null; st.checkProgress = 0; st.checkStep = "Looking at your game's Database folder…";
            string source = st.source, dest = st.destination, parent = Path.GetDirectoryName(dest);
            st.check = Task.Run(() =>
            {
                // mods first: what cannot be read is left out, and the space is measured without it
                st.mods = ImportModScan.Scan(Path.Combine(source, "Database"), cancel.Token);
                foreach (var f in ImportModScan.FilesOf(st.mods.Where(g => g.unreadable != null))) st.skip.Add(f);
                st.checkStep = "Measuring what the import needs…";
                string key = source + "|" + string.Join(",", st.skip.OrderBy(x => x, StringComparer.OrdinalIgnoreCase));
                if (st.plan == null || st.planKey != key) { st.plan = ReflexAssetImport.Measure(source, st.skip, cancel.Token, p => st.checkProgress = p); st.planKey = key; }
                // a required database that cannot be read, which is a mod: left out too (a community track installed the original way)
                if (st.plan.fatalFile != null && st.mods.Any(g => g.files.Contains(st.plan.fatalFile, StringComparer.OrdinalIgnoreCase)))
                {
                    var g = st.mods.First(x => x.files.Contains(st.plan.fatalFile, StringComparer.OrdinalIgnoreCase));
                    if (g.unreadable == null) g.unreadable = st.plan.fatalFile + ": " + st.plan.fatalWhy;
                    foreach (var f in ImportModScan.FilesOf(new[] { g })) st.skip.Add(f);
                    st.plan = ReflexAssetImport.Measure(source, st.skip, cancel.Token, p => st.checkProgress = p); st.planKey = source + "|" + string.Join(",", st.skip.OrderBy(x => x, StringComparer.OrdinalIgnoreCase));
                }
                st.checkStep = "Checking the free space…";
                st.destExists = Directory.Exists(dest); st.destImported = st.destExists && File.Exists(Path.Combine(dest, ReflexAssetImport.ExtractedMarker));
                st.writeProblem = ReflexAssetImport.DestinationProblem(parent);
                st.cluster = ReflexAssetImport.ClusterSize(parent);
                st.free = ReflexFs.Within(5000, () => ReflexAssetImport.FreeBytes(parent), out bool slow);
                if (slow) st.free = null;
                st.leftovers = ReflexAssetImport.Leftovers(parent);
                if (st.resume != null && !st.leftovers.Any(l => l.path == st.resume)) st.resume = null;
                st.checkProgress = 1;
            }, cancel.Token);
        }

        void ShowImportCheckPage()
        {
            var st = _imp;
            ImportPage("ou-import-check", "CHECK BEFORE IMPORTING", 3, out var body, out var buttons);
            var rows = OuUi.Div("ou-import-checks", body);
            VisualElement start = ImportButton(buttons, "START IMPORT", "ou-import-go", () => BeginImport(), true);
            ImportButton(buttons, "BACK", "ou-import-back", () => { st.checkCancel?.Cancel(); ShowImportFind(); });
            start.SetEnabled(false);
            bool shown = false; VisualElement focus = null;
            void Render()
            {
                rows.Clear(); focus = null;
                if (!string.IsNullOrEmpty(st.note)) ImportRow(rows, "!", st.note, null);
                ImportRow(rows, "✓", "Your game: " + st.source, FoundText(st.found),
                    ("CHANGE", "ou-import-change-source", ShowImportFind));
                if (!st.check.IsCompleted)
                {
                    var busy = ImportRow(rows, "…", st.checkStep, st.plan == null && st.checkProgress > 0 ? Mathf.RoundToInt(st.checkProgress * 100) + " %" : "This takes a few seconds.");
                    busy.name = "ou-import-checking";
                    start.SetEnabled(false);
                    return;
                }
                if (st.check.IsFaulted || st.check.IsCanceled)
                {
                    var e = st.check.Exception?.GetBaseException();
                    ImportRow(rows, "×", "The check could not finish", e != null ? e.Message : "It was stopped.", ("TRY AGAIN", "ou-import-recheck", () => { StartImportCheck(); ShowImportCheckPage(); }));
                    return;
                }
                bool blocked = false;
                var plan = st.plan; long need = st.Need;
                string place = ReflexAssetImport.PlaceName(st.destination);
                // the space
                bool enough = ReflexAssetImport.Enough(st.free, need);
                string needText = "Needs " + ReflexAssetImport.GB(need) + (st.free != null ? ", " + place + " has " + ReflexAssetImport.GB(st.free.Value) + " free." : ". The free space of " + place + " could not be read.");
                if (enough)
                    ImportRow(rows, "✓", needText, "The game files take " + ReflexAssetImport.GB(plan.Bytes) + " (" + plan.assetFiles.ToString("N0") + " files) and " + ReflexAssetImport.GB(ImportPlan.Margin(need)) + " is kept spare.",
                        ("ANOTHER DRIVE", "ou-import-space", () => ShowImportSpace(null)));
                else
                {
                    blocked = true;
                    long freeable = st.leftovers?.Where(l => l.path != st.resume).Sum(l => l.bytes) ?? 0;
                    var r = ImportRow(rows, "×", "Not enough space. " + needText,
                        "Put the game files on another drive instead: " + OpenReflex.Name + " remembers where they are." + (freeable > 0 ? " Cleaning up the unfinished import below frees " + ReflexAssetImport.GB(freeable) + "." : ""),
                        ("CHOOSE ANOTHER DRIVE", "ou-import-space", () => ShowImportSpace(null)));
                    focus = r.Q<VisualElement>("ou-import-space");
                }
                // where they go
                ImportRow(rows, st.Moved ? "!" : "✓", "Game files go to: " + st.destination,
                    st.Moved ? "Another drive than " + OpenReflex.Name + "'s own folder. It will look for them there from now on. Your mods and settings stay where they are." : "Next to " + OpenReflex.Name + " (its client folder), where every update keeps them.",
                    st.Moved ? new[] { ("USE THE DEFAULT", "ou-import-default", (Action)(() => { st.destination = ReflexAssetImport.DefaultDestination; StartImportCheck(); ShowImportCheckPage(); })) } : new (string, string, Action)[0]);
                if (st.writeProblem != null)
                {
                    blocked = true;
                    var r = ImportRow(rows, "×", "Can't write there", st.writeProblem + " Pick another place.", ("CHOOSE ANOTHER PLACE", "ou-import-space-write", () => ShowImportSpace(null)));
                    focus = focus ?? r.Q<VisualElement>("ou-import-space-write");
                }
                if (ReflexInstall.WindowsHost && ReflexAssetImport.DeepestPath(st.destination, plan) > ReflexAssetImport.WindowsMaxPath)
                {
                    blocked = true;
                    var r = ImportRow(rows, "×", "That folder's path is too long for Windows", "Some game files would end up deeper than Windows allows (260 characters). Pick a short folder, such as " + ReflexPaths.ShortFolderExample(st.destination) + ".",
                        ("CHOOSE ANOTHER PLACE", "ou-import-space-path", () => ShowImportSpace(null)));
                    focus = focus ?? r.Q<VisualElement>("ou-import-space-path");
                }
                if (ReflexAssetImport.InOneDrive(st.destination, Environment.GetEnvironmentVariable))
                    ImportRow(rows, "!", "That folder is in OneDrive", "OneDrive would upload " + ReflexAssetImport.GB(plan.Bytes) + " of game files and may move them off this PC. A folder outside OneDrive is better.", ("CHOOSE ANOTHER PLACE", "ou-import-space-onedrive", () => ShowImportSpace(null)));
                if (ReflexAssetImport.InOneDrive(st.source, Environment.GetEnvironmentVariable))
                    ImportRow(rows, "!", "Your game is in OneDrive", "Files that are online-only are downloaded while importing, which is slow. Right-click the game's folder > Always keep on this device first.");
                // the game files folder already there
                if (st.destExists)
                {
                    if (st.destImported)
                    {
                        blocked = true;
                        var r = ImportRow(rows, "✓", "Game files are already imported there", "Use them as they are, or put them aside and import again.",
                            ("USE THEM", "ou-import-use-existing", () => TryUse(st.destination, Para("", rows, "mxa-error"))), ("IMPORT AGAIN", "ou-import-aside", () => ConfirmAside(st)));
                        focus = focus ?? r.Q<VisualElement>("ou-import-use-existing");
                    }
                    else
                    {
                        blocked = true;
                        var r = ImportRow(rows, "×", "A \"game files\" folder is in the way", st.destination + " is there but isn't a finished import. Nothing is deleted: it can be renamed out of the way.",
                            ("RENAME IT", "ou-import-aside", () => ConfirmAside(st)));
                        focus = focus ?? r.Q<VisualElement>("ou-import-aside");
                    }
                }
                // mods
                var mods = st.mods ?? new List<DbModGroup>();
                int bad = mods.Count(g => g.unreadable != null), movable = mods.Count(g => g.moveTo != null && g.unreadable == null);
                if (mods.Count == 0) ImportRow(rows, "✓", "No mods in the game folder", null);
                else
                {
                    string head = mods.Count == 1 ? "1 mod in your game's Database folder" : mods.Count + " mods in your game's Database folder";
                    string sub = (bad > 0 ? bad + (bad == 1 ? " can't be read and will be left out. " : " can't be read and will be left out. ") : "") +
                                 (movable > 0 ? movable + (movable == 1 ? " pack works better" : " packs work better") + " from " + OpenReflex.Name + "'s Mods folder. " : "") +
                                 (st.userSkip ? "You chose to leave the mods out." : "");
                    var r = ImportRow(rows, bad > 0 || movable > 0 ? "!" : "✓", head, sub.Trim().Length > 0 ? sub.Trim() : "They will be imported as they are.", ("REVIEW MODS", "ou-import-mods", () => ShowImportMods()));
                }
                // a required file that cannot be read (and is no mod): the install is damaged
                if (plan.fatalFile != null && !st.skip.Contains(plan.fatalFile))
                {
                    blocked = true;
                    ImportRow(rows, "×", "A game file can't be read: " + plan.fatalFile, "Your install may be damaged (" + plan.fatalWhy + "). In Steam: right-click MX vs ATV Reflex > Properties > Installed Files > Verify integrity of game files, then check again.",
                        ("CHECK AGAIN", "ou-import-recheck", () => { st.plan = null; StartImportCheck(); ShowImportCheckPage(); }));
                }
                // unfinished imports left behind
                if (st.leftovers != null && st.leftovers.Count > 0)
                {
                    long bytes = st.leftovers.Sum(l => l.bytes);
                    var newest = st.leftovers[0];
                    if (st.resume != null)
                        ImportRow(rows, "✓", "Going on with the unfinished import (" + ReflexAssetImport.GB(newest.bytes) + " already there)", "Its files are checked against your game; anything damaged is copied again.",
                            ("START OVER", "ou-import-noresume", () => { st.resume = null; ShowImportCheckPage(); }));
                    else
                        ImportRow(rows, "!", (st.leftovers.Count == 1 ? "An unfinished import is still here" : st.leftovers.Count + " unfinished imports are still here") + " (" + ReflexAssetImport.GB(bytes) + ")",
                            "From an import that didn't finish (" + OpenReflex.Name + " was closed or crashed). Go on with it, or remove it to free the space.",
                            ("RESUME", "ou-import-resume", () => { st.resume = newest.path; ShowImportCheckPage(); }),
                            ("CLEAN UP", "ou-import-cleanup", () => CleanUpLeftovers(st)));
                }
                if (ReflexAssetImport.InProgramFiles(st.source, Environment.GetEnvironmentVariable) && movable > 0)
                    ImportRow(rows, "…", "Your game is in Program Files", "Moving mods out of it may need Windows' permission. You can always leave them in and they'll be imported as they are.");
                var (lo, hi) = ReflexAssetImport.Minutes(plan);
                ImportRow(rows, "…", "Takes about " + lo + " to " + hi + " minutes", "Depends on your drive. You can cancel at any time; nothing is left half-done.");
                start.SetEnabled(!blocked);
                if (!shown || _nav.Focused == null || _nav.Focused.panel == null) { shown = true; _nav.Focus(focus ?? (blocked ? (VisualElement)rows.Q<VisualElement>(className: "ou-chip") ?? start : start)); }
            }
            Render();
            bool done = st.check.IsCompleted;
            float last = 0;
            ScreenUpdate = () =>
            {
                if (st.check == null) return;
                if (!done && st.check.IsCompleted) { done = true; Render(); return; }
                if (!done && Time.unscaledTime - last > 0.25f) { last = Time.unscaledTime; var b = rows.Q<VisualElement>("ou-import-checking"); var labels = b?.Query<Label>().ToList(); if (labels != null && labels.Count >= 3) { labels[1].text = st.checkStep; labels[2].text = st.checkProgress > 0 && st.checkProgress < 1 ? Mathf.RoundToInt(st.checkProgress * 100) + " %" : "This takes a few seconds."; } }
            };
            _nav.DefaultItem = () => start.enabledSelf ? start : (VisualElement)rows.Q<VisualElement>(className: "ou-chip") ?? start;
            _nav.OnCancel = () => { st.checkCancel?.Cancel(); ShowImportFind(); return true; };
            ImportHints();
            if (!done) _nav.Focus(buttons.Q<VisualElement>("ou-import-back"));
        }

        static string FoundText(string found)
        {
            switch (found)
            {
                case null: return "The folder you picked.";
                case "STEAM": return "Found in your Steam library.";
                case "GAMES FOLDER": return "Found in a games folder.";
                case "DRIVE ROOT": return "Found at the top of a drive (a disc or a mounted image).";
                case "BOTTLE": return "Found in a Windows bottle (CrossOver / Whisky).";
                default: return "Found where " + OpenReflex.Name + " looks for it.";
            }
        }

        /// Renames a game files folder in the way to "game files (old <date>)" (never deletes), asking first.
        void ConfirmAside(ImportState st)
        {
            string to = st.destination + " (old " + DateTime.Now.ToString("yyyy-MM-dd HHmm") + ")";
            OpenPopup("popup-import-aside", "RENAME THE OLD FOLDER?", null, "\"" + Path.GetFileName(st.destination) + "\" will be renamed to \"" + Path.GetFileName(to) + "\" in the same place, and a new import goes in. Nothing is deleted.",
                () => { }, 1, false,
                ("RENAME", "popup-aside-yes", () =>
                {
                    try { ReflexAssetImport.Retry(() => Directory.Move(st.destination, to), CancellationToken.None, 4); st.note = "The old folder is now " + to + "."; }
                    catch (Exception e) { st.note = "The folder could not be renamed: " + e.Message; }
                    StartImportCheck(); ShowImportCheckPage();
                }), ("CANCEL", "popup-aside-no", null));
        }

        /// Removes the unfinished imports' staging folders (the import's own: no confirmation needed beyond the button).
        void CleanUpLeftovers(ImportState st)
        {
            long freed = 0; int failed = 0;
            foreach (var l in st.leftovers.ToList()) { if (ReflexAssetImport.DeleteTree(l.path)) freed += l.bytes; else failed++; }
            st.resume = null;
            st.note = failed == 0 ? "Cleaned up: " + ReflexAssetImport.GB(freed) + " freed." : "Freed " + ReflexAssetImport.GB(freed) + "; " + failed + " folder(s) could not be removed (in use?).";
            StartImportCheck(); ShowImportCheckPage();
        }

        // ------------------------------------------------------------------------------------------------ SPACE

        Task<List<ImportLocation>> _locations;

        /// NOT ENOUGH SPACE / CHOOSE WHERE: the drives with their free space; picking one moves where the game files go (for good).
        public void ShowImportSpace(string note)
        {
            var st = _imp; if (st == null || st.plan == null) { ShowImportWelcome(false); return; }
            if (note != null) st.note = note;
            long need = st.Need;
            bool short_ = !ReflexAssetImport.Enough(st.free, need);
            ImportPage("ou-import-space-page", short_ ? "NOT ENOUGH SPACE" : "CHOOSE WHERE TO PUT THE GAME FILES", 3, out var body, out var buttons);
            if (!string.IsNullOrEmpty(st.note)) ImportRow(body, "!", st.note, null);
            string place = ReflexAssetImport.PlaceName(st.destination);
            var head = Para("The imported game files need " + ReflexAssetImport.GB(need) + (st.free != null ? ". " + Cap(place) + " has " + ReflexAssetImport.GB(st.free.Value) + " free" : "") + ". " +
                            "Pick a drive with enough room: " + OpenReflex.Name + " puts them in an OpenReflex folder there and always looks for them there, after updates too. " +
                            "Your mods and settings stay where they are.", body);
            head.style.fontSize = 28;
            var list = OuUi.Div("ou-import-drives", body);
            var status = Para("Reading your drives…", body, "mxa-note");
            var browse = ImportButton(buttons, "BROWSE FOR A FOLDER", "ou-import-dest-browse", () => ShowImportBrowseDest(), false);
            if (st.error != null) { AddCopyDetails(buttons, st.error); AddHelp(buttons); }
            ImportButton(buttons, "BACK", "ou-import-back", () => { st.note = null; st.error = null; ShowImportCheckPage(); });
            string current = st.destination ?? ReflexAssetImport.DefaultDestination;
            if (_locations == null || _locations.IsCompleted) _locations = Task.Run(() => ImportLocations(current));
            var task = _locations;
            VisualElement first = null; bool filled = false;
            void Fill()
            {
                filled = true; list.Clear();
                var locs = task.IsFaulted ? new List<ImportLocation>() : task.Result;
                foreach (var l in locs.OrderByDescending(x => ReflexAssetImport.Enough(x.free, need)).ThenByDescending(x => x.free ?? 0))
                {
                    bool ok = ReflexAssetImport.Enough(l.free, need) && !(l.current && st.writeProblem != null);
                    string freeText = l.free == null ? "free space unknown" : ReflexAssetImport.GB(l.free.Value) + " free";
                    var row = ImportRow(list, ok ? "›" : "×", l.label.ToUpperInvariant() + "  ·  " + freeText + (l.current ? "  ·  WHERE THEY GO NOW" : ""),
                        ok ? (l.current ? "Keep them in " + l.folder + "." : "Puts them in " + Path.Combine(l.folder, "game files") + ".") + (l.network ? " A network drive: slower to import and to load." : "")
                           : "Not enough room (" + ReflexAssetImport.GB(need + ImportPlan.Margin(need)) + " needed with room to spare).");
                    row.name = "ou-import-drive-" + list.childCount;
                    if (ok)
                    {
                        var loc = l;
                        Selectable(row, () => ChooseDestination(loc.current ? current : loc.Destination));
                        first = first ?? row;
                    }
                    else row.style.opacity = 0.55f;
                }
                status.text = locs.Count == 0 ? "No other drive was found. Use BROWSE FOR A FOLDER, or free up space on " + place + " (empty the Recycle Bin, uninstall games you don't play) and check again."
                    : first == null ? "No drive has enough room. Free up some space (empty the Recycle Bin, uninstall games you don't play), connect a drive, or BROWSE for a folder on a drive with room."
                    : "Choose a drive. A fast internal drive (SSD) loads tracks quickest.";
                _nav.Focus(first ?? browse);
            }
            if (task.IsCompleted) Fill();
            ScreenUpdate = () => { if (!filled && task.IsCompleted) Fill(); };
            _nav.DefaultItem = () => first ?? browse;
            _nav.OnCancel = () => { st.note = null; st.error = null; ShowImportCheckPage(); return true; };
            ImportHints();
            _nav.Focus(browse);
        }

        /// The places the game files can go, with their free space (on a worker: a drive that does not answer is left out after a few seconds).
        static List<ImportLocation> ImportLocations(string current)
        {
            List<DriveEntry> drives = null; List<string> mounts = new List<string>();
            string home = null; try { home = Environment.GetFolderPath(Environment.SpecialFolder.UserProfile); } catch (Exception) { }
            if (ReflexInstall.WindowsHost)
            {
                drives = DriveScanner.Default.Ready(2000);
                if (ReflexFs.IsWine) mounts = ReflexAssetImport.ProtonMounts(new SystemFinderFs(), Environment.GetEnvironmentVariable("HOME"));
            }
            else
            {
                mounts.Add("/");
                try { if (Directory.Exists("/Volumes")) foreach (var v in Directory.GetDirectories("/Volumes").OrderBy(x => x)) if (!MacSystemVolumes.Contains(Path.GetFileName(v)) && !ReflexFs.IsLink(v)) mounts.Add(v); } catch (Exception) { }
            }
            // TEST: REFLEX_IMPORT_TEST_MOUNTS = "<label>=<folder>;..." adds drives (the showcase has one disk)
            var labels = new Dictionary<string, string>(StringComparer.Ordinal);
            foreach (var part in (Environment.GetEnvironmentVariable("REFLEX_IMPORT_TEST_MOUNTS") ?? "").Split(';'))
            { int eq = part.IndexOf('='); if (eq > 0) { string f = part.Substring(eq + 1).Trim(); mounts.Insert(0, f); labels[f.TrimEnd('/', '\\')] = part.Substring(0, eq).Trim(); } }
            var list = ReflexAssetImport.Locations(drives, mounts, home, current, f => ReflexFs.Within(3000, () => ReflexAssetImport.FreeBytes(f), out bool slow) is long n && !slow ? n : (long?)null);
            foreach (var l in list) if (labels.TryGetValue(l.root, out var label)) l.label = label;
            return list;
        }

        static readonly HashSet<string> MacSystemVolumes = new HashSet<string>(StringComparer.OrdinalIgnoreCase) { "Macintosh HD", "Recovery", "Preboot", "VM", "Update", "xarts", "iSCPreboot", "Hardware", "com.apple.TimeMachine.localsnapshots" };

        /// Puts the game files at destination (checked again before the import starts; remembered once the import finished).
        void ChooseDestination(string destination)
        {
            var st = _imp;
            st.destination = destination; st.note = null; st.error = null; st.resume = null;
            StartImportCheck(); ShowImportCheckPage();
        }

        /// A folder browser for the destination: drives and places, the folder's subfolders, a path field; USE THIS FOLDER.
        void ShowImportBrowseDest()
        {
            var st = _imp;
            ImportPage("ou-import-dest", "PICK A FOLDER FOR THE GAME FILES", 3, out var body, out var buttons);
            Para("Pick a folder on a drive with at least " + ReflexAssetImport.GB(st.Need + ImportPlan.Margin(st.Need)) + " free. The game files go into an OpenReflex folder inside it. Avoid OneDrive and network folders.", body);
            string dir = Path.GetDirectoryName(st.destination ?? ReflexAssetImport.DefaultDestination);
            while (dir != null && !Directory.Exists(dir)) dir = ReflexPaths.Parent(dir);
            dir = dir ?? Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);
            var field = new TextField { value = dir }; field.name = "ou-import-dest-path"; field.AddToClassList("mxa-path"); body.Add(field);
            var status = Para("", body, "mxa-install-status");
            var list = OuUi.Div("ou-import-dest-list", body);
            string Target(string d) => string.Equals(ReflexPaths.LabelOf(d), "OpenReflex", StringComparison.OrdinalIgnoreCase) ? Path.Combine(d, "game files") : Path.Combine(d, "OpenReflex", "game files");
            void Refresh(string d)
            {
                dir = d; field.SetValueWithoutNotify(d); list.Clear();
                long? free = ReflexFs.Within(3000, () => ReflexAssetImport.FreeBytes(d), out bool slow);
                bool ok = !slow && ReflexAssetImport.Enough(free, st.Need);
                status.text = (free != null ? Cap(ReflexAssetImport.PlaceName(d)) + " has " + ReflexAssetImport.GB(free.Value) + " free" : "Free space unknown") + (ok ? "  ·  the game files would go to " + Target(d) : "  ·  not enough room here");
                status.EnableInClassList("mxa-ok", ok);
                OuUi.Text("DRIVES & PLACES", "mxa-dir-header", list);
                foreach (var (path, label) in InstallPlaces(false)) { var pp = path; OuUi.Item(OuUi.Text(label, "mxa-dir", list), () => Refresh(pp)); }
                OuUi.Text("IN " + ShortPath(d).ToUpperInvariant(), "mxa-dir-header", list);
                string up = ReflexPaths.Parent(d);
                if (up != null) OuUi.Item(OuUi.Text("←  ..", "mxa-dir", list), () => Refresh(up));
                try
                {
                    foreach (var sub in Directory.GetDirectories(d).OrderBy(x => x, StringComparer.OrdinalIgnoreCase))
                    {
                        string name = Path.GetFileName(sub); if (name.StartsWith(".") || ReflexMods.IsJunk(name)) continue;
                        var s = sub; OuUi.Item(OuUi.Text(name, "mxa-dir", list), () => Refresh(s));
                    }
                }
                catch (Exception e) { OuUi.Text("This folder can't be read: " + e.Message, "mxa-error", list); }
            }
            field.isDelayed = true;
            field.RegisterValueChangedCallback(e => { string t = ResolveTypedFolder(e.newValue, out string why); if (t != null) Refresh(t); else { status.text = why; status.RemoveFromClassList("mxa-ok"); } });
            var use = ImportButton(buttons, "USE THIS FOLDER", "ou-import-dest-use", () =>
            {
                string target = Target(dir);
                if (ReflexPaths.IsUnder(st.source, target) || ReflexPaths.SamePath(st.source, Path.GetDirectoryName(target))) { status.text = "That is inside your game's install. Pick a folder outside it."; status.RemoveFromClassList("mxa-ok"); OuSound.Play("Locked"); return; }
                string problem = ReflexAssetImport.DestinationProblem(Path.GetDirectoryName(target));
                if (problem != null) { status.text = problem; status.RemoveFromClassList("mxa-ok"); OuSound.Play("Locked"); return; }
                ChooseDestination(target);
            }, true);
            ImportButton(buttons, "BACK", "ou-import-back", () => ShowImportSpace(null));
            Refresh(dir);
            _nav.DefaultItem = () => use;
            _nav.OnCancel = () => { ShowImportSpace(null); return true; };
            Hints((OuUi.KeySubmit, "OPEN / SELECT"), (OuUi.KeyCancel, "BACK"));
            _nav.Focus(use);
        }

        // ------------------------------------------------------------------------------------------------ MODS

        /// MODS IN YOUR GAME FOLDER: what was found in Database, what happens to it, and what the player can do.
        public void ShowImportMods(string note = null)
        {
            var st = _imp; if (st?.mods == null) { ShowImportWelcome(false); return; }
            if (note != null) st.note = note;
            string dbDir = Path.Combine(st.source, "Database");
            ImportPage("ou-import-mods-page", "MODS IN YOUR GAME FOLDER", 3, out var body, out var buttons);
            if (!string.IsNullOrEmpty(st.note)) ImportRow(body, "!", st.note, null);
            Para("These files were added to the game's Database folder by hand: community gear, bike kits or tracks, installed the way the original game needed. " +
                 "Uninstalling the game or Steam's \"Verify integrity of game files\" does not remove added files, so they stay until someone moves or deletes them. " +
                 "They are in: " + dbDir, body);
            Para(OpenReflex.Name + " reads gear and kit packs from its own Mods/Gear folder and tracks from Mods/Tracks instead, without touching the game folder, and any number of them at once.",
                 body, "mxa-note");
            var list = OuUi.Div("ou-import-mod-list", body);
            foreach (var g in st.mods.OrderBy(x => st.badFile != null && x.files.Any(f => string.Equals(f, st.badFile, StringComparison.OrdinalIgnoreCase)) ? 0 : 1))
            {
                string files = g.files.Count <= 4 ? string.Join(", ", g.files) : string.Join(", ", g.files.Take(3)) + " and " + (g.files.Count - 3) + " more";
                bool hit = st.badFile != null && g.files.Any(f => string.Equals(f, st.badFile, StringComparison.OrdinalIgnoreCase));
                bool left = g.files.Where(f => !f.EndsWith("/")).Any(f => st.skip.Contains(f));
                string what = hit ? "The import stopped at " + st.badFile + ". " : "";
                var row = ImportRow(list, g.unreadable != null || hit ? "×" : g.moveTo != null ? "!" : "…", ImportModScan.Title(g) + (left ? "  ·  LEFT OUT" : ""),
                    what + ImportModScan.Advice(g) + "\n" + files + "  (" + ReflexAssetImport.GB(g.bytes) + ")" + (g.unreadable != null ? "\nWhy: " + g.unreadable : ""));
                row.name = "ou-import-mod-" + list.childCount;
            }
            Para("To remove them for good: OPEN FOLDER, then delete the files listed above. Moving them to " + OpenReflex.Name + "'s Mods folder keeps them working here (the original game won't see them there).", body, "mxa-note");
            var status = Para("", body, "mxa-install-status");
            var movable = st.mods.Where(g => g.moveTo != null).ToList();
            VisualElement first = null;
            if (movable.Count > 0)
                first = ImportButton(buttons, movable.Count == 1 ? "MOVE IT TO MODS" : "MOVE " + movable.Count + " TO MODS", "ou-import-mods-move", () => ConfirmMoveMods(st, movable, dbDir), true);
            var leavable = st.mods.Where(g => g.kind != DbModGroup.Kinds.Changed && !g.IsFolder && g.files.Any(f => !st.skip.Contains(f))).ToList();
            if (leavable.Count > 0)
            {
                var b = ImportButton(buttons, "LEAVE THEM OUT", "ou-import-mods-skip", () =>
                {
                    foreach (var f in ImportModScan.FilesOf(leavable)) st.skip.Add(f);
                    st.userSkip = true; st.note = null; st.badFile = null; StartImportCheck(); ShowImportCheckPage();
                });
                first = first ?? b;
            }
            ImportButton(buttons, "OPEN FOLDER", "ou-import-mods-open", () => { try { Application.OpenURL(new Uri(dbDir + Path.DirectorySeparatorChar).AbsoluteUri); } catch (Exception e) { status.text = "The folder could not be opened: " + e.Message; } });
            if (st.error != null) AddCopyDetails(buttons, st.error);
            var cont = ImportButton(buttons, "CONTINUE", "ou-import-mods-back", () => { st.note = null; st.badFile = null; st.error = null; StartImportCheck(); ShowImportCheckPage(); });
            _nav.DefaultItem = () => first ?? cont;
            _nav.OnCancel = () => { st.note = null; StartImportCheck(); ShowImportCheckPage(); return true; };
            ImportHints();
            _nav.Focus(first ?? cont);
        }

        /// Moves the movable packs to Mods/Gear / Mods/Tracks, after asking (they leave the game folder: the original game no longer sees them).
        void ConfirmMoveMods(ImportState st, List<DbModGroup> groups, string dbDir)
        {
            string mods = Path.GetDirectoryName(ReflexMods.Folder(ReflexMods.GearName));
            string list = string.Join("\n", groups.Select(g => "  " + ImportModScan.Title(g) + "  ->  Mods/" + g.moveTo));
            OpenPopup("popup-import-move", "MOVE TO " + OpenReflex.Name + "'S MODS FOLDER?", null,
                "These files will be moved out of the game's Database folder:\n" + list + "\n\n" + OpenReflex.Name + " will use them from " + mods + ". The original game won't see them any more. Nothing is deleted.",
                () => { }, 0, true,
                ("MOVE THEM", "popup-move-yes", () =>
                {
                    var done = new List<string>(); var errors = new List<string>();
                    foreach (var g in groups)
                    {
                        try { done.Add(ImportModScan.Title(g) + " -> " + ImportModScan.Move(g, dbDir, mods)); }
                        catch (Exception e) { errors.Add(ImportModScan.Title(g) + ": " + (e is UnauthorizedAccessException ? "Windows didn't allow it (the game is in a protected folder such as Program Files). Move them yourself with OPEN FOLDER, or leave them in: they will be imported as they are." : e.Message)); }
                    }
                    try { KitPacks.Refresh(); } catch (Exception) { }
                    st.note = (done.Count > 0 ? "Moved: " + string.Join("; ", done) + ". " : "") + (errors.Count > 0 ? "Not moved: " + string.Join("; ", errors) : "");
                    st.plan = null; StartImportCheck(); ShowImportCheckPage();
                }), ("CANCEL", "popup-move-no", null));
        }

        // ------------------------------------------------------------------------------------------------ 4 IMPORT

        Task<ReflexAssetImport.Result> _assetImport;
        CancellationTokenSource _assetCancel;
        ReflexAssetImport.Progress _assetProgress;
        Label _assetStatus;
        bool _assetStopping;
        string _assetDestination;

        /// The library's IMPORT CONNECTED ASSETS and CONNECT FOLDER's IMPORT GAME FILES: the check first.
        void BeginAssetImport(string source) => ShowImportCheck(source);

        /// Starts the import the check page agreed to.
        void BeginImport()
        {
            var st = _imp;
            if (_assetImport != null || st?.plan == null) return;
            _assetCancel = new CancellationTokenSource(); _assetProgress = null; _assetStopping = false; _assetDestination = st.destination;
            var options = new ReflexAssetImport.Options { plan = st.plan, skipFiles = st.skip.ToList(), resumeStage = st.resume };
            Debug.Log("[import] " + st.source + " -> " + st.destination + ": " + ReflexAssetImport.GB(st.plan.Bytes) + " measured, needs " + ReflexAssetImport.GB(st.Need) + ", free " + (st.free == null ? "?" : ReflexAssetImport.GB(st.free.Value)) +
                      (st.skip.Count > 0 ? ", leaving out " + string.Join(", ", st.skip) : "") + (st.resume != null ? ", resuming " + st.resume : ""));
            _assetImport = ReflexAssetImport.Import(st.source, st.destination, p => Interlocked.Exchange(ref _assetProgress, p), _assetCancel.Token, options);
            ShowImportProgress();
        }

        Label _impPercent, _impBytes, _impStep, _impFile; VisualElement _impFill;
        void ShowImportProgress()
        {
            ImportPage("ou-import-progress", "IMPORTING YOUR GAME", 4, out var body, out var buttons);
            _impStep = Para("Starting…", body, "mxa-library-description"); _impStep.style.fontSize = 30;
            _impPercent = OuUi.Text("0 %", "mxa-track-title", body); _impPercent.style.fontSize = 72;
            var bar = OuUi.Div("ou-progress", body); bar.style.height = 18; bar.style.marginTop = 6; bar.style.marginBottom = 10;
            _impFill = OuUi.Div("ou-progress-fill", bar); OuTheme.Backdrop(_impFill); _impFill.style.width = Length.Percent(0);
            _impBytes = Para("", body); _impBytes.style.fontSize = 28;
            _impFile = Para("", body, "mxa-note"); _impFile.style.opacity = 0.7f;
            _assetStatus = _impFile;
            Para("You can leave this running. Nothing is downloaded and your original game isn't changed. If you cancel, the files copied so far are removed.", body, "mxa-note");
            var cancel = ImportButton(buttons, "CANCEL", "ou-import-cancel", ConfirmCancelImport);
            _nav.DefaultItem = () => cancel; _nav.OnCancel = () => { ConfirmCancelImport(); return true; };
            Hints((OuUi.KeyCancel, "CANCEL"));
            _nav.Focus(cancel);
            RenderImportProgress();
        }

        void ConfirmCancelImport()
        {
            if (_assetImport == null || _assetStopping) return;
            OpenPopup("popup-import-cancel", "STOP THE IMPORT?", null, "The files copied so far will be removed. Your game isn't changed, and you can start again any time.", () => { }, 1, false,
                ("STOP", "popup-cancel-yes", () => { _assetStopping = true; _assetCancel?.Cancel(); RenderImportProgress(); }), ("KEEP GOING", "popup-cancel-no", null));
        }

        void RenderImportProgress()
        {
            if (_impPercent == null || _impPercent.panel == null) return;
            var p = Interlocked.CompareExchange(ref _assetProgress, null, null);
            if (_assetStopping) { _impStep.text = "Stopping… removing the unfinished files"; return; }
            if (p == null) return;
            double frac = p.total > 0 ? Math.Min(1, (double)p.copied / p.total) : 0;
            _impPercent.text = Math.Floor(frac * 100) + " %";
            _impFill.style.width = Length.Percent((float)(frac * 100));
            string of = ReflexAssetImport.PackOnImport(null) ? " of 3" : " of 2";
            _impStep.text = p.phase == 1 ? "Step 1" + of + ": copying videos, settings and track data" : p.phase == 2 ? "Step 2" + of + ": unpacking models, textures and sounds"
                          : p.phase == 4 ? "Step 3 of 3: packing them into compressed archives (every file is checked)" : "Finishing: writing the file index";
            string left = "";
            if (p.seconds > 8 && frac > 0.01 && frac < 1 && p.phase != 4)
            {
                double rate = p.copied / p.seconds, rest = (p.total - p.copied) / Math.Max(1, rate);
                left = rest < 60 ? "  ·  less than a minute left" : "  ·  about " + Math.Ceiling(rest / 60) + " min left";
            }
            _impBytes.text = p.phase == 4 ? ReflexAssetImport.GB(p.total / 2) + " of game files  ·  packed and checked one track at a time"   // (packing counts every byte twice: packed, then checked)
                           : ReflexAssetImport.GB(p.copied) + " of " + ReflexAssetImport.GB(p.total) + (p.count > 0 ? "  ·  " + p.files.ToString("N0") + " of " + p.count.ToString("N0") + " files" : "") + left;
            string f = p.current ?? ""; _impFile.text = f.Length > 100 ? "…" + f.Substring(f.Length - 99) : f;
        }

        float _impRenderAt;
        void UpdateAssetImport()
        {
            if (_assetImport == null) return;
            if (Time.unscaledTime >= _impRenderAt) { _impRenderAt = Time.unscaledTime + 0.1f; RenderImportProgress(); }
            if (!_assetImport.IsCompleted) return;
            var completed = _assetImport; _assetImport = null; _assetCancel.Dispose(); _assetCancel = null;
            var st = _imp ?? new ImportState();
            if (completed.IsCanceled || completed.IsFaulted)
            {
                var error = completed.Exception?.Flatten().InnerExceptions.FirstOrDefault();
                bool cancelled = completed.IsCanceled || error is OperationCanceledException;
                if (cancelled) { Debug.Log("[import] cancelled"); ShowImportCancelled(); return; }
                Debug.LogException(error);
                ImportFailed(error);
                return;
            }
            var result = completed.Result;
            try { ReflexAssetImport.RememberDestination(result.directory); }
            catch (Exception e) { Debug.LogWarning("[import] the game files location was not saved: " + e.Message); }
            st.resume = null; st.plan = null;
            string before = ReflexInstall.Dir;
            try { ReflexInstall.Set(result.directory); }
            catch (Exception e) { ImportFailed(e); return; }
            try
            {
                if (!string.Equals(before, ReflexInstall.Dir, StringComparison.Ordinal)) ReflexInstall.ResetReaders();
                OpenContent.Reset();
                Connected();
            }
            catch (Exception e) { Debug.LogWarning("[import] the imported game did not load at once (it will on the next start): " + e); }
            ShowImportDone(result);
        }

        /// A failure: a full disk -> the space page; a file of the install -> the mods page (or what to do about a damaged install); else the
        /// failure page.
        void ImportFailed(Exception error)
        {
            var st = _imp ?? (_imp = new ImportState());
            st.error = error; st.resume = null;   // (a resumed stage is removed by a failure too)
            var ie = error as ImportException;
            if (ReflexAssetImport.IsDiskFull(error))
            {
                st.free = ReflexAssetImport.FreeBytes(Path.GetDirectoryName(st.destination ?? ReflexAssetImport.Destination));
                ShowImportSpace("The drive filled up during the import, so it stopped. The unfinished files were removed: nothing was left behind. " +
                                (st.free != null ? Cap(ReflexAssetImport.PlaceName(st.destination)) + " has " + ReflexAssetImport.GB(st.free.Value) + " free now. " : "") + "Pick a drive with more room.");
                return;
            }
            if (ie != null && ie.Kind == ImportException.Failure.BadSourceFile && st.source != null)
            {
                st.badFile = ie.SourceFile;
                try { st.mods = ImportModScan.Scan(Path.Combine(st.source, "Database")); } catch (Exception) { }
                if (st.mods != null && st.mods.Any(g => g.files.Contains(ie.SourceFile, StringComparer.OrdinalIgnoreCase)))
                {
                    var g = st.mods.First(x => x.files.Contains(ie.SourceFile, StringComparer.OrdinalIgnoreCase));
                    if (g.unreadable == null) g.unreadable = ie.SourceFile + ": " + (ie.InnerException?.Message ?? ie.Message);
                    foreach (var f in ImportModScan.FilesOf(new[] { g })) st.skip.Add(f);
                    st.plan = null;
                    ShowImportMods("The import stopped at " + ie.SourceFile + ", a mod in your game folder this version can't read. It is now left out: CONTINUE to import the rest, or move or delete it first.");
                    return;
                }
            }
            ShowImportFailedPage(error);
        }

        void ShowImportFailedPage(Exception error)
        {
            var st = _imp;
            var ie = error as ImportException;
            ImportPage("ou-import-failed", "THE IMPORT STOPPED", 4, out var body, out var buttons);
            string what, todo;
            if (ie != null && ie.Kind == ImportException.Failure.BadSourceFile)
            {
                what = "A file of your game couldn't be read: " + ie.SourceFile + ".";
                todo = "Your install may be damaged. In Steam: right-click MX vs ATV Reflex > Properties > Installed Files > Verify integrity of game files, then try again. From a disc: reinstall the game.";
            }
            else if (error is UnauthorizedAccessException || error?.InnerException is UnauthorizedAccessException || (error?.Message ?? "").IndexOf("denied", StringComparison.OrdinalIgnoreCase) >= 0)
            {
                what = (ReflexInstall.WindowsHost ? "Windows" : "Your system") + " didn't let " + OpenReflex.Name + " write a file.";
                todo = "An antivirus may be blocking it, or the folder is protected. Try again; if it happens again, allow " + OpenReflex.Name + " in your antivirus, or pick another place for the game files.";
            }
            else if ((error?.Message ?? "").IndexOf("too deeply nested", StringComparison.OrdinalIgnoreCase) >= 0 || error is PathTooLongException)
            {
                what = "The folder's path is too long for Windows.";
                todo = "Pick a short folder for the game files, such as " + ReflexPaths.ShortFolderExample(st?.destination ?? "") + ".";
            }
            else if ((error?.Message ?? "").IndexOf("source changed", StringComparison.OrdinalIgnoreCase) >= 0)
            {
                what = "A game file changed while it was being copied.";
                todo = "Steam may be updating or verifying the game. Wait until Steam is done, then try again.";
            }
            else
            {
                what = "Something went wrong: " + (ie?.InnerException?.Message ?? error?.Message ?? "unknown error") + ".";
                todo = "Try again. If it keeps happening, COPY ERROR DETAILS and include them in a bug report on the project's issue tracker.";
            }
            ImportRow(body, "×", what, todo);
            Para("The unfinished files were removed. Your game wasn't changed.", body, "mxa-note");
            var again = ImportButton(buttons, "TRY AGAIN", "ou-import-retry", () => { if (st?.source != null) { st.plan = null; ShowImportCheck(st.source, st.found); } else ShowImportWelcome(false); }, true);
            if (st?.plan != null) ImportButton(buttons, "ANOTHER PLACE", "ou-import-failed-space", () => ShowImportSpace(null));
            AddCopyDetails(buttons, error);
            AddHelp(buttons);
            ImportButton(buttons, "BACK", "ou-import-back", () => ShowImportWelcome(st?.fromLibrary ?? false));
            _nav.DefaultItem = () => again; _nav.OnCancel = () => { ShowImportWelcome(st?.fromLibrary ?? false); return true; };
            ImportHints(); _nav.Focus(again);
        }

        /// OPEN HELP: HELP - IMPORTING YOUR GAME.txt next to Mods/ (written now when missing).
        void AddHelp(VisualElement buttons)
        {
            ImportButton(buttons, "OPEN HELP", "ou-import-help", () => { string f = ReflexAssetImport.EnsureHelp(); if (f != null) { try { Application.OpenURL(new Uri(f).AbsoluteUri); } catch (Exception) { } } });
        }

        void AddCopyDetails(VisualElement buttons, Exception error)
        {
            string details = FormatImportErrorDetails(error, Application.version, Application.platform.ToString(), SystemInfo.operatingSystem, Application.unityVersion);
            VisualElement b = null;
            b = ImportButton(buttons, "COPY ERROR DETAILS", "import-copy-error-details", () =>
            {
                var l = b.Q<Label>();
                try { GUIUtility.systemCopyBuffer = details; if (l != null) l.text = "COPIED  ·  NOTHING SENT"; }
                catch (Exception e) { if (l != null) l.text = "COULD NOT COPY"; Debug.LogWarning("[import] " + e.Message); }
            });
        }

        void ShowImportCancelled()
        {
            var st = _imp;
            ImportPage("ou-import-cancelled", "IMPORT CANCELLED", 4, out var body, out var buttons);
            ImportRow(body, "✓", "The import was stopped", "The files copied so far were removed. Your game wasn't changed.");
            var again = ImportButton(buttons, "START AGAIN", "ou-import-retry", () => { if (st?.source != null) ShowImportCheck(st.source, st.found); else ShowImportWelcome(false); }, true);
            ImportButton(buttons, "BACK", "ou-import-back", () => ShowImportWelcome(st?.fromLibrary ?? false));
            _nav.DefaultItem = () => again; _nav.OnCancel = () => { ShowImportWelcome(st?.fromLibrary ?? false); return true; };
            ImportHints(); _nav.Focus(again);
        }

        // ------------------------------------------------------------------------------------------------ 5 DONE

        void ShowImportDone(ReflexAssetImport.Result result)
        {
            var st = _imp;
            ImportPage("ou-import-done", "YOUR GAME IS READY", 5, out var body, out var buttons);
            ImportRow(body, "✓", result.files.ToString("N0") + " game files imported (" + ReflexAssetImport.GB(result.bytes) + ")", "Into " + result.directory + ". " + (result.resumed ? "The unfinished import was completed. " : "") + "Your original game wasn't changed.");
            if (st != null && st.Moved)
                ImportRow(body, "!", "Your game files are on " + ReflexAssetImport.PlaceName(result.directory), OpenReflex.Name + " will always look for them there, after updates too. Keep that drive connected when you play. To move them back, import again into the default place.");
            var left = (result.warnings ?? new string[0]).Where(w => w.StartsWith("Skipped ", StringComparison.Ordinal) || w.StartsWith("Left out", StringComparison.Ordinal)).ToList();
            if (result.skippedFiles != null && result.skippedFiles.Length > 0)
                ImportRow(body, "…", "Left out: " + result.skippedFiles.Length + " mod file" + (result.skippedFiles.Length == 1 ? "" : "s") + " from your game folder", string.Join(", ", result.skippedFiles.Take(6)) + (result.skippedFiles.Length > 6 ? " …" : "") + ". They are still in the game's Database folder (HELP - IMPORTING YOUR GAME.txt says how to remove them).");
            foreach (var w in left.Where(w => w.StartsWith("Skipped ", StringComparison.Ordinal)).Take(3))
                ImportRow(body, "…", w.Substring(0, Math.Min(w.Length, w.IndexOf(':') > 0 ? w.IndexOf(':') : w.Length)), "This version can't read it, so it was left out. Everything else was imported.");
            if ((result.warnings ?? new string[0]).Any(w => w.IndexOf("FresnoB", StringComparison.OrdinalIgnoreCase) >= 0 && w.IndexOf("excluded", StringComparison.OrdinalIgnoreCase) >= 0))
                ImportRow(body, "…", "One unused, cut track couldn't be restored", "Nothing you can race is missing.");
            bool library = st?.fromLibrary == true;
            var play = ImportButton(buttons, library ? "DONE" : "LET'S RIDE", "ou-import-play", () =>
            {
                _imp = null;
                if (library) ShowLibrary(_libraryFromMain);
                else if (MotocardWelcomeDue()) ShowMotocardWelcome(ShowMain); else ShowMain();
            }, true);
            string dir = result.directory;
            ImportButton(buttons, "OPEN GAME FILES", "ou-import-open", () => { try { Application.OpenURL(new Uri(dir + Path.DirectorySeparatorChar).AbsoluteUri); } catch (Exception) { } });
            _nav.DefaultItem = () => play; _nav.OnCancel = () => false;
            Hints((OuUi.KeySubmit, "SELECT"));
            _nav.Focus(play);
        }

        internal static string FormatImportErrorDetails(Exception error, string version, string platform, string operatingSystem, string unityVersion)
        {
            return OpenReflex.Name + " import failure\nClient version: " + version + "\nPlatform: " + platform +
                "\nOperating system: " + operatingSystem + "\nUnity: " + unityVersion + "\n\n" +
                (error?.ToString() ?? "No exception details were supplied.");
        }

        // ------------------------------------------------------------------------------------------------ tests

        /// Tests (the showcase): the import's state, to show a page with given data.
        public void TestImportState(string source, string destination, ImportPlan plan, List<DbModGroup> mods, long? free, List<ImportLeftover> leftovers = null, string note = null, Exception error = null)
        {
            _imp = new ImportState { source = source, destination = destination ?? ReflexAssetImport.Destination, plan = plan, mods = mods ?? new List<DbModGroup>(), free = free, leftovers = leftovers ?? new List<ImportLeftover>(), note = note, error = error, found = "STEAM", check = Task.CompletedTask };
            _imp.planKey = source + "|";
        }
        public bool ImportRunning => _assetImport != null;
        /// The check's worker finished (tests).
        public bool ImportCheckDone => _imp?.check != null && _imp.check.IsCompleted;
        /// Where the next check / import puts the game files (tests: a scratch folder).
        public void TestImportDestination(string destination) { _imp = _imp ?? new ImportState(); _imp.destination = destination; }
        public void TestBeginImport() => BeginImport();
        public string ImportNote => _imp?.note;
        public void TestImportFailed(Exception e) => ImportFailed(e);
        public void TestImportDone(ReflexAssetImport.Result r) => ShowImportDone(r);
        public void TestImportCheckPage() => ShowImportCheckPage();
        public void TestImportBrowseDest() => ShowImportBrowseDest();
        public void TestImportCancelled() => ShowImportCancelled();
    }
}

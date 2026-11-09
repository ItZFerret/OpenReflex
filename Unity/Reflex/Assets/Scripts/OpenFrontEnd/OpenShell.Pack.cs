// Packed game files (core/plan/RXARCHIVE.md; Content/ReflexDataPack.cs): Assets.rxdata + Tracks/<level>.rxtrack in the imported library.
//   the prompt   packing is not optional: a library imported before packing existed (loose files) is packed at startup, before the shell
//                connects to it (nothing reads the library yet, the menus' warm test-ride session waits while a pack runs: OpenArenaWarm.Enabled).
//                The page has PACK NOW only (no way past it but packing). Not shown when REFLEX_GAME_DIR names the folder (developer and test runs)
//                or with REFLEX_NO_PACK_PROMPT=1. A pack that fails leaves the rest loose (the game plays from it) and is asked again next start.
//   the page     the pack's progress (from the startup prompt it can't be stopped; GAME LIBRARY's can: STOP keeps what is packed so far, a valid
//                library), then what it saved, then the shell goes on.
//   the library  GAME LIBRARY's card says whether the files are packed and how big the folder is (measured on a worker), and offers
//                PACK GAME FILES while loose files are left.
using System;
using System.IO;
using System.Threading;
using System.Threading.Tasks;
using Reflex.Port.Content;
using UnityEngine;
using UnityEngine.UIElements;

namespace Reflex.Port.OpenFrontEnd
{
    public sealed partial class OpenShell
    {
        static Task<ReflexDataPack.Result> s_pack;
        static ReflexDataPack.Progress s_packProgress;
        static string s_packMessage;
        static CancellationTokenSource s_packCancel;
        static bool s_packAsked;

        /// Whether packing has work: loose files, or archives packed before dedupe (ReflexDataPack.Worth).
        static bool PackWorth(string dir) => ReflexDataPack.Worth(dir);
        static bool PackRunning => s_pack != null && !s_pack.IsCompleted;

        // ------------------------------------------------------------------------------------------------ the startup prompt

        /// The prompt is due: an imported library with loose files, not asked this run (an old DON'T ASK AGAIN is no longer honoured).
        bool PackPromptDue()
        {
            if (s_packAsked || PackRunning || !Reflex.Port.RxCore.Bridge.OpenReflexSelector.UseOpenReflex) return false;
            if (!string.IsNullOrWhiteSpace(Environment.GetEnvironmentVariable("REFLEX_GAME_DIR")) || Environment.GetEnvironmentVariable("REFLEX_NO_PACK_PROMPT") == "1") return false;
            return PackWorth(ReflexInstall.Dir);
        }

        /// PACK YOUR GAME FILES (before the shell connects): PACK NOW is the only way on; next: the rest of the start.
        void ShowPackPrompt(Action next)
        {
            s_packAsked = true;
            string dir = ReflexInstall.Dir;
            ImportPage("ou-pack-prompt", "PACK YOUR GAME FILES", 0, out var body, out var buttons);
            bool again = ReflexDataPack.OldArchives(dir);   // (packed before dedupe: the archives packed again, smaller)
            OuTheme.Section(body, again ? "YOUR GAME FILES CAN BE SMALLER" : "YOUR GAME FILES ARE LOOSE");
            var size = Para("Measuring your game files…", body);
            Para(again ? "Your game files were packed before " + OpenReflex.Name + " learned to keep one copy of the files tracks share (a track's B layout shares most of its files). Packing them again roughly halves them."
                       : OpenReflex.Name + " now keeps your imported game files packed: every level in a file of its own (Tracks), the rest in one compressed archive, every file tracks share kept once. " +
                         "They take about a fifth of the space and tracks load faster.", body);
            ImportRow(body, "✓", "Nothing is lost", "Every file is read back and compared with the original before its loose copy is removed. Your game install isn't touched.");
            ImportRow(body, "✓", "Once", "This runs once. " + OpenReflex.Name + " keeps your game files packed from now on, and the next import packs them as it goes.");
            ImportRow(body, "…", "A few minutes", "Usually 1 to 5 minutes. It needs a little free space while it works (about 3 GB).");
            var task = Task.Run(() => ReflexDataPack.Plan(dir));
            size.schedule.Execute(() =>
            {
                if (!task.IsCompleted) return;
                var (levels, bytes) = task.IsFaulted ? (0, 0L) : task.Result;
                size.text = bytes > 0 ? (again ? "Your game files: " : "Loose now: ") + ReflexAssetImport.GB(bytes) + (levels > 0 ? " in " + levels + " levels and the shared assets" : "") + ". Packed: about " + ReflexAssetImport.GB(bytes / 5) + "." : "Your game files are ready to pack.";
            }).Every(250).Until(() => task.IsCompleted && size.text != "Measuring your game files…");
            var pack = ImportButton(buttons, "PACK NOW", "ou-pack-now", () => { StartPack(dir); ShowPackProgress(next, false); }, true);
            _nav.DefaultItem = () => pack;
            _nav.OnCancel = () => true;   // (BACK doesn't skip it)
            Hints((OuUi.KeySubmit, "SELECT"));
            _nav.Focus(pack);
        }

        /// The pack's progress, then its result and CONTINUE (next). canStop: STOP (GAME LIBRARY's pack), none for the startup pack.
        void ShowPackProgress(Action next, bool canStop = true)
        {
            ImportPage("ou-pack-progress", "PACKING YOUR GAME FILES", 0, out var body, out var buttons);
            var step = Para("Starting…", body);
            var bar = OuUi.Div("ou-import-bar", body);
            bar.style.height = 14; bar.style.backgroundColor = new Color(1, 1, 1, 0.12f); bar.style.marginTop = 6; bar.style.marginBottom = 12;
            var fill = OuUi.Div("ou-import-fill", bar); fill.style.height = Length.Percent(100); fill.style.width = Length.Percent(0); fill.style.backgroundColor = ImpOrange;
            var detail = Para("", body, "mxa-note");
            if (canStop)
            {
                VisualElement stop = ImportButton(buttons, "STOP", "ou-pack-stop", () => { s_packCancel?.Cancel(); }, true);
                _nav.DefaultItem = () => stop;
                _nav.OnCancel = () => { if (PackRunning) s_packCancel?.Cancel(); return true; };
                Hints((OuUi.KeySubmit, "SELECT"));
                _nav.Focus(stop);
            }
            else { _nav.DefaultItem = null; _nav.OnCancel = () => true; Hints(); }
            bool finished = false;
            step.schedule.Execute(() =>
            {
                if (finished) return;
                var p = s_packProgress;
                if (PackRunning || s_pack == null)
                {
                    if (p == null) return;
                    double frac = p.total > 0 ? Math.Min(1, (double)p.done / p.total) : 0;
                    fill.style.width = Length.Percent((float)(frac * 100));
                    step.text = (p.phase == 0 ? "Reading " : p.phase == 3 ? "Checking " : p.phase == 4 ? "Tidying up " : "Packing ") + (p.current ?? "") + "  ·  " + Math.Floor(frac * 100) + " %";
                    detail.text = ReflexAssetImport.GB(p.total / 3) + " of game files: read, packed and checked one track at a time.";
                    if (s_packCancel != null && s_packCancel.IsCancellationRequested) step.text = "Stopping…";
                    return;
                }
                finished = true;
                fill.style.width = Length.Percent(100);
                step.text = s_packMessage ?? "Done.";
                detail.text = s_pack.IsFaulted ? "Nothing was lost: the files that were not packed stay loose and the game uses them for now. It tries again next time you start." :
                              s_pack.IsCanceled ? "What was packed stays packed; the rest stays loose. GAME LIBRARY > PACK GAME FILES goes on from there." :
                              "Your game files are packed. GAME LIBRARY shows them.";
                buttons.Clear();
                var go = ImportButton(buttons, "CONTINUE", "ou-pack-continue", next, true);
                _nav.DefaultItem = () => go;
                _nav.OnCancel = () => { next(); return true; };
                _nav.Focus(go);
            }).Every(200);
        }

        // ------------------------------------------------------------------------------------------------ the library card

        static ReflexDataPack.State s_packState; static string s_packStateDir; static int s_packStateGen = -1; static Task<ReflexDataPack.State> s_packMeasure;

        static string PackLabel()
        {
            var p = s_packProgress;
            if (PackRunning && p != null)
            {
                string what = p.phase == 0 ? "READING" : p.phase == 3 ? "CHECKING" : p.phase == 4 ? "TIDYING UP" : "PACKING";
                return what + "  " + (p.total > 0 ? 100L * p.done / p.total : 0) + " %";
            }
            return "PACK GAME FILES";
        }

        /// The card's line about the library's files ("PACKED · 7.9 GB · 83 archives"), measured on a worker and filled in when known.
        void PackStatus(string dir, VisualElement copy)
        {
            var line = OuUi.Text("GAME FILES  ·  measuring…", "mxa-library-state", copy);
            line.style.whiteSpace = WhiteSpace.Normal;
            void Fill()
            {
                var st = s_packState;
                if (st == null) return;
                string total = ReflexAssetImport.GB(st.totalBytes);
                line.text = st.Packed && st.oldArchives > 0 ? "PACKED  ·  " + total + "  ·  " + st.archives + " archives  ·  PACK GAME FILES makes them about half that"
                          : st.Packed ? "PACKED  ·  " + total + "  ·  " + st.archives + " archives"
                          : st.Partly ? "PARTLY PACKED  ·  " + total + "  ·  " + ReflexAssetImport.GB(st.looseBytes) + " still loose"
                          : "LOOSE FILES  ·  " + total + "  ·  " + st.totalFiles.ToString("N0") + " files (PACK GAME FILES: about " + ReflexAssetImport.GB(st.looseBytes / 5) + " packed)";
            }
            bool Stale() => s_packStateDir != dir || s_packStateGen != RxFiles.Generation;
            void Measure()
            {
                if (s_packMeasure != null && !s_packMeasure.IsCompleted) return;
                string d = dir; int gen = RxFiles.Generation;
                s_packMeasure = Task.Run(() => { var st = ReflexDataPack.Measure(d); s_packState = st; s_packStateDir = d; s_packStateGen = gen; return st; });
            }
            if (!Stale()) Fill(); else Measure();
            line.schedule.Execute(() =>
            {
                if (PackRunning) { line.text = "GAME FILES  ·  " + PackLabel().ToLowerInvariant().Replace("pack game files", "packing"); return; }
                if (Stale()) { Measure(); return; }
                Fill();
            }).Every(500);
        }

        /// The library card's PACK GAME FILES button (OpenShell.Library.cs), or nothing when there is nothing to pack.
        void PackAction(string dir, Func<string, Action, string, VisualElement> libraryAction)
        {
            if (!PackRunning && !PackWorth(dir)) return;
            var button = libraryAction(PackLabel(), () => { if (!PackRunning) StartPack(dir); ShowPackProgress(() => ShowLibrary(_libraryFromMain)); }, "ou-library-pack");
            var label = button.Q<Label>();
            button.schedule.Execute(() => { if (label != null) label.text = PackLabel(); }).Every(250);
        }

        static void StartPack(string dir)
        {
            if (PackRunning) return;
            s_packMessage = null;
            s_packProgress = new ReflexDataPack.Progress();
            s_packCancel = new CancellationTokenSource();
            Debug.Log("[pack] PACK GAME FILES: " + dir);
            s_pack = ReflexDataPack.Pack(dir, p => s_packProgress = p, s_packCancel.Token);
            s_pack.ContinueWith(t =>
            {
                if (t.IsCanceled) { s_packMessage = "Stopped."; Debug.Log("[pack] stopped; what was packed stays packed"); }
                else if (t.IsFaulted) { var e = t.Exception?.GetBaseException(); s_packMessage = "Packing stopped: " + e?.Message; Debug.LogWarning("[pack] failed, the rest stays loose: " + e); }
                else
                {
                    var r = t.Result;
                    s_packMessage = r.archives.Count == 0 ? "Nothing to pack: your game files are already packed."
                                  : "Packed: " + ReflexAssetImport.GB(r.looseBytes) + " of files now take " + ReflexAssetImport.GB(r.packedBytes) + ".";
                    Debug.Log($"[pack] {r.levels} levels, {r.files} files: {r.looseBytes >> 20} MB loose -> {r.packedBytes >> 20} MB packed in {r.seconds:0.0} s; loose copies removed: {r.removed}");
                }
                RxFiles.Reset();
                AliveLevels.Reset();
            }, TaskScheduler.Default);
        }
    }
}

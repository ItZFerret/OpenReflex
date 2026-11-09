// TEST ONLY: page `importflow` (w-importflow): every page and state of IMPORT YOUR GAME (Scripts/OpenFrontEnd/OpenShell.ImportFlow.cs), opened
// as a player gets there, with real imports of scratch installs (tests/importflow/showcase_fixture.sh: REFLEX_IMPORTFLOW_FIXTURE=<its folder>).
// The client root is the run folder (run.py), so the default game files folder, the pointer file and the staging folders are the run's own.
// Muted. Shots if-01 .. if-19; checks: nothing is left behind by a cancel or a full disk, the pointer names the moved folder afterwards.
using System;
using System.Collections;
using System.IO;
using System.Linq;
using Reflex.Port.Content;
using Reflex.Port.OpenFrontEnd;
using UnityEngine;
using UnityEngine.UIElements;

namespace Reflex.Port.Showcase
{
    static class UiShowcaseImportFlow
    {
        const string Tag = "[ui-showcase] [importflow] ";
        static int fails;
        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        static void Register() => UiShowcase.Register("importflow", Run);

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
        static void Mute()
        {
            if ((Environment.GetEnvironmentVariable("REFLEX_UI_SHOWCASE") ?? "").IndexOf("importflow", StringComparison.Ordinal) < 0) return;
            AudioListener.volume = 0f; AudioListener.pause = true;
            try { Reflex.Port.Options.ReflexSettings.MasterVolume = 0f; } catch (Exception) { }
            var go = new GameObject("importflow mute TEST ONLY"); UnityEngine.Object.DontDestroyOnLoad(go); go.AddComponent<KeepMuted>();
        }
        sealed class KeepMuted : MonoBehaviour { void LateUpdate() { AudioListener.volume = 0f; AudioListener.pause = true; } }

        static void Check(bool ok, string what) { if (ok) Debug.Log(Tag + "ok " + what); else { fails++; Debug.LogError(Tag + "FAIL " + what); } }
        static OpenShell Shell => OpenShell.Instance;
        static VisualElement Item(string name) => Shell.Nav.Items().FirstOrDefault(e => e.name == name) ?? Shell.Layer.Q<VisualElement>(name);
        static bool Press(string name)
        {
            var e = Item(name);
            if (e?.userData is OuNavItem it && it.submit != null) { Shell.Nav.Focus(e); it.submit(); Debug.Log(Tag + "press " + name); return true; }
            Check(false, "no button " + name); return false;
        }
        static string Texts(string name) { var e = Shell.Layer.Q<VisualElement>(name); return e == null ? "" : string.Join(" | ", e.Query<Label>().ToList().Select(l => l.text)); }
        static IEnumerator Wait(Func<bool> done, float seconds)
        {
            float t0 = Time.realtimeSinceStartup;
            while (!done() && Time.realtimeSinceStartup - t0 < seconds) yield return null;
        }
        static string Stage(string parent) => Directory.Exists(parent) ? Directory.GetDirectories(parent, ".rxi-*").FirstOrDefault() : null;

        static IEnumerator Run()
        {
            UiShowcaseNav.IsolateDevices();
            string fx = Environment.GetEnvironmentVariable("REFLEX_IMPORTFLOW_FIXTURE");
            if (string.IsNullOrEmpty(fx)) { Check(false, "REFLEX_IMPORTFLOW_FIXTURE not set"); yield break; }
            string modded = Path.Combine(fx, "src-modded", "MX vs ATV Reflex"), small = Path.Combine(fx, "src-small", "MX vs ATV Reflex");
            string client = ReflexAssetImport.ClientDirectory, moved = Path.Combine(fx, "dest", "OpenReflex", "game files");
            Debug.Log(Tag + "client " + client + "; default destination " + ReflexAssetImport.DefaultDestination);
            Check(ReflexAssetImport.MovedDestination == null, "no pointer at the start");
            var shell = Shell;

            // 1 ABOUT (the first run)
            shell.ShowImportWelcome(false); yield return UiShowcase.Frames(40);
            yield return UiShowcase.Shot("if-01-welcome");
            // 2 FIND: the small install offered (REFLEX_GAME_DIR is one of the places looked at)
            string had = Environment.GetEnvironmentVariable("REFLEX_GAME_DIR");
            Environment.SetEnvironmentVariable("REFLEX_GAME_DIR", modded);
            Press("ou-import-start"); yield return UiShowcase.Frames(90);
            Check(shell.Layer.Q<VisualElement>("ou-import-find") != null, "GET STARTED opens FIND YOUR GAME");
            yield return UiShowcase.Shot("if-02-find");
            Environment.SetEnvironmentVariable("REFLEX_GAME_DIR", had);
            // the browser: a folder above the game (one folder down), then the Database folder itself
            Press("ou-import-browse"); yield return UiShowcase.Frames(30);
            shell.SetInstallPath(Path.GetDirectoryName(modded)); yield return UiShowcase.Frames(30);
            string hint = Texts("ou-install-status");
            Check(hint.Contains("one folder down"), "the browser says the game is one folder down: " + hint);
            yield return UiShowcase.Shot("if-03-browse-hint");
            shell.SetInstallPath(modded); yield return UiShowcase.Frames(20);
            yield return UiShowcase.Shot("if-04-browse-found");

            // 3 CHECK of the modded install: C: (the run folder) has 3.1 GB, every other place 412 GB; an unfinished import left behind
            Environment.SetEnvironmentVariable("REFLEX_IMPORT_TEST_FREE_BYTES", client + "=3.1G;*=412G");
            Environment.SetEnvironmentVariable("REFLEX_IMPORT_TEST_MOUNTS", "D: (GAMES)=" + Path.Combine(fx, "dest") + ";E: (USB)=" + Path.Combine(fx, "usb"));
            Directory.CreateDirectory(Path.Combine(fx, "usb"));
            Environment.SetEnvironmentVariable("REFLEX_IMPORT_TEST_FREE_BYTES", client + "=3.1G;" + Path.Combine(fx, "usb") + "=9G;*=412G");
            string leftover = Path.Combine(client, ".rxi-1a2b3c4d"); Directory.CreateDirectory(Path.Combine(leftover, "Assets")); File.WriteAllBytes(Path.Combine(leftover, "Assets", "part.bin"), new byte[3 << 20]);
            Press("ou-install-use"); yield return UiShowcase.Frames(10);
            Check(shell.Layer.Q<VisualElement>("ou-import-check") != null, "USE THIS FOLDER opens the check");
            yield return UiShowcase.Frames(20);
            if (!shell.ImportCheckDone) yield return UiShowcase.Shot("if-05-checking");
            yield return Wait(() => shell.ImportCheckDone, 180); yield return UiShowcase.Frames(30);
            string check = Texts("ou-import-check");
            Debug.Log(Tag + "check: " + check);
            Check(check.Contains("Not enough space") && check.Contains(ReflexAssetImport.GB((long)(3.1 * (1L << 30))) + " free"), "not enough space, in numbers");
            Check(check.Contains("mods in your game's Database folder") && check.Contains("unfinished import"), "the mods and the leftover are listed");
            Check(!(Item("ou-import-go")?.enabledSelf ?? true), "START IMPORT is off while the space is short");
            yield return UiShowcase.Shot("if-06-check-problems");
            // MODS
            Press("ou-import-mods"); yield return UiShowcase.Frames(40);
            string mods = Texts("ou-import-mods-page");
            Check(mods.Contains("Rider gear pack") && mods.Contains("Bike graphics kit pack") && mods.Contains("Beta_Nat_Track_Slot_5") && mods.Contains("MXTables_DLC008"), "the packs are listed by kind");
            yield return UiShowcase.Shot("if-07-mods");
            Press("ou-import-mods-move"); yield return UiShowcase.Frames(30);
            Check(shell.PopupOpen, "MOVE asks first");
            yield return UiShowcase.Shot("if-08-mods-move-confirm");
            Press("popup-move-no"); yield return UiShowcase.Frames(10);
            Check(File.Exists(Path.Combine(modded, "Database", "MXTables_DLC010.dx9.database")), "CANCEL moved nothing");
            Press("ou-import-mods-back"); yield return Wait(() => shell.ImportCheckDone, 120); yield return UiShowcase.Frames(20);
            // SPACE: the drives
            Press("ou-import-space"); yield return UiShowcase.Frames(20); yield return Wait(() => shell.Layer.Q<VisualElement>("ou-import-drive-1") != null, 30); yield return UiShowcase.Frames(20);
            Check(Texts("ou-import-space-page").Contains("NOT ENOUGH SPACE"), "the space page");
            yield return UiShowcase.Shot("if-09-space");
            Press("ou-import-dest-browse"); yield return UiShowcase.Frames(40);
            yield return UiShowcase.Shot("if-10-dest-browse");
            // a folder on "another drive" (the fixture's dest; '*' = 412 GB), then CLEAN UP the leftover
            shell.TestImportDestination(moved); shell.ShowImportCheck(modded, "STEAM"); yield return Wait(() => shell.ImportCheckDone, 120); yield return UiShowcase.Frames(20);
            Check(Texts("ou-import-check").Contains("Game files go to: " + moved), "the check shows the new place");
            yield return UiShowcase.Shot("if-11-check-ready");
            // back to the default place: CLEAN UP the unfinished import found there
            shell.TestImportDestination(ReflexAssetImport.DefaultDestination); shell.ShowImportCheck(modded, "STEAM"); yield return Wait(() => shell.ImportCheckDone, 120); yield return UiShowcase.Frames(20);
            Press("ou-import-cleanup"); yield return Wait(() => shell.ImportCheckDone, 120); yield return UiShowcase.Frames(20);
            Check(!Directory.Exists(leftover), "CLEAN UP removed the leftover");
            yield return UiShowcase.Shot("if-11b-cleaned-up");
            Environment.SetEnvironmentVariable("REFLEX_IMPORT_TEST_FREE_BYTES", null);

            // 4 IMPORT (the small install, slowed), CANCEL
            Environment.SetEnvironmentVariable("REFLEX_IMPORT_TEST_SLOW_MS", "40");
            shell.TestImportDestination(ReflexAssetImport.DefaultDestination);
            shell.ShowImportCheck(small, "FOUND"); yield return Wait(() => shell.ImportCheckDone, 120); yield return UiShowcase.Frames(10);
            var walk = UiShowcase.AfterShot; UiShowcase.AfterShot = null;   // (no controller walk while the import runs: it would outlast it)
            Press("ou-import-go"); yield return UiShowcase.Frames(10);
            Check(shell.ImportRunning, "START IMPORT runs it");
            float t0 = Time.realtimeSinceStartup; while (Time.realtimeSinceStartup - t0 < 5.5f) yield return null;
            yield return UiShowcase.Shot("if-12-progress");
            Press("ou-import-cancel"); yield return UiShowcase.Frames(20);
            yield return UiShowcase.Shot("if-13-cancel-confirm");
            Press("popup-cancel-yes"); yield return Wait(() => !shell.ImportRunning, 60); yield return UiShowcase.Frames(30);
            Check(!Directory.Exists(ReflexAssetImport.DefaultDestination) && Stage(client) == null, "after CANCEL: no game files, no staging folder");
            UiShowcase.AfterShot = walk;
            yield return UiShowcase.Shot("if-14-cancelled");
            Environment.SetEnvironmentVariable("REFLEX_IMPORT_TEST_SLOW_MS", null);

            // the disk fills up mid-way (the fault injector at 20 MB): the space page, nothing left behind
            Environment.SetEnvironmentVariable("REFLEX_IMPORT_TEST_DISK_FULL_AT", (20L << 20).ToString());
            shell.ShowImportCheck(small, "FOUND"); yield return Wait(() => shell.ImportCheckDone, 120); yield return UiShowcase.Frames(10);
            Press("ou-import-go");
            Environment.SetEnvironmentVariable("REFLEX_IMPORT_TEST_FREE_BYTES", client + "=0.2G;" + Path.Combine(fx, "usb") + "=9G;*=412G");   // (what a full drive shows afterwards)
            yield return Wait(() => !shell.ImportRunning, 120); yield return UiShowcase.Frames(30);
            Check(shell.Layer.Q<VisualElement>("ou-import-space-page") != null && (shell.ImportNote ?? "").Contains("filled up"), "a full disk lands on the space page: " + shell.ImportNote);
            Check(!Directory.Exists(ReflexAssetImport.DefaultDestination) && Stage(client) == null, "after the full disk: no game files, no staging folder");
            Check(Item("import-copy-error-details") != null, "COPY ERROR DETAILS is there");
            yield return UiShowcase.Shot("if-15-disk-full");
            Environment.SetEnvironmentVariable("REFLEX_IMPORT_TEST_DISK_FULL_AT", null);
            Environment.SetEnvironmentVariable("REFLEX_IMPORT_TEST_FREE_BYTES", null);

            // the import stops at a mod's file (as support's MXTables_DLC008 cast error): the mods page says so
            shell.ShowImportCheck(modded, "STEAM"); yield return Wait(() => shell.ImportCheckDone, 120);
            shell.TestImportFailed(new ImportException(ImportException.Failure.BadSourceFile, "reading database MXTables_DLC008", "While reading MXTables_DLC008.dx9.database: Specified cast is not valid.", new InvalidCastException("Specified cast is not valid."), "MXTables_DLC008.dx9.database"));
            yield return UiShowcase.Frames(40);
            Check(Texts("ou-import-mods-page").Contains("The import stopped at MXTables_DLC008.dx9.database"), "a mod's file that stopped the import: the mods page names it");
            yield return UiShowcase.Shot("if-16-stopped-at-mod");
            // any other failure
            shell.TestImportFailed(new ImportException(ImportException.Failure.Other, "copying Videos/Intro.bik", "While copying Videos/Intro.bik: Access to the path is denied.", new UnauthorizedAccessException("Access to the path \"C:\\Users\\player\\AppData\\Roaming\\mxa-launcher\\clients\\ItZFerret--reflex-releases\\.rxi-f0ea4f04\\Videos\\Intro.bik\" is denied.")));
            yield return UiShowcase.Frames(40);
            yield return UiShowcase.Shot("if-17-failed");

            // 5 DONE: the small install imported to the "other drive"; the pointer remembers it
            shell.TestImportDestination(moved);
            shell.ShowImportCheck(small, "FOUND"); yield return Wait(() => shell.ImportCheckDone, 120); yield return UiShowcase.Frames(10);
            Press("ou-import-go"); yield return Wait(() => !shell.ImportRunning, 300); yield return UiShowcase.Frames(40);
            Check(shell.Layer.Q<VisualElement>("ou-import-done") != null, "the import finished: " + Texts("ou-import-done"));
            Check(ReflexAssetImport.MovedDestination != null && ReflexPaths.SamePath(ReflexAssetImport.MovedDestination, moved), "the pointer names the other drive's folder: " + ReflexAssetImport.MovedDestination);
            Check(ReflexPaths.SamePath(ReflexAssetImport.Destination, moved) && File.Exists(Path.Combine(moved, ReflexAssetImport.ExtractedMarker)), "Destination is the moved folder, imported");
            Check(ReflexInstall.Candidates(false).Any(c => ReflexPaths.SamePath(c, moved)), "a later start looks there (ReflexInstall.Candidates)");
            Check(Stage(Path.GetDirectoryName(moved)) == null, "no staging folder next to it");
            yield return UiShowcase.Shot("if-18-done");
            shell.ShowLibrary(true); yield return UiShowcase.Frames(40);
            yield return UiShowcase.Shot("if-19-library");
            Debug.Log(Tag + (fails == 0 ? "ALL PASS" : fails + " FAILED"));
            if (fails > 0) UiShowcaseNav.Check(false, "importflow", fails + " checks failed");
            yield return UiShowcase.Frames(5);
        }
    }
}

// OPTIONS > SYSTEM (Open Unleashed's SYSTEM page: MxaOptionsMenu.SaveData.cs / .Support.cs):
//   GAME FOLDER  the Reflex install the client reads (ReflexInstall); ENTER opens the shell's Game Library (ReflexOptionsHooks).
//   SAVE DATA    BACK UP NOW (a dated ZIP in Save/Backups: save.json, the game's own save, graphics.cfg), OPEN SAVE FOLDER, RESTORE
//                (choose a backup, press twice: the current save is backed up first, the game closes, the backup applies at the
//                next start).
//   SUPPORT      CREATE REPORT / OPEN LOG FOLDER: the platform's report (Reflex.Port.Platform.ReflexSupport; ReflexOptionsHooks override).
using System;
using System.IO;
using Reflex.Port.Content;
using UnityEngine;
using UnityEngine.UIElements;

namespace Reflex.Port.Options
{
    public sealed partial class ReflexOptionsMenu
    {
        static string s_saveStatus = "", s_supportStatus = "";
        int _restorePick; string _restoreArmed;

        static string ShortPath(string p)
        {
            if (string.IsNullOrEmpty(p)) return "(NOT SET)";
            p = ReflexSave.Redact(p);
            return p.Length <= 46 ? p : "…" + p.Substring(p.Length - 45);
        }

        VisualElement BuildSystem(VisualElement panel)
        {
            Section(panel, "GAME FILES");
            string dir = null; try { dir = ReflexInstall.Dir; } catch (Exception) { }
            var gf = Named(OptUi.Spinner("GAME FOLDER", () => ShortPath(dir), _ => { }, panel, () => OpenLibrary()), "game-folder");   // (Enter / A / a click open it; the arrows do not: a stick brushing left must not leave the page)
            if (ReflexOptionsHooks.OpenGameLibrary == null || ReflexClientLive.Runtime) gf.SetEnabled(false);
            Note(panel, ReflexClientLive.Runtime ? "The MX vs ATV Reflex install the client reads everything from. To change it, go back to the front end's GAME LIBRARY."
                                                 : "The MX vs ATV Reflex install the client reads everything from. Enter opens the GAME LIBRARY to choose another.");
            var first = gf.enabledSelf ? gf : null;
            if (!Reflex.Port.RxCore.Bridge.OpenReflexSelector.UseOpenReflex)
            {
                Named(OptUi.Spinner("FRONT END", () => Reflex.Port.OpenFrontEnd.FrontEndSwitch.Saved == Reflex.Port.OpenFrontEnd.FrontEndStyle.Open ? Reflex.Port.OpenFrontEnd.OpenReflex.Name : "ORIGINAL",
                    _ => Reflex.Port.OpenFrontEnd.FrontEndSwitch.Set(Reflex.Port.OpenFrontEnd.FrontEndSwitch.Saved == Reflex.Port.OpenFrontEnd.FrontEndStyle.Open ? Reflex.Port.OpenFrontEnd.FrontEndStyle.Original : Reflex.Port.OpenFrontEnd.FrontEndStyle.Open, false), panel), "front-end");
            }

            Section(panel, "SAVE DATA");
            Note(panel, "Your options and the game's own save (the profile, the career, the garage, the controls) are backed up once a day automatically.");
            var buttons = OptUi.Div("mxa-install-buttons", panel);
            buttons.style.flexDirection = FlexDirection.Row;
            Label status = null;
            void Say(string text) { s_saveStatus = text; if (status != null) status.text = text; }
            var backup = Named(OptUi.Button("BACK UP NOW", "mxa-start", () =>
            {
                try { string zip = ReflexSave.BackUp("manual"); Say("Backed up to " + ReflexSave.Redact(zip)); }
                catch (Exception e) { Say("Could not back up: " + e.Message); Debug.LogWarning("[Save] backup: " + e); }
            }, buttons), "save-backup");
            Named(OptUi.Button("OPEN SAVE FOLDER", "mxa-secondary", () => ReflexSave.Reveal(File.Exists(ReflexSave.FilePath) ? ReflexSave.FilePath : ReflexSave.Folder), buttons), "save-open-folder");
            Note(panel, "BACK UP NOW makes a dated ZIP in the Save folder's Backups (your options, the game's own save, your graphics.cfg); the last ten daily ones are kept. You can also copy the Save folder anywhere you like.");
            Named(OptUi.Spinner("RESTORE", () =>
            {
                var l = ReflexSave.Backups();
                if (l.Count == 0) return "NO BACKUPS YET";
                _restorePick = Mathf.Clamp(_restorePick, 0, l.Count - 1);
                string name = Path.GetFileNameWithoutExtension(l[_restorePick]).ToUpperInvariant();
                return _restoreArmed == l[_restorePick] ? "PRESS AGAIN: " + name : name;
            }, d => { var n = ReflexSave.Backups().Count; if (n > 0) _restorePick = ((_restorePick + d) % n + n) % n; _restoreArmed = null; }, panel, () =>
            {
                var l = ReflexSave.Backups(); if (l.Count == 0) return;
                string pick = l[Mathf.Clamp(_restorePick, 0, l.Count - 1)];
                if (_restoreArmed != pick) { _restoreArmed = pick; Say("Press again to restore " + Path.GetFileNameWithoutExtension(pick) + ". Your current save is backed up first."); return; }
                _restoreArmed = null;
                try
                {
                    if (!ReflexSave.Restore(pick)) { Say("That file is not a save backup (or this run does not write the save)."); return; }
                    Say(Application.isEditor ? "Restored: it applies when you start the game again." : "Restored: the game closes now and starts with that backup next time.");
                    if (!Application.isEditor) panel.schedule.Execute(Application.Quit).ExecuteLater(2500);
                }
                catch (Exception e) { Say("Could not restore: " + e.Message); Debug.LogWarning("[Save] restore: " + e); }
            }), "save-restore");
            Note(panel, "Press Enter on a backup twice to restore it: your current save is backed up first, the game closes and starts with the backup next time.");
            status = Keep(panel, s_saveStatus);
            Keep(panel, "Save folder: " + ReflexSave.Redact(ReflexSave.Folder) + (ReflexSave.Writable ? "" : "  (this run only reads it)"));
            if (ReflexSave.Origin == "migrated")
                Keep(panel, $"Your {ReflexSave.MigratedKeys} settings from the earlier versions were moved into the save file this session (a backup of them is in Backups, \"before the save system\").");
            if (ReflexSave.RestoredThisStart != null) Keep(panel, "A backup was restored at this start.");

            Section(panel, "SUPPORT");
            Note(panel, "Game crashing or misbehaving? CREATE REPORT puts the logs and system details into one ZIP to send with a short description.");
            var sb = OptUi.Div("mxa-install-buttons", panel);
            sb.style.flexDirection = FlexDirection.Row;
            Label sstatus = null;
            var create = Named(OptUi.Button("CREATE REPORT", "mxa-start", () =>
            {
                if (ReflexOptionsHooks.CreateSupportReport != null)
                {
                    try { string zip = ReflexOptionsHooks.CreateSupportReport(); s_supportStatus = string.IsNullOrEmpty(zip) ? "No report was made." : "Saved " + ReflexSave.Redact(zip); if (!string.IsNullOrEmpty(zip)) ReflexSave.Reveal(zip); }
                    catch (Exception e) { s_supportStatus = "Could not create the report: " + e.Message; Debug.LogWarning("[Options] create report: " + e); }
                }
                else s_supportStatus = Reflex.Port.Platform.ReflexSupport.CreateReportForPlayer();   // the platform's report (writes, reveals, never throws)
                if (sstatus != null) sstatus.text = s_supportStatus;
            }, sb), "support-create-report");
            Named(OptUi.Button("OPEN LOG FOLDER", "mxa-secondary", () =>
            {
                if (ReflexOptionsHooks.OpenLogFolder != null) ReflexOptionsHooks.OpenLogFolder();
                else Reflex.Port.Platform.ReflexSupport.OpenLogFolder();
            }, sb), "support-open-folder");
            Note(panel, "The report holds the session logs, crash reports and a system summary (your home folder shown as ~, passwords and tokens removed). Nothing is uploaded: you choose where to send it.");
            sstatus = Keep(panel, s_supportStatus);
            try
            {
                Keep(panel, "Log folder: " + ReflexSave.Redact(Reflex.Port.Platform.ReflexSupport.LogDir));
                string build = string.IsNullOrEmpty(Reflex.Port.Platform.ReflexCrashLog.BuildStamp) ? "Editor" : Reflex.Port.Platform.ReflexCrashLog.BuildStamp;
                Keep(panel, "Build: " + build + (string.IsNullOrEmpty(Reflex.Port.Platform.ReflexCrashLog.SessionTag) ? "" : "   Session: " + Reflex.Port.Platform.ReflexCrashLog.SessionTag));
                if (Reflex.Port.Platform.ReflexCrashLog.PreviousCrashed) Keep(panel, "The previous session ended unexpectedly.");
            }
            catch (Exception e) { Debug.LogWarning("[Options] support info: " + e.Message); }
            Section(panel, "ABOUT");
            Note(panel, "Who made " + ShellName() + " and the notice that it is not affiliated with the owners of MX vs ATV Reflex.");
            var about = Named(OptUi.Button("CREDITS", "mxa-start", () => { if (ReflexOptionsHooks.OpenCredits != null && !ReflexClientLive.Runtime) { var open = ReflexOptionsHooks.OpenCredits; _back(); open(); } }, panel), "about-credits");
            if (ReflexOptionsHooks.OpenCredits == null || ReflexClientLive.Runtime) about.SetEnabled(false);
            return first ?? backup;
        }

        static string ShellName() => Reflex.Port.OpenFrontEnd.OpenReflex.Name;

        void OpenLibrary()
        {
            if (ReflexOptionsHooks.OpenGameLibrary == null || ReflexClientLive.Runtime) return;
            var open = ReflexOptionsHooks.OpenGameLibrary;
            _back();
            open();
        }
    }
}

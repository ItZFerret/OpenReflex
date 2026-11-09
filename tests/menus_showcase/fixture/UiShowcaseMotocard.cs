// TEST ONLY: the MOTOCARD pages (OpenFrontEnd/OpenShell.Motocard.cs) in the showcase.
//   motocard-empty   the four tabs on a profile with no progress at all (the starting profile of the scratch save)
//   motocard-data    the four tabs on a profile with earned progress: credits, career medals (the progress figure), unlocked and half-won
//                    highlights, personal records of several locations / vehicle types (the rows are SYNTHETIC: the shipped fixture save
//                    testdata/moto_career_flow/core-player-career-v2.bin is an unlocked clone with no earned progress; the rows are written
//                    through CoreGarageProfile.Change, the profile path the product uses), plus the hub's MOTOCARD card and the garage's RIDER tab
//   nav-motocard     the controller journey: hub -> MOTOCARD (d-pad, A) -> every tab with LB / RB -> B back to the card; garage RIDER header -> A -> B
// Run: run.py <Showcase.app> --seed-save ... --pages motocard-empty,motocard-data [--walk]   (--seed-save copies the fixture save into the scratch save)
using System;
using System.Collections;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using Reflex.Port.Content;
using Reflex.Port.FrontEnd;
using Reflex.Port.OpenFrontEnd;
using UnityEngine;
using UnityEngine.InputSystem;
using UnityEngine.InputSystem.LowLevel;
using UnityEngine.UIElements;

namespace Reflex.Port.Showcase
{
    static class UiShowcaseMotocard
    {
        const string Tag = "[ui-showcase] [motocard] ";
        static readonly string[] TabNames = { "stats", "achievements", "records", "tricks" };

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        static void Register()
        {
            UiShowcase.Register("motocard-empty", () => Tabs("empty", false));
            UiShowcase.Register("motocard-data", () => Tabs("data", true));
            UiShowcase.Register("nav-motocard", Journey);
            UiShowcase.Register("motocard-portrait", PortraitRaw);
        }

        static void Open()
        {
            ReflexProfile.Disabled = false;
            if (!CoreGarageProfile.Open()) throw new InvalidOperationException(CoreGarageProfile.Error);
            OpenMenuWorld.Still = true; FeGarage.RiderPlays = false;
            ReflexProfile.Open();
        }

        /// The hub up with the 3D menu world built, the rider standing and the portrait drawn (as the product's first hub is).
        static IEnumerator Ready()
        {
            var shell = OpenShell.Instance;
            shell.ShowMain();
            float t0 = Time.realtimeSinceStartup;
            while (OpenMenuWorld.Instance == null && Time.realtimeSinceStartup - t0 < 10f) yield return null;
            var w = OpenMenuWorld.Instance;
            while (w != null && w.Status == OpenMenuWorld.State.Loading && Time.realtimeSinceStartup - t0 < 120f) yield return null;
            for (float t1 = Time.realtimeSinceStartup; w != null && (w.Hero == null || w.Hero.RiderModel == null) && Time.realtimeSinceStartup - t1 < 30f;) yield return null;
            for (float t1 = Time.realtimeSinceStartup; !shell.Portrait.Ready && Time.realtimeSinceStartup - t1 < 20f;) yield return null;
            w?.SetWalkTime(99f);
            yield return new WaitForSecondsRealtime(1.5f);
            Debug.Log(Tag + "ready: world " + (w == null ? "none" : w.Status.ToString()) + ", portrait " + shell.Portrait.Ready);
        }

        /// The portrait's render texture as it is drawn (480x600), for the review.
        static void SavePortrait(OpenRiderPortrait p, string name)
        {
            if (p?.Texture == null) { Debug.Log(Tag + "no portrait texture"); return; }
            var prev = RenderTexture.active; RenderTexture.active = p.Texture;
            var t = new Texture2D(p.Texture.width, p.Texture.height, TextureFormat.RGBA32, false); t.ReadPixels(new Rect(0, 0, t.width, t.height), 0, 0); t.Apply();
            RenderTexture.active = prev;
            string dir = Environment.GetEnvironmentVariable("REFLEX_PREVIEW_SHOTS");
            if (!string.IsNullOrEmpty(dir)) System.IO.File.WriteAllBytes(System.IO.Path.Combine(dir, name + ".png"), t.EncodeToPNG());
            UnityEngine.Object.Destroy(t);
        }

        // ------------------------------------------------------------------------------------------------ the synthetic progress

        static readonly HashSet<string> StringColumns = new HashSet<string> { "SeriesOwner", "Name", "Number" };

        static void AddRow(ProfileData p, string table, params (string col, double v)[] values)
        {
            if (!p.tables.TryGetValue(table, out var t)) throw new InvalidOperationException("the profile has no " + table);
            var d = values.ToDictionary(x => x.col, x => x.v);
            foreach (var c in d.Keys) if (!t.columns.ContainsKey(c)) t.columns[c] = new List<object>(Enumerable.Repeat<object>(null, t.rows));
            foreach (var kv in t.columns.ToList())
            {
                var l = kv.Value; while (l.Count < t.rows) l.Add(null);
                l.Add(StringColumns.Contains(kv.Key) ? "" : (object)(d.TryGetValue(kv.Key, out var v) ? v : 0.0));
            }
            t.rows++;
        }

        static void Change(Action<ProfileData> edit)
        {
            var change = typeof(CoreGarageProfile).GetMethod("Change", BindingFlags.Static | BindingFlags.NonPublic);
            var ok = (bool)change.Invoke(null, new object[] { edit });
            if (!ok) throw new InvalidOperationException("seeding the profile: " + CoreGarageProfile.Error);
            ReflexProfile.Open();
        }

        static void Seed()
        {
            if (ReflexProfile.Current != null && ReflexProfile.Current.T("PlayerHighlight").rows > 0) return;   // (seeded already by an earlier page of this run)
            var tables = ReflexEvents.Tables;
            var seriesIds = tables["Series"].Where(r => FeTables.F(r, "CareerCompletionFactor") > 0f).Select(r => FeTables.I(r, "SeriesId")).OrderBy(x => x).ToList();
            Debug.Log(Tag + "seeding: " + seriesIds.Count + " series with a completion factor");
            Change(p =>
            {
                // credits and the lifetime earnings
                var rider = p.T("PlayerRider");
                rider.columns["AccountBalance"][0] = 250000.0;
                if (!rider.columns.ContainsKey("TotalEarnings")) rider.columns["TotalEarnings"] = new List<object> { 0.0 };
                rider.columns["TotalEarnings"][0] = 1840500.0;
                // career medals: 12 gold, 8 silver, 5 bronze, 5 series entered without a medal (BestPlaceFinish 1000 = none)
                for (int i = 0; i < 30 && i < seriesIds.Count; i++)
                {
                    double place = i < 12 ? 0 : i < 20 ? 1 : i < 25 ? 2 : 1000;
                    AddRow(p, "PlayerSeriesStatus", ("SeriesId", seriesIds[i]), ("BestPlaceFinish", place), ("SeriesStatus", 2), ("CurrentEventNumber", 0));
                }
                // highlights: 17 unlocked (in a shuffled order), five with progress
                int[] unlocked = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 31, 33, 37, 45, 46 };
                int[] order = { 3, 1, 2, 9, 4, 5, 6, 10, 7, 8, 11, 12, 13, 14, 15, 16, 17 };
                var goals = tables["Highlight"].ToDictionary(r => FeTables.I(r, "HighlightId"), r => FeTables.I(r, "Goal"));
                for (int i = 0; i < unlocked.Length; i++) AddRow(p, "PlayerHighlight", ("HighlightId", unlocked[i]), ("GoalProgress", goals[unlocked[i]]), ("UnlockedOrder", order[i]));
                foreach (var (id, progress) in new[] { (20, 2), (21, 17), (23, 5), (38, 6), (44, 41) })
                    AddRow(p, "PlayerHighlight", ("HighlightId", id), ("GoalProgress", progress), ("UnlockedOrder", -1));
                // personal records: the first three locations of each leaderboard type, several vehicle types on the first
                int row = 0;
                foreach (int typeId in new[] { 14, 10, 17, 7, 8, 16 })
                {
                    var locs = tables["EventTypeLocations"].Where(r => FeTables.I(r, "EventTypeId") == typeId).Take(4).ToList();
                    for (int li = 0; li < locs.Count; li++)
                    {
                        int etl = FeTables.I(locs[li], "EventTypeLocationId");
                        int[] vts = li == 0 ? new[] { 3, 4, 2 } : li == 1 ? new[] { 3 } : li == 2 ? new[] { 2, 4 } : new int[0];
                        foreach (int vt in vts)
                        {
                            row++;
                            double best = 74.0 + row * 1.37 + (vt == 4 ? 6.5 : 0);
                            AddRow(p, "PlayerRecords", ("EventTypeLocationId", etl), ("VehicleTypeId", vt), ("TotalEvents", 2 + row % 5), ("TotalLaps", 6 + row), ("TotalTime", 240.0 + row * 31.5),
                                   ("BestLapTime", typeId == 14 || typeId == 7 ? 0.0 : best), ("BestPlace", 1 + row % 4), ("BestScore", typeId == 7 ? 41250 + row * 3300 : 0), ("TotalAttemptedJumps", 40 + row * 3),
                                   ("TotalFailedJumps", row), ("TotalCombosCompleted", row % 4), ("TotalHoleshotsWon", row % 3), ("TotalScore", 0), ("TotalPlacing", 3 + row));
                        }
                    }
                }
            });
            // the rider's jersey name and number as the motocard's pages write them
            Debug.Log(Tag + "SetRiderText Number " + CoreGarageProfile.SetRiderText("Number", "21") + " Name " + CoreGarageProfile.SetRiderText("Name", "ferret") + " " + CoreGarageProfile.Error);
            ReflexProfile.Open();
            var pr = ReflexProfile.Current;
            Debug.Log(Tag + $"profile: {pr.T("PlayerHighlight").rows} highlight rows, {pr.T("PlayerRecords").rows} record rows, {pr.T("PlayerSeriesStatus").rows} series rows, completion {OpenMotocardData.CareerPercent(pr)} % (expected 22)");
        }

        // ------------------------------------------------------------------------------------------------ the pages

        static IEnumerator Tabs(string kind, bool seed)
        {
            Open();
            if (seed) Seed();
            var shell = OpenShell.Instance;
            yield return Ready();
            for (int tab = 0; tab < 4; tab++)
            {
                shell.ShowMotocard(tab);
                yield return UiShowcase.Frames(60);   // (the lists settle)
                shell.Nav.Focus(shell.Nav.DefaultItem?.Invoke());   // (the page's own first focus again: the machine's real mouse may sit over the window and hover another item)
                Debug.Log(Tag + $"{kind} {TabNames[tab]}: portrait ready {shell.Portrait.Ready}, items {shell.Nav.Items().Count}, focus {shell.Nav.Focused?.name}");
                yield return UiShowcase.Shot("motocard-" + TabNames[tab] + "-" + kind);
            }
            if (seed)
            {
                // a second look at RECORDS on another activity type, and at an achievement with progress
                shell.ShowMotocard(OpenShell.MotocardRecords); yield return UiShowcase.Frames(40);
                var type = shell.Layer.Q<VisualElement>("motocard-type");
                if (type != null) { shell.Nav.Focus(type); yield return UiShowcase.Frames(5); for (int i = 0; i < 3; i++) { (type.userData as OuNavItem)?.adjust?.Invoke(1); yield return UiShowcase.Frames(6); } }
                yield return UiShowcase.Frames(30);
                var loc = shell.Layer.Q<VisualElement>("motocard-loc-0"); if (loc != null) shell.Nav.Focus(loc);
                yield return UiShowcase.Shot("motocard-records-" + kind + "-freestyle");
                shell.ShowMotocard(OpenShell.MotocardAchievements); yield return UiShowcase.Frames(40);
                var hl = shell.Layer.Q<VisualElement>("motocard-hl-21"); if (hl != null) shell.Nav.Focus(hl);
                yield return UiShowcase.Shot("motocard-achievements-" + kind + "-progress");
                shell.ShowMotocard(OpenShell.MotocardAchievements); yield return UiShowcase.Frames(40);
                var hl1 = shell.Layer.Q<VisualElement>("motocard-hl-1"); if (hl1 != null) shell.Nav.Focus(hl1);
                yield return UiShowcase.Shot("motocard-achievements-" + kind + "-unlocked");
                // the entrances: the hub card and the garage's RIDER header
                shell.ShowMain(); yield return UiShowcase.Frames(40);
                OpenMenuWorld.Instance?.SetWalkTime(99f);
                yield return new WaitForSecondsRealtime(1.5f);
                var card = shell.Layer.Q<VisualElement>("ou-garage-card"); if (card != null) shell.Nav.Focus(card);
                yield return UiShowcase.Shot("motocard-hub-card");
                shell.ShowGarage(OpenShell.GarageHome); yield return UiShowcase.Frames(60);
                OpenMenuWorld.Instance?.SetWalkTime(99f);
                yield return new WaitForSecondsRealtime(2f);
                var head = shell.Layer.Q<VisualElement>("garage-motocard"); if (head != null) shell.Nav.Focus(head);
                yield return UiShowcase.Shot("motocard-garage-header");
            }
            shell.ShowMain(); yield return UiShowcase.Frames(5);
        }

        /// The portrait as it is drawn (480x600, saved raw): it must stand upright (OpenRiderPortrait turns a mirrored render over; the count is logged).
        static IEnumerator PortraitRaw()
        {
            Open();
            var shell = OpenShell.Instance;
            yield return Ready();
            Debug.Log(Tag + "portrait turned over " + shell.Portrait.TurnedOver + " time(s)");
            SavePortrait(shell.Portrait, "motocard-portrait-final");
        }

        // ------------------------------------------------------------------------------------------------ the controller journey

        static IEnumerator Journey()
        {
            Open(); Seed();
            var shell = OpenShell.Instance;
            yield return Ready();
            // the hub: the MOTOCARD card by the d-pad
            for (int i = 0; i < 10 && shell.Nav.Focused?.name != "ou-garage-card"; i++)
            {
                yield return UiShowcaseNav.PadTap(i < 2 ? GamepadButton.DpadRight : GamepadButton.DpadDown); yield return UiShowcase.Frames(4);
            }
            UiShowcaseNav.Check(shell.Nav.Focused?.name == "ou-garage-card", "motocard", "the hub's MOTOCARD card could not be reached with the d-pad (on " + shell.Nav.Focused?.name + ")");
            yield return UiShowcaseNav.PadTap(GamepadButton.South); yield return UiShowcase.Frames(80);
            UiShowcaseNav.Check(shell.Current == OpenShell.Screen.Motocard, "motocard", "A on the MOTOCARD card did not open the pages (" + shell.Current + ")");
            for (int tab = 0; tab < 4; tab++)
            {
                yield return UiShowcaseNav.Settle("motocard tab " + tab, "RB");
                UiShowcaseNav.Check(shell.MotocardCurrentTab == tab, "motocard", "RB did not reach tab " + tab + " (on " + shell.MotocardCurrentTab + ")");
                yield return UiShowcaseNav.Walk("motocard-tab-" + tab, false);
                if (tab == 0)
                {   // A on a recent achievement opens it on the ACHIEVEMENTS tab
                    var recent = shell.Layer.Q<VisualElement>("motocard-recent-0"); if (recent != null) shell.Nav.Focus(recent);
                    yield return UiShowcase.Frames(8);
                    yield return UiShowcaseNav.PadTap(GamepadButton.South); yield return UiShowcase.Frames(30);
                    UiShowcaseNav.Check(shell.MotocardCurrentTab == OpenShell.MotocardAchievements, "motocard", "A on a recent achievement did not open ACHIEVEMENTS (" + shell.MotocardCurrentTab + ")");
                    var focused = shell.Nav.Focused; UiShowcaseNav.Check(focused != null && focused.name != null && focused.name.StartsWith("motocard-hl-"), "motocard", "the achievement did not take the focus (" + focused?.name + ")");
                    yield return UiShowcaseNav.PadTap(GamepadButton.LeftShoulder); yield return UiShowcase.Frames(30);   // back to STATS
                    UiShowcaseNav.Check(shell.MotocardCurrentTab == OpenShell.MotocardStats, "motocard", "LB did not return to STATS");
                }
                if (tab == 2)
                {   // RECORDS: right on the activity row changes the type, down walks the locations and the board follows
                    var type = shell.Layer.Q<VisualElement>("motocard-type"); if (type != null) shell.Nav.Focus(type);
                    yield return UiShowcase.Frames(6);
                    string before = type?.Q<Label>(className: "mxa-value")?.text;
                    yield return UiShowcaseNav.PadTap(GamepadButton.DpadRight); yield return UiShowcase.Frames(8);
                    string after = type?.Q<Label>(className: "mxa-value")?.text;
                    Debug.Log(Tag + "RECORDS activity " + before + " -> " + after);
                    UiShowcaseNav.Check(before != after, "motocard", "right on the activity row did not change the type (" + before + ")");
                    yield return UiShowcaseNav.PadTap(GamepadButton.DpadDown); yield return UiShowcase.Frames(8);
                    UiShowcaseNav.Check(shell.Nav.Focused?.name == "motocard-loc-0", "motocard", "down from the activity row did not reach the first location (" + shell.Nav.Focused?.name + ")");
                }
                yield return UiShowcaseNav.PadTap(GamepadButton.RightShoulder); yield return UiShowcase.Frames(30);
            }
            // B from any tab goes back to the hub with the card focused
            yield return UiShowcaseNav.PadTap(GamepadButton.East); yield return UiShowcase.Frames(40);
            UiShowcaseNav.Check(shell.Current == OpenShell.Screen.Main, "motocard", "B did not return to the hub (" + shell.Current + ")");
            UiShowcaseNav.Check(shell.Nav.Focused?.name == "ou-garage-card", "motocard", "the hub did not put the focus on the MOTOCARD card (" + shell.Nav.Focused?.name + ")");
            // the garage's home: its MOTOCARD card opens the pages, B comes back to it
            shell.ShowGarage(OpenShell.GarageHome); yield return UiShowcase.Frames(80);
            var head = shell.Layer.Q<VisualElement>("garage-motocard"); UiShowcaseNav.Check(head != null, "motocard", "the garage's home has no MOTOCARD card");
            if (head != null) { shell.Nav.Focus(head); yield return UiShowcase.Frames(8); }
            UiShowcaseNav.Check(shell.Nav.Focused?.name == "garage-motocard", "motocard", "the motocard header cannot take the focus (" + shell.Nav.Focused?.name + ")");
            yield return UiShowcaseNav.PadTap(GamepadButton.South); yield return UiShowcase.Frames(60);
            UiShowcaseNav.Check(shell.Current == OpenShell.Screen.Motocard, "motocard", "A on the garage header did not open the pages (" + shell.Current + ")");
            yield return UiShowcaseNav.PadTap(GamepadButton.East); yield return UiShowcase.Frames(80);
            UiShowcaseNav.Check(shell.Current == OpenShell.Screen.Garage && shell.GarageCurrentTab == OpenShell.GarageHome, "motocard", "B did not return to the garage's home (" + shell.Current + ")");
            UiShowcaseNav.Check(shell.Nav.Focused?.name == "garage-motocard", "motocard", "the garage did not put the focus back on the motocard header (" + shell.Nav.Focused?.name + ")");
            // the mouse: a click on the header
            shell.ShowMain(); yield return UiShowcase.Frames(5);
        }
    }
}

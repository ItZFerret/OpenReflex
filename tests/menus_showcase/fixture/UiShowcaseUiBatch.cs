// TEST ONLY (w-uibatch): pages for the batch of UI fixes, built from the product's own presenters with synthetic data.
//   uib-results            RACE RESULTS with every action offered (RACE AGAIN, HIGHLIGHT REEL, REPLAY, RETURN TO MENU): the side column must end above
//                          the legend band at any screen shape (the log line [uib] says where the last action ends and where the band starts)
//   uib-results-freestyle  the same with Freestyle's judges' card (the tallest side column)
//   uib-results-waypoint   one rider, no place number (the lead row of tiles)
//   uib-nowplaying         the NOW PLAYING pop-up: the race's card (a custom song, the game's track with an album line, SKIPPED TO) and the menus' strip
//   uib-gatearrow          (run.py --seed-save testdata/moto_career_flow/core-player-career-v2.bin) the career's Series 14 (NAT_Stillwater, a moto:
//                          the gate cards / arrow of the level file sit ~1200 ft from its start gate), its rules accepted, the level left to finish
//                          reading its scenery LOD data before CHOOSE START GATE (the order that culled the arrow), then the gate selection: the
//                          arrow must be on and drawn over the focused gate, after d-pad right x2 too
//   uib-gridnav            the event browser's GRID with the pad's d-pad: right / right / left / down / up from the first card must each move exactly one
//                          card; then the front end as it comes back from a race whose pause fallback (OriginalPauseMenu.Build) made a kept
//                          (DontDestroyOnLoad) EventSystem with the UI module's default actions, the front-end scene loaded again, the same walk
using System;
using System.Collections;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using Reflex.Port.Content;
using Reflex.Port.Hud;
using Reflex.Port.OpenFrontEnd;
using UnityEngine;
using UnityEngine.EventSystems;
using UnityEngine.InputSystem;
using UnityEngine.InputSystem.LowLevel;
using UnityEngine.InputSystem.UI;
using UnityEngine.UIElements;

namespace Reflex.Port.Showcase
{
    static class UiShowcaseUiBatch
    {
        const string Tag = "[ui-showcase] [uib] ";
        const BindingFlags All = BindingFlags.Instance | BindingFlags.Static | BindingFlags.NonPublic | BindingFlags.Public;

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        static void Register()
        {
            UiShowcase.Register("uib-results", () => Results("uib-results", HudMode.Race, false));
            UiShowcase.Register("uib-results-freestyle", () => Results("uib-results-freestyle", HudMode.Freestyle, true));
            UiShowcase.Register("uib-results-waypoint", () => Results("uib-results-waypoint", HudMode.Waypoint, true));
            UiShowcase.Register("uib-nowplaying", NowPlaying);
            UiShowcase.Register("uib-gridnav", GridNav);
            UiShowcase.Register("uib-gatearrow", GateArrow);
        }

        static void Set(object o, string field, object value) { var f = o.GetType().GetField(field, All); if (f == null) throw new MissingFieldException(o.GetType().Name, field); f.SetValue(o, value); }

        static OwnRaceResultsSnapshot Snapshot(HudMode mode, bool modes)
        {
            var t = modes ? typeof(UiShowcasePauseModes) : typeof(UiShowcasePages);
            var m = modes ? t.GetMethod("Snapshot", All, null, new[] { typeof(HudMode) }, null) : t.GetMethod("Snapshot", All, null, Type.EmptyTypes, null);
            return (OwnRaceResultsSnapshot)m.Invoke(null, modes ? new object[] { mode } : null);
        }

        static IEnumerator Results(string shot, HudMode mode, bool modes)
        {
            UiShowcase.ShellVisible(false);
            var go = new GameObject("showcase results " + shot);
            var page = go.AddComponent<OwnRaceResults>(); page.enabled = false;   // (no world: its Update would close it)
            Set(page, "snapshot", Snapshot(mode, modes)); Set(page, "active", true); Set(page, "armed", true);
            Set(page, "reelForShowcase", true); Set(page, "replayForShowcase", true);
            UiShowcasePages.Call(page, "Build"); UiShowcasePages.Call(page, "ShowResults", new object[] { null });
            UiShowcasePages.DriveOf(go, page);
            yield return UiShowcase.Frames(60);
            Report(shot, page.Root);
            yield return UiShowcase.Shot(shot);
            Set(page, "active", false);
            UnityEngine.Object.Destroy(go);
            yield return UiShowcase.Frames(3);
            UiShowcase.ShellVisible(true);
        }

        /// Where the side column's last action ends against the top of the legend band (panel px); FAIL when it reaches into the band.
        static void Report(string shot, VisualElement root)
        {
            if (root == null) { UiShowcaseNav.Check(false, shot, "no page root"); return; }
            var menu = root.Q<VisualElement>("results-menu");
            var band = root.Q<VisualElement>(className: "ou-legend-band");
            var side = root.Q<VisualElement>(className: "ou-page-side");
            float panelH = root.worldBound.height;
            float bandTop = band != null && band.resolvedStyle.display != DisplayStyle.None ? band.worldBound.yMin : panelH;
            float menuBottom = menu != null ? menu.worldBound.yMax : -1;
            Debug.Log(Tag + FormattableString.Invariant($"{shot}: panel {root.worldBound.width:0}x{panelH:0}, side {side?.worldBound.yMin:0}..{side?.worldBound.yMax:0}, RETURN TO MENU ends {menuBottom:0}, legend band from {bandTop:0}, side classes '{string.Join(" ", side?.GetClasses() ?? new string[0])}'"));
            UiShowcaseNav.Check(menu != null && menuBottom <= bandTop + 0.5f, shot, FormattableString.Invariant($"RETURN TO MENU ends at {menuBottom:0}, over the legend band from {bandTop:0}"));
        }

        static IEnumerator NowPlaying()
        {
            var mine = new ReflexSong { title = "Silencer", artist = "Steeler", source = ReflexSong.SourceCustom };
            var game = new ReflexSong { title = "A Very Long Song Name That Does Not Fit On The Card At All, Part Two", artist = "Some Band With A Long Name", album = "Reflex Soundtrack", year = 2009, source = ReflexSong.SourceGame };
            UiShowcase.ShellVisible(false);   // (a race has no shell: the card is drawn over the world)
            ReflexNowPlaying.ShowNow(mine, false, true);
            yield return new WaitForSecondsRealtime(0.9f);
            yield return UiShowcase.Shot("uib-nowplaying-card");
            ReflexNowPlaying.ShowNow(game, false, true);
            yield return new WaitForSecondsRealtime(0.9f);
            yield return UiShowcase.Shot("uib-nowplaying-card-game");
            ReflexNowPlaying.ShowNow(mine, true, true);
            yield return new WaitForSecondsRealtime(0.9f);
            yield return UiShowcase.Shot("uib-nowplaying-card-skipped");
            UiShowcase.ShellVisible(true);
            ReflexNowPlaying.ShowNow(game, false, false);
            yield return new WaitForSecondsRealtime(0.9f);
            yield return UiShowcase.Shot("uib-nowplaying-strip");
            yield return new WaitForSecondsRealtime(7f);   // (gone again before the next page)
        }

        // ------------------------------------------------------------------------------------------------ the grid's left / right after a race

        static List<VisualElement> Cards(OpenShell shell)
        {
            var list = (System.Collections.IList)typeof(OpenShell).GetField("_cards", All).GetValue(shell);
            var rows = new List<VisualElement>();
            foreach (var c in list) rows.Add((VisualElement)c.GetType().GetField("row", All).GetValue(c));
            return rows;
        }

        static string EsState()
        {
            var all = UnityEngine.Object.FindObjectsByType<EventSystem>(FindObjectsInactive.Include, FindObjectsSortMode.None);
            return all.Length + " event system(s): " + string.Join(", ", all.Select(e => { var m = e.GetComponent<InputSystemUIInputModule>(); return e.name + (m == null ? " (no module)" : " move " + (m.move != null && m.move.action != null ? "ON" : "off") + " submit " + (m.submit != null && m.submit.action != null ? "ON" : "off")) + " nav events " + e.sendNavigationEvents; }));
        }

        static IEnumerator Walk(string when)
        {
            var shell = OpenShell.Instance;
            OpenHistory.View = 0;
            shell.ShowMain(); yield return UiShowcase.Frames(20);
            shell.ShowBrowser(false); yield return UiShowcase.Frames(80);
            var cards = Cards(shell);
            Debug.Log(Tag + when + ": " + cards.Count + " cards; " + EsState());
            if (cards.Count < 6) { UiShowcaseNav.Check(false, "uib-gridnav", when + ": fewer than 6 cards"); yield break; }
            int perRow = 1; while (perRow < cards.Count && Mathf.Abs(cards[perRow].worldBound.center.y - cards[0].worldBound.center.y) < cards[0].worldBound.height * 0.5f) perRow++;
            shell.Nav.Focus(cards[0]); yield return UiShowcase.Frames(10);
            int At() => cards.IndexOf(shell.Nav.Focused);
            var steps = new (GamepadButton b, int expect, string name)[] { (GamepadButton.DpadRight, 1, "right"), (GamepadButton.DpadRight, 2, "right"), (GamepadButton.DpadLeft, 1, "left"), (GamepadButton.DpadDown, 1 + perRow, "down"), (GamepadButton.DpadUp, 1, "up"), (GamepadButton.DpadLeft, 0, "left") };
            foreach (var st in steps)
            {
                int from = At();
                yield return UiShowcaseNav.PadTap(st.b); yield return UiShowcase.Frames(12);
                int to = At();
                Debug.Log(Tag + FormattableString.Invariant($"{when}: {st.name} from card {from} -> card {to} (expected {st.expect}; {perRow} a row)"));
                UiShowcaseNav.Check(to == st.expect, "uib-gridnav", FormattableString.Invariant($"{when}: d-pad {st.name} from card {from} went to card {to}, not {st.expect}"));
                if (to != st.expect && st.expect >= 0 && st.expect < cards.Count) { shell.Nav.Focus(cards[st.expect]); yield return UiShowcase.Frames(6); }
            }
            yield return UiShowcase.Shot("uib-gridnav-" + when);
        }

        static IEnumerator GridNav()
        {
            UiShowcaseNav.PadState(new UnityEngine.InputSystem.LowLevel.GamepadState());   // (the virtual pad, the machine's own devices off)
            yield return Walk("fresh");
            // what a race leaves behind: OriginalPauseMenu.Build's kept EventSystem (the UI module with its default actions, navigation on)
            foreach (var e in UnityEngine.Object.FindObjectsByType<EventSystem>(FindObjectsSortMode.None)) UnityEngine.Object.Destroy(e.gameObject);
            yield return null;
            var es = new GameObject("EventSystem", typeof(EventSystem), typeof(InputSystemUIInputModule)); UnityEngine.Object.DontDestroyOnLoad(es);
            yield return null;
            Debug.Log(Tag + "a race's kept EventSystem: " + EsState());
            var old = OpenShell.Instance;
            FrontEndSwitch.Reload();   // back to the front end (the race's RETURN TO MENU loads the front-end scene)
            for (float t = 0; t < 20f && (OpenShell.Instance == null || OpenShell.Instance == old); t += Time.unscaledDeltaTime) yield return null;
            yield return UiShowcase.Frames(60);
            yield return Walk("after-race");
        }

        // ------------------------------------------------------------------------------------------------ the gate selection's arrow

        static VisualElement FindItem(string name)
        {
            VisualElement best = null; int order = int.MinValue;
            foreach (var doc in UnityEngine.Object.FindObjectsByType<UIDocument>(FindObjectsInactive.Exclude, FindObjectsSortMode.None))
            {
                var v = doc.rootVisualElement?.Q<VisualElement>(name);
                if (v != null && v.resolvedStyle.display != DisplayStyle.None && v.enabledInHierarchy && doc.sortingOrder >= order) { best = v; order = (int)doc.sortingOrder; }
            }
            return best;
        }
        static bool Press(string name)
        {
            var v = FindItem(name);
            if (v?.userData is OuNavItem nav && nav.submit != null) { Debug.Log(Tag + "submit " + name); nav.submit(); return true; }
            return false;
        }

        static bool ArrowState(string when, OriginalWorld.OwnGateView gv)
        {
            var level = UnityEngine.Object.FindAnyObjectByType<OriginalWorld.OriginalLevel>();
            var cards = level ? level.GateCards : null;
            var arrow = cards == null ? null : typeof(OriginalWorld.OriginalGateCards).GetField("arrow", All).GetValue(cards) as Transform;
            var r = arrow ? arrow.GetComponent<MeshRenderer>() : null;
            var cam = Camera.allCameras.Where(c => c && c.enabled).OrderByDescending(c => c.depth).FirstOrDefault();
            bool inView = false; float dist = -1;
            if (r && cam) { var vp = cam.WorldToViewportPoint(r.bounds.center); inView = vp.z > 0 && vp.x > 0 && vp.x < 1 && vp.y > 0 && vp.y < 1; dist = Vector3.Distance(cam.transform.position, r.bounds.center); }
            bool drawn = arrow && arrow.gameObject.activeInHierarchy && r && r.enabled && !r.forceRenderingOff;
            Debug.Log(Tag + FormattableString.Invariant($"{when}: focus gate {gv?.Focus}, arrow {(arrow ? "found" : "MISSING")} active {(arrow && arrow.gameObject.activeInHierarchy)} renderer {(r ? (r.enabled ? "on" : "off") : "-")} forceRenderingOff {(r && r.forceRenderingOff)} in view {inView} {dist:0.0} ft from the camera; cards {cards?.CardCount}"));
            return drawn && inView;
        }

        static IEnumerator GateArrow()
        {
            ReflexProfile.Disabled = false;
            if (!CoreGarageProfile.Open()) throw new InvalidOperationException(CoreGarageProfile.Error);
            ReflexProfile.Open();
            if (!RxCore.RxNativeLoader.IsLoaded) RxCore.RxNativeLoader.Load(RxCore.RxNativeLoader.ResolvePath(RxCore.RxNativeLoader.PluginDirectory(Application.dataPath, Application.isEditor)));
            var presentation = new OwnCareerPresentationInstallation(OwnCareerPresentationInstallation.InstalledHuds, OpenShell.ReturnToOwnedCareer, OpenShell.ReturnToOwnedCareer);
            CoreGarageProfile.SetUnlockEverything(true); OpenGarageData.SelectVehicle(2);
            var task = RxCore.OpenReflexSimulation.ReadCareerCatalogAsync(ReflexInstall.Require(), CareerEventCatalog.Read(), CoreGarageProfile.CapturePersistence(), presentation.Capabilities, default);
            while (!task.IsCompleted) yield return null;
            if (task.IsFaulted) { UiShowcaseNav.Check(false, "uib-gatearrow", "catalog: " + task.Exception.GetBaseException().Message); yield break; }
            RxCore.CareerEventChoice pick = null;
            foreach (var c in task.Result) if (c.Entry.SeriesId == 14) pick = c;
            UiShowcaseNav.Check(pick != null && pick.CanSelect, "uib-gatearrow", "series 14 cannot be selected");
            if (pick == null || !pick.CanSelect) yield break;
            Debug.Log(Tag + "career series " + pick.Entry.SeriesId + " " + pick.Entry.TrackAsset);
            float t0 = Time.realtimeSinceStartup, gateAt = -1, lastPress = -10, runningAt = -1, nextLog = 0;
            OriginalWorld.OwnCareerLaunch.Launch(pick, presentation, motoSkipPractice: true);
            OriginalWorld.OwnGateView gv = null;
            while (Time.realtimeSinceStartup - t0 < 150f)
            {
                float t = Time.realtimeSinceStartup - t0;
                var rt = UnityEngine.Object.FindAnyObjectByType<OriginalWorld.OriginalWorldRuntime>();
                gv = rt ? typeof(OriginalWorld.OriginalWorldRuntime).GetField("gateView", All)?.GetValue(rt) as OriginalWorld.OwnGateView : null;
                var lvl = UnityEngine.Object.FindAnyObjectByType<OriginalWorld.OriginalLevel>();
                if (lvl && lvl.GateCards != null && runningAt < 0) { runningAt = t; Debug.Log(Tag + "level with its gate cards at " + t.ToString("F2") + " s (running " + (rt && rt.Running) + ")"); }
                if (t >= nextLog) { nextLog = t + 3f; Debug.Log(Tag + FormattableString.Invariant($"t={t:F1} runtime {(bool)rt} running {(rt && rt.Running)} gate view {(gv != null ? (gv.Active ? "active" : "idle") : "-")} rules {(FindItem("career-rules-accept") != null)} gates {(FindItem("career-moto-gates") != null)}")); }
                if (gv != null && gv.Active && gv.Ready && gv.Focus >= 0 && gateAt < 0) { gateAt = t; Debug.Log(Tag + "GATE VIEW at " + t.ToString("F2") + " s"); }
                // the rules first; CHOOSE START GATE once the level has been loaded for 8 s (its scenery LOD data read: the order that culled the arrow)
                if (t - lastPress > 1.5f && gateAt < 0 && (Press("career-rules-accept") || runningAt >= 0 && t - runningAt > 8f && Press("career-moto-gates"))) lastPress = t;
                if (gateAt >= 0 && t - gateAt > 2.5f) break;
                yield return null;
            }
            UiShowcaseNav.Check(gateAt >= 0, "uib-gatearrow", "the gate selection never showed");
            if (gateAt < 0) yield break;
            bool first = ArrowState("gate view", gv);
            yield return UiShowcase.Shot("uib-gatearrow-first");
            UiShowcaseNav.Check(first, "uib-gatearrow", "the arrow is not drawn over the first gate");
            for (int i = 0; i < 2; i++) { yield return UiShowcaseNav.PadTap(UnityEngine.InputSystem.LowLevel.GamepadButton.DpadRight); yield return new WaitForSecondsRealtime(1.2f); }   // (the camera eases over 0.5 s)
            bool moved = ArrowState("after right x2", gv);
            yield return UiShowcase.Shot("uib-gatearrow-right2");
            UiShowcaseNav.Check(moved, "uib-gatearrow", "the arrow is not drawn after the focus moved");
            // a song starts while the gate selection is up (the race's music): where the NOW PLAYING pop-up goes over the page
            ReflexNowPlaying.Notify(new ReflexSong { title = "Silencer", artist = "Steeler", source = ReflexSong.SourceGame }, false);
            yield return new WaitForSecondsRealtime(1.2f);
            Debug.Log(Tag + "NOW PLAYING over the gate selection: visible " + ReflexNowPlaying.Visible + " '" + ReflexNowPlaying.Text + "'");
            yield return UiShowcase.Shot("uib-gatearrow-nowplaying");
        }
    }
}

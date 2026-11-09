// TEST ONLY: the owned in-game / career pages in the showcase, built with synthetic data (no world, no core session): the career pages and
// panels, the race results, the pause page, the challenge popups, the AAA loading page. The presenters' own methods are called (some through
// reflection: their fields belong to a running world), so what is photographed is what the game draws.
using System;
using System.Collections;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using Reflex.Port.Content;
using Reflex.Port.OpenFrontEnd;
using Reflex.Port.OriginalWorld;
using Reflex.Port.RxCore;
using UnityEngine;
using UnityEngine.UIElements;

namespace Reflex.Port.Showcase
{
    static class UiShowcasePages
    {
        const BindingFlags All = BindingFlags.Instance | BindingFlags.NonPublic | BindingFlags.Public;
        static void Set(object o, string field, object value) { var f = o.GetType().GetField(field, All); if (f == null) throw new MissingFieldException(o.GetType().Name, field); f.SetValue(o, value); }
        internal static object Call(object o, string method, params object[] args) { var m = o.GetType().GetMethod(method, All); if (m == null) throw new MissingMethodException(o.GetType().Name, method); return m.Invoke(o, args); }

        /// Runs the page's own input every frame as its Update would with a world: nav.Update(input) (its private fields), or a method.
        sealed class NavDriver : MonoBehaviour
        {
            public object page; public string method;
            void Update()
            {
                if (page == null || (page is UnityEngine.Object o && !o)) return;
                if (method != null) { page.GetType().GetMethod(method, All)?.Invoke(page, null); return; }
                var nav = page.GetType().GetField("nav", All)?.GetValue(page) as OuNav; var input = page.GetType().GetField("input", All)?.GetValue(page) as OuMenuInput;
                if (nav != null && input != null) nav.Update(input);
            }
        }
        static void Drive(GameObject host, object page, string method = null) { var d = host.AddComponent<NavDriver>(); d.page = page; d.method = method; }
        internal static void DriveOf(GameObject host, object page, string method = null) => Drive(host, page, method);

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        static void Register()
        {
            UiShowcase.Register("career-list", CareerList);
            UiShowcase.Register("career-results", CareerResults);
            UiShowcase.Register("career-rewards", CareerRewards);
            UiShowcase.Register("career-hub", CareerHub);
            UiShowcase.Register("career-unlocked", () => CareerReal(true));
            UiShowcase.Register("career-tier", () => CareerSynth("career-tier", 0, -1));
            UiShowcase.Register("career-events", () => CareerSynth("career-events", 0, 1));
            UiShowcase.Register("career-far", () => CareerSynth("career-far", 4, 2));
            UiShowcase.Register("career-heat", () => CareerSynth("career-heat", 9, -1));
            UiShowcase.Register("career-practice", CareerPractice);
            UiShowcase.Register("career-nav", CareerNav);
            UiShowcase.Register("career-probe", CareerProbe);
            UiShowcase.Register("career-gates", CareerGates);
            UiShowcase.Register("career-rules", CareerRules);
            UiShowcase.Register("race-results", RaceResults);
            UiShowcase.Register("pause", Pause);
            UiShowcase.Register("pause-confirm", PauseConfirm);
            UiShowcase.Register("pre-challenge", PreChallenge);
            UiShowcase.Register("post-challenge", () => PostChallenge(3, "post-challenge"));
            UiShowcase.Register("post-challenge-fail", () => PostChallenge(0, "post-challenge-fail"));
            UiShowcase.Register("aaa-loading", AaaLoading);
        }


        // The real catalog with the whole career unlocked (Options > Gameplay's UNLOCK mode of the scratch profile).
        static IEnumerator CareerReal(bool unlocked)
        {
            if (unlocked) { bool ok = Reflex.Port.Content.CoreGarageProfile.SetUnlockEverything(true); Debug.Log("[ui-showcase] unlock everything: " + ok); }
            OpenShell.Instance.ShowCoreCareer();
            for (int i = 0; i < 600 && OpenShell.Instance.Layer.Q<VisualElement>(className: "oc-event") == null; i++) yield return null;
            yield return UiShowcase.Frames(60);
            yield return UiShowcase.Shot("career-unlocked");
            OpenShell.Instance.ShowMain();
            yield return UiShowcase.Frames(5);
        }

        // A synthetic catalog (the install's real series with invented progress: medals, locks) drawn by the hub itself.
        internal static IEnumerator CareerSynth(string shot, int tier, int eventIndex)
        {
            var shell = OpenShell.Instance;
            shell.Clear(OpenShell.Screen.Working, "career");
            Call(shell, "CareerChrome");
            var hub = new OcRaceAndRide(shell.Layer, shell.Nav, shell.ShowMain, c => { });
            var choices = SynthChoices();
            hub.Fill(choices, "CAREER PROFILE · Earn progress by completing events.");
            yield return UiShowcase.Frames(20);
            var focus = hub.DefaultFocus;
            for (int i = 0; i < tier && focus != null; i++) focus = ((OuNavItem)focus.userData).right;
            if (focus != null) shell.Nav.Focus(focus);
            yield return UiShowcase.Frames(20);
            if (eventIndex >= 0)
            {
                var rows = shell.Layer.Query<VisualElement>(className: "oc-event").ToList().FindAll(e => e.worldBound.x >= 0 && e.enabledInHierarchy && e.worldBound.width > 0);
                rows.Sort((a, b) => a.worldBound.y.CompareTo(b.worldBound.y));
                if (rows.Count > 0) shell.Nav.Focus(rows[Mathf.Min(eventIndex, rows.Count - 1)]);
                yield return UiShowcase.Frames(20);
            }
            yield return UiShowcase.Frames(40);
            yield return UiShowcase.Shot(shot);
            shell.ShowMain();
            yield return UiShowcase.Frames(5);
        }

        internal static List<CareerEventChoice> SynthChoicesPublic() => SynthChoices();
        static List<CareerEventChoice> SynthChoices()
        {
            var choices = new List<CareerEventChoice>();
            foreach (var entry in CareerEventCatalog.Read())
            {
                var info = RxCareerSeriesInfo.Create(); info.series_id = (uint)entry.SeriesId; info.authored = 1; info.supported = 1; info.eligible = 1;
                info.product_id = entry.ProductId; info.vehicle_class = entry.VehicleClass; info.event_type_id = entry.EventTypeId;
                info.locked = (uint)(entry.SeriesId >= 56 ? 1 : 0);
                var presentation = RxCareerSeriesPresentation.Create(); presentation.series_id = (uint)entry.SeriesId; presentation.authored = 1; presentation.admitted = 1;
                int medal = entry.SeriesId < 3 ? -1 : entry.SeriesId < 12 ? entry.SeriesId % 3 : entry.SeriesId < 24 && entry.SeriesId % 2 == 0 ? 0 : -1;
                choices.Add((CareerEventChoice)Activator.CreateInstance(typeof(CareerEventChoice), All, null, new object[] { entry, info, presentation, medal }, null));
            }
            return choices;
        }

        // Scripted controller input through the shell's own menu input: right / down / back / LB / RB.
        static IEnumerator CareerNav()
        {
            var shell = OpenShell.Instance;
            shell.Clear(OpenShell.Screen.Working, "career");
            Call(shell, "CareerChrome");
            var hub = new OcRaceAndRide(shell.Layer, shell.Nav, shell.ShowMain, c => Debug.Log("[ui-showcase] SELECTED series " + c.Entry.SeriesId));
            hub.Fill(SynthChoices(), "nav test");
            yield return UiShowcase.Frames(30);
            shell.Nav.Focus(hub.DefaultFocus);
            yield return UiShowcase.Frames(5);
            string Where() => shell.Nav.Focused?.name ?? "(none)";
            IEnumerator Do(string what, Action act) { act(); yield return UiShowcase.Frames(8); Debug.Log("[ui-showcase] nav " + what + " -> " + Where()); }
            yield return Do("start", () => { });
            yield return Do("right", () => shell.Input.injectStep = new Vector2Int(1, 0));
            yield return Do("right", () => shell.Input.injectStep = new Vector2Int(1, 0));
            yield return Do("left", () => shell.Input.injectStep = new Vector2Int(-1, 0));
            yield return Do("down (into the events)", () => shell.Input.injectStep = new Vector2Int(0, -1));
            yield return Do("down", () => shell.Input.injectStep = new Vector2Int(0, -1));
            yield return Do("up", () => shell.Input.injectStep = new Vector2Int(0, 1));
            yield return Do("RB (next tier, stays in events)", () => shell.Input.injectTab = 1);
            yield return Do("LB (previous tier)", () => shell.Input.injectTab = -1);
            yield return Do("left (back to the poster)", () => shell.Input.injectStep = new Vector2Int(-1, 0));
            yield return Do("down", () => shell.Input.injectStep = new Vector2Int(0, -1));
            yield return Do("cancel (back to the poster)", () => shell.Input.injectCancel = true);
            yield return Do("right at the end x12", () => { });
            for (int i = 0; i < 12; i++) { shell.Input.injectStep = new Vector2Int(1, 0); yield return UiShowcase.Frames(3); }
            Debug.Log("[ui-showcase] nav at the end -> " + Where());
            yield return Do("down", () => shell.Input.injectStep = new Vector2Int(0, -1));
            yield return Do("submit", () => shell.Input.injectSubmit = true);
            yield return UiShowcase.Frames(30);
            yield return UiShowcase.Shot("career-nav");
            shell.ShowMain();
            yield return UiShowcase.Frames(5);
        }


        // The moto_career_flow probes' way into the career page, replayed on the real unlocked catalog without riding: find
        // "career-series-14" (a Moto series far from the first poster), make it visible the way their Drawn() does (Focus + ScrollTo),
        // click it like Submit() and check the practice-choice page the probe expects (skip button outside any ScrollView, no series list).
        static bool ProbeAvailable(VisualElement value)
        {
            if (value == null || value.panel == null || value.worldBound.width <= 0 || value.worldBound.height <= 0) return false;
            for (var p = value; p != null; p = p.parent)
                if (!p.visible || p.resolvedStyle.display == DisplayStyle.None || p.resolvedStyle.visibility == Visibility.Hidden || p.resolvedStyle.opacity <= 0) return false;
            return true;
        }
        static bool ProbeVisible(VisualElement value)
        {
            if (!ProbeAvailable(value)) return false;
            var bounds = value.worldBound; var clip = value.panel.visualTree.worldBound;
            for (var p = value.parent; p != null; p = p.parent)
            {
                if (!(p is ScrollView scroll)) continue;
                var limit = scroll.contentViewport.worldBound;
                clip = Rect.MinMaxRect(Mathf.Max(clip.xMin, limit.xMin), Mathf.Max(clip.yMin, limit.yMin), Mathf.Min(clip.xMax, limit.xMax), Mathf.Min(clip.yMax, limit.yMax));
            }
            return bounds.xMin >= clip.xMin - 1 && bounds.xMax <= clip.xMax + 1 && bounds.yMin >= clip.yMin - 1 && bounds.yMax <= clip.yMax + 1;
        }
        static IEnumerator CareerProbe()
        {
            Reflex.Port.Content.CoreGarageProfile.SetUnlockEverything(true);
            var shell = OpenShell.Instance;
            shell.ShowOwnedCareer(new OwnCareerPresentationInstallation(RxCareerPresenter.NationalHud | (RxCareerPresenter)256, () => { }, () => { }));   // as MotoCareerFlowProbe opens it
            VisualElement Find(string name) { var v = shell.Root.Q<VisualElement>(name); return ProbeAvailable(v) ? v : null; }
            for (int i = 0; i < 900 && Find("career-series-0") == null; i++) yield return null;
            yield return UiShowcase.Frames(20);
            var entries = CareerEventCatalog.Read();
            // the enabled row of the highest series id (far from the first poster), then an enabled Moto / Heat row for the click
            VisualElement button = null; int target = -1;
            foreach (var e in entries.OrderByDescending(x => x.SeriesId)) { var v = Find("career-series-" + e.SeriesId); if (v != null && v.enabledInHierarchy) { button = v; target = e.SeriesId; break; } }
            Debug.Log("[ui-showcase] probe far target series " + target + " (enabled rows: " + shell.Root.Query<VisualElement>(className: "oc-event").ToList().Count(r => r.enabledInHierarchy) + " of " + entries.Count + ")");
            int frames = 0;
            while (button != null && !ProbeVisible(button) && frames < 120)
            {
                if (button.userData is OuNavItem) { button.Focus(); for (var p = button.parent; p != null; p = p.parent) if (p is ScrollView sv) { sv.ScrollTo(button); break; } }
                frames++; yield return null;
            }
            yield return UiShowcase.Frames(30);
            Debug.Log("[ui-showcase] probe far series visible after " + frames + " frames: " + ProbeVisible(button));
            yield return UiShowcase.Shot("career-probe");
            var pick = entries.FirstOrDefault(e => (e.IsMoto || e.IsHeat) && Find("career-series-" + e.SeriesId) is VisualElement r && r.enabledInHierarchy);
            if (pick == null) { Debug.Log("[ui-showcase] probe: no enabled Moto / Heat row in this profile"); }
            else
            {
                var row = Find("career-series-" + pick.SeriesId);
                for (int i = 0; i < 120 && !ProbeVisible(row); i++) { row.Focus(); yield return null; }
                var trigger = new UnityEngine.Event { type = EventType.MouseUp, button = 0, clickCount = 1, mousePosition = row.worldBound.center };
                using (var click = PointerEventBase<ClickEvent>.GetPooled(trigger)) { click.target = row; row.SendEvent(click); }
                yield return UiShowcase.Frames(30);
                string kind = pick.IsMoto ? "moto" : "heat";
                var skip = Find(pick.IsMoto ? "career-moto-skip-practice" : "career-heat-skip-practice"); var choice = Find("career-" + kind + "-practice-choice");
                bool ok = skip != null && choice != null && choice.Contains(skip) && skip.GetFirstAncestorOfType<ScrollView>() == null && Find("career-series-" + pick.SeriesId) == null && Find("career-" + kind + "-practice-back") != null;
                Debug.Log("[ui-showcase] probe " + kind + " series " + pick.SeriesId + " practice-choice page ok=" + ok);
                yield return UiShowcase.Shot("career-probe-choice");
            }
            shell.ShowMain();
            yield return UiShowcase.Frames(5);
        }

        static IEnumerator CareerPractice()
        {
            var shell = OpenShell.Instance;
            var entry = CareerEventCatalog.Read().First(e => e.IsHeat);
            var info = RxCareerSeriesInfo.Create(); info.series_id = (uint)entry.SeriesId; info.authored = 1; info.supported = 1; info.eligible = 1;
            var choice = (CareerEventChoice)Activator.CreateInstance(typeof(CareerEventChoice), All, null, new object[] { entry, info, RxCareerSeriesPresentation.Create(), -1 }, null);
            Call(shell, "ShowHeatPracticeChoice", choice, new OwnCareerPresentationInstallation(OwnCareerPresentationInstallation.InstalledHuds, () => { }, () => { }));
            yield return UiShowcase.Frames(50);
            yield return UiShowcase.Shot("career-practice");
            shell.ShowMain();
            yield return UiShowcase.Frames(5);
        }

        static IEnumerator CareerGates()
        {
            UiShowcase.ShellVisible(false);
            var host = new GameObject("showcase career gates");
            var view = new OwnCareerPageView(host.transform, () => true);
            view.Show("MOTOCAREER", "MOTO ONE");
            view.Hero("Pro Nationals", "Southwick");
            view.Heading("CHOOSE YOUR START GATE");
            for (int g = 0; g < 12; g++) view.Button("GATE " + (g + 1), "career-moto-gate-" + g, () => true);
            view.Button("LEAVE EVENT", "career-leave", () => true);
            yield return UiShowcase.Frames(60);
            yield return UiShowcase.Shot("career-gates");
            view.Dispose(); UnityEngine.Object.Destroy(host);
            yield return UiShowcase.Frames(3);
        }

        static IEnumerator CareerRules()
        {
            UiShowcase.ShellVisible(false);
            var host = new GameObject("showcase career rules");
            var view = new OwnCareerPageView(host.transform, () => true);
            view.Show("ELIMINATION", "MOTOCAREER · RULES");
            view.Text("The rider in last place at each checkpoint is out of the race. Stay clear of the back of the pack and ride until one rider is left.");
            view.Button("ACCEPT", "career-rules-accept", () => true);
            view.Button("DON'T SHOW AGAIN", "career-rules-skip", () => true);
            view.Text("Leaving closes the current event. Any completed player checkpoint stays saved.");
            view.Button("LEAVE EVENT", "career-leave", () => true);
            yield return UiShowcase.Frames(60);
            yield return UiShowcase.Shot("career-rules");
            view.Dispose(); UnityEngine.Object.Destroy(host);
            yield return UiShowcase.Frames(3);
        }

        static IEnumerator CareerList()
        {
            OpenShell.Instance.ShowCoreCareer();
            for (int i = 0; i < 400 && OpenShell.Instance.Layer.Q<VisualElement>(className: "oc-event") == null; i++) yield return null;
            yield return UiShowcase.Frames(40);
            yield return UiShowcase.Shot("career-list");
            OpenShell.Instance.ShowMain();
            yield return UiShowcase.Frames(5);
        }

        static unsafe RxCareerStatus Status()
        {
            var s = RxCareerStatus.Create();
            s.results_count = 12; s.continue_allowed = 1; s.reward_valid = 1; s.reward_credits_flag = 1; s.reward_base = 12500; s.reward_bonus = 2500;
            string[] names = { "Josh Cooper", "Ali Bouda", "Melissa Saito", "Cece Harris", "Dante Cruz", "Pat Moreno", "Ferret", "Ray Okafor", "Lena Voss", "Tomas Reyes", "Kai Anders", "Ivy Lang" };
            uint[] points = { 25, 22, 20, 18, 16, 15, 14, 13, 12, 11, 10, 9 };
            for (int i = 0; i < 12; i++)
            {
                var row = new RxCareerResultRow { rank = i, player_type = names[i] == "Ferret" ? 0 : 1, points = points[i], dnf = (uint)(i == 11 ? 1 : 0) };
                var bytes = System.Text.Encoding.UTF8.GetBytes(names[i]);
                for (int k = 0; k < bytes.Length && k < 127; k++) row.name[k] = bytes[k];
                ((RxCareerResultRow*)s.results)[i] = row;
            }
            return s;
        }

        static IEnumerator CareerResults()
        {
            UiShowcase.ShellVisible(false);
            var host = new GameObject("showcase career panels");
            var panels = new OwnCareerPanels(host.transform, () => true);
            panels.ShowResults(Status(), () => { });
            yield return UiShowcase.Frames(50);
            yield return UiShowcase.Shot("career-results");
            panels.ShowRewards(Status(), true, () => { });
            yield return UiShowcase.Frames(50);
            yield return UiShowcase.Shot("career-rewards");
            panels.Dispose(); UnityEngine.Object.Destroy(host);
            yield return UiShowcase.Frames(3);
        }
        static IEnumerator CareerRewards() { yield break; }   // (shot with the results above)

        static IEnumerator CareerHub()
        {
            UiShowcase.ShellVisible(false);
            var host = new GameObject("showcase career page");
            var view = new OwnCareerPageView(host.transform, () => true);
            view.Show("MOTOCAREER", "HEAT");
            view.Hero("National Moto", "Southwick");
            view.Heading("HEAT RESULTS");
            string[] names = { "Josh Cooper", "Ali Bouda", "Melissa Saito", "Cece Harris", "Ferret", "Pat Moreno", "Ray Okafor", "Lena Voss" };
            for (int i = 0; i < names.Length; i++) view.Row(i + 1, names[i] + (names[i] == "Ferret" ? "  (YOU)" : ""), names[i] == "Ferret", i < 4 ? "→ MAIN EVENT" : i < 6 ? "→ LCQ" : "OUT");
            view.Button("CONTINUE", "career-heat-continue", () => true);
            view.Button("LEAVE EVENT", "career-leave", () => true);
            yield return UiShowcase.Frames(60);
            yield return UiShowcase.Shot("career-hub");
            view.Dispose(); UnityEngine.Object.Destroy(host);
            yield return UiShowcase.Frames(3);
        }


        static OwnRaceResultsSnapshot Snapshot()
        {
            var race = new ReflexRaceState { valid = 1, state = 4, num_laps = 3, laps_completed = 3, place = 4, riders = 12 };
            var ui = new RxEventUiState { known = RxEventUiKnown.ActivityType | RxEventUiKnown.RaceState | RxEventUiKnown.RulesHold | RxEventUiKnown.PostState, activity_type = 0, race_state = 4, rules_hold = 0, post_state = 2 };
            var drivers = new ReflexDriverResult[12]; var names = new Dictionary<int, string>();
            string[] n = { "Josh Cooper", "Ali Bouda", "Melissa Saito", "Ferret", "Cece Harris", "Pat Moreno", "Ray Okafor", "Lena Voss", "Tomas Reyes", "Kai Anders", "Ivy Lang", "Dante Cruz" };
            for (int i = 0; i < 12; i++)
            {
                drivers[i] = new ReflexDriverResult { slot = i, local = i == 3 ? 1 : 0, place = i + 1, laps_completed = 3, finished = 1, race_time = 301.2f + i * 2.4f, best_lap = 97.4f + i * 0.31f, last_lap = 98f };
                names[i] = n[i];
            }
            if (!OwnRaceResultsSnapshot.TryCreate(1, "National/NAT_Showcase", true, race, ui, drivers, 12, names, out var snapshot, out var reason)) throw new InvalidOperationException(reason);
            return snapshot;
        }

        static IEnumerator RaceResults()
        {
            UiShowcase.ShellVisible(false);
            var go = new GameObject("showcase race results");
            var page = go.AddComponent<OwnRaceResults>(); page.enabled = false;   // (no world: its Update would close it)
            Set(page, "snapshot", Snapshot()); Set(page, "active", true); Set(page, "armed", true);
            Call(page, "Build"); Call(page, "ShowResults", new object[] { null });
            Drive(go, page);
            yield return UiShowcase.Frames(60);
            yield return UiShowcase.Shot("race-results");
            Call(page, "Confirm", true);
            yield return UiShowcase.Frames(50);
            yield return UiShowcase.Shot("race-results-confirm");
            Set(page, "active", false);
            UnityEngine.Object.Destroy(go);
            yield return UiShowcase.Frames(3);
        }

        static IEnumerator Pause()
        {
            UiShowcase.ShellVisible(false);
            var go = new GameObject("showcase pause");
            var page = go.AddComponent<OwnRacePause>(); page.enabled = false;   // (no world: its Update would close it)
            Set(page, "open", true);
            Call(page, "Build");
            Call(page, "BuildMain", UiShowcasePauseModes.Race(), "pause-resume");
            Drive(go, page);
            yield return UiShowcase.Frames(50);
            yield return UiShowcase.Shot("pause");
            Call(page, "Confirm", false);
            yield return UiShowcase.Frames(40);
            yield return UiShowcase.Shot("pause-confirm");
            Set(page, "open", false);
            UnityEngine.Object.Destroy(go);
            yield return UiShowcase.Frames(3);
        }
        static IEnumerator PauseConfirm() { yield break; }

        static ReflexEvents.Event AnyEvent() { foreach (var e in ReflexEvents.All) return e; throw new InvalidOperationException("no events"); }

        static IEnumerator PreChallenge()
        {
            UiShowcase.ShellVisible(false);
            var go = new GameObject("showcase pre-challenge");
            var page = go.AddComponent<OwnPreChallenge>();
            Set(page, "ev", AnyEvent());
            var ch = RxChallengeState.Create(); ch.valid = 1; ch.kind = 1; ch.criteria0 = 65f; ch.criteria1 = 130f; ch.criteria2 = 200f; ch.recordValid = 1; ch.record = 157f;
            Call(page, "Open", ch);
            yield return UiShowcase.Frames(50);
            yield return UiShowcase.Shot("pre-challenge");
            UnityEngine.Object.Destroy(go);
            yield return UiShowcase.Frames(3);
        }

        static IEnumerator PostChallenge(int medal, string shot)
        {
            UiShowcase.ShellVisible(false);
            var go = new GameObject("showcase post-challenge");
            var page = go.AddComponent<OwnPostChallenge>();
            Set(page, "ev", AnyEvent());
            var ch = RxChallengeState.Create(); ch.valid = 1; ch.kind = 1; ch.state = 4; ch.medal = medal; ch.gold = medal == 3 ? 1 : 0; ch.valueValid = 1; ch.value = medal == 3 ? 212.6f : 48f;
            ch.criteria0 = 65f; ch.criteria1 = 130f; ch.criteria2 = 200f; ch.recordValid = 1; ch.record = 212f;
            Call(page, "Build"); Call(page, "Populate", ch);
            var root = (VisualElement)page.GetType().GetField("root", All).GetValue(page);
            root.style.display = DisplayStyle.Flex;
            var layer = (VisualElement)page.GetType().GetField("layer", All).GetValue(page);
            OuMotion.Fade(layer);
            if (medal > 0)
            {   // the prize
                var payoutRow = (VisualElement)page.GetType().GetField("payoutRow", All).GetValue(page);
                payoutRow.style.display = DisplayStyle.Flex;
                ((Label)page.GetType().GetField("prizeValue", All).GetValue(page)).text = "1,250"; ((Label)page.GetType().GetField("bonusValue", All).GetValue(page)).text = "250"; ((Label)page.GetType().GetField("totalValue", All).GetValue(page)).text = "1,500";
            }
            var b0 = ((VisualElement[])page.GetType().GetField("button", All).GetValue(page))[0];
            b0.schedule.Execute(() => b0.Focus());
            Set(page, "open", true); Set(page, "armed", true); Set(page, "openedFrame", -1);
            Drive(go, page, "InputTick");
            yield return UiShowcase.Frames(50);
            yield return UiShowcase.Shot(shot);
            UnityEngine.Object.Destroy(go);
            yield return UiShowcase.Frames(3);
        }

        static IEnumerator AaaLoading()
        {
            var page = OwnAaaLoading.ShowHeld();
            yield return UiShowcase.Frames(50);
            yield return UiShowcase.Shot("aaa-loading");
            page.Release();
            yield return UiShowcase.Frames(3);
        }
    }
}

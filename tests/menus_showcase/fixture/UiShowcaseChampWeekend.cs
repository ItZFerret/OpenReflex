// TEST ONLY (w-champweekend; tests/menus_showcase, run.py --batch): the championship weekend's front end from the product's own code.
//   champweekend-setup     RACE > a National's setup page: the EVENT tab's FORMAT row (SINGLE RACE / CHAMPIONSHIP WEEKEND, pad right), START
//                          reads SET UP WEEKEND, A opens the weekend setup page (STAGES: CLOCK + LAPS per stage; FIELD: AI, FIELD SIZE),
//                          a CLOCK change through the pad, B back to the setup page (the weekend goes); a Supercross weekend's page too,
//                          its FIELD SIZE through the pad to 24 and 32 (the split's line and the rules follow, the event's setup keeps it:
//                          its own FIELD SIZE row reads the same), then back to the 12 it had
//   champweekend-card      the weekend card between two stages from synthetic weekends (National after MOTO 1, Supercross after the heat:
//                          MAIN / LCQ / OUT, the final standings, a 24-rider Supercross after the heat)
//   champweekend-hud       the MODERN HUD's lap counter of a timed stage: the clock, "+1 LAP", "FINAL LAP" (synthetic models)
//   champweekend-e2e-national / champweekend-e2e-supercross
//                          a whole weekend in the real flow: the setup page (FORMAT through the pad), the weekend page (every stage's CLOCK
//                          set to REFLEX_CW_CLOCK minutes, default 2, + 1 lap, the practice + 0, through the pad), START WEEKEND, the load
//                          arena's PRESS START, then per stage: the intro skipped, the race-line autopilot (the core career test's) rides
//                          a virtual pad, the lap counters are read every half second (the MODERN HUD's text, the classic HUD's, the model's)
//                          and must never show the 96-lap placeholder, the results page (its title the stage's, no RACE AGAIN), CONTINUE
//                          WEEKEND (or B), the weekend card (the stage recorded, the next one is a new stage: the "practice again" bug),
//                          its START (the pad's A) loads the next stage in the same scene. REFLEX_CW_ACTIONS: a comma list, one per stage:
//                          continue | back (B on the results) | restart (the pause page's RESTART once, then the stage is raced) | retire
//                          (the pause page's RETIRE FROM <STAGE>) | quit (the pause page's QUIT WEEKEND: the menu). The end: FINISH
//                          WEEKEND (or the quit), the event's setup page back, no weekend left.
// Result lines "[ui-showcase] [champweekend] PASS|FAIL ..."; failures count as the showcase's NAV failures.
using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;
using Reflex.Port.Content;
using Reflex.Port.Hud;
using Reflex.Port.OpenFrontEnd;
using Reflex.Port.OriginalWorld;
using Reflex.Port.RxCore;
using UnityEngine;
using UnityEngine.InputSystem;
using UnityEngine.InputSystem.LowLevel;
using UnityEngine.UIElements;

namespace Reflex.Port.Showcase
{
    static class UiShowcaseChampWeekend
    {
        const string Tag = "[ui-showcase] [champweekend] ";
        const BindingFlags All = BindingFlags.Instance | BindingFlags.Static | BindingFlags.Public | BindingFlags.NonPublic;

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        static void Register()
        {
            UiShowcase.Register("champweekend-setup", Setup);
            UiShowcase.Register("champweekend-card", Cards);
            UiShowcase.Register("champweekend-hud", Hud);
            UiShowcase.Register("champweekend-e2e-national", () => E2E("National"));
            UiShowcase.Register("champweekend-e2e-supercross", () => E2E("Supercross"));
            // muted (the user's rule for test players): the scratch save's MASTER VOLUME 0, the UI sounds' volume 0, the listener held at 0
            if ((Environment.GetEnvironmentVariable("REFLEX_UI_SHOWCASE") ?? "").Contains("champweekend"))
            {
                try { Reflex.Port.Options.ReflexSettings.MasterVolume = 0f; } catch (Exception e) { Debug.LogWarning(Tag + "mute: " + e.Message); }
                PlayerPrefs.SetFloat("ReflexAudio.master", 0f);
                var g = new GameObject("champ weekend mute (test only)"); UnityEngine.Object.DontDestroyOnLoad(g); g.AddComponent<Mute>();
            }
        }
        sealed class Mute : MonoBehaviour { void LateUpdate() { if (AudioListener.volume != 0f) AudioListener.volume = 0f; } }

        static OpenShell Shell => OpenShell.Instance;
        static string Focused => (Shell.Nav.Focused as VisualElement)?.name ?? "(none)";
        static VisualElement Q(string name) => Shell.Root.Q(name);
        static void Log(string s) => Debug.Log(Tag + "[" + Time.realtimeSinceStartup.ToString("F0") + " s] " + s);
        static bool Check(bool ok, string what) { Debug.Log(Tag + (ok ? "PASS " : "FAIL ") + what); UiShowcaseNav.Check(ok, "champweekend", what); return ok; }
        static IEnumerator Tap(GamepadButton b) { yield return UiShowcaseNav.PadTap(b); yield return UiShowcase.Frames(6); }
        static IEnumerator Seconds(float s) { float t0 = Time.realtimeSinceStartup; while (Time.realtimeSinceStartup - t0 < s) yield return null; }
        static void Set(object o, string field, object value) => o.GetType().GetField(field, All).SetValue(o, value);
        static string Value(string row) => Q(row)?.Q<Label>(className: "mxa-value")?.text ?? "(no row)";
        static string FocusIn(VisualElement root) => (root?.panel?.focusController?.focusedElement as VisualElement)?.name ?? "(none)";
        /// The pause page's focused row (its own OuNav: the panel's focus controller does not follow it).
        static string PauseFocus(OwnRacePause p) => (typeof(OwnRacePause).GetField("nav", All)?.GetValue(p) as OuNav)?.Focused?.name ?? FocusIn(p?.Root);

        static OpenEvent EventOf(string type, string location)
        {
            var evs = OpenContent.Events.Where(e => !e.freeRide && e.mod == null && e.ev != null && e.ev.spec.type == type).ToList();
            return evs.FirstOrDefault(e => e.ev.location == location) ?? evs.FirstOrDefault();
        }

        static IEnumerator ToSetup(OpenEvent ev)
        {
            Shell.ShowMain(); yield return UiShowcase.Frames(8);
            Shell.ShowBrowser(false); yield return UiShowcase.Frames(10);
            if (!Shell.SelectEvent(ev.key)) Set(Shell, "_browseEvent", ev);
            Set(Shell, "_setup", OpenHistory.SetupOf(ev.key));
            Shell.ShowSetupPage();
            yield return UiShowcase.Frames(40);
            typeof(OpenShell).GetMethod("SetSetupTab", All).Invoke(Shell, new object[] { 0, false });
            yield return UiShowcase.Frames(4);
        }

        /// The FORMAT row to `weekend` through the pad (right on the focused row), then focus START.
        static IEnumerator Format(bool weekend)
        {
            var row = Q("ou-setup-format");
            if (!Check(row != null, "the setup page has a FORMAT row")) yield break;
            Shell.Nav.Focus(row); yield return UiShowcase.Frames(6);
            for (int i = 0; i < 2 && (Value("ou-setup-format") == "CHAMPIONSHIP WEEKEND") != weekend; i++) yield return Tap(GamepadButton.DpadRight);
            Check((Value("ou-setup-format") == "CHAMPIONSHIP WEEKEND") == weekend, "FORMAT reads " + Value("ou-setup-format"));
        }

        static IEnumerator FocusAndTap(string name, GamepadButton b)
        {
            var e = Q(name);
            if (!Check(e != null, "the page has " + name)) yield break;
            Shell.Nav.Focus(e); yield return UiShowcase.Frames(6);
            yield return Tap(b);
        }

        /// A stage's CLOCK / LAPS to a value through the pad (left / right on the focused row).
        static IEnumerator Spin(string name, Func<int> now, int want)
        {
            var row = Q(name);
            if (row == null) { Check(false, "no row " + name); yield break; }
            Shell.Nav.Focus(row); yield return UiShowcase.Frames(4);
            for (int i = 0; i < 70 && now() != want; i++) yield return UiShowcaseNav.PadTap(now() > want ? GamepadButton.DpadLeft : GamepadButton.DpadRight);
            yield return UiShowcase.Frames(4);
            Check(now() == want, name + " = " + now() + " (wanted " + want + "), shown '" + Value(name) + "'");
        }

        // ------------------------------------------------------------------------------------------------ the setup page

        static IEnumerator Setup()
        {
            var nat = EventOf("National", "NAT_Concord"); var sx = EventOf("Supercross", "SX_Springfield");
            if (!Check(nat != null && sx != null, "the install has a National and a Supercross event")) yield break;
            PlayerPrefs.DeleteKey("OpenReflex.ChampWeekend.Format." + nat.key);
            for (int s = 0; s < 8; s++) foreach (var d in new[] { 0, 1 }) { PlayerPrefs.DeleteKey("OpenReflex.ChampWeekend." + d + "." + s + ".min"); PlayerPrefs.DeleteKey("OpenReflex.ChampWeekend." + d + "." + s + ".laps"); }
            yield return ToSetup(nat);
            Check(Shell.Current == OpenShell.Screen.Setup, "the setup page opened");
            Check(Q("ou-champweekend") == null, "no CHAMPIONSHIP WEEKEND chip in the browser's tools row");
            Check(Value("ou-setup-format") == "SINGLE RACE", "FORMAT starts at SINGLE RACE (" + Value("ou-setup-format") + ")");
            Check(Q("ou-setup-duration") != null, "SINGLE RACE keeps EVENT DURATION");
            yield return UiShowcase.Shot("champweekend-setup-1-single");
            yield return Format(true);
            Check(Q("ou-setup-duration") == null, "the weekend's stages have their own laps: no EVENT DURATION");
            Check(Shell.StartButton?.Q<Label>()?.text?.StartsWith("SET UP WEEKEND") == true, "START reads " + Shell.StartButton?.Q<Label>()?.text);
            yield return UiShowcase.Shot("champweekend-setup-2-weekend");
            Shell.Nav.Focus(Shell.StartButton); yield return UiShowcase.Frames(4);
            yield return Tap(GamepadButton.South);
            yield return UiShowcase.Frames(30);
            Check(Shell.ChampSetupOpen, "A on SET UP WEEKEND opens the weekend setup page");
            Check(RxChampWeekend.Active && RxChampWeekend.Discipline == RxChampWeekend.National && RxChampWeekend.StageCount == 4, "a National weekend of 4 stages is set up");
            Check(Value("champweekend-clock-2") == "15:00" && Value("champweekend-laps-2") == "+ 1", "MOTO 1 starts at the default 15:00 + 1 (" + Value("champweekend-clock-2") + " " + Value("champweekend-laps-2") + ")");
            yield return Spin("champweekend-clock-1", () => RxChampWeekend.StageMinutes(1), 4);
            Check(PlayerPrefs.GetInt("OpenReflex.ChampWeekend.0.1.min", -1) == 4, "the HEAT's clock is remembered");
            var text = string.Join(" | ", Shell.Root.Query<Label>(className: "ou-cw-stage-sub").ToList().Select(l => l.text));
            Log("the stages' flow: " + text);
            Check(text.Contains("4:00 + 1 LAP"), "the flow follows the change");
            Shell.Nav.Focus(Shell.StartButton); yield return UiShowcase.Frames(6);
            Check(Shell.Root.Query<Label>().ToList().All(l => l.text == null || !l.text.Contains("RESTORED")), "no RESTORED text on the page");
            yield return UiShowcase.Shot("champweekend-setup-3-stages");
            typeof(OpenShell).GetMethod("ChampTab", All).Invoke(Shell, new object[] { 1, true }); yield return UiShowcase.Frames(10);
            Check(Q("champweekend-ai") != null && Q("champweekend-riders-slot") != null && Q("champweekend-riders") != null, "FIELD has AI OPPONENTS and FIELD SIZE (" + Value("champweekend-riders") + ")");
            Check(Value("champweekend-riders").StartsWith("12 RIDERS"), "a weekend starts at the original's 12 riders");
            yield return UiShowcase.Shot("champweekend-setup-4-field");
            yield return Tap(GamepadButton.East); yield return UiShowcase.Frames(20);
            Check(Shell.Current == OpenShell.Screen.Setup && !Shell.ChampSetupOpen && Q("ou-setup-format") != null, "B goes back to the event's setup page");
            Check(!RxChampWeekend.Active, "the weekend that was being set up is gone");
            PlayerPrefs.DeleteKey("OpenReflex.ChampWeekend.0.1.min"); PlayerPrefs.DeleteKey("OpenReflex.ChampWeekend.0.1.laps");
            yield return Format(false);
            // Supercross: its four stages and the LCQ
            yield return ToSetup(sx);
            yield return Format(true);
            Shell.Nav.Focus(Shell.StartButton); yield return UiShowcase.Frames(4);
            yield return Tap(GamepadButton.South); yield return UiShowcase.Frames(30);
            Check(Shell.ChampSetupOpen && RxChampWeekend.Discipline == RxChampWeekend.Supercross && RxChampWeekend.StageLabel(2) == "LCQ", "a Supercross weekend: PRACTICE > HEAT > LCQ > MAIN");
            Shell.Nav.Focus(Shell.StartButton); yield return UiShowcase.Frames(6);
            Check((Q<Label>("champweekend-rules")?.text ?? "").Contains("Heat 1st to 5th go straight to the main, 6th to 11th to the LCQ, whose top 2 join them."), "12 riders: the original's split in the rules (" + Q<Label>("champweekend-rules")?.text + ")");
            yield return UiShowcase.Shot("champweekend-setup-5-supercross");
            // FIELD SIZE: 24 then 32 through the pad; the split follows; the event's setup keeps the choice
            int before = SetupNow().riders;
            typeof(OpenShell).GetMethod("ChampTab", All).Invoke(Shell, new object[] { 1, true }); yield return UiShowcase.Frames(10);
            Log("FIELD SIZE row: '" + Value("champweekend-riders") + "', the split: '" + Q<Label>("champweekend-split")?.text + "'");
            Check(Q<Label>("champweekend-split")?.text == "A HEAT OF 12  ›  A MAIN OF 7: HEAT 1-5 + LCQ TOP 2  (6 IN THE LCQ, 1 OUT)", "12 riders: the split's line (" + Q<Label>("champweekend-split")?.text + ")");
            foreach (int n in new[] { 24, 32 })
            {
                yield return Spin("champweekend-riders", () => SetupNow().riders, n);
                RxChampWeekend.Split(n, out int toMain, out int lcqLast, out int lcqToMain);
                string line = Q<Label>("champweekend-split")?.text ?? "", rules = Q<Label>("champweekend-rules")?.text ?? "";
                Log(n + " riders: '" + Value("champweekend-riders") + "' | " + line + " | " + rules);
                Check(Value("champweekend-riders").StartsWith(n + " RIDERS"), n + " riders: the row reads " + Value("champweekend-riders"));
                Check(line.Contains("A HEAT OF " + n + "  ›  A MAIN OF " + (toMain + lcqToMain) + ": HEAT 1-" + toMain + " + LCQ TOP " + lcqToMain), n + " riders: the split's line follows (" + line + ")");
                Check(rules.Contains("Heat 1st to " + Th(toMain) + " go straight to the main, " + Th(toMain + 1) + " to " + Th(lcqLast) + " to the LCQ, whose top " + lcqToMain + " join them."), n + " riders: the rules follow");
                Check(OpenHistory.SetupOf(sx.key).riders == n, n + " riders: saved with the event's setup (" + OpenHistory.SetupOf(sx.key).riders + ")");
                var chips = string.Join(" | ", Shell.Root.Query<Label>().ToList().Select(l => l.text ?? "").Where(t => t.EndsWith(" RIDERS")));
                Check(chips.Contains(n + " RIDERS"), n + " riders: the summary's FIELD (" + chips + ")");
                Check(Q<Label>("champweekend-riders-note")?.text?.Contains("You and " + (n - 1) + " computer riders") == true, n + " riders: the row's note (" + Q<Label>("champweekend-riders-note")?.text + ")");
                yield return UiShowcase.Shot("champweekend-setup-" + (n == 24 ? "6" : "7") + "-supercross-" + n);
            }
            yield return Tap(GamepadButton.East); yield return UiShowcase.Frames(20);
            Check(Value("ou-setup-riders").StartsWith("32 RIDERS"), "the event's setup page's FIELD SIZE reads the weekend's choice (" + Value("ou-setup-riders") + ")");
            var setupNow = SetupNow(); setupNow.riders = before; OpenHistory.SaveSetup(setupNow);
            yield return Format(false);
        }

        static T Q<T>(string name) where T : VisualElement => Shell.Root.Q<T>(name);
        static string Th(int n) => n + (n % 100 >= 11 && n % 100 <= 13 ? "th" : n % 10 == 1 ? "st" : n % 10 == 2 ? "nd" : n % 10 == 3 ? "rd" : "th");
        static OpenSetup SetupNow() => (OpenSetup)typeof(OpenShell).GetField("_setup", All).GetValue(Shell);

        // ------------------------------------------------------------------------------------------------ the weekend card (synthetic)

        static readonly string[] Names = { "SAITO", "BOUDA", "MARQUEZ", "PETERS", "WEASLE", "COONCE", "MCFERRON", "SNOW", "HARRIS", "DUNGEY", "TICKLE" };

        static ChampWeekendView Synthetic(int discipline, int raced, int local, bool finished = false, int n = 12)
        {
            var v = new ChampWeekendView { discipline = discipline, disciplineName = ChampWeekendView.DisciplineName(discipline), timeable = true, finished = finished };
            v.eventLabel = discipline == RxChampWeekend.National ? "NATIONAL  ·  CONCORD" : "SUPERCROSS  ·  SPRINGFIELD";
            string[] labels = discipline == RxChampWeekend.National ? new[] { "PRACTICE", "HEAT", "MOTO 1", "MOTO 2" } : new[] { "PRACTICE", "HEAT", "LCQ", "MAIN" };
            int[] min = discipline == RxChampWeekend.National ? new[] { 10, 5, 15, 15 } : new[] { 6, 4, 3, 10 };
            for (int s = 0; s < 4; s++)
            {
                var st = new ChampWeekendView.Stage { label = labels[s], minutes = min[s], laps = s == 0 ? 0 : 1, scored = discipline == RxChampWeekend.National ? s >= 2 : s == 3, raced = s < raced };
                st.format = st.laps == 0 ? min[s] + ":00" : min[s] + ":00 + " + st.laps + (st.laps == 1 ? " LAP" : " LAPS");
                st.field = discipline == RxChampWeekend.Supercross && s == 2 ? RxChampWeekend.FieldLcq : discipline == RxChampWeekend.Supercross && s == 3 ? RxChampWeekend.FieldMain : RxChampWeekend.FieldAll;
                st.grid = s == 0 ? RxChampWeekend.GridEvent : s == 1 ? RxChampWeekend.GridBestLap : discipline == RxChampWeekend.Supercross && s == 3 ? RxChampWeekend.GridMain : RxChampWeekend.GridPlace;
                st.gridFrom = s == 0 ? -1 : discipline == RxChampWeekend.Supercross && s >= 2 ? 1 : s - 1;
                v.stages.Add(st);
            }
            v.last = raced - 1; v.next = finished ? -1 : raced;
            var rng = new System.Random(7);
            for (int i = 0; i < n; i++)
            {
                int k = i < local ? i : i - 1;
                var r = new ChampWeekendView.Rider { name = i == local ? "FERRET" : k < Names.Length ? Names[k] : "RIDER " + (k + 1), local = i == local };
                for (int s = 0; s < raced; s++)
                {
                    r.place[s] = ((i + s * 5 + rng.Next(3)) % n) + 1;
                    r.bestLap[s] = 61.5f + i * 0.4f + s * 0.1f; r.gate[s] = (i + s) % n + 1;
                }
                if (discipline == RxChampWeekend.Supercross && raced >= 2) r.place[1] = i + 1;   // (the heat's order = the standings here)
                r.points = discipline == RxChampWeekend.National ? Math.Max(0, (raced >= 3 ? 25 - i * 2 : 0) + (raced >= 4 ? 22 - i * 2 : 0)) : 0;
                v.riders.Add(r);
            }
            if (discipline == RxChampWeekend.National) v.riders.Sort((a, b) => b.points.CompareTo(a.points));
            v.Derive();
            return v;
        }

        static IEnumerator Card(string name, ChampWeekendView v)
        {
            UiShowcase.ShellVisible(false);   // (the menu world behind it, as the race's world is in game)
            GameObject backdrop = null;
            var host = new GameObject("weekend card gallery");
            var card = host.AddComponent<OwnChampWeekendCard>();
            card.Preview(v);
            yield return UiShowcase.Frames(20);
            Check(card.Active && card.Page == "card", name + ": the card is up");
            var labels = card.Root.Query<Label>().ToList().Select(l => l.text ?? "").ToList();
            Log(name + ": " + string.Join(" | ", labels.Where(t => t.Length > 0).Take(40)));
            Check(!labels.Any(t => t.Contains("RESTORED")), name + ": no RESTORED text");
            Check(v.next < 0 ? card.Root.Q("cw-finish") != null : card.Root.Q("cw-next") != null, name + ": its action (" + (v.next < 0 ? "FINISH" : "NEXT STAGE") + ")");
            yield return UiShowcase.Shot(name);
            UnityEngine.Object.Destroy(host); if (backdrop) UnityEngine.Object.Destroy(backdrop);
            yield return UiShowcase.Frames(4);
            UiShowcase.ShellVisible(true);
        }

        static IEnumerator Cards()
        {
            var nat = Synthetic(RxChampWeekend.National, 3, 3);
            Check(nat.NextStage?.label == "MOTO 2" && nat.nextGate > 0, "the National card's next stage MOTO 2, your gate " + nat.nextGate + " of " + nat.nextField);
            yield return Card("champweekend-card-1-national", nat);
            var sx = Synthetic(RxChampWeekend.Supercross, 2, 7);
            Check(sx.Local.status == "LCQ" && sx.riders[0].status == "MAIN" && sx.riders[11].status == "OUT" && sx.NextStage?.label == "LCQ" && sx.nextField == 6, "Supercross after the heat: 1-5 MAIN, 6-11 LCQ, 12 OUT; the LCQ's field of 6 (" + sx.nextField + "), your gate " + sx.nextGate);
            yield return Card("champweekend-card-2-supercross-lcq", sx);
            var fin = Synthetic(RxChampWeekend.National, 4, 1, true);
            yield return Card("champweekend-card-3-final", fin);
            // FIELD SIZE: a 24-rider Supercross after the heat: 1-10 MAIN, 11-22 LCQ (its top 4 to the main), 23-24 OUT
            var big = Synthetic(RxChampWeekend.Supercross, 2, 13, false, 24);
            Check(big.heatField == 24 && big.heatToMain == 10 && big.heatLcqLast == 22 && big.lcqToMain == 4, "24 riders: the split 10 / 22 / 4 (" + big.heatToMain + " / " + big.heatLcqLast + " / " + big.lcqToMain + ")");
            Check(big.Local.status == "LCQ" && big.riders[9].status == "MAIN" && big.riders[10].status == "LCQ" && big.riders[22].status == "OUT" && big.nextField == 12,
                  "24 riders after the heat: 1-10 MAIN, 11-22 LCQ, 23-24 OUT; the LCQ's field of 12 (" + big.nextField + "), your gate " + big.nextGate);
            Check(big.LocalStatusLine(out _) == "TO THE LCQ  ·  THE TOP 4 MAKE THE MAIN", "24 riders: " + big.LocalStatusLine(out _));
            yield return Card("champweekend-card-5-supercross-24", big);
        }

        // ------------------------------------------------------------------------------------------------ the HUD's timed lap counter (synthetic)

        static IEnumerator HudPage(string name, Action<ReflexHudModel> fill)
        {
            UiShowcase.ShellVisible(false);
            var backdrop = HudShowcase.Backdrop();
            var view = ReflexHudView.Create(null, "HUD gallery " + name, 30001);
            view.Options.scale = 1f; view.Options.opacity = 1f; view.Options.standings = true; view.Options.gauge = true; view.Options.reduceMotion = false;
            var m = new ReflexHudModel();
            m.mode = HudMode.Race; m.valid = true; m.inRace = true; m.racing = true; m.state = 2; m.place = 4; m.riders = 12; m.haveHud = true; m.onTrack = true; m.speedMph = 58f; m.gear = 3;
            m.hasBest = true; m.bestLap = 71.84f; m.lastLap = 72.40f; m.lapTime = 33.1f; m.shownLapTime = 33.1f; m.champStage = "MOTO 1";
            m.timed = true; m.timedSeconds = 900f; m.timedExtra = 1;
            fill(m);
            view.Visible = true;
            float t0 = Time.unscaledTime;
            while (Time.unscaledTime < t0 + 1f) { view.Apply(m, Time.unscaledDeltaTime, 2); yield return null; }
            var lap = typeof(ReflexHudView).GetField("lapNum", All)?.GetValue(view) as UnityEngine.UI.Text;
            Log(name + ": the lap counter reads '" + lap?.text + "'");
            string plain = System.Text.RegularExpressions.Regex.Replace(lap?.text ?? "", "<[^>]*>", "");
            Check(lap != null && !plain.Contains("96") && !plain.Contains("/"), name + ": no lap total while timed (" + plain + ")");
            yield return UiShowcase.Shot(name);
            view.Destroy(); UnityEngine.Object.Destroy(backdrop);
            yield return UiShowcase.Frames(3);
            UiShowcase.ShellVisible(true);
        }

        static IEnumerator Hud()
        {
            yield return HudPage("champweekend-hud-1-clock", m => { m.laps = RxChampWeekend.TimedLaps; m.lapsCompleted = 4; m.lap = 5; m.timedLeft = 287.4f; });
            yield return HudPage("champweekend-hud-2-plus-one", m => { m.laps = 9; m.lapsCompleted = 7; m.lap = 8; m.timedExpired = true; m.timedToGo = 2; });
            yield return HudPage("champweekend-hud-3-final", m => { m.laps = 9; m.lapsCompleted = 8; m.lap = 9; m.timedExpired = true; m.timedToGo = 1; m.finalLap = true; m.finalLapUntil = Time.unscaledTime + 100f; });
        }

        // ------------------------------------------------------------------------------------------------ a whole weekend (the real flow)

        sealed class Pilot : MonoBehaviour
        {
            public bool drive; Vector3 bikePos, bikeFwd; float speed; readonly float[] line = new float[3 * 64]; int points; bool have; float stuck, reverse;
            void OnEnable() => OriginalWorldRuntime.ViewApplied += OnView;
            void OnDisable() => OriginalWorldRuntime.ViewApplied -= OnView;
            void OnView(OriginalWorldRuntime world, OriginalSimulation.Results v)
            {
                if (v.bikePose == null || v.bikeNodes < 1) return;
                bikePos = new Vector3(v.bikePose[12], v.bikePose[13], v.bikePose[14]); bikeFwd = new Vector3(v.bikePose[8], v.bikePose[9], v.bikePose[10]);
                speed = v.bike.speed; points = Math.Min(64, v.raceLinePoints); Array.Copy(v.raceLine, line, points * 3); have = true;
            }
            // the race-line autopilot of core/tools/career_series_test.c (GhostRideProbe's): left stick steering, right trigger throttle, left trigger brake
            void Update()
            {
                if (!drive || !have || points < 2) return;
                int nearest = 0; float best = float.MaxValue;
                for (int i = 0; i < points && i < 6; i++) { float dx = line[3 * i] - bikePos.x, dz = line[3 * i + 2] - bikePos.z, d = dx * dx + dz * dz; if (d < best) { best = d; nearest = i; } }
                float ahead = 18f + speed * 0.6f; int target = nearest;
                for (int i = nearest; i < points; i++) { float dx = line[3 * i] - bikePos.x, dz = line[3 * i + 2] - bikePos.z; target = i; if (dx * dx + dz * dz >= ahead * ahead) break; }
                float err = Mathf.DeltaAngle(Mathf.Atan2(bikeFwd.x, bikeFwd.z) * Mathf.Rad2Deg, Mathf.Atan2(line[3 * target] - bikePos.x, line[3 * target + 2] - bikePos.z) * Mathf.Rad2Deg) * Mathf.Deg2Rad;
                float steer = Mathf.Clamp(err * 2.2f, -1f, 1f), turn = Mathf.Abs(err);
                float desired = turn > 1.2f ? 8f : turn > 0.6f ? 14f : turn > 0.3f ? 20f : 30f;
                float dt = Time.unscaledDeltaTime;
                if (speed < 1f) stuck += dt; else stuck = 0;
                if (stuck > 4f) { reverse = 1.5f; stuck = 0; }
                var s = new GamepadState();
                if (reverse > 0) { reverse -= dt; s.leftTrigger = 1f; s.leftStick = new Vector2(-steer, 0); }
                else { s.leftStick = new Vector2(steer, 0); if (speed < desired) s.rightTrigger = 1f; else if (speed > desired + 6f) s.leftTrigger = 0.63f; else s.rightTrigger = 0.47f; }
                UiShowcaseNav.PadState(s);
            }
            public void Stop() { drive = false; UiShowcaseNav.PadState(new GamepadState()); }
        }

        static IEnumerator Shot(string name) { yield return UiShowcase.Shot(name); }

        /// The lap counters on screen now: the MODERN HUD's, the classic HUD's, the model's (TimedText or n / N).
        static string Counters(OriginalWorldRuntime rt, ReflexHudModel m)
        {
            string modern = "-", classic = "-";
            var mh = UnityEngine.Object.FindAnyObjectByType<ReflexModernHud>();
            if (mh && mh.View != null) modern = (typeof(ReflexHudView).GetField("lapNum", All)?.GetValue(mh.View) as UnityEngine.UI.Text)?.text ?? "-";
            if (rt && rt.originalHud)
            {
                var node = typeof(OriginalHud).GetField("laps", All)?.GetValue(rt.originalHud) as OriginalUiTree.Node;
                if (node?.text) classic = node.text.Text;
            }
            string model = m.timed ? "timed " + m.TimedText(false) : m.lap + "/" + m.laps;
            return "modern '" + modern + "' classic '" + classic + "' model " + model;
        }

        static IEnumerator E2E(string type)
        {
            int clock = int.TryParse(Environment.GetEnvironmentVariable("REFLEX_CW_CLOCK"), out int c) ? c : 2;
            var actions = (Environment.GetEnvironmentVariable("REFLEX_CW_ACTIONS") ?? (type == "National" ? "continue,back,restart,retire" : "continue,continue,continue,continue")).Split(',').Select(x => x.Trim()).ToList();
            var ev = EventOf(type, type == "National" ? "NAT_Concord" : "SX_Springfield");
            if (!Check(ev != null, type + ": an event")) yield break;
            Log(type + ": " + ev.key + ", every stage " + clock + ":00 (+ 1, the practice + 0), actions " + string.Join(",", actions));
            var pilotGo = new GameObject("champ weekend pilot (test only)"); UnityEngine.Object.DontDestroyOnLoad(pilotGo);
            var pilot = pilotGo.AddComponent<Pilot>();
            Application.runInBackground = true;

            // ---- the setup page, the weekend page (the player's way: the pad)
            yield return ToSetup(ev);
            yield return Format(true);
            Shell.Nav.Focus(Shell.StartButton); yield return UiShowcase.Frames(4);
            yield return Tap(GamepadButton.South); yield return UiShowcase.Frames(30);
            if (!Check(Shell.ChampSetupOpen, type + ": the weekend setup page")) yield break;
            int count = RxChampWeekend.StageCount;
            for (int s = 0; s < count; s++)
            {
                int stage = s;
                yield return Spin("champweekend-clock-" + s, () => RxChampWeekend.StageMinutes(stage), clock);
                yield return Spin("champweekend-laps-" + s, () => RxChampWeekend.StageLaps(stage), s == 0 ? 0 : 1);
            }
            Shell.Nav.Focus(Q("champweekend-start")); yield return UiShowcase.Frames(6);
            yield return Shot("champweekend-e2e-" + type.ToLowerInvariant() + "-0-setup");
            yield return Tap(GamepadButton.South);
            Log(type + ": START WEEKEND pressed: " + RxChampWeekend.Armed);
            // the load arena: PRESS START once the event is loaded (shown in the menus with REFLEX_LOADARENA_RIDE=0, else ridden: the test ride's own
            // session in the core's second image beside the event's prestart, whose end used to take the weekend's stage away)
            bool sawRide = false; float pressedAt = -10f;
            for (float t = 0; t < 300f; t += Time.unscaledDeltaTime)
            {
                if (OpenLoadArena.Riding) sawRide = true;
                bool shownReady = OpenShell.Instance && OpenShell.Instance.LoadArenaPhase == "ready";
                bool rideReady = OpenLoadArena.Riding && OpenLoadArena.Phase == "riding" && OpenEventPrestart.Current && OpenEventPrestart.Current.Ready;
                if ((shownReady || rideReady) && Time.realtimeSinceStartup - pressedAt > 2f)
                {
                    yield return Seconds(rideReady ? 3f : 0.5f);   // (the ride: a few seconds in the arena first)
                    pressedAt = Time.realtimeSinceStartup; yield return Tap(GamepadButton.Start); Log(type + ": PRESS START in the load arena (" + (rideReady ? "ridden" : "shown") + ")");
                }
                var w = UnityEngine.Object.FindObjectsByType<OriginalWorldRuntime>(FindObjectsSortMode.None).FirstOrDefault(x => x && (x.EventIdentity ?? "").StartsWith(type + "/"));
                if (!OpenLoadArena.Riding && w && RxChampWeekend.StageInSession) break;
                yield return null;
            }
            Log(type + ": the load arena was " + (sawRide ? "ridden" : "not ridden"));

            int prevStage = -1, raced = 0; string prevLabel = null; bool restarted = false, ended = false;
            var m = new ReflexHudModel();
            while (!ended && raced < 12)
            {
                // ---- a stage loads
                OriginalWorldRuntime rt = null;
                for (float t = 0; t < 300f; t += Time.unscaledDeltaTime)
                {
                    rt = UnityEngine.Object.FindObjectsByType<OriginalWorldRuntime>(FindObjectsSortMode.None).FirstOrDefault(w => w && (w.EventIdentity ?? "").StartsWith(type + "/"));   // (not the load arena's ride)
                    if (rt && rt.Running && RxChampWeekend.StageInSession && (rt.IntroCardShown || rt.EventUi.race_state >= 1)) break;
                    yield return null;
                }
                if (!Check(rt && rt.Running && RxChampWeekend.StageInSession, type + ": a stage's session runs")) break;
                int stage = RxChampWeekend.SessionStage; string label = RxChampWeekend.StageLabel(stage);
                string action = stage < actions.Count ? actions[stage] : "continue";
                Log(type + ": == stage " + stage + " " + label + " (" + RxChampWeekend.StageFormat(stage) + "), action " + action + ", identity " + rt.EventIdentity);
                if (restarted) Check(stage == prevStage, type + ": the pause page's RESTART raced the same stage (" + label + ")");
                else Check(stage > prevStage && label != prevLabel, type + ": a new stage after " + (prevLabel ?? "the start") + " (not the same one again)");
                Check(!RxChampWeekend.StageRaced(stage), type + ": " + label + " is not recorded before it is raced");
                if (rt.IntroCardShown) { yield return Seconds(1f); yield return Tap(GamepadButton.South); }
                for (float t = 0; t < 120f && !(rt && rt.Running && rt.Race.state == 2); t += Time.unscaledDeltaTime) yield return null;
                if (!Check(rt && rt.Race.state == 2, type + ": " + label + " in progress")) break;
                pilot.drive = true;
                float raceStart = Time.realtimeSinceStartup; string lastCounters = null; bool sentinel = false, shotClock = false, shotPlus = false, shotFinal = false;
                bool sawClock = false, sawPlus = false, sawFinal = false;

                if (action == "restart" && !restarted)
                {
                    yield return Seconds(6f);
                    pilot.Stop(); yield return UiShowcase.Frames(4);
                    yield return Tap(GamepadButton.Start); yield return Seconds(1.2f);
                    var p = rt.racePause;
                    if (Check(p && p.IsOpen && p.RowNames.Contains("pause-restart") && p.RowNames.Contains("pause-retire") && p.Spec.quitLabel == "QUIT WEEKEND", type + ": the stage's pause page: RESTART, RETIRE, QUIT WEEKEND (" + (p ? string.Join(",", p.RowNames) : "-") + ")"))
                    {
                        yield return Shot("champweekend-e2e-" + type.ToLowerInvariant() + "-pause");
                        for (int i = 0; i < 12 && PauseFocus(p) != "pause-restart"; i++) yield return Tap(GamepadButton.DpadDown);
                        if (!Check(PauseFocus(p) == "pause-restart", type + ": RESTART focused (" + PauseFocus(p) + ")")) break;
                        yield return Tap(GamepadButton.South); yield return UiShowcase.Frames(10);
                        yield return Tap(GamepadButton.DpadRight); yield return UiShowcase.Frames(5);
                        yield return Tap(GamepadButton.South);
                        Log(type + ": RESTART confirmed");
                        restarted = true; prevStage = stage; prevLabel = label;
                        yield return Seconds(2f);
                        continue;
                    }
                }
                restarted = false;
                if (action == "retire" || action == "quit")
                {
                    yield return Seconds(12f);
                    pilot.Stop(); yield return UiShowcase.Frames(4);
                    yield return Tap(GamepadButton.Start); yield return Seconds(1.2f);
                    var p = rt.racePause;
                    string row = action == "retire" ? "pause-retire" : "pause-quit";
                    if (Check(p && p.IsOpen && p.RowNames.Contains(row), type + ": the pause page has " + row))
                    {
                        for (int i = 0; i < 12 && PauseFocus(p) != row; i++) yield return Tap(GamepadButton.DpadDown);
                        if (!Check(PauseFocus(p) == row, type + ": " + row + " focused (" + PauseFocus(p) + ")")) break;
                        yield return Tap(GamepadButton.South); yield return UiShowcase.Frames(10);
                        yield return Shot("champweekend-e2e-" + type.ToLowerInvariant() + "-" + action + "-confirm");
                        yield return Tap(GamepadButton.DpadRight); yield return UiShowcase.Frames(5);
                        yield return Tap(GamepadButton.South);
                        Log(type + ": " + action.ToUpperInvariant() + " confirmed");
                    }
                    if (action == "quit")
                    {
                        for (float t = 0; t < 60f && !(OpenShell.Instance && OpenShell.Instance.Current == OpenShell.Screen.Setup); t += Time.unscaledDeltaTime) yield return null;
                        Check(OpenShell.Instance && OpenShell.Instance.Current == OpenShell.Screen.Setup && !RxChampWeekend.Active, type + ": QUIT WEEKEND: the event's setup page, no weekend left");
                        ended = true; break;
                    }
                }
                else
                {
                    // ---- the race: the lap counters every half second until the results page
                    float next = 0f, diagAt = 0f, outroAt = -1f;
                    for (float t = 0; t < (clock * 60f + 1200f) && !(rt.raceResults && rt.raceResults.Active) && !ChampWeekendFlow.CardUp(rt); t += Time.unscaledDeltaTime)
                    {
                        if (rt.Race.state >= 3 && Time.realtimeSinceStartup >= diagAt)
                        {   // the end of the race: where the results stand (every 5 s)
                            diagAt = Time.realtimeSinceStartup + 5f;
                            var ui = rt.EventUi;
                            Log(type + ": " + label + " end: race " + rt.Race.state + ", ui race " + ui.race_state + " post " + ui.post_state + " known " + (int)ui.known + " rules " + ui.rules_hold
                                + ", own results " + rt.OwnResultsPresentation + ", offline exhibition " + rt.OfflineExhibitionOpen + ", outro " + (rt.eventIntro && rt.eventIntro.OutroShown)
                                + ", results " + (rt.raceResults ? rt.raceResults.Active.ToString() : "none") + ", legacy post " + OriginalPostRace.Active + ", recorded " + RxChampWeekend.StageRaced(stage) + ", failure " + (rt.FailureMessage ?? "-"));
                        }
                        if (rt.eventIntro && rt.eventIntro.OutroShown)
                        {   // the finish scenario: skipped with A after a moment, as a player would
                            if (outroAt < 0) outroAt = Time.realtimeSinceStartup;
                            else if (Time.realtimeSinceStartup - outroAt > 4f) { outroAt = Time.realtimeSinceStartup + 6f; Log(type + ": A on the finish scenario (skip)"); yield return UiShowcaseNav.PadTap(GamepadButton.South); }
                        }
                        if (OriginalPostRace.Active) break;
                        if (Time.realtimeSinceStartup >= next)
                        {
                            next = Time.realtimeSinceStartup + 0.5f;
                            m.Fill(rt, 0.5f);
                            string counters = Counters(rt, m);
                            if (counters != lastCounters) { Log(type + ": " + label + " " + (Time.realtimeSinceStartup - raceStart).ToString("F1") + " s state " + rt.Race.state + " laps " + rt.Race.laps_completed + "/" + rt.Race.num_laps + ": " + counters); lastCounters = counters; }
                            if (counters.Contains("/96") || counters.Contains("/ 96") || counters.Contains("/97") || counters.Contains("96 LAPS") || counters.Contains("<size=40> / 96")) sentinel = true;
                            if (m.timed && !m.timedExpired) sawClock = true;
                            if (m.timed && m.timedExpired && m.TimedText(false).StartsWith("+")) sawPlus = true;
                            if (m.timed && m.timedExpired && m.TimedText(false) == "FINAL LAP") sawFinal = true;
                            if (!shotClock && m.timed && !m.timedExpired && Time.realtimeSinceStartup - raceStart > 8f) { shotClock = true; yield return Shot("champweekend-e2e-" + type.ToLowerInvariant() + "-" + stage + "-hud-clock"); }
                            if (!shotPlus && sawPlus) { shotPlus = true; yield return Shot("champweekend-e2e-" + type.ToLowerInvariant() + "-" + stage + "-hud-plus"); }
                            if (!shotFinal && sawFinal) { shotFinal = true; yield return Shot("champweekend-e2e-" + type.ToLowerInvariant() + "-" + stage + "-hud-final"); }
                        }
                        yield return null;
                    }
                    pilot.Stop();
                    Check(!sentinel, type + ": " + label + ": no lap counter showed the placeholder total");
                    if (RxChampWeekend.StageMinutes(stage) > 0) Check(sawClock && (stage == 0 || sawPlus || sawFinal), type + ": " + label + ": the counter showed the clock" + (stage == 0 ? "" : ", then the laps after it") + " (clock " + sawClock + ", +n " + sawPlus + ", final " + sawFinal + ")");
                    Check(RxChampWeekend.StageRaced(stage), type + ": " + label + " was recorded at the end of the race (before CONTINUE)");
                    if (OriginalPostRace.Active)
                    {   // the post-race page of a stage (the original's results table: the stages' Post never reaches the own results page, see the
                        // report); its RESULTS > menu > CONTINUE is the weekend card. No RESTART in a stage.
                        yield return Seconds(2f);
                        yield return Shot("champweekend-e2e-" + type.ToLowerInvariant() + "-" + stage + "-results");
                        yield return Tap(GamepadButton.South); yield return Seconds(1.5f);
                        yield return Shot("champweekend-e2e-" + type.ToLowerInvariant() + "-" + stage + "-results-menu");
                        Log(type + ": the post-race page's menu: CONTINUE");
                        yield return Tap(GamepadButton.South);
                    }
                    else
                    {
                        if (!Check(rt.raceResults && rt.raceResults.Active, type + ": " + label + ": the results page")) break;
                        yield return Seconds(2f);
                        var res = rt.raceResults;
                        string title = res.Root?.Q<Label>(className: "ou-header-title")?.text ?? "";
                        Check(title.StartsWith(label), type + ": the results page is the stage's (" + title + ")");
                        Check(res.Root.Q("results-again") == null, type + ": no RACE AGAIN in a stage");
                        yield return Shot("champweekend-e2e-" + type.ToLowerInvariant() + "-" + stage + "-results");
                        if (action == "back") { yield return Tap(GamepadButton.East); Log(type + ": B on the results"); }
                        else
                        {
                            for (int i = 0; i < 6 && FocusIn(res.Root) != "results-menu"; i++) yield return Tap(GamepadButton.DpadDown);
                            Check(FocusIn(res.Root) == "results-menu" && res.Root.Q("results-menu")?.Q<Label>()?.text == "CONTINUE WEEKEND", type + ": CONTINUE WEEKEND focused");
                            yield return Tap(GamepadButton.South);
                        }
                    }
                }

                // ---- the weekend card
                OwnChampWeekendCard card = null;
                for (float t = 0; t < 30f; t += Time.unscaledDeltaTime)
                {
                    card = rt ? rt.GetComponent<OwnChampWeekendCard>() : null;
                    if (card && card.Active && card.Page == "card") break;
                    yield return null;
                }
                if (!Check(card && card.Active && card.Page == "card", type + ": the weekend card after " + label)) break;
                yield return Seconds(1.5f);
                var v = card.View;
                Check(RxChampWeekend.StageRaced(stage), type + ": " + label + " is recorded");
                Check(v.last == stage || v.stages[Math.Max(0, v.last)].decided, type + ": the card's last stage is " + label + " (" + (v.LastStage?.label ?? "-") + ")");
                Check(RxChampWeekend.Finished || RxChampWeekend.Stage > stage, type + ": the weekend moved past " + label + " (next " + (RxChampWeekend.Finished ? "none: over" : RxChampWeekend.StageLabel(RxChampWeekend.Stage)) + ")");
                var me = v.Local;
                Log(type + ": card after " + label + ": you " + v.localRank + " of " + v.riders.Count + ", " + (me != null ? (v.judged ? me.total.ToString("0.00") : me.points + " pts") + ", " + label + " place " + me.place[stage] + ", status " + (me.status ?? "-") : "no local rider")
                    + ", next " + (v.NextStage?.label ?? "-") + " " + (v.NextStage?.format ?? "") + ", your gate " + v.nextGate + " of " + v.nextField + (v.eliminated ? ", ELIMINATED" : ""));
                Log(type + ": standings " + string.Join(", ", v.riders.Select(r => r.name + (r.local ? "*" : "") + " " + (v.judged ? r.total.ToString("0.0") : r.points.ToString()) + (r.status != null ? " " + r.status : ""))));
                yield return Shot("champweekend-e2e-" + type.ToLowerInvariant() + "-" + stage + "-card");
                prevStage = stage; prevLabel = label; raced++;
                Check(FocusIn(card.Root) == (v.next < 0 ? "cw-finish" : "cw-next"), type + ": the card's first action is focused (" + FocusIn(card.Root) + ")");
                yield return Tap(GamepadButton.South);
                if (v.next < 0)
                {
                    Log(type + ": FINISH WEEKEND");
                    for (float t = 0; t < 60f && !(OpenShell.Instance && OpenShell.Instance.Current == OpenShell.Screen.Setup); t += Time.unscaledDeltaTime) yield return null;
                    yield return Seconds(2f);
                    Check(OpenShell.Instance && OpenShell.Instance.Current == OpenShell.Screen.Setup && !RxChampWeekend.Active, type + ": after the weekend: the event's setup page, no weekend left");
                    yield return Shot("champweekend-e2e-" + type.ToLowerInvariant() + "-9-back-in-menu");
                    ended = true;
                }
                else
                {
                    yield return UiShowcase.Frames(10);
                    Check(OpenLoading.Current != null, type + ": the loading screen covers the next stage's load");
                    yield return Shot("champweekend-e2e-" + type.ToLowerInvariant() + "-" + stage + "-loading");
                }
            }
            Check(ended, type + ": the weekend ended in the flow (" + raced + " stages)");
            UnityEngine.Object.Destroy(pilotGo);
            if (UnityEngine.Object.FindAnyObjectByType<OriginalWorldRuntime>()) { OriginalPauseMenu.QuitToFrontEnd(); yield return Seconds(5f); }
            for (float t = 0; t < 30f && !OpenShell.Instance; t += Time.unscaledDeltaTime) yield return null;
            yield return Seconds(2f);
        }
    }
}

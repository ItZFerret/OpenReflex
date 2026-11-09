// TEST ONLY: the audit's P2 front-end pages (w-p2front) in the showcase, each from the product's own code and the install's tables / strings:
//   p2-quit              hub QUIT asks first (QuitConfirmationPopup: YES focused, NO, Back = NO), the quit itself replaced by a probe
//   p2-career            the career flow with the real core catalog: the CAREER WELCOME page (difficulty), the hub with the player's CREDITS, a LOCKED event's
//                        popup, an INVALID VEHICLE event's popup (both with the pad), the VEHICLE LIST swap on the unlocked profile
//   p2-completion        the CAREER COMPLETION page: Videos\CareerCompletion.bik then Videos\Credits.bik (ffmpeg), skipped with Back
//   p2-rules             the arcade RULES popup before Freestyle / Section Race events (do-not-show-again kept in the profile) and the RULES chip
//   p2-challenges        the Race & Ride CHALLENGES page of each Free Ride location with seeded medals, then the pad journey from the hub (FREE RIDE > CHALLENGES,
//                        START of a challenge, the Free Ride, the locked popup)
//   p2-motocard-welcome  the first-run MOTOCARD welcome and its SCREEN ADJUSTMENT page; p2-screen-adjust the same page from OPTIONS > VIDEO
//   p2-reel              the results page with HIGHLIGHT REEL and the reel page (only the legend)
// Run: run.py <Showcase.app> --pages p2-quit,p2-career,... [--walk]
using System;
using System.Collections;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using Reflex.Port.Content;
using Reflex.Port.FrontEnd;
using Reflex.Port.Hud;
using Reflex.Port.OpenFrontEnd;
using Reflex.Port.Options;
using UnityEngine;
using UnityEngine.InputSystem.LowLevel;
using UnityEngine.UIElements;

namespace Reflex.Port.Showcase
{
    static class UiShowcaseP2Front
    {
        const string Tag = "[ui-showcase] [p2front] ";
        const BindingFlags All = BindingFlags.Instance | BindingFlags.NonPublic | BindingFlags.Public;

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        static void Register()
        {
            UiShowcase.Register("p2-quit", Quit);
            UiShowcase.Register("p2-rules", Rules);
            UiShowcase.Register("p2-challenges", ChallengePages);
            UiShowcase.Register("p2-challenges-nav", ChallengeJourney);
            UiShowcase.Register("p2-motocard-welcome", MotocardWelcome);
            UiShowcase.Register("p2-screen-adjust", ScreenAdjust);
            UiShowcase.Register("p2-reel", Reel);
            UiShowcase.Register("p2-completion", Completion);
            UiShowcase.Register("p2-career", Career);   // (last: it switches the profile to the unlocked clone and back)
        }

        // ------------------------------------------------------------------------------------------------ helpers

        static void Open()
        {
            ReflexProfile.Disabled = false;
            if (!CoreGarageProfile.Open()) throw new InvalidOperationException(CoreGarageProfile.Error);
            OpenMenuWorld.Still = true; FeGarage.RiderPlays = false;
            ReflexProfile.Open();
        }

        static IEnumerator HubReady()
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
            yield return new WaitForSecondsRealtime(1.2f);
        }

        static readonly HashSet<string> StringColumns = new HashSet<string> { "EventId" };
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
            if (!(bool)change.Invoke(null, new object[] { edit })) throw new InvalidOperationException("seeding the profile: " + CoreGarageProfile.Error);
            ReflexProfile.Open();
        }
        static string Focused() => OpenShell.Instance.Nav.Focused?.name;
        static VisualElement Q(string name) => OpenShell.Instance.Layer.Q<VisualElement>(name);

        static IEnumerator Tap(GamepadButton b, int wait = 6) { yield return UiShowcaseNav.PadTap(b); yield return UiShowcase.Frames(wait); }
        static IEnumerator FocusTo(string name, string screen, int max = 12)
        {
            var shell = OpenShell.Instance;
            var target = shell.Layer.Q<VisualElement>(name);
            if (target != null) { shell.Nav.Focus(target); yield return UiShowcase.Frames(6); }
            UiShowcaseNav.Check(Focused() == name, screen, "could not focus " + name + " (on " + Focused() + ")");
        }

        // ------------------------------------------------------------------------------------------------ p2-quit

        static IEnumerator Quit()
        {
            Open();
            var shell = OpenShell.Instance;
            int quits = 0; OpenShell.QuitOverride = () => quits++;
            yield return HubReady();
            yield return FocusTo("ou-hub-quit", "quit");
            yield return Tap(GamepadButton.South, 10);
            UiShowcaseNav.Check(shell.PopupOpen, "quit", "A on QUIT did not ask first");
            UiShowcaseNav.Check(Focused() == "popup-yes", "quit", "YES should be focused as in QuitConfirmationPopup (" + Focused() + ")");
            Debug.Log(Tag + "quit popup: " + shell.Popup?.Q<Label>(className: "ou-dialog-title")?.text + " | " + shell.Popup?.Q<Label>("popup-quit-text")?.text);
            yield return UiShowcase.Frames(30);
            yield return UiShowcase.Shot("p2-quit");
            // B = NO: closed, nothing quit, the QUIT chip focused again
            yield return Tap(GamepadButton.East, 10);
            UiShowcaseNav.Check(!shell.PopupOpen && quits == 0, "quit", "B did not answer NO (open " + shell.PopupOpen + ", quits " + quits + ")");
            UiShowcaseNav.Check(Focused() == "ou-hub-quit", "quit", "the QUIT chip did not get the focus back (" + Focused() + ")");
            // right to NO, A: closed, nothing quit
            yield return Tap(GamepadButton.South, 10);
            yield return Tap(GamepadButton.DpadRight);
            UiShowcaseNav.Check(Focused() == "popup-no", "quit", "right from YES did not reach NO (" + Focused() + ")");
            yield return Tap(GamepadButton.South, 10);
            UiShowcaseNav.Check(!shell.PopupOpen && quits == 0, "quit", "NO quit anyway");
            // YES quits
            yield return Tap(GamepadButton.South, 10);
            yield return Tap(GamepadButton.South, 10);
            UiShowcaseNav.Check(quits == 1, "quit", "YES did not quit (" + quits + ")");
            OpenShell.QuitOverride = null;
            shell.ShowMain(); yield return UiShowcase.Frames(5);
        }

        // ------------------------------------------------------------------------------------------------ p2-career

        static IEnumerator WaitHub(float seconds = 240f)
        {
            var shell = OpenShell.Instance;
            float t0 = Time.realtimeSinceStartup;
            while (Time.realtimeSinceStartup - t0 < seconds && shell.Layer.Q<VisualElement>(className: "oc-poster") == null) yield return null;
            Debug.Log(Tag + "the career hub came after " + (Time.realtimeSinceStartup - t0).ToString("F1") + " s");
            yield return UiShowcase.Frames(40);
        }

        static List<VisualElement> EventRows() => OpenShell.Instance.Layer.Query<VisualElement>(className: "oc-event").ToList();
        static VisualElement RowWithTag(string text) => EventRows().FirstOrDefault(r => r.Query<Label>(className: "oc-tag").ToList().Any(l => l.text == text));

        static IEnumerator Career()
        {
            Open();
            var shell = OpenShell.Instance;
            // the profile has not seen the welcome page
            UiShowcaseNav.Check(OpenShell.CareerWelcomeDue(), "career", "the fresh profile should be due the welcome page");
            Change(p =>
            {   // credits for the hub's plate
                var rider = p.T("PlayerRider"); rider.columns["AccountBalance"][0] = 48250.0;
            });
            shell.ShowCoreCareer();
            yield return UiShowcase.Frames(40);
            UiShowcaseNav.Check(Q("career-welcome") != null, "career", "the CAREER WELCOME page did not open (" + shell.Current + ")");
            Debug.Log(Tag + "welcome: " + Q("career-welcome")?.Q<Label>(className: "oc-panel-title")?.text + " | focus " + Focused());
            UiShowcaseNav.Check(shell.Nav.DefaultItem?.Invoke()?.name == "career-welcome-difficulty", "career", "the difficulty list should be the page's default item (" + shell.Nav.DefaultItem?.Invoke()?.name + ")");
            shell.Nav.Focus(Q("career-welcome-difficulty")); yield return UiShowcase.Frames(6);
            yield return UiShowcase.Frames(20);
            yield return UiShowcase.Shot("p2-career-welcome");
            shell.Nav.Focus(Q("career-welcome-difficulty")); yield return UiShowcase.Frames(6);
            string labelBefore = Q("career-welcome-difficulty")?.Q<Label>(className: "mxa-value")?.text;
            yield return Tap(GamepadButton.DpadRight);
            string label = Q("career-welcome-difficulty")?.Q<Label>(className: "mxa-value")?.text;
            Debug.Log(Tag + "difficulty " + labelBefore + " -> " + label);
            UiShowcaseNav.Check(label != labelBefore, "career", "right on the difficulty list did not change it (" + labelBefore + ")");
            yield return Tap(GamepadButton.DpadDown);
            UiShowcaseNav.Check(Focused() == "career-welcome-accept", "career", "down from the difficulty did not reach CONTINUE (" + Focused() + ")");
            yield return Tap(GamepadButton.DpadUp);
            yield return Tap(GamepadButton.South, 10);   // A on the list accepts (CareerWelcome:OnAcceptKeypress)
            int after = Reflex.Port.Options.ReflexSettings.CareerDifficulty;
            string[] levels = { "ROOKIE", "AMATEUR", "PRO", "ALL TIME" };
            UiShowcaseNav.Check(after == OpenShell.CareerWelcomeDifficulties[Array.IndexOf(levels, label)], "career", "accepting did not store the difficulty shown (" + label + " -> " + after + ")");
            UiShowcaseNav.Check(CoreGarageProfile.EventStatus("CareerWelcome") == 1, "career", "accepting did not set the CareerWelcome flag");
            yield return WaitHub();
            UiShowcaseNav.Check(shell.Layer.Q<VisualElement>(className: "oc-poster") != null, "career", "the Race & Ride page did not follow the welcome page");
            var credits = shell.Layer.Q<Label>(className: "p2-credits-value");
            Debug.Log(Tag + "credits plate: " + credits?.text);
            UiShowcaseNav.Check(credits != null && credits.text.Contains("48,250") || credits != null && credits.text.Contains("48250"), "career", "the credits are not in the hub (" + credits?.text + ")");
            // the welcome page does not come twice
            UiShowcaseNav.Check(!OpenShell.CareerWelcomeDue(), "career", "the welcome page would come twice");
            yield return UiShowcase.Shot("p2-career-hub-credits");

            // a LOCKED event: A shows the LockedPopup, B closes it
            var locked = RowWithTag("LOCKED");
            UiShowcaseNav.Check(locked != null, "career", "the earned profile has no locked event row");
            if (locked != null)
            {
                shell.Nav.Focus(locked); yield return UiShowcase.Frames(30);
                Debug.Log(Tag + "before A: focus " + Focused() + " on " + locked.name);
                yield return Tap(GamepadButton.South, 12);
                Debug.Log(Tag + "after A: focus " + Focused() + ", popup " + shell.PopupOpen + ", screen " + shell.Current);
                if (!shell.PopupOpen) { ((OuNavItem)locked.userData).submit?.Invoke(); yield return UiShowcase.Frames(10); Debug.Log(Tag + "direct submit: popup " + shell.PopupOpen); }
                UiShowcaseNav.Check(shell.PopupOpen && Q("popup-locked") != null, "career", "A on a locked event did not show the LockedPopup");
                Debug.Log(Tag + "locked: " + Q("popup-locked")?.Q<Label>(className: "ou-dialog-title")?.text + " | " + Q("popup-locked-text")?.Text());
                yield return UiShowcase.Frames(30);
                yield return UiShowcase.Shot("p2-career-locked");
                yield return Tap(GamepadButton.East, 10);
                UiShowcaseNav.Check(!shell.PopupOpen, "career", "B did not close the LockedPopup");
                UiShowcaseNav.Check(Focused() == locked.name, "career", "the event did not get the focus back (" + Focused() + ")");
            }
            // a locked tier: A on its poster
            shell.Nav.Focus(shell.Layer.Query<VisualElement>(className: "oc-poster").ToList().Last()); yield return UiShowcase.Frames(20);
            var lastPoster = Focused();
            yield return Tap(GamepadButton.South, 12);
            Debug.Log(Tag + "last poster " + lastPoster + ": popup " + shell.PopupOpen);
            if (shell.PopupOpen) { yield return Tap(GamepadButton.East, 10); }

            // an event for another vehicle: INVALID VEHICLE
            var invalid = RowWithTag("INVALID VEHICLE");
            Debug.Log(Tag + "invalid vehicle row: " + (invalid?.name ?? "none"));
            if (invalid != null)
            {
                shell.Nav.Focus(invalid); yield return UiShowcase.Frames(30);
                yield return Tap(GamepadButton.South, 12);
                UiShowcaseNav.Check(shell.PopupOpen && Q("popup-invalid-vehicle") != null, "career", "A on an event for another vehicle did not show InvalidRaceRequirements");
                Debug.Log(Tag + "invalid: " + Q("popup-invalid-vehicle")?.Q<Label>(className: "ou-dialog-title")?.text + " | " + Q("popup-invalid-vehicle-text")?.Text());
                yield return UiShowcase.Frames(30);
                yield return UiShowcase.Shot("p2-career-invalid-vehicle");
                yield return Tap(GamepadButton.South, 10);   // CONTINUE
                UiShowcaseNav.Check(!shell.PopupOpen, "career", "CONTINUE did not close the popup");
            }

            // the quick vehicle swap, on the unlocked profile (every vehicle owned)
            if (!CoreGarageProfile.UnlockEverythingEnabled && !CoreGarageProfile.SetUnlockEverything(true)) throw new InvalidOperationException(CoreGarageProfile.Error);
            ReflexProfile.Open();
            shell.ShowCoreCareer();
            yield return WaitHub();
            var target = RowWithTag("INVALID VEHICLE");
            Debug.Log(Tag + "unlocked hub tags: " + string.Join(", ", EventRows().SelectMany(r => r.Query<Label>(className: "oc-tag").ToList().Select(l => l.text)).GroupBy(x => x).Select(g => g.Key + " x" + g.Count())) + " | ride plate " + shell.Layer.Q<Label>(className: "oc-ride-name")?.text);
            UiShowcaseNav.Check(target != null, "career", "the unlocked profile has no event for another vehicle");
            if (target != null)
            {
                shell.Nav.Focus(target); yield return UiShowcase.Frames(30);
                var legend = shell.Layer.Q<VisualElement>("career-legend-vehicles");
                UiShowcaseNav.Check(legend != null && legend.resolvedStyle.display != DisplayStyle.None, "career", "the VEHICLE LIST legend should show for an event the player owns a vehicle for");
                string seriesRow = target.name;
                // A on it: InvalidRaceRequirements
                yield return Tap(GamepadButton.South, 12);
                UiShowcaseNav.Check(shell.PopupOpen && Q("popup-invalid-vehicle") != null, "career", "A on an event for another vehicle did not show InvalidRaceRequirements");
                Debug.Log(Tag + "invalid: " + Q("popup-invalid-vehicle")?.Q<Label>(className: "ou-dialog-title")?.text + " | " + Q("popup-invalid-vehicle-text")?.Text());
                yield return UiShowcase.Frames(30);
                yield return UiShowcase.Shot("p2-career-invalid-vehicle");
                yield return Tap(GamepadButton.South, 10);   // CONTINUE
                UiShowcaseNav.Check(!shell.PopupOpen && Focused() == seriesRow, "career", "CONTINUE did not close the popup / return the focus (" + Focused() + ")");
                yield return UiShowcaseNav.PadTap(GamepadButton.North); yield return UiShowcase.Frames(20);   // Y: VehiclesQuickViewPopup
                UiShowcaseNav.Check(shell.PopupOpen && Q("popup-vehicle-list") != null, "career", "Y on the event did not open the vehicle list");
                var rows = shell.Layer.Query<VisualElement>(className: "p2-qv-row").ToList();
                Debug.Log(Tag + "vehicle list: " + rows.Count + " vehicles, focus " + Focused());
                yield return UiShowcase.Frames(40);
                yield return UiShowcase.Shot("p2-career-vehicle-list");
                UiShowcaseNav.Check(rows.Count > 0, "career", "the vehicle list is empty");
                if (rows.Count > 0)
                {
                    yield return Tap(GamepadButton.South, 6);   // pick the focused vehicle (the equipped one when it is of the class, else the first)
                    for (int i = 0; i < 4 && shell.Layer.Q<VisualElement>(className: "oc-poster") == null; i++) yield return WaitHub(30f);
                    yield return WaitHub();
                    var again = shell.Layer.Q<VisualElement>(seriesRow);
                    var tags = again?.Query<Label>(className: "oc-tag").ToList().Select(l => l.text).ToList() ?? new List<string>();
                    Debug.Log(Tag + "after the swap " + seriesRow + ": tags " + string.Join(",", tags) + ", focus " + Focused());
                    UiShowcaseNav.Check(again != null && !tags.Contains("INVALID VEHICLE"), "career", "the event is still for another vehicle after the swap (" + string.Join(",", tags) + ")");
                    UiShowcaseNav.Check(Focused() == seriesRow, "career", "the hub did not put the focus back on the event (" + Focused() + ")");
                }
            }
            if (CoreGarageProfile.UnlockEverythingEnabled) CoreGarageProfile.SetUnlockEverything(false);
            ReflexProfile.Open();
            shell.ShowMain(); yield return UiShowcase.Frames(5);
        }

        // ------------------------------------------------------------------------------------------------ p2-completion

        static IEnumerator Completion()
        {
            Open();
            var shell = OpenShell.Instance;
            bool done = false;
            shell.ShowCareerCompletion(() => done = true);
            for (float t = 0; t < 25f && !(shell.CareerVideo != null && shell.CareerVideo.HasFrame); t += Time.unscaledDeltaTime) yield return null;
            var video = shell.CareerVideo;
            UiShowcaseNav.Check(video != null && video.HasFrame, "completion", "the completion video shows no picture (" + video?.Error + ")");
            Debug.Log(Tag + "completion video: " + video?.video + " " + video?.Info.width + "x" + video?.Info.height + ", " + video?.Info.frames + " frames");
            yield return new WaitForSecondsRealtime(2.5f);
            yield return UiShowcase.Shot("p2-completion-video");
            yield return Tap(GamepadButton.East, 10);   // skip: the credits video
            for (float t = 0; t < 25f && !(shell.CareerVideo != null && shell.CareerVideo.video.Contains("Credits") && shell.CareerVideo.HasFrame); t += Time.unscaledDeltaTime) yield return null;
            UiShowcaseNav.Check(shell.CareerVideo != null && shell.CareerVideo.video.Contains("Credits"), "completion", "skipping did not go on to the credits video");
            yield return new WaitForSecondsRealtime(1.5f);
            yield return UiShowcase.Shot("p2-completion-credits");
            yield return Tap(GamepadButton.East, 10);   // skip: back to the page that follows
            UiShowcaseNav.Check(done && shell.CareerVideo == null, "completion", "the page did not finish after the credits video (done " + done + ")");
            shell.ShowMain(); yield return UiShowcase.Frames(5);
        }

        // ------------------------------------------------------------------------------------------------ p2-rules

        static IEnumerator ToSetup(string type)
        {
            var shell = OpenShell.Instance;
            var ev = OpenContent.Events.FirstOrDefault(e => e.ev.spec.type == type && !e.freeRide);
            if (ev == null) { Debug.LogWarning(Tag + "no " + type + " event in this install"); yield break; }
            shell.ShowMain(); yield return UiShowcase.Frames(8);
            shell.ShowBrowser(false);
            Set(shell, "_browseEvent", ev); Set(shell, "_setup", OpenHistory.SetupOf(ev.key));
            shell.ShowSetupPage();
            yield return UiShowcase.Frames(40);
        }
        static void Set(object o, string field, object value) { var f = o.GetType().GetField(field, All); if (f == null) throw new MissingFieldException(o.GetType().Name, field); f.SetValue(o, value); }

        static IEnumerator Rules()
        {
            Open();
            var shell = OpenShell.Instance;
            var launched = new List<string>(); OpenSetup.LaunchOverride = s => launched.Add(s.eventKey);
            // RulesPopup:Start's narration (App.PlayUILocalizedSoundEvent( "AudioLocalizedActivity", "Audio_Loc_<ACTIVITY>", "<ACTIVITY>/<ACTIVITY>/RULE" )), stopped at End
            int Voice(string s) { lock (FeSounds.Log) return FeSounds.Log.Count(l => l.Contains(s)); }
            foreach (var (type, flag, package, path) in new[] { ("Freestyle", "Rule_Freestyle", "Audio_Loc_FREESTYLE", "FREESTYLE/FREESTYLE/RULE"), ("SectionRace", "Rule_SectionRace", "Audio_Loc_SECTION_RACE", "SECTIONRACE/SECTIONRACE/RULE") })
            {
                yield return ToSetup(type);
                if (shell.Current != OpenShell.Screen.Setup) continue;
                UiShowcaseNav.Check(CoreGarageProfile.EventStatus(flag) == 0, "rules", flag + " should start unset");
                UiShowcaseNav.Check(Q("ou-setup-rules") != null, "rules", "the " + type + " setup page has no RULES chip");
                // START EVENT: the rules page first
                int base0 = launched.Count, voiceBase = Voice("vo rules-popup: AudioLocalizedActivity/" + package + " " + path + " -> "), stopBase = Voice("vo stop rules-popup");
                shell.Nav.Focus(shell.StartButton); yield return UiShowcase.Frames(6);
                yield return Tap(GamepadButton.South, 12);
                UiShowcaseNav.Check(shell.PopupOpen && Q("popup-rules") != null && launched.Count == base0, "rules", type + ": START EVENT did not show the rules first (launched " + (launched.Count - base0) + ")");
                Debug.Log(Tag + type + " rules: " + Q("popup-rules")?.Q<Label>(className: "ou-dialog-title")?.text + " | " + Q("popup-rules")?.Q<Label>(className: "p2-popup-format")?.text + " | " + Q("popup-rules-text")?.Text()?.Length + " chars");
                yield return UiShowcase.Frames(30);
                string voice = "vo rules-popup: AudioLocalizedActivity/" + package + " " + path + " -> ";
                UiShowcaseNav.Check(Voice(voice) == voiceBase + 1, "rules", type + ": the rules popup did not ask for its narration (" + voice + ")");
                UiShowcaseNav.Check(Voice("(no such package)") == 0, "rules", type + ": the narration did not resolve to an event");
                Debug.Log(Tag + type + " narration: " + FeSounds.Log.LastOrDefault(l => l.StartsWith("vo rules-popup")));
                {   // EFFECTS VOLUME reaches the narration: the VoiceOver channel's gain follows it (the options page moves the shell's sounds too)
                    var player = shell.Sounds.Player;
                    Reflex.Port.Options.ReflexClientLive.EffectsVolume(1f); yield return new WaitForSecondsRealtime(0.4f); float full = player.ChannelGain(ReflexChannel.VoiceOver);   // (the gains refresh four times a second)
                    Reflex.Port.Options.ReflexClientLive.EffectsVolume(0.4f); yield return new WaitForSecondsRealtime(0.4f); float low = player.ChannelGain(ReflexChannel.VoiceOver);
                    Debug.Log(Tag + type + " VoiceOver gain at EFFECTS VOLUME 1.0 " + full.ToString("F3") + ", at 0.4 " + low.ToString("F3"));
                    UiShowcaseNav.Check(full > 0.01f && Mathf.Abs(low / full - 0.4f) < 0.02f, "rules", type + ": the narration's gain does not follow EFFECTS VOLUME (" + full + " -> " + low + ")");
                    Reflex.Port.Options.ReflexClientLive.EffectsVolume(1f);
                }
                yield return UiShowcase.Shot("p2-rules-" + type.ToLowerInvariant());
                yield return Tap(GamepadButton.East, 8);   // Back does nothing (RulesPopup:OnBackKeypress: disabled)
                UiShowcaseNav.Check(shell.PopupOpen, "rules", "Back closed the rules page (the original's is disabled)");
                // CONTINUE, DO NOT SHOW AGAIN: starts, and the flag stays
                yield return Tap(GamepadButton.DpadDown);
                UiShowcaseNav.Check(Focused() == "popup-rules-skip", "rules", "down did not reach CONTINUE, DO NOT SHOW AGAIN (" + Focused() + ")");
                yield return Tap(GamepadButton.South, 12);
                UiShowcaseNav.Check(launched.Count == base0 + 1, "rules", type + ": the event did not start after the rules (" + (launched.Count - base0) + ")");
                UiShowcaseNav.Check(Voice("vo stop rules-popup") == stopBase + 1, "rules", type + ": the narration was not stopped when the rules closed");
                UiShowcaseNav.Check(CoreGarageProfile.EventStatus(flag) == 1, "rules", flag + " was not kept");
                // again: no rules
                yield return ToSetup(type);
                shell.Nav.Focus(shell.StartButton); yield return UiShowcase.Frames(6);
                yield return Tap(GamepadButton.South, 12);
                UiShowcaseNav.Check(!shell.PopupOpen && launched.Count == base0 + 2, "rules", type + ": the rules came again after DO NOT SHOW AGAIN");
                // the RULES chip shows them whenever (Back closes this one)
                yield return ToSetup(type);
                yield return FocusTo("ou-setup-rules", "rules");
                yield return Tap(GamepadButton.South, 12);
                UiShowcaseNav.Check(shell.PopupOpen && Q("popup-rules") != null, "rules", "the RULES chip did not open the rules");
                UiShowcaseNav.Check(Voice("vo rules-popup: AudioLocalizedActivity/" + package + " " + path + " -> ") == voiceBase + 2, "rules", type + ": the RULES chip's page did not ask for its narration");
                var buttons = shell.Layer.Query<VisualElement>(className: "p2-popup-button").ToList();
                UiShowcaseNav.Check(buttons.Count == 1, "rules", "the viewed rules should have CONTINUE only (" + buttons.Count + ")");
                yield return Tap(GamepadButton.East, 10);
                UiShowcaseNav.Check(!shell.PopupOpen && Focused() == "ou-setup-rules", "rules", "Back did not close the viewed rules (" + shell.PopupOpen + ", " + Focused() + ")");
                UiShowcaseNav.Check(Voice("vo stop rules-popup") == stopBase + 2, "rules", type + ": the narration was not stopped when the viewed rules closed");
            }
            // an event type without rules has no chip and starts at once
            yield return ToSetup("National");
            if (shell.Current == OpenShell.Screen.Setup)
            {
                UiShowcaseNav.Check(Q("ou-setup-rules") == null, "rules", "a National has no rules chip in the original");
                int n = launched.Count;
                shell.Nav.Focus(shell.StartButton); yield return UiShowcase.Frames(6);
                yield return Tap(GamepadButton.South, 12);
                UiShowcaseNav.Check(launched.Count == n + 1 && !shell.PopupOpen, "rules", "a National should start without rules");
            }
            OpenSetup.LaunchOverride = null;
            shell.ShowMain(); yield return UiShowcase.Frames(5);
        }

        // ------------------------------------------------------------------------------------------------ p2-challenges

        static void SeedChallenges()
        {
            var locs = OpenChallengeData.Read(ReflexProfile.Current, false);
            if (locs.Count == 0) throw new InvalidOperationException("no Free Ride challenge series");
            if (ReflexProfile.Current != null && ReflexProfile.Current.T("PlayerChallenges").rows > 0) return;
            if (ReflexProfile.Current == null || !ReflexProfile.Current.tables.ContainsKey("PlayerChallenges")) { Debug.Log(Tag + "this profile has no PlayerChallenges table: no medals seeded"); return; }
            Change(p =>
            {
                var t = p.T("PlayerChallenges");
                int i = 0;
                foreach (var l in locs)
                    foreach (var c in l.challenges)
                    {
                        i++;
                        if (i % 5 == 0) continue;   // some without a record
                        int medal = i % 4;   // 0 none-but-tried, 1 bronze, 2 silver, 3 gold
                        double param = c.kind == 1 ? 40 + i * 9 : c.kind == 6 ? 120.0 - i * 3 : c.kind == 8 ? 3 + i % 6 : 2 + i;
                        AddRow(p, "PlayerChallenges", ("EventTypeLocationId", c.eventTypeLocationId), ("Bronze", medal >= 1 ? 1 : 0), ("Silver", medal >= 2 ? 1 : 0), ("Gold", medal >= 3 ? 1 : 0), ("Param", param));
                    }
            });
        }

        static IEnumerator ChallengePages()
        {
            Open(); SeedChallenges();
            var shell = OpenShell.Instance;
            shell.ShowChallenges();
            yield return UiShowcase.Frames(50);
            UiShowcaseNav.Check(shell.ChallengesOpen, "challenges", "the page did not open");
            var cards = shell.Layer.Query<VisualElement>(className: "p2-ch-card").ToList();
            Debug.Log(Tag + "challenges page: " + cards.Count + " locations, focus " + Focused());
            UiShowcaseNav.Check(cards.Count >= 1, "challenges", "no Free Ride location listed");
            for (int i = 0; i < cards.Count; i++)
            {
                shell.Nav.Focus(cards[i]); yield return UiShowcase.Frames(36);
                var panel = shell.Layer.Query<VisualElement>(className: "p2-ch-panel").ToList().FirstOrDefault(p => !p.ClassListContains("p2-ch-off"));
                var rows = panel?.Query<VisualElement>(className: "p2-ch-row").ToList() ?? new List<VisualElement>();
                var tiles = panel?.Query<Label>(className: "p2-ch-tier-value").ToList().Select(l => l.text) ?? Enumerable.Empty<string>();
                Debug.Log(Tag + "location " + cards[i].name + ": " + rows.Count + " challenges; thresholds " + string.Join(" | ", tiles));
                UiShowcaseNav.Check(rows.Count >= 3, "challenges", "location " + i + " lists " + rows.Count + " challenges");
                if (i == 0) yield return UiShowcase.Shot("p2-challenges");
                else yield return UiShowcase.Shot("p2-challenges-" + i);
            }
            shell.ShowMain(); yield return UiShowcase.Frames(5);
        }

        static IEnumerator ChallengeJourney()
        {
            Open(); SeedChallenges();
            var shell = OpenShell.Instance;
            var started = new List<string>(); OpenShell.ChallengeLaunchOverride = ev => started.Add(ev.key);
            yield return HubReady();
            // FREE RIDE (hub) > CHALLENGES (the browser's chip)
            shell.ShowPlay(); yield return UiShowcase.Frames(10);   // (FREE RIDE is on the PLAY page)
            yield return FocusTo("ou-hub-freeride", "challenges nav");
            yield return Tap(GamepadButton.South, 40);
            UiShowcaseNav.Check(shell.Current == OpenShell.Screen.Browser && shell.FreeRide, "challenges nav", "FREE RIDE did not open the browser (" + shell.Current + ")");
            var chip = Q("ou-challenges");
            UiShowcaseNav.Check(chip != null, "challenges nav", "the Free Ride browser has no CHALLENGES chip");
            if (chip != null)
            {
                shell.Nav.Focus(chip); yield return UiShowcase.Frames(30);
                yield return UiShowcase.Shot("p2-browser-challenges-chip");
                shell.Nav.Focus(Q("ou-challenges")); yield return UiShowcase.Frames(8);   // (the walk of the page moved the focus)
                yield return Tap(GamepadButton.South, 40);
                UiShowcaseNav.Check(shell.ChallengesOpen, "challenges nav", "the CHALLENGES chip did not open the page");
            }
            if (shell.ChallengesOpen)
            {
                string first = Focused();
                UiShowcaseNav.Check(first != null && first.StartsWith("challenge-location-"), "challenges nav", "the page should open on a location (" + first + ")");
                // RB: the next location; LB: back
                yield return Tap(GamepadButton.RightShoulder, 12);
                string second = Focused();
                UiShowcaseNav.Check(second != first && second != null && second.StartsWith("challenge-location-"), "challenges nav", "RB did not change the location (" + first + " -> " + second + ")");
                yield return Tap(GamepadButton.LeftShoulder, 12);
                UiShowcaseNav.Check(Focused() == first, "challenges nav", "LB did not come back (" + Focused() + ")");
                // right: into the challenges; down walks them; left comes back to the location; B from a row goes to the location, B again to the browser
                yield return Tap(GamepadButton.DpadRight);
                string row = Focused();
                UiShowcaseNav.Check(row != null && row.StartsWith("challenge-") && !row.StartsWith("challenge-location-"), "challenges nav", "right did not reach the challenges (" + row + ")");
                yield return Tap(GamepadButton.DpadDown);
                UiShowcaseNav.Check(Focused() != row, "challenges nav", "down did not walk the challenges");
                yield return Tap(GamepadButton.East, 8);
                UiShowcaseNav.Check(Focused() == first, "challenges nav", "B from a challenge did not return to its location (" + Focused() + ")");
                // A on the first challenge of the location (Hit Discovery: the series' Free Ride, whose rules page comes first, once)
                yield return Tap(GamepadButton.DpadRight);
                string firstRow = Focused();
                Debug.Log(Tag + "starting " + firstRow);
                yield return Tap(GamepadButton.South, 14);
                if (shell.PopupOpen && Q("popup-rules") != null)
                {
                    Debug.Log(Tag + "the Free Ride rules came first: " + Q("popup-rules")?.Q<Label>(className: "ou-dialog-title")?.text);
                    yield return UiShowcase.Frames(30);
                    yield return UiShowcase.Shot("p2-challenges-rules");
                    yield return Tap(GamepadButton.South, 14);   // CONTINUE
                }
                Debug.Log(Tag + "started: " + string.Join(", ", started));
                UiShowcaseNav.Check(started.Count == 1 && started[0].Contains("FR_"), "challenges nav", "A on a challenge did not start it (" + string.Join(",", started) + ")");
                // the page again (what the front end shows coming back from the race), a Hill Climb row: its own world
                shell.ShowChallenges(); yield return UiShowcase.Frames(40);
                var hillRow = shell.Layer.Query<VisualElement>().Where(e => e.name != null && e.name.StartsWith("challenge-HillClimb-")).ToList().FirstOrDefault(e => e.panel != null && e.resolvedStyle.display != DisplayStyle.None && e.enabledInHierarchy);
                UiShowcaseNav.Check(hillRow != null, "challenges nav", "no Hill Climb row");
                if (hillRow != null)
                {
                    shell.Nav.Focus(hillRow); yield return UiShowcase.Frames(10);
                    yield return Tap(GamepadButton.South, 14);
                    UiShowcaseNav.Check(started.Count == 2 && started[1].StartsWith("HillClimb/FR_"), "challenges nav", "START on Hill Climb did not start its world (" + string.Join(",", started) + ")");
                }
                // a locked location: A on its card answers with the LockedPopup and starts nothing
                shell.ShowChallenges(); yield return UiShowcase.Frames(40);
                var lockedCard = shell.Layer.Query<VisualElement>(className: "p2-ch-card").ToList().FirstOrDefault(c => c.Q<Label>(className: "oc-tag")?.text == "LOCKED");
                UiShowcaseNav.Check(lockedCard != null, "challenges nav", "the earned profile has no locked Free Ride location");
                if (lockedCard != null)
                {
                    int n = started.Count;
                    shell.Nav.Focus(lockedCard); yield return UiShowcase.Frames(10);
                    yield return Tap(GamepadButton.South, 14);
                    UiShowcaseNav.Check(shell.PopupOpen && Q("popup-locked") != null && started.Count == n, "challenges nav", "A on a locked location did not show the LockedPopup (" + shell.PopupOpen + ")");
                    yield return UiShowcase.Frames(30);
                    yield return UiShowcase.Shot("p2-challenges-locked");
                    yield return Tap(GamepadButton.East, 10);
                    UiShowcaseNav.Check(!shell.PopupOpen && Focused() == lockedCard.name, "challenges nav", "B did not close the popup on the card (" + Focused() + ")");
                }
            }
            OpenShell.ChallengeLaunchOverride = null;
            shell.ShowMain(); yield return UiShowcase.Frames(5);
        }

        // ------------------------------------------------------------------------------------------------ p2-motocard-welcome / p2-screen-adjust

        static IEnumerator MotocardWelcome()
        {
            Open();
            var shell = OpenShell.Instance;
            bool done = false;
            shell.ShowMotocardWelcome(() => done = true);
            yield return UiShowcase.Frames(40);
            UiShowcaseNav.Check(Q("motocard-welcome") != null, "motocard welcome", "the page did not open");
            Debug.Log(Tag + "motocard welcome: " + Q("motocard-welcome")?.Q<Label>(className: "oc-panel-title")?.text + " | focus " + Focused());
            yield return UiShowcase.Shot("p2-motocard-welcome");
            Debug.Log(Tag + "motocard welcome focus after the shot: " + Focused());
            UiShowcaseNav.Check(Focused() == "motocard-welcome-accept", "motocard welcome", "the page did not open on GRAB YOUR RIDE! (" + Focused() + ")");
            yield return Tap(GamepadButton.DpadDown);
            UiShowcaseNav.Check(Focused() == "motocard-welcome-adjust", "motocard welcome", "down did not reach SCREEN ADJUSTMENT (" + Focused() + ")");
            yield return Tap(GamepadButton.South, 40);
            UiShowcaseNav.Check(ReflexOptions.IsOpen, "motocard welcome", "SCREEN ADJUSTMENT did not open the page");
            yield return UiShowcase.Frames(30);
            yield return UiShowcase.Shot("p2-screen-adjust-welcome");
            var root = ReflexOptions.Root;
            UiShowcaseNav.Check(root?.Q<VisualElement>("screen-adjustment") != null, "motocard welcome", "the adjustment page is not up");
            yield return Tap(GamepadButton.East, 20);   // Back closes the options and returns to the welcome page
            UiShowcaseNav.Check(!ReflexOptions.IsOpen && Focused() == "motocard-welcome-adjust", "motocard welcome", "Back did not return to the welcome page (" + ReflexOptions.IsOpen + ", " + Focused() + ")");
            yield return Tap(GamepadButton.DpadUp);
            yield return Tap(GamepadButton.South, 10);
            UiShowcaseNav.Check(done && CoreGarageProfile.EventStatus(OpenShell.MotocardWelcomeFlag) == 1, "motocard welcome", "GRAB YOUR RIDE! did not go on and set the flag");
            shell.ShowMain(); yield return UiShowcase.Frames(5);
        }

        static IEnumerator ScreenAdjust()
        {
            Open();
            var shell = OpenShell.Instance;
            yield return HubReady();
            shell.ShowOptions(ReflexOptions.Video); yield return UiShowcase.Frames(30);
            var root = ReflexOptions.Root;
            var row = root?.Q<VisualElement>("screen-adjustment");
            UiShowcaseNav.Check(row != null, "screen adjustment", "OPTIONS > VIDEO has no SCREEN ADJUSTMENT row");
            if (row != null)
            {
                shell.Nav.Focus(row);   // (the options pages have their own nav: focus it directly)
                row.Focus(); yield return UiShowcase.Frames(6);
                yield return Tap(GamepadButton.South, 30);
                UiShowcaseNav.Check(root.Q<VisualElement>("screen-adjustment-steps") != null, "screen adjustment", "A on the row did not open the page");
                yield return UiShowcase.Frames(20);
                yield return UiShowcase.Shot("p2-screen-adjust");
                // gamma and brightness answer left / right
                int g = ReflexSave.GetInt("ReflexUI.Gamma", 0);
                root.Q<VisualElement>("gamma").Focus(); yield return UiShowcase.Frames(6);   // (a real mouse resting over a row would take the focus)
                yield return Tap(GamepadButton.DpadRight);
                UiShowcaseNav.Check(ReflexSave.GetInt("ReflexUI.Gamma", 0) != g, "screen adjustment", "right on GAMMA did not change it");
                yield return Tap(GamepadButton.DpadLeft);
                yield return Tap(GamepadButton.DpadDown);
                Debug.Log(Tag + "focus " + ReflexOptions.Root?.panel?.focusController?.focusedElement?.GetType().Name + " " + (ReflexOptions.Root?.panel?.focusController?.focusedElement as VisualElement)?.name);
                int b = ReflexSave.GetInt("ReflexUI.Brightness", 50);
                yield return Tap(GamepadButton.DpadRight);
                Debug.Log(Tag + "brightness " + b + " -> " + ReflexSave.GetInt("ReflexUI.Brightness", 50));
                UiShowcaseNav.Check(ReflexSave.GetInt("ReflexUI.Brightness", 50) != b, "screen adjustment", "right on BRIGHTNESS did not change it");
                yield return Tap(GamepadButton.DpadLeft);
                yield return Tap(GamepadButton.East, 20);   // Back: the VIDEO tab again
                UiShowcaseNav.Check(ReflexOptions.IsOpen && ReflexOptions.Tab == ReflexOptions.Video, "screen adjustment", "Back did not return to VIDEO (" + ReflexOptions.IsOpen + ", tab " + ReflexOptions.Tab + ")");
            }
            ReflexOptions.Close();
            shell.ShowMain(); yield return UiShowcase.Frames(5);
        }

        // ------------------------------------------------------------------------------------------------ p2-reel

        static IEnumerator Reel()
        {
            UiShowcase.ShellVisible(false);
            var go = new GameObject("showcase results");
            var page = go.AddComponent<OwnRaceResults>(); page.enabled = false;
            var snap = typeof(UiShowcasePauseModes).GetMethod("Snapshot", BindingFlags.Static | BindingFlags.NonPublic).Invoke(null, new object[] { HudMode.Race });
            SetOn(page, "snapshot", snap); SetOn(page, "active", true); SetOn(page, "armed", true); SetOn(page, "reelForShowcase", true);
            UiShowcasePages.Call(page, "Build"); UiShowcasePages.Call(page, "ShowResults", new object[] { null });
            UiShowcasePages.DriveOf(go, page);
            yield return UiShowcase.Frames(60);
            yield return UiShowcase.Shot("p2-reel-results");
            UiShowcasePages.Call(page, "EnterReel");
            yield return UiShowcase.Frames(40);
            yield return UiShowcase.Shot("p2-reel");
            SetOn(page, "active", false);
            UnityEngine.Object.Destroy(go);
            yield return UiShowcase.Frames(3);
        }
        static void SetOn(object o, string field, object value) => Set(o, field, value);
    }

    static class P2Ext
    {
        public static string Text(this VisualElement e) => e is Label l ? l.text : null;
    }
}

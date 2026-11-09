// The Free Ride challenges' end-of-challenge page in the open product: the original's PostChallengePopup.
//
// In the original the challenge activity reaches its Post state, awards the medals the class's vf64 found (MXActivityChallenge +0xff074
// gold / +0xff078 silver / +0xff07c bronze, written to the profile by 0054e670) and raises "Show Activity Results"; the UI manager
// (MXUIPostEventOverlay_OnShowActivityResults 005f42e0) puts the medal into the UI variable ChallengeMedalStatus ("Gold", "Silver",
// "Bronze", "Failed") and shows the page (Shared.ENG PostChallengePopup.lyr + PostChallengePopup.lua). The page is the install's own
// layer: TitleText = the challenge's title (DYN_CHALLENGE_TITLE: STR_<ACTIVITY>_TITLE), MessageText = the medal's sentence
// (DYN_CHALLENGE_RESULT_TEXT: STR_CHALLENGE_GOLD / SILVER / BRONZE / FAILURE, 00613cd0), two buttons ButtonOne (STR_POSTEVENT_RESTART)
// and ButtonTwo (STR_EXIT), left / right to move, accept (Enter / pad A / a click) to choose, back disabled (OnBackKeypress).
//   RESTART: HidePostChallengePopup, then PostChallengePopup:End -> App.RestartActivity (MXGame_RestartActivity 00759d00): the core
//            restarts the activity in place (rx_session_challenge_restart): the world and the session keep running, the challenge's
//            players go back to the start and the state machine runs again from Pre (no new world, no loading page).
//   EXIT:    App.RealWorldSwitchToFreeRide (the AAA loading page, the level's Free Ride) and App.RequestSaveGame
//            (OwnActivationArea.QuitToFreeRide; the open product saves through its own profile pipeline).
// Everything the page shows comes from the core (rx_session_challenge: the medal flags, the value, the criteria, the profile's record
// after the award); nothing is recomputed here. The CreditReward container (PRIZE / BONUS / TOTAL) shows what the core paid when the
// page appeared (rx_session_challenge_payout: PostChallengePopup:Start's SeriesPoints prize, the bonus rule, MXTableHelper:DepositIntoAccount
// and CheckForFreeRideSeriesChallengeCompletion); a Failed challenge pays nothing and the container stays hidden. The page adds the medal criteria and "Your record: " the original's Pre popup has, so the result reads against them.
// Presentation (modern, UI Toolkit): a dialog tinted by the medal won with the title, the sentence, the result value and the three criteria
// as tiles (the one reached is lit), the record, the prize / bonus / total, and the RESTART / EXIT buttons with their key caps.
// Sounds: PostChallengePopup:Start plays PopupShow and OnAcceptButtonOne / TwoKeypress play Accept, through the race's UI sound path
// (OriginalHud.UiSound: UIRegistry.xml's UI_02 events on the race's own event player). The key caps of the buttons show the device in use (OuGlyphs; OwnKeyGlyphs.ForCaps keeps the glyph record).
using System;
using System.Collections.Generic;
using System.Globalization;
using Reflex.Port.Content;
using Reflex.Port.OriginalWorld;
using Reflex.Port.RxCore;
using UnityEngine;
using UnityEngine.InputSystem;
using UnityEngine.UIElements;

namespace Reflex.Port.OpenFrontEnd
{
    [DisallowMultipleComponent]
    public sealed class OwnPostChallenge : MonoBehaviour
    {
        OriginalWorldRuntime runtime;
        ReflexEvents.Event ev;
        GameObject host; VisualElement root, layer, card, payoutRow;
        Label titleText, messageText, resultText, recordText;
        readonly Label[] criterionValue = new Label[3];
        readonly VisualElement[] criterionBox = new VisualElement[3];
        Label prizeValue, bonusValue, totalValue; VisualElement bonusStat;
        string messageString = "";
        volatile int payoutResult = int.MinValue; Reflex.Port.RxCore.RxChallengePayout payoutValue; bool payoutShown;
        /// The prize the core paid for the page on screen (valid once PayoutShown).
        public Reflex.Port.RxCore.RxChallengePayout Payout => payoutValue;
        public bool PayoutShown => payoutShown;
        readonly VisualElement[] button = new VisualElement[2];
        readonly Label[] buttonText = new Label[2];
        bool built, failed, open, armed, committed, cursorLease, postLatched;
        int focus, openedFrame = -1, restartCount, shownCount;
        float restartAt; volatile int restartResult = int.MinValue; bool restarting;
        RxChallengeState shown;
        OwnKeyGlyphs glyphs;

        /// The page is up (the world takes no input; the pause key does nothing).
        public bool Active => open;
        /// The core's snapshot the page was opened with.
        public RxChallengeState Shown => shown;
        /// The KeyMap glyphs of the page (tests).
        public OwnKeyGlyphs Glyphs => glyphs;
        /// The page's texts (tests).
        public string Title => titleText?.text;
        public string Message => messageString;
        public string[] ButtonLabels => new[] { buttonText[0]?.text, buttonText[1]?.text };
        /// 0 RESTART, 1 EXIT.
        public int FocusedButton => focus;
        /// How many times the page was shown / RESTART was chosen in this world.
        public int ShownCount => shownCount;
        public int RestartCount => restartCount;
        /// The page is on screen (built, shown, painted).
        public bool Rendered => open && built && root != null && root.panel != null && card.worldBound.width > 0;

        public static OwnPostChallenge Attach(OriginalWorldRuntime rt, ReflexEvents.Event e)
        {
            if (!rt || e == null) return null;
            var p = rt.GetComponent<OwnPostChallenge>() ?? rt.gameObject.AddComponent<OwnPostChallenge>();
            p.Close(); p.runtime = rt; p.ev = e; p.restarting = false; p.postLatched = false; p.restartCount = p.shownCount = 0; p.shown = default;
            rt.postChallenge = p;
            return p;
        }

        // ------------------------------------------------------------------------------------------------------ input
        static bool Down(Key key) => Keyboard.current != null && Keyboard.current[key].isPressed;
        static bool AcceptDown()
        {
            if (Down(Key.Enter) || Down(Key.NumpadEnter) || Down(Key.Space)) return true;
            if (Mouse.current?.leftButton.isPressed == true) return true;
            foreach (var pad in Gamepad.all) if (pad.buttonSouth.isPressed) return true;
            return false;
        }
        static bool AcceptPressed()
        {
            var kb = Keyboard.current;
            if (kb != null && (kb.enterKey.wasPressedThisFrame || kb.numpadEnterKey.wasPressedThisFrame || kb.spaceKey.wasPressedThisFrame)) return true;
            foreach (var pad in Gamepad.all) if (pad.buttonSouth.wasPressedThisFrame) return true;
            return false;
        }
        // -1 / +1 for a left (up) / right (down) step this frame: the shared direction stepper (d-pad, stick with its hysteresis and repeat,
        // arrows) and the keys A / D
        OuMenuInput input;
        int MoveStep()
        {
            input ??= new OuMenuInput();
            var st = input.Step();
            int d = st.x != 0 ? st.x : -st.y;
            var kb = Keyboard.current;
            if (kb != null) { if (kb.aKey.wasPressedThisFrame) d--; if (kb.dKey.wasPressedThisFrame) d++; }
            return Mathf.Clamp(d, -1, 1);
        }

        // ------------------------------------------------------------------------------------------------------ the page
        void Update()
        {
            if (!runtime || ev == null) return;
            if (restarting) { PollRestart(); return; }
            if (open && !runtime.Running) { Close(); return; }
            var ch = runtime.Challenge;
            bool known = runtime.HasChallenge && ch.valid != 0;
            bool post = known && ch.state == 4;
            // PostChallengePopup shows once per Post, as the original's Post enter raises it once. Only a real state other than Post
            // (Pre after a restart) re-arms it; a frame without a valid snapshot does not.
            if (known && ch.state != 4) postLatched = false;
            if (!open)
            {
                if (post && !postLatched && runtime.Running && !runtime.paused) { postLatched = true; Open(ch); }
                return;
            }
            // the activity left Post (a restart by another path): the page goes
            if (known && ch.state != 4) { Close(); return; }
            ShowPayout();
            glyphs?.Refresh();
            InputTick();
        }

        /// The page's own input: the buttons' focus moves with the shared direction stepper and A / Enter chooses (the showcase's controller
        /// walk calls this without a world).
        void InputTick()
        {
            if (Time.frameCount <= openedFrame) return;
            if (!armed) { if (!AcceptDown()) armed = true; return; }   // what was held when the page appeared does not choose
            if (committed) return;
            int move = MoveStep();
            if (move != 0) SetFocus(Mathf.Clamp(focus + (move < 0 ? -1 : 1), 0, 1));
            if (AcceptPressed()) Choose(focus);   // (the mouse: hovering a button focuses it, a click chooses it: the buttons' UI Toolkit items)
        }

        void Open(RxChallengeState ch)
        {
            Build();
            if (!built) return;
            shown = ch; open = true; armed = false; committed = false; focus = 0; openedFrame = Time.frameCount; shownCount++;
            Populate(ch);
            RequestPayout();
            root.style.display = DisplayStyle.Flex;
            OuMotion.Sync(root);
            layer.RemoveFromClassList("ou-in"); OuMotion.Fade(layer);
            OriginalHud.UiSound(runtime, "play", "PopupShow");         // PostChallengePopup:Start: App.PlayUISound( "PopupShow" )
            SetFocus(0);
            glyphs?.Refresh(true);
            cursorLease = true; Reflex.Port.Platform.ReflexCursor.Want(this, true);   // App.ShowMouseCursor( true ): shown for a mouse player only
            Debug.Log("[PostChallenge] " + ev.key + ": kind " + ch.kind + " medal " + ch.medal + " (gold " + ch.gold + " silver " + ch.silver + " bronze " + ch.bronze + ") value "
                + ch.value.ToString("F2", CultureInfo.InvariantCulture) + " criteria " + ch.criteria0.ToString("F1", CultureInfo.InvariantCulture) + " / " + ch.criteria1.ToString("F1", CultureInfo.InvariantCulture)
                + " / " + ch.criteria2.ToString("F1", CultureInfo.InvariantCulture) + " record " + (ch.recordValid != 0 ? ch.record.ToString("F2", CultureInfo.InvariantCulture) : "none"));
        }

        void Close()
        {
            open = false; armed = false; committed = false;
            input?.Dispose(); input = null;
            if (root != null) root.style.display = DisplayStyle.None;
            if (cursorLease) { cursorLease = false; Reflex.Port.Platform.ReflexCursor.Want(this, false); }   // App.ShowMouseCursor( false )
        }

        void SetFocus(int index)
        {
            focus = index;
            if (button[index] != null && button[index].panel != null && button[index].focusController?.focusedElement != button[index]) button[index].Focus();
        }

        void Choose(int index)
        {
            if (!open || !armed || committed || Time.frameCount <= openedFrame) return;
            committed = true;
            OriginalHud.UiSound(runtime, "play", "Accept");            // OnAcceptButtonOne / TwoKeypress
            if (index == 0) Restart(); else Exit();
        }

        // ------------------------------------------------------------------------------------------------------ the buttons
        // ButtonOne: PostChallengePopup:OnAcceptButtonOneKeypress -> "restart": End() calls App.RestartActivity.
        void Restart()
        {
            Debug.Log("[PostChallenge] RESTART " + ev.key);
            restartCount++; restarting = true; restartAt = Time.unscaledTime; restartResult = int.MinValue;
            Close();
            if (!runtime.RestartChallenge(r => restartResult = r)) restartResult = -2;
        }

        // The core answers on the worker before its next job; the challenge leaves Post when it ran. A core that cannot restart in place (an
        // older librxcore) falls back to a new session of the same challenge.
        void PollRestart()
        {
            var ch = runtime.Challenge;
            int r = restartResult;
            if (r != int.MinValue && r != 0)
            {
                Debug.LogWarning("[PostChallenge] the core could not restart the activity in place (" + r + "): a new session");
                restarting = false; runtime.Restart(); return;
            }
            bool left = r == 0 && runtime.HasChallenge && ch.valid != 0 && ch.state != 4;
            if (left) { restarting = false; Debug.Log("[PostChallenge] restarted in place: state " + ch.state); return; }
            if (Time.unscaledTime - restartAt > 10f) { Debug.LogWarning("[PostChallenge] the restart did not leave Post: a new session"); restarting = false; runtime.Restart(); }
        }

        // ButtonTwo: "exit": End() calls App.RealWorldSwitchToFreeRide and App.RequestSaveGame.
        void Exit()
        {
            Debug.Log("[PostChallenge] EXIT " + ev.key);
            Close();
            if (!OwnActivationArea.QuitToFreeRide(runtime)) OriginalPauseMenu.QuitToFrontEnd();
        }

        // ------------------------------------------------------------------------------------------------------ the page
        void Build()
        {
            if (built || failed) return;
            try
            {
                OuUi.EnsureEventSystem();
                host = new GameObject("Open Reflex PostChallengePopup"); host.transform.SetParent(transform, false);
                root = OuUi.CreateDocument(host, 72); OuTheme.Apply(root);
                root.style.display = DisplayStyle.None;
                layer = OuUi.Div("mxa-layer ou-dialog-layer", root);
                card = OuWidgets.Dialog(layer);
                var eyebrow = OuUi.Text("", "ou-eyebrow2 ou-label", card); eyebrow.name = "postchallenge-medal"; eyebrow.enableRichText = false;
                titleText = OuUi.Text("", "ou-dialog-title ou-display", card); titleText.enableRichText = false;
                messageText = OuUi.Text("", "ou-dialog-lead", card); messageText.enableRichText = false;
                resultText = OuUi.Text("", "ou-stat-value ou-display", card); resultText.enableRichText = false; resultText.style.marginBottom = 12; resultText.style.fontSize = 56;
                var criteria = OuUi.Div("ou-criteria", card);
                string[] kinds = { "bronze", "silver", "gold" };
                for (int i = 0; i < 3; i++)
                {
                    criterionBox[i] = OuUi.Div("ou-criterion ou-criterion-" + kinds[i], criteria);
                    criterionBox[i].userData = null;
                    OuUi.Text("", "ou-criterion-label ou-label", criterionBox[i]).enableRichText = false;
                    criterionValue[i] = OuUi.Text("", "ou-criterion-value ou-display", criterionBox[i]); criterionValue[i].enableRichText = false;
                }
                recordText = OuUi.Text("", "ou-eyebrow2 ou-label", card); recordText.enableRichText = false; recordText.style.marginBottom = 16;
                payoutRow = OuUi.Div("ou-payout ou-stats", card); payoutRow.style.display = DisplayStyle.None;
                var prize = OuWidgets.Stat(payoutRow, "PRIZE", "", "gold"); prizeValue = prize.Q<Label>(className: "ou-stat-value");
                bonusStat = OuWidgets.Stat(payoutRow, "BONUS", "", "good"); bonusValue = bonusStat.Q<Label>(className: "ou-stat-value");
                var total = OuWidgets.Stat(payoutRow, "TOTAL", ""); totalValue = total.Q<Label>(className: "ou-stat-value");
                var row = OuUi.Div("ou-button-row", card);
                glyphs = OwnKeyGlyphs.ForCaps();
                string[] names = { "postchallenge-restart", "postchallenge-exit" };
                for (int i = 0; i < 2; i++)
                {
                    int index = i;
                    button[i] = OuUi.Button("", "mxa-main-button" + (i == 0 ? " ou-primary" : ""), () => { SetFocus(index); Choose(index); }, row); button[i].name = names[i];
                    buttonText[i] = button[i].Q<Label>(); buttonText[i].enableRichText = false;
                    button[i].RegisterCallback<FocusInEvent>(_ => focus = index);
                    var cap = OuUi.Text("", "mxa-key", button[i]); cap.style.marginLeft = 16;
                    glyphs.AddCap("Accept", cap);
                }
                OuWidgets.LinkRow(button);
                built = true;
            }
            catch (Exception e) { failed = true; Debug.LogWarning("[PostChallenge] the popup was not built: " + e.Message); if (host) Destroy(host); }
        }

        // PostChallengePopup:Start and the strings the UI fills (00613cd0, App.PopulatePostChallengeStrings)
        void Populate(RxChallengeState ch)
        {
            payoutRow.style.display = DisplayStyle.None;     // shown by ShowPayout once the core paid
            titleText.text = ChallengeTitle(ch.kind);
            buttonText[0].text = Str("STR_POSTEVENT_RESTART", "RESTART");
            buttonText[1].text = Str("STR_EXIT", "EXIT");
            string sentence = Str(ch.medal == 3 ? "STR_CHALLENGE_GOLD" : ch.medal == 2 ? "STR_CHALLENGE_SILVER" : ch.medal == 1 ? "STR_CHALLENGE_BRONZE" : "STR_CHALLENGE_FAILURE", "");
            string result = ResultLine(ch);
            messageText.text = sentence;
            messageString = sentence + "\n" + result;
            // the dialog is tinted by the medal; the value read against the three criteria (Bronze, Silver, Gold), the one reached lit
            foreach (var k in new[] { "gold", "silver", "bronze", "fail" }) card.RemoveFromClassList("ou-dialog-" + k);
            card.AddToClassList("ou-dialog-" + (ch.medal == 3 ? "gold" : ch.medal == 2 ? "silver" : ch.medal == 1 ? "bronze" : "fail"));
            var eyebrow = card.Q<Label>("postchallenge-medal");
            eyebrow.text = ch.medal == 3 ? Str("STR_GOLD", "Gold").ToUpperInvariant() : ch.medal == 2 ? Str("STR_SILVER", "Silver").ToUpperInvariant() : ch.medal == 1 ? Str("STR_BRONZE", "Bronze").ToUpperInvariant() : "";
            eyebrow.style.display = ch.medal > 0 ? DisplayStyle.Flex : DisplayStyle.None;
            resultText.text = ch.valueValid != 0 ? Format(ch.kind, ch.value) : "";
            resultText.style.display = ch.valueValid != 0 ? DisplayStyle.Flex : DisplayStyle.None;
            string[] medalNames = { Str("STR_BRONZE", "Bronze"), Str("STR_SILVER", "Silver"), Str("STR_GOLD", "Gold") };
            for (int m = 0; m < 3; m++)
            {
                criterionBox[m].Q<Label>(className: "ou-criterion-label").text = medalNames[m].ToUpperInvariant();
                criterionValue[m].text = Format(ch.kind, ch.Criterion(m));
                criterionBox[m].EnableInClassList("ou-criterion-hit", ch.medal == m + 1);
            }
            bool record = ch.recordValid != 0;
            recordText.style.display = record ? DisplayStyle.Flex : DisplayStyle.None;
            if (record) recordText.text = Embedded("EMB_CHALLENGE_RECORD", "Your record: ").TrimEnd() + " " + Format(ch.kind, ch.record);
        }

        // PostChallengePopup:Start's payout: the core pays once per Post (UI SeriesId: the event's challenges series, else the level's).
        void RequestPayout()
        {
            payoutResult = int.MinValue; payoutShown = false;
            int series = ev.spec.seriesChallenge >= 0 ? ev.spec.seriesChallenge : ReflexModes.ChallengeSeriesOf(ev.spec.locationId);
            if (!runtime.PayoutChallenge(series, (r, v) => { payoutValue = v; payoutResult = r; })) payoutResult = -2;
        }
        // the CreditReward container: App.FormatCredits of the prize, the bonus and the total (shown only for a medal)
        void ShowPayout()
        {
            if (payoutShown || payoutResult == int.MinValue) return;
            payoutShown = true;
            var p = payoutValue;
            if (payoutResult != 0) { Debug.LogWarning("[PostChallenge] the core did not pay the prize (" + payoutResult + ")"); return; }
            Debug.Log("[PostChallenge] PAYOUT medal " + p.medal + " level " + p.rewardLevel + " prize " + p.prize + " bonus " + p.bonus + " total " + p.total
                + " balance " + p.accountBalance + " series " + p.seriesId + " best " + p.seriesBestPlace + (p.repeat != 0 ? " (repeat)" : ""));
            if (p.medal == 0 || p.paid == 0) return;
            prizeValue.text = Credits(p.prize); bonusValue.text = Credits(p.bonus); totalValue.text = Credits(p.total);
            payoutRow.style.display = DisplayStyle.Flex;
        }
        // App.FormatCredits 005dc430: the grouped number ("1,250")
        static string Credits(int value) => value.ToString("N0", CultureInfo.InvariantCulture);

        // the challenge's title (PreChallengePopup's table: STR_<ACTIVITY>_TITLE)
        internal static string ChallengeTitle(int kind)
        {
            string key = kind == 0 ? "STR_BIGAIR_TITLE" : kind == 1 ? "STR_HILLCLIMB_TITLE" : kind == 6 ? "STR_TRAILBLAZER_TITLE" : kind == 7 ? "STR_HITDISCOVERY_TITLE" : kind == 8 ? "STR_TARGETPRACTICE_TITLE" : null;
            return key == null ? "CHALLENGE" : Str(key, "CHALLENGE");
        }

        // The result against the medal criteria and the stored record, in the units of PreChallengePopup (App.FormatDistance for the Hill
        // Climb, App.FormatTime for the Trailblazer, ChallengeFormatTargets for Target Practice).
        string ResultLine(RxChallengeState ch)
        {
            string value = ch.valueValid != 0 ? Format(ch.kind, ch.value) : "";
            string line = value;
            string[] names = { Str("STR_BRONZE", "Bronze"), Str("STR_SILVER", "Silver"), Str("STR_GOLD", "Gold") };
            for (int m = 0; m < 3; m++) line += (line.Length > 0 ? "   |   " : "") + names[m].ToUpperInvariant() + " " + Format(ch.kind, ch.Criterion(m));
            if (ch.recordValid != 0) line += "\n" + Embedded("EMB_CHALLENGE_RECORD", "Your record: ").TrimEnd() + " " + Format(ch.kind, ch.record);
            return line;
        }

        public static string Format(int kind, float value)
        {
            switch (kind)
            {
                case 6: return TimeText(value);
                case 8: case 7: return Count(kind, value);
                default: return Distance(value);
            }
        }
        // App.FormatDistance 0073d290 -> 005dc4b0: the value truncated to an integer number of feet, metres when the units are metric; one decimal
        static string Distance(float feet)
        {
            bool metric = Reflex.Port.Options.ReflexSettings.Metric;
            float v = (int)feet * (metric ? 0.3048f : 1f);
            return v.ToString("0.0", CultureInfo.InvariantCulture) + " " + Str(metric ? "STR_UNIT_METER" : "STR_UNIT_FEET", metric ? "m" : "ft");
        }
        // App.FormatTime 00790100: minutes and seconds with the hundredths ("00:12.34"); 0 and the unset float show dashes
        static string TimeText(float t)
        {
            if (t == 0f || t == float.MaxValue) return "--:--.--";
            if (t > 5999.99f) return "99:99.99";
            if (t < -5999.99f) return "-99:99.99";
            int cs = (int)(Math.Abs(t * 100f) + 0.5f);
            return (t < 0 ? "-" : "") + (cs / 6000).ToString("00") + ":" + (cs / 100 % 60).ToString("00") + "." + (cs % 100).ToString("00");
        }
        // ChallengeFormatTargets / ChallengeFormatDiscoveries: App.FormatChallengeCriteria( key, param ), "singular" for 1
        static string Count(int kind, float n)
        {
            int count = (int)n;
            string key = kind == 8 ? (count > 1 ? "STR_TARGETS_PLURAL" : "STR_TARGETS_SINGULAR") : (count > 1 ? "STR_DISCOVERY_PLURAL" : "STR_DISCOVERY_SINGULAR");
            return count + " " + Str(key, kind == 8 ? "Targets" : "Discoveries");
        }

        // an embedded-format string (EMB_...: the text the game formats a value into; FrontEndCareer.Embeds reads them from the table)
        static Dictionary<string, string> embeds;
        internal static string Embedded(string key, string def)
        {
            try
            {
                var data = OriginalUiAssets.Data;
                embeds ??= Reflex.Port.FrontEnd.FeCareerScripts.Embeds(data.Read(data.Pkg("InGame"), "localiz", "MXRaven_Default_Strings"));
                if (embeds.TryGetValue(key, out var s) && !string.IsNullOrEmpty(s)) return s;
            }
            catch (Exception) { }
            return def;
        }

        // a string of the in-game tables (InGame, then Shared), else the front end's
        internal static string Str(string key, string def)
        {
            var data = OriginalUiAssets.Data;
            foreach (var package in new[] { "InGame", "Shared" })
            {
                try { if (data.Strings(package).TryGetValue(key, out var s) && !string.IsNullOrEmpty(s) && !s.StartsWith("?")) return s; } catch (Exception) { }
            }
            return OpenContent.Str(key, def);
        }

        void OnDisable() => Close();
        void OnDestroy() { Close(); if (host) Destroy(host); if (runtime && runtime.postChallenge == this) runtime.postChallenge = null; }
    }
}

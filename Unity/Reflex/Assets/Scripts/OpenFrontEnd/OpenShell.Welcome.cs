// The first-run MOTOCARD welcome (MotocardWelcome.lua, the Welcome Page of the Motocard setup wizard, mode "" = the first sign-in): the title
// STR_MOTOCARD_WELCOME_TITLE ("WELCOME TO MX VS ATV REFLEX!"), the message STR_MOTOCARD_WELCOME_MESSAGE, and the accept legend
// STR_MOTOCARD_NAV_WELCOME ("GRAB YOUR RIDE!"). The original sits it after the name entry and the screen adjustment in its create-a-MotoCard
// wizard (Autosave Notice > Name > Screen Adjustment > Welcome > Vehicle > Gear ...); the open product's profile is made at the first start
// and its name, gear and vehicle are the GARAGE's (RIDER / GEAR / VEHICLE), so this port shows the two pages that carry words of their own: the
// SCREEN ADJUSTMENT (OPTIONS > VIDEO > SCREEN, reachable from here as well) and this welcome, once, on a profile that has not played yet.
// The flag is the PlayerEvent "MotocardWelcome" of the profile; a profile with any career or Free Ride progress is never welcomed (it is not a first run).
using System;
using System.Linq;
using Reflex.Port.Content;
using UnityEngine;
using UnityEngine.UIElements;

namespace Reflex.Port.OpenFrontEnd
{
    public sealed partial class OpenShell
    {
        public const string MotocardWelcomeFlag = "MotocardWelcome";

        /// The first run: the open product, a profile without the flag and without any play on it.
        public static bool MotocardWelcomeDue()
        {
            if (SkipIntro || Environment.GetEnvironmentVariable("REFLEX_NO_WELCOME") == "1") return false;
            if (!Reflex.Port.RxCore.Bridge.OpenReflexSelector.UseOpenReflex) return false;
            try
            {
                if (!CoreGarageProfile.Open() || CoreGarageProfile.EventStatus(MotocardWelcomeFlag) != 0) return false;
                var p = CoreGarageProfile.Capture();
                if (p == null) return false;
                bool played = p.T("PlayerSeriesStatus").rows > 0 || p.T("PlayerRecords").rows > 0 || p.T("PlayerChallenges").rows > 0
                              || p.T("PlayerHighlight").rows > 0 || CoreGarageProfile.EventStatus("CareerWelcome") != 0;
                return !played;
            }
            catch (Exception e) { Debug.LogWarning("[Welcome] " + e.Message); return false; }
        }

        /// App.PlayUILocalizedSoundEvent( "UIAudio", "UI_Game_Intro", "UI_GAME_INTRO/VO/INTRO" ): the narration of the first-run welcome (mode "").
        public static readonly (string database, string package, string eventPath) MotocardWelcomeVoice = ("UIAudio", "UI_Game_Intro", "UI_GAME_INTRO/VO/INTRO");

        // ---- narration (App.PlayUILocalizedSoundEvent / StopUILocalizedSoundEvent on the shell's FeSounds): one voice per page, stopped when the page goes
        string _narrationOwner;
        /// The spoken introduction of a page; the owner is the page's name. A voice the install does not have is silent, as in the original.
        void Narrate((string database, string package, string eventPath) voice, string owner)
        {
            StopNarration();
            if (_sounds == null) return;
            try { _sounds.PlayLocalized(voice.database, voice.package, voice.eventPath, owner); _narrationOwner = owner; }
            catch (Exception e) { Debug.LogWarning("[Welcome] narration: " + e.Message); }
        }
        /// App.StopUILocalizedSoundEvent( the page ): called when another screen replaces the page (Clear) and when it is accepted.
        void StopNarration()
        {
            if (_narrationOwner == null) return;
            try { _sounds?.StopLocalized(_narrationOwner); } catch (Exception) { }
            _narrationOwner = null;
        }

        /// MotocardWelcome:Start / OnAcceptKeypress: the page; accept goes on (here: to the hub) and keeps the flag.
        public void ShowMotocardWelcome(Action proceed)
        {
            Clear(Screen.Working, "career"); SetBackground(null); MusicTier(2); CareerChrome();
            OcUi.Stage(_layer, true);
            var mid = OuUi.Div("oc-mid p2-center", _layer);
            var panel = OcUi.Panel(mid, OpenContent.Str("STR_MOTOCARD_WELCOME_TITLE", "WELCOME TO MX VS ATV REFLEX!").ToUpperInvariant(), out var body, out _, "p2-welcome p2-motocard-welcome");
            panel.name = "motocard-welcome";
            foreach (var paragraph in OpenContent.Str("STR_MOTOCARD_WELCOME_MESSAGE", "").Split(new[] { "\n \n", "\n\n" }, StringSplitOptions.RemoveEmptyEntries))
                OuUi.Text(paragraph.Trim(), "p2-welcome-text", body).enableRichText = false;
            Action accept = () =>
            {
                if (!CoreGarageProfile.SetEventStatus(MotocardWelcomeFlag, 1)) Debug.LogWarning("[Welcome] the flag was not saved: " + CoreGarageProfile.Error);
                OuSound.Play("Accept");
                StopNarration();   // MotocardWelcome:End
                proceed?.Invoke();
            };
            var go = OcUi.Button(OpenContent.Str("STR_MOTOCARD_NAV_WELCOME", "GRAB YOUR RIDE!").ToUpperInvariant(), "motocard-welcome-accept", accept, body, "p2-welcome-go");
            // the wizard's Screen Adjustment Page comes before this one in the original: here it is a button, the same page as OPTIONS > VIDEO
            var adjust = OcUi.Button(OpenContent.Str("STR_SCREENADJUSTMENT_TITLE", "SCREEN ADJUSTMENT").ToUpperInvariant(), "motocard-welcome-adjust",
                () => ShowScreenAdjustmentPage(() => _nav.Focus(_layer.Q<VisualElement>("motocard-welcome-adjust"))), body, "oc-btn-quiet");
            OuWidgets.LinkColumn(new System.Collections.Generic.List<VisualElement> { go, adjust });
            OcUi.Plaque(OuUi.Div("oc-bottom", _layer), OpenContent.Str("STR_MOTOCARD", "MOTOCARD").ToUpperInvariant());
            OcUi.Legend(_layer, (OuUi.KeySubmit, "SELECT", null));
            _nav.OnCancel = () => true;   // (the original's wizard page has no Back)
            _nav.DefaultItem = () => go; _nav.Focus(go);
            Narrate(MotocardWelcomeVoice, "motocard-welcome");   // MotocardWelcome:Start (mode ""): the game's introduction, stopped at End
        }

        /// The SCREEN ADJUSTMENT page (OPTIONS > VIDEO > SCREEN ADJUSTMENT) alone, over the shell, which steps aside meanwhile (as ShowOptions).
        public void ShowScreenAdjustmentPage(Action closed)
        {
            if (Options.ReflexOptions.IsOpen) return;
            var soundBefore = Options.OptUi.Sound;
            Options.OptUi.Sound = OuSound.Play;
            _layer.style.display = DisplayStyle.None; _hints.style.display = DisplayStyle.None; _status.style.display = DisplayStyle.None;
            Options.ReflexOptions.OpenScreenAdjustment(() =>
            {
                Options.OptUi.Sound = soundBefore;
                if (!this) return;
                _layer.style.display = DisplayStyle.Flex;
                closed?.Invoke();
            });
        }
    }
}

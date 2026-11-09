// The front end's side of the MotoSkills tutorials (WP-T, _re/notes/tutorials.md): the main menu's MOTOSKILLS key and the first-visit
// popup. The tutorials themselves run in their own world (ReflexTutorials, OriginalTutorial.cs): the game's TutorialManager and page
// Lua, live.
//   MainMenu:OnTutorialKeypress (MainMenu.lyr's misc key, F2 / Y; the button legend's STR_TUTORIALS): PlayerMode SinglePlayer, the
//     saved TutorialRequestSaveGame cleared, then what the game does in the front end before the Tutorial Load Page - the equipped
//     vehicle kept in UI TutorialOwnerVehicleId / TutorialVehicleOwner, ChangeVehicle( row, 2 ) (an MX Lite), SyncDatabaseWithManagers,
//     EnableTutorialMode( TRUE ), "GotoLoadingPage" -> TransitionPageGraph "Tutorial Load Page" (Background01.lyr + LoadingScreen.lyr),
//     PendingHUBPopup ShowTutorialIntroPopup - happens in the tutorial world's boot (tutorial.cpp start) and, on the player's loaded
//     tables, again in the world (tutorial "attach"). Here: the Tutorial Load Page over the scene load (FrontEndManager.LaunchTutorial),
//     back on the Main Menu Page when the game leaves the tutorial. TutorialLoadingPage.lua has no Start: the UI music keeps playing.
//   MainMenu:Present: PlayerRider.TutorialsEntered == FALSE (and no invite awaited) -> "ShowTutorialFirstTimeEntryPopup".
//   TutorialFirstTimeEntryPopup.lua (TutorialFirstTimeEntryPopup.lyr): GOT SKILLS? / STR_TUTORIAL_POPUP_DESCRIPTION / START (ButtonOne,
//     focused) / SKIP (ButtonTwo); Back = SKIP. End: PlayerRider.TutorialsEntered = TRUE, START -> "TriggerTutorialKeypress" (the main
//     menu's OnTutorialKeypress), App.RequestSaveGame (the profile session: set_int + save).
using System;
using System.Linq;
using System.Threading.Tasks;
using Reflex.Port.Content;
using UnityEngine;

namespace Reflex.Port.FrontEnd
{
    public static class FeTutorialScripts
    {
        public static FeScript Create(string script) => script == "TutorialFirstTimeEntryPopup" ? new TutorialFirstTimeEntryPopupScript() : null;

        /// Headless (batch-mode) runs keep the main menu free of the first-visit popup (the other benches walk the main menu with a fresh
        /// profile), unless a test sets this (TutorialTests).
        public static bool FirstTimePopupInBatchMode;
        // the default profile's PlayerRider without a profile session (a guest): the game keeps TutorialsEntered = TRUE in memory after the
        // popup's End (nothing saved without storage), so the popup shows once per run
        static bool guestEntered;
        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.SubsystemRegistration)]
        static void ResetStatics() { guestEntered = false; entering = null; startAfter = null; }   // (FirstTimePopupInBatchMode: set by a test before play mode)

        /// PlayerRider.TutorialsEntered of the player (the profile session's tables; a guest's in-memory value).
        public static bool TutorialsEntered => ReflexProfile.Current != null ? ReflexProfile.Current.T("PlayerRider").Int("TutorialsEntered", 0, 0) != 0 : guestEntered;
        /// PlayerRider.TutorialsCompleted (TutorialPauseMenu: the lessons unlocked are this + 1).
        public static int TutorialsCompleted => ReflexProfile.Current?.T("PlayerRider").Int("TutorialsCompleted", 0, 0) ?? 0;

        /// MainMenu:Present's first-visit check.
        public static void MainMenuPresent(FrontEndManager fe)
        {
            if (fe == null || Application.isBatchMode && !FirstTimePopupInBatchMode) return;
            if (ReflexProfile.Busy || TutorialsEntered || fe.TopPopup != null) return;
            fe.SendUIEvent("ShowTutorialFirstTimeEntryPopup");
        }

        /// MainMenu:OnTutorialKeypress.
        public static void OnTutorialKeypress(FrontEndManager fe)
        {
            var ev = ReflexTutorials.Event;
            if (fe == null || ev == null) { Debug.LogWarning("[Tutorials] no UI_Compound in the install: MOTOSKILLS does nothing"); return; }
            fe.PlaySound("Accept");
            fe.vars["PlayerMode"] = "SinglePlayer";
            fe.vars.Remove("TutorialRequestSaveGame");
            fe.LaunchTutorial(ev);
        }

        /// The first-visit popup's End: TutorialsEntered = TRUE and App.RequestSaveGame, in the profile session (a guest: in memory);
        /// START: MOTOSKILLS once that is written.
        internal static void Entered(FrontEndManager fe, bool start)
        {
            if (ReflexProfile.Disabled || ReflexProfile.Current == null) { guestEntered = true; entering = Task.FromResult(true); }
            else entering = ReflexProfile.Run("set_int PlayerRider 0 TutorialsEntered 1", "save");
            startAfter = start ? fe : null;
        }
        static Task<bool> entering; static FrontEndManager startAfter;
        /// The main menu's frame (MainMenuScript.Update): START of the first-visit popup, after its save.
        public static void Update(FrontEndManager fe)
        {
            if (startAfter == null || startAfter != fe || entering == null || !entering.IsCompleted) return;
            startAfter = null; OnTutorialKeypress(fe);
        }
    }

    // TutorialFirstTimeEntryPopup.lua
    public sealed class TutorialFirstTimeEntryPopupScript : FeScript
    {
        bool accept;
        public override void Initialize()
        {
            C("TitleText")?.SetText(Str("STR_TUTORIAL_POPUP_TITLE"));
            C("MessageText")?.SetText(Str("STR_TUTORIAL_POPUP_DESCRIPTION"));
            C("ButtonOne")?.Find("ButtonText")?.SetText(Str("STR_TUTORIAL_POPUPBUTTON_ONE"));
            C("ButtonTwo")?.Find("ButtonText")?.SetText(Str("STR_TUTORIAL_POPUPBUTTON_TWO"));
            functions["OnAcceptButtonOneKeypress"] = () => { fe.PlaySound("Accept"); accept = true; fe.SendUIEvent(FePopupScripts.HideEvent(fe, "HideTutorialFirstTimeEntryPopup")); };
            functions["OnAcceptButtonTwoKeypress"] = () => { fe.PlaySound("Accept"); fe.SendUIEvent(FePopupScripts.HideEvent(fe, "HideTutorialFirstTimeEntryPopup")); };
            functions["OnBackKeypress"] = () => { fe.PlaySound("Back"); fe.SendUIEvent(FePopupScripts.HideEvent(fe, "HideTutorialFirstTimeEntryPopup")); };
        }
        public override void Start()
        {
            fe.PlaySound("PopupShow");
            // m_ButtonOneContainer:SetFocused( TRUE ): START (first of the popup's button list)
            var one = C("ButtonOne"); var list = one?.parent;
            if (list != null && list.kids.Contains(one)) list.focusIndex = list.kids.IndexOf(one);
            accept = false;
            KeyGlyph.Refresh(fe, layer.root);
        }
        // TutorialsEntered = TRUE, "TriggerTutorialKeypress" with START, App.RequestSaveGame: the tutorial world loads that save, so
        // MOTOSKILLS follows once it is written (FeTutorialScripts.Update, from the main menu's frame)
        public override void End() { FeTutorialScripts.Entered(fe, accept); accept = false; }
    }
}

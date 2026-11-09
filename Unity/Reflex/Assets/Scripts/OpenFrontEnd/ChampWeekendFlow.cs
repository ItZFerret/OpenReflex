// RESTORED CONTENT (championship weekend): the weekend's in-game flow. A weekend is started from the event's setup page (FORMAT: CHAMPIONSHIP
// WEEKEND, OpenShell.ChampWeekend.cs) and then stays in the race scene until it is over:
//   a stage ends     the finish (the results page, then CONTINUE WEEKEND or BACK), the pause page's RETIRE FROM <STAGE> (you classed last) or
//                    the legacy post-race page's CONTINUE: StageOver shows the weekend card (OwnChampWeekendCard: the standings, the
//                    qualification, your gate and the next stage's format) over the world
//   the next stage   NextStage: the next stage armed at the same location (RxChampWeekend.Arm) and a new session of the same event in this
//                    scene (OriginalWorldRuntime.Restart, as the pause page's RESTART), the loading screen (OpenLoading) over it until it runs
//   the end          the card's FINISH WEEKEND after the last stage (or the local rider's elimination), or QUIT WEEKEND (the card, the pause
//                    page): the weekend ends and the menu comes back (the event's setup page, as after any race)
//   RESTART          the pause page's RESTART runs the same stage again with no result (RxChampWeekend.PrepareRestart, RestartEvent)
using System;
using Reflex.Port.OriginalWorld;
using Reflex.Port.RxCore;
using UnityEngine;

namespace Reflex.Port.OpenFrontEnd
{
    public static class ChampWeekendFlow
    {
        /// The running session races a weekend's stage.
        public static bool InStage => RxChampWeekend.StageInSession;
        /// The weekend card is up over this world.
        public static bool CardUp(OriginalWorldRuntime rt) => rt && rt.TryGetComponent<OwnChampWeekendCard>(out var c) && c.Active;

        /// What the card shows: the weekend's standings.
        internal static ChampWeekendView View() => ChampWeekendView.FromWeekend();

        /// The stage is over (its results seen): the weekend card over the world. The stage is recorded on the worker at Post (or by a retire);
        /// the card waits for that before it reads the standings.
        public static void StageOver(OriginalWorldRuntime rt)
        {
            if (!rt) return;
            var card = rt.GetComponent<OwnChampWeekendCard>() ?? rt.gameObject.AddComponent<OwnChampWeekendCard>();
            Debug.Log("[ChampWeekend] stage over: " + (RxChampWeekend.SessionStage >= 0 ? RxChampWeekend.StageLabel(RxChampWeekend.SessionStage) : "(no stage)") + " (recorded " + RxChampWeekend.StageRecorded + ")");
            card.Begin(rt);
        }

        /// The pause page's RETIRE FROM <STAGE>: the stage is recorded with the local rider last (on the worker, between two frames), then the card.
        public static void Retire(OriginalWorldRuntime rt)
        {
            if (!rt) return;
            var open = rt.SimDriver?.Open;
            if (open != null) open.BetweenFrames(s => { if (!RxChampWeekend.RetireNow(s)) Debug.LogWarning("[ChampWeekend] the retire was not recorded: " + RxChampWeekend.LastError); });
            Debug.Log("[ChampWeekend] RETIRE from " + (RxChampWeekend.SessionStage >= 0 ? RxChampWeekend.StageLabel(RxChampWeekend.SessionStage) : "(no stage)"));
            StageOver(rt);
        }

        /// THE hook between two stages (the card's START): the weekend's next stage loads in this scene (a new
        /// session of the same event at the same location, the loading screen over it); after the last stage the weekend ends and the menu
        /// comes back. False when nothing was started (the weekend is over or gone: the menu then).
        public static bool NextStage(OriginalWorldRuntime rt)
        {
            if (!RxChampWeekend.Active) { Debug.Log("[ChampWeekend] no weekend: back to the menu"); ToMenu(); return false; }
            if (RxChampWeekend.Finished) { FinishWeekend(); return false; }
            var ev = OpenContent.Find(RxChampWeekend.EventKey);
            string location = RxChampWeekend.Location ?? ev?.Location;
            if (!rt || string.IsNullOrEmpty(location) || !RxChampWeekend.Arm(location)) { Debug.LogWarning("[ChampWeekend] the next stage could not be armed: back to the menu"); QuitWeekend(); return false; }
            int next = RxChampWeekend.Stage;
            Debug.Log("[ChampWeekend] next stage: " + RxChampWeekend.StageLabel(next) + " (" + RxChampWeekend.StageFormat(next) + ") at " + location);
            if (rt.TryGetComponent<OwnChampWeekendCard>(out var card)) card.Close();
            if (ev != null) OpenLoading.Show(ev, null);   // (over the world until the new session runs)
            rt.Restart();
            return true;
        }

        /// The weekend is over (the card's FINISH WEEKEND): the menu.
        public static void FinishWeekend()
        {
            Debug.Log("[ChampWeekend] the weekend is over: back to the menu");
            RxChampWeekend.Abandon();
            ToMenu();
        }

        /// QUIT WEEKEND (the pause page, the card): the weekend ends where it is, the menu.
        public static void QuitWeekend()
        {
            Debug.Log("[ChampWeekend] QUIT WEEKEND");
            RxChampWeekend.Abandon();
            ToMenu();
        }

        static void ToMenu() => OriginalPauseMenu.QuitToFrontEnd();
    }
}

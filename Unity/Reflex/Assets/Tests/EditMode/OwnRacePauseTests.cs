using System;
using System.Linq;
using System.Reflection;
using System.Runtime.InteropServices;
using NUnit.Framework;

namespace Reflex.Port.RxCore.Tests
{
    public class OwnRacePauseTests
    {
        static Type RuntimeType(string name) => AppDomain.CurrentDomain.GetAssemblies().Select(a => a.GetType(name)).First(t => t != null);
        static bool Supports(RxEventUiState s) => (bool)RuntimeType("Reflex.Port.OpenFrontEnd.OwnRacePause").GetMethod("Supports").Invoke(null, new object[] { s });
        static RxEventUiState Racing(int activity = 0) => new RxEventUiState { known = RxEventUiKnown.ActivityType | RxEventUiKnown.RaceState | RxEventUiKnown.RulesHold, activity_type = activity, race_state = 2, rules_hold = 0, native_allow_pause = -1, native_pause_owner = -1 };

        // every offline mode: National 0, Supercross 1, Freestyle 5, Waypoint 6, Free Ride 8, Section Race 12, Terracross 17, Powercross 19, the challenges 22..30;
        // not the online party modes (9 Tag, 10 Snake, 11 Graffiti, 13..16), the unlisted types (2, 3, 4, 7) or an unknown one
        [TestCase(0, true)] [TestCase(1, true)] [TestCase(5, true)] [TestCase(6, true)] [TestCase(8, true)] [TestCase(12, true)] [TestCase(17, true)] [TestCase(19, true)]
        [TestCase(22, true)] [TestCase(27, true)] [TestCase(29, true)] [TestCase(30, true)]
        [TestCase(2, false)] [TestCase(3, false)] [TestCase(4, false)] [TestCase(7, false)] [TestCase(9, false)] [TestCase(10, false)] [TestCase(13, false)] [TestCase(31, false)] [TestCase(-1, false)]
        public void OwnedPauseCoversTheOfflineModes(int activity, bool expected) => Assert.AreEqual(expected, Supports(Racing(activity)));
        // the countdown and the event in progress are the pause's windows; the intro (0), the wind-down (3), the post pages (4) and the inactive activity (5) are not
        [TestCase(0)] [TestCase(3)] [TestCase(4)] [TestCase(5)] [TestCase(-1)]
        public void IntroFinishAndInactiveKeepTheirExistingRoute(int state) { var s = Racing(); s.race_state = state; Assert.IsFalse(Supports(s)); }
        [Test] public void TheCountdownCanBePaused() { var s = Racing(); s.race_state = 1; Assert.IsTrue(Supports(s)); }
        [TestCase(RxEventUiKnown.ActivityType)] [TestCase(RxEventUiKnown.RaceState)] [TestCase(RxEventUiKnown.RulesHold)]
        public void MissingAuthoritativeFieldCannotOpenOwnedPause(RxEventUiKnown missing) { var s = Racing(); s.known &= ~missing; Assert.IsFalse(Supports(s)); }
        [Test] public void RulesAndNativeOwnershipPreventCompetingPauseMenus()
        {
            var s = Racing(); s.rules_hold = 1; Assert.IsFalse(Supports(s));
            s = Racing(); s.known |= RxEventUiKnown.PauseOwner; s.native_pause_owner = 1; Assert.IsFalse(Supports(s));
            s = Racing(); s.known |= RxEventUiKnown.AllowPause; s.native_allow_pause = 0; Assert.IsFalse(Supports(s));
            s.native_allow_pause = -1; Assert.IsFalse(Supports(s));
            s.native_allow_pause = 1; Assert.IsTrue(Supports(s));
            Assert.IsFalse(Supports(default));
        }
        [Test] public void ModeResultsAndHudExportsKeepTheirLayout()
        {
            Assert.AreEqual(1200, Marshal.SizeOf<RxModeResultsState>());
            Assert.AreEqual(96, Marshal.SizeOf<RxModeResultRow>());
            Assert.AreEqual(620, Marshal.SizeOf<RxRaceHudState>());
            var hud = RxRaceHudState.Create();
            Assert.AreEqual(0u, hud.discoveryEvents); Assert.AreEqual(0, hud.discoveryFound); Assert.AreEqual(0, hud.discoveryTotal);
            var state = RxModeResultsState.Make(5, -1, RxModeResultRow.Make(3, true, 2, true, 60f, score: 71.5f, bestTrickScore: 8.2f, trick: "STR_RODEO"), RxModeResultRow.Make(1, false, 1, true, 60f, score: 80f));
            Assert.AreEqual(2, state.Count);
            Assert.IsTrue(state.TryLocal(out var mine)); Assert.AreEqual(3, mine.slot); Assert.AreEqual("STR_RODEO", mine.BestTrick); Assert.AreEqual(71.5f, mine.score);
            Assert.IsTrue(state.TryBySlot(1, out var other)); Assert.AreEqual(0, other.hasBestTrick); Assert.AreEqual(string.Empty, other.BestTrick);
            Assert.IsFalse(state.TryBySlot(7, out _));
        }
        [Test] public void SnapshotHasExactNativeScalarLayout()
        {
            Assert.AreEqual(32, Marshal.SizeOf<RxEventUiState>());
            var names = new[] { "known", "activity_type", "race_state", "rules_hold", "post_state", "skip_requested", "native_allow_pause", "native_pause_owner" };
            for (int i = 0; i < names.Length; i++) Assert.AreEqual(i * 4, Marshal.OffsetOf<RxEventUiState>(names[i]).ToInt32(), names[i]);
        }
        [Test] public void ResultBridgeCopiesSnapshotAndClearsItWhenRecycled()
        {
            var bridge = RuntimeType("Reflex.Port.RxCore.Bridge.OpenReflexResultsAdapter");
            var target = bridge.GetMethod("Create").Invoke(null, null);
            var source = new OpenReflexSimulation.Results { eventUi = Racing() };
            bridge.GetMethod("Apply").Invoke(null, new object[] { target, source });
            var field = target.GetType().GetField("eventUi");
            Assert.AreEqual(source.eventUi, field.GetValue(target));
            bridge.GetMethod("Reset").Invoke(null, new[] { target });
            Assert.AreEqual(default(RxEventUiState), field.GetValue(target));
        }
    }
}

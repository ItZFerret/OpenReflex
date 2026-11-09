using System;
using System.Collections.Generic;
using System.Linq;
using NUnit.Framework;

namespace Reflex.Port.RxCore.Tests
{
    public class OwnRaceResultsPolicyTests
    {
        static Type TypeOf(string name) => AppDomain.CurrentDomain.GetAssemblies().Select(a => a.GetType(name)).First(t => t != null);
        static Type Policy => TypeOf("Reflex.Port.OpenFrontEnd.OwnRaceResultsPolicy");
        static bool Invoke(string name, params object[] args) => (bool)Policy.GetMethod(name).Invoke(null, args);
        static RxEventSpec Spec() => new RxEventSpec { PlayerMode = "SinglePlayer", EventType = "National", DurationType = "Laps", duration = 1, riders = 2 };
        [TestCase("National")] [TestCase("Supercross")] [TestCase("Terracross")] [TestCase("Powercross")] [TestCase("WaypointRace")]
        public void ResolvedOfflineLapSpecSelectsSupportedMode(string type)
        { var s = Spec(); s.EventType = type; Assert.IsTrue(Invoke("IsExhibitionSpec", s)); }
        // the timed modes: their duration is minutes (an event's Duration row), never laps
        [TestCase("Freestyle")] [TestCase("SectionRace")]
        public void ResolvedOfflineTimedSpecSelectsSupportedMode(string type)
        {
            var s = Spec(); s.EventType = type; s.DurationType = "Minutes"; Assert.IsTrue(Invoke("IsExhibitionSpec", s));
            s.DurationType = "Laps"; Assert.IsFalse(Invoke("IsExhibitionSpec", s));
        }
        [Test] public void OtherLaunchContextsCannotSelectOwnedResults()
        {
            var s = Spec(); s.PlayerMode = "Network"; Assert.IsFalse(Invoke("IsExhibitionSpec", s));
            s = Spec(); s.PlayerMode = ""; Assert.IsFalse(Invoke("IsExhibitionSpec", s));
            s = Spec(); s.EventActivation = "Career"; Assert.IsFalse(Invoke("IsExhibitionSpec", s));
            s = Spec(); s.EventType = "FreeRide"; Assert.IsFalse(Invoke("IsExhibitionSpec", s));
            s = Spec(); s.DurationType = "Minutes"; Assert.IsFalse(Invoke("IsExhibitionSpec", s));   // (a lap race counts laps)
            s = Spec(); s.EventType = "UTVMoto"; Assert.IsFalse(Invoke("IsExhibitionSpec", s));
            s = Spec(); s.duration = 0; Assert.IsFalse(Invoke("IsExhibitionSpec", s));
        }
        static object Struct(string name, params (string, int)[] values)
        {
            var value = Activator.CreateInstance(TypeOf("Reflex.Port.OriginalWorld." + name));
            foreach (var (key, field) in values) value.GetType().GetField(key).SetValue(value, field);
            return value;
        }
        static Array Rows(params (int slot, int local)[] values)
        {
            var a = Array.CreateInstance(TypeOf("Reflex.Port.OriginalWorld.ReflexDriverResult"), values.Length);
            for (int i = 0; i < values.Length; i++) a.SetValue(Struct("ReflexDriverResult", ("slot", values[i].slot), ("local", values[i].local)), i);
            return a;
        }
        [Test] public void RosterRequiresExactSlotsAndExactLocalIdentity()
        {
            var roster = new Dictionary<int, bool> { [0] = true, [5] = false };
            Assert.IsTrue(Invoke("MatchesRoster", roster, Rows((5, 0), (0, 1)), 2, 2));
            Assert.IsFalse(Invoke("MatchesRoster", roster, Rows((4, 0), (0, 1)), 2, 2));
            Assert.IsFalse(Invoke("MatchesRoster", roster, Rows((5, 1), (0, 0)), 2, 2));
            Assert.IsFalse(Invoke("MatchesRoster", roster, Rows((0, 1), (0, 1)), 2, 2));
            Assert.IsFalse(Invoke("MatchesRoster", roster, Rows((0, 1)), 1, 2));
        }
        [TestCase(5, 0)] [TestCase(6, 1)] [TestCase(12, 1)] [TestCase(17, 1)] [TestCase(19, 1)]
        public void TheOtherModesOwnTheirPostWithoutLaps(int activity, int laps)
        {
            var race = Struct("ReflexRaceState", ("valid", 1), ("state", 4), ("num_laps", laps), ("riders", 3));
            var ui = new RxEventUiState { known = RxEventUiKnown.ActivityType | RxEventUiKnown.RaceState | RxEventUiKnown.RulesHold | RxEventUiKnown.PostState, activity_type = activity, race_state = 4, rules_hold = 0, post_state = 2 };
            Assert.IsTrue(Invoke("OwnsPost", true, race, ui));
            // a lap race needs its laps (Freestyle and the Section Race run on a clock); the Free Ride and a challenge have no results page of their own
            var none = Struct("ReflexRaceState", ("valid", 1), ("state", 4), ("num_laps", 0), ("riders", 3));
            Assert.AreEqual(activity == 5 || activity == 6 || activity == 12, Invoke("OwnsPost", true, none, ui));
            foreach (int other in new[] { 8, 22, 27, 29 }) { ui.activity_type = other; Assert.IsFalse(Invoke("OwnsPost", true, race, ui)); }
        }
        [Test] public void OutroIsOwnedWithoutClaimingReadyResultsAndUnknownPostKeepsExistingOwner()
        {
            var race = Struct("ReflexRaceState", ("valid", 1), ("state", 4), ("num_laps", 1), ("riders", 1));
            var ui = new RxEventUiState { known = RxEventUiKnown.ActivityType | RxEventUiKnown.RaceState | RxEventUiKnown.RulesHold | RxEventUiKnown.PostState, activity_type = 0, race_state = 4, rules_hold = 0 };
            foreach (int post in new[] { 0, 1, 2 }) { ui.post_state = post; Assert.IsTrue(Invoke("OwnsPost", true, race, ui)); }
            foreach (int post in new[] { -1, 3, 99 }) { ui.post_state = post; Assert.IsFalse(Invoke("OwnsPost", true, race, ui)); }
            ui.post_state = 2; Assert.IsFalse(Invoke("OwnsPost", false, race, ui));
            ui.known &= ~RxEventUiKnown.PostState; Assert.IsFalse(Invoke("OwnsPost", true, race, ui));
        }
    }
}

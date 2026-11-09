using System;
using System.Collections;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Reflection;
using NUnit.Framework;

namespace Reflex.Port.RxCore.Tests
{
    // Runtime model lives in Assembly-CSharp; preserve the existing test assembly boundary.
    public class OwnRaceResultsSnapshotTests
    {
        static Type TypeOf(string name) => AppDomain.CurrentDomain.GetAssemblies().Select(a => a.GetType(name)).First(t => t != null);
        static Type Model => TypeOf("Reflex.Port.OpenFrontEnd.OwnRaceResultsSnapshot");
        static object Value(string name) => Activator.CreateInstance(TypeOf("Reflex.Port.OriginalWorld." + name));
        static void Set(object target, string field, object value) => target.GetType().GetField(field).SetValue(target, value);
        static object Get(object target, string property) => target.GetType().GetProperty(property).GetValue(target);
        static object Race(int riders = 2)
        {
            var v = Value("ReflexRaceState");
            Set(v, "valid", 1); Set(v, "state", 4); Set(v, "num_laps", 1); Set(v, "riders", riders);
            return v;
        }
        static RxEventUiState Post(int activity = 0) => new RxEventUiState
        {
            known = RxEventUiKnown.ActivityType | RxEventUiKnown.RaceState | RxEventUiKnown.RulesHold | RxEventUiKnown.PostState,
            activity_type = activity, race_state = 4, post_state = 2, rules_hold = 0
        };
        static object Driver(int slot, bool local, int place, bool finished = true, float time = 146.383987f)
        {
            var d = Value("ReflexDriverResult");
            Set(d, "slot", slot); Set(d, "local", local ? 1 : 0); Set(d, "place", place);
            Set(d, "finished", finished ? 1 : 0); Set(d, "laps_completed", finished ? 1 : 0);
            Set(d, "race_time", time); Set(d, "best_lap", float.MaxValue); Set(d, "last_lap", 0f);
            return d;
        }
        static Array Drivers(params object[] entries)
        {
            var a = Array.CreateInstance(TypeOf("Reflex.Port.OriginalWorld.ReflexDriverResult"), entries.Length);
            for (int i = 0; i < entries.Length; i++) a.SetValue(entries[i], i);
            return a;
        }
        static bool Capture(out object snapshot, object race = null, RxEventUiState? ui = null,
            Array rows = null, int count = 2, bool origin = true, long generation = 3, string identity = "National/NAT_Concord#3",
            Dictionary<int, string> names = null)
        {
            var args = new object[] { generation, identity, origin, race ?? Race(), ui ?? Post(),
                rows ?? Drivers(Driver(5, true, 1), Driver(2, false, 2)), count,
                names ?? new Dictionary<int, string> { [5] = "Local", [2] = "Opponent" }, null, null };
            bool result = (bool)Model.GetMethod("TryCreate").Invoke(null, args);
            snapshot = args[8];
            if (!result) { Assert.IsNull(snapshot); Assert.IsNotEmpty((string)args[9]); }
            return result;
        }
        static object[] Rows(object snapshot) => ((IEnumerable)Get(snapshot, "Rows")).Cast<object>().ToArray();
        static string Format(float f) => (string)Model.GetMethod("FormatTime").Invoke(null, new object[] { f });

        [TestCase(0)] [TestCase(1)] [TestCase(17)] [TestCase(19)]
        public void ExplicitOfflineOriginAndCompletePost2CreateNativeResults(int activity)
        {
            Assert.IsTrue(Capture(out var snapshot, ui: Post(activity)));
            Assert.AreEqual(3L, Get(snapshot, "Generation")); Assert.AreEqual("National/NAT_Concord#3", Get(snapshot, "EventIdentity"));
            Assert.AreEqual("02:26.384", Get(Rows(snapshot)[0], "Time"));
            Assert.IsFalse(Capture(out _, origin: false));
        }
        [TestCase(0)] [TestCase(-1)]
        public void MissingGenerationIsRejected(long generation) => Assert.IsFalse(Capture(out _, generation: generation));
        [TestCase(null)] [TestCase("")] [TestCase(" ")]
        public void MissingEventIdentityIsRejected(string identity) => Assert.IsFalse(Capture(out _, identity: identity));
        [TestCase(RxEventUiKnown.ActivityType)] [TestCase(RxEventUiKnown.RaceState)]
        [TestCase(RxEventUiKnown.RulesHold)] [TestCase(RxEventUiKnown.PostState)]
        public void UnknownNativeFieldsCannotSelectResults(RxEventUiKnown missing)
        { var ui = Post(); ui.known &= ~missing; Assert.IsFalse(Capture(out _, ui: ui)); }
        [TestCase(0)] [TestCase(1)] [TestCase(3)] [TestCase(5)]
        public void OtherLifecycleStatesKeepExistingOwner(int state)
        { var race = Race(); Set(race, "state", state); Assert.IsFalse(Capture(out _, race: race)); }
        // the timed modes and the Waypoint race have no laps to finish and no `num_laps` to count: their standings are enough
        [TestCase(5)] [TestCase(6)] [TestCase(12)]
        public void TheOtherModesCreateResultsWithoutLaps(int activity)
        {
            var race = Race(); Set(race, "num_laps", 0);
            var rows = Drivers(Driver(5, true, 1, finished: false), Driver(2, false, 2, finished: false));
            Assert.IsTrue(Capture(out var snapshot, race: race, ui: Post(activity), rows: rows));
            Assert.AreEqual(2, Rows(snapshot).Length);
        }
        [Test] public void MatchingPostStateIsRequiredAndRulesCannotBePending()
        {
            var ui = Post(); ui.post_state = 1; Assert.IsFalse(Capture(out _, ui: ui));
            ui = Post(); ui.race_state = 3; Assert.IsFalse(Capture(out _, ui: ui));
            ui = Post(); ui.rules_hold = 1; Assert.IsFalse(Capture(out _, ui: ui));
            ui = Post(2); Assert.IsFalse(Capture(out _, ui: ui));    // (an activity with no results page of its own)
            ui = Post(8); Assert.IsFalse(Capture(out _, ui: ui));    // (the Free Ride)
            var race = Race(); Set(race, "valid", 0); Assert.IsFalse(Capture(out _, race: race));
            race = Race(); Set(race, "num_laps", 0); Assert.IsFalse(Capture(out _, race: race));
        }
        [Test] public void IncompleteDuplicateAndInvalidSlotRowsAreRejected()
        {
            Assert.IsFalse(Capture(out _, count: 1));
            Assert.IsFalse(Capture(out _, rows: Drivers(Driver(5, true, 1)), count: 2));
            Assert.IsFalse(Capture(out _, rows: Drivers(Driver(5, true, 1), Driver(5, false, 2))));
            Assert.IsFalse(Capture(out _, rows: Drivers(Driver(5, true, 1), Driver(-1, false, 2))));
        }
        [Test] public void ExactlyOneLocalCompletedRiderIsRequired()
        {
            Assert.IsFalse(Capture(out _, rows: Drivers(Driver(5, false, 1), Driver(2, false, 2))));
            Assert.IsFalse(Capture(out _, rows: Drivers(Driver(5, true, 1), Driver(2, true, 2))));
            Assert.IsFalse(Capture(out _, rows: Drivers(Driver(5, true, 1, false), Driver(2, false, 2))));
            var race = Race(); Set(race, "num_laps", 2); Assert.IsFalse(Capture(out _, race: race));
        }
        [Test] public void SnapshotCopiesRowsNamesAndStateRatherThanWorkerReferences()
        {
            var race = Race(); var ui = Post(); var rows = Drivers(Driver(5, true, 1), Driver(2, false, 2));
            var names = new Dictionary<int, string> { [5] = "Local", [2] = "Opponent" };
            Assert.IsTrue(Capture(out var snapshot, race: race, ui: ui, rows: rows, names: names));
            rows.SetValue(Driver(5, true, 9), 0); names[5] = "Changed"; Set(race, "state", 2); ui.post_state = 1;
            var first = Rows(snapshot)[0]; Assert.AreEqual(1, Get(first, "Place")); Assert.AreEqual("Local", Get(first, "Name"));
            Assert.AreEqual(4, Get(snapshot, "Race").GetType().GetField("state").GetValue(Get(snapshot, "Race")));
            Assert.AreEqual(2, ((RxEventUiState)Get(snapshot, "EventUi")).post_state);
            var nativeCopy = Get(first, "Native"); Set(nativeCopy, "place", 99); Assert.AreEqual(1, Get(first, "Place"));
            Assert.Throws<NotSupportedException>(() => ((IList)Get(snapshot, "Rows"))[0] = first);
        }
        [Test] public void NativeRankIsNotRecalculatedFromTimeAndTiesUseStableSlots()
        {
            var a = Driver(5, true, 2, true, 10); var b = Driver(2, false, 1, true, 200);
            Assert.IsTrue(Capture(out var snapshot, rows: Drivers(a, b))); Assert.AreEqual(2, Get(Rows(snapshot)[0], "Slot"));
            Set(a, "place", 1); Assert.IsTrue(Capture(out snapshot, rows: Drivers(a, b)));
            Assert.AreEqual(2, Get(Rows(snapshot)[0], "Slot")); Assert.AreEqual(5, Get(Rows(snapshot)[1], "Slot"));
            Set(b, "place", 0); Assert.IsTrue(Capture(out snapshot, rows: Drivers(a, b))); Assert.AreEqual("—", Get(Rows(snapshot)[1], "Rank"));
        }
        [TestCase(-1)] [TestCase(0)] [TestCase(3)] [TestCase(int.MaxValue)]
        public void InvalidNativeRankIsUnavailableWithoutChangingRawPlace(int invalidPlace)
        {
            // Two riders: even a positive value above two is unavailable, not an official rank.
            // A faster clock cannot promote the invalid row ahead of the valid native rank.
            var invalid = Driver(2, false, invalidPlace, true, 1);
            var valid = Driver(5, true, 2, true, 200);
            Assert.IsTrue(Capture(out var snapshot, rows: Drivers(invalid, valid)));
            var rows = Rows(snapshot);
            Assert.AreEqual(5, Get(rows[0], "Slot")); Assert.AreEqual("2", Get(rows[0], "Rank"));
            Assert.AreEqual(2, Get(rows[1], "Slot")); Assert.AreEqual("—", Get(rows[1], "Rank"));
            Assert.AreEqual(invalidPlace, Get(rows[1], "Place"));
            Assert.AreEqual(invalidPlace, Get(rows[1], "Native").GetType().GetField("place").GetValue(Get(rows[1], "Native")));
        }
        [Test] public void MixedUnavailableRanksUseSlotOrderInsteadOfRawInvalidValues()
        {
            // Positive overflow rank must share the same unavailable bucket as zero/negative ranks.
            Assert.IsTrue(Capture(out var snapshot, race: Race(3), count: 3,
                rows: Drivers(Driver(9, true, 99), Driver(5, false, 0), Driver(2, false, -1))));
            var rows = Rows(snapshot);
            CollectionAssert.AreEqual(new[] { 2, 5, 9 }, rows.Select(row => (int)Get(row, "Slot")).ToArray());
            foreach (var row in rows) Assert.AreEqual("—", Get(row, "Rank"));
        }
        [Test] public void UnfinishedClockIsNotAResultAndNamesHaveHonestFallbacks()
        {
            Assert.IsTrue(Capture(out var snapshot, rows: Drivers(Driver(5, true, 1), Driver(2, false, 2, false, 50)), names: new Dictionary<int, string>()));
            var rows = Rows(snapshot); Assert.AreEqual("You", Get(rows[0], "Name")); Assert.AreEqual("Rider 3", Get(rows[1], "Name"));
            Assert.AreEqual("NOT FINISHED", Get(rows[1], "Status")); Assert.AreEqual("—", Get(rows[1], "Time"));
            Assert.AreEqual("—", Get(rows[0], "BestLap")); Assert.AreEqual("—", Get(rows[0], "LastLap"));
        }
        [Test] public void MissingTimeDoesNotInvalidateNativeFinishedFlag()
        {
            Assert.IsTrue(Capture(out var snapshot, rows: Drivers(Driver(5, true, 1, true, float.NaN), Driver(2, false, 2))));
            Assert.AreEqual("FINISHED", Get(Rows(snapshot)[0], "Status")); Assert.AreEqual("—", Get(Rows(snapshot)[0], "Time"));
        }
        // w-wpfill: the game table's cells of every mode reach the rows (the Waypoint's projected TOTAL of a rider still racing, any column
        // by its title), and the Eliminator's place text follows the original's rule (1 / 2 / 3 / any other: 21 is "21TH"; the leader's
        // cell stays empty)
        [Test] public void ResultsPageCellsReachEveryModesRows()
        {
            var race = Race(); Set(race, "num_laps", 0);
            var rows = Drivers(Driver(5, true, 1, finished: true, time: 61f), Driver(2, false, 2, finished: false));
            Assert.IsTrue(Capture(out var snapshot, race: race, ui: Post(6), rows: rows));
            var pageType = TypeOf("Reflex.Port.RxCore.RxResultsPage"); var rowType = TypeOf("Reflex.Port.RxCore.RxResultsPage+Row");
            var page = Activator.CreateInstance(pageType);
            pageType.GetField("populated").SetValue(page, 1u); pageType.GetField("activityType").SetValue(page, 6); pageType.GetField("columns").SetValue(page, 1);
            ((string[])pageType.GetField("titles").GetValue(page))[0] = "STR_ACTIVITYRESULTSCOLUMN_TOTALTIME";
            var pr = Array.CreateInstance(rowType, 2);
            for (int i = 0; i < 2; i++)
            {
                var r = Activator.CreateInstance(rowType);
                rowType.GetField("index").SetValue(r, i); rowType.GetField("slot").SetValue(r, i == 0 ? 5 : 2);
                ((string[])rowType.GetField("cells").GetValue(r))[0] = i == 0 ? "01:01.00" : "01:09.50";
                ((float[])rowType.GetField("sort").GetValue(r))[0] = i == 0 ? -61f : -69.5f;
                pr.SetValue(r, i);
            }
            pageType.GetField("rows").SetValue(page, pr);
            Assert.IsTrue((bool)Model.GetMethod("ApplyResultsPage").Invoke(snapshot, new[] { page }));
            var second = Rows(snapshot)[1];
            Assert.AreEqual(Format(69.5f), Get(second, "Time"));
            var args = new object[] { "TOTALTIME", null, null };
            Assert.IsTrue((bool)second.GetType().GetMethod("TryPageCell").Invoke(second, args));
            Assert.AreEqual("01:09.50", args[1]); Assert.AreEqual(-69.5f, args[2]);
            var place = TypeOf("Reflex.Port.OpenFrontEnd.OwnModes").GetMethod("EliminatedPlace", BindingFlags.NonPublic | BindingFlags.Static);
            Assert.AreEqual("21TH", place.Invoke(null, new object[] { 21f, "21TH" }));
            Assert.AreEqual("2ND", place.Invoke(null, new object[] { 2f, "2ND" }));
            Assert.AreEqual("", place.Invoke(null, new object[] { 0f, "" }));
        }
        [Test] public void TimeFormattingRejectsAbsenceAndHugeValuesWithoutOverflow()
        {
            foreach (float f in new[] { 0f, -1f, float.NaN, float.PositiveInfinity, float.NegativeInfinity, float.MaxValue, 1e30f })
                Assert.AreEqual("—", Format(f), f.ToString(CultureInfo.InvariantCulture));
            Assert.AreEqual("00:01.250", Format(1.25f)); Assert.AreEqual("01:00.000", Format(59.9996f));
            var old = CultureInfo.CurrentCulture;
            try { CultureInfo.CurrentCulture = CultureInfo.GetCultureInfo("fr-FR"); Assert.AreEqual("02:26.384", Format(146.383987f)); }
            finally { CultureInfo.CurrentCulture = old; }
        }
    }
}

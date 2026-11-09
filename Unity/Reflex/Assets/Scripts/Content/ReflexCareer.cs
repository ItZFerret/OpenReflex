using System;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using UnityEngine;

namespace Reflex.Port.Content
{
    // A MOTOCAREER race (native/drafts/world_runtime/career.h, _re/notes/career.md): the front end's choice of a Tour series
    // (RaceAndRideEvents:OnAcceptSeries + StartRacePopup:End) handed to the race scene, and its end.
    //   world     ReflexWorldFiles.For gives the series event a career world: the game booted with the profile signed in from the
    //             current save, then LoadSeries / CommitAndInitializeSeries / StartEvent / GotoLoadingPage (career::start_series_event).
    //             The world carries the profile (tables, series progress), so it is made from exactly one save (its name has the
    //             save's digest) and made again once a race changed the save.
    //   end       after the race's post state, the post-race CONTINUE runs the game's own completion in that world
    //             (reflex_world_career_finish: CompleteSeriesEvent, CompleteSeries -> PlayerSeriesStatus, unlocks and the save request,
    //             the saved-game worker's write of savegame.bxml), then the front end comes back on the series' events page
    //             (CareerFrontEndLoadingPage -> GotoCareer -> FrontEndRoutingMessage "GotoRaceAndRideEvents").
    public static class ReflexCareer
    {
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate int reflex_world_career_finish__fn(IntPtr world, string remoteDir, byte[] output, int size);
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_career_finish__fn> reflex_world_career_finish__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_career_finish__fn>("reflex_world_career_finish");
        static int reflex_world_career_finish(IntPtr world, string remoteDir, byte[] output, int size) => reflex_world_career_finish__p.Get()(world, remoteDir, output, size);
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate IntPtr reflex_world_error__fn(IntPtr world);
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_error__fn> reflex_world_error__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_error__fn>("reflex_world_error");
        static IntPtr reflex_world_error(IntPtr world) => reflex_world_error__p.Get()(world);
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate int reflex_world_career__fn(IntPtr world, string command, byte[] output, int size);
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_career__fn> reflex_world_career__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_career__fn>("reflex_world_career");
        static int reflex_world_career(IntPtr world, string command, byte[] output, int size) => reflex_world_career__p.Get()(world, command, output, size);

        public sealed class Race
        {
            public int seriesId, vehicleTypeId, parentStoreProductId;
            public string eventKey;   // the series' event of ReflexEvents (type / location / track)
            public string stamp;      // ReflexProfile.SaveStamp() when the race was chosen
            // Heat / Moto: the CareerPracticePopup answer (-1 not asked, 0 practice, 1 skip) and the outcomes of the series' races
            // already run (reflex_world_career "outcome", hex), replayed by the boot that builds the world of the next race.
            public int skipPractice = -1;
            public readonly System.Collections.Generic.List<string> outcomes = new System.Collections.Generic.List<string>();
            /// The player's gate at each gate selection of the series so far (the Gate Selection page, OriginalGateSelection; -1 the page's
            /// default, which the boot takes by itself). WP-5.
            public readonly System.Collections.Generic.List<int> gates = new System.Collections.Generic.List<int>();
            public bool HasGateChoices => gates.Exists(g => g >= 0);
            /// The world-name part of this race (the series, the save, the practice answer and the races run before it).
            public string WorldName
            {
                get
                {
                    string n = "Career_s" + seriesId + "_" + stamp + (skipPractice >= 0 ? "_p" + skipPractice : "");
                    if (outcomes.Count > 0)
                    {
                        using var sha = System.Security.Cryptography.SHA1.Create();
                        var h = sha.ComputeHash(Encoding.ASCII.GetBytes(string.Join(",", outcomes)));
                        n += "_r" + outcomes.Count + "_" + BitConverter.ToString(h, 0, 4).Replace("-", "").ToLowerInvariant();
                    }
                    if (HasGateChoices) n += "_g" + string.Join(".", gates.ConvertAll(g => g < 0 ? "d" : g.ToString(System.Globalization.CultureInfo.InvariantCulture)));   // the Gate Selection page's choices (career_gates.bin; d = the default)
                    return n;
                }
            }
            /// The outcomes as the build reads them (<save>/career_outcomes.bin: the words of each, little-endian, concatenated).
            public byte[] OutcomeBytes()
            {
                var all = new System.Collections.Generic.List<byte>();
                foreach (var hex in outcomes)
                    for (int i = 0; i + 8 <= hex.Length; i += 8) all.AddRange(BitConverter.GetBytes(Convert.ToUInt32(hex.Substring(i, 8), 16)));
                return all.ToArray();
            }
            public override string ToString() => $"series {seriesId} ({eventKey}), vehicle type {vehicleTypeId}, save {stamp}" + (skipPractice >= 0 ? ", skip practice " + skipPractice : "") + (outcomes.Count > 0 ? ", race " + (outcomes.Count + 1) : "");
        }

        static int Field(string json, string key)
        {
            int i = json.IndexOf("\"" + key + "\":", StringComparison.Ordinal); if (i < 0) return -1;
            i += key.Length + 3; int j = i; while (j < json.Length && (char.IsDigit(json[j]) || json[j] == '-')) j++;
            return int.TryParse(json.Substring(i, j - i), out int v) ? v : -1;
        }

        /// The in-race career pages' calls on the running career world (career::world_command; between its frames or with them
        /// stopped): "status", "rules_done <0|1>", "results", "moto_results", "outcome <flags>". Null on error (Debug log).
        public static string WorldCommand(IntPtr world, string command)
        {
            if (world == IntPtr.Zero) return null;
            try
            {
                var buf = new byte[1 << 14];
                int n = reflex_world_career(world, command, buf, buf.Length);
                if (n < -1) { buf = new byte[-n + 16]; n = reflex_world_career(world, command, buf, buf.Length); }
                if (n < 0) { Debug.LogWarning("[Career] " + command + ": " + Marshal.PtrToStringAnsi(reflex_world_error(world))); return null; }
                return Encoding.UTF8.GetString(buf, 0, n);
            }
            catch (Exception e) { Debug.LogWarning("[Career] " + command + ": " + e.Message); return null; }
        }

        /// The career race the race scene is running (null: arcade).
        public static Race Current;
        /// The series group (Series parent store product) the events page shows (App.GetUIVariableInt "SeriesStoreProductId", kept across
        /// the race scene).
        public static int SeriesStoreProductId = -1;
        /// The game's report of the last finished career event (finish_event JSON), for the front end.
        public static string LastResult;
        /// The vehicle the last completed series rewarded (CareerFrontEndLoadingPage:DoneLoading: the first purchase reward the player
        /// did not own that is a VehicleModel's StoreAssetId; -1 none), for the front end's Vehicle Reward Page.
        public static int RewardVehicleAsset = -1, RewardVehicleModel = -1;

        /// The series' event (SeriesEventLocations EventNumber 1 + Series.EventTypeId) among the events the race scene runs.
        public static ReflexEvents.Event EventOf(int seriesId, Func<string, int, string, int> lookup)
        {
            int typeId = lookup("Series", seriesId, "EventTypeId"), location = lookup("SeriesEventLocations", seriesId, "EventLocationId"), track = lookup("SeriesEventLocations", seriesId, "TrackId");
            if (track < 0) track = 0;
            return ReflexEvents.All.FirstOrDefault(e => e.spec.typeId == typeId && e.spec.locationId == location && e.spec.trackId == track);
        }

        /// The post-race CONTINUE of a career race: the game completes the series event and saves, in the race's world (its frames
        /// stopped). Returns the game's JSON or null (Debug log).
        public static string Finish(IntPtr world)
        {
            if (Current == null || world == IntPtr.Zero) return null;
            var race = Current; Current = null;
            try
            {
                var buf = new byte[1 << 14];
                int n = reflex_world_career_finish(world, ReflexProfile.RemoteDir, buf, buf.Length);
                if (n < -1) { buf = new byte[-n + 16]; n = reflex_world_career_finish(world, ReflexProfile.RemoteDir, buf, buf.Length); }
                if (n < 0) { Debug.LogWarning("[Career] the series event was not completed: " + Marshal.PtrToStringAnsi(reflex_world_error(world))); return null; }
                LastResult = Encoding.UTF8.GetString(buf, 0, n);
                RewardVehicleAsset = Field(LastResult, "rewardVehicleAsset"); RewardVehicleModel = Field(LastResult, "rewardVehicleModel");
                bool saved = LastResult.Contains("\"saved\":0");
                Debug.Log("[Career] " + race + " completed" + (saved ? "" : " without a save (" + Marshal.PtrToStringAnsi(reflex_world_error(world)) + ")") + ": " + LastResult);
                ReflexProfile.Invalidate();   // the next profile session reads the new save
                return LastResult;
            }
            catch (Exception e) { Debug.LogWarning("[Career] finish: " + e.Message); return null; }
        }
    }
}

// What the Race & Ride challenges page shows (RaceAndRideChallenges.lua), read from the install's tables and the player's profile:
//   locations   the Series rows of SeriesOwner "Tour" under the Free Ride group's store product 2904 (ReflexModes.ChallengeSeries): one per Free Ride
//               level (FR_Alpine, FR_Gibson, FR_Cheyenne), locked by IsLocked(StoreProductId), new by IsStoreProductNew, the poster = the product's IconId,
//               LocationNameText = the product's title, LocationImageComponent = the level's Free Ride EventTypeLocations PreviewIconId (EventTypeId 9)
//   challenges  the level's EventTypeLocations rows whose EventType IsChallenge (Free Ride excluded), in table order (UpdateChallengeDisplay), with
//               the player's medal (PlayerChallenges by EventTypeLocationId: Gold, else Silver, else Bronze, each >= 1) and the stored best value (Param)
//   criteria    the medal thresholds PreChallengePopup lists (ReflexChallengeSpecs: the level's MXChallengesSpecEntity), formatted as the popup does
// A challenge starts through the world the port boots for it (ReflexModes.Challenges: "<Activation>/<level>"; Hit Discovery is the series' Free Ride).
using System;
using System.Collections.Generic;
using System.Linq;
using Reflex.Port.Content;
using Reflex.Port.FrontEnd;
using UnityEngine;

namespace Reflex.Port.OpenFrontEnd
{
    internal sealed class OpenChallenge
    {
        public int eventTypeLocationId, eventTypeId, kind;   // kind: OwnPostChallenge's (1 Hill Climb, 6 Trailblazer, 7 Hit Discovery, 8 Target Practice)
        public string activity, title, description;
        /// 0 none, 1 bronze, 2 silver, 3 gold (PlayerChallenges: Gold >= 1, else Silver >= 1, else Bronze >= 1).
        public int medal;
        public bool hasRecord; public float record;
        /// Bronze, silver, gold in the kind's unit (null: the level does not say).
        public float[] criteria;
        /// The world that starts it (null: the port has none for this one).
        public ReflexEvents.Event ev;
        public bool IsFreeRide => ev != null && string.IsNullOrEmpty(ev.spec.activation);   // Hit Discovery rides the series' Free Ride
        public string Format(float v) => OwnPostChallenge.Format(kind, v);
    }

    internal sealed class OpenChallengeLocation
    {
        public int seriesId, product, locationId;
        public string level, title;
        public bool locked, isNew;
        public Texture2D poster, photo;
        public ReflexEvents.Event freeRide;
        public readonly List<OpenChallenge> challenges = new List<OpenChallenge>();
        public int Medals => challenges.Count(c => c.medal > 0);
    }

    internal static class OpenChallengeData
    {
        /// The Free Ride group's store product (RaceAndRide: the Free Ride poster; ReflexModes.ChallengeSeries' default).
        public const int GroupProduct = 2904;

        static readonly Dictionary<string, int> Kinds = new Dictionary<string, int>(StringComparer.OrdinalIgnoreCase)
        { ["HillClimb"] = 1, ["TrailBlazer"] = 6, ["Trailblazer"] = 6, ["HitDiscovery"] = 7, ["TargetPractice"] = 8 };
        static readonly Dictionary<int, string> TitleKeys = new Dictionary<int, string>
        { [1] = "HILLCLIMB", [6] = "TRAILBLAZER", [7] = "HITDISCOVERY", [8] = "TARGETPRACTICE" };

        /// The page's Series title ("DYN_HEADER": the group product's title).
        public static string GroupTitle()
        {
            try { return OpenContent.Str(FeTables.Shared.TitleKey(GroupProduct) ?? "STR_RACEANDRIDE", "FREE RIDE").ToUpperInvariant(); }
            catch (Exception) { return "FREE RIDE"; }
        }

        public static List<OpenChallengeLocation> Read(ProfileData p, bool specs = true)
        {
            var list = new List<OpenChallengeLocation>();
            var t = FeTables.Shared;
            var challengeTypes = new Dictionary<int, string>();
            foreach (var r in t.Rows("EventType"))
                if (FeTables.I(r, "IsChallenge", 0) == 1 && FeTables.I(r, "EventTypeId") != 9) challengeTypes[FeTables.I(r, "EventTypeId")] = FeTables.S(r, "ActivityName");
            var player = p?.T("PlayerChallenges");
            foreach (var series in ReflexModes.ChallengeSeries(GroupProduct))
            {
                var loc = new OpenChallengeLocation
                {
                    seriesId = series.seriesId, product = series.storeProductId, locationId = series.locationId, level = series.location,
                    locked = p != null && p.IsLocked(series.storeProductId), isNew = p != null && p.IsNew(series.storeProductId),
                    freeRide = ReflexModes.SeriesFreeRide(series.locationId),
                };
                loc.title = OpenContent.Str(t.TitleKey(series.storeProductId) ?? "", series.location ?? "").ToUpperInvariant();
                var product = t.Rows("StoreProducts").FirstOrDefault(r => FeTables.I(r, "StoreProductId") == series.storeProductId);
                if (product != null) loc.poster = OpenContent.Icon(FeTables.I(product, "IconId"));
                var free = t.Rows("EventTypeLocations").FirstOrDefault(r => FeTables.I(r, "EventLocationId") == series.locationId && FeTables.I(r, "EventTypeId") == 9);
                if (free != null) loc.photo = OpenContent.Icon(FeTables.I(free, "PreviewIconId"));
                var spec = specs ? ReflexChallengeSpecs.For(series.location) : new ReflexChallengeSpecs();
                foreach (var r in t.Rows("EventTypeLocations"))
                {
                    if (FeTables.I(r, "EventLocationId") != series.locationId) continue;
                    int typeId = FeTables.I(r, "EventTypeId");
                    if (!challengeTypes.TryGetValue(typeId, out string activity) || !Kinds.TryGetValue(activity ?? "", out int kind)) continue;   // (Big Air ... : no world in the port)
                    int etl = FeTables.I(r, "EventTypeLocationId");
                    string key = TitleKeys[kind];
                    var c = new OpenChallenge
                    {
                        eventTypeLocationId = etl, eventTypeId = typeId, kind = kind, activity = activity,
                        title = OwnPostChallenge.Str("STR_" + key + "_TITLE", activity).ToUpperInvariant(),
                        description = OwnPostChallenge.Str("STR_" + key + "_DESCRIPTION", ""),
                        ev = ReflexModes.Challenges.FirstOrDefault(e => e.eventTypeLocationId == etl),
                        criteria = kind == 1 ? spec.hillClimb : kind == 6 ? spec.trailblazer : kind == 8 ? spec.targets : spec.discoveries,
                    };
                    int row = player == null ? -1 : player.Find("EventTypeLocationId", etl);
                    if (row >= 0)
                    {
                        c.medal = player.Int("Gold", row, 0) >= 1 ? 3 : player.Int("Silver", row, 0) >= 1 ? 2 : player.Int("Bronze", row, 0) >= 1 ? 1 : 0;
                        float param = player.Float("Param", row);
                        c.hasRecord = param > 0f && param != -1f; c.record = param;
                    }
                    loc.challenges.Add(c);
                }
                list.Add(loc);
            }
            return list;
        }
    }
}

using System;
using System.Collections.Generic;
using System.Linq;

namespace Reflex.Port.Content
{
    // The game modes beyond the races (_re/notes/modes.md; native modes.cpp, boot.h EventSpec mode fields), read from the install's
    // tables as ReflexEvents reads them:
    //   Freestyle      EventType 7 (FMX levels, classes 25 MX Freestyle = VehicleType 3, 24 ATV Freestyle = type 4; Minutes) - an arcade
    //                  type of ReflexEvents once its activity is runnable (EventSelect.lua lists it).
    //   Section Race   EventType 2 (5 levels; the Race & Ride Tour series 13, 52, 73, 79, 86, SeriesFormat "SectionRace").
    //   the Free Ride challenges   EventTypes 24 Hill Climb, 29 Trailblazer, 30 Hit Discovery, 31 Target Practice (IsChallenge) at
    //                  FR_Alpine / FR_Gibson / FR_Cheyenne: the Race & Ride challenges page (RaceAndRideChallenges.lua) starts the Free
    //                  Ride of a challenge series' location (UI SeriesId); in it the game starts a challenge the way its AAA loading page
    //                  does (the pause handler FUN_00598e40 in an activation area): EventType FreeRide, EventTypeActivation = the challenge
    //                  (MXUIManager::SetupEvent 0059a970 takes the activity from it), TrackId = the area's variant. Here a challenge's
    //                  world is booted with exactly those UI variables ("HillClimb/FR_Alpine" ...; ReflexEvents.Find finds them).
    // Every mode world boots with the UI's exclusive device 0 (the title page's controller; the pages' Lua look the player's tables up
    // with it) and the cursor away from the screen (boot.h cursor_off_screen).
    public static class ReflexModes
    {
        /// EventType.ActivityName -> the UI's EventTypeActivation value (the names PreChallengePopup.lua passes to App.GetMXActivityType).
        static readonly Dictionary<string, string> ChallengeActivation = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase)
        {
            ["HillClimb"] = "HillClimb", ["TrailBlazer"] = "Trailblazer", ["HitDiscovery"] = "HitDiscovery", ["TargetPractice"] = "TargetPractice",
        };
        /// The activities whose worlds get the mode fields (boot.h: exclusive device, cursor) and run the game's UI live (OriginalLiveUi).
        public static readonly HashSet<string> ModeActivities = new HashSet<string>(StringComparer.Ordinal) { "Freestyle", "SectionRace" };

        public sealed class Series { public int seriesId, storeProductId, locationId, trackId; public string location; public bool locked; }

        static List<ReflexEvents.Event> _challenges;
        static readonly object gate = new object();

        /// The 12 challenge events (4 types x 3 Free Ride levels), keys "<Activation>/<level>".
        public static IReadOnlyList<ReflexEvents.Event> Challenges
        {
            get
            {
                lock (gate)
                {
                    if (_challenges != null) return _challenges;
                    var t = ReflexEvents.Tables;
                    List<Dictionary<string, object>> T(string n) => t.TryGetValue(n, out var r) ? r : new List<Dictionary<string, object>>();
                    var types = T("EventType").GroupBy(r => Rfx.Int(r, "EventTypeId")).ToDictionary(g => g.Key, g => g.First());
                    var locs = T("EventLocation").GroupBy(r => Rfx.Int(r, "EventLocationId")).ToDictionary(g => g.Key, g => g.First());
                    var free = T("EventTypeLocations").Where(r => Rfx.Int(r, "EventTypeId") == 9).ToList();
                    var list = new List<ReflexEvents.Event>();
                    foreach (var row in T("EventTypeLocations"))
                    {
                        int typeId = Rfx.Int(row, "EventTypeId"), locId = Rfx.Int(row, "EventLocationId");
                        if (!types.TryGetValue(typeId, out var type) || Rfx.Int(type, "IsChallenge", 0) == 0 || !locs.TryGetValue(locId, out var loc)) continue;
                        if (!ChallengeActivation.TryGetValue(Rfx.Str(type, "ActivityName") ?? "", out string act)) continue;
                        string level = Rfx.Str(loc, "Filename");
                        if (string.IsNullOrEmpty(level) || !ReflexInstall.Packages.Has(level)) continue;
                        int track = Rfx.Int(row, "TrackId", 0);
                        // Hit Discovery is Free Ride's own (its collectables are in "<level>FreeRide", its HUD Free Ride's MXUIHitDiscoveredComponent;
                        // FUN_00555e60 has no section for it and booting it as an activation stops the game): the series' Free Ride
                        bool own = act != "HitDiscovery";
                        int engine = ReflexEvents.ActivityIndex(act);
                        var e = new ReflexEvents.Event
                        {
                            key = act + "/" + level + (track > 0 ? "/" + track : ""), typeName = Rfx.Str(type, "Name"), location = level,
                            locationLocId = Rfx.Int(loc, "LocalizationId"), sort = Convert.ToSingle(row.TryGetValue("SortValue", out var sv) ? sv : 0f),
                            eventTypeLocationId = Rfx.Int(row, "EventTypeLocationId"), storeProductId = Rfx.Int(row, "StoreProductId"),
                            tables = ReflexEvents.SourceOf(row) ?? ReflexEvents.DefaultTables,
                            // the sections the game loads with Base: the challenge's own ("<level>ChallengeHillClimbA": its start, finish marker,
                            // flares, targets; FUN_00555e60) - not the Free Ride one
                            sceneSection = ReflexEvents.SceneSection(own && engine >= 0 ? engine : ReflexEvents.ActivityIndex("FreeRide"), level, track),
                            spec = new ReflexWorldFiles.EventSpec
                            {
                                type = "FreeRide", location = level, durationType = "", typeId = 9, locationId = locId, trackId = track, vehicleClass = -1,
                                riders = 1, ai = 0, duration = 0, activity = ReflexEvents.ActivityIndex("FreeRide"),
                                activation = own ? act : "", seriesChallenge = SeriesOfLocation(locId)?.seriesId ?? -1,
                            },
                        };
                        list.Add(e); challengeOf[e.key] = act;
                    }
                    return _challenges = list.OrderBy(e => e.location, StringComparer.Ordinal).ThenBy(e => e.sort).ToList();
                }
            }
        }

        public static ReflexEvents.Event Find(string key) => Challenges.FirstOrDefault(e => string.Equals(e.key, key, StringComparison.OrdinalIgnoreCase));
        /// The challenge (EventTypeActivation) of an event, null for the others.
        public static string ChallengeOf(ReflexEvents.Event e) { if (e == null) return null; var _ = Challenges; lock (gate) return challengeOf.TryGetValue(e.key, out var c) ? c : null; }
        static readonly Dictionary<string, string> challengeOf = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
        /// The Free Ride a challenge's EXIT goes back to (App.RealWorldSwitchToFreeRide): its level's Race & Ride challenges Free Ride
        /// ("HitDiscovery/FR_Alpine": the series' Free Ride, where the activation areas start the other challenges).
        public static string FreeRideOf(string challengeKey)
        {
            var e = Find(challengeKey); if (e == null) return null;
            return Challenges.FirstOrDefault(x => x.location == e.location && string.IsNullOrEmpty(x.spec.activation))?.key
                   ?? ReflexEvents.All.FirstOrDefault(x => x.spec.type == "FreeRide" && x.location == e.location)?.key;
        }
        /// The Race & Ride challenges Free Ride of a location (RaceAndRideChallenges:OnAcceptChallenge).
        public static ReflexEvents.Event SeriesFreeRide(int eventLocationId) => Challenges.FirstOrDefault(x => x.spec.locationId == eventLocationId && string.IsNullOrEmpty(x.spec.activation));
        public static bool IsMode(ReflexEvents.Event e) => e != null && (ModeActivities.Contains(e.spec.type) || ChallengeOf(e) != null);
        /// The open product's Free Ride of a level with a Race & Ride challenges series, and the challenges started in it: they run as the
        /// original's Race & Ride challenges Free Ride, with the UI property InCareerMode (MainMenu:CareerMode sets it before
        /// RaceAndRideChallenges:OnAcceptChallenge starts the Free Ride). MXActivityFreeRide::vf54 (005602f0) is the Free Ride's
        /// "discovery mode" only then: the activation areas are marked (MXUIManager_OnTriggerEnter 005a3b60), the AAA_ markers stay
        /// shown, the found discovery collectables are the profile's. The arcade Free Ride of the original has none of that.
        public static bool InChallengesFreeRide(ReflexEvents.Event e) =>
            e != null && e.spec.type == "FreeRide" && (ChallengeOf(e) != null || SeriesFreeRide(e.spec.locationId) != null);
        /// The challenge an activation area starts (MXUIManager_OnPauseRequest 00598e40: EventTypeActivation = the area's activity,
        /// TrackId = its track): the level's challenge event of that activity and track, null when the port has none.
        public static ReflexEvents.Event ChallengeAt(string level, string activity, int track) =>
            string.IsNullOrEmpty(activity) || string.Equals(activity, "FreeRide", StringComparison.OrdinalIgnoreCase) ? null :
            Challenges.FirstOrDefault(e => e.location == level && e.spec.trackId == track && !string.IsNullOrEmpty(e.spec.activation)
                                           && string.Equals(e.spec.activation, activity, StringComparison.OrdinalIgnoreCase));

        /// The Race & Ride challenges series (RaceAndRideChallenges:Start: Series rows of SeriesOwner "Tour" whose StoreProductId's parent
        /// is the challenges product, 2904 "Free Ride"), with their SeriesEventLocations row.
        public static List<Series> ChallengeSeries(int parentStoreProductId = 2904)
        {
            var t = ReflexEvents.Tables;
            List<Dictionary<string, object>> T(string n) => t.TryGetValue(n, out var r) ? r : new List<Dictionary<string, object>>();
            var products = T("StoreProducts").GroupBy(r => Rfx.Int(r, "StoreProductId")).ToDictionary(g => g.Key, g => g.First());
            var locs = T("EventLocation").GroupBy(r => Rfx.Int(r, "EventLocationId")).ToDictionary(g => g.Key, g => g.First());
            var list = new List<Series>();
            foreach (var s in T("Series"))
            {
                if (Rfx.Str(s, "SeriesOwner") != "Tour") continue;
                int sp = Rfx.Int(s, "StoreProductId");
                if (!products.TryGetValue(sp, out var p) || Rfx.Int(p, "ParentStoreProductId", -1) != parentStoreProductId) continue;
                int id = Rfx.Int(s, "SeriesId");
                var sel = T("SeriesEventLocations").FirstOrDefault(r => Rfx.Int(r, "SeriesId") == id);
                if (sel == null) continue;
                int loc = Rfx.Int(sel, "EventLocationId");
                list.Add(new Series { seriesId = id, storeProductId = sp, locationId = loc, trackId = Rfx.Int(sel, "TrackId", 0), location = locs.TryGetValue(loc, out var l) ? Rfx.Str(l, "Filename") : null });
            }
            return list;
        }
        static List<Series> _series;
        static Series SeriesOfLocation(int locationId) { _series ??= ChallengeSeries(); return _series.FirstOrDefault(s => s.locationId == locationId); }
        /// The Race & Ride challenges series of a Free Ride level (the UI SeriesId RaceAndRideChallenges:OnAcceptChallenge sets), -1 without one.
        public static int ChallengeSeriesOf(int locationId) => SeriesOfLocation(locationId)?.seriesId ?? -1;

        /// The world build's mode fields (ReflexWorldFiles.Build): a mode world boots with the exclusive device and the cursor away.
        public static void ApplyModeFields(ReflexEvents.Event e, ref ReflexWorldFiles.EventSpec spec)
        {
            if (!IsMode(e)) return;
            spec.modes = ReflexWorldFiles.ModesSet;
            spec.exclusiveDevice = 0; spec.cursorOffScreen = 1;
            if (spec.activation == null) spec.activation = "";
            if (ChallengeOf(e) == null) spec.seriesChallenge = -1;
            if (spec.garage != ReflexWorldFiles.GarageSet) { spec.garage = ReflexWorldFiles.GarageSet; spec.riderOwner = spec.vehicleOwner = spec.vehicleType = -1; }
        }
    }
}

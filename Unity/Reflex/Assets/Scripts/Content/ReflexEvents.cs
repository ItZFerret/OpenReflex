using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;

namespace Reflex.Port.Content
{
    // Every single-player event the original front end offers, read from the install's tables as the game merges them
    // (MXTables_Default plus the table data of every DLC database the game finds, see Tables) - EventType, EventLocation,
    // EventTypeLocations, EventTypeVehicleClasses, VehicleClass / VehicleClassType, EventTypeDurations - as ReflexWorldFiles
    // event specs: the engine activity is the index of the type's ActivityName in the executable's activity enum (0x009cb9e8).
    //
    // Which rows the port offers is data-driven by two switches that other work packages flip (then call Refresh):
    //   RunnableActivities    the EventType.ActivityName values whose game mode the port runs (WP-M adds Freestyle, SectionRace ...);
    //   DrawableVehicleTypes  the VehicleType ids the port draws and drives (WP-V adds UTV 11, Super Buggy 12, Pro Lite 14, Pro 2 15).
    // A row is offered when its type's activity is runnable and the type has a vehicle class (EventTypeVehicleClasses) of a
    // RaceableClassFormats format whose every VehicleClassType vehicle type is drawable, or the activity has no rules (Free Ride).
    public static class ReflexEvents
    {
        // Engine activity enum, in order (MXReflex.exe string table at 0x009cb9e8; the pointer table 009cf448 lists the same names).
        static readonly string[] Activities =
        {
            "National", "Supercross", "Opencross", "Supermoto", "Minimoto", "Freestyle", "WaypointRace", "Endurocross", "FreeRide",
            "Tag", "Snake", "Graffiti", "SectionRace", "PokerRace", "Infection", "EggHunt", "Capture", "Terracross", "UTVMoto",
            "Powercross", "FMXJam", "BigAir", "HillClimb", "Jumper", "Pathfinder", "StepUp", "Stoppie", "Trailblazer", "HitDiscovery",
            "TargetPractice", "Wheelie",
        };
        public static IReadOnlyList<string> ActivityNames => Activities;
        public static int ActivityIndex(string activityName) => Array.IndexOf(Activities, activityName);

        // ------------------------------------------------------------------------------------------- the filter (flipped by WP-M / WP-V)

        /// EventType.ActivityName values whose mode the port runs. Mini games (Section Race, Snake, Tag ...), challenges, Freestyle and
        /// Powercross are not in it yet.
        public static readonly HashSet<string> RunnableActivities = new HashSet<string>(StringComparer.Ordinal)
        {
            "National", "Supercross", "WaypointRace", "FreeRide", "Terracross",
            "Powercross", "UTVMoto",   // WP-V: machine-only race types (ReflexVehicles; UTVMoto has no installed location)
            "Freestyle", "SectionRace",   // WP-M (ReflexModes, OriginalLiveUi: the game's own HUD and pages)
        };
        /// VehicleType ids the port draws and drives, in the order an event's classes are listed (and its default class chosen when the
        /// default profile's type has none): 2 MX Lite (the default profile's PlayerVehicle), 3 MX, 4 ATV (ReflexAtv).
        public static readonly List<int> DrawableVehicleTypes = new List<int> { 2, 3, ReflexAtv.VehicleTypeAtv,
            ReflexVehicles.TypeUtv, ReflexVehicles.TypeProLite, ReflexVehicles.TypeSuperBuggy, ReflexVehicles.TypePro2 };   // WP-V: the machines (VehicleSelect's order 11, 14, 12, 15)
        /// VehicleClass.Format values the port races: Single (one vehicle type for every rider), Rival (two types: 10 / 28 MX vs ATV,
        /// 11 MX vs MX Lite, 12 ATV vs MX Lite, 23 Super Buggy vs Pro 2, 26 MX vs ATV Freestyle) and Multi (15 Moto, 16 Machines,
        /// 17 Unlimited). The game itself gives every AI rider its vehicle type in the world boot (WP-V2, _re/notes/vehicles.md "Rival
        /// and Multi classes"): MXUIManager::SetupEvent 0059a970 -> MXUIManager_SetupAIRiders 0059f2d0 -> MXUIManager_BuildAIRoster
        /// 0059f3e0 -> per AI rider MXUIManager_AddAIRiderOfClass 0059ebc0, which draws one of the class's VehicleClassType rows with the
        /// game's generator (0x009cb860) until the rider has an AIRiderVehicle of that type; the port draws each slot's own machine.
        public static readonly HashSet<string> RaceableClassFormats = new HashSet<string>(StringComparer.Ordinal) { "Single", "Rival", "Multi" };
        /// Activities without LocationSelect rules (no vehicle class, AI or duration: LocationSelect sets VehicleClass -1).
        public static readonly HashSet<string> RulelessActivities = new HashSet<string>(StringComparer.Ordinal) { "FreeRide" };

        /// The default profile's vehicle type (PlayerVehicle row 0 -> VehicleModel.VehicleTypeId: 2 MX Lite), LocationSelect's
        /// Player1VehicleTypeId when Vehicle Select left the profile's own.
        public static int DefaultVehicleType
        {
            get
            {
                var t = Tables; List<Dictionary<string, object>> T(string n) => t.TryGetValue(n, out var r) ? r : new List<Dictionary<string, object>>();
                var pv = T("PlayerVehicle").FirstOrDefault();
                return Rfx.Int(T("VehicleModel").FirstOrDefault(r => Rfx.Int(r, "VehicleModelId") == Rfx.Int(pv, "VehicleModelId")), "VehicleTypeId", 2);
            }
        }
        /// A VehicleClass's Name key (STR_VEHICLECLASS_MX_VS_ATV ...), null when the class has no row.
        public static string ClassName(int vehicleClassId) => Rfx.Str(Tables.TryGetValue("VehicleClass", out var r) ? r.FirstOrDefault(x => Rfx.Int(x, "VehicleClassId") == vehicleClassId) : null, "Name");
        /// A VehicleClass's Format ("Single", "Rival", "Multi"), null when the class has no row.
        public static string ClassFormat(int vehicleClassId) => Rfx.Str(Tables.TryGetValue("VehicleClass", out var r) ? r.FirstOrDefault(x => Rfx.Int(x, "VehicleClassId") == vehicleClassId) : null, "Format");

        /// Rebuilds All after a filter change.
        public static void Refresh() { lock (gate) { _all = null; _anyLevel = null; _restored = null; } }

        // ------------------------------------------------------------------------------------------- tables, as the game merges them

        public const string DefaultTables = "MXTables_Default";

        /// The DLC databases the game finds: XenonOnlineContentPackage::vf5 (006d3f40) globs `<install>\Database\*DLC*.dx9.database`
        /// (FindFirstFileA / FindNextFileA; the boot's stand-in lists the install folder, boot.cpp) and keeps every name up to its first
        /// '.' in a std::map, so the set is in ordinal name order: ATVDLC01 ... MXTables_DLC003, MXTables_DLC004, MXTables_DLC005,
        /// MXUI_DLC001 ... NAT_DLC05_Dalton ... SX_DLC02_FreightYardB. An install without DLC gives an empty list.
        public static IReadOnlyList<string> DlcDatabases
        {
            get
            {
                lock (gate)
                {
                    if (_dlc != null) return _dlc;
                    var names = new SortedSet<string>(StringComparer.Ordinal);
                    try
                    {
                        foreach (var f in RxFiles.GetFiles(ReflexInstall.Packages.dir))
                        {
                            string n = Path.GetFileName(f);
                            if (!n.EndsWith(".dx9.database", StringComparison.OrdinalIgnoreCase) || n.IndexOf("DLC", StringComparison.OrdinalIgnoreCase) < 0) continue;
                            names.Add(n.Substring(0, n.IndexOf('.')));
                        }
                    }
                    catch (Exception e) when (e is IOException || e is UnauthorizedAccessException) { }
                    return _dlc = names.ToList();
                }
            }
        }

        /// MXTables_Default with the rows of every table data (tbldata) asset of the DLC databases appended to the table of the same
        /// name, database by database in DlcDatabases order (MXTables_DLC004: EventLocation 98-100, EventTypeLocations 285-287,
        /// StoreProducts 3508.., Icon, Localization, DLCUnlocks ...; MXTables_DLC005: 101-103 / 288-290; MXTables_DLC003 is empty).
        /// The profile session (the game itself) confirms the merge: its StoreProducts answers include the DLC rows.
        public static Dictionary<string, List<Dictionary<string, object>>> Tables
        {
            get
            {
                lock (gate)
                {
                    if (_tables != null) return _tables;
                    var inst = ReflexInstall.Packages;
                    var t = Rfx.Tables(inst.Read(DefaultTables, DefaultTables, DefaultTables, "tbldata"));
                    var origin = new Dictionary<Dictionary<string, object>, string>(RowIdentity.Instance);
                    foreach (var rows in t.Values) foreach (var r in rows) origin[r] = DefaultTables;
                    foreach (string dbName in DlcDatabases)
                    {
                        Rfx.Database db;
                        try { db = inst.Db(dbName); }
                        catch (Exception e) { UnityEngine.Debug.LogWarning($"DLC database {dbName} not read (skipped: the game runs without it): {e.Message}"); continue; }
                        if (db == null) continue;
                        foreach (var pkg in db.packages.Values.OrderBy(p => p.name, StringComparer.Ordinal))
                            foreach (var a in pkg.OfType("tbldata"))
                            {
                                Dictionary<string, List<Dictionary<string, object>>> dt;
                                try { dt = Rfx.Tables(inst.Read(a)); }
                                catch (Exception e) { UnityEngine.Debug.LogWarning($"DLC tables {dbName}/{a.name} not read: {e.Message}"); continue; }
                                foreach (var kv in dt)
                                {
                                    if (!t.TryGetValue(kv.Key, out var list)) t[kv.Key] = list = new List<Dictionary<string, object>>();
                                    Keys.TryGetValue(kv.Key, out string key);
                                    foreach (var r in kv.Value)
                                    {
                                        // (w-gearmods) a community DLC pack put into Database the original way can repeat an id (Distractions' and the
                                        // Jett Lawrence pack's StoreProductId 3650 is both their unused line and the helmet) or reuse one of the game's:
                                        // one row per id, as the profile's unlock checks require (CoreGarageUnlocks: "Duplicate installed store
                                        // product"); of a pack's own repeat the row that is a piece of a line wins (the one its asset names)
                                        int id = key == null ? int.MinValue : Rfx.Int(r, key, int.MinValue);
                                        int at = id == int.MinValue ? -1 : list.FindIndex(x => Rfx.Int(x, key, int.MinValue) == id);
                                        if (at >= 0)
                                        {
                                            bool replace = origin.TryGetValue(list[at], out var from) && from == dbName && kv.Key == "StoreProducts"
                                                           && Rfx.Int(r, "ParentStoreProductId", -1) >= 0 && Rfx.Int(list[at], "ParentStoreProductId", -1) < 0;
                                            UnityEngine.Debug.LogWarning($"DLC tables {dbName}: {kv.Key} {key} {id} is already {(from == dbName ? "in this table" : "the game's / " + from + "'s")}: {(replace ? "the line piece kept" : "the repeat skipped")}");
                                            if (replace) { origin.Remove(list[at]); list[at] = r; origin[r] = dbName; }
                                            continue;
                                        }
                                        list.Add(r); origin[r] = dbName;
                                    }
                                }
                            }
                    }
                    _origin = origin;
                    return _tables = t;
                }
            }
        }
        /// The id column of the tables whose rows are one per id (a DLC table's repeat of an id is not merged).
        static readonly Dictionary<string, string> Keys = new Dictionary<string, string>(StringComparer.Ordinal)
        {
            ["StoreProducts"] = "StoreProductId", ["StoreAssets"] = "StoreAssetId", ["RiderGear"] = "RiderGearId", ["VehiclePart"] = "VehiclePartId",
            ["AssetPackage"] = "PackageId", ["AssetDatabase"] = "DatabaseId", ["AssetPackageDatabase"] = "PackageDatabaseId", ["Localization"] = "LocalizationId",
            ["Icon"] = "IconId", ["Brand"] = "BrandId",
        };

        /// The table database a merged row came from (DefaultTables, "MXTables_DLC004" ...), or null for a row not from Tables.
        public static string SourceOf(Dictionary<string, object> row) { var _ = Tables; return row != null && _origin.TryGetValue(row, out var s) ? s : null; }
        /// A row of the DLC tables.
        public static bool IsDlcRow(Dictionary<string, object> row) { string s = SourceOf(row); return s != null && s != DefaultTables; }

        sealed class RowIdentity : IEqualityComparer<Dictionary<string, object>>
        {
            public static readonly RowIdentity Instance = new RowIdentity();
            public bool Equals(Dictionary<string, object> a, Dictionary<string, object> b) => ReferenceEquals(a, b);
            public int GetHashCode(Dictionary<string, object> o) => System.Runtime.CompilerServices.RuntimeHelpers.GetHashCode(o);
        }

        // ------------------------------------------------------------------------------------------- scene sections

        /// The activity's section of `<level>.dx9.scene` the game loads with the Base section: FUN_00555e60 (EAX activity, buffer,
        /// level, track / variant): race types "<level>Exhibition<Activity>[<n>]" (no level has those sections), WaypointRace and
        /// SectionRace "<level><Activity>[<track>]" (track 0 has no suffix: FR_AlpineWaypointRace, FR_AlpineWaypointRace1),
        /// FreeRide "<level>FreeRide", the mini games "<level>MiniGame<Activity>[<n>]" (Infection uses Tag's: the pointer at 009cf46c),
        /// Greed / CTF "<level>MiniGameGreed|CTF[<n>]", BigAir / HillClimb / Pathfinder / Trailblazer "<level>Challenge<Activity><'A'+n>",
        /// the other challenges "<level>Challenge<Activity>".
        public static string SceneSection(int activity, string level, int track)
        {
            if (activity < 0 || activity >= Activities.Length) return null;
            string a = Activities[activity], n = track != 0 ? track.ToString(System.Globalization.CultureInfo.InvariantCulture) : "";
            switch (activity)
            {
                case 0: case 1: case 2: case 3: case 4: case 5: case 7: case 0x11: case 0x12: case 0x13: case 0x14: return level + "Exhibition" + a + n;
                case 6: case 0xc: return level + a + n;
                case 8: return level + a;
                case 9: case 10: case 0xb: return level + "MiniGame" + a;
                case 0xd: return level + "MiniGame" + a + n;
                case 0xe: return level + "MiniGame" + Activities[9];
                case 0xf: return level + "MiniGameGreed" + n;
                case 0x10: return level + "MiniGameCTF" + n;
                case 0x15: case 0x16: case 0x18: case 0x1b: return level + "Challenge" + a + (char)('A' + track);
                default: return level + "Challenge" + a;
            }
        }

        /// The section names of `<level>.dx9.scene` (empty when the scene is missing or unreadable).
        public static List<string> SceneSectionsOf(string level)
        {
            try { return ReflexScene.SectionNames(RxFiles.ReadAllBytes(TrackMods.LevelFile(level, ".dx9.scene"))); }
            catch (Exception) { return new List<string>(); }   // missing or unreadable scene (the Ctrl_* scenes)
        }

        // ------------------------------------------------------------------------------------------- events

        public sealed class Event
        {
            public string key;            // "<ActivityName>/<location>[/<TrackId>]", e.g. "National/NAT_Concord", "WaypointRace/FR_Alpine/1"
            public string typeName;       // table name, e.g. "Free Ride"
            public string location;       // EventLocation Filename = level (Database) name
            public int locationLocId;     // EventLocation LocalizationId (display name in the string tables)
            public float sort;
            public ReflexWorldFiles.EventSpec spec;
            public int eventTypeLocationId, storeProductId;
            /// The table database of the event's EventTypeLocations row (DefaultTables or a DLC's, e.g. "MXTables_DLC004").
            public string tables = DefaultTables;
            public bool Dlc => tables != DefaultTables;
            /// The scene section the game loads with Base for this activity / track (SceneSection), e.g. "FR_AlpineWaypointRace1".
            public string sceneSection;
            /// sceneSection after the level name: OriginalLevel.activity ("WaypointRace1", "FreeRide", "ExhibitionNational").
            public string SceneActivity => sceneSection != null && sceneSection.StartsWith(location, StringComparison.Ordinal) ? sceneSection.Substring(location.Length) : spec.type;
            /// The event type's raceable vehicle classes (EventTypeVehicleClasses of a RaceableClassFormats format whose vehicle types are
            /// all DrawableVehicleTypes): the Single classes first in DrawableVehicleTypes order (3 MX Lite, 2 MX, 1 ATV for the race types),
            /// then the Rival / Multi ones in the same order (National: 11, 12, 15, 10).
            public int[] vehicleClasses = new int[0];
            /// LocationSelect:BuildVehicleClassList for a vehicle type (Player1VehicleTypeId): the raceable classes containing it, sorted by
            /// their number of vehicle types with the game's Lua table.sort (LocationSelect_VehicleClassSort; FeTables.LuaSort keeps the
            /// game's order of ties: an ATV at a National gets 1 ATV, 12 ATV vs MX Lite, 10 MX vs ATV, 15 Moto). The first is the default.
            public int[] ClassesFor(int vehicleTypeId)
            {
                var l = vehicleClasses.OrderBy(c => c).Where(c => ReflexAtv.TypesOfClass(c).Contains(vehicleTypeId)).Select(c => (c, n: ReflexAtv.TypesOfClass(c).Length)).ToList();
                Reflex.Port.FrontEnd.FeTables.LuaSort(l, (a, b) => a.n < b.n);
                return l.Select(x => x.c).ToArray();
            }
            /// The race on a vehicle type in one of its classes (VehicleSelect's first pro with one; vehicleClass -1: LocationSelect's first).
            public ReflexWorldFiles.Garage GarageFor(int vehicleTypeId, int vehicleClass)
            {
                var g = GarageFor(vehicleTypeId);
                if (g != null && vehicleClass >= 0) { if (!ClassesFor(vehicleTypeId).Contains(vehicleClass)) return null; g.vehicleClass = vehicleClass; }
                return g;
            }
            internal bool atv;
            /// A raceable class is an ATV class (VehicleClassType has the ATV type 4).
            public bool Atv => atv;
            /// The race on an ATV (VehicleSelect's first ATV rider and his ATV, LocationSelect's ATV class); null when the type has none.
            public ReflexWorldFiles.Garage AtvGarage() => Atv ? ReflexAtv.DefaultGarage(spec.typeId) : null;
            /// The race on a vehicle type (VehicleSelect's first pro with one, LocationSelect's class for it); null when no raceable class has it.
            public ReflexWorldFiles.Garage GarageFor(int vehicleTypeId) => vehicleClasses.Any(c => ReflexAtv.TypesOfClass(c).Contains(vehicleTypeId)) ? ReflexAtv.DefaultGarage(spec.typeId, vehicleTypeId) : null;
            /// Every raceable class is a machine's (Powercross, UTVMoto): the race needs a machine's garage (the profile owns none).
            public bool MachineOnly => vehicleClasses.Length > 0 && vehicleClasses.All(c => ReflexAtv.TypesOfClass(c).All(x => ReflexVehicles.IsMachineFactory(ReflexVehicles.FactoryOfType(x))));
            /// RESTORATION (w-freerideall, FreeRideAnyLevel): the race event whose level this Free Ride rides on (its location's name,
            /// pictures and store product); null for the original's own events.
            public Event freeRideOf;
            /// MODS/TRACKS (TrackMods): the custom track this event rides (its files bound to `location` while it runs); null for the original's.
            public TrackMod trackMod;
            public bool Custom => trackMod != null;
            /// PORT ADDITION (w-aliveai): the AI race of a converted MX vs ATV Alive level (AliveLevels.Entry.race); its gate has 12 slots.
            public bool aliveRace;
            /// RESTORATION (w-fresnob, Restored): an event on a level cut from the original game that the port restores (no location row).
            public bool restored;
        }

        static readonly object gate = new object();
        static List<string> _dlc;
        static Dictionary<string, List<Dictionary<string, object>>> _tables;
        static Dictionary<Dictionary<string, object>, string> _origin;
        static List<Event> _all;
        static List<Event> _anyLevel;
        static List<Event> _restored;

        /// The events (cached), MX-bike types first.
        public static IReadOnlyList<Event> All
        {
            get
            {
                lock (gate) { if (_all != null) return _all; }
                var sorted = Build(a => RunnableActivities.Contains(a), true);
                lock (gate) return _all ??= sorted;
            }
        }

        /// The events of the EventTypeLocations rows whose activity `accept`s (the runnable ones for All); alive: the converted Alive maps' Free Rides (and the races of those that carry race data) too.
        internal static List<Event> Build(Func<string, bool> accept, bool alive)
        {
            {
                var tables = Tables;
                List<Dictionary<string, object>> T(string n) => tables.TryGetValue(n, out var r) ? r : new List<Dictionary<string, object>>();
                var types = T("EventType").GroupBy(r => Rfx.Int(r, "EventTypeId")).ToDictionary(g => g.Key, g => g.First());
                var locations = T("EventLocation").GroupBy(r => Rfx.Int(r, "EventLocationId")).ToDictionary(g => g.Key, g => g.First());
                var classFormat = T("VehicleClass").GroupBy(r => Rfx.Int(r, "VehicleClassId")).ToDictionary(g => g.Key, g => Rfx.Str(g.First(), "Format") ?? "");
                var classTypes = T("VehicleClassType").GroupBy(r => Rfx.Int(r, "VehicleClassId")).ToDictionary(g => g.Key, g => g.Select(r => Rfx.Int(r, "VehicleTypeId")).ToArray());
                var typeClasses = T("EventTypeVehicleClasses").GroupBy(r => Rfx.Int(r, "EventTypeId")).ToDictionary(g => g.Key, g => g.Select(r => Rfx.Int(r, "VehicleClassId")).Distinct().ToArray());
                var durations = T("EventTypeDurations").GroupBy(r => Rfx.Int(r, "EventTypeId")).ToDictionary(g => g.Key, g => g.First());
                // the default profile's vehicle type (PlayerVehicle row 0 -> VehicleModel.VehicleTypeId: MX Lite), LocationSelect's Player1VehicleTypeId
                var pv = T("PlayerVehicle").FirstOrDefault();
                int defaultType = Rfx.Int(T("VehicleModel").FirstOrDefault(r => Rfx.Int(r, "VehicleModelId") == Rfx.Int(pv, "VehicleModelId")), "VehicleTypeId", 2);
                int Pref(int cls) { var ts = classTypes.TryGetValue(cls, out var a) ? a : new int[0]; return ts.Length == 0 ? int.MaxValue : ts.Min(x => { int i = DrawableVehicleTypes.IndexOf(x); return i < 0 ? int.MaxValue : i; }); }
                bool Raceable(int cls) => classFormat.TryGetValue(cls, out var f) && RaceableClassFormats.Contains(f)
                                          && classTypes.TryGetValue(cls, out var ts) && ts.Length > 0 && ts.All(DrawableVehicleTypes.Contains);
                bool Single(int cls) => classFormat.TryGetValue(cls, out var f) && f == "Single";

                var list = new List<Event>();
                foreach (var row in T("EventTypeLocations"))
                {
                    int typeId = Rfx.Int(row, "EventTypeId"), locId = Rfx.Int(row, "EventLocationId");
                    if (!types.TryGetValue(typeId, out var type) || !locations.TryGetValue(locId, out var loc)) continue;
                    string activity = Rfx.Str(type, "ActivityName");
                    int engine = Array.IndexOf(Activities, activity);
                    if (engine < 0 || !accept(activity)) continue;
                    bool ruleless = RulelessActivities.Contains(activity);
                    var raceable = (typeClasses.TryGetValue(typeId, out var cl) ? cl : new int[0]).Where(Raceable)
                                   .Select((c, i) => (c, i)).OrderBy(x => Single(x.c) ? 0 : 1).ThenBy(x => Pref(x.c)).ThenBy(x => x.i).Select(x => x.c).ToArray();
                    if (!ruleless && raceable.Length == 0) continue;   // a type the port's vehicles cannot race (trucks, buggies, UTVs)
                    // LocationSelect:BuildVehicleClassList's first entry for the default profile's vehicle type (fewest types first: the type's
                    // Single class), else the first raceable class
                    int cls = ruleless ? -1 : raceable.Where(c => classTypes[c].Contains(defaultType)).OrderBy(c => classTypes[c].Length).Select(c => (int?)c).FirstOrDefault() ?? raceable[0];
                    string durType = ""; int dur = 0;
                    if (!ruleless && durations.TryGetValue(typeId, out var d)) { durType = Rfx.Str(d, "ActivityDurationType") ?? ""; dur = Rfx.Int(d, "Min", 0); }
                    string level = Rfx.Str(loc, "Filename");
                    if (string.IsNullOrEmpty(level) || !ReflexInstall.Packages.Has(level)) continue;   // level not installed
                    int track = Rfx.Int(row, "TrackId", 0);   // levels with several layouts of one type (waypoint races: TrackId 0 and 1)
                    list.Add(new Event
                    {
                        key = activity + "/" + level + (track > 0 ? "/" + track : ""), typeName = Rfx.Str(type, "Name"), location = level,
                        locationLocId = Rfx.Int(loc, "LocalizationId"), sort = Convert.ToSingle(row.TryGetValue("SortValue", out var sv) ? sv : 0f),
                        eventTypeLocationId = Rfx.Int(row, "EventTypeLocationId"), storeProductId = Rfx.Int(row, "StoreProductId"),
                        tables = SourceOf(row) ?? DefaultTables, sceneSection = SceneSection(engine, level, track),
                        spec = new ReflexWorldFiles.EventSpec
                        {
                            type = activity, location = level, durationType = durType, typeId = typeId, locationId = locId,
                            trackId = track, vehicleClass = cls, riders = 1, ai = 0, duration = dur, activity = engine,
                        },
                        vehicleClasses = raceable,
                        atv = raceable.Any(c => classTypes[c].Contains(ReflexAtv.VehicleTypeAtv)),
                    });
                }
                // Converted MX vs ATV Alive maps (AliveLevels: Database/alive-levels.json, written by the overlay installer / importer
                // after assembling each level) have no Reflex store product or career progression: each is one Free Ride event.
                int aliveOrder = 0;
                foreach (var alive_ in alive ? AliveLevels.All : new List<AliveLevels.Entry>())
                {
                    string lvl = alive_.level;
                    aliveOrder++;
                    if (!ReflexInstall.Packages.Has(lvl) || list.Any(e => e.location == lvl && e.spec.type == "FreeRide")) continue;
                    list.Add(new Event {
                        key = "FreeRide/" + lvl, typeName = "Free Ride", location = lvl,
                        locationLocId = -1, eventTypeLocationId = -1, storeProductId = -1, sort = 10000 + aliveOrder,
                        tables = "AliveConvertedMaps", sceneSection = SceneSection(8, lvl, 0),
                        spec = new ReflexWorldFiles.EventSpec {
                            type = "FreeRide", location = lvl, durationType = "", typeId = 9,
                            locationId = -1, trackId = 0, vehicleClass = -1, riders = 1, ai = 0, duration = 0, activity = 8,
                        },
                    });
                }
                // PORT ADDITION (w-aliveai, core/plan/ALIVE_RACING.md): the race of a converted Alive level whose install carries its race data
                // (the registry's "race": track definition, lap pacing, the start gate's subscene), as the custom tracks' races (TrackMods.Make):
                // the original's first event of the type lends its rules, classes and durations, at no location row (-1)
                aliveOrder = 0;
                foreach (var alive_ in alive ? AliveLevels.All : new List<AliveLevels.Entry>())
                {
                    aliveOrder++;
                    string lvl = alive_.level, act = alive_.race?.event_type;
                    if (string.IsNullOrEmpty(act) || !ReflexInstall.Packages.Has(lvl) || list.Any(e => e.location == lvl && e.spec.type == act)) continue;
                    int engine = Array.IndexOf(Activities, act);
                    var template = list.FirstOrDefault(e => e.spec.type == act && !e.Dlc && e.vehicleClasses.Length > 0 && e.tables != "AliveConvertedMaps");
                    if (engine < 0 || template == null) continue;
                    list.Add(new Event {
                        key = act + "/" + lvl, typeName = template.typeName, location = lvl,
                        locationLocId = -1, eventTypeLocationId = -1, storeProductId = -1, sort = 10000 + aliveOrder,
                        tables = "AliveConvertedMaps", sceneSection = SceneSection(engine, lvl, 0),
                        spec = new ReflexWorldFiles.EventSpec {
                            type = act, location = lvl, durationType = template.spec.durationType, typeId = template.spec.typeId,
                            locationId = -1, trackId = 0, vehicleClass = template.spec.vehicleClass, riders = 1, ai = 0,
                            duration = template.spec.duration, activity = engine,
                        },
                        vehicleClasses = template.vehicleClasses, atv = template.Atv, aliveRace = true,
                    });
                }
                return list.OrderBy(e => e.spec.activity).ThenBy(e => e.sort).ToList();
            }
        }

        // ------------------------------------------------------------------------------------------- Free Ride on every level

        // The race types whose levels get a Free Ride, in the order that names a level shared by several (NAT_ConcordB: National, not
        // SectionRace; PX_Laguna: Powercross).
        static readonly string[] AnyLevelSources = { "National", "Supercross", "Terracross", "Powercross", "Freestyle", "UTVMoto", "SectionRace", "WaypointRace" };

        /// RESTORATION (w-freerideall; not in the original's menus): one Free Ride event on every level that has a race event but no
        /// Free Ride of its own (the National, Supercross, Terracross, Powercross and Freestyle levels), one per level however many events
        /// share it. The original game's data has a "<level>FreeRide" scene section on 36 of these 38 levels (free ride starts on 33 of them); the
        /// core gives the levels without free ride starts a spawn from their own data (core/game/freeride_any_level.c). Key
        /// "FreeRide/<level>", the Free Ride event type (9, no rules: any vehicle) at the race event's location id; freeRideOf = that
        /// race event. Kept out of All, so the original front end, the career and ForLevel see exactly the original's events.
        public static IReadOnlyList<Event> FreeRideAnyLevel
        {
            get
            {
                lock (gate) { if (_anyLevel != null) return _anyLevel; }
                var all = All;
                var list = new List<Event>();
                var have = new HashSet<string>(all.Where(e => e.spec.type == "FreeRide").Select(e => e.location), StringComparer.OrdinalIgnoreCase);
                int Rank(Event e) { int i = Array.IndexOf(AnyLevelSources, e.spec.type); return i < 0 ? int.MaxValue : i; }
                foreach (var g in all.Where(e => Rank(e) != int.MaxValue && !have.Contains(e.location)).GroupBy(e => e.location, StringComparer.OrdinalIgnoreCase))
                {
                    var src = g.OrderBy(Rank).ThenBy(e => e.spec.trackId).First();
                    list.Add(new Event
                    {
                        key = "FreeRide/" + src.location, typeName = "Free Ride", location = src.location, locationLocId = src.locationLocId,
                        sort = src.sort, eventTypeLocationId = -1, storeProductId = src.storeProductId, tables = src.tables,
                        sceneSection = SceneSection(ActivityIndex("FreeRide"), src.location, 0), freeRideOf = src,
                        spec = new ReflexWorldFiles.EventSpec
                        {
                            type = "FreeRide", location = src.location, durationType = "", typeId = FreeRideTypeId(all),
                            locationId = src.spec.locationId, trackId = 0, vehicleClass = -1, riders = 1, ai = 0, duration = 0, activity = ActivityIndex("FreeRide"),
                        },
                    });
                }
                var sorted = list.OrderBy(e => Rank(e.freeRideOf)).ThenBy(e => e.freeRideOf.spec.activity).ThenBy(e => e.sort).ToList();
                lock (gate) return _anyLevel ??= sorted;
            }
        }
        // ------------------------------------------------------------------------------------------- restored levels

        /// RESTORATION (w-fresnob; the "restored content" pass): levels the original shipped in its install but cut from its menus (no
        /// EventLocation row in any table). level, the name the browser shows (no original localisation string names them), whether it races
        /// (its own track definition: the AI racing line, the gate): SX_Fresno (Fresno, layout A: complete, races like the shipped arenas);
        /// SX_FresnoB (layout B: its package is truncated, the importer repairs it from the install's other levels, FresnoBRepair; its track
        /// definition is lost, so Free Ride only, spawned at its start gate: core/game/freeride_any_level.c).
        static readonly (string level, string name, bool race)[] RestoredLevels = { ("SX_Fresno", "FRESNO", true), ("SX_FresnoB", "FRESNO B", false) };
        /// The display name of a restored level, null for any other.
        public static string RestoredName(string level) => RestoredLevels.Where(r => string.Equals(r.level, level, StringComparison.OrdinalIgnoreCase)).Select(r => r.name).FirstOrDefault();

        /// The restored levels' events (installed ones, never one the tables already list): a Free Ride (type 9, no rules) and, for a level
        /// that races, a Supercross race with the rules, classes and durations of the original's first Supercross event (as the Alive races,
        /// at no location row: -1). Kept out of All (the career and the original front end see the original's events only).
        /// Hidden for now (the user, 2026-11-02): no restored level is offered unless REFLEX_RESTORED_TRACKS=1 (testing).
        static bool RestoredShown => Environment.GetEnvironmentVariable("REFLEX_RESTORED_TRACKS") == "1";
        public static IReadOnlyList<Event> Restored
        {
            get
            {
                if (!RestoredShown) return Array.Empty<Event>();
                lock (gate) { if (_restored != null) return _restored; }
                var all = All;
                var list = new List<Event>();
                int order = 0;
                foreach (var (level, name, race) in RestoredLevels)
                {
                    order++;
                    if (!ReflexInstall.Packages.Has(level) || all.Any(e => string.Equals(e.location, level, StringComparison.OrdinalIgnoreCase))) continue;
                    int fr = ActivityIndex("FreeRide");
                    list.Add(new Event
                    {
                        key = "FreeRide/" + level, typeName = "Free Ride", location = level, locationLocId = -1, eventTypeLocationId = -1, storeProductId = -1,
                        sort = 20000 + order, tables = "Restored", sceneSection = SceneSection(fr, level, 0), restored = true,
                        spec = new ReflexWorldFiles.EventSpec
                        {
                            type = "FreeRide", location = level, durationType = "", typeId = FreeRideTypeId(all), locationId = -1, trackId = 0,
                            vehicleClass = -1, riders = 1, ai = 0, duration = 0, activity = fr,
                        },
                    });
                    var template = race ? all.FirstOrDefault(e => e.spec.type == "Supercross" && !e.Dlc && e.vehicleClasses.Length > 0 && e.tables != "AliveConvertedMaps") : null;
                    if (template == null) continue;
                    int engine = ActivityIndex("Supercross");
                    list.Add(new Event
                    {
                        key = "Supercross/" + level, typeName = template.typeName, location = level, locationLocId = -1, eventTypeLocationId = -1, storeProductId = -1,
                        sort = 20000 + order, tables = "Restored", sceneSection = SceneSection(engine, level, 0), restored = true,
                        spec = new ReflexWorldFiles.EventSpec
                        {
                            type = "Supercross", location = level, durationType = template.spec.durationType, typeId = template.spec.typeId, locationId = -1,
                            trackId = 0, vehicleClass = template.spec.vehicleClass, riders = 1, ai = 0, duration = template.spec.duration, activity = engine,
                        },
                        vehicleClasses = template.vehicleClasses, atv = template.Atv,
                    });
                }
                lock (gate) return _restored ??= list;
            }
        }

        /// The Free Ride EventTypeId (9 in MXTables_Default; read from an original Free Ride event when there is one).
        static int FreeRideTypeId(IReadOnlyList<Event> all) => all.FirstOrDefault(e => e.spec.type == "FreeRide" && e.eventTypeLocationId >= 0)?.spec.typeId ?? 9;

        public static Event Find(string key) => All.FirstOrDefault(e => string.Equals(e.key, key, StringComparison.OrdinalIgnoreCase))
            ?? FreeRideAnyLevel.FirstOrDefault(e => string.Equals(e.key, key, StringComparison.OrdinalIgnoreCase))
            ?? Restored.FirstOrDefault(e => string.Equals(e.key, key, StringComparison.OrdinalIgnoreCase)) ?? ReflexModes.Find(key) ?? ReflexTutorials.Find(key) ?? ReflexGarage.TestDrive.Find(key) ?? TrackMods.FindEvent(key);   // the custom tracks (TrackMods); the Free Ride challenges (ReflexModes), the garage's test drive (ReflexGarage); the MotoSkills tutorials (ReflexTutorials, WP-T)
        /// The first event at a level (the old per-level scenes).
        public static Event ForLevel(string level) => All.FirstOrDefault(e => string.Equals(e.location, level, StringComparison.OrdinalIgnoreCase));
        /// The event of an EventTypeLocations row's (type, location, track), or null when the port does not run it.
        public static Event For(int eventTypeId, int eventLocationId, int trackId) =>
            All.FirstOrDefault(e => e.spec.typeId == eventTypeId && e.spec.locationId == eventLocationId && e.spec.trackId == trackId)
            ?? FreeRideAnyLevel.FirstOrDefault(e => e.spec.typeId == eventTypeId && e.spec.locationId == eventLocationId && e.spec.trackId == trackId);
    }
}

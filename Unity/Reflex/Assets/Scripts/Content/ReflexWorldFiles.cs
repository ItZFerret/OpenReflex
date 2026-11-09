using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Threading.Tasks;

namespace Reflex.Port.Content
{
    // The running original world of a level, made on the player's machine from their own install: libreflexworld boots
    // the unchanged MXReflex.exe inside the interpreter, starts the event through the game's own front-end functions
    // (sign-in, local player, event properties, SetupEvent, StartEvent, GotoLoadingPage, the loading confirmation) and
    // writes the running process as a world image at the start gate / free-ride spawn (native/drafts/world_runtime/boot.h).
    // The files live in the per-user cache and are made once per level (about 40 s); nothing of the game is shipped.
    public sealed class ReflexWorldFiles
    {
        // Bump when the boot or world-image layout changes: older cached worlds are rebuilt. "baton1": a plugin that boots with the baton
        // scheduler and the native ports (ReflexNativeBoot; 2026-10-18, also the CPUID fix); "boot1": an older plugin's exact-slice boots.
        public static string Version => ReflexNativeBoot.PluginBootsBaton ? "baton1" : "boot1";

        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
        public struct EventSpec
        {
            [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 32)] public string type;
            [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 64)] public string location;
            [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 16)] public string durationType;
            public int typeId, locationId, trackId, vehicleClass, riders, ai, duration, activity;
            // Vehicle Select (reflex_world.h: read only when garage == GarageSet): the race's rider / vehicle as VehicleSelect:End
            // equips them (MXTableHelper EquipRider / EquipVehicle; owners 0 Player1, 4 AI, 8 Preset; -1 the profile's).
            public int garage, riderOwner, riderId, riderGearSet, vehicleOwner, vehicleId, vehicleType;
            // MOTOCAREER (reflex_world.h: read when garage == GarageSet and career == CareerSet): a Tour series of the Series table,
            // started with the profile signed in from the save (ReflexCareer, career.h start_series_event).
            public int career, seriesId, seriesVehicleType;
            // Heat / Moto (read when career == CareerSet2): the practice answer and the count of race outcomes in <save>/career_outcomes.bin.
            public int seriesSkipPractice, seriesOutcomes;
            // Game modes (read when garage == GarageSet and modes == ModesSet; ReflexModes.ApplyModeFields): the UI's EventTypeActivation (a Free
            // Ride challenge), the challenges series (UI SeriesId), the exclusive device, the cursor away from the screen.
            public int modes;
            [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 32)] public string activation;
            public int seriesChallenge, exclusiveDevice, cursorOffScreen;
            // WP-G test drive (read when garage == GarageSet and testDrive == ReflexGarage.TestDrive.TestDriveSet; garage.h): the garage's
            // TEST DRIVE in UI_Compound, booted with the profile signed in from the save (ReflexGarage.TestDrive).
            public int testDrive;
        }
        public const int GarageSet = 0x45474152;   // REFLEX_GARAGE_SET
        public const int CareerSet = 0x45524143;   // REFLEX_CAREER_SET
        public const int CareerSet2 = 0x32524143;  // REFLEX_CAREER_SET2 (with the Heat / Moto fields)
        public const int ModesSet = 0x45444f4d;    // REFLEX_MODES_SET (the game-mode fields)

        /// The Vehicle Select page's choice (_re/notes/vehicle-select.md). A rider of -1 owner / a vehicle of -1 owner is left as the
        /// profile has it (EnsureLocalPlayers: PlayerRider LastRiderId / LastEquippedVehicle, the old default); vehicleClass is the
        /// LocationSelect class of the chosen vehicle type (-1: the event's own).
        [Serializable]
        public sealed class Garage
        {
            public const int Player1 = 0, AI = 4, Preset = 8;
            public int riderOwner = -1, riderId, riderGearSet;     // Player1: the player's own rider (0, 0); AI: AIRiderId + its gear set
            public int vehicleOwner = -1, vehicleId;               // Player1: a PlayerVehicleId; AI: an AIVehicleId
            public int vehicleTypeId = -1;                         // UI Player1VehicleTypeId
            public int vehicleClass = -1;                          // VehicleClass (-1: the event's)
            public bool Equips => riderOwner >= 0 || vehicleOwner >= 0;
            /// The world-name part (only for a choice that differs from the default).
            public string Suffix(int eventClass)
            {
                string s = "";
                if (Equips) s += "_vs" + (riderOwner < 0 ? "x" : riderOwner == AI ? "r" + riderId + "g" + riderGearSet : "p" + riderOwner)
                               + (vehicleOwner < 0 ? "x" : (vehicleOwner == AI ? "a" : vehicleOwner == Preset ? "s" : "p" + vehicleOwner + "v") + vehicleId);
                if (vehicleClass >= 0 && vehicleClass != eventClass) s += "_c" + vehicleClass;
                return s;
            }
            public override string ToString() => $"rider owner {riderOwner} id {riderId} gear {riderGearSet}, vehicle owner {vehicleOwner} id {vehicleId}, type {vehicleTypeId}, class {vehicleClass}";
        }

        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate int reflex_world_build__fn(string gameDir, string crtDir, string saveDir, ref EventSpec e, string outPrefix);
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_build__fn> reflex_world_build__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_build__fn>("reflex_world_build");
        static int reflex_world_build(string gameDir, string crtDir, string saveDir, ref EventSpec e, string outPrefix) => reflex_world_build__p.Get()(gameDir, crtDir, saveDir, ref e, outPrefix);
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate float reflex_world_build_progress__fn(byte[] status, int size);
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_build_progress__fn> reflex_world_build_progress__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_build_progress__fn>("reflex_world_build_progress");
        static float reflex_world_build_progress(byte[] status, int size) => reflex_world_build_progress__p.Get()(status, size);
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate IntPtr reflex_world_build_error__fn();
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_build_error__fn> reflex_world_build_error__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_build_error__fn>("reflex_world_build_error");
        static IntPtr reflex_world_build_error() => reflex_world_build_error__p.Get()();

        public readonly ReflexEvents.Event ev;
        public readonly string level, dir, name;
        public string World => Path.Combine(dir, name + "-world.bin");
        public string Deform => Path.Combine(dir, name + "-deform.bin");
        public string DeformModel => Path.Combine(dir, name + "-deform-model.bin");
        public string Log => Path.Combine(dir, name + "-boot.log");
        string Stamp => Path.Combine(dir, name + ".ready");

        public readonly int opponents;   // AI riders (0..RxRiderCap.MaxOpponents: 31; the original's full gate 11)
        public readonly int aiSkill;     // the front end's AIDifficulty for them (AiSkills)

        public readonly int laps;        // EventDuration of a lap race / a Freestyle's minutes (the table's when 0)
        public readonly Garage garage;   // the Vehicle Select choice (null: the profile's rider and bike)
        public readonly ReflexCareer.Race career;   // a MOTOCAREER series event (null: arcade)

        ReflexWorldFiles(ReflexEvents.Event e, int opponents, int aiSkill, int laps, Garage garage = null)
        {
            ev = e; level = e.location; this.opponents = Math.Max(0, Math.Min(Reflex.Port.RxCore.RxRiderCap.MaxOpponents, opponents));
            this.aiSkill = this.opponents > 0 ? aiSkill : 0;
            this.laps = (e.spec.durationType == "Laps" || e.spec.durationType == "Minutes") && laps > 0 && laps != e.spec.duration ? laps : 0;
            name = e.key.Replace('/', '_') + (this.opponents > 0 ? "_ai" + this.opponents + (this.aiSkill != AiSkillNormal ? "_s" + this.aiSkill : "") : "")
                 + (this.laps > 0 ? "_l" + this.laps : "");
            this.garage = garage != null && (garage.Equips || garage.vehicleClass >= 0 && garage.vehicleClass != e.spec.vehicleClass) ? garage : null;
            if (this.garage != null) name += this.garage.Suffix(e.spec.vehicleClass);
            dir = Path.Combine(ReflexInstall.CacheDir, "worlds", Version);
        }
        /// An event ("National/NAT_Concord") or a level (its first event).
        public static ReflexWorldFiles For(string eventOrLevel, int opponents = 0, int aiSkill = AiSkillNormal, int laps = 0, Garage garage = null)
        {
            var e = eventOrLevel.Contains("/") ? ReflexEvents.Find(eventOrLevel) : ReflexEvents.ForLevel(eventOrLevel);
            if (e == null) throw new ArgumentException("no single-player event for " + eventOrLevel + " in the install's tables");
            if (ReflexCareer.Current is ReflexCareer.Race cr && cr.eventKey == e.key) return new ReflexWorldFiles(e, cr);   // MOTOCAREER
            if (e.spec.type == "FreeRide") opponents = 0;   // free ride has no AI
            return new ReflexWorldFiles(e, opponents, aiSkill, laps, garage);
        }
        // A career series event: its world is made from one save (the name carries the save's digest; ReflexCareer).
        ReflexWorldFiles(ReflexEvents.Event e, ReflexCareer.Race race)
        {
            ev = e; level = e.location; career = race;
            name = race.WorldName;
            dir = Path.Combine(ReflexInstall.CacheDir, "worlds", Version);
        }
        public const int AiSkillNormal = 95;
        /// LocationSelect's AI skill list (AIDifficulty): Easy, Normal, Hard, Pro. AI on means a full gate (EventRiders 12).
        public static readonly (string name, int skill)[] AiSkills = { ("ROOKIE", 80), ("AMATEUR", 95), ("PRO", 105), ("ALL TIME", 120) };   // STR_AI_LEVEL1..4
        public const int FullGateOpponents = Reflex.Port.RxCore.RxRiderCap.OriginalOpponents;
        /// RIDER CAP (w-rc-client): the race setup's FIELD SIZE (the riders, you included) as the world's AI riders; 0 or less = the original's
        /// full gate (12 riders, 11 AI).
        public static int OpponentsFor(int riders) => riders <= 0 ? FullGateOpponents : Math.Max(1, Math.Min(Reflex.Port.RxCore.RxRiderCap.MaxOpponents, riders - 1));

        public bool Ready => File.Exists(Stamp) && File.Exists(World) && File.Exists(Deform);

        static Task building; static string buildingLevel;
        public bool Building => building != null && !building.IsCompleted && buildingLevel == name;
        public string Failure { get; private set; }

        /// Starts (or joins) the background build; poll Progress / Ready.
        public Task Build()
        {
            if (Building) return building;
            var spec = ev.spec;
            if (career != null)   // MOTOCAREER: the series sets riders, AI and duration itself (StartEvent); garage fields -1 = the profile's
            {
                spec.garage = GarageSet; spec.riderOwner = spec.vehicleOwner = spec.vehicleType = -1;
                spec.career = CareerSet; spec.seriesId = career.seriesId; spec.seriesVehicleType = career.vehicleTypeId;
                if (career.skipPractice >= 0 || career.outcomes.Count > 0)
                {
                    spec.career = CareerSet2; spec.seriesSkipPractice = career.skipPractice; spec.seriesOutcomes = career.outcomes.Count;
                    string save0 = Path.Combine(ReflexInstall.CacheDir, "save"); Directory.CreateDirectory(save0);
                    File.WriteAllBytes(Path.Combine(save0, "career_outcomes.bin"), career.OutcomeBytes());
                    // the Gate Selection page's choices (WP-5; career::read_gates): int32 per gate selection of the series, -1 the default
                    string gatesFile = Path.Combine(save0, "career_gates.bin");
                    if (career.HasGateChoices) { var gb = new byte[4 * career.gates.Count]; for (int i = 0; i < career.gates.Count; i++) BitConverter.GetBytes(career.gates[i]).CopyTo(gb, 4 * i); File.WriteAllBytes(gatesFile, gb); }
                    else if (File.Exists(gatesFile)) File.Delete(gatesFile);
                }
            }
            if (opponents > 0) { spec.riders = opponents + 1; spec.ai = aiSkill; }   // LocationSelect.lua: AI on -> EventRiders 12
            if (laps > 0) spec.duration = laps;                                      // EventDuration
            if (garage != null)                                                      // Vehicle Select (VehicleSelect:End), its class (LocationSelect)
            {
                if (garage.vehicleClass >= 0) spec.vehicleClass = garage.vehicleClass;
                if (garage.Equips)
                {
                    spec.garage = GarageSet; spec.riderOwner = garage.riderOwner; spec.riderId = garage.riderId; spec.riderGearSet = garage.riderGearSet;
                    spec.vehicleOwner = garage.vehicleOwner; spec.vehicleId = garage.vehicleId; spec.vehicleType = garage.vehicleTypeId;
                }
            }
            ReflexModes.ApplyModeFields(ev, ref spec);                               // Freestyle, Section Race, the challenges (ReflexModes)
            ReflexNativeBoot.Apply();                                                // the native game in the boot (reflex_world_boot_ports)
            Directory.CreateDirectory(dir);
            string game = ReflexInstall.Require(), crt = ReflexCrt.Dir, save = Path.Combine(ReflexInstall.CacheDir, "save");
            Directory.CreateDirectory(save);
            string prefix = Path.Combine(dir, name);
            buildingLevel = name; Failure = null;
            var self = this;
            building = Task.Run(() =>
            {
                File.Delete(self.Stamp);
                int r = reflex_world_build(game, crt, save, ref spec, prefix);
                if (r != 0) { self.Failure = Marshal.PtrToStringAnsi(reflex_world_build_error()) ?? "world build failed"; throw new InvalidOperationException(self.Failure); }
                File.WriteAllText(self.Stamp, Version);
                if (self.career != null) PruneCareer(self.dir, self.name);
                Prune(self.dir, self.name);
            });
            return building;
        }

        /// Marks this world as used (the cache keeps the most recently used ones).
        public void Touch() { try { if (File.Exists(Stamp)) File.SetLastWriteTimeUtc(Stamp, DateTime.UtcNow); } catch (Exception) { } }

        /// Worlds kept in the cache (each about 40-60 MB; any other one is rebuilt from the install when chosen again).
        public const int CacheKeep = 12;
        /// A career world holds the save it was made from: the older ones (other saves) can never be used again.
        static void PruneCareer(string dir, string keep)
        {
            try
            {
                foreach (var old in new DirectoryInfo(dir).GetFiles("Career_*.ready"))
                {
                    string n = Path.GetFileNameWithoutExtension(old.Name); if (n == keep) continue;
                    old.Delete();
                    foreach (var suffix in new[] { "-world.bin", "-deform.bin", "-deform-model.bin", "-boot.log", "-build.log" }) { string f = Path.Combine(dir, n + suffix); if (File.Exists(f)) File.Delete(f); }
                }
            }
            catch (Exception) { }
        }
        static void Prune(string dir, string keep)
        {
            try
            {
                var stamps = new DirectoryInfo(dir).GetFiles("*.ready").OrderByDescending(f => f.LastWriteTimeUtc).ToList();
                foreach (var old in stamps.Skip(CacheKeep))
                {
                    string n = Path.GetFileNameWithoutExtension(old.Name);
                    if (n == keep) continue;
                    old.Delete();   // the stamp first: a half-deleted world is never taken as ready
                    foreach (var suffix in new[] { "-world.bin", "-deform.bin", "-deform-model.bin", "-boot.log", "-build.log" })
                    { string f = Path.Combine(dir, n + suffix); if (File.Exists(f)) File.Delete(f); }
                }
            }
            catch (Exception) { }   // pruning is best effort
        }

        public static float Progress(out string status)
        {
            var b = new byte[256];
            float p = reflex_world_build_progress(b, b.Length);
            int n = Array.IndexOf(b, (byte)0); status = System.Text.Encoding.ASCII.GetString(b, 0, n < 0 ? b.Length : n);
            return p;
        }

        /// The level of a scene that names its world by the old captured-file convention ("concord-world.bin").
        public static string LevelOf(string worldFile)
        {
            string f = (worldFile ?? "").ToLowerInvariant();
            if (f.Contains("concord")) return "NAT_Concord";
            return "FR_Alpine";
        }
    }
}

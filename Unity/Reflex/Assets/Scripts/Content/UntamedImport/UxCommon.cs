// MX vs ATV Untamed converter, foundation: the port of the Python reference converter's ux_common.py (constants, the level table, slot names,
// the RSData namespace spellings) and the stage "work store" (the Python stages talk through files in work/<Level>/: JSON manifests and
// binary outputs; the C# stages keep that shape in the import's staging folder so each stage can be compared with its Python output).
// The Python module's paths into its own folder become inputs: the disc (UxDisc), the stock library (UxStock), the work store (UxWork).
using System;
using System.Collections.Generic;
using System.IO;
using System.Text;
using System.Threading;
using Reflex.Port.Content.AliveImport;

namespace Reflex.Port.Content.UntamedImport
{
    /// ux_common.Level: one row of the level table.
    public sealed class UxLevelRow
    {
        public readonly string disc, shortName, mode, tdf, display, freeride, location, condition;
        public UxLevelRow(string disc, string shortName, string mode, string tdf, string display, string freeride, string location, string condition)
        {
            this.disc = disc; this.shortName = shortName; this.mode = mode; this.tdf = tdf; this.display = display; this.freeride = freeride;
            this.location = location; this.condition = condition;
        }
        /// The row as json.dumps writes the namedtuple (a list of its 8 fields).
        public object[] AsList() => new object[] { disc, shortName, mode, tdf, display, freeride, location, condition };
        /// Level._asdict() (ux_common._json_default never sees it: a namedtuple is a tuple).
        public JObj AsDict() => new JObj().Set("disc", disc).Set("short", shortName).Set("mode", mode).Set("tdf", tdf).Set("display", display)
            .Set("freeride", freeride).Set("location", location).Set("condition", condition);
        public override string ToString() => disc;
    }

    /// ux_common.py (U.*).
    public static class UxCommon
    {
        // ---------------------------------------------------------------------------------------- slot names
        public const int DefaultSlot = 1;                       // DEFAULT_SLOT
        /// SLOT_PREFIX[kind] ('sx' or 'fr').
        public static string SlotPrefix(string kind)
        {
            switch (kind) { case "sx": return "Beta_Sx_Track_Slot_"; case "fr": return "Beta_Track_Slot_"; }
            throw new UxException("unknown slot kind " + kind);
        }
        /// slot_name(kind, n): 'Beta_Sx_Track_Slot_1' (sx) or 'Beta_Track_Slot_1' (fr).
        public static string SlotName(string kind = "sx", int n = DefaultSlot) => SlotPrefix(kind) + n.ToString(System.Globalization.CultureInfo.InvariantCulture);
        public static readonly string[] SlotFileKinds = { "database", "level", "package", "scene" };
        /// slot_files(folder, name)[kind]: '<name>.dx9.<kind>'.
        public static string SlotFileName(string name, string kind) => name + ".dx9." + kind;
        /// disc_files(lv)[kind]: '<Level>.xenon.<kind>' (in the disc's Database folder).
        public static string DiscFileName(UxLevelRow lv, string kind) => lv.disc + ".xenon." + kind;

        // ---------------------------------------------------------------------------------------- design constants
        public const double Y_LIFT = 10.0;                      // DESIGN 5.7
        /// LEVEL_Y_LIFT (DESIGN 16).
        public static readonly KeyValuePair<string, double>[] LevelYLift =
        {
            new KeyValuePair<string, double>("HarrisPark_STADIUM", 25.0),
            new KeyValuePair<string, double>("WestStone_STADIUM", 23.0),
            new KeyValuePair<string, double>("BridgePort_STADIUM", 66.0),
        };
        /// y_lift(lv): LEVEL_Y_LIFT or Y_LIFT.
        public static double YLift(UxLevelRow lv)
        {
            foreach (var kv in LevelYLift) if (kv.Key == lv.disc) return kv.Value;
            return Y_LIFT;
        }
        public static double YLift(string name) => YLift(Level(name));
        public const double HEIGHT_SCALE = 2048.0;              // R32F DISP value = height_ft / 2048
        public const int HF_SIZE = 1024;                        // px, 1 ft per px, centred at 0

        /// LIMITS (DESIGN 5.11 V5).
        public static class Limits
        {
            public const int MaxGroupVerts = 4800, MaxGroupStripIndices = 9600, MaxGroupsPerModel = 24, MaxPlacedClusters = 10000;
            public const int MaxSurfaceBytes = 61 * 1024 * 1024, MaxEntitiesPerChunk = 30, WarnPackageBytes = 150 * 1024 * 1024;
            public const double HfMinFt = 0.0, HfMaxFt = 2047.0;
        }
        /// LIMITS as the dict the Python stages write into manifests (key order = the Python literal's).
        public static JObj LimitsDict() => new JObj().Set("max_group_verts", Limits.MaxGroupVerts).Set("max_group_strip_indices", Limits.MaxGroupStripIndices)
            .Set("max_groups_per_model", Limits.MaxGroupsPerModel).Set("max_placed_clusters", Limits.MaxPlacedClusters)
            .Set("max_surface_bytes", Limits.MaxSurfaceBytes).Set("max_entities_per_chunk", Limits.MaxEntitiesPerChunk)
            .Set("hf_min_ft", Limits.HfMinFt).Set("hf_max_ft", Limits.HfMaxFt).Set("warn_package_bytes", Limits.WarnPackageBytes);

        // ---------------------------------------------------------------------------------------- levels
        public static readonly UxLevelRow[] Levels =
        {
            new UxLevelRow("Norton_STADIUM", "norton", "sx", "Norton_STADIUM-sx.tdf07", "Norton Coliseum Supercross", "Norton Coliseum", "Norton", "Morning, Sunny"),
            new UxLevelRow("Capital_City_STADIUM", "capitalcity", "sx", "Capital_City_STADIUM-sx.tdf07", "Capital City Supercross", "Capital City Stadium", "Capital City", "Night, Clear"),
            new UxLevelRow("Columbus_STADIUM", "columbus", "sx", "Columbus_STADIUM-sx.tdf07", "Columbus Stadium Supercross", "Columbus Stadium", "Columbus", "Night, Clear"),
            new UxLevelRow("Denton_STADIUM", "denton", "sx", "Denton_STADIUM-sx.tdf07", "Denton Stadium Supercross", "Denton Stadium", "Denton", "Night, Clear"),
            new UxLevelRow("HarrisPark_STADIUM", "harrispark", "sx", "HarrisPark_STADIUM-sx.tdf07", "Harris Park Supercross", "Harris Park Stadium", "Harris Park", "Afternoon, Clear & Sunny"),
            new UxLevelRow("Foothills_STADIUM", "foothills", "sx", "Foothills_STADIUM-sx.tdf07", "Foothills Stadium Supercross", "Foothills Stadium", "Foothills", "Afternoon, Cloudy"),
            new UxLevelRow("WestStone_STADIUM", "weststone", "sx", "WestStone_STADIUM-sx.tdf07", "West Stone Gardens Supercross", "West Stone Gardens Stadium", "West Stone Gardens", "Evening, Clear"),
            new UxLevelRow("Jefferson_STADIUM", "jefferson", "sx", "Jefferson_STADIUM-sx.tdf07", "Jefferson Stadium Supercross", "Jefferson Stadium", "Jefferson", "Afternoon, Clear"),
            new UxLevelRow("Wilford_STADIUM", "wilford", "sx", "Wilford_STADIUM-sx.tdf07", "Wilford Stadium Supercross", "Wilford Stadium", "Wilford", "Evening, Cloudy"),
            new UxLevelRow("BridgePort_STADIUM", "bridgeport", "sx", "BridgePort_STADIUM-sx.tdf07", "Bridgeport Supercross", "Bridgeport Stadium", "Bridgeport", "Evening, Clear"),
            new UxLevelRow("Fairview_STADIUM", "fairview", "ec", "Fairview_STADIUM-ec.tdf07", "Fairview Arena, Maxxis EnduroCross", "Fairview Arena", "Fairview Arena",
                "Challenge: Boulders, Log Piles, Tire Climbs, Water Crossings"),
            new UxLevelRow("Lincoln_STADIUM", "lincoln", "ec", "Lincoln_STADIUM-ec.tdf07", "Lincoln Memorial Arena, Maxxis EnduroCross", "Lincoln Memorial Arena", "Lincoln Memorial",
                "Challenge: Boulders, Log Piles, Mud Pit, Water Crossings"),
        };
        /// CHAMPIONSHIP_SUBSCENE[mode] (DESIGN 5.2).
        public static string ChampionshipSubscene(string mode)
        {
            switch (mode) { case "sx": return "ChampionshipSupercross"; case "ec": return "ChampionshipEndurocross"; }
            throw new UxException("unknown race mode " + mode);
        }

        /// level(name): by disc name ('Norton_STADIUM'), short name ('norton') or a path / stem containing the disc name; case-insensitive.
        public static UxLevelRow Level(string name)
        {
            string s = (name ?? "").Trim();
            string file = s.Replace('\\', '/'); file = file.Substring(file.LastIndexOf('/') + 1);
            int dot = file.IndexOf('.'); string stem = dot >= 0 ? file.Substring(0, dot) : file;
            foreach (string k in new[] { s, stem })
            {
                foreach (var lv in Levels) if (lv.disc == k) return lv;
                foreach (var lv in Levels) if (lv.shortName == k.ToLowerInvariant()) return lv;
                foreach (var lv in Levels) if (string.Equals(lv.disc, k, StringComparison.OrdinalIgnoreCase)) return lv;
            }
            throw new UxException("unknown level '" + name + "'");
        }

        // ---------------------------------------------------------------------------------------- RSData namespace spelling (DESIGN 18)
        public const string NS_PREFIX = "RSToolsNS_RSDataNS_";
        /// The RSToolsNS_RSDataNS_<Type> names of the core's g_types (core/game/build_scene.c), in table order.
        public static readonly string[] RSDATA_CORE_TYPES =
        {
            "LWModelRSData", "ModelRSData", "CollisionRSData", "AmbientLightRSData", "PointLightRSData", "SpotLightRSData",
            "DirectionalLightRSData", "ImageBasedLightRSData", "AmbientDirectionalLightRSData", "SkyRSData",
            "PVSEntityRSData", "DistanceFogRSData", "WaterSystemRSData", "HFTerrainRSData", "HFBumpModelRSData",
            "TextureAnimControllerRSData", "LevelAmbientAudioZoneRSData", "GeneralStorageEntityRSData",
            "ReflectionMapGenRSData", "GrassSystemRSData", "ForestSystemRSData", "CustomizeVehicleStartRSData",
            "StartEntityFreestyleRSData", "StartEntitySupermotoRSData", "StartEntityOpencrossRSData",
            "StartEntityFreeRideRSData", "StartEntityFreeRideStarRSData", "StartEntityFreeRideLineRSData",
            "StartEntitySnakeTagRSData", "TagBallStartRSData", "ChallengeSpecsRSData", "AnchorEntityRSData",
            "BigAirStartRSData", "HillClimbStartRSData", "JumperStartRSData", "PathfinderStartRSData", "StoppieStartRSData",
            "TargetPracticeStartRSData", "TrailBlazerStartRSData", "WheelieStartRSData", "StepUpStartRSData",
            "StepUpBronzeStartRSData", "StepUpSilverStartRSData", "StepUpGoldStartRSData", "TerrainRSData", "TriggerRSData",
            "PhysicsRSData", "ParticleRSData", "ParticleSphereRSData", "ParticleYRingRSData", "BermParticleRSData",
            "FireworksParticleRSData", "TriggeredPlaceableParticleRSData", "ParticleManagerRSData", "PopUpBaleRSData",
            "PopDownBaleRSData", "DropGateRSData", "HoleShotLightRSData", "LeaderLightRSData", "TripleLightRSData",
            "LeaderBoardRSData", "TrackSideFlagHolderRSData", "ScoreTowerFlagHolderRSData", "ThirtySecondBoardRSData",
            "CrewChiefRSData", "GraffitiStartRSData", "HockeyStartRSData", "CaptureStartRSData", "CaptureBaseRSData",
            "GraffitiCapturePointRSData", "BillboardCrowdRSData", "SectionRaceTriggerRSData", "WaypointGateRSData",
            "EggHuntEggEntityRSData", "MXFixedCameraRSData", "MXReplayTowerCameraRSData", "MXReplayZipLineCameraRSData",
            "MXReplaySplineCameraRSData", "MXReplayAnimatedCameraRSData", "MassiveAdEntityRSData", "MXCameraPathRSData",
            "OnFieldCrowdMapRSData", "MudManagerRSData", "BillboardRSData", "ScenarioAnimEntityRSData",
            "AnimEntitySingleAnimLoopRSData", "AnimEntitySingleAnimLoopInstancedRSData", "AnimEntitySplineAnimLoopRSData",
            "AnimEntitySplineWithStopLogicRSData", "AnimEntityProximityRSData", "AnimEntityTriggerRSData",
            "AnimEntitySoaringRSData", "AnimEntityRoamingProximityRSData", "AnimEntitySingleAnimLoopWithParticlesRSData",
            "AnimEntitySplineAnimLoopWithParticlesRSData", "AnimEntitySplineWithStopLogicWithParticlesRSData",
            "JumbotronCameraRSData", "AudioTerrainZoneManagerRSData", "AudioCollisionManagerRSData",
            "AudioVolumeLevelMixEntityRSData", "VolumeFogRSData", "LightFlareRSData", "LensFlareRSData",
            "TracksEntityRSData", "OverlayLightRSData", "HDRSettingsRSData", "ShadowMapRSData", "ScreenDustRSData",
            "MudSplatterRSData", "ScreenWaterDropsRSData", "CameraScreenshotRSData", "CameraPanoramicRSData",
            "SXSeatedCameraManRSData", "CollectibleRSData", "CollectibleHitDiscoveryRSData",
            "CollectibleTargetPracticeRSData", "StepUpBarRSData", "StepUpUprightRSData", "TutorialTriggerRSData",
            "AAARSData", "AIAvoidRSData", "MXRaceLine", "RaceLineArrowRSData", "TDFEntityRSData", "VideoEntityRSData",
            "ScrollingTextureRSData", "CameraFacingEntityRSData", "LevelInfoRSData", "TimedSectionTriggerRSData",
            "MusicTrackListRSData", "MusicScoreListRSData", "StatusBoardRSData", "UIGateSelectionRSData",
            "UIGateSelectionCameraRSData", "AudioEmitterRSData", "AudioLineEmitterRSData", "AudioCrowdEmitterRSData",
            "AudioZoneSphereRSData", "AudioLineZoneSphereRSData", "AudioVolumeLevelMixRSData",
            "FreeRideActivityAudioManagerRSData", "FreestyleActivityAudioManagerRSData", "RaceActivityAudioManagerRSData",
            "SectionRaceActivityAudioManagerRSData", "WaypointRaceActivityAudioManagerRSData",
            "PokerRaceActivityAudioManagerRSData", "InfectionActivityAudioManagerRSData",
            "EggHuntActivityAudioManagerRSData", "SnakeActivityAudioManagerRSData", "TagActivityAudioManagerRSData",
            "ChallengeActivityAudioManagerRSData", "LocalizedPackageReferenceEntityRSData",
        };
        static Dictionary<string, string> s_canon;

        /// canon_namespace(ns): the core's exact spelling of an RSData namespace that matches a g_types entry ignoring case, else ns unchanged.
        public static string CanonNamespace(string ns)
        {
            if (ns == null) return null;
            var map = s_canon;
            if (map == null)
            {
                map = new Dictionary<string, string>(StringComparer.Ordinal);
                foreach (string t in RSDATA_CORE_TYPES) map[PyLower(NS_PREFIX + t)] = NS_PREFIX + t;
                s_canon = map;
            }
            return map.TryGetValue(PyLower(ns), out string c) && c.Length == ns.Length ? c : ns;
        }

        /// canon_namespaces(elems): rewrites, in place, every string 'namespace' attribute under the trees to the core's spelling.
        /// Returns the changes as (old, new, count) in first-seen order (the Python Counter's insertion order).
        public static List<(string old, string canon, int count)> CanonNamespaces(IEnumerable<UxElem> elems)
        {
            var n = new List<(string, string, int)>();
            foreach (var root in elems)
                foreach (var x in root.Iter())
                    foreach (var a in x.attrs)
                        if (a.name == "namespace" && a.type == 1 && a.str != null)
                        {
                            string c = CanonNamespace(a.str);
                            if (c == a.str) continue;
                            int at = n.FindIndex(r => r.Item1 == a.str && r.Item2 == c);
                            if (at < 0) n.Add((a.str, c, 1)); else n[at] = (a.str, c, n[at].Item3 + 1);
                            a.str = c;
                        }
            return n;
        }

        /// rsdata_type(entity): 'PhysicsRSData' from an Entity Elem.
        public static string RsdataType(UxElem entity) => ((string)entity.children[0].Attr("namespace")).Replace(NS_PREFIX, "");

        /// str.lower() for the ASCII range (asset and namespace names are ASCII; non-ASCII characters are left as they are).
        public static string PyLower(string s)
        {
            int i = 0; while (i < s.Length && !(s[i] >= 'A' && s[i] <= 'Z')) i++;
            if (i == s.Length) return s;
            var c = s.ToCharArray();
            for (; i < c.Length; i++) if (c[i] >= 'A' && c[i] <= 'Z') c[i] = (char)(c[i] + 32);
            return new string(c);
        }

        /// logger(tag): '[tag] a b c' lines to the sink (the Python log files' time stamps are not reproduced).
        public static Action<string> Logger(string tag, Action<string> sink) => line => sink?.Invoke("[" + tag + "] " + line);
    }

    /// The stage work store: Python's work/<Level>/ folder (level.json, textures/, meshes/, materials/, collision/, terrain/, entities.json,
    /// entities/, race.json, race/ ...), a plain folder of the import's staging area. Paths are '/'-separated and relative to the store.
    /// Writes are atomic (a temporary file, then a rename: ux_common.write_bytes) and carry no time stamps of their own.
    public sealed class UxWork
    {
        public readonly string Root;
        public UxWork(string root) { Root = Path.GetFullPath(root); }
        /// work_dir(lv, *sub): the store of one level below a common root.
        public static UxWork ForLevel(string workRoot, UxLevelRow lv) => new UxWork(Path.Combine(workRoot, lv.disc));
        public UxWork Sub(string relative) => new UxWork(PathOf(relative));

        public string PathOf(string relative)
        {
            UxBin.Require(!string.IsNullOrEmpty(relative), "work store: empty path");
            string r = relative.Replace('\\', '/');
            UxBin.Require(!r.StartsWith("/", StringComparison.Ordinal) && r.IndexOf(':') < 0, "work store: absolute path " + relative);
            foreach (string part in r.Split('/')) UxBin.Require(part != ".." && part.Length > 0 && part != ".", "work store: unsafe path " + relative);
            return Path.Combine(Root, r.Replace('/', Path.DirectorySeparatorChar));
        }
        public bool Exists(string relative) => File.Exists(PathOf(relative));
        public bool DirExists(string relative) => Directory.Exists(PathOf(relative));
        public long Length(string relative) => new FileInfo(PathOf(relative)).Length;
        public byte[] ReadBytes(string relative)
        {
            string p = PathOf(relative);
            if (!File.Exists(p)) throw new UxException("work store: " + relative + " is missing (" + Root + ")");
            return File.ReadAllBytes(p);
        }
        public string ReadText(string relative) => Encoding.UTF8.GetString(ReadBytes(relative));
        /// read_json(path): JObj / List<object> / ...
        public object ReadJson(string relative) => UxJson.ParseUtf8(ReadBytes(relative));
        public JObj ReadJsonObj(string relative) => UxJson.Obj(ReadJson(relative));
        /// read_json(path, default): null when the file is missing.
        public object ReadJsonOrNull(string relative) => Exists(relative) ? ReadJson(relative) : null;

        static int s_tmp;
        /// write_bytes(path, data): parent folders created, a temporary file replaced into place.
        public string WriteBytes(string relative, byte[] data)
        {
            string p = PathOf(relative);
            Directory.CreateDirectory(Path.GetDirectoryName(p));
            string tmp = p + ".tmp" + Interlocked.Increment(ref s_tmp).ToString(System.Globalization.CultureInfo.InvariantCulture);
            File.WriteAllBytes(tmp, data);
            if (File.Exists(p)) File.Delete(p);
            File.Move(tmp, p);
            return p;
        }
        public string WriteText(string relative, string text) => WriteBytes(relative, new UTF8Encoding(false).GetBytes(text));
        /// write_json(path, obj, indent): ux_common.dumps_json (sorted keys, indent 1, raw UTF-8, a final newline).
        public string WriteJson(string relative, object obj, int indent = 1) => WriteText(relative, UxJson.DumpsUx(obj, indent));
        /// Path(...).write_text(json.dumps(obj, indent=indent)): insertion order, ASCII escapes, no final newline (uxpkg's index.json).
        public string WriteJsonPlain(string relative, object obj, int indent = 1) => WriteText(relative, UxJson.Dumps(obj, indent));

        public void Delete(string relative) { string p = PathOf(relative); if (File.Exists(p)) File.Delete(p); }
        /// sorted(folder.glob(pattern)) as store-relative paths (ordinal order, as Python sorts paths on Windows by their lower... see note).
        /// Python's Path sort on Windows compares case-insensitively (the folded parts); this lists in that order.
        public List<string> List(string relativeDir, string pattern = "*", bool recursive = false)
        {
            var result = new List<string>();
            string dir = string.IsNullOrEmpty(relativeDir) ? Root : PathOf(relativeDir);
            if (!Directory.Exists(dir)) return result;
            foreach (string f in Directory.GetFiles(dir, pattern, recursive ? SearchOption.AllDirectories : SearchOption.TopDirectoryOnly))
                result.Add(f.Substring(Root.Length + 1).Replace('\\', '/'));
            result.Sort((a, b) => string.CompareOrdinal(UxCommon.PyLower(a), UxCommon.PyLower(b)));
            return result;
        }
    }

    /// What a stage gets: the level, its disc data, the player's stock library, the work store, a log sink and the cancel token.
    public sealed class UxContext
    {
        public UxLevelRow lv;
        public UxDiscLevel disc;
        public IUxLibrary library;
        public UxStock stock;
        public UxWork work;
        public Action<string> log = s => { };
        public CancellationToken cancel;
    }
}

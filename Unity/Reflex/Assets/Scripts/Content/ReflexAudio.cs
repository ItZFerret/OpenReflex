// The per-level FMOD event library, built at run time from the player's own install (port of _re/formats/audio_export.py's
// normalisation and _re/formats/unity_audio_library.py): the FEV event projects of the level's database plus the global projects a
// race on that level uses, flattened to the arrays OriginalAudio plays, and the wave banks they reference.
// Waves are addressed by the same "<project>[/<bank>]/<wave>.wav" keys the Python export used (verified by the editor test
// ReflexAudioTests); their bytes come from the FSB banks (ReflexFmod): an MP3 stream, or PCM16 for PCM/IMA samples.
// Nothing here uses the Unity API except Mathf-free arithmetic, so Build() runs on a worker thread.
//
// Which projects a race loads (ReflexAudioSet.Race, WP-1), from the install's own configuration where it says so:
//   * the level database's FEV projects (as before);
//   * VEHICLE_FOLEY_01 + RIDER_AUDIO (VehicleFoleyAudio/GeneralFoley, preloaded by AudioPackagesList.xml);
//   * the engine project of every engine size a fielded vehicle type can have (VehicleModel.VehicleEngineSizeId ->
//     VehicleEngineSize.FactoryName -> AudioEngineStates `sound`), in AudioPackagesList.xml's package order: MX_125R_01,
//     MX_250R_TEST_05, MX_250F_01, MX_450F_01 for the MX Lite / MX / ATV types the port fields (FieldedVehicleTypes).
//     The original preloads all 11 size packages; car engines (RHINO_ENGINE_02, PRO_02_02, PRO_LITE_ENGINE_01, BUGGY_ENGINE_01)
//     join when a car is in the race (OriginalAudio.UseRoster: the same table path per roster entry) or when its vehicle type
//     is added to FieldedVehicleTypes;
//   * the crowd project of the level's crowd type (CrowdSoundProjects/Crowd_Config CrowdAudioParameters: CROWDTYPE_STADIUM ->
//     CROWD_IND_LRG for the Supercross stadiums, CROWDTYPE_OUTDOOR -> CROWD_OUT_LRG elsewhere; the game's command stream
//     issues CROWD_IND_LRG/... on SX_Kingston and CROWD_OUT_LRG/... on NAT_Concord);
//   * the activity's projects from AudioLocalizedActivity: ACTIVITY_<ACT> (language-neutral stingers) and the announcer
//     <VO>_<LANG> (Audio_Loc_<...>.<LANG> packages) for the voice language (ENG / FRE ship; GER / ITA / SPA text languages
//     fall back to ENG voice).
// Every event also carries the game's volume channel (ReflexChannel: Engine / Crowd / DriveOn / Skid / Scenario / VoiceOver / UI /
// Music / Effects, MXRegistry.xml VolumeChannels) and its max-playbacks limit and behaviour (FEV event header).
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using System.Text;
using Reflex.Port.OriginalWorld;
using Fev = Reflex.Port.Content.ReflexFmod.Fev;

namespace Reflex.Port.Content
{
    public sealed class ReflexWave
    {
        public string key;                  // "<project>[/<bank>]/<stem>.wav" (OriginalAudio.Library file name)
        public ReflexFmod.Fsb bank; public ReflexFmod.Sample sample; public string bankId;
        public string codec;                // mpeg | pcm | ima_adpcm | unknown
        public int rate => sample.frequency; public int channels => sample.channels; public int lengthSamples => sample.lengthSamples;
        byte[] _mp3;
        /// The clean MPEG stream of an "mpeg" sample (FMOD padding stripped).
        public byte[] Mp3()
        {
            if (_mp3 == null) _mp3 = ReflexFmod.CleanMpeg(bank.Raw(sample), out _, out _, out _, out _);
            return _mp3;
        }
        /// Interleaved PCM16 of a "pcm" / "ima_adpcm" sample.
        public short[] Pcm()
        {
            var raw = bank.Raw(sample);
            return codec == "ima_adpcm" ? ReflexFmod.DecodeIma(raw, sample.channels, sample.lengthSamples) : ReflexFmod.DecodePcm(raw, sample.mode);
        }
        /// Stable cache file name for the MP3 stream.
        public string CacheName => bankId.Substring(0, 12) + "_" + sample.index + ".mp3";
        public void DropBank() { _mp3 = null; }
    }

    public sealed class ReflexAudioSet
    {
        public string level;
        public OriginalAudio.Library library;
        public readonly Dictionary<string, ReflexWave> waves = new Dictionary<string, ReflexWave>(StringComparer.Ordinal);
        public readonly List<string> unresolved = new List<string>();   // "project: wave (bank): reason"
        public readonly List<string> projects = new List<string>();     // the FEV projects in the library
        public readonly List<string> missing = new List<string>();      // wanted global projects that are not in the install
        /// The FEV projects' reverb definitions by name (EventSystem::getReverbPreset: the level's Default project has FirstPerson).
        public readonly Dictionary<string, ReflexFmod.ReverbDef> reverbDefs = new Dictionary<string, ReflexFmod.ReverbDef>(StringComparer.OrdinalIgnoreCase);

        // unity_audio_library.py GLOBAL_COMMON / GLOBAL_BY_KIND (the projects every race of that kind uses besides the level's own):
        // the python-identical library of Build(level) (AudioContentTests.CompareLibraries). Races use Build(Race).
        static readonly string[] GlobalCommon = { "VEHICLE_FOLEY_01", "RIDER_AUDIO", "CROWD_OUT_LRG", "MX_250F_01" };
        static readonly string[] Foley = { "VEHICLE_FOLEY_01", "RIDER_AUDIO" };
        // audio_export.py GLOBAL_DBS: where those projects live
        static readonly string[] GlobalDbs = { "VehicleEngineAudio", "VehicleFoleyAudio", "CrowdSoundProjects", "Music", "ScenarioMusic", "UIAudio",
            "AudioDependencies", "AudioLocalizedActivity", "AudioLocalizedChallenges", "AudioLocalizedTutorial" };
        public static string[] GlobalFor(string level) => GlobalForStyled(TrackMods.Style(level));
        static string[] GlobalForStyled(string level) =>
            level.StartsWith("NAT_", StringComparison.Ordinal) ? new[] { "ACTIVITY_NATIONALS", "NATIONAL_ENG" } :
            level.StartsWith("FR_", StringComparison.Ordinal) ? new[] { "ACTIVITY_FREERIDE", "FREERIDE_ENG" } : new string[0];

        // ------------------------------------------------------------------------------------------------ the race's projects
        /// What a race loads (see the file header). `activity` is the engine ActivityName (ReflexEvents: National, Supercross,
        /// WaypointRace, FreeRide, Terracross ...); empty: from the level's prefix. `language`: the UI language (ReflexUiData).
        public sealed class Race
        {
            public string level, activity = "", language = "ENG";
            public readonly List<string> extra = new List<string>();
            public string Activity => !string.IsNullOrEmpty(activity) ? activity : ActivityOfLevel(level);
            public string Voice => VoiceLanguage(language);
            /// The global projects besides the level database's own, in load order.
            public List<string> Projects()
            {
                var l = new List<string>(Foley);
                string crowd = CrowdProject(level); if (crowd != null) l.Add(crowd);
                l.AddRange(EngineProjects(FieldedVehicleTypes));
                l.AddRange(ActivityProjects(Activity, Voice));
                l.AddRange(extra);
                return l.Distinct(StringComparer.OrdinalIgnoreCase).ToList();
            }
        }

        /// The vehicle types whose engine projects every race preloads: 2 MX Lite, 3 MX, 4 ATV (the types the port fields). The
        /// car-type vehicles (10 ORV, 11 UTV, 12 Super Buggy, 14 Pro Lite, 15 Pro 2) are added here once they are offered, or come
        /// per race from the roster (OriginalAudio.UseRoster).
        public static readonly HashSet<int> FieldedVehicleTypes = new HashSet<int> { 2, 3, 4, 11, 12, 14, 15 };   // WP-V: + UTV, Super Buggy, Pro Lite, Pro 2 (ReflexVehicles)

        /// Voice-over ships in ENG and FRE only (AudioLocalized* / UIAudio packages); the other text languages hear ENG.
        public static string VoiceLanguage(string uiLanguage) => string.Equals(uiLanguage, "FRE", StringComparison.OrdinalIgnoreCase) ? "FRE" : "ENG";

        public static string ActivityOfLevel(string level)
        {
            level = TrackMods.Style(level ?? "");   // (a custom track: its kind's, TrackMods)
            if (level.StartsWith("NAT_", StringComparison.Ordinal)) return "National";
            if (level.StartsWith("SX_", StringComparison.Ordinal)) return "Supercross";
            if (level.StartsWith("FR_", StringComparison.Ordinal)) return "FreeRide";
            if (level.StartsWith("TRX_", StringComparison.Ordinal)) return "Terracross";
            if (level.StartsWith("PX_", StringComparison.Ordinal)) return "Powercross";
            if (level.StartsWith("FMX_", StringComparison.Ordinal)) return "Freestyle";
            return "";
        }

        // engine ActivityName -> (ACTIVITY_* project, announcer base of the Audio_Loc_*.<LANG> package's project). The package
        // names (Audio_Loc_OMNICROSS -> TERRACROSS_<L>, Audio_Loc_WAYPOINT -> WAYPOINTRACE_<L>, Audio_Loc_CHAMPION_SPORT ->
        // POWERCROSS_<L>) are the install's; the pairing per activity follows the names and the game's command stream (National:
        // ACTIVITY_NATIONALS/ACTIVITY/DROP_GATE + National_ENG/National/INTRO; Supercross: ACTIVITY_SUPERCROSS/ACTIVITY/DROP_GATE).
        // The mini-games / challenges (Tag, Snake, PokerRace, the Free Ride challenges ...) are not offered by the port yet.
        static readonly Dictionary<string, (string act, string vo)> ByActivity = new Dictionary<string, (string, string)>(StringComparer.OrdinalIgnoreCase)
        {
            ["National"] = ("ACTIVITY_NATIONALS", "NATIONAL"), ["Supercross"] = ("ACTIVITY_SUPERCROSS", "SUPERCROSS"),
            ["WaypointRace"] = ("ACTIVITY_WAYPOINT", "WAYPOINTRACE"), ["FreeRide"] = ("ACTIVITY_FREERIDE", "FREERIDE"),
            ["Terracross"] = ("ACTIVITY_TERRACROSS", "TERRACROSS"), ["Powercross"] = ("ACTIVITY_POWERCROSS", "POWERCROSS"),
            ["Freestyle"] = ("ACTIVITY_FREESTYLE", "FREESTYLE"), ["SectionRace"] = ("ACTIVITY_SECTIONRACE", "SECTIONRACE"),
        };
        /// The sound engine's path tokens (0067e560, as App.PlayUILocalizedSoundEvent's: [PLATFORM] -> "PC", [LOC] -> the voice language,
        /// [FORMAT] -> the activity's announcer format, FreeRide -> FREERIDE): e.g. a Free Ride scenario's pilot
        /// "[FORMAT]_[LOC]/[FORMAT]/PILOT_AL" -> "FREERIDE_ENG/FREERIDE/PILOT_AL" (the race library's FREERIDE_ENG project).
        public static string ResolveTokens(string path, Race race)
        {
            if (string.IsNullOrEmpty(path) || path.IndexOf('[') < 0) return path;
            string format = race != null && ByActivity.TryGetValue(race.Activity ?? "", out var a) ? a.vo : "";
            return path.Replace("[PLATFORM]", "PC").Replace("[LOC]", race != null ? race.Voice : "ENG").Replace("[FORMAT]", format);
        }

        public static string[] ActivityProjects(string activity, string voice)
        {
            if (string.IsNullOrEmpty(activity) || !ByActivity.TryGetValue(activity, out var a)) return new string[0];
            return new[] { a.act, a.vo + "_" + VoiceLanguage(voice) };
        }

        /// The crowd project of a level (CrowdAudioParameters: the stadium type for the Supercross stadiums, outdoor elsewhere;
        /// every size of a type maps to the same project).
        public static string CrowdProject(string level)
        {
            bool stadium = (TrackMods.Style(level) ?? "").StartsWith("SX_", StringComparison.Ordinal);   // (a custom Supercross track too)
            string type = stadium ? "CROWDTYPE_STADIUM" : "CROWDTYPE_OUTDOOR";
            try
            {
                var root = Rfx.Bxml(ReflexInstall.Packages.Read("CrowdSoundProjects", "Crowd_Config", "CrowdAudioParameters", "bxml"));
                foreach (var n in root.All()) if (n.tag == "Assets" && n.S("Type") == type && !string.IsNullOrEmpty(n.S("SoundAsset"))) return n.S("SoundAsset");
            }
            catch (Exception) { }
            return stadium ? "CROWD_IND_LRG" : "CROWD_OUT_LRG";
        }

        /// AudioEngineStates (VehicleEngineAudio/EngineState): engine size package (MXVehicleEngineSize_<size>) -> FEV project, in
        /// AudioPackagesList.xml's package order.
        public static List<(string size, string project)> EngineSizes()
        {
            lock (cacheLock)
            {
                if (engineSizes != null) return engineSizes;
                var bySize = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
                var root = Rfx.Bxml(ReflexInstall.Packages.Read("VehicleEngineAudio", "EngineState", "AudioEngineStates", "bxml"));
                foreach (var n in root.All()) if (n.tag != null && n.tag.StartsWith("MXVehicleEngineSize_", StringComparison.Ordinal) && !string.IsNullOrEmpty(n.S("sound"))) bySize[n.tag] = n.S("sound");
                var order = new List<string>();
                try
                {
                    string xml = System.Text.RegularExpressions.Regex.Replace(File.ReadAllText(ReflexInstall.PathOf("AudioPackagesList.xml")), "<!--.*?-->", "", System.Text.RegularExpressions.RegexOptions.Singleline);
                    var db = System.Text.RegularExpressions.Regex.Match(xml, "<DataBase\\s+name=\"VehicleEngineAudio\"\\s*>(.*?)</DataBase>", System.Text.RegularExpressions.RegexOptions.Singleline);
                    foreach (System.Text.RegularExpressions.Match m in System.Text.RegularExpressions.Regex.Matches(db.Groups[1].Value, "<Package[^>]*>\\s*([^<\\s]+)\\s*</Package>")) order.Add(m.Groups[1].Value);
                }
                catch (Exception) { }
                foreach (var k in bySize.Keys) if (!order.Contains(k)) order.Add(k);
                engineSizes = order.Where(bySize.ContainsKey).Select(k => (k, bySize[k])).ToList();
                return engineSizes;
            }
        }
        static readonly object cacheLock = new object();
        static List<(string, string)> engineSizes;
        static Dictionary<int, (int type, string size)> modelEngine;

        /// VehicleModel -> (VehicleTypeId, engine size package) from the install's MXTables_Default.
        public static (int type, string size) ModelEngine(int vehicleModelId)
        {
            lock (cacheLock)
            {
                if (modelEngine == null)
                {
                    modelEngine = new Dictionary<int, (int, string)>();
                    var t = Rfx.Tables(ReflexInstall.Packages.Read("MXTables_Default", "MXTables_Default", "MXTables_Default", "tbldata"));
                    var sizes = new Dictionary<int, string>();
                    foreach (var r in t["VehicleEngineSize"]) { string f = Rfx.Str(r, "FactoryName") ?? ""; sizes[Rfx.Int(r, "VehicleEngineSizeId")] = f.StartsWith("MXVehicleEngineSize_", StringComparison.Ordinal) ? f : "MXVehicleEngineSize_" + f; }
                    foreach (var r in t["VehicleModel"])
                        modelEngine[Rfx.Int(r, "VehicleModelId")] = (Rfx.Int(r, "VehicleTypeId"), sizes.TryGetValue(Rfx.Int(r, "VehicleEngineSizeId"), out var z) ? z : null);
                }
                return modelEngine.TryGetValue(vehicleModelId, out var e) ? e : (-1, null);
            }
        }

        /// The engine project of a vehicle model (null when its size has none).
        public static string EngineProjectOfModel(int vehicleModelId)
        {
            var (_, size) = ModelEngine(vehicleModelId);
            return size == null ? null : EngineSizes().Where(e => string.Equals(e.size, size, StringComparison.OrdinalIgnoreCase)).Select(e => e.project).FirstOrDefault();
        }

        /// The engine projects of every engine size the given vehicle types have (VehicleModel rows), in AudioPackagesList order.
        public static List<string> EngineProjects(IEnumerable<int> vehicleTypes)
        {
            var types = new HashSet<int>(vehicleTypes);
            ModelEngine(-1);
            HashSet<string> used;
            lock (cacheLock) used = new HashSet<string>(modelEngine.Values.Where(v => types.Contains(v.type) && v.size != null).Select(v => v.size), StringComparer.OrdinalIgnoreCase);
            return EngineSizes().Where(e => used.Contains(e.size)).Select(e => e.project).Distinct(StringComparer.OrdinalIgnoreCase).ToList();
        }

        /// The game's volume channel of an event (the channel its emitter's event wrapper is given: Engine for the engine emitter
        /// 0x6855c0, Crowd for the crowd manager 0x67391d, DriveOn / Skid for the drive-on emitters 0x682032.., Music for the
        /// music player 0x675bce; VoiceOver / Scenario / UI as their managers choose), by the project's database and name.
        static ReflexChannel ChannelOf(Proj p, string groupPath)
        {
            string n = p.fev.name;
            if (p.db == "VehicleEngineAudio") return ReflexChannel.Engine;
            if (p.db == "CrowdSoundProjects") return ReflexChannel.Crowd;
            if (p.db == "Music" || p.db == "ScenarioMusic") return ReflexChannel.Music;
            if (p.db == "UIAudio") return n.EndsWith("_ENG", StringComparison.OrdinalIgnoreCase) || n.EndsWith("_FRE", StringComparison.OrdinalIgnoreCase) ? ReflexChannel.VoiceOver : ReflexChannel.UI;
            if (n.EndsWith("_ENG", StringComparison.OrdinalIgnoreCase) || n.EndsWith("_FRE", StringComparison.OrdinalIgnoreCase)) return ReflexChannel.VoiceOver;
            if (n.StartsWith("SCENARIOS_", StringComparison.OrdinalIgnoreCase)) return ReflexChannel.Scenario;
            if (n.IndexOf("DriveOn", StringComparison.OrdinalIgnoreCase) >= 0 || string.Equals(n, "DRIVE_ON", StringComparison.OrdinalIgnoreCase))
                return groupPath.StartsWith("Skid", StringComparison.OrdinalIgnoreCase) ? ReflexChannel.Skid : ReflexChannel.DriveOn;
            return ReflexChannel.Effects;
        }

        sealed class Proj { public Fev fev; public string db, package; public Dictionary<string, Rfx.Asset> banks = new Dictionary<string, Rfx.Asset>(StringComparer.Ordinal); }
        sealed class BankInfo { public Rfx.Asset asset; public ReflexFmod.Fsb fsb; public string id, dir; public Dictionary<int, (string stem, string file, ReflexFmod.Sample s)> samples; public string label; }

        /// Builds the library of a level ("NAT_Concord", "FR_Alpine"). Reads the level's database, the global audio databases and
        /// their wave banks (headers only are kept; sample data stays in the bank images). Thread-safe w.r.t. the Unity API.
        public static ReflexAudioSet Build(string level, params string[] extraProjects)
        {
            var want = new List<string>(GlobalCommon); want.AddRange(GlobalFor(level)); want.AddRange(extraProjects);
            return Build(level, true, want);
        }
        /// The library of a race: the level database's projects + Race.Projects().
        public static ReflexAudioSet Build(Race race) => Build(race.level, true, race.Projects());
        /// A library of global projects only (no level database): the extension OriginalAudio merges for projects added later.
        public static ReflexAudioSet BuildProjects(string label, IEnumerable<string> projects) => Build(label, false, projects.ToList());

        /// The project names the global audio databases hold (FindGlobal's search space), upper case; for OriginalAudio's on-demand load.
        public static bool IsGlobalProject(string project)
        {
            lock (cacheLock)
            {
                if (globalNames == null)
                {
                    globalNames = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
                    var inst = ReflexInstall.Packages;
                    foreach (var dbn in GlobalDbs) { var db = inst.Db(dbn); if (db == null) continue; foreach (var pk in db.packages.Values) foreach (var a in pk.OfType("sound")) globalNames.Add(a.name); }
                }
                return globalNames.Contains(project);
            }
        }
        static HashSet<string> globalNames;

        static ReflexAudioSet Build(string level, bool levelDb, List<string> want)
        {
            var inst = ReflexInstall.Packages;
            var ldb = levelDb ? inst.Db(level) : null;
            if (levelDb && ldb == null) throw new FileNotFoundException("Reflex level database missing: " + level);
            var set = new ReflexAudioSet { level = level };
            var projects = new List<Proj>();
            var names = new HashSet<string>(StringComparer.Ordinal);
            void Add(Fev f, string db, string package) { if (f != null && names.Add(f.name)) projects.Add(new Proj { fev = f, db = db, package = package }); }

            // 1. the FEV projects of the level's own database (level_projects)
            if (ldb != null)
                foreach (var pk in ldb.packages.Values)
                    foreach (var a in pk.OfType("sound")) Add(TryFev(inst, a), level, pk.name);
            // 2. the global projects (GLOBAL_COMMON + the race kind's), looked up by asset name in the global audio databases
            foreach (var w in want)
            {
                if (projects.Any(p => string.Equals(p.fev.name, w, StringComparison.OrdinalIgnoreCase))) continue;
                for (int pass = 0; pass < 2; pass++)       // pass 0: the asset is named like the project; pass 1: any sound asset
                {
                    var hit = FindGlobal(inst, w, pass == 0);
                    if (hit.fev != null) { Add(hit.fev, hit.db, hit.package); break; }
                }
            }
            projects.Sort((a, b) => string.CompareOrdinal(a.fev.name, b.fev.name));    // audio_export sorts by (name, variant)
            foreach (var p in projects) foreach (var rd in p.fev.reverbs) set.reverbDefs.TryAdd(rd.name, rd);
            var upper = new HashSet<string>(projects.Select(p => p.fev.name.ToUpperInvariant()));

            // 3. wave banks: for every wave-bank name of a project, the soundbnk asset of the same database (same package first)
            var bankCache = new Dictionary<Rfx.Asset, BankInfo>();
            var bankByProjName = new Dictionary<(Proj, string), BankInfo>();
            foreach (var p in projects)
            {
                var db = inst.Db(p.db);
                foreach (var wb in p.fev.waveBanks)
                {
                    if (bankByProjName.ContainsKey((p, wb.name))) continue;
                    var cands = new List<Rfx.Asset>();
                    foreach (var pk in db.packages.Values) foreach (var a in pk.OfType("soundbnk")) if (string.Equals(a.name, wb.name, StringComparison.OrdinalIgnoreCase)) cands.Add(a);
                    cands = cands.OrderBy(a => a.package.name != p.package).ToList();      // prefer the package of the project's own FEV asset
                    BankInfo bi = null;
                    foreach (var a in cands)
                    {
                        if (bankCache.TryGetValue(a, out bi)) break;
                        byte[] blob; try { blob = inst.Read(a); } catch (Exception) { continue; }
                        ReflexFmod.Fsb fsb; try { fsb = ReflexFmod.ParseFsb(blob); } catch (Exception) { continue; }
                        bi = new BankInfo { asset = a, fsb = fsb, id = Hex(SHA1.Create().ComputeHash(fsb.blob)) };
                        // identical banks (ATV450f reuses MX450F byte for byte; the three crowd packages) are one bank
                        var same = bankCache.Values.FirstOrDefault(o => o.id == bi.id);
                        if (same != null) bi = same;
                        bankCache[a] = bi; break;
                    }
                    bankByProjName[(p, wb.name)] = bi;
                }
            }

            // 4. wave names per bank index come from the FEV entries (first project wins); sample files per bank (export_samples)
            var bankNames = new Dictionary<BankInfo, Dictionary<int, string>>();
            foreach (var p in projects)
                foreach (var sd in p.fev.soundDefs)
                    foreach (var e in sd.entries)
                        if (e.kind == "wave" && bankByProjName.TryGetValue((p, e.bank), out var bi) && bi != null)
                        {
                            if (!bankNames.TryGetValue(bi, out var m)) bankNames[bi] = m = new Dictionary<int, string>();
                            if (!m.ContainsKey((int)e.index)) m[(int)e.index] = ReflexFmod.Stem(e.wave);
                        }
            foreach (var p in projects)
            {
                bool engine = p.db == "VehicleEngineAudio";
                foreach (var wb in p.fev.waveBanks)
                {
                    var bi = bankByProjName[(p, wb.name)];
                    if (bi == null || bi.samples != null) continue;
                    string rel = engine ? "engine/" + wb.name : p.fev.waveBanks.Length > 1 ? p.fev.name + "/" + wb.name : p.fev.name;
                    bi.dir = rel; bi.label = wb.name; bi.samples = new Dictionary<int, (string, string, ReflexFmod.Sample)>();
                    var used = new HashSet<string>(StringComparer.Ordinal);
                    foreach (var s in bi.fsb.samples)
                    {
                        string stem;
                        if (engine) stem = s.Stem;                                  // fsb_export names engine waves after the FSB sample name
                        else
                        {
                            stem = bankNames.TryGetValue(bi, out var m) && m.TryGetValue(s.index, out var n) && n.Length > 0 ? n : s.Stem;
                            if (used.Contains(stem.ToLowerInvariant())) stem = stem + "__" + s.index;
                            used.Add(stem.ToLowerInvariant());
                        }
                        string codec = ReflexFmod.CodecOf(s, bi.fsb.Head(s));
                        bi.samples[s.index] = (stem, codec == "unknown" ? null : rel + "/" + stem + ".wav", s);
                    }
                }
            }

            // 5. the library: events of the wanted projects, flattened (audio_export.norm_event + unity_audio_library.event)
            var files = new HashSet<string>(StringComparer.Ordinal);
            var order = new List<string>(); var byKey = new Dictionary<string, OriginalAudio.EventDef>(StringComparer.Ordinal);
            foreach (var p in projects)
                foreach (var (gpath, ev) in p.fev.Events())
                {
                    string key = p.fev.name + "/" + gpath + "/" + ev.name;
                    var def = BuildEvent(set, p, ev, bankByProjName);
                    def.channel = (int)ChannelOf(p, gpath); def.maxPlaybacks = (int)ev.maxPlaybacks; def.maxPlaybacksBehavior = (int)ev.maxPlaybacksBehavior; def.priority = (int)ev.priority;
                    if (!byKey.ContainsKey(key)) order.Add(key);
                    byKey[key] = def;                       // later duplicates replace the earlier event but keep its position
                }
            var events = new List<OriginalAudio.EventDef>();
            foreach (var key in order)
            {
                string last = key.Substring(key.LastIndexOf('/') + 1);
                if (last.Contains("__")) continue;          // non-canonical duplicate variants
                var def = byKey[key];
                string proj = key.Substring(0, key.IndexOf('/'));
                if (!upper.Contains(proj.ToUpperInvariant())) continue;
                def.path = key.ToLowerInvariant();
                events.Add(def);
                foreach (var d in def.defs) foreach (var f in d.files) if (f.Length > 0) files.Add(f);
            }
            var sorted = files.ToList(); sorted.Sort(string.CompareOrdinal);
            set.library = new OriginalAudio.Library { level = level, events = events.ToArray(), files = sorted.ToArray() };
            set.projects.AddRange(projects.Select(p => p.fev.name));
            set.missing.AddRange(want.Where(w => !names.Contains(w) && !projects.Any(p => string.Equals(p.fev.name, w, StringComparison.OrdinalIgnoreCase))));
            foreach (var f in sorted) if (set.waveByKey.TryGetValue(f, out var w)) set.waves[f] = w;
            return set;
        }

        readonly Dictionary<string, ReflexWave> waveByKey = new Dictionary<string, ReflexWave>(StringComparer.Ordinal);

        static string Hex(byte[] b) { var s = new StringBuilder(); foreach (var x in b) s.Append(x.ToString("x2")); return s.ToString(); }
        static Fev TryFev(Rfx.Install inst, Rfx.Asset a)
        {
            try { return ReflexFmod.ParseFev(inst.Read(a)); } catch (Exception e) when (e is InvalidDataException || e is IOException) { return null; }
        }
        static (Fev fev, string db, string package) FindGlobal(Rfx.Install inst, string project, bool byAssetName)
        {
            foreach (var dbn in GlobalDbs)
            {
                var db = inst.Db(dbn); if (db == null) continue;
                foreach (var pk in db.packages.Values)
                    foreach (var a in pk.OfType("sound"))
                    {
                        if (byAssetName && !string.Equals(a.name, project, StringComparison.OrdinalIgnoreCase)) continue;
                        var f = TryFev(inst, a);
                        if (f != null && string.Equals(f.name, project, StringComparison.OrdinalIgnoreCase)) return (f, dbn, pk.name);
                    }
            }
            return (null, null, null);
        }

        // audio_export.DSP_PARAMS x unity_audio_library.DSP_KIND: (effect name, parameter index) -> (kind, min, max)
        static bool DspSpec(string name, uint param, out string kind, out float min, out float max)
        {
            kind = null; min = 0; max = 1;
            switch (name)
            {
                case "FMOD Lowpass": if (param == 0) { kind = "lowpass"; min = 10; max = 22000; } else if (param == 1) { kind = "lowpass_q"; min = 1; max = 10; } break;
                case "FMOD Lowpass Simple": if (param == 0) { kind = "lowpass"; min = 10; max = 22000; } break;
                case "FMOD Highpass": if (param == 0) { kind = "highpass"; min = 1; max = 22000; } else if (param == 1) { kind = "highpass_q"; min = 1; max = 10; } break;
                case "FMOD Distortion": if (param == 0) { kind = "distortion"; min = 0; max = 1; } break;
            }
            return kind != null;
        }

        static OriginalAudio.EnvelopeDef BuildEnvelope(ReflexFmod.Envelope en, ReflexFmod.Envelope[] siblings)
        {
            string kind = en.Kind;
            var o = new OriginalAudio.EnvelopeDef { kind = kind, param = unchecked((int)en.controlParam), log = false, dspMin = 0f, dspMax = 1f };
            if (kind == "dsp")
            {
                int owner = en.dspEffectIndex;
                string name = !string.IsNullOrEmpty(en.name) ? en.name : owner >= 0 && owner < siblings.Length ? siblings[owner].name : "";
                if (string.IsNullOrEmpty(name) || !DspSpec(name, en.dspParamIndex, out string k, out float mn, out float mx)) return null;   // reverb sends / unnamed: not reproduced
                o.kind = k; o.log = en.logScale; o.dspMin = mn; o.dspMax = mx;
            }
            else if (kind == "spawn_intensity") o.kind = "spawn";
            else if (kind != "volume" && kind != "pitch") return null;
            int n = en.points.Length;
            o.x = new float[n]; o.y = new float[n]; o.shape = new int[n];
            for (int i = 0; i < n; i++) { o.x[i] = en.points[i].x; o.y[i] = en.points[i].y; o.shape[i] = (int)en.points[i].shape; }
            return o;
        }

        static OriginalAudio.EventDef BuildEvent(ReflexAudioSet set, Proj p, ReflexFmod.Event e, Dictionary<(Proj, string), BankInfo> banks)
        {
            var fev = p.fev;
            var d = new OriginalAudio.EventDef
            {
                is3d = (e.mode & 0x10) != 0, minDistance = e.minDistance, maxDistance = e.maxDistance, volume = e.volume,
                pitchOctaves = 4f * e.pitch, pitchRandOctaves = 4f * e.pitchRandomization, volumeRand = e.volumeRandomization,
                fadeInMs = (int)e.fadeInMs, fadeOutMs = (int)e.fadeOutMs
            };
            d.parameters = e.parameters.Select(q => new OriginalAudio.ParamDef { name = q.name.ToLowerInvariant(), min = q.min, max = q.max }).ToArray();
            // sound definitions used by the layers, in first-use order, keyed by name (a later definition of the same name replaces the earlier)
            var used = new List<int>();
            foreach (var l in e.layers) foreach (var si in l.instances) if (!used.Contains(si.soundDef)) used.Add(si.soundDef);
            var defNames = new List<string>(); var defByName = new Dictionary<string, OriginalAudio.SoundDef>(StringComparer.Ordinal);
            foreach (int i in used)
            {
                var sd = fev.soundDefs[i]; var blk = fev.blocks[sd.block];
                var o = new OriginalAudio.SoundDef
                {
                    name = sd.name, mode = blk.PlayModeName, volume = blk.f10, volumeRandMin = blk.f1c, pitchOctaves = 4f * blk.f20, pitchRandOctaves = 4f * blk.f2c,
                    maxSpawned = blk.f04 == 0 ? 1 : (int)blk.f04, spawnMinMs = (int)blk.spawnMin, spawnMaxMs = (int)blk.spawnMax,
                    files = new string[sd.entries.Length], weights = new float[sd.entries.Length]
                };
                for (int k = 0; k < sd.entries.Length; k++)
                {
                    var en = sd.entries[k];
                    o.weights[k] = en.weight == 0 ? 100f : en.weight;
                    o.files[k] = en.kind == "wave" ? set.Resolve(p, en, banks) ?? "" : "";
                }
                if (!defByName.ContainsKey(sd.name)) defNames.Add(sd.name);
                defByName[sd.name] = o;
            }
            d.defs = defNames.Select(n => defByName[n]).ToArray();
            var layers = new List<OriginalAudio.LayerDef>();
            foreach (var l in e.layers)
            {
                var insts = new List<OriginalAudio.InstanceDef>();
                foreach (var si in l.instances)
                {
                    string name = fev.soundDefs[si.soundDef].name;
                    insts.Add(new OriginalAudio.InstanceDef
                    {
                        def = defNames.IndexOf(name), start = si.start, length = si.length, gain = si.volume, loop = si.Loop,
                        fadeIn = si.fadeIn > 0 ? si.fadeIn : 0f, fadeOut = si.fadeOut > 0 ? si.fadeOut : 0f
                    });
                }
                var envs = l.envelopes.Select(en => BuildEnvelope(en, l.envelopes)).Where(x => x != null).ToArray();
                layers.Add(new OriginalAudio.LayerDef { param = l.controlParam < e.parameters.Length ? l.controlParam : -1, instances = insts.ToArray(), envelopes = envs });
            }
            d.layers = layers.ToArray();
            return d;
        }

        /// audio_export's wave resolver: the library file name of a FEV wave entry (null when its bank or sample is missing).
        string Resolve(Proj p, ReflexFmod.WaveEntry e, Dictionary<(Proj, string), BankInfo> banks)
        {
            if (!banks.TryGetValue((p, e.bank), out var bi) || bi == null || bi.samples == null) { unresolved.Add(p.fev.name + ": " + e.wave + " (" + e.bank + "): bank missing"); return null; }
            string want = ReflexFmod.Stem(e.wave).ToLowerInvariant();
            bool Same((string stem, string file, ReflexFmod.Sample s) x)
            {
                string st = x.s.Stem.ToLowerInvariant();
                return want == st || want.StartsWith(st, StringComparison.Ordinal) || x.stem.ToLowerInvariant() == want;
            }
            bi.samples.TryGetValue((int)e.index, out var s);
            if (s.s == null || !Same(s))
            {
                s = default;
                foreach (var x in bi.samples.Values) if (x.stem.ToLowerInvariant() == want) { s = x; break; }
                if (s.s == null) { unresolved.Add(p.fev.name + ": " + e.wave + " (" + e.bank + "): sample not found"); return null; }
            }
            if (s.file == null) return null;
            if (!waveByKey.ContainsKey(s.file))
                waveByKey[s.file] = new ReflexWave { key = s.file, bank = bi.fsb, sample = s.s, bankId = bi.id, codec = ReflexFmod.CodecOf(s.s, bi.fsb.Head(s.s)) };
            return s.file;
        }
    }

    /// The game's volume channels, in the order of the executable's name table (0x9c49a0, Audio::SetVolumeChannelNames); channel
    /// k is the 0x54-byte record at 0xd679a0 + k * 0x54.
    public enum ReflexChannel { Global, Effects, UI, Music, Engine, Crowd, DriveOn, Skid, Scenario, VoiceOver }

    /// MXRegistry.xml <Audio><VolumeChannels>: per channel MasterVolume, PausedWithGame, DuckedPercentageWhenPaused, DuckTime,
    /// MutedDuringLevelTransitions (defaults = the shipped file). The user's settings act on channels as the game's settings
    /// code 0x5b9b93..0x5b9e15 does: EffectsVolume (UI setting 6) -> Effects, UI, Scenario, VoiceOver, DriveOn, Skid, Crowd;
    /// MusicVolume (7) -> Music; EngineVolume (8) -> Engine (each value * 0.01, clamped 0..1). Global has no setting.
    public sealed class ReflexVolumeChannels
    {
        public sealed class Channel { public float master = 1f, duckWhenPaused, duckTime; public bool pausedWithGame, mutedInTransitions; }
        public readonly Channel[] channels = new Channel[10];
        public enum Setting { None, Effects, Music, Engine }
        public static Setting SettingOf(ReflexChannel c) =>
            c == ReflexChannel.Music ? Setting.Music : c == ReflexChannel.Engine ? Setting.Engine : c == ReflexChannel.Global ? Setting.None : Setting.Effects;

        static ReflexVolumeChannels shared;
        public static ReflexVolumeChannels Shared { get { lock (typeof(ReflexVolumeChannels)) return shared ??= Load(); } }

        ReflexVolumeChannels()
        {
            for (int i = 0; i < channels.Length; i++) channels[i] = new Channel();
            foreach (var c in new[] { ReflexChannel.Effects, ReflexChannel.Engine, ReflexChannel.Crowd, ReflexChannel.DriveOn, ReflexChannel.Skid, ReflexChannel.Scenario })
                channels[(int)c].pausedWithGame = true;
            channels[(int)ReflexChannel.Music].duckWhenPaused = 0.5f; channels[(int)ReflexChannel.Music].duckTime = 0.5f;
        }
        public Channel this[ReflexChannel c] => channels[(int)c];

        static float Num(string s) => float.TryParse(s.Trim().TrimEnd('f', 'F'), System.Globalization.NumberStyles.Float, System.Globalization.CultureInfo.InvariantCulture, out float v) ? v : 0f;
        public static ReflexVolumeChannels Load()
        {
            var o = new ReflexVolumeChannels();
            try
            {
                string xml = System.Text.RegularExpressions.Regex.Replace(File.ReadAllText(ReflexInstall.PathOf("MXRegistry.xml")), "<!--.*?-->", "", System.Text.RegularExpressions.RegexOptions.Singleline);
                var vc = System.Text.RegularExpressions.Regex.Match(xml, "<VolumeChannels>(.*?)</VolumeChannels>", System.Text.RegularExpressions.RegexOptions.Singleline);
                if (!vc.Success) return o;
                foreach (ReflexChannel c in Enum.GetValues(typeof(ReflexChannel)))
                {
                    var m = System.Text.RegularExpressions.Regex.Match(vc.Groups[1].Value, "<" + c + ">(.*?)</" + c + ">", System.Text.RegularExpressions.RegexOptions.Singleline);
                    if (!m.Success) continue;
                    string Tag(string t) { var x = System.Text.RegularExpressions.Regex.Match(m.Groups[1].Value, "<" + t + ">(.*?)</" + t + ">", System.Text.RegularExpressions.RegexOptions.Singleline); return x.Success ? x.Groups[1].Value.Trim() : null; }
                    var ch = o.channels[(int)c];
                    if (Tag("MasterVolume") is string mv) ch.master = Num(mv);
                    if (Tag("PausedWithGame") is string pg) ch.pausedWithGame = pg.StartsWith("T", StringComparison.OrdinalIgnoreCase);
                    if (Tag("MutedDuringLevelTransitions") is string mt) ch.mutedInTransitions = mt.StartsWith("T", StringComparison.OrdinalIgnoreCase);
                    ch.duckWhenPaused = Tag("DuckedPercentageWhenPaused") is string dp ? Num(dp) : 0f;
                    ch.duckTime = Tag("DuckTime") is string dt ? Num(dt) : 0f;
                }
            }
            catch (Exception) { }
            return o;
        }
    }

    // The in-race soundtrack (MusicAudioMap.bxml in the Music database: Jukebox/Track list, per-activity timing).
    public sealed class ReflexMusicTrack
    {
        public string title, artist, song, bank; internal Rfx.Asset asset;
        public string CacheName => "mus_" + asset.package.name + "_" + asset.name + "_" + asset.size + ".mp3";
    }
    public static class ReflexMusic
    {
        public sealed class Info
        {
            public readonly List<ReflexMusicTrack> tracks = new List<ReflexMusicTrack>();
            /// Every Jukebox Track's BankName in MusicAudioMap order (the game's track indices, also for tracks whose bank is missing; w-sfx).
            public readonly List<string> trackBanks = new List<string>();
            /// The ScenarioMusic scores (MUSIC_SCORES/Music/Play's MUS_SCORE_nn banks) in MusicAudioMap order (the game's score indices; w-sfx).
            public readonly List<ReflexMusicTrack> scores = new List<ReflexMusicTrack>();
            public readonly Dictionary<string, float> scoreFadeIn = new Dictionary<string, float>(), scoreFadeOut = new Dictionary<string, float>();
            public readonly Dictionary<string, float> startDelay = new Dictionary<string, float>(), transitionTime = new Dictionary<string, float>();
            public float uiTierTransitionTime;
            public string jukeboxEvent;
        }

        static float Num(object v)
        {
            if (v == null) return 0f;
            if (v is float f) return f;
            string s = Convert.ToString(v, System.Globalization.CultureInfo.InvariantCulture).Trim().TrimEnd('f', 'F');
            return float.TryParse(s, System.Globalization.NumberStyles.Float, System.Globalization.CultureInfo.InvariantCulture, out float r) ? r : 0f;
        }

        public static Info Load()
        {
            var inst = ReflexInstall.Packages;
            var root = Rfx.Bxml(inst.Read("Music", "MusicConfig", "MusicAudioMap", "bxml"));
            var info = new Info();
            var db = inst.Db("Music");
            var jb = root.Child("Jukebox");
            if (jb == null) return info;
            var proj = jb.Child("Project"); if (proj != null) info.jukeboxEvent = proj.S("EventFullPath");
            var set = root.Child("Settings"); var tt = set?.Child("UITierTransitionTime"); if (tt != null) info.uiTierTransitionTime = Num(tt.value);
            foreach (var t in jb.children)
            {
                if (t.tag == "Track")
                {
                    string pkg = t.S("Package"), bank = t.S("BankName");
                    info.trackBanks.Add(bank ?? "");
                    Rfx.Asset a = db.Find(pkg)?.Find(bank, "soundbnk");
                    if (a == null) foreach (var pk in db.packages.Values) { a = pk.Find(bank, "soundbnk"); if (a != null) break; }
                    if (a == null) continue;
                    info.tracks.Add(new ReflexMusicTrack { artist = t.S("ArtistName"), song = t.S("SongName"), title = t.S("ArtistName") + " - " + t.S("SongName"), bank = bank, asset = a });
                }
                else if (t.tag == "ActivityStartDelay") foreach (var c in t.children) info.startDelay[c.tag] = Num(c.value);
                else if (t.tag == "ActivityTransitionTimes") foreach (var c in t.children) info.transitionTime[c.tag] = Num(c.value);
            }
            var sm = root.Child("ScenarioMusic");
            if (sm != null)
                foreach (var t in sm.children)
                {
                    if (t.tag == "Score")
                    {
                        string pkg = t.S("Package"), bank = t.S("BankName");
                        Rfx.Asset a = db.Find(pkg)?.Find(bank, "soundbnk");
                        if (a == null) foreach (var pk in db.packages.Values) { a = pk.Find(bank, "soundbnk"); if (a != null) break; }
                        info.scores.Add(a == null ? null : new ReflexMusicTrack { song = t.S("Name"), title = "Score " + t.S("Name"), bank = bank, asset = a });
                    }
                    else if (t.tag == "ActivityTransitionTimes")
                        foreach (var c in t.children)
                        {
                            var parts = Convert.ToString(c.value, System.Globalization.CultureInfo.InvariantCulture)?.Split(',');
                            if (parts == null || parts.Length == 0) continue;
                            info.scoreFadeIn[c.tag] = Num(parts[0]); info.scoreFadeOut[c.tag] = parts.Length > 1 ? Num(parts[1]) : 1f;
                        }
                }
            return info;
        }

        /// Writes the clean MPEG stream of a track's first sample to the run-time cache folder (once) and returns its path.
        public static string Extract(ReflexMusicTrack t, string cacheDir)
        {
            string dir = cacheDir; Directory.CreateDirectory(dir);
            string path = Path.Combine(dir, t.CacheName);
            if (File.Exists(path) && new FileInfo(path).Length > 0) return path;
            var fsb = ReflexFmod.ParseFsb(ReflexInstall.Packages.Read(t.asset));
            var s = fsb.samples[0];
            var mp3 = ReflexFmod.CleanMpeg(fsb.Raw(s), out _, out _, out _, out _);
            string tmp = path + "." + Environment.CurrentManagedThreadId + ".tmp";
            File.WriteAllBytes(tmp, mp3);
            try { File.Move(tmp, path); } catch (IOException) { File.Delete(tmp); }
            return path;
        }
    }
}

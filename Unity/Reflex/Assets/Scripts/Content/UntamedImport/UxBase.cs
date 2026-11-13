// MX vs ATV Untamed converter, assembly parts: the port of the Python reference converter's ux_base.py for the STOCK clone base (a stock Reflex
// SX level, SX_FortDodge, read from the player's Reflex library and reduced in memory to the race / free-ride kit).
//   UxBase.Resolve / Descriptor      resolve('stock', level) / Base: kind, stem, level, roles, policy, record()
//   UxBase.KeepEntity                keep_entity(pol, suffix, e): the keep policy
//   UxBase.Prepare / LoadPrepared    prepare(base, subs, level_bxml) / load_prepared(base): kept sub-scenes, chunk regrouping (<= 30
//                                    entities), the synthesised DistanceFog, the level-file edit, the record
//   UxBase.HfRoles                   hf_roles(subs, stem, ts): the HF asset roles (Custom_* role name -> the base's asset name)
//   UxBase.IntroTrackPart / ScriptKinds / ScriptRenames     the scenario script renames
//   UxBase.CORE_LITERALS / LiteralRows / LiteralRoots / CoreGlobalAssets / LOAD_ORDER_DEPS / CORE_FIELDS / CORE_GLOBAL_DBS
//   UxBase.AnimSets / SessionAnims / AssetStrings / EntityStrings / Closure     the asset closure of the kept entities
//   UxBase.TexBc / ConstantTexture   tex_bc(img, fmt, straight_alpha) (= ux_build_track.tex_bc) / constant_texture(colour, w, h, fmt)
// Left out (not on the stock-base build path): the DE sketch base (SKETCH_ROLES, resolve('sketch'), for_folder), ensure_stock_copy (the
// stock files come from the library through UxStock.BaseFile / BaseSet, checked against the same md5 pins), detect_base (verifier) and
// the provenance helpers package_md5s / lib_md5s / foreign_bytes / constant_recipe (verifier).
// UxRace.cs holds a private subset of this policy (UxRaceTemplate: its keep rule and roles); both follow the same Python tables.
// Pure C# (System only).
using System;
using System.Collections.Generic;
using System.Globalization;
using System.Text;
using Reflex.Port.Content.AliveImport;

namespace Reflex.Port.Content.UntamedImport
{
    /// ux_base.py (the stock base).
    public static class UxBase
    {
        public const string DEFAULT_STOCK_LEVEL = "SX_FortDodge";
        public const long BASE_SYNTH_ID = 18000001;
        public static readonly string[] KEEP_SUBS = { "Base", "FreeRide", "SectionRace" };
        public static readonly string[] INTRO_KINDS = { "Intro", "StartLine", "StartStaggered", "HeroBike", "HeroVehicle", "30SecGirl", "WinFinish", "WinMound",
            "WinPodium", "WinHeatChief", "LoseWalk", "LoseHeatChief", "2Heroes", "OutrunLose" };
        public static readonly string[] INTRO_SERIES = { "NAT", "SX", "SM", "FMX", "FMJ", "PX", "TX", "UTV", "FR", "X" };
        static readonly string[] GATE_COMMON = { @"^Anchor_StartLine_(LensFlare|TrophyGirl|FreeStyleGirl)$", @"^Anchor_LoseWalk", @"^Anchor_LoseHeatChief_HumanPlayerLost" };
        static readonly string[] BOARD_PATTERNS = { @"^Anchor_30SecGirl", @"^Anchor_StartLine_(30SecGirl|CardGirl|CheckGirl|CameraGuyStrafe)$" };
        static string I(long v) => v.ToString(CultureInfo.InvariantCulture);

        // ================================================================================================ policy
        /// Base.roles: the names the race kit moves.
        public sealed class Roles
        {
            public string dropgate, startgate, board_camera; public string[] gate_patterns, board_patterns, startgate_followers;
            public JObj Record() => new JObj().Set("dropgate", dropgate).Set("startgate", startgate).Set("gate_patterns", new List<object>(gate_patterns))
                .Set("board_patterns", new List<object>(board_patterns)).Set("startgate_followers", new List<object>(startgate_followers)).Set("board_camera", board_camera);
        }
        /// STOCK_POLICY[level].
        public sealed class Policy
        {
            public Roles roles; public string intro_track, hf_heightmap;
            public string[] single_types, all_of_type, bump_models, freeride_types, sectionrace_types, implicit_sounds, script_kinds;
            public List<KeyValuePair<string, string[]>> named = new List<KeyValuePair<string, string[]>>();
            public (string suffix, int count)[] expect; public (string name, string type)[] implicit_roots;
            /// 'race', or 'freeride' for a policy made by FreeridePolicy.
            public string kit = "race";
            public string[] Named(string type) { foreach (var kv in named) if (kv.Key == type) return kv.Value; return null; }
        }
        static readonly string[] SINGLE_TYPES = { "HFTerrainRSData", "ShadowMapRSData", "TDFEntityRSData", "AmbientDirectionalLightRSData", "SkyRSData",
            "HDRSettingsRSData", "MudManagerRSData", "VolumeFogRSData", "ReflectionMapGenRSData", "OverlayLightRSData", "ParticleManagerRSData",
            "AudioCollisionManagerRSData", "AudioTerrainZoneManagerRSData", "LevelAmbientAudioZoneRSData", "AudioVolumeLevelMixRSData", "LensFlareRSData",
            "DropGateRSData", "ThirtySecondBoardRSData" };
        static string[] FdAnchors()
        {
            var a = new List<string> { "Anchor_Intro", "Anchor_30SecGirl" };
            for (int i = 1; i < 12; i++) a.Add("Anchor_30SecGirl_CameraHound" + i.ToString("00", CultureInfo.InvariantCulture));
            a.Add("Anchor_StartLine");
            foreach (string s in new[] { "TrophyGirl", "LensFlare", "FreeStyleGirl", "CheckGirl", "CardGirl", "CameraGuyStrafe", "30SecGirl" }) a.Add("Anchor_StartLine_" + s);
            a.Add("Anchor_WinFinish"); a.Add("Anchor_LoseHeatChief_HumanPlayerLost1"); a.Add("Anchor_LoseWalk");
            for (int i = 4; i < 13; i++) a.Add("Anchor_LoseWalk_PlayerCC" + I(i));
            for (int i = 4; i < 13; i++) a.Add("Anchor_LoseWalk_PlayerFinished" + I(i));
            return a.ToArray();
        }
        static string[] FdActors()
        {
            var a = new List<string> { "CameraPerson01", "CameraPerson02", "CameraGuyStrafe_B", "SC_TrackSideFlagHolder", "30SecGirl", "CardGirl", "CheckGirl", "FreeStyleGirl", "TrophyGirl" };
            foreach (string p in new[] { "CameraHound_", "PitCrewPerson_", "ScoreTowerPerson_" })
                for (int i = 1; i < 13; i++) a.Add(p + i.ToString("00", CultureInfo.InvariantCulture));
            for (int i = 1; i < 16; i++) if (i != 8) a.Add("CrowdPerson" + i.ToString("00", CultureInfo.InvariantCulture));
            return a.ToArray();
        }
        static string[] Cat(string first, string[] rest) { var l = new List<string> { first }; l.AddRange(rest); return l.ToArray(); }
        static readonly Policy FORTDODGE = new Policy
        {
            roles = new Roles { dropgate = "DropGate", startgate = "TO_StartingGate", gate_patterns = Cat(@"^Anchor_StartLine$", GATE_COMMON), board_patterns = BOARD_PATTERNS,
                startgate_followers = new[] { "SX_FD_AF_Camera_Startline" }, board_camera = "SX_FD_AF_Camera_30Second" },
            intro_track = "FD_AF",
            single_types = SINGLE_TYPES,
            named = new List<KeyValuePair<string, string[]>>
            {
                new KeyValuePair<string, string[]>("CollisionRSData", new[] { "TO_StartingGate" }),
                new KeyValuePair<string, string[]>("AnchorEntityRSData", FdAnchors()),
                new KeyValuePair<string, string[]>("ScenarioAnimEntityRSData", FdActors()),
                new KeyValuePair<string, string[]>("MXCameraPathRSData", new[] { "SX_FD_AF_Camera_Startline", "SX_FD_AF_Camera_30Second" }),
                new KeyValuePair<string, string[]>("GeneralStorageEntityRSData", new[] { "CAMANIMS_Intro_StorageEntity", "GLOBAL_CrewChief_ALL_StorageEntity", "SC_EventPeopleStorageEntity" }),
            },
            all_of_type = new[] { "CrewChiefRSData" },
            bump_models = new[] { "SX_FD_MainBump" },
            freeride_types = new[] { "StartEntityFreeRideRSData" },
            sectionrace_types = new[] { "LocalizedPackageReferenceEntityRSData" },
            expect = new[] { ("Base", 139), ("FreeRide", 12), ("SectionRace", 1) },
            implicit_sounds = new[] { "SCENARIOS_GLOBAL" },
            implicit_roots = new[] { ("SCENARIOS_GLOBAL", "sound") },
            script_kinds = new[] { "Intro", "StartLine", "HeroBike", "30SecGirl", "WinFinish", "LoseWalk" },
            hf_heightmap = "SX_FortDodgeA_DISP",
        };
        /// STOCK_POLICY.get(level).
        public static Policy StockPolicy(string level) => level == "SX_FortDodge" ? FORTDODGE : null;

        // Free-ride kit (FREESTYLE.md): a stock base reduced for a track with no race (the freestyle arenas, slot Beta_Fmx_Track_Slot_N, FREE RIDE
        // only). Same environment / audio / terrain singletons and bump model as the race kit, without the race and cut-scene kit (TDFEntity,
        // the gate kit, the 30 s board, crew chiefs, anchors, scenario actors, camera paths, anim storages) and without the SectionRace
        // sub-scene: the shape of the in-game-proven DE HuCkiT free-ride template. Open Reflex's free ride reads only <L>FreeRide's
        // StartEntityFreeRideRSData; its intro scenario FR_<5 letters>_Intro is absent, which the core treats as no script.
        public static readonly string[] KIT_CHOICES = { "race", "freeride" };
        static readonly string[] FREERIDE_DROP_TYPES = { "TDFEntityRSData", "DropGateRSData", "ThirtySecondBoardRSData" };
        public static readonly string[] FREERIDE_SUBS = { "Base", "FreeRide" };
        /// FREERIDE_EXPECT[stem].
        static (string suffix, int count)[] FreerideExpect(string stem) => stem == "SX_FortDodge" ? new[] { ("Base", 16), ("FreeRide", 12) } : null;
        /// freeride_policy(stem, pol): the race-kit policy of a stock base reduced to the free-ride kit.
        public static Policy FreeridePolicy(string stem, Policy pol)
        {
            var expect = FreerideExpect(stem) ?? throw new UxException("no free-ride kit count for stock base " + stem + " (known: ['SX_FortDodge'])");
            var single = new List<string>(); foreach (string t in pol.single_types) if (!In(FREERIDE_DROP_TYPES, t)) single.Add(t);
            return new Policy
            {
                roles = pol.roles, intro_track = pol.intro_track, hf_heightmap = pol.hf_heightmap, single_types = single.ToArray(),
                named = new List<KeyValuePair<string, string[]>>(), all_of_type = new string[0], bump_models = pol.bump_models,
                freeride_types = pol.freeride_types, sectionrace_types = new string[0], expect = expect, script_kinds = new string[0],
                implicit_sounds = new string[0], implicit_roots = new (string, string)[0], kit = "freeride",
            };
        }

        /// ENGINE_ANIM_SETS: (set name, the RSData type that needs it or null = always).
        public static readonly (string set, string type)[] ENGINE_ANIM_SETS = { ("30SecondGirlShowBoard", "ThirtySecondBoardRSData"), ("HiResCrewChiefLoseWalk_PushBike", null) };

        // ================================================================================================ core literal names
        /// One CORE_LITERALS row.
        public sealed class Literal
        {
            public readonly string feature, name, type, miss, where, source;
            public Literal(string feature, string name, string type, string miss, string where, string source) { this.feature = feature; this.name = name; this.type = type; this.miss = miss; this.where = where; this.source = source; }
        }
        public static readonly Literal[] CORE_LITERALS =
        {
            new Literal("always", "PhysicsProperties", "bxml", "abort", "global", "core build_world.c:431 rx_asset_handle (EngineDependencies)"),
            new Literal("HFTerrainRSData", "HFT_Helper", "shader", "abort", "global", "core build_terrain.c:181 rx_asset_handle (EngineDependencies)"),
            new Literal("HFTerrainRSData", "HFT_Deform_Default", "shader", "abort", "package", "core build_terrain.c:381 rx_asset_handle"),
            new Literal("HFTerrainRSData", "HFT_Track_Default", "shader", "abort", "package", "core build_terrain.c:388 rx_asset_handle"),
            new Literal("LensFlareRSData", "BillboardQuad", "surface", "abort", "package", "core build_scene_simple.c:280 rx_lens_flare_vf5 rx_asset_handle"),
            new Literal("LensFlareRSData", "LensFlare", "material", "abort", "package",
                "core build_scene_simple.c:281 rx_lens_flare_vf5 rx_asset_handle (+ shader LensFlare, texture LightFlare by the material walk)"),
            new Literal("BillboardCrowdRSData", "Crowd", "shader", "abort", "package",
                "core build_scene_crowd.c:169-170 (act_unreached when absent or not loaded); client ReflexAmbientContent.cs:220 (caught: soft)"),
            new Literal("BillboardCrowdRSData", "CameraFlash", "texture", "soft", "package",
                "core build_scene_crowd.c:153; client ReflexAmbientContent.cs:205 HasAsset (DESIGN 18; V5.14)"),
            new Literal("ParticleManagerRSData", "Editor_Default_White", "texture", "soft", "package", "core scene_fx_entities.c:386 rx_particle_entity_vf5"),
            new Literal("CrewChiefRSData", "CrewChief_ShirtColor_Default", "texture", "fault", "package",
                "core anim_scenario_misc.c:79-81 CrewChief_SetShirt (handle +0x4c written unchecked)"),
            new Literal("VolumeFogRSData", "VolumeFog", "material", "original", "package",
                "MXReflex.exe string \"VolumeFog\" (VolumeFogEntity, render); core outside_core 0x105f (build_scene.c:518), client not drawn (ReflexLightGlow.cs:147); stock FortDodge and every sketch build carry it (fix round)"),
            new Literal("WaterSystemRSData", "Water_Wake", "shader", "abort", "package", "core build_water.c:389-390 fatal"),
            new Literal("WaterSystemRSData", "WaterPressure", "texture", "soft", "package", "core build_water.c:389; client OriginalWater.cs:134"),
            new Literal("ForestSystemRSData", "TreeShader", "shader", "abort", "global", "core build_forest.c:428-429 fatal (EngineDependencies)"),
        };
        public static readonly string[] CORE_LITERAL_FAIL = { "abort", "fault" };
        /// LOAD_ORDER_DEPS: (asset type that loads, needed name, needed type, source).
        public static readonly (string type, string name, string needType, string source)[] LOAD_ORDER_DEPS =
        {
            ("water", "Water_Wake", "shader", "core build_water.c:566-621 water_load -> 458 surface_init -> 380 wake_setup: 389-391 fatal \"the Water_Wake shader asset is not loaded\" (fairview-v3 / lincoln-v3 in game 2026-11-10)"),
            ("forest", "TreeShader", "shader", "core build_forest.c:428-429 fatal \"the TreeShader shader asset is not loaded\""),
        };
        static readonly string[] MODEL_FIELDS = { "ModelAsset", "MaterialList" };
        static readonly string[] COLL_FIELDS = { "ModelAsset", "MaterialList", "MaleCollisionAsset", "FemaleCollisionAsset" };
        static string[] Plus(string[] a, params string[] b) { var l = new List<string>(a); l.AddRange(b); return l.ToArray(); }
        /// CORE_FIELDS: the asset fields the core's entity builders read with abort, by RSData type (the verifier's V4.13 table).
        public static readonly Dictionary<string, string[]> CORE_FIELDS = new Dictionary<string, string[]>(StringComparer.Ordinal)
        {
            { "ModelRSData", MODEL_FIELDS }, { "LWModelRSData", MODEL_FIELDS }, { "MassiveAdEntityRSData", MODEL_FIELDS }, { "SkyRSData", MODEL_FIELDS },
            { "HFBumpModelRSData", MODEL_FIELDS }, { "BillboardCrowdRSData", MODEL_FIELDS }, { "CrewChiefRSData", MODEL_FIELDS },
            { "ScenarioAnimEntityRSData", MODEL_FIELDS }, { "ScrollingTextureRSData", MODEL_FIELDS }, { "CameraFacingEntityRSData", MODEL_FIELDS },
            { "CollisionRSData", COLL_FIELDS }, { "DropGateRSData", COLL_FIELDS }, { "PhysicsRSData", COLL_FIELDS }, { "StepUpBarRSData", COLL_FIELDS },
            { "PopUpBaleRSData", COLL_FIELDS }, { "PopDownBaleRSData", COLL_FIELDS },
            { "ThirtySecondBoardRSData", Plus(MODEL_FIELDS, "ShowSign", "RunAnim", "WalkAnim", "IdleAnimation") },
            { "TrackSideFlagHolderRSData", Plus(MODEL_FIELDS, "WaveFlag", "IdleAnimation", "PassingFlag", "YellowFlag") },
            { "ScoreTowerFlagHolderRSData", Plus(MODEL_FIELDS, "WaveFlag", "IdleAnimation", "CheckeredFlag", "WhiteFlag") },
            { "AnimEntitySingleAnimLoopRSData", Plus(MODEL_FIELDS, "LoopingAnimation") },
            { "AnimEntitySplineAnimLoopRSData", Plus(MODEL_FIELDS, "LoopingAnimation", "SoundActions", "AnimationSpline") },
            { "MXCameraPathRSData", new[] { "Animation Asset" } },
            { "ShadowMapRSData", new[] { "StaticShadowMapAsset", "VolumeShadowMapAsset" } },
            { "TDFEntityRSData", new[] { "TDFAssetList" } },
            { "LensFlareRSData", new[] { "LensFlareTextureList" } },
            { "HFTerrainRSData", new[] { "HeightMapAsset", "MaterialAsset", "SoftnessMask", "FrictionZones", "TerrainDeformation" } },
            { "ParticleManagerRSData", new[] { "ParticleEmitterAsset", "TextureAsset", "ParticleAttribAsset" } },
            { "WaterSystemRSData", new[] { "WaterAsset", "MaterialAsset" } },
            { "ForestSystemRSData", new[] { "ForestAsset" } },
        };
        /// CORE_GLOBAL_DBS: (database, the package the core loads from it).
        public static readonly (string db, string package)[] CORE_GLOBAL_DBS =
        {
            ("EngineDependencies", "EngineDependencies"), ("Animations", "Animations"), ("RiderPhysics", "RiderPhysics"), ("AIData", "AIData"),
            ("SnakeDatabase", "SnakePackage"), ("StuntDatabase", "StuntData"),
        };

        /// literal_rows(types): the CORE_LITERALS rows that apply to a scene holding the RSData types.
        public static List<Literal> LiteralRows(ICollection<string> types)
        {
            var o = new List<Literal>();
            foreach (var r in CORE_LITERALS) if (r.feature == "always" || types.Contains(r.feature)) o.Add(r);
            return o;
        }
        /// literal_roots(types): [(name, type)] the builder keeps as closure roots: every 'package' row that applies.
        public static List<(string name, string type)> LiteralRoots(ICollection<string> types)
        {
            var o = new List<(string, string)>();
            foreach (var r in LiteralRows(types)) if (r.where == "package" && !o.Contains((r.name, r.type))) o.Add((r.name, r.type));
            return o;
        }
        /// core_global_assets(): {(lower name, type): 'Database/Package'} of the assets in the packages the core loads globally.
        public static Dictionary<(string lower, string type), string> CoreGlobalAssets(IUxLibrary library)
        {
            var o = new Dictionary<(string, string), string>();
            foreach (var (dbn, pkn) in CORE_GLOBAL_DBS)
            {
                if (library == null || !library.HasLevel(dbn)) continue;
                UxBxml db;
                try { db = new UxBxml(library.ReadLevelFile(dbn, "database")); } catch (Exception) { continue; }
                int pk = db.Find(0, "Packages");
                if (pk < 0) continue;
                foreach (int p in db.Children(pk))
                {
                    if (db.Name(p) != "Package") continue;
                    string pname = db.Attr(p, "name") as string;
                    foreach (int an in db.Children(p))
                    {
                        if (db.Name(an) != "Asset") continue;
                        string name = db.Attr(an, "name") as string, typ = db.Attr(an, "type") as string;
                        if (db.Find(an, "Heap") < 0 || string.IsNullOrEmpty(name) || string.IsNullOrEmpty(typ)) continue;
                        if (pname != pkn) continue;
                        var key = (UxCommon.PyLower(name), typ);
                        if (!o.ContainsKey(key)) o[key] = dbn + "/" + pkn;
                    }
                }
            }
            return o;
        }

        // ================================================================================================ base descriptor
        /// ux_base.Base of kind 'stock'.
        public sealed class Descriptor
        {
            public readonly string kind = "stock", stem, level, kit; public readonly Policy policy; public readonly Roles roles;
            internal Descriptor(string level, string kit = "race")
            {
                if (!In(KIT_CHOICES, kit)) throw new UxException("kit '" + kit + "' not in ('race', 'freeride')");
                stem = level; this.level = level; this.kit = kit;
                policy = StockPolicy(level) ?? throw new UxException("no stock base policy for " + level + " (known: ['SX_FortDodge'])");
                roles = policy.roles;
                if (kit == "freeride") policy = FreeridePolicy(stem, policy);
            }
            /// Base.folder as Python writes it (templates/stock-<level>).
            public string Folder(UxPaths paths = null) => (paths ?? new UxPaths()).Templates("stock-" + stem);
            /// Base.record().
            public JObj Record(UxPaths paths = null)
            {
                var r = new JObj().Set("kind", kind).Set("folder", Folder(paths)).Set("stem", stem).Set("level", level).Set("roles", roles.Record());
                if (kit != "race") r.Set("kit", kit);
                return r;
            }
        }
        /// resolve('stock', base_level, None, 'sx', kit=kit): the stock base of an sx build; kit 'freeride' = FREESTYLE.md.
        public static Descriptor Resolve(string baseLevel = DEFAULT_STOCK_LEVEL, string kind = "sx", string kit = "race")
        {
            if (kind != "sx") throw new UxException("--base stock is for sx builds only");
            if (UxStock.StockPin(baseLevel, "database") == null) throw new UxException("no md5 pins for stock level " + baseLevel);
            return new Descriptor(baseLevel, kit);
        }

        // ================================================================================================ scene helpers
        /// _name(e): the Name prop's value, or null.
        public static string NameOf(UxElem e)
        {
            var p = UxPcScene.Prop(e, "Name");
            return p != null && p.children.Count > 0 ? p.children[0].value as string : null;
        }
        static string ModelAsset(UxElem e)
        {
            var p = UxPcScene.Prop(e, "ModelAsset");
            return p == null || p.children.Count == 0 ? null : p.children[0].Attr("name") as string;
        }
        static bool In(string[] set, string v) { if (v == null || set == null) return false; foreach (string s in set) if (s == v) return true; return false; }

        /// keep_entity(pol, suffix, e): (keep?, rule) for one stock entity under the policy.
        public static (bool keep, string rule) KeepEntity(Policy pol, string suffix, UxElem e)
        {
            string t = UxPcScene.Ns(e), nm = NameOf(e);
            if (suffix == "FreeRide") { bool k = In(pol.freeride_types, t); return (k, k ? "free-ride start" : "FreeRide extra"); }
            if (suffix == "SectionRace") return (In(pol.sectionrace_types, t), "SectionRace reference");
            if (suffix != "Base") return (false, "sub-scene dropped");
            if (In(pol.single_types, t)) return (true, "environment / audio / terrain / race kit singleton");
            if (In(pol.all_of_type, t)) return (true, "every " + t);
            if (t == "HFBumpModelRSData")
            {
                bool ok = In(pol.bump_models, ModelAsset(e));
                return (ok, ok ? "plane bump model" : "FortDodge-track-shaped bump model");
            }
            var names = pol.Named(t);
            if (names != null && In(names, nm)) return (true, "race / cut-scene kit role");
            return (false, "FortDodge stadium / track content");
        }

        /// A prepared base: load_prepared(base).
        public sealed class Prepared
        {
            public Descriptor @base; public UxTrackSet ts; public List<UxSubScene> subs; public UxBxml level; public JObj record;
            public UxSubScene Sub(string suffix) { foreach (var s in subs) if (s.name == @base.stem + suffix) return s; return null; }
        }

        static void SetEntityId(UxElem e, long id)
        {
            foreach (var a in e.attrs) if (a.name == "entityId") { var b = new byte[4]; UxBin.PutU32(b, 0, (uint)id); a.raw = b; a.str = null; }
        }
        static JObj Counted(SortedDictionary<string, long> d) { var j = new JObj(); foreach (var kv in d) j.items.Add(new KeyValuePair<string, object>(kv.Key, kv.Value)); return j; }
        static List<object> Names(IEnumerable<UxSubScene> subs) { var l = new List<object>(); foreach (var s in subs) l.Add(s.name); return l; }
        static bool SameAttrFlags(UxBxml a, UxBxml b)
        {
            if (a.attrs.Count != b.attrs.Count) return false;
            var x = new List<(long, long)>(); var y = new List<(long, long)>();
            foreach (var t in a.attrs) x.Add((t[2], t[3]));
            foreach (var t in b.attrs) y.Add((t[2], t[3]));
            x.Sort(); y.Sort();
            for (int i = 0; i < x.Count; i++) if (x[i] != y[i]) return false;
            return true;
        }

        /// prepare(base, subs, level_bxml): reduces a stock base in memory. subs (scene.load) are changed and returned filtered;
        /// the level file is returned re-encoded with the dropped sub-scenes removed. stock: the DistanceFog prototype.
        public static (List<UxSubScene> subs, UxBxml level, JObj record) Prepare(Descriptor b, List<UxSubScene> subs, UxBxml levelBxml, UxStock stock, Action<string> log = null)
        {
            var pol = b.policy; string stem = b.stem;
            string[] keepSubs = b.kit == "freeride" ? FREERIDE_SUBS : KEEP_SUBS;
            var rec = new JObj().Set("policy", b.kit == "race" ? "mirror (the DE sketch kit roles)"
                : "free-ride kit (FREESTYLE.md: the HuCkiT free-ride template shape, no race / cut-scene kit)").Set("subscenes_before", Names(subs));
            var keptSubs = new List<UxSubScene>(); var droppedSubs = new List<UxSubScene>();
            foreach (var s in subs)
            {
                string suf = s.name.StartsWith(stem, StringComparison.Ordinal) ? s.name.Substring(stem.Length) : null;
                (In(keepSubs, suf) ? keptSubs : droppedSubs).Add(s);
            }
            rec.Set("subscenes_dropped", Names(droppedSubs));
            var stripped = new SortedDictionary<string, long>(StringComparer.Ordinal); var kept = new SortedDictionary<string, long>(StringComparer.Ordinal);
            var ids = new HashSet<long>();
            int maxChunk = UxCommon.Limits.MaxEntitiesPerChunk;
            foreach (var s in keptSubs)
            {
                string suf = s.name.Substring(stem.Length);
                var chunks = new List<UxElem>(); var vers = new List<(long, long)>();
                int n = Math.Min(s.chunks.Count, s.versions.Count);
                for (int ci = 0; ci < n; ci++)
                {
                    var c = s.chunks[ci];
                    var k = new List<UxElem>();
                    foreach (var e in c.children)
                    {
                        string key = suf + "/" + UxPcScene.Ns(e);
                        if (KeepEntity(pol, suf, e).keep) { k.Add(e); kept[key] = (kept.TryGetValue(key, out long v) ? v : 0) + 1; ids.Add(UxPcScene.EntityId(e)); }
                        else stripped[key] = (stripped.TryGetValue(key, out long v2) ? v2 : 0) + 1;
                    }
                    for (int i = 0; i < k.Count; i += maxChunk)
                    {
                        var at = new List<UxAttr>(); foreach (var a in c.attrs) at.Add(a.Copy());
                        chunks.Add(new UxElem(c.tag, at, c.vtype, c.vcount, c.value, k.GetRange(i, Math.Min(maxChunk, k.Count - i))));
                        vers.Add(s.versions[ci]);
                    }
                }
                s.chunks = chunks; s.versions = vers;
            }
            // counts against the policy expectation
            var got = new JObj(); bool same = true; var want = new JObj();
            foreach (string suf in keepSubs)
            {
                long n = 0; foreach (var kv in kept) if (kv.Key.Substring(0, kv.Key.IndexOf('/')) == suf) n += kv.Value;
                got.Set(suf, n);
            }
            foreach (var (suffix, count) in pol.expect) { want.Set(suffix, (long)count); if (!(got.Get(suffix) is long g) || g != count) same = false; }
            if (!same || got.items.Count != pol.expect.Length)
                throw new UxException("stock base " + stem + ": kept " + UxJson.Dumps(got, -1) + ", policy expects " + UxJson.Dumps(want, -1) + " (stale policy?)");
            UxSubScene baseSub = null;
            foreach (var s in keptSubs) if (s.name == stem + "Base") { baseSub = s; break; }
            if (baseSub == null) throw new UxException("stock base " + stem + ": no " + stem + "Base sub-scene");
            var tb = new Dictionary<string, int>(StringComparer.Ordinal);
            foreach (var e in baseSub.Entities()) { string t = UxPcScene.Ns(e); tb[t] = (tb.TryGetValue(t, out int c) ? c : 0) + 1; }
            var miss = new List<string>();
            foreach (string t in pol.single_types) if (!tb.TryGetValue(t, out int c) || c != 1) miss.Add(t + ": " + (tb.TryGetValue(t, out int c2) ? c2 : 0));
            if (miss.Count > 0) throw new UxException("stock base " + stem + ": singletons not exactly once: {" + string.Join(", ", miss) + "}");
            // DistanceFog: FortDodge has none; the builder needs exactly one (ux_entities.distance_fog -> 'DistanceFog')
            var synth = new List<object>();
            if (!tb.ContainsKey("DistanceFogRSData"))
            {
                var (proto, ver, unk) = stock.LibPrototype("DistanceFogRSData");
                var e = proto.DeepCopy();
                SetEntityId(e, BASE_SYNTH_ID);
                var P = UxEntities.PropsOf(e);
                UxEntities.SetValue(P["Name"], "DistanceFog");
                UxEntities.SetValue(P["Id"], BASE_SYNTH_ID);
                if (ids.Contains(BASE_SYNTH_ID)) throw new UxException("BASE_SYNTH_ID " + I(BASE_SYNTH_ID) + " already used by the stock base");
                baseSub.chunks.Add(new UxElem("SceneChunk", null, 0, 0, null, new List<UxElem> { e }));
                baseSub.versions.Add(baseSub.versions.Count > 0 ? baseSub.versions[0] : (ver, unk));
                UxStockPrototype idx = null;
                foreach (var p in UxStock.LibPrototypes) if (p.rsdata == "DistanceFogRSData") idx = p;
                synth.Add(new JObj().Set("type", "DistanceFogRSData").Set("name", "DistanceFog").Set("id", BASE_SYNTH_ID)
                    .Set("source", "templates/reflex-lib/prototypes/DistanceFogRSData.bxml (stock " + idx.SourceScene + " entity " + I(idx.entityId) + ")")
                    .Set("prototype_md5", idx.md5));
            }
            rec.Set("synthesised", synth);
            // level file: drop the <Subscene> entries of the dropped sub-scenes (tree edit), after proving the re-encode is the same document
            var tree = levelBxml.ToTree();
            var again = new UxBxml(UxBxml.EncodeTree(tree, levelBxml.version, levelBxml.unknown));
            if (again.ToXml() != levelBxml.ToXml() || !SameAttrFlags(again, levelBxml))
                throw new UxException("stock level file does not re-encode to the same document through bxml.encode_tree");
            var drop = new HashSet<string>(StringComparer.Ordinal); foreach (var s in droppedSubs) drop.Add(s.name);
            int removedCount = 0;
            void Walk(UxElem x)
            {
                int before = x.children.Count;
                var keepKids = new List<UxElem>();
                foreach (var c in x.children) if (!(UxCommon.PyLower(c.tag) == "subscene" && c.Attr("name") is string cn && drop.Contains(cn))) keepKids.Add(c);
                x.children = keepKids;
                removedCount += before - keepKids.Count;
                foreach (var c in x.children) Walk(c);
            }
            Walk(tree);
            if (removedCount != drop.Count) throw new UxException("level file: removed " + removedCount + " sub-scene entries, want " + drop.Count);
            var newLevel = new UxBxml(UxBxml.EncodeTree(tree, levelBxml.version, levelBxml.unknown));
            var listed = new List<object>();
            foreach (var x in newLevel.ToTree().Iter()) if (UxCommon.PyLower(x.tag) == "subscene") listed.Add(x.Attr("name"));
            bool sameList = listed.Count == keptSubs.Count;
            for (int i = 0; sameList && i < listed.Count; i++) sameList = listed[i] as string == keptSubs[i].name;
            if (!sameList) throw new UxException("level file lists " + UxJson.Dumps(listed, -1) + ", scene keeps " + UxJson.Dumps(Names(keptSubs), -1));
            var chunkCounts = new JObj(); foreach (var s in keptSubs) chunkCounts.Set(s.name, (long)s.chunks.Count);
            rec.Set("kept", Counted(kept)).Set("stripped", Counted(stripped)).Set("counts", got).Set("chunks", chunkCounts).Set("level_subscenes", listed);
            if (log != null)
            {
                long ns = 0; foreach (var kv in stripped) ns += kv.Value;
                log("stock base " + stem + ": kept " + UxJson.Dumps(got, -1) + ", stripped " + I(ns) + " entities, dropped " + drop.Count + " sub-scenes, synthesised " + synth.Count);
            }
            return (keptSubs, newLevel, rec);
        }

        /// load_prepared(base): the base's TrackSet, prepared sub-scenes, prepared level file and record, from the player's library.
        /// Every call loads a fresh scene (the builder edits it).
        public static Prepared LoadPrepared(Descriptor b, UxStock stock, Action<string> log = null)
        {
            var ts = stock.BaseSet(b.stem);
            var subs = UxPcScene.Load(stock.BaseFile("scene", b.stem));
            var lvb = new UxBxml(stock.BaseFile("level", b.stem));
            var (ks, nl, rec) = Prepare(b, subs, lvb, stock, log);
            return new Prepared { @base = b, ts = ts, subs = ks, level = nl, record = rec };
        }

        // ================================================================================================ terrain roles
        /// TERRAIN_SLOTS: the sketch's role name of each HF material sampler slot.
        public static readonly (string role, int slot)[] TERRAIN_SLOTS = { ("Custom_TopDown", 0), ("Custom_Base", 1), ("Custom_Dirt_R", 2), ("Custom_Dirt_G", 3),
            ("Custom_Dirt_B", 4), ("Custom_DetailMasks", 5), ("Custom_CHR_LUM", 6) };

        /// ux_verify_codec.material_info(data)['textures']: the textureName of every Texture element, in document order.
        public static List<string> MaterialTextures(byte[] material)
        {
            var o = new List<string>();
            foreach (var e in new UxBxml(material).ToTree().Iter()) if (e.tag == "Texture") o.Add(e.Attr("textureName") as string);
            return o;
        }

        /// The HF asset roles of a base (hf_roles).
        public sealed class HfRoleSet
        {
            public string entity; public (string name, string type)? heightmap, material, softness, zones;
            public List<string> textures = new List<string>();
            public UxOrdered<string, string> by_role = new UxOrdered<string, string>(StringComparer.Ordinal);
            static object Pair((string name, string type)? k) => k == null ? null : (object)new List<object> { k.Value.name, k.Value.type };
            public JObj Record()
            {
                var by = new JObj(); foreach (var kv in by_role) by.Set(kv.Key, kv.Value);
                var tx = new List<object>(); foreach (string t in textures) tx.Add(t);
                return new JObj().Set("entity", entity).Set("heightmap", Pair(heightmap)).Set("material", Pair(material)).Set("softness", Pair(softness))
                    .Set("zones", Pair(zones)).Set("textures", tx).Set("by_role", by);
            }
            /// by_role[role], or null.
            public string Role(string role) => by_role.Get(role);
        }

        /// hf_roles(subs, stem, ts): entity name, HeightMapAsset, MaterialAsset, SoftnessMask, FrictionZones and the HF material's sampler
        /// textures; by_role maps the sketch's Custom_* role names to the base's names.
        public static HfRoleSet HfRoles(List<UxSubScene> subs, string stem, UxTrackSet ts)
        {
            UxSubScene bs = null;
            foreach (var s in subs) if (s.name == stem + "Base") { bs = s; break; }
            var hf = new List<UxElem>();
            if (bs != null) foreach (var e in bs.Entities()) if (UxPcScene.Ns(e) == "HFTerrainRSData") hf.Add(e);
            if (hf.Count != 1) throw new UxException(hf.Count + " HFTerrainRSData in " + stem + "Base");
            var P = UxEntities.PropsOf(hf[0]);
            (string, string)? Asset(string k)
            {
                if (!P.Has(k)) return null;
                var v = UxEntities.GetValue(P[k]) as JObj;
                string a = v?.Get("asset") as string;
                return string.IsNullOrEmpty(a) ? ((string, string)?)null : (a, v.Get("type") as string);
            }
            var o = new HfRoleSet { entity = NameOf(hf[0]), heightmap = Asset("HeightMapAsset"), material = Asset("MaterialAsset"), softness = Asset("SoftnessMask"), zones = Asset("FrictionZones") };
            if (o.material == null) throw new UxException("the HFTerrainRSData of " + stem + "Base has no MaterialAsset");
            byte[] mat = ts.ReadLogical(ts.Get(o.material.Value.name, o.material.Value.type));
            o.textures = MaterialTextures(mat);
            foreach (var (role, i) in TERRAIN_SLOTS) o.by_role[role] = i < o.textures.Count ? o.textures[i] : null;
            if (o.heightmap == null) throw new UxException("the HFTerrainRSData of " + stem + "Base has no HeightMapAsset");
            o.by_role["Custom_DISP"] = o.heightmap.Value.name;
            o.by_role["Custom_HFTRN"] = o.material.Value.name;
            o.by_role["Custom_DEF"] = o.softness?.name;
            o.by_role["Custom_Zones"] = o.zones?.name;
            return o;
        }

        // ================================================================================================ scenario scripts
        /// intro_track_part(slot): the 5 characters after the first '_' (Beta_Sx_Track_Slot_1 -> 'Sx_Tr').
        public static string IntroTrackPart(string slot)
        {
            int i = slot.IndexOf('_');
            if (i < 0) return "X_X";
            int a = i + 1, e = Math.Min(slot.Length, i + 6);
            return a >= slot.Length ? "" : slot.Substring(a, e - a);
        }
        /// script_kinds(base, which): 'stock' = the policy's script_kinds, 'all' = INTRO_KINDS, 'none' = ().
        public static string[] ScriptKinds(Descriptor b, string which = "stock")
        {
            if (which == "none" || b.kit == "freeride") return new string[0];     // (FREESTYLE.md: the free-ride kit ships no intro / race scripts)
            if (which == "all") return INTRO_KINDS;
            return b.policy.script_kinds != null && b.policy.script_kinds.Length > 0 ? b.policy.script_kinds : INTRO_KINDS;
        }
        /// script_renames(base, ts, slot, kinds): {(old name, 'bxml'): new name} for the base's top-level intro scripts.
        /// kinds null = every kind.
        public static UxOrdered<(string name, string type), string> ScriptRenames(Descriptor b, UxTrackSet ts, string slot, IEnumerable<string> kinds = null)
        {
            string tr = b.policy.intro_track, newTr = IntroTrackPart(slot);
            var have = new HashSet<(string, string)>(); var namesAny = new Dictionary<string, int>(StringComparer.Ordinal); var lower = new HashSet<string>(StringComparer.Ordinal);
            foreach (var a in ts.assets)
            {
                have.Add((a.name, a.type));
                namesAny[a.name] = (namesAny.TryGetValue(a.name, out int c) ? c : 0) + 1;
                lower.Add(UxCommon.PyLower(a.name));
            }
            var o = new UxOrdered<(string, string), string>();
            var use = new List<string>();
            if (kinds == null) use.AddRange(INTRO_KINDS);
            else { var set = new HashSet<string>(kinds, StringComparer.Ordinal); foreach (string k in INTRO_KINDS) if (set.Contains(k)) use.Add(k); }
            foreach (string se in INTRO_SERIES)
                foreach (string k in use)
                {
                    string old = se + "_" + tr + "_" + k;
                    if (!have.Contains((old, "bxml"))) continue;
                    string nn = se + "_" + newTr + "_" + k;
                    if (namesAny[old] != 1) throw new UxException("script " + old + ": " + namesAny[old] + " assets carry that name (rename would hit them all)");
                    if (lower.Contains(UxCommon.PyLower(nn))) throw new UxException("script rename target " + nn + " already exists in the base");
                    o[(old, "bxml")] = nn;
                }
            return o;
        }

        // ================================================================================================ asset closure
        /// FOLLOW: asset type -> the types a name string inside it may reference.
        public static readonly Dictionary<string, string[]> FOLLOW = new Dictionary<string, string[]>(StringComparer.Ordinal)
        {
            { "material", new[] { "texture", "shader" } },
            { "model", new[] { "material" } },
            { "bxml", new[] { "bxml", "anim", "material", "texture", "sound", "soundbnk", "particle", "partattr" } },
            { "particle", new[] { "texture", "model", "surface", "material", "particle" } },
            { "partattr", new[] { "particle", "texture" } },
            { "adv", new[] { "texture", "material" } },
            { "sound", new[] { "soundbnk", "sound" } },
        };
        /// The FOLLOW keys in the Python dict's order (ux_build_track prefetches these types).
        public static readonly string[] FOLLOW_TYPES = { "material", "model", "bxml", "particle", "partattr", "adv", "sound" };

        /// anim_sets(): the core's animation alias table, <install>/AnimationSets.bxml.
        public sealed class AnimSetTable
        {
            /// lower set name -> member clips; vehicle: the members with HasVehicleAnim. Insertion order = the file's.
            public UxOrdered<string, List<string>> sets = new UxOrdered<string, List<string>>(StringComparer.Ordinal), vehicle = new UxOrdered<string, List<string>>(StringComparer.Ordinal);
            public string file, md5;
        }
        static string XmlText(object v)
        {
            switch (v)
            {
                case null: return null;
                case string s: return s;
                case double d: return UxBxml.G7(d);
                case long l: return I(l);
                case double[] a: return string.Join(",", Array.ConvertAll(a, UxBxml.G7));
                case UxBlobRef r: return "blob@" + I(r.offset);
            }
            return Convert.ToString(v, CultureInfo.InvariantCulture);
        }
        /// anim_sets(): throws without the file (the core reads the animation sets from the Reflex install).
        public static AnimSetTable AnimSets(IUxLibrary library, UxPaths paths = null)
        {
            string file = (paths ?? new UxPaths()).Game("AnimationSets.bxml");
            byte[] data = library?.ReadRootFile("AnimationSets.bxml");
            if (data == null) throw new UxException(file + " missing (the core reads the animation sets from the Reflex install)");
            var b = new UxBxml(data);
            var t = new AnimSetTable { file = file, md5 = UxBin.Md5(data) };
            void Walk(int ni)
            {
                if (b.Name(ni) == "Set")
                {
                    string nm = XmlText(b.Attr(ni, "Name"));
                    var mem = new List<string>(); var veh = new List<string>();
                    foreach (int c in b.Children(ni))
                    {
                        if (b.Name(c) != "Anim") continue;
                        string an = XmlText(b.Attr(c, "Name"));
                        if (string.IsNullOrEmpty(an)) continue;
                        mem.Add(an);
                        string hv = XmlText(b.Attr(c, "HasVehicleAnim")) ?? "";
                        hv = UxCommon.PyLower(hv);
                        if (hv == "1" || hv == "true") veh.Add(an);
                    }
                    if (!string.IsNullOrEmpty(nm) && mem.Count > 0)
                    {
                        t.sets[UxCommon.PyLower(nm)] = mem;
                        if (veh.Count > 0) t.vehicle[UxCommon.PyLower(nm)] = veh;
                    }
                }
                foreach (int c in b.Children(ni)) Walk(c);
            }
            Walk(0);
            return t;
        }

        /// session_anims(): {lower anim name: database} of the anim assets the session loads globally (Animations.dx9.database).
        public static Dictionary<string, string> SessionAnims(IUxLibrary library)
        {
            var o = new Dictionary<string, string>(StringComparer.Ordinal);
            foreach (string n in new[] { "Animations" })
            {
                if (library == null || !library.HasLevel(n)) continue;
                var b = new UxBxml(library.ReadLevelFile(n, "database"));
                for (int i = 0; i < b.nodes.Count; i++)
                    if (b.Name(i) == "Asset" && b.Attr(i, "type") as string == "anim")
                    {
                        string k = UxCommon.PyLower(b.Attr(i, "name") as string);
                        if (!o.ContainsKey(k)) o[k] = n;
                    }
            }
            return o;
        }

        /// asset_strings(data, typ): the name-like strings of an asset: the BXML string table, else printable ASCII runs of 3 and more.
        public static List<string> AssetStrings(byte[] data, string typ = null)
        {
            var o = new List<string>();
            if (data.Length >= 4 && data[0] == (byte)'B' && data[1] == (byte)'X' && data[2] == (byte)'M' && data[3] == (byte)'L')
            {
                try
                {
                    foreach (string s in new UxBxml(data).strings) if (!string.IsNullOrEmpty(s)) o.Add(s);
                    return o;
                }
                catch (Exception) { o.Clear(); }
            }
            int i = 0, n = data.Length;
            while (i < n)
            {
                if (data[i] < 0x20 || data[i] > 0x7e) { i++; continue; }
                int j = i; while (j < n && data[j] >= 0x20 && data[j] <= 0x7e) j++;
                if (j - i >= 3) o.Add(Encoding.ASCII.GetString(data, i, j - i));
                i = j;
            }
            return o;
        }

        /// entity_strings(e): the string values of an Entity (props other than Name) that may name assets.
        public static List<string> EntityStrings(UxElem e)
        {
            var o = new List<string>();
            foreach (var c in e.children[0].children)
            {
                if (c.Attr("name") as string == "Name") continue;
                foreach (var x in c.Iter()) if (x.vtype == 1 && x.value is string s && s.Length > 0) o.Add(s);
            }
            return o;
        }

        /// What closure() returns.
        public sealed class ClosureResult
        {
            /// kept: (name, type) -> the reason, in the order found.
            public UxOrdered<(string name, string type), string> kept = new UxOrdered<(string, string), string>();
            public List<(string name, string type, string why)> unresolved = new List<(string, string, string)>();
            public HashSet<(string name, string type)> glob = new HashSet<(string, string)>();
            /// info['sets'] {lower set name: members} and info['set_members_nowhere'] [(member, set)].
            public UxOrdered<string, List<string>> sets = new UxOrdered<string, List<string>>(StringComparer.Ordinal);
            public List<(string member, string set)> nowhere = new List<(string, string)>();
        }

        /// closure(entities, extra_roots, index, read, global_keys, sets, set_roots): the assets reached from entities (Asset elements
        /// + string props) and extra roots. index: {lower name: [(name, type)]} of every asset available (base package + build
        /// additions); read(key) -> logical bytes; globalKeys: (name, type) of the always-loaded databases (UxBuildAssets.GlobalAssets);
        /// sets: AnimSets().sets (null = no animation-set walk); sessionAnims: SessionAnims() (used with sets).
        public static ClosureResult Closure(IEnumerable<UxElem> entities, IEnumerable<(string name, string type)> extraRoots,
            IDictionary<string, List<(string name, string type)>> index, Func<(string name, string type), byte[]> read,
            ICollection<(string name, string type)> globalKeys = null, UxOrdered<string, List<string>> sets = null, IEnumerable<string> setRoots = null,
            IDictionary<string, string> sessionAnims = null)
        {
            var r = new ClosureResult();
            var todo = new Queue<(string name, string type)>();
            bool useSets = sets != null && sets.Count > 0;
            var sess = useSets ? sessionAnims : null;

            int Want(string name, string typ, string why, bool strict = true)
            {
                int hits = 0;
                if (index.TryGetValue(UxCommon.PyLower(name), out var cands))
                    foreach (var k in cands)
                    {
                        if (typ != null && k.type != typ) continue;
                        hits++;
                        if (!r.kept.Has(k)) { r.kept[k] = why; todo.Enqueue(k); }
                    }
                if (hits == 0 && typ != null)
                {
                    if (globalKeys != null && globalKeys.Contains((name, typ))) r.glob.Add((name, typ));
                    else if (strict) r.unresolved.Add((name, typ, why));
                }
                return hits;
            }
            void WantSet(string s, string why)
            {
                if (!useSets) return;
                string ls = UxCommon.PyLower(s);
                if (!sets.TryGet(ls, out var mem) || r.sets.Has(ls)) return;
                r.sets[ls] = new List<string>(mem);
                foreach (string m in mem)
                {
                    int hits = Want(m, "anim", "member of animation set " + s + " (" + why + ")", false);
                    if (hits == 0)
                    {
                        if (sess != null && sess.ContainsKey(UxCommon.PyLower(m))) r.glob.Add((m, "anim"));
                        else r.nowhere.Add((m, s));
                    }
                }
            }
            foreach (var e in entities)
            {
                string nm = NameOf(e) ?? "None";
                foreach (var (n, t) in UxBuildAssets.EntityRefs(e)) Want(n, t, "entity " + nm);
                foreach (string s in EntityStrings(e))
                {
                    Want(s, null, "entity " + nm + " (string)", false);
                    WantSet(s, "entity " + nm);
                }
            }
            if (extraRoots != null) foreach (var (n, t) in extraRoots) Want(n, t, "root");
            if (setRoots != null) foreach (string s in setRoots) WantSet(s, "engine constant");
            while (todo.Count > 0)
            {
                var k = todo.Dequeue();
                string name = k.name, typ = k.type;
                FOLLOW.TryGetValue(typ, out var follow);
                if (typ == "sound")
                    foreach (string bn in new[] { name, name + "_bank00" }) Want(bn, "soundbnk", "bank of " + name + ".sound", false);
                if (typ == "anim")
                    foreach (string suf in new[] { "_BIKE", "_ATV" }) Want(name + suf, "anim", "variant of " + name + ".anim", false);
                if (follow == null) continue;
                byte[] data;
                try { data = read(k); }
                catch (OperationCanceledException) { throw; }
                catch (Exception ex) { r.unresolved.Add((name, typ, "unreadable: " + ex.Message)); continue; }
                bool anim = useSets && Array.IndexOf(follow, "anim") >= 0;
                string why = name + "." + typ;
                foreach (string s in AssetStrings(data, typ))
                {
                    foreach (string t in follow) Want(s, t, why, false);
                    if (anim) WantSet(s, why);
                }
            }
            return r;
        }

        /// {lower name: [(name, type)]} of a list of asset keys, in order (the index closure() takes).
        public static Dictionary<string, List<(string name, string type)>> IndexOf(IEnumerable<(string name, string type)> keys)
        {
            var index = new Dictionary<string, List<(string, string)>>(StringComparer.Ordinal);
            foreach (var k in keys)
            {
                string l = UxCommon.PyLower(k.name);
                if (!index.TryGetValue(l, out var list)) index[l] = list = new List<(string, string)>();
                list.Add(k);
            }
            return index;
        }

        // ================================================================================================ constant textures
        /// tex_bc(img, fmt, straight_alpha): a PC v4 DXT1 / DXT5 texture with a full LANCZOS mip chain (every level resized from level
        /// 0; rtex.bc_encode, uxtex.build_pc): the encoder of ux_build_track.tex_bc. img: RGB (3) or RGBA (4) channels. straightAlpha
        /// (the dirt-look maps): an RGBA image's colour and alpha are resized apart (_resize_straight); without it Pillow resizes RGBA
        /// premultiplied, so a map with alpha 0 loses its colour in every mip (the older builds' bytes).
        public static byte[] TexBc(UxImage img, bool dxt5, bool straightAlpha = false)
        {
            int w = img.w, h = img.h;
            int n = 0; for (int m = Math.Max(w, h); m > 0; m >>= 1) n++;          // int(math.log2(max(w, h))) + 1
            byte[] fmt = dxt5 ? UxTex.PC_DXT5 : UxTex.PC_DXT1;
            var mips = new List<byte[]>(n); var table = new List<(int size, int pitch)>(n);
            for (int i = 0; i < n; i++)
            {
                int mw = Math.Max(1, w >> i), mh = Math.Max(1, h >> i);
                var cur = i == 0 ? img : straightAlpha && img.c == 4 ? ResizeStraight(img, mw, mh) : UxPil.Resize(img, mw, mh, UxPil.Filter.Lanczos);
                if (cur.w < 4 || cur.h < 4) cur = UxPil.Resize(cur, Math.Max(4, cur.w), Math.Max(4, cur.h), UxPil.Filter.Box);   // rtex.bc_encode
                if (!dxt5 && cur.c == 4) cur = DropAlpha(cur);                         // img.convert('RGB')
                byte[] m = UxPil.BcEncode(cur, dxt5);
                mips.Add(m); table.Add((m.Length, UxTex.PcPitch(fmt, Math.Max(1, w >> i))));
            }
            return UxTex.BuildPc(new List<(int mips, int w, int h, byte[] fmt)> { (n, w, h, fmt) }, table, mips);
        }
        /// _resize_straight(img, size): img.convert('RGB') and img.getchannel('A') resized with LANCZOS, then joined.
        static UxImage ResizeStraight(UxImage im, int ow, int oh)
        {
            int n = im.w * im.h; var a = new byte[n];
            for (int i = 0; i < n; i++) a[i] = im.data[4 * i + 3];
            var rgb = UxPil.Resize(DropAlpha(im), ow, oh, UxPil.Filter.Lanczos); var al = UxPil.Resize(new UxImage(a, im.h, im.w, 1), ow, oh, UxPil.Filter.Lanczos);
            var o = new byte[ow * oh * 4];
            for (int i = 0; i < ow * oh; i++) { o[4 * i] = rgb.data[3 * i]; o[4 * i + 1] = rgb.data[3 * i + 1]; o[4 * i + 2] = rgb.data[3 * i + 2]; o[4 * i + 3] = al.data[i]; }
            return new UxImage(o, oh, ow, 4);
        }
        static UxImage DropAlpha(UxImage im)
        {
            var o = new byte[im.w * im.h * 3];
            for (int i = 0, j = 0; i < im.data.Length; i += 4, j += 3) { o[j] = im.data[i]; o[j + 1] = im.data[i + 1]; o[j + 2] = im.data[i + 2]; }
            return new UxImage(o, im.h, im.w, 3);
        }
        /// constant_texture(colour, w, h, fmt): a constant-colour DXT1 (RGB) / DXT5 (RGBA) texture (the --base-layout neutral
        /// SoftnessMask / ChromLum maps). colour: 3 or 4 values 0..255.
        public static byte[] ConstantTexture(IList<int> colour, int w, int h, bool dxt5)
        {
            int c = dxt5 ? 4 : 3;
            var px = new byte[w * h * c];
            for (int i = 0; i < w * h; i++) for (int k = 0; k < c; k++) px[i * c + k] = (byte)colour[k];
            return TexBc(new UxImage(px, h, w, c), dxt5);
        }
    }
}

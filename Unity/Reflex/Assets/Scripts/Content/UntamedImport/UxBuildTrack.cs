// MX vs ATV Untamed converter, the track builder: the port of the Python reference converter's ux_build_track.py (the Builder of the current
// stock-base sx builds, DESIGN 23) joined with ux_package.py (UxPackage.cs). One call converts one Untamed stadium into an OPEN REFLEX
// custom track folder: Beta_Sx_Track_Slot_1.dx9.{database,level,package,scene}, mod.ini, preview.png and sha256.txt (a freestyle arena:
// Beta_Fmx_Track_Slot_1, a free-ride track on the base's free-ride kit, without S8 and the race kit; its ramps: UxFreestyle).
//   1. stages S1-S6 (UxLevel, UxTerrainStage, UxMeshes, UxCollision, UxMaterials, UxTextures) into the work store (the staging folder),
//      S7 UxEntities and S8 UxRace in memory, each with the build's options
//   2. the stock level SX_FortDodge from the player's Reflex library, reduced by UxBase.Prepare to the race / free-ride kit
//   3. the combined-round parts: tuff blocks (UxTuff), EnduroCross water and its spray (UxWater), MassiveAd panels (UxAds), the
//      jumbotron stills (UxScreens), the split gate (UxSplitGate), the freestyle ramp carriers (UxFreestyle)
//   4. the scene (GridResolution, DistanceFog values, the race kit moves, anchor moves, the 12 free-ride starts, the dropped-material
//      filter, the template y offset), the assets (clashes with the base, UX_ renames, terrain textures, the HF material, FrictionZones,
//      CameraFlash, the tdf), the prune of the base package (UxBase.Closure), the slot rename and the four files
// Only the code path of the current builds is ported: kind sx, --base stock, the options of UxTrackOptions (the per-track values of the
// reference's commands files are UxTrackOptions.ForLevel), as of ux_build_track.py REFERENCE_TOOL_MD5 (the releases norton-v7 ...
// lincoln-v7: the water-glare round for the two arenas, ecWaterReflection, after the dirt-look rounds, DESIGN 23.10 / 23.11: the stock base's own dirt layers with generated DetailMask / ChromLum /
// TopDown maps, UxDirtLook, and the EnduroCross shore colour taken from that ground; dirtLook = "untamed" with ecWaterShore =
// "untamed" writes the releases before them, norton-v6 ... lincoln-v5). The Python records (build_info.json: assembly, rollout ...)
// are kept and written into the work store as build_info.json; nothing of them enters the four files.
// Package resources: the reference compresses with Microsoft's XMemCompress. Here a resource is written with UxLzxEncoder (LZX
// chunks the core and the client decode, the default), as stored chunks, or raw (no Compress element): UxTrackOptions.packing.
// The four files, mod.ini, preview.png and sha256.txt hold no time stamp, path or machine name; they depend on the disc, the library
// content and the options only (not on the thread count, nor on whether the library is an original install or the client's game files).
// System only, C# 9, .NET Standard 2.1.
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;
using System.Threading.Tasks;
using Reflex.Port.Content.AliveImport;

namespace Reflex.Port.Content.UntamedImport
{
    /// The options of one track build: what the reference's command line gives ux_build_track.py (and ux_package.py).
    public sealed class UxTrackOptions
    {
        // ---- per track (the data table below)
        /// --type9 refuse | accept (S5: a type-9 frustum primitive written as the enclosing cylinder).
        public string type9 = "accept";
        /// --bale-trigger-move DX,DZ (S7), or null.
        public string baleTriggerMove;
        /// --bale-edge-clear FT (S8) and --bale-reseat FT (S7); 0 = off.
        public double baleEdgeClear, baleReseat;
        /// --split-gate template | untamed, --startgate-visual keep | hide.
        public string splitGate = "template", startgateVisual = "keep";
        /// --anchor-move NAME=dx,dy,dz, in command-line order.
        public string[] anchorMoves = new string[0];

        // ---- the same on every current build (the reference's defaults)
        public int slot = UxCommon.DefaultSlot;
        public string alpha = "eco_frond", blend = "keep", baleBase = "hay", staticPop = "all", gatePatch = "auto";
        public bool prelitOff, nrmOff, crowdOff, staticBales, dropGateTop, noCameraflash;
        public string crowdTier = "stock", groupNumber = "zero", hbFreeride = "drop", mipfilter = "linear", tdfPad = "zero", deckNorespawn = "engine";
        public string composite = "bake", literalAlbedo = "program", alphaSource = "program", diffusePick = "program", rigidModels = "static", popClip = "on";
        public string buried = "drop", frClearance = "strict", occMap = "contact", lappacing = "none", dropgateMaterials = "template";
        public string zones = "fill", baseScripts = "stock", baseCameraflash = "untamed", baseLayout = "neutral";
        /// --dirt-tint const | template and --hf-params today | base; null (the default) follows dirtLook as the reference does:
        /// 'stock' keeps the base HF material's own rut tint and detail tiling (template / base), 'untamed' writes the constants of
        /// the older builds (const / today). DirtTint / HfParams give the value a build uses.
        public string dirtTint, hfParams;
        public string DirtTint => dirtTint ?? (dirtLook == "stock" ? "template" : "const");
        public string HfParams => hfParams ?? (dirtLook == "stock" ? "base" : "today");
        public int[] zonesFill = { 2, 0, 0, 0 };
        public string bales = "tuff", tuffCollision = "stock", tuffWraps = "rule";
        public string ecWater = "on", ecWaterKitOrder = "first", ecWaterLook = "arena", ecWaterSplash = "on";
        /// --ec-water-shore (arena look only): 'ground' = the shore colour is a wet version of the built ground next to each pit
        /// (UxWater.GroundShore; the releases fairview-v6 / lincoln-v6), 'untamed' = the Untamed Color x 0.62 (fairview-v5 / lincoln-v5).
        public string ecWaterShore = "ground";
        /// --ec-water-reflection: 'dim' (the default; fairview-v7 / lincoln-v7) = the cube the water reflects is a copy of the level cube
        /// with the stadium lights capped (UxWater.ReflectionCube: no white bloom blobs on the water), 'nolights' = the lights
        /// in-painted, 'level' = the level cube itself (fairview-v6 / lincoln-v6).
        public string ecWaterReflection = "dim";
        /// --ec-water-depth: the cap in ft, or NaN for 'untamed'.
        public double ecWaterDepth = 1.25;
        public string screens = "stills", massiveAds = "keep";
        /// --dirt-look: 'stock' (the default; the releases norton-v7 ... lincoln-v6) = the base's own HF material and four detail
        /// textures with DetailMask / ChromLum / TopDown generated around our track (UxDirtLook); 'untamed' = the ground of every
        /// release up to norton-v6 / fairview-v5 / lincoln-v5 (Untamed's own tiling textures and masks, a neutral ChromLum).
        public string dirtLook = "stock";
        public double screenLight = 0.9;
        public int texMax;

        // ---- how the port runs and writes (no Python counterpart)
        /// Package resources: 'lzx' (UxLzxEncoder: LZX chunks, the default), 'stored' (stored chunks, no padding) or 'raw' (no Compress
        /// element: the heap region is the resource).
        public string packing = "lzx";
        /// Worker threads of the stages and of the resource encoder (0 = one per processor). The outputs do not depend on it.
        public int threads;
        /// Stage outputs already in the work store are used when they were written for the same disc level, options and converter
        /// (build_stages.json); false runs every stage again.
        public bool reuseStages;
        /// The date in mod.ini's description (ux_package: time.strftime('%Y-%m-%d')); null leaves it out, so the file never depends on
        /// the clock.
        public string date;
        /// mod.ini author.
        public string author = UxPackage.AUTHOR;
        /// The path text of the records (build_info.json); never used to read a file.
        public UxPaths paths;
        /// Also write the release-form checks' re-read of the four files (UxPackage.Rules); a failure throws.
        public bool check = true;
        /// GC.Collect() at every step boundary (about 20 collections per track): bounds the peak memory of an import.
        public bool collect = true;

        /// The reference's build of each track (work/combined/commands.json 2026-11-11, the 12 stadiums): level -> the options that
        /// differ from the defaults above.
        public static readonly KeyValuePair<string, string[]>[] Table =
        {
            Row("Norton_STADIUM", "--anchor-move", "Anchor_30SecGirl_CameraHound10=1.182,0.148,-1.29", "--anchor-move", "Anchor_StartLine=0.0,7.5,0.0", "--anchor-move", "Anchor_WinFinish=0.0,20.5,0.0"),
            Row("Capital_City_STADIUM", "--bale-edge-clear", "4", "--anchor-move", "Anchor_LoseWalk_PlayerCC11=-0.428,-0.003,-1.175", "--anchor-move", "Anchor_StartLine=0.0,7.75,0.0",
                "--anchor-move", "Anchor_WinFinish=0.0,19.25,0.0"),
            Row("Columbus_STADIUM", "--bale-edge-clear", "4", "--anchor-move", "Anchor_LoseWalk_PlayerCC4=-1.028,-0.024,-4.637", "--anchor-move", "Anchor_StartLine=0.0,7.5,0.0",
                "--anchor-move", "Anchor_WinFinish=0.0,20.5,0.0"),
            Row("Denton_STADIUM", "--bale-trigger-move", "0,413.31", "--bale-edge-clear", "4", "--bale-reseat", "6", "--anchor-move", "Anchor_StartLine=0.0,3.25,0.0",
                "--anchor-move", "Anchor_LoseWalk=0.0,6.25,0.0", "--anchor-move", "Anchor_WinFinish=0.0,18.5,0.0"),
            Row("HarrisPark_STADIUM", "--anchor-move", "Anchor_Intro=0.0,31.25,0.0", "--anchor-move", "Anchor_StartLine=0.0,7.5,0.0", "--anchor-move", "Anchor_WinFinish=0.0,18.5,0.0"),
            Row("Foothills_STADIUM", "--split-gate", "untamed", "--startgate-visual", "hide", "--anchor-move", "Anchor_StartLine=0.0,7.25,0.0", "--anchor-move", "Anchor_WinFinish=0.0,21.0,0.0"),
            Row("WestStone_STADIUM", "--anchor-move", "Anchor_LoseWalk_PlayerCC8=1.0,-0.0,0.0", "--anchor-move", "Anchor_LoseWalk_PlayerCC9=3.0,-0.001,0.0",
                "--anchor-move", "Anchor_LoseWalk_PlayerFinished6=-0.482,0.0,0.575", "--anchor-move", "Anchor_LoseWalk_PlayerFinished8=-0.887,0.0,0.462",
                "--anchor-move", "Anchor_Intro=0.0,6.5,0.0", "--anchor-move", "Anchor_StartLine=0.0,7.25,0.0", "--anchor-move", "Anchor_WinFinish=0.0,20.5,0.0"),
            Row("Jefferson_STADIUM", "--anchor-move", "Anchor_LoseWalk_PlayerCC10=1.133,-0.0,0.528", "--anchor-move", "Anchor_LoseWalk_PlayerFinished10=5.312,-0.0,2.2",
                "--anchor-move", "Anchor_StartLine=0.0,7.5,0.0", "--anchor-move", "Anchor_LoseWalk=0.0,10.25,0.0", "--anchor-move", "Anchor_WinFinish=0.0,21.75,0.0"),
            Row("Wilford_STADIUM", "--anchor-move", "Anchor_StartLine_CardGirl=0.196,-0.001,2.241", "--anchor-move", "Anchor_StartLine=0.0,7.5,0.0", "--anchor-move", "Anchor_LoseWalk=0.0,7.25,0.0",
                "--anchor-move", "Anchor_WinFinish=0.0,21.0,0.0", "--bale-edge-clear", "4"),
            Row("BridgePort_STADIUM", "--anchor-move", "Anchor_LoseWalk_PlayerFinished8=-2.107,0.408,1.768", "--anchor-move", "Anchor_LoseWalk_PlayerFinished9=1.0,-0.14,0.0",
                "--anchor-move", "Anchor_Intro=0.0,49.5,0.0", "--anchor-move", "Anchor_StartLine=0.0,7.25,0.0", "--anchor-move", "Anchor_WinFinish=0.0,20.0,0.0"),
            Row("Fairview_STADIUM", "--anchor-move", "Anchor_30SecGirl_CameraHound07=0.75,0.003,0.0", "--anchor-move", "Anchor_LoseWalk_PlayerCC6=4.75,-0.042,0.0",
                "--anchor-move", "Anchor_LoseWalk_PlayerCC8=-1.644,-1.476,0.599", "--anchor-move", "Anchor_LoseWalk_PlayerFinished4=-1.998,-1.109,0.087",
                "--anchor-move", "Anchor_LoseWalk_PlayerFinished7=-1.192,0.013,0.376", "--anchor-move", "Anchor_LoseWalk_PlayerFinished8=0.259,-0.127,0.966",
                "--anchor-move", "Anchor_LoseWalk_PlayerFinished9=-1.848,-0.267,0.765", "--anchor-move", "Anchor_StartLine_CardGirl=2.0,0.144,0.0",
                "--anchor-move", "Anchor_StartLine=0.0,7.75,0.0", "--anchor-move", "Anchor_LoseWalk=0.0,7.0,0.0", "--anchor-move", "Anchor_WinFinish=0.0,17.0,0.0"),
            Row("Lincoln_STADIUM", "--anchor-move", "Anchor_30SecGirl_CameraHound05=3.289,-0.559,-1.197", "--anchor-move", "Anchor_30SecGirl_CameraHound09=-0.732,-0.006,0.162",
                "--anchor-move", "Anchor_StartLine=0.0,3.75,0.0", "--anchor-move", "Anchor_LoseWalk=0.0,1.75,0.0", "--anchor-move", "Anchor_WinFinish=0.0,20.5,0.0"),
            // the Minimoto stadiums (work/minimoto/commands.json 2026-11-12)
            Row("Dyson_STADIUM", "--anchor-move", "Anchor_Intro=0.0,6.5,0.0", "--anchor-move", "Anchor_StartLine=0.0,7.5,0.0", "--anchor-move", "Anchor_LoseWalk=0.0,6.75,0.0",
                "--anchor-move", "Anchor_WinFinish=0.0,19.25,0.0"),
            Row("Edgewater_STADIUM", "--anchor-move", "Anchor_LoseWalk_PlayerCC10=2.182,0.0,1.674", "--anchor-move", "Anchor_StartLine=0.0,7.25,0.0",
                "--anchor-move", "Anchor_LoseWalk=0.0,11.25,0.0", "--anchor-move", "Anchor_WinFinish=0.0,19.75,0.0"),
            // the freestyle arenas (work/freestyle/commands-v3.json): the defaults only (no anchors in the free-ride kit)
            Row("RBWCenter_STADIUM"), Row("Southbend_STADIUM"), Row("THQSpectrum_STADIUM"), Row("WestLake_STADIUM"),
        };
        static KeyValuePair<string, string[]> Row(string level, params string[] args) => new KeyValuePair<string, string[]>(level, args);

        /// The options of a level's current build: '--type9 accept' and the level's row of Table.
        public static UxTrackOptions ForLevel(UxLevelRow lv)
        {
            foreach (var kv in Table) if (kv.Key == lv.disc) return Parse(kv.Value);
            throw new UxException("no build options for " + lv.disc + " (every supported track has a row in UxTrackOptions.Table)");
        }
        public static UxTrackOptions ForLevel(string level) => ForLevel(UxCommon.Level(level));

        /// The options of a reference command line (the switches a current build uses; anything else throws).
        public static UxTrackOptions Parse(IList<string> args)
        {
            var o = new UxTrackOptions(); var moves = new List<string>();
            for (int i = 0; i < args.Count; i++)
            {
                string a = args[i];
                string Val() { if (i + 1 >= args.Count) throw new UxException(a + " needs a value"); return args[++i]; }
                switch (a)
                {
                    case "--type9": o.type9 = Val(); break;
                    case "--bale-trigger-move": o.baleTriggerMove = Val(); break;
                    case "--bale-edge-clear": o.baleEdgeClear = UxJson.ParseDouble(Val()); break;
                    case "--bale-reseat": o.baleReseat = UxJson.ParseDouble(Val()); break;
                    case "--split-gate": o.splitGate = Val(); break;
                    case "--startgate-visual": o.startgateVisual = Val(); break;
                    case "--anchor-move": moves.Add(Val()); break;
                    case "--ec-water-look": o.ecWaterLook = Val(); break;
                    case "--ec-water-shore": o.ecWaterShore = Val(); break;
                    case "--ec-water-reflection": o.ecWaterReflection = Val(); break;
                    case "--dirt-look": o.dirtLook = Val(); break;
                    case "--dirt-tint": o.dirtTint = Val(); break;
                    case "--hf-params": o.hfParams = Val(); break;
                    case "--ec-water-splash": o.ecWaterSplash = Val(); break;
                    case "--level": case "--kind": case "--slot": case "--out": case "--jobs": Val(); break;      // the command's own
                    case "--reuse": break;
                    default: throw new UxException("option " + a + " is not one the ported builder knows");
                }
            }
            o.anchorMoves = moves.ToArray();
            return o;
        }

        /// The S4 options of the build (Stages.mat_opts).
        public UxMaterialsOptions MaterialsOptions() => new UxMaterialsOptions
        {
            alpha = alpha, prelit_off = prelitOff, nrm_off = nrmOff, crowd_off = crowdOff, blend = blend, bale_base = baleBase, mipfilter = mipfilter,
            composite = composite, literal_albedo = literalAlbedo, alpha_source = alphaSource, diffuse_pick = diffusePick, jobs = threads,
        };

        /// What this port does not carry of ux_build_track's switches: a value outside the current builds' code path throws.
        public void Validate()
        {
            void Only(string name, string value, params string[] ok) { if (Array.IndexOf(ok, value) < 0) throw new UxException("option " + name + " = '" + value + "' is not ported (" + string.Join(" | ", ok) + ")"); }
            Only("type9", type9, "refuse", "accept"); Only("split-gate", splitGate, "template", "untamed"); Only("startgate-visual", startgateVisual, "keep", "hide");
            Only("dirt-look", dirtLook, "stock", "untamed"); Only("ec-water-shore", ecWaterShore, "ground", "untamed"); Only("ec-water-reflection", ecWaterReflection, UxWater.REFLECTION_CHOICES);
            Only("zones", zones, "fill", "template"); Only("dirt-tint", DirtTint, "const", "template"); Only("base-scripts", baseScripts, "stock", "all", "none");
            Only("base-cameraflash", baseCameraflash, "untamed", "stock"); Only("base-layout", baseLayout, "neutral", "keep"); Only("hf-params", HfParams, "today", "base");
            Only("bales", bales, "tuff", "untamed"); Only("ec-water", ecWater, "on", "off"); Only("ec-water-look", ecWaterLook, "arena", "untamed");
            Only("ec-water-splash", ecWaterSplash, "on", "off"); Only("screens", screens, "stills", "untamed"); Only("massive-ads", massiveAds, "keep", "model", "drop");
            Only("lappacing", lappacing, "none", "untamed"); Only("packing", packing, "lzx", "stored", "raw");
            if (slot < 1 || slot > 99) throw new UxException("slot " + slot + " out of range");
            if (zonesFill == null || zonesFill.Length != 4 || zonesFill[0] == 7) throw new UxException("zones-fill takes four integers >= 0 (layer 0 value 7 is reset terrain)");
            if (!(screenLight > 0 && screenLight <= 1)) throw new UxException("screen-light takes a gain in (0, 1]");
            if (baleEdgeClear < 0 || baleReseat < 0) throw new UxException("bale-edge-clear / bale-reseat take ft >= 0");
            if ((baleEdgeClear != 0 || baleReseat != 0) && staticBales) throw new UxException("bale-edge-clear / bale-reseat need the PopDown bales as pop bales");
            if (prelitOff || nrmOff || crowdOff || staticBales || texMax != 0) throw new UxException("the safe-variant switches (prelit-off, nrm-off, crowd-off, static-bales, tex-max) are not ported");
        }

        /// vars(o) of the reference, for build_info.json (the switches this port has).
        public JObj Record(UxLevelRow lv)
        {
            var o = new JObj();
            void S(string k, object v) => o.items.Add(new KeyValuePair<string, object>(k, v));
            S("alpha", alpha); S("alpha_source", alphaSource); S("anchor_move", anchorMoves.Length == 0 ? null : (object)new List<object>(anchorMoves)); S("bale_base", baleBase);
            S("bale_edge_clear", baleEdgeClear); S("bale_reseat", baleReseat); S("bale_trigger_move", baleTriggerMove); S("bales", bales); S("base", "stock");
            S("base_cameraflash", baseCameraflash); S("base_layout", baseLayout); S("base_level", UxBase.DEFAULT_STOCK_LEVEL); S("base_scripts", baseScripts); S("blend", blend);
            S("buried", buried); S("composite", composite); S("crowd_off", crowdOff); S("crowd_tier", crowdTier); S("deck_norespawn", deckNorespawn);
            S("diffuse_pick", diffusePick); S("dirt_look", dirtLook); S("dirt_tint", DirtTint); S("drop_gate_top", dropGateTop); S("dropgate_materials", dropgateMaterials); S("ec_water", ecWater);
            S("ec_water_depth", double.IsNaN(ecWaterDepth) ? (object)"untamed" : ecWaterDepth); S("ec_water_kit_order", ecWaterKitOrder); S("ec_water_look", ecWaterLook);
            S("ec_water_reflection", ecWaterReflection); S("ec_water_shore", ecWaterShore);
            S("ec_water_splash", ecWaterSplash); S("fr_clearance", frClearance); S("gate_patch", gatePatch); S("group_number", groupNumber); S("hb_freeride", hbFreeride);
            S("hf_params", HfParams); S("kind", "sx"); S("lappacing", lappacing); S("legacy_defaults", false); S("level", lv.disc); S("literal_albedo", literalAlbedo);
            S("massive_ads", massiveAds); S("mipfilter", mipfilter); S("no_cameraflash", noCameraflash); S("nrm_off", nrmOff); S("occ_map", occMap); S("pop_clip", popClip);
            S("prelit_off", prelitOff); S("prims_to_22", false); S("rigid_models", rigidModels); S("safe", false); S("screen_light", screenLight); S("screens", screens);
            S("slot", (long)slot); S("split_gate", splitGate); S("startgate_visual", startgateVisual); S("static_bales", staticBales); S("static_pop", staticPop);
            S("tdf_pad", tdfPad); S("tex_max", texMax == 0 ? null : (object)(long)texMax); S("tuff_collision", tuffCollision); S("tuff_wraps", tuffWraps); S("type9", type9);
            S("zones", zones); S("zones_fill", new List<object> { (long)zonesFill[0], (long)zonesFill[1], (long)zonesFill[2], (long)zonesFill[3] });
            S("packing", packing);
            return o;
        }
    }

    /// What a build returns.
    public sealed class UxBuildResult
    {
        /// The release-form folder and the slot name (Beta_Sx_Track_Slot_1).
        public string folder, slot;
        /// File name -> size / sha256 of every file of the folder but sha256.txt.
        public readonly List<(string name, long bytes, string sha256)> files = new List<(string, long, string)>();
        /// build_info.json (also in the work store): the reference's record of the build.
        public JObj info;
        /// Seconds: the whole build; per step ('level', 'terrain', ... 'entities', 'race', 'assemble', 'assets', 'write', 'package').
        public double seconds; public readonly List<KeyValuePair<string, double>> steps = new List<KeyValuePair<string, double>>();
        /// The package: logical bytes of its resources and the bytes written.
        public long logicalBytes, packageBytes; public int assets;
        public List<string> warnings = new List<string>();
    }

    /// What every build of one session shares: the stock base's assets, the always-loaded databases, the animation sets and the race
    /// template of a Reflex library (read once; the builds only read them).
    public sealed class UxBuildShared
    {
        public readonly UxStock stock; public readonly IUxLibrary library;
        public readonly UxBase.Descriptor @base;
        public readonly UxTrackSet ts; public readonly UxTemplateAssets TA;
        Dictionary<(string name, string type), string> _globals; UxBase.AnimSetTable _animSets; Dictionary<string, string> _sessionAnims; UxRaceTemplate _raceTemplate;
        UxEntities.Prototypes _protos;
        readonly object _gate = new object();
        public UxBuildShared(UxStock stock)
        {
            this.stock = stock ?? throw new UxException("the builder needs the player's Reflex library");
            library = stock.library;
            @base = UxBase.Resolve();
            ts = stock.BaseSet(@base.stem);
            TA = new UxTemplateAssets(ts);
        }
        public Dictionary<(string name, string type), string> Globals { get { lock (_gate) return _globals ?? (_globals = UxBuildAssets.GlobalAssets(library)); } }
        public UxBase.AnimSetTable AnimSets(UxPaths paths) { lock (_gate) return _animSets ?? (_animSets = UxBase.AnimSets(library, paths)); }
        public Dictionary<string, string> SessionAnims { get { lock (_gate) return _sessionAnims ?? (_sessionAnims = UxBase.SessionAnims(library)); } }
        public UxRaceTemplate RaceTemplate { get { lock (_gate) return _raceTemplate ?? (_raceTemplate = new UxRaceTemplate(stock)); } }
        public UxEntities.Prototypes Prototypes { get { lock (_gate) return _protos ?? (_protos = new UxEntities.Prototypes(stock)); } }
    }

    /// ux_build_track.py: the Builder.
    public static class UxBuildTrack
    {
        public const string TOOL = "ux_build_track.py", SCHEMA = "ux_build_track/1";
        /// The reference this port mirrors: ux_build_track.py of 2026-11-11 13:41 (md5), with ux_water.py a51b3d57 and ux_dirtlook.py
        /// 566533af: the builder of capitalcity-v5, columbus-v6, denton-v5, harrispark-v6, foothills-v6, weststone-v5, jefferson-v5,
        /// wilford-v5, bridgeport-v5, fairview-v6 and lincoln-v6; norton-v7 was built by its predecessor 67ffbe75 (the dirt look
        /// without the shore colour, which Norton has no water for). With dirtLook = ecWaterShore = "untamed" it writes what
        /// 60ba557d wrote: norton-v6, capitalcity-v4, columbus-v5, denton-v4, harrispark-v5, foothills-v5, weststone-v4,
        /// jefferson-v4, wilford-v4, bridgeport-v4, fairview-v5, lincoln-v5.
        /// Since the water-glare round (2026-11-12) the reference is 15fa7970 with ux_water.py 1fc7f0da: fairview-v7 and lincoln-v7 (the
        /// water reflects a copy of the level cube with the stadium lights dimmed); ecWaterReflection = "level" writes fairview-v6 /
        /// lincoln-v6. The ten stadiums without water are unchanged.
        /// Since the freestyle round (2026-11-12) the reference is the merged tools' 1ef081f1 (tools-fmx2): the four freestyle arenas
        /// (westlake-v3 ... rbwcenter-v3, free-ride builds in a Beta_Fmx slot) and, unchanged, every stadium and the Minimoto arenas.
        public const string REFERENCE_TOOL_MD5 = "1ef081f19405e97c17b69da30f201ec0";
        /// The version of this port's outputs: part of the stage stamp (a change here makes every staged stage output stale).
        /// 2 = the dirt-look rounds (the default ground and shore colour changed); 3 = the water-glare round (the arenas' reflection cube);
        /// 4 = the freestyle round (the free-ride arenas; duplicate collision triangles dropped on every level).
        public const string PORT = "UxBuildTrack/4";
        public static readonly string[] SUFFIXES = { "", "Base", "FreeRide", "SectionRace", "-sx" };
        static readonly string[] STRIP = { "CollisionRSData", "ModelRSData" };
        static readonly string[] KEEP_NAMES = { "TO_StartingGate" };
        static readonly string[] TEMPLATE_Y_ENTITIES = { "VolumeFogRSData", "ShadowMapRSData", "ReflectionMapGenRSData", "LevelAmbientAudioZoneRSData", "SkyRSData" };
        /// TERRAIN_TEXTURES: role -> (WP6 file or null = the top-down, size, DXT5).
        static readonly (string role, string file, int size, bool dxt5)[] TERRAIN_TEXTURES =
        {
            ("Custom_TopDown", null, 2048, false), ("Custom_DetailMasks", "detailmask.png", 2048, true), ("Custom_Base", "base.png", 1024, true),
            ("Custom_Dirt_R", "dirt_r.png", 1024, true), ("Custom_Dirt_G", "dirt_g.png", 1024, true), ("Custom_Dirt_B", "dirt_b.png", 1024, true),
        };
        /// --dirt-tint const: the HuCkiT free-ride template's usr_dirt_tint_color as a constant.
        public static readonly float[] DIRT_TINT_CONST = { 0.329f, 0.290f, 0.239f, 1.0f };
        /// --hf-params today: the HF material's detail tiling and bump scale every earlier build shipped.
        public static readonly (string name, float[] value)[] HF_PARAMS_TODAY = { ("usr_detail_tile", new[] { 100.0f, 64.0f, 50.0f, 75.0f }), ("usr_bump_scalar", new[] { 0.2f, 0.2f, 0.2f, 0.2f }) };
        public static readonly int[] ZONES_FILL_DEFAULT = { 2, 0, 0, 0 };
        public static readonly int[] SOFTNESS_CONST_RGB = { 16, 16, 16 }, CHROMLUM_CONST_RGBA = { 0, 0, 0, 127 };
        /// G_DL: the dirt-look stock map size (UxDirtLook.G).
        public const int G_DL = UxDirtLook.G;
        public static readonly (string name, string type) COLORREF_NEUTRAL = ("Editor_Default_White", "texture");
        static readonly (string name, string type)[] ORPHAN_CANDIDATES = { ("Custom_ShadowMap", "texture") };

        // ================================================================================================ entry points
        /// Converts one stadium. ctx: the disc level (ctx.disc), the stock library (ctx.stock) and the staging folder (ctx.work, the
        /// stage outputs and build_info.json); disc: the whole disc (the screen stills are made from two other levels' textures);
        /// outFolder: the release-form folder (created; an existing slot set of another name is refused). progress(fraction, text).
        public static UxBuildResult Build(UxContext ctx, UxDisc disc, UxTrackOptions options, string outFolder, Action<float, string> progress = null, CancellationToken cancel = default,
            UxBuildShared shared = null)
        {
            if (ctx == null || ctx.disc == null || ctx.stock == null || ctx.work == null) throw new UxException("the builder needs a disc level, the stock library and a work store");
            options = options ?? UxTrackOptions.ForLevel(ctx.disc.lv);
            options.Validate();
            if (cancel == default) cancel = ctx.cancel; else if (ctx.cancel == default) ctx.cancel = cancel;      // the stages take the context's token
            var b = new Builder(ctx, disc, options, shared ?? new UxBuildShared(ctx.stock), progress, cancel);
            return b.Run(outFolder);
        }

        /// The same with the whole disc taken from the level's own volume (ctx.disc was opened from it).
        public static UxBuildResult Build(UxContext ctx, UxTrackOptions options, string outFolder, Action<float, string> progress = null, CancellationToken cancel = default)
        {
            if (ctx == null || ctx.disc == null) throw new UxException("the builder needs a disc level");
            return Build(ctx, new UxDisc(ctx.disc.volume, ctx.disc.directory, ctx.disc.root), options, outFolder, progress, cancel, null);
        }

        // ================================================================================================ small helpers
        static string I(long v) => v.ToString(CultureInfo.InvariantCulture);
        static string NameOf(UxElem e)
        {
            var p = UxPcScene.Prop(e, "Name");
            return p != null && p.children.Count > 0 ? p.children[0].value as string : null;
        }
        static long EntityId(UxElem e)
        {
            foreach (var a in e.attrs) if (a.name == "entityId" && a.raw != null && a.raw.Length >= 4) return UxBin.U32LE(a.raw, 0);
            return -1;
        }
        static string PyList(IEnumerable<string> items)
        {
            var sb = new StringBuilder("["); bool first = true;
            foreach (string s in items) { if (!first) sb.Append(", "); first = false; sb.Append('\'').Append(s).Append('\''); }
            return sb.Append(']').ToString();
        }
        static List<object> Strs(IEnumerable<string> s) { var l = new List<object>(); foreach (string x in s) l.Add(x); return l; }
        static List<object> Nums(IEnumerable<double> s) { var l = new List<object>(); foreach (double x in s) l.Add(x); return l; }
        static JObj Obj(object o) => o as JObj;
        static string K((string name, string type) k) => k.name + "." + k.type;

        /// ground_ft(h, grid, x, z): bilinear height of a heightfield (row 0 = +Z, col 0 = -X, pixel centres) at world (x, z).
        static double GroundFt(double[] h, int W, double grid, double x, double z)
        {
            double c = x / grid + W / 2.0 - 0.5, r = W / 2.0 - z / grid - 0.5;
            c = Math.Min(Math.Max(c, 0), W - 1); r = Math.Min(Math.Max(r, 0), W - 1);
            int c0 = (int)c, r0 = (int)r, c1 = Math.Min(c0 + 1, W - 1), r1 = Math.Min(r0 + 1, W - 1);
            double fc = c - c0, fr = r - r0;
            return (h[r0 * W + c0] * (1 - fc) + h[r0 * W + c1] * fc) * (1 - fr) + (h[r1 * W + c0] * (1 - fc) + h[r1 * W + c1] * fc) * fr;
        }

        /// tex_r32f(arr): a PC v4 R32F texture with a full point-sampled mip chain (the stock height map convention).
        public static byte[] TexR32F(float[] a, int h, int w)
        {
            int n = 0; for (int m = Math.Max(w, h); m > 0; m >>= 1) n++;          // int(math.log2(max(w, h))) + 1
            var mips = new List<byte[]>(n); var table = new List<(int size, int pitch)>(n);
            float[] cur = a; int ch = h, cw = w;
            for (int i = 0; i < n; i++)
            {
                var bytes = new byte[4 * cur.Length];
                for (int k = 0; k < cur.Length; k++) UxBin.PutF32(bytes, 4 * k, cur[k]);
                mips.Add(bytes); table.Add((bytes.Length, UxTex.PcPitch(UxTex.PC_R32F, Math.Max(1, w >> i))));
                if (i + 1 < n)
                {   // cur[::2, ::2]
                    int nh = (ch + 1) / 2, nw = (cw + 1) / 2; var nx = new float[nh * nw];
                    for (int y = 0; y < nh; y++) for (int x = 0; x < nw; x++) nx[y * nw + x] = cur[2 * y * cw + 2 * x];
                    cur = nx; ch = nh; cw = nw;
                }
            }
            return UxTex.BuildPc(new List<(int mips, int w, int h, byte[] fmt)> { (n, w, h, UxTex.PC_R32F) }, table, mips);
        }

        // ---- FrictionZones
        sealed class ZoneLayer { public int W, H, nb, nw, off; public long[] vals; public byte[] words; }
        /// zones_layer(b, k): FrictionZones layer k (1-based Data<k>): NumBits<k>-bit fields LSB first in LE u32 words.
        static ZoneLayer ZonesLayer(UxBxml b, int k)
        {
            var nodes = new Dictionary<string, long[]>(StringComparer.Ordinal);
            foreach (var n in b.nodes) nodes[b.strings[(int)n[0]]] = n;
            int U(string t) => (int)UxBin.U32LE(b.pool, (int)nodes[t + I(k)][1]);
            var L = new ZoneLayer { W = U("Width"), H = U("Height"), nb = U("NumBits"), nw = U("NumWords") };
            var dn = nodes["Data" + I(k)];
            L.off = (int)dn[1];
            if (dn[3] != 4 || dn[2] != L.nw || (long)L.W * L.H * L.nb > (long)L.nw * 32)
                throw new UxException("zones Data" + I(k) + " is not a u32[" + I(L.nw) + "] field array for " + I(L.W) + "x" + I(L.H) + " x " + I(L.nb) + " bits");
            L.words = UxBin.Slice(b.pool, L.off, 4 * L.nw);
            L.vals = new long[L.W * L.H];
            for (int i = 0; i < L.vals.Length; i++)
            {
                long v = 0;
                for (int j = 0; j < L.nb; j++) { int bit = i * L.nb + j; if ((L.words[bit >> 3] >> (bit & 7) & 1) != 0) v |= 1L << j; }
                L.vals[i] = v;
            }
            return L;
        }
        static JObj Hist(long[] v)
        {
            var d = new SortedDictionary<long, long>();
            foreach (long x in v) d[x] = d.TryGetValue(x, out long c) ? c + 1 : 1;
            var j = new JObj(); foreach (var kv in d) j.items.Add(new KeyValuePair<string, object>(I(kv.Key), kv.Value));
            return j;
        }
        /// zones_patch(data, 'fill', fill): every cell of Data1..4 = fill[k-1] -> (new bytes or null when nothing changes, report).
        static (byte[] data, JObj report) ZonesPatch(byte[] data, string mode, int[] fill)
        {
            var b0 = new UxBxml(data);
            if (!UxBin.Same(b0.ToBytes(), data)) throw new UxException("zones packdata: BXML identity re-encode differs; refusing to patch");
            var b1 = new UxBxml(data);
            var rep = new JObj().Set("mode", mode); var layers = new JObj(); rep.Set("layers", layers);
            var ranges = new List<(int lo, int hi)>(); var want = new long[5][];
            long changed = 0;
            for (int k = 1; k <= 4; k++)
            {
                var L = ZonesLayer(b0, k);
                var nw = new long[L.vals.Length];
                if (mode != "fill") throw new UxException("zones mode " + mode + " is not ported");
                for (int i = 0; i < nw.Length; i++) nw[i] = fill[k - 1];
                long mx = 0; foreach (long x in nw) mx = Math.Max(mx, x);
                if (mx >= 1L << L.nb) throw new UxException("zones Data" + I(k) + ": value " + I(mx) + " does not fit " + I(L.nb) + " bits");
                long cc = 0; for (int i = 0; i < nw.Length; i++) if (nw[i] != L.vals[i]) cc++;
                layers.Set("L" + I(k - 1), new JObj().Set("size", new List<object> { (long)L.W, (long)L.H }).Set("bits", (long)L.nb).Set("before", Hist(L.vals)).Set("after", Hist(nw))
                    .Set("cells_changed", cc));
                want[k] = nw; changed += cc;
                if (cc > 0)
                {
                    var words = (byte[])L.words.Clone();
                    for (int i = 0; i < nw.Length; i++)
                        for (int j = 0; j < L.nb; j++)
                        {
                            int bit = i * L.nb + j; byte m = (byte)(1 << (bit & 7));
                            if ((nw[i] >> j & 1) != 0) words[bit >> 3] |= m; else words[bit >> 3] &= (byte)~m;
                        }
                    Buffer.BlockCopy(words, 0, b1.pool, L.off, words.Length);
                    ranges.Add((L.off, L.off + 4 * L.nw));
                }
            }
            rep.Set("cells_changed", changed);
            if (changed == 0) { rep.Set("replaced", false).Set("md5", UxBin.Md5(data)); return (null, rep); }
            byte[] o = b1.ToBytes();
            var b2 = new UxBxml(o);
            if (b2.version != b0.version || b2.unknown != b0.unknown || !SameTables(b0, b2) || b2.pool.Length != b0.pool.Length) throw new UxException("zones patch changed the BXML tables");
            long diff = 0;
            for (int i = 0; i < b0.pool.Length; i++)
                if (b0.pool[i] != b2.pool[i])
                {
                    diff++; bool inside = false;
                    foreach (var (lo, hi) in ranges) if (lo <= i && i < hi) { inside = true; break; }
                    if (!inside) throw new UxException("zones patch changed pool bytes outside the patched Data ranges");
                }
            for (int k = 1; k <= 4; k++)
            {
                var L2 = ZonesLayer(b2, k); var L0 = ZonesLayer(b0, k);
                for (int i = 0; i < L2.vals.Length; i++) if (L2.vals[i] != want[k][i]) throw new UxException("zones Data" + I(k) + " does not decode to the intended values");
                for (int bit = L2.W * L2.H * L2.nb; bit < 32 * L2.nw; bit++)
                    if ((L0.words[bit >> 3] >> (bit & 7) & 1) != (L2.words[bit >> 3] >> (bit & 7) & 1)) throw new UxException("zones Data" + I(k) + " padding bits changed");
            }
            rep.Set("replaced", true).Set("md5_before", UxBin.Md5(data)).Set("md5_after", UxBin.Md5(o)).Set("pool_bytes_changed", diff)
                .Set("proofs", "identity round trip; header/strings/attrs/nodes identical; only patched Data words changed; decoded layers as intended; padding bits kept");
            return (o, rep);
        }
        static bool SameTables(UxBxml a, UxBxml b)
        {
            if (a.strings.Count != b.strings.Count || a.attrs.Count != b.attrs.Count || a.nodes.Count != b.nodes.Count) return false;
            for (int i = 0; i < a.strings.Count; i++) if (a.strings[i] != b.strings[i]) return false;
            for (int i = 0; i < a.attrs.Count; i++) for (int k = 0; k < 4; k++) if (a.attrs[i][k] != b.attrs[i][k]) return false;
            for (int i = 0; i < a.nodes.Count; i++) for (int k = 0; k < 8; k++) if (a.nodes[i][k] != b.nodes[i][k]) return false;
            return true;
        }

        /// rename_scene(subs, rename): sub-scene names and every string attribute / value.
        static int RenameScene(List<UxSubScene> subs, IDictionary<string, string> rename)
        {
            int n = 0;
            foreach (var s in subs)
            {
                if (rename.TryGetValue(s.name, out string nn) && nn != s.name) { s.name = nn; n++; }
                foreach (var c in s.chunks) n += RenameTree(c, rename);
            }
            return n;
        }
        static int RenameTree(UxElem root, IDictionary<string, string> rename)
        {
            int n = 0;
            foreach (var x in root.Iter())
            {
                foreach (var a in x.attrs)
                    if (a.type == 1 && a.str != null && rename.TryGetValue(a.str, out string na) && na != a.str) { a.str = na; n++; }
                if (x.vtype == 1 && x.value is string v && rename.TryGetValue(v, out string nv) && nv != v) { x.value = nv; n++; }
            }
            return n;
        }

        /// One resource of the package being written.
        sealed class Blob { public UxElem elem; public byte[] logical; public bool compress; public byte[] stored; }

        /// rebuild_database (pkg.rebuild_with_additions + the slot rename + removals): the database bytes; the package goes to `package`.
        /// replacements / additions carry LOGICAL bytes; `packing` decides the stored form of every resource that has a Compress
        /// element in the reference ('raw' also drops the element). Returns (database, renamed strings, logical bytes, assets).
        static (byte[] database, int renamed, long logical, int assets) RebuildDatabase(UxTrackSet ts, UxTemplateAssets TA, IDictionary<(string, string), byte[]> replacements,
            IList<(string name, string type, byte[] data)> additions, string targetPackage, IDictionary<string, string> rename, ICollection<(string, string)> removals, Stream package,
            string packing, int threads, CancellationToken cancel)
        {
            var root = ts.db.ToTree();
            var handles = root.Find("Handles");
            var pkgs = root.Find("Packages").FindAll("Package");
            UxElem tp = null;
            foreach (var p in pkgs) if (p.AttrStr("name") == targetPackage) { tp = p; break; }
            UxBin.Require(tp != null, "no package " + targetPackage);
            UxElem tmplC = null;
            foreach (var p in pkgs) { foreach (var a in p.FindAll("Asset")) if (a.Find("Compress") != null) { tmplC = a; break; } if (tmplC != null) break; }
            UxBin.Require(tmplC != null && handles != null && handles.children.Count > 0, "database without a compressed asset / handle to copy");
            var tmplH = handles.children[0];
            var rem = new HashSet<(string, string)>(removals ?? new (string, string)[0]);
            if (rem.Count > 0)
            {
                int nEl = 0, nH = 0;
                foreach (var p in pkgs) foreach (var e in p.FindAll("Asset")) if (rem.Contains((e.AttrStr("name"), e.AttrStr("type")))) nEl++;
                foreach (var h in handles.children) if (rem.Contains((h.AttrStr("name"), h.AttrStr("type")))) nH++;
                if (nEl != rem.Count || nH != rem.Count) throw new UxException("removal of " + rem.Count + " assets: " + nEl + " package entries, " + nH + " handles (want one each)");
                foreach (var p in pkgs) p.children.RemoveAll(e => e.tag == "Asset" && rem.Contains((e.AttrStr("name"), e.AttrStr("type"))));
                handles.children.RemoveAll(h => rem.Contains((h.AttrStr("name"), h.AttrStr("type"))));
            }
            var have = new HashSet<(string, string)>(); var haveL = new HashSet<(string, string)>();
            foreach (var a in ts.assets) { have.Add((a.name, a.type)); haveL.Add((UxCommon.PyLower(a.name), a.type)); }
            var newElems = new Dictionary<UxElem, byte[]>(UxBxmlRef.Instance);
            foreach (var (name, typ, data) in additions)
            {
                if (have.Contains((name, typ)) || haveL.Contains((UxCommon.PyLower(name), typ))) throw new UxException("asset already exists (ignoring case): " + name + "." + typ);
                var e = tmplC.DeepCopy();
                e.attrs = new List<UxAttr> { new UxAttr("name", name), new UxAttr("type", typ) };
                tp.children.Add(e); newElems[e] = data;
                var h = tmplH.DeepCopy(); h.attrs = new List<UxAttr> { new UxAttr("name", name), new UxAttr("type", typ) };
                handles.children.Add(h);
            }
            var old = new Dictionary<(int, string, string), UxPcAsset>();
            foreach (var a in ts.assets) old[(a.pkg_node, a.name, a.type)] = a;
            // ---- the layout plan: per package the kept assets in their old order, then the additions
            var plan = new List<List<Blob>>();
            for (int pi = 0; pi < pkgs.Count; pi++)
            {
                var items = new List<(double order, Blob blob)>();
                foreach (var e in pkgs[pi].FindAll("Asset"))
                {
                    if (newElems.TryGetValue(e, out byte[] nb)) { items.Add((double.PositiveInfinity, new Blob { elem = e, logical = nb, compress = true })); continue; }
                    var a = old[(ts.packages[pi], e.AttrStr("name"), e.AttrStr("type"))];
                    byte[] logical = replacements != null && replacements.TryGetValue(a.Key, out var r) && r != null ? r : TA.Logical(a);
                    items.Add((a.Offset, new Blob { elem = e, logical = logical, compress = a.compressed }));
                }
                UxNp.StableSort(items, (x, y) => x.order.CompareTo(y.order));
                var list = new List<Blob>(); foreach (var it in items) list.Add(it.blob);
                plan.Add(list);
            }
            // ---- the stored form of every resource
            var all = new List<Blob>(); foreach (var l in plan) all.AddRange(l);
            long logicalBytes = 0; foreach (var bl in all) logicalBytes += bl.logical.Length;
            if (packing == "raw")
                foreach (var bl in all) { bl.stored = bl.logical; if (bl.compress) bl.elem.children.RemoveAll(c => c.tag == "Compress"); }
            else
            {
                var po = new ParallelOptions { MaxDegreeOfParallelism = threads > 0 ? threads : Environment.ProcessorCount, CancellationToken = cancel };
                Parallel.For(0, all.Count, po, i =>
                {
                    var bl = all[i];
                    bl.stored = !bl.compress ? bl.logical : packing == "lzx" ? UxLzxEncoder.EncodeBlob(bl.logical) : UxLzxEncoder.StoredBlob(bl.logical);
                });
            }
            // ---- offsets, sizes and the package bytes
            void SetU(UxElem el, string key, long val)
            {
                foreach (var at in el.attrs)
                    if (at.name == key)
                    {
                        var b = new byte[4];
                        if (at.type == 4) UxBin.PutU32(b, 0, checked((uint)val)); else UxBin.PutI32(b, 0, checked((int)val));
                        at.raw = b; at.str = null;
                        return;
                    }
                throw new UxException("no attribute " + key);
            }
            long pos = 0;
            for (int pi = 0; pi < pkgs.Count; pi++)
            {
                long newPo = pos;
                foreach (var bl in plan[pi])
                {
                    var heap = bl.elem.Find("Heap");
                    SetU(heap, "offset", pos - newPo); SetU(heap, "size", bl.stored.Length);
                    package.Write(bl.stored, 0, bl.stored.Length); pos += bl.stored.Length;
                    bl.stored = null;
                }
                foreach (string k in new[] { "offset", "heapOffset" }) if (pkgs[pi].Attr(k) != null) SetU(pkgs[pi], k, newPo);
            }
            int n = RenameTree(root, rename);
            return (UxBxml.EncodeTree(root, ts.db.version, ts.db.unknown), n, logicalBytes, all.Count);
        }
        sealed class UxBxmlRef : IEqualityComparer<UxElem>
        {
            public static readonly UxBxmlRef Instance = new UxBxmlRef();
            public bool Equals(UxElem a, UxElem b) => ReferenceEquals(a, b);
            public int GetHashCode(UxElem e) => System.Runtime.CompilerServices.RuntimeHelpers.GetHashCode(e);
        }

        // ================================================================================================ the build
        sealed class Item { public byte[] data; public (string name, string type) src; public Item(byte[] d, (string, string) s) { data = d; src = s; } }

        sealed class Builder
        {
            readonly UxContext ctx; readonly UxDisc disc; readonly UxTrackOptions o; readonly UxBuildShared sh; readonly Action<float, string> progress; readonly CancellationToken cancel;
            readonly UxLevelRow lv; readonly UxPaths paths; readonly Action<string> log;
            readonly string slot, tslot; readonly UxBase.Descriptor @base;
            /// FREESTYLE.md: a freestyle arena is a free-ride track (Beta_Fmx_Track_Slot_N) on the stock base's free-ride kit: no tdf, gate, board,
            /// scenario kit or SectionRace; S8 (ux_race) is not run.
            readonly bool fmx; JObj freestyle;
            readonly UxOrdered<string, string> rename = new UxOrdered<string, string>(StringComparer.Ordinal);
            readonly List<object> problems = new List<object>(), warnings = new List<object>();
            readonly JObj info = new JObj();
            readonly UxBuildResult result = new UxBuildResult();
            UxTrackSet ts; UxTemplateAssets TA; List<UxSubScene> subs; UxBxml levelBxml; UxBase.HfRoleSet hfRoles;
            UxMaterialsOptions matOpts; Dictionary<string, string> dirs; JObj stageRecord = new JObj();
            UxEntitiesResult ents; JObj edoc; UxRaceResult raceRes; JObj race; byte[] tdfBytes;
            readonly UxOrdered<(string name, string type), UxAsset> extraAssets = new UxOrdered<(string, string), UxAsset>();
            readonly JObj combined = new JObj();
            JObj splitgate; UxAssetMap splitgateAssets;
            UxSources sources; UxOrdered<(string name, string type), byte[]> variantAssets = new UxOrdered<(string, string), byte[]>();
            List<UxElem> frExtra = new List<UxElem>(); double[] hf; int hfN;
            UxOrdered<string, UxScreens.ScreenMaterial> screenMats = new UxOrdered<string, UxScreens.ScreenMaterial>(StringComparer.Ordinal);
            readonly UxOrdered<(string name, string type), (string source, string origin)> extraSources = new UxOrdered<(string, string), (string, string)>();
            UxOrdered<(string name, string type), string> renames = new UxOrdered<(string, string), string>();
            (string name, string type) zonesKey; ((string name, string type) key, byte[] data)? cameraflashReplacement;
            JObj assetRecord; UxOrdered<(string name, string type), string> scriptRen = new UxOrdered<(string, string), string>();

            public Builder(UxContext ctx, UxDisc disc, UxTrackOptions o, UxBuildShared sh, Action<float, string> progress, CancellationToken cancel)
            {
                this.ctx = ctx; this.disc = disc; this.o = o; this.sh = sh; this.progress = progress; this.cancel = cancel;
                lv = ctx.disc.lv; paths = o.paths ?? new UxPaths(); log = ctx.log ?? (s => { });
                fmx = UxCommon.IsFmx(lv);
                slot = UxCommon.RaceSlotName(lv, o.slot);
                @base = fmx ? UxBase.Resolve(sh.@base.level, "sx", "freeride") : sh.@base; tslot = @base.stem;
                foreach (string s in SUFFIXES) rename[tslot + s] = slot + s;
            }

            /// A step boundary: cancellation, the garbage of the step before (the stages leave hundreds of MB of decoded assets behind:
            /// collecting here keeps the peak near the live data), the progress callback.
            void Step(float f, string text)
            {
                cancel.ThrowIfCancellationRequested();
                if (o.collect) GC.Collect();
                progress?.Invoke(f, text);
            }
            T Timed<T>(string name, Func<T> f)
            {
                var sw = Stopwatch.StartNew(); T r = f();
                result.steps.Add(new KeyValuePair<string, double>(name, sw.Elapsed.TotalSeconds));
                return r;
            }
            void Timed(string name, Action f) => Timed(name, () => { f(); return 0; });

            /// role(name): the base's asset name for a sketch terrain role name (Custom_DISP, Custom_TopDown ... Custom_Zones).
            string Role(string name)
            {
                string n = hfRoles.Role(name);
                if (string.IsNullOrEmpty(n)) throw new UxException("stock base " + tslot + " has no " + name + " role");
                return n;
            }
            UxSubScene Sub(string suffix)
            {
                foreach (var s in subs) if (s.name == tslot + suffix) return s;
                throw new UxException("template scene has no sub-scene " + tslot + suffix);
            }

            // ------------------------------------------------------------------ main
            public UxBuildResult Run(string outFolder)
            {
                var clock = Stopwatch.StartNew();
                Step(0.00f, "reading the stock base " + tslot);
                Timed("base", LoadTemplate);
                RunStages();
                // ---- S7 / S8 in memory
                Step(0.62f, "entities");
                var items = Timed("entities", () =>
                {
                    var eo = new UxEntitiesOptions
                    {
                        kind = "sx", static_bales = o.staticBales, static_pop = o.staticPop, crowd_off = o.crowdOff, crowd_tier = o.crowdTier, group_number = o.groupNumber,
                        hb_freeride = o.hbFreeride, buried = o.buried, fr_clearance = o.frClearance, bale_trigger_move = UxEntitiesOptions.ParseMove(o.baleTriggerMove),
                        drop_gate_top = o.dropGateTop, bale_reseat = o.baleReseat, write = false,
                        manifest_paths = new Dictionary<string, string> { { "collision", dirs["collision"] + "/collision.json" }, { "materials", dirs["materials"] + "/materials.json" } },
                    };
                    ents = UxEntities.Convert(ctx.disc, ctx.stock, ctx.work, eo, null, cancel);
                    ents.Require();
                    edoc = ents.doc;
                    return new List<KeyValuePair<string, UxElem>>(ents.items);
                });
                Step(0.66f, "race line");
                if (!fmx) Timed("race", () =>
                {
                    var ro = new UxRaceOptions
                    {
                        slotName = slot, gatePatch = o.gatePatch, tdfPad = o.tdfPad, lapPacing = o.lappacing, deckRespawn = o.deckNorespawn, baleEdgeClear = o.baleEdgeClear,
                        dropgateMaterials = o.dropgateMaterials, startgateVisual = o.startgateVisual,
                    };
                    raceRes = UxRace.BuildRace(ctx.disc, ctx.stock, ctx.work, ro, slot, sh.RaceTemplate, null, cancel);
                    race = raceRes.race; tdfBytes = raceRes.tdf;
                    var rp = race.Get("problems") as List<object>;
                    if (rp != null && rp.Count > 0) throw new UxException("ux_race: " + UxJson.Dumps(rp, -1));
                    var tdf = Obj(race.Get("tdf"));
                    if ((string)tdf.Get("asset") != slot + "-sx" || UxBin.Md5(tdfBytes) != (string)UxJson.Get(tdf, "facts", "md5")) throw new UxException("ux_race tdf asset " + tdf.Get("asset") + " / md5 mismatch");
                    if (o.lappacing == "none" && raceRes.lapPacing != null) throw new UxException("--lappacing none but ux_race returned lap pacing bytes");
                    if (o.lappacing == "untamed" && (raceRes.lapPacing == null || (string)UxJson.Get(race, "lappacing", "engine_check") != "PASS"))
                        throw new UxException("lap pacing missing or fails the engine-reader check");
                    var failed = new List<string>();
                    if (race.Get("checks") is JObj checks)
                        foreach (var kv in checks.items)
                            if (kv.Value is bool bv && !bv || kv.Value is JObj cj && cj.Get("ok") is bool ok && !ok) failed.Add(kv.Key);
                    if (failed.Count > 0) warnings.Add("ux_race checks not ok (reported, not gated here): " + PyList(failed));
                });
                Step(0.72f, "tuff blocks, water, panels");
                Timed("assemble", () =>
                {
                    // ---- combined round (DESIGN 23): tuff blocks, EC water, MassiveAd panels (screens: collect_assets)
                    items = CombinedEntities(items);
                    // ---- FREESTYLE.md: the ramp faces carried by the ramp entities
                    if (fmx) items = FreestyleRamps(items);
                    // ---- --split-gate untamed (the Untamed drop gate on the base drop gate + its base)
                    if (o.splitGate == "untamed") items.AddRange(SplitGate());
                });
                List<UxElem> conv = null;
                Timed("scene", () => conv = AssembleScene(items));
                Step(0.78f, "assets");
                UxOrdered<(string name, string type), Item> data = null; UxOrdered<(string name, string type), byte[]> replacements = null;
                Timed("assets", () => (data, replacements) = CollectAssets(conv));
                Step(0.86f, "writing the track");
                Timed("write", () => Write(outFolder, conv, data, replacements));
                result.seconds = clock.Elapsed.TotalSeconds;
                info.Set("seconds", UxNp.PyRound(result.seconds, 1));
                result.info = BuildInfo();
                ctx.work.WriteJson("build_info.json", result.info);
                foreach (object w in warnings) result.warnings.Add(w as string);
                Step(1f, "done");
                return result;
            }

            // ------------------------------------------------------------------ template
            void LoadTemplate()
            {
                ts = sh.ts; TA = sh.TA;
                var loaded = UxPcScene.Load(ctx.stock.BaseFile("scene", tslot));
                var lvb = new UxBxml(ctx.stock.BaseFile("level", tslot));
                var md5 = new JObj();
                foreach (string k in UxCommon.SlotFileKinds) md5.Set(k, UxStock.StockPin(tslot, k));
                info.Set("template", new JObj().Set("dir", @base.Folder(paths)).Set("slot", tslot).Set("md5", md5).Set("md5_txt", "ok"));
                var (ks, nl, prep) = UxBase.Prepare(@base, loaded, lvb, ctx.stock, log);
                subs = ks; levelBxml = nl;
                hfRoles = UxBase.HfRoles(subs, tslot, ts);
                info.Set("base", @base.Record(paths).Set("preparation", prep).Set("hf_roles", hfRoles.Record()));
            }

            // ------------------------------------------------------------------ stages S1-S6
            /// Stages.ensure_all: level, terrain, meshes, collision, materials, textures, each into its variant folder. A stage is reused
            /// when build_stages.json says it was completed for the same disc level, options and port version (o.reuseStages).
            void RunStages()
            {
                matOpts = o.MaterialsOptions();
                var copt = new UxCollisionOptions { type9 = o.type9, popClip = o.popClip, threads = o.threads };
                dirs = new Dictionary<string, string>(StringComparer.Ordinal)
                {
                    { "level", "" }, { "meshes", "meshes" }, { "terrain", "terrain" }, { "collision", UxCollision.OutDirFor(false, o.type9) },
                    { "materials", "materials" + (matOpts.Tag().Length > 0 ? "-" + matOpts.Tag() : "") }, { "textures", UxTextures.OutDirFor(o.texMax) },
                };
                JObj stamp = o.reuseStages && ctx.work.Exists("build_stages.json") ? ctx.work.ReadJsonObj("build_stages.json") : new JObj();
                string identity = PORT + " " + lv.disc + " " + UxBin.Sha256(ctx.disc.ReadFile("database", cancel));
                if (stamp.Get("identity") as string != identity) stamp = new JObj().Set("identity", identity);
                var stages = new (string name, float at, string options, string manifest, Action run)[]
                {
                    ("level", 0.02f, "rigid_models=" + o.rigidModels, "level.json", () => UxLevel.Convert(ctx.disc, ctx.work, new UxLevelOptions { rigidModels = o.rigidModels }, null, cancel)),
                    ("terrain", 0.10f, "occ_map=" + o.occMap, "terrain/terrain.json", () => UxTerrainStage.Convert(ctx.disc, ctx.work, new UxTerrainOptions { occ_map = o.occMap, jobs = o.threads }, null, cancel, null)),
                    ("meshes", 0.30f, "", "meshes/meshes.json", () => UxMeshes.Convert(ctx.disc, ctx.work, new UxMeshesOptions { threads = o.threads }, null, cancel)),
                    ("collision", 0.38f, "type9=" + o.type9 + " pop_clip=" + o.popClip, dirs["collision"] + "/collision.json", () => UxCollision.Convert(ctx, copt)),
                    ("materials", 0.48f, UxJson.Dumps(matOpts.AsDict(), -1), dirs["materials"] + "/materials.json", () => UxMaterials.Convert(ctx, matOpts)),
                    ("textures", 0.56f, "tex_max=" + I(o.texMax), dirs["textures"] + "/textures.json", () => UxTextures.Convert(ctx, new UxTexturesOptions { texMax = o.texMax, threads = o.threads })),
                };
                bool upstreamRan = false;
                foreach (var st in stages)
                {
                    Step(st.at, "converting: " + st.name);
                    string dirText = paths.Work(lv, dirs[st.name].Length == 0 ? null : dirs[st.name]);
                    bool fresh = !upstreamRan && stamp.Get(st.name) as string == st.options && ctx.work.Exists(st.manifest);
                    if (fresh)
                    {
                        stageRecord.Set(st.name, new JObj().Set("action", "reused").Set("dir", dirText));
                        result.steps.Add(new KeyValuePair<string, double>(st.name, 0));
                        continue;
                    }
                    upstreamRan = true;
                    stamp.items.RemoveAll(kv => kv.Key == st.name);
                    if (o.reuseStages) ctx.work.WriteJson("build_stages.json", stamp);
                    var sw = Stopwatch.StartNew();
                    st.run();
                    double sec = sw.Elapsed.TotalSeconds;
                    result.steps.Add(new KeyValuePair<string, double>(st.name, sec));
                    stageRecord.Set(st.name, new JObj().Set("action", "ran").Set("reason", "run requested (no --reuse)").Set("seconds", UxNp.PyRound(sec, 1)).Set("dir", dirText));
                    stamp.Set(st.name, st.options);
                    ctx.work.WriteJson("build_stages.json", stamp);
                    ctx.disc.ClearCache();
                }
            }

            // ------------------------------------------------------------------ combined round (DESIGN 23)
            /// _extra(assets, tag): registers assets a combined-round module adds to <L>Base.
            void Extra(UxAssetMap assets, string tag)
            {
                foreach (var kv in assets)
                {
                    if (extraAssets.TryGet(kv.Key, out var old))
                    {
                        if (!UxBin.Same(old.data, kv.Value.data)) throw new UxException(tag + " asset " + K(kv.Key) + " offered by " + old.tag + " with other bytes");
                        continue;
                    }
                    extraAssets[kv.Key] = new UxAsset(kv.Value.data, kv.Value.source, kv.Value.origin, tag);
                }
            }

            UxSources NewSources()
            {
                var d = new Dictionary<string, string>(StringComparer.Ordinal);
                foreach (var kv in dirs) if (kv.Key != "level") d[kv.Key] = kv.Value;
                return new UxSources(lv, ctx.work, d, paths);
            }

            /// combined_entities: --bales tuff, --ec-water on, --massive-ads keep | model.
            List<KeyValuePair<string, UxElem>> CombinedEntities(List<KeyValuePair<string, UxElem>> items)
            {
                if (o.bales == "tuff")
                {
                    var (rec, need) = UxTuff.Swap(items, lv, o.tuffCollision, o.tuffWraps, log);
                    Extra(UxTuff.Assets(ctx.stock, need, paths), "tuff");
                    combined.Set("tuff", rec);
                }
                else combined.Set("tuff", new JObj().Set("option", "untamed"));
                if (o.ecWater == "on")
                {
                    bool untamed = double.IsNaN(o.ecWaterDepth);
                    string shore = o.ecWaterLook == "arena" ? o.ecWaterShore : "untamed";
                    UxWater.Ground ground = shore == "ground" && UxWater.HasWater(ctx.work) ? WaterGround() : null;
                    var r = UxWater.Build(ctx, null, 0, untamed ? 1e9 : o.ecWaterDepth, o.ecWaterKitOrder, paths, log, o.ecWaterLook, shore, ground);
                    if (untamed) r.record.Set("cap_ft", "untamed");
                    Extra(r.assets, "water");
                    foreach (var e in r.elems) items.Add(new KeyValuePair<string, UxElem>("Base", e));
                    if (r.elems.Count > 0 && o.ecWaterSplash == "on") WaterSplash(items, r.record);
                    if (r.elems.Count > 0 && o.ecWaterReflection != "level") WaterReflection(items, r.record, o.ecWaterReflection);
                    combined.Set("ec_water", r.record);
                }
                else combined.Set("ec_water", new JObj().Set("option", "off"));
                if (o.massiveAds != "drop")
                {
                    var src = NewSources();
                    if (src.problems.Count > 0) throw new UxException("stage outputs (MassiveAd panels): " + UxJson.Dumps(src.problems, -1));
                    var r = UxAds.Build(ctx, o.massiveAds, matOpts, src, TA, UxCommon.YLift(lv), o.groupNumber, paths, log);
                    Extra(r.assets, "ads");
                    foreach (var e in r.elems) items.Add(new KeyValuePair<string, UxElem>("Base", e));
                    combined.Set("massive_ads", r.record);
                }
                else combined.Set("massive_ads", new JObj().Set("option", "drop"));
                var ids = new HashSet<long>();
                foreach (var it in items) if (!ids.Add(EntityId(it.Value))) throw new UxException("combined round: duplicate entity id " + I(EntityId(it.Value)));
                return items;
            }

            /// water_ground: --ec-water-shore ground: the client's near albedo on the 2048 client grid of the ground this build draws:
            /// dirt-look stock = UxDirtLook's TopDown composite (the maps TerrainReplacements writes, computed once); dirt-look untamed
            /// = the terrain stage's TopDown on that grid.
            UxWater.Ground WaterGround()
            {
                double S = (UxCommon.HF_SIZE - 1) * 1;
                double[] alb;
                if (o.dirtLook == "stock")
                {
                    byte[] td = DirtLookMaps().topdown;
                    alb = new double[td.Length];
                    for (int i = 0; i < td.Length; i++) alb[i] = td[i] / 255.0;
                }
                else
                {
                    var tj = ctx.work.ReadJsonObj(dirs["terrain"] + "/terrain.json");
                    string td = UxJson.Get(tj, "ground", "default_topdown") as string ?? "topdown_occ.png";
                    alb = UxDirtLook.OnClientRgb(UxImaging.ReadPngRgb(ctx.work.ReadBytes(dirs["terrain"] + "/" + td)), S);
                }
                return new UxWater.Ground { albedo = alb, n = UxDirtLook.G, S = S };
            }

            /// water_splash: the base ParticleManager's kit + the bike water spray as UXW_<kit>; the base entity is pointed at it.
            void WaterSplash(List<KeyValuePair<string, UxElem>> items, JObj rec)
            {
                foreach (var it in items) if (UxCommon.RsdataType(it.Value) == "ParticleManagerRSData") throw new UxException("--ec-water-splash on: a converted ParticleManagerRSData (the base one is expected)");
                var pms = new List<UxElem>();
                foreach (var e in Sub("Base").Entities()) if (UxCommon.RsdataType(e) == "ParticleManagerRSData") pms.Add(e);
                if (pms.Count != 1) throw new UxException("--ec-water-splash on: " + pms.Count + " ParticleManagerRSData entities in the template Base (want 1)");
                var pm = pms[0];
                var roles = UxWater.ParticleManagerAssets(pm);
                string kit = null;
                foreach (var (role, _) in UxWater.PM_ROLES)
                {
                    if (!roles.TryGetValue(role, out var v) || v.name == null || kit != null && kit != v.name) throw new UxException("--ec-water-splash on: ParticleManager kit names differ (want one name for all three)");
                    kit = v.name;
                }
                byte[] Kit(string t)
                {
                    var ta = TA.Clash(kit, t);
                    if (ta == null || ta.name != kit) throw new UxException("--ec-water-splash on: the base has no " + kit + "." + t);
                    return TA.Logical(ta);
                }
                var (srec, assets) = UxWater.Splash(ctx.stock, Kit("particle"), Kit("partattr"), Kit("texture"), kit, log);
                Extra(assets, "water");
                UxWater.RetargetParticles(pm, (string)srec.Get("assets"));
                srec.Set("entity", EntityId(pm));
                rec.Set("splash", srec);
            }

            /// water_reflection: --ec-water-reflection dim | nolights: the level cube the water reflects (the template Base's
            /// AmbientDirectionalLight _ReflectionMap; in Open Reflex only the water samples it) copied as UXW_<cube> with the stadium
            /// lights dimmed / removed (UxWater.ReflectionCube), and the base entity pointed at the copy in place.
            void WaterReflection(List<KeyValuePair<string, UxElem>> items, JObj rec, string mode)
            {
                foreach (var it in items) if (UxCommon.RsdataType(it.Value) == "AmbientDirectionalLightRSData") throw new UxException("--ec-water-reflection: a converted AmbientDirectionalLightRSData (the base one is expected)");
                var amb = new List<UxElem>();
                foreach (var e in Sub("Base").Entities()) if (UxCommon.RsdataType(e) == "AmbientDirectionalLightRSData") amb.Add(e);
                if (amb.Count != 1) throw new UxException("--ec-water-reflection: " + amb.Count + " AmbientDirectionalLightRSData entities in the template Base (want 1)");
                var (cube, typ) = UxWater.ReflectionMap(amb[0]);
                var ta = cube == null ? null : TA.Clash(cube, "texture");
                if (typ != "texture" || ta == null || ta.name != cube) throw new UxException("--ec-water-reflection: the base has no texture " + cube);
                string name = UxWater.REFLECTION_PREFIX + cube;
                if (name.ToLowerInvariant().EndsWith("reflectionmap", StringComparison.Ordinal)) throw new UxException("--ec-water-reflection: " + name + " would be taken by the object shading (*ReflectionMap)");
                var (b, rrec) = UxWater.ReflectionCube(TA.Logical(ta), mode);
                var assets = new UxAssetMap();
                assets.Set(name, "texture", b, cube, "ux_water.reflection_cube (" + mode + ", base " + cube + ")");
                Extra(assets, "water");
                UxWater.ReflectionRetarget(amb[0], name);
                rrec.Set("option", mode).Set("source", cube).Set("asset", name).Set("entity", EntityId(amb[0])).Set("md5", UxBin.Md5(b));
                rec.Set("reflection", rrec);
                long n = 0; foreach (var x in (List<object>)rrec.Get("light_texels")) n += (long)((List<object>)x)[2];
                log?.Invoke("EC water reflection: " + name + " = " + cube + " with " + n + " light texels " + (mode == "dim" ? "dimmed" : "in-painted"));
            }

            /// screens(S): --screens stills -> material overrides + generated textures.
            void Screens(UxSources S)
            {
                screenMats = new UxOrdered<string, UxScreens.ScreenMaterial>(StringComparer.Ordinal);
                if (o.screens != "stills") { combined.Set("screens", new JObj().Set("option", "untamed")); return; }
                var r = UxScreens.Build(ctx, disc, n => S.material_entries.Get(n), o.screenLight, paths, log);
                screenMats = r.materials;
                Extra(r.textures, "screens");
                combined.Set("screens", r.record);
            }

            /// add_extra(data): the combined-round assets into <L>Base (see the Python doc for the clash rules).
            void AddExtra(UxOrdered<(string name, string type), Item> data)
            {
                var rec = new JObj();
                foreach (var kv in extraAssets)
                {
                    var key = kv.Key; var x = kv.Value; string name = K(key);
                    var ta = TA.Clash(key.name, key.type);
                    if (ta != null)
                    {
                        if (ta.name == key.name && UxBin.Same(TA.Logical(ta), x.data)) { rec.Set(name, x.tag + ": base copy (identical)"); continue; }
                        throw new UxException(x.tag + " asset " + name + " clashes with the base asset " + ta.name + " (other bytes)");
                    }
                    if (data.TryGet(key, out var have))
                    {
                        if (!UxBin.Same(have.data, x.data)) throw new UxException(x.tag + " asset " + name + " differs from the converted asset of that name");
                        rec.Set(name, x.tag + ": = converted asset"); continue;
                    }
                    foreach (var k in data.Keys) if (k.type == key.type && UxCommon.PyLower(k.name) == UxCommon.PyLower(key.name)) throw new UxException(x.tag + " asset " + name + " clashes ignoring case");
                    data[key] = new Item(x.data, key);
                    extraSources[key] = (x.source, x.origin);
                    rec.Set(name, x.tag + ": added");
                }
                info.Set("combined_assets", rec);
            }

            // ------------------------------------------------------------------ freestyle ramps (FREESTYLE.md)
            /// freestyle_ramps: UxFreestyle.Ramps: every collision-only ramp face the terrain stage left out of the HF goes into the collision of
            /// the ramp it lies on (a new kind-22 UXR_ asset, stock FMX ramp-top values); the assets go to <L>Base.
            List<KeyValuePair<string, UxElem>> FreestyleRamps(List<KeyValuePair<string, UxElem>> items)
            {
                var src = NewSources();
                if (src.problems.Count > 0) throw new UxException("stage outputs (freestyle ramps): " + UxJson.Dumps(src.problems, -1));
                var (rec, assets) = UxFreestyle.Ramps(items, ctx, src, UxCommon.YLift(lv), log, cancel);
                Extra(assets, "freestyle");
                freestyle = rec;
                info.Set("freestyle", rec);
                return items;
            }

            // ------------------------------------------------------------------ split gate
            List<KeyValuePair<string, UxElem>> SplitGate()
            {
                var src = NewSources();
                if (src.problems.Count > 0) throw new UxException("stage outputs (split gate): " + UxJson.Dumps(src.problems, -1));
                var r = UxSplitGate.Build(ctx, matOpts, src, TA, UxCommon.YLift(lv), o.groupNumber, paths, log);
                string dgn = @base.roles.dropgate ?? "TO_DropGate";
                var mv = Obj(Obj(race.Get("moves")).Get(dgn));
                if (mv == null) throw new UxException("--split-gate untamed: no " + dgn + " move");
                UxElem tdg = null;
                foreach (var e in Sub("Base").Entities()) if (NameOf(e) == dgn) { tdg = e; break; }
                if (tdg == null) throw new UxException("--split-gate untamed: the base has no " + dgn);
                var P = UxEntities.PropsOf(tdg);
                var props = new JObj(); foreach (var kv in r.dropgate_props.items) props.Set(kv.Key, kv.Value);
                var oldp = new JObj(); foreach (var kv in props.items) oldp.Set(kv.Key, UxEntities.GetValue(P[kv.Key]));
                mv.Set("props", props); mv.Set("old_props", oldp);
                if (UxJson.Get(race, "gate", "dropgate_materials") is JObj dm && UxEntities.Py.Truthy(dm.Get("written")))
                    dm.Set("superseded_by", "--split-gate untamed (the converted Untamed gate MaterialList)");
                splitgateAssets = r.assets;
                splitgate = r.record;
                var o2 = new List<KeyValuePair<string, UxElem>>();
                foreach (var e in r.elems) o2.Add(new KeyValuePair<string, UxElem>("Base", e));
                return o2;
            }

            // ------------------------------------------------------------------ scene
            List<UxElem> AssembleScene(List<KeyValuePair<string, UxElem>> items)
            {
                UxSubScene bs = Sub("Base"), fr = Sub("FreeRide");
                // strip placed template objects
                var removed = new JObj();
                foreach (var c in bs.chunks)
                {
                    var kept = new List<UxElem>();
                    foreach (var e in c.children)
                    {
                        string t = UxPcScene.Ns(e);
                        if (Array.IndexOf(STRIP, t) >= 0 && Array.IndexOf(KEEP_NAMES, NameOf(e)) < 0) removed.Set(t, (removed.Get(t) is long n ? n : 0) + 1);
                        else kept.Add(e);
                    }
                    c.children = kept;
                }
                info.Set("stripped", removed);
                var tents = new List<UxElem>(bs.Entities());
                var byName = new Dictionary<string, List<UxElem>>(StringComparer.Ordinal);
                foreach (var e in tents)
                {
                    string nm = NameOf(e) ?? "\0";
                    if (!byName.TryGetValue(nm, out var l)) byName[nm] = l = new List<UxElem>();
                    l.Add(e);
                }
                List<UxElem> Named(string nm, string type)
                {
                    var r = new List<UxElem>();
                    if (nm != null && byName.TryGetValue(nm, out var l)) foreach (var e in l) if (UxPcScene.Ns(e) == type) r.Add(e);
                    return r;
                }
                // heightfield entity: GridResolution 1 ft
                var hfe = tents.FindAll(e => UxPcScene.Ns(e) == "HFTerrainRSData");
                if (hfe.Count != 1) throw new UxException("template Base holds " + hfe.Count + " HFTerrainRSData");
                var P = UxEntities.PropsOf(hfe[0]);
                var fz = P.Has("FrictionZones") ? UxEntities.GetValue(P["FrictionZones"]) as JObj : null;
                if (fz == null || string.IsNullOrEmpty(fz.Get("asset") as string)) throw new UxException("template HFTerrainRSData has no FrictionZones asset (the core dereferences it unguarded)");
                zonesKey = ((string)fz.Get("asset"), (string)fz.Get("type"));
                var gr = Obj(UxEntities.GetValue(P["GridResolution"]));
                if (gr.Get("Enum") as string != "1" || UxJson.Int(gr.Get("EnumValue")) != 0) UxEntities.SetValue(P["GridResolution"], new JObj().Set("Enum", "1").Set("EnumValue", 0L));
                // distance fog values
                if (edoc.Get("distance_fog") is JObj fog)
                {
                    var te = Named(fog.Get("template_entity") as string, fog.Get("template_type") as string);
                    if (te.Count != 1) throw new UxException("template has " + te.Count + " " + fog.Get("template_type") + " " + fog.Get("template_entity") + " entities");
                    P = UxEntities.PropsOf(te[0]);
                    var values = Obj(fog.Get("values"));
                    foreach (var kv in values.items)
                    {
                        if (!P.Has(kv.Key)) throw new UxException("template DistanceFog has no prop " + kv.Key);
                        UxEntities.SetValue(P[kv.Key], kv.Value);
                    }
                    info.Set("distance_fog", new JObj().Set("source_id", fog.Get("source_id")).Set("values", values));
                }
                // WP8 moves
                var moved = new List<object>();
                if (race != null) foreach (var kv in Obj(race.Get("moves")).items)
                {
                    string nm = kv.Key; var mv = Obj(kv.Value);
                    var es = Named(nm, mv.Get("type") as string);
                    if (es.Count != 1) throw new UxException("move " + nm + ": " + es.Count + " template " + mv.Get("type") + " entities with that name");
                    var e = es[0];
                    if (mv.Get("old_matrix") is List<object> om && om.Count > 0)
                    {
                        var cur = UxPcScene.GetMatrix(e);
                        for (int i = 0; i < Math.Min(cur.Length, om.Count); i++)
                            if (Math.Abs(cur[i] - UxJson.Num(om[i])) > 1e-4) throw new UxException("move " + nm + ": template matrix differs from the one WP8 planned against");
                    }
                    UxPcScene.SetMatrix(e, MatrixOf(mv.Get("matrix")));
                    if (mv.Get("props") is JObj props)
                        foreach (var pv in props.items)
                        {
                            var Pe = UxEntities.PropsOf(e);
                            if (!Pe.Has(pv.Key)) throw new UxException("move " + nm + ": template entity has no prop " + pv.Key);
                            UxEntities.SetValue(Pe[pv.Key], pv.Value);
                        }
                    moved.Add(nm);
                }
                info.Set("moved", moved);
                AnchorMoves(Named);
                BaseLayoutScene(tents);
                // converted entities
                var convBase = new List<UxElem>(); var convFr = new List<UxElem>(); var frX = new List<UxElem>();
                foreach (var it in items)
                {
                    if (it.Key == "Base") convBase.Add(it.Value);
                    else if (it.Key == "FreeRide") { if (UxPcScene.Ns(it.Value) == "StartEntityFreeRideRSData") convFr.Add(it.Value); else frX.Add(it.Value); }
                }
                if (convFr.Count != 12) throw new UxException("WP7 gave " + convFr.Count + " FreeRide starts, not 12");
                // free-ride starts: the template's 12 StartEntityFreeRideRSData take the WP7 poses (by Index)
                var tstarts = new Dictionary<long, UxElem>();
                foreach (var e in fr.Entities()) if (UxPcScene.Ns(e) == "StartEntityFreeRideRSData") tstarts[UxJson.Int(UxEntities.GetValue(UxEntities.PropsOf(e)["Index"]))] = e;
                for (long i = 1; i <= 12; i++) if (tstarts.Count != 12 || !tstarts.ContainsKey(i)) throw new UxException("template FreeRide starts do not have Index 1..12");
                var starts = new List<(long idx, List<object> row)>();
                foreach (var e in convFr)
                {
                    var Pe = UxEntities.PropsOf(e);
                    long idx = UxJson.Int(UxEntities.GetValue(Pe["Index"])); var m = MatrixOf(UxEntities.GetValue(Pe["Matrix"]));
                    UxPcScene.SetMatrix(tstarts[idx], m);
                    starts.Add((idx, new List<object> { idx, UxNp.PyRound(m[12], 3), UxNp.PyRound(m[13], 3), UxNp.PyRound(m[14], 3) }));
                }
                UxNp.StableSort(starts, (a, b2) => a.idx.CompareTo(b2.idx));
                var rows = new List<object>(); foreach (var s in starts) rows.Add(s.row);
                info.Set("freeride_starts", new JObj().Set("source", "WP7 poses on the template entities").Set("layout", UxJson.Get(edoc, "freeride", "chosen")).Set("starts", rows));
                // dropped-material filter (--alpha drop, --crowd-off materials, --safe)
                sources = NewSources();
                if (sources.problems.Count > 0) throw new UxException("stage outputs: " + UxJson.Dumps(sources.problems, -1));
                var dropped = sources.dropped_materials;
                object frep = null;
                if (dropped != null && dropped.Count > 0)
                {
                    string fb = TA.Has("Editor_Default", "material") ? "Editor_Default" : null;
                    var pairs = new List<KeyValuePair<string, UxElem>>();
                    foreach (var e in convBase) pairs.Add(new KeyValuePair<string, UxElem>("Base", e));
                    foreach (var e in frX) pairs.Add(new KeyValuePair<string, UxElem>("FreeRide", e));
                    var f = UxBuildAssets.FilterDropped(pairs, dropped, ReadPart, log, fb);
                    variantAssets = f.new_assets; frep = f.report;
                    convBase = new List<UxElem>(); frX = new List<UxElem>();
                    foreach (var kv in f.items) if (kv.Key == "Base") convBase.Add(kv.Value); else if (kv.Key == "FreeRide") frX.Add(kv.Value);
                    foreach (var e in convBase) foreach (var r in UxBuildAssets.EntityRefs(e)) if (r.type == "material" && dropped.Contains(r.name)) throw new UxException("dropped materials still referenced: " + r.name);
                    foreach (var e in frX) foreach (var r in UxBuildAssets.EntityRefs(e)) if (r.type == "material" && dropped.Contains(r.name)) throw new UxException("dropped materials still referenced: " + r.name);
                }
                info.Set("dropped_material_filter", frep);
                if (fmx && freestyle != null)       // FREESTYLE.md: carrier bounds after the filter
                {
                    byte[] ReadModel(string n) => variantAssets.TryGet((n, "model"), out var vb) ? vb : sources.Read((n, "model"));
                    byte[] ReadCollis(string n) => extraAssets[(n, "collis")].data;
                    Extra(UxFreestyle.WidenCarriers(convBase, freestyle, ReadModel, ReadCollis, log), "freestyle");
                }
                // template y offset
                YOffset(bs, fr);
                frExtra = frX;
                // id uniqueness
                var ids = new HashSet<long>();
                foreach (var s in subs) foreach (var e in s.Entities()) if (!ids.Add(EntityId(e))) throw new UxException("duplicate entity ids: " + I(EntityId(e)));
                foreach (var e in convBase) if (!ids.Add(EntityId(e))) throw new UxException("duplicate entity ids: " + I(EntityId(e)));
                foreach (var e in frX) if (!ids.Add(EntityId(e))) throw new UxException("duplicate entity ids: " + I(EntityId(e)));
                return convBase;
            }
            static double[] MatrixOf(object v)
            {
                if (v is double[] d) return d;
                var l = (System.Collections.IList)v; var r = new double[l.Count];
                for (int i = 0; i < r.Length; i++) r[i] = UxJson.Num(l[i]);
                return r;
            }
            (byte[] model, byte[] surface) ReadPart(string name)
            {
                if (variantAssets.Has((name, "model"))) throw new UxException("nested variant " + name);
                return (sources.Read((name, "model")), sources.Read((name, "surface")));
            }

            static readonly Regex s_anchorMove = new Regex(@"^\s*([A-Za-z0-9_]+)\s*=\s*([-+0-9.eE]+)\s*,\s*([-+0-9.eE]+)\s*,\s*([-+0-9.eE]+)\s*$", RegexOptions.CultureInvariant);
            /// anchor_moves: --anchor-move NAME=dx,dy,dz translates the named base AnchorEntityRSData after the race kit moves.
            void AnchorMoves(Func<string, string, List<UxElem>> named)
            {
                var rec = new List<object>();
                foreach (string sp in o.anchorMoves ?? new string[0])
                {
                    var m = s_anchorMove.Match(sp);
                    if (!m.Success) throw new UxException("--anchor-move '" + sp + "': want NAME=dx,dy,dz");
                    string nm = m.Groups[1].Value;
                    double[] d = { UxJson.ParseDouble(m.Groups[2].Value), UxJson.ParseDouble(m.Groups[3].Value), UxJson.ParseDouble(m.Groups[4].Value) };
                    var es = named(nm, "AnchorEntityRSData");
                    if (es.Count != 1) throw new UxException("--anchor-move " + nm + ": " + es.Count + " AnchorEntityRSData of that name in Base");
                    var mm = UxPcScene.GetMatrix(es[0]);
                    double[] old = { mm[12], mm[13], mm[14] };
                    mm[12] += d[0]; mm[13] += d[1]; mm[14] += d[2];
                    UxPcScene.SetMatrix(es[0], mm);
                    rec.Add(new JObj().Set("name", nm).Set("offset", Nums(d)).Set("old", new List<object> { UxNp.PyRound(old[0], 4), UxNp.PyRound(old[1], 4), UxNp.PyRound(old[2], 4) })
                        .Set("new", new List<object> { UxNp.PyRound(mm[12], 4), UxNp.PyRound(mm[13], 4), UxNp.PyRound(mm[14], 4) }));
                }
                info.Set("anchor_moves", rec);
            }

            void YOffset(UxSubScene bs, UxSubScene fr)
            {
                hf = UxEntities.ReadNpy(ctx.work.ReadBytes(dirs["terrain"] + "/hf.npy"), out int[] shape);
                UxBin.Require(shape.Length == 2 && shape[0] == shape[1], "hf.npy is not a square 2-D array");
                hfN = shape[0];
                UxElem First(IEnumerable<UxElem> es)
                {
                    UxElem best = null; long bi = long.MaxValue;
                    foreach (var e in es)
                        if (UxPcScene.Ns(e) == "StartEntityFreeRideRSData")
                        {
                            long ix = UxJson.Int(UxEntities.GetValue(UxEntities.PropsOf(e)["Index"]));
                            if (ix < bi) { bi = ix; best = e; }
                        }
                    return best;
                }
                var m = UxPcScene.GetMatrix(First(fr.Entities()));
                double ours = GroundFt(hf, hfN, 1, m[12], m[14]);
                int tplGrid = 1;
                foreach (var e in bs.Entities())
                    if (UxPcScene.Ns(e) == "HFTerrainRSData") tplGrid = 1 << (int)UxJson.Int(Obj(UxEntities.GetValue(UxEntities.PropsOf(e)["GridResolution"])).Get("EnumValue"));
                var t = new UxPcTexture(TA.Logical(ts.Get(Role("Custom_DISP"), "texture")));
                var (_, tw, th, _) = t.recs[0];
                var tex = new double[th * tw];
                for (int i = 0; i < tex.Length; i++) tex[i] = (double)UxBin.F32LE(t.entries[0], 4 * i) * UxCommon.HEIGHT_SCALE;
                UxSubScene tfr = null;
                foreach (var s in UxPcScene.Load(ctx.stock.BaseFile("scene", tslot))) if (s.name == tslot + "FreeRide") { tfr = s; break; }
                var tm = UxPcScene.GetMatrix(First(tfr.Entities()));
                double tg = GroundFt(tex, th, tplGrid, tm[12], tm[14]);
                double yoff = ours - tg;
                string how = "auto: start ground " + UxImaging.PyFixed(ours, 2) + " ft - template start ground " + UxImaging.PyFixed(tg, 2) + " ft";
                var movedTypes = new List<object>();
                if (yoff != 0)
                    foreach (var e in bs.Entities())
                        if (Array.IndexOf(TEMPLATE_Y_ENTITIES, UxPcScene.Ns(e)) >= 0)
                        {
                            var mm = UxPcScene.GetMatrix(e); mm[13] += yoff; UxPcScene.SetMatrix(e, mm); movedTypes.Add(UxPcScene.Ns(e));
                        }
                info.Set("template_y_offset", new JObj().Set("offset_ft", UxNp.PyRound(yoff, 4)).Set("how", how).Set("moved", movedTypes));
            }

            /// base_layout_scene: AmbientDirectionalLight._ColorReferenceMap -> the flat Editor_Default_White (--base-layout neutral).
            void BaseLayoutScene(List<UxElem> tents)
            {
                var rec = new JObj().Set("option", o.baseLayout);
                var adl = tents.FindAll(e => UxPcScene.Ns(e) == "AmbientDirectionalLightRSData");
                if (adl.Count != 1) throw new UxException("stock base holds " + adl.Count + " AmbientDirectionalLightRSData");
                var P = UxEntities.PropsOf(adl[0]);
                object old = P.Has("_ColorReferenceMap") ? UxEntities.GetValue(P["_ColorReferenceMap"]) : null;
                rec.Set("colorref_before", old);
                if (o.baseLayout == "neutral" && old != null)
                {
                    if (!TA.Has(COLORREF_NEUTRAL.name, COLORREF_NEUTRAL.type)) throw new UxException("stock base has no " + K(COLORREF_NEUTRAL) + " for the neutral colour reference");
                    UxEntities.SetValue(P["_ColorReferenceMap"], new JObj().Set("asset", COLORREF_NEUTRAL.name).Set("type", COLORREF_NEUTRAL.type));
                    rec.Set("colorref_after", new JObj().Set("asset", COLORREF_NEUTRAL.name).Set("type", COLORREF_NEUTRAL.type));
                }
                info.Set("base_layout", rec);
            }

            // ------------------------------------------------------------------ assets
            (UxOrdered<(string name, string type), Item> data, UxOrdered<(string name, string type), byte[]> replacements) CollectAssets(List<UxElem> conv)
            {
                var S = sources; var gl = sh.Globals;
                // 1. references of the converted entities
                var refs = new UxOrdered<(string name, string type), int>();
                foreach (var e in conv) foreach (var r in UxBuildAssets.EntityRefs(e)) refs[r] = refs.Get(r) + 1;
                foreach (var e in frExtra) foreach (var r in UxBuildAssets.EntityRefs(e)) refs[r] = refs.Get(r) + 1;
                var want = new UxOrdered<(string name, string type), string>();
                var unresolved = new List<string>(); var fromTemplate = new SortedDictionary<string, long>(StringComparer.Ordinal); var fromGlobal = new JObj();
                var variant = variantAssets;
                var sga = splitgateAssets;
                Screens(S);
                var xa = extraAssets;
                void Need((string name, string type) key, string why)
                {
                    if (want.Has(key) || sga != null && sga.Has(key.name, key.type) || xa.Has(key)) return;
                    if (variant.Has(key)) { want[key] = "variant"; return; }
                    if (S.assets.Has(key)) { want[key] = "source"; return; }
                    if (TA.Has(key)) { fromTemplate[key.type] = (fromTemplate.TryGetValue(key.type, out long n) ? n : 0) + 1; return; }
                    if (gl.ContainsKey(key)) { fromGlobal.Set(K(key), (fromGlobal.Get(K(key)) is long n ? n : 0) + 1); return; }
                    if (Array.IndexOf(UxStock.BASE_GAPS, key) >= 0)
                    {   // combined round: a template texture the stock base lacks
                        byte[] bb = ctx.stock.Get(key.name, key.type);
                        if (bb != null)
                        {
                            xa[key] = new UxAsset(bb, paths.StockLib(key.name, key.type), "stock " + UxStock.Record(key.name, key.type).source +
                                " (templates/stock-lib; the DE sketch carried it, SX_FortDodge does not)", "base-gap");
                            return;
                        }
                    }
                    unresolved.Add(K(key) + " (" + why + ")");
                }
                foreach (var key in refs.Keys) Need(key, "entity");
                // 2. materials -> textures and shaders
                foreach (var key in new List<(string name, string type)>(want.Keys))
                {
                    if (key.type != "material") continue;
                    if (screenMats.TryGet(key.name, out var ov))
                    {   // --screens stills: the still material's textures and shader
                        foreach (string t in ov.textures) Need((t, "texture"), "screen material " + key.name);
                        Need((ov.template, "shader"), "screen material " + key.name);
                        continue;
                    }
                    var ent = S.assets[key].entry;
                    foreach (object t in UxJson.Arr(ent.Get("textures"))) Need(((string)t, "texture"), "material " + key.name);
                    Need(((string)ent.Get("template"), "shader"), "material " + key.name);
                }
                if (unresolved.Count > 0) throw new UxException("unresolved asset references: " + unresolved.Count + ", e.g. " + PyList(unresolved.GetRange(0, Math.Min(8, unresolved.Count))));
                // 3. clashes with the template package (case-insensitive): identical bytes -> template wins; else UX_<name>
                var data = new UxOrdered<(string name, string type), Item>();
                var ren = new UxOrdered<(string name, string type), string>();
                var same = new List<object>();
                var order = new List<(string name, string type)>(want.Keys);
                UxNp.StableSort(order, (a, b2) => { int c = UxBuildAssets.TypeOrder(a.type).CompareTo(UxBuildAssets.TypeOrder(b2.type)); return c != 0 ? c : string.CompareOrdinal(a.name, b2.name); });
                foreach (var key in order)
                {
                    byte[] bb = want[key] == "variant" ? variant[key] : S.Read(key);
                    if (key.type == "material" && screenMats.TryGet(key.name, out var sm)) bb = sm.data;
                    var ta = TA.Clash(key.name, key.type);
                    if (ta != null)
                    {
                        if (UxBin.Same(TA.Logical(ta), bb))
                        {
                            same.Add(K(key));
                            if (ta.name != key.name) ren[key] = ta.name;
                            continue;
                        }
                        string nn = "UX_" + key.name;
                        if (TA.Clash(nn, key.type) != null || want.Has((nn, key.type))) throw new UxException("cannot rename " + K(key) + ": " + nn + " exists too");
                        ren[key] = nn;
                        data[(nn, key.type)] = new Item(bb, key);
                        continue;
                    }
                    data[key] = new Item(bb, key);
                }
                // WP13c start-gate patch assets (ux_race)
                var raceKeys = new HashSet<(string, string)>();
                if (raceRes != null) foreach (var (name, type, bytes) in raceRes.assets)
                {
                    if (TA.Clash(name, type) != null || data.Has((name, type))) throw new UxException("start-gate patch asset " + name + "." + type + " clashes with an existing asset");
                    data[(name, type)] = new Item(bytes, (name, type)); raceKeys.Add((name, type));
                }
                // DESIGN 18 additions: CameraFlash (crowd builds) and the lap pacing asset (sx)
                foreach (var k in new List<(string name, string type)>(data.Keys))
                    if (k.type == "material" && screenMats.TryGet(k.name, out var sm)) extraSources[k] = ("ux_screens", "ux_screens: Template_Prelit [" + string.Join(", ", sm.textures) + "]");
                AddSplitgate(data);
                AddExtra(data);
                AddCameraflash(want, S, data);
                AddLappacing(data);
                // own case-insensitive duplicates
                var lc = new HashSet<(string, string)>();
                foreach (var k in data.Keys) if (!lc.Add((UxCommon.PyLower(k.name), k.type))) throw new UxException("converted assets clash ignoring case: " + K(k));
                // 4. apply renames to references
                var matPatch = new JObj();
                if (ren.Count > 0)
                {
                    var texRen = new Dictionary<string, string>(StringComparer.Ordinal); var entRen = new Dictionary<(string, string), string>();
                    foreach (var kv in ren)
                    {
                        if (kv.Key.type == "shader") throw new UxException("a stock shader differs from the template copy with the same name: " + kv.Key.name);
                        if (kv.Key.type == "texture") texRen[kv.Key.name] = kv.Value; else entRen[kv.Key] = kv.Value;
                    }
                    if (texRen.Count > 0)
                        foreach (var key in new List<(string name, string type)>(data.Keys))
                        {
                            if (key.type != "material") continue;
                            var it = data[key];
                            if (!S.assets.Has(it.src)) continue;                       // combined-round materials (none name a renamed texture)
                            if (screenMats.TryGet(key.name, out var sm))
                            {
                                foreach (string t in sm.textures) if (texRen.ContainsKey(t)) throw new UxException("screen material " + key.name + " names a renamed texture " + t);
                                continue;
                            }
                            var ent = S.assets[it.src].entry;
                            var hit = new Dictionary<string, string>(StringComparer.Ordinal); var hitRec = new JObj();
                            foreach (object t in UxJson.Arr(ent.Get("textures"))) if (texRen.TryGetValue((string)t, out string nn)) { hit[(string)t] = nn; hitRec.Set((string)t, nn); }
                            if (hit.Count > 0) { data[key] = new Item(UxMaterials.RebuildEntry(ctx.stock, ent, hit), it.src); matPatch.Set(key.name, hitRec); }
                        }
                    if (entRen.Count > 0)
                    {
                        foreach (var e in conv) UxBuildAssets.RenameRefs(e, entRen);
                        foreach (var e in frExtra) UxBuildAssets.RenameRefs(e, entRen);
                    }
                }
                renames = ren;
                // 5. terrain replacements (+ tdf)
                var replacements = TerrainReplacements();
                byte[] hm = HfMaterialPatch();
                if (hm != null) replacements[(Role("Custom_HFTRN"), "material")] = hm;
                byte[] z = ZonesReplacement();
                if (z != null) replacements[zonesKey] = z;
                if (cameraflashReplacement != null) replacements[cameraflashReplacement.Value.key] = cameraflashReplacement.Value.data;
                if (!fmx)
                {
                    replacements[(tslot + "-sx", "tdf")] = tdfBytes;
                    if (!TA.Has(tslot + "-sx", "tdf")) throw new UxException("template has no tdf " + tslot + "-sx");
                }
                // ---- the record
                var added = new SortedDictionary<string, long>(StringComparer.Ordinal); long addedBytes = 0;
                var list = new List<object>();
                foreach (var kv in data)
                {
                    added[kv.Key.type] = (added.TryGetValue(kv.Key.type, out long n) ? n : 0) + 1; addedBytes += kv.Value.data.Length;
                    string src, origin;
                    if (extraSources.TryGet(kv.Key, out var xs)) { src = xs.source; origin = xs.origin; }
                    else if (S.assets.TryGet(kv.Value.src, out var sa)) { src = S.PyPath(sa.file); origin = sa.origin; }
                    else if (raceKeys.Contains(kv.Value.src)) { src = "race kit (start-gate patch)"; origin = "ux_race"; }
                    else { src = "variant (group filter)"; origin = "filter_dropped"; }
                    list.Add(new JObj().Set("name", kv.Key.name).Set("type", kv.Key.type).Set("bytes", (long)kv.Value.data.Length).Set("md5", UxBin.Md5(kv.Value.data)).Set("source", src).Set("origin", origin));
                }
                var addedJ = new JObj(); foreach (var kv in added) addedJ.Set(kv.Key, kv.Value);
                var tref = new JObj(); foreach (var kv in fromTemplate) tref.Set(kv.Key, kv.Value);
                var renJ = new JObj(); foreach (var kv in ren) renJ.Set(K(kv.Key), kv.Value);
                var variants = new List<string>(); foreach (var k in variant.Keys) if (k.type == "model") variants.Add(k.name);
                variants.Sort(string.CompareOrdinal);
                var kit = new List<object>(); if (raceRes != null) foreach (var (name, type, _) in raceRes.assets) kit.Add(name + "." + type);
                assetRecord = new JObj().Set("added", addedJ).Set("added_bytes", addedBytes).Set("template_refs", tref).Set("global_refs", fromGlobal).Set("identical_to_template", same)
                    .Set("renames", renJ).Set("materials_retextured", matPatch).Set("variants", Strs(variants)).Set("race_kit", kit).Set("list", list);
                return (data, replacements);
            }

            /// add_splitgate(data): the ux_splitgate assets into <L>Base.
            void AddSplitgate(UxOrdered<(string name, string type), Item> data)
            {
                if (splitgateAssets == null) return;
                var S = sources; var used = new List<object>();
                foreach (var kv in splitgateAssets)
                {
                    var key = kv.Key; byte[] bb = kv.Value.data; string name = K(key);
                    if (S.assets.Has(key) && !UxBin.Same(S.Read(key), bb)) throw new UxException("split-gate asset " + name + " differs from the stage output " + S.PyPath(S.assets[key].file));
                    var ta = TA.Clash(key.name, key.type);
                    if (ta != null)
                    {
                        if (ta.name == key.name && UxBin.Same(TA.Logical(ta), bb)) { used.Add(name + " (template)"); continue; }
                        throw new UxException("split-gate asset " + name + " clashes with the template asset " + ta.name);
                    }
                    if (data.TryGet(key, out var have))
                    {
                        if (!UxBin.Same(have.data, bb)) throw new UxException("split-gate asset " + name + " differs from the converted asset of that name");
                        used.Add(name + " (already in the build)"); continue;
                    }
                    foreach (var k in data.Keys) if (k.type == key.type && UxCommon.PyLower(k.name) == UxCommon.PyLower(key.name)) throw new UxException("split-gate asset " + name + " clashes ignoring case");
                    data[key] = new Item(bb, key);
                    extraSources[key] = (kv.Value.source, kv.Value.origin);
                    used.Add(name + (S.assets.Has(key) ? " (= stage output " + S.assets[key].origin + ")" : ""));
                }
                splitgate.Set("assets_in_package", used);
            }

            /// add_cameraflash: the base's 'CameraFlash' texture gets the converted Untamed bytes when the build has crowd materials.
            void AddCameraflash(UxOrdered<(string name, string type), string> want, UxSources S, UxOrdered<(string name, string type), Item> data)
            {
                long crowd = 0;
                foreach (var k in want.Keys) if (k.type == "material" && S.assets.Has(k) && S.assets[k].entry?.Get("template") as string == "crowd") crowd++;
                var rec = new JObj().Set("option", o.noCameraflash ? "off" : "on").Set("crowd_materials", crowd).Set("added", false);
                var key = ("CameraFlash", "texture");
                cameraflashReplacement = null;
                var ta = TA.Clash(key.Item1, key.Item2);
                if (o.noCameraflash || crowd == 0) rec.Set("reason", o.noCameraflash ? "--no-cameraflash" : "no crowd materials in this build");
                else if (ta != null && o.baseCameraflash == "untamed")
                {
                    if (ta.name != key.Item1) throw new UxException("base CameraFlash is named '" + ta.name + "' (the crowd looks up '" + key.Item1 + "' exactly)");
                    const string file = "texture/cameraflash.texture";
                    if (!ctx.disc.HasExtractFile(file)) throw new UxException("crowd build without " + paths.Extract(lv, file));
                    byte[] sb = ctx.disc.ReadExtractFile(file, cancel);
                    var conv = UxTextures.ConvertTexture(sb, "CameraFlash", 0, "nearest", null, ctx.stock.templateNames);
                    if (conv.pc == null) throw new UxException("cameraflash.texture not converted: " + conv.rec.Get("reason"));
                    var pr = UxTextures.CheckTexture(sb, conv.pc, conv.rec, conv.expect, out _);
                    pr.AddRange(UxTextures.PcLayoutProblems(conv.pc));
                    if (pr.Count > 0) throw new UxException("CameraFlash texture: " + string.Join("; ", pr));
                    cameraflashReplacement = ((ta.name, ta.type), conv.pc);
                    rec.Set("added", false).Set("replaced", ta.name + "." + ta.type + " (base " + tslot + " bytes, md5 " + UxBin.Md5(TA.Logical(ta)) + ")").Set("option_base", "untamed")
                        .Set("source", paths.Extract(lv, file)).Set("source_md5", UxBin.Md5(sb)).Set("bytes", (long)conv.pc.Length).Set("md5", UxBin.Md5(conv.pc))
                        .Set("format", conv.rec.Get("fmt_out")).Set("size", new List<object> { conv.rec.Get("w"), conv.rec.Get("h") }).Set("mips", conv.rec.Get("mips"))
                        .Set("action", conv.rec.Get("action")).Set("reason", conv.rec.Get("reason"));
                }
                else if (ta != null) rec.Set("reason", "the template already holds " + ta.name);
                else throw new UxException("a base without a CameraFlash texture is not ported (the stock base holds one)");
                info.Set("cameraflash", rec);
            }

            /// add_lappacing: ux_race's '<slot>-lappacing' bxml in <L>Base (only with --lappacing untamed).
            void AddLappacing(UxOrdered<(string name, string type), Item> data)
            {
                if (fmx) return;
                var lp = race.Get("lappacing") as JObj ?? new JObj();
                var rec = new JObj();
                foreach (string k in new[] { "asset", "type", "included", "source", "source_md5", "events_in", "renamed", "added", "bytes", "md5", "engine_check", "reason" }) rec.Set(k, lp.Get(k));
                if (raceRes.lapPacing != null)
                {
                    var key = ((string)lp.Get("asset"), "bxml");
                    if (TA.Clash(key.Item1, key.Item2) != null || data.Has(key)) throw new UxException("lap pacing asset " + key.Item1 + " clashes with an existing asset");
                    data[key] = new Item(raceRes.lapPacing, key);
                    extraSources[key] = (lp.Get("source") as string, "ux_race / lappacing_to_reflex.to_reflex (DESIGN 18)");
                }
                info.Set("lappacing", rec);
            }

            /// zones_replacement: the FrictionZones packdata for --zones fill (null = keep the template bytes).
            byte[] ZonesReplacement()
            {
                byte[] data = TA.Logical(ts.Get(zonesKey.name, zonesKey.type));
                var rec = new JObj().Set("mode", o.zones).Set("asset", K(zonesKey)).Set("template_md5", UxBin.Md5(data));
                byte[] outp = null;
                if (o.zones == "template") rec.Set("replaced", false).Set("reason", "--zones template: template bytes kept");
                else
                {
                    var (nb, rep) = ZonesPatch(data, o.zones, o.zonesFill);
                    outp = nb;
                    foreach (var kv in rep.items) rec.Set(kv.Key, kv.Value);
                    rec.Set("fill", new List<object> { (long)o.zonesFill[0], (long)o.zonesFill[1], (long)o.zonesFill[2], (long)o.zonesFill[3] });
                    if (outp == null) rec.Set("reason", "no cell changes (template has no value to remap)");
                }
                if (outp != null)
                    foreach (long v in ZonesLayer(new UxBxml(outp), 1).vals) if (v == 7) throw new UxException("zones layer 0 still holds value 7 after --zones " + o.zones);
                info.Set("zones", rec);
                return outp;
            }

            /// hf_material_patch: the base's HF material with usr_dirt_tint_color = DIRT_TINT_CONST (--dirt-tint const) and (--hf-params
            /// today) usr_detail_tile / usr_bump_scalar = HF_PARAMS_TODAY, patched in the pool. null when nothing changes (the dirt-look
            /// stock defaults: --dirt-tint template, --hf-params base).
            byte[] HfMaterialPatch()
            {
                var key = (Role("Custom_HFTRN"), "material");
                byte[] data = TA.Logical(ts.Get(key.Item1, key.Item2));
                var b = new UxBxml(data);
                if (!UxBin.Same(b.ToBytes(), data)) throw new UxException(key.Item1 + " does not round-trip through bxml.Bxml");
                int Param(string name)
                {
                    long[] hit = null; int hits = 0;
                    foreach (var n in b.nodes)
                    {
                        if (b.strings[(int)n[0]] != "Parameter") continue;
                        string nm = null;
                        for (long ai = n[6]; ai < n[6] + n[7]; ai++) { var a = b.attrs[(int)ai]; if (b.strings[(int)a[0]] == "name") nm = a[3] == 1 ? b.strings[(int)a[1]] : null; }
                        if (nm == name) { hit = n; hits++; }
                    }
                    if (hits != 1 || hit[3] != 7 || hit[1] < 0 || hit[1] + 16 > b.pool.Length) throw new UxException(key.Item1 + ": " + hits + " " + name + " Parameters (want one 4-float value)");
                    return (int)hit[1];
                }
                var want = new List<(string name, float[] v)>();
                if (o.DirtTint == "const") want.Add(("usr_dirt_tint_color", DIRT_TINT_CONST));
                if (o.HfParams == "today") foreach (var (n, v) in HF_PARAMS_TODAY) want.Add((n, v));
                var values = new JObj();
                var rec = new JObj().Set("material", key.Item1).Set("dirt_tint", o.DirtTint).Set("hf_params", o.HfParams).Set("values", values);
                var ranges = new List<(int lo, int hi)>();
                foreach (var (name, v) in want)
                {
                    int off = Param(name);
                    var oldL = new List<object>(); var newL = new List<object>();
                    for (int i = 0; i < 4; i++) { oldL.Add(UxNp.PyRound(UxBin.F32LE(b.pool, off + 4 * i), 7)); newL.Add(UxNp.PyRound(v[i], 7)); }
                    values.Set(name, new JObj().Set("old", oldL).Set("new", newL));
                    for (int i = 0; i < 4; i++) UxBin.PutF32(b.pool, off + 4 * i, v[i]);
                    ranges.Add((off, off + 16));
                }
                byte[] outp = b.ToBytes();
                var b0 = new UxBxml(data); var b2 = new UxBxml(outp);
                bool bad = !SameTables(b0, b2) || b2.pool.Length != b0.pool.Length;
                for (int i = 0; !bad && i < b0.pool.Length; i++)
                    if (b0.pool[i] != b2.pool[i])
                    {
                        bool inside = false; foreach (var (lo, hi) in ranges) if (lo <= i && i < hi) { inside = true; break; }
                        if (!inside) bad = true;
                    }
                if (bad) throw new UxException(key.Item1 + " parameter patch changed more than the value bytes");
                info.Set("hf_material", rec);
                if (o.DirtTint == "const")
                {
                    var t = Obj(values.Get("usr_dirt_tint_color"));
                    info.Set("dirt_tint", new JObj().Set("source", "const DIRT_TINT_CONST (== HuCkiT bytes)").Set("old", t.Get("old")).Set("new", t.Get("new")));
                }
                return UxBin.Same(outp, data) ? null : outp;
            }

            /// dirt_look_maps: dirt-look stock (DESIGN 23.10): UxDirtLook.StockLook on the terrain stage outputs, the build's tdf and the
            /// base HF material's own four detail textures (their mean colours for the TopDown). Computed once (WaterGround may ask
            /// first). The reference also saves previews (assembly/dirtlook_*.png): not ported.
            UxDirtLook.Maps DirtLookMaps()
            {
                if (dirtLookCache != null) return dirtLookCache;
                var key = (Role("Custom_HFTRN"), "material");
                var (shader, texs, pars) = UxDirtLook.MaterialTextures(TA.Logical(ts.Get(key.Item1, key.Item2)));
                if (texs.Count < 7) throw new UxException(key.Item1 + " has " + I(texs.Count) + " samplers (dirt-look stock needs the 4 detail layers + mask + ChromLum)");
                foreach (var (i, r) in new[] { (1, "Custom_Base"), (2, "Custom_Dirt_R"), (3, "Custom_Dirt_G"), (4, "Custom_Dirt_B"), (5, "Custom_DetailMasks"), (6, "Custom_CHR_LUM") })
                    if (texs[i] != Role(r)) throw new UxException("HF material slot " + I(i) + " is " + texs[i] + ", role " + r + " is " + Role(r));
                var layers = new List<byte[]>();
                for (int i = 1; i < 5; i++) layers.Add(TA.Logical(ts.Get(texs[i], "texture")));
                double[] means = UxDirtLook.LayerMeans(layers);
                string tdir = dirs["terrain"];
                var tj = sources != null ? sources.terrain : ctx.work.ReadJsonObj(tdir + "/terrain.json");
                var m = UxDirtLook.StockLook(ctx.work, tdir, tj, tdfBytes, means, (UxCommon.HF_SIZE - 1) * 1, log, o.threads, cancel);
                pars.Sort((a, b) => string.CompareOrdinal(a.Key, b.Key));
                var pj = new JObj(); foreach (var kv in pars) pj.Set(kv.Key, Nums(kv.Value));
                m.rec.Set("material", new JObj().Set("name", key.Item1).Set("shader", shader).Set("detail_textures", Strs(texs.GetRange(1, 4))).Set("params", pj));
                m.rec.Set("tool_md5", UxDirtLook.TOOL_MD5);
                m.rec.Set("maps_md5", new JObj().Set("mask", UxBin.Md5(m.mask)).Set("chromlum", UxBin.Md5(m.chromlum)).Set("topdown", UxBin.Md5(m.topdown)));
                info.Set("dirt_look", m.rec);
                return dirtLookCache = m;
            }
            UxDirtLook.Maps dirtLookCache;

            UxOrdered<(string name, string type), byte[]> TerrainReplacements()
            {
                string tdir = dirs["terrain"]; var tj = sources.terrain;
                if (hfN != UxCommon.HF_SIZE) throw new UxException("hf.npy shape (" + hfN + ", " + hfN + ")");
                double mn = double.PositiveInfinity, mx = double.NegativeInfinity;
                foreach (double v in hf) { if (!UxNp.Finite(v)) throw new UxException("hf.npy non-finite"); if (v < mn) mn = v; if (v > mx) mx = v; }
                if (mn < UxCommon.Limits.HfMinFt || mx > UxCommon.Limits.HfMaxFt) throw new UxException("HF heights " + UxImaging.PyFixed(mn, 2) + ".." + UxImaging.PyFixed(mx, 2) + " outside 0..2047 ft");
                var rep = new UxOrdered<(string name, string type), byte[]>();
                var disp = new float[hf.Length];
                for (int i = 0; i < disp.Length; i++) disp[i] = (float)(hf[i] / UxCommon.HEIGHT_SCALE);
                rep[(Role("Custom_DISP"), "texture")] = TexR32F(disp, hfN, hfN);
                string td = UxJson.Get(tj, "ground", "default_topdown") as string ?? "topdown_occ.png";
                var filesRec = new JObj();
                var tf = tj.Get("files") as JObj;
                var dl = o.dirtLook == "stock" ? DirtLookMaps() : null;
                foreach (var (role, file, size, dxt5) in TERRAIN_TEXTURES)
                {
                    if (dl != null)
                    {   // dirt-look stock (DESIGN 23.10): the base's own four detail textures stay (stock bytes); TopDown and DetailMasks
                        // are generated by UxDirtLook (the ChromLum below)
                        if (role == "Custom_Base" || role == "Custom_Dirt_R" || role == "Custom_Dirt_G" || role == "Custom_Dirt_B")
                        {
                            filesRec.Set(role, new JObj().Set("asset", Role(role)).Set("kept", "base texture (dirt-look stock)"));
                            continue;
                        }
                        byte[] arr = role == "Custom_TopDown" ? dl.topdown : dl.mask;
                        rep[(Role(role), "texture")] = UxBase.TexBc(new UxImage(arr, G_DL, G_DL, role == "Custom_TopDown" ? 3 : 4), dxt5, true);
                        filesRec.Set(role, new JObj().Set("generated", "ux_dirtlook stock").Set("md5", UxBin.Md5(arr)).Set("size", (long)G_DL).Set("format", dxt5 ? "DXT5" : "DXT1")
                            .Set("asset", Role(role)));
                        continue;
                    }
                    string fn = file ?? td, rel = tdir + "/" + fn;
                    byte[] png = ctx.work.ReadBytes(rel); string md5 = UxBin.Md5(png);
                    if (tf?.Get(fn) is string wantMd5 && wantMd5.Length > 0 && md5 != wantMd5) throw new UxException(paths.Work(lv, rel) + " md5 differs from terrain.json (stale)");
                    var im = UxImaging.ReadPngRgb(png);
                    if (im.w != size || im.h != size) im = UxPil.Resize(im, size, size, UxPil.Filter.Lanczos);
                    if (dxt5)
                    {   // Custom_DetailMasks: alpha 0; the others: convert('RGBA')
                        byte alpha = role == "Custom_DetailMasks" ? (byte)0 : (byte)255;
                        var px = new byte[im.w * im.h * 4];
                        for (int i = 0, j = 0; i < im.data.Length; i += 3, j += 4) { px[j] = im.data[i]; px[j + 1] = im.data[i + 1]; px[j + 2] = im.data[i + 2]; px[j + 3] = alpha; }
                        im = new UxImage(px, im.h, im.w, 4);
                    }
                    rep[(Role(role), "texture")] = UxBase.TexBc(im, dxt5);
                    filesRec.Set(role, new JObj().Set("file", paths.Work(lv, rel)).Set("md5", md5).Set("size", (long)size).Set("format", dxt5 ? "DXT5" : "DXT1").Set("asset", Role(role)));
                }
                if (o.baseLayout == "neutral")
                    foreach (var (role, rgba) in new[] { ("Custom_DEF", new[] { SOFTNESS_CONST_RGB[0], SOFTNESS_CONST_RGB[1], SOFTNESS_CONST_RGB[2], 255 }), ("Custom_CHR_LUM", CHROMLUM_CONST_RGBA) })
                    {
                        string nm = Role(role);
                        if (role == "Custom_CHR_LUM" && dl != null) continue;                 // dirt-look stock: generated below
                        byte[] stock = TA.Logical(ts.Get(nm, "texture"));
                        var old = new UxPcTexture(stock);
                        var (_, w, h, fmt) = old.recs[0];
                        bool d5 = UxTex.Fmt(fmt, UxTex.PC_DXT5);
                        if (!d5 && !UxTex.Fmt(fmt, UxTex.PC_DXT1)) throw new UxException(nm + " is not DXT1/DXT5");
                        rep[(nm, "texture")] = UxBase.ConstantTexture(rgba, w, h, d5);
                        var cst = new List<object>(); for (int i = 0; i < (d5 ? 4 : 3); i++) cst.Add((long)rgba[i]);
                        filesRec.Set(role, new JObj().Set("asset", nm).Set("constant", cst).Set("size", new List<object> { (long)w, (long)h }).Set("format", d5 ? "DXT5" : "DXT1")
                            .Set("stock_md5", UxBin.Md5(stock)));
                    }
                if (dl != null)
                {
                    string nm = Role("Custom_CHR_LUM");
                    rep[(nm, "texture")] = UxBase.TexBc(new UxImage(dl.chromlum, G_DL, G_DL, 4), true, true);
                    filesRec.Set("Custom_CHR_LUM", new JObj().Set("asset", nm).Set("generated", "ux_dirtlook stock").Set("md5", UxBin.Md5(dl.chromlum)).Set("size", (long)G_DL)
                        .Set("format", "DXT5").Set("stock_md5", UxBin.Md5(TA.Logical(ts.Get(nm, "texture")))));
                }
                foreach (var kv in rep)
                {
                    var pr = UxTextures.PcLayoutProblems(kv.Value);
                    if (pr.Count > 0) throw new UxException(kv.Key.name + ": layout " + string.Join("; ", pr));
                    if (!TA.Has(kv.Key.name, "texture")) throw new UxException("template has no " + kv.Key.name + " texture to replace");
                }
                if (dl != null) { dl.mask = null; dl.chromlum = null; dl.topdown = null; }      // the maps are in the textures now (the record stays)
                var tt = new JObj().Set("Custom_DISP", new JObj().Set("file", paths.Work(lv, tdir + "/hf.npy")).Set("md5", UxBin.Md5(ctx.work.ReadBytes(tdir + "/hf.npy"))).Set("min_ft", mn)
                    .Set("max_ft", mx).Set("size", (long)UxCommon.HF_SIZE).Set("grid_ft", 1L).Set("value", "ft / 2048").Set("mips", "point"));
                foreach (var kv in filesRec.items) tt.Set(kv.Key, kv.Value);
                info.Set("terrain_textures", tt);
                return rep;
            }

            /// base_prune: the base package assets nothing in the written scene needs -> the removal list (UxBase.Closure).
            List<(string name, string type)> BasePrune(UxOrdered<(string name, string type), Item> data, UxOrdered<(string name, string type), byte[]> replacements)
            {
                var sw = Stopwatch.StartNew();
                var by = new Dictionary<(string, string), UxPcAsset>(); var keys = new List<(string name, string type)>();
                foreach (var a in ts.assets) { if (!by.ContainsKey((a.name, a.type))) keys.Add((a.name, a.type)); by[(a.name, a.type)] = a; }
                var all = new List<(string name, string type)>(keys); all.AddRange(data.Keys);
                var index = UxBase.IndexOf(all);
                byte[] Read((string name, string type) k)
                {
                    if (data.TryGet(k, out var it)) return it.data;
                    if (replacements.TryGet(k, out var r)) return r;
                    return TA.Logical(by[k]);
                }
                var entsAll = new List<UxElem>(); foreach (var s in subs) entsAll.AddRange(s.Entities());
                var roots = new List<(string name, string type)>();
                if (!fmx) roots.Add((tslot + "-sx", "tdf"));
                if (o.baseScripts != "none")
                {
                    foreach (var kv in scriptRen) roots.Add(kv.Key);
                    foreach (string n in @base.policy.implicit_sounds ?? new string[0]) roots.Add((n, "sound"));
                }
                foreach (var r in @base.policy.implicit_roots ?? new (string, string)[0]) if (!roots.Contains(r)) roots.Add(r);
                var types = new HashSet<string>(StringComparer.Ordinal); foreach (var e in entsAll) types.Add(UxPcScene.Ns(e));
                var lit = UxBase.LiteralRoots(types);
                foreach (var k in lit) if (!roots.Contains(k)) roots.Add(k);
                var AS = sh.AnimSets(paths);
                var setRoots = new List<string>(); if (!fmx) foreach (var (set, type) in UxBase.ENGINE_ANIM_SETS) if (type == null || types.Contains(type)) setRoots.Add(set);
                var c = UxBase.Closure(entsAll, roots, index, Read, sh.Globals.Keys, AS.sets, setRoots, sh.SessionAnims);
                var outp = new List<(string name, string type)>();
                var kc = new SortedDictionary<string, long[]>(StringComparer.Ordinal); var rc = new SortedDictionary<string, long[]>(StringComparer.Ordinal);
                long kept = 0, keptStored = 0, removedStored = 0, animsKept = 0;
                foreach (var k in keys)
                {
                    var a = by[k]; string pt = a.PackageName + "/" + k.type;
                    bool gone = !c.kept.Has(k) && !replacements.Has(k);
                    var d = gone ? rc : kc;
                    if (!d.TryGetValue(pt, out var v)) d[pt] = v = new long[2];
                    v[0]++; v[1] += a.Size;
                    if (gone) { outp.Add(k); removedStored += a.Size; } else { kept++; keptStored += a.Size; }
                }
                foreach (var k in c.kept.Keys) if (k.type == "anim" && by.ContainsKey(k)) animsKept++;
                JObj PT(SortedDictionary<string, long[]> d) { var j = new JObj(); foreach (var kv in d) j.Set(kv.Key, new List<object> { kv.Value[0], kv.Value[1] }); return j; }
                var unres = new List<object>(); foreach (var u in c.unresolved) unres.Add(u.name + "." + u.type + " <- " + u.why);
                var globs = new List<string>(); foreach (var g in c.glob) globs.Add(K(g)); globs.Sort(string.CompareOrdinal);
                var expanded = new SortedDictionary<string, List<string>>(StringComparer.Ordinal); foreach (var kv in c.sets) expanded[kv.Key] = kv.Value;
                var expJ = new JObj(); foreach (var kv in expanded) expJ.Set(kv.Key, Strs(kv.Value));
                var nowhere = new List<object>(); foreach (var (member, set) in c.nowhere) nowhere.Add(member + " (set " + set + ")");
                var rootsJ = new List<object>(); foreach (var r in roots) rootsJ.Add(K(r));
                var litJ = new List<object>(); foreach (var r in lit) litJ.Add(K(r));
                info.Set("base_prune", new JObj().Set("rule", CLOSURE_RULE).Set("roots", rootsJ).Set("literal_roots", litJ).Set("base_assets", (long)keys.Count).Set("kept", kept)
                    .Set("removed", (long)outp.Count).Set("kept_stored", keptStored).Set("removed_stored", removedStored).Set("kept_by_package_type", PT(kc))
                    .Set("removed_by_package_type", PT(rc)).Set("unresolved", unres).Set("global_refs", Strs(globs))
                    .Set("animation_sets", new JObj().Set("file", AS.file).Set("md5", AS.md5).Set("engine_sets", Strs(setRoots)).Set("expanded", expJ).Set("members_nowhere", nowhere)
                        .Set("anims_kept", animsKept))
                    .Set("seconds", UxNp.PyRound(sw.Elapsed.TotalSeconds, 1)));
                if (c.unresolved.Count > 0)
                {
                    var six = new List<string>(); foreach (var u in c.unresolved) { if (six.Count == 6) break; six.Add(u.name + "." + u.type); }
                    warnings.Add("base closure: " + c.unresolved.Count + " references resolve nowhere (stock base own): " + PyList(six));
                }
                if (c.nowhere.Count > 0) warnings.Add("base closure: " + c.nowhere.Count + " animation set members in no package or global database");
                return outp;
            }
            const string CLOSURE_RULE = "the assets a set of entities needs, by a name walk (entity Asset elements and string props -> material ->\ntexture / shader, model -> material, bxml -> any type, " +
                "particle / partattr / adv, sound -> soundbnk of the same name\nor <name>_bank00 and the names inside the .sound, anim X -> X_BIKE / X_ATV ...); see ux_base.py";

            // ------------------------------------------------------------------ write
            void Write(string outFolder, List<UxElem> conv, UxOrdered<(string name, string type), Item> data, UxOrdered<(string name, string type), byte[]> replacements)
            {
                var bs = Sub("Base");
                var ver = UxEntities.ChunkVersion(sh.Prototypes);
                int max = UxCommon.Limits.MaxEntitiesPerChunk;
                for (int i = 0; i < conv.Count; i += max)
                {
                    bs.chunks.Add(new UxElem("SceneChunk", null, 0, 0, null, conv.GetRange(i, Math.Min(max, conv.Count - i))));
                    bs.versions.Add(ver);
                }
                if (frExtra.Count > 0)
                {
                    var fr = Sub("FreeRide");
                    for (int i = 0; i < frExtra.Count; i += max)
                    {
                        fr.chunks.Add(new UxElem("SceneChunk", null, 0, 0, null, frExtra.GetRange(i, Math.Min(max, frExtra.Count - i))));
                        fr.versions.Add(ver);
                    }
                }
                // P0 (DESIGN 18): every RSData namespace in the core's exact spelling
                var chunks = new List<UxElem>(); foreach (var s in subs) chunks.AddRange(s.chunks);
                var nsc = UxCommon.CanonNamespaces(chunks);
                foreach (var c in chunks)
                    foreach (var x in c.Iter())
                        foreach (var a in x.attrs)
                            if (a.name == "namespace" && a.type == 1 && a.str != null && UxCommon.CanonNamespace(a.str) != a.str) throw new UxException("namespaces still off the core spelling: " + a.str);
                var changedNs = new SortedDictionary<string, long>(StringComparer.Ordinal); foreach (var (oldNs, canon, count) in nsc) changedNs[oldNs + " -> " + canon] = count;
                var changedJ = new JObj(); foreach (var kv in changedNs) changedJ.Set(kv.Key, kv.Value);
                var et = new SortedDictionary<string, long>(StringComparer.Ordinal);
                foreach (var s in subs) foreach (var e in s.Entities()) { string t = UxPcScene.Ns(e); et[t] = (et.TryGetValue(t, out long n) ? n : 0) + 1; }
                var etJ = new JObj(); foreach (var kv in et) etJ.Set(kv.Key, kv.Value);
                info.Set("namespaces", new JObj().Set("table", "ux_common.RSDATA_CORE_TYPES (Open Reflex core/game/build_scene.c g_types)").Set("changed", changedJ).Set("entity_types", etJ));
                // orphans (DESIGN 18): the stock base holds none of the candidates
                foreach (var k in ORPHAN_CANDIDATES) if (TA.Has(k)) throw new UxException("a base with " + K(k) + " is not ported (orphan drop)");
                info.Set("orphans", new JObj().Set("option", "drop").Set("removed", new List<object>()).Set("kept", new JObj()).Set("scanned", null));
                // tools-stockbase: scenario script names + package = what the scene needs
                scriptRen = o.baseScripts != "none" ? UxBase.ScriptRenames(@base, ts, slot, UxBase.ScriptKinds(@base, o.baseScripts)) : new UxOrdered<(string, string), string>();
                var removals = BasePrune(data, replacements);
                var dbRename = new Dictionary<string, string>(StringComparer.Ordinal);
                var renMap = new Dictionary<string, string>(StringComparer.Ordinal);
                foreach (var kv in rename) { dbRename[kv.Key] = kv.Value; renMap[kv.Key] = kv.Value; }
                var renamedJ = new JObj();
                foreach (var kv in scriptRen) { dbRename[kv.Key.name] = kv.Value; renamedJ.Set(K(kv.Key), kv.Value); }
                info.Set("base_scripts", new JObj().Set("option", o.baseScripts).Set("kinds", Strs(UxBase.ScriptKinds(@base, o.baseScripts))).Set("renamed", renamedJ)
                    .Set("track_part", UxBase.IntroTrackPart(slot)).Set("rule", "core anim_scenario_intro.c:854-938 (<series>_<track>_<kind>; unlisted level: 5 chars after the first _)"));
                int nrenScene = RenameScene(subs, renMap);
                // ---- the files (build form: attr flags 0), then the release form (UxPackage)
                Directory.CreateDirectory(outFolder);
                foreach (string f in Directory.GetFiles(outFolder, "*.dx9.*"))
                    if (!Path.GetFileName(f).StartsWith(slot + ".", StringComparison.Ordinal)) throw new UxException("output folder already holds another slot set: " + Path.GetFileName(f));
                var adds = new List<(string name, string type, byte[] data)>(); foreach (var kv in data) adds.Add((kv.Key.name, kv.Key.type, kv.Value.data));
                var repl = new Dictionary<(string, string), byte[]>(); foreach (var kv in replacements) repl[kv.Key] = kv.Value;
                string pkgPath = Path.Combine(outFolder, UxCommon.SlotFileName(slot, "package")), tmp = pkgPath + ".tmp";
                byte[] dbBytes; int nrenDb; long logical; int nAssets;
                using (var fs = new FileStream(tmp, FileMode.Create, FileAccess.Write, FileShare.None, 1 << 20))
                    (dbBytes, nrenDb, logical, nAssets) = RebuildDatabase(ts, TA, repl, adds, tslot + "Base", dbRename, removals, fs, o.packing, o.threads, cancel);
                if (File.Exists(pkgPath)) File.Delete(pkgPath);
                File.Move(tmp, pkgPath);
                var before = new List<string>(levelBxml.strings);
                levelBxml.ReplaceStrings(renMap);
                int nrenLv = 0; for (int i = 0; i < before.Count; i++) if (before[i] != levelBxml.strings[i]) nrenLv++;
                byte[] lvBytes = levelBxml.ToBytes();
                byte[] sceneBytes = UxPcScene.Save(subs);
                var map = new JObj(); foreach (var kv in rename) map.Set(kv.Key, kv.Value);
                info.Set("rename", new JObj().Set("map", map).Set("changed", new JObj().Set("level", (long)nrenLv).Set("database", (long)nrenDb).Set("scene", (long)nrenScene)));
                result.logicalBytes = logical; result.assets = nAssets; result.packageBytes = new FileInfo(pkgPath).Length;
                // ---- ux_package: stock attr flags, mod.ini, preview.png, sha256.txt
                Step(0.97f, "packaging");
                var sw = Stopwatch.StartNew();
                byte[] topdown = ctx.work.ReadBytes(dirs["terrain"] + "/" + (UxJson.Get(sources.terrain, "ground", "default_topdown") as string ?? "topdown_occ.png"));
                var files = UxPackage.Write(outFolder, slot, lv, dbBytes, lvBytes, sceneBytes, topdown, o.author, o.date, o.check, fmx ? "freeride" : "supercross");
                result.steps.Add(new KeyValuePair<string, double>("package", sw.Elapsed.TotalSeconds));
                result.folder = outFolder; result.slot = slot;
                var md5 = new JObj(); var bytes = new JObj();
                foreach (var f in files) result.files.Add(f);
                foreach (string k in UxCommon.SlotFileKinds)
                {
                    string p = Path.Combine(outFolder, UxCommon.SlotFileName(slot, k));
                    md5.Set(k, UxBin.Md5File(p)); bytes.Set(k, new FileInfo(p).Length);
                }
                info.Set("output", new JObj().Set("slot", slot).Set("form", "release (stock attr flags)").Set("md5", md5).Set("bytes", bytes).Set("packing", o.packing)
                    .Set("logical_bytes", logical).Set("assets", (long)nAssets));
            }

            // ------------------------------------------------------------------ build_info
            JObj BuildInfo()
            {
                var bi = new JObj().Set("schema", SCHEMA).Set("tool", TOOL).Set("port", PORT);
                bi.Set("level", lv.disc).Set("short", lv.shortName).Set("kind", "sx").Set("slot", (long)o.slot).Set("slot_name", slot).Set("variant", "");
                bi.Set("template", @base.Folder(paths)).Set("base", @base.Record(paths)).Set("y_lift", UxCommon.YLift(lv));
                bi.Set("options", o.Record(lv));
                var rn = new JObj();
                var S = sources;
                if (S.texture_renames != null && S.texture_renames.Count > 0) { var t = new JObj(); foreach (var kv in S.texture_renames) t.Set(kv.Key, kv.Value); rn.Set("texture", t); }
                foreach (var kv in renames)
                {
                    if (!(rn.Get(kv.Key.type) is JObj t)) rn.Set(kv.Key.type, t = new JObj());
                    t.Set(kv.Key.name, kv.Value);
                }
                var matRen = new JObj();
                foreach (var kv in S.material_entries) if (kv.Value.Get("asset") is string a && a.Length > 0 && a != kv.Key) matRen.Set(kv.Key, a);
                if (matRen.items.Count > 0)
                {
                    if (!(rn.Get("material") is JObj t)) rn.Set("material", t = new JObj());
                    foreach (var kv in matRen.items) t.Set(kv.Key, kv.Value);
                }
                bi.Set("renames", rn);
                var sd = new JObj(); foreach (var kv in dirs) sd.Set(kv.Key, paths.Work(lv, kv.Value.Length == 0 ? null : kv.Value));
                bi.Set("stage_dirs", sd).Set("stages", stageRecord);
                var sm = new JObj();
                foreach (var (k, rel) in new[] { ("level", "level.json"), ("level_fr", "level_fr.json"), ("meshes", dirs["meshes"] + "/meshes.json"), ("terrain", dirs["terrain"] + "/terrain.json"),
                    ("hf", dirs["terrain"] + "/hf.npy"), ("collision", dirs["collision"] + "/collision.json"), ("materials", dirs["materials"] + "/materials.json"),
                    ("textures", dirs["textures"] + "/textures.json") })
                    sm.Set(k, new JObj().Set("path", paths.Work(lv, rel)).Set("md5", ctx.work.Exists(rel) ? UxBin.Md5(ctx.work.ReadBytes(rel)) : null));
                bi.Set("source_md5s", sm);
                var counts = Obj(edoc.Get("counts")) ?? new JObj();
                long nBase = 0; foreach (object s in UxJson.Arr(edoc.Get("entities"))) if (Obj(s)?.Get("subscene") as string == "Base") nBase++;
                bi.Set("counts", new JObj().Set("entities", new JObj().Set("converted", nBase).Set("by_type", counts.Get("by_type")).Set("by_role", counts.Get("by_role"))
                    .Set("removed_by_options", counts.Get("removed_by_options"))).Set("assets", assetRecord.Get("added")).Set("asset_bytes", assetRecord.Get("added_bytes")));
                var warn = edoc.Get("warnings") as List<object> ?? new List<object>();
                bi.Set("entities", new JObj().Set("options", edoc.Get("options")).Set("distance_fog", edoc.Get("distance_fog")).Set("freeride", edoc.Get("freeride"))
                    .Set("warnings", warn.Count > 50 ? warn.GetRange(0, 50) : warn));
                var tdf = Obj(race?.Get("tdf")) ?? new JObj(); var gate = Obj(race?.Get("gate")) ?? new JObj();
                if (race != null)
                {
                    var tdfRec = new JObj(); foreach (string k in new[] { "asset", "mode", "sx_flags", "source", "source_md5" }) if (tdf.Has(k)) tdfRec.Set(k, tdf.Get(k));
                    var gateRec = new JObj();
                    foreach (string k in new[] { "gatestart_on_chute_dirt", "gatestart_in_corridor", "lap_crosses_gatestart_line", "template_gatestart_on_chute_dirt", "template_gatestart_in_corridor",
                        "node_patch", "startgate_collision", "startgate_collision_trim", "gate_patch_option", "dropgate_materials", "startgate_visual" }) gateRec.Set(k, gate.Get(k));
                    bi.Set("race", new JObj().Set("tdf", tdfRec).Set("tdf_facts", tdf.Get("facts")).Set("gate", gateRec).Set("y_lift", race.Get("y_lift")).Set("checks", race.Get("checks"))
                        .Set("warnings", race.Get("warnings")));
                }
                var assets = new JObj(); foreach (var kv in assetRecord.items) if (kv.Key != "list") assets.Set(kv.Key, kv.Value);
                bi.Set("assets", assets);
                bi.Set("assembly", info);
                bi.Set("zones", info.Get("zones"));
                // ---- DESIGN 18 rollout record
                var zr = new JObj(); foreach (string k in new[] { "mode", "replaced", "cells_changed" }) zr.Set(k, Obj(info.Get("zones"))?.Get(k));
                var lp = Obj(info.Get("lappacing"));
                var dn = Obj(tdf.Get("deck_norespawn"));
                JObj dnRec = null;
                if (dn != null)
                {
                    dnRec = new JObj();
                    foreach (var kv in dn.items) if (Array.IndexOf(new[] { "option", "threshold_ft", "changed", "md5_before", "md5_after", "mesh_tris", "engine" }, kv.Key) >= 0) dnRec.Set(kv.Key, kv.Value);
                    if (dnRec.items.Count == 0) dnRec = null;
                }
                var dnNodes = new List<object>();
                if (dn?.Get("nodes") is List<object> nl) foreach (object r in nl) if (r is JObj rj && UxEntities.Py.Truthy(rj.Get("changed"))) dnNodes.Add("S" + I(UxJson.Int(rj.Get("section"))) + " n" + I(UxJson.Int(rj.Get("node"))));
                var gen = new List<string>();
                if (S.materials?.Get("generated_textures") is JObj gt) foreach (var kv in gt.items) if (kv.Value is JObj gv && UxEntities.Py.Truthy(gv.Get("recipe"))) gen.Add(kv.Key);
                gen.Sort(string.CompareOrdinal);
                var orph = new List<object>();
                var ro = new JObj().Set("namespaces", Obj(info.Get("namespaces"))?.Get("changed")).Set("zones", zr).Set("cameraflash", Obj(info.Get("cameraflash"))?.Get("added"))
                    .Set("orphans_removed", orph).Set("lappacing", lp != null && UxEntities.Py.Truthy(lp.Get("included")) ? lp.Get("asset") : null).Set("lappacing_option", o.lappacing)
                    .Set("entities", counts.Get("rollout")).Set("entities_removed", counts.Get("removed_by_options"))
                    .Set("entities_not_in_base", edoc.Get("rollout_removed") ?? new List<object>()).Set("entities_moved", edoc.Get("rollout_moved") ?? new List<object>())
                    .Set("deck_norespawn", dnRec).Set("deck_norespawn_nodes", dnNodes).Set("fr_clearance", o.frClearance).Set("buried", o.buried).Set("occ_map", o.occMap)
                    .Set("mipfilter", o.mipfilter).Set("composite", o.composite).Set("literal_albedo", o.literalAlbedo).Set("alpha_source", o.alphaSource).Set("diffuse_pick", o.diffusePick)
                    .Set("pop_clip", o.popClip).Set("rigid_models", o.rigidModels).Set("generated_textures", Strs(gen)).Set("tdf_line_padding", tdf.Get("line_padding"))
                    .Set("legacy_defaults", false);
                var dgm = Obj(gate.Get("dropgate_materials"));
                ro.Set("dropgate_materials", new JObj().Set("option", o.dropgateMaterials).Set("written", dgm?.Get("written")).Set("superseded_by", dgm?.Get("superseded_by")));
                ro.Set("startgate_visual", o.startgateVisual);
                var sg = new JObj().Set("option", o.splitGate);
                if (splitgate != null)
                    foreach (string k in new[] { "entity_ids", "untamed_dropgate", "untamed_startinggate", "dropgate_model", "base_model", "dropgate_props", "base_entity", "materials",
                        "materials_manifest", "assets", "assets_in_package", "tool_md5", "level_json_md5" }) sg.Set(k, splitgate.Get(k));
                ro.Set("split_gate", sg);
                ro.Set("base_option", "stock");
                ro.Set("tuff", combined.Get("tuff") ?? new JObj().Set("option", o.bales));
                ro.Set("ec_water", combined.Get("ec_water") ?? new JObj().Set("option", o.ecWater));
                ro.Set("screens", combined.Get("screens") ?? new JObj().Set("option", o.screens));
                ro.Set("massive_ads", combined.Get("massive_ads") ?? new JObj().Set("option", o.massiveAds));
                ro.Set("combined_assets", info.Get("combined_assets"));
                if (fmx)                                                                  // FREESTYLE.md record
                    ro.Set("freestyle", new JObj().Set("slot", slot).Set("kind", "freeride (Beta_Fmx: Open Reflex FREE RIDE only)").Set("base_kit", "freeride")
                        .Set("race_kit", null).Set("ramps", freestyle));
                var dlr = new JObj().Set("option", o.dirtLook);                           // dirt-look round (DESIGN 23.10)
                if (info.Get("dirt_look") is JObj dlj) foreach (var kv in dlj.items) dlr.Set(kv.Key, kv.Value);
                ro.Set("dirt_look", dlr);
                if (o.baleEdgeClear != 0 || o.baleReseat != 0)
                {
                    var bec = Obj(tdf.Get("bale_edge_clear")) ?? new JObj();
                    var bales = new List<object>(); if (bec.Get("bales") is List<object> bl) foreach (object x in bl) bales.Add(Obj(x).Get("id"));
                    var nodes = new List<object>();
                    if (bec.Get("nodes") is List<object> nn)
                        foreach (object x in nn)
                        {
                            var n = Obj(x);
                            nodes.Add("S" + I(UxJson.Int(n.Get("section"))) + " n" + I(UxJson.Int(n.Get("node"))) + " e" + I(UxJson.Int(n.Get("side"))) + " -" + UxImaging.PyFixed(UxJson.Num(n.Get("trim_ft")), 2) + " ft");
                        }
                    ro.Set("bale_edge_clear", new JObj().Set("option", o.baleEdgeClear).Set("bales", bales).Set("nodes", nodes).Set("min_clearance_ft", bec.Get("min_clearance_ft"))
                        .Set("md5_before", bec.Get("md5_before")).Set("md5_after", bec.Get("md5_after")).Set("problems", bec.Get("problems") ?? new List<object>()));
                    var reseated = new List<object>();
                    if (edoc.Get("rollout_moved") is List<object> rm) foreach (object x in rm) if (Obj(x)?.Get("option") as string == "bale-reseat") reseated.Add(Obj(x).Get("untamed_id"));
                    ro.Set("bale_reseat", new JObj().Set("option", o.baleReseat).Set("bales", reseated));
                }
                bi.Set("rollout", ro);
                bi.Set("problems", problems).Set("warnings", warnings);
                return bi;
            }
        }
    }
}

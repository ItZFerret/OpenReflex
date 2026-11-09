// One Alive level from the player's source into the client's content folder: the conversion of _re/formats/alive_convert.run (every
// package resource through Converters.ConvertAsset, unconverted ones reported as pending) and the installer of alive_install.install_level
// (the database filtered to the installed resources, the Free Ride scene, the .level descriptor, the base Reflex programs the level uses,
// the per-level sidecars, the height range, the registry entry). Everything is written into a staging folder inside the content folder and
// published as new real files (no links); an existing file is never replaced (the registry, the importer's own index, is rewritten).
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using System.Text;
using System.Threading;

namespace Reflex.Port.Content.AliveImport
{
    /// A level found in a source: its four files <Level>.xenon.{level,database,package,scene} in one directory of a volume.
    public sealed class AliveLevelSource
    {
        public string level, displayName, pack, packDisplay, kind;
        public string[] activities = Array.Empty<string>();
        public Volume volume; public string directory;
        /// The files match the original release (known names and sizes, database SHA-256); only such levels are imported.
        public bool recognized; public string problem;
        public long bytes;
        public string Label => string.IsNullOrEmpty(displayName) ? level : displayName;
        internal VolumeFile File(string suffix) => volume.Find((directory.Length > 0 ? directory + "/" : "") + level + ".xenon." + suffix);
        internal VolumeFile Sibling(string name) => volume.Find((directory.Length > 0 ? directory + "/" : "") + name);
        /// The level's own menu pictures in the sources (AlivePictures: "preview", "map"), or null.
        internal Dictionary<string, AlivePictureRef> pictures;
    }

    public sealed class AliveProgress
    {
        public string level, stage; public int done, total;
        public double Fraction => total > 0 ? Math.Min(1.0, (double)done / total) : 0.0;
    }

    public sealed class AliveLevelResult
    {
        public string level, displayName, error;
        public bool ok;
        public int converted, pending, resources, deferredEntities, starts, files;
        public long bytes; public double seconds;
        public override string ToString() => ok ? $"{displayName}: {resources} resources, {starts} Free Ride starts, {pending} pending" : $"{displayName}: {error}";
    }

    internal sealed class ConvertedAsset { public string package, name, type, path, method; public long bytes; }

    /// alive_convert.run's report and outputs (written under `stage`).
    internal sealed class Conversion
    {
        public string level; public byte[] database, scene, levelBytes;
        public readonly List<ConvertedAsset> converted = new List<ConvertedAsset>();
        public readonly List<(string package, string name, string type, string reason)> pending = new List<(string, string, string, string)>();
        public readonly List<KeyValuePair<string, double?>> rockTiles = new List<KeyValuePair<string, double?>>();
        public bool hasRockTiles;
        public List<KeyValuePair<string, object>> skyHdr;
        public readonly Dictionary<string, JObj> albedoPrograms = new Dictionary<string, JObj>(StringComparer.Ordinal);
        /// The level's menu pictures (AlivePictures.Of: kind, PC texture v4 bytes).
        public List<(string kind, byte[] data)> pictures = new List<(string, byte[])>();
        public string stage;
        /// PORT ADDITION (AliveRace): the race activity (report["race"]); null for a Free Ride-only level. raceTdf: its track definition's
        /// resource name; racePacingPath: the converted lap pacing (stage-relative), null when the level has none.
        public string raceActivity, raceTdf, raceResource, racePacingPath;
        public long raceMaxPlayers;
        internal readonly List<(string name, byte[] data)> racePacing = new List<(string, byte[])>();
    }

    internal static class LevelInstaller
    {
        static readonly HashSet<string> Supported = new HashSet<string> { "material", "texture", "model", "surface", "collis", "packdata", "bxml", "partattr", "forest", "tree",
                                                                         "grass", "water", "particle", "shader", "anim" };
        static readonly HashSet<string> BasePrograms = new HashSet<string> { "water_wake" };
        public const string Registry = "alive-levels.json";

        // ------------------------------------------------------------------------------------- conversion

        public static Conversion Convert(AliveLevelSource src, string stage, Action<string, int, int> progress, CancellationToken cancel)
        {
            var c = new Conversion { level = src.level, stage = stage };
            string level = src.level;
            var dbFile = src.File("database"); Bin.Require(dbFile != null, level + ".xenon.database not found");
            c.database = src.volume.Read(dbFile, cancel);
            var tree = Bxml.Parse(c.database).ToElement();
            c.scene = Scenes.ConvertScene(src.volume.Read(src.File("scene"), cancel), out _);
            c.levelBytes = src.volume.Read(src.File("level"), cancel);
            Bxml.Parse(c.levelBytes);
            var materials = new HashSet<string>(tree.Iter("Asset").Where(a => a.Get("type") == "material").Select(a => a.Get("name")).Where(n => n != null));
            var packages = tree.FindAll("Packages").SelectMany(p => p.FindAll("Package")).ToList();
            int total = packages.Sum(p => p.FindAll("Asset").Count()), done = 0;
            var raws = new Dictionary<string, byte[]>(StringComparer.Ordinal);
            foreach (var package in packages)
            {
                string filename = package.Get("file");
                Converters.Segment(filename);
                if (!raws.TryGetValue(filename, out var raw))
                {
                    var pf = src.Sibling(filename + ".xenon.package");
                    Bin.Require(pf != null, filename + ".xenon.package not found");
                    raws.Clear();   // one package file in memory at a time
                    raws[filename] = raw = src.volume.Read(pf, cancel);
                }
                foreach (var asset in package.FindAll("Asset"))
                {
                    cancel.ThrowIfCancellationRequested();
                    progress?.Invoke("Converting", done++, total);
                    string name = asset.Get("name"), kind = asset.Get("type"), pkg = package.Get("name");
                    var heap = asset.Find("Heap");
                    Bin.Require(heap != null, "Asset without Heap");
                    long pos = (long)XMem.MetadataUint(package.Get("offset")) + XMem.MetadataUint(heap.Get("offset"));
                    long size = XMem.MetadataUint(heap.Get("size"));
                    Bin.Require(pos + size <= raw.Length, "Asset exceeds source package");
                    var block = asset.Find("Block");
                    Bin.Require(block != null && XMem.MetadataUint(block.Get("size")) == 0, "Unsupported block asset");
                    var data = Bin.Slice(raw, (int)pos, (int)size);
                    var comp = asset.Find("Compress");
                    if (comp != null && comp.Get("enabled") == "_bool:true")
                    {
                        Bin.Require(comp.Get("codec") == "LZX", "Unsupported compression");
                        data = XMem.Decode(data, cancel);
                    }
                    if (kind == "texture" && name.ToLowerInvariant().Contains("sky"))
                    {
                        var hdr = Converters.SkyHdrFooter(data);
                        if (hdr != null && (c.skyHdr == null || name.EndsWith("_Sat", StringComparison.Ordinal))) c.skyHdr = hdr;
                    }
                    if (kind == "activity" || (kind == "bxml" && name.EndsWith("-lappacing", StringComparison.Ordinal)))
                    {
                        // the race data (AliveRace: PORT ADDITION, Alive levels race with AI): the race activity's track definition, the lap pacing
                        if (kind == "activity")
                        {
                            var race = AliveRace.RaceActivity(data);
                            if (race.HasValue)
                            {
                                Bin.Require(c.raceActivity == null, "Two race activities");
                                c.raceActivity = race.Value.activity; c.raceTdf = race.Value.tdf; c.raceMaxPlayers = race.Value.maxPlayers; c.raceResource = name;
                            }
                        }
                        else c.racePacing.Add((name, data));
                    }
                    if (kind == "shader" && name.Contains("Rock") && name.EndsWith("_NoTemp", StringComparison.Ordinal))
                    {
                        double? tile;
                        try { tile = Converters.RockDetailTile(data); } catch (AliveImportException) { tile = null; }
                        c.hasRockTiles = true;
                        int at = c.rockTiles.FindIndex(kv => kv.Key == name);
                        if (at >= 0) c.rockTiles[at] = new KeyValuePair<string, double?>(name, tile); else c.rockTiles.Add(new KeyValuePair<string, double?>(name, tile));
                    }
                    if (kind == "shader")
                    {
                        // the albedo of a level's own pixel program as the client's generic program (alive_recipe: cliffs, outcrops,
                        // end-of-world backdrops ...)
                        JObj program = null;
                        byte[] shared = null, vertex = null, normalPass = null;
                        try
                        {
                            foreach (var p in Shaders.Programs(data))
                            {
                                if (p.name == "SharedPixelShader") shared = p.container;
                                else if (p.name == "StaticVertexShader") vertex = p.container;
                                else if (p.name == "NormalPixelShader") normalPass = p.container;
                            }
                            program = shared != null && shared.Length > 0 ? Recipes.Recipe(shared, vertex) : null;
                        }
                        catch (Exception ex) when (!(ex is OperationCanceledException)) { program = null; }
                        // a program that does not reduce to a channel-parallel albedo: compiled whole (alive_general: normal-mapped blends, cube
                        // reflections, per-channel constants)
                        if (program == null && shared != null && shared.Length > 0 && Recipes.GeneralEligible(name))
                        {
                            try { program = Recipes.General(shared, vertex, normalPass); }
                            catch (Exception ex) when (!(ex is OperationCanceledException)) { program = null; }
                        }
                        if (program != null) c.albedoPrograms[name] = program;
                    }
                    byte[] converted; string method;
                    try { (converted, method) = Converters.ConvertAsset(data, kind, materials); }
                    catch (Exception ex) when (ex is AliveImportException || ex is IndexOutOfRangeException || ex is ArgumentException || ex is InvalidDataException || ex is KeyNotFoundException)
                    {
                        c.pending.Add((pkg, name, kind, ex.Message));
                        continue;
                    }
                    string path = "Assets/" + Converters.Segment(kind) + "/" + level + "/" + Converters.Segment(pkg) + "/" + Converters.Segment(name) + "." + Converters.Segment(kind);
                    WriteNew(stage, path, converted);
                    c.converted.Add(new ConvertedAsset { package = pkg, name = name, type = kind, path = path, method = method, bytes = converted.Length });
                }
            }
            if (c.raceActivity != null) { AliveRace.ResolveTrack(c); AddLapPacing(c, src, cancel); }
            progress?.Invoke("Converting", total, total);
            // the level's own menu pictures (a picture that cannot be read leaves the level without it, as alive_install.install does)
            try { c.pictures = Pictures.Of(src, cancel); }
            catch (AliveImportException) { c.pictures.Clear(); }
            return c;
        }

        /// alive_race.add_lap_pacing: the level's own "*-lappacing" resource, else "<level>-lappacing" of AIData.xenon.database next to the level,
        /// converted to Reflex's schema into race/<level>-lappacing.bxml; none: no pacing (racePacingPath null).
        static void AddLapPacing(Conversion c, AliveLevelSource src, CancellationToken cancel)
        {
            byte[] found = null;
            if (c.racePacing.Count > 0)
            {
                Bin.Require(c.racePacing.Count == 1, "Several lap pacing resources");
                found = c.racePacing[0].data;
            }
            else
            {
                var aiFile = src.Sibling("AIData.xenon.database");
                if (aiFile != null)
                {
                    var tree = Bxml.Parse(src.volume.Read(aiFile, cancel)).ToElement();
                    foreach (var package in tree.FindAll("Packages").SelectMany(p => p.FindAll("Package")))
                        foreach (var asset in package.FindAll("Asset"))
                        {
                            if (asset.Get("type") != "bxml" || asset.Get("name") != c.level + "-lappacing") continue;
                            var pf = src.Sibling(package.Get("file") + ".xenon.package");
                            Bin.Require(pf != null, package.Get("file") + ".xenon.package not found");
                            var raw = src.volume.Read(pf, cancel);
                            var heap = asset.Find("Heap");
                            long pos = (long)XMem.MetadataUint(package.Get("offset")) + XMem.MetadataUint(heap.Get("offset"));
                            long size = XMem.MetadataUint(heap.Get("size"));
                            Bin.Require(pos + size <= raw.Length, "Asset exceeds source package");
                            var data = Bin.Slice(raw, (int)pos, (int)size);
                            var comp = asset.Find("Compress");
                            if (comp != null && comp.Get("enabled") == "_bool:true")
                            {
                                Bin.Require(comp.Get("codec") == "LZX", "Unsupported compression");
                                data = XMem.Decode(data, cancel);
                            }
                            found = data;
                        }
                }
            }
            if (found == null) return;
            var converted = AliveRace.ConvertLapPacing(found, AliveRace.Activities[c.raceActivity].pacing);
            c.racePacingPath = "race/" + c.level + "-lappacing.bxml";
            WriteNew(c.stage, c.racePacingPath, converted);
        }

        static string Full(string root, string rel) => Path.Combine(root, rel.Replace('/', Path.DirectorySeparatorChar));

        static void WriteNew(string root, string rel, byte[] data)
        {
            string p = Full(root, rel);
            Directory.CreateDirectory(Path.GetDirectoryName(p));
            using (var f = new FileStream(p, FileMode.CreateNew, FileAccess.Write, FileShare.None)) f.Write(data, 0, data.Length);
        }

        // ------------------------------------------------------------------------------------- install

        public sealed class Installed
        {
            public JObj entry; public List<string> files = new List<string>(); public int deferred;
            public List<(object id, string ns, string reason)> deferredEntities;
        }

        /// alive_install.install_level into `outRoot` (the staging tree of the published files). `content`: the client's content folder
        /// (base programs are copied from it). `info`: display name / pack / kind / activities.
        public static Installed Install(Conversion conv, string content, string outRoot, AliveLevelSource info, string origin, ICollection<string> replaced = null)
        {
            bool Exists(string rel) => File.Exists(Full(content, rel)) && !(replaced != null && replaced.Contains(rel));
            string level = conv.level;
            bool Installable(ConvertedAsset a) => Supported.Contains(a.type) && !(a.type == "shader" && BasePrograms.Contains(a.name.ToLowerInvariant()));
            // PORT ADDITION (AliveRace): the race's track definition and lap pacing under the names Reflex's race activity looks up
            // ("<level>-<suffix>", "<level>-lappacing"); an Alive resource of the same name (SX_DLC_SX01's own lap pacing) gives way
            var raceAssets = new List<((string, string) key, string source)>();
            AliveRace.Spec spec = null;
            if (conv.raceActivity != null)
            {
                spec = AliveRace.Activities[conv.raceActivity];
                var tdf = conv.converted.Where(a => a.type == "tdf" && a.name == conv.raceTdf).ToList();
                Bin.Require(tdf.Count == 1, "Race track definition " + conv.raceTdf + " not converted");
                raceAssets.Add((("tdf", level + "-" + spec.suffix), Full(conv.stage, tdf[0].path)));
                if (conv.racePacingPath != null) raceAssets.Add((("bxml", level + "-lappacing"), Full(conv.stage, conv.racePacingPath)));
            }
            var raceKeys = new HashSet<(string, string)>(raceAssets.Select(r => r.key));
            var keep = new HashSet<(string, string)>(conv.converted.Where(Installable).Select(a => (a.type, a.name)));
            keep.ExceptWith(raceKeys);
            var result = new Installed();
            var written = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            void Publish(string rel, string from, bool move)
            {
                Bin.Require(written.Add(rel), "Two resources would share the path " + rel + " (names differing only by letter case)");
                string dest = Full(outRoot, rel);
                Directory.CreateDirectory(Path.GetDirectoryName(dest));
                if (move) File.Move(from, dest);
                else if (File.Exists(from)) File.Copy(from, dest, false);
                else File.WriteAllBytes(dest, Reflex.Port.Content.RxFiles.ReadAllBytes(from));   // a donor file of a packed library (Tracks/<level>.rxtrack)
                result.files.Add(rel);
            }
            void Write(string rel, byte[] data)
            {
                Bin.Require(written.Add(rel), "Two outputs would share the path " + rel);
                WriteNew(outRoot, rel, data); result.files.Add(rel);
            }
            foreach (var a in conv.converted) if (Installable(a) && !raceKeys.Contains((a.type, a.name))) Publish(a.path, Full(conv.stage, a.path), true);
            var database = Bxml.Parse(conv.database).ToElement();
            foreach (var package in database.Iter("Package"))
                foreach (var asset in package.FindAll("Asset").ToList())
                    if (asset.Get("type") == "shader" && asset.Get("name") == "Water_Wake")
                    {
                        string source = Full(content, "Assets/shader/FR_Gibson/FR_GibsonBase/Water_Wake.shader");
                        Bin.Require(Reflex.Port.Content.RxFiles.Exists(source), "Base water-wake program missing (Assets/shader/FR_Gibson/FR_GibsonBase/Water_Wake.shader)");
                        Publish("Assets/shader/" + level + "/" + package.Get("name") + "/Water_Wake.shader", source, false);
                        keep.Add(("shader", "Water_Wake"));
                    }
            var baseEntities = Scenes.ReadSections(conv.scene, new[] { level + "Base" })[level + "Base"].entities;
            bool hasCrowd = baseEntities.Any(e => Scenes.Fields(e).ns.EndsWith("_BillboardCrowdRSData", StringComparison.Ordinal));
            if (hasCrowd && !keep.Any(k => k.Item1 == "shader" && k.Item2.ToLowerInvariant() == "crowd"))
            {
                string source = Full(content, "Assets/shader/SX_Kingston/SX_KingstonBase/crowd.shader");
                Bin.Require(Reflex.Port.Content.RxFiles.Exists(source), "Base crowd program missing (Assets/shader/SX_Kingston/SX_KingstonBase/crowd.shader)");
                var package = database.Iter("Package").FirstOrDefault(p => p.Get("name") == level + "Base");
                Bin.Require(package != null, "No " + level + "Base package");
                Publish("Assets/shader/" + level + "/" + package.Get("name") + "/crowd.shader", source, false);
                var asset = new Elem("Asset").Set("type", EVal.S("shader")).Set("name", EVal.S("crowd"));
                asset.children.Add(new Elem("Heap").Set("offset", EVal.S("_uint:0")).Set("size", EVal.S("_uint:" + Reflex.Port.Content.RxFiles.Length(source))));
                asset.children.Add(new Elem("Block").Set("offset", EVal.S("_uint:0")).Set("size", EVal.S("_uint:0")));
                package.children.Add(asset);
                var handles = database.Find("Handles");
                if (handles != null) handles.children.Add(new Elem("Asset").Set("name", EVal.S("crowd")).Set("type", EVal.S("shader")));
                keep.Add(("shader", "crowd"));
            }
            string raceSection = null;
            if (spec != null)
            {
                raceSection = level + spec.subscene;
                var package = database.Iter("Package").FirstOrDefault(p => p.Get("name") == level + "Base");
                Bin.Require(package != null, "No " + level + "Base package");
                foreach (var parent in database.Iter().ToList()) parent.children.RemoveAll(ch => ch.tag == "Asset" && raceKeys.Contains((ch.Get("type"), ch.Get("name"))));
                var handles = database.Find("Handles");
                foreach (var (key, source) in raceAssets)
                {
                    long size = new FileInfo(source).Length;
                    Publish("Assets/" + key.Item1 + "/" + level + "/" + package.Get("name") + "/" + key.Item2 + "." + key.Item1, source, true);
                    var asset = new Elem("Asset").Set("type", EVal.S(key.Item1)).Set("name", EVal.S(key.Item2));
                    asset.children.Add(new Elem("Heap").Set("offset", EVal.S("_uint:0")).Set("size", EVal.S("_uint:" + size)));
                    asset.children.Add(new Elem("Block").Set("offset", EVal.S("_uint:0")).Set("size", EVal.S("_uint:0")));
                    package.children.Add(asset);
                    if (handles != null) handles.children.Add(new Elem("Asset").Set("name", EVal.S(key.Item2)).Set("type", EVal.S(key.Item1)));
                    keep.Add(key);
                }
            }
            // the race's assets do not make an entity installable: the Base and FreeRide sections stay those of a level without race data
            var blocking = new HashSet<(string, string)>(database.Iter("Asset").Where(a => Scenes.BlockingTypes.Contains(a.Get("type") ?? "")).Select(a => (a.Get("type"), a.Get("name"))));
            blocking.ExceptWith(keep.Where(k => !raceKeys.Contains(k)));
            void Prune(Elem parent)
            {
                parent.children.RemoveAll(ch => ch.tag == "Asset" && !keep.Contains((ch.Get("type"), ch.Get("name"))));
                foreach (var ch in parent.children) Prune(ch);
            }
            Prune(database);
            Write("Database/" + level + ".dx9.database", Bxml.EncodeElement(database));
            var ride = Scenes.FreeRideScene(conv.scene, level, blocking, raceSection);
            Write("Database/" + level + ".dx9.scene", ride.scene);
            Write("Database/" + level + ".dx9.level", LevelDescriptor(conv.levelBytes, level, raceSection));
            result.deferred = ride.deferred.Count; result.deferredEntities = ride.deferred;
            var entry = new JObj()
                .Set("level", level).Set("display_name", string.IsNullOrEmpty(info?.displayName) ? level : info.displayName)
                .Set("pack", info?.pack ?? "").Set("kind", info?.kind ?? "").Set("alive_activities", (info?.activities ?? Array.Empty<string>()).Cast<object>().ToList())
                .Set("conversion", "client-import:" + origin).Set("resources", (long)keep.Count).Set("deferred_entities", (long)ride.deferred.Count)
                .Set("collision", (long)ride.collision).Set("starts", (long)ride.starts).Set("entities", (long)ride.entities);
            if (spec != null)   // the race the client offers (ReflexEvents: an AI race on the level, PORT ADDITION)
                entry.Set("race", new JObj().Set("activity", conv.raceActivity).Set("event_type", spec.eventType).Set("event_type_id", (long)spec.eventTypeId)
                    .Set("subscene", raceSection).Set("riders", conv.raceMaxPlayers).Set("lap_pacing", conv.racePacingPath != null));
            try
            {
                var (min, max) = HeightRange(outRoot, level, ride.scene, conv);
                entry.Set("height_min", min).Set("height_max", max);
            }
            catch (AliveImportException ex) { entry.Set("height_error", ex.Message); }
            // terrain decal layers (HFDecalModel entities), ordered by SortBias
            var decals = new List<(double bias, int order, JObj d)>();
            for (int order = 0; order < baseEntities.Count; order++)
            {
                var f = Scenes.Fields(baseEntities[order]);
                if (!f.ns.EndsWith("_HFDecalModelRSData", StringComparison.Ordinal)) continue;
                string Asset(string key) { var v = f.Field(key); return v != null && v.Count > 0 && v[0].attributes.Count > 0 ? v[0].Attr("name") as string : null; }
                object Value(string key, object def = null) { var v = f.Field(key); return v != null && v.Count > 0 ? v[0].value : def; }
                var sortBias = Value("SortBias", 0.0);
                var d = new JObj().Set("name", Value("Name", "")).Set("model", Asset("ModelAsset")).Set("surface", Asset("SurfaceAsset"))
                    .Set("diffuse", Asset("DiffuseMaterialAsset")).Set("bump", Asset("BumpMaterialAsset")).Set("relief", Asset("ReliefMaterialAsset"))
                    .Set("sort_bias", sortBias).Set("group", Value("GroupNumber", 0L)).Set("matrix", Value("Matrix")).Set("order", (long)order);
                decals.Add((System.Convert.ToDouble(sortBias, System.Globalization.CultureInfo.InvariantCulture), order, d));
            }
            decals = decals.OrderBy(x => x.bias).ThenBy(x => x.order).ToList();
            if (decals.Count > 0)
            {
                entry.Set("terrain_decals", $"alive-{level}-decals.json");
                Write("Database/" + $"alive-{level}-decals.json", Utf8(Json.Dumps(new JObj().Set("level", level).Set("decals", decals.Select(x => (object)x.d).ToList()), 1)));
            }
            // the bounce light volume (BounceLightEntity) as the client's ABNC sidecar
            var bounceEntity = baseEntities.Select(Scenes.Fields).FirstOrDefault(e => e.ns.EndsWith("_BounceLightRSData", StringComparison.Ordinal));
            if (bounceEntity != null && bounceEntity.fields.Count > 0)
            {
                var ba = bounceEntity.Field("BounceAsset");
                string refName = ba != null && ba.Count > 0 ? ba[0].Attr("name") as string : null;
                var bc = conv.converted.FirstOrDefault(a => a.type == "bounce" && a.name == refName);
                if (bc != null)
                {
                    entry.Set("bounce", $"alive-{level}-bounce.bin");
                    Publish("Database/" + $"alive-{level}-bounce.bin", Full(conv.stage, bc.path), true);
                    var ls = bounceEntity.Field("LightScale");
                    entry.Set("bounce_light_scale", ls != null && ls.Count > 0 ? ls[0].value : 1.0);
                }
            }
            // the colour grade (ColorLUTEntity) as the client's ALUT sidecar
            var lutEntity = baseEntities.Select(Scenes.Fields).FirstOrDefault(e => e.ns.EndsWith("_ColorLUTRSData", StringComparison.Ordinal));
            if (lutEntity != null && lutEntity.fields.Count > 0)
            {
                var la = lutEntity.Field("ColorLUTAsset");
                string refName = la != null && la.Count > 0 ? la[0].Attr("name") as string : null;
                var lc = conv.converted.FirstOrDefault(a => a.type == "colorlut" && a.name == refName);
                if (lc != null)
                {
                    entry.Set("color_lut", $"alive-{level}-lut.bin");
                    Publish("Database/" + $"alive-{level}-lut.bin", Installable(lc) ? Full(outRoot, lc.path) : Full(conv.stage, lc.path), !Installable(lc));
                }
            }
            // the albedo programs of the level's own pixel programs (alive_recipe; ReflexOriginalScenery _ALIVE_VM)
            if (conv.albedoPrograms.Count > 0)
            {
                entry.Set("albedo_programs", $"alive-{level}-programs.json");
                var programs = conv.albedoPrograms.OrderBy(kv => kv.Key, StringComparer.Ordinal)
                    .Select(kv =>
                    {
                        var entry1 = new JObj().Set("shader", kv.Key).Set("ops", kv.Value.Get("ops")).Set("result", kv.Value.Get("result"))
                                               .Set("slots", kv.Value.Get("slots")).Set("lit", kv.Value.Get("lit"))
                                               .Set("params", kv.Value.Get("params")).Set("alpha", kv.Value.Get("alpha"));
                        // alive_install.program_entry: a general program's emissive / normal registers + 1 (0: none) and its cube map slots
                        if (kv.Value.Has("general"))
                            entry1.Set("general", 1L).Set("emissive", (long)kv.Value.Get("emissive") + 1).Set("normal", (long)kv.Value.Get("normal") + 1).Set("cubes", kv.Value.Get("cubes"));
                        return (object)entry1;
                    }).ToList();
                Write("Database/" + $"alive-{level}-programs.json", Utf8(Json.Dumps(new JObj().Set("version", 1L).Set("programs", programs)) + "\n"));
            }
            JObj Tiles() { var o = new JObj(); foreach (var kv in conv.rockTiles) o.Set(kv.Key, kv.Value.HasValue ? (object)kv.Value.Value : null); return o; }
            if (conv.rockTiles.Count > 0)
            {
                entry.Set("rock_detail", $"alive-{level}-rock-detail.json");
                Write("Database/" + $"alive-{level}-rock-detail.json", Utf8(Json.Dumps(Tiles(), -1, true)));
            }
            if (conv.skyHdr != null && conv.skyHdr.Count > 0)
            {
                entry.Set("sky_hdr", $"alive-{level}-sky-hdr.json");
                Write("Database/" + $"alive-{level}-sky-hdr.json", Utf8(Json.Dumps(SkyObj(conv.skyHdr))));
            }
            // Alive's own menu pictures of the level (AlivePictures: the loading picture LP_*, the track map LS_* / Free Ride map FRMap_*)
            foreach (var (kind, data) in conv.pictures)
            {
                entry.Set(kind, $"alive-{level}-{kind}.texture");
                Write("Database/" + $"alive-{level}-{kind}.texture", data);
            }
            if (level == "FR_DLC_Hills")
            {
                // the sidecars the Sliderock client code read before the registry existed
                if (conv.hasRockTiles && !Exists("Database/sliderock-rock-detail.json")) Write("Database/sliderock-rock-detail.json", Utf8(Json.Dumps(Tiles(), -1, true)));
                if (conv.skyHdr != null && conv.skyHdr.Count > 0 && !Exists("Database/sliderock-sky-hdr.json")) Write("Database/sliderock-sky-hdr.json", Utf8(Json.Dumps(SkyObj(conv.skyHdr))));
                if (!Exists("Database/sliderock-map-v1.txt")) Write("Database/sliderock-map-v1.txt", Utf8("Sliderock Canyon map preview v1\n"));
            }
            result.entry = entry;
            return result;
        }

        static JObj SkyObj(List<KeyValuePair<string, object>> attrs) { var o = new JObj(); foreach (var kv in attrs) o.Set(kv.Key, kv.Value); return o; }
        static byte[] Utf8(string s) => new UTF8Encoding(false).GetBytes(s);

        /// alive_install.level_descriptor: the .level with the installed subscenes (Base, FreeRide, the race's).
        static byte[] LevelDescriptor(byte[] source, string level, string raceSection = null)
        {
            var root = Bxml.Parse(source).ToElement();
            var subs = root.Find("Subscenes");
            Bin.Require(subs != null, "Level descriptor without Subscenes");
            subs.children.Clear();
            foreach (var name in raceSection != null ? new[] { level + "Base", level + "FreeRide", raceSection } : new[] { level + "Base", level + "FreeRide" })
                subs.children.Add(new Elem("Subscene").Set("name", EVal.S(name)).Set("path", EVal.S(".")));
            return Bxml.EncodeElement(root);
        }

        /// alive_heights.terrain_height_range: (min, max) terrain height in feet of the installed level.
        static (double, double) HeightRange(string outRoot, string level, byte[] installedScene, Conversion conv)
        {
            Entity terrain = null;
            foreach (var e in Scenes.ReadSections(installedScene, new[] { level + "Base" })[level + "Base"].entities)
            {
                var f = Scenes.Fields(e);
                if (f.ns.EndsWith("_HFTerrainRSData", StringComparison.Ordinal)) { terrain = f; break; }
            }
            Bin.Require(terrain != null, level + ": no HFTerrain entity");
            var hm = terrain.Field("HeightMapAsset");
            Bin.Require(hm != null && hm.Count > 0 && hm[0].Attr("name") is string, level + ": no height map");
            string name = (string)hm[0].Attr("name");
            object strengthV = terrain.FirstValue("TerrainStrength", 2048.0);
            float strength = (float)System.Convert.ToDouble(strengthV, System.Globalization.CultureInfo.InvariantCulture);
            var matches = conv.converted.Where(a => a.type == "texture" && a.path.Substring(a.path.LastIndexOf('/') + 1) == name + ".texture")
                .Select(a => a.path.Split('/')).OrderBy(p => p, PartsComparer.Instance).ToList();
            Bin.Require(matches.Count > 0, $"{level}: heightfield {name} not converted");
            var data = File.ReadAllBytes(Full(outRoot, string.Join("/", matches[0])));
            Bin.Require(data.Length >= 56 && Bin.U32LE(data, 52) == 114, "Heightfield is not R32F");
            int width = (int)Bin.U32LE(data, 44), height = (int)Bin.U32LE(data, 48), offset = 56 + (int)Bin.U32LE(data, 16) * 8;
            Bin.Require(offset + (long)width * height * 4 <= data.Length, "Truncated heightfield");
            float min = float.PositiveInfinity, max = float.NegativeInfinity; bool nan = false;
            for (int i = 0; i < width * height; i++)
            {
                float v = Bin.F32LE(data, offset + 4 * i) * strength;
                if (float.IsNaN(v)) nan = true;
                if (v < min) min = v;
                if (v > max) max = v;
            }
            if (nan) return (double.NaN, double.NaN);
            return (min, max);
        }

        sealed class PartsComparer : IComparer<string[]>
        {
            public static readonly PartsComparer Instance = new PartsComparer();
            public int Compare(string[] a, string[] b)
            {
                for (int i = 0; i < Math.Min(a.Length, b.Length); i++) { int c = string.CompareOrdinal(a[i], b[i]); if (c != 0) return c; }
                return a.Length.CompareTo(b.Length);
            }
        }

        // ------------------------------------------------------------------------------------- publication

        public static string Sha256(string path)
        {
            using (var s = File.OpenRead(path)) using (var h = SHA256.Create()) return Bin.Hex(h.ComputeHash(s), 0, 32);
        }

        /// The registry with `entries` added (an existing entry of the same level is replaced; other levels are kept as written).
        public static string MergeRegistry(string existing, IEnumerable<JObj> entries)
        {
            var levels = new List<object>();
            var added = entries.ToList();
            if (existing != null)
            {
                var old = Json.Parse(existing) as JObj;
                Bin.Require(old != null && old.Get("levels") is List<object>, "Unreadable " + Registry);
                foreach (var e in (List<object>)old.Get("levels"))
                {
                    var same = e is JObj o ? added.FirstOrDefault(a => (string)a.Get("level") == o.Get("level") as string) : null;
                    if (same != null) { levels.Add(same); added.Remove(same); }   // a re-imported level keeps its place
                    else levels.Add(e);
                }
            }
            levels.AddRange(added);
            return Json.Dumps(new JObj().Set("version", 1L).Set("levels", levels), 1) + "\n";
        }
    }
}

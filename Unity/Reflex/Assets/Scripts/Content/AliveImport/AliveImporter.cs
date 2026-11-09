// The in-client MX vs ATV Alive importer: finds the player's Alive levels in what they point it at (the disc image, an extracted disc folder,
// the 5451087B content folder of an Xbox 360 drive or any folder of LIVE packages, single LIVE packages, extracted package folders), checks
// that each level is the original release (STFS packages: the whole SHA-1 hash tree and every block read; every source: the level's known
// file names and sizes and the SHA-256 of its database), names the levels from the game's own tables (_re/formats/alive_inventory.py) and
// finds their menu pictures (AlivePictures), and converts / installs the chosen levels into the client's content folder (LevelInstaller).
// Pure C# (System only): no Python, no native code.
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using System.Text;
using System.Threading;

namespace Reflex.Port.Content.AliveImport
{
    /// The levels found in the player's sources. Keeps the sources open until disposed.
    public sealed class AliveSources : IDisposable
    {
        public readonly List<AliveLevelSource> levels = new List<AliveLevelSource>();
        /// The Alive motorcycles found (AliveBikes.Find): the ones that ride as a Reflex bike are importable.
        public readonly List<AliveBikeSource> bikes = new List<AliveBikeSource>();
        public readonly List<Volume> volumes = new List<Volume>();
        public readonly List<string> warnings = new List<string>();
        /// Levels named from the game's own tables found in the sources (the others carry the built-in names).
        public int tableNames;
        /// Levels imported before that AddPictures gave their menu pictures (no new import).
        public readonly List<string> picturesAdded = new List<string>();
        public IEnumerable<AliveLevelSource> Importable => levels.Where(l => l.recognized);
        public void Dispose() { foreach (var v in volumes) v.Dispose(); volumes.Clear(); }
    }

    public sealed class AliveImportOptions
    {
        /// Replace a level this importer installed before (only the files its import manifest lists, unchanged since).
        public bool replace = true;
    }

    public static class AliveImporter
    {
        public const uint TitleId = 0x5451087B;
        public const string ManifestPrefix = "alive-import-";

        sealed class Known
        {
            public readonly string level, pack, name; public readonly string[] activities; public readonly long[] sizes; public readonly string databaseSha256;
            public Known(string level, string pack, string name, string activities, long l, long d, long p, long s, string sha)
            { this.level = level; this.pack = pack; this.name = name; this.activities = activities.Split(','); sizes = new[] { l, d, p, s }; databaseSha256 = sha; }
        }

        static readonly string[] Suffixes = { "level", "database", "package", "scene" };

        // The 26 levels of the original release (disc + level DLC): files .xenon.{level,database,package,scene} sizes, database SHA-256.
        static readonly Known[] KnownLevels =
        {
            new Known("FR_Dunes", "disc", "Devil's Playground", "Free Ride", 198, 21270, 68727952, 313072, "34e133c2fe36ef76118fd5d026d640e37633670cfe955e46f7b559c1a8e2db07"),
            new Known("FR_Quarry", "disc", "Bryn Carreg Quarry", "Free Ride", 198, 22946, 65966839, 105493, "d4639311ab7fb25ccd63bd2508c2ad45322b710ca1baa1d423b8572163ccb9d8"),
            new Known("NAT_Farm", "disc", "Les Champs MX Park", "National", 214, 51923, 69213419, 626458, "1b3f08407887faa86b012458fd85f00455103425ee2fe8d80359dbf02bcb673b"),
            new Known("NAT_Farm_B", "disc", "Sous le Pont", "National", 217, 50400, 66719840, 816585, "4a08e37a4bdb90990ee6ed0e82032ce65f639fc2c625cb5117dcdc8e2ad671db"),
            new Known("NAT_Farm_C", "disc", "Maison de MotoX", "National", 215, 47644, 65844049, 849507, "26b3b15ecba9e64229ccb130f95e516d610b27243d2b96611964703ac085c048"),
            new Known("NAT_Northwest", "disc", "Onsenki Park", "National", 217, 40167, 58307937, 679525, "e83fffd13e4aae5822331bfbe8d8b96f95a3cd47367a45261afd4803a1ec6a4d"),
            new Known("NAT_Northwest_B", "disc", "Sanko Speedway", "National", 219, 42990, 65657439, 778841, "9e7ad6e8e086e966d31ca030702c46c4c80428113f486ffff474444c92f5b1b7"),
            new Known("NAT_Northwest_C", "disc", "Yukiyama MX", "National", 221, 42208, 59364021, 871725, "e55fb03630f79fa93e3307b0067956aca93198168aa9d7d8331a8c7c0100643e"),
            new Known("NAT_Southwest", "disc", "Grandview", "National", 219, 50548, 77027936, 714829, "111e48754730fb3155584a278b3b2180ce4fdec3e6c50b7106862361734dd8ca"),
            new Known("NAT_Southwest_B", "disc", "Piston Valley", "National", 218, 51758, 79891865, 772128, "c8ce95285c1045260f251752cfc49d9d13d57089918d36ab1c83f8f774fc5db2"),
            new Known("NAT_Southwest_C", "disc", "Catapult Canyon", "National", 219, 55290, 80872652, 775696, "b5fd8b12106c17eec6ee97103264b7afd5816d7957d4153fe22c432896da2757"),
            new Known("NAT_Tropic", "disc", "45 Palms MX", "National", 213, 55809, 79024636, 585841, "158acd6a3849caf69c68e5f54886d6ec0274f40b09609a82aa08932d2e7578b8"),
            new Known("NAT_Tropic_B", "disc", "Oceanview", "National", 218, 54383, 77803258, 636623, "bede03d7a7fb74591a08b8077ab151fbdc2764cde46e446e1191eb0501fddda5"),
            new Known("NAT_Tropic_C", "disc", "Bamboo Hills", "National", 216, 55585, 79612803, 737160, "4436937fca53de3c72436c936b44dcbeb2ea885c1d3ab3f3109028412ef593de"),
            new Known("ST_Farm", "disc", "La Ferme du Vert", "Short track (Rhythm Racing)", 213, 40144, 61717314, 215733, "3f8bc5408ffe486695884e34f4df655ddb36910a75bf08d0017def4ed979e6fc"),
            new Known("ST_Northwest", "disc", "Kinoko", "Short track (Rhythm Racing)", 214, 37490, 62113839, 259906, "aa78fbd0f16fa20059790088c115f40128b81ff1b300ac4f8ab8f7e2e6356f7c"),
            new Known("ST_Southwest", "disc", "MX Battlegrounds", "Short track (Rhythm Racing)", 216, 40915, 72463108, 303583, "08e85abb2b2923eac9cafe12b16c5ac8af46c6307528864286fddf58b3d105f2"),
            new Known("ST_Tropic", "disc", "Foggy Ridge", "Short track (Rhythm Racing)", 215, 44612, 65081520, 262912, "a6bff34d963f32adf161e1323d937622e00ff7b4d4b588f97c98e6a96281972d"),
            new Known("FR_DLC_JSComp", "stewart_compound", "The Stewart Compound", "Free Ride", 203, 47177, 68325101, 602944, "df98c64922be45decb4bc5c08b60739b185ebaa1ac01cf6114c5d8091ac6227f"),
            new Known("NAT_DLC_JSComp_A", "stewart_compound", "Stewart MX South Track", "National", 218, 55570, 72859499, 626245, "22f0f8a3187466aa68ccc091f53a15227d44e85099f5e81a595bdd0163115d35"),
            new Known("NAT_DLC_JSComp_B", "stewart_compound", "JS7 Pro Loop", "National", 221, 55663, 73129984, 726758, "ba1b41f267dd444532af430cbec1f9194a4ad25b98b7707a9b0635b27ae91f13"),
            new Known("NAT_DLC_Quarry_A", "quarry_nationals", "Quarry Basin", "National", 222, 52632, 86921610, 788170, "d36bb5da968bc796fb691e70d58168048dbdd3352d571c68cd7fef3ac881879e"),
            new Known("NAT_DLC_Quarry_B", "quarry_nationals", "Edgerock MX", "National", 223, 52366, 87127637, 815758, "303af0c2b29b4ab7589862098c3053c086008fb55b43e87ab5c63e7f1ef3edd1"),
            new Known("SX_DLC_SX01", "sx_events", "Lexington", "Supercross", 215, 61912, 49894869, 485430, "3e18b8a199f2a811810d80b3cac48c1994eb62463e0b5ee6f5c1ba3671aba9e3"),
            new Known("SX_DLC_SX02", "sx_events", "Williamsburg", "Supercross", 219, 61440, 48704976, 468525, "a598035e4c7159735820dad67c648ee2bce0dc5cc0b0e50a45b661de5b4355ea"),
            new Known("FR_DLC_Hills", "sliderock_canyon", "Sliderock Canyon", "Free Ride", 200, 32084, 85320266, 221821, "3ccd0b6263bb977bf8a19eb8035525521278e6392452b25b550417de9fb5aa0a"),
        };

        // alive_sources.PACKS: display names and LIVE package names (00000002/<id>).
        static readonly (string pack, string display, string package)[] Packs =
        {
            ("disc", "MX vs. ATV Alive (disc)", null),
            ("stewart_compound", "Stewart Compound", "6F77D36D52D636B74F780FF11347A9A7494061AC54"),
            ("quarry_nationals", "Quarry Nationals Events Pack", "0C6A024B0FA1F0836E5D0A2490A16074479CE0D654"),
            ("sx_events", "SX Events Pack", "E100C35E07486A75C07077A40EF134F5E9C7EBED54"),
            ("sliderock_canyon", "Sliderock Canyon Event Pack", "9F93B2EC2A51B0EC79554D391E8051055BA0262154"),
        };
        static readonly HashSet<string> NotTracks = new HashSet<string> { "UI_World", "Model_Viewer_Tool" };
        static readonly Dictionary<int, string> ActivityTypes = new Dictionary<int, string> { { 8, "Supercross" }, { 9, "Free Ride" }, { 10, "National" }, { 19, "Short track (Rhythm Racing)" } };

        /// alive_sources.classify.
        public static string Classify(string level)
        {
            if (level.StartsWith("NAT_", StringComparison.Ordinal)) return "National";
            if (level.StartsWith("ST_", StringComparison.Ordinal)) return "Short track (Rhythm Racing)";
            if (level.StartsWith("SX_", StringComparison.Ordinal)) return "Supercross";
            if (level == "FR_DLC_JSComp") return "Free Ride compound";
            if (level.StartsWith("FR_", StringComparison.Ordinal)) return "Free Ride";
            return "other";
        }

        // ------------------------------------------------------------------------------------- finding sources

        /// Opens every source under the given paths and lists their levels (original-release check included).
        public static AliveSources Scan(IEnumerable<string> paths, Action<string> status, CancellationToken cancel)
        {
            var r = new AliveSources();
            foreach (var path in paths)
            {
                cancel.ThrowIfCancellationRequested();
                try { Open(path, r, status, cancel, 0); }
                catch (OperationCanceledException) { throw; }
                catch (Exception ex) { r.warnings.Add(ReflexPaths.LabelOf(path) + ": " + ex.Message); }
            }
            status?.Invoke("Reading level names");
            var names = new Dictionary<string, (string name, List<string> activities)>();
            try { names = NamesFromTables(r, cancel); r.tableNames = names.Count(n => !string.IsNullOrEmpty(n.Value.name)); }
            catch (OperationCanceledException) { throw; }
            catch (Exception ex) { r.warnings.Add("Level names: " + ex.Message); }
            status?.Invoke("Finding the bikes");
            try { r.bikes.AddRange(AliveBikes.Find(r, cancel)); }
            catch (OperationCanceledException) { throw; }
            catch (Exception ex) { r.warnings.Add("Bikes: " + ex.Message); }
            status?.Invoke("Finding the levels' pictures");
            var art = new Dictionary<string, Dictionary<string, AlivePictureRef>>();
            try { art = Pictures.Index(r.volumes, cancel); }
            catch (OperationCanceledException) { throw; }
            catch (Exception ex) { r.warnings.Add("Level pictures: " + ex.Message); }
            foreach (var v in r.volumes)
            {
                foreach (var f in v.files.Where(f => f.Name.EndsWith(".xenon.level", StringComparison.OrdinalIgnoreCase)))
                {
                    cancel.ThrowIfCancellationRequested();
                    string level = f.Name.Substring(0, f.Name.Length - ".xenon.level".Length);
                    if (NotTracks.Contains(level) || r.levels.Any(l => l.level == level && l.recognized)) continue;
                    var src = new AliveLevelSource { level = level, volume = v, directory = f.Directory, kind = Classify(level) };
                    var known = KnownLevels.FirstOrDefault(k => k.level == level);
                    if (known != null)
                    {
                        src.pack = known.pack; src.packDisplay = Packs.First(p => p.pack == known.pack).display;
                        src.displayName = known.name; src.activities = known.activities;
                    }
                    if (names.TryGetValue(level, out var n))
                    {
                        if (!string.IsNullOrEmpty(n.name)) src.displayName = n.name;
                        src.activities = n.activities.Distinct().OrderBy(a => a, StringComparer.Ordinal).ToArray();
                    }
                    art.TryGetValue(level, out src.pictures);
                    src.problem = Check(src, known, cancel);
                    src.recognized = src.problem == null;
                    var files = Suffixes.Select(s => src.File(s)).ToList();
                    src.bytes = files.Where(x => x != null).Sum(x => x.size);
                    r.levels.RemoveAll(l => l.level == level);
                    r.levels.Add(src);
                }
            }
            r.levels.Sort((a, b) =>
            {
                int pa = Array.FindIndex(Packs, p => p.pack == a.pack), pb = Array.FindIndex(Packs, p => p.pack == b.pack);
                if (pa < 0) pa = 99; if (pb < 0) pb = 99;
                return pa != pb ? pa.CompareTo(pb) : string.CompareOrdinal(a.level, b.level);
            });
            return r;
        }

        static void Open(string path, AliveSources r, Action<string> status, CancellationToken cancel, int depth)
        {
            path = Path.GetFullPath(path);
            if (File.Exists(path))
            {
                string magic = "";
                using (var s = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read))
                {
                    var b = new byte[4]; if (s.Read(b, 0, 4) == 4) magic = Encoding.ASCII.GetString(b);
                }
                if (magic == "LIVE" || magic == "PIRS" || magic == "CON ")
                {
                    status?.Invoke("Checking " + Path.GetFileName(path));
                    var v = new StfsVolume(path);
                    if (v.titleId != TitleId) { v.Dispose(); r.warnings.Add(Path.GetFileName(path) + ": not MX vs ATV Alive content (title " + v.titleId.ToString("X8") + ")"); return; }
                    if (v.displayName.Length == 0) v.displayName = Path.GetFileName(path);
                    r.volumes.Add(v);
                    return;
                }
                status?.Invoke("Reading " + Path.GetFileName(path));
                r.volumes.Add(new DiscVolume(path));
                return;
            }
            Bin.Require(Directory.Exists(path), "Not found: " + path);
            // an Xbox 360 content folder: <...>/5451087B/00000002/<packages>
            foreach (var sub in new[] { "00000002", Path.Combine("5451087B", "00000002") })
            {
                string d = Path.Combine(path, sub);
                if (Directory.Exists(d) && depth == 0) { OpenPackages(d, r, status, cancel); }
            }
            bool packages = Directory.GetFiles(path).Any(f => LooksLikePackage(f));
            if (packages) OpenPackages(path, r, status, cancel);
            var folder = new FolderVolume(path);
            if (folder.files.Any(f => f.Name.EndsWith(".xenon.level", StringComparison.OrdinalIgnoreCase) || f.Name.EndsWith(".xenon.database", StringComparison.OrdinalIgnoreCase)))
            {
                folder.displayName = ReflexPaths.LabelOf(path);
                r.volumes.Add(folder);
            }
            else if (!packages && !r.volumes.Any()) r.warnings.Add(ReflexPaths.LabelOf(path) + ": no MX vs ATV Alive levels or LIVE packages found");
        }

        static bool LooksLikePackage(string file)
        {
            try
            {
                if (new FileInfo(file).Length < 0x971a) return false;
                using (var s = new FileStream(file, FileMode.Open, FileAccess.Read, FileShare.Read))
                {
                    var b = new byte[4]; if (s.Read(b, 0, 4) != 4) return false;
                    string m = Encoding.ASCII.GetString(b);
                    return m == "LIVE" || m == "PIRS" || m == "CON ";
                }
            }
            catch (IOException) { return false; }
            catch (UnauthorizedAccessException) { return false; }
        }

        static void OpenPackages(string dir, AliveSources r, Action<string> status, CancellationToken cancel)
        {
            foreach (var f in Directory.GetFiles(dir).OrderBy(x => x, StringComparer.OrdinalIgnoreCase))
            {
                cancel.ThrowIfCancellationRequested();
                if (!LooksLikePackage(f) || r.volumes.Any(v => string.Equals(v.origin, Path.GetFullPath(f), StringComparison.OrdinalIgnoreCase))) continue;
                try
                {
                    status?.Invoke("Checking " + Path.GetFileName(f));
                    var v = new StfsVolume(f);
                    if (v.titleId != TitleId) { v.Dispose(); continue; }
                    // content packages without Alive data files (gear, vehicles) stay open only when they carry levels or tables
                    if (!v.files.Any(x => x.Name.IndexOf(".xenon.", StringComparison.OrdinalIgnoreCase) >= 0)) { v.Dispose(); continue; }
                    if (v.displayName.Length == 0) v.displayName = Path.GetFileName(f);
                    r.volumes.Add(v);
                }
                catch (AliveImportException ex) { r.warnings.Add(Path.GetFileName(f) + ": " + ex.Message); }
            }
        }

        /// Why a level is not the original release, or null.
        static string Check(AliveLevelSource src, Known known, CancellationToken cancel)
        {
            if (known == null) return "not a level of the original release";
            for (int i = 0; i < 4; i++)
            {
                var f = src.File(Suffixes[i]);
                if (f == null) return "missing " + src.level + ".xenon." + Suffixes[i];
                if (f.size != known.sizes[i]) return src.level + ".xenon." + Suffixes[i] + " differs from the original release (size)";
            }
            try
            {
                var db = src.volume.Read(src.File("database"), cancel);
                using (var h = SHA256.Create())
                    if (Bin.Hex(h.ComputeHash(db), 0, 32) != known.databaseSha256) return src.level + ".xenon.database differs from the original release";
            }
            catch (AliveImportException ex) { return ex.Message; }
            return null;
        }

        /// alive_inventory.names_by_level: level file name -> (store product name, activity types), from MXTables_Default / MXTables_DLC*
        /// (ActivityLocations, ActivityTypeLocations) and the MXUI* StoreProducts.ENG strings.
        static Dictionary<string, (string name, List<string> activities)> NamesFromTables(AliveSources r, CancellationToken cancel)
        {
            var locations = new Dictionary<long, string>(); var typeLocations = new List<(long location, long type, long product)>();
            var strings = new Dictionary<string, string>(StringComparer.Ordinal);
            foreach (var v in r.volumes.OrderBy(PackOrder))
                foreach (var dir in v.files.Select(f => f.Directory).Distinct())
                {
                    var inDir = v.files.Where(f => f.Directory == dir).OrderBy(f => f.Name, StringComparer.Ordinal).ToList();
                    foreach (var db in inDir.Where(f => f.Name.StartsWith("MXTables_", StringComparison.Ordinal) && f.Name.EndsWith(".xenon.database", StringComparison.Ordinal)))
                    {
                        if (!(db.Name.StartsWith("MXTables_Default", StringComparison.Ordinal) || db.Name.StartsWith("MXTables_DLC", StringComparison.Ordinal))) continue;
                        foreach (var data in Assets(v, db, (p, n, t) => t == "tbldata", cancel))
                        {
                            var doc = Bxml.Parse(data, scene: true);
                            for (int i = 0; i < doc.nodes.Count; i++)
                            {
                                if (doc.Tag(i) != "Row") continue;
                                var a = doc.AttributesAt(i);
                                object A(string k) => a.FirstOrDefault(kv => kv.Key == k).Value;
                                if (a.Any(kv => kv.Key == "Filename") && A("ActivityLocationId") is long loc && A("Filename") is string fn) locations[loc] = fn;
                                if (a.Any(kv => kv.Key == "ActivityTypeLocationId") && A("StoreProductId") is long sp && A("ActivityTypeId") is long ty && A("ActivityLocationId") is long l2)
                                    typeLocations.Add((l2, ty, sp));
                            }
                        }
                    }
                    foreach (var db in inDir.Where(f => f.Name.StartsWith("MXUI", StringComparison.Ordinal) && f.Name.EndsWith(".xenon.database", StringComparison.Ordinal)))
                        foreach (var data in Assets(v, db, (p, n, t) => t == "localiz" && n.EndsWith("StoreProducts.ENG", StringComparison.Ordinal), cancel))
                            foreach (var kv in Localiz(data)) strings[kv.Key] = kv.Value;
                }
            var result = new Dictionary<string, (string, List<string>)>();
            foreach (var (location, type, product) in typeLocations)
            {
                if (!locations.TryGetValue(location, out var level) || !ActivityTypes.ContainsKey((int)type)) continue;
                strings.TryGetValue("STR_" + product, out var name);
                if (!result.TryGetValue(level, out var e)) result[level] = e = (name, new List<string>());
                e.Item2.Add(ActivityTypes[(int)type]);
            }
            return result;
        }

        /// A volume's place among the packs (alive_sources.PACKS: the disc, Stewart Compound, Quarry, SX, Sliderock) by its first level; 99 none.
        internal static int PackOrder(Volume v)
        {
            var lv = v.files.FirstOrDefault(f => f.Name.EndsWith(".xenon.level", StringComparison.OrdinalIgnoreCase));
            string level = lv == null ? null : lv.Name.Substring(0, lv.Name.Length - 12);
            var k = KnownLevels.FirstOrDefault(x => x.level == level);
            int i = k == null ? -1 : Array.FindIndex(Packs, p => p.pack == k.pack);
            return i < 0 ? 99 : i;
        }

        /// alive_sources.iter_assets: the decoded heap resources of one database's packages (only those `want` selects).
        internal static IEnumerable<byte[]> Assets(Volume v, VolumeFile database, Func<string, string, string, bool> want, CancellationToken cancel)
        {
            var tree = Bxml.Parse(v.Read(database, cancel)).ToElement();
            foreach (var package in tree.FindAll("Packages").SelectMany(p => p.FindAll("Package")))
            {
                string filename = package.Get("file");
                Bin.Require(!string.IsNullOrEmpty(filename) && !filename.Contains("/") && !filename.Contains("\\"), "Package file name");
                var assets = package.FindAll("Asset").Where(a => want(package.Get("name"), a.Get("name"), a.Get("type"))).ToList();
                if (assets.Count == 0) continue;
                var pf = v.Find((database.Directory.Length > 0 ? database.Directory + "/" : "") + filename + ".xenon.package");
                Bin.Require(pf != null, filename + ".xenon.package not found");
                foreach (var asset in assets)
                {
                    var heap = asset.Find("Heap");
                    long pos = (long)XMem.MetadataUint(package.Get("offset")) + XMem.MetadataUint(heap.Get("offset"));
                    long size = XMem.MetadataUint(heap.Get("size"));
                    Bin.Require(pos + size <= pf.size, "Asset exceeds source package");
                    var block = asset.Find("Block");
                    Bin.Require(block == null || XMem.MetadataUint(block.Get("size")) == 0, "Unsupported block asset");
                    var data = v.ReadRange(pf, pos, (int)size, cancel);
                    var comp = asset.Find("Compress");
                    if (comp != null && comp.Get("enabled") == "_bool:true")
                    {
                        Bin.Require(comp.Get("codec") == "LZX", "Unsupported compression");
                        data = XMem.Decode(data, cancel);
                    }
                    yield return data;
                }
            }
        }

        internal static Dictionary<string, string> LocalizStrings(byte[] data) => Localiz(data);

        /// alive_inventory.localiz_strings.
        static Dictionary<string, string> Localiz(byte[] data)
        {
            uint count = Bin.U32BE(data, 4), keyBytes = Bin.U32BE(data, 8);
            var keys = new List<string>(); int st = 12;
            for (int i = 12; i < 12 + keyBytes && keys.Count < count; i++) if (data[i] == 0) { keys.Add(Encoding.UTF8.GetString(data, st, i - st)); st = i + 1; }
            int p = 12 + (int)keyBytes;
            var lengths = new uint[count]; for (int i = 0; i < count; i++) lengths[i] = Bin.U32BE(data, p + 4 * i);
            p += 4 * (int)count + 4;
            var o = new Dictionary<string, string>(StringComparer.Ordinal);
            for (int i = 0; i < Math.Min(keys.Count, (int)count); i++)
            {
                o[keys[i]] = Encoding.BigEndianUnicode.GetString(data, p, 2 * (int)lengths[i]);
                p += 2 * (int)lengths[i] + 2;
            }
            return o;
        }

        // ------------------------------------------------------------------------------------- importing

        /// Why `content` cannot receive Alive levels, or null: it must be an extracted Reflex content folder (reflex-extracted-v1.txt, Database/).
        public static string ContentProblem(string content)
        {
            if (string.IsNullOrEmpty(content) || !Directory.Exists(content)) return "The client's content folder was not found";
            if (!File.Exists(Path.Combine(content, "reflex-extracted-v1.txt")) || !Directory.Exists(Path.Combine(content, "Database")))
                return "The content folder is not an extracted Reflex install (import the Reflex game files first)";
            // the content folder itself must be real (a link to another install's content is an overlay, and RequireNoLink keeps writes out
            // of links inside it); the folders above it are wherever the player keeps the client (SteamOS / Proton: ~/.steam/steam, /run/media)
            return ReflexFs.IsLink(content.TrimEnd('/', '\\')) ? "The content folder is a symbolic link to another folder: " + content : null;
        }

        /// Whether a level is already in the content folder (by this importer, or by another installer).
        public static bool Installed(string content, string level) => RxFiles.Exists(Path.Combine(content, "Database", level + ".dx9.database"));   // (packed in Tracks/<level>.rxtrack too)

        /// Converts and installs the given levels, one after another; the result lists every level (a failed level leaves nothing behind).
        /// Cancelling stops after the level in progress (removed, its result "Cancelled"); the levels finished before stay installed.
        public static List<AliveLevelResult> Import(IEnumerable<AliveLevelSource> levels, string content, AliveImportOptions options,
                                                    Action<AliveProgress> progress, CancellationToken cancel)
        {
            options = options ?? new AliveImportOptions();
            content = Path.GetFullPath(content);
            string problem = ContentProblem(content);
            if (problem != null) throw new AliveImportException(problem);
            var results = new List<AliveLevelResult>();
            foreach (var src in levels)
            {
                if (cancel.IsCancellationRequested) break;
                var res = new AliveLevelResult { level = src.level, displayName = src.Label };
                var clock = System.Diagnostics.Stopwatch.StartNew();
                try { ImportLevel(src, content, options, res, progress, cancel); res.ok = true; }
                catch (OperationCanceledException) { res.error = "Cancelled"; results.Add(res); break; }
                catch (Exception ex) { res.error = ex.Message; }
                res.seconds = clock.Elapsed.TotalSeconds;
                results.Add(res);
            }
            return results;
        }

        static string Full(string root, string rel) => Path.Combine(root, rel.Replace('/', Path.DirectorySeparatorChar));

        static void ImportLevel(AliveLevelSource src, string content, AliveImportOptions options, AliveLevelResult res, Action<AliveProgress> progress, CancellationToken cancel)
        {
            Bin.Require(src.recognized, src.problem ?? "Not an original level");
            string level = src.level, db = Path.Combine(content, "Database");
            string manifestPath = Path.Combine(db, ManifestPrefix + level + ".json");
            void Report(string stage, int done, int total) => progress?.Invoke(new AliveProgress { level = level, stage = stage, done = done, total = total });
            List<string> previous = null;
            // a level packed into Tracks/<level>.rxtrack (PACK GAME FILES: core/plan/RXARCHIVE.md) has its manifest inside the archive (RxFiles):
            // still this importer's. The new import is published loose (a loose file wins) and the level's old archive goes once it is all in
            string packedArchive = Path.Combine(content, ReflexDataPack.TracksDir, level + ".rxtrack");
            bool packed = File.Exists(packedArchive) && !File.Exists(manifestPath) && RxFiles.Exists(manifestPath);
            if (Installed(content, level))
            {
                Bin.Require(RxFiles.Exists(manifestPath), src.Label + " is already installed in the content folder (by another installer); remove it there first");
                Bin.Require(options.replace, src.Label + " is already imported");
                previous = PreviousFiles(content, manifestPath);
            }
            // the staging folder stays beside the published files (one same-volume move each) and short (Windows' 260-character paths)
            string stage = Path.Combine(content, ".ai-" + Guid.NewGuid().ToString("N").Substring(0, 8));
            if (Path.DirectorySeparatorChar == '\\') CheckPathLengths(src, Path.Combine(stage, "o"), cancel);
            var moved = new List<string>(); var setAside = new List<string>();
            try
            {
                Directory.CreateDirectory(stage);
                string convDir = Path.Combine(stage, "c"), outDir = Path.Combine(stage, "o");
                Report("Reading", 0, 1);
                var conv = LevelInstaller.Convert(src, convDir, (s, d, t) => Report(s, d, t), cancel);
                cancel.ThrowIfCancellationRequested();
                Report("Installing", 0, 1);
                var installed = LevelInstaller.Install(conv, content, outDir, src, src.volume.origin, new HashSet<string>(previous ?? new List<string>(), StringComparer.OrdinalIgnoreCase));
                cancel.ThrowIfCancellationRequested();
                bool windows = Path.DirectorySeparatorChar == '\\';
                var prev = new HashSet<string>(previous ?? new List<string>(), StringComparer.OrdinalIgnoreCase);
                foreach (var rel in installed.files)
                {
                    string dest = Full(content, rel);
                    if (windows) Bin.Require(dest.Length < 260 && rel.Split('/').All(s => s.Length <= 255), "The client folder is too deeply nested for " + rel + "; move the client to a shorter path, such as " + ReflexPaths.ShortFolderExample(content));
                    Bin.Require(!File.Exists(dest) || prev.Contains(rel), "Would replace an existing file: " + rel);
                    RequireNoLink(content, rel);
                }
                RequireNoLink(content, "Database/" + ManifestPrefix + level + ".json");
                // publish: set the previous import of this level aside (only its own unchanged files; restored if publishing fails), then move
                // the new files in
                if (previous != null)
                {
                    foreach (var rel in previous)
                    {
                        string p = Full(content, rel), keep = Full(Path.Combine(stage, "previous"), rel);
                        Directory.CreateDirectory(Path.GetDirectoryName(keep));
                        File.Move(p, keep); setAside.Add(rel);
                    }
                    if (File.Exists(manifestPath)) File.Move(manifestPath, Path.Combine(stage, "previous-manifest.json"));   // (a packed one stays in its archive)
                }
                int n = 0; long bytes = 0;
                foreach (var rel in installed.files)
                {
                    cancel.ThrowIfCancellationRequested();
                    Report("Publishing", n++, installed.files.Count);
                    string from = Full(outDir, rel), dest = Full(content, rel);
                    Directory.CreateDirectory(Path.GetDirectoryName(dest));
                    File.Move(from, dest);
                    moved.Add(rel);
                    bytes += new FileInfo(dest).Length;
                }
                var manifest = new JObj().Set("version", 1L).Set("level", level).Set("display_name", src.Label).Set("source", src.volume.origin)
                    .Set("imported_utc", DateTime.UtcNow.ToString("yyyy-MM-ddTHH:mm:ssZ", System.Globalization.CultureInfo.InvariantCulture))
                    .Set("files", moved.Select(rel => (object)new JObj().Set("path", rel).Set("sha256", LevelInstaller.Sha256(Full(content, rel)))).ToList());
                File.WriteAllText(manifestPath, Json.Dumps(manifest, 1) + "\n", new UTF8Encoding(false));
                string registry = Path.Combine(db, LevelInstaller.Registry);
                WriteRegistry(registry, LevelInstaller.MergeRegistry(File.Exists(registry) ? File.ReadAllText(registry) : null, new[] { installed.entry }));
                if (packed)
                {   // the old import's archive: every file of the level is loose now, so its leftovers (files the new conversion doesn't write) must not
                    // stay visible under them; PACK GAME FILES packs the level again
                    try { RxFiles.Release(packedArchive); File.Delete(packedArchive); RxFiles.Reset(); }
                    catch (Exception e) when (e is IOException || e is UnauthorizedAccessException) { ReflexAssetImport.DefaultLog?.Invoke("[alive] the old packed copy " + packedArchive + " could not be removed: " + e.Message); }
                }
                res.converted = conv.converted.Count; res.pending = conv.pending.Count; res.files = moved.Count; res.bytes = bytes;
                res.resources = (int)(long)installed.entry.Get("resources"); res.deferredEntities = installed.deferred; res.starts = (int)(long)installed.entry.Get("starts");
                Report("Done", 1, 1);
            }
            catch
            {
                // a failed or cancelled level leaves nothing behind (its staged files and anything already published)
                foreach (var rel in moved) { try { File.Delete(Full(content, rel)); } catch (Exception) { } }
                try { if (File.Exists(manifestPath) && !File.Exists(Path.Combine(stage, "previous-manifest.json")) && previous == null && moved.Count > 0) File.Delete(manifestPath); } catch (Exception) { }
                foreach (var rel in setAside) { try { File.Move(Full(Path.Combine(stage, "previous"), rel), Full(content, rel)); } catch (Exception) { } }
                try { string pm = Path.Combine(stage, "previous-manifest.json"); if (File.Exists(pm)) { if (File.Exists(manifestPath)) File.Delete(manifestPath); File.Move(pm, manifestPath); } } catch (Exception) { }
                throw;
            }
            finally
            {
                try { if (Directory.Exists(stage)) Directory.Delete(stage, true); } catch (Exception) { }
            }
        }

        /// The registry rewritten (a temporary file, then replaced). A registry linked from elsewhere (an overlay of another install) is read
        /// through, then replaced by our own file: removing the link never touches what it points at.
        static void WriteRegistry(string registry, string text)
        {
            string tmp = registry + ".tmp-" + Guid.NewGuid().ToString("N").Substring(0, 6);
            File.WriteAllText(tmp, text, new UTF8Encoding(false));
            if (IsLink(registry)) File.Delete(registry);
            if (File.Exists(registry)) File.Replace(tmp, registry, null); else File.Move(tmp, registry);
        }

        /// The menu pictures (AlivePictures) of the levels this importer installed before it wrote them, from the sources just scanned: each
        /// picture is written next to the level's files and added to the level's import manifest and registry entry; nothing is converted
        /// again and no other file changes. A level whose entry already names its pictures, or one installed by another installer, is left
        /// as it is. Returns the levels given pictures (also in found.picturesAdded).
        public static List<string> AddPictures(AliveSources found, string content, CancellationToken cancel)
        {
            var added = new List<string>();
            if (found == null || ContentProblem(content) != null) return added;
            content = Path.GetFullPath(content);
            string db = Path.Combine(content, "Database"), registry = Path.Combine(db, LevelInstaller.Registry);
            if (!File.Exists(registry)) return added;
            var reg = Json.Parse(File.ReadAllText(registry)) as JObj;
            Bin.Require(reg != null && reg.Get("levels") is List<object>, "Unreadable " + LevelInstaller.Registry);
            foreach (var src in found.levels.Where(l => l.recognized && l.pictures != null && l.pictures.Count > 0))
            {
                cancel.ThrowIfCancellationRequested();
                string level = src.level, manifestPath = Path.Combine(db, ManifestPrefix + level + ".json");
                var entry = ((List<object>)reg.Get("levels")).OfType<JObj>().FirstOrDefault(e => e.Get("level") as string == level);
                // (a packed level's manifest and pictures are in its archive: RxFiles; what this writes is loose and wins)
                if (entry == null || !Installed(content, level) || !RxFiles.Exists(manifestPath)) continue;
                if (Pictures.Kinds.All(k => !src.pictures.ContainsKey(k) || entry.Get(k) is string f && RxFiles.Exists(Path.Combine(db, f)))) continue;
                var manifest = Json.Parse(RxFiles.ReadAllText(manifestPath)) as JObj;
                if (manifest == null || !(manifest.Get("files") is List<object> files)) continue;
                bool any = false;
                foreach (var (kind, data) in Pictures.Of(src, cancel))
                {
                    string rel = "Database/" + $"alive-{level}-{kind}.texture", dest = Full(content, rel);
                    if (entry.Get(kind) is string f && RxFiles.Exists(Path.Combine(db, f))) continue;
                    if (RxFiles.Exists(dest))
                    {
                        // never replaces a file; one the manifest lists unchanged (an earlier pass whose registry was not written) is named again
                        string sha; using (var h = System.Security.Cryptography.SHA256.Create()) sha = Bin.Hex(h.ComputeHash(RxFiles.ReadAllBytes(dest)), 0, 32);
                        if (files.Any(x => x is JObj o && o.Get("path") as string == rel && o.Get("sha256") as string == sha)) { entry.Set(kind, $"alive-{level}-{kind}.texture"); any = true; }
                        continue;
                    }
                    RequireNoLink(content, rel);
                    string tmp = dest + ".tmp-" + Guid.NewGuid().ToString("N").Substring(0, 6);
                    File.WriteAllBytes(tmp, data); File.Move(tmp, dest);
                    files.RemoveAll(x => x is JObj o && o.Get("path") as string == rel);
                    files.Add(new JObj().Set("path", rel).Set("sha256", LevelInstaller.Sha256(dest)));
                    entry.Set(kind, $"alive-{level}-{kind}.texture");
                    any = true;
                }
                if (!any) continue;
                RequireNoLink(content, "Database/" + ManifestPrefix + level + ".json");
                File.WriteAllText(manifestPath, Json.Dumps(manifest, 1) + "\n", new UTF8Encoding(false));
                added.Add(level);
            }
            if (added.Count > 0) WriteRegistry(registry, Json.Dumps(reg, 1) + "\n");
            found.picturesAdded.Clear(); found.picturesAdded.AddRange(added);
            return added;
        }

        static bool IsLink(string path)
        {
            try { return ReflexFs.IsLink(path); }
            catch (FileNotFoundException) { return false; }
            catch (DirectoryNotFoundException) { return false; }
        }

        /// No existing part of content/rel is a symbolic link (or junction): a content folder assembled from links to another install
        /// (an overlay) receives new real folders and files, and nothing is ever written through a link into the linked install.
        static void RequireNoLink(string content, string rel)
        {
            string at = content, sofar = "";
            foreach (var part in rel.Split('/'))
            {
                at = Path.Combine(at, part); sofar = sofar.Length == 0 ? part : sofar + "/" + part;
                if (!File.Exists(at) && !Directory.Exists(at) && !IsLink(at)) return;   // the rest is created by the importer
                Bin.Require(!IsLink(at), "Would write through a symbolic link into another folder: " + sofar);
            }
        }

        /// Windows: every resource path of the level (staged and published) fits in 260 characters.
        static void CheckPathLengths(AliveLevelSource src, string root, CancellationToken cancel)
        {
            var tree = Bxml.Parse(src.volume.Read(src.File("database"), cancel)).ToElement();
            foreach (var package in tree.FindAll("Packages").SelectMany(p => p.FindAll("Package")))
                foreach (var asset in package.FindAll("Asset"))
                {
                    string name = asset.Get("name"), kind = asset.Get("type"), pkg = package.Get("name");
                    if (string.IsNullOrEmpty(name) || string.IsNullOrEmpty(kind) || string.IsNullOrEmpty(pkg)) continue;
                    string rel = "Assets/" + Converters.Segment(kind) + "/" + src.level + "/" + Converters.Segment(pkg) + "/" + Converters.Segment(name) + "." + Converters.Segment(kind);
                    Bin.Require(Full(root, rel).Length < 260, "The client folder is too deeply nested for the level's files; move the client to a shorter path, such as " + ReflexPaths.ShortFolderExample(root));
                }
        }

        /// The files of the previous import of a level (its manifest), each unchanged since it was written.
        static List<string> PreviousFiles(string content, string manifestPath)
        {
            var m = Json.Parse(RxFiles.ReadAllText(manifestPath)) as JObj;   // (in its level's archive when packed)
            Bin.Require(m != null && m.Get("files") is List<object>, "Unreadable import manifest " + Path.GetFileName(manifestPath));
            var o = new List<string>();
            foreach (var x in (List<object>)m.Get("files"))
            {
                var f = x as JObj; string rel = f?.Get("path") as string, sha = f?.Get("sha256") as string;
                Bin.Require(rel != null && sha != null && !rel.Contains("..") && !Path.IsPathRooted(rel), "Unreadable import manifest " + Path.GetFileName(manifestPath));
                string p = Full(content, rel);
                if (!File.Exists(p)) continue;
                Bin.Require(LevelInstaller.Sha256(p) == sha, rel + " changed since it was imported; remove the level's files by hand to import it again");
                o.Add(rel);
            }
            return o;
        }
    }
}

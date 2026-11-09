// Community gear and bike kit packs (w-gearmods): the original PC game's "DLC" kit packs, as the community makes them, used as add-ons next
// to the stock gear and kits - never a replacement, any number at once (the brief: "integrated like the mod bikes where we remove the limits").
//
// What such a pack is (core/plan/QUEUE.md "w-gearmods" has the table dumps): eight files named after one DLC slot, MXTables_DLC0NN,
// MXUI_DLC0NN and the slot's graphics kit database with its HR twin - RiderDLC0NN_graphicskit (rider gear) or MXDLC0NN_graphicskit (a bike's
// graphics kit). MXTables_DLC0NN holds the rows the game merges into its tables: StoreProducts (a line product and its colours / styles; a
// colour often joins a STOCK line, e.g. ParentStoreProductId 2291 = Fox's jerseys), StoreAssets, RiderGear (RiderGearTypeId, the stock gear
// MODEL package, the pack's MATERIAL package) or VehiclePart (type 11 graphics kit: the stock chassis package, the pack's kit materials) +
// VehicleModelParts (which stock VehicleModelId it fits), AssetPackage / AssetDatabase / AssetPackageDatabase (the pack's packages), Localization
// (STR_<n> keys whose text is in MXUI_DLC0NN's MXTables_DLC0NN_StoreProducts string tables), DLC / DLCUnlocks / DLCPurchases (owned at once
// in the original: a found DLC unlocks its products at sign-in). In the original game the files are dropped into Database, so two packs of one
// slot (4 of the 7 gear packs here use DLC010) overwrite each other, and their ids collide (every DLC010 pack numbers its rows 3030.., 6250..).
//
// Here every pack is its own namespace (like BikeMods): its databases are read through "mod:kit/<content hash>>RiderDLC010_graphicskit", so
// nothing of a pack is cached under another pack's or the game's names; its rows are renumbered into ids of their own (Catalog: 20,000,000 and
// up, stable per pack content and row for the run), so any number of packs of any slot sit side by side. The renumbered rows never enter the
// game's own tables (ReflexEvents.Tables, which the core's profile logic reads); the garage and the spec builders read them through Source(), and
// the player's choice of a pack's piece is stored in the client save as a token "g:<hash>:<RiderGearId>" / "k:<hash>:<VehiclePartId>" (a pack
// that is gone falls back to the stock piece the profile has equipped: KitPacks.Choice.cs). The pack's line products merge by name across packs
// (five "AlpineStars" kit lines of five packs make one line); a colour of a stock line joins that line.
//
// Install (players): <client root>/Mods/Gear/ (also Mods/Skins/ when it exists) - drop each download as it is: a folder, a zip / RAR / 7z (an
// archive inside an archive too: TrackArchives installs each one into a folder of its own), any depth. A pack that cannot be read is listed with
// the file and the reason (Problems) and skipped; nothing else is affected. A pack the player installed the original way, into the game's
// Database folder, stays what the original makes of it: merged into the game's tables (by the core and by ReflexEvents.Tables, one row per id),
// one pack per slot; ScanInstall can read one as a pack (the import's check, tests).
// Nothing here uses Unity (tests/gearmods builds it with RfxData on .NET): the save, the folders and the logs are KitPacks.Unity.cs.
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using System.Text;

namespace Reflex.Port.Content
{
    /// One gear / kit pack (one MXTables_<slot> with its databases).
    public sealed class KitPack
    {
        public string id;              // where it is: the folder relative to its Mods folder ("2020TMMX450/2021 TM MX450"), "Database/<MXTables name>" for one in the game's Database
        public string name;            // what the menus call it: mod.ini name, else the folder's name
        public string author = "", description = "";
        public string folder;          // its folder
        public string tables;          // its MXTables database's name ("MXTables_DLC010"), the slot is the rest
        public string hash;            // content identity (12 hex): the same files anywhere are the same pack (the save names it by this)
        public bool fromInstall;       // put into the game's Database folder the original way
        public bool builtIn;           // the game's own content offered as a pack (KitPacks.Geico.cs): DLC, not a MOD
        public string Slot => tables != null && tables.StartsWith("MXTables_", StringComparison.OrdinalIgnoreCase) ? tables.Substring(9) : tables;
        public string Namespace => BikeMods.NamespaceMark + KitPacks.IdMark + hash + ">";
        public readonly List<string> databases = new List<string>();     // its databases (base names), as the folder has them
        public readonly List<KitItem> items = new List<KitItem>();
        public readonly List<string> notes = new List<string>();          // what was skipped inside the pack (a row whose package is missing ...)
        internal Rfx.Install install;
        internal Dictionary<string, List<Dictionary<string, object>>> rows;
        internal Dictionary<string, Dictionary<string, string>> strings;  // language ("ENG") -> key -> text
        public bool HasGear => items.Any(i => !i.bike);
        public bool HasKits => items.Any(i => i.bike);
        public string Kind => HasGear && HasKits ? "rider gear + bike kits" : HasKits ? "bike kits" : "rider gear";
    }

    /// A piece of a pack: rider gear (a RiderGear row) or a bike's graphics kit (a VehiclePart row, type 11 - or another part type a pack adds).
    public sealed class KitItem
    {
        public KitPack pack;
        public bool bike;
        public int rowId;              // the pack's own RiderGearId / VehiclePartId
        public int id;                 // the renumbered id (Catalog), unique among all packs and the game's
        public int type;               // RiderGearTypeId / VehiclePartTypeId
        public int product, asset, parent;   // renumbered StoreProductId / StoreAssetId; parent: a stock line's StoreProductId, a merged pack line's id, -1 none
        public int brandId = -1, iconId = -1, price, unlockRequirement;
        public bool hidden, invertText;
        public string title = "", lineTitle = "";
        public int[] models = new int[0];    // a bike kit: the VehicleModelIds it fits (VehicleModelParts)
        public (string db, string pkg)? material, model;   // the namespaced material package; the model package (a stock one: its plain name)
        public string Token => (bike ? "k:" : "g:") + pack.hash + ":" + rowId;
        public override string ToString() => $"{(bike ? "kit" : "gear")} type {type} #{rowId} -> {id} '{title}' line {parent} '{lineTitle}' brand {brandId} price {price}"
            + (bike ? " models [" + string.Join(",", models) + "]" : "") + $" mat {material?.db}/{material?.pkg} model {model?.db}/{model?.pkg}";
    }

    public static partial class KitPacks
    {
        /// The id part of a pack's namespace: "mod:kit/<hash>>..." (BikeMods.Split gives "kit/<hash>"; BikeMods.InstallOf asks InstallOf).
        public const string IdMark = "kit/";
        /// The renumbered ids start here (the game's own ids stay below 10,000).
        public const int IdBase = 20_000_000;
        public static bool IsKitId(int id) => id >= IdBase;
        public static Action<string> Log = s => { }, Warn = s => { };

        /// The catalog in use (KitPacks.Unity.cs: the scan of Mods/Gear and the Database folder; tests set it).
        public static Catalog Active;
        /// The install of a pack's namespace id ("kit/<hash>": BikeMods.InstallOf routes "mod:kit/..>" database names here), null for none.
        internal static Rfx.Install InstallOf(string id)
        {
            var c = Active; if (c == null || id == null || !id.StartsWith(IdMark, StringComparison.Ordinal)) return null;
            string h = id.Substring(IdMark.Length);
            foreach (var p in c.packs) if (p.hash == h) return p.install;
            return null;
        }

        const string DbExt = ".dx9.database", PkgExt = ".dx9.package";
        const int MaxDepth = 10;

        // ------------------------------------------------------------------------------------------------ the scan

        /// A problem of a pack or an archive in a Mods folder: what (the file / folder) and why; shown in the garage.
        public sealed class Problem { public string where, why; public override string ToString() => where + ": " + why; }

        /// The packs under root (pure file system walk): every MXTables_*.dx9.database with its .dx9.package is one pack, read with the other
        /// databases of its folder. archive(path) is asked about each archive file found (the Unity side installs it; tests: null = ignore) and
        /// returns a problem to list or null. Junk and dot folders (".installing") are skipped. Ordered by id.
        public static List<KitPack> Scan(string root, List<Problem> problems, Func<string, string> archive = null)
        {
            var packs = new List<KitPack>();
            if (string.IsNullOrEmpty(root) || !Directory.Exists(root)) return packs;
            void Walk(string dir, int depth)
            {
                string[] files, dirs;
                try { files = Directory.GetFiles(dir); dirs = Directory.GetDirectories(dir); }
                catch (Exception e) when (e is IOException || e is UnauthorizedAccessException) { problems?.Add(new Problem { where = dir, why = "not readable (" + e.Message + ")" }); return; }
                var byLower = Files(files);
                {   // (packs loose in the Mods folder itself count too: a pack is its files, not its folder)
                    var tables = byLower.Keys.Where(k => k.StartsWith("mxtables_", StringComparison.Ordinal) && k.EndsWith(DbExt, StringComparison.Ordinal)).OrderBy(k => k, StringComparer.Ordinal).ToList();
                    foreach (var t in tables)
                    {
                        string baseName = Path.GetFileName(byLower[t]); baseName = baseName.Substring(0, baseName.Length - DbExt.Length);
                        if (!byLower.ContainsKey(t.Substring(0, t.Length - DbExt.Length) + PkgExt)) { problems?.Add(new Problem { where = byLower[t], why = "the " + baseName + PkgExt + " beside it is missing (the download may be incomplete)" }); continue; }
                        string rel = ReflexMods.RelativeId(root, dir) ?? Path.GetFileName(dir);
                        if (string.IsNullOrEmpty(rel) || rel == ".") rel = baseName;
                        var p = new KitPack { id = tables.Count > 1 ? rel + "#" + baseName : rel, name = depth == 0 ? baseName : Path.GetFileName(dir.TrimEnd('/', '\\')), folder = dir, tables = baseName };
                        string why = Load(p, byLower, extracted: false);
                        if (why != null) problems?.Add(new Problem { where = Path.Combine(dir, baseName + DbExt), why = why });
                        else packs.Add(p);
                    }
                    if (archive != null)
                        foreach (var f in byLower.Values.OrderBy(x => x, StringComparer.OrdinalIgnoreCase))
                        {
                            string ext = Path.GetExtension(f).ToLowerInvariant();
                            if (!TrackArchives.Extensions.Contains(ext) && TrackArchives.Sniff(f) == TrackArchives.Kind.None) continue;
                            if (f.EndsWith(DbExt, StringComparison.OrdinalIgnoreCase) || f.EndsWith(PkgExt, StringComparison.OrdinalIgnoreCase)) continue;
                            string why = archive(f);
                            if (why != null) problems?.Add(new Problem { where = f, why = why });
                        }
                }
                if (depth >= MaxDepth) return;
                foreach (var d in dirs.OrderBy(x => x, StringComparer.OrdinalIgnoreCase))
                    if (!ReflexMods.IsJunk(Path.GetFileName(d))) Walk(d, depth + 1);
            }
            Walk(root, 0);
            packs.Sort((a, b) => string.Compare(a.id, b.id, StringComparison.OrdinalIgnoreCase));
            return packs;
        }

        static Dictionary<string, string> Files(IEnumerable<string> files)
        {
            var byLower = new Dictionary<string, string>(StringComparer.Ordinal);
            foreach (var f in files) { string n = Path.GetFileName(f); if (!ReflexMods.IsJunk(n)) byLower[n.ToLowerInvariant()] = f; }
            return byLower;
        }

        /// The packs put into the game's (imported) Database folder the original way: an MXTables database of a DLC name whose AssetDatabase names a
        /// "*_graphicskit" database (the community kit tools' naming; the game's own DLC tables name none). extracted: the folder is the import's
        /// copy (assets read where the import extracted them). A table that cannot be read is a problem, not a failure. (The game merges such a
        /// pack into its tables; this reads one as a pack of its own: the import's check and tests.)
        public static List<KitPack> ScanInstall(string dbDir, bool extracted, List<Problem> problems)
        {
            var packs = new List<KitPack>();
            if (string.IsNullOrEmpty(dbDir) || !Directory.Exists(dbDir)) return packs;
            Dictionary<string, string> byLower;
            try { byLower = Files(Directory.GetFiles(dbDir, "*" + DbExt).Concat(extracted ? Enumerable.Empty<string>() : Directory.GetFiles(dbDir, "*" + PkgExt))); }
            catch (Exception e) when (e is IOException || e is UnauthorizedAccessException) { return packs; }
            foreach (var t in byLower.Keys.Where(k => k.StartsWith("mxtables_", StringComparison.Ordinal) && k.Contains("dlc") && k.EndsWith(DbExt, StringComparison.Ordinal)).OrderBy(k => k, StringComparer.Ordinal))
            {
                string baseName = Path.GetFileName(byLower[t]); baseName = baseName.Substring(0, baseName.Length - DbExt.Length);
                var p = new KitPack { id = "Database/" + baseName, name = baseName.Replace("MXTables_", "") + " (Database folder)", folder = dbDir, tables = baseName, fromInstall = true };
                Dictionary<string, List<Dictionary<string, object>>> rows;
                try { rows = ReadTables(new Rfx.Install(dbDir, "mod:kit/probe>", byLower, extracted), "mod:kit/probe>", baseName); }
                catch (Exception e) when (!(e is OutOfMemoryException)) { problems?.Add(new Problem { where = Path.Combine(dbDir, baseName + DbExt), why = "a DLC table this version cannot read (" + e.Message + "): skipped" }); continue; }
                bool kitTool = rows.TryGetValue("AssetDatabase", out var dbs) && dbs.Any(r => (Rfx.Str(r, "DatabaseName") ?? "").EndsWith("_graphicskit", StringComparison.OrdinalIgnoreCase));
                if (!kitTool) continue;
                string why = Load(p, byLower, extracted);
                if (why != null) problems?.Add(new Problem { where = Path.Combine(dbDir, baseName + DbExt), why = why });
                else packs.Add(p);
            }
            return packs;
        }

        /// A pack's merged table rows (every tbldata asset of its MXTables database).
        static Dictionary<string, List<Dictionary<string, object>>> ReadTables(Rfx.Install install, string ns, string tables)
        {
            var db = install.Db(ns + tables) ?? throw new InvalidDataException(tables + DbExt + " did not load");
            var t = new Dictionary<string, List<Dictionary<string, object>>>(StringComparer.Ordinal);
            int n = 0;
            foreach (var pkg in db.packages.Values.OrderBy(p => p.name, StringComparer.Ordinal))
                foreach (var a in pkg.OfType("tbldata"))
                {
                    n++;
                    foreach (var kv in Rfx.Tables(install.Read(a)))
                    {
                        if (!t.TryGetValue(kv.Key, out var list)) t[kv.Key] = list = new List<Dictionary<string, object>>();
                        list.AddRange(kv.Value);
                    }
                }
            if (n == 0) throw new InvalidDataException(tables + DbExt + " holds no table data (tbldata)");
            return t;
        }

        // ------------------------------------------------------------------------------------------------ archives

        /// How TrackArchives installs a gear / kit pack archive: into a folder of its own next to it; it must hold a pack, or archives of packs
        /// (a download that bundles several packs as archives: each is installed in turn, into its own folder inside).
        public static readonly TrackArchives.Profile ArchiveProfile = new TrackArchives.Profile { folder = "Mods/Gear", what = "a gear or kit pack", tag = "[KitPacks]", validate = Validate };

        /// Null when dir holds a pack (an MXTables_<slot>.dx9.database with its .dx9.package) or an archive; else what it holds instead.
        public static (string error, string detail) Validate(string dir)
        {
            string root = Path.GetFullPath(dir).TrimEnd(Path.DirectorySeparatorChar) + Path.DirectorySeparatorChar;
            var files = Directory.GetFiles(dir, "*", SearchOption.AllDirectories)
                .Where(f => !Path.GetFullPath(f).Substring(root.Length).Split(Path.DirectorySeparatorChar, '/').Any(ReflexMods.IsJunk)).ToList();
            var low = new HashSet<string>(files.Select(f => f.ToLowerInvariant()), StringComparer.Ordinal);
            var tables = files.Where(f => { string n = Path.GetFileName(f).ToLowerInvariant(); return n.StartsWith("mxtables_", StringComparison.Ordinal) && n.EndsWith(DbExt, StringComparison.Ordinal)
                                              && low.Contains(f.Substring(0, f.Length - DbExt.Length).ToLowerInvariant() + PkgExt); }).ToList();
            if (tables.Count > 0) return (null, string.Join(", ", tables.Select(f => Path.GetFileName(f).Substring(0, Path.GetFileName(f).Length - DbExt.Length))));
            var archives = files.Where(f => TrackArchives.Sniff(f) != TrackArchives.Kind.None).ToList();
            if (archives.Count > 0) return (null, "archives: " + string.Join(", ", archives.Select(Path.GetFileName)));
            if (TrackArchives.Tracks(dir, out _) == null) return ("holds a custom track, not a gear or kit pack: put it in Mods/Tracks", "");
            if (files.Any(f => f.EndsWith(DbExt, StringComparison.OrdinalIgnoreCase)))
                return ("holds .dx9 files but no MXTables_<slot> table: a bike mod goes in Mods/Bikes; a gear or kit pack has MXTables_DLC0NN.dx9.database / .package files", "");
            return ("holds no gear or kit pack (no MXTables_<slot>.dx9.database + .dx9.package)" + (files.Count == 0 ? ": it is empty" : ""), "");
        }

        // ------------------------------------------------------------------------------------------------ one pack

        /// Reads a pack: its tables, its strings, its pieces (null: fine; else why it cannot be used). files: its folder's files by lower-case name.
        internal static string Load(KitPack p, Dictionary<string, string> files, bool extracted)
        {
            try
            {
                ReadInfo(p, files);
                string probe = BikeMods.NamespaceMark + IdMark + "probe" + Guid.NewGuid().ToString("N").Substring(0, 8) + ">";
                var probeInstall = new Rfx.Install(p.folder, probe, files, extracted);
                Dictionary<string, List<Dictionary<string, object>>> rows;
                try { rows = ReadTables(probeInstall, probe, p.tables); }
                catch (Exception e) when (e is InvalidDataException || e is IOException || e is InvalidCastException || e is FormatException || e is OverflowException || e is ArgumentException || e is KeyNotFoundException || e is IndexOutOfRangeException || e is NullReferenceException)
                { return "its table (" + p.tables + DbExt + ") cannot be read: " + e.Message; }
                finally { probeInstall.Close(); }
                p.rows = rows;
                // the pack's databases: those its AssetDatabase rows name that the folder has (+ their HR twins), and its MXUI strings database
                var names = Rows(rows, "AssetDatabase").Select(r => Rfx.Str(r, "DatabaseName")).Where(n => !string.IsNullOrEmpty(n)).ToList();
                foreach (string n in names.Concat(names.Select(n => "HR_" + n)).Append("MXUI_" + p.Slot).Append(p.tables))
                    if (files.ContainsKey(n.ToLowerInvariant() + DbExt) && (extracted || files.ContainsKey(n.ToLowerInvariant() + PkgExt)) && !p.databases.Contains(n, StringComparer.OrdinalIgnoreCase))
                        p.databases.Add(Path.GetFileName(files[n.ToLowerInvariant() + DbExt]).Substring(0, n.Length));
                p.hash = Hash(p, files);
                p.install = new Rfx.Install(p.folder, p.Namespace, files, extracted);
                p.strings = ReadStrings(p);
                Items(p);
                if (p.items.Count == 0)
                    return Rows(rows, "RiderGear").Count == 0 && Rows(rows, "VehiclePart").Count == 0 ? "its table declares no rider gear and no bike kit (no RiderGear / VehiclePart rows)"
                         : "none of its pieces can be used: " + string.Join("; ", p.notes.Take(3));
                return null;
            }
            catch (Exception e) when (e is IOException || e is UnauthorizedAccessException || e is InvalidDataException || e is ArgumentException)
            { return "it cannot be read: " + e.Message; }
        }

        static List<Dictionary<string, object>> Rows(Dictionary<string, List<Dictionary<string, object>>> t, string table) => t != null && t.TryGetValue(table, out var r) ? r : new List<Dictionary<string, object>>();

        /// The pack's content identity: its databases' bytes (they list every asset's place and size) and its table / string packages' bytes.
        static string Hash(KitPack p, Dictionary<string, string> files)
        {
            using (var sha = SHA1.Create())
            {
                void Add(byte[] b) => sha.TransformBlock(b, 0, b.Length, null, 0);
                foreach (string n in p.databases.OrderBy(x => x, StringComparer.OrdinalIgnoreCase))
                {
                    Add(Encoding.UTF8.GetBytes(n.ToLowerInvariant() + "\n"));
                    if (files.TryGetValue(n.ToLowerInvariant() + DbExt, out var db)) Add(File.ReadAllBytes(db));
                    bool small = n.StartsWith("MXTables_", StringComparison.OrdinalIgnoreCase) || n.StartsWith("MXUI_", StringComparison.OrdinalIgnoreCase);
                    if (files.TryGetValue(n.ToLowerInvariant() + PkgExt, out var pk))
                    {
                        long len = new FileInfo(pk).Length;
                        if (small && len <= 4 << 20) Add(File.ReadAllBytes(pk)); else Add(BitConverter.GetBytes(len));
                    }
                }
                sha.TransformFinalBlock(new byte[0], 0, 0);
                return string.Concat(sha.Hash.Take(6).Select(b => b.ToString("x2")));
            }
        }

        /// The string tables of the pack's LocalizationAsset rows (MXTables_DLC010_StoreProducts in MXUI_DLC010's packages MXUI_DLC010.ENG ...):
        /// language -> key -> text. A pack's MXUI often names the asset without the language (each language package holds one of that name).
        static Dictionary<string, Dictionary<string, string>> ReadStrings(KitPack p)
        {
            var d = new Dictionary<string, Dictionary<string, string>>(StringComparer.OrdinalIgnoreCase);
            var assets = Rows(p.rows, "LocalizationAsset").Select(r => Rfx.Str(r, "LocalizationAsset")).Where(n => !string.IsNullOrEmpty(n)).ToList();
            if (assets.Count == 0) assets.Add(p.tables + "_StoreProducts");
            foreach (string db in p.databases)
            {
                Rfx.Database database;
                try { database = p.install.Db(p.Namespace + db); } catch (Exception e) when (e is InvalidDataException || e is IOException) { p.notes.Add(db + ": " + e.Message); continue; }
                if (database == null) continue;
                foreach (var pkg in database.packages.Values)
                    foreach (var a in pkg.OfType("localiz"))
                    {
                        string an = a.name; string lang = null;
                        foreach (string want in assets)
                        {
                            if (string.Equals(an, want, StringComparison.OrdinalIgnoreCase)) { int dot = pkg.name.LastIndexOf('.'); lang = dot >= 0 ? pkg.name.Substring(dot + 1) : "ENG"; }
                            else if (an.StartsWith(want + ".", StringComparison.OrdinalIgnoreCase)) lang = an.Substring(want.Length + 1);
                        }
                        if (lang == null) continue;
                        try
                        {
                            if (!d.TryGetValue(lang, out var table)) d[lang] = table = new Dictionary<string, string>(StringComparer.Ordinal);
                            foreach (var kv in Rfx.Strings(p.install.Read(a))) table[kv.Key] = kv.Value;
                        }
                        catch (Exception e) when (e is InvalidDataException || e is IOException) { p.notes.Add(db + "/" + pkg.name + "/" + an + ": " + e.Message); }
                    }
            }
            return d;
        }

        /// A pack string of a key in a language (else English, else any).
        public static string Str(KitPack p, string key, string language = "ENG")
        {
            if (p?.strings == null || string.IsNullOrEmpty(key)) return null;
            if (language != null && p.strings.TryGetValue(language, out var t) && t.TryGetValue(key, out var s) && !string.IsNullOrWhiteSpace(s)) return s.Trim();
            if (p.strings.TryGetValue("ENG", out t) && t.TryGetValue(key, out s) && !string.IsNullOrWhiteSpace(s)) return s.Trim();
            foreach (var tt in p.strings.Values) if (tt.TryGetValue(key, out s) && !string.IsNullOrWhiteSpace(s)) return s.Trim();
            return null;
        }

        /// What a piece is called in the menus: its pack string, unless that is missing or a left-over asset name ("MX_KawasakiF_Plastic"),
        /// then the pack's name with its words spaced ("ReplicaKenRoczenSXParis2022" -> "Replica Ken Roczen SX Paris 2022").
        public static string ShownTitle(string title, string packName)
        {
            if (!string.IsNullOrWhiteSpace(title) && !System.Text.RegularExpressions.Regex.IsMatch(title, @"^[A-Za-z0-9]+(_[A-Za-z0-9]+)+$")) return title;
            if (string.IsNullOrWhiteSpace(packName)) return title;
            string spaced = System.Text.RegularExpressions.Regex.Replace(packName.Replace('_', ' ').Replace('.', ' '), @"(?<=[a-z])(?=[A-Z0-9])|(?<=[0-9])(?=[A-Za-z])|(?<=[A-Z])(?=[A-Z][a-z])", " ");
            return System.Text.RegularExpressions.Regex.Replace(spaced, @"\s+", " ").Trim();
        }

        /// The pack's pieces from its rows (ids still the pack's own: Catalog renumbers them).
        static void Items(KitPack p)
        {
            var t = p.rows;
            // a pack may repeat an id (Distractions' and the Jett Lawrence pack's StoreProductId 3650 is both the unused line and the helmet):
            // a reference picks the row that is a piece of a line (ParentStoreProductId set), as the asset that names it means that one
            Dictionary<int, Dictionary<string, object>> ById(string table, string col, Func<Dictionary<string, object>, int> prefer = null)
            {
                var d = new Dictionary<int, Dictionary<string, object>>();
                foreach (var r in Rows(t, table))
                {
                    int k = Rfx.Int(r, col, int.MinValue); if (k == int.MinValue) continue;
                    if (!d.TryGetValue(k, out var old) || prefer != null && prefer(r) > prefer(old)) d[k] = r;
                }
                return d;
            }
            var products = ById("StoreProducts", "StoreProductId", r => Rfx.Int(r, "ParentStoreProductId", -1) >= 0 ? 1 : 0);
            var assets = ById("StoreAssets", "StoreAssetId");
            var apd = ById("AssetPackageDatabase", "PackageDatabaseId");
            var pkgs = ById("AssetPackage", "PackageId");
            var dbs = ById("AssetDatabase", "DatabaseId");
            var loc = ById("Localization", "LocalizationId");
            (string db, string pkg)? Package(int id, out bool own)
            {
                own = apd.TryGetValue(id, out var r);
                if (!own) return null;   // not the pack's: a stock package (resolved through the game's tables)
                string db = Rfx.Str(dbs.TryGetValue(Rfx.Int(r, "DatabaseId"), out var dr) ? dr : null, "DatabaseName");
                string pkg = Rfx.Str(pkgs.TryGetValue(Rfx.Int(r, "PackageId"), out var pr) ? pr : null, "PackageName");
                if (string.IsNullOrEmpty(db) || string.IsNullOrEmpty(pkg)) return null;
                string mine = p.databases.FirstOrDefault(x => string.Equals(x, db, StringComparison.OrdinalIgnoreCase));
                return mine != null ? (p.Namespace + mine, pkg) : (db, pkg);
            }
            string Title(Dictionary<string, object> product)
            {
                var l = loc.TryGetValue(Rfx.Int(product, "LocalizationId"), out var lr) ? lr : null;
                return Str(p, Rfx.Str(l, "LocalizationKey")) ?? Str(p, "STR_" + Rfx.Int(product, "StoreProductId"));
            }
            void Add(Dictionary<string, object> row, bool bike)
            {
                int rowId = Rfx.Int(row, bike ? "VehiclePartId" : "RiderGearId", int.MinValue);
                string what = (bike ? "VehiclePart " : "RiderGear ") + rowId;
                if (rowId == int.MinValue) { p.notes.Add((bike ? "a VehiclePart" : "a RiderGear") + " row without an id"); return; }
                if (p.items.Any(i => i.bike == bike && i.rowId == rowId)) { p.notes.Add(what + " is listed twice: the first kept"); return; }
                int productId = bike ? Rfx.Int(row, "StoreProductId") : Rfx.Int(assets.TryGetValue(Rfx.Int(row, "StoreAssetId"), out var ar) ? ar : null, "StoreProductId");
                int assetId = bike ? -1 : Rfx.Int(row, "StoreAssetId");
                if (bike)
                {   // the StoreAsset of a kit is its VehicleModelParts row's
                    var vmp = Rows(t, "VehicleModelParts").FirstOrDefault(r => Rfx.Int(r, "VehiclePartId") == rowId);
                    assetId = Rfx.Int(vmp, "StoreAssetId");
                    if (productId < 0) productId = Rfx.Int(assets.TryGetValue(assetId, out var ka) ? ka : null, "StoreProductId");
                }
                if (!products.TryGetValue(productId, out var product)) { p.notes.Add(what + ": its store product " + productId + " is not in the pack"); return; }
                var mat = Package(Rfx.Int(row, "MaterialPackageDatabaseId", -1), out bool ownMat);
                var mdl = Package(Rfx.Int(row, "ModelPackageDatabaseId", -1), out bool ownModel);
                var it = new KitItem
                {
                    pack = p, bike = bike, rowId = rowId, type = Rfx.Int(row, bike ? "VehiclePartTypeId" : "RiderGearTypeId"),
                    product = productId, asset = assetId, parent = Rfx.Int(product, "ParentStoreProductId", -1),
                    brandId = Rfx.Int(product, "BrandId", -1), iconId = Rfx.Int(product, "IconId", -1), price = Math.Max(0, Rfx.Int(product, "Price", 0)),
                    unlockRequirement = Rfx.Int(product, "UnlockRequirement", 0), hidden = Rfx.Int(product, "Hidden", 0) != 0, invertText = Rfx.Int(row, "InvertTextColors", 0) != 0,
                    title = ShownTitle(Title(product), p.name), material = mat,
                };
                // the model: a stock package keeps its id (Catalog resolves it through the game's AssetPackageDatabase), a pack's is namespaced
                it.model = ownModel ? mdl : null;
                if (ownModel && mdl == null) { p.notes.Add(what + " '" + it.title + "': its model package is named but not described in the pack"); return; }
                if (ownMat && (mat == null || !p.databases.Any(d => mat.Value.db == p.Namespace + d)))
                { p.notes.Add(what + " '" + it.title + "': its material package " + (mat?.pkg ?? "?") + " is not in the pack's files"); return; }
                if (ownMat && p.install.Pkg(mat.Value.db, mat.Value.pkg) == null) { p.notes.Add(what + " '" + it.title + "': " + BikeMods.BaseDb(mat.Value.db) + " has no package " + mat.Value.pkg); return; }
                if (it.parent >= 0 && products.TryGetValue(it.parent, out var line) && !ReferenceEquals(line, product)) it.lineTitle = Title(line) ?? "";
                if (bike) it.models = Rows(t, "VehicleModelParts").Where(r => Rfx.Int(r, "VehiclePartId") == rowId).Select(r => Rfx.Int(r, "VehicleModelId")).Where(m => m >= 0).Distinct().ToArray();
                if (bike && it.models.Length == 0) { p.notes.Add(what + " '" + it.title + "': fits no vehicle (no VehicleModelParts row)"); return; }
                p.items.Add(it);
            }
            foreach (var r in Rows(t, "RiderGear")) Add(r, false);
            foreach (var r in Rows(t, "VehiclePart")) Add(r, true);
            // a pack that names several pieces of one line alike (2020FoxFlexair: three "2020 Fox Flexair How"): numbered, so the list tells them apart
            // (kits for different vehicles are never listed together: the GEICO pack's "Black" of each chassis keeps its name)
            foreach (var g in p.items.GroupBy(i => (i.bike, i.type, i.parent, i.title.Trim().ToLowerInvariant(), string.Join(",", i.models))).Where(g => g.Count() > 1))
            {
                int n = 0;
                foreach (var it in g) it.title = it.title.Trim() + " " + ++n;
            }
        }

        /// mod.ini (name / author / description) in the pack's folder.
        static void ReadInfo(KitPack p, Dictionary<string, string> files)
        {
            if (p.fromInstall || !files.TryGetValue("mod.ini", out var ini)) return;
            try
            {
                foreach (var raw in File.ReadAllText(ini).Replace("\r", "").Split('\n'))
                {
                    string l = raw.Trim().TrimStart('﻿'); if (l.Length == 0 || l[0] == ';' || l[0] == '#' || l[0] == '[') continue;
                    int e = l.IndexOf('='); if (e <= 0) continue;
                    string k = l.Substring(0, e).Trim().ToLowerInvariant(), v = l.Substring(e + 1).Trim().Trim('"');
                    if (k == "name" && v.Length > 0) p.name = v; else if (k == "author") p.author = v; else if (k == "description") p.description = v;
                }
            }
            catch (Exception e) when (e is IOException || e is UnauthorizedAccessException) { }
        }

        // ------------------------------------------------------------------------------------------------ the catalog

        /// The renumbered rows of every pack, as the game's tables name them (the overlay Source adds them to the game's).
        public sealed class Catalog
        {
            public readonly List<KitPack> packs;
            public readonly Dictionary<string, List<Dictionary<string, object>>> tables = new Dictionary<string, List<Dictionary<string, object>>>(StringComparer.Ordinal);
            public readonly Dictionary<int, KitItem> byId = new Dictionary<int, KitItem>();
            public readonly Dictionary<string, KitItem> byToken = new Dictionary<string, KitItem>(StringComparer.Ordinal);
            /// The merged lines of the packs' own line products: id -> (title, brand, icon).
            public readonly Dictionary<int, (string title, int brand, int icon)> lines = new Dictionary<int, (string, int, int)>();
            public readonly Dictionary<int, KitItem> byProduct = new Dictionary<int, KitItem>();
            internal Catalog(List<KitPack> packs) { this.packs = packs; }
            public List<Dictionary<string, object>> Rows(string table) => tables.TryGetValue(table, out var r) ? r : null;
            internal void Add(string table, params (string col, object v)[] cells)
            {
                if (!tables.TryGetValue(table, out var list)) tables[table] = list = new List<Dictionary<string, object>>();
                var row = new Dictionary<string, object>(StringComparer.Ordinal);
                foreach (var (c, v) in cells) row[c] = v;
                list.Add(row);
            }
        }

        // the renumbering: one id per (pack content, space, own id) for the run (a rescan keeps a pack's ids, so a garage preview key stays valid)
        static readonly Dictionary<string, int> s_ids = new Dictionary<string, int>(StringComparer.Ordinal);
        static int s_next = IdBase;
        static int Id(string key) { lock (s_ids) { if (!s_ids.TryGetValue(key, out int v)) s_ids[key] = v = s_next++; return v; } }

        /// The catalog of a set of packs (a pack of the same content as an earlier one is dropped: listed once).
        public static Catalog Build(List<KitPack> packs, List<Problem> problems = null)
        {
            var seen = new Dictionary<string, KitPack>(StringComparer.Ordinal);
            var kept = new List<KitPack>();
            foreach (var p in packs)
            {
                if (seen.TryGetValue(p.hash, out var first)) { problems?.Add(new Problem { where = p.folder, why = "the same pack as " + first.folder + ": listed once" }); continue; }
                seen[p.hash] = p; kept.Add(p);
            }
            var c = new Catalog(kept);
            foreach (var p in kept)
            {
                string k = p.hash + "|";
                foreach (var it in p.items)
                {
                    it.id = Id(k + (it.bike ? "part|" : "gear|") + it.rowId);
                    int product = Id(k + "product|" + (it.bike ? "k" : "g") + it.rowId), asset = Id(k + "asset|" + (it.bike ? "k" : "g") + it.rowId);
                    int parentRaw = it.parent;
                    bool ownLine = parentRaw >= 0 && Rows(p.rows, "StoreProducts").Any(r => Rfx.Int(r, "StoreProductId") == parentRaw && Rfx.Int(r, "ParentStoreProductId", -1) < 0);
                    if (ownLine)
                    {   // the pack's own line product: one line per name and brand among all packs (each pack names its own "AlpineStars")
                        string title = string.IsNullOrWhiteSpace(it.lineTitle) ? p.name : it.lineTitle;
                        int line = Id("line|" + (it.bike ? "k" : "g") + it.type + "|" + title.Trim().ToLowerInvariant() + "|" + it.brandId);
                        if (!c.lines.ContainsKey(line)) c.lines[line] = (title.Trim(), it.brandId, it.iconId);
                        it.parent = line; it.lineTitle = title.Trim();
                    }
                    it.product = product; it.asset = asset;
                    c.byId[it.id] = it; c.byToken[it.Token] = it; c.byProduct[product] = it;
                    // the rows the spec builders read (ReflexVehicleSpecs.Rider / Bike through Source): the packages first
                    int mat = -1, mdl = -1;
                    if (it.material != null) mat = Package(c, k, it.material.Value);
                    if (it.model != null) mdl = Package(c, k, it.model.Value);
                    else mdl = Rfx.Int(Raw(p, it), "ModelPackageDatabaseId", -1);   // a stock model package: the game's own id
                    c.Add("StoreProducts", ("StoreProductId", product), ("ParentStoreProductId", it.parent), ("Hidden", it.hidden ? 1 : 0), ("Price", it.price), ("BrandId", it.brandId),
                        ("UnlockRequirement", it.unlockRequirement), ("LocalizationId", -1), ("IconId", it.iconId));
                    c.Add("StoreAssets", ("StoreAssetId", asset), ("StoreProductId", product));
                    var raw = Raw(p, it);
                    if (!it.bike)
                        c.Add("RiderGear", ("RiderGearId", it.id), ("RiderGearTypeId", it.type), ("InvertTextColors", it.invertText ? 1 : 0), ("StoreAssetId", asset),
                            ("MaterialPackageDatabaseId", mat), ("ModelPackageDatabaseId", mdl), ("HRAvailable", Rfx.Int(raw, "HRAvailable", 0)));
                    else
                    {
                        c.Add("VehiclePart", ("VehiclePartId", it.id), ("VehiclePartTypeId", it.type), ("StoreProductId", product), ("HRAvailable", Rfx.Int(raw, "HRAvailable", 0)),
                            ("DefaultTuneAttribute1", Rfx.Int(raw, "DefaultTuneAttribute1", 0)), ("DefaultTuneAttribute2", Rfx.Int(raw, "DefaultTuneAttribute2", 0)),
                            ("MaterialPackageDatabaseId", mat), ("ModelPackageDatabaseId", mdl));
                        foreach (int m in it.models) c.Add("VehicleModelParts", ("VehicleModelId", m), ("VehiclePartId", it.id), ("Default", 0), ("StoreAssetId", asset));
                    }
                }
            }
            return c;
        }

        static Dictionary<string, object> Raw(KitPack p, KitItem it) =>
            Rows(p.rows, it.bike ? "VehiclePart" : "RiderGear").FirstOrDefault(r => Rfx.Int(r, it.bike ? "VehiclePartId" : "RiderGearId") == it.rowId);

        /// A package's renumbered AssetPackageDatabase id (the database / package rows added once per pack).
        static int Package(Catalog c, string k, (string db, string pkg) p)
        {
            int apd = Id(k + "apd|" + p.db + "|" + p.pkg);
            if (c.Rows("AssetPackageDatabase")?.Any(r => Rfx.Int(r, "PackageDatabaseId") == apd) == true) return apd;
            int db = Id(k + "db|" + p.db), pkg = Id(k + "pkg|" + p.db + "|" + p.pkg);
            if (c.Rows("AssetDatabase")?.Any(r => Rfx.Int(r, "DatabaseId") == db) != true) c.Add("AssetDatabase", ("DatabaseId", db), ("DatabaseName", p.db));
            c.Add("AssetPackage", ("PackageId", pkg), ("PackageName", p.pkg));
            c.Add("AssetPackageDatabase", ("PackageDatabaseId", apd), ("PackageId", pkg), ("DatabaseId", db));
            return apd;
        }

        // ------------------------------------------------------------------------------------------------ the tables with the packs' rows

        /// The game's tables (stock) with a catalog's rows after them, for ReflexGameTables (the spec builders, the garage). Null catalog: stock.
        public static ReflexGameTables.ISource Source(ReflexGameTables.ISource stock, Catalog catalog) => catalog == null || catalog.tables.Count == 0 ? stock : new OverlaySource(stock, catalog);

        sealed class OverlaySource : ReflexGameTables.ISource
        {
            readonly ReflexGameTables.ISource stock; readonly Catalog c;
            public OverlaySource(ReflexGameTables.ISource stock, Catalog c) { this.stock = stock; this.c = c; }
            int StockRows(string table) => Math.Max(0, stock.Rows(table));
            public int Rows(string table)
            {
                int s = stock.Rows(table); var o = c.Rows(table);
                return o == null ? s : Math.Max(0, s) + o.Count;
            }
            public int[] Ints(string table, string column)
            {
                var s = stock.Ints(table, column); var o = c.Rows(table);
                if (o == null || !o.Any(r => r.ContainsKey(column))) return s;
                int n = StockRows(table);
                var a = new int[n + o.Count];
                for (int i = 0; i < n; i++) a[i] = s != null && i < s.Length ? s[i] : -1;
                for (int i = 0; i < o.Count; i++) a[n + i] = Rfx.Int(o[i], column);
                return a;
            }
            public string[] Strings(string table, string column)
            {
                var s = stock.Strings(table, column); var o = c.Rows(table);
                if (o == null || !o.Any(r => r.ContainsKey(column))) return s;
                int n = StockRows(table);
                var a = new string[n + o.Count];
                for (int i = 0; i < n; i++) a[i] = s != null && i < s.Length ? s[i] : null;
                for (int i = 0; i < o.Count; i++) a[n + i] = Rfx.Str(o[i], column);
                return a;
            }
        }

        // ------------------------------------------------------------------------------------------------ choices applied to id lists

        /// A rider's RiderGear ids with the chosen pack pieces in place of the ones of their types (typeOf: an id's RiderGearTypeId, a pack's too).
        public static int[] WithGear(IEnumerable<int> stockIds, IEnumerable<KitItem> chosen, Func<int, int> typeOf)
        {
            var ids = (stockIds ?? Enumerable.Empty<int>()).ToList();
            foreach (var it in chosen ?? Enumerable.Empty<KitItem>())
            {
                if (it == null || it.bike) continue;
                int k = ids.FindIndex(x => typeOf(x) == it.type);
                if (k >= 0) ids[k] = it.id; else ids.Add(it.id);
            }
            return ids.ToArray();
        }

        /// A vehicle's VehiclePart ids with a pack kit (or part) in place of the part of its type (typeOf: an id's VehiclePartTypeId, a pack's too).
        public static int[] WithPart(IEnumerable<int> stockIds, KitItem chosen, Func<int, int> typeOf)
        {
            var ids = (stockIds ?? Enumerable.Empty<int>()).ToList();
            if (chosen == null || !chosen.bike) return ids.ToArray();
            int k = ids.FindIndex(x => typeOf(x) == chosen.type);
            if (k >= 0) ids[k] = chosen.id; else ids.Add(chosen.id);
            return ids.ToArray();
        }

        /// One line per pack: what it is, its slot, its pieces (logs, tests).
        public static string Describe(KitPack p) =>
            $"{p.id}: \"{p.name}\" {p.Kind} slot {p.Slot} hash {p.hash}{(p.fromInstall ? " (Database folder)" : "")}: {p.items.Count} pieces"
            + (p.notes.Count > 0 ? "; skipped: " + string.Join("; ", p.notes) : "");
    }
}

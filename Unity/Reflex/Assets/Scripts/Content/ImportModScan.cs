// Mods in the game's own Database folder, found before the import (pure: System only; tests/importflow runs it against copies of real packs).
// Players install community content the original way, by copying files into the game's Database folder; Steam's uninstall and its "Verify
// integrity of game files" never remove files it did not install, so a pack stays there for good, and a pack this version cannot read used
// to stop the import ("While reading MXTables_DLC008.dx9.database: Specified cast is not valid").
// The scan compares the folder with the Steam release's (ReflexStockDatabase: names and sizes) and groups what differs:
//   GearPack / KitPack   a community rider gear or bike graphics kit pack (MXTables_DLC0NN, MXUI_DLC0NN, RiderDLC0NN_graphicskit or
//                        MXDLC0NN_graphicskit and their HR_ twins): OPEN REFLEX reads these from Mods/Gear without touching the game folder,
//                        any number at once (KitPacks); Move puts the files there
//   TrackSlot            a custom track over a beta slot (Beta_Nat_Track_Slot_4 ...): Mods/Tracks reads it (TrackMods); Move puts it there
//   Track / DlcFiles     a community track level, or DLC tables of a slot without a kit (track DLC packs): imported as they are
//   Changed              a game file with the stock name but another size (a bike mod copied over MXs.dx9.package ...): imported as it
//                        is; Steam's "Verify integrity" puts the original back
//   Extra                anything else added (a folder inside Database, a stray file): copied, not read by the game
// Every group with a database is test-read (its databases parsed, an MXTables' tables decoded): one that fails is `unreadable` and the
// import leaves it out (the choice of the player otherwise: LEAVE OUT / MOVE / keep).
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.RegularExpressions;
using System.Threading;

namespace Reflex.Port.Content
{
    public sealed class DbModGroup
    {
        public enum Kinds { GearPack, KitPack, TrackSlot, Track, DlcFiles, Changed, Extra }
        public Kinds kind;
        /// "DLC010", "Beta_Nat_Track_Slot_4", "MXs" ...
        public string key;
        /// File (and folder) names in the Database folder.
        public readonly List<string> files = new List<string>();
        public long bytes;
        /// Why its database cannot be read (null: it reads).
        public string unreadable;
        /// "Gear" / "Tracks": the Mods folder OPEN REFLEX reads it from, when Move can put it there; null when it stays.
        public string moveTo;
        /// Some of its files have stock names with other sizes (a pack in an official DLC slot, a replaced bike).
        public bool replacesStock;
        public bool IsFolder => kind == Kinds.Extra && files.Count == 1 && files[0].EndsWith("/", StringComparison.Ordinal);
        public override string ToString() => kind + " " + key + (unreadable != null ? " UNREADABLE(" + unreadable + ")" : "") + (moveTo != null ? " -> Mods/" + moveTo : "") + ": " + string.Join(", ", files);
    }

    public static class ImportModScan
    {
        static Dictionary<string, long> s_stock;
        /// The Steam release's Database files by name (any case) -> size.
        public static Dictionary<string, long> Stock
        {
            get
            {
                if (s_stock != null) return s_stock;
                var d = new Dictionary<string, long>(StringComparer.OrdinalIgnoreCase);
                foreach (var line in ReflexStockDatabase.Files.Split('\n'))
                {
                    int t = line.IndexOf('\t'); if (t <= 0) continue;
                    d[line.Substring(0, t)] = long.Parse(line.Substring(t + 1), System.Globalization.CultureInfo.InvariantCulture);
                }
                return s_stock = d;
            }
        }

        static readonly Regex Slot = new Regex(@"DLC_?(\d+)", RegexOptions.IgnoreCase | RegexOptions.CultureInvariant);
        static readonly Regex BetaSlot = new Regex(@"^Beta_(\w+_)?Track_Slot_\d+$", RegexOptions.IgnoreCase | RegexOptions.CultureInvariant);
        static readonly string[] DxExt = { ".dx9.database", ".dx9.package", ".dx9.level", ".dx9.scene" };

        /// "Rider_x.dx9.package" -> "Rider_x"; names without a .dx9 extension stay whole.
        public static string BaseName(string file)
        {
            foreach (var e in DxExt) if (file.EndsWith(e, StringComparison.OrdinalIgnoreCase)) return file.Substring(0, file.Length - e.Length);
            return file;
        }
        static string Unhr(string b) => b.StartsWith("HR_", StringComparison.OrdinalIgnoreCase) ? b.Substring(3) : b;

        /// The mods in a Database folder (see the file header); an empty list for a stock folder. stock: names -> sizes (null: the Steam release's).
        public static List<DbModGroup> Scan(string databaseDir, CancellationToken cancel = default, Dictionary<string, long> stock = null)
        {
            stock = stock ?? Stock;
            var groups = new Dictionary<string, DbModGroup>(StringComparer.OrdinalIgnoreCase);
            if (string.IsNullOrEmpty(databaseDir) || !Directory.Exists(databaseDir)) return new List<DbModGroup>();
            DbModGroup Group(string key, DbModGroup.Kinds kind)
            {
                if (!groups.TryGetValue(key, out var g)) groups[key] = g = new DbModGroup { key = key, kind = kind };
                return g;
            }
            var all = Directory.GetFiles(databaseDir).Select(Path.GetFileName).Where(n => !ReflexMods.IsJunk(n)).OrderBy(n => n, StringComparer.OrdinalIgnoreCase).ToList();
            var added = new List<string>(); var changed = new List<string>();
            foreach (var n in all)
            {
                cancel.ThrowIfCancellationRequested();
                long size = new FileInfo(Path.Combine(databaseDir, n)).Length;
                if (!stock.TryGetValue(n, out long want)) added.Add(n);
                else if (want != size) changed.Add(n);
            }
            var lowerSet = new HashSet<string>(all, StringComparer.OrdinalIgnoreCase);
            void Put(string n, bool isChanged)
            {
                string b = BaseName(n), core = Unhr(b);
                var m = Slot.Match(core);
                DbModGroup g;
                if (BetaSlot.IsMatch(core)) g = Group(core, DbModGroup.Kinds.TrackSlot);
                else if (m.Success && (core.StartsWith("MXTables_", StringComparison.OrdinalIgnoreCase) || core.StartsWith("MXUI_", StringComparison.OrdinalIgnoreCase) || core.IndexOf("graphic", StringComparison.OrdinalIgnoreCase) >= 0
                                       || core.StartsWith("RiderDLC", StringComparison.OrdinalIgnoreCase) || core.StartsWith("MXDLC", StringComparison.OrdinalIgnoreCase) || core.StartsWith("ATVDLC", StringComparison.OrdinalIgnoreCase) || core.StartsWith("UTVDLC", StringComparison.OrdinalIgnoreCase)))
                    g = Group("DLC" + m.Groups[1].Value, DbModGroup.Kinds.DlcFiles);
                else if (isChanged) g = Group("changed:" + core, DbModGroup.Kinds.Changed);
                else if (lowerSet.Contains(b + ".dx9.level") || lowerSet.Contains(core + ".dx9.level")) g = Group("track:" + core, DbModGroup.Kinds.Track);
                else g = Group("extra", DbModGroup.Kinds.Extra);
                g.files.Add(n); g.bytes += new FileInfo(Path.Combine(databaseDir, n)).Length;
                if (isChanged) g.replacesStock = true;
            }
            foreach (var n in added) Put(n, false);
            foreach (var n in changed) Put(n, true);
            // a DLC slot: what its files make it
            foreach (var g in groups.Values.Where(x => x.kind == DbModGroup.Kinds.DlcFiles))
            {
                bool tables = g.files.Any(f => f.StartsWith("MXTables_", StringComparison.OrdinalIgnoreCase) && f.EndsWith(".dx9.database", StringComparison.OrdinalIgnoreCase))
                           && g.files.Any(f => f.StartsWith("MXTables_", StringComparison.OrdinalIgnoreCase) && f.EndsWith(".dx9.package", StringComparison.OrdinalIgnoreCase));
                var kit = g.files.FirstOrDefault(f => Unhr(BaseName(f)).IndexOf("graphicskit", StringComparison.OrdinalIgnoreCase) >= 0 || Unhr(BaseName(f)).IndexOf("graphickit", StringComparison.OrdinalIgnoreCase) >= 0 && !stock.ContainsKey(f));
                if (kit != null) g.kind = Unhr(BaseName(kit)).StartsWith("Rider", StringComparison.OrdinalIgnoreCase) ? DbModGroup.Kinds.GearPack : DbModGroup.Kinds.KitPack;
                if (kit != null && tables) g.moveTo = ReflexMods.GearName;
            }
            foreach (var g in groups.Values.Where(x => x.kind == DbModGroup.Kinds.TrackSlot))
                if (g.files.Any(f => f.EndsWith(".dx9.level", StringComparison.OrdinalIgnoreCase)) && g.files.Any(f => f.EndsWith(".dx9.database", StringComparison.OrdinalIgnoreCase))) g.moveTo = ReflexMods.TracksName;
            // folders inside Database (a pack unzipped into it with its folders): the game reads none of them
            try
            {
                foreach (var d in Directory.GetDirectories(databaseDir).OrderBy(x => x, StringComparer.OrdinalIgnoreCase))
                {
                    string n = Path.GetFileName(d); if (ReflexMods.IsJunk(n)) continue;
                    var g = Group("folder:" + n, DbModGroup.Kinds.Extra); g.files.Add(n + "/");
                    try { g.bytes = new DirectoryInfo(d).EnumerateFiles("*", SearchOption.AllDirectories).Sum(f => f.Length); } catch (Exception) { }
                }
            }
            catch (Exception) { }
            // can its databases be read?
            var install = new Rfx.Install(databaseDir);
            try
            {
                foreach (var g in groups.Values)
                {
                    cancel.ThrowIfCancellationRequested();
                    foreach (var f in g.files.Where(f => f.EndsWith(".dx9.database", StringComparison.OrdinalIgnoreCase)))
                    {
                        string b = BaseName(f);
                        try
                        {
                            var db = install.Db(b) ?? throw new InvalidDataException("it did not load");
                            if (b.StartsWith("MXTables_", StringComparison.OrdinalIgnoreCase))
                                foreach (var p in db.packages.Values) foreach (var a in p.OfType("tbldata")) Rfx.Tables(install.Read(a, cancel));
                        }
                        catch (Exception e) when (!(e is OperationCanceledException))
                        {
                            g.unreadable = f + ": " + (e is InvalidCastException ? "a value of an unexpected type (" + e.Message + ")" : e.Message);
                            break;
                        }
                    }
                }
            }
            finally { install.Close(); }
            int Order(DbModGroup g) => g.unreadable != null ? 0 : (int)g.kind + 1;
            return groups.Values.OrderBy(Order).ThenBy(g => g.key, StringComparer.OrdinalIgnoreCase).ToList();
        }

        /// What a group is, in a few words ("Rider gear pack, DLC slot 010").
        public static string Title(DbModGroup g)
        {
            string slot = g.key.StartsWith("DLC", StringComparison.OrdinalIgnoreCase) ? "DLC slot " + g.key.Substring(3) : g.key;
            switch (g.kind)
            {
                case DbModGroup.Kinds.GearPack: return "Rider gear pack  ·  " + slot;
                case DbModGroup.Kinds.KitPack: return "Bike graphics kit pack  ·  " + slot;
                case DbModGroup.Kinds.TrackSlot: return "Custom track in " + g.key;
                case DbModGroup.Kinds.Track: return "Community track " + g.key.Substring(g.key.IndexOf(':') + 1);
                case DbModGroup.Kinds.DlcFiles: return "Community DLC files  ·  " + slot;
                case DbModGroup.Kinds.Changed: return "Changed game files  ·  " + g.key.Substring(g.key.IndexOf(':') + 1);
                default: return g.key.StartsWith("folder:", StringComparison.Ordinal) ? "A folder inside Database  ·  " + g.key.Substring(7) : "Other added files";
            }
        }

        /// What happens to a group, in plain words.
        public static string Advice(DbModGroup g)
        {
            if (g.unreadable != null) return "This version can't read it, so it will be left out of the import." + (g.moveTo == ReflexMods.GearName ? " Gear and kit packs are best kept in Mods/Gear." : "");
            switch (g.kind)
            {
                case DbModGroup.Kinds.GearPack: case DbModGroup.Kinds.KitPack:
                    return "OPEN REFLEX can use it from Mods/Gear instead, alongside any number of other packs, without touching the game folder.";
                case DbModGroup.Kinds.TrackSlot: return g.moveTo != null ? "OPEN REFLEX can use it from Mods/Tracks instead (FREE RIDE / RACE > CUSTOM TRACKS)." : "Some of the track's files are missing.";
                case DbModGroup.Kinds.Track: case DbModGroup.Kinds.DlcFiles: return "It will be imported as it is.";
                case DbModGroup.Kinds.Changed: return "Imported as it is. Steam's \"Verify integrity of game files\" puts the original files back.";
                default: return g.IsFolder ? "The game doesn't read folders inside Database. It will be copied, but it does nothing." : "Copied as it is.";
            }
        }

        /// The files of the groups (Database file names; folders as "name/" are not files and are left out).
        public static List<string> FilesOf(IEnumerable<DbModGroup> groups) => groups.SelectMany(g => g.files).Where(f => !f.EndsWith("/", StringComparison.Ordinal)).Distinct(StringComparer.OrdinalIgnoreCase).ToList();

        /// Moves a group's files from the Database folder into <modsRoot>/<moveTo>/<a folder of its own> (never overwriting: a new folder each
        /// time). On a failure the files already moved go back. Returns the new folder, or throws with what went wrong.
        public static string Move(DbModGroup g, string databaseDir, string modsRoot)
        {
            if (g.moveTo == null) throw new InvalidOperationException("This one stays in the game folder.");
            string kindDir = Path.Combine(modsRoot, g.moveTo);
            string name = g.kind == DbModGroup.Kinds.TrackSlot ? g.key + " (from game folder)" : g.key + " pack (from game folder)";
            string target = Path.Combine(kindDir, name);
            for (int i = 2; Directory.Exists(target) || File.Exists(target); i++) target = Path.Combine(kindDir, name + " " + i);
            Directory.CreateDirectory(target);
            var moved = new List<(string from, string to)>();
            try
            {
                foreach (var f in g.files.Where(f => !f.EndsWith("/", StringComparison.Ordinal)))
                {
                    string from = Path.Combine(databaseDir, f), to = Path.Combine(target, f);
                    ReflexAssetImport.Retry(() => File.Move(from, to), CancellationToken.None, 3);
                    moved.Add((from, to));
                }
                return target;
            }
            catch (Exception)
            {
                foreach (var (from, to) in moved) { try { File.Move(to, from); } catch (Exception) { } }
                try { if (!Directory.EnumerateFileSystemEntries(target).Any()) Directory.Delete(target); } catch (Exception) { }
                throw;
            }
        }
    }
}

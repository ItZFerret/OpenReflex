// Gear / kit packs in the running game (KitPacks.cs has the format and the readers): the folders scanned (Mods/Gear, Mods/Skins, the game's
// Database folder), the archives installed there, the player's choices (the client save) and what they change - the garage's lists and
// previews (OpenGarageData) and the local rider's race rider and bike (ReflexRoster).
//
// The player's choice of a pack piece is a token per gear type ("KitPack.Gear.<RiderGearTypeId>") and per vehicle model and part type
// ("KitPack.Part.<VehicleModelId>.<VehiclePartTypeId>"): "g:<pack hash>:<RiderGearId>" / "k:<pack hash>:<VehiclePartId>" (the pack's own ids;
// the hash is its content, so a pack moved or renamed keeps the choice). The game's profile keeps its stock piece equipped underneath: a pack
// that is removed simply leaves the stock piece showing (the key stays, so putting the pack back brings the choice back); choosing a stock
// piece clears the key. The core never sees a pack id: its tables and its profile keep the stock ids.
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using UnityEngine;

namespace Reflex.Port.Content
{
    public static partial class KitPacks
    {
        public const string GearName = ReflexMods.GearName, SkinsName = "Skins";
        public const string GearKey = "KitPack.Gear.", PartKey = "KitPack.Part.";

        static Catalog s_catalog;
        static List<Problem> s_problems = new List<Problem>();
        static readonly List<string> s_installing = new List<string>();
        static readonly object s_lock = new object();
        static int s_generation = int.MinValue;
        static bool s_stale = true;

        /// Bumped by a rescan and by every choice (the garage's preview keys).
        public static int Version { get; private set; }
        /// Raised after a choice is stored.
        public static event Action Changed;

        /// The folders scanned: <client root>/Mods/Gear (created with a README) and Mods/Skins when the player made one.
        public static IEnumerable<string> Roots()
        {
            string g = ReflexMods.Folder(GearName); if (g != null) yield return g;
            string s = ReflexMods.Folder(SkinsName); if (s != null && Directory.Exists(s)) yield return s;
        }

        /// The catalog of every pack found (scanned on first use, again after Refresh or when an archive install ended).
        public static Catalog Current
        {
            get
            {
                lock (s_lock)
                {
                    if (s_catalog != null && !s_stale && s_generation == TrackArchives.Generation) return s_catalog;
                    Rescan();
                    return s_catalog;
                }
            }
        }
        /// Every pack in use.
        public static List<KitPack> All => Current.packs;
        /// What could not be read or installed (the garage lists it).
        public static List<Problem> Problems { get { var _ = Current; lock (s_lock) return s_problems.ToList(); } }
        /// Archives being installed now.
        public static List<string> Installing { get { var _ = Current; lock (s_lock) return s_installing.ToList(); } }
        /// Scan again on the next use (the garage opened: packs dropped in since).
        public static void Refresh() { lock (s_lock) s_stale = true; }

        static void Rescan()
        {
            s_generation = TrackArchives.Generation; s_stale = false;
            var problems = new List<Problem>(); var packs = new List<KitPack>(); s_installing.Clear();
            Log = Debug.Log; Warn = Debug.LogWarning; TrackArchives.Log = Debug.Log;
            try
            {   // the game's own GEICO kits (KitPacks.Geico.cs): in every install, named by no table of it
                string dbDir = ReflexInstall.Dir != null ? ReflexInstall.DatabaseDir : null;
                bool extracted = dbDir != null && File.Exists(Path.Combine(Path.GetDirectoryName(dbDir), ReflexAssetImport.ExtractedMarker));
                var geico = dbDir == null ? null : Geico(dbDir, extracted, ReflexEvents.Tables, problems);
                if (geico != null) packs.Add(geico);
            }
            catch (Exception e) { problems.Add(new Problem { where = GeicoPackId, why = "not read (" + e.Message + ")" }); }
            foreach (var root in Roots())
            {
                try { packs.AddRange(Scan(root, problems, a => Archive(a, root))); }
                catch (Exception e) { problems.Add(new Problem { where = root, why = "not scanned (" + e.Message + ")" }); }
            }
            // (a pack put into the game's Database folder the original way is merged into the game's tables as the original game merges it - the
            // core reads it there too, so both keep its ids; ReflexEvents.Tables keeps one row per id. KitPacks.ScanInstall reads one as a pack: tests.)
            if (ReflexMods.ClientRoot == null) s_stale = true;   // (asked before the client root was known: again on the next use)
            var old = s_catalog;
            var c = Build(packs, problems);
            if (old != null)
            {   // a pack that is still there keeps its open install (the models built from it read on); the others are closed
                foreach (var p in c.packs)
                {
                    var o = old.packs.FirstOrDefault(x => x.hash == p.hash);
                    if (o != null && o.install != null) { p.install.Close(); p.install = o.install; }
                }
                foreach (var o in old.packs) if (!c.packs.Any(p => ReferenceEquals(p.install, o.install))) o.install?.Close();
            }
            s_catalog = c; Active = c; s_problems = problems;
            Rfx.Install.ModRouter = BikeMods.InstallOf;
            Version++;
            foreach (var p in c.packs) Debug.Log("[KitPacks] " + Describe(p) + " in " + p.folder);
            foreach (var p in problems) Debug.LogWarning("[KitPacks] NOT LOADED " + p);
            if (c.packs.Count > 0 || problems.Count > 0) Debug.Log("[KitPacks] " + c.packs.Count + " packs (" + c.byId.Count + " pieces: " + c.byId.Values.Count(i => !i.bike) + " rider gear, "
                + c.byId.Values.Count(i => i.bike) + " vehicle parts), " + problems.Count + " problems; " + s_installing.Count + " archives installing");
        }

        /// An archive found in a Mods folder: installed into a folder of its own next to it (TrackArchives, one at a time on a worker); its state.
        static string Archive(string archive, string root)
        {
            if (TrackArchives.InstalledFolder(archive) != null) return null;   // installed: its folder is scanned
            var job = TrackArchives.Ensure(archive, root, ArchiveProfile);
            if (job.state == "installing") { s_installing.Add(Path.GetFileName(archive)); return null; }
            return job.state == "failed" ? "CANNOT INSTALL - " + job.error : null;
        }

        /// The pack piece of a renumbered id (null: a stock id, or not installed).
        public static KitItem Item(int id) => IsKitId(id) && Current.byId.TryGetValue(id, out var it) ? it : null;
        public static KitItem FromToken(string token) => !string.IsNullOrEmpty(token) && Current.byToken.TryGetValue(token, out var it) ? it : null;
        /// The renumbered StoreProductId's piece or line: a line's (title, brand, icon) when id is a merged pack line.
        public static bool Line(int id, out (string title, int brand, int icon) line) => Current.lines.TryGetValue(id, out line);
        public static KitItem ProductItem(int product) => IsKitId(product) && Current.byProduct.TryGetValue(product, out var it) ? it : null;

        /// The game's tables with the packs' rows (the spec builders, the garage's questions).
        public static ReflexGameTables Tables(ReflexGameTables.ISource stock) => new ReflexGameTables(Source(stock, Current));

        // ------------------------------------------------------------------------------------------------ the player's choices

        /// The pack piece chosen for a gear type (null: the profile's stock piece; a pack that is gone counts as none).
        public static KitItem ChosenGear(int type)
        {
            var it = FromToken(Options.ReflexSave.GetString(GearKey + type, ""));
            return it != null && !it.bike && it.type == type ? it : null;
        }
        /// Every gear type's chosen pack piece.
        public static List<KitItem> ChosenGear() => Current.byId.Values.Where(i => !i.bike).Select(i => i.type).Distinct().Select(ChosenGear).Where(i => i != null).ToList();
        /// The pack part (a graphics kit ...) chosen for a vehicle model's part type (null: the profile's stock part).
        public static KitItem ChosenPart(int model, int type)
        {
            var it = FromToken(Options.ReflexSave.GetString(PartKey + model + "." + type, ""));
            return it != null && it.bike && it.type == type && it.models.Contains(model) ? it : null;
        }
        public static List<KitItem> ChosenParts(int model) => model < 0 ? new List<KitItem>() : Current.byId.Values.Where(i => i.bike && i.models.Contains(model)).Select(i => i.type).Distinct().Select(t => ChosenPart(model, t)).Where(i => i != null).ToList();

        /// Stores a gear type's choice (null: back to the profile's stock piece).
        public static void SetGear(int type, KitItem it)
        {
            Options.ReflexSave.SetString(GearKey + type, it == null ? "" : it.Token); Options.ReflexSave.Save();
            Version++; Changed?.Invoke();
        }
        public static void SetPart(int model, int type, KitItem it)
        {
            Options.ReflexSave.SetString(PartKey + model + "." + type, it == null ? "" : it.Token); Options.ReflexSave.Save();
            Version++; Changed?.Invoke();
        }
        /// Whether a stock choice clears something (no write when nothing was chosen).
        public static void ClearGear(int type) { if (Options.ReflexSave.GetString(GearKey + type, "").Length > 0) SetGear(type, null); }
        public static void ClearPart(int model, int type) { if (Options.ReflexSave.GetString(PartKey + model + "." + type, "").Length > 0) SetPart(model, type, null); }

        // ------------------------------------------------------------------------------------------------ what they change

        /// A rider's / vehicle's ids with the player's chosen pieces in place; t: the tables with the packs' rows (Tables).
        public static int[] GearIds(ReflexGameTables t, int[] stock)
        {
            var chosen = ChosenGear();
            return chosen.Count == 0 ? stock : WithGear(stock, chosen, id => t.Int("RiderGear", "RiderGearTypeId", t.Find("RiderGear", "RiderGearId", id)));
        }
        public static int[] PartIds(ReflexGameTables t, int model, int[] stock)
        {
            var chosen = ChosenParts(model);
            var ids = stock;
            foreach (var it in chosen) ids = WithPart(ids, it, id => t.Int("VehiclePart", "VehiclePartTypeId", t.Find("VehiclePart", "VehiclePartId", id)));
            return ids;
        }

    }
}

// The game's own GEICO graphics kits, offered as a built-in kit pack (no files to install: they ship in every PC install).
//
// What is in the install: MXDLC01_graphickit (40 packages: MXDLC01_graphickit_<model>_Geico_<colour>, two colours for each of the 20 MX
// chassis Earth / EarthF / EarthLite / EarthFLite ... Venus*) and ATVDLC01_graphickit (18: two colours for each of the 9 ATV chassis Bliz, Diz,
// Ganiz, Gliz, KTM450, Muniz, Phiz, Shiz, Triz), each with its HR_ twin and laid out like a stock kit (MX_Plstc_* / MX_Seat_* / MX_HGuard_* or
// ATV_Plstc_* / ATV_HGuard_*, which the chassis bindings take by prefix: ReflexVehicleMaterials.Resolve). The empty MXDLC01 / ATVDLC01 databases
// hold one empty package per chassis. No table names any of it: MXTables_DLC003, the slot their rows would be in, holds no rows on PC, so the
// original game never offers the kits (the GEICO brand row, BrandId 84 with its icon, is in MXTables_DLC004 with the GEICO-sponsored DLC tracks).
//
// Here the rows a DLC table would hold are written from the install: per kit package a store product (line "GEICO", brand GEICO when the
// install has its Brand row, the colour as its name), a VehiclePart of type 11 (the kit as its material package, the stock chassis package of
// the model the stock kits of that chassis use as its model package, HRAvailable when the twin is there, the stock kit's tuning defaults) and
// a VehicleModelParts row for each stock VehicleModelId the stock kits of that chassis fit. The rows then go through the pack reader every
// community kit pack goes through (Items: checks, names; Build: renumbered ids, the garage, the save tokens), so nothing of
// the game's own tables changes. A kit whose chassis no stock vehicle uses (the two KTM450 ATV kits: PC has no KTM 450 ATV) is skipped with a note.
//
// Identity: the pack hash is the four databases' bytes (not the package files: an imported install has none) with a version mark, so every
// player with the same game files and this version names the kits alike. The pack's own row ids are fixed per kit package (its
// position in the sorted package list), so a saved choice stays valid.
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using System.Text;

namespace Reflex.Port.Content
{
    public static partial class KitPacks
    {
        public const string GeicoPackId = "Built-in/GEICO", GeicoName = "GEICO";
        const string GeicoMark = "open-reflex built-in GEICO kits v1";
        static readonly (string db, string chassisDb, string prefix)[] GeicoSources =
        {
            ("MXDLC01_graphickit", "MXs", "MXDLC01_graphickit_"),
            ("ATVDLC01_graphickit", "ATVs", "ATVDLC01_graphickit_"),
        };
        // the pack's own ids: far from the game's (a stock package id must never read as one of the pack's: Items' Package)
        const int GeicoDbBase = 5_000_000, GeicoPkgBase = 5_100_000, GeicoLine = 5_200_000, GeicoProductBase = 5_200_001, GeicoAssetBase = 5_300_000, GeicoPartBase = 5_400_000;

        /// The GEICO kits of the install's Database folder as a pack (null: the install has none of their databases). stock: the game's
        /// merged tables (ReflexEvents.Tables; tests: the dumped MXTables_Default + DLC004 / 005). extracted: the folder is the import's
        /// copy (assets read where the import extracted them).
        public static KitPack Geico(string dbDir, bool extracted, Dictionary<string, List<Dictionary<string, object>>> stock, List<Problem> problems = null)
        {
            if (string.IsNullOrEmpty(dbDir) || !Directory.Exists(dbDir) || stock == null) return null;
            Dictionary<string, string> files;
            try { files = Files(Directory.GetFiles(dbDir, "*" + DbExt).Concat(extracted ? Enumerable.Empty<string>() : Directory.GetFiles(dbDir, "*" + PkgExt))); }
            catch (Exception e) when (e is IOException || e is UnauthorizedAccessException) { return null; }
            bool Present(string n) => files.ContainsKey(n.ToLowerInvariant() + DbExt) && (extracted || files.ContainsKey(n.ToLowerInvariant() + PkgExt));
            var sources = GeicoSources.Where(s => Present(s.db)).ToList();
            if (sources.Count == 0) return null;

            var p = new KitPack
            {
                id = GeicoPackId, name = GeicoName, folder = dbDir, tables = "MXTables_DLC01", fromInstall = true, builtIn = true, author = "THQ / Rainbow Studios",
                description = "The GEICO graphics kits that ship with the game but were never offered in its menus.",
            };
            foreach (var s in sources)
                foreach (string n in new[] { s.db, "HR_" + s.db })
                    if (Present(n)) p.databases.Add(Path.GetFileName(files[n.ToLowerInvariant() + DbExt]).Substring(0, n.Length));
            using (var sha = SHA1.Create())
            {
                void Add(byte[] b) => sha.TransformBlock(b, 0, b.Length, null, 0);
                Add(Encoding.UTF8.GetBytes(GeicoMark + "\n"));
                foreach (string n in p.databases.OrderBy(x => x, StringComparer.OrdinalIgnoreCase))
                {
                    Add(Encoding.UTF8.GetBytes(n.ToLowerInvariant() + "\n"));
                    Add(File.ReadAllBytes(files[n.ToLowerInvariant() + DbExt]));
                }
                sha.TransformFinalBlock(new byte[0], 0, 0);
                p.hash = string.Concat(sha.Hash.Take(6).Select(b => b.ToString("x2")));
            }
            p.install = new Rfx.Install(dbDir, p.Namespace, files, extracted);
            try
            {
                p.rows = GeicoRows(p, sources, stock);
            }
            catch (Exception e) when (e is InvalidDataException || e is IOException || e is KeyNotFoundException || e is FormatException)
            {
                p.install.Close();
                problems?.Add(new Problem { where = Path.Combine(dbDir, sources[0].db + DbExt), why = "the GEICO kits cannot be read (" + e.Message + "): skipped" });
                return null;
            }
            Items(p);
            if (p.items.Count == 0)
            {
                p.install.Close();
                problems?.Add(new Problem { where = Path.Combine(dbDir, sources[0].db + DbExt), why = "no GEICO kit fits a vehicle: " + string.Join("; ", p.notes.Take(3)) });
                return null;
            }
            return p;
        }

        /// The table rows a DLC table would hold for the kits (the pack's own ids), and the pack's strings (line and colour names).
        static Dictionary<string, List<Dictionary<string, object>>> GeicoRows(KitPack p, List<(string db, string chassisDb, string prefix)> sources,
                                                                           Dictionary<string, List<Dictionary<string, object>>> stock)
        {
            List<Dictionary<string, object>> S(string table) => stock.TryGetValue(table, out var r) ? r : new List<Dictionary<string, object>>();
            var t = new Dictionary<string, List<Dictionary<string, object>>>(StringComparer.Ordinal);
            void Row(string table, params (string col, object v)[] cells)
            {
                if (!t.TryGetValue(table, out var list)) t[table] = list = new List<Dictionary<string, object>>();
                var r = new Dictionary<string, object>(StringComparer.Ordinal);
                foreach (var (c, v) in cells) r[c] = v;
                list.Add(r);
            }
            var eng = new Dictionary<string, string>(StringComparer.Ordinal);
            p.strings = new Dictionary<string, Dictionary<string, string>>(StringComparer.OrdinalIgnoreCase) { ["ENG"] = eng };

            // the stock kits: chassis package ("MXs/MarsF") -> (its PackageDatabaseId, a stock kit row, the VehicleModelIds they fit)
            var dbName = S("AssetDatabase").GroupBy(r => Rfx.Int(r, "DatabaseId")).ToDictionary(g => g.Key, g => Rfx.Str(g.First(), "DatabaseName"));
            var pkgName = S("AssetPackage").GroupBy(r => Rfx.Int(r, "PackageId")).ToDictionary(g => g.Key, g => Rfx.Str(g.First(), "PackageName"));
            var apd = S("AssetPackageDatabase").GroupBy(r => Rfx.Int(r, "PackageDatabaseId")).ToDictionary(g => g.Key, g => g.First());
            string PackageOf(int id) => apd.TryGetValue(id, out var r) && dbName.TryGetValue(Rfx.Int(r, "DatabaseId"), out var d) && pkgName.TryGetValue(Rfx.Int(r, "PackageId"), out var k) ? d + "/" + k : null;
            var fits = S("VehicleModelParts").GroupBy(r => Rfx.Int(r, "VehiclePartId")).ToDictionary(g => g.Key, g => g.Select(r => Rfx.Int(r, "VehicleModelId")).ToList());
            var chassis = new Dictionary<string, (int model, Dictionary<string, object> kit, SortedSet<int> models)>(StringComparer.OrdinalIgnoreCase);
            foreach (var r in S("VehiclePart"))
            {
                if (Rfx.Int(r, "VehiclePartTypeId") != 11) continue;
                int m = Rfx.Int(r, "ModelPackageDatabaseId", -1);
                string key = PackageOf(m); if (key == null) continue;
                if (!chassis.TryGetValue(key, out var c)) chassis[key] = c = (m, r, new SortedSet<int>());
                if (fits.TryGetValue(Rfx.Int(r, "VehiclePartId"), out var ms)) foreach (int x in ms) if (x >= 0) c.models.Add(x);
            }
            var brand = S("Brand").FirstOrDefault(r => string.Equals(Rfx.Str(r, "BrandName"), "Geico", StringComparison.OrdinalIgnoreCase));
            int brandId = brand != null ? Rfx.Int(brand, "BrandId", -1) : -1;

            Row("StoreProducts", ("StoreProductId", GeicoLine), ("ParentStoreProductId", -1), ("Hidden", 0), ("Price", 0), ("BrandId", brandId), ("UnlockRequirement", 0), ("LocalizationId", -1), ("IconId", -1));
            eng["STR_" + GeicoLine] = GeicoName;
            int dbIndex = 0;
            foreach (var s in sources)
            {
                string mine = p.databases.First(x => string.Equals(x, s.db, StringComparison.OrdinalIgnoreCase));
                var db = p.install.Db(p.Namespace + mine) ?? throw new InvalidDataException(s.db + DbExt + " did not load");
                int dbId = GeicoDbBase + dbIndex++;
                Row("AssetDatabase", ("DatabaseId", dbId), ("DatabaseName", mine));
                var kits = db.packages.Values.Where(k => k.name.StartsWith(s.prefix, StringComparison.OrdinalIgnoreCase) && k.OfType("material").Any())
                                             .OrderBy(k => k.name, StringComparer.Ordinal).ToList();
                for (int i = 0; i < kits.Count; i++)
                {
                    string pkg = kits[i].name;
                    int own = ((dbId - GeicoDbBase) << 12) | i;   // fixed per database and package position
                    // "<chassis>_<sponsor>_<colour>": the chassis as the stock package names it (EarthLite -> Earth_Lite, EarthFLite -> EarthF_Lite)
                    var parts = pkg.Substring(s.prefix.Length).Split('_');
                    if (parts.Length < 3) continue;
                    string body = string.Join("_", parts.Take(parts.Length - 2)), colour = parts[parts.Length - 1];
                    string stockBody = body.EndsWith("FLite", StringComparison.Ordinal) ? body.Substring(0, body.Length - 5) + "F_Lite"
                                     : body.EndsWith("Lite", StringComparison.Ordinal) && !body.EndsWith("_Lite", StringComparison.Ordinal) ? body.Substring(0, body.Length - 4) + "_Lite" : body;
                    if (!chassis.TryGetValue(s.chassisDb + "/" + stockBody, out var c) || c.models.Count == 0)
                    { p.notes.Add(pkg + ": no vehicle of this version uses the " + s.chassisDb + "/" + stockBody + " chassis"); continue; }
                    int product = GeicoProductBase + own, asset = GeicoAssetBase + own, part = GeicoPartBase + own, pkgId = GeicoPkgBase + own;
                    bool hr = p.install.Pkg(p.Namespace + "HR_" + mine, "HR_" + pkg) != null;
                    Row("AssetPackage", ("PackageId", pkgId), ("PackageName", pkg));
                    Row("AssetPackageDatabase", ("PackageDatabaseId", pkgId), ("PackageId", pkgId), ("DatabaseId", dbId));
                    Row("StoreProducts", ("StoreProductId", product), ("ParentStoreProductId", GeicoLine), ("Hidden", 0), ("Price", 0), ("BrandId", brandId), ("UnlockRequirement", 0), ("LocalizationId", -1), ("IconId", -1));
                    Row("StoreAssets", ("StoreAssetId", asset), ("StoreProductId", product));
                    Row("VehiclePart", ("VehiclePartId", part), ("VehiclePartTypeId", 11), ("StoreProductId", product), ("HRAvailable", hr ? 1 : 0),
                        ("DefaultTuneAttribute1", Rfx.Int(c.kit, "DefaultTuneAttribute1", 0)), ("DefaultTuneAttribute2", Rfx.Int(c.kit, "DefaultTuneAttribute2", 0)),
                        ("MaterialPackageDatabaseId", pkgId), ("ModelPackageDatabaseId", c.model));
                    foreach (int m in c.models) Row("VehicleModelParts", ("VehicleModelId", m), ("VehiclePartId", part), ("Default", 0), ("StoreAssetId", asset));
                    eng["STR_" + product] = colour.Length == 0 ? pkg : char.ToUpperInvariant(colour[0]) + colour.Substring(1).ToLowerInvariant();
                }
            }
            return t;
        }
    }
}

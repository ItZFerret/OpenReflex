// MX vs ATV Alive's motorcycles, imported from the player's own Alive install (core/plan/ALIVE_BIKES.md): each bike becomes a .rxmod bike
// folder (core/plan/RXMOD.md) in the client's content folder, <content>/AliveBikes/<slug>/ - its mod.json, its bike.rxmesh and its maps as
// PNG - built on the player's machine from the disc and the LIVE packages they point the importer at (nothing of Alive ships with the game).
//
//   which bikes      Alive's tables (MXTables_Default / MXTables_DLC*, MXTables_Core): VehicleModel rows whose VehicleType is a
//                    "MotocrossBike", named by their store product (StoreAssets -> StoreProducts -> the MXUI* StoreProducts.ENG strings) and
//                    Brand; their parts are the VehicleModelParts rows marked Default (VehiclePart -> AssetPackageDatabase: the model and the
//                    material package of each part: the chassis + graphic kit, handlebar, tyres, exhaust, wheels)
//   what it rides as the chassis package's physics file (mx125 / mx250 / mx250f / mx450f_vehicle: Alive's class file, Reflex's schema) and the
//                    brand pick the Reflex bike of that class and brand (Honda = Blaze, Yamaha = Takashi, Suzuki = Shima, KTM = KTM, none =
//                    Dakumoto); its physics are that bike's 1:1. The 50cc (mx50) and the MXFR650 (mx650f) have no Reflex equivalent: they
//                    ride on their own: Alive's file is written beside the bike ("physics"), and the core puts it over the class file of
//                    the slot's bike for the local rider (rx_session_vehicle_physics; the 50cc also its own hierarchy: ALIVE_BIKES.md)
//   the model        the chassis model's first LOD on its own 18-node MX_Hierarchy (Alive's is node for node Reflex's), the part models on
//                    the nodes they name (a tyre model in its node's space on the chassis' OBJ_FRT_Tire / OBJ_BCK_Tire, a bar / exhaust model
//                    in its node's space), each binding's material resolved as the game does (the first material of the bike's material
//                    packages - the graphic kit first - whose key prefixes the binding's: ReflexVehicles.MaterialKey; else the model
//                    package's own material of that name)
//   the maps         each material's textures converted from the Xbox layout (Textures.Convert) and written as PNG (AliveBikeMaps: the
//                    same bytes on every machine, so two players who imported the same bike hold the same .rxmod and see each other's)
// Converters / Textures are the level importer's (alive_convert.py's checks); Rfx reads the converted PC resources (RfxData.cs).
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using System.Text;
using System.Threading;
using UnityEngine;

namespace Reflex.Port.Content.AliveImport
{
    /// One Alive motorcycle found in the player's sources.
    public sealed class AliveBikeSource
    {
        public int aliveId;                    // Alive's VehicleModelId
        public string product = "";            // its store product's English name ("Honda CRF450R MX")
        public string brand = "", model = "";  // the garage card's lines ("Honda", "CRF450R")
        public string pack = "";               // the source it came from (the disc, "Honda Vehicle Pack")
        public string vehicleType = "", physics = "";   // VehicleType.Name ("MX450"), the chassis' physics file ("mx450f_vehicle")
        public string hierarchy = "MX_Hierarchy";
        /// An ATV (VehicleType FactoryName "Atv": ATV_Hierarchy, four wheels on A-arms), drawn over a Reflex ATV.
        public bool atv;
        /// The hierarchy the Reflex vehicle it rides as has (a bike's or an ATV's): another means a vehicle of its own size.
        public string StandardHierarchy => atv ? "ATV_Hierarchy" : "MX_Hierarchy";
        /// The Reflex bike (VehicleModelId) it rides as, -1: none (ownClass).
        public int ridesAs = -1; public string ridesAsName = "";
        /// A bike with no Reflex equivalent: the vehicle class it needs ("mx50", "mx650f"); null: it rides as ridesAs.
        public string ownClass;
        public (string db, string pkg) chassis, kit;
        public readonly List<AlivePart> parts = new List<AlivePart>();
        /// The default parts' colours (VehiclePartCustomColor -> CustomColor.ConstantName, CustomDynamicColor RGB), by the material constant they
        /// set ("usr_mxhubdyncolor"): Alive's run-time colours, over the materials' own defaults.
        public readonly Dictionary<string, float[]> colors = new Dictionary<string, float[]>(StringComparer.OrdinalIgnoreCase);
        /// Why it cannot be imported (null: it can).
        public string problem;
        public bool Importable => problem == null && ridesAs >= 0;
        public string Label => (brand.Length > 0 ? brand + " " : "") + model;
        public string Slug => "alive-" + aliveId + "-" + new string(Label.ToLowerInvariant().Select(c => char.IsLetterOrDigit(c) ? c : '-').ToArray()).Trim('-').Replace("--", "-");
    }

    /// A default part of an Alive bike: its type (VehiclePartType.Name) and its model / material packages (null: none).
    public sealed class AlivePart
    {
        public string type; public (string db, string pkg)? model, materials;
        public override string ToString() => $"{type}: model {(model == null ? "-" : model.Value.db + "/" + model.Value.pkg)}, materials {(materials == null ? "-" : materials.Value.db + "/" + materials.Value.pkg)}";
    }

    public sealed class AliveBikeResult
    {
        public AliveBikeSource bike; public bool ok; public string error, folder;
        public int surfaces, vertices, triangles, maps; public long bytes; public double seconds;
        public readonly List<string> notes = new List<string>();
    }

    public static class AliveBikes
    {
        public const string Folder = "AliveBikes";
        public const string Author = "MX vs ATV Alive";

        // the Reflex bikes of each class by brand (core/plan/RXMOD.md "The original bikes"): Blaze, Dakumoto, KTM, Shima, Takashi
        static readonly string[] Brands = { "Blaze", "Dakumoto", "KTM", "Shima", "Takashi" };
        static readonly Dictionary<string, (int[] ids, string[] names)> Classes = new Dictionary<string, (int[], string[])>
        {
            { "mx450f_vehicle", (new[] { 1, 14, 28, 10, 32 }, new[] { "Blaze 450F", "Dakumoto 450F", "KTM 450 SX-F", "Shima 450F", "Takashi 450F" }) },
            { "mx250f_vehicle", (new[] { 11, 13, 18, 17, 31 }, new[] { "Blaze 250F", "Dakumoto 250F", "KTM 250 SX-F", "Shima 250F", "Takashi 250F" }) },
            { "mx125_vehicle", (new[] { 12, 35, 36, 6, 29 }, new[] { "Blaze 125S", "Dakumoto 125", "KTM 125 SX", "Shima 125R", "Takashi 125R" }) },
            { "mx250_vehicle", (new[] { 27, 16, 15, 9, 30 }, new[] { "Blaze 250R", "Dakumoto 250S", "KTM 250 SX", "Shima 250R", "Takashi 250R" }) },
            // the Reflex ATVs (VehicleType ATV, one class: atv_vehicle) by brand; the ATV1000's slot: the Raven ATV (a brand no Alive ATV takes)
            { "atv_vehicle", (new[] { 0, 22, 74, 20, 21 }, new[] { "Blaze ATV", "Dakumoto ATV", "KTM 450 SX ATV", "Shima ATV", "Takashi ATV" }) },
            { "atv_vehicle#raven", (new[] { 23, 23, 23, 23, 23 }, new[] { "Raven ATV", "Raven ATV", "Raven ATV", "Raven ATV", "Raven ATV" }) },
        };
        /// Alive's brand -> the Reflex brand of the bike it rides as (the original's fictional brands; Kawasaki = Dakumoto, no brand = Dakumoto).
        static int BrandIndex(string brand)
        {
            switch ((brand ?? "").ToLowerInvariant())
            {
                case "blaze": case "honda": return 0;
                case "dakumoto": case "kawasaki": return 1;
                case "ktm": return 2;
                case "shima": case "suzuki": return 3;
                case "takashi": case "yamaha": return 4;
                default: return 1;
            }
        }

        // ------------------------------------------------------------------------------------- the tables

        /// Every row of the tables of the sources, by table name (a later source's row with the same first column replaces an earlier's).
        internal sealed class Tables
        {
            public readonly Dictionary<string, List<Dictionary<string, object>>> rows = new Dictionary<string, List<Dictionary<string, object>>>(StringComparer.Ordinal);
            public readonly Dictionary<string, string> strings = new Dictionary<string, string>(StringComparer.Ordinal);
            public readonly Dictionary<string, string> sourceOf = new Dictionary<string, string>(StringComparer.Ordinal);   // "VehicleModel:<id>" -> the volume's name
            public IEnumerable<Dictionary<string, object>> Of(string table) => rows.TryGetValue(table, out var l) ? l : Enumerable.Empty<Dictionary<string, object>>();
            public static long? L(Dictionary<string, object> r, string k) => r != null && r.TryGetValue(k, out var v) && v is long x ? x : (long?)null;
            public static string S(Dictionary<string, object> r, string k) => r != null && r.TryGetValue(k, out var v) ? v as string : null;
            public static double? D(Dictionary<string, object> r, string k) => r != null && r.TryGetValue(k, out var v) ? v is double d ? d : v is long l ? l : v is float f ? f : (double?)null : null;

            public void Add(byte[] tbldata, string source)
            {
                var doc = Bxml.Parse(tbldata, scene: true);
                for (int i = 0; i < doc.nodes.Count; i++)
                {
                    if (doc.Tag(i) != "Table") continue;
                    string name = null; int rowsNode = -1;
                    foreach (int c in doc.Children(i))
                    {
                        if (doc.Tag(c) == "Name") name = doc.Value(doc.nodes[c]) as string;
                        else if (doc.Tag(c) == "Rows") rowsNode = c;
                    }
                    if (name == null || rowsNode < 0) continue;
                    if (!rows.TryGetValue(name, out var list)) rows[name] = list = new List<Dictionary<string, object>>();
                    foreach (int r in doc.Children(rowsNode))
                    {
                        if (doc.Tag(r) != "Row") continue;
                        var d = new Dictionary<string, object>(StringComparer.Ordinal);
                        foreach (var kv in doc.AttributesAt(r)) d[kv.Key] = kv.Value;
                        list.Add(d);
                        if (name == "VehicleModel" && L(d, "VehicleModelId") is long id) sourceOf["VehicleModel:" + id] = source;
                    }
                }
            }
        }

        static Tables ReadTables(AliveSources r, CancellationToken cancel)
        {
            var t = new Tables();
            foreach (var v in r.volumes.OrderBy(AliveImporter.PackOrder))
                foreach (var db in v.files.Where(f => f.Name.EndsWith(".xenon.database", StringComparison.OrdinalIgnoreCase)).OrderBy(f => f.Name, StringComparer.Ordinal))
                {
                    cancel.ThrowIfCancellationRequested();
                    if (db.Name.StartsWith("MXTables_", StringComparison.Ordinal) && !db.Name.StartsWith("MXTables_Schema", StringComparison.Ordinal) && !db.Name.StartsWith("MXTables_Leaderboards", StringComparison.Ordinal))
                        foreach (var a in Package.All(v, db, (p, n, ty) => ty == "tbldata", cancel)) t.Add(a.data, v.displayName);
                    else if (db.Name.StartsWith("MXUI", StringComparison.Ordinal))
                        foreach (var a in Package.All(v, db, (p, n, ty) => ty == "localiz" && n.EndsWith("StoreProducts.ENG", StringComparison.Ordinal), cancel))
                            foreach (var kv in AliveImporter.LocalizStrings(a.data)) t.strings[kv.Key] = kv.Value;
                }
            return t;
        }

        // ------------------------------------------------------------------------------------- finding the bikes

        /// The Alive motorcycles of the sources (the tables joined; each with its parts and what it rides as).
        public static List<AliveBikeSource> Find(AliveSources r, CancellationToken cancel)
        {
            var t = ReadTables(r, cancel);
            var index = new PackageIndex(r.volumes);
            string Str(long? id) => id is long x && t.strings.TryGetValue("STR_" + x, out var s) ? s : null;
            var products = t.Of("StoreProducts").Where(x => Tables.L(x, "StoreProductId") != null).GroupBy(x => Tables.L(x, "StoreProductId").Value).ToDictionary(g => g.Key, g => g.Last());
            var assets = t.Of("StoreAssets").Where(x => Tables.L(x, "StoreAssetId") != null).GroupBy(x => Tables.L(x, "StoreAssetId").Value).ToDictionary(g => g.Key, g => Tables.L(g.Last(), "StoreProductId"));
            var brands = t.Of("Brand").Where(x => Tables.L(x, "BrandId") != null).GroupBy(x => Tables.L(x, "BrandId").Value).ToDictionary(g => g.Key, g => Tables.S(g.Last(), "BrandName"));
            var types = t.Of("VehicleType").Where(x => Tables.L(x, "VehicleTypeId") != null).GroupBy(x => Tables.L(x, "VehicleTypeId").Value).ToDictionary(g => g.Key, g => g.Last());
            var hierarchies = t.Of("VehicleHierarchies").Where(x => Tables.L(x, "VehicleHierarchyId") != null).GroupBy(x => Tables.L(x, "VehicleHierarchyId").Value).ToDictionary(g => g.Key, g => Tables.S(g.Last(), "AssetName"));
            var partTypes = t.Of("VehiclePartType").Where(x => Tables.L(x, "VehiclePartTypeId") != null).GroupBy(x => Tables.L(x, "VehiclePartTypeId").Value).ToDictionary(g => g.Key, g => Tables.S(g.Last(), "Name"));
            var parts = t.Of("VehiclePart").Where(x => Tables.L(x, "VehiclePartId") != null).GroupBy(x => Tables.L(x, "VehiclePartId").Value).ToDictionary(g => g.Key, g => g.Last());
            var pkgs = t.Of("AssetPackage").Where(x => Tables.L(x, "PackageId") != null).GroupBy(x => Tables.L(x, "PackageId").Value).ToDictionary(g => g.Key, g => Tables.S(g.Last(), "PackageName"));
            var dbs = t.Of("AssetDatabase").Where(x => Tables.L(x, "DatabaseId") != null).GroupBy(x => Tables.L(x, "DatabaseId").Value).ToDictionary(g => g.Key, g => Tables.S(g.Last(), "DatabaseName"));
            var pkgDb = t.Of("AssetPackageDatabase").Where(x => Tables.L(x, "PackageDatabaseId") != null).GroupBy(x => Tables.L(x, "PackageDatabaseId").Value)
                         .ToDictionary(g => g.Key, g => (pkg: pkgs.TryGetValue(Tables.L(g.Last(), "PackageId") ?? -1, out var p) ? p : null, db: dbs.TryGetValue(Tables.L(g.Last(), "DatabaseId") ?? -1, out var d) ? d : null));
            (string db, string pkg)? PkgOf(long? pdb) => pdb is long x && pkgDb.TryGetValue(x, out var v) && v.pkg != null && v.db != null ? (v.db, v.pkg) : ((string, string)?)null;

            var colorNames = t.Of("CustomColor").Where(x => Tables.L(x, "CustomColorId") != null).GroupBy(x => Tables.L(x, "CustomColorId").Value).ToDictionary(g => g.Key, g => Tables.S(g.Last(), "ConstantName"));
            var dynColors = t.Of("CustomDynamicColor").Where(x => Tables.L(x, "CustomDynamicColorId") != null).GroupBy(x => Tables.L(x, "CustomDynamicColorId").Value).ToDictionary(g => g.Key, g => g.Last());
            var partColors = t.Of("VehiclePartCustomColor").Where(x => Tables.L(x, "VehiclePartId") != null).GroupBy(x => Tables.L(x, "VehiclePartId").Value).ToDictionary(g => g.Key, g => g.ToList());
            var result = new List<AliveBikeSource>();
            foreach (var m in t.Of("VehicleModel").GroupBy(x => Tables.L(x, "VehicleModelId") ?? -1).Select(g => g.Last()).OrderBy(x => Tables.L(x, "VehicleModelId")))
            {
                cancel.ThrowIfCancellationRequested();
                long id = Tables.L(m, "VehicleModelId") ?? -1;
                if (!types.TryGetValue(Tables.L(m, "VehicleTypeId") ?? -1, out var type)) continue;
                string factory = Tables.S(type, "FactoryName");
                if (factory != "MotocrossBike" && factory != "Atv") continue;   // (Alive's VehicleModel rows: motorcycles and ATVs only)
                var b = new AliveBikeSource { aliveId = (int)id, vehicleType = Tables.S(type, "Name") ?? "", atv = factory == "Atv" };
                if (b.atv) b.hierarchy = "ATV_Hierarchy";
                t.sourceOf.TryGetValue("VehicleModel:" + id, out b.pack);
                if (string.IsNullOrEmpty(b.pack)) b.pack = "MX vs. ATV Alive (disc)";
                if (hierarchies.TryGetValue(Tables.L(m, "VehicleHierarchyId") ?? -1, out var h) && !string.IsNullOrEmpty(h)) b.hierarchy = h;
                long? product = assets.TryGetValue(Tables.L(m, "StoreAssetId") ?? -1, out var sp) ? sp : null;
                if (product is long pid && products.TryGetValue(pid, out var prow))
                {
                    b.product = Str(pid) ?? "";
                    if (brands.TryGetValue(Tables.L(prow, "BrandId") ?? -1, out var bn)) b.brand = bn ?? "";
                }
                foreach (var mp in t.Of("VehicleModelParts").Where(x => Tables.L(x, "VehicleModelId") == id && Tables.L(x, "Default") == 1))
                {
                    if (!parts.TryGetValue(Tables.L(mp, "VehiclePartId") ?? -1, out var part)) continue;
                    if (partColors.TryGetValue(Tables.L(mp, "VehiclePartId") ?? -1, out var pcs))
                        foreach (var pc in pcs)
                            if (colorNames.TryGetValue(Tables.L(pc, "CustomColorId") ?? -1, out var cn) && !string.IsNullOrEmpty(cn) && dynColors.TryGetValue(Tables.L(pc, "CustomDynamicColorId") ?? -1, out var dc)
                                && Tables.D(dc, "R") is double cr && Tables.D(dc, "G") is double cg && Tables.D(dc, "B") is double cb)
                                b.colors["usr_" + cn] = new[] { (float)cr, (float)cg, (float)cb, 1f };
                    var p = new AlivePart { type = partTypes.TryGetValue(Tables.L(part, "VehiclePartTypeId") ?? -1, out var pt) ? pt : "?",
                                            model = PkgOf(Tables.L(part, "ModelPackageDatabaseId")), materials = PkgOf(Tables.L(part, "MaterialPackageDatabaseId")) };
                    if (p.model == null && p.materials == null) continue;
                    if (b.parts.Any(x => x.type == p.type)) continue;   // (one of each type: the first default)
                    if (p.type == "GraphicKits") { b.chassis = p.model ?? default; b.kit = p.materials ?? default; }
                    b.parts.Add(p);
                }
                Name(b);
                Classify(b, index, cancel);
                result.Add(b);
            }
            return result;
        }

        /// The garage card's brand / model lines from the store product's name.
        static void Name(AliveBikeSource b)
        {
            string p = (b.product ?? "").Trim();
            if (p.EndsWith(" MX", StringComparison.Ordinal)) p = p.Substring(0, p.Length - 3);
            if (b.aliveId == 30 || b.vehicleType == "MX50") { b.brand = "MX vs ATV"; b.model = "50cc Mini"; return; }
            if (b.vehicleType == "MX650") { b.brand = "MX vs ATV"; b.model = "MXFR 650"; return; }
            if (b.vehicleType == "ATV1000") { b.brand = "MX vs ATV"; b.model = "ATV 1000 Dune"; return; }
            if (p.EndsWith(" ATV", StringComparison.Ordinal)) p = p.Substring(0, p.Length - 4);   // (the DLC ATVs' products: "Honda TRX450R ATV")
            if (p.StartsWith("DLC", StringComparison.Ordinal)) { int s = p.IndexOf(' '); p = s > 0 ? p.Substring(s + 1) : p; }   // "DLC0011 MX250 FMX"
            if (b.brand.Length > 0 && p.StartsWith(b.brand + " ", StringComparison.OrdinalIgnoreCase)) p = p.Substring(b.brand.Length + 1);
            bool baseAtv = b.atv && p.StartsWith("ATV ", StringComparison.Ordinal);   // (the disc's "Takashi ATV 250f": the card reads ATV 250F)
            if (baseAtv) p = p.Substring(4);
            if (p.StartsWith("MX ", StringComparison.Ordinal)) p = p.Substring(3);   // "Blaze MX 450f"
            if (p.EndsWith("f", StringComparison.Ordinal) && p.Length > 1 && char.IsDigit(p[p.Length - 2])) p = p.Substring(0, p.Length - 1) + "F";
            if (b.brand.Length == 0) b.brand = "MX vs ATV";
            b.model = p.Length > 0 ? p : "Bike " + b.aliveId;
            if (baseAtv) b.model = "ATV " + b.model;
        }

        /// What it rides as (its chassis' physics file and its brand), or why it cannot be imported.
        static void Classify(AliveBikeSource b, PackageIndex index, CancellationToken cancel)
        {
            if (b.chassis.pkg == null) { b.problem = "no chassis (graphic kit part) in the tables"; return; }
            var chassis = index.Read(b.chassis.db, b.chassis.pkg, cancel);
            if (chassis == null) { b.problem = $"{b.chassis.db}/{b.chassis.pkg} is not in the sources"; return; }
            b.physics = chassis.assets.FirstOrDefault(a => a.type == "bxml" && a.name.EndsWith("_vehicle", StringComparison.Ordinal)).name ?? "";
            // the 50cc / the MXFR650: their own physics (Alive's file over a Reflex class's: core rx_session_vehicle_physics), drawn over the Reflex
            // bike of the nearest class (its garage slot and ownership): a 125 for the 50cc, a 450F for the 650
            // an ATV: Reflex has one ATV class (atv_vehicle): Alive's 250 and 450 ATVs ride as the Reflex ATV of their brand; the ATV1000 dune ATV
            // (a 2.3 ft swingarm, its own ATVFR_Hierarchy) on its own physics (atvDune1000_vehicle) in the Raven ATV's slot
            string physics = b.physics;
            if (b.physics == "mx50_vehicle" || b.physics == "mx650f_vehicle") { b.ownClass = b.physics.Substring(0, b.physics.Length - 8); physics = b.ownClass == "mx50" ? "mx125_vehicle" : "mx450f_vehicle"; }
            if (b.physics == "atvDune1000_vehicle") { b.ownClass = "atv1000"; physics = "atv_vehicle#raven"; }
            else if (b.atv && (b.physics == "atv250f_vehicle" || b.physics == "atv450f_vehicle")) physics = "atv_vehicle";
            if (!Classes.TryGetValue(physics, out var c)) { b.problem = "unknown physics file " + (b.physics.Length > 0 ? b.physics : "(none)"); return; }
            if (b.hierarchy != b.StandardHierarchy && b.ownClass == null) { b.problem = "hierarchy " + b.hierarchy; return; }
            int k = BrandIndex(b.brand);
            b.ridesAs = c.ids[k]; b.ridesAsName = c.names[k];
        }

        // ------------------------------------------------------------------------------------- packages

        internal sealed class PackageAssets
        {
            public string db, pkg; public readonly List<(string name, string type, byte[] data)> assets = new List<(string, string, byte[])>();
            public byte[] Find(string name, string type) { foreach (var a in assets) if (a.type == type && string.Equals(a.name, name, StringComparison.OrdinalIgnoreCase)) return a.data; return null; }
            public IEnumerable<string> Names(string type) => assets.Where(a => a.type == type).Select(a => a.name);
        }

        internal static class Package
        {
            /// alive_sources.iter_assets with the asset names: the decoded heap resources of a database's packages that `want` selects.
            public static IEnumerable<(string pkg, string name, string type, byte[] data)> All(Volume v, VolumeFile database, Func<string, string, string, bool> want, CancellationToken cancel)
            {
                var tree = Bxml.Parse(v.Read(database, cancel)).ToElement();
                foreach (var package in tree.FindAll("Packages").SelectMany(p => p.FindAll("Package")))
                {
                    string filename = package.Get("file"), pname = package.Get("name");
                    Bin.Require(!string.IsNullOrEmpty(filename) && !filename.Contains("/") && !filename.Contains("\\"), "Package file name");
                    var list = package.FindAll("Asset").Where(a => want(pname, a.Get("name"), a.Get("type"))).ToList();
                    if (list.Count == 0) continue;
                    var pf = v.Find((database.Directory.Length > 0 ? database.Directory + "/" : "") + filename + ".xenon.package");
                    Bin.Require(pf != null, filename + ".xenon.package not found");
                    foreach (var asset in list)
                    {
                        var heap = asset.Find("Heap");
                        long pos = (long)XMem.MetadataUint(package.Get("offset")) + XMem.MetadataUint(heap.Get("offset"));
                        long size = XMem.MetadataUint(heap.Get("size"));
                        Bin.Require(pos + size <= pf.size, "Asset exceeds source package");
                        var block = asset.Find("Block");
                        Bin.Require(block == null || XMem.MetadataUint(block.Get("size")) == 0, "Unsupported block asset");
                        var data = size == 0 ? Array.Empty<byte>() : v.ReadRange(pf, pos, (int)size, cancel);
                        var comp = asset.Find("Compress");
                        if (comp != null && comp.Get("enabled") == "_bool:true")
                        {
                            Bin.Require(comp.Get("codec") == "LZX", "Unsupported compression");
                            data = XMem.Decode(data, cancel);
                        }
                        yield return (pname, asset.Get("name"), asset.Get("type"), data);
                    }
                }
            }
        }

        /// Every <db>.xenon.database of the sources by name; a package's assets read once.
        internal sealed class PackageIndex
        {
            readonly Dictionary<string, (Volume v, VolumeFile f)> dbs = new Dictionary<string, (Volume, VolumeFile)>(StringComparer.OrdinalIgnoreCase);
            readonly Dictionary<string, PackageAssets> cache = new Dictionary<string, PackageAssets>(StringComparer.OrdinalIgnoreCase);
            public PackageIndex(IEnumerable<Volume> volumes)
            {
                foreach (var v in volumes)
                    foreach (var f in v.files.Where(f => f.Name.EndsWith(".xenon.database", StringComparison.OrdinalIgnoreCase)))
                    {
                        string n = f.Name.Substring(0, f.Name.Length - ".xenon.database".Length);
                        if (!dbs.ContainsKey(n)) dbs[n] = (v, f);
                    }
            }
            public bool Has(string db) => db != null && dbs.ContainsKey(db);
            public string SourceOf(string db) => db != null && dbs.TryGetValue(db, out var e) ? e.v.displayName : null;
            /// The package's assets (null: its database is not in the sources, or it has no such package).
            public PackageAssets Read(string db, string pkg, CancellationToken cancel)
            {
                if (db == null || pkg == null || !dbs.TryGetValue(db, out var e)) return null;
                string key = db + "/" + pkg;
                if (cache.TryGetValue(key, out var c)) return c;
                var r = new PackageAssets { db = db, pkg = pkg };
                foreach (var a in Package.All(e.v, e.f, (p, n, t) => string.Equals(p, pkg, StringComparison.OrdinalIgnoreCase), cancel)) r.assets.Add((a.name, a.type, a.data));
                if (r.assets.Count == 0 && !PackageExists(e.v, e.f, pkg, cancel)) r = null;
                cache[key] = r;
                return r;
            }
            static bool PackageExists(Volume v, VolumeFile f, string pkg, CancellationToken cancel)
            {
                var tree = Bxml.Parse(v.Read(f, cancel)).ToElement();
                return tree.FindAll("Packages").SelectMany(p => p.FindAll("Package")).Any(p => string.Equals(p.Get("name"), pkg, StringComparison.OrdinalIgnoreCase));
            }
        }

        // ------------------------------------------------------------------------------------- converting a bike

        /// A resolved material: where it is, its parsed (converted) definition.
        sealed class Mat { public string key; public PackageAssets pkg; public Rfx.Material def; public string template; }

        /// The bike's model assembled in the chassis' model space (feet, X right, Y up, Z forward): the .rxmesh's surfaces.
        sealed class Part { public Rfx.Surface s; public int node; public Mat mat; public string from; }

        /// Converts one bike into a .rxmod folder at `folder` (created; must not exist). Returns the result (ok or why not).
        public static AliveBikeResult Convert(AliveBikeSource b, AliveSources sources, string folder, CancellationToken cancel)
        {
            var res = new AliveBikeResult { bike = b, folder = folder };
            var clock = System.Diagnostics.Stopwatch.StartNew();
            try { ConvertBike(b, new PackageIndex(sources.volumes), folder, res, cancel); res.ok = true; }
            catch (OperationCanceledException) { throw; }
            catch (Exception e) when (e is AliveImportException || e is InvalidDataException || e is IOException || e is ArgumentException || e is IndexOutOfRangeException)
            { res.error = e.Message; }
            res.seconds = clock.Elapsed.TotalSeconds;
            return res;
        }

        static void ConvertBike(AliveBikeSource b, PackageIndex index, string folder, AliveBikeResult res, CancellationToken cancel)
        {
            Bin.Require(b.Importable, b.problem ?? "rides as no Reflex bike");
            var chassis = index.Read(b.chassis.db, b.chassis.pkg, cancel) ?? throw new AliveImportException($"{b.chassis.db}/{b.chassis.pkg} is not in the sources");
            // the material pool in part order, the graphic kit first (BikeSpec.materials)
            var pool = new List<PackageAssets>();
            void AddPool((string db, string pkg)? p) { if (p == null) return; var a = index.Read(p.Value.db, p.Value.pkg, cancel); if (a != null && !pool.Contains(a)) pool.Add(a); }
            AddPool(b.kit);
            foreach (var p in b.parts) if (p.type != "GraphicKits") AddPool(p.materials);

            var body = ReadModel(chassis, ModelName(chassis, b.hierarchy), out var bodySurfaces);
            var hierBytes = chassis.Find(b.hierarchy, "model");
            var hier = hierBytes != null ? Rfx.ParseModel(Converters.Model(hierBytes)) : body;
            var mats = new Dictionary<string, Mat>(StringComparer.OrdinalIgnoreCase);
            var parts = new List<Part>();
            int NodeOf(string name) { int i = hier.NodeIndex(name); return i >= 0 ? i : 0; }
            Matrix4x4 BindOf(string name) { int i = body.NodeIndex(name); return i >= 0 ? body.nodes[i].world : hier.nodes[NodeOf(name)].world; }

            // the chassis, first LOD: static surfaces carry their node's world matrix, skinned ones are stored in model space
            foreach (var bd in body.Lod(0))
            {
                var s = bodySurfaces[bd.surface];
                parts.Add(new Part { s = Place(s, s.skinned ? Matrix4x4.identity : body.nodes[bd.node].world, body, hier), node = NodeOf(body.nodes[bd.node].name),
                                     mat = Resolve(mats, bd.material, chassis, pool, index, cancel), from = b.chassis.pkg });
            }
            // the parts: tyres on the chassis' tyre nodes, the others on the node their model's first node names
            foreach (var p in b.parts.Where(p => p.type != "GraphicKits" && p.model != null))
            {
                var pa = index.Read(p.model.Value.db, p.model.Value.pkg, cancel);
                if (pa == null) { res.notes.Add($"{p.type}: {p.model.Value.db}/{p.model.Value.pkg} is not in the sources"); continue; }
                var own = p.materials != null ? index.Read(p.materials.Value.db, p.materials.Value.pkg, cancel) : null;
                foreach (string mn in pa.Names("model").Where(n => n.IndexOf("Hierarchy", StringComparison.OrdinalIgnoreCase) < 0).ToList())
                {
                    var pm = ReadModel(pa, mn, out var ps);
                    string attach = pm.nodes[0].name;
                    if (hier.NodeIndex(attach) < 0 && body.NodeIndex(attach) < 0) { res.notes.Add($"{p.type}: {mn} hangs on {attach}, not a node of the bike"); continue; }
                    var attachWorld = BindOf(attach); var rootInv = RigidInverse(pm.nodes[0].world);
                    foreach (var bd in pm.Lod(0))
                    {
                        if (bd.material.IndexOf("Stand", StringComparison.OrdinalIgnoreCase) >= 0) continue;
                        var s = ps[bd.surface];
                        string bound = pm.nodes[bd.node].name;
                        bool onNode = bd.node != 0 && !string.Equals(bound, attach, StringComparison.OrdinalIgnoreCase) && (hier.NodeIndex(bound) >= 0 || body.NodeIndex(bound) >= 0);
                        var m = s.skinned ? Matrix4x4.identity : onNode ? BindOf(bound) : attachWorld * rootInv * pm.nodes[bd.node].world;
                        var mat = Resolve(mats, bd.material, pa, own != null ? new List<PackageAssets> { own }.Concat(pool).ToList() : pool, index, cancel);
                        parts.Add(new Part { s = Place(s, m, pm, hier), node = NodeOf(onNode ? bound : attach), mat = mat, from = p.model.Value.pkg });
                    }
                }
            }
            Bin.Require(parts.Count > 0, "the bike's model has no surfaces");
            Bin.Require(b.atv ? hier.NodeIndex("SWP_FRT_RHT_Tire") >= 0 && hier.NodeIndex("SWP_BCK_LFT_Tire") >= 0 : hier.NodeIndex("OBJ_FRT_Tire") >= 0 && hier.NodeIndex("OBJ_BCK_Tire") >= 0, "the hierarchy has no tyre nodes");

            // the maps (one PNG per texture) and the manifest's materials
            Directory.CreateDirectory(folder);
            var written = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
            var decoded = new Dictionary<string, (int w, int h, byte[] px)?>(StringComparer.OrdinalIgnoreCase);
            (int w, int h, byte[] px)? Pixels(Mat m, string texture)
            {
                if (string.IsNullOrEmpty(texture)) return null;
                if (decoded.TryGetValue(texture, out var c)) return c;
                byte[] src = m.pkg.Find(texture, "texture") ?? chassis.Find(texture, "texture") ?? pool.Select(x => x.Find(texture, "texture")).FirstOrDefault(x => x != null);
                if (src == null) res.notes.Add($"material {m.key}: texture {texture} not found");
                (int w, int h, byte[] px)? d = null;
                try { d = src == null ? null : AliveBikeMaps.Decode(Textures.Convert(src).Item1); }
                catch (AliveImportException e) { res.notes.Add($"material {m.key}: texture {texture}: {e.Message}"); return decoded[texture] = null; }
                if (src != null && d == null) res.notes.Add($"material {m.key}: texture {texture}: format not converted");
                return decoded[texture] = d;
            }
            string Write(string entryName, (int w, int h, byte[] px)? img)
            {
                if (img == null) return null;
                string entry = "maps/" + Safe(entryName) + ".png";
                if (written.ContainsKey(entry)) return entry;
                var png = AliveBikeMaps.Png(img.Value.w, img.Value.h, img.Value.px);
                Directory.CreateDirectory(Path.Combine(folder, "maps"));
                File.WriteAllBytes(Path.Combine(folder, entry.Replace('/', Path.DirectorySeparatorChar)), png);
                res.maps++; res.bytes += png.Length; written[entry] = entry;
                return entry;
            }
            var used = parts.Select(p => p.mat).Distinct().ToList();
            // Alive's part colours (the tables) over the materials' own constants
            foreach (var m in used)
                foreach (var k in m.def.parameters.Keys.ToList())
                    if (b.colors.TryGetValue(k.TrimEnd('_'), out var col)) m.def.parameters[k] = col;   // (a few materials spell the constant with a '_' at its end: usr_atvarmdyncolor_)
            var materialsJson = new JObj();
            foreach (var m in used)
            {
                var (diffuse, sfx, normal) = Roles(m);
                res.notes.Add($"material {m.key} ({m.pkg.pkg}): {m.def.pixelShader} [{string.Join(", ", m.def.textures)}] {{{string.Join(" ", m.def.parameters.Select(kv => kv.Key + "=" + string.Join(",", kv.Value.Select(x => x.ToString("0.###")))))}}} -> {m.template}: diffuse {diffuse ?? "-"}, sfx {sfx ?? "-"}, normal {normal ?? "-"}");
                var o = new JObj().Set("template", m.template);
                var D = Pixels(m, diffuse); var C = Pixels(m, sfx);
                string d = null, x = null;
                bool dynPlastic = (m.def.pixelShader ?? "").IndexOf("DynPlastic", StringComparison.OrdinalIgnoreCase) >= 0;
                bool wheel = (m.def.pixelShader ?? "").IndexOf("Wheel", StringComparison.OrdinalIgnoreCase) >= 0;
                // the ATV programs that colour a combo map at run time (Template_ATV_FrameRefl_FNL: frame / swing arm / A-arm colour; Template_Vehicle_Wheel_FNL:
                // the rim colour; Template_ATV_Spring_FNL: usr_color): their "diffuse" is the combo map (R the shading, G mud)
                string ps = m.def.pixelShader ?? "";
                float[] tint = null;
                float[] P(string k) => m.def.Param(k) ?? m.def.Param(k + "_");
                if (ps.IndexOf("ATV_FrameRefl", StringComparison.OrdinalIgnoreCase) >= 0)
                    tint = (m.key.IndexOf("Swing", StringComparison.OrdinalIgnoreCase) >= 0 ? P("usr_atvswingdyncolor") : P("usr_atvframedyncolor")) ?? P("usr_frame_color");
                else if (ps.IndexOf("Aarms", StringComparison.OrdinalIgnoreCase) >= 0) tint = P("usr_atvarmdyncolor");
                else if (ps.IndexOf("Vehicle_Wheel", StringComparison.OrdinalIgnoreCase) >= 0) tint = m.def.Param("usr_atvrimdyncolor");
                else if (ps.IndexOf("ATV_Spring", StringComparison.OrdinalIgnoreCase) >= 0) tint = m.def.Param("usr_color");
                var combo = tint != null ? (D ?? C) : null;
                if (combo != null)
                {
                    d = Write(m.key + "_diffuse", AliveBikeMaps.BakeTint(combo.Value, tint));
                    x = Write((D != null ? diffuse : sfx) + "_sfx", AliveBikeMaps.Channels(combo.Value, 0, 1, 0));
                }
                else if (dynPlastic && D != null)
                {   // the dynamic colour and the plate's colour baked in; R of the SFX (the plate mask) cleared: Reflex's plastics light the diffuse by it
                    d = Write(m.key + "_diffuse", AliveBikeMaps.BakePlastic(D.Value, C, m.def.Param("usr_userdyncolor"), m.def.Param("usr_dynnumplate")));
                    if (C != null) x = Write(m.key + "_sfx", AliveBikeMaps.Channels(C.Value, -1, 1, 2));
                    if (m.def.textures.Any(t => string.Equals(t, "Numbers", StringComparison.OrdinalIgnoreCase))) o.Set("numbers", true);   // (the rider's plate on uv1, as Alive's own)
                }
                else if (wheel && D == null && C != null)
                {
                    bool hub = m.key.IndexOf("Hub", StringComparison.OrdinalIgnoreCase) >= 0;
                    var col = m.def.Param(hub ? "usr_mxhubdyncolor" : "usr_mxrimdyncolor") ?? m.def.parameters.Where(kv => kv.Key.IndexOf("dyncolor", StringComparison.OrdinalIgnoreCase) >= 0).Select(kv => kv.Value).FirstOrDefault();
                    d = Write(m.key + "_diffuse", AliveBikeMaps.BakeWheel(C.Value, col, hub));
                    x = Write(sfx + "_wheel", AliveBikeMaps.Channels(C.Value, 2, 1, 2));   // (R the shine where the colour is, G mud, B the reflection)
                }
                else
                {
                    d = Write(diffuse, D);
                    // Alive's frame SFX (Template_MX_FrameRefl_FNL) keeps the shine in R and a flat B: B := R (Reflex's frame combo: B the reflection)
                    if (C != null) x = m.template == "frame" && (m.def.pixelShader ?? "").IndexOf("FrameRefl", StringComparison.OrdinalIgnoreCase) >= 0
                        ? Write(sfx + "_frame", AliveBikeMaps.Channels(C.Value, 0, 1, 0)) : Write(sfx, C);
                }
                if (d != null) o.Set("diffuse", d);
                if (x != null) o.Set("sfx", x);
                string n = Write(normal, Pixels(m, normal)); if (n != null) o.Set("normal", n);
                if (m.def.alphaTest) o.Set("alphaTest", true).Set("alphaRef", (long)Math.Max(1, Math.Min(255, m.def.alphaRef)));
                if (m.def.cull == 1) o.Set("twoSided", true);
                materialsJson.Set(m.key, o);
            }

            // the mesh
            var mesh = WriteMesh(hier, parts, used, res);
            File.WriteAllBytes(Path.Combine(folder, "bike.rxmesh"), mesh);
            res.bytes += mesh.Length;
            // a bike of its own class: Alive's physics file as it is in the install (the mx*_vehicle schema the core reads), and for a bike of another
            // size (the 50cc: its own hierarchy) that hierarchy's rest locals, which the core gives the vehicle's model (rx_session_vehicle_physics_nodes)
            bool ownSize = b.ownClass != null && b.hierarchy != b.StandardHierarchy;
            JObj physicsJson = null;
            if (b.ownClass != null)
            {
                var file = chassis.Find(b.physics, "bxml") ?? throw new AliveImportException($"{b.chassis.db}/{b.chassis.pkg}: no {b.physics}");
                Directory.CreateDirectory(Path.Combine(folder, "physics"));
                File.WriteAllBytes(Path.Combine(folder, "physics", b.physics + ".bxml"), file);
                res.bytes += file.Length;
                physicsJson = new JObj().Set("file", "physics/" + b.physics + ".bxml").Set("class", b.ownClass);
                if (ownSize)
                {
                    var sb = new StringBuilder();
                    foreach (var n in hier.nodes)
                    {   // the game's row-major local (Rfx.ParseModel transposed it into Unity's column convention)
                        sb.Append(n.name);
                        for (int k = 0; k < 16; k++) sb.Append(' ').Append(n.local[k % 4, k / 4].ToString("R", System.Globalization.CultureInfo.InvariantCulture));
                        sb.Append('\n');
                    }
                    var nodes = Encoding.ASCII.GetBytes(sb.ToString());
                    File.WriteAllBytes(Path.Combine(folder, "physics", "nodes.txt"), nodes);
                    res.bytes += nodes.Length;
                    physicsJson.Set("nodes", "physics/nodes.txt");
                }
            }
            var bike = new JObj().Set("bike", (long)b.ridesAs).Set("original", b.ridesAsName).Set("id", Safe(b.Label).ToLowerInvariant())
                .Set("brand", b.brand).Set("model", b.model).Set("mesh", "bike.rxmesh").Set("fit", ownSize ? "keep" : b.atv ? "nodes" : "axles");
            if (physicsJson != null) bike.Set("physics", physicsJson).Set("class", b.ownClass == "mx50" ? "50CC" : b.ownClass == "atv1000" ? "ATV 1000" : "650");
            if (ownSize) bike.Set("pose", "physics");
            bike.Set("materials", materialsJson);
            var manifest = new JObj().Set("format", "reflex-mod").Set("version", 1L).Set("type", "bikes").Set("id", b.Slug)
                .Set("name", b.Label).Set("author", Author)
                .Set("description", $"{b.product} from MX vs ATV Alive ({b.pack}), imported from your own copy; " + (b.ownClass != null ? $"rides on its own Alive physics ({b.ownClass}) in the {b.ridesAsName}'s garage slot" : $"rides as the {b.ridesAsName}"))
                .Set("bikes", new List<object> { bike });
            var text = Encoding.UTF8.GetBytes(Json.Dumps(manifest, 1) + "\n");
            File.WriteAllBytes(Path.Combine(folder, "mod.json"), text);
            res.bytes += text.Length;
        }

        /// The model asset of a chassis package (the one that is not the hierarchy).
        static string ModelName(PackageAssets p, string hierarchy) => p.Names("model").FirstOrDefault(n => !string.Equals(n, hierarchy, StringComparison.OrdinalIgnoreCase) && n.IndexOf("Hierarchy", StringComparison.OrdinalIgnoreCase) < 0)
                                                                     ?? throw new AliveImportException($"{p.db}/{p.pkg} has no model");

        static Rfx.Model ReadModel(PackageAssets p, string name, out Rfx.Surface[] surfaces)
        {
            var mb = p.Find(name, "model") ?? throw new AliveImportException($"{p.db}/{p.pkg}: no model {name}");
            var sb = p.Find(name, "surface") ?? throw new AliveImportException($"{p.db}/{p.pkg}: no surfaces {name}");
            var model = Rfx.ParseModel(Converters.Model(mb));
            surfaces = Rfx.ParseSurfaces(Converters.Surfaces(sb));
            Bin.Require(surfaces.Length >= model.surfaceCount, $"{p.db}/{p.pkg}/{name}: surfaces");
            return model;
        }

        /// A surface moved into model space by m; skinned bones mapped from the model's nodes to the hierarchy's (by name).
        static Rfx.Surface Place(Rfx.Surface s, Matrix4x4 m, Rfx.Model model, Rfx.Model hier)
        {
            var o = new Rfx.Surface { skinned = s.skinned, uv0 = s.uv0, uv1 = s.uv1, colors = s.colors, weights = s.weights, positions = new Vector3[s.positions.Length], normals = new Vector3[s.normals.Length] };
            for (int i = 0; i < s.positions.Length; i++) { o.positions[i] = m.MultiplyPoint3x4(s.positions[i]); o.normals[i] = m.MultiplyVector(s.normals[i]).normalized; }
            var tri = s.Triangles(); Rfx.Orient(tri, o.positions, o.normals); o.triangles = tri.ToArray();
            if (s.skinned && s.bones != null)
            {
                o.bones = new int[s.bones.Length];
                for (int i = 0; i < s.bones.Length; i++)
                {
                    int bi = s.bones[i];
                    o.bones[i] = bi < 0 || bi >= model.nodes.Length ? -1 : hier.NodeIndex(model.nodes[bi].name);
                    if (o.bones[i] < 0 && bi >= 0) o.bones[i] = 0;
                }
            }
            else o.skinned = false;
            return o;
        }

        /// The inverse of a rigid (rotation + translation) matrix.
        static Matrix4x4 RigidInverse(Matrix4x4 m)
        {
            var r = Matrix4x4.identity;
            for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) r[i, j] = m[j, i];
            var t = new Vector3(m.m03, m.m13, m.m23);
            var it = -(r.MultiplyVector(t));
            r.m03 = it.x; r.m13 = it.y; r.m23 = it.z;
            return r;
        }

        /// The binding's material as the game resolves it: the first material of the pool (the kit first) whose key prefixes the binding's key
        /// (ReflexVehicles.MaterialKey / ReflexBikeModel.GameMaterial), else the model package's own material of that name.
        static Mat Resolve(Dictionary<string, Mat> cache, string binding, PackageAssets own, List<PackageAssets> pool, PackageIndex index, CancellationToken cancel)
        {
            string kb = MaterialKey(binding);
            PackageAssets at = null; string name = null;
            foreach (var p in pool)
            {
                foreach (string m in p.Names("material"))
                {
                    string km = MaterialKey(m);
                    if (km.Length > 0 && kb.Length >= km.Length && string.Compare(kb, 0, km, 0, km.Length, StringComparison.OrdinalIgnoreCase) == 0) { at = p; name = m; break; }
                }
                if (at != null) break;
            }
            if (at == null && own.Find(binding, "material") != null) { at = own; name = binding; }
            if (at == null) { at = own; name = binding; }
            string key = Safe(name);
            if (cache.TryGetValue(key, out var c)) return c;
            var bytes = at.Find(name, "material");
            var def = bytes != null ? ReadMaterial(name, bytes) : new Rfx.Material { name = name };
            return cache[key] = new Mat { key = key, pkg = at, def = def, template = Template(def) };
        }

        /// An Alive material document (MaterialEditorMaterial) as far as a .rxmod material needs it: its pixel program, its textures in
        /// sampler order, alpha test + reference and the cull mode from its Xenos render states (Converters.Material's state ids: 96 alpha
        /// test, 100 its reference, 56 the cull mode 4 / 6 = none / counter-clockwise). Its sampler states are not read (the bike materials
        /// carry ids the level materials never had).
        static Rfx.Material ReadMaterial(string name, byte[] data)
        {
            var doc = Bxml.Parse(data, scene: true);
            Bin.Require(doc.Tag(0) == "MaterialEditorMaterial", "Material root");
            var m = new Rfx.Material { name = name };
            for (int i = 0; i < doc.nodes.Count; i++)
            {
                string tag = doc.Tag(i);
                if (tag == "PixelShader" && m.pixelShader == null) m.pixelShader = doc.AttributeAt(i, "asset") as string;
                else if (tag == "Texture") m.textures.Add(doc.AttributeAt(i, "textureName") as string ?? "");
                else if (tag == "Parameter" && doc.AttributeAt(i, "name") is string pn)
                {
                    var v = doc.Value(doc.nodes[i]);
                    float[] f = v is double[] da ? da.Select(x => (float)x).ToArray() : v is float[] fa ? fa : v is double d1 ? new[] { (float)d1 } : null;
                    if (f != null && !m.parameters.ContainsKey(pn)) m.parameters[pn] = f;
                }
                else if (tag == "XenonStates" && doc.Value(doc.nodes[i]) is Opaque st)
                    for (int k = 0; k + 7 < st.bytes.Length; k += 8)
                    {
                        uint key = Bin.U32BE(st.bytes, k), val = Bin.U32BE(st.bytes, k + 4);
                        if (key == 96) m.alphaTest = val != 0; else if (key == 100) m.alphaRef = (int)val; else if (key == 56) m.cull = val == 4 ? 1 : 3; else if (key == 60) m.alphaBlend = val != 0;
                    }
            }
            return m;
        }

        /// ReflexVehicles.MaterialKey (FUN_00543fc0): digits dropped, cut before the second '_' (the third after "HR_").
        static string MaterialKey(string name)
        {
            if (string.IsNullOrEmpty(name)) return "";
            int stop = name.Length >= 3 && name[0] == 'H' && name[1] == 'R' && name[2] == '_' ? 3 : 2, underscores = 0;
            var o = new StringBuilder(name.Length);
            foreach (char c in name)
            {
                if (c >= '0' && c <= '9') continue;
                if (c == '_' && o.Length > 0 && o[o.Length - 1] == '_') continue;
                if (c == '_' && ++underscores == stop) break;
                o.Append(c);
            }
            return o.ToString();
        }

        /// The .rxmod template (the Reflex program the material borrows) for an Alive material, by its pixel program, else its name.
        static string Template(Rfx.Material m)
        {
            string ps = m.pixelShader ?? "", n = m.name ?? "";
            bool P(string s) => ps.IndexOf(s, StringComparison.OrdinalIgnoreCase) >= 0;
            bool N(string s) => n.IndexOf(s, StringComparison.OrdinalIgnoreCase) >= 0;
            if (P("Plastic") || P("Plstc")) return "plastic";
            if (P("Tire")) return "tire";
            if (P("Wheel")) return "wheel";
            if (P("AlphaMtl") || P("DifSpecAlpha")) return "chain";
            if (P("Engine")) return "engine";
            if (P("Exhst")) return "engine";
            if (P("Frame") || P("Hbars")) return "frame";
            if (N("Plstc") || N("Seat")) return "plastic";
            if (N("Tire")) return "tire";
            if (N("Rim") || N("Hub")) return "wheel";
            if (N("Chain") || N("Alpha")) return "chain";
            if (N("_Eng") || N("Exh")) return "engine";
            return "frame";
        }

        /// The material's diffuse, SFX / combo and normal map textures (by name: the original's naming), null where it has none.
        static (string diffuse, string sfx, string normal) Roles(Mat m)
        {
            string diffuse = null, sfx = null, normal = null;
            foreach (string t in m.def.textures)
            {
                if (string.IsNullOrEmpty(t)) continue;
                bool H(string s) => t.IndexOf(s, StringComparison.OrdinalIgnoreCase) >= 0;
                if (H("Mud_Noise") || t.Equals("Numbers", StringComparison.OrdinalIgnoreCase) || H("CarReflect") || H("SpecStudio") || H("ReflectStudio") || H("Editor_") || H("Blur") || H("_Fork") && m.template != "frame") continue;
                if (H("_SFX") || H("Combo")) { sfx ??= t; continue; }
                if (H("Nrml") || H("_Norm") || t.EndsWith("_N", StringComparison.OrdinalIgnoreCase)) { normal ??= t; continue; }
                if (t.StartsWith("MX_Fork", StringComparison.OrdinalIgnoreCase)) continue;   // (the fork sticker of the frame's second UV set)
                diffuse ??= t;
            }
            return (diffuse, sfx, normal);
        }

        static string Safe(string s) => new string((s ?? "").Select(c => char.IsLetterOrDigit(c) || c == '_' || c == '-' ? c : '_').ToArray());

        /// The .rxmesh (core/plan/RXMOD.md "Mesh"; RxModBikes.ParseMesh): the hierarchy's nodes, the materials, the surfaces in model space.
        static byte[] WriteMesh(Rfx.Model hier, List<Part> parts, List<Mat> mats, AliveBikeResult res)
        {
            var ms = new MemoryStream(); var w = new BinaryWriter(ms, Encoding.UTF8);
            void Str(string s) { var b = Encoding.UTF8.GetBytes(s ?? ""); w.Write((ushort)b.Length); w.Write(b); }
            w.Write(Encoding.ASCII.GetBytes("RXMS")); w.Write(1); w.Write(0);
            w.Write(hier.nodes.Length);
            foreach (var n in hier.nodes) { Str(n.name); w.Write(n.parent); for (int k = 0; k < 16; k++) w.Write(n.world[k / 4, k % 4]); }
            w.Write(mats.Count); foreach (var m in mats) Str(m.key);
            w.Write(parts.Count);
            foreach (var p in parts)
            {
                var s = p.s; int v = s.positions.Length;
                bool uv1 = s.uv1 != null && s.uv1.Any(x => x != Vector2.zero), skin = s.skinned && s.bones != null;
                w.Write(mats.IndexOf(p.mat)); w.Write(p.node); w.Write((skin ? 1 : 0) | (uv1 ? 2 : 0)); w.Write(v); w.Write(s.triangles.Length);
                foreach (var x in s.positions) { w.Write(x.x); w.Write(x.y); w.Write(x.z); }
                foreach (var x in s.normals) { w.Write(x.x); w.Write(x.y); w.Write(x.z); }
                foreach (var x in s.uv0) { w.Write(x.x); w.Write(1f - x.y); }
                if (uv1) foreach (var x in s.uv1) { w.Write(x.x); w.Write(1f - x.y); }
                if (skin)
                {
                    for (int i = 0; i < v * 4; i++) w.Write((byte)(s.bones[i] < 0 || s.weights[i] <= 0 ? 255 : s.bones[i]));
                    for (int i = 0; i < v * 4; i++) w.Write(s.bones[i] < 0 ? 0f : s.weights[i]);
                }
                foreach (int k in s.triangles) w.Write(k);
                res.surfaces++; res.vertices += v; res.triangles += s.triangles.Length / 3;
            }
            w.Flush();
            return ms.ToArray();
        }

        // ------------------------------------------------------------------------------------- installing

        /// Where the imported bikes go: <content>/AliveBikes (BikeMods scans it next to Mods/Bikes).
        public static string Root(string content) => Path.Combine(content, Folder);

        /// Whether a bike was imported before (its folder holds a mod.json).
        public static bool Installed(string content, AliveBikeSource b) => File.Exists(Path.Combine(Root(content), b.Slug, "mod.json"));

        /// Converts and installs the given bikes into <content>/AliveBikes/<slug> (each staged, then moved in; a bike imported before by this
        /// importer is replaced). A failed bike leaves nothing behind.
        public static List<AliveBikeResult> Import(IEnumerable<AliveBikeSource> bikes, AliveSources sources, string content, Action<string> status, CancellationToken cancel)
        {
            content = Path.GetFullPath(content);
            string root = Root(content);
            Directory.CreateDirectory(root);
            Bin.Require(!ReflexFs.IsLink(root), "Would write through a symbolic link: " + root);
            var results = new List<AliveBikeResult>();
            foreach (var b in bikes)
            {
                if (cancel.IsCancellationRequested) break;
                status?.Invoke("Converting " + b.Label);
                string stage = Path.Combine(root, ".ab-" + Guid.NewGuid().ToString("N").Substring(0, 8)), dest = Path.Combine(root, b.Slug);
                AliveBikeResult res;
                try
                {
                    res = Convert(b, sources, stage, cancel);
                    if (res.ok)
                    {
                        string manifest = Path.Combine(root, b.Slug + ".import.json");
                        if (Directory.Exists(dest))
                        {
                            Bin.Require(File.Exists(manifest), b.Label + ": " + dest + " exists and was not made by this importer; remove it first");
                            Directory.Delete(dest, true);
                        }
                        Directory.Move(stage, dest);
                        res.folder = dest;
                        var files = Directory.GetFiles(dest, "*", SearchOption.AllDirectories).OrderBy(f => f, StringComparer.Ordinal)
                            .Select(f => (object)new JObj().Set("path", f.Substring(dest.Length + 1).Replace('\\', '/')).Set("sha256", LevelInstaller.Sha256(f))).ToList();
                        var m = new JObj().Set("version", 1L).Set("alive_id", (long)b.aliveId).Set("name", b.Label).Set("pack", b.pack).Set("rides_as", (long)b.ridesAs)
                            .Set("imported_utc", DateTime.UtcNow.ToString("yyyy-MM-ddTHH:mm:ssZ", System.Globalization.CultureInfo.InvariantCulture)).Set("files", files);
                        File.WriteAllText(manifest, Json.Dumps(m, 1) + "\n", new UTF8Encoding(false));
                    }
                }
                catch (OperationCanceledException) { res = new AliveBikeResult { bike = b, error = "Cancelled" }; results.Add(res); break; }
                catch (Exception e) when (e is IOException || e is UnauthorizedAccessException || e is AliveImportException) { res = new AliveBikeResult { bike = b, error = e.Message }; }
                finally { try { if (Directory.Exists(stage)) Directory.Delete(stage, true); } catch (Exception) { } }
                results.Add(res);
            }
            return results;
        }
    }
}

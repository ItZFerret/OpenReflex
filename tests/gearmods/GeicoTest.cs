// TEST ONLY (tests/gearmods/geico_test.sh): the built-in GEICO kit pack (Unity/Reflex/Assets/Scripts/Content/KitPacks.Geico.cs) on .NET.
//   GeicoTest <Database dir> [extracted]
// Reads the game's merged tables (MXTables_Default + the DLC tables, as ReflexEvents.Tables merges them), builds the GEICO pack from the
// install's MXDLC01_graphickit / ATVDLC01_graphickit, and checks: 56 kits (40 MX, 16 ATV; the two KTM450 ATV kits skipped), each fitting
// exactly the stock VehicleModelIds of its chassis, its own namespaced material package with plastics / seat or hand guard materials that
// parse, its HR twin, the stock chassis model package through the overlay tables, one "GEICO" line with the GEICO brand, colour names
// without numbering, unique renumbered ids and tokens, and a hash that does not depend on the package files. Exit 0: GEICO PASS.
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using Reflex.Port.Content;

static class GeicoTest
{
    static int fails;
    static void Check(bool ok, string what) { Console.WriteLine((ok ? "  ok   " : "  FAIL ") + what); if (!ok) fails++; }

    static int Main(string[] args)
    {
        string dbDir = args[0]; bool extracted = args.Length > 1 && args[1] == "extracted";
        var stock = new Rfx.Install(dbDir);
        var tables = Rfx.Tables(stock.Read("MXTables_Default", "MXTables_Default", "MXTables_Default", "tbldata"));
        foreach (var n in new[] { "MXTables_DLC003", "MXTables_DLC004", "MXTables_DLC005" })
            foreach (var p in stock.Db(n).packages.Values) foreach (var a in p.OfType("tbldata"))
                foreach (var kv in Rfx.Tables(stock.Read(a))) { if (!tables.TryGetValue(kv.Key, out var l)) tables[kv.Key] = l = new List<Dictionary<string, object>>(); l.AddRange(kv.Value); }

        var problems = new List<KitPacks.Problem>();
        var pack = KitPacks.Geico(dbDir, extracted, tables, problems);
        foreach (var pr in problems) Console.WriteLine("  problem " + pr);
        Check(pack != null, "the GEICO pack reads from " + dbDir + (extracted ? " (extracted)" : ""));
        if (pack == null) { Console.WriteLine("GEICO FAIL"); return 1; }
        Console.WriteLine(KitPacks.Describe(pack));
        var c = KitPacks.Build(new List<KitPack> { pack }, problems);
        KitPacks.Active = c;
        Rfx.Install.ModRouter = db => KitPacks.InstallOf(BikeMods.Split(db).id);
        var t = new ReflexGameTables(KitPacks.Source(new DictSource(tables), c));

        var items = c.byId.Values.OrderBy(i => i.id).ToList();
        Check(items.Count == 56, items.Count + " kits (40 MX + 16 ATV)");
        Check(pack.notes.Count == 2 && pack.notes.All(n => n.Contains("KTM450")), "skipped: " + string.Join("; ", pack.notes));
        Check(items.All(i => i.bike && i.type == 11 && i.models.Length == 1), "every kit is a type 11 graphics kit for one stock vehicle model");
        Check(items.Select(i => i.models[0]).Distinct().Count() == 28 && items.GroupBy(i => i.models[0]).All(g => g.Count() == 2), "28 vehicle models, two colours each");
        var brand = tables["Brand"].FirstOrDefault(r => Rfx.Str(r, "BrandName") == "Geico");
        Check(brand != null && items.All(i => i.brandId == Rfx.Int(brand, "BrandId")), "brand GEICO (" + Rfx.Int(brand, "BrandId") + ")");
        Check(c.lines.Count == 1 && c.lines.Values.First().title == "GEICO" && items.All(i => c.lines.ContainsKey(i.parent) && i.lineTitle == "GEICO"), "one line 'GEICO'");
        var colours = new HashSet<string> { "Black", "Blue", "Red", "Orange", "Green", "Yellow", "White", "Gray" };
        Check(items.All(i => colours.Contains(i.title)), "colour names: " + string.Join(", ", items.Select(i => i.title).Distinct()));
        Check(c.byId.Keys.All(KitPacks.IsKitId) && c.byToken.Count == items.Count, "renumbered ids >= " + KitPacks.IdBase + ", unique tokens");

        int hr = 0;
        foreach (var it in items)
        {
            var mat = it.material.Value;
            var pkg = stock.Pkg(mat.db, mat.pkg);
            var mats = pkg?.OfType("material").Select(a => a.name).ToList() ?? new List<string>();
            int r = t.Find("VehiclePart", "VehiclePartId", it.id);
            var viaTables = t.Package(t.Int("VehiclePart", "MaterialPackageDatabaseId", r));
            var model = t.Package(t.Int("VehiclePart", "ModelPackageDatabaseId", r));
            bool atv = mat.pkg.StartsWith("ATV", StringComparison.Ordinal);
            string body = mat.pkg.Split('_')[2];
            string want = atv ? "ATVs" : "MXs";
            bool parsed = pkg != null && pkg.OfType("material").All(a => Rfx.ParseMaterial(a.name, stock.Read(a)).pixelShader != null);
            if (stock.Pkg(it.pack.Namespace + "HR_" + BikeMods.BaseDb(mat.db), "HR_" + mat.pkg) != null && t.Int("VehiclePart", "HRAvailable", r) == 1) hr++;
            Check(pkg != null && pkg.database.mod && mat.db.StartsWith(pack.Namespace) && parsed
                  && mats.Any(m => m.Contains("_Plstc_")) && mats.Any(m => m.Contains("_HGuard_"))
                  && viaTables != null && viaTables.Value == mat && model != null && model.Value.db == want && stock.Pkg(model.Value.db, model.Value.pkg) != null
                  && model.Value.pkg.Replace("_", "") == body
                  && t.Find("VehicleModel", "VehicleModelId", it.models[0]) >= 0,
                  $"{mat.pkg}: '{it.lineTitle} {it.title}' model {model?.db}/{model?.pkg} vehicle {it.models[0]} [{string.Join(", ", mats)}]");
        }
        Check(hr == items.Count, "HR twins: " + hr + " of " + items.Count);

        // the identity: from the databases, not the package files (an imported install has none)
        var again = KitPacks.Geico(dbDir, !extracted, tables, null);
        Check(again == null || again.hash == pack.hash, "hash " + pack.hash + " the same read either way (" + (again?.hash ?? "-") + ")");

        Console.WriteLine(fails == 0 ? "GEICO PASS" : "GEICO FAIL (" + fails + ")");
        return fails == 0 ? 0 : 1;
    }

    sealed class DictSource : ReflexGameTables.ISource
    {
        readonly Dictionary<string, List<Dictionary<string, object>>> t;
        public DictSource(Dictionary<string, List<Dictionary<string, object>>> t) { this.t = t; }
        public int Rows(string table) => t.TryGetValue(table, out var r) ? r.Count : -1;
        public int[] Ints(string table, string column) => t.TryGetValue(table, out var r) && r.Any(x => x.ContainsKey(column)) ? r.Select(x => Rfx.Int(x, column)).ToArray() : null;
        public string[] Strings(string table, string column) => t.TryGetValue(table, out var r) && r.Any(x => x.ContainsKey(column)) ? r.Select(x => Rfx.Str(x, column)).ToArray() : null;
    }
}

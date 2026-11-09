// TEST ONLY (tests/gearmods/table_test.sh): the gear / kit pack readers (Unity/Reflex/Assets/Scripts/Content/KitPacks.cs, RfxData.cs) on .NET.
//   KitPacksTest <work dir> <stock Database dir> <pack source>...
// A pack source is a folder or an archive (copied into <work>/Mods/Gear as it is: archives are installed by TrackArchives with KitPacks'
// profile, an archive inside an archive too). Checks: every pack reads, its pieces resolve to its own namespaced packages (and HR twins),
// the renumbered ids are unique, the overlay tables give the spec builders the same packages, stock MXTables_DLC003-005 read, a pack in
// a Database folder is found there and the game's own DLC tables are not, and database BXMLs whose numbers are written as an empty value /
// array / blob / text (the import's "Specified cast is not valid") read. Exit 0: GEARMODS PASS.
using System;
using System.Collections.Generic;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Text;
using Reflex.Port.Content;

static class KitPacksTest
{
    static int fails;
    static void Check(bool ok, string what) { Console.WriteLine((ok ? "  ok   " : "  FAIL ") + what); if (!ok) fails++; }

    static void CopyDir(string src, string dst)
    {
        Directory.CreateDirectory(dst);
        foreach (var f in Directory.GetFiles(src)) File.Copy(f, Path.Combine(dst, Path.GetFileName(f)), true);
        foreach (var d in Directory.GetDirectories(src)) CopyDir(d, Path.Combine(dst, Path.GetFileName(d)));
    }

    static int Main(string[] args)
    {
        string work = args[0], stockDb = args[1];
        if (Directory.Exists(work)) Directory.Delete(work, true);
        string gear = Path.Combine(work, "Mods", "Gear"); Directory.CreateDirectory(gear);
        TrackArchives.Log = s => Console.WriteLine("  log  " + s);
        foreach (var src in args.Skip(2))
        {
            if (Directory.Exists(src)) CopyDir(src, Path.Combine(gear, Path.GetFileName(src.TrimEnd('/'))));
            else File.Copy(src, Path.Combine(gear, Path.GetFileName(src)));
        }

        // ---- 1. the scan, with the archives installed (again until nothing new: an archive inside an archive)
        Console.WriteLine("== scan " + gear);
        List<KitPack> packs = null; var problems = new List<KitPacks.Problem>();
        for (int round = 0; round < 4; round++)
        {
            int installs = 0; problems = new List<KitPacks.Problem>();
            packs = KitPacks.Scan(gear, problems, archive =>
            {
                if (TrackArchives.InstalledFolder(archive) != null) return null;
                var job = new TrackArchives.Job { archive = archive, kind = TrackArchives.Sniff(archive), profile = KitPacks.ArchiveProfile };
                TrackArchives.Install(job, gear); installs++;
                Console.WriteLine("  archive " + Path.GetFileName(archive) + " (" + TrackArchives.Name(job.kind) + "): " + (job.error ?? "installed into " + job.folder));
                return job.error;
            });
            if (installs == 0) break;
        }
        foreach (var pr in problems) Console.WriteLine("  problem " + pr);
        var c = KitPacks.Build(packs, problems);
        KitPacks.Active = c;
        Rfx.Install.ModRouter = db => KitPacks.InstallOf(BikeMods.Split(db).id);
        Console.WriteLine("== " + c.packs.Count + " packs, " + c.byId.Count + " pieces");
        foreach (var p in c.packs)
        {
            Console.WriteLine(KitPacks.Describe(p));
            foreach (var it in p.items) Console.WriteLine("    " + it);
        }
        Check(problems.Count == 0, "no problems (" + problems.Count + ")");

        // ---- 2. every piece: its material package in its own namespace (and the HR twin), its materials parse
        var stock = new Rfx.Install(stockDb);
        var stockTables = Rfx.Tables(stock.Read("MXTables_Default", "MXTables_Default", "MXTables_Default", "tbldata"));
        foreach (var n in new[] { "MXTables_DLC003", "MXTables_DLC004", "MXTables_DLC005" })   // the game's own DLC rows merged, as ReflexEvents.Tables does
            foreach (var p in stock.Db(n).packages.Values) foreach (var a in p.OfType("tbldata"))
                foreach (var kv in Rfx.Tables(stock.Read(a))) { if (!stockTables.TryGetValue(kv.Key, out var l)) stockTables[kv.Key] = l = new List<Dictionary<string, object>>(); l.AddRange(kv.Value); }
        var source = new DictSource(stockTables);
        var t = new ReflexGameTables(KitPacks.Source(source, c));
        int hr = 0, numbers = 0, kits = 0;
        foreach (var it in c.byId.Values.OrderBy(i => i.id))
        {
            string label = it.pack.id + " " + (it.bike ? "kit" : "gear") + " #" + it.rowId + " '" + it.title + "'";
            var mat = it.material.Value;
            var pkg = stock.Pkg(mat.db, mat.pkg);   // through the router, as VehicleCatalog's install reads it
            Check(pkg != null && pkg.database.mod && mat.db.StartsWith(it.pack.Namespace), label + ": material " + mat.db + "/" + mat.pkg + " (" + (pkg?.assets.Count ?? 0) + " assets)");
            var hrPkg = stock.Pkg(it.pack.Namespace + "HR_" + BikeMods.BaseDb(mat.db), "HR_" + mat.pkg);
            if (hrPkg != null) hr++;
            var mats = pkg?.OfType("material").ToList() ?? new List<Rfx.Asset>();
            Check(mats.Count > 0, label + ": " + mats.Count + " materials: " + string.Join(", ", mats.Select(a => a.name)));
            foreach (var a in mats)
            {
                var m = Rfx.ParseMaterial(a.name, stock.Read(a));
                if (it.bike && m.textures.Any(x => x.Equals("Numbers", StringComparison.OrdinalIgnoreCase))) numbers++;
                Console.WriteLine("      " + a.name + ": " + m.pixelShader + " [" + string.Join(", ", m.textures) + "]");
            }
            if (it.bike) kits++;
            // the overlay tables: the spec builders' Package() of the renumbered row is the same package; the model is a stock one
            int r = t.Find(it.bike ? "VehiclePart" : "RiderGear", it.bike ? "VehiclePartId" : "RiderGearId", it.id);
            var viaTables = t.Package(t.Int(it.bike ? "VehiclePart" : "RiderGear", "MaterialPackageDatabaseId", r));
            var model = t.Package(t.Int(it.bike ? "VehiclePart" : "RiderGear", "ModelPackageDatabaseId", r));
            Check(r >= 0 && viaTables != null && viaTables.Value == mat, label + ": the overlay row resolves the material");
            Check(model != null && stock.Pkg(model.Value.db, model.Value.pkg) != null, label + ": model package " + model?.db + "/" + model?.pkg + " (stock)");
            if (it.bike) foreach (int vm in it.models) Check(t.Find("VehicleModel", "VehicleModelId", vm) >= 0, label + ": fits VehicleModelId " + vm + " (a stock model)");
            Check(it.parent < 0 || c.lines.ContainsKey(it.parent) || stockTables["StoreProducts"].Any(x => Rfx.Int(x, "StoreProductId") == it.parent), label + ": line " + it.parent + " ('" + it.lineTitle + "')");
        }
        Console.WriteLine("  HR twins " + hr + " of " + c.byId.Count + "; kit materials sampling Numbers: " + numbers + " (" + kits + " kits)");
        Check(c.byId.Keys.Distinct().Count() == c.byId.Count && c.byId.Keys.All(KitPacks.IsKitId), "renumbered ids unique and >= " + KitPacks.IdBase);
        Check(c.byId.Values.Select(i => i.Token).Distinct().Count() == c.byId.Count, "tokens unique");

        // ---- 3. the game's own DLC tables read (no pack among them)
        foreach (var n in new[] { "MXTables_DLC003", "MXTables_DLC004", "MXTables_DLC005" })
        {
            var db = stock.Db(n); int rows = 0;
            foreach (var p in db.packages.Values) foreach (var a in p.OfType("tbldata")) rows += Rfx.Tables(stock.Read(a)).Values.Sum(x => x.Count);
            Check(db != null, n + ": " + rows + " rows");
        }

        // ---- 4. a pack put into a Database folder the original way: found there; the game's own DLC tables are not packs
        string dbDir = Path.Combine(work, "Install", "Database"); Directory.CreateDirectory(dbDir);
        foreach (var f in Directory.GetFiles(stockDb, "MXTables_DLC00*")) File.Copy(f, Path.Combine(dbDir, Path.GetFileName(f)));
        var first = c.packs.First(p => !p.fromInstall);
        foreach (var f in Directory.GetFiles(first.folder).Where(f => f.Contains(".dx9."))) File.Copy(f, Path.Combine(dbDir, Path.GetFileName(f)));
        var ip = new List<KitPacks.Problem>();
        var inst = KitPacks.ScanInstall(dbDir, false, ip);
        Check(inst.Count == 1 && inst[0].hash == first.hash && inst[0].items.Count == first.items.Count, "Database folder: " + inst.Count + " pack(s) " + string.Join(", ", inst.Select(KitPacks.Describe)) + " (" + ip.Count + " problems)");

        // ---- 5. database BXMLs with their numbers written otherwise (the cast error): every variant reads, the old Convert calls throw
        Console.WriteLine("== number encodings");
        string probe = Path.Combine(work, "Encodings"); Directory.CreateDirectory(probe);
        string srcDb = Path.Combine(first.folder, first.tables + ".dx9.database"), srcPkg = Path.Combine(first.folder, first.tables + ".dx9.package");
        foreach (var variant in new[] { "empty-block-size", "float-offset", "blob-size", "empty-enabled", "uint-enabled" })
        {
            string dir = Path.Combine(probe, variant); Directory.CreateDirectory(dir);
            File.WriteAllBytes(Path.Combine(dir, first.tables + ".dx9.database"), BxmlEdit.Rewrite(File.ReadAllBytes(srcDb), variant, out object oldValue));
            File.Copy(srcPkg, Path.Combine(dir, first.tables + ".dx9.package"));
            string old;
            try { Convert.ToInt64(oldValue); Convert.ToBoolean(oldValue); old = "the old Convert reads it"; }
            catch (Exception e) { old = "the old Convert: " + e.GetType().Name; }
            try
            {
                var i2 = new Rfx.Install(dir); var d = i2.Db(first.tables);
                var tab = Rfx.Tables(i2.Read(d.packages.Values.First().OfType("tbldata").First()));
                Check(tab.Count > 0, variant + " (" + oldValue?.GetType().Name + "): reads, " + tab.Count + " tables; " + old);
            }
            catch (Exception e) { Check(false, variant + ": " + e.GetType().Name + " " + e.Message); }
        }

        Console.WriteLine(fails == 0 ? "GEARMODS PASS" : "GEARMODS FAIL (" + fails + ")");
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

/// Rewrites one attribute of a database BXML (0x103ea: zlib, adler32) the way other writers store numbers.
static class BxmlEdit
{
    static int LE(byte[] b, int p) => BitConverter.ToInt32(b, p);
    public static byte[] Rewrite(byte[] src, string variant, out object oldValue)
    {
        int count = LE(src, 8), pool = LE(src, 12), poolSize = LE(src, 16), ac = LE(src, 20), nc = LE(src, 24);
        int packed = LE(src, 32);
        byte[] b;
        using (var ms = new MemoryStream(src, 38, packed - 6)) using (var z = new DeflateStream(ms, CompressionMode.Decompress)) using (var o = new MemoryStream()) { z.CopyTo(o); b = o.ToArray(); }
        var names = Encoding.Latin1.GetString(b, 0, pool).Split('\0');
        // the blob grows by 8 bytes at its end for the new values
        int blob = pool + poolSize;
        var nb = new byte[b.Length + 8];
        Buffer.BlockCopy(b, 0, nb, 0, blob); Buffer.BlockCopy(b, blob, nb, blob + 8, b.Length - blob);
        int newAt = poolSize; int attrs = blob + 8;
        string want = variant.EndsWith("size") ? "size" : variant.EndsWith("offset") ? "offset" : "enabled";
        oldValue = null;
        for (int i = ac - 1; i >= 0; i--)
        {
            int p = attrs + i * 12;
            if (names[LE(nb, p)] != want) continue;
            if (variant == "empty-block-size" || variant == "empty-enabled") { BitConverter.GetBytes((ushort)0).CopyTo(nb, p + 8); oldValue = Array.Empty<float>(); }
            else if (variant == "float-offset") { BitConverter.GetBytes(0f).CopyTo(nb, pool + newAt); BitConverter.GetBytes(0f).CopyTo(nb, pool + newAt + 4); BitConverter.GetBytes(newAt).CopyTo(nb, p + 4); BitConverter.GetBytes((ushort)2).CopyTo(nb, p + 8); BitConverter.GetBytes((ushort)5).CopyTo(nb, p + 10); oldValue = new float[2]; }
            else if (variant == "blob-size")
            {   // the Heap size as a 4-byte blob (type 2: pooled = its byte count)
                int v = LE(nb, pool + LE(nb, p + 4)); BitConverter.GetBytes(v).CopyTo(nb, pool + newAt);
                BitConverter.GetBytes(newAt).CopyTo(nb, p + 4); BitConverter.GetBytes((ushort)4).CopyTo(nb, p + 8); BitConverter.GetBytes((ushort)2).CopyTo(nb, p + 10); oldValue = new ArraySegment<byte>(new byte[4]);
            }
            else if (variant == "uint-enabled") { BitConverter.GetBytes(1).CopyTo(nb, pool + newAt); BitConverter.GetBytes(newAt).CopyTo(nb, p + 4); BitConverter.GetBytes((ushort)1).CopyTo(nb, p + 8); BitConverter.GetBytes((ushort)4).CopyTo(nb, p + 10); oldValue = 1u; }
            if (variant == "empty-block-size") { /* the last 'size' is the Block's (written after Heap) */ }
            break;
        }
        var hdr = (byte[])src.Clone();
        BitConverter.GetBytes(poolSize + 8).CopyTo(hdr, 16);
        byte[] z2;
        using (var o = new MemoryStream()) { using (var z = new DeflateStream(o, CompressionLevel.Optimal, true)) z.Write(nb, 0, nb.Length); z2 = o.ToArray(); }
        uint a = 1, cc = 0; foreach (byte v in nb) { a = (a + v) % 65521; cc = (cc + a) % 65521; }
        var outp = new MemoryStream();
        outp.Write(hdr, 0, 32);
        outp.Write(BitConverter.GetBytes(z2.Length + 6), 0, 4);
        outp.Write(new byte[] { 0x78, 0xDA }, 0, 2);
        outp.Write(z2, 0, z2.Length);
        uint ad = cc << 16 | a; outp.Write(new[] { (byte)(ad >> 24), (byte)(ad >> 16), (byte)(ad >> 8), (byte)ad }, 0, 4);
        return outp.ToArray();
    }
}

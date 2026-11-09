// RESTORATION (not original game code): the repair of the cut supercross level SX_FresnoB from the player's own install, run by the
// importer (ReflexAssetImport). The C# twin of _re/formats/fresnob_repair.py, byte for byte (the report's SHA-256 of the three outputs).
//
// The shipped SX_FresnoB.dx9.package ends after 462 of the 675 assets its .dx9.database lists, and stores those 462 with sizes a few
// bytes off the index (the package was rebuilt after the index was written), so every offset after the first asset is wrong. The repair:
//   1. walks the package resource by resource (an XMem resource is a u32 limit, its chunks, a u32 last-chunk length: limit + 4 bytes);
//      the walk must cover the file and decode, and every resource's signature must match the indexed type at its position;
//   2. takes each missing asset from another level of the install with the same name and type (it must decode; a model only from a
//      level whose same-named surface is byte-identical to SX_FresnoB's own; the index's stored size first, then the ordinal database
//      order) and appends its stored resource, unchanged, to a repaired copy of the package (the game loads only a level's Shared and
//      <level>Base packages, 00756060);
//   3. substitutes the lost friction zones SX_FR_B_Zones with SX_Fresno's SX_FR_A_Zones (the same arena's A layout: uniform maps, the
//      same stored size);
//   4. leaves the rest out (the track definition, cinematic anchors and cameras, a few crowd materials and animations, the stadium
//      beams, the ad stand-ins and the trophy girl): a lost model's or track definition's handle leaves <Handles>, and the scene loses
//      the entities that place one.
// Result: the repaired .dx9.database, .dx9.package and .dx9.scene. Nothing of the game ships with the client.
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using System.Text;
using System.Threading;
using Reflex.Port.Content.AliveImport;

namespace Reflex.Port.Content
{
    internal static class FresnoBRepair
    {
        public const string Level = "SX_FresnoB";
        static readonly HashSet<string> PruneTypes = new HashSet<string>(StringComparer.Ordinal) { "model", "tdf" };
        static readonly Dictionary<(string, string), (string db, string name, string type)> Substitutes =
            new Dictionary<(string, string), (string, string, string)> { { ("sx_fr_b_zones", "packdata"), ("SX_Fresno", "SX_FR_A_Zones", "packdata") } };
        static readonly HashSet<string> BxmlTypes = new HashSet<string>(StringComparer.Ordinal) { "material", "bxml", "packdata", "tbldata" };
        static readonly Dictionary<string, uint> Magic = new Dictionary<string, uint>(StringComparer.Ordinal)
            { { "texture", 0x77ddcc00 }, { "surface", 0x99bbcc00 }, { "model", 0x88aabb00 }, { "anim", 0xfffffff5 }, { "shader", 2 } };

        public sealed class Result
        {
            public byte[] database, package, scene;
            public int indexed, walked, missing, filled, substituted;
            public readonly List<string> lost = new List<string>(), dropped = new List<string>();
            public string Summary => "SX_FresnoB was repaired from your install: " + walked + " of its " + indexed + " assets are in its package, " + filled +
                                     " missing ones were taken from other levels (" + substituted + " substituted), " + lost.Count + " are lost (" +
                                     string.Join(", ", lost.Take(6)) + (lost.Count > 6 ? ", ..." : "") + "); " + dropped.Count + " scene entities that need a lost asset were left out. RESTORED: Free Ride only (its track definition is lost).";
            public static string Sha(byte[] b) { using (var s = SHA256.Create()) return string.Concat(s.ComputeHash(b).Select(x => x.ToString("x2"))); }
        }

        sealed class A
        {
            public string name, type, file, database, package; public long offset = -1; public long size, block; public bool lzx;
        }

        sealed class Install
        {
            readonly string dir; readonly Dictionary<string, (Elem root, List<(Elem pkg, List<A> assets)> packages)> cache = new Dictionary<string, (Elem, List<(Elem, List<A>)>)>(StringComparer.Ordinal);
            readonly Dictionary<string, FileStream> files = new Dictionary<string, FileStream>(StringComparer.Ordinal);
            public Install(string dir) { this.dir = dir; }
            public string Path(string name) => System.IO.Path.Combine(dir, name);
            public List<string> Names() => Directory.EnumerateFiles(dir, "*.dx9.database").Select(p => System.IO.Path.GetFileName(p)).Select(n => n.Substring(0, n.Length - ".dx9.database".Length)).OrderBy(n => n, StringComparer.Ordinal).ToList();
            public (Elem root, List<(Elem pkg, List<A> assets)> packages) Index(string name)
            {
                if (cache.TryGetValue(name, out var c)) return c;
                var root = Bxml.Parse(File.ReadAllBytes(Path(name + ".dx9.database"))).ToElement();
                var packages = new List<(Elem, List<A>)>();
                foreach (var p in root.Iter("Package"))
                {
                    long offset = XMem.MetadataUint(p.Get("offset"));
                    var assets = new List<A>();
                    foreach (var a in p.FindAll("Asset"))
                    {
                        var heap = a.Find("Heap"); var block = a.Find("Block"); var comp = a.Find("Compress");
                        assets.Add(new A
                        {
                            name = a.Get("name"), type = a.Get("type"), file = p.Get("file"), database = name, package = p.Get("name"),
                            offset = heap != null ? offset + XMem.MetadataUint(heap.Get("offset")) : -1,
                            size = heap != null ? XMem.MetadataUint(heap.Get("size")) : 0,
                            block = block != null ? XMem.MetadataUint(block.Get("size")) : 0,
                            lzx = comp != null && comp.Get("enabled") == "_bool:true" && comp.Get("codec") == "LZX",
                        });
                    }
                    packages.Add((p, assets));
                }
                return cache[name] = (root, packages);
            }
            public byte[] Raw(string file, long offset, long size)
            {
                if (!files.TryGetValue(file, out var fs)) files[file] = fs = new FileStream(Path(file + ".dx9.package"), FileMode.Open, FileAccess.Read, FileShare.Read, 1 << 16);
                Bin.Require(size >= 0 && offset >= 0 && offset + size <= fs.Length, "Short package read");
                var b = new byte[size]; fs.Position = offset;
                int got = 0; while (got < b.Length) { int n = fs.Read(b, got, b.Length - got); Bin.Require(n > 0, "Short package read"); got += n; }
                return b;
            }
            public byte[] Decode(A a, CancellationToken cancel) { var raw = Raw(a.file, a.offset, a.size); return a.lzx ? XMem.Decode(raw, cancel) : raw; }
            public void Close() { foreach (var f in files.Values) f.Dispose(); files.Clear(); }
        }

        static bool SignatureOk(string type, byte[] data)
        {
            if (BxmlTypes.Contains(type)) return Bin.StartsWith(data, "BXML");
            if (Magic.TryGetValue(type, out uint m)) return data.Length >= 4 && Bin.U32LE(data, 0) == m;
            return true;
        }

        /// True when the install holds SX_FresnoB with a package that does not match its index (the shipped state).
        public static bool Needed(string databaseDir)
        {
            string db = Path.Combine(databaseDir, Level + ".dx9.database"), pk = Path.Combine(databaseDir, Level + ".dx9.package");
            if (!File.Exists(db) || !File.Exists(pk)) return false;
            var root = Bxml.Parse(File.ReadAllBytes(db)).ToElement();
            long end = 0;
            foreach (var p in root.Iter("Package"))
                foreach (var a in p.FindAll("Asset"))
                {
                    var heap = a.Find("Heap"); if (heap == null) continue;
                    end = Math.Max(end, XMem.MetadataUint(p.Get("offset")) + XMem.MetadataUint(heap.Get("offset")) + XMem.MetadataUint(heap.Get("size")));
                }
            return end > new FileInfo(pk).Length;
        }

        public static Result Repair(string databaseDir, CancellationToken cancel = default)
        {
            var install = new Install(databaseDir);
            try { return Run(install, cancel); }
            finally { install.Close(); }
        }

        static Result Run(Install install, CancellationToken cancel)
        {
            var (root, packages) = install.Index(Level);
            Bin.Require(root.Iter("Package").All(p => p.Get("file") == Level), "Unexpected package files");
            var indexed = packages.SelectMany(p => p.assets).ToList();
            // the walk
            byte[] shipped = File.ReadAllBytes(install.Path(Level + ".dx9.package"));
            var walked = new List<(long offset, long size, byte[] data)>();
            for (long pos = 0; pos < shipped.Length;)
            {
                cancel.ThrowIfCancellationRequested();
                Bin.Require(pos + 4 <= shipped.Length, "Truncated resource header");
                long size = (long)Bin.U32LE(shipped, (int)pos) + 4;
                Bin.Require(16 <= size && size <= shipped.Length - pos, "Resource exceeds the package");
                walked.Add((pos, size, XMem.Decode(Bin.Slice(shipped, (int)pos, (int)size), cancel)));
                pos += size;
            }
            int k = walked.Count;
            Bin.Require(k > 0 && k <= indexed.Count, "The package holds more resources than the index");
            for (int i = 0; i < k; i++)
            {
                Bin.Require(indexed[i].lzx && indexed[i].block == 0, "Unexpected indexed resource kind");
                Bin.Require(SignatureOk(indexed[i].type, walked[i].data), "Resource at " + walked[i].offset + " is not the indexed " + indexed[i].type + " " + indexed[i].name);
            }
            Bin.Require(!(k == indexed.Count && Enumerable.Range(0, k).All(i => walked[i].offset == indexed[i].offset && walked[i].size == indexed[i].size)), "SX_FresnoB's package already matches its index");
            var present = new Dictionary<(string, string), byte[]>();
            for (int i = 0; i < k; i++) present[(indexed[i].name.ToLowerInvariant(), indexed[i].type)] = walked[i].data;

            // candidates
            var byKey = new Dictionary<(string, string), List<A>>();
            foreach (string n in install.Names())
            {
                if (n.ToLowerInvariant() == Level.ToLowerInvariant()) continue;
                List<(Elem, List<A>)> pk;
                try { pk = install.Index(n).packages; } catch (Exception e) when (!(e is OperationCanceledException)) { continue; }
                foreach (var (_, assets) in pk)
                    foreach (var a in assets)
                        if (a.offset >= 0 && a.block == 0 && a.size > 0)
                        {
                            var key = (a.name.ToLowerInvariant(), a.type);
                            if (!byKey.TryGetValue(key, out var l)) byKey[key] = l = new List<A>();
                            l.Add(a);
                        }
            }
            var surfaceCache = new Dictionary<(string, string), byte[]>();
            bool Usable(A want, A c)
            {
                try { install.Decode(c, cancel); } catch (Exception e) when (!(e is OperationCanceledException)) { return false; }
                string lname = want.name.ToLowerInvariant();
                if (want.type == "model" && present.ContainsKey((lname, "surface")))
                {
                    var key = (c.database, lname);
                    if (!surfaceCache.TryGetValue(key, out var s))
                    {
                        var src = byKey.TryGetValue((lname, "surface"), out var ls) ? ls.FirstOrDefault(x => x.database == c.database) : null;
                        try { s = src != null ? install.Decode(src, cancel) : null; } catch (Exception e) when (!(e is OperationCanceledException)) { s = null; }
                        surfaceCache[key] = s;
                    }
                    return s != null && s.SequenceEqual(present[(lname, "surface")]);
                }
                return true;
            }
            var missing = indexed.Skip(k).ToList();
            var choice = new Dictionary<int, A>(); var lost = new List<A>(); int substituted = 0;
            for (int i = 0; i < missing.Count; i++)
            {
                cancel.ThrowIfCancellationRequested();
                var want = missing[i]; var key = (want.name.ToLowerInvariant(), want.type);
                if (Substitutes.TryGetValue(key, out var sub))
                {
                    var c = install.Index(sub.db).packages.SelectMany(p => p.assets).First(a => a.name == sub.name && a.type == sub.type);
                    Bin.Require(install.Decode(c, cancel) != null, "Substitute does not decode");
                    choice[i] = c; substituted++;
                    continue;
                }
                var cands = (byKey.TryGetValue(key, out var l) ? l : new List<A>()).Where(c => Usable(want, c)).ToList();
                var exact = cands.Where(c => c.size == want.size).ToList();
                var pool = exact.Count > 0 ? exact : cands;
                if (pool.Count > 0) choice[i] = pool.OrderBy(c => c.size != want.size).ThenBy(c => c.database, StringComparer.Ordinal).First();
                else lost.Add(want);
            }

            // the repaired package
            var package = new MemoryStream(); package.Write(shipped, 0, shipped.Length);
            var appended = new Dictionary<int, long>();
            foreach (int i in choice.Keys.OrderBy(x => x))
            {
                appended[i] = package.Length;
                var raw = install.Raw(choice[i].file, choice[i].offset, choice[i].size); package.Write(raw, 0, raw.Length);
            }
            long packageLength = package.Length;

            // the repaired database
            var lostKeys = new HashSet<(string, string)>(lost.Select(a => (a.type, a.name.ToLowerInvariant())));
            var newRoot = new Elem("Database");
            var handles = new Elem("Handles"); newRoot.children.Add(handles);
            foreach (var h in root.Find("Handles").children)
                if (!lostKeys.Contains((h.Get("type"), (h.Get("name") ?? "").ToLowerInvariant())) || !PruneTypes.Contains(h.Get("type"))) handles.children.Add(h);
            var pkEl = new Elem("Packages"); newRoot.children.Add(pkEl);
            int g = 0;
            foreach (var (attrs, assets) in packages)
            {
                var idx = Enumerable.Range(g, assets.Count).ToList(); g += assets.Count;
                var offs = idx.Where(j => j < k || choice.ContainsKey(j - k)).Select(j => j < k ? walked[j].offset : appended[j - k]).ToList();
                long first = offs.Count > 0 ? offs[0] : packageLength;
                var p = new Elem("Package").Set("name", EVal.S(attrs.Get("name"))).Set("file", EVal.S(attrs.Get("file")))
                    .Set("offset", EVal.Uint(first)).Set("blockOffset", EVal.Uint(0)).Set("heapOffset", EVal.Uint(first));
                pkEl.children.Add(p);
                for (int n = 0; n < idx.Count; n++)
                {
                    int j = idx[n];
                    if (j < k) AddAsset(p, assets[n].type, assets[n].name, walked[j].offset - first, walked[j].size, true);
                    else if (choice.TryGetValue(j - k, out var c)) AddAsset(p, assets[n].type, assets[n].name, appended[j - k] - first, c.size, c.lzx);
                }
            }
            Bin.Require(g == indexed.Count, "Index walk");

            var result = new Result { indexed = indexed.Count, walked = k, missing = missing.Count, filled = choice.Count - substituted, substituted = substituted };
            result.lost.AddRange(lost.Select(a => a.type + " " + a.name));
            result.scene = PruneScene(File.ReadAllBytes(install.Path(Level + ".dx9.scene")), lostKeys, result.dropped);
            result.database = Bxml.EncodeElement(newRoot);
            result.package = package.ToArray();
            return result;
        }

        static void AddAsset(Elem package, string type, string name, long offset, long size, bool lzx)
        {
            var a = new Elem("Asset").Set("type", EVal.S(type)).Set("name", EVal.S(name));
            a.children.Add(new Elem("Heap").Set("offset", EVal.Uint(offset)).Set("size", EVal.Uint(size)));
            a.children.Add(new Elem("Block").Set("offset", EVal.Uint(0)).Set("size", EVal.Uint(0)));
            if (lzx) a.children.Add(new Elem("Compress").Set("enabled", EVal.Typed(11, 1)).Set("codec", EVal.S("LZX")));
            package.children.Add(a);
        }

        /// fresnob_repair.prune_scene: every section keeps its name and order; the entities placing a lost model or track definition
        /// leave; a chunk without a dropped entity keeps its bytes.
        static byte[] PruneScene(byte[] source, HashSet<(string, string)> lostKeys, List<string> dropped)
        {
            uint magic = Bin.U32LE(source, 0), count = Bin.U32LE(source, 4);
            Bin.Require(magic == 0xc0fffeee && count >= 1, "Scene container");
            var names = new List<string>(); var offsets = new List<int>();
            for (int i = 0; i < count; i++)
            {
                int p = 8 + i * 264; int z = Array.IndexOf(source, (byte)0, p, 256); if (z < 0) z = p + 256;
                names.Add(Encoding.UTF8.GetString(source, p, z - p)); offsets.Add((int)Bin.U32LE(source, p + 260));
            }
            var sections = Scenes.ReadSections(source, names);
            var dropIds = new HashSet<string>(StringComparer.Ordinal);
            int before = 0;
            foreach (string name in names)
            {
                before += sections[name].entityCount;
                foreach (var e in sections[name].entities)
                {
                    var f = Scenes.Fields(e);
                    var hit = Scenes.AssetRefs(e).Select(r => (r.type, r.name.ToLowerInvariant())).Where(r => lostKeys.Contains(r) && PruneTypes.Contains(r.Item1)).Distinct().ToList();
                    if (hit.Count == 0) continue;
                    dropIds.Add(Convert.ToString(f.entityId, System.Globalization.CultureInfo.InvariantCulture));
                    dropped.Add(name + " " + f.ns.Substring(f.ns.LastIndexOf('_') + 1) + " " + f.entityId);
                }
            }
            var parts = new List<byte[]>();
            for (int s = 0; s < names.Count; s++)
            {
                int start = offsets[s], end = s + 1 < names.Count ? offsets[s + 1] : source.Length;
                int chunksN = (int)Bin.U32LE(source, start), cursor = start + 8, total = 0;
                var chunks = new List<byte[]>();
                for (int c = 0; c < chunksN; c++)
                {
                    int size = 36 + (int)Bin.U32LE(source, cursor + 32);
                    var raw = Bin.Slice(source, cursor, size); cursor += size;
                    var doc = Bxml.Parse(raw, scene: true);
                    var nodes = new List<BxNode> { null }; var attrs = new List<BxAttr>();
                    bool changed = false;
                    void Build(int old, int idx)
                    {
                        var node = doc.nodes[old]; var kids = new List<int>();
                        foreach (int child in doc.Children(old))
                        {
                            if (doc.Tag(child) == "Entity")
                            {
                                if (dropIds.Contains(Convert.ToString(doc.AttributeAt(child, "entityId"), System.Globalization.CultureInfo.InvariantCulture))) { changed = true; continue; }
                                total++;
                            }
                            kids.Add(child);
                        }
                        int a = attrs.Count; foreach (int i in doc.AttrIndices(old)) attrs.Add(doc.attrs[i]);
                        int startIdx = nodes.Count; for (int q = 0; q < kids.Count; q++) nodes.Add(null);
                        var n2 = node.Clone(); n2.childStart = (uint)startIdx; n2.childCount = (uint)kids.Count; n2.attrStart = (uint)a;
                        nodes[idx] = n2;
                        for (int j = 0; j < kids.Count; j++) Build(kids[j], startIdx + j);
                    }
                    Build(0, 0);
                    if (changed) { doc.nodes = nodes; doc.attrs = attrs; raw = doc.Serialize(); }
                    chunks.Add(raw);
                }
                Bin.Require(cursor == end, "Scene section extent");
                var o = new MemoryStream(); var hdr = new byte[8]; Bin.PutU32LE(hdr, 0, (uint)chunksN); Bin.PutU32LE(hdr, 4, (uint)total); o.Write(hdr, 0, 8);
                foreach (var ch in chunks) o.Write(ch, 0, ch.Length);
                parts.Add(o.ToArray());
            }
            var header = new byte[8 + 264 * names.Count];
            Bin.PutU32LE(header, 0, magic); Bin.PutU32LE(header, 4, count);
            int at = header.Length;
            for (int s = 0; s < names.Count; s++)
            {
                var nb = Encoding.UTF8.GetBytes(names[s]); Buffer.BlockCopy(nb, 0, header, 8 + 264 * s, Math.Min(256, nb.Length));
                Bin.PutU32LE(header, 8 + 264 * s + 260, (uint)at); at += parts[s].Length;
            }
            var all = new byte[at]; Buffer.BlockCopy(header, 0, all, 0, header.Length); at = header.Length;
            foreach (var p in parts) { Buffer.BlockCopy(p, 0, all, at, p.Length); at += p.Length; }
            Bin.Require(Scenes.ReadSections(all, names).Values.Sum(x => x.entityCount) == before - dropped.Count, "Pruned scene entity count");
            return all;
        }
    }
}

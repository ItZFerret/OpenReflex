// MX vs ATV Alive's own menu pictures of each level (_re/formats/alive_pictures.py, byte-identical): the game's tables name them as its
// LocationSelect reads them, ActivityLocations (Filename = the level) -> the level's ActivityTypeLocations row of a riding activity
// (Supercross 8, Free Ride 9, National 10, Short track 19: PreviewIconId, MapIconId) -> Icons (IconId -> AssetName) -> the texture of that
// name in the MXUI* databases of the sources (the disc: MXUI_Streaming; each level pack: its own MXUI_DLC<n>):
//   preview  LP_*     1024 x 256 DXT1, the loading picture (a screenshot of the track)
//   map      LS_*     256 x 256 DXT5, the track select's layout of the track (National, Short track, Supercross)
//            FRMap_*  512 x 512 DXT1, the Free Ride map (an overhead of the whole terrain)
// Installed as PC texture v4 (Textures.ConvertBc: the blocks untiled, unchanged) to Database/alive-<level>-preview.texture / -map.texture,
// registry fields "preview" / "map"; the client draws them as it draws the install's UI textures (OpenContent.AlivePicture).
using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;

namespace Reflex.Port.Content.AliveImport
{
    /// Where one picture is: a texture asset of a database of a source volume.
    public sealed class AlivePictureRef
    {
        public Volume volume; public VolumeFile database; public string package, asset;
    }

    internal static class Pictures
    {
        public static readonly string[] Kinds = { "preview", "map" };
        static readonly string[] Columns = { "PreviewIconId", "MapIconId" };
        static readonly HashSet<long> Riding = new HashSet<long> { 8, 9, 10, 19 };

        /// alive_pictures.art_index: the pictures of every level the sources' tables name (the volumes in AliveImporter.PackOrder, within a
        /// directory the databases by name; the first ActivityLocations row of a file name, the first Icons row of an id and the first
        /// texture of a name win; of the level's riding rows, the first with an icon (>= 0)).
        public static Dictionary<string, Dictionary<string, AlivePictureRef>> Index(IEnumerable<Volume> volumes, CancellationToken cancel)
        {
            var locations = new Dictionary<string, long>(StringComparer.Ordinal);
            var rows = new List<(long location, long type, long preview, long map)>();
            var icons = new Dictionary<long, string>();
            var textures = new Dictionary<string, AlivePictureRef>(StringComparer.Ordinal);
            foreach (var v in volumes.OrderBy(AliveImporter.PackOrder))
                foreach (var dir in v.files.Select(f => f.Directory).Distinct())
                {
                    var inDir = v.files.Where(f => f.Directory == dir).OrderBy(f => f.Name, StringComparer.Ordinal).ToList();
                    foreach (var db in inDir.Where(f => f.Name.StartsWith("MXTables_", StringComparison.Ordinal) && f.Name.EndsWith(".xenon.database", StringComparison.Ordinal)))
                    {
                        if (!(db.Name.StartsWith("MXTables_Default", StringComparison.Ordinal) || db.Name.StartsWith("MXTables_DLC", StringComparison.Ordinal))) continue;
                        foreach (var data in AliveImporter.Assets(v, db, (p, n, t) => t == "tbldata", cancel))
                        {
                            var doc = Bxml.Parse(data, scene: true);
                            for (int i = 0; i < doc.nodes.Count; i++)
                            {
                                if (doc.Tag(i) != "Row") continue;
                                var a = doc.AttributesAt(i);
                                bool Has(string k) => a.Any(kv => kv.Key == k);
                                object A(string k) => a.FirstOrDefault(kv => kv.Key == k).Value;
                                if (Has("Filename") && A("Filename") is string fn && A("ActivityLocationId") is long loc && !locations.ContainsKey(fn)) locations[fn] = loc;
                                if (Has("ActivityTypeLocationId") && A("ActivityLocationId") is long l2 && A("ActivityTypeId") is long ty && A("PreviewIconId") is long pv && A("MapIconId") is long mp)
                                    rows.Add((l2, ty, pv, mp));
                                if (Has("AssetName") && Has("PackageDatabaseId") && A("IconId") is long icon && A("AssetName") is string an && !icons.ContainsKey(icon)) icons[icon] = an;
                            }
                        }
                    }
                    foreach (var db in inDir.Where(f => f.Name.StartsWith("MXUI", StringComparison.Ordinal) && f.Name.EndsWith(".xenon.database", StringComparison.Ordinal)))
                    {
                        var tree = Bxml.Parse(v.Read(db, cancel)).ToElement();
                        foreach (var package in tree.FindAll("Packages").SelectMany(p => p.FindAll("Package")))
                            foreach (var asset in package.FindAll("Asset"))
                            {
                                string name = asset.Get("name");
                                if (asset.Get("type") == "texture" && !string.IsNullOrEmpty(name) && !textures.ContainsKey(name))
                                    textures[name] = new AlivePictureRef { volume = v, database = db, package = package.Get("name"), asset = name };
                            }
                    }
                }
            var o = new Dictionary<string, Dictionary<string, AlivePictureRef>>(StringComparer.Ordinal);
            foreach (var kv in locations)
            {
                var mine = rows.Where(r => r.location == kv.Value && Riding.Contains(r.type)).ToList();
                var art = new Dictionary<string, AlivePictureRef>(StringComparer.Ordinal);
                for (int k = 0; k < Kinds.Length; k++)
                {
                    long icon = -1;
                    foreach (var r in mine) { long id = k == 0 ? r.preview : r.map; if (id >= 0) { icon = id; break; } }
                    if (icons.TryGetValue(icon, out var name) && textures.TryGetValue(name, out var tex)) art[Kinds[k]] = tex;
                }
                if (art.Count > 0) o[kv.Key] = art;
            }
            return o;
        }

        /// alive_pictures.picture: the PC texture v4 bytes of one picture (a BC texture of 4 .. 2048 texels a side), or null for another format.
        public static byte[] Picture(AlivePictureRef r, CancellationToken cancel)
        {
            var found = AliveImporter.Assets(r.volume, r.database, (p, n, t) => p == r.package && n == r.asset && t == "texture", cancel).ToList();
            Bin.Require(found.Count == 1, "Picture " + r.asset + " not found in " + r.database.Name);
            var s = found[0];
            Bin.Require(s.Length >= 56, "Truncated texture");
            uint fmt = Bin.U32BE(s, 52), flags = Bin.U32BE(s, 20), w = Bin.U32BE(s, 44), h = Bin.U32BE(s, 48);
            if (!Textures.IsBc(fmt) || (flags & 2) != 0 || w < 4 || w > 2048 || h < 4 || h > 2048) return null;
            return Textures.ConvertBc(s);
        }

        /// alive_pictures.level_pictures: (kind, bytes) of one level, in Kinds order (only those found).
        public static List<(string kind, byte[] data)> Of(AliveLevelSource src, CancellationToken cancel)
        {
            var o = new List<(string, byte[])>();
            if (src?.pictures == null) return o;
            foreach (var kind in Kinds)
                if (src.pictures.TryGetValue(kind, out var r))
                {
                    var data = Picture(r, cancel);
                    if (data != null) o.Add((kind, data));
                }
            return o;
        }
    }
}

// RxModSkinWriter.cs - a dependency-free writer of Open Reflex SKIN mods (.rxmod: an RXAR container of kind MODS, "type": "skins"), for
// skin exporters written in C# (.NET or Unity, C# 7.3+, no packages, no native library). It needs RxTrackWriter.cs beside it (the RXAR
// container writer shared with track archives). The format is core/plan/RXMOD.md "Skins"; the Python reference is tools/rxmod/rxmod.py
// (pack / check / token give the same archive and the same online token). The game loads the file from <game>/Mods/Bikes/ (any folder).
//
//   // a skin of a .rxmod bike: its map file replaced by yours (keys: the bike mod's own map entries, see the guide's table)
//   var skin = new RxModSkinWriter.Skin { Mod = "nfgcole-2023-yamaha-yz-f", Bike = "yz250f", Name = "Monster Energy" };
//   skin.Maps["yz250f/diffuse.png"] = File.ReadAllBytes("my-yz250f-livery.png");
//
//   // a skin of one of the game's own bikes: by VehicleModelId, maps by role (plastics, seat, frame, engine, rims, tires)
//   var orig = new RxModSkinWriter.Skin { Original = 31, Name = "Blue Takashi" };
//   orig.Maps["plastics"] = File.ReadAllBytes("plastics.png"); orig.Maps["seat"] = File.ReadAllBytes("seat.png");
//
//   string token = RxModSkinWriter.Write(@"C:\Out\My Livery.rxmod", "My Livery", "Your Name", new[] { skin, orig });
//   // token: the archive's content id ("e34cbb1974d9"); the game's online tokens are "s:" + token + ":" + <skin index>
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using System.Text;

public static class RxModSkinWriter
{
    public const uint KindMods = 0x53444F4D;   // "MODS" little endian (a .rxtrack is "TRAK", a .rxdata "DATA")
    public static readonly string[] Roles = { "plastics", "seat", "frame", "engine", "rims", "tires" };

    /// One skin of the file: either a .rxmod bike's (Mod + Bike) or an original bike's (Original = its VehicleModelId).
    public sealed class Skin
    {
        /// The bike mod it paints: its mod.json "id" (or its "name"). Null / empty for an original bike's skin.
        public string Mod;
        /// Which bike of that mod: its "id", model ("YZ250F"), brand + model or VehicleModelId; several: one Skin each.
        public string Bike;
        /// The game's own bike it paints (VehicleModelId: 31 Takashi 250F ...), -1: a .rxmod bike's skin.
        public int Original = -1;
        /// The skin's name: the garage card reads "<model> · <Name>" (a .rxmod bike) or "<Name>" (an original bike).
        public string Name = "";
        /// Key -> the PNG / JPG bytes. A .rxmod bike's skin: the bike mod's map entry it replaces ("yz250f/diffuse.png"). An original's: a role
        /// (plastics, seat, frame, engine, rims, tires) or an original texture's name.
        public readonly Dictionary<string, byte[]> Maps = new Dictionary<string, byte[]>(StringComparer.Ordinal);
        /// Optional per-material overrides of a .rxmod bike (material key of its mod.json -> field -> bytes): "diffuse", "spec", "normal", "sfx".
        public readonly Dictionary<string, Dictionary<string, byte[]>> Materials = new Dictionary<string, Dictionary<string, byte[]>>(StringComparer.Ordinal);
        /// Optional brand logo (PNG / JPG) for the garage stage.
        public byte[] Logo;
    }

    /// Writes the skins into one .rxmod (every picture under skin<index>/, mod.json at the root) and returns its content id (12 hex).
    public static string Write(string output, string name, string author, IList<Skin> skins, string description = "", byte[] thumbnail = null)
    {
        if (skins == null || skins.Count == 0) throw new ArgumentException("no skin");
        if (string.IsNullOrWhiteSpace(author)) throw new ArgumentException("an author is required (the garage credits it)");
        // one skin per target in a file (the game keeps the first; a .rxmod bike's skin and an original's can still collide when that bike
        // rides as that original: the YZ250F is #31 - see the guide's "One skin per bike")
        var seen = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        foreach (var k in skins)
            if (!seen.Add(k.Original >= 0 ? "#" + k.Original : (k.Mod ?? "") + "/" + (k.Bike ?? "")))
                throw new ArgumentException("two skins for " + (k.Original >= 0 ? "original bike #" + k.Original : k.Mod + " / " + k.Bike) + " in one file: one per bike (a second livery goes in a file of its own)");
        var files = new List<KeyValuePair<string, byte[]>>();
        var sb = new StringBuilder();
        sb.Append("{\n \"format\": \"reflex-mod\",\n \"version\": 1,\n \"type\": \"skins\",\n \"name\": ").Append(Json(name))
          .Append(",\n \"author\": ").Append(Json(author)).Append(",\n \"description\": ").Append(Json(description ?? ""));
        if (thumbnail != null) { Check(thumbnail, "thumbnail"); files.Add(Pair("preview" + Ext(thumbnail), thumbnail)); sb.Append(",\n \"thumbnail\": ").Append(Json("preview" + Ext(thumbnail))); }
        sb.Append(",\n \"skins\": [");
        for (int i = 0; i < skins.Count; i++)
        {
            var k = skins[i];
            bool original = k.Original >= 0;
            if (!original && string.IsNullOrWhiteSpace(k.Mod)) throw new ArgumentException("skin " + i + ": Mod (the bike mod's id or name) or Original (a VehicleModelId)");
            if (k.Maps.Count == 0 && k.Materials.Count == 0) throw new ArgumentException("skin " + i + " replaces no map");
            string dir = "skin" + i + "/";
            sb.Append(i == 0 ? "\n  {" : ",\n  {");
            if (original) sb.Append(" \"original\": ").Append(k.Original);
            else sb.Append(" \"mod\": ").Append(Json(k.Mod)).Append(", \"bike\": ").Append(Json(k.Bike ?? ""));
            sb.Append(", \"name\": ").Append(Json(k.Name ?? ""));
            if (k.Logo != null) { Check(k.Logo, "logo"); files.Add(Pair(dir + "logo" + Ext(k.Logo), k.Logo)); sb.Append(", \"logo\": ").Append(Json(dir + "logo" + Ext(k.Logo))); }
            if (k.Maps.Count > 0)
            {
                sb.Append(",\n    \"maps\": {");
                int n = 0;
                foreach (var kv in k.Maps.OrderBy(x => x.Key, StringComparer.Ordinal))
                {
                    if (original && Array.IndexOf(Roles, kv.Key) < 0 && kv.Key.IndexOf('_') < 0) throw new ArgumentException("skin " + i + ": map key " + kv.Key + " is neither a role nor a texture name");
                    Check(kv.Value, "skin " + i + " map " + kv.Key);
                    string entry = dir + Safe(kv.Key) + Ext(kv.Value);
                    files.Add(Pair(entry, kv.Value));
                    sb.Append(n++ == 0 ? " " : ", ").Append(Json(kv.Key)).Append(": ").Append(Json(entry));
                }
                sb.Append(" }");
            }
            if (k.Materials.Count > 0)
            {
                sb.Append(",\n    \"materials\": {");
                int n = 0;
                foreach (var m in k.Materials.OrderBy(x => x.Key, StringComparer.Ordinal))
                {
                    sb.Append(n++ == 0 ? " " : ", ").Append(Json(m.Key)).Append(": {");
                    int f = 0;
                    foreach (var kv in m.Value.OrderBy(x => x.Key, StringComparer.Ordinal))
                    {
                        if (kv.Key != "diffuse" && kv.Key != "spec" && kv.Key != "normal" && kv.Key != "sfx") throw new ArgumentException("skin " + i + " material " + m.Key + ": " + kv.Key + " (diffuse, spec, normal or sfx)");
                        Check(kv.Value, "skin " + i + " material " + m.Key + " " + kv.Key);
                        string entry = dir + Safe(m.Key) + "_" + kv.Key + Ext(kv.Value);
                        files.Add(Pair(entry, kv.Value));
                        sb.Append(f++ == 0 ? " " : ", ").Append(Json(kv.Key)).Append(": ").Append(Json(entry));
                    }
                    sb.Append(" }");
                }
                sb.Append(" }");
            }
            sb.Append(" }");
        }
        sb.Append("\n ]\n}\n");
        files.Insert(0, Pair("mod.json", Encoding.UTF8.GetBytes(sb.ToString())));
        // the same metadata rxmod.py writes: the game reads the type from it before unpacking anything
        string meta = "{\"format\":\"reflex-mod\",\"type\":\"skins\",\"name\":" + Json(name) + ",\"author\":" + Json(author) + "}";
        RxTrackWriter.Write(output, files, meta, KindMods);
        return ContentId(files);
    }

    /// The content id the game derives from an archive (RxModBikes.Hash): SHA-256 over "path:size:sha256" lines of every entry, sorted by
    /// path bytes, its first 6 bytes in hex. The online tokens are "s:<id>:<skin index>" (skins) and "b:<id>:<VehicleModelId>" (bikes).
    public static string ContentId(IEnumerable<KeyValuePair<string, byte[]>> files)
    {
        var lines = files.OrderBy(f => f.Key, StringComparer.Ordinal)   // (ASCII paths: ordinal = byte order)
                         .Select(f => f.Key + ":" + f.Value.Length + ":" + Hex(Sha(f.Value)));
        return Hex(Sha(Encoding.UTF8.GetBytes(string.Join("\n", lines)))).Substring(0, 12);
    }

    /// A map must be a PNG or a JPG whose sides are powers of two, at most 4096 (the game uploads it as is, mipmapped and block-compressed).
    public static void Check(byte[] b, string what)
    {
        var (w, h) = Size(b);
        if (w == 0) throw new InvalidDataException(what + ": not a PNG or JPG");
        if (w > 4096 || h > 4096 || (w & (w - 1)) != 0 || (h & (h - 1)) != 0) throw new InvalidDataException(what + ": " + w + "x" + h + " (powers of two, at most 4096)");
    }

    /// (width, height) of a PNG or a baseline / progressive JPG, (0, 0) for anything else.
    public static (int w, int h) Size(byte[] b)
    {
        if (b != null && b.Length >= 24 && b[0] == 0x89 && b[1] == 'P' && b[2] == 'N' && b[3] == 'G')
            return ((b[16] << 24) | (b[17] << 16) | (b[18] << 8) | b[19], (b[20] << 24) | (b[21] << 16) | (b[22] << 8) | b[23]);
        if (b != null && b.Length > 4 && b[0] == 0xFF && b[1] == 0xD8)
            for (int i = 2; i + 9 < b.Length;)
            {
                if (b[i] != 0xFF) { i++; continue; }
                int m = b[i + 1], len = (b[i + 2] << 8) | b[i + 3];
                if (m == 0xC0 || m == 0xC1 || m == 0xC2) return ((b[i + 7] << 8) | b[i + 8], (b[i + 5] << 8) | b[i + 6]);
                i += 2 + len;
            }
        return (0, 0);
    }

    static string Ext(byte[] b) => b.Length > 1 && b[0] == 0xFF && b[1] == 0xD8 ? ".jpg" : ".png";
    static string Safe(string key)
    {
        var sb = new StringBuilder();
        foreach (char c in key) sb.Append(char.IsLetterOrDigit(c) || c == '-' || c == '_' ? c : '_');
        return sb.Length == 0 ? "map" : sb.ToString();
    }
    static KeyValuePair<string, byte[]> Pair(string k, byte[] v) => new KeyValuePair<string, byte[]>(k, v);
    static byte[] Sha(byte[] d) { using (var s = SHA256.Create()) return s.ComputeHash(d); }
    static string Hex(byte[] b) { var sb = new StringBuilder(b.Length * 2); foreach (byte x in b) sb.Append(x.ToString("x2")); return sb.ToString(); }
    static string Json(string s)
    {
        var sb = new StringBuilder("\"");
        foreach (char c in s ?? "")
            if (c == '"' || c == '\\') sb.Append('\\').Append(c);
            else if (c < 0x20) sb.Append("\\u").Append(((int)c).ToString("x4"));
            else sb.Append(c);
        return sb.Append('"').ToString();
    }
}

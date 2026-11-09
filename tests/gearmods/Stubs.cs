// TEST ONLY (tests/gearmods): the few game types KitPacks / RfxData name, as the game defines them, so the pack readers build on .NET.
using System;
using System.Text;

namespace Reflex.Port.Content
{
    internal static class BikeMods
    {
        public const string NamespaceMark = "mod:";
        public static (string id, string db) Split(string db)
        {
            if (db == null || !db.StartsWith(NamespaceMark, StringComparison.Ordinal)) return (null, db);
            int e = db.LastIndexOf('>');
            return e < NamespaceMark.Length ? (null, db) : (db.Substring(NamespaceMark.Length, e - NamespaceMark.Length), db.Substring(e + 1));
        }
        public static string BaseDb(string db) => Split(db).db;
    }
    public static class ReflexAssetImport
    {
        public const string ExtractedMarker = "reflex-extracted-v1.txt";
        // (ReflexAssetImport.Segment / AssetPath: the import's own mapping)
        public static string Segment(string value)
        {
            var result = new StringBuilder();
            foreach (byte b in Encoding.UTF8.GetBytes(value ?? ""))
                if (b >= 65 && b <= 90 || b >= 97 && b <= 122 || b >= 48 && b <= 57 || b == 95 || b == 45) result.Append((char)b); else result.Append('%').Append(b.ToString("X2"));
            return result.ToString();
        }
        public static string AssetPath(string database, string package, string asset, string type) => "Assets/" + Segment(type) + "/" + Segment(database) + "/" + Segment(package) + "/" + Segment(asset) + "." + Segment(type);
    }
}

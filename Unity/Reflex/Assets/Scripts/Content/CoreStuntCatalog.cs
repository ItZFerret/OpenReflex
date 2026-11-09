using System;
using System.Collections.Generic;
using System.Linq;

namespace Reflex.Port.Content
{
    // Read-only preview projection of006c8b70/006c8670. Physics is the XML
    // attribute-read success result, not the value stored in the Physics field.
    internal static class CoreStuntCatalog
    {
        internal static List<(int tier, string name, string key)> LoadAll()
        {
            return Parse(ReflexInstall.Packages.Read("StuntDatabase", "StuntData", "StuntTable", "bxml"));
        }

        /// The StuntTable's names as written (the perfdat of a stunt is "<MX|ATV>Rider_<name>"), by the lower-cased name the list above carries.
        internal static Dictionary<string, string> RawNames()
        {
            var root = BxmlData.Xml(ReflexInstall.Packages.Read("StuntDatabase", "StuntData", "StuntTable", "bxml"));
            var map = new Dictionary<string, string>(StringComparer.Ordinal);
            foreach (var n in root.children.Where(x => x.tag == "Stunt"))
            {
                string name = n.S("name"); if (string.IsNullOrEmpty(name)) continue;
                string lower = (name.Length > 30 ? name.Substring(0, 30) : name).ToLowerInvariant();
                if (!map.ContainsKey(lower)) map[lower] = name;
            }
            return map;
        }

        internal static List<(int tier, string name, string key)> Parse(byte[] data)
        {
            var root = BxmlData.Xml(data);
            BxmlData.Check(root.tag == "StuntTable", "StuntTable root");
            object Value(BxmlData.Node row, string child)
            {
                var node = row.Child(child);
                return node != null && node.attributes.TryGetValue("value", out var value) ? value : null;
            }
            return FromRows(root.children.Where(n => n.tag == "Stunt")
                .Select(n => (n.S("name"), Value(n, "Physics"), Value(n, "Tier"), Value(n, "LocString") as string)));
        }

        internal static List<(int tier, string name, string key)> FromRows(IEnumerable<(string name, object physics, object tier, string key)> rows)
        {
            var tiers = new List<(int tier, string name, string key)>[4];
            for (int i = 0; i < tiers.Length; i++) tiers[i] = new List<(int, string, string)>();
            foreach (var row in rows)
            {
                // build_xml.c attr_values: direct bool(type11) always succeeds,
                // including false; string(type1) succeeds iff its first byte exists.
                if (!(row.physics is bool) && !(row.physics is string text && text.Length != 0)) continue;
                int tier = row.tier is int value ? value : row.tier is string number ? ReadInteger(number) : 0;
                if (tier < 1 || tier > 4) continue;
                string name = row.name ?? "";
                //0078fd60(size31) terminates at byte30. Installed identifiers are ASCII.
                if (name.Length > 30) name = name.Substring(0, 30);
                char[] chars = name.ToCharArray();
                for (int i = 0; i < chars.Length; i++) if (chars[i] >= 'A' && chars[i] <= 'Z') chars[i] = (char)(chars[i] + ('a' - 'A'));
                tiers[tier - 1].Add((tier, new string(chars), row.key));
            }
            // Original per-tier vectors preserve source row order and duplicate keys.
            // FeStuntPreview.Fill alone removes later duplicate display-name keys.
            return tiers.SelectMany(t => t).ToList();
        }

        //006c8670 Tier uses the int attribute reader: typed int or CRT strtol(base0),
        // plus the reader's leading T/F shortcut. Failed/other types leave zero.
        static int ReadInteger(string text)
        {
            if (text.Length > 0 && (text[0] == 'T' || text[0] == 't')) return 1;
            if (text.Length > 0 && (text[0] == 'F' || text[0] == 'f')) return 0;
            int p = 0;
            while (p < text.Length && " \t\n\r\v\f".IndexOf(text[p]) >= 0) p++;
            bool negative = p < text.Length && text[p] == '-';
            if (p < text.Length && (text[p] == '+' || text[p] == '-')) p++;
            int radix = p < text.Length && text[p] == '0' ? 8 : 10;
            if (p + 2 < text.Length && text[p] == '0' && (text[p + 1] == 'x' || text[p + 1] == 'X') && Hex(text[p + 2]) >= 0)
            { radix = 16; p += 2; }
            long number = 0;
            while (p < text.Length)
            {
                int digit = Hex(text[p++]);
                if (digit < 0 || digit >= radix) break;
                number = Math.Min(2147483648L, number * radix + digit);
            }
            return negative ? (int)-number : (int)Math.Min(int.MaxValue, number);
        }
        static int Hex(char c) => c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
    }
}

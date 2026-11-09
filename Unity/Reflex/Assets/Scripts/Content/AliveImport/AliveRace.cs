// PORT ADDITION (w-aliveai): the race data of an Alive level for Reflex's race activities, the C# port of _re/formats/alive_race.py (byte
// parity): the track definition (the tdf resource's size word little endian, the stream validated as Reflex's parser 00536ef0 reads it),
// the race activity's track and gate size, the lap pacing in Reflex's schema (engine nodes renamed MXEngineClass_<size>). The mapping and
// the gaps: core/plan/ALIVE_RACING.md.
using System;
using System.Collections.Generic;
using System.Linq;

namespace Reflex.Port.Content.AliveImport
{
    internal static class AliveRace
    {
        public const string Gate = "TO_StartingGate";

        /// alive_race.ACTIVITIES: Alive's race activity -> the Reflex event it races as (UI EventType, EventTypeId, .tdf07 suffix, lap pacing
        /// node, the race's subscene after the level name). A short track races as a National.
        public sealed class Spec { public string eventType, suffix, pacing, subscene; public int eventTypeId; }
        public static readonly Dictionary<string, Spec> Activities = new Dictionary<string, Spec>(StringComparer.Ordinal)
        {
            { "National", new Spec { eventType = "National", eventTypeId = 10, suffix = "nat", pacing = "National0", subscene = "ExhibitionNational" } },
            { "ShortTrack", new Spec { eventType = "National", eventTypeId = 10, suffix = "nat", pacing = "National0", subscene = "ExhibitionNational" } },
            { "Supercross", new Spec { eventType = "Supercross", eventTypeId = 8, suffix = "sx", pacing = "Supercross0", subscene = "ExhibitionSupercross" } },
        };
        static readonly Dictionary<string, string> Engines = new Dictionary<string, string> { { "125", "125cc" }, { "250", "250cc" }, { "450", "500cc" } };

        static readonly HashSet<uint> Grid = new HashSet<uint> { 0x06, 0x12, 0x13, 0x1c, 0x1d, 0x21, 0x22, 0x28, 0x2e, 0x2f, 0x30, 0x31 };
        static readonly HashSet<uint> Vec3 = new HashSet<uint> { 0x01, 0x02, 0x03, 0x15, 0x16, 0x17, 0x1a, 0x2c, 0x2d };
        static readonly HashSet<uint> Word = new HashSet<uint> { 0x04, 0x05, 0x07, 0x0d, 0x11, 0x18, 0x19, 0x1b, 0x1f, 0x26, 0x27, 0x29, 0x2a };
        static readonly HashSet<uint> Record = new HashSet<uint> { 0x08, 0x09, 0x0a, 0x23, 0x2b };

        /// alive_race.walk_tdf over data[start..]: raises unless Reflex's parser reads the stream to its exact end.
        public static void WalkTdf(byte[] data, int start)
        {
            int len = data.Length;
            Bin.Require(len - start >= 16, "Truncated track definition");
            uint nNodes = Bin.U32BE(data, start), nSections = Bin.U32BE(data, start + 4), nX = Bin.U32BE(data, start + 8), nY = Bin.U32BE(data, start + 12);
            Bin.Require(nNodes > 0 && nNodes <= 0x10000 && nSections > 0 && nSections <= 0x1000 && nX <= 0x1000 && nY <= 0x1000, "Track definition counts");
            long p = start + 16; uint nodes = 0, sections = 0;
            while (p + 8 <= len)
            {
                uint tag = Bin.U32BE(data, (int)p), length = Bin.U32BE(data, (int)p + 4);
                p += 8;
                long n;
                if (Grid.Contains(tag))
                {
                    Bin.Require(p + 8 <= len, "Truncated track grid");
                    uint r = Bin.U32BE(data, (int)p), c = Bin.U32BE(data, (int)p + 4);
                    Bin.Require(r <= 8 && c <= 4, "Track grid larger than 8 x 4");
                    n = 8 + 4L * r * c;
                }
                else if (Vec3.Contains(tag)) n = 12;
                else if (Word.Contains(tag)) n = 4;
                else if (Record.Contains(tag)) n = 0x30;
                else if (tag == 0x0b || tag == 0x14)
                {
                    n = 0;
                    if (tag == 0x14) nodes++; else sections++;
                    Bin.Require(nodes <= nNodes && sections <= nSections, "More nodes / sections than the header counts");
                }
                else if (tag == 0x0c || tag == 0x0f) n = 4;
                else if (tag == 0x0e || tag == 0x10)
                {
                    n = 8;
                    Bin.Require(p + 8 <= len, "Truncated section list entry");
                    Bin.Require(Bin.U32BE(data, (int)p) < nSections && Bin.U32BE(data, (int)p + 4) < nSections, "Section list entry out of range");
                }
                else if (tag == 0x24)
                {
                    Bin.Require(p + 4 <= len, "Truncated class limits");
                    uint count = Bin.U32BE(data, (int)p);
                    Bin.Require(count <= 8, "More than 8 class limits");
                    n = 4 + 4L * count;
                }
                else n = length;
                Bin.Require(p + n <= len, "Track definition record exceeds the asset");
                p += n;
            }
            Bin.Require(p == len, "Track definition stream does not end on a record");
            Bin.Require(nodes == nNodes && sections == nSections, "Node / section counts differ from the header");
        }

        /// alive_race.convert_tdf: the size word little endian, the stream unchanged.
        public static byte[] ConvertTdf(byte[] data)
        {
            Bin.Require(data.Length >= 20 && Bin.U32BE(data, 0) == (uint)(data.Length - 4), "Unsupported track definition header");
            WalkTdf(data, 4);
            var o = (byte[])data.Clone();
            Bin.PutU32LE(o, 0, (uint)(data.Length - 4));
            return o;
        }

        /// alive_race.race_activity: (Alive activity type, tdf asset name, max players) of a race activity resource, or null for any other
        /// (the level preview, the multiplayer lobby). tdf "": the race names no track (NAT_Southwest*, NAT_Tropic*): the level's own (ResolveTrack).
        public static (string activity, string tdf, long maxPlayers)? RaceActivity(byte[] bxml)
        {
            var root = Bxml.Parse(bxml).ToElement();
            var config = root.Find("configuration");
            if (config == null) return null;
            var values = new Dictionary<string, string>(StringComparer.Ordinal);
            foreach (var c in config.children) values[c.tag] = c.Get("value");
            string script = (values.TryGetValue("scriptobjectname", out var so) ? so ?? "" : "").Split('.')[0];
            if (!Activities.ContainsKey(script)) return null;
            string tdf = values.TryGetValue("tdfassetname", out var t) ? t ?? "" : "";
            if (tdf.EndsWith(".tdf", StringComparison.Ordinal)) tdf = tdf.Substring(0, tdf.Length - 4);
            string mp = values.TryGetValue("maxplayerssupported", out var m) ? m : null;
            return (script, tdf, string.IsNullOrEmpty(mp) ? 0 : long.Parse(mp, System.Globalization.CultureInfo.InvariantCulture));
        }

        /// alive_race.resolve_track: a race that names no track definition races on the level's only one.
        public static void ResolveTrack(Conversion c)
        {
            if (!string.IsNullOrEmpty(c.raceTdf)) return;
            var names = c.converted.Where(a => a.type == "tdf").Select(a => a.name).Concat(c.pending.Where(p => p.type == "tdf").Select(p => p.name)).Distinct().ToList();
            Bin.Require(names.Count == 1, "The race activity names no track definition and the level has " + names.Count);
            c.raceTdf = names[0];
        }

        /// alive_race.convert_lap_pacing: Reflex's lap pacing tree from an Alive one (deterministic BXML).
        public static byte[] ConvertLapPacing(byte[] bxml, string pacingNode)
        {
            var root = Bxml.Parse(bxml).ToElement();
            Bin.Require(root.tag == "LapPacing" && root.children.Count >= 1, "Not a lap pacing tree");
            var activity = root.children[0];
            var o = new Elem("LapPacing");
            var node = new Elem(pacingNode); o.children.Add(node);
            foreach (var cls in activity.children)
            {
                Bin.Require(cls.tag == "Atv" || cls.tag == "MotocrossBike", "Unknown lap pacing class " + cls.tag);
                var c = new Elem(cls.tag); node.children.Add(c);
                var seen = new HashSet<string>();
                foreach (var engine in cls.children)
                {
                    string digits = new string(engine.tag.Where(ch => ch >= '0' && ch <= '9').ToArray());
                    Bin.Require(Engines.TryGetValue(digits, out var size), "Unknown lap pacing engine " + engine.tag);
                    Bin.Require(seen.Add(size), "Two lap pacing engines map to " + size);
                    var e = new Elem("MXEngineClass_" + size); c.children.Add(e);
                    foreach (var key in new[] { "NormalTime", "Pro", "Hard", "Normal", "Easy" })
                    {
                        var tv = engine.Find(key);
                        Bin.Require(tv != null && tv.text != null, "Lap pacing " + engine.tag + " without " + key);
                        Bin.Require(double.TryParse(tv.text.Trim(), System.Globalization.NumberStyles.Float, System.Globalization.CultureInfo.InvariantCulture, out _), "Lap pacing value");
                        e.children.Add(new Elem(key) { text = tv.text.Trim() });
                    }
                }
            }
            return Bxml.EncodeElement(o);
        }
    }
}

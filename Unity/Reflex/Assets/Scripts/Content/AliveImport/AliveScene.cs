// Scene containers (0xC0FFFEEE): the Xbox-to-PC header swap (alive_convert.convert_scene), section reading (scene_read.read_sections /
// entity_fields) and the installed Free Ride scene (alive_install.free_ride_scene: Base without deferred entities + a FreeRide section).
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace Reflex.Port.Content.AliveImport
{
    internal sealed class Entity
    {
        public object entityId; public string ns;
        public readonly List<KeyValuePair<string, List<Record>>> fields = new List<KeyValuePair<string, List<Record>>>();
        public string Type => ns.Substring(ns.LastIndexOf('_') + 1);
        public List<Record> Field(string name) { foreach (var kv in fields) if (kv.Key == name) return kv.Value; return null; }
        /// The `attributes.name` of the field's first record (an Asset reference), or null.
        public string AssetName(string key)
        {
            var f = Field(key);
            return f != null && f.Count > 0 ? f[0].Attr("name") as string : null;
        }
        public object FirstValue(string key, object def = null) { var f = Field(key); return f != null && f.Count > 0 ? f[0].value : def; }
    }

    internal sealed class Section
    {
        public string name; public int offset, bytes, chunks, entityCount;
        public readonly List<Record> entities = new List<Record>();
    }

    internal static class Scenes
    {
        /// The PC (little endian) container from an Xbox one: only the table words are swapped (the embedded BXML is little endian).
        public static byte[] ConvertScene(byte[] source, out List<string> names)
        {
            Bin.Require(source.Length >= 8, "Truncated scene");
            uint magic = Bin.U32BE(source, 0), count = Bin.U32BE(source, 4);
            Bin.Require(magic == 0xc0fffeee && count > 0 && count <= 128, "Unsupported Xbox scene");
            Bin.Require(8 + count * 264 <= source.Length, "Truncated scene table");
            var data = (byte[])source.Clone();
            Bin.SwapWords(data, 0, 8);
            names = new List<string>();
            for (int i = 0; i < count; i++)
            {
                int p = 8 + i * 264;
                int z = Array.IndexOf(source, (byte)0, p, 256); if (z < 0) z = p + 256;
                names.Add(Encoding.ASCII.GetString(source, p, z - p));
                int offset = (int)Bin.U32BE(source, p + 260);
                Bin.SwapWords(data, p + 256, 8);
                Bin.SwapWords(data, offset, 8);
            }
            ReadSections(data, names);
            return data;
        }

        public static Dictionary<string, Section> ReadSections(byte[] data, IList<string> selected)
        {
            Bin.Require(data.Length >= 8 && data.Length <= 128 * 1024 * 1024, "Scene container outside inspection bounds");
            uint magic = Bin.U32LE(data, 0), count = Bin.U32LE(data, 4);
            Bin.Require(magic == 0xc0fffeee && count > 0 && count <= 128, "Invalid scene container header");
            int tableEnd = 8 + (int)count * 264;
            Bin.Require(tableEnd <= data.Length, "Truncated scene section table");
            var table = new List<(string name, int offset)>();
            for (int i = 0; i < count; i++)
            {
                int p = 8 + i * 264;
                int z = Array.IndexOf(data, (byte)0, p, 256);
                Bin.Require(z > p, "Invalid scene section name padding");
                for (int k = z; k < p + 256; k++) Bin.Require(data[k] == 0, "Invalid scene section name padding");
                uint reserved = Bin.U32LE(data, p + 256); int offset = (int)Bin.U32LE(data, p + 260);
                Bin.Require(reserved == 0 && tableEnd <= offset && offset < data.Length, "Invalid scene section pointer");
                table.Add((Encoding.UTF8.GetString(data, p, z - p), offset));
            }
            Bin.Require(table.Select(t => t.name).Distinct().Count() == table.Count, "Duplicate scene section name");
            for (int i = 1; i < table.Count; i++) Bin.Require(table[i].offset > table[i - 1].offset, "Scene sections are duplicated or out of order");
            foreach (var s in selected) Bin.Require(table.Any(t => t.name == s), "Requested scene section is absent");
            var result = new Dictionary<string, Section>();
            for (int index = 0; index < table.Count; index++)
            {
                var (name, start) = table[index];
                if (!selected.Contains(name)) continue;
                int end = index + 1 < table.Count ? table[index + 1].offset : data.Length;
                Bin.Require(start + 8 <= end, "Truncated scene section header");
                int chunks = (int)Bin.U32LE(data, start), entities = (int)Bin.U32LE(data, start + 4);
                Bin.Require(chunks > 0 && chunks <= 4096 && entities <= 100000, "Scene section counts exceed bounds");
                var sec = new Section { name = name, offset = start, bytes = end - start, chunks = chunks, entityCount = entities };
                int cursor = start + 8;
                for (int c = 0; c < chunks; c++)
                {
                    Bin.Require(cursor + 36 <= end, "Truncated embedded BXML header");
                    Bin.Require(Bin.StartsWith(data, "BXML", cursor) && Bin.U32LE(data, cursor + 4) == Bxml.VersionFlags, "Unsupported embedded scene BXML");
                    int size = 36 + (int)Bin.U32LE(data, cursor + 32);
                    Bin.Require(cursor + size <= end, "Embedded BXML crosses section boundary");
                    var doc = Bxml.Parse(Bin.Slice(data, cursor, size), scene: true);
                    for (int n = 0; n < doc.nodes.Count; n++)
                        if (doc.Tag(n) == "Entity") sec.entities.Add(doc.AsRecord(n));
                    cursor += size;
                }
                Bin.Require(cursor == end && sec.entities.Count == entities, "Scene section extent/entity count mismatch");
                result[name] = sec;
            }
            return result;
        }

        public static Entity Fields(Record record)
        {
            Bin.Require(record.name == "Entity" && record.children.Count == 1, "Unsupported entity wrapper");
            var root = record.children[0];
            Bin.Require(root.name == "RSData", "Missing entity RSData");
            var e = new Entity { entityId = record.Attr("entityId"), ns = root.Attr("namespace") as string };
            foreach (var child in root.children)
            {
                var name = child.Attr("name") as string;
                Bin.Require(!string.IsNullOrEmpty(name) && e.Field(name) == null, "Unnamed or duplicate entity field");
                e.fields.Add(new KeyValuePair<string, List<Record>>(name, child.children));
            }
            return e;
        }

        /// (type, name) of every Asset reference in an entity's fields (alive_install.asset_refs).
        public static IEnumerable<(string type, string name)> AssetRefs(Record r)
        {
            if (r.name == "Asset" && r.HasAttr("name") && r.HasAttr("type"))
                yield return (Convert.ToString(r.Attr("type"), System.Globalization.CultureInfo.InvariantCulture), Convert.ToString(r.Attr("name"), System.Globalization.CultureInfo.InvariantCulture));
            foreach (var c in r.children) foreach (var x in AssetRefs(c)) yield return x;
        }

        public static readonly HashSet<string> Deferred = new HashSet<string>
        {
            "LevelZoneEntityRSData", "VehicleSwapEntityRSData", "BounceLightRSData", "SunFlareRSData", "ColorLUTRSData",
            // Alive-only animated entities (the looping / instanced / spline / roaming ones are Reflex entity types: their converted
            // .anim streams play in the core like Reflex's own crowds, banners and birds)
            "AnimEntityProximityRSData", "AnimEntityTriggerRSData", "ScenarioAnimEntityRSData",
            "ChallengeSpecsRSData", "GeneralStorageEntityRSData", "LensFlareRSData",
            "MusicScoreListRSData", "MusicTrackListRSData", "HFDecalModelRSData",
            "PlayerTriggerRSData", "TriggerRSData", "LeaderBoardRSData", "ScoreTowerFlagHolderRSData", "LevelInfoRSData",
            "RaceActivityAudioManagerRSData",
        };
        public static readonly HashSet<string> BlockingTypes = new HashSet<string> { "anim", "animset", "tdf", "celib" };

        public static string Deferral(Entity e, HashSet<(string, string)> missing)
        {
            if (Deferred.Contains(e.Type)) return "entity type has no Reflex equivalent / Free Ride role";
            var refs = new SortedSet<string>(StringComparer.Ordinal);
            foreach (var kv in e.fields) foreach (var r in kv.Value) foreach (var (t, n) in AssetRefs(r)) if (missing.Contains((t, n))) refs.Add(t + " " + n);
            if (refs.Count > 0) return "references unconverted resources: " + string.Join(", ", refs.Take(4));
            return null;
        }

        static string Canonical(Record r)
        {
            var sb = new StringBuilder(); Canon(sb, r); return sb.ToString();
        }
        static void Canon(StringBuilder sb, object v)
        {
            switch (v)
            {
                case null: sb.Append('~'); break;
                case Record r:
                    sb.Append('{').Append(r.name).Append('|'); Canon(sb, r.value);
                    foreach (var kv in r.attributes) { sb.Append(kv.Key).Append('='); Canon(sb, kv.Value); }
                    foreach (var c in r.children) Canon(sb, c);
                    sb.Append('}'); break;
                case Opaque o: sb.Append(Bin.Hex(o.bytes, 0, o.bytes.Length)); break;
                case double d: sb.Append(BitConverter.DoubleToInt64Bits(d)).Append(';'); break;
                case double[] a: foreach (var x in a) Canon(sb, x); sb.Append(';'); break;
                case System.Collections.IEnumerable list when !(v is string): sb.Append('['); foreach (var x in list) Canon(sb, x); sb.Append(']'); break;
                default: sb.Append(Convert.ToString(v, System.Globalization.CultureInfo.InvariantCulture)).Append(';'); break;
            }
        }

        // Alive's pop-up / pop-down bales (w-fb-models, alive_install.POP_BALE_*): Reflex has entity types for them (PopUpBaleRSData /
        // PopDownBaleRSData: hidden 5 ft under their place or standing there, swapped when a vehicle leaves the trigger their LinkedTriggerID
        // names, the end of the first lap); Alive draws the same props as plain PhysicsRSData tagged by their Groups string set ("PopUpBale" /
        // "PopDownBale") and fires them from a PlayerTriggerRSData whose TriggerID is "PopUpBaleTrigger" (in Base, e.g. SX_DLC_SX01, or in the
        // race activity's own section, e.g. NAT_Farm_BNational0). Installed as they were, the bales stood as ordinary props: the ones that should
        // rise were up from the start and the ones that should sink under the track never did. They are installed as Reflex's bale entities
        // linked to that trigger, and the trigger as a Reflex TriggerRSData (Base: every activity; an activity section: the race subscene).
        public const string PopBaleTrigger = "PopUpBaleTrigger";
        static readonly Dictionary<string, string> PopBaleKinds = new Dictionary<string, string>
        {
            ["PopUpBale"] = "RSToolsNS_RSDataNS_PopUpBaleRSData", ["PopDownBale"] = "RSToolsNS_RSDataNS_PopDownBaleRSData",
        };
        const string TriggerNs = "RSToolsNS_RSDataNS_TriggerRSData";

        /// "PopUpBale" / "PopDownBale" for a PhysicsRSData entity tagged so in its Groups, else null.
        public static string PopBaleKind(Entity e)
        {
            if (e.Type != "PhysicsRSData") return null;
            var groups = e.Field("Groups");
            if (groups == null) return null;
            foreach (var item in groups)
                foreach (var c in item.children)
                    if (c.name == "String" && c.value is string v && PopBaleKinds.ContainsKey(v)) return v;
            return null;
        }

        public static bool IsPopBaleTrigger(Entity e) => e.Type == "PlayerTriggerRSData" && e.FirstValue("TriggerID") as string == PopBaleTrigger;

        /// The index of `text` in the document's string pool (appended when absent).
        static uint Intern(Bxml doc, string text)
        {
            int i = doc.strings.IndexOf(text);
            if (i >= 0) return (uint)i;
            doc.strings.Add(text); doc.stringBytes.Add(Encoding.UTF8.GetBytes(text));
            return (uint)(doc.strings.Count - 1);
        }

        /// RSData node -> (new namespace, extra child node or -1) for the document's pop bales and pop bale triggers. The bales get a
        /// LinkedTriggerID String field (a copy of their Name field's records, appended to the document's nodes / attributes).
        static Dictionary<int, (string ns, int extra)> PopBaleEdits(Bxml doc)
        {
            var edits = new Dictionary<int, (string, int)>();
            int count = doc.nodes.Count;
            for (int entity = 0; entity < count; entity++)
            {
                if (doc.Tag(entity) != "Entity") continue;
                int rs = doc.Children(entity).First();
                var f = Fields(doc.AsRecord(entity));
                string kind = PopBaleKind(f);
                if (kind != null)
                {
                    int nameField = doc.Children(rs).First(c => doc.AttributeAt(c, "name") as string == "Name");
                    int stringNode = doc.Children(nameField).First();
                    var add = new List<BxAttr>();
                    foreach (int a in doc.AttrIndices(nameField))
                    {
                        var at = doc.attrs[a];
                        if (doc.strings[(int)at.name] == "name") add.Add(new BxAttr { name = at.name, refv = Intern(doc, "LinkedTriggerID"), count = at.count, kind = at.kind });
                        else add.Add(at);
                    }
                    int fieldIndex = doc.nodes.Count, attrStart = doc.attrs.Count;
                    doc.attrs.AddRange(add);
                    var field = doc.nodes[nameField].Clone(); field.childStart = (uint)(fieldIndex + 1); field.childCount = 1; field.attrStart = (uint)attrStart; field.attrCount = (uint)add.Count;
                    doc.nodes.Add(field);
                    var str = doc.nodes[stringNode].Clone(); str.refv = Intern(doc, PopBaleTrigger);
                    doc.nodes.Add(str);
                    edits[rs] = (PopBaleKinds[kind], fieldIndex);
                }
                else if (IsPopBaleTrigger(f)) edits[rs] = (TriggerNs, -1);
            }
            return edits;
        }

        public sealed class FreeRide
        {
            public byte[] scene; public List<(object id, string ns, string reason)> deferred = new List<(object, string, string)>();
            public int collision, starts, entities;
        }

        /// alive_install.free_ride_scene; with `raceSection` also that race subscene ("<level>Exhibition<Type>", AliveRace: PORT ADDITION) holding
        /// the start gate, which only the race loads.
        public static FreeRide FreeRideScene(byte[] source, string level, HashSet<(string, string)> missing, string raceSection = null)
        {
            uint magic = Bin.U32LE(source, 0), count = Bin.U32LE(source, 4);
            Bin.Require(magic == 0xc0fffeee && count >= 1, "Alive scene container");
            var names = new List<string>();
            for (int i = 0; i < count; i++)
            {
                int p = 8 + i * 264; int z = Array.IndexOf(source, (byte)0, p, 256); if (z < 0) z = p + 256;
                names.Add(Encoding.UTF8.GetString(source, p, z - p));
            }
            string baseName = level + "Base";
            Bin.Require(names.Contains(baseName), "Scene has no Base section");
            int index = names.IndexOf(baseName);
            int offset = (int)Bin.U32LE(source, 8 + index * 264 + 260);
            int chunksN = (int)Bin.U32LE(source, offset);
            var original = ReadSections(source, new[] { baseName })[baseName].entities;
            var result = new FreeRide();
            var deferredIds = new HashSet<string>();
            foreach (var r in original)
            {
                var f = Fields(r);
                string why = IsPopBaleTrigger(f) ? null : Deferral(f, missing);
                if (why != null) { result.deferred.Add((f.entityId, f.ns, why)); deferredIds.Add(Convert.ToString(f.entityId, System.Globalization.CultureInfo.InvariantCulture)); }
            }
            var gateIds = new HashSet<string>();
            if (raceSection != null)
            {
                foreach (var r in original)
                {
                    var f = Fields(r);
                    if (f.FirstValue("Name") as string == AliveRace.Gate) gateIds.Add(Convert.ToString(f.entityId, System.Globalization.CultureInfo.InvariantCulture));
                }
                Bin.Require(gateIds.Count == 1, "The level has no single start gate (" + AliveRace.Gate + ")");
            }
            // the race activity's pop bale trigger when it is not in Base (the race subscene takes it: only the race loads it)
            (string name, string id)? triggerSection = null;
            if (raceSection != null)
                foreach (var name in names)
                {
                    if (name == baseName || name == level + "FreeRide") continue;
                    foreach (var r in ReadSections(source, new[] { name })[name].entities)
                    {
                        var f = Fields(r);
                        if (IsPopBaleTrigger(f) && triggerSection == null) triggerSection = (name, Convert.ToString(f.entityId, System.Globalization.CultureInfo.InvariantCulture));
                    }
                }
            // the section's chunks at `at`, its entities filtered by keep(entity id, type) and the pop bale edits applied
            int ChunksOf(int at, int count, Func<string, string, bool> keep, List<byte[]> chunks)
            {
                int cursor = at + 8, total = 0;
                for (int c = 0; c < count; c++)
                {
                    int size = 36 + (int)Bin.U32LE(source, cursor + 32);
                    var doc = Bxml.Parse(Bin.Slice(source, cursor, size), scene: true); cursor += size;
                    var edits = PopBaleEdits(doc);
                    var nodes = new List<BxNode> { null }; var attrs = new List<BxAttr>();
                    void Build(int old, int idx)
                    {
                        var node = doc.nodes[old]; var kids = new List<int>();
                        foreach (int child in doc.Children(old))
                        {
                            if (doc.Tag(child) == "Entity")
                            {
                                int rs = doc.Children(child).First();
                                string ns = doc.AttributeAt(rs, "namespace") as string;
                                string typ = ns.Substring(ns.LastIndexOf('_') + 1);
                                string eid = Convert.ToString(doc.AttributeAt(child, "entityId"), System.Globalization.CultureInfo.InvariantCulture);
                                if (!keep(eid, typ)) continue;
                                total++;
                            }
                            kids.Add(child);
                        }
                        bool edited = edits.TryGetValue(old, out var edit);
                        if (edited && edit.extra >= 0) kids.Add(edit.extra);
                        int a = attrs.Count;
                        foreach (int i in doc.AttrIndices(old))
                        {
                            var at = doc.attrs[i];
                            if (edited && doc.strings[(int)at.name] == "namespace") attrs.Add(new BxAttr { name = at.name, refv = Intern(doc, edit.ns), count = at.count, kind = at.kind });
                            else attrs.Add(at);
                        }
                        int startIdx = nodes.Count; for (int k = 0; k < kids.Count; k++) nodes.Add(null);
                        var n2 = node.Clone(); n2.childStart = (uint)startIdx; n2.childCount = (uint)kids.Count; n2.attrStart = (uint)a;
                        nodes[idx] = n2;
                        for (int j = 0; j < kids.Count; j++) Build(kids[j], startIdx + j);
                    }
                    Build(0, 0);
                    doc.nodes = nodes; doc.attrs = attrs;
                    chunks.Add(doc.Serialize());
                }
                return total;
            }
            // starts: true the Free Ride starts, false Base, null the race subscene (the gate, and the race's pop bale trigger)
            byte[] Section(bool? starts)
            {
                var chunks = new List<byte[]>();
                int total = ChunksOf(offset, chunksN, (eid, typ) => starts == null ? gateIds.Contains(eid)
                                                                     : !deferredIds.Contains(eid) && (typ == "StartEntityFreeRideRSData") == starts.Value, chunks);
                if (starts == null && triggerSection != null)
                {
                    int at = (int)Bin.U32LE(source, 8 + names.IndexOf(triggerSection.Value.name) * 264 + 260);
                    string tid = triggerSection.Value.id;
                    total += ChunksOf(at, (int)Bin.U32LE(source, at), (eid, typ) => eid == tid, chunks);
                }
                var o = new System.IO.MemoryStream();
                var hdr = new byte[8]; Bin.PutU32LE(hdr, 0, (uint)chunks.Count); Bin.PutU32LE(hdr, 4, (uint)total); o.Write(hdr, 0, 8);
                foreach (var ch in chunks) o.Write(ch, 0, ch.Length);
                return o.ToArray();
            }
            var parts = new List<(string name, byte[] data)> { (baseName, Section(false)), (level + "FreeRide", Section(true)) };
            if (raceSection != null) parts.Add((raceSection, Section(null)));
            var header = new byte[8 + parts.Count * 264];
            Bin.PutU32LE(header, 0, 0xc0fffeee); Bin.PutU32LE(header, 4, (uint)parts.Count);
            void Entry(int at, string name, int off) { var nb = Encoding.UTF8.GetBytes(name); Buffer.BlockCopy(nb, 0, header, at, Math.Min(256, nb.Length)); Bin.PutU32LE(header, at + 260, (uint)off); }
            int offAt = header.Length;
            for (int i = 0; i < parts.Count; i++) { Entry(8 + 264 * i, parts[i].name, offAt); offAt += parts[i].data.Length; }
            var all = new byte[offAt];
            Buffer.BlockCopy(header, 0, all, 0, header.Length);
            offAt = header.Length;
            foreach (var part in parts) { Buffer.BlockCopy(part.data, 0, all, offAt, part.data.Length); offAt += part.data.Length; }
            var sections = ReadSections(all, new[] { baseName, level + "FreeRide" });
            var output = sections.Values.SelectMany(s => s.entities).Select(Fields).ToList();
            var outRecords = sections.Values.SelectMany(s => s.entities).ToList();
            foreach (var typ in new[] { "CollisionRSData", "StartEntityFreeRideRSData", "HFTerrainRSData" })
            {
                var before = original.Where(r => Fields(r).ns.EndsWith("_" + typ)).Select(Canonical).ToList();
                var after = outRecords.Where(r => Fields(r).ns.EndsWith("_" + typ)).Select(Canonical).ToList();
                Bin.Require(before.SequenceEqual(after) && (before.Count > 0 || typ == "HFTerrainRSData"), typ + " records changed");
            }
            result.starts = output.Count(e => e.ns.EndsWith("_StartEntityFreeRideRSData"));
            Bin.Require(result.starts >= 1, "Level has no Free Ride start");
            result.collision = output.Count(e => e.ns.EndsWith("_CollisionRSData"));
            result.entities = output.Count;
            result.scene = all;
            return result;
        }
    }
}

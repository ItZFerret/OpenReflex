// Reflex level scene container reader (Database/<level>.dx9.scene), the port of _re/formats/scene_read.py.
//   u32 0xc0fffeee, u32 section count, then per section {char[256] name, u32 0, u32 offset} (264 bytes); each section is
//   u32 chunk count, u32 entity count, then the chunks: embedded BXML documents (0x103ea, zlib; size = 36 + the last header
//   word). Every "Entity" node holds one RSData child (attribute `namespace`) whose children are the named fields.
// Entities come back as lightweight records over the BXML tree (BxmlData.Node), so the loaders can ask for the fields they need.
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using UnityEngine;
using Node = Reflex.Port.Content.BxmlData.Node;

namespace Reflex.Port.Content
{
    internal sealed class SceneEntity
    {
        public int id;                 // the Entity node's entityId attribute
        public string ns;              // e.g. RSToolsNS_RSDataNS_ModelRSData
        public string section;
        public Node rsdata;

        /// Short type name: ModelRSData, DropGateRSData ...
        public string Type { get { int i = ns.LastIndexOf('_'); return i < 0 ? ns : ns.Substring(i + 1); } }

        public Node Field(string name) => rsdata.children.Find(c => c.S("name") == name);
        public bool Has(string name) => Field(name) != null;
        /// The first child of a field (String / Float / Matrix / Asset ...).
        public Node Value(string name) { var f = Field(name); return f != null && f.children.Count > 0 ? f.children[0] : null; }
        public string String(string name) => Value(name)?.value as string;
        public float Float(string name, float def = 0) { var v = Value(name)?.value; return v is float f ? f : v is int i ? i : def; }
        public int Int(string name, int def = 0) { var v = Value(name)?.value; return v is int i ? i : v is uint u ? (int)u : v is float f ? (int)f : def; }
        public bool Bool(string name, bool def = false) { var v = Value(name)?.value; return v is bool b ? b : v is int i ? i != 0 : def; }
        public float[] Floats(string name) => Value(name)?.value as float[];
        /// The `name` attribute of an Asset field (ModelAsset, SurfaceAsset, HeightMapAsset ...).
        public string Asset(string name) => Value(name)?.S("name");
        /// The scene matrix (row-major D3D, row vectors: rows 0..2 are the local axes, row 3 the translation).
        public float[] Matrix => Floats("Matrix");
        /// The list of children names under a field, e.g. the MaterialList overrides: first child of each item's `name`.
        public List<string> MaterialList()
        {
            var list = new List<string>();
            var f = Field("MaterialList");
            if (f == null) return list;
            foreach (var m in f.children) if (m.children.Count > 0) list.Add(m.children[0].S("name"));
            return list;
        }
        public override string ToString() => $"{Type}#{id} {String("Name")}";
    }

    internal static class ReflexScene
    {
        /// The section names of a scene file, in table order.
        public static List<string> SectionNames(byte[] data)
        {
            Rfx.Check(Rfx.U32(data, 0) == 0xc0fffeee, "scene container magic");
            int count = Rfx.I32(data, 4); Rfx.Check(count > 0 && count <= 128 && 8 + count * 264 <= data.Length, "scene section count");
            var names = new List<string>();
            for (int i = 0; i < count; i++) names.Add(SectionName(data, 8 + i * 264));
            return names;
        }

        static string SectionName(byte[] data, int at)
        {
            int e = Array.IndexOf(data, (byte)0, at, 256); if (e < 0) e = at + 256;
            return Encoding.ASCII.GetString(data, at, e - at);
        }

        /// All entities of the named sections (a name that is absent is skipped), in file order. Chunks are inflated in parallel.
        public static List<SceneEntity> Read(byte[] data, params string[] sections)
        {
            var names = SectionNames(data);
            int count = names.Count;
            var offsets = new int[count];
            for (int i = 0; i < count; i++) offsets[i] = Rfx.I32(data, 8 + i * 264 + 260);
            var chunks = new List<(string section, int at, int size)>();
            foreach (string wanted in sections)
            {
                int s = names.IndexOf(wanted); if (s < 0) continue;
                int start = offsets[s], end = s + 1 < count ? offsets[s + 1] : data.Length;
                Rfx.Check(start >= 8 + count * 264 && start + 8 <= end && end <= data.Length, "scene section extent");
                int n = Rfx.I32(data, start), p = start + 8;
                Rfx.Check(n > 0 && n <= 4096, "scene chunk count");
                for (int j = 0; j < n; j++)
                {
                    Rfx.Check(p + 36 <= end && Rfx.U32(data, p) == 0x4c4d5842 && Rfx.I32(data, p + 4) == 0x103ea, "embedded scene BXML");
                    int size = 36 + Rfx.I32(data, p + 32);
                    Rfx.Check(size > 36 && p + size <= end, "embedded scene BXML extent");
                    chunks.Add((wanted, p, size)); p += size;
                }
            }
            var parsed = new List<SceneEntity>[chunks.Count];
            Parallel.For(0, chunks.Count, new ParallelOptions { MaxDegreeOfParallelism = 2 }, k =>
            {
                var part = new byte[chunks[k].size];
                Buffer.BlockCopy(data, chunks[k].at, part, 0, part.Length);
                var list = new List<SceneEntity>();
                foreach (var node in Rfx.Bxml(part).All())
                {
                    if (node.tag != "Entity" || node.children.Count != 1 || node.children[0].tag != "RSData") continue;
                    var rs = node.children[0];
                    list.Add(new SceneEntity { id = node.I("entityId"), ns = rs.S("namespace") ?? "", rsdata = rs, section = chunks[k].section });
                }
                parsed[k] = list;
            });
            return parsed.SelectMany(l => l).ToList();
        }
    }
}

// The per-resource converters of _re/formats/alive_convert.py (collision, model, surfaces, zones, materials, the asset dispatch, sky HDR
// footer, rock detail tiling, asset path segments), alive_environment.py (forest, SpeedTree 5 trees, grass, water, particles, bounce
// light) and alive_shader.py (shader resources as PC constant-table programs). Same checks, same output bytes.
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;

namespace Reflex.Port.Content.AliveImport
{
    internal static class Converters
    {
        // ------------------------------------------------------------------------------------- alive_convert

        public static byte[] Collision(byte[] source)
        {
            Bin.Require(source.Length >= 24, "Truncated collision");
            uint magic = Bin.U32BE(source, 0), version = Bin.U32BE(source, 8), kind = Bin.U32BE(source, 12), zero = Bin.U32BE(source, 16);
            Bin.Require(magic == 1234 && Bin.StartsWith(source, "col\0", 4) && version == 3 && zero == 0, "Unsupported collision header");
            Bin.Require(kind == 2 || kind == 11 || kind == 15 || kind == 16 || kind == 17 || kind == 22, $"Collision kind {kind} needs a separate converter");
            var data = (byte[])source.Clone();
            Bin.SwapWords(data, 0, 4);
            Bin.SwapWords(data, 8, 12);
            if (kind == 11 || kind == 16 || kind == 17)
            {
                int size = kind == 11 ? 16 : kind == 16 ? 4 : 8;
                Bin.Require(source.Length == 20 + size, "Collision primitive extent mismatch");
                Bin.SwapWords(data, 20, size);
                return data;
            }
            if (kind == 2)
            {
                Bin.Require(source.Length >= 96, "Truncated kind 2 collision");
                uint n2 = Bin.U32BE(source, 88), n3 = Bin.U32BE(source, 92);
                Bin.Require(n2 <= 65536 && n3 <= 65536 && source.Length == 96 + (long)n2 * 32 + (long)n3 * 24, "Kind 2 collision extent mismatch");
                Bin.SwapWords(data, 20, 76);
                int cur = 96;
                for (int i = 0; i < n2; i++) { Bin.SwapWords(data, cur, 28); Bin.SwapWords(data, cur + 28, 4, 2); cur += 32; }
                Bin.SwapWords(data, cur, (int)n3 * 24);
                return data;
            }
            uint count = Bin.U32BE(source, 20);
            Bin.Require(count <= source.Length / 16, "Invalid collision count");
            Bin.SwapWords(data, 20, 4);
            int cursor = 24;
            if (kind == 22)
            {
                for (int i = 0; i < count; i++)
                {
                    Bin.Require(cursor + 16 <= data.Length, "Truncated collision chunk");
                    long triangles = Bin.U32BE(source, cursor + 12);
                    long size = 16 + triangles * 36;
                    Bin.Require(cursor + size <= data.Length, "Word range outside asset");
                    Bin.SwapWords(data, cursor, (int)size);
                    cursor += (int)size;
                }
            }
            else
            {
                Bin.Require(data.Length >= 28, "Truncated collision name pool");
                long pool = Bin.U32BE(source, cursor);
                Bin.SwapWords(data, cursor, 4);
                Bin.Require(cursor + 4 + pool <= data.Length, "Collision name pool exceeds asset");
                cursor += 4 + (int)pool;
                for (int i = 0; i < count; i++)
                {
                    Bin.Require(cursor + 4 <= data.Length, "Truncated collision shape");
                    uint typ = Bin.U32BE(source, cursor);
                    int size = typ == 5 ? 88 : typ == 7 ? 76 : typ == 8 ? 80 : -1;
                    Bin.Require(size > 0, $"Unsupported collision shape {typ}");
                    Bin.SwapWords(data, cursor, size);
                    cursor += size;
                }
            }
            Bin.Require(cursor == data.Length, "Collision extent mismatch");
            return data;
        }

        public static byte[] Model(byte[] source)
        {
            Bin.Require(source.Length >= 172, "Truncated model");
            var h = new uint[10]; for (int i = 0; i < 10; i++) h[i] = Bin.U32BE(source, 4 * i);
            Bin.Require(h[0] == 0x88aabb00 && h[1] == 4 && (h[2] == 0 || h[2] == 1) && h[9] == 0, "Unsupported model");
            long count = h[3], names = h[4], lods = h[5], mats = h[6], binds = h[7], refs = h[8];
            Bin.Require(count > 0 && count <= 1024 && lods <= 16 && binds <= 4096 && refs <= 4096, "Model counts");
            Bin.Require(172 + count * 108 + names + lods * 12 + binds * 20 + mats + refs * 5 == source.Length, "Model extent mismatch");
            var data = (byte[])source.Clone();
            Bin.SwapWords(data, 0, 40);
            Bin.SwapWords(data, 72, 100 + (int)count * 108);
            long p = 172 + count * 108 + names;
            Bin.SwapWords(data, (int)p, (int)(lods * 12 + binds * 20));
            p += lods * 12 + binds * 20 + mats + refs;
            Bin.SwapWords(data, (int)p, (int)refs * 4);
            return data;
        }

        /// alive_convert.convert_zones: the four interleaved Xbox terrain bitmaps to the PC packdata schema.
        public static byte[] Zones(byte[] source)
        {
            Bin.Require(source.Length >= 36, "Truncated zones BXML");
            uint flags = Bin.U32LE(source, 4), ns = Bin.U32LE(source, 8), ss = Bin.U32LE(source, 12), bs = Bin.U32LE(source, 16), na = Bin.U32LE(source, 20),
                 nn = Bin.U32LE(source, 24), reserved = Bin.U32LE(source, 28), compressed = Bin.U32LE(source, 32);
            Bin.Require(Bin.StartsWith(source, "BXML") && flags == 0x103ea && na == 0 && nn == 10 && reserved == 0 && compressed == source.Length - 36, "Unsupported zones document");
            Bin.Require((long)ss + bs + nn * 32 <= 16 * 1024 * 1024, "Zones document too large");
            byte[] payload;
            try { payload = Bin.Inflate(source, 36, source.Length - 36, (int)(ss + bs + nn * 32)); }
            catch (AliveImportException) { throw new AliveImportException("Zones payload mismatch"); }
            var strings = new List<string>(); int st = 0;
            for (int i = 0; i < ss; i++) if (payload[i] == 0) { strings.Add(Encoding.UTF8.GetString(payload, st, i - st)); st = i + 1; }
            Bin.Require(strings.Count == ns, "Zones string count");
            var values = new Dictionary<string, uint[]>();
            for (int i = 0; i < nn; i++)
            {
                int p = (int)(ss + bs) + i * 32;
                uint name = Bin.U32LE(payload, p), refv = Bin.U32LE(payload, p + 4), count = Bin.U32LE(payload, p + 8), kind = Bin.U32LE(payload, p + 12),
                     child = Bin.U32LE(payload, p + 16), children = Bin.U32LE(payload, p + 20), attrs = Bin.U32LE(payload, p + 28);
                Bin.Require(name < ns && attrs == 0, "Zones node bounds");
                if (i == 0) { Bin.Require(kind == 0 && child == 1 && children == 9, "Zones root"); continue; }
                Bin.Require((kind == 4 || kind == 11) && children == 0 && refv + (long)count * 4 <= bs, "Zones value bounds");
                var v = new uint[count]; for (int k = 0; k < count; k++) v[k] = Bin.U32LE(payload, (int)(ss + refv) + 4 * k);
                values[strings[(int)name]] = v;
            }
            var expected = new HashSet<string> { "Interleaved", "Width", "Height", "NumBits1", "NumBits2", "NumBits3", "NumBits4", "NumWords", "Data" };
            Bin.Require(expected.SetEquals(values.Keys), "Unexpected zones fields");
            Bin.Require(values["Interleaved"].Length == 1 && values["Interleaved"][0] == 1, "Zones are not interleaved");
            Bin.Require(values["Width"].Length > 0 && values["Height"].Length > 0, "Zones dimensions/bits");
            long width = values["Width"][0], height = values["Height"][0];
            var bits = new int[4];
            for (int i = 0; i < 4; i++) { Bin.Require(values["NumBits" + (i + 1)].Length > 0, "Zones dimensions/bits"); bits[i] = (int)values["NumBits" + (i + 1)][0]; }
            Bin.Require(width >= 1 && width <= 4096 && height >= 1 && height <= 4096 && bits.All(n => n >= 1 && n <= 8), "Zones dimensions/bits");
            var data = values["Data"];
            int sum = bits.Sum();
            Bin.Require(values["NumWords"].Length > 0 && data.Length == values["NumWords"][0] && (long)data.Length * 32 >= width * height * sum, "Zones data size");
            var d = new ulong[data.Length + 1]; for (int i = 0; i < data.Length; i++) d[i] = data[i];
            long cells = width * height;
            var output = new List<(string name, int kind, uint[] v)> { ("Interleaved", 11, new uint[] { 0 }) };
            int shift = 0;
            for (int i = 1; i <= 4; i++)
            {
                int n = bits[i - 1]; ulong mask = (1UL << n) - 1;
                var words = new ulong[(cells * n + 31) / 32 + 1];
                for (long pos = 0; pos < cells; pos++)
                {
                    long at = pos * sum + shift;
                    int sh = (int)(at % 32);
                    ulong sample = ((d[at / 32] >> sh) | (sh == 0 ? d[at / 32 + 1] << 32 : d[at / 32 + 1] << (32 - sh))) & mask;
                    long dest = pos * n; int ds = (int)(dest % 32);
                    words[dest / 32] |= (sample << ds) & 0xffffffffUL;
                    words[dest / 32 + 1] |= ds == 0 ? sample >> 32 : sample >> (32 - ds);
                }
                output.Add(("Width" + i, 4, new[] { (uint)width }));
                output.Add(("Height" + i, 4, new[] { (uint)height }));
                output.Add(("NumBits" + i, 4, new[] { (uint)n }));
                output.Add(("NumWords" + i, 4, new[] { (uint)words.Length }));
                output.Add(("Data" + i, 4, words.Select(w => (uint)w).ToArray()));
                shift += n;
            }
            var pool = new MemoryStream();
            void Str(string s) { var b = Encoding.UTF8.GetBytes(s); pool.Write(b, 0, b.Length); pool.WriteByte(0); }
            Str("Root"); foreach (var o in output) Str(o.name);
            var blob = new MemoryStream(); var w4 = new byte[32];
            foreach (var o in output) foreach (var v in o.v) { Bin.PutU32LE(w4, 0, v); blob.Write(w4, 0, 4); }
            var nodes = new MemoryStream();
            void Node(uint a, uint b, uint c, uint k, uint e, uint f, uint g, uint h2)
            {
                Bin.PutU32LE(w4, 0, a); Bin.PutU32LE(w4, 4, b); Bin.PutU32LE(w4, 8, c); Bin.PutU32LE(w4, 12, k);
                Bin.PutU32LE(w4, 16, e); Bin.PutU32LE(w4, 20, f); Bin.PutU32LE(w4, 24, g); Bin.PutU32LE(w4, 28, h2); nodes.Write(w4, 0, 32);
            }
            Node(0, 0xffffffff, 0, 0, 1, (uint)output.Count, 0, 0);
            uint off = 0;
            for (int i = 0; i < output.Count; i++) { Node((uint)(i + 1), off, (uint)output[i].v.Length, (uint)output[i].kind, 0, 0, 0, 0); off += (uint)output[i].v.Length * 4; }
            var payloadOut = Cat(pool.ToArray(), blob.ToArray(), nodes.ToArray());
            var z = Bin.ZlibStored(payloadOut);
            return Cat(Bxml.Header(output.Count + 1, (int)pool.Length, (int)blob.Length, 0, output.Count + 1, z.Length), z);
        }

        public static byte[] Cat(params byte[][] parts)
        {
            long n = 0; foreach (var p in parts) n += p.Length;
            var r = new byte[n]; int at = 0;
            foreach (var p in parts) { Buffer.BlockCopy(p, 0, r, at, p.Length); at += p.Length; }
            return r;
        }

        static readonly Dictionary<uint, (int outType, int size)> SurfaceFormats = new Dictionary<uint, (int, int)>
        {
            { 0x1a2360, (16, 8) }, { 0x2a2187, (2, 12) }, { 0x2c235f, (15, 4) }, { 0x182886, (4, 4) }, { 0x1a2286, (5, 4) }, { 0x1a2086, (8, 4) },
        };

        /// alive_convert.convert_surfaces: Xbox vertex declarations expanded into the client's PC layout.
        public static byte[] Surfaces(byte[] source)
        {
            Bin.Require(source.Length >= 16, "Truncated surface");
            uint magic = Bin.U32BE(source, 0), version = Bin.U32BE(source, 4), count = Bin.U32BE(source, 8);
            Bin.Require(magic == 0x99bbcc00 && version == 4 && count > 0 && count <= 4096, "Unsupported surface");
            long cursor = 12 + (long)count * 352;
            Bin.Require(cursor + 4 <= source.Length && Bin.U32BE(source, (int)cursor) == 0, "Invalid surface data start");
            cursor += 4;
            var headers = new List<byte[]>(); var buffers = new List<byte[]>();
            for (int i = 0; i < count; i++)
            {
                int q = 12 + i * 352;
                var words = new uint[88]; for (int k = 0; k < 88; k++) words[k] = Bin.U32BE(source, q + 4 * k);
                uint flags = words[3], prims = words[4], nv = words[5], ib = words[6], vb = words[68], palette = words[84];
                Bin.Require(words[0] == 0 && words[1] == 0 && words[2] == 0xffffffff && (flags == 0 || flags == 0x30004), "Unsupported surface flags");
                Bin.Require(nv > 0 && nv <= 65535 && ib == ((long)prims + 2) * 2 && vb > 0 && vb % nv == 0 && palette <= 256, "Invalid surface buffers");
                Bin.Require(cursor + palette + ib + vb <= source.Length, "Truncated surface payload");
                int stride = (int)(vb / nv);
                var decls = new List<(int offset, uint typ, int outoff, int size)>(); int outstride = 0; var uvsets = new Dictionary<int, int>();
                var header = new byte[352];
                for (int k = 0; k < 7; k++) Bin.PutU32LE(header, 4 * k, words[k]);
                Bin.PutU32LE(header, 12, flags == 0x30004 ? 0x40003u : 0u);
                bool terminated = false;
                for (int d = 0; d < 20; d++)
                {
                    int at = q + 28 + d * 12;
                    int stream = Bin.U16BE(source, at), offset = Bin.U16BE(source, at + 2); uint typ = Bin.U32BE(source, at + 4);
                    int method = source[at + 8], usage = source[at + 9], index = source[at + 10], zero = source[at + 11];
                    if (stream == 255)
                    {
                        PutDecl(header, 28 + d * 8, 255, 0, 17, 0, 0, 0);
                        terminated = true; break;
                    }
                    Bin.Require(stream == 0 && method == zero && zero == 0 && SurfaceFormats.ContainsKey(typ), "Unsupported Xbox vertex declaration/stream");
                    var (outtype, size) = SurfaceFormats[typ];
                    int inputsize = typ == 0x1a2360 ? 8 : 4;
                    Bin.Require(offset + inputsize <= stride, "Vertex element exceeds source stride");
                    PutDecl(header, 28 + d * 8, 0, outstride, outtype, 0, usage, index);
                    decls.Add((offset, typ, outstride, size));
                    if (usage == 5 && typ == 0x2c235f) uvsets[index] = outstride;
                    outstride += size;
                }
                Bin.Require(terminated, "Unterminated vertex declaration");
                Bin.PutU32LE(header, 188, 1); Bin.PutU32LE(header, 192, (uint)(nv * outstride));
                Bin.PutU32LE(header, 256, palette);
                var pal = Bin.Slice(source, (int)cursor, (int)palette);
                cursor += palette;
                var idx = new byte[ib];
                for (int k = 0; k < ib / 2; k++)
                {
                    int v = Bin.U16BE(source, (int)cursor + 2 * k);
                    Bin.Require(v < nv, "Surface index outside vertices");
                    idx[2 * k] = (byte)v; idx[2 * k + 1] = (byte)(v >> 8);
                }
                cursor += ib;
                var vertices = new byte[nv * outstride];
                foreach (var (offset, typ, outoff, size) in decls)
                    for (int v = 0; v < nv; v++)
                    {
                        int at = (int)cursor + v * stride + offset, dest = v * outstride + outoff;
                        if (typ == 0x2a2187)
                        {
                            uint word = Bin.U32BE(source, at);
                            for (int k = 0; k < 3; k++)
                            {
                                int x = (int)((word >> (10 * k)) & 1023);
                                double f = Math.Max(-1.0, ((x & 512) != 0 ? x - 1024 : x) / 511.0);
                                Bin.PutF32LE(vertices, dest + 4 * k, (float)f);
                            }
                        }
                        else if (typ == 0x1a2360 || typ == 0x2c235f)
                        {
                            if (typ == 0x1a2360)
                            {
                                for (int k = 0; k < 3; k++) { float c = Bin.Half(Bin.U16BE(source, at + 2 * k)); Bin.Require(!float.IsNaN(c) && !float.IsInfinity(c), "Source contains invalid half-float position"); }
                                Bin.Require(Bin.Half(Bin.U16BE(source, at + 6)) == 1f, "Source contains invalid half-float position");
                            }
                            for (int k = 0; k < size; k += 2) { vertices[dest + k] = source[at + k + 1]; vertices[dest + k + 1] = source[at + k]; }
                        }
                        else
                            for (int k = 0; k < 4; k++) vertices[dest + k] = source[at + 3 - k];
                    }
                if (uvsets.TryGetValue(0, out int uv0) && uvsets.TryGetValue(1, out int uv1))
                {
                    bool any = false;
                    for (int v = 0; v < nv && !any; v++) for (int k = 0; k < 4; k++) if (vertices[v * outstride + uv1 + k] != 0) { any = true; break; }
                    if (!any) for (int v = 0; v < nv; v++) Buffer.BlockCopy(vertices, v * outstride + uv0, vertices, v * outstride + uv1, 4);
                }
                cursor += vb;
                headers.Add(header);
                buffers.Add(Cat(pal, idx, vertices));
            }
            Bin.Require(cursor == source.Length, "Unexpected surface trailing data");
            var h = new byte[12]; Bin.PutU32LE(h, 0, magic); Bin.PutU32LE(h, 4, version); Bin.PutU32LE(h, 8, count);
            var parts = new List<byte[]> { h }; parts.AddRange(headers); parts.Add(new byte[4]); parts.AddRange(buffers);
            return Cat(parts.ToArray());
        }

        static void PutDecl(byte[] b, int at, int stream, int offset, int type, int method, int usage, int index)
        {
            Bin.PutU16LE(b, at, (ushort)stream); Bin.PutU16LE(b, at + 2, (ushort)offset);
            b[at + 4] = (byte)type; b[at + 5] = (byte)method; b[at + 6] = (byte)usage; b[at + 7] = (byte)index;
        }

        /// alive_convert.segment (ReflexAssetImport.Segment / fmt_db.c), including encoded periods.
        public static string Segment(string name)
        {
            Bin.Require(!string.IsNullOrEmpty(name) && name != "." && name != ".." && !name.Contains("/") && !name.Contains("\\"), "Invalid asset identifier");
            var sb = new StringBuilder();
            foreach (var b in Encoding.UTF8.GetBytes(name))
            {
                if (b >= 'A' && b <= 'Z' || b >= 'a' && b <= 'z' || b >= '0' && b <= '9' || b == '_' || b == '-') sb.Append((char)b);
                else sb.Append('%').Append(b.ToString("X2"));
            }
            string result = sb.ToString(), upper = name.ToUpperInvariant();
            bool device = upper == "CON" || upper == "PRN" || upper == "AUX" || upper == "NUL" ||
                          upper.Length == 4 && (upper.StartsWith("COM", StringComparison.Ordinal) || upper.StartsWith("LPT", StringComparison.Ordinal)) && upper[3] >= '1' && upper[3] <= '9';
            if (device) result = "%" + ((int)name[0]).ToString("X2") + result.Substring(1);
            return result;
        }

        static readonly Dictionary<uint, uint> RenderStates = new Dictionary<uint, uint>
            { { 40, 7 }, { 44, 23 }, { 48, 14 }, { 56, 22 }, { 60, 27 }, { 72, 19 }, { 76, 20 }, { 80, 171 }, { 96, 15 }, { 100, 24 }, { 104, 25 } };
        static readonly Dictionary<uint, uint> SamplerStates = new Dictionary<uint, uint> { { 0, 1 }, { 4, 2 }, { 8, 3 }, { 16, 5 }, { 20, 6 }, { 24, 7 }, { 28, 8 } };

        public static byte[] Material(byte[] data)
        {
            var doc = Bxml.Parse(data, scene: true);
            Bin.Require(doc.Tag(0) == "MaterialEditorMaterial", "Material root");
            var blob = (byte[])doc.blob.Clone();
            foreach (var node in doc.nodes)
            {
                if (node.kind != 2) continue;
                string tag = doc.strings[(int)node.name];
                Bin.Require((tag == "XenonStates" || tag == "XenonSamplerStates") && node.count % 8 == 0, "Unknown material binary field");
                for (long offset = node.refv; offset < node.refv + (long)node.count; offset += 8)
                {
                    uint key = Bin.U32BE(doc.blob, (int)offset), value = Bin.U32BE(doc.blob, (int)offset + 4);
                    var mapping = tag == "XenonSamplerStates" ? SamplerStates : RenderStates;
                    Bin.Require(mapping.ContainsKey(key), $"Unknown {tag} state {key}");
                    if (tag == "XenonSamplerStates" && key != 28) value += 1;
                    else if (tag == "XenonStates")
                    {
                        if (key == 56) { Bin.Require(value == 4 || value == 6, "Unknown Xbox cull mode"); value -= 3; }
                        else if (key == 44 || key == 80 || key == 104) value += 1;
                        else if (key == 72 || key == 76) { Bin.Require(value == 6 || value == 7, "Unknown Xbox blend factor"); value -= 1; }
                    }
                    Bin.PutU32LE(blob, (int)offset, mapping[key]); Bin.PutU32LE(blob, (int)offset + 4, value);
                }
            }
            doc.blob = blob;
            return doc.Serialize();
        }

        static readonly byte[] SharedPS = Encoding.ASCII.GetBytes("SharedPixelShader"), NormalPS = Encoding.ASCII.GetBytes("NormalPixelShader");

        /// alive_convert.rock_detail_tile: the detail UV scale literal of an Alive rock program, or null.
        public static double? RockDetailTile(byte[] data)
        {
            int start = Bin.IndexOf(data, SharedPS);
            int end = start < 0 ? -1 : Bin.IndexOf(data, NormalPS, start);
            Bin.Require(start > 0 && end > start, "Rock program entry points");
            var key = new byte[8]; Bin.PutU32BE(key, 0, (uint)BitConverter.SingleToInt32Bits(3.0f)); Bin.PutU32BE(key, 4, (uint)BitConverter.SingleToInt32Bits(0.005f));
            int row = Bin.IndexOf(data, key, start, end);
            if (row < 0) return null;
            Bin.Require(row + 32 <= data.Length, "Rock program literal row");
            var tiles = new List<double>();
            for (int k = 4; k < 8; k++) { double v = Bin.F32BE(data, row + 4 * k); if (v == 8.0 || v == 10.0) tiles.Add(v); }
            return tiles.Count == 1 ? tiles[0] : (double?)null;
        }

        /// alive_convert.sky_hdr_footer: the HDRComposite attributes of a sky texture's BXML footer, or null.
        public static List<KeyValuePair<string, object>> SkyHdrFooter(byte[] data)
        {
            int footer = Bin.LastIndexOf(data, Encoding.ASCII.GetBytes("BXML"));
            if (footer <= 0) return null;
            Record hdr;
            try
            {
                var rec = Bxml.Parse(Bin.Slice(data, footer, data.Length - footer), scene: true).AsRecord();
                if (rec.children.Count == 0) return null;
                hdr = rec.children[0];
            }
            catch (Exception) { return null; }
            if (hdr.name != "HDRComposite") return null;
            var type = hdr.Attr("type");
            Bin.Require((type is long t && t == 1) || (type is double d && d == 1), "Sky HDR footer schema");
            return hdr.attributes;
        }

        public static readonly HashSet<string> EnvironmentKinds = new HashSet<string> { "forest", "tree", "grass", "water", "particle", "bounce", "anim" };

        /// alive_convert.convert_asset: (converted bytes, method). `materials`: the level's material names (forest conversion).
        public static (byte[] data, string method) ConvertAsset(byte[] data, string kind, HashSet<string> materials)
        {
            switch (kind)
            {
                case "forest": return (Forest(data, materials), "alive-environment-to-pc");
                case "tree": return (Tree(data), "alive-environment-to-pc");
                case "grass": return (Grass(data), "alive-environment-to-pc");
                case "water": return (Water(data), "alive-environment-to-pc");
                case "particle": return (Particle(data), "alive-environment-to-pc");
                case "bounce": return (Bounce(data), "alive-environment-to-pc");
                case "anim": return (Anim(data), "alive-environment-to-pc");
                case "material": return (Material(data), "xenos-material-states-to-d3d9");
                case "colorlut": return (Textures.ConvertColorLut(data), "tiled-lut-volume-to-rgba");
                case "shader": return (Shaders.ToPcResource(data), "xenos-program-constant-tables-to-d3d9-ctab");
                case "collis": return (Collision(data), "collision-endian");
                case "model": return (Model(data), "model-endian");
                case "texture": return Textures.Convert(data);
                case "packdata": return (Zones(data), "deinterleaved-terrain-zones");
                case "surface": return (Surfaces(data), "xenos-to-dx9-vertices");
                case "tdf": return (AliveRace.ConvertTdf(data), "tdf-size-word-to-pc");
            }
            if (Bin.StartsWith(data, "BXML"))
            {
                var doc = Bxml.Parse(data, scene: true);
                Bin.Require(!doc.nodes.Any(x => x.kind == 2) && !doc.attrs.Any(x => x.kind == 2), "BXML contains platform-dependent opaque data");
                return (data, "validated-shared-bxml");
            }
            throw new AliveImportException($"No validated {kind} conversion yet");
        }

        // ------------------------------------------------------------------------------------- alive_environment

        public static byte[] Forest(byte[] source, HashSet<string> materials)
        {
            Bin.Require(source.Length >= 36, "Forest BXML header");
            Bin.Require(Bin.StartsWith(source, "BXML") && Bin.U32LE(source, 4) == 0x103ea, "Forest BXML header");
            long end = 36 + (long)Bin.U32LE(source, 32);
            Bin.Require(Bin.U32LE(source, 28) == end && end + 2 <= source.Length && source[end] == 'B' && source[end + 1] == 'M', "Forest editor thumbnail boundary");
            long tables = (long)Bin.U32LE(source, 12) + Bin.U32LE(source, 16) + (long)Bin.U32LE(source, 20) * 12 + (long)Bin.U32LE(source, 24) * 32;
            var payload = Bin.InflateAll(source, 36, (int)end - 36);
            Bin.Require(payload.Length == tables, "Forest table lengths");
            var raw = Bin.Slice(source, 0, (int)end); Bin.PutU32LE(raw, 28, 0);
            var doc = Bxml.Parse(raw, scene: true, float2: 1);
            int Str(string v)
            {
                int i = doc.strings.IndexOf(v);
                if (i < 0) { doc.strings.Add(v); doc.stringBytes.Add(Encoding.UTF8.GetBytes(v)); i = doc.strings.Count - 1; }
                return i;
            }
            var attrs = new List<BxAttr>();
            for (int i = 0; i < doc.nodes.Count; i++)
            {
                var node = doc.nodes[i];
                var current = doc.AttrIndices(i).Select(j => doc.attrs[j]).ToList();
                if (doc.strings[(int)node.name] == "Type")
                {
                    var at = doc.AttributesAt(i);
                    Bin.Require(at.Any(kv => kv.Key == "TreeAsset"), "Forest type without TreeAsset");
                    string name = Convert.ToString(at.First(kv => kv.Key == "TreeAsset").Value, System.Globalization.CultureInfo.InvariantCulture);
                    Bin.Require(materials != null, "Forest conversion needs the level material list");
                    foreach (var (key, suffix) in new[] { ("BranchMaterial", "_Br0"), ("FrondMaterial", "_Fr0"), ("LeafMaterial", "_Lf0") })
                        if (materials.Contains(name + suffix))
                        {
                            int k = Str(key), v = Str(name + suffix);
                            current.Add(new BxAttr { name = (uint)k, refv = (uint)v, count = 0, kind = 1 });
                        }
                }
                var n2 = node.Clone(); n2.attrStart = (uint)attrs.Count; n2.attrCount = (uint)current.Count;
                doc.nodes[i] = n2; attrs.AddRange(current);
            }
            doc.attrs = attrs;
            return doc.Serialize();
        }

        /// alive_environment.tree_materials: the SpeedTree 5 Materials records of a raw tree XML: LodParams tag -> per LOD the surface
        /// materials.
        public static Dictionary<string, List<List<string>>> TreeMaterials(byte[] source)
        {
            uint ns = Bin.U32LE(source, 8), ss = Bin.U32LE(source, 12), bs = Bin.U32LE(source, 16), na = Bin.U32LE(source, 20), nn = Bin.U32LE(source, 24);
            var strings = new List<string>(); int st = 32;
            for (int i = 32; i < 32 + ss; i++) if (source[i] == 0) { strings.Add(Encoding.UTF8.GetString(source, st, i - st)); st = i + 1; }
            int ap = 32 + (int)ss + (int)bs, np = ap + (int)na * 12;
            var o = new Dictionary<string, List<List<string>>>();
            for (int i = 0; i < nn; i++)
            {
                int p = np + i * 32;
                string tag = strings[(int)Bin.U32LE(source, p)];
                uint cs = Bin.U32LE(source, p + 16), cc = Bin.U32LE(source, p + 20);
                if (!tag.EndsWith("LodParams", StringComparison.Ordinal) || cc < 1) continue;
                int m = np + (int)cs * 32;
                Bin.Require(strings[(int)Bin.U32LE(source, m)] == "Materials", "Tree LodParams materials");
                uint ms = Bin.U32LE(source, m + 16), mc = Bin.U32LE(source, m + 20);
                var lods = new List<List<string>>();
                for (uint c = 0; c < mc; c++)
                {
                    int l = np + (int)(ms + c) * 32;
                    uint a0 = Bin.U32LE(source, l + 24), ac = Bin.U32LE(source, l + 28);
                    var names = new List<string>();
                    for (uint a = a0; a < a0 + ac; a++)
                    {
                        int q = ap + (int)a * 12;
                        if (strings[(int)Bin.U32LE(source, q)] == "surfaceMaterial" && Bin.U16LE(source, q + 10) == 1) names.Add(strings[(int)Bin.U32LE(source, q + 4)]);
                    }
                    lods.Add(names);
                }
                o[tag] = lods;
            }
            return o;
        }

        /// Alive's geometry blocks (branches, fronds, 3D leaves, leaf cards): the LodParams that name their surfaces, and the PC block
        static readonly string[] TreeBlockParams = { "BranchLodParams", "FrondLodParams", "3DLeafLodParams", "LeafLodParams" };
        static readonly int[] TreePcBlock = { 0, 1, 3, 2 };

        /// alive_environment.tree_xml: the SpeedTree 5 records as the Reflex (SpeedTree 4 transition model) tree XML; `surfaces`: (PC block,
        /// LOD, material, start, count) of a tree whose LODs draw more than one material (the Surfaces node the client reads).
        public static byte[] TreeXml(byte[] source, int leafLods, List<(int block, int lod, string material, int start, int count)> surfaces = null)
        {
            Bin.Require(source.Length >= 32 && Bin.StartsWith(source, "BXML") && Bin.U32LE(source, 4) == 0x3ea, "Raw tree XML");
            uint ns = Bin.U32LE(source, 8), ss = Bin.U32LE(source, 12), bs = Bin.U32LE(source, 16), na = Bin.U32LE(source, 20), nn = Bin.U32LE(source, 24), res = Bin.U32LE(source, 28);
            Bin.Require(res == 0 && source.Length == 32 + (long)ss + bs + (long)na * 12 + (long)nn * 32, "Tree XML bounds");
            var strings = new List<string>(); int st = 32;
            for (int i = 32; i < 32 + ss; i++) if (source[i] == 0) { strings.Add(Encoding.UTF8.GetString(source, st, i - st)); st = i + 1; }
            Bin.Require(strings.Count == ns, "Tree XML strings");
            int blob = 32 + (int)ss, ap = blob + (int)bs, np = ap + (int)na * 12;
            Dictionary<string, double[]> Values(string tag)
            {
                for (int i = 0; i < nn; i++)
                {
                    int p = np + i * 32;
                    if (strings[(int)Bin.U32LE(source, p)] != tag) continue;
                    uint a0 = Bin.U32LE(source, p + 24), ac = Bin.U32LE(source, p + 28);
                    var o = new Dictionary<string, double[]>();
                    for (uint a = a0; a < a0 + ac; a++)
                    {
                        int q = ap + (int)a * 12;
                        string name = strings[(int)Bin.U32LE(source, q)]; int r = blob + (int)Bin.U32LE(source, q + 4); int kind = Bin.U16LE(source, q + 10);
                        double[] v;
                        switch (kind)
                        {
                            case 3: v = new double[] { Bin.I32LE(source, r) }; break;
                            case 4: v = new double[] { Bin.U32LE(source, r) }; break;
                            case 5: v = new double[] { Bin.F32LE(source, r) }; break;
                            case 10: v = new double[] { Bin.F32LE(source, r), Bin.F32LE(source, r + 4), Bin.F32LE(source, r + 8) }; break;
                            default: throw new AliveImportException("Tree XML attribute kind " + kind);
                        }
                        o[name] = v;
                    }
                    return o;
                }
                throw new AliveImportException("Tree XML without " + tag);
            }
            var speed = Values("SpeedTree5"); var bounds = Values("BoundingBox");
            double near = speed["highLodDistance"][0], far = speed["bbEndDistance"][0], bb = speed["bbStartDistance"][0];
            // a tree without leaf cards (branches and fronds only) has no leaf LODs: its leaf transition values are unused, it takes the
            // one-LOD values
            leafLods = Math.Max(1, leafLods);
            Bin.Require(near <= bb && bb < far && leafLods <= 5, "Tree transition distances");
            double radius = (far - bb) / (far - near) / 1.4, fade = (far - bb) / (far - near), cycle = 1.0 / leafLods;
            string F(double v) => "_float:" + PyFormat.Repr(v);
            var root = new Elem("Root");
            var lod = new Elem("Lods"); root.children.Add(lod);
            foreach (var (k, v) in new[] {
                ("near", F(near)), ("far", F(far)), ("targetAlphaValue", "_float:84"), ("transitionFactor", F(.3)), ("leafLodTransitionRadius", F(radius)),
                ("leafLodCurveExponent", F(1.6)), ("leafTransitionFactor", F(.3)), ("crestWidthBB", "_float:1"), ("crestWidth", "_float:1"),
                ("cycleLengthBB", F(cycle)), ("cycleLength", F(cycle)), ("transitionWidth", F(.1)), ("branchFadeDistance", F(fade)),
                ("frondFadeDistance", F(fade)), ("transitionPercent", F(.15)), ("leafLodMethod", "_uint:1"), ("dropToBillboard", "_uint:1") })
                lod.Set(k, EVal.S(v));
            var box = new Elem("BoundingBox"); root.children.Add(box);
            foreach (var k in new[] { "center", "half" }) box.Set(k, EVal.S("_vector3:" + string.Join(",", bounds[k].Select(PyFormat.Repr))));
            root.children.Add(new Elem("ShadowFitPoints").Set("numShadowFitPoints", EVal.S("_uint:0")));
            root.children.Add(new Elem("TreeAsset").Set("version", EVal.S("_uint:1")));
            if (surfaces != null && surfaces.Count > 0)
            {
                var sn = new Elem("Surfaces"); root.children.Add(sn);
                foreach (var (block, l, material, start, count) in surfaces)
                    sn.children.Add(new Elem("Surface").Set("block", EVal.S("_uint:" + block)).Set("lod", EVal.S("_uint:" + l)).Set("material", EVal.S(material))
                        .Set("start", EVal.S("_uint:" + start)).Set("count", EVal.S("_uint:" + count)));
            }
            return Bxml.EncodeElement(root);
        }

        /// alive_environment.byte4n: D3DDECLTYPE_BYTE4N, x in the last byte in memory.
        static float Byte4N(byte[] s, int at, int k)
        {
            int b = s[at + 3 - k];
            return (float)Math.Max(-1.0, (b > 127 ? b - 256 : b) / 127.0);
        }

        public static byte[] Tree(byte[] source)
        {
            Bin.Require(source.Length >= 12, "Alive tree v2");
            uint magic = Bin.U32BE(source, 0), version = Bin.U32BE(source, 4), xmlsize = Bin.U32BE(source, 8);
            Bin.Require(magic == 0x54524545 && version == 2, "Alive tree v2");
            long p = 0x7ac;
            Bin.Require(p + xmlsize <= source.Length, "Tree XML");
            var xml = Bin.Slice(source, (int)p, (int)xmlsize); Bin.Require(Bin.StartsWith(xml, "BXML"), "Tree XML"); p += xmlsize;
            var headers = new List<byte[]>(); var buffers = new List<byte[]>();
            // a LOD's surfaces (per LOD: start, count, surface count, 0, then (start, count) per surface, absolute) with the Materials records'
            // names; kept only for a tree with a LOD of more than one surface (alive_environment.tree)
            var surfaces = new List<(int block, int lod, string material, int start, int count)>();
            bool multi = false;
            for (int b = 0; b < 4; b++)
            {
                int hb = 12 + b * 488;
                if (Bin.U32BE(source, hb + 4) == 0) continue;
                for (int l = 0; l < Math.Min(10, (int)Bin.U16BE(source, hb + 14)); l++) if (Bin.U16BE(source, hb + 16 + l * 28 + 4) > 1) multi = true;
            }
            var materials = multi ? TreeMaterials(xml) : new Dictionary<string, List<List<string>>>();
            for (int block = 0; block < 4; block++)
            {
                int h = 12 + block * 488;
                uint ib = Bin.U32BE(source, h), vb = Bin.U32BE(source, h + 4), flag = Bin.U32BE(source, h + 8);
                int stride = Bin.U16BE(source, h + 12), lods = Bin.U16BE(source, h + 14);
                Bin.Require(lods <= 10 && stride <= 256 && (vb == 0 || stride > 0), "Tree block counts");
                var outh = new byte[184]; byte[] buf = Array.Empty<byte>();
                if (vb != 0 && lods != 0)
                {
                    long n = vb / stride;
                    Bin.Require(n * stride == vb && p + vb + ib <= source.Length, "Tree buffers");
                    int orig = (int)p; p += vb;
                    int indicesAt = (int)p; p += ib;
                    var decl = new List<(int off, uint typ, int usage, int index, int count, int dest)>(); int outstride = 0; bool terminated = false;
                    for (int j = 0; j < 16; j++)
                    {
                        int at = h + 0x128 + j * 12;
                        int stream = Bin.U16BE(source, at), off = Bin.U16BE(source, at + 2); uint typ = Bin.U32BE(source, at + 4);
                        int method = source[at + 8], usage = source[at + 9], index = source[at + 10];
                        if (stream == 255) { terminated = true; break; }
                        Bin.Require(stream == 0 && method == 0, "Tree declaration stream");
                        Bin.Require(typ == 0x1a2360 || typ == 0x2c235f || typ == 0x1a2186, "Tree declaration format");
                        int count = typ == 0x1a2360 ? 4 : typ == 0x2c235f ? 2 : 4;
                        Bin.Require(off + (typ != 0x1a2186 ? count * 2 : 4) <= stride, "Tree declaration range");
                        decl.Add((off, typ, usage, index, count, outstride));
                        PutDecl(outh, 56 + j * 8, 0, outstride, count - 1, 0, usage, index);
                        outstride += 4 * count;
                    }
                    Bin.Require(terminated, "Unterminated tree declaration");
                    PutDecl(outh, 56 + decl.Count * 8, 255, 0, 17, 0, 0, 0);
                    int cols = outstride / 4;
                    var verts = new float[n * cols];
                    foreach (var (off, typ, usage, index, count, dest) in decl)
                    {
                        for (int v = 0; v < n; v++)
                        {
                            int at = orig + v * stride + off;
                            for (int k = 0; k < count; k++)
                            {
                                float x = typ != 0x1a2186 ? Bin.Half(Bin.U16BE(source, at + 2 * k)) : Byte4N(source, at, k);
                                Bin.Require(!float.IsNaN(x) && !float.IsInfinity(x), "Nonfinite tree vertex");
                                verts[v * cols + dest / 4 + k] = x;
                            }
                        }
                        if (usage == 0 && block != 3)
                        {
                            Bin.Require(dest / 4 + 3 < cols, "Tree position column");
                            for (int v = 0; v < n; v++) verts[v * cols + dest / 4 + 3] = 0;
                        }
                    }
                    var starts = new List<int>(); var counts = new List<int>();
                    var lodSurfaces = new List<List<(string name, int s, int c)>>();
                    materials.TryGetValue(TreeBlockParams[block], out var names);
                    for (int l = 0; l < lods; l++)
                    {
                        int ls = Bin.U16BE(source, h + 16 + l * 28), lc = Bin.U16BE(source, h + 18 + l * 28), nsurf = Bin.U16BE(source, h + 20 + l * 28);
                        starts.Add(ls); counts.Add(lc);
                        Bin.Require(nsurf <= 5, "Tree surface count");
                        var surf = new List<(string name, int s, int c)>();
                        if (multi)
                        {
                            var lodNames = names != null && l < names.Count ? names[l] : new List<string>();
                            Bin.Require(lodNames.Count == nsurf, "Tree surface materials");
                            for (int k = 0; k < nsurf; k++)
                            {
                                int s0 = Bin.U16BE(source, h + 24 + l * 28 + 4 * k), c0 = Bin.U16BE(source, h + 26 + l * 28 + 4 * k);
                                Bin.Require(ls <= s0 && s0 + c0 <= ls + lc, "Tree surface range");
                                surf.Add((lodNames[k], s0, c0));
                            }
                        }
                        lodSurfaces.Add(surf);
                    }
                    byte[] indicesLE;
                    if (block == 3)
                    {
                        Bin.Require(ib == 0 && n % 4 == 0 && starts.Zip(counts, (s, c) => s % 4 == c % 4 && c % 4 == 0).All(x => x), "Leaf quad layout");
                        Bin.Require(starts.Zip(counts, (s, c) => s + c <= n).All(x => x), "Leaf LOD range");
                        var quads = new float[n / 4 * 6 * cols];
                        int[] order = { 0, 1, 2, 0, 2, 3 };
                        for (long qd = 0; qd < n / 4; qd++)
                            for (int k = 0; k < 6; k++) Array.Copy(verts, (qd * 4 + order[k]) * cols, quads, (qd * 6 + k) * cols, cols);
                        verts = quads;
                        starts = starts.Select(s => s / 4 * 6).ToList(); counts = counts.Select(c => c / 4 * 6).ToList();
                        for (int l = 0; l < lodSurfaces.Count; l++)
                            foreach (var (name, s0, c0) in lodSurfaces[l])
                            {
                                Bin.Require(s0 % 4 == 0 && c0 % 4 == 0, "Leaf surface quads");
                                surfaces.Add((2, l, name, s0 / 4 * 6, c0 / 4 * 6));
                            }
                        indicesLE = Array.Empty<byte>();
                    }
                    else
                    {
                        Bin.Require(starts.Zip(counts, (s, c) => (s + c) * 2 <= ib).All(x => x), "Tree strip LOD range");
                        var src = new int[ib / 2]; for (int k = 0; k < src.Length; k++) src[k] = Bin.U16BE(source, indicesAt + 2 * k);
                        var rebuilt = new List<int>(); var ns2 = new List<int>(); var nc2 = new List<int>();
                        List<int[]> Triangles(int start, int count)
                        {
                            var tri = new List<int[]>();
                            if (flag == 1)
                            {
                                Bin.Require(count % 3 == 0, "Tree triangle list count");
                                for (int k = 0; k < count; k += 3) tri.Add(new[] { src[start + k], src[start + k + 1], src[start + k + 2] });
                            }
                            else
                            {
                                var strip = new List<int>();
                                for (int k = 0; k <= count; k++)
                                {
                                    int idx = k < count ? src[start + k] : 65535;
                                    if (idx == 65535)
                                    {
                                        for (int t = 0; t < strip.Count - 2; t++)
                                        {
                                            int a = strip[t], b = strip[t + 1], c = strip[t + 2];
                                            if (a != b && b != c && a != c) tri.Add(t % 2 == 1 ? new[] { b, a, c } : new[] { a, b, c });
                                        }
                                        strip.Clear();
                                    }
                                    else strip.Add(idx);
                                }
                            }
                            return tri;
                        }
                        for (int l = 0; l < starts.Count; l++)
                        {
                            var tri = Triangles(starts[l], counts[l]);
                            // triangle k of the LOD is at 6 k of the rebuilt list, so a surface of triangles [t, t + n) is the range (6 t, 6 n - 3)
                            int t0 = 0; var cover = new List<int[]>();
                            foreach (var (name, s0, c0) in lodSurfaces[l])
                            {
                                var st = Triangles(s0, c0); cover.AddRange(st);
                                surfaces.Add((TreePcBlock[block], l, name, st.Count > 0 ? rebuilt.Count + 6 * t0 : rebuilt.Count, st.Count > 0 ? 6 * st.Count - 3 : 0));
                                t0 += st.Count;
                            }
                            Bin.Require(lodSurfaces[l].Count == 0 || (cover.Count == tri.Count && cover.Zip(tri, (x, y) => x[0] == y[0] && x[1] == y[1] && x[2] == y[2]).All(z => z)), "Tree surfaces cover the LOD");
                            var lod = new List<int>();
                            foreach (var t in tri)
                            {
                                Bin.Require(Math.Max(t[0], Math.Max(t[1], t[2])) < n, "Tree index range");
                                if (lod.Count > 0) { int last = lod[lod.Count - 1]; lod.AddRange(new[] { last, last, t[0], t[0], t[1], t[2] }); }
                                else lod.AddRange(t);
                            }
                            ns2.Add(rebuilt.Count); nc2.Add(lod.Count); rebuilt.AddRange(lod);
                        }
                        starts = ns2; counts = nc2;
                        Bin.Require(rebuilt.All(v => v < 65536), "Converted tree index");
                        indicesLE = new byte[rebuilt.Count * 2];
                        for (int k = 0; k < rebuilt.Count; k++) { indicesLE[2 * k] = (byte)rebuilt[k]; indicesLE[2 * k + 1] = (byte)(rebuilt[k] >> 8); }
                        ib = (uint)indicesLE.Length;
                        Bin.Require(starts.Concat(counts).All(v => v < 65536), "Converted tree LOD size");
                    }
                    Bin.PutU32LE(outh, 0, ib); Bin.PutU32LE(outh, 4, (uint)(verts.Length * 4)); Bin.PutU32LE(outh, 8, flag);
                    Bin.PutU16LE(outh, 12, (ushort)outstride); Bin.PutU16LE(outh, 14, (ushort)lods);
                    for (int l = 0; l < starts.Count; l++) { Bin.PutU16LE(outh, 16 + 2 * l, (ushort)starts[l]); Bin.PutU16LE(outh, 36 + 2 * l, (ushort)counts[l]); }
                    var vbytes = new byte[verts.Length * 4]; Buffer.BlockCopy(verts, 0, vbytes, 0, vbytes.Length);
                    if (!BitConverter.IsLittleEndian) for (int k = 0; k < vbytes.Length; k += 4) Array.Reverse(vbytes, k, 4);
                    buf = Cat(vbytes, indicesLE);
                }
                headers.Add(outh); buffers.Add(buf);
            }
            Bin.Require(p == source.Length, "Tree trailing bytes");
            int[] ord = { 0, 1, 3, 2 };
            var x2 = TreeXml(xml, Bin.U16LE(headers[3], 14), surfaces.Select((v, i) => (v, i)).OrderBy(q => q.v.block).ThenBy(q => q.v.lod).ThenBy(q => q.i).Select(q => q.v).ToList());
            var head = new byte[12]; Bin.PutU32LE(head, 0, magic); Bin.PutU32LE(head, 4, 1); Bin.PutU32LE(head, 8, (uint)x2.Length);
            var parts = new List<byte[]> { head };
            foreach (int k in ord) parts.Add(headers[k]);
            parts.Add(x2);
            foreach (int k in ord) parts.Add(buffers[k]);
            return Cat(parts.ToArray());
        }

        /// alive_convert.tiled_block_offsets (pitch = width aligned to 32).
        static long TiledOffset(long x, long y, long width, long bpb) => Textures.TiledOffset(x, y, (width + 31) & ~31L, bpb);

        public static byte[] Grass(byte[] source)
        {
            Bin.Require(source.Length >= 288 && Bin.U32BE(source, 0) == 0xbb88aa11 && Bin.U32BE(source, 4) == 2 && Bin.U32BE(source, 8) == 2, "Grass header");
            uint blades = Bin.U32BE(source, 12), cells = Bin.U32BE(source, 16), w = Bin.U32BE(source, 20), h = Bin.U32BE(source, 24);
            int tw = Bin.U16BE(source, 256), th = Bin.U16BE(source, 258); uint fmt = Bin.U32BE(source, 260), size = Bin.U32BE(source, 264);
            Bin.Require(fmt == 0x18280186 && size == tw * th * 4 && source.Length == 288 + (long)size + (long)blades * 12 + (long)cells * 32 && cells == (long)w * h, "Grass payload");
            var o = (byte[])source.Clone();
            Bin.SwapWords(o, 0, 68);
            Bin.PutU16LE(o, 256, (ushort)tw); Bin.PutU16LE(o, 258, (ushort)th); Bin.PutU32LE(o, 260, 21); Bin.PutU32LE(o, 264, size);
            long max = 0;
            for (int y = 0; y < th; y++) for (int x = 0; x < tw; x++) max = Math.Max(max, TiledOffset(x, y, tw, 4));
            Bin.Require(max + 4 <= size, "Grass texture addresses");
            for (int y = 0; y < th; y++)
                for (int x = 0; x < tw; x++)
                    Buffer.BlockCopy(source, 288 + (int)TiledOffset(x, y, tw, 4), o, 288 + (y * tw + x) * 4, 4);
            int p = 288 + (int)size;
            Bin.SwapWords(o, p, (int)blades * 12); p += (int)blades * 12;
            for (int i = 0; i < cells; i++) { Bin.SwapWords(o, p, 12); Bin.SwapWords(o, p + 16, 16); p += 32; }
            return o;
        }

        public static byte[] Water(byte[] source)
        {
            Bin.Require(source.Length >= 232 && Bin.U32BE(source, 0) == 0x77770360 && Bin.U32BE(source, 4) == 5 && Bin.U32BE(source, 8) == 0, "Water header");
            Bin.Require(Bin.U32BE(source, 212) == 0 && Bin.U32BE(source, 216) == 1 && Bin.U32BE(source, 220) == 0, "Lake descriptor layout");
            long w = Bin.U32BE(source, 224), h = Bin.U32BE(source, 228), pitch = Math.Max(256, w * 4);
            Bin.Require(source.Length == 232 + pitch * h, "Lake padded rows");
            var o = new byte[232 + w * h * 4];
            Buffer.BlockCopy(source, 0, o, 0, 232);
            Bin.SwapWords(o, 0, 12);
            Array.Clear(o, 12, 128);
            Bin.SwapWords(o, 140, 92);
            for (long y = 0; y < h; y++)
                for (long x = 0; x < w; x++)
                    for (int c = 0; c < 4; c++) o[232 + (y * w + x) * 4 + c] = source[232 + y * pitch + x * 4 + 3 - c];
            return o;
        }

        public static byte[] Particle(byte[] source)
        {
            var doc = Bxml.Parse(source, scene: true, float2: 2);
            Bin.Require(!doc.nodes.Any(x => x.kind == 2) && !doc.attrs.Any(x => x.kind == 2), "Opaque particle payload");
            return source;
        }

        /// alive_environment.anim: Alive animation streams (big endian) to the PC layout (AnimAssetIO 0082cc70 / 0082bf10): i32 -version, then
        /// the packed format (version >= 5 and the marker 0x12345678: a word, the track count, the names, a 16-word key-set header and its
        /// data block: header[11] bytes of 16-byte float rows, then int16 quaternion keys) or the track format (a word, +0x48, the track
        /// count, the names, the header words, the translation tracks: a key count, a bone word, 16-byte (time, x, y, z) float keys, the
        /// rotation tracks: a key count, a bone word, 12-byte keys of a float time and an int16 xyzw quaternion, then 4-byte words to the
        /// end). Names: a length word and that many bytes. The rotation keys' int16s are swapped one by one (as words they came out
        /// (y, x, w, z): the thrashing banners, holeshot arches, flags and eagles).
        public static byte[] Anim(byte[] source)
        {
            Bin.Require(source.Length >= 16, "Truncated anim");
            long version = -(long)(int)Bin.U32BE(source, 0);
            var o = (byte[])source.Clone();
            void Words(long at, long n)
            {
                Bin.Require(at + 4 * n <= source.Length, "Anim words beyond the stream");
                for (long p = at; p < at + 4 * n; p += 4) { o[p] = source[p + 3]; o[p + 1] = source[p + 2]; o[p + 2] = source[p + 1]; o[p + 3] = source[p]; }
            }
            void Halves(long at, long n)
            {
                Bin.Require(at + 2 * n <= source.Length, "Anim int16s beyond the stream");
                for (long p = at; p < at + 2 * n; p += 2) { o[p] = source[p + 1]; o[p + 1] = source[p]; }
            }
            long Names(long at, long count)
            {
                Bin.Require(count >= 0 && count <= 4096, "Anim track count");
                for (long i = 0; i < count; i++)
                {
                    Bin.Require(at + 4 <= source.Length, "Truncated anim name");
                    uint n = Bin.U32BE(source, (int)at);
                    Bin.Require(n <= 0x20, "Anim name longer than its 32-byte slot");
                    Words(at, 1); at += 4 + n;
                }
                return at;
            }
            Words(0, 2);
            uint marker = Bin.U32BE(source, 4);
            if (version >= 5 && marker == 0x12345678)
            {
                Words(8, 2);
                long count = Bin.U32BE(source, 12);
                long at = Names(16, count);
                Bin.Require(at + 64 <= source.Length, "Truncated anim key-set header");
                long size = Bin.U32BE(source, (int)at), rot = Bin.U32BE(source, (int)at + 44);
                Words(at, 16); at += 64;
                Bin.Require(at + size == source.Length && rot <= size && rot % 16 == 0 && (size - rot) % 8 == 0, "Packed anim extents");
                Words(at, rot / 4);
                Halves(at + rot, (size - rot) / 2);
                return o;
            }
            Words(8, 2);
            long tracks = Bin.U32BE(source, 12);
            long a2 = Names(16, tracks);
            // the header words 0082bf10 reads after the names (alive_environment.anim)
            long fields = 5 + (version >= 6 ? 1 : 0) + (version >= 9 ? 1 : 0) + (version >= 10 ? 1 : 0) + (version <= 7 ? 1 : 0) + (version > 10 ? 3 : 0);
            Bin.Require(a2 + 4 * fields <= source.Length, "Truncated anim track header");
            long translations = Bin.U32BE(source, (int)a2 + 12), rotations = Bin.U32BE(source, (int)a2 + 16);
            Bin.Require(translations <= 4096 && rotations <= 4096, "Anim track counts");
            Words(a2, fields); a2 += 4 * fields;
            for (long i = 0; i < translations; i++)
            {
                Bin.Require(a2 + 8 <= source.Length, "Truncated anim translation track");
                long keys = Bin.U32BE(source, (int)a2);
                Words(a2, 2 + 4 * keys); a2 += 8 + 16 * keys;
            }
            for (long i = 0; i < rotations; i++)
            {
                Bin.Require(a2 + 8 <= source.Length, "Truncated anim rotation track");
                long keys = Bin.U32BE(source, (int)a2);
                Bin.Require(a2 + 8 + 12 * keys <= source.Length, "Anim rotation keys beyond the stream");
                Words(a2, 2); a2 += 8;
                for (long k = 0; k < keys; k++) { Words(a2, 1); Halves(a2 + 4, 4); a2 += 12; }
            }
            long rest = source.Length - a2;
            Bin.Require(rest % 4 == 0, "Anim track data is not whole words");
            Words(a2, rest / 4);
            return o;
        }

        /// alive_environment.bounce: the bounce light volume to the client's ABNC sidecar.
        public static byte[] Bounce(byte[] source)
        {
            Bin.Require(source.Length >= 136 && Bin.StartsWith(source, "BONC") && Bin.U32BE(source, 4) == 2, "Bounce resource");
            var desc = new uint[5, 4];
            for (int i = 0; i < 5; i++) for (int k = 0; k < 4; k++) desc[i, k] = Bin.U32BE(source, 56 + 16 * i + 4 * k);
            Bin.Require(desc[0, 0] == 0x1a22aa60 && desc[1, 0] == 0x1a22aa60 && desc[2, 0] == 0x18280086 && desc[3, 0] == 0x18280086 && desc[4, 0] == 0x18280086, "Bounce volume formats");
            (long, long, long) Dims(int i) => (desc[i, 1] & 0xFFFF, desc[i, 1] >> 16, desc[i, 2] >> 16);
            var idim = Dims(0); var adim = Dims(2);
            Bin.Require(Dims(1) == idim && Dims(2) == adim && Dims(3) == adim && Dims(4) == adim, "Bounce volume sizes");
            long isize = idim.Item1 * idim.Item2 * idim.Item3 * 8, asize = adim.Item1 * adim.Item2 * adim.Item3 * 4;
            const int data = 136;
            Bin.Require(desc[1, 3] == isize && desc[2, 3] == 0 && desc[3, 3] == asize && desc[4, 3] == 2 * asize, "Bounce data offsets");
            Bin.Require(source.Length == data + 2 * isize + 3 * asize, "Bounce resource size");
            var o = new byte[4 + 4 + 24 + 24 + 8 + 2 * isize + 3 * asize];
            o[0] = (byte)'A'; o[1] = (byte)'B'; o[2] = (byte)'N'; o[3] = (byte)'C';
            Bin.PutU32LE(o, 4, 1);
            for (int k = 0; k < 6; k++) Bin.PutF32LE(o, 8 + 4 * k, Bin.F32BE(source, 28 + 4 * k));
            uint[] dims = { (uint)idim.Item1, (uint)idim.Item2, (uint)idim.Item3, (uint)adim.Item1, (uint)adim.Item2, (uint)adim.Item3 };
            for (int k = 0; k < 6; k++) Bin.PutU32LE(o, 32 + 4 * k, dims[k]);
            Bin.PutU32LE(o, 56, Bin.U32BE(source, 24)); Bin.PutU32LE(o, 60, Bin.U32BE(source, 52));
            long at = 64;
            for (long i = 0; i < 2 * isize; i += 2) { o[at + i] = source[data + i + 1]; o[at + i + 1] = source[data + i]; }
            at += 2 * isize;
            long src = data + 2 * isize;
            for (long i = 0; i < 3 * asize; i += 4)
            {
                o[at + i] = source[src + i + 1]; o[at + i + 1] = source[src + i + 2]; o[at + i + 2] = source[src + i + 3]; o[at + i + 3] = source[src + i];
            }
            return o;
        }
    }

    /// alive_shader.py: Alive shader resources as the PC (D3D9) resource the Reflex readers parse.
    internal static class Shaders
    {
        internal sealed class Program { public uint kind; public string name; public byte[] container; }
        internal sealed class Constant { public string name, regset; public int index, count, cls, typ, rows, cols; }

        static int BxmlSize(byte[] data, int at)
        {
            Bin.Require(at + 36 <= data.Length && Bin.StartsWith(data, "BXML", at), "Expected a BXML document");
            return 36 + (int)Bin.U32LE(data, at + 32);
        }

        internal static List<Program> Programs(byte[] data)
        {
            Bin.Require(Bin.StartsWith(data, "JHM\0"), "Not an Alive shader resource");
            int p = 4 + BxmlSize(data, 4);
            Bin.Require(p + 8 <= data.Length, "Shader resource version / count");
            uint version = Bin.U32BE(data, p), count = Bin.U32BE(data, p + 4); p += 8;
            Bin.Require(version == 2 && count > 0 && count <= 64, "Shader resource version / count");
            var o = new List<Program>();
            for (int i = 0; i < count; i++)
            {
                Bin.Require(p + 8 <= data.Length, "Truncated shader program");
                uint kind = Bin.U32BE(data, p), nl = Bin.U32BE(data, p + 4); p += 8;
                Bin.Require(p + nl + 4 <= data.Length, "Truncated shader program");
                int z = Array.IndexOf(data, (byte)0, p, (int)nl); string name = Encoding.UTF8.GetString(data, p, (z < 0 ? p + (int)nl : z) - p); p += (int)nl;
                int size = (int)Bin.U32BE(data, p); p += 4;
                Bin.Require(size >= 0 && p + size <= data.Length, "Truncated shader program");
                var blob = Bin.Slice(data, p, size); p += size;
                p += BxmlSize(data, p);
                o.Add(new Program { kind = kind, name = name, container = blob });
            }
            Bin.Require(p == data.Length, "Shader resource trailing bytes");
            return o;
        }

        static string CString(byte[] b, int at)
        {
            int e = Array.IndexOf(b, (byte)0, at);
            Bin.Require(at >= 0 && e >= 0, "Constant table string");
            return Encoding.UTF8.GetString(b, at, e - at);
        }

        internal static (string target, List<Constant> constants) Ctab(byte[] blob, int offset)
        {
            uint nconst = Bin.U32BE(blob, offset + 12), cinfo = Bin.U32BE(blob, offset + 16), target = Bin.U32BE(blob, offset + 24);
            var o = new List<Constant>();
            string[] regsets = { "bool", "int4", "float4", "sampler" };
            for (int i = 0; i < nconst; i++)
            {
                int at = offset + (int)cinfo + i * 20;
                uint name = Bin.U32BE(blob, at); int regset = Bin.U16BE(blob, at + 4);
                Bin.Require(regset < 4, "Constant register set");
                int ti = offset + (int)Bin.U32BE(blob, at + 12);
                o.Add(new Constant { name = CString(blob, offset + (int)name), regset = regsets[regset], index = Bin.U16BE(blob, at + 6), count = Bin.U16BE(blob, at + 8),
                                     cls = Bin.U16BE(blob, ti), typ = Bin.U16BE(blob, ti + 2), rows = Bin.U16BE(blob, ti + 4), cols = Bin.U16BE(blob, ti + 6) });
            }
            return (CString(blob, offset + (int)target), o);
        }

        static byte[] CtabBytes(List<Constant> constants, string target, bool pixel)
        {
            int n = constants.Count, header = 28, infosAt = header, typesAt = infosAt + 20 * n, stringsAt = typesAt + 16 * n;
            var strings = new MemoryStream();
            int Add(string s) { int at = (int)strings.Length; var b = Encoding.UTF8.GetBytes(s); strings.Write(b, 0, b.Length); strings.WriteByte(0); return at; }
            int creator = Add("MX vs ATV Alive (Xenos) constant table"), tgt = Add(target);
            var names = constants.Select(c => Add(c.name)).ToList();
            var body = new MemoryStream(); var w = new byte[20];
            void U32(uint v) { Bin.PutU32LE(w, 0, v); body.Write(w, 0, 4); }
            void U16(int v) { Bin.PutU16LE(w, 0, (ushort)v); body.Write(w, 0, 2); }
            U32((uint)header); U32((uint)(stringsAt + creator)); U32(pixel ? 0xFFFF0300u : 0xFFFE0300u); U32((uint)n); U32((uint)infosAt); U32(0); U32((uint)(stringsAt + tgt));
            var regsets = new Dictionary<string, int> { { "bool", 0 }, { "int4", 1 }, { "float4", 2 }, { "sampler", 3 } };
            for (int i = 0; i < n; i++)
            {
                var c = constants[i];
                U32((uint)(stringsAt + names[i])); U16(regsets[c.regset]); U16(c.index); U16(c.count); U16(0); U32((uint)(typesAt + 16 * i)); U32(0);
            }
            foreach (var c in constants) { U16(c.cls); U16(c.typ); U16(c.rows); U16(c.cols); U16(1); U16(0); U32(0); }
            strings.Position = 0; strings.CopyTo(body);
            while (body.Length % 4 != 0) body.WriteByte(0);
            return body.ToArray();
        }

        public static byte[] ToPcResource(byte[] data)
        {
            var progs = Programs(data);
            var parsed = new List<(Program p, uint[] w, bool pixel, string target, List<Constant> constants)>();
            foreach (var p in progs)
            {
                Bin.Require(p.container.Length >= 48, "Not a Xenos shader container");
                var w = new uint[12]; for (int i = 0; i < 12; i++) w[i] = Bin.U32BE(p.container, 4 * i);
                Bin.Require(w[0] == 0x102A1100 || w[0] == 0x102A1101, "Not a Xenos shader container");
                var (target, constants) = Ctab(p.container, (int)w[4] + 4);
                parsed.Add((p, w, w[0] == 0x102A1100, target, constants));
            }
            var union = new Dictionary<(string, int), Constant>(); var unionOrder = new List<Constant>();
            foreach (var x in parsed)
                if (x.pixel)
                    foreach (var k in x.constants)
                        if (!union.ContainsKey((k.regset, k.index))) { union[(k.regset, k.index)] = k; unionOrder.Add(k); }
            var o = new MemoryStream(); var w4 = new byte[8];
            void U32(Stream s, uint v) { Bin.PutU32LE(w4, 0, v); s.Write(w4, 0, 4); }
            U32(o, 2); U32(o, (uint)progs.Count);
            bool firstPixel = true;
            foreach (var x in parsed)
            {
                var constants = x.constants;
                if (x.pixel && firstPixel)
                {
                    constants = unionOrder.OrderBy(k => k.regset, StringComparer.Ordinal).ThenBy(k => k.index).ToList();
                    firstPixel = false;
                }
                var table = Converters.Cat(Encoding.ASCII.GetBytes("CTAB"), CtabBytes(constants, x.target, x.pixel));
                var code = new MemoryStream();
                U32(code, x.pixel ? 0xFFFF0300u : 0xFFFE0300u);
                U32(code, 0xFFFE | (uint)((table.Length / 4) << 16)); code.Write(table, 0, table.Length);
                foreach (var (reg, v) in LiteralRegisters(x.p.container, x.w).OrderBy(kv => kv.Key).Select(kv => (kv.Key, kv.Value)))
                {
                    U32(code, 81 | (5 << 24)); U32(code, 0xA00F0000u | (uint)(reg & 0x7FF));
                    foreach (var f in v) { Bin.PutF32LE(w4, 0, (float)(double)f); code.Write(w4, 0, 4); }
                }
                if (x.pixel && x.p.name == "SharedPixelShader")
                {
                    // the sampler whose alpha the program outputs, as the D3D9 pair the Reflex readers recognize (texld r0, v0, sK;
                    // mov oC0.w, r0.w: ReflexScenery ScanAlphaSampler): alpha tested / blended Alive materials keep their cut-outs
                    int? k = AlphaSampler(x.p.container, x.w);
                    if (k is int ks)
                    {
                        U32(code, 66 | (3 << 24)); U32(code, 0x800F0000); U32(code, 0x90E40000); U32(code, 0xA0E40800u | (uint)ks);
                        U32(code, 1 | (2 << 24)); U32(code, 0x80080800); U32(code, 0x80FF0000);
                    }
                }
                U32(code, 0x0000FFFF);
                var name = Encoding.UTF8.GetBytes(x.p.name + "\0");
                Bin.Require(name.Length <= 0x40, "Shader entry name too long for the readers");
                U32(o, x.p.kind); U32(o, (uint)name.Length); o.Write(name, 0, name.Length); U32(o, (uint)code.Length);
                code.Position = 0; code.CopyTo(o);
            }
            return o.ToArray();
        }

        static readonly string[] CfOps = { "nop", "exec", "exece", "cexec", "cexece", "cexec_pred", "cexece_pred", "loop_start", "loop_end", "ccall", "ret",
                                           "cjmp", "alloc", "cexec_predclean", "cexece_predclean", "vfetch_end" };

        /// xenos_disasm.cf_decode / control_flow: (op, address, count, sequence) of the CF list.
        internal static List<(string op, int address, int count, int sequence)> ControlFlow(uint[] words)
        {
            var o = new List<(string, int, int, int)>(); int i = 0;
            (string, int, int, int) Decode(ulong bits)
            {
                int op = (int)((bits >> 44) & 15);
                bool exec = op == 1 || op == 2 || op == 3 || op == 4 || op == 5 || op == 6 || op == 13 || op == 14;
                return (CfOps[op], exec ? (int)(bits & 0xFFF) : 0, exec ? (int)((bits >> 12) & 7) : 0, exec ? (int)((bits >> 16) & 0xFFF) : 0);
            }
            while (3 * (i / 2) + 2 < words.Length)
            {
                ulong w0 = words[3 * (i / 2)], w1 = words[3 * (i / 2) + 1], w2 = words[3 * (i / 2) + 2];
                ulong bits = i % 2 == 0 ? (w0 | ((w1 & 0xFFFF) << 32)) : ((w1 >> 16) | (w2 << 16));
                var d = Decode(bits); o.Add(d); i++;
                if (d.Item1 == "exece" || d.Item1 == "cexece" || d.Item1 == "cexece_pred" || d.Item1 == "cexece_predclean" || i > 512)
                {
                    if (i % 2 == 1) o.Add(Decode((w1 >> 16) | (w2 << 16)));
                    break;
                }
            }
            return o;
        }

        /// alive_shader.alpha_sampler: the texture fetch constant (= sampler register) whose texel alpha a pixel program writes to oC0.w
        /// unchanged (tfetch rN <- tfK, then move-like writes: max(a, a) / maxs(a)), or null. Walks the straight-line exec clauses.
        static int? AlphaSampler(byte[] blob, uint[] w)
        {
            uint litBytes = Bin.U32BE(blob, (int)w[6]), insBytes = Bin.U32BE(blob, (int)w[6] + 4);
            long ucodeEnd = Math.Min((long)w[1] + w[2], blob.Length);
            long from = Math.Min(w[1] + (long)litBytes, ucodeEnd), to = Math.Min(w[1] + (long)litBytes + insBytes, ucodeEnd);
            int n = (int)Math.Max(0, (to - from) / 4);
            var words = new uint[n]; for (int i = 0; i < n; i++) words[i] = Bin.U32BE(blob, (int)from + 4 * i);
            var origin = new Dictionary<(int, int), (int sampler, int comp)>();
            (int sampler, int comp)? result = null;
            string[] execs = { "exec", "exece", "cexec", "cexece", "cexec_pred", "cexece_pred", "cexec_predclean", "cexece_predclean" };
            foreach (var cf in ControlFlow(words))
            {
                if (Array.IndexOf(execs, cf.op) < 0) continue;
                for (int i = 0; i < cf.count; i++)
                {
                    int at = cf.address + i;
                    if (3L * at + 2 >= words.Length) return null;
                    uint w0 = words[3 * at], w1 = words[3 * at + 1], w2 = words[3 * at + 2];
                    if (((cf.sequence >> (2 * i)) & 1) != 0)
                    {
                        if ((w0 & 31) != 1) continue;
                        int dst = (int)((w0 >> 12) & 63), cst = (int)((w0 >> 20) & 31);
                        uint dsw = w1 & 0xFFF;
                        for (int c = 0; c < 4; c++)
                        {
                            int sel = (int)((dsw >> (3 * c)) & 7);
                            if (sel < 4) origin[(dst, c)] = (cst, sel);
                            else if (sel != 7) origin.Remove((dst, c));
                        }
                        continue;
                    }
                    int vdest = (int)(w0 & 63), export = (int)((w0 >> 15) & 1), sdest = (int)((w0 >> 8) & 63);
                    int vmask = (int)((w0 >> 16) & 15), smask = (int)((w0 >> 20) & 15), sop = (int)((w0 >> 26) & 63);
                    int[] swz = { (int)((w1 >> 16) & 255), (int)((w1 >> 8) & 255), (int)(w1 & 255) };
                    int[] neg = { (int)((w1 >> 26) & 1), (int)((w1 >> 25) & 1), (int)((w1 >> 24) & 1) };
                    int[] regs = { (int)((w2 >> 16) & 255), (int)((w2 >> 8) & 255), (int)(w2 & 255) };
                    int vop = (int)((w2 >> 24) & 31);
                    int[] sl = { (int)((w2 >> 31) & 1), (int)((w2 >> 30) & 1), (int)((w2 >> 29) & 1) };
                    int Comp(int k, int lane) => ((swz[k] >> (2 * lane)) + lane) & 3;
                    (int, int)? SrcOrigin(int k, int lane)
                    {
                        if (sl[k] == 0 || neg[k] != 0) return null;
                        return origin.TryGetValue((regs[k] & 63, Comp(k, lane)), out var v) ? v : ((int, int)?)null;
                    }
                    bool vmove = vop == 2 && sl[0] != 0 && sl[1] != 0 && regs[0] == regs[1] && swz[0] == swz[1] && neg[0] == neg[1] && neg[1] == 0;
                    var nw = new Dictionary<int, (int, int)?>();
                    for (int lane = 0; lane < 4; lane++) if ((vmask >> lane & 1) != 0) nw[lane] = vmove ? SrcOrigin(0, lane) : null;
                    bool smove = sop == 5 && Comp(2, 0) == Comp(2, 1);
                    var sval = smove ? SrcOrigin(2, 0) : null;
                    if (export != 0 && vdest == 0 && (vmask & 8) != 0) result = nw.TryGetValue(3, out var r3) ? r3 : null;
                    if (export != 0 && sdest == 0 && (smask & 8) != 0) result = sval;
                    if (export == 0)
                    {
                        foreach (var kv in nw) { if (kv.Value == null) origin.Remove((vdest, kv.Key)); else origin[(vdest, kv.Key)] = kv.Value.Value; }
                        for (int lane = 0; lane < 4; lane++)
                            if ((smask >> lane & 1) != 0) { if (sval == null) origin.Remove((sdest, lane)); else origin[(sdest, lane)] = sval.Value; }
                    }
                }
            }
            if (result == null || result.Value.comp != 3) return null;
            return result.Value.sampler;
        }

        /// alive_shader.literal_registers: {register: (x, y, z, w)} of the program's nonzero literal constants.
        static Dictionary<int, float[]> LiteralRegisters(byte[] blob, uint[] w)
        {
            uint litBytes = Bin.U32BE(blob, (int)w[6]);
            int bas = (int)((Bin.U32BE(blob, (int)w[5] + 20) >> 16) & 0xFF);
            var o = new Dictionary<int, float[]>();
            for (int k = 0; k < litBytes / 16; k++)
            {
                Bin.Require(16L * k + 16 <= w[2] && w[1] + 16L * k + 16 <= blob.Length, "Shader literal constants");
                var v = new float[4]; for (int i = 0; i < 4; i++) v[i] = Bin.F32BE(blob, (int)w[1] + 16 * k + 4 * i);
                if (v.Any(f => f != 0)) o[bas + k] = v;
            }
            return o;
        }
    }
}

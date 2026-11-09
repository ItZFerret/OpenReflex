// MX vs ATV Alive importer (pure C#, no UnityEngine: it also builds standalone for the parity harness tests/alive_tracks/importer_parity).
// Shared helpers: big / little endian access, the deterministic zlib writer (stored deflate blocks, alive_convert.zlib_stored),
// inflate, a JSON writer that formats like Python's json.dumps, Python float repr.
// Reference implementation: _re/formats/alive_*.py (the Python pipeline is the oracle; outputs are byte-identical).
using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.IO.Compression;
using System.Numerics;
using System.Text;

namespace Reflex.Port.Content.AliveImport
{
    public sealed class AliveImportException : Exception
    {
        public AliveImportException(string message) : base(message) { }
    }

    internal static class Bin
    {
        public static void Require(bool ok, string message) { if (!ok) throw new AliveImportException(message); }

        public static uint U32BE(byte[] b, int p) => (uint)(b[p] << 24 | b[p + 1] << 16 | b[p + 2] << 8 | b[p + 3]);
        public static ushort U16BE(byte[] b, int p) => (ushort)(b[p] << 8 | b[p + 1]);
        public static uint U32LE(byte[] b, int p) => (uint)(b[p] | b[p + 1] << 8 | b[p + 2] << 16 | b[p + 3] << 24);
        public static ushort U16LE(byte[] b, int p) => (ushort)(b[p] | b[p + 1] << 8);
        public static int I32LE(byte[] b, int p) => (int)U32LE(b, p);
        public static float F32BE(byte[] b, int p) => BitConverter.Int32BitsToSingle((int)U32BE(b, p));
        public static float F32LE(byte[] b, int p) => BitConverter.Int32BitsToSingle((int)U32LE(b, p));
        public static void PutU32LE(byte[] b, int p, uint v) { b[p] = (byte)v; b[p + 1] = (byte)(v >> 8); b[p + 2] = (byte)(v >> 16); b[p + 3] = (byte)(v >> 24); }
        public static void PutU16LE(byte[] b, int p, ushort v) { b[p] = (byte)v; b[p + 1] = (byte)(v >> 8); }
        public static void PutU32BE(byte[] b, int p, uint v) { b[p] = (byte)(v >> 24); b[p + 1] = (byte)(v >> 16); b[p + 2] = (byte)(v >> 8); b[p + 3] = (byte)v; }
        public static void PutF32LE(byte[] b, int p, float v) => PutU32LE(b, p, (uint)BitConverter.SingleToInt32Bits(v));

        /// Reverse each `width`-byte unit in [start, start + size) (alive_convert.swap_words).
        public static void SwapWords(byte[] data, int start, int size, int width = 4)
        {
            Require(start >= 0 && size % width == 0 && start + size <= data.Length, "Word range outside asset");
            for (int p = start; p < start + size; p += width) Array.Reverse(data, p, width);
        }

        public static byte[] Slice(byte[] b, int start, int length)
        {
            Require(start >= 0 && length >= 0 && start + length <= b.Length, "Range outside data");
            var r = new byte[length]; Buffer.BlockCopy(b, start, r, 0, length); return r;
        }

        public static int IndexOf(byte[] hay, byte[] needle, int start = 0, int end = -1)
        {
            if (end < 0) end = hay.Length;
            for (int i = Math.Max(0, start); i + needle.Length <= end; i++)
            {
                int k = 0; while (k < needle.Length && hay[i + k] == needle[k]) k++;
                if (k == needle.Length) return i;
            }
            return -1;
        }
        public static int LastIndexOf(byte[] hay, byte[] needle)
        {
            for (int i = hay.Length - needle.Length; i >= 0; i--)
            {
                int k = 0; while (k < needle.Length && hay[i + k] == needle[k]) k++;
                if (k == needle.Length) return i;
            }
            return -1;
        }
        public static bool StartsWith(byte[] b, string ascii, int at = 0)
        {
            if (at + ascii.Length > b.Length) return false;
            for (int i = 0; i < ascii.Length; i++) if (b[at + i] != (byte)ascii[i]) return false;
            return true;
        }

        public static uint Adler32(byte[] data, int start, int length)
        {
            uint a = 1, s = 0;
            for (int i = 0; i < length;)
            {
                int n = Math.Min(3800, length - i);
                for (int k = 0; k < n; k++, i++) { a += data[start + i]; s += a; }
                a %= 65521; s %= 65521;
            }
            return (s << 16) | a;
        }

        /// A zlib stream of stored deflate blocks (alive_convert.zlib_stored).
        public static byte[] ZlibStored(byte[] payload)
        {
            var o = new MemoryStream(payload.Length + payload.Length / 65535 * 5 + 16);
            o.WriteByte(0x78); o.WriteByte(0x01);
            if (payload.Length == 0) { o.WriteByte(1); o.WriteByte(0); o.WriteByte(0); o.WriteByte(0xFF); o.WriteByte(0xFF); }
            for (int at = 0; at < payload.Length; at += 65535)
            {
                int n = Math.Min(65535, payload.Length - at);
                o.WriteByte((byte)(at + 65535 >= payload.Length ? 1 : 0));
                o.WriteByte((byte)n); o.WriteByte((byte)(n >> 8));
                int c = n ^ 0xFFFF; o.WriteByte((byte)c); o.WriteByte((byte)(c >> 8));
                o.Write(payload, at, n);
            }
            uint ad = Adler32(payload, 0, payload.Length);
            o.WriteByte((byte)(ad >> 24)); o.WriteByte((byte)(ad >> 16)); o.WriteByte((byte)(ad >> 8)); o.WriteByte((byte)ad);
            return o.ToArray();
        }

        /// Inflates a zlib stream (2-byte header, deflate data) to exactly `expected` bytes.
        public static byte[] Inflate(byte[] data, int start, int length, int expected)
        {
            Require(length >= 2 && (data[start] & 0x0F) == 8 && ((data[start] << 8) | data[start + 1]) % 31 == 0, "Invalid zlib stream");
            using (var input = new MemoryStream(data, start + 2, length - 2, false))
            using (var z = new DeflateStream(input, CompressionMode.Decompress))
            {
                var o = new byte[expected];
                int got = 0;
                while (got < expected)
                {
                    int n = z.Read(o, got, expected - got);
                    if (n <= 0) break;
                    got += n;
                }
                Require(got == expected && z.Read(new byte[1], 0, 1) == 0, "Decoded table lengths do not match header");
                return o;
            }
        }

        /// Inflates a complete zlib stream (zlib.decompress).
        public static byte[] InflateAll(byte[] data, int start, int length)
        {
            Require(length >= 2 && (data[start] & 0x0F) == 8 && ((data[start] << 8) | data[start + 1]) % 31 == 0, "Invalid zlib stream");
            using (var input = new MemoryStream(data, start + 2, length - 2, false))
            using (var z = new DeflateStream(input, CompressionMode.Decompress))
            using (var o = new MemoryStream())
            {
                z.CopyTo(o);
                return o.ToArray();
            }
        }

        public static string Hex(byte[] b, int start, int length)
        {
            var sb = new StringBuilder(length * 2);
            for (int i = 0; i < length; i++) sb.Append(b[start + i].ToString("x2"));
            return sb.ToString();
        }

        /// IEEE half -> float (exact).
        public static float Half(ushort h)
        {
            int s = (h >> 15) & 1, e = (h >> 10) & 31, m = h & 1023;
            float v;
            if (e == 0) v = m * (1f / 16777216f);
            else if (e == 31) v = m == 0 ? float.PositiveInfinity : float.NaN;
            else v = (1f + m / 1024f) * (float)Math.Pow(2, e - 15);
            return s == 1 ? -v : v;
        }
    }

    /// Python-compatible text formatting (repr of floats, json.dumps layout).
    public static class PyFormat
    {
        /// repr(float): the shortest round-trip digits, fixed notation for 1e-4 <= |x| < 1e16, else d.ddde+XX.
        public static string Repr(double v)
        {
            if (double.IsNaN(v)) return "NaN";
            if (double.IsInfinity(v)) return v > 0 ? "Infinity" : "-Infinity";
            if (v == 0) return (1 / v) < 0 ? "-0.0" : "0.0";
            bool neg = v < 0;
            var (digits, pointPos) = Shortest(Math.Abs(v));   // value = 0.d1d2... * 10^pointPos
            int sciExp = pointPos - 1;   // value = 0.d1d2... * 10^pointPos = d1.d2... * 10^(pointPos-1)
            string body;
            if (sciExp >= -4 && sciExp < 16)
            {
                if (pointPos <= 0) body = "0." + new string('0', -pointPos) + digits;
                else if (pointPos >= digits.Length) body = digits + new string('0', pointPos - digits.Length) + ".0";
                else body = digits.Substring(0, pointPos) + "." + digits.Substring(pointPos);
            }
            else
            {
                string mant = digits.Length > 1 ? digits[0] + "." + digits.Substring(1) : digits;
                body = mant + "e" + (sciExp < 0 ? "-" : "+") + Math.Abs(sciExp).ToString("00", CultureInfo.InvariantCulture);
            }
            return (neg ? "-" : "") + body;
        }

        /// The shortest decimal digits that read back as v (> 0, finite), the closest of them to v (David Gay's shortest mode, as Python's
        /// repr): exact integer arithmetic, so every runtime (.NET, Mono) writes the same digits. Returns (digits, decimal point position).
        static (string digits, int point) Shortest(double v)
        {
            long bits = BitConverter.DoubleToInt64Bits(v);
            int be = (int)((bits >> 52) & 0x7FF); long frac = bits & 0xFFFFFFFFFFFFFL;
            long m = be == 0 ? frac : frac | (1L << 52); int e = be == 0 ? -1074 : be - 1075;   // v = m * 2^e
            // v and its rounding interval [low, high] as fractions over a common denominator: num / den, (num -+ half) / den
            BigInteger num, den, halfLow, halfHigh;
            bool asym = frac == 0 && be > 1;   // a power of two: the gap below is half the gap above
            if (e >= 0) { num = new BigInteger(m) << (e + 2); den = 4; halfHigh = BigInteger.One << (e + 1); halfLow = asym ? BigInteger.One << e : halfHigh; }
            else { num = new BigInteger(m) << 2; den = BigInteger.One << (2 - e); halfHigh = 2; halfLow = asym ? 1 : 2; }
            bool inclusive = (m & 1) == 0;
            // k: 10^k <= v < 10^(k+1)
            int k = (int)Math.Floor(Math.Log10(v));
            while (Pow10Cmp(num, den, k) < 0) k--;
            while (Pow10Cmp(num, den, k + 1) >= 0) k++;
            for (int p = 1; p <= 17; p++)
            {
                int scale = p - 1 - k;   // D = round(v * 10^scale)
                BigInteger n = num, d = den;
                if (scale >= 0) n *= BigInteger.Pow(10, scale); else d *= BigInteger.Pow(10, -scale);
                BigInteger q = BigInteger.DivRem(n, d, out BigInteger r);
                int c = (2 * r).CompareTo(d);
                if (c > 0 || c == 0 && !q.IsEven) q += 1;
                BigInteger best = BigInteger.MinusOne; BigInteger bestDist = BigInteger.Zero;
                foreach (var cand in new[] { q, q - 1, q + 1 })
                {
                    if (cand <= 0) continue;
                    // cand * 10^-scale against [num - halfLow, num + halfHigh] / den
                    BigInteger lhs = cand * den, low = (num - halfLow), high = (num + halfHigh);
                    if (scale >= 0) { low *= BigInteger.Pow(10, scale); high *= BigInteger.Pow(10, scale); lhs = cand * den; }
                    else lhs = cand * den * BigInteger.Pow(10, -scale);
                    int cl = lhs.CompareTo(low), ch = lhs.CompareTo(high);
                    bool inside = inclusive ? cl >= 0 && ch <= 0 : cl > 0 && ch < 0;
                    if (!inside) continue;
                    BigInteger dist = BigInteger.Abs(lhs - (scale >= 0 ? num * BigInteger.Pow(10, scale) : num));
                    if (best < 0 || dist < bestDist) { best = cand; bestDist = dist; }
                }
                if (best < 0) continue;
                string ds = best.ToString(CultureInfo.InvariantCulture);
                int point = k + 1 + (ds.Length - p);   // a rounding carry (999.. -> 1000..) adds a digit
                return (ds.TrimEnd('0').Length == 0 ? "0" : ds.TrimEnd('0'), point);
            }
            throw new AliveImportException("No round-trip digits for " + v.ToString("R", CultureInfo.InvariantCulture));
        }

        /// Sign of num / den - 10^k.
        static int Pow10Cmp(BigInteger num, BigInteger den, int k)
            => k >= 0 ? num.CompareTo(den * BigInteger.Pow(10, k)) : (num * BigInteger.Pow(10, -k)).CompareTo(den);

        /// format(v, ".9g") (bxml.format_value of float32 values).
        public static string G9(double v)
        {
            string s = v.ToString("G9", CultureInfo.InvariantCulture);
            int e = s.IndexOf('E');
            if (e < 0) return s;
            string mant = s.Substring(0, e); string ex = s.Substring(e + 1);
            int n = int.Parse(ex, CultureInfo.InvariantCulture);
            return mant + "e" + (n < 0 ? "-" : "+") + Math.Abs(n).ToString("00", CultureInfo.InvariantCulture);
        }
    }

    /// A JSON value tree with insertion-ordered objects, written like Python's json.dumps.
    public sealed class JObj
    {
        public readonly List<KeyValuePair<string, object>> items = new List<KeyValuePair<string, object>>();
        public JObj Set(string key, object value)
        {
            for (int i = 0; i < items.Count; i++) if (items[i].Key == key) { items[i] = new KeyValuePair<string, object>(key, value); return this; }
            items.Add(new KeyValuePair<string, object>(key, value)); return this;
        }
        public bool Has(string key) { foreach (var kv in items) if (kv.Key == key) return true; return false; }
        public object Get(string key) { foreach (var kv in items) if (kv.Key == key) return kv.Value; return null; }
    }

    public static class Json
    {
        /// json.dumps(value, indent=indent, sort_keys=sort) (indent < 0: the compact one-line form with ", " and ": ").
        public static string Dumps(object v, int indent = -1, bool sortKeys = false)
        {
            var sb = new StringBuilder(); Write(sb, v, indent, 0, sortKeys); return sb.ToString();
        }

        static void Write(StringBuilder sb, object v, int indent, int depth, bool sort)
        {
            switch (v)
            {
                case null: sb.Append("null"); return;
                case bool b: sb.Append(b ? "true" : "false"); return;
                case string s: Str(sb, s); return;
                case double d: sb.Append(PyFormat.Repr(d)); return;
                case float f: sb.Append(PyFormat.Repr(f)); return;
                case int i: sb.Append(i.ToString(CultureInfo.InvariantCulture)); return;
                case long l: sb.Append(l.ToString(CultureInfo.InvariantCulture)); return;
                case uint u: sb.Append(u.ToString(CultureInfo.InvariantCulture)); return;
                case JObj o:
                {
                    var items = new List<KeyValuePair<string, object>>(o.items);
                    if (sort) items.Sort((a, c) => string.CompareOrdinal(a.Key, c.Key));
                    if (items.Count == 0) { sb.Append("{}"); return; }
                    sb.Append('{');
                    for (int k = 0; k < items.Count; k++)
                    {
                        if (k > 0) sb.Append(indent < 0 ? ", " : ",");
                        if (indent >= 0) { sb.Append('\n'); sb.Append(' ', indent * (depth + 1)); }
                        Str(sb, items[k].Key); sb.Append(": ");
                        Write(sb, items[k].Value, indent, depth + 1, sort);
                    }
                    if (indent >= 0) { sb.Append('\n'); sb.Append(' ', indent * depth); }
                    sb.Append('}'); return;
                }
                case System.Collections.IEnumerable list:
                {
                    var arr = new List<object>(); foreach (var x in list) arr.Add(x);
                    if (arr.Count == 0) { sb.Append("[]"); return; }
                    sb.Append('[');
                    for (int k = 0; k < arr.Count; k++)
                    {
                        if (k > 0) sb.Append(indent < 0 ? ", " : ",");
                        if (indent >= 0) { sb.Append('\n'); sb.Append(' ', indent * (depth + 1)); }
                        Write(sb, arr[k], indent, depth + 1, sort);
                    }
                    if (indent >= 0) { sb.Append('\n'); sb.Append(' ', indent * depth); }
                    sb.Append(']'); return;
                }
                default: throw new AliveImportException("JSON: unsupported value " + v.GetType().Name);
            }
        }

        /// json.loads into JObj (ordered) / List<object> / string / long / double / bool / null.
        public static object Parse(string text)
        {
            int i = 0;
            var v = Value(text, ref i);
            Ws(text, ref i);
            Bin.Require(i == text.Length, "JSON: trailing data");
            return v;
        }

        static void Ws(string t, ref int i) { while (i < t.Length && (t[i] == ' ' || t[i] == '\t' || t[i] == '\n' || t[i] == '\r')) i++; }

        static object Value(string t, ref int i)
        {
            Ws(t, ref i);
            Bin.Require(i < t.Length, "JSON: unexpected end");
            char c = t[i];
            if (c == '{')
            {
                var o = new JObj(); i++; Ws(t, ref i);
                if (i < t.Length && t[i] == '}') { i++; return o; }
                while (true)
                {
                    Ws(t, ref i); Bin.Require(i < t.Length && t[i] == '"', "JSON: object key");
                    string k = Text(t, ref i); Ws(t, ref i);
                    Bin.Require(i < t.Length && t[i] == ':', "JSON: ':' expected"); i++;
                    o.Set(k, Value(t, ref i)); Ws(t, ref i);
                    Bin.Require(i < t.Length, "JSON: unexpected end");
                    if (t[i] == ',') { i++; continue; }
                    Bin.Require(t[i] == '}', "JSON: '}' expected"); i++; return o;
                }
            }
            if (c == '[')
            {
                var l = new List<object>(); i++; Ws(t, ref i);
                if (i < t.Length && t[i] == ']') { i++; return l; }
                while (true)
                {
                    l.Add(Value(t, ref i)); Ws(t, ref i);
                    Bin.Require(i < t.Length, "JSON: unexpected end");
                    if (t[i] == ',') { i++; continue; }
                    Bin.Require(t[i] == ']', "JSON: ']' expected"); i++; return l;
                }
            }
            if (c == '"') return Text(t, ref i);
            foreach (var (word, val) in new (string, object)[] { ("true", true), ("false", false), ("null", null), ("NaN", double.NaN), ("Infinity", double.PositiveInfinity), ("-Infinity", double.NegativeInfinity) })
                if (string.CompareOrdinal(t, i, word, 0, word.Length) == 0) { i += word.Length; return val; }
            int s = i;
            while (i < t.Length && "+-0123456789.eE".IndexOf(t[i]) >= 0) i++;
            string num = t.Substring(s, i - s);
            Bin.Require(num.Length > 0, "JSON: unexpected character");
            if (num.IndexOfAny(new[] { '.', 'e', 'E' }) < 0 && long.TryParse(num, NumberStyles.AllowLeadingSign, CultureInfo.InvariantCulture, out long n)) return n;
            Bin.Require(double.TryParse(num, NumberStyles.Float, CultureInfo.InvariantCulture, out double d), "JSON: number");
            return d;
        }

        static string Text(string t, ref int i)
        {
            var sb = new StringBuilder(); i++;
            while (true)
            {
                Bin.Require(i < t.Length, "JSON: unterminated string");
                char c = t[i++];
                if (c == '"') return sb.ToString();
                if (c != '\\') { sb.Append(c); continue; }
                Bin.Require(i < t.Length, "JSON: unterminated string");
                char e = t[i++];
                switch (e)
                {
                    case '"': sb.Append('"'); break;
                    case '\\': sb.Append('\\'); break;
                    case '/': sb.Append('/'); break;
                    case 'b': sb.Append('\b'); break;
                    case 'f': sb.Append('\f'); break;
                    case 'n': sb.Append('\n'); break;
                    case 'r': sb.Append('\r'); break;
                    case 't': sb.Append('\t'); break;
                    case 'u':
                        Bin.Require(i + 4 <= t.Length, "JSON: escape");
                        sb.Append((char)int.Parse(t.Substring(i, 4), NumberStyles.HexNumber, CultureInfo.InvariantCulture)); i += 4; break;
                    default: throw new AliveImportException("JSON: escape");
                }
            }
        }

        static void Str(StringBuilder sb, string s)
        {
            sb.Append('"');
            foreach (char c in s)
            {
                switch (c)
                {
                    case '"': sb.Append("\\\""); break;
                    case '\\': sb.Append("\\\\"); break;
                    case '\n': sb.Append("\\n"); break;
                    case '\r': sb.Append("\\r"); break;
                    case '\t': sb.Append("\\t"); break;
                    case '\b': sb.Append("\\b"); break;
                    case '\f': sb.Append("\\f"); break;
                    default:
                        if (c < 0x20 || c > 0x7E) sb.Append("\\u").Append(((int)c).ToString("x4"));
                        else sb.Append(c);
                        break;
                }
            }
            sb.Append('"');
        }
    }
}

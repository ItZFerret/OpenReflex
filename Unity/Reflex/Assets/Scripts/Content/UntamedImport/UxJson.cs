// MX vs ATV Untamed converter, foundation: JSON exactly as the Python reference writes it (json.dumps and ux_common.dumps_json: key
// order, indent, separators, float repr, ensure_ascii, numpy scalars as plain numbers) and a reader with exactly rounded numbers.
// The value tree is the Alive importer's: JObj (insertion-ordered object), List<object> / arrays, string, long, double, bool, null.
using System;
using System.Collections;
using System.Collections.Generic;
using System.Globalization;
using System.Numerics;
using System.Text;
using Reflex.Port.Content.AliveImport;

namespace Reflex.Port.Content.UntamedImport
{
    public static class UxJson
    {
        /// ux_common.dumps_json(obj, indent): json.dumps(indent=indent, sort_keys=True, allow_nan=False, ensure_ascii=False) + "\n".
        public static string DumpsUx(object v, int indent = 1) => Dumps(v, indent, true, false, false) + "\n";

        /// json.dumps(v, indent=indent, sort_keys=sortKeys, ensure_ascii=ensureAscii, allow_nan=allowNan); indent < 0 = indent=None (one line,
        /// ", " and ": ").
        /// Values: null, bool, string, every integer type and BigInteger, float (written as float(np.float32)), double, JObj, a dictionary with
        /// string or integer keys (integer keys sort numerically under sortKeys, as Python sorts before it converts), byte[] (hex string:
        /// _json_default's bytes rule; pass int[] for a uint8 array), any other IEnumerable (a list; multi-dimensional arrays nest).
        public static string Dumps(object v, int indent = -1, bool sortKeys = false, bool ensureAscii = true, bool allowNan = true)
        {
            var sb = new StringBuilder();
            Write(sb, v, indent, 0, sortKeys, ensureAscii, allowNan);
            return sb.ToString();
        }

        static void Number(StringBuilder sb, double d, bool allowNan)
        {
            if (double.IsNaN(d) || double.IsInfinity(d))
            {
                if (!allowNan) throw new UxException("Out of range float values are not JSON compliant: " + d.ToString(CultureInfo.InvariantCulture));
                sb.Append(double.IsNaN(d) ? "NaN" : d > 0 ? "Infinity" : "-Infinity");
                return;
            }
            sb.Append(PyFormat.Repr(d));
        }

        static void Write(StringBuilder sb, object v, int indent, int depth, bool sort, bool ascii, bool nan)
        {
            switch (v)
            {
                case null: sb.Append("null"); return;
                case bool b: sb.Append(b ? "true" : "false"); return;
                case string s: Str(sb, s, ascii); return;
                case double d: Number(sb, d, nan); return;
                case float f: Number(sb, f, nan); return;
                case int i: sb.Append(i.ToString(CultureInfo.InvariantCulture)); return;
                case long l: sb.Append(l.ToString(CultureInfo.InvariantCulture)); return;
                case uint u: sb.Append(u.ToString(CultureInfo.InvariantCulture)); return;
                case ulong ul: sb.Append(ul.ToString(CultureInfo.InvariantCulture)); return;
                case short sh: sb.Append(sh.ToString(CultureInfo.InvariantCulture)); return;
                case ushort us: sb.Append(us.ToString(CultureInfo.InvariantCulture)); return;
                case byte by: sb.Append(by.ToString(CultureInfo.InvariantCulture)); return;
                case sbyte sby: sb.Append(sby.ToString(CultureInfo.InvariantCulture)); return;
                case BigInteger bi: sb.Append(bi.ToString(CultureInfo.InvariantCulture)); return;
                case byte[] bytes: Str(sb, UxBin.Hex(bytes), ascii); return;
                case JObj o: Object(sb, o.items, indent, depth, sort, ascii, nan); return;
                case IDictionary dict:
                {
                    var items = new List<KeyValuePair<object, object>>();
                    foreach (DictionaryEntry e in dict) items.Add(new KeyValuePair<object, object>(e.Key, e.Value));
                    bool numeric = items.Count > 0 && !(items[0].Key is string);
                    if (sort && numeric) items.Sort((a, c) => Convert.ToInt64(a.Key, CultureInfo.InvariantCulture).CompareTo(Convert.ToInt64(c.Key, CultureInfo.InvariantCulture)));
                    var named = new List<KeyValuePair<string, object>>(items.Count);
                    foreach (var kv in items) named.Add(new KeyValuePair<string, object>(KeyText(kv.Key), kv.Value));
                    Object(sb, named, indent, depth, sort, ascii, nan, numeric);     // numeric keys are in order already; the values below still sort
                    return;
                }
                case Array arr when arr.Rank > 1: Nested(sb, arr, new int[arr.Rank], 0, indent, depth, sort, ascii, nan); return;
                case IEnumerable list:
                {
                    var items = new List<object>(); foreach (var x in list) items.Add(x);
                    List(sb, items, indent, depth, sort, ascii, nan);
                    return;
                }
                default: throw new UxException("not JSON serialisable: " + v.GetType().Name);
            }
        }

        static string KeyText(object k)
        {
            switch (k)
            {
                case string s: return s;
                case bool b: return b ? "true" : "false";
                case null: return "null";
                case double d: return PyFormat.Repr(d);
                case float f: return PyFormat.Repr(f);
                default: return Convert.ToString(k, CultureInfo.InvariantCulture);
            }
        }

        static void Object(StringBuilder sb, List<KeyValuePair<string, object>> source, int indent, int depth, bool sort, bool ascii, bool nan, bool presorted = false)
        {
            var items = source;
            if (sort && !presorted) { items = new List<KeyValuePair<string, object>>(source); UxNp.StableSort(items, (a, c) => string.CompareOrdinal(a.Key, c.Key)); }
            if (items.Count == 0) { sb.Append("{}"); return; }
            sb.Append('{');
            for (int k = 0; k < items.Count; k++)
            {
                if (k > 0) sb.Append(indent < 0 ? ", " : ",");
                if (indent >= 0) { sb.Append('\n'); sb.Append(' ', indent * (depth + 1)); }
                Str(sb, items[k].Key, ascii); sb.Append(": ");
                Write(sb, items[k].Value, indent, depth + 1, sort, ascii, nan);
            }
            if (indent >= 0) { sb.Append('\n'); sb.Append(' ', indent * depth); }
            sb.Append('}');
        }

        static void List(StringBuilder sb, List<object> items, int indent, int depth, bool sort, bool ascii, bool nan)
        {
            if (items.Count == 0) { sb.Append("[]"); return; }
            sb.Append('[');
            for (int k = 0; k < items.Count; k++)
            {
                if (k > 0) sb.Append(indent < 0 ? ", " : ",");
                if (indent >= 0) { sb.Append('\n'); sb.Append(' ', indent * (depth + 1)); }
                Write(sb, items[k], indent, depth + 1, sort, ascii, nan);
            }
            if (indent >= 0) { sb.Append('\n'); sb.Append(' ', indent * depth); }
            sb.Append(']');
        }

        static void Nested(StringBuilder sb, Array arr, int[] index, int dim, int indent, int depth, bool sort, bool ascii, bool nan)
        {
            int n = arr.GetLength(dim);
            if (n == 0) { sb.Append("[]"); return; }
            sb.Append('[');
            for (int k = 0; k < n; k++)
            {
                if (k > 0) sb.Append(indent < 0 ? ", " : ",");
                if (indent >= 0) { sb.Append('\n'); sb.Append(' ', indent * (depth + 1)); }
                index[dim] = k;
                if (dim + 1 < arr.Rank) Nested(sb, arr, index, dim + 1, indent, depth + 1, sort, ascii, nan);
                else Write(sb, arr.GetValue(index), indent, depth + 1, sort, ascii, nan);
            }
            if (indent >= 0) { sb.Append('\n'); sb.Append(' ', indent * depth); }
            sb.Append(']');
        }

        static void Str(StringBuilder sb, string s, bool ascii)
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
                        if (c < 0x20 || ascii && c > 0x7E) sb.Append("\\u").Append(((int)c).ToString("x4", CultureInfo.InvariantCulture));
                        else sb.Append(c);
                        break;
                }
            }
            sb.Append('"');
        }

        // ------------------------------------------------------------------------------------------------ reading

        /// json.loads: JObj (key order kept) / List<object> / string / long (BigInteger beyond 64 bits) / double / bool / null.
        public static object Parse(string text)
        {
            int i = 0;
            if (text.Length > 0 && text[0] == '﻿') i = 1;
            var v = Value(text, ref i);
            Ws(text, ref i);
            UxBin.Require(i == text.Length, "JSON: trailing data at " + i);
            return v;
        }
        public static object ParseUtf8(byte[] data) => Parse(Encoding.UTF8.GetString(data));

        static void Ws(string t, ref int i) { while (i < t.Length && (t[i] == ' ' || t[i] == '\t' || t[i] == '\n' || t[i] == '\r')) i++; }

        static object Value(string t, ref int i)
        {
            Ws(t, ref i);
            UxBin.Require(i < t.Length, "JSON: unexpected end");
            char c = t[i];
            if (c == '{')
            {
                var o = new JObj(); i++; Ws(t, ref i);
                if (i < t.Length && t[i] == '}') { i++; return o; }
                // a fresh object: append (Set would scan every key: quadratic on the big manifests); a repeated key keeps the last value
                Dictionary<string, int> seen = null;
                while (true)
                {
                    Ws(t, ref i); UxBin.Require(i < t.Length && t[i] == '"', "JSON: object key at " + i);
                    string k = Text(t, ref i); Ws(t, ref i);
                    UxBin.Require(i < t.Length && t[i] == ':', "JSON: ':' expected at " + i); i++;
                    object val = Value(t, ref i);
                    if (seen == null) seen = new Dictionary<string, int>(StringComparer.Ordinal);
                    if (seen.TryGetValue(k, out int at)) o.items[at] = new KeyValuePair<string, object>(k, val);
                    else { seen[k] = o.items.Count; o.items.Add(new KeyValuePair<string, object>(k, val)); }
                    Ws(t, ref i);
                    UxBin.Require(i < t.Length, "JSON: unexpected end");
                    if (t[i] == ',') { i++; continue; }
                    UxBin.Require(t[i] == '}', "JSON: '}' expected at " + i); i++; return o;
                }
            }
            if (c == '[')
            {
                var l = new List<object>(); i++; Ws(t, ref i);
                if (i < t.Length && t[i] == ']') { i++; return l; }
                while (true)
                {
                    l.Add(Value(t, ref i)); Ws(t, ref i);
                    UxBin.Require(i < t.Length, "JSON: unexpected end");
                    if (t[i] == ',') { i++; continue; }
                    UxBin.Require(t[i] == ']', "JSON: ']' expected at " + i); i++; return l;
                }
            }
            if (c == '"') return Text(t, ref i);
            if (Word(t, ref i, "true")) return true;
            if (Word(t, ref i, "false")) return false;
            if (Word(t, ref i, "null")) return null;
            if (Word(t, ref i, "NaN")) return double.NaN;
            if (Word(t, ref i, "Infinity")) return double.PositiveInfinity;
            if (Word(t, ref i, "-Infinity")) return double.NegativeInfinity;
            int s = i;
            while (i < t.Length && "+-0123456789.eE".IndexOf(t[i]) >= 0) i++;
            string num = t.Substring(s, i - s);
            UxBin.Require(num.Length > 0, "JSON: unexpected character at " + s);
            if (num.IndexOfAny(new[] { '.', 'e', 'E' }) < 0)
            {
                if (long.TryParse(num, NumberStyles.AllowLeadingSign, CultureInfo.InvariantCulture, out long n)) return n;
                UxBin.Require(BigInteger.TryParse(num, NumberStyles.AllowLeadingSign, CultureInfo.InvariantCulture, out BigInteger big), "JSON: number at " + s);
                return big;
            }
            return ParseDouble(num);
        }

        static bool Word(string t, ref int i, string word)
        {
            if (string.CompareOrdinal(t, i, word, 0, word.Length) != 0) return false;
            i += word.Length; return true;
        }

        static string Text(string t, ref int i)
        {
            var sb = new StringBuilder(); i++;
            while (true)
            {
                UxBin.Require(i < t.Length, "JSON: unterminated string");
                char c = t[i++];
                if (c == '"') return sb.ToString();
                if (c != '\\') { sb.Append(c); continue; }
                UxBin.Require(i < t.Length, "JSON: unterminated string");
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
                        UxBin.Require(i + 4 <= t.Length, "JSON: escape");
                        sb.Append((char)int.Parse(t.Substring(i, 4), NumberStyles.HexNumber, CultureInfo.InvariantCulture)); i += 4; break;
                    default: throw new UxException("JSON: escape");
                }
            }
        }

        /// float(text) for a decimal literal: the nearest double, ties to even, by exact integer arithmetic (the same result on every runtime).
        public static double ParseDouble(string text)
        {
            int p = 0; bool neg = false;
            if (p < text.Length && (text[p] == '+' || text[p] == '-')) { neg = text[p] == '-'; p++; }
            BigInteger m = BigInteger.Zero; int scale = 0, digits = 0; bool point = false;
            var chunk = new StringBuilder();
            for (; p < text.Length; p++)
            {
                char c = text[p];
                if (c >= '0' && c <= '9') { chunk.Append(c); digits++; if (point) scale--; }
                else if (c == '.' && !point) point = true;
                else break;
            }
            UxBin.Require(digits > 0, "not a number: " + text);
            m = BigInteger.Parse(chunk.ToString(), NumberStyles.None, CultureInfo.InvariantCulture);
            if (p < text.Length && (text[p] == 'e' || text[p] == 'E'))
            {
                UxBin.Require(int.TryParse(text.Substring(p + 1), NumberStyles.AllowLeadingSign, CultureInfo.InvariantCulture, out int ex), "not a number: " + text);
                scale += ex; p = text.Length;
            }
            UxBin.Require(p == text.Length, "not a number: " + text);
            double r;
            if (m.IsZero) r = 0;
            else if (scale > 400) r = double.PositiveInfinity;
            else if (scale < -400 - digits) r = 0;
            else
            {
                BigInteger num = m, den = BigInteger.One;
                if (scale >= 0) num *= BigInteger.Pow(10, scale); else den = BigInteger.Pow(10, -scale);
                r = Ratio(num, den);
            }
            return neg ? -r : r;
        }

        static int BitLength(BigInteger v) { int n = 0; var bytes = v.ToByteArray(); int top = bytes.Length - 1; while (top > 0 && bytes[top] == 0) top--; n = top * 8; int b = bytes[top]; while (b != 0) { n++; b >>= 1; } return n; }

        /// num / den (both positive) as the nearest double, ties to even.
        internal static double Ratio(BigInteger num, BigInteger den)
        {
            int k = BitLength(num) - BitLength(den) - 53;          // q = num / den / 2^k has 53 or 54 bits
            BigInteger q, r, d;
            while (true)
            {
                if (k < -1074) k = -1074;
                d = k >= 0 ? den << k : den;
                BigInteger n = k >= 0 ? num : num << -k;
                q = BigInteger.DivRem(n, d, out r);
                if (q >= (BigInteger.One << 53)) { k++; continue; }
                if (q < (BigInteger.One << 52) && k > -1074) { k--; continue; }
                break;
            }
            int c = (2 * r).CompareTo(d);
            long ql = (long)q;
            if (c > 0 || c == 0 && (ql & 1) != 0) ql++;
            if (ql == 1L << 53) { ql >>= 1; k++; }
            if (k + 52 > 1023) return double.PositiveInfinity;
            long bits = ql < (1L << 52) ? ql : ((long)(k + 1075) << 52) | (ql - (1L << 52));
            return BitConverter.Int64BitsToDouble(bits);
        }

        // ------------------------------------------------------------------------------------------------ access helpers

        public static JObj Obj(object v) => v as JObj ?? throw new UxException("JSON: object expected");
        public static List<object> Arr(object v) => v as List<object> ?? throw new UxException("JSON: list expected");
        public static string Str(object v) => v as string ?? throw new UxException("JSON: string expected");
        public static long Int(object v) => v is long l ? l : v is int i ? i : v is bool b ? (b ? 1 : 0) : throw new UxException("JSON: integer expected");
        /// A JSON number as a Python float (an int converts exactly as float(int) for |n| < 2^53).
        public static double Num(object v) => v is double d ? d : v is long l ? l : v is int i ? i : v is float f ? f : throw new UxException("JSON: number expected");
        public static bool Bool(object v) => v is bool b ? b : throw new UxException("JSON: bool expected");
        /// obj[k1][k2]...: null when a key is missing.
        public static object Get(object v, params string[] path)
        {
            foreach (var k in path) { if (!(v is JObj o)) return null; v = o.Get(k); }
            return v;
        }
    }
}

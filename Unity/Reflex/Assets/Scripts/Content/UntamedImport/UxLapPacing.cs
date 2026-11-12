// MX vs ATV Untamed converter, stage "race" (S8): the port of the Python reference converter's lappacing_to_reflex.py. An Untamed
// AI/<Level>-lappacing.bxml becomes a Reflex lap pacing bxml (same BXML version, same schema LapPacing/<Event>0/<Atv|MotocrossBike>/
// MXEngineClass_<cc>/{NormalTime, Pro, Hard, Normal, Easy}): the event key renamed to Supercross0 and an MXEngineClass_500cc row (a copy of
// the 250cc row) for MotocrossBike and Atv. EngineCheck mirrors the core reader (build_activity_race.c:964-1040).
// OPT-IN only (--lappacing untamed, an A/B option): the current builds embed no lap pacing asset, because Untamed's 250cc targets
// over-drive the Reflex AI (reference DESIGN 20). Pure C# (System only).
using System;
using System.Collections.Generic;
using System.Text.RegularExpressions;
using Reflex.Port.Content.AliveImport;

namespace Reflex.Port.Content.UntamedImport
{
    /// lappacing_to_reflex.py.
    public static class UxLapPacing
    {
        public static readonly string[] CLASS_NAMES = { "MotocrossBike", "Atv", "MonsterTruck", "TrophyTruck", "SandRail", "Buggy", "GolfCart", "Rhino" };
        public static readonly string[] SIZE_NAMES = { "50cc", "125cc", "250cc", "500cc" };
        public static readonly string[] TIME_KEYS = { "NormalTime", "Pro", "Hard", "Normal", "Easy" };
        public const string ENGINE_PREFIX = "MXEngineClass_";          // 14 characters: the core reads the size from char 14 on
        static readonly string[] ADD_CLASSES = { "MotocrossBike", "Atv" };

        /// convert(data, event): the event key only ("<X>0" -> "<event>0"; null / "" = no rename). keys: the event keys of the input;
        /// mapping: what was renamed.
        public static byte[] Convert(byte[] data, out List<string> keys, out JObj mapping, string @event = "Supercross")
        {
            var b = new UxBxml(data);
            UxBin.Require(UxBin.Same(b.ToBytes(), data), "BXML re-encode is not byte-exact for this file");
            UxBin.Require(b.Name(0) == "LapPacing", "lap pacing: the root is " + b.Name(0));
            keys = new List<string>(); foreach (int c in b.Children(0)) keys.Add(b.Name(c));
            mapping = new JObj();
            if (!string.IsNullOrEmpty(@event))
                foreach (string k in keys)
                    if (k.EndsWith("0", StringComparison.Ordinal) && k != @event + "0") mapping.Set(k, @event + "0");
            if (mapping.items.Count > 0)
            {
                var m = new Dictionary<string, string>(StringComparer.Ordinal);
                foreach (var kv in mapping.items) m[kv.Key] = (string)kv.Value;
                b.ReplaceStrings(m);
            }
            return b.ToBytes();
        }

        /// to_reflex(data, event, add_500cc): convert(), then the 500cc rows (copies of 250cc) for MotocrossBike and Atv where missing.
        /// info: events_in, renamed, added, source_md5, md5, bytes.
        public static byte[] ToReflex(byte[] data, out JObj info, string @event = "Supercross", bool add_500cc = true, string src_size = "250cc", string dst_size = "500cc")
        {
            var o = Convert(data, out var keys, out var mapping, @event);
            var added = new List<object>(); var ev_in = new List<object>(); foreach (string k in keys) ev_in.Add(k);
            info = new JObj().Set("events_in", ev_in).Set("renamed", mapping).Set("added", added).Set("source_md5", UxBin.Md5(data));
            if (add_500cc)
            {
                var b = new UxBxml(o);
                var t = b.ToTree();
                UxElem ev = null;
                if (!string.IsNullOrEmpty(@event)) { foreach (var c in t.children) if (c.tag == @event + "0") { ev = c; break; } }
                else if (t.children.Count > 0) ev = t.children[0];
                if (ev != null)
                    foreach (var c in ev.children)
                    {
                        if (Array.IndexOf(ADD_CLASSES, c.tag) < 0) continue;
                        int src = -1; bool have = false;
                        for (int i = 0; i < c.children.Count; i++)
                        {
                            if (c.children[i].tag == ENGINE_PREFIX + dst_size) have = true;
                            if (src < 0 && c.children[i].tag == ENGINE_PREFIX + src_size) src = i;
                        }
                        if (have || src < 0) continue;
                        var nw = c.children[src].DeepCopy();
                        nw.tag = ENGINE_PREFIX + dst_size;
                        c.children.Add(nw);
                        added.Add(ev.tag + "/" + c.tag + "/" + nw.tag + " (copy of " + c.children[src].tag + ")");
                    }
                if (added.Count > 0) o = UxBxml.EncodeTree(t, b.version, b.unknown);
            }
            info.Set("md5", UxBin.Md5(o)).Set("bytes", (long)o.Length);
            return o;
        }

        static readonly Regex FloatText = new Regex(@"\A[+-]?(?:\d+(?:_\d+)*\.?(?:\d+(?:_\d+)*)?|\.\d+(?:_\d+)*)(?:[eE][+-]?\d+(?:_\d+)*)?\z", RegexOptions.CultureInvariant);
        /// float(text.strip()) for the texts a lap pacing file holds: decimal literals, inf / infinity / nan. false = ValueError.
        static bool PyFloat(string text, out double v)
        {
            string s = text.Trim(' ', '\t', '\n', '\r', '\f', '\v');
            string low = s.ToLowerInvariant().TrimStart('+', '-');
            bool neg = s.StartsWith("-", StringComparison.Ordinal);
            if (low == "inf" || low == "infinity") { v = neg ? double.NegativeInfinity : double.PositiveInfinity; return true; }
            if (low == "nan") { v = UxNp.NaN; return true; }
            if (!FloatText.IsMatch(s)) { v = 0; return false; }
            v = UxJson.ParseDouble(s.Replace("_", ""));
            return true;
        }

        /// engine_check(data, type_name, area): the port of the core reader. Returns the table {class: {size: [NormalTime, Pro, Hard,
        /// Normal, Easy]}} (insertion order) and the problems (empty = every row lands in its own slot of the 8 x 4 table with finite
        /// times > 0).
        public static JObj EngineCheck(byte[] data, out List<string> probs, string type_name = "Supercross", int area = 0)
        {
            probs = new List<string>(); var table = new JObj();
            UxElem t;
            try { t = new UxBxml(data).ToTree(); }
            catch (Exception ex) { probs.Add("not a BXML document: " + ex.Message); return new JObj(); }
            string ttl = type_name + area.ToString(System.Globalization.CultureInfo.InvariantCulture);
            UxElem node = null; foreach (var c in t.children) if (c.tag == ttl) { node = c; break; }      // strcmp: exact
            if (node == null)
            {
                var have = new List<object>(); foreach (var c in t.children) have.Add(c.tag);
                probs.Add("no \"" + ttl + "\" node under the root (have " + UxRaceNum.PyRepr(have) + "): the pace table stays 0");
                return new JObj();
            }
            var seen = new HashSet<int>();
            foreach (var c in node.children)
            {
                int cls = 8; for (int k = 0; k < CLASS_NAMES.Length; k++) if (UxCommon.PyLower(CLASS_NAMES[k]) == UxCommon.PyLower(c.tag)) { cls = k; break; }
                if (cls == 8) { probs.Add("class node " + UxRaceNum.PyRepr(c.tag) + " is not a FactoryName (class 8: written outside the 8 x 4 table)"); continue; }
                foreach (var e in c.children)
                {
                    string sz_name = e.tag.Length > 14 ? e.tag.Substring(14) : "";
                    int size = 4; for (int k = 0; k < SIZE_NAMES.Length; k++) if (UxCommon.PyLower(SIZE_NAMES[k]) == UxCommon.PyLower(sz_name)) { size = k; break; }
                    if (size == 4) { probs.Add(c.tag + "/" + e.tag + ": size " + UxRaceNum.PyRepr(sz_name) + " unknown (index 4 aliases the next class)"); continue; }
                    if (seen.Contains(cls * 4 + size)) probs.Add(c.tag + "/" + e.tag + ": second row for the same slot (the later one wins)");
                    seen.Add(cls * 4 + size);
                    var vals = new List<object>();
                    foreach (string k in TIME_KEYS)
                    {
                        UxElem x = null; foreach (var y in e.children) if (y.tag == k) { x = y; break; }
                        if (x == null || x.vtype != 1 || x.value == null) { probs.Add(c.tag + "/" + e.tag + ": no " + k + " text"); vals.Add(null); continue; }
                        if (!PyFloat(x.value as string ?? "", out double v)) { probs.Add(c.tag + "/" + e.tag + ": " + k + " " + UxRaceNum.PyRepr(x.value) + " is not a number"); vals.Add(null); continue; }
                        if (!(UxNp.Finite(v) && v > 0)) probs.Add(c.tag + "/" + e.tag + ": " + k + " = " + UxRaceNum.PyRepr(v));
                        vals.Add(v);
                    }
                    var row = table.Get(CLASS_NAMES[cls]) as JObj;
                    if (row == null) { row = new JObj(); table.Set(CLASS_NAMES[cls], row); }
                    row.Set(SIZE_NAMES[size], vals);
                }
            }
            return table;
        }
    }
}

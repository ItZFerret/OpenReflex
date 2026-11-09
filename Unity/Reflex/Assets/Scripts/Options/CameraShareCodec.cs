// CAMERA SHARING (w-camshare; a port addition after PHYSICS STUDIO's share codes, OpenFrontEnd/PhysicsStudio.cs): the camera configs' format, share codes
// and the section registry. No UnityEngine here: tests/camshare/codec_test.sh runs it on the SDK's dotnet.
//   CameraShareParam    one value of a section: its key (what the JSON carries), label, kind (number, whole number, switch, choice), default and range
//   CameraShareSection  a keyed, separately versioned group of values: "chase" (one of MY CAMERAS: ReflexCameraShare.cs) and "fpv" (the Dynamic FPV
//                       settings: ReflexFpvShare.cs registers today's). A section's owner registers it with CameraShare.Register; the codec reads and
//                       writes whatever is registered, so a section's parameters change without touching this file
//   CameraConfig        a named config (name, author, notes, the game version) holding one or more sections; JSON (format "open-reflex-camera",
//                       envelope version 1) and the share code "ORCAM1:" + base64url(deflate(the compact JSON)), as ORPHYS1
// Reading is forgiving where it is safe and strict where it is not:
//   - an unknown section, or a key a section does not register, is ignored (counted: the UI says OTHER VERSION)
//   - a registered key that is missing (or not a finite number / true / false) takes the parameter's default (counted)
//   - a value is clamped to its range and rounded for whole numbers, switches and choices
//   - a section written by an older version of it goes through the section's Upgrade first (when it has one); a newer one is read key by key (flagged)
//   - refused: not JSON, another format, a newer envelope, too deep, too large (MaxJson / MaxCode / the inflated size), no section this build knows
using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Text;

namespace Reflex.Port.Options
{
    public enum CameraShareKind { Number, Whole, Switch, Choice }

    public sealed class CameraShareParam
    {
        public string Key, Label, Unit = "";
        public CameraShareKind Kind;
        public double Default, Min, Max;
        /// A Choice's names (its values are 0 .. Choices.Length - 1).
        public string[] Choices;

        public static CameraShareParam Number(string key, string label, double def, double min, double max, string unit = "") =>
            new CameraShareParam { Key = key, Label = label, Kind = CameraShareKind.Number, Default = def, Min = min, Max = max, Unit = unit ?? "" };
        public static CameraShareParam Whole(string key, string label, int def, int min, int max, string unit = "") =>
            new CameraShareParam { Key = key, Label = label, Kind = CameraShareKind.Whole, Default = def, Min = min, Max = max, Unit = unit ?? "" };
        public static CameraShareParam Switch(string key, string label, bool def) =>
            new CameraShareParam { Key = key, Label = label, Kind = CameraShareKind.Switch, Default = def ? 1 : 0, Min = 0, Max = 1 };
        public static CameraShareParam Choice(string key, string label, int def, params string[] choices) =>
            new CameraShareParam { Key = key, Label = label, Kind = CameraShareKind.Choice, Default = def, Min = 0, Max = Math.Max(0, choices.Length - 1), Choices = choices };

        /// The value made legal: finite (else the default), within the range, whole for every kind but Number (a float there).
        public double Clean(double v)
        {
            if (double.IsNaN(v) || double.IsInfinity(v)) v = Default;
            if (Kind != CameraShareKind.Number) v = Math.Round(v, MidpointRounding.AwayFromZero);
            v = v < Min ? Min : v > Max ? Max : v;
            return Kind == CameraShareKind.Number ? (double)(float)v : v;   // (the cameras hold floats: a value is float-exact, so it survives the text bit for bit)
        }

        /// The value as the UI shows it (labels' summaries).
        public string Format(double v)
        {
            switch (Kind)
            {
                case CameraShareKind.Switch: return v >= 0.5 ? "ON" : "OFF";
                case CameraShareKind.Choice: int i = (int)Clean(v); return Choices != null && i >= 0 && i < Choices.Length ? Choices[i] : i.ToString(CultureInfo.InvariantCulture);
                case CameraShareKind.Whole: return ((long)v).ToString(CultureInfo.InvariantCulture) + Unit;
                default: return v.ToString("0.###", CultureInfo.InvariantCulture) + Unit;
            }
        }
    }

    public sealed class CameraShareSection
    {
        public readonly string Id;
        /// This build's version of the section. Bump it when a key's meaning changes (and give Upgrade the old meaning); adding or dropping keys
        /// needs no bump (unknown keys are ignored, missing keys take defaults).
        public readonly int Version;
        public string Label;
        public readonly List<CameraShareParam> Params;
        /// Values written by an older version (fromVersion < Version) into this version's keys; null: the keys are read as they are.
        public Func<int, Dictionary<string, double>, Dictionary<string, double>> Upgrade;
        /// The values in use now (a section that is one global set, as the Dynamic FPV settings); null for a per-item section (chase presets).
        public Func<Dictionary<string, double>> Read;
        /// Makes the values the ones in use (at once where the camera allows); null for a per-item section.
        public Action<Dictionary<string, double>> Apply;

        public CameraShareSection(string id, int version, string label, IEnumerable<CameraShareParam> parms)
        {
            if (string.IsNullOrEmpty(id)) throw new ArgumentException("a section needs an id");
            Id = id; Version = Math.Max(1, version); Label = label ?? id;
            Params = (parms ?? Enumerable.Empty<CameraShareParam>()).Where(p => p != null && !string.IsNullOrEmpty(p.Key)).GroupBy(p => p.Key, StringComparer.Ordinal).Select(g => g.First()).ToList();
        }

        public CameraShareParam Param(string key) { foreach (var p in Params) if (string.Equals(p.Key, key, StringComparison.Ordinal)) return p; return null; }

        /// Every parameter at its default.
        public Dictionary<string, double> Defaults()
        {
            var d = new Dictionary<string, double>(StringComparer.Ordinal);
            foreach (var p in Params) d[p.Key] = p.Clean(p.Default);
            return d;
        }

        /// values completed and cleaned: every registered key (missing ones at their default), nothing else.
        public Dictionary<string, double> Complete(IDictionary<string, double> values, out int missing, out int unknown)
        {
            var d = new Dictionary<string, double>(StringComparer.Ordinal);
            missing = 0; unknown = 0;
            foreach (var p in Params)
            {
                if (values != null && values.TryGetValue(p.Key, out double v) && !double.IsNaN(v) && !double.IsInfinity(v)) d[p.Key] = p.Clean(v);
                else { d[p.Key] = p.Clean(p.Default); missing++; }
            }
            if (values != null) foreach (var k in values.Keys) if (Param(k) == null) unknown++;
            return d;
        }
    }

    /// The registry: one section per id. Register replaces (the owner of a section's parameters: the FPV rewrite registers its own "fpv");
    /// RegisterDefault only fills an empty id (the built-in definitions), so the owner wins whichever registers first.
    public static class CameraShare
    {
        public const string ChaseId = "chase", FpvId = "fpv";
        static readonly List<CameraShareSection> s_sections = new List<CameraShareSection>();
        static readonly object s_lock = new object();

        /// A section was registered or replaced (its id).
        public static event Action<string> Registered;

        public static void Register(CameraShareSection section)
        {
            if (section == null) return;
            lock (s_lock)
            {
                int at = s_sections.FindIndex(s => s.Id == section.Id);
                if (at >= 0) s_sections[at] = section; else s_sections.Add(section);
            }
            Registered?.Invoke(section.Id);
        }

        public static bool RegisterDefault(CameraShareSection section)
        {
            if (section == null) return false;
            lock (s_lock) { if (s_sections.Any(s => s.Id == section.Id)) return false; }
            Register(section);
            return true;
        }

        public static void Unregister(string id) { lock (s_lock) s_sections.RemoveAll(s => s.Id == id); }

        public static CameraShareSection Section(string id) { lock (s_lock) return s_sections.FirstOrDefault(s => s.Id == id); }

        public static IReadOnlyList<CameraShareSection> Sections { get { lock (s_lock) return s_sections.ToList(); } }
    }

    /// One section's values in a config.
    public sealed class CameraSectionData
    {
        public string Id;
        /// The version it was written with (a config made here: the section's own).
        public int Version;
        /// Every key the section registers, cleaned (missing ones at their default).
        public Dictionary<string, double> Values = new Dictionary<string, double>(StringComparer.Ordinal);
        /// Read from a code: keys the section does not register / registered keys the code did not carry.
        public int Unknown, Missing;
        /// Written by a newer version of the section than this build's.
        public bool Newer;

        public double Get(string key, double fallback) => Values.TryGetValue(key, out double v) ? v : fallback;
    }

    public sealed class CameraConfig
    {
        public const string Format = "open-reflex-camera";
        public const int EnvelopeVersion = 1;
        public const string SharePrefix = "ORCAM1:";
        /// Limits: the JSON text, a share code's characters, the inflated code, sections per config, keys per section, nesting.
        public const int MaxJson = 64 << 10, MaxCode = 16 << 10, MaxSections = 16, MaxKeys = 256, MaxDepth = 8;
        public const int MaxName = 40, MaxAuthor = 40, MaxNotes = 400;

        /// The file name without .json in the library; "" not saved.
        public string id = "";
        public string name = "Camera", author = "", description = "", game = "";
        /// The sections this build knows, in the registry's order.
        public readonly List<CameraSectionData> Sections = new List<CameraSectionData>();
        /// Sections a read code carried that this build does not know (ignored).
        public int UnknownSections;

        public CameraSectionData Section(string sectionId) => Sections.FirstOrDefault(s => s.Id == sectionId);
        public bool Has(string sectionId) => Section(sectionId) != null;
        /// Read from a code made by another build of a section (or with sections / keys this one does not know).
        public bool OtherVersion => UnknownSections > 0 || Sections.Any(s => s.Unknown > 0 || s.Newer || s.Missing > 0);

        /// Adds (or replaces) a section's values, completed and cleaned against the registered section.
        public CameraSectionData Put(string sectionId, IDictionary<string, double> values)
        {
            var sec = CameraShare.Section(sectionId) ?? throw new InvalidOperationException("no camera section '" + sectionId + "' is registered");
            var d = new CameraSectionData { Id = sec.Id, Version = sec.Version, Values = sec.Complete(values, out _, out _) };
            int at = Sections.FindIndex(s => s.Id == sec.Id);
            if (at >= 0) Sections[at] = d; else Sections.Add(d);
            return d;
        }

        public CameraConfig Clone()
        {
            var c = new CameraConfig { id = id, name = name, author = author, description = description, game = game, UnknownSections = UnknownSections };
            foreach (var s in Sections)
                c.Sections.Add(new CameraSectionData { Id = s.Id, Version = s.Version, Values = new Dictionary<string, double>(s.Values, StringComparer.Ordinal), Unknown = s.Unknown, Missing = s.Missing, Newer = s.Newer });
            return c;
        }

        /// Whether a section's values equal these (within a hair: the floats went through text).
        public static bool SameValues(IDictionary<string, double> a, IDictionary<string, double> b)
        {
            if (a == null || b == null || a.Count != b.Count) return false;
            foreach (var kv in a) if (!b.TryGetValue(kv.Key, out double v) || Math.Abs(v - kv.Value) > 1e-4 * Math.Max(1.0, Math.Abs(v))) return false;
            return true;
        }

        // ------------------------------------------------------------------------------------------------ JSON

        public string ToJson(bool indent = true)
        {
            var sb = new StringBuilder();
            string nl = indent ? "\n" : "", i1 = indent ? "  " : "", i2 = indent ? "    " : "", i3 = indent ? "      " : "", i4 = indent ? "        " : "", sp = indent ? " " : "";
            sb.Append('{').Append(nl);
            void Field(string k, string v) { sb.Append(i1); Json.Quote(sb, k); sb.Append(':').Append(sp); Json.Quote(sb, v ?? ""); sb.Append(',').Append(nl); }
            sb.Append(i1); Json.Quote(sb, "format"); sb.Append(':').Append(sp); Json.Quote(sb, Format); sb.Append(',').Append(nl);
            sb.Append(i1); Json.Quote(sb, "version"); sb.Append(':').Append(sp).Append(EnvelopeVersion.ToString(CultureInfo.InvariantCulture)).Append(',').Append(nl);
            Field("name", name); Field("author", author); Field("description", description); Field("game", game);
            sb.Append(i1); Json.Quote(sb, "sections"); sb.Append(':').Append(sp).Append('{').Append(nl);
            for (int s = 0; s < Sections.Count; s++)
            {
                var sec = Sections[s];
                var reg = CameraShare.Section(sec.Id);
                sb.Append(i2); Json.Quote(sb, sec.Id); sb.Append(':').Append(sp).Append('{').Append(nl);
                sb.Append(i3); Json.Quote(sb, "version"); sb.Append(':').Append(sp).Append(sec.Version.ToString(CultureInfo.InvariantCulture)).Append(',').Append(nl);
                sb.Append(i3); Json.Quote(sb, "values"); sb.Append(':').Append(sp).Append('{').Append(nl);
                // the registered order first (readable files), then anything else the section data holds
                var keys = (reg != null ? reg.Params.Select(p => p.Key).Where(sec.Values.ContainsKey) : Enumerable.Empty<string>())
                           .Concat(sec.Values.Keys.OrderBy(k => k, StringComparer.Ordinal)).Distinct(StringComparer.Ordinal).ToList();
                for (int k = 0; k < keys.Count; k++)
                {
                    sb.Append(i4); Json.Quote(sb, keys[k]); sb.Append(':').Append(sp);
                    Json.Number(sb, sec.Values[keys[k]], reg?.Param(keys[k])?.Kind == CameraShareKind.Number);
                    if (k + 1 < keys.Count) sb.Append(',');
                    sb.Append(nl);
                }
                sb.Append(i3).Append('}').Append(nl);
                sb.Append(i2).Append('}');
                if (s + 1 < Sections.Count) sb.Append(',');
                sb.Append(nl);
            }
            sb.Append(i1).Append('}').Append(nl);
            sb.Append('}');
            if (indent) sb.Append('\n');
            return sb.ToString();
        }

        /// Reads a config (see the file's head for what is checked). Throws FormatException for what is not one.
        public static CameraConfig FromJson(string json)
        {
            if (string.IsNullOrWhiteSpace(json)) throw new FormatException("empty");
            json = json.Trim().TrimStart('\uFEFF');
            if (json.Length > MaxJson) throw new FormatException("too large");
            object parsed;
            try { parsed = Json.Parse(json, MaxDepth); }
            catch (FormatException e) { throw new FormatException("not JSON (" + e.Message + ")"); }
            if (!(parsed is Dictionary<string, object> o)) throw new FormatException("not a camera config");
            if (!(o.TryGetValue("format", out var f) && f is string fs && fs == Format)) throw new FormatException("not an OPEN REFLEX camera config");
            if (o.TryGetValue("version", out var ver) && ver is double vd && vd > EnvelopeVersion) throw new FormatException("made by a newer OPEN REFLEX (version " + vd.ToString(CultureInfo.InvariantCulture) + ")");
            var c = new CameraConfig
            {
                name = CleanText(Str(o, "name"), MaxName), author = CleanText(Str(o, "author"), MaxAuthor),
                description = CleanText(Str(o, "description"), MaxNotes, true), game = CleanText(Str(o, "game"), 40),
            };
            if (c.name.Length == 0) c.name = "Imported camera";
            if (o.TryGetValue("sections", out var so) && so is Dictionary<string, object> sections)
            {
                int n = 0;
                foreach (var kv in sections)
                {
                    if (++n > MaxSections) { c.UnknownSections++; continue; }
                    var reg = CameraShare.Section(kv.Key);
                    if (reg == null || !(kv.Value is Dictionary<string, object> sd)) { c.UnknownSections++; continue; }
                    c.Sections.RemoveAll(s => s.Id == reg.Id);
                    c.Sections.Add(ReadSection(reg, sd));
                }
            }
            if (c.Sections.Count == 0) throw new FormatException(c.UnknownSections > 0 ? "made for camera settings this version does not have" : "no camera settings in it");
            // the registry's order
            var order = CameraShare.Sections.Select(s => s.Id).ToList();
            c.Sections.Sort((a, b) => order.IndexOf(a.Id).CompareTo(order.IndexOf(b.Id)));
            return c;
        }

        static CameraSectionData ReadSection(CameraShareSection reg, Dictionary<string, object> sd)
        {
            int version = 1;
            if (sd.TryGetValue("version", out var vo) && vo is double vv && vv >= 1 && vv <= int.MaxValue) version = (int)Math.Floor(vv);
            var raw = new Dictionary<string, double>(StringComparer.Ordinal);
            int unknown = 0;
            if (sd.TryGetValue("values", out var valo) && valo is Dictionary<string, object> vals)
            {
                int n = 0;
                foreach (var kv in vals)
                {
                    if (++n > MaxKeys) { unknown++; continue; }
                    if (kv.Value is double d) { if (!double.IsNaN(d) && !double.IsInfinity(d)) raw[kv.Key] = d; }
                    else if (kv.Value is bool b) raw[kv.Key] = b ? 1 : 0;
                    else if (reg.Param(kv.Key) == null) unknown++;
                    // (text, null, arrays, objects: not a value; a registered key then takes its default, another one is unknown)
                }
            }
            bool newer = version > reg.Version;
            if (version < reg.Version && reg.Upgrade != null)
            {
                try { raw = reg.Upgrade(version, raw) ?? raw; } catch (Exception) { /* (an upgrade that fails: the keys as they are) */ }
            }
            var data = new CameraSectionData { Id = reg.Id, Version = reg.Version, Newer = newer };
            data.Values = reg.Complete(raw, out data.Missing, out int extra);
            data.Unknown = unknown + extra;
            return data;
        }

        static string Str(Dictionary<string, object> o, string k) => o.TryGetValue(k, out var v) && v is string s ? s : "";

        /// Text from a code: no control characters (notes keep line breaks), trimmed, clipped.
        public static string CleanText(string s, int max, bool lines = false)
        {
            var sb = new StringBuilder();
            foreach (char ch in s ?? "")
            {
                if (ch == '\n' && lines) sb.Append('\n');
                else if (char.IsControl(ch) || char.IsSurrogate(ch) || ch == '\uFEFF') continue;
                else sb.Append(ch);
                if (sb.Length >= max) break;
            }
            return sb.ToString().Trim();
        }

        // ------------------------------------------------------------------------------------------------ share code

        public string ShareCode()
        {
            byte[] raw = Encoding.UTF8.GetBytes(ToJson(false));
            using var ms = new MemoryStream();
            using (var z = new DeflateStream(ms, CompressionLevel.Optimal, true)) z.Write(raw, 0, raw.Length);
            return SharePrefix + Convert.ToBase64String(ms.ToArray()).TrimEnd('=').Replace('+', '-').Replace('/', '_');
        }

        /// A share code found anywhere in text (a chat line), or a config's JSON pasted as text. Null with error otherwise.
        public static CameraConfig FromShareText(string text, out string error)
        {
            error = null;
            text = (text ?? "").Trim();
            try
            {
                if (text.Length > 4 * MaxCode) throw new FormatException("the text is too long to hold a camera code");
                int at = text.IndexOf(SharePrefix, StringComparison.OrdinalIgnoreCase);
                if (at >= 0)
                {
                    var b = new StringBuilder();
                    for (int i = at + SharePrefix.Length; i < text.Length; i++)
                    {
                        char ch = text[i];
                        if (!(ch >= 'A' && ch <= 'Z' || ch >= 'a' && ch <= 'z' || ch >= '0' && ch <= '9' || ch == '-' || ch == '_' || ch == '+' || ch == '/' || ch == '=')) break;
                        b.Append(ch);
                        if (b.Length > MaxCode) throw new FormatException("the code is too long");
                    }
                    if (b.Length == 0) throw new FormatException("the code is empty");
                    string s = b.ToString().TrimEnd('=').Replace('-', '+').Replace('_', '/');
                    if (s.Length % 4 == 1) throw new FormatException("the code is cut short");
                    s += new string('=', (4 - s.Length % 4) % 4);
                    byte[] data;
                    try { data = Convert.FromBase64String(s); } catch (FormatException) { throw new FormatException("the code is damaged"); }
                    string json;
                    try
                    {
                        using var z = new DeflateStream(new MemoryStream(data), CompressionMode.Decompress);
                        using var outp = new MemoryStream();
                        var buf = new byte[4096];
                        int n, total = 0;
                        while ((n = z.Read(buf, 0, buf.Length)) > 0) { total += n; if (total > MaxJson) throw new FormatException("the code unpacks too large"); outp.Write(buf, 0, n); }
                        json = new UTF8Encoding(false, true).GetString(outp.ToArray());
                        if (json.Length == 0) throw new FormatException("the code is damaged");
                    }
                    catch (FormatException) { throw; }
                    catch (Exception) { throw new FormatException("the code is damaged"); }
                    return FromJson(json);
                }
                if (text.StartsWith("{", StringComparison.Ordinal)) return FromJson(text);
                error = "No camera share code found (it starts with " + SharePrefix + ")";
            }
            catch (FormatException e) { error = "Not a valid camera config: " + e.Message; }
            catch (Exception e) { error = "Not a valid camera config: " + e.GetType().Name; }
            return null;
        }

        // ------------------------------------------------------------------------------------------------ a small JSON reader / writer
        // (objects Dictionary<string, object>, arrays List<object>, numbers double; the nesting is limited while reading)
        internal static class Json
        {
            public static object Parse(string s, int maxDepth)
            {
                int i = 0;
                var v = Value(s, ref i, 0, maxDepth);
                Ws(s, ref i);
                if (i != s.Length) throw new FormatException("text after the value at " + i);
                return v;
            }
            static void Ws(string s, ref int i) { while (i < s.Length && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r')) i++; }
            static object Value(string s, ref int i, int depth, int maxDepth)
            {
                Ws(s, ref i);
                if (i >= s.Length) throw new FormatException("end of input");
                char c = s[i];
                if (c == '{' || c == '[')
                {
                    if (depth + 1 > maxDepth) throw new FormatException("nested too deeply");
                    if (c == '{')
                    {
                        var d = new Dictionary<string, object>(StringComparer.Ordinal); i++;
                        Ws(s, ref i); if (i < s.Length && s[i] == '}') { i++; return d; }
                        while (true)
                        {
                            Ws(s, ref i); string k = Str(s, ref i); Ws(s, ref i);
                            if (i >= s.Length || s[i] != ':') throw new FormatException("':' expected at " + i);
                            i++; d[k] = Value(s, ref i, depth + 1, maxDepth); Ws(s, ref i);
                            if (i < s.Length && s[i] == ',') { i++; continue; }
                            if (i < s.Length && s[i] == '}') { i++; return d; }
                            throw new FormatException("',' or '}' expected at " + i);
                        }
                    }
                    var l = new List<object>(); i++;
                    Ws(s, ref i); if (i < s.Length && s[i] == ']') { i++; return l; }
                    while (true)
                    {
                        l.Add(Value(s, ref i, depth + 1, maxDepth)); Ws(s, ref i);
                        if (i < s.Length && s[i] == ',') { i++; continue; }
                        if (i < s.Length && s[i] == ']') { i++; return l; }
                        throw new FormatException("',' or ']' expected at " + i);
                    }
                }
                if (c == '"') return Str(s, ref i);
                if (string.CompareOrdinal(s, i, "true", 0, 4) == 0) { i += 4; return true; }
                if (string.CompareOrdinal(s, i, "false", 0, 5) == 0) { i += 5; return false; }
                if (string.CompareOrdinal(s, i, "null", 0, 4) == 0) { i += 4; return null; }
                int st = i;
                while (i < s.Length && (s[i] >= '0' && s[i] <= '9' || s[i] == '-' || s[i] == '+' || s[i] == '.' || s[i] == 'e' || s[i] == 'E')) i++;
                if (i == st || i - st > 40) throw new FormatException("value expected at " + st);
                if (!double.TryParse(s.Substring(st, i - st), NumberStyles.Float, CultureInfo.InvariantCulture, out double v)) throw new FormatException("bad number at " + st);
                return v;
            }
            static string Str(string s, ref int i)
            {
                if (i >= s.Length || s[i] != '"') throw new FormatException("string expected at " + i);
                i++; var sb = new StringBuilder();
                while (true)
                {
                    if (i >= s.Length) throw new FormatException("unterminated string");
                    char c = s[i++];
                    if (c == '"') return sb.ToString();
                    if (c != '\\') { sb.Append(c); continue; }
                    if (i >= s.Length) throw new FormatException("unterminated string");
                    char e = s[i++];
                    switch (e)
                    {
                        case 'n': sb.Append('\n'); break; case 'r': sb.Append('\r'); break; case 't': sb.Append('\t'); break;
                        case 'b': sb.Append('\b'); break; case 'f': sb.Append('\f'); break;
                        case '"': case '\\': case '/': sb.Append(e); break;
                        case 'u':
                            if (i + 4 > s.Length || !int.TryParse(s.Substring(i, 4), NumberStyles.HexNumber, CultureInfo.InvariantCulture, out int u)) throw new FormatException("bad \\u escape at " + i);
                            sb.Append((char)u); i += 4; break;
                        default: throw new FormatException("bad escape at " + i);
                    }
                }
            }

            public static void Quote(StringBuilder sb, string s)
            {
                sb.Append('"');
                foreach (char c in s ?? "")
                {
                    switch (c)
                    {
                        case '"': sb.Append("\\\""); break; case '\\': sb.Append("\\\\"); break;
                        case '\n': sb.Append("\\n"); break; case '\r': sb.Append("\\r"); break; case '\t': sb.Append("\\t"); break;
                        default: if (c < 0x20 || c == '\u2028' || c == '\u2029') sb.Append("\\u").Append(((int)c).ToString("x4", CultureInfo.InvariantCulture)); else sb.Append(c); break;
                    }
                }
                sb.Append('"');
            }

            public static void Number(StringBuilder sb, double v, bool fraction)
            {
                if (double.IsNaN(v) || double.IsInfinity(v)) v = 0;
                if (!fraction && v == Math.Floor(v) && Math.Abs(v) < 1e15) sb.Append(((long)v).ToString(CultureInfo.InvariantCulture));
                else if (Math.Abs(v) < 3e38 && (double)(float)v == v) sb.Append(((float)v).ToString("R", CultureInfo.InvariantCulture));   // (the shortest text of a float)
                else sb.Append(v.ToString("R", CultureInfo.InvariantCulture));
            }
        }
    }
}

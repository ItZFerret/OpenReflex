// PHYSICS STUDIO (port addition, after Open Unleashed's Physics Studio: MxaPhysicsConfig / MxaPhysicsTuning): the configs of the
// studio page (OpenShell.PhysicsStudio.cs).
//   PhysicsCatalog  the core's catalog (rx_phys_catalog): every registered parameter of the nine vehicle parameter files, each
//                   component with its default (the file's value), bounds and unit, and the terrain grip / drag tables
//   PhysicsConfig   a named set of absolute values (vehicle / parameter / component), grip and drag materials and two scope switches;
//                   JSON (format "open-reflex-physics" 1), a share code "ORPHYS1:" + base64url(deflate(the JSON)), the core text
//   PhysicsLibrary  the player's configs (JSON files in the client's settings folder: ReflexSave.Folder/Physics), the built-in presets,
//                   the active config (ReflexSave "Physics.Active") and what the next event gets (RxPhysicsStudio.SetPending)
// "Original" is no values at all: the core is then not touched (bit for bit the original game, core/plan/PHYSICS_STUDIO.md).
using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Text;
using Reflex.Port.Options;
using Reflex.Port.RxCore;
using UnityEngine;

namespace Reflex.Port.OpenFrontEnd
{
    public sealed class PhysicsCatalog
    {
        public sealed class Comp { public int index; public string label, unit; public float def, min, max; }
        /// What the core's N lines say of one parameter name (the classes share them): a player-friendly label, a one-line description,
        /// the unit (of a curve: its y; UnitX its x) and whether the value is a 0 / 1 switch. "(unknown)" marks what the port could not read.
        public sealed class Info { public string name, label, unit = "", unitX = "", desc = ""; public bool toggle; }
        public sealed class Param
        {
            public int cls, index, kind; public string name, group; public bool inFile; public Info info;
            public readonly List<Comp> comps = new List<Comp>();
            /// The friendly label ("Rear spring rate"); the file's name spaced for reading when the core sent none.
            public string Label => info != null && info.label.Length > 0 ? info.label : Pretty(name).Replace(" · ", " ");
            public string Desc => info?.desc ?? "";
            public bool Toggle => info != null && info.toggle;
            /// Everything the search looks in, lower case: the raw name (also spaced), label, description, group and unit.
            string _hay;
            string Hay => _hay ?? (_hay = (name + " " + Pretty(name).Replace(" · ", " ") + " " + Label + " " + Desc + " " + group + " " + (info?.unit ?? "") + " " + (info?.unitX ?? "")).ToLowerInvariant());
            /// True when every word of the query (any case) appears in the name, label, description, group or unit.
            public bool Matches(string query)
            {
                if (string.IsNullOrWhiteSpace(query)) return true;
                string hay = Hay;
                foreach (var w in query.ToLowerInvariant().Split(new[] { ' ', '\t' }, StringSplitOptions.RemoveEmptyEntries))
                    if (hay.IndexOf(w, StringComparison.Ordinal) < 0 && hay.Replace(" ", "").IndexOf(w, StringComparison.Ordinal) < 0) return false;
                return true;
            }
            /// A component's unit as shown next to its number ("" for "(unknown)").
            public static string ShortUnit(string unit) => unit == "(unknown)" ? "" : unit ?? "";
            /// The side-panel text of a value: what it does, its unit, the raw name (share codes use it), the file's value and the range.
            public string Describe(Comp c)
            {
                string u = string.IsNullOrEmpty(c.unit) ? (info != null && info.unit == "(unknown)" ? "(unknown)" : "none") : c.unit;
                var sb = new StringBuilder();
                if (Desc.Length > 0) sb.Append(Desc).Append('\n');
                sb.Append("UNIT ").Append(u).Append("  ·  FILE VALUE ").Append(Num(c.def)).Append(' ').Append(ShortUnit(c.unit)).Append("  ·  RANGE ").Append(Num(c.min)).Append(" .. ").Append(Num(c.max));
                sb.Append('\n').Append("RAW NAME ").Append(name).Append(" [").Append(c.label).Append(']');
                return sb.ToString();
            }
        }
        public sealed class VehicleClass { public int index; public string id, file, label; public bool live; public readonly List<Param> parms = new List<Param>(); }
        public sealed class Material { public int index; public string label; public float def, min, max; }

        public string Hash = "";
        /// Label / description / unit per raw parameter name (the core's N lines); empty from a core that has none.
        public readonly Dictionary<string, Info> Infos = new Dictionary<string, Info>(StringComparer.Ordinal);
        public readonly List<VehicleClass> Classes = new List<VehicleClass>();
        public readonly List<Material> Grip = new List<Material>(), Drag = new List<Material>();
        public int Values;

        public static readonly string[] Groups = { "ENGINE & POWER", "GEARING", "BRAKES", "CHASSIS & MASS", "STEERING & LEAN", "SUSPENSION", "TYRES & GRIP", "AIR CONTROL", "RIDER" };

        static volatile PhysicsCatalog s_cat;
        public static string Error { get; private set; }
        static readonly object s_gate = new object();
        static System.Threading.Tasks.Task s_warm;

        /// The catalog of the connected install (read once; null with Error set when the core or the install cannot give it). Main thread.
        /// A read started by Warm is waited for (WaitedMs says how long).
        public static PhysicsCatalog Get()
        {
            if (s_cat != null) return s_cat;
            var warm = s_warm;
            if (warm != null && !warm.IsCompleted)
            {
                var w = System.Diagnostics.Stopwatch.StartNew();
                try { warm.Wait(); } catch (Exception) { }
                WaitedMs = w.Elapsed.TotalMilliseconds;
                if (s_cat != null) return s_cat;
            }
            try
            {
                if (string.IsNullOrEmpty(RxNativeLoader.LoadedPath))
                {
                    string path = RxNativeLoader.ResolvePath(RxNativeLoader.PluginDirectory(Application.dataPath, Application.isEditor));
                    string project = Path.GetDirectoryName(Application.dataPath);
                    RxNativeLoader.Load(path, Application.isEditor ? Path.Combine(string.IsNullOrEmpty(project) ? Application.dataPath : project, "Temp", "OpenReflex") : null);
                }
                string dir = Reflex.Port.Content.ReflexInstall.Dir;
                if (string.IsNullOrEmpty(dir)) { Error = "Connect your Reflex game folder first (GAME LIBRARY)."; return null; }
                Read(dir);
            }
            catch (Exception e) { Error = "The physics catalog could not be read: " + e.Message; }
            return s_cat;
        }

        /// The catalog read from the install at dir and parsed (any thread; one read at a time; the core's catalog is thread-safe).
        static void Read(string dir)
        {
            lock (s_gate)
            {
                if (s_cat != null) return;
                try
                {
                    string text = RxPhysicsNative.Catalog(dir);
                    if (string.IsNullOrEmpty(text)) { Error = "The vehicle parameter files could not be read from the install (EngineDependencies)."; return; }
                    s_cat = Parse(text);
                    Error = null;
                }
                catch (EntryPointNotFoundException) { Error = "This core has no PHYSICS STUDIO: rebuild librxcore."; }
                catch (Exception e) { Error = "The physics catalog could not be read: " + e.Message; }
            }
        }

        /// (w-joincrash) Starts reading the catalog on a worker, ahead of its first use: a first read opens the install and reads every vehicle
        /// parameter file (0.7 s warm, 3 s from a cold disk on the developer's Mac), and a first read on the main thread held its frame
        /// that long. Main thread; nothing when read, reading, before the core
        /// is loaded (Get loads it) or without an install.
        public static void Warm()
        {
            if (s_cat != null || s_warm != null || string.IsNullOrEmpty(RxNativeLoader.LoadedPath)) return;
            string dir; try { dir = Reflex.Port.Content.ReflexInstall.Dir; } catch (Exception) { return; }
            if (string.IsNullOrEmpty(dir)) return;
            s_warm = System.Threading.Tasks.Task.Run(() => Read(dir));
        }
        /// The catalog is read (Get returns at once).
        public static bool Ready => s_cat != null;
        /// How long the last Get waited for a read Warm had started (ms; 0: none).
        public static double WaitedMs { get; private set; }

        static float F(string s) => float.Parse(s, NumberStyles.Float, CultureInfo.InvariantCulture);
        public static PhysicsCatalog Parse(string text)
        {
            var c = new PhysicsCatalog();
            foreach (var line in text.Split('\n'))
            {
                if (line.Length < 2) continue;
                var f = line.Split('\t');
                switch (f[0])
                {
                    case "H": if (f.Length > 1) c.Hash = f[1]; break;
                    case "N":
                        if (f.Length >= 7) c.Infos[f[1]] = new Info { name = f[1], label = f[2], toggle = f[3] == "1", unit = f[4], unitX = f[5], desc = f[6] };
                        break;
                    case "C": if (f.Length >= 7) c.Classes.Add(new VehicleClass { index = int.Parse(f[1]), id = f[2], file = f[3], label = f[4], live = f[6] == "1" }); break;
                    case "P":
                        if (f.Length >= 8 && int.TryParse(f[1], out int pc) && pc >= 0 && pc < c.Classes.Count)
                            c.Classes[pc].parms.Add(new Param { cls = pc, index = int.Parse(f[2]), name = f[3], group = f[4], kind = int.Parse(f[5]), inFile = f[7] == "1",
                                                                 info = c.Infos.TryGetValue(f[3], out Info inf) ? inf : null });
                        break;
                    case "V":
                        if (f.Length >= 9 && int.TryParse(f[1], out int vc) && vc >= 0 && vc < c.Classes.Count && int.TryParse(f[2], out int vp) && vp >= 0 && vp < c.Classes[vc].parms.Count)
                        { c.Classes[vc].parms[vp].comps.Add(new Comp { index = int.Parse(f[3]), label = f[4], def = F(f[5]), min = F(f[6]), max = F(f[7]), unit = f[8] }); c.Values++; }
                        break;
                    case "G": case "D":
                        if (f.Length >= 6) (f[0] == "G" ? c.Grip : c.Drag).Add(new Material { index = int.Parse(f[1]), label = f[2], def = F(f[3]), min = F(f[4]), max = F(f[5]) });
                        break;
                }
            }
            return c;
        }

        static string Num(float v) => Mathf.Abs(v) >= 1000f || v == Mathf.Round(v) ? v.ToString("0.###", CultureInfo.InvariantCulture) : v.ToString("0.#####", CultureInfo.InvariantCulture);

        public VehicleClass Class(string id) => Classes.FirstOrDefault(k => string.Equals(k.id, id, StringComparison.OrdinalIgnoreCase));
        public Param Find(string cls, string name) => Class(cls)?.parms.FirstOrDefault(p => string.Equals(p.name, name, StringComparison.OrdinalIgnoreCase));

        /// "RearTire_StaticMuSideSlip" -> "REAR TIRE · STATIC MU SIDE SLIP" (the file's names spaced for reading).
        public static string Pretty(string name)
        {
            int u = name.IndexOf('_');
            string head = u > 0 ? name.Substring(0, u) : "", tail = u > 0 ? name.Substring(u + 1) : name;
            return (head.Length > 0 ? Spaced(head) + " · " : "") + Spaced(tail);
        }
        static string Spaced(string s)
        {
            var sb = new StringBuilder();
            for (int i = 0; i < s.Length; i++)
            {
                char ch = s[i];
                if (i > 0 && char.IsUpper(ch) && (char.IsLower(s[i - 1]) || i + 1 < s.Length && char.IsLower(s[i + 1]) && char.IsUpper(s[i - 1]))) sb.Append(' ');
                else if (i > 0 && char.IsDigit(ch) && !char.IsDigit(s[i - 1])) sb.Append(' ');
                sb.Append(char.ToUpperInvariant(ch));
            }
            return sb.ToString();
        }
    }

    public sealed class PhysicsConfig
    {
        public const string Format = "open-reflex-physics";
        public const int Version = 1;
        public const string SharePrefix = "ORPHYS1:";
        public const int MaxValues = 20000;

        public string id = "";            // the file name without .json in the library; "" not saved; "preset-..." a built-in preset
        public string name = "New config", author = "", description = "";
        public string gameVersion = "", paramSet = "";
        public bool gripGlobal, allVehicles;
        /// cls id | parameter name | component -> the value (absolute; the original's when absent)
        public readonly Dictionary<string, float> values = new Dictionary<string, float>(StringComparer.OrdinalIgnoreCase);
        public readonly Dictionary<int, float> grip = new Dictionary<int, float>(), drag = new Dictionary<int, float>();
        /// Entries an import named that the catalog does not have (another game version); kept out of the core text.
        public int unknown;

        public bool IsPreset => id.StartsWith("preset-", StringComparison.Ordinal);
        public bool IsOriginal => values.Count == 0 && grip.Count == 0 && drag.Count == 0;
        public int Changes => values.Count + grip.Count + drag.Count;

        public static string Key(string cls, string param, int comp) => cls + "|" + param + "|" + comp.ToString(CultureInfo.InvariantCulture);

        public PhysicsConfig Clone()
        {
            var c = new PhysicsConfig { id = id, name = name, author = author, description = description, gameVersion = gameVersion, paramSet = paramSet, gripGlobal = gripGlobal, allVehicles = allVehicles, unknown = unknown };
            foreach (var kv in values) c.values[kv.Key] = kv.Value;
            foreach (var kv in grip) c.grip[kv.Key] = kv.Value;
            foreach (var kv in drag) c.drag[kv.Key] = kv.Value;
            return c;
        }

        /// The core text (rx_session_phys_set). "" for the original.
        public string NativeText()
        {
            var sb = new StringBuilder();
            foreach (var kv in values.OrderBy(k => k.Key, StringComparer.Ordinal))
            {
                var k = kv.Key.Split('|');
                if (k.Length != 3 || !IsFinite(kv.Value)) continue;
                sb.Append("v\t").Append(k[0]).Append('\t').Append(k[1]).Append('\t').Append(k[2]).Append('\t').Append(kv.Value.ToString("R", CultureInfo.InvariantCulture)).Append('\n');
            }
            foreach (var kv in grip.OrderBy(k => k.Key)) sb.Append("g\t").Append(kv.Key).Append('\t').Append(kv.Value.ToString("R", CultureInfo.InvariantCulture)).Append('\n');
            foreach (var kv in drag.OrderBy(k => k.Key)) sb.Append("d\t").Append(kv.Key).Append('\t').Append(kv.Value.ToString("R", CultureInfo.InvariantCulture)).Append('\n');
            if (sb.Length == 0) return "";
            if (gripGlobal) sb.Append("o\tgrip_global\t1\n");
            if (allVehicles) sb.Append("o\tall_vehicles\t1\n");
            return sb.ToString();
        }

        /// A short fingerprint of what the config does (not its name): equal physics, equal hash.
        public string Fingerprint()
        {
            using var sha = System.Security.Cryptography.SHA1.Create();
            var h = sha.ComputeHash(Encoding.UTF8.GetBytes(NativeText()));
            return string.Concat(h.Take(4).Select(b => b.ToString("x2")));
        }

        static bool IsFinite(float v) => !float.IsNaN(v) && !float.IsInfinity(v);

        // ------------------------------------------------------------------------------------------- JSON

        public string ToJson(bool indent = true)
        {
            var o = new Dictionary<string, object>
            {
                ["format"] = Format, ["version"] = Version, ["name"] = name ?? "", ["author"] = author ?? "", ["description"] = description ?? "",
                ["game"] = string.IsNullOrEmpty(gameVersion) ? Application.version : gameVersion,
                ["paramSet"] = string.IsNullOrEmpty(paramSet) ? PhysicsCatalog.Get()?.Hash ?? "" : paramSet,
                ["gripGlobal"] = gripGlobal, ["allVehicles"] = allVehicles,
                ["values"] = values.OrderBy(k => k.Key, StringComparer.Ordinal).Select(kv =>
                {
                    var k = kv.Key.Split('|');
                    return (object)new Dictionary<string, object> { ["vehicle"] = k[0], ["param"] = k.Length > 1 ? k[1] : "", ["comp"] = k.Length > 2 ? int.Parse(k[2], CultureInfo.InvariantCulture) : 0, ["value"] = (double)kv.Value };
                }).ToList(),
                ["grip"] = grip.OrderBy(k => k.Key).Select(kv => (object)new Dictionary<string, object> { ["material"] = kv.Key, ["value"] = (double)kv.Value }).ToList(),
                ["drag"] = drag.OrderBy(k => k.Key).Select(kv => (object)new Dictionary<string, object> { ["material"] = kv.Key, ["value"] = (double)kv.Value }).ToList(),
            };
            string json = ReflexSave.Json.Write(o);
            return indent ? json : string.Join("", json.Split('\n').Select(l => l.Trim()));
        }

        /// Reads a config; every value checked against the catalog (unknown entries counted in `unknown`, values clamped to the bounds,
        /// NaN / infinite / non-numeric refused). Throws FormatException for what is not a config.
        public static PhysicsConfig FromJson(string json, PhysicsCatalog cat)
        {
            if (string.IsNullOrWhiteSpace(json)) throw new FormatException("empty");
            json = json.Trim().TrimStart('﻿');
            if (json.Length > 4 << 20) throw new FormatException("too large");
            if (Depth(json) > 16) throw new FormatException("nested too deeply");
            object parsed;
            try { parsed = ReflexSave.Json.Parse(json); } catch (Exception e) { throw new FormatException("not JSON (" + e.Message + ")"); }
            if (!(parsed is Dictionary<string, object> o)) throw new FormatException("not a physics config");
            if (!(o.TryGetValue("format", out var f) && f is string fs && fs == Format)) throw new FormatException("not an OPEN REFLEX physics config");
            if (o.TryGetValue("version", out var ver) && ver is double vd && vd > Version) throw new FormatException("made by a newer OPEN REFLEX (version " + vd + ")");
            var c = new PhysicsConfig
            {
                name = Clip(Str(o, "name", "Imported config"), 60), author = Clip(Str(o, "author", ""), 40), description = Clip(Str(o, "description", ""), 400),
                gameVersion = Clip(Str(o, "game", ""), 40), paramSet = Clip(Str(o, "paramSet", ""), 16),
                gripGlobal = o.TryGetValue("gripGlobal", out var gg) && gg is bool gb && gb, allVehicles = o.TryGetValue("allVehicles", out var av) && av is bool ab && ab,
            };
            if (string.IsNullOrWhiteSpace(c.name)) c.name = "Imported config";
            if (o.TryGetValue("values", out var vo) && vo is List<object> vl)
                foreach (var e in vl.OfType<Dictionary<string, object>>().Take(MaxValues))
                {
                    string cls = Str(e, "vehicle", ""), param = Str(e, "param", "");
                    if (!(e.TryGetValue("comp", out var co) && co is double cd) || !(e.TryGetValue("value", out var va) && va is double vv) || double.IsNaN(vv) || double.IsInfinity(vv)) { c.unknown++; continue; }
                    var p = cat?.Find(cls, param);
                    int comp = (int)cd;
                    if (cat != null && (p == null || comp < 0 || comp >= p.comps.Count || comp != cd)) { c.unknown++; continue; }
                    float v = (float)vv;
                    if (p != null) { var k = p.comps[comp]; v = Mathf.Clamp(v, k.min, k.max); cls = cat.Classes[p.cls].id; param = p.name; }
                    c.values[Key(cls, param, comp)] = v;
                }
            ReadMaterials(o, "grip", c.grip, cat?.Grip.Count ?? 6, ref c.unknown);
            ReadMaterials(o, "drag", c.drag, cat?.Drag.Count ?? 4, ref c.unknown);
            return c;
        }
        static void ReadMaterials(Dictionary<string, object> o, string key, Dictionary<int, float> into, int count, ref int unknown)
        {
            if (!(o.TryGetValue(key, out var go) && go is List<object> gl)) return;
            foreach (var e in gl.OfType<Dictionary<string, object>>().Take(64))
            {
                if (!(e.TryGetValue("material", out var m) && m is double md && md == Math.Floor(md) && md >= 0 && md < count) ||
                    !(e.TryGetValue("value", out var v) && v is double vd) || double.IsNaN(vd) || double.IsInfinity(vd)) { unknown++; continue; }
                into[(int)md] = Mathf.Clamp((float)vd, 0f, 2f);
            }
        }
        static int Depth(string s)
        {
            int d = 0, max = 0; bool str = false, esc = false;
            foreach (char ch in s)
            {
                if (str) { if (esc) esc = false; else if (ch == '\\') esc = true; else if (ch == '"') str = false; continue; }
                if (ch == '"') str = true;
                else if (ch == '{' || ch == '[') { if (++d > max) max = d; }
                else if (ch == '}' || ch == ']') d--;
            }
            return max;
        }
        static string Str(Dictionary<string, object> o, string k, string def) => o.TryGetValue(k, out var v) && v is string s ? s.Trim() : def;
        static string Clip(string s, int n) => s.Length > n ? s.Substring(0, n) : s;

        // ------------------------------------------------------------------------------------------- share code

        public string ShareCode()
        {
            byte[] raw = Encoding.UTF8.GetBytes(ToJson(false));
            using var ms = new MemoryStream();
            using (var z = new DeflateStream(ms, System.IO.Compression.CompressionLevel.Optimal, true)) z.Write(raw, 0, raw.Length);
            return SharePrefix + Convert.ToBase64String(ms.ToArray()).TrimEnd('=').Replace('+', '-').Replace('/', '_');
        }

        /// A share code (anywhere in the text: a chat message), or a config's JSON pasted as text. Null with error otherwise.
        public static PhysicsConfig FromShareText(string text, PhysicsCatalog cat, out string error)
        {
            error = null;
            text = (text ?? "").Trim();
            try
            {
                int at = text.IndexOf(SharePrefix, StringComparison.OrdinalIgnoreCase);
                if (at >= 0)
                {
                    string b = new string(text.Substring(at + SharePrefix.Length).TakeWhile(ch => char.IsLetterOrDigit(ch) || ch == '-' || ch == '_' || ch == '+' || ch == '/' || ch == '=').ToArray());
                    if (b.Length == 0 || b.Length > 4 << 20) throw new FormatException("the code is empty or too long");
                    b = b.TrimEnd('=').Replace('-', '+').Replace('_', '/');
                    b += new string('=', (4 - b.Length % 4) % 4);
                    byte[] data = Convert.FromBase64String(b);
                    using var z = new DeflateStream(new MemoryStream(data), CompressionMode.Decompress);
                    using var outp = new MemoryStream();
                    var buf = new byte[8192];
                    int n, total = 0;
                    while ((n = z.Read(buf, 0, buf.Length)) > 0) { total += n; if (total > 4 << 20) throw new FormatException("the code unpacks too large"); outp.Write(buf, 0, n); }
                    return FromJson(Encoding.UTF8.GetString(outp.ToArray()), cat);
                }
                if (text.StartsWith("{", StringComparison.Ordinal)) return FromJson(text, cat);
                error = "No physics share code found (it starts with " + SharePrefix + ")";
            }
            catch (Exception e) { error = "Not a valid physics config: " + e.Message; }
            return null;
        }

        /// Made for another parameter set (another install / game version): the names that match still load.
        public bool Incompatible(PhysicsCatalog cat) => cat != null && (unknown > 0 || !string.IsNullOrEmpty(paramSet) && !string.Equals(paramSet, cat.Hash, StringComparison.OrdinalIgnoreCase));
    }

    public static class PhysicsLibrary
    {
        public const string ActiveKey = "Physics.Active";
        public static string Folder => Path.Combine(ReflexSave.Folder, "Physics");
        public static string ExportFolder => Path.Combine(Folder, "Export");
        public static string ImportFolder => Path.Combine(Folder, "Import");

        // ------------------------------------------------------------------------------------------- presets (built from the catalog)

        public static List<PhysicsConfig> Presets(PhysicsCatalog cat)
        {
            var list = new List<PhysicsConfig> { new PhysicsConfig { id = "preset-original", name = "Original", author = "MX vs ATV Reflex", description = "The game's own physics: nothing is changed (the core is not touched at all)." } };
            if (cat == null) return list;
            list.Add(Scaled(cat, "preset-moregrip", "More Grip", "Every tyre holds 25 % more sideways (static and sliding) and 10 % more along the ground: the rear steps out later.",
                            p => p.group == "TYRES & GRIP" && (p.name.EndsWith("StaticMuSideSlip") || p.name.EndsWith("DynamicMuSideMultiplier")) ? 1.25f : p.group == "TYRES & GRIP" && p.name.EndsWith("StaticMuRoll") ? 1.1f : 1f));
            list.Add(Scaled(cat, "preset-lighterair", "Lighter Air", "Gravity at 80 %: longer, floatier jumps with more time for whips and tricks.",
                            p => p.name == "VehicleSim_Gravity" ? 0.8f : 1f));
            list.Add(Scaled(cat, "preset-snappy", "Snappy Power", "20 % more engine torque across the rev range: harder launches and quicker drives out of turns.",
                            p => p.name == "Engine_TorqueCurve" ? 1.2f : 1f, yOnly: true));
            return list;
        }
        static PhysicsConfig Scaled(PhysicsCatalog cat, string id, string name, string desc, Func<PhysicsCatalog.Param, float> factor, bool yOnly = false)
        {
            var c = new PhysicsConfig { id = id, name = name, author = "OPEN REFLEX", description = desc, paramSet = cat.Hash };
            foreach (var k in cat.Classes)
                foreach (var p in k.parms)
                {
                    float f = factor(p);
                    if (f == 1f) continue;
                    int knots = p.kind == 2 ? (p.comps.Count - 2) / 2 : 0;
                    foreach (var v in p.comps)
                    {
                        if (p.kind == 2 && (!yOnly || v.index >= knots)) continue;   // a curve: its y values only
                        if (v.def == 0f) continue;
                        c.values[PhysicsConfig.Key(k.id, p.name, v.index)] = Mathf.Clamp(v.def * f, v.min, v.max);
                    }
                }
            return c;
        }

        // ------------------------------------------------------------------------------------------- the library

        public static List<PhysicsConfig> Saved(PhysicsCatalog cat)
        {
            var list = new List<PhysicsConfig>();
            try
            {
                if (!Directory.Exists(Folder)) return list;
                foreach (var path in Directory.GetFiles(Folder, "*.json").OrderBy(p => p, StringComparer.OrdinalIgnoreCase).Take(500))
                {
                    try
                    {
                        if (new FileInfo(path).Length > 4 << 20) continue;
                        var c = PhysicsConfig.FromJson(File.ReadAllText(path), cat);
                        c.id = Path.GetFileNameWithoutExtension(path);
                        list.Add(c);
                    }
                    catch (Exception e) { Debug.LogWarning("[PhysicsStudio] " + Path.GetFileName(path) + ": " + e.Message); }
                }
            }
            catch (Exception e) { Debug.LogWarning("[PhysicsStudio] library: " + e.Message); }
            return list;
        }
        public static List<PhysicsConfig> All(PhysicsCatalog cat) => Presets(cat).Concat(Saved(cat)).ToList();

        static string Slug(string name)
        {
            var sb = new StringBuilder();
            foreach (char ch in (name ?? "").Trim().ToLowerInvariant()) sb.Append(char.IsLetterOrDigit(ch) ? ch : '-');
            string s = sb.ToString().Trim('-');
            while (s.Contains("--")) s = s.Replace("--", "-");
            return s.Length == 0 ? "config" : s.Length > 40 ? s.Substring(0, 40) : s;
        }
        static void WriteAtomic(string path, string text)
        {
            Directory.CreateDirectory(Path.GetDirectoryName(path));
            string tmp = path + ".tmp";
            File.WriteAllText(tmp, text, new UTF8Encoding(false));
            if (File.Exists(path)) File.Delete(path);
            File.Move(tmp, path);
        }

        /// Saves c in the library (a preset or an unsaved config gets a new file); returns the path.
        public static string Save(PhysicsConfig c)
        {
            if (ReflexSave.Sandboxed) throw new InvalidOperationException("the settings folder is not writable in this run");
            if (string.IsNullOrEmpty(c.id) || c.IsPreset)
            {
                string stem = Slug(c.name), id = stem; int n = 2;
                while (File.Exists(Path.Combine(Folder, id + ".json"))) id = stem + "-" + n++;
                c.id = id;
            }
            c.paramSet = PhysicsCatalog.Get()?.Hash ?? c.paramSet;
            c.gameVersion = Application.version;
            string path = Path.Combine(Folder, c.id + ".json");
            WriteAtomic(path, c.ToJson());
            return path;
        }
        public static void Delete(PhysicsConfig c)
        {
            if (c == null || c.IsPreset || string.IsNullOrEmpty(c.id)) return;
            string path = Path.Combine(Folder, c.id + ".json");
            if (File.Exists(path)) File.Delete(path);
            if (ActiveId == c.id) SetActive(null);
        }
        /// A copy of c as a .json file in Physics/Export (for sharing as a file); returns the path.
        public static string Export(PhysicsConfig c)
        {
            string path = Path.Combine(ExportFolder, Slug(c.name) + ".json");
            WriteAtomic(path, c.ToJson());
            return path;
        }
        /// Every .json in Physics/Import into the library (each saved as a new config; the file is left where it is). Returns the
        /// configs imported and what was refused.
        public static List<PhysicsConfig> ImportFiles(PhysicsCatalog cat, List<string> refused)
        {
            var got = new List<PhysicsConfig>();
            Directory.CreateDirectory(ImportFolder);
            foreach (var path in Directory.GetFiles(ImportFolder, "*.json").Take(200))
            {
                try
                {
                    if (new FileInfo(path).Length > 4 << 20) throw new FormatException("too large");
                    var c = PhysicsConfig.FromJson(File.ReadAllText(path), cat);
                    c.id = "";
                    Save(c);
                    got.Add(c);
                }
                catch (Exception e) { refused?.Add(Path.GetFileName(path) + ": " + e.Message); }
            }
            return got;
        }

        // ------------------------------------------------------------------------------------------- shared by the hub's CONFIGS page and the in-ride panel

        /// Whether cfg is what the player's rides use (active null: the original physics).
        public static bool InUse(PhysicsConfig cfg, PhysicsConfig active) => active != null ? active.id == cfg.id : cfg.id == "preset-original";

        /// "IN USE" / "PRESET" / "OTHER VERSION" (joined with " · "), else the change count.
        public static string StateLabel(PhysicsConfig cfg, PhysicsConfig active, PhysicsCatalog cat)
        {
            bool inUse = InUse(cfg, active);
            string state = (inUse ? "IN USE" : cfg.IsPreset ? "PRESET" : "") + (cfg.Incompatible(cat) ? (inUse || cfg.IsPreset ? " · " : "") + "OTHER VERSION" : "");
            return state.Length == 0 ? (cfg.IsOriginal ? "NO CHANGES" : cfg.Changes + " CHANGES") : state;
        }

        /// One LIBRARY row (a value row without arrows; submit loads it); the caller names it and adds its focus help.
        public static UnityEngine.UIElements.VisualElement ConfigRow(UnityEngine.UIElements.VisualElement parent, PhysicsConfig cfg, PhysicsConfig active, PhysicsCatalog cat, Color accent, Action submit)
        {
            string state = StateLabel(cfg, active, cat);
            var row = OuUi.Spinner(cfg.name.ToUpperInvariant(), () => state, _ => { }, parent, submit);
            row.name = "ps-config-" + cfg.id;
            ((OuNavItem)row.userData).adjust = null;
            if (InUse(cfg, active))
            {   // the accent, except while focused (the focused row's fill is the accent: its own text then)
                var v = UnityEngine.UIElements.UQueryExtensions.Q<UnityEngine.UIElements.Label>(row, className: "mxa-value"); v.style.color = accent;
                row.RegisterCallback<UnityEngine.UIElements.FocusInEvent>(_ => v.style.color = new UnityEngine.UIElements.StyleColor(UnityEngine.UIElements.StyleKeyword.Null));
                row.RegisterCallback<UnityEngine.UIElements.FocusOutEvent>(_ => v.style.color = accent);
            }
            return row;
        }

        /// The help text of a library entry (author, file, fingerprint, description, what it changes).
        public static string ConfigHelp(PhysicsConfig cfg, PhysicsCatalog cat) =>
            (string.IsNullOrEmpty(cfg.author) ? "" : "BY " + cfg.author.ToUpperInvariant() + "  ·  ") + (cfg.IsPreset ? "BUILT-IN PRESET" : cfg.id + ".json") + "  ·  " + cfg.Fingerprint() +
            "\n" + (cfg.description ?? "") + (cfg.Incompatible(cat) ? "\nMade for another game version or parameter set" + (cfg.unknown > 0 ? ": " + cfg.unknown + " value(s) do not exist here and are left out." : ": the values that exist here load.") : "") +
            "\n" + Summary(cfg, cat);

        public static string Summary(PhysicsConfig c, PhysicsCatalog cat)
        {
            if (c.IsOriginal) return "No changes.";
            var parts = c.values.Keys.Select(k => k.Split('|')).Where(k => k.Length == 3).GroupBy(k => k[0]).Select(g => (cat?.Class(g.Key)?.label ?? g.Key) + " " + g.Count()).ToList();
            if (c.grip.Count + c.drag.Count > 0) parts.Add("TERRAIN " + (c.grip.Count + c.drag.Count));
            return string.Join("  ·  ", parts) + (c.allVehicles ? "  ·  EVERY RIDER" : "") + (c.gripGlobal ? "  ·  GRIP FOR EVERY RIDER" : "");
        }

        /// A config from the clipboard's share code (or JSON): not in the library yet (id empty); null + a message when there is none.
        public static PhysicsConfig FromClipboard(PhysicsCatalog cat, out string error)
        {
            var got = PhysicsConfig.FromShareText(GUIUtility.systemCopyBuffer, cat, out error);
            if (got == null) { error = (error ?? "NOTHING TO IMPORT").ToUpperInvariant(); return null; }
            got.id = "";
            return got;
        }

        /// The share code of c onto the clipboard.
        public static void CopyShareCode(PhysicsConfig c) => GUIUtility.systemCopyBuffer = c.ShareCode();

        /// The status line after ImportFiles (empty when nothing was found in the folder).
        public static string ImportReport(int got, List<string> refused) =>
            got == 0 && refused.Count == 0 ? "PUT .JSON FILES IN PHYSICS/IMPORT FIRST" : "IMPORTED " + got + (refused.Count > 0 ? ", REFUSED " + refused.Count + ": " + string.Join("; ", refused.Take(2)).ToUpperInvariant() : "");

        // ------------------------------------------------------------------------------------------- the active config

        /// The config the player's rides use ("" or "preset-original": the original physics).
        public static string ActiveId => ReflexSave.GetString(ActiveKey, "");
        public static PhysicsConfig Active(PhysicsCatalog cat)
        {
            string id = ActiveId;
            if (string.IsNullOrEmpty(id) || id == "preset-original") return null;
            return All(cat).FirstOrDefault(c => c.id == id);
        }
        public static void SetActive(PhysicsConfig c)
        {
            if (c == null || c.IsOriginal) ReflexSave.Delete(ActiveKey); else ReflexSave.SetString(ActiveKey, c.id);
            ReflexSave.Save();
            SyncPending();
        }
        /// The next event's config from the active one (the shell calls it when it starts and when the choice changes).
        public static void SyncPending()
        {
            try
            {
                if (string.IsNullOrEmpty(ActiveId)) { RxPhysicsStudio.SetPending("", ""); return; }
                var cat = PhysicsCatalog.Get();
                var c = Active(cat);
                RxPhysicsStudio.SetPending(c?.name ?? "", c?.NativeText() ?? "");
            }
            catch (Exception e) { Debug.LogWarning("[PhysicsStudio] " + e.Message); RxPhysicsStudio.SetPending("", ""); }
        }
    }
}

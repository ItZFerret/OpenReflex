// MX vs ATV Untamed converter, stage S7 "entities": the port of the Python reference converter's ux_entities.py. Untamed race entities become
// Reflex RSData entities: every emitted entity is a deep copy of the stock prototype of its RSData type (UxStock.LibPrototype) with values
// filled in; no property is invented. UxEntities.Convert reads level.json (level_fr.json for kind fr) and the manifests of the meshes /
// materials / collision / terrain stages from the work store (each optional: a stub stands in, as in Python), the Untamed collision, the
// terrain collision and (for --bale-reseat) the race line from the disc, and writes what Python writes below work/<Level>/:
//   entities.json (entities_fr.json)        the specs, counts, checks, problems, warnings (ux_common.write_json: sorted keys, indent 1)
//   entities/<kind>_Base.chunks, <kind>_FreeRide.chunks   back-to-back zlib BXML SceneChunk documents of at most 30 entities
// With options.write = false nothing is written and the result holds the document, the Elems and the chunk bytes (the builder's call).
//
// Also here, because the stage needs them: the --bale-reseat part of ux_startpack.py (footprints, chute_models, classify, reseat) with the
// tdf walk of tdf07_to_reflex.py (split / structure), numpy's default_rng (SeedSequence + PCG64, Generator.random / choice with p) for the
// ridable-collision samples, an .npy reader, and the Python text rules the document's strings depend on ('%g', '%.2e', repr, round).
// Python's numbers are kept: float32 matrices, the fused dot products of the reference's BLAS (UxNp.MatMul / VecMat / Norm / Fma), numpy's
// pairwise sums, round-half-even, np.round against Python's round, np.hypot as sqrt(x*x + y*y).
//
// Not ported: the command line (main, --selftest, report, --jobs: run levels in parallel with Parallel.For, the stage is thread safe per
// level), the Builder's injected level_json / cworld / tquery objects (hf: only its path, options.hf_path), and of ux_startpack.py
// edge_clear / min_clearance / class_report (the race stage's --bale-edge-clear).
// Environment data Python writes and this port cannot: source.tool / source.tool_md5 (the Python file and its md5: here TOOL and null),
// and the absolute paths in source.level_json, source.manifests.*.path, source.hf, freeride.hf_source and options.manifest_paths (written
// relative to the work store). Everything else is byte for byte Python's on the 12 levels (tests/untamed_convert/entities).
using System;
using System.Collections.Generic;
using System.Globalization;
using System.Numerics;
using System.Text;
using System.Threading;
using Reflex.Port.Content.AliveImport;

namespace Reflex.Port.Content.UntamedImport
{
    /// The options of ux_entities.build_entities (the Builder's keyword arguments; the defaults are Python's).
    public sealed class UxEntitiesOptions
    {
        /// 'sx' (level.json -> entities.json) or 'fr' (level_fr.json -> entities_fr.json).
        public string kind = "sx";
        public bool static_bales;
        public string static_pop = "all";          // all | initial | drop
        public bool crowd_off;
        public string static_male = "map";         // map | editor_default
        /// {type: {old: new}} applied last to entity asset references (--renames), or null.
        public JObj renames;
        public bool drop_gate_top;
        public string crowd_tier = "stock";        // stock | untamed
        public string group_number = "zero";       // zero | copy
        public string hb_freeride = "drop";        // drop | freeride | base
        public string buried = "drop";             // drop | keep
        public string fr_clearance = "strict";     // strict | legacy
        /// (dx, dz) ft: --bale-trigger-move (sx), or null.
        public double[] bale_trigger_move;
        /// --bale-reseat FT (sx); 0 = off.
        public double bale_reseat;
        /// Manifest overrides {meshes | collision | terrain | materials | hf: path relative to the work store} (the builder passes
        /// collision = 'collision-t9/collision.json' for --type9 accept, materials = 'materials/materials.json'), or null.
        public Dictionary<string, string> manifest_paths;
        /// The terrain stage's height field, relative to the work store (Python: Builder(hf=HeightField(path=...)); the 'hf' entry of
        /// manifest_paths only names the file in the document's manifest table). A missing file = the stub (the trncol surface + lift).
        public string hf_path = "terrain/hf.npy";
        /// false = build_entities(write=False): nothing is written, the result holds the document and the chunk bytes.
        public bool write = true;

        /// ux_entities.parse_move: 'DX,DZ' -> (dx, dz), null for an empty text.
        public static double[] ParseMove(string text)
        {
            if (string.IsNullOrEmpty(text)) return null;
            string[] a = text.Split(',');
            if (a.Length != 2) throw new UxException("--bale-trigger-move needs DX,DZ, got '" + text + "'");
            return new[] { UxJson.ParseDouble(a[0].Trim()), UxJson.ParseDouble(a[1].Trim()) };
        }
    }

    /// What build_entities returns: (sub-scene, Entity Elem) pairs and the entities.json document, plus the encoded chunks.
    public sealed class UxEntitiesResult
    {
        /// (sub-scene 'Base' | 'FreeRide', Entity Elem), in entities.json order.
        public List<KeyValuePair<string, UxElem>> items;
        /// The entities.json dict.
        public JObj doc;
        /// 'Base' / 'FreeRide' -> the bytes of entities/<kind>_<Sub>.chunks.
        public Dictionary<string, byte[]> chunks;
        /// The work-store paths written ('' when write was off).
        public string jsonPath, chunkDir;
        public List<string> problems, failedChecks;
        /// all checks ok and no problems (ux_entities._run's 'ok').
        public bool Ok => problems.Count == 0 && failedChecks.Count == 0;
        public double seconds;
        /// The builder's gate: 'ux_entities: problems ..., failed checks ...'.
        public void Require()
        {
            if (Ok) return;
            throw new UxException("ux_entities: problems [" + string.Join("; ", problems.GetRange(0, Math.Min(5, problems.Count))) + "], failed checks [" +
                string.Join("; ", failedChecks) + "]");
        }
    }

    /// ux_entities.py.
    public static class UxEntities
    {
        public const string SCHEMA = "ux_entities/1";
        public const string TOOL = "UxEntities.cs";
        public const long ID_OFFSET = 20000000;            // primary entity id = Untamed entityId + ID_OFFSET
        public const long SYNTH_ID_BASE = 40000000;        // extra part models and terrain parts: SYNTH_ID_BASE + n
        public const long FR_ID_BASE = 19000000;           // free-ride starts: FR_ID_BASE + Index
        public const long TEMPLATE_ID_LO = 9000000, TEMPLATE_ID_HI = 9000999;
        public const int CHUNK_MAX = UxCommon.Limits.MaxEntitiesPerChunk;
        public const string PFX = UxCommon.NS_PREFIX;
        public const double BURIED_MIN_FT = 0.05;
        public const double FR_POINT_CLEAR_FT = 3.0;

        static readonly string[] PHYSICS_FAMILY = { "PhysicsRSData", "PopUpBaleRSData", "PopDownBaleRSData" };
        static readonly string[] COLLISION_TYPES = { "PhysicsRSData", "PopUpBaleRSData", "PopDownBaleRSData", "CollisionRSData" };
        static readonly string[] RENDER_TYPES = { "ModelRSData", "BillboardCrowdRSData" };
        static readonly string[] COPY_PROPS = { "EnableTick", "ExportName", "AlwaysVisible", "CastsShadow", "GroupNumber", "AIAvoid", "LinkedTriggerID",
            "Tier Number", "Is Non Animated", "TriggerType", "TriggerEntityType", "TriggerID", "InverseTickFreq", "LevelConfig" };
        static bool In(string[] set, string v) { if (v == null) return false; foreach (string s in set) if (s == v) return true; return false; }

        // ---- stock Reflex enum tables (name -> value)
        static Dictionary<string, long> Table(params object[] kv)
        {
            var d = new Dictionary<string, long>(StringComparer.Ordinal);
            for (int i = 0; i < kv.Length; i += 2) d[(string)kv[i]] = System.Convert.ToInt64(kv[i + 1], CultureInfo.InvariantCulture);
            return d;
        }
        static readonly Dictionary<string, Dictionary<string, long>> STOCK_ENUMS = MakeEnums();
        static Dictionary<string, Dictionary<string, long>> MakeEnums()
        {
            var e = new Dictionary<string, Dictionary<string, long>>(StringComparer.Ordinal);
            e["CollisionAudioMaterial"] = Table("Undefined", 0, "AirBag", 1, "ChainLink", 4, "ConcreteWall", 5, "ConcreteWallVinyl", 6, "MetalContainer", 12,
                "MetalFrame", 13, "MetalFrameTuffBlock", 14, "MetalFrameVinyl", 15, "MetalPole", 16, "MetalPoleVinyl", 17, "MetalSolid", 18, "MetalTube", 19,
                "MetalTubeVinyl", 20, "PylonCone", 23, "PylonSpring", 24, "RockSmall", 26, "TuffBlock", 33, "VehicleTruck", 34, "Vinyl", 36, "WoodBox", 39,
                "WoodFence", 40, "WoodFenceVinyl", 41, "WoodFrame", 42, "WoodSolid", 44, "VehicleUTVOrBuggy", 48, "AmbientVehicleCart", 51, "Jumbotron", 52,
                "WoodBoxTuffBlock", 53, "RubberFence", 54, "TireAmbient", 55, "MetalPoleSmall", 56, "Haybale", 57, "CorrugatedMetal", 58,
                "MetalContainerSmall", 59, "Logs", 60, "ConcreteWood", 61, "ConcreteChainLink", 63, "MechanicRow", 64, "MetalRubber", 65, "WindowPane", 66);
            e["PhysicsType"] = Table("MXBike", 0, "TagBall", 4, "Default", 5, "LightSolid", 7, "HeavySolid", 8, "HaySolid", 9, "LightHollow", 10);
            var dm = new Dictionary<string, long>(StringComparer.Ordinal);
            for (int i = 0; i < 8; i++) dm["ID " + i.ToString(CultureInfo.InvariantCulture)] = i;
            e["DriveOnMaterial"] = dm;
            e["TriggerType"] = Table("Box", 0, "Sphere", 1);
            e["TriggerEntityType"] = Table("Vehicle", 0);
            e["LevelConfig"] = Table("", 0, "ChallengeHillClimbA", 15, "ChallengeTargetPractice", 27, "ChallengeWheelie", 28, "ChallengeTrailBlazerA", 39);
            return e;
        }
        /// AUDIO_TABLE: Untamed AudioCollisionMaterial (+ AudioCollisionSubclass; "\n" + null = any subclass) -> Reflex CollisionAudioMaterial.
        static readonly Dictionary<string, string> AUDIO_TABLE = new Dictionary<string, string>(StringComparer.Ordinal)
        {
            { "Metal\nMedium", "MetalFrame" }, { "Metal\nSmall", "MetalPoleSmall" }, { "Metal\nSolid", "MetalSolid" }, { "Metal\nUser1", "VehicleTruck" },
            { "Metal\n\0", "MetalFrame" }, { "Wood\n\0", "WoodSolid" }, { "Eco\n\0", "Undefined" }, { "Plastic/Rubber\nLarge", "TuffBlock" },
            { "Plastic/Rubber\nMedium", "TireAmbient" }, { "Plastic/Rubber\nHollow", "TuffBlock" }, { "Plastic/Rubber\n\0", "TuffBlock" },
            { "Pole\n\0", "MetalPole" }, { "Concrete\n\0", "ConcreteWall" }, { "Undefined\n\0", "Undefined" },
        };
        const string AUDIO_DEFAULT = "Undefined";
        static string TriggerIdMap(string id) => id == "HoleShotTrigger" || id == "HoleShotLight_Trigger" ? "Holeshot" : null;

        /// GATE_TOP_RX.match: '(StartingGate|Split_Gate)_\d+_Top$' from the start of the name.
        static bool GateTop(string s)
        {
            if (s == null) return false;
            int p;
            if (s.StartsWith("StartingGate_", StringComparison.Ordinal)) p = 13; else if (s.StartsWith("Split_Gate_", StringComparison.Ordinal)) p = 11; else return false;
            int q = p; while (q < s.Length && char.IsDigit(s[q])) q++;
            if (q == p) return false;
            string rest = s.Substring(q);
            return rest == "_Top" || rest == "_Top\n";
        }
        static bool FreerideOnly(string s) => s != null && s.IndexOf("freeride_only", StringComparison.OrdinalIgnoreCase) >= 0;

        // ================================================================== Python values
        /// The few Python rules the stage's texts and comparisons depend on: truth, str / repr, ==, '%.Nf', '%.Ne', '%g'.
        public static class Py
        {
            public static bool Truthy(object v)
            {
                switch (v)
                {
                    case null: return false;
                    case bool b: return b;
                    case long l: return l != 0;
                    case int i: return i != 0;
                    case double d: return d != 0;
                    case string s: return s.Length > 0;
                    case JObj o: return o.items.Count > 0;
                    case System.Collections.ICollection c: return c.Count > 0;
                }
                return true;
            }
            public static bool IsNum(object v) => v is long || v is int || v is double || v is bool;
            public static double Num(object v) => v is double d ? d : v is long l ? l : v is int i ? i : v is bool b ? (b ? 1 : 0) : throw new UxException("a number was expected, not " + Repr(v));
            /// int(v).
            public static long Int(object v)
            {
                switch (v)
                {
                    case long l: return l;
                    case int i: return i;
                    case bool b: return b ? 1 : 0;
                    case double d:
                        if (double.IsNaN(d) || double.IsInfinity(d)) throw new UxException("cannot convert float " + Str(d) + " to integer");
                        return (long)Math.Truncate(d);
                    case string s:
                        if (long.TryParse(s.Trim(), NumberStyles.AllowLeadingSign, CultureInfo.InvariantCulture, out long r)) return r;
                        break;
                }
                throw new UxException("int() of " + Repr(v));
            }
            /// a == b for JSON-style values (numbers by value across int / float / bool, dicts ignoring order).
            public static bool Eq(object a, object b)
            {
                if (a == null || b == null) return a == null && b == null;
                if (IsNum(a) && IsNum(b))
                {
                    if (a is double || b is double) return Num(a) == Num(b);
                    return Int(a) == Int(b);
                }
                if (a is string sa) return b is string sb && string.Equals(sa, sb, StringComparison.Ordinal);
                if (a is JObj ja)
                {
                    if (!(b is JObj jb) || ja.items.Count != jb.items.Count) return false;
                    foreach (var kv in ja.items) { if (!jb.Has(kv.Key) || !Eq(kv.Value, jb.Get(kv.Key))) return false; }
                    return true;
                }
                if (a is System.Collections.IList la)
                {
                    if (!(b is System.Collections.IList lb) || b is string || la.Count != lb.Count) return false;
                    for (int i = 0; i < la.Count; i++) if (!Eq(la[i], lb[i])) return false;
                    return true;
                }
                return a.Equals(b);
            }
            static string FloatRepr(double d)
            {
                if (double.IsNaN(d)) return "nan";
                if (double.IsInfinity(d)) return d > 0 ? "inf" : "-inf";
                return PyFormat.Repr(d);
            }
            /// str(v). A List is a Python list, an object[] a tuple.
            public static string Str(object v)
            {
                switch (v)
                {
                    case null: return "None";
                    case string s: return s;
                    case bool b: return b ? "True" : "False";
                    case double d: return FloatRepr(d);
                    case float f: return FloatRepr(f);
                    case JObj o:
                    {
                        var sb = new StringBuilder("{");
                        for (int i = 0; i < o.items.Count; i++) { if (i > 0) sb.Append(", "); sb.Append(Repr(o.items[i].Key)).Append(": ").Append(Repr(o.items[i].Value)); }
                        return sb.Append('}').ToString();
                    }
                    case object[] t:
                    {
                        var sb = new StringBuilder("(");
                        for (int i = 0; i < t.Length; i++) { if (i > 0) sb.Append(", "); sb.Append(Repr(t[i])); }
                        if (t.Length == 1) sb.Append(',');
                        return sb.Append(')').ToString();
                    }
                    case System.Collections.IEnumerable e:
                    {
                        var sb = new StringBuilder("["); bool first = true;
                        foreach (object x in e) { if (!first) sb.Append(", "); first = false; sb.Append(Repr(x)); }
                        return sb.Append(']').ToString();
                    }
                }
                return System.Convert.ToString(v, CultureInfo.InvariantCulture);
            }
            /// repr(v).
            public static string Repr(object v)
            {
                if (!(v is string s)) return Str(v);
                char q = s.IndexOf('\'') >= 0 && s.IndexOf('"') < 0 ? '"' : '\'';
                var sb = new StringBuilder(); sb.Append(q);
                foreach (char c in s)
                {
                    if (c == q || c == '\\') sb.Append('\\').Append(c);
                    else if (c == '\n') sb.Append("\\n");
                    else if (c == '\r') sb.Append("\\r");
                    else if (c == '\t') sb.Append("\\t");
                    else if (c < 0x20 || c == 0x7F) sb.Append("\\x").Append(((int)c).ToString("x2", CultureInfo.InvariantCulture));
                    else sb.Append(c);
                }
                return sb.Append(q).ToString();
            }

            /// round_half_even(a * 10^scale) for a >= 0, exactly (the decimal digits C's printf writes).
            static BigInteger ScaledRound(double a, int scale)
            {
                if (a == 0) return BigInteger.Zero;
                long bits = BitConverter.DoubleToInt64Bits(a); int be = (int)((bits >> 52) & 0x7FF); long frac = bits & 0xFFFFFFFFFFFFFL;
                long m = be == 0 ? frac : frac | (1L << 52); int e = be == 0 ? -1074 : be - 1075;
                BigInteger n = m, d = BigInteger.One;
                if (e >= 0) n <<= e; else d <<= -e;
                if (scale >= 0) n *= BigInteger.Pow(10, scale); else d *= BigInteger.Pow(10, -scale);
                var q = BigInteger.DivRem(n, d, out var r);
                int c = (2 * r).CompareTo(d);
                if (c > 0 || c == 0 && !q.IsEven) q += 1;
                return q;
            }
            static bool Neg(double v) => v < 0 || v == 0 && 1 / v < 0;
            static string Special(double v) => double.IsNaN(v) ? "nan" : v > 0 ? "inf" : "-inf";
            /// '%.<prec>f' % v.
            public static string F(double v, int prec)
            {
                if (double.IsNaN(v) || double.IsInfinity(v)) return Special(v);
                string s = ScaledRound(Math.Abs(v), prec).ToString(CultureInfo.InvariantCulture);
                if (prec > 0)
                {
                    if (s.Length <= prec) s = new string('0', prec + 1 - s.Length) + s;
                    s = s.Substring(0, s.Length - prec) + "." + s.Substring(s.Length - prec);
                }
                return (Neg(v) ? "-" : "") + s;
            }
            /// prec + 1 significant digits of a > 0 and the decimal exponent of the first one.
            static void Sci(double a, int prec, out string digits, out int exp)
            {
                int e = (int)Math.Floor(Math.Log10(a));
                BigInteger lo = BigInteger.Pow(10, prec), hi = lo * 10;
                for (int guard = 0; ; guard++)
                {
                    BigInteger q = ScaledRound(a, prec - e);
                    if (q >= hi && guard < 8) { e++; continue; }
                    if (q < lo && guard < 8) { e--; continue; }
                    digits = q.ToString(CultureInfo.InvariantCulture); exp = e; return;
                }
            }
            static string Exp(int e) => "e" + (e < 0 ? "-" : "+") + Math.Abs(e).ToString("00", CultureInfo.InvariantCulture);
            /// '%.<prec>e' % v.
            public static string E(double v, int prec)
            {
                if (double.IsNaN(v) || double.IsInfinity(v)) return Special(v);
                string sign = Neg(v) ? "-" : "";
                if (v == 0) return sign + "0" + (prec > 0 ? "." + new string('0', prec) : "") + "e+00";
                Sci(Math.Abs(v), prec, out string d, out int e);
                return sign + d.Substring(0, 1) + (prec > 0 ? "." + d.Substring(1) : "") + Exp(e);
            }
            /// '%g' % v ('%+g' with plus).
            public static string G(double v, bool plus = false)
            {
                if (double.IsNaN(v) || double.IsInfinity(v)) return (plus && !(v < 0) ? "+" : "") + Special(v);
                string sign = Neg(v) ? "-" : plus ? "+" : "";
                if (v == 0) return sign + "0";
                const int P = 6;
                Sci(Math.Abs(v), P - 1, out string d, out int x);
                string s;
                if (x >= -4 && x < P)
                {
                    s = F(Math.Abs(v), P - 1 - x);
                    if (s.IndexOf('.') >= 0) s = s.TrimEnd('0').TrimEnd('.');
                }
                else
                {
                    string mant = (d.Substring(0, 1) + "." + d.Substring(1)).TrimEnd('0').TrimEnd('.');
                    s = mant + Exp(x);
                }
                return sign + s;
            }
            /// Python's round(x, n) of a float.
            public static double Round(double x, int n) => UxNp.PyRound(x, n);
            public static string D(long v) => v.ToString(CultureInfo.InvariantCulture);
        }

        // ================================================================== prototypes and Elem helpers
        /// prototype(rsdata): the stock prototypes of a run (cached; never modified: SpecToElem works on copies).
        public sealed class Prototypes
        {
            readonly UxStock _stock;
            readonly Dictionary<string, (UxElem entity, long version, long unknown)> _cache = new Dictionary<string, (UxElem, long, long)>(StringComparer.Ordinal);
            readonly Dictionary<string, Dictionary<string, string>> _sig = new Dictionary<string, Dictionary<string, string>>(StringComparer.Ordinal);
            public Prototypes(UxStock stock) { _stock = stock ?? throw new UxException("the entities stage needs the stock Reflex library (prototypes)"); }
            public (UxElem entity, long version, long unknown) Get(string rsdata)
            {
                lock (_cache)
                {
                    if (!_cache.TryGetValue(rsdata, out var p)) { p = _stock.LibPrototype(rsdata); _cache[rsdata] = p; }
                    return p;
                }
            }
            public UxElem Entity(string rsdata) => Get(rsdata).entity;
            /// dict(prop_signature(prototype)): prop name -> RSData type.
            public Dictionary<string, string> Signature(string rsdata)
            {
                lock (_cache)
                {
                    if (_sig.TryGetValue(rsdata, out var s)) return s;
                    s = new Dictionary<string, string>(StringComparer.Ordinal);
                    foreach (var kv in PropSignature(Entity(rsdata))) s[kv.Key] = kv.Value;
                    _sig[rsdata] = s;
                    return s;
                }
            }
            public string Md5(string rsdata) => UxBin.Md5(_stock.LibPrototypeBytes(rsdata));
        }

        /// props_of(entity): prop name -> prop RSData Elem, in order.
        public sealed class PropMap
        {
            public readonly List<string> names = new List<string>();
            readonly Dictionary<string, UxElem> _map = new Dictionary<string, UxElem>(StringComparer.Ordinal);
            public void Set(string name, UxElem e) { if (!_map.ContainsKey(name)) names.Add(name); _map[name] = e; }
            public bool Has(string name) => name != null && _map.ContainsKey(name);
            public UxElem this[string name] => _map.TryGetValue(name, out var e) ? e : throw new UxException("no prop " + name);
        }
        static string NsOf(UxElem prop) => ((prop.Attr("namespace") as string) ?? "").Replace(PFX, "");
        public static PropMap PropsOf(UxElem entity)
        {
            var m = new PropMap();
            foreach (var c in entity.children[0].children) m.Set(c.Attr("name") as string ?? "\0None", c);
            return m;
        }
        /// prop_signature(entity): [(prop name, RSData type)] in order.
        public static List<KeyValuePair<string, string>> PropSignature(UxElem entity)
        {
            var r = new List<KeyValuePair<string, string>>();
            foreach (var c in entity.children[0].children) r.Add(new KeyValuePair<string, string>(c.Attr("name") as string ?? "\0None", NsOf(c)));
            return r;
        }
        static bool SameSignature(List<KeyValuePair<string, string>> a, List<KeyValuePair<string, string>> b)
        {
            if (a.Count != b.Count) return false;
            for (int i = 0; i < a.Count; i++) if (a[i].Key != b[i].Key || a[i].Value != b[i].Value) return false;
            return true;
        }

        static void SetAttrStr(UxElem e, string name, string value)
        {
            foreach (var a in e.attrs) if (a.name == name) { a.type = 1; a.str = value; a.raw = null; return; }
            throw new UxException("attr " + name + " not on " + e.tag);
        }
        static void SetAttrRaw(UxElem e, string name, int type, byte[] value)
        {
            foreach (var a in e.attrs) if (a.name == name) { a.type = type; a.str = null; a.raw = value; return; }
            throw new UxException("attr " + name + " not on " + e.tag);
        }
        static byte[] PackI32(long v)
        {
            if (v < int.MinValue || v > int.MaxValue) throw new UxException("'i' format requires -2147483648 <= number <= 2147483647");
            var b = new byte[4]; UxBin.PutI32(b, 0, (int)v); return b;
        }
        static byte[] PackU32(long v)
        {
            if (v < 0 || v > uint.MaxValue) throw new UxException("'I' format requires 0 <= number <= 4294967295");
            var b = new byte[4]; UxBin.PutU32(b, 0, (uint)v); return b;
        }
        static byte[] PackF32(params double[] v)
        {
            var b = new byte[4 * v.Length];
            for (int i = 0; i < v.Length; i++)
            {
                float f = (float)v[i];
                if (float.IsInfinity(f) && !double.IsInfinity(v[i])) throw new UxException("float too large to pack with f format");
                UxBin.PutF32(b, 4 * i, f);
            }
            return b;
        }
        static string AssetField(object value, string key) => UxJson.Obj(value).Get(key) as string;

        /// set_value(prop, value): fills one property RSData Elem with a JSON-style value, in the prototype's own value encoding.
        public static void SetValue(UxElem prop, object value)
        {
            string ns = NsOf(prop);
            if (ns == "AssetRSData")
            {
                var a = prop.children[0];
                SetAttrStr(a, "name", AssetField(value, "asset")); SetAttrStr(a, "type", AssetField(value, "type"));
                return;
            }
            if (ns == "AssetListRSData")
            {
                if (prop.children.Count == 0) throw new UxException("prototype " + prop.Attr("name") + " list is empty: no element template");
                var tmpl = prop.children[0];
                var kids = new List<UxElem>();
                foreach (object v in (System.Collections.IEnumerable)value)
                {
                    var k = tmpl.DeepCopy();
                    var a = k.children[0];
                    SetAttrStr(a, "name", AssetField(v, "asset")); SetAttrStr(a, "type", AssetField(v, "type"));
                    kids.Add(k);
                }
                prop.children = kids;
                return;
            }
            if (ns == "EnumRSData")
            {
                var o = UxJson.Obj(value);
                UxElem en = prop.Find("Enum"), ev = prop.Find("EnumValue");
                en.value = Py.Str(o.Get("Enum")); ev.value = PackI32(Py.Int(o.Get("EnumValue")));
                return;
            }
            if (prop.children.Count != 1) throw new UxException("prop " + prop.Attr("name") + " (" + ns + ") has " + prop.children.Count + " value children");
            var val = prop.children[0];
            switch (val.vtype)
            {
                case 1: val.value = Py.Str(value); return;
                case 3: val.value = PackI32(Py.Int(value)); return;
                case 4: case 11: val.value = PackU32(Py.Int(value)); return;
                case 5: val.value = PackF32(Py.Num(value)); return;
                case 7: case 8: case 10:
                {
                    int n = val.vtype == 7 ? 4 : val.vtype == 8 ? 16 : 3;
                    var vals = new List<double>();
                    foreach (object x in (System.Collections.IEnumerable)value) vals.Add(Py.Num(x));
                    if (vals.Count != n) throw new UxException("pack expected " + n + " items for packing (got " + vals.Count + ")");
                    val.value = PackF32(vals.ToArray());
                    return;
                }
            }
            throw new UxException("prop " + prop.Attr("name") + " value type " + val.vtype + " not handled");
        }

        static JObj AssetOf(UxElem a) => new JObj().Set("asset", a.Attr("name")).Set("type", a.Attr("type"));
        /// get_value(prop): the JSON-style value of a property RSData Elem (JObj {asset, type} / a list of them / {Enum, EnumValue} /
        /// string / long / double / a list of doubles), or null.
        public static object GetValue(UxElem prop)
        {
            string ns = NsOf(prop);
            if (ns == "AssetRSData") return AssetOf(prop.children[0]);
            if (ns == "AssetListRSData")
            {
                var l = new List<object>();
                foreach (var k in prop.children) l.Add(AssetOf(k.children[0]));
                return l;
            }
            if (ns == "EnumRSData")
                return new JObj().Set("Enum", prop.Find("Enum").value).Set("EnumValue", (long)UxBin.I32LE((byte[])prop.Find("EnumValue").value, 0));
            if (prop.children.Count != 1) return null;
            var v = prop.children[0];
            if (v.vtype == 1) return v.value;
            byte[] b = v.value as byte[];
            switch (v.vtype)
            {
                case 3: return (long)UxBin.I32LE(b, 0);
                case 4: case 11: return (long)UxBin.U32LE(b, 0);
                case 5: return (double)UxBin.F32LE(b, 0);
                case 7: case 8: case 10:
                {
                    int n = v.vtype == 7 ? 4 : v.vtype == 8 ? 16 : 3;
                    var l = new List<object>(n);
                    for (int i = 0; i < n; i++) l.Add((double)UxBin.F32LE(b, 4 * i));
                    return l;
                }
            }
            return null;
        }

        /// One entities.json spec.
        public sealed class Spec
        {
            public long id; public string name, type, subscene, cls, role; public double[] matrix; public JObj props, source, start; public bool collision;
            public JObj ToJson()
            {
                var o = new JObj().Set("id", id).Set("name", name).Set("type", type).Set("subscene", subscene).Set("matrix", matrix).Set("props", props)
                    .Set("source", source).Set("cls", cls).Set("role", role).Set("collision", collision);
                if (start != null) o.Set("start", start);
                return o;
            }
            /// A spec of a parsed entities.json (load_elems).
            public static Spec FromJson(JObj o)
            {
                var m = UxJson.Arr(o.Get("matrix")); var mm = new double[m.Count];
                for (int i = 0; i < mm.Length; i++) mm[i] = UxJson.Num(m[i]);
                return new Spec { id = UxJson.Int(o.Get("id")), name = o.Get("name") as string, type = o.Get("type") as string, subscene = o.Get("subscene") as string,
                    cls = o.Get("cls") as string, role = o.Get("role") as string, matrix = mm, props = UxJson.Obj(o.Get("props")), source = o.Get("source") as JObj,
                    start = o.Get("start") as JObj, collision = Py.Truthy(o.Get("collision")) };
            }
        }

        /// spec_to_elem(spec): a clone of the prototype of spec.type with entityId, Name, Id, Matrix and the props filled in.
        public static UxElem SpecToElem(Spec spec, Prototypes protos)
        {
            var e = protos.Entity(spec.type).DeepCopy();
            SetAttrRaw(e, "entityId", 4, PackU32(spec.id));
            var P = PropsOf(e);
            List<string> extra = null;
            foreach (var kv in spec.props.items) if (!P.Has(kv.Key)) (extra ?? (extra = new List<string>())).Add(kv.Key);
            if (extra != null) throw new UxException(spec.type + " " + spec.name + ": props not in the prototype: " + Py.Str(extra));
            SetValue(P["Name"], spec.name);
            SetValue(P["Id"], spec.id);
            SetValue(P["Matrix"], spec.matrix);
            foreach (var kv in spec.props.items) SetValue(P[kv.Key], kv.Value);
            return e;
        }

        /// chunk_version(): the (version, unknown) words of the SceneChunk documents (the ModelRSData prototype's).
        public static (long version, long unknown) ChunkVersion(Prototypes protos) { var p = protos.Get("ModelRSData"); return (p.version, p.unknown); }

        /// encode_chunks(elems): the encoded SceneChunk BXML documents (at most CHUNK_MAX entities each; zlib level 6 as Python's).
        public static List<byte[]> EncodeChunks(IList<UxElem> elems, Prototypes protos)
        {
            var (_, ver, unk) = protos.Get("ModelRSData");
            var o = new List<byte[]>();
            for (int i = 0; i < elems.Count; i += CHUNK_MAX)
            {
                var kids = new List<UxElem>();
                for (int k = i; k < Math.Min(elems.Count, i + CHUNK_MAX); k++) kids.Add(elems[k]);
                o.Add(UxBxml.EncodeTree(new UxElem("SceneChunk", null, 0, 0, null, kids), ver, unk));
            }
            return o;
        }

        static bool ValueEqual(object a, object b)
        {
            if (a == null || b == null) return a == null && b == null;
            if (a is string sa) return b is string sb && sa == sb;
            if (a is byte[] ba) return b is byte[] bb && UxBin.Same(ba, bb);
            return false;
        }
        /// elem_equal(a, b).
        public static bool ElemEqual(UxElem a, UxElem b)
        {
            if (a.tag != b.tag || a.vtype != b.vtype || a.vcount != b.vcount || !ValueEqual(a.value, b.value)) return false;
            if (a.attrs.Count != b.attrs.Count || a.children.Count != b.children.Count) return false;
            for (int i = 0; i < a.attrs.Count; i++)
                if (a.attrs[i].name != b.attrs[i].name || a.attrs[i].type != b.attrs[i].type || !ValueEqual(a.attrs[i].Value, b.attrs[i].Value)) return false;
            for (int i = 0; i < a.children.Count; i++) if (!ElemEqual(a.children[i], b.children[i])) return false;
            return true;
        }

        // ================================================================== geometry helpers
        static double[] Nums(object list)
        {
            var l = UxJson.Arr(list); var r = new double[l.Count];
            for (int i = 0; i < r.Length; i++) r[i] = Py.Num(l[i]);
            return r;
        }
        /// mat4(m): the 4 x 4 float64 of a 16-float matrix, every element rounded to float32 first.
        public static double[] Mat4(double[] m)
        {
            if (m.Length != 16) throw new UxException("cannot reshape array of size " + m.Length + " into shape (4,4)");
            var r = new double[16];
            for (int i = 0; i < 16; i++) r[i] = (double)(float)m[i];
            return r;
        }
        /// f32list(a).
        public static double[] F32List(double[] a) { var r = new double[a.Length]; for (int i = 0; i < r.Length; i++) r[i] = UxNp.F32(a[i]); return r; }
        static bool Any(double[] v) { if (v == null) return false; foreach (double x in v) if (x != 0) return true; return false; }
        /// lifted(m16, offset, lift): translate(offset) x M, then + (0, lift, 0); float32-exact.
        public static double[] Lifted(double[] m16, double[] offset, double lift)
        {
            double[] M = Mat4(m16);
            if (Any(offset))
            {
                var T = new double[] { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, offset[0], offset[1], offset[2], 1 };
                M = UxNp.MatMul(T, 4, 4, M, 4);
            }
            M[13] += lift;
            return F32List(M);
        }
        static double RowNorm(double[] M, int r) => UxNp.Norm(new[] { M[4 * r], M[4 * r + 1], M[4 * r + 2] });
        /// is_scaled(m16).
        public static bool IsScaled(double[] m16, double tol = 1e-3)
        {
            double[] M = Mat4(m16);
            for (int r = 0; r < 3; r++) if (Math.Abs(RowNorm(M, r) - 1.0) > tol) return true;
            return false;
        }
        /// unscaled(M).
        public static double[] Unscaled(double[] m16)
        {
            double[] R = Mat4(m16);
            for (int i = 0; i < 3; i++)
            {
                double n = RowNorm(R, i);
                if (n > 0) { R[4 * i] /= n; R[4 * i + 1] /= n; R[4 * i + 2] /= n; }
            }
            return R;
        }

        /// point_tri_dist for one triangle T[o .. o + 9) = A, B, C (Ericson 5.1.5), in numpy's order of operations.
        static double PointTriDist(double px, double py, double pz, double[] T, int o)
        {
            double ax = T[o], ay = T[o + 1], az = T[o + 2], bx = T[o + 3], by = T[o + 4], bz = T[o + 5], cx = T[o + 6], cy = T[o + 7], cz = T[o + 8];
            double abx = bx - ax, aby = by - ay, abz = bz - az, acx = cx - ax, acy = cy - ay, acz = cz - az, apx = px - ax, apy = py - ay, apz = pz - az;
            double d1 = abx * apx + aby * apy + abz * apz, d2 = acx * apx + acy * apy + acz * apz;
            double bpx = px - bx, bpy = py - by, bpz = pz - bz;
            double d3 = abx * bpx + aby * bpy + abz * bpz, d4 = acx * bpx + acy * bpy + acz * bpz;
            double cpx = px - cx, cpy = py - cy, cpz = pz - cz;
            double d5 = abx * cpx + aby * cpy + abz * cpz, d6 = acx * cpx + acy * cpy + acz * cpz;
            double va = d3 * d6 - d5 * d4, vb = d5 * d2 - d1 * d6, vc = d1 * d4 - d3 * d2;
            double den = va + vb + vc;
            if (Math.Abs(den) < 1e-30) den = 1e-30;
            double v = vb / den, w = vc / den;
            double rx = ax + abx * v + acx * w, ry = ay + aby * v + acy * w, rz = az + abz * v + acz * w;
            if (vc <= 0 && d1 >= 0 && d3 <= 0)
            {
                double dd = d1 - d3; if (Math.Abs(dd) < 1e-30) dd = 1e-30;
                double t = d1 / dd; rx = ax + abx * t; ry = ay + aby * t; rz = az + abz * t;
            }
            if (vb <= 0 && d2 >= 0 && d6 <= 0)
            {
                double dd = d2 - d6; if (Math.Abs(dd) < 1e-30) dd = 1e-30;
                double t = d2 / dd; rx = ax + acx * t; ry = ay + acy * t; rz = az + acz * t;
            }
            if (va <= 0 && (d4 - d3) >= 0 && (d5 - d6) >= 0)
            {
                double dd = (d4 - d3) + (d5 - d6); if (Math.Abs(dd) < 1e-30) dd = 1e-30;
                double t = (d4 - d3) / dd; rx = bx + (cx - bx) * t; ry = by + (cy - by) * t; rz = bz + (cz - bz) * t;
            }
            if (d1 <= 0 && d2 <= 0) { rx = ax; ry = ay; rz = az; }
            if (d3 >= 0 && d4 <= d3) { rx = bx; ry = by; rz = bz; }
            if (d6 >= 0 && d5 <= d6) { rx = cx; ry = cy; rz = cz; }
            double ex = rx - px, ey = ry - py, ez = rz - pz;
            return Math.Sqrt(ex * ex + ey * ey + ez * ez);
        }

        /// The nearest of the triangles tris[9 * i ..) whose box (lo / hi, 3 per triangle) lies within `radius` of p: np.argmin of the
        /// distances of the masked triangles (the first minimum; the first NaN if any). index -1: no triangle in reach.
        static void NearestIn(double[] tris, double[] lo, double[] hi, int n, double px, double py, double pz, double radius, out double dist, out int index)
        {
            dist = double.PositiveInfinity; index = -1; bool nan = false;
            double hx = px + radius, hy = py + radius, hz = pz + radius, lx = px - radius, ly = py - radius, lz = pz - radius;
            for (int i = 0; i < n; i++)
            {
                int q = 3 * i;
                if (!(lo[q] <= hx && lo[q + 1] <= hy && lo[q + 2] <= hz && hi[q] >= lx && hi[q + 1] >= ly && hi[q + 2] >= lz)) continue;
                double d = PointTriDist(px, py, pz, tris, 9 * i);
                if (nan) continue;
                if (double.IsNaN(d)) { nan = true; dist = d; index = i; continue; }
                if (index < 0 || d < dist) { dist = d; index = i; }
            }
        }
        static void Boxes(double[] tris, int n, out double[] lo, out double[] hi)
        {
            lo = new double[3 * n]; hi = new double[3 * n];
            for (int i = 0; i < n; i++)
                for (int c = 0; c < 3; c++)
                {
                    double a = tris[9 * i + c], b = tris[9 * i + 3 + c], d = tris[9 * i + 6 + c];
                    lo[3 * i + c] = Math.Min(Math.Min(a, b), d); hi[3 * i + c] = Math.Max(Math.Max(a, b), d);
                }
        }
        /// t @ M[:3, :3] + M[3, :3] for triangles (n, 3, 3) (matmul of each 3 x 3 stack item: fused chains) or points (n, 3).
        static double[] Transform(double[] t, double[] M)
        {
            var r = new double[t.Length];
            for (int i = 0; i < t.Length; i += 3)
                for (int j = 0; j < 3; j++)
                {
                    double s = t[i] * M[j];
                    s = UxNp.Fma(t[i + 1], M[4 + j], s);
                    s = UxNp.Fma(t[i + 2], M[8 + j], s);
                    r[i + j] = s + M[12 + j];
                }
            return r;
        }

        // ================================================================== level inputs
        /// One level.json entity (the fields the stage reads).
        public sealed class Ent
        {
            public JObj j, props; public long id; public string cls, type, name, reflex; public bool collision; public double[] matrix;
            public Ent(JObj e)
            {
                j = e; id = UxJson.Int(e.Get("id")); cls = e.Get("cls") as string; type = e.Get("type") as string; name = Py.Str(e.Get("name"));
                reflex = e.Get("reflex") as string; collision = Py.Truthy(e.Get("collision")); props = e.Get("props") as JObj ?? new JObj();
                matrix = e.Get("matrix") is List<object> ? Nums(e.Get("matrix")) : null;
            }
            public bool Kept => cls == "keep" || cls == "demote";
            public string Asset(string prop) => (props.Get(prop) as JObj)?.Get("asset") as string;
        }
        static List<Ent> Entities(JObj d)
        {
            var r = new List<Ent>();
            foreach (object e in UxJson.Arr(d.Get("entities"))) r.Add(new Ent(UxJson.Obj(e)));
            return r;
        }

        /// ExtractIndex: the case-insensitive closure lookup (name, type) -> the canonical closure key / record.
        public sealed class ExtractIndex
        {
            readonly Dictionary<string, Dictionary<string, string>> _low = new Dictionary<string, Dictionary<string, string>>(StringComparer.Ordinal);
            readonly Dictionary<string, Dictionary<string, JObj>> _rec = new Dictionary<string, Dictionary<string, JObj>>(StringComparer.Ordinal);
            public ExtractIndex(JObj levelJson)
            {
                foreach (var t in UxJson.Obj(levelJson.Get("closure")).items)
                {
                    var low = new Dictionary<string, string>(StringComparer.Ordinal); var rec = new Dictionary<string, JObj>(StringComparer.Ordinal);
                    foreach (var kv in UxJson.Obj(t.Value).items) { low[UxCommon.PyLower(kv.Key)] = kv.Key; rec[kv.Key] = kv.Value as JObj; }
                    _low[t.Key] = low; _rec[t.Key] = rec;
                }
            }
            /// canon(name, typ): null when the closure has no such asset (a null name is looked up as 'none', as str(None).lower()).
            public string Canon(string name, string typ)
                => _low.TryGetValue(typ, out var m) && m.TryGetValue(UxCommon.PyLower(name ?? "None"), out string k) ? k : null;
            public JObj Rec(string name, string typ)
            {
                string k = Canon(name, typ);
                return k == null ? null : _rec[typ][k];
            }
        }

        static bool ContainsNoCase(string s, string part) => s.IndexOf(part, StringComparison.OrdinalIgnoreCase) >= 0;
        /// design_exclusions(lv, mats): the HF exclusions (render material indices; -1 = collision-only): '*Over*' decks on every level,
        /// and on the EC levels Rocks / Logs / Log_Ends plus the collision-only triangles.
        public static List<int> DesignExclusions(UxLevelRow lv, List<string> mats)
        {
            bool ec = lv.mode == "ec";
            var ex = new List<int>();
            for (int k = 0; k < mats.Count; k++)
                if (ContainsNoCase(mats[k], "over") || ec && (ContainsNoCase(mats[k], "rock") || ContainsNoCase(mats[k], "log"))) ex.Add(k);
            if (ec) ex.Add(-1);
            return ex;
        }

        /// One trncol surface over a point: (y, triangle, material index, normal y).
        public struct Surface { public double y; public int tri, mat; public double ny; }

        /// TerrainQuery: point queries on the Untamed terrain collision (.trncol) of a level, in the Untamed frame (no lift).
        public sealed class TerrainQuery
        {
            public readonly int n;
            /// The triangle corners, 3 per triangle.
            public readonly double[] A, B, C;
            public readonly double[] ny, xmin, xmax, zmin, zmax;
            public readonly int[] tri_mat; public readonly bool[] kept; public readonly List<string> mats; public readonly List<int> excl;
            // a uniform x / z grid of triangle lists (ascending triangle index), only to find the candidates np.nonzero(mask) would
            readonly int _gx, _gz; readonly double _x0, _z0, _cell; readonly int[] _start, _list;

            public TerrainQuery(UxLevelRow lv, JObj d, UxDiscLevel disc, ExtractIndex idx, CancellationToken cancel = default)
            {
                var t = UxJson.Obj(d.Get("terrain"));
                JObj tr = idx.Rec(t.Get("terrain") as string, "terrain"), tc = idx.Rec(t.Get("trncol") as string, "trncol");
                if (tr == null || tc == null) throw new UxException(lv.disc + ": level.json names no terrain / trncol of the closure");
                var T = UxTerrain.ReadTerrain(disc.ReadExtractFile(tr.Get("file") as string, cancel));
                var Cc = UxTerrain.ReadTrncol(disc.ReadExtractFile(tc.Get("file") as string, cancel));
                mats = new List<string>(); foreach (object m in UxJson.Arr(t.Get("materials"))) mats.Add(m as string);
                tri_mat = UxTerrain.CollisionMaterials(T, Cc);
                excl = DesignExclusions(lv, mats);
                n = Cc.ntris; double[] V = Cc.verts; int[] F = Cc.tris;
                A = new double[3 * n]; B = new double[3 * n]; C = new double[3 * n];
                ny = new double[n]; xmin = new double[n]; xmax = new double[n]; zmin = new double[n]; zmax = new double[n]; kept = new bool[n];
                double gx0 = double.PositiveInfinity, gx1 = double.NegativeInfinity, gz0 = double.PositiveInfinity, gz1 = double.NegativeInfinity;
                for (int i = 0; i < n; i++)
                {
                    for (int c = 0; c < 3; c++) { A[3 * i + c] = V[3 * F[3 * i] + c]; B[3 * i + c] = V[3 * F[3 * i + 1] + c]; C[3 * i + c] = V[3 * F[3 * i + 2] + c]; }
                    double ux = B[3 * i] - A[3 * i], uy = B[3 * i + 1] - A[3 * i + 1], uz = B[3 * i + 2] - A[3 * i + 2];
                    double vx = C[3 * i] - A[3 * i], vy = C[3 * i + 1] - A[3 * i + 1], vz = C[3 * i + 2] - A[3 * i + 2];
                    double n0 = uy * vz - uz * vy, n1 = uz * vx - ux * vz, n2 = ux * vy - uy * vx;       // np.cross
                    double L = Math.Sqrt(n0 * n0 + n1 * n1 + n2 * n2);
                    ny[i] = L > 0 ? n1 / Math.Max(L, 1e-30) : 0.0;
                    xmin[i] = Math.Min(Math.Min(A[3 * i], B[3 * i]), C[3 * i]); xmax[i] = Math.Max(Math.Max(A[3 * i], B[3 * i]), C[3 * i]);
                    zmin[i] = Math.Min(Math.Min(A[3 * i + 2], B[3 * i + 2]), C[3 * i + 2]); zmax[i] = Math.Max(Math.Max(A[3 * i + 2], B[3 * i + 2]), C[3 * i + 2]);
                    kept[i] = !excl.Contains(tri_mat[i]);
                    if (xmin[i] < gx0) gx0 = xmin[i]; if (xmax[i] > gx1) gx1 = xmax[i]; if (zmin[i] < gz0) gz0 = zmin[i]; if (zmax[i] > gz1) gz1 = zmax[i];
                }
                if (n == 0 || !UxNp.Finite(gx0, gx1, gz0, gz1)) { _gx = _gz = 1; _cell = 1; _x0 = _z0 = 0; _start = new int[2]; _list = new int[0]; return; }
                const double Pad = 4e-6;
                _x0 = gx0 - Pad; _z0 = gz0 - Pad;
                _cell = Math.Max(1.0, Math.Max(gx1 - gx0, gz1 - gz0) / 160.0);
                _gx = (int)Math.Floor((gx1 + Pad - _x0) / _cell) + 1; _gz = (int)Math.Floor((gz1 + Pad - _z0) / _cell) + 1;
                var count = new int[_gx * _gz + 1];
                for (int pass = 0; pass < 2; pass++)
                {
                    for (int i = 0; i < n; i++)
                    {
                        if (double.IsNaN(xmin[i]) || double.IsNaN(zmin[i])) continue;
                        int ix0 = Cell(xmin[i] - 2e-6, _x0, _gx), ix1 = Cell(xmax[i] + 2e-6, _x0, _gx), iz0 = Cell(zmin[i] - 2e-6, _z0, _gz), iz1 = Cell(zmax[i] + 2e-6, _z0, _gz);
                        for (int ix = ix0; ix <= ix1; ix++)
                            for (int iz = iz0; iz <= iz1; iz++)
                            {
                                int c = ix * _gz + iz;
                                if (pass == 0) count[c + 1]++; else _list[count[c]++] = i;
                            }
                    }
                    if (pass == 0)
                    {
                        for (int c = 0; c < _gx * _gz; c++) count[c + 1] += count[c];
                        _start = (int[])count.Clone(); _list = new int[count[_gx * _gz]];
                    }
                }
            }
            int Cell(double v, double origin, int cells)
            {
                double f = Math.Floor((v - origin) / _cell);
                return f < 0 ? 0 : f >= cells ? cells - 1 : (int)f;
            }

            /// surfaces(x, z): all trncol surfaces over (x, z), highest first (then the higher triangle index, as sorted(reverse=True)).
            public List<Surface> Surfaces(double x, double z)
            {
                var o = new List<Surface>();
                if (n == 0 || _list.Length == 0 || double.IsNaN(x) || double.IsNaN(z)) return o;
                int cell = Cell(x, _x0, _gx) * _gz + Cell(z, _z0, _gz);
                for (int q = _start[cell]; q < _start[cell + 1]; q++)
                {
                    int t = _list[q];
                    if (!(xmin[t] <= x + 1e-6 && xmax[t] >= x - 1e-6 && zmin[t] <= z + 1e-6 && zmax[t] >= z - 1e-6)) continue;
                    double a0 = A[3 * t], a1 = A[3 * t + 1], a2 = A[3 * t + 2], b0 = B[3 * t], b1 = B[3 * t + 1], b2 = B[3 * t + 2], c0 = C[3 * t], c1 = C[3 * t + 1], c2 = C[3 * t + 2];
                    double det = (b0 - a0) * (c2 - a2) - (c0 - a0) * (b2 - a2);
                    if (Math.Abs(det) < 1e-12) continue;
                    double l1 = ((x - a0) * (c2 - a2) - (c0 - a0) * (z - a2)) / det;
                    double l2 = ((b0 - a0) * (z - a2) - (x - a0) * (b2 - a2)) / det;
                    double l0 = 1 - l1 - l2;
                    if (Math.Min(Math.Min(l0, l1), l2) < -1e-7) continue;
                    o.Add(new Surface { y = l0 * a1 + l1 * b1 + l2 * c1, tri = t, mat = tri_mat[t], ny = ny[t] });
                }
                // sorted(out, reverse=True): tuples (y, tri, ...) descending; the triangle index is unique
                o.Sort((p, r) => p.y != r.y ? r.y.CompareTo(p.y) : r.tri.CompareTo(p.tri));
                return o;
            }
            /// top(x, z, kept_only): the highest (kept) surface at (x, z), or null.
            public Surface? Top(double x, double z, bool keptOnly = true)
            {
                foreach (var s in Surfaces(x, z)) if (!keptOnly || kept[s.tri]) return s;
                return null;
            }
        }

        /// np.load of a .npy file: the shape and the values as float64 (any little-endian float or integer dtype, C order).
        public static double[] ReadNpy(byte[] b, out int[] shape)
        {
            UxBin.Require(b.Length >= 10 && b[0] == 0x93 && b[1] == (byte)'N' && b[2] == (byte)'U' && b[3] == (byte)'M' && b[4] == (byte)'P' && b[5] == (byte)'Y', "not a .npy file");
            int hl, hp;
            if (b[6] == 1) { hl = UxBin.U16LE(b, 8); hp = 10; } else { hl = (int)UxBin.U32LE(b, 8); hp = 12; }
            string h = UxBin.Latin1(b, hp, hl);
            string Field(string key)
            {
                int k = h.IndexOf("'" + key + "'", StringComparison.Ordinal); UxBin.Require(k >= 0, ".npy header without " + key);
                int c = h.IndexOf(':', k) + 1;
                while (h[c] == ' ') c++;
                int e;
                if (h[c] == '\'') { e = h.IndexOf('\'', c + 1); return h.Substring(c + 1, e - c - 1); }
                if (h[c] == '(') { e = h.IndexOf(')', c); return h.Substring(c + 1, e - c - 1); }
                e = h.IndexOf(',', c); return h.Substring(c, e - c).Trim();
            }
            string descr = Field("descr");
            UxBin.Require(Field("fortran_order") == "False", ".npy in Fortran order");
            var dims = new List<int>();
            foreach (string s in Field("shape").Split(',')) if (s.Trim().Length > 0) dims.Add(int.Parse(s.Trim(), CultureInfo.InvariantCulture));
            shape = dims.ToArray();
            long count = 1; foreach (int s in shape) count *= s;
            UxBin.Require(descr.Length >= 3 && (descr[0] == '<' || descr[0] == '|'), ".npy dtype " + descr);
            char kind = descr[1]; int size = int.Parse(descr.Substring(2), CultureInfo.InvariantCulture);
            int p = hp + hl;
            UxBin.Require(p + count * size <= b.Length, ".npy truncated");
            var r = new double[count];
            for (long i = 0; i < count; i++, p += size)
            {
                if (kind == 'f' && size == 4) r[i] = UxBin.F32LE(b, p);
                else if (kind == 'f' && size == 8) r[i] = BitConverter.Int64BitsToDouble((long)UxBin.U64(b, p, false));
                else if (kind == 'f' && size == 2) r[i] = UxNp.HalfToFloat(UxBin.U16LE(b, p));
                else if (kind == 'i' && size == 4) r[i] = UxBin.I32LE(b, p);
                else if (kind == 'i' && size == 8) r[i] = (long)UxBin.U64(b, p, false);
                else if (kind == 'i' && size == 2) r[i] = UxBin.I16(b, p, false);
                else if (kind == 'i' && size == 1) r[i] = (sbyte)b[p];
                else if (kind == 'u' && size == 4) r[i] = UxBin.U32LE(b, p);
                else if (kind == 'u' && size == 8) r[i] = UxBin.U64(b, p, false);
                else if (kind == 'u' && size == 2) r[i] = UxBin.U16LE(b, p);
                else if ((kind == 'u' || kind == 'b') && size == 1) r[i] = b[p];
                else throw new UxException(".npy dtype " + descr + " is not handled");
            }
            return r;
        }

        /// HeightField: ground height for placements (ft, lifted frame). Source: the terrain stage's hf.npy (DISP orientation: sample
        /// (r, c) at x = c + 0.5 - 512, z = 512 - r - 0.5, bilinear); a stub when it is missing: the highest kept trncol surface + lift.
        public sealed class HeightField
        {
            readonly double[] _h; readonly Func<TerrainQuery> _tq; readonly double _lift;
            public readonly string source, path;
            public bool Present => _h != null;
            public HeightField(UxLevelRow lv, UxWork work, string path, Func<TerrainQuery> terrainQuery)
            {
                this.path = path; _tq = terrainQuery; _lift = UxCommon.YLift(lv);
                if (work.Exists(path))
                {
                    byte[] b = work.ReadBytes(path);
                    _h = ReadNpy(b, out int[] shape);
                    if (shape.Length != 2 || shape[0] != UxCommon.HF_SIZE || shape[1] != UxCommon.HF_SIZE) throw new UxException(path + ": shape " + Py.Str(Array.ConvertAll(shape, s => (object)(long)s)));
                    source = "WP6 " + path + " (md5 " + UxBin.Md5(b) + ")";
                }
                else source = "stub: highest kept trncol surface + " + Py.F(_lift, 0) + " ft (WP6 hf.npy missing)";
            }
            /// hf(x, z): null when the stub finds no surface.
            public double? At(double x, double z)
            {
                if (_h != null)
                {
                    const int n = UxCommon.HF_SIZE;
                    double c = x + n / 2.0 - 0.5, r = n / 2.0 - 0.5 - z;
                    double fc0 = Math.Floor(c), fr0 = Math.Floor(r);
                    if (double.IsNaN(fc0) || double.IsNaN(fr0)) throw new UxException("height field: cannot convert float NaN to integer");
                    double fc = c - fc0, fr = r - fr0;
                    int c0 = (int)Math.Min(Math.Max(fc0, 0), n - 2), r0 = (int)Math.Min(Math.Max(fr0, 0), n - 2);
                    double[] h = _h;
                    return (h[r0 * n + c0] * (1 - fc) + h[r0 * n + c0 + 1] * fc) * (1 - fr) + (h[(r0 + 1) * n + c0] * (1 - fc) + h[(r0 + 1) * n + c0 + 1] * fc) * fr;
                }
                var s = _tq().Top(x, z);
                return s == null ? (double?)null : s.Value.y + _lift;
            }
        }

        /// The Untamed collision triangles of the closure's collis files (uxmesh.collis_triangles; null when a file cannot be decoded
        /// or is a kind without triangles), read once per run.
        public sealed class CollisCache
        {
            readonly UxDiscLevel _disc; readonly CancellationToken _cancel;
            readonly Dictionary<string, double[]> _tris = new Dictionary<string, double[]>(StringComparer.Ordinal);
            public CollisCache(UxDiscLevel disc, CancellationToken cancel) { _disc = disc; _cancel = cancel; }
            public double[] Get(string file)
            {
                lock (_tris) if (_tris.TryGetValue(file, out var hit)) return hit;
                double[] t;
                try { t = UxMesh.CollisTriangles(_disc.ReadExtractFile(file, _cancel)); }
                catch (OperationCanceledException) { throw; }
                catch (Exception) { t = null; }
                lock (_tris) _tris[file] = t;
                return t;
            }
        }

        /// CollisionWorld: the world-space (lifted) collision triangles of the kept collision-bearing entities of a level (Untamed source
        /// geometry; kind 11 / 15 / 17 primitives as boxes, kind-2 point sets ignored).
        public sealed class CollisionWorld
        {
            public readonly int n; public readonly double[] tris, lo, hi; public readonly long[] owner;
            public CollisionWorld(UxLevelRow lv, List<Ent> entities, ExtractIndex idx, CollisCache collis, ICollection<long> excludeIds = null)
            {
                double lift = UxCommon.YLift(lv);
                var parts = new List<double[]>(); var own = new List<long>(); int total = 0;
                foreach (var e in entities)
                {
                    if (!e.Kept || !e.collision || excludeIds != null && excludeIds.Contains(e.id)) continue;
                    string fem = e.Asset("FemaleCollisionAsset");
                    JObj rec = Py.Truthy(fem) ? idx.Rec(fem, "collis") : null;
                    if (rec == null) continue;
                    double[] t = collis.Get(rec.Get("file") as string);
                    if (t == null || t.Length == 0) continue;
                    double[] w = Transform(t, Mat4(e.matrix));
                    for (int i = 1; i < w.Length; i += 3) w[i] += lift;
                    parts.Add(w); total += w.Length;
                    for (int i = 0; i < w.Length / 9; i++) own.Add(e.id);
                }
                tris = new double[total]; int at = 0;
                foreach (var p in parts) { Array.Copy(p, 0, tris, at, p.Length); at += p.Length; }
                owner = own.ToArray(); n = owner.Length;
                Boxes(tris, n, out lo, out hi);
            }
            /// nearest(p, radius): (distance, owner entity id) of the nearest kept collision triangle; (inf, null) when none is in reach.
            public (double dist, long? owner) Nearest(double px, double py, double pz, double radius = 30.0)
            {
                NearestIn(tris, lo, hi, n, px, py, pz, radius, out double d, out int k);
                return k < 0 ? (double.PositiveInfinity, (long?)null) : (d, owner[k]);
            }
            /// clearance(x, ground, z): the smallest distance from a rider-height segment (ground .. ground + height) to kept collision.
            public (double dist, long? owner) Clearance(double x, double ground, double z, double height = 4.0, int steps = 5, double radius = 30.0)
            {
                (double dist, long? owner) best = (double.PositiveInfinity, null);
                for (int k = 0; k < steps; k++)
                {
                    double y = ground + 0.25 + (height - 0.25) * k / Math.Max(1, steps - 1);
                    var r = Nearest(x, y, z, radius);
                    if (r.dist < best.dist) best = r;
                }
                return best;
            }
        }

        // ================================================================== manifests (meshes / materials / collision / terrain) with stubs
        /// One part of a source model (Manifests.Parts).
        public sealed class Part { public string part, surface; public double[] offset; public List<long> groups; public object collision; }

        public sealed class Manifests
        {
            public static readonly string[] Keys = { "meshes", "collision", "terrain", "materials", "hf" };
            public readonly ExtractIndex idx;
            public readonly Dictionary<string, string> paths = new Dictionary<string, string>(StringComparer.Ordinal);
            public readonly Dictionary<string, string> used = new Dictionary<string, string>(StringComparer.Ordinal), md5 = new Dictionary<string, string>(StringComparer.Ordinal);
            public readonly List<string> warnings = new List<string>();
            readonly Dictionary<string, List<object>> _meshes = new Dictionary<string, List<object>>(StringComparer.Ordinal);
            readonly JObj _meshSkipped = new JObj();
            readonly Dictionary<string, JObj> _collEntities = new Dictionary<string, JObj>(StringComparer.Ordinal), _collSources = new Dictionary<string, JObj>(StringComparer.Ordinal);
            public List<JObj> terrain_parts = new List<JObj>();
            public readonly Dictionary<string, Dictionary<string, string>> renames = new Dictionary<string, Dictionary<string, string>>(StringComparer.Ordinal);
            public readonly HashSet<string> dropped_materials = new HashSet<string>(StringComparer.Ordinal);

            public Manifests(JObj d, UxWork work, JObj renamesOption, Dictionary<string, string> pathOverrides)
            {
                idx = new ExtractIndex(d);
                paths["meshes"] = "meshes/meshes.json"; paths["collision"] = "collision/collision.json"; paths["terrain"] = "terrain/terrain.json";
                paths["materials"] = "materials/materials.json"; paths["hf"] = "terrain/hf.npy";
                if (pathOverrides != null)
                    foreach (var kv in pathOverrides)
                    {
                        if (!paths.ContainsKey(kv.Key)) throw new UxException("unknown manifest key '" + kv.Key + "'");
                        if (kv.Value != null) paths[kv.Key] = kv.Value.Replace('\\', '/');
                    }
                foreach (string k in Keys)
                {
                    bool there = work.Exists(paths[k]);
                    used[k] = there ? "present" : "stub";
                    md5[k] = there ? UxBin.Md5File(work.PathOf(paths[k])) : null;
                }
                // meshes
                if (work.Exists(paths["meshes"]))
                {
                    var mj = work.ReadJsonObj(paths["meshes"]);
                    JObj models = mj.Has("models") ? UxJson.Obj(mj.Get("models")) : mj;
                    if (mj.Get("skipped") is JObj sk) _meshSkipped = sk;
                    foreach (var kv in models.items)
                    {
                        switch (kv.Key) { case "skipped": case "schema": case "level": case "counts": case "source": case "tool": case "problems": case "warnings": continue; }
                        if (kv.Value is List<object> l) _meshes[UxCommon.PyLower(kv.Key)] = l;
                    }
                }
                // collision
                if (work.Exists(paths["collision"]))
                {
                    var cj = work.ReadJsonObj(paths["collision"]);
                    object em = d.Get("kind") as string == "fr" ? cj.Get("entities_fr") : null;
                    if (!Py.Truthy(em)) em = cj.Get("entities");
                    if (!Py.Truthy(em)) em = cj.Get("entity_map");
                    if (em is JObj emo) foreach (var kv in emo.items) _collEntities[kv.Key] = kv.Value as JObj;
                    object src = cj.Get("sources");
                    if (!Py.Truthy(src))
                    {
                        var s2 = new JObj();
                        foreach (var kv in cj.items) if (kv.Value is JObj v && v.Has("out_name")) s2.items.Add(kv);
                        src = s2;
                    }
                    foreach (var kv in UxJson.Obj(src).items) _collSources[UxCommon.PyLower(kv.Key)] = kv.Value as JObj;
                }
                // terrain
                if (work.Exists(paths["terrain"]))
                {
                    var tj = work.ReadJsonObj(paths["terrain"]);
                    object tp = tj.Get("excluded_parts");
                    if (!Py.Truthy(tp)) tp = tj.Get("parts");
                    if (tp is JObj dict)
                    {
                        var keys = new List<KeyValuePair<string, object>>(dict.items);
                        UxNp.StableSort(keys, (a, b) => string.CompareOrdinal(a.Key, b.Key));
                        foreach (var kv in keys)
                        {
                            var v = UxJson.Obj(kv.Value); var c = new JObj(); c.items.AddRange(v.items);
                            c.Set("name", v.Has("name") ? v.Get("name") : kv.Key);
                            terrain_parts.Add(c);
                        }
                    }
                    else if (tp is List<object> list) foreach (object p in list) terrain_parts.Add(UxJson.Obj(p));
                }
                // materials + renames
                if (work.Exists(paths["materials"]))
                {
                    var mj = work.ReadJsonObj(paths["materials"]);
                    JObj mats = mj.Has("materials") ? UxJson.Obj(mj.Get("materials")) : mj;
                    foreach (var kv in mats.items)
                    {
                        if (!(kv.Value is JObj v) || !v.Has("action")) continue;
                        object nn = v.Get("out_name"); if (!Py.Truthy(nn)) nn = v.Get("rename"); if (!Py.Truthy(nn)) nn = v.Get("asset");
                        if (Py.Truthy(nn) && !Py.Eq(nn, kv.Key)) Renames("material")[kv.Key] = Py.Str(nn);
                        if (Py.Str(v.Get("action")).StartsWith("drop", StringComparison.Ordinal)) dropped_materials.Add(kv.Key);
                    }
                }
                if (renamesOption != null)
                    foreach (var t in renamesOption.items)
                        foreach (var kv in UxJson.Obj(t.Value).items) Renames(t.Key)[kv.Key] = Py.Str(kv.Value);
            }
            Dictionary<string, string> Renames(string typ)
            {
                if (!renames.TryGetValue(typ, out var m)) { m = new Dictionary<string, string>(StringComparer.Ordinal); renames[typ] = m; }
                return m;
            }
            public string Renamed(string typ, string name) => name != null && renames.TryGetValue(typ, out var m) && m.TryGetValue(name, out string r) ? r : name;
            /// name(nm, typ): the canonical closure name, renamed.
            public string Name(string nm, string typ) => Renamed(typ, idx.Canon(nm, typ) ?? nm);

            /// parts(model, ngroups): the parts of a source model (a stub when meshes.json has none: the model itself).
            public List<Part> Parts(string model, int ngroups)
            {
                string c = idx.Canon(model, "model") ?? model;
                if (c == null) throw new UxException("an entity with a model was expected (ModelAsset is missing)");
                var o = new List<Part>();
                if (!_meshes.TryGetValue(UxCommon.PyLower(c), out var ps))
                {
                    if (used["meshes"] == "present" && !_meshSkipped.Has(c)) warnings.Add("meshes.json has no entry for model " + c + ": used the model as one part");
                    var g = new List<long>(); for (int i = 0; i < ngroups; i++) g.Add(i);
                    o.Add(new Part { part = c, surface = c, offset = new double[] { 0.0, 0.0, 0.0 }, groups = g, collision = null });
                    return o;
                }
                foreach (object po in ps)
                {
                    var p = UxJson.Obj(po);
                    object oo = p.Get("origin_offset");
                    double[] off = Py.Truthy(oo) ? Nums(oo) : new double[] { 0.0, 0.0, 0.0 };
                    object g = p.Get("material_index_map"); if (!Py.Truthy(g)) g = p.Get("groups");
                    var groups = new List<long>();
                    if (g is List<object> gl)
                    {
                        if (gl.Count > 0 && gl[0] is List<object>) foreach (object x in gl) { var pair = UxJson.Arr(x); groups.Add(Py.Int(pair[pair.Count - 1])); }
                        else foreach (object x in gl) groups.Add(Py.Int(x));
                    }
                    else if (g == null) for (int i = 0; i < ngroups; i++) groups.Add(i);
                    object part = p.Get("part"); if (!Py.Truthy(part)) part = p.Get("model");
                    object surface = p.Get("surface"); if (!Py.Truthy(surface)) surface = p.Get("part"); if (!Py.Truthy(surface)) surface = p.Get("model");
                    o.Add(new Part { part = part as string, surface = surface as string, offset = off, groups = groups, collision = p.Has("collision") ? p.Get("collision") : p.Get("carries_collision") });
                }
                return o;
            }
            /// skipped(model): the meshes stage's reason when it skipped the model, else null.
            public object Skipped(string model)
            {
                string c = idx.Canon(model, "model") ?? model;
                object r = _meshSkipped.Get(c);
                return Py.Truthy(r) ? r : _meshSkipped.Get(model);
            }

            /// collis(e): (female, male, source) of a collision-bearing entity, or (null, null, reason).
            public (string female, string male, string how) Collis(Ent e)
            {
                string fem = e.Asset("FemaleCollisionAsset"), mal = e.Asset("MaleCollisionAsset");
                if (!Py.Truthy(fem)) return (null, null, "no FemaleCollisionAsset");
                if (_collEntities.TryGetValue(Py.D(e.id), out JObj m) && m != null)
                {
                    if (!Py.Truthy(m.Get("female")))
                    {
                        object why = m.Get("reason"); if (!Py.Truthy(why)) why = m.Get("action"); if (!Py.Truthy(why)) why = "no female";
                        return (null, null, "collision.json: " + Py.Str(why));
                    }
                    string f = m.Get("female") as string;
                    return (f, Py.Truthy(m.Get("male")) ? m.Get("male") as string : f, "collision.json entity map");
                }
                JObj fr = idx.Rec(fem, "collis"), mr = Py.Truthy(mal) ? idx.Rec(mal, "collis") : null;
                bool Kind(JObj rec, long k) => rec != null && rec.Get("kind") != null && Py.IsNum(rec.Get("kind")) && Py.Num(rec.Get("kind")) == k;
                if (_collSources.Count > 0)
                {
                    _collSources.TryGetValue(UxCommon.PyLower(idx.Canon(fem, "collis") ?? fem), out JObj s);
                    if (s == null || !Py.Truthy(s.Get("out_name")) || Py.Str(s.Has("action") ? s.Get("action") : "").StartsWith("refuse", StringComparison.Ordinal))
                        return (null, null, "collision.json: female " + fem + " " + Py.Str(s != null && s.Has("action") ? s.Get("action") : "missing"));
                    string fo = s.Get("out_name") as string;
                    if (mr == null || Kind(mr, 2) || UxCommon.PyLower(idx.Canon(mal, "collis") ?? "") == UxCommon.PyLower(idx.Canon(fem, "collis") ?? ""))
                        return (fo, fo, "collision.json sources");
                    _collSources.TryGetValue(UxCommon.PyLower(idx.Canon(mal, "collis") ?? mal), out JObj sm);
                    return (fo, sm != null && Py.Truthy(sm.Get("out_name")) ? sm.Get("out_name") as string : fo, "collision.json sources");
                }
                if (fr == null) return (null, null, "female " + fem + " not in the closure");
                if (Kind(fr, 2) || Kind(fr, 19)) return (null, null, "female " + fem + " is kind " + Py.D(Py.Int(fr.Get("kind"))) + " (no Reflex female)");
                string fo2 = Name(fem, "collis");
                string mo = mr == null || Kind(mr, 2) ? fo2 : Name(mal, "collis");
                return (fo2, mo, "stub (WP5 collision.json missing)");
            }
        }

        // ================================================================== numpy's default_rng (PCG64 seeded through SeedSequence)
        /// np.random.default_rng(seed) for a small non-negative seed: SeedSequence (pool of 4 uint32) -> PCG64 (XSL-RR 128/64);
        /// random() = (next >> 11) * 2^-53.
        public sealed class Pcg64
        {
            ulong _sHi, _sLo, _iHi, _iLo;
            const ulong MulHi = 2549297995355413924UL, MulLo = 4865540595714422341UL;
            public Pcg64(uint seed)
            {
                const uint INIT_A = 0x43b0d7e5, MULT_A = 0x931e8875, INIT_B = 0x8b51f9dd, MULT_B = 0x58f38ded, MIX_MULT_L = 0xca01f9dd, MIX_MULT_R = 0x4973f715;
                uint hc = INIT_A;
                uint HashMix(uint value) { unchecked { value ^= hc; hc *= MULT_A; value *= hc; value ^= value >> 16; return value; } }
                uint Mix(uint x, uint y) { unchecked { uint r = MIX_MULT_L * x - MIX_MULT_R * y; r ^= r >> 16; return r; } }
                var pool = new uint[4];
                for (int i = 0; i < 4; i++) pool[i] = HashMix(i == 0 ? seed : 0u);
                for (int s = 0; s < 4; s++) for (int d = 0; d < 4; d++) if (s != d) pool[d] = Mix(pool[d], HashMix(pool[s]));
                var w = new uint[8]; uint hb = INIT_B;
                for (int i = 0; i < 8; i++) unchecked { uint v = pool[i % 4]; v ^= hb; hb *= MULT_B; v *= hb; v ^= v >> 16; w[i] = v; }
                ulong U(int k) => w[2 * k] | (ulong)w[2 * k + 1] << 32;
                ulong seedHi = U(0), seedLo = U(1), incHi = U(2), incLo = U(3);
                _sHi = 0; _sLo = 0;
                _iHi = incHi << 1 | incLo >> 63; _iLo = incLo << 1 | 1;
                Step();
                unchecked { ulong lo = _sLo + seedLo; _sHi = _sHi + seedHi + (lo < _sLo ? 1UL : 0UL); _sLo = lo; }
                Step();
            }
            static ulong High(ulong a, ulong b)
            {
                unchecked
                {
                    ulong a0 = (uint)a, a1 = a >> 32, b0 = (uint)b, b1 = b >> 32;
                    ulong t = a0 * b0, u = a1 * b0 + (t >> 32), v = a0 * b1 + (uint)u;
                    return a1 * b1 + (u >> 32) + (v >> 32);
                }
            }
            void Step()
            {
                unchecked
                {
                    ulong lo = _sLo * MulLo, hi = High(_sLo, MulLo) + _sHi * MulLo + _sLo * MulHi;
                    ulong lo2 = lo + _iLo; hi = hi + _iHi + (lo2 < lo ? 1UL : 0UL);
                    _sLo = lo2; _sHi = hi;
                }
            }
            public ulong Next()
            {
                Step();
                ulong x = _sHi ^ _sLo; int r = (int)(_sHi >> 58);
                return r == 0 ? x : x >> r | x << (64 - r);
            }
            public double NextDouble() => (Next() >> 11) * (1.0 / 9007199254740992.0);
            /// Generator.choice(len(p), size=n, p=p): cdf = p.cumsum() / its last; n uniforms; cdf.searchsorted(u, side='right').
            public int[] Choice(double[] p, int n)
            {
                var cdf = new double[p.Length]; double s = 0;
                for (int i = 0; i < p.Length; i++) { s = i == 0 ? p[0] : s + p[i]; cdf[i] = s; }
                double last = cdf[cdf.Length - 1];
                for (int i = 0; i < cdf.Length; i++) cdf[i] /= last;
                var idx = new int[n];
                for (int k = 0; k < n; k++)
                {
                    double u = NextDouble();
                    int lo = 0, hi = cdf.Length;
                    while (lo < hi) { int mid = (lo + hi) >> 1; if (cdf[mid] <= u) lo = mid + 1; else hi = mid; }
                    idx[k] = lo;
                }
                return idx;
            }
        }

        // ================================================================== ridable-role detection for Untamed collision
        /// ridable_collision(lv, d, tq): {entity id: info} for kept collision entities whose up-facing collision faces coincide (within
        /// 1 ft) with the driven trncol surface over >= 100 ft^2; those get the ridable role (DriveOn 1, MXBike).
        public static SortedDictionary<long, JObj> RidableCollision(List<Ent> entities, ExtractIndex idx, CollisCache collis, Func<TerrainQuery> tq, CancellationToken cancel = default)
        {
            var o = new SortedDictionary<long, JObj>();
            var rng = new Pcg64(7);
            foreach (var e in entities)
            {
                if (!e.Kept || !e.collision || In(PHYSICS_FAMILY, e.type)) continue;
                if (e.reflex != "CollisionRSData") continue;
                string fem = e.Asset("FemaleCollisionAsset");
                JObj rec = Py.Truthy(fem) ? idx.Rec(fem, "collis") : null;
                if (rec == null) continue;
                double[] t = collis.Get(rec.Get("file") as string);
                if (t == null || t.Length == 0) continue;
                cancel.ThrowIfCancellationRequested();
                double[] w = Transform(t, Mat4(e.matrix));
                int nt = w.Length / 9;
                var ar = new double[nt]; var up = new List<int>();
                for (int i = 0; i < nt; i++)
                {
                    int q = 9 * i;
                    double ux = w[q + 3] - w[q], uy = w[q + 4] - w[q + 1], uz = w[q + 5] - w[q + 2], vx = w[q + 6] - w[q], vy = w[q + 7] - w[q + 1], vz = w[q + 8] - w[q + 2];
                    double n0 = uy * vz - uz * vy, n1 = uz * vx - ux * vz, n2 = ux * vy - uy * vx;
                    double L = Math.Sqrt(n0 * n0 + n1 * n1 + n2 * n2);
                    ar[i] = L / 2; double nyv = L > 0 ? n1 / Math.Max(L, 1e-30) : 0;
                    if (nyv > 0.7 && ar[i] > 0.5) up.Add(i);
                }
                if (up.Count == 0) continue;
                var aru = new double[up.Count]; for (int i = 0; i < aru.Length; i++) aru[i] = ar[up[i]];
                double total = UxNp.Sum(aru);
                if (total < 100) continue;
                int n = (int)Math.Min(400, Math.Max(50, (long)Math.Truncate(total / 20)));
                var p = new double[aru.Length]; for (int i = 0; i < p.Length; i++) p[i] = aru[i] / total;
                int[] pick = rng.Choice(p, n);
                var u = new double[2 * n]; for (int i = 0; i < u.Length; i++) u[i] = rng.NextDouble();
                var Tq = tq();
                int hit = 0;
                for (int k = 0; k < n; k++)
                {
                    double u0 = u[2 * k], u1 = u[2 * k + 1];
                    if (u0 + u1 > 1) { u0 = 1 - u0; u1 = 1 - u1; }
                    int q = 9 * up[pick[k]];
                    double x = w[q] + (w[q + 3] - w[q]) * u0 + (w[q + 6] - w[q]) * u1;
                    double y = w[q + 1] + (w[q + 4] - w[q + 1]) * u0 + (w[q + 7] - w[q + 1]) * u1;
                    double z = w[q + 2] + (w[q + 5] - w[q + 2]) * u0 + (w[q + 8] - w[q + 2]) * u1;
                    bool any = false; double min = 0;
                    foreach (var s in Tq.Surfaces(x, z))
                    {
                        if (!(s.ny > 0.3)) continue;
                        double dy = Math.Abs(s.y - y);
                        if (!any || dy < min) { min = dy; any = true; }
                    }
                    if (any && min <= 1.0) hit++;
                }
                double area = total * hit / n;
                if (area >= 100)
                    // the keys in sorted order: UxJson does not sort the values of an integer-keyed dictionary (json.dumps(sort_keys=True) does)
                    o[e.id] = new JObj().Set("coincident_up_area_ft2", Py.Round(area, 1)).Set("hits", (long)hit).Set("name", e.name).Set("samples", (long)n).Set("up_area_ft2", Py.Round(total, 1));
            }
            return o;
        }

        // ================================================================== ux_startpack.py (the --bale-reseat part) and the tdf07 walk
        /// One item of a tdf07 stream: (id, size word, payload).
        public struct TdfItem { public uint id, size; public byte[] raw; }
        public sealed class TdfSection
        {
            public TdfItem head; public List<TdfItem> fields = new List<TdfItem>(); public List<List<TdfItem>> nodes = new List<List<TdfItem>>();
            /// section_kind(s): the u32 of field 17.
            public uint Kind
            {
                get { foreach (var f in fields) if (f.id == 17) return UxBin.U32BE(f.raw, 0); throw new UxException("tdf section without a kind field (17)"); }
            }
        }
        /// tdf07_to_reflex.split + structure: the sections of a tdf (a Reflex tdf with its size word, or an Untamed tdf07).
        public static List<TdfSection> TdfSections(byte[] data)
        {
            int b;
            if (data.Length >= 0x20 && UxBin.U32LE(data, 0) == (uint)(data.Length - 4) && UxBin.U32BE(data, 0x18) == 260) b = 4;
            else if (data.Length >= 0x1c && UxBin.U32BE(data, 0x14) == 260) b = 0;
            else throw new UxException("not a TDF: no 260-byte path field at 0x14 (tdf07) or 0x18 (Reflex)");
            long p = b + 24 + UxBin.U32BE(data, b + 20);
            var secs = new List<TdfSection>(); TdfSection cur = null; List<TdfItem> node = null;
            while (p + 8 <= data.Length)
            {
                uint id = UxBin.U32BE(data, (int)p), size = UxBin.U32BE(data, (int)p + 4);
                long n = size == 0x10 ? 12 : size;
                if (p + 8 + n > data.Length) throw new UxException("truncated item " + id + " at 0x" + p.ToString("x", CultureInfo.InvariantCulture));
                var it = new TdfItem { id = id, size = size, raw = UxBin.Slice(data, (int)p + 8, (int)n) };
                p += 8 + n;
                if (id == 11) { cur = new TdfSection { head = it }; secs.Add(cur); node = null; }
                else if (id == 20)
                {
                    if (cur == null) throw new UxException("tdf: a node before the first section");
                    node = new List<TdfItem> { it }; cur.nodes.Add(node);
                }
                else if (node != null) node.Add(it);
                else if (cur != null) cur.fields.Add(it);
            }
            if (p != data.Length) throw new UxException("stream does not end on an item (" + (data.Length - p) + " trailing bytes)");
            return secs;
        }
        static double[] TdfVec(List<TdfItem> node, uint tag)
        {
            byte[] raw = null;
            foreach (var it in node) if (it.id == tag) raw = it.raw;             // node_dict: the last item of an id
            if (raw == null) throw new UxException("tdf node without item " + tag);
            return new double[] { UxBin.F32BE(raw, 0), UxBin.F32BE(raw, 4), UxBin.F32BE(raw, 8) };
        }

        /// One footprint of ux_startpack.footprints: the Untamed id, the unique (x, z) of its collision triangles, the centre (x, z).
        public sealed class Footprint { public long id; public double[] P; public double[] ctr; }
        static double Zero(double v) => v == 0 ? 0.0 : v;
        /// footprints(lv, d, cw, types).
        public static List<Footprint> Footprints(List<Ent> entities, CollisionWorld cw, string[] types)
        {
            var first = new Dictionary<long, int>(); var count = new Dictionary<long, int>();
            for (int i = 0; i < cw.n; i++)
            {
                if (!first.ContainsKey(cw.owner[i])) { first[cw.owner[i]] = i; count[cw.owner[i]] = 0; }
                count[cw.owner[i]]++;
            }
            var o = new List<Footprint>();
            foreach (var e in entities)
            {
                if (!e.Kept || !In(types, e.reflex) && !In(types, e.type)) continue;
                if (!first.ContainsKey(e.id)) continue;
                var pts = new List<(double x, double z)>();
                for (int i = 0; i < cw.n; i++)
                {
                    if (cw.owner[i] != e.id) continue;
                    for (int v = 0; v < 3; v++) pts.Add((UxNp.Round(cw.tris[9 * i + 3 * v], 4), UxNp.Round(cw.tris[9 * i + 3 * v + 2], 4)));
                }
                pts.Sort((a, b) => a.x != b.x ? a.x.CompareTo(b.x) : a.z.CompareTo(b.z));          // np.unique(axis=0): rows in lexicographic order
                var P = new List<double>();
                for (int i = 0; i < pts.Count; i++)
                    if (i == 0 || pts[i].x != pts[i - 1].x || pts[i].z != pts[i - 1].z) { P.Add(pts[i].x); P.Add(pts[i].z); }
                o.Add(new Footprint { id = e.id, P = P.ToArray(), ctr = new[] { UxNp.F32(e.matrix[12]), UxNp.F32(e.matrix[14]) } });
            }
            return o;
        }

        const double TURN_DEG = 20.0, TURN_SPAN_FT = 40.0, BERM_FT = 3.0, CLASS_OUT_FT = 4.0, CLASS_IN_FT = 12.0, T_TOL = 0.05;
        /// One start-chute (kind 4) section of chute_models.
        public sealed class Chute
        {
            public int si, m; public double[] C, E22, E23, cum, turn; public int[] outside; public bool[] frozen; public double?[] berm22, berm23;
            public double[] E(int k) => k == 22 ? E22 : E23;
            public double? Berm(int j, int k) => k == 22 ? berm22[j] : berm23[j];
        }
        static double Hypot(double x, double y) => Math.Sqrt(x * x + y * y);            // np.hypot on the reference machine: no extra precision
        static double Dot2(double a0, double a1, double b0, double b1) => UxNp.Fma(a1, b1, a0 * b0);    // a @ b of two 2-vectors (BLAS ddot)
        /// chute_models(secs, hf).
        public static List<Chute> ChuteModels(List<TdfSection> secs, HeightField hf)
        {
            var allc = new Dictionary<(double, double), HashSet<int>>();
            for (int si = 0; si < secs.Count; si++)
                foreach (var nd in secs[si].nodes)
                {
                    double[] c = TdfVec(nd, 21);
                    var key = (Zero(Py.Round(c[0], 2)), Zero(Py.Round(c[2], 2)));            // round() of Python floats
                    if (!allc.TryGetValue(key, out var set)) { set = new HashSet<int>(); allc[key] = set; }
                    set.Add(si);
                }
            var models = new List<Chute>();
            for (int si = 0; si < secs.Count; si++)
            {
                var sc = secs[si];
                if (sc.Kind != 4) continue;
                int m = sc.nodes.Count;
                if (m < 2) continue;
                var M = new Chute { si = si, m = m, C = new double[2 * m], E22 = new double[2 * m], E23 = new double[2 * m], cum = new double[m], turn = new double[m],
                    outside = new int[m], frozen = new bool[m], berm22 = new double?[m], berm23 = new double?[m] };
                for (int i = 0; i < m; i++)
                {
                    double[] c = TdfVec(sc.nodes[i], 21), e22 = TdfVec(sc.nodes[i], 22), e23 = TdfVec(sc.nodes[i], 23);
                    M.C[2 * i] = c[0]; M.C[2 * i + 1] = c[2]; M.E22[2 * i] = e22[0]; M.E22[2 * i + 1] = e22[2]; M.E23[2 * i] = e23[0]; M.E23[2 * i + 1] = e23[2];
                }
                double[] C = M.C;
                double run = 0;
                for (int i = 1; i < m; i++) { double h = Hypot(C[2 * i] - C[2 * i - 2], C[2 * i + 1] - C[2 * i - 1]); run = i == 1 ? h : run + h; M.cum[i] = run; }
                int ArgNear(double target)
                {
                    int best = 0; double bd = Math.Abs(M.cum[0] - target);
                    for (int i = 1; i < m; i++) { double d = Math.Abs(M.cum[i] - target); if (d < bd) { bd = d; best = i; } }
                    return best;
                }
                for (int i = 0; i < m; i++)
                {
                    int a = ArgNear(M.cum[i] - TURN_SPAN_FT), b = ArgNear(M.cum[i] + TURN_SPAN_FT);
                    int n1 = Math.Min(i + 1, m - 1), p1 = Math.Max(i - 1, 0);
                    double d1x, d1z, d2x, d2z;
                    if (i > a) { d1x = C[2 * i] - C[2 * a]; d1z = C[2 * i + 1] - C[2 * a + 1]; } else { d1x = C[2 * n1] - C[2 * i]; d1z = C[2 * n1 + 1] - C[2 * i + 1]; }
                    if (b > i) { d2x = C[2 * b] - C[2 * i]; d2z = C[2 * b + 1] - C[2 * i + 1]; } else { d2x = C[2 * i] - C[2 * p1]; d2z = C[2 * i + 1] - C[2 * p1 + 1]; }
                    double ang = Math.Atan2(d1x * d2z - d1z * d2x, Dot2(d1x, d1z, d2x, d2z)) * (180.0 / Math.PI);
                    M.turn[i] = ang;
                    double dvx = C[2 * n1] - C[2 * p1], dvz = C[2 * n1 + 1] - C[2 * p1 + 1];
                    double s23 = dvx * (M.E23[2 * i + 1] - C[2 * i + 1]) - dvz * (M.E23[2 * i] - C[2 * i]);
                    M.outside[i] = (s23 > 0) != (ang > 0) ? 23 : 22;
                    // round() of numpy float64 scalars: np.round
                    M.frozen[i] = allc.TryGetValue((Zero(UxNp.Round(C[2 * i], 2)), Zero(UxNp.Round(C[2 * i + 1], 2))), out var set) && set.Count > 1;
                }
                for (int i = 0; i < m; i++)
                {
                    double? hc = hf.At(C[2 * i], C[2 * i + 1]);
                    double? h22 = hf.At(M.E22[2 * i], M.E22[2 * i + 1]), h23 = hf.At(M.E23[2 * i], M.E23[2 * i + 1]);
                    M.berm22[i] = hc != null && h22 != null ? h22 - hc : null;
                    M.berm23[i] = hc != null && h23 != null ? h23 - hc : null;
                }
                models.Add(M);
            }
            return models;
        }
        /// _normal_out(a, b, e, c): the unit normal of segment a-b pointing away from the corridor.
        static void NormalOut(double ax, double az, double bx, double bz, double ex, double ez, double cx, double cz, out double nx, out double nz)
        {
            double abx = bx - ax, abz = bz - az, l = Math.Max(Hypot(abx, abz), 1e-9);
            nx = abz / l; nz = -abx / l;
            if (Dot2(ex - cx, ez - cz, nx, nz) < 0) { nx = -nx; nz = -nz; }
        }
        /// _signed(p, a, b, n): (the distance of p outside segment a-b along n, the projection parameter t).
        static void Signed(double px, double pz, double ax, double az, double bx, double bz, double nx, double nz, out double s, out double t)
        {
            double abx = bx - ax, abz = bz - az;
            double L2 = Dot2(abx, abz, abx, abz);
            t = L2 == 0 ? 0.0 : Dot2(px - ax, pz - az, abx, abz) / L2;
            s = Dot2(px - ax, pz - az, nx, nz);
        }
        /// classify(models, bales, out_ft): [(bale id, model index, quad i, side k)].
        public static List<(long bid, int mi, int i, int k)> Classify(List<Chute> models, List<Footprint> bales, double outFt = CLASS_OUT_FT)
        {
            var sel = new List<(long, int, int, int)>();
            for (int mi = 0; mi < models.Count; mi++)
            {
                var M = models[mi]; double[] C = M.C;
                foreach (var bale in bales)
                    for (int i = 0; i < M.m - 1; i++)
                        foreach (int k in new[] { 22, 23 })
                        {
                            double[] E = M.E(k);
                            double ax = E[2 * i], az = E[2 * i + 1], bx = E[2 * i + 2], bz = E[2 * i + 3];
                            NormalOut(ax, az, bx, bz, ax, az, C[2 * i], C[2 * i + 1], out double nx, out double nz);
                            bool near = false;
                            for (int q = 0; q < bale.P.Length; q += 2)
                            {
                                Signed(bale.P[q], bale.P[q + 1], ax, az, bx, bz, nx, nz, out double s, out double t);
                                if (-T_TOL <= t && t <= 1 + T_TOL && -CLASS_IN_FT <= s && s < outFt) { near = true; break; }
                            }
                            if (!near) continue;
                            bool hit = false;
                            for (int j = i; j <= i + 1 && !hit; j++)
                                hit = M.outside[j] == k && Math.Abs(M.turn[j]) >= TURN_DEG && M.Berm(j, k) != null && M.Berm(j, k).Value >= BERM_FT;
                            if (hit) sel.Add((bale.id, mi, i, k));
                        }
            }
            return sel;
        }
        /// One move of ux_startpack.reseat.
        public sealed class Reseat { public long id; public double[] unit, from_node; public double dist_ft; }
        /// reseat(secs, bales, hf, dist_ft): the start-pack bales (class with out_ft = CLASS_OUT_FT), each `dist_ft` away from the nearest
        /// start-chute centre node, in the bales' order. Empty for dist_ft <= 0.
        public static List<Reseat> ReseatMoves(List<TdfSection> secs, List<Footprint> bales, HeightField hf, double distFt)
        {
            var o = new List<Reseat>();
            if (!(distFt > 0)) return o;
            var models = ChuteModels(secs, hf);
            var ids = new HashSet<long>();
            foreach (var s in Classify(models, bales, CLASS_OUT_FT)) ids.Add(s.bid);
            var C = new List<double>(); foreach (var M in models) C.AddRange(M.C);
            foreach (var bale in bales)
            {
                if (!ids.Contains(bale.id)) continue;
                int k = 0; double bd = 0;
                for (int i = 0; i < C.Count / 2; i++)
                {
                    double d = Hypot(C[2 * i] - bale.ctr[0], C[2 * i + 1] - bale.ctr[1]);
                    if (i == 0 || d < bd) { bd = d; k = i; }
                }
                double vx = bale.ctr[0] - C[2 * k], vz = bale.ctr[1] - C[2 * k + 1], l = Math.Max(Hypot(vx, vz), 1e-9);
                o.Add(new Reseat { id = bale.id, unit = new[] { vx / l, vz / l }, dist_ft = distFt, from_node = new[] { Py.Round(C[2 * k], 2), Py.Round(C[2 * k + 1], 2) } });
            }
            return o;
        }

        // ================================================================== spec building
        internal sealed class Counter
        {
            public readonly List<string> keys = new List<string>(); public readonly Dictionary<string, long> n = new Dictionary<string, long>(StringComparer.Ordinal);
            public void Add(string k) { if (!n.ContainsKey(k)) { keys.Add(k); n[k] = 0; } n[k]++; }
            public long Get(string k) => n.TryGetValue(k, out long v) ? v : 0;
            public long Total { get { long t = 0; foreach (var kv in n) t += kv.Value; return t; } }
            public int Count => keys.Count;
            public JObj ToJson(bool sorted = false)
            {
                var ks = new List<string>(keys); if (sorted) ks.Sort(StringComparer.Ordinal);
                var o = new JObj(); foreach (string k in ks) o.items.Add(new KeyValuePair<string, object>(k, n[k]));
                return o;
            }
        }
        static JObj Asset(string name, string type) => new JObj().Set("asset", name).Set("type", type);
        static JObj EnumOf(string prop, string name)
        {
            if (!STOCK_ENUMS[prop].TryGetValue(name, out long v)) throw new UxException("no stock " + prop + " value " + Py.Repr(name));
            return new JObj().Set("Enum", name).Set("EnumValue", v);
        }
        static JObj CopyObj(JObj o) { var c = new JObj(); c.items.AddRange(o.items); return c; }
        static List<object> Floats(double[] v) { var l = new List<object>(v.Length); foreach (double x in v) l.Add(x); return l; }

        /// ux_entities.Builder: the specs of one level.
        public sealed class Builder
        {
            public readonly UxLevelRow lv; public readonly string kind, levelPath; public readonly double lift;
            public readonly UxEntitiesOptions o; public readonly JObj opts = new JObj(); public readonly JObj d;
            public readonly List<Ent> entities; public readonly Manifests M; public readonly HeightField hf; public readonly Prototypes protos;
            public readonly List<Spec> specs = new List<Spec>(); public readonly List<string> problems = new List<string>(), warnings = new List<string>();
            public readonly List<string> rollout_notes = new List<string>(); public readonly List<JObj> rollout_removed = new List<JObj>(), rollout_moved = new List<JObj>();
            public SortedDictionary<long, JObj> ridable = new SortedDictionary<long, JObj>();
            public JObj fr_layout, fog; public double bale_reseat;
            internal readonly Counter removed = new Counter(), rollout = new Counter();
            internal readonly Dictionary<string, Counter> dropped_props = new Dictionary<string, Counter>(StringComparer.Ordinal);
            internal readonly List<(string src, string dst, long n)> audio_map = new List<(string, string, long)>();
            readonly UxDiscLevel _disc; readonly UxWork _work; readonly CancellationToken _cancel; readonly CollisCache _collis;
            readonly Dictionary<string, Dictionary<string, string>> _types = new Dictionary<string, Dictionary<string, string>>(StringComparer.Ordinal);
            TerrainQuery _tq; CollisionWorld _cw; List<Reseat> _reseat; HashSet<string> _baleLinks; double[] _tpt, _tptLo, _tptHi; int _tptN; long _synth;
            readonly double[] _triggerMove;

            public Builder(UxDiscLevel disc, UxStock stock, UxWork work, UxEntitiesOptions options, CancellationToken cancel)
            {
                o = options ?? new UxEntitiesOptions(); _disc = disc ?? throw new UxException("the entities stage needs the Untamed disc level"); _work = work; _cancel = cancel;
                lv = disc.lv; kind = o.kind; lift = UxCommon.YLift(lv);
                if (kind != "sx" && kind != "fr") throw new UxException("kind " + Py.Repr(kind) + " is not sx or fr");
                bool Choice(string v, params string[] of) => Array.IndexOf(of, v) >= 0;
                if (!Choice(o.crowd_tier, "stock", "untamed") || !Choice(o.group_number, "zero", "copy") || !Choice(o.hb_freeride, "drop", "freeride", "base"))
                    throw new UxException("crowd_tier " + Py.Repr(o.crowd_tier) + " / group_number " + Py.Repr(o.group_number) + " / hb_freeride " + Py.Repr(o.hb_freeride) + " is not a choice");
                if (!Choice(o.buried, "drop", "keep") || !Choice(o.fr_clearance, "strict", "legacy"))
                    throw new UxException("buried " + Py.Repr(o.buried) + " / fr_clearance " + Py.Repr(o.fr_clearance) + " is not a choice");
                opts.Set("kind", kind).Set("static_bales", o.static_bales).Set("static_pop", o.static_pop).Set("crowd_off", o.crowd_off).Set("static_male", o.static_male)
                    .Set("renames", o.renames != null && o.renames.items.Count > 0);
                if (o.drop_gate_top) opts.Set("drop_gate_top", true);
                if (o.crowd_tier != "untamed") opts.Set("crowd_tier", o.crowd_tier);          // recorded only when not the legacy value
                if (o.group_number != "copy") opts.Set("group_number", o.group_number);
                if (o.hb_freeride != "base") opts.Set("hb_freeride", o.hb_freeride);
                _triggerMove = o.bale_trigger_move != null && o.bale_trigger_move.Length > 0 ? new[] { o.bale_trigger_move[0], o.bale_trigger_move[1] } : null;
                if (o.buried != "keep") opts.Set("buried", o.buried);
                if (o.fr_clearance != "legacy") opts.Set("fr_clearance", o.fr_clearance);
                if (_triggerMove != null) opts.Set("bale_trigger_move", Floats(_triggerMove));
                bale_reseat = o.bale_reseat;
                if (bale_reseat < 0) throw new UxException("bale_reseat must be >= 0 ft, not " + Py.Str(o.bale_reseat));
                if (bale_reseat != 0 && kind != "sx") bale_reseat = 0.0;                      // pop bales race only in sx
                if (bale_reseat != 0) opts.Set("bale_reseat", bale_reseat);
                if (o.manifest_paths != null && o.manifest_paths.Count > 0)
                {
                    var keys = new List<string>(o.manifest_paths.Keys); keys.Sort(StringComparer.Ordinal);
                    var mp = new JObj();
                    foreach (string k in keys) if (!string.IsNullOrEmpty(o.manifest_paths[k])) mp.Set(k, o.manifest_paths[k].Replace('\\', '/'));
                    opts.Set("manifest_paths", mp);
                }
                levelPath = kind == "sx" ? "level.json" : "level_fr.json";
                d = work.ReadJsonObj(levelPath);
                if (d.Get("kind") as string != kind) throw new UxException(levelPath + " is kind " + Py.Repr(d.Get("kind")) + ", expected " + Py.Repr(kind));
                entities = Entities(d);
                M = new Manifests(d, work, o.renames, o.manifest_paths);
                protos = new Prototypes(stock);
                _collis = new CollisCache(disc, cancel);
                hf = new HeightField(lv, work, string.IsNullOrEmpty(o.hf_path) ? "terrain/hf.npy" : o.hf_path.Replace('\\', '/'), () => tq);
                foreach (var t in UxJson.Obj(d.Get("types")).items)
                {
                    var m = new Dictionary<string, string>(StringComparer.Ordinal);
                    if (UxJson.Obj(t.Value).Get("prop_types") is List<object> pt)
                        foreach (object pair in pt) { var p = UxJson.Arr(pair); m[p[0] as string] = p[1] as string; }
                    _types[t.Key] = m;
                }
            }

            public TerrainQuery tq => _tq ?? (_tq = new TerrainQuery(lv, d, _disc, M.idx, _cancel));
            public CollisionWorld cw
            {
                get
                {
                    if (_cw == null)
                    {
                        var ex = new HashSet<long>();
                        if (o.drop_gate_top) foreach (var e in entities) if (GateTop(e.Asset("ModelAsset"))) ex.Add(e.id);
                        _cw = new CollisionWorld(lv, entities, M.idx, _collis, ex);
                    }
                    return _cw;
                }
            }

            /// reseat_map(): the --bale-reseat moves of the start-pack bales (ux_startpack.reseat), empty when off.
            public List<Reseat> reseat_map()
            {
                if (_reseat == null)
                {
                    _reseat = new List<Reseat>();
                    if (bale_reseat > 0)
                        _reseat = ReseatMoves(TdfSections(_disc.ReadTdf(_cancel)), Footprints(entities, cw, new[] { "PopDownBaleRSData" }), hf, bale_reseat);
                }
                return _reseat;
            }

            /// reseat_move(e, target): (dx, dy, dz) of a reseated bale, recorded once, or null.
            double[] reseat_move(Ent e, string target)
            {
                if (target != "PopDownBaleRSData" || bale_reseat == 0) return null;
                Reseat r = null;
                foreach (var x in reseat_map()) if (x.id == e.id) r = x;
                if (r == null) return null;
                double[] m = Lifted(e.matrix, null, lift);
                double x0 = m[12], y0 = m[13], z0 = m[14];
                double nx = x0 + r.dist_ft * r.unit[0], nz = z0 + r.dist_ft * r.unit[1];
                double? g0 = hf.At(x0, z0), g1 = hf.At(nx, nz);
                if (g0 == null || g1 == null)
                {
                    problems.Add(e.type + " " + e.name + " (" + Py.D(e.id) + "): --bale-reseat has no HF at (" + Py.F(x0, 1) + ", " + Py.F(z0, 1) + ") / (" + Py.F(nx, 1) + ", " + Py.F(nz, 1) + ")");
                    return null;
                }
                double ny = y0 + g1.Value - g0.Value;
                double[] nw = { UxNp.F32(nx), UxNp.F32(ny), UxNp.F32(nz) };
                double[] mv = { nw[0] - x0, nw[1] - y0, nw[2] - z0 };
                rollout.Add("bale-reseat: " + e.name + " moved " + Py.G(r.dist_ft) + " ft off the chute centre line");
                rollout_moved.Add(new JObj().Set("untamed_id", e.id).Set("id", e.id + ID_OFFSET).Set("name", e.name).Set("type", target).Set("option", "bale-reseat")
                    .Set("dist_ft", r.dist_ft).Set("unit", new List<object> { Py.Round(r.unit[0], 6), Py.Round(r.unit[1], 6) }).Set("from_node", Floats(r.from_node))
                    .Set("move_xyz", Floats(mv)).Set("old_xyz", new List<object> { x0, y0, z0 }).Set("new_xyz", Floats(nw))
                    .Set("ground_old", Py.Round(g0.Value, 3)).Set("ground_new", Py.Round(g1.Value, 3)));
                return mv;
            }

            long next_synth_id() { _synth++; return SYNTH_ID_BASE + _synth; }

            /// bale_links(): the lower-cased LinkedTriggerIDs of the kept PopUp / PopDown bales.
            HashSet<string> bale_links()
            {
                if (_baleLinks == null)
                {
                    _baleLinks = new HashSet<string>(StringComparer.Ordinal);
                    foreach (var x in entities)
                        if (x.Kept && (x.reflex == "PopUpBaleRSData" || x.reflex == "PopDownBaleRSData") && Py.Truthy(x.props.Get("LinkedTriggerID")))
                            _baleLinks.Add(UxCommon.PyLower(Py.Str(x.props.Get("LinkedTriggerID"))));
                }
                return _baleLinks;
            }

            /// buried_facts(e): {top_gap_ft, bottom_gap_ft, vertices} when every placed vertex of the entity's Untamed Female collision lies
            /// more than BURIED_MIN_FT under the highest trncol surface at its x / z (Untamed frame), else null.
            JObj buried_facts(Ent e)
            {
                string fem = e.Asset("FemaleCollisionAsset");
                JObj rec = Py.Truthy(fem) ? M.idx.Rec(fem, "collis") : null;
                if (rec == null) return null;
                double[] t = _collis.Get(rec.Get("file") as string);
                if (t == null || t.Length == 0) return null;
                double[] w = Transform(t, Mat4(e.matrix));
                int nv = w.Length / 3;
                var order = new int[nv];
                for (int i = 0; i < nv; i++) { order[i] = i; for (int c = 0; c < 3; c++) w[3 * i + c] = UxNp.Round(w[3 * i + c], 4); }
                int Cmp(int a, int b)
                {
                    for (int c = 0; c < 3; c++) { double x = w[3 * a + c], y = w[3 * b + c]; if (x < y) return -1; if (x > y) return 1; }
                    return 0;
                }
                Array.Sort(order, Cmp);                                                    // np.unique(axis=0): the rows in lexicographic order
                double min = 0, max = 0; int count = 0;
                for (int k = 0; k < nv; k++)
                {
                    if (k > 0 && Cmp(order[k], order[k - 1]) == 0) continue;
                    int v = order[k];
                    var top = tq.Top(w[3 * v], w[3 * v + 2], false);
                    if (top == null || !(w[3 * v + 1] < top.Value.y - BURIED_MIN_FT)) return null;
                    double gap = top.Value.y - w[3 * v + 1];
                    if (count == 0 || gap < min) min = gap;
                    if (count == 0 || gap > max) max = gap;
                    count++;
                }
                if (count == 0) throw new UxException("buried check: no collision vertex");
                return new JObj().Set("top_gap_ft", Py.Round(min, 3)).Set("bottom_gap_ft", Py.Round(max, 3)).Set("vertices", (long)count);
            }

            /// terrain_part_tris(): the world (lifted) trncol triangles of the terrain stage's parts (terrain/excluded_tris.npy).
            void terrain_part_tris()
            {
                if (_tpt != null) return;
                _tpt = new double[0]; _tptN = 0;
                const string ex = "terrain/excluded_tris.npy";
                if (_work.Exists(ex))
                {
                    double[] raw = ReadNpy(_work.ReadBytes(ex), out _);
                    var ids = new List<int>();
                    foreach (double v in raw) { long id = (long)v; if (id >= 0 && id < tq.n) ids.Add((int)id); }
                    if (ids.Count > 0)
                    {
                        _tpt = new double[9 * ids.Count]; _tptN = ids.Count;
                        for (int i = 0; i < ids.Count; i++)
                        {
                            int t = 3 * ids[i];
                            for (int c = 0; c < 3; c++) { _tpt[9 * i + c] = tq.A[t + c]; _tpt[9 * i + 3 + c] = tq.B[t + c]; _tpt[9 * i + 6 + c] = tq.C[t + c]; }
                            _tpt[9 * i + 1] += lift; _tpt[9 * i + 4] += lift; _tpt[9 * i + 7] += lift;
                        }
                    }
                }
                Boxes(_tpt, _tptN, out _tptLo, out _tptHi);
            }

            /// point_clearance(p): the smallest distance from point p (lifted) to the kept Untamed collision and the terrain parts.
            (double dist, object who) point_clearance(double px, double py, double pz, double radius = 30.0)
            {
                var near = cw.Nearest(px, py, pz, radius);
                double d0 = near.dist; object who = near.owner;
                terrain_part_tris();
                if (_tptN > 0)
                {
                    NearestIn(_tpt, _tptLo, _tptHi, _tptN, px, py, pz, radius, out double d, out int k);
                    if (k >= 0 && d < d0) { d0 = d; who = "terrain part"; }
                }
                return (d0, who);
            }

            // ---------------------------------------------------------- per-prop helpers
            /// copy_props(e, target, props): the COPY_PROPS that exist in the prototype with the same RSData type.
            void copy_props(Ent e, string target, JObj props)
            {
                var sig = protos.Signature(target);
                _types.TryGetValue(e.type ?? "", out var ut);
                foreach (string k in COPY_PROPS)
                {
                    if (!sig.TryGetValue(k, out string ptype) || !e.props.Has(k) || ut == null || !ut.TryGetValue(k, out string utype) || utype != ptype) continue;
                    object v = e.props.Get(k);
                    if (v is JObj vo && vo.Has("Enum"))
                    {
                        v = new JObj().Set("Enum", vo.Get("Enum")).Set("EnumValue", vo.Get("EnumValue"));
                        if (STOCK_ENUMS.TryGetValue(k, out var tbl))
                        {
                            bool stock = vo.Get("Enum") is string en && tbl.TryGetValue(en, out long sv) && Py.Eq(sv, vo.Get("EnumValue"));
                            if (!stock) warnings.Add(e.type + " " + e.name + ": copied enum " + k + "=" + Py.Repr(vo.Get("Enum")) + "/" + Py.D(Py.Int(vo.Get("EnumValue"))) + " is not a stock Reflex value");
                        }
                    }
                    props.Set(k, v);
                }
                string key = e.type + "->" + target;
                foreach (var kv in e.props.items)
                    if (!sig.ContainsKey(kv.Key))
                    {
                        if (!dropped_props.TryGetValue(key, out var c)) { c = new Counter(); dropped_props[key] = c; }
                        c.Add(kv.Key);
                    }
            }

            /// rollout_props(target, props): the DESIGN 18 defaults on one entity's props: crowd Tier Number 0, GroupNumber 0.
            void rollout_props(string target, JObj props)
            {
                var sig = protos.Signature(target);
                if (o.crowd_tier == "stock" && target == "BillboardCrowdRSData" && sig.ContainsKey("Tier Number"))
                {
                    object old = props.Has("Tier Number") ? props.Get("Tier Number") : GetValue(PropsOf(protos.Entity(target))["Tier Number"]);
                    props.Set("Tier Number", 0L);
                    rollout.Add("crowd-tier stock: Tier Number " + Py.Str(old) + " -> 0");
                }
                if (o.group_number == "zero" && sig.ContainsKey("GroupNumber"))
                {
                    object old = props.Get("GroupNumber");
                    string src = old != null ? "copied" : "prototype";
                    if (old == null) old = GetValue(PropsOf(protos.Entity(target))["GroupNumber"]);
                    if (!Py.Eq(old, 0L)) rollout.Add("group-number zero: " + target + " " + src + " value -> 0");
                    props.Set("GroupNumber", 0L);
                }
            }

            string audio_for(Ent e)
            {
                string m = (e.props.Get("AudioCollisionMaterial") as JObj)?.Get("Enum") as string, s = (e.props.Get("AudioCollisionSubclass") as JObj)?.Get("Enum") as string;
                string r = null;
                if (m != null && s != null) AUDIO_TABLE.TryGetValue(m + "\n" + s, out r);
                if (string.IsNullOrEmpty(r) && m != null) AUDIO_TABLE.TryGetValue(m + "\n\0", out r);
                if (string.IsNullOrEmpty(r)) r = AUDIO_DEFAULT;
                string src = Py.Str(m) + "/" + Py.Str(s);
                int at = audio_map.FindIndex(x => x.src == src && x.dst == r);
                if (at < 0) audio_map.Add((src, r, 1)); else audio_map[at] = (src, r, audio_map[at].n + 1);
                return r;
            }

            void add(Spec s) { specs.Add(s); }

            // ---------------------------------------------------------- one Untamed entity
            void entity(Ent e)
            {
                string cls = e.cls, typ = e.type;
                if (!e.Kept) return;
                string target = e.reflex;
                bool coll = e.collision;
                if (cls == "demote") target = coll ? "CollisionRSData" : "ModelRSData";
                string head = typ + " " + e.name + " (" + Py.D(e.id) + "): ";
                switch (target)
                {
                    case "ModelRSData": case "BillboardCrowdRSData": case "CollisionRSData": case "PhysicsRSData": case "PopUpBaleRSData": case "PopDownBaleRSData": case "TriggerRSData": break;
                    default: problems.Add(head + "no WP7 rule for reflex target " + Py.Repr(target)); return;
                }
                if (target == "BillboardCrowdRSData" && o.crowd_off) { removed.Add("crowd-off: BillboardCrowdRSData"); return; }
                if (o.drop_gate_top && GateTop(e.Asset("ModelAsset"))) { removed.Add("drop-gate-top: Untamed start-gate top"); return; }
                if (o.buried == "drop" && In(PHYSICS_FAMILY, target) && coll)
                {
                    JObj bur = buried_facts(e);
                    if (bur != null)
                    {
                        removed.Add("buried drop: " + target + " " + e.name);
                        rollout.Add("buried drop: " + target + " " + e.name + " (" + Py.D(e.id) + ") left out");
                        rollout_notes.Add(head.Substring(0, head.Length - 2) + " left out (--buried drop): its Untamed collision lies wholly under the Untamed terrain (top " +
                            Py.F(Py.Num(bur.Get("top_gap_ft")), 2) + " ft, bottom " + Py.F(Py.Num(bur.Get("bottom_gap_ft")), 2) +
                            " ft below the trncol surface); in Reflex it would be a sleeping body inside the HF");
                        rollout_removed.Add(new JObj().Set("untamed_id", e.id).Set("id", e.id + ID_OFFSET).Set("name", e.name).Set("type", typ).Set("option", "buried drop")
                            .Set("subscene", "Base").Set("buried", bur));
                        return;
                    }
                }
                string sub = "Base";
                if (kind == "sx" && (target == "PopUpBaleRSData" || target == "PopDownBaleRSData") && FreerideOnly(e.name) && o.hb_freeride != "base")
                {
                    if (o.hb_freeride == "drop")
                    {
                        removed.Add("hb-freeride drop: " + target + " " + e.name);
                        rollout_notes.Add(head.Substring(0, head.Length - 2) + " left out of the sx build (--hb-freeride drop): a " + target + " is hidden in Free Ride and stands in the race until " +
                            Py.Repr(e.props.Get("LinkedTriggerID")) + " fires (core build_activity_records.c:33-69), the opposite of its name; no stock SX FreeRide sub-scene holds a physics prop");
                        rollout_removed.Add(new JObj().Set("untamed_id", e.id).Set("id", e.id + ID_OFFSET).Set("name", e.name).Set("type", typ).Set("option", "hb-freeride drop").Set("subscene", "Base"));
                        return;
                    }
                    target = "PhysicsRSData"; sub = "FreeRide";                              // --hb-freeride freeride (A/B)
                    rollout.Add("hb-freeride freeride: " + e.name + " -> PhysicsRSData in FreeRide");
                    rollout_removed.Add(new JObj().Set("untamed_id", e.id).Set("id", e.id + ID_OFFSET).Set("name", e.name).Set("type", typ).Set("option", "hb-freeride freeride").Set("subscene", "FreeRide"));
                }
                bool static_bale = false;
                if (In(PHYSICS_FAMILY, target) && o.static_bales)
                {
                    if (target == "PopUpBaleRSData" && o.static_pop == "initial") { removed.Add("static-pop initial: PopUpBaleRSData"); return; }
                    if ((target == "PopUpBaleRSData" || target == "PopDownBaleRSData") && o.static_pop == "drop") { removed.Add("static-pop drop: " + target); return; }
                    static_bale = true;
                    target = "CollisionRSData";
                }
                var src = new JObj().Set("id", e.id).Set("type", typ).Set("name", e.name).Set("cls", cls).Set("rule", e.j.Get("rule")).Set("subscene", e.j.Get("subscene"));
                if (target == "TriggerRSData")
                {
                    var props = new JObj();
                    copy_props(e, target, props);
                    if (props.Get("TriggerID") is string tid && TriggerIdMap(tid) != null) props.Set("TriggerID", TriggerIdMap(tid));
                    props.Set("RenderIntoStaticShadow", 0L).Set("RenderIntoVolumeShadow", 0L).Set("LockScale", 0L);
                    double[] mtx = Lifted(e.matrix, null, lift);
                    if (_triggerMove != null && kind == "sx" && bale_links().Contains(UxCommon.PyLower(props.Has("TriggerID") ? Py.Str(props.Get("TriggerID")) : "")))
                    {
                        double[] old = { mtx[12], mtx[13], mtx[14] };
                        mtx[12] = UxNp.F32(mtx[12] + _triggerMove[0]); mtx[14] = UxNp.F32(mtx[14] + _triggerMove[1]);
                        rollout.Add("bale-trigger-move: " + e.name + " moved (" + Py.G(_triggerMove[0], true) + ", " + Py.G(_triggerMove[1], true) + ") ft");
                        rollout_moved.Add(new JObj().Set("untamed_id", e.id).Set("id", e.id + ID_OFFSET).Set("name", e.name).Set("trigger_id", props.Get("TriggerID"))
                            .Set("option", "bale-trigger-move").Set("move_ft", Floats(_triggerMove))
                            .Set("old_xyz", new List<object> { Py.Round(old[0], 3), Py.Round(old[1], 3), Py.Round(old[2], 3) })
                            .Set("new_xyz", new List<object> { Py.Round(mtx[12], 3), Py.Round(mtx[13], 3), Py.Round(mtx[14], 3) }));
                    }
                    add(new Spec { id = e.id + ID_OFFSET, name = e.name, type = target, subscene = "Base", matrix = mtx, props = props, source = src, cls = cls, role = "trigger", collision = false });
                    return;
                }
                // models: parts from the meshes stage
                string model = e.Asset("ModelAsset");
                var mlist = new List<string>();
                if (e.props.Get("MaterialList") is List<object> ml) foreach (object m in ml) mlist.Add(UxJson.Obj(m).Get("asset") as string);
                object sk = Py.Truthy(model) ? M.Skipped(model) : null;
                if (Py.Truthy(sk)) { problems.Add(head + "model " + model + " skipped by WP3 (" + Py.Str(sk) + ")"); return; }
                JObj mrec = Py.Truthy(model) ? M.idx.Rec(model, "model") : null;
                long ng = mrec != null && mrec.Has("ngroups") ? Py.Int(mrec.Get("ngroups")) : mlist.Count;
                if (mrec != null && ng != mlist.Count) problems.Add(head + "MaterialList " + Py.D(mlist.Count) + " != model groups " + Py.D(ng));
                var parts = M.Parts(model, (int)ng);
                // collision
                string fem = null, mal = null;
                if (coll && In(COLLISION_TYPES, target))
                {
                    string how;
                    (fem, mal, how) = M.Collis(e);
                    if (fem == null)
                    {
                        warnings.Add(head + "collision lost (" + how + "): emitted as ModelRSData");
                        target = "ModelRSData"; coll = false;
                    }
                }
                else if (coll) coll = false;
                // which part carries the collision
                int cpart = -1;
                if (coll)
                {
                    int flagged = parts.FindIndex(p => Py.Truthy(p.collision)), zero = parts.FindIndex(p => !Any(p.offset));
                    cpart = flagged >= 0 ? flagged : zero;
                    if (cpart < 0)
                    {
                        problems.Add(head + "no part of " + model + " has a zero origin offset to carry the collision");
                        cpart = 0;
                    }
                    if (parts.Count > 1 && In(PHYSICS_FAMILY, target))
                        problems.Add(head + "physics prop model " + model + " has " + Py.D(parts.Count) + " parts (they would not move together)");
                }
                double[] rsm = kind == "sx" ? reseat_move(e, target) : null;               // --bale-reseat (null when off)
                string role = target == "BillboardCrowdRSData" ? "crowd" : target == "ModelRSData" ? "render" : In(PHYSICS_FAMILY, target) ? "physics" : static_bale ? "static_bale" :
                    ridable.ContainsKey(e.id) ? "ridable" : "wall";
                for (int k = 0; k < parts.Count; k++)
                {
                    var p = parts[k];
                    bool carries = coll && k == cpart;
                    string ptype = carries || In(RENDER_TYPES, target) ? target : "ModelRSData";
                    if (!coll && In(COLLISION_TYPES, target)) ptype = "ModelRSData";
                    var mats = new List<object>();
                    foreach (long g in p.groups)
                    {
                        if (g >= mlist.Count)
                        {
                            problems.Add(head + "part " + p.part + " group -> source group " + Py.D(g) + " beyond MaterialList " + Py.D(mlist.Count));
                            continue;
                        }
                        string mg = mlist[(int)g];
                        mats.Add(Asset(M.Name(mg, "material"), "material"));
                        if (M.dropped_materials.Contains(M.idx.Canon(mg, "material") ?? mg ?? ""))
                            warnings.Add(head + "MaterialList entry " + mg + " is dropped by WP4 (group still in " + p.part + ")");
                    }
                    var props = new JObj();
                    copy_props(e, ptype, props);
                    if (sub == "FreeRide") props.items.RemoveAll(kv => kv.Key == "LinkedTriggerID");      // a static prop in FreeRide: no pop trigger link
                    props.Set("RenderIntoStaticShadow", 0L);
                    props.Set("RenderIntoVolumeShadow", 0L);
                    props.Set("LockScale", carries && !IsScaled(e.matrix) ? 1L : 0L);
                    props.Set("ModelAsset", Asset(M.Renamed("model", p.part), "model"));
                    props.Set("SurfaceAsset", Asset(M.Renamed("surface", p.surface), "surface"));
                    props.Set("MaterialList", mats);
                    if (carries)
                    {
                        props.Set("FemaleCollisionAsset", Asset(fem, "collis"));
                        if (ptype == "CollisionRSData" && o.static_male == "editor_default" && role == "wall") props.Set("MaleCollisionAsset", Asset("Editor_Default", "collis"));
                        else props.Set("MaleCollisionAsset", Asset(mal, "collis"));
                        props.Set("DriveOnMaterial", EnumOf("DriveOnMaterial", "ID 0"));
                        props.Set("UseSoftCollision", 0L);
                        if (In(PHYSICS_FAMILY, ptype))
                        {
                            props.Set("PhysicsType", EnumOf("PhysicsType", "HaySolid")); props.Set("CollisionAudioMaterial", EnumOf("CollisionAudioMaterial", "TuffBlock"));
                            props.Set("DriveOn", 0L); props.Set("AIAvoid", 1L);
                        }
                        else if (static_bale)
                        {
                            props.Set("PhysicsType", EnumOf("PhysicsType", "Default")); props.Set("CollisionAudioMaterial", EnumOf("CollisionAudioMaterial", "TuffBlock"));
                            props.Set("DriveOn", 0L); props.Set("AIAvoid", 1L);
                        }
                        else if (role == "ridable")
                        {
                            props.Set("PhysicsType", EnumOf("PhysicsType", "MXBike")); props.Set("CollisionAudioMaterial", EnumOf("CollisionAudioMaterial", audio_for(e)));
                            props.Set("DriveOn", 1L);
                        }
                        else
                        {
                            props.Set("PhysicsType", EnumOf("PhysicsType", "Default")); props.Set("CollisionAudioMaterial", EnumOf("CollisionAudioMaterial", audio_for(e)));
                            props.Set("DriveOn", 0L);
                        }
                    }
                    long eid; string nm;
                    if (k == 0) { eid = e.id + ID_OFFSET; nm = e.name; } else { eid = next_synth_id(); nm = e.name + "_p" + k.ToString(CultureInfo.InvariantCulture); }
                    double[] mtx = Lifted(e.matrix, p.offset, lift);
                    if (rsm != null) for (int q = 0; q < 3; q++) mtx[12 + q] = UxNp.F32(mtx[12 + q] + rsm[q]);
                    var source = CopyObj(src).Set("part", (long)k).Set("parts", (long)parts.Count).Set("origin_offset", Floats(p.offset)).Set("model", model).Set("part_model", p.part);
                    add(new Spec { id = eid, name = nm, type = ptype, subscene = sub, matrix = mtx, props = props, source = source, cls = cls,
                        role = carries || In(RENDER_TYPES, ptype) ? role : "render_part", collision = carries });
                }
            }

            // ---------------------------------------------------------- terrain parts (decks, EC rocks / logs)
            void terrain_part(JObj p, int k)
            {
                object First(params string[] keys) { foreach (string key in keys) if (Py.Truthy(p.Get(key))) return p.Get(key); return null; }
                string nm = First("name", "model") as string ?? "TerrainPart" + k.ToString("000", CultureInfo.InvariantCulture);
                string model = First("model") as string ?? nm;
                double[] m16;
                if (Py.Truthy(p.Get("matrix"))) m16 = Nums(p.Get("matrix"));
                else
                {
                    object oo = First("origin", "origin_offset");
                    double[] og = oo != null ? Nums(oo) : new double[] { 0, 0, 0 };
                    m16 = new double[] { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, og[0], og[1], og[2], 1 };
                }
                double[] Mx = Py.Truthy(p.Get("lifted")) ? F32List(m16) : Lifted(m16, null, lift);
                var props = new JObj().Set("EnableTick", 1L).Set("ExportName", 0L).Set("RenderIntoStaticShadow", 0L).Set("RenderIntoVolumeShadow", 0L);
                var mats = new List<object>();
                if (First("material_list", "materials") is List<object> ml) foreach (object m in ml) mats.Add(Asset(M.Name(m as string, "material"), "material"));
                props.Set("ModelAsset", Asset(model, "model"));
                props.Set("SurfaceAsset", Asset(First("surface") as string ?? model, "surface"));
                props.Set("MaterialList", mats);
                props.Set("AlwaysVisible", 0L).Set("CastsShadow", 0L).Set("GroupNumber", 0L);
                string col = First("collis") as string, typ;
                if (col != null)
                {
                    props.Set("LockScale", 1L);
                    props.Set("FemaleCollisionAsset", Asset(col, "collis"));
                    props.Set("MaleCollisionAsset", Asset(col, "collis"));
                    // RIDABLE_VALUES (the FR_Gibson ridable rocks)
                    props.Set("PhysicsType", EnumOf("PhysicsType", "MXBike")).Set("CollisionAudioMaterial", EnumOf("CollisionAudioMaterial", "ConcreteWall"))
                        .Set("DriveOnMaterial", EnumOf("DriveOnMaterial", "ID 0")).Set("DriveOn", 1L).Set("UseSoftCollision", 0L).Set("AIAvoid", 1L);
                    typ = "CollisionRSData";
                }
                else { props.Set("LockScale", 0L); typ = "ModelRSData"; }
                add(new Spec { id = next_synth_id(), name = nm, type = typ, subscene = "Base", matrix = Mx, props = props,
                    source = new JObj().Set("id", null).Set("type", "TerrainRSData").Set("name", nm).Set("cls", "terrain_part").Set("role", p.Get("role")).Set("model", model),
                    cls = "terrain_part", role = col != null ? "ridable" : "render", collision = col != null });
            }

            // ---------------------------------------------------------- 12 free-ride starts
            sealed class Row { public int index; public double offset, x, z, ground, y, clearance, pointClearance; public long? nearest; public object pointNearest; public bool strict, groundNp; }

            List<Spec> freeride_starts(double clearance = 2.0)
            {
                var res = new List<Spec>();
                if (!(d.Get("freeride_start") is JObj fs) || fs.items.Count == 0) { problems.Add("level.json has no freeride_start"); return res; }
                double[] Mu = Unscaled(Nums(fs.Get("matrix")));
                double[] x = { Mu[0], 0.0, Mu[2] };
                if (UxNp.Norm(x) < 1e-6) { problems.Add("free-ride line x axis is vertical"); return res; }
                double nrm = UxNp.Norm(x); for (int i = 0; i < 3; i++) x[i] /= nrm;
                double[] pos = Nums(fs.Get("position")); double[] p0 = { UxNp.F32(pos[0]), UxNp.F32(pos[1]), UxNp.F32(pos[2]) };
                bool strict = o.fr_clearance == "strict";
                var layouts = new List<KeyValuePair<string, double[]>>();
                var a = new double[12]; var b = new double[12]; var c = new double[12];
                for (int k = 1; k <= 12; k++) { a[k - 1] = -(k - 1) * 3.0; b[k - 1] = (k - 1) * 3.0; c[k - 1] = -(k - 6.5) * 3.0; }
                for (int k = 0; k < 12; k++) if (a[k] == 0) a[k] = 0.0;                    // -(0) * 3.0 is +0.0 in Python (the int is negated)
                layouts.Add(new KeyValuePair<string, double[]>("-x", a)); layouts.Add(new KeyValuePair<string, double[]>("+x", b)); layouts.Add(new KeyValuePair<string, double[]>("centred", c));

                List<Row> Place(double[] offs)
                {
                    var rows = new List<Row>();
                    for (int k = 1; k <= offs.Length; k++)
                    {
                        double of = offs[k - 1];
                        double px = p0[0] + x[0] * of, pz = p0[2] + x[2] * of;
                        double? g0 = hf.At(px, pz); bool np = false; double g;
                        if (g0 == null)
                        {
                            g = p0[1] + lift; np = true;
                            warnings.Add("free-ride start " + Py.D(k) + ": no ground at (" + Py.F(px, 1) + ", " + Py.F(pz, 1) + "); used the line height");
                        }
                        else g = g0.Value;
                        var cl = cw.Clearance(px, g, pz);
                        var row = new Row { index = k, offset = of, x = px, z = pz, ground = g, y = g + 1.0, clearance = cl.dist, nearest = cl.owner, strict = strict, groundNp = np };
                        if (strict) { var pc = point_clearance(px, g + 1.0, pz); row.pointClearance = pc.dist; row.pointNearest = pc.who; }
                        rows.Add(row);
                    }
                    return rows;
                }
                bool Blocked(Row r) => r.clearance < clearance || strict && r.pointClearance < FR_POINT_CLEAR_FT;

                var tried = new List<object>(); var placed = new Dictionary<string, List<Row>>(StringComparer.Ordinal);
                string name = null;
                foreach (var lay in layouts)
                {
                    _cancel.ThrowIfCancellationRequested();
                    placed[lay.Key] = Place(lay.Value);
                    var blocked = new List<object>(); bool bad = false;
                    foreach (var r in placed[lay.Key])
                    {
                        if (!Blocked(r)) continue;
                        bad = true;
                        blocked.Add(r.strict ? new object[] { (long)r.index, Py.Round(r.clearance, 2), r.nearest, Py.Round(r.pointClearance, 2), r.pointNearest }
                            : new object[] { (long)r.index, Py.Round(r.clearance, 2), r.nearest });
                    }
                    tried.Add(new JObj().Set("layout", lay.Key).Set("blocked", blocked));
                    if (!bad) { name = lay.Key; break; }
                }
                if (name == null)
                {   // every layout blocked: keep the design layout and say so
                    name = "-x";
                    warnings.Add("free-ride starts: every layout has a start within " + Py.F(clearance, 1) + " ft of collision: " + Py.Str(tried));
                }
                foreach (var r in placed[name])
                {
                    var m = (double[])Mu.Clone(); m[12] = r.x; m[13] = r.y; m[14] = r.z;
                    var props = new JObj().Set("EnableTick", 1L).Set("ExportName", 0L).Set("RenderIntoStaticShadow", 0L).Set("RenderIntoVolumeShadow", 0L).Set("LockScale", 0L).Set("Index", (long)r.index);
                    bool finite = !double.IsNaN(r.clearance) && !double.IsInfinity(r.clearance);
                    res.Add(new Spec { id = FR_ID_BASE + r.index, name = "StartEntityFreeRide" + r.index.ToString("00", CultureInfo.InvariantCulture), type = "StartEntityFreeRideRSData", subscene = "FreeRide",
                        matrix = F32List(m), props = props,
                        source = new JObj().Set("id", fs.Get("id")).Set("type", fs.Get("type")).Set("name", fs.Get("name")).Set("cls", "synth").Set("rule", "freeride_start")
                            .Set("subscene", fs.Get("subscene")).Set("in_race_merge", fs.Get("in_race_merge")),
                        cls = "synth", role = "start", collision = false,
                        start = new JObj().Set("layout", name).Set("offset_ft", r.offset).Set("ground_ft", r.groundNp ? UxNp.Round(r.ground, 3) : Py.Round(r.ground, 3))
                            .Set("clearance_ft", finite ? (object)Py.Round(r.clearance, 2) : null).Set("nearest_collision_entity", r.nearest) });
                }
                fr_layout = new JObj().Set("chosen", name).Set("tried", tried).Set("hf_source", hf.source);
                return res;
            }

            // ---------------------------------------------------------- distance fog values for the template entity
            JObj distance_fog()
            {
                Ent e = null;
                foreach (var x in entities) if (x.type == "DistanceFogRSData" && x.cls == "replace") { e = x; break; }
                if (e == null) return null;
                _types.TryGetValue("DistanceFogRSData", out var ut);
                var vals = new JObj();
                foreach (var kv in PropSignature(protos.Entity("DistanceFogRSData")))
                {
                    switch (kv.Key) { case "Name": case "Id": case "Matrix": case "EnableTick": case "ExportName": case "RenderIntoStaticShadow": case "RenderIntoVolumeShadow": case "LockScale": continue; }
                    if (e.props.Has(kv.Key) && ut != null && ut.TryGetValue(kv.Key, out string t) && t == kv.Value) vals.Set(kv.Key, e.props.Get(kv.Key));
                }
                var others = new List<object>();
                foreach (var x in entities) if (x.type == "DistanceFogRSData" && x.cls == "drop") others.Add(x.id);
                return new JObj().Set("template_entity", "DistanceFog").Set("template_type", "DistanceFogRSData").Set("source_id", e.id).Set("source_name", e.name).Set("values", vals)
                    .Set("others_dropped", others);
            }

            public List<Spec> build()
            {
                bool any = false;
                foreach (var e in entities) if (e.collision && e.Kept && e.reflex == "CollisionRSData") { any = true; break; }
                if (any) ridable = RidableCollision(entities, M.idx, _collis, () => tq, _cancel);
                foreach (var e in entities) { _cancel.ThrowIfCancellationRequested(); entity(e); }
                for (int k = 0; k < M.terrain_parts.Count; k++) terrain_part(M.terrain_parts[k], k);
                specs.AddRange(freeride_starts());
                foreach (var sp in specs) rollout_props(sp.type, sp.props);                 // the DESIGN 18 defaults on every emitted spec
                fog = distance_fog();
                return specs;
            }

            /// The files of a work-store folder: name -> store path, case ignored (Path.exists on the reference's file system).
            internal Dictionary<string, string> Files(string dir)
            {
                var set = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
                if (_work.DirExists(dir)) foreach (string f in _work.List(dir)) set[f.Substring(f.LastIndexOf('/') + 1)] = f;
                return set;
            }
            internal byte[] WorkBytes(string rel) => _work.ReadBytes(rel);
        }

        // ================================================================== checks
        static JObj CheckRow(string name, bool ok, object detail) => new JObj().Set("check", name).Set("ok", ok).Set("detail", detail);
        static List<object> Head(List<object> l, int n) => l.Count <= n ? l : l.GetRange(0, n);

        /// check_specs(b, specs, elems): the acceptance checks of the stage on the built specs / elems (entities.json 'checks').
        /// encoded: the chunk documents per sub-scene (the first check encodes them; build_entities writes the same bytes).
        static List<JObj> CheckSpecs(Builder b, List<Spec> specs, List<UxElem> elems, Dictionary<string, List<byte[]>> encoded)
        {
            var res = new List<JObj>();
            var types = new Counter(); foreach (var s in specs) types.Add(s.type);
            bool ok = true;
            foreach (string t in types.keys) if (Array.IndexOf(UxCommon.RSDATA_CORE_TYPES, t) < 0) ok = false;
            res.Add(CheckRow("types allowed by rxcore", ok, types.ToJson()));
            // encode -> decode identical, per chunk
            ok = true; int nchunk = 0; var badChunks = new List<object>();
            foreach (string sub in new[] { "Base", "FreeRide" })
            {
                var els = new List<UxElem>();
                for (int i = 0; i < specs.Count; i++) if (specs[i].subscene == sub) els.Add(elems[i]);
                var blobs = EncodeChunks(els, b.protos);
                encoded[sub] = blobs;
                for (int i = 0; i < blobs.Count; i++)
                {
                    nchunk++;
                    var t = new UxBxml(blobs[i]).ToTree();
                    int n = Math.Min(els.Count, (i + 1) * CHUNK_MAX) - i * CHUNK_MAX;
                    bool same = t.tag == "SceneChunk" && t.children.Count == n;
                    for (int k = 0; same && k < n; k++) same = ElemEqual(t.children[k], els[i * CHUNK_MAX + k]);
                    if (!same) { ok = false; badChunks.Add(new object[] { sub, (long)i }); }
                    if (t.children.Count > CHUNK_MAX) ok = false;
                }
            }
            res.Add(CheckRow("every Elem encodes to a SceneChunk and decodes identically", ok, Py.D(nchunk) + " chunks" + (badChunks.Count > 0 ? " bad " + Py.Str(badChunks) : "")));
            // prop name / type sets equal the prototype's, and the values read back as the spec says
            ok = true; var bad = new List<object>();
            for (int i = 0; i < specs.Count; i++)
            {
                var s = specs[i]; var el = elems[i];
                if (!SameSignature(PropSignature(el), PropSignature(b.protos.Entity(s.type)))) { ok = false; bad.Add(s.name); continue; }
                var P = PropsOf(el);
                var items = new List<KeyValuePair<string, object>>(s.props.items);
                items.Add(new KeyValuePair<string, object>("Name", s.name)); items.Add(new KeyValuePair<string, object>("Id", s.id)); items.Add(new KeyValuePair<string, object>("Matrix", Floats(s.matrix)));
                foreach (var kv in items)
                {
                    object g = GetValue(P[kv.Key]), v = kv.Value;
                    bool same;
                    if (v is double || v is List<object> vl && vl.Count > 0 && vl[0] is double) same = ExactFloats(g, v);
                    else same = Py.Eq(g, v);
                    if (!same) { ok = false; bad.Add(s.name + "." + kv.Key); }
                }
            }
            res.Add(CheckRow("prop name/type sets equal the prototype (and values read back)", ok, Head(bad, 5)));
            // positions = Untamed + lift (primary entities; parts: translate(offset) x M + lift)
            var ents = new Dictionary<long, Ent>(); foreach (var e in b.entities) ents[e.id] = e;
            ok = true; int count = 0; double worst = 0.0;
            var moved = new Dictionary<long, double[]>(); var reseated = new Dictionary<long, double[]>();
            foreach (var m in b.rollout_moved)
            {
                string opt = m.Has("option") ? m.Get("option") as string : "bale-trigger-move";
                if (opt == "bale-trigger-move") moved[UxJson.Int(m.Get("untamed_id"))] = Nums(m.Get("move_ft"));
                if (opt == "bale-reseat") reseated[UxJson.Int(m.Get("untamed_id"))] = Nums(m.Get("move_xyz"));
            }
            foreach (var s in specs)
            {
                if (s.cls != "keep" && s.cls != "demote" || !(s.source.Get("id") is long sid) || !ents.TryGetValue(sid, out Ent e)) continue;
                object oo = s.source.Get("origin_offset");
                double[] off = Py.Truthy(oo) ? Nums(oo) : new double[] { 0, 0, 0 };
                double[] Mx = Mat4(e.matrix);
                double[] mv = s.type == "TriggerRSData" && moved.TryGetValue(sid, out var mvv) ? mvv : new double[] { 0.0, 0.0 };
                double[] rv = s.type == "PopDownBaleRSData" && reseated.TryGetValue(sid, out var rvv) ? rvv : null;
                double[] want = UxNp.VecMat(off, new[] { Mx[0], Mx[1], Mx[2], Mx[4], Mx[5], Mx[6], Mx[8], Mx[9], Mx[10] }, 3);
                for (int q = 0; q < 3; q++) want[q] += Mx[12 + q];
                want[1] += b.lift; want[0] += mv[0]; want[2] += mv[1];
                if (rv != null) for (int q = 0; q < 3; q++) want[q] = want[q] + rv[q];
                double[] got = { s.matrix[12], s.matrix[13], s.matrix[14] };
                if (!Any(off))
                {
                    double[] exact = { UxNp.F32(Mx[12]), UxNp.F32(Mx[13] + b.lift), UxNp.F32(Mx[14]) };
                    if (Any(mv)) exact = new[] { UxNp.F32(exact[0] + mv[0]), exact[1], UxNp.F32(exact[2] + mv[1]) };
                    if (rv != null) exact = new[] { UxNp.F32(exact[0] + rv[0]), UxNp.F32(exact[1] + rv[1]), UxNp.F32(exact[2] + rv[2]) };
                    if (got[0] != exact[0] || got[1] != exact[1] || got[2] != exact[2]) ok = false;
                }
                double mx = Math.Abs(got[0] - want[0]);
                for (int q = 1; q < 3; q++) { double dq = Math.Abs(got[q] - want[q]); if (dq > mx || double.IsNaN(dq)) mx = dq; }
                if (mx > worst) worst = mx;
                count++;
                bool close = true;
                for (int q = 0; q < 12 && close; q++)
                {
                    double x = s.matrix[q], y = UxNp.F32(Mx[q]);
                    close = x == y || Math.Abs(x - y) <= 1e-5 * Math.Abs(y);
                }
                if (!close && !Any(off)) ok = false;
            }
            res.Add(CheckRow("positions equal Untamed + the level lift (" + Py.G(b.lift) + " ft; rotation unchanged)" +
                (moved.Count > 0 ? " + the --bale-trigger-move offset on " + Py.D(moved.Count) + " trigger(s)" : "") + (reseated.Count > 0 ? " + the --bale-reseat move on " + Py.D(reseated.Count) + " bale(s)" : ""),
                ok && worst < 1e-3, Py.D(count) + " entities, worst " + Py.E(worst, 2) + " ft"));
            if (b.bale_reseat != 0)
            {
                var rm = new List<long>(); foreach (var r in b.reseat_map()) if (!rm.Contains(r.id)) rm.Add(r.id);
                rm.Sort();
                var done = new List<long>(reseated.Keys); done.Sort();
                bool distOk = true;
                foreach (var v in reseated.Values) if (!(Math.Abs(Math.Sqrt(v[0] * v[0] + v[2] * v[2]) - b.bale_reseat) < 1e-3)) distOk = false;
                bool sameIds = done.Count == rm.Count; for (int i = 0; sameIds && i < done.Count; i++) sameIds = done[i] == rm[i];
                var doneObj = new List<object>(); foreach (long v in done) doneObj.Add(v);
                res.Add(CheckRow("--bale-reseat " + Py.G(b.bale_reseat) + ": every start-pack bale moved once by " + Py.G(b.bale_reseat) + " ft horizontally (" + Py.D(done.Count) + ": " + Py.Str(doneObj) + ")",
                    sameIds && done.Count > 0 && distOk, ""));
            }
            var tp = new List<Spec>(); foreach (var s in specs) if (s.cls == "terrain_part") tp.Add(s);
            var tparts = new Dictionary<string, JObj>(StringComparer.Ordinal);
            foreach (var p in b.M.terrain_parts) { object k = Py.Truthy(p.Get("name")) ? p.Get("name") : p.Get("model"); if (k is string ks) tparts[ks] = p; }
            bool okt = true;
            foreach (var s in tp)
            {
                tparts.TryGetValue(s.name, out JObj p);
                bool wrong = p == null || Py.Truthy(p.Get("lifted"));
                if (!wrong)
                {
                    double[] og;
                    if (Py.Truthy(p.Get("matrix"))) { var m = Nums(p.Get("matrix")); og = new[] { m[12], m[13], m[14] }; }
                    else og = Py.Truthy(p.Get("origin")) ? Nums(p.Get("origin")) : new double[] { 0, 0, 0 };
                    wrong = s.matrix[12] != UxNp.F32(og[0]) || s.matrix[13] != UxNp.F32(og[1] + b.lift) || s.matrix[14] != UxNp.F32(og[2]);
                }
                if (wrong) okt = okt && p != null && Py.Truthy(p.Get("lifted"));
            }
            bool allRidable = true;
            foreach (var s in tp) if (s.collision && !(s.type == "CollisionRSData" && Py.Eq(s.props.Get("DriveOn"), 1L))) allRidable = false;
            res.Add(CheckRow("WP6 terrain parts at their Untamed origin + the level lift (" + Py.G(b.lift) + " ft), ridable CollisionRSData", okt && allRidable, Py.D(tp.Count) + " parts"));
            // MaterialList length == group count of the written model (meshes/, terrain/parts/), when the file exists
            var cache = new Dictionary<string, object>(StringComparer.Ordinal); var badModels = new List<object>(); int nchk = 0;
            Dictionary<string, string> meshFiles = b.Files("meshes"), partFiles = b.Files("terrain/parts");
            foreach (var s in specs)
            {
                string m = (s.props.Get("ModelAsset") as JObj)?.Get("asset") as string;
                if (!Py.Truthy(m)) continue;
                if (!cache.TryGetValue(m, out object g))
                {
                    if (!meshFiles.TryGetValue(m + ".model", out string f) && !partFiles.TryGetValue(m + ".model", out f)) f = null;
                    try { g = f != null ? (object)(long)UxRmesh.ReadModel(b.WorkBytes(f)).materials.Count : null; }
                    catch (Exception ex) when (!(ex is OperationCanceledException)) { g = "unreadable: " + ex.Message; }
                    cache[m] = g;
                }
                if (g == null) continue;
                nchk++;
                long have = s.props.Get("MaterialList") is List<object> ml ? ml.Count : 0;
                if (!(g is long gl) || gl != have) badModels.Add(new object[] { s.name, m, g, have });
            }
            int nfiles = 0; foreach (var v in cache.Values) if (v != null) nfiles++;
            res.Add(CheckRow("MaterialList length == groups of the written WP3/WP6 model", badModels.Count == 0,
                Py.D(nchk) + " entities checked against " + Py.D(nfiles) + " model files" + (badModels.Count > 0 ? " bad " + Py.Str(Head(badModels, 3)) : "")));
            // collis files referenced exist in the collision stage's / the terrain parts' outputs (when those manifests exist)
            var missing = new List<string>(); int nc = 0;
            string cpath = b.M.paths["collision"]; string cdir = cpath.LastIndexOf('/') >= 0 ? cpath.Substring(0, cpath.LastIndexOf('/')) : "";
            string cname = cdir.Substring(cdir.LastIndexOf('/') + 1);
            Dictionary<string, string> collisFiles = cdir.Length > 0 ? b.Files(cdir) : new Dictionary<string, string>();
            foreach (var s in specs)
                foreach (string k in new[] { "FemaleCollisionAsset", "MaleCollisionAsset" })
                {
                    string c = (s.props.Get(k) as JObj)?.Get("asset") as string;
                    if (!Py.Truthy(c) || c == "Editor_Default") continue;
                    if (b.M.used["collision"] != "present" && s.cls != "terrain_part") continue;
                    nc++;
                    if (!(collisFiles.ContainsKey(c + ".collis") || partFiles.ContainsKey(c + ".collis"))) missing.Add(c);
                }
            var missSet = new List<string>(new HashSet<string>(missing)); missSet.Sort(StringComparer.Ordinal);
            var missObj = new List<object>(); foreach (string c in missSet) missObj.Add(c);
            res.Add(CheckRow("every referenced collis exists in the WP5 collision folder (" + cname + ") or terrain/parts", missing.Count == 0,
                Py.D(nc) + " references" + (missing.Count > 0 ? " missing " + Py.Str(Head(missObj, 5)) : "")));
            // counts per class equal level.json
            var lvc = new Counter(); foreach (var e in b.entities) lvc.Add(e.cls ?? "None");
            var prim = new Counter();
            foreach (var s in specs) if ((s.cls == "keep" || s.cls == "demote") && Py.Eq(s.source.Has("part") ? s.source.Get("part") : 0L, 0L)) prim.Add(s.cls);
            long removed = b.removed.Total; int lost = 0;
            foreach (string p in b.problems) if (p.Contains("skipped by WP3") || p.Contains("no WP7 rule")) lost++;
            int starts = 0; foreach (var s in specs) if (s.type == "StartEntityFreeRideRSData") starts++;
            bool okc = prim.Get("keep") + prim.Get("demote") + lost + removed == lvc.Get("keep") + lvc.Get("demote") && prim.Get("demote") == lvc.Get("demote");
            res.Add(CheckRow("primary counts per class equal level.json", okc, "level.json keep " + Py.D(lvc.Get("keep")) + " demote " + Py.D(lvc.Get("demote")) + " synth " + Py.D(lvc.Get("synth")) +
                " | emitted keep " + Py.D(prim.Get("keep")) + " demote " + Py.D(prim.Get("demote")) + ", removed by options " + Py.D(removed) + ", lost " + Py.D(lost) + ", starts " + Py.D(starts)));
            var idx = new List<long>(); bool allFr = true;
            foreach (var s in specs) if (s.type == "StartEntityFreeRideRSData") { idx.Add(s.props.Get("Index") is long l ? l : -1); if (s.subscene != "FreeRide") allFr = false; }
            idx.Sort(); bool seq = idx.Count == 12; for (int i = 0; seq && i < 12; i++) seq = idx[i] == i + 1;
            res.Add(CheckRow("12 free-ride starts, Index 1..12, FreeRide sub-scene", starts == 12 && seq && allFr, Py.D(starts) + " starts, layout " + Py.Str(b.fr_layout?.Get("chosen"))));
            var ids = new HashSet<long>(); bool clash = false, range = true; long idMin = long.MaxValue, idMax = long.MinValue;
            foreach (var s in specs)
            {
                ids.Add(s.id); if (s.id >= TEMPLATE_ID_LO && s.id <= TEMPLATE_ID_HI) clash = true; if (!(s.id > 0 && s.id < 2147483648L)) range = false;
                idMin = Math.Min(idMin, s.id); idMax = Math.Max(idMax, s.id);
            }
            if (specs.Count == 0) throw new UxException("no entity specs (min() of an empty list)");
            res.Add(CheckRow("ids unique, 32-bit, clear of template ids", ids.Count == specs.Count && !clash && range, Py.D(specs.Count) + " ids " + Py.D(idMin) + ".." + Py.D(idMax)));
            bool fin = true; foreach (var s in specs) foreach (double x in s.matrix) if (double.IsNaN(x) || double.IsInfinity(x)) fin = false;
            res.Add(CheckRow("matrices finite", fin, ""));
            // LockScale only on unscaled collision-bearing entities
            bool ls = true;
            foreach (var s in specs) if (In(COLLISION_TYPES, s.type) || In(RENDER_TYPES, s.type)) if (Py.Eq(s.props.Get("LockScale"), 1L) != (s.collision && !IsScaled(s.matrix))) ls = false;
            res.Add(CheckRow("LockScale 1 exactly on unscaled collision-bearing entities", ls, ""));
            // physics family values
            bool pf = true;
            foreach (var s in specs)
                if (In(PHYSICS_FAMILY, s.type) && !(Py.Eq((s.props.Get("PhysicsType") as JObj)?.Get("EnumValue"), 9L) && Py.Eq(s.props.Get("DriveOn"), 0L) && Py.Eq(s.props.Get("AIAvoid"), 1L) &&
                    Py.Eq((s.props.Get("CollisionAudioMaterial") as JObj)?.Get("EnumValue"), 33L))) pf = false;
            res.Add(CheckRow("Physics/PopUp/PopDown: HaySolid(9), TuffBlock(33), DriveOn 0, AIAvoid 1", pf, ""));
            bool lt = true;
            foreach (var s in specs)
                if (s.type == "PopUpBaleRSData" || s.type == "PopDownBaleRSData")
                {
                    if (!(s.source.Get("id") is long sid) || !ents.TryGetValue(sid, out Ent e)) throw new UxException("pop bale " + s.name + " has no source entity");
                    if (!Py.Eq(s.props.Get("LinkedTriggerID"), e.props.Get("LinkedTriggerID"))) lt = false;
                }
            res.Add(CheckRow("LinkedTriggerID kept on pop bales", lt, ""));
            var en = new List<object>();
            foreach (var s in specs)
                foreach (var kv in s.props.items)
                    if (kv.Value is JObj v && v.Has("Enum") && STOCK_ENUMS.TryGetValue(kv.Key, out var tbl) &&
                        !(v.Get("Enum") is string name && tbl.TryGetValue(name, out long sv) && Py.Eq(sv, v.Get("EnumValue")))) en.Add(new object[] { s.name, kv.Key, v });
            res.Add(CheckRow("every written enum is a stock Reflex (name, value)", en.Count == 0, Head(en, 3)));
            // DESIGN 18 rollout defaults
            var offNs = new List<string>();
            foreach (var el in elems)
                foreach (var x in el.Iter())
                    if (x.Attr("namespace") is string ns && ns.Length > 0 && UxCommon.CanonNamespace(ns) != ns && !offNs.Contains(ns)) offNs.Add(ns);
            offNs.Sort(StringComparer.Ordinal);
            var offObj = new List<object>(); foreach (string ns in offNs) offObj.Add(ns);
            res.Add(CheckRow("every RSData namespace in the core spelling (ux_common.RSDATA_CORE_TYPES)", offNs.Count == 0, Head(offObj, 3)));
            if (b.o.crowd_tier == "stock")
            {
                bad = new List<object>();
                foreach (var s in specs) if (s.type == "BillboardCrowdRSData" && !Py.Eq(s.props.Get("Tier Number"), 0L)) bad.Add(s.name);
                res.Add(CheckRow("crowd-tier stock: every BillboardCrowdRSData Tier Number 0", bad.Count == 0, Head(bad, 5)));
            }
            if (b.o.group_number == "zero")
            {
                bad = new List<object>();
                for (int i = 0; i < specs.Count; i++)
                {
                    var P = PropsOf(elems[i]);
                    if (P.Has("GroupNumber") && !Py.Eq(GetValue(P["GroupNumber"]), 0L)) bad.Add(specs[i].name);
                }
                res.Add(CheckRow("group-number zero: GroupNumber 0 on every entity that has it", bad.Count == 0, Head(bad, 5)));
            }
            if (b.kind == "sx" && b.o.hb_freeride != "base")
            {
                bad = new List<object>(); var frExtra = new List<object>();
                foreach (var s in specs)
                {
                    if (s.subscene == "Base" && FreerideOnly(s.name)) bad.Add(s.name);
                    if (s.subscene == "FreeRide" && s.type != "StartEntityFreeRideRSData") frExtra.Add(s.name);
                }
                res.Add(CheckRow("hb-freeride " + b.o.hb_freeride + ": no freeride_only bale in Base; FreeRide extras " + Py.Str(frExtra),
                    bad.Count == 0 && (b.o.hb_freeride == "freeride" || frExtra.Count == 0), Head(bad, 5)));
            }
            return res;
        }
        /// np.allclose(g, v, rtol=0, atol=0) of a float or a list of floats.
        static bool ExactFloats(object g, object v)
        {
            if (v is List<object> vl)
            {
                if (!(g is List<object> gl) || gl.Count != vl.Count) return false;
                for (int i = 0; i < vl.Count; i++) if (!ExactFloats(gl[i], vl[i])) return false;
                return true;
            }
            return g != null && Py.IsNum(g) && Py.IsNum(v) && Py.Num(g) == Py.Num(v);
        }

        // ================================================================== run one level
        /// out_paths(lv, kind): the JSON file and the chunk folder, relative to the work store.
        public static string JsonPath(string kind) => kind == "sx" ? "entities.json" : "entities_fr.json";
        public const string ChunkDir = "entities";

        /// build_entities(level, kind, write, **opts): the Reflex entities of one level. With options.write also writes entities.json /
        /// entities_fr.json and the encoded chunks into the work store.
        public static UxEntitiesResult Convert(UxDiscLevel disc, UxStock stock, UxWork work, UxEntitiesOptions options = null, Action<string> progress = null, CancellationToken cancel = default)
        {
            var clock = System.Diagnostics.Stopwatch.StartNew();
            options = options ?? new UxEntitiesOptions();
            var b = new Builder(disc, stock, work, options, cancel);
            var specs = b.build();
            var elems = new List<UxElem>(specs.Count);
            foreach (var s in specs) elems.Add(SpecToElem(s, b.protos));
            var encoded = new Dictionary<string, List<byte[]>>(StringComparer.Ordinal);
            var checks = CheckSpecs(b, specs, elems, encoded);
            cancel.ThrowIfCancellationRequested();
            string kind = b.kind;
            var result = new UxEntitiesResult { items = new List<KeyValuePair<string, UxElem>>(), chunks = new Dictionary<string, byte[]>(StringComparer.Ordinal),
                problems = b.problems, failedChecks = new List<string>(), jsonPath = "", chunkDir = "" };
            for (int i = 0; i < specs.Count; i++) result.items.Add(new KeyValuePair<string, UxElem>(specs[i].subscene, elems[i]));
            var chunks = new JObj();
            foreach (string sub in new[] { "Base", "FreeRide" })
            {
                int n = 0; foreach (var s in specs) if (s.subscene == sub) n++;
                byte[] data = UxBin.Concat(encoded[sub].ToArray());
                result.chunks[sub] = data;
                chunks.Set(sub, new JObj().Set("entities", (long)n).Set("chunks", (long)encoded[sub].Count).Set("bytes", (long)data.Length).Set("md5", UxBin.Md5(data))
                    .Set("file", "entities/" + kind + "_" + sub + ".chunks")
                    .Set("note", sub == "FreeRide" ? "replaces the template FreeRide starts" : "append to <L>Base after the template entities"));
                if (options.write) work.WriteBytes(ChunkDir + "/" + kind + "_" + sub + ".chunks", data);
            }
            Counter byType = new Counter(), byRole = new Counter(), byClass = new Counter(); long collisionBearing = 0, partsExtra = 0, terrainParts = 0;
            foreach (var s in specs)
            {
                byType.Add(s.type); byRole.Add(s.role); byClass.Add(s.cls);
                if (s.collision) collisionBearing++;
                if (s.source.Has("part") && Py.Num(s.source.Get("part")) > 0) partsExtra++;
                if (s.cls == "terrain_part") terrainParts++;
            }
            var counts = new JObj().Set("specs", (long)specs.Count).Set("by_type", byType.ToJson(true)).Set("by_role", byRole.ToJson(true)).Set("by_class", byClass.ToJson(true))
                .Set("collision_bearing", collisionBearing).Set("parts_extra", partsExtra).Set("terrain_parts", terrainParts).Set("removed_by_options", b.removed.ToJson());
            if (b.rollout.Count > 0) counts.Set("rollout", b.rollout.ToJson(true));
            var manifests = new JObj();
            foreach (string k in Manifests.Keys) manifests.Set(k, new JObj().Set("path", b.M.paths[k]).Set("used", b.M.used[k]).Set("md5", b.M.md5[k]));
            var ptypes = new List<string>(byType.keys); if (!ptypes.Contains("DistanceFogRSData")) ptypes.Add("DistanceFogRSData");
            ptypes.Sort(StringComparer.Ordinal);
            var pmd5 = new JObj(); foreach (string t in ptypes) pmd5.Set(t, b.protos.Md5(t));
            var audio = new List<(string src, string dst, long n)>(b.audio_map);
            audio.Sort((x, y) => { int c = string.CompareOrdinal(x.src, y.src); if (c == 0) c = string.CompareOrdinal(x.dst, y.dst); return c != 0 ? c : x.n.CompareTo(y.n); });
            var audioJson = new List<object>(); foreach (var x in audio) audioJson.Add(new List<object> { x.src, x.dst, x.n });
            var dropped = new JObj(); var dk = new List<string>(b.dropped_props.Keys); dk.Sort(StringComparer.Ordinal);
            foreach (string k in dk) dropped.Set(k, b.dropped_props[k].ToJson());
            var warnings = new List<object>(); foreach (string w in b.M.warnings) warnings.Add(w); foreach (string w in b.warnings) warnings.Add(w);
            var problems = new List<object>(); foreach (string p in b.problems) problems.Add(p);
            var entities = new List<object>(specs.Count); foreach (var s in specs) entities.Add(s.ToJson());
            var checkList = new List<object>(); foreach (var c in checks) { checkList.Add(c); if (!(bool)c.Get("ok")) result.failedChecks.Add(c.Get("check") as string); }
            var doc = new JObj().Set("schema", SCHEMA).Set("level", b.lv.disc).Set("short", b.lv.shortName).Set("kind", kind).Set("y_lift", b.lift).Set("options", b.opts)
                .Set("id_scheme", new JObj().Set("primary", "Untamed entityId + " + Py.D(ID_OFFSET)).Set("synth", Py.D(SYNTH_ID_BASE) + " + n (extra parts, terrain parts)")
                    .Set("freeride_starts", Py.D(FR_ID_BASE) + " + Index").Set("template_ids", Py.D(TEMPLATE_ID_LO) + ".." + Py.D(TEMPLATE_ID_HI)))
                .Set("source", new JObj().Set("level_json", b.levelPath).Set("level_json_md5", UxBin.Md5File(work.PathOf(b.levelPath))).Set("manifests", manifests).Set("prototypes", pmd5)
                    .Set("tool", TOOL).Set("tool_md5", null).Set("hf", b.hf.source))
                .Set("counts", counts).Set("chunks", chunks).Set("distance_fog", b.fog).Set("freeride", b.fr_layout).Set("ridable_collision", b.ridable)
                .Set("audio_map", audioJson).Set("dropped_props", dropped).Set("checks", checkList).Set("problems", problems).Set("warnings", warnings).Set("entities", entities);
            if (b.rollout_notes.Count > 0) { var l = new List<object>(); foreach (string s in b.rollout_notes) l.Add(s); doc.Set("rollout_notes", l); }
            if (b.rollout_removed.Count > 0) { var l = new List<object>(); foreach (var s in b.rollout_removed) l.Add(s); doc.Set("rollout_removed", l); }
            if (b.rollout_moved.Count > 0) { var l = new List<object>(); foreach (var s in b.rollout_moved) l.Add(s); doc.Set("rollout_moved", l); }
            result.doc = doc;
            if (options.write)
            {
                work.WriteJson(JsonPath(kind), doc);
                result.jsonPath = JsonPath(kind); result.chunkDir = ChunkDir;
            }
            result.seconds = clock.Elapsed.TotalSeconds;
            progress?.Invoke("[" + b.lv.disc + " " + kind + "] " + (result.Ok ? "ok" : "FAIL") + "  " + specs.Count + " specs  " + Py.Str(byType.ToJson(true)) + "  coll " + collisionBearing +
                "  parts+" + partsExtra + "  terrain parts " + terrainParts + "  starts " + (b.fr_layout != null ? Py.Str(b.fr_layout.Get("chosen")) : "-") +
                "  (" + result.seconds.ToString("0.0", CultureInfo.InvariantCulture) + "s)");
            return result;
        }

        /// The stage entry of the contract: reads ctx.disc / ctx.stock / ctx.work, writes entities.json and entities/ into ctx.work.
        public static UxEntitiesResult Convert(UxContext ctx, UxEntitiesOptions options = null) => Convert(ctx.disc, ctx.stock, ctx.work, options, ctx.log, ctx.cancel);

        /// load_elems(level, kind): the (sub-scene, Entity Elem) list rebuilt from the written entities.json, and the document.
        public static List<KeyValuePair<string, UxElem>> LoadElems(UxWork work, UxStock stock, string kind, out JObj doc)
        {
            doc = work.ReadJsonObj(JsonPath(kind));
            var protos = new Prototypes(stock);
            var r = new List<KeyValuePair<string, UxElem>>();
            foreach (object s in UxJson.Arr(doc.Get("entities")))
            {
                var spec = Spec.FromJson(UxJson.Obj(s));
                r.Add(new KeyValuePair<string, UxElem>(spec.subscene, SpecToElem(spec, protos)));
            }
            return r;
        }
    }
}

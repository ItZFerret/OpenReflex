// MX vs ATV Untamed converter, stage S1 "level": the port of the Python reference converter's ux_level.py (build_level).
// It merges the Untamed race scene (<L>Base + <L>Championship<Mode> by entityId, Base first), classifies every entity
// (keep | demote | replace | synth | drop: the DESIGN 5.8 policy table as data, the name rules, the per-kind overrides, the model check,
// the sky dome and underworld rules), computes the asset closure of the kept and demoted entities with the header facts of every
// closure asset, and writes work/<Level>/level.json (kind sx) and level_fr.json (kind fr: the builder writes both, ux_collision reads
// both). The Python stage reads extract/<Level>/ (uxpkg's S0 extract and its index.json); here the assets come straight from the disc
// (UxDiscLevel): one pass reads the package front to back and decodes every asset of the level once on worker threads (size and md5
// of each = the rows of index.json, whose md5 is source.extract_index_md5), keeping what the header facts need. So every asset the
// later stages read is known to hold the reference's bytes when extract_index_md5 is the reference's.
// Numbers: the sky dome / underworld placement is float64 with the fused multiply-add chain of the reference's `P @ M` (UxNp.Fma);
// arctan2 is a double-double implementation (+, -, *, /, sqrt only), so the yaw of the free-ride start and the dome's elevation do not
// depend on the platform's C library; float texts are numpy's shortest float32 decimals, by integer arithmetic.
// System only, C# 9, .NET Standard 2.1.
using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Globalization;
using System.Numerics;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using Reflex.Port.Content.AliveImport;

namespace Reflex.Port.Content.UntamedImport
{
    /// The options of the level stage. The per-track option is rigidModels (ux_build_track --rigid-models; every current build: static).
    public sealed class UxLevelOptions
    {
        /// The kinds to build: 'sx' writes level.json, 'fr' writes level_fr.json (ux_build_track runs both; ux_collision reads both).
        public string[] kinds = { "sx", "fr" };
        /// build_level(rigid_models=): 'static' keeps entities whose model is a rigid multi-node model with identity node matrices and
        /// an unskinned surface (drawn as a static model); 'drop' = the old rule (DESIGN 5.4).
        public string rigidModels = "static";

        // ---- environment fields of level.json. Python writes absolute paths of the reference machine and the md5 of its own tool
        // files there; nothing reads them but the builder's freshness check. null = the neutral default named below.
        /// source.scene (Python: the absolute path of <Level>.xenon.scene). Default: the file's path inside the disc volume.
        public string scenePath;
        /// source.extract_index (Python: the absolute path of extract/<Level>/index.json). Default: 'extract/<Level>/index.json'.
        public string extractIndexPath;
        /// template.dir for kind sx / fr (Python: U.TEMPLATE_DIR[kind]). Default: 'templates/sx-sketch-slot1' / 'templates/huckit-freeride-slot1'.
        public string templateDirSx, templateDirFr;
        /// source.tool_md5 ('ux_level.py'), source.uxmesh_md5 and source.readers[name] by tool file name. Default: UxLevel.ReferenceToolMd5,
        /// the md5 of the Python tool files this port was made from and compared with.
        public IDictionary<string, string> toolMd5;
        /// The index.json whose md5 is source.extract_index_md5 has CRLF line ends (the reference wrote it through a text-mode file on
        /// Windows). Keep true: the value is then the same on every platform and equal to the reference's.
        public bool extractIndexCrlf = true;

        /// As ux_build_track: a level with problems (a closure miss, an unknown entity type, no free-ride start) is not written and
        /// the stage throws. false: the file is written and the problems are returned (the ux_level.py command line's behaviour).
        public bool failOnProblems = true;
        /// Worker threads of the asset pass (0 = the processor count). The output does not depend on it.
        public int threads;
    }

    /// What one kind of the level stage produced (the summary row of ux_level.run_one).
    public sealed class UxLevelSummary
    {
        public string kind, file;                    // 'sx' | 'fr'; the work-store path written (null when not written)
        public int entities, warnings;
        public JObj by_class, closure;               // {cls: n}, {asset type: n}
        public List<string> problems = new List<string>();
    }

    public sealed class UxLevelResult
    {
        public readonly List<UxLevelSummary> kinds = new List<UxLevelSummary>();
        public string extractIndexMd5, sceneMd5;
        /// Wall time: the whole stage; the scene (read, parse, merge); the asset pass (read, decode, md5); the kinds (classify, closure); writing.
        public double seconds, secondsScene, secondsAssets, secondsBuild, secondsWrite;
    }

    /// ux_level.py.
    public static class UxLevel
    {
        public const string SCHEMA = "ux_level/1";
        public const double SKY_MIN_RADIUS_FT = 4000.0, SKY_MIN_ELEV_DEG = 60.0;
        public const double UNDERWORLD_FT = 100.0;   // rule '+underworld': a render-only model wholly this far below the lowest trncol vertex
        public static readonly string[] CLASSES = { "keep", "demote", "replace", "synth", "drop" };
        public static readonly string[] CLOSURE_TYPES = { "model", "surface", "material", "texture", "collis", "terrain", "trncol" };
        public static readonly string[] RIGID_CHOICES = { "static", "drop" };
        public const string TOOL = "ux_level.py";

        /// The md5 of the reference's tool files this port mirrors (the Python reference converter, 2026-11-10): the default of
        /// source.tool_md5, source.uxmesh_md5 and source.readers.
        public static readonly IDictionary<string, string> ReferenceToolMd5 = new Dictionary<string, string>(StringComparer.Ordinal)
        {
            { "ux_level.py", "986d0e84f3a1fcea4ce117053545ccd4" },
            { "uxscene.py", "8e6652f219ca2f5d9ef891ca78bb1285" },
            { "uxmesh.py", "146690511db2f48584732c12c5be876e" },
            { "uxmat.py", "5db0bfcc1c7b6eb597f353d1385fd8a5" },
            { "uxtex.py", "77b39dabdbc113f0ba5e53d518bc1a36" },
            { "ux_common.py", "90d40f0617d7c0eb2a90b22de7b66972" },
        };
        static readonly string[] READERS = { "uxscene.py", "uxmesh.py", "uxmat.py", "uxtex.py", "ux_common.py" };

        /// The work-store file of a kind (ux_level.out_path).
        public static string OutPath(string kind) => kind == "sx" ? "level.json" : "level_fr.json";
        /// U.TEMPLATE_DIR[kind] as a neutral relative name (the reference's templates folder).
        public static string DefaultTemplateDir(string kind) => kind == "sx" ? "templates/sx-sketch-slot1" : "templates/huckit-freeride-slot1";

        // ------------------------------------------------------------------------------------------- policy (DESIGN 5.8 table as data)
        sealed class Policy
        {
            public readonly string key, cls, reflex, note; public readonly bool design;
            public Policy(string key, string cls, string reflex, string note, bool design = true) { this.key = key; this.cls = cls; this.reflex = reflex; this.note = note; this.design = design; }
        }
        const string TEMPLATE_KEPT = "the template entity of this type is kept";
        const string DEMOTE_NOTE = "static CollisionRSData with the same model and collis (no runtime behaviour)";
        /// Untamed RSData type -> class for the race build (kind sx). design false = not named in the DESIGN 5.8 table. A key ending
        /// in '*' is a prefix rule.
        static readonly Policy[] POLICY =
        {
            new Policy("ModelRSData", "keep", "ModelRSData", "ModelRSData (+ _pN / cell parts); a sky dome model is dropped (rule +sky, the template SkyRSData draws the sky)"),
            new Policy("CollisionRSData", "keep", "CollisionRSData", "AudioCollisionMaterial->CollisionAudioMaterial by name table; DrivingZone/SoundZone/" +
                "AudioCollisionSubclass dropped; DriveOnMaterial ID 0, UseSoftCollision 0; PhysicsType/DriveOn by role (5.6). TO_StartingGate: see NAME_RULES"),
            new Policy("PhysicsRSData", "keep", "PhysicsRSData", "PhysicsType HaySolid(9) for every PhysicsAsset; CollisionAudioMaterial TuffBlock(33), " +
                "DriveOn 0, AIAvoid 1, AudioEmitter from the stock prototype"),
            new Policy("PopUpBaleRSData", "keep", "PopUpBaleRSData", "same mapping as PhysicsRSData + LinkedTriggerID, HaySolid"),
            new Policy("PopDownBaleRSData", "keep", "PopDownBaleRSData", "same mapping as PhysicsRSData + LinkedTriggerID, HaySolid"),
            new Policy("TriggerRSData", "keep", "TriggerRSData", "same props in both games"),
            new Policy("BillboardCrowdRSData", "keep", "BillboardCrowdRSData", "crowd decl + crowd materials"),
            new Policy("DistanceFogRSData", "replace", "template DistanceFogRSData", "values copied into the template DistanceFog"),
            new Policy("DropGateRSData", "replace", "template DropgateRSData TO_DropGate", "pose drives the template gate group (S8)"),
            new Policy("ThirtySecondBoardRSData", "replace", "template ThirtySecondBoardRSData", "pose drives the template board group (S8)"),
            new Policy("LeaderBoardRSData", "demote", "CollisionRSData", DEMOTE_NOTE),
            new Policy("ScoreTowerFlagHolderRSData", "demote", "CollisionRSData", DEMOTE_NOTE),
            new Policy("HoleShotLightRSData", "demote", "CollisionRSData", DEMOTE_NOTE),
            new Policy("TripleLightRSData", "demote", "CollisionRSData", DEMOTE_NOTE),
            new Policy("LeaderLightRSData", "demote", "CollisionRSData", DEMOTE_NOTE),
            new Policy("StartEntityFreeRideLineRSData", "synth", "StartEntityFreeRideRSData x12 in <L>FreeRide", "Index 1..12, same yaw, 3 ft apart along -right, Y = HF + 1 ft"),
            new Policy("TerrainRSData", "replace", "template HFTerrainRSData", "heightfield (S6); the template HFTerrainRSData stays; terrain assets are in the closure"),
            new Policy("ImageBasedLightRSData", "drop", null, "v1: template AmbientDirectionalLight/HDR/Overlay kept; IBL is the A/B variant --light ibl"),
            new Policy("MudManagerRSData", "drop", null, TEMPLATE_KEPT),
            new Policy("ParticleManagerRSData", "drop", null, TEMPLATE_KEPT),
            new Policy("MassiveAdEntityRSData", "drop", null, TEMPLATE_KEPT + " (its 18 MassiveAd sit under the HF)"),
            new Policy("AnimEntity*", "drop", null, "animated entity (crowd people, roaming, spline props): skinned or multi-node models and anim assets are not converted"),
            new Policy("SXSeatedCameraManRSData", "drop", null, "animated camera man (DESIGN 5.8: skinned row)"),
            new Policy("MXCameraPathRSData", "drop", null, "Untamed anim assets not converted; template cameras kept"),
            new Policy("MXReplayTowerCameraRSData", "drop", null, "Untamed replay cameras not converted; template cameras kept"),
            new Policy("CameraPanoramicRSData", "drop", null, "Untamed camera not converted; template cameras kept"),
            new Policy("JumbotronCameraRSData", "drop", null, "Untamed camera not converted; template cameras kept"),
            new Policy("AudioCrowdEmitterRSData", "drop", null, "Untamed audio not converted"),
            new Policy("TracksEntityRSData", "drop", null, "Untamed tyre-track system not converted"),
            new Policy("MudSplatterRSData", "drop", null, "Untamed effect not converted"),
            new Policy("ShadowPointEntityRSData", "drop", null, "Untamed effect not converted"),
            new Policy("LightFlareRSData", "drop", null, "Untamed effect not converted"),
            new Policy("TextureAnimControllerRSData", "drop", null, "texture animation not converted (surface 0 only)"),
            new Policy("AAARSData", "drop", null, "Untamed-only type"),
            new Policy("ScreenDustRSData", "drop", null, "Untamed effect not converted"),
            new Policy("WaterSystemRSData", "drop", null, "v1: water pits stay dry (A/B in WP13)"),
            new Policy("ForestSystemRSData", "drop", null, "v1: forests are backdrop (A/B in WP13)"),
            // not named in the DESIGN 5.8 table (WP1 survey of the 12 levels):
            new Policy("AudioZoneCubeRSData", "drop", null, "audio reverb zone; the template audio managers are kept", false),
            new Policy("OverlayLightRSData", "drop", null, "v1 uses the template lighting; " + TEMPLATE_KEPT, false),
            new Policy("ParticleSphereRSData", "drop", null, "particle emitter; Untamed particles not converted", false),
            new Policy("StartEntityFreeRideRSData", "drop", null, "the 12 free-ride starts are synthesised from the free-ride line pose (synth when it is the pose source)", false),
            new Policy("StartEntityFreestyleRSData", "drop", null, "freestyle start (not a target mode)", false),
            new Policy("StartEntitySnakeTagRSData", "drop", null, "mini-game start (not a target mode)", false),
        };
        const string FREERIDE_LINE_REFLEX = "StartEntityFreeRideRSData x12 in <L>FreeRide";       // POLICY['StartEntityFreeRideLineRSData']['reflex']

        sealed class Rule
        {
            public readonly string name, kind, cls, reflex, why;
            public Rule(string name, string kind, string cls, string reflex, string why) { this.name = name; this.kind = kind; this.cls = cls; this.reflex = reflex; this.why = why; }
        }
        /// Entity-name rules, checked before the type policy (per kind): NAME_RULES[name][kind].
        static readonly Rule[] NAME_RULES =
        {
            new Rule("TO_StartingGate", "sx", "replace", "template CollisionRSData TO_StartingGate",
                "replaced by the template TO_StartingGate (GL_StartGate, 16 GateStart nodes); the Untamed gate base is a rigid multi-node model"),
            new Rule("TO_StartingGate", "fr", "drop", null, "no gate kit in the free-ride template (DESIGN 7); the Untamed gate base is a rigid multi-node model"),
        };
        /// Per-kind changes to the type policy: KIND_OVERRIDES[kind][type]. The free-ride template has no tdf, gate kit or 30 s board.
        static readonly Rule[] KIND_OVERRIDES =
        {
            new Rule("DropGateRSData", "fr", "drop", null, "no gate kit in the free-ride template (DESIGN 7)"),
            new Rule("ThirtySecondBoardRSData", "fr", "drop", null, "no race kit in the free-ride template (DESIGN 7)"),
        };
        static readonly string[] KINDS = { "sx", "fr" };

        /// policy_for(typ): the exact entry, else the longest '*' prefix rule, else null.
        static Policy PolicyFor(string typ)
        {
            foreach (var p in POLICY) if (p.key == typ) return p;
            Policy best = null;
            foreach (var p in POLICY)
                if (p.key.EndsWith("*", StringComparison.Ordinal) && typ.StartsWith(p.key.Substring(0, p.key.Length - 1), StringComparison.Ordinal) && (best == null || p.key.Length > best.key.Length)) best = p;
            return best;
        }

        /// classify(etype, name, kind): (cls, reflex, reason, rule) before the model check; cls null = the type is not in the table.
        static (string cls, string reflex, string reason, string rule) Classify(string etype, object name, string kind)
        {
            if (name is string nm)
                foreach (var r in NAME_RULES) if (r.name == nm && r.kind == kind) return (r.cls, r.reflex, r.why, "name:" + nm);
            foreach (var r in KIND_OVERRIDES) if (r.kind == kind && r.name == etype) return (r.cls, r.reflex, r.why, "kind:" + kind);
            var p = PolicyFor(etype);
            if (p == null) return (null, null, "UNKNOWN type: not in the policy table", null);
            return (p.cls, p.reflex, p.note, "type:" + p.key);
        }

        // ------------------------------------------------------------------------------------------- Python text
        static string I(long v) => v.ToString(CultureInfo.InvariantCulture);

        /// '%s' % v.
        static string PyStr(object v)
        {
            switch (v)
            {
                case null: return "None";
                case string s: return s;
                case bool b: return b ? "True" : "False";
                case long l: return I(l);
                case int i: return I(i);
                case double d: return PyFloat(d);
                default: return PyRepr(v);
            }
        }
        static string PyFloat(double d) => double.IsNaN(d) ? "nan" : double.IsInfinity(d) ? (d > 0 ? "inf" : "-inf") : PyFormat.Repr(d);

        /// '%r' % v (str, None, numbers, lists, dicts, the ('raw', type, offset) tuple).
        static string PyRepr(object v)
        {
            switch (v)
            {
                case null: return "None";
                case string s: return StrRepr(s);
                case bool b: return b ? "True" : "False";
                case long l: return I(l);
                case int i: return I(i);
                case double d: return PyFloat(d);
                case UxRawRef raw: return "('raw', " + I(raw.type) + ", " + I(raw.offset) + ")";
                case JObj o:
                {
                    var sb = new StringBuilder("{");
                    for (int k = 0; k < o.items.Count; k++) { if (k > 0) sb.Append(", "); sb.Append(StrRepr(o.items[k].Key)).Append(": ").Append(PyRepr(o.items[k].Value)); }
                    return sb.Append('}').ToString();
                }
                case System.Collections.IEnumerable list:
                {
                    var sb = new StringBuilder("["); bool first = true;
                    foreach (var x in list) { if (!first) sb.Append(", "); first = false; sb.Append(PyRepr(x)); }
                    return sb.Append(']').ToString();
                }
                default: return System.Convert.ToString(v, CultureInfo.InvariantCulture);
            }
        }
        /// repr(str).
        static string StrRepr(string s)
        {
            char q = s.IndexOf('\'') >= 0 && s.IndexOf('"') < 0 ? '"' : '\'';
            var sb = new StringBuilder(s.Length + 2); sb.Append(q);
            foreach (char c in s)
            {
                if (c == q || c == '\\') sb.Append('\\').Append(c);
                else if (c == '\n') sb.Append("\\n");
                else if (c == '\r') sb.Append("\\r");
                else if (c == '\t') sb.Append("\\t");
                else if (c < 0x20 || c == 0x7F || (c >= 0x80 && c <= 0xA0) || c == 0xAD) sb.Append("\\x").Append(((int)c).ToString("x2", CultureInfo.InvariantCulture));
                else sb.Append(c);
            }
            return sb.Append(q).ToString();
        }

        /// '%.<digits>f' % x: the exact binary value rounded to the decimal place, ties to even (the same text on every runtime).
        internal static string Fixed(double x, int digits)
        {
            if (double.IsNaN(x)) return "nan";
            if (double.IsInfinity(x)) return x > 0 ? "inf" : "-inf";
            long bits = BitConverter.DoubleToInt64Bits(x);
            bool neg = bits < 0; int be = (int)((bits >> 52) & 0x7FF); long frac = bits & 0xFFFFFFFFFFFFFL;
            long m = be == 0 ? frac : frac | (1L << 52); int e = be == 0 ? -1074 : be - 1075;      // |x| = m * 2^e
            BigInteger num = m, den = BigInteger.One;
            if (e >= 0) num <<= e; else den <<= -e;
            num *= BigInteger.Pow(10, digits);
            var q = BigInteger.DivRem(num, den, out var r);
            int c = (2 * r).CompareTo(den);
            if (c > 0 || c == 0 && !q.IsEven) q += 1;
            string s = q.ToString(CultureInfo.InvariantCulture);
            if (digits > 0)
            {
                if (s.Length <= digits) s = new string('0', digits + 1 - s.Length) + s;
                s = s.Substring(0, s.Length - digits) + "." + s.Substring(s.Length - digits);
            }
            return (neg ? "-" : "") + s;
        }

        /// Python truth of a property value.
        static bool Truthy(object v)
        {
            switch (v)
            {
                case null: return false;
                case string s: return s.Length > 0;
                case bool b: return b;
                case long l: return l != 0;
                case int i: return i != 0;
                case double d: return d != 0;
                case JObj o: return o.items.Count > 0;
                case List<object> l: return l.Count > 0;
                default: return true;
            }
        }
        /// A dictionary key that is equal when the Python values are (5 == 5.0, a str only equals a str).
        static string KeyOf(object v)
        {
            switch (v)
            {
                case null: return "n";
                case string s: return "s" + s;
                case bool b: return "i" + (b ? "1" : "0");
                case long l: return "i" + I(l);
                case int i: return "i" + I(i);
                case double d: return d == Math.Floor(d) && Math.Abs(d) < 9e15 ? "i" + I((long)d) : "f" + PyFloat(d);
                default: return "r" + PyRepr(v);
            }
        }

        // ------------------------------------------------------------------------------------------- JSON helpers
        /// float(str(np.float32(v))): the double nearest to the shortest decimal that reads back as the same float32 (numpy's Dragon4
        /// in unique mode: the fewest digits inside the rounding interval, the closest of them, a tie to the even digit). Exact
        /// integer arithmetic.
        internal static double F32Shortest(float v) => F32Short(v, out _);

        /// F32Shortest, and the text json.dumps writes for the result (repr of that double: the same digits), or null for a NaN / infinity.
        internal static double F32Short(float v, out string text)
        {
            text = null;
            if (float.IsNaN(v) || float.IsInfinity(v)) return v;
            if (v == 0) { text = BitConverter.SingleToInt32Bits(v) < 0 ? "-0.0" : "0.0"; return v; }
            float av = Math.Abs(v);
            if (av < 16777216f && av == (float)Math.Floor(av)) { text = I((long)v) + ".0"; return v; }      // an integer below 2^24 is its own shortest decimal
            int bits = BitConverter.SingleToInt32Bits(av);
            int be = (bits >> 23) & 0xFF; int frac = bits & 0x7FFFFF;
            long m = be == 0 ? frac : frac | (1 << 23); int e = be == 0 ? -149 : be - 150;      // av = m * 2^e
            // av and its rounding interval as fractions over a common denominator: num / den, (num - halfLow, num + halfHigh) / den
            BigInteger num, den, halfLow, halfHigh;
            bool asym = frac == 0 && be > 1;            // a power of two: the gap below is half the gap above
            if (e >= 0) { num = new BigInteger(m) << (e + 2); den = 4; halfHigh = BigInteger.One << (e + 1); halfLow = asym ? BigInteger.One << e : halfHigh; }
            else { num = new BigInteger(m) << 2; den = BigInteger.One << (2 - e); halfHigh = 2; halfLow = asym ? 1 : 2; }
            bool inclusive = (m & 1) == 0;
            int k = (int)Math.Floor(Math.Log10(av));    // 10^k <= av < 10^(k+1), corrected exactly below
            while (Pow10Cmp(num, den, k) < 0) k--;
            while (Pow10Cmp(num, den, k + 1) >= 0) k++;
            for (int p = 1; p <= 9; p++)
            {
                int scale = p - 1 - k;                  // candidates: integers near av * 10^scale
                BigInteger n = num, d = den, lo = num - halfLow, hi = num + halfHigh;
                if (scale >= 0) { var t = BigInteger.Pow(10, scale); n *= t; lo *= t; hi *= t; } else d *= BigInteger.Pow(10, -scale);
                BigInteger q = BigInteger.DivRem(n, d, out BigInteger r);       // floor
                BigInteger best = BigInteger.MinusOne;
                bool down = q > 0 && Inside(q * d, lo, hi, inclusive), up = Inside((q + 1) * d, lo, hi, inclusive);
                if (down && up)
                {
                    int c = (2 * r).CompareTo(d);
                    best = c < 0 ? q : c > 0 ? q + 1 : (q.IsEven ? q : q + 1);
                }
                else if (down) best = q;
                else if (up) best = q + 1;
                if (best < 0) continue;
                string ds = best.ToString(CultureInfo.InvariantCulture);
                double val = UxJson.ParseDouble(ds + "e" + (-scale).ToString(CultureInfo.InvariantCulture));
                text = (v < 0 ? "-" : "") + ReprText(ds.TrimEnd('0'), ds.Length - scale);
                return v < 0 ? -val : val;
            }
            return v;       // nine digits always identify a float32; not reached
        }
        /// repr(float) of the value 0.<digits> * 10^point (PyFormat.Repr's layout: fixed notation for 1e-4 <= x < 1e16, else d.ddde+XX).
        static string ReprText(string digits, int point)
        {
            int sci = point - 1;
            if (sci >= -4 && sci < 16)
            {
                if (point <= 0) return "0." + new string('0', -point) + digits;
                if (point >= digits.Length) return digits + new string('0', point - digits.Length) + ".0";
                return digits.Substring(0, point) + "." + digits.Substring(point);
            }
            return (digits.Length > 1 ? digits[0] + "." + digits.Substring(1) : digits) + "e" + (sci < 0 ? "-" : "+") + Math.Abs(sci).ToString("00", CultureInfo.InvariantCulture);
        }
        static bool Inside(BigInteger x, BigInteger lo, BigInteger hi, bool inclusive)
        {
            int cl = x.CompareTo(lo), ch = x.CompareTo(hi);
            return inclusive ? cl >= 0 && ch <= 0 : cl > 0 && ch < 0;
        }
        static int Pow10Cmp(BigInteger num, BigInteger den, int k)
            => k >= 0 ? num.CompareTo(den * BigInteger.Pow(10, k)) : (num * BigInteger.Pow(10, -k)).CompareTo(den);

        sealed class Cleaner
        {
            readonly Dictionary<int, double> _cache = new Dictionary<int, double>();
            /// The JSON text of the doubles met so far, by their bits (a float32's is known from its shortest decimal).
            public readonly Dictionary<long, string> repr = new Dictionary<long, string>();
            /// _f32s(x): the shortest decimal that reads back to the same float32 when x is one; other floats unchanged.
            public double F32s(double x)
            {
                if (double.IsNaN(x) || double.IsInfinity(x)) throw new UxException("non-finite float " + PyFloat(x));
                float v = (float)x;
                if ((double)v != x) return x;
                int key = BitConverter.SingleToInt32Bits(v);
                if (!_cache.TryGetValue(key, out double r))
                {
                    r = F32Short(v, out string text); _cache[key] = r;
                    if (text != null) repr[BitConverter.DoubleToInt64Bits(r)] = text;
                }
                return r;
            }
            /// clean(o).
            public object Clean(object o)
            {
                switch (o)
                {
                    case null: return null;
                    case string _: case bool _: case long _: return o;
                    case int i: return (long)i;
                    case double d: return F32s(d);
                    case JObj j:
                    {
                        var r = new JObj();
                        foreach (var kv in j.items) r.items.Add(new KeyValuePair<string, object>(kv.Key, Clean(kv.Value)));
                        return r;
                    }
                    case UxRawRef raw: return new List<object> { "raw", raw.type, raw.offset };
                    case List<object> l:
                    {
                        var r = new List<object>(l.Count);
                        foreach (var x in l) r.Add(Clean(x));
                        return r;
                    }
                    case double[] a:
                    {
                        var r = new List<object>(a.Length);
                        foreach (double x in a) r.Add(F32s(x));
                        return r;
                    }
                    case System.Collections.IEnumerable en:
                    {
                        var r = new List<object>();
                        foreach (var x in en) r.Add(Clean(x));
                        return r;
                    }
                    default: return o;
                }
            }
        }

        /// ux_common.dumps_json(doc, 1) of a cleaned document (objects, lists, strings, integers, floats, booleans, null): sorted keys,
        /// indent 1, raw UTF-8, a final newline. The same bytes as UxJson.DumpsUx; the float texts are cached (a level.json holds
        /// tens of thousands of floats, most of them repeated or float32 values whose text the cleaner already knows).
        static string DumpsDocument(object doc, Dictionary<long, string> repr)
        {
            var sb = new StringBuilder(1 << 22);
            WriteJson(sb, doc, 0, repr);
            return sb.Append('\n').ToString();
        }
        static void WriteJson(StringBuilder sb, object v, int depth, Dictionary<long, string> repr)
        {
            switch (v)
            {
                case null: sb.Append("null"); return;
                case bool b: sb.Append(b ? "true" : "false"); return;
                case string s: WriteJsonString(sb, s); return;
                case long l: sb.Append(l.ToString(CultureInfo.InvariantCulture)); return;
                case double d:
                {
                    long bits = BitConverter.DoubleToInt64Bits(d);
                    if (!repr.TryGetValue(bits, out string t))
                    {
                        if (double.IsNaN(d) || double.IsInfinity(d)) throw new UxException("Out of range float values are not JSON compliant: " + PyFloat(d));
                        t = PyFormat.Repr(d); repr[bits] = t;
                    }
                    sb.Append(t); return;
                }
                case JObj o:
                {
                    if (o.items.Count == 0) { sb.Append("{}"); return; }
                    var items = o.items.ToArray();
                    Array.Sort(items, (a, c) => string.CompareOrdinal(a.Key, c.Key));
                    sb.Append('{');
                    for (int k = 0; k < items.Length; k++)
                    {
                        if (k > 0) sb.Append(',');
                        sb.Append('\n').Append(' ', depth + 1);
                        WriteJsonString(sb, items[k].Key); sb.Append(": ");
                        WriteJson(sb, items[k].Value, depth + 1, repr);
                    }
                    sb.Append('\n').Append(' ', depth).Append('}');
                    return;
                }
                case List<object> list:
                {
                    if (list.Count == 0) { sb.Append("[]"); return; }
                    sb.Append('[');
                    for (int k = 0; k < list.Count; k++)
                    {
                        if (k > 0) sb.Append(',');
                        sb.Append('\n').Append(' ', depth + 1);
                        WriteJson(sb, list[k], depth + 1, repr);
                    }
                    sb.Append('\n').Append(' ', depth).Append(']');
                    return;
                }
                default: throw new UxException("not JSON serialisable: " + v.GetType().Name);
            }
        }
        static void WriteJsonString(StringBuilder sb, string s)
        {
            sb.Append('"');
            int run = 0;
            for (int i = 0; i < s.Length; i++)
            {
                char c = s[i];
                if (c >= 0x20 && c != '"' && c != '\\') continue;
                sb.Append(s, run, i - run); run = i + 1;
                switch (c)
                {
                    case '"': sb.Append("\\\""); break;
                    case '\\': sb.Append("\\\\"); break;
                    case '\n': sb.Append("\\n"); break;
                    case '\r': sb.Append("\\r"); break;
                    case '\t': sb.Append("\\t"); break;
                    case '\b': sb.Append("\\b"); break;
                    case '\f': sb.Append("\\f"); break;
                    default: sb.Append("\\u").Append(((int)c).ToString("x4", CultureInfo.InvariantCulture)); break;
                }
            }
            sb.Append(s, run, s.Length - run).Append('"');
        }

        /// json.dumps(clean(a)) == json.dumps(clean(b)) for two property values: the same structure, key order, types and float32 values.
        static bool SameProps(object a, object b)
        {
            if (a == null || b == null) return a == null && b == null;
            if (a is JObj ja)
            {
                if (!(b is JObj jb) || ja.items.Count != jb.items.Count) return false;
                for (int i = 0; i < ja.items.Count; i++) if (ja.items[i].Key != jb.items[i].Key || !SameProps(ja.items[i].Value, jb.items[i].Value)) return false;
                return true;
            }
            if (a is List<object> la)
            {
                if (!(b is List<object> lb) || la.Count != lb.Count) return false;
                for (int i = 0; i < la.Count; i++) if (!SameProps(la[i], lb[i])) return false;
                return true;
            }
            if (a is double da) return b is double db && BitConverter.DoubleToInt64Bits(da) == BitConverter.DoubleToInt64Bits(db);
            if (a is long xa) return b is long xb && xa == xb;
            if (a is string sa) return b is string sb && sa == sb;
            if (a is UxRawRef ra) return b is UxRawRef rb && ra.type == rb.type && ra.offset == rb.offset;
            return a.Equals(b);
        }

        // ------------------------------------------------------------------------------------------- arctan2, platform independent
        // Double-double arithmetic (Dekker / Knuth; no fused multiply-add), accurate to about 2^-100: the rounded result is the
        // correctly rounded arctan2 except in cases closer to a rounding boundary than that.
        static void TwoSum(double a, double b, out double s, out double e) { s = a + b; double bb = s - a; e = (a - (s - bb)) + (b - bb); }
        static void Quick(double a, double b, out double s, out double e) { s = a + b; e = b - (s - a); }
        static void TwoProd(double a, double b, out double p, out double e)
        {
            p = a * b;
            double t = 134217729.0 * a, ah = t - (t - a), al = a - ah;
            t = 134217729.0 * b; double bh = t - (t - b), bl = b - bh;
            e = ((ah * bh - p) + ah * bl + al * bh) + al * bl;
        }
        static void DdAdd(double ah, double al, double bh, double bl, out double h, out double l)
        {
            TwoSum(ah, bh, out double s, out double e); TwoSum(al, bl, out double t, out double f);
            e += t; Quick(s, e, out s, out e); e += f; Quick(s, e, out h, out l);
        }
        static void DdMul(double ah, double al, double bh, double bl, out double h, out double l)
        {
            TwoProd(ah, bh, out double p, out double e);
            e += ah * bl + al * bh;
            Quick(p, e, out h, out l);
        }
        static void DdDiv(double ah, double al, double bh, double bl, out double h, out double l)
        {
            double q1 = ah / bh;
            DdMul(bh, bl, q1, 0, out double ph, out double pl);
            DdAdd(ah, al, -ph, -pl, out double rh, out double rl);
            double q2 = rh / bh;
            DdMul(bh, bl, q2, 0, out ph, out pl);
            DdAdd(rh, rl, -ph, -pl, out rh, out rl);
            double q3 = rh / bh;
            Quick(q1, q2, out double s, out double e);
            DdAdd(s, e, q3, 0, out h, out l);
        }
        static void DdSqrt(double ah, double al, out double h, out double l)
        {
            double x = 1.0 / Math.Sqrt(ah), ax = ah * x;
            TwoProd(ax, ax, out double ph, out double pl);
            DdAdd(ah, al, -ph, -pl, out double dh, out double _);
            TwoSum(ax, dh * (x * 0.5), out h, out l);
        }
        const double PI_HI = 3.141592653589793, PI_LO = 1.2246467991473532e-16;
        const int ATAN_TERMS = 18;
        static readonly double[] s_invOddHi = new double[ATAN_TERMS + 1], s_invOddLo = new double[ATAN_TERMS + 1];
        static UxLevel()
        {
            for (int n = 0; n <= ATAN_TERMS; n++) DdDiv(1.0, 0, 2 * n + 1, 0, out s_invOddHi[n], out s_invOddLo[n]);
        }

        /// arctan2(y, x) as a double, by double-double arithmetic for finite non-zero arguments of ordinary size; the special cases
        /// (zeros, infinities, NaN, magnitudes outside 2^-400 .. 2^400) are Math.Atan2's.
        internal static double Atan2(double y, double x)
        {
            double ax = Math.Abs(x), ay = Math.Abs(y);
            const double tiny = 3.87e-121, huge = 2.58e120;         // 2^-400, 2^400
            if (!(ax > tiny && ax < huge && ay > tiny && ay < huge)) return Math.Atan2(y, x);
            bool swap = ay > ax;
            DdDiv(swap ? ax : ay, 0, swap ? ay : ax, 0, out double th, out double tl);       // t in (0, 1]
            // atan(t) = 8 atan(u) after three halvings u = t / (1 + sqrt(1 + t^2)): u <= tan(pi / 32)
            for (int k = 0; k < 3; k++)
            {
                DdMul(th, tl, th, tl, out double sh, out double sl);
                DdAdd(sh, sl, 1.0, 0, out sh, out sl);
                DdSqrt(sh, sl, out double rh, out double rl);
                DdAdd(rh, rl, 1.0, 0, out rh, out rl);
                DdDiv(th, tl, rh, rl, out th, out tl);
            }
            DdMul(th, tl, th, tl, out double uh, out double ul);
            double ph = s_invOddHi[ATAN_TERMS], pl = s_invOddLo[ATAN_TERMS];
            for (int n = ATAN_TERMS - 1; n >= 0; n--)
            {
                DdMul(ph, pl, uh, ul, out ph, out pl);
                DdAdd(s_invOddHi[n], s_invOddLo[n], -ph, -pl, out ph, out pl);
            }
            DdMul(ph, pl, th, tl, out double ah, out double al);
            ah *= 8; al *= 8;
            if (swap) DdAdd(PI_HI * 0.5, PI_LO * 0.5, -ah, -al, out ah, out al);
            if (x < 0) DdAdd(PI_HI, PI_LO, -ah, -al, out ah, out al);
            double a = ah + al;
            return y < 0 ? -a : a;
        }
        const double RAD2DEG = 180.0 / Math.PI;

        // ------------------------------------------------------------------------------------------- extract index
        sealed class IxAsset
        {
            public int index; public UxDiscAsset asset;
            public string name, type, package, file, md5; public long size;
            public byte[] kept;             // the bytes the header facts need (whole for small types and surfaces, the head of a texture)
        }

        /// ExtractIndex: extract/<Level>/index.json looked up by (type, name) ignoring case; the first entry of a pair wins.
        sealed class Index
        {
            public readonly List<IxAsset> assets = new List<IxAsset>();
            readonly Dictionary<string, IxAsset> _byKey = new Dictionary<string, IxAsset>(StringComparer.Ordinal);
            readonly Dictionary<string, IxAsset> _byFile = new Dictionary<string, IxAsset>(StringComparer.Ordinal);
            public void Add(IxAsset a)
            {
                assets.Add(a);
                string k = a.type + "\n" + UxCommon.PyLower(a.name);
                if (!_byKey.ContainsKey(k)) _byKey[k] = a;
                string f = UxCommon.PyLower(a.file);
                if (!_byFile.ContainsKey(f)) _byFile[f] = a;
            }
            /// get(name, typ).
            public IxAsset Get(object name, string typ)
            {
                if (!Truthy(name) || typ == null) return null;
                return _byKey.TryGetValue(typ + "\n" + UxCommon.PyLower(PyStr(name)), out var a) ? a : null;
            }
            /// The asset behind a path below extract/<Level> (the reference's file system ignores case), or null.
            public IxAsset File(string file) => _byFile.TryGetValue(UxCommon.PyLower(file), out var a) ? a : null;
        }

        // ------------------------------------------------------------------------------------------- header facts
        const int TEXTURE_HEAD = 0x28 + 16 * 64;

        /// model_facts.
        static JObj ModelFacts(Index ix, IxAsset a)
        {
            byte[] data = a.kept;
            var m = UxMesh.ReadModel(data);
            var r = new JObj().Set("boned", m.boned).Set("skinned", m.skinned && !m.boned).Set("static", !m.skinned).Set("nodes", (long)m.hdr[3]).Set("ngroups", (long)m.ngroups);
            if (!m.skinned) r.Set("draw_distance", (long)m.draw_distance).Set("default_materials", StrList(m.materials));
            else if (m.boned)
            {
                // a rigid multi-node model whose node matrices are the identity and whose surface has no skinning can be drawn as a
                // static model (uxmesh.read_model_rigid)
                string file = a.file.Substring(a.file.LastIndexOf('/') + 1);
                string stem = file.LastIndexOf('.') > 0 ? file.Substring(0, file.LastIndexOf('.')) : file;
                var sp = ix.File("surface/" + stem + ".surface");
                List<string> pr; UxModel mr = null;
                try
                {
                    mr = UxMesh.ReadModelRigid(data);
                    pr = sp != null ? UxMesh.RigidStaticProblems(mr, UxMesh.ReadSurface(sp.kept)) : new List<string> { "surface " + stem + ".surface not in extract" };
                }
                catch (Exception ex) when (!(ex is OperationCanceledException)) { pr = new List<string> { "rigid layout: " + ex.Message }; }
                r.Set("rigid_static", pr.Count == 0);
                if (pr.Count != 0) r.Set("rigid_static_problems", StrList(pr));
                else r.Set("draw_distance", (long)mr.draw_distance).Set("default_materials", StrList(mr.materials)).Set("node_names", StrList(mr.node_names));
            }
            return r;
        }
        static List<object> StrList(IEnumerable<string> s) { var l = new List<object>(); foreach (var x in s) l.Add(x); return l; }

        /// surface_facts.
        static JObj SurfaceFacts(byte[] head)
        {
            UxBin.Require(head.Length >= 12, "unpack requires a buffer of 12 bytes");
            if (UxBin.U32BE(head, 0) != UxMesh.SURF_MAGIC) throw new UxException("not an Untamed surface");
            return new JObj().Set("ngroups", (long)UxBin.U32BE(head, 8)).Set("version", (long)UxBin.U32BE(head, 4));
        }
        /// collis_facts.
        static JObj CollisFacts(byte[] head)
        {
            UxBin.Require(head.Length >= 16, "unpack requires a buffer of 16 bytes");
            if (UxBin.U32BE(head, 0) != UxMesh.COLLIS_MAGIC || head[4] != 'c' || head[5] != 'o' || head[6] != 'l' || head[7] != 0) throw new UxException("not an Untamed collis");
            return new JObj().Set("kind", (long)UxBin.U32BE(head, 12)).Set("version", (long)UxBin.U32BE(head, 8));
        }
        /// texture_facts.
        static JObj TextureFacts(byte[] h)
        {
            UxBin.Require(h.Length >= 28, "unpack_from requires a buffer of at least 28 bytes");
            uint magic = UxBin.U32BE(h, 0), nsurf = UxBin.U32BE(h, 12), kind = UxBin.U32BE(h, 20), flags = UxBin.U32BE(h, 24);
            if (magic != UxTex.MAGIC) throw new UxException("not an Untamed texture");
            UxBin.Require(h.Length >= 0x28 + 16, "unpack_from requires a buffer of at least 56 bytes");
            uint mips = UxBin.U32BE(h, 0x28), w = UxBin.U32BE(h, 0x2C), hh = UxBin.U32BE(h, 0x30), fmt = UxBin.U32BE(h, 0x34);
            return new JObj().Set("width", (long)w).Set("height", (long)hh).Set("mips", (long)mips).Set("frames", (long)nsurf).Set("kind", (long)kind).Set("flags", (long)flags)
                .Set("format", UxTex.Xfmt(fmt).name).Set("d3dformat", fmt.ToString("x8", CultureInfo.InvariantCulture));
        }
        /// material_facts.
        static JObj MaterialFacts(byte[] data)
        {
            var m = UxMat.ReadMaterial(data, true);
            var tex = new List<object>(); var seen = new HashSet<string>(StringComparer.Ordinal);
            foreach (var p in m.passes)
                foreach (var t in p.textures)
                    if (!string.IsNullOrEmpty(t.Key) && seen.Add(UxCommon.PyLower(t.Key))) tex.Add(t.Key);
            var p0 = m.passes.Count > 0 ? m.passes[0] : null;
            return new JObj().Set("passes", (long)m.passes.Count).Set("shader", p0?.vs).Set("textures", tex).Set("ntextures", (long)tex.Count)
                .Set("blend", p0 != null ? UxMat.BlendClass(p0.states) : null);
        }

        /// file_facts(typ, path, size): the facts (JObj) or why not (string). A file read from the disc always exists with its index
        /// size, so only 'does not parse' remains of the reference's three failures.
        static object FileFacts(Index ix, IxAsset a)
        {
            try
            {
                switch (a.type)
                {
                    case "model": return ModelFacts(ix, a);
                    case "surface": return SurfaceFacts(a.kept);
                    case "collis": return CollisFacts(a.kept);
                    case "texture": return TextureFacts(a.kept);
                    case "material": return MaterialFacts(a.kept);
                    default: return new JObj();
                }
            }
            catch (Exception ex) when (!(ex is OperationCanceledException)) { return "does not parse: " + ex.Message; }
        }

        // ------------------------------------------------------------------------------------------- closure
        sealed class ClItem
        {
            public string name, file, package, md5; public long size; public IxAsset a;
            public readonly HashSet<string> ref_names = new HashSet<string>(StringComparer.Ordinal), materials = new HashSet<string>(StringComparer.Ordinal);
            public readonly HashSet<long> entities = new HashSet<long>();
            public bool terrain, facts;
            public readonly JObj x = new JObj();           // the header facts and the flags added later (as_female, with_collision ...)
        }
        sealed class Miss { public string type, material, why; public object name; public long? entity; }

        /// Closure: the asset closure keyed by (type, canonical extract name).
        sealed class Closure
        {
            readonly Run _run; readonly Index _ix;
            public readonly Dictionary<string, List<ClItem>> lists = new Dictionary<string, List<ClItem>>(StringComparer.Ordinal);
            readonly Dictionary<string, ClItem> _map = new Dictionary<string, ClItem>(StringComparer.Ordinal);
            public readonly List<Miss> misses = new List<Miss>();
            readonly Dictionary<string, string> _failed = new Dictionary<string, string>(StringComparer.Ordinal);

            public Closure(Run run) { _run = run; _ix = run.ix; foreach (string t in CLOSURE_TYPES) lists[t] = new List<ClItem>(); }
            public ClItem Item(string typ, string cn) => cn != null && _map.TryGetValue(typ + "\n" + cn, out var r) ? r : null;
            public bool Has(string typ, string cn) => Item(typ, cn) != null;

            /// add(typ, name, entity, terrain, material): one reference; the canonical name, or null (a miss is recorded).
            public string Add(string typ, object name, long? entity = null, bool terrain = false, string material = null)
            {
                var a = _ix.Get(name, typ);
                if (a == null) { misses.Add(new Miss { type = typ, name = name, entity = entity, material = material }); return null; }
                string cn = a.name, key = typ + "\n" + cn;
                if (_failed.TryGetValue(key, out string why))
                {
                    misses.Add(new Miss { type = typ, name = name, entity = entity, material = material, why = "file " + a.file + " " + why });
                    return null;
                }
                if (!_map.TryGetValue(key, out var r))
                {
                    r = new ClItem { name = cn, file = a.file, package = a.package, size = a.size, md5 = a.md5, a = a };
                    _map[key] = r; lists[typ].Add(r);
                }
                r.ref_names.Add(PyStr(name));
                if (entity != null) r.entities.Add(entity.Value);
                if (terrain) r.terrain = true;
                if (material != null) r.materials.Add(material);
                return cn;
            }

            /// fill(typ): the header facts of every item not yet filled; an unreadable file becomes a miss.
            public void Fill(string typ)
            {
                var todo = new List<ClItem>();
                foreach (var r in lists[typ]) if (!r.facts) todo.Add(r);
                if (todo.Count == 0) return;
                todo.Sort((p, q) => string.CompareOrdinal(p.name, q.name));       // facts_many: sorted(jobs, key=str)
                var jobs = new List<IxAsset>(todo.Count); foreach (var r in todo) jobs.Add(r.a);
                _run.FactsMany(jobs);
                foreach (var r in todo)
                {
                    object f = _run.facts[r.a.index];
                    if (f is JObj fo) { foreach (var kv in fo.items) r.x.Set(kv.Key, kv.Value); r.facts = true; continue; }
                    string why = (string)f, key = typ + "\n" + r.name;
                    _failed[key] = why;
                    long? first = null; foreach (long id in r.entities) if (first == null || id < first) first = id;
                    misses.Add(new Miss { type = typ, name = r.name, entity = first, why = "file " + r.file + " " + why });
                    _map.Remove(key); lists[typ].Remove(r);
                }
            }

            /// to_json().
            public JObj ToJson()
            {
                var o = new JObj();
                foreach (string t in CLOSURE_TYPES)
                {
                    var d = new JObj();
                    foreach (var r in lists[t])
                    {
                        var j = new JObj();
                        j.items.Add(new KeyValuePair<string, object>("name", r.name));
                        j.items.Add(new KeyValuePair<string, object>("ref_names", SortedStrings(r.ref_names)));
                        j.items.Add(new KeyValuePair<string, object>("file", r.file));
                        j.items.Add(new KeyValuePair<string, object>("package", r.package));
                        j.items.Add(new KeyValuePair<string, object>("size", r.size));
                        j.items.Add(new KeyValuePair<string, object>("md5", r.md5));
                        j.items.Add(new KeyValuePair<string, object>("entities", SortedLongs(r.entities)));
                        j.items.Add(new KeyValuePair<string, object>("terrain", r.terrain));
                        if (t == "texture") j.items.Add(new KeyValuePair<string, object>("materials", SortedStrings(r.materials)));
                        foreach (var kv in r.x.items) j.Set(kv.Key, kv.Value);
                        d.items.Add(new KeyValuePair<string, object>(r.name, j));
                    }
                    o.items.Add(new KeyValuePair<string, object>(t, d));
                }
                return o;
            }
        }
        static List<object> SortedStrings(IEnumerable<string> s)
        {
            var l = new List<string>(s); l.Sort(string.CompareOrdinal);
            var o = new List<object>(l.Count); foreach (var x in l) o.Add(x); return o;
        }
        static List<object> SortedLongs(IEnumerable<long> s)
        {
            var l = new List<long>(s); l.Sort();
            var o = new List<object>(l.Count); foreach (var x in l) o.Add(x); return o;
        }

        // ------------------------------------------------------------------------------------------- placed surfaces
        /// What sky_dome_facts and placed_top_y measure of a surface placed by an entity matrix (row vectors, Untamed frame).
        internal struct Placed { public double rad, el95, top; public bool hasElev; }

        /// W = P @ M[:3, :3] + M[3, :3] in float64 (the reference's dgemm rounds each element as the fused chain k = 0, 1, 2), then:
        /// rad = max horizontal radius, el95 = the 95th percentile of the elevation angle from the origin (degrees; computed when
        /// wanted), top = the highest y of the finite rows (-inf when there is none). No vertex: rad 0, el95 0, top -inf.
        internal static Placed PlaceSurface(double[] P, double[] M, bool elevation, bool libraryAtan2 = false)
        {
            int n = P.Length / 3;
            var r = new Placed { rad = 0.0, el95 = 0.0, top = double.NegativeInfinity, hasElev = true };
            if (n == 0) return r;
            double m00 = M[0], m01 = M[1], m02 = M[2], m10 = M[4], m11 = M[5], m12 = M[6], m20 = M[8], m21 = M[9], m22 = M[10], tx = M[12], ty = M[13], tz = M[14];
            var wy = new double[n]; var rr = new double[n];
            double rad = double.NegativeInfinity, top = double.NegativeInfinity; bool radNan = false;
            for (int i = 0; i < n; i++)
            {
                double p0 = P[3 * i], p1 = P[3 * i + 1], p2 = P[3 * i + 2];
                double x = UxNp.Fma(p2, m20, UxNp.Fma(p1, m10, p0 * m00)) + tx;
                double y = UxNp.Fma(p2, m21, UxNp.Fma(p1, m11, p0 * m01)) + ty;
                double z = UxNp.Fma(p2, m22, UxNp.Fma(p1, m12, p0 * m02)) + tz;
                double h = Math.Sqrt(x * x + z * z);
                wy[i] = y; rr[i] = h;
                if (double.IsNaN(h)) radNan = true; else if (h > rad) rad = h;
                if (UxNp.Finite(x) && UxNp.Finite(y) && UxNp.Finite(z) && y > top) top = y;
            }
            r.rad = radNan ? UxNp.NaN : rad; r.top = top;
            if (!elevation) { r.hasElev = false; return r; }
            // elev = degrees(arctan2(W.y, max(r, 1e-6))); np.percentile(elev, 95), method 'linear'
            var elev = new double[n]; bool nan = false;
            for (int i = 0; i < n; i++)
            {
                double h = rr[i]; if (!(h >= 1e-6) && !double.IsNaN(h)) h = 1e-6;
                double v = (libraryAtan2 ? Math.Atan2(wy[i], h) : Atan2(wy[i], h)) * RAD2DEG;
                elev[i] = v; if (double.IsNaN(v)) nan = true;
            }
            if (nan) { r.el95 = UxNp.NaN; return r; }
            Array.Sort(elev);
            double virt = (n - 1) * (95.0 / 100.0);
            double prevIndex = Math.Floor(virt);
            double a, b, gamma;
            if (virt >= n - 1) { a = b = elev[n - 1]; gamma = virt - (-1.0); }
            else { a = elev[(int)prevIndex]; b = elev[(int)prevIndex + 1]; gamma = virt - prevIndex; }
            double diff = b - a;
            r.el95 = gamma >= 0.5 ? b - diff * (1 - gamma) : a + diff * gamma;
            return r;
        }

        // ------------------------------------------------------------------------------------------- one level, shared by its kinds
        sealed class Rec
        {
            public UxEntity e; public long id; public string type, sub; public object name;
            public string cls, reflex, reason, rule, policy_cls;
            public bool collision;
            public JObj model, sky_dome, underworld;
            public List<object> refs;
        }

        sealed class Run
        {
            public UxDiscLevel disc; public UxLevelRow lv; public UxLevelOptions opt; public CancellationToken cancel; public ParallelOptions po;
            public List<UxXSubScene> subs; public List<KeyValuePair<string, UxXSubScene>> byShort; public string champ;
            public UxXSubScene baseSub, chSub;
            public List<(object id, string sub, UxEntity entity)> merged;
            public Dictionary<long, List<object>> present = new Dictionary<long, List<object>>();
            public List<string> sceneWarnings = new List<string>();
            public JObj frStart; public UxEntity frEnt;
            public string sceneMd5, indexMd5;
            public Index ix; public object[] facts;       // per asset index: JObj, or the reason it does not parse
            public double? floorY; bool _floorDone;
            readonly ConcurrentDictionary<int, double[]> _positions = new ConcurrentDictionary<int, double[]>();
            readonly ConcurrentDictionary<int, Placed> _placed = new ConcurrentDictionary<int, Placed>();
            public readonly Cleaner cleaner = new Cleaner();

            /// facts_many: the header facts of the jobs not read yet, in parallel (the result does not depend on the thread count).
            public void FactsMany(List<IxAsset> jobs)
            {
                var todo = new List<IxAsset>(); var seen = new HashSet<int>();
                foreach (var a in jobs) if (facts[a.index] == null && seen.Add(a.index)) todo.Add(a);
                if (todo.Count == 0) return;
                if (todo.Count == 1) { facts[todo[0].index] = FileFacts(ix, todo[0]); return; }
                Parallel.For(0, todo.Count, po, i => { facts[todo[i].index] = FileFacts(ix, todo[i]); });
            }

            /// The vertex positions of a surface as float64 (n, 3): the groups' positions concatenated.
            public double[] Positions(IxAsset a) => _positions.GetOrAdd(a.index, _ =>
            {
                var gs = UxMesh.ReadSurface(a.kept);
                int n = 0; foreach (var g in gs) if (g.pos != null && g.pos.Length > 0) n += g.pos.Length;
                var p = new double[n]; int at = 0;
                foreach (var g in gs) if (g.pos != null && g.pos.Length > 0) { for (int i = 0; i < g.pos.Length; i++) p[at + i] = g.pos[i]; at += g.pos.Length; }
                return p;
            });

            /// sky_dome_facts and placed_top_y of a merged entity's surface. The elevation percentile only decides anything beyond the
            /// sky radius, so it is computed there (and always when `always`: the numerics test).
            public Placed PlacedOf(int mergedIndex, IxAsset surface, bool always = false, bool libraryAtan2 = false)
            {
                if (!always && _placed.TryGetValue(mergedIndex, out var have)) return have;
                var e = merged[mergedIndex].entity;
                double[] m = e.Matrix;
                if (m == null) throw new UxException(disc.name + ": entity " + PyStr(e.id) + " " + PyStr(e.NameValue) + " has no 4 x 4 Matrix");
                double[] P = Positions(surface);
                var r = PlaceSurface(P, m, always, libraryAtan2);
                if (!always && r.rad > SKY_MIN_RADIUS_FT) r = PlaceSurface(P, m, true);
                if (!always) _placed[mergedIndex] = r;
                return r;
            }

            /// terrain_floor: the lowest trncol vertex y (Untamed frame) of the level's TerrainRSData, or null.
            public double? TerrainFloor()
            {
                if (_floorDone) return floorY;
                _floorDone = true; floorY = null;
                foreach (var me in merged)
                {
                    var e = me.entity;
                    if (e.type != "TerrainRSData") continue;
                    var a = e.props.Get("TerrainCollisionAsset") is JObj tc ? ix.Get(tc.Get("asset"), "trncol") : null;
                    if (a == null) continue;
                    var c = UxTerrain.ReadTrncol(a.kept);
                    if (c.nverts == 0) return floorY;
                    double min = double.PositiveInfinity; bool nan = false;
                    for (int i = 0; i < c.nverts; i++) { double y = c.verts[3 * i + 1]; if (double.IsNaN(y)) nan = true; else if (y < min) min = y; }
                    floorY = nan ? UxNp.NaN : min;
                    return floorY;
                }
                return floorY;
            }
        }

        /// The scene part of build_level that does not depend on the kind: the sub-scenes, the merge, the free-ride start pose.
        static Run OpenScene(UxDiscLevel disc, UxLevelOptions opt, CancellationToken cancel)
        {
            var lv = disc.lv;
            var run = new Run { disc = disc, lv = lv, opt = opt, cancel = cancel };
            run.po = new ParallelOptions { MaxDegreeOfParallelism = opt.threads > 0 ? opt.threads : Math.Max(1, Environment.ProcessorCount), CancellationToken = cancel };
            byte[] scene = disc.ReadFile("scene", cancel);
            run.sceneMd5 = UxBin.Md5(scene);
            run.subs = UxXScene.Load(scene);
            run.byShort = new List<KeyValuePair<string, UxXSubScene>>();
            foreach (var s in run.subs)
            {
                string sh = s.Short(lv.disc);
                int at = run.byShort.FindIndex(kv => kv.Key == sh);
                if (at >= 0) run.byShort[at] = new KeyValuePair<string, UxXSubScene>(sh, s); else run.byShort.Add(new KeyValuePair<string, UxXSubScene>(sh, s));
            }
            run.champ = UxCommon.ChampionshipSubscene(lv.mode);
            foreach (string need in new[] { "Base", run.champ })
                if (run.byShort.FindIndex(kv => kv.Key == need) < 0)
                {
                    var have = new List<object>(); foreach (var kv in run.byShort) have.Add(kv.Key);
                    throw new UxException(lv.disc + ": sub-scene " + lv.disc + need + " not found (have " + PyRepr(have) + ")");
                }
            run.baseSub = run.byShort.Find(kv => kv.Key == "Base").Value;
            run.chSub = run.byShort.Find(kv => kv.Key == run.champ).Value;

            // (a) merge: Base first, first occurrence wins
            run.merged = UxXScene.MergedEntities(new List<UxXSubScene> { run.baseSub, run.chSub }, lv.disc);
            foreach (var s in new[] { run.baseSub, run.chSub })
            {
                var count = new Dictionary<long, int>(); var order = new List<long>();
                foreach (var e in s.Entities()) { long id = IdOf(e, lv); if (count.TryGetValue(id, out int c)) count[id] = c + 1; else { count[id] = 1; order.Add(id); } }
                var dupl = new List<object>(); foreach (long id in order) if (count[id] > 1) dupl.Add(id);
                if (dupl.Count > 0) run.sceneWarnings.Add(s.name + ": " + dupl.Count + " entity ids repeat inside the sub-scene: " + PyRepr(dupl.GetRange(0, Math.Min(10, dupl.Count))));
                string sh = s.Short(lv.disc);
                foreach (var e in s.Entities())
                {
                    long id = IdOf(e, lv);
                    if (!run.present.TryGetValue(id, out var l)) { l = new List<object>(); run.present[id] = l; }
                    if (!l.Contains(sh)) l.Add(sh);
                }
            }
            // first-wins conflicts (same id, different props)
            var baseById = new Dictionary<long, UxEntity>();
            foreach (var e in run.baseSub.Entities()) baseById[IdOf(e, lv)] = e;
            var conflicts = new List<object>();
            foreach (var e in run.chSub.Entities())
                if (baseById.TryGetValue(IdOf(e, lv), out var b) && (b.type != e.type || !SameProps(b.props, e.props))) conflicts.Add(IdOf(e, lv));
            if (conflicts.Count > 0)
                run.sceneWarnings.Add(conflicts.Count + " ids differ between Base and " + run.champ + " (Base copy kept): " + PyRepr(conflicts.GetRange(0, Math.Min(20, conflicts.Count))));

            // free-ride start pose: the race merge, then FreeRide, then the other sub-scenes
            var search = new List<(string sh, UxXSubScene sub, bool inMerge)> { ("Base", run.baseSub, true), (run.champ, run.chSub, true) };
            foreach (var kv in run.byShort) if (kv.Key == "FreeRide") search.Add(("FreeRide", kv.Value, false));
            foreach (var kv in run.byShort) if (kv.Key != "Base" && kv.Key != run.champ && kv.Key != "FreeRide") search.Add((kv.Key, kv.Value, false));
            FindFreerideStart(search, lv, out run.frStart, out run.frEnt);
            if (run.frStart != null)
                run.frStart.Set("synth", new JObj().Set("count", 12L).Set("spacing_ft", 3.0).Set("along", "-x_axis").Set("height", "HF + 1 ft").Set("index_from", 1L)
                    .Set("note", "DESIGN 5.8: 12 StartEntityFreeRideRSData in <L>FreeRide, same yaw"));
            return run;
        }

        static long IdOf(UxEntity e, UxLevelRow lv)
        {
            if (e.id is long l) return l;
            throw new UxException(lv.disc + ": an entity (" + e.type + ") without an entityId");
        }

        /// find_freeride_start: the line first, then a free-ride start (lowest Index, then lowest id); pose_record of it.
        static void FindFreerideStart(List<(string sh, UxXSubScene sub, bool inMerge)> order, UxLevelRow lv, out JObj record, out UxEntity entity)
        {
            foreach (string typ in new[] { "StartEntityFreeRideLineRSData", "StartEntityFreeRideRSData" })
                foreach (var (sh, sub, inMerge) in order)
                {
                    UxEntity best = null; long bestIndex = 0, bestId = 0;
                    foreach (var e in sub.Entities())
                    {
                        if (e.type != typ || e.Matrix == null) continue;
                        long idx = e.props.Get("Index") is long li ? li : 1L << 30, id = IdOf(e, lv);
                        if (best == null || idx < bestIndex || idx == bestIndex && id < bestId) { best = e; bestIndex = idx; bestId = id; }
                    }
                    if (best == null) continue;
                    double[] m = best.Matrix;
                    var rec = new JObj().Set("type", best.type).Set("id", best.id).Set("name", best.NameValue).Set("subscene", sh).Set("in_race_merge", inMerge)
                        .Set("index", best.props.Get("Index")).Set("matrix", m);
                    rec.Set("position", new[] { m[12], m[13], m[14] }).Set("x_axis", new[] { m[0], m[1], m[2] }).Set("y_axis", new[] { m[4], m[5], m[6] })
                        .Set("z_axis", new[] { m[8], m[9], m[10] }).Set("yaw_deg", Atan2(m[8], m[10]) * RAD2DEG);
                    record = rec; entity = best;
                    return;
                }
            record = null; entity = null;
        }

        /// One pass over every asset of the level: size and md5 (the rows of extract/<Level>/index.json), and the bytes the header facts
        /// need. The package is read front to back by the calling thread (one sequential pass: a disc image or a hard disk does not
        /// seek), the resources are decoded and hashed by the workers.
        static void ReadAssets(Run run, Action<float, string> progress)
        {
            var disc = run.disc; int n = disc.assets.Count;
            var rows = new IxAsset[n];
            var order = new int[n]; for (int i = 0; i < n; i++) order[i] = i;
            Array.Sort(order, (p, q) => { int c = disc.assets[p].AbsOffset.CompareTo(disc.assets[q].AbsOffset); return c != 0 ? c : p.CompareTo(q); });
            long total = 0; foreach (var a in disc.assets) total += a.size;
            long done = 0; int reported = -1; object gate = new object();
            int workers = Math.Max(1, run.po.MaxDegreeOfParallelism);
            Exception failure = null;
            using (var stop = CancellationTokenSource.CreateLinkedTokenSource(run.cancel))
            using (var queue = new BlockingCollection<KeyValuePair<int, byte[]>>(Math.Max(8, 4 * workers)))
            {
                var tasks = new Task[workers];
                for (int w = 0; w < workers; w++)
                    tasks[w] = Task.Run(() =>
                    {
                        try
                        {
                            foreach (var item in queue.GetConsumingEnumerable(stop.Token))
                            {
                                var a = disc.assets[item.Key];
                                byte[] d = a.compressed ? UxLzx.Decode(item.Value, stop.Token) : item.Value;
                                var row = new IxAsset { index = item.Key, asset = a, name = a.name, type = a.type, package = a.package, file = a.file, size = d.Length, md5 = UxBin.Md5(d) };
                                switch (a.type)
                                {
                                    case "model": case "material": case "collis": case "surface": case "trncol": row.kept = d; break;
                                    case "texture": row.kept = UxBin.PySlice(d, 0, TEXTURE_HEAD); break;
                                }
                                rows[item.Key] = row;
                                if (progress != null)
                                {
                                    long now = Interlocked.Add(ref done, a.size);
                                    int pct = total > 0 ? (int)(now * 100 / total) : 100;
                                    lock (gate) if (pct > reported) { reported = pct; progress(0.05f + 0.8f * pct / 100f, "reading the level's assets"); }
                                }
                            }
                        }
                        catch (OperationCanceledException) { }
                        catch (Exception ex) { Interlocked.CompareExchange(ref failure, ex, null); stop.Cancel(); }
                    });
                try
                {
                    foreach (int i in order) queue.Add(new KeyValuePair<int, byte[]>(i, disc.Blob(disc.assets[i], stop.Token)), stop.Token);
                }
                catch (OperationCanceledException) { }
                catch (Exception ex) { Interlocked.CompareExchange(ref failure, ex, null); stop.Cancel(); }
                finally { queue.CompleteAdding(); }
                Task.WaitAll(tasks);
            }
            if (failure != null) System.Runtime.ExceptionServices.ExceptionDispatchInfo.Capture(failure).Throw();
            run.cancel.ThrowIfCancellationRequested();
            run.ix = new Index();
            var list = new List<object>(n);
            foreach (var row in rows)
            {
                run.ix.Add(row);
                var a = row.asset;
                list.Add(new JObj().Set("name", a.name).Set("type", a.type).Set("package", a.package).Set("offset", a.AbsOffset).Set("stored_size", a.size)
                    .Set("compressed", a.compressed).Set("file", a.file.Replace('/', '\\')).Set("size", row.size).Set("md5", row.md5));
            }
            // index.json as uxpkg.extract writes it: json.dumps(indent=1) through a text-mode file
            string text = UxJson.Dumps(new JObj().Set("level", disc.name).Set("packages", disc.packages).Set("assets", list), 1);
            if (run.opt.extractIndexCrlf) text = text.Replace("\n", "\r\n");
            run.indexMd5 = UxBin.Md5(new UTF8Encoding(false).GetBytes(text));
            run.facts = new object[n];
        }

        // ------------------------------------------------------------------------------------------- main builder
        /// build_level(level, kind, rigid_models): the content of level.json / level_fr.json (JSON-ready, cleaned).
        static JObj BuildLevel(Run run, string kind)
        {
            var lv = run.lv; var ix = run.ix; string disc = lv.disc, rigid = run.opt.rigidModels;
            var problems = new List<object>(); var warnings = new List<object>();
            foreach (string w in run.sceneWarnings) warnings.Add(w);
            if (run.frStart == null) problems.Add("no StartEntityFreeRideLineRSData or StartEntityFreeRideRSData in any sub-scene");
            var merged = run.merged;
            var cl = new Closure(run);

            // model facts of every entity the policy would keep or demote (needed for the skinned check), and the placed surfaces of
            // the kept ModelRSData entities (sky dome and underworld rules), both in parallel
            var pre = new List<IxAsset>(); var placeJobs = new List<(int index, IxAsset surface)>();
            for (int i = 0; i < merged.Count; i++)
            {
                var e = merged[i].entity;
                string cls0 = Classify(e.type, e.NameValue, kind).cls;
                if ((cls0 == "keep" || cls0 == "demote") && e.props.Get("ModelAsset") is JObj ma && Truthy(ma.Get("asset")))
                {
                    var a = ix.Get(ma.Get("asset"), "model");
                    if (a != null) pre.Add(a);
                }
                if (cls0 == "keep" && e.type == "ModelRSData" && e.Matrix != null && e.props.Get("SurfaceAsset") is JObj sa)
                {
                    var a = ix.Get(sa.Get("asset"), "surface");
                    if (a != null) placeJobs.Add((i, a));
                }
            }
            pre.Sort((p, q) => string.CompareOrdinal(p.name, q.name));
            run.FactsMany(pre);
            Parallel.For(0, placeJobs.Count, run.po, i => { run.PlacedOf(placeJobs[i].index, placeJobs[i].surface); });
            double? floorY = run.TerrainFloor();      // for the '+underworld' rule

            // (b) classify
            var entities = new List<Rec>(merged.Count);
            var unknown = new Dictionary<string, int>(StringComparer.Ordinal);
            var unconverted = new List<(string name, JObj facts, List<long> entities, HashSet<string> types)>();
            var replaced = new Dictionary<string, long>(StringComparer.Ordinal);      // rule -> first entity id
            long? frId = run.frEnt != null ? (long?)IdOf(run.frEnt, lv) : null;
            for (int mi = 0; mi < merged.Count; mi++)
            {
                run.cancel.ThrowIfCancellationRequested();
                var e = merged[mi].entity; long eid = IdOf(e, lv);
                var (cls, reflex, reason, rule) = Classify(e.type, e.NameValue, kind);
                if (cls == null) { unknown[e.type] = (unknown.TryGetValue(e.type, out int u) ? u : 0) + 1; cls = "drop"; reflex = null; }
                if (cls == "replace")
                {
                    if (replaced.TryGetValue(rule, out long first))
                    {
                        cls = "drop"; reflex = null;
                        reason = "another " + e.type + " (" + PyStr(e.NameValue) + ") replaces the single template entity; first in merge order (id " + I(first) + ") used";
                        warnings.Add(I(eid) + " " + e.type + " " + PyStr(e.NameValue) + " dropped: " + reason);
                    }
                    else replaced[rule] = eid;
                }
                if (e.type == "StartEntityFreeRideLineRSData" || e.type == "StartEntityFreeRideRSData")
                {
                    if (frId != null && eid == frId)
                    {
                        cls = "synth"; reflex = FREERIDE_LINE_REFLEX;
                        reason = "pose source of the 12 synthesised free-ride starts";
                        rule = "freeride_start";
                    }
                    else if (e.type == "StartEntityFreeRideLineRSData") { cls = "drop"; reflex = null; reason = "a second free-ride line (not the pose source)"; rule = "freeride_start"; }
                    else { cls = "drop"; reflex = null; }
                }
                object mnameObj = e.props.Get("ModelAsset") is JObj maj ? maj.Get("asset") : null;
                var refs = new List<object>();
                foreach (var (prop, name, type) in e.Assets()) refs.Add(new List<object> { prop, name, type });
                var rec = new Rec
                {
                    e = e, id = eid, type = e.type, name = e.NameValue, sub = merged[mi].sub, refs = refs,
                    cls = cls, reflex = reflex, reason = reason, rule = rule, collision = e.props.Get("FemaleCollisionAsset") is JObj,
                };
                JObj info = null; IxAsset modelAsset = null;
                if (Truthy(mnameObj) && (cls == "keep" || cls == "demote"))
                {
                    modelAsset = ix.Get(mnameObj, "model");
                    if (modelAsset != null) info = run.facts[modelAsset.index] as JObj;       // null: not read or does not parse
                }
                if (info != null)
                {
                    string mname = PyStr(mnameObj);
                    bool isStatic = (bool)info.Get("static"), skinned = (bool)info.Get("skinned"), boned = (bool)info.Get("boned"); long nodes = (long)info.Get("nodes");
                    rec.model = new JObj().Set("name", mnameObj).Set("static", isStatic).Set("skinned", skinned).Set("boned", boned).Set("nodes", nodes);
                    if (info.Get("rigid_static") is bool rs && rs && rigid == "static")
                    {
                        rec.model.Set("rigid_static", true);
                        rec.reason = (!string.IsNullOrEmpty(rec.reason) ? rec.reason + "; " : "") + "rigid multi-node model " + mname + " (" + I(nodes) + " nodes " +
                            PyStr(info.Get("node_names")) + ", identity node matrices, unskinned surface): drawn as a static model (--rigid-models static)";
                    }
                    else if (!isStatic)
                    {
                        string what = boned ? "rigid multi-node model " + mname + " (" + I(nodes) + " nodes)" : "skinned model " + mname;
                        rec.reason = what + ": not converted (DESIGN 5.4); policy was " + cls + " (" + PyStr(reason) + ")";
                        rec.rule = (rule ?? "") + "+model";
                        rec.policy_cls = cls;
                        rec.cls = "drop"; rec.reflex = null;
                        int at = unconverted.FindIndex(x => x.name == modelAsset.name);
                        if (at < 0) { unconverted.Add((modelAsset.name, new JObj().Set("skinned", skinned).Set("boned", boned).Set("nodes", nodes), new List<long>(), new HashSet<string>(StringComparer.Ordinal))); at = unconverted.Count - 1; }
                        unconverted[at].entities.Add(eid); unconverted[at].types.Add(e.type);
                    }
                }
                if (rec.cls == "keep" && e.type == "ModelRSData")
                {
                    var a = e.props.Get("SurfaceAsset") is JObj sa ? ix.Get(sa.Get("asset"), "surface") : null;
                    if (a != null)
                    {
                        var pf = run.PlacedOf(mi, a);
                        if (pf.rad > SKY_MIN_RADIUS_FT && pf.el95 > SKY_MIN_ELEV_DEG)
                        {
                            rec.policy_cls = rec.cls;
                            rec.cls = "drop"; rec.reflex = null;
                            rec.rule = (rec.rule ?? "") + "+sky";
                            rec.reason = "sky dome (surface " + a.name + " reaches " + Fixed(pf.rad, 0) + " ft, 95% of its vertices above " + Fixed(pf.el95, 0) +
                                " deg elevation): as a ModelRSData on Template_Diffuse it would be a lit, fogged prop beyond the fog end (stock domes <= 2,000 ft on " +
                                "sky shaders); the template SkyRSData draws the sky";
                            rec.sky_dome = new JObj().Set("radius_ft", UxNp.PyRound(pf.rad, 1)).Set("elevation_p95_deg", UxNp.PyRound(pf.el95, 2));
                        }
                    }
                }
                if (rec.cls == "keep" && e.type == "ModelRSData" && !rec.collision && floorY != null)
                {
                    var a = e.props.Get("SurfaceAsset") is JObj sa ? ix.Get(sa.Get("asset"), "surface") : null;
                    if (a != null)
                    {
                        double top = run.PlacedOf(mi, a).top, floor = floorY.Value;
                        if (top < floor - UNDERWORLD_FT)
                        {
                            rec.policy_cls = rec.cls;
                            rec.cls = "drop"; rec.reflex = null;
                            rec.rule = (rec.rule ?? "") + "+underworld";
                            rec.reason = "underworld staging model: every vertex of surface " + a.name + " lies below " + Fixed(top, 1) + " ft, more than " + Fixed(UNDERWORLD_FT, 0) +
                                " ft under the lowest terrain point (" + Fixed(floor, 2) + " ft); seen only by the Untamed jumbotron/intro cameras (dropped), it would sit " +
                                "under the HF floor (DESIGN 16 below-floor fix)";
                            rec.underworld = new JObj().Set("top_y_ft", UxNp.PyRound(top, 2)).Set("terrain_floor_ft", UxNp.PyRound(floor, 3));
                        }
                    }
                }
                if (rec.cls == "demote" && !rec.collision)
                {
                    rec.reflex = "ModelRSData";
                    rec.reason += " (no collis on the entity: static ModelRSData)";
                }
                entities.Add(rec);
            }
            var unknownTypes = new List<string>(unknown.Keys); unknownTypes.Sort(string.CompareOrdinal);
            foreach (string t in unknownTypes) problems.Add("entity type " + t + " (" + unknown[t] + ") is not in the policy table");

            // (c) closure of keep/demote entities: register refs, then read the header facts in parallel
            var matNames = new Dictionary<long, List<string>>();
            var closureProps = new[] { ("ModelAsset", "model"), ("SurfaceAsset", "surface"), ("FemaleCollisionAsset", "collis"), ("MaleCollisionAsset", "collis") };
            foreach (var rec in entities)
            {
                if (rec.cls != "keep" && rec.cls != "demote") continue;
                var p = rec.e.props; long eid = rec.id;
                foreach (var (prop, typ) in closureProps)
                {
                    if (p.Get(prop) is JObj v && Truthy(v.Get("asset")))
                    {
                        if (!(v.Get("type") is string vt) || vt != typ)
                            problems.Add("entity " + I(eid) + " " + PyStr(rec.name) + ": " + prop + " has type " + PyRepr(v.Get("type")) + ", expected " + PyRepr(typ));
                        string cn = cl.Add(typ, v.Get("asset"), eid);
                        if (cn != null && typ == "collis") cl.Item("collis", cn).x.Set(prop.StartsWith("Female", StringComparison.Ordinal) ? "as_female" : "as_male", true);
                    }
                    else if (rec.type != "TriggerRSData" && (prop == "ModelAsset" || prop == "SurfaceAsset"))
                        warnings.Add("entity " + I(eid) + " " + PyStr(rec.name) + " (" + rec.type + "): no " + prop);
                }
                object ml = p.Get("MaterialList");
                if (ml != null)
                {
                    var names = new List<string>();
                    foreach (object it in ml is List<object> mll ? mll : new List<object> { ml })
                    {
                        if (it is JObj d && Truthy(d.Get("asset"))) names.Add(cl.Add("material", d.Get("asset"), eid) ?? PyStr(d.Get("asset")));
                        else
                        {
                            warnings.Add("entity " + I(eid) + " " + PyStr(rec.name) + ": empty MaterialList entry " + PyRepr(it));
                            names.Add(null);
                        }
                    }
                    matNames[eid] = names;
                }
            }

            // terrain (TerrainRSData is 'replace', but its assets feed S6)
            JObj terrain = null; var terrainMaterials = new List<object>();
            var trs = entities.FindAll(r => r.type == "TerrainRSData");
            if (trs.Count != 1) problems.Add(trs.Count + " TerrainRSData entities (expected 1)");
            if (trs.Count > 0)
            {
                var p = trs[0].e.props; long tid = trs[0].id;
                object Aname(string k) => p.Get(k) is JObj v ? v.Get("asset") : null;
                terrain = new JObj().Set("entity_id", tid).Set("terrain", null).Set("trncol", null).Set("materials", terrainMaterials).Set("static_shadow_map", null)
                    .Set("textures", new List<object>());
                if (Truthy(Aname("TerrainAsset"))) terrain.Set("terrain", cl.Add("terrain", Aname("TerrainAsset"), tid, true));
                if (Truthy(Aname("TerrainCollisionAsset"))) terrain.Set("trncol", cl.Add("trncol", Aname("TerrainCollisionAsset"), tid, true));
                if (p.Get("MaterialList") is List<object> tml)
                    foreach (object it in tml)
                        if (it is JObj d && Truthy(d.Get("asset"))) terrainMaterials.Add(cl.Add("material", d.Get("asset"), tid, true) ?? PyStr(d.Get("asset")));
                if (Truthy(Aname("StaticShadowMapAsset"))) terrain.Set("static_shadow_map", cl.Add("texture", Aname("StaticShadowMapAsset"), tid, true));
                foreach (string k in new[] { "terrain", "trncol" })
                    if (terrain.Get(k) == null) problems.Add("TerrainRSData " + I(tid) + ": no " + k + " asset");
            }

            foreach (string typ in new[] { "model", "surface", "collis", "material", "terrain", "trncol" }) cl.Fill(typ);

            // group-count checks (DESIGN 5.11 V4: model groups = surface groups = MaterialList length), instancing facts
            var matLists = new Dictionary<string, HashSet<string>>(StringComparer.Ordinal);
            foreach (var rec in entities)
            {
                if (!matNames.TryGetValue(rec.id, out var names)) continue;
                foreach (var (prop, typ) in new[] { ("ModelAsset", "model"), ("SurfaceAsset", "surface") })
                {
                    var a = rec.e.props.Get(prop) is JObj v ? ix.Get(v.Get("asset"), typ) : null;
                    var it = a != null ? cl.Item(typ, a.name) : null;
                    if (it == null) continue;
                    if (typ == "model")
                    {
                        if (!matLists.TryGetValue(it.name, out var set)) { set = new HashSet<string>(StringComparer.Ordinal); matLists[it.name] = set; }
                        var key = new StringBuilder(); foreach (string nm in names) key.Append(nm ?? "").Append('\n');
                        set.Add(key.ToString());
                    }
                    if (it.x.Get("ngroups") is long ng && names.Count != ng)
                        warnings.Add("entity " + I(rec.id) + " " + PyStr(rec.name) + ": MaterialList " + names.Count + " entries, " + typ + " " + it.name + " has " + I(ng) + " groups");
                }
            }
            foreach (var rec in entities)
                if ((rec.cls == "keep" || rec.cls == "demote") && rec.collision)
                {
                    var a = rec.e.props.Get("ModelAsset") is JObj m ? ix.Get(m.Get("asset"), "model") : null;
                    if (a != null) cl.Item("model", a.name)?.x.Set("with_collision", true);
                }
            foreach (var r in cl.lists["model"])
            {
                r.x.Set("material_lists", (long)(matLists.TryGetValue(r.name, out var set) ? set.Count : 0));
                if (!r.x.Has("with_collision")) r.x.Set("with_collision", false);
            }
            foreach (var r in cl.lists["collis"])
            {
                if (!r.x.Has("as_female")) r.x.Set("as_female", false);
                if (!r.x.Has("as_male")) r.x.Set("as_male", false);
            }

            // textures of every closure material
            var terrainMats = new HashSet<string>(StringComparer.Ordinal);
            foreach (object m in terrainMaterials) if (m is string ms && ms.Length > 0) terrainMats.Add(ms);
            var mats = new List<ClItem>(cl.lists["material"]); mats.Sort((p, q) => string.CompareOrdinal(p.name, q.name));
            foreach (var mr in mats)
            {
                var resolved = new List<object>();
                if (mr.x.Get("textures") is List<object> tl)
                    foreach (object t in tl) resolved.Add(cl.Add("texture", t, null, terrainMats.Contains(mr.name), mr.name) ?? t);
                mr.x.Set("textures", resolved);
            }
            cl.Fill("texture");
            if (terrain != null)
            {
                var tt = new HashSet<string>(StringComparer.Ordinal);
                foreach (string mn in terrainMats)
                {
                    var it = cl.Item("material", mn);
                    if (it != null && it.x.Get("textures") is List<object> tl)
                        foreach (object t in tl) if (t is string ts && cl.Has("texture", ts)) tt.Add(ts);
                }
                if (terrain.Get("static_shadow_map") is string ssm && ssm.Length > 0 && cl.Has("texture", ssm)) tt.Add(ssm);
                terrain.Set("textures", SortedStrings(tt));
            }

            // refs of closure entities that are not converted (PhysicsAsset bxml ...)
            var other = new List<(string type, List<(string name, int[] count, bool[] resolved, HashSet<string> props)> names)>();
            foreach (var rec in entities)
            {
                if (rec.cls != "keep" && rec.cls != "demote") continue;
                foreach (List<object> rf in rec.refs)
                {
                    string prop = (string)rf[0]; object name = rf[1]; string typ = rf[2] as string;
                    string head = prop.IndexOf('.') >= 0 ? prop.Substring(0, prop.IndexOf('.')) : prop;
                    if (head == "ModelAsset" || head == "SurfaceAsset" || head == "FemaleCollisionAsset" || head == "MaleCollisionAsset" || head == "MaterialList") continue;
                    var a = ix.Get(name, typ);
                    string tkey = PyStr(rf[2]), nkey = a != null ? a.name : PyStr(name);
                    int ti = other.FindIndex(x => x.type == tkey);
                    if (ti < 0) { other.Add((tkey, new List<(string, int[], bool[], HashSet<string>)>())); ti = other.Count - 1; }
                    int ni = other[ti].names.FindIndex(x => x.name == nkey);
                    if (ni < 0) { other[ti].names.Add((nkey, new int[1], new bool[1], new HashSet<string>(StringComparer.Ordinal))); ni = other[ti].names.Count - 1; }
                    var o = other[ti].names[ni];
                    o.count[0]++; o.resolved[0] = a != null; o.props.Add(prop);
                }
            }
            var otherJson = new JObj();
            foreach (var (type, names) in other)
            {
                var d = new JObj();
                foreach (var (name, count, resolved, props) in names)
                    d.Set(name, new JObj().Set("count", (long)count[0]).Set("resolved", resolved[0]).Set("props", SortedStrings(props)));
                otherJson.Set(type, d);
            }
            foreach (var (type, names) in other)
                foreach (var (name, count, resolved, props) in names)
                    if (!resolved[0]) warnings.Add("unconverted ref " + type + " " + name + " (" + count[0] + ") not in extract");

            // trigger links (LinkedTriggerID of kept entities -> TriggerID of kept triggers)
            var trigIds = new HashSet<string>(StringComparer.Ordinal);
            foreach (var rec in entities)
                if (rec.type == "TriggerRSData" && rec.cls == "keep") trigIds.Add(KeyOf(rec.e.props.Get("TriggerID")));
            foreach (var rec in entities)
                if (rec.cls == "keep" && Truthy(rec.e.props.Get("LinkedTriggerID")))
                {
                    object lt = rec.e.props.Get("LinkedTriggerID");
                    if (!trigIds.Contains(KeyOf(lt)))
                        warnings.Add("entity " + I(rec.id) + " " + PyStr(rec.name) + " (" + rec.type + "): LinkedTriggerID " + PyRepr(lt) + " has no kept TriggerRSData");
                }

            // misses (one line per missing (type, name), with the first referencing entity / material and the count)
            var missN = new Dictionary<string, int>(StringComparer.Ordinal);
            foreach (var m in cl.misses) { string k = m.type + "\n" + UxCommon.PyLower(PyStr(m.name)); missN[k] = (missN.TryGetValue(k, out int c) ? c : 0) + 1; }
            var seenMiss = new HashSet<string>(StringComparer.Ordinal);
            foreach (var m in cl.misses)
            {
                string k = m.type + "\n" + UxCommon.PyLower(PyStr(m.name));
                if (!seenMiss.Add(k)) continue;
                problems.Add("closure miss: " + m.type + " " + PyRepr(m.name) + (m.entity != null ? " entity " + I(m.entity.Value) : "") +
                    (!string.IsNullOrEmpty(m.material) ? " material " + m.material : "") + (!string.IsNullOrEmpty(m.why) ? " (" + m.why + ")" : "") + " x" + missN[k]);
            }

            // race poses (gate group, board) for S8
            var racePoses = new JObj();
            foreach (var rec in entities)
            {
                string key = rec.type == "DropGateRSData" ? "DropGate" : rec.type == "ThirtySecondBoardRSData" ? "ThirtySecondBoard" : rec.name is string rn && rn == "TO_StartingGate" ? "TO_StartingGate" : null;
                if (key == null) continue;
                if (racePoses.Get(key) is JObj had)
                {
                    warnings.Add("more than one " + key + " (" + I((long)had.Get("id")) + " and " + I(rec.id) + "); first kept in race_poses");
                    continue;
                }
                racePoses.Set(key, new JObj().Set("id", rec.id).Set("type", rec.type).Set("name", rec.name).Set("matrix", rec.e.Matrix).Set("cls", rec.cls));
            }
            foreach (string key in new[] { "DropGate", "TO_StartingGate", "ThirtySecondBoard" })
                if (!racePoses.Has(key)) warnings.Add("no " + key + " entity in the race merge");

            // (d) counts
            var typeOrder = new List<string>(); var typeIndex = new Dictionary<string, int>(StringComparer.Ordinal);
            var typeCount = new List<long>(); var typeClasses = new List<JObj>(); var typeProps = new List<JObj>();
            foreach (var rec in entities)
            {
                if (!typeIndex.TryGetValue(rec.type, out int ti))
                {
                    ti = typeOrder.Count; typeIndex[rec.type] = ti; typeOrder.Add(rec.type); typeCount.Add(0); typeClasses.Add(new JObj()); typeProps.Add(new JObj());
                }
                typeCount[ti]++;
                typeClasses[ti].Set(rec.cls, (long)(typeClasses[ti].Get(rec.cls) ?? 0L) + 1);
                foreach (var kv in rec.e.prop_types.items) if (!typeProps[ti].Has(kv.Key)) typeProps[ti].Set(kv.Key, kv.Value);
            }
            var typesJson = new JObj(); var byType = new JObj(); var typeCounts = new JObj();
            for (int ti = 0; ti < typeOrder.Count; ti++)
            {
                var pt = new List<object>(); foreach (var kv in typeProps[ti].items) pt.Add(new List<object> { kv.Key, kv.Value });
                typesJson.items.Add(new KeyValuePair<string, object>(typeOrder[ti], new JObj().Set("count", typeCount[ti]).Set("classes", typeClasses[ti]).Set("prop_types", pt)));
                byType.items.Add(new KeyValuePair<string, object>(typeOrder[ti], typeClasses[ti]));
                typeCounts.items.Add(new KeyValuePair<string, object>(typeOrder[ti], typeCount[ti]));
            }
            var byClass = new JObj(); foreach (string c in CLASSES) byClass.Set(c, 0L);
            long keptWithCollision = 0;
            foreach (var rec in entities)
            {
                byClass.Set(rec.cls, (long)(byClass.Get(rec.cls) ?? 0L) + 1);
                if ((rec.cls == "keep" || rec.cls == "demote") && rec.collision) keptWithCollision++;
            }
            var closureCounts = new JObj(); var closureBytes = new JObj();
            foreach (string t in CLOSURE_TYPES)
            {
                long bytes = 0; foreach (var r in cl.lists[t]) bytes += r.size;
                closureCounts.Set(t, (long)cl.lists[t].Count); closureBytes.Set(t, bytes);
            }
            long unconvertedEntities = 0; var unconvertedJson = new JObj();
            foreach (var (name, f, ents, types) in unconverted)
            {
                unconvertedEntities += ents.Count;
                unconvertedJson.Set(name, new JObj().Set("skinned", f.Get("skinned")).Set("boned", f.Get("boned")).Set("nodes", f.Get("nodes")).Set("entities", SortedLongs(ents)).Set("types", SortedStrings(types)));
            }
            var counts = new JObj().Set("entities", (long)entities.Count).Set("by_class", byClass).Set("by_type", byType).Set("type_counts", typeCounts).Set("closure", closureCounts)
                .Set("kept_with_collision", keptWithCollision).Set("unconverted_entities", unconvertedEntities).Set("closure_bytes", closureBytes);

            // the document
            var opt = run.opt; var md5 = opt.toolMd5 ?? ReferenceToolMd5;
            string Tool(string name) => md5.TryGetValue(name, out string v) ? v : null;
            var source = new JObj().Set("scene", opt.scenePath ?? (run.disc.directory.Length == 0 ? "" : run.disc.directory + "/") + UxCommon.DiscFileName(lv, "scene"))
                .Set("scene_md5", run.sceneMd5).Set("extract_index", opt.extractIndexPath ?? "extract/" + disc + "/index.json").Set("extract_index_md5", run.indexMd5)
                .Set("tool", TOOL).Set("tool_md5", Tool(TOOL));
            if (rigid != "drop") source.Set("uxmesh_md5", Tool("uxmesh.py"));
            var readers = new JObj(); foreach (string n in READERS) readers.Set(n, Tool(n));
            source.Set("readers", readers);

            var all = new List<object>(); foreach (var s in run.subs) all.Add(new List<object> { s.name, s.nent });
            var ignored = new List<object>(); foreach (var kv in run.byShort) if (kv.Key != "Base" && kv.Key != run.champ) ignored.Add(kv.Key);
            var policy = new JObj();
            foreach (var p in POLICY) policy.Set(p.key, new JObj().Set("cls", p.cls).Set("reflex", p.reflex).Set("note", p.note).Set("design", p.design));
            var nameRules = new JObj();
            foreach (var r in NAME_RULES)
            {
                if (!(nameRules.Get(r.name) is JObj d)) { d = new JObj(); nameRules.Set(r.name, d); }
                d.Set(r.kind, new List<object> { r.cls, r.reflex, r.why });
            }
            var kindOverrides = new JObj(); foreach (string k in KINDS) kindOverrides.Set(k, new JObj());
            foreach (var r in KIND_OVERRIDES) ((JObj)kindOverrides.Get(r.kind)).Set(r.name, new List<object> { r.cls, r.reflex, r.why });

            var entList = new List<object>(entities.Count); long idMin = long.MaxValue, idMax = long.MinValue;
            foreach (var rec in entities)
            {
                var j = new JObj().Set("id", rec.id).Set("type", rec.type).Set("name", rec.name).Set("subscene", rec.sub).Set("present", run.present[rec.id])
                    .Set("matrix", rec.e.Matrix).Set("props", rec.e.props).Set("refs", rec.refs).Set("cls", rec.cls).Set("reflex", rec.reflex).Set("reason", rec.reason)
                    .Set("rule", rec.rule).Set("collision", rec.collision);
                if (rec.model != null) j.Set("model", rec.model);
                if (rec.policy_cls != null) j.Set("policy_cls", rec.policy_cls);
                if (rec.sky_dome != null) j.Set("sky_dome", rec.sky_dome);
                if (rec.underworld != null) j.Set("underworld", rec.underworld);
                entList.Add(j);
                if (rec.id < idMin) idMin = rec.id; if (rec.id > idMax) idMax = rec.id;
            }

            string tdir = kind == "sx" ? opt.templateDirSx : opt.templateDirFr;
            var doc = new JObj().Set("schema", SCHEMA).Set("level", disc).Set("short", lv.shortName).Set("mode", lv.mode).Set("kind", kind).Set("display", lv.display)
                .Set("freeride_display", lv.freeride)
                .Set("template", new JObj().Set("kind", kind).Set("dir", tdir ?? DefaultTemplateDir(kind)).Set("slot", UxCommon.SlotName(kind)))
                .Set("y_lift", UxCommon.YLift(lv))
                .Set("source", source)
                .Set("subscenes", new JObj().Set("all", all).Set("used", new List<object> { "Base", run.champ }).Set("ignored", ignored).Set("championship", run.champ))
                .Set("policy", policy).Set("name_rules", nameRules).Set("kind_overrides", kindOverrides)
                .Set("entities", entList)
                .Set("types", typesJson)
                .Set("freeride_start", run.frStart)
                .Set("race_poses", racePoses)
                .Set("terrain", terrain)
                .Set("closure", cl.ToJson())
                .Set("unconverted_models", unconvertedJson)
                .Set("other_refs", otherJson)
                .Set("counts", counts)
                .Set("id_range", entities.Count > 0 ? new List<object> { idMin, idMax } : null)
                .Set("problems", problems)
                .Set("warnings", warnings);
            if (rigid != "drop")
            {
                doc.Set("options", new JObj().Set("rigid_models", rigid));
                var names = new HashSet<string>(StringComparer.Ordinal);
                foreach (var rec in entities) if (rec.model != null && rec.model.Get("rigid_static") is bool b && b) names.Add(PyStr(rec.model.Get("name")));
                doc.Set("rigid_static_models", SortedStrings(names));
            }
            return (JObj)run.cleaner.Clean(doc);
        }

        // ------------------------------------------------------------------------------------------- entry points
        /// The stage: reads ctx.disc, writes level.json / level_fr.json into ctx.work (the stock library is not needed).
        public static UxLevelResult Convert(UxContext ctx, UxLevelOptions options = null, Action<float, string> progress = null)
            => Convert(ctx.disc, ctx.work, options, progress, ctx.cancel, ctx.log);

        /// The stage on its inputs: the disc level, the work store, the options, a progress callback (fraction 0..1, what is being
        /// done; called from worker threads during the asset pass, one call at a time), the cancel token and a log sink. Writes
        /// work/<Level>/level.json (kind sx) and level_fr.json (kind fr). Throws UxException when a kind has problems
        /// (options.failOnProblems), as the builder does.
        public static UxLevelResult Convert(UxDiscLevel disc, UxWork work, UxLevelOptions options = null, Action<float, string> progress = null,
            CancellationToken cancel = default, Action<string> log = null)
        {
            var opt = options ?? new UxLevelOptions();
            UxBin.Require(disc != null && work != null, "ux_level: the disc level and the work store are needed");
            UxBin.Require(opt.kinds != null && opt.kinds.Length > 0, "ux_level: no kind to build");
            foreach (string k in opt.kinds) UxBin.Require(k == "sx" || k == "fr", "kind must be sx or fr");
            UxBin.Require(Array.IndexOf(RIGID_CHOICES, opt.rigidModels) >= 0, "rigid_models must be one of ('static', 'drop')");
            var clock = System.Diagnostics.Stopwatch.StartNew();
            var result = new UxLevelResult();
            progress?.Invoke(0f, "reading the scene");
            var run = OpenScene(disc, opt, cancel);
            result.secondsScene = clock.Elapsed.TotalSeconds;
            progress?.Invoke(0.05f, "reading the level's assets");
            ReadAssets(run, progress);
            result.secondsAssets = clock.Elapsed.TotalSeconds - result.secondsScene;
            result.sceneMd5 = run.sceneMd5; result.extractIndexMd5 = run.indexMd5;
            for (int i = 0; i < opt.kinds.Length; i++)
            {
                cancel.ThrowIfCancellationRequested();
                string kind = opt.kinds[i];
                progress?.Invoke(0.85f + 0.15f * i / opt.kinds.Length, "classifying the entities (" + kind + ")");
                double t0 = clock.Elapsed.TotalSeconds;
                var d = BuildLevel(run, kind);
                result.secondsBuild += clock.Elapsed.TotalSeconds - t0;
                var counts = (JObj)d.Get("counts");
                var s = new UxLevelSummary { kind = kind, entities = (int)(long)counts.Get("entities"), by_class = (JObj)counts.Get("by_class"), closure = (JObj)counts.Get("closure"),
                    warnings = ((List<object>)d.Get("warnings")).Count };
                foreach (object p in (List<object>)d.Get("problems")) s.problems.Add((string)p);
                result.kinds.Add(s);
                if (s.problems.Count > 0 && opt.failOnProblems)
                    throw new UxException("ux_level " + kind + ": " + PyRepr(s.problems.GetRange(0, Math.Min(5, s.problems.Count))));
                t0 = clock.Elapsed.TotalSeconds;
                work.WriteText(OutPath(kind), DumpsDocument(d, run.cleaner.repr));       // U.write_json(out_path(level, kind), d)
                result.secondsWrite += clock.Elapsed.TotalSeconds - t0;
                s.file = OutPath(kind);
                log?.Invoke("[ux_level] " + disc.name + " " + kind + ": " + s.entities + " entities " + UxJson.Dumps(s.by_class) + " closure " + UxJson.Dumps(s.closure) + " -> " + s.file);
            }
            result.seconds = clock.Elapsed.TotalSeconds;
            progress?.Invoke(1f, "level model written");
            return result;
        }

        // ------------------------------------------------------------------------------------------- test access
        /// The numbers behind the sky dome and underworld rules and the free-ride yaw, unrounded (compared
        /// bit for bit with the reference's sky_dome_facts / placed_top_y / terrain_floor): {floor_y, yaw_deg, entities:
        /// [[id, rad, el95, top, el95 with Math.Atan2]]} for every merged ModelRSData the policy keeps and whose surface is in the level.
        /// The fifth column is el95 with the runtime's Math.Atan2 in place of UxLevel.Atan2 (on the reference's machine that is the C
        /// library Python and numpy call: it shows that only arctan2's last bit can differ).
        internal static JObj Numerics(UxDiscLevel disc, CancellationToken cancel = default)
        {
            var run = OpenScene(disc, new UxLevelOptions(), cancel);
            ReadAssets(run, null);
            var rows = new List<(int index, IxAsset surface)>();
            for (int i = 0; i < run.merged.Count; i++)
            {
                var e = run.merged[i].entity;
                if (e.type != "ModelRSData" || Classify(e.type, e.NameValue, "sx").cls != "keep" || e.Matrix == null) continue;
                var a = e.props.Get("SurfaceAsset") is JObj sa ? run.ix.Get(sa.Get("asset"), "surface") : null;
                if (a != null) rows.Add((i, a));
            }
            var placed = new Placed[rows.Count]; var library = new Placed[rows.Count];
            Parallel.For(0, rows.Count, run.po, i =>
            {
                placed[i] = run.PlacedOf(rows[i].index, rows[i].surface, true);
                library[i] = run.PlacedOf(rows[i].index, rows[i].surface, true, true);
            });
            var list = new List<object>();
            for (int i = 0; i < rows.Count; i++) list.Add(new List<object> { run.merged[rows[i].index].entity.id, placed[i].rad, placed[i].el95, placed[i].top, library[i].el95 });
            return new JObj().Set("floor_y", run.TerrainFloor()).Set("yaw_deg", run.frStart?.Get("yaw_deg")).Set("entities", list);
        }
    }
}
